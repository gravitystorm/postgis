/**********************************************************************
 * $Id$
 *
 * PostGIS - Spatial Types for PostgreSQL
 * http://postgis.refractions.net
 * Copyright 2001-2003 Refractions Research Inc.
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of hte GNU General Public Licence. See the COPYING file.
 * 
 **********************************************************************
 * $Log$
 * Revision 1.24  2003/11/16 00:27:46  strk
 * Huge code re-organization. More structured code, more errors handled,
 * cursor based iteration, less code lines.
 *
 * Revision 1.23  2003/11/14 22:04:51  strk
 * Used environment vars to pass libpq connection options (less error prone,
 * easier to read). Printed clearer error message on query error.
 *
 * Revision 1.22  2003/09/10 22:44:56  jeffloun
 * got rid of warning...
 *
 * Revision 1.21  2003/09/10 22:40:11  jeffloun
 * changed it to make the field names in the dbf file capital letters
 *
 * Revision 1.20  2003/09/10 21:36:04  jeffloun
 * fixed a bug in is_clockwise...
 *
 * Revision 1.19  2003/07/01 18:30:55  pramsey
 * Added CVS revision headers.
 *
 * Revision 1.18  2003/02/04 21:39:20  pramsey
 * Added CVS substitution strings for logging.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "libpq-fe.h"
#include "shapefil.h"
#include "getopt.h"

#define	POINTTYPE	1
#define	LINETYPE	2
#define	POLYGONTYPE	3
#define	MULTIPOINTTYPE	4
#define	MULTILINETYPE	5
#define	MULTIPOLYGONTYPE	6
#define	COLLECTIONTYPE	7
#define	BBOXONLYTYPE	99

/* Global data */
PGconn *conn;
char *geo_col_name, *table;
int type_ary[256];
int geo_col_num; // attribute num for the requested or detected geometry 
DBFHandle dbf;
SHPHandle shp;
int geotype;
int is3d;
int (*shape_creator)(char *, int, SHPHandle, int);

/* Prototypes */
char *getTableOID(char *table);
int addRecord(PGresult *res, int row);
int initShapefile(char *shp_file, PGresult *res);
int getGeometryOID(PGconn *conn);
int getGeometryType(char *table, char *geo_col_name);
int         create_lines(char *str,int shape_id, SHPHandle shp,int dims);
int    create_multilines(char *str,int shape_id, SHPHandle shp,int dims);
int        create_points(char *str,int shape_id, SHPHandle shp,int dims);
int   create_multipoints(char *str,int shape_id, SHPHandle shp,int dims);
int      create_polygons(char *str,int shape_id, SHPHandle shp,int dims);
int create_multipolygons(char *str,int shape_id, SHPHandle shp,int dims);
int parse_points(char *str, int num_points, double *x,double *y,double *z);
int num_points(char *str);
int num_lines(char *str);
char *scan_to_same_level(char *str);
int points_per_sublist( char *str, int *npoints, long max_lists);
int reverse_points(int num_points,double *x,double *y,double *z);
int is_clockwise(int num_points,double *x,double *y,double *z);

static void exit_nicely(PGconn *conn){
	PQfinish(conn);
	exit(1);
}

//main
//USAGE: pgsql2shp [<options>] <database> <table>
//OPTIONS:
//  -d Set the dump file to 3 dimensions, if this option is not used
//     all dumping will be 2d only.
//  -f <filename>  Use this option to specify the name of the file
//     to create.
//  -h <host>  Allows you to specify connection to a database on a
//     machine other than the localhost.
//  -p <port>  Allows you to specify a database port other than 5432.
//  -P <password>  Connect to the database with the specified password.
//  -u <user>  Connect to the database as the specified user.
//  -g <geometry_column> Specify the geometry column to be exported.

int main(int ARGC, char **ARGV){
	char *dbName, *query, *shp_file;
	int c, errflg, curindex;
	int row;
	PGresult *res;

	dbf=NULL;
	shp=NULL;
	geotype=-1;
	shape_creator = NULL;
	table = NULL;
	geo_col_name = NULL;
	dbName = NULL;
	shp_file = NULL;
	geo_col_num = -1;
	is3d = 0;
	errflg = 0;

	/* Parse command line */
        while ((c = getopt(ARGC, ARGV, "f:h:du:p:P:g:")) != EOF){
               switch (c) {
               case 'f':
                    shp_file = optarg;
                    break;
               case 'h':
        	    setenv("PGHOST", optarg, 1);
                    break;
               case 'd':
	            is3d = 1;
                    break;		  
               case 'u':
		    setenv("PGUSER", optarg, 1);
                    break;
               case 'p':
		    setenv("PGPORT", optarg, 1);
                    break;
	       case 'P':
		    setenv("PGPASSWORD", optarg, 1);
		    break;
	       case 'g':
		    geo_col_name = optarg;
		    break;
               case '?':
                    errflg=1;
               }
        }

        curindex=0;
        for ( ; optind < ARGC; optind++){
                if(curindex ==0){
                        dbName = ARGV[optind];
			setenv("PGDATABASE", dbName, 1);
                }else if(curindex == 1){
                        table = ARGV[optind];
                }
                curindex++;
        }
        if(curindex != 2){
                errflg = 1;
        }

        if (errflg==1) {
                printf("\n**ERROR** invalid option or command parameters\n");
                printf("\n");
                printf("USAGE: pgsql2shp [<options>] <database> <table>\n");
                printf("\n");
                printf("OPTIONS:\n");
                printf("  -d Set the dump file to 3 dimensions, if this option is not used\n");
                printf("     all dumping will be 2d only.\n");
                printf("  -f <filename>  Use this option to specify the name of the file\n");
                printf("     to create.\n");
                printf("  -h <host>  Allows you to specify connection to a database on a\n");
                printf("     machine other than the localhost.\n");
                printf("  -p <port>  Allows you to specify a database port other than 5432.\n");
                printf("  -P <password>  Connect to the database with the specified password.\n");
                printf("  -u <user>  Connect to the database as the specified user.\n");
		printf("  -g <geometry_column> Specify the geometry column to be exported.\n");
                printf("\n");
                exit (2);
        }

        if(shp_file == NULL){
		shp_file = malloc(strlen(table) + 1);
                strcpy(shp_file,table);
        }

	/* make a connection to the specified database */
	conn = PQconnectdb("");

	/* check to see that the backend connection was successfully made */
	if (PQstatus(conn) == CONNECTION_BAD)
	{
		fprintf(stderr, "Connection to database '%s' failed.\n", dbName);
		fprintf(stderr, "%s", PQerrorMessage(conn));
		exit_nicely(conn);
	}

#ifdef DEBUG
	debug = fopen("/tmp/trace.out", "w");
	PQtrace(conn, debug);
#endif	 /* DEBUG */



//------------------------------------------------------------
//Get the table data in a cursor

	// begin the transaction
	res=PQexec(conn, "BEGIN");
	if ( ! res || PQresultStatus(res) != PGRES_COMMAND_OK ) {
		fprintf(stderr, "%s\n", PQerrorMessage(conn));
		exit_nicely(conn);
	}
	PQclear(res);

	// declare a cursor
	query= (char *)malloc(strlen(table)+256);
	sprintf(query, "DECLARE cur CURSOR FOR SELECT * FROM \"%s\"", table);
	//printf("%s\n",query);
	res = PQexec(conn, query);	
	free(query);
	if ( ! res || PQresultStatus(res) != PGRES_COMMAND_OK ) {
		fprintf(stderr, "%s\n", PQerrorMessage(conn));
		exit_nicely(conn);
	}
	PQclear(res);

//------------------------------------------------------------
// Fetch each result in cursor, initialize stuff on first row
//------------------------------------------------------------

	row=0;
	while(1)
	{
		res = PQexec(conn, "FETCH FROM cur");
		if ( ! res || PQresultStatus(res) != PGRES_TUPLES_OK ) {
			fprintf(stderr, "%s\n", PQerrorMessage(conn));
			exit_nicely(conn);
		}

		// This is our first row.. let's initialize 
		if ( ! row ) {

			if ( ! PQntuples(res) ) { // No tuples ? Give up !
				fprintf(stderr, "No records in table\n");
				exit_nicely(conn);
			}

			printf("Initializing shapefile %s... ", shp_file);
			fflush(stdout);
			if ( ! initShapefile(shp_file, res) )
				exit_nicely(conn);
			printf("done.\n");
			printf("Adding records");
			fflush(stdout);
		}

		/* No more rows, end of loop */
		if ( ! PQntuples(res) ) {
			PQclear(res);
			break;
		}

		/* Add record in all output files */
		if ( ! addRecord(res, row) ) exit_nicely(conn);
		printf("."); fflush(stdout);

		row++;
		PQclear(res);
	}
	printf("[%d rows]\n", (row-1));

	DBFClose(dbf);
	if (shp) SHPClose(shp);
	exit_nicely(conn);


#ifdef DEBUG
	fclose(debug);
#endif	 /* DEBUG */

	return 0;
}





//reads points into x,y,z co-ord arrays
int parse_points(char *str, int num_points, double *x,double *y,double *z){
	int	keep_going;
	int	num_found= 0;
	char	*end_of_double;

	if ( (str == NULL) || (str[0] == 0) ){
		return 0;  //either null string or empty string
	}
	
	//look ahead for the "("
	str = strchr(str,'(') ;
	
	if ( (str == NULL) || (str[1] == 0) ){  // str[0] = '(';
		return 0;  //either didnt find "(" or its at the end of the string
	}
	str++;  //move forward one char
	keep_going = 1;
	while (keep_going == 1){
		
		//attempt to get the point
		//scanf is slow, so we use strtod()

		x[num_found] = (double)strtod(str,&end_of_double);
		if (end_of_double == str){
			return 0; //error occured (nothing parsed)
		}
		str = end_of_double;
		y[num_found] = strtod(str,&end_of_double);
		if (end_of_double == str){
			return 0; //error occured (nothing parsed)
		}
		str = end_of_double;
		z[num_found] = strtod(str,&end_of_double); //will be zero if error occured
		str = end_of_double;
		num_found++;

		str=strpbrk(str,",)");  // look for a "," or ")"
		if (str != NULL && str[0] == ','){
			str++;
		}
		keep_going = (str != NULL) && (str[0] != ')');		
	}
	return num_found; 
}





//returns how many points are in the first list in str
//
//  1. scan ahead looking for "("
//  2. find "," until hit a ")"
//  3. return number of points found
//	
// NOTE: doesnt actually parse the points, so if the 
//       str contains an invalid geometry, this could give
// 	   back the wrong answer.
//
// "(1 2 3, 4 5 6),(7 8, 9 10, 11 12 13)" => 2 (2nd list is not included)
int	num_points(char *str){
	int		keep_going;
	int		points_found = 1; //no "," if only one point (and last point)
						

	if ( (str == NULL) || (str[0] == 0) )
	{
		return 0;  //either null string or empty string
	}

	//look ahead for the "("

	str = strchr(str,'(') ;
	
	if ( (str == NULL) || (str[1] == 0) )  // str[0] = '(';
	{
		return 0;  //either didnt find "(" or its at the end of the string
	}

	keep_going = 1;
	while (keep_going) 
	{
		str=strpbrk(str,",)");  // look for a "," or ")"
		keep_going = (str != NULL);
		if (keep_going)  // found a , or )
		{
			if (str[0] == ')')
			{
				//finished
				return points_found;
			}
			else	//str[0] = ","
			{
				points_found++;
				str++; //move 1 char forward	
			}
		}
	}
	return points_found; // technically it should return an error.
}

//number of sublist in a string.

// Find the number of lines in a Multiline
// OR
// The number of rings in a Polygon
// OR
// The number of polygons in a multipolygon

// ( (..),(..),(..) )  -> 3
// ( ( (..),(..) ), ( (..) )) -> 2
// ( ) -> 0
// scan through the list, for every "(", depth (nesting) increases by 1
//				  for every ")", depth (nesting) decreases by 1
// if find a "(" at depth 1, then there is a sub list
//
// example: 
//      "(((..),(..)),((..)))"
//depth  12333223332112333210
//        +           +         increase here

int num_lines(char *str){
	int	current_depth = 0;
	int	numb_lists = 0;


	while ( (str != NULL) && (str[0] != 0) )
	{
		str=strpbrk(str,"()"); //look for "(" or ")"
		if (str != NULL)
		{
			if (str[0] == '(')
			{
				current_depth++;
				if (current_depth == 2)
					numb_lists ++;
			}
			if (str[0] == ')')
			{
				current_depth--;
				if (current_depth == 0)
					return numb_lists ;
			}
			str++;
		}
	}
	return numb_lists ; // probably should give an error
}



//simple scan-forward to find the next "(" at the same level
//  ( (), (),(), ),(...
//                 + return this location
char *scan_to_same_level(char *str){

	//scan forward in string looking for at "(" at the same level
	// as the one its already pointing at

	int	current_depth = 0;
	int  first_one=1;


	while ( (str != NULL) && (str[0] != 0) )
	{
		str=strpbrk(str,"()");
		if (str != NULL)
		{
			if (str[0] == '(')
			{
				if (!(first_one))
				{
					if (current_depth == 0)
						return str;
				}
				else
					first_one = 0;  //ignore the first opening "("
				current_depth++;
			}
			if (str[0] == ')')
			{
				current_depth--;
			}

			str++;
		}
	}
	return str ; // probably should give an error
}






// Find out how many points are in each sublist, put the result in the array npoints[]
//  (for at most max_list sublists)
//
//  ( (L1),(L2),(L3) )  --> npoints[0] = points in L1,
//				    npoints[1] = points in L2,
//				    npoints[2] = points in L3
//
// We find these by, again, scanning through str looking for "(" and ")"
// to determine the current depth.  We dont actually parse the points.

int points_per_sublist( char *str, int *npoints, long max_lists){
	//scan through, noting depth and ","s

	int	current_depth = 0;
	int	current_list =-1 ;


	while ( (str != NULL) && (str[0] != 0) )
	{
		str=strpbrk(str,"(),");  //find "(" or ")" or ","
		if (str != NULL)
		{
			if (str[0] == '(')
			{
				current_depth++;
				if (current_depth == 2)
				{
					current_list ++;
					if (current_list >=max_lists)
						return 1;			// too many sub lists found
					npoints[current_list] = 1;
				}
				// might want to return an error if depth>2
			}
			if (str[0] == ')')
			{
				current_depth--;
				if (current_depth == 0)
					return 1 ;
			}
			if (str[0] == ',')
			{
				if (current_depth==2)
				{
					npoints[current_list] ++;	
				}
			}

			str++;
		}
	}
	return 1 ; // probably should give an error
}



int create_multilines(char *str,int shape_id, SHPHandle shp,int dims){
	int		lines,i,j,max_points,index;
	int		*points;
	int		*part_index;
	int 		notnull;
	
	double	*x;
	double	*y;
	double	*z;

	double	*totx;
	double	*toty;
	double	*totz;
	SHPObject *obj;

	notnull=1;
	lines = num_lines(str);
	
	points = (int *)malloc(sizeof(int) * lines);

	if(points_per_sublist(str, points, lines) ==0){
		printf("error - points_per_sublist failed");
	}
	max_points = 0;
	for(j=0;j<lines;j++){
		max_points += points[j];
	}
		
	part_index = (int *)malloc(sizeof(int) * lines);
	totx = (double *)malloc(sizeof(double) * max_points);
	toty = (double *)malloc(sizeof(double) * max_points);
	totz = (double *)malloc(sizeof(double) * max_points);
	
	index=0;

	if(lines == 0 ){
		notnull = 0;
	}
	for(i=0;i<lines;i++){
		str = strchr(str,'(') ;

		
		if(str[0] =='(' && str[1] == '('){
			str++;
		}
		
		x = (double *)malloc(sizeof(double) * points[i]);
		y = (double *)malloc(sizeof(double) * points[i]);
		z = (double *)malloc(sizeof(double) * points[i]);

		notnull = parse_points(str,points[i],x,y,z);
		str = scan_to_same_level(str);
		part_index[i] = index;
		for(j=0;j<points[i];j++){
			totx[index] = x[j];
			toty[index] = y[j];
			totz[index] = z[j];
			index++;
		}
		free(x);
		free(y);
		free(z);
	}

	
	obj = (SHPObject *)malloc(sizeof(SHPObject));
	if(dims == 0){
		if(notnull > 0){
			obj = SHPCreateObject(SHPT_ARC,shape_id,lines,part_index,NULL,max_points,totx,toty,totz,NULL);
		}else{
			obj = SHPCreateObject(SHPT_NULL,shape_id,lines,part_index,NULL,max_points,totx,toty,totz,NULL);
		}
	}else{
		if(notnull > 0){
			obj = SHPCreateObject(SHPT_ARCZ,shape_id,lines,part_index,NULL,max_points,totx,toty,totz,NULL);
		}else{
			obj = SHPCreateObject(SHPT_NULL,shape_id,lines,part_index,NULL,max_points,totx,toty,totz,NULL);
		}
	}
	free(part_index);
	free(points);
	free(totx);
	free(toty);
	free(totz);
	if(SHPWriteObject(shp,-1,obj)!= -1){
		SHPDestroyObject(obj);
		return 1;
	}else{
		SHPDestroyObject(obj);
		return 0;
	}
}


int create_lines(char *str,int shape_id, SHPHandle shp,int dims){
	int		points;
	int		*part_index;
	int 		notnull;
	
	double	*x,
			*y,
			*z;
	SHPObject	*obj;

	notnull =1;
	part_index = (int *)malloc(sizeof(int));	//we know lines only have 1 part so make the array of size 1
	part_index[0] = 0;

	points = num_points(str);
	x = (double *)malloc(sizeof(double) * points);
	y = (double *)malloc(sizeof(double) * points);
	z = (double *)malloc(sizeof(double) * points);

	notnull = parse_points(str,points,x,y,z);

	obj = (SHPObject *)malloc(sizeof(SHPObject));
	
	if(dims == 0){
		if(notnull > 0){
			obj = SHPCreateObject(SHPT_ARC,shape_id,1,part_index,NULL,points,x,y,z,NULL);
		}else{
			obj = SHPCreateObject(SHPT_NULL,shape_id,1,part_index,NULL,points,x,y,z,NULL);
		}
	}else{
		if(notnull > 0){
			obj = SHPCreateObject(SHPT_ARCZ,shape_id,1,part_index,NULL,points,x,y,z,NULL);
		}else{
			obj = SHPCreateObject(SHPT_NULL,shape_id,1,part_index,NULL,points,x,y,z,NULL);
		}
	}
	free(part_index);
	free(x);
	free(y);
	free(z);
	if(SHPWriteObject(shp,-1,obj)!= -1){
		SHPDestroyObject(obj);
		return 1;
	}else{
		SHPDestroyObject(obj);
		return 0;
	}
}



int create_points(char *str, int shape_id, SHPHandle shp, int dims){
	
	double	*x,
			*y,
			*z;
	SHPObject	*obj;
	int notnull;
	notnull = 1;
	
	x = (double *)malloc(sizeof(double));
	y = (double *)malloc(sizeof(double));
	z = (double *)malloc(sizeof(double));

	notnull = parse_points(str,1,x,y,z);
	

	obj = (SHPObject *)malloc(sizeof(SHPObject));
	
	if(dims == 0){
		if(notnull > 0){
			obj = SHPCreateSimpleObject(SHPT_POINT,1,x,y,z);
		}else{
			obj = SHPCreateSimpleObject(SHPT_NULL,1,x,y,z);
		}
	}else{
		if(notnull > 0){
			obj = SHPCreateSimpleObject(SHPT_POINTZ,1,x,y,z);
		}else{
			obj = SHPCreateSimpleObject(SHPT_NULL,1,x,y,z);
		}
	}
	free(x);
	free(y);
	free(z);
	if(SHPWriteObject(shp,-1,obj)!= -1){
		SHPDestroyObject(obj);
		return 1;
	}else{
		SHPDestroyObject(obj);
		return 0;
	}
}



int create_multipoints(char *str,int shape_id, SHPHandle shp,int dims){
	int points;	
	
	double	*x,
			*y,
			*z;
	SHPObject	*obj;
	int notnull;
	notnull=1;	

	points = num_points(str);
	x = (double *)malloc(sizeof(double)*points);
	y = (double *)malloc(sizeof(double)*points);
	z = (double *)malloc(sizeof(double)*points);

	notnull = parse_points(str,points,x,y,z);

	obj = (SHPObject *)malloc(sizeof(SHPObject));
	
	if(dims == 0){
		if(notnull > 0){
			obj = SHPCreateSimpleObject(SHPT_MULTIPOINT ,points,x,y,z);
		}else{
			obj = SHPCreateSimpleObject(SHPT_NULL ,points,x,y,z);
		}
	}else{
		if(notnull > 0){
			obj = SHPCreateSimpleObject(SHPT_MULTIPOINTZ ,points,x,y,z);
		}else{
			obj = SHPCreateSimpleObject(SHPT_NULL ,points,x,y,z);
		}
	}

	free(x);
	free(y);
	free(z);
	if(SHPWriteObject(shp,-1,obj)!= -1){
		SHPDestroyObject(obj);
		return 1;
	}else{
		SHPDestroyObject(obj);
		return 0;
	}
}




int create_polygons(char *str,int shape_id, SHPHandle shp,int dims){
	int		rings,i,j,max_points,index;
	int		*points;
	int		*part_index;
	int notnull;
	
	double	*x;
	double	*y;
	double	*z;

	double	*totx;
	double	*toty;
	double	*totz;
	SHPObject *obj;

	notnull = 1;

	rings = num_lines(str); //the number of rings in the polygon
	points = (int *)malloc(sizeof(int) * rings);

	if(points_per_sublist(str, points, rings) ==0){
		printf("error - points_per_sublist failed");
	}
	max_points = 0;
	for(j=0;j<rings;j++){
		max_points += points[j];
	}
		
	part_index = (int *)malloc(sizeof(int) * rings);
	totx = (double *)malloc(sizeof(double) * max_points);
	toty = (double *)malloc(sizeof(double) * max_points);
	totz = (double *)malloc(sizeof(double) * max_points);
	
	index=0;
	if (rings == 0){
		notnull = 0;
	}
	for(i=0;i<rings;i++){
		str = strchr(str,'(') ;

		
		if(str[0] =='(' && str[1] == '('){
			str++;
		}
		
		x = (double *)malloc(sizeof(double) * points[i]);
		y = (double *)malloc(sizeof(double) * points[i]);
		z = (double *)malloc(sizeof(double) * points[i]);

		notnull = parse_points(str,points[i],x,y,z);

		str = scan_to_same_level(str);
		part_index[i] = index;

		//if this the first ring it should be clockwise, other rings are holes and should be counter-clockwise
		if(i ==0){
			if(is_clockwise(points[i],x,y,z) == 0){
				reverse_points(points[i],x,y,z);
			}
		}else{
			if(is_clockwise(points[i],x,y,z) == 1){
				reverse_points(points[i],x,y,z);
			}
		}

		for(j=0;j<points[i];j++){
			totx[index] = x[j];
			toty[index] = y[j];
			totz[index] = z[j];
			index++;
		}
//		free(x);
//		free(y);
//		free(z);
	}

	obj = (SHPObject *)malloc(sizeof(SHPObject));
	if(dims == 0){
		if(notnull > 0){
			obj = SHPCreateObject(SHPT_POLYGON,shape_id,rings,part_index,NULL,max_points,totx,toty,totz,NULL);
		}else{
			obj = SHPCreateObject(SHPT_NULL,shape_id,rings,part_index,NULL,max_points,totx,toty,totz,NULL);
		}
	}else{
		if(notnull > 0){
			obj = SHPCreateObject(SHPT_POLYGONZ,shape_id,rings,part_index,NULL,max_points,totx,toty,totz,NULL);
		}else{
			obj = SHPCreateObject(SHPT_NULL,shape_id,rings,part_index,NULL,max_points,totx,toty,totz,NULL);
		}
	}
	free(part_index);
	free(points);
	free(totx);
	free(toty);
	free(totz);
	if(SHPWriteObject(shp,-1,obj)!= -1){
		SHPDestroyObject(obj);
		return 1;
	}else{
		SHPDestroyObject(obj);
		return 0;
	}
}


int create_multipolygons(char *str,int shape_id, SHPHandle shp,int dims){
	int		polys,rings,i,j,k,max_points;
	int		index,indexk,index2part,tot_rings,final_max_points;
	int		*points;
	int		*part_index;
	int		*final_part_index;
	int 		notnull;
	
	char	*temp;
	char	*temp_addr;

	double	*x;
	double	*y;
	double	*z;

	double	*totx;
	double	*toty;
	double	*totz;

	double	*finalx;
	double	*finaly;
	double	*finalz;

	SHPObject *obj;
	
	points=0;
	notnull = 1;

	polys = num_lines(str); //the number of rings in the polygon
	final_max_points =0;

	temp_addr = (char *)malloc(strlen(str) +1 );
	temp = temp_addr; //keep original pointer to free the mem later
	strcpy(temp,str);
	tot_rings=0;
	index2part=0;
	indexk=0;
	

	for(i=0;i<polys;i++){
		temp = strstr(temp,"((") ;
		if(temp[0] =='(' && temp[1] == '(' && temp[2] =='('){
			temp++;
		}
		rings = num_lines(temp);
		points = (int *)malloc(sizeof(int) * rings);
		points_per_sublist(temp, points, rings);
		tot_rings += rings;
		for(j=0;j<rings;j++){
			final_max_points += points[j];
		}
		
		temp+= 2;
	}
	free(points);
	temp= temp_addr;
	
	final_part_index = (int *)malloc(sizeof(int) * tot_rings);	
	finalx = (double *)malloc(sizeof(double) * final_max_points); 
	finaly = (double *)malloc(sizeof(double) * final_max_points); 
	finalz = (double *)malloc(sizeof(double) * final_max_points); 

	if(polys == 0){
		notnull = 0;
	}

	for(k=0;k<polys ; k++){ //for each polygon  

		str = strstr(str,"((") ;
		if(strlen(str) >2 && str[0] =='(' && str[1] == '(' && str[2] =='('){
			str++;
		}

		rings = num_lines(str);
		points = (int *)malloc(sizeof(int) * rings);

		if(points_per_sublist(str, points, rings) ==0){
			printf("error - points_per_sublist failed");
		}
		max_points = 0;
		for(j=0;j<rings;j++){
			max_points += points[j];
		}
		
		part_index = (int *)malloc(sizeof(int) * rings);
		totx = (double *)malloc(sizeof(double) * max_points);
		toty = (double *)malloc(sizeof(double) * max_points);
		totz = (double *)malloc(sizeof(double) * max_points);
	
		index=0;
		for(i=0;i<rings;i++){
			str = strchr(str,'(') ;
			if(str[0] =='(' && str[1] == '('){
				str++;
			}
			
			x = (double *)malloc(sizeof(double) * points[i]);
			y = (double *)malloc(sizeof(double) * points[i]);
			z = (double *)malloc(sizeof(double) * points[i]);

			notnull = parse_points(str,points[i],x,y,z);
			str = scan_to_same_level(str);
			part_index[i] = index;

			//if this the first ring it should be clockwise, other rings are holes and should be counter-clockwise
	                if(i ==0){
        	                if(is_clockwise(points[i],x,y,z) == 0){
                	                reverse_points(points[i],x,y,z);
                        	}
	                }else{
        	                if(is_clockwise(points[i],x,y,z) == 1){
                	                reverse_points(points[i],x,y,z);
	                        }
        	        }

			for(j=0;j<points[i];j++){
				totx[index] = x[j];
				toty[index] = y[j];
				totz[index] = z[j];
				index++;
			}
			free(x);
			free(y);
			free(z);
		}
		for(j=0;j<i;j++){
			final_part_index[index2part] = part_index[j]+indexk ;
			index2part++;
		}

		for(j=0;j<index;j++){
			finalx[indexk] = totx[j];
			finaly[indexk] = toty[j];
			finalz[indexk] = totz[j];
			indexk++;
		}
		
		free(points);
		free(part_index);
		free(totx);
		free(toty);
		free(totz);
		str -= 1;
	}//end for (k < polys... loop


	obj	= (SHPObject *)malloc(sizeof(SHPObject));
	if(dims == 0){
		if(notnull > 0){
			obj = SHPCreateObject(SHPT_POLYGON,shape_id,tot_rings,final_part_index,NULL,final_max_points,finalx,finaly,finalz,NULL);
		}else{
			obj = SHPCreateObject(SHPT_NULL,shape_id,tot_rings,final_part_index,NULL,final_max_points,finalx,finaly,finalz,NULL);
		}
	}else{
		if(notnull > 0){		
			obj = SHPCreateObject(SHPT_POLYGONZ,shape_id,tot_rings,final_part_index,NULL,final_max_points,finalx,finaly,finalz,NULL);
		}else{
			obj = SHPCreateObject(SHPT_NULL,shape_id,tot_rings,final_part_index,NULL,final_max_points,finalx,finaly,finalz,NULL);
		}
	}
	
	free(final_part_index);
	free(finalx);
	free(finaly);
	free(finalz);
	if(SHPWriteObject(shp,-1,obj)!= -1){
		SHPDestroyObject(obj);
		return 1;
	}else{
		SHPDestroyObject(obj);
		return 0;
	}
	
}



//Reverse the clockwise-ness of the point list...
int reverse_points(int num_points,double *x,double *y,double *z){
	
	int i,j;
	double temp;
	j = num_points -1;
	for(i=0; i <num_points; i++){
		if(j <= i){
			break;
		}
		temp = x[j];
		x[j] = x[i];
		x[i] = temp;

		temp = y[j];
		y[j] = y[i];
		y[i] = temp;

		temp = z[j];
		z[j] = z[i];
		z[i] = temp;

		j--;
	}
	return 1;
}

//return 1 if the points are in clockwise order
int is_clockwise(int  num_points,double *x,double *y,double *z){
	int i;
	double x_change,y_change,area;
	double *x_new, *y_new; //the points, translated to the origin for safer accuracy

	x_new = (double *)malloc(sizeof(double) * num_points);	
	y_new = (double *)malloc(sizeof(double) * num_points);	
	area=0.0;
	x_change = x[0];
	y_change = y[0];

	for(i=0; i < num_points ; i++){
		x_new[i] = x[i] - x_change;
		y_new[i] = y[i] - y_change;
	}

	for(i=0; i < num_points - 1; i++){
		area += (x[i] * y[i+1]) - (y[i] * x[i+1]); //calculate the area	
	}
	if(area > 0 ){
		return 0; //counter-clockwise
	}else{
		return 1; //clockwise
	}
}

/*
 * Returns OID integer on success
 * Returns -1 on error.
 */
int
getGeometryOID(PGconn *conn)
{
	PGresult *res1;
	char *temp_int;
	int OID;

	res1=PQexec(conn, "select OID from pg_type where typname = 'geometry'");	
	if ( ! res1 || PQresultStatus(res1) != PGRES_TUPLES_OK )
	{
		fprintf(stderr, "OID query error: %s\n",
				PQerrorMessage(conn));
		return -1;
	}

	if(PQntuples(res1) <= 0 )
	{
		fprintf(stderr, "Geometry type unknown "
				"(have you enabled postgis?)\n");
		return -1;
	}

	temp_int = (char *)PQgetvalue(res1, 0, 0);
	OID = atoi(temp_int);
	PQclear(res1);
	return OID;
}


/*
 * Initialize the Shapefile (dbf,shp,shx) using
 * information from given postgresql result.
 *
 * Return 1 on success.
 * Return 0 on failure.
 */
int
initShapefile(char *shp_file, PGresult *res)
{
	PGresult *res2;
	int flds, nFields, i, type, size, z, j;
	char field_name[32];
	char *query1;
	int geometry_oid;
	
	geometry_oid = getGeometryOID(conn);
	if ( geometry_oid == -1 ) return 0;
	
	/* Create the dbf file */
	dbf = DBFCreate(shp_file);
	if(dbf == NULL){
		fprintf(stderr, "DBF could not be created - Dump FAILED.\n");
		return 0;
	}

	// add the fields to the DBF
	nFields = PQnfields(res);

	flds =0; //keep track of how many fields you have actually created
	for (i = 0; i < nFields; i++){
		if(strlen(PQfname(res, i)) <32){
			strcpy(field_name, PQfname(res, i));
		}else{
			printf("field name %s is too long, must be less than 32 characters.\n",PQfname(res, i));
			return 0;
		}
		for(z=0; z < strlen(field_name); z++){
			field_name[z] = toupper(field_name[z]);
		}

		/* 
		 * make sure the fields all have unique names,
		 * 10-digit limit on dbf names...
		 */
		for(j=0;j<i;j++){
			if(strncmp(field_name, PQfname(res, j),10) == 0){
				printf("\nWarning: Field '%s' has first 10 characters which duplicate a previous field name's.\nRenaming it to be: '",field_name);
				strncpy(field_name,field_name,9);
				field_name[9] = 0;
				sprintf(field_name,"%s%d",field_name,i);
				printf("%s'\n\n",field_name);
			}
		}

		type = PQftype(res,i);
		size = PQfsize(res,i);
		if(type == 1082){ //date field, which we store as a string so we need more width in the column
			size = 10;
		}
		if(size==-1 && type != geometry_oid){ //-1 represents variable size in postgres, this should not occur, but use 32 bytes in case it does
			
			//( this is ugly: don't forget counting the length 
			// when changing the fixed query strings )
			query1 = (char *)malloc(strlen(PQfname(res, i))+strlen(table)+40); 
			sprintf(query1, "select max(octet_length(\"%s\")) from \"%s\"",
								 PQfname(res, i), table);
			res2 = PQexec(conn, query1);			
			free(query1);
			if ( ! res2 || PQresultStatus(res2) != PGRES_TUPLES_OK ) {
				fprintf(stderr, "%s\n", PQerrorMessage(conn));
				return 0;
			}

			if(PQntuples(res2) > 0 ){
				char *temp_int = (char *)PQgetvalue(res2, 0, 0);
				size = atoi(temp_int);
			}else{
				size = 32;
			}

		}
		if(type == 20 || type == 21 || type == 22 || type == 23){
			if(DBFAddField(dbf, field_name,FTInteger,16,0) == -1)printf("error - Field could not be created.\n");
			type_ary[i]=1;
			flds++;
		}else if(type == 700 || type == 701){
			if(DBFAddField(dbf, field_name,FTDouble,32,10) == -1)printf("error - Field could not be created.\n");
			type_ary[i]=2;
			flds++;
		}
		
		// This is a geometry column
		//
		else if(type == geometry_oid)
		{
			type_ary[i]=9; //the geometry type field			

			// a geometry attribute name as not been specified
			if ( ! geo_col_name )
			{
				geo_col_name = PQfname(res, i);
				geo_col_num = i;
				flds++;
			}
			else if (!strcasecmp(geo_col_name,field_name))
			{
				geo_col_num = i;
				flds++;
			}
		}
		
		else{
			if(DBFAddField(dbf, field_name,FTString,size,0) == -1)printf("error - Field could not be created.\n");
			type_ary[i]=3;
			flds++;
		}
	}

	/* No geometry attribute has been found */
	if ( geo_col_num == -1 )
	{
		/* If a geometry column was specified this is an error */
		if ( geo_col_name )
		{
			fprintf(stderr, "%s: no such attribute in table %s\n",
					geo_col_name, table);
			return 0;
		}

		/* Otherwise we'll just forget about .shp and .shx files */
		fprintf(stderr, "No geometry column found.\n");
		fprintf(stderr, "The DBF file will be created "
				"but not the shx or shp files.\n");
		return 1;
	}

	/*
	 * We now create the appropriate shape (shp) file.
	 */

	geotype = getGeometryType(table, geo_col_name);
	if ( geotype == -1 ) return 0;

	switch (geotype)
	{
		case MULTILINETYPE:
			if ( is3d ) shp = SHPCreate(shp_file, SHPT_ARCZ);
			else shp = SHPCreate(shp_file, SHPT_ARC);
			shape_creator = create_multilines;
			break;
			
		case LINETYPE:
			if ( is3d ) shp = SHPCreate(shp_file, SHPT_ARCZ);
			else shp = SHPCreate(shp_file, SHPT_ARC);
			shape_creator = create_lines;
			break;

		case POLYGONTYPE:
			if ( is3d ) shp = SHPCreate(shp_file, SHPT_POLYGONZ);
			else shp = SHPCreate(shp_file, SHPT_POLYGON);
			shape_creator = create_polygons;
			break;

		case MULTIPOLYGONTYPE:
			if ( is3d ) shp = SHPCreate(shp_file, SHPT_POLYGONZ);
			else shp = SHPCreate(shp_file, SHPT_POLYGON);
			shape_creator = create_multipolygons;
			break;

		case POINTTYPE:
			if ( is3d ) shp = SHPCreate(shp_file, SHPT_POINTZ);
			else shp = SHPCreate(shp_file, SHPT_POINT);
			shape_creator = create_points;
			break;

		case MULTIPOINTTYPE:
			if ( is3d ) shp = SHPCreate(shp_file, SHPT_MULTIPOINTZ);
			else shp = SHPCreate(shp_file, SHPT_MULTIPOINT);
			shape_creator = create_multipoints;
			break;

		
		default:
			shp = NULL;
			shape_creator = NULL;
			fprintf(stderr, "You've found a bug! (%s:%d)\n",
				__FILE__, __LINE__);
			return 0;

	}

	return 1;
}


/* 
 * Passed result is a 1 row result.
 * Return 1 on success.
 * Return 0 on failure.
 */
int
addRecord(PGresult *res, int row)
{
	int j;
	int nFields = PQnfields(res);
	int flds = 0; /* number of dbf field */
	char *val;

	for (j=0; j<nFields; j++)
	{
		/* Integer attribute */
		if (type_ary[j] == 1)
		{
			val = (char *)PQgetvalue(res, 0, j);
			int temp = atoi(val);
			if (!DBFWriteIntegerAttribute(dbf, row, flds, temp))
			{
				fprintf(stderr, "error(int) - Record could not be created\n");
				return 0;
			}
			flds++;
			continue;
		}
		
		/* Double attribute */
		if (type_ary[j] == 2)
		{
			val = PQgetvalue(res, 0, j);
			double temp = atof(val);
			if (!DBFWriteDoubleAttribute(dbf, row, flds, temp))
			{
				fprintf(stderr, "error(double) - Record could "
						"not be created\n");
				return 0;
			}
			flds++;
			continue;
		}

		/* Default (not geometry) attribute */
		if (type_ary[j] != 9)
		{
			val = PQgetvalue(res, 0, j);
			if(!DBFWriteStringAttribute(dbf, row, flds, val))
			{
				printf("error(string) - Record could not be "
						"created\n");
				return 0;
			}
			flds++;
			continue;
		}
		
		/* If we arrived here it is a geometry attribute */

		/* not the requested one, skipping */
		if ( j != geo_col_num ) continue;
		
		// (All your base belong to us. For great justice.) 

		val = PQgetvalue(res, 0, j);
		if ( ! shape_creator )
		{
			fprintf(stderr, "shape_creator is not set...\n");
			return 0;
		}
		if ( ! shape_creator(val, row, shp, is3d) )
		{
			fprintf(stderr, "Error creating shape for record %d "
					"(geotype is %d)\n", row, geotype);
			return 0;
		}
	}

	return 1;
}

/* 
 * Return allocate memory. Free after use.
 */
char *
getTableOID(char *table)
{
   PGresult *res3;
	char *query;
	char *ret;

	query = (char *)malloc(strlen(table)+256);
	sprintf(query, "SELECT oid FROM pg_class WHERE relname = '%s'", table);
	res3 = PQexec(conn, query);
	free(query);
	if ( ! res3 || PQresultStatus(res3) != PGRES_TUPLES_OK ) {
		fprintf(stderr, "%s\n", PQerrorMessage(conn));
		exit_nicely(conn);
	}
	if(PQntuples(res3) == 1 ){
		ret = strdup(PQgetvalue(res3, 0, 0));
	}else if(PQntuples(res3) == 0 ){
		fprintf(stderr, "ERROR: Cannot determine relation OID.\n");
		return NULL;
	}else{
		ret = strdup(PQgetvalue(res3, 0, 0));
		fprintf(stderr, "Warning: Multiple relations detected, the program will only dump the first relation.\n");
	}	
	return ret;
}

/*
 * Return geometry type as defined at top file.
 * Return -1 on error
 * Return  0 on unknown or unsupported geometry type
 */
int
getGeometryType(char *table, char *geo_col_name)
{
	char query[1024];
	PGresult *res;
	char *geo_str; // the geometry type string

	//get what kind of Geometry type is in the table
	sprintf(query, "SELECT DISTINCT geometrytype(\"%s\") FROM \"%s\" WHERE NOT geometrytype(\"%s\") IS NULL", geo_col_name, table, geo_col_name);

	//printf("\n\n-->%s\n\n",query);
	res = PQexec(conn, query);	
	if ( ! res || PQresultStatus(res) != PGRES_TUPLES_OK ) {
		fprintf(stderr, "%s\n", PQerrorMessage(conn));
		return -1;
	}

	if(PQntuples(res) == 1 ){
		geo_str = (char *)malloc(strlen(PQgetvalue(res, 0, 0))+1);
		memcpy(geo_str, (char *)PQgetvalue(res, 0, 0),strlen(PQgetvalue(res,0,0))+1);
	}else if(PQntuples(res) > 1 ){
		fprintf(stderr, "ERROR: Cannot have multiple geometry types in a shapefile.\nUse option -t(unimplemented currently,sorry...) to specify what type of geometry you want dumped\n\n");
		return -1;
	}else{
		printf("ERROR: Cannot determine geometry type of table. \n");
		return -1;
	}

	if ( ! strncmp(geo_str, "MULTILIN", 8) ) return MULTILINETYPE;
	if ( ! strncmp(geo_str, "MULTIPOL", 8) ) return MULTIPOLYGONTYPE;
	if ( ! strncmp(geo_str, "MULTIPOI", 8) ) return MULTIPOINTTYPE;
	if ( ! strncmp(geo_str, "LINESTRI", 8) ) return LINETYPE;
	if ( ! strncmp(geo_str, "POLYGON", 7) ) return POLYGONTYPE;
	if ( ! strncmp(geo_str, "POINT", 5) ) return POINTTYPE;

	fprintf(stderr, "type '%s' is not Supported at this time.\n",
			geo_str);
	fprintf(stderr, "The DBF file will be created but not the shx "
			"or shp files.\n");
	return  0;
}

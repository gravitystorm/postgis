/**********************************************************************
 * $Id:$
 *
 * PostGIS - Spatial Types for PostgreSQL
 * http://postgis.refractions.net
 * Copyright 2010 Olivier Courtin <olivier.courtin@oslandia.com>
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU General Public Licence. See the COPYING file.
 *
 **********************************************************************/

#include "cu_polyhedralsurface.h"


void polyhedralsurface_parse(void)
{
	LWGEOM *geom;
	cu_error_msg_reset();	/* Because i don't trust that much prior tests...  ;) */

	/* 2 dims */
	geom = lwgeom_from_ewkt("POLYHEDRALSURFACE(((0 1,2 3,4 5,0 1)))", PARSER_CHECK_NONE);
	CU_ASSERT_EQUAL(strlen(cu_error_msg), 0);
	CU_ASSERT_EQUAL(TYPE_GETTYPE(geom->type), POLYHEDRALSURFACETYPE);
	CU_ASSERT_STRING_EQUAL("010D00000001000000010300000001000000040000000000000000000000000000000000F03F00000000000000400000000000000840000000000000104000000000000014400000000000000000000000000000F03F", lwgeom_to_hexwkb(geom, PARSER_CHECK_NONE, (char) -1));
	CU_ASSERT_STRING_EQUAL("POLYHEDRALSURFACE(((0 1,2 3,4 5,0 1)))", lwgeom_to_ewkt(geom, PARSER_CHECK_NONE));
	lwgeom_free(geom);

	/* 3DM */
	geom = lwgeom_from_ewkt("POLYHEDRALSURFACEM(((0 1 2,3 4 5,6 7 8,0 1 2)))", PARSER_CHECK_NONE);
	CU_ASSERT_EQUAL(strlen(cu_error_msg), 0);
	CU_ASSERT_EQUAL(TYPE_GETTYPE(geom->type), POLYHEDRALSURFACETYPE);
	CU_ASSERT_STRING_EQUAL("POLYHEDRALSURFACEM(((0 1 2,3 4 5,6 7 8,0 1 2)))", lwgeom_to_ewkt(geom, PARSER_CHECK_NONE));
	CU_ASSERT_STRING_EQUAL("010D00004001000000010300004001000000040000000000000000000000000000000000F03F000000000000004000000000000008400000000000001040000000000000144000000000000018400000000000001C4000000000000020400000000000000000000000000000F03F0000000000000040", lwgeom_to_hexwkb(geom, PARSER_CHECK_NONE, (char) -1));
	lwgeom_free(geom);

	/* ERROR: a missing Z values */
	geom = lwgeom_from_ewkt("POLYHEDRALSURFACE(((0 1 2,3 4 5,6 7,0 1 2)))", PARSER_CHECK_NONE);
	CU_ASSERT_STRING_EQUAL("can not mix dimensionality in a geometry", cu_error_msg);
	cu_error_msg_reset();
	lwgeom_free(geom);

	/* 1 face with 1 interior ring */
	geom = lwgeom_from_ewkt("POLYHEDRALSURFACE(((0 1 2,3 4 5,6 7 8,0 1 2),(9 10 11,12 13 14,15 16 17,9 10 11)))", PARSER_CHECK_NONE);
	CU_ASSERT_EQUAL(strlen(cu_error_msg), 0);
	CU_ASSERT_EQUAL(TYPE_GETTYPE(geom->type), POLYHEDRALSURFACETYPE);
	CU_ASSERT_STRING_EQUAL("POLYHEDRALSURFACE(((0 1 2,3 4 5,6 7 8,0 1 2),(9 10 11,12 13 14,15 16 17,9 10 11)))", lwgeom_to_ewkt(geom, PARSER_CHECK_NONE));
	CU_ASSERT_STRING_EQUAL("010D00008001000000010300008002000000040000000000000000000000000000000000F03F000000000000004000000000000008400000000000001040000000000000144000000000000018400000000000001C4000000000000020400000000000000000000000000000F03F00000000000000400400000000000000000022400000000000002440000000000000264000000000000028400000000000002A400000000000002C400000000000002E4000000000000030400000000000003140000000000000224000000000000024400000000000002640", lwgeom_to_hexwkb(geom, PARSER_CHECK_NONE, (char) -1));
	lwgeom_free(geom);

	/* ERROR: non closed rings */
	geom = lwgeom_from_ewkt("POLYHEDRALSURFACE(((0 1 2,3 4 5,6 7 8,0 0 2)))", PARSER_CHECK_NONE);
	CU_ASSERT_STRING_EQUAL("geometry contains non-closed rings", cu_error_msg);
	cu_error_msg_reset();
	lwgeom_free(geom);

	/* ERROR: non closed face in Z dim */
	geom = lwgeom_from_ewkt("POLYHEDRALSURFACE(((0 1 2,3 4 5,6 7 8,0 1 3)))", PARSER_CHECK_NONE);
	CU_ASSERT_STRING_EQUAL("geometry contains non-closed rings", cu_error_msg);
	cu_error_msg_reset();
	lwgeom_free(geom);

	/* ERROR: non closed face in Z dim, with a 4D geom */
	geom = lwgeom_from_ewkt("POLYHEDRALSURFACE(((0 1 2 3,4 5 6 7,8 9 10 11,0 1 3 3)))", PARSER_CHECK_NONE);
	CU_ASSERT_STRING_EQUAL("geometry contains non-closed rings", cu_error_msg);
	cu_error_msg_reset();
	lwgeom_free(geom);

	/* ERROR: only 3 points in a face */
	geom = lwgeom_from_ewkt("POLYHEDRALSURFACE(((0 1 2,3 4 5,0 1 2)))", PARSER_CHECK_NONE);
	CU_ASSERT_STRING_EQUAL("geometry requires more points", cu_error_msg);
	cu_error_msg_reset();
	lwgeom_free(geom);

	/* EMPTY face */	
	geom = lwgeom_from_ewkt("POLYHEDRALSURFACE EMPTY", PARSER_CHECK_NONE);
	CU_ASSERT_EQUAL(strlen(cu_error_msg), 0);
        /*
          NOTA: Theses 3 ASSERT results will change a day cf #294 
        */
	CU_ASSERT_EQUAL(TYPE_GETTYPE(geom->type), COLLECTIONTYPE);
	CU_ASSERT_STRING_EQUAL("010700000000000000", lwgeom_to_hexwkb(geom, PARSER_CHECK_NONE, (char) -1));
	CU_ASSERT_STRING_EQUAL("GEOMETRYCOLLECTION EMPTY", lwgeom_to_ewkt(geom, PARSER_CHECK_NONE));
	lwgeom_free(geom);

	/* A simple tetrahedron */
	geom = lwgeom_from_ewkt("POLYHEDRALSURFACE(((0 0 0,0 0 1,0 1 0,0 0 0)),((0 0 0,0 1 0,1 0 0,0 0 0)),((0 0 0,1 0 0,0 0 1,0 0 0)),((1 0 0,0 1 0,0 0 1,1 0 0)))", PARSER_CHECK_NONE);
	CU_ASSERT_EQUAL(strlen(cu_error_msg), 0);
	CU_ASSERT_EQUAL(TYPE_GETTYPE(geom->type), POLYHEDRALSURFACETYPE);
	CU_ASSERT_EQUAL(geom->SRID, -1);
	CU_ASSERT_STRING_EQUAL("POLYHEDRALSURFACE(((0 0 0,0 0 1,0 1 0,0 0 0)),((0 0 0,0 1 0,1 0 0,0 0 0)),((0 0 0,1 0 0,0 0 1,0 0 0)),((1 0 0,0 1 0,0 0 1,1 0 0)))", lwgeom_to_ewkt(geom, PARSER_CHECK_NONE)); 
	CU_ASSERT_STRING_EQUAL("010D000080040000000103000080010000000400000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000F03F0000000000000000000000000000F03F0000000000000000000000000000000000000000000000000000000000000000010300008001000000040000000000000000000000000000000000000000000000000000000000000000000000000000000000F03F0000000000000000000000000000F03F0000000000000000000000000000000000000000000000000000000000000000000000000000000001030000800100000004000000000000000000000000000000000000000000000000000000000000000000F03F0000000000000000000000000000000000000000000000000000000000000000000000000000F03F00000000000000000000000000000000000000000000000001030000800100000004000000000000000000F03F000000000000000000000000000000000000000000000000000000000000F03F000000000000000000000000000000000000000000000000000000000000F03F000000000000F03F00000000000000000000000000000000", lwgeom_to_hexwkb(geom, PARSER_CHECK_NONE, (char) -1));
	lwgeom_free(geom);

	/* A 4D tetrahedron */
	geom = lwgeom_from_ewkt("POLYHEDRALSURFACE(((0 0 0 0,0 0 1 0,0 1 0 2,0 0 0 0)),((0 0 0 0,0 1 0 0,1 0 0 4,0 0 0 0)),((0 0 0 0,1 0 0 0,0 0 1 6,0 0 0 0)),((1 0 0 0,0 1 0 0,0 0 1 0,1 0 0 0)))", PARSER_CHECK_NONE);
	CU_ASSERT_EQUAL(strlen(cu_error_msg), 0);
	CU_ASSERT_EQUAL(TYPE_GETTYPE(geom->type), POLYHEDRALSURFACETYPE);
	CU_ASSERT_EQUAL(TYPE_HASM(geom->type), 1);
	CU_ASSERT_EQUAL(geom->SRID, -1);
	CU_ASSERT_STRING_EQUAL("POLYHEDRALSURFACE(((0 0 0 0,0 0 1 0,0 1 0 2,0 0 0 0)),((0 0 0 0,0 1 0 0,1 0 0 4,0 0 0 0)),((0 0 0 0,1 0 0 0,0 0 1 6,0 0 0 0)),((1 0 0 0,0 1 0 0,0 0 1 0,1 0 0 0)))", lwgeom_to_ewkt(geom, PARSER_CHECK_NONE)); 
	CU_ASSERT_STRING_EQUAL("010D0000C00400000001030000C00100000004000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000F03F00000000000000000000000000000000000000000000F03F00000000000000000000000000000040000000000000000000000000000000000000000000000000000000000000000001030000C0010000000400000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000F03F00000000000000000000000000000000000000000000F03F000000000000000000000000000000000000000000001040000000000000000000000000000000000000000000000000000000000000000001030000C001000000040000000000000000000000000000000000000000000000000000000000000000000000000000000000F03F00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000F03F0000000000001840000000000000000000000000000000000000000000000000000000000000000001030000C00100000004000000000000000000F03F0000000000000000000000000000000000000000000000000000000000000000000000000000F03F0000000000000000000000000000000000000000000000000000000000000000000000000000F03F0000000000000000000000000000F03F000000000000000000000000000000000000000000000000", lwgeom_to_hexwkb(geom, PARSER_CHECK_NONE, (char) -1));
	lwgeom_free(geom);


	/* explicit SRID */
	geom = lwgeom_from_ewkt("SRID=4326;POLYHEDRALSURFACE(((0 0 0,0 0 1,0 1 0,0 0 0)),((0 0 0,0 1 0,1 0 0,0 0 0)),((0 0 0,1 0 0,0 0 1,0 0 0)),((1 0 0,0 1 0,0 0 1,1 0 0)))", PARSER_CHECK_NONE);
	CU_ASSERT_EQUAL(strlen(cu_error_msg), 0);
	CU_ASSERT_EQUAL(TYPE_GETTYPE(geom->type), POLYHEDRALSURFACETYPE);
	CU_ASSERT_EQUAL(geom->SRID, 4326);
	CU_ASSERT_STRING_EQUAL("SRID=4326;POLYHEDRALSURFACE(((0 0 0,0 0 1,0 1 0,0 0 0)),((0 0 0,0 1 0,1 0 0,0 0 0)),((0 0 0,1 0 0,0 0 1,0 0 0)),((1 0 0,0 1 0,0 0 1,1 0 0)))", lwgeom_to_ewkt(geom, PARSER_CHECK_NONE)); 
	CU_ASSERT_STRING_EQUAL("010D0000A0E6100000040000000103000080010000000400000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000F03F0000000000000000000000000000F03F0000000000000000000000000000000000000000000000000000000000000000010300008001000000040000000000000000000000000000000000000000000000000000000000000000000000000000000000F03F0000000000000000000000000000F03F0000000000000000000000000000000000000000000000000000000000000000000000000000000001030000800100000004000000000000000000000000000000000000000000000000000000000000000000F03F0000000000000000000000000000000000000000000000000000000000000000000000000000F03F00000000000000000000000000000000000000000000000001030000800100000004000000000000000000F03F000000000000000000000000000000000000000000000000000000000000F03F000000000000000000000000000000000000000000000000000000000000F03F000000000000F03F00000000000000000000000000000000", lwgeom_to_hexwkb(geom, PARSER_CHECK_NONE, (char) -1));

}


/*
** Used by test harness to register the tests in this file.
*/
CU_TestInfo polyhedralsurface_tests[] = {
        PG_TEST(polyhedralsurface_parse),
        CU_TEST_INFO_NULL
};
CU_SuiteInfo polyhedralsurface_suite = {"PolyhedralSurface Suite",  NULL,  NULL, polyhedralsurface_tests};

/**********************************************************************
 * $Id$
 *
 * PostGIS - Spatial Types for PostgreSQL
 * http://postgis.refractions.net
 * Copyright 2009 Paul Ramsey <pramsey@opengeo.org>
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU General Public Licence. See the COPYING file.
 *
 **********************************************************************/

#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "access/tupmacs.h"
#include "utils/array.h"
#include "utils/lsyscache.h"

#include "liblwgeom.h"
#include "lwgeom_pg.h"

Datum pgis_geometry_accum_transfn(PG_FUNCTION_ARGS);
Datum pgis_geometry_accum_finalfn(PG_FUNCTION_ARGS);
Datum pgis_geometry_union_finalfn(PG_FUNCTION_ARGS);
Datum pgis_geometry_collect_finalfn(PG_FUNCTION_ARGS);
Datum pgis_geometry_polygonize_finalfn(PG_FUNCTION_ARGS);
Datum pgis_geometry_makeline_finalfn(PG_FUNCTION_ARGS);
Datum pgis_abs_in(PG_FUNCTION_ARGS);
Datum pgis_abs_out(PG_FUNCTION_ARGS);

/* External prototypes */
Datum pgis_union_geometry_array(PG_FUNCTION_ARGS);
Datum LWGEOM_collect_garray(PG_FUNCTION_ARGS);
Datum polygonize_garray(PG_FUNCTION_ARGS);
Datum LWGEOM_makeline_garray(PG_FUNCTION_ARGS);

/*
** To pass the internal ArrayBuildState pointer between the 
** transfn and finalfn we need to wrap it into a custom type first,
** the pgis_abs type in our case.  
*/

typedef struct {
	ArrayBuildState *a;
	} pgis_abs;

/*
** We're never going to use this type externally so the in/out 
** functions are dummies.
*/
PG_FUNCTION_INFO_V1(pgis_abs_in);
Datum
pgis_abs_in(PG_FUNCTION_ARGS)
{
	ereport(ERROR,(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
            errmsg("function pgis_abs_in not implemented")));
	PG_RETURN_POINTER(NULL);
}
PG_FUNCTION_INFO_V1(pgis_abs_out);
Datum
pgis_abs_out(PG_FUNCTION_ARGS)
{
	ereport(ERROR,(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
            errmsg("function pgis_abs_out not implemented")));
	PG_RETURN_POINTER(NULL);
}

/*
** The transfer function hooks into the PostgreSQL accumArrayResult()
** function (present since 8.0) to build an array in a side memory
** context.
*/
PG_FUNCTION_INFO_V1(pgis_geometry_accum_transfn);
Datum
pgis_geometry_accum_transfn(PG_FUNCTION_ARGS)
{
	Oid arg1_typeid = get_fn_expr_argtype(fcinfo->flinfo, 1);
	MemoryContext aggcontext;
	ArrayBuildState *state;
	pgis_abs *p;
	Datum elem;

	if (arg1_typeid == InvalidOid)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("could not determine input data type")));

	if (fcinfo->context && IsA(fcinfo->context, AggState))
		aggcontext = ((AggState *) fcinfo->context)->aggcontext;
#if POSTGIS_PGSQL_VERSION >= 84
	else if (fcinfo->context && IsA(fcinfo->context, WindowAggState))
		aggcontext = ((WindowAggState *) fcinfo->context)->wincontext;
#endif
	else
	{
		/* cannot be called directly because of dummy-type argument */
		elog(ERROR, "array_agg_transfn called in non-aggregate context");
		aggcontext = NULL;		/* keep compiler quiet */
	}

	if ( PG_ARGISNULL(0) )
	{
		p = (pgis_abs*) palloc(sizeof(pgis_abs));
		p->a = NULL;
	}
	else
	{
		p = (pgis_abs*) PG_GETARG_POINTER(0);
	}
	state = p->a;
	elem = PG_ARGISNULL(1) ? (Datum) 0 : PG_GETARG_DATUM(1);
	state = accumArrayResult(state,
							 elem,
							 PG_ARGISNULL(1),
							 arg1_typeid,
							 aggcontext);
	p->a = state;

	PG_RETURN_POINTER(p);
}

Datum pgis_accum_finalfn(pgis_abs *p, MemoryContext mctx, FunctionCallInfo fcinfo);

Datum 
pgis_accum_finalfn(pgis_abs *p, MemoryContext mctx, FunctionCallInfo fcinfo)
{
	int dims[1];
	int lbs[1];
	ArrayBuildState *state;
	Datum result;
	
	/* cannot be called directly because of internal-type argument */
	Assert(fcinfo->context &&
	       (IsA(fcinfo->context, AggState) 
#if POSTGIS_PGSQL_VERSION >= 84
			|| IsA(fcinfo->context, WindowAggState)
#endif
			));
	
	state = p->a;
	dims[0] = state->nelems;
	lbs[0] = 1;
#if POSTGIS_PGSQL_VERSION < 84
    result = makeMdArrayResult(state, 1, dims, lbs, mctx);
#else
	/* Release working state if regular aggregate, but not if window agg */
 	result = makeMdArrayResult(state, 1, dims, lbs, mctx,
                       	       IsA(fcinfo->context, AggState));
#endif
	return result;
}

PG_FUNCTION_INFO_V1(pgis_geometry_accum_finalfn);
Datum
pgis_geometry_accum_finalfn(PG_FUNCTION_ARGS)
{
	pgis_abs *p;
	Datum result = 0;

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();   /* returns null iff no input values */

	p = (pgis_abs*) PG_GETARG_POINTER(0);

	result = pgis_accum_finalfn(p, CurrentMemoryContext, fcinfo);

	PG_RETURN_DATUM(result);

}

PG_FUNCTION_INFO_V1(pgis_geometry_union_finalfn);
Datum
pgis_geometry_union_finalfn(PG_FUNCTION_ARGS)
{
	pgis_abs *p;
	Datum result = 0;
	Datum geometry_array = 0;

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();   /* returns null iff no input values */

	p = (pgis_abs*) PG_GETARG_POINTER(0);

	geometry_array = pgis_accum_finalfn(p, CurrentMemoryContext, fcinfo);
	result = DirectFunctionCall1( pgis_union_geometry_array, geometry_array );

	PG_RETURN_DATUM(result);
}

PG_FUNCTION_INFO_V1(pgis_geometry_collect_finalfn);
Datum
pgis_geometry_collect_finalfn(PG_FUNCTION_ARGS)
{
	pgis_abs *p;
	Datum result = 0;
	Datum geometry_array = 0;

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();   /* returns null iff no input values */

	p = (pgis_abs*) PG_GETARG_POINTER(0);

	geometry_array = pgis_accum_finalfn(p, CurrentMemoryContext, fcinfo);
	result = DirectFunctionCall1( LWGEOM_collect_garray, geometry_array );

	PG_RETURN_DATUM(result);
}

PG_FUNCTION_INFO_V1(pgis_geometry_polygonize_finalfn);
Datum
pgis_geometry_polygonize_finalfn(PG_FUNCTION_ARGS)
{
	pgis_abs *p;
	Datum result = 0;
	Datum geometry_array = 0;

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();   /* returns null iff no input values */

	p = (pgis_abs*) PG_GETARG_POINTER(0);

	geometry_array = pgis_accum_finalfn(p, CurrentMemoryContext, fcinfo);
	result = DirectFunctionCall1( polygonize_garray, geometry_array );

	PG_RETURN_DATUM(result);
}

PG_FUNCTION_INFO_V1(pgis_geometry_makeline_finalfn);
Datum
pgis_geometry_makeline_finalfn(PG_FUNCTION_ARGS)
{
	pgis_abs *p;
	Datum result = 0;
	Datum geometry_array = 0;

	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();   /* returns null iff no input values */

	p = (pgis_abs*) PG_GETARG_POINTER(0);

	geometry_array = pgis_accum_finalfn(p, CurrentMemoryContext, fcinfo);
	result = DirectFunctionCall1( LWGEOM_makeline_garray, geometry_array );

	PG_RETURN_DATUM(result);
}


#ifndef _LWPGIS_DEFINES
#define _LWPGIS_DEFINES

/*
 * Define just the version numbers; otherwise we get some strange substitutions in lwpostgis.sql.in
 */
#define POSTGIS_PGSQL_VERSION 83
#define POSTGIS_GEOS_VERSION 30
#define POSTGIS_PROJ_VERSION 46

/*
 * Define the build date and the version number
 * (these substitiutions are done with extra quotes sinces CPP
 * won't substitute within apostrophes)
 */
#define _POSTGIS_SQL_SELECT_POSTGIS_VERSION 'SELECT ''1.4 USE_GEOS=1 USE_PROJ=1 USE_STATS=1''::text AS version'
#define _POSTGIS_SQL_SELECT_POSTGIS_BUILD_DATE 'SELECT ''2008-05-14 08:59:43''::text AS version'


#define CREATEFUNCTION CREATE OR REPLACE FUNCTION

#if POSTGIS_PGSQL_VERSION > 72
# define _IMMUTABLE_STRICT IMMUTABLE STRICT
# define _IMMUTABLE IMMUTABLE
# define _STABLE_STRICT STABLE STRICT
# define _STABLE STABLE
# define _VOLATILE_STRICT VOLATILE STRICT
# define _VOLATILE VOLATILE
# define _STRICT STRICT
# define _SECURITY_DEFINER SECURITY DEFINER
#else 
# define _IMMUTABLE_STRICT  with(iscachable,isstrict)
# define _IMMUTABLE with(iscachable)
# define _STABLE_STRICT with(isstrict)
# define _STABLE 
# define _VOLATILE_STRICT with(isstrict)
# define _VOLATILE 
# define _STRICT with(isstrict)
# define _SECURITY_DEFINER 
#endif 

#if POSTGIS_PGSQL_VERSION >= 73
# define HAS_SCHEMAS 1
#endif

#endif /* _LWPGIS_DEFINES */

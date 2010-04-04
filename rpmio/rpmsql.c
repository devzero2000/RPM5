#include "system.h"

#define	_RPMSQL_INTERNAL
#include <rpmsql.h>

#if defined(WITH_SQLITE)
#include <sqlite3.h>
#endif

#include "debug.h"

/*@unchecked@*/
int _rpmsql_debug = -1;

/*@unchecked@*/ /*@relnull@*/
rpmsql _rpmsqlI = NULL;

/*@unchecked@*/
struct rpmsql_s _sql;		/* XXX static */

/*==============================================================*/

static void rpmsqlFini(void * _sql)
	/*@globals fileSystem @*/
	/*@modifies *_sql, fileSystem @*/
{
    rpmsql sql = _sql;

SQLDBG((stderr, "==> %s(%p)\n", __FUNCTION__, sql));
    sql->av = argvFree(sql->av);
#if defined(WITH_SQLITE)
    if (sql->I) {
	int xx;
	xx = sqlite3_close((sqlite3 *)sql->I);
    }
#endif
    sql->I = NULL;
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmsqlPool;

static rpmsql rpmsqlGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmsqlPool, fileSystem @*/
	/*@modifies pool, _rpmsqlPool, fileSystem @*/
{
    rpmsql sql;

    if (_rpmsqlPool == NULL) {
	_rpmsqlPool = rpmioNewPool("sql", sizeof(*sql), -1, _rpmsql_debug,
			NULL, NULL, rpmsqlFini);
	pool = _rpmsqlPool;
    }
    sql = (rpmsql) rpmioGetPool(pool, sizeof(*sql));
    memset(((char *)sql)+sizeof(sql->_item), 0, sizeof(*sql)-sizeof(sql->_item));
    return sql;
}

static rpmsql rpmsqlI(void)
	/*@globals _rpmsqlI @*/
	/*@modifies _rpmsqlI @*/
{
    if (_rpmsqlI == NULL) {
	_rpmsqlI = rpmsqlNew(NULL, 0);
    }
SQLDBG((stderr, "<== %s() _rpmsqlI %p\n", __FUNCTION__, _rpmsqlI));
    return _rpmsqlI;
}

static void rpmsqlInitEnv(rpmsql sql)
	/*@modifies sql @*/
{
}

rpmsql rpmsqlNew(char ** av, uint32_t flags)
{
    rpmsql sql = rpmsqlGetPool(_rpmsqlPool);
    int ac = argvCount((ARGV_t)av);

SQLDBG((stderr, "==> %s(%p[%u], 0x%x)\n", __FUNCTION__, av, (unsigned)ac, flags));

#if defined(WITH_SQLITE)
    if (av && av[1]) {
	int xx;
	xx = sqlite3_open(av[1], &sql->I);
    }
    rpmsqlInitEnv(sql);
#endif

    return rpmsqlLink(sql);
}

rpmRC rpmsqlRun(rpmsql sql, const char * str, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

    if (sql == NULL) sql = rpmsqlI();

#ifdef	NOTYET
    if (str != NULL) {
    }
#endif

SQLDBG((stderr, "<== %s(%p,%p[%u]) rc %d\n", __FUNCTION__, sql, str, (unsigned)(str ? strlen(str) : 0), rc));

    return rc;
}

/** \ingroup rpmio
 * \file rpmio/rpmodbc.c
 */

#include "system.h"

#if defined(HAVE_SQLEXT_H)
#include <sqlext.h>
#endif

#include <rpmiotypes.h>
#include <rpmio.h>	/* for *Pool methods */
#include <rpmlog.h>
#include <rpmurl.h>

#define	_RPMODBC_INTERNAL
#include <rpmodbc.h>

#include "debug.h"

/*@unchecked@*/
int _rpmodbc_debug = -1;

#define	SPEW(_t, _rc, _odbc)	\
  { if ((_t) || _rpmodbc_debug ) \
	fprintf(stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, (_odbc), \
		(_rc)); \
  }

/*==============================================================*/

int rpmodbcConnect(rpmodbc odbc,
		const char * db, const char * u, const char * pw)
{
    int rc = -1;

fprintf(stderr, "--> %s(%p,%s,%s,%s)\n", __FUNCTION__, odbc, db, u, pw);
    odbc->db = xstrdup(db);
    odbc->u = xstrdup(u);
    odbc->pw = xstrdup(pw);

#if defined(WITH_UNIXODBC)
assert(odbc->env);
    if (odbc->dbc == NULL) {
	SQLAllocHandle(SQL_HANDLE_DBC, odbc->env, &odbc->dbc);
assert(odbc->dbc);
    }

    rc = SQLConnect(odbc->dbc,
		(SQLCHAR *) db, SQL_NTS,
		(SQLCHAR *) u, SQL_NTS,
		(SQLCHAR *) pw, SQL_NTS);
#endif

SPEW(0, rc, odbc);
    return rc;
}

int rpmodbcDisconnect(rpmodbc odbc)
{
    int rc = 0;

#if defined(WITH_UNIXODBC)
assert(odbc->env);
assert(odbc->dbc);
    SQLDisconnect(odbc->dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, odbc->dbc );
    odbc->dbc = NULL;
#endif

SPEW(0, rc, odbc);
    return rc;
}

int rpmodbcListDataSources(rpmodbc odbc, void *_fp)
{
    int rc = 0;
    FILE * fp = (_fp ? _fp : stderr);

#if defined(WITH_UNIXODBC)
    SQLCHAR dsn[256];
    SQLCHAR desc[256];
    SQLSMALLINT dsn_ret;
    SQLSMALLINT desc_ret;
    SQLUSMALLINT direction = SQL_FETCH_FIRST;
    SQLRETURN ret;

assert(odbc->env);
    while (SQL_SUCCEEDED(ret = SQLDrivers(odbc->env, direction,
					dsn, sizeof(dsn), &dsn_ret,
					desc, sizeof(desc), &desc_ret)))
    {
	direction = SQL_FETCH_NEXT;
	fprintf(fp, "%s - %s\n", dsn, desc);
#ifdef	NOTYET
	if (ret == SQL_SUCCESS_WITH_INFO) fprintf(fp, "\tdata truncation\n");
#endif
    }
#endif

SPEW(0, rc, odbc);
    return rc;
}

int rpmodbcListDrivers(rpmodbc odbc, void *_fp)
{
    int rc = 0;
    FILE * fp = (_fp ? _fp : stderr);

#if defined(WITH_UNIXODBC)
    SQLCHAR driver[256];
    SQLCHAR attr[256];
    SQLSMALLINT driver_ret;
    SQLSMALLINT attr_ret;
    SQLUSMALLINT direction = SQL_FETCH_FIRST;
    SQLRETURN ret;

assert(odbc->env);
    while (SQL_SUCCEEDED(ret = SQLDrivers(odbc->env, direction,
					driver, sizeof(driver), &driver_ret,
					attr, sizeof(attr), &attr_ret)))
    {
	direction = SQL_FETCH_NEXT;
	fprintf(fp, "%s - %s\n", driver, attr);
#ifdef	NOTYET
	if (ret == SQL_SUCCESS_WITH_INFO) fprintf(fp, "\tdata truncation\n");
#endif
    }
#endif

SPEW(0, rc, odbc);
    return rc;
}

int rpmodbcTables(rpmodbc odbc)
{
    int rc = 0;

#if defined(WITH_UNIXODBC)
assert(odbc->env);
assert(odbc->dbc);
    if (odbc->stmt == NULL) {
	SQLAllocHandle(SQL_HANDLE_STMT, odbc->dbc, &odbc->stmt);
assert(odbc->stmt);
    }

    SQLTables(odbc->stmt, NULL, 0, NULL, 0, NULL, 0,
		(SQLCHAR *) "TABLE", SQL_NTS);
#endif

SPEW(0, rc, odbc);
    return rc;
}

int rpmodbcColumns(rpmodbc odbc)
{
    int rc = 0;

#if defined(WITH_UNIXODBC)
assert(odbc->env);
assert(odbc->dbc);
    if (odbc->stmt == NULL) {
	SQLAllocHandle(SQL_HANDLE_STMT, odbc->dbc, &odbc->stmt);
assert(odbc->stmt);
    }

    SQLColumns(odbc->stmt, NULL, 0, NULL, 0, NULL, 0,
		(SQLCHAR *) "TABLE", SQL_NTS);
#endif

SPEW(0, rc, odbc);
    return rc;
}

int rpmodbcNCols(rpmodbc odbc)
{
    int rc = 0;

#if defined(WITH_UNIXODBC)
    SQLSMALLINT columns;

assert(odbc->env);
assert(odbc->dbc);
assert(odbc->stmt);
    SQLNumResultCols(odbc->stmt, &columns);
    rc = columns;
#endif

SPEW(0, rc, odbc);
    return rc;
}

int rpmodbcExecDirect(rpmodbc odbc, const char * s, size_t ns)
{
    int rc = 0;

    if (ns == 0)
	ns = strlen(s);

#if defined(WITH_UNIXODBC)
assert(odbc->env);
assert(odbc->dbc);
    if (odbc->stmt == NULL) {
	SQLAllocHandle(SQL_HANDLE_STMT, odbc->dbc, &odbc->stmt);
assert(odbc->stmt);
    }

    rc = SQLExecDirect(odbc->stmt, (SQLCHAR *) s, (SQLINTEGER) ns);
#endif

SPEW(0, rc, odbc);
    return rc;
}

int rpmodbcPrepare(rpmodbc odbc, const char * s, size_t ns)
{
    int rc = 0;

    if (ns == 0)
	ns = strlen(s);

#if defined(WITH_UNIXODBC)
assert(odbc->env);
assert(odbc->dbc);
    if (odbc->stmt == NULL) {
	SQLAllocHandle(SQL_HANDLE_STMT, odbc->dbc, &odbc->stmt);
assert(odbc->stmt);
    }

    rc = SQLPrepare(odbc->stmt, (SQLCHAR *) s, (SQLINTEGER) ns);
#endif

SPEW(0, rc, odbc);
    return rc;
}

int rpmodbcExecute(rpmodbc odbc)
{
    int rc = -1;

#if defined(WITH_UNIXODBC)
assert(odbc->env);
assert(odbc->dbc);
assert(odbc->stmt);

    rc = SQLExecute(odbc->stmt);
#endif

SPEW(0, rc, odbc);
    return rc;
}

int rpmodbcFetch(rpmodbc odbc)
{
    int rc = 0;

#if defined(WITH_UNIXODBC)
assert(odbc->env);
assert(odbc->dbc);
assert(odbc->stmt);

    rc = SQLFetch(odbc->stmt);
#endif

SPEW(0, rc, odbc);
    return rc;
}

/*==============================================================*/

static void rpmodbcFini(void * _odbc)
	/*@globals fileSystem @*/
	/*@modifies *_odbc, fileSystem @*/
{
    rpmodbc odbc = _odbc;

#if defined(WITH_UNIXODBC)
    if (odbc->desc) {
	SQLFreeHandle(SQL_HANDLE_DESC, odbc->desc);
	odbc->desc = NULL;
    }
    if (odbc->stmt) {
	SQLFreeHandle(SQL_HANDLE_DESC, odbc->stmt);
	odbc->stmt = NULL;
    }
    if (odbc->dbc) {
	SQLFreeHandle(SQL_HANDLE_DBC, odbc->dbc);
	odbc->dbc = NULL;
    }
    if (odbc->env) {
	SQLFreeHandle(SQL_HANDLE_ENV, odbc->env);
	odbc->env = NULL;
    }
#endif

    odbc->pw = _free(odbc->pw);
    odbc->u = _free(odbc->u);
    odbc->db = _free(odbc->db);

    odbc->fn = _free(odbc->fn);
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmodbcPool = NULL;

static rpmodbc rpmodbcGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmodbcPool, fileSystem @*/
	/*@modifies pool, _rpmodbcPool, fileSystem @*/
{
    rpmodbc odbc;

    if (_rpmodbcPool == NULL) {
	_rpmodbcPool = rpmioNewPool("odbc", sizeof(*odbc), -1, _rpmodbc_debug,
			NULL, NULL, rpmodbcFini);
	pool = _rpmodbcPool;
    }
    odbc = (rpmodbc) rpmioGetPool(pool, sizeof(*odbc));
    memset(((char *)odbc)+sizeof(odbc->_item), 0, sizeof(*odbc)-sizeof(odbc->_item));
    return odbc;
}

rpmodbc rpmodbcNew(const char * fn, int flags)
{
    rpmodbc odbc = rpmodbcGetPool(_rpmodbcPool);

    if (fn)
	odbc->fn = xstrdup(fn);
    odbc->flags = flags;

#if defined(WITH_UNIXODBC)
    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &odbc->env);
assert(odbc->env);
    SQLSetEnvAttr(odbc->env, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
#endif

    return rpmodbcLink(odbc);
}

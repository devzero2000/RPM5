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
#include <rpmmacro.h>
#include <rpmurl.h>

#define	_RPMODBC_INTERNAL
#include <rpmodbc.h>

#include "debug.h"

/*@unchecked@*/
int _odbc_debug = -1;

#define	SPEW(_t, _rc, _odbc)	\
  { if ((_t) || _odbc_debug ) \
	fprintf(stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, (_odbc), \
		(_rc)); \
  }

/*==============================================================*/

int odbcConnect(ODBC_t odbc, const char * uri)
{
    const char * db = NULL;
    urlinfo u = NULL;
    int rc = -1;

fprintf(stderr, "--> %s(%p,%s)\n", __FUNCTION__, odbc, uri);

    if (uri) {
	const char * dbpath = NULL;
	int ut = urlPath(uri, &dbpath);
assert(ut == URL_IS_MYSQL || ut == URL_IS_POSTGRES);
	rc = urlSplit(uri, &u);
	db = rpmExpand(u->scheme, "_", basename((char *)dbpath), NULL);
    } else {
	u = odbc->u;
	db = xstrdup(odbc->db);
    }
assert(u);
assert(db);

fprintf(stderr, "\tdb: %s\n", db);
fprintf(stderr, "\t u: %s\n", u->user);
fprintf(stderr, "\tpw: %s\n", u->password);

#if defined(WITH_UNIXODBC)
assert(odbc->env);
    if (odbc->dbc == NULL) {
	SQLAllocHandle(SQL_HANDLE_DBC, odbc->env, &odbc->dbc);
assert(odbc->dbc);
    }

    /* XXX FIXME: odbc->u->user and odbc->u->password */
    rc = SQLConnect(odbc->dbc,
		(SQLCHAR *) db, SQL_NTS,
		(SQLCHAR *) u->user, SQL_NTS,
		(SQLCHAR *) u->password, SQL_NTS);
#endif

SPEW(0, rc, odbc);
    db = _free(db);
    return rc;
}

int odbcDisconnect(ODBC_t odbc)
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

int odbcListDataSources(ODBC_t odbc, void *_fp)
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

int odbcListDrivers(ODBC_t odbc, void *_fp)
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

int odbcTables(ODBC_t odbc)
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

int odbcColumns(ODBC_t odbc)
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

int odbcNCols(ODBC_t odbc)
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

int odbcExecDirect(ODBC_t odbc, const char * s, size_t ns)
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

int odbcPrepare(ODBC_t odbc, const char * s, size_t ns)
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

int odbcExecute(ODBC_t odbc)
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

int odbcFetch(ODBC_t odbc)
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

static void odbcFini(void * _odbc)
	/*@globals fileSystem @*/
	/*@modifies *_odbc, fileSystem @*/
{
    ODBC_t odbc = _odbc;

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

    odbc->db = _free(odbc->db);
    odbc->u = urlFree(odbc->u, __FUNCTION__);
    odbc->fn = _free(odbc->fn);
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _odbcPool = NULL;

static ODBC_t odbcGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _odbcPool, fileSystem @*/
	/*@modifies pool, _odbcPool, fileSystem @*/
{
    ODBC_t odbc;

    if (_odbcPool == NULL) {
	_odbcPool = rpmioNewPool("odbc", sizeof(*odbc), -1, _odbc_debug,
			NULL, NULL, odbcFini);
	pool = _odbcPool;
    }
    odbc = (ODBC_t) rpmioGetPool(pool, sizeof(*odbc));
    memset(((char *)odbc)+sizeof(odbc->_item), 0, sizeof(*odbc)-sizeof(odbc->_item));
    return odbc;
}

static char * _odbc_uri = "mysql://luser:jasnl@localhost/test";

ODBC_t odbcNew(const char * fn, int flags)
{
    ODBC_t odbc = odbcGetPool(_odbcPool);

    if (fn == NULL)
	fn = _odbc_uri;
    odbc->fn = xstrdup(fn);
    odbc->flags = flags;

    {	const char * dbpath = NULL;
	int ut = urlPath(fn, &dbpath);
	urlinfo u = NULL;
	int xx = urlSplit(fn, &u);
assert(ut == URL_IS_MYSQL || ut == URL_IS_POSTGRES);
	odbc->db = rpmExpand(u->scheme, "_", basename((char *)dbpath), NULL);
	odbc->u = urlLink(u, __FUNCTION__);
    }

#if defined(WITH_UNIXODBC)
    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &odbc->env);
assert(odbc->env);
    SQLSetEnvAttr(odbc->env, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
#endif

    return odbcLink(odbc);
}

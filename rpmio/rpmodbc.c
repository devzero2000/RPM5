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
int _odbc_debug = 0;

#define DBG(_t, _l) \
  if ((_t) || _odbc_debug) fprintf _l

#define	SPEW(_t, _rc, _odbc)	\
  { if ((_t) || _odbc_debug) \
	fprintf(stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, (_odbc), \
		(_rc)); \
  }

/*==============================================================*/
#if defined(WITH_UNIXODBC)
static int Xchkodbc(/*@unused@*/ ODBC_t odbc, const char * msg,
                int error, int printit,
                const char * func, const char * fn, unsigned ln)
{
    int rc = error;

    /* XXX filter SQL_NO_DATA(100) return. */
    if (printit && !SQL_SUCCEEDED(rc) && rc != SQL_NO_DATA) {
#define odbc_strerror(_e)	""	/* XXX odbc_strerror? */
        rpmlog(RPMLOG_ERR, "%s:%s:%u: %s(%d): %s\n",
                func, fn, ln, msg, rc, odbc_strerror(rc));
    }

    return rc;
}
#define CHECK(_odbc, _msg, _error)  \
    Xchkodbc(_odbc, _msg, _error, _odbc_debug, __FUNCTION__, __FILE__, __LINE__)
#endif	/* WITH_UNIXODBC */

/*==============================================================*/

int odbcConnect(ODBC_t odbc, const char * uri)
{
    const char * db = NULL;
    urlinfo u = NULL;
    int rc = -1;
int xx;

DBG(0, (stderr, "--> %s(%p,%s)\n", __FUNCTION__, odbc, uri));

    /*XXX FIXME: SQLConnect by DSN should be done here. */

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

DBG(0, (stderr, "\tdb: %s\n", db));
DBG(0, (stderr, "\t u: %s\n", u->user));
DBG(0, (stderr, "\tpw: %s\n", u->password));

#if defined(WITH_UNIXODBC)
assert(odbc->env);
    if (odbc->dbc == NULL) {
	xx = CHECK(odbc, "SQLAllocHandle(DBC)",
		SQLAllocHandle(SQL_HANDLE_DBC, odbc->env, &odbc->dbc));
assert(odbc->dbc);
    }

    rc = CHECK(odbc, "SQLConnect",
		SQLConnect(odbc->dbc,
			(SQLCHAR *) db, SQL_NTS,
			(SQLCHAR *) u->user, SQL_NTS,
			(SQLCHAR *) u->password, SQL_NTS));

	/* XXX FIXME: SQLDriverConnect should print once?. */
    if (rc == 0) {
	SQLCHAR dbms_name[256];
	SQLCHAR dbms_ver[256];
	SQLUINTEGER getdata_support;
	SQLUSMALLINT max_concur_act;

	xx = CHECK(odbc, "SQLGetInfo(DBMS_NAME)",
		SQLGetInfo(odbc->dbc, SQL_DBMS_NAME, (SQLPOINTER)dbms_name,
			sizeof(dbms_name), NULL));
	xx = CHECK(odbc, "SQLGetInfo(DBMS_NAME)",
		SQLGetInfo(odbc->dbc, SQL_DBMS_VER, (SQLPOINTER)dbms_ver,
			sizeof(dbms_ver), NULL));
	xx = CHECK(odbc, "SQLGetInfo(DBMS_NAME)",
		SQLGetInfo(odbc->dbc, SQL_GETDATA_EXTENSIONS, (SQLPOINTER)&getdata_support,
			0, 0));
	xx = CHECK(odbc, "SQLGetInfo(DBMS_NAME)",
		SQLGetInfo(odbc->dbc, SQL_MAX_CONCURRENT_ACTIVITIES, &max_concur_act, 0, 0));

fprintf(stderr, "\tDBMS Name: %s\n", dbms_name);
fprintf(stderr, "\tDBMS Version: %s\n", dbms_ver);
fprintf(stderr, "\tSQL_MAX_CONCURRENT_ACTIVITIES = %u\n", max_concur_act);

	/* XXX FIXME: dig out and print all the bleeping attribute bits. */
fprintf(stderr, "\tSQLGetData - %s\n", ((getdata_support & SQL_GD_ANY_ORDER)
		? "columns can be retrieved in any order"
		: "columns must be retrieved in order\n"));
fprintf(stderr, "\tSQLGetData - %s\n", ((getdata_support & SQL_GD_ANY_COLUMN)
		? "can retrieve columns before last bound one"
		: "columns must be retrieved after last bound one"));

    }
#endif

SPEW(0, rc, odbc);
    db = _free(db);
    return rc;
}

int odbcDisconnect(ODBC_t odbc)
{
    int rc = 0;
int xx;

#if defined(WITH_UNIXODBC)
assert(odbc->env);
assert(odbc->dbc);
    rc = CHECK(odbc, "SQLDisconnect",
		SQLDisconnect(odbc->dbc));
    if (odbc->desc) {
	xx = CHECK(odbc, "SQLFreeHandle(DESC)",
		SQLFreeHandle(SQL_HANDLE_DESC, odbc->desc));
	odbc->desc = NULL;
    }
    if (odbc->stmt) {
	xx = CHECK(odbc, "SQLFreeHandle(STMT)",
		SQLFreeHandle(SQL_HANDLE_STMT, odbc->stmt));
	odbc->stmt = NULL;
    }
    xx = CHECK(odbc, "SQLFreeHandle(DBC)",
		SQLFreeHandle(SQL_HANDLE_DBC, odbc->dbc));
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
/* XXX CHECK */
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
/* XXX CHECK */
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

int odbcNCols(ODBC_t odbc)
{
    int rc = 0;

#if defined(WITH_UNIXODBC)
    SQLSMALLINT columns;

assert(odbc->env);
assert(odbc->dbc);
assert(odbc->stmt);
    rc = CHECK(odbc, "SQLNumResultCols",
	    SQLNumResultCols(odbc->stmt, &columns));
    rc = columns;
#endif

SPEW(0, rc, odbc);
    return rc;
}

int odbcPrint(ODBC_t odbc, void * _fp)
{
    FILE * fp = (_fp ? _fp : stderr);
    int rc = 0;
int xx;

DBG(0, (stderr, "--> %s(%p,%p)\n", __FUNCTION__, odbc, _fp));

#if defined(WITH_UNIXODBC)

assert(odbc->env);
assert(odbc->dbc);
assert(odbc->stmt);

    odbc->ncols = odbcNCols(odbc);

    odbc->nrows = 0;
    /* XXX filter SQL_NO_DATA(100) return. */
    while (SQL_SUCCEEDED((xx = CHECK(odbc, "SQLFetch",
		SQLFetch(odbc->stmt)))))
    {
	int i;

	fprintf(fp, "Row %d\n", ++odbc->nrows);
	for (i = 0; i < odbc->ncols; i++) {
	    SQLLEN got;
	    char b[BUFSIZ];
	    size_t nb = sizeof(b);
	    /* XXX filter -1 return (columns start with 1 not 0). */
	    xx = CHECK(odbc, "SQLGetData",
			SQLGetData(odbc->stmt, i+1, SQL_C_CHAR, b, nb, &got));
	    if (SQL_SUCCEEDED(xx)) {
		if (got == 0) strcpy(b, "NULL");
		fprintf(fp, "  Column %d : %s\n", i+1, b);
	    }
	}
    }
    if (odbc->stmt) {
	xx = CHECK(odbc, "SQLFreeHandle(STMT)",
		SQLFreeHandle(SQL_HANDLE_STMT, odbc->stmt));
	odbc->stmt = NULL;
    }
#endif

SPEW(0, rc, odbc);

    return rc;
}

int odbcTables(ODBC_t odbc)
{
    int rc = 0;
int xx;

#if defined(WITH_UNIXODBC)
assert(odbc->env);
assert(odbc->dbc);
    if (odbc->stmt == NULL) {
	xx = CHECK(odbc, "SQLAllocHandle(STMT)",
		SQLAllocHandle(SQL_HANDLE_STMT, odbc->dbc, &odbc->stmt));
assert(odbc->stmt);
    }

    rc = CHECK(odbc, "SQLTables",
		SQLTables(odbc->stmt, NULL, 0, NULL, 0, NULL, 0,
			(SQLCHAR *) "TABLE", SQL_NTS));
#endif

SPEW(0, rc, odbc);
    return rc;
}

int odbcColumns(ODBC_t odbc)
{
    int rc = 0;
int xx;

#if defined(WITH_UNIXODBC)
assert(odbc->env);
assert(odbc->dbc);
    if (odbc->stmt == NULL) {
	xx = CHECK(odbc, "SQLAllocHandle(STMT)",
		SQLAllocHandle(SQL_HANDLE_STMT, odbc->dbc, &odbc->stmt));
assert(odbc->stmt);
    }

    rc = CHECK(odbc, "SQLTables",
    		SQLColumns(odbc->stmt, NULL, 0, NULL, 0, NULL, 0,
			(SQLCHAR *) "TABLE", SQL_NTS));
#endif

SPEW(0, rc, odbc);
    return rc;
}

int odbcExecDirect(ODBC_t odbc, const char * s, size_t ns)
{
    int rc = 0;
int xx;

DBG(0, (stderr, "--> %s(%p,%s,%u)\n", __FUNCTION__, odbc, s, (unsigned)ns));

    if (ns == 0)
	ns = strlen(s);

#if defined(WITH_UNIXODBC)
assert(odbc->env);
assert(odbc->dbc);
    if (odbc->stmt == NULL) {
	xx = CHECK(odbc, "SQLAllocHandle(STMT)",
		SQLAllocHandle(SQL_HANDLE_STMT, odbc->dbc, &odbc->stmt));
assert(odbc->stmt);
    }

    rc = CHECK(odbc, "SQLExecDirect",
		SQLExecDirect(odbc->stmt, (SQLCHAR *) s, (SQLINTEGER) ns));
#endif

SPEW(0, rc, odbc);
    return rc;
}

int odbcPrepare(ODBC_t odbc, const char * s, size_t ns)
{
    int rc = 0;
int xx;

DBG(0, (stderr, "--> %s(%p,%s,%u)\n", __FUNCTION__, odbc, s, (unsigned)ns));

    if (ns == 0)
	ns = strlen(s);

#if defined(WITH_UNIXODBC)
assert(odbc->env);
assert(odbc->dbc);
    /* XXX FIXME: programmer error */
    if (odbc->stmt) {
	xx = CHECK(odbc, "SQLFreeHandle(STMT)",
		SQLFreeHandle(SQL_HANDLE_STMT, odbc->stmt));
	odbc->stmt = NULL;
    }
assert(odbc->stmt == NULL);
    if (odbc->stmt == NULL) {
	xx = CHECK(odbc, "SQLAllocHandle(STMT)",
		SQLAllocHandle(SQL_HANDLE_STMT, odbc->dbc, &odbc->stmt));
assert(odbc->stmt);
    }

    rc = CHECK(odbc, "SQLPrepare",
		SQLPrepare(odbc->stmt, (SQLCHAR *) s, (SQLINTEGER) ns));
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

    rc = CHECK(odbc, "SQLExecute",
		SQLExecute(odbc->stmt));
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
int xx;
    if (odbc->desc) {
	xx = CHECK(odbc, "SQLFreeHandle(DESC)",
		SQLFreeHandle(SQL_HANDLE_DESC, odbc->desc));
	odbc->desc = NULL;
    }
    if (odbc->stmt) {
	xx = CHECK(odbc, "SQLFreeHandle(STMT)",
		SQLFreeHandle(SQL_HANDLE_STMT, odbc->stmt));
	odbc->stmt = NULL;
    }
    if (odbc->dbc) {
	xx = CHECK(odbc, "SQLFreeHandle(DBC)",
		SQLFreeHandle(SQL_HANDLE_DBC, odbc->dbc));
	odbc->dbc = NULL;
    }
    if (odbc->env) {
	xx = CHECK(odbc, "SQLFreeHandle(ENV)",
		SQLFreeHandle(SQL_HANDLE_ENV, odbc->env));
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
int xx;

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
    xx = CHECK(odbc, "SQLAllocHandle(ENV)",
		SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &odbc->env));
assert(odbc->env);
    xx = CHECK(odbc, "SQLSetEnvAttr",
		SQLSetEnvAttr(odbc->env, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0));
	/* XXX FIXME: SQLDriverConnect should be done here. */
#endif

    return odbcLink(odbc);
}

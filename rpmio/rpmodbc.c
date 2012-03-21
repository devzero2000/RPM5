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

#if !defined(WITH_UNIXODBC)	/* XXX retrofit */
#define SQL_SUCCEEDED(rc) (((rc)&(~1))==0)
#define	SQL_NO_DATA	100
#define SQL_FETCH_NEXT		1
#define SQL_FETCH_FIRST		2
#define SQL_FETCH_LAST		3
#define SQL_FETCH_PRIOR		4
#define SQL_FETCH_ABSOLUTE	5
#define SQL_FETCH_RELATIVE	6
#endif

/*==============================================================*/
#if defined(WITH_UNIXODBC)
static int Xchkodbc(/*@unused@*/ ODBC_t odbc, int ht, const char * msg,
                int error, int printit,
                const char * func, const char * fn, unsigned ln)
{
    int rc = error;

    /* XXX filter SQL_NO_DATA(100) return. */
    if (printit && !SQL_SUCCEEDED(rc) && rc != SQL_NO_DATA) {
	HNDL_t H;
	short ret;
	int i;

        fprintf(stderr, "error: %s:%s:%u: %s(%d):\n",
                func, fn, ln, msg, rc);

	switch (ht) {
	case SQL_HANDLE_ENV:	H = odbc->env;	break;
	case SQL_HANDLE_DBC:	H = odbc->dbc;	break;
	case SQL_HANDLE_STMT:	H = odbc->stmt;	break;
	case SQL_HANDLE_DESC:	H = odbc->desc;	break;
	default:		H = NULL;		break;
	}

assert(H);
	i = 0;
	do {
	    int  native;
	    unsigned char state[7];
	    unsigned char text[256];
	    short len;
	    ret = SQLGetDiagRec(H->ht, H->hp, ++i, state, &native, text,
                            sizeof(text), &len);
	    if (SQL_SUCCEEDED(ret))
		fprintf(stderr, "\t%s:%ld:%ld:%s\n",
				state, (long)i, (long)native, text);
	} while (ret == SQL_SUCCESS);
    }

    return rc;
}

/* XXF FIXME: add logic to set printit */
#define CHECK(_o, _t, _m, _rc)  \
    Xchkodbc(_o, _t, _m, _rc, _odbc_debug, __FUNCTION__, __FILE__, __LINE__)
#else
#define CHECK(_o, _t, _m, _rc)  (-1)
#endif	/* WITH_UNIXODBC */

static void * hFree(ODBC_t odbc, HNDL_t H)
{
    int xx;

    if (H) {
	if (H->hp)
	    xx = CHECK(odbc, H->ht, "SQLFreeHandle",
			SQLFreeHandle(H->ht, H->hp));
	H->ht = 0;
	H->hp = NULL;
	H = _free(H);
    }
    return NULL;
};

static void * hAlloc(ODBC_t odbc, int ht)
{
    HNDL_t PH;
    HNDL_t H = xmalloc(sizeof(*H));
    SQLHANDLE * parent;
    int xx;

    H->ht = ht;
    H->hp = NULL;

    switch (ht) {
    case SQL_HANDLE_ENV:	PH = NULL;		break;
    case SQL_HANDLE_DBC:	PH = odbc->env;		break;
    case SQL_HANDLE_STMT:	PH = odbc->dbc;		break;
    case SQL_HANDLE_DESC:	PH = odbc->dbc;		break;
    default:			PH = NULL;		break;
    }
    parent = (PH ? PH->hp : NULL);

    xx = CHECK(odbc, H->ht, "SQLAllocHandle",
		SQLAllocHandle(H->ht, parent, &H->hp));
assert(H->hp);

    return H;
};

/*==============================================================*/

int odbcConnect(ODBC_t odbc, const char * uri)
{
    const char * db = NULL;
    SQLHANDLE * dbc;
    urlinfo u = NULL;
    int rc = -1;

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

    if (odbc->dbc == NULL)	/* XXX lazy? */
	odbc->dbc = hAlloc(odbc, SQL_HANDLE_DBC);
    dbc = odbc->dbc->hp;

    rc = CHECK(odbc, SQL_HANDLE_DBC, "SQLConnect",
		SQLConnect(dbc,
			(SQLCHAR *) db, SQL_NTS,
			(SQLCHAR *) u->user, SQL_NTS,
			(SQLCHAR *) u->password, SQL_NTS));

	/* XXX FIXME: SQLDriverConnect should print once?. */

    if (rc == 0) {
	unsigned char dbms_name[256] = "UNKNOWN";
	unsigned char dbms_ver[256] = "UNKNOWN";
	unsigned int getdata_support = 0;
	unsigned short max_concur_act = 0;
int xx;

	xx = CHECK(odbc, SQL_HANDLE_DBC, "SQLGetInfo(DBMS_NAME)",
		SQLGetInfo(dbc, SQL_DBMS_NAME, (SQLPOINTER)dbms_name,
			sizeof(dbms_name), NULL));
	xx = CHECK(odbc, SQL_HANDLE_DBC, "SQLGetInfo(DBMS_NAME)",
		SQLGetInfo(dbc, SQL_DBMS_VER, (SQLPOINTER)dbms_ver,
			sizeof(dbms_ver), NULL));
	xx = CHECK(odbc, SQL_HANDLE_DBC, "SQLGetInfo(DBMS_NAME)",
		SQLGetInfo(dbc, SQL_GETDATA_EXTENSIONS, (SQLPOINTER)&getdata_support,
			0, 0));
	xx = CHECK(odbc, SQL_HANDLE_DBC, "SQLGetInfo(DBMS_NAME)",
		SQLGetInfo(dbc, SQL_MAX_CONCURRENT_ACTIVITIES, &max_concur_act, 0, 0));

fprintf(stderr, "\tDBMS Name: %s\n", dbms_name);
fprintf(stderr, "\tDBMS Version: %s\n", dbms_ver);
fprintf(stderr, "\tSQL_MAX_CONCURRENT_ACTIVITIES = %u\n", max_concur_act);

	/* XXX FIXME: dig out and print all the bleeping attribute bits. */
#if !defined(SQL_GD_ANY_ORDER)
#define	SQL_GD_ANY_ORDER	0
#endif
fprintf(stderr, "\tSQLGetData - %s\n", ((getdata_support & SQL_GD_ANY_ORDER)
		? "columns can be retrieved in any order"
		: "columns must be retrieved in order\n"));
#if !defined(SQL_GD_ANY_COLUMN)
#define	SQL_GD_ANY_COLUMN	0
#endif
fprintf(stderr, "\tSQLGetData - %s\n", ((getdata_support & SQL_GD_ANY_COLUMN)
		? "can retrieve columns before last bound one"
		: "columns must be retrieved after last bound one"));

    }

SPEW(0, rc, odbc);
    db = _free(db);
    return rc;
}

int odbcDisconnect(ODBC_t odbc)
{
    SQLHANDLE * dbc = odbc->dbc->hp;
    int rc = 0;
    (void)dbc;

    rc = CHECK(odbc, SQL_HANDLE_DBC, "SQLDisconnect",
		SQLDisconnect(dbc));

    odbc->desc = hFree(odbc, odbc->desc);	/* XXX needed? */
    odbc->stmt = hFree(odbc, odbc->stmt);	/* XXX needed? */
    odbc->dbc = hFree(odbc, odbc->dbc);		/* XXX lazy? */

SPEW(0, rc, odbc);
    return rc;
}

int odbcListDataSources(ODBC_t odbc, void *_fp)
{
    FILE * fp = (_fp ? _fp : stderr);
    SQLHANDLE * env = odbc->env->hp;
    int rc = 0;
    (void)fp;

    unsigned char dsn[256];
    unsigned char desc[256];
    short dsn_ret;
    short desc_ret;
    unsigned short direction = SQL_FETCH_FIRST;
int xx;
    (void)env;
    (void)dsn_ret;
    (void)desc_ret;

    while (SQL_SUCCEEDED((xx = CHECK(odbc, SQL_HANDLE_ENV, "SQLDataSources",
		SQLDataSources(env, direction,
			dsn, sizeof(dsn), &dsn_ret,
			desc, sizeof(desc), &desc_ret)))))
    {
	direction = SQL_FETCH_NEXT;
	fprintf(fp, "%s - %s\n", dsn, desc);
#ifdef	NOTYET
	if (ret == SQL_SUCCESS_WITH_INFO) fprintf(fp, "\tdata truncation\n");
#endif
    }

SPEW(0, rc, odbc);
    return rc;
}

int odbcListDrivers(ODBC_t odbc, void *_fp)
{
    FILE * fp = (_fp ? _fp : stderr);
    int rc = 0;
    (void)fp;

    SQLHANDLE * env = odbc->env->hp;
    unsigned char driver[256];
    unsigned char attr[256];
    short driver_ret;
    short attr_ret;
    unsigned short direction = SQL_FETCH_FIRST;
int xx;
    (void)env;
    (void)driver_ret;
    (void)attr_ret;

    while (SQL_SUCCEEDED((xx = CHECK(odbc, SQL_HANDLE_ENV, "SQLDrivers",
		SQLDrivers(env, direction,
			driver, sizeof(driver), &driver_ret,
			attr, sizeof(attr), &attr_ret)))))
    {
	direction = SQL_FETCH_NEXT;
	fprintf(fp, "%s - %s\n", driver, attr);
#ifdef	NOTYET
	if (xx == SQL_SUCCESS_WITH_INFO) fprintf(fp, "\tdata truncation\n");
#endif
    }

SPEW(0, rc, odbc);
    return rc;
}

int odbcNCols(ODBC_t odbc)
{
    SQLHANDLE * stmt = odbc->stmt->hp;
    short columns = 0;
    int rc = 0;
    (void)stmt;

    rc = CHECK(odbc, SQL_HANDLE_STMT, "SQLNumResultCols",
	    SQLNumResultCols(stmt, &columns));
    rc = columns;

SPEW(0, rc, odbc);
    return rc;
}

int odbcPrint(ODBC_t odbc, void * _fp)
{
    FILE * fp = (_fp ? _fp : stderr);
    SQLHANDLE * stmt = odbc->stmt->hp;
    int rc = 0;
int xx;
    (void)stmt;

DBG(0, (stderr, "--> %s(%p,%p)\n", __FUNCTION__, odbc, fp));


    odbc->ncols = odbcNCols(odbc);
    odbc->nrows = 0;

    /* XXX filter SQL_NO_DATA(100) return. */
    if (odbc->ncols)
    while (SQL_SUCCEEDED((xx = CHECK(odbc, SQL_HANDLE_STMT, "SQLFetch",
		SQLFetch(stmt)))))
    {
	int i;

	fprintf(fp, "Row %d\n", ++odbc->nrows);
	for (i = 0; i < odbc->ncols; i++) {
	    long got;
	    char b[BUFSIZ];
	    size_t nb = sizeof(b);
	    (void)nb;

	    /* XXX filter -1 return (columns start with 1 not 0). */
	    xx = CHECK(odbc, SQL_HANDLE_STMT, "SQLGetData",
			SQLGetData(stmt, i+1, SQL_C_CHAR, b, nb, &got));
	    if (SQL_SUCCEEDED(xx)) {
		if (got == 0) strcpy(b, "NULL");
		fprintf(fp, "  Column %d : %s\n", i+1, b);
	    }
	}
    }

    odbc->nrows = 0;
    odbc->ncols = 0;

    odbc->stmt = hFree(odbc, odbc->stmt);	/* XXX lazy? */

SPEW(0, rc, odbc);

    return rc;
}

int odbcTables(ODBC_t odbc)
{
    SQLHANDLE * stmt;
    int rc = 0;

    if (odbc->stmt == NULL)
	odbc->stmt = hAlloc(odbc, SQL_HANDLE_STMT);
    stmt = odbc->stmt->hp;

    rc = CHECK(odbc, SQL_HANDLE_STMT, "SQLTables",
		SQLTables(stmt, NULL, 0, NULL, 0, NULL, 0,
			(SQLCHAR *) "TABLE", SQL_NTS));

SPEW(0, rc, odbc);
    return rc;
}

int odbcColumns(ODBC_t odbc)
{
    SQLHANDLE * stmt;
    int rc = 0;

    if (odbc->stmt == NULL)
	odbc->stmt = hAlloc(odbc, SQL_HANDLE_STMT);
    stmt = odbc->stmt->hp;

    rc = CHECK(odbc, SQL_HANDLE_STMT, "SQLTables",
    		SQLColumns(stmt, NULL, 0, NULL, 0, NULL, 0,
			(SQLCHAR *) "TABLE", SQL_NTS));

SPEW(0, rc, odbc);
    return rc;
}

int odbcExecDirect(ODBC_t odbc, const char * s, size_t ns)
{
    SQLHANDLE * stmt;
    int rc = 0;

DBG(0, (stderr, "--> %s(%p,%s,%u)\n", __FUNCTION__, odbc, s, (unsigned)ns));

    if (ns == 0)
	ns = strlen(s);

    if (odbc->stmt == NULL)
	odbc->stmt = hAlloc(odbc, SQL_HANDLE_STMT);
    stmt = odbc->stmt->hp;

    rc = CHECK(odbc, SQL_HANDLE_STMT, "SQLExecDirect",
		SQLExecDirect(stmt, (SQLCHAR *) s, (SQLINTEGER) ns));

SPEW(0, rc, odbc);
    return rc;
}

int odbcPrepare(ODBC_t odbc, const char * s, size_t ns)
{
    SQLHANDLE * stmt;
    int rc = 0;

DBG(0, (stderr, "--> %s(%p,%s,%u)\n", __FUNCTION__, odbc, s, (unsigned)ns));

    if (ns == 0)
	ns = strlen(s);

    /* XXX FIXME: programmer error */
    odbc->stmt = hFree(odbc, odbc->stmt);

    if (odbc->stmt == NULL)
	odbc->stmt = hAlloc(odbc, SQL_HANDLE_STMT);
    stmt = odbc->stmt->hp;

    rc = CHECK(odbc, SQL_HANDLE_STMT, "SQLPrepare",
		SQLPrepare(stmt, (SQLCHAR *) s, (SQLINTEGER) ns));

SPEW(0, rc, odbc);
    return rc;
}

int odbcBind(ODBC_t odbc, _PARAM_t param)
{
    SQLHANDLE * stmt = odbc->stmt->hp;
    int rc = CHECK(odbc, SQL_HANDLE_STMT, "SQLBindParam",
		SQLBindParam(stmt,
			param->ParameterNumber,
			param->ValueType,
			param->ParameterType,
			param->LengthPrecision,
			param->ParameterScale,
			param->ParameterValue,
			param->Strlen_or_Ind));
    (void)stmt;

SPEW(0, rc, odbc);
    return rc;
}

int odbcExecute(ODBC_t odbc)
{
    SQLHANDLE * stmt = odbc->stmt->hp;
    int rc = CHECK(odbc, SQL_HANDLE_STMT, "SQLExecute",
		SQLExecute(stmt));
    (void)stmt;

SPEW(0, rc, odbc);
    return rc;
}

/*==============================================================*/

static void odbcFini(void * _odbc)
	/*@globals fileSystem @*/
	/*@modifies *_odbc, fileSystem @*/
{
    ODBC_t odbc = _odbc;

    odbc->desc = hFree(odbc, odbc->desc);
    odbc->stmt = hFree(odbc, odbc->stmt);
    odbc->dbc = hFree(odbc, odbc->dbc);
    odbc->env = hFree(odbc, odbc->env);

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
    SQLHANDLE * env = NULL;
int xx;

    if (fn == NULL)
	fn = _odbc_uri;
    odbc->fn = xstrdup(fn);
    odbc->flags = flags;

    {	const char * dbpath = NULL;
	int ut = urlPath(fn, &dbpath);
	urlinfo u = NULL;
	int xx;
	xx = urlSplit(fn, &u);
assert(ut == URL_IS_MYSQL || ut == URL_IS_POSTGRES);
	odbc->db = rpmExpand(u->scheme, "_", basename((char *)dbpath), NULL);
	odbc->u = urlLink(u, __FUNCTION__);
    }

    odbc->env = hAlloc(odbc, SQL_HANDLE_ENV);
    env = odbc->env->hp;
    xx = CHECK(odbc, SQL_HANDLE_ENV, "SQLSetEnvAttr",
		SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0));
	/* XXX FIXME: SQLDriverConnect should be done here. */

    return odbcLink(odbc);
}

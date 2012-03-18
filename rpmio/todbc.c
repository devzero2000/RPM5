#include "system.h"

#include <sql.h>
#include <sqlext.h>

#include <poptIO.h>

#define	_RPMODBC_INTERNAL
#include <rpmodbc.h>

#include "debug.h"

#define	SPEW(_t, _rc, _odbc)	\
  { if ((_t) || _odbc_debug ) \
	fprintf(stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, (_odbc), \
		(_rc)); \
  }

static char * _odbc_uri = "mysql://luser:jasnl@localhost/test";
static int _odbc_flags = 0;

static char * _odbc_db	= "mysql_test";
static char * _odbc_u	= "luser";
static char * _odbc_pw	= "jasnl";

/*==============================================================*/

static int odbcOpen(ODBC_t odbc)
{
    int rc = -1;
SPEW(0, rc, odbc);
    return rc;
}

static int odbcClose(ODBC_t odbc)
{
    int rc = -1;
SPEW(0, rc, odbc);
    return rc;
}

static int odbcCOpen(ODBC_t odbc)
{
    int rc = -1;
SPEW(0, rc, odbc);
    return rc;
}

static int odbcCClose(ODBC_t odbc)
{
    int rc = -1;
SPEW(0, rc, odbc);
    return rc;
}

static int odbcCDup(ODBC_t odbc)
{
    int rc = -1;
SPEW(0, rc, odbc);
    return rc;
}

static int odbcCDel(ODBC_t odbc)
{
    int rc = -1;
SPEW(0, rc, odbc);
    return rc;
}

static int odbcCGet(ODBC_t odbc)
{
    int rc = -1;
SPEW(0, rc, odbc);
    return rc;
}

static int odbcCPut(ODBC_t odbc)
{
    int rc = -1;
SPEW(0, rc, odbc);
    return rc;
}

static int odbcCCount(ODBC_t odbc)
{
    int rc = -1;
SPEW(0, rc, odbc);
    return rc;
}

/*==============================================================*/

static struct poptOption rpmsvnOptionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL,	&_rpmmg_debug, -1,		NULL, NULL },
  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    poptContext con = rpmioInit(argc, argv, rpmsvnOptionsTable);
    ODBC_t odbc = odbcNew(_odbc_uri, _odbc_flags);
    FILE * _odbc_fp = stderr;
    int rc = 0;
    int xx;

    rc = odbcListDrivers(odbc, _odbc_fp);
    rc = odbcListDataSources(odbc, _odbc_fp);

    rc = odbcConnect(odbc, _odbc_db, _odbc_u, _odbc_pw);

    xx = odbcColumns(odbc);
    odbc->ncols = odbcNCols(odbc);
    odbc->nrows = 0;
    while ((xx = odbcFetch(odbc)) != SQL_NO_DATA) {
	int i;

	fprintf(stdout, "Row %d\n", odbc->nrows++);
	for (i = 0; i <= odbc->ncols; i++) {
	    SQLRETURN ret;
	    SQLLEN got;
	    char b[512];
	    size_t nb = sizeof(b);
	    ret = SQLGetData(odbc->stmt, i, SQL_C_CHAR, b, nb, &got);
	    if (SQL_SUCCEEDED(ret)) {
		if (got == 0) strcpy(b, "NULL");
		fprintf(stdout, "  Column %d : %s\n", i, b);
	    }
	}
    }

    rc = odbcDisconnect(odbc);

    xx = odbcOpen(odbc);
    xx = odbcCOpen(odbc);

    xx = odbcCDup(odbc);
    xx = odbcCDel(odbc);
    xx = odbcCGet(odbc);
    xx = odbcCPut(odbc);
    xx = odbcCCount(odbc);

    xx = odbcCClose(odbc);
    xx = odbcClose(odbc);

    odbc = odbcFree(odbc);

    con = rpmioFini(con);
/*@i@*/ urlFreeCache();

    return rc;
}

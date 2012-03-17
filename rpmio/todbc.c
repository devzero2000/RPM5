#include "system.h"

#include <sql.h>
#include <sqlext.h>

#include <poptIO.h>

#define	_RPMODBC_INTERNAL
#include <rpmodbc.h>

#include "debug.h"

static char * _odbc_db	= "mysql_test";
static char * _odbc_u	= "luser";
static char * _odbc_pw	= "jasnl";

static struct poptOption rpmsvnOptionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL,	&_rpmmg_debug, -1,		NULL, NULL },
  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    poptContext con = rpmioInit(argc, argv, rpmsvnOptionsTable);
    rpmodbc odbc;
    int rc = 0;
    int xx;

_rpmodbc_debug = -1;

    odbc = rpmodbcNew(NULL, 0);

    rc = rpmodbcListDrivers(odbc, NULL);
    rc = rpmodbcListDataSources(odbc, NULL);

    rc = rpmodbcConnect(odbc, _odbc_db, _odbc_u, _odbc_pw);

    xx = rpmodbcColumns(odbc);
    odbc->ncols = rpmodbcNCols(odbc);
    odbc->nrows = 0;
    while ((xx = rpmodbcFetch(odbc)) != SQL_NO_DATA) {
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

    rc = rpmodbcDisconnect(odbc);

    odbc = rpmodbcFree(odbc);

    con = rpmioFini(con);
/*@i@*/ urlFreeCache();

    return rc;
}

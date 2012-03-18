#include "system.h"

#include <sql.h>
#include <sqlext.h>

#include <poptIO.h>

#define	_RPMODBC_INTERNAL
#include <rpmodbc.h>

#include "debug.h"

static char * _odbc_uri = "mysql://luser:jasnl@localhost/test";
static int _odbc_flags = 0;

#define	SPEW(_t, _rc, _odbc)	\
  { if ((_t) || _odbc_debug ) \
	fprintf(stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, (_odbc), \
		(_rc)); \
  }
/*==============================================================*/
static int Xchkodbc(/*@unused@*/ ODBC_t odbc, const char * msg,
                int error, int printit,
                const char * func, const char * fn, unsigned ln)
{
    int rc = error;

    if (printit && rc) {
#define odbc_strerror(_e)	""	/* XXX odbc_strerror? */
        rpmlog(RPMLOG_ERR, "%s:%s:%u: %s(%d): %s\n",
                func, fn, ln, msg, rc, odbc_strerror(rc));
    }

    return rc;
}
#define CHECK(_odbc, _msg, _error)  \
    Xchkodbc(_odbc, _msg, _error, _odbc_debug, __FUNCTION__, __FILE__, __LINE__)

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
static const char *const _odbc_stmts[] = {
    "SHOW DATABASES;",
    "USE test;",
    "SHOW TABLES;",
    "DROP TABLE Packages;",
"CREATE TABLE Packages (\n\
    i INTEGER PRIMARY KEY NOT NULL,\n\
    Nvra VARCHAR(64)\n\
);",
    "DESCRIBE Packages;",
    "INSERT INTO Packages VALUES( 1, 'Bing' );",
    "INSERT INTO Packages VALUES( 2, 'Bang' );",
    "INSERT INTO Packages VALUES( 3, 'Boom' );",
    "SELECT * from Packages;",
    "DROP TABLE Packages;",
    NULL
};

static int odbcRun(ODBC_t odbc, const char *const stmts[], void * _fp)
{
    FILE * fp = (_fp ? _fp : stderr);
    const char * s;
    int rc = -1;
    int i;

    for (i = 0; (s = stmts[i]) != NULL; i++) {
fprintf(fp, "==> %s\n", s);
	rc = odbcPrepare(odbc, s, 0);
	rc = odbcExecute(odbc);
	rc = odbcPrint(odbc, fp);
    }

SPEW(0, rc, odbc);
    return rc;
}

/*==============================================================*/

static struct poptOption odbcOptionsTable[] = {
 { NULL, 'f', POPT_ARG_INT,	&_odbc_flags, -1,
 	NULL, NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"),
	NULL },

  POPT_AUTOALIAS
  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    poptContext con = rpmioInit(argc, argv, odbcOptionsTable);
    char ** av = (char **) poptGetArgs(con);
    int ac = argvCount((ARGV_t)av);
    const char * _uri = (ac > 1 ? av[1] : _odbc_uri);
    ODBC_t odbc = odbcNew(_uri, _odbc_flags);
    FILE * _odbc_fp = stderr;
    int rc = 0;
    int xx;

fprintf(_odbc_fp, "==> %s\n", "ListDrivers");
    rc = odbcListDrivers(odbc, _odbc_fp);
fprintf(_odbc_fp, "==> %s\n", "ListDataSources");
    rc = odbcListDataSources(odbc, _odbc_fp);

fprintf(_odbc_fp, "==> %s\n", "Connect");
    rc = odbcConnect(odbc, _uri);

fprintf(_odbc_fp, "==> %s\n", "Tables");
    xx = odbcTables(odbc);
    xx = odbcPrint(odbc, _odbc_fp);

fprintf(_odbc_fp, "==> %s\n", "Columns");
    xx = odbcColumns(odbc);
    xx = odbcPrint(odbc, _odbc_fp);

fprintf(_odbc_fp, "==> %s\n", "ExecDirect");
    xx = odbcExecDirect(odbc, "SHOW DATABASES;", 0);
    xx = odbcPrint(odbc, _odbc_fp);

    xx = odbcRun(odbc, _odbc_stmts, _odbc_fp);

fprintf(_odbc_fp, "<== %s\n", "Disconnect");
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

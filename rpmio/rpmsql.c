#include "system.h"

#include <popt.h>

#define	_RPMSQL_INTERNAL
#include <rpmsql.h>
#include <rpmmacro.h>

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

static void rpmsqlArgCallback(poptContext con,
			      /*@unused@ */ enum poptCallbackReason reason,
			      const struct poptOption *opt,
			      const char *arg,
			      /*@unused@ */ void *_data)
	/*@ */
{
    rpmsql sql = &_sql;

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
    case 'S':				/*    -separator x */
assert(arg != NULL);
	sqlite3_snprintf(sizeof(sql->separator), sql->separator,
			     "%.*s", (int) sizeof(sql->separator) - 1, arg);
	break;
    case 'N':				/*    -nullvalue text */
assert(arg != NULL);
	sqlite3_snprintf(sizeof(sql->nullvalue), sql->nullvalue,
			     "%.*s", (int) sizeof(sql->nullvalue) - 1, arg);
	break;
    case 'V':				/*    -version */
	printf("%s\n", sqlite3_libversion());
	/*@-exitarg@ */ exit(0); /*@=exitarg@ */
	/*@notreached@ */ break;
    default:
	fprintf(stderr, _("%s: Unknown callback(0x%x)\n"),
		    __FUNCTION__, (unsigned) opt->val);
	poptPrintUsage(con, stderr, 0);
	/*@-exitarg@ */ exit(2); /*@=exitarg@ */
	/*@notreached@ */ break;
    }
}

struct poptOption _rpmsqlOptions[] = {
    /*@-type@*//* FIX: cast? */
    {NULL, '\0',
     POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
     rpmsqlArgCallback, 0, NULL, NULL},
/*@=type@*/

 { "debug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_ONEDASH|POPT_ARGFLAG_DOC_HIDDEN, &_rpmsql_debug, -1,
        N_("Debug embedded SQL interpreter"), NULL},

 { "init", '\0', POPT_ARG_STRING|POPT_ARGFLAG_ONEDASH,	&_sql.zInitFile, 0,
	N_("read/process named FILE"), N_("FILE") },
 { "echo", '\0', POPT_BIT_SET|POPT_ARGFLAG_ONEDASH,	&_sql.flags, RPMSQL_FLAGS_ECHO,
	N_("print commands before execution"), NULL },

 { "header", '\0', POPT_BIT_SET|POPT_ARGFLAG_TOGGLE|POPT_ARGFLAG_ONEDASH, &_sql.flags, RPMSQL_FLAGS_SHOWHDR,
	N_("turn headers on or off"), NULL },

 { "bail", '\0', POPT_BIT_SET|POPT_ARGFLAG_ONEDASH,	&_sql.flags, RPMSQL_FLAGS_BAIL,
	N_("stop after hitting an error"), NULL },

 { "interactive", '\0', POPT_BIT_SET|POPT_ARGFLAG_TOGGLE|POPT_ARGFLAG_ONEDASH,	&_sql.flags, RPMSQL_FLAGS_INTERACTIVE,
	N_("force interactive I/O"), NULL },
 { "batch", '\0', POPT_BIT_CLR|POPT_ARGFLAG_TOGGLE|POPT_ARGFLAG_ONEDASH, &_sql.flags, RPMSQL_FLAGS_INTERACTIVE,
	N_("force batch I/O"), NULL },

 { "column", '\0', POPT_ARG_VAL|POPT_ARGFLAG_ONEDASH,	&_sql.mode, RPMSQL_MODE_COLUMN,
	N_("set output mode to 'column'"), NULL },
 { "csv", '\0', POPT_ARG_VAL|POPT_ARGFLAG_ONEDASH,	&_sql.mode, RPMSQL_MODE_CSV,
	N_("set output mode to 'csv'"), NULL },
 { "html", '\0', POPT_ARG_VAL|POPT_ARGFLAG_ONEDASH,	&_sql.mode, RPMSQL_MODE_HTML,
	N_("set output mode to HTML"), NULL },
 { "line", '\0', POPT_ARG_VAL|POPT_ARGFLAG_ONEDASH,	&_sql.mode, RPMSQL_MODE_LINE,
	N_("set output mode to 'line'"), NULL },
 { "list", '\0', POPT_ARG_VAL|POPT_ARGFLAG_ONEDASH,	&_sql.mode, RPMSQL_MODE_LIST,
	N_("set output mode to 'list'"), NULL },
 { "separator", '\0', POPT_ARG_STRING|POPT_ARGFLAG_ONEDASH,	0, 'S',
	N_("set output field separator (|)"), N_("CHAR") },
 { "nullvalue", '\0', POPT_ARG_STRING|POPT_ARGFLAG_ONEDASH,	0, 'N',
	N_("set text string for NULL values"), N_("TEXT") },

 { "version", '\0', POPT_ARG_NONE|POPT_ARGFLAG_ONEDASH,	0, 'V',
	N_("show SQLite version"), NULL},

#ifdef	NOTYET
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"), NULL},
#endif

    POPT_AUTOHELP {NULL, (char) -1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
	N_("\
Usage: dbsql [OPTIONS] FILENAME [SQL]\n\
FILENAME is the name of an SQLite database. A new database is created\n\
if the file does not previously exist.\n\
\n\
OPTIONS include:\n\
   -help                show this message\n\
   -init filename       read/process named file\n\
   -echo                print commands before execution\n\
   -[no]header          turn headers on or off\n\
   -bail                stop after hitting an error\n\
   -interactive         force interactive I/O\n\
   -batch               force batch I/O\n\
   -column              set output mode to 'column'\n\
   -csv                 set output mode to 'csv'\n\
   -html                set output mode to HTML\n\
   -line                set output mode to 'line'\n\
   -list                set output mode to 'list'\n\
   -separator 'x'       set output field separator (|)\n\
   -nullvalue 'text'    set text string for NULL values\n\
   -version             show SQLite version\n\
"), NULL},

    POPT_TABLEEND
};

/*==============================================================*/

static void rpmsqlFini(void * _sql)
	/*@globals fileSystem @*/
	/*@modifies *_sql, fileSystem @*/
{
    rpmsql sql = _sql;

SQLDBG((stderr, "==> %s(%p)\n", __FUNCTION__, sql));

    /* XXX INTERACTIVE cruft. */
    sql->zHome = _free(sql->zHome);
    sql->zInitrc = _free(sql->zInitrc);
    sql->zHistory = _free(sql->zHistory);
    sql->zPrompt = _free(sql->zPrompt);
    sql->zContinue = _free(sql->zContinue);

    sql->zInitFile = _free(sql->zInitFile);
    sql->av = argvFree(sql->av);
#if defined(WITH_SQLITE)
    if (sql->I) {
	sqlite3 * db = (sqlite3 *)sql->I;
	switch (sqlite3_close(db)) {
	default:
	    fprintf(stderr, "Error: cannot close database \"%s\"\n",
                    sqlite3_errmsg(db));
	    break;
	case SQLITE_OK:
	    break;
	}
    }
#endif
    sql->I = NULL;
    (void) rpmiobFree(sql->iob);
    sql->iob = NULL;
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

const char ** rpmsqlArgv(rpmsql sql, int * argcp)
{
    const char ** av = sql->av;

    if (argcp)
	*argcp = argvCount(av);
    return av;
}

static void rpmsqlInitPopt(rpmsql sql, int ac, char ** av, poptOption tbl)
	/*@modifies sql @*/
{
    void *use =  sql->_item.use;
    void *pool = sql->_item.pool;
    poptContext con;
    int rc;

SQLDBG((stderr, "==> %s(%p, %p[%u], %p)\n", __FUNCTION__, sql, av, (unsigned)ac, tbl));
    if (av == NULL || av[0] == NULL || av[1] == NULL)
	goto exit;

    con = poptGetContext(av[0], ac, (const char **)av, tbl, 0);

    /* Process all options into _sql, whine if unknown options. */
    while ((rc = poptGetNextOpt(con)) > 0) {
	const char * arg = poptGetOptArg(con);
	arg = _free(arg);
	switch (rc) {
	default:
		fprintf(stderr, _("%s: option table misconfigured (%d)\n"),
                __FUNCTION__, rc);
		break;
	}
    }
    /* XXX FIXME: arrange error return iff rc < -1. */
if (rc != 0)
fprintf(stderr, "%s: poptGetNextOpt rc(%d): %s\n", __FUNCTION__, rc, poptStrerror(rc));

    *sql = _sql;	/* structure assignment */

    sql->_item.use = use;
    sql->_item.pool = pool;
    rc = argvAppend(&sql->av, poptGetArgs(con));

    con = poptFreeContext(con);

exit:
SQLDBG((stderr, "<== %s(%p, %p[%u], %p)\n", __FUNCTION__, sql, av, (unsigned)ac, tbl));
}

rpmsql rpmsqlNew(char ** av, uint32_t flags)
{
    rpmsql sql = rpmsqlGetPool(_rpmsqlPool);
    int ac = argvCount((ARGV_t)av);

SQLDBG((stderr, "==> %s(%p[%u], 0x%x)\n", __FUNCTION__, av, (unsigned)ac, flags));

#if defined(WITH_SQLITE)
    if (av && av[1]) {
	sqlite3 * db = NULL;
	int xx;

	/* Initialize defaults for popt parsing. */
	memset(&_sql, 0, sizeof(_sql));
	_sql.flags = flags;
	_sql.mode = RPMSQL_MODE_LIST;
	memcpy(_sql.separator, "|", 2);
	rpmsqlInitPopt(sql, ac, av, (poptOption) _rpmsqlOptions);

	/* The 1st argument is the database to open (or :memory: default). */
	if (sql->av && sql->av[0]) {
	    sql->zDbFilename = xstrdup(sql->av[0]);	/* XXX strdup? */
	    /* If database alread exists, open immediately. */
	    if (Access(sql->zDbFilename, R_OK)) {
		xx = sqlite3_open(sql->zDbFilename, &db);
		sql->I = (void *) db;
	    }
	} else
	    sql->zDbFilename = xstrdup(":memory:");

#ifdef	NOTYET
	/* Read ~/.sqliterc (if specified), then reparse options. */
	if (sql->zInitFile) {
	    db = (sqlite3 *)sql->I;
	    xx = rpmsqlInitRC(sql, sql->zInitFIle);
	    sql->I = NULL;	/* XXX avoid the db close */
	    rpmsqlFini(sql);
	    rpmsqlInitPopt(sql, ac, av, (poptOption) _rpmsqlOptions);
	    sql->I = (void *)db;
	}
#endif

    }

    { /* XXX INTERACTIVE cruft. */
	static const char _zInitrc[]	= "/.sqliterc";
	static const char _zHistory[]	= "/.sqlite_history";
	static const char _zPrompt[]	= "dbsql> ";
	static const char _zContinue[]	= "  ...> ";
	/* XXX getpwuid? */
	sql->zHome = xstrdup(getenv("HOME"));
	sql->zInitrc = rpmGetPath(sql->zHome, _zInitrc, NULL);
	sql->zHistory = rpmGetPath(sql->zHome, _zHistory, NULL);
	/*
	 ** Prompt strings. Initialized in main. Settable with
	 **   .prompt main continue
	 */
	sql->zPrompt = xstrdup(_zPrompt);
	sql->zContinue = xstrdup(_zContinue);

	/* Determine whether stdio or rpmiob should be used for output */
	sql->out = (F_ISSET(sql, INTERACTIVE) ? stdout : NULL);
    }

#endif

    sql->iob = rpmiobNew(0);

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

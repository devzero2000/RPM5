#include "system.h"

#include <popt.h>

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
    {NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
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
	sqlite3 * db = NULL;
	int xx;
	xx = sqlite3_open(av[1], &db);
	sql->I = (void *) db;
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

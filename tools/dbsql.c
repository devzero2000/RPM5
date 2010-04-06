/*
** 2001 September 15
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This file contains code to implement the "sqlite" command line
** utility for accessing SQLite databases.
*/

/*
** Include the configuration header output by 'configure' if we're using the
** autoconf-based build
*/

#include "system.h"

#include <stdarg.h>

#define	_RPMSQL_INTERNAL
#include <rpmsql.h>

/*
** Build options detected by SQLite's configure script but not normally part
** of config.h.  Accept what configure detected unless it was overridden on the
** command line.
*/
#ifndef HAVE_EDITLINE
#define HAVE_EDITLINE 0
#endif
#if !HAVE_EDITLINE
#undef HAVE_EDITLINE
#endif

#ifndef HAVE_READLINE
#define HAVE_READLINE 0
#endif
#if !HAVE_READLINE
#undef HAVE_READLINE
#endif

#ifndef SQLITE_OS_UNIX
#define SQLITE_OS_UNIX 1
#endif
#if !SQLITE_OS_UNIX
#undef SQLITE_OS_UNIX
#endif

#ifndef SQLITE_OS_WIN
#define SQLITE_OS_WIN 0
#endif
#if !SQLITE_OS_WIN
#undef SQLITE_OS_WIN
#endif

#ifndef SQLITE_THREADSAFE
#define SQLITE_THREADSAFE 1
#endif
#if !SQLITE_THREADSAVE
#undef SQLITE_THREADSAVE
#endif

#ifndef SQLITE_THREAD_OVERRIDE_LOCK
#define SQLITE_THREAD_OVERRIDE_LOCK -1
#endif
#if !SQLITE_THREAD_OVERRIDE_LOCK
#undef SQLITE_THREAD_OVERRIDE_LOCK
#endif

#ifndef SQLITE_TEMP_STORE
#define SQLITE_TEMP_STORE 1
#endif
#if !SQLITE_THREAD_OVERRIDE_LOCK
#undef SQLITE_THREAD_OVERRIDE_LOCK
#endif

#include "sqlite3.h"

#if defined(HAVE_EDITLINE) && HAVE_EDITLINE==1
#include <editline/readline.h>
#elif defined(HAVE_READLINE) && HAVE_READLINE==1
# include <readline/readline.h>
# include <readline/history.h>
#else
# define readline(p) local_getline(p,stdin)
# define add_history(X)
# define read_history(X)
# define write_history(X)
# define stifle_history(X)
#endif

#include "debug.h"

/*==============================================================*/

#ifdef SIGINT
/*
** This routine runs when the user presses Ctrl-C
*/
static void interrupt_handler(int NotUsed)
{
    NotUsed = NotUsed;
    _rpmsqlSeenInterrupt = 1;
    if (_rpmsqlI && _rpmsqlI->I) {
	sqlite3 * db = (sqlite3 *) _rpmsqlI->I;
	sqlite3_interrupt(db);
    }
}
#endif

int main(int argc, char **argv)
{
    int _flags = (isatty(0) ? RPMSQL_FLAGS_INTERACTIVE : 0);
    rpmsql sql = rpmsqlNew(argv, _flags | 0x80000000);
    char *zErrMsg = NULL;
    char *zFirstCmd = NULL;
    const char ** av = NULL;
    int ac;
    int ec = 1;		/* assume error */

    /* Make sure we have a valid signal handler early, before anything
     ** else is done.
     */
#ifdef SIGINT
    signal(SIGINT, interrupt_handler);
#endif

    /* Do an initial pass through the command-line argument to locate
     ** the name of the database file, the name of the initialization file,
     ** and the first command to execute.
     */
    av = rpmsqlArgv(sql, &ac);
    if (ac > 2) {
	fprintf(stderr, "%s: Error: too many options: \"%s ...\"\n", __progname,
		av[2]);
	goto exit;
    }
    if (ac > 1)
	zFirstCmd = xstrdup(av[1]);	/* XXX strdup? */

    if (sql->out == NULL)	/* XXX needed? */
	sql->out = stdout;
    sql->iob = rpmiobFree(sql->iob);

    /* XXX refactored from popt callback */
    if (sql->mode == RPMSQL_MODE_CSV)
	memcpy(sql->separator, ",", 2);

    if (zFirstCmd) {
	/* Run just the command that follows the database name
	 */
	if (zFirstCmd[0] == '.') {
	    ec = rpmsqlMetaCommand(sql, zFirstCmd);
	    goto exit;
	} else {
	    _rpmsqlOpenDB(sql);
	    ec = _rpmsqlShellExec(sql, zFirstCmd, _rpmsqlShellCallback, &zErrMsg);
	    if (zErrMsg != 0) {
		fprintf(stderr, "Error: %s\n", zErrMsg);
		if (ec == 0) ec = 1;
		goto exit;
	    } else if (ec != 0) {
		fprintf(stderr, "Error: unable to process SQL \"%s\"\n",
			zFirstCmd);
		goto exit;
	    }
	}
    } else {
	/* Run commands received from standard input
	 */
	if (F_ISSET(sql, INTERACTIVE)) {
	    extern char *db_full_version(int *, int *, int *, int *,
					 int *);
	    printf("%s\n" "Enter \".help\" for instructions\n"
		   "Enter SQL statements terminated with a \";\"\n",
		   db_full_version(NULL, NULL, NULL, NULL, NULL)
		);
#if defined(HAVE_READLINE) && HAVE_READLINE==1
	    if (sql->zHistory)
		read_history(sql->zHistory);
#endif
	    ec = rpmsqlInput(sql, NULL);
	    if (sql->zHistory) {
		stifle_history(100);
		write_history(sql->zHistory);
	    }
	} else {
	    ec = (int) rpmsqlRun(NULL, "", NULL);
	}
    }

exit:

    zFirstCmd = _free(zFirstCmd);

    sql = rpmsqlFree(sql);
    rpmioClean();	/* XXX 0x8000000 global newref */

    return ec;
}

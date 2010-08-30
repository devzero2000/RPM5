/** \ingroup rpmio
 * \file rpmio/rpmrepo.c
 */

#include "system.h"

#if defined(WITH_SQLITE)
#include <sqlite3.h>
#ifdef	__LCLINT__
/*@-incondefs -redecl @*/
extern const char *sqlite3_errmsg(sqlite3 *db)
	/*@*/;
extern int sqlite3_open(
  const char *filename,		   /* Database filename (UTF-8) */
  /*@out@*/ sqlite3 **ppDb	   /* OUT: SQLite db handle */
)
	/*@modifies *ppDb @*/;
extern int sqlite3_exec(
  sqlite3 *db,			   /* An open database */
  const char *sql,		   /* SQL to be evaluted */
  int (*callback)(void*,int,char**,char**),  /* Callback function */
  void *,			   /* 1st argument to callback */
  /*@out@*/ char **errmsg	   /* Error msg written here */
)
	/*@modifies db, *errmsg @*/;
extern int sqlite3_prepare(
  sqlite3 *db,			   /* Database handle */
  const char *zSql,		   /* SQL statement, UTF-8 encoded */
  int nByte,			   /* Maximum length of zSql in bytes. */
  /*@out@*/ sqlite3_stmt **ppStmt, /* OUT: Statement handle */
  /*@out@*/ const char **pzTail	   /* OUT: Pointer to unused portion of zSql */
)
	/*@modifies *ppStmt, *pzTail @*/;
extern int sqlite3_reset(sqlite3_stmt *pStmt)
	/*@modifies pStmt @*/;
extern int sqlite3_step(sqlite3_stmt *pStmt)
	/*@modifies pStmt @*/;
extern int sqlite3_finalize(/*@only@*/ sqlite3_stmt *pStmt)
	/*@modifies pStmt @*/;
extern int sqlite3_close(sqlite3 * db)
	/*@modifies db @*/;
/*@=incondefs =redecl @*/
#endif	/* __LCLINT__ */
#endif	/* WITH_SQLITE */

#include <rpmiotypes.h>
#include <rpmio.h>	/* for *Pool methods */
#include <rpmlog.h>
#include <rpmurl.h>

#define	_RPMREPO_INTERNAL
#include <rpmrepo.h>

#include "debug.h"

/*@unchecked@*/
int _rpmrepo_debug = 0;

static void rpmrepoFini(void * _repo)
	/*@globals fileSystem @*/
	/*@modifies *_repo, fileSystem @*/
{
    rpmrepo repo = _repo;

    repo->fn = _free(repo->fn);
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmrepoPool = NULL;

static rpmrepo rpmrepoGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmrepoPool, fileSystem @*/
	/*@modifies pool, _rpmrepoPool, fileSystem @*/
{
    rpmrepo repo;

    if (_rpmrepoPool == NULL) {
	_rpmrepoPool = rpmioNewPool("repo", sizeof(*repo), -1, _rpmrepo_debug,
			NULL, NULL, rpmrepoFini);
	pool = _rpmrepoPool;
    }
    repo = (rpmrepo) rpmioGetPool(pool, sizeof(*repo));
    memset(((char *)repo)+sizeof(repo->_item), 0, sizeof(*repo)-sizeof(repo->_item));
    return repo;
}

rpmrepo rpmrepoNew(const char * fn, int flags)
{
    rpmrepo repo = rpmrepoGetPool(_rpmrepoPool);

    if (fn)
	repo->fn = xstrdup(fn);

    return rpmrepoLink(repo);
}

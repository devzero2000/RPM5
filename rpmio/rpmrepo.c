/** \ingroup rpmio
 * \file rpmio/rpmrepo.c
 */

#include "system.h"

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
    int xx;

    if (fn)
	repo->fn = xstrdup(fn);

    return rpmrepoLink(repo);
}

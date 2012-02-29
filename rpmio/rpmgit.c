/** \ingroup rpmio
 * \file rpmio/rpmgit.c
 */

#include "system.h"

#include <rpmiotypes.h>
#include <rpmio.h>	/* for *Pool methods */
#include <rpmlog.h>
#include <rpmurl.h>

#if defined(HAVE_GIT2_H)
#include <git2.h>
#endif

#define	_RPMGIT_INTERNAL
#include <rpmgit.h>

#include "debug.h"

/*@unchecked@*/
int _rpmgit_debug = 0;

static void rpmgitFini(void * _git)
	/*@globals fileSystem @*/
	/*@modifies *_git, fileSystem @*/
{
    rpmgit git = _git;

    if (git->repo) {
#if defined(WITH_LIBGIT2)
	git_repository_free(git->repo);
#endif
	git->repo = NULL;
    }
    git->fn = _free(git->fn);
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmgitPool = NULL;

static rpmgit rpmgitGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmgitPool, fileSystem @*/
	/*@modifies pool, _rpmgitPool, fileSystem @*/
{
    rpmgit git;

    if (_rpmgitPool == NULL) {
	_rpmgitPool = rpmioNewPool("git", sizeof(*git), -1, _rpmgit_debug,
			NULL, NULL, rpmgitFini);
	pool = _rpmgitPool;
    }
    git = (rpmgit) rpmioGetPool(pool, sizeof(*git));
    memset(((char *)git)+sizeof(git->_item), 0, sizeof(*git)-sizeof(git->_item));
    return git;
}

rpmgit rpmgitNew(const char * fn, int flags)
{
    static const char _gitfn[] = "/var/tmp/rpm/.git";
    rpmgit git = rpmgitGetPool(_rpmgitPool);
    int xx;

    if (fn == NULL)
	fn = _gitfn;

    if (fn)
	git->fn = xstrdup(fn);

#if defined(WITH_LIBGIT2)
    git_repository_open(&git->repo, fn);
#endif

    return rpmgitLink(git);
}

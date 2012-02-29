/** \ingroup rpmio
 * \file rpmio/rpmsvn.c
 */

#include "system.h"

#include <rpmiotypes.h>
#include <rpmio.h>	/* for *Pool methods */
#include <rpmlog.h>
#include <rpmurl.h>

#if defined(WITH_SUBVERSION)
#include "svn_pools.h"
#include "svn_client.h"
#include "svn_repos.h"
#include "svn_subst.h"
#endif

#define _RPMSVN_INTERNAL
#include <rpmsvn.h>

#include "debug.h"

/*@unchecked@*/
int _rpmsvn_debug = 0;

static void rpmsvnFini(void * _svn)
	/*@globals fileSystem @*/
	/*@modifies *_svn, fileSystem @*/
{
    rpmsvn svn = _svn;

#if defined(WITH_SUBVERSION)
#endif

    svn->fn = _free(svn->fn);

}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmsvnPool = NULL;

static rpmsvn rpmsvnGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmsvnPool, fileSystem @*/
	/*@modifies pool, _rpmsvnPool, fileSystem @*/
{
    rpmsvn svn;

    if (_rpmsvnPool == NULL) {
	_rpmsvnPool = rpmioNewPool("svn", sizeof(*svn), -1, _rpmsvn_debug,
			NULL, NULL, rpmsvnFini);
	pool = _rpmsvnPool;
    }
    svn = (rpmsvn) rpmioGetPool(pool, sizeof(*svn));
    memset(((char *)svn)+sizeof(svn->_item), 0, sizeof(*svn)-sizeof(svn->_item));
    return svn;
}

rpmsvn rpmsvnNew(const char * fn, int flags)
{
    rpmsvn svn = rpmsvnGetPool(_rpmsvnPool);
    int xx;

    if (fn)
	svn->fn = xstrdup(fn);

#if defined(WITH_SUBVERSION)
#endif

    return rpmsvnLink(svn);
}

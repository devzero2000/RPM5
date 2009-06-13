/** \ingroup rpmio
 * \file rpmio/rpmaug.c
 */

#include "system.h"

#if defined(WITH_AUGEAS) && defined(HAVE_AUGEAS_H)
#include "augeas.h"
#endif

#include <rpmiotypes.h>
#include <rpmio.h>	/* for *Pool methods */
#include <rpmlog.h>
#define	_RPMAUG_INTERNAL
#include <rpmaug.h>

#include "debug.h"

/*@unchecked@*/
int _rpmaug_debug = 0;

/*@-mustmod@*/	/* XXX splint on crack */
static void rpmaugFini(void * _aug)
	/*@globals fileSystem @*/
	/*@modifies *_aug, fileSystem @*/
{
    rpmaug aug = _aug;

#if defined(WITH_AUGEAS)
    (void) aug_close(aug->I);
    aug->I = NULL;
#endif
    aug->root = _free(aug->root);
    aug->loadpath = _free(aug->loadpath);
}
/*@=mustmod@*/

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmaugPool = NULL;

static rpmaug rpmaugGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmaugPool, fileSystem @*/
	/*@modifies pool, _rpmaugPool, fileSystem @*/
{
    rpmaug aug;

    if (_rpmaugPool == NULL) {
	_rpmaugPool = rpmioNewPool("aug", sizeof(*aug), -1, _rpmaug_debug,
			NULL, NULL, rpmaugFini);
	pool = _rpmaugPool;
    }
    return (rpmaug) rpmioGetPool(pool, sizeof(*aug));
}

/*@unchecked@*/
static const char _root[] = "/";
/*@unchecked@*/
static const char _loadpath[] = "";	/* XXX /usr/lib/rpm/lib? */

rpmaug rpmaugNew(const char * root, const char * loadpath, unsigned int flags)
{
    rpmaug aug = rpmaugGetPool(_rpmaugPool);
    int xx;

    if (root == NULL)
	root = _root;
    if (loadpath == NULL)
	loadpath = _loadpath;
    aug->root = xstrdup(root);
    aug->loadpath = xstrdup(loadpath);
    aug->flags = flags;
#if defined(WITH_AUGEAS)
    aug->I = (void *) aug_init(aug->root, aug->loadpath, aug->flags);
assert(aug->I != NULL);
#endif

    return rpmaugLink(aug);
}

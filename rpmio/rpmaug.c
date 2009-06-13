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

int rpmaugDefvar(rpmaug aug, const char * name, const char * expr)
{
    int rc = -1;
#ifdef	WITH_AUGEAS
    rc = aug_defvar(aug->I, name, expr);
#endif
    return rc;
}

int rpmaugDefnode(rpmaug aug, const char * name, const char * expr,
		const char * value, int * created)
{
    int rc = -1;
#ifdef	WITH_AUGEAS
    rc = aug_defnode(aug->I, name, expr, value, created);
#endif
    return rc;
}

int rpmaugGet(rpmaug aug, const char * path, const char ** value)
{
    int rc = -1;
#ifdef	WITH_AUGEAS
    rc = aug_get(aug->I, path, value);
#endif
    return rc;
}

int rpmaugSet(rpmaug aug, const char * path, const char * value)
{
    int rc = -1;
#ifdef	WITH_AUGEAS
    rc = aug_set(aug->I, path, value);
#endif
    return rc;
}

int rpmaugInsert(rpmaug aug, const char * path, const char * label, int before)
{
    int rc = -1;
#ifdef	WITH_AUGEAS
    rc = aug_insert(aug->I, path, label, before);
#endif
    return rc;
}

int rpmaugRm(rpmaug aug, const char * path)
{
    int rc = -1;
#ifdef	WITH_AUGEAS
    rc = aug_rm(aug->I, path);
#endif
    return rc;
}

int rpmaugMv(rpmaug aug, const char * src, const char * dst)
{
    int rc = -1;
#ifdef	WITH_AUGEAS
    rc = aug_mv(aug->I, src, dst);
#endif
    return rc;
}

int rpmaugMatch(rpmaug aug, const char * path, char *** matches)
{
    int rc = -1;
#ifdef	WITH_AUGEAS
    rc = aug_match(aug->I, path, matches);
#endif
    return rc;
}

int rpmaugSave(rpmaug aug)
{
    int rc = -1;
#ifdef	WITH_AUGEAS
    rc = aug_save(aug->I);
#endif
    return rc;
}

int rpmaugLoad(rpmaug aug)
{
    int rc = -1;
#ifdef	WITH_AUGEAS
    rc = aug_load(aug->I);
#endif
    return rc;
}

int rpmaugPrint(rpmaug aug, FILE *out, const char * path)
{
    int rc = -1;
#ifdef	WITH_AUGEAS
    rc = aug_print(aug->I, out, path);
#endif
    return rc;
}

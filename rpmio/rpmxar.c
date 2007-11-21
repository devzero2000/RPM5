#include "system.h"
#include "xar.h"

#include <rpmio.h>

#define	_RPMXAR_INTERNAL
#include <rpmxar.h>

#include "debug.h"

/*@access FD_t @*/

/*@unchecked@*/
int _xar_debug = 0;

#if defined(WITH_XAR)

rpmxar XrpmxarUnlink(rpmxar xar, const char * msg, const char * fn, unsigned ln)
{
    if (xar == NULL) return NULL;
/*@-modfilesys@*/
if (_xar_debug && msg != NULL)
fprintf(stderr, "--> xar %p -- %d %s at %s:%u\n", xar, xar->nrefs, msg, fn, ln);
/*@=modfilesys@*/
    xar->nrefs--;
    return NULL;
}

rpmxar XrpmxarLink(rpmxar xar, const char * msg, const char * fn, unsigned ln)
{
    if (xar == NULL) return NULL;
    xar->nrefs++;

/*@-modfilesys@*/
if (_xar_debug && msg != NULL)
fprintf(stderr, "--> xar %p ++ %d %s at %s:%u\n", xar, xar->nrefs, msg, fn, ln);
/*@=modfilesys@*/

    /*@-refcounttrans@*/ return xar; /*@=refcounttrans@*/
}

rpmxar rpmxarFree(rpmxar xar)
{
    if (xar) {
	int xx;

/*@-onlytrans@*/
	if (xar->nrefs > 1)
	    return rpmxarUnlink(xar, "rpmxarFree");

	if (xar->i) {
/*@-noeffectuncon@*/
	    xar_iter_free(xar->i);
/*@=noeffectuncon@*/
	    xar->i = NULL;
	}
	if (xar->x) {
/*@-noeffectuncon@*/
	    xx = xar_close(xar->x);
/*@=noeffectuncon@*/
	    xar->x = NULL;
	}

	(void) rpmxarUnlink(xar, "rpmxarFree");
/*@=onlytrans@*/
	/*@-refcounttrans -usereleased@*/
	memset(xar, 0, sizeof(*xar));         /* XXX trash and burn */
	xar = _free(xar);
	/*@=refcounttrans =usereleased@*/
    }
    return NULL;
}

rpmxar rpmxarNew(const char * fn, const char * fmode)
{
    rpmxar xar = xcalloc(1, sizeof(*xar));
    int flags = ((fmode && *fmode == 'w') ? WRITE : READ);

assert(fn != NULL);
/*@-moduncon@*/
    xar->x = xar_open(fn, flags);
    if (flags == READ) {
	xar->i = xar_iter_new();
	xar->first = 1;
    }
/*@=moduncon@*/
    return rpmxarLink(xar, "rpmxarNew");
}

int rpmxarNext(rpmxar xar)
{
if (_xar_debug)
fprintf(stderr, "*** rpmxarNext(%p) first %d\n", xar, xar->first);
/*@-moduncon@*/
    if (xar->first) {
	xar->f = xar_file_first(xar->x, xar->i);
	xar->first = 0;
    } else
	xar->f = xar_file_next(xar->i);
/*@=moduncon@*/

    return (xar->f == NULL ? 1 : 0);
}

int rpmxarPush(rpmxar xar, const char * fn, void * b, size_t nb)
{
    if (xar->x && b != NULL && nb > 0) {
/*@-moduncon@*/
	xar->f = xar_add_frombuffer(xar->x, NULL, fn, b, nb);
/*@=moduncon@*/
	if (xar->f == NULL)
	    return 2;
    }
    return 0;
}

int rpmxarPull(rpmxar xar, const char * fn, void * bp, size_t * nbp)
{
/*@-moduncon@*/
    const char * path = xar_get_path(xar->f);
/*@=moduncon@*/
    char * b = NULL;
    size_t nb = 0;
    int rc;

    if (fn != NULL && strcmp(fn, path)) {
	path = _free(path);
	return 1;
    }

/*@-moduncon@*/
    rc = (int) xar_extract_tobuffersz(xar->x, xar->f, &b, &nb);
/*@=moduncon@*/
if (_xar_debug)
fprintf(stderr, "*** %s %p[%lu] rc %d\n", path, b, (unsigned long)nb, rc);
    path = _free(path);
    if (rc || b == NULL || nb == 0)
	return 1;

    if (bp)
	*((char **)bp) = b;
    if (nbp)
	*nbp = nb;

    return 0;
}
#endif

#include "system.h"

#define	_RPMXAR_INTERNAL
#include <rpmio_internal.h>

#include <rpmxar.h>

#include "debug.h"

/*@access FD_t @*/

/*@unchecked@*/
int _xar_debug = 0;

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

/*@-onlytrans@*/
	if (xar->nrefs > 1)
	    return rpmxarUnlink(xar, "rpmxarFree");

#if defined(WITH_XAR)
	if (xar->i) {
/*@-noeffectuncon@*/
	    xar_iter_free(xar->i);
/*@=noeffectuncon@*/
	    xar->i = NULL;
	}
	if (xar->x) {
	    int xx;
/*@-noeffectuncon@*/
	    xx = xar_close(xar->x);
/*@=noeffectuncon@*/
	    xar->x = NULL;
	}
#endif	/* WITH_XAR */

	xar->member = _free(xar->member);
	xar->b = _free(xar->b);

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
#if defined(WITH_XAR)
    int flags = ((fmode && *fmode == 'w') ? WRITE : READ);
#endif	/* WITH_XAR */

assert(fn != NULL);
#if defined(WITH_XAR)
/*@-moduncon@*/
    xar->x = xar_open(fn, flags);
    if (flags == READ) {
	xar->i = xar_iter_new();
	xar->first = 1;
    }
/*@=moduncon@*/
#endif	/* WITH_XAR */
    return rpmxarLink(xar, "rpmxarNew");
}

int rpmxarNext(rpmxar xar)
{
/*@-modfilesys@*/
if (_xar_debug)
fprintf(stderr, "--> rpmxarNext(%p) first %d\n", xar, xar->first);
/*@=modfilesys@*/

#if defined(WITH_XAR)
/*@-moduncon@*/
    if (xar->first) {
	xar->f = xar_file_first(xar->x, xar->i);
	xar->first = 0;
    } else
	xar->f = xar_file_next(xar->i);
/*@=moduncon@*/
#endif	/* WITH_XAR */

    return (xar->f == NULL ? 1 : 0);
}

int rpmxarPush(rpmxar xar, const char * fn, char * b, size_t bsize)
{
/*@-modfilesys@*/
if (_xar_debug)
fprintf(stderr, "--> rpmxarPush(%p, %s) %p[%u]\n", xar, fn, b, (unsigned)bsize);
/*@=modfilesys@*/

    if (xar->x && b != NULL) {
#if defined(WITH_XAR)
/*@-moduncon@*/
	xar->f = xar_add_frombuffer(xar->x, NULL, fn, b, bsize);
/*@=moduncon@*/
#endif
	if (xar->f == NULL)
	    return 2;
    }
    return 0;
}

int rpmxarPull(rpmxar xar, const char * fn)
{
#if defined(WITH_XAR)
/*@-moduncon@*/
    const char * path = xar_get_path(xar->f);
/*@=moduncon@*/
#else
    const char * path = xstrdup("*No XAR*");
#endif	/* WITH_XAR */
    int rc = 1;

    if (fn != NULL && strcmp(fn, path)) {
	path = _free(path);
	return rc;
    }
    xar->member = _free(xar->member);
    xar->member = path;

    xar->b = _free(xar->b);
    xar->bsize = xar->bx = 0;

#if defined(WITH_XAR)
/*@-moduncon -nullstate @*/
    rc = (int) xar_extract_tobuffersz(xar->x, xar->f, &xar->b, &xar->bsize);
/*@=moduncon =nullstate @*/
#endif	/* WITH_XAR */

/*@-modfilesys@*/
if (_xar_debug)
fprintf(stderr, "--> rpmxarPull(%p, %s) %p[%u] rc %d\n", xar, fn, xar->b, (unsigned)xar->bsize, rc);
/*@=modfilesys@*/

    if (rc)
	return 1;

    return 0;
}

int rpmxarSwapBuf(rpmxar xar, char * b, size_t bsize,
		char ** obp, size_t * obsizep)
{
/*@-modfilesys@*/
if (_xar_debug)
fprintf(stderr, "*** rpmxarSwapBuf(%p, %p[%u], %p, %p) %p[%u]\n", xar, b, (unsigned) bsize, obp, obsizep, xar->b, (unsigned) xar->bsize);
/*@=modfilesys@*/

    if (xar) {
	if (obsizep != NULL)
	    *obsizep = xar->bsize;
	if (obp != NULL) {
/*@-onlytrans@*/
	    *obp = xar->b;
/*@=onlytrans@*/
	    xar->b = NULL;
	}
	xar->b = _free(xar->b);
/*@-assignexpose -temptrans @*/
	xar->b = b;
/*@=assignexpose =temptrans @*/
	xar->bsize = bsize;
    }
/*@-nullstate@*/
    return 0;
/*@=nullstate@*/
}

ssize_t xarRead(void * cookie, /*@out@*/ char * buf, size_t count)
{
    FD_t fd = cookie;
    rpmxar xar = fdGetXAR(fd);
    ssize_t rc = 0;

assert(xar != NULL);
#if 0
    if ((xx = rpmxarNext(xar)) != 0)    return RPMRC_FAIL;
    if ((xx = rpmxarPull(xar, "Signature")) != 0) return RPMRC_FAIL;
    (void) rpmxarSwapBuf(xar, NULL, 0, &b, &nb);
#endif

    rc = xar->bsize - xar->bx;
    if (rc > 0) {
	if (count < rc) rc = count;
assert(xar->b != NULL);
	memmove(buf, &xar->b[xar->bx], rc);
	xar->bx += rc;
    } else
    if (rc < 0) {
	rc = -1;
    } else
	rc = 0;

if (_xar_debug)
fprintf(stderr, "*** xarRead(%p,%p,0x%x) %s %p[%u:%u] rc 0x%x\n", cookie, buf, (unsigned)count, (xar->member ? xar->member : "(nil)"), xar->b, (unsigned)xar->bx, (unsigned)xar->bsize, (unsigned)rc);

    return rc;
}

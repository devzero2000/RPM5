#include "system.h"
#include "xar.h"

#define	_RPMXAR_INTERNAL
#include <rpmio_internal.h>

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

int rpmxarPush(rpmxar xar, const char * fn)
{
    if (xar->x && xar->b != NULL) {
/*@-moduncon@*/
	xar->f = xar_add_frombuffer(xar->x, NULL, fn, xar->b, xar->bsize);
/*@=moduncon@*/
	if (xar->f == NULL)
	    return 2;
    }
    return 0;
}

int rpmxarPull(rpmxar xar, const char * fn)
{
/*@-moduncon@*/
    const char * path = xar_get_path(xar->f);
/*@=moduncon@*/
    int rc;

    if (fn != NULL && strcmp(fn, path)) {
	path = _free(path);
	return 1;
    }
    xar->member = _free(xar->member);
    xar->member = path;

    xar->b = _free(xar->b);
    xar->bsize = xar->bx = 0;

/*@-moduncon@*/
    rc = (int) xar_extract_tobuffersz(xar->x, xar->f, &xar->b, &xar->bsize);
/*@=moduncon@*/
if (_xar_debug)
fprintf(stderr, "*** %s %p[%lu] rc %d\n", xar->member, xar->b, (unsigned long)xar->bsize, rc);
    if (rc)
	return 1;

    return 0;
}

int rpmxarSwapBuf(rpmxar xar, char * b, size_t bsize,
		char ** obp, size_t * obsizep)
{
if (_xar_debug)
fprintf(stderr, "*** rpmxarSwapBuf(%p, %p[%u], %p, %p) %p[%u]\n", xar, b, (unsigned) bsize, obp, obsizep, xar->b, (unsigned) xar->bsize);
    if (xar) {
	if (obsizep != NULL)
	    *obsizep = xar->bsize;
	if (obp != NULL) {
	    *obp = xar->b;
	    xar->b = NULL;
	}
	xar->b = _free(xar->b);
	xar->b = b;
	xar->bsize = bsize;
    }
    return 0;
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
	memmove(buf, &xar->b[xar->bx], rc);
	xar->bx += rc;
    } else
    if (rc < 0) {
	rc = -1;
    } else
	rc = 0;

if (_xar_debug)
fprintf(stderr, "*** xarRead(%p,%p,0x%x) %s %p[%u:%u] rc 0x%x\n", cookie, buf, (unsigned)count, xar->member, xar->b, (unsigned)xar->bx, (unsigned)xar->bsize, (unsigned)rc);

    return rc;
}
#endif

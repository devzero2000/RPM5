#include "system.h"

/* XXX Get rid of the pugly #ifdef's */
#if defined(WITH_XAR) && defined(HAVE_XAR_H)

#include "xar.h"

#if defined(__LCLINT__)
/*@-incondefs -redecl@*/
/*@null@*/
xar_t xar_open(const char *file, int32_t flags)
	/*@*/;
int xar_close(/*@only@*/ xar_t x)
	/*@globals fileSystem @*/
	/*@modifies x, fileSystem @*/;
/*@null@*/
xar_iter_t xar_iter_new(void)
	/*@*/;
/*@null@*/
xar_file_t xar_file_first(xar_t x, xar_iter_t i)
	/*@modifies x, i @*/;
/*@null@*/
xar_file_t xar_file_next(xar_iter_t i)
	/*@modifies i @*/;
/*@null@*/
xar_file_t xar_add_frombuffer(xar_t x, /*@null@*/ xar_file_t parent,
		const char *name, char *buffer, size_t length)
	/*@globals fileSystem @*/
	/*@modifies x, fileSystem @*/;
int32_t xar_extract_tobuffersz(xar_t x, xar_file_t f,
		char **buffer, size_t *size)
	/*@globals fileSystem @*/
	/*@modifies x, f, *buffer, *size @*/;
/*@only@*/
char *xar_get_path(xar_file_t f)
	/*@*/;
/*@=incondefs =redecl@*/

#endif	/* __LCLINT__ */

#else	/* WITH_XAR */
#define	READ	0
#define	WRITE	1
#define	xar_open(_fn, _f)	(NULL)
#define	xar_close(_x)	(1)
#define	xar_iter_new()		(NULL)
#define	xar_iter_free(_i)
#define	xar_file_first(_x, _i)	(NULL)
#define	xar_file_next(_i)	(NULL)
#define	xar_add_frombuffer(_x, _parent, _fn, _b, _bsize)	(NULL)
#define	xar_extract_tobuffersz(_x, _f, _b, _bsize)	(1)
#define	xar_get_path(_f)	"*No XAR*"
#define xar_opt_set(_a1, _a2, _a3) (1)
#define XAR_OPT_COMPRESSION 0
#define XAR_OPT_VAL_NONE 0
#define XAR_OPT_VAL_GZIP 0
#endif	/* WITH_XAR */

#define	_RPMXAR_INTERNAL
#include <rpmxar.h>
#include <rpmio_internal.h>	/* for fdGetXAR */

#include "debug.h"

/*@access FD_t @*/

/*@unchecked@*/
int _xar_debug = 0;

/*@unchecked@*/ /*@null@*/
rpmioPool _xarPool;

static rpmxar rpmxarGetPool(/*@null@*/ rpmioPool pool)
	/*@modifies pool @*/
{
    rpmxar xar;

    if (_xarPool == NULL) {
	_xarPool = rpmioNewPool("xar", sizeof(*xar), -1, _xar_debug,
			NULL, NULL, NULL);
	pool = _xarPool;
    }
    return (rpmxar) rpmioGetPool(pool, sizeof(*xar));
}
rpmxar rpmxarFree(rpmxar xar)
{
    if (xar == NULL)
	return NULL;
    yarnPossess(xar->use);
/*@-modfilesys@*/
if (_xar_debug)
fprintf(stderr, "--> xar %p -- %ld %s at %s:%u\n", xar, yarnPeekLock(xar->use), "rpmxarFree", __FILE__, __LINE__);
/*@=modfilesys@*/
    if (yarnPeekLock(xar->use) <= 1L) {
/*@-onlytrans@*/
	if (xar->i) {
	    xar_iter_free(xar->i);
	    xar->i = NULL;
	}
	if (xar->x) {
	    int xx;
	    xx = xar_close(xar->x);
	    xar->x = NULL;
	}

	xar->member = _free(xar->member);
	xar->b = _free(xar->b);

/*@=onlytrans@*/
	xar = (rpmxar) rpmioPutPool((rpmioItem)xar);
    } else
	yarnTwist(xar->use, BY, -1);
    return NULL;
}

rpmxar rpmxarNew(const char * fn, const char * fmode)
{
    rpmxar xar = rpmxarGetPool(_xarPool);
    int flags = ((fmode && *fmode == 'w') ? WRITE : READ);

assert(fn != NULL);
    xar->x = xar_open(fn, flags);
    if (flags == READ) {
	xar->i = xar_iter_new();
	xar->first = 1;
    }
    return rpmxarLink(xar, "rpmxarNew");
}

int rpmxarNext(rpmxar xar)
{
if (_xar_debug)
fprintf(stderr, "--> rpmxarNext(%p) first %d\n", xar, xar->first);

    if (xar->first) {
	xar->f = xar_file_first(xar->x, xar->i);
	xar->first = 0;
    } else
	xar->f = xar_file_next(xar->i);

    return (xar->f == NULL ? 1 : 0);
}

int rpmxarPush(rpmxar xar, const char * fn, unsigned char * b, size_t bsize)
{
    int payload = !strcmp(fn, "Payload");

/*@+charint@*/
if (_xar_debug)
fprintf(stderr, "--> rpmxarPush(%p, %s) %p[%u] %02x%02x%02x%02x%02x%02x%02x%02x\n", xar, fn, b, (unsigned)bsize, b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
/*@=charint@*/

    if (xar->x && b != NULL) {
	if (payload) /* payload is already compressed */
	    (void) xar_opt_set(xar->x, XAR_OPT_COMPRESSION, XAR_OPT_VAL_NONE);
	xar->f = xar_add_frombuffer(xar->x, NULL, fn, (char *)b, bsize);
	if (payload) /* restore default xar compression */
	    (void) xar_opt_set(xar->x, XAR_OPT_COMPRESSION, XAR_OPT_VAL_GZIP);
	if (xar->f == NULL)
	    return 2;
    }
    return 0;
}

int rpmxarPull(rpmxar xar, const char * fn)
{
    const char * path = xar_get_path(xar->f);
    int rc = 1;

    if (fn != NULL && strcmp(fn, path)) {
	path = _free(path);
	return rc;
    }
    xar->member = _free(xar->member);
    xar->member = path;

    xar->b = _free(xar->b);
    xar->bsize = xar->bx = 0;

/*@-nullstate @*/
    rc = (int) xar_extract_tobuffersz(xar->x, xar->f, (char **)&xar->b, &xar->bsize);
/*@=nullstate @*/
    if (rc)
	return 1;

/*@+charint -nullpass -nullderef @*/
if (_xar_debug) {
unsigned char * b = xar->b;
size_t bsize = xar->bsize;
fprintf(stderr, "--> rpmxarPull(%p, %s) %p[%u] %02x%02x%02x%02x%02x%02x%02x%02x\n", xar, fn, b, (unsigned)bsize, b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
}
/*@=charint =nullpass =nullderef @*/

    return 0;
}

int rpmxarSwapBuf(rpmxar xar, unsigned char * b, size_t bsize,
		unsigned char ** obp, size_t * obsizep)
{
if (_xar_debug)
fprintf(stderr, "--> rpmxarSwapBuf(%p, %p[%u], %p, %p) %p[%u]\n", xar, b, (unsigned) bsize, obp, obsizep, xar->b, (unsigned) xar->bsize);

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
	if (count < (size_t)rc) rc = count;
assert(xar->b != NULL);
	memmove(buf, &xar->b[xar->bx], rc);
	xar->bx += rc;
    } else
    if (rc < 0) {
	rc = -1;
    } else
	rc = 0;

if (_xar_debug)
fprintf(stderr, "--> xarRead(%p,%p,0x%x) %s %p[%u:%u] rc 0x%x\n", cookie, buf, (unsigned)count, (xar->member ? xar->member : "(nil)"), xar->b, (unsigned)xar->bx, (unsigned)xar->bsize, (unsigned)rc);

    return rc;
}

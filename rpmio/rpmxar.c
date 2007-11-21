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
int rpmxarFini(rpmxar xar)
{
    int xx;

if (_xar_debug)
fprintf(stderr, "*** rpmxarFini(%p)\n", xar);
/*@-moduncon@*/
    if (xar->i) {
/*@-noeffectuncon@*/
	xar_iter_free(xar->i);
/*@=noeffectuncon@*/
	xar->i = NULL;
    }
    if (xar->x) {
	xx = xar_close(xar->x);
	xar->x = NULL;
    }
    return 0;
/*@=moduncon@*/
}

int rpmxarInit(rpmxar xar, const char * fn, const char * fmode)
{
    int flags = ((fmode && *fmode == 'w') ? WRITE : READ);

if (_xar_debug)
fprintf(stderr, "*** rpmxarInit(%p, %s, %s)\n", xar, fn, fmode);
assert(fn != NULL);
    
/*@-moduncon@*/
    xar->x = xar_open(fn, flags);
    if (flags == READ) {
	xar->i = xar_iter_new();
	xar->first = 1;
    }
    return 0;
/*@=moduncon@*/
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
    int xx;

    if (fn == NULL)
	fn = path;
    else
	assert(!strcmp(fn, path));

/*@-moduncon@*/
    xx = (int) xar_extract_tobuffersz(xar->x, xar->f, &b, &nb);
/*@=moduncon@*/
if (_xar_debug)
fprintf(stderr, "*** xx %d %p[%lu]\n", xx, b, (unsigned long)nb);
    if (xx || b == NULL || nb == 0) {
	path = _free(path);
	return 1;
    }

if (_xar_debug)
fprintf(stderr, "*** %s %p[%lu]\n", path, b, (unsigned long)nb);

    if (bp)
	*((char **)bp) = b;
    if (nbp)
	*nbp = nb;

    path = _free(path);
    return 0;
}
#endif

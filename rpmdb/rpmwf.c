
#define	RPM2XAR
#include "system.h"
#ifdef WITH_XAR
#include "xar.h"
#endif
#include <rpmio.h>
#include <rpmlib.h>

#define	_RPMWF_INTERNAL
#include <rpmwf.h>

#include "debug.h"

int _rpmwf_debug = 0;

rpmRC rpmwfFiniXAR(rpmwf wf)
{
if (_rpmwf_debug)
fprintf(stderr, "*** rpmwfFiniXAR(%p)\n", wf);
#ifdef WITH_XAR
    if (wf->i) {
	xar_iter_free(wf->i);
	wf->i = NULL;
    }
    if (wf->x) {
	xar_close(wf->x);
	wf->x = NULL;
    }
    return RPMRC_OK;
#else
    return RPMRC_FAIL;
#endif
}

rpmRC rpmwfInitXAR(rpmwf wf, const char * fn, const char * fmode)
{
#ifdef WITH_XAR
    int flags = ((fmode && *fmode == 'w') ? WRITE : READ);
#endif

if (_rpmwf_debug)
fprintf(stderr, "*** rpmwfInitXAR(%p, %s, %s)\n", wf, fn, fmode);
    if (fn == NULL)
	fn = wf->fn;
assert(fn);
    
#ifdef WITH_XAR
    wf->x = xar_open(fn, flags);
    if (flags == READ) {
	wf->i = xar_iter_new();
	wf->first = 1;
    }
    return RPMRC_OK;
#else
    return RPMRC_FAIL;
#endif
}

rpmRC rpmwfNextXAR(rpmwf wf)
{
if (_rpmwf_debug)
fprintf(stderr, "*** rpmwfNextXAR(%p) first %d\n", wf, wf->first);
#ifdef WITH_XAR
    if (wf->first) {
	wf->f = xar_file_first(wf->x, wf->i);
	wf->first = 0;
    } else
	wf->f = xar_file_next(wf->i);
#endif

    if (wf->f == NULL)
	return RPMRC_NOTFOUND;
    return RPMRC_OK;
}

rpmRC rpmwfPushXAR(rpmwf wf, const char * fn)
{
    char * b = NULL;
    size_t nb = 0;

    if (!strcmp(fn, "Lead")) {
	b = wf->l;
	nb = wf->nl;
    } else
    if (!strcmp(fn, "Signature")) {
	b = wf->s;
	nb = wf->ns;
    } else
    if (!strcmp(fn, "Header")) {
	b = wf->h;
	nb = wf->nh;
    } else
    if (!strcmp(fn, "Payload")) {
	b = wf->p;
	nb = wf->np;
    }

    if (wf->x && b && nb > 0) {
#ifdef WITH_XAR
	wf->f = xar_add_frombuffer(wf->x, NULL, fn, b, nb);
	if (wf->f == NULL)
#endif
	    return RPMRC_FAIL;
    }
    return RPMRC_OK;
}

rpmRC rpmwfPullXAR(rpmwf wf, const char * fn)
{
    rpmRC rc = RPMRC_OK;
#ifdef WITH_XAR
    const char * path = xar_get_path(wf->f);
#else
    const char * path = "*** WITHOUT_XAR ***";
#endif
    char * b = NULL;
    size_t nb = 0;
    int xx = 1;		/* assume failure */

#ifdef	NOTYET
    const char * name = NULL;
    const char * type = NULL;

    xx = xar_prop_get(wf->f, "name", &name);
if (_rpmwf_debug)
fprintf(stderr, "*** xx %d name %s\n", xx, name);
    if (xx || name == NULL)
	return RPMRC_NOTFOUND;

    xx = xar_prop_get(wf->f, "type", &type);
if (_rpmwf_debug)
fprintf(stderr, "*** xx %d type %s\n", xx, type);
    if (xx || type == NULL || strcmp(type, "file"))
	return RPMRC_NOTFOUND;
#endif

    if (fn == NULL)
	fn = path;
    else
	assert(!strcmp(fn, path));

#ifdef WITH_XAR
    xx = xar_extract_tobuffersz(wf->x, wf->f, &b, &nb);
#endif
if (_rpmwf_debug)
fprintf(stderr, "*** xx %d %p[%lu]\n", xx, b, (unsigned long)nb);
    if (xx || b == NULL || nb == 0) {
	path = _free(path);
	return RPMRC_NOTFOUND;
    }

if (_rpmwf_debug)
fprintf(stderr, "*** %s %p[%lu]\n", path, b, (unsigned long)nb);
    if (!strcmp(fn, "Lead")) {
	wf->l = b;
	wf->nl = nb;
    } else
    if (!strcmp(fn, "Signature")) {
	wf->s = b;
	wf->ns = nb;
    } else
    if (!strcmp(fn, "Header")) {
	wf->h = b;
	wf->nh = nb;
    } else
    if (!strcmp(fn, "Payload")) {
	wf->p = b;
	wf->np = nb;
    } else
	rc = RPMRC_NOTFOUND;

    path = _free(path);
    return rc;
}

rpmRC rpmwfFiniRPM(rpmwf wf)
{
    int xx;

    if (wf->b && wf->b != (void *)-1) {
	xx = munmap(wf->b, wf->nb);
	wf->b = NULL;
    }
    if (wf->fd) {
	(void) Fclose(wf->fd);
	wf->fd = NULL;
    }
    return RPMRC_OK;
}

static size_t hSize(uint_32 *p)
{
    return (8 + 8 + 16 * ntohl(p[2]) + ntohl(p[3]));
}

rpmRC rpmwfInitRPM(rpmwf wf, const char * fn, const char * fmode)
{
if (_rpmwf_debug)
fprintf(stderr, "*** rpmwfInitRPM(%p, %s, %s)\n", wf, fn, fmode);
    if (fn == NULL)
	fn = wf->fn;
assert(fn);

    wf->fd = Fopen(fn, fmode);
if (_rpmwf_debug)
fprintf(stderr, "*** Fopen(%s, %s) fd %p nb %u\n", fn, fmode, wf->fd, (unsigned)wf->nb);
    if (wf->fd == NULL || Ferror(wf->fd)) {
	(void) rpmwfFiniRPM(wf);
	return RPMRC_NOTFOUND;
    }

    if (fmode && *fmode == 'r') {
	wf->b = mmap(NULL, wf->nb, PROT_READ, MAP_SHARED, Fileno(wf->fd), 0L);

	if (wf->b == (void *)-1) {
	    wf->b = NULL;
	    (void) rpmwfFiniRPM(wf);
	    return RPMRC_NOTFOUND;
	}

	wf->l = wf->b;
	wf->nl = 96;

	wf->s = wf->l + wf->nl;
	wf->ns = hSize((void *)wf->s);
	wf->ns += (8 - (wf->ns % 8));	/* padding */

	wf->h = wf->s + wf->ns;
	wf->nh = hSize((void *)wf->h);

	wf->p = wf->h + wf->nh;
	wf->np = wf->nb;
	wf->np -= wf->nl + wf->ns + wf->nh;
    }

    return RPMRC_OK;
}

rpmRC rpmwfPushRPM(rpmwf wf, const char * fn)
{
    char * b = NULL;
    size_t nb = 0;

    if (!strcmp(fn, "Lead")) {
	b = wf->l;
	nb = wf->nl;
    } else
    if (!strcmp(fn, "Signature")) {
	b = wf->s;
	nb = wf->ns;
    } else
    if (!strcmp(fn, "Header")) {
	b = wf->h;
	nb = wf->nh;
    } else
    if (!strcmp(fn, "Payload")) {
	b = wf->p;
	nb = wf->np;
    }

    if (!(b && nb > 0))
	return RPMRC_NOTFOUND;

    if (Fwrite(b, 1, nb, wf->fd) != nb)
	return RPMRC_FAIL;

    return RPMRC_OK;
}

rpmwf rpmwfFree(rpmwf wf)
{
if (_rpmwf_debug)
fprintf(stderr, "*** rpmwfFree(%p)\n", wf);
    if (wf) {

	if (wf->b == NULL) {
	    wf->l = _free(wf->l);
	    wf->s = _free(wf->s);
	    wf->h = _free(wf->h);
	    wf->p = _free(wf->p);
	}

	(void) rpmwfFiniXAR(wf);
	(void) rpmwfFiniRPM(wf);

	wf->fn = _free(wf->fn);
	wf = _free(wf);
    }
    return NULL;
}

rpmwf rpmwfNew(const char * fn)
{
    struct stat sb, *st = &sb;
    rpmwf wf;
    int xx;

    if ((xx = Stat(fn, st)) < 0)
	return NULL;
    wf = xcalloc(1, sizeof(*wf));
    wf->fn = xstrdup(fn);
    wf->nb = st->st_size;

if (_rpmwf_debug)
fprintf(stderr, "*** rpmwfNew(%s) wf %p nb %u\n", wf->fn, wf, (unsigned)wf->nb);
    return wf;
}

rpmwf rdRPM(const char * rpmfn)
{
    rpmwf wf;
    rpmRC rc;

    if ((wf = rpmwfNew(rpmfn)) == NULL)
	return wf;

    if ((rc = rpmwfInitRPM(wf, NULL, "r")) != RPMRC_OK) {
	wf = rpmwfFree(wf);
	return NULL;
    }

if (_rpmwf_debug)
fprintf(stderr, "*** rdRPM(%s) wf %p\n", rpmfn, wf);

    return wf;
}

rpmwf rdXAR(const char * xarfn)
{
    rpmwf wf;
    rpmRC rc;

    if ((wf = rpmwfNew(xarfn)) == NULL)
	return wf;

    if ((rc = rpmwfInitXAR(wf, NULL, "r")) != RPMRC_OK) {
	wf = rpmwfFree(wf);
	return NULL;
    }
if (_rpmwf_debug)
fprintf(stderr, "*** rdXAR(%s) wf %p\n", xarfn, wf);

    while ((rc = rpmwfNextXAR(wf)) == RPMRC_OK) {
	rc = rpmwfPullXAR(wf, NULL);
    }

    (void) rpmwfFiniXAR(wf);
    return wf;
}

rpmRC wrXAR(const char * xarfn, rpmwf wf)
{
    rpmRC rc;

    if ((rc = rpmwfInitXAR(wf, xarfn, "w")) != RPMRC_OK)
	goto exit;
if (_rpmwf_debug)
fprintf(stderr, "*** wrXAR(%s, %p)\n", xarfn, wf);

    if ((rc = rpmwfPushXAR(wf, "Lead")) != RPMRC_OK)
	goto exit;
    if ((rc = rpmwfPushXAR(wf, "Signature")) != RPMRC_OK)
	goto exit;
    if ((rc = rpmwfPushXAR(wf, "Header")) != RPMRC_OK)
	goto exit;
    if ((rc = rpmwfPushXAR(wf, "Payload")) != RPMRC_OK)
	goto exit;

exit:
    (void) rpmwfFiniXAR(wf);

    return rc;
}

rpmRC wrRPM(const char * rpmfn, rpmwf wf)
{
    rpmRC rc;

    if ((rc = rpmwfInitRPM(wf, rpmfn, "w")) != RPMRC_OK)
	goto exit;
if (_rpmwf_debug)
fprintf(stderr, "*** wrRPM(%s, %p)\n", rpmfn, wf);

    if ((rc = rpmwfPushRPM(wf, "Lead")) != RPMRC_OK)
	goto exit;
    if ((rc = rpmwfPushRPM(wf, "Signature")) != RPMRC_OK)
	goto exit;
    if ((rc = rpmwfPushRPM(wf, "Header")) != RPMRC_OK)
	goto exit;
    if ((rc = rpmwfPushRPM(wf, "Payload")) != RPMRC_OK)
	goto exit;

exit:
    (void) rpmwfFiniRPM(wf);

    return rc;
}

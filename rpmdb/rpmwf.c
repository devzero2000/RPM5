
#include "system.h"

#include <rpmio.h>
#include <rpmlib.h>

#include <rpmxar.h>
#define	_RPMWF_INTERNAL
#include <rpmwf.h>

#include "debug.h"

/*@access FD_t @*/
/*@access rpmxar @*/	/* XXX fprintf */

/*@unchecked@*/
int _rpmwf_debug = 0;

#if defined(WITH_XAR)

rpmRC rpmwfPushXAR(rpmwf wf, const char * fn)
{
    char * b = NULL;
    size_t nb = 0;
    int xx;

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

    xx = rpmxarSwapBuf(wf->xar, b, nb, NULL, NULL);
    xx = rpmxarPush(wf->xar, fn);
    return (xx == 0 ? RPMRC_OK : RPMRC_FAIL);
}

rpmRC rpmwfPullXAR(rpmwf wf, const char * fn)
{
    rpmRC rc = RPMRC_OK;
    char * b = NULL;
    size_t nb = 0;
    int xx;

    xx = rpmxarPull(wf->xar, fn);
    if (xx == 1)
	return RPMRC_NOTFOUND;
    xx = rpmxarSwapBuf(wf->xar, NULL, 0, &b, &nb);

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

static size_t hSize(uint32_t *p)
	/*@*/
{
    return (8 + 8 + 16 * ntohl(p[2]) + ntohl(p[3]));
}

rpmRC rpmwfInitRPM(rpmwf wf, const char * fn, const char * fmode)
{
if (_rpmwf_debug)
fprintf(stderr, "*** rpmwfInitRPM(%p, %s, %s)\n", wf, fn, fmode);
    if (fn == NULL)
	fn = wf->fn;
assert(fn != NULL);

/*@-globs@*/
    wf->fd = Fopen(fn, fmode);
/*@=globs@*/
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

rpmwf XrpmwfUnlink(rpmwf wf, const char * msg, const char * fn, unsigned ln)
{
    if (wf == NULL) return NULL;
/*@-modfilesys@*/
if (_rpmwf_debug && msg != NULL)
fprintf(stderr, "--> wf %p -- %d %s at %s:%u\n", wf, wf->nrefs, msg, fn, ln);
/*@=modfilesys@*/
    wf->nrefs--;
    return NULL;
}

rpmwf XrpmwfLink(rpmwf wf, const char * msg, const char * fn, unsigned ln)
{
    if (wf == NULL) return NULL;
    wf->nrefs++;

/*@-modfilesys@*/
if (_rpmwf_debug && msg != NULL)
fprintf(stderr, "--> wf %p ++ %d %s at %s:%u\n", wf, wf->nrefs, msg, fn, ln);
/*@=modfilesys@*/

    /*@-refcounttrans@*/ return wf; /*@=refcounttrans@*/
}

rpmwf rpmwfFree(rpmwf wf)
{
    if (wf) {

/*@-onlytrans@*/
	if (wf->nrefs > 1)
	    return rpmwfUnlink(wf, "rpmwfFree");

	if (wf->b == NULL) {
/*@-dependenttrans@*/	/* rpm needs dependent, xar needs only */
	    wf->l = _free(wf->l);
	    wf->s = _free(wf->s);
	    wf->h = _free(wf->h);
	    wf->p = _free(wf->p);
/*@=dependenttrans@*/
	}

	wf->xar = rpmxarFree(wf->xar);
	(void) rpmwfFiniRPM(wf);

	wf->fn = _free(wf->fn);

	(void) rpmwfUnlink(wf, "rpmwfFree");
/*@=onlytrans@*/
	/*@-refcounttrans -usereleased@*/
	memset(wf, 0, sizeof(*wf));         /* XXX trash and burn */
	wf = _free(wf);
	/*@=refcounttrans =usereleased@*/
    }
    return NULL;
}

rpmwf rpmwfNew(const char * fn)
{
    struct stat sb, *st = &sb;
    rpmwf wf;
    int xx;

/*@-globs@*/
    if ((xx = Stat(fn, st)) < 0)
	return NULL;
/*@=globs@*/
    wf = xcalloc(1, sizeof(*wf));
    wf->fn = xstrdup(fn);
    wf->nb = st->st_size;

if (_rpmwf_debug)
fprintf(stderr, "*** rpmwfNew(%s) wf %p nb %u\n", wf->fn, wf, (unsigned)wf->nb);

    return rpmwfLink(wf, "rpmwfNew");
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

    wf->xar = rpmxarNew(wf->fn, "r");
    if (wf->xar == NULL) {
	wf = rpmwfFree(wf);
	return NULL;
    }
if (_rpmwf_debug)
fprintf(stderr, "*** rdXAR(%s) wf %p xar %p\n", xarfn, wf, wf->xar);

    while (rpmxarNext(wf->xar) == 0)
	rc = rpmwfPullXAR(wf, NULL);

    wf->xar = rpmxarFree(wf->xar);
    return wf;
}

rpmRC wrXAR(const char * xarfn, rpmwf wf)
{
    rpmRC rc;

if (_rpmwf_debug)
fprintf(stderr, "*** wrXAR(%s, %p)\n", xarfn, wf);
    wf->xar = rpmxarNew(xarfn, "w");
    if (wf->xar == NULL)
	return RPMRC_FAIL;

    if ((rc = rpmwfPushXAR(wf, "Lead")) != RPMRC_OK)
	goto exit;
    if ((rc = rpmwfPushXAR(wf, "Signature")) != RPMRC_OK)
	goto exit;
    if ((rc = rpmwfPushXAR(wf, "Header")) != RPMRC_OK)
	goto exit;
    if ((rc = rpmwfPushXAR(wf, "Payload")) != RPMRC_OK)
	goto exit;

exit:
    wf->xar = rpmxarFree(wf->xar);
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

#endif

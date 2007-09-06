
#define	RPM2XAR
#include "system.h"
#include <inttypes.h>
#include <xar/xar.h>
#include <rpmio.h>
#include <rpmcli.h>
#include "debug.h"

static int _debug = 0;

typedef struct rpmwf_s {
    const char * fn;
    FD_t fd;
    void * b;
    size_t nb;
    char * l;
    size_t nl;
    char * s;
    size_t ns;
    char * h;
    size_t nh;
    char * p;
    size_t np;
    xar_t x;
    xar_file_t f;
    xar_iter_t i;
    int first;
} * rpmwf;

static rpmRC rpmwfFiniXAR(rpmwf wf)
{
    if (wf->i) {
	xar_iter_free(wf->i);
	wf->i = NULL;
    }
    if (wf->x) {
	xar_close(wf->x);
	wf->x = NULL;
    }
    return RPMRC_OK;
}

static rpmRC rpmwfInitXAR(rpmwf wf, const char * fn, const char * fmode)
{
    int flags = ((fmode && *fmode == 'w') ? WRITE : READ);

    if (fn == NULL)
	fn = wf->fn;
assert(fn);
    
    wf->x = xar_open(fn, flags);
    if (flags == READ) {
	wf->i = xar_iter_new();
	wf->first = 1;
    }
    return RPMRC_OK;
}

static rpmRC rpmwfNextXAR(rpmwf wf)
{
    if (wf->first) {
	wf->f = xar_file_first(wf->x, wf->i);
	wf->first = 0;
    } else
	wf->f = xar_file_next(wf->i);

    if (wf->f == NULL)
	return RPMRC_NOTFOUND;

    return RPMRC_OK;
}

static rpmRC rpmwfPushXAR(rpmwf wf, const char * fn)
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
	wf->f = xar_add_frombuffer(wf->x, NULL, fn, b, nb);
	if (wf->f == NULL)
	    return RPMRC_FAIL;
    }
    return RPMRC_OK;
}

static rpmRC rpmwfPullXAR(rpmwf wf, const char * fn)
{
    const char * path = xar_get_path(wf->f);
    char * b = NULL;
    size_t nb = 0;
    rpmRC rc = RPMRC_OK;
    int xx;

    if (fn == NULL)
	fn = path;
    else
	assert(!strcmp(fn, path));

    xx = xar_extract_tobuffersz(wf->x, wf->f, &b, &nb);
    if (xx || b == NULL || nb == 0)
	return RPMRC_NOTFOUND;

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

static rpmRC rpmwfFiniRPM(rpmwf wf)
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

static rpmRC rpmwfInitRPM(rpmwf wf, const char * fn, const char * fmode)
{
    if (fn == NULL)
	fn = wf->fn;
assert(fn);

    wf->fd = Fopen(fn, fmode);
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

static rpmRC rpmwfPushRPM(rpmwf wf, const char * fn)
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

static rpmwf rpmwfFree(rpmwf wf)
{
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

static rpmwf rpmwfNew(const char * fn)
{
    struct stat sb, *st = &sb;
    rpmwf wf;
    int xx;

    if ((xx = Stat(fn, st)) < 0)
	return NULL;
    wf = xcalloc(1, sizeof(*wf));
    wf->fn = xstrdup(fn);
    wf->nb = st->st_size;

    return wf;
}

static rpmwf rdRPM(const char * rpmfn)
{
    rpmwf wf;
    rpmRC rc;

    if ((wf = rpmwfNew(rpmfn)) == NULL)
	return wf;

    if ((rc = rpmwfInitRPM(wf, NULL, "r")) != RPMRC_OK) {
	wf = rpmwfFree(wf);
	return NULL;
    }


    return wf;
}

static rpmwf rdXAR(const char * xarfn)
{
    rpmwf wf;
    rpmRC rc;

    if ((wf = rpmwfNew(xarfn)) == NULL)
	return wf;

    if ((rc = rpmwfInitXAR(wf, NULL, "r")) != RPMRC_OK) {
	wf = rpmwfFree(wf);
	return NULL;
    }

    while ((rc = rpmwfNextXAR(wf)) == RPMRC_OK) {
	rc = rpmwfPullXAR(wf, NULL);
    }

    (void) rpmwfFiniXAR(wf);
    return wf;
}

static rpmRC wrXAR(const char * xarfn, rpmwf wf)
{
    rpmRC rc;

    if ((rc = rpmwfInitXAR(wf, xarfn, "w")) != RPMRC_OK)
	goto exit;

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

static rpmRC wrRPM(const char * rpmfn, rpmwf wf)
{
    rpmRC rc;

    if ((rc = rpmwfInitRPM(wf, rpmfn, "w")) != RPMRC_OK)
	goto exit;

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

static struct poptOption optionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,		&_debug, 1,
	NULL, NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
	N_("Common options:"),
	NULL },

    POPT_AUTOALIAS
    POPT_AUTOHELP
    POPT_TABLEEND
};

int
main(int argc, char *const argv[])
{
    poptContext optCon = rpmcliInit(argc, argv, optionsTable);
    const char ** args;
    const char * sfn;
    int ec = 0;

    if (optCon == NULL)
        exit(EXIT_FAILURE);

    if ((args = poptGetArgs(optCon)) != NULL)
    while ((sfn = *args++) != NULL) {
	rpmwf wf;
	char * tfn;
	size_t nb = strlen(sfn);
	int x = nb - (sizeof(".rpm") - 1);
	rpmRC rc;

	if (x <= 0)
	    rc = RPMRC_FAIL;
	else
	if (!strcmp(&sfn[x], ".rpm")) {
	    tfn = xstrdup(sfn);
	    strcpy(&tfn[x], ".xar");
	    if ((wf = rdRPM(sfn)) != NULL) {
		rc = wrXAR(tfn, wf);
		wf = rpmwfFree(wf);
	    } else
		rc = RPMRC_FAIL;
	    tfn = _free(tfn);
	} else
	if (!strcmp(&sfn[x], ".xar")) {
	    tfn = xstrdup(sfn);
	    strcpy(&tfn[x], ".rpm");
	    if ((wf = rdXAR(sfn)) != NULL) {
		rc = wrRPM(tfn, wf);
		wf = rpmwfFree(wf);
	    } else
		rc = RPMRC_FAIL;
	    tfn = _free(tfn);
	} else
	    rc = RPMRC_FAIL;
	if (rc != RPMRC_OK)
	    ec++;
    }

    optCon = rpmcliFini(optCon);

    return ec;
}

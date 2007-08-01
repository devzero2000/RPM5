#include "system.h"

#include <rpmio_internal.h>
#include <rpmmessages.h>
#include <rpmmacro.h>
#include <popt.h>

#include "magic.h"

#include "debug.h"

static int _debug = 0;

#define	FNPATH		"tmagic"
static char * fnpath = FNPATH;

typedef struct rpmmp_t * rpmmp;

struct rpmmp_t {
    const char * fn;
    int flags;
    magic_t ms;
};

static
rpmmp rpmmpFree(rpmmp mp)
{
    if (mp) {
	if (mp->ms) {
	    magic_close(mp->ms);
	    mp->ms = NULL;
	}
	mp->fn = _free(mp->fn);
	mp = _free(mp);
    }
    return NULL;
}

static
rpmmp rpmmpNew(const char * fn, int flags)
{
    rpmmp mp = xcalloc(1, sizeof(*mp));
    int xx;

    if (fn)
	mp->fn = xstrdup(fn);
    mp->flags = (flags ? flags : MAGIC_CHECK);
    mp->ms = magic_open(mp->ms);
    if (mp->ms == NULL)
	return rpmmpFree(mp);
    xx = magic_load(mp->ms, mp->fn);
    return mp;
}

static
const char * rpmmpBuffer(rpmmp mp, const char * b, size_t nb)
{
    const char * t = NULL;

    if (mp->ms) {
	t = magic_buffer(mp->ms, b, nb);
	t = xstrdup((t ? t : ""));
    }

    return t;
}

static
void readFile(rpmmp mp, const char * path)
{
    FD_t fd;

fprintf(stderr, "===== %s(%p, %s)\n", __FUNCTION__, mp, path);
    fd = Fopen(path, "r");
    if (fd != NULL) {
	char buf[BUFSIZ];
	size_t len = Fread(buf, 1, sizeof(buf), fd);
	(void) Fclose(fd);

	if (len > 0) {
	    const char * magic = rpmmpBuffer(mp, buf, len);
	    fprintf(stderr, "==> magic \"%s\"\n", magic);
	    magic = _free(magic);
	}
    }
}

static struct poptOption optionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL,	&_debug, -1,		NULL, NULL },
 { "ftpdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_ftp_debug, -1,
	N_("debug protocol data stream"), NULL},
 { "rpmiodebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmio_debug, -1,
	N_("debug rpmio I/O"), NULL},
 { "urldebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_url_debug, -1,
	N_("debug URL cache handling"), NULL},
 { "verbose", 'v', 0, 0, 'v',				NULL, NULL },
  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, const char *argv[])
{
    poptContext optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
    rpmmp mp;
    int rc;

    while ((rc = poptGetNextOpt(optCon)) > 0) {
	switch (rc) {
	case 'v':
	    rpmIncreaseVerbosity();
	    /*@switchbreak@*/ break;
	default:
            /*@switchbreak@*/ break;
	}
    }


    if (_debug) {
	rpmIncreaseVerbosity();
	rpmIncreaseVerbosity();
    }

    mp = rpmmpNew(NULL, 0);
    readFile(mp, fnpath);
    mp = rpmmpFree(mp);

/*@i@*/ urlFreeCache();

    return 0;
}

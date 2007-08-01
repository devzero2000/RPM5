#include "system.h"

#include <rpmio_internal.h>
#include <rpmmessages.h>
#include <rpmerr.h>
#include <rpmmacro.h>
#include <popt.h>

#include "magic.h"

#include "debug.h"

static int _rpmmg_debug = -1;

#define	FNPATH		"tmagic"
static char * fnpath = FNPATH;

typedef struct rpmmg_t * rpmmg;

struct rpmmg_t {
    const char * fn;
    int flags;
    magic_t ms;
};

static
rpmmg rpmmgFree(rpmmg mg)
{
if (_rpmmg_debug)
fprintf(stderr, "--> rpmmgFree(%p)\n", mg);
    if (mg) {
	if (mg->ms) {
	    magic_close(mg->ms);
	    mg->ms = NULL;
	}
	mg->fn = _free(mg->fn);
	mg = _free(mg);
    }
    return NULL;
}

static
rpmmg rpmmgNew(const char * fn, int flags)
{
    rpmmg mg = xcalloc(1, sizeof(*mg));
    int xx;

    if (fn)
	mg->fn = xstrdup(fn);
    mg->flags = (flags ? flags : MAGIC_CHECK);
    mg->ms = magic_open(flags);
    if (mg->ms == NULL) {
	rpmError(RPMERR_EXEC, _("magic_open(0x%x) failed: %s\n"),
		flags, strerror(errno));
	return rpmmgFree(mg);
    }
    xx = magic_load(mg->ms, mg->fn);
    if (xx == -1) {
        rpmError(RPMERR_EXEC, _("magic_load(ms, \"%s\") failed: %s\n"),
                fn, magic_error(mg->ms));
	return rpmmgFree(mg);
    }

if (_rpmmg_debug)
fprintf(stderr, "--> rpmmgNew(%s, 0x%x) mg %p\n", fn, flags, mg);
    return mg;
}

static
const char * rpmmgFile(rpmmg mg, const char *fn)
{
    const char * t = NULL;

    if (mg->ms)
	t = magic_file(mg->ms, fn);
    t = xstrdup((t ? t : ""));

if (_rpmmg_debug)
fprintf(stderr, "--> rpmmgFile(%p, %s) %s\n", mg, fn, t);
    return t;
}

static
const char * rpmmgBuffer(rpmmg mg, const char * b, size_t nb)
{
    const char * t = NULL;

    if (mg->ms)
	t = magic_buffer(mg->ms, b, nb);
    t = xstrdup((t ? t : ""));

if (_rpmmg_debug)
fprintf(stderr, "--> rpmmgBuffer(%p, %p[%d]) %s\n", mg, b, nb, t);
    return t;
}

static
void readFile(rpmmg mg, const char * path)
{
    FD_t fd;

fprintf(stderr, "===== %s(%p, %s)\n", __FUNCTION__, mg, path);
    fd = Fopen(path, "r");
    if (fd != NULL) {
	char buf[BUFSIZ];
	size_t len = Fread(buf, 1, sizeof(buf), fd);
	(void) Fclose(fd);

	if (len > 0) {
	    const char * magic = rpmmgBuffer(mg, buf, len);
	    fprintf(stderr, "==> magic \"%s\"\n", magic);
	    magic = _free(magic);
	}
    }
}

static struct poptOption optionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL,	&_rpmmg_debug, -1,		NULL, NULL },
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
    rpmmg mg;
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


    if (_rpmmg_debug) {
	rpmIncreaseVerbosity();
	rpmIncreaseVerbosity();
    }

    mg = rpmmgNew(NULL, 0);
    readFile(mg, fnpath);
    mg = rpmmgFree(mg);

/*@i@*/ urlFreeCache();

    return 0;
}

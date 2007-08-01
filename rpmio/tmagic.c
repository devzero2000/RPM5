#include "system.h"

#include "magic.h"
#define	_RPMMG_INTERNAL
#include <rpmmg.h>

#include <rpmio_internal.h>
#include <rpmmessages.h>
#include <rpmmacro.h>
#include <popt.h>

#include "debug.h"

#define        FNPATH          "tmagic"
static char * fnpath = FNPATH;

static
void readFile(rpmmg mg, const char * path)
{
    FD_t fd;

fprintf(stderr, "===== readFile(%p, %s)\n", mg, path);
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


_rpmmg_debug = -1;
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

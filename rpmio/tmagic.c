#include "system.h"

#include <rpmio_internal.h>
#include <rpmmessages.h>
#include <rpmmacro.h>
#include <popt.h>

#include "magic.h"

#include "debug.h"

static int _debug = 0;

#define	FNPATH		"tmagic.test"
static char * fnpath = FNPATH;

static void readFile(const char * path)
{
    FD_t fd;

fprintf(stderr, "===== %s\n", path);
    fd = Fopen(path, "r");
    if (fd != NULL) {
	char buf[BUFSIZ];
	size_t len = Fread(buf, 1, sizeof(buf), fd);
	int xx;
        xx = Fclose(fd);

	if (len > 0)
	    fwrite(buf, 1, len, stderr);
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
    int msflags = MAGIC_CHECK;
    magic_t ms = NULL;
    const char * magicfile = NULL;
    int rc;
    int xx;

    while ((rc = poptGetNextOpt(optCon)) > 0) {
	switch (rc) {
	case 'v':
	    rpmIncreaseVerbosity();
	    /*@switchbreak@*/ break;
	default:
            /*@switchbreak@*/ break;
	}
    }

    ms = magic_open(msflags);
assert(ms);
    xx = magic_load(ms, magicfile);

    if (_debug) {
	rpmIncreaseVerbosity();
	rpmIncreaseVerbosity();
    }

    readFile(fnpath);

    if (ms)
	magic_close(ms);
/*@i@*/ urlFreeCache();

    return 0;
}

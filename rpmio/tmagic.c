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

static const char * Fmagic(const char * buffer, size_t length)
{
    int msflags = MAGIC_CHECK;
    const char * magicfile = NULL;
    magic_t ms = magic_open(msflags);
    const char * t = NULL;

    if (ms) {
	(void) magic_load(ms, magicfile);
	t = magic_buffer(ms, buffer, length);
	t = xstrdup((t ? t : ""));
	magic_close(ms);
    }

    return t;
}

static void readFile(const char * path)
{
    FD_t fd;

fprintf(stderr, "===== %s\n", path);
    fd = Fopen(path, "r");
    if (fd != NULL) {
	char buf[BUFSIZ];
	size_t len = Fread(buf, 1, sizeof(buf), fd);
	(void) Fclose(fd);

	if (len > 0) {
	    const char * magic = Fmagic(buf, len);
	    fprintf(stderr, "==> magic \"%s\"\n", magic);
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

    readFile(fnpath);

/*@i@*/ urlFreeCache();

    return 0;
}

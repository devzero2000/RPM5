#include "system.h"

#include <rpmio_internal.h>
#include <rpmmacro.h>
#include <rpmcb.h>
#include <argv.h>
#include <popt.h>

#include "debug.h"

static int _debug = 0;

static int my_Glob_error(const char *epath, int eerrno)
{
fprintf(stderr, "*** glob_error(%p,%d) path %s\n", epath, eerrno, epath);
    return 1;
}

static int printGlob(const char * path)
{
    rpmop op = memset(alloca(sizeof(*op)), 0, sizeof(*op));
    glob_t gl = { .gl_pathc = 0, .gl_pathv = NULL, .gl_offs = 0 };
    int rc;
    int xx;

fprintf(stderr, "===== %s\n", path);
    xx = rpmswEnter(op, 0);
    gl.gl_pathc = 0;
    gl.gl_pathv = NULL;
    gl.gl_offs = 0;
    rc = Glob(path, 0, my_Glob_error, &gl);
    if (rc != 0) {
fprintf(stderr, "*** Glob rc %d\n", rc);
    } else
    if (rpmIsVerbose()) {
	int i;
	for (i = 0; i < gl.gl_pathc; i++)
	    fprintf(stderr, "%5d %s\n", i, gl.gl_pathv[i]);
    }
    Globfree(&gl);
    xx = rpmswExit(op, 0);

    rpmswPrint("glob:", op);
    return rc;
}

static struct poptOption optionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL,	&_debug, -1,		NULL, NULL },

 { "avdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_av_debug, -1,
	N_("debug protocol data stream"), NULL},
 { "davdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_dav_debug, -1,
	N_("debug protocol data stream"), NULL},
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
main(int argc, char *argv[])
{
    poptContext optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
    ARGV_t av;
    int ac;
    const char * dn;
    int rc;
    int xx;

    while ((rc = poptGetNextOpt(optCon)) > 0) {
	const char * optArg = poptGetOptArg(optCon);
	switch (rc) {
	case 'v':
	    rpmIncreaseVerbosity();
	    /*@switchbreak@*/ break;
	default:
            /*@switchbreak@*/ break;
	}
	optArg = _free(optArg);
    }

    if (_debug) {
	rpmIncreaseVerbosity();
	rpmIncreaseVerbosity();
_av_debug = -1;
_dav_debug = -1;
_ftp_debug = -1;
_url_debug = -1;
_rpmio_debug = -1;
    }

    av = poptGetArgs(optCon);
    ac = argvCount(av);
    if (ac < 1) {
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }

    while ((dn = *av++) != NULL)
	xx = printGlob(dn);

exit:

/*@i@*/ urlFreeCache();

    optCon = poptFreeContext(optCon);

    return 0;
}

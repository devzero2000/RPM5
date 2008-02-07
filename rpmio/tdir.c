#include "system.h"

#include <rpmio_internal.h>
#include <rpmmacro.h>
#include <rpmcb.h>
#include <argv.h>
#include <popt.h>

#include "debug.h"

static int _debug = 0;

static void printDir(const char * path)
{
    struct dirent * dp;
    DIR * dir;
    int xx;
    int i;

fprintf(stderr, "===== %s\n", path);
    dir = Opendir(path);
    i = 0;
    while ((dp = Readdir(dir)) != NULL) {
fprintf(stderr, "%5d (%x,%x) %x %x %s\n", i++,
(unsigned) dp->d_ino,
#if !(defined(hpux) || defined(__hpux) || defined(sun) || defined(RPM_OS_AIX)) && \
    !defined(__APPLE__) && !defined(__FreeBSD_kernel__) && !defined(__FreeBSD__) && !defined(__NetBSD__) && !defined(__DragonFly__)
(unsigned) dp->d_off,
#else
(unsigned)0,
#endif
(unsigned) dp->d_reclen,
(unsigned) dp->d_type,
dp->d_name);
    }
    xx = Closedir(dir);
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
    ARGV_t av = NULL;
    int ac;
    const char * dn;

    int rc;

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
    }

    av = poptGetArgs(optCon);
    ac = argvCount(av);
    if (ac < 1) {
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }

    while ((dn = *av++) != NULL) {
	dn = rpmGetPath(dn, "/", NULL);
	printDir(dn);
	dn = _free(dn);
    }

exit:

/*@i@*/ urlFreeCache();

    optCon = poptFreeContext(optCon);

    return 0;
}

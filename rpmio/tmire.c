#include "system.h"

#define _MIRE_INTERNAL
#include <rpmio.h>
#include <rpmcb.h>
#include <argv.h>
#include <mire.h>
#include <popt.h>

#include "debug.h"

static int _debug = 0;

static struct poptOption optionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL,	&_debug, -1,		NULL, NULL },
  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    poptContext optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
    miRE mire = NULL;
    ARGV_t av = NULL;
    int ac = 0;
    int rc;
    int xx;
    int i;

    while ((rc = poptGetNextOpt(optCon)) > 0) {
        const char * optArg = poptGetOptArg(optCon);
        optArg = _free(optArg);
	switch (rc) {
	case 'v':
	    rpmIncreaseVerbosity();
	    /*@switchbreak@*/ break;
	default:
	    poptPrintUsage(optCon, stderr, 0);
	    goto exit;
            /*@switchbreak@*/ break;
	}
    }

    if (_debug) {
	rpmIncreaseVerbosity();
	rpmIncreaseVerbosity();
_mire_debug = 1;
    }

    av = poptGetArgs(optCon);
    ac = argvCount(av);
    if (ac != 1) {
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }

    mire = mireNew(RPMMIRE_REGEX, 0);
    if ((xx = mireRegcomp(mire, argv[1])) != 0)
	goto exit;
    
    xx = argvFgets(&av, NULL);
    ac = argvCount(av);

    for (i = 0; i < ac; i++) {
	xx = mireRegexec(mire, av[i]);
	if (xx == 0)
	    fprintf(stdout, "%s\n", av[i]);
    }

exit:
    mire = mireFree(mire);

    av = argvFree(av);

    optCon = poptFreeContext(optCon);

    return 0;
}

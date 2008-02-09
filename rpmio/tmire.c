#include "system.h"

#define _MIRE_INTERNAL
#include <poptIO.h>

#include "debug.h"

static struct poptOption optionsTable[] = {

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"),
	NULL },

  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    poptContext optCon = rpmioInit(argc, argv, optionsTable);
    miRE mire = NULL;
    ARGV_t av = NULL;
    int ac = 0;
    int rc;
    int xx;
    int i;

    if (__debug) {
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

#include "system.h"

#include <rpmio_internal.h>
#include <poptIO.h>

#define	_RPMRUBY_INTERNAL
#include <rpmruby.h>

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
    const char * rubyFN = NULL;
    int rubyFlags = 0;
    rpmruby ruby = rpmrubyNew(rubyFN, rubyFlags);
    ARGV_t av = poptGetArgs(optCon);
    int ac = argvCount(av);
    const char * fn;
    int rc = 1;		/* assume failure */

    if (ac < 1) {
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }

    while ((fn = *av++) != NULL) {
	const char * result;
	rpmRC ret;
	result = NULL;
	if ((ret = rpmrubyRunFile(ruby, fn, &result)) != RPMRC_OK)
	    goto exit;
	if (result != NULL && *result != '\0')
	    fprintf(stdout, "%s\n", result);
    }
    rc = 0;

exit:
    ruby = rpmrubyFree(ruby);
    optCon = rpmioFini(optCon);

    return rc;
}

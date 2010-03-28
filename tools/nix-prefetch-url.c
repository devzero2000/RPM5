#include "system.h"

#define	_RPMNIX_INTERNAL
#include <rpmnix.h>
#include <poptIO.h>

#include "debug.h"

static struct poptOption nixPrefetchUrlOptions[] = {

    /* XXX add --quiet and --print options. */

#ifndef	NOTYET
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"), NULL },
#endif

  POPT_AUTOHELP

  { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
	N_("\
syntax: nix-prefetch-url URL [EXPECTED-HASH]\n\
"), NULL },

  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    rpmnix nix = rpmnixNew(argv, RPMNIX_FLAGS_NONE, nixPrefetchUrlOptions);
    int ec = rpmnixPrefetchURL(nix);

    nix = rpmnixFree(nix);

    return ec;
}

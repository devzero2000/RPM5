#include "system.h"

#define	_RPMNIX_INTERNAL
#include <rpmnix.h>
#include <rpmdir.h>
#include <poptIO.h>

/*==============================================================*/

static struct poptOption rpmnixCollectGarbageOptions[] = {

 { "delete-old", 'd', POPT_BIT_SET,	&_nix.flags, RPMNIX_FLAGS_DELETEOLD,
	N_("FIXME"), NULL },

#ifndef	NOTYET
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"), NULL },
#endif

  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    rpmnix nix = rpmnixNew(argv, RPMNIX_FLAGS_NONE, rpmnixCollectGarbageOptions);
    int ec = rpmnixCollectGarbage(nix);

    nix = rpmnixFree(nix);

    return ec;
}

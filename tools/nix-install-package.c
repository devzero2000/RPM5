#include "system.h"

#define	_RPMNIX_INTERNAL
#include <rpmnix.h>
#include <poptIO.h>

#include "debug.h"

/*==============================================================*/

static struct poptOption rpmnixInstallPackageOptions[] = {

 { "profile", 'p', POPT_ARG_ARGV,	&_nix.profiles, 0,
	N_("FIXME"), NULL },
 { "non-interactive", '\0', POPT_BIT_CLR, &_nix.flags, RPMNIX_FLAGS_INTERACTIVE,
	N_("FIXME"), NULL },
 { "url", '\0', POPT_ARG_STRING,	&_nix.url, 0,
	N_("FIXME"), NULL },

#ifndef	NOTYET
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"), NULL },
#endif

  POPT_AUTOHELP

  { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
	N_("\
Usage: nix-install-package (FILE | --url URL)\n\
\n\
Install a Nix Package (.nixpkg) either directly from FILE or by\n\
downloading it from URL.\n\
\n\
Flags:\n\
  --profile / -p LINK: install into the specified profile\n\
  --non-interactive: don't run inside a new terminal\n\
"), NULL },

  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    rpmnix nix = rpmnixNew(argv, RPMNIX_FLAGS_INTERACTIVE, rpmnixInstallPackageOptions);
    int ec = rpmnixInstallPackage(nix);

    nix = rpmnixFree(nix);

    return ec;
}

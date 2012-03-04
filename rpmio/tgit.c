#include "system.h"

#include <git2.h>

#include <poptIO.h>

#define	_RPMGIT_INTERNAL
#include <rpmgit.h>

#include "debug.h"

static struct poptOption rpmgitOptionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL,	&_rpmmg_debug, -1,		NULL, NULL },
  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    poptContext con = rpmioInit(argc, argv, rpmgitOptionsTable);
    rpmgit git;
    int rc = 0;

_rpmgit_debug = -1;

    git = rpmgitNew(NULL, 0);

    rc = rpmgitConfig(git);

    rc = rpmgitInfo(git);

    rc = rpmgitWalk(git);

    git = rpmgitFree(git);

/*@i@*/ urlFreeCache();

    return rc;
}

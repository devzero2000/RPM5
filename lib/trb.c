
#include "system.h"
#include <rpmcli.h>
#include <rpmts.h>
#include "misc.h"

#include "debug.h"

#define	REPACKAGEGLOB	"/var/spool/repackage/*.rpm"

static struct poptOption optionsTable[] = {
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmInstallPoptTable, 0,
	N_("Install/Upgrade/Erase options:"),
	NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
        N_("Common options for all modes and executables:"),
        NULL },

   POPT_AUTOALIAS
   POPT_AUTOHELP
   POPT_TABLEEND
};

int
main(int argc, char *const argv[])
{
    poptContext optCon;
    rpmts ts = NULL;
    QVA_t ia = &rpmIArgs;
    int arg;
    int ec = 0;

    optCon = rpmcliInit(argc, argv, optionsTable);
    if (optCon == NULL)
        exit(EXIT_FAILURE);

    if (ia->rbtid == 0) {
	fprintf(stderr, "--rollback <timestamp> is required\n");
	exit(EXIT_FAILURE);
    }

    ts = rpmtsCreate();

    ec = rpmRollback(ts, ia, NULL);

    ts = rpmtsFree(ts);

    optCon = rpmcliFini(optCon);

    return ec;
}

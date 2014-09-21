#include "system.h"

#include <rpmio.h>	/* XXX rpmioClean */

#define	_RPMSED_INTERNAL
#include <rpmsed.h>

#include "debug.h"

int main(int argc, char **argv)
{
    /* Parse options and initialize job patterns. */
    rpmsed sed = rpmsedNew(argv, 0);
    int rc = EXIT_FAILURE;
    rpmRC xx;
    int k;

    if (sed == NULL)
	goto exit;

    sed->ofp = stdout;
    for (k = 0; k < sed->ac; k++) {

	/* Open next input file */
	sed->fn = sed->av[k];
	xx = rpmsedOpen(sed, sed->fn);
	if (xx)
	    goto exit;

	/* Read & Execute jobs on every input line */
	xx = rpmsedProcess(sed);
	if (xx)
	    goto exit;

	/* Close input file */
	xx = rpmsedClose(sed);
	if (xx)
	    goto exit;
    }
    sed->ofp = NULL;

    rc = EXIT_SUCCESS;

exit:
    sed = rpmsedFree(sed);
    rpmioClean();

    return rc;

}

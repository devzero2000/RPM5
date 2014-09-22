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

    for (k = 0; k < sed->ac; k++) {

	/* Read input file lines */
	xx = rpmsedInput(sed, sed->av[k]);
	if (xx)
	    goto exit;

	/* Execute jobs on every input line */
	xx = rpmsedProcess(sed);
	if (xx)
	    goto exit;

	/* Output result */
	xx = rpmsedOutput(sed, NULL);
	if (xx)
	    goto exit;

    }

    rc = EXIT_SUCCESS;

exit:
    sed = rpmsedFree(sed);
    rpmioClean();

    return rc;

}

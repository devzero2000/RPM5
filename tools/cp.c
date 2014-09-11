#include "system.h"

#include <rpmio.h>
#include <rpmct.h>

#include "debug.h"

int
main(int argc, char *argv[])
{
    rpmct ct = rpmctNew(argv, 0);
    rpmRC rc = RPMRC_FAIL;

    __progname = "cp";

    rc = rpmctCopy(ct);

    ct = rpmctFree(ct);

    rpmioClean();

    return (rc == RPMRC_OK ? EXIT_SUCCESS : EXIT_FAILURE);
}

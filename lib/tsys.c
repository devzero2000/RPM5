#include "system.h"

#include <rpmio_internal.h>
#include <rpmlib.h>

#include <rpmds.h>

#include "debug.h"

extern int _rpmds_debug;

int main(int argc, char *argv[])
{
    rpmds ds = NULL;
    int rc;

    rc = rpmdsSysinfo(&ds, NULL);
    
    ds = rpmdsInit(ds);
    while (rpmdsNext(ds) >= 0)
	fprintf(stderr, "%d %s\n", rpmdsIx(ds), rpmdsDNEVR(ds)+2);

    ds = rpmdsFree(ds);

    return 0;
}

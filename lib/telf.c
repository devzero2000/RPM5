#include "system.h"

#include <rpmlib.h>
#include <rpmds.h>

#include "debug.h"

extern int _rpmds_debug;

static rpmds P = NULL;
static rpmds R = NULL;

static struct rpmMergePRCO_s mergePRCO = { &P, &R, NULL, NULL };

int main(int argc, char *argv[])
{
    int flags = 0;
    int rc = 0;
    int i;

    for (i = 1; i < argc; i++)
	rc = rpmdsELF(argv[i], flags, rpmdsMergePRCO, &mergePRCO);
	
    P = rpmdsInit(P);
    while (rpmdsNext(P) >= 0)
	fprintf(stderr, "%d Provides: %s\n", rpmdsIx(P), rpmdsDNEVR(P)+2);
    P = rpmdsFree(P);

    R = rpmdsInit(R);
    while (rpmdsNext(R) >= 0)
	fprintf(stderr, "%d Requires: %s\n", rpmdsIx(R), rpmdsDNEVR(R)+2);
    R = rpmdsFree(R);

    return rc;
}

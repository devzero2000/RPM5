#include "system.h"

#include <rpmlib.h>

#include <rpmds.h>

#include "debug.h"

extern int _rpmds_debug;

int main(int argc, char *argv[])
{
    rpmMergePRCO PRCO = memset(alloca(sizeof(*PRCO)), 0, sizeof(*PRCO));
    rpmds P = NULL;
    rpmds R = NULL;
    int rc;

    PRCO->Pdsp = &P;
    PRCO->Rdsp = &R;

    rc = rpmdsLdconfig(PRCO, NULL);
    
    P = rpmdsInit(P);
    while (rpmdsNext(P) >= 0)
	fprintf(stderr, "%d Provides: %s\n", rpmdsIx(P), rpmdsDNEVR(P)+2);
    P = rpmdsFree(P);

    R = rpmdsInit(R);
    while (rpmdsNext(R) >= 0)
	fprintf(stderr, "%d Requires: %s\n", rpmdsIx(R), rpmdsDNEVR(R)+2);
    R = rpmdsFree(R);

    return 0;
}

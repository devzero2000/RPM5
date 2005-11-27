#include "system.h"
#include <rpmlib.h>
#include <rpmds.h>
#include "debug.h"

int main(int argc, char *argv[])
{
    rpmPRCO PRCO = memset(alloca(sizeof(*PRCO)), 0, sizeof(*PRCO));
    rpmds P = NULL;
    rpmds R = NULL;
    int rc;
    int xx;

    PRCO->Pdsp = &P;
    PRCO->Rdsp = &R;

    rc = rpmdsLdconfig(PRCO, NULL);

    xx = rpmdsPrint(P, NULL);
    xx = rpmdsPrint(R, NULL);
    
    P = rpmdsFree(P);
    R = rpmdsFree(R);

    return rc;
}

#include "system.h"
#include <rpmlib.h>
#include <rpmds.h>
#include "debug.h"

static rpmds P = NULL;
static rpmds R = NULL;

static struct rpmMergePRCO_s mergePRCO = { &P, &R, NULL, NULL };

int main(int argc, char *argv[])
{
    int flags = 0;
    int rc = 0;
    int xx;
    int i;

    for (i = 1; i < argc; i++)
	rc = rpmdsELF(argv[i], flags, rpmdsMergePRCO, &mergePRCO);
	
    xx = rpmdsPrint(P, NULL);
    xx = rpmdsPrint(R, NULL);

    P = rpmdsFree(P);
    R = rpmdsFree(R);

    return rc;
}

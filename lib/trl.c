#include "system.h"

#include <rpmlib.h>

#include <rpmds.h>

#include "debug.h"

extern int _rpmds_debug;

int main(int argc, char *argv[])
{
    rpmds P = NULL;
    int rc;
    int xx;

    rc = rpmdsRpmlib(&P, NULL);
    
    xx = rpmdsPrint(P, NULL);

    P = rpmdsFree(P);

    return 0;
}

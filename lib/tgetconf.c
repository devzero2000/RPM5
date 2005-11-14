#include "system.h"
#include <rpmlib.h>
#include <rpmds.h>
#include "debug.h"

int
main (int argc, char *argv[])
{
     rpmds P = NULL;
     int rc;
     int xx;

    /* Set locale.  Do not set LC_ALL because the other categories must
       not be affected (according to POSIX.2).  */
    setlocale (LC_CTYPE, "");
    setlocale (LC_MESSAGES, "");

    /* Initialize the message catalog.  */
    textdomain (PACKAGE);

    rc = rpmdsGetconf (&P, NULL);
    xx = rpmdsPrint(P, NULL);

    P = rpmdsFree(P);

    return rc;
}

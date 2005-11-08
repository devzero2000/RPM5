#include "system.h"

#include <rpmlib.h>

#include <rpmds.h>

#include "debug.h"

extern int _rpmds_debug;

#define _PKGCONFIG_PROVIDES  "/usr/bin/find /usr/lib -name '*.pc' | /usr/lib/rpm/pkgconfigdeps.sh -P"
static const char * _pkgconfig_provides = _PKGCONFIG_PROVIDES;

#define _PKGCONFIG_REQUIRES  "/bin/rpm -qal | grep '\\.pc$' | /usr/lib/rpm/pkgconfigdeps.sh -R"
static const char * _pkgconfig_requires = _PKGCONFIG_REQUIRES;

int main(int argc, char *argv[])
{
    rpmds P = NULL;
    rpmds R = NULL;
    int rc;

fprintf(stderr, "\n*** Gathering pkgconfig Provides: using\n\t%s\n", _pkgconfig_provides);
    rc = rpmdsPipe(&P, RPMTAG_PROVIDENAME, _pkgconfig_provides);
fprintf(stderr, "\n*** Gathering pkgconfig Requires: using\n\t%s\n", _pkgconfig_requires);
    rc = rpmdsPipe(&R, RPMTAG_REQUIRENAME, _pkgconfig_requires);

fprintf(stderr, "\n*** Checking pkgconfig Requires(%d): against Provides(%d): closure --\n", rpmdsCount(R), rpmdsCount(P));

    /* Allocate the R results array (to be filled in by rpmdsSearch). */
    (void) rpmdsSetResult(R, 0);

    /* Collect the rpmdsSearch results (in the R dependency set). */
    R = rpmdsInit(R);
    while (rpmdsNext(R) >= 0)
	rc = rpmdsSearch(P, R);

    /* Display the results. */
    R = rpmdsInit(R);
    while (rpmdsNext(R) >= 0) {
	rc = rpmdsResult(R);
	    fprintf(stderr, "%d %s\n", rpmdsIx(R), rpmdsDNEVR(R)+2);
    }

    P = rpmdsFree(P);
    R = rpmdsFree(R);

    return 0;
}

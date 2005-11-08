#include "system.h"

#include <rpmlib.h>

#include <rpmds.h>

#include "debug.h"

extern int _rpmds_debug;

#define _LIBTOOL_PROVIDES  "/usr/bin/find /usr/lib -name '*.la' | /usr/lib/rpm/libtooldeps.sh -P"
static const char * _libtool_provides = _LIBTOOL_PROVIDES;

#define _LIBTOOL_REQUIRES  "/bin/rpm -qal | grep '\\.la$' | /usr/lib/rpm/libtooldeps.sh -R"
static const char * _libtool_requires = _LIBTOOL_REQUIRES;

int main(int argc, char *argv[])
{
    rpmds P = NULL;
    rpmds R = NULL;
    int rc;

fprintf(stderr, "\n*** Gathering libtool Provides: using\n\t%s\n", _libtool_provides);
    rc = rpmdsPipe(&P, RPMTAG_PROVIDENAME, _libtool_provides);
fprintf(stderr, "\n*** Gathering libtool Requires: using\n\t%s\n", _libtool_requires);
    rc = rpmdsPipe(&R, RPMTAG_REQUIRENAME, _libtool_requires);

fprintf(stderr, "\n*** Checking libtool Requires(%d): against Provides(%d): closure --\n", rpmdsCount(R), rpmdsCount(P));

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
	if (rc > 0)
	    continue;
	fprintf(stderr, "%d %s\n", rpmdsIx(R), rpmdsDNEVR(R)+2);
    }

    P = rpmdsFree(P);
    R = rpmdsFree(R);

    return 0;
}

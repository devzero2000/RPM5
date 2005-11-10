#include "system.h"

#include <rpmlib.h>

#include <rpmds.h>

#include "debug.h"

extern int _rpmds_debug;

#define _DPKG_PROVIDES  "egrep '^(Package|Status|Version|Provides):' /var/lib/dpkg/status | sed -e '\n\
/^Package: / {\n\
	N\n\
	/not-installed/d\n\
	N\n\
	s|^Package: \\([^\\n]*\\)\\n[^\\n]*\\nVersion: \\(.*\\)$|\\1 = \\2|\n\
}\n\
/^Provides: / {\n\
	s|^Provides: ||\n\
	s|, |\\n|g\n\
}' | sort -u"
static const char * _dpkg_provides = _DPKG_PROVIDES;

#define _RPMDB_REQUIRES  "/bin/rpm -qa --requires | sort -u | sed -e '/^\\//d' -e '/.*\\.so.*/d' -e '/^%/d' -e '/^.*(.*)/d'"
static const char * _rpmdb_requires = _RPMDB_REQUIRES;

int main(int argc, char *argv[])
{
    rpmds P = NULL;
    rpmds R = NULL;
    int rc;
    int xx;

fprintf(stderr, "\n*** Gathering dpkg Provides: using\n\t%s\n", _dpkg_provides);
    rc = rpmdsPipe(&P, RPMTAG_PROVIDENAME, _dpkg_provides);
fprintf(stderr, "\n*** Gathering rpmdb Requires: using\n\t%s\n", _rpmdb_requires);
    rc = rpmdsPipe(&R, RPMTAG_REQUIRENAME, _rpmdb_requires);

fprintf(stderr, "\n*** Checking rpmdb Requires(%d): against dpkg Provides(%d): closure --\n", rpmdsCount(R), rpmdsCount(P));

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

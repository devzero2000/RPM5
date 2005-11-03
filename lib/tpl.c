#include "system.h"

#include <rpmlib.h>

#include <rpmds.h>

#include "debug.h"

extern int _rpmds_debug;

#define _PERL_PROVIDES  "/usr/bin/find /usr/lib/perl5 | /usr/lib/rpm/perl.prov"
static const char * _perl_provides = _PERL_PROVIDES;

#define _PERL_REQUIRES  "/usr/bin/find /usr/lib/perl5 | /usr/lib/rpm/perl.req"
static const char * _perl_requires = _PERL_REQUIRES;

int main(int argc, char *argv[])
{
    rpmds P = NULL;
    rpmds R = NULL;
    int rc;

    fprintf(stderr, "Perl Provides: from \"%s\" pipe:\n", _perl_provides);
    rc = rpmdsPipe(&P, RPMTAG_PROVIDENAME, _perl_provides);
    P = rpmdsInit(P);
    while (rpmdsNext(P) >= 0)
	fprintf(stderr, "%d %s\n", rpmdsIx(P), rpmdsDNEVR(P)+2);
    P = rpmdsFree(P);

    fprintf(stderr, "\n\nPerl Requires: from \"%s\" pipe:\n", _perl_requires);
    rc = rpmdsPipe(&R, RPMTAG_REQUIRENAME, _perl_requires);
    R = rpmdsInit(R);
    while (rpmdsNext(R) >= 0)
	fprintf(stderr, "%d %s\n", rpmdsIx(R), rpmdsDNEVR(R)+2);
    R = rpmdsFree(R);

    return 0;
}

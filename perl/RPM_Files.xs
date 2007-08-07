#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#undef Fflush
#undef Mkdir
#undef Stat

#include "rpmlib.h"
#include "rpmfi.h"
#include "rpmio_internal.h"
#include "rpmpgp.h"

MODULE = RPM::Files		PACKAGE = RPM::Files

PROTOTYPES: ENABLE

void
DESTROY(Files)
    rpmfi Files
    PPCODE:
    Files = rpmfiFree(Files);

int
compare(Files, Fb)
    rpmfi Files
    rpmfi Fb
    CODE:
    RETVAL = rpmfiCompare(Files, Fb);
    OUTPUT:
    RETVAL

int
move(Files, index = 0)
    rpmfi Files;
    int index
    PREINIT:
    int i;
    CODE:
    index ++; /* keeping same behaviour than Header::Dep */
    rpmfiInit(Files, 0);
    RETVAL = 0;
    for (i=-1; i < index && (RETVAL = rpmfiNext(Files)) >= 0; i++) {}
    if (RETVAL == -1) {
        rpmfiInit(Files, 0);
        rpmfiNext(Files);
    }
    OUTPUT:
    RETVAL

int
count(Files)
    rpmfi Files
    CODE:
    RETVAL = rpmfiFC(Files);
    OUTPUT:
    RETVAL

int
countdir(Files)
    rpmfi Files
    CODE:
    RETVAL = rpmfiDC(Files);
    OUTPUT:
    RETVAL

void
init(Files)
    rpmfi Files
    CODE:
    rpmfiInit(Files, 0);
        
void
initdir(Files)
    rpmfi Files
    CODE:
    rpmfiInitD(Files, 0);

int
next(Files)
    rpmfi Files
    CODE:
    RETVAL = rpmfiNext(Files);
    OUTPUT:
    RETVAL

int
hasnext(Files)
    rpmfi Files
    CODE:
    RETVAL = rpmfiNext(Files) > -1;
    OUTPUT:
    RETVAL
        
int
nextdir(Files)
    rpmfi Files
    CODE:
    RETVAL = rpmfiNextD(Files);
    OUTPUT:
    RETVAL

void
filename(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSVpv(rpmfiFN(Files), 0)));

void
dirname(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSVpv(rpmfiDN(Files), 0)));

void
basename(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSVpv(rpmfiBN(Files), 0)));

void
fflags(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSViv(rpmfiFFlags(Files))));

void
mode(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSVuv(rpmfiFMode(Files))));

void
md5(Files)
    rpmfi Files
    PREINIT:
    const byte * md5;
    char * fmd5 = malloc((char) 33);
    PPCODE:
    if ((md5 = rpmfiDigest(Files, NULL, NULL)) != NULL && 
        *md5 != 0 /* return undef if empty */) {
        (void) pgpHexCvt(fmd5, md5, 16);
        XPUSHs(sv_2mortal(newSVpv(fmd5, 0)));
    }
    _free(fmd5);

void
link(Files)
    rpmfi Files
    PREINIT:
    const char * link;
    PPCODE:
    if ((link = rpmfiFLink(Files)) != NULL && *link != 0 /* return undef if empty */) {
        XPUSHs(sv_2mortal(newSVpv(link, 0)));
    }

void
user(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSVpv(rpmfiFUser(Files), 0)));

void
group(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSVpv(rpmfiFGroup(Files), 0)));

void
inode(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSViv(rpmfiFInode(Files))));
    
void
size(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSViv(rpmfiFSize(Files))));

void
dev(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSViv(rpmfiFRdev(Files))));

void
color(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSViv(rpmfiFColor(Files))));

void
class(Files)
    rpmfi Files
    PREINIT:
    const char * class;
    PPCODE:
    if ((class = rpmfiFClass(Files)) != NULL)
        XPUSHs(sv_2mortal(newSVpv(rpmfiFClass(Files), 0)));

void
mtime(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSViv(rpmfiFMtime(Files))));

void
nlink(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSViv(rpmfiFNlink(Files))));


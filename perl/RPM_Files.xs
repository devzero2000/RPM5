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

rpmfi
new(class, header, ts = NULL)
    char * class
    Header header
    rpmts ts
    CODE:
    RETVAL = rpmfiNew(ts, header, RPMTAG_BASENAMES, 0);
    OUTPUT:
    RETVAL

void
DESTROY(Files)
    rpmfi Files
    PPCODE:
    Files = rpmfiFree(Files);

int
count(Files)
    rpmfi Files
    CODE:
    RETVAL = rpmfiFC(Files);
    OUTPUT:
    RETVAL

int
_compare(Files, Fb)
    rpmfi Files
    rpmfi Fb
    CODE:
    RETVAL = rpmfiCompare(Files, Fb);
    OUTPUT:
    RETVAL

int
_move(Files, index = 0)
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
_countdir(Files)
    rpmfi Files
    CODE:
    RETVAL = rpmfiDC(Files);
    OUTPUT:
    RETVAL

void
_init(Files)
    rpmfi Files
    CODE:
    rpmfiInit(Files, 0);
        
void
_initdir(Files)
    rpmfi Files
    CODE:
    rpmfiInitD(Files, 0);

int
_next(Files)
    rpmfi Files
    CODE:
    RETVAL = rpmfiNext(Files);
    OUTPUT:
    RETVAL

int
_hasnext(Files)
    rpmfi Files
    CODE:
    RETVAL = rpmfiNext(Files) > -1;
    OUTPUT:
    RETVAL
        
int
_nextdir(Files)
    rpmfi Files
    CODE:
    RETVAL = rpmfiNextD(Files);
    OUTPUT:
    RETVAL

void
_filename(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSVpv(rpmfiFN(Files), 0)));

void
_dirname(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSVpv(rpmfiDN(Files), 0)));

void
_basename(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSVpv(rpmfiBN(Files), 0)));

void
_fflags(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSViv(rpmfiFFlags(Files))));

void
_mode(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSVuv(rpmfiFMode(Files))));

void
_md5(Files)
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
_link(Files)
    rpmfi Files
    PREINIT:
    const char * link;
    PPCODE:
    if ((link = rpmfiFLink(Files)) != NULL && *link != 0 /* return undef if empty */) {
        XPUSHs(sv_2mortal(newSVpv(link, 0)));
    }

void
_user(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSVpv(rpmfiFUser(Files), 0)));

void
_group(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSVpv(rpmfiFGroup(Files), 0)));

void
_inode(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSViv(rpmfiFInode(Files))));
    
void
_size(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSViv(rpmfiFSize(Files))));

void
_dev(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSViv(rpmfiFRdev(Files))));

void
_color(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSViv(rpmfiFColor(Files))));

void
_class(Files)
    rpmfi Files
    PREINIT:
    const char * class;
    PPCODE:
    if ((class = rpmfiFClass(Files)) != NULL)
        XPUSHs(sv_2mortal(newSVpv(rpmfiFClass(Files), 0)));

void
_mtime(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSViv(rpmfiFMtime(Files))));

void
_nlink(Files)
    rpmfi Files
    PPCODE:
    XPUSHs(sv_2mortal(newSViv(rpmfiFNlink(Files))));


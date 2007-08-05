/* common function for perl module */

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#undef Fflush
#undef Mkdir
#undef Stat

#include <stdio.h>
#include <string.h>
#include <utime.h>
#include <utime.h>

#include "rpmlib.h"
#include "rpmio.h"
#include "rpmconstant.h"

static int scalar2constant(SV * svconstant, const char * context, int * val) {
    int rc = 0;
    if (!svconstant || !SvOK(svconstant)) {
        warn("Use of an undefined value");
        return 0;
    } else if (SvIOK(svconstant)) {
        *val = SvIV(svconstant);
        rc = 1;
    } else if (SvPOK(svconstant)) {
        rc = rpmconstantFindName((char *)context, (void *) SvPV_nolen(svconstant), val, 0);
    } else {
    }
    return rc;
}

int sv2constant(SV * svconstant, const char * context) {
    AV * avparam;
    int val = 0;
    SV **tmpsv;
    int i;
    if (svconstant == NULL) {
        return 0;
    } else if (!SvOK(svconstant)) {
        return 0;
    } else if (SvPOK(svconstant) || SvIOK(svconstant)) {
        if (!scalar2constant(svconstant, context, &val))
            warn("Unknow value '%s' in '%s'", SvPV_nolen(svconstant), context);
    } else if (SvTYPE(SvRV(svconstant)) == SVt_PVAV) {
        avparam = (AV*) SvRV(svconstant);
        for (i = 0; i <= av_len(avparam); i++) {
            tmpsv = av_fetch(avparam, i, 0);
            if (!scalar2constant(*tmpsv, context, &val))
                warn("Unknow value '%s' in '%s' from array", SvPV_nolen(*tmpsv), context);
        }
    } else {
    }
    return val;
}

rpmTag sv2dbquerytag(SV * sv_tag) {
    int val = 0;
    if (!scalar2constant(sv_tag, "rpmdbi", &val) && !scalar2constant(sv_tag, "rpmtag", &val))
        croak("unknown tag value '%s'", SvPV_nolen(sv_tag));
    return val;
}

void _rpm2header(rpmts ts, char * filename, int checkmode) {
    FD_t fd;
    Header ret = NULL;
    rpmRC rc;
    dSP;
    if ((fd = Fopen(filename, "r"))) {
        rc = rpmReadPackageFile(ts, fd, filename, &ret);
	    if (checkmode) {
	        XPUSHs(sv_2mortal(newSViv(rc)));
		    ret = headerFree(ret); /* For checking the package, we don't keep the header */
        } else {
            if (rc == 0) {
        	    XPUSHs(sv_2mortal(sv_setref_pv(newSVpv("", 0), "RPM::Header", (void *)ret)));
            } else {
                XPUSHs(sv_2mortal(&PL_sv_undef));
            }
	    }
        Fclose(fd);
    } else {
        XPUSHs(sv_2mortal(&PL_sv_undef));
    }
        
    PUTBACK;
    return;
}

void _newiterator(rpmts ts, SV * sv_tagname, SV * sv_tagvalue, int keylen) {
    rpmdbMatchIterator mi;
    int tag = RPMDBI_PACKAGES;
    void * value = NULL;
    int i = 0;
    dSP;
        if (sv_tagname == NULL || !SvOK(sv_tagname)) {
        tag = RPMDBI_PACKAGES; /* Assume search into installed packages */
    } else {
        tag = sv2dbquerytag(sv_tagname);
    }
    if (sv_tagvalue != NULL && SvOK(sv_tagvalue)) {
        switch (tag) {
            case RPMDBI_PACKAGES:
                i = SvIV(sv_tagvalue);
                value = &i;
                keylen = sizeof(i);
            break;
            default:
                value = (void *) SvPV_nolen(sv_tagvalue);
            break;
        }
    }
    mi = rpmtsInitIterator(ts, tag, value, keylen);
    XPUSHs(sv_2mortal(sv_setref_pv(newSVpv("", 0), "RPM::PackageIterator", mi)));
    PUTBACK;
    return;
}

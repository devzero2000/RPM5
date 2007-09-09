#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#undef Fflush
#undef Mkdir
#undef Stat

#include <stdio.h>
#include <string.h>
#include <utime.h>
#include "rpmlib.h"
#include <rpmio.h>
#include "rpmcli.h"

#include "rpmts.h"
#include "rpmte.h"

#include "header.h"
#include "rpmdb.h"
#include "misc.h"

#include "rpmxs.h"

void
_populate_header_tags(HV *href)
{
    int i = 0;

    for (i = 0; i < rpmTagTableSize; i++) {
        hv_store(href, rpmTagTable[i].name, strlen(rpmTagTable[i].name), newSViv(rpmTagTable[i].val), 0);
    }
}

MODULE = RPM::Header		PACKAGE = RPM::Header

PROTOTYPES: ENABLE

void
stream2header(fp, callback = NULL)
    FILE *fp
    SV * callback
    PREINIT:
    FD_t fd = NULL;
    Header header;
    PPCODE:
    if (fp && (fd = fdDup(fileno(fp)))) {
        while ((header = headerRead(fd))) {
            if (callback != NULL && SvROK(callback)) {
                ENTER;
                SAVETMPS;
                PUSHMARK(SP);
                XPUSHs(sv_2mortal(sv_setref_pv(newSVpv("", 0), "RPM::Header", (void *)header)));
                PUTBACK;
                call_sv(callback, G_DISCARD | G_SCALAR);
                SPAGAIN;
                FREETMPS;
                LEAVE;
            } else {
                XPUSHs(sv_2mortal(sv_setref_pv(newSVpv("", 0), "RPM::Header", (void *)header)));
            }
        }
        Fclose(fd);
    }

# Read a rpm and return a Header
# Return undef if failed
void
rpm2header(filename, sv_vsflags = NULL)
    char * filename
    SV * sv_vsflags
    PREINIT:
    rpmts ts = rpmtsCreate();
    rpmVSFlags vsflags = RPMVSF_DEFAULT; 
    PPCODE:
    if (sv_vsflags == NULL) /* Nothing has been passed, default is no signature */
        vsflags |= _RPMVSF_NOSIGNATURES;
    else
        vsflags = sv2constant(sv_vsflags, "rpmvsflags");
    rpmtsSetVSFlags(ts, vsflags);
    _rpm2header(ts, filename, 0);
    SPAGAIN;
    ts = rpmtsFree(ts);

void
DESTROY(h)
	Header h
    CODE:
	headerFree(h);

#
# OO methodes
#

#
# Comparison functions
#

int
compare(h1, h2)
	Header h1
	Header h2
    CODE:
	RETVAL = rpmVersionCompare(h1, h2);
    OUTPUT:
    RETVAL


int
_op_spaceship(h1, h2, ...)
	Header h1
	Header h2
    CODE:
	RETVAL = rpmVersionCompare(h1, h2);
    OUTPUT:
    RETVAL

int
is_source_package(h)
	Header h
    CODE:
	RETVAL = !headerIsEntry(h, RPMTAG_SOURCERPM);
    OUTPUT:
	RETVAL

void
tagformat(h, format)
	Header h
	char * format
    PREINIT:
	char * s;
    PPCODE:
	s =  headerSprintf(h, format, rpmTagTable, rpmHeaderFormats, NULL);
	PUSHs(sv_2mortal(newSVpv((char *)s, 0)));
	s = _free(s);

# Write rpm header into file pointer
# fedora use HEADER_MAGIC_NO, too bad, set no_header_magic make the function
# compatible
int
write(h, fp, no_header_magic = 0)
    Header h
    FILE * fp
    int no_header_magic
    PREINIT:
    FD_t fd;
    CODE:
    RETVAL = 0;
    if (h) {
        if ((fd = fdDup(fileno(fp))) != NULL) {
            headerWrite(fd, h);
            Fclose(fd);
            RETVAL = 1;
        }
    }
    OUTPUT:
    RETVAL

void
hsize(h, no_header_magic = 0)
    Header h
    int no_header_magic
    PPCODE:
    XPUSHs(sv_2mortal(newSViv(headerSizeof(h))));
    
void
copy(h)
    Header h
    PREINIT:
    Header hcopy;
    PPCODE:
    hcopy = headerCopy(h);
    XPUSHs(sv_2mortal(sv_setref_pv(newSVpv("", 0), "RPM::Header", (void *)hcopy)));
#ifdef HDRPMMEM
    PRINTF_NEW("RPM::Header", hcopy, hcopy->nrefs);
#endif

void
string(h, no_header_magic = 0)
    Header h
    int no_header_magic
    PREINIT:
    char * string = NULL;
    int offset = 8; /* header magic length */
    char * ptr = NULL;
    int hsize = 0;
    PPCODE:
    hsize = headerSizeof(h);
    string = headerUnload(h, NULL);
    if (! no_header_magic) {
        ptr = malloc(hsize);
        memcpy(ptr, header_magic, 8);
        memcpy(ptr + 8, string, hsize - 8);
    }
    XPUSHs(sv_2mortal(newSVpv(ptr ? ptr : string, hsize)));
    free(string);
    free(ptr);

int
removetag(h, sv_tag)
    Header h
    SV * sv_tag
    PREINIT:
    rpmTag tag = -1;
    CODE:
    if (SvIOK(sv_tag)) {
        tag = SvIV(sv_tag);
    } else if (SvPOK(sv_tag)) {
        tag = tagValue(SvPV_nolen(sv_tag));
    }
    if (tag > 0)
        RETVAL = headerRemoveEntry(h, tag);
    else
        RETVAL = 1;
    OUTPUT:
    RETVAL

int
addtag(h, sv_tag, sv_tagtype, ...)
    Header h
    SV * sv_tag
    SV * sv_tagtype
    PREINIT:
    char * value;
    int ivalue;
    int i;
    rpmTag tag = -1;
    rpmTagType tagtype = RPM_NULL_TYPE;
    STRLEN len;
    CODE:
    if (SvIOK(sv_tag)) {
        tag = SvIV(sv_tag);
    } else if (SvPOK(sv_tag)) {
        tag = tagValue(SvPV_nolen(sv_tag));
    }
    tagtype = sv2constant(sv_tagtype, "rpmtagtype");
    if (tag > 0)
        RETVAL = 1;
    else
        RETVAL = 0;
    /* if (tag == RPMTAG_OLDFILENAMES)
        expandFilelist(h); */
    for (i = 3; (i < items) && RETVAL; i++) {
        switch (tagtype) {
            case RPM_CHAR_TYPE:
            case RPM_INT8_TYPE:
            case RPM_INT16_TYPE:
            case RPM_INT32_TYPE:
                ivalue = SvUV(ST(i));
                RETVAL = headerAddOrAppendEntry(h, tag, tagtype, &ivalue, 1);
                break;
            case RPM_BIN_TYPE:
                value = (char *)SvPV(ST(i), len);
                RETVAL = headerAddEntry(h, tag, tagtype, value, len);
                break;
            case RPM_STRING_ARRAY_TYPE:
                value = SvPV_nolen(ST(i));
                RETVAL = headerAddOrAppendEntry(h, tag, tagtype, &value, 1);
                break;
            default:
                value = SvPV_nolen(ST(i));
                RETVAL = headerAddOrAppendEntry(h, tag, tagtype, value, 1); 
                break;
        }
    }
    /* if (tag == RPMTAG_OLDFILENAMES) {
        compressFilelist(h); 
    } */
    OUTPUT:
    RETVAL
    
void
listtag(h)
    Header h
    PREINIT:
    HeaderIterator iterator;
    int tag;
    PPCODE:
    iterator = headerInitIterator(h);
    while (headerNextIterator(iterator, &tag, NULL, NULL, NULL)) {
        XPUSHs(sv_2mortal(newSViv(tag)));
    }
    headerFreeIterator(iterator);
    
int
hastag(h, sv_tag)
    Header h
    SV * sv_tag
    PREINIT:
    rpmTag tag = -1;
    CODE:
    if (SvIOK(sv_tag)) {
        tag = SvIV(sv_tag);
    } else if (SvPOK(sv_tag)) {
        tag = tagValue(SvPV_nolen(sv_tag));
    }    
    if (tag > 0)
        RETVAL = headerIsEntry(h, tag);
    else
        RETVAL = -1;
    OUTPUT:
    RETVAL
 
# Return the tag value in headers
void
tag(h, sv_tag)
    Header h
    SV * sv_tag
    PREINIT:
    void *ret = NULL;
    int type;
    int n;
    rpmTag tag = -1;
    PPCODE:
    if (SvIOK(sv_tag)) {
        tag = SvIV(sv_tag);
    } else if (SvPOK(sv_tag)) {
        tag = tagValue(SvPV_nolen(sv_tag));
    }
    if (tag > 0)
        if (headerGetEntry(h, tag, &type, &ret, &n)) {
            switch(type) {
                case RPM_STRING_ARRAY_TYPE:
                    {
                        int i;
                        char **s;

                        EXTEND(SP, n);
                        s = (char **)ret;
        
                        for (i = 0; i < n; i++) {
                            PUSHs(sv_2mortal(newSVpv(s[i], 0)));
                        }
                    }
                break;
                case RPM_STRING_TYPE:
                    PUSHs(sv_2mortal(newSVpv((char *)ret, 0)));
                break;
                case RPM_CHAR_TYPE:
                case RPM_INT8_TYPE:
                case RPM_INT16_TYPE:
                case RPM_INT32_TYPE:
                    {
                        int i;
                        int *r;

                        EXTEND(SP, n);
                        r = (int *)ret;

                        for (i = 0; i < n; i++) {
                            PUSHs(sv_2mortal(newSViv(r[i])));
                        }
                    }
                break;
                case RPM_BIN_TYPE:
                    PUSHs(sv_2mortal(newSVpv((char *)ret, n)));
                break;
                default:
                    croak("unknown rpm tag type %d", type);
            }
        }
    headerFreeTag(h, ret, type);

int
tagtype(h, sv_tag)
    Header h
    SV * sv_tag
    PREINIT:
    int type;
    rpmTag tag = -1;
    CODE:
    if (SvIOK(sv_tag)) {
        tag = SvIV(sv_tag);
    } else if (SvPOK(sv_tag)) {
        tag = tagValue(SvPV_nolen(sv_tag));
    }
    RETVAL = RPM_NULL_TYPE;
    if (tag > 0)
        if (headerGetEntry(h, tag, &type, NULL, NULL))
            RETVAL = type;
    OUTPUT:
    RETVAL
    
rpmds
dependencies(header, sv_tag)
    Header header
    SV * sv_tag
    PREINIT:
    rpmTag tag;
    CODE:
    tag = sv2constant(sv_tag, "rpmtag");
    RETVAL = rpmdsNew(header, tag, 0);
    if (RETVAL) RETVAL = rpmdsInit(RETVAL);
    OUTPUT:
    RETVAL

rpmfi
files(header, ts = NULL)
    Header header
    rpmts ts
    PREINIT:
    rpmfi fi;
    CODE:
    if (ts)
        ts = rpmtsLink(ts, "RPM::Header::files");
    else
        ts = rpmtsCreate();
    RETVAL = rpmfiNew(ts, header, RPMTAG_BASENAMES, 0);
    if (RETVAL != NULL) RETVAL = rpmfiInit(RETVAL, 0);
    ts = rpmtsFree(ts);
    OUTPUT:
    RETVAL
 

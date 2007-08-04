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

void
tag_by_id(h, tag)
	Header h
	int tag
    PREINIT:
	void *ret = NULL;
	int type;
	int n;
	int ok;
    PPCODE:
	ok = headerGetEntry(h, tag, &type, &ret, &n);

	if (!ok) {
		/* nop, empty stack */
	}
	else {
		switch(type)
		{
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
		default:
			croak("unknown rpm tag type %d", type);
		}
	}
	headerFreeData(ret, type);

int
_header_compare(h1, h2)
	Header h1
	Header h2
    CODE:
	RETVAL = rpmVersionCompare(h1, h2);
    OUTPUT:
        RETVAL

int
_header_is_source(h)
	Header h
    CODE:
	RETVAL = headerIsEntry(h, RPMTAG_SOURCEPACKAGE);
    OUTPUT:
	RETVAL

void
_header_sprintf(h, format)
	Header h
	char * format
    PREINIT:
	char * s;
    PPCODE:
	s =  headerSprintf(h, format, rpmTagTable, rpmHeaderFormats, NULL);
	PUSHs(sv_2mortal(newSVpv((char *)s, 0)));
	s = _free(s);


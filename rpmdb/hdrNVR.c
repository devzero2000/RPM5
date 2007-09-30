/** \ingroup rpmdb
 * \file rpmdb/hdrNVR.c
 */

#include "system.h"
#include <rpmlib.h>
#include <rpmio.h>
#include <rpmmacro.h>

#include "header_internal.h"		/* XXX hdrchkType(), hdrchkData() */

#include "debug.h"

/**
 * Macros to be defined from per-header tag values.
 * @todo Should other macros be added from header when installing a package?
 */
/*@observer@*/ /*@unchecked@*/
static struct tagMacro {
/*@observer@*/ /*@null@*/
    const char *macroname;	/*!< Macro name to define. */
    rpmTag	tag;		/*!< Header tag to use for value. */
} tagMacros[] = {
    { "name",		RPMTAG_NAME },
    { "version",	RPMTAG_VERSION },
    { "release",	RPMTAG_RELEASE },
    { "epoch",		RPMTAG_EPOCH },
    { "arch",		RPMTAG_ARCH },
    { "os",		RPMTAG_OS },
    { NULL, 0 }
};

/*@-globs -mods -incondefs@*/
int headerMacrosLoad(Header h)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies rpmGlobalMacroContext @*/
{
    struct tagMacro * tagm;
    union {
	const void * ptr;
/*@unused@*/
	const char ** argv;
	const char * str;
	int_32 * i32p;
    } body;
    char numbuf[32];
    int_32 type;
    int xx;

    /* XXX pre-expand %{buildroot} (if any) */
    {   const char *s = rpmExpand("%{?buildroot}", NULL);
	if (s && *s) 
	    (void) addMacro(NULL, "..buildroot", NULL, s, -1);
	s = _free(s);
    }
    {   const char *s = rpmExpand("%{?_builddir}", NULL);
	if (s && *s) 
	    (void) addMacro(NULL, ".._builddir", NULL, s, -1);
	s = _free(s);
    }

    for (tagm = tagMacros; tagm->macroname != NULL; tagm++) {
	xx = headerGetEntryMinMemory(h, tagm->tag, &type, (hPTR_t *) &body.ptr, NULL);
	if (!xx)
	    continue;
	switch (type) {
	case RPM_INT32_TYPE:
	    sprintf(numbuf, "%d", *body.i32p);
	    addMacro(NULL, tagm->macroname, NULL, numbuf, -1);
	    /*@switchbreak@*/ break;
	case RPM_STRING_TYPE:
	    addMacro(NULL, tagm->macroname, NULL, body.str, -1);
	    /*@switchbreak@*/ break;
	case RPM_STRING_ARRAY_TYPE:
	case RPM_I18NSTRING_TYPE:
	case RPM_BIN_TYPE:
	    body.ptr = headerFreeData(body.ptr, type);
	    /*@fallthrough@*/
	case RPM_NULL_TYPE:
	case RPM_CHAR_TYPE:
	case RPM_INT8_TYPE:
	case RPM_INT16_TYPE:
	default:
	    /*@switchbreak@*/ break;
	}
    }
    return 0;
}
/*@=globs =mods =incondefs@*/

/*@-globs -mods -incondefs@*/
int headerMacrosUnload(Header h)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies rpmGlobalMacroContext @*/
{
    struct tagMacro * tagm;
    union {
	const void * ptr;
/*@unused@*/
	const char ** argv;
	const char * str;
	int_32 * i32p;
    } body;
    int_32 type;
    int xx;

    for (tagm = tagMacros; tagm->macroname != NULL; tagm++) {
	xx = headerGetEntryMinMemory(h, tagm->tag, &type, (hPTR_t *) &body.ptr, NULL);
	if (!xx)
	    continue;
	switch (type) {
	case RPM_INT32_TYPE:
	    delMacro(NULL, tagm->macroname);
	    /*@switchbreak@*/ break;
	case RPM_STRING_TYPE:
	    delMacro(NULL, tagm->macroname);
	    /*@switchbreak@*/ break;
	case RPM_STRING_ARRAY_TYPE:
	case RPM_I18NSTRING_TYPE:
	case RPM_BIN_TYPE:
	    body.ptr = headerFreeData(body.ptr, type);
	    /*@fallthrough@*/
	case RPM_NULL_TYPE:
	case RPM_CHAR_TYPE:
	case RPM_INT8_TYPE:
	case RPM_INT16_TYPE:
	default:
	    /*@switchbreak@*/ break;
	}
    }

    /* XXX restore previous %{buildroot} (if any) */
    {   const char *s = rpmExpand("%{?_builddir}", NULL);
	if (s && *s) 
	    (void) delMacro(NULL, "_builddir");
	s = _free(s);
    }
    {   const char *s = rpmExpand("%{?buildroot}", NULL);
	if (s && *s) 
	    (void) delMacro(NULL, "buildroot");
	s = _free(s);
    }

    return 0;
}
/*@=globs =mods =incondefs@*/

int headerNEVRA(Header h, const char **np,
		/*@unused@*/ const char **ep, const char **vp, const char **rp,
		const char **ap)
{
    int type;
    int count;

    if (np) {
	if (!(headerGetEntry(h, RPMTAG_NAME, &type, np, &count)
	    && type == RPM_STRING_TYPE && count == 1))
		*np = NULL;
    }
    if (vp) {
	if (!(headerGetEntry(h, RPMTAG_VERSION, &type, vp, &count)
	    && type == RPM_STRING_TYPE && count == 1))
		*vp = NULL;
    }
    if (rp) {
	if (!(headerGetEntry(h, RPMTAG_RELEASE, &type, rp, &count)
	    && type == RPM_STRING_TYPE && count == 1))
		*rp = NULL;
    }
    if (ap) {
/*@-observertrans -readonlytrans@*/
	if (!headerIsEntry(h, RPMTAG_ARCH))
	    *ap = "pubkey";
	else
	if (!headerIsEntry(h, RPMTAG_SOURCERPM))
	    *ap = "src";
/*@=observertrans =readonlytrans@*/
	else
	if (!(headerGetEntry(h, RPMTAG_ARCH, &type, ap, &count)
	    && type == RPM_STRING_TYPE && count == 1))
		*ap = NULL;
    }
    return 0;
}

uint_32 hGetColor(Header h)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    uint_32 hcolor = 0;
    uint_32 * fcolors;
    int_32 ncolors;
    int i;

    fcolors = NULL;
    ncolors = 0;
    if (hge(h, RPMTAG_FILECOLORS, NULL, &fcolors, &ncolors)
     && fcolors != NULL && ncolors > 0)
    {
	for (i = 0; i < ncolors; i++)
	    hcolor |= fcolors[i];
    }
    hcolor &= 0x0f;

    return hcolor;
}

void headerMergeLegacySigs(Header h, const Header sigh)
{
    HFD_t hfd = (HFD_t) headerFreeData;
    HeaderIterator hi;
    int_32 tag, type, count;
    const void * ptr;
    int xx;

    if (h == NULL || sigh == NULL)
	return;

    for (hi = headerInitIterator(sigh);
        headerNextIterator(hi, &tag, &type, &ptr, &count);
        ptr = hfd(ptr, type))
    {
	switch (tag) {
	/* XXX Translate legacy signature tag values. */
	case RPMSIGTAG_SIZE:
	    tag = RPMTAG_SIGSIZE;
	    /*@switchbreak@*/ break;
#if defined(SUPPORT_RPMV3_BROKEN)
	case RPMSIGTAG_LEMD5_1:
	    tag = RPMTAG_SIGLEMD5_1;
	    /*@switchbreak@*/ break;
	case RPMSIGTAG_LEMD5_2:
	    tag = RPMTAG_SIGLEMD5_2;
	    /*@switchbreak@*/ break;
#endif
#if defined(SUPPORT_RPMV3_VERIFY_RSA)
	case RPMSIGTAG_PGP:
	    tag = RPMTAG_SIGPGP;
	    /*@switchbreak@*/ break;
	case RPMSIGTAG_PGP5:
	    tag = RPMTAG_SIGPGP5;
	    /*@switchbreak@*/ break;
#endif
	case RPMSIGTAG_MD5:
	    tag = RPMTAG_SIGMD5;
	    /*@switchbreak@*/ break;
#if defined(SUPPORT_RPMV3_VERIFY_DSA)
	case RPMSIGTAG_GPG:
	    tag = RPMTAG_SIGGPG;
	    /*@switchbreak@*/ break;
#endif
	case RPMSIGTAG_PAYLOADSIZE:
	    tag = RPMTAG_ARCHIVESIZE;
	    /*@switchbreak@*/ break;
	case RPMSIGTAG_SHA1:
	case RPMSIGTAG_DSA:
	case RPMSIGTAG_RSA:
	default:
	    if (!(tag >= HEADER_SIGBASE && tag < HEADER_TAGBASE))
		continue;
	    /*@switchbreak@*/ break;
	}
	if (ptr == NULL) continue;	/* XXX can't happen */
	if (!headerIsEntry(h, tag)) {
	    if (hdrchkType(type))
		continue;
	    if (count < 0 || hdrchkData(count))
		continue;
	    switch(type) {
	    case RPM_NULL_TYPE:
		continue;
		/*@notreached@*/ /*@switchbreak@*/ break;
	    case RPM_CHAR_TYPE:
	    case RPM_INT8_TYPE:
	    case RPM_INT16_TYPE:
	    case RPM_INT32_TYPE:
		if (count != 1)
		    continue;
		/*@switchbreak@*/ break;
	    case RPM_STRING_TYPE:
	    case RPM_BIN_TYPE:
		if (count >= 16*1024)
		    continue;
		/*@switchbreak@*/ break;
	    case RPM_STRING_ARRAY_TYPE:
	    case RPM_I18NSTRING_TYPE:
		continue;
		/*@notreached@*/ /*@switchbreak@*/ break;
	    }
 	    xx = headerAddEntry(h, tag, type, ptr, count);
	}
    }
    hi = headerFreeIterator(hi);
}

Header headerRegenSigHeader(const Header h, int noArchiveSize)
{
    HFD_t hfd = (HFD_t) headerFreeData;
    Header sigh = headerNew();
    HeaderIterator hi;
    int_32 tag, stag, type, count;
    const void * ptr;
    int xx;

    for (hi = headerInitIterator(h);
        headerNextIterator(hi, &tag, &type, &ptr, &count);
        ptr = hfd(ptr, type))
    {
	switch (tag) {
	/* XXX Translate legacy signature tag values. */
	case RPMTAG_SIGSIZE:
	    stag = RPMSIGTAG_SIZE;
	    /*@switchbreak@*/ break;
#if defined(SUPPORT_RPMV3_BROKEN)
	case RPMTAG_SIGLEMD5_1:
	    stag = RPMSIGTAG_LEMD5_1;
	    /*@switchbreak@*/ break;
	case RPMTAG_SIGLEMD5_2:
	    stag = RPMSIGTAG_LEMD5_2;
	    /*@switchbreak@*/ break;
#endif
#if defined(SUPPORT_RPMV3_VERIFY_RSA)
	case RPMTAG_SIGPGP:
	    stag = RPMSIGTAG_PGP;
	    /*@switchbreak@*/ break;
	case RPMTAG_SIGPGP5:
	    stag = RPMSIGTAG_PGP5;
	    /*@switchbreak@*/ break;
#endif
	case RPMTAG_SIGMD5:
	    stag = RPMSIGTAG_MD5;
	    /*@switchbreak@*/ break;
#if defined(SUPPORT_RPMV3_VERIFY_DSA)
	case RPMTAG_SIGGPG:
	    stag = RPMSIGTAG_GPG;
	    /*@switchbreak@*/ break;
#endif
	case RPMTAG_ARCHIVESIZE:
	    /* XXX rpm-4.1 and later has archive size in signature header. */
	    if (noArchiveSize)
		continue;
	    stag = RPMSIGTAG_PAYLOADSIZE;
	    /*@switchbreak@*/ break;
	case RPMTAG_SHA1HEADER:
	case RPMTAG_DSAHEADER:
	case RPMTAG_RSAHEADER:
	default:
	    if (!(tag >= HEADER_SIGBASE && tag < HEADER_TAGBASE))
		continue;
	    stag = tag;
	    /*@switchbreak@*/ break;
	}
	if (ptr == NULL) continue;	/* XXX can't happen */
	if (!headerIsEntry(sigh, stag))
	    xx = headerAddEntry(sigh, stag, type, ptr, count);
    }
    hi = headerFreeIterator(hi);
    return sigh;
}

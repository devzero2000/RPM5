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
    HGE_t hge = (HGE_t)headerGetExtension;
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    struct tagMacro * tagm;
    char numbuf[64];
    const char * val;
    uint64_t ival;
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
	he->tag = tagm->tag;
	xx = hge(h, he, 0);
	if (!xx)
	    continue;
	val = NULL;
	ival = 0;
	switch (he->t) {
	case RPM_CHAR_TYPE:
	case RPM_UINT8_TYPE:
	    ival = he->p.ui8p[0];
	    val = numbuf;
	    /*@switchbreak@*/ break;
	case RPM_UINT16_TYPE:
	    ival = he->p.ui16p[0];
	    val = numbuf;
	    /*@switchbreak@*/ break;
	case RPM_UINT32_TYPE:
	    ival = he->p.ui32p[0];
	    val = numbuf;
	    /*@switchbreak@*/ break;
	case RPM_UINT64_TYPE:
	    ival = he->p.ui64p[0];
	    val = numbuf;
	    /*@switchbreak@*/ break;
	case RPM_STRING_TYPE:
	    val = he->p.str;
	    /*@switchbreak@*/ break;
	case RPM_STRING_ARRAY_TYPE:
	case RPM_I18NSTRING_TYPE:
	case RPM_BIN_TYPE:
	case RPM_NULL_TYPE:
	default:
	    /*@switchbreak@*/ break;
	}
	
	if (val) {
	    if (val == numbuf)
		sprintf(numbuf, "%llu", ival);
	    addMacro(NULL, tagm->macroname, NULL, val, -1);
	}
	he->p.ptr = _free(he->p.ptr);
    }
    return 0;
}
/*@=globs =mods =incondefs@*/

/*@-globs -mods -incondefs@*/
int headerMacrosUnload(Header h)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies rpmGlobalMacroContext @*/
{
    HGE_t hge = (HGE_t)headerGetExtension;
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    struct tagMacro * tagm;
    int xx;

    for (tagm = tagMacros; tagm->macroname != NULL; tagm++) {
	he->tag = tagm->tag;
	xx = hge(h, he, 0);
	if (!xx)
	    continue;
	switch (he->t) {
	case RPM_UINT32_TYPE:
	    delMacro(NULL, tagm->macroname);
	    /*@switchbreak@*/ break;
	case RPM_STRING_TYPE:
	    delMacro(NULL, tagm->macroname);
	    /*@switchbreak@*/ break;
	case RPM_STRING_ARRAY_TYPE:
	case RPM_I18NSTRING_TYPE:
	case RPM_BIN_TYPE:
	case RPM_NULL_TYPE:
	case RPM_CHAR_TYPE:
	case RPM_UINT8_TYPE:
	case RPM_UINT16_TYPE:
	default:
	    /*@switchbreak@*/ break;
	}
	he->p.ptr = _free(he->p.ptr);
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
    rpmTagType t;
    rpmTagData p;
    rpmTagCount c;

    if (np) {
	if (headerGetEntry(h, RPMTAG_NAME, &t, &p, &c)
	 && t == RPM_STRING_TYPE && c == 1)
	    *np = p.str;
	else
	    *np = NULL;
    }
    if (vp) {
	if (headerGetEntry(h, RPMTAG_VERSION, &t, &p, &c)
	 && t == RPM_STRING_TYPE && c == 1)
	    *vp = p.str;
	else
	    *vp = NULL;
    }
    if (rp) {
	if (headerGetEntry(h, RPMTAG_RELEASE, &t, &p, &c)
	 && t == RPM_STRING_TYPE && c == 1)
	    *rp = p.str;
	else
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
	if (headerGetEntry(h, RPMTAG_ARCH, &t, &p, &c)
	 && t == RPM_STRING_TYPE && c == 1)
	    *ap = p.str;
	else
	    *ap = NULL;
    }
    return 0;
}

uint_32 hGetColor(Header h)
{
    HGE_t hge = (HGE_t)headerGetExtension;
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    uint_32 hcolor = 0;
    int xx;

    he->tag = RPMTAG_FILECOLORS;
    xx = hge(h, he, 0);
    if (xx && he->p.ptr != NULL && he->c > 0) {
	int i;
	for (i = 0; i < he->c; i++)
	    hcolor |= he->p.ui32p[i];
    }
    he->p.ptr = _free(he->p.ptr);
    hcolor &= 0x0f;

    return hcolor;
}

void headerMergeLegacySigs(Header h, const Header sigh)
{
    HAE_t hae = headerAddExtension;
    HFD_t hfd = (HFD_t) headerFreeData;
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    HeaderIterator hi;
    int xx;

    if (h == NULL || sigh == NULL)
	return;

    for (hi = headerInitIterator(sigh);
        headerNextIterator(hi, &he->tag, &he->t, &he->p, &he->c);
        he->p.ptr = hfd(he->p.ptr, he->t))
    {
	/* XXX Translate legacy signature tag values. */
	switch (he->tag) {
	case RPMSIGTAG_SIZE:
	    he->tag = RPMTAG_SIGSIZE;
	    /*@switchbreak@*/ break;
#if defined(SUPPORT_RPMV3_BROKEN)
	case RPMSIGTAG_LEMD5_1:
	    he->tag = RPMTAG_SIGLEMD5_1;
	    /*@switchbreak@*/ break;
	case RPMSIGTAG_LEMD5_2:
	    he->tag = RPMTAG_SIGLEMD5_2;
	    /*@switchbreak@*/ break;
#endif
#if defined(SUPPORT_RPMV3_VERIFY_RSA)
	case RPMSIGTAG_PGP:
	    he->tag = RPMTAG_SIGPGP;
	    /*@switchbreak@*/ break;
	case RPMSIGTAG_PGP5:
	    he->tag = RPMTAG_SIGPGP5;
	    /*@switchbreak@*/ break;
#endif
	case RPMSIGTAG_MD5:
	    he->tag = RPMTAG_SIGMD5;
	    /*@switchbreak@*/ break;
#if defined(SUPPORT_RPMV3_VERIFY_DSA)
	case RPMSIGTAG_GPG:
	    he->tag = RPMTAG_SIGGPG;
	    /*@switchbreak@*/ break;
#endif
	case RPMSIGTAG_PAYLOADSIZE:
	    he->tag = RPMTAG_ARCHIVESIZE;
	    /*@switchbreak@*/ break;
	case RPMSIGTAG_SHA1:
	case RPMSIGTAG_DSA:
	case RPMSIGTAG_RSA:
	default:
	    /* Skip all unknown tags that are not in the signature tag range. */
	    if (!(he->tag >= HEADER_SIGBASE && he->tag < HEADER_TAGBASE))
		continue;
	    /*@switchbreak@*/ break;
	}
assert(he->p.ptr != NULL);
	if (!headerIsEntry(h, he->tag)) {
	    if (hdrchkType(he->t))
		continue;
	    if (he->c < 0 || hdrchkData(he->c))
		continue;
	    switch(he->t) {
	    default:
assert(0);	/* XXX keep gcc quiet */
		/*@switchbreak@*/ break;
	    case RPM_CHAR_TYPE:
	    case RPM_UINT8_TYPE:
	    case RPM_UINT16_TYPE:
	    case RPM_UINT32_TYPE:
	    case RPM_UINT64_TYPE:
		if (he->c != 1)
		    continue;
		/*@switchbreak@*/ break;
	    case RPM_STRING_TYPE:
	    case RPM_BIN_TYPE:
		if (he->c >= 16*1024)
		    continue;
		/*@switchbreak@*/ break;
	    case RPM_STRING_ARRAY_TYPE:
	    case RPM_I18NSTRING_TYPE:
		continue;
		/*@notreached@*/ /*@switchbreak@*/ break;
	    }
 	    xx = hae(h, he, 0);
assert(xx == 1);
	}
    }
    hi = headerFreeIterator(hi);
}

Header headerRegenSigHeader(const Header h, int noArchiveSize)
{
    HAE_t hae = headerAddExtension;
    HFD_t hfd = (HFD_t) headerFreeData;
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    Header sigh = headerNew();
    HeaderIterator hi;
    int xx;

    for (hi = headerInitIterator(h);
        headerNextIterator(hi, &he->tag, &he->t, &he->p, &he->c);
        he->p.ptr = hfd(he->p.ptr, he->t))
    {
	/* XXX Translate legacy signature tag values. */
	switch (he->tag) {
	case RPMTAG_SIGSIZE:
	    he->tag = RPMSIGTAG_SIZE;
	    /*@switchbreak@*/ break;
#if defined(SUPPORT_RPMV3_BROKEN)
	case RPMTAG_SIGLEMD5_1:
	    he->tag = RPMSIGTAG_LEMD5_1;
	    /*@switchbreak@*/ break;
	case RPMTAG_SIGLEMD5_2:
	    he->tag = RPMSIGTAG_LEMD5_2;
	    /*@switchbreak@*/ break;
#endif
#if defined(SUPPORT_RPMV3_VERIFY_RSA)
	case RPMTAG_SIGPGP:
	    he->tag = RPMSIGTAG_PGP;
	    /*@switchbreak@*/ break;
	case RPMTAG_SIGPGP5:
	    he->tag = RPMSIGTAG_PGP5;
	    /*@switchbreak@*/ break;
#endif
	case RPMTAG_SIGMD5:
	    he->tag = RPMSIGTAG_MD5;
	    /*@switchbreak@*/ break;
#if defined(SUPPORT_RPMV3_VERIFY_DSA)
	case RPMTAG_SIGGPG:
	    he->tag = RPMSIGTAG_GPG;
	    /*@switchbreak@*/ break;
#endif
	case RPMTAG_ARCHIVESIZE:
	    /* XXX rpm-4.1 and later has archive size in signature header. */
	    if (noArchiveSize)
		continue;
	    he->tag = RPMSIGTAG_PAYLOADSIZE;
	    /*@switchbreak@*/ break;
	case RPMTAG_SHA1HEADER:
	case RPMTAG_DSAHEADER:
	case RPMTAG_RSAHEADER:
	default:
	    /* Skip all unknown tags that are not in the signature tag range. */
	    if (!(he->tag >= HEADER_SIGBASE && he->tag < HEADER_TAGBASE))
		continue;
	    /*@switchbreak@*/ break;
	}
assert(he->p.ptr != NULL);
	if (!headerIsEntry(sigh, he->tag)) {
	    xx = hae(sigh, he, 0);
assert(xx == 1);
	}
    }
    hi = headerFreeIterator(hi);
    return sigh;
}

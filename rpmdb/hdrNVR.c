/** \ingroup rpmdb
 * \file rpmdb/hdrNVR.c
 */

#include "system.h"
#include <rpmlib.h>
#include <rpmmacro.h>
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

int headerMacrosLoad(Header h)
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
/*@-boundsread@*/
	    sprintf(numbuf, "%d", *body.i32p);
/*@=boundsread@*/
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
int headerMacrosUnload(Header h)
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

int headerNVR(Header h, const char **np, const char **vp, const char **rp)
{
    int type;
    int count;

/*@-boundswrite@*/
    if (np) {
	if (!(headerGetEntry(h, RPMTAG_NAME, &type, (void **) np, &count)
	    && type == RPM_STRING_TYPE && count == 1))
		*np = NULL;
    }
    if (vp) {
	if (!(headerGetEntry(h, RPMTAG_VERSION, &type, (void **) vp, &count)
	    && type == RPM_STRING_TYPE && count == 1))
		*vp = NULL;
    }
    if (rp) {
	if (!(headerGetEntry(h, RPMTAG_RELEASE, &type, (void **) rp, &count)
	    && type == RPM_STRING_TYPE && count == 1))
		*rp = NULL;
    }
/*@=boundswrite@*/
    return 0;
}

int headerNEVRA(Header h, const char **np,
		/*@unused@*/ const char **ep, const char **vp, const char **rp,
		const char **ap)
{
    int type;
    int count;

/*@-boundswrite@*/
    if (np) {
	if (!(headerGetEntry(h, RPMTAG_NAME, &type, (void **) np, &count)
	    && type == RPM_STRING_TYPE && count == 1))
		*np = NULL;
    }
    if (vp) {
	if (!(headerGetEntry(h, RPMTAG_VERSION, &type, (void **) vp, &count)
	    && type == RPM_STRING_TYPE && count == 1))
		*vp = NULL;
    }
    if (rp) {
	if (!(headerGetEntry(h, RPMTAG_RELEASE, &type, (void **) rp, &count)
	    && type == RPM_STRING_TYPE && count == 1))
		*rp = NULL;
    }
    if (ap) {
	if (!(headerGetEntry(h, RPMTAG_ARCH, &type, (void **) ap, &count)
	    && type == RPM_STRING_TYPE && count == 1))
		*ap = NULL;
    }
/*@=boundswrite@*/
    return 0;
}

char * hGetNEVR(Header h, const char ** np)
{
    const char * n, * v, * r;
    char * NVR, * t;

    (void) headerNVR(h, &n, &v, &r);
    NVR = t = xcalloc(1, strlen(n) + strlen(v) + strlen(r) + sizeof("--"));
/*@-boundswrite@*/
    t = stpcpy(t, n);
    t = stpcpy(t, "-");
    t = stpcpy(t, v);
    t = stpcpy(t, "-");
    t = stpcpy(t, r);
    if (np)
	*np = n;
/*@=boundswrite@*/
    return NVR;
}

char * hGetNEVRA(Header h, const char ** np)
{
    const char * n, * v, * r, * a;
    char * NVRA, * t;

    (void) headerNVR(h, &n, &v, &r);
    /* XXX pubkeys have no arch. */
/*@-branchstate@*/
    a = NULL;
    if (!headerGetEntry(h, RPMTAG_ARCH, NULL, &a, NULL) || a == NULL)
	a = "pubkey";
/*@=branchstate@*/
    NVRA = t = xcalloc(1, strlen(n) + strlen(v) + strlen(r) + strlen(a) + sizeof("--."));
/*@-boundswrite@*/
    t = stpcpy(t, n);
    t = stpcpy(t, "-");
    t = stpcpy(t, v);
    t = stpcpy(t, "-");
    t = stpcpy(t, r);
    t = stpcpy(t, ".");
    t = stpcpy(t, a);
    if (np)
	*np = n;
/*@=boundswrite@*/
    return NVRA;
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
/*@-boundsread@*/
	for (i = 0; i < ncolors; i++)
	    hcolor |= fcolors[i];
/*@=boundsread@*/
    }
    hcolor &= 0x0f;

    return hcolor;
}

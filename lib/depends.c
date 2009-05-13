/** \ingroup rpmts
 * \file lib/depends.c
 */

#include "system.h"

#include <rpmio.h>
#include <rpmiotypes.h>		/* XXX fnpyKey */
#include <rpmcb.h>
#include <rpmmacro.h>		/* XXX rpmExpand("%{_dependency_whiteout}" */
#include <envvar.h>
#include <ugid.h>		/* XXX user()/group() probes */

#include <rpmtag.h>
#define	_RPMDB_INTERNAL		/* XXX response cache needs dbiOpen et al. */
#include <rpmdb.h>

#define	_RPMTE_INTERNAL
#include <rpmte.h>
#define	_RPMTS_INTERNAL
#include <rpmcli.h>		/* XXX rpmcliPackagesTotal */

#define	_RPMEVR_INTERNAL
#include <rpmds.h>
#include <rpmfi.h>

#include "debug.h"

/*@access tsortInfo @*/
/*@access rpmte @*/		/* XXX for install <-> erase associate. */
/*@access rpmts @*/
/*@access rpmDiskSpaceInfo @*/

/*@access alKey @*/	/* XXX for reordering and RPMAL_NOMATCH assign */

/**
 */
typedef /*@abstract@*/ struct orderListIndex_s *	orderListIndex;
/*@access orderListIndex@*/

/**
 */
struct orderListIndex_s {
/*@dependent@*/
    alKey pkgKey;
    int orIndex;
};

#if defined(CACHE_DEPENDENCY_RESULT)
/*@unchecked@*/
int _cacheDependsRC = CACHE_DEPENDENCY_RESULT;
#endif

/*@observer@*/ /*@unchecked@*/
const char *rpmNAME = PACKAGE;

/*@observer@*/ /*@unchecked@*/
const char *rpmEVR = VERSION;

/*@unchecked@*/
int rpmFLAGS = RPMSENSE_EQUAL;

/**
 * Compare removed package instances (qsort/bsearch).
 * @param a		1st instance address
 * @param b		2nd instance address
 * @return		result of comparison
 */
static int intcmp(const void * a, const void * b)
	/*@requires maxRead(a) == 0 /\ maxRead(b) == 0 @*/
{
    const int * aptr = a;
    const int * bptr = b;
    int rc = (*aptr - *bptr);
    return rc;
}

/**
 * Add removed package instance to ordered transaction set.
 * @param ts		transaction set
 * @param h		header
 * @param dboffset	rpm database instance
 * @retval *indexp	removed element index (if not NULL)
 * @param depends	installed package of pair (or RPMAL_NOMATCH on erase)
 * @return		0 on success
 */
static int removePackage(rpmts ts, Header h, int dboffset,
		/*@null@*/ int * indexp,
		/*@exposed@*/ /*@dependent@*/ /*@null@*/ alKey depends)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, h, *indexp, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    rpmte p;

    /* Filter out duplicate erasures. */
    if (ts->numRemovedPackages > 0 && ts->removedPackages != NULL) {
	int * needle = NULL;
	needle = bsearch(&dboffset, ts->removedPackages, ts->numRemovedPackages,
			sizeof(*ts->removedPackages), intcmp);
	if (needle != NULL) {
	    /* XXX lastx should be per-call, not per-ts. */
	    if (indexp != NULL)
	        *indexp = needle - ts->removedPackages;
	    return 0;
	}
    }

    if (ts->numRemovedPackages == ts->allocedRemovedPackages) {
	ts->allocedRemovedPackages += ts->delta;
	ts->removedPackages = xrealloc(ts->removedPackages,
		sizeof(ts->removedPackages) * ts->allocedRemovedPackages);
    }

    if (ts->removedPackages != NULL) {	/* XXX can't happen. */
	ts->removedPackages[ts->numRemovedPackages] = dboffset;
	ts->numRemovedPackages++;
	if (ts->numRemovedPackages > 1)
	    qsort(ts->removedPackages, ts->numRemovedPackages,
			sizeof(*ts->removedPackages), intcmp);
    }

    if (ts->orderCount >= ts->orderAlloced) {
	ts->orderAlloced += (ts->orderCount - ts->orderAlloced) + ts->delta;
/*@-type +voidabstract @*/
	ts->order = xrealloc(ts->order, sizeof(*ts->order) * ts->orderAlloced);
/*@=type =voidabstract @*/
    }

    p = rpmteNew(ts, h, TR_REMOVED, NULL, NULL, dboffset, depends);
    ts->order[ts->orderCount] = p;
    if (indexp != NULL)
	*indexp = ts->orderCount;
    ts->orderCount++;

/*@-nullstate@*/	/* XXX FIX: ts->order[] can be NULL. */
   return 0;
/*@=nullstate@*/
}

/**
 * Are two headers identical?
 * @param first		first header
 * @param second	second header
 * @return		1 if headers are identical, 0 otherwise
 */
static int rpmHeadersIdentical(Header first, Header second)
	/*@globals internalState @*/
	/*@modifies internalState @*/
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    const char * one, * two;
    int rc = 0;
    int xx;

    he->tag = RPMTAG_HDRID;
    xx = headerGet(first, he, 0);
    one = he->p.str;
    he->tag = RPMTAG_HDRID;
    xx = headerGet(second, he, 0);
    two = he->p.str;

    if (one && two)
	rc = ((strcmp(one, two) == 0) ? 1 : 0);
    else if (one && !two)
	rc = 0;
    else if (!one && two)
	rc = 0;
    else {
	/* XXX Headers w/o digests case devolves to NEVR comparison. */
	rpmds A = rpmdsThis(first, RPMTAG_REQUIRENAME, RPMSENSE_EQUAL);
	rpmds B = rpmdsThis(second, RPMTAG_REQUIRENAME, RPMSENSE_EQUAL);
	rc = rpmdsCompare(A, B);
	(void)rpmdsFree(A);
	A = NULL;
	(void)rpmdsFree(B);
	B = NULL;
    }
    one = _free(one);
    two = _free(two);
    return rc;
}

/*@unchecked@*/
static rpmTag _upgrade_tag;
/*@unchecked@*/
static rpmTag _debuginfo_tag;
/*@unchecked@*/
static rpmTag _obsolete_tag;

/**
 * Add upgrade erasures to a transaction set.
 * @param ts		transaction set
 * @param p		transaction element
 * @param hcolor	header color
 * @param h		header
 * @return		0 on success
 */
static int rpmtsAddUpgrades(rpmts ts, rpmte p, rpmuint32_t hcolor, Header h)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, p, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    rpmuint32_t tscolor = rpmtsColor(ts);
    alKey pkgKey = rpmteAddedKey(p);
    rpmuint32_t ohcolor;
    rpmdbMatchIterator mi;
    Header oh;
    int xx;

    if (_upgrade_tag == 0) {
	const char * t = rpmExpand("%{?_upgrade_tag}", NULL);
/*@-mods@*/
	_upgrade_tag = (!strcmp(t, "name") ? RPMTAG_NAME : RPMTAG_PROVIDENAME);
/*@=mods@*/
	t = _free(t);
    }

    mi = rpmtsInitIterator(ts, _upgrade_tag, rpmteN(p), 0);
    while((oh = rpmdbNextIterator(mi)) != NULL) {
	int lastx;
	rpmte q;

	/* Ignore colored packages not in our rainbow. */
	ohcolor = hGetColor(oh);
	if (tscolor && hcolor && ohcolor && !(hcolor & ohcolor))
	    continue;

	/* Snarf the original install tid & time from older package(s). */
	he->tag = RPMTAG_ORIGINTID;
	xx = headerGet(oh, he, 0);
	if (xx && he->p.ui32p != NULL) {
	    if (p->originTid[0] == 0 || p->originTid[0] > he->p.ui32p[0]
	     || (he->c > 1 && p->originTid[0] == he->p.ui32p[0] && p->originTid[1] > he->p.ui32p[1]))
	    {
		p->originTid[0] = he->p.ui32p[0];
		p->originTid[1] = (he->c > 1 ? he->p.ui32p[1] : 0);
	    }
	    he->p.ptr = _free(he->p.ptr);
	}
	he->tag = RPMTAG_ORIGINTIME;
	xx = headerGet(oh, he, 0);
	if (xx && he->p.ui32p != NULL) {
	    if (p->originTime[0] == 0 || p->originTime[0] > he->p.ui32p[0]
	     || (he->c > 1 && p->originTime[0] == he->p.ui32p[0] && p->originTime[1] > he->p.ui32p[1]))
	    {
		p->originTime[0] = he->p.ui32p[0];
		p->originTime[1] = (he->c > 1 ? he->p.ui32p[1] : 0);
	    }
	    he->p.ptr = _free(he->p.ptr);
	}

#if defined(RPM_VENDOR_WINDRIVER)
	/*
	 * If we're capable of installing multiple colors
	 * but at least one of the packages are white (0), we
	 * further verify the arch is the same (or compatible) to trigger an upgrade
	 * we do have a special case to allow upgrades of noarch w/ a arch package
	 */
	if (tscolor && (!hcolor || !ohcolor)) {
	    const char * arch;
	    const char * oharch;
	    he->tag = RPMTAG_ARCH;
	    xx = headerGet(h, he, 0);
	    arch = (xx && he->p.str != NULL ? he->p.str : NULL);
	    he->tag = RPMTAG_ARCH;
	    xx = headerGet(oh, he, 0);
	    oharch = (xx && he->p.str != NULL ? he->p.str : NULL);
	    if (arch != NULL && oharch != NULL) {
	        if (strcmp("noarch", arch) || strcmp("noarch", oharch)) {
		    if (!_isCompatibleArch(arch, oharch)) {
			arch = _free(arch);
			oharch = _free(oharch);
			continue;
		    }
		}
	    }
	    arch = _free(arch);
	    oharch = _free(oharch);
	}
#endif

	/* Skip identical packages. */
	if (rpmHeadersIdentical(h, oh))
	    continue;

	/* Create an erasure element. */
	lastx = -1;
	xx = removePackage(ts, oh, rpmdbGetIteratorOffset(mi), &lastx, pkgKey);
assert(lastx >= 0 && lastx < ts->orderCount);
	q = ts->order[lastx];

	/* Chain through upgrade flink. */
	xx = rpmteChain(p, q, oh, "Upgrades");

/*@-nullptrarith@*/
	rpmlog(RPMLOG_DEBUG, D_("   upgrade erases %s\n"), rpmteNEVRA(q));
/*@=nullptrarith@*/

    }
    mi = rpmdbFreeIterator(mi);

    return 0;
}

/**
 * Check string for a suffix.
 * @param fn		string
 * @param suffix	suffix
 * @return		1 if string ends with suffix
 */
static inline int chkSuffix(const char * fn, const char * suffix)
        /*@*/
{
    size_t flen = strlen(fn);
    size_t slen = strlen(suffix);
    return (flen > slen && !strcmp(fn + flen - slen, suffix));
}

/**
 * Add unreferenced debuginfo erasures to a transaction set.
 * @param ts		transaction set
 * @param p		transaction element
 * @param h		header
 * @param pkgKey	added package key (erasure uses RPMAL_NOKEY)
 * @return		no. of references from build set
 */
static int rpmtsEraseDebuginfo(rpmts ts, rpmte p, Header h,
		/*@exposed@*/ /*@dependent@*/ /*@null@*/ alKey pkgKey)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, p, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    const void *keyval = NULL;
    size_t keylen = 0;
    size_t nrefs = 0;
    rpmuint32_t debuginfoInstance = 0;
    Header debuginfoHeader = NULL;
    rpmdbMatchIterator mi;
    Header oh;
    int xx;

    /* XXX SOURCEPKGID is not populated reliably, do not use (yet). */
    if (_debuginfo_tag == 0) {
	const char * t = rpmExpand("%{?_debuginfo_tag}", NULL);
/*@-mods@*/
	_debuginfo_tag = (*t != '\0' && !strcmp(t, "pkgid")
		? RPMTAG_SOURCEPKGID : RPMTAG_SOURCERPM);
/*@=mods@*/
	t = _free(t);
    }

    /* Grab the retrieval key. */
    switch (_debuginfo_tag) {
    default:		return 0;	/*@notreached@*/	break;
    case RPMTAG_SOURCERPM:	keyval = rpmteSourcerpm(p);	break;
    }

    /* Count remaining members in build set, excluding -debuginfo (if any). */
    mi = rpmtsInitIterator(ts, _debuginfo_tag, keyval, keylen);
    xx = rpmdbPruneIterator(mi, ts->removedPackages, ts->numRemovedPackages, 1);
    while((oh = rpmdbNextIterator(mi)) != NULL) {
	/* Skip identical packages. */
	if (rpmHeadersIdentical(h, oh))
	    continue;

	he->tag = RPMTAG_NAME;
	xx = headerGet(oh, he, 0);
	if (!xx || he->p.str == NULL)
	    continue;
	/* Save the -debuginfo member. */
	if (chkSuffix(he->p.str, "-debuginfo")) {
	    debuginfoInstance = rpmdbGetIteratorOffset(mi);
	    debuginfoHeader = headerLink(oh);
	} else
	    nrefs++;
	he->p.str = _free(he->p.str);
    }
    mi = rpmdbFreeIterator(mi);

    /* Remove -debuginfo package when last build member is erased. */
    if (nrefs == 0 && debuginfoInstance > 0 && debuginfoHeader != NULL) {
	int lastx = -1;
	rpmte q;

	/* Create an erasure element. */
	lastx = -1;
	xx = removePackage(ts, debuginfoHeader, debuginfoInstance,
		&lastx, pkgKey);
assert(lastx >= 0 && lastx < ts->orderCount);
	q = ts->order[lastx];

	/* Chain through upgrade flink. */
	/* XXX avoid assertion failure when erasing. */
	if (pkgKey != RPMAL_NOMATCH)
	    xx = rpmteChain(p, q, oh, "Upgrades");

/*@-nullptrarith@*/
	rpmlog(RPMLOG_DEBUG, D_("   lastref erases %s\n"), rpmteNEVRA(q));
/*@=nullptrarith@*/

    }
    (void)headerFree(debuginfoHeader);
    debuginfoHeader = NULL;

    return (int)nrefs;
}

/**
 * Add Obsoletes: erasures to a transaction set.
 * @param ts		transaction set
 * @param p		transaction element
 * @param hcolor	header color
 * @return		0 on success
 */
static int rpmtsAddObsoletes(rpmts ts, rpmte p, rpmuint32_t hcolor)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, p, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    rpmuint32_t tscolor = rpmtsColor(ts);
    alKey pkgKey = rpmteAddedKey(p);
    rpmuint32_t ohcolor;
    rpmds obsoletes;
    rpmuint32_t dscolor;
    rpmdbMatchIterator mi;
    Header oh;
    int xx;

    if (_obsolete_tag == 0) {
	const char *t = rpmExpand("%{?_obsolete_tag}", NULL);
/*@-mods@*/
	_obsolete_tag = (!strcmp(t, "name") ? RPMTAG_NAME : RPMTAG_PROVIDENAME);
/*@=mods@*/
	t = _free(t);
    }

    obsoletes = rpmdsLink(rpmteDS(p, RPMTAG_OBSOLETENAME), "Obsoletes");
    obsoletes = rpmdsInit(obsoletes);
    if (obsoletes != NULL)
    while (rpmdsNext(obsoletes) >= 0) {
	const char * Name;

	if ((Name = rpmdsN(obsoletes)) == NULL)
	    continue;	/* XXX can't happen */

	/* Ignore colored obsoletes not in our rainbow. */
#if 0
	/* XXX obsoletes are never colored, so this is for future devel. */
	dscolor = rpmdsColor(obsoletes);
#else
	dscolor = hcolor;
#endif
	if (tscolor && dscolor && !(tscolor & dscolor))
	    continue;

	/* XXX avoid self-obsoleting packages. */
	if (!strcmp(rpmteN(p), Name))
	    continue;

	/* Obsolete containing package if given a file, otherwise provide. */
	if (Name[0] == '/')
	    mi = rpmtsInitIterator(ts, RPMTAG_BASENAMES, Name, 0);
	else
	    mi = rpmtsInitIterator(ts, _obsolete_tag, Name, 0);

	xx = rpmdbPruneIterator(mi,
	    ts->removedPackages, ts->numRemovedPackages, 1);

	while((oh = rpmdbNextIterator(mi)) != NULL) {
	    int lastx;
	    rpmte q;

	    /* Ignore colored packages not in our rainbow. */
	    ohcolor = hGetColor(oh);

	    /* XXX provides *are* colored, effectively limiting Obsoletes:
		to matching only colored Provides: based on pkg coloring. */
	    if (tscolor && hcolor && ohcolor && !(hcolor & ohcolor))
		/*@innercontinue@*/ continue;

	    /*
	     * Rpm prior to 3.0.3 does not have versioned obsoletes.
	     * If no obsoletes version info is available, match all names.
	     */
	    if (!(rpmdsEVR(obsoletes) == NULL
	     || rpmdsAnyMatchesDep(oh, obsoletes, _rpmds_nopromote)))
		/*@innercontinue@*/ continue;

	    /* Create an erasure element. */
	    lastx = -1;
	    xx = removePackage(ts, oh, rpmdbGetIteratorOffset(mi), &lastx, pkgKey);
assert(lastx >= 0 && lastx < ts->orderCount);
	    q = ts->order[lastx];

	    /* Chain through obsoletes flink. */
	    xx = rpmteChain(p, q, oh, "Obsoletes");

/*@-nullptrarith@*/
	    rpmlog(RPMLOG_DEBUG, D_("  Obsoletes: %s\t\terases %s\n"),
			rpmdsDNEVR(obsoletes)+2, rpmteNEVRA(q));
/*@=nullptrarith@*/
	}
	mi = rpmdbFreeIterator(mi);
    }
    (void)rpmdsFree(obsoletes);
    obsoletes = NULL;

    return 0;
}

#if defined(RPM_VENDOR_WINDRIVER)
/* Is "compat" compatible w/ arch? */
int _isCompatibleArch(const char * arch, const char * compat)
{
    const char * compatArch = rpmExpand(compat, " %{?_", compat, "_compat_arch}", NULL);
    const char * p, * pe, * t;
    int match = 0;

    /* Hack to ensure iX86 being automatically compatible */
    if (arch[0] == 'i' && arch[2] == '8' && arch[3] == '6') {
	if ((arch[0] == compat[0]) &&
	    (arch[2] == compat[2]) &&
	    (arch[3] == compat[3]))
	    match = 1;

        if (!strcmp(compat, "x86_32"))
	    match = 1;
    }

    for ( p = pe = compatArch ; *pe && match == 0 ; ) {
	while (*p && xisspace(*p)) p++;
	pe = p ; while (*pe && !xisspace(*pe)) pe++;
	if (p == pe)
	    break;
	t = strndup(p, (pe - p));
	p = pe; /* Advance to next chunk */
rpmlog(RPMLOG_DEBUG, D_("   Comparing compat archs %s ? %s\n"), arch, t);
	if (!strcmp(arch, t))
	    match = 1;
	t = _free(t);
    }
    compatArch = _free(compatArch);
    return match;
}
#endif

int rpmtsAddInstallElement(rpmts ts, Header h,
			fnpyKey key, int upgrade, rpmRelocation relocs)
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    rpmdepFlags depFlags = rpmtsDFlags(ts);
    rpmuint32_t tscolor = rpmtsColor(ts);
    rpmuint32_t hcolor;
    int isSource;
    int duplicate = 0;
    rpmtsi pi = NULL; rpmte p;
    const char * arch = NULL;
    const char * os = NULL;
    rpmds oldChk, newChk;
    alKey pkgKey;	/* addedPackages key */
    int xx;
    int ec = 0;
    int rc;
    int oc;

    hcolor = hGetColor(h);
    pkgKey = RPMAL_NOMATCH;

    /*
     * Always add source headers.
     */
    isSource =
	(headerIsEntry(h, RPMTAG_SOURCERPM) == 0 &&
	 headerIsEntry(h, RPMTAG_ARCH) != 0);
    if (isSource) {
	oc = ts->orderCount;
	goto addheader;
    }

    /*
     * Check platform affinity of binary packages.
     */
    he->tag = RPMTAG_ARCH;
    xx = headerGet(h, he, 0);
    arch = he->p.str;
    he->tag = RPMTAG_OS;
    xx = headerGet(h, he, 0);
    os = he->p.str;
    if (nplatpat > 1) {
	const char * platform = NULL;

	he->tag = RPMTAG_PLATFORM;
	xx = headerGet(h, he, 0);
	platform = he->p.str;
	if (!xx || platform == NULL)
	    platform = rpmExpand(arch, "-unknown-", os, NULL);

	rc = rpmPlatformScore(platform, platpat, nplatpat);
	if (rc <= 0) {
	    rpmps ps = rpmtsProblems(ts);
	    he->tag = RPMTAG_NVRA;
	    xx = headerGet(h, he, 0);
assert(he->p.str != NULL);

	    rpmpsAppend(ps, RPMPROB_BADPLATFORM, he->p.str, key,
                        platform, NULL, NULL, 0);

	    /* XXX problem string should be printed by caller instead. */
	    if (rpmIsVerbose()) {
		const char * msg = rpmProblemString(rpmpsGetProblem(ps, -1));
		rpmlog(RPMLOG_WARNING, "%s\n", msg);
		msg = _free(msg);
	    }

	    ps = rpmpsFree(ps);
	    he->p.ptr = _free(he->p.ptr);
	    ec = 1;
	}
	platform = _free(platform);
	if (ec)
	    goto exit;
    }

    /*
     * Always install compatible binary packages.
     */
    if (!upgrade) {
	oc = ts->orderCount;
	goto addheader;
    }

    /*
     * Check that upgrade package is uniquely newer, replace older if necessary.
     */
    oldChk = rpmdsThis(h, RPMTAG_REQUIRENAME, (RPMSENSE_LESS));
    newChk = rpmdsThis(h, RPMTAG_REQUIRENAME, (RPMSENSE_EQUAL|RPMSENSE_GREATER));
    /* XXX can't use rpmtsiNext() filter or oc will have wrong value. */
    for (pi = rpmtsiInit(ts), oc = 0; (p = rpmtsiNext(pi, 0)) != NULL; oc++) {
	rpmds this;

	/* XXX Only added packages need be checked for dupes here. */
	if (rpmteType(p) == TR_REMOVED)
	    continue;

	/* XXX Never check source header NEVRAO. */
	if (rpmteIsSource(p))
	    continue;

	if (tscolor) {
	    const char * parch;
	    const char * pos;

	    if (arch == NULL || (parch = rpmteA(p)) == NULL)
		continue;
#if defined(RPM_VENDOR_WINDRIVER)
	    /* XXX hackery for alias matching. */
	    if (!_isCompatibleArch(arch, parch))
		continue;
#else
	    /* XXX hackery for i[3456]86 alias matching. */
	    if (arch[0] == 'i' && arch[2] == '8' && arch[3] == '6') {
		if (arch[0] != parch[0]) continue;
		if (arch[2] != parch[2]) continue;
		if (arch[3] != parch[3]) continue;
	    }
#endif
	    else if (strcmp(arch, parch))
		continue;
	    if (os == NULL || (pos = rpmteO(p)) == NULL)
		continue;

	    if (strcmp(os, pos))
		continue;
	}

	/* OK, binary rpm's with same arch and os.  Check NEVR. */
	if ((this = rpmteDS(p, RPMTAG_NAME)) == NULL)
	    continue;	/* XXX can't happen */

	/* If newer NEVRAO already added, then skip adding older. */
	rc = rpmdsCompare(newChk, this);
	if (rc != 0) {
	    const char * pkgNEVR = rpmdsDNEVR(this);
	    const char * addNEVR = rpmdsDNEVR(oldChk);
	    if (rpmIsVerbose())
		rpmlog(RPMLOG_WARNING,
		    _("package %s was already added, skipping %s\n"),
		    (pkgNEVR ? pkgNEVR + 2 : "?pkgNEVR?"),
		    (addNEVR ? addNEVR + 2 : "?addNEVR?"));
	    ec = 1;
	    break;
	}

	/* If older NEVRAO already added, then replace old with new. */
	rc = rpmdsCompare(oldChk, this);
	if (rc != 0) {
	    const char * pkgNEVR = rpmdsDNEVR(this);
	    const char * addNEVR = rpmdsDNEVR(newChk);
	    if (rpmIsVerbose())
		rpmlog(RPMLOG_WARNING,
		    _("package %s was already added, replacing with %s\n"),
		    (pkgNEVR ? pkgNEVR + 2 : "?pkgNEVR?"),
		    (addNEVR ? addNEVR + 2 : "?addNEVR?"));
	    duplicate = 1;
	    pkgKey = rpmteAddedKey(p);
	    break;
	}
    }
    pi = rpmtsiFree(pi);
    (void)rpmdsFree(oldChk);
    oldChk = NULL;
    (void)rpmdsFree(newChk);
    newChk = NULL;

    /* If newer (or same) NEVRAO was already added, exit now. */
    if (ec)
	goto exit;

addheader:
    if (oc >= ts->orderAlloced) {
	ts->orderAlloced += (oc - ts->orderAlloced) + ts->delta;
/*@-type +voidabstract @*/
	ts->order = xrealloc(ts->order, ts->orderAlloced * sizeof(*ts->order));
/*@=type =voidabstract @*/
    }

    p = rpmteNew(ts, h, TR_ADDED, key, relocs, -1, pkgKey);
assert(p != NULL);

    if (duplicate && oc < ts->orderCount) {
/*@-type -unqualifiedtrans@*/
	ts->order[oc] = rpmteFree(ts->order[oc]);
/*@=type =unqualifiedtrans@*/
    }

    ts->order[oc] = p;
    if (!duplicate) {
	ts->orderCount++;
	rpmcliPackagesTotal++;
    }
    
    pkgKey = rpmalAdd(&ts->addedPackages, pkgKey, rpmteKey(p),
			rpmteDS(p, RPMTAG_PROVIDENAME),
			rpmteFI(p, RPMTAG_BASENAMES), tscolor);
    if (pkgKey == RPMAL_NOMATCH) {
	ts->order[oc] = rpmteFree(ts->order[oc]);
	ts->teInstall = NULL;
	ec = 1;
	goto exit;
    }
    (void) rpmteSetAddedKey(p, pkgKey);

    if (!duplicate) {
	ts->numAddedPackages++;
    }

    ts->teInstall = ts->order[oc];

    /* XXX rpmgi hack: Save header in transaction element if requested. */
    if (upgrade & 0x2)
	(void) rpmteSetHeader(p, h);

    /* If not upgrading, then we're done. */
    if (!(upgrade & 0x1))
	goto exit;

    /* If source rpm, then we're done. */
    if (isSource)
	goto exit;

    /* Do lazy (readonly?) open of rpm database. */
    if (rpmtsGetRdb(ts) == NULL && rpmtsDBMode(ts) != -1) {
	if ((ec = rpmtsOpenDB(ts, rpmtsDBMode(ts)) != 0))
	    goto exit;
    }

    /* Add upgrades to the transaction (if not disabled). */
    if (!(depFlags & RPMDEPS_FLAG_NOUPGRADE)) {
	/*
	 * Don't upgrade -debuginfo until build set is empty.
	 *
	 * XXX Almost, but not quite, correct since the test depends on
	 * added package arrival order.
	 * I.e. -debuginfo additions must always follow all
	 * other additions so that erasures of other members in the
	 * same build set are seen if/when included in the same transaction.
	 */
	xx = rpmtsEraseDebuginfo(ts, p, h, pkgKey);
	if (!chkSuffix(rpmteN(p), "-debuginfo") || xx == 0)
	    xx = rpmtsAddUpgrades(ts, p, hcolor, h);
    }

    /* Add Obsoletes: to the transaction (if not disabled). */
    if (!(depFlags & RPMDEPS_FLAG_NOOBSOLETES)) {
	xx = rpmtsAddObsoletes(ts, p, hcolor);
    }

    ec = 0;

exit:
    arch = _free(arch);
    os = _free(os);
    pi = rpmtsiFree(pi);
    return ec;
}

int rpmtsAddEraseElement(rpmts ts, Header h, int dboffset)
{
    int oc = -1;
    int rc = removePackage(ts, h, dboffset, &oc, RPMAL_NOMATCH);
    if (rc == 0 && oc >= 0 && oc < ts->orderCount) {
	(void) rpmtsEraseDebuginfo(ts, ts->order[oc], h, RPMAL_NOMATCH);
	ts->teErase = ts->order[oc];
    } else
	ts->teErase = NULL;
    return rc;
}

/*@only@*/ /*@null@*/ /*@unchecked@*/
static char *sysinfo_path = NULL;

/*@refcounted@*/ /*@null@*/ /*@unchecked@*/
static rpmds rpmlibP = NULL;
/*@refcounted@*/ /*@null@*/ /*@unchecked@*/
rpmds cpuinfoP = NULL;
/*@refcounted@*/ /*@null@*/ /*@unchecked@*/
static rpmds getconfP = NULL;
/*@refcounted@*/ /*@null@*/ /*@unchecked@*/
static rpmds unameP = NULL;

void rpmnsClean(void)
	/*@globals sysinfo_path, _sysinfo_path, rpmlibP, cpuinfoP, getconfP, unameP @*/
	/*@modifies sysinfo_path, _sysinfo_path, rpmlibP, cpuinfoP, getconfP, unameP @*/
{
    (void)rpmdsFree(rpmlibP);
    rpmlibP = NULL;
    (void)rpmdsFree(cpuinfoP);
    cpuinfoP = NULL;
    (void)rpmdsFree(getconfP);
    getconfP = NULL;
    (void)rpmdsFree(unameP);
    unameP = NULL;
/*@-observertrans@*/
    _sysinfo_path = _free(_sysinfo_path);
/*@=observertrans@*/
    sysinfo_path = _free(sysinfo_path);
}

/**
 * Check dep for an unsatisfied dependency.
 * @param ts		transaction set
 * @param dep		dependency
 * @param adding	dependency is from added package set?
 * @return		0 if satisfied, 1 if not satisfied, 2 if error
 */
static int unsatisfiedDepend(rpmts ts, rpmds dep, int adding)
	/*@globals rpmGlobalMacroContext, h_errno,
		sysinfo_path, fileSystem, internalState @*/
	/*@modifies ts, dep, rpmGlobalMacroContext,
		sysinfo_path, fileSystem, internalState @*/
{
    DBT * key = alloca(sizeof(*key));
    DBT * data = alloca(sizeof(*data));
    rpmdbMatchIterator mi;
    nsType NSType;
    const char * Name;
    rpmuint32_t Flags;
    Header h;
#if defined(CACHE_DEPENDENCY_RESULT)
    int _cacheThisRC = 1;
#endif
    int rc;
    int xx;
    int retries = 20;

    if ((Name = rpmdsN(dep)) == NULL)
	return 0;	/* XXX can't happen */
    Flags = rpmdsFlags(dep);
    NSType = rpmdsNSType(dep);

#if defined(CACHE_DEPENDENCY_RESULT)
    /*
     * Check if dbiOpen/dbiPut failed (e.g. permissions), we can't cache.
     */
    if (_cacheDependsRC) {
	dbiIndex dbi;
	dbi = dbiOpen(rpmtsGetRdb(ts), RPMDBI_DEPENDS, 0);
	if (dbi == NULL)
	    _cacheDependsRC = 0;
	else {
	    const char * DNEVR;

	    rc = -1;
	    if ((DNEVR = rpmdsDNEVR(dep)) != NULL) {
		DBC * dbcursor = NULL;
		void * datap = NULL;
		size_t datalen = 0;
		size_t DNEVRlen = strlen(DNEVR);

		xx = dbiCopen(dbi, dbiTxnid(dbi), &dbcursor, 0);

		memset(key, 0, sizeof(*key));
/*@i@*/		key->data = (void *) DNEVR;
		key->size = DNEVRlen;
		memset(data, 0, sizeof(*data));
		data->data = datap;
		data->size = datalen;
/*@-nullstate@*/ /* FIX: data->data may be NULL */
		xx = dbiGet(dbi, dbcursor, key, data, DB_SET);
/*@=nullstate@*/
		DNEVR = key->data;
		DNEVRlen = key->size;
		datap = data->data;
		datalen = data->size;

		if (xx == 0 && datap && datalen == 4)
		    memcpy(&rc, datap, datalen);
		xx = dbiCclose(dbi, dbcursor, 0);
	    }

	    if (rc >= 0) {
		rpmdsNotify(dep, _("(cached)"), rc);
		return rpmdsNegateRC(dep, rc);
	    }
	}
    }
#endif

retry:
    rc = 0;	/* assume dependency is satisfied */

    /* Expand macro probe dependencies. */
    if (NSType == RPMNS_TYPE_FUNCTION) {
	xx = rpmExpandNumeric(Name);
	rc = (xx ? 0 : 1);
	if (Flags & RPMSENSE_MISSINGOK)
	    goto unsatisfied;
	rpmdsNotify(dep, _("(function probe)"), rc);
	goto exit;
    }

    /* Evaluate user/group lookup probes. */
    if (NSType == RPMNS_TYPE_USER) {
	const char *s;
	uid_t uid = 0;
	s = Name; while (*s && xisdigit(*s)) s++;

	if (*s)
	    xx = unameToUid(Name, &uid);
	else {
	    uid = strtol(Name, NULL, 10);
	    xx = (uidToUname(uid) ? 0 : -1);
	}
	rc = (xx >= 0 ? 0 : 1);
	if (Flags & RPMSENSE_MISSINGOK)
	    goto unsatisfied;
	rpmdsNotify(dep, _("(user lookup)"), rc);
	goto exit;
    }
    if (NSType == RPMNS_TYPE_GROUP) {
	const char *s;
	gid_t gid = 0;
	s = Name; while (*s && xisdigit(*s)) s++;

	if (*s)
	    xx = gnameToGid(Name, &gid);
	else {
	    gid = strtol(Name, NULL, 10);
	    xx = (gidToGname(gid) ? 0 : -1);
	}
	rc = (xx >= 0 ? 0 : 1);
	if (Flags & RPMSENSE_MISSINGOK)
	    goto unsatisfied;
	rpmdsNotify(dep, _("(group lookup)"), rc);
	goto exit;
    }

    /* Evaluate access(2) probe dependencies. */
    if (NSType == RPMNS_TYPE_ACCESS) {
	rc = rpmioAccess(Name, NULL, X_OK);
	if (Flags & RPMSENSE_MISSINGOK)
	    goto unsatisfied;
	rpmdsNotify(dep, _("(access probe)"), rc);
	goto exit;
    }

    /* Evaluate mtab lookup and diskspace probe dependencies. */
    if (NSType == RPMNS_TYPE_MOUNTED) {
	const char ** fs = NULL;
	int nfs = 0;
	int i = 0;

	xx = rpmtsInitDSI(ts);
	fs = ts->filesystems;
	nfs = ts->filesystemCount;

	if (fs != NULL)
	for (i = 0; i < nfs; i++) {
	    if (!strcmp(fs[i], Name))
		break;
	}
	rc = (i < nfs ? 0 : 1);
	if (Flags & RPMSENSE_MISSINGOK)
	    goto unsatisfied;
	rpmdsNotify(dep, _("(mtab probe)"), rc);
	goto exit;
    }

    if (NSType == RPMNS_TYPE_DISKSPACE) {
	size_t nb = strlen(Name);
	rpmDiskSpaceInfo dsi = NULL;
	const char ** fs = NULL;
	size_t fslen = 0, longest = 0;
	int nfs = 0;
	int i = 0;

	xx = rpmtsInitDSI(ts);
	fs = ts->filesystems;
	nfs = ts->filesystemCount;

	if (fs != NULL)
	for (i = 0; i < nfs; i++) {
	    fslen = strlen(fs[i]);
	    if (fslen > nb)
		continue;
	    if (strncmp(fs[i], Name, fslen))
		continue;
	    if (fslen > 1 && Name[fslen] != '/' && Name[fslen] != '\0')
		continue;
	    if (fslen < longest)
		continue;
	    longest = fslen;
	    dsi = ts->dsi + i;
	}
	if (dsi == NULL)
	    rc = 1;	/* no mounted paths !?! */
	else {
	    char * end = NULL;
/*@-unrecog@*/
	    rpmuint64_t needed = strtoll(rpmdsEVR(dep), &end, 0);
/*@=unrecog@*/

	    if (end && *end) {
		if (strchr("Gg", end[0]) && strchr("Bb", end[1]) && !end[2])
		    needed *= 1024 * 1024 * 1024;
		if (strchr("Mm", end[0]) && strchr("Bb", end[1]) && !end[2])
		    needed *= 1024 * 1024;
		if (strchr("Kk", end[0]) && strchr("Bb", end[1]) && !end[2])
		    needed *= 1024;
	    } else
		needed *= 1024 * 1024;	/* XXX assume Mb if no units given */

	    needed = BLOCK_ROUND(needed, dsi->f_bsize);
	    xx = (dsi->f_bavail - needed);
	    if ((Flags & RPMSENSE_LESS) && xx < 0) rc = 0;
	    else if ((Flags & RPMSENSE_GREATER) && xx > 0) rc = 0;
	    else if ((Flags & RPMSENSE_EQUAL) && xx == 0) rc = 0;
	    else rc = 1;
	}
	if (Flags & RPMSENSE_MISSINGOK)
	    goto unsatisfied;
	rpmdsNotify(dep, _("(diskspace probe)"), rc);
	goto exit;
    }

    if (NSType == RPMNS_TYPE_DIGEST) {
	const char * EVR = rpmdsEVR(dep);
        const char *filename;
        pgpHashAlgo digestHashAlgo;
        FD_t fd;
        char *cp;
        int algo;

        filename = Name;
        digestHashAlgo = PGPHASHALGO_MD5;
        if ((cp = strchr(filename, ':')) != NULL) {
            if ((algo = pgpHashAlgoStringToNumber(filename, cp-filename)) != PGPHASHALGO_ERROR) {
                digestHashAlgo = algo;
                filename = cp + 1;
            }
        }
	rc = 1;		/* XXX assume failure */
        fd = Fopen(filename, "r.fdio");
	if (fd && !Ferror(fd)) {
	    DIGEST_CTX ctx = rpmDigestInit(digestHashAlgo, RPMDIGEST_NONE);
	    const char * digest = NULL;
	    size_t digestlen = 0;
	    int asAscii = 1;
	    size_t nbuf = 8 * BUFSIZ;
	    char * buf = alloca(nbuf);
	    size_t nb;

	    while ((nb = Fread(buf, sizeof(buf[0]), nbuf, fd)) > 0)
		xx = rpmDigestUpdate(ctx, buf, nb);
	    xx = Fclose(fd);	fd = NULL;
	    xx = rpmDigestFinal(ctx, &digest, &digestlen, asAscii);

	    xx = (EVR && *EVR && digest && *digest) ? strcasecmp(EVR, digest) : -1;
	    /* XXX only equality makes sense for digest compares */
	    if ((Flags & RPMSENSE_EQUAL) && xx == 0) rc = 0;
	}
	if (Flags & RPMSENSE_MISSINGOK)
	    goto unsatisfied;
	rpmdsNotify(dep, _("(digest probe)"), rc);
	goto exit;
    }

    if (NSType == RPMNS_TYPE_SIGNATURE) {
	const char * EVR = rpmdsEVR(dep);
	ARGV_t avN = NULL;
	ARGV_t avEVR = NULL;
	rpmRC res;

	/* Split /fn:/sig */
	xx = argvSplit(&avN, Name, ":");

	/* Split /pub:id */
	xx = (EVR && *EVR) ? argvSplit(&avEVR, EVR, ":") : argvAdd(&avEVR, "");

	res = rpmnsProbeSignature(ts, avN[0], avN[1], avEVR[0], avEVR[1], 0);
	rc = (res == RPMRC_OK ? 0 : 1);

	avN = argvFree(avN);
	avEVR = argvFree(avEVR);

	if (Flags & RPMSENSE_MISSINGOK)
	    goto unsatisfied;
	rpmdsNotify(dep, _("(signature probe)"), rc);
	goto exit;
    }

    if (NSType == RPMNS_TYPE_VERIFY) {
	QVA_t qva = memset(alloca(sizeof(*qva)), 0, sizeof(*qva));

	qva->qva_mode = 'v';
	qva->qva_flags = (int)(VERIFY_ALL & ~(VERIFY_DEPS|VERIFY_SCRIPT));
	rc = 0;		/* assume success */
	if (rpmtsGetRdb(ts) != NULL) {
	    if (!strcmp(Name, "*"))			/* -Va probe */
		mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES, NULL, 0);
	    else if (Name[0] == '/')		/* -Vf probe */
		mi = rpmtsInitIterator(ts, RPMTAG_BASENAMES, Name, 0);
	    else				/* -V probe */
		mi = rpmtsInitIterator(ts, RPMTAG_PROVIDENAME, Name, 0);
	    while ((h = rpmdbNextIterator(mi)) != NULL) {
		if (!(Name[0] == '/' || !strcmp(Name, "*")))
		if (!rpmdsAnyMatchesDep(h, dep, _rpmds_nopromote))
		    continue;
		xx = (showVerifyPackage(qva, ts, h) ? 1 : 0);
		if (xx)
		    rc = 1;
	    }
	    mi = rpmdbFreeIterator(mi);
	}

	if (Flags & RPMSENSE_MISSINGOK)
	    goto unsatisfied;
	rpmdsNotify(dep, _("(verify probe)"), rc);
	goto exit;
    }

    if (NSType == RPMNS_TYPE_GNUPG) {
	const char * EVR = rpmdsEVR(dep);
	if (!(EVR && *EVR)) {
	    static const char gnupg_pre[] = "%(%{__gpg} --batch --no-tty --quiet --verify ";
	    static const char gnupg_post[] = " 2>/dev/null; echo $?)";
	    const char * t = rpmExpand(gnupg_pre, Name, gnupg_post, NULL);
	    rc = (t && t[0] == '0') ? 0 : 1;
	    t = _free(t);
        }
        else {
	    static const char gnupg_pre[] = "%(%{__gpg} --batch --no-tty --quiet --verify ";
	    static const char gnupg_post[] = " 2>&1 | grep '^Primary key fingerprint:' | sed -e 's;^.*: *;;' -e 's; *;;g')";
	    const char * t = rpmExpand(gnupg_pre, Name, gnupg_post, NULL);
	    rc = ((Flags & RPMSENSE_EQUAL) && strcasecmp(EVR, t) == 0) ? 0 : 1;
	    t = _free(t);
        }
	if (Flags & RPMSENSE_MISSINGOK)
	    goto unsatisfied;
	rpmdsNotify(dep, _("(gnupg probe)"), rc);
	goto exit;
    }

    if (NSType == RPMNS_TYPE_MACRO) {
	static const char macro_pre[] = "%{?";
	static const char macro_post[] = ":0}";
	const char * a = rpmExpand(macro_pre, Name, macro_post, NULL);

	rc = (a && a[0] == '0') ? 0 : 1;
	a = _free(a);
	if (Flags & RPMSENSE_MISSINGOK)
	    goto unsatisfied;
	rpmdsNotify(dep, _("(macro probe)"), rc);
	goto exit;
    }

    if (NSType == RPMNS_TYPE_ENVVAR) {
	const char * a = envGet(Name);
	const char * b = rpmdsEVR(dep);

	/* Existence test if EVR is missing/empty. */
	if (!(b && *b))
	    rc = (!(a && *a));
	else {
	    int sense = (a && *a) ? strcmp(a, b) : -1;

	    if ((Flags & RPMSENSE_SENSEMASK) == RPMSENSE_NOTEQUAL)
		rc = (sense == 0);
	    else if (sense < 0 && (Flags & RPMSENSE_LESS))
		rc = 0;
	    else if (sense > 0 && (Flags & RPMSENSE_GREATER))
		rc = 0;
	    else if (sense == 0 && (Flags & RPMSENSE_EQUAL))
		rc = 0;
	    else
		rc = (sense != 0);
	}

	if (Flags & RPMSENSE_MISSINGOK)
	    goto unsatisfied;
	rpmdsNotify(dep, _("(envvar probe)"), rc);
	goto exit;
    }

    if (NSType == RPMNS_TYPE_RUNNING) {
	char *t = NULL;
	pid_t pid = strtol(Name, &t, 10);

	if (t == NULL || *t != '\0') {
	    const char * fn = rpmGetPath("%{_varrun}/", Name, ".pid", NULL);
	    FD_t fd = NULL;

	    if (fn && *fn != '%' && (fd = Fopen(fn, "r.fdio")) && !Ferror(fd)) {
		char buf[32];
		size_t nb = Fread(buf, sizeof(buf[0]), sizeof(buf), fd);

		if (nb > 0)
		    pid = strtol(buf, &t, 10);
	    } else
		pid = 0;
	    if (fd != NULL)
		(void) Fclose(fd);
	    fn = _free(fn);
	}
	rc = (pid > 0 ? (kill(pid, 0) < 0 && errno == ESRCH) : 1);
	if (Flags & RPMSENSE_MISSINGOK)
	    goto unsatisfied;
	rpmdsNotify(dep, _("(running probe)"), rc);
	goto exit;
    }

    if (NSType == RPMNS_TYPE_SANITY) {
	/* XXX only the installer does not have the database open here. */
	rc = 1;		/* assume failure */
	if (rpmtsGetRdb(ts) != NULL) {
	    mi = rpmtsInitIterator(ts, RPMTAG_PROVIDENAME, Name, 0);
	    while ((h = rpmdbNextIterator(mi)) != NULL) {
		if (!rpmdsAnyMatchesDep(h, dep, _rpmds_nopromote))
		    continue;
		rc = (headerIsEntry(h, RPMTAG_SANITYCHECK) == 0);
		if (rc == 0) {
		    /* XXX FIXME: actually run the sanitycheck script. */
		    break;
		}
	    }
	    mi = rpmdbFreeIterator(mi);
	}
	if (Flags & RPMSENSE_MISSINGOK)
	    goto unsatisfied;
	rpmdsNotify(dep, _("(sanity probe)"), rc);
	goto exit;
    }

    if (NSType == RPMNS_TYPE_VCHECK) {
	rc = 1;		/* assume failure */
	if (rpmtsGetRdb(ts) != NULL) {
	    mi = rpmtsInitIterator(ts, RPMTAG_PROVIDENAME, Name, 0);
	    while ((h = rpmdbNextIterator(mi)) != NULL) {
		if (!rpmdsAnyMatchesDep(h, dep, _rpmds_nopromote))
		    continue;
		rc = (headerIsEntry(h, RPMTAG_TRACK) == 0);
		if (rc == 0) {
		    /* XXX FIXME: actually run the vcheck script. */
		    break;
		}
	    }
	    mi = rpmdbFreeIterator(mi);
	}
	if (Flags & RPMSENSE_MISSINGOK)
	    goto unsatisfied;
	rpmdsNotify(dep, _("(vcheck probe)"), rc);
	goto exit;
    }

    /* Search system configured provides. */
    if (sysinfo_path == NULL) {
	sysinfo_path = rpmExpand("%{?_rpmds_sysinfo_path}", NULL);
	if (!(sysinfo_path != NULL && *sysinfo_path == '/')) {
	    sysinfo_path = _free(sysinfo_path);
	    sysinfo_path = xstrdup(SYSCONFIGDIR "/sysinfo");
	}
    }

    if (!rpmioAccess(sysinfo_path, NULL, R_OK)) {
#ifdef	NOTYET	/* XXX just sysinfo Provides: for now. */
	rpmTag tagN = (Name[0] == '/' ? RPMTAG_DIRNAMES : RPMTAG_PROVIDENAME);
#else
	rpmTag tagN = RPMTAG_PROVIDENAME;
#endif
	rpmds P = rpmdsFromPRCO(rpmtsPRCO(ts), tagN);
	if (rpmdsSearch(P, dep) >= 0) {
	    rpmdsNotify(dep, _("(sysinfo provides)"), rc);
	    goto exit;
	}
    }

    /*
     * New features in rpm packaging implicitly add versioned dependencies
     * on rpmlib provides. The dependencies look like "rpmlib(YaddaYadda)".
     * Check those dependencies now.
     */
    if (NSType == RPMNS_TYPE_RPMLIB) {
	static int oneshot = -1;

	if (oneshot)
	    oneshot = rpmdsRpmlib(&rpmlibP, NULL);
	if (rpmlibP == NULL)
	    goto unsatisfied;

	if (rpmdsSearch(rpmlibP, dep) >= 0) {
	    rpmdsNotify(dep, _("(rpmlib provides)"), rc);
	    goto exit;
	}
	goto unsatisfied;
    }

    if (NSType == RPMNS_TYPE_CPUINFO) {
	static int oneshot = -1;

	if (oneshot && cpuinfoP == NULL)
	    oneshot = rpmdsCpuinfo(&cpuinfoP, NULL);
	if (cpuinfoP == NULL)
	    goto unsatisfied;

	if (rpmdsSearch(cpuinfoP, dep) >= 0) {
	    rpmdsNotify(dep, _("(cpuinfo provides)"), rc);
	    goto exit;
	}
	goto unsatisfied;
    }

    if (NSType == RPMNS_TYPE_GETCONF) {
	static int oneshot = -1;

	if (oneshot)
	    oneshot = rpmdsGetconf(&getconfP, NULL);
	if (getconfP == NULL)
	    goto unsatisfied;

	if (rpmdsSearch(getconfP, dep) >= 0) {
	    rpmdsNotify(dep, _("(getconf provides)"), rc);
	    goto exit;
	}
	goto unsatisfied;
    }

    if (NSType == RPMNS_TYPE_UNAME) {
	static int oneshot = -1;

	if (oneshot)
	    oneshot = rpmdsUname(&unameP, NULL);
	if (unameP == NULL)
	    goto unsatisfied;

	if (rpmdsSearch(unameP, dep) >= 0) {
	    rpmdsNotify(dep, _("(uname provides)"), rc);
	    goto exit;
	}
	goto unsatisfied;
    }

    if (NSType == RPMNS_TYPE_SONAME) {
	rpmds sonameP = NULL;
	rpmPRCO PRCO = rpmdsNewPRCO(NULL);
	char * fn = strcpy(alloca(strlen(Name)+1), Name);
	int flags = 0;	/* XXX RPMELF_FLAG_SKIPREQUIRES? */
	rpmds ds;

	/* XXX Only absolute paths for now. */
	if (*fn != '/')
	    goto unsatisfied;
	fn[strlen(fn)-1] = '\0';

	/* Extract ELF Provides: from /path/to/DSO. */
	xx = rpmdsELF(fn, flags, rpmdsMergePRCO, PRCO);
	sonameP = rpmdsFromPRCO(PRCO, RPMTAG_PROVIDENAME);
	if (!(xx == 0 && sonameP != NULL))
	    goto unsatisfied;

	/* Search using the original {EVR,"",Flags} from the dep set. */
	ds = rpmdsSingle(rpmdsTagN(dep), rpmdsEVR(dep), "", Flags);
	xx = rpmdsSearch(sonameP, ds);
	(void)rpmdsFree(ds);
	ds = NULL;
	PRCO = rpmdsFreePRCO(PRCO);

	/* Was the dependency satisfied? */
	if (xx >= 0) {
	    rpmdsNotify(dep, _("(soname provides)"), rc);
	    goto exit;
	}
	goto unsatisfied;
    }

    /* Search added packages for the dependency. */
    if (rpmalSatisfiesDepend(ts->addedPackages, dep, NULL) != NULL) {
#if defined(CACHE_DEPENDENCY_RESULT)
	/*
	 * XXX Ick, context sensitive answers from dependency cache.
	 * XXX Always resolve added dependencies within context to disambiguate.
	 */
	if (_rpmds_nopromote)
	    _cacheThisRC = 0;
#endif
	goto exit;
    }

    /* XXX only the installer does not have the database open here. */
    if (rpmtsGetRdb(ts) != NULL) {
	if (Name[0] == '/') {
	    /* depFlags better be 0! */

	    mi = rpmtsInitIterator(ts, RPMTAG_BASENAMES, Name, 0);
	    (void) rpmdbPruneIterator(mi,
			ts->removedPackages, ts->numRemovedPackages, 1);
	    while ((h = rpmdbNextIterator(mi)) != NULL) {
		rpmdsNotify(dep, _("(db files)"), rc);
		mi = rpmdbFreeIterator(mi);
		goto exit;
	    }
	    mi = rpmdbFreeIterator(mi);
	}

	mi = rpmtsInitIterator(ts, RPMTAG_PROVIDENAME, Name, 0);
	(void) rpmdbPruneIterator(mi,
			ts->removedPackages, ts->numRemovedPackages, 1);
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    if (rpmdsAnyMatchesDep(h, dep, _rpmds_nopromote)) {
		rpmdsNotify(dep, _("(db provides)"), rc);
		mi = rpmdbFreeIterator(mi);
		goto exit;
	    }
	}
	mi = rpmdbFreeIterator(mi);
    }

    /*
     * Search for an unsatisfied dependency.
     */
    if (adding == 1 && retries > 0 && !(rpmtsDFlags(ts) & RPMDEPS_FLAG_NOSUGGEST)) {
	if (ts->solve != NULL) {
	    xx = (*ts->solve) (ts, dep, ts->solveData);
	    if (xx == 0)
		goto exit;
	    if (xx == -1) {
		retries--;
		rpmalMakeIndex(ts->addedPackages);
		goto retry;
	    }
	}
    }

unsatisfied:
    if (Flags & RPMSENSE_MISSINGOK) {
	rc = 0;	/* dependency is unsatisfied, but just a hint. */
#if defined(CACHE_DEPENDENCY_RESULT)
	_cacheThisRC = 0;
#endif
	rpmdsNotify(dep, _("(hint skipped)"), rc);
    } else {
	rc = 1;	/* dependency is unsatisfied */
	rpmdsNotify(dep, NULL, rc);
    }

exit:
    /*
     * If dbiOpen/dbiPut fails (e.g. permissions), we can't cache.
     */
#if defined(CACHE_DEPENDENCY_RESULT)
    if (_cacheDependsRC && _cacheThisRC) {
	dbiIndex dbi;
	dbi = dbiOpen(rpmtsGetRdb(ts), RPMDBI_DEPENDS, 0);
	if (dbi == NULL) {
	    _cacheDependsRC = 0;
	} else {
	    const char * DNEVR;
	    xx = 0;
	    if ((DNEVR = rpmdsDNEVR(dep)) != NULL) {
		DBC * dbcursor = NULL;
		size_t DNEVRlen = strlen(DNEVR);

		xx = dbiCopen(dbi, dbiTxnid(dbi), &dbcursor, DB_WRITECURSOR);

		memset(key, 0, sizeof(*key));
/*@i@*/		key->data = (void *) DNEVR;
		key->size = DNEVRlen;
		memset(data, 0, sizeof(*data));
		data->data = &rc;
		data->size = sizeof(rc);

		/*@-compmempass@*/
		xx = dbiPut(dbi, dbcursor, key, data, 0);
		/*@=compmempass@*/
		xx = dbiCclose(dbi, dbcursor, DB_WRITECURSOR);
	    }
	    if (xx)
		_cacheDependsRC = 0;
	}
    }
#endif

    return rpmdsNegateRC(dep, rc);
}

/**
 * Check added requires/conflicts against against installed+added packages.
 * @param ts		transaction set
 * @param pkgNEVRA	package name-version-release.arch
 * @param requires	Requires: dependencies (or NULL)
 * @param conflicts	Conflicts: dependencies (or NULL)
 * @param dirnames	Dirnames: dependencies (or NULL)
 * @param linktos	Filelinktos: dependencies (or NULL)
 * @param depName	dependency name to filter (or NULL)
 * @param tscolor	color bits for transaction set (0 disables)
 * @param adding	dependency is from added package set?
 * @return		0 = deps ok, 1 = dep problems, 2 = error
 */
static int checkPackageDeps(rpmts ts, const char * pkgNEVRA,
		/*@null@*/ rpmds requires,
		/*@null@*/ rpmds conflicts,
		/*@null@*/ rpmds dirnames,
		/*@null@*/ rpmds linktos,
		/*@null@*/ const char * depName,
		rpmuint32_t tscolor, int adding)
	/*@globals rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies ts, requires, conflicts, dirnames, linktos,
		rpmGlobalMacroContext, fileSystem, internalState */
{
    rpmps ps = rpmtsProblems(ts);
    rpmuint32_t dscolor;
    const char * Name;
    int terminate = 2;		/* XXX terminate if rc >= terminate */
    int rc;
    int ourrc = 0;
#if defined(RPM_VENDOR_MANDRIVA) || defined(RPM_VENDOR_ARK) /* optional-dirname-and-symlink-deps */
    int dirname_deps;
    int symlink_deps;
#endif

    requires = rpmdsInit(requires);
    if (requires != NULL)
    while (ourrc < terminate && rpmdsNext(requires) >= 0) {

	if ((Name = rpmdsN(requires)) == NULL)
	    continue;	/* XXX can't happen */

	/* Filter out requires that came along for the ride. */
	if (depName != NULL && strcmp(depName, Name))
	    continue;

	/* Ignore colored requires not in our rainbow. */
	dscolor = rpmdsColor(requires);
	if (tscolor && dscolor && !(tscolor & dscolor))
	    continue;

	rc = unsatisfiedDepend(ts, requires, adding);

	switch (rc) {
	case 0:		/* requirements are satisfied. */
	    /*@switchbreak@*/ break;
	case 1:		/* requirements are not satisfied. */
	{   fnpyKey * suggestedKeys = NULL;

	    if (ts->availablePackages != NULL) {
		suggestedKeys = rpmalAllSatisfiesDepend(ts->availablePackages,
				requires, NULL);
	    }

	    rpmdsProblem(ps, pkgNEVRA, requires, suggestedKeys, adding);

	}
	    ourrc = 1;
	    /*@switchbreak@*/ break;
	case 2:		/* something went wrong! */
	default:
	    ourrc = 2;
	    /*@switchbreak@*/ break;
	}
    }

    conflicts = rpmdsInit(conflicts);
    if (conflicts != NULL)
    while (ourrc < terminate && rpmdsNext(conflicts) >= 0) {

	if ((Name = rpmdsN(conflicts)) == NULL)
	    continue;	/* XXX can't happen */

	/* Filter out conflicts that came along for the ride. */
	if (depName != NULL && strcmp(depName, Name))
	    continue;

	/* Ignore colored conflicts not in our rainbow. */
	dscolor = rpmdsColor(conflicts);
	if (tscolor && dscolor && !(tscolor & dscolor))
	    continue;

	rc = unsatisfiedDepend(ts, conflicts, adding);

	/* 1 == unsatisfied, 0 == satsisfied */
	switch (rc) {
	case 0:		/* conflicts exist. */
	    rpmdsProblem(ps, pkgNEVRA, conflicts, NULL, adding);
	    ourrc = 1;
	    /*@switchbreak@*/ break;
	case 1:		/* conflicts don't exist. */
	    /*@switchbreak@*/ break;
	case 2:		/* something went wrong! */
	default:
	    ourrc = 2;
	    /*@switchbreak@*/ break;
	}
    }

#if defined(RPM_VENDOR_MANDRIVA) || defined(RPM_VENDOR_ARK) /* optional-dirname-and-symlink-deps */
    dirname_deps = rpmExpandNumeric("%{?_check_dirname_deps}%{?!_check_dirname_deps:1}");
    if (dirname_deps) {
#endif
    dirnames = rpmdsInit(dirnames);
    if (dirnames != NULL)
    while (ourrc < terminate && rpmdsNext(dirnames) >= 0) {

	if ((Name = rpmdsN(dirnames)) == NULL)
	    continue;	/* XXX can't happen */

	/* Filter out dirnames that came along for the ride. */
	if (depName != NULL && strcmp(depName, Name))
	    continue;

	/* Ignore colored dirnames not in our rainbow. */
	dscolor = rpmdsColor(dirnames);
	if (tscolor && dscolor && !(tscolor & dscolor))
	    continue;

	rc = unsatisfiedDepend(ts, dirnames, adding);

	switch (rc) {
	case 0:		/* requirements are satisfied. */
	    /*@switchbreak@*/ break;
	case 1:		/* requirements are not satisfied. */
	{   fnpyKey * suggestedKeys = NULL;

	    if (ts->availablePackages != NULL) {
		suggestedKeys = rpmalAllSatisfiesDepend(ts->availablePackages,
				dirnames, NULL);
	    }

	    rpmdsProblem(ps, pkgNEVRA, dirnames, suggestedKeys, adding);

	}
	    ourrc = 1;
	    /*@switchbreak@*/ break;
	case 2:		/* something went wrong! */
	default:
	    ourrc = 2;
	    /*@switchbreak@*/ break;
	}
    }
#if defined(RPM_VENDOR_MANDRIVA) || defined(RPM_VENDOR_ARK) /* optional-dirname-and-symlink-deps */
    }

    symlink_deps = rpmExpandNumeric("%{?_check_symlink_deps}%{?!_check_symlink_deps:1}");
    if (symlink_deps) {
#endif
    linktos = rpmdsInit(linktos);
    if (linktos != NULL)
    while (ourrc < terminate && rpmdsNext(linktos) >= 0) {

	if ((Name = rpmdsN(linktos)) == NULL)
	    continue;	/* XXX can't happen */
	if (*Name == '\0')	/* XXX most linktos are empty */
		continue;

	/* Filter out linktos that came along for the ride. */
	if (depName != NULL && strcmp(depName, Name))
	    continue;

	/* Ignore colored linktos not in our rainbow. */
	dscolor = rpmdsColor(linktos);
	if (tscolor && dscolor && !(tscolor & dscolor))
	    continue;

	rc = unsatisfiedDepend(ts, linktos, adding);

	switch (rc) {
	case 0:		/* requirements are satisfied. */
	    /*@switchbreak@*/ break;
	case 1:		/* requirements are not satisfied. */
	{   fnpyKey * suggestedKeys = NULL;

	    if (ts->availablePackages != NULL) {
		suggestedKeys = rpmalAllSatisfiesDepend(ts->availablePackages,
				linktos, NULL);
	    }

	    rpmdsProblem(ps, pkgNEVRA, linktos, suggestedKeys, adding);

	}
	    ourrc = 1;
	    /*@switchbreak@*/ break;
	case 2:		/* something went wrong! */
	default:
	    ourrc = 2;
	    /*@switchbreak@*/ break;
	}
    }
#if defined(RPM_VENDOR_MANDRIVA) || defined(RPM_VENDOR_ARK) /* optional-dirname-and-symlink-deps */
    }
#endif    

    ps = rpmpsFree(ps);
    return ourrc;
}

/**
 * Check dependency against installed packages.
 * Adding: check name/provides dep against each conflict match,
 * Erasing: check name/provides/filename dep against each requiredby match.
 * @param ts		transaction set
 * @param depName	dependency name
 * @param mi		rpm database iterator
 * @param adding	dependency is from added package set?
 * @return		0 no problems found
 */
static int checkPackageSet(rpmts ts, const char * depName,
		/*@only@*/ /*@null@*/ rpmdbMatchIterator mi, int adding)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, mi, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    rpmdepFlags depFlags = rpmtsDFlags(ts);
    rpmuint32_t tscolor = rpmtsColor(ts);
    int scareMem = 0;
    Header h;
    int terminate = 2;		/* XXX terminate if rc >= terminate */
    int ourrc = 0;

    (void) rpmdbPruneIterator(mi,
		ts->removedPackages, ts->numRemovedPackages, 1);
    while (ourrc < terminate && (h = rpmdbNextIterator(mi)) != NULL) {
	rpmds requires = NULL;
	rpmds conflicts = NULL;
	rpmds dirnames = NULL;
	rpmds linktos = NULL;
	int rc;

	he->tag = RPMTAG_NVRA;
	rc = (headerGet(h, he, 0) ? 0 : 2);
	if (rc > ourrc)
	    ourrc = rc;
	if (ourrc >= terminate) {
	    he->p.str = _free(he->p.str);
	    break;
	}

	if (!(depFlags & RPMDEPS_FLAG_NOREQUIRES))
	    requires = rpmdsNew(h, RPMTAG_REQUIRENAME, scareMem);
	if (!(depFlags & RPMDEPS_FLAG_NOCONFLICTS))
	    conflicts = rpmdsNew(h, RPMTAG_CONFLICTNAME, scareMem);
	if (!(depFlags & RPMDEPS_FLAG_NOPARENTDIRS))
	    dirnames = rpmdsNew(h, RPMTAG_DIRNAMES, scareMem);
	if (!(depFlags & RPMDEPS_FLAG_NOLINKTOS))
	    linktos = rpmdsNew(h, RPMTAG_FILELINKTOS, scareMem);

	(void) rpmdsSetNoPromote(requires, _rpmds_nopromote);
	(void) rpmdsSetNoPromote(conflicts, _rpmds_nopromote);
	(void) rpmdsSetNoPromote(dirnames, _rpmds_nopromote);
	(void) rpmdsSetNoPromote(linktos, _rpmds_nopromote);

	rc = checkPackageDeps(ts, he->p.str,
		requires, conflicts, dirnames, linktos,
		depName, tscolor, adding);

	(void)rpmdsFree(linktos);
	linktos = NULL;
	(void)rpmdsFree(dirnames);
	dirnames = NULL;
	(void)rpmdsFree(conflicts);
	conflicts = NULL;
	(void)rpmdsFree(requires);
	requires = NULL;
	he->p.str = _free(he->p.str);

	if (rc > ourrc)
	    ourrc = rc;
    }
    mi = rpmdbFreeIterator(mi);

    return ourrc;
}

/**
 * Check to-be-erased dependencies against installed requires.
 * @param ts		transaction set
 * @param depName	requires name
 * @return		0 no problems found
 */
static int checkDependentPackages(rpmts ts, const char * depName)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    int rc = 0;

    /* XXX rpmdb can be closed here, avoid error msg. */
    if (rpmtsGetRdb(ts) != NULL) {
	rpmdbMatchIterator mi;
	mi = rpmtsInitIterator(ts, RPMTAG_REQUIRENAME, depName, 0);
	rc = checkPackageSet(ts, depName, mi, 0);
    }
    return rc;
}

/**
 * Check to-be-added dependencies against installed conflicts.
 * @param ts		transaction set
 * @param depName	conflicts name
 * @return		0 no problems found
 */
static int checkDependentConflicts(rpmts ts, const char * depName)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    int rc = 0;

    /* XXX rpmdb can be closed here, avoid error msg. */
    if (rpmtsGetRdb(ts) != NULL) {
	rpmdbMatchIterator mi;
	mi = rpmtsInitIterator(ts, RPMTAG_CONFLICTNAME, depName, 0);
	rc = checkPackageSet(ts, depName, mi, 1);
    }

    return rc;
}

struct badDeps_s {
/*@observer@*/ /*@owned@*/ /*@null@*/
    const char * pname;
/*@observer@*/ /*@dependent@*/ /*@null@*/
    const char * qname;
};

#ifdef REFERENCE
static struct badDeps_s {
/*@observer@*/ /*@null@*/ const char * pname;
/*@observer@*/ /*@null@*/ const char * qname;
} badDeps[] = {
    { NULL, NULL }
};
#else
/*@unchecked@*/
static int badDepsInitialized = 0;

/*@unchecked@*/ /*@only@*/ /*@null@*/
static struct badDeps_s * badDeps = NULL;
#endif

/**
 */
/*@-modobserver -observertrans @*/
static void freeBadDeps(void)
	/*@globals badDeps, badDepsInitialized @*/
	/*@modifies badDeps, badDepsInitialized @*/
{
    if (badDeps) {
	struct badDeps_s * bdp;
	for (bdp = badDeps; bdp->pname != NULL && bdp->qname != NULL; bdp++)
	    bdp->pname = _free(bdp->pname);
	badDeps = _free(badDeps);
    }
    badDepsInitialized = 0;
}
/*@=modobserver =observertrans @*/

/**
 * Check for dependency relations to be ignored.
 *
 * @param ts		transaction set
 * @param p		successor element (i.e. with Requires: )
 * @param q		predecessor element (i.e. with Provides: )
 * @return		1 if dependency is to be ignored.
 */
static int ignoreDep(const rpmts ts, const rpmte p, const rpmte q)
	/*@globals badDeps, badDepsInitialized,
		rpmGlobalMacroContext, h_errno, internalState @*/
	/*@modifies badDeps, badDepsInitialized,
		rpmGlobalMacroContext, internalState @*/
{
    struct badDeps_s * bdp;

    if (!badDepsInitialized) {
	char * s = rpmExpand("%{?_dependency_whiteout}", NULL);
	const char ** av = NULL;
	int anaconda = rpmtsDFlags(ts) & RPMDEPS_FLAG_ANACONDA;
	int msglvl = (anaconda || (rpmtsDFlags(ts) & RPMDEPS_FLAG_DEPLOOPS))
			? RPMLOG_WARNING : RPMLOG_DEBUG;
	int ac = 0;
	int i;

	if (s != NULL && *s != '\0'
	&& !(i = poptParseArgvString(s, &ac, (const char ***)&av))
	&& ac > 0 && av != NULL)
	{
	    bdp = badDeps = xcalloc(ac+1, sizeof(*badDeps));
	    for (i = 0; i < ac; i++, bdp++) {
		char * pname, * qname;

		if (av[i] == NULL)
		    break;
		pname = xstrdup(av[i]);
		if ((qname = strchr(pname, '>')) != NULL)
		    *qname++ = '\0';
		bdp->pname = pname;
		/*@-usereleased@*/
		bdp->qname = qname;
		/*@=usereleased@*/
		rpmlog(msglvl,
			_("ignore package name relation(s) [%d]\t%s -> %s\n"),
			i, bdp->pname, (bdp->qname ? bdp->qname : "???"));
	    }
	    bdp->pname = NULL;
	    bdp->qname = NULL;
	}
	av = _free(av);
	s = _free(s);
	badDepsInitialized++;
    }

    /*@-compdef@*/
    if (badDeps != NULL)
    for (bdp = badDeps; bdp->pname != NULL && bdp->qname != NULL; bdp++) {
	if (!strcmp(rpmteN(p), bdp->pname) && !strcmp(rpmteN(q), bdp->qname))
	    return 1;
    }
    return 0;
    /*@=compdef@*/
}

/**
 * Recursively mark all nodes with their predecessors.
 * @param tsi		successor chain
 * @param q		predecessor
 */
static void markLoop(/*@special@*/ tsortInfo tsi, rpmte q)
	/*@globals internalState @*/
	/*@uses tsi @*/
	/*@modifies internalState @*/
{
    rpmte p;

    while (tsi != NULL && (p = tsi->tsi_suc) != NULL) {
	tsi = tsi->tsi_next;
	if (rpmteTSI(p)->tsi_chain != NULL)
	    continue;
	/*@-assignexpose -temptrans@*/
	rpmteTSI(p)->tsi_chain = q;
	/*@=assignexpose =temptrans@*/
	if (rpmteTSI(p)->tsi_next != NULL)
	    markLoop(rpmteTSI(p)->tsi_next, p);
    }
}

/*
 * Return display string a dependency, adding contextual flags marker.
 * @param f		dependency flags
 * @return		display string
 */
static inline /*@observer@*/ const char * identifyDepend(rpmuint32_t f)
	/*@*/
{
    f = _notpre(f);
    if (f & RPMSENSE_SCRIPT_PRE)
	return "Requires(pre):";
    if (f & RPMSENSE_SCRIPT_POST)
	return "Requires(post):";
    if (f & RPMSENSE_SCRIPT_PREUN)
	return "Requires(preun):";
    if (f & RPMSENSE_SCRIPT_POSTUN)
	return "Requires(postun):";
    if (f & RPMSENSE_SCRIPT_VERIFY)
	return "Requires(verify):";
    if (f & RPMSENSE_MISSINGOK)
	return "Requires(hint):";
    if (f & RPMSENSE_FIND_REQUIRES)
	return "Requires(auto):";
    return "Requires:";
}

/**
 * Find (and eliminate co-requisites) "q <- p" relation in dependency loop.
 * Search all successors of q for instance of p. Format the specific relation,
 * (e.g. p contains "Requires: q"). Unlink and free co-requisite (i.e.
 * pure Requires: dependencies) successor node(s).
 * @param q		sucessor (i.e. package required by p)
 * @param p		predecessor (i.e. package that "Requires: q")
 * @param zap		max. no. of co-requisites to remove (-1 is all)?
 * @retval nzaps	address of no. of relations removed
 * @param msglvl	message level at which to spew
 * @return		(possibly NULL) formatted "q <- p" releation (malloc'ed)
 */
/*@-mustmod@*/ /* FIX: hack modifies, but -type disables */
static /*@owned@*/ /*@null@*/ const char *
zapRelation(rpmte q, rpmte p,
		int zap, /*@in@*/ /*@out@*/ int * nzaps, int msglvl)
	/*@globals rpmGlobalMacroContext, h_errno, internalState @*/
	/*@modifies q, p, *nzaps, rpmGlobalMacroContext, internalState @*/
{
    rpmds requires;
    tsortInfo tsi_prev;
    tsortInfo tsi;
    const char *dp = NULL;

    for (tsi_prev = rpmteTSI(q), tsi = rpmteTSI(q)->tsi_next;
	 tsi != NULL;
	/* XXX Note: the loop traverses "not found", break on "found". */
	/*@-nullderef@*/
	 tsi_prev = tsi, tsi = tsi->tsi_next)
	/*@=nullderef@*/
    {
	rpmuint32_t Flags;

	/*@-abstractcompare@*/
	if (tsi->tsi_suc != p)
	    continue;
	/*@=abstractcompare@*/

	requires = rpmteDS((rpmteType(p) == TR_REMOVED ? q : p), tsi->tsi_tagn);
	if (requires == NULL) continue;		/* XXX can't happen */

	(void) rpmdsSetIx(requires, tsi->tsi_reqx);

	Flags = rpmdsFlags(requires);

	dp = rpmdsNewDNEVR( identifyDepend(Flags), requires);

	/*
	 * Attempt to unravel a dependency loop by eliminating Requires's.
	 */
	if (zap) {
	    rpmlog(msglvl,
			_("removing %s \"%s\" from tsort relations.\n"),
			(rpmteNEVRA(p) ?  rpmteNEVRA(p) : "???"), dp);
	    rpmteTSI(p)->tsi_count--;
	    if (tsi_prev) tsi_prev->tsi_next = tsi->tsi_next;
	    tsi->tsi_next = NULL;
	    tsi->tsi_suc = NULL;
	    tsi = _free(tsi);
	    if (nzaps)
		(*nzaps)++;
	    if (zap)
		zap--;
	}
	/* XXX Note: the loop traverses "not found", get out now! */
	break;
    }
    return dp;
}
/*@=mustmod@*/

/**
 * Record next "q <- p" relation (i.e. "p" requires "q").
 * @param ts		transaction set
 * @param p		predecessor (i.e. package that "Requires: q")
 * @param selected	boolean package selected array
 * @param requires	relation
 * @return		0 always
 */
/*@-mustmod@*/
static inline int addRelation(rpmts ts,
		/*@dependent@*/ rpmte p,
		unsigned char * selected,
		rpmds requires)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, p, *selected, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    rpmtsi qi; rpmte q;
    tsortInfo tsi;
    nsType NSType = rpmdsNSType(requires);
    fnpyKey key;
    int teType = rpmteType(p);
    alKey pkgKey;
    int i = 0;
    rpmal al = (teType == TR_ADDED ? ts->addedPackages : ts->erasedPackages);

    /* Avoid certain NS dependencies. */
    switch (NSType) {
    case RPMNS_TYPE_RPMLIB:
    case RPMNS_TYPE_CONFIG:
    case RPMNS_TYPE_CPUINFO:
    case RPMNS_TYPE_GETCONF:
    case RPMNS_TYPE_UNAME:
    case RPMNS_TYPE_SONAME:
    case RPMNS_TYPE_ACCESS:
    case RPMNS_TYPE_USER:
    case RPMNS_TYPE_GROUP:
    case RPMNS_TYPE_MOUNTED:
    case RPMNS_TYPE_DISKSPACE:
    case RPMNS_TYPE_DIGEST:
    case RPMNS_TYPE_GNUPG:
    case RPMNS_TYPE_MACRO:
    case RPMNS_TYPE_ENVVAR:
    case RPMNS_TYPE_RUNNING:
    case RPMNS_TYPE_SANITY:
    case RPMNS_TYPE_VCHECK:
    case RPMNS_TYPE_SIGNATURE:
	return 0;
	/*@notreached@*/ break;
    default:
	break;
    }

    pkgKey = RPMAL_NOMATCH;
    key = rpmalSatisfiesDepend(al, requires, &pkgKey);

    /* Ordering depends only on added/erased package relations. */
    if (pkgKey == RPMAL_NOMATCH)
	return 0;

/* XXX Set q to the added/removed package that was found. */
    /* XXX pretend erasedPackages are just appended to addedPackages. */
    if (teType == TR_REMOVED)
	pkgKey = (alKey)(((long)pkgKey) + ts->numAddedPackages);

    for (qi = rpmtsiInit(ts), i = 0; (q = rpmtsiNext(qi, 0)) != NULL; i++) {
	if (pkgKey == rpmteAddedKey(q))
	    break;
    }
    qi = rpmtsiFree(qi);
    if (q == NULL || i >= ts->orderCount)
	return 0;

    /* Avoid certain dependency relations. */
    if (ignoreDep(ts, p, q))
	return 0;

    /* Avoid redundant relations. */
    if (selected[i] != 0)
	return 0;
    selected[i] = 1;

    /* Erasures are reversed installs. */
    if (teType == TR_REMOVED) {
	rpmte r = p;
	p = q;
	q = r;
    }

    /* T3. Record next "q <- p" relation (i.e. "p" requires "q"). */
    rpmteTSI(p)->tsi_count++;			/* bump p predecessor count */

    if (rpmteDepth(p) <= rpmteDepth(q))	/* Save max. depth in dependency tree */
	(void) rpmteSetDepth(p, (rpmteDepth(q) + 1));
    if (rpmteDepth(p) > ts->maxDepth)
	ts->maxDepth = rpmteDepth(p);

    tsi = xcalloc(1, sizeof(*tsi));
    tsi->tsi_suc = p;

    tsi->tsi_tagn = rpmdsTagN(requires);
    tsi->tsi_reqx = rpmdsIx(requires);

    tsi->tsi_next = rpmteTSI(q)->tsi_next;
    rpmteTSI(q)->tsi_next = tsi;
    rpmteTSI(q)->tsi_qcnt++;			/* bump q successor count */

    return 0;
}
/*@=mustmod@*/

/**
 * Compare ordered list entries by index (qsort/bsearch).
 * @param one		1st ordered list entry
 * @param two		2nd ordered list entry
 * @return		result of comparison
 */
static int orderListIndexCmp(const void * one, const void * two)	/*@*/
{
    /*@-castexpose@*/
    long a = (long) ((const orderListIndex)one)->pkgKey;
    long b = (long) ((const orderListIndex)two)->pkgKey;
    /*@=castexpose@*/
    return (a - b);
}

/**
 * Add element to list sorting by tsi_qcnt.
 * @param p		new element
 * @retval *qp		first element
 * @retval *rp		last element
 * @param prefcolor	preferred color
 */
/*@-mustmod@*/
static void addQ(/*@dependent@*/ rpmte p,
		/*@in@*/ /*@out@*/ rpmte * qp,
		/*@in@*/ /*@out@*/ rpmte * rp,
		rpmuint32_t prefcolor)
	/*@modifies p, *qp, *rp @*/
{
    rpmte q, qprev;

    /* Mark the package as queued. */
    rpmteTSI(p)->tsi_queued = 1;

    if ((*rp) == NULL) {	/* 1st element */
	/*@-dependenttrans@*/ /* FIX: double indirection */
	(*rp) = (*qp) = p;
	/*@=dependenttrans@*/
	return;
    }

    /* Find location in queue using metric tsi_qcnt. */
    for (qprev = NULL, q = (*qp);
	 q != NULL;
	 qprev = q, q = rpmteTSI(q)->tsi_suc)
    {
	/* XXX Insure preferred color first. */
	if (rpmteColor(p) != prefcolor && rpmteColor(p) != rpmteColor(q))
	    continue;

	/* XXX Insure removed after added. */
	if (rpmteType(p) == TR_REMOVED && rpmteType(p) != rpmteType(q))
	    continue;
	if (rpmteTSI(q)->tsi_qcnt <= rpmteTSI(p)->tsi_qcnt)
	    break;
    }

    if (qprev == NULL) {	/* insert at beginning of list */
	rpmteTSI(p)->tsi_suc = q;
	/*@-dependenttrans@*/
	(*qp) = p;		/* new head */
	/*@=dependenttrans@*/
    } else if (q == NULL) {	/* insert at end of list */
	rpmteTSI(qprev)->tsi_suc = p;
	/*@-dependenttrans@*/
	(*rp) = p;		/* new tail */
	/*@=dependenttrans@*/
    } else {			/* insert between qprev and q */
	rpmteTSI(p)->tsi_suc = q;
	rpmteTSI(qprev)->tsi_suc = p;
    }
}
/*@=mustmod@*/

/*@unchecked@*/
#ifdef	NOTYET
static rpmuint32_t _autobits = _notpre(_ALL_REQUIRES_MASK);
#else
static rpmuint32_t _autobits = 0xffffffff;
#endif
#define isAuto(_x)	((_x) & _autobits)

/*@unchecked@*/
static int slashDepth = 100;	/* #slashes pemitted in parentdir deps. */

static int countSlashes(const char * dn)
	/*@*/
{
    int nslashes = 0;
    int c;

    while ((c = (int)*dn++) != 0) {
	switch (c) {
	default:	continue;	/*@notreached@*/ /*@switchbreak@*/break;
	case '/':	nslashes++;	/*@switchbreak@*/break;
	}
    }

    return nslashes;
}

int rpmtsOrder(rpmts ts)
{
    rpmds requires;
    rpmuint32_t Flags;
    int anaconda = rpmtsDFlags(ts) & RPMDEPS_FLAG_ANACONDA;
    rpmuint32_t prefcolor = rpmtsPrefColor(ts);
    rpmtsi pi; rpmte p;
    rpmtsi qi; rpmte q;
    rpmtsi ri; rpmte r;
    tsortInfo tsi;
    tsortInfo tsi_next;
    alKey * ordering;
    int orderingCount = 0;
    unsigned char * selected = alloca(sizeof(*selected) * (ts->orderCount + 1));
    int loopcheck;
    rpmte * newOrder;
    int newOrderCount = 0;
    orderListIndex orderList;
    int numOrderList;
    int npeer = 128;	/* XXX more than deep enough for now. */
    int * peer = memset(alloca(npeer*sizeof(*peer)), 0, (npeer*sizeof(*peer)));
    int nrescans = 10;
    int _printed = 0;
    char deptypechar;
    size_t tsbytes;
    int oType = 0;
    int treex;
    int depth;
    int breadth;
    int qlen;
    int i, j;

#ifdef	DYING
    rpmalMakeIndex(ts->addedPackages);
#endif

    /* Create erased package index. */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, TR_REMOVED)) != NULL) {
	alKey pkgKey;
	fnpyKey key;
	rpmuint32_t tscolor = rpmtsColor(ts);
	pkgKey = RPMAL_NOMATCH;
/*@-abstract@*/
	key = (fnpyKey) p;
/*@=abstract@*/
	pkgKey = rpmalAdd(&ts->erasedPackages, pkgKey, key,
			rpmteDS(p, RPMTAG_PROVIDENAME),
			rpmteFI(p, RPMTAG_BASENAMES), tscolor);
	/* XXX pretend erasedPackages are just appended to addedPackages. */
	pkgKey = (alKey)(((long)pkgKey) + ts->numAddedPackages);
	(void) rpmteSetAddedKey(p, pkgKey);
    }
    pi = rpmtsiFree(pi);
    rpmalMakeIndex(ts->erasedPackages);

    (void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_ORDER), 0);

    /* T1. Initialize. */
    if (oType == 0)
	numOrderList = ts->orderCount;
    else {
	numOrderList = 0;
	if (oType & TR_ADDED)
	    numOrderList += ts->numAddedPackages;
	if (oType & TR_REMOVED)
	    numOrderList += ts->numRemovedPackages;
     }
    ordering = alloca(sizeof(*ordering) * (numOrderList + 1));
    loopcheck = numOrderList;
    tsbytes = 0;

    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, oType)) != NULL)
	rpmteNewTSI(p);
    pi = rpmtsiFree(pi);

    /* Record all relations. */
    rpmlog(RPMLOG_DEBUG, D_("========== recording tsort relations\n"));
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, oType)) != NULL) {

	memset(selected, 0, sizeof(*selected) * ts->orderCount);

      if ((requires = rpmteDS(p, RPMTAG_REQUIRENAME)) != NULL) {

	/* Avoid narcisstic relations. */
	selected[rpmtsiOc(pi)] = 1;

	/* T2. Next "q <- p" relation. */

	/* First, do pre-requisites. */
	requires = rpmdsInit(requires);
	if (requires != NULL)
	while (rpmdsNext(requires) >= 0) {

	    Flags = rpmdsFlags(requires);
	    if (!isAuto(Flags))
		/*@innercontinue@*/ continue;

	    switch (rpmteType(p)) {
	    case TR_REMOVED:
		/* Skip if not %preun/%postun requires. */
		if (!isErasePreReq(Flags))
		    /*@innercontinue@*/ continue;
		/*@switchbreak@*/ break;
	    case TR_ADDED:
		/* Skip if not %pre/%post requires. */
		if (!isInstallPreReq(Flags))
		    /*@innercontinue@*/ continue;
		/*@switchbreak@*/ break;
	    }

	    /* T3. Record next "q <- p" relation (i.e. "p" requires "q"). */
	    (void) addRelation(ts, p, selected, requires);

	}

	/* Then do co-requisites. */
	requires = rpmdsInit(requires);
	if (requires != NULL)
	while (rpmdsNext(requires) >= 0) {

	    Flags = rpmdsFlags(requires);
	    if (!isAuto(Flags))
		/*@innercontinue@*/ continue;

	    switch (rpmteType(p)) {
	    case TR_REMOVED:
		/* Skip if %preun/%postun requires. */
		if (isErasePreReq(Flags))
		    /*@innercontinue@*/ continue;
		/*@switchbreak@*/ break;
	    case TR_ADDED:
		/* Skip if %pre/%post requires. */
		if (isInstallPreReq(Flags))
		    /*@innercontinue@*/ continue;
		/*@switchbreak@*/ break;
	    }

	    /* T3. Record next "q <- p" relation (i.e. "p" requires "q"). */
	    (void) addRelation(ts, p, selected, requires);

	}
      }

	/* Ensure that erasures follow installs during upgrades. */
      if (rpmteType(p) == TR_REMOVED && p->flink.Pkgid && p->flink.Pkgid[0]) {

	qi = rpmtsiInit(ts);
	while ((q = rpmtsiNext(qi, TR_ADDED)) != NULL) {
	    if (strcmp(q->pkgid, p->flink.Pkgid[0]))
		/*@innercontinue@*/ continue;
	    requires = rpmdsFromPRCO(q->PRCO, RPMTAG_NAME);
	    if (requires != NULL) {
		/* XXX disable erased arrow reversal. */
		p->type = TR_ADDED;
		(void) addRelation(ts, p, selected, requires);
		p->type = TR_REMOVED;
	    }
	}
	qi = rpmtsiFree(qi);
      }

      {

	/* Order by requiring parent directories pre-requsites. */
	requires = rpmdsInit(rpmteDS(p, RPMTAG_DIRNAMES));
	if (requires != NULL)
	while (rpmdsNext(requires) >= 0) {

	    /* XXX Attempt to avoid loops by filtering out deep paths. */
	    if (countSlashes(rpmdsN(requires)) > slashDepth)
		/*@innercontinue@*/ continue;

	    /* T3. Record next "q <- p" relation (i.e. "p" requires "q"). */
	    (void) addRelation(ts, p, selected, requires);

	}

	/* Order by requiring no dangling symlinks. */
	requires = rpmdsInit(rpmteDS(p, RPMTAG_FILELINKTOS));
	if (requires != NULL)
	while (rpmdsNext(requires) >= 0) {

	    /* T3. Record next "q <- p" relation (i.e. "p" requires "q"). */
	    (void) addRelation(ts, p, selected, requires);

	}
      }

    }
    pi = rpmtsiFree(pi);

    /* Save predecessor count and mark tree roots. */
    treex = 0;
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, oType)) != NULL) {
	int npreds;

	npreds = rpmteTSI(p)->tsi_count;

	(void) rpmteSetNpreds(p, npreds);
	(void) rpmteSetDepth(p, 0);

	if (npreds == 0) {
	    treex++;
	    (void) rpmteSetTree(p, treex);
	    (void) rpmteSetBreadth(p, treex);
	} else
	    (void) rpmteSetTree(p, -1);
#ifdef	UNNECESSARY
	(void) rpmteSetParent(p, NULL);
#endif

    }
    pi = rpmtsiFree(pi);
    ts->ntrees = treex;

    /* T4. Scan for zeroes. */
    rpmlog(RPMLOG_DEBUG, D_("========== tsorting packages (order, #predecessors, #succesors, tree, Ldepth, Rbreadth)\n"));

rescan:
    if (pi != NULL) pi = rpmtsiFree(pi);
    q = r = NULL;
    qlen = 0;
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, oType)) != NULL) {

	/* Prefer packages in chainsaw or anaconda presentation order. */
	if (anaconda)
	    rpmteTSI(p)->tsi_qcnt = (ts->orderCount - rpmtsiOc(pi));

	if (rpmteTSI(p)->tsi_count != 0)
	    continue;
	rpmteTSI(p)->tsi_suc = NULL;
	addQ(p, &q, &r, prefcolor);
	qlen++;
    }
    pi = rpmtsiFree(pi);

    /* T5. Output front of queue (T7. Remove from queue.) */
    for (; q != NULL; q = rpmteTSI(q)->tsi_suc) {

	/* Mark the package as unqueued. */
	rpmteTSI(q)->tsi_queued = 0;

	if (oType != 0)
	switch (rpmteType(q)) {
	case TR_ADDED:
	    if (!(oType & TR_ADDED))
		continue;
	    /*@switchbreak@*/ break;
	case TR_REMOVED:
	    if (!(oType & TR_REMOVED))
		continue;
	    /*@switchbreak@*/ break;
	default:
	    continue;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	}
	deptypechar = (rpmteType(q) == TR_REMOVED ? '-' : '+');

	treex = rpmteTree(q);
	depth = rpmteDepth(q);
	breadth = ((depth < npeer) ? peer[depth]++ : 0);
	(void) rpmteSetBreadth(q, breadth);

	rpmlog(RPMLOG_DEBUG, "%5d%5d%5d%5d%5d%5d %*s%c%s\n",
			orderingCount, rpmteNpreds(q),
			rpmteTSI(q)->tsi_qcnt,
			treex, depth, breadth,
			(2 * depth), "",
			deptypechar,
			(rpmteNEVRA(q) ? rpmteNEVRA(q) : "???"));

	(void) rpmteSetDegree(q, 0);
	tsbytes += rpmtePkgFileSize(q);

	ordering[orderingCount] = rpmteAddedKey(q);
	orderingCount++;
	qlen--;
	loopcheck--;

	/* T6. Erase relations. */
	tsi_next = rpmteTSI(q)->tsi_next;
	rpmteTSI(q)->tsi_next = NULL;
	while ((tsi = tsi_next) != NULL) {
	    tsi_next = tsi->tsi_next;
	    tsi->tsi_next = NULL;
	    p = tsi->tsi_suc;
	    if (p && (--rpmteTSI(p)->tsi_count) <= 0) {

		(void) rpmteSetTree(p, treex);
		(void) rpmteSetDepth(p, depth+1);
		(void) rpmteSetParent(p, q);
		(void) rpmteSetDegree(q, rpmteDegree(q)+1);

		/* XXX TODO: add control bit. */
		rpmteTSI(p)->tsi_suc = NULL;
/*@-nullstate@*/	/* XXX FIX: rpmteTSI(q)->tsi_suc can be NULL. */
		addQ(p, &rpmteTSI(q)->tsi_suc, &r, prefcolor);
/*@=nullstate@*/
		qlen++;
	    }
	    tsi = _free(tsi);
	}
	if (!_printed && loopcheck == qlen && rpmteTSI(q)->tsi_suc != NULL) {
	    _printed++;
	    (void) rpmtsUnorderedSuccessors(ts, orderingCount);
	    rpmlog(RPMLOG_DEBUG,
		D_("========== successors only (%d bytes)\n"), (int)tsbytes);

	    /* Relink the queue in presentation order. */
	    tsi = rpmteTSI(q);
	    pi = rpmtsiInit(ts);
	    while ((p = rpmtsiNext(pi, oType)) != NULL) {
		/* Is this element in the queue? */
		if (rpmteTSI(p)->tsi_queued == 0)
		    /*@innercontinue@*/ continue;
		tsi->tsi_suc = p;
		tsi = rpmteTSI(p);
	    }
	    pi = rpmtsiFree(pi);
	    tsi->tsi_suc = NULL;
	}
    }

    /* T8. End of process. Check for loops. */
    if (loopcheck != 0) {
	int nzaps;

	/* T9. Initialize predecessor chain. */
	nzaps = 0;
	qi = rpmtsiInit(ts);
	while ((q = rpmtsiNext(qi, oType)) != NULL) {
	    rpmteTSI(q)->tsi_chain = NULL;
	    rpmteTSI(q)->tsi_queued = 0;
	    /* Mark packages already sorted. */
	    if (rpmteTSI(q)->tsi_count == 0)
		rpmteTSI(q)->tsi_count = -1;
	}
	qi = rpmtsiFree(qi);

	/* T10. Mark all packages with their predecessors. */
	qi = rpmtsiInit(ts);
	while ((q = rpmtsiNext(qi, oType)) != NULL) {
	    if ((tsi = rpmteTSI(q)->tsi_next) == NULL)
		continue;
	    rpmteTSI(q)->tsi_next = NULL;
	    markLoop(tsi, q);
	    rpmteTSI(q)->tsi_next = tsi;
	}
	qi = rpmtsiFree(qi);

	/* T11. Print all dependency loops. */
	ri = rpmtsiInit(ts);
	while ((r = rpmtsiNext(ri, oType)) != NULL)
	{
	    int printed;

	    printed = 0;

	    /* T12. Mark predecessor chain, looking for start of loop. */
	    for (q = rpmteTSI(r)->tsi_chain; q != NULL;
		 q = rpmteTSI(q)->tsi_chain)
	    {
		if (rpmteTSI(q)->tsi_queued)
		    /*@innerbreak@*/ break;
		rpmteTSI(q)->tsi_queued = 1;
	    }

	    /* T13. Print predecessor chain from start of loop. */
	    while ((p = q) != NULL && (q = rpmteTSI(p)->tsi_chain) != NULL) {
#if 0
		const char * nevra;
#endif
		const char * dp;
		rpmlogLvl msglvl = (anaconda || (rpmtsDFlags(ts) & RPMDEPS_FLAG_DEPLOOPS))
			? RPMLOG_WARNING : RPMLOG_ERR;
#if defined(RPM_VENDOR_MANDRIVA) /* loop-detection-optional-loglevel */
		// Report loops as debug-level message by default (7 = RPMLOG_DEBUG), overridable
		msglvl = rpmExpandNumeric("%{?_loop_detection_loglevel}%{?!_loop_detection_loglevel:7}");
#endif

		/* Unchain predecessor loop. */
		rpmteTSI(p)->tsi_chain = NULL;

		if (!printed) {
		    rpmlog(msglvl, _("LOOP:\n"));
		    printed = 1;
		}

		/* Find (and destroy if co-requisite) "q <- p" relation. */
		dp = zapRelation(q, p, 1, &nzaps, msglvl);

#if 0
		/* Print next member of loop. */
		nevra = rpmteNEVRA(p);
		rpmlog(msglvl, "    %-40s %s\n", (nevra ? nevra : "???"),
			(dp ? dp : "not found!?!"));
#endif

		dp = _free(dp);
	    }

	    /* Walk (and erase) linear part of predecessor chain as well. */
	    for (p = r, q = rpmteTSI(r)->tsi_chain; q != NULL;
		 p = q, q = rpmteTSI(q)->tsi_chain)
	    {
		/* Unchain linear part of predecessor loop. */
		rpmteTSI(p)->tsi_chain = NULL;
		rpmteTSI(p)->tsi_queued = 0;
	    }
	}
	ri = rpmtsiFree(ri);

	/* If a relation was eliminated, then continue sorting. */
	/* XXX TODO: add control bit. */
	if (nzaps && nrescans-- > 0) {
	    rpmlog(RPMLOG_DEBUG, D_("========== continuing tsort ...\n"));
	    goto rescan;
	}

	/* Return no. of packages that could not be ordered. */
	rpmlog(RPMLOG_ERR, _("rpmtsOrder failed, %d elements remain\n"),
			loopcheck);

#ifdef	NOTYET
	/* Do autorollback goal since we could not sort this transaction properly. */
	(void) rpmtsRollback(ts, RPMPROB_FILTER_NONE, 0, NULL);
#endif

	return loopcheck;
    }

    /* Clean up tsort remnants (if any). */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL)
	rpmteFreeTSI(p);
    pi = rpmtsiFree(pi);

    /*
     * The order ends up as installed packages followed by removed packages.
     */
    orderList = xcalloc(numOrderList, sizeof(*orderList));
    j = 0;
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, oType)) != NULL) {
	/* Prepare added/erased package ordering permutation. */
	orderList[j].pkgKey = rpmteAddedKey(p);
	orderList[j].orIndex = rpmtsiOc(pi);
	j++;
    }
    pi = rpmtsiFree(pi);

    qsort(orderList, numOrderList, sizeof(*orderList), orderListIndexCmp);

/*@-type@*/
    newOrder = xcalloc(ts->orderCount, sizeof(*newOrder));
/*@=type@*/
    for (i = 0, newOrderCount = 0; i < orderingCount; i++)
    {
	struct orderListIndex_s key;
	orderListIndex needle;

	key.pkgKey = ordering[i];
	needle = bsearch(&key, orderList, numOrderList,
				sizeof(key), orderListIndexCmp);
	if (needle == NULL)	/* XXX can't happen */
	    continue;

	j = needle->orIndex;
	if ((q = ts->order[j]) == NULL || needle->pkgKey == RPMAL_NOMATCH)
	    continue;

	newOrder[newOrderCount++] = q;
	ts->order[j] = NULL;
    }

assert(newOrderCount == ts->orderCount);

/*@+voidabstract@*/
    ts->order = _free(ts->order);
/*@=voidabstract@*/
    ts->order = newOrder;
    ts->orderAlloced = ts->orderCount;
    orderList = _free(orderList);

#ifdef	DYING	/* XXX now done at the CLI level just before rpmtsRun(). */
    rpmtsClean(ts);
#endif
    freeBadDeps();

    (void) rpmswExit(rpmtsOp(ts, RPMTS_OP_ORDER), 0);

    return 0;
}

int rpmtsCheck(rpmts ts)
{
    const char * depName = NULL;
    rpmdepFlags depFlags = rpmtsDFlags(ts);
    rpmuint32_t tscolor = rpmtsColor(ts);
    rpmdbMatchIterator mi = NULL;
    rpmtsi pi = NULL; rpmte p;
    int closeatexit = 0;
    int xx;
    int terminate = 2;		/* XXX terminate if rc >= terminate */
    int rc = 0;
    int ourrc = 0;

    (void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_CHECK), 0);

    /* Do lazy, readonly, open of rpm database. */
    if (rpmtsGetRdb(ts) == NULL && rpmtsDBMode(ts) != -1) {
	rc = (rpmtsOpenDB(ts, rpmtsDBMode(ts)) ? 2 : 0);
	closeatexit = (rc == 0);
    }
    if (rc && (ourrc = rc) >= terminate)
	goto exit;

    ts->probs = rpmpsFree(ts->probs);

    rpmalMakeIndex(ts->addedPackages);

    /*
     * Look at all of the added packages and make sure their dependencies
     * are satisfied.
     */
    pi = rpmtsiInit(ts);
    while (ourrc < terminate && (p = rpmtsiNext(pi, TR_ADDED)) != NULL) {
	rpmds provides, requires, conflicts, dirnames, linktos;
	rpmfi fi;

/*@-nullpass@*/	/* FIX: rpmts{A,O} can return null. */
	rpmlog(RPMLOG_DEBUG, "========== +++ %s %s/%s 0x%x\n",
		rpmteNEVR(p), rpmteA(p), rpmteO(p), rpmteColor(p));
/*@=nullpass@*/
	requires = (!(depFlags & RPMDEPS_FLAG_NOREQUIRES)
	    ? rpmteDS(p, RPMTAG_REQUIRENAME) : NULL);
	conflicts = (!(depFlags & RPMDEPS_FLAG_NOCONFLICTS)
	    ? rpmteDS(p, RPMTAG_CONFLICTNAME) : NULL);
	/* XXX srpm's don't have directory paths. */
	if (p->isSource) {
	    dirnames = NULL;
	    linktos = NULL;
	} else {
	    dirnames = (!(depFlags & RPMDEPS_FLAG_NOPARENTDIRS)
		? rpmteDS(p, RPMTAG_DIRNAMES) : NULL);
	    linktos = (!(depFlags & RPMDEPS_FLAG_NOLINKTOS)
		? rpmteDS(p, RPMTAG_FILELINKTOS) : NULL);
	}

	rc = checkPackageDeps(ts, rpmteNEVRA(p),
			requires, conflicts, dirnames, linktos,
			NULL, tscolor, 1);
	if (rc && (ourrc = rc) >= terminate)
	    break;

	provides = rpmteDS(p, RPMTAG_PROVIDENAME);
	provides = rpmdsInit(provides);
	if (provides != NULL)
	while (ourrc < terminate && rpmdsNext(provides) >= 0) {
	    depName = _free(depName);
	    depName = xstrdup(rpmdsN(provides));

#ifdef	NOTYET
	    if (rpmdsNSType(provides) == RPMNS_TYPE_ENVVAR) {
		const char * EVR = rpmdsEVR(provides);
		if (rpmdsNegateRC(provides, 0))
		    EVR = NULL;
		if (envPut(depName, EVR));
		    rc = 2;
	    } else
#endif

	    /* Adding: check provides key against conflicts matches. */
	    if (checkDependentConflicts(ts, depName))
		rc = 1;
	}
	if (rc && (ourrc = rc) >= terminate)
	    break;

	fi = rpmteFI(p, RPMTAG_BASENAMES);
	fi = rpmfiInit(fi, 0);
	while (ourrc < terminate && rpmfiNext(fi) >= 0) {
	    depName = _free(depName);
	    depName = xstrdup(rpmfiFN(fi));
	    /* Adding: check filename against conflicts matches. */
	    if (checkDependentConflicts(ts, depName))
		rc = 1;
	}
	if (rc && (ourrc = rc) >= terminate)
	    break;
    }
    pi = rpmtsiFree(pi);
    if (rc && (ourrc = rc) >= terminate)
	goto exit;

    /*
     * Look at the removed packages and make sure they aren't critical.
     */
    pi = rpmtsiInit(ts);
    while (ourrc < terminate && (p = rpmtsiNext(pi, TR_REMOVED)) != NULL) {
	rpmds provides;
	rpmfi fi;

/*@-nullpass@*/	/* FIX: rpmts{A,O} can return null. */
	rpmlog(RPMLOG_DEBUG, "========== --- %s %s/%s 0x%x\n",
		rpmteNEVR(p), rpmteA(p), rpmteO(p), rpmteColor(p));
/*@=nullpass@*/

	provides = rpmteDS(p, RPMTAG_PROVIDENAME);
	provides = rpmdsInit(provides);
	if (provides != NULL)
	while (ourrc < terminate && rpmdsNext(provides) >= 0) {
	    depName = _free(depName);
	    depName = xstrdup(rpmdsN(provides));

	    /* Erasing: check provides against requiredby matches. */
	    if (checkDependentPackages(ts, depName))
		rc = 1;
	}
	if (rc && (ourrc = rc) >= terminate)
	    break;

	fi = rpmteFI(p, RPMTAG_BASENAMES);
	fi = rpmfiInit(fi, 0);
	while (ourrc < terminate && rpmfiNext(fi) >= 0) {
	    depName = _free(depName);
	    depName = xstrdup(rpmfiFN(fi));
	    /* Erasing: check filename against requiredby matches. */
	    if (checkDependentPackages(ts, depName))
		rc = 1;
	}
	if (rc && (ourrc = rc) >= terminate)
	    break;
    }
    pi = rpmtsiFree(pi);
    if (rc && (ourrc = rc) >= terminate)
	goto exit;

    /*
     * Make sure transaction dependencies are satisfied.
     */
    {	const char * tsNEVRA = "transaction dependencies";
	rpmds R = rpmdsFromPRCO(rpmtsPRCO(ts), RPMTAG_REQUIRENAME);
	rpmds C = rpmdsFromPRCO(rpmtsPRCO(ts), RPMTAG_CONFLICTNAME);
	rpmds D = NULL;
	rpmds L = NULL;
	const char * dep = NULL;
	int adding = 2;
	tscolor = 0;	/* XXX no coloring for transaction dependencies. */
	rc = checkPackageDeps(ts, tsNEVRA, R, C, D, L, dep, tscolor, adding);
    }
    if (rc && (ourrc = rc) >= terminate)
	goto exit;

exit:
    mi = rpmdbFreeIterator(mi);
    pi = rpmtsiFree(pi);
    depName = _free(depName);

    (void) rpmswExit(rpmtsOp(ts, RPMTS_OP_CHECK), 0);

    if (closeatexit)
	xx = rpmtsCloseDB(ts);
#if defined(CACHE_DEPENDENCY_RESULT)
    else if (_cacheDependsRC)
	xx = rpmdbCloseDBI(rpmtsGetRdb(ts), RPMDBI_DEPENDS);
#endif

#ifdef	NOTYET
     /* On failed dependencies, perform the autorollback goal (if any). */
    {	rpmps ps = rpmtsProblems(ts);
	if (rc || rpmpsNumProblems(ps) > 0)
	    (void) rpmtsRollback(ts, RPMPROB_FILTER_NONE, 0, NULL);
	ps = rpmpsFree(ps);
    }
#endif

    return ourrc;
}

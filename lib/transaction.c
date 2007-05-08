/** \ingroup rpmts
 * \file lib/transaction.c
 */

#include "system.h"
#include "rpmcli.h"	/* IDTX prototypes */
#include <rpmlib.h>

#include <rpmmacro.h>	/* XXX for rpmExpand */

#include "fsm.h"
#include "psm.h"

#include "rpmdb.h"

#include "rpmds.h"

#include "rpmlock.h"

#define	_RPMFI_INTERNAL
#include "rpmfi.h"

#define	_RPMTE_INTERNAL
#include "rpmte.h"

#define	_RPMTS_INTERNAL
#include "rpmts.h"

#include "cpio.h"
#include "fprint.h"
#include "legacy.h"	/* XXX dodigest */
#include "misc.h" /* XXX stripTrailingChar, splitString, currentDirectory */

#include "debug.h"

/*@access Header @*/		/* XXX ts->notify arg1 is void ptr */
/*@access rpmps @*/	/* XXX need rpmProblemSetOK() */
/*@access dbiIndexSet @*/

/*@access rpmpsm @*/

/*@access alKey @*/
/*@access fnpyKey @*/

/*@access rpmfi @*/

/*@access rpmte @*/
/*@access rpmtsi @*/
/*@access rpmts @*/

/*@access IDT @*/
/*@access IDTX @*/
/*@access FD_t @*/

/* XXX: This is a hack.  I needed a to setup a notify callback
 * for the rollback transaction, but I did not want to create
 * a header for rpminstall.c.
 */
extern void * rpmShowProgress(/*@null@*/ const void * arg,
			const rpmCallbackType what,
			const unsigned long long amount,
			const unsigned long long total,
			/*@null@*/ fnpyKey key,
			/*@null@*/ void * data)
	/*@*/;

/**
 */
static int sharedCmp(const void * one, const void * two)
	/*@*/
{
    sharedFileInfo a = (sharedFileInfo) one;
    sharedFileInfo b = (sharedFileInfo) two;

    if (a->otherPkg < b->otherPkg)
	return -1;
    else if (a->otherPkg > b->otherPkg)
	return 1;

    return 0;
}

/**
 * @param ts		transaction set
 * @param p		current transaction element
 * @param fi		file info set
 * @param shared	shared file info
 * @param sharedCount	no. of shared elements
 * @param reportConflicts
 *
 * XXX only ts->{probs,rpmdb} modified
 */
/*@-bounds@*/
static int handleInstInstalledFiles(const rpmts ts,
		rpmte p, rpmfi fi,
		sharedFileInfo shared,
		int sharedCount, int reportConflicts)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, fi, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    uint_32 tscolor = rpmtsColor(ts);
    uint_32 prefcolor = rpmtsPrefColor(ts);
    uint_32 otecolor, tecolor;
    uint_32 oFColor, FColor;
    uint_32 oFFlags, FFlags;
    struct stat sb, *st = &sb;
    const char * altNEVRA = NULL;
    rpmfi otherFi = NULL;
    int numReplaced = 0;
    rpmps ps;
    int i;

    {	rpmdbMatchIterator mi;
	Header h;
	int scareMem = 0;

	mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES,
			&shared->otherPkg, sizeof(shared->otherPkg));
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    altNEVRA = hGetNEVRA(h, NULL);
	    otherFi = rpmfiNew(ts, h, RPMTAG_BASENAMES, scareMem);
	    break;
	}
	mi = rpmdbFreeIterator(mi);
    }

    /* Compute package color. */
    tecolor = rpmteColor(p);
    tecolor &= tscolor;

    /* Compute other pkg color. */
    otecolor = 0;
    otherFi = rpmfiInit(otherFi, 0);
    if (otherFi != NULL)
    while (rpmfiNext(otherFi) >= 0)
	otecolor |= rpmfiFColor(otherFi);
    otecolor &= tscolor;

    if (otherFi == NULL)
	return 1;

    fi->replaced = xcalloc(sharedCount, sizeof(*fi->replaced));

    ps = rpmtsProblems(ts);
    for (i = 0; i < sharedCount; i++, shared++) {
	int otherFileNum, fileNum;

	otherFileNum = shared->otherFileNum;
	(void) rpmfiSetFX(otherFi, otherFileNum);
	oFFlags = rpmfiFFlags(otherFi);
	oFColor = rpmfiFColor(otherFi);
	oFColor &= tscolor;

	fileNum = shared->pkgFileNum;
	(void) rpmfiSetFX(fi, fileNum);
	FFlags = rpmfiFFlags(fi);
	FColor = rpmfiFColor(fi);
	FColor &= tscolor;

#ifdef	DYING
	/* XXX another tedious segfault, assume file state normal. */
	if (otherStates && otherStates[otherFileNum] != RPMFILE_STATE_NORMAL)
	    continue;
#endif

	if (XFA_SKIPPING(fi->actions[fileNum]))
	    continue;

	/* Remove setuid/setgid bits on other (possibly hardlinked) files. */
	if (!(fi->mapflags & CPIO_SBIT_CHECK)) {
	    int_16 omode = rpmfiFMode(otherFi);
	    if (S_ISREG(omode) && (omode & 06000) != 0)
		fi->mapflags |= CPIO_SBIT_CHECK;
	}

	if (((FFlags | oFFlags) & RPMFILE_GHOST))
	    continue;

	/* Check for shared %config files that are installed and sparse. */
	if ((FFlags | oFFlags) & RPMFILE_CONFIG) {
	    if (!Lstat(rpmfiFN(fi), st)) {
		if (FFlags & RPMFILE_CONFIG) {
		    FFlags |= RPMFILE_EXISTS;
		    if ((512 * st->st_blocks) < st->st_size)
		 	FFlags |= RPMFILE_SPARSE;
		    (void) rpmfiSetFFlags(fi, FFlags);
		}
		if (oFFlags & RPMFILE_CONFIG) {
		    oFFlags |= RPMFILE_EXISTS;
		    if ((512 * st->st_blocks) < st->st_size)
		 	oFFlags |= RPMFILE_SPARSE;
		    (void) rpmfiSetFFlags(otherFi, oFFlags);
		}
	    }
	}

	if (rpmfiCompare(otherFi, fi)) {
	    int rConflicts;

	    rConflicts = reportConflicts;
	    /* Resolve file conflicts to prefer Elf64 (if not forced). */
	    if (tscolor != 0 && FColor != 0 && FColor != oFColor)
	    {
		if (oFColor & prefcolor) {
		    fi->actions[fileNum] = FA_SKIPCOLOR;
		    rConflicts = 0;
		} else
		if (FColor & prefcolor) {
		    fi->actions[fileNum] = FA_CREATE;
		    rConflicts = 0;
		}
	    }

	    if (rConflicts) {
		rpmpsAppend(ps, RPMPROB_FILE_CONFLICT,
			rpmteNEVRA(p), rpmteKey(p),
			rpmfiDN(fi), rpmfiBN(fi),
			altNEVRA,
			0);
	    }

	    /* Save file identifier to mark as state REPLACED. */
	    if ( !(((FFlags | oFFlags) & RPMFILE_CONFIG) || XFA_SKIPPING(fi->actions[fileNum])) ) {
		/*@-assignexpose@*/ /* FIX: p->replaced, not fi */
		if (!shared->isRemoved)
		    fi->replaced[numReplaced++] = *shared;
		/*@=assignexpose@*/
	    }
	}

	/* Determine config file dispostion, skipping missing files (if any). */
	if (((FFlags | oFFlags) & RPMFILE_CONFIG)) {
	    int skipMissing =
		((rpmtsFlags(ts) & RPMTRANS_FLAG_ALLFILES) ? 0 : 1);
	    fileAction action = rpmfiDecideFate(otherFi, fi, skipMissing);
	    fi->actions[fileNum] = action;
	}
	fi->replacedSizes[fileNum] = rpmfiFSize(otherFi);
    }
    ps = rpmpsFree(ps);

    altNEVRA = _free(altNEVRA);
    otherFi = rpmfiFree(otherFi);

    fi->replaced = xrealloc(fi->replaced,	/* XXX memory leak */
			   sizeof(*fi->replaced) * (numReplaced + 1));
    fi->replaced[numReplaced].otherPkg = 0;

    return 0;
}
/*@=bounds@*/

/**
 */
/* XXX only ts->rpmdb modified */
static int handleRmvdInstalledFiles(const rpmts ts, rpmfi fi,
		sharedFileInfo shared, int sharedCount)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, fi, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    HGE_t hge = fi->hge;
    Header h;
    const char * otherStates;
    int i, xx;

    rpmdbMatchIterator mi;

    mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES,
			&shared->otherPkg, sizeof(shared->otherPkg));
    h = rpmdbNextIterator(mi);
    if (h == NULL) {
	mi = rpmdbFreeIterator(mi);
	return 1;
    }

    xx = hge(h, RPMTAG_FILESTATES, NULL, (void **) &otherStates, NULL);

/*@-boundswrite@*/
    /* XXX there's an obscure segfault here w/o NULL check ... */
    if (otherStates != NULL)
    for (i = 0; i < sharedCount; i++, shared++) {
	int otherFileNum, fileNum;
	otherFileNum = shared->otherFileNum;
	fileNum = shared->pkgFileNum;

	if (otherStates[otherFileNum] != RPMFILE_STATE_NORMAL)
	    continue;

	fi->actions[fileNum] = FA_SKIP;
    }
/*@=boundswrite@*/

    mi = rpmdbFreeIterator(mi);

    return 0;
}

#define	ISROOT(_d)	(((_d)[0] == '/' && (_d)[1] == '\0') ? "" : (_d))

/*@unchecked@*/
int _fps_debug = 0;

static int fpsCompare (const void * one, const void * two)
	/*@*/
{
    const struct fingerPrint_s * a = (const struct fingerPrint_s *)one;
    const struct fingerPrint_s * b = (const struct fingerPrint_s *)two;
    int adnlen = strlen(a->entry->dirName);
    int asnlen = (a->subDir ? strlen(a->subDir) : 0);
    int abnlen = strlen(a->baseName);
    int bdnlen = strlen(b->entry->dirName);
    int bsnlen = (b->subDir ? strlen(b->subDir) : 0);
    int bbnlen = strlen(b->baseName);
    char * afn, * bfn, * t;
    int rc = 0;

    if (adnlen == 1 && asnlen != 0) adnlen = 0;
    if (bdnlen == 1 && bsnlen != 0) bdnlen = 0;

/*@-boundswrite@*/
    afn = t = alloca(adnlen+asnlen+abnlen+2);
    if (adnlen) t = stpcpy(t, a->entry->dirName);
    *t++ = '/';
    if (a->subDir && asnlen) t = stpcpy(t, a->subDir);
    if (abnlen) t = stpcpy(t, a->baseName);
    if (afn[0] == '/' && afn[1] == '/') afn++;

    bfn = t = alloca(bdnlen+bsnlen+bbnlen+2);
    if (bdnlen) t = stpcpy(t, b->entry->dirName);
    *t++ = '/';
    if (b->subDir && bsnlen) t = stpcpy(t, b->subDir);
    if (bbnlen) t = stpcpy(t, b->baseName);
    if (bfn[0] == '/' && bfn[1] == '/') bfn++;
/*@=boundswrite@*/

    rc = strcmp(afn, bfn);

    return rc;
}

/*@unchecked@*/
static int _linear_fps_search = 0;

static int findFps(const struct fingerPrint_s * fiFps,
		const struct fingerPrint_s * otherFps,
		int otherFc)
	/*@*/
{
    int otherFileNum;

  if (_linear_fps_search) {

linear:
    for (otherFileNum = 0; otherFileNum < otherFc; otherFileNum++, otherFps++) {

	/* If the addresses are the same, so are the values. */
	if (fiFps == otherFps)
	    break;

	/* Otherwise, compare fingerprints by value. */
	/*@-nullpass@*/	/* LCL: looks good to me */
	if (FP_EQUAL((*fiFps), (*otherFps)))
	    break;
	/*@=nullpass@*/
    }

    return otherFileNum;

  } else {

    const struct fingerPrint_s * bingoFps;

/*@-boundswrite@*/
    bingoFps = bsearch(fiFps, otherFps, otherFc, sizeof(*otherFps), fpsCompare);
/*@=boundswrite@*/
    if (bingoFps == NULL)
	goto linear;

    /* If the addresses are the same, so are the values. */
    if (!(fiFps == bingoFps || FP_EQUAL((*fiFps), (*bingoFps))))
	goto linear;

    otherFileNum = (bingoFps != NULL ? (bingoFps - otherFps) : 0);

  }

    return otherFileNum;
}

/**
 * Update disk space needs on each partition for this package's files.
 */
/* XXX only ts->{probs,di} modified */
static void handleOverlappedFiles(const rpmts ts,
		const rpmte p, rpmfi fi)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies ts, fi, fileSystem, internalState @*/
{
    uint_32 fixupSize = 0;
    rpmps ps;
    const char * fn;
    int i, j;

    ps = rpmtsProblems(ts);
    fi = rpmfiInit(fi, 0);
    if (fi != NULL)
    while ((i = rpmfiNext(fi)) >= 0) {
	uint_32 tscolor = rpmtsColor(ts);
	uint_32 prefcolor = rpmtsPrefColor(ts);
	uint_32 oFColor, FColor;
	struct fingerPrint_s * fiFps;
	int otherPkgNum, otherFileNum;
	rpmfi otherFi;
	int_32 FFlags;
	int_16 FMode;
	const rpmfi * recs;
	int numRecs;

	if (XFA_SKIPPING(fi->actions[i]))
	    continue;

	fn = rpmfiFN(fi);
	fiFps = fi->fps + i;
	FFlags = rpmfiFFlags(fi);
	FMode = rpmfiFMode(fi);
	FColor = rpmfiFColor(fi);
	FColor &= tscolor;

	fixupSize = 0;

	/*
	 * Retrieve all records that apply to this file. Note that the
	 * file info records were built in the same order as the packages
	 * will be installed and removed so the records for an overlapped
	 * files will be sorted in exactly the same order.
	 */
	(void) htGetEntry(ts->ht, fiFps,
			(const void ***) &recs, &numRecs, NULL);

	/*
	 * If this package is being added, look only at other packages
	 * being added -- removed packages dance to a different tune.
	 *
	 * If both this and the other package are being added, overlapped
	 * files must be identical (or marked as a conflict). The
	 * disposition of already installed config files leads to
	 * a small amount of extra complexity.
	 *
	 * If this package is being removed, then there are two cases that
	 * need to be worried about:
	 * If the other package is being added, then skip any overlapped files
	 * so that this package removal doesn't nuke the overlapped files
	 * that were just installed.
	 * If both this and the other package are being removed, then each
	 * file removal from preceding packages needs to be skipped so that
	 * the file removal occurs only on the last occurence of an overlapped
	 * file in the transaction set.
	 *
	 */

	/* Locate this overlapped file in the set of added/removed packages. */
	for (j = 0; j < numRecs && recs[j] != fi; j++)
	    {};

	/* Find what the previous disposition of this file was. */
	otherFileNum = -1;			/* keep gcc quiet */
	otherFi = NULL;
	for (otherPkgNum = j - 1; otherPkgNum >= 0; otherPkgNum--) {
	    struct fingerPrint_s * otherFps;
	    int otherFc;

	    otherFi = recs[otherPkgNum];

	    /* Added packages need only look at other added packages. */
	    if (rpmteType(p) == TR_ADDED && rpmteType(otherFi->te) != TR_ADDED)
		/*@innercontinue@*/ continue;

	    otherFps = otherFi->fps;
	    otherFc = rpmfiFC(otherFi);

	    otherFileNum = findFps(fiFps, otherFps, otherFc);
	    (void) rpmfiSetFX(otherFi, otherFileNum);

	    /* XXX Happens iff fingerprint for incomplete package install. */
	    if (otherFi->actions[otherFileNum] != FA_UNKNOWN)
		/*@innerbreak@*/ break;
	}

	oFColor = rpmfiFColor(otherFi);
	oFColor &= tscolor;

/*@-boundswrite@*/
	switch (rpmteType(p)) {
	case TR_ADDED:
	  { int reportConflicts =
		!(rpmtsFilterFlags(ts) & RPMPROB_FILTER_REPLACENEWFILES);
	    int done = 0;

	    if (otherPkgNum < 0) {
		/* XXX is this test still necessary? */
		if (fi->actions[i] != FA_UNKNOWN)
		    /*@switchbreak@*/ break;
		if ((FFlags & RPMFILE_CONFIG) && (FFlags & RPMFILE_EXISTS)) {
		    /* Here is a non-overlapped pre-existing config file. */
		    fi->actions[i] = (FFlags & RPMFILE_NOREPLACE)
			? FA_ALTNAME : FA_BACKUP;
		} else {
		    fi->actions[i] = FA_CREATE;
		}
		/*@switchbreak@*/ break;
	    }

assert(otherFi != NULL);
	    /* Mark added overlapped non-identical files as a conflict. */
	    if (rpmfiCompare(otherFi, fi)) {
		int rConflicts;

		rConflicts = reportConflicts;
		/* Resolve file conflicts to prefer Elf64 (if not forced) ... */
		if (tscolor != 0) {
		    if (FColor & prefcolor) {
			/* ... last file of preferred colour is installed ... */
			if (!XFA_SKIPPING(fi->actions[i])) {
			    /* XXX static helpers are order dependent. Ick. */
			    if (strcmp(fn, "/usr/sbin/libgcc_post_upgrade")
			     && strcmp(fn, "/usr/sbin/glibc_post_upgrade"))
				otherFi->actions[otherFileNum] = FA_SKIPCOLOR;
			}
			fi->actions[i] = FA_CREATE;
			rConflicts = 0;
		    } else
		    if (oFColor & prefcolor) {
			/* ... first file of preferred colour is installed ... */
			if (XFA_SKIPPING(fi->actions[i]))
			    otherFi->actions[otherFileNum] = FA_CREATE;
			fi->actions[i] = FA_SKIPCOLOR;
			rConflicts = 0;
		    } else
		    if (FColor == 0 && oFColor == 0) {
			/* ... otherwise, do both, last in wins. */
			otherFi->actions[otherFileNum] = FA_CREATE;
			fi->actions[i] = FA_CREATE;
			rConflicts = 0;
		    }
		    done = 1;
		}

		if (rConflicts) {
		    rpmpsAppend(ps, RPMPROB_NEW_FILE_CONFLICT,
			rpmteNEVR(p), rpmteKey(p),
			fn, NULL,
			rpmteNEVR(otherFi->te),
			0);
		}
	    }

	    /* Try to get the disk accounting correct even if a conflict. */
	    fixupSize = rpmfiFSize(otherFi);

	    if ((FFlags & RPMFILE_CONFIG) && (FFlags & RPMFILE_EXISTS)) {
		/* Here is an overlapped  pre-existing config file. */
		fi->actions[i] = (FFlags & RPMFILE_NOREPLACE)
			? FA_ALTNAME : FA_SKIP;
	    } else {
		if (!done)
		    fi->actions[i] = FA_CREATE;
	    }
	  } /*@switchbreak@*/ break;

	case TR_REMOVED:
	    if (otherPkgNum >= 0) {
assert(otherFi != NULL);
		/* Here is an overlapped added file we don't want to nuke. */
		if (otherFi->actions[otherFileNum] != FA_ERASE) {
		    /* On updates, don't remove files. */
		    fi->actions[i] = FA_SKIP;
		    /*@switchbreak@*/ break;
		}
		/* Here is an overlapped removed file: skip in previous. */
		otherFi->actions[otherFileNum] = FA_SKIP;
	    }
	    if (XFA_SKIPPING(fi->actions[i]))
		/*@switchbreak@*/ break;
	    if (rpmfiFState(fi) != RPMFILE_STATE_NORMAL)
		/*@switchbreak@*/ break;

	    /* Disposition is assumed to be FA_ERASE. */
	    fi->actions[i] = FA_ERASE;
	    if (!(S_ISREG(FMode) && (FFlags & RPMFILE_CONFIG)))
		/*@switchbreak@*/ break;
		
	    /* Check for pre-existing modified config file that needs saving. */
	    if (!(FFlags & RPMFILE_SPARSE))
	    {	int dalgo = 0;
		size_t dlen = 0;
		const unsigned char * digest = rpmfiDigest(fi, &dalgo, &dlen);
		unsigned char * fdigest;
assert(digest != NULL);
		
		fdigest = xcalloc(1, dlen);
		/* Save (by renaming) locally modified config files. */
		if (!dodigest(dalgo, fn, fdigest, 0, NULL)
		 && memcmp(digest, fdigest, dlen))
		    fi->actions[i] = FA_BACKUP;
		fdigest = _free(fdigest);
	    }
	    /*@switchbreak@*/ break;
	}
/*@=boundswrite@*/

	/* Update disk space info for a file. */
	rpmtsUpdateDSI(ts, fiFps->entry->dev, rpmfiFSize(fi),
		fi->replacedSizes[i], fixupSize, fi->actions[i]);

    }
    ps = rpmpsFree(ps);
}

/**
 * Ensure that current package is newer than installed package.
 * @param ts		transaction set
 * @param p		current transaction element
 * @param h		installed header
 * @return		0 if not newer, 1 if okay
 */
/*@-nullpass@*/
static int ensureOlder(rpmts ts,
		const rpmte p, const Header h)
	/*@modifies ts @*/
{
    int_32 reqFlags = (RPMSENSE_LESS | RPMSENSE_EQUAL);
    const char * reqEVR;
    rpmds req;
    char * t;
    int nb;
    int rc;

    if (p == NULL || h == NULL)
	return 1;

/*@-boundswrite@*/
    nb = strlen(rpmteNEVR(p)) + (rpmteE(p) != NULL ? strlen(rpmteE(p)) : 0) + 1;
    t = alloca(nb);
    *t = '\0';
    reqEVR = t;
    if (rpmteE(p) != NULL)	t = stpcpy( stpcpy(t, rpmteE(p)), ":");
    if (rpmteV(p) != NULL)	t = stpcpy(t, rpmteV(p));
    *t++ = '-';
    if (rpmteR(p) != NULL)	t = stpcpy(t, rpmteR(p));
/*@=boundswrite@*/

    req = rpmdsSingle(RPMTAG_REQUIRENAME, rpmteN(p), reqEVR, reqFlags);
    rc = rpmdsNVRMatchesDep(h, req, _rpmds_nopromote);
    req = rpmdsFree(req);

    if (rc == 0) {
	rpmps ps = rpmtsProblems(ts);
	const char * altNEVR = hGetNEVR(h, NULL);
	rpmpsAppend(ps, RPMPROB_OLDPACKAGE,
		rpmteNEVR(p), rpmteKey(p),
		NULL, NULL,
		altNEVR,
		0);
	altNEVR = _free(altNEVR);
	ps = rpmpsFree(ps);
	rc = 1;
    } else
	rc = 0;

    return rc;
}
/*@=nullpass@*/

/**
 * Skip any files that do not match install policies.
 * @param ts		transaction set
 * @param fi		file info set
 */
/*@-mustmod@*/ /* FIX: fi->actions is modified. */
/*@-bounds@*/
/*@-nullpass@*/
static void skipFiles(const rpmts ts, rpmfi fi)
	/*@globals rpmGlobalMacroContext, h_errno @*/
	/*@modifies fi, rpmGlobalMacroContext @*/
{
    uint_32 tscolor = rpmtsColor(ts);
    uint_32 FColor;
    int noConfigs = (rpmtsFlags(ts) & RPMTRANS_FLAG_NOCONFIGS);
    int noDocs = (rpmtsFlags(ts) & RPMTRANS_FLAG_NODOCS);
    char ** netsharedPaths = NULL;
    const char ** languages;
    const char * dn, * bn;
    int dnlen, bnlen, ix;
    const char * s;
    int * drc;
    char * dff;
    int dc;
    int i, j;

    if (!noDocs)
	noDocs = rpmExpandNumeric("%{_excludedocs}");

    {	const char *tmpPath = rpmExpand("%{_netsharedpath}", NULL);
	/*@-branchstate@*/
	if (tmpPath && *tmpPath != '%')
	    netsharedPaths = splitString(tmpPath, strlen(tmpPath), ':');
	/*@=branchstate@*/
	tmpPath = _free(tmpPath);
    }

    s = rpmExpand("%{_install_langs}", NULL);
    /*@-branchstate@*/
    if (!(s && *s != '%'))
	s = _free(s);
    if (s) {
	languages = (const char **) splitString(s, strlen(s), ':');
	s = _free(s);
    } else
	languages = NULL;
    /*@=branchstate@*/

    /* Compute directory refcount, skip directory if now empty. */
    dc = rpmfiDC(fi);
    drc = alloca(dc * sizeof(*drc));
    memset(drc, 0, dc * sizeof(*drc));
    dff = alloca(dc * sizeof(*dff));
    memset(dff, 0, dc * sizeof(*dff));

    fi = rpmfiInit(fi, 0);
    if (fi != NULL)	/* XXX lclint */
    while ((i = rpmfiNext(fi)) >= 0)
    {
	char ** nsp;

	bn = rpmfiBN(fi);
	bnlen = strlen(bn);
	ix = rpmfiDX(fi);
	dn = rpmfiDN(fi);
	if (dn == NULL)
	    continue;	/* XXX can't happen */
	dnlen = strlen(dn);

	drc[ix]++;

	/* Don't bother with skipped files */
	if (XFA_SKIPPING(fi->actions[i])) {
	    drc[ix]--; dff[ix] = 1;
	    continue;
	}

	/* Ignore colored files not in our rainbow. */
	FColor = rpmfiFColor(fi);
	if (tscolor && FColor && !(tscolor & FColor)) {
	    drc[ix]--;	dff[ix] = 1;
	    fi->actions[i] = FA_SKIPCOLOR;
	    continue;
	}

	/*
	 * Skip net shared paths.
	 * Net shared paths are not relative to the current root (though
	 * they do need to take package relocations into account).
	 */
	for (nsp = netsharedPaths; nsp && *nsp; nsp++) {
	    int len;

	    len = strlen(*nsp);
	    if (dnlen >= len) {
		if (strncmp(dn, *nsp, len))
		    /*@innercontinue@*/ continue;
		/* Only directories or complete file paths can be net shared */
		if (!(dn[len] == '/' || dn[len] == '\0'))
		    /*@innercontinue@*/ continue;
	    } else {
		if (len < (dnlen + bnlen))
		    /*@innercontinue@*/ continue;
		if (strncmp(dn, *nsp, dnlen))
		    /*@innercontinue@*/ continue;
		/* Insure that only the netsharedpath basename is compared. */
		if ((s = strchr((*nsp) + dnlen, '/')) != NULL && s[1] != '\0')
		    /*@innercontinue@*/ continue;
		if (strncmp(bn, (*nsp) + dnlen, bnlen))
		    /*@innercontinue@*/ continue;
		len = dnlen + bnlen;
		/* Only directories or complete file paths can be net shared */
		if (!((*nsp)[len] == '/' || (*nsp)[len] == '\0'))
		    /*@innercontinue@*/ continue;
	    }

	    /*@innerbreak@*/ break;
	}

	if (nsp && *nsp) {
	    drc[ix]--;	dff[ix] = 1;
	    fi->actions[i] = FA_SKIPNETSHARED;
	    continue;
	}

	/*
	 * Skip i18n language specific files.
	 */
	if (languages != NULL && fi->flangs != NULL && *fi->flangs[i]) {
	    const char **lang, *l, *le;
	    for (lang = languages; *lang != NULL; lang++) {
		if (!strcmp(*lang, "all"))
		    /*@innerbreak@*/ break;
		for (l = fi->flangs[i]; *l != '\0'; l = le) {
		    for (le = l; *le != '\0' && *le != '|'; le++)
			{};
		    if ((le-l) > 0 && !strncmp(*lang, l, (le-l)))
			/*@innerbreak@*/ break;
		    if (*le == '|') le++;	/* skip over | */
		}
		if (*l != '\0')
		    /*@innerbreak@*/ break;
	    }
	    if (*lang == NULL) {
		drc[ix]--;	dff[ix] = 1;
		fi->actions[i] = FA_SKIPNSTATE;
		continue;
	    }
	}

	/*
	 * Skip config files if requested.
	 */
	if (noConfigs && (rpmfiFFlags(fi) & RPMFILE_CONFIG)) {
	    drc[ix]--;	dff[ix] = 1;
	    fi->actions[i] = FA_SKIPNSTATE;
	    continue;
	}

	/*
	 * Skip documentation if requested.
	 */
	if (noDocs && (rpmfiFFlags(fi) & RPMFILE_DOC)) {
	    drc[ix]--;	dff[ix] = 1;
	    fi->actions[i] = FA_SKIPNSTATE;
	    continue;
	}
    }

    /* Skip (now empty) directories that had skipped files. */
#ifndef	NOTYET
    if (fi != NULL)	/* XXX can't happen */
    for (j = 0; j < dc; j++)
#else
    if ((fi = rpmfiInitD(fi)) != NULL)
    while (j = rpmfiNextD(fi) >= 0)
#endif
    {

	if (drc[j]) continue;	/* dir still has files. */
	if (!dff[j]) continue;	/* dir was not emptied here. */
	
	/* Find parent directory and basename. */
	dn = fi->dnl[j];	dnlen = strlen(dn) - 1;
	bn = dn + dnlen;	bnlen = 0;
	while (bn > dn && bn[-1] != '/') {
		bnlen++;
		dnlen--;
		bn--;
	}

	/* If explicitly included in the package, skip the directory. */
	fi = rpmfiInit(fi, 0);
	if (fi != NULL)		/* XXX lclint */
	while ((i = rpmfiNext(fi)) >= 0) {
	    const char * fdn, * fbn;
	    int_16 fFMode;

	    if (XFA_SKIPPING(fi->actions[i]))
		/*@innercontinue@*/ continue;

	    fFMode = rpmfiFMode(fi);

	    if (whatis(fFMode) != XDIR)
		/*@innercontinue@*/ continue;
	    fdn = rpmfiDN(fi);
	    if (strlen(fdn) != dnlen)
		/*@innercontinue@*/ continue;
	    if (strncmp(fdn, dn, dnlen))
		/*@innercontinue@*/ continue;
	    fbn = rpmfiBN(fi);
	    if (strlen(fbn) != bnlen)
		/*@innercontinue@*/ continue;
	    if (strncmp(fbn, bn, bnlen))
		/*@innercontinue@*/ continue;
	    rpmMessage(RPMMESS_DEBUG, _("excluding directory %s\n"), dn);
	    fi->actions[i] = FA_SKIPNSTATE;
	    /*@innerbreak@*/ break;
	}
    }

/*@-dependenttrans@*/
    if (netsharedPaths) freeSplitString(netsharedPaths);
#ifdef	DYING	/* XXX freeFi will deal with this later. */
    fi->flangs = _free(fi->flangs);
#endif
    if (languages) freeSplitString((char **)languages);
/*@=dependenttrans@*/
}
/*@=nullpass@*/
/*@=bounds@*/
/*@=mustmod@*/

/**
 * Return transaction element's file info.
 * @todo Take a rpmfi refcount here.
 * @param tsi		transaction element iterator
 * @return		transaction element file info
 */
static /*@null@*/
rpmfi rpmtsiFi(const rpmtsi tsi)
	/*@*/
{
    rpmfi fi = NULL;

    if (tsi != NULL && tsi->ocsave != -1) {
	/*@-type -abstract@*/ /* FIX: rpmte not opaque */
	rpmte te = rpmtsElement(tsi->ts, tsi->ocsave);
	/*@-assignexpose@*/
	if (te != NULL && (fi = te->fi) != NULL)
	    fi->te = te;
	/*@=assignexpose@*/
	/*@=type =abstract@*/
    }
    /*@-compdef -refcounttrans -usereleased @*/
    return fi;
    /*@=compdef =refcounttrans =usereleased @*/
}

/**
 * This is not a generalized function to be called from outside
 * librpm.  It is called internally by rpmtsRun() to add elements
 * to its rollback transaction.
 * @param ts		current transaction set
 * @param p 		failed rpmte. 
 * @return 		RPMRC_OK, or RPMRC_FAIL
 */
/*@-nullpass@*/
static rpmRC _processFailedPackage(rpmts ts, rpmte p)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, p, rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
{
    int rc  = RPMRC_OK;	/* Assume Success */

    /* Handle failed packages. */
    /* XXX TODO: Add header to rpmdb in PSM_INIT, not PSM_POST. */
    if (p != NULL && rpmteType(p) == TR_ADDED && !p->installed) {
/*@-compdef -usereleased@*/	/* p->fi->te undefined */
	rpmpsm psm = rpmpsmNew(ts, p, p->fi);
/*@=compdef =usereleased@*/
	/*
	 * If it died before the header was put in the rpmdb, we need
	 * do to something wacky which is add the header to the DB anyway.
	 * This will allow us to add the failed package as an erase
	 * to the rollback transaction.  This must be done because we
	 * want the the erase scriptlets to run, and the only way that
	 * is going is if the header is in the rpmdb.
	 */
assert(psm != NULL);
	psm->stepName = "failed";	/* XXX W2DO? */
	rc = rpmpsmStage(psm, PSM_RPMDB_ADD);
	psm = rpmpsmFree(psm);
    }
    return rc;
}
/*@=nullpass@*/

/*@-nullpass@*/
rpmRC rpmtsRollback(rpmts rbts, rpmprobFilterFlags ignoreSet, int running, rpmte rbte)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies rbts, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    const char * semfn = NULL;
    rpmRC rc = 0;
    uint_32 arbgoal = rpmtsARBGoal(rbts);
    QVA_t ia = memset(alloca(sizeof(*ia)), 0, sizeof(*ia));
    time_t ttid;
    int xx;

    /* Don't attempt rollback's of rollback transactions */
    if ((rpmtsType(rbts) & RPMTRANS_TYPE_ROLLBACK) ||
	(rpmtsType(rbts) & RPMTRANS_TYPE_AUTOROLLBACK))
	return RPMRC_OK;

    if (arbgoal == 0xffffffff) 
	arbgoal = rpmtsGetTid(rbts) - 1;

    /* Don't attempt rollbacks if no goal is set. */
    if (!running && arbgoal == 0xffffffff)
	return RPMRC_OK;

    /* We need to remove an headers that were going to be removed so 
     * as to not foul up the regular rollback mechanism which will not 
     * handle properly a file being in the repackaged package directory
     * and also its header still in the DB.
     */
    {	rpmtsi tsi;
	rpmte te;

	/* XXX Insure an O_RDWR rpmdb. */
	rpmtsOpenDB(rbts, O_RDWR);

	tsi = rpmtsiInit(rbts);
	while((te = rpmtsiNext(tsi, TR_REMOVED)) != NULL) {
	    if(!te->u.removed.dboffset)
		continue;
	    rc = rpmdbRemove(rpmtsGetRdb(rbts),
			rpmtsGetTid(rbts),
			te->u.removed.dboffset, NULL, NULL);
	    if (rc != RPMRC_OK) {
		rpmMessage(RPMMESS_ERROR, _("rpmdb erase failed. NEVRA: %s\n"),
			rpmteNEVRA(te));
		break;
	    }
	}
	tsi = rpmtsiFree(tsi);
	if (rc != RPMRC_OK) 
	    goto cleanup;
    }

    /* Process the failed package */
    rc = _processFailedPackage(rbts, rbte);
    if (rc != RPMRC_OK)
	goto cleanup;

    rpmtsEmpty(rbts);

    ttid = (time_t)arbgoal;
    rpmMessage(RPMMESS_NORMAL, _("Rollback to %-24.24s (0x%08x)\n"),
	ctime(&ttid), arbgoal);

    /* Set the verify signature flags:
     *  - can't verify signatures/digests on repackaged packages.
     *  - header check are out.
     */
    {
	rpmVSFlags vsflags = rpmExpandNumeric("%{?_vsflags_erase}");
	vsflags |= _RPMVSF_NODIGESTS;
	vsflags |= _RPMVSF_NOSIGNATURES;
	vsflags |= RPMVSF_NOHDRCHK;
	vsflags |= RPMVSF_NEEDPAYLOAD;      
	xx = rpmtsSetVSFlags(rbts, vsflags); 
    }

    /* Set transaction flags to be the same as the running transaction */
    {
	rpmtransFlags tsFlags = rpmtsFlags(rbts);
	tsFlags &= ~RPMTRANS_FLAG_DIRSTASH;	/* No repackage of rollbacks */
	tsFlags &= ~RPMTRANS_FLAG_REPACKAGE;	/* No repackage of rollbacks */
	tsFlags |= RPMTRANS_FLAG_NOFDIGESTS;	/* Don't check file digests */
	tsFlags = rpmtsSetFlags(rbts, tsFlags);
    }

    /* Create install arguments structure */ 	
    ia->rbtid = arbgoal;
    /* transFlags/depFlags from rbts, (re-)set in rpmRollback(). */
    ia->transFlags = rpmtsFlags(rbts);
    ia->depFlags = rpmtsDFlags(rbts);
    /* XXX probFilter is normally set in main(). */
    ia->probFilter = ignoreSet;	/* XXX RPMPROB_FILTER_NONE? */
    /* XXX installInterfaceFlags is normally set in main(). */
    ia->installInterfaceFlags = INSTALL_UPGRADE | INSTALL_HASH ;

    /* rpmtsCheck and rpmtsOrder failures do not have links. */
    ia->no_rollback_links = 1;

    /* Create a file semaphore. */
    semfn = rpmExpand("%{?semaphore_backout}", NULL);
    if (semfn && *semfn) {
	FD_t fd = Fopen(semfn, "w.fdio");
	if (fd)
	    xx = Fclose(fd);
    }

/*@-compmempass@*/
    rc = rpmRollback(rbts, ia, NULL);
/*@=compmempass@*/

cleanup: 
    /* Remove the file semaphore. */
    if (semfn && *semfn)
	xx = Unlink(semfn);
    semfn = _free(semfn);

    return rc;
}
/*@=nullpass@*/

/**
 * Search for string B in argv array AV.
 * @param AV		argv array
 * @param B		string
 * @return		1 if found, 0 otherwise
 */
static int cmpArgvStr(/*@null@*/ const char ** AV, /*@null@*/ const char * B)
	/*@*/
{
    const char ** a;

    if (AV != NULL && B != NULL)
    for (a = AV; *a != NULL; a++) {
	if (**a && *B && !strcmp(*a, B))
	    return 1;
    }
    return 0;
}


/**
 * Mark all erasure elements linked to installed element p as failed.
 * @param ts		transaction set
 * @param p		failed install transaction element
 * @return		0 always
 */
static int markLinkedFailed(rpmts ts, rpmte p)
	/*@globals fileSystem @*/
	/*@modifies ts, p, fileSystem @*/
{
    rpmtsi qi; rpmte q;
    int bingo;

    p->linkFailed = 1;

    qi = rpmtsiInit(ts);
    while ((q = rpmtsiNext(qi, TR_REMOVED)) != NULL) {

	if (q->done)
	    continue;

	/*
	 * Either element may have missing data and can have multiple entries.
	 * Try for hdrid, then pkgid, finally NEVRA, argv vs. argv compares.
	 */
	bingo = cmpArgvStr(q->flink.Hdrid, p->hdrid);
	if (!bingo)
		bingo = cmpArgvStr(q->flink.Pkgid, p->pkgid);
	if (!bingo)
		bingo = cmpArgvStr(q->flink.NEVRA, p->NEVRA);

	if (!bingo)
	    continue;

	q->linkFailed = p->linkFailed;
    }
    qi = rpmtsiFree(qi);

    return 0;
}

#define	NOTIFY(_ts, _al) /*@i@*/ if ((_ts)->notify) (void) (_ts)->notify _al

int rpmtsRun(rpmts ts, rpmps okProbs, rpmprobFilterFlags ignoreSet)
{
    uint_32 tscolor = rpmtsColor(ts);
    int i, j;
    int ourrc = 0;
    int totalFileCount = 0;
    rpmfi fi;
    sharedFileInfo shared, sharedList;
    int numShared;
    int nexti;
    fingerPrintCache fpc;
    rpmps ps;
    rpmpsm psm;
    rpmtsi pi;	rpmte p;
    rpmtsi qi;	rpmte q;
    int numAdded;
    int numRemoved;
    int rollbackFailures = 0;
    void * lock = NULL;
    int xx;

    /* XXX programmer error segfault avoidance. */
    if (rpmtsNElements(ts) <= 0) {
	rpmMessage(RPMMESS_ERROR,
	    _("Invalid number of transaction elements.\n"));
	return -1;
    }

    rollbackFailures = rpmExpandNumeric("%{?_rollback_transaction_on_failure}");
    /* Don't rollback unless repackaging. */
    if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_REPACKAGE))
	rollbackFailures = 0;
    /* Don't rollback if testing. */
    if (rpmtsFlags(ts) & RPMTRANS_FLAG_TEST)
	rollbackFailures = 0;

    if (rpmtsType(ts) & (RPMTRANS_TYPE_ROLLBACK | RPMTRANS_TYPE_AUTOROLLBACK))
	rollbackFailures = 0;

    /* If we are in test mode, there is no need to rollback on
     * failure, nor acquire the transaction lock.
     */
/*@-branchstate@*/
    /* Don't acquire the transaction lock if testing. */
    if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_TEST))
	lock = rpmtsAcquireLock(ts);
/*@=branchstate@*/

    /* --noscripts implies no scripts or triggers, duh. */
    if (rpmtsFlags(ts) & RPMTRANS_FLAG_NOSCRIPTS)
	(void) rpmtsSetFlags(ts, (rpmtsFlags(ts) | _noTransScripts | _noTransTriggers));
    /* --notriggers implies no triggers, duh. */
    if (rpmtsFlags(ts) & RPMTRANS_FLAG_NOTRIGGERS)
	(void) rpmtsSetFlags(ts, (rpmtsFlags(ts) | _noTransTriggers));

    /* --justdb implies no scripts or triggers, duh. */
    if (rpmtsFlags(ts) & RPMTRANS_FLAG_JUSTDB)
	(void) rpmtsSetFlags(ts, (rpmtsFlags(ts) | _noTransScripts | _noTransTriggers));

    ts->probs = rpmpsFree(ts->probs);
    ts->probs = rpmpsCreate();

    /* XXX Make sure the database is open RDWR for package install/erase. */
    {	int dbmode = (rpmtsFlags(ts) & RPMTRANS_FLAG_TEST)
		? O_RDONLY : (O_RDWR|O_CREAT);

	/* Open database RDWR for installing packages. */
	if (rpmtsOpenDB(ts, dbmode)) {
	    lock = rpmtsFreeLock(lock);
	    return -1;	/* XXX W2DO? */
	}
    }

    ts->ignoreSet = ignoreSet;
    {	const char * currDir = currentDirectory();
	rpmtsSetCurrDir(ts, currDir);
	currDir = _free(currDir);
    }

    (void) rpmtsSetChrootDone(ts, 0);

    /* XXX rpmtsCreate() sets the transaction id, but apps may not honor. */
    {	int_32 tid = (int_32) time(NULL);
	(void) rpmtsSetTid(ts, tid);
    }

    /* Get available space on mounted file systems. */
    xx = rpmtsInitDSI(ts);

    /* ===============================================
     * For packages being installed:
     * - verify package epoch:version-release is newer.
     * - count files.
     * For packages being removed:
     * - count files.
     */

rpmMessage(RPMMESS_DEBUG, _("sanity checking %d elements\n"), rpmtsNElements(ts));
    ps = rpmtsProblems(ts);
    /* The ordering doesn't matter here */
    pi = rpmtsiInit(ts);
    /* XXX Only added packages need be checked. */
    while ((p = rpmtsiNext(pi, TR_ADDED)) != NULL) {
	rpmdbMatchIterator mi;
	int fc;

	if ((fi = rpmtsiFi(pi)) == NULL)
	    continue;	/* XXX can't happen */
	fc = rpmfiFC(fi);

	if (!(rpmtsFilterFlags(ts) & RPMPROB_FILTER_OLDPACKAGE)) {
	    Header h;
	    mi = rpmtsInitIterator(ts, RPMTAG_NAME, rpmteN(p), 0);
	    while ((h = rpmdbNextIterator(mi)) != NULL)
		xx = ensureOlder(ts, p, h);
	    mi = rpmdbFreeIterator(mi);
	}

	if (!(rpmtsFilterFlags(ts) & RPMPROB_FILTER_REPLACEPKG)) {
	    mi = rpmtsInitIterator(ts, RPMTAG_NAME, rpmteN(p), 0);
	    xx = rpmdbSetIteratorRE(mi, RPMTAG_EPOCH, RPMMIRE_STRCMP,
				rpmteE(p));
	    xx = rpmdbSetIteratorRE(mi, RPMTAG_VERSION, RPMMIRE_STRCMP,
				rpmteV(p));
	    xx = rpmdbSetIteratorRE(mi, RPMTAG_RELEASE, RPMMIRE_STRCMP,
				rpmteR(p));
	    if (tscolor) {
		xx = rpmdbSetIteratorRE(mi, RPMTAG_ARCH, RPMMIRE_STRCMP,
				rpmteA(p));
		xx = rpmdbSetIteratorRE(mi, RPMTAG_OS, RPMMIRE_STRCMP,
				rpmteO(p));
	    }

	    while (rpmdbNextIterator(mi) != NULL) {
		rpmpsAppend(ps, RPMPROB_PKG_INSTALLED,
			rpmteNEVR(p), rpmteKey(p),
			NULL, NULL,
			NULL, 0);
		/*@innerbreak@*/ break;
	    }
	    mi = rpmdbFreeIterator(mi);
	}

	/* Count no. of files (if any). */
	totalFileCount += fc;

    }
    pi = rpmtsiFree(pi);
    ps = rpmpsFree(ps);

    /* The ordering doesn't matter here */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, TR_REMOVED)) != NULL) {
	int fc;

	if ((fi = rpmtsiFi(pi)) == NULL)
	    continue;	/* XXX can't happen */
	fc = rpmfiFC(fi);

	totalFileCount += fc;
    }
    pi = rpmtsiFree(pi);


    /* Run pre-transaction scripts, but only if there are no known
     * problems up to this point. */
    if (!((rpmtsFlags(ts) & RPMTRANS_FLAG_BUILD_PROBS)
     	  || (rpmpsNumProblems(ts->probs) &&
		(okProbs == NULL || rpmpsTrim(ts->probs, okProbs))))) {
	rpmMessage(RPMMESS_DEBUG, _("running pre-transaction scripts\n"));
	pi = rpmtsiInit(ts);
	while ((p = rpmtsiNext(pi, TR_ADDED)) != NULL) {
	    if ((fi = rpmtsiFi(pi)) == NULL)
		continue;	/* XXX can't happen */

	    /* If no pre-transaction script, then don't bother. */
	    if (fi->pretrans == NULL)
		continue;

	    p->fd = ts->notify(p->h, RPMCALLBACK_INST_OPEN_FILE, 0, 0,
			    rpmteKey(p), ts->notifyData);
	    p->h = NULL;
	    if (rpmteFd(p) != NULL) {
		rpmVSFlags ovsflags = rpmtsVSFlags(ts);
		rpmVSFlags vsflags = ovsflags | RPMVSF_NEEDPAYLOAD;
		rpmRC rpmrc;
		ovsflags = rpmtsSetVSFlags(ts, vsflags);
		rpmrc = rpmReadPackageFile(ts, rpmteFd(p),
			    rpmteNEVR(p), &p->h);
		vsflags = rpmtsSetVSFlags(ts, ovsflags);
		switch (rpmrc) {
		default:
		    /*@-noeffectuncon@*/ /* FIX: notify annotations */
		    p->fd = ts->notify(p->h, RPMCALLBACK_INST_CLOSE_FILE,
				    0, 0,
				    rpmteKey(p), ts->notifyData);
		    /*@=noeffectuncon@*/
		    p->fd = NULL;
		    /*@switchbreak@*/ break;
		case RPMRC_NOTTRUSTED:
		case RPMRC_NOKEY:
		case RPMRC_OK:
		    /*@switchbreak@*/ break;
		}
	    }

/*@-branchstate@*/
	    if (rpmteFd(p) != NULL) {
		fi = rpmfiNew(ts, p->h, RPMTAG_BASENAMES, 1);
		if (fi != NULL) {	/* XXX can't happen */
		    fi->te = p;
		    p->fi = fi;
		}
/*@-compdef -usereleased@*/	/* p->fi->te undefined */
		psm = rpmpsmNew(ts, p, p->fi);
/*@=compdef =usereleased@*/
assert(psm != NULL);
		psm->stepName = "pretrans";
		psm->scriptTag = RPMTAG_PRETRANS;
		psm->progTag = RPMTAG_PRETRANSPROG;
		xx = rpmpsmStage(psm, PSM_SCRIPT);
		psm = rpmpsmFree(psm);

/*@-noeffectuncon -compdef -usereleased @*/
		(void) ts->notify(p->h, RPMCALLBACK_INST_CLOSE_FILE, 0, 0,
				  rpmteKey(p), ts->notifyData);
/*@=noeffectuncon =compdef =usereleased @*/
		p->fd = NULL;
		p->h = headerFree(p->h);
	    }
/*@=branchstate@*/
	}
	pi = rpmtsiFree(pi);
    }

    /* ===============================================
     * Initialize transaction element file info for package:
     */

    /*
     * FIXME?: we'd be better off assembling one very large file list and
     * calling fpLookupList only once. I'm not sure that the speedup is
     * worth the trouble though.
     */
rpmMessage(RPMMESS_DEBUG, _("computing %d file fingerprints\n"), totalFileCount);

    numAdded = numRemoved = 0;
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	int fc;

	if ((fi = rpmtsiFi(pi)) == NULL)
	    continue;	/* XXX can't happen */
	fc = rpmfiFC(fi);

	/*@-branchstate@*/
	switch (rpmteType(p)) {
	case TR_ADDED:
	    numAdded++;
	    fi->record = 0;
	    /* Skip netshared paths, not our i18n files, and excluded docs */
	    if (fc > 0)
		skipFiles(ts, fi);
	    /*@switchbreak@*/ break;
	case TR_REMOVED:
	    numRemoved++;
	    fi->record = rpmteDBOffset(p);
	    /*@switchbreak@*/ break;
	}
	/*@=branchstate@*/

	fi->fps = (fc > 0 ? xmalloc(fc * sizeof(*fi->fps)) : NULL);
    }
    pi = rpmtsiFree(pi);

    if (!rpmtsChrootDone(ts)) {
	const char * rootDir = rpmtsRootDir(ts);
	static int openall_before_chroot = -1;

	if (openall_before_chroot < 0)
	    openall_before_chroot = rpmExpandNumeric("%{?_openall_before_chroot}");

	xx = Chdir("/");
	/*@-superuser -noeffect @*/
	if (rootDir != NULL && strcmp(rootDir, "/") && *rootDir == '/') {
	    if (openall_before_chroot)
		xx = rpmdbOpenAll(rpmtsGetRdb(ts));
	    xx = Chroot(rootDir);
	}
	/*@=superuser =noeffect @*/
	(void) rpmtsSetChrootDone(ts, 1);
    }

    ts->ht = htCreate(totalFileCount * 2, 0, 0, fpHashFunction, fpEqual);
    fpc = fpCacheCreate(totalFileCount);

    /* ===============================================
     * Add fingerprint for each file not skipped.
     */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	int fc;

	(void) rpmdbCheckSignals();

	if ((fi = rpmtsiFi(pi)) == NULL)
	    continue;	/* XXX can't happen */
	fc = rpmfiFC(fi);

	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), 0);
	fpLookupList(fpc, fi->dnl, fi->bnl, fi->dil, fc, fi->fps);
	/*@-branchstate@*/
 	fi = rpmfiInit(fi, 0);
 	if (fi != NULL)		/* XXX lclint */
	while ((i = rpmfiNext(fi)) >= 0) {
	    if (XFA_SKIPPING(fi->actions[i]))
		/*@innercontinue@*/ continue;
	    /*@-dependenttrans@*/
	    htAddEntry(ts->ht, fi->fps + i, (void *) fi);
	    /*@=dependenttrans@*/
	}
	/*@=branchstate@*/
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), fc);

    }
    pi = rpmtsiFree(pi);

    NOTIFY(ts, (NULL, RPMCALLBACK_TRANS_START, 6, ts->orderCount,
	NULL, ts->notifyData));

    /* ===============================================
     * Compute file disposition for each package in transaction set.
     */
rpmMessage(RPMMESS_DEBUG, _("computing file dispositions\n"));
    ps = rpmtsProblems(ts);
    pi = rpmtsiInit(ts);
/*@-nullpass@*/
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	dbiIndexSet * matches;
	unsigned int exclude;
	int knownBad;
	int fc;

	(void) rpmdbCheckSignals();

	if ((fi = rpmtsiFi(pi)) == NULL)
	    continue;	/* XXX can't happen */
	fc = rpmfiFC(fi);

	NOTIFY(ts, (NULL, RPMCALLBACK_TRANS_PROGRESS, rpmtsiOc(pi),
			ts->orderCount, NULL, ts->notifyData));

	if (fc == 0) continue;

	(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), 0);
	/* Extract file info for all files in this package from the database. */
	matches = xcalloc(fc, sizeof(*matches));
	exclude = (rpmteType(p) == TR_REMOVED ? fi->record : 0);
	if (rpmdbFindFpList(rpmtsGetRdb(ts), fi->fps, matches, fc, exclude)) {
	    ps = rpmpsFree(ps);
	    lock = rpmtsFreeLock(lock);
	    return 1;	/* XXX WTFO? */
	}

	numShared = 0;
 	fi = rpmfiInit(fi, 0);
	while ((i = rpmfiNext(fi)) >= 0)
	    numShared += dbiIndexSetCount(matches[i]);

	/* Build sorted file info list for this package. */
	shared = sharedList = xcalloc((numShared + 1), sizeof(*sharedList));

 	fi = rpmfiInit(fi, 0);
	while ((i = rpmfiNext(fi)) >= 0) {
	    /*
	     * Take care not to mark files as replaced in packages that will
	     * have been removed before we will get here.
	     */
	    for (j = 0; j < dbiIndexSetCount(matches[i]); j++) {
		int ro;
		ro = dbiIndexRecordOffset(matches[i], j);
		knownBad = 0;
		qi = rpmtsiInit(ts);
		while ((q = rpmtsiNext(qi, TR_REMOVED)) != NULL) {
		    if (ro == knownBad)
			/*@innerbreak@*/ break;
		    if (rpmteDBOffset(q) == ro)
			knownBad = ro;
		}
		qi = rpmtsiFree(qi);

		shared->pkgFileNum = i;
		shared->otherPkg = dbiIndexRecordOffset(matches[i], j);
		shared->otherFileNum = dbiIndexRecordFileNumber(matches[i], j);
		shared->isRemoved = (knownBad == ro);
		shared++;
	    }
	    matches[i] = dbiFreeIndexSet(matches[i]);
	}
	numShared = shared - sharedList;
	shared->otherPkg = -1;
	matches = _free(matches);

	/* Sort file info by other package index (otherPkg) */
	qsort(sharedList, numShared, sizeof(*shared), sharedCmp);

	/* For all files from this package that are in the database ... */
/*@-nullpass@*/
	/*@-branchstate@*/
	for (i = 0; i < numShared; i = nexti) {
	    int beingRemoved;

	    shared = sharedList + i;

	    /* Find the end of the files in the other package. */
	    for (nexti = i + 1; nexti < numShared; nexti++) {
		if (sharedList[nexti].otherPkg != shared->otherPkg)
		    /*@innerbreak@*/ break;
	    }

	    /* Is this file from a package being removed? */
	    beingRemoved = 0;
	    if (ts->removedPackages != NULL)
	    for (j = 0; j < ts->numRemovedPackages; j++) {
		if (ts->removedPackages[j] != shared->otherPkg)
		    /*@innercontinue@*/ continue;
		beingRemoved = 1;
		/*@innerbreak@*/ break;
	    }

	    /* Determine the fate of each file. */
	    switch (rpmteType(p)) {
	    case TR_ADDED:
		xx = handleInstInstalledFiles(ts, p, fi, shared, nexti - i,
	!(beingRemoved || (rpmtsFilterFlags(ts) & RPMPROB_FILTER_REPLACEOLDFILES)));
		/*@switchbreak@*/ break;
	    case TR_REMOVED:
		if (!beingRemoved)
		    xx = handleRmvdInstalledFiles(ts, fi, shared, nexti - i);
		/*@switchbreak@*/ break;
	    }
	}
	/*@=branchstate@*/
/*@=nullpass@*/

	free(sharedList);

	/* Update disk space needs on each partition for this package. */
/*@-nullpass@*/
	handleOverlappedFiles(ts, p, fi);
/*@=nullpass@*/

	/* Check added package has sufficient space on each partition used. */
	switch (rpmteType(p)) {
	case TR_ADDED:
	    rpmtsCheckDSIProblems(ts, p);
	    /*@switchbreak@*/ break;
	case TR_REMOVED:
	    /*@switchbreak@*/ break;
	}
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_FINGERPRINT), fc);
    }
/*@=nullpass@*/
    pi = rpmtsiFree(pi);
    ps = rpmpsFree(ps);

    if (rpmtsChrootDone(ts)) {
	const char * rootDir = rpmtsRootDir(ts);
	const char * currDir = rpmtsCurrDir(ts);
	/*@-superuser -noeffect @*/
	if (rootDir != NULL && strcmp(rootDir, "/") && *rootDir == '/')
	    xx = Chroot(".");
	/*@=superuser =noeffect @*/
	(void) rpmtsSetChrootDone(ts, 0);
	if (currDir != NULL)
	    xx = Chdir(currDir);
    }

    NOTIFY(ts, (NULL, RPMCALLBACK_TRANS_STOP, 6, ts->orderCount,
	NULL, ts->notifyData));

    /* ===============================================
     * Free unused memory as soon as possible.
     */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	if ((fi = rpmtsiFi(pi)) == NULL)
	    continue;	/* XXX can't happen */
	if (rpmfiFC(fi) == 0)
	    continue;
	fi->fps = _free(fi->fps);
    }
    pi = rpmtsiFree(pi);

    fpc = fpCacheFree(fpc);
    ts->ht = htFree(ts->ht);

    /* ===============================================
     * If unfiltered problems exist, free memory and return.
     */
    if ((rpmtsFlags(ts) & RPMTRANS_FLAG_BUILD_PROBS)
     || (rpmpsNumProblems(ts->probs) &&
		(okProbs == NULL || rpmpsTrim(ts->probs, okProbs)))
       )
    {
	lock = rpmtsFreeLock(lock);
	return ts->orderCount;
    }

    /* ===============================================
     * Save removed files before erasing.
     */
    if (rpmtsFlags(ts) & (RPMTRANS_FLAG_DIRSTASH | RPMTRANS_FLAG_REPACKAGE)) {
	int progress;

	progress = 0;
	pi = rpmtsiInit(ts);
	while ((p = rpmtsiNext(pi, 0)) != NULL) {

	    (void) rpmdbCheckSignals();

	    if ((fi = rpmtsiFi(pi)) == NULL)
		continue;	/* XXX can't happen */
	    switch (rpmteType(p)) {
	    case TR_ADDED:
		/*@switchbreak@*/ break;
	    case TR_REMOVED:
		if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_REPACKAGE))
		    /*@switchbreak@*/ break;
		if (!progress)
		    NOTIFY(ts, (NULL, RPMCALLBACK_REPACKAGE_START,
				7, numRemoved, NULL, ts->notifyData));

		NOTIFY(ts, (NULL, RPMCALLBACK_REPACKAGE_PROGRESS, progress,
			numRemoved, NULL, ts->notifyData));
		progress++;

		(void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_REPACKAGE), 0);

	/* XXX TR_REMOVED needs CPIO_MAP_{ABSOLUTE,ADDDOT} CPIO_ALL_HARDLINKS */
		fi->mapflags |= CPIO_MAP_ABSOLUTE;
		fi->mapflags |= CPIO_MAP_ADDDOT;
		fi->mapflags |= CPIO_ALL_HARDLINKS;
		psm = rpmpsmNew(ts, p, fi);
assert(psm != NULL);
		xx = rpmpsmStage(psm, PSM_PKGSAVE);
		psm = rpmpsmFree(psm);
		fi->mapflags &= ~CPIO_MAP_ABSOLUTE;
		fi->mapflags &= ~CPIO_MAP_ADDDOT;
		fi->mapflags &= ~CPIO_ALL_HARDLINKS;

		(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_REPACKAGE), 0);

		/*@switchbreak@*/ break;
	    }
	}
	pi = rpmtsiFree(pi);
	if (progress) {
	    NOTIFY(ts, (NULL, RPMCALLBACK_REPACKAGE_STOP, 7, numRemoved,
			NULL, ts->notifyData));
	}
    }

    /* ===============================================
     * Install and remove packages.
     */
/*@-nullpass@*/
    pi = rpmtsiInit(ts);
    /*@-branchstate@*/ /* FIX: fi reload needs work */
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	alKey pkgKey;
	int gotfd;

	(void) rpmdbCheckSignals();

	gotfd = 0;
	if ((fi = rpmtsiFi(pi)) == NULL)
	    continue;	/* XXX can't happen */
	
	psm = rpmpsmNew(ts, p, fi);
assert(psm != NULL);
	psm->unorderedSuccessor =
		(rpmtsiOc(pi) >= rpmtsUnorderedSuccessors(ts, -1) ? 1 : 0);

	switch (rpmteType(p)) {
	case TR_ADDED:
	    (void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_INSTALL), 0);

	    pkgKey = rpmteAddedKey(p);

	    rpmMessage(RPMMESS_DEBUG, "========== +++ %s %s-%s 0x%x\n",
		rpmteNEVR(p), rpmteA(p), rpmteO(p), rpmteColor(p));

	    p->h = NULL;
	    /*@-type@*/ /* FIX: rpmte not opaque */
	    {
		/*@-noeffectuncon@*/ /* FIX: notify annotations */
		p->fd = ts->notify(p->h, RPMCALLBACK_INST_OPEN_FILE, 0, 0,
				rpmteKey(p), ts->notifyData);
		/*@=noeffectuncon@*/
		if (rpmteFd(p) != NULL) {
		    rpmVSFlags ovsflags = rpmtsVSFlags(ts);
		    rpmVSFlags vsflags = ovsflags | RPMVSF_NEEDPAYLOAD;
		    rpmRC rpmrc;

		    ovsflags = rpmtsSetVSFlags(ts, vsflags);
		    rpmrc = rpmReadPackageFile(ts, rpmteFd(p),
				rpmteNEVR(p), &p->h);
		    vsflags = rpmtsSetVSFlags(ts, ovsflags);

		    switch (rpmrc) {
		    default:
			/*@-noeffectuncon@*/ /* FIX: notify annotations */
			p->fd = ts->notify(p->h, RPMCALLBACK_INST_CLOSE_FILE,
					0, 0,
					rpmteKey(p), ts->notifyData);
			/*@=noeffectuncon@*/
			p->fd = NULL;
			ourrc++;
			/*@innerbreak@*/ break;
		    case RPMRC_NOTTRUSTED:
		    case RPMRC_NOKEY:
		    case RPMRC_OK:
			/*@innerbreak@*/ break;
		    }
		    if (rpmteFd(p) != NULL) gotfd = 1;
		}
	    }
	    /*@=type@*/

	    if (rpmteFd(p) != NULL) {
		/*
		 * XXX Sludge necessary to tranfer existing fstates/actions
		 * XXX around a recreated file info set.
		 */
		psm->fi = rpmfiFree(psm->fi);
		{
		    char * fstates = fi->fstates;
		    fileAction * actions = fi->actions;
		    int mapflags = fi->mapflags;
		    rpmte savep;
		    int scareMem = 1;	/* XXX rpmpsmStage needs fi->h */

		    fi->fstates = NULL;
		    fi->actions = NULL;
/*@-nullstate@*/ /* FIX: fi->actions is NULL */
		    fi = rpmfiFree(fi);
/*@=nullstate@*/

		    savep = rpmtsSetRelocateElement(ts, p);
		    fi = rpmfiNew(ts, p->h, RPMTAG_BASENAMES, scareMem);
		    (void) rpmtsSetRelocateElement(ts, savep);

		    if (fi != NULL) {	/* XXX can't happen */
			fi->te = p;
			fi->fstates = _free(fi->fstates);
			fi->fstates = fstates;
			fi->actions = _free(fi->actions);
			fi->actions = actions;
			if (mapflags & CPIO_SBIT_CHECK)
			    fi->mapflags |= CPIO_SBIT_CHECK;
			p->fi = fi;
		    }
		}
		psm->fi = rpmfiLink(p->fi, NULL);

		if ((xx = rpmpsmStage(psm, PSM_PKGINSTALL)) != 0) {
		    ourrc++;
		    xx = markLinkedFailed(ts, p);
		} else
		    p->done = 1;

	    } else {
		ourrc++;
	    }

	    if (gotfd) {
		/*@-noeffectuncon @*/ /* FIX: check rc */
		(void) ts->notify(p->h, RPMCALLBACK_INST_CLOSE_FILE, 0, 0,
			rpmteKey(p), ts->notifyData);
		/*@=noeffectuncon @*/
		/*@-type@*/
		p->fd = NULL;
		/*@=type@*/
	    }

	    (void) rpmswExit(rpmtsOp(ts, RPMTS_OP_INSTALL), 0);

	    /*@switchbreak@*/ break;

	case TR_REMOVED:
	    (void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_ERASE), 0);

	    rpmMessage(RPMMESS_DEBUG, "========== --- %s %s-%s 0x%x\n",
		rpmteNEVR(p), rpmteA(p), rpmteO(p), rpmteColor(p));

	    /* If linked element install failed, then don't erase. */
	    if (p->linkFailed == 0) {
		if ((xx != rpmpsmStage(psm, PSM_PKGERASE)) != 0) {
		    ourrc++;
		} else
		    p->done = 1;
	    } else
		ourrc++;

	    (void) rpmswExit(rpmtsOp(ts, RPMTS_OP_ERASE), 0);

	    /*@switchbreak@*/ break;
	}

	/* Would have freed header above in TR_ADD portion of switch
	 * but needed the header to add it to the autorollback transaction.
	 */
	if (rpmteType(p) == TR_ADDED)
	    p->h = headerFree(p->h);

	xx = rpmdbSync(rpmtsGetRdb(ts));

/*@-nullstate@*/ /* FIX: psm->fi may be NULL */
	psm = rpmpsmFree(psm);
/*@=nullstate@*/

#ifdef	DYING
/*@-type@*/ /* FIX: p is almost opaque */
	p->fi = rpmfiFree(p->fi);
/*@=type@*/
#endif

	/* If we received an error, lets break out and rollback, provided
	 * autorollback is enabled.
	 */
	if (ourrc && rollbackFailures) break;
    }
/*@=nullpass@*/
    /*@=branchstate@*/
    pi = rpmtsiFree(pi);

    /* If we should rollback this transaction on failure, lets do it. */
    if (ourrc && rollbackFailures)
	xx = rpmtsRollback(ts, ignoreSet, 1, p);

    rpmMessage(RPMMESS_DEBUG, _("running post-transaction scripts\n"));
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, TR_ADDED)) != NULL) {
	int haspostscript;

	if ((fi = rpmtsiFi(pi)) == NULL)
	    continue;	/* XXX can't happen */

	haspostscript = (fi->posttrans != NULL ? 1 : 0);
	p->fi = rpmfiFree(p->fi);

	/* If no post-transaction script, then don't bother. */
	if (!haspostscript)
	    continue;

	p->fd = ts->notify(p->h, RPMCALLBACK_INST_OPEN_FILE, 0, 0,
			rpmteKey(p), ts->notifyData);
	p->h = NULL;
	if (rpmteFd(p) != NULL) {
	    rpmVSFlags ovsflags = rpmtsVSFlags(ts);
	    rpmVSFlags vsflags = ovsflags | RPMVSF_NEEDPAYLOAD;
	    rpmRC rpmrc;
	    ovsflags = rpmtsSetVSFlags(ts, vsflags);
	    rpmrc = rpmReadPackageFile(ts, rpmteFd(p),
			rpmteNEVR(p), &p->h);
	    vsflags = rpmtsSetVSFlags(ts, ovsflags);
	    switch (rpmrc) {
	    default:
		p->fd = ts->notify(p->h, RPMCALLBACK_INST_CLOSE_FILE,
				0, 0, rpmteKey(p), ts->notifyData);
		p->fd = NULL;
		/*@switchbreak@*/ break;
	    case RPMRC_NOTTRUSTED:
	    case RPMRC_NOKEY:
	    case RPMRC_OK:
		/*@switchbreak@*/ break;
	    }
	}

/*@-nullpass@*/
	if (rpmteFd(p) != NULL) {
	    p->fi = rpmfiNew(ts, p->h, RPMTAG_BASENAMES, 1);
	    if (p->fi != NULL)	/* XXX can't happen */
		p->fi->te = p;
/*@-compdef -usereleased@*/	/* p->fi->te undefined */
	    psm = rpmpsmNew(ts, p, p->fi);
/*@=compdef =usereleased@*/
assert(psm != NULL);
	    psm->stepName = "posttrans";
	    psm->scriptTag = RPMTAG_POSTTRANS;
	    psm->progTag = RPMTAG_POSTTRANSPROG;
	    xx = rpmpsmStage(psm, PSM_SCRIPT);
	    psm = rpmpsmFree(psm);

/*@-noeffectuncon -compdef -usereleased @*/
	    (void) ts->notify(p->h, RPMCALLBACK_INST_CLOSE_FILE, 0, 0,
			      rpmteKey(p), ts->notifyData);
/*@=noeffectuncon =compdef =usereleased @*/
	    p->fd = NULL;
	    p->fi = rpmfiFree(p->fi);
	    p->h = headerFree(p->h);
	}
/*@=nullpass@*/
    }
    pi = rpmtsiFree(pi);

    lock = rpmtsFreeLock(lock);

    /*@-nullstate@*/ /* FIX: ts->flList may be NULL */
    if (ourrc)
    	return -1;
    else
	return 0;
    /*@=nullstate@*/
}

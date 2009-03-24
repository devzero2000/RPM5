/** \ingroup rpmdep
 * \file lib/rpmts.c
 * Routine(s) to handle a "rpmts" transaction sets.
 */
#include "system.h"

#include <rpmio.h>
#include <rpmiotypes.h>		/* XXX fnpyKey */
#include <rpmlog.h>
#include <iosm.h>		/* XXX iosmFileAction */
#include <rpmurl.h>
#include <rpmpgp.h>
#include <rpmmacro.h>		/* XXX rpmtsOpenDB() needs rpmGetPath */
#include <rpmkeyring.h>

#include <rpmtypes.h>
#include <rpmtag.h>
#include <pkgio.h>

#define	_RPMDB_INTERNAL		/* XXX almost opaque sigh */
#include "rpmdb.h"		/* XXX stealing db->db_mode. */

#include "rpmal.h"
#include "rpmds.h"
#include "rpmfi.h"
#include "rpmlock.h"
#include "rpmns.h"

#define	_RPMTE_INTERNAL		/* XXX te->h */
#include "rpmte.h"

#define	_RPMTS_INTERNAL
#include "rpmts.h"

#include <rpmcli.h>

#include "fs.h"

/* XXX FIXME: merge with existing (broken?) tests in system.h */
/* portability fiddles */
#if STATFS_IN_SYS_STATVFS
/*@-incondefs@*/
#if defined(__LCLINT__)
/*@-declundef -exportheader -protoparammatch @*/ /* LCL: missing annotation */
extern int statvfs (const char * file, /*@out@*/ struct statvfs * buf)
	/*@globals fileSystem @*/
	/*@modifies *buf, fileSystem @*/;
/*@=declundef =exportheader =protoparammatch @*/
/*@=incondefs@*/
#else
# include <sys/statvfs.h>
#endif
#else
# if STATFS_IN_SYS_VFS
#  include <sys/vfs.h>
# else
#  if STATFS_IN_SYS_MOUNT
#   include <sys/mount.h>
#  else
#   if STATFS_IN_SYS_STATFS
#    include <sys/statfs.h>
#   endif
#  endif
# endif
#endif

#include "debug.h"

/*@access FD_t @*/		/* XXX void * arg */
/*@access rpmdb @*/		/* XXX db->db_chrootDone, NULL */

/*@access rpmDiskSpaceInfo @*/
/*@access rpmKeyring @*/
/*@access rpmps @*/
/*@access rpmsx @*/
/*@access rpmte @*/
/*@access rpmtsi @*/
/*@access fnpyKey @*/
/*@access pgpDig @*/
/*@access pgpDigParams @*/

/*@unchecked@*/
int _rpmts_debug = 0;

/*@unchecked@*/
int _rpmts_stats = 0;

/*@unchecked@*/
int _rpmts_macros = 0;

int rpmtsCloseDB(rpmts ts)
{
    int rc = 0;

    if (ts->rdb != NULL) {
	(void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_DBGET), &ts->rdb->db_getops);
	(void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_DBPUT), &ts->rdb->db_putops);
	(void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_DBDEL), &ts->rdb->db_delops);
	rc = rpmdbClose(ts->rdb);
	ts->rdb = NULL;
    }
    return rc;
}

int rpmtsOpenDB(rpmts ts, int dbmode)
{
    int rc = 0;

    if (ts->rdb != NULL && ts->dbmode == dbmode)
	return 0;

    (void) rpmtsCloseDB(ts);

    /* XXX there's a db lock race here that is the callers responsibility. */

    ts->dbmode = dbmode;
    rc = rpmdbOpen(ts->rootDir, &ts->rdb, ts->dbmode, 0644);
    if (rc) {
	const char * dn;
	dn = rpmGetPath(ts->rootDir, "%{_dbpath}", NULL);
	rpmlog(RPMLOG_ERR,
			_("cannot open Packages database in %s\n"), dn);
	dn = _free(dn);
    }
    return rc;
}

int rpmtsInitDB(rpmts ts, int dbmode)
{
#if defined(SUPPORT_INITDB)
    void *lock = rpmtsAcquireLock(ts);
    int rc = rpmdbInit(ts->rootDir, dbmode);
    lock = rpmtsFreeLock(lock);
    return rc;
#else
    return -1;
#endif
}

int rpmtsRebuildDB(rpmts ts)
{
    void * lock = rpmtsAcquireLock(ts);
    int rc = rpmtsOpenDB(ts, ts->dbmode);

    if (rc == 0)
	rc = rpmdbRebuild(ts->rootDir,
    		(!(rpmtsVSFlags(ts) & RPMVSF_NOHDRCHK) ? ts : NULL));
    lock = rpmtsFreeLock(lock);
    return rc;
}

int rpmtsVerifyDB(rpmts ts)
{
#if defined(SUPPORT_VERIFYDB)
    return rpmdbVerify(ts->rootDir);
#else
    return -1;
#endif
}

/*@-compdef@*/ /* keyp might not be defined. */
rpmdbMatchIterator rpmtsInitIterator(const rpmts ts, rpmTag rpmtag,
			const void * keyp, size_t keylen)
{
    rpmdbMatchIterator mi;
    const char * arch = NULL;
    int xx;

    if (ts->rdb == NULL && rpmtsOpenDB(ts, ts->dbmode))
	return NULL;

    /* Parse out "N(EVR).A" tokens from a label key. */
    if (rpmtag == RPMDBI_LABEL && keyp != NULL) {
	const char * s = keyp;
	const char *se;
	size_t slen = strlen(s);
	char *t = alloca(slen+1);
	int level = 0;
	int c;

	keyp = t;
	while ((c = *s++) != '\0') {
	    switch (c) {
	    default:
		*t++ = (char)c;
		/*@switchbreak@*/ break;
	    case '(':
		/* XXX Fail if nested parens. */
		if (level++ != 0) {
		    rpmlog(RPMLOG_ERR, _("extra '(' in package label: %s\n"), (char *)keyp);
		    return NULL;
		}
		/* Parse explicit epoch. */
		for (se = s; *se && xisdigit(*se); se++)
		    {};
		if (*se == ':') {
		    /* XXX skip explicit epoch's (for now) */
		    *t++ = '-';
		    s = se + 1;
		} else {
		    /* No Epoch: found. Convert '(' to '-' and chug. */
		    *t++ = '-';
		}
		/*@switchbreak@*/ break;
	    case ')':
		/* XXX Fail if nested parens. */
		if (--level != 0) {
		    rpmlog(RPMLOG_ERR, _("missing '(' in package label: %s\n"), (char *)keyp);
		    return NULL;
		}
		/* Don't copy trailing ')' */
		/*@switchbreak@*/ break;
	    }
	}
	if (level) {
	    rpmlog(RPMLOG_ERR, _("missing ')' in package label: %s\n"), (char *)keyp);
	    return NULL;
	}
	*t = '\0';
	t = (char *) keyp;
	t = strrchr(t, '.');
	/* Is this a valid ".arch" suffix? */
	if (t != NULL && rpmnsArch(t+1)) {
	   *t++ = '\0';
	   arch = t;
	}
    }

    mi = rpmdbInitIterator(ts->rdb, rpmtag, keyp, keylen);

    /* Verify header signature/digest during retrieve (if not disabled). */
    if (mi && !(rpmtsVSFlags(ts) & RPMVSF_NOHDRCHK))
	(void) rpmdbSetHdrChk(mi, ts);

    /* Select specified arch only. */
    if (arch != NULL)
	xx = rpmdbSetIteratorRE(mi, RPMTAG_ARCH, RPMMIRE_DEFAULT, arch);
    return mi;
}
/*@=compdef@*/

int rpmtsCloseSDB(rpmts ts)
{
    int rc = 0;

    if (ts->sdb != NULL) {
	(void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_DBGET), &ts->sdb->db_getops);
	(void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_DBPUT), &ts->sdb->db_putops);
	(void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_DBDEL), &ts->sdb->db_delops);
	rc = rpmdbClose(ts->sdb);
	ts->sdb = NULL;
    }
    return rc;
}

int rpmtsOpenSDB(rpmts ts, int dbmode)
{
    static int has_sdbpath = -1;
    int rc = 0;

    if (ts->sdb != NULL && ts->sdbmode == dbmode)
	return 0;

    if (has_sdbpath < 0)
	has_sdbpath = rpmExpandNumeric("%{?_solve_dbpath:1}");

    /* If not configured, don't try to open. */
    if (has_sdbpath <= 0)
	return 1;

    addMacro(NULL, "_dbpath", NULL, "%{_solve_dbpath}", RMIL_DEFAULT);

    rc = rpmdbOpen(ts->rootDir, &ts->sdb, ts->sdbmode, 0644);
    if (rc) {
	const char * dn;
	dn = rpmGetPath(ts->rootDir, "%{_dbpath}", NULL);
	rpmlog(RPMLOG_WARNING,
			_("cannot open Solve database in %s\n"), dn);
	dn = _free(dn);
	/* XXX only try to open the solvedb once. */
	has_sdbpath = 0;
    }
    delMacro(NULL, "_dbpath");

    return rc;
}

/**
 * Compare suggested package resolutions (qsort/bsearch).
 * @param a		1st instance address
 * @param b		2nd instance address
 * @return		result of comparison
 */
static int sugcmp(const void * a, const void * b)
	/*@*/
{
    const char * astr = *(const char **)a;
    const char * bstr = *(const char **)b;
    return strcmp(astr, bstr);
}

int rpmtsSolve(rpmts ts, rpmds ds, /*@unused@*/ const void * data)
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    const char * errstr;
    const char * str = NULL;
    const char * qfmt;
    rpmdbMatchIterator mi;
    Header bh = NULL;
    Header h = NULL;
    size_t bhnamelen = 0;
    time_t bhtime = 0;
    rpmTag rpmtag;
    const char * keyp;
    size_t keylen = 0;
    int rc = 1;	/* assume not found */
    int xx;

    /* Make suggestions only for installing Requires: */
    if (ts->goal != TSM_INSTALL)
	return rc;

    switch (rpmdsTagN(ds)) {
    case RPMTAG_CONFLICTNAME:
    default:
	return rc;
	/*@notreached@*/ break;
    case RPMTAG_DIRNAMES:	/* XXX perhaps too many wrong answers */
    case RPMTAG_REQUIRENAME:
    case RPMTAG_FILELINKTOS:
	break;
    }

    keyp = rpmdsN(ds);
    if (keyp == NULL)
	return rc;

    if (ts->sdb == NULL) {
	xx = rpmtsOpenSDB(ts, ts->sdbmode);
	if (xx) return rc;
    }

    /* Look for a matching Provides: in suggested universe. */
    rpmtag = (*keyp == '/' ? RPMTAG_BASENAMES : RPMTAG_PROVIDENAME);
    mi = rpmdbInitIterator(ts->sdb, rpmtag, keyp, keylen);
    while ((h = rpmdbNextIterator(mi)) != NULL) {
	size_t hnamelen;
	time_t htime;

	if (rpmtag == RPMTAG_PROVIDENAME && !rpmdsAnyMatchesDep(h, ds, 1))
	    continue;

	he->tag = RPMTAG_NAME;
	xx = headerGet(h, he, 0);
	hnamelen = ((xx && he->p.str) ? strlen(he->p.str) : 0);
	he->p.ptr = _free(he->p.ptr);

	/* XXX Prefer the shortest pkg N for basenames/provides resp. */
	if (bhnamelen > 0 && hnamelen > bhnamelen)
	    continue;

	/* XXX Prefer the newest build if given alternatives. */
	he->tag = RPMTAG_BUILDTIME;
	xx = headerGet(h, he, 0);
	htime = (xx && he->p.ui32p ? he->p.ui32p[0] : 0);
	he->p.ptr = _free(he->p.ptr);

	if (htime <= bhtime)
	    continue;

	/* Save new "best" candidate. */
	bh = headerFree(bh);
	bh = headerLink(h);
	bhtime = htime;
	bhnamelen = hnamelen;
    }
    mi = rpmdbFreeIterator(mi);

    /* Is there a suggested resolution? */
    if (bh == NULL)
	goto exit;

    /* Format the suggested resolution path. */
    qfmt = rpmExpand("%{?_solve_name_fmt}", NULL);
    if (qfmt == NULL || *qfmt == '\0')
	goto exit;
    str = headerSprintf(bh, qfmt, NULL, rpmHeaderFormats, &errstr);
    bh = headerFree(bh);
    qfmt = _free(qfmt);
    if (str == NULL) {
	rpmlog(RPMLOG_ERR, _("incorrect solve path format: %s\n"), errstr);
	goto exit;
    }

    if (ts->depFlags & RPMDEPS_FLAG_ADDINDEPS) {
	FD_t fd;
	rpmRC rpmrc;

	fd = Fopen(str, "r.fdio");
	if (fd == NULL || Ferror(fd)) {
	    rpmlog(RPMLOG_ERR, _("open of %s failed: %s\n"), str,
			Fstrerror(fd));
            if (fd != NULL) {
                xx = Fclose(fd);
                fd = NULL;
            }
	    str = _free(str);
	    goto exit;
	}
	rpmrc = rpmReadPackageFile(ts, fd, str, &h);
	xx = Fclose(fd);
	switch (rpmrc) {
	default:
	    break;
	case RPMRC_NOTTRUSTED:
	case RPMRC_NOKEY:
	case RPMRC_OK:
	    if (h != NULL &&
	        !rpmtsAddInstallElement(ts, h, (fnpyKey)str, 1, NULL))
	    {
		rpmlog(RPMLOG_DEBUG, D_("Adding: %s\n"), str);
		rc = -1;	/* XXX restart unsatisfiedDepends() */
		break;
	    }
	    break;
	}
	str = _free(str);
	h = headerFree(h);
	goto exit;
    }

    rpmlog(RPMLOG_DEBUG, D_("Suggesting: %s\n"), str);
    /* If suggestion is already present, don't bother. */
    if (ts->suggests != NULL && ts->nsuggests > 0) {
	if (bsearch(&str, ts->suggests, ts->nsuggests,
			sizeof(*ts->suggests), sugcmp))
	{
	    str = _free(str);
	    goto exit;
	}
    }

    /* Add a new (unique) suggestion. */
    ts->suggests = xrealloc(ts->suggests,
			sizeof(*ts->suggests) * (ts->nsuggests + 2));
    ts->suggests[ts->nsuggests] = str;
    ts->nsuggests++;
    ts->suggests[ts->nsuggests] = NULL;

    if (ts->nsuggests > 1)
	qsort(ts->suggests, ts->nsuggests, sizeof(*ts->suggests), sugcmp);

exit:
/*@-nullstate@*/ /* FIX: ts->suggests[] may be NULL */
    return rc;
/*@=nullstate@*/
}

int rpmtsAvailable(rpmts ts, const rpmds ds)
{
    fnpyKey * sugkey;
    int rc = 1;	/* assume not found */

    if (ts->availablePackages == NULL)
	return rc;
    sugkey = rpmalAllSatisfiesDepend(ts->availablePackages, ds, NULL);
    if (sugkey == NULL)
	return rc;

    /* XXX no alternatives yet */
    if (sugkey[0] != NULL) {
	ts->suggests = xrealloc(ts->suggests,
			sizeof(*ts->suggests) * (ts->nsuggests + 2));
	ts->suggests[ts->nsuggests] = sugkey[0];
	sugkey[0] = NULL;
	ts->nsuggests++;
	ts->suggests[ts->nsuggests] = NULL;
    }
    sugkey = _free(sugkey);
/*@-nullstate@*/ /* FIX: ts->suggests[] may be NULL */
    return rc;
/*@=nullstate@*/
}

int rpmtsSetSolveCallback(rpmts ts,
		int (*solve) (rpmts ts, rpmds key, const void * data),
		const void * solveData)
{
    int rc = 0;

    if (ts) {
/*@-assignexpose -temptrans @*/
	ts->solve = solve;
	ts->solveData = solveData;
/*@=assignexpose =temptrans @*/
    }
    return rc;
}

rpmps rpmtsProblems(rpmts ts)
{
    static const char msg[] = "rpmtsProblems";
    rpmps ps = NULL;
    if (ts) {
	if (ts->probs == NULL)
	    ts->probs = rpmpsCreate();
	ps = rpmpsLink(ts->probs, msg);
    }
    return ps;
}

void rpmtsClean(rpmts ts)
{
    rpmtsi pi; rpmte p;

    if (ts == NULL)
	return;

    /* Clean up after dependency checks. */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL)
	rpmteCleanDS(p);
    pi = rpmtsiFree(pi);

    ts->addedPackages = rpmalFree(ts->addedPackages);
    ts->numAddedPackages = 0;

    ts->erasedPackages = rpmalFree(ts->erasedPackages);
    ts->numErasedPackages = 0;

    ts->suggests = _free(ts->suggests);
    ts->nsuggests = 0;

    ts->probs = rpmpsFree(ts->probs);

    rpmtsCleanDig(ts);
}

void rpmtsEmpty(rpmts ts)
{
    rpmtsi pi; rpmte p;
    int oc;

    if (ts == NULL)
	return;

/*@-nullstate@*/	/* FIX: partial annotations */
    rpmtsClean(ts);
/*@=nullstate@*/

    for (pi = rpmtsiInit(ts), oc = 0; (p = rpmtsiNext(pi, 0)) != NULL; oc++) {
/*@-type -unqualifiedtrans @*/
	ts->order[oc] = rpmteFree(ts->order[oc]);
/*@=type =unqualifiedtrans @*/
    }
    pi = rpmtsiFree(pi);

    ts->orderCount = 0;
    ts->ntrees = 0;
    ts->maxDepth = 0;

    ts->numRemovedPackages = 0;
/*@-nullstate@*/	/* FIX: partial annotations */
    return;
/*@=nullstate@*/
}

static void rpmtsPrintStat(const char * name, /*@null@*/ struct rpmop_s * op)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    static unsigned int scale = (1000 * 1000);
    if (op != NULL && op->count > 0)
	fprintf(stderr, "   %s %8d %6lu.%06lu MB %6lu.%06lu secs\n",
		name, op->count,
		(unsigned long)op->bytes/scale, (unsigned long)op->bytes%scale,
		op->usecs/scale, op->usecs%scale);
}

/*@unchecked@*/ /*@relnull@*/
extern rpmop _hdr_loadops;
/*@unchecked@*/ /*@relnull@*/
extern rpmop _hdr_getops;

static void rpmtsPrintStats(rpmts ts)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    (void) rpmswExit(rpmtsOp(ts, RPMTS_OP_TOTAL), 0);

    if (_hdr_loadops)
	(void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_HDRLOAD), _hdr_loadops);
    if (_hdr_getops)
	(void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_HDRGET), _hdr_getops);

    rpmtsPrintStat("total:       ", rpmtsOp(ts, RPMTS_OP_TOTAL));
    rpmtsPrintStat("check:       ", rpmtsOp(ts, RPMTS_OP_CHECK));
    rpmtsPrintStat("order:       ", rpmtsOp(ts, RPMTS_OP_ORDER));
    rpmtsPrintStat("fingerprint: ", rpmtsOp(ts, RPMTS_OP_FINGERPRINT));
    rpmtsPrintStat("repackage:   ", rpmtsOp(ts, RPMTS_OP_REPACKAGE));
    rpmtsPrintStat("install:     ", rpmtsOp(ts, RPMTS_OP_INSTALL));
    rpmtsPrintStat("erase:       ", rpmtsOp(ts, RPMTS_OP_ERASE));
    rpmtsPrintStat("scriptlets:  ", rpmtsOp(ts, RPMTS_OP_SCRIPTLETS));
    rpmtsPrintStat("compress:    ", rpmtsOp(ts, RPMTS_OP_COMPRESS));
    rpmtsPrintStat("uncompress:  ", rpmtsOp(ts, RPMTS_OP_UNCOMPRESS));
    rpmtsPrintStat("digest:      ", rpmtsOp(ts, RPMTS_OP_DIGEST));
    rpmtsPrintStat("signature:   ", rpmtsOp(ts, RPMTS_OP_SIGNATURE));
    rpmtsPrintStat("dbadd:       ", rpmtsOp(ts, RPMTS_OP_DBADD));
    rpmtsPrintStat("dbremove:    ", rpmtsOp(ts, RPMTS_OP_DBREMOVE));
    rpmtsPrintStat("dbget:       ", rpmtsOp(ts, RPMTS_OP_DBGET));
    rpmtsPrintStat("dbput:       ", rpmtsOp(ts, RPMTS_OP_DBPUT));
    rpmtsPrintStat("dbdel:       ", rpmtsOp(ts, RPMTS_OP_DBDEL));
    rpmtsPrintStat("readhdr:     ", rpmtsOp(ts, RPMTS_OP_READHDR));
    rpmtsPrintStat("hdrload:     ", rpmtsOp(ts, RPMTS_OP_HDRLOAD));
    rpmtsPrintStat("hdrget:      ", rpmtsOp(ts, RPMTS_OP_HDRGET));
/*@-globstate@*/
    return;
/*@=globstate@*/
}

static void rpmtsFini(void * _ts)
	/*@modifies *_ts @*/
{
    rpmts ts = _ts;

/*@-nullstate@*/	/* FIX: partial annotations */
    /* XXX there's a recursion here ... release and reacquire the lock */
#ifndef	BUGGY
    yarnRelease(ts->_item.use);	/* XXX hack-o-round */
#endif
    rpmtsEmpty(ts);
#ifndef	BUGGY
    yarnPossess(ts->_item.use);	/* XXX hack-o-round */
#endif
/*@=nullstate@*/

    ts->PRCO = rpmdsFreePRCO(ts->PRCO);

    (void) rpmtsCloseDB(ts);

    (void) rpmtsCloseSDB(ts);

    ts->sx = rpmsxFree(ts->sx);

    ts->removedPackages = _free(ts->removedPackages);

    ts->availablePackages = rpmalFree(ts->availablePackages);
    ts->numAvailablePackages = 0;

    ts->dsi = _free(ts->dsi);

    if (ts->scriptFd != NULL) {
/*@-refcounttrans@*/	/* FIX: XfdFree annotation */
	ts->scriptFd = fdFree(ts->scriptFd, "rpmtsFree");
/*@=refcounttrans@*/
	ts->scriptFd = NULL;
    }
    ts->rootDir = _free(ts->rootDir);
    ts->currDir = _free(ts->currDir);

/*@-type +voidabstract @*/	/* FIX: double indirection */
    ts->order = _free(ts->order);
/*@=type =voidabstract @*/
    ts->orderAlloced = 0;

    ts->keyring = rpmKeyringFree(ts->keyring);
    ts->pkpkt = _free(ts->pkpkt);
    ts->pkpktlen = 0;
    memset(ts->pksignid, 0, sizeof(ts->pksignid));

    if (_rpmts_stats)
	rpmtsPrintStats(ts);

    if (_rpmts_macros) {
	const char ** av = NULL;
/*@-globs@*/	/* Avoid rpmGlobalMcroContext et al. */
	(void)rpmGetMacroEntries(NULL, NULL, 1, &av);
/*@=globs@*/
	argvPrint("macros used", av, NULL);
	av = argvFree(av);
    }
}

rpmts rpmtsFree(rpmts ts)
{
    (void) rpmioFreePoolItem((rpmioItem)ts, __FUNCTION__, __FILE__, __LINE__);
    return NULL;
}

/*@unchecked@*/ /*@null@*/
rpmioPool _rpmtsPool;

static rpmts rpmtsGetPool(/*@null@*/ rpmioPool pool)
	/*@modifies pool @*/
{
    rpmts ts;

    if (_rpmtsPool == NULL) {
	_rpmtsPool = rpmioNewPool("ts", sizeof(*ts), -1, _rpmts_debug,
			NULL, NULL, rpmtsFini);
	pool = _rpmtsPool;
    }
    return (rpmts) rpmioGetPool(pool, sizeof(*ts));
}

void * rpmtsGetKeyring(rpmts ts, /*@unused@*/ int autoload)
{
    rpmKeyring keyring = NULL;
    if (ts) {
#ifdef	NOTYET
	if (ts->keyring == NULL && autoload)
	    loadKeyring(ts);
	keyring = rpmKeyringLink(ts->keyring);
#else
	keyring = ts->keyring;
#endif
    }
/*@-refcounttrans@*/
    return (void *)keyring;
/*@=refcounttrans@*/
}

int rpmtsSetKeyring(rpmts ts, void * _keyring)
{
    rpmKeyring keyring = _keyring;

    if (ts == NULL)
       return -1;

#ifdef	NOTYET
    /*
     * Should we permit switching keyring on the fly? For now, require
     * rpmdb isn't open yet (fairly arbitrary limitation)...
     */
    if (rpmtsGetRdb(ts) != NULL)
	return -1;
#endif

/*@-modnomods@*/
    ts->keyring = rpmKeyringFree(ts->keyring);
/*@=modnomods@*/

#ifdef	NOTYET
    ts->keyring = rpmKeyringLink(keyring);
#else
/*@-assignexpose -newreftrans @*/
/*@i@*/    ts->keyring = keyring;
/*@=assignexpose =newreftrans @*/
#endif

    return 0;
}

rpmVSFlags rpmtsVSFlags(/*@unused@*/ rpmts ts)
{
    return pgpDigVSFlags;
}

rpmVSFlags rpmtsSetVSFlags(/*@unused@*/ rpmts ts, rpmVSFlags vsflags)
	/*@globals pgpDigVSFlags @*/
	/*@modifies pgpDigVSFlags @*/
{
    rpmVSFlags ovsflags;
    ovsflags = pgpDigVSFlags;
    pgpDigVSFlags = vsflags;
    return ovsflags;
}

/*
 * This allows us to mark transactions as being of a certain type.
 * The three types are:
 *
 *     RPM_TRANS_NORMAL 	
 *     RPM_TRANS_ROLLBACK
 *     RPM_TRANS_AUTOROLLBACK
 *
 * ROLLBACK and AUTOROLLBACK transactions should always be ran as
 * a best effort.  In particular this is important to the autorollback
 * feature to avoid rolling back a rollback (otherwise known as
 * dueling rollbacks (-;).  AUTOROLLBACK's additionally need instance
 * counts passed to scriptlets to be altered.
 */
/* Let them know what type of transaction we are */
rpmTSType rpmtsType(rpmts ts)
{
    return ((ts != NULL) ? ts->type : 0);
}

void rpmtsSetType(rpmts ts, rpmTSType type)
{
    if (ts != NULL)
	ts->type = type;
}

rpmuint32_t rpmtsARBGoal(rpmts ts)
{
    return ((ts != NULL) ?  ts->arbgoal : 0);
}

void rpmtsSetARBGoal(rpmts ts, rpmuint32_t goal)
{
    if (ts != NULL)
	ts->arbgoal = goal;
}

int rpmtsUnorderedSuccessors(rpmts ts, int first)
{
    int unorderedSuccessors = 0;
    if (ts != NULL) {
	unorderedSuccessors = ts->unorderedSuccessors;
	if (first >= 0)
	    ts->unorderedSuccessors = first;
    }
    return unorderedSuccessors;
}

const char * rpmtsRootDir(rpmts ts)
{
    const char * rootDir = NULL;

    if (ts != NULL && ts->rootDir != NULL) {
	urltype ut = urlPath(ts->rootDir, &rootDir);
	switch (ut) {
	case URL_IS_UNKNOWN:
	case URL_IS_PATH:
	    break;
	case URL_IS_HTTPS:
	case URL_IS_HTTP:
	case URL_IS_HKP:
	case URL_IS_FTP:
	case URL_IS_DASH:
	default:
	    rootDir = "/";
	    break;
	}
    }
    return rootDir;
}

void rpmtsSetRootDir(rpmts ts, const char * rootDir)
{
    if (ts != NULL) {
	size_t rootLen;

	ts->rootDir = _free(ts->rootDir);

	if (rootDir == NULL) {
#ifndef	DYING
	    ts->rootDir = xstrdup("");
#endif
	    return;
	}
	rootLen = strlen(rootDir);

	/* Make sure that rootDir has trailing / */
	if (!(rootLen && rootDir[rootLen - 1] == '/')) {
	    char * t = alloca(rootLen + 2);
	    *t = '\0';
	    (void) stpcpy( stpcpy(t, rootDir), "/");
	    rootDir = t;
	}
	ts->rootDir = xstrdup(rootDir);
    }
}

const char * rpmtsCurrDir(rpmts ts)
{
    const char * currDir = NULL;
    if (ts != NULL) {
	currDir = ts->currDir;
    }
    return currDir;
}

void rpmtsSetCurrDir(rpmts ts, const char * currDir)
{
    if (ts != NULL) {
	ts->currDir = _free(ts->currDir);
	if (currDir)
	    ts->currDir = xstrdup(currDir);
    }
}

FD_t rpmtsScriptFd(rpmts ts)
{
    FD_t scriptFd = NULL;
    if (ts != NULL) {
	scriptFd = ts->scriptFd;
    }
/*@-compdef -refcounttrans -usereleased@*/
    return scriptFd;
/*@=compdef =refcounttrans =usereleased@*/
}

void rpmtsSetScriptFd(rpmts ts, FD_t scriptFd)
{

    if (ts != NULL) {
	if (ts->scriptFd != NULL) {
/*@-refcounttrans@*/	/* FIX: XfdFree annotation */
	    ts->scriptFd = fdFree(ts->scriptFd, "rpmtsSetScriptFd");
/*@=refcounttrans@*/
	    ts->scriptFd = NULL;
	}
/*@-refcounttrans@*/	/* FIX: XfdLink annotation */
	if (scriptFd != NULL)
	    ts->scriptFd = fdLink((void *)scriptFd, "rpmtsSetScriptFd");
/*@=refcounttrans@*/
    }
}

int rpmtsSELinuxEnabled(rpmts ts)
{
    int selinuxEnabled = 0;
    if (ts)
	selinuxEnabled = (ts->selinuxEnabled > 0);
    return selinuxEnabled;
}

int rpmtsChrootDone(rpmts ts)
{
    return (ts != NULL ? ts->chrootDone : 0);
}

int rpmtsSetChrootDone(rpmts ts, int chrootDone)
{
    int ochrootDone = 0;
    if (ts != NULL) {
	ochrootDone = ts->chrootDone;
	if (ts->rdb != NULL)
	    ts->rdb->db_chrootDone = chrootDone;
	ts->chrootDone = chrootDone;
    }
    return ochrootDone;
}

rpmuint32_t rpmtsGetTid(rpmts ts)
{
    rpmuint32_t tid = 0;	/* XXX -1 is time(2) error return. */
    if (ts != NULL) {
	tid = ts->tid[0];
    }
    return tid;
}

rpmuint32_t rpmtsSetTid(rpmts ts, rpmuint32_t tid)
{
    rpmuint32_t otid = 0;	/* XXX -1 is time(2) error return. */
    if (ts != NULL) {
	otid = ts->tid[0];
	ts->tid[0] = tid;
	ts->tid[1] = 0;
    }
    return otid;
}

rpmPRCO rpmtsPRCO(rpmts ts)
{
    rpmPRCO PRCO = NULL;

    if (ts != NULL) {
	static int oneshot = 0;
	if (!oneshot) {
	    const char * fn = rpmGetPath("%{?_rpmds_sysinfo_path}", NULL);
	    int xx;

	    ts->PRCO = rpmdsNewPRCO(NULL);
	    if (fn && *fn != '\0' && !rpmioAccess(fn, NULL, R_OK))
		xx = rpmdsSysinfo(ts->PRCO, NULL);
	    fn = _free(fn);
	    oneshot++;
	}
	PRCO = ts->PRCO;
    }
/*@-compdef -retexpose -usereleased @*/
    return PRCO;
/*@=compdef =retexpose =usereleased @*/
}

int rpmtsInitDSI(const rpmts ts)
{
    rpmDiskSpaceInfo dsi;
    struct stat sb;
    int rc;
    size_t i;

    if (rpmtsFilterFlags(ts) & RPMPROB_FILTER_DISKSPACE)
	return 0;
    if (ts->filesystems != NULL)
	return 0;

    rpmlog(RPMLOG_DEBUG, D_("mounted filesystems:\n"));
    rpmlog(RPMLOG_DEBUG,
	D_("    i        dev    bsize       bavail       iavail mount point\n"));

    rc = rpmGetFilesystemList(&ts->filesystems, &ts->filesystemCount);
    if (rc || ts->filesystems == NULL || ts->filesystemCount == 0)
	return rc;

    /* Get available space on mounted file systems. */

    ts->dsi = _free(ts->dsi);
    ts->dsi = xcalloc((ts->filesystemCount + 1), sizeof(*ts->dsi));

    dsi = ts->dsi;

    if (dsi != NULL)
    for (i = 0; (i < ts->filesystemCount) && dsi; i++, dsi++) {
#if STATFS_IN_SYS_STATVFS
	struct statvfs sfb;
	memset(&sfb, 0, sizeof(sfb));
	rc = statvfs(ts->filesystems[i], &sfb);
#else
	struct statfs sfb;
	memset(&sfb, 0, sizeof(sfb));
#  if STAT_STATFS4
/* This platform has the 4-argument version of the statfs call.  The last two
 * should be the size of struct statfs and 0, respectively.  The 0 is the
 * filesystem type, and is always 0 when statfs is called on a mounted
 * filesystem, as we're doing.
 */
	rc = statfs(ts->filesystems[i], &sfb, sizeof(sfb), 0);
#  else
	rc = statfs(ts->filesystems[i], &sfb);
#  endif
#endif
	if (rc)
	    break;

	rc = stat(ts->filesystems[i], &sb);
	if (rc)
	    break;
	dsi->dev = sb.st_dev;
/* XXX figger out how to get this info for non-statvfs systems. */
#if STATFS_IN_SYS_STATVFS
	dsi->f_frsize = sfb.f_frsize;
#if defined(RPM_OS_AIX)
	dsi->f_fsid = 0; /* sfb.f_fsid is a structure on AIX */
#else
	dsi->f_fsid = sfb.f_fsid;
#endif
	dsi->f_flag = sfb.f_flag;
	dsi->f_favail = (long long) sfb.f_favail;
	dsi->f_namemax = sfb.f_namemax;
#elif defined(__APPLE__) && defined(__MACH__) && !defined(_SYS_STATVFS_H_)
	dsi->f_fsid = 0; /* "Not meaningful in this implementation." */
	dsi->f_namemax = pathconf(ts->filesystems[i], _PC_NAME_MAX);
#elif defined(__OpenBSD__)
	dsi->f_fsid = 0; /* sfb.f_fsid is a structure on OpenBSD */
	dsi->f_namemax = pathconf(ts->filesystems[i], _PC_NAME_MAX);
#else
	dsi->f_fsid = sfb.f_fsid;
	dsi->f_namemax = sfb.f_namelen;
#endif

	dsi->f_bsize = sfb.f_bsize;
	dsi->f_blocks = (unsigned long long)sfb.f_blocks;
	dsi->f_bfree = (unsigned long long)sfb.f_bfree;
	dsi->f_files = (unsigned long long)sfb.f_files;
	dsi->f_ffree = (unsigned long long)sfb.f_ffree;

	dsi->bneeded = 0;
	dsi->ineeded = 0;
#ifdef STATFS_HAS_F_BAVAIL
	dsi->f_bavail = (long long)(sfb.f_bavail ? sfb.f_bavail : 1);
	if (sfb.f_ffree > 0 && sfb.f_files > 0 && sfb.f_favail > 0)
	    dsi->f_favail = (long long)sfb.f_favail;
	else	/* XXX who knows what evil lurks here? */
	    dsi->f_favail = !(sfb.f_ffree == 0 && sfb.f_files == 0)
				?  (signed long long) sfb.f_ffree : -1;
#else
/* FIXME: the statfs struct doesn't have a member to tell how many blocks are
 * available for non-superusers.  f_blocks - f_bfree is probably too big, but
 * it's about all we can do.
 */
	dsi->f_bavail = sfb.f_blocks - sfb.f_bfree;
	/* XXX Avoid FAT and other file systems that have not inodes. */
	dsi->f_favail = !(sfb.f_ffree == 0 && sfb.f_files == 0)
				? sfb.f_ffree : -1;
#endif

#if !defined(ST_RDONLY)
#define	ST_RDONLY	1
#endif
	rpmlog(RPMLOG_DEBUG, "%5u 0x%08x %8u %12ld %12ld %s %s\n",
		(unsigned)i, (unsigned) dsi->dev, (unsigned) dsi->f_bsize,
		(signed long) dsi->f_bavail, (signed long) dsi->f_favail,
		((dsi->f_flag & ST_RDONLY) ? "ro" : "rw"),
		ts->filesystems[i]);
    }
    return rc;
}

void rpmtsUpdateDSI(const rpmts ts, dev_t dev,
		rpmuint32_t fileSize, rpmuint32_t prevSize, rpmuint32_t fixupSize,
		int _action)
{
    iosmFileAction action = _action;
    rpmDiskSpaceInfo dsi;
    rpmuint64_t bneeded;

    dsi = ts->dsi;
    if (dsi) {
	while (dsi->f_bsize && dsi->dev != dev)
	    dsi++;
	if (dsi->f_bsize == 0)
	    dsi = NULL;
    }
    if (dsi == NULL)
	return;

    bneeded = BLOCK_ROUND(fileSize, dsi->f_bsize);

    switch (action) {
    case FA_BACKUP:
    case FA_SAVE:
    case FA_ALTNAME:
	dsi->ineeded++;
	dsi->bneeded += bneeded;
	/*@switchbreak@*/ break;

    /*
     * FIXME: If two packages share a file (same md5sum), and
     * that file is being replaced on disk, will dsi->bneeded get
     * adjusted twice? Quite probably!
     */
    case FA_CREATE:
	dsi->bneeded += bneeded;
	dsi->bneeded -= BLOCK_ROUND(prevSize, dsi->f_bsize);
	/*@switchbreak@*/ break;

    case FA_ERASE:
	dsi->ineeded--;
	dsi->bneeded -= bneeded;
	/*@switchbreak@*/ break;

    default:
	/*@switchbreak@*/ break;
    }

    if (fixupSize)
	dsi->bneeded -= BLOCK_ROUND(fixupSize, dsi->f_bsize);
}

void rpmtsCheckDSIProblems(const rpmts ts, const rpmte te)
{
    rpmDiskSpaceInfo dsi;
    rpmps ps;
    int fc;
    size_t i;

    if (ts->filesystems == NULL || ts->filesystemCount == 0)
	return;

    dsi = ts->dsi;
    if (dsi == NULL)
	return;
    fc = rpmfiFC( rpmteFI(te, RPMTAG_BASENAMES) );
    if (fc <= 0)
	return;

    ps = rpmtsProblems(ts);
    for (i = 0; i < ts->filesystemCount; i++, dsi++) {

	if (dsi->f_bavail > 0 && adj_fs_blocks(dsi->bneeded) > dsi->f_bavail) {
	    rpmpsAppend(ps, RPMPROB_DISKSPACE,
			rpmteNEVR(te), rpmteKey(te),
			ts->filesystems[i], NULL, NULL,
 	   (adj_fs_blocks(dsi->bneeded) - dsi->f_bavail) * dsi->f_bsize);
	}

	if (dsi->f_favail > 0 && adj_fs_blocks(dsi->ineeded) > dsi->f_favail) {
	    rpmpsAppend(ps, RPMPROB_DISKNODES,
			rpmteNEVR(te), rpmteKey(te),
			ts->filesystems[i], NULL, NULL,
 	    (adj_fs_blocks(dsi->ineeded) - dsi->f_favail));
	}

	if ((dsi->bneeded || dsi->ineeded) && (dsi->f_flag & ST_RDONLY)) {
	    rpmpsAppend(ps, RPMPROB_RDONLY,
			rpmteNEVR(te), rpmteKey(te),
			ts->filesystems[i], NULL, NULL, 0);
	}
    }
    ps = rpmpsFree(ps);
}

void * rpmtsNotify(rpmts ts, rpmte te,
		rpmCallbackType what, rpmuint64_t amount, rpmuint64_t total)
{
    void * ptr = NULL;
    if (ts && ts->notify) {
	Header h;
	fnpyKey cbkey;
	/*@-type@*/ /* FIX: cast? */
	/*@-noeffectuncon @*/ /* FIX: check rc */
	if (te) {
/*@-mods@*/	/* XXX noisy in transaction.c */
	    h = headerLink(te->h);
/*@=mods@*/
	    cbkey = rpmteKey(te);
	} else {
	    h = NULL;
	    cbkey = NULL;
	}
	ptr = ts->notify(h, what, amount, total, cbkey, ts->notifyData);
	h = headerFree(h);
	/*@=noeffectuncon @*/
	/*@=type@*/
    }
    return ptr;
}

int rpmtsNElements(rpmts ts)
{
    int nelements = 0;
    if (ts != NULL && ts->order != NULL) {
	nelements = ts->orderCount;
    }
    return nelements;
}

rpmte rpmtsElement(rpmts ts, int ix)
{
    rpmte te = NULL;
    if (ts != NULL && ts->order != NULL) {
	if (ix >= 0 && ix < ts->orderCount)
	    te = ts->order[ix];
    }
    /*@-compdef@*/
    return te;
    /*@=compdef@*/
}

rpmprobFilterFlags rpmtsFilterFlags(rpmts ts)
{
    return (ts != NULL ? ts->ignoreSet : 0);
}

rpmtransFlags rpmtsFlags(rpmts ts)
{
    rpmtransFlags transFlags = 0;
    if (ts != NULL) {
	transFlags = ts->transFlags;
	if (rpmtsSELinuxEnabled(ts) > 0)
	    transFlags |= RPMTRANS_FLAG_NOCONTEXTS;
	else
	    transFlags &= ~RPMTRANS_FLAG_NOCONTEXTS;
    }
    return transFlags;
}

rpmtransFlags rpmtsSetFlags(rpmts ts, rpmtransFlags transFlags)
{
    rpmtransFlags otransFlags = 0;
    if (ts != NULL) {
	otransFlags = ts->transFlags;
	if (rpmtsSELinuxEnabled(ts) > 0)
	    transFlags |= RPMTRANS_FLAG_NOCONTEXTS;
	else
	    transFlags &= ~RPMTRANS_FLAG_NOCONTEXTS;
	ts->transFlags = transFlags;
    }
    return otransFlags;
}

rpmdepFlags rpmtsDFlags(rpmts ts)
{
    return (ts != NULL ? ts->depFlags : 0);
}

rpmdepFlags rpmtsSetDFlags(rpmts ts, rpmdepFlags depFlags)
{
    rpmdepFlags odepFlags = 0;
    if (ts != NULL) {
	odepFlags = ts->depFlags;
	ts->depFlags = depFlags;
    }
    return odepFlags;
}

Spec rpmtsSpec(rpmts ts)
{
/*@-compdef -retexpose -usereleased@*/
    return ts->spec;
/*@=compdef =retexpose =usereleased@*/
}

Spec rpmtsSetSpec(rpmts ts, Spec spec)
{
    Spec ospec = ts->spec;
/*@-assignexpose -temptrans@*/
    ts->spec = spec;
/*@=assignexpose =temptrans@*/
    return ospec;
}

rpmte rpmtsRelocateElement(rpmts ts)
{
/*@-compdef -retexpose -usereleased@*/
    return ts->relocateElement;
/*@=compdef =retexpose =usereleased@*/
}

rpmte rpmtsSetRelocateElement(rpmts ts, rpmte relocateElement)
{
    rpmte orelocateElement = ts->relocateElement;
/*@-assignexpose -temptrans@*/
    ts->relocateElement = relocateElement;
/*@=assignexpose =temptrans@*/
    return orelocateElement;
}

tsmStage rpmtsGoal(rpmts ts)
{
    return (ts != NULL ? ts->goal : TSM_UNKNOWN);
}

tsmStage rpmtsSetGoal(rpmts ts, tsmStage goal)
{
    tsmStage ogoal = TSM_UNKNOWN;
    if (ts != NULL) {
	ogoal = ts->goal;
	ts->goal = goal;
    }
    return ogoal;
}

int rpmtsDBMode(rpmts ts)
{
    return (ts != NULL ? ts->dbmode : 0);
}

int rpmtsSetDBMode(rpmts ts, int dbmode)
{
    int odbmode = 0;
    if (ts != NULL) {
	odbmode = ts->dbmode;
	ts->dbmode = dbmode;
    }
    return odbmode;
}

rpmuint32_t rpmtsColor(rpmts ts)
{
    return (ts != NULL ? ts->color : 0);
}

rpmuint32_t rpmtsSetColor(rpmts ts, rpmuint32_t color)
{
    rpmuint32_t ocolor = 0;
    if (ts != NULL) {
	ocolor = ts->color;
	ts->color = color;
    }
    return ocolor;
}

rpmuint32_t rpmtsPrefColor(rpmts ts)
{
    return (ts != NULL ? ts->prefcolor : 0);
}

int rpmtsSetNotifyCallback(rpmts ts,
		rpmCallbackFunction notify, rpmCallbackData notifyData)
{
    if (ts != NULL) {
	ts->notify = notify;
	ts->notifyData = notifyData;
    }
    return 0;
}

rpmts rpmtsCreate(void)
{
    rpmts ts = rpmtsGetPool(_rpmtsPool);
    int xx;

    memset(&ts->ops, 0, sizeof(ts->ops));
    (void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_TOTAL), -1);
    ts->type = RPMTRANS_TYPE_NORMAL;
    ts->goal = TSM_UNKNOWN;
    ts->filesystemCount = 0;
    ts->filesystems = NULL;
    ts->dsi = NULL;

    ts->solve = rpmtsSolve;
    ts->solveData = NULL;
    ts->nsuggests = 0;
    ts->suggests = NULL;

    ts->PRCO = NULL;

    ts->sdb = NULL;
    ts->sdbmode = O_RDONLY;

    ts->rdb = NULL;
    ts->dbmode = O_RDONLY;

    ts->scriptFd = NULL;
    {   struct timeval tv;
	xx = gettimeofday(&tv, NULL);
	ts->tid[0] = (rpmuint32_t) tv.tv_sec;
        ts->tid[1] = (rpmuint32_t) tv.tv_usec;
    }
    ts->delta = 5;

    ts->color = rpmExpandNumeric("%{?_transaction_color}");
    ts->prefcolor = rpmExpandNumeric("%{?_prefer_color}");
    if (!ts->prefcolor) ts->prefcolor = 0x2;

    ts->numRemovedPackages = 0;
    ts->allocedRemovedPackages = ts->delta;
    ts->removedPackages = xcalloc(ts->allocedRemovedPackages,
			sizeof(*ts->removedPackages));

    ts->rootDir = NULL;
    ts->currDir = NULL;
    ts->chrootDone = 0;

    ts->selinuxEnabled = is_selinux_enabled();

    ts->numAddedPackages = 0;
    ts->addedPackages = NULL;

    ts->numErasedPackages = 0;
    ts->erasedPackages = NULL;

    ts->numAvailablePackages = 0;
    ts->availablePackages = NULL;

    ts->orderAlloced = 0;
    ts->orderCount = 0;
    ts->order = NULL;
    ts->ntrees = 0;
    ts->maxDepth = 0;

    ts->probs = NULL;

    ts->keyring = NULL;
    ts->pkpkt = NULL;
    ts->pkpktlen = 0;
    memset(ts->pksignid, 0, sizeof(ts->pksignid));
    ts->dig = NULL;

    /* Set autorollback goal to the end of time. */
    ts->arbgoal = 0xffffffff;

    return rpmtsLink(ts, "tsCreate");
}

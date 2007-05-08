/** \ingroup rpmcli
 * \file lib/rpmrollback.c
 */

#include "system.h"

#include <rpmcli.h>

#include "rpmdb.h"
#include "rpmds.h"

#define	_RPMTE_INTERNAL		/* XXX findErases needs rpmte internals. */
#include "rpmte.h"		/* XXX: rpmteChain */
#define	_RPMTS_INTERNAL		/* ts->goal, ts->dbmode, ts->suggests */
#include "rpmts.h"

#include "manifest.h"
#include "misc.h"		/* XXX rpmGlob() */
#include "rpmgi.h"		/* XXX rpmgiEscapeSpaces */
#include "debug.h"

/*@access rpmts @*/	/* XXX ts->goal, ts->dbmode */
/*@access rpmte @*/	/* XXX p->hdrid, p->pkgid, p->NEVRA */
/*@access IDTX @*/
/*@access IDT @*/

/*@unchecked@*/
static int reverse = -1;

/**
 */
static int IDTintcmp(const void * a, const void * b)
	/*@*/
{
    /*@-castexpose@*/
    return ( reverse * (((IDT)a)->val.u32 - ((IDT)b)->val.u32) );
    /*@=castexpose@*/
}

IDTX IDTXfree(IDTX idtx)
{
    if (idtx) {
	int i;
	if (idtx->idt)
	for (i = 0; i < idtx->nidt; i++) {
	    IDT idt = idtx->idt + i;
	    idt->h = headerFree(idt->h);
	    idt->key = _free(idt->key);
	}
	idtx->idt = _free(idtx->idt);
	idtx = _free(idtx);
    }
    return NULL;
}

IDTX IDTXnew(void)
{
    IDTX idtx = xcalloc(1, sizeof(*idtx));
    idtx->delta = 10;
    idtx->size = sizeof(*((IDT)0));
    return idtx;
}

IDTX IDTXgrow(IDTX idtx, int need)
{
    if (need < 0) return NULL;
    if (idtx == NULL)
	idtx = IDTXnew();
    if (need == 0) return idtx;

    if ((idtx->nidt + need) > idtx->alloced) {
	while (need > 0) {
	    idtx->alloced += idtx->delta;
	    need -= idtx->delta;
	}
	idtx->idt = xrealloc(idtx->idt, (idtx->alloced * idtx->size) );
    }
    return idtx;
}

IDTX IDTXsort(IDTX idtx)
{
    if (idtx != NULL && idtx->idt != NULL && idtx->nidt > 0)
	qsort(idtx->idt, idtx->nidt, idtx->size, IDTintcmp);
    return idtx;
}

IDTX IDTXload(rpmts ts, rpmTag tag, uint_32 rbtid)
{
    IDTX idtx = NULL;
    rpmdbMatchIterator mi;
    HGE_t hge = (HGE_t) headerGetEntry;
    Header h;

    /*@-branchstate@*/
    mi = rpmtsInitIterator(ts, tag, NULL, 0);
#ifdef	NOTYET
    (void) rpmdbSetIteratorRE(mi, RPMTAG_NAME, RPMMIRE_DEFAULT, '!gpg-pubkey');
#endif
    while ((h = rpmdbNextIterator(mi)) != NULL) {
	rpmTagType type = RPM_NULL_TYPE;
	int_32 count = 0;
	int_32 * tidp;

	tidp = NULL;
	if (!hge(h, tag, &type, (void **)&tidp, &count) || tidp == NULL)
	    continue;

	if (type == RPM_INT32_TYPE && (*tidp == 0 || *tidp == -1))
	    continue;

	/* Don't bother with headers installed prior to the rollback goal. */
	if (*tidp < rbtid)
	    continue;

	idtx = IDTXgrow(idtx, 1);
	if (idtx == NULL || idtx->idt == NULL)
	    continue;

	{   IDT idt;
	    /*@-nullderef@*/
	    idt = idtx->idt + idtx->nidt;
	    /*@=nullderef@*/
	    idt->done = 0;
	    idt->h = headerLink(h);
	    idt->key = NULL;
	    idt->instance = rpmdbGetIteratorOffset(mi);
	    idt->val.u32 = *tidp;
	}
	idtx->nidt++;
    }
    mi = rpmdbFreeIterator(mi);
    /*@=branchstate@*/

    return IDTXsort(idtx);
}

IDTX IDTXglob(rpmts ts, const char * globstr, rpmTag tag, uint_32 rbtid)
{
    HGE_t hge = (HGE_t) headerGetEntry;		/* XXX MinMem? <shrug> */
    IDTX idtx = NULL;
    Header h;
    int_32 * tidp;
    FD_t fd;
    const char ** av = NULL;
    const char * fn;
    int ac = 0;
    rpmRC rpmrc;
    int xx;
    int i;

    av = NULL;	ac = 0;
    fn = rpmgiEscapeSpaces(globstr);
    xx = rpmGlob(fn, &ac, &av);
    fn = _free(fn);

/*@-branchstate@*/
    if (xx == 0)
    for (i = 0; i < ac; i++) {
	rpmTagType type;
	int_32 count;
	int isSource;

	fd = Fopen(av[i], "r");
	if (fd == NULL || Ferror(fd)) {
	    rpmError(RPMERR_OPEN, _("open of %s failed: %s\n"), av[i],
	    		Fstrerror(fd));
	    if (fd != NULL) (void) Fclose(fd);
	    continue;
	}

	rpmrc = rpmReadPackageFile(ts, fd, av[i], &h);
	(void) Fclose(fd);
	switch (rpmrc) {
	default:
	    goto bottom;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	case RPMRC_NOTTRUSTED:
	case RPMRC_NOKEY:
	case RPMRC_OK:
	    isSource = (headerIsEntry(h, RPMTAG_SOURCERPM) == 0);
	    if (isSource)
		goto bottom;
	    /*@switchbreak@*/ break;
	}

{ const char * origin = headerGetOrigin(h);
assert(origin != NULL);
assert(!strcmp(av[i], origin));
}
	tidp = NULL;
	if (!hge(h, tag, &type, (void **) &tidp, &count) || tidp == NULL)
	    goto bottom;

	/* Don't bother with headers installed prior to the rollback goal. */
	if (*tidp < rbtid)
	    goto bottom;

	idtx = IDTXgrow(idtx, 1);
	if (idtx == NULL || idtx->idt == NULL)
	    goto bottom;

	{   IDT idt;
	    idt = idtx->idt + idtx->nidt;
	    idt->done = 0;
	    idt->h = headerLink(h);
	    idt->key = av[i];
	    av[i] = NULL;
	    idt->instance = 0;
	    idt->val.u32 = *tidp;
	}
	idtx->nidt++;
bottom:
	h = headerFree(h);
    }
/*@=branchstate@*/

    for (i = 0; i < ac; i++)
	av[i] = _free(av[i]);
    av = _free(av);	ac = 0;

    return IDTXsort(idtx);
}

/**
 * Search for string B in argv array AV.
 * @param ts		transaction set
 * @param lname		type of link
 * @param AV		argv array
 * @param AC		no. of args
 * @param B		string
 * @return		1 if found, 0 not found, -1 error
 */
static int cmpArgvStr(rpmts ts, const char *lname, const char ** AV, int AC,
		/*@null@*/ const char * B)
	/*@*/
{
    const char * A;
    int i;

    if (AV != NULL && AC > 0 && B == NULL) {
      if (!strcmp(lname, "NEVRA")) {
	rpmps ps = rpmtsProblems(ts);
	for (i = 0; i < AC && (A = AV[i]) != NULL; i++) {
fprintf(stderr, "==> %s(%p) missing %s[%d]: %s\n", __FUNCTION__, ts, lname, i, A);
	    rpmpsAppend(ps, RPMPROB_NOREPACKAGE,
			NULL, NULL,	/* NEVRA, key */
			lname, NULL,	/* dn, bn */
			A,		/* altNEVRA */
			0);
	}
	ps = rpmpsFree(ps);
      }
	return 0;
    }

    if (AV != NULL && B != NULL)
    for (i = 0; i < AC && (A = AV[i]) != NULL; i++) {
	if (*A && *B && !strcmp(A, B))
	    return 1;
    }
    return 0;
}

/**
 * Find (and add to transaction set) all erase elements with matching blink.
 * In addition, recreate any added transaction element linkages.
 *
 * XXX rp->h should have FLINK{HDRID,PKGID,NEVRA} populated.
 * XXX ip->h should have BLINK{HDRID,PKGID,NEVRA} populated.
 * XXX p = ts->teInstall is added transaction element from rp->h.
 *
 * @param ts		transaction set (ts->teInstall set to last added pkg)
 * @param p		most recently added install element (NULL skips linking)
 * @param thistid	current transaction id
 * @param ip		currently installed package(s) to be erased
 * @param niids		no. of currently installed package(s)
 * @return		-1 on error, otherwise no. of erase elemnts added
 */
static int findErases(rpmts ts, /*@null@*/ rpmte p, unsigned thistid,
		/*@null@*/ IDT ip, int niids)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, p, ip, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    int rc = 0;
    int xx;

fprintf(stderr, "==> %s(%p)\n", __FUNCTION__, ts);
    /* Erase the previously installed packages for this transaction. 
     * Provided this transaction is not excluded from the rollback.
     */
    while (ip != NULL && ip->val.u32 == thistid) {

	if (ip->done)
	    goto bottom;

	{
	    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
	    const char ** flinkPkgid = NULL;
	    const char ** flinkHdrid = NULL;
	    const char ** flinkNEVRA = NULL;
	    rpmTagType pt, ht, nt;
	    int_32 pn, hn, nn;
	    int bingo;

	    xx = hge(ip->h, RPMTAG_BLINKPKGID, &pt, (void **)&flinkPkgid, &pn);

	    /* XXX Always erase packages at beginning of upgrade chain. */
	    if (pn == 1 && flinkPkgid[0] != NULL && !strcmp(flinkPkgid[0], RPMTE_CHAIN_END)) {
		flinkPkgid = headerFreeData(flinkPkgid, pt);
		goto erase;
	    }

	    xx = hge(ip->h, RPMTAG_BLINKHDRID, &ht, (void **)&flinkHdrid, &hn);
	    xx = hge(ip->h, RPMTAG_BLINKNEVRA, &nt, (void **)&flinkNEVRA, &nn);

	    /*
	     * Link data may be missing and can have multiple entries.
	     */
	    /* XXX Until link tags are reliably populated, check in the order
	     * 	NEVRA -> hdrid -> pkgid
	     * because NEVRA is easier to debug (hdrid/pkgid are more precise.)
	    */
	    bingo = 0;
	    if (!bingo)
		bingo = cmpArgvStr(ts, "NEVRA", flinkNEVRA, nn, (p ? p->NEVRA : NULL));
	    if (!bingo)
		bingo = cmpArgvStr(ts, "Hdrid", flinkHdrid, hn, (p ? p->hdrid : NULL));
	    if (!bingo)
		bingo = cmpArgvStr(ts, "Pkgid", flinkPkgid, pn, (p ? p->pkgid : NULL));
	    flinkPkgid = headerFreeData(flinkPkgid, pt);
	    flinkHdrid = headerFreeData(flinkHdrid, ht);
	    flinkNEVRA = headerFreeData(flinkNEVRA, nt);

	    if (bingo < 0) {
		rc = -1;
		goto exit;
	    }

	    if (!bingo)
		goto bottom;
	}

erase:
	rpmMessage(RPMMESS_DEBUG, "\t--- erase h#%u\n", ip->instance);

	rc = rpmtsAddEraseElement(ts, ip->h, ip->instance);
	if (rc != 0)
	    goto exit;

	/* Cross link the transaction elements to mimic --upgrade. */
	if (p != NULL) {
	    rpmte q = ts->teErase;
	    xx = rpmteChain(p, q, ip->h, "Rollback");
	}

#ifdef	NOTYET
	ip->instance = 0;
#endif
	ip->done = 1;

bottom:

	/* Go to the next header in the rpmdb */
	niids--;
	if (niids > 0)
	    ip++;
	else
	    ip = NULL;
    }

exit:
    return rc;
}

static int rpmrbProblems(rpmts ts, /*@null@*/ const char * msg, int rc)
{
    rpmps ps = rpmtsProblems(ts);

    if (rc != 0 && rpmpsNumProblems(ps) > 0) {
	if (msg)
	    rpmMessage(RPMMESS_ERROR, "%s:\n", msg);
	rpmpsPrint(NULL, ps);
    }
    ps = rpmpsFree(ps);
    return rc;
}

int rpmrbCheck(rpmts ts)
{
    return rpmrbProblems(ts, N_("Failed dependencies"), rpmtsCheck(ts));
}

int rpmrbOrder(rpmts ts)
{
    return rpmrbProblems(ts, N_("Ordering problems"), rpmtsOrder(ts));
}

int rpmrbRun(rpmts ts, rpmps okProbs, rpmprobFilterFlags ignoreSet)
{
    return rpmrbProblems(ts, N_("Rollback problems"),
			rpmtsRun(ts, okProbs, ignoreSet));
}

/** @todo Transaction handling, more, needs work. */
int rpmRollback(rpmts ts, QVA_t ia, const char ** argv)
{
    int ifmask= (INSTALL_UPGRADE|INSTALL_FRESHEN|INSTALL_INSTALL|INSTALL_ERASE);
    unsigned thistid = 0xffffffff;
    unsigned prevtid;
    time_t tid;
    IDTX itids = NULL;
    IDTX rtids = NULL;
    IDT rp;
    int nrids = 0;
    IDT ip;
    int niids = 0;
    int rc = 0;
    int vsflags, ovsflags;
    int numAdded;
    int numRemoved;
    int _unsafe_rollbacks = 0;
    rpmtransFlags transFlags = ia->transFlags;
    rpmdepFlags depFlags = ia->depFlags;
    int xx;

    if (argv != NULL && *argv != NULL) {
	rc = -1;
	goto exit;
    }

    _unsafe_rollbacks = rpmExpandNumeric("%{?_unsafe_rollbacks}");

    vsflags = rpmExpandNumeric("%{?_vsflags_erase}");
    if (ia->qva_flags & VERIFY_DIGEST)
	vsflags |= _RPMVSF_NODIGESTS;
    if (ia->qva_flags & VERIFY_SIGNATURE)
	vsflags |= _RPMVSF_NOSIGNATURES;
    if (ia->qva_flags & VERIFY_HDRCHK)
	vsflags |= RPMVSF_NOHDRCHK;
    vsflags |= RPMVSF_NEEDPAYLOAD;	/* XXX no legacy signatures */
    ovsflags = rpmtsSetVSFlags(ts, vsflags);

    (void) rpmtsSetFlags(ts, transFlags);
    (void) rpmtsSetDFlags(ts, depFlags);

    /*  Make the transaction a rollback transaction.  In a rollback
     *  a best effort is what we want 
     */
    rpmtsSetType(ts, RPMTRANS_TYPE_ROLLBACK);

    itids = IDTXload(ts, RPMTAG_INSTALLTID, ia->rbtid);
    if (itids != NULL) {
	ip = itids->idt;
	niids = itids->nidt;
    } else {
	ip = NULL;
	niids = 0;
    }

    {	const char * globstr = rpmExpand("%{_repackage_dir}/*.rpm", NULL);
	if (globstr == NULL || *globstr == '%') {
	    globstr = _free(globstr);
	    rc = -1;
	    goto exit;
	}
	rtids = IDTXglob(ts, globstr, RPMTAG_REMOVETID, ia->rbtid);

	if (rtids != NULL) {
	    rp = rtids->idt;
	    nrids = rtids->nidt;
	} else {
	    rp = NULL;
	    nrids = 0;
	}
	globstr = _free(globstr);
    }

    {	int notifyFlags, xx;
	notifyFlags = ia->installInterfaceFlags | (rpmIsVerbose() ? INSTALL_LABEL : 0 );
	xx = rpmtsSetNotifyCallback(ts,
			rpmShowProgress, (void *) ((long)notifyFlags));
    }

    /* Run transactions until rollback goal is achieved. */
    do {
	prevtid = thistid;
	rc = 0;
	rpmcliPackagesTotal = 0;
	numAdded = 0;
	numRemoved = 0;
	ia->installInterfaceFlags &= ~ifmask;

	/* Find larger of the remaining install/erase transaction id's. */
	thistid = 0;
	if (ip != NULL && ip->val.u32 > thistid)
	    thistid = ip->val.u32;
	if (rp != NULL && rp->val.u32 > thistid)
	    thistid = rp->val.u32;

	/* If we've achieved the rollback goal, then we're done. */
	if (thistid == 0 || thistid < ia->rbtid)
	    break;

	/* If we've reached the (configured) rollback goal, then we're done. */
	if (_unsafe_rollbacks && thistid <= _unsafe_rollbacks)
	    break;

	/* Is this transaction excluded from the rollback? */
	if (ia->rbtidExcludes != NULL && ia->numrbtidExcludes > 0)
	{
	    uint_32 *excludedTID;
	    int excluded = 0;
	    for(excludedTID = ia->rbtidExcludes; 
		excludedTID < ia->rbtidExcludes + ia->numrbtidExcludes;
		excludedTID++) {
		if (thistid == *excludedTID) {
		    time_t ttid = (time_t)thistid;
		    rpmMessage(RPMMESS_NORMAL,
			_("Excluding TID from rollback: %-24.24s (0x%08x)\n"),
				ctime(&ttid), thistid);
		    excluded = 1;
		    /*@innerbreak@*/ break;	
		}
	    }	
	    if (excluded) {
		/* Iterate over repackaged packages */
		while (rp != NULL && rp->val.u32 == thistid) {
		    /* Go to the next repackaged package */
		    nrids--;
		    if (nrids > 0)
			rp++;
		    else
			rp = NULL;
		}
		/* Iterate over installed packages */
		while (ip != NULL && ip->val.u32 == thistid) {
		    /* Go to the next header in the rpmdb */
		    niids--;
		    if (niids > 0)
			ip++;
		    else
			ip = NULL;
		}
		continue;		/* with next transaction */
	    }
	}

	rpmtsEmpty(ts);
	(void) rpmtsSetFlags(ts, transFlags);
	(void) rpmtsSetDFlags(ts, depFlags);
	ts->probs = rpmpsFree(ts->probs);
	ts->probs = rpmpsCreate();

	/* Install the previously erased packages for this transaction. 
	 */
	while (rp != NULL && rp->val.u32 == thistid) {
	    if (!rp->done) {
		rpmMessage(RPMMESS_DEBUG, "\t+++ install %s\n",
			(rp->key ? rp->key : "???"));

/*@-abstract@*/
		rc = rpmtsAddInstallElement(ts, rp->h, (fnpyKey)rp->key,
			       0, ia->relocations);
/*@=abstract@*/
		if (rc != 0)
		    goto exit;

		numAdded++;
		rpmcliPackagesTotal++;
		if (!(ia->installInterfaceFlags & ifmask))
		    ia->installInterfaceFlags |= INSTALL_UPGRADE;

		/* Re-add linked (i.e. from upgrade/obsoletes) erasures. */
		rc = findErases(ts, ts->teInstall, thistid, ip, niids);
		if (rc < 0)
		    goto exit;
#ifdef	NOTYET
	    	rp->h = headerFree(rp->h);
#endif
		rp->done = 1;
	    }

	    /* Go to the next repackaged package */
	    nrids--;
	    if (nrids > 0)
		rp++;
	    else
		rp = NULL;
	}

	/* Re-add pure (i.e. not from upgrade/obsoletes) erasures. */
	rc = findErases(ts, NULL, thistid, ip, niids);
	if (rc < 0)
	    goto exit;

	/* Check that all erasures have been re-added. */
	while (ip != NULL && ip->val.u32 == thistid) {
#ifdef	NOTNOW
/* XXX Prevent incomplete rollback transactions. */
assert(ip->done || ia->no_rollback_links);
#endif
	    if (!(ip->done || ia->no_rollback_links)) {
		numRemoved++;

		if (_unsafe_rollbacks)
		    rpmcliPackagesTotal++;

		if (!(ia->installInterfaceFlags & ifmask))
		    ia->installInterfaceFlags |= INSTALL_ERASE;
	    }

	    /* Go to the next header in the rpmdb */
	    niids--;
	    if (niids > 0)
		ip++;
	    else
		ip = NULL;
	}

	/* Print any rollback transaction problems */
	(void) rpmrbProblems(ts, N_("Missing re-packaged package(s)"), 1);

	/* Anything to do? */
	if (rpmcliPackagesTotal <= 0)
	    break;

	tid = (time_t)thistid;
	rpmMessage(RPMMESS_NORMAL,
		_("Rollback packages (+%d/-%d) to %-24.24s (0x%08x):\n"),
			numAdded, numRemoved, ctime(&tid), thistid);

	rc = (ia->rbCheck ? (*ia->rbCheck) (ts) : 0);
	if (rc != 0)
	    goto exit;

	rc = (ia->rbOrder ? (*ia->rbOrder) (ts) : 0);
	if (rc != 0)
	    goto exit;

	/* Drop added/available package indices and dependency sets. */
	rpmtsClean(ts);

	/* Print the transaction set. */
	xx = rpmtsPrint(ts, stdout);

	rc = (ia->rbRun
	    ? (*ia->rbRun)(ts, NULL, (ia->probFilter|RPMPROB_FILTER_OLDPACKAGE))
	    : 0);
	if (rc != 0)
	    goto exit;

	/* Remove repackaged packages after successful reinstall. */
	if (rtids && !rpmIsDebug()) {
	    int i;
	    rpmMessage(RPMMESS_NORMAL, _("Cleaning up repackaged packages:\n"));
	    if (rtids->idt)
	    for (i = 0; i < rtids->nidt; i++) {
		IDT rrp = rtids->idt + i;
		if (rrp->val.u32 != thistid)
		    /*@innercontinue@*/ continue;
		if (rrp->key) {	/* XXX can't happen */
		    rpmMessage(RPMMESS_NORMAL, _("\tRemoving %s:\n"), rrp->key);
		    (void) unlink(rrp->key);	/* XXX: Should check rc??? */
		}
	    }
	}

    } while (1);

exit:
    rtids = IDTXfree(rtids);
    itids = IDTXfree(itids);

    rpmtsEmpty(ts);
    (void) rpmtsSetFlags(ts, transFlags);
    (void) rpmtsSetDFlags(ts, depFlags);

    return rc;
}

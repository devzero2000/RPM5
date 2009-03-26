/** \ingroup header
 * \file lib/package.c
 */

#include "system.h"

#include <netinet/in.h>

#include <rpmio_internal.h>
#include <rpmcb.h>		/* XXX fnpyKey */

#include <rpmtag.h>
#include <rpmtypes.h>
#include <pkgio.h>
#include "signature.h"		/* XXX rpmVerifySignature */

#include "rpmts.h"

#include "debug.h"
#define headerFree() rpmioFreePoolItem()

#define	alloca_strdup(_s)	strcpy(alloca(strlen(_s)+1), (_s))

/*@access pgpDig @*/
/*@access pgpDigParams @*/
/*@access Header @*/		/* XXX compared with NULL */
/*@access FD_t @*/		/* XXX void * */

/*@unchecked@*/
static int _print_pkts = 0;

/*@unchecked@*/
static unsigned int nkeyids_max = 256;
/*@unchecked@*/
static unsigned int nkeyids = 0;
/*@unchecked@*/
static unsigned int nextkeyid  = 0;
/*@unchecked@*/ /*@only@*/ /*@null@*/
unsigned int * keyids = NULL;

/**
 * Remember current key id.
 * @param dig		container
 * @return		0 if new keyid, otherwise 1
 */
static int pgpStashKeyid(pgpDig dig)
	/*@globals nextkeyid, nkeyids, keyids @*/
	/*@modifies nextkeyid, nkeyids, keyids @*/
{
    pgpDigParams sigp = pgpGetSignature(dig);
    const void * sig = pgpGetSig(dig);
    unsigned int keyid;
    unsigned int i;

    if (sig == NULL || dig == NULL || sigp == NULL)
	return 0;

    keyid = pgpGrab(sigp->signid+4, 4);
    if (keyid == 0)
	return 0;

    if (keyids != NULL)
    for (i = 0; i < nkeyids; i++) {
	if (keyid == keyids[i])
	    return 1;
    }

    if (nkeyids < nkeyids_max) {
	nkeyids++;
	keyids = xrealloc(keyids, nkeyids * sizeof(*keyids));
    }
    if (keyids)		/* XXX can't happen */
	keyids[nextkeyid] = keyid;
    nextkeyid++;
    nextkeyid %= nkeyids_max;

    return 0;
}

/*@-mods@*/
rpmRC rpmReadPackageFile(rpmts ts, FD_t fd, const char * fn, Header * hdrp)
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    HE_t she = memset(alloca(sizeof(*she)), 0, sizeof(*she));
    pgpDig dig = rpmtsDig(ts);
    char buf[8*BUFSIZ];
    ssize_t count;
    Header sigh = NULL;
    rpmtsOpX opx;
    rpmop op = NULL;
    size_t nb;
    Header h = NULL;
    const char * msg = NULL;
    rpmVSFlags vsflags;
    rpmRC rc = RPMRC_FAIL;	/* assume failure */
    rpmop opsave = memset(alloca(sizeof(*opsave)), 0, sizeof(*opsave));
    int xx;

    if (hdrp) *hdrp = NULL;

assert(dig != NULL);
    (void) fdSetDig(fd, dig);

    /* Snapshot current I/O counters (cached persistent I/O reuses counters) */
    (void) rpmswAdd(opsave, fdstat_op(fd, FDSTAT_READ));

   {	const char item[] = "Lead";
	msg = NULL;
	rc = rpmpkgRead(item, fd, NULL, &msg);
	switch (rc) {
	default:
	   rpmlog(RPMLOG_ERR, "%s: %s: %s\n", fn, item, msg);
	   /*@fallthrough@*/
	case RPMRC_NOTFOUND:
	   msg = _free(msg);
	   goto exit;
	   /*@notreached@*/ break;
	case RPMRC_OK:
	   break;
	}
	msg = _free(msg);
    }

    {	const char item[] = "Signature";
	msg = NULL;
	rc = rpmpkgRead(item, fd, &sigh, &msg);
	switch (rc) {
	default:
	    rpmlog(RPMLOG_ERR, "%s: %s: %s", fn, item,
		(msg && *msg ? msg : _("read failed\n")));
	    msg = _free(msg);
	    goto exit;
	    /*@notreached@*/ break;
	case RPMRC_OK:
	    if (sigh == NULL) {
		rpmlog(RPMLOG_ERR, _("%s: No signature available\n"), fn);
		rc = RPMRC_FAIL;
		goto exit;
	    }
	    break;
	}
	msg = _free(msg);
    }

#define	_chk(_mask)	(she->tag == 0 && !(vsflags & (_mask)))

    /*
     * Figger the most effective available signature.
     * Prefer signatures over digests, then header-only over header+payload.
     * DSA will be preferred over RSA if both exist because tested first.
     * Note that NEEDPAYLOAD prevents header+payload signatures and digests.
     */
    she->tag = 0;
    opx = 0;
    vsflags = pgpDigVSFlags;
    if (_chk(RPMVSF_NODSAHEADER) && headerIsEntry(sigh, (rpmTag)RPMSIGTAG_DSA)) {
	she->tag = (rpmTag)RPMSIGTAG_DSA;
    } else
    if (_chk(RPMVSF_NORSAHEADER) && headerIsEntry(sigh, (rpmTag)RPMSIGTAG_RSA)) {
	she->tag = (rpmTag)RPMSIGTAG_RSA;
    } else
    if (_chk(RPMVSF_NOSHA1HEADER) && headerIsEntry(sigh, (rpmTag)RPMSIGTAG_SHA1)) {
	she->tag = (rpmTag)RPMSIGTAG_SHA1;
    } else
    if (_chk(RPMVSF_NOMD5|RPMVSF_NEEDPAYLOAD) &&
	headerIsEntry(sigh, (rpmTag)RPMSIGTAG_MD5))
    {
	she->tag = (rpmTag)RPMSIGTAG_MD5;
	fdInitDigest(fd, PGPHASHALGO_MD5, 0);
	opx = RPMTS_OP_DIGEST;
    }

    /* Read the metadata, computing digest(s) on the fly. */
    h = NULL;
    msg = NULL;

    /* XXX stats will include header i/o and setup overhead. */
    /* XXX repackaged packages have appended tags, legacy dig/sig check fails */
    if (opx > 0) {
	op = pgpStatsAccumulator(dig, opx);
	(void) rpmswEnter(op, 0);
    }
/*@-type@*/	/* XXX arrow access of non-pointer (FDSTAT_t) */
    nb = fd->stats->ops[FDSTAT_READ].bytes;
    {	const char item[] = "Header";
	msg = NULL;
	rc = rpmpkgRead(item, fd, &h, &msg);
	if (rc != RPMRC_OK) {
	    rpmlog(RPMLOG_ERR, "%s: %s: %s\n", fn, item, msg);
	    msg = _free(msg);
	    goto exit;
	}
	msg = _free(msg);
    }
    nb = fd->stats->ops[FDSTAT_READ].bytes - nb;
/*@=type@*/
    if (opx > 0 && op != NULL) {
	(void) rpmswExit(op, nb);
	op = NULL;
    }

    /* Any digests or signatures to check? */
    if (she->tag == 0) {
	rc = RPMRC_OK;
	goto exit;
    }

    dig->nbytes = 0;

    /* Retrieve the tag parameters from the signature header. */
    xx = headerGet(sigh, she, 0);
    if (she->p.ptr == NULL) {
	rc = RPMRC_FAIL;
	goto exit;
    }
/*@-ownedtrans -noeffect@*/
    xx = pgpSetSig(dig, she->tag, she->t, she->p.ptr, she->c);
/*@=ownedtrans =noeffect@*/

    switch ((rpmSigTag)she->tag) {
    default:	/* XXX keep gcc quiet. */
assert(0);
	/*@notreached@*/ break;
    case RPMSIGTAG_RSA:
	/* Parse the parameters from the OpenPGP packets that will be needed. */
	xx = pgpPrtPkts(she->p.ptr, she->c, dig, (_print_pkts & rpmIsDebug()));
	if (dig->signature.version != 3 && dig->signature.version != 4) {
	    rpmlog(RPMLOG_ERR,
		_("skipping package %s with unverifiable V%u signature\n"),
		fn, dig->signature.version);
	    rc = RPMRC_FAIL;
	    goto exit;
	}
    {	void * uh = NULL;
	rpmTagType uht;
	rpmTagCount uhc;
	unsigned char * hmagic = NULL;
	size_t nmagic = 0;

	he->tag = RPMTAG_HEADERIMMUTABLE;
	xx = headerGet(h, he, 0);
	uht = he->t;
	uh = he->p.ptr;
	uhc = he->c;
	if (!xx)
	    break;
	(void) headerGetMagic(NULL, &hmagic, &nmagic);
	op = pgpStatsAccumulator(dig, 10);	/* RPMTS_OP_DIGEST */
	(void) rpmswEnter(op, 0);
	dig->hdrctx = rpmDigestInit(dig->signature.hash_algo, RPMDIGEST_NONE);
	if (hmagic && nmagic > 0) {
	    (void) rpmDigestUpdate(dig->hdrctx, hmagic, nmagic);
	    dig->nbytes += nmagic;
	}
	(void) rpmDigestUpdate(dig->hdrctx, uh, uhc);
	dig->nbytes += uhc;
	(void) rpmswExit(op, dig->nbytes);
	op->count--;	/* XXX one too many */
	uh = _free(uh);
    }	break;
    case RPMSIGTAG_DSA:
	/* Parse the parameters from the OpenPGP packets that will be needed. */
	xx = pgpPrtPkts(she->p.ptr, she->c, dig, (_print_pkts & rpmIsDebug()));
	if (dig->signature.version != 3 && dig->signature.version != 4) {
	    rpmlog(RPMLOG_ERR,
		_("skipping package %s with unverifiable V%u signature\n"), 
		fn, dig->signature.version);
	    rc = RPMRC_FAIL;
	    goto exit;
	}
	/*@fallthrough@*/
    case RPMSIGTAG_SHA1:
    {	void * uh = NULL;
	rpmTagType uht;
	rpmTagCount uhc;
	unsigned char * hmagic = NULL;
	size_t nmagic = 0;

	he->tag = RPMTAG_HEADERIMMUTABLE;
	xx = headerGet(h, he, 0);
	uht = he->t;
	uh = he->p.ptr;
	uhc = he->c;
	if (!xx)
	    break;
	(void) headerGetMagic(NULL, &hmagic, &nmagic);
	op = pgpStatsAccumulator(dig, 10);	/* RPMTS_OP_DIGEST */
	(void) rpmswEnter(op, 0);
	dig->hdrsha1ctx = rpmDigestInit(PGPHASHALGO_SHA1, RPMDIGEST_NONE);
	if (hmagic && nmagic > 0) {
	    (void) rpmDigestUpdate(dig->hdrsha1ctx, hmagic, nmagic);
	    dig->nbytes += nmagic;
	}
	(void) rpmDigestUpdate(dig->hdrsha1ctx, uh, uhc);
	dig->nbytes += uhc;
	(void) rpmswExit(op, dig->nbytes);
	if ((rpmSigTag)she->tag == RPMSIGTAG_SHA1)
	    op->count--;	/* XXX one too many */
	uh = _free(uh);
    }	break;
    case RPMSIGTAG_MD5:
	/* Legacy signatures need the compressed payload in the digest too. */
	op = pgpStatsAccumulator(dig, 10);	/* RPMTS_OP_DIGEST */
	(void) rpmswEnter(op, 0);
	while ((count = Fread(buf, sizeof(buf[0]), sizeof(buf), fd)) > 0)
	    dig->nbytes += count;
	(void) rpmswExit(op, dig->nbytes);
	op->count--;	/* XXX one too many */
	dig->nbytes += nb;	/* XXX include size of header blob. */
	if (count < 0) {
	    rpmlog(RPMLOG_ERR, _("%s: Fread failed: %s\n"),
					fn, Fstrerror(fd));
	    rc = RPMRC_FAIL;
	    goto exit;
	}

	/* XXX Steal the digest-in-progress from the file handle. */
	fdStealDigest(fd, dig);
	break;
    }

/** @todo Implement disable/enable/warn/error/anal policy. */

    buf[0] = '\0';
    rc = rpmVerifySignature(dig, buf);
    switch (rc) {
    case RPMRC_OK:		/* Signature is OK. */
	rpmlog(RPMLOG_DEBUG, "%s: %s\n", fn, buf);
	break;
    case RPMRC_NOTTRUSTED:	/* Signature is OK, but key is not trusted. */
    case RPMRC_NOKEY:		/* Public key is unavailable. */
	/* XXX Print NOKEY/NOTTRUSTED warning only once. */
    {	int lvl = (pgpStashKeyid(dig) ? RPMLOG_DEBUG : RPMLOG_WARNING);
	rpmlog(lvl, "%s: %s\n", fn, buf);
    }	break;
    case RPMRC_NOTFOUND:	/* Signature is unknown type. */
	rpmlog(RPMLOG_WARNING, "%s: %s\n", fn, buf);
	break;
    default:
    case RPMRC_FAIL:		/* Signature does not verify. */
	rpmlog(RPMLOG_ERR, "%s: %s\n", fn, buf);
	break;
    }

exit:
    if (rc != RPMRC_FAIL && h != NULL && hdrp != NULL) {

	/* Append (and remap) signature tags to the metadata. */
	headerMergeLegacySigs(h, sigh);

	/* Bump reference count for return. */
	*hdrp = headerLink(h);
    }
    h = headerFree(h);

    /* Accumulate time reading package header. */
    (void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_READHDR),
		fdstat_op(fd, FDSTAT_READ));
    (void) rpmswSub(rpmtsOp(ts, RPMTS_OP_READHDR),
		opsave);

    rpmtsCleanDig(ts);
    sigh = headerFree(sigh);
    return rc;
}
/*@=mods@*/

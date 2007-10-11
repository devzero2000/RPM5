/** \ingroup header
 * \file lib/package.c
 */

#include "system.h"

#include <netinet/in.h>

#include <rpmio_internal.h>
#include <rpmlib.h>

#include "rpmts.h"

#include "misc.h"		/* XXX stripTrailingChar() */
#include <pkgio.h>

#include "header_internal.h"	/* XXX headerCheck */
#include "signature.h"
#include "debug.h"

#define	alloca_strdup(_s)	strcpy(alloca(strlen(_s)+1), (_s))

/*@access pgpDig @*/
/*@access pgpDigParams @*/
/*@access Header @*/		/* XXX compared with NULL */
/*@access entryInfo @*/		/* XXX headerCheck */
/*@access indexEntry @*/	/* XXX headerCheck */
/*@access FD_t @*/		/* XXX stealing digests */

/*@unchecked@*/
static int _print_pkts = 0;

/*@unchecked@*/
static unsigned int nkeyids_max = 256;
/*@unchecked@*/
static unsigned int nkeyids = 0;
/*@unchecked@*/
static unsigned int nextkeyid  = 0;
/*@unchecked@*/ /*@only@*/ /*@null@*/
static unsigned int * keyids;

/*@unchecked@*/
extern int _nolead;
/*@unchecked@*/
extern int _nosigh;

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
    int i;

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

rpmRC rpmReadHeader(rpmts ts, void * _fd, Header *hdrp, const char ** msg)
{
    FD_t fd = _fd;
    char buf[BUFSIZ];
    int_32 block[4];
    int_32 il;
    int_32 dl;
    int_32 * ei = NULL;
    size_t uc;
    unsigned char * b;
    size_t nb;
    Header h = NULL;
    const char * origin = NULL;
    rpmRC rc = RPMRC_FAIL;		/* assume failure */
    int xx;

    buf[0] = '\0';

    if (hdrp)
	*hdrp = NULL;
    if (msg)
	*msg = NULL;

    memset(block, 0, sizeof(block));
    if ((xx = timedRead(fd, (char *)block, sizeof(block))) != sizeof(block)) {
	(void) snprintf(buf, sizeof(buf),
		_("hdr size(%d): BAD, read returned %d\n"), (int)sizeof(block), xx);
	goto exit;
    }

    b = NULL;
    nb = 0;
    (void) headerGetMagic(NULL, &b, &nb);
    if (memcmp(block, b, nb)) {
	(void) snprintf(buf, sizeof(buf), _("hdr magic: BAD\n"));
	goto exit;
    }

    il = ntohl(block[2]);
    if (hdrchkTags(il)) {
	(void) snprintf(buf, sizeof(buf),
		_("hdr tags: BAD, no. of tags(%d) out of range\n"), il);

	goto exit;
    }
    dl = ntohl(block[3]);
    if (hdrchkData(dl)) {
	(void) snprintf(buf, sizeof(buf),
		_("hdr data: BAD, no. of bytes(%d) out of range\n"), dl);
	goto exit;
    }

/*@-sizeoftype@*/
    nb = (il * sizeof(struct entryInfo_s)) + dl;
/*@=sizeoftype@*/
    uc = sizeof(il) + sizeof(dl) + nb;
    ei = xmalloc(uc);
    ei[0] = block[2];
    ei[1] = block[3];
    if ((xx = timedRead(fd, (char *)&ei[2], nb)) != nb) {
	(void) snprintf(buf, sizeof(buf),
		_("hdr blob(%u): BAD, read returned %d\n"), (unsigned)nb, xx);
	goto exit;
    }

    /* Sanity check header tags */
    rc = headerCheck(ts, ei, uc, msg);
    if (rc != RPMRC_OK)
	goto exit;

    /* OK, blob looks sane, load the header. */
    h = headerLoad(ei);
    if (h == NULL) {
	(void) snprintf(buf, sizeof(buf), _("hdr load: BAD\n"));
        goto exit;
    }
    h->flags |= HEADERFLAG_ALLOCATED;
    ei = NULL;	/* XXX will be freed with header */

    /* Save the opened path as the header origin. */
    origin = fdGetOPath(fd);
    if (origin != NULL)
	(void) headerSetOrigin(h, origin);
    
exit:
    if (hdrp && h && rc == RPMRC_OK)
	*hdrp = headerLink(h);
    ei = _free(ei);
    h = headerFree(h);

    if (msg != NULL && *msg == NULL && buf[0] != '\0') {
	buf[sizeof(buf)-1] = '\0';
	*msg = xstrdup(buf);
    }

    return rc;
}

/*@-mods@*/
rpmRC rpmReadPackageFile(rpmts ts, void * _fd, const char * fn, Header * hdrp)
{
    pgpDig dig = rpmtsDig(ts);
    FD_t fd = _fd;
    char buf[8*BUFSIZ];
    ssize_t count;
    Header sigh = NULL;
    int_32 sigtag;
    int_32 sigtype;
    const void * sig;
    int_32 siglen;
    rpmtsOpX opx;
    rpmop op = NULL;
    size_t nb;
    Header h = NULL;
    const char * msg = NULL;
    rpmVSFlags vsflags;
    rpmRC rc = RPMRC_FAIL;	/* assume failure */
    rpmop opsave = memset(alloca(sizeof(*opsave)), 0, sizeof(*opsave));
    int xx;
    int i;

    if (hdrp) *hdrp = NULL;

#ifdef	DYING
    {	struct stat st;
	memset(&st, 0, sizeof(st));
	(void) fstat(Fileno(fd), &st);
	/* if fd points to a socket, pipe, etc, st.st_size is *always* zero */
	if (S_ISREG(st.st_mode) && st.st_size < sizeof(*l)) {
	    rc = RPMRC_NOTFOUND;
	    goto exit;
	}
    }
#endif

    /* Snapshot current I/O counters (cached persistent I/O reuses counters) */
    (void) rpmswAdd(opsave, fdstat_op(fd, FDSTAT_READ));

if (!_nolead) {
    const char item[] = "Lead";
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

if (!_nosigh) {
    const char item[] = "Signature";
    msg = NULL;
    rc = rpmpkgRead(item, fd, &sigh, &msg);
    switch (rc) {
    default:
	rpmlog(RPMLOG_ERR, "%s: %s: %s", fn, item,
		(msg && *msg ? msg : "\n"));
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

#define	_chk(_mask)	(sigtag == 0 && !(vsflags & (_mask)))

    /*
     * Figger the most effective available signature.
     * Prefer signatures over digests, then header-only over header+payload.
     * DSA will be preferred over RSA if both exist because tested first.
     * Note that NEEDPAYLOAD prevents header+payload signatures and digests.
     */
    sigtag = 0;
    opx = 0;
    vsflags = pgpGetVSFlags(dig);
    if (_chk(RPMVSF_NODSAHEADER) && headerIsEntry(sigh, RPMSIGTAG_DSA)) {
	sigtag = RPMSIGTAG_DSA;
    } else
    if (_chk(RPMVSF_NORSAHEADER) && headerIsEntry(sigh, RPMSIGTAG_RSA)) {
	sigtag = RPMSIGTAG_RSA;
    } else
#if defined(SUPPORT_RPMV3_VERIFY_DSA)
    if (_chk(RPMVSF_NODSA|RPMVSF_NEEDPAYLOAD) &&
	headerIsEntry(sigh, RPMSIGTAG_GPG))
    {
	sigtag = RPMSIGTAG_GPG;
	fdInitDigest(fd, PGPHASHALGO_SHA1, 0);
	opx = RPMTS_OP_SIGNATURE;
    } else
#endif
#if defined(SUPPORT_RPMV3_VERIFY_RSA)
    if (_chk(RPMVSF_NORSA|RPMVSF_NEEDPAYLOAD) &&
	headerIsEntry(sigh, RPMSIGTAG_PGP))
    {
	sigtag = RPMSIGTAG_PGP;
	fdInitDigest(fd, PGPHASHALGO_MD5, 0);
	opx = RPMTS_OP_SIGNATURE;
    } else
#endif
    if (_chk(RPMVSF_NOSHA1HEADER) && headerIsEntry(sigh, RPMSIGTAG_SHA1)) {
	sigtag = RPMSIGTAG_SHA1;
    } else
    if (_chk(RPMVSF_NOMD5|RPMVSF_NEEDPAYLOAD) &&
	headerIsEntry(sigh, RPMSIGTAG_MD5))
    {
	sigtag = RPMSIGTAG_MD5;
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
    nb = -fd->stats->ops[FDSTAT_READ].bytes;
    rc = rpmReadHeader(ts, fd, &h, &msg);
    nb += fd->stats->ops[FDSTAT_READ].bytes;
/*@=type@*/
    if (opx > 0 && op != NULL) {
	(void) rpmswExit(op, nb);
	op = NULL;
    }

    if (rc != RPMRC_OK || h == NULL) {
	rpmlog(RPMLOG_ERR, _("%s: headerRead failed: %s"), fn,
		(msg && *msg ? msg : "\n"));
	msg = _free(msg);
	goto exit;
    }
    msg = _free(msg);

    /* Any digests or signatures to check? */
    if (sigtag == 0) {
	rc = RPMRC_OK;
	goto exit;
    }

assert(dig != NULL);
    dig->nbytes = 0;

    /* Retrieve the tag parameters from the signature header. */
    sig = NULL;
    xx = headerGetEntry(sigh, sigtag, &sigtype, (void **) &sig, &siglen);
    if (sig == NULL) {
	rc = RPMRC_FAIL;
	goto exit;
    }
/*@-noeffect@*/
    (void) rpmtsSetSig(ts, sigtag, sigtype, sig, siglen);
/*@=noeffect@*/

    switch (sigtag) {
    case RPMSIGTAG_RSA:
	/* Parse the parameters from the OpenPGP packets that will be needed. */
	xx = pgpPrtPkts(sig, siglen, dig, (_print_pkts & rpmIsDebug()));
	if (dig->signature.version != 3 && dig->signature.version != 4) {
	    rpmMessage(RPMMESS_ERROR,
		_("skipping package %s with unverifiable V%u signature\n"),
		fn, dig->signature.version);
	    rc = RPMRC_FAIL;
	    goto exit;
	}
    {	void * uh = NULL;
	int_32 uht;
	int_32 uhc;
	unsigned char * hmagic = NULL;
	size_t nmagic = 0;

	if (!headerGetEntry(h, RPMTAG_HEADERIMMUTABLE, &uht, &uh, &uhc))
	    break;
	(void) headerGetMagic(NULL, &hmagic, &nmagic);
	op = pgpStatsAccumulator(dig, 10);	/* RPMTS_OP_DIGEST */
	(void) rpmswEnter(op, 0);
	dig->hdrmd5ctx = rpmDigestInit(dig->signature.hash_algo, RPMDIGEST_NONE);
	if (hmagic && nmagic > 0) {
	    (void) rpmDigestUpdate(dig->hdrmd5ctx, hmagic, nmagic);
	    dig->nbytes += nmagic;
	}
	(void) rpmDigestUpdate(dig->hdrmd5ctx, uh, uhc);
	dig->nbytes += uhc;
	(void) rpmswExit(op, dig->nbytes);
	op->count--;	/* XXX one too many */
	uh = headerFreeData(uh, uht);
    }	break;
    case RPMSIGTAG_DSA:
	/* Parse the parameters from the OpenPGP packets that will be needed. */
	xx = pgpPrtPkts(sig, siglen, dig, (_print_pkts & rpmIsDebug()));
	if (dig->signature.version != 3 && dig->signature.version != 4) {
	    rpmMessage(RPMMESS_ERROR,
		_("skipping package %s with unverifiable V%u signature\n"), 
		fn, dig->signature.version);
	    rc = RPMRC_FAIL;
	    goto exit;
	}
	/*@fallthrough@*/
    case RPMSIGTAG_SHA1:
    {	void * uh = NULL;
	int_32 uht;
	int_32 uhc;
	unsigned char * hmagic = NULL;
	size_t nmagic = 0;

	if (!headerGetEntry(h, RPMTAG_HEADERIMMUTABLE, &uht, &uh, &uhc))
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
	if (sigtag == RPMSIGTAG_SHA1)
	    op->count--;	/* XXX one too many */
	uh = headerFreeData(uh, uht);
    }	break;
#if defined(SUPPORT_RPMV3_VERIFY_DSA) || defined(SUPPORT_RPMV3_VERIFY_RSA)
#if defined(SUPPORT_RPMV3_VERIFY_DSA)
    case RPMSIGTAG_GPG:
#endif
#if defined(SUPPORT_RPMV3_VERIFY_RSA)
    case RPMSIGTAG_PGP5:	/* XXX legacy */
    case RPMSIGTAG_PGP:
#endif
	/* Parse the parameters from the OpenPGP packets that will be needed. */
	xx = pgpPrtPkts(sig, siglen, dig, (_print_pkts & rpmIsDebug()));

	if (dig->signature.version != 3 && dig->signature.version != 4) {
	    rpmMessage(RPMMESS_ERROR,
		_("skipping package %s with unverifiable V%u signature\n"),
		fn, dig->signature.version);
	    rc = RPMRC_FAIL;
	    goto exit;
	}
	/*@fallthrough@*/
#endif
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
	for (i = fd->ndigests - 1; i >= 0; i--) {
	    FDDIGEST_t fddig = fd->digests + i;
	    if (fddig->hashctx != NULL)
	    switch (fddig->hashalgo) {
	    case PGPHASHALGO_MD5:
		dig->md5ctx = fddig->hashctx;
		fddig->hashctx = NULL;
		/*@switchbreak@*/ break;
	    case PGPHASHALGO_SHA1:
	    case PGPHASHALGO_RIPEMD160:
#if defined(HAVE_BEECRYPT_API_H)
	    case PGPHASHALGO_SHA256:
	    case PGPHASHALGO_SHA384:
	    case PGPHASHALGO_SHA512:
#endif
		dig->sha1ctx = fddig->hashctx;
		fddig->hashctx = NULL;
		/*@switchbreak@*/ break;
	    default:
		/*@switchbreak@*/ break;
	    }
	}
	break;
    }

/** @todo Implement disable/enable/warn/error/anal policy. */

    buf[0] = '\0';
    rc = rpmVerifySignature(dig, buf);
    switch (rc) {
    case RPMRC_OK:		/* Signature is OK. */
	rpmMessage(RPMMESS_DEBUG, "%s: %s", fn, buf);
	break;
    case RPMRC_NOTTRUSTED:	/* Signature is OK, but key is not trusted. */
    case RPMRC_NOKEY:		/* Public key is unavailable. */
	/* XXX Print NOKEY/NOTTRUSTED warning only once. */
    {	int lvl = (pgpStashKeyid(dig) ? RPMMESS_DEBUG : RPMMESS_WARNING);
	rpmMessage(lvl, "%s: %s", fn, buf);
    }	break;
    case RPMRC_NOTFOUND:	/* Signature is unknown type. */
	rpmMessage(RPMMESS_WARNING, "%s: %s", fn, buf);
	break;
    default:
    case RPMRC_FAIL:		/* Signature does not verify. */
	rpmMessage(RPMMESS_ERROR, "%s: %s", fn, buf);
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

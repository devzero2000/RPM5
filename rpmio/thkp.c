#include "system.h"

#include <rpmio.h>
#include <rpmdir.h>
#include <rpmdav.h>
#include <rpmmacro.h>
#include <rpmcb.h>
#include <popt.h>

#define	_RPMPGP_INTERNAL
#include <rpmpgp.h>

#include "debug.h"

static int _debug = 0;
static int _printing = 0;

int noNeon;

static int _spew;
#define	SPEW(_list)	if (_spew) fprintf _list

static unsigned int keyids[] = {
	0xc2b079fc, 0xf5c75256,
	0x94cd5742, 0xe418e3aa,
	0xb44269d0, 0x4f2a6fd2,
	0xda84cbd4, 0x30c9ecf8,
	0x29d5ba24, 0x8df56d05,
#if 0	/* XXX V2 revocations (and V3 RSA) live here */
	0xa520e8f1, 0xcba29bf9,
#endif
	0x219180cd, 0xdb42a60e,
	0xfd372689, 0x897da07a,
	0xe1385d4e, 0x1cddbca9,
	0xb873641b, 0x2039b291,
	0x58e727c4, 0xc621be0f,
	0x6bddfe8e, 0x54a2acf1,
	0xb873641b, 0x2039b291,
/* --- RHEL6 */
	0x00000000, 0x2fa658e0,
	0x00000000, 0x37017186,
	0x00000000, 0x42193e6b,
	0x00000000, 0x897da07a,
	0x00000000, 0xdb42a60e,
	0x00000000, 0xf21541eb,
	0x00000000, 0xfd431d51,
/* --- Fedorable */
	0x00000000, 0xd22e77f2,	/* F11 */
	0x00000000, 0x57bbccba,	/* F12 */
	0x00000000, 0xe8e40fde,	/* F13 */
	0,0
};

/*==============================================================*/

static const char * _pgpSigType2Name(uint32_t sigtype)
{
    return pgpValStr(pgpSigTypeTbl, (rpmuint8_t)sigtype);
}

static const char * _pgpHashAlgo2Name(uint32_t algo)
{
    return pgpValStr(pgpHashTbl, (rpmuint8_t)algo);
}

static const char * _pgpPubkeyAlgo2Name(uint32_t algo)
{
    return pgpValStr(pgpPubkeyTbl, (rpmuint8_t)algo);
}

struct pgpPkt_s {
    pgpTag tag;
    unsigned int pktlen;
    const rpmuint8_t * h;
    unsigned int hlen;
};

static const rpmuint8_t * pgpGrabSubTagVal(const rpmuint8_t * h, size_t hlen,
		rpmuint8_t subtag, /*@null@*/ size_t * tlenp)
{
    const rpmuint8_t * p = h;
    const rpmuint8_t * pend = h + hlen;
    unsigned plen = 0;
    unsigned len;
    rpmuint8_t stag;

    if (tlenp)
	*tlenp = 0;

    while (hlen > 0) {
	len = pgpLen(p, &plen);
	p += len;
	hlen -= len;

	stag = (*p & ~PGPSUBTYPE_CRITICAL);

	if (stag == subtag) {
SPEW((stderr, "\tSUBTAG %02X %p[%2u]\t%s\n", stag, p+1, plen-1, pgpHexStr(p+1, plen-1)));
	    if (tlenp)
		*tlenp = plen-1;
	    return p+1;
	}

	p += plen;
assert(p < pend);
	hlen -= plen;
    }
    return NULL;
}

/*==============================================================*/
typedef struct rpmhkp_s * rpmhkp;

struct rpmhkp_s {

    rpmuint8_t * pkt;
    size_t pktlen;
    rpmuint8_t ** pkts;
    int npkts;

    int pubx;
    int uidx;
    int subx;
    int sigx;

    rpmuint8_t keyid[8];
    rpmuint8_t subid[8];
    rpmuint8_t goop[6];

};

static rpmhkp rpmhkpFree(rpmhkp hkp)
{
    if (hkp) {
	hkp->pkt = _free(hkp->pkt);
	hkp->pktlen = 0;
	hkp->pkts = _free(hkp->pkts);
	hkp->npkts = 0;
	hkp = _free(hkp);
    }
    return NULL;
}

static rpmhkp rpmhkpNew(const rpmuint8_t * keyid)
{
    rpmhkp hkp = xcalloc(1, sizeof(*hkp));

    if (keyid)
	memcpy(hkp->keyid, keyid, sizeof(hkp->keyid));

hkp->pubx = -1;
hkp->uidx = -1;
hkp->subx = -1;
hkp->sigx = -1;

    return hkp;
}

static rpmhkp rpmhkpLookup(const rpmuint8_t * keyid)
{
    static char _uri[] = "hkp://keys.rpm5.org";
    static char _path[] = "/pks/lookup?op=get&search=0x";
    rpmhkp hkp = NULL;
    const char * fn;
    pgpArmor pa;
    int rc = 1;	/* assume failure */

    if (keyid[0] || keyid[1] || keyid[2] || keyid[3])
	fn = rpmExpand(_uri, _path, pgpHexStr(keyid, 8), NULL);
    else
	fn = rpmExpand(_uri, _path, pgpHexStr(keyid+4, 4), NULL);

    hkp = rpmhkpNew(keyid);

    pa = pgpReadPkts(fn, &hkp->pkt, &hkp->pktlen);
    if (pa == PGPARMOR_ERROR || pa == PGPARMOR_NONE
     || hkp->pkt == NULL || hkp->pktlen == 0)
	goto exit;

    rc = pgpGrabPkts(hkp->pkt, hkp->pktlen, &hkp->pkts, &hkp->npkts);

exit:
    if (rc && hkp)
	hkp = rpmhkpFree(hkp);
    fn = _free(fn);
    return hkp;
}

static int rpmhkpLoadKey(rpmhkp hkp, pgpDig dig,
		int keyx, pgpPubkeyAlgo pubkey_algo)
{
    pgpPkt pp = alloca(sizeof(*pp));
    size_t pleft = hkp->pktlen - (hkp->pkts[keyx] - hkp->pkt);
    int len = pgpPktLen(hkp->pkts[keyx], pleft, pp);
    const rpmuint8_t * p;
len = len;

    if (pp->h[0] == 3) {
	pgpPktKeyV3 v = (pgpPktKeyV3)pp->h;
	p = ((rpmuint8_t *)v) + sizeof(*v);
	p = pgpPrtPubkeyParams(dig, pp, pubkey_algo, p);
    }

    if (pp->h[0] == 4) {
	pgpPktKeyV4 v = (pgpPktKeyV4)pp->h;
	p = ((rpmuint8_t *)v) + sizeof(*v);
	p = pgpPrtPubkeyParams(dig, pp, pubkey_algo, p);
    }

    return 0;
}

static int rpmhkpFindKey(rpmhkp hkp, pgpDig dig,
		const rpmuint8_t * signid,
		pgpPubkeyAlgo pubkey_algo)
{
    int ix = -1;	/* assume failure */

    if (hkp->pubx >= 0 && hkp->pubx < hkp->npkts
     && !memcmp(hkp->keyid, signid, sizeof(hkp->keyid)))
	return hkp->pubx;

    if (hkp->subx >= 0 && hkp->subx < hkp->npkts
     && !memcmp(hkp->subid, signid, sizeof(hkp->subid)))
	return hkp->subx;

    {	rpmhkp ohkp = rpmhkpLookup(signid);
	if (ohkp == NULL) {
fprintf(stderr, "\tAWOL\n");
	    ix = -2;
	}
	ohkp = rpmhkpFree(ohkp);
    }

    return ix;
}

static int rpmhkpLoadSignature(rpmhkp hkp, pgpDig dig, pgpPkt pp)
{
    pgpDigParams sigp = pgpGetSignature(dig);
    const rpmuint8_t * p = NULL;

    sigp->version = *pp->h;

    if (sigp->version == 3) {
	pgpPktSigV3 v = (pgpPktSigV3)pp->h;

	sigp->pubkey_algo = v->pubkey_algo;
	sigp->hash_algo = v->hash_algo;
	sigp->sigtype = v->sigtype;
	memcpy(sigp->time, v->time, sizeof(sigp->time));
	sigp->hash = pp->h;
	sigp->hashlen = (size_t)v->hashlen;
	memcpy(sigp->signid, v->signid, sizeof(sigp->signid));
	memcpy(sigp->signhash16, v->signhash16, sizeof(sigp->signhash16));

/* XXX set pointer to signature parameters. */
p = ((rpmuint8_t *)v) + sizeof(*v);

    }

    if (sigp->version == 4) {
	pgpPktSigV4 v = (pgpPktSigV4)pp->h;
	const rpmuint8_t * phash;
	size_t nhash;
	const rpmuint8_t * punhash;
	size_t nunhash;
	size_t tlen;

	sigp->pubkey_algo = v->pubkey_algo;
	sigp->hash_algo = v->hash_algo;
	sigp->sigtype = v->sigtype;

	phash = pp->h + sizeof(*v);
	nhash = pgpGrab(v->hashlen, sizeof(v->hashlen));
	sigp->hash = pp->h;
	sigp->hashlen = sizeof(*v) + nhash;

	nunhash = pgpGrab(phash+nhash, 2);
	punhash = phash + nhash + 2;
	memcpy(sigp->signhash16, punhash+nunhash, sizeof(sigp->signhash16));

	tlen = 0;
	p = pgpGrabSubTagVal(phash, nhash, PGPSUBTYPE_SIG_CREATE_TIME, &tlen);
	memcpy(sigp->time, p, tlen);

	tlen = 0;
	p = pgpGrabSubTagVal(punhash, nunhash, PGPSUBTYPE_ISSUER_KEYID, &tlen);
	memcpy(sigp->signid, p, sizeof(sigp->signid));

/* XXX set pointer to signature parameters. */
p = punhash + nunhash + 2;

    }

    /* XXX Load signature paramaters. */
    pgpPrtSigParams(dig, pp, sigp->pubkey_algo, sigp->sigtype, p);

    return 0;
}

static int rpmhkpUpdate(/*@null@*/DIGEST_CTX ctx, const void * data, size_t len)
{
    int rc = rpmDigestUpdate(ctx, data, len);
SPEW((stderr, "*** Update(%3d): %s\n", 3, pgpHexStr(data, len)));
    return rc;
}

static DIGEST_CTX rpmhkpHashKey(rpmhkp hkp, int ix, pgpHashAlgo dalgo)
{
    DIGEST_CTX ctx = rpmDigestInit(dalgo, RPMDIGEST_NONE);
pgpPkt pp = alloca(sizeof(*pp));
assert(ix >= 0 && ix < hkp->npkts);
#ifdef NOTYET
assert(*hkp->pkts[ix] == 0x99);
#else
if (*hkp->pkts[ix] != 0x99) fprintf(stderr, "*** %s: %02X\n", __FUNCTION__, *hkp->pkts[ix]);
#endif
(void) pgpPktLen(hkp->pkts[ix], hkp->pktlen, pp);

    hkp->goop[0] = *hkp->pkts[ix];
    hkp->goop[1] = (pp->hlen >>  8) & 0xff;
    hkp->goop[2] = (pp->hlen      ) & 0xff;
    rpmhkpUpdate(ctx, hkp->goop, 3);
    rpmhkpUpdate(ctx, pp->h, pp->hlen);
    return ctx;
}

static DIGEST_CTX rpmhkpHashUid(rpmhkp hkp, int ix, pgpHashAlgo dalgo)
{
    DIGEST_CTX ctx = rpmhkpHashKey(hkp, hkp->pubx, dalgo);
pgpPkt pp = alloca(sizeof(*pp));
assert(ix > 0 && ix < hkp->npkts);
#ifdef NOTYET
assert(*hkp->pkts[ix] == 0xb4 || *hkp->pkts[ix] == 0xd1);
#else
if (*hkp->pkts[ix] != 0xb4) fprintf(stderr, "*** %s: %02X\n", __FUNCTION__, *hkp->pkts[ix]);
#endif
(void) pgpPktLen(hkp->pkts[ix], hkp->pktlen, pp);

    hkp->goop[0] = *hkp->pkts[ix];
    hkp->goop[1] = (pp->hlen >> 24) & 0xff;
    hkp->goop[2] = (pp->hlen >> 16) & 0xff;
    hkp->goop[3] = (pp->hlen >>  8) & 0xff;
    hkp->goop[4] = (pp->hlen      ) & 0xff;
    rpmhkpUpdate(ctx, hkp->goop, 5);
    rpmhkpUpdate(ctx, pp->h, pp->hlen);
    return ctx;
}

static DIGEST_CTX rpmhkpHashSubkey(rpmhkp hkp, int ix, pgpHashAlgo dalgo)
{
    DIGEST_CTX ctx = rpmhkpHashKey(hkp, hkp->pubx, dalgo);
pgpPkt pp = alloca(sizeof(*pp));
assert(ix > 0 && ix < hkp->npkts);
#ifdef NOTYET
assert(*hkp->pkts[ix] == 0xb9);
#else
if (*hkp->pkts[ix] != 0xb9) fprintf(stderr, "*** %s: %02X\n", __FUNCTION__, *hkp->pkts[ix]);
#endif
(void) pgpPktLen(hkp->pkts[ix], hkp->pktlen, pp);

    hkp->goop[0] = 0x99;
    hkp->goop[1] = (pp->hlen >>  8) & 0xff;
    hkp->goop[2] = (pp->hlen      ) & 0xff;
    rpmhkpUpdate(ctx, hkp->goop, 3);
    rpmhkpUpdate(ctx, pp->h, pp->hlen);
    return ctx;
}

static DIGEST_CTX rpmhkpHash(rpmhkp hkp, int keyx,
		pgpSigType sigtype, pgpHashAlgo dalgo)
{
    DIGEST_CTX ctx = NULL;

    switch (sigtype) {
    case PGPSIGTYPE_BINARY:
    case PGPSIGTYPE_TEXT:
    case PGPSIGTYPE_STANDALONE:
    default:
	break;
    case PGPSIGTYPE_GENERIC_CERT:
    case PGPSIGTYPE_PERSONA_CERT:
    case PGPSIGTYPE_CASUAL_CERT:
    case PGPSIGTYPE_POSITIVE_CERT:
	if (hkp->pubx >= 0 && hkp->uidx >= 0)
	    ctx = rpmhkpHashUid(hkp, hkp->uidx, dalgo);
	break;
    case PGPSIGTYPE_SUBKEY_BINDING:
	if (hkp->pubx >= 0 && hkp->subx >= 0)
	    ctx = rpmhkpHashSubkey(hkp, hkp->subx, dalgo);
	break;
    case PGPSIGTYPE_KEY_BINDING:
	if (hkp->pubx >= 0)
	    ctx = rpmhkpHashSubkey(hkp, hkp->pubx, dalgo);
	break;
    case PGPSIGTYPE_SIGNED_KEY:
	/* XXX search for signid amongst the packets? */
	break;
    case PGPSIGTYPE_KEY_REVOKE:
	/* XXX authorized revocation key too. */
	if (hkp->pubx >= 0 && hkp->pubx == keyx)
	    ctx = rpmhkpHashKey(hkp, hkp->pubx, dalgo);
	break;
    case PGPSIGTYPE_SUBKEY_REVOKE:
	/* XXX authorized revocation key too. */
	if (hkp->pubx >= 0 && hkp->pubx == keyx && hkp->subx >= 0)
	    ctx = rpmhkpHashKey(hkp, hkp->subx, dalgo);
	break;
    case PGPSIGTYPE_CERT_REVOKE:
    case PGPSIGTYPE_TIMESTAMP:
    case PGPSIGTYPE_CONFIRM:
	break;
    }
    return ctx;
}

#ifdef	DYING
static void dumpDigParams(const char * msg, pgpDigParams sigp)
{
fprintf(stderr, "%s: %p\n", msg, sigp);
fprintf(stderr, "\t     userid: %s\n", sigp->userid);
fprintf(stderr, "\t       hash: %p[%u]\n", sigp->hash, sigp->hashlen);
fprintf(stderr, "\t        tag: %02X\n", sigp->tag);
fprintf(stderr, "\t    version: %02X\n", sigp->version);
fprintf(stderr, "\t       time: %08X\n", pgpGrab(sigp->time, sizeof(sigp->time)));
fprintf(stderr, "\tpubkey_algo: %02X\n", sigp->pubkey_algo);
fprintf(stderr, "\t  hash_algo: %02X\n", sigp->hash_algo);
fprintf(stderr, "\t    sigtype: %02X\n", sigp->sigtype);
fprintf(stderr, "\t signhash16: %04X\n", pgpGrab(sigp->signhash16, sizeof(sigp->signhash16)));
fprintf(stderr, "\t     signid: %08X %08X\n", pgpGrab(sigp->signid+4, 4), pgpGrab(sigp->signid, 4));
fprintf(stderr, "\t      saved: %02X\n", sigp->saved);
}

static void dumpDig(const char * msg, pgpDig dig)
{
fprintf(stderr, "%s: dig %p\n", msg, dig);

fprintf(stderr, "\t    sigtag: 0x%08x\n", dig->sigtag);
fprintf(stderr, "\t   sigtype: 0x%08x\n", dig->sigtype);
fprintf(stderr, "\t       sig: %p[%u]\n", dig->sig, dig->siglen);
fprintf(stderr, "\t   vsflags: 0x%08x\n", dig->vsflags);
fprintf(stderr, "\tfindPubkey: %p\n", dig->findPubkey);
fprintf(stderr, "\t       _ts: %p\n", dig->_ts);
fprintf(stderr, "\t     ppkts: %p[%u]\n", dig->ppkts, dig->npkts);
fprintf(stderr, "\t    nbytes: 0x%08x\n", dig->nbytes);

fprintf(stderr, "\t   sha1ctx: %p\n", dig->sha1ctx);
fprintf(stderr, "\thdrsha1ctx: %p\n", dig->hdrsha1ctx);
fprintf(stderr, "\t      sha1: %p[%u]\n", dig->sha1, dig->sha1len);

fprintf(stderr, "\t    md5ctx: %p\n", dig->md5ctx);
fprintf(stderr, "\t    hdrctx: %p\n", dig->hdrctx);
fprintf(stderr, "\t       md5: %p[%u]\n", dig->md5, dig->md5len);
fprintf(stderr, "\t      impl: %p\n", dig->impl);

dumpDigParams("PUB", pgpGetPubkey(dig));
dumpDigParams("SIG", pgpGetSignature(dig));
}
#endif

static int rpmhkpVerifyHash(rpmhkp hkp, pgpDig dig, DIGEST_CTX ctx)
{
    pgpDigParams sigp = pgpGetSignature(dig);
    const char * dname = xstrdup(rpmDigestName(ctx));
    rpmuint8_t * digest = NULL;
    size_t digestlen = 0;
    int rc = rpmDigestFinal(ctx, digest, &digestlen, 0);

    rc = memcmp(sigp->signhash16, digest, sizeof(sigp->signhash16));

if (rc)
fprintf(stderr, "\t%s\t%s\n", dname, pgpHexStr(digest, digestlen));
fprintf(stderr, "%s\t%s\n", (!rc ? "\tGOOD" : "------> BAD"), pgpHexStr(sigp->signhash16, sizeof(sigp->signhash16)));

    digest = _free(digest);
    digestlen = 0;
    dname = _free(dname);

    return rc;
}

static int rpmhkpVerifySignature(rpmhkp hkp, pgpDig dig, DIGEST_CTX ctx)
{
    pgpDigParams sigp = pgpGetSignature(dig);
    int rc = 0;		/* XXX assume failure */

    switch (sigp->pubkey_algo) {

    case PGPPUBKEYALGO_DSA:
	if (pgpImplSetDSA(ctx, dig, sigp))
fprintf(stderr, "------> BAD\t%s\n", pgpHexStr(sigp->signhash16, 2));
	else if (!pgpImplVerifyDSA(dig))
fprintf(stderr, "------> BAD\tV%u %s-%s\n",
		sigp->version,
		_pgpPubkeyAlgo2Name(sigp->pubkey_algo),
		_pgpHashAlgo2Name(sigp->hash_algo));
	else {
fprintf(stderr, "\tGOOD\tV%u %s-%s\n",
		sigp->version,
		_pgpPubkeyAlgo2Name(sigp->pubkey_algo),
		_pgpHashAlgo2Name(sigp->hash_algo));
	    rc = 1;
	}
	break;
    case PGPPUBKEYALGO_RSA:
	if (pgpImplSetRSA(ctx, dig, sigp))
fprintf(stderr, "------> BAD\t%s\n", pgpHexStr(sigp->signhash16, 2));
	else if (!pgpImplVerifyRSA(dig))
fprintf(stderr, "------> BAD\tV%u %s-%s\n",
		sigp->version,
		_pgpPubkeyAlgo2Name(sigp->pubkey_algo),
		_pgpHashAlgo2Name(sigp->hash_algo));
	else {
fprintf(stderr, "\tGOOD\tV%u %s-%s\n",
		sigp->version,
		_pgpPubkeyAlgo2Name(sigp->pubkey_algo),
		_pgpHashAlgo2Name(sigp->hash_algo));
	    rc = 1;
	}
	break;
    }

    return rc;
}

static int rpmhkpVerify(rpmhkp hkp, pgpPkt pp)
{
    DIGEST_CTX ctx = NULL;
    int keyx;
    int rc = 1;		/* assume failure */

pgpDig dig = pgpDigNew(0);
pgpDigParams sigp = pgpGetSignature(dig);
int xx;

    /* XXX Load signature paramaters. */
    xx = rpmhkpLoadSignature(hkp, dig, pp);

    fprintf(stderr, "  SIG: %08X %08X V%u %s-%s %s\n",
		pgpGrab(sigp->signid, 4), pgpGrab(sigp->signid+4, 4),
		sigp->version,
		_pgpPubkeyAlgo2Name(sigp->pubkey_algo),
		_pgpHashAlgo2Name(sigp->hash_algo),
		_pgpSigType2Name(sigp->sigtype)
    );

    /* XXX don't fuss V3 for now. */
    if (sigp->version == 3)	 goto exit;

    /* XXX handle only RSA/DSA. */
    if (!(sigp->pubkey_algo == PGPPUBKEYALGO_DSA || sigp->pubkey_algo == PGPPUBKEYALGO_RSA)) goto exit;

    keyx = rpmhkpFindKey(hkp, dig, sigp->signid, sigp->pubkey_algo);

    /* XXX handle only self-signed digital signatures. */
    if (keyx < 0) goto exit;

    /* XXX Load pubkey paramaters. */
    xx = rpmhkpLoadKey(hkp, dig, keyx, sigp->pubkey_algo);

    ctx = rpmhkpHash(hkp, keyx, sigp->sigtype, sigp->hash_algo);

    if (ctx) {

	/* XXX something fishy here with V3 signatures. */
	if (sigp->hash)
	    rpmhkpUpdate(ctx, sigp->hash, sigp->hashlen);

	if (sigp->version == 4) {
	    hkp->goop[0] = sigp->version;
	    hkp->goop[1] = (rpmuint8_t)0xff;
	    hkp->goop[2] = (sigp->hashlen >> 24) & 0xff;
	    hkp->goop[3] = (sigp->hashlen >> 16) & 0xff;
	    hkp->goop[4] = (sigp->hashlen >>  8) & 0xff;
	    hkp->goop[5] = (sigp->hashlen      ) & 0xff;
	    rpmhkpUpdate(ctx, hkp->goop, 6);
	}

	if (keyx >= 0) {
	    rc = rpmhkpVerifySignature(hkp, dig, ctx);
	} else {
	    rc = rpmhkpVerifyHash(hkp, dig, ctx);
	}
	ctx = NULL;
    }

exit:
dig = pgpDigFree(dig);

    return rc;	/* XXX 1 on success */
}

static int rpmhkpValidate(rpmhkp hkp)
{
pgpPkt pp = alloca(sizeof(*pp));
size_t pleft;
    int rc = 1;		/* assume failure */
    int xx;
    int i;

    pleft = hkp->pktlen;
    if (hkp->pkts)
    for (i = 0; i < hkp->npkts; i++) {
	xx = pgpPktLen(hkp->pkts[i], pleft, pp);
	pleft -= pp->pktlen;
SPEW((stderr, "%6d %p[%3u] %02X %s\n", i, hkp->pkts[i], (unsigned)pp->pktlen, *hkp->pkts[i], pgpValStr(pgpTagTbl, (rpmuint8_t)pp->tag)));
SPEW((stderr, "\t%s\n", pgpHexStr(hkp->pkts[i], pp->pktlen)));

	switch (pp->tag) {
	default:
	    break;
	case PGPTAG_PUBLIC_KEY:
	    hkp->pubx = i;
{
xx = pgpPubkeyFingerprint(hkp->pkts[i], pp->pktlen, hkp->keyid);
fprintf(stderr, "  PUB: %08X %08X", pgpGrab(hkp->keyid, 4), pgpGrab(hkp->keyid+4, 4));
if (*pp->h == 3) {
    pgpPktKeyV3 v = (pgpPktKeyV3)pp->h;
fprintf(stderr, " V%u %s", v->version, _pgpPubkeyAlgo2Name(v->pubkey_algo));
    if (v->valid[0] || v->valid[1]) {
	rpmuint32_t days = pgpGrab(v->valid, sizeof(v->valid));
	time_t expired = pgpGrab(v->time, 4) + (24 * 60 * 60 * days);
	if (expired < time(NULL))
	    fprintf(stderr, " EXPIRED");
    }
}
if (*pp->h == 4) {
    pgpPktKeyV4 v = (pgpPktKeyV4)pp->h;
fprintf(stderr, " V%u %s", v->version, _pgpPubkeyAlgo2Name(v->pubkey_algo));
    /* XXX check expiry */
}
fprintf(stderr, "\n");
}

	    break;
	case PGPTAG_USER_ID:
	    hkp->uidx = i;
{
pgpPktUid * u = (pgpPktUid *)pp->h;
fprintf(stderr, "  UID: %.*s\n", pp->hlen, u->userid);
}

	    break;
	case PGPTAG_PUBLIC_SUBKEY:
	    hkp->subx = i;
{
xx = pgpPubkeyFingerprint(hkp->pkts[i], pp->pktlen, hkp->subid);
fprintf(stderr, "  SUB: %08X %08X", pgpGrab(hkp->keyid, 4), pgpGrab(hkp->keyid+4, 4));
if (*pp->h == 3) {
    pgpPktKeyV3 v = (pgpPktKeyV3)pp->h;
fprintf(stderr, " V%u %s", v->version, _pgpPubkeyAlgo2Name(v->pubkey_algo));
    if (v->valid[0] || v->valid[1]) {
	rpmuint32_t days = pgpGrab(v->valid, sizeof(v->valid));
	time_t expired = pgpGrab(v->time, 4) + (24 * 60 * 60 * days);
	if (expired < time(NULL))
	    fprintf(stderr, " EXPIRED");
    }
}
if (*pp->h == 4) {
    pgpPktKeyV4 v = (pgpPktKeyV4)pp->h;
fprintf(stderr, " V%u %s", v->version, _pgpPubkeyAlgo2Name(v->pubkey_algo));
    /* XXX check expiry */
}
fprintf(stderr, "\n");
}

	    break;
	case PGPTAG_SIGNATURE:
	    hkp->sigx = i;
	    if (!(*pp->h == 3 || *pp->h == 4)) {
fprintf(stderr, "  SIG: V%u\n", *pp->h);
fprintf(stderr, "\tSKIP(V%u != V3 | V4)\t%s\n", *pp->h, pgpHexStr(pp->h, pp->pktlen));
		break;
	    }
	    xx = rpmhkpVerify(hkp, pp);		/* XXX 1 on success */
	    break;
	}
    }
    rc = 0;

    return rc;
}

static int rpmhkpReadKeys(void)
{
    rpmhkp hkp;
    unsigned int * kip;
    rpmuint8_t keyid[8];
    int rc;
    int ec = 0;

    for (kip = keyids; kip[1]; kip += 2) {

	keyid[0] = (kip[0] >> 24);
	keyid[1] = (kip[0] >> 16);
	keyid[2] = (kip[0] >>  8);
	keyid[3] = (kip[0]      );
	keyid[4] = (kip[1] >> 24);
	keyid[5] = (kip[1] >> 16);
	keyid[6] = (kip[1] >>  8);
	keyid[7] = (kip[1]      );

fprintf(stderr, "===============\n");
	hkp = rpmhkpLookup(keyid);
	if (hkp == NULL) {
	    ec++;
	    continue;
	}

	rc = rpmhkpValidate(hkp);
	if (rc)
	    ec++;

	hkp = rpmhkpFree(hkp);

    }

    return ec;
}

static struct poptOption optionsTable[] = {
 { "print", 'p', POPT_ARG_VAL,  &_printing, 1,		NULL, NULL },
 { "noprint", 'n', POPT_ARG_VAL, &_printing, 0,		NULL, NULL },
 { "debug", 'd', POPT_ARG_VAL,	&_debug, -1,		NULL, NULL },
 { "spew", '\0', POPT_ARG_VAL,	&_spew, -1,		NULL, NULL },
 { "davdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_dav_debug, -1,
	N_("debug protocol data stream"), NULL},
 { "ftpdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_ftp_debug, -1,
	N_("debug protocol data stream"), NULL},
 { "noneon", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &noNeon, 1,
	N_("disable use of libneon for HTTP"), NULL},
 { "rpmiodebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmio_debug, -1,
	N_("debug rpmio I/O"), NULL},
 { "urldebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_url_debug, -1,
	N_("debug URL cache handling"), NULL},
 { "verbose", 'v', 0, 0, 'v',				NULL, NULL },
  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    poptContext optCon = poptGetContext(argv[0], argc, (const char **)argv, optionsTable, 0);
    int rc;

    while ((rc = poptGetNextOpt(optCon)) > 0) {
	switch (rc) {
	case 'v':
	    rpmIncreaseVerbosity();
	    /*@switchbreak@*/ break;
	default:
            /*@switchbreak@*/ break;
	}
    }

    if (_debug) {
	rpmIncreaseVerbosity();
	rpmIncreaseVerbosity();
    }

    rpmhkpReadKeys();

/*@i@*/ urlFreeCache();

    return 0;
}

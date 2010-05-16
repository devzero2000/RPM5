#include "system.h"

#include <rpmio.h>
#include <rpmbf.h>
#include <rpmdir.h>
#include <rpmdav.h>
#include <rpmmacro.h>
#include <rpmcb.h>

#define	_RPMPGP_INTERNAL
#include <rpmpgp.h>

#include <poptIO.h>

#include "debug.h"

static int _debug = 0;
static int _printing = 0;

int noNeon;

static rpmbf awol;
static size_t awol_n = 100000;
static double awol_e = 1.0e-4;
static size_t awol_m;
static size_t awol_k;

typedef struct _Astats_s {
    size_t good;
    size_t bad;
} _Astats;

typedef struct _BAstats_s {
    _Astats DSA;
    _Astats RSA;
    _Astats HASH;
    _Astats AWOL;
    _Astats SKIP;
    size_t count;
} _BAstats;

static _BAstats SUM;

static int _spew;
#define	SPEW(_list)	if (_spew) fprintf _list

static uint64_t keyids[] = {
	0x87CC9EC7F9033421ULL,	/* XXX rpmhkpHashKey: 98 */
	0xc2b079fcf5c75256ULL,
	0x94cd5742e418e3aaULL,
	0xb44269d04f2a6fd2ULL,
	0xda84cbd430c9ecf8ULL,
	0x29d5ba248df56d05ULL,
	0xa520e8f1cba29bf9ULL, /* V2 revocations (and V3 RSA) */
	0x219180cddb42a60eULL,
	0xfd372689897da07aULL,
	0xe1385d4e1cddbca9ULL,
	0x58e727c4c621be0fULL,
	0x6bddfe8e54a2acf1ULL,
	0xb873641b2039b291ULL,
/* --- RHEL6 */
	0x2fa658e0,
	0x37017186,
	0x42193e6b,
	0x897da07a,
	0xdb42a60e,
	0xf21541eb,
	0xfd431d51,
/* --- Fedorable */
	0xd22e77f2,	/* F11 */
	0x57bbccba,	/* F12 */
	0xe8e40fde,	/* F13 */
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

    rpmbf awol;

};

static rpmhkp rpmhkpFree(rpmhkp hkp)
{
    if (hkp) {
	hkp->pkt = _free(hkp->pkt);
	hkp->pktlen = 0;
	hkp->pkts = _free(hkp->pkts);
	hkp->npkts = 0;
	hkp->awol = rpmbfFree(hkp->awol);
	hkp = _free(hkp);
    }
    return NULL;
}

static rpmhkp rpmhkpNew(const rpmuint8_t * keyid)
{
    rpmhkp hkp = xcalloc(1, sizeof(*hkp));

    if (keyid)
	memcpy(hkp->keyid, keyid, sizeof(hkp->keyid));

    if (awol)
	hkp->awol = rpmbfLink(awol);

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
    /* XXX "best effort" use primary key if keyx is bogus */
    int ix = (keyx >= 0 && keyx < hkp->npkts) ? keyx : 0;
    size_t pleft = hkp->pktlen - (hkp->pkts[ix] - hkp->pkt);
    int len = pgpPktLen(hkp->pkts[ix], pleft, pp);
    const rpmuint8_t * p;
    int rc = 0;	/* assume success */
len = len;

    if (pp->h[0] == 3) {
	pgpPktKeyV3 v = (pgpPktKeyV3)pp->h;
	p = ((rpmuint8_t *)v) + sizeof(*v);
	p = pgpPrtPubkeyParams(dig, pp, pubkey_algo, p);
    } else
    if (pp->h[0] == 4) {
	pgpPktKeyV4 v = (pgpPktKeyV4)pp->h;
	p = ((rpmuint8_t *)v) + sizeof(*v);
	p = pgpPrtPubkeyParams(dig, pp, pubkey_algo, p);
    } else
	rc = -1;

    return rc;
}

static int rpmhkpFindKey(rpmhkp hkp, pgpDig dig,
		const rpmuint8_t * signid,
		pgpPubkeyAlgo pubkey_algo)
{
    pgpDigParams sigp = pgpGetSignature(dig);
    int keyx = -1;	/* assume notfound (in this cert) */
int xx;

    if (hkp->pubx >= 0 && hkp->pubx < hkp->npkts
     && !memcmp(hkp->keyid, signid, sizeof(hkp->keyid))) {
	if (!rpmhkpLoadKey(hkp, dig, hkp->pubx, sigp->pubkey_algo))
	    keyx = hkp->pubx;
	goto exit;
    }

    if (hkp->subx >= 0 && hkp->subx < hkp->npkts
     && !memcmp(hkp->subid, signid, sizeof(hkp->subid))) {
	if (!rpmhkpLoadKey(hkp, dig, hkp->subx, sigp->pubkey_algo))
	    keyx = hkp->subx;
	goto exit;
    }

    if (hkp->awol && rpmbfChk(hkp->awol, signid, 8)) {
	keyx = -2;
	SUM.AWOL.good++;
	goto exit;
    }

    {	rpmhkp ohkp = rpmhkpLookup(signid);
	if (ohkp == NULL) {
	    xx = rpmbfAdd(hkp->awol, signid, 8);
SPEW((stderr, "\tAWOL\n"));
	    SUM.AWOL.bad++;
	    keyx = -2;
	    goto exit;
	}
	if (rpmhkpLoadKey(ohkp, dig, 0, sigp->pubkey_algo))
	    keyx = -2;		/* XXX skip V2 certs */
	ohkp = rpmhkpFree(ohkp);
    }

exit:
    return keyx;
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
switch (*hkp->pkts[ix]) {
default: fprintf(stderr, "*** %s: %02X\n", __FUNCTION__, *hkp->pkts[ix]);
case 0x99: case 0x98: case 0xb9: break;
}
#endif
(void) pgpPktLen(hkp->pkts[ix], hkp->pktlen, pp);

    hkp->goop[0] = 0x99;	/* XXX correct for revocation? */
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
switch (*hkp->pkts[ix]) {
default: fprintf(stderr, "*** %s: %02X\n", __FUNCTION__, *hkp->pkts[ix]);
case 0xb4: break;
}
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
switch (*hkp->pkts[ix]) {
default: fprintf(stderr, "*** %s: %02X\n", __FUNCTION__, *hkp->pkts[ix]);
case 0xb9: break;
}
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
	/* XXX only primary key */
	/* XXX authorized revocation key too. */
	if (hkp->pubx >= 0)
	    ctx = rpmhkpHashKey(hkp, hkp->pubx, dalgo);
	break;
    case PGPSIGTYPE_SUBKEY_REVOKE:
	/* XXX only primary key */
	/* XXX authorized revocation key too. */
	if (hkp->pubx >= 0 && hkp->subx >= 0)
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
    int rc = rpmDigestFinal(ctx, &digest, &digestlen, 0);

    rc = memcmp(sigp->signhash16, digest, sizeof(sigp->signhash16));

if (rc)
SPEW((stderr, "\t%s\t%s\n", dname, pgpHexStr(digest, digestlen)));
SPEW((stderr, "%s\t%s\n", (!rc ? "\tGOOD" : "------> BAD"), pgpHexStr(sigp->signhash16, sizeof(sigp->signhash16))));

    if (rc)
	SUM.HASH.bad++;
    else
	SUM.HASH.good++;

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
	if (pgpImplSetDSA(ctx, dig, sigp)) {
SPEW((stderr, "------> BAD\t%s\n", pgpHexStr(sigp->signhash16, 2)));
	    SUM.HASH.bad++;
	    goto exit;
	}
	if (!pgpImplVerifyDSA(dig)) {
SPEW((stderr, "------> BAD\tV%u %s-%s\n",
		sigp->version,
		_pgpPubkeyAlgo2Name(sigp->pubkey_algo),
		_pgpHashAlgo2Name(sigp->hash_algo)));
	    SUM.DSA.bad++;
	} else {
SPEW((stderr, "\tGOOD\tV%u %s-%s\n",
		sigp->version,
		_pgpPubkeyAlgo2Name(sigp->pubkey_algo),
		_pgpHashAlgo2Name(sigp->hash_algo)));
	    SUM.DSA.good++;
	    rc = 1;
	}
	break;
    case PGPPUBKEYALGO_RSA:
	if (pgpImplSetRSA(ctx, dig, sigp)) {
SPEW((stderr, "------> BAD\t%s\n", pgpHexStr(sigp->signhash16, 2)));
	    SUM.HASH.bad++;
	    goto exit;
	}
	if (!pgpImplVerifyRSA(dig)) {
SPEW((stderr, "------> BAD\tV%u %s-%s\n",
		sigp->version,
		_pgpPubkeyAlgo2Name(sigp->pubkey_algo),
		_pgpHashAlgo2Name(sigp->hash_algo)));
	    SUM.RSA.bad++;
	} else {
SPEW((stderr, "\tGOOD\tV%u %s-%s\n",
		sigp->version,
		_pgpPubkeyAlgo2Name(sigp->pubkey_algo),
		_pgpHashAlgo2Name(sigp->hash_algo)));
	    SUM.RSA.good++;
	    rc = 1;
	}
	break;
    }

exit:
    return rc;
}

static int rpmhkpVerify(rpmhkp hkp, pgpPkt pp)
{
    pgpDig dig = pgpDigNew(0);
    pgpDigParams sigp = pgpGetSignature(dig);
    DIGEST_CTX ctx = NULL;
    int keyx;
    int rc = 1;		/* assume failure */
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

    /* XXX Load pubkey paramaters. */
    keyx = rpmhkpFindKey(hkp, dig, sigp->signid, sigp->pubkey_algo);
    if (keyx == -2)	/* XXX AWOL */
	goto exit;

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

	switch (sigp->pubkey_algo) {
	/* XXX handle only RSA/DSA? */
	case PGPPUBKEYALGO_DSA:
	case PGPPUBKEYALGO_RSA:
	    /* XXX only V4 certs for now. */
	    if (sigp->version == 4) {
		rc = rpmhkpVerifySignature(hkp, dig, ctx);
		break;
	    }
	    /*@fallthrough@*/
	default:
	    rc = rpmhkpVerifyHash(hkp, dig, ctx);
	    break;
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
    size_t pleft = hkp->pktlen;
    int rc = 1;		/* assume failure */
    int xx;
    int i;

    if (hkp->pkts)
    for (i = 0; i < hkp->npkts; i++) {
	xx = pgpPktLen(hkp->pkts[i], pleft, pp);
assert(xx > 0);
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
SPEW((stderr, "  SIG: V%u\n", *pp->h));
SPEW((stderr, "\tSKIP(V%u != V3 | V4)\t%s\n", *pp->h, pgpHexStr(pp->h, pp->pktlen)));
		SUM.SKIP.bad++;
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
    uint64_t * kip;
    rpmuint8_t keyid[8];
    int rc;
    int ec = 0;

    for (kip = keyids; *kip; kip++) {

	keyid[0] = (kip[0] >> 56);
	keyid[1] = (kip[0] >> 48);
	keyid[2] = (kip[0] >> 40);
	keyid[3] = (kip[0] >> 32);
	keyid[4] = (kip[0] >> 24);
	keyid[5] = (kip[0] >> 16);
	keyid[6] = (kip[0] >>  8);
	keyid[7] = (kip[0]      );

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
 { "noneon", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &noNeon, 1,
	N_("disable use of libneon for HTTP"), NULL},

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
        N_("Common options:"), NULL },

  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    poptContext optCon = rpmioInit(argc, argv, optionsTable);
    int ec;

    if (_debug) {
	rpmIncreaseVerbosity();
	rpmIncreaseVerbosity();
    }

    rpmbfParams(awol_n, awol_e, &awol_m, &awol_k);
    awol = rpmbfNew(awol_m, awol_k, 0);

    ec = rpmhkpReadKeys();

fprintf(stderr, " DSA:%10u:%-10u\n", SUM.DSA.good, (SUM.DSA.good+SUM.DSA.bad));
fprintf(stderr, " RSA:%10u:%-10u\n", SUM.RSA.good, (SUM.RSA.good+SUM.RSA.bad));
fprintf(stderr, "HASH:%10u:%-10u\n", SUM.HASH.good, (SUM.HASH.good+SUM.HASH.bad));
fprintf(stderr, "AWOL:%10u:%-10u\n", SUM.AWOL.good, (SUM.AWOL.good+SUM.AWOL.bad));
fprintf(stderr, "SKIP:%10u:%-10u\n", SUM.SKIP.good, (SUM.SKIP.good+SUM.SKIP.bad));

    awol = rpmbfFree(awol);

/*@i@*/ urlFreeCache();

    optCon = rpmioFini(optCon);

    return ec;
}

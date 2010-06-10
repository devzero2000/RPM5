/** \ingroup rpmpgp
 * \file rpmio/rpmltc.c
 */

#include "system.h"
#include <rpmlog.h>

#include <rpmiotypes.h>

#define	_RPMPGP_INTERNAL
#if defined(WITH_TOMCRYPT)
#define	_RPMLTC_INTERNAL
#include <rpmltc.h>
#endif

#include "debug.h"

#if defined(WITH_TOMCRYPT)

/*@access pgpDig @*/
/*@access pgpDigParams @*/

/*@-redecl@*/
/*@unchecked@*/
extern int _pgp_debug;

/*@unchecked@*/
extern int _pgp_print;
/*@=redecl@*/

/*@unchecked@*/
static int _rpmltc_debug = 1;

#define	SPEW(_t, _rc, _dig)	\
  { if ((_t) || _rpmltc_debug || _pgp_debug < 0) \
	fprintf(stderr, "<-- %s(%p) %s\t%s\n", __FUNCTION__, (_dig), \
		((_rc) ? "OK" : "BAD"), (_dig)->pubkey_algoN); \
  }

static const char * rpmltcHashAlgo2Name(uint32_t algo)
{
    return pgpValStr(pgpHashTbl, (rpmuint8_t)algo);
}

static const char * rpmltcPubkeyAlgo2Name(uint32_t algo)
{
    return pgpValStr(pgpPubkeyTbl, (rpmuint8_t)algo);
}

static
int rpmltcSetRSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    rpmltc ltc = dig->impl;
    int rc;
    int xx;
pgpDigParams pubp = pgpGetPubkey(dig);
dig->pubkey_algoN = rpmltcPubkeyAlgo2Name(pubp->pubkey_algo);
dig->hash_algoN = rpmltcHashAlgo2Name(sigp->hash_algo);

assert(sigp->hash_algo == rpmDigestAlgo(ctx));

/* XXX FIXME: should this lazy free be done elsewhere? */
ltc->digest = _free(ltc->digest);
ltc->digestlen = 0;
    xx = rpmDigestFinal(ctx, (void **)&ltc->digest, &ltc->digestlen, 0);

    /* Add PKCS1 padding. */

    /* Compare leading 16 bits of digest for quick check. */
rc = 0;

SPEW(0, !rc, dig);
    return rc;
}

static
int rpmltcVerifyRSA(pgpDig dig)
	/*@*/
{
    rpmltc ltc = dig->impl;
    int rc = 0;		/* assume failure. */
ltc = ltc;

SPEW(!rc, rc, dig);
    return rc;
}

static
int rpmltcSignRSA(pgpDig dig)
	/*@*/
{
    rpmltc ltc = dig->impl;
    int rc = 0;		/* assume failure. */
ltc = ltc;

SPEW(!rc, rc, dig);

    return rc;
}

static
int rpmltcGenerateRSA(pgpDig dig)
	/*@*/
{
    rpmltc ltc = dig->impl;
    int rc = 0;		/* assume failure. */
ltc = ltc;

SPEW(!rc, rc, dig);

    return rc;
}

static
int rpmltcSetDSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    rpmltc ltc = dig->impl;
    int rc;
    int xx;
pgpDigParams pubp = pgpGetPubkey(dig);
dig->pubkey_algoN = rpmltcPubkeyAlgo2Name(pubp->pubkey_algo);
dig->hash_algoN = rpmltcHashAlgo2Name(sigp->hash_algo);

assert(sigp->hash_algo == rpmDigestAlgo(ctx));
    /* Set DSA hash. */
/* XXX FIXME: should this lazy free be done elsewhere? */
ltc->digest = _free(ltc->digest);
ltc->digestlen = 0;
    xx = rpmDigestFinal(ctx, (void **)&ltc->digest, &ltc->digestlen, 0);

    /* Compare leading 16 bits of digest for quick check. */
    rc = memcmp(ltc->digest, sigp->signhash16, sizeof(sigp->signhash16));

SPEW(0, !rc, dig);
    return rc;
}

static
int rpmltcVerifyDSA(pgpDig dig)
	/*@*/
{
    rpmltc ltc = dig->impl;
    int rc;
ltc = ltc;

SPEW(!rc, rc, dig);
    return rc;
}

static
int rpmltcSignDSA(pgpDig dig)
	/*@*/
{
    rpmltc ltc = dig->impl;
    int rc = 0;		/* assume failure */
ltc = ltc;

SPEW(!rc, rc, dig);

    return rc;
}

static
int rpmltcGenerateDSA(pgpDig dig)
	/*@*/
{
    rpmltc ltc = dig->impl;
    int rc = 0;		/* assume failure. */
ltc = ltc;

SPEW(!rc, rc, dig);

    return rc;
}

static
int rpmltcSetELG(/*@only@*/ DIGEST_CTX ctx, /*@unused@*/pgpDig dig, pgpDigParams sigp)
	/*@*/
{
    rpmltc ltc = dig->impl;
    int rc = 1;		/* XXX always fail. */
    int xx;

assert(sigp->hash_algo == rpmDigestAlgo(ctx));

/* XXX FIXME: should this lazy free be done elsewhere? */
ltc->digest = _free(ltc->digest);
ltc->digestlen = 0;
    xx = rpmDigestFinal(ctx, (void **)&ltc->digest, &ltc->digestlen, 0);

    /* Compare leading 16 bits of digest for quick check. */
    rc = memcmp(ltc->digest, sigp->signhash16, sizeof(sigp->signhash16));

SPEW(rc, !rc, dig);
    return rc;
}

static
int rpmltcSetECDSA(/*@only@*/ DIGEST_CTX ctx, /*@unused@*/pgpDig dig, pgpDigParams sigp)
	/*@*/
{
    rpmltc ltc = dig->impl;
    int rc = 1;		/* assume failure. */
    int xx;

assert(sigp->hash_algo == rpmDigestAlgo(ctx));

/* XXX FIXME: should this lazy free be done elsewhere? */
ltc->digest = _free(ltc->digest);
ltc->digestlen = 0;
    xx = rpmDigestFinal(ctx, &ltc->digest, &ltc->digestlen, 0);

    /* Compare leading 16 bits of digest for quick check. */
    rc = memcmp(ltc->digest, sigp->signhash16, sizeof(sigp->signhash16));

SPEW(rc, !rc, dig);
    return rc;
}

static
int rpmltcVerifyECDSA(pgpDig dig)
	/*@*/
{
    rpmltc ltc = dig->impl;
    int rc = 0;		/* assume failure. */
ltc = ltc;

SPEW(!rc, rc, dig);
    return rc;
}

static
int rpmltcSignECDSA(pgpDig dig)
	/*@*/
{
    rpmltc ltc = dig->impl;
    int rc = 0;		/* assume failure. */
ltc = ltc;

SPEW(!rc, rc, dig);
    return rc;
}

static
int rpmltcGenerateECDSA(pgpDig dig)
	/*@*/
{
    rpmltc ltc = dig->impl;
    int rc = 0;		/* assume failure. */
ltc = ltc;

SPEW(!rc, rc, dig);
    return rc;
}

static int rpmltcErrChk(pgpDig dig, const char * msg, int rc, unsigned expected)
{
#ifdef	REFERENCE
rpmgc gc = dig->impl;
    /* Was the return code the expected result? */
    rc = (gcry_err_code(gc->err) != expected);
    if (rc)
	fail("%s failed: %s\n", msg, gpg_strerror(gc->err));
/* XXX FIXME: rpmltcStrerror */
#else
    rc = (rc == 0);	/* XXX impedance match 1 -> 0 on success */
#endif
    return rc;	/* XXX 0 on success */
}

static int rpmltcAvailableCipher(pgpDig dig, int algo)
{
    int rc = 0;	/* assume available */
#ifdef	REFERENCE
    rc = rpmgcAvailable(dig->impl, algo,
    	(gcry_md_test_algo(algo) || algo == PGPHASHALGO_MD5));
#endif
    return rc;
}

static int rpmltcAvailableDigest(pgpDig dig, int algo)
{
    int rc = 0;	/* assume available */
#ifdef	REFERENCE
    rc = rpmgcAvailable(dig->impl, algo,
    	(gcry_md_test_algo(algo) || algo == PGPHASHALGO_MD5));
#endif
    return rc;
}

static int rpmltcAvailablePubkey(pgpDig dig, int algo)
{
    int rc = 0;	/* assume available */
#ifdef	REFERENCE
    rc = rpmgcAvailable(dig->impl, algo, gcry_pk_test_algo(algo));
#endif
    return rc;
}

static int rpmltcVerify(pgpDig dig)
{
    int rc = 0;		/* assume failure */
pgpDigParams pubp = pgpGetPubkey(dig);
pgpDigParams sigp = pgpGetSignature(dig);
dig->pubkey_algoN = rpmltcPubkeyAlgo2Name(pubp->pubkey_algo);
dig->hash_algoN = rpmltcHashAlgo2Name(sigp->hash_algo);

    switch (pubp->pubkey_algo) {
    default:
	break;
    case PGPPUBKEYALGO_RSA:
	rc = rpmltcVerifyRSA(dig);
	break;
    case PGPPUBKEYALGO_DSA:
	rc = rpmltcVerifyDSA(dig);
	break;
    case PGPPUBKEYALGO_ELGAMAL:
#ifdef	NOTYET
	rc = rpmltcVerifyELG(dig);
#endif
	break;
    case PGPPUBKEYALGO_ECDSA:
	rc = rpmltcVerifyECDSA(dig);
	break;
    }
SPEW(!rc, rc, dig);
    return rc;
}

static int rpmltcSign(pgpDig dig)
{
    int rc = 0;		/* assume failure */
pgpDigParams pubp = pgpGetPubkey(dig);
dig->pubkey_algoN = rpmltcPubkeyAlgo2Name(pubp->pubkey_algo);

    switch (pubp->pubkey_algo) {
    default:
	break;
    case PGPPUBKEYALGO_RSA:
	rc = rpmltcSignRSA(dig);
	break;
    case PGPPUBKEYALGO_DSA:
	rc = rpmltcSignDSA(dig);
	break;
    case PGPPUBKEYALGO_ELGAMAL:
#ifdef	NOTYET
	rc = rpmltcSignELG(dig);
#endif
	break;
    case PGPPUBKEYALGO_ECDSA:
	rc = rpmltcSignECDSA(dig);
	break;
    }
SPEW(!rc, rc, dig);
    return rc;
}

static int rpmltcGenerate(pgpDig dig)
{
    int rc = 0;		/* assume failure */
pgpDigParams pubp = pgpGetPubkey(dig);
dig->pubkey_algoN = rpmltcPubkeyAlgo2Name(pubp->pubkey_algo);

    switch (pubp->pubkey_algo) {
    default:
	break;
    case PGPPUBKEYALGO_RSA:
	rc = rpmltcGenerateRSA(dig);
	break;
    case PGPPUBKEYALGO_DSA:
	rc = rpmltcGenerateDSA(dig);
	break;
    case PGPPUBKEYALGO_ELGAMAL:
#ifdef	NOTYET
	rc = rpmltcGenerateELG(dig);
#endif
	break;
    case PGPPUBKEYALGO_ECDSA:
	rc = rpmltcGenerateECDSA(dig);
	break;
    }
SPEW(!rc, rc, dig);
    return rc;
}

static
int rpmltcMpiItem(/*@unused@*/ const char * pre, pgpDig dig, int itemno,
		const rpmuint8_t * p,
		/*@unused@*/ /*@null@*/ const rpmuint8_t * pend)
	/*@*/
{
    rpmltc ltc = dig->impl;
    unsigned int nb = ((pgpMpiBits(p) + 7) >> 3);
    int rc = 0;

    switch (itemno) {
    default:
assert(0);
    case 50:		/* ECDSA r */
    case 51:		/* ECDSA s */
    case 60:		/* ECDSA curve OID */
    case 61:		/* ECDSA Q */
	break;
    case 10:		/* RSA m**d */
	break;
    case 20:		/* DSA r */
	break;
    case 21:		/* DSA s */
	break;
    case 30:		/* RSA n */
	break;
    case 31:		/* RSA e */
	break;
    case 40:		/* DSA p */
	break;
    case 41:		/* DSA q */
	break;
    case 42:		/* DSA g */
	break;
    case 43:		/* DSA y */
	break;
    }
    return rc;
}

static
void rpmltcClean(void * impl)
	/*@modifies impl @*/
{
    rpmltc ltc = impl;
    if (ltc != NULL) {
	ltc->nbits = 0;
	ltc->err = 0;
	ltc->badok = 0;
	ltc->digest = _free(ltc->digest);
	ltc->digestlen = 0;

    }
}

static /*@null@*/
void * rpmltcFree(/*@only@*/ void * impl)
	/*@modifies impl @*/
{
    rpmltcClean(impl);
    impl = _free(impl);
    return NULL;
}

static
void * rpmltcInit(void)
	/*@*/
{
    rpmltc ltc = xcalloc(1, sizeof(*ltc));
    return (void *) ltc;
}

struct pgpImplVecs_s rpmltcImplVecs = {
	rpmltcSetRSA,
	rpmltcSetDSA,
	rpmltcSetELG,
	rpmltcSetECDSA,

	rpmltcErrChk,
	rpmltcAvailableCipher, rpmltcAvailableDigest, rpmltcAvailablePubkey,
	rpmltcVerify, rpmltcSign, rpmltcGenerate,

	rpmltcMpiItem, rpmltcClean,
	rpmltcFree, rpmltcInit
};

#endif	/* WITH_TOMCRYPT */


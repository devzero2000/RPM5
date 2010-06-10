/** \ingroup rpmpgp
 * \file rpmio/rpmcdsa.c
 */

#include "system.h"
#include <rpmlog.h>

#include <rpmiotypes.h>
#define	_RPMPGP_INTERNAL
#if defined(WITH_CDSA)

#define	_RPMCDSA_INTERNAL
#include <rpmcdsa.h>
#endif

#include "debug.h"

#if defined(WITH_CDSA)

/*@access pgpDig @*/
/*@access pgpDigParams @*/

/*@-redecl@*/
/*@unchecked@*/
extern int _pgp_debug;

/*@unchecked@*/
extern int _pgp_print;
/*@=redecl@*/

/*@unchecked@*/
static int _rpmcdsa_debug = 1;

#define	SPEW(_t, _rc, _dig)	\
  { if ((_t) || _rpmcdsa_debug || _pgp_debug < 0) \
	fprintf(stderr, "<-- %s(%p) %s\t%s\n", __FUNCTION__, (_dig), \
		((_rc) ? "OK" : "BAD"), (_dig)->pubkey_algoN); \
  }

static const char * rpmcdsaHashAlgo2Name(uint32_t algo)
{
    return pgpValStr(pgpHashTbl, (rpmuint8_t)algo);
}

static const char * rpmcdsaPubkeyAlgo2Name(uint32_t algo)
{
    return pgpValStr(pgpPubkeyTbl, (rpmuint8_t)algo);
}

static
int rpmcdsaSetRSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    rpmcdsa cdsa = dig->impl;
    int rc;
    int xx;
pgpDigParams pubp = pgpGetPubkey(dig);
dig->pubkey_algoN = rpmcdsaPubkeyAlgo2Name(pubp->pubkey_algo);
dig->hash_algoN = rpmcdsaHashAlgo2Name(sigp->hash_algo);

assert(sigp->hash_algo == rpmDigestAlgo(ctx));

/* XXX FIXME: should this lazy free be done elsewhere? */
cdsa->digest = _free(cdsa->digest);
cdsa->digestlen = 0;
    xx = rpmDigestFinal(ctx, (void **)&cdsa->digest, &cdsa->digestlen, 1);

    /* Compare leading 16 bits of digest for quick check. */
rc = 0;

SPEW(0, !rc, dig);
    return rc;
}

static
int rpmcdsaVerifyRSA(pgpDig dig)
	/*@*/
{
    rpmcdsa cdsa = dig->impl;
    int rc = 0;		/* assume failure. */
cdsa = cdsa;

SPEW(!rc, rc, dig);
    return rc;
}

static
int rpmcdsaSignRSA(pgpDig dig)
	/*@*/
{
    rpmcdsa cdsa = dig->impl;
    int rc = 0;		/* assume failure. */
cdsa = cdsa;

SPEW(!rc, rc, dig);

    return rc;
}

static
int rpmcdsaGenerateRSA(pgpDig dig)
	/*@*/
{
    rpmcdsa cdsa = dig->impl;
    int rc = 0;		/* assume failure. */
cdsa = cdsa;

SPEW(!rc, rc, dig);

    return rc;
}

static
int rpmcdsaSetDSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    rpmcdsa cdsa = dig->impl;
    int rc;
    int xx;
pgpDigParams pubp = pgpGetPubkey(dig);
dig->pubkey_algoN = rpmcdsaPubkeyAlgo2Name(pubp->pubkey_algo);
dig->hash_algoN = rpmcdsaHashAlgo2Name(sigp->hash_algo);

assert(sigp->hash_algo == rpmDigestAlgo(ctx));
    /* Set DSA hash. */
/* XXX FIXME: should this lazy free be done elsewhere? */
cdsa->digest = _free(cdsa->digest);
cdsa->digestlen = 0;
    xx = rpmDigestFinal(ctx, (void **)&cdsa->digest, &cdsa->digestlen, 0);

    /* Compare leading 16 bits of digest for quick check. */
    rc = memcmp(cdsa->digest, sigp->signhash16, sizeof(sigp->signhash16));

SPEW(0, !rc, dig);
    return rc;
}

static
int rpmcdsaVerifyDSA(pgpDig dig)
	/*@*/
{
    rpmcdsa cdsa = dig->impl;
    int rc;
cdsa = cdsa;

SPEW(!rc, rc, dig);
    return rc;
}

static
int rpmcdsaSignDSA(pgpDig dig)
	/*@*/
{
    rpmcdsa cdsa = dig->impl;
    int rc = 0;		/* assume failure */
cdsa = cdsa;

SPEW(!rc, rc, dig);

    return rc;
}

static
int rpmcdsaGenerateDSA(pgpDig dig)
	/*@*/
{
    rpmcdsa cdsa = dig->impl;
    int rc = 0;		/* assume failure. */
cdsa = cdsa;

SPEW(!rc, rc, dig);

    return rc;
}

static
int rpmcdsaSetELG(/*@only@*/ DIGEST_CTX ctx, /*@unused@*/pgpDig dig, pgpDigParams sigp)
	/*@*/
{
    rpmcdsa cdsa = dig->impl;
    int rc = 1;		/* XXX always fail. */
    int xx;

assert(sigp->hash_algo == rpmDigestAlgo(ctx));

/* XXX FIXME: should this lazy free be done elsewhere? */
cdsa->digest = _free(cdsa->digest);
cdsa->digestlen = 0;
    xx = rpmDigestFinal(ctx, (void **)&cdsa->digest, &cdsa->digestlen, 0);

    /* Compare leading 16 bits of digest for quick check. */
    rc = memcmp(cdsa->digest, sigp->signhash16, sizeof(sigp->signhash16));

SPEW(rc, !rc, dig);
    return rc;
}

static
int rpmcdsaSetECDSA(/*@only@*/ DIGEST_CTX ctx, /*@unused@*/pgpDig dig, pgpDigParams sigp)
	/*@*/
{
    rpmcdsa cdsa = dig->impl;
    int rc = 1;		/* assume failure. */
    int xx;

assert(sigp->hash_algo == rpmDigestAlgo(ctx));

/* XXX FIXME: should this lazy free be done elsewhere? */
cdsa->digest = _free(cdsa->digest);
cdsa->digestlen = 0;
    xx = rpmDigestFinal(ctx, &cdsa->digest, &cdsa->digestlen, 0);

    /* Compare leading 16 bits of digest for quick check. */
    rc = memcmp(cdsa->digest, sigp->signhash16, sizeof(sigp->signhash16));

SPEW(rc, !rc, dig);
    return rc;
}

static
int rpmcdsaVerifyECDSA(pgpDig dig)
	/*@*/
{
    rpmcdsa cdsa = dig->impl;
    int rc = 0;		/* assume failure. */
cdsa = cdsa;

SPEW(!rc, rc, dig);
    return rc;
}

static
int rpmcdsaSignECDSA(pgpDig dig)
	/*@*/
{
    rpmcdsa cdsa = dig->impl;
    int rc = 0;		/* assume failure. */
cdsa = cdsa;

SPEW(!rc, rc, dig);
    return rc;
}

static
int rpmcdsaGenerateECDSA(pgpDig dig)
	/*@*/
{
    rpmcdsa cdsa = dig->impl;
    int rc = 0;		/* assume failure. */
cdsa = cdsa;

SPEW(!rc, rc, dig);
    return rc;
}

static int rpmcdsaErrChk(pgpDig dig, const char * msg, int rc, unsigned expected)
{
#ifdef	REFERENCE
rpmgc gc = dig->impl;
    /* Was the return code the expected result? */
    rc = (gcry_err_code(gc->err) != expected);
    if (rc)
	fail("%s failed: %s\n", msg, gpg_strerror(gc->err));
/* XXX FIXME: rpmcdsaStrerror */
#else
    rc = (rc == 0);	/* XXX impedance match 1 -> 0 on success */
#endif
    return rc;	/* XXX 0 on success */
}

static int rpmcdsaAvailableCipher(pgpDig dig, int algo)
{
    int rc = 0;	/* assume available */
#ifdef	REFERENCE
    rc = rpmgcAvailable(dig->impl, algo,
    	(gcry_md_test_algo(algo) || algo == PGPHASHALGO_MD5));
#endif
    return rc;
}

static int rpmcdsaAvailableDigest(pgpDig dig, int algo)
{
    int rc = 0;	/* assume available */
#ifdef	REFERENCE
    rc = rpmgcAvailable(dig->impl, algo,
    	(gcry_md_test_algo(algo) || algo == PGPHASHALGO_MD5));
#endif
    return rc;
}

static int rpmcdsaAvailablePubkey(pgpDig dig, int algo)
{
    int rc = 0;	/* assume available */
#ifdef	REFERENCE
    rc = rpmgcAvailable(dig->impl, algo, gcry_pk_test_algo(algo));
#endif
    return rc;
}

static int rpmcdsaVerify(pgpDig dig)
{
    int rc = 0;		/* assume failure */
pgpDigParams pubp = pgpGetPubkey(dig);
pgpDigParams sigp = pgpGetSignature(dig);
dig->pubkey_algoN = rpmcdsaPubkeyAlgo2Name(pubp->pubkey_algo);
dig->hash_algoN = rpmcdsaHashAlgo2Name(sigp->hash_algo);

    switch (pubp->pubkey_algo) {
    default:
	break;
    case PGPPUBKEYALGO_RSA:
	rc = rpmcdsaVerifyRSA(dig);
	break;
    case PGPPUBKEYALGO_DSA:
	rc = rpmcdsaVerifyDSA(dig);
	break;
    case PGPPUBKEYALGO_ELGAMAL:
#ifdef	NOTYET
	rc = rpmcdsaVerifyELG(dig);
#endif
	break;
    case PGPPUBKEYALGO_ECDSA:
	rc = rpmcdsaVerifyECDSA(dig);
	break;
    }
SPEW(!rc, rc, dig);
    return rc;
}

static int rpmcdsaSign(pgpDig dig)
{
    int rc = 0;		/* assume failure */
pgpDigParams pubp = pgpGetPubkey(dig);
dig->pubkey_algoN = rpmcdsaPubkeyAlgo2Name(pubp->pubkey_algo);

    switch (pubp->pubkey_algo) {
    default:
	break;
    case PGPPUBKEYALGO_RSA:
	rc = rpmcdsaSignRSA(dig);
	break;
    case PGPPUBKEYALGO_DSA:
	rc = rpmcdsaSignDSA(dig);
	break;
    case PGPPUBKEYALGO_ELGAMAL:
#ifdef	NOTYET
	rc = rpmcdsaSignELG(dig);
#endif
	break;
    case PGPPUBKEYALGO_ECDSA:
	rc = rpmcdsaSignECDSA(dig);
	break;
    }
SPEW(!rc, rc, dig);
    return rc;
}

static int rpmcdsaGenerate(pgpDig dig)
{
    int rc = 0;		/* assume failure */
pgpDigParams pubp = pgpGetPubkey(dig);
dig->pubkey_algoN = rpmcdsaPubkeyAlgo2Name(pubp->pubkey_algo);

    switch (pubp->pubkey_algo) {
    default:
	break;
    case PGPPUBKEYALGO_RSA:
	rc = rpmcdsaGenerateRSA(dig);
	break;
    case PGPPUBKEYALGO_DSA:
	rc = rpmcdsaGenerateDSA(dig);
	break;
    case PGPPUBKEYALGO_ELGAMAL:
#ifdef	NOTYET
	rc = rpmcdsaGenerateELG(dig);
#endif
	break;
    case PGPPUBKEYALGO_ECDSA:
	rc = rpmcdsaGenerateECDSA(dig);
	break;
    }
SPEW(!rc, rc, dig);
    return rc;
}

static
int rpmcdsaMpiItem(/*@unused@*/ const char * pre, pgpDig dig, int itemno,
		const rpmuint8_t * p,
		/*@unused@*/ /*@null@*/ const rpmuint8_t * pend)
	/*@*/
{
    rpmcdsa cdsa = dig->impl;
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
void rpmcdsaClean(void * impl)
	/*@modifies impl @*/
{
    rpmcdsa cdsa = impl;
    if (cdsa != NULL) {
	cdsa->nbits = 0;
	cdsa->err = 0;
	cdsa->badok = 0;
	cdsa->digest = _free(cdsa->digest);
	cdsa->digestlen = 0;

    }
}

static /*@null@*/
void * rpmcdsaFree(/*@only@*/ void * impl)
	/*@modifies impl @*/
{
    rpmcdsaClean(impl);
    impl = _free(impl);
    return NULL;
}

static
void * rpmcdsaInit(void)
	/*@*/
{
    rpmcdsa cdsa = xcalloc(1, sizeof(*cdsa));
    return (void *) cdsa;
}

struct pgpImplVecs_s rpmcdsaImplVecs = {
	rpmcdsaSetRSA,
	rpmcdsaSetDSA,
	rpmcdsaSetELG,
	rpmcdsaSetECDSA,

	rpmcdsaErrChk,
	rpmcdsaAvailableCipher, rpmcdsaAvailableDigest, rpmcdsaAvailablePubkey,
	rpmcdsaVerify, rpmcdsaSign, rpmcdsaGenerate,

	rpmcdsaMpiItem, rpmcdsaClean,
	rpmcdsaFree, rpmcdsaInit
};

#endif	/* WITH_CDSA */


/** \ingroup rpmpgp
 * \file rpmio/rpmltc.c
 */

#include "system.h"
#include <rpmlog.h>

#include <rpmiotypes.h>

#define	_RPMPGP_INTERNAL
#if defined(WITH_TOMCRYPT)
#define	LTM_DESC
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
static int _rpmltc_debug;

/*@unchecked@*/
static prng_state yarrow_prng;

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
int rpmltcErr(rpmltc ltc, const char * msg, int rc)
        /*@*/
{
    /* XXX FIXME: Don't spew on expected failures ... */
    if (rc != CRYPT_OK) {
        fprintf (stderr, "rpmltc: %s rc(%d) %s\n",
                msg, rc, error_to_string(rc));
    }
    return rc;
}

#define	_spewBN(_N, _BN)	\
  { mp_int * bn = _BN; \
    int nt = 0; \
    int err = (bn ? mp_radix_size(bn, 16, &nt) : 0); \
    char * t = (nt ? xmalloc(nt) : ""); \
    if (nt) \
	err = mp_toradix(bn, t, 16); \
    fprintf(stderr, "\t" _N ": %s\n", t); \
    if (nt) \
	t = _free(t); \
  }

static void rpmltcDumpRSA(const char * msg, rpmltc ltc)
{
    if (msg) fprintf(stderr, "========== %s\n", msg);

    {
	_spewBN(" n", ltc->rsa.N);
	_spewBN(" e", ltc->rsa.e);
	_spewBN(" d", ltc->rsa.d);
	_spewBN(" p", ltc->rsa.p);
	_spewBN(" q", ltc->rsa.q);
	_spewBN("dp", ltc->rsa.dP);
	_spewBN("dq", ltc->rsa.dQ);
	_spewBN("qi", ltc->rsa.qP);
    }

    _spewBN(" c", ltc->c);
}

static void rpmltcDumpDSA(const char * msg, rpmltc ltc)
{
    if (msg) fprintf(stderr, "========== %s\n", msg);

    {
	_spewBN(" p", ltc->dsa.p);
	_spewBN(" q", ltc->dsa.q);
	_spewBN(" g", ltc->dsa.g);
	_spewBN(" x", ltc->dsa.x);
	_spewBN(" y", ltc->dsa.y);
    }

    _spewBN(" r", ltc->r);
    _spewBN(" s", ltc->s);

}

static void rpmltcDumpECDSA(const char * msg, rpmltc ltc)
{
    if (msg) fprintf(stderr, "========== %s\n", msg);

    {
	const ltc_ecc_set_type * dp = ltc->ecdsa.idx >= 0
		? ltc_ecc_sets + ltc->ecdsa.idx : NULL;
	if (dp == NULL)
	    dp = ltc->ecdsa.dp;

	if (dp) {
	    fprintf(stderr, "\tsize: %d\n", dp->size);
	    fprintf(stderr, "\tname: %s\n", dp->name);
	    fprintf(stderr, "\t p: %s\n", dp->prime);
	    fprintf(stderr, "\t n: %s\n", dp->order);
	    fprintf(stderr, "\t b: %s\n", dp->B);
	    fprintf(stderr, "\tGx: %s\n", dp->Gx);
	    fprintf(stderr, "\tGy: %s\n", dp->Gy);
	}

	_spewBN("Qx", ltc->ecdsa.pubkey.x);
	_spewBN("Qy", ltc->ecdsa.pubkey.y);

	_spewBN(" d", ltc->ecdsa.k);

    }

    _spewBN(" r", ltc->r);
    _spewBN(" s", ltc->s);
}

#undef	_spewBN

#define	_initBN(_t) \
  { if (_t == NULL) _t = xmalloc(sizeof(mp_int)); \
    xx = mp_init(_t); \
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

    /* Compare leading 16 bits of digest for quick check. */
    rc = memcmp(ltc->digest, sigp->signhash16, sizeof(sigp->signhash16));

SPEW(0, !rc, dig);
    return rc;
}

static
int rpmltcVerifyRSA(pgpDig dig)
	/*@*/
{
    rpmltc ltc = dig->impl;
    int rc = 0;		/* assume failure. */
int _padding = LTC_LTC_PKCS_1_V1_5;
int hash_idx = find_hash("sha1");
unsigned long saltlen = 0;
unsigned char sig[2048];
unsigned long siglen = sizeof(sig);
unsigned char digest[2048];
unsigned long digestlen = sizeof(digest);
int xx;

#ifdef	DYING
rpmltcDumpRSA(__FUNCTION__, ltc);
#endif
if (ltc->digest == NULL || ltc->digestlen == 0) goto exit;

xx = mp_to_unsigned_bin_n(ltc->c, sig, &siglen);
memcpy(digest, ltc->digest, ltc->digestlen);

#ifndef	NOTYET
    rc = rpmltcErr(ltc, "rsa_verify_hash_ex",
		rsa_verify_hash_ex(sig, siglen,
			ltc->digest, ltc->digestlen,
			_padding, hash_idx, saltlen, &rc, &ltc->rsa));
#else
    rc = rpmltcErr(ltc, "rsa_decrypt_key_ex",
		rsa_decrypt_key_ex(sig, siglen, digest, &digestlen,
			NULL, 0, hash_idx, _padding, &rc, &ltc->rsa));
#endif

exit:
SPEW(!rc, rc, dig);
    return rc;
}

static
int rpmltcSignRSA(pgpDig dig)
	/*@*/
{
    rpmltc ltc = dig->impl;
    int rc = 0;		/* assume failure. */
int _padding = LTC_LTC_PKCS_1_V1_5;
int hash_idx = find_hash("sha1");
unsigned long saltlen = 0;
unsigned char sig[2048];
unsigned long siglen = sizeof(sig);

if (ltc->digest == NULL || ltc->digestlen == 0) goto exit;

#ifdef	NOTYET
    rc = rpmltcErr(ltc, "rsa_sign_hash_ex",
		rsa_sign_hash_ex(ltc->digest, ltc->digestlen, sig, &siglen,
			_padding, &yarrow_prng, find_prng("yarrow"),
			hash_idx, saltlen, &ltc->rsa));
#else
    rc = rpmltcErr(ltc, "rsa_encrypt_key_ex",
		rsa_encrypt_key_ex(ltc->digest, ltc->digestlen, sig, &siglen,
			NULL, 0, &yarrow_prng, find_prng("yarrow"),
			hash_idx, _padding, &ltc->rsa));
#endif
    if (rc == CRYPT_OK) {
	int xx;
	_initBN(ltc->c);
	xx = mp_read_unsigned_bin(ltc->c, sig, siglen);
    }

#ifdef	DYING
rpmltcDumpRSA(__FUNCTION__, ltc);
#endif

    rc = (rc == CRYPT_OK);

exit:
SPEW(!rc, rc, dig);

    return rc;
}

static
int rpmltcGenerateRSA(pgpDig dig)
	/*@*/
{
    rpmltc ltc = dig->impl;
    int rc = 0;		/* assume failure. */
static long _e = 0x10001;	/* XXX FIXME */

if (ltc->nbits == 0) ltc->nbits = 1024;	/* XXX FIXME */

    rc = rpmltcErr(ltc, "rsa_make_key",
		rsa_make_key(&yarrow_prng, find_prng("yarrow"),
			ltc->nbits/8, _e, &ltc->rsa));
    rc = (rc == CRYPT_OK);

#ifdef	DYING
rpmltcDumpRSA(__FUNCTION__, ltc);
#endif

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
    int rc = 0;		/* assume failure. */
int xx;

if (ltc->digest == NULL || ltc->digestlen == 0) goto exit;
if (ltc->r == NULL || ltc->s == NULL) goto exit;

#ifdef	DYING
rpmltcDumpDSA(__FUNCTION__, ltc);
#endif
    xx = rpmltcErr(ltc, "dsa_verify_hash_raw",
		dsa_verify_hash_raw(ltc->r, ltc->s,
			ltc->digest, ltc->digestlen, &rc, &ltc->dsa));

exit:
SPEW(!rc, rc, dig);
    return rc;
}

static
int rpmltcSignDSA(pgpDig dig)
	/*@*/
{
    rpmltc ltc = dig->impl;
    int rc = 0;		/* assume failure */
int xx;

if (ltc->digest == NULL || ltc->digestlen == 0) goto exit;

    _initBN(ltc->r);
    _initBN(ltc->s);
    rc = rpmltcErr(ltc, "dsa_sign_hash_raw",
		dsa_sign_hash_raw(ltc->digest, ltc->digestlen, ltc->r, ltc->s,
			&yarrow_prng, find_prng("yarrow"), &ltc->dsa));

#ifdef	DYING
rpmltcDumpDSA(__FUNCTION__, ltc);
#endif

    rc = (rc == CRYPT_OK);

exit:
SPEW(!rc, rc, dig);

    return rc;
}

static
int rpmltcGenerateDSA(pgpDig dig)
	/*@*/
{
    rpmltc ltc = dig->impl;
    int rc = 0;		/* assume failure. */
int xx;
int _group_size;
int _modulus_size;

if (ltc->qbits == 0) ltc->qbits = 160;	/* XXX FIXME */
if (ltc->nbits == 0) ltc->nbits = 1024;	/* XXX FIXME */

_group_size = ltc->qbits/8;
_modulus_size = ltc->nbits/8;

    xx = rpmltcErr(ltc, "dsa_make_key",
		dsa_make_key(&yarrow_prng, find_prng("yarrow"),
			_group_size, _modulus_size, &ltc->dsa));
#ifdef	NOTYET
    xx = rpmltcErr(ltc, "dsa_verify_key",
		dsa_verify_key(&ltc->dsa, &rc));
#else
    rc = (xx == CRYPT_OK);
#endif
#ifdef	DYING
rpmltcDumpDSA(__FUNCTION__, ltc);
#endif

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
unsigned char sig[2048];
unsigned long siglen = sizeof(sig);
int xx;

if (ltc->digest == NULL || ltc->digestlen == 0) goto exit;
if (ltc->r == NULL || ltc->s == NULL) goto exit;

#ifdef	DYING
rpmltcDumpECDSA(__FUNCTION__, ltc);
#endif

    xx = der_encode_sequence_multi(sig, &siglen,
			LTC_ASN1_INTEGER, 1UL, ltc->r,
			LTC_ASN1_INTEGER, 1UL, ltc->s,
			LTC_ASN1_EOL, 0UL, NULL);

    xx = rpmltcErr(ltc, "ecc_verify_hash",
		ecc_verify_hash(sig, siglen,
			ltc->digest, ltc->digestlen, &rc, &ltc->ecdsa));

exit:
SPEW(!rc, rc, dig);
    return rc;
}

static
int rpmltcSignECDSA(pgpDig dig)
	/*@*/
{
    rpmltc ltc = dig->impl;
    int rc = 0;		/* assume failure. */
unsigned char sig[2048];
unsigned long siglen = sizeof(sig);

if (ltc->digest == NULL || ltc->digestlen == 0) goto exit;

    rc = rpmltcErr(ltc, "ecc_sign_hash",
		ecc_sign_hash(ltc->digest, ltc->digestlen, sig, &siglen,
			&yarrow_prng, find_prng ("yarrow"), &ltc->ecdsa));
    if (rc == CRYPT_OK) {
	int xx;
	_initBN(ltc->r);
	_initBN(ltc->s);
	xx = der_decode_sequence_multi(sig, siglen,
				LTC_ASN1_INTEGER, 1UL, ltc->r,
				LTC_ASN1_INTEGER, 1UL, ltc->s,
				LTC_ASN1_EOL, 0UL, NULL);
    }
    rc = (rc == CRYPT_OK);

#ifdef	DYING
rpmltcDumpECDSA(__FUNCTION__, ltc);
#endif

exit:
SPEW(!rc, rc, dig);
    return rc;
}

static
int rpmltcGenerateECDSA(pgpDig dig)
	/*@*/
{
    rpmltc ltc = dig->impl;
    int rc;

if (ltc->nbits == 0) ltc->nbits = 256;	/* XXX FIXME */

    rc = rpmltcErr(ltc, "ecc_make_key",
		ecc_make_key(&yarrow_prng, find_prng ("yarrow"),
			ltc->nbits/8, &ltc->ecdsa));
    rc = (rc == CRYPT_OK);

#ifdef	DYING
rpmltcDumpECDSA(__FUNCTION__, ltc);
#endif

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

#define	_loadBN(_t, _s, _ns) \
  { int xx; \
    _initBN(_t); \
    xx = mp_read_unsigned_bin(_t, _s, _ns); \
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
	break;
    case 10:	_loadBN(ltc->c, p+2, nb);	break;	/* RSA m**d */
    case 20:	_loadBN(ltc->r, p+2, nb);	break;	/* DSA r */
    case 21:	_loadBN(ltc->s, p+2, nb);	break;	/* DSA s */
    case 30:	_loadBN(ltc->rsa.N, p+2, nb);	break;	/* RSA n */
    case 31:	_loadBN(ltc->rsa.e, p+2, nb);	break;	/* RSA e */
    case 40:	_loadBN(ltc->dsa.p, p+2, nb);	break;	/* DSA p */
    case 41:	_loadBN(ltc->dsa.q, p+2, nb);	break;	/* DSA q */
    case 42:	_loadBN(ltc->dsa.g, p+2, nb);	break;	/* DSA g */
    case 43:	_loadBN(ltc->dsa.y, p+2, nb);	break;	/* DSA y */
    case 50:	_loadBN(ltc->r, p+2, nb);	break;	/* ECDSA r */
    case 51:	_loadBN(ltc->s, p+2, nb);	break;	/* ECDSA s */
    case 60:		/* ECDSA curve OID */
    case 61:		/* ECDSA Q */
	break;
    }

#ifdef	DYING
{   
const char * pubkey_algoN = dig->pubkey_algoN;
dig->pubkey_algoN = pre;
SPEW(0, !rc, dig);
dig->pubkey_algoN = pubkey_algoN;
}
#endif

    return rc;
}

#undef	_loadBN

#define	_freeBN(_t) \
    if (_t) { \
	(void) mp_clear(_t); \
	_t = _free(_t); \
    }

static
void rpmltcClean(void * impl)
	/*@modifies impl @*/
{
    rpmltc ltc = impl;
    if (ltc != NULL) {
	ltc->nbits = 0;
	ltc->qbits = 0;
	ltc->err = 0;
	ltc->badok = 0;
	ltc->digest = _free(ltc->digest);
	ltc->digestlen = 0;

	_freeBN(ltc->rsa.N);
	_freeBN(ltc->rsa.e);
	_freeBN(ltc->rsa.d);
	_freeBN(ltc->rsa.p);
	_freeBN(ltc->rsa.q);
	_freeBN(ltc->rsa.dP);
	_freeBN(ltc->rsa.dQ);
	_freeBN(ltc->rsa.qP);
	memset(&ltc->rsa, 0, sizeof(ltc->rsa));

	_freeBN(ltc->c);

	_freeBN(ltc->dsa.p);
	_freeBN(ltc->dsa.q);
	_freeBN(ltc->dsa.g);
	_freeBN(ltc->dsa.x);
	_freeBN(ltc->dsa.y);
	memset(&ltc->dsa, 0, sizeof(ltc->dsa));

	_freeBN(ltc->r);
	_freeBN(ltc->s);

	ecc_free(&ltc->ecdsa);
	memset(&ltc->ecdsa, 0, sizeof(ltc->ecdsa));

    }
}

#undef	_freeBN

static /*@null@*/
void * rpmltcFree(/*@only@*/ void * impl)
	/*@modifies impl @*/
{
    rpmltcClean(impl);
    impl = _free(impl);
    return NULL;
}

static void reg_algs(void)
{
  int err;
#ifdef LTC_RIJNDAEL
  register_cipher (&aes_desc);
#endif
#ifdef LTC_BLOWFISH
  register_cipher (&blowfish_desc);
#endif
#ifdef LTC_XTEA
  register_cipher (&xtea_desc);
#endif
#ifdef LTC_RC5
  register_cipher (&rc5_desc);
#endif
#ifdef LTC_RC6
  register_cipher (&rc6_desc);
#endif
#ifdef LTC_SAFERP
  register_cipher (&saferp_desc);
#endif
#ifdef LTC_TWOFISH
  register_cipher (&twofish_desc);
#endif
#ifdef LTC_SAFER
  register_cipher (&safer_k64_desc);
  register_cipher (&safer_sk64_desc);
  register_cipher (&safer_k128_desc);
  register_cipher (&safer_sk128_desc);
#endif
#ifdef LTC_RC2
  register_cipher (&rc2_desc);
#endif
#ifdef LTC_DES
  register_cipher (&des_desc);
  register_cipher (&des3_desc);
#endif
#ifdef LTC_CAST5
  register_cipher (&cast5_desc);
#endif
#ifdef LTC_NOEKEON
  register_cipher (&noekeon_desc);
#endif
#ifdef LTC_SKIPJACK
  register_cipher (&skipjack_desc);
#endif
#ifdef LTC_KHAZAD
  register_cipher (&khazad_desc);
#endif
#ifdef LTC_ANUBIS
  register_cipher (&anubis_desc);
#endif
#ifdef LTC_KSEED
  register_cipher (&kseed_desc);
#endif
#ifdef LTC_KASUMI
  register_cipher (&kasumi_desc);
#endif
#ifdef LTC_MULTI2
  register_cipher (&multi2_desc);
#endif

#ifdef LTC_TIGER
  register_hash (&tiger_desc);
#endif
#ifdef LTC_MD2
  register_hash (&md2_desc);
#endif
#ifdef LTC_MD4
  register_hash (&md4_desc);
#endif
#ifdef LTC_MD5
  register_hash (&md5_desc);
#endif
#ifdef LTC_SHA1
  register_hash (&sha1_desc);
#endif
#ifdef LTC_SHA224
  register_hash (&sha224_desc);
#endif
#ifdef LTC_SHA256
  register_hash (&sha256_desc);
#endif
#ifdef LTC_SHA384
  register_hash (&sha384_desc);
#endif
#ifdef LTC_SHA512
  register_hash (&sha512_desc);
#endif
#ifdef LTC_RIPEMD128
  register_hash (&rmd128_desc);
#endif
#ifdef LTC_RIPEMD160
  register_hash (&rmd160_desc);
#endif
#ifdef LTC_RIPEMD256
  register_hash (&rmd256_desc);
#endif
#ifdef LTC_RIPEMD320
  register_hash (&rmd320_desc);
#endif
#ifdef LTC_WHIRLPOOL
  register_hash (&whirlpool_desc);
#endif
#ifdef LTC_CHC_HASH
  register_hash(&chc_desc);
  if ((err = chc_register(register_cipher(&aes_desc))) != CRYPT_OK) {
     fprintf(stderr, "chc_register error: %s\n", error_to_string(err));
     exit(EXIT_FAILURE);
  }
#endif

#ifndef LTC_YARROW 
   #error This demo requires Yarrow.
#endif
register_prng(&yarrow_desc);
#ifdef LTC_FORTUNA
register_prng(&fortuna_desc);
#endif
#ifdef LTC_RC4
register_prng(&rc4_desc);
#endif
#ifdef LTC_SOBER128
register_prng(&sober128_desc);
#endif

   if ((err = rng_make_prng(128, find_prng("yarrow"), &yarrow_prng, NULL)) != CRYPT_OK) {
      fprintf(stderr, "rng_make_prng failed: %s\n", error_to_string(err));
      exit(EXIT_FAILURE);
   }
   
}
static
void * rpmltcInit(void)
	/*@*/
{
    static int oneshot;
    rpmltc ltc = xcalloc(1, sizeof(*ltc));
    if (!oneshot) {
	reg_algs();
	ltc_mp = ltm_desc;	/* XXX tfm_desc */
	oneshot++;
    }
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


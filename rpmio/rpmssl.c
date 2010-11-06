/** \ingroup rpmpgp
 * \file rpmio/rpmssl.c
 */

#include "system.h"
#include <rpmlog.h>

#include <rpmiotypes.h>
#define	_RPMPGP_INTERNAL
#if defined(WITH_SSL)

#if defined(__LCLINT__) && !defined(__i386__)
#define	__i386__
#endif

#define	_RPMSSL_INTERNAL
#include <rpmssl.h>
#endif

#include "debug.h"

#if defined(WITH_SSL)

/*@access pgpDig @*/
/*@access pgpDigParams @*/

/*@-redecl@*/
/*@unchecked@*/
extern int _pgp_debug;

/*@unchecked@*/
extern int _pgp_print;
/*@=redecl@*/

/*@unchecked@*/
static int _rpmssl_debug;

#define	SPEW(_t, _rc, _dig)	\
  { if ((_t) || _rpmssl_debug || _pgp_debug < 0) \
	fprintf(stderr, "<-- %s(%p) %s\t%s\n", __FUNCTION__, (_dig), \
		((_rc) ? "OK" : "BAD"), (_dig)->pubkey_algoN); \
  }

static const char * rpmsslHashAlgo2Name(uint32_t algo)
{
    return pgpValStr(pgpHashTbl, (rpmuint8_t)algo);
}

static const char * rpmsslPubkeyAlgo2Name(uint32_t algo)
{
    return pgpValStr(pgpPubkeyTbl, (rpmuint8_t)algo);
}

/**
 * Convert hex to binary nibble.
 * @param c            hex character
 * @return             binary nibble
 */
static
unsigned char nibble(char c)
	/*@*/
{
    if (c >= '0' && c <= '9')
	return (unsigned char) (c - '0');
    if (c >= 'A' && c <= 'F')
	return (unsigned char)((int)(c - 'A') + 10);
    if (c >= 'a' && c <= 'f')
	return (unsigned char)((int)(c - 'a') + 10);
    return (unsigned char) '\0';
}

static unsigned char * rpmsslBN2bin(const char * msg, const BIGNUM * s, size_t maxn)
{
    unsigned char * t = xcalloc(1, maxn);
/*@-modunconnomods@*/
    size_t nt = BN_bn2bin(s, t);
/*@=modunconnomods@*/

    if (nt < maxn) {
	size_t pad = (maxn - nt);
	memmove(t+pad, t, nt);
	memset(t, 0, pad);
    }
    return t;
}

static
int rpmsslSetRSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    rpmssl ssl = dig->impl;
    unsigned int nb = RSA_size(ssl->rsa);
    const char * prefix = rpmDigestASN1(ctx);
    const char * hexstr;
    const char * s;
    rpmuint8_t signhash16[2];
    char * tt;
    int rc;
    int xx;
pgpDigParams pubp = pgpGetPubkey(dig);
dig->pubkey_algoN = rpmsslPubkeyAlgo2Name(pubp->pubkey_algo);
dig->hash_algoN = rpmsslHashAlgo2Name(sigp->hash_algo);

assert(sigp->hash_algo == rpmDigestAlgo(ctx));
    if (prefix == NULL)
	return 1;

/* XXX FIXME: do PKCS1 padding in binary not hex */
/* XXX FIXME: should this lazy free be done elsewhere? */
ssl->digest = _free(ssl->digest);
ssl->digestlen = 0;
    xx = rpmDigestFinal(ctx, (void **)&ssl->digest, &ssl->digestlen, 1);

    hexstr = tt = xmalloc(2 * nb + 1);
    memset(tt, (int) 'f', (2 * nb));
    tt[0] = '0'; tt[1] = '0';
    tt[2] = '0'; tt[3] = '1';
    tt += (2 * nb) - strlen(prefix) - strlen(ssl->digest) - 2;
    *tt++ = '0'; *tt++ = '0';
    tt = stpcpy(tt, prefix);
    tt = stpcpy(tt, ssl->digest);

    /* Set RSA hash. */
/*@-moduncon -noeffectuncon @*/
    xx = BN_hex2bn(&ssl->hm, hexstr);
/*@=moduncon =noeffectuncon @*/

/*@-modfilesys@*/
if (_pgp_debug < 0) fprintf(stderr, "*** hm: %s\n", hexstr);
    hexstr = _free(hexstr);
/*@=modfilesys@*/

    /* Compare leading 16 bits of digest for quick check. */
    s = ssl->digest;
/*@-type@*/
    signhash16[0] = (rpmuint8_t) (nibble(s[0]) << 4) | nibble(s[1]);
    signhash16[1] = (rpmuint8_t) (nibble(s[2]) << 4) | nibble(s[3]);
/*@=type@*/
    rc = memcmp(signhash16, sigp->signhash16, sizeof(sigp->signhash16));
SPEW(0, !rc, dig);
    return rc;
}

static
int rpmsslVerifyRSA(pgpDig dig)
	/*@*/
{
    rpmssl ssl = dig->impl;
/*@-moduncon@*/
    size_t maxn;
    unsigned char * hm;
    unsigned char *  c;
    size_t nb;
/*@=moduncon@*/
    size_t i;
    int rc = 0;
    int xx;

assert(ssl->rsa);	/* XXX ensure lazy malloc with parameter set. */
    maxn = BN_num_bytes(ssl->rsa->n);
    hm = rpmsslBN2bin("hm", ssl->hm, maxn);
    c = rpmsslBN2bin(" c", ssl->c, maxn);
    nb = RSA_public_decrypt((int)maxn, c, c, ssl->rsa, RSA_PKCS1_PADDING);

/*@=moduncon@*/
    /* Verify RSA signature. */
    /* XXX This is _NOT_ the correct openssl function to use:
     *	rc = RSA_verify(type, m, m_len, sigbuf, siglen, ssl->rsa)
     *
     * Here's what needs doing (from OpenPGP reference sources in 1999):
     *	static u32_t checkrsa(BIGNUM * a, RSA * key, u8_t * hash, int hlen)
     *	{
     *	  u8_t dbuf[MAXSIGM];
     *	  int j, ll;
     *
     *	  j = BN_bn2bin(a, dbuf);
     *	  ll = BN_num_bytes(key->n);
     *	  while (j < ll)
     *	    memmove(&dbuf[1], dbuf, j++), dbuf[0] = 0;
     *	  j = RSA_public_decrypt(ll, dbuf, dbuf, key, RSA_PKCS1_PADDING);
     *	  RSA_free(key);
     *	  return (j != hlen || memcmp(dbuf, hash, j));
     *	}
     */
    for (i = 2; i < maxn; i++) {
	if (hm[i] == 0xff)
	    continue;
	i++;
	break;
    }

    rc = ((maxn - i) == nb && (xx = memcmp(hm+i, c, nb)) == 0);

    c = _free(c);
    hm = _free(hm);

SPEW(!rc, rc, dig);
    return rc;
}

static
int rpmsslSignRSA(pgpDig dig)
	/*@*/
{
    rpmssl ssl = dig->impl;
    int rc = 0;		/* assume failure. */
    unsigned char *  c = NULL;
    unsigned char * hm = NULL;
    size_t maxn;
int xx;

#ifdef	DYING
assert(ssl->rsa);	/* XXX ensure lazy malloc with parameter set. */
#else
if (ssl->rsa == NULL) return rc;
#endif

    maxn = RSA_size(ssl->rsa);
assert(ssl->hm);
    hm = rpmsslBN2bin("hm", ssl->hm, maxn);

    c = xmalloc(maxn);
    xx = RSA_private_encrypt((int)maxn, hm, c, ssl->rsa, RSA_NO_PADDING);
    ssl->c = BN_bin2bn(c, maxn, NULL);

    c = _free(c);
    hm = _free(hm);

    rc = (ssl->c != NULL);

SPEW(!rc, rc, dig);

    return rc;
}

static
int rpmsslGenerateRSA(pgpDig dig)
	/*@*/
{
    rpmssl ssl = dig->impl;
    int rc = 0;		/* assume failure. */
static unsigned long _e = 0x10001;

if (ssl->nbits == 0) ssl->nbits = 1024;	/* XXX FIXME */
assert(ssl->nbits);

#if defined(HAVE_RSA_GENERATE_KEY_EX)
    {	BIGNUM * bn = BN_new();
assert(bn);
	if ((ssl->rsa = RSA_new()) != NULL
	 && BN_set_word(bn, _e)
	 && RSA_generate_key_ex(ssl->rsa, ssl->nbits, bn, NULL))
	    rc = 1;
	if (bn) BN_free(bn);
    }
#else
    /* XXX older & deprecated API in openssl-0.97a (Centos4/s390x) */
    if ((ssl->rsa = RSA_generate_key(ssl->nbits, _e, NULL, NULL)) != NULL)
	rc = 1;
#endif

    if (!rc && ssl->rsa) {
	RSA_free(ssl->rsa);
	ssl->rsa = NULL;
    }

SPEW(!rc, rc, dig);

    return rc;
}

static
int rpmsslSetDSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    rpmssl ssl = dig->impl;
    int rc;
    int xx;
pgpDigParams pubp = pgpGetPubkey(dig);
dig->pubkey_algoN = rpmsslPubkeyAlgo2Name(pubp->pubkey_algo);
dig->hash_algoN = rpmsslHashAlgo2Name(sigp->hash_algo);

assert(sigp->hash_algo == rpmDigestAlgo(ctx));
    /* Set DSA hash. */
/* XXX FIXME: should this lazy free be done elsewhere? */
ssl->digest = _free(ssl->digest);
ssl->digestlen = 0;
    xx = rpmDigestFinal(ctx, (void **)&ssl->digest, &ssl->digestlen, 0);

    /* Compare leading 16 bits of digest for quick check. */
    rc = memcmp(ssl->digest, sigp->signhash16, sizeof(sigp->signhash16));
SPEW(0, !rc, dig);
    return rc;
}

static
int rpmsslVerifyDSA(pgpDig dig)
	/*@*/
{
    rpmssl ssl = dig->impl;
    int rc;

assert(ssl->dsa);	/* XXX ensure lazy malloc with parameter set. */
    /* Verify DSA signature. */
    rc = DSA_do_verify(ssl->digest, (int)ssl->digestlen, ssl->dsasig, ssl->dsa);
    rc = (rc == 1);

SPEW(!rc, rc, dig);
    return rc;
}

static
int rpmsslSignDSA(pgpDig dig)
	/*@*/
{
    rpmssl ssl = dig->impl;
    int rc = 0;		/* assume failure */

#ifdef	DYING
assert(ssl->dsa);	/* XXX ensure lazy malloc with parameter set. */
#else
if (ssl->dsa == NULL) return rc;
#endif

    ssl->dsasig = DSA_do_sign(ssl->digest, ssl->digestlen, ssl->dsa);
    rc = (ssl->dsasig != NULL);

SPEW(!rc, rc, dig);

    return rc;
}

static
int rpmsslGenerateDSA(pgpDig dig)
	/*@*/
{
    rpmssl ssl = dig->impl;
    int rc = 0;		/* assume failure. */

if (ssl->nbits == 0) ssl->nbits = 1024;	/* XXX FIXME */
assert(ssl->nbits);

#if defined(HAVE_DSA_GENERATE_PARAMETERS_EX)
    if ((ssl->dsa = DSA_new()) != NULL
     && DSA_generate_parameters_ex(ssl->dsa, ssl->nbits,
		NULL, 0, NULL, NULL, NULL)
     && DSA_generate_key(ssl->dsa))
	rc = 1;
#else
    /* XXX older & deprecated API in openssl-0.97a (Centos4/s390x) */
    if ((ssl->dsa = DSA_generate_parameters(ssl->nbits,
		NULL, 0, NULL, NULL, NULL, NULL)) != NULL
     && DSA_generate_key(ssl->dsa))
	rc = 1;
#endif

    if (!rc && ssl->dsa) {
	DSA_free(ssl->dsa);
	ssl->dsa = NULL;
    }

SPEW(!rc, rc, dig);

    return rc;
}

static
int rpmsslSetELG(/*@only@*/ DIGEST_CTX ctx, /*@unused@*/pgpDig dig, pgpDigParams sigp)
	/*@*/
{
    rpmssl ssl = dig->impl;
    int rc = 1;		/* XXX always fail. */
    int xx;

assert(sigp->hash_algo == rpmDigestAlgo(ctx));

/* XXX FIXME: should this lazy free be done elsewhere? */
ssl->digest = _free(ssl->digest);
ssl->digestlen = 0;
    xx = rpmDigestFinal(ctx, (void **)&ssl->digest, &ssl->digestlen, 0);

    /* Compare leading 16 bits of digest for quick check. */
rc = 0;

SPEW(rc, !rc, dig);
    return rc;
}

static
int rpmsslSetECDSA(/*@only@*/ DIGEST_CTX ctx, /*@unused@*/pgpDig dig, pgpDigParams sigp)
	/*@*/
{
    rpmssl ssl = dig->impl;
    int rc = 1;		/* assume failure. */
    int xx;

assert(sigp->hash_algo == rpmDigestAlgo(ctx));

/* XXX FIXME: should this lazy free be done elsewhere? */
ssl->digest = _free(ssl->digest);
ssl->digestlen = 0;
    xx = rpmDigestFinal(ctx, &ssl->digest, &ssl->digestlen, 0);

    /* Compare leading 16 bits of digest for quick check. */
rc = 0;

SPEW(rc, !rc, dig);
    return rc;
}

static
int rpmsslVerifyECDSA(pgpDig dig)
	/*@*/
{
    int rc = 0;		/* assume failure. */

#if !defined(OPENSSL_NO_ECDSA)
    rpmssl ssl = dig->impl;
    rc = ECDSA_do_verify(ssl->digest, ssl->digestlen, ssl->ecdsasig, ssl->ecdsakey);
#endif
    rc = (rc == 1);

SPEW(!rc, rc, dig);
    return rc;
}

static
int rpmsslSignECDSA(pgpDig dig)
	/*@*/
{
    int rc = 0;		/* assume failure. */

#if !defined(OPENSSL_NO_ECDSA)
    rpmssl ssl = dig->impl;
    ssl->ecdsasig = ECDSA_do_sign(ssl->digest, ssl->digestlen, ssl->ecdsakey);
    rc = (ssl->ecdsasig != NULL);
#endif

SPEW(!rc, rc, dig);
    return rc;
}

static
int rpmsslGenerateECDSA(pgpDig dig)
	/*@*/
{
    int rc = 0;		/* assume failure. */

#if !defined(OPENSSL_NO_ECDSA)
    rpmssl ssl = dig->impl;

if (ssl->nid == 0) {		/* XXX FIXME */
ssl->nid = NID_X9_62_prime256v1;
ssl->nbits = 256;
}

    if ((ssl->ecdsakey = EC_KEY_new_by_curve_name(ssl->nid)) != NULL
     && EC_KEY_generate_key(ssl->ecdsakey))
        rc = 1;
#endif

SPEW(!rc, rc, dig);
    return rc;
}

static int rpmsslErrChk(pgpDig dig, const char * msg, int rc, unsigned expected)
{
#ifdef	NOTYET
rpmgc gc = dig->impl;
    /* Was the return code the expected result? */
    rc = (gcry_err_code(gc->err) != expected);
    if (rc)
	fail("%s failed: %s\n", msg, gpg_strerror(gc->err));
/* XXX FIXME: rpmsslStrerror */
#else
    rc = (rc == 0);	/* XXX impedance match 1 -> 0 on success */
#endif
    return rc;	/* XXX 0 on success */
}

static int rpmsslAvailableCipher(pgpDig dig, int algo)
{
    int rc = 0;	/* assume available */
#ifdef	NOTYET
    rc = rpmgsslvailable(dig->impl, algo,
    	(gcry_md_test_algo(algo) || algo == PGPHASHALGO_MD5));
#endif	/* _RPMGC_INTERNAL */
    return rc;
}

static int rpmsslAvailableDigest(pgpDig dig, int algo)
{
    int rc = 0;	/* assume available */
#ifdef	NOTYET
    rc = rpmgsslvailable(dig->impl, algo,
    	(gcry_md_test_algo(algo) || algo == PGPHASHALGO_MD5));
#endif	/* _RPMGC_INTERNAL */
    return rc;
}

static int rpmsslAvailablePubkey(pgpDig dig, int algo)
{
    int rc = 0;	/* assume available */
#ifdef	NOTYET
    rc = rpmsslAvailable(dig->impl, algo, gcry_pk_test_algo(algo));
#endif	/* _RPMGC_INTERNAL */
    return rc;
}

static int rpmsslVerify(pgpDig dig)
{
    int rc = 0;		/* assume failure */
pgpDigParams pubp = pgpGetPubkey(dig);
pgpDigParams sigp = pgpGetSignature(dig);
dig->pubkey_algoN = rpmsslPubkeyAlgo2Name(pubp->pubkey_algo);
dig->hash_algoN = rpmsslHashAlgo2Name(sigp->hash_algo);

    switch (pubp->pubkey_algo) {
    default:
	break;
    case PGPPUBKEYALGO_RSA:
	rc = rpmsslVerifyRSA(dig);
	break;
    case PGPPUBKEYALGO_DSA:
	rc = rpmsslVerifyDSA(dig);
	break;
    case PGPPUBKEYALGO_ELGAMAL:
#ifdef	NOTYET
	rc = rpmsslVerifyELG(dig);
#endif
	break;
    case PGPPUBKEYALGO_ECDSA:
	rc = rpmsslVerifyECDSA(dig);
	break;
    }
SPEW(!rc, rc, dig);
    return rc;
}

static int rpmsslSign(pgpDig dig)
{
    int rc = 0;		/* assume failure */
pgpDigParams pubp = pgpGetPubkey(dig);
dig->pubkey_algoN = rpmsslPubkeyAlgo2Name(pubp->pubkey_algo);

    switch (pubp->pubkey_algo) {
    default:
	break;
    case PGPPUBKEYALGO_RSA:
	rc = rpmsslSignRSA(dig);
	break;
    case PGPPUBKEYALGO_DSA:
	rc = rpmsslSignDSA(dig);
	break;
    case PGPPUBKEYALGO_ELGAMAL:
#ifdef	NOTYET
	rc = rpmsslSignELG(dig);
#endif
	break;
    case PGPPUBKEYALGO_ECDSA:
	rc = rpmsslSignECDSA(dig);
	break;
    }
SPEW(!rc, rc, dig);
    return rc;
}

static int rpmsslGenerate(pgpDig dig)
{
    int rc = 0;		/* assume failure */
pgpDigParams pubp = pgpGetPubkey(dig);
dig->pubkey_algoN = rpmsslPubkeyAlgo2Name(pubp->pubkey_algo);

    switch (pubp->pubkey_algo) {
    default:
	break;
    case PGPPUBKEYALGO_RSA:
	rc = rpmsslGenerateRSA(dig);
	break;
    case PGPPUBKEYALGO_DSA:
	rc = rpmsslGenerateDSA(dig);
	break;
    case PGPPUBKEYALGO_ELGAMAL:
#ifdef	NOTYET
	rc = rpmsslGenerateELG(dig);
#endif
	break;
    case PGPPUBKEYALGO_ECDSA:
	rc = rpmsslGenerateECDSA(dig);
	break;
    }
SPEW(!rc, rc, dig);
    return rc;
}

static
int rpmsslMpiItem(/*@unused@*/ const char * pre, pgpDig dig, int itemno,
		const rpmuint8_t * p,
		/*@unused@*/ /*@null@*/ const rpmuint8_t * pend)
	/*@*/
{
    rpmssl ssl = dig->impl;
    unsigned int nb = ((pgpMpiBits(p) + 7) >> 3);
    int rc = 0;

/*@-moduncon@*/
    switch (itemno) {
    default:
assert(0);
    case 50:		/* ECDSA r */
    case 51:		/* ECDSA s */
    case 60:		/* ECDSA curve OID */
    case 61:		/* ECDSA Q */
	break;
    case 10:		/* RSA m**d */
	ssl->c = BN_bin2bn(p+2, nb, ssl->c);
	break;
    case 20:		/* DSA r */
	if (ssl->dsasig == NULL) ssl->dsasig = DSA_SIG_new();
	ssl->dsasig->r = BN_bin2bn(p+2, nb, ssl->dsasig->r);
	break;
    case 21:		/* DSA s */
	if (ssl->dsasig == NULL) ssl->dsasig = DSA_SIG_new();
	ssl->dsasig->s = BN_bin2bn(p+2, nb, ssl->dsasig->s);
	break;
    case 30:		/* RSA n */
	if (ssl->rsa == NULL) ssl->rsa = RSA_new();
	ssl->rsa->n = BN_bin2bn(p+2, nb, ssl->rsa->n);
	break;
    case 31:		/* RSA e */
	if (ssl->rsa == NULL) ssl->rsa = RSA_new();
	ssl->rsa->e = BN_bin2bn(p+2, nb, ssl->rsa->e);
	break;
    case 40:		/* DSA p */
	if (ssl->dsa == NULL) ssl->dsa = DSA_new();
	ssl->dsa->p = BN_bin2bn(p+2, nb, ssl->dsa->p);
	break;
    case 41:		/* DSA q */
	if (ssl->dsa == NULL) ssl->dsa = DSA_new();
	ssl->dsa->q = BN_bin2bn(p+2, nb, ssl->dsa->q);
	break;
    case 42:		/* DSA g */
	if (ssl->dsa == NULL) ssl->dsa = DSA_new();
	ssl->dsa->g = BN_bin2bn(p+2, nb, ssl->dsa->g);
	break;
    case 43:		/* DSA y */
	if (ssl->dsa == NULL) ssl->dsa = DSA_new();
	ssl->dsa->pub_key = BN_bin2bn(p+2, nb, ssl->dsa->pub_key);
	break;
    }
/*@=moduncon@*/
    return rc;
}

/*@-mustmod@*/
static
void rpmsslClean(void * impl)
	/*@modifies impl @*/
{
    rpmssl ssl = impl;
/*@-moduncon@*/
    if (ssl != NULL) {
	ssl->nbits = 0;
	ssl->err = 0;
	ssl->badok = 0;
	ssl->digest = _free(ssl->digest);
	ssl->digestlen = 0;

	if (ssl->dsa)
	    DSA_free(ssl->dsa);
	ssl->dsa = NULL;
	if (ssl->dsasig)
	    DSA_SIG_free(ssl->dsasig);
	ssl->dsasig = NULL;

	if (ssl->rsa)
	    RSA_free(ssl->rsa);
	ssl->rsa = NULL;
	if (ssl->hm)
	    BN_free(ssl->hm);
	ssl->hm = NULL;
	if (ssl->c)
	    BN_free(ssl->c);
	ssl->c = NULL;

#if !defined(OPENSSL_NO_ECDSA)
	if (ssl->ecdsakey)
	    EC_KEY_free(ssl->ecdsakey);
	ssl->ecdsakey = NULL;
	if (ssl->ecdsasig)
	    ECDSA_SIG_free(ssl->ecdsasig);
	ssl->ecdsasig = NULL;

	if (ssl->ecdsakey_bad)
	    EC_KEY_free(ssl->ecdsakey_bad);
	ssl->ecdsakey_bad = NULL;
#endif

    }
/*@=moduncon@*/
}
/*@=mustmod@*/

static /*@null@*/
void * rpmsslFree(/*@only@*/ void * impl)
	/*@modifies impl @*/
{
    rpmsslClean(impl);
    impl = _free(impl);
    return NULL;
}

static
void * rpmsslInit(void)
	/*@*/
{
    rpmssl ssl = xcalloc(1, sizeof(*ssl));
/*@-moduncon@*/
    ERR_load_crypto_strings();
/*@=moduncon@*/
    return (void *) ssl;
}

struct pgpImplVecs_s rpmsslImplVecs = {
	rpmsslSetRSA,
	rpmsslSetDSA,
	rpmsslSetELG,
	rpmsslSetECDSA,

	rpmsslErrChk,
	rpmsslAvailableCipher, rpmsslAvailableDigest, rpmsslAvailablePubkey,
	rpmsslVerify, rpmsslSign, rpmsslGenerate,

	rpmsslMpiItem, rpmsslClean,
	rpmsslFree, rpmsslInit
};

#endif


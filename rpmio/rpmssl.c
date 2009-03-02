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

/*@-modfilesys@*/
static
void hexdump(const char * msg, unsigned char * b, size_t blen)
	/*@*/
{
    static const char hex[] = "0123456789abcdef";

    fprintf(stderr, "*** %s:", msg);
    if (b != NULL)
    while (blen > 0) {
	fprintf(stderr, "%c%c",
		hex[ (unsigned)((*b >> 4) & 0x0f) ],
		hex[ (unsigned)((*b     ) & 0x0f) ]);
	blen--;
	b++;
    }
    fprintf(stderr, "\n");
    return;
}
/*@=modfilesys@*/

static
int rpmsslSetRSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    rpmssl ssl = dig->impl;
    unsigned int nbits = BN_num_bits(ssl->c);
    unsigned int nb = (nbits + 7) >> 3;
    const char * prefix = rpmDigestASN1(ctx);
    const char * hexstr;
    const char * s;
    rpmuint8_t signhash16[2];
    char * tt;
    int xx;

assert(sigp->hash_algo == rpmDigestAlgo(ctx));
    if (prefix == NULL)
	return 1;

    xx = rpmDigestFinal(ctx, (void **)&dig->md5, &dig->md5len, 1);
    hexstr = tt = xmalloc(2 * nb + 1);
    memset(tt, (int) 'f', (2 * nb));
    tt[0] = '0'; tt[1] = '0';
    tt[2] = '0'; tt[3] = '1';
    tt += (2 * nb) - strlen(prefix) - strlen(dig->md5) - 2;
    *tt++ = '0'; *tt++ = '0';
    tt = stpcpy(tt, prefix);
    tt = stpcpy(tt, dig->md5);

    /* Set RSA hash. */
/*@-moduncon -noeffectuncon @*/
    xx = BN_hex2bn(&ssl->rsahm, hexstr);
/*@=moduncon =noeffectuncon @*/

/*@-modfilesys@*/
if (_pgp_debug < 0) fprintf(stderr, "*** rsahm: %s\n", hexstr);
    hexstr = _free(hexstr);
/*@=modfilesys@*/

    /* Compare leading 16 bits of digest for quick check. */
    s = dig->md5;
/*@-type@*/
    signhash16[0] = (rpmuint8_t) (nibble(s[0]) << 4) | nibble(s[1]);
    signhash16[1] = (rpmuint8_t) (nibble(s[2]) << 4) | nibble(s[3]);
/*@=type@*/
    return memcmp(signhash16, sigp->signhash16, sizeof(sigp->signhash16));
}

static unsigned char * rpmsslBN2bin(const char * msg, const BIGNUM * s, size_t maxn)
{
    unsigned char * t = xcalloc(1, maxn);
/*@-modunconnomods@*/
    size_t nt = BN_bn2bin(s, t);
/*@=modunconnomods@*/

    if (nt < maxn) {
	size_t pad = (maxn - nt);
/*@-modfilesys@*/
if (_pgp_debug < 0) fprintf(stderr, "\tmemmove(%p, %p, %u)\n", t+pad, t, (unsigned)nt);
/*@=modfilesys@*/
	memmove(t+pad, t, nt);
/*@-modfilesys@*/
if (_pgp_debug < 0) fprintf(stderr, "\tmemset(%p, 0, %u)\n", t, (unsigned)pad);
/*@=modfilesys@*/
	memset(t, 0, pad);
    }
/*@-modfilesys@*/
if (_pgp_debug < 0) hexdump(msg, t, maxn);
/*@=modfilesys@*/
    return t;
}

static
int rpmsslVerifyRSA(pgpDig dig)
	/*@*/
{
    rpmssl ssl = dig->impl;
/*@-moduncon@*/
    size_t maxn = BN_num_bytes(ssl->rsa->n);
    unsigned char * hm = rpmsslBN2bin("hm", ssl->rsahm, maxn);
    unsigned char *  c = rpmsslBN2bin(" c", ssl->c, maxn);
    size_t nb = RSA_public_decrypt((int)maxn, c, c, ssl->rsa, RSA_PKCS1_PADDING);
/*@=moduncon@*/
    size_t i;
    int rc = 0;
    int xx;

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
/*@-modfilesys@*/
if (_pgp_debug < 0) hexdump("HM", hm + i, (maxn - i));
/*@=modfilesys@*/
	break;
    }

/*@-modfilesys@*/
if (_pgp_debug < 0) hexdump("HM", hm + (maxn - nb), nb);
if (_pgp_debug < 0) hexdump(" C",  c, nb);
/*@=modfilesys@*/

    rc = ((maxn - i) == nb && (xx = memcmp(hm+i, c, nb)) == 0);

    c = _free(c);
    hm = _free(hm);

    return rc;
}

static
int rpmsslSetDSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    int xx;

assert(sigp->hash_algo == rpmDigestAlgo(ctx));
    /* Set DSA hash. */
    xx = rpmDigestFinal(ctx, (void **)&dig->sha1, &dig->sha1len, 0);

    /* Compare leading 16 bits of digest for quick check. */
    return memcmp(dig->sha1, sigp->signhash16, sizeof(sigp->signhash16));
}

static
int rpmsslVerifyDSA(pgpDig dig)
	/*@*/
{
    rpmssl ssl = dig->impl;
    int rc;

    /* Verify DSA signature. */
/*@-moduncon@*/
    rc = (DSA_do_verify(dig->sha1, (int)dig->sha1len, ssl->dsasig, ssl->dsa) == 1);
/*@=moduncon@*/

    return rc;
}


static
int rpmsslSetECDSA(/*@only@*/ DIGEST_CTX ctx, /*@unused@*/pgpDig dig, pgpDigParams sigp)
	/*@*/
{
    int rc = 1;		/* XXX always fail. */
    int xx;

assert(sigp->hash_algo == rpmDigestAlgo(ctx));
    xx = rpmDigestFinal(ctx, (void **)NULL, NULL, 0);

    /* Compare leading 16 bits of digest for quick check. */

    return rc;
}

static
int rpmsslVerifyECDSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    int rc = 0;		/* XXX always fail. */

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
	if (ssl->dsa) {
	    DSA_free(ssl->dsa);
	    ssl->dsa = NULL;
	}
	if (ssl->dsasig) {
	    DSA_SIG_free(ssl->dsasig);
	    ssl->dsasig = NULL;
	}
	if (ssl->rsa) {
	    RSA_free(ssl->rsa);
	    ssl->rsa = NULL;
	}
	if (ssl->c) {
	    BN_free(ssl->c);
	    ssl->c = NULL;
	}
    }
/*@=moduncon@*/
}
/*@=mustmod@*/

static /*@null@*/
void * rpmsslFree(/*@only@*/ void * impl)
	/*@modifies impl @*/
{
    rpmssl ssl = impl;
    rpmsslClean(impl);
    ssl = _free(ssl);
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
	rpmsslSetRSA, rpmsslVerifyRSA,
	rpmsslSetDSA, rpmsslVerifyDSA,
	rpmsslSetECDSA, rpmsslVerifyECDSA,
	rpmsslMpiItem, rpmsslClean,
	rpmsslFree, rpmsslInit
};

#endif


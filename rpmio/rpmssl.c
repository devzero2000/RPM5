/** \ingroup rpmpgp
 * \file rpmio/rpmssl.c
 */

#include "system.h"
#include <rpmio.h>

#define	_RPMPGP_INTERNAL
#define	_RPMSSL_INTERNAL
#include <rpmssl.h>

#include "debug.h"

/*@access pgpDig @*/
/*@access pgpDigParams @*/

/*@-redecl@*/
/*@unchecked@*/
extern int _pgp_debug;

/*@unchecked@*/
extern int _pgp_print;
/*@=redecl@*/

static
int rpmsslSetRSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies ctx, dig @*/
{
    rpmssl ssl = dig->impl;
    unsigned int nbits = 0;	/* WRONG WRONG WRONG */
    unsigned int nb = (nbits + 7) >> 3;
    const char * prefix;
    const char * hexstr;
    const char * s;
    uint8_t signhash16[2];
    char * tt;
    int xx;

    /* XXX Values from PKCS#1 v2.1 (aka RFC-3447) */
    switch (sigp->hash_algo) {
    case PGPHASHALGO_MD5:
	prefix = "3020300c06082a864886f70d020505000410";
	break;
    case PGPHASHALGO_SHA1:
	prefix = "3021300906052b0e03021a05000414";
	break;
    case PGPHASHALGO_RIPEMD160:
	prefix = "3021300906052b2403020105000414";
	break;
    case PGPHASHALGO_MD2:
	prefix = "3020300c06082a864886f70d020205000410";
	break;
    case PGPHASHALGO_TIGER192:
	prefix = "3029300d06092b06010401da470c0205000418";
	break;
    case PGPHASHALGO_HAVAL_5_160:
	prefix = NULL;
	break;
    case PGPHASHALGO_SHA256:
	prefix = "3031300d060960864801650304020105000420";
	break;
    case PGPHASHALGO_SHA384:
	prefix = "3041300d060960864801650304020205000430";
	break;
    case PGPHASHALGO_SHA512:
	prefix = "3051300d060960864801650304020305000440";
	break;
    default:
	prefix = NULL;
	break;
    }
    if (prefix == NULL)
	return 1;

    xx = rpmDigestFinal(ctx, (void **)&dig->md5, &dig->md5len, 0);
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
/*@=moduncon =noeffectuncon @*/

    hexstr = _free(hexstr);

    /* Compare leading 16 bits of digest for quick check. */
    return memcmp(dig->md5, sigp->signhash16, sizeof(sigp->signhash16));
}

static
int rpmsslVerifyRSA(pgpDig dig)
	/*@*/
{
    rpmssl ssl = dig->impl;
    int rc = 0;

    /* Verify RSA signature. */
/*@-moduncon@*/
#if 0
    rc = RSA_verify(type, m, m_len, sigbuf, siglen, ssl->rsa)
#endif
/*@=moduncon@*/

    return rc;
}

static
int rpmsslSetDSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies ctx, dig @*/
{
    int xx;

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
    rc = (DSA_do_verify(dig->sha1, dig->sha1len, ssl->dsasig, ssl->dsa) == 1);
/*@=moduncon@*/

    return rc;
}

static
int rpmsslMpiItem(const char * pre, pgpDig dig, int itemno,
		const uint8_t * p, /*@null@*/ const uint8_t * pend)
	/*@globals fileSystem @*/
	/*@modifies dig, fileSystem @*/
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

static
void rpmsslClean(void * impl)
	/*@modifies impl @*/
{
    rpmssl ssl = impl;
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
}

static
void * rpmsslFree(/*@only@*/ void * impl)
	/*@modifies impl @*/
{
    rpmssl ssl = impl;
    if (ssl != NULL) {
	ssl = _free(ssl);
    }
    return NULL;
}

static
void * rpmsslInit(void)
	/*@*/
{
    rpmssl ssl = xcalloc(1, sizeof(*ssl));
    ERR_load_crypto_strings();
    return (void *) ssl;
}

struct pgpImplVecs_s rpmsslImplVecs = {
	rpmsslSetRSA, rpmsslVerifyRSA,
	rpmsslSetDSA, rpmsslVerifyDSA,
	rpmsslMpiItem, rpmsslClean,
	rpmsslFree, rpmsslInit
};

/** \ingroup rpmpgp
 * \file rpmio/rpmbc.c
 */

#include "system.h"
#define	_RPMBC_INTERNAL
#define	_RPMPGP_INTERNAL
#include <rpmbc.h>
#include "debug.h"

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

static
int rpmbcSetRSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    rpmbc bc = dig->impl;
    unsigned int nbits = (unsigned) MP_WORDS_TO_BITS(bc->c.size);
    unsigned int nb = (nbits + 7) >> 3;
    const char * prefix = rpmDigestASN1(ctx);
    const char * hexstr;
    char * tt;
    int rc;
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

/*@-moduncon -noeffectuncon @*/
    mpnzero(&bc->rsahm);   (void) mpnsethex(&bc->rsahm, hexstr);
/*@=moduncon =noeffectuncon @*/

    hexstr = _free(hexstr);

    /* Compare leading 16 bits of digest for quick check. */
    {	const char *str = dig->md5;
	rpmuint8_t s[2];
	const rpmuint8_t *t = sigp->signhash16;
	s[0] = (rpmuint8_t) (nibble(str[0]) << 4) | nibble(str[1]);
	s[1] = (rpmuint8_t) (nibble(str[2]) << 4) | nibble(str[3]);
	rc = memcmp(s, t, sizeof(sigp->signhash16));
#ifdef	DYING
	if (rc != 0)
	    fprintf(stderr, "*** hash fails: digest(%02x%02x) != signhash(%02x%02x)\n",
		s[0], s[1], t[0], t[1]);
#endif
    }
    return rc;
}

static
int rpmbcVerifyRSA(pgpDig dig)
	/*@*/
{
    rpmbc bc = dig->impl;
    int rc;

/*@-moduncon@*/
#if defined(HAVE_BEECRYPT_API_H)
	rc = rsavrfy(&bc->rsa_pk.n, &bc->rsa_pk.e, &bc->c, &bc->rsahm);
#else
	rc = rsavrfy(&bc->rsa_pk, &bc->rsahm, &bc->c);
#endif
/*@=moduncon@*/

    return rc;
}

static
int rpmbcSetDSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    rpmbc bc = dig->impl;
    rpmuint8_t signhash16[2];
    int xx;

assert(sigp->hash_algo == rpmDigestAlgo(ctx));
    xx = rpmDigestFinal(ctx, (void **)&dig->sha1, &dig->sha1len, 1);

/*@-moduncon -noeffectuncon @*/
    mpnzero(&bc->hm);	(void) mpnsethex(&bc->hm, dig->sha1);
/*@=moduncon =noeffectuncon @*/

    /* Compare leading 16 bits of digest for quick check. */
    signhash16[0] = (rpmuint8_t)((*bc->hm.data >> 24) & 0xff);
    signhash16[1] = (rpmuint8_t)((*bc->hm.data >> 16) & 0xff);
    return memcmp(signhash16, sigp->signhash16, sizeof(signhash16));
}

static
int rpmbcVerifyDSA(pgpDig dig)
	/*@*/
{
    rpmbc bc = dig->impl;
    int rc;

/*@-moduncon@*/
    rc = dsavrfy(&bc->p, &bc->q, &bc->g, &bc->hm, &bc->y, &bc->r, &bc->s);
/*@=moduncon@*/

    return rc;
}

static
int rpmbcSetECDSA(/*@only@*/ DIGEST_CTX ctx, /*@unused@*/pgpDig dig, pgpDigParams sigp)
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
int rpmbcVerifyECDSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    int rc = 0;		/* XXX always fail. */

    return rc;
}

/**
 */
static /*@observer@*/
const char * pgpMpiHex(const rpmuint8_t *p)
        /*@*/
{
    static char prbuf[2048];
    char *t = prbuf;
    t = pgpHexCvt(t, p+2, pgpMpiLen(p)-2);
    return prbuf;
}

/**
 * @return		0 on success
 */
static
int pgpMpiSet(const char * pre, unsigned int lbits,
		/*@out@*/ void * dest, const rpmuint8_t * p,
		/*@null@*/ const rpmuint8_t * pend)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    mpnumber * mpn = dest;
    unsigned int mbits = pgpMpiBits(p);
    unsigned int nbits;
    unsigned int nbytes;
    char * t;
    unsigned int ix;

    if (pend != NULL && (p + ((mbits+7) >> 3)) > pend)
	return 1;

    if (mbits > lbits)
	return 1;

    nbits = (lbits > mbits ? lbits : mbits);
    nbytes = ((nbits + 7) >> 3);
    t = xmalloc(2*nbytes+1);
    ix = 2 * ((nbits - mbits) >> 3);

if (_pgp_debug)
fprintf(stderr, "*** mbits %u nbits %u nbytes %u t %p[%d] ix %u\n", mbits, nbits, nbytes, t, (2*nbytes+1), ix);
    if (ix > 0) memset(t, (int)'0', ix);
    strcpy(t+ix, (const char *) pgpMpiHex(p));
if (_pgp_debug)
fprintf(stderr, "*** %s %s\n", pre, t);
    (void) mpnsethex(mpn, t);
    t = _free(t);
if (_pgp_debug && _pgp_print)
fprintf(stderr, "\t %s ", pre), mpfprintln(stderr, mpn->size, mpn->data);
    return 0;
}

static
int rpmbcMpiItem(const char * pre, pgpDig dig, int itemno,
		const rpmuint8_t * p, /*@null@*/ const rpmuint8_t * pend)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    rpmbc bc = dig->impl;
    int rc = 0;

    switch (itemno) {
    default:
assert(0);
	break;
    case 10:		/* RSA m**d */
	(void) mpnsethex(&bc->c, pgpMpiHex(p));
if (_pgp_debug && _pgp_print)
fprintf(stderr, "\t %s ", pre),  mpfprintln(stderr, bc->c.size, bc->c.data);
	break;
    case 20:		/* DSA r */
	rc = pgpMpiSet(pre, 160, &bc->r, p, pend);
	break;
    case 21:		/* DSA s */
	rc = pgpMpiSet(pre, 160, &bc->s, p, pend);
	break;
    case 30:		/* RSA n */
	(void) mpbsethex(&bc->rsa_pk.n, pgpMpiHex(p));
if (_pgp_debug && _pgp_print)
fprintf(stderr, "\t %s ", pre),  mpfprintln(stderr, bc->rsa_pk.n.size, bc->rsa_pk.n.modl);
	break;
    case 31:		/* RSA e */
	(void) mpnsethex(&bc->rsa_pk.e, pgpMpiHex(p));
if (_pgp_debug && _pgp_print)
fprintf(stderr, "\t %s ", pre),  mpfprintln(stderr, bc->rsa_pk.e.size, bc->rsa_pk.e.data);
	break;
    case 40:		/* DSA p */
	(void) mpbsethex(&bc->p, pgpMpiHex(p));
if (_pgp_debug && _pgp_print)
fprintf(stderr, "\t %s ", pre),  mpfprintln(stderr, bc->p.size, bc->p.modl);
	break;
    case 41:		/* DSA q */
	(void) mpbsethex(&bc->q, pgpMpiHex(p));
if (_pgp_debug && _pgp_print)
fprintf(stderr, "\t %s ", pre),  mpfprintln(stderr, bc->q.size, bc->q.modl);
	break;
    case 42:		/* DSA g */
	(void) mpnsethex(&bc->g, pgpMpiHex(p));
if (_pgp_debug && _pgp_print)
fprintf(stderr, "\t %s ", pre),  mpfprintln(stderr, bc->g.size, bc->g.data);
	break;
    case 43:		/* DSA y */
	(void) mpnsethex(&bc->y, pgpMpiHex(p));
if (_pgp_debug && _pgp_print)
fprintf(stderr, "\t %s ", pre),  mpfprintln(stderr, bc->y.size, bc->y.data);
	break;
    }
    return rc;
}

/*@-mustmod@*/
static
void rpmbcClean(void * impl)
	/*@modifies impl @*/
{
    rpmbc bc = impl;
    if (bc != NULL) {
	mpnfree(&bc->hm);
	mpnfree(&bc->r);
	mpnfree(&bc->s);
	(void) rsapkFree(&bc->rsa_pk);
	mpnfree(&bc->m);
	mpnfree(&bc->c);
	mpnfree(&bc->rsahm);
    }
}
/*@=mustmod@*/

static /*@null@*/
void * rpmbcFree(/*@only@*/ void * impl)
	/*@modifies impl @*/
{
    rpmbc bc = impl;
    if (bc != NULL) {
	mpbfree(&bc->p);
	mpbfree(&bc->q);
	mpnfree(&bc->g);
	mpnfree(&bc->y);
	mpnfree(&bc->hm);
	mpnfree(&bc->r);
	mpnfree(&bc->s);

	mpbfree(&bc->rsa_pk.n);
	mpnfree(&bc->rsa_pk.e);
	mpnfree(&bc->m);
	mpnfree(&bc->c);
	mpnfree(&bc->hm);
	bc = _free(bc);
    }
    return NULL;
}

static
void * rpmbcInit(void)
	/*@*/
{
    rpmbc bc = xcalloc(1, sizeof(*bc));
    return (void *) bc;
}

struct pgpImplVecs_s rpmbcImplVecs = {
	rpmbcSetRSA, rpmbcVerifyRSA,
	rpmbcSetDSA, rpmbcVerifyDSA,
	rpmbcSetECDSA, rpmbcVerifyECDSA,
	rpmbcMpiItem, rpmbcClean,
	rpmbcFree, rpmbcInit
};

/** \ingroup rpmpgp
 * \file rpmio/rpmbc.c
 */

#include "system.h"
#include <rpmio.h>
#include <rpmbc.h>
#include "debug.h"

/*@unchecked@*/
extern int _pgp_debug;

/*@unchecked@*/
extern int _pgp_print;

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

int pgpSetRSA(DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
{
    unsigned int nbits = (unsigned) MP_WORDS_TO_BITS(dig->c.size);
    unsigned int nb = (nbits + 7) >> 3;
    const char * prefix;
    const char * hexstr;
    const char * s;
    byte signhash16[2];
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
    mpnzero(&dig->rsahm);   (void) mpnsethex(&dig->rsahm, hexstr);
/*@=moduncon =noeffectuncon @*/

    hexstr = _free(hexstr);

    /* Compare leading 16 bits of digest for quick check. */
    s = dig->md5;
/*@-type@*/
    signhash16[0] = (byte) (nibble(s[0]) << 4) | nibble(s[1]);
    signhash16[1] = (byte) (nibble(s[2]) << 4) | nibble(s[3]);
/*@=type@*/
    return memcmp(signhash16, sigp->signhash16, sizeof(signhash16));
}

int pgpVerifyRSA(pgpDig dig)
{
    int rc;

/*@-moduncon@*/
#if defined(HAVE_BEECRYPT_API_H)
	rc = rsavrfy(&dig->rsa_pk.n, &dig->rsa_pk.e, &dig->c, &dig->rsahm);
#else
	rc = rsavrfy(&dig->rsa_pk, &dig->rsahm, &dig->c);
#endif
/*@=moduncon@*/

    return rc;
}

int pgpSetDSA(DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
{
    byte signhash16[2];
    int xx;

    xx = rpmDigestFinal(ctx, (void **)&dig->sha1, &dig->sha1len, 1);

/*@-moduncon -noeffectuncon @*/
    mpnzero(&dig->hm);	(void) mpnsethex(&dig->hm, dig->sha1);
/*@=moduncon =noeffectuncon @*/

    /* Compare leading 16 bits of digest for quick check. */
    signhash16[0] = (*dig->hm.data >> 24) & 0xff;
    signhash16[1] = (*dig->hm.data >> 16) & 0xff;
    return memcmp(signhash16, sigp->signhash16, sizeof(signhash16));
}

int pgpVerifyDSA(pgpDig dig)
{
    int rc;

/*@-moduncon@*/
    rc = dsavrfy(&dig->p, &dig->q, &dig->g, &dig->hm, &dig->y, &dig->r, &dig->s);
/*@=moduncon@*/

    return rc;
}

/**
 */
static /*@observer@*/
const char * pgpMpiHex(const byte *p)
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
int pgpMpiSet(const char * pre, int lbits,
		/*@out@*/ void * dest, const byte * p, const byte * pend)
	/*@globals fileSystem @*/
	/*@modifies dest, fileSystem @*/
{
    mpnumber * mpn = dest;
    unsigned int mbits = pgpMpiBits(p);
    unsigned int nbits;
    unsigned int nbytes;
    char * t;
    unsigned int ix;

    if ((p + ((mbits+7) >> 3)) > pend)
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

int pgpMpiItem(const char * pre, pgpDig dig, int itemno,
		const byte * p, const byte * pend)
{
    int rc = 0;
    switch (itemno) {
    default:
assert(0);
	break;
    case 10:
	(void) mpnsethex(&dig->c, pgpMpiHex(p));
if (_pgp_debug && _pgp_print)
fprintf(stderr, "\t %s ", pre),  mpfprintln(stderr, dig->c.size, dig->c.data);
	break;
    case 20:
	rc = pgpMpiSet(pre, 160, &dig->r, p, pend);
	break;
    case 21:
	rc = pgpMpiSet(pre, 160, &dig->s, p, pend);
	break;
    case 30:
	(void) mpbsethex(&dig->rsa_pk.n, pgpMpiHex(p));
if (_pgp_debug && _pgp_print)
fprintf(stderr, "\t %s ", pre),  mpfprintln(stderr, dig->rsa_pk.n.size, dig->rsa_pk.n.modl);
	break;
    case 31:
	(void) mpnsethex(&dig->rsa_pk.e, pgpMpiHex(p));
if (_pgp_debug && _pgp_print)
fprintf(stderr, "\t %s ", pre),  mpfprintln(stderr, dig->rsa_pk.e.size, dig->rsa_pk.e.data);
	break;
    case 40:
	(void) mpbsethex(&dig->p, pgpMpiHex(p));
if (_pgp_debug && _pgp_print)
fprintf(stderr, "\t %s ", pre),  mpfprintln(stderr, dig->p.size, dig->p.modl);
	break;
    case 41:
	(void) mpbsethex(&dig->q, pgpMpiHex(p));
if (_pgp_debug && _pgp_print)
fprintf(stderr, "\t %s ", pre),  mpfprintln(stderr, dig->q.size, dig->q.modl);
	break;
    case 42:
	(void) mpnsethex(&dig->g, pgpMpiHex(p));
if (_pgp_debug && _pgp_print)
fprintf(stderr, "\t %s ", pre),  mpfprintln(stderr, dig->g.size, dig->g.data);
	break;
    case 43:
	(void) mpnsethex(&dig->y, pgpMpiHex(p));
if (_pgp_debug && _pgp_print)
fprintf(stderr, "\t %s ", pre),  mpfprintln(stderr, dig->y.size, dig->y.data);
	break;
    }
    return rc;
}

void rpmbcClean(pgpDig dig)
{
    if (dig != NULL) {
	mpnfree(&dig->hm);
	mpnfree(&dig->r);
	mpnfree(&dig->s);
	(void) rsapkFree(&dig->rsa_pk);
	mpnfree(&dig->m);
	mpnfree(&dig->c);
	mpnfree(&dig->rsahm);
    }
}

void rpmbcFree(pgpDig dig)
{
    if (dig != NULL) {
	mpbfree(&dig->p);
	mpbfree(&dig->q);
	mpnfree(&dig->g);
	mpnfree(&dig->y);
	mpnfree(&dig->hm);
	mpnfree(&dig->r);
	mpnfree(&dig->s);

	mpbfree(&dig->rsa_pk.n);
	mpnfree(&dig->rsa_pk.e);
	mpnfree(&dig->m);
	mpnfree(&dig->c);
	mpnfree(&dig->hm);
    }
}

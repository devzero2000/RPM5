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

static const char * _pgpHashAlgo2Name(uint32_t algo)
{
    return pgpValStr(pgpHashTbl, (rpmuint8_t)algo);
}

static const char * _pgpPubkeyAlgo2Name(uint32_t algo)
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
	rc = rsavrfy(&bc->rsa_pk.n, &bc->rsa_pk.e, &bc->c, &bc->rsahm);
/*@=moduncon@*/

    return rc;
}

#ifdef	NOTYET
static
int rpmbcSignRSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    int rc = 0;		/* XXX always fail. */

    return rc;
}

static
int rpmbcGenerateRSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    int rc = 0;		/* XXX always fail. */

    return rc;
}
#endif

static
int rpmbcSetDSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    rpmbc bc = dig->impl;
    rpmuint8_t signhash16[2];
    int xx;

assert(sigp->hash_algo == rpmDigestAlgo(ctx));
    xx = rpmDigestFinal(ctx, (void **)&dig->sha1, &dig->sha1len, 1);

    {	char * hm = dig->sha1;
	char lastc = hm[40];
	/* XXX Truncate to 160bits. */
	hm[40] = '\0';
/*@-moduncon -noeffectuncon @*/
	mpnzero(&bc->hm);	(void) mpnsethex(&bc->hm, hm);
/*@=moduncon =noeffectuncon @*/
	hm[40] = lastc;
    }

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
    rc = dsavrfy(&bc->dsa_keypair.param.p, &bc->dsa_keypair.param.q, &bc->dsa_keypair.param.g, &bc->hm, &bc->dsa_keypair.y, &bc->r, &bc->s);
/*@=moduncon@*/

    return rc;
}

#ifdef	NOTYET
static
int rpmbcSignDSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    int rc = 0;		/* XXX always fail. */

    return rc;
}

static
int rpmbcGenerateDSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    int rc = 0;		/* XXX always fail. */

    return rc;
}
#endif

static
int rpmbcSetDUMMY(/*@only@*/ DIGEST_CTX ctx, /*@unused@*/pgpDig dig, pgpDigParams sigp)
	/*@*/
{
    int rc = 1;		/* XXX always fail. */
    int xx;

assert(sigp->hash_algo == rpmDigestAlgo(ctx));
    xx = rpmDigestFinal(ctx, (void **)NULL, NULL, 0);

    /* Compare leading 16 bits of digest for quick check. */

    return rc;
}

static int rpmbcErrChk(pgpDig dig, const char * msg, int rc, unsigned expected)
{
#ifdef	NOTYET
rpmgc gc = dig->impl;
    /* Was the return code the expected result? */
    rc = (gcry_err_code(gc->err) != expected);
    if (rc)
	fail("%s failed: %s\n", msg, gpg_strerror(gc->err));
#endif
/* XXX FIXME: rpmbcStrerror */
    return rc;	/* XXX 0 on success */
}

static int rpmbcAvailableCipher(pgpDig dig, int algo)
{
    int rc = 0;	/* assume available */
#ifdef	NOTYET
    rc = rpmgbcvailable(dig->impl, algo,
    	(gcry_md_test_algo(algo) || algo == PGPHASHALGO_MD5));
#endif
    return rc;
}

static int rpmbcAvailableDigest(pgpDig dig, int algo)
{
    int rc = 0;	/* assume available */
#ifdef	NOTYET
    rc = rpmgbcvailable(dig->impl, algo,
    	(gcry_md_test_algo(algo) || algo == PGPHASHALGO_MD5));
#endif
    return rc;
}

static int rpmbcAvailablePubkey(pgpDig dig, int algo)
{
    int rc = 0;	/* assume available */
#ifdef	NOTYET
    rc = rpmbcAvailable(dig->impl, algo, gcry_pk_test_algo(algo));
#endif
    return rc;
}

static int rpmbcVerify(pgpDig dig)
{
    int rc = 0;		/* assume failure */
pgpDigParams pubp = pgpGetPubkey(dig);
pgpDigParams sigp = pgpGetSignature(dig);
dig->pubkey_algoN = _pgpPubkeyAlgo2Name(pubp->pubkey_algo);
dig->hash_algoN = _pgpHashAlgo2Name(sigp->hash_algo);

    switch (pubp->pubkey_algo) {
    default:
	break;
    case PGPPUBKEYALGO_RSA:
	rc = rpmbcVerifyRSA(dig);
	break;
    case PGPPUBKEYALGO_DSA:
	rc = rpmbcVerifyDSA(dig);
	break;
    case PGPPUBKEYALGO_ELGAMAL:
#ifdef	NOTYET
	rc = rpmbcVerifyELG(dig);
#endif
	break;
    case PGPPUBKEYALGO_ECDSA:
#ifdef	NOTYET
	rc = rpmbcVerifyECDSA(dig);
#endif
	break;
    }
if (_pgp_debug < 0)
fprintf(stderr, "<-- %s(%p) rc %d\t%s\n", __FUNCTION__, dig, rc, dig->pubkey_algoN);
    return rc;
}

static int rpmbcSign(pgpDig dig)
{
    int rc = 0;		/* assume failure */
pgpDigParams pubp = pgpGetPubkey(dig);
    switch (pubp->pubkey_algo) {
    default:
	break;
    case PGPPUBKEYALGO_RSA:
#ifdef	NOTYET
	rc = rpmbcSignRSA(dig);
#endif
	break;
    case PGPPUBKEYALGO_DSA:
#ifdef	NOTYET
	rc = rpmbcSignDSA(dig);
#endif
	break;
    case PGPPUBKEYALGO_ELGAMAL:
#ifdef	NOTYET
	rc = rpmbcSignELG(dig);
#endif
	break;
    case PGPPUBKEYALGO_ECDSA:
#ifdef	NOTYET
	rc = rpmbcSignECDSA(dig);
#endif
	break;
    }
if (1 || _pgp_debug < 0)
fprintf(stderr, "<-- %s(%p) rc %d\t%s\n", __FUNCTION__, dig, rc, dig->pubkey_algoN);
    return rc;
}

static int rpmbcGenerate(pgpDig dig)
{
    int rc = 0;		/* assume failure */
pgpDigParams pubp = pgpGetPubkey(dig);
    switch (pubp->pubkey_algo) {
    default:
	break;
    case PGPPUBKEYALGO_RSA:
#ifdef	NOTYET
	rc = rpmbcGenerateRSA(dig);
#endif
	break;
    case PGPPUBKEYALGO_DSA:
#ifdef	NOTYET
	rc = rpmbcGenerateDSA(dig);
#endif
	break;
    case PGPPUBKEYALGO_ELGAMAL:
#ifdef	NOTYET
	rc = rpmbcGenerateELG(dig);
#endif
	break;
    case PGPPUBKEYALGO_ECDSA:
#ifdef	NOTYET
	rc = rpmbcGenerateECDSA(dig);
#endif
	break;
    }
if (1 || _pgp_debug < 0)
fprintf(stderr, "<-- %s(%p) rc %d\t%s\n", __FUNCTION__, dig, rc, dig->pubkey_algoN);
    return rc;
}

/**
 */
static /*@only@*/
char * pgpMpiHex(const rpmuint8_t *p)
        /*@*/
{
    size_t nb = pgpMpiLen(p);
    char * t = xmalloc(2*nb + 1);
    (void) pgpHexCvt(t, p+2, nb-2);
    return t;
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
    {	const char * s = pgpMpiHex(p);
	strcpy(t+ix, s);
	s = _free(s);
    }
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
    const char * s = NULL;
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
	(void) mpnsethex(&bc->c, s = pgpMpiHex(p));
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
	(void) mpbsethex(&bc->rsa_pk.n, s = pgpMpiHex(p));
if (_pgp_debug && _pgp_print)
fprintf(stderr, "\t %s ", pre),  mpfprintln(stderr, bc->rsa_pk.n.size, bc->rsa_pk.n.modl);
	break;
    case 31:		/* RSA e */
	(void) mpnsethex(&bc->rsa_pk.e, s = pgpMpiHex(p));
if (_pgp_debug && _pgp_print)
fprintf(stderr, "\t %s ", pre),  mpfprintln(stderr, bc->rsa_pk.e.size, bc->rsa_pk.e.data);
	break;
    case 40:		/* DSA p */
	(void) mpbsethex(&bc->dsa_keypair.param.p, s = pgpMpiHex(p));
if (_pgp_debug && _pgp_print)
fprintf(stderr, "\t %s ", pre),  mpfprintln(stderr, bc->dsa_keypair.param.p.size, bc->dsa_keypair.param.p.modl);
	break;
    case 41:		/* DSA q */
	(void) mpbsethex(&bc->dsa_keypair.param.q, s = pgpMpiHex(p));
if (_pgp_debug && _pgp_print)
fprintf(stderr, "\t %s ", pre),  mpfprintln(stderr, bc->dsa_keypair.param.q.size, bc->dsa_keypair.param.q.modl);
	break;
    case 42:		/* DSA g */
	(void) mpnsethex(&bc->dsa_keypair.param.g, s = pgpMpiHex(p));
if (_pgp_debug && _pgp_print)
fprintf(stderr, "\t %s ", pre),  mpfprintln(stderr, bc->dsa_keypair.param.g.size, bc->dsa_keypair.param.g.data);
	break;
    case 43:		/* DSA y */
	(void) mpnsethex(&bc->dsa_keypair.y, s = pgpMpiHex(p));
if (_pgp_debug && _pgp_print)
fprintf(stderr, "\t %s ", pre),  mpfprintln(stderr, bc->dsa_keypair.y.size, bc->dsa_keypair.y.data);
	break;
    }
    s = _free(s);
    return rc;
}

/*@-mustmod@*/
static
void rpmbcClean(void * impl)
	/*@modifies impl @*/
{
    rpmbc bc = impl;
    if (bc != NULL) {
        bc->nbits = 0;
        bc->err = 0;
        bc->badok = 0;
	bc->digest = _free(bc->digest);
	bc->digestlen = 0;

mpnfree(&bc->rsa_decipher);
mpnfree(&bc->rsa_cipher);
rsakpFree(&bc->rsa_keypair);
randomGeneratorContextFree(&bc->rsa_rngc);

dlkp_pFree(&bc->elg_keypair);
#ifdef	DYING
dldp_pFree(&bc->elg_params);
#endif

#ifdef	DYING
	mpbfree(&bc->p);
	mpbfree(&bc->q);
	mpnfree(&bc->g);
	mpnfree(&bc->y);
	mpnfree(&bc->r);
	mpnfree(&bc->s);
#else
	dlkp_pFree(&bc->dsa_keypair);
#endif	/* DYING */
	mpnfree(&bc->hm);

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
    rpmbcClean(impl);
    impl = _free(impl);
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
	rpmbcSetRSA,
	rpmbcSetDSA,
	rpmbcSetDUMMY,
	rpmbcSetDUMMY,

	rpmbcErrChk,
	rpmbcAvailableCipher, rpmbcAvailableDigest, rpmbcAvailablePubkey,
	rpmbcVerify, rpmbcSign, rpmbcGenerate,

	rpmbcMpiItem, rpmbcClean,
	rpmbcFree, rpmbcInit
};

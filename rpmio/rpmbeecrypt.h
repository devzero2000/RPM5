#ifndef	H_RPMBEECRYPT
#define	H_RPMBEECRYPT

/** \ingroup rpmpgp
 * \file rpmio/rpmbeecrypt.h
 */

#ifdef HAVE_BEECRYPT_API_H
#include <beecrypt/api.h>
#endif

#include <rpmpgp.h>
#include <rpmsw.h>

/* Drag in the beecrypt includes. */
#include <beecrypt/beecrypt.h>
#include <beecrypt/base64.h>
#include <beecrypt/dsa.h>
#include <beecrypt/endianness.h>
#include <beecrypt/md5.h>
#include <beecrypt/mp.h>
#include <beecrypt/rsa.h>
#include <beecrypt/rsapk.h>
#include <beecrypt/sha1.h>
#include <beecrypt/sha256.h>
#include <beecrypt/sha384.h>
#include <beecrypt/sha512.h>

/** \ingroup rpmpgp
 * Values parsed from OpenPGP signature/pubkey packet(s).
 */
struct pgpDigParams_s {
/*@only@*/ /*@null@*/
    const char * userid;
/*@only@*/ /*@null@*/
    const byte * hash;
    const char * params[4];
    byte tag;

    byte version;		/*!< version number. */
    byte time[4];		/*!< time that the key was created. */
    byte pubkey_algo;		/*!< public key algorithm. */

    byte hash_algo;
    byte sigtype;
    byte hashlen;
    byte signhash16[2];
    byte signid[8];
    byte saved;
#define	PGPDIG_SAVED_TIME	(1 << 0)
#define	PGPDIG_SAVED_ID		(1 << 1)

};

/** \ingroup rpmpgp
 * Container for values parsed from an OpenPGP signature and public key.
 */
struct pgpDig_s {
    struct pgpDigParams_s signature;
    struct pgpDigParams_s pubkey;

    uint32_t sigtag;		/*!< Package signature tag. */
    uint32_t sigtype;		/*!< Package signature data type. */
/*@relnull@*/
    const void * sig;		/*!< Package signature. */
    uint32_t siglen;		/*!< Package signature length. */

    pgpVSFlags vsflags;		/*!< Digest/signature operation disablers. */
    struct rpmop_s dops;	/*!< Digest operation statistics. */
    struct rpmop_s sops;	/*!< Signature operation statistics. */

    int (*findPubkey) (void * _ts, /*@null@*/ void * _dig)
	/*@modifies *_ts, *_dig @*/;/*!< Find pubkey, i.e. rpmtsFindPubkey(). */
/*@null@*/
    void * _ts;			/*!< Find pubkey argument, i.e. rpmts. */
/*@refs@*/
    int nrefs;			/*!< Reference count. */

    byte ** ppkts;
    int npkts;
    size_t nbytes;		/*!< No. bytes of plain text. */

/*@only@*/ /*@null@*/
    DIGEST_CTX sha1ctx;		/*!< (dsa) sha1 hash context. */
/*@only@*/ /*@null@*/
    DIGEST_CTX hdrsha1ctx;	/*!< (dsa) header sha1 hash context. */
/*@only@*/ /*@null@*/
    void * sha1;		/*!< (dsa) V3 signature hash. */
    size_t sha1len;		/*!< (dsa) V3 signature hash length. */

/*@only@*/ /*@null@*/
    DIGEST_CTX md5ctx;		/*!< (rsa) md5 hash context. */
/*@only@*/ /*@null@*/
    DIGEST_CTX hdrmd5ctx;	/*!< (rsa) header md5 hash context. */
/*@only@*/ /*@null@*/
    void * md5;			/*!< (rsa) V3 signature hash. */
    size_t md5len;		/*!< (rsa) V3 signature hash length. */

    /* DSA parameters. */
    mpbarrett p;
    mpbarrett q;
    mpnumber g;
    mpnumber y;
    mpnumber hm;
    mpnumber r;
    mpnumber s;

    /* RSA parameters. */
    rsapk rsa_pk;
    mpnumber m;
    mpnumber c;
    mpnumber rsahm;
};

/**
 * Convert hex to binary nibble.
 * @param c            hex character
 * @return             binary nibble
 */
/*@unused@*/ static inline
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

/*@unused@*/ static inline
int pgpSetRSA(DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
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

/*@unused@*/ static inline
int pgpVerifyRSA(pgpDig dig)
	/*@*/
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

/*@unused@*/ static inline
int pgpSetDSA(DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
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

/*@unused@*/ static inline
int pgpVerifyDSA(pgpDig dig)
	/*@*/
{
    int rc;

/*@-moduncon@*/
    rc = dsavrfy(&dig->p, &dig->q, &dig->g, &dig->hm, &dig->y, &dig->r, &dig->s);
/*@=moduncon@*/

    return rc;
}

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMBEECRYPT */

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

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMBEECRYPT */

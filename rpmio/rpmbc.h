#ifndef	H_RPMBC
#define	H_RPMBC

/** \ingroup rpmpgp
 * \file rpmio/rpmbc.h
 */

#ifdef HAVE_BEECRYPT_API_H
#include <beecrypt/api.h>
#endif

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

/**
 */
typedef	/*abstract@*/ struct rpmbc_s * rpmbc;

/**
 */
#if defined(_RPMBC_INTERNAL)
struct rpmbc_s {
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
#endif

#ifdef __cplusplus
extern "C" {
#endif

int rpmbcSetRSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies ctx, dig @*/;

int rpmbcVerifyRSA(pgpDig dig)
	/*@*/;

int rpmbcSetDSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies ctx, dig @*/;

int rpmbcVerifyDSA(pgpDig dig)
	/*@*/;

int rpmbcMpiItem(const char * pre, pgpDig dig, int itemno,
		const byte * p, /*@null@*/ const byte * pend)
	/*@globals fileSystem @*/
	/*@modifies dig, fileSystem @*/;

void rpmbcClean(void * impl)
	/*@modifies impl @*/;

/*@null@*/
void * rpmbcFree(/*@only@*/ void * impl)
	/*@modifies impl @*/;

void * rpmbcInit(void)
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMBC */

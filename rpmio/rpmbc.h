#ifndef	H_RPMBC
#define	H_RPMBC

/** \ingroup rpmpgp
 * \file rpmio/rpmbc.h
 */

#ifdef HAVE_BEECRYPT_API_H
#include <beecrypt/api.h>
#endif

#include <rpmiotypes.h>
#include <rpmpgp.h>
#include <rpmsw.h>

/* Drag in the beecrypt includes. */
#include <beecrypt/beecrypt.h>
#include <beecrypt/base64.h>
#include <beecrypt/dsa.h>
#include <beecrypt/endianness.h>
#include <beecrypt/md4.h>
#include <beecrypt/md5.h>
#include <beecrypt/mp.h>
#include <beecrypt/rsa.h>
#include <beecrypt/rsapk.h>
#include <beecrypt/ripemd128.h>
#include <beecrypt/ripemd160.h>
#include <beecrypt/ripemd256.h>
#include <beecrypt/ripemd320.h>
#include <beecrypt/sha1.h>
#include <beecrypt/sha224.h>
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

/*@unchecked@*/
extern pgpImplVecs_t rpmbcImplVecs;

#endif	/* H_RPMBC */

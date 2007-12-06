#ifndef	H_RPMSSL
#define	H_RPMSSL

/** \ingroup rpmpgp
 * \file rpmio/rpmssl.h
 */

#include <rpmpgp.h>
#include <rpmsw.h>

/* Implementation specific includes. */
#if defined(_RPMSSL_INTERNAL)
#include <openssl/bn.h>
#include <openssl/dsa.h>
#include <openssl/rsa.h>
#include <openssl/engine.h>
#endif

/**
 */
typedef	/*abstract@*/ struct rpmssl_s * rpmssl;

/**
 * Implementation specific parameter storage.
 */
#if defined(_RPMSSL_INTERNAL)
struct rpmssl_s {
    /* DSA parameters. */
    DSA * dsa;
    DSA_SIG * dsasig;

    /* RSA parameters. */
    RSA * rsa;

    BIGNUM * rsahm;

    BIGNUM * c;
};
#endif

/*@unchecked@*/
extern pgpImplVecs_t rpmsslImplVecs;

#endif	/* H_RPMSSL */

#ifndef	H_RPMSSL
#define	H_RPMSSL

/** \ingroup rpmpgp
 * \file rpmio/rpmssl.h
 */

#include <rpmiotypes.h>
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
    BIO * out;			/* XXX tecdsa */
    EC_builtin_curve * curves;	/* XXX tecdsa */
    EC_GROUP * group;		/* XXX tecdsa */
    EC_KEY * ecdsakey_bad;	/* XXX tecdsa */

    /* DSA parameters. */
    DSA * dsa;
    DSA_SIG * dsasig;

    /* RSA parameters. */
    RSA * rsa;

    BIGNUM * rsahm;
    BIGNUM * c;

    /* ECDSA parameters. */
    EC_KEY * ecdsakey;
    ECDSA_SIG * ecdsasig;
    EVP_MD_CTX ecdsahm;

    BIGNUM * r;
    BIGNUM * s;
};
#endif

/*@unchecked@*/
extern pgpImplVecs_t rpmsslImplVecs;

#endif	/* H_RPMSSL */

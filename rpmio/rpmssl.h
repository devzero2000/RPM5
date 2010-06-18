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
    int in_fips_mode;	/* XXX trsa */
    int nbits;		/* XXX trsa */
    int qbits;		/* XXX trsa */
    int badok;		/* XXX trsa */
    int err;

    void * digest;
    size_t digestlen;

    BIO * out;			/* XXX tecdsa */
    EC_builtin_curve * curves;	/* XXX tecdsa */
    size_t ncurves;
    EC_GROUP * group;		/* XXX tecdsa */
    EC_KEY * ecdsakey_bad;	/* XXX tecdsa */

    /* DSA parameters. */
    DSA * dsa;
    DSA_SIG * dsasig;

    /* RSA parameters. */
    RSA * rsa;

    BIGNUM * hm;
    BIGNUM * c;

    /* ECDSA parameters. */
    int nid;
    EC_KEY * ecdsakey;
    ECDSA_SIG * ecdsasig;

};
#endif

/*@unchecked@*/
extern pgpImplVecs_t rpmsslImplVecs;

#endif	/* H_RPMSSL */

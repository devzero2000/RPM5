#ifndef	H_RPMNSS
#define	H_RPMNSS

/** \ingroup rpmpgp
 * \file rpmio/rpmnss.h
 */

#include <rpmiotypes.h>
#include <rpmpgp.h>
#include <rpmsw.h>

#if defined(_RPMNSS_INTERNAL)
#if defined(__LCLINT__)
#define	__i386__
#endif
#include <nss.h>
#include <sechash.h>
#include <keyhi.h>
#include <cryptohi.h>
#endif

/** \ingroup rpmpgp
 */
typedef	/*abstract@*/ struct rpmnss_s * rpmnss;

/** \ingroup rpmpgp
 */
#if defined(_RPMNSS_INTERNAL)
struct rpmnss_s {
    int in_fips_mode;	/* XXX trsa */
    int nbits;		/* XXX trsa */
    int qbits;		/* XXX trsa */
    int badok;		/* XXX trsa */
    int err;

    void * digest;
    size_t digestlen;

    SECOidTag sigalg;
    SECItem item;

    /* RSA parameters. */
    SECKEYPublicKey * rsa;
    SECItem * rsasig;

    /* DSA parameters. */
    SECKEYPublicKey * dsa;
    SECItem * dsasig;

    /* ECDSA parameters. */
    SECKEYECParams * ecparams;
    SECKEYPrivateKey * ecpriv;

    SECKEYPublicKey * ecdsa;
    SECItem * ecdsasig;

};
#endif

/** \ingroup rpmpgp
 */
/*@unchecked@*/
extern pgpImplVecs_t rpmnssImplVecs;

#endif	/* H_RPMNSS */

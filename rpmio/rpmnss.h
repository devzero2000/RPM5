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

    SECOidTag sigalg;
    SECItem item;

    /* DSA parameters. */
    SECKEYPublicKey *dsa;
    SECItem * dsasig;

    /* RSA parameters. */
    SECKEYPublicKey *rsa;
    SECItem * rsasig;
};
#endif

/** \ingroup rpmpgp
 */
/*@unchecked@*/
extern pgpImplVecs_t rpmnssImplVecs;

#endif	/* H_RPMNSS */

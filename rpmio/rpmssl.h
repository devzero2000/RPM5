#ifndef	H_RPMSSL
#define	H_RPMSSL

/** \ingroup rpmpgp
 * \file rpmio/rpmssl.h
 */

#include <rpmpgp.h>
#include <rpmsw.h>

/* Implementation specific includes. */
#if defined(_RPMSSL_INTERNAL)
#endif

/**
 */
typedef	/*abstract@*/ struct rpmssl_s * rpmssl;

/**
 * Implementation specific parameter storage.
 */
#if defined(_RPMSSL_INTERNAL)
struct rpmssl_s {
    int foo;

    /* DSA parameters. */

    /* RSA parameters. */
};
#endif

/*@unchecked@*/
extern pgpImplVecs_t rpmsslImplVecs;

#endif	/* H_RPMSSL */

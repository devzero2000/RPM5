#ifndef	H_RPMGC
#define	H_RPMGC

/** \ingroup rpmpgp
 * \file rpmio/rpmgc.h
 */

#include <rpmpgp.h>
#include <rpmsw.h>

/* Implementation specific includes. */
#if defined(_RPMGC_INTERNAL)
#endif

/**
 */
typedef	/*abstract@*/ struct rpmgc_s * rpmgc;

/**
 * Implementation specific parameter storage.
 */
#if defined(_RPMGC_INTERNAL)
struct rpmgc_s {
    int foo;

    /* DSA parameters. */

    /* RSA parameters. */
};
#endif

/*@unchecked@*/
extern pgpImplVecs_t rpmgcImplVecs;

#endif	/* H_RPMGC */

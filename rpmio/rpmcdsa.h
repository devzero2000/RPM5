#ifndef	H_RPMCDSA
#define	H_RPMCDSA

/** \ingroup rpmpgp
 * \file rpmio/rpmcdsa.h
 */

#include <rpmiotypes.h>
#include <rpmpgp.h>
#include <rpmsw.h>

/* Implementation specific includes. */
#if defined(_RPMCDSA_INTERNAL)
#endif

/**
 */
typedef	/*abstract@*/ struct rpmcdsa_s * rpmcdsa;

/**
 * Implementation specific parameter storage.
 */
#if defined(_RPMCDSA_INTERNAL)
struct rpmcdsa_s {
    int in_fips_mode;	/* XXX trsa */
    int nbits;		/* XXX trsa */
    int qbits;		/* XXX trsa */
    int badok;		/* XXX trsa */
    int err;

    void * digest;
    size_t digestlen;

    /* DSA parameters. */

    /* RSA parameters. */

    /* ECDSA parameters. */

};
#endif

/*@unchecked@*/
extern pgpImplVecs_t rpmcdsaImplVecs;

#endif	/* H_RPMCDSA */

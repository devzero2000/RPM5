#ifndef	H_RPMLTC
#define	H_RPMLTC

/** \ingroup rpmpgp
 * \file rpmio/rpmltc.h
 */

#include <rpmiotypes.h>
#include <rpmpgp.h>
#include <rpmsw.h>

/* Implementation specific includes. */
#if defined(_RPMLTC_INTERNAL)
#include <tomcrypt.h>
#endif

/**
 */
typedef	/*abstract@*/ struct rpmltc_s * rpmltc;

/**
 * Implementation specific parameter storage.
 */
#if defined(_RPMLTC_INTERNAL)
struct rpmltc_s {
    int in_fips_mode;	/* XXX trsa */
    int nbits;		/* XXX trsa */
    int qbits;		/* XXX trsa */
    int badok;		/* XXX trsa */
    int err;

    void * digest;
    size_t digestlen;

    /* DSA parameters. */
    dsa_key dsa;

    /* RSA parameters. */
    rsa_key rsa;

    /* ECDSA parameters. */
    ecc_key pub_key;
    ecc_key sec_key;

};
#endif

/*@unchecked@*/
extern pgpImplVecs_t rpmltcImplVecs;

#endif	/* H_RPMLTC */

#ifndef	H_RPMGC
#define	H_RPMGC

/** \ingroup rpmpgp
 * \file rpmio/rpmgc.h
 */

#include <rpmiotypes.h>
#include <rpmpgp.h>
#include <rpmsw.h>

/* Implementation specific includes. */
#if defined(_RPMGC_INTERNAL)
#include <gcrypt.h>
#endif

/**
 */
typedef	/*abstract@*/ struct rpmgc_s * rpmgc;

/**
 * Implementation specific parameter storage.
 */
#if defined(_RPMGC_INTERNAL)
struct rpmgc_s {
    int in_fips_mode;	/* XXX trsa */
    int nbits;		/* XXX trsa */
    int qbits;		/* XXX trsa */
    gcry_error_t badok;	/* XXX trsa */
    gcry_error_t err;

    void * digest;
    size_t digestlen;

    gcry_sexp_t key_spec;	/* XXX private to Generate? */
    gcry_sexp_t key_pair;	/* XXX private to Generate? */

    gcry_sexp_t pub_key;
    gcry_sexp_t sec_key;
    gcry_sexp_t hash;
    gcry_sexp_t sig;

    /* DSA parameters. */
    gcry_mpi_t p;
    gcry_mpi_t q;
    gcry_mpi_t g;
    gcry_mpi_t y;

    gcry_mpi_t r;
    gcry_mpi_t s;

    gcry_mpi_t hm;

    /* RSA parameters. */
    gcry_mpi_t n;
    gcry_mpi_t e;
    gcry_mpi_t c;

    /* ECDSA parameters (none atm). */

};
#endif

/*@unchecked@*/
extern pgpImplVecs_t rpmgcImplVecs;

#endif	/* H_RPMGC */

#ifndef	H_RPMGC
#define	H_RPMGC

/** \ingroup rpmpgp
 * \file rpmio/rpmgc.h
 */

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
    gcry_sexp_t sig;
    gcry_sexp_t hash;
    gcry_sexp_t pkey;

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

};
#endif

/*@unchecked@*/
extern pgpImplVecs_t rpmgcImplVecs;

#endif	/* H_RPMGC */

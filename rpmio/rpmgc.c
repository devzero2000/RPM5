/** \ingroup rpmpgp
 * \file rpmio/rpmgc.c
 */

#include "system.h"

#include <rpmiotypes.h>
#define	_RPMGC_INTERNAL
#if defined(WITH_GCRYPT)
#define	_RPMPGP_INTERNAL
#include <rpmgc.h>
#endif

#include "debug.h"

#if defined(WITH_GCRYPT)

/*@access pgpDig @*/
/*@access pgpDigParams @*/

/*@-redecl@*/
/*@unchecked@*/
extern int _pgp_debug;

/*@unchecked@*/
extern int _pgp_print;
/*@=redecl@*/

static
void rpmgcDump(const char * msg, gcry_sexp_t sexp)
	/*@*/
{
    char buf[BUFSIZ];
    size_t nb;

/*@-modunconnomods @*/
    nb = gcry_sexp_sprint(sexp, GCRYSEXP_FMT_ADVANCED, buf, sizeof(buf));
/*@=modunconnomods @*/
/*@-modfilesys@*/
if (_pgp_debug)
fprintf(stderr, "========== %s:\n%s", msg, buf);
/*@=modfilesys@*/
    return;
}

static
int rpmgcSetRSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    rpmgc gc = dig->impl;
    const char * hash_algo_name = NULL;
    gcry_mpi_t c = NULL;
    gcry_error_t rc;
    int xx;

    switch (sigp->hash_algo) {
    case PGPHASHALGO_MD5:
	hash_algo_name = "md5";
	break;
    case PGPHASHALGO_SHA1:
	hash_algo_name = "sha1";
	break;
    case PGPHASHALGO_RIPEMD160:
	hash_algo_name = "ripemd160";
	break;
    case PGPHASHALGO_MD2:
	hash_algo_name = "md2";
	break;
    case PGPHASHALGO_TIGER192:
	hash_algo_name = "tiger";
	break;
    case PGPHASHALGO_HAVAL_5_160:
#ifdef	NOTYET
	hash_algo_name = "haval";
#endif
	break;
    case PGPHASHALGO_SHA256:
	hash_algo_name = "sha256";
	break;
    case PGPHASHALGO_SHA384:
	hash_algo_name = "sha384";
	break;
    case PGPHASHALGO_SHA512:
	hash_algo_name = "sha512";
	break;
    case PGPHASHALGO_SHA224:
#ifdef	NOTYET
	hash_algo_name = "sha224";
#endif
	break;
    default:
	break;
    }
    if (hash_algo_name == NULL)
	return 1;

    xx = rpmDigestFinal(ctx, (void **)&dig->md5, &dig->md5len, 0);

    /* Set RSA hash. */
/*@-moduncon -noeffectuncon @*/
    xx = gcry_mpi_scan(&c, GCRYMPI_FMT_USG, dig->md5, dig->md5len, NULL);
    rc = gcry_sexp_build(&gc->hash, NULL, "(data (flags pkcs1) (hash %s %m))",
		hash_algo_name, c);
    gcry_mpi_release(c);
if (_pgp_debug)
rpmgcDump("gc->hash", gc->hash);
/*@=moduncon =noeffectuncon @*/

    /* Compare leading 16 bits of digest for quick check. */
    return memcmp(dig->md5, sigp->signhash16, sizeof(sigp->signhash16));
}

static
int rpmgcVerifyRSA(pgpDig dig)
	/*@*/
{
    rpmgc gc = dig->impl;
    gcry_error_t rc;

/*@-moduncon@*/
    rc = gcry_sexp_build(&gc->sig, NULL,
		"(sig-val (RSA (s %m)))",
		 gc->c);
/*@=moduncon@*/
if (_pgp_debug)
rpmgcDump("gc->sig", gc->sig);
/*@-moduncon@*/
    rc = gcry_sexp_build(&gc->pkey, NULL,
		"(public-key (RSA (n %m) (e %m)))",
		gc->n, gc->e);
/*@=moduncon@*/
if (_pgp_debug)
rpmgcDump("gc->pkey", gc->pkey);

    /* Verify RSA signature. */
/*@-moduncon@*/
    rc = gcry_pk_verify (gc->sig, gc->hash, gc->pkey);
/*@=moduncon@*/

    gcry_sexp_release(gc->pkey);	gc->pkey = NULL;
    gcry_sexp_release(gc->hash);	gc->hash = NULL;
    gcry_sexp_release(gc->sig);		gc->sig = NULL;

    return (rc ? 0 : 1);
}

static
int rpmgcSetDSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    rpmgc gc = dig->impl;
    gcry_error_t rc;
    int xx;

    xx = rpmDigestFinal(ctx, (void **)&dig->sha1, &dig->sha1len, 0);

   /* Set DSA hash. */
/*@-moduncon -noeffectuncon @*/
    rc = gcry_sexp_build(&gc->hash, NULL,
		"(data (flags raw) (value %b))",
		dig->sha1len, dig->sha1);
/*@=moduncon =noeffectuncon @*/
if (_pgp_debug)
rpmgcDump("gc->hash", gc->hash);

    /* Compare leading 16 bits of digest for quick check. */
    return memcmp(dig->sha1, sigp->signhash16, sizeof(sigp->signhash16));
}

static
int rpmgcVerifyDSA(pgpDig dig)
	/*@*/
{
    rpmgc gc = dig->impl;
    gcry_error_t rc;

/*@-moduncon -noeffectuncon @*/
    rc = gcry_sexp_build(&gc->sig, NULL,
		"(sig-val (DSA (r %m) (s %m)))",
		gc->r, gc->s);
if (_pgp_debug)
rpmgcDump("gc->sig", gc->sig);
    rc = gcry_sexp_build(&gc->pkey, NULL,
		"(public-key (DSA (p %m) (q %m) (g %m) (y %m)))",
		gc->p, gc->q, gc->g, gc->y);
if (_pgp_debug)
rpmgcDump("gc->pkey", gc->pkey);

    /* Verify DSA signature. */
    rc = gcry_pk_verify (gc->sig, gc->hash, gc->pkey);

    gcry_sexp_release(gc->pkey);	gc->pkey = NULL;
    gcry_sexp_release(gc->hash);	gc->hash = NULL;
    gcry_sexp_release(gc->sig);		gc->sig = NULL;
/*@=moduncon -noeffectuncon @*/
    return (rc ? 0 : 1);
}

/*@-globuse -mustmod @*/
static
int rpmgcMpiItem(/*@unused@*/ const char * pre, pgpDig dig, int itemno,
		const rpmuint8_t * p,
		/*@unused@*/ /*@null@*/ const rpmuint8_t * pend)
	/*@globals fileSystem @*/
	/*@modifies dig, fileSystem @*/
{
    rpmgc gc = dig->impl;
    unsigned int nb;
    int rc = 0;
    int xx;

/*@-moduncon -noeffectuncon @*/
    switch (itemno) {
    default:
assert(0);
	break;
    case 10:		/* RSA m**d */
	nb = ((pgpMpiBits(p) + 7) >> 3) + 2;
	xx = gcry_mpi_scan(&gc->c, GCRYMPI_FMT_PGP, p, nb, NULL);
	break;
    case 20:		/* DSA r */
	nb = ((pgpMpiBits(p) + 7) >> 3) + 2;
	xx = gcry_mpi_scan(&gc->r, GCRYMPI_FMT_PGP, p, nb, NULL);
	break;
    case 21:		/* DSA s */
	nb = ((pgpMpiBits(p) + 7) >> 3) + 2;
	xx = gcry_mpi_scan(&gc->s, GCRYMPI_FMT_PGP, p, nb, NULL);
	break;
    case 30:		/* RSA n */
	nb = ((pgpMpiBits(p) + 7) >> 3) + 2;
	xx = gcry_mpi_scan(&gc->n, GCRYMPI_FMT_PGP, p, nb, NULL);
	break;
    case 31:		/* RSA e */
	nb = ((pgpMpiBits(p) + 7) >> 3) + 2;
	xx = gcry_mpi_scan(&gc->e, GCRYMPI_FMT_PGP, p, nb, NULL);
	break;
    case 40:		/* DSA p */
	nb = ((pgpMpiBits(p) + 7) >> 3) + 2;
	xx = gcry_mpi_scan(&gc->p, GCRYMPI_FMT_PGP, p, nb, NULL);
	break;
    case 41:		/* DSA q */
	nb = ((pgpMpiBits(p) + 7) >> 3) + 2;
	xx = gcry_mpi_scan(&gc->q, GCRYMPI_FMT_PGP, p, nb, NULL);
	break;
    case 42:		/* DSA g */
	nb = ((pgpMpiBits(p) + 7) >> 3) + 2;
	xx = gcry_mpi_scan(&gc->g, GCRYMPI_FMT_PGP, p, nb, NULL);
	break;
    case 43:		/* DSA y */
	nb = ((pgpMpiBits(p) + 7) >> 3) + 2;
	xx = gcry_mpi_scan(&gc->y, GCRYMPI_FMT_PGP, p, nb, NULL);
	break;
    }
/*@=moduncon =noeffectuncon @*/
    return rc;
}
/*@=globuse =mustmod @*/

/*@-mustmod@*/
static
void rpmgcClean(void * impl)
	/*@modifies impl @*/
{
    rpmgc gc = impl;
/*@-moduncon -noeffectuncon @*/
    if (gc != NULL) {
	if (gc->sig) {
	    gcry_sexp_release(gc->sig);
	    gc->sig = NULL;
	}
	if (gc->hash) {
	    gcry_sexp_release(gc->hash);
	    gc->hash = NULL;
	}
	if (gc->pkey) {
	    gcry_sexp_release(gc->pkey);
	    gc->pkey = NULL;
	}
	if (gc->r) {
	    gcry_mpi_release(gc->r);
	    gc->r = NULL;
	}
	if (gc->s) {
	    gcry_mpi_release(gc->s);
	    gc->s = NULL;
	}
	if (gc->n) {
	    gcry_mpi_release(gc->n);
	    gc->n = NULL;
	}
	if (gc->e) {
	    gcry_mpi_release(gc->e);
	    gc->e = NULL;
	}
	if (gc->c) {
	    gcry_mpi_release(gc->c);
	    gc->c = NULL;
	}
	if (gc->p) {
	    gcry_mpi_release(gc->p);
	    gc->p = NULL;
	}
	if (gc->q) {
	    gcry_mpi_release(gc->q);
	    gc->q = NULL;
	}
	if (gc->g) {
	    gcry_mpi_release(gc->g);
	    gc->g = NULL;
	}
	if (gc->y) {
	    gcry_mpi_release(gc->y);
	    gc->y = NULL;
	}
    }
/*@=moduncon =noeffectuncon @*/
}
/*@=mustmod@*/

static /*@null@*/
void * rpmgcFree(/*@only@*/ void * impl)
	/*@modifies impl @*/
{
    rpmgc gc = impl;
    rpmgcClean(impl);
    gc = _free(gc);
    return NULL;
}

static
void * rpmgcInit(void)
	/*@*/
{
    rpmgc gc = xcalloc(1, sizeof(*gc));
    return (void *) gc;
}

struct pgpImplVecs_s rpmgcImplVecs = {
	rpmgcSetRSA, rpmgcVerifyRSA,
	rpmgcSetDSA, rpmgcVerifyDSA,
	rpmgcMpiItem, rpmgcClean,
	rpmgcFree, rpmgcInit
};

#endif


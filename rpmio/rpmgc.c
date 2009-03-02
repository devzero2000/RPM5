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
    size_t nb = gcry_sexp_sprint(sexp, GCRYSEXP_FMT_ADVANCED, NULL, 0);
    char * buf = alloca(nb+1);

/*@-modunconnomods @*/
    nb = gcry_sexp_sprint(sexp, GCRYSEXP_FMT_ADVANCED, buf, nb);
/*@=modunconnomods @*/
    buf[nb] = '\0';
/*@-modfilesys@*/
if (_pgp_debug)
fprintf(stderr, "========== %s:\n%s", msg, buf);
/*@=modfilesys@*/
    return;
}

static
gcry_error_t rpmgcErr(/*@unused@*/rpmgc gc, const char * msg, gcry_error_t err)
	/*@*/
{
/*@-evalorderuncon -modfilesys -moduncon @*/
    if (err) {
	fprintf (stderr, "rpmgc: %s: %s/%s\n",
		msg, gcry_strsource (err), gcry_strerror (err));
    }
/*@=evalorderuncon =modfilesys =moduncon @*/
    return err;
}

static
int rpmgcSetRSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    rpmgc gc = dig->impl;
    const char * hash_algo_name = NULL;
    int rc;
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
    {	gcry_mpi_t c = NULL;
	gcry_error_t yy;
	xx = gcry_mpi_scan(&c, GCRYMPI_FMT_USG, dig->md5, dig->md5len, NULL);
	yy = rpmgcErr(gc, "RSA c",
		gcry_sexp_build(&gc->hash, NULL,
			"(data (flags pkcs1) (hash %s %m))", hash_algo_name, c) );
	gcry_mpi_release(c);
if (_pgp_debug < 0) rpmgcDump("gc->hash", gc->hash);
    }
/*@=moduncon =noeffectuncon @*/

    /* Compare leading 16 bits of digest for quick check. */
    {	const rpmuint8_t *s = dig->md5;
	const rpmuint8_t *t = sigp->signhash16;
	rc = memcmp(s, t, sizeof(sigp->signhash16));
#ifdef	DYING
	if (rc != 0)
	    fprintf(stderr, "*** hash fails: digest(%02x%02x) != signhash(%02x%02x)\n",
		s[0], s[1], t[0], t[1]);
#endif
    }

    return rc;
}

static
int rpmgcVerifyRSA(pgpDig dig)
	/*@*/
{
    rpmgc gc = dig->impl;
    gcry_error_t err;

/*@-moduncon@*/
    err = rpmgcErr(gc, "RSA gc->sig",
		gcry_sexp_build(&gc->sig, NULL,
			"(sig-val (RSA (s %m)))", gc->c) );
/*@=moduncon@*/
if (_pgp_debug < 0)
rpmgcDump("gc->sig", gc->sig);
/*@-moduncon@*/
    err = rpmgcErr(gc, "RSA gc->pkey",
		gcry_sexp_build(&gc->pkey, NULL,
			"(public-key (RSA (n %m) (e %m)))", gc->n, gc->e) );
/*@=moduncon@*/
if (_pgp_debug < 0)
rpmgcDump("gc->pkey", gc->pkey);

    /* Verify RSA signature. */
/*@-moduncon@*/
    err = rpmgcErr(gc, "RSA verify",
		gcry_pk_verify (gc->sig, gc->hash, gc->pkey) );
/*@=moduncon@*/

    gcry_sexp_release(gc->pkey);	gc->pkey = NULL;
    gcry_sexp_release(gc->hash);	gc->hash = NULL;
    gcry_sexp_release(gc->sig);		gc->sig = NULL;

    return (err ? 0 : 1);
}

static
int rpmgcSetDSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    rpmgc gc = dig->impl;
    gcry_error_t err;
    int xx;

assert(sigp->hash_algo == rpmDigestAlgo(ctx));
    xx = rpmDigestFinal(ctx, (void **)&dig->sha1, &dig->sha1len, 0);

    /* Set DSA hash. */
    err = rpmgcErr(gc, "DSA gc->hash",
		gcry_sexp_build(&gc->hash, NULL,
			"(data (flags raw) (value %b))", dig->sha1len, dig->sha1) );
if (_pgp_debug < 0)
rpmgcDump("gc->hash", gc->hash);

    /* Compare leading 16 bits of digest for quick check. */
    return memcmp(dig->sha1, sigp->signhash16, sizeof(sigp->signhash16));
}

static
int rpmgcVerifyDSA(pgpDig dig)
	/*@*/
{
    rpmgc gc = dig->impl;
    gcry_error_t err;

/*@-moduncon -noeffectuncon @*/

    err = rpmgcErr(gc, "DSA gc->sig",
		gcry_sexp_build(&gc->sig, NULL,
			"(sig-val (DSA (r %m) (s %m)))", gc->r, gc->s) );
if (_pgp_debug < 0)
rpmgcDump("gc->sig", gc->sig);
    err = rpmgcErr(gc, "DSA gc->pkey",
		gcry_sexp_build(&gc->pkey, NULL,
			"(public-key (DSA (p %m) (q %m) (g %m) (y %m)))",
			gc->p, gc->q, gc->g, gc->y) );
if (_pgp_debug < 0)
rpmgcDump("gc->pkey", gc->pkey);

    /* Verify DSA signature. */
    err = rpmgcErr(gc, "DSA verify",
		gcry_pk_verify (gc->sig, gc->hash, gc->pkey) );

    gcry_sexp_release(gc->pkey);	gc->pkey = NULL;
    gcry_sexp_release(gc->hash);	gc->hash = NULL;
    gcry_sexp_release(gc->sig);		gc->sig = NULL;

/*@=moduncon -noeffectuncon @*/

    return (err ? 0 : 1);
}


static
int rpmgcSetECDSA(/*@only@*/ DIGEST_CTX ctx, /*@unused@*/pgpDig dig, pgpDigParams sigp)
	/*@*/
{
    int rc = 1;		/* XXX always fail. */
    int xx;

assert(sigp->hash_algo == rpmDigestAlgo(ctx));
    xx = rpmDigestFinal(ctx, (void **)NULL, NULL, 0);

    /* Compare leading 16 bits of digest for quick check. */

    return rc;
}

static
int rpmgcVerifyECDSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    int rc = 0;		/* XXX always fail. */

    return rc;
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
    size_t nb = pgpMpiLen(p);
    const char * mpiname = "";
    gcry_mpi_t * mpip = NULL;
    size_t nscan = 0;
    gcry_error_t err;
    int rc = 0;

    switch (itemno) {
    default:
assert(0);
	break;
    case 10:		/* RSA m**d */
	mpiname ="RSA m**d";	mpip = &gc->c;
	break;
    case 20:		/* DSA r */
	mpiname = "DSA r";	mpip = &gc->r;
	break;
    case 21:		/* DSA s */
	mpiname = "DSA s";	mpip = &gc->s;
	break;
    case 30:		/* RSA n */
	mpiname = "RSA n";	mpip = &gc->n;
	break;
    case 31:		/* RSA e */
	mpiname = "RSA e";	mpip = &gc->e;
	break;
    case 40:		/* DSA p */
	mpiname = "DSA p";	mpip = &gc->p;
	break;
    case 41:		/* DSA q */
	mpiname = "DSA q";	mpip = &gc->q;
	break;
    case 42:		/* DSA g */
	mpiname = "DSA g";	mpip = &gc->g;
	break;
    case 43:		/* DSA y */
	mpiname = "DSA y";	mpip = &gc->y;
	break;
    }

/*@-moduncon -noeffectuncon @*/
    err = rpmgcErr(gc, mpiname,
		gcry_mpi_scan(mpip, GCRYMPI_FMT_PGP, p, nb, &nscan) );
/*@=moduncon =noeffectuncon @*/

    if (_pgp_debug < 0)
    {	size_t nbits = gcry_mpi_get_nbits(*mpip);
	unsigned char * hex = NULL;
	size_t nhex = 0;
	err = rpmgcErr(gc, "MPI print",
		gcry_mpi_aprint(GCRYMPI_FMT_HEX, &hex, &nhex, *mpip) );
	fprintf(stderr, "*** %s\t%5d:%s\n", mpiname, (int)nbits, hex);
	hex = _free(hex);
    }

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

/*@unchecked@*/
static int rpmgc_initialized;

static /*@null@*/
void * rpmgcFree(/*@only@*/ void * impl)
	/*@globals rpmgc_initialized @*/
	/*@modifies impl, rpmgc_initialized @*/
{
    rpmgc gc = impl;

    rpmgcClean(impl);

    if (--rpmgc_initialized == 0 && _pgp_debug < 0) {
	gcry_error_t err;
	err = rpmgcErr(gc, "CLEAR_DEBUG_FLAGS",
		gcry_control(GCRYCTL_CLEAR_DEBUG_FLAGS, 3));
	err = rpmgcErr(gc, "SET_VERBOSITY",
		gcry_control(GCRYCTL_SET_VERBOSITY, 0) );
    }

    gc = _free(gc);

    return NULL;
}

static
void * rpmgcInit(void)
	/*@globals rpmgc_initialized @*/
	/*@modifies rpmgc_initialized @*/
{
    rpmgc gc = xcalloc(1, sizeof(*gc));

    if (rpmgc_initialized++ == 0 && _pgp_debug < 0) {
	gcry_error_t err;
	err = rpmgcErr(gc, "SET_VERBOSITY",
		gcry_control(GCRYCTL_SET_VERBOSITY, 3) );
	err = rpmgcErr(gc, "SET_DEBUG_FLAGS",
		gcry_control(GCRYCTL_SET_DEBUG_FLAGS, 3) );
    }

    return (void *) gc;
}

struct pgpImplVecs_s rpmgcImplVecs = {
	rpmgcSetRSA, rpmgcVerifyRSA,
	rpmgcSetDSA, rpmgcVerifyDSA,
	rpmgcSetECDSA, rpmgcVerifyECDSA,
	rpmgcMpiItem, rpmgcClean,
	rpmgcFree, rpmgcInit
};

#endif


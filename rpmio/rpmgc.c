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
	fprintf (stderr, "rpmgc: %s(0x%0x): %s/%s\n",
		msg, (unsigned)err, gcry_strsource(err), gcry_strerror(err));
    }
/*@=evalorderuncon =modfilesys =moduncon @*/
    return err;
}

static
int rpmgcSetRSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    rpmgc gc = dig->impl;
    gcry_error_t err;
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
    err = rpmgcErr(gc, "RSA c",
	    gcry_sexp_build(&gc->hash, NULL,
		"(data (flags pkcs1) (hash %s %b))", hash_algo_name, dig->md5len, dig->md5) );
if (_pgp_debug < 0) rpmgcDump("gc->hash", gc->hash);

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

/*@-moduncon@*/
    gc->err = rpmgcErr(gc, "RSA gc->sig",
		gcry_sexp_build(&gc->sig, NULL,
			"(sig-val (RSA (s %m)))", gc->c) );
/*@=moduncon@*/
if (_pgp_debug < 0)
rpmgcDump("gc->sig", gc->sig);
/*@-moduncon@*/
    gc->err = rpmgcErr(gc, "RSA gc->pub_key",
		gcry_sexp_build(&gc->pub_key, NULL,
			"(public-key (RSA (n %m) (e %m)))", gc->n, gc->e) );
/*@=moduncon@*/
if (_pgp_debug < 0)
rpmgcDump("gc->pub_key", gc->pub_key);

    /* Verify RSA signature. */
/*@-moduncon@*/
    gc->err = rpmgcErr(gc, "RSA verify",
		gcry_pk_verify (gc->sig, gc->hash, gc->pub_key) );
/*@=moduncon@*/

#ifdef	DYING
    gcry_sexp_release(gc->pub_key);	gc->pub_key = NULL;
    gcry_sexp_release(gc->hash);	gc->hash = NULL;
    gcry_sexp_release(gc->sig);		gc->sig = NULL;
#endif

    return (gc->err ? 0 : 1);
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
/*@-moduncon -noeffectuncon @*/
    {	gcry_mpi_t c = NULL;
	/* XXX truncate to 160 bits */
	err = rpmgcErr(gc, "DSA c",
		gcry_mpi_scan(&c, GCRYMPI_FMT_USG, dig->sha1, 160/8, NULL));
	err = rpmgcErr(gc, "DSA gc->hash",
		gcry_sexp_build(&gc->hash, NULL,
			"(data (flags raw) (value %m))", c) );
	gcry_mpi_release(c);
if (_pgp_debug < 0) rpmgcDump("gc->hash", gc->hash);
    }
/*@=moduncon =noeffectuncon @*/

    /* Compare leading 16 bits of digest for quick check. */
    return memcmp(dig->sha1, sigp->signhash16, sizeof(sigp->signhash16));
}

static
int rpmgcVerifyDSA(pgpDig dig)
	/*@*/
{
    rpmgc gc = dig->impl;

/*@-moduncon -noeffectuncon @*/

    gc->err = rpmgcErr(gc, "DSA gc->sig",
		gcry_sexp_build(&gc->sig, NULL,
			"(sig-val (DSA (r %m) (s %m)))", gc->r, gc->s) );
if (_pgp_debug < 0)
rpmgcDump("gc->sig", gc->sig);
    gc->err = rpmgcErr(gc, "DSA gc->pub_key",
		gcry_sexp_build(&gc->pub_key, NULL,
			"(public-key (DSA (p %m) (q %m) (g %m) (y %m)))",
			gc->p, gc->q, gc->g, gc->y) );
if (_pgp_debug < 0)
rpmgcDump("gc->pub_key", gc->pub_key);

    /* Verify DSA signature. */
    gc->err = rpmgcErr(gc, "DSA verify",
		gcry_pk_verify (gc->sig, gc->hash, gc->pub_key) );

#ifdef	DYING
    gcry_sexp_release(gc->pub_key);	gc->pub_key = NULL;
    gcry_sexp_release(gc->hash);	gc->hash = NULL;
    gcry_sexp_release(gc->sig);		gc->sig = NULL;
#endif

/*@=moduncon -noeffectuncon @*/

    return (gc->err ? 0 : 1);
}

static
int rpmgcSetELG(/*@only@*/ DIGEST_CTX ctx, /*@unused@*/pgpDig dig, pgpDigParams sigp)
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
int rpmgcSetECDSA(/*@only@*/ DIGEST_CTX ctx, /*@unused@*/pgpDig dig, pgpDigParams sigp)
	/*@*/
{
    int rc = 1;		/* assume failure. */
    rpmgc gc = dig->impl;
    gpg_error_t err;
    int xx;

assert(sigp->hash_algo == rpmDigestAlgo(ctx));
gc->digest = _free(gc->digest);
gc->digestlen = 0;
    xx = rpmDigestFinal(ctx, (void **)&gc->digest, &gc->digestlen, 0);

    {   gcry_mpi_t c = NULL;
	err = rpmgcErr(gc, "ECDSA c",
		gcry_mpi_scan(&c, GCRYMPI_FMT_USG, gc->digest, gc->digestlen, NULL));
if (gc->hash) {
    gcry_sexp_release(gc->hash);	gc->hash = NULL;
}
	err = rpmgcErr(gc, "ECDSA gc->hash",
		gcry_sexp_build(&gc->hash, NULL,
			"(data (flags raw) (value %m))", c) );
	gcry_mpi_release(c);
if (_pgp_debug < 0) rpmgcDump("gc->hash", gc->hash);
    }

    /* Compare leading 16 bits of digest for quick check. */

if (_pgp_debug < 0)
fprintf(stderr, "<-- %s(%p,%p) rc %d\n", __FUNCTION__, dig, sigp, rc);

    return rc;
}

static
int rpmgcVerifyECDSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    int rc = 0;		/* assume failure. */
    rpmgc gc = dig->impl;
    gpg_error_t err;

    /* Verify ECDSA signature. */
    err = rpmgcErr(gc, "ECDSA verify",
		gcry_pk_verify (gc->sig, gc->hash, gc->pub_key));

    /* XXX unnecessary? */
#ifdef	DYING
    gcry_sexp_release(gc->pub_key);	gc->pub_key = NULL;
#endif
    gcry_sexp_release(gc->hash);	gc->hash = NULL;
    gcry_sexp_release(gc->sig);		gc->sig = NULL;

    rc = (err == 0);

if (_pgp_debug < 0)
fprintf(stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, dig, rc);

    return rc;
}

static
int rpmgcSignECDSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    int rc = 0;		/* assume failure. */
    rpmgc gc = dig->impl;
    gpg_error_t err;

#ifdef	NOTYET
    err = rpmgcErr(gc, "gcry_sexp_build",
		gcry_sexp_build (&data, NULL,
			"(data (flags raw) (value %m))", gc->x));
#endif

    err = rpmgcErr(gc, "ECDSA sign",
		gcry_pk_sign (&gc->sig, gc->hash, gc->sec_key));
if (_pgp_debug < 0 && gc->sig) rpmgcDump("gc->sig", gc->sig);

    rc = (err == 0);

if (_pgp_debug < 0)
fprintf(stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, dig, rc);
    return rc;
}

static
int rpmgcGenerateECDSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    rpmgc gc = dig->impl;
    int rc;

    gc->err = rpmgcErr(gc, "ECDSA gc->key_spec",
		gcry_sexp_build (&gc->key_spec, NULL,
			"(genkey (ECDSA (nbits %d)))", gc->nbits));
if (_pgp_debug < 0 && gc->key_spec) rpmgcDump("gc->key_spec", gc->key_spec);

    if (gc->err == 0)
	gc->err = rpmgcErr(gc, "ECDSA generate",
		gcry_pk_genkey (&gc->key_pair, gc->key_spec));
if (_pgp_debug < 0 && gc->key_pair) rpmgcDump("gc->key_pair", gc->key_pair);

    if (gc->err == 0) {
	gc->pub_key = gcry_sexp_find_token (gc->key_pair, "public-key", 0);
if (_pgp_debug < 0 && gc->pub_key) rpmgcDump("gc->pub_key", gc->pub_key);
	gc->sec_key = gcry_sexp_find_token (gc->key_pair, "private-key", 0);
if (_pgp_debug < 0 && gc->sec_key) rpmgcDump("gc->sec_key", gc->sec_key);
    }

#ifdef	NOTYET
    if (gc->key_spec) {
	gcry_sexp_release(gc->key_spec);
	gc->key_spec = NULL;
    }
#endif

    rc = (gc->err == 0);

if (_pgp_debug < 0)
fprintf(stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, dig, rc);

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
    int rc = 0;

    switch (itemno) {
    default:
assert(0);
    case 50:		/* ECDSA r */
    case 51:		/* ECDSA s */
    case 60:		/* ECDSA curve OID */
    case 61:		/* ECDSA Q */
	break;
    case 10:		/* RSA m**d */
	mpiname = "RSA m**d";	mpip = &gc->c;
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
    gc->err = rpmgcErr(gc, mpiname,
		gcry_mpi_scan(mpip, GCRYMPI_FMT_PGP, p, nb, &nscan) );
/*@=moduncon =noeffectuncon @*/
assert(nb == nscan);

    if (_pgp_debug < 0)
    {	unsigned nbits = gcry_mpi_get_nbits(*mpip);
	unsigned char * hex = NULL;
	size_t nhex = 0;
	gc->err = rpmgcErr(gc, "MPI print",
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
	gc->nbits = 0;
	gc->err = 0;
	gc->badok = 0;

	if (gc->key_spec) {
	    gcry_sexp_release(gc->key_spec);
	    gc->key_spec = NULL;
	}
	if (gc->key_pair) {
	    gcry_sexp_release(gc->key_pair);
	    gc->key_pair = NULL;
	}
	if (gc->pub_key) {
	    gcry_sexp_release(gc->pub_key);
	    gc->pub_key = NULL;
	}
	if (gc->sec_key) {
	    gcry_sexp_release(gc->sec_key);
	    gc->sec_key = NULL;
	}
	if (gc->hash) {
	    gcry_sexp_release(gc->hash);
	    gc->hash = NULL;
	}
	if (gc->sig) {
	    gcry_sexp_release(gc->sig);
	    gc->sig = NULL;
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

	gc->digest = _free(gc->digest);
	gc->digestlen = 0;
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
	gc->err = rpmgcErr(gc, "CLEAR_DEBUG_FLAGS",
		gcry_control(GCRYCTL_CLEAR_DEBUG_FLAGS, 3));
	gc->err = rpmgcErr(gc, "SET_VERBOSITY",
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
	gc->err = rpmgcErr(gc, "SET_VERBOSITY",
		gcry_control(GCRYCTL_SET_VERBOSITY, 3) );
	gc->err = rpmgcErr(gc, "SET_DEBUG_FLAGS",
		gcry_control(GCRYCTL_SET_DEBUG_FLAGS, 3) );
    }

    return (void *) gc;
}

struct pgpImplVecs_s rpmgcImplVecs = {
	rpmgcSetRSA, rpmgcVerifyRSA, NULL, NULL,
	rpmgcSetDSA, rpmgcVerifyDSA, NULL, NULL,
	rpmgcSetELG, NULL, NULL, NULL,
	rpmgcSetECDSA, rpmgcVerifyECDSA, rpmgcSignECDSA, rpmgcGenerateECDSA,
	rpmgcMpiItem, rpmgcClean,
	rpmgcFree, rpmgcInit
};

#endif


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


/*@unchecked@*/
static int _rpmgc_debug;

#define	SPEW(_t, _rc, _dig)	\
  { if ((_t) || _rpmgc_debug || _pgp_debug < 0) \
	fprintf(stderr, "<-- %s(%p) %s\t%s\n", __FUNCTION__, (_dig), \
		((_rc) ? "OK" : "BAD"), (_dig)->pubkey_algoN); \
  }

static const char * rpmgcHashAlgo2Name(uint32_t algo)
{
    return pgpValStr(pgpHashTbl, (rpmuint8_t)algo);
}

static const char * rpmgcPubkeyAlgo2Name(uint32_t algo)
{
    return pgpValStr(pgpPubkeyTbl, (rpmuint8_t)algo);
}

static void fail(const char *format, ...)
{
    va_list arg_ptr;

    va_start(arg_ptr, format);
    vfprintf(stderr, format, arg_ptr);
    va_end(arg_ptr);
    _pgp_error_count++;
}

static
void rpmgcDump(const char * msg, gcry_sexp_t sexp)
	/*@*/
{
    if (_rpmgc_debug || _pgp_debug) {
	size_t nb = gcry_sexp_sprint(sexp, GCRYSEXP_FMT_ADVANCED, NULL, 0);
	char * buf = alloca(nb+1);

	nb = gcry_sexp_sprint(sexp, GCRYSEXP_FMT_ADVANCED, buf, nb);
	buf[nb] = '\0';
	fprintf(stderr, "========== %s:\n%s", msg, buf);
    }
    return;
}

static
gcry_error_t rpmgcErr(rpmgc gc, const char * msg, gcry_error_t err)
	/*@*/
{
    /* XXX Don't spew on expected failures ... */
    if (err && gcry_err_code(err) != gc->badok)
	fprintf (stderr, "rpmgc: %s(0x%0x): %s/%s\n",
		msg, (unsigned)err, gcry_strsource(err), gcry_strerror(err));
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
pgpDigParams pubp = pgpGetPubkey(dig);
dig->pubkey_algoN = rpmgcPubkeyAlgo2Name(pubp->pubkey_algo);
dig->hash_algoN = rpmgcHashAlgo2Name(sigp->hash_algo);

    switch (sigp->hash_algo) {
    case PGPHASHALGO_MD5:
	hash_algo_name = "md5";
	break;
    case PGPHASHALGO_SHA1:
	hash_algo_name = "sha1";
	break;
    case PGPHASHALGO_RIPEMD160:
	hash_algo_name = "ripemd160";	/* XXX FIXME: RPM != GCRYPT name */
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

    xx = rpmDigestFinal(ctx, (void **)&gc->digest, &gc->digestlen, 0);

    /* Set RSA hash. */
    err = rpmgcErr(gc, "RSA c",
	    gcry_sexp_build(&gc->hash, NULL,
		"(data (flags pkcs1) (hash %s %b))", hash_algo_name, gc->digestlen, gc->digest) );
if (_pgp_debug < 0) rpmgcDump("gc->hash", gc->hash);

    /* Compare leading 16 bits of digest for quick check. */
    {	const rpmuint8_t *s = gc->digest;
	const rpmuint8_t *t = sigp->signhash16;
	rc = memcmp(s, t, sizeof(sigp->signhash16));
    }

SPEW(0, !rc, dig);
    return rc;
}

static
int rpmgcSetDSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    rpmgc gc = dig->impl;
    gcry_error_t err;
    int rc;
    int xx;
pgpDigParams pubp = pgpGetPubkey(dig);
dig->pubkey_algoN = rpmgcPubkeyAlgo2Name(pubp->pubkey_algo);
dig->hash_algoN = rpmgcHashAlgo2Name(sigp->hash_algo);

assert(sigp->hash_algo == rpmDigestAlgo(ctx));
    xx = rpmDigestFinal(ctx, (void **)&gc->digest, &gc->digestlen, 0);

    /* Set DSA hash. */
/*@-moduncon -noeffectuncon @*/
    {	gcry_mpi_t c = NULL;
	/* XXX truncate to 160 bits */
	err = rpmgcErr(gc, "DSA c",
		gcry_mpi_scan(&c, GCRYMPI_FMT_USG, gc->digest, 160/8, NULL));
	err = rpmgcErr(gc, "DSA gc->hash",
		gcry_sexp_build(&gc->hash, NULL,
			"(data (flags raw) (value %m))", c) );
	gcry_mpi_release(c);
if (_pgp_debug < 0) rpmgcDump("gc->hash", gc->hash);
    }
/*@=moduncon =noeffectuncon @*/

    /* Compare leading 16 bits of digest for quick check. */
    rc = memcmp(gc->digest, sigp->signhash16, sizeof(sigp->signhash16));
SPEW(0, !rc, dig);
    return rc;
}

static
int rpmgcSetELG(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@*/
{
    rpmgc gc = dig->impl;
    int rc = 1;		/* XXX always fail. */
    int xx;
pgpDigParams pubp = pgpGetPubkey(dig);
dig->pubkey_algoN = rpmgcPubkeyAlgo2Name(pubp->pubkey_algo);
dig->hash_algoN = rpmgcHashAlgo2Name(sigp->hash_algo);

assert(sigp->hash_algo == rpmDigestAlgo(ctx));
    xx = rpmDigestFinal(ctx, (void **)&gc->digest, &gc->digestlen, 0);

    /* Compare leading 16 bits of digest for quick check. */

SPEW(0, !rc, dig);
    return rc;
}

static
int rpmgcSetECDSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@*/
{
    rpmgc gc = dig->impl;
    int rc = 1;		/* assume failure. */
    gpg_error_t err;
    int xx;
pgpDigParams pubp = pgpGetPubkey(dig);
dig->pubkey_algoN = rpmgcPubkeyAlgo2Name(pubp->pubkey_algo);
dig->hash_algoN = rpmgcHashAlgo2Name(sigp->hash_algo);

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
    rc = 0;

SPEW(0, !rc, dig);
    return rc;
}

static int rpmgcErrChk(pgpDig dig, const char * msg, int rc, unsigned expected)
{
rpmgc gc = dig->impl;
    /* Was the return code the expected result? */
    rc = (gcry_err_code(gc->err) != expected);
    if (rc)
	fail("%s failed: %s\n", msg, gpg_strerror(gc->err));
/* XXX FIXME: pgpImplStrerror */
    return rc;	/* XXX 0 on success */
}

static int rpmgcAvailable(rpmgc gc, int algo, int rc)
{
    /* Permit non-certified algo's if not in FIPS mode. */
    if (rc && !gc->in_fips_mode)
	rc = 0;
#ifdef	NOTNOW
    if (rc)
	rpmlog(RPMLOG_INFO,"  algorithm %d not available in fips mode\n", algo);
#else
/* XXX FIXME: refactor back into trsa.c */
    if (rc)
	fprintf(stderr,"  algorithm %d not available in fips mode\n", algo);
#endif
    return rc;	/* XXX 0 on success */
}

static int rpmgcAvailableCipher(pgpDig dig, int algo)
{
    return rpmgcAvailable(dig->impl, algo, gcry_cipher_test_algo(algo));
}

static int rpmgcAvailableDigest(pgpDig dig, int algo)
{
    int rc = 0;	/* assume available */
    rc = rpmgcAvailable(dig->impl, algo,
    	(gcry_md_test_algo(algo) || algo == PGPHASHALGO_MD5));
    return rc;
}

static int rpmgcAvailablePubkey(pgpDig dig, int algo)
{
    int rc = 0;	/* assume available */
    rc = rpmgcAvailable(dig->impl, algo, gcry_pk_test_algo(algo));
    return rc;
}

static
int rpmgcVerify(pgpDig dig)
{
    rpmgc gc = dig->impl;
    int rc;
pgpDigParams pubp = pgpGetPubkey(dig);
pgpDigParams sigp = pgpGetSignature(dig);
dig->pubkey_algoN = rpmgcPubkeyAlgo2Name(pubp->pubkey_algo);
dig->hash_algoN = rpmgcHashAlgo2Name(sigp->hash_algo);

    if (gc->sig == NULL) {
	pgpDigParams pubp = pgpGetPubkey(dig);
	switch (pubp->pubkey_algo) {
	case PGPPUBKEYALGO_RSA:
assert(gc->c);
	    gc->err = rpmgcErr(gc, "RSA gc->sig",
		gcry_sexp_build(&gc->sig, NULL,
			"(sig-val (RSA (s %m)))", gc->c) );
	    break;
	case PGPPUBKEYALGO_DSA:
	case PGPPUBKEYALGO_ELGAMAL:	/* XXX FIXME: untested. */
assert(gc->r);
assert(gc->s);
	    gc->err = rpmgcErr(gc, "DSA gc->sig",
		gcry_sexp_build(&gc->sig, NULL,
			"(sig-val (DSA (r %m) (s %m)))", gc->r, gc->s) );
	    break;
	case PGPPUBKEYALGO_ECDSA:	/* XXX FIXME */
	default:
assert(0);
	    break;
	}
if (_pgp_debug < 0)
rpmgcDump("gc->sig", gc->sig);
    }

    if (gc->pub_key == NULL) {
	pgpDigParams pubp = pgpGetPubkey(dig);
	switch (pubp->pubkey_algo) {
	case PGPPUBKEYALGO_RSA:
assert(gc->n);
assert(gc->e);
	    gc->err = rpmgcErr(gc, "RSA gc->pub_key",
		gcry_sexp_build(&gc->pub_key, NULL,
			"(public-key (RSA (n %m) (e %m)))", gc->n, gc->e) );
	    break;
	case PGPPUBKEYALGO_DSA:
assert(gc->p);
assert(gc->q);
assert(gc->g);
assert(gc->y);
	    gc->err = rpmgcErr(gc, "DSA gc->pub_key",
		gcry_sexp_build(&gc->pub_key, NULL,
			"(public-key (DSA (p %m) (q %m) (g %m) (y %m)))",
			gc->p, gc->q, gc->g, gc->y) );
	    break;
	case PGPPUBKEYALGO_ELGAMAL:	/* XXX FIXME: untested. */
assert(gc->p);
assert(gc->g);
assert(gc->y);
	    gc->err = rpmgcErr(gc, "ELG gc->pub_key",
		gcry_sexp_build(&gc->pub_key, NULL,
			"(public-key (ELG (p %m) (g %m) (y %m)))",
			gc->p, gc->g, gc->y) );
	    break;
	case PGPPUBKEYALGO_ECDSA:
	default:
assert(0);
	    break;
	}
if (_pgp_debug < 0)
rpmgcDump("gc->pub_key", gc->pub_key);
    }

    /* Verify the signature. */
    gc->err = rpmgcErr(gc, "gcry_pk_verify",
		gcry_pk_verify (gc->sig, gc->hash, gc->pub_key));

    rc = (gc->err == 0);

SPEW(0, rc, dig);
    return rc;		/* XXX 1 on success */
}

static
int rpmgcSign(pgpDig dig)
{
    rpmgc gc = dig->impl;
    int rc;
pgpDigParams pubp = pgpGetPubkey(dig);
pgpDigParams sigp = pgpGetSignature(dig);
dig->pubkey_algoN = rpmgcPubkeyAlgo2Name(pubp->pubkey_algo);
dig->hash_algoN = rpmgcHashAlgo2Name(sigp->hash_algo);

    /* Sign the hash. */
    gc->err = rpmgcErr(gc, "gcry_pk_sign",
		gcry_pk_sign (&gc->sig, gc->hash, gc->sec_key));

if (_pgp_debug < 0 && gc->sig) rpmgcDump("gc->sig", gc->sig);

    rc = (gc->err == 0);

SPEW(!rc, rc, dig);
    return rc;		/* XXX 1 on success */
}

static
int rpmgcGenerate(pgpDig dig)
	/*@*/
{
    rpmgc gc = dig->impl;
    int rc;
pgpDigParams pubp = pgpGetPubkey(dig);
dig->pubkey_algoN = rpmgcPubkeyAlgo2Name(pubp->pubkey_algo);

/* XXX FIXME: gc->{key_spec,key_pair} could be local. */
/* XXX FIXME: gc->qbits w DSA? curve w ECDSA? other params? */
    switch (pubp->pubkey_algo) {
    case PGPPUBKEYALGO_RSA:
if (gc->nbits == 0) gc->nbits = 1024;   /* XXX FIXME */
	gc->err = rpmgcErr(gc, "gc->key_spec",
		gcry_sexp_build(&gc->key_spec, NULL,
			gc->in_fips_mode
			    ? "(genkey (RSA (nbits %d)))"
			    : "(genkey (RSA (nbits %d)(transient-key)))",
			gc->nbits));
	break;
    case PGPPUBKEYALGO_DSA:
if (gc->nbits == 0) gc->nbits = 1024;   /* XXX FIXME */
	gc->err = rpmgcErr(gc, "gc->key_spec",
		gcry_sexp_build(&gc->key_spec, NULL,
			gc->in_fips_mode
			    ? "(genkey (DSA (nbits %d)))"
			    : "(genkey (DSA (nbits %d)(transient-key)))",
			gc->nbits));
	break;
    case PGPPUBKEYALGO_ELGAMAL:	/* XXX FIXME: untested. */
if (gc->nbits == 0) gc->nbits = 1024;   /* XXX FIXME */
	gc->err = rpmgcErr(gc, "gc->key_spec",
		gcry_sexp_build(&gc->key_spec, NULL,
			gc->in_fips_mode
			    ? "(genkey (ELG (nbits %d)))"
			    : "(genkey (ELG (nbits %d)(transient-key)))",
			gc->nbits));
	break;
    case PGPPUBKEYALGO_ECDSA:
if (gc->nbits == 0) gc->nbits = 256;   /* XXX FIXME */
#ifdef	DYING
	gc->err = rpmgcErr(gc, "gc->key_spec",
		gcry_sexp_build(&gc->key_spec, NULL,
			gc->in_fips_mode
			    ? "(genkey (ECDSA (nbits %d)))"
			    : "(genkey (ECDSA (nbits %d)(transient-key)))",
			gc->nbits));
#else
	gc->err = rpmgcErr(gc, "gc->key_spec",
		gcry_sexp_build(&gc->key_spec, NULL,
			gc->in_fips_mode
			    ? "(genkey (ECDSA (curve prime256v1)))"
			    : "(genkey (ECDSA (curve prime256v1)(transient-key)))",
			gc->nbits));
#endif
	break;
    default:
assert(0);
	break;
    }
    if (gc->err)
	goto exit;
if ((_rpmgc_debug || _pgp_debug < 0) && gc->key_spec) rpmgcDump("gc->key_spec", gc->key_spec);

    /* Generate the key pair. */
    gc->err = rpmgcErr(gc, "gc->key_pair",
		gcry_pk_genkey(&gc->key_pair, gc->key_spec));
    if (gc->err)
	goto exit;
if ((_rpmgc_debug || _pgp_debug < 0) && gc->key_pair) rpmgcDump("gc->key_pair", gc->key_pair);

    gc->pub_key = gcry_sexp_find_token(gc->key_pair, "public-key", 0);
    if (gc->pub_key == NULL)
/* XXX FIXME: refactor errmsg here. */
	goto exit;
if ((_rpmgc_debug || _pgp_debug < 0) && gc->pub_key) rpmgcDump("gc->pub_key", gc->pub_key);

    gc->sec_key = gcry_sexp_find_token(gc->key_pair, "private-key", 0);
    if (gc->sec_key == NULL)
/* XXX FIXME: refactor errmsg here. */
	goto exit;
if ((_rpmgc_debug || _pgp_debug < 0) && gc->sec_key) rpmgcDump("gc->sec_key", gc->sec_key);

exit:

    rc = (gc->err == 0 && gc->pub_key && gc->sec_key);

#ifdef	NOTYET
if (gc->key_spec) {
    gcry_sexp_release(gc->key_spec);
    gc->key_spec = NULL;
}
if (gc->key_pair) {
    gcry_sexp_release(gc->key_pair);
    gc->key_pair = NULL;
}
#endif

SPEW(!rc, rc, dig);
    return rc;		/* XXX 1 on success */
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
        gc->digest = _free(gc->digest);
        gc->digestlen = 0;

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

    rpmgcClean(impl);

    if (--rpmgc_initialized == 0 && _pgp_debug < 0) {
	rpmgc gc = impl;
	gc->err = rpmgcErr(gc, "CLEAR_DEBUG_FLAGS",
		gcry_control(GCRYCTL_CLEAR_DEBUG_FLAGS, 3));
	gc->err = rpmgcErr(gc, "SET_VERBOSITY",
		gcry_control(GCRYCTL_SET_VERBOSITY, 0) );
    }

    impl = _free(impl);

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
	rpmgcSetRSA,
	rpmgcSetDSA,
	rpmgcSetELG,
	rpmgcSetECDSA,

	rpmgcErrChk,
	rpmgcAvailableCipher, rpmgcAvailableDigest, rpmgcAvailablePubkey,
	rpmgcVerify, rpmgcSign, rpmgcGenerate,

	rpmgcMpiItem, rpmgcClean,
	rpmgcFree, rpmgcInit
};

#endif


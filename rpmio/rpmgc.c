/** \ingroup rpmpgp
 * \file rpmio/rpmgc.c
 */

#include "system.h"

#include <rpmiotypes.h>
#include <rpmmacro.h>
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

/*==============================================================*/
/* Swiped (and largely rewritten) from LGPL gnupg/common/openpgp-oid.c */

/* Helper for openpgp_oid_from_str.  */
static size_t
make_flagged_int(unsigned long value, unsigned char * b, size_t nb)
{
    int more = 0;
    int shift;

    /* fixme: figure out the number of bits in an ulong and start with
       that value as shift (after making it a multiple of 7) a more
       straigtforward implementation is to do it in reverse order using
       a temporary buffer - saves a lot of compares */
    for (more = 0, shift = 28; shift > 0; shift -= 7) {
	if (more || value >= (1UL << shift)) {
	    b[nb++] = 0x80 | (value >> shift);
	    value -= (value >> shift) << shift;
	    more = 1;
	}
    }
    b[nb++] = value;
    return nb;
}

/*
 * Convert the OID given in dotted decimal form in STRING to an DER
 * encoding and store it as an opaque value at R_MPI.  The format of
 * the DER encoded is not a regular ASN.1 object but the modified
 * format as used by OpenPGP for the ECC curve description.  On error
 * the function returns and error code an NULL is stored at R_BUG.
 * Note that scanning STRING stops at the first white space
 * character.
 */
static
int openpgp_oid_from_str(const char * s, gcry_mpi_t * r_mpi)
{
    const char * se = s;
    size_t ns = (s ? strlen(s) : 0);
    unsigned char * b = xmalloc(ns + 2 + 1);
    size_t nb = 1;	/* count the length byte */
    const char *endp;
    unsigned long val;
    unsigned long val1 = 0;
    int arcno = 0;
    int rc = -1;	/* assume failure */

    *r_mpi = NULL;

    if (s == NULL || s[0] == '\0')
	goto exit;

    do {
	arcno++;
	val = strtoul(se, (char **) &endp, 10);
	if (!xisdigit(*se) || !(*endp == '.' || *endp == '\0'))
	    goto exit;

	if (*endp == '.')
	    se = endp + 1;

	switch (arcno) {
	case 1:
	    if (val > 2)	/* Not allowed.  */
		goto exit;
	    val1 = val;
	    break;
	case 2:			/* Combine the 1st 2 arcs into octet. */
	    if (val1 < 2) {
		if (val > 39)
		    goto exit;
		b[nb++] = val1 * 40 + val;
	    } else {
		val += 80;
		nb = make_flagged_int(val, b, nb);
	    }
	    break;
	default:
	    nb = make_flagged_int(val, b, nb);
	    break;
	}
    } while (*endp == '.');

    if (arcno == 1 || nb < 2 || nb > 254)	/* Can't encode 1st arc.  */
	goto exit;

    *b = nb - 1;
    *r_mpi = gcry_mpi_set_opaque(NULL, b, nb * 8);
    if (*r_mpi)		/* Success? */
	rc = 0;

exit:
    if (rc)
	b = _free(b);
    return rc;
}

/*
 * Return a malloced string represenation of the OID in the opaque MPI
 * A. In case of an error NULL is returned and ERRNO is set.
 */
static
char *openpgp_oid_to_str(gcry_mpi_t a)
{
    unsigned int nbits = 0;
    const unsigned char * b = (a && gcry_mpi_get_flag(a, GCRYMPI_FLAG_OPAQUE))
    	? gcry_mpi_get_opaque(a, &nbits)
	: NULL;
    size_t nb = (nbits + 7) / 8;
    /* 2 chars in prefix, (3+1) decimalchars/byte, trailing NUL, skip length */
    size_t nt = (2 + ((nb ? nb-1 : 0) * (3+1)) + 1);
    char *t = xmalloc(nt);
    char *te = t;
    size_t ix = 0;
    unsigned long valmask = (unsigned long) 0xfe << (8 * (sizeof(valmask) - 1));
    unsigned long val;

    *te = '\0';

    /* Check oid consistency, skipping the length byte.  */
    if (b == NULL || nb == 0 || *b++ != --nb)
	goto invalid;

    /* Add the prefix and decode the 1st byte */
    if (b[ix] < 40)
	te += sprintf(te, "0.%d", b[ix]);
    else if (b[ix] < 80)
	te += sprintf(te, "1.%d", b[ix] - 40);
    else {
	val = b[ix] & 0x7f;
	while ((b[ix] & 0x80) && ++ix < nb) {
	    if ((val & valmask))
		goto overflow;
	    val <<= 7;
	    val |= b[ix] & 0x7f;
	}
	val -= 80;
	te += sprintf(te, "2.%lu", val);
    }
    *te = '\0';

    /* Append the (dotted) oid integers */
    for (ix++; ix < nb; ix++) {
	val = b[ix] & 0x7f;
	while ((b[ix] & 0x80) && ++ix < nb) {
	    if ((val & valmask))
		goto overflow;
	    val <<= 7;
	    val |= b[ix] & 0x7f;
	}
	te += sprintf(te, ".%lu", val);
    }
    *te = '\0';
    goto exit;

overflow:
    /*
     * Return a special OID (gnu.gnupg.badoid) to indicate the error
     * case.  The OID is broken and thus we return one which can't do
     * any harm.  Formally this does not need to be a bad OID but an OID
     * with an arc that can't be represented in a 32 bit word is more
     * than likely corrupt.
     */
    t = _free(t);
    t = xstrdup("1.3.6.1.4.1.11591.2.12242973");
    goto exit;

invalid:
    errno = EINVAL;
    t = _free(t);
    goto exit;

exit:
    return t;
}

/*==============================================================*/

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
	char * buf = (char *) alloca(nb+1);

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
    rpmgc gc = (rpmgc) dig->impl;
    gcry_error_t err;
    const char * hash_algo_name = NULL;
    int rc = 1;		/* assume error */
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
	goto exit;

    xx = rpmDigestFinal(ctx, (void **)&gc->digest, &gc->digestlen, 0);
    ctx = NULL;		/* XXX avoid double free */

    /* Set RSA hash. */
    err = rpmgcErr(gc, "RSA c",
	    gcry_sexp_build(&gc->hash, NULL,
		"(data (flags pkcs1) (hash %s %b))", hash_algo_name, gc->digestlen, gc->digest) );
if (_pgp_debug < 0) rpmgcDump("gc->hash", gc->hash);

    /* Compare leading 16 bits of digest for quick check. */
    {	const rpmuint8_t *s = (const rpmuint8_t *) gc->digest;
	const rpmuint8_t *t = sigp->signhash16;
	rc = memcmp(s, t, sizeof(sigp->signhash16));
    }

exit:
    if (ctx) {		/* XXX Free the context on error returns. */
	xx = rpmDigestFinal(ctx, NULL, NULL, 0);
	ctx = NULL;
    }
SPEW(0, !rc, dig);
    return rc;
}

static
int rpmgcSetDSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    rpmgc gc = (rpmgc) dig->impl;
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
	/* XXX truncate to 160 bits for DSA2? */
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

    /* XXX FIXME: avoid spurious "BAD" error msg while signing. */
    if (rc && sigp->signhash16[0] == 0 && sigp->signhash16[1] == 0)
	rc = 0;

SPEW(0, !rc, dig);
    return rc;
}

static
int rpmgcSetELG(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@*/
{
    rpmgc gc = (rpmgc) dig->impl;
    gcry_error_t err;
    int rc = 0;		/* XXX always fail. */
    int xx;
pgpDigParams pubp = pgpGetPubkey(dig);
dig->pubkey_algoN = rpmgcPubkeyAlgo2Name(pubp->pubkey_algo);
dig->hash_algoN = rpmgcHashAlgo2Name(sigp->hash_algo);

assert(sigp->hash_algo == rpmDigestAlgo(ctx));
    xx = rpmDigestFinal(ctx, (void **)&gc->digest, &gc->digestlen, 0);

    /* Set ELG hash. */
/*@-moduncon -noeffectuncon @*/
    {	gcry_mpi_t c = NULL;
	/* XXX truncate to 160 bits for DSA2? */
	err = rpmgcErr(gc, "ELG c",
		gcry_mpi_scan(&c, GCRYMPI_FMT_USG, gc->digest, 160/8, NULL));
	err = rpmgcErr(gc, "ELG gc->hash",
		gcry_sexp_build(&gc->hash, NULL,
			"(data (flags raw) (value %m))", c) );
	gcry_mpi_release(c);
if (1 || _pgp_debug < 0) rpmgcDump("gc->hash", gc->hash);
    }
/*@=moduncon =noeffectuncon @*/

    /* Compare leading 16 bits of digest for quick check. */
    rc = memcmp(gc->digest, sigp->signhash16, sizeof(sigp->signhash16));

SPEW(0, !rc, dig);
    return rc;
}

static
int rpmgcSetECDSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@*/
{
    rpmgc gc = (rpmgc) dig->impl;
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

    /* Set ECDSA hash. */
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
    rc = memcmp(gc->digest, sigp->signhash16, sizeof(sigp->signhash16));

SPEW(0, !rc, dig);
    return rc;
}

static int rpmgcErrChk(pgpDig dig, const char * msg, int rc, unsigned expected)
{
rpmgc gc = (rpmgc) dig->impl;
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
    rpmgc gc = (rpmgc) dig->impl;
    return rpmgcAvailable(gc, algo, gcry_cipher_test_algo(algo));
}

static int rpmgcAvailableDigest(pgpDig dig, int algo)
{
    rpmgc gc = (rpmgc) dig->impl;
    int rc = 0;	/* assume available */
    rc = rpmgcAvailable(gc, algo,
    	(gcry_md_test_algo(algo) || algo == PGPHASHALGO_MD5));
    return rc;
}

static int rpmgcAvailablePubkey(pgpDig dig, int algo)
{
    rpmgc gc = (rpmgc) dig->impl;
    int rc = 0;	/* assume available */
    rc = rpmgcAvailable(gc, algo, gcry_pk_test_algo(algo));
    return rc;
}

static char * _curve_nist = "NIST P-256";	/* XXX FIXME */
static char * _curve_oid = "1.2.840.10045.3.1.7";/* XXX FIXME */
static char * _curve_x962 = "prime256v1";	/* XXX FIXME */
static char * _curve_secp = "secp256r1";	/* XXX FIXME */

static
int rpmgcVerify(pgpDig dig)
{
    rpmgc gc = (rpmgc) dig->impl;
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
assert(gc->r);
assert(gc->s);
	    gc->err = rpmgcErr(gc, "DSA gc->sig",
		gcry_sexp_build(&gc->sig, NULL,
			"(sig-val (DSA (r %m) (s %m)))", gc->r, gc->s) );
	    break;
	case PGPPUBKEYALGO_ELGAMAL:	/* XXX FIXME: untested. */
assert(gc->r);
assert(gc->s);
	    gc->err = rpmgcErr(gc, "ELG gc->sig",
		gcry_sexp_build(&gc->sig, NULL,
			"(sig-val (ELG (r %m) (s %m)))", gc->r, gc->s) );
	    break;
	case PGPPUBKEYALGO_ECDSA:	/* XXX FIXME */
assert(gc->r);
assert(gc->s);
	    gc->err = rpmgcErr(gc, "ECDSA gc->sig",
		gcry_sexp_build(&gc->sig, NULL,
			"(sig-val (ECDSA (r %m) (s %m)))", gc->r, gc->s) );
	    break;
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
/* gc->d priv_key */
/* gc->p priv_key optional */
/* gc->q priv_key optional */
/* gc->u priv_key optional */
	    gc->err = rpmgcErr(gc, "RSA gc->pub_key",
		gcry_sexp_build(&gc->pub_key, NULL,
			"(public-key (RSA (n %m) (e %m)))", gc->n, gc->e) );
	    break;
	case PGPPUBKEYALGO_DSA:
assert(gc->p);
assert(gc->q);
assert(gc->g);
assert(gc->y);
/* gc->x priv_key */
	    gc->err = rpmgcErr(gc, "DSA gc->pub_key",
		gcry_sexp_build(&gc->pub_key, NULL,
			"(public-key (DSA (p %m) (q %m) (g %m) (y %m)))",
			gc->p, gc->q, gc->g, gc->y) );
	    break;
	case PGPPUBKEYALGO_ELGAMAL:	/* XXX FIXME: untested. */
assert(gc->p);
assert(gc->g);
assert(gc->y);
/* gc->x priv_key */
	    gc->err = rpmgcErr(gc, "ELG gc->pub_key",
		gcry_sexp_build(&gc->pub_key, NULL,
			"(public-key (ELG (p %m) (g %m) (y %m)))",
			gc->p, gc->g, gc->y) );
	    break;
	case PGPPUBKEYALGO_ECDSA:	/* XXX FIXME */
assert(gc->o);
/* gc->p curve */
/* gc->a curve */
/* gc->b curve */
/* gc->g curve */
/* gc->n curve */
assert(gc->q);
/* gc->d priv_key */
	    {	unsigned int nbits = 0;
		rpmuint8_t * b = gcry_mpi_get_opaque(gc->o, &nbits);
		size_t nb = (nbits+7)/8;
		char * t;
fprintf(stderr, "oid: %p[%u] %s\n", b, nbits, pgpHexStr(b, nb));
		t = openpgp_oid_to_str(gc->o);
fprintf(stderr, "\t%s\n", t);
		t = _free(t);
	    }
	    gc->err = rpmgcErr(gc, "ECDSA gc->pub_key",
		gcry_sexp_build(&gc->pub_key, NULL,
			"(public-key (ECDSA (curve \"%s\") (q %m))",
			_curve_x962, gc->q) );
	    break;
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
    rpmgc gc = (rpmgc) dig->impl;
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
    rpmgc gc = (rpmgc) dig->impl;
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
	/* XXX gcry_sexp_build %s format is fubar in libgcrypt-1.5.3 ?!? */
	{   char * t = rpmExpand("(genkey (ECDSA (curve \"", _curve_nist, "\")",
				gc->in_fips_mode ? "" : "(transient-key)",
				"))", NULL);
	    gc->err = rpmgcErr(gc, "gc->key_spec",
		gcry_sexp_build(&gc->key_spec, NULL, t));
	    t = _free(t);
	}
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
    rpmgc gc = (rpmgc) dig->impl;
    size_t nb;
    const char * mpiname = "";
    gcry_mpi_t * mpip = NULL;
    size_t nscan = 0;
    int rc = 0;

    switch (itemno) {
    default:
assert(0);
    case 50:		/* ECDSA r */
	mpiname = "ECDSA r";	mpip = &gc->r;
	break;
    case 51:		/* ECDSA s */
	mpiname = "ECDSA s";	mpip = &gc->s;
	break;
    case 60:		/* ECDSA curve OID */
	mpiname = "ECDSA o";	mpip = &gc->o;
	break;
    case 61:		/* ECDSA Q */
	mpiname = "ECDSA Q";	mpip = &gc->q;
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

    if (mpip == NULL)
	goto exit;

/*@-moduncon -noeffectuncon @*/
    if (itemno == 60) {
	nb = (pend >= p ? (pend - p) : 0);
assert(nb >= 2 && nb <= 254);
	*mpip = gcry_mpi_set_opaque(*mpip, (void *)p, 8*nb);
    } else {
	nb = pgpMpiLen(p);
	gc->err = rpmgcErr(gc, mpiname,
		gcry_mpi_scan(mpip, GCRYMPI_FMT_PGP, p, nb, &nscan) );
/*@=moduncon =noeffectuncon @*/
assert(nb == nscan);
    }

    if (_pgp_debug < 0)
    {	unsigned nbits = gcry_mpi_get_nbits(*mpip);
	unsigned char * hex = NULL;
	size_t nhex = 0;
	gc->err = rpmgcErr(gc, "MPI print",
		gcry_mpi_aprint(GCRYMPI_FMT_HEX, &hex, &nhex, *mpip) );
	fprintf(stderr, "*** %s\t%5d:%s\n", mpiname, (int)nbits, hex);
	hex = _free(hex);
    }

exit:
    return rc;
}
/*@=globuse =mustmod @*/

/*@-mustmod@*/
static
void rpmgcClean(void * impl)
	/*@modifies impl @*/
{
    rpmgc gc = (rpmgc) impl;
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
	if (gc->o) {
	    gcry_mpi_release(gc->o);
	    gc->o = NULL;
	}
	if (gc->a) {	/* XXX unused */
	    gcry_mpi_release(gc->a);
	    gc->a = NULL;
	}
	if (gc->b) {	/* XXX unused */
	    gcry_mpi_release(gc->b);
	    gc->b = NULL;
	}

	gc->oid = _free(gc->oid);
	gc->curve = _free(gc->curve);

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
	rpmgc gc = (rpmgc) impl;
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
    rpmgc gc = (rpmgc) xcalloc(1, sizeof(*gc));

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

int rpmgcExportPubkey(pgpDig dig)
{
    uint8_t pkt[8192];
    uint8_t * be = pkt;
    size_t pktlen;
    time_t now = time(NULL);
    uint32_t bt = now;
    uint16_t bn;
    pgpDigParams pubp = pgpGetPubkey(dig);
    rpmgc gc = (rpmgc) dig->impl;
    int xx;

    *be++ = 0x80 | (PGPTAG_PUBLIC_KEY << 2) | 0x01;
    be += 2;

    *be++ = 0x04;
    *be++ = (bt >> 24);
    *be++ = (bt >> 16);
    *be++ = (bt >>  8);
    *be++ = (bt      );
    *be++ = pubp->pubkey_algo;

assert(gc->pub_key);
xx = gcry_sexp_extract_param (gc->pub_key, NULL, "p q g y", &gc->p, &gc->q, &gc->g, &gc->y, NULL);
assert(gc->p);
assert(gc->q);
assert(gc->g);
assert(gc->y);
    bn = gcry_mpi_get_nbits(gc->p);
    bn += 7; bn &= ~7;
    *be++ = (bn >> 8);	*be++ = (bn     );
    xx = gcry_mpi_print(GCRYMPI_FMT_USG, be, bn/8, NULL, gc->p);
    be += bn/8;

    bn = gcry_mpi_get_nbits(gc->q);
    bn += 7; bn &= ~7;
    *be++ = (bn >> 8);	*be++ = (bn     );
    xx = gcry_mpi_print(GCRYMPI_FMT_USG, be, bn/8, NULL, gc->q);
    be += bn/8;

    bn = gcry_mpi_get_nbits(gc->g);
    bn += 7; bn &= ~7;
    *be++ = (bn >> 8);	*be++ = (bn     );
    xx = gcry_mpi_print(GCRYMPI_FMT_USG, be, bn/8, NULL, gc->g);
    be += bn/8;

    bn = gcry_mpi_get_nbits(gc->y);
    bn += 7; bn &= ~7;
    *be++ = (bn >> 8);	*be++ = (bn     );
    xx = gcry_mpi_print(GCRYMPI_FMT_USG, be, bn/8, NULL, gc->y);
    be += bn/8;

    pktlen = (be - pkt);
    bn = pktlen - 3;
    pkt[1] = (bn >> 8);
    pkt[2] = (bn     );

    xx = pgpPubkeyFingerprint(pkt, pktlen, pubp->signid);

    dig->pub = memcpy(xmalloc(pktlen), pkt, pktlen);
    dig->publen = pktlen;

    return 0;
}

int rpmgcExportSignature(pgpDig dig, /*@only@*/ DIGEST_CTX ctx)
{
    uint8_t pkt[8192];
    uint8_t * be = pkt;
    uint8_t * h;
    size_t pktlen;
    time_t now = time(NULL);
    uint32_t bt;
    uint16_t bn;
    pgpDigParams pubp = pgpGetPubkey(dig);
    pgpDigParams sigp = pgpGetSignature(dig);
    rpmgc gc = (rpmgc) dig->impl;
    int xx;

    sigp->tag = PGPTAG_SIGNATURE;
    *be++ = 0x80 | (sigp->tag << 2) | 0x01;
    be += 2;				/* pktlen */

    sigp->hash = be;
    *be++ = sigp->version = 0x04;		/* version */
    *be++ = sigp->sigtype = PGPSIGTYPE_BINARY;	/* sigtype */
    *be++ = sigp->pubkey_algo = pubp->pubkey_algo;	/* pubkey_algo */
    *be++ = sigp->hash_algo;		/* hash_algo */

    be += 2;				/* skip hashd length */
    h = (uint8_t *) be;

    *be++ = 1 + 4;			/* signature creation time */
    *be++ = PGPSUBTYPE_SIG_CREATE_TIME;
    bt = now;
    *be++ = sigp->time[0] = (bt >> 24);
    *be++ = sigp->time[1] = (bt >> 16);
    *be++ = sigp->time[2] = (bt >>  8);
    *be++ = sigp->time[3] = (bt      );

    *be++ = 1 + 4;			/* signature expiration time */
    *be++ = PGPSUBTYPE_SIG_EXPIRE_TIME;
    bt = 30 * 24 * 60 * 60;		/* XXX 30 days from creation */
    *be++ = sigp->expire[0] = (bt >> 24);
    *be++ = sigp->expire[1] = (bt >> 16);
    *be++ = sigp->expire[2] = (bt >>  8);
    *be++ = sigp->expire[3] = (bt      );

/* key expiration time (only on a self-signature) */

    *be++ = 1 + 1;			/* exportable certification */
    *be++ = PGPSUBTYPE_EXPORTABLE_CERT;
    *be++ = 0;

    *be++ = 1 + 1;			/* revocable */
    *be++ = PGPSUBTYPE_REVOCABLE;
    *be++ = 0;

/* notation data */

    sigp->hashlen = (be - h);		/* set hashed length */
    h[-2] = (sigp->hashlen >> 8);
    h[-1] = (sigp->hashlen     );
    sigp->hashlen += sizeof(struct pgpPktSigV4_s);

    if (sigp->hash != NULL)
	xx = rpmDigestUpdate(ctx, sigp->hash, sigp->hashlen);

    if (sigp->version == (rpmuint8_t) 4) {
	uint8_t trailer[6];
	trailer[0] = sigp->version;
	trailer[1] = (rpmuint8_t)0xff;
	trailer[2] = (sigp->hashlen >> 24);
	trailer[3] = (sigp->hashlen >> 16);
	trailer[4] = (sigp->hashlen >>  8);
	trailer[5] = (sigp->hashlen      );
	xx = rpmDigestUpdate(ctx, trailer, sizeof(trailer));
    }

    sigp->signhash16[0] = 0x00;
    sigp->signhash16[1] = 0x00;
    xx = pgpImplSetDSA(ctx, dig, sigp);	/* XXX signhash16 check always fails */
    h = (uint8_t *) gc->digest;
    sigp->signhash16[0] = h[0];
    sigp->signhash16[1] = h[1];

    /* XXX pgpImplVec forces "--usecrypto foo" to also be used */
    xx = pgpImplSign(dig);
assert(xx == 1);

    be += 2;				/* skip unhashed length. */
    h = be;

    *be++ = 1 + 8;			/* issuer key ID */
    *be++ = PGPSUBTYPE_ISSUER_KEYID;
    *be++ = pubp->signid[0];
    *be++ = pubp->signid[1];
    *be++ = pubp->signid[2];
    *be++ = pubp->signid[3];
    *be++ = pubp->signid[4];
    *be++ = pubp->signid[5];
    *be++ = pubp->signid[6];
    *be++ = pubp->signid[7];

    bt = (be - h);			/* set unhashed length */
    h[-2] = (bt >> 8);
    h[-1] = (bt     );

    *be++ = sigp->signhash16[0];	/* signhash16 */
    *be++ = sigp->signhash16[1];

assert(gc->sig);
xx = gcry_sexp_extract_param (gc->sig, NULL, "r s", &gc->r, &gc->s, NULL);
assert(gc->r);
assert(gc->s);
    bn = gcry_mpi_get_nbits(gc->r);
    bn += 7;	bn &= ~7;
    *be++ = (bn >> 8);
    *be++ = (bn     );
    xx = gcry_mpi_print(GCRYMPI_FMT_USG, be, bn/8, NULL, gc->r);
    be += bn/8;

    bn = gcry_mpi_get_nbits(gc->s);
    bn += 7;	bn &= ~7;
    *be++ = (bn >> 8);
    *be++ = (bn     );
    xx = gcry_mpi_print(GCRYMPI_FMT_USG, be, bn/8, NULL, gc->s);
    be += bn/8;

    pktlen = (be - pkt);		/* packet length */
    bn = pktlen - 3;
    pkt[1] = (bn >> 8);
    pkt[2] = (bn     );

    dig->sig = memcpy(xmalloc(pktlen), pkt, pktlen);
    dig->siglen = pktlen;

    return 0;

}

#endif	/* WITH_GCRYPT */

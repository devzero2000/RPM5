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

/**
 * Convert hex to binary nibble.
 * @param c            hex character
 * @return             binary nibble
 */
static
unsigned char nibble(char c)
	/*@*/
{
    if (c >= '0' && c <= '9')
	return (unsigned char) (c - '0');
    if (c >= 'A' && c <= 'F')
	return (unsigned char)((int)(c - 'A') + 10);
    if (c >= 'a' && c <= 'f')
	return (unsigned char)((int)(c - 'a') + 10);
    return (unsigned char) '\0';
}

static
int rpmgcSetRSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    rpmgc gc = dig->impl;
    unsigned int nbits = gcry_mpi_get_nbits(gc->c);
    unsigned int nb = (nbits + 7) >> 3;
    const char * prefix;
    const char * hexstr;
    const char * s;
    rpmuint8_t signhash16[2];
    char * tt;
    gcry_mpi_t c = NULL;
    gcry_error_t rc;
    int xx;

    /* XXX Values from PKCS#1 v2.1 (aka RFC-3447) */
    switch (sigp->hash_algo) {
    case PGPHASHALGO_MD5:
	prefix = "3020300c06082a864886f70d020505000410";
	break;
    case PGPHASHALGO_SHA1:
	prefix = "3021300906052b0e03021a05000414";
	break;
    case PGPHASHALGO_RIPEMD160:
	prefix = "3021300906052b2403020105000414";
	break;
    case PGPHASHALGO_MD2:
	prefix = "3020300c06082a864886f70d020205000410";
	break;
    case PGPHASHALGO_TIGER192:
	prefix = "3029300d06092b06010401da470c0205000418";
	break;
    case PGPHASHALGO_HAVAL_5_160:
	prefix = NULL;
	break;
    case PGPHASHALGO_SHA256:
	prefix = "3031300d060960864801650304020105000420";
	break;
    case PGPHASHALGO_SHA384:
	prefix = "3041300d060960864801650304020205000430";
	break;
    case PGPHASHALGO_SHA512:
	prefix = "3051300d060960864801650304020305000440";
	break;
    default:
	prefix = NULL;
	break;
    }
    if (prefix == NULL)
	return 1;

    xx = rpmDigestFinal(ctx, (void **)&dig->md5, &dig->md5len, 1);
    hexstr = tt = xmalloc(2 * nb + 1);
    memset(tt, (int) 'f', (2 * nb));
    tt[0] = '0'; tt[1] = '0';
    tt[2] = '0'; tt[3] = '1';
    tt += (2 * nb) - strlen(prefix) - strlen(dig->md5) - 2;
    *tt++ = '0'; *tt++ = '0';
    tt = stpcpy(tt, prefix);
    tt = stpcpy(tt, dig->md5);

    /* Set RSA hash. */
/*@-moduncon -noeffectuncon @*/
    xx = gcry_mpi_scan(&c, GCRYMPI_FMT_HEX, hexstr, strlen(hexstr), NULL);
    rc = gcry_sexp_build(&gc->hash, NULL,
		"(data (flags pkcs1) (hash %s %m))",
		gcry_pk_algo_name((int)sigp->hash_algo), c);
    gcry_mpi_release(c);
if (_pgp_debug)
rpmgcDump("gc->hash", gc->hash);
/*@=moduncon =noeffectuncon @*/

    hexstr = _free(hexstr);

    /* Compare leading 16 bits of digest for quick check. */
    s = dig->md5;
/*@-type@*/
    signhash16[0] = (rpmuint8_t) (nibble(s[0]) << 4) | nibble(s[1]);
    signhash16[1] = (rpmuint8_t) (nibble(s[2]) << 4) | nibble(s[3]);
/*@=type@*/
    return memcmp(signhash16, sigp->signhash16, sizeof(sigp->signhash16));
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


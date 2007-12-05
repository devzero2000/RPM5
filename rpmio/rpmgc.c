/** \ingroup rpmpgp
 * \file rpmio/rpmgc.c
 */

#include "system.h"
#include <rpmio.h>
#define	_RPMGC_INTERNAL
#define	_RPMPGP_INTERNAL
#include <rpmgc.h>
#include "debug.h"

/*@access pgpDig @*/
/*@access pgpDigParams @*/

/*@-redecl@*/
/*@unchecked@*/
extern int _pgp_debug;

/*@unchecked@*/
extern int _pgp_print;
/*@=redecl@*/

static
int rpmgcSetRSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies ctx, dig @*/
{
    rpmgc gc = dig->impl;
    unsigned int nbits = 0;	/* WRONG */
    unsigned int nb = (nbits + 7) >> 3;
    const char * prefix;
    const char * hexstr;
    const char * s;
    uint8_t signhash16[2];
    char * tt;
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
/*@=moduncon =noeffectuncon @*/

    hexstr = _free(hexstr);

    /* Compare leading 16 bits of digest for quick check. */
    s = dig->md5;
/*@-type@*/
    signhash16[0] = (uint8_t) (nibble(s[0]) << 4) | nibble(s[1]);
    signhash16[1] = (uint8_t) (nibble(s[2]) << 4) | nibble(s[3]);
/*@=type@*/
    return memcmp(signhash16, sigp->signhash16, sizeof(signhash16));
}

static
int rpmgcVerifyRSA(pgpDig dig)
	/*@*/
{
    rpmgc gc = dig->impl;
    int rc;

    /* Verify RSA signature. */
/*@-moduncon@*/
/*@=moduncon@*/

    return rc;
}

static
int rpmgcSetDSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies ctx, dig @*/
{
    rpmgc gc = dig->impl;
    uint8_t signhash16[2];
    int xx;

    xx = rpmDigestFinal(ctx, (void **)&dig->sha1, &dig->sha1len, 1);

   /* Set DSA hash. */
/*@-moduncon -noeffectuncon @*/
/*@=moduncon =noeffectuncon @*/

    /* Compare leading 16 bits of digest for quick check. */
    return memcmp(signhash16, sigp->signhash16, sizeof(signhash16));
}

static
int rpmgcVerifyDSA(pgpDig dig)
	/*@*/
{
    rpmgc gc = dig->impl;
    int rc;

    /* Verify DSA signature. */
/*@-moduncon@*/
/*@=moduncon@*/

    return rc;
}

static
int rpmgcMpiItem(const char * pre, pgpDig dig, int itemno,
		const uint8_t * p, /*@null@*/ const uint8_t * pend)
	/*@globals fileSystem @*/
	/*@modifies dig, fileSystem @*/
{
    rpmgc gc = dig->impl;
    int rc = 0;

    switch (itemno) {
    default:
assert(0);
	break;
    case 10:		/* RSA m**d */
	break;
    case 20:		/* DSA r */
	break;
    case 21:		/* DSA s */
	break;
    case 30:		/* RSA n */
	break;
    case 31:		/* RSA e */
	break;
    case 40:		/* DSA p */
	break;
    case 41:		/* DSA q */
	break;
    case 42:		/* DSA g */
	break;
    case 43:		/* DSA y */
	break;
    }
    return rc;
}

static
void rpmgcClean(void * impl)
	/*@modifies impl @*/
{
    rpmgc gc = impl;
    if (gc != NULL) {
    }
}

static
void * rpmgcFree(/*@only@*/ void * impl)
	/*@modifies impl @*/
{
    rpmgc gc = impl;
    if (gc != NULL) {
	gc = _free(gc);
    }
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

/** \ingroup rpmpgp
 * \file rpmio/rpmnss.c
 */

#include "system.h"
#include <rpmio.h>
#define	_RPMNSS_INTERNAL
#define	_RPMPGP_INTERNAL
#include <rpmnss.h>
#include "debug.h"

/*@access pgpDig @*/
/*@access pgpDigParams @*/

/*@unchecked@*/
extern int _pgp_debug;

/*@unchecked@*/
extern int _pgp_print;

static
int rpmnssSetRSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies ctx, dig @*/
{
    rpmnss nss = dig->impl;
    int xx;

    switch (sigp->hash_algo) {
    case PGPHASHALGO_MD5:
	nss->sigalg = SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION;
	break;
    case PGPHASHALGO_SHA1:
	nss->sigalg = SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION;
	break;
    case PGPHASHALGO_RIPEMD160:
	nss->sigalg = SEC_OID_UNKNOWN;
	break;
    case PGPHASHALGO_MD2:
	nss->sigalg = SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION;
	break;
    case PGPHASHALGO_TIGER192:
	nss->sigalg = SEC_OID_UNKNOWN;
	break;
    case PGPHASHALGO_HAVAL_5_160:
	nss->sigalg = SEC_OID_UNKNOWN;
	break;
    case PGPHASHALGO_SHA256:
	nss->sigalg = SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION;
	break;
    case PGPHASHALGO_SHA384:
	nss->sigalg = SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION;
	break;
    case PGPHASHALGO_SHA512:
	nss->sigalg = SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION;
	break;
    default:
	nss->sigalg = SEC_OID_UNKNOWN;
	break;
    }
    if (nss->sigalg == SEC_OID_UNKNOWN)
	return 1;

    xx = rpmDigestFinal(ctx, (void **)&dig->md5, &dig->md5len, 0);

    /* Compare leading 16 bits of digest for quick check. */
    return memcmp(dig->md5, sigp->signhash16, sizeof(sigp->signhash16));
}

static
int rpmnssVerifyRSA(pgpDig dig)
	/*@*/
{
    rpmnss nss = dig->impl;
    int rc;

    nss->digest.type = siBuffer;
    nss->digest.data = dig->md5;
    nss->digest.len = dig->md5len;

/*@-moduncon@*/
    rc = (VFY_VerifyDigest(&nss->digest, nss->rsa, nss->rsasig, nss->sigalg, NULL) == SECSuccess);
/*@=moduncon@*/

    return rc;
}

static
int rpmnssSetDSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies ctx, dig @*/
{
    rpmnss nss = dig->impl;
    int xx;

    nss->sigalg = SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST;

    xx = rpmDigestFinal(ctx, (void **)&dig->sha1, &dig->sha1len, 1);

    /* Compare leading 16 bits of digest for quick check. */
    return memcmp(dig->sha1, sigp->signhash16, sizeof(sigp->signhash16));
}

static
int rpmnssVerifyDSA(pgpDig dig)
	/*@*/
{
    rpmnss nss = dig->impl;
    SECItem digest;
    int rc;

    nss->digest.type = siBuffer;
    nss->digest.data = dig->sha1;
    nss->digest.len = dig->sha1len;

/*@-moduncon@*/
    rc = (VFY_VerifyDigest(&nss->digest, nss->dsa, nss->dsasig, nss->sigalg, NULL) == SECSuccess);
/*@=moduncon@*/

    return rc;
}

static
int rpmnssMpiItem(const char * pre, pgpDig dig, int itemno,
		const byte * p, /*@null@*/ const byte * pend)
	/*@globals fileSystem @*/
	/*@modifies dig, fileSystem @*/
{
    int rc = 0;

    switch (itemno) {
    default:
assert(0);
	break;
    case 10:
	break;
    case 20:
	break;
    case 21:
	break;
    case 30:
	break;
    case 31:
	break;
    case 40:
	break;
    case 41:
	break;
    case 42:
	break;
    case 43:
	break;
    }
    return rc;
}

static
void rpmnssClean(void * impl)
	/*@modifies impl @*/
{
    rpmnss nss = impl;
    if (nss != NULL) {
    }
}

static
void * rpmnssFree(/*@only@*/ void * impl)
	/*@modifies impl @*/
{
    rpmnss nss = impl;
    if (nss != NULL) {
	nss = _free(nss);
    }
    return NULL;
}

static
void * rpmnssInit(void)
	/*@*/
{
    rpmnss nss = xcalloc(1, sizeof(*nss));
    return (void *) nss;
}

struct pgpImplVecs_s rpmnssImplVecs = {
	rpmnssSetRSA, rpmnssVerifyRSA,
	rpmnssSetDSA, rpmnssVerifyDSA,
	rpmnssMpiItem, rpmnssClean,
	rpmnssFree, rpmnssInit
};

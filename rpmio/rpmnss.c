/** \ingroup rpmpgp
 * \file rpmio/rpmnss.c
 */

#include "system.h"
#include <rpmio.h>

#if defined(__LCLINT__)
#define	__i386__
#endif

#define	_RPMPGP_INTERNAL
#if defined(WITH_NSS)
#define	_RPMNSS_INTERNAL
#include <rpmnss.h>
#else
#include <rpmpgp.h>		/* XXX DIGEST_CTX */
#endif

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
int rpmnssSetRSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies ctx, dig @*/
{
#if defined(WITH_NSS)
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
#else
    return 1;
#endif	/* WITH_NSS */
}

static
int rpmnssVerifyRSA(pgpDig dig)
	/*@*/
{
#if defined(WITH_NSS)
    rpmnss nss = dig->impl;
    int rc;

    nss->item.type = siBuffer;
    nss->item.data = dig->md5;
    nss->item.len = (unsigned) dig->md5len;

/*@-moduncon -nullstate @*/
    rc = (VFY_VerifyDigest(&nss->item, nss->rsa, nss->rsasig, nss->sigalg, NULL) == SECSuccess);
/*@=moduncon =nullstate @*/

    return rc;
#else
    return 0;
#endif	/* WITH_NSS */
}

static
int rpmnssSetDSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies ctx, dig @*/
{
#if defined(WITH_NSS)
    rpmnss nss = dig->impl;
    int xx;

    nss->sigalg = SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST;

    xx = rpmDigestFinal(ctx, (void **)&dig->sha1, &dig->sha1len, 0);

    /* Compare leading 16 bits of digest for quick check. */
    return memcmp(dig->sha1, sigp->signhash16, sizeof(sigp->signhash16));
#else
    return 1;
#endif	/* WITH_NSS */
}

static
int rpmnssVerifyDSA(pgpDig dig)
	/*@*/
{
#if defined(WITH_NSS)
    rpmnss nss = dig->impl;
    int rc;

    nss->item.type = siBuffer;
    nss->item.data = dig->sha1;
    nss->item.len = (unsigned) dig->sha1len;

/*@-moduncon -nullstate @*/
    rc = (VFY_VerifyDigest(&nss->item, nss->dsa, nss->dsasig, nss->sigalg, NULL) == SECSuccess);
/*@=moduncon =nullstate @*/

    return rc;
#else
    return 0;
#endif	/* WITH_NSS */
}

#if defined(WITH_NSS)
/**
 * @return		0 on success
 */
static
int rpmnssMpiSet(const char * pre, int lbits,
		/*@out@*/ void * dest, const uint8_t * p,
		/*@null@*/ const uint8_t * pend)
	/*@globals fileSystem @*/
	/*@modifies *dest, fileSystem @*/
{
    unsigned int mbits = pgpMpiBits(p);
    unsigned int nbits;
    unsigned int nbytes;
    char * t = dest;
    unsigned int ix;

    if (pend != NULL && (p + ((mbits+7) >> 3)) > pend)
	return 1;

    if (mbits > lbits)
	return 1;

    nbits = (lbits > mbits ? lbits : mbits);
    nbytes = ((nbits + 7) >> 3);
    ix = ((nbits - mbits) >> 3);

if (_pgp_debug)
fprintf(stderr, "*** mbits %u nbits %u nbytes %u ix %u\n", mbits, nbits, nbytes, ix);
    if (ix > 0) memset(t, (int)'\0', ix);
    memcpy(t+ix, p+2, nbytes-ix);
if (_pgp_debug && _pgp_print)
fprintf(stderr, "\t %s %s", pre, pgpHexStr(dest, nbytes));
    return 0;
}

/**
 * @return		NULL on error
 */
static
/*@only@*/ /*@null@*/
SECItem * rpmnssMpiCopy(PRArenaPool * arena, /*@returned@*/ SECItem * item,
		const uint8_t * p)
	/*@modifies item @*/
{
    unsigned int nbytes = pgpMpiLen(p)-2;

    if (item == NULL) {
	if ((item = SECITEM_AllocItem(arena, item, nbytes)) == NULL)
	    return item;
    } else {
	if (arena != NULL)
	    item->data = PORT_ArenaGrow(arena, item->data, item->len, nbytes);
	else
	    item->data = PORT_Realloc(item->data, nbytes);
 	
	if (item->data == NULL) {
	    if (arena == NULL)
		SECITEM_FreeItem(item, PR_TRUE);
	    return NULL;
	}
    }

    memcpy(item->data, p+2, nbytes);
    item->len = nbytes;
/*@-temptrans@*/
    return item;
/*@=temptrans@*/
}

static
SECKEYPublicKey * rpmnssNewPublicKey(KeyType type)
	/*@*/
{
    PRArenaPool *arena;
    SECKEYPublicKey *key;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL)
	return NULL;

    key = PORT_ArenaZAlloc(arena, sizeof(*key));

    if (key == NULL) {
	PORT_FreeArena(arena, PR_FALSE);
	return NULL;
    }
    
    key->keyType = type;
    key->pkcs11ID = CK_INVALID_HANDLE;
    key->pkcs11Slot = NULL;
    key->arena = arena;
    return key;
}

static
SECKEYPublicKey * rpmnssNewRSAKey(void)
	/*@*/
{
    return rpmnssNewPublicKey(rsaKey);
}

static
SECKEYPublicKey * rpmnssNewDSAKey(void)
	/*@*/
{
    return rpmnssNewPublicKey(dsaKey);
}
#endif	/* WITH_NSS */

static
int rpmnssMpiItem(const char * pre, pgpDig dig, int itemno,
		const uint8_t * p, /*@null@*/ const uint8_t * pend)
	/*@globals fileSystem @*/
	/*@modifies dig, fileSystem @*/
{
#if defined(WITH_NSS)
    rpmnss nss = dig->impl;
    int rc = 0;

    switch (itemno) {
    default:
assert(0);
	break;
    case 10:		/* RSA m**d */
	nss->rsasig = rpmnssMpiCopy(NULL, nss->rsasig, p);
	if (nss->rsasig == NULL)
	    rc = 1;
	break;
    case 20:		/* DSA r */
	nss->item.type = 0;
	nss->item.len = 2 * (160/8);
	nss->item.data = xcalloc(1, nss->item.len);
	rc = rpmnssMpiSet(pre, 160, nss->item.data, p, pend);
	break;
    case 21:		/* DSA s */
	rc = rpmnssMpiSet(pre, 160, nss->item.data + (160/8), p, pend);
	if (nss->dsasig != NULL)
	    SECITEM_FreeItem(nss->dsasig, PR_FALSE);
	if ((nss->dsasig = SECITEM_AllocItem(NULL, NULL, 0)) == NULL
	 || DSAU_EncodeDerSig(nss->dsasig, &nss->item) != SECSuccess)
	    rc = 1;
	nss->item.data = _free(nss->item.data);
	break;
    case 30:		/* RSA n */
	if (nss->rsa == NULL)
	    nss->rsa = rpmnssNewRSAKey();
	if (nss->rsa == NULL)
	    rc = 1;
	else
	    (void) rpmnssMpiCopy(nss->rsa->arena, &nss->rsa->u.rsa.modulus, p);
	break;
    case 31:		/* RSA e */
	if (nss->rsa == NULL)
	    nss->rsa = rpmnssNewRSAKey();
	if (nss->rsa == NULL)
	    rc = 1;
	else
	    (void) rpmnssMpiCopy(nss->rsa->arena, &nss->rsa->u.rsa.publicExponent, p);
	break;
    case 40:		/* DSA p */
	if (nss->dsa == NULL)
	    nss->dsa = rpmnssNewDSAKey();
	if (nss->dsa == NULL)
	    rc = 1;
	else
	    (void) rpmnssMpiCopy(nss->dsa->arena, &nss->dsa->u.dsa.params.prime, p);
	break;
    case 41:		/* DSA q */
	if (nss->dsa == NULL)
	    nss->dsa = rpmnssNewDSAKey();
	if (nss->dsa == NULL)
	    rc = 1;
	else
	    (void) rpmnssMpiCopy(nss->dsa->arena, &nss->dsa->u.dsa.params.subPrime, p);
	break;
    case 42:		/* DSA g */
	if (nss->dsa == NULL)
	    nss->dsa = rpmnssNewDSAKey();
	if (nss->dsa == NULL)
	    rc = 1;
	else
	    (void) rpmnssMpiCopy(nss->dsa->arena, &nss->dsa->u.dsa.params.base, p);
	break;
    case 43:		/* DSA y */
	if (nss->dsa == NULL)
	    nss->dsa = rpmnssNewDSAKey();
	if (nss->dsa == NULL)
	    rc = 1;
	else
	    (void) rpmnssMpiCopy(nss->dsa->arena, &nss->dsa->u.dsa.publicValue, p);
	break;
    }
    return rc;
#else
    return 1;
#endif	/* WITH_NSS */
}

static
void rpmnssClean(void * impl)
	/*@modifies impl @*/
{
#if defined(WITH_NSS)
    rpmnss nss = impl;
    if (nss != NULL) {
	if (nss->dsa != NULL) {
	    SECKEY_DestroyPublicKey(nss->dsa);
	    nss->dsa = NULL;
	}
	if (nss->dsasig != NULL) {
	    SECITEM_ZfreeItem(nss->dsasig, PR_TRUE);
	    nss->dsasig = NULL;
	}
	if (nss->rsa != NULL) {
	    SECKEY_DestroyPublicKey(nss->rsa);
	    nss->rsa = NULL;
	}
	if (nss->rsasig != NULL) {
	    SECITEM_ZfreeItem(nss->rsasig, PR_TRUE);
	    nss->rsasig = NULL;
	}
    }
#endif	/* WITH_NSS */
}

static
void * rpmnssFree(/*@only@*/ void * impl)
	/*@modifies impl @*/
{
#if defined(WITH_NSS)
    rpmnss nss = impl;
    if (nss != NULL) {
	rpmnssClean(impl);
	nss = _free(nss);
    }
#endif	/* WITH_NSS */
    return NULL;
}

static
void * rpmnssInit(void)
	/*@*/
{
#if defined(WITH_NSS)
    rpmnss nss = xcalloc(1, sizeof(*nss));

    (void) NSS_NoDB_Init(NULL);

    return (void *) nss;
#else
    return NULL;
#endif	/* WITH_NSS */
}

struct pgpImplVecs_s rpmnssImplVecs = {
	rpmnssSetRSA, rpmnssVerifyRSA,
	rpmnssSetDSA, rpmnssVerifyDSA,
	rpmnssMpiItem, rpmnssClean,
	rpmnssFree, rpmnssInit
};

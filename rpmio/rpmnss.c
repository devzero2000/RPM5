/** \ingroup rpmpgp
 * \file rpmio/rpmnss.c
 */

#include "system.h"

#include <rpmiotypes.h>
#define	_RPMPGP_INTERNAL
#if defined(WITH_NSS)
#define	_RPMNSS_INTERNAL
#include <rpmnss.h>
#endif

#include "debug.h"

#if defined(WITH_NSS)

/*@access pgpDig @*/
/*@access pgpDigParams @*/

/*@-redecl@*/
/*@unchecked@*/
extern int _pgp_debug;

/*@unchecked@*/
extern int _pgp_print;
/*@=redecl@*/

/*@unchecked@*/
extern int _rpmnss_init;

static
int rpmnssSetRSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    rpmnss nss = dig->impl;
    int xx;

assert(sigp->hash_algo == rpmDigestAlgo(ctx));
    nss->sigalg = SEC_OID_UNKNOWN;
    switch (sigp->hash_algo) {
    case PGPHASHALGO_MD5:
	nss->sigalg = SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION;
	break;
    case PGPHASHALGO_SHA1:
	nss->sigalg = SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION;
	break;
    case PGPHASHALGO_RIPEMD160:
	break;
    case PGPHASHALGO_MD2:
	nss->sigalg = SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION;
	break;
    case PGPHASHALGO_MD4:
	nss->sigalg = SEC_OID_PKCS1_MD4_WITH_RSA_ENCRYPTION;
	break;
    case PGPHASHALGO_TIGER192:
	break;
    case PGPHASHALGO_HAVAL_5_160:
	break;
    case PGPHASHALGO_SHA256:
	nss->sigalg = SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION;
	break;
    case PGPHASHALGO_SHA384:
	nss->sigalg = SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION;
	break;
    case PGPHASHALGO_SHA512:
	nss->sigalg = SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION;
	break;
    case PGPHASHALGO_SHA224:
	break;
    default:
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

    nss->item.type = siBuffer;
    nss->item.data = dig->md5;
    nss->item.len = (unsigned) dig->md5len;

/*@-moduncon -nullstate @*/
    rc = (VFY_VerifyDigest(&nss->item, nss->rsa, nss->rsasig, nss->sigalg, NULL) == SECSuccess);
/*@=moduncon =nullstate @*/

    return rc;
}

static
int rpmnssSetDSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    rpmnss nss = dig->impl;
    int xx;

assert(sigp->hash_algo == rpmDigestAlgo(ctx));
    xx = rpmDigestFinal(ctx, (void **)&dig->sha1, &dig->sha1len, 0);

    nss->sigalg = SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST;

    /* Compare leading 16 bits of digest for quick check. */
    return memcmp(dig->sha1, sigp->signhash16, sizeof(sigp->signhash16));
}

static
int rpmnssVerifyDSA(pgpDig dig)
	/*@*/
{
    rpmnss nss = dig->impl;
    int rc;

    nss->item.type = siBuffer;
    nss->item.data = dig->sha1;
    nss->item.len = (unsigned) dig->sha1len;

/*@-moduncon -nullstate @*/
    rc = (VFY_VerifyDigest(&nss->item, nss->dsa, nss->dsasig, nss->sigalg, NULL) == SECSuccess);
/*@=moduncon =nullstate @*/

    return rc;
}

static
int rpmnssSetECDSA(/*@only@*/ DIGEST_CTX ctx, /*@unused@*/pgpDig dig, pgpDigParams sigp)
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
int rpmnssVerifyECDSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    int rc = 0;		/* XXX always fail. */

    return rc;
}

/**
 * @return		0 on success
 */
static
int rpmnssMpiSet(const char * pre, unsigned int lbits,
		/*@out@*/ void * dest, const rpmuint8_t * p,
		/*@null@*/ const rpmuint8_t * pend)
	/*@modifies *dest @*/
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

/*@-modfilesystem @*/
if (_pgp_debug)
fprintf(stderr, "*** mbits %u nbits %u nbytes %u ix %u\n", mbits, nbits, nbytes, ix);
    if (ix > 0) memset(t, (int)'\0', ix);
    memcpy(t+ix, p+2, nbytes-ix);
if (_pgp_debug && _pgp_print)
fprintf(stderr, "\t %s %s", pre, pgpHexStr(dest, nbytes));
/*@=modfilesystem @*/
    return 0;
}

/**
 * @return		NULL on error
 */
static
/*@only@*/ /*@null@*/
SECItem * rpmnssMpiCopy(PRArenaPool * arena, /*@returned@*/ SECItem * item,
		const rpmuint8_t * p)
	/*@modifies item @*/
{
    unsigned int nbytes = pgpMpiLen(p)-2;

/*@-moduncon@*/
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
/*@=moduncon@*/

    memcpy(item->data, p+2, nbytes);
    item->len = nbytes;
/*@-temptrans@*/
    return item;
/*@=temptrans@*/
}

static /*@null@*/
SECKEYPublicKey * rpmnssNewPublicKey(KeyType type)
	/*@*/
{
    PRArenaPool *arena;
    SECKEYPublicKey *key;

/*@-moduncon@*/
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL)
	return NULL;

    key = PORT_ArenaZAlloc(arena, sizeof(*key));

    if (key == NULL) {
	PORT_FreeArena(arena, PR_FALSE);
	return NULL;
    }
/*@=moduncon@*/
    
    key->keyType = type;
    key->pkcs11ID = CK_INVALID_HANDLE;
    key->pkcs11Slot = NULL;
    key->arena = arena;
/*@-nullret@*/	/* key->pkcs11Slot can be NULL */
    return key;
/*@=nullret@*/
}

static
int rpmnssMpiItem(const char * pre, pgpDig dig, int itemno,
		const rpmuint8_t * p, /*@null@*/ const rpmuint8_t * pend)
	/*@*/
{
    rpmnss nss = dig->impl;
    int rc = 0;

/*@-moduncon@*/
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
	    nss->rsa = rpmnssNewPublicKey(rsaKey);
	if (nss->rsa == NULL)
	    rc = 1;
	else
	    (void) rpmnssMpiCopy(nss->rsa->arena, &nss->rsa->u.rsa.modulus, p);
	break;
    case 31:		/* RSA e */
	if (nss->rsa == NULL)
	    nss->rsa = rpmnssNewPublicKey(rsaKey);
	if (nss->rsa == NULL)
	    rc = 1;
	else
	    (void) rpmnssMpiCopy(nss->rsa->arena, &nss->rsa->u.rsa.publicExponent, p);
	break;
    case 40:		/* DSA p */
	if (nss->dsa == NULL)
	    nss->dsa = rpmnssNewPublicKey(dsaKey);
	if (nss->dsa == NULL)
	    rc = 1;
	else
	    (void) rpmnssMpiCopy(nss->dsa->arena, &nss->dsa->u.dsa.params.prime, p);
	break;
    case 41:		/* DSA q */
	if (nss->dsa == NULL)
	    nss->dsa = rpmnssNewPublicKey(dsaKey);
	if (nss->dsa == NULL)
	    rc = 1;
	else
	    (void) rpmnssMpiCopy(nss->dsa->arena, &nss->dsa->u.dsa.params.subPrime, p);
	break;
    case 42:		/* DSA g */
	if (nss->dsa == NULL)
	    nss->dsa = rpmnssNewPublicKey(dsaKey);
	if (nss->dsa == NULL)
	    rc = 1;
	else
	    (void) rpmnssMpiCopy(nss->dsa->arena, &nss->dsa->u.dsa.params.base, p);
	break;
    case 43:		/* DSA y */
	if (nss->dsa == NULL)
	    nss->dsa = rpmnssNewPublicKey(dsaKey);
	if (nss->dsa == NULL)
	    rc = 1;
	else
	    (void) rpmnssMpiCopy(nss->dsa->arena, &nss->dsa->u.dsa.publicValue, p);
	break;
    }
/*@=moduncon@*/
    return rc;
}

/*@-mustmod@*/
static
void rpmnssClean(void * impl)
	/*@modifies impl @*/
{
    rpmnss nss = impl;
/*@-moduncon@*/
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
/*@=moduncon@*/
    }
}
/*@=mustmod@*/

static /*@null@*/
void * rpmnssFree(/*@only@*/ void * impl)
	/*@*/
{
    rpmnss nss = impl;
    if (nss != NULL) {
	rpmnssClean(impl);
	nss = _free(nss);
    }
    return NULL;
}

static
void * rpmnssInit(void)
	/*@globals _rpmnss_init @*/
	/*@modifies _rpmnss_init @*/
{
    rpmnss nss = xcalloc(1, sizeof(*nss));

/*@-moduncon@*/
    (void) NSS_NoDB_Init(NULL);
/*@=moduncon@*/
    _rpmnss_init = 1;

    return (void *) nss;
}

struct pgpImplVecs_s rpmnssImplVecs = {
	rpmnssSetRSA, rpmnssVerifyRSA,
	rpmnssSetDSA, rpmnssVerifyDSA,
	rpmnssSetECDSA, rpmnssVerifyECDSA,
	rpmnssMpiItem, rpmnssClean,
	rpmnssFree, rpmnssInit
};

#endif


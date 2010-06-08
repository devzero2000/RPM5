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

/*@unchecked@*/
static int _rpmnss_debug;

#define	SPEW(_t, _rc, _dig)	\
  { if ((_t) || _rpmnss_debug || _pgp_debug < 0) \
	fprintf(stderr, "<-- %s(%p) %s\t%s\n", __FUNCTION__, (_dig), \
		((_rc) ? "OK" : "BAD"), (_dig)->pubkey_algoN); \
  }

static const char * _pgpHashAlgo2Name(uint32_t algo)
{
    return pgpValStr(pgpHashTbl, (rpmuint8_t)algo);
}

static const char * _pgpPubkeyAlgo2Name(uint32_t algo)
{
    return pgpValStr(pgpPubkeyTbl, (rpmuint8_t)algo);
}

static const char * rpmnssStrerror(int err)
{
    static char buf[64];
#ifdef	NOTYET
    const char * errN = keyVN(rpmnssERRS, nrpmnssERRS, err);
#else
    const char * errN = NULL;
#endif
    if (errN == NULL) {
	snprintf(buf, sizeof(buf), "SEC_ERROR(%d)", err);
	errN = buf;
    }
    return errN;
}

static
int rpmnssErr(rpmnss nss, const char * msg, int rc)
        /*@*/
{
#ifdef	REFERENCE
    /* XXX Don't spew on expected failures ... */
    if (err && gcry_err_code(err) != gc->badok)
        fprintf (stderr, "rpmgc: %s(0x%0x): %s/%s\n",
                msg, (unsigned)err, gcry_strsource(err), gcry_strerror(err));
#endif
    if (rc != SECSuccess) {
	int err = PORT_GetError();
        fprintf (stderr, "rpmnss: %s rc(%d) err(%d) %s\n",
                msg, rc, err, rpmnssStrerror(err));
    }
    return rc;
}

static
int rpmnssSetRSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    rpmnss nss = dig->impl;
    int rc;
    int xx;
pgpDigParams pubp = pgpGetPubkey(dig);
dig->pubkey_algoN = _pgpPubkeyAlgo2Name(pubp->pubkey_algo);
dig->hash_algoN = _pgpHashAlgo2Name(sigp->hash_algo);

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

nss->digest = _free(nss->digest);
nss->digestlen = 0;
    xx = rpmDigestFinal(ctx, (void **)&nss->digest, &nss->digestlen, 0);

    /* Compare leading 16 bits of digest for quick check. */
    rc = memcmp(nss->digest, sigp->signhash16, sizeof(sigp->signhash16));
SPEW(rc, !rc, dig);
    return rc;
}

static
int rpmnssVerifyRSA(pgpDig dig)
	/*@*/
{
    rpmnss nss = dig->impl;
    int rc;

    nss->item.type = siBuffer;
    nss->item.data = nss->digest;
    nss->item.len = (unsigned) nss->digestlen;

    rc = rpmnssErr(nss, "VFY_VerifyDigest",
		VFY_VerifyDigest(&nss->item, nss->pub_key,
				nss->sig, nss->sigalg, NULL));
    rc = (rc == SECSuccess);

SPEW(!rc, rc, dig);
    return rc;
}

static
int rpmnssSetDSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    rpmnss nss = dig->impl;
    int rc;
    int xx;
pgpDigParams pubp = pgpGetPubkey(dig);
dig->pubkey_algoN = _pgpPubkeyAlgo2Name(pubp->pubkey_algo);
dig->hash_algoN = _pgpHashAlgo2Name(sigp->hash_algo);

assert(sigp->hash_algo == rpmDigestAlgo(ctx));
nss->digest = _free(nss->digest);
nss->digestlen = 0;
    xx = rpmDigestFinal(ctx, (void **)&nss->digest, &nss->digestlen, 0);

    nss->sigalg = SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST;

    /* Compare leading 16 bits of digest for quick check. */
    rc = memcmp(nss->digest, sigp->signhash16, sizeof(sigp->signhash16));
SPEW(rc, !rc, dig);
    return rc;
}

static int rpmnssSignRSA(pgpDig dig)
{
    rpmnss nss = dig->impl;
pgpDigParams sigp = pgpGetSignature(dig);
    int rc = 0;		/* assume failure. */

SECOidTag sigalg = SEC_OID_UNKNOWN;
    switch (sigp->hash_algo) {
    case PGPHASHALGO_MD5:
	sigalg = SEC_OID_MD5;
	break;
    case PGPHASHALGO_SHA1:
	sigalg = SEC_OID_SHA1;
	break;
    case PGPHASHALGO_RIPEMD160:
	break;
    case PGPHASHALGO_MD2:
	sigalg = SEC_OID_MD2;
	break;
    case PGPHASHALGO_MD4:
	sigalg = SEC_OID_MD4;
	break;
    case PGPHASHALGO_TIGER192:
	break;
    case PGPHASHALGO_HAVAL_5_160:
	break;
    case PGPHASHALGO_SHA256:
	sigalg = SEC_OID_SHA256;
	break;
    case PGPHASHALGO_SHA384:
	sigalg = SEC_OID_SHA384;
	break;
    case PGPHASHALGO_SHA512:
	sigalg = SEC_OID_SHA512;
	break;
    case PGPHASHALGO_SHA224:
	break;
    default:
	break;
    }
    if (sigalg == SEC_OID_UNKNOWN)
	goto exit;

    nss->item.type = siBuffer;
    nss->item.data = nss->digest;
    nss->item.len = (unsigned) nss->digestlen;

if (nss->sig != NULL) {
    SECITEM_ZfreeItem(nss->sig, PR_TRUE);
    nss->sig = NULL;
}
nss->sig = SECITEM_AllocItem(NULL, NULL, 0);
nss->sig->type = siBuffer;

    rc = rpmnssErr(nss, "SGN_Digest",
	    SGN_Digest(nss->sec_key, sigalg, nss->sig, &nss->item));
    rc = (rc == SECSuccess);

exit:
SPEW(!rc, rc, dig);
    return rc;
}

static int rpmnssGenerateRSA(pgpDig dig)
{
    rpmnss nss = dig->impl;
    int rc = 0;		/* assume failure */

if (nss->nbits == 0) nss->nbits = 1024; /* XXX FIXME */
assert(nss->nbits);

    {	CK_MECHANISM_TYPE _type = CKM_RSA_PKCS_KEY_PAIR_GEN;
	PK11SlotInfo * _slot = PK11_GetBestSlot(_type, NULL);
	int _isPerm = PR_FALSE;
	int _isSensitive = PR_FALSE;
	void * _cx = NULL;

	if (_slot) {
	    static unsigned _pe = 0x10001;	/* XXX FIXME: pass in e */
	    PK11RSAGenParams rsaparams =
		{ .keySizeInBits = nss->nbits, .pe = _pe };
	    void * params = &rsaparams;

	    nss->sec_key = PK11_GenerateKeyPair(_slot, _type, params,
			&nss->pub_key, _isPerm, _isSensitive, _cx);

	    PK11_FreeSlot(_slot);
	}
    }

    rc = (nss->sec_key && nss->pub_key);

SPEW(!rc, rc, dig);
    return rc;
}

static
int rpmnssVerifyDSA(pgpDig dig)
	/*@*/
{
    rpmnss nss = dig->impl;
    int rc;

    nss->item.type = siBuffer;
    nss->item.data = nss->digest;
    nss->item.len = (unsigned) nss->digestlen;

    rc = rpmnssErr(nss, "VFY_VerifyDigest",
		VFY_VerifyDigest(&nss->item, nss->pub_key,
				nss->sig, nss->sigalg, NULL));
    rc = (rc == SECSuccess);

SPEW(!rc, rc, dig);
    return rc;
}

static int rpmnssSignDSA(pgpDig dig)
{
    rpmnss nss = dig->impl;
pgpDigParams sigp = pgpGetSignature(dig);
    int rc = 0;		/* assume failure. */
SECItem sig = { .type = siBuffer, .len = 0, .data = NULL };

SECOidTag sigalg = SEC_OID_UNKNOWN;
    switch (sigp->hash_algo) {
    case PGPHASHALGO_MD5:
	break;
    case PGPHASHALGO_SHA1:
	sigalg = SEC_OID_SHA1;
	break;
    case PGPHASHALGO_RIPEMD160:
	break;
    case PGPHASHALGO_MD2:
	break;
    case PGPHASHALGO_MD4:
	break;
    case PGPHASHALGO_TIGER192:
	break;
    case PGPHASHALGO_HAVAL_5_160:
	break;
    case PGPHASHALGO_SHA256:
	break;
    case PGPHASHALGO_SHA384:
	break;
    case PGPHASHALGO_SHA512:
	break;
    case PGPHASHALGO_SHA224:
	break;
    default:
	break;
    }
    if (sigalg == SEC_OID_UNKNOWN)
	goto exit;

    nss->item.type = siBuffer;
    nss->item.data = nss->digest;
    nss->item.len = (unsigned) nss->digestlen;

if (nss->sig != NULL) {
    SECITEM_ZfreeItem(nss->sig, PR_TRUE);
    nss->sig = NULL;
}

nss->sig = SECITEM_AllocItem(NULL, NULL, 0);
nss->sig->type = siBuffer;

    rc = rpmnssErr(nss, "SGN_Digest",
	    SGN_Digest(nss->sec_key, sigalg, &sig, &nss->item));

    if (rc == SECSuccess)
	rc = rpmnssErr(nss, "DSAU_EncodeDerSig",
		DSAU_EncodeDerSig(nss->sig, &sig));

    sig.data = _free(sig.data);

    rc = (rc == SECSuccess);

exit:
SPEW(!rc, rc, dig);
    return rc;
}

static int rpmnssGenerateDSA(pgpDig dig)
{
    rpmnss nss = dig->impl;
    int rc = 0;		/* assume failure */

if (nss->nbits == 0) nss->nbits = 1024; /* XXX FIXME */
assert(nss->nbits);

    {	CK_MECHANISM_TYPE _type = CKM_DSA_KEY_PAIR_GEN;
	PK11SlotInfo * _slot = PK11_GetBestSlot(_type, NULL);
	int _isPerm = PR_FALSE;
	int _isSensitive = PR_FALSE;
	void * _cx = NULL;

	if (_slot) {

static const unsigned char P[] = { 0,
       0x98, 0xef, 0x3a, 0xae, 0x70, 0x98, 0x9b, 0x44,
       0xdb, 0x35, 0x86, 0xc1, 0xb6, 0xc2, 0x47, 0x7c,
       0xb4, 0xff, 0x99, 0xe8, 0xae, 0x44, 0xf2, 0xeb,
       0xc3, 0xbe, 0x23, 0x0f, 0x65, 0xd0, 0x4c, 0x04,
       0x82, 0x90, 0xa7, 0x9d, 0x4a, 0xc8, 0x93, 0x7f,
       0x41, 0xdf, 0xf8, 0x80, 0x6b, 0x0b, 0x68, 0x7f,
       0xaf, 0xe4, 0xa8, 0xb5, 0xb2, 0x99, 0xc3, 0x69,
       0xfb, 0x3f, 0xe7, 0x1b, 0xd0, 0x0f, 0xa9, 0x7a,
       0x4a, 0x04, 0xbf, 0x50, 0x9e, 0x22, 0x33, 0xb8,
       0x89, 0x53, 0x24, 0x10, 0xf9, 0x68, 0x77, 0xad,
       0xaf, 0x10, 0x68, 0xb8, 0xd3, 0x68, 0x5d, 0xa3,
       0xc3, 0xeb, 0x72, 0x3b, 0xa0, 0x0b, 0x73, 0x65,
       0xc5, 0xd1, 0xfa, 0x8c, 0xc0, 0x7d, 0xaa, 0x52,
       0x29, 0x34, 0x44, 0x01, 0xbf, 0x12, 0x25, 0xfe,
       0x18, 0x0a, 0xc8, 0x3f, 0xc1, 0x60, 0x48, 0xdb,
       0xad, 0x93, 0xb6, 0x61, 0x67, 0xd7, 0xa8, 0x2d };
static const unsigned char Q[] = { 0,
       0xb5, 0xb0, 0x84, 0x8b, 0x44, 0x29, 0xf6, 0x33,
       0x59, 0xa1, 0x3c, 0xbe, 0xd2, 0x7f, 0x35, 0xa1,
       0x76, 0x27, 0x03, 0x81                         };
static const unsigned char G[] = {
       0x04, 0x0e, 0x83, 0x69, 0xf1, 0xcd, 0x7d, 0xe5,
       0x0c, 0x78, 0x93, 0xd6, 0x49, 0x6f, 0x00, 0x04,
       0x4e, 0x0e, 0x6c, 0x37, 0xaa, 0x38, 0x22, 0x47,
       0xd2, 0x58, 0xec, 0x83, 0x12, 0x95, 0xf9, 0x9c,
       0xf1, 0xf4, 0x27, 0xff, 0xd7, 0x99, 0x57, 0x35,
       0xc6, 0x64, 0x4c, 0xc0, 0x47, 0x12, 0x31, 0x50,
       0x82, 0x3c, 0x2a, 0x07, 0x03, 0x01, 0xef, 0x30,
       0x09, 0x89, 0x82, 0x41, 0x76, 0x71, 0xda, 0x9e,
       0x57, 0x8b, 0x76, 0x38, 0x37, 0x5f, 0xa5, 0xcd,
       0x32, 0x84, 0x45, 0x8d, 0x4c, 0x17, 0x54, 0x2b,
       0x5d, 0xc2, 0x6b, 0xba, 0x3e, 0xa0, 0x7b, 0x95,
       0xd7, 0x00, 0x42, 0xf7, 0x08, 0xb8, 0x83, 0x87,
       0x60, 0xe1, 0xe5, 0xf4, 0x1a, 0x54, 0xc2, 0x20,
       0xda, 0x38, 0x3a, 0xd1, 0xb6, 0x10, 0xf4, 0xcb,
       0x35, 0xda, 0x97, 0x92, 0x87, 0xd6, 0xa5, 0x37,
       0x62, 0xb4, 0x93, 0x4a, 0x15, 0x21, 0xa5, 0x10 };
static const SECKEYPQGParams default_pqg_params = {
    NULL,
    { 0, (unsigned char *)P, sizeof(P) },
    { 0, (unsigned char *)Q, sizeof(Q) },
    { 0, (unsigned char *)G, sizeof(G) }
};
void * params = (void *)&default_pqg_params;

	    nss->sec_key = PK11_GenerateKeyPair(_slot, _type, params,
			&nss->pub_key, _isPerm, _isSensitive, _cx);

	    PK11_FreeSlot(_slot);
	}
    }

    rc = (nss->sec_key && nss->pub_key);

SPEW(!rc, rc, dig);
    return rc;
}

static
int rpmnssSetELG(/*@only@*/ DIGEST_CTX ctx, /*@unused@*/pgpDig dig, pgpDigParams sigp)
	/*@*/
{
    rpmnss nss = dig->impl;
    int rc = 1;		/* XXX always fail. */
    int xx;

assert(sigp->hash_algo == rpmDigestAlgo(ctx));
nss->digest = _free(nss->digest);
nss->digestlen = 0;
    xx = rpmDigestFinal(ctx, (void **)&nss->digest, &nss->digestlen, 0);

    /* Compare leading 16 bits of digest for quick check. */

    return rc;
}

static
int rpmnssSetECDSA(/*@only@*/ DIGEST_CTX ctx, /*@unused@*/pgpDig dig, pgpDigParams sigp)
	/*@*/
{
    rpmnss nss = dig->impl;
    int xx;

assert(sigp->hash_algo == rpmDigestAlgo(ctx));
    nss->sigalg = SEC_OID_UNKNOWN;
    switch (sigp->hash_algo) {
    case PGPHASHALGO_SHA1:
	nss->sigalg = SEC_OID_ANSIX962_ECDSA_SHA1_SIGNATURE;
	break;
    case PGPHASHALGO_SHA224:
	nss->sigalg = SEC_OID_ANSIX962_ECDSA_SHA224_SIGNATURE;
	break;
    case PGPHASHALGO_SHA256:
	nss->sigalg = SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE;
	break;
    case PGPHASHALGO_SHA384:
	nss->sigalg = SEC_OID_ANSIX962_ECDSA_SHA384_SIGNATURE;
	break;
    case PGPHASHALGO_SHA512:
	nss->sigalg = SEC_OID_ANSIX962_ECDSA_SHA512_SIGNATURE;
	break;
    default:
	break;
    }
    if (nss->sigalg == SEC_OID_UNKNOWN)
	return 1;

nss->digest = _free(nss->digest);
nss->digestlen = 0;
    xx = rpmDigestFinal(ctx, (void **)&nss->digest, &nss->digestlen, 0);

    /* Compare leading 16 bits of digest for quick check. */
    return memcmp(nss->digest, sigp->signhash16, sizeof(sigp->signhash16));
}

static
int rpmnssVerifyECDSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    rpmnss nss = dig->impl;
    int rc;

    nss->item.type = siBuffer;
    nss->item.data = nss->digest;
    nss->item.len = (unsigned) nss->digestlen;

/*@-moduncon -nullstate @*/
    rc = VFY_VerifyDigest(&nss->item, nss->pub_key, nss->sig, nss->sigalg, NULL);
/*@=moduncon =nullstate @*/
    return (rc == SECSuccess);
}

static int rpmnssErrChk(pgpDig dig, const char * msg, int rc, unsigned expected)
{
#ifdef	NOTYET
rpmgc gc = dig->impl;
    /* Was the return code the expected result? */
    rc = (gcry_err_code(gc->err) != expected);
    if (rc)
	fail("%s failed: %s\n", msg, gpg_strerror(gc->err));
/* XXX FIXME: rpmnssStrerror */
#else
    rc = (rc == 0);	/* XXX impedance match 1 -> 0 on success */
#endif
    return rc;	/* XXX 0 on success */
}

static int rpmnssAvailableCipher(pgpDig dig, int algo)
{
    int rc = 0;	/* assume available */
#ifdef	NOTYET
    rc = rpmgnssvailable(dig->impl, algo,
    	(gcry_md_test_algo(algo) || algo == PGPHASHALGO_MD5));
#endif
    return rc;
}

static int rpmnssAvailableDigest(pgpDig dig, int algo)
{
    int rc = 0;	/* assume available */
#ifdef	NOTYET
    rc = rpmgnssvailable(dig->impl, algo,
    	(gcry_md_test_algo(algo) || algo == PGPHASHALGO_MD5));
#endif
    return rc;
}

static int rpmnssAvailablePubkey(pgpDig dig, int algo)
{
    int rc = 0;	/* assume available */
#ifdef	NOTYET
    rc = rpmnssAvailable(dig->impl, algo, gcry_pk_test_algo(algo));
#endif
    return rc;
}

static int rpmnssVerify(pgpDig dig)
{
    int rc = 0;		/* assume failure */
pgpDigParams pubp = pgpGetPubkey(dig);
pgpDigParams sigp = pgpGetSignature(dig);
dig->pubkey_algoN = _pgpPubkeyAlgo2Name(pubp->pubkey_algo);
dig->hash_algoN = _pgpHashAlgo2Name(sigp->hash_algo);

    switch (pubp->pubkey_algo) {
    default:
	break;
    case PGPPUBKEYALGO_RSA:
	rc = rpmnssVerifyRSA(dig);
	break;
    case PGPPUBKEYALGO_DSA:
	rc = rpmnssVerifyDSA(dig);
	break;
    case PGPPUBKEYALGO_ELGAMAL:
#ifdef	NOTYET
	rc = rpmnssVerifyELG(dig);
#endif
	break;
    case PGPPUBKEYALGO_ECDSA:
	rc = rpmnssVerifyECDSA(dig);
	break;
    }
SPEW(!rc, rc, dig);
    return rc;
}

static int rpmnssSign(pgpDig dig)
{
    int rc = 0;		/* assume failure */
pgpDigParams pubp = pgpGetPubkey(dig);
dig->pubkey_algoN = _pgpPubkeyAlgo2Name(pubp->pubkey_algo);
    switch (pubp->pubkey_algo) {
    default:
	break;
    case PGPPUBKEYALGO_RSA:
	rc = rpmnssSignRSA(dig);
	break;
    case PGPPUBKEYALGO_DSA:
	rc = rpmnssSignDSA(dig);
	break;
    case PGPPUBKEYALGO_ELGAMAL:
#ifdef	NOTYET
	rc = rpmnssSignELG(dig);
#endif
	break;
    case PGPPUBKEYALGO_ECDSA:
#ifdef	NOTYET
	rc = rpmnssSignECDSA(dig);
#endif
	break;
    }
SPEW(!rc, rc, dig);
    return rc;
}

static int rpmnssGenerate(pgpDig dig)
{
    int rc = 0;		/* assume failure */
pgpDigParams pubp = pgpGetPubkey(dig);
dig->pubkey_algoN = _pgpPubkeyAlgo2Name(pubp->pubkey_algo);
    switch (pubp->pubkey_algo) {
    default:
	break;
    case PGPPUBKEYALGO_RSA:
	rc = rpmnssGenerateRSA(dig);
	break;
    case PGPPUBKEYALGO_DSA:
	rc = rpmnssGenerateDSA(dig);
	break;
    case PGPPUBKEYALGO_ELGAMAL:
#ifdef	NOTYET
	rc = rpmnssGenerateELG(dig);
#endif
	break;
    case PGPPUBKEYALGO_ECDSA:
#ifdef	NOTYET
	rc = rpmnssGenerateECDSA(dig);
#endif
	break;
    }
SPEW(!rc, rc, dig);
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
fprintf(stderr, "\t %s %s\n", pre, pgpHexStr(dest, nbytes));
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
    unsigned int hbits;
    size_t nb = (pend >= p ? (pend - p) : 0);
    int rc = 0;

/*@-moduncon@*/
    switch (itemno) {
    default:
assert(0);
	break;
    case 10:		/* RSA m**d */
	nss->sig = rpmnssMpiCopy(NULL, nss->sig, p);
	if (nss->sig == NULL)
	    rc = 1;
	break;
    case 20:		/* DSA r */
	hbits = 160;
	nss->item.type = 0;
	nss->item.len = 2 * (hbits/8);
	nss->item.data = xcalloc(1, nss->item.len);
	rc = rpmnssMpiSet(pre, hbits, nss->item.data, p, pend);
	break;
    case 21:		/* DSA s */
	hbits = 160;
	rc = rpmnssMpiSet(pre, hbits, nss->item.data + (hbits/8), p, pend);
	if (nss->sig != NULL)
	    SECITEM_FreeItem(nss->sig, PR_FALSE);
	if ((nss->sig = SECITEM_AllocItem(NULL, NULL, 0)) == NULL
	 || DSAU_EncodeDerSig(nss->sig, &nss->item) != SECSuccess)
	    rc = 1;
	nss->item.data = _free(nss->item.data);
	break;
    case 30:		/* RSA n */
	if (nss->pub_key == NULL)
	    nss->pub_key = rpmnssNewPublicKey(rsaKey);
	if (nss->pub_key == NULL)
	    rc = 1;
	else
	    (void) rpmnssMpiCopy(nss->pub_key->arena, &nss->pub_key->u.rsa.modulus, p);
	break;
    case 31:		/* RSA e */
	if (nss->pub_key == NULL)
	    nss->pub_key = rpmnssNewPublicKey(rsaKey);
	if (nss->pub_key == NULL)
	    rc = 1;
	else
	    (void) rpmnssMpiCopy(nss->pub_key->arena, &nss->pub_key->u.rsa.publicExponent, p);
	break;
    case 40:		/* DSA p */
	if (nss->pub_key == NULL)
	    nss->pub_key = rpmnssNewPublicKey(dsaKey);
	if (nss->pub_key == NULL)
	    rc = 1;
	else
	    (void) rpmnssMpiCopy(nss->pub_key->arena, &nss->pub_key->u.dsa.params.prime, p);
	break;
    case 41:		/* DSA q */
	if (nss->pub_key == NULL)
	    nss->pub_key = rpmnssNewPublicKey(dsaKey);
	if (nss->pub_key == NULL)
	    rc = 1;
	else
	    (void) rpmnssMpiCopy(nss->pub_key->arena, &nss->pub_key->u.dsa.params.subPrime, p);
	break;
    case 42:		/* DSA g */
	if (nss->pub_key == NULL)
	    nss->pub_key = rpmnssNewPublicKey(dsaKey);
	if (nss->pub_key == NULL)
	    rc = 1;
	else
	    (void) rpmnssMpiCopy(nss->pub_key->arena, &nss->pub_key->u.dsa.params.base, p);
	break;
    case 43:		/* DSA y */
	if (nss->pub_key == NULL)
	    nss->pub_key = rpmnssNewPublicKey(dsaKey);
	if (nss->pub_key == NULL)
	    rc = 1;
	else
	    (void) rpmnssMpiCopy(nss->pub_key->arena, &nss->pub_key->u.dsa.publicValue, p);
	break;
    case 50:		/* ECDSA r */
	hbits = 256;
	nss->item.type = 0;
	nss->item.len = 2 * (hbits/8);
	nss->item.data = xcalloc(1, nss->item.len);
	rc = rpmnssMpiSet(pre, hbits, nss->item.data, p, pend);
	break;
    case 51:		/* ECDSA s */
	hbits = 256;
	rc = rpmnssMpiSet(pre, hbits, nss->item.data + (hbits/8), p, pend);
	if (nss->sig != NULL)
	    SECITEM_FreeItem(nss->sig, PR_FALSE);
	if ((nss->sig = SECITEM_AllocItem(NULL, NULL, 0)) == NULL
	 || DSAU_EncodeDerSigWithLen(nss->sig, &nss->item, nss->item.len) != SECSuccess)
	    rc = 1;
	nss->item.data = _free(nss->item.data);
	break;
    case 60:		/* ECDSA curve OID */
assert(pend > p);
	if (nss->pub_key == NULL)
	    nss->pub_key = rpmnssNewPublicKey(ecKey);
	if (nss->pub_key == NULL)
	    rc = 1;
	else {
	    SECKEYECParams * ecp = &nss->pub_key->u.ec.DEREncodedParams;
	    ecp->data = PORT_ArenaZAlloc(nss->pub_key->arena, nb + 2);
	    ecp->data[0] = SEC_ASN1_OBJECT_ID;
	    ecp->data[1] = nb;
	    memcpy(ecp->data + 2, p, nb);
	    ecp->len = nb + 2;
	}
	break;
    case 61:		/* ECDSA Q */
assert(nss->pub_key);
	/* XXX assumes uncompressed Q as a MPI */
	nss->pub_key->u.ec.size = ((nb - (2 + 1)) * 8)/2;
	(void) rpmnssMpiCopy(nss->pub_key->arena, &nss->pub_key->u.ec.publicValue, p);
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
	nss->nbits = 0;
	nss->err = 0;
	nss->badok = 0;
	nss->digest = _free(nss->digest);
	nss->digestlen = 0;

	if (nss->sec_key != NULL) {
	    SECKEY_DestroyPrivateKey(nss->sec_key);
	    nss->sec_key = NULL;
	}
	if (nss->pub_key != NULL) {
	    SECKEY_DestroyPublicKey(nss->pub_key);
	    nss->pub_key = NULL;
	}
	if (nss->sig != NULL) {
	    SECITEM_ZfreeItem(nss->sig, PR_TRUE);
	    nss->sig = NULL;
	}

	if (nss->ecparams != NULL) {
	    SECITEM_FreeItem(nss->ecparams, PR_FALSE);
	    nss->ecparams = NULL;
	}
/*@=moduncon@*/
    }
}
/*@=mustmod@*/

static /*@null@*/
void * rpmnssFree(/*@only@*/ void * impl)
	/*@*/
{
    rpmnssClean(impl);
    impl = _free(impl);
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
	rpmnssSetRSA,
	rpmnssSetDSA,
	rpmnssSetELG,
	rpmnssSetECDSA,

	rpmnssErrChk,
	rpmnssAvailableCipher, rpmnssAvailableDigest, rpmnssAvailablePubkey,
	rpmnssVerify, rpmnssSign, rpmnssGenerate,

	rpmnssMpiItem, rpmnssClean,
	rpmnssFree, rpmnssInit
};

#endif


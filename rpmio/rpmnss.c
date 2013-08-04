/** \ingroup rpmpgp
 * \file rpmio/rpmnss.c
 */

#include "system.h"
#include <rpmio.h>

#include <rpmiotypes.h>
#define	_RPMPGP_INTERNAL
#if defined(WITH_NSS)
#define	_RPMNSS_INTERNAL
#include <rpmnss.h>
#endif
#include <rpmmacro.h>

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

/*==============================================================*/

typedef struct keyNV_s {
/*@observer@*/
    const char * N;		/* key name */
    uint32_t V;
} keyNV_t;

static int
keyNVCmp(const void * a, const void * b)
{
    return strcmp(((keyNV_t *)a)->N, ((keyNV_t *)b)->N);
}

static uint32_t
keyNV(keyNV_t * keys, size_t nkeys, /*@null@*/ const char *N)
{
    uint32_t V = 0;

    if (N && *N) {
	keyNV_t needle = { .N = N, .V = 0 };
	keyNV_t *k = (keyNV_t *)
		bsearch(&needle, keys, nkeys, sizeof(*keys), keyNVCmp);
	if (k)
	    V = k->V;
    }
    return V;
}

typedef struct keyVN_s {
    int V;
/*@observer@*/
    const char * N;		/* key name */
} keyVN_t;

static int
keyVNCmp(const void * a, const void * b)
{
    return (((keyVN_t *)a)->V - ((keyVN_t *)b)->V);
}

static const char *
keyVN(keyVN_t * keys, size_t nkeys, /*@null@*/ int V)
{
    const char * N = NULL;

    if (V) {
	/* XXX bsearch is overkill */
	keyVN_t needle = {};
	keyVN_t *k;

	needle.V = V;
	needle.N = NULL;
	k = (keyVN_t *)
		bsearch(&needle, keys, nkeys, sizeof(*keys), keyVNCmp);
	if (k)
	    N = k->N;
    }
    return N;
}

static const char * _pgpHashAlgo2Name(uint32_t algo)
{
    return pgpValStr(pgpHashTbl, (rpmuint8_t)algo);
}

static const char * _pgpPubkeyAlgo2Name(uint32_t algo)
{
    return pgpValStr(pgpPubkeyTbl, (rpmuint8_t)algo);
}

/*==============================================================*/

extern SECStatus
EC_DecodeParams(const SECItem *encodedParams, ECParams **ecparams);

static keyNV_t rpmnssOIDS[] = {
  { "c2onb191v4", SEC_OID_ANSIX962_EC_C2ONB191V4 },
  { "c2onb191v5", SEC_OID_ANSIX962_EC_C2ONB191V5 },
  { "c2onb239v4", SEC_OID_ANSIX962_EC_C2ONB239V4 },
  { "c2onb239v5", SEC_OID_ANSIX962_EC_C2ONB239V5 },
  { "c2pnb163v1", SEC_OID_ANSIX962_EC_C2PNB163V1 },
  { "c2pnb163v2", SEC_OID_ANSIX962_EC_C2PNB163V2 },
  { "c2pnb163v3", SEC_OID_ANSIX962_EC_C2PNB163V3 },
  { "c2pnb176v1", SEC_OID_ANSIX962_EC_C2PNB176V1 },
  { "c2pnb208w1", SEC_OID_ANSIX962_EC_C2PNB208W1 },
  { "c2pnb272w1", SEC_OID_ANSIX962_EC_C2PNB272W1 },
  { "c2pnb304w1", SEC_OID_ANSIX962_EC_C2PNB304W1 },
  { "c2pnb368w1", SEC_OID_ANSIX962_EC_C2PNB368W1 },
  { "c2tnb191v1", SEC_OID_ANSIX962_EC_C2TNB191V1 },
  { "c2tnb191v2", SEC_OID_ANSIX962_EC_C2TNB191V2 },
  { "c2tnb191v3", SEC_OID_ANSIX962_EC_C2TNB191V3 },
  { "c2tnb239v1", SEC_OID_ANSIX962_EC_C2TNB239V1 },
  { "c2tnb239v2", SEC_OID_ANSIX962_EC_C2TNB239V2 },
  { "c2tnb239v3", SEC_OID_ANSIX962_EC_C2TNB239V3 },
  { "c2tnb359v1", SEC_OID_ANSIX962_EC_C2TNB359V1 },
  { "c2tnb431r1", SEC_OID_ANSIX962_EC_C2TNB431R1 },
  { "nistb163", SEC_OID_SECG_EC_SECT163R2},
  { "nistb233", SEC_OID_SECG_EC_SECT233R1},
  { "nistb283", SEC_OID_SECG_EC_SECT283R1},
  { "nistb409", SEC_OID_SECG_EC_SECT409R1},
  { "nistb571", SEC_OID_SECG_EC_SECT571R1},
  { "nistk163", SEC_OID_SECG_EC_SECT163K1},
  { "nistk233", SEC_OID_SECG_EC_SECT233K1},
  { "nistk283", SEC_OID_SECG_EC_SECT283K1},
  { "nistk409", SEC_OID_SECG_EC_SECT409K1},
  { "nistk571", SEC_OID_SECG_EC_SECT571K1},
  { "nistp192", SEC_OID_SECG_EC_SECP192R1},
  { "nistp224", SEC_OID_SECG_EC_SECP224R1},
  { "nistp256", SEC_OID_SECG_EC_SECP256R1},
  { "nistp384", SEC_OID_SECG_EC_SECP384R1},
  { "nistp521", SEC_OID_SECG_EC_SECP521R1},
  { "prime192v1", SEC_OID_ANSIX962_EC_PRIME192V1 },
  { "prime192v2", SEC_OID_ANSIX962_EC_PRIME192V2 },
  { "prime192v3", SEC_OID_ANSIX962_EC_PRIME192V3 },
  { "prime239v1", SEC_OID_ANSIX962_EC_PRIME239V1 },
  { "prime239v2", SEC_OID_ANSIX962_EC_PRIME239V2 },
  { "prime239v3", SEC_OID_ANSIX962_EC_PRIME239V3 },
  { "secp112r1", SEC_OID_SECG_EC_SECP112R1},
  { "secp112r2", SEC_OID_SECG_EC_SECP112R2},
  { "secp128r1", SEC_OID_SECG_EC_SECP128R1},
  { "secp128r2", SEC_OID_SECG_EC_SECP128R2},
  { "secp160k1", SEC_OID_SECG_EC_SECP160K1},
  { "secp160r1", SEC_OID_SECG_EC_SECP160R1},
  { "secp160r2", SEC_OID_SECG_EC_SECP160R2},
  { "secp192k1", SEC_OID_SECG_EC_SECP192K1},
  { "secp192r1", SEC_OID_SECG_EC_SECP192R1},
  { "secp224k1", SEC_OID_SECG_EC_SECP224K1},
  { "secp224r1", SEC_OID_SECG_EC_SECP224R1},
  { "secp256k1", SEC_OID_SECG_EC_SECP256K1},
  { "secp256r1", SEC_OID_SECG_EC_SECP256R1},
  { "secp384r1", SEC_OID_SECG_EC_SECP384R1},
  { "secp521r1", SEC_OID_SECG_EC_SECP521R1},
  { "sect113r1", SEC_OID_SECG_EC_SECT113R1},
  { "sect113r2", SEC_OID_SECG_EC_SECT113R2},
  { "sect131r1", SEC_OID_SECG_EC_SECT131R1},
  { "sect131r2", SEC_OID_SECG_EC_SECT131R2},
  { "sect163k1", SEC_OID_SECG_EC_SECT163K1},
  { "sect163r1", SEC_OID_SECG_EC_SECT163R1},
  { "sect163r2", SEC_OID_SECG_EC_SECT163R2},
  { "sect193r1", SEC_OID_SECG_EC_SECT193R1},
  { "sect193r2", SEC_OID_SECG_EC_SECT193R2},
  { "sect233k1", SEC_OID_SECG_EC_SECT233K1},
  { "sect233r1", SEC_OID_SECG_EC_SECT233R1},
  { "sect239k1", SEC_OID_SECG_EC_SECT239K1},
  { "sect283k1", SEC_OID_SECG_EC_SECT283K1},
  { "sect283r1", SEC_OID_SECG_EC_SECT283R1},
  { "sect409k1", SEC_OID_SECG_EC_SECT409K1},
  { "sect409r1", SEC_OID_SECG_EC_SECT409R1},
  { "sect571k1", SEC_OID_SECG_EC_SECT571K1},
  { "sect571r1", SEC_OID_SECG_EC_SECT571R1},
};
static size_t nrpmnssOIDS = sizeof(rpmnssOIDS) / sizeof(rpmnssOIDS[0]);

#define _ENTRY(_v)      { SEC_ERROR_##_v, #_v }
/* XXX sorted table */
static keyVN_t rpmnssERRS[] = {
    _ENTRY(IO),
    _ENTRY(LIBRARY_FAILURE),
    _ENTRY(BAD_DATA),
    _ENTRY(OUTPUT_LEN),
    _ENTRY(INPUT_LEN),
    _ENTRY(INVALID_ARGS),
    _ENTRY(INVALID_ALGORITHM),
    _ENTRY(INVALID_AVA),
    _ENTRY(INVALID_TIME),
    _ENTRY(BAD_DER),
    _ENTRY(BAD_SIGNATURE),
    _ENTRY(EXPIRED_CERTIFICATE),
    _ENTRY(REVOKED_CERTIFICATE),
    _ENTRY(UNKNOWN_ISSUER),
    _ENTRY(BAD_KEY),
    _ENTRY(BAD_PASSWORD),
    _ENTRY(RETRY_PASSWORD),
    _ENTRY(NO_NODELOCK),
    _ENTRY(BAD_DATABASE),
    _ENTRY(NO_MEMORY),
    _ENTRY(UNTRUSTED_ISSUER),
    _ENTRY(UNTRUSTED_CERT),
    _ENTRY(DUPLICATE_CERT),
    _ENTRY(DUPLICATE_CERT_NAME),
    _ENTRY(ADDING_CERT),
    _ENTRY(FILING_KEY),
    _ENTRY(NO_KEY),
    _ENTRY(CERT_VALID),
    _ENTRY(CERT_NOT_VALID),
    _ENTRY(CERT_NO_RESPONSE),
    _ENTRY(EXPIRED_ISSUER_CERTIFICATE),
    _ENTRY(CRL_EXPIRED),
    _ENTRY(CRL_BAD_SIGNATURE),
    _ENTRY(CRL_INVALID),
    _ENTRY(EXTENSION_VALUE_INVALID),
    _ENTRY(EXTENSION_NOT_FOUND),
    _ENTRY(CA_CERT_INVALID),
    _ENTRY(PATH_LEN_CONSTRAINT_INVALID),
    _ENTRY(CERT_USAGES_INVALID),
/* SEC_INTERNAL_ONLY */
    _ENTRY(INVALID_KEY),
    _ENTRY(UNKNOWN_CRITICAL_EXTENSION),
    _ENTRY(OLD_CRL),
    _ENTRY(NO_EMAIL_CERT),
    _ENTRY(NO_RECIPIENT_CERTS_QUERY),
    _ENTRY(NOT_A_RECIPIENT),
    _ENTRY(PKCS7_KEYALG_MISMATCH),
    _ENTRY(PKCS7_BAD_SIGNATURE),
    _ENTRY(UNSUPPORTED_KEYALG),
    _ENTRY(DECRYPTION_DISALLOWED),
/* Fortezza Alerts */
/* XP_SEC_FORTEZZA_BAD_CARD */
/* XP_SEC_FORTEZZA_NO_CARD */
/* XP_SEC_FORTEZZA_NONE_SELECTED */
/* XP_SEC_FORTEZZA_MORE_INFO */
/* XP_SEC_FORTEZZA_PERSON_NOT_FOUND */
/* XP_SEC_FORTEZZA_NO_MORE_INFO */
/* XP_SEC_FORTEZZA_BAD_PIN */
/* XP_SEC_FORTEZZA_PERSON_ERROR */
    _ENTRY(NO_KRL),
    _ENTRY(KRL_EXPIRED),
    _ENTRY(KRL_BAD_SIGNATURE),
    _ENTRY(REVOKED_KEY),
    _ENTRY(KRL_INVALID),
    _ENTRY(NEED_RANDOM),
    _ENTRY(NO_MODULE),
    _ENTRY(NO_TOKEN),
    _ENTRY(READ_ONLY),
    _ENTRY(NO_SLOT_SELECTED),
    _ENTRY(CERT_NICKNAME_COLLISION),
    _ENTRY(KEY_NICKNAME_COLLISION),
    _ENTRY(SAFE_NOT_CREATED),
    _ENTRY(BAGGAGE_NOT_CREATED),
/* XP_JAVA_REMOVE_PRINCIPAL_ERROR */
/* XP_JAVA_DELETE_PRIVILEGE_ERROR */
/* XP_JAVA_CERT_NOT_EXISTS_ERROR */
    _ENTRY(BAD_EXPORT_ALGORITHM),
    _ENTRY(EXPORTING_CERTIFICATES),
    _ENTRY(IMPORTING_CERTIFICATES),
    _ENTRY(PKCS12_DECODING_PFX),
    _ENTRY(PKCS12_INVALID_MAC),
    _ENTRY(PKCS12_UNSUPPORTED_MAC_ALGORITHM),
    _ENTRY(PKCS12_UNSUPPORTED_TRANSPORT_MODE),
    _ENTRY(PKCS12_CORRUPT_PFX_STRUCTURE),
    _ENTRY(PKCS12_UNSUPPORTED_PBE_ALGORITHM),
    _ENTRY(PKCS12_UNSUPPORTED_VERSION),
    _ENTRY(PKCS12_PRIVACY_PASSWORD_INCORRECT),
    _ENTRY(PKCS12_CERT_COLLISION),
    _ENTRY(USER_CANCELLED),
    _ENTRY(PKCS12_DUPLICATE_DATA),
    _ENTRY(MESSAGE_SEND_ABORTED),
    _ENTRY(INADEQUATE_KEY_USAGE),
    _ENTRY(INADEQUATE_CERT_TYPE),
    _ENTRY(CERT_ADDR_MISMATCH),
    _ENTRY(PKCS12_UNABLE_TO_IMPORT_KEY),
    _ENTRY(PKCS12_IMPORTING_CERT_CHAIN),
    _ENTRY(PKCS12_UNABLE_TO_LOCATE_OBJECT_BY_NAME),
    _ENTRY(PKCS12_UNABLE_TO_EXPORT_KEY),
    _ENTRY(PKCS12_UNABLE_TO_WRITE),
    _ENTRY(PKCS12_UNABLE_TO_READ),
    _ENTRY(PKCS12_KEY_DATABASE_NOT_INITIALIZED),
    _ENTRY(KEYGEN_FAIL),
    _ENTRY(INVALID_PASSWORD),
    _ENTRY(RETRY_OLD_PASSWORD),
    _ENTRY(BAD_NICKNAME),
    _ENTRY(NOT_FORTEZZA_ISSUER),
    _ENTRY(CANNOT_MOVE_SENSITIVE_KEY),
    _ENTRY(JS_INVALID_MODULE_NAME),
    _ENTRY(JS_INVALID_DLL),
    _ENTRY(JS_ADD_MOD_FAILURE),
    _ENTRY(JS_DEL_MOD_FAILURE),
    _ENTRY(OLD_KRL),
    _ENTRY(CKL_CONFLICT),
    _ENTRY(CERT_NOT_IN_NAME_SPACE),
    _ENTRY(KRL_NOT_YET_VALID),
    _ENTRY(CRL_NOT_YET_VALID),
    _ENTRY(UNKNOWN_CERT),
    _ENTRY(UNKNOWN_SIGNER),
    _ENTRY(CERT_BAD_ACCESS_LOCATION),
    _ENTRY(OCSP_UNKNOWN_RESPONSE_TYPE),
    _ENTRY(OCSP_BAD_HTTP_RESPONSE),
    _ENTRY(OCSP_MALFORMED_REQUEST),
    _ENTRY(OCSP_SERVER_ERROR),
    _ENTRY(OCSP_TRY_SERVER_LATER),
    _ENTRY(OCSP_REQUEST_NEEDS_SIG),
    _ENTRY(OCSP_UNAUTHORIZED_REQUEST),
    _ENTRY(OCSP_UNKNOWN_RESPONSE_STATUS),
    _ENTRY(OCSP_UNKNOWN_CERT),
    _ENTRY(OCSP_NOT_ENABLED),
    _ENTRY(OCSP_NO_DEFAULT_RESPONDER),
    _ENTRY(OCSP_MALFORMED_RESPONSE),
    _ENTRY(OCSP_UNAUTHORIZED_RESPONSE),
    _ENTRY(OCSP_FUTURE_RESPONSE),
    _ENTRY(OCSP_OLD_RESPONSE),
/* smime stuff */
    _ENTRY(DIGEST_NOT_FOUND),
    _ENTRY(UNSUPPORTED_MESSAGE_TYPE),
    _ENTRY(MODULE_STUCK),
    _ENTRY(BAD_TEMPLATE),
    _ENTRY(CRL_NOT_FOUND),
    _ENTRY(REUSED_ISSUER_AND_SERIAL),
    _ENTRY(BUSY),
    _ENTRY(EXTRA_INPUT),
/* error codes used by elliptic curve code */
    _ENTRY(UNSUPPORTED_ELLIPTIC_CURVE),
    _ENTRY(UNSUPPORTED_EC_POINT_FORM),
    _ENTRY(UNRECOGNIZED_OID),
    _ENTRY(OCSP_INVALID_SIGNING_CERT),
/* new revocation errors */
    _ENTRY(REVOKED_CERTIFICATE_CRL),
    _ENTRY(REVOKED_CERTIFICATE_OCSP),
    _ENTRY(CRL_INVALID_VERSION),
    _ENTRY(CRL_V1_CRITICAL_EXTENSION),
    _ENTRY(CRL_UNKNOWN_CRITICAL_EXTENSION),
    _ENTRY(UNKNOWN_OBJECT_TYPE),
    _ENTRY(INCOMPATIBLE_PKCS11),
    _ENTRY(NO_EVENT),
    _ENTRY(CRL_ALREADY_EXISTS),
    _ENTRY(NOT_INITIALIZED),
    _ENTRY(TOKEN_NOT_LOGGED_IN),
    _ENTRY(OCSP_RESPONDER_CERT_INVALID),
    _ENTRY(OCSP_BAD_SIGNATURE),
    _ENTRY(OUT_OF_SEARCH_LIMITS),
    _ENTRY(INVALID_POLICY_MAPPING),
    _ENTRY(POLICY_VALIDATION_FAILED),
/* No longer used.  Unknown AIA location types are now silently ignored. */
    _ENTRY(UNKNOWN_AIA_LOCATION_TYPE),
    _ENTRY(BAD_HTTP_RESPONSE),
    _ENTRY(BAD_LDAP_RESPONSE),
    _ENTRY(FAILED_TO_ENCODE_DATA),
    _ENTRY(BAD_INFO_ACCESS_LOCATION),
    _ENTRY(LIBPKIX_INTERNAL),
    _ENTRY(PKCS11_GENERAL_ERROR),
    _ENTRY(PKCS11_FUNCTION_FAILED),
    _ENTRY(PKCS11_DEVICE_ERROR),
#if defined(SEC_ERROR_BAD_INFO_ACCESS_METHOD)
    _ENTRY(BAD_INFO_ACCESS_METHOD),
#endif
#if defined(SEC_ERROR_CRL_IMPORT_FAILED)
    _ENTRY(CRL_IMPORT_FAILED),
#endif
#if defined(SEC_ERROR_EXPIRED_PASSWORD)
    _ENTRY(EXPIRED_PASSWORD),
#endif
#if defined(SEC_ERROR_LOCKED_PASSWORD)
    _ENTRY(LOCKED_PASSWORD),
#endif
#if defined(SEC_ERROR_UNKNOWN_PKCS11_ERROR)
    _ENTRY(UNKNOWN_PKCS11_ERROR),
#endif
#if defined(SEC_ERROR_BAD_CRL_DP_URL)
    _ENTRY(BAD_CRL_DP_URL),
#endif
#if defined(SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED)
    _ENTRY(CERT_SIGNATURE_ALGORITHM_DISABLED),
#endif
#if defined(SEC_ERROR_LEGACY_DATABASE)
    _ENTRY(LEGACY_DATABASE),
#endif
#if defined(SEC_ERROR_APPLICATION_CALLBACK_ERROR)
    _ENTRY(APPLICATION_CALLBACK_ERROR),
#endif
};
static size_t nrpmnssERRS = sizeof(rpmnssERRS) / sizeof(rpmnssERRS[0]);
#undef _ENTRY

static uint32_t curve2oid(const char * name)
{
    uint32_t oid = keyNV(rpmnssOIDS, nrpmnssOIDS, name);
    if (oid == 0)
	oid = SEC_OID_UNKNOWN;

if (_pgp_debug)
fprintf(stderr, "<-- %s(%s) oid %u\n", __FUNCTION__, name, oid);

    return oid;
}

static const char * rpmnssStrerror(int err)
{
    static char buf[64];
    const char * errN = keyVN(rpmnssERRS, nrpmnssERRS, err);
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

/*==============================================================*/
static SECOidTag getEncAlg(unsigned pubkey_algo)
{
    SECOidTag encAlg = SEC_OID_UNKNOWN;

    switch (pubkey_algo) {
    case PGPPUBKEYALGO_RSA:	encAlg = SEC_OID_PKCS1_RSA_ENCRYPTION;	break;
    case PGPPUBKEYALGO_DSA:	encAlg = SEC_OID_ANSIX9_DSA_SIGNATURE;	break;
    case PGPPUBKEYALGO_ECDSA:	encAlg = SEC_OID_ANSIX962_EC_PUBLIC_KEY;break;
    case PGPPUBKEYALGO_ELGAMAL:	/*@fallthrough@*/
    default:
	break;
    }
    return encAlg;
}

static SECOidTag getHashAlg(unsigned hash_algo)
{
    SECOidTag hashAlg = SEC_OID_UNKNOWN;

    switch (hash_algo) {
    case PGPHASHALGO_MD2:	hashAlg = SEC_OID_MD2;		break;
    case PGPHASHALGO_MD4:	hashAlg = SEC_OID_MD4;		break;
    case PGPHASHALGO_MD5:	hashAlg = SEC_OID_MD5;		break;
    case PGPHASHALGO_SHA1:	hashAlg = SEC_OID_SHA1;		break;
    case PGPHASHALGO_SHA224:	hashAlg = SEC_OID_SHA224;	break;
    case PGPHASHALGO_SHA256:	hashAlg = SEC_OID_SHA256;	break;
    case PGPHASHALGO_SHA384:	hashAlg = SEC_OID_SHA384;	break;
    case PGPHASHALGO_SHA512:	hashAlg = SEC_OID_SHA512;	break;
    case PGPHASHALGO_RIPEMD160:	/*@fallthrough@*/
    case PGPHASHALGO_TIGER192:	/*@fallthrough@*/
    case PGPHASHALGO_HAVAL_5_160:	/*@fallthrough@*/
    default:
	break;
    }
    return hashAlg;
}

/*==============================================================*/

static
int rpmnssSetRSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    rpmnss nss = (rpmnss) dig->impl;
    int rc = 1;		/* assume error */
    int xx;
pgpDigParams pubp = pgpGetPubkey(dig);
dig->pubkey_algoN = _pgpPubkeyAlgo2Name(pubp->pubkey_algo);
dig->hash_algoN = _pgpHashAlgo2Name(sigp->hash_algo);

assert(sigp->hash_algo == rpmDigestAlgo(ctx));

nss->digest = _free(nss->digest);
nss->digestlen = 0;
    xx = rpmDigestFinal(ctx, (void **)&nss->digest, &nss->digestlen, 0);

    nss->encAlg = getEncAlg(pubp->pubkey_algo);
    nss->hashAlg = getHashAlg(sigp->hash_algo);
    if (nss->hashAlg == SEC_OID_UNKNOWN)
	goto exit;

    /* Compare leading 16 bits of digest for quick check. */
    rc = memcmp(nss->digest, sigp->signhash16, sizeof(sigp->signhash16));

exit:
SPEW(rc, !rc, dig);
    return rc;
}

static
int rpmnssVerifyRSA(pgpDig dig)
	/*@*/
{
    rpmnss nss = (rpmnss) dig->impl;
    int rc = 0;		/* assume failure */

nss->encAlg = SEC_OID_PKCS1_RSA_ENCRYPTION;	/* XXX FIXME */
assert(nss->encAlg == SEC_OID_PKCS1_RSA_ENCRYPTION);
assert(nss->hashAlg != SEC_OID_UNKNOWN);

    nss->item.type = siBuffer;
    nss->item.data = (unsigned char *) nss->digest;
    nss->item.len = (unsigned) nss->digestlen;

    rc = rpmnssErr(nss, "VFY_VerifyDigestDirect",
		VFY_VerifyDigestDirect(&nss->item, nss->pub_key,
				nss->sig, nss->encAlg, nss->hashAlg, NULL));
    rc = (rc == SECSuccess);

SPEW(!rc, rc, dig);
    return rc;
}

static int rpmnssSignRSA(pgpDig dig)
{
    rpmnss nss = (rpmnss) dig->impl;
    int rc = 0;		/* assume failure. */

assert(nss->hashAlg != SEC_OID_UNKNOWN);

    nss->item.type = siBuffer;
    nss->item.data = (unsigned char *) nss->digest;
    nss->item.len = (unsigned) nss->digestlen;

if (nss->sig != NULL) {
    SECITEM_ZfreeItem(nss->sig, PR_TRUE);
    nss->sig = NULL;
}
nss->sig = SECITEM_AllocItem(NULL, NULL, 0);
nss->sig->type = siBuffer;

    rc = rpmnssErr(nss, "SGN_Digest",
	    SGN_Digest(nss->sec_key, nss->hashAlg, nss->sig, &nss->item));
    rc = (rc == SECSuccess);

SPEW(!rc, rc, dig);
    return rc;
}

static int rpmnssGenerateRSA(pgpDig dig)
{
    rpmnss nss = (rpmnss) dig->impl;
    int rc = 0;		/* assume failure */

if (nss->nbits == 0) nss->nbits = 1024; /* XXX FIXME */
assert(nss->nbits);

    {	CK_MECHANISM_TYPE _type = CKM_RSA_PKCS_KEY_PAIR_GEN;
	PK11SlotInfo * _slot = PK11_GetBestSlot(_type, NULL);
	int _isPerm = PR_FALSE;
	int _isSensitive = PR_TRUE;
	void * _cx = NULL;

	if (_slot) {
	    static unsigned _pe = 0x10001;	/* XXX FIXME: pass in e */
	    PK11RSAGenParams rsaparams = {};
	    void * params = &rsaparams;

	    rsaparams.keySizeInBits = nss->nbits;
	    rsaparams.pe = _pe;

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
int rpmnssSetDSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    rpmnss nss = (rpmnss) dig->impl;
    int rc = 1;		/* assume error */
    int xx;
pgpDigParams pubp = pgpGetPubkey(dig);
dig->pubkey_algoN = _pgpPubkeyAlgo2Name(pubp->pubkey_algo);
dig->hash_algoN = _pgpHashAlgo2Name(sigp->hash_algo);

assert(sigp->hash_algo == rpmDigestAlgo(ctx));

nss->digest = _free(nss->digest);
nss->digestlen = 0;
    xx = rpmDigestFinal(ctx, (void **)&nss->digest, &nss->digestlen, 0);

    nss->encAlg = getEncAlg(pubp->pubkey_algo);
    nss->hashAlg = getHashAlg(sigp->hash_algo);
    if (nss->hashAlg == SEC_OID_UNKNOWN)
	goto exit;

    /* Compare leading 16 bits of digest for quick check. */
    rc = memcmp(nss->digest, sigp->signhash16, sizeof(sigp->signhash16));

exit:
SPEW(rc, !rc, dig);
    return rc;
}

static
int rpmnssVerifyDSA(pgpDig dig)
	/*@*/
{
    rpmnss nss = (rpmnss) dig->impl;
    int rc = 0;		/* assume failure */

nss->encAlg = SEC_OID_ANSIX9_DSA_SIGNATURE;	/* XXX FIXME */
assert(nss->encAlg == SEC_OID_ANSIX9_DSA_SIGNATURE);
assert(nss->hashAlg != SEC_OID_UNKNOWN);

    nss->item.type = siBuffer;
    nss->item.data = (unsigned char *) nss->digest;
    nss->item.len = (unsigned) nss->digestlen;

    rc = rpmnssErr(nss, "VFY_VerifyDigestDirect",
		VFY_VerifyDigestDirect(&nss->item, nss->pub_key,
				nss->sig, nss->encAlg, nss->hashAlg, NULL));
    rc = (rc == SECSuccess);

SPEW(!rc, rc, dig);
    return rc;
}

static int rpmnssSignDSA(pgpDig dig)
{
    rpmnss nss = (rpmnss) dig->impl;
    int rc = 0;		/* assume failure. */
SECItem sig = { siBuffer, NULL, 0 };

assert(nss->hashAlg != SEC_OID_UNKNOWN);
    switch (nss->hashAlg) {
    default:
	goto exit;
    case SEC_OID_SHA1:	/* XXX DSA2? */
	break;
    }

    nss->item.type = siBuffer;
    nss->item.data = (unsigned char *) nss->digest;
    nss->item.len = (unsigned) nss->digestlen;

if (nss->sig != NULL) {
    SECITEM_ZfreeItem(nss->sig, PR_TRUE);
    nss->sig = NULL;
}

nss->sig = SECITEM_AllocItem(NULL, NULL, 0);
nss->sig->type = siBuffer;

    rc = rpmnssErr(nss, "SGN_Digest",
	    SGN_Digest(nss->sec_key, nss->hashAlg, &sig, &nss->item));

    if (rc == SECSuccess)
	rc = rpmnssErr(nss, "DSAU_EncodeDerSigWithLen",
		DSAU_EncodeDerSigWithLen(nss->sig, &sig, sig.len));
    sig.data = _free(sig.data);

    rc = (rc == SECSuccess);

exit:
SPEW(!rc, rc, dig);
    return rc;
}

static int rpmnssGenerateDSA(pgpDig dig)
{
    rpmnss nss = (rpmnss) dig->impl;
    int rc = 0;		/* assume failure */

if (nss->nbits == 0) nss->nbits = 1024; /* XXX FIXME */
assert(nss->nbits);

    {	CK_MECHANISM_TYPE _type = CKM_DSA_KEY_PAIR_GEN;
	PK11SlotInfo * _slot = PK11_GetBestSlot(_type, NULL);
	int _isPerm = PR_FALSE;
	int _isSensitive = PR_TRUE;
	void * _cx = NULL;

	if (_slot) {
	    PQGParams *pqgParams = NULL;
	    PQGVerify *pqgVfy = NULL;
	    void * params = NULL;
int xx;

	    xx = rpmnssErr(nss, "PK11_PQG_ParamGen",
			PK11_PQG_ParamGen(0, &pqgParams, &pqgVfy));
	    if (xx != SECSuccess)
		goto exit;
	    params = pqgParams;

	    nss->sec_key = PK11_GenerateKeyPair(_slot, _type, params,
			&nss->pub_key, _isPerm, _isSensitive, _cx);

	    if (pqgVfy) PK11_PQG_DestroyVerify(pqgVfy);
	    if (pqgParams) PK11_PQG_DestroyParams(pqgParams);

	    PK11_FreeSlot(_slot);
	}
    }

    rc = (nss->sec_key && nss->pub_key);

exit:
SPEW(!rc, rc, dig);
    return rc;
}

static
int rpmnssSetELG(/*@only@*/ DIGEST_CTX ctx, /*@unused@*/pgpDig dig, pgpDigParams sigp)
	/*@*/
{
    rpmnss nss = (rpmnss) dig->impl;
    int rc = 1;		/* assume failure */
    int xx;

assert(sigp->hash_algo == rpmDigestAlgo(ctx));
nss->digest = _free(nss->digest);
nss->digestlen = 0;
    xx = rpmDigestFinal(ctx, (void **)&nss->digest, &nss->digestlen, 0);

    /* Compare leading 16 bits of digest for quick check. */
    rc = memcmp(nss->digest, sigp->signhash16, sizeof(sigp->signhash16));
    rc = 1;	/* XXX always fail */

SPEW(rc, !rc, dig);
    return rc;
}

static
int rpmnssSetECDSA(/*@only@*/ DIGEST_CTX ctx, /*@unused@*/pgpDig dig, pgpDigParams sigp)
	/*@*/
{
    rpmnss nss = (rpmnss) dig->impl;
    int rc = 1;		/* assume failure */
    int xx;
pgpDigParams pubp = pgpGetPubkey(dig);
dig->pubkey_algoN = _pgpPubkeyAlgo2Name(pubp->pubkey_algo);
dig->hash_algoN = _pgpHashAlgo2Name(sigp->hash_algo);

assert(sigp->hash_algo == rpmDigestAlgo(ctx));

nss->digest = _free(nss->digest);
nss->digestlen = 0;
    xx = rpmDigestFinal(ctx, (void **)&nss->digest, &nss->digestlen, 0);

    nss->encAlg = getEncAlg(pubp->pubkey_algo);
    nss->hashAlg = getHashAlg(sigp->hash_algo);
    if (nss->hashAlg == SEC_OID_UNKNOWN)
	goto exit;

    /* Compare leading 16 bits of digest for quick check. */
    rc = memcmp(nss->digest, sigp->signhash16, sizeof(sigp->signhash16));

exit:
SPEW(rc, !rc, dig);
    return rc;
}

static
int rpmnssVerifyECDSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    rpmnss nss = (rpmnss) dig->impl;
    int rc = 0;		/* assume failure */

nss->encAlg = SEC_OID_ANSIX962_EC_PUBLIC_KEY;	/* XXX FIXME */
assert(nss->encAlg == SEC_OID_ANSIX962_EC_PUBLIC_KEY);
assert(nss->hashAlg != SEC_OID_UNKNOWN);

    nss->item.type = siBuffer;
    nss->item.data = (unsigned char *) nss->digest;
    nss->item.len = (unsigned) nss->digestlen;

    rc = rpmnssErr(nss, "VFY_VerifyDigestDirect",
		VFY_VerifyDigestDirect(&nss->item, nss->pub_key,
				nss->sig, nss->encAlg, nss->hashAlg, NULL));
    rc = (rc == SECSuccess);

SPEW(!rc, rc, dig);
    return rc;
}

static
int rpmnssSignECDSA(pgpDig dig)
	/*@*/
{
    rpmnss nss = (rpmnss) dig->impl;
    int rc = 0;		/* assume failure. */
SECItem sig = { siBuffer, NULL, 0 };

assert(nss->hashAlg != SEC_OID_UNKNOWN);

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
	    SGN_Digest(nss->sec_key, nss->hashAlg, &sig, &nss->item));

    if (rc == SECSuccess)
	rc = rpmnssErr(nss, "DSAU_EncodeDerSigWithLen",
		DSAU_EncodeDerSigWithLen(nss->sig, &sig, sig.len));
    sig.data = _free(sig.data);

    rc = (rc == SECSuccess);

SPEW(!rc, rc, dig);
    return rc;
}

static int rpmnssLoadParams(pgpDig dig)
{
    rpmnss nss = (rpmnss) dig->impl;
    const char * name;
    SECOidTag curveOid = SEC_OID_UNKNOWN;
    SECOidData * oidData = NULL;
    int rc = 1;		/* assume failure. */
    
    name = nss->curveN;
    if (name == NULL)
	goto exit;
    nss->curveOid = curveOid = curve2oid(name);
    if (curveOid == SEC_OID_UNKNOWN)
	goto exit;
    oidData = SECOID_FindOIDByTag(curveOid);
    if (oidData == NULL)
	goto exit;

    nss->ecparams = SECITEM_AllocItem(NULL, NULL, (2 + oidData->oid.len));
    nss->ecparams->data[0] = SEC_ASN1_OBJECT_ID;
    nss->ecparams->data[1] = oidData->oid.len;
    memcpy(nss->ecparams->data + 2, oidData->oid.data, oidData->oid.len);
    rc = 0;

exit:
if (_pgp_debug)
fprintf(stderr, "<-- %s(%p,%s) oid %u params %p\n", __FUNCTION__, dig, name, nss->curveOid, nss->ecparams);
    return rc;
}

static
int rpmnssGenerateECDSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    rpmnss nss = (rpmnss) dig->impl;
    int rc = 0;		/* assume failure. */

if (nss->sec_key != NULL) {
    SECKEY_DestroyPrivateKey(nss->sec_key);
    nss->sec_key = NULL;
}
if (nss->pub_key != NULL) {
    SECKEY_DestroyPublicKey(nss->pub_key);
    nss->pub_key = NULL;
}

    if (nss->nbits == 0)	/* XXX FIXME */
	nss->nbits = 256;
assert(nss->nbits);

    if (nss->curveN == NULL)	/* XXX FIXME */
    switch (nss->nbits) {	/* XXX only NIST prime curves for now */
    default:	goto exit;	/*@notreached@*/ break;
    case 192:	nss->curveN = xstrdup("nistp192");	break;
    case 224:	nss->curveN = xstrdup("nistp224");	break;
    case 256:	nss->curveN = xstrdup("nistp256");	break;
    case 384:	nss->curveN = xstrdup("nistp384");	break;
    case 521:	nss->curveN = xstrdup("nistp521");	break;
    }
assert(nss->curveN);

    rc = rpmnssLoadParams(dig);
assert(nss->ecparams);

    {	CK_MECHANISM_TYPE _type = CKM_EC_KEY_PAIR_GEN;
	PK11SlotInfo * _slot = PK11_GetBestSlot(_type, NULL);
	int _isPerm = PR_FALSE;
	int _isSensitive = PR_FALSE;
	void * _cx = NULL;

	if (_slot) {

	    nss->sec_key = PK11_GenerateKeyPair(_slot, _type, nss->ecparams,
			&nss->pub_key, _isPerm, _isSensitive, _cx);

	    PK11_FreeSlot(_slot);
	}
    }

    rc = (nss->sec_key && nss->pub_key);

exit:
SPEW(!rc, rc, dig);

    return rc;
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
SPEW(0, rc, dig);
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
	rc = rpmnssSignECDSA(dig);
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
	rc = rpmnssGenerateECDSA(dig);
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
    char * t = (char *) dest;
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
fprintf(stderr, "\t %s %s\n", pre, pgpHexStr((rpmuint8_t *)dest, nbytes));
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
	    item->data = (unsigned char *) PORT_ArenaGrow(arena, item->data, item->len, nbytes);
	else
	    item->data = (unsigned char *) PORT_Realloc(item->data, nbytes);
 	
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

    key = (SECKEYPublicKey *) PORT_ArenaZAlloc(arena, sizeof(*key));

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
    rpmnss nss = (rpmnss) dig->impl;
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
	nss->item.type = (SECItemType) 0;
	nss->item.len = 2 * (hbits/8);
	nss->item.data = (unsigned char *) xcalloc(1, nss->item.len);
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
	nss->item.type = (SECItemType) 0;
	nss->item.len = 2 * (hbits/8);
	nss->item.data = (unsigned char *) xcalloc(1, nss->item.len);
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
	    ecp->data = (unsigned char *) PORT_ArenaZAlloc(nss->pub_key->arena, nb + 2);
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
    rpmnss nss = (rpmnss) impl;
/*@-moduncon@*/
    if (nss != NULL) {
	nss->nbits = 0;
	nss->qbits = 0;
	nss->badok = 0;
	nss->err = 0;

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

	nss->encAlg = SEC_OID_UNKNOWN;
	nss->hashAlg = SEC_OID_UNKNOWN;

	nss->item.type = siBuffer;
	nss->item.data = (unsigned char *) NULL;
	nss->item.len = (unsigned) 0;

	if (nss->ecparams != NULL) {
	    SECITEM_FreeItem(nss->ecparams, PR_FALSE);
	    nss->ecparams = NULL;
	}
	nss->curveN = _free(nss->curveN);
	nss->curveOid = SEC_OID_UNKNOWN;
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
    rpmnss nss = (rpmnss) xcalloc(1, sizeof(*nss));
    const char * _nssdb_path = rpmExpand("%{?_nssdb_path}", NULL);

/*@-moduncon@*/
    if (_nssdb_path != NULL && *_nssdb_path == '/')
	(void) NSS_Init(_nssdb_path);
    else
	(void) NSS_NoDB_Init(NULL);
/*@=moduncon@*/
    _nssdb_path = _free(_nssdb_path);

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

#endif	/* WITH_NSS */


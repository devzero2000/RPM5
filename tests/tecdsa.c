/* crypto/ecdsa/ecdsatest.c */
/*
 * Written by Nils Larsch for the OpenSSL project.
 */
/* ====================================================================
 * Copyright (c) 2000-2005 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * Portions of the attached software ("Contribution") are developed by 
 * SUN MICROSYSTEMS, INC., and are contributed to the OpenSSL project.
 *
 * The Contribution is licensed pursuant to the OpenSSL open source
 * license provided above.
 *
 * The elliptic curve binary polynomial software is originally written by 
 * Sheueling Chang Shantz and Douglas Stebila of Sun Microsystems Laboratories.
 *
 */

#include "system.h"

#include <rpmio.h>
#include <rpmlog.h>

#define	_RPMPGP_INTERNAL
#include <poptIO.h>

#define	_RPMBC_INTERNAL
#include <rpmbc.h>
#define	_RPMGC_INTERNAL
#include <rpmgc.h>
#define	_RPMNSS_INTERNAL
#include <rpmnss.h>

#ifdef	NOTNOW
#define	_RPMSSL_INTERNAL
#include <rpmssl.h>
#include <openssl/opensslconf.h>	/* XXX OPENSSL_NO_ECDSA */
#include <openssl/crypto.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/ecdsa.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#endif

#include "debug.h"

typedef struct key_s {
/*@observer@*/
    const char * name;		/* key name */
    uint32_t value;
} KEY;

static int
keyCmp(const void * a, const void * b)
{
    return strcmp(((KEY *)a)->name, ((KEY *)b)->name);
}

static uint32_t
keyValue(KEY * keys, size_t nkeys, /*@null@*/ const char *name)
{
    uint32_t keyval = 0;

    if (name && *name) {
	KEY needle = { .name = name, .value = 0 };
	KEY *k = (KEY *)bsearch(&needle, keys, nkeys, sizeof(*keys), keyCmp);
	if (k)
	    keyval = k->value;
    }
    return keyval;
}

/*==============================================================*/
static
int rpmgcSetECDSA(/*@only@*/ DIGEST_CTX ctx, /*@unused@*/pgpDig dig, pgpDigParams sigp)
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
int rpmgcVerifyECDSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    int rc = 0;		/* XXX always fail. */

    return rc;
}

static
int rpmgcSignECDSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    int rc = 0;		/* XXX always fail. */

    return rc;
}

static
int rpmgcGenerateECDSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    int rc = 0;		/* XXX always fail. */

    return rc;
}

/*==============================================================*/

static KEY rpmnssOIDS[] = {
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

static uint32_t curve2oid(const char * name)
{
    uint32_t oid = keyValue(rpmnssOIDS, nrpmnssOIDS, name);
    if (oid == 0)
	oid = SEC_OID_UNKNOWN;
exit:
fprintf(stderr, "<-- %s(%s) oid %u\n", __FUNCTION__, name, oid);
    return oid;
}

static void rpmnssLoadParams(pgpDig dig, const char * name)
{
    rpmnss nss = dig->impl;
    SECOidTag curveOidTag = curve2oid(name);
    SECOidData *oidData = SECOID_FindOIDByTag(curveOidTag);
    
    if (curveOidTag == SEC_OID_UNKNOWN || oidData == NULL) {
	nss->sigalg = curveOidTag;
	goto exit;
    }
    nss->sigalg = curveOidTag;

#ifdef	NOTYET
    nss->ecparams = SECITEM_AllocItem(NULL, NULL, (2 + oidData->oid.len));
    nss->ecparams->data[0] = SEC_ASN1_OBJECT_ID;
    nss->ecparams->data[1] = oidData->oid.len;
    memcpy(nss->ecparams->data + 2, oidData->oid.data, oidData->oid.len);
#endif

exit:
fprintf(stderr, "<-- %s(%p,%s) oid %u\n", __FUNCTION__, dig, name, nss->sigalg);
    return;
}

static
int rpmnssSetECDSA(/*@only@*/ DIGEST_CTX ctx, /*@unused@*/pgpDig dig, pgpDigParams sigp)
	/*@*/
{
    rpmnss nss = dig->impl;
    int xx;

assert(sigp->hash_algo == rpmDigestAlgo(ctx));
#ifdef	DYING
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
#endif
    if (nss->sigalg == SEC_OID_UNKNOWN)
	return 1;
assert(nss->sigalg != 0);

    xx = rpmDigestFinal(ctx, (void **)&dig->digest, &dig->digestlen, 0);

    /* Compare leading 16 bits of digest for quick check. */
    return memcmp(dig->digest, sigp->signhash16, sizeof(sigp->signhash16));
}

static
int rpmnssVerifyECDSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    int rc = 0;		/* assume failure */;

#ifdef	DYING
    rpmnss nss = dig->impl;
    nss->item.type = siBuffer;
    nss->item.data = dig->md5;
    nss->item.len = (unsigned) dig->md5len;

    rc = VFY_VerifyDigest(&nss->item, nss->ecdsa, nss->ecdsasig, nss->sigalg, NULL);
#endif

#ifdef	NOTYET
    rpmnss nss = dig->impl;
    ECPublicKey ecpub;
    SECItem digest =
	{ .type = siBuffer, .data = nss->digest, .len = nss->digestlen };
    unsigned char sig[2*MAX_ECKEY_LEN];
    SECItem signature =
	{ .type = siBuffer, .data = sig, .len = sizeof sig };

    if (ECDSA_VerifyDigest(&ecpub, &signature, &digest) == SECSuccess)
	rc = 1;
#endif


    return (rc == SECSuccess);
}

static
int rpmnssSignECDSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    int rc = 0;		/* XXX always fail. */

#ifdef	NOTYET
    rpmnss nss = dig->impl;
    SECItem digest =
	{ .type = siBuffer, .data = nss->digest, .len = nss->digestlen };
    unsigned char sig[2*MAX_ECKEY_LEN];
    SECItem signature =
	{ .type = siBuffer, .data = sig, .len = sizeof sig };

    if (ECDSA_SignDigest(nss->ecpriv, &signature, &digest) == SECSuccess)
	rc = 1;
#endif

    return rc;
}

static
int rpmnssGenerateECDSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    int rc = 0;		/* XXX always fail. */

#ifdef	NOTYET
    rpmnss nss = dig->impl;

    if (EC_NewKey(nss->ecparams, &nss->ecpriv) == SECSuccess
     && EC_ValidatePublicKey(nss->ecparams, &nss->ecpriv->publicValue) == SECSuccess)
	rc = 1;
#endif

    return rc;
}

/*==============================================================*/

#if defined(_RPMSSL_INTERNAL)
static KEY rpmsslNIDS[] = {
  { "c2pnb163v1", 684 },	/* X9.62 curve over a 163 bit binary field */
  { "c2pnb163v2", 685 },	/* X9.62 curve over a 163 bit binary field */
  { "c2pnb163v3", 686 },	/* X9.62 curve over a 163 bit binary field */
  { "c2pnb176v1", 687 },	/* X9.62 curve over a 176 bit binary field */
  { "c2pnb208w1", 693 },	/* X9.62 curve over a 208 bit binary field */
  { "c2pnb272w1", 699 },	/* X9.62 curve over a 272 bit binary field */
  { "c2pnb304w1", 700 },	/* X9.62 curve over a 304 bit binary field */
  { "c2pnb368w1", 702 },	/* X9.62 curve over a 368 bit binary field */
  { "c2tnb191v1", NID_X9_62_c2tnb191v1 },	/* X9.62 curve over a 191 bit binary field */
  { "c2tnb191v2", 689 },	/* X9.62 curve over a 191 bit binary field */
  { "c2tnb191v3", 690 },	/* X9.62 curve over a 191 bit binary field */
  { "c2tnb239v1", NID_X9_62_c2tnb239v1 },	/* X9.62 curve over a 239 bit binary field */
  { "c2tnb239v2", 695 },	/* X9.62 curve over a 239 bit binary field */
  { "c2tnb239v3", 696 },	/* X9.62 curve over a 239 bit binary field */
  { "c2tnb359v1", 701 },	/* X9.62 curve over a 359 bit binary field */
  { "c2tnb431r1", 703 },	/* X9.62 curve over a 431 bit binary field */
  { "prime192v1", NID_X9_62_prime192v1 }, /* NIST/X9.62/SECG curve over a 192 bit prime field */
  { "prime192v2", 410 },	/* X9.62 curve over a 192 bit prime field */
  { "prime192v3", 411 },	/* X9.62 curve over a 192 bit prime field */
  { "prime239v1", NID_X9_62_prime239v1 },	/* X9.62 curve over a 239 bit prime field */
  { "prime239v2", 413 },	/* X9.62 curve over a 239 bit prime field */
  { "prime239v3", 414 },	/* X9.62 curve over a 239 bit prime field */
  { "prime256v1", NID_X9_62_prime256v1 },	/* X9.62/SECG curve over a 256 bit prime field */
  { "secp112r1", 704 },	/* SECG/WTLS curve over a 112 bit prime field */
  { "secp112r2", 705 },	/* SECG curve over a 112 bit prime field */
  { "secp128r1", 706 },	/* SECG curve over a 128 bit prime field */
  { "secp128r2", 707 },	/* SECG curve over a 128 bit prime field */
  { "secp160k1", 708 },	/* SECG curve over a 160 bit prime field */
  { "secp160r1", 709 },	/* SECG curve over a 160 bit prime field */
  { "secp160r2", 710 },	/* SECG/WTLS curve over a 160 bit prime field */
  { "secp192k1", 711 },	/* SECG curve over a 192 bit prime field */
  { "secp224k1", 712 },	/* SECG curve over a 224 bit prime field */
  { "secp224r1", NID_secp224r1 },	/* NIST/SECG curve over a 224 bit prime field */
  { "secp256k1", 714 },	/* SECG curve over a 256 bit prime field */
  { "secp384r1", NID_secp384r1 },	/* NIST/SECG curve over a 384 bit prime field */
  { "secp521r1", NID_secp521r1 },	/* NIST/SECG curve over a 521 bit prime field */
  { "sect113r1", 717 },	/* SECG curve over a 113 bit binary field */
  { "sect113r2", 718 },	/* SECG curve over a 113 bit binary field */
  { "sect131r1", 719 },	/* SECG/WTLS curve over a 131 bit binary field */
  { "sect131r2", 720 },	/* SECG curve over a 131 bit binary field */
  { "sect163k1", 721 },	/* NIST/SECG/WTLS curve over a 163 bit binary field */
  { "sect163r1", 722 },	/* SECG curve over a 163 bit binary field */
  { "sect163r2", 723 },	/* NIST/SECG curve over a 163 bit binary field */
  { "sect193r1", 724 },	/* SECG curve over a 193 bit binary field */
  { "sect193r2", 725 },	/* SECG curve over a 193 bit binary field */
  { "sect233k1", 726 },	/* NIST/SECG/WTLS curve over a 233 bit binary field */
  { "sect233r1", 727 },	/* NIST/SECG/WTLS curve over a 233 bit binary field */
  { "sect239k1", 728 },	/* SECG curve over a 239 bit binary field */
  { "sect283k1", 729 },	/* NIST/SECG curve over a 283 bit binary field */
  { "sect283r1", 730 },	/* NIST/SECG curve over a 283 bit binary field */
  { "sect409k1", 731 },	/* NIST/SECG curve over a 409 bit binary field */
  { "sect409r1", 732 },	/* NIST/SECG curve over a 409 bit binary field */
  { "sect571k1", 733 },	/* NIST/SECG curve over a 571 bit binary field */
  { "sect571r1", 734 },	/* NIST/SECG curve over a 571 bit binary field */
  { "wap-wsg-idm-ecid-wtls10", 743 },	/* NIST/SECG/WTLS curve over a 233 bit binary field */
  { "wap-wsg-idm-ecid-wtls11", 744 },	/* NIST/SECG/WTLS curve over a 233 bit binary field */
  { "wap-wsg-idm-ecid-wtls12", 745 },	/* WTLS curvs over a 224 bit prime field */
  { "wap-wsg-idm-ecid-wtls1", 735 },	/* WTLS curve over a 113 bit binary field */
  { "wap-wsg-idm-ecid-wtls3", 736 },	/* NIST/SECG/WTLS curve over a 163 bit binary field */
  { "wap-wsg-idm-ecid-wtls4", 737 },	/* SECG curve over a 113 bit binary field */
  { "wap-wsg-idm-ecid-wtls5", 738 },	/* X9.62 curve over a 163 bit binary field */
  { "wap-wsg-idm-ecid-wtls6", 739 },	/* SECG/WTLS curve over a 112 bit prime field */
  { "wap-wsg-idm-ecid-wtls7", 740 },	/* SECG/WTLS curve over a 160 bit prime field */
  { "wap-wsg-idm-ecid-wtls8", 741 },	/* WTLS curve over a 112 bit prime field */
  { "wap-wsg-idm-ecid-wtls9", 742 },	/* WTLS curve over a 160 bit prime field */
};
static size_t nrpmsslNIDS = sizeof(rpmsslNIDS) / sizeof(rpmsslNIDS[0]);

static int curve2nid(const char * name)
{
    int nid = keyValue(rpmsslNIDS, nrpmsslNIDS, name);
fprintf(stderr, "<-- %s(%s) nid %u\n", __FUNCTION__, name, nid);
    return nid;
}

static int _rpmssl_spew;

static int rpmsslLoadBN(BIGNUM ** bnp, const char * bnstr, int spew)
{
    int rc;

    *bnp = NULL;
    if (bnstr[0] == '0' && bnstr[1] == 'x')
	rc = BN_hex2bn(bnp, bnstr + 2);
    else
	rc = BN_dec2bn(bnp, bnstr);

    if (rc && (spew || _rpmssl_spew)) {
	char * t;
	rpmlog(RPMLOG_DEBUG, "\t%p: %s\n", *bnp, t=BN_bn2dec(*bnp));
	OPENSSL_free(t);
	rpmlog(RPMLOG_DEBUG, "\t%p: 0x%s\n", *bnp, t=BN_bn2hex(*bnp));
	OPENSSL_free(t);
    }

    return rc;
}
#endif	/* _RPMSSL_INTERNAL */

/*==============================================================*/

static const char ** _numbers;
static int _ix = 0;
static int _numbers_max = 2;

#if defined(_RPMSSL_INTERNAL)
static const RAND_METHOD *old_rand;

static int fbytes(unsigned char *buf, int num)
{
    BIGNUM *tmp = NULL;
    int ret = 0;	/* assume failure */

    if (!(_numbers && _ix < _numbers_max && _numbers[_ix]))
	return 0;

    tmp = NULL;
    if (!rpmsslLoadBN(&tmp, _numbers[_ix], _rpmssl_spew))
	goto exit;

    _ix++;
    ret = BN_bn2bin(tmp, buf);
    if (ret == 0 || ret != num)
	ret = 0;
    else
	ret = 1;

exit:
    if (tmp)
	BN_free(tmp);
    return ret;
}
#endif	/* _RPMSSL_INTERNAL */

static int change_rand(void)
{
    int rc = 1;		/* assume success */
#if defined(_RPMSSL_INTERNAL)
    static RAND_METHOD fake_rand;

    /* save old rand method */
    if ((old_rand = RAND_get_rand_method()) == NULL) {
	rc = 0;
	goto exit;
    }

    fake_rand.seed = old_rand->seed;
    fake_rand.cleanup = old_rand->cleanup;
    fake_rand.add = old_rand->add;
    fake_rand.status = old_rand->status;
    /* use own random function */
    fake_rand.bytes = fbytes;
    fake_rand.pseudorand = old_rand->bytes;

    rc = (RAND_set_rand_method(&fake_rand) ? 1 : 0);
exit:
#endif	/* _RPMSSL_INTERNAL */
    return rc;
}

static int restore_rand(void)
{
    int rc = 1;		/* assume success */
#if defined(_RPMSSL_INTERNAL)
    rc = (RAND_set_rand_method(old_rand) ? 1 : 0);
#endif	/* _RPMSSL_INTERNAL */
    return rc;
}

/*==============================================================*/

struct ECDSAvec_s {
    const char * name;
    const char * msg;
    int dalgo;
    const char * d;
    const char * k;
    const char * r;
    const char * s;
} ECDSAvecs[] = {
/* ----- X9.66-1998 J.3.1 */
  { "prime192v1", "abc", PGPHASHALGO_SHA1,
    "0x1A8D598FC15BF0FD89030B5CB1111AEB92AE8BAF5EA475FB",
    "0xFA6DE29746BBEB7F8BB1E761F85F7DFB2983169D82FA2F4E",
    "0x885052380FF147B734C330C43D39B2C4A89F29B0F749FEAD",
    "0xE9ECC78106DEF82BF1070CF1D4D804C3CB390046951DF686"
  },
  { "prime239v1", "abc", PGPHASHALGO_SHA1,
    "0x7EF7C6FABEFFFDEA864206E80B0B08A9331ED93E698561B64CA0F7777F3D",
    "0x656C7196BF87DCC5D1F1020906DF2782360D36B2DE7A17ECE37D503784AF",
    "0x2CB7F36803EBB9C427C58D8265F11FC5084747133078FC279DE874FBECB0",
    "0x2EEAE988104E9C2234A3C2BEB1F53BFA5DC11FF36A875D1E3CCB1F7E45CF"
  },
  { "c2tnb191v1", "abc", PGPHASHALGO_SHA1,
    "0x340562E1DDA332F9D2AEC168249B5696EE39D0ED4D03760F",
    "0x3EEACE72B4919D991738D521879F787CB590AFF8189D2B69",
    "0x038E5A11FB55E4C65471DCD4998452B1E02D8AF7099BB930",
    "0x0C9A08C34468C244B4E5D6B21B3C68362807416020328B6E"
  },
  { "c2tnb239v1", "abc", PGPHASHALGO_SHA1,
    "0x151A30A6D843DB3B25063C5108255CC4448EC0F4D426D4EC884502229C96",
    "0x18D114BDF47E2913463E50375DC92784A14934A124F83D28CAF97C5D8AAB",
    "0x03210D71EF6C10157C0D1053DFF93E8B085F1E9BC22401F7A24798A63C00",
    "0x1C8C4343A8ECBF7C4D4E48F7D76D5658BC027C77086EC8B10097DEB307D6"
  },
/* --- P-192 FIPS 186-3 */
  { "prime192v1", "Example of ECDSA with P-192", PGPHASHALGO_SHA1,
    "0x7891686032fd8057f636b44b1f47cce564d2509923a7465b",
    "0xd06cb0a0ef2f708b0744f08aa06b6deedea9c0f80a69d847",
    "0xf0ecba72b88cde399cc5a18e2a8b7da54d81d04fb9802821",
    "0x1e6d3d4ae2b1fab2bd2040f5dabf00f854fa140b6d21e8ed"
  },
/* --- P-224 */
  { "secp224r1", "Example of ECDSA with P-224", PGPHASHALGO_SHA224,
    "0x3f0c488e987c80be0fee521f8d90be6034ec69ae11ca72aa777481e8",
    "0xa548803b79df17c40cde3ff0e36d025143bcbba146ec32908eb84937",
    "0xc3a3f5b82712532004c6f6d1db672f55d931c3409ea1216d0be77380",
    "0xc5aa1eae6095dea34c9bd84da3852cca41a8bd9d5548f36dabdf6617"
  },
/* --- P-256 */
  { "prime256v1", "Example of ECDSA with P-256", PGPHASHALGO_SHA256,
    "0xc477f9f65c22cce20657faa5b2d1d8122336f851a508a1ed04e479c34985bf96",
    "0x7a1a7e52797fc8caaa435d2a4dace39158504bf204fbe19f14dbb427faee50ae",
    "0x2b42f576d07f4165ff65d1f3b1500f81e44c316f1f0b3ef57325b69aca46104f",
    "0xdc42c2122d6392cd3e3a993a89502a8198c1886fe69d262c4b329bdb6b63faf1"
  },
/* --- P-384 */
  { "secp384r1", "Example of ECDSA with P-384", PGPHASHALGO_SHA384,
    "0xf92c02ed629e4b48c0584b1c6ce3a3e3b4faae4afc6acb0455e73dfc392e6a0ae393a8565e6b9714d1224b57d83f8a08",
    "0x2e44ef1f8c0bea8394e3dda81ec6a7842a459b534701749e2ed95f054f0137680878e0749fc43f85edcae06cc2f43fef",
    "0x30ea514fc0d38d8208756f068113c7cada9f66a3b40ea3b313d040d9b57dd41a332795d02cc7d507fcef9faf01a27088",
    "0xcc808e504be414f46c9027bcbf78adf067a43922d6fcaa66c4476875fbb7b94efd1f7d5dbe620bfb821c46d549683ad8"
  },
#ifdef	NOTYET
/* --- P-521 */
  { "secp521r1", "Example of ECDSA with P-521", PGPHASHALGO_SHA512,
    "0x0100085f47b8e1b8b11b7eb33028c0b2888e304bfc98501955b45bba1478dc184eeedf09b86a5f7c21994406072787205e69a63709fe35aa93ba333514b24f961722",
    "0xc91e2349ef6ca22d2de39dd51819b6aad922d3aecdeab452ba172f7d63e370cecd70575f597c09a174ba76bed05a48e562be0625336d16b8703147a6a231d6bf",
    "0x0140c8edca57108ce3f7e7a240ddd3ad74d81e2de62451fc1d558fdc79269adacd1c2526eeeef32f8c0432a9d56e2b4a8a732891c37c9b96641a9254ccfe5dc3e2ba",
    "0x00d72f15229d0096376da6651d9985bfd7c07f8d49583b545db3eab20e0a2c1e8615bd9e298455bdeb6b61378e77af1c54eee2ce37b2c61f5c9a8232951cb988b5b1"
  },
#endif	/* NOTYET */
/* --- P-256 NSA Suite B */
  { "prime256v1", "This is only a test message. It is 48 bytes long", PGPHASHALGO_SHA256,
    "0x70a12c2db16845ed56ff68cfc21a472b3f04d7d6851bf6349f2d7d5b3452b38a",
    "0x580ec00d856434334cef3f71ecaed4965b12ae37fa47055b1965c7b134ee45d0",
    "0x7214bc9647160bbd39ff2f80533f5dc6ddd70ddf86bb815661e805d5d4e6f27c",
    "0x7d1ff961980f961bdaa3233b6209f4013317d3e3f9e1493592dbeaa1af2bc367"
  },
/* --- P-384 NSA Suite B */
  { "secp384r1", "This is only a test message. It is 48 bytes long", PGPHASHALGO_SHA384,
    "0xc838b85253ef8dc7394fa5808a5183981c7deef5a69ba8f4f2117ffea39cfcd90e95f6cbc854abacab701d50c1f3cf24",
    "0xdc6b44036989a196e39d1cdac000812f4bdd8b2db41bb33af51372585ebd1db63f0ce8275aa1fd45e2d2a735f8749359",
    "0xa0c27ec893092dea1e1bd2ccfed3cf945c8134ed0c9f81311a0f4a05942db8dbed8dd59f267471d5462aa14fe72de856",
    "0x20ab3f45b74f10b6e11f96a2c8eb694d206b9dda86d3c7e331c26b22c987b7537726577667adadf168ebbe803794a402"
  },
  { NULL, NULL, 0,
    NULL,
    NULL
  }
};

static int pgpDigTestOne(pgpDig dig,
		const char * msg, const char *r_in, const char *s_in)
{
    pgpDigParams sigp = pgpGetSignature(dig);
    DIGEST_CTX ctx;
    int rc;
int bingo = 0;

    /* create the key */
    rc = pgpImplGenerateECDSA(dig);
    if (!rc)
	goto exit;
bingo++;

    /* generate the hash */
    ctx = rpmDigestInit(sigp->hash_algo, RPMDIGEST_NONE);
    rc = rpmDigestUpdate(ctx, msg, strlen(msg));

    /* create the signature */
    rc = pgpImplSetECDSA(rpmDigestDup(ctx), dig, sigp);
    rc = pgpImplSignECDSA(dig);
    if (!rc)
	goto exit;
bingo++;

#if defined(_RPMSSL_INTERNAL)
    /* check the {r,s} parameters */
    {	rpmssl ssl = dig->impl;
	if (!rpmsslLoadBN(&ssl->r, r_in, _rpmssl_spew)
	 || BN_cmp(ssl->ecdsasig->r, ssl->r))
	    goto exit;
	if (!rpmsslLoadBN(&ssl->s, s_in, _rpmssl_spew)
	 || BN_cmp(ssl->ecdsasig->s, ssl->s))
	    goto exit;
    }
bingo++;
#endif	/* _RPMSSL_INTERNAL */

    /* verify the signature */
    rc = pgpImplSetECDSA(ctx, dig, sigp);
    ctx = NULL;

    rc = pgpImplVerifyECDSA(dig);
    if (rc != 1)
	goto exit;
bingo++;

exit:

    return bingo;		/* XXX 1 on success */
}

static int pgpDigTests(pgpDig dig)
{
    pgpDigParams sigp = pgpGetSignature(dig);
    struct ECDSAvec_s * v;
    int ret = -1;	/* assume failure */
static const char dots[] = "..........";

    rpmlog(RPMLOG_NOTICE, "========== X9.62/NIST/NSA ECDSA vectors:\n");

    /* set own rand method */
    if (!change_rand())
	goto exit;

    for (v = ECDSAvecs; v->r != NULL; v++) {
	int rc;

	pgpDigClean(dig);
#if defined(_RPMSSL_INTERNAL)
((rpmssl)dig->impl)->nid = curve2nid(v->name);
#endif
#if defined(_RPMNSS_INTERNAL)
rpmnssLoadParams(dig, v->name);
#endif
sigp->hash_algo = v->dalgo;
_ix = 0;
_numbers = &v->d;
_numbers_max = 2;

	rpmlog(RPMLOG_INFO, "%s:\t", v->name);

	rc = pgpDigTestOne(dig, v->msg, v->r, v->s);

	rpmlog(RPMLOG_INFO, "%s %s\n", &dots[strlen(dots)-rc],
			(rc >= 4 ? " ok" : " failed"));
	if (rc < 4)
	    ret = 0;

	pgpImplClean(dig->impl);	/* XXX needed for memleaks */

    }
    if (ret == -1)
	ret = 1;

exit:
    if (!restore_rand())
	ret = -1;

    return ret;
}

#if defined(_RPMSSL_INTERNAL)
static int test_builtin(pgpDig dig)
{
    pgpDigParams sigp = pgpGetSignature(dig);
rpmssl ssl = dig->impl;
    size_t n = 0;
    unsigned char * digest = NULL;
    size_t digestlen = 0;
    unsigned char * digest_bad = NULL;
    size_t digestlen_bad = 0;
    unsigned char *ecdsasig = NULL;
    unsigned int ecsdasiglen;
    int ret = -1;	/* assume failure */
static const char dots[] = "...............";
int bingo = 0;
int xx;

pgpDigClean(dig);
sigp->hash_algo = PGPHASHALGO_SHA1;

    /* create a "good" and a "bad" digest. */
    {	const char * msg;
	DIGEST_CTX ctx;
	int xx;

	msg = "bad";
	ctx = rpmDigestInit(sigp->hash_algo, RPMDIGEST_NONE);
	xx = rpmDigestUpdate(ctx, msg, strlen(msg));
	xx = rpmDigestFinal(ctx, &digest_bad, &digestlen_bad, 0);
	ctx = NULL;

	msg = "good";
	ctx = rpmDigestInit(sigp->hash_algo, RPMDIGEST_NONE);
	xx = rpmDigestUpdate(ctx, msg, strlen(msg));
	xx = rpmDigestFinal(ctx, &digest, &digestlen, 0);
	ctx = NULL;
    }

    /* create and verify a ecdsa signature with every availble curve */
    rpmlog(RPMLOG_NOTICE, "========== sign/verify on internal ECDSA curves:\n");

    /* get a list of all internal curves */
    ssl->ncurves = EC_get_builtin_curves(NULL, 0);

    ssl->curves = OPENSSL_malloc(ssl->ncurves * sizeof(EC_builtin_curve));
assert(ssl->curves);

    xx = EC_get_builtin_curves(ssl->curves, ssl->ncurves);
assert(xx);

    /* now create and verify a signature for every curve */
    for (n = 0; n < ssl->ncurves; n++) {
	unsigned char offset;
	unsigned char dirt;

	ssl->nid = ssl->curves[n].nid;
	if (ssl->nid == NID_ipsec3 || ssl->nid == NID_ipsec4)
	    continue;

	/* create new ecdsa key (for EC_KEY_set_group) */
	ssl->ecdsakey = EC_KEY_new();
assert(ssl->ecdsakey);
	ssl->group = EC_GROUP_new_by_curve_name(ssl->nid);
assert(ssl->group);
	xx = EC_KEY_set_group(ssl->ecdsakey, ssl->group);
assert(xx);
	EC_GROUP_free(ssl->group);
	ssl->group = NULL;

	rpmlog(RPMLOG_INFO, "%s:\t", OBJ_nid2sn(ssl->nid));
bingo = 0;

	/* create key */
	if (!EC_KEY_generate_key(ssl->ecdsakey))
	    goto bottom;

	/* create second key */
	ssl->ecdsakey_bad = EC_KEY_new();
assert(ssl->ecdsakey_bad);
	ssl->group = EC_GROUP_new_by_curve_name(ssl->nid);
assert(ssl->group);
	xx = EC_KEY_set_group(ssl->ecdsakey_bad, ssl->group);
assert(xx);
	EC_GROUP_free(ssl->group);
	ssl->group = NULL;

	if (!EC_KEY_generate_key(ssl->ecdsakey_bad))
	    goto bottom;
bingo++;

	/* check key */
	if (!EC_KEY_check_key(ssl->ecdsakey))
	    goto bottom;
bingo++;

	/* create signature */
	ecsdasiglen = ECDSA_size(ssl->ecdsakey);
	ecdsasig = OPENSSL_malloc(ecsdasiglen);
assert(ecdsasig);
	if (!ECDSA_sign(0, digest, digestlen, ecdsasig, &ecsdasiglen, ssl->ecdsakey))
	    goto bottom;
bingo++;

	/* verify signature */
	if (ECDSA_verify(0, digest, digestlen, ecdsasig, ecsdasiglen, ssl->ecdsakey) != 1)
	    goto bottom;
bingo++;

	/* verify signature with the wrong key */
	if (ECDSA_verify(0, digest, digestlen, ecdsasig, ecsdasiglen,
			 ssl->ecdsakey_bad) == 1)
	    goto bottom;
bingo++;

	/* wrong digest */
	if (ECDSA_verify(0, digest_bad, sizeof(digest_bad), ecdsasig, ecsdasiglen,
			 ssl->ecdsakey) == 1)
	    goto bottom;
bingo++;

	/* modify a single byte of the signature */
	offset = ecdsasig[10] % ecsdasiglen;
	dirt = ecdsasig[11];
	ecdsasig[offset] ^= dirt ? dirt : 1;
	if (ECDSA_verify(0, digest, digestlen, ecdsasig, ecsdasiglen, ssl->ecdsakey) == 1)
	    goto bottom;
bingo++;

bottom:
	rpmlog(RPMLOG_INFO, "%s %s\n", &dots[strlen(dots) - bingo],
			(bingo >= 7 ? " ok" : " failed"));
	if (bingo < 7)
	    ret = 0;

	/* cleanup */
	if (ecdsasig)
	    OPENSSL_free(ecdsasig);
	ecdsasig = NULL;
	pgpImplClean(ssl);
    }
    if (ret == -1)
	ret = 1;

    if (ecdsasig)
	OPENSSL_free(ecdsasig);
    ecdsasig = NULL;

    if (ssl->curves)
            OPENSSL_free(ssl->curves);
    ssl->curves = NULL;
    ssl->ncurves = 0;

    pgpImplClean(ssl);

digest_bad = _free(digest_bad);
digestlen_bad = 0;
digest = _free(digest);
digestlen = 0;

    return ret;
}
#endif	/* _RPMSSL_INTERNAL */

/*==============================================================*/

static struct poptOption optionsTable[] = {
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options:"), NULL },

  POPT_AUTOHELP
  POPT_TABLEEND
};

#if defined(_RPMBC_INTERNAL)
static pgpDig _rpmbcFini(pgpDig dig)
{
rpmbc bc = (dig ? dig->impl : NULL);
    if (bc) {
    }
dig = pgpDigFree(dig);
    return NULL;
}

static pgpDig _rpmbcInit(void)
{
pgpDig dig;
rpmbc bc;
    pgpImplVecs = &rpmbcImplVecs;

dig = pgpDigNew(0);
bc = dig->impl;

    return dig;
}
#endif	/* _RPMBC_INTERNAL */

#if defined(_RPMGC_INTERNAL)
static pgpDig _rpmgcFini(pgpDig dig)
{
rpmgc gc = (dig ? dig->impl : NULL);
    if (gc) {
    }
dig = pgpDigFree(dig);
    return NULL;
}

static pgpDig _rpmgcInit(void)
{
pgpDig dig;
rpmgc gc;

rpmgcImplVecs._pgpSetECDSA = rpmgcSetECDSA;
rpmgcImplVecs._pgpVerifyECDSA = rpmgcVerifyECDSA;
rpmgcImplVecs._pgpSignECDSA = rpmgcSignECDSA;
rpmgcImplVecs._pgpGenerateECDSA = rpmgcGenerateECDSA;

    pgpImplVecs = &rpmgcImplVecs;

dig = pgpDigNew(0);
gc = dig->impl;

    return dig;
}
#endif	/* _RPMGC_INTERNAL */

#if defined(_RPMNSS_INTERNAL)
static pgpDig _rpmnssFini(pgpDig dig)
{
rpmnss nss = (dig ? dig->impl : NULL);
    if (nss) {
    }
dig = pgpDigFree(dig);
    return NULL;
}

static pgpDig _rpmnssInit(void)
{
pgpDig dig;
rpmnss nss;

rpmnssImplVecs._pgpSetECDSA = rpmnssSetECDSA;
rpmnssImplVecs._pgpVerifyECDSA = rpmnssVerifyECDSA;
rpmnssImplVecs._pgpSignECDSA = rpmnssSignECDSA;
rpmnssImplVecs._pgpGenerateECDSA = rpmnssGenerateECDSA;

    pgpImplVecs = &rpmnssImplVecs;

dig = pgpDigNew(0);
nss = dig->impl;

    return dig;
}
#endif	/* _RPMNSS_INTERNAL */

#if defined(_RPMSSL_INTERNAL)
static pgpDig _rpmsslFini(pgpDig dig)
{
rpmssl ssl = (dig ? dig->impl : NULL);
    if (ssl) {
	CRYPTO_cleanup_all_ex_data();
	ERR_remove_thread_state(NULL);
	ERR_free_strings();

	if (ssl->out) {
	    CRYPTO_mem_leaks(ssl->out);
	    BIO_free(ssl->out);
	}
	ssl->out = NULL;
    }
dig = pgpDigFree(dig);
    return NULL;
}

static pgpDig _rpmsslInit(void)
{
pgpDig dig;
rpmssl ssl;
    pgpImplVecs = &rpmsslImplVecs;

    /* enable memory leak checking unless explicitly disabled */
    if (!((getenv("OPENSSL_DEBUG_MEMORY") != NULL) &&
	  (0 == strcmp(getenv("OPENSSL_DEBUG_MEMORY"), "off")))) {
	    CRYPTO_malloc_debug_init();
	CRYPTO_set_mem_debug_options(V_CRYPTO_MDEBUG_ALL);
    } else {
	/* OPENSSL_DEBUG_MEMORY=off */
	CRYPTO_set_mem_debug_functions(0, 0, 0, 0, 0);
    }
    CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

    /* initialize the prng */
    {   static const char rnd_seed[] =
	      "string to make the random number generator think it has entropy";
	RAND_seed(rnd_seed, sizeof(rnd_seed));
    }

dig = pgpDigNew(0);
ssl = dig->impl;
ssl->out = BIO_new_fp(stdout, BIO_NOCLOSE);

    return dig;
}
#endif	/* _RPMSSL_INTERNAL */

int main(int argc, char *argv[])
{
    poptContext con = rpmioInit(argc, argv, optionsTable);
pgpDig dig;
    int ec = 1;	/* assume failure */
int rc;

#if defined(_RPMSSL_INTERNAL) && !defined(OPENSSL_NO_ECDSA)
dig = _rpmsslInit();
    if (pgpDigTests(dig) <= 0)
	goto exit;
    if (test_builtin(dig) <= 0)
	goto exit;
dig = _rpmsslFini(dig);
#endif

#if defined(_RPMNSS_INTERNAL)
dig = _rpmnssInit();
    rc = 1;	/* assume success */
    if (pgpDigTests(dig) <= 0)
	rc = 0;
dig = _rpmnssFini(dig);
    if (rc <= 0)
	goto exit;
#endif	/* _RPMNSS_INTERNAL */

#if defined(_RPMGC_INTERNAL)
dig = _rpmgcInit();
    rc = 1;	/* assume success */
    if (pgpDigTests(dig) <= 0)
	rc = 0;
dig = _rpmgcFini(dig);
    if (rc <= 0)
	goto exit;
#endif	/* _RPMGC_INTERNAL */

#if defined(_RPMBC_INTERNAL)
dig = _rpmbcInit();
dig = _rpmbcFini(dig);
#endif	/* _RPMBC_INTERNAL */

    ec = 0;

exit:
    rpmlog(RPMLOG_INFO, "ECDSA tests %s\n", (ec ? "failed" : "passed"));

    con = rpmioFini(con);

    return ec;
}

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

#ifdef	NOTYET
#define	_RPMBC_INTERNAL
#include <rpmbc.h>
#define	_RPMGC_INTERNAL
#include <rpmgc.h>
#define	_RPMNSS_INTERNAL
#include <rpmnss.h>
#include <pk11pub.h>
#endif	/* NOTYET */

#define	_RPMSSL_INTERNAL
#include <rpmssl.h>
#include <openssl/opensslconf.h>	/* XXX OPENSSL_NO_ECDSA */
#include <openssl/crypto.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/ecdsa.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include "debug.h"

/*@access pgpDig @*/
/*@access pgpDigParams @*/

/*@-redecl@*/
/*@unchecked@*/
extern int _pgp_debug;

/*@unchecked@*/
extern int _pgp_print;
/*@=redecl@*/

#ifdef	NOTYET
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
#endif

/*==============================================================*/

#if defined(_RPMNSS_INTERNAL)

static void rpmnssDumpSECITEM(const char * msg, SECItem * p)
{
    fprintf(stderr, "%s: %p", msg, p);
    if (p)
	fprintf(stderr, " type %d %p[%u]\n\t%s",
		p->type, p->data, (unsigned)p->len, pgpHexStr(p->data, p->len));
    fprintf(stderr, "\n");
}

static void rpmnssDumpPUBKEY(const char * msg, SECKEYPublicKey * p)
{
    fprintf(stderr, "%s: %p", msg, p);
    if (p)
	fprintf(stderr, " type %d slot %p id %p",
		p->keyType, p->pkcs11Slot, (void *)p->pkcs11ID);
    fprintf(stderr, "\n");
}

static void rpmnssDumpPRVKEY(const char * msg, SECKEYPrivateKey * p)
{
    fprintf(stderr, "%3s: %p", msg, p);
    if (p)
	fprintf(stderr, " type %d slot %p id %p",
		p->keyType, p->pkcs11Slot, (void *)p->pkcs11ID);
    fprintf(stderr, "\n");
}

static
int rpmnssSetECDSA(/*@only@*/ DIGEST_CTX ctx, /*@unused@*/pgpDig dig, pgpDigParams sigp)
	/*@*/
{
    int rc = 1;		/* assume failure */;
    rpmnss nss = dig->impl;
    int xx;

assert(sigp->hash_algo == rpmDigestAlgo(ctx));

nss->digest = _free(nss->digest);
nss->digestlen = 0;
    xx = rpmDigestFinal(ctx, (void **)&nss->digest, &nss->digestlen, 0);

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
	goto exit;
assert(nss->sigalg != 0);

#ifdef	NOTYET
    /* Compare leading 16 bits of digest for quick check. */
    rc = memcmp(nss->digest, sigp->signhash16, sizeof(sigp->signhash16));
#else
    rc = 0;
#endif

exit:
if (_pgp_debug)
fprintf(stderr, "<-- %s(%p,%p) rc %d\n", __FUNCTION__, dig, sigp, rc);
    return rc;
}

static
int rpmnssVerifyECDSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    rpmnss nss = dig->impl;
    int rc = 0;		/* assume failure */;

    nss->item.type = siBuffer;
    nss->item.data = nss->digest;
    nss->item.len = (unsigned) nss->digestlen;

    if (VFY_VerifyDigest(&nss->item, nss->ecdsa, nss->ecdsasig, nss->sigalg, NULL) == SECSuccess)
	rc = 1;

if (_pgp_debug || rc == 0) {
rpmnssDumpSECITEM("  digest", &nss->item);
rpmnssDumpPUBKEY( "     pub", nss->ecdsa);
rpmnssDumpSECITEM("     sig", nss->ecdsasig);
fprintf(stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, dig, rc);
}

    return rc;
}

static
int rpmnssSignECDSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    rpmnss nss = dig->impl;
    int rc = 0;		/* assume failure. */

    nss->item.type = siBuffer;
    nss->item.data = nss->digest;
    nss->item.len = (unsigned) nss->digestlen;

if (nss->ecdsasig != NULL) {
    SECITEM_ZfreeItem(nss->ecdsasig, PR_TRUE);
    nss->ecdsasig = NULL;
}

nss->ecdsasig = SECITEM_AllocItem(NULL, NULL, 0);
nss->ecdsasig->type = siBuffer;

    if (SGN_Digest(nss->ecpriv, nss->sigalg, nss->ecdsasig, &nss->item) == SECSuccess)
	rc = 1;

if (_pgp_debug || rc == 0) {
rpmnssDumpSECITEM("  digest", &nss->item);
rpmnssDumpPRVKEY( "    priv", nss->ecpriv);
rpmnssDumpSECITEM("     sig", nss->ecdsasig);
fprintf(stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, dig, rc);
}

    return rc;
}

static
int rpmnssGenerateECDSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    rpmnss nss = dig->impl;
    int rc = 0;		/* assume failure. */

if (nss->ecpriv != NULL) {
    SECKEY_DestroyPrivateKey(nss->ecpriv);
    nss->ecpriv = NULL;
}
if (nss->ecdsa != NULL) {
    SECKEY_DestroyPublicKey(nss->ecdsa);
    nss->ecdsa = NULL;
}

#ifndef	DYING
    {	CK_MECHANISM_TYPE _type = CKM_EC_KEY_PAIR_GEN;
	PK11SlotInfo * _slot = PK11_GetBestSlot(_type, NULL);

	if (_slot) {
	    nss->ecpriv = PK11_GenerateKeyPair(_slot, _type, nss->ecparams,
			&nss->ecdsa, PR_FALSE, PR_FALSE, NULL);
	    PK11_FreeSlot(_slot);
	}
    }
#else
    nss->ecpriv = SECKEY_CreateECPrivateKey(nss->ecparams, &nss->ecdsa, NULL);
#endif

    rc = (nss->ecpriv && nss->ecdsa);

if (_pgp_debug || rc == 0) {
rpmnssDumpPRVKEY("    priv", nss->ecpriv);
rpmnssDumpPUBKEY("     pub", nss->ecdsa);
fprintf(stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, dig, rc);
}

    return rc;
}
#endif	/* _RPMNSS_INTERNAL */

/*==============================================================*/

#if defined(_RPMSSL_INTERNAL)

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

/*@-modfilesys@*/
static
void hexdump(const char * msg, unsigned char * b, size_t blen)
	/*@*/
{
    static const char hex[] = "0123456789ABCDEF";

    fprintf(stderr, "*** %s:", msg);
    if (b != NULL)
    while (blen > 0) {
	fprintf(stderr, "%c%c",
		hex[ (unsigned)((*b >> 4) & 0x0f) ],
		hex[ (unsigned)((*b     ) & 0x0f) ]);
	blen--;
	b++;
    }
    fprintf(stderr, "\n");
    return;
}
/*@=modfilesys@*/

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

#define SetKey \
  BIGNUM * cex = NULL; \
  int xx; \
  xx = rpmsslLoadBN(&key->n, n, _rpmssl_spew); \
  xx = rpmsslLoadBN(&key->e, e, _rpmssl_spew); \
  xx = rpmsslLoadBN(&key->d, d, _rpmssl_spew); \
  xx = rpmsslLoadBN(&key->p, p, _rpmssl_spew); \
  xx = rpmsslLoadBN(&key->q, q, _rpmssl_spew); \
  xx = rpmsslLoadBN(&key->dmp1, dmp1, _rpmssl_spew); \
  xx = rpmsslLoadBN(&key->dmq1, dmq1, _rpmssl_spew); \
  xx = rpmsslLoadBN(&key->iqmp, iqmp, _rpmssl_spew); \
  xx = rpmsslLoadBN(&cex, ctext_ex, _rpmssl_spew); \
  xx = BN_bn2bin(cex, c); \
  BN_free(cex); \
  return xx;

static int key1(RSA *key, unsigned char *c)
{
    static char n[] = "0x"
	"00AA36ABCE88ACFDFF55523C7FC4523F90EFA00DF3774A259F2E62B4C5D99CB5"
	"ADB300A0285E5301930E0C70FB6876939CE616CE624A11E0086D341EBCACA0A1"
	"F5";
    static char e[] = "0x" "11";
    static char d[] =
	"0A033748626487695F5F30BC38B98B44C2CD2DFF434098CD20D8A138D090BF64"
	"797C3FA7A2CDCB3CD1E0BDBA2654B4F9DF8E8AE59D733D9F33B301624AFD1D51";
    static char p[] = "0x"
	"00D840B41666B42E92EA0DA3B43204B5CFCE3352524D0416A5A441E700AF4612"
	"0D";
    static char q[] = "0x"
	"00C97FB1F027F453F6341233EAAAD1D9353F6C42D08866B1D05A0F2035028B9D"
	"89";
    static char dmp1[] = "0x"
	"590B9572A2C2A9C406059DC2AB2F1DAFEB7E8B4F10A7549E8EEDF5B4FCE09E05";
    static char dmq1[] = "0x"
	"008E3C0521FE15E0EA06A36FF0F10C9952C35B7A7514FD3238B80AAD5298628D"
	"51";
    static char iqmp[] = "0x"
	"363FF7189DA8E90B1D341F71D09B76A8A943E11D10B24D249F2DEAFEF80C1826";
    static char ctext_ex[] = "0x"
	"1b8f05f9ca1a79526e53f3cc514fdb892bfb9193231e78b992e68d50a480cb52"
	"33895c74958d5d02ab8c0fd040eb5844b005c39ed8274a9dbfa80671409439d2";

    SetKey;
}

static int key2(RSA *key, unsigned char *c)
{
    static char n[] = "0x"
	"00A3079A90DF0DFD72AC090CCC2A78B87413133E40759C98FAF8204F358A0B26"
	"3C6770E783A93B6971B73779D2717BE83477CF";
    static char e[] = "0x" "3";
    static char d[] = "0x"
	"6CAFBC6094B3FE4C72B0B332C6FB25A2B76229804E6865FCA45A74DF0F8FB841"
	"3B52C0D0E53D9B590FF19BE79F49DD21E5EB";
    static char p[] = "0x"
	"00CF2035028B9D869840B41666B42E92EA0DA3B43204B5CFCE91";
    static char q[] = "0x"
	"00C97FB1F027F453F6341233EAAAD1D9353F6C42D08866B1D05F";
    static char dmp1[] = "0x"
	"008A1578AC5D13AF102B22B999CD7461F15E6D22CC0323DFDF0B";
    static char dmq1[] = "0x"
	"008655214AC54D8D4ECD6177F1C73690CE2A482C8B0599CBE03F";
    static char iqmp[] = "0x"
	"0083EFEFB8A9A40D1DB6ED98AD84ED1335DCC108F322D057CF8D";
    static char ctext_ex[] = "0x"
	"14bddd28c98335192380e8e549b1582a8b40b4486d03a6a5311f1fd5f0a180e4"
	"17530329a9349074b1521354290824526251";

    SetKey;
}

static int key3(RSA *key, unsigned char *c)
{
    static char n[] = "0x"
	"00BBF82F090682CE9C2338AC2B9DA871F7368D07EED41043A440D6B6F07454F5"
	"1FB8DFBAAF035C02AB61EA48CEEB6FCD4876ED520D60E1EC4619719D8A5B8B80"
	"7FAFB8E0A3DFC737723EE6B4B7D93A2584EE6A649D060953748834B245459839"
	"4EE0AAB12D7B61A51F527A9A41F6C1687FE2537298CA2A8F5946F8E5FD091DBD"
	"CB";
    static char e[] = "0x" "11";
    static char d[] = "0x"
	"00A5DAFC5341FAF289C4B988DB30C1CDF83F31251E0668B42784813801579641"
	"B29410B3C7998D6BC465745E5C392669D6870DA2C082A939E37FDCB82EC93EDA"
	"C97FF3AD5950ACCFBC111C76F1A9529444E56AAF68C56C092CD38DC3BEF5D20A"
	"939926ED4F74A13EDDFBE1A1CECC4894AF9428C2B7B8883FE4463A4BC85B1CB3"
	"C1";
    static char p[] = "0x"
	"00EECFAE81B1B9B3C908810B10A1B5600199EB9F44AEF4FDA493B81A9E3D84F6"
	"32124EF0236E5D1E3B7E28FAE7AA040A2D5B252176459D1F397541BA2A58FB65"
	"99";
    static char q[] = "0x"
	"00C97FB1F027F453F6341233EAAAD1D9353F6C42D08866B1D05A0F2035028B9D"
	"869840B41666B42E92EA0DA3B43204B5CFCE3352524D0416A5A441E700AF4615"
	"03";
    static char dmp1[] = "0x"
	"54494CA63EBA0337E4E24023FCD69A5AEB07DDDC0183A4D0AC9B54B051F2B13E"
	"D9490975EAB77414FF59C1F7692E9A2E202B38FC910A474174ADC93C1F67C981";
    static char dmq1[] = "0x"
	"471E0290FF0AF0750351B7F878864CA961ADBD3A8A7E991C5C0556A94C3146A7"
	"F9803F8F6F8AE342E931FD8AE47A220D1B99A495849807FE39F9245A9836DA3D";
    static char iqmp[] = "0x"
	"00B06C4FDABB6301198D265BDBAE9423B380F271F73453885093077FCD39E211"
	"9FC98632154F5883B167A967BF402B4E9E2E0F9656E698EA3666EDFB25798039"
	"F7";
    static char ctext_ex[] = "0x"
	"b8246b56a6ed5881aeb585d9a25b2ad790c417e080681bf1ac2bc3deb69d8bce"
	"f0c4366fec400af052a72e9b0effb5b3f2f192dbeaca03c12740057113bf1f06"
	"69ac22e9f3a7852e3c15d913cab0b8863a95c99294ce8674214954610346f4d4"
	"74b26f7c48b42ee68e1f572a1fc4026ac456b4f59f7b621ea1b9d88f64202fb1";

    SetKey;
}

static unsigned char ptext_ex[] = "\x54\x85\x9b\x34\x2c\x49\xea\x2a";
static size_t plen = sizeof(ptext_ex) - 1;
static unsigned char ptext[256];
static unsigned char ctext[256];
static unsigned char ctext_ex[256];
static size_t clen = 0;

static void rpmsslLoadKEY(pgpDig dig, int v)
{
    rpmssl ssl = dig->impl;

    ssl->rsa = RSA_new();
    switch (v%3) {
    case 0:
	clen = key1(ssl->rsa, ctext_ex);
	break;
    case 1:
	clen = key2(ssl->rsa, ctext_ex);
	break;
    case 2:
	clen = key3(ssl->rsa, ctext_ex);
	break;
    }
    if (v/3 >= 1) ssl->rsa->flags |= RSA_FLAG_NO_CONSTTIME;
}

static int rpmsslTestPKCS(pgpDig dig)
{
    rpmssl ssl = dig->impl;
    const char * msg;
    int num;
    int rc = 0;		/* assume failure */

ERR_clear_error();
    num = RSA_public_encrypt(plen, ptext_ex, ctext, ssl->rsa,
				 RSA_PKCS1_PADDING);
    msg = "encryption";
    if (num != (int)clen)
	goto exit;
  
    num = RSA_private_decrypt(num, ctext, ptext, ssl->rsa,
				  RSA_PKCS1_PADDING);
    msg = "decryption";
    if (num != (int)plen || memcmp(ptext, ptext_ex, num) != 0)
	goto exit;
    msg = "encryption/decryption";
    rc = 1;

exit:
    fprintf(stderr, "PKCS #1 v1.5 %s %s\n", msg, (!rc ? "failed" : "ok"));
    return rc;		/* XXX 1 on success */
}

static int pad_unknown(void)
{
    unsigned long l;
    while ((l = ERR_get_error()) != 0)
      if (ERR_GET_REASON(l) == RSA_R_UNKNOWN_PADDING_TYPE)
	return 1;
    return 0;
}

static int rpmsslTestOAEP(pgpDig dig)
{
    rpmssl ssl = dig->impl;
    const char * msg;
    int num;
    size_t n;
    int rc = 0;		/* assume failure */

ERR_clear_error();

    num = RSA_public_encrypt(plen, ptext_ex, ctext, ssl->rsa,
				 RSA_PKCS1_OAEP_PADDING);
    msg = "unsupported";
    if (num == -1 && pad_unknown())
	goto exit;
    msg = "encryption";
    if (num != (int)clen)
	goto exit;

    num = RSA_private_decrypt(num, ctext, ptext, ssl->rsa,
				  RSA_PKCS1_OAEP_PADDING);
    msg = "decryption (encrypted data)";
    if (num != (int)plen || memcmp(ptext, ptext_ex, num) != 0)
	goto exit;
    if (memcmp(ctext, ctext_ex, num) != 0) {
	/* Different ciphertexts (rsa_oaep.c w/o -DPKCS_TESTVECT). */
	num = RSA_private_decrypt(clen, ctext_ex, ptext, ssl->rsa,
				  RSA_PKCS1_OAEP_PADDING);

	msg = "decryption (test vector data)";
	if (num != (int)plen || memcmp(ptext, ptext_ex, num) != 0)
	    goto exit;
    }

    /* Try decrypting corrupted ciphertexts */
    msg = "decryption (corrupt data)";
    for (n = 0 ; n < clen ; ++n) {
	unsigned char saved = ctext[n];
	int b;
	for (b = 0 ; b < 256 ; ++b) {
	    if (b == saved)
		continue;
	    ctext[n] = b;
	    num = RSA_private_decrypt(num, ctext, ptext, ssl->rsa,
					  RSA_PKCS1_OAEP_PADDING);
	    if (num > 0)
		goto exit;
	}
    }

    msg = "encryption/decryption";
    rc = 1;

exit:
    fprintf(stderr, "OAEP %s %s\n", msg, (!rc ? "failed" : "ok"));
    return rc;		/* XXX 1 on success */
}

static
int rpmsslSetRSA(/*@only@*/ DIGEST_CTX ctx, pgpDig dig, pgpDigParams sigp)
	/*@modifies dig @*/
{
    rpmssl ssl = dig->impl;
    unsigned int nbits = BN_num_bits(ssl->c);
    unsigned int nb = (nbits + 7) >> 3;
    const char * prefix = rpmDigestASN1(ctx);
    const char * hexstr;
    const char * s;
    rpmuint8_t signhash16[2];
    char * tt;
    int xx;

assert(sigp->hash_algo == rpmDigestAlgo(ctx));
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
    xx = BN_hex2bn(&ssl->rsahm, hexstr);
/*@=moduncon =noeffectuncon @*/

/*@-modfilesys@*/
if (_pgp_debug < 0) fprintf(stderr, "*** rsahm: %s\n", hexstr);
    hexstr = _free(hexstr);
/*@=modfilesys@*/

    /* Compare leading 16 bits of digest for quick check. */
    s = dig->md5;
/*@-type@*/
    signhash16[0] = (rpmuint8_t) (nibble(s[0]) << 4) | nibble(s[1]);
    signhash16[1] = (rpmuint8_t) (nibble(s[2]) << 4) | nibble(s[3]);
/*@=type@*/
    return memcmp(signhash16, sigp->signhash16, sizeof(sigp->signhash16));
}

static unsigned char * rpmsslBN2bin(const char * msg, const BIGNUM * s, size_t maxn)
{
    unsigned char * t = xcalloc(1, maxn);
/*@-modunconnomods@*/
    size_t nt = BN_bn2bin(s, t);
/*@=modunconnomods@*/

    if (nt < maxn) {
	size_t pad = (maxn - nt);
/*@-modfilesys@*/
if (_pgp_debug < 0) fprintf(stderr, "\tmemmove(%p, %p, %u)\n", t+pad, t, (unsigned)nt);
/*@=modfilesys@*/
	memmove(t+pad, t, nt);
/*@-modfilesys@*/
if (_pgp_debug < 0) fprintf(stderr, "\tmemset(%p, 0, %u)\n", t, (unsigned)pad);
/*@=modfilesys@*/
	memset(t, 0, pad);
    }
/*@-modfilesys@*/
if (_pgp_debug < 0) hexdump(msg, t, maxn);
/*@=modfilesys@*/
    return t;
}

static
int rpmsslVerifyRSA(pgpDig dig)
	/*@*/
{
    rpmssl ssl = dig->impl;
/*@-moduncon@*/
    size_t maxn;
    unsigned char * hm;
    unsigned char *  c;
    size_t nb;
/*@=moduncon@*/
    size_t i;
    int rc = 0;
    int xx;

assert(ssl->rsa);	/* XXX ensure lazy malloc with parameter set. */
    maxn = BN_num_bytes(ssl->rsa->n);
    hm = rpmsslBN2bin("hm", ssl->rsahm, maxn);
    c = rpmsslBN2bin(" c", ssl->c, maxn);
    nb = RSA_public_decrypt((int)maxn, c, c, ssl->rsa, RSA_PKCS1_PADDING);

/*@=moduncon@*/
    /* Verify RSA signature. */
    /* XXX This is _NOT_ the correct openssl function to use:
     *	rc = RSA_verify(type, m, m_len, sigbuf, siglen, ssl->rsa)
     *
     * Here's what needs doing (from OpenPGP reference sources in 1999):
     *	static u32_t checkrsa(BIGNUM * a, RSA * key, u8_t * hash, int hlen)
     *	{
     *	  u8_t dbuf[MAXSIGM];
     *	  int j, ll;
     *
     *	  j = BN_bn2bin(a, dbuf);
     *	  ll = BN_num_bytes(key->n);
     *	  while (j < ll)
     *	    memmove(&dbuf[1], dbuf, j++), dbuf[0] = 0;
     *	  j = RSA_public_decrypt(ll, dbuf, dbuf, key, RSA_PKCS1_PADDING);
     *	  RSA_free(key);
     *	  return (j != hlen || memcmp(dbuf, hash, j));
     *	}
     */
    for (i = 2; i < maxn; i++) {
	if (hm[i] == 0xff)
	    continue;
	i++;
/*@-modfilesys@*/
if (_pgp_debug < 0) hexdump("HM", hm + i, (maxn - i));
/*@=modfilesys@*/
	break;
    }

/*@-modfilesys@*/
if (_pgp_debug < 0) hexdump("HM", hm + (maxn - nb), nb);
if (_pgp_debug < 0) hexdump(" C",  c, nb);
/*@=modfilesys@*/

    rc = ((maxn - i) == nb && (xx = memcmp(hm+i, c, nb)) == 0);

    c = _free(c);
    hm = _free(hm);

    return rc;
}

static
int rpmsslSignRSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    int rc = 0;		/* XXX always fail. */

    return rc;
}

static
int rpmsslGenerateRSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    int rc = 0;		/* XXX always fail. */

    return rc;
}

#endif	/* _RPMSSL_INTERNAL */

/*==============================================================*/

#ifdef	NOTYET
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
    if (pgpImplVecs == &rpmsslImplVecs) {
	rpmssl ssl = dig->impl;
	if (!rpmsslLoadBN(&ssl->r, r_in, _rpmssl_spew)
	 || BN_cmp(ssl->ecdsasig->r, ssl->r))
	    goto exit;
	if (!rpmsslLoadBN(&ssl->s, s_in, _rpmssl_spew)
	 || BN_cmp(ssl->ecdsasig->s, ssl->s))
	    goto exit;
    }
#endif	/* _RPMSSL_INTERNAL */
bingo++;

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
#endif	/* NOTYET */

static int pgpDigRSATests(pgpDig dig)
{
    int ret = -1;	/* assume failure */

#ifdef	DYING
    pgpDigParams sigp = pgpGetSignature(dig);
static const char dots[] = "..........";
    struct ECDSAvec_s * v;

    rpmlog(RPMLOG_NOTICE, "========== X9.62/NIST/NSA ECDSA vectors:\n");

    for (v = ECDSAvecs; v->r != NULL; v++) {
	int rc;

	pgpDigClean(dig);

#if defined(_RPMBC_INTERNAL)
if (pgpImplVecs == &rpmbcImplVecs) {
}
#endif
#if defined(_RPMGC_INTERNAL)
if (pgpImplVecs == &rpmgcImplVecs) {
    rpmgc gc = dig->impl;
    gc->nbits = v->nbits;
}
#endif
#if defined(_RPMNSS_INTERNAL)
if (pgpImplVecs == &rpmnssImplVecs) {
    rpmnssLoadParams(dig, v->name);
}
#endif
#if defined(_RPMSSL_INTERNAL)
if (pgpImplVecs == &rpmsslImplVecs) {
    rpmssl ssl = dig->impl;
    ssl->nid = curve2nid(v->name);
}
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

#else	/* DYING */

#if defined(_RPMSSL_INTERNAL)
    int v;

    for (v = 0; v < 6; v++) {

	rpmsslLoadKEY(dig, v);

	if (!rpmsslTestPKCS(dig))
	    ret = 0;

	if (!rpmsslTestOAEP(dig))
	    ret = 0;

	pgpImplClean(dig->impl);

    }
#endif	/* _RPMSSL_INTERNAL */

#endif	/* DYING */

    if (ret == -1)
	ret = 1;

    return ret;
}

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

    pgpImplVecs = &rpmgcImplVecs;

dig = pgpDigNew(0);
gc = dig->impl;

gcry_control (GCRYCTL_DISABLE_SECMEM, 0);
gcry_control (GCRYCTL_ENABLE_QUICK_RANDOM, 0);

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

rpmsslImplVecs._pgpSetRSA = rpmsslSetRSA;
rpmsslImplVecs._pgpVerifyRSA = rpmsslVerifyRSA;
rpmsslImplVecs._pgpSignRSA = rpmsslSignRSA;
rpmsslImplVecs._pgpGenerateRSA = rpmsslGenerateRSA;

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

#if defined(_RPMSSL_INTERNAL)
dig = _rpmsslInit();
    rc = 1;	/* assume success */
    if (pgpDigRSATests(dig) <= 0)
	rc = 0;
dig = _rpmsslFini(dig);
    if (rc <= 0)
	goto exit;
#endif

#if defined(_RPMGC_INTERNAL)
dig = _rpmgcInit();
    rc = 1;	/* assume success */
    if (pgpDigRSATests(dig) <= 0)
	rc = 0;
dig = _rpmgcFini(dig);
    if (rc <= 0)
	goto exit;
#endif	/* _RPMGC_INTERNAL */

#if defined(_RPMNSS_INTERNAL)
dig = _rpmnssInit();
    rc = 1;	/* assume success */
    if (pgpDigRSATests(dig) <= 0)
	rc = 0;
dig = _rpmnssFini(dig);
    if (rc <= 0)
	goto exit;
#endif	/* _RPMNSS_INTERNAL */

#if defined(_RPMBC_INTERNAL)
dig = _rpmbcInit();
dig = _rpmbcFini(dig);
#endif	/* _RPMBC_INTERNAL */

    ec = 0;

exit:
    rpmlog(RPMLOG_INFO, "RSA tests %s\n", (ec ? "failed" : "passed"));

    con = rpmioFini(con);

    return ec;
}

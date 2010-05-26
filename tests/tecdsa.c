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

#define	_RPMPGP_INTERNAL
#include <poptIO.h>
#define	_RPMSSL_INTERNAL
#include <rpmssl.h>

#include <openssl/opensslconf.h>	/* To see if OPENSSL_NO_ECDSA is defined */

#ifdef OPENSSL_NO_ECDSA
#ifdef	DYING
int main(int argc, char *argv[])
{
    puts("Elliptic curves are disabled.");
    return 0;
}
#endif
#endif

#include <openssl/crypto.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/ecdsa.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include "debug.h"

static int _rpmssl_spew;

static const RAND_METHOD *old_rand;

/*==============================================================*/

static rpmssl rpmsslFree(rpmssl ssl)
{
    if (ssl) {
	CRYPTO_cleanup_all_ex_data();
	ERR_remove_thread_state(NULL);
	ERR_free_strings();

	if (ssl->out) {
	    CRYPTO_mem_leaks(ssl->out);
	    BIO_free(ssl->out);
	}
	ssl->out = NULL;

	ssl = pgpImplFree(ssl);
    }
    return NULL;
}

static rpmssl rpmsslNew(FILE * fp)
{
    static int oneshot;
    rpmssl ssl;

    if (!oneshot) {
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

	oneshot++;
    }

    ssl = pgpImplInit();

    if (fp == NULL)
	fp = stdout;
    ssl->out = BIO_new_fp(fp, BIO_NOCLOSE);

    return ssl;
}

static int rpmsslPrint(rpmssl ssl, const char * fmt, ...)
{
    va_list ap;
    int rc;

    va_start(ap, fmt);
    rc = BIO_vprintf(ssl->out, fmt, ap);
    va_end(ap);
    (void) BIO_flush(ssl->out);
    return rc;
}

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
	fprintf(stderr, "\t%p: %s\n", *bnp, t=BN_bn2dec(*bnp));
	OPENSSL_free(t);
	fprintf(stderr, "\t%p: 0x%s\n", *bnp, t=BN_bn2hex(*bnp));
	OPENSSL_free(t);
    }

    return rc;
}

static
int rpmsslSetECDSA(/*@only@*/ DIGEST_CTX ctx, /*@unused@*/pgpDig dig, pgpDigParams sigp)
	/*@*/
{
    rpmssl ssl = dig->impl;
    int rc = 1;		/* assume failure. */
    int xx;

assert(sigp->hash_algo == rpmDigestAlgo(ctx));

ssl->digest = _free(ssl->digest);
ssl->digestlen = 0;
    xx = rpmDigestFinal(ctx, &ssl->digest, &ssl->digestlen, 0);

    /* Compare leading 16 bits of digest for quick check. */
    rc = 0;

    return rc;
}

static
int rpmsslGenkeyECDSA(pgpDig dig)
	/*@*/
{
    rpmssl ssl = dig->impl;
    int rc = 0;		/* assume failure. */

    if ((ssl->ecdsakey = EC_KEY_new_by_curve_name(ssl->nid)) != NULL
     && EC_KEY_generate_key(ssl->ecdsakey))
	rc = 1;

    return rc;		/* XXX 1 on success */
}

static
int rpmsslSignECDSA(pgpDig dig)
	/*@*/
{
    rpmssl ssl = dig->impl;
    int rc = 0;		/* assume failure. */

    ssl->ecdsasig = ECDSA_do_sign(ssl->digest, ssl->digestlen, ssl->ecdsakey);
    if (ssl->ecdsasig)
	rc = 1;

    return rc;		/* XXX 1 on success */
}

static
int rpmsslVerifyECDSA(/*@unused@*/pgpDig dig)
	/*@*/
{
    rpmssl ssl = dig->impl;
    int rc = 0;		/* XXX always fail. */

    rc = (ECDSA_do_verify(ssl->digest, ssl->digestlen, ssl->ecdsasig, ssl->ecdsakey) == 1);

    return rc;		/* XXX 1 on success */
}

/*==============================================================*/

static int fbytes(unsigned char *buf, int num)
{
    static int fbytes_counter = 0;
    static const char *numbers[] = {
#ifdef	DYING
	"651056770906015076056810763456358567190100156695615665659",
	"6140507067065001063065065565667405560006161556565665656654",
	"8763001015071075675010661307616710783570106710677817767166"
		"71676178726717",
	"7000000175690566466555057817571571075705015757757057795755"
		"55657156756655",
	"1275552191113212300012030439187146164646146646466749494799",
	"1542725565216523985789236956265265265235675811949404040041",
	"1456427555219115346513212300075341203043918714616464614664"
		"64667494947990",
	"1712787255652165239672857892369562652652652356758119494040"
		"40041670216363",
#else
    "0x1A8D598FC15BF0FD89030B5CB1111AEB92AE8BAF5EA475FB",
    "0xFA6DE29746BBEB7F8BB1E761F85F7DFB2983169D82FA2F4E",
    "0x7EF7C6FABEFFFDEA864206E80B0B08A9331ED93E698561B64CA0F7777F3D",
    "0x656C7196BF87DCC5D1F1020906DF2782360D36B2DE7A17ECE37D503784AF",
    "0x340562E1DDA332F9D2AEC168249B5696EE39D0ED4D03760F",
    "0x3EEACE72B4919D991738D521879F787CB590AFF8189D2B69",
    "0x151A30A6D843DB3B25063C5108255CC4448EC0F4D426D4EC884502229C96",
    "0x18D114BDF47E2913463E50375DC92784A14934A124F83D28CAF97C5D8AAB",
#endif
/* --- P-192 FIPS 186-3 */
    "0x7891686032fd8057f636b44b1f47cce564d2509923a7465b",
    "0xd06cb0a0ef2f708b0744f08aa06b6deedea9c0f80a69d847",
/* --- P-224 */
    "0x3f0c488e987c80be0fee521f8d90be6034ec69ae11ca72aa777481e8",
    "0xa548803b79df17c40cde3ff0e36d025143bcbba146ec32908eb84937",
/* --- P-256 */
    "0xc477f9f65c22cce20657faa5b2d1d8122336f851a508a1ed04e479c34985bf96",
    "0x7a1a7e52797fc8caaa435d2a4dace39158504bf204fbe19f14dbb427faee50ae",
/* --- P-384 */
    "0xf92c02ed629e4b48c0584b1c6ce3a3e3b4faae4afc6acb0455e73dfc392e6a0ae393a8565e6b9714d1224b57d83f8a08",
    "0x2e44ef1f8c0bea8394e3dda81ec6a7842a459b534701749e2ed95f054f0137680878e0749fc43f85edcae06cc2f43fef",
#ifdef	NOTYET
/* --- P-521 */
    "0x0100085f47b8e1b8b11b7eb33028c0b2888e304bfc98501955b45bba1478dc184eeedf09b86a5f7c21994406072787205e69a63709fe35aa93ba333514b24f961722",
    "0xc91e2349ef6ca22d2de39dd51819b6aad922d3aecdeab452ba172f7d63e370cecd70575f597c09a174ba76bed05a48e562be0625336d16b8703147a6a231d6bf",
#endif
/* --- P-256 NSA Suite B */
"0x70a12c2db16845ed56ff68cfc21a472b3f04d7d6851bf6349f2d7d5b3452b38a",
"0x580ec00d856434334cef3f71ecaed4965b12ae37fa47055b1965c7b134ee45d0",
/* --- P-384 NSA Suite B */
"0xc838b85253ef8dc7394fa5808a5183981c7deef5a69ba8f4f2117ffea39cfcd90e95f6cbc854abacab701d50c1f3cf24",
"0xdc6b44036989a196e39d1cdac000812f4bdd8b2db41bb33af51372585ebd1db63f0ce8275aa1fd45e2d2a735f8749359",
    NULL, NULL
    };
    BIGNUM *tmp = NULL;
    int ret = 0;	/* assume failure */

    if (numbers[fbytes_counter] == NULL)
	return 0;
#ifdef	DYING
    tmp = BN_new();
    if (!tmp)
	return 0;
    if (!BN_dec2bn(&tmp, numbers[fbytes_counter])) {
	BN_free(tmp);
	return 0;
    }
#else
    tmp = NULL;
    if (!rpmsslLoadBN(&tmp, numbers[fbytes_counter], _rpmssl_spew))
	goto exit;
#endif
    fbytes_counter++;
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

static int change_rand(void)
{
    static RAND_METHOD fake_rand;

    /* save old rand method */
    if ((old_rand = RAND_get_rand_method()) == NULL)
	return 0;

    fake_rand.seed = old_rand->seed;
    fake_rand.cleanup = old_rand->cleanup;
    fake_rand.add = old_rand->add;
    fake_rand.status = old_rand->status;
    /* use own random function */
    fake_rand.bytes = fbytes;
    fake_rand.pseudorand = old_rand->bytes;

    return (RAND_set_rand_method(&fake_rand) ? 1 : 0);
}

static int restore_rand(void)
{
    return (RAND_set_rand_method(old_rand) ? 1 : 0);
}

/*==============================================================*/

struct ECDSAvec_s {
    int nid;
    const char * msg;
    int dalgo;
    const char * d;
    const char * k;
    const char * r;
    const char * s;
} ECDSAvecs[] = {
/* ----- X9.66-1998 J.3.1 */
	/* same as secp192r1 */
  { NID_X9_62_prime192v1, "abc", PGPHASHALGO_SHA1,
#ifdef	DYING
    "651056770906015076056810763456358567190100156695615665659",
    "6140507067065001063065065565667405560006161556565665656654",
    "3342403536405981729393488334694600415596881826869351677613",
    "5735822328888155254683894997897571951568553642892029982342"
#else
    "0x1A8D598FC15BF0FD89030B5CB1111AEB92AE8BAF5EA475FB",
    "0xFA6DE29746BBEB7F8BB1E761F85F7DFB2983169D82FA2F4E",
    "0x885052380FF147B734C330C43D39B2C4A89F29B0F749FEAD",
    "0xE9ECC78106DEF82BF1070CF1D4D804C3CB390046951DF686"
#endif
  },
  { NID_X9_62_prime239v1, "abc", PGPHASHALGO_SHA1,
#ifdef	DYING
    "876300101507107567501066130761671078357010671067781776716671676178726717",
    "700000017569056646655505781757157107570501575775705779575555657156756655",
    "308636143175167811492622547300668018854959378758531778147462058306432176",
    "323813553209797357708078776831250505931891051755007842781978505179448783"
#else
    "0x7EF7C6FABEFFFDEA864206E80B0B08A9331ED93E698561B64CA0F7777F3D",
    "0x656C7196BF87DCC5D1F1020906DF2782360D36B2DE7A17ECE37D503784AF",
    "0x2CB7F36803EBB9C427C58D8265F11FC5084747133078FC279DE874FBECB0",
    "0x2EEAE988104E9C2234A3C2BEB1F53BFA5DC11FF36A875D1E3CCB1F7E45CF"
#endif
  },
  { NID_X9_62_c2tnb191v1, "abc", PGPHASHALGO_SHA1,
#ifdef	DYING
    "1275552191113212300012030439187146164646146646466749494799",
    "1542725565216523985789236956265265265235675811949404040041",
    "87194383164871543355722284926904419997237591535066528048",
    "308992691965804947361541664549085895292153777025772063598"
#else
    "0x340562E1DDA332F9D2AEC168249B5696EE39D0ED4D03760F",
    "0x3EEACE72B4919D991738D521879F787CB590AFF8189D2B69",
    "0x038E5A11FB55E4C65471DCD4998452B1E02D8AF7099BB930",
    "0x0C9A08C34468C244B4E5D6B21B3C68362807416020328B6E"
#endif
  },
  { NID_X9_62_c2tnb239v1, "abc", PGPHASHALGO_SHA1,
#ifdef	DYING
    "145642755521911534651321230007534120304391871461646461466464667494947990",
    "171278725565216523967285789236956265265265235675811949404040041670216363",
    "21596333210419611985018340039034612628818151486841789642455876922391552",
    "197030374000731686738334997654997227052849804072198819102649413465737174"
#else
    "0x151A30A6D843DB3B25063C5108255CC4448EC0F4D426D4EC884502229C96",
    "0x18D114BDF47E2913463E50375DC92784A14934A124F83D28CAF97C5D8AAB",
    "0x03210D71EF6C10157C0D1053DFF93E8B085F1E9BC22401F7A24798A63C00",
    "0x1C8C4343A8ECBF7C4D4E48F7D76D5658BC027C77086EC8B10097DEB307D6"
#endif
  },
/* --- P-192 FIPS 186-3 */
  { NID_X9_62_prime192v1, "Example of ECDSA with P-192", PGPHASHALGO_SHA1,
    "0x7891686032fd8057f636b44b1f47cce564d2509923a7465b",
    "0xd06cb0a0ef2f708b0744f08aa06b6deedea9c0f80a69d847",
    "0xf0ecba72b88cde399cc5a18e2a8b7da54d81d04fb9802821",
    "0x1e6d3d4ae2b1fab2bd2040f5dabf00f854fa140b6d21e8ed"
  },
/* --- P-224 */
  { NID_secp224r1, "Example of ECDSA with P-224", PGPHASHALGO_SHA224,
    "0x3f0c488e987c80be0fee521f8d90be6034ec69ae11ca72aa777481e8",
    "0xa548803b79df17c40cde3ff0e36d025143bcbba146ec32908eb84937",
    "0xc3a3f5b82712532004c6f6d1db672f55d931c3409ea1216d0be77380",
    "0xc5aa1eae6095dea34c9bd84da3852cca41a8bd9d5548f36dabdf6617"
  },
/* --- P-256 */
  { NID_X9_62_prime256v1, "Example of ECDSA with P-256", PGPHASHALGO_SHA256,
    "0xc477f9f65c22cce20657faa5b2d1d8122336f851a508a1ed04e479c34985bf96",
    "0x7a1a7e52797fc8caaa435d2a4dace39158504bf204fbe19f14dbb427faee50ae",
    "0x2b42f576d07f4165ff65d1f3b1500f81e44c316f1f0b3ef57325b69aca46104f",
    "0xdc42c2122d6392cd3e3a993a89502a8198c1886fe69d262c4b329bdb6b63faf1"
  },
/* --- P-384 */
  { NID_secp384r1, "Example of ECDSA with P-384", PGPHASHALGO_SHA384,
    "0xf92c02ed629e4b48c0584b1c6ce3a3e3b4faae4afc6acb0455e73dfc392e6a0ae393a8565e6b9714d1224b57d83f8a08",
    "0x2e44ef1f8c0bea8394e3dda81ec6a7842a459b534701749e2ed95f054f0137680878e0749fc43f85edcae06cc2f43fef",
    "0x30ea514fc0d38d8208756f068113c7cada9f66a3b40ea3b313d040d9b57dd41a332795d02cc7d507fcef9faf01a27088",
    "0xcc808e504be414f46c9027bcbf78adf067a43922d6fcaa66c4476875fbb7b94efd1f7d5dbe620bfb821c46d549683ad8"
  },
#ifdef	NOTYET
/* --- P-521 */
  { NID_secp521r1, "Example of ECDSA with P-521", PGPHASHALGO_SHA512,
    "0x0100085f47b8e1b8b11b7eb33028c0b2888e304bfc98501955b45bba1478dc184eeedf09b86a5f7c21994406072787205e69a63709fe35aa93ba333514b24f961722",
    "0xc91e2349ef6ca22d2de39dd51819b6aad922d3aecdeab452ba172f7d63e370cecd70575f597c09a174ba76bed05a48e562be0625336d16b8703147a6a231d6bf",
    "0x0140c8edca57108ce3f7e7a240ddd3ad74d81e2de62451fc1d558fdc79269adacd1c2526eeeef32f8c0432a9d56e2b4a8a732891c37c9b96641a9254ccfe5dc3e2ba",
    "0x00d72f15229d0096376da6651d9985bfd7c07f8d49583b545db3eab20e0a2c1e8615bd9e298455bdeb6b61378e77af1c54eee2ce37b2c61f5c9a8232951cb988b5b1"
  },
#endif	/* NOTYET */
/* --- P-256 NSA Suite B */
  { NID_X9_62_prime256v1, "This is only a test message. It is 48 bytes long", PGPHASHALGO_SHA256,
    "0x70a12c2db16845ed56ff68cfc21a472b3f04d7d6851bf6349f2d7d5b3452b38a",
    "0x580ec00d856434334cef3f71ecaed4965b12ae37fa47055b1965c7b134ee45d0",
    "0x7214bc9647160bbd39ff2f80533f5dc6ddd70ddf86bb815661e805d5d4e6f27c",
    "0x7d1ff961980f961bdaa3233b6209f4013317d3e3f9e1493592dbeaa1af2bc367"
  },
/* --- P-384 NSA Suite B */
  { NID_secp384r1, "This is only a test message. It is 48 bytes long", PGPHASHALGO_SHA384,
    "0xc838b85253ef8dc7394fa5808a5183981c7deef5a69ba8f4f2117ffea39cfcd90e95f6cbc854abacab701d50c1f3cf24",
    "0xdc6b44036989a196e39d1cdac000812f4bdd8b2db41bb33af51372585ebd1db63f0ce8275aa1fd45e2d2a735f8749359",
    "0xa0c27ec893092dea1e1bd2ccfed3cf945c8134ed0c9f81311a0f4a05942db8dbed8dd59f267471d5462aa14fe72de856",
    "0x20ab3f45b74f10b6e11f96a2c8eb694d206b9dda86d3c7e331c26b22c987b7537726577667adadf168ebbe803794a402"
  },
  { 0, NULL, 0,
    NULL,
    NULL
  }
};

static int rpmsslTestOne(rpmssl ssl, pgpDig dig,
		const char * msg, const char *r_in, const char *s_in)
{
    pgpDigParams sigp = pgpGetSignature(dig);
    DIGEST_CTX ctx;
    int ret = 0;	/* assume failure */
    int rc;

    /* create the key */
    rc = rpmsslGenkeyECDSA(dig);
    if (!rc)
	goto exit;
    rpmsslPrint(ssl, ".");

    /* generate the hash */
    ctx = rpmDigestInit(sigp->hash_algo, RPMDIGEST_NONE);
    rc = rpmDigestUpdate(ctx, msg, strlen(msg));

    /* create the signature */
    rc = rpmsslSetECDSA(rpmDigestDup(ctx), dig, sigp);
    rc = rpmsslSignECDSA(dig);
    if (!rc)
	goto exit;
    rpmsslPrint(ssl, ".");

    /* check the parameters */
    if (!rpmsslLoadBN(&ssl->r, r_in, _rpmssl_spew))
	goto exit;
    if (!rpmsslLoadBN(&ssl->s, s_in, _rpmssl_spew))
	goto exit;
    if (BN_cmp(ssl->ecdsasig->r, ssl->r) || BN_cmp(ssl->ecdsasig->s, ssl->s))
	goto exit;
    rpmsslPrint(ssl, ".");

    /* verify the signature */
    rc = rpmsslSetECDSA(ctx, dig, sigp);
    ctx = NULL;

    rc = rpmsslVerifyECDSA(dig);
    if (rc != 1)
	goto exit;
    rpmsslPrint(ssl, ".");

    ret = 1;

exit:

    return ret;		/* XXX 1 on success */
}

static int rpmsslTests(rpmssl ssl)
{
    pgpDig dig = pgpDigNew(0);
    pgpDigParams sigp = pgpGetSignature(dig);
    struct ECDSAvec_s * v;
    int ret = 1;	/* assume success */

    rpmsslPrint(ssl, "test vectors from X9.62/NIST/NSA:\n");

    /* set own rand method */
    if (!change_rand()) {
	ret = 0;
	goto exit;
    }

    for (v = ECDSAvecs; v->r != NULL; v++) {
	int rc;

	pgpDigClean(dig);
	ssl->nid = v->nid;
	sigp->hash_algo = v->dalgo;

	rpmsslPrint(ssl, "testing %s: ", OBJ_nid2sn(ssl->nid));

dig->impl = ssl;	/* XXX hack */
	rc = rpmsslTestOne(ssl, dig, v->msg, v->r, v->s);
dig->impl = NULL;	/* XXX hack */

	rpmsslPrint(ssl, "%s\n", (rc ? " ok" : " failed"));

	pgpImplClean(ssl);	/* XXX needed for memleaks */

	if (!rc)
	    ret = 0;
    }

exit:
    if (!restore_rand())
	ret = 0;

    dig = pgpDigFree(dig);

    return ret;
}

static int test_builtin(rpmssl ssl)
{
    pgpDig dig = pgpDigNew(0);
    pgpDigParams sigp = pgpGetSignature(dig);
    size_t n = 0;
    unsigned char * digest = NULL;
    size_t digestlen = 0;
    unsigned char * digest_bad = NULL;
    size_t digestlen_bad = 0;
    unsigned char *ecdsasig = NULL;
    unsigned int ecsdasiglen;
    int ret = 0;

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
    rpmsslPrint(ssl, "\ntesting ECDSA_sign() and ECDSA_verify() with some internal curves:\n");

    /* get a list of all internal curves */
    ssl->ncurves = EC_get_builtin_curves(NULL, 0);

    ssl->curves = OPENSSL_malloc(ssl->ncurves * sizeof(EC_builtin_curve));

    if (ssl->curves == NULL) {
	rpmsslPrint(ssl, "malloc error\n");
	goto exit;
    }

    if (!EC_get_builtin_curves(ssl->curves, ssl->ncurves)) {
	rpmsslPrint(ssl, "unable to get internal curves\n");
	goto exit;
    }

    /* now create and verify a signature for every curve */
    for (n = 0; n < ssl->ncurves; n++) {
	unsigned char offset;
	unsigned char dirt;

	ssl->nid = ssl->curves[n].nid;
	if (ssl->nid == NID_ipsec4)
	    continue;

	/* create new ecdsa key (for EC_KEY_set_group) */
	if ((ssl->ecdsakey = EC_KEY_new()) == NULL)
	    goto exit;
	ssl->group = EC_GROUP_new_by_curve_name(ssl->nid);
	if (ssl->group == NULL)
	    goto exit;
	if (EC_KEY_set_group(ssl->ecdsakey, ssl->group) == 0)
	    goto exit;
	EC_GROUP_free(ssl->group);
	ssl->group = NULL;

#ifdef	REFERENCE
Oakley-EC2N-3: .. failed

ECDSA test failed
3077970496:error:0306E06C:bignum routines:BN_mod_inverse:no inverse:bn_gcd.c:491:
3077970496:error:2A067003:lib(42):ECDSA_sign_setup:BN lib:ecs_ossl.c:182:
3077970496:error:2A06502A:lib(42):ECDSA_do_sign:reason(42):ecs_ossl.c:277:
#endif
	/* drop curves with lest than 160 bits */
	if (EC_GROUP_get_degree(EC_KEY_get0_group(ssl->ecdsakey)) < 160)
	{
#ifndef	DYING	/* XXX watchout: ssl->curves */
	    EC_KEY_free(ssl->ecdsakey);
	    ssl->ecdsakey = NULL;
#else
	    pgpImplClean(ssl);
#endif
	    continue;
	}
	rpmsslPrint(ssl, "%s: ", OBJ_nid2sn(ssl->nid));

	/* create key */
	if (!EC_KEY_generate_key(ssl->ecdsakey)) {
	    rpmsslPrint(ssl, " failed\n");
	    goto exit;
	}

	/* create second key */
	if ((ssl->ecdsakey_bad = EC_KEY_new()) == NULL)
	    goto exit;
	ssl->group = EC_GROUP_new_by_curve_name(ssl->nid);
	if (ssl->group == NULL)
	    goto exit;
	if (EC_KEY_set_group(ssl->ecdsakey_bad, ssl->group) == 0)
	    goto exit;
	EC_GROUP_free(ssl->group);
	ssl->group = NULL;

	if (!EC_KEY_generate_key(ssl->ecdsakey_bad)) {
	    rpmsslPrint(ssl, " failed\n");
	    goto exit;
	}
	rpmsslPrint(ssl, ".");

	/* check key */
	if (!EC_KEY_check_key(ssl->ecdsakey)) {
	    rpmsslPrint(ssl, " failed\n");
	    goto exit;
	}
	rpmsslPrint(ssl, ".");

	/* create signature */
	ecsdasiglen = ECDSA_size(ssl->ecdsakey);
	if ((ecdsasig = OPENSSL_malloc(ecsdasiglen)) == NULL)
	    goto exit;
	if (!ECDSA_sign(0, digest, digestlen, ecdsasig, &ecsdasiglen, ssl->ecdsakey)) {
	    rpmsslPrint(ssl, " failed\n");
	    goto exit;
	}
	rpmsslPrint(ssl, ".");

	/* verify signature */
	if (ECDSA_verify(0, digest, digestlen, ecdsasig, ecsdasiglen, ssl->ecdsakey) != 1) {
	    rpmsslPrint(ssl, " failed\n");
	    goto exit;
	}
	rpmsslPrint(ssl, ".");

	/* verify signature with the wrong key */
	if (ECDSA_verify(0, digest, digestlen, ecdsasig, ecsdasiglen,
			 ssl->ecdsakey_bad) == 1) {
	    rpmsslPrint(ssl, " failed\n");
	    goto exit;
	}
	rpmsslPrint(ssl, ".");

	/* wrong digest */
	if (ECDSA_verify(0, digest_bad, sizeof(digest_bad), ecdsasig, ecsdasiglen,
			 ssl->ecdsakey) == 1) {
	    rpmsslPrint(ssl, " failed\n");
	    goto exit;
	}
	rpmsslPrint(ssl, ".");

	/* modify a single byte of the signature */
	offset = ecdsasig[10] % ecsdasiglen;
	dirt = ecdsasig[11];
	ecdsasig[offset] ^= dirt ? dirt : 1;
	if (ECDSA_verify(0, digest, digestlen, ecdsasig, ecsdasiglen, ssl->ecdsakey) == 1) {
	    rpmsslPrint(ssl, " failed\n");
	    goto exit;
	}
	rpmsslPrint(ssl, ".");

	rpmsslPrint(ssl, " ok\n");

	/* cleanup */
#ifndef	DYING	/* XXX watchout: ssl->curves */
	OPENSSL_free(ecdsasig);
	ecdsasig = NULL;
	EC_KEY_free(ssl->ecdsakey);
	ssl->ecdsakey = NULL;
	EC_KEY_free(ssl->ecdsakey_bad);
	ssl->ecdsakey_bad = NULL;
#else
	if (ecdsasig)
	    OPENSSL_free(ecdsasig);
	ecdsasig = NULL;
	pgpImplClean(ssl);
#endif
    }

    ret = 1;

exit:
#ifdef	DYING
    if (ssl->ecdsakey)
	EC_KEY_free(ssl->ecdsakey);
    if (ssl->ecdsakey_bad)
	EC_KEY_free(ssl->ecdsakey_bad);
    if (ecdsasig)
	OPENSSL_free(ecdsasig);
    if (ssl->curves)
	OPENSSL_free(ssl->curves);
#else
    if (ecdsasig)
	OPENSSL_free(ecdsasig);
    ecdsasig = NULL;
    pgpImplClean(ssl);
#endif

digest_bad = _free(digest_bad);
digestlen_bad = 0;
digest = _free(digest);
digestlen = 0;

    dig = pgpDigFree(dig);

    return ret;
}

/*==============================================================*/
static struct poptOption optionsTable[] = {
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options:"), NULL },

  POPT_AUTOHELP
  POPT_TABLEEND
};

int main(int argc, char *argv[])
{
    poptContext con = rpmioInit(argc, argv, optionsTable);
    rpmssl ssl;
    int ret = 1;	/* assume failure */

pgpImplVecs = &rpmsslImplVecs;
ssl = rpmsslNew(stdout);

    /* the tests */
    if (!rpmsslTests(ssl))
	goto err;
    if (!test_builtin(ssl))
	goto err;

    ret = 0;

err:
    if (ret) {
	rpmsslPrint(ssl, "\nECDSA test failed\n");
	ERR_print_errors(ssl->out);
    } else
	rpmsslPrint(ssl, "\nECDSA test passed\n");

ssl = rpmsslFree(ssl);

    con = rpmioFini(con);

    return ret;
}

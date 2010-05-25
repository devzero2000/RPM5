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

/*==============================================================*/

static int fbytes(unsigned char *buf, int num)
{
    static int fbytes_counter = 0;
    static const char *numbers[8] = {
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
		"40041670216363"
    };
    BIGNUM *tmp = NULL;
    int ret;

    if (fbytes_counter >= 8)
	return 0;
    tmp = BN_new();
    if (!tmp)
	return 0;
    if (!BN_dec2bn(&tmp, numbers[fbytes_counter])) {
	BN_free(tmp);
	return 0;
    }
    fbytes_counter++;
    ret = BN_bn2bin(tmp, buf);
    if (ret == 0 || ret != num)
	ret = 0;
    else
	ret = 1;
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

/* some tests from the X9.62 draft */
static int x9_62_test_internal(rpmssl ssl, int nid, const char *r_in,
			const char *s_in)
{
    const char message[] = "abc";
    unsigned char digest[20];
    unsigned int dgst_len = 0;
    EVP_MD_CTX md_ctx;
    EC_KEY *key = NULL;
    ECDSA_SIG *signature = NULL;
    BIGNUM *r = NULL;
    BIGNUM *s = NULL;
    int ret = 0;

    EVP_MD_CTX_init(&md_ctx);
    /* get the message digest */
    EVP_DigestInit(&md_ctx, EVP_ecdsa());
    EVP_DigestUpdate(&md_ctx, (const void *) message, 3);
    EVP_DigestFinal(&md_ctx, digest, &dgst_len);

    rpmsslPrint(ssl, "testing %s: ", OBJ_nid2sn(nid));
    /* create the key */
    if ((key = EC_KEY_new_by_curve_name(nid)) == NULL)
	goto exit;
    if (!EC_KEY_generate_key(key))
	goto exit;
    rpmsslPrint(ssl, ".");

    /* create the signature */
    signature = ECDSA_do_sign(digest, 20, key);
    if (signature == NULL)
	goto exit;
    rpmsslPrint(ssl, ".");

    /* compare the created signature with the expected signature */
    if ((r = BN_new()) == NULL || (s = BN_new()) == NULL)
	goto exit;
    if (!BN_dec2bn(&r, r_in) || !BN_dec2bn(&s, s_in))
	goto exit;
    if (BN_cmp(signature->r, r) || BN_cmp(signature->s, s))
	goto exit;
    rpmsslPrint(ssl, ".");

    /* verify the signature */
    if (ECDSA_do_verify(digest, 20, signature, key) != 1)
	goto exit;
    rpmsslPrint(ssl, ".");

    rpmsslPrint(ssl, " ok\n");
    ret = 1;

exit:
    if (!ret)
	rpmsslPrint(ssl, " failed\n");
    if (key)
	EC_KEY_free(key);
    if (signature)
	ECDSA_SIG_free(signature);
    if (r)
	BN_free(r);
    if (s)
	BN_free(s);
    EVP_MD_CTX_cleanup(&md_ctx);
    return ret;
}

static int x9_62_tests(rpmssl ssl)
{
    int ret = 0;

    rpmsslPrint(ssl, "some tests from X9.62:\n");

    /* set own rand method */
    if (!change_rand())
	goto exit;

    if (!x9_62_test_internal(ssl, NID_X9_62_prime192v1,
	     "3342403536405981729393488334694600415596881826869351677613",
	     "5735822328888155254683894997897571951568553642892029982342"))
	goto exit;
    if (!x9_62_test_internal(ssl, NID_X9_62_prime239v1,
	     "3086361431751678114926225473006680188549593787585317781474"
	     "62058306432176",
	     "3238135532097973577080787768312505059318910517550078427819"
	     "78505179448783"))
	goto exit;
    if (!x9_62_test_internal(ssl, NID_X9_62_c2tnb191v1,
	     "87194383164871543355722284926904419997237591535066528048",
	     "308992691965804947361541664549085895292153777025772063598"))
	goto exit;
    if (!x9_62_test_internal(ssl, NID_X9_62_c2tnb239v1,
	     "2159633321041961198501834003903461262881815148684178964245"
	     "5876922391552",
	     "1970303740007316867383349976549972270528498040721988191026"
	     "49413465737174"))
	goto exit;

    ret = 1;

exit:
    if (!restore_rand())
	ret = 0;
    return ret;
}

static int test_builtin(rpmssl ssl)
{
    EC_builtin_curve *curves = NULL;
    size_t crv_len = 0;
    size_t n = 0;
    EC_KEY *eckey = NULL;
    EC_KEY *wrong_eckey = NULL;
    EC_GROUP *group;
    unsigned char digest[20];
    unsigned char wrong_digest[20];
    unsigned char *signature = NULL;
    unsigned int sig_len;
    int ret = 0;
    int nid;

    /* fill digest values with some random data */
    if (!RAND_pseudo_bytes(digest, 20) ||
	!RAND_pseudo_bytes(wrong_digest, 20)) {
	rpmsslPrint(ssl, "ERROR: unable to get random data\n");
	goto exit;
    }

    /* create and verify a ecdsa signature with every availble curve
     * (with ) */
    rpmsslPrint(ssl, "\ntesting ECDSA_sign() and ECDSA_verify() with some internal curves:\n");

    /* get a list of all internal curves */
    crv_len = EC_get_builtin_curves(NULL, 0);

    curves = OPENSSL_malloc(sizeof(EC_builtin_curve) * crv_len);

    if (curves == NULL) {
	rpmsslPrint(ssl, "malloc error\n");
	goto exit;
    }

    if (!EC_get_builtin_curves(curves, crv_len)) {
	rpmsslPrint(ssl, "unable to get internal curves\n");
	goto exit;
    }

    /* now create and verify a signature for every curve */
    for (n = 0; n < crv_len; n++) {
	unsigned char offset;
	unsigned char dirt;

	nid = curves[n].nid;
	if (nid == NID_ipsec4)
	    continue;
	/* create new ecdsa key (== EC_KEY) */
	if ((eckey = EC_KEY_new()) == NULL)
	    goto exit;
	group = EC_GROUP_new_by_curve_name(nid);
	if (group == NULL)
	    goto exit;
	if (EC_KEY_set_group(eckey, group) == 0)
	    goto exit;
	EC_GROUP_free(group);
	if (EC_GROUP_get_degree(EC_KEY_get0_group(eckey)) < 160)
	    /* drop the curve */
	{
	    EC_KEY_free(eckey);
	    eckey = NULL;
	    continue;
	}
	rpmsslPrint(ssl, "%s: ", OBJ_nid2sn(nid));
	/* create key */
	if (!EC_KEY_generate_key(eckey)) {
	    rpmsslPrint(ssl, " failed\n");
	    goto exit;
	}
	/* create second key */
	if ((wrong_eckey = EC_KEY_new()) == NULL)
	    goto exit;
	group = EC_GROUP_new_by_curve_name(nid);
	if (group == NULL)
	    goto exit;
	if (EC_KEY_set_group(wrong_eckey, group) == 0)
	    goto exit;
	EC_GROUP_free(group);
	if (!EC_KEY_generate_key(wrong_eckey)) {
	    rpmsslPrint(ssl, " failed\n");
	    goto exit;
	}

	rpmsslPrint(ssl, ".");
	/* check key */
	if (!EC_KEY_check_key(eckey)) {
	    rpmsslPrint(ssl, " failed\n");
	    goto exit;
	}
	rpmsslPrint(ssl, ".");

	/* create signature */
	sig_len = ECDSA_size(eckey);
	if ((signature = OPENSSL_malloc(sig_len)) == NULL)
	    goto exit;
	if (!ECDSA_sign(0, digest, 20, signature, &sig_len, eckey)) {
	    rpmsslPrint(ssl, " failed\n");
	    goto exit;
	}
	rpmsslPrint(ssl, ".");

	/* verify signature */
	if (ECDSA_verify(0, digest, 20, signature, sig_len, eckey) != 1) {
	    rpmsslPrint(ssl, " failed\n");
	    goto exit;
	}
	rpmsslPrint(ssl, ".");

	/* verify signature with the wrong key */
	if (ECDSA_verify(0, digest, 20, signature, sig_len,
			 wrong_eckey) == 1) {
	    rpmsslPrint(ssl, " failed\n");
	    goto exit;
	}
	rpmsslPrint(ssl, ".");

	/* wrong digest */
	if (ECDSA_verify(0, wrong_digest, 20, signature, sig_len,
			 eckey) == 1) {
	    rpmsslPrint(ssl, " failed\n");
	    goto exit;
	}
	rpmsslPrint(ssl, ".");

	/* modify a single byte of the signature */
	offset = signature[10] % sig_len;
	dirt = signature[11];
	signature[offset] ^= dirt ? dirt : 1;
	if (ECDSA_verify(0, digest, 20, signature, sig_len, eckey) == 1) {
	    rpmsslPrint(ssl, " failed\n");
	    goto exit;
	}
	rpmsslPrint(ssl, ".");

	rpmsslPrint(ssl, " ok\n");
	/* cleanup */
	OPENSSL_free(signature);
	signature = NULL;
	EC_KEY_free(eckey);
	eckey = NULL;
	EC_KEY_free(wrong_eckey);
	wrong_eckey = NULL;
    }

    ret = 1;

exit:
    if (eckey)
	EC_KEY_free(eckey);
    if (wrong_eckey)
	EC_KEY_free(wrong_eckey);
    if (signature)
	OPENSSL_free(signature);
    if (curves)
	OPENSSL_free(curves);

    return ret;
}

/*==============================================================*/

int main(void)
{
    rpmssl ssl = rpmsslNew(stdout);
    int ret = 1;	/* assume failure */


    /* the tests */
    if (!x9_62_tests(ssl))
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

    return ret;
}

/** \ingroup rpmio signature
 * \file rpmio/tkey.c
 * Routines to handle RFC-2440 detached signatures.
 */

static int _debug = 0;
extern int _pgp_debug;
extern int _pgp_print;

#include "system.h"
#include <rpmio.h>

#define	_RPMPGP_INTERNAL
#define	_RPMBC_INTERNAL
#include <rpmbc.h>
#define	_RPMGC_INTERNAL
#include <rpmgc.h>
#define	_RPMNSS_INTERNAL
#include <rpmnss.h>
#define	_RPMSSL_INTERNAL
#include <rpmssl.h>

#include "debug.h"

static int doit(const char *sig, pgpDig dig, int printing)
{
    const char *s, *t;
    unsigned char * dec;
    size_t declen;
    char * enc;
    int rc;
    int i;

if (_debug)
fprintf(stderr, "*** sig is\n%s\n", sig);

    if ((rc = b64decode(sig, (void **)&dec, &declen)) != 0) {
	fprintf(stderr, "*** b64decode returns %d\n", rc);
	return rc;
    }
    rc = pgpPrtPkts(dec, declen, dig, printing);
    if (rc < 0) {
	fprintf(stderr, "*** pgpPrtPkts returns %d\n", rc);
	return rc;
    }

    if ((enc = b64encode(dec, declen)) == NULL) {
	fprintf(stderr, "*** b64encode failed\n");
	return rc;
    }
    dec = _free(dec);

if (_debug)
fprintf(stderr, "*** enc is\n%s\n", enc);

rc = 0;
for (i = 0, s = sig, t = enc; *s & *t; i++, s++, t++) {
    if (*s == '\n') s++;
    if (*t == '\n') t++;
    if (*s == *t) continue;
fprintf(stderr, "??? %5d %02x != %02x '%c' != '%c'\n", i, (*s & 0xff), (*t & 0xff), *s, *t);
    rc = 5;
}
    enc = _free(enc);

    return rc;
}

/* FIPS-186 test vectors. */
static const char * fips_p = "8df2a494492276aa3d25759bb06869cbeac0d83afb8d0cf7cbb8324f0d7882e5d0762fc5b7210eafc2e9adac32ab7aac49693dfbf83724c2ec0736ee31c80291";
static const char * fips_q = "c773218c737ec8ee993b4f2ded30f48edace915f";
static const char * fips_g = "626d027839ea0a13413163a55b4cb500299d5522956cefcb3bff10f399ce2c2e71cb9de5fa24babf58e5b79521925c9cc42e9f6f464b088cc572af53e6d78802";

static const char * fips_hm = "a9993e364706816aba3e25717850c26c9cd0d89d";

static const char * fips_y = "19131871d75b1612a819f29d78d1b0d7346f7aa77bb62a859bfd6c5675da9d212d3a36ef1672ef660b8c7c255cc0ec74858fba33f44c06699630a76b030ee333";

static const char * fips_r = "8bac1ab66410435cb7181f95b16ab97c92b341c0";
static const char * fips_s = "41e2345f1f56df2458f426d155b4ba2db6dcd8c8";

/* Secret key */
static const char * jbjSecretDSA =
"lQFvBDu6XHwRAwCTIHRgKeIlOFUIEZeJVYSrXn0eUrM5S8OF471tTc+IV7AwiXBR\n"
"zCFCan4lO1ipmoAipyN2A6ZX0HWOcWdYlWz2adxA7l8JNiZTzkemA562xwex2wLy\n"
"AQWVTtRN6jv0LccAoN4UWZkIvkT6tV918sEvDEggGARxAv9190RhrDq/GMqd+AHm\n"
"qWrRkrBRHDUBBL2fYEuU3gFekYrW5CDIN6s3Mcq/yUsvwHl7bwmoqbf2qabbyfnv\n"
"Y66ETOPKLcw67ggcptHXHcwlvpfJmHKpjK+ByzgauPXXbRAC+gKDjzXL0kAQxjmT\n"
"2D+16O4vI8Emlx2JVcGLlq/aWhspvQWIzN6PytA3iKZ6uzesrM7yXmqzgodZUsJh\n"
"1wwl/0K5OIJn/oD41UayU8RXNER8SzDYvDYsJymFRwE1s58lL/8DAwJUAllw1pdZ\n"
"WmBIoAvRiv7kE6hWfeCvZzdBVgrHYrp8ceUa3OdulGfYw/0sIzpEU0FfZmFjdG9y\n"
"OgAA30gJ4JMFKVfthnDCHHL+O8lNxykKBmrgVPLClue0KUplZmYgSm9obnNvbiAo\n"
"QVJTIE4zTlBRKSA8amJqQHJlZGhhdC5jb20+iFcEExECABcFAju6XHwFCwcKAwQD\n"
"FQMCAxYCAQIXgAAKCRCB0qVW2I6DmQU6AJ490bVWZuM4yCOh8MWj6qApCr1/gwCf\n"
"f3+QgXFXAeTyPtMmReyWxThABtE=\n"
;

/* Public key */
static const char * jbjPublicDSA =
"mQFCBDu6XHwRAwCTIHRgKeIlOFUIEZeJVYSrXn0eUrM5S8OF471tTc+IV7AwiXBR\n"
"zCFCan4lO1ipmoAipyN2A6ZX0HWOcWdYlWz2adxA7l8JNiZTzkemA562xwex2wLy\n"
"AQWVTtRN6jv0LccAoN4UWZkIvkT6tV918sEvDEggGARxAv9190RhrDq/GMqd+AHm\n"
"qWrRkrBRHDUBBL2fYEuU3gFekYrW5CDIN6s3Mcq/yUsvwHl7bwmoqbf2qabbyfnv\n"
"Y66ETOPKLcw67ggcptHXHcwlvpfJmHKpjK+ByzgauPXXbRAC+gKDjzXL0kAQxjmT\n"
"2D+16O4vI8Emlx2JVcGLlq/aWhspvQWIzN6PytA3iKZ6uzesrM7yXmqzgodZUsJh\n"
"1wwl/0K5OIJn/oD41UayU8RXNER8SzDYvDYsJymFRwE1s58lL7QpSmVmZiBKb2hu\n"
"c29uIChBUlMgTjNOUFEpIDxqYmpAcmVkaGF0LmNvbT6IVwQTEQIAFwUCO7pcfAUL\n"
"BwoDBAMVAwIDFgIBAheAAAoJEIHSpVbYjoOZBToAn3TXaAI+bhg51EeyaiFip/6W\n"
"OVwBAJ44rTtNsgZBQxXISjB64CWxl4VaWQ==\n"
;

/* Signature */
static const char * abcSignatureDSA =
"iD8DBQA7vII+gdKlVtiOg5kRAvg4AJ0fV3gDBADobAnK2HOkV88bfmFMEgCeNysO\n"
"nP3dWWJnp0Pnbor7pIob4Dk=\n"
;

int
main(int argc, char *argv[])
{
    pgpDig dig;
    rpmbc bc;
    int printing = 1;
    int rc;


    pgpImplVecs = &rpmbcImplVecs;

    dig = pgpDigNew(0);
    bc = dig->impl;

    mpbzero(&bc->p);	mpbsethex(&bc->p, fips_p);
    mpbzero(&bc->q);	mpbsethex(&bc->q, fips_q);
    mpnzero(&bc->g);	mpnsethex(&bc->g, fips_g);
    mpnzero(&bc->y);	mpnsethex(&bc->y, fips_y);
    mpnzero(&bc->r);	mpnsethex(&bc->r, fips_r);
    mpnzero(&bc->s);	mpnsethex(&bc->s, fips_s);
    mpnzero(&bc->hm);	mpnsethex(&bc->hm, fips_hm);

    rc = pgpImplVerifyDSA(dig);

fprintf(stderr, "=============================== DSA FIPS-186-1: rc %d\n", rc);

    dig = pgpDigFree(dig);

    pgpImplVecs = &rpmsslImplVecs;

    dig = pgpDigNew(0);
_pgp_debug = 1;
_pgp_print = 1;

fprintf(stderr, "=============================== GPG Secret Key\n");
    if ((rc = doit(jbjSecretDSA, dig, printing)) != 0)
	fprintf(stderr, "==> FAILED: rc %d\n", rc);

fprintf(stderr, "=============================== GPG Public Key\n");
    if ((rc = doit(jbjPublicDSA, dig, printing)) != 0)
	fprintf(stderr, "==> FAILED: rc %d\n", rc);

fprintf(stderr, "=============================== GPG Signature of \"abc\"\n");
    if ((rc = doit(abcSignatureDSA, dig, printing)) != 0)
	fprintf(stderr, "==> FAILED: rc %d\n", rc);

    {	DIGEST_CTX ctx = rpmDigestInit(PGPHASHALGO_SHA1, RPMDIGEST_NONE);
	pgpDigParams dsig = pgpGetSignature(dig);
	const char * txt = "abc";
	
	rpmDigestUpdate(ctx, txt, strlen(txt));
	rpmDigestUpdate(ctx, dsig->hash, dsig->hashlen);

	(void) pgpImplSetDSA(ctx, dig, dsig);
    }

    rc = pgpImplVerifyDSA(dig);
    
fprintf(stderr, "=============================== DSA verify: rc %d\n", rc);

    dig = pgpDigFree(dig);

    if (pgpImplVecs == &rpmnssImplVecs)
	NSS_Shutdown();

    return rc;
}

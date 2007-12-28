/** \ingroup rpmio signature
 * \file rpmio/tkey.c
 * Routines to handle RFC-2440 detached signatures.
 */

static int _debug = 1;
extern int _pgp_debug;
extern int _pgp_print;

#include "system.h"
#include <rpmio.h>
#include <rpmmacro.h>

#define	_RPMPGP_INTERNAL
#define	_RPMBC_INTERNAL
#include <rpmbc.h>
#define	_RPMGC_INTERNAL
#include <rpmgc.h>
#define	_RPMNSS_INTERNAL
#include <rpmnss.h>
#define	_RPMSSL_INTERNAL
#include <rpmssl.h>

#include "genpgp.h"

#include "debug.h"

static
int rpmCheckPgpSignatureOnFile(const char * fn, const char * sigfn,
		const char * pubfn, const char * pubfingerprint)
{
    static const char * plaintext = "This is the plaintext\n";
    pgpDig dig;
    pgpDigParams sigp;
    const char * _fn = NULL;
    const char * _sigfn = NULL;
    const unsigned char * sigpkt = NULL;
    size_t sigpktlen = 0;
    const char * _pubfn = NULL;
    const unsigned char * pubpkt = NULL;
    size_t pubpktlen = 0;
    DIGEST_CTX ctx = NULL;
    int printing = 0;
    int rc = 0;
    int xx;

if (_debug)
fprintf(stderr, "==> check(%s, %s, %s, %s)\n", fn, sigfn, pubfn, pubfingerprint);

    dig = pgpDigNew(0);

    _fn = rpmExpand(fn, NULL);

    _sigfn = rpmExpand(sigfn, NULL);
    xx = pgpReadPkts(_sigfn, &sigpkt, &sigpktlen);
    if (xx != PGPARMOR_SIGNATURE) {
fprintf(stderr, "==> pgpReadPkts(%s) SIG %p[%u] ret %d\n", _sigfn, sigpkt, sigpktlen, xx);
	goto exit;
    }
    xx = pgpPrtPkts((uint8_t *)sigpkt, sigpktlen, dig, printing);

    _pubfn = rpmExpand(pubfn, NULL);
    xx = pgpReadPkts(_pubfn, &pubpkt, &pubpktlen);
    if (xx != PGPARMOR_PUBKEY) {
fprintf(stderr, "==> pgpReadPkts(%s) PUB %p[%u] ret %d\n", _pubfn, pubpkt, pubpktlen, xx);
	goto exit;
    }
    xx = pgpPrtPkts((uint8_t *)pubpkt, pubpktlen, dig, printing);

    sigp = pgpGetSignature(dig);

    if (sigp->version != 3 && sigp->version != 4) {
fprintf(stderr, "==> unverifiable V%d\n", sigp->version);
	goto exit;
    }

    ctx = rpmDigestInit(sigp->hash_algo, RPMDIGEST_NONE);

    xx = rpmDigestUpdate(ctx, plaintext, strlen(plaintext));

    if (sigp->hash != NULL)
	xx = rpmDigestUpdate(ctx, sigp->hash, sigp->hashlen);
    if (sigp->version == 4) {
	uint32_t nb = sigp->hashlen;
	uint8_t trailer[6];
	nb = htonl(nb);
	trailer[0] = sigp->version;
	trailer[1] = 0xff;
	memcpy(trailer+2, &nb, sizeof(nb));
	xx = rpmDigestUpdate(ctx, trailer, sizeof(trailer));
    }

    switch(sigp->pubkey_algo) {
    default:
	xx = 1;
	break;
    case PGPPUBKEYALGO_DSA:
	xx = pgpImplSetDSA(ctx, dig, sigp);
	break;
    case PGPPUBKEYALGO_RSA:
	xx = pgpImplSetRSA(ctx, dig, sigp);
	break;
    }
    if (xx) {
fprintf(stderr, "==> can't load pubkey_algo(%u)\n", sigp->pubkey_algo);
	goto exit;
    }

    switch(sigp->pubkey_algo) {
    default:
	rc = 0;
	break;
    case PGPPUBKEYALGO_DSA:
	rc = pgpImplVerifyDSA(dig);
	break;
    case PGPPUBKEYALGO_RSA:
	rc = pgpImplVerifyRSA(dig);
	break;
    }

exit:
    pubpkt = _free(pubpkt);
    _pubfn = _free(_pubfn);
    sigpkt = _free(sigpkt);
    _sigfn = _free(_sigfn);
    _fn = _free(_fn);

    dig = pgpDigFree(dig);

if (_debug)
fprintf(stderr, "============================ verify: rc %d\n", rc);

    return rc;
}

static
int doit(const char * sigtype)
{
    int rc = 0;

    if (!strcmp("DSA", sigtype)) {
	rc = rpmCheckPgpSignatureOnFile("plaintext", DSAsig, DSApub, NULL);
    }
    if (!strcmp("RSA", sigtype)) {
	rc = rpmCheckPgpSignatureOnFile("plaintext", RSAsig, RSApub, NULL);
    }
    
    return rc;
}

int
main(int argc, char *argv[])
{
    int rc;

    pgpImplVecs = &rpmnssImplVecs;
_pgp_debug = 1;
_pgp_print = 1;

    rc = doit("DSA");

    rc = doit("RSA");

    if (pgpImplVecs == &rpmnssImplVecs)
	NSS_Shutdown();

    return rc;
}

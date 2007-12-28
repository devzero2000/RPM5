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
    const char * _fn = NULL;
    const char * _sigfn = NULL;
    const unsigned char * sigpkt = NULL;
    size_t sigpktlen = 0;
    const char * _pubfn = NULL;
    const unsigned char * pubpkt = NULL;
    size_t pubpktlen = 0;
    int rc = 0;
    int xx;

if (_debug)
fprintf(stderr, "==> check(%s, %s, %s, %s)\n", fn, sigfn, pubfn, pubfingerprint);

    _fn = rpmExpand(fn, NULL);

    _sigfn = rpmExpand(sigfn, NULL);
    xx = pgpReadPkts(_sigfn, &sigpkt, &sigpktlen);
    if (xx != PGPARMOR_SIGNATURE) {
fprintf(stderr, "==> pgpReadPkts(%s) SIG %p[%u] ret %d\n", _sigfn, sigpkt, sigpktlen, xx);
	goto exit;
    }

    _pubfn = rpmExpand(pubfn, NULL);
    xx = pgpReadPkts(_pubfn, &pubpkt, &pubpktlen);
    if (xx != PGPARMOR_PUBKEY) {
fprintf(stderr, "==> pgpReadPkts(%s) PUB %p[%u] ret %d\n", _pubfn, pubpkt, pubpktlen, xx);
	goto exit;
    }

    rc = 1;

exit:
    pubpkt = _free(pubpkt);
    _pubfn = _free(_pubfn);
    sigpkt = _free(sigpkt);
    _sigfn = _free(_sigfn);
    _fn = _free(_fn);
    return rc;
}

static
int doit(const char * sigtype)
{
    pgpDig dig;
    int rc = 0;

    dig = pgpDigNew(0);

    if (!strcmp("DSA", sigtype))
	rc = rpmCheckPgpSignatureOnFile("plaintext", DSAsig, DSApub, NULL);
    if (!strcmp("RSA", sigtype))
	rc = rpmCheckPgpSignatureOnFile("plaintext", RSAsig, RSApub, NULL);
    
if (_debug)
fprintf(stderr, "============================ %s verify: rc %d\n", sigtype, rc);

    dig = pgpDigFree(dig);

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

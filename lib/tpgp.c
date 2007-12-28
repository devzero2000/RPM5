/** \ingroup rpmio signature
 * \file rpmio/tkey.c
 * Routines to handle RFC-2440 detached signatures.
 */

static int _debug = 1;
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

#include "genpgp.h"

#include "debug.h"

static
int rpmCheckPgpSignatureOnFile(const char * fn, const char * sigfn,
		const char * pubfn, const char * pubfingerprint)
{

if (_debug)
fprintf(stderr, "==> check(%s, %s, %s, %s)\n", fn, sigfn, pubfn, pubfingerprint);
    return 1;
}

static
int doit(const char * sigtype)
{
    pgpDig dig;
    int printing = -1;
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

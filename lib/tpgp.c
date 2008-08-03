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
#if defined(WITH_NSS)
#define	_RPMNSS_INTERNAL
#include <rpmnss.h>
#endif
#if defined(WITH_SSL)
#define	_RPMSSL_INTERNAL
#include <rpmssl.h>
#endif

#include "genpgp.h"

#define	_RPMTS_INTERNAL		/* XXX ts->pkpkt */
#include <rpmcli.h>
#include <rpmns.h>

#include <rpmcb.h>
#include <rpmdb.h>
#include <rpmps.h>
#include <rpmts.h>

#include "debug.h"

static
rpmRC doit(rpmts ts, const char * sigtype)
{
    rpmRC rc = RPMRC_FAIL;

    if (!strcmp("DSA", sigtype)) {
	rc = rpmnsProbeSignature(ts, DSApem, NULL, DSApub, DSApubid, 0);
	rc = rpmnsProbeSignature(ts, plaintextfn, DSAsig, DSApub, DSApubid, 0);
	rc = rpmnsProbeSignature(ts, plaintextfn, DSAsig, DSApubpem, DSApubid, 0);
	rc = rpmnsProbeSignature(ts, plaintextfn, DSAsigpem, DSApub, DSApubid, 0);
	rc = rpmnsProbeSignature(ts, plaintextfn, DSAsigpem, DSApubpem, DSApubid, 0);
	rc = rpmnsProbeSignature(ts, plaintextfn, DSAsig, NULL, DSApubid, 0);
	rc = rpmnsProbeSignature(ts, plaintextfn, DSAsigpem, NULL, DSApubid, 0);
    }
    if (!strcmp("RSA", sigtype)) {
	rc = rpmnsProbeSignature(ts, RSApem, NULL, RSApub, RSApubid, 0);
#ifdef	NOTYET	/* XXX RSA key id's are funky. */
	rc = rpmnsProbeSignature(ts, plaintextfn, RSAsig, RSApub, RSApubid, 0);
	rc = rpmnsProbeSignature(ts, plaintextfn, RSAsigpem, RSApubpem, RSApubid, 0);
	rc = rpmnsProbeSignature(ts, plaintextfn, RSAsig, NULL, RSApubid, 0);
#endif
    }
    
    return rc;
}

static struct poptOption optionsTable[] = {
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
        N_("Common options:"),
        NULL },

   POPT_AUTOALIAS
   POPT_AUTOHELP
   POPT_TABLEEND
};      

int
main(int argc, char *argv[])
{
    poptContext optCon;
    rpmts ts = NULL;
    int rc;

_rpmns_debug = 1;
_pgp_debug = 1;
_pgp_print = 1;

    optCon = rpmcliInit(argc, argv, optionsTable);
    ts = rpmtsCreate();
    (void) rpmtsOpenDB(ts, O_RDONLY);

    rc = doit(ts, "DSA");

    rc = doit(ts, "RSA");

    ts = rpmtsFree(ts);

#if defined(WITH_NSS)
    if (pgpImplVecs == &rpmnssImplVecs)
	NSS_Shutdown();
#endif

    optCon = rpmcliFini(optCon);

    return rc;
}

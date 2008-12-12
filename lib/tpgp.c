/** \ingroup rpmio signature
 * \file rpmio/tkey.c
 * Routines to handle RFC-2440 detached signatures.
 */

extern int _pgp_debug;
extern int _pgp_print;

#include "system.h"
#include <rpmio.h>
#include <rpmcb.h>
#include <rpmmacro.h>
#include <poptIO.h>

#include <rpmtag.h>
#include <rpmdb.h>
#include <rpmns.h>

#include <rpmps.h>
#include <rpmts.h>

#include "genpgp.h"

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
	rc = rpmnsProbeSignature(ts, plaintextfn, RSAsig, RSApub, RSApubid, 0);
	rc = rpmnsProbeSignature(ts, plaintextfn, RSAsigpem, RSApubpem, RSApubid, 0);
	rc = rpmnsProbeSignature(ts, plaintextfn, RSAsigpem, RSApub, RSApubid, 0);
	rc = rpmnsProbeSignature(ts, plaintextfn, RSAsigpem, RSApubpem, RSApubid, 0);
	rc = rpmnsProbeSignature(ts, plaintextfn, RSAsig, NULL, RSApubid, 0);
	rc = rpmnsProbeSignature(ts, plaintextfn, RSAsigpem, NULL, RSApubid, 0);
    }
    
    return rc;
}

static struct poptOption optionsTable[] = {
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
        N_("Common options:"),
        NULL },

   POPT_AUTOALIAS
   POPT_AUTOHELP
   POPT_TABLEEND
};      

int
main(int argc, char *argv[])
{
    poptContext optCon = rpmioInit(argc, argv, optionsTable);
    rpmts ts = rpmtsCreate();
    int rc;

_rpmns_debug = 1;
_pgp_debug = 1;
_pgp_print = 1;

    (void) rpmtsOpenDB(ts, O_RDONLY);

    rc = doit(ts, "DSA");

    rc = doit(ts, "RSA");

    ts = rpmtsFree(ts);

    optCon = rpmioFini(optCon);

    return rc;
}

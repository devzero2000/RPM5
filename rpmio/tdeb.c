#include "system.h"

#include "rpmio_internal.h"
#include <rpmcb.h>
#include <rpmmacro.h>
#include <poptIO.h>

#include "../rpmdb/rpmtag.h"

typedef /*@abstract@*/ /*@refcounted@*/ struct rpmts_s * rpmts;
typedef /*@abstract@*/ struct rpmte_s * rpmte;

#define _RPMFI_INTERNAL
#include "../lib/rpmfi.h"

#include <iosm.h>                /* XXX CPIO_FOO/FSM_FOO constants */
#include <ar.h>
#include <cpio.h>
#include <tar.h>

typedef /*@abstract@*/ /*@refcounted@*/ struct rpmds_s * rpmds;
typedef struct rpmRelocation_s * rpmRelocation;

typedef /*@abstract@*/ /*@refcounted@*/ struct rpmdb_s * rpmdb;
typedef /*@abstract@*/ struct rpmdbMatchIterator_s * rpmdbMatchIterator;
typedef struct rpmPRCO_s * rpmPRCO;
typedef struct Spec_s * Spec;
#include "../lib/rpmts.h"

typedef /*@abstract@*/ /*@refcounted@*/ struct rpmpsm_s * rpmpsm;
#define	_RPMSQ_INTERNAL
#include "../lib/psm.h"

#include "debug.h"

/*@access FD_t @*/

/*@access rpmpsm @*/

/*@access IOSM_t @*/
/*@access rpmfi @*/

static int rpmIOSM(rpmts ts, const char * fn, int mapflags)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    rpmpsm psm;
    rpmfi fi;
    const char * ioflags;
    int fsmmode;
    int rc = 0;
    int xx;

fprintf(stderr, "--> rpmIOSM(%p, \"%s\", 0x%x)\n", ts, fn, mapflags);

    if (fn != NULL) {

	fi = rpmfiNew(ts, NULL, RPMTAG_BASENAMES, 0);
assert(fi != NULL);

	psm = rpmpsmNew(ts, NULL, fi);
assert(psm != NULL);

	ioflags = (mapflags & IOSM_PAYLOAD_CREATE) ? "w.ufdio" : "r.ufdio";
	psm->cfd = Fopen(fn, ioflags);
	if (psm->cfd != NULL && !Ferror(psm->cfd)) {

	    fi->mapflags |= mapflags;
	    fsmmode = (mapflags & IOSM_PAYLOAD_CREATE) ? IOSM_PKGBUILD : IOSM_PKGINSTALL;
	    rc = iosmSetup(fi->fsm, fsmmode, "ar", ts, fi,
			psm->cfd, NULL, &psm->failedFile);
	    (void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_UNCOMPRESS),
			fdstat_op(psm->cfd, FDSTAT_READ));
	    (void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_DIGEST),
		fdstat_op(psm->cfd, FDSTAT_DIGEST));
	    xx = iosmTeardown(fi->fsm);

	    xx = Fclose(psm->cfd);
	    psm->cfd = NULL;

	    if (rc != 0 || psm->failedFile != NULL) {
		const char * msg = iosmStrerror(rc);
		fprintf(stderr, "%s: %s: %s\n", fn, msg, psm->failedFile);
		msg = _free(msg);
	    }
	}

	psm = rpmpsmFree(psm);

	fi = rpmfiFree(fi);
    }

    return rc;
}

#define	RPMAR_LIST	(1 << 0)
#define	RPMAR_CREATE	(1 << 1)
#define	RPMAR_EXTRACT	(1 << 2)

/*@unchecked@*/
static int armode = RPMAR_LIST;
/*@unchecked@*/ /*@relnull@*/
static const char * arfn = NULL;

static int rpmarCreate(rpmts ts, /*@unused@*/ const char ** av)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, rpmGlobalMacroContext, fileSystem, internalState @*/
{
fprintf(stderr, "--> rpmarCreate(%p, %p) \"%s\"\n", ts, av, arfn);
    return rpmIOSM(ts, (arfn ? arfn : "-"), IOSM_PAYLOAD_CREATE);
}

static int rpmarExtract(rpmts ts, /*@unused@*/ const char ** av)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, rpmGlobalMacroContext, fileSystem, internalState @*/
{
fprintf(stderr, "--> rpmarExtract(%p, %p) \"%s\"\n", ts, av, arfn);
    return rpmIOSM(ts, (arfn ? arfn : "-"), IOSM_PAYLOAD_EXTRACT);
}

static int rpmarList(rpmts ts, /*@unused@*/ const char ** av)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, rpmGlobalMacroContext, fileSystem, internalState @*/
{
fprintf(stderr, "--> rpmarList(%p, %p) \"%s\"\n", ts, av, arfn);
    return rpmIOSM(ts, (arfn ? arfn : "-"), IOSM_PAYLOAD_LIST);
}

/*@unchecked@*/
static struct poptOption optionsTable[] = {
 { "create", 'c', POPT_ARG_VAL,		&armode, RPMAR_CREATE,
        N_("create archive.a from file arguments"), NULL },
 { "extract", 'x', POPT_ARG_VAL,	&armode, RPMAR_EXTRACT,
        N_("extract files from archive.a"), NULL },
 { "file", 'f', POPT_ARG_STRING,	&arfn, 0,
        N_("path to archive.a"), N_("FILE") },
 { "list", 't', POPT_ARG_VAL,		&armode, RPMAR_LIST,
        N_("list contents of archive.a"), NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
        N_("Common options for all modes and executables:"),
        NULL },

   POPT_AUTOALIAS
   POPT_AUTOHELP
   POPT_TABLEEND
};

int
main(int argc, char *const argv[])
	/*@globals _ar_debug, _iosm_debug,
		rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies _ar_debug, _iosm_debug,
		rpmGlobalMacroContext, fileSystem, internalState @*/
{
    poptContext optCon;
    rpmts ts = NULL;
    const char ** av = NULL;
    int ec = 0;

    optCon = rpmioInit(argc, argv, optionsTable);
    if (optCon == NULL)
        exit(EXIT_FAILURE);

    av = poptGetArgs(optCon);

    ts = rpmtsCreate();

_iosmNext = &iosmNext;
_iosm_debug = -1;
_ar_debug = 1;
rpmIncreaseVerbosity();
rpmIncreaseVerbosity();
    switch (armode) {
    default:
	break;
    case RPMAR_CREATE:
	ec = rpmarCreate(ts, av);
	break;
    case RPMAR_EXTRACT:
	ec = rpmarExtract(ts, av);
	break;
    case RPMAR_LIST:
	ec = rpmarList(ts, av);
	break;
    }

    ts = rpmtsFree(ts);

    optCon = rpmioFini(optCon);

    return ec;
}

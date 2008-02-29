#include "system.h"

#include "rpmio_internal.h"

#define _RPMFI_INTERNAL
#include <rpmcli.h>
#include <rpmts.h>

#include "cpio.h"		/* XXX cpioStrerror */
#include "ar.h"

#include "fsm.h"                /* XXX CPIO_FOO/FSM_FOO constants */

#define	_RPMSQ_INTERNAL
#include "psm.h"

#include <rpmfi.h>

#include "debug.h"

static int rpmFSM(rpmts ts, const char * fn, int mapflags)
{
    rpmpsm psm;
    rpmfi fi;
    const char * ioflags;
    int fsmmode;
    int rc = 0;
    int xx;

fprintf(stderr, "--> rpmFSM(%p, \"%s\", 0x%x)\n", ts, fn, mapflags);

    if (fn != NULL) {

	fi = rpmfiNew(ts, NULL, RPMTAG_BASENAMES, 0);

	fi->fsm->headerRead = &arHeaderRead;
	fi->fsm->headerWrite = &arHeaderWrite;
	fi->fsm->trailerWrite = &arTrailerWrite;
	fi->fsm->blksize = 1;	/* XXX AR_BLOCKSIZE? */

	psm = rpmpsmNew(ts, NULL, fi);

	ioflags = (mapflags & CPIO_PAYLOAD_CREATE) ? "w.ufdio" : "r.ufdio";
	psm->cfd = Fopen(fn, ioflags);
	if (psm->cfd != NULL && !Ferror(psm->cfd)) {
char buf[BUFSIZ];
Fread(buf, 1, sizeof(AR_MAGIC)-1, psm->cfd);

	    fi->mapflags |= mapflags;
	    fsmmode = (mapflags & CPIO_PAYLOAD_CREATE) ? FSM_PKGBUILD : FSM_PKGINSTALL;
	    rc = fsmSetup(fi->fsm, fsmmode, "ar", ts, fi,
			psm->cfd, NULL, &psm->failedFile);
	    (void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_UNCOMPRESS),
			fdstat_op(psm->cfd, FDSTAT_READ));
	    (void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_DIGEST),
		fdstat_op(psm->cfd, FDSTAT_DIGEST));
	    xx = fsmTeardown(fi->fsm);

	    xx = Fclose(psm->cfd);

	    if (rc != 0 || psm->failedFile != NULL) {
		const char * msg = cpioStrerror(rc);
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

static int armode = RPMAR_LIST;
static const char * arfn = NULL;

static int rpmarCreate(rpmts ts, QVA_t ia, const char ** av)
{
    return rpmFSM(ts, (arfn ? arfn : "-"), CPIO_PAYLOAD_CREATE);
}

static int rpmarExtract(rpmts ts, QVA_t ia, const char ** av)
{
    return rpmFSM(ts, (arfn ? arfn : "-"), CPIO_PAYLOAD_EXTRACT);
}

static int rpmarList(rpmts ts, QVA_t ia, const char ** av)
{
    return rpmFSM(ts, (arfn ? arfn : "-"), CPIO_PAYLOAD_LIST);
}

static struct poptOption optionsTable[] = {
 { "create", 'c', POPT_ARG_VAL,		&armode, RPMAR_CREATE,
        N_("create archive.a from file arguments"), NULL },
 { "extract", 'x', POPT_ARG_VAL,	&armode, RPMAR_EXTRACT,
        N_("extract files from archive.a"), NULL },
 { "file", 'f', POPT_ARG_STRING,	&arfn, 0,
        N_("path to archive.a"), N_("FILE") },
 { "list", 't', POPT_ARG_VAL,		&armode, RPMAR_LIST,
        N_("list contents of archive.a"), NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
        N_("Common options for all modes and executables:"),
        NULL },

   POPT_AUTOALIAS
   POPT_AUTOHELP
   POPT_TABLEEND
};

int
main(int argc, char *const argv[])
{
    poptContext optCon;
    rpmts ts = NULL;
    QVA_t ia = &rpmIArgs;
    const char ** av = NULL;
    int ec = 0;

    optCon = rpmcliInit(argc, argv, optionsTable);
    if (optCon == NULL)
        exit(EXIT_FAILURE);

    av = poptGetArgs(optCon);

    ts = rpmtsCreate();

_fsmNext = &fsmNext;
_fsm_debug = -1;
_ar_debug = 1;
rpmIncreaseVerbosity();
rpmIncreaseVerbosity();
    switch (armode) {
    default:
	break;
    case RPMAR_CREATE:
	ec = rpmarCreate(ts, ia, av);
	break;
    case RPMAR_EXTRACT:
	ec = rpmarExtract(ts, ia, av);
	break;
    case RPMAR_LIST:
	ec = rpmarList(ts, ia, av);
	break;
    }

    ts = rpmtsFree(ts);

    optCon = rpmcliFini(optCon);

    return ec;
}

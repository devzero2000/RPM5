#include "system.h"

#include "rpmio_internal.h"

#define _RPMFI_INTERNAL
#include <rpmcli.h>
#include <rpmts.h>

#include "cpio.h"		/* XXX cpioStrerror */
#include "tar.h"

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

	fi = rpmfiNew(ts, NULL, RPMTAG_BASENAMES, 1);

	fi->fsm->headerRead = &tarHeaderRead;
	fi->fsm->headerWrite = &tarHeaderWrite;
	fi->fsm->trailerWrite = &tarTrailerWrite;
	fi->fsm->blksize = TAR_BLOCK_SIZE;

	psm = rpmpsmNew(ts, NULL, fi);

	ioflags = (mapflags & CPIO_PAYLOAD_CREATE) ? "w.ufdio" : "r.ufdio";
	psm->cfd = Fopen(fn, ioflags);
	if (psm->cfd != NULL && !Ferror(psm->cfd)) {

	    fi->mapflags |= mapflags;
	    fsmmode = (mapflags & CPIO_PAYLOAD_CREATE) ? FSM_PKGBUILD : FSM_PKGINSTALL;
	    rc = fsmSetup(fi->fsm, fsmmode, "tar", ts, fi,
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

#define	RPMTAR_LIST	(1 << 0)
#define	RPMTAR_CREATE	(1 << 1)
#define	RPMTAR_EXTRACT	(1 << 2)

static int tarmode = RPMTAR_LIST;
static const char * tarfn = NULL;

static int rpmtarCreate(rpmts ts, QVA_t ia, const char ** av)
{
    return rpmFSM(ts, (tarfn ? tarfn : "-"), CPIO_PAYLOAD_CREATE);
}

static int rpmtarExtract(rpmts ts, QVA_t ia, const char ** av)
{
    return rpmFSM(ts, (tarfn ? tarfn : "-"), CPIO_PAYLOAD_EXTRACT);
}

static int rpmtarList(rpmts ts, QVA_t ia, const char ** av)
{
    return rpmFSM(ts, (tarfn ? tarfn : "-"), CPIO_PAYLOAD_LIST);
}

static struct poptOption optionsTable[] = {
 { "create", 'c', POPT_ARG_VAL,		&tarmode, RPMTAR_CREATE,
        N_("create archive.tar from file arguments"), NULL },
 { "extract", 'x', POPT_ARG_VAL,	&tarmode, RPMTAR_EXTRACT,
        N_("extract files from archive.tar"), NULL },
 { "file", 'f', POPT_ARG_STRING,	&tarfn, 0,
        N_("path to archive"), N_("FILE") },
 { "list", 't', POPT_ARG_VAL,		&tarmode, RPMTAR_LIST,
        N_("list contents of archive.tar"), NULL },

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

_fsm_debug = -1;
_tar_debug = 1;
rpmIncreaseVerbosity();
rpmIncreaseVerbosity();
    switch (tarmode) {
    default:
	break;
    case RPMTAR_CREATE:
	ec = rpmtarCreate(ts, ia, av);
	break;
    case RPMTAR_EXTRACT:
	ec = rpmtarExtract(ts, ia, av);
	break;
    case RPMTAR_LIST:
	ec = rpmtarList(ts, ia, av);
	break;
    }

    ts = rpmtsFree(ts);

    optCon = rpmcliFini(optCon);

    return ec;
}

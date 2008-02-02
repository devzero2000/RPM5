#include "system.h"
#include <fts.h>

#include <rpmio_internal.h>
#include <rpmmacro.h>
#include <rpmcb.h>
#include <mire.h>
#include <argv.h>
#include <popt.h>

#include "debug.h"

static const char * pattern = ".*\\.rpm$";
static miRE mire = NULL;

/*@unchecked@*/
static int _fts_debug = 0;

static int ndirs = 0;
static int nfiles = 0;

static int indent = 2;

static const char * ftsInfoStrings[] = {
    "UNKNOWN",
    "D",
    "DC",
    "DEFAULT",
    "DNR",
    "DOT",
    "DP",
    "ERR",
    "F",
    "INIT",
    "NS",
    "NSOK",
    "SL",
    "SLNONE",
    "W",
};

static const char * ftsInfoStr(int fts_info) {
    if (!(fts_info >= 1 && fts_info <= 14))
	fts_info = 0;
    return ftsInfoStrings[ fts_info ];
}

static int ftsPrint(FTS * ftsp, FTSENT * fts)
{
    int xx;

    if (_fts_debug)
	fprintf(stderr, "FTS_%s\t%*s %s\n", ftsInfoStr(fts->fts_info),
		indent * (fts->fts_level < 0 ? 0 : fts->fts_level), "",
		fts->fts_name);

    switch (fts->fts_info) {
    case FTS_D:		/* preorder directory */
	ndirs++;
	break;
    case FTS_DP:	/* postorder directory */
	break;
    case FTS_F:		/* regular file */
	if (mire)
	    xx = mireRegexec(mire, fts->fts_accpath);
	nfiles++;
	break;
    case FTS_NS:	/* stat(2) failed */
    case FTS_DNR:	/* unreadable directory */
    case FTS_ERR:	/* error; errno is set */
	break;
    case FTS_DC:	/* directory that causes cycles */
    case FTS_DEFAULT:	/* none of the above */
    case FTS_DOT:	/* dot or dot-dot */
    case FTS_INIT:	/* initialized only */
    case FTS_NSOK:	/* no stat(2) requested */
    case FTS_SL:	/* symbolic link */
    case FTS_SLNONE:	/* symbolic link without target */
    case FTS_W:		/* whiteout object */
    default:
	break;
    }

    return 0;
}

static int ftsOpts = 0;

static int ftsWalk(ARGV_t av)
{
    FTS * ftsp;
    FTSENT * fts;
    int xx;

    ndirs = nfiles = 0;
    ftsp = Fts_open((char *const *)av, ftsOpts, NULL);
    while((fts = Fts_read(ftsp)) != NULL)
	xx = ftsPrint(ftsp, fts);
    xx = Fts_close(ftsp);

fprintf(stderr, "===== (%d/%d) dirs/files in:\n", ndirs, nfiles);
    argvPrint(NULL, av, NULL);

    return 0;
}

static struct poptOption optionsTable[] = {
 { "ftsdebug", 'd', POPT_ARG_VAL,	&_fts_debug, -1,	NULL, NULL },

 { "comfollow", '\0', POPT_BIT_SET,	&ftsOpts, FTS_COMFOLLOW,
	N_("follow command line symlinks"), NULL },
 { "logical", '\0', POPT_BIT_SET,	&ftsOpts, FTS_LOGICAL,
	N_("logical walk"), NULL },
 { "nochdir", '\0', POPT_BIT_SET,	&ftsOpts, FTS_NOCHDIR,
	N_("don't change directories"), NULL },
 { "nostat", '\0', POPT_BIT_SET,	&ftsOpts, FTS_NOSTAT,
	N_("don't get stat info"), NULL },
 { "physical", '\0', POPT_BIT_SET,	&ftsOpts, FTS_PHYSICAL,
	N_("physical walk"), NULL },
 { "seedot", '\0', POPT_BIT_SET,	&ftsOpts, FTS_SEEDOT,
	N_("return dot and dot-dot"), NULL },
 { "xdev", '\0', POPT_BIT_SET,		&ftsOpts, FTS_XDEV,
	N_("don't cross devices"), NULL },
 { "whiteout", '\0', POPT_BIT_SET,	&ftsOpts, FTS_WHITEOUT,
	N_("return whiteout information"), NULL },

 { "ftpdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_ftp_debug, -1,
	N_("debug protocol data stream"), NULL},
 { "rpmiodebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmio_debug, -1,
	N_("debug rpmio I/O"), NULL},
 { "urldebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_url_debug, -1,
	N_("debug URL cache handling"), NULL},
 { "verbose", 'v', 0, 0, 'v',				NULL, NULL },
  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    poptContext optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
    ARGV_t av = NULL;
    int ac = 0;
    int rc;
    int xx;

    while ((rc = poptGetNextOpt(optCon)) > 0) {
	const char * optArg = poptGetOptArg(optCon);
	optArg = _free(optArg);
	switch (rc) {
	case 'v':
	    rpmIncreaseVerbosity();
	    /*@switchbreak@*/ break;
	default:
            /*@switchbreak@*/ break;
	}
    }

    if (ftsOpts == 0)
	ftsOpts = (FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOSTAT);

    if (_fts_debug) {
	rpmIncreaseVerbosity();
	rpmIncreaseVerbosity();
_av_debug = -1;
_ftp_debug = -1;
_dav_debug = 1;
_mire_debug = 1;
    }


    av = poptGetArgs(optCon);
    ac = argvCount(av);
    if (ac < 1) {
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }

    if (pattern) {
	mire = mireNew(RPMMIRE_REGEX, 0);
	if ((xx = mireRegcomp(mire, pattern)) != 0)
	    goto exit;;
    }

    ftsWalk(av);

exit:
    mire = mireFree(mire);

/*@i@*/ urlFreeCache();

    optCon = poptFreeContext(optCon);

    return 0;
}

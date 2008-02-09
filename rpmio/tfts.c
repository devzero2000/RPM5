#include "system.h"
#include <fts.h>

#include <rpmio_internal.h>
#include <poptIO.h>

#include "debug.h"

static int mireMode = RPMMIRE_REGEX;
static int mireTag = 0;
static const char * mirePattern = NULL;
static miRE mire = NULL;
static int mireNExecs = 0;
static int mireNMatches = 0;
static int mireNFails = 0;

static const char * mgFile = NULL;
static int mgFlags = 0;
static rpmmg mg = NULL;
static int mgNFiles = 0;
static int mgNMatches = 0;
static int mgNFails = 0;

static int _fts_debug = 0;

extern int _dav_nooptions;

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

    if (rpmIsDebug())
	fprintf(stderr, "FTS_%s\t%*s %s\n", ftsInfoStr(fts->fts_info),
		indent * (fts->fts_level < 0 ? 0 : fts->fts_level), "",
		fts->fts_name);

    switch (fts->fts_info) {
    case FTS_D:		/* preorder directory */
	ndirs++;
	if (rpmIsVerbose() && !rpmIsDebug()) {
	    size_t nb = strlen(fts->fts_path);
	    /* Add trailing '/' if not present. */
	    fprintf(stderr, "%s%s\n", fts->fts_path,
		(fts->fts_path[nb-1] != '/' ? "/" : ""));
	}
	break;
    case FTS_DP:	/* postorder directory */
	break;
    case FTS_F:		/* regular file */
	nfiles++;
	if (rpmIsVerbose() && !rpmIsDebug())
	    fprintf(stderr, "%s\n", fts->fts_path);
	if (mire) {
	    mireNExecs++;
	    xx = mireRegexec(mire, fts->fts_path);
	    if (xx == 0) {
		fprintf(stdout, " mire: %s\n", fts->fts_path);
		mireNMatches++;
	    } else {
		mireNFails++;
	    }
	}
	if (mg) {
	    const char * s = rpmmgFile(mg, fts->fts_accpath);
	    mgNFiles++;
	    if (s != NULL) {
		fprintf(stdout, "magic: %s: %s\n", fts->fts_path, s);
		mgNMatches++;
	    } else {
		mgNFails++;
	    }
	    s = _free(s);
	}
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

extern int rpmioFtsOpts;

static int ftsWalk(ARGV_t av)
{
    rpmop op = memset(alloca(sizeof(*op)), 0, sizeof(*op));
    FTS * ftsp;
    FTSENT * fts;
    int rc = 1;
    int xx;

    xx = rpmswEnter(op, 0);
    ndirs = nfiles = 0;
    if ((ftsp = Fts_open((char *const *)av, rpmioFtsOpts, NULL)) == NULL)
	goto exit;
    while((fts = Fts_read(ftsp)) != NULL)
	xx = ftsPrint(ftsp, fts);
    rc = Fts_close(ftsp);

exit:
    xx = rpmswExit(op, ndirs);

fprintf(stderr, "===== (%d/%d) dirs/files in:\n", ndirs, nfiles);
    argvPrint(NULL, av, NULL);
    rpmswPrint("fts:", op);

    return rc;
}

static struct poptOption optionsTable[] = {
 { "pattern", '\0', POPT_ARG_STRING,	&mirePattern, 0,	NULL, NULL },
 { "magic", '\0', POPT_ARG_STRING,	&mgFile, 0,	NULL, NULL },

 { "options", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_dav_nooptions, 0,
	N_("always send http OPTIONS"), NULL},
 { "nooptions", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_dav_nooptions, -1,
	N_("use cached http OPTIONS"), NULL},

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioFtsPoptTable, 0,
	N_("Options for Fts(3):"),
	NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"),
	NULL },

  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    poptContext optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
    ARGV_t av = NULL;
    int ac = 0;
    ARGV_t dav;
    const char * dn;
    int rc;

    if (rpmioFtsOpts == 0)
	rpmioFtsOpts = (FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOSTAT);

    if (_fts_debug) {
_av_debug = -1;
_dav_debug = -1;
_ftp_debug = -1;
_url_debug = -1;
_rpmio_debug = -1;
_rpmmg_debug = 1;
_mire_debug = 1;
    }

    dav = poptGetArgs(optCon);
    ac = argvCount(dav);
    if (ac < 1) {
	poptPrintUsage(optCon, stderr, 0);
	rc = 1;
	goto exit;
    }

    if (mirePattern) {
	mire = mireNew(mireMode, mireTag);
	if ((rc = mireRegcomp(mire, mirePattern)) != 0)
	    goto exit;;
    }

    if (mgFile) {
	mg = rpmmgNew(mgFile, mgFlags);
    }

    /* XXX Add pesky trailing '/' to http:// URI's */
    while ((dn = *dav++) != NULL) {
	size_t nb = strlen(dn);
	dn = rpmExpand(dn, (dn[nb-1] != '/' ? "/" : NULL), NULL);
	argvAdd(&av, dn);
	dn = _free(dn);
    }

    rc = ftsWalk(av);

exit:
    mg = rpmmgFree(mg);
    mire = mireFree(mire);

    av = argvFree(av);

    optCon = rpmioFini(optCon);

    return rc;
}

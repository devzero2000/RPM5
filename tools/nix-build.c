
#include "system.h"

#include <rpmiotypes.h>
#include <rpmio.h>
#include <rpmlog.h>
#include <rpmmacro.h>
#include <argv.h>
#include <poptIO.h>

#include "debug.h"


#define _KFB(n) (1U << (n))
#define _DFB(n) (_KFB(n) | 0x40000000)

#define F_ISSET(_nix, _FLAG) ((_nix)->flags & ((RPMNIX_FLAGS_##_FLAG) & ~0x40000000))

/**
 * Bit field enum for rpmdigest CLI options.
 */
enum nixFlags_e {
    RPMNIX_FLAGS_NONE		= 0,
    RPMNIX_FLAGS_ADDDRVLINK	= _DFB(0),	/*    --add-drv-link */
    RPMNIX_FLAGS_NOOUTLINK	= _DFB(1),	/* -o,--no-out-link */
    RPMNIX_FLAGS_DRYRUN		= _DFB(2),	/*    --dry-run */
};

/**
 */
typedef struct rpmnix_s * rpmnix;

/**
 */
struct rpmnix_s {
    enum nixFlags_e flags;	/*!< rpmnix control bits. */

    const char * outLink;
    const char * drvLink;

    const char ** instArgs;
    const char ** buildArgs;
    const char ** exprs;
};

/**
 */
static struct rpmnix_s _nix = {
	.flags = RPMNIX_FLAGS_NOOUTLINK
};

/*==============================================================*/

static const char _NIX_BIN_DIR[] = "NIX_BIN_DIR";
static const char _build_tmp[] = ".nix-build-tmp-";
static const char _default_nix[] = "./default.nix";
static const char * binDir = "/usr/bin";

static int verbose = 0;

static int rpmnixInstantiate(rpmnix nix, const char * expr, ARGV_t * drvPathsP)
	/*@*/
{
    ARGV_t argv = NULL;
    const char * argv0 = rpmGetPath(binDir, "/nix-instantiate", NULL);
    const char * cmd;
    int rc = 1;		/* assume failure */
    int xx;

assert(drvPathsP);
    *drvPathsP = NULL;

argvPrint(__FUNCTION__, nix->instArgs, NULL);

    /* Construct the nix-instantiate command to run. */
    xx = argvAdd(&argv, argv0);
    argv0 = _free(argv0);

    xx = argvAdd(&argv, "--add-root");
    xx = argvAdd(&argv, nix->drvLink);
    xx = argvAdd(&argv, "--indirect");
    xx = argvAppend(&argv, nix->instArgs);
    xx = argvAdd(&argv, expr);
    cmd = argvJoin(argv, ' ');

    /* Chomp nix-instantiate spewage into *drvPathsP */
    argv0 = rpmExpand("%(", cmd, ")", NULL);
    xx = argvSplit(drvPathsP, argv0, NULL);
    argv0 = _free(argv0);
    rc = 0;

    cmd = _free(cmd);
    argv = argvFree(argv);

    return rc;
}

static int rpmnixStore(rpmnix nix, ARGV_t drvPaths, ARGV_t * outPathsP)
	/*@*/
{
    ARGV_t argv = NULL;
    const char * argv0 = rpmGetPath(binDir, "/nix-store", NULL);
    const char * cmd;
    int rc = 1;		/* assume failure */
    int xx;

assert(outPathsP);
    *outPathsP = NULL;

    /* Construct the nix-store command to run. */
    xx = argvAdd(&argv, argv0);
    argv0 = _free(argv0);

    xx = argvAdd(&argv, "--add-root");
    xx = argvAdd(&argv, nix->outLink);
    xx = argvAdd(&argv, "--indirect");
    xx = argvAdd(&argv, "-rv");
    xx = argvAppend(&argv, nix->buildArgs);
    xx = argvAppend(&argv, drvPaths);
    cmd = argvJoin(argv, ' ');

    /* Chomp nix-store spewage into *outPathsP */
    argv0 = rpmExpand("%(", cmd, ")", NULL);
    xx = argvSplit(outPathsP, argv0, NULL);
    argv0 = _free(argv0);
    rc = 0;

    cmd = _free(cmd);
    argv = argvFree(argv);

    return rc;
}

#define	_BASE	0x40000000
enum {
#ifdef	DYING
    NIX_ADD_DRV_LINK	= _BASE + 0,	/*    --add-drv-link */
    NIX_DRV_LINK	= _BASE + 1,	/*    --drv-link NAME */
    NIX_NO_OUT_LINK	= _BASE + 2,	/* -o,--no-out-link */
#endif
    NIX_ATTR		= _BASE + 3,	/*    --attr ATTR */

    NIX_ARG		= _BASE + 4,	/*    --arg ARG1 ARG2 */
    NIX_LOG_TYPE	= _BASE + 5,	/*    --log-type TYPE */
    NIX_OPTION		= _BASE + 6,	/*    --option OPT1 OPT2 */

    NIX_MAX_JOBS	= _BASE + 7,	/* -j,--max-jobs JOBS */
    NIX_DRY_RUN		= _BASE + 8,	/*X   --dry-run */
    NIX_SHOW_TRACE	= _BASE + 9,	/*X   --show-trace */
    NIX_VERBOSE		= _BASE + 10,	/* -v,--verbose */

/* XXX from both nix-instatiate and nix-store: */
    NIX_ADD_ROOT	= _BASE + 20,	/*X   --add-root */
    NIX_VERSION		= _BASE + 21,	/*    --version */

/* XXX from nix-instatiate: */
    NIX_EVAL_ONLY	= _BASE + 22,	/*    --eval-only */
    NIX_PARSE_ONLY	= _BASE + 23,	/*    --parse-only */
    NIX_XML		= _BASE + 24,	/*    --xml */
    NIX_STRICT		= _BASE + 25,	/*    --strict */

/* XXX from nix-store: */
    NIX_REALISE		= _BASE + 30,	/* -r,--realise */
    NIX_ADD		= _BASE + 31,	/* -A,--add */
    NIX_DELETE		= _BASE + 32,	/*    --delete */

    NIX_READ_LOG	= _BASE + 33,	/* -l,--read-log */
    NIX_REGISTER_VALIDITY = _BASE + 34,	/*    --register-validity */
    NIX_CHECK_VALIDITY	= _BASE + 35,	/*    --check-validity */

    NIX_DUMP		= _BASE + 36,	/*    --dump */
    NIX_RESTORE		= _BASE + 37,	/*    --restore */
    NIX_EXPORT		= _BASE + 38,	/*    --export */
    NIX_IMPORT		= _BASE + 39,	/*    --import */
    NIX_VERIFY		= _BASE + 40,	/*    --verify */
    NIX_OPTIMISE	= _BASE + 41,	/*    --optimise */

    NIX_QUERY		= _BASE + 256,	/* -q,--query */
    NIX_QUERY_OUTPUTS	= _BASE + 256+1,	/*    --outputs */
    NIX_QUERY_REQUISITES = _BASE + 256+2,	/*    --requisites */
    NIX_QUERY_REFERENCES = _BASE + 256+3,	/*    --references */
    NIX_QUERY_REFERRERS = _BASE + 256+4,	/*    --referrers */
    NIX_QUERY_REFERRERS_CLOSURE = _BASE+256+5,	/*    --referrers-closure */
    NIX_QUERY_TREE	= _BASE + 256+6,	/*    --tree */
    NIX_QUERY_GRAPH	= _BASE + 256+7,	/*    --graph */
    NIX_QUERY_HASH	= _BASE + 256+8,	/*    --hash */
    NIX_QUERY_ROOTS	= _BASE + 256+9,	/*    --roots */
    NIX_QUERY_USE_OUTPUT = _BASE + 256+10,	/*    --use-output */
    NIX_QUERY_FORCE_REALISE = _BASE + 256+11,	/*    --force-realise */
    NIX_QUERY_INCLUDE_OUTPUTS = _BASE + 256+12,	/*    --include-outputs */

    NIX_GC		= _BASE + 512,	/*    --gc */
    NIX_GC_PRINT_ROOTS	= _BASE + 512+1,	/*    --print-roots */
    NIX_GC_PRINT_LIVE	= _BASE + 512+2,	/*    --print-live */
    NIX_GC_PRINT_DEAD	= _BASE + 512+3,	/*    --print-dead */
    NIX_GC_DELETE	= _BASE + 512+4,	/*    --delete */

};

static void nixInstantiateArgCallback(poptContext con,
                /*@unused@*/ enum poptCallbackReason reason,
                const struct poptOption * opt, const char * arg,
                /*@unused@*/ void * data)
	/*@*/
{
    rpmnix nix = &_nix;
    const char * s = NULL;
    int xx;

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
    case NIX_ATTR:			/*    --attr ATTR */
    case NIX_LOG_TYPE:			/*    --log-type TYPE */
	s = rpmExpand("--", opt->longName, NULL);
	xx = argvAdd(&nix->instArgs, s);
	xx = argvAdd(&nix->instArgs, arg);
	s = _free(s);
	break;
    case NIX_ARG:			/*    --arg ARG1 ARG2 */
#ifdef	NOTYET	/* XXX needs next 2 args */
	xx = argvAdd(&nix->instArgs, arg1);
	xx = argvAdd(&nix->instArgs, arg2);
#endif
	break;
    case NIX_OPTION:			/*    --option OPT1 OPT2 */
#ifdef	NOTYET	/* XXX needs next 2 args */
	xx = argvAdd(&nix->instArgs, opt1);
	xx = argvAdd(&nix->instArgs, opt2);
#endif
	break;
    case NIX_VERBOSE:			/* -v,--verbose */
	xx = argvAdd(&nix->instArgs, "-v");
	break;

    case NIX_SHOW_TRACE:		/*    --show-trace */

    case NIX_ADD_ROOT:			/*    --add-root */
    case NIX_VERSION:			/*    --version */

    case NIX_EVAL_ONLY:			/*    --eval-only */
    case NIX_PARSE_ONLY:		/*    --parse-only */
    case NIX_XML:			/*    --xml */
    case NIX_STRICT:			/*    --strict */
	s = rpmExpand("--", opt->longName, NULL);
	xx = argvAdd(&nix->instArgs, s);
	s = _free(s);
	break;
    default:
	fprintf(stderr, _("%s: Unknown callback(0x%x)\n"), __FUNCTION__, (unsigned) opt->val);
	poptPrintUsage(con, stderr, 0);
	/*@-exitarg@*/ exit(2); /*@=exitarg@*/
	/*@notreached@*/ break;
    }
}

static struct poptOption nixInstantiateOptions[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
	nixInstantiateArgCallback, 0, NULL, NULL },
/*@=type@*/

 { "eval-only", '\0', POPT_ARG_NONE,			0, NIX_EVAL_ONLY,
	N_("evaluate and print resulting term; do not instantiate"), NULL },
 { "parse-only", '\0', POPT_ARG_NONE,			0, NIX_PARSE_ONLY,
	N_("parse and print abstract syntax tree"), NULL },
 { "attr", 'A', POPT_ARG_STRING,			0, NIX_ATTR,
	N_("select a specific ATTR from the Nix expression"), N_("ATTR") },
 { "add-root", '\0', POPT_ARG_NONE,			0, NIX_ADD_ROOT,
	N_("add garbage collector roots for the result"), NULL },

 { "xml", '\0', POPT_ARG_NONE,				0, NIX_XML,
	N_("print an XML representation of the abstract syntax tree"), NULL },
 { "strict", '\0', POPT_ARG_NONE,			0, NIX_STRICT,
	N_("compute attributes and list elements, rather than being lazy"), NULL },

 { "log-type", '\0', POPT_ARG_STRING,			0, NIX_LOG_TYPE,
	N_("FIXME"), N_("TYPE") },
/* XXX needs next 2 args */
 { "arg", '\0', POPT_ARG_NONE,				0, NIX_ARG,
	N_("FIXME"), N_("ARG1 ARG2") },
 { "argstr", '\0', POPT_ARG_NONE|POPT_ARGFLAG_DOC_HIDDEN, 0, NIX_ARG,
	N_("FIXME"), N_("ARG1 ARG2") },
/* XXX needs next 2 args */
 { "option", '\0', POPT_ARG_NONE,			0, NIX_OPTION,
	N_("FIXME"), N_("OPT1 OPT2") },
 { "show-trace", '\0', POPT_ARG_NONE,			0, NIX_SHOW_TRACE,
	N_("FIXME"), NULL },

 { "verbose", 'v', POPT_ARG_NONE,			0, NIX_VERBOSE,
	N_("verbose operation (may be repeated)"), NULL },

 { "version", '\0', POPT_ARG_NONE,			0, NIX_VERSION,
	N_("output version information"), NULL },

  POPT_TABLEEND
};

static void nixStoreArgCallback(poptContext con,
                /*@unused@*/ enum poptCallbackReason reason,
                const struct poptOption * opt, const char * arg,
                /*@unused@*/ void * data)
	/*@*/
{
    rpmnix nix = &_nix;
    const char * s = NULL;
    int xx;

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
    case NIX_LOG_TYPE:			/*    --log-type TYPE */
    case NIX_MAX_JOBS:			/*    --max-jobs JOBS */
	s = rpmExpand("--", opt->longName, NULL);
	xx = argvAdd(&nix->buildArgs, s);
	xx = argvAdd(&nix->buildArgs, arg);
	s = _free(s);
	break;
    case NIX_OPTION:			/*    --option OPT1 OPT2 */
#ifdef	NOTYET	/* XXX needs next 2 args */
	xx = argvAdd(&nix->buildArgs, opt1);
	xx = argvAdd(&nix->buildArgs, opt2);
#endif
	break;

    case NIX_VERBOSE:			/* -v,--verbose */
	xx = argvAdd(&nix->buildArgs, "-v");
	break;
    case NIX_DRY_RUN:			/*    --dry-run */
    case NIX_ADD_ROOT:			/*    --add-root */
    case NIX_VERSION:			/*    --version */
    case NIX_REALISE:			/* -r,--realise */
    case NIX_ADD:			/* -A,--add */
    case NIX_DELETE:			/*    --delete */
    case NIX_READ_LOG:			/* -l,--read-log */
    case NIX_REGISTER_VALIDITY:		/*    --register-validity */
    case NIX_CHECK_VALIDITY:		/*    --check-validity */
    case NIX_DUMP:			/*    --dump */
    case NIX_RESTORE:			/*    --restore */
    case NIX_EXPORT:			/*    --export */
    case NIX_IMPORT:			/*    --import */
    case NIX_VERIFY:			/*    --verify */
    case NIX_OPTIMISE:			/*    --optimise */
    case NIX_QUERY:			/* -q,--query */
    case NIX_QUERY_OUTPUTS:		/*    --outputs */
    case NIX_QUERY_REQUISITES:		/*    --requisites */
    case NIX_QUERY_REFERENCES:		/*    --references */
    case NIX_QUERY_REFERRERS:		/*    --referrers */
    case NIX_QUERY_REFERRERS_CLOSURE:	/*    --referrers-closure */
    case NIX_QUERY_TREE:		/*    --tree */
    case NIX_QUERY_GRAPH:		/*    --graph */
    case NIX_QUERY_HASH:		/*    --hash */
    case NIX_QUERY_ROOTS:		/*    --roots */
    case NIX_QUERY_USE_OUTPUT:		/*    --use-output */
    case NIX_QUERY_FORCE_REALISE:	/*    --force-realise */
    case NIX_QUERY_INCLUDE_OUTPUTS:	/*    --include-outputs */
    case NIX_GC:			/*    --gc */
    case NIX_GC_PRINT_ROOTS:		/*    --print-roots */
    case NIX_GC_PRINT_LIVE:		/*    --print-live */
    case NIX_GC_PRINT_DEAD:		/*    --print-dead */
    case NIX_GC_DELETE:			/*    --delete */
	s = rpmExpand("--", opt->longName, NULL);
	xx = argvAdd(&nix->buildArgs, s);
	s = _free(s);
	break;

    default:
	fprintf(stderr, _("%s: Unknown callback(0x%x)\n"), __FUNCTION__, (unsigned) opt->val);
	poptPrintUsage(con, stderr, 0);
	/*@-exitarg@*/ exit(2); /*@=exitarg@*/
	/*@notreached@*/ break;
    }
}

static struct poptOption nixStoreOptions[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
	nixStoreArgCallback, 0, NULL, NULL },
/*@=type@*/

 { "log-type", '\0', POPT_ARG_STRING,			0, NIX_LOG_TYPE,
	N_("FIXME"), N_("TYPE") },
/* XXX needs next 2 args */
 { "option", '\0', POPT_ARG_NONE,			0, NIX_OPTION,
	N_("FIXME"), N_("OPT1 OPT2") },
 { "max-jobs", 'j', POPT_ARG_STRING,			0, NIX_MAX_JOBS,
	N_("FIXME"), N_("JOBS") },

 { "verbose", 'v', POPT_ARG_NONE,			0, NIX_VERBOSE,
	N_("verbose operation (may be repeated)"), NULL },
 { "dry-run", '\0', POPT_ARG_NONE,			0, NIX_DRY_RUN,
	N_("FIXME"), NULL },

 { "add-root", '\0', POPT_ARG_NONE,			0, NIX_ADD_ROOT,
	N_("add garbage collector roots for the result"), NULL },
 { "version", '\0', POPT_ARG_NONE,			0, NIX_VERSION,
	N_("output version information"), NULL },

 { "realise", 'r', POPT_ARG_NONE,			0, NIX_REALISE,
	N_("ensure path validity; if a derivation, ensure the validity of the outputs"), NULL },
/* XXX conflict with -A,--attr otherwise -A,--add */
 { "add", '\0', POPT_ARG_NONE,			0, NIX_ADD,
	N_("copy a path to the Nix store"), NULL },
 { "delete", '\0', POPT_ARG_NONE,			0, NIX_DELETE,
	N_("safely delete paths from the Nix store"), NULL },
 { "read-log", 'l', POPT_ARG_NONE,			0, NIX_READ_LOG,
	N_("print build log of given store paths"), NULL },
 { "register-validity", '\0', POPT_ARG_NONE,			0, NIX_REGISTER_VALIDITY,
	N_("register path validity (dangerous!)"), NULL },
 { "check-validity", '\0', POPT_ARG_NONE,			0, NIX_CHECK_VALIDITY,
	N_("check path validity"), NULL },
 { "dump", '\0', POPT_ARG_NONE,			0, NIX_DUMP,
	N_("dump a path as a Nix archive, forgetting dependencies"), NULL },
 { "restore", '\0', POPT_ARG_NONE,			0, NIX_RESTORE,
	N_("restore a path from a Nix archive, without registering validity"), NULL },
 { "export", '\0', POPT_ARG_NONE,			0, NIX_EXPORT,
	N_("export a path as a Nix archive, marking dependencies"), NULL },
 { "import", '\0', POPT_ARG_NONE,			0, NIX_IMPORT,
	N_("import a path from a Nix archive, and register as valid"), NULL },
 { "verify", '\0', POPT_ARG_NONE,			0, NIX_VERIFY,
	N_("verify Nix structures"), NULL },
 { "optimise", '\0', POPT_ARG_NONE,			0, NIX_OPTIMISE,
	N_("optimise the Nix store by hard-linking identical files"), NULL },
 { "query", 'q', POPT_ARG_NONE,			0, NIX_QUERY,
	N_("query information"), NULL },
 { "outputs", '\0', POPT_ARG_NONE,			0, NIX_QUERY_OUTPUTS,
	N_("query the output paths of a Nix derivation (default)"), NULL },
 { "requisites", '\0', POPT_ARG_NONE,			0, NIX_QUERY_REQUISITES,
	N_("print all paths necessary to realise the path"), NULL },
 { "references", '\0', POPT_ARG_NONE,			0, NIX_QUERY_REFERENCES,
	N_("print all paths referenced by the path"), NULL },
 { "referrers", '\0', POPT_ARG_NONE,			0, NIX_QUERY_REFERRERS,
	N_("print all paths directly refering to the path"), NULL },
 { "referrers-closure", '\0', POPT_ARG_NONE,			0, NIX_QUERY_REFERRERS_CLOSURE,
	N_("print all paths (in)directly refering to the path"), NULL },
 { "tree", '\0', POPT_ARG_NONE,			0, NIX_QUERY_TREE,
	N_("print a tree showing the dependency graph of the path"), NULL },
 { "graph", '\0', POPT_ARG_NONE,			0, NIX_QUERY_GRAPH,
	N_("print a dot graph rooted at given path"), NULL },
 { "hash", '\0', POPT_ARG_NONE,			0, NIX_QUERY_HASH,
	N_("print the SHA-256 hash of the contents of the path"), NULL },
 { "roots", '\0', POPT_ARG_NONE,			0, NIX_QUERY_ROOTS,
	N_("print the garbage collector roots that point to the path"), NULL },
 { "use-output", '\0', POPT_ARG_NONE,			0, NIX_QUERY_USE_OUTPUT,
	N_("perform query on output of derivation, not derivation itself"), NULL },
 { "force-realise", '\0', POPT_ARG_NONE,			0, NIX_QUERY_FORCE_REALISE,
	N_("realise the path before performing the query"), NULL },
 { "include-outputs", '\0', POPT_ARG_NONE,			0, NIX_QUERY_INCLUDE_OUTPUTS,
	N_("in `-R' on a derivation, include requisites of outputs"), NULL },
 { "gc", '\0', POPT_ARG_NONE,			0, NIX_GC,
	N_("run the garbage collector"), NULL },
 { "print-roots", '\0', POPT_ARG_NONE,			0, NIX_GC_PRINT_ROOTS,
	N_("print GC roots and exit"), NULL },
 { "print-live", '\0', POPT_ARG_NONE,			0, NIX_GC_PRINT_LIVE,
	N_("print live paths and exit"), NULL },
 { "print-dead", '\0', POPT_ARG_NONE,			0, NIX_GC_PRINT_DEAD,
	N_("print dead paths and exit"), NULL },
/* XXX conflict with --delete */
 { "gcdelete", '\0', POPT_ARG_NONE,			0, NIX_GC_DELETE,
	N_("delete dead paths (default)"), NULL },

  POPT_TABLEEND
};

static void nixBuildArgCallback(poptContext con,
                /*@unused@*/ enum poptCallbackReason reason,
                const struct poptOption * opt, const char * arg,
                /*@unused@*/ void * data)
	/*@*/
{
    rpmnix nix = &_nix;

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
/* XXX must be callback to continue through tables. */
    case NIX_DRY_RUN:			/*    --dry-run */
	nix->flags |= RPMNIX_FLAGS_DRYRUN;
	break;
    case NIX_VERBOSE:			/* -v,--verbose */
	verbose++;
	break;

    /* XXX Collect and filter unknown options for pass thru. */
    default:
	fprintf(stderr, _("%s: Unknown callback(0x%x)\n"), __FUNCTION__, (unsigned) opt->val);
	poptPrintUsage(con, stderr, 0);
	/*@-exitarg@*/ exit(2); /*@=exitarg@*/
	/*@notreached@*/ break;
    }
}

static struct poptOption nixBuildOptions[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
	nixBuildArgCallback, 0, NULL, NULL },
/*@=type@*/

 { "add-drv-link", '\0', POPT_BIT_SET,	&_nix.flags, RPMNIX_FLAGS_ADDDRVLINK,
	N_("create a symlink `derivation' to the store derivation"), NULL },
 { "no-out-link", 'o', POPT_BIT_CLR,	&_nix.flags, RPMNIX_FLAGS_NOOUTLINK,
	N_("do not create the `result' symlink"), NULL },
 { "no-link", '\0', POPT_BIT_CLR|POPT_ARGFLAG_DOC_HIDDEN,	&_nix.flags, RPMNIX_FLAGS_NOOUTLINK,
	N_("do not create the `result' symlink"), NULL },

 { "drv-link", '\0', POPT_ARG_STRING,	&_nix.drvLink, 0,
	N_("create symlink NAME instead of `derivation'"), N_("NAME") },
 { "out-link", 'o', POPT_ARG_STRING,	&_nix.outLink, 0,
	N_("create symlink NAME instead of `result'"), N_("NAME") },

/* XXX must be callback to continue through tables. */
 { "dry-run", '\0', POPT_ARG_NONE,			0, NIX_DRY_RUN,
	N_("FIXME"), NULL },
 { "verbose", 'v', POPT_ARG_NONE,			0, NIX_VERBOSE,
	N_("verbose operation (may be repeated)"), NULL },

#ifdef	NOTYET
 { "version", '\0', POPT_ARG_NONE,			0, NIX_VERSION,
	N_("output version information"), NULL },
#endif

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, nixInstantiateOptions, 0,
        N_("Options passed to nix-instantiate:"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, nixStoreOptions, 0,
        N_("Options passed to nix-store:"), NULL },

#ifdef	NOTYET
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"), NULL },
#endif

  POPT_AUTOHELP

  { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
	N_("\
Usage: nix-build [OPTION]... [FILE]...\n\
\n\
`nix-build' builds the given Nix expressions (which\n\
default to ./default.nix if none are given).  A symlink called\n\
`result' is placed in the current directory.\n\
"), NULL },

  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    rpmnix nix = &_nix;
    poptContext optCon = rpmioInit(argc, argv, nixBuildOptions);
    ARGV_t av = poptGetArgs(optCon);
    int ac = argvCount(av);
    const char * s = NULL;
    ARGV_t drvPaths = NULL;
    int ndrvPaths = 0;
    ARGV_t outPaths = NULL;
    int noutPaths = 0;
    int nexpr = 0;
    int ec = 1;		/* assume failure */
    int xx;
    int i;

    if ((s = getenv(_NIX_BIN_DIR)) != NULL) binDir = s;

#ifdef	REFERENCE
sub intHandler {
    exit 1;
}
$SIG{'INT'} = 'intHandler';
#endif

    if (ac == 0)
	xx = argvAdd(&nix->exprs, _default_nix);
    else
	xx = argvAppend(&nix->exprs, av);
    nexpr = argvCount(nix->exprs);

    if (nix->drvLink == NULL) {
	const char * _pre = !F_ISSET(nix, ADDDRVLINK) ? _build_tmp : "";
	nix->drvLink = rpmExpand(_pre, "derivation", NULL);
    }
    if (nix->outLink == NULL) {
	const char * _pre = !F_ISSET(nix, NOOUTLINK) ? _build_tmp : "";
	nix->outLink = rpmExpand(_pre, "result", NULL);
    }

    /* Loop over all the Nix expression arguments. */
    for (i = 0; i < nexpr; i++) {
	const char * expr = nix->exprs[i];

	/* Instantiate. */
	if (rpmnixInstantiate(nix, expr, &drvPaths))
	    goto exit;

	/* Verify that the derivations exist, print if verbose. */
	ndrvPaths = argvCount(drvPaths);
	for (i = 0; i < ndrvPaths; i++) {
	    const char * drvPath = drvPaths[i];
	    char target[BUFSIZ];
	    ssize_t nb = Readlink(drvPath, target, sizeof(target));
	    if (nb < 0) {
		fprintf(stderr, _("%s: cannot read symlink `%s'\n"),
			__progname, drvPath);
		goto exit;
	    }
	    target[nb] = '\0';
	    if (verbose)
		fprintf(stderr, "derivation is %s\n", target);
	}

	/* Build. */
	if (rpmnixStore(nix, drvPaths, &outPaths))
	    goto exit;

	if (F_ISSET(nix, DRYRUN))
	    continue;

	noutPaths = argvCount(outPaths);
	for (i = 0; i < noutPaths; i++) {
	    const char * outPath = outPaths[i];
	    char target[BUFSIZ];
	    ssize_t nb = Readlink(outPath, target, sizeof(target));
	    if (nb < 0) {
		fprintf(stderr, _("%s: cannot read symlink `%s'\n"),
			__progname, outPath);
		goto exit;
	    }
	    target[nb] = '\0';
	    fprintf(stdout, "%s\n", target);
	}

    }

    ec = 0;	/* XXX success */

exit:
    /* Clean up any generated files. */
    av = NULL;
    ac = 0;
    if (!rpmGlob(".nix-build-tmp-*", &ac, &av)) {
	for (i = 0; i < ac; i++)
	    xx = Unlink(av[i]);
	av = argvFree(av);
	ac = 0;
    }

    nix->outLink = _free(nix->outLink);
    nix->drvLink = _free(nix->drvLink);

    nix->instArgs = argvFree(nix->instArgs);
    nix->buildArgs = argvFree(nix->buildArgs);
    nix->exprs = argvFree(nix->exprs);

    optCon = rpmioFini(optCon);

    return ec;
}

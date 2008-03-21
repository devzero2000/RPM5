/**
 * \file rpmio/rpmrepo.c
 */

#include "system.h"

#include <poptIO.h>

#include "debug.h"

/*==============================================================*/

/*@unchecked@*/
static int quiet;
/*@unchecked@*/
static int verbose;
/*@unchecked@*/
static const char ** excludes;
/*@unchecked@*/
static const char * basedir;
/*@unchecked@*/
static const char * baseurl;
/*@unchecked@*/
static const char * groupfile;
/*@unchecked@*/
static const char * checksum = "sha";
/*@unchecked@*/
static int noepoch;
/*@unchecked@*/
static int pretty;
/*@unchecked@*/
static const char * cachedir;
/*@unchecked@*/
static int checkts;
/*@unchecked@*/
static int database;
/*@unchecked@*/
static int update;
/*@unchecked@*/
static int skipstat;
/*@unchecked@*/
static int split;
/*@unchecked@*/
static const char ** pkglist;
/*@unchecked@*/
static const char * outputdir;
/*@unchecked@*/
static int skipsymlinks;
/*@unchecked@*/
static int changeloglimit;
/*@unchecked@*/
static int uniquemdfilenames;

#if !defined(POPT_ARG_ARGV)
static int _poptSaveString(const char ***argvp, unsigned int argInfo, const char * val)
	/*@*/
{
    ARGV_t argv;
    int argc = 0;
    if (argvp == NULL)
	return -1;
    if (*argvp)
    while ((*argvp)[argc] != NULL)
	argc++;
    *argvp = xrealloc(*argvp, (argc + 1 + 1) * sizeof(**argvp));
    if ((argv = *argvp) != NULL) {
	argv[argc++] = xstrdup(val);
	argv[argc  ] = NULL;
    }
    return 0;
}
#endif

/**
 */
static void repoArgCallback(poptContext con,
                /*@unused@*/ enum poptCallbackReason reason,
                const struct poptOption * opt, const char * arg,
                /*@unused@*/ void * data)
        /*@globals fileSystem, internalState @*/
        /*@modifies fileSystem, internalState @*/
{
    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
#if !defined(POPT_ARG_ARGV)
	int xx;
    case 'x':			/* --excludes */
assert(arg != NULL);
        xx = _poptSaveString(&excludes, opt->argInfo, arg);
	break;
    case 'i':			/* --pkglist */
assert(arg != NULL);
        xx = _poptSaveString(&pkglist, opt->argInfo, arg);
	break;
#endif

    case 'v':			/* --verbose */
	verbose++;
	break;
    case '?':
    default:
	fprintf(stderr, _("%s: Unknown option -%c\n"), __progname, opt->val);
	poptPrintUsage(con, stderr, 0);
	exit(EXIT_FAILURE);
	/*@notreached@*/ break;
    }
}

/*@unchecked@*/ /*@observer@*/
static struct poptOption optionsTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        repoArgCallback, 0, NULL, NULL },
/*@=type@*/

 { "quiet", 'q', POPT_ARG_VAL,			&quiet, 0,
	N_("output nothing except for serious errors"), NULL },
 { "verbose", 'v', 0,				NULL, (int)'v',
	N_("output more debugging info."), NULL },
#if defined(POPT_ARG_ARGV)
 { "excludes", 'x', POPT_ARG_ARGV,		&excludes, 0,
	N_("file(s) to exclude"), N_("FILE") },
#else
 { "excludes", 'x', POPT_ARG_STRING,		NULL, 'x',
	N_("files to exclude"), N_("FILE") },
#endif
 { "basedir", '\0', POPT_ARG_STRING,		&basedir, 0,
	N_("files to exclude"), N_("DIR") },
 { "baseurl", 'u', POPT_ARG_STRING,		&baseurl, 0,
	N_("baseurl to append on all files"), N_("BASEURL") },
 { "groupfile", 'g', POPT_ARG_STRING,		&groupfile, 0,
	N_("path to groupfile to include in metadata"), N_("FILE") },
 { "checksum", 's', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN, &checksum, 0,
	N_("Deprecated, ignore"), NULL },
 { "noepoch", 'n', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&noepoch, 1,
	N_("don't add zero epochs for non-existent epochs "
           "(incompatible with yum and smart but required for "
           "systems with rpm < 4.2.1)"), NULL },
 { "pretty", 'p', POPT_ARG_VAL,			&pretty, 1,
	N_("make sure all xml generated is formatted"), NULL },
 { "cachedir", 'c', POPT_ARG_STRING,		&cachedir, 0,
	N_("set path to cache dir"), N_("DIR") },
 { "checkts", 'C', POPT_ARG_VAL,		&checkts, 1,
	N_("check timestamps on files vs the metadata to see if we need to update"), NULL },
 { "database", 'd', POPT_ARG_VAL,		&database, 1,
	N_("create sqlite database files"), NULL },
 { "update", '\0', POPT_ARG_VAL,		&update, 1,
	N_("use the existing repodata to speed up creation of new"), NULL },
 { "skip-stat", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&skipstat, 1,
	N_("skip the stat() call on a --update, assumes if the file "
	   "name is the same, then the file is still the same "
	   "(only use this if you're fairly trusting or gullible)"), NULL },
 { "split", '\0', POPT_ARG_VAL,			&split, 1,
	N_("generate split media"), NULL },
#if defined(POPT_ARG_ARGV)
 { "pkglist", 'i', POPT_ARG_ARGV,		&pkglist, 0,
	N_("use only the files listed in this file from the directory specified"), N_("FILE") },
#else
 { "pkglist", 'i', POPT_ARG_STRING,		NULL, 'i',
	N_("use only the files listed in this file from the directory specified"), N_("FILE") },
#endif
 { "outputdir", 'o', POPT_ARG_STRING,		&outputdir, 0,
	N_("<dir> = optional directory to output to"), N_("DIR") },
 { "skip-symlinks", 'S', POPT_ARG_VAL,		&skipsymlinks, 1,
	N_("ignore symlinks of packages"), NULL },
 { "changelog-limit", '\0', POPT_ARG_INT,	&changeloglimit, 0,
	N_("only import the last N changelog entries"), N_("N") },
 { "unique-md-filenames", '\0', POPT_ARG_VAL,	&uniquemdfilenames, 1,
	N_("include the file's checksum in the filename, helps with proxies"), NULL },

#ifdef	NOTYET
 { "version", '\0', 0, NULL, POPT_SHOWVERSION,
	N_("print the version"), NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioFtsPoptTable, 0,
	N_("Fts(3) traversal options:"), NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioDigestPoptTable, 0,
	N_("Available digests:"), NULL },
#endif

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"),
	NULL },

  POPT_AUTOALIAS
  POPT_AUTOHELP
  POPT_TABLEEND

};

int
main(int argc, char *argv[])
	/*@globals __assert_program_name,
		rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies __assert_program_name,
		rpmGlobalMacroContext, fileSystem, internalState @*/
{
    poptContext optCon;
    int rc = 1;		/* assume failure. */

    __progname = "rpmmrepo";

    /* Process options. */
    optCon = rpmioInit(argc, argv, optionsTable);

    argv = (char **) poptGetArgs(optCon);

    if (argv == NULL || argv[0] == NULL) {
	rpmlog(RPMLOG_ERR, _("Must specify a directory to index."));
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }
    if (argv[1] != NULL && !split) {
	rpmlog(RPMLOG_ERR, _("Only one directory allowed per run."));
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }
    if (split && checkts) {
	rpmlog(RPMLOG_ERR, _("--split and --checkts options are mutually exclusive"));
	goto exit;
    }

    /* Load the package manifest(s). */
    if (pkglist) {
	const char ** av = pkglist;
	const char * fn;
	while ((fn = *av++) != NULL) {
	    /* Append packages to todo list. */
	}
    }

    if (split) {
	/* SplitMetaDataGenerator */
    } else {
	/* MetaDataGenerator */
	/* checkTimeStamps */
#ifdef	NOTYET
	if (uptodate) {
	    fprintf(stdout, _("repo is up to date\n"));
	    rc = 0;
	    goto exit;
	}
#endif
    }

    /* doPkgMetadata */
    /* doRepoMetadata */
    /* doFinalMove */

    rc = 0;

exit:

    optCon = rpmioFini(optCon);

    return rc;
}

/**
 * \file rpmio/rpmrepo.c
 */

#include "system.h"

#include <argv.h>
#include <poptIO.h>

#include "debug.h"

/*==============================================================*/

typedef struct rpmrepo_s * rpmrepo;

struct rpmrepo_s {
    int quiet;
    int verbose;
/*@null@*/
    ARGV_t excludes;
/*@null@*/
    const char * baseurl;
/*@null@*/
    const char * groupfile;
/*@null@*/
    const char * sumtype;
    int noepoch;
    int pretty;
/*@null@*/
    const char * cachedir;
    int use_cache;
/*@null@*/
    const char * basedir;
    int checkts;
    int split;
    int update;
    int skipstat;
    int database;
/*@null@*/
    const char * outputdir;

/*@null@*/
    ARGV_t filepatterns;
/*@null@*/
    ARGV_t dirpatterns;
    int skipsymlinks;
/*@null@*/
    ARGV_t pkglist;
/*@null@*/
    const char * primaryfile;
/*@null@*/
    const char * filelistsfile;
/*@null@*/
    const char * otherfile;
/*@null@*/
    const char * repomdfile;
/*@null@*/
    const char * tempdir;
/*@null@*/
    const char * finaldir;
/*@null@*/
    const char * olddir;

    time_t mdtimestamp;

/*@null@*/
    const char * directory;
/*@null@*/
    ARGV_t directories;

    int changeloglimit;
    int uniquemdfilenames;

/*@null@*/
    const char * checksum;

/*@null@*/
    void * ts;
    int pkgcount;
/*@null@*/
    ARGV_t files;
/*@null@*/
    const char * package_dir;

};

/*@unchecked@*/
static struct rpmrepo_s __rpmrepo = {
    .sumtype = "sha", .checksum = "sha", .pretty = 1, .changeloglimit = 10,
    .primaryfile= "primary.xml.gz",
    .filelistsfile = "filelists.xml.gz",
    .otherfile	= "other.xml.gz",
    .repomdfile	= "repomd.xml.gz",
    .tempdir	= ".repodata",
    .finaldir	= "repodata",
    .olddir	= ".olddata"
};

/*@unchecked@*/
static rpmrepo _rpmrepo = &__rpmrepo;

/*==============================================================*/
/*@exist@*/
static void
repo_error(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    (void) fflush(NULL);
    (void) fprintf(stderr, "%s: ", __progname);
    (void) vfprintf(stderr, fmt, ap);
    va_end (ap);
    (void) fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
    /*@notreached@*/
}

static int rpmioExists(const char * fn, struct stat * st)
{
    return (Stat(fn, st) == 0);
}

/*==============================================================*/

static void repoParseDirectory(rpmrepo repo)
	/*@modifies repo @*/
{
    const char * relative_dir;
    char * fn = NULL;

assert(repo->directory != NULL);
    if (repo->directory[0] == '/') {
	fn = xstrdup(repo->directory);
	repo->basedir = xstrdup(dirname(fn));
	relative_dir = basename(fn);
    } else {
	repo->basedir = Realpath(repo->basedir, NULL);
	relative_dir = repo->directory;
    }
    repo->package_dir = rpmGetPath(repo->basedir, "/", relative_dir, NULL);
    if (repo->outputdir == NULL)
	repo->outputdir = rpmGetPath(repo->basedir, "/", relative_dir, NULL);
    fn = _free(fn);
}

static void repoTestSetupDirs(rpmrepo repo)
	/*@modifies repo @*/
{
    const char ** directories = repo->directories;
    struct stat sb, *st = &sb;
    const char * temp_output;
    const char * temp_final;
    const char * fn;

    while ((fn = *directories++) != NULL) {
	const char * testdir;

	if (fn[0] == '/')
	    testdir = xstrdup(fn);
	else if (fn[0] == '.' && fn[1] == '.' && fn[2] == '/') {
	    if ((testdir = Realpath(fn, NULL)) == NULL) {
		char buf[MAXPATHLEN];
		if ((testdir = Realpath(fn, buf)) == NULL)
		    repo_error(_("Realpath(%s) failed"), fn);
		testdir = xstrdup(testdir);
	    }
	} else
	    testdir = rpmGetPath(repo->basedir, "/", fn, NULL);

	if (!rpmioExists(testdir, st))
	    repo_error(_("Directory %s must exist"), testdir);

	if (!S_ISDIR(st->st_mode))
	    repo_error(_("%s must be a directory"), testdir);

	testdir = _free(testdir);
    }

    if (Access(repo->outputdir, W_OK))
	repo_error(_("Directory %s must be writable."), repo->outputdir);

    temp_output = rpmGetPath(repo->outputdir, "/", repo->tempdir, NULL);
    if (rpmioMkpath(temp_output, 0755, (uid_t)-1, (gid_t)-1))
	repo_error(_("Cannot create/verify %s"), temp_output);

    temp_final = rpmGetPath(repo->outputdir, "/", repo->finaldir, NULL);
    if (rpmioMkpath(temp_final, 0755, (uid_t)-1, (gid_t)-1))
	repo_error(_("Cannot create/verify %s"), temp_final);
    
    fn = rpmGetPath(repo->outputdir, "/", repo->olddir, NULL);
    if (rpmioExists(fn, st))
	repo_error(_("Old data directory exists, please remove: %s"), fn);
    fn = _free(fn);

  { static const char * dirs[] = { "tempdir", "finaldir", NULL };
    static const char * files[] =
	{ "primaryfile", "filelistsfile", "otherfile", "repomdfile", NULL };
    const char ** dirp, ** filep;
    for (dirp = dirs; *dirp != NULL; dirp++) {
	for (filep = files; *filep != NULL; filep++) {
	    fn = rpmGetPath(repo->outputdir, "/", *dirp, "/", *filep, NULL);
	    if (rpmioExists(fn, st)) {
		if (Access(fn, W_OK))
		    repo_error(_("Path must be writable: %s"), fn);

		if (repo->checkts && st->st_ctime > repo->mdtimestamp)
		    repo->mdtimestamp = st->st_ctime;
	    }
	    fn = _free(fn);
	}
    }
  }

    if (repo->groupfile != NULL) {
	fn = repo->groupfile;
	if (repo->split || fn[0] != '/')
	    fn = rpmGetPath(repo->package_dir, "/", repo->groupfile, NULL);
	if (!rpmioExists(fn, st))
	    repo_error(_("groupfile %s cannot be found."), fn);
	repo->groupfile = fn;
    }

    if (repo->cachedir != NULL) {
	fn = repo->cachedir;
	if (fn[0] == '/')
	    fn = rpmGetPath(repo->outputdir, "/", fn, NULL);
	if (rpmioMkpath(fn, 0755, (uid_t)-1, (gid_t)-1))
	    repo_error(_("cannot open/write to cache dir %s"), fn);
	repo->cachedir = fn;
    }
}

static int repoMetaDataGenerator(rpmrepo repo)
	/*@modifies repo @*/
{
    int xx;

    repo->ts = NULL;	/* rpmtsCreate() */
    repo->pkgcount = 0;
    repo->files = NULL;

    if (repo->directory == NULL && repo->directories == NULL)
	repo_error(_("No directory given on which to run."));

    if (repo->directory == NULL)
	repo->directory = xstrdup(repo->directories[0]);
    else if (repo->directories == NULL)
	xx = argvAdd(&repo->directories, repo->directory);
    repo->use_cache = (repo->cachedir != NULL);

    repoParseDirectory(repo);
    repoTestSetupDirs(repo);
	
    return 0;
}

static int repoCheckTimeStamps(rpmrepo repo)
	/*@*/
{   return 0; }

static int repoDoPkgMetadata(rpmrepo repo)
	/*@*/
{   return 0; }

static int repoDoRepoMetadata(rpmrepo repo)
	/*@*/
{   return 0; }

static int repoDoFinalMove(rpmrepo repo)
	/*@*/
{   return 0; }

/*==============================================================*/

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
    rpmrepo repo = _rpmrepo;

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
#if !defined(POPT_ARG_ARGV)
	int xx;
    case 'x':			/* --excludes */
assert(arg != NULL);
        xx = _poptSaveString(&repo->excludes, opt->argInfo, arg);
	break;
    case 'i':			/* --pkglist */
assert(arg != NULL);
        xx = _poptSaveString(&repo->pkglist, opt->argInfo, arg);
	break;
#endif

    case 'v':			/* --verbose */
	repo->verbose++;
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

 { "quiet", 'q', POPT_ARG_VAL,			&__rpmrepo.quiet, 0,
	N_("output nothing except for serious errors"), NULL },
 { "verbose", 'v', 0,				NULL, (int)'v',
	N_("output more debugging info."), NULL },
#if defined(POPT_ARG_ARGV)
 { "excludes", 'x', POPT_ARG_ARGV,		&__rpmrepo.excludes, 0,
	N_("file(s) to exclude"), N_("FILE") },
#else
 { "excludes", 'x', POPT_ARG_STRING,		NULL, 'x',
	N_("files to exclude"), N_("FILE") },
#endif
 { "basedir", '\0', POPT_ARG_STRING,		&__rpmrepo.basedir, 0,
	N_("files to exclude"), N_("DIR") },
 { "baseurl", 'u', POPT_ARG_STRING,		&__rpmrepo.baseurl, 0,
	N_("baseurl to append on all files"), N_("BASEURL") },
 { "groupfile", 'g', POPT_ARG_STRING,		&__rpmrepo.groupfile, 0,
	N_("path to groupfile to include in metadata"), N_("FILE") },
 { "checksum", 's', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN, &__rpmrepo.checksum, 0,
	N_("Deprecated, ignore"), NULL },
 { "noepoch", 'n', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmrepo.noepoch, 1,
	N_("don't add zero epochs for non-existent epochs "
           "(incompatible with yum and smart but required for "
           "systems with rpm < 4.2.1)"), NULL },
 { "pretty", 'p', POPT_ARG_VAL,			&__rpmrepo.pretty, 1,
	N_("make sure all xml generated is formatted"), NULL },
 { "cachedir", 'c', POPT_ARG_STRING,		&__rpmrepo.cachedir, 0,
	N_("set path to cache dir"), N_("DIR") },
 { "checkts", 'C', POPT_ARG_VAL,		&__rpmrepo.checkts, 1,
	N_("check timestamps on files vs the metadata to see if we need to update"), NULL },
 { "database", 'd', POPT_ARG_VAL,		&__rpmrepo.database, 1,
	N_("create sqlite database files"), NULL },
 { "update", '\0', POPT_ARG_VAL,		&__rpmrepo.update, 1,
	N_("use the existing repodata to speed up creation of new"), NULL },
 { "skip-stat", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmrepo.skipstat, 1,
	N_("skip the stat() call on a --update, assumes if the file "
	   "name is the same, then the file is still the same "
	   "(only use this if you're fairly trusting or gullible)"), NULL },
 { "split", '\0', POPT_ARG_VAL,			&__rpmrepo.split, 1,
	N_("generate split media"), NULL },
#if defined(POPT_ARG_ARGV)
 { "pkglist", 'i', POPT_ARG_ARGV,		&__rpmrepo.pkglist, 0,
	N_("use only the files listed in this file from the directory specified"), N_("FILE") },
#else
 { "pkglist", 'i', POPT_ARG_STRING,		NULL, 'i',
	N_("use only the files listed in this file from the directory specified"), N_("FILE") },
#endif
 { "outputdir", 'o', POPT_ARG_STRING,		&__rpmrepo.outputdir, 0,
	N_("<dir> = optional directory to output to"), N_("DIR") },
 { "skip-symlinks", 'S', POPT_ARG_VAL,		&__rpmrepo.skipsymlinks, 1,
	N_("ignore symlinks of packages"), NULL },
 { "changelog-limit", '\0', POPT_ARG_INT,	&__rpmrepo.changeloglimit, 0,
	N_("only import the last N changelog entries"), N_("N") },
 { "unique-md-filenames", '\0', POPT_ARG_VAL,	&__rpmrepo.uniquemdfilenames, 1,
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
    rpmrepo repo = _rpmrepo;
    poptContext optCon;
    int rc = 1;		/* assume failure. */
    int xx;

    __progname = "rpmmrepo";

    xx = argvAdd(&repo->filepatterns, ".*bin\\/.*");
    xx = argvAdd(&repo->filepatterns, "^\\/etc\\/.*");
    xx = argvAdd(&repo->filepatterns, "^\\/usr\\/lib\\/sendmail$");
    xx = argvAdd(&repo->filepatterns, "^\\/usr\\/lib\\/sendmail$");
    xx = argvAdd(&repo->dirpatterns, ".*bin\\/.*");
    xx = argvAdd(&repo->dirpatterns, "^\\/etc\\/.*");

    /* Process options. */
    optCon = rpmioInit(argc, argv, optionsTable);

    if (repo->basedir == NULL)
	repo->basedir = Realpath(".", NULL);

    repo->directories = poptGetArgs(optCon);

    if (repo->directories == NULL || repo->directories[0] == NULL) {
	rpmlog(RPMLOG_ERR, _("Must specify a directory to index.\n"));
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }
    if (repo->directories[1] != NULL && !repo->split) {
	rpmlog(RPMLOG_ERR, _("Only one directory allowed per run.\n"));
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }
    if (repo->split && repo->checkts) {
	rpmlog(RPMLOG_ERR, _("--split and --checkts options are mutually exclusive\n"));
	goto exit;
    }

    /* Load the package manifest(s). */
    if (repo->pkglist) {
	const char ** av = repo->pkglist;
	const char * fn;
	while ((fn = *av++) != NULL) {
	    /* Append packages to todo list. */
	}
    }

    rc = repoMetaDataGenerator(repo);
    if (!repo->split) {
	rc = repoCheckTimeStamps(repo);
#ifdef	NOTYET
	if (uptodate) {
	    fprintf(stdout, _("repo is up to date\n"));
	    rc = 0;
	    goto exit;
	}
#endif
    }

    rc = repoDoPkgMetadata(repo);
    rc = repoDoRepoMetadata(repo);
    rc = repoDoFinalMove(repo);

    rc = 0;

exit:
    repo->filepatterns = argvFree(repo->filepatterns);
    repo->dirpatterns = argvFree(repo->dirpatterns);

    optCon = rpmioFini(optCon);

    return rc;
}

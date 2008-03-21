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

/**
 * Check file name for a suffix.
 * @param fn            file name
 * @param suffix        suffix
 * @return              1 if file name ends with suffix
 */
static int chkSuffix(const char * fn, const char * suffix)
        /*@*/
{
    size_t flen = strlen(fn);
    size_t slen = strlen(suffix);
    return (flen > slen && !strcmp(fn + flen - slen, suffix));
}

/*@null@*/
static const char ** repoGetFileList(rpmrepo repo, const char * directory,
		const char * ext)
	/*@*/
{
    const char ** files = NULL;
    char *roots[2];
    FTS * t;
    FTSENT * p;
    int ftsoptions = FTS_PHYSICAL;
    int xx;

    roots[0] = (char *) directory;	/* XXX no cast */
    roots[1] = NULL;

    if ((t = Fts_open(roots, ftsoptions, NULL)) == NULL)
	repo_error(_("Fts_open: %s"), strerror(errno));

    while ((p = Fts_read(t)) != NULL) {

	switch (p->fts_info) {
	case FTS_D:
	case FTS_DP:
	default:
	    continue;
	    break;
	case FTS_SL:
	    if (repo->skipsymlinks)
		continue;
	    break;	/* XXX todo fuss with symlinks */
	case FTS_F:
	    if (!chkSuffix(p->fts_accpath, ext))
		continue;
	    xx = argvAdd(&files, p->fts_path);
	    break;
	}
    }

    return files;
}

static int repoCheckTimeStamps(rpmrepo repo)
	/*@*/
{
    int rc = 0;

    if (repo->checkts) {
	struct stat sb, *st = &sb;
	const char * basedirectory =
		rpmGetPath(repo->basedir, "/", repo->directory, NULL);
	/* XXX check whether repo->directory is relative. */
	const char ** files = repoGetFileList(repo, basedirectory, ".rpm");
	const char ** f;
	if (files != NULL)
	for (f = files; *f; f++) {
	    const char * fn =
		rpmGetPath(basedirectory, "/", *f, NULL);
	    int xx = rpmioExists(fn, st);

	    if (!xx)
		fprintf(stderr, _("cannot get to file: %s"), fn);
	    else if (st->st_ctime > repo->mdtimestamp)
		rc = 1;
	    fn = _free(fn);
	}
	basedirectory = _free(basedirectory);
    } else
	rc = 1;

    return rc;
}

static int repoOpenMetadataDocs(rpmrepo repo)
	/*@modifies repo @*/
{
    return 0;
}

static int repoWriteMetadataDocs(rpmrepo repo, const char ** packages)
	/*@modifies repo @*/
{
    return 0;
}

static int repoCloseMetadataDocs(rpmrepo repo)
	/*@modifies repo @*/
{
    return 0;
}

static int repoDoPkgMetadata(rpmrepo repo)
	/*@*/
{
    const char ** packages = NULL;
#ifdef	NOTYET
        if (repo->update)
            self._setup_old_metadata_lookup()
#endif

    if ((packages = repo->pkglist) == NULL)
	packages = repoGetFileList(repo, repo->package_dir, ".rpm");

#ifdef	NOTYET
        packages = self.trimRpms(packages)
#endif
    repo->pkgcount = argvCount(packages);
    repoOpenMetadataDocs(repo);
    repoWriteMetadataDocs(repo, packages);
    repoCloseMetadataDocs(repo);
    return 0;
}

static int repoDoRepoMetadata(rpmrepo repo)
	/*@*/
{
#ifdef	NOTYET
    def doRepoMetadata(self):
        """wrapper to generate the repomd.xml file that stores the info on the other files"""
        repodoc = libxml2.newDoc("1.0")
        reporoot = repodoc.newChild(None, "repomd", None)
        repons = reporoot.newNs('http://linux.duke.edu/metadata/repo', None)
        reporoot.setNs(repons)
        repopath = rpmGetPath(repo->outputdir, "/", repo->tempdir, NULL);
        repofilepath = rpmGetPath(repopath, "/", repo->repomdfile, NULL);

        sumtype = repo->sumtype
        workfiles = [(repo->otherfile, 'other',),
                     (repo->filelistsfile, 'filelists'),
                     (repo->primaryfile, 'primary')]
        repoid='garbageid'

        if repo->database:
            if not repo->quiet: self.callback.log('Generating sqlite DBs')
            try:
                dbversion = str(sqlitecachec.DBVERSION)
            except AttributeError:
                dbversion = '9'
            rp = sqlitecachec.RepodataParserSqlite(repopath, repoid, None)

        for (file, ftype) in workfiles:
            complete_path = rpmGetPath(repopath, "/", file, NULL);

            zfo = _gzipOpen(complete_path)
            uncsum = misc.checksum(sumtype, zfo)
            zfo.close()
            csum = misc.checksum(sumtype, complete_path)
	    (void) rpmioExists(complete_path, st)
            timestamp = os.stat(complete_path)[8]

            db_csums = {}
            db_compressed_sums = {}

            if repo->database:
                if repo->verbose:
                    self.callback.log("Starting %s db creation: %s" % (ftype, time.ctime()))

                if ftype == 'primary':
                    rp.getPrimary(complete_path, csum)

                elif ftype == 'filelists':
                    rp.getFilelists(complete_path, csum)

                elif ftype == 'other':
                    rp.getOtherdata(complete_path, csum)



                tmp_result_name = '%s.xml.gz.sqlite' % ftype
                tmp_result_path = rpmGetPath(repopath, "/", tmp_result_name, NULL);
                good_name = '%s.sqlite' % ftype
                resultpath = rpmGetPath(repopath, "/", good_name, NULL);

                # rename from silly name to not silly name
                xx = Rename(tmp_result_path, resultpath);
                compressed_name = '%s.bz2' % good_name
                result_compressed = rpmGetPath(repopath, "/", compressed_name, NULL);
                db_csums[ftype] = misc.checksum(sumtype, resultpath)

                # compress the files
                bzipFile(resultpath, result_compressed)
                # csum the compressed file
                db_compressed_sums[ftype] = misc.checksum(sumtype, result_compressed)
                # remove the uncompressed file
                xx = Unlink(resultpath);

                if repo->uniquemdfilenames:
                    csum_compressed_name = '%s-%s.bz2' % (db_compressed_sums[ftype], good_name)
                    csum_result_compressed =  rpmGetPath(repopath, "/", csum_compressed_name, NULL);
                    xx = Rename(result_compressed, csum_result_compressed);
                    result_compressed = csum_result_compressed
                    compressed_name = csum_compressed_name

                # timestamp the compressed file
		(void) rpmioExists(result_compressed, st)
                db_timestamp = os.stat(result_compressed)[8]

                # add this data as a section to the repomdxml
                db_data_type = '%s_db' % ftype
                data = reporoot.newChild(None, 'data', None)
                data.newProp('type', db_data_type)
                location = data.newChild(None, 'location', None)
                if repo->baseurl is not None:
                    location.newProp('xml:base', repo->baseurl)

                location.newProp('href', rpmGetPath(repo->finaldir, "/", compressed_name, NULL));
                checksum = data.newChild(None, 'checksum', db_compressed_sums[ftype])
                checksum.newProp('type', sumtype)
                db_tstamp = data.newChild(None, 'timestamp', str(db_timestamp))
                unchecksum = data.newChild(None, 'open-checksum', db_csums[ftype])
                unchecksum.newProp('type', sumtype)
                database_version = data.newChild(None, 'database_version', dbversion)
                if repo->verbose:
                    self.callback.log("Ending %s db creation: %s" % (ftype, time.ctime()))



            data = reporoot.newChild(None, 'data', None)
            data.newProp('type', ftype)

            checksum = data.newChild(None, 'checksum', csum)
            checksum.newProp('type', sumtype)
            timestamp = data.newChild(None, 'timestamp', str(timestamp))
            unchecksum = data.newChild(None, 'open-checksum', uncsum)
            unchecksum.newProp('type', sumtype)
            location = data.newChild(None, 'location', None)
            if repo->baseurl is not None:
                location.newProp('xml:base', repo->baseurl)
            if repo->uniquemdfilenames:
                res_file = '%s-%s.xml.gz' % (csum, ftype)
                orig_file = rpmGetPath(repopath, "/", file, NULL);
                dest_file = rpmGetPath(repopath, "/", res_file, NULL);
                xx = Rename(orig_file, dest_file);

            else:
                res_file = file

            file = res_file

            location.newProp('href', rpmGetPath(repo->finaldir, "/", file, NULL));


        if not repo->quiet and repo->database: self.callback.log('Sqlite DBs complete')


        if repo->groupfile is not None:
            self.addArbitraryMetadata(repo->groupfile, 'group_gz', reporoot)
            self.addArbitraryMetadata(repo->groupfile, 'group', reporoot, compress=False)

        # save it down
        try:
            repodoc.saveFormatFileEnc(repofilepath, 'UTF-8', 1)
        except:
            self.callback.errorlog(_('Error saving temp file for repomd.xml: %s') % repofilepath)
            repo_error("Could not save temp file: %s"), repofilepath);

        del repodoc
#endif
    return 0;
}

static int repoDoFinalMove(rpmrepo repo)
	/*@*/
{
    const char * output_final_dir =
		rpmGetPath(repo->outputdir, "/", repo->finaldir, NULL);
    const char * output_old_dir =
		rpmGetPath(repo->outputdir, "/", repo->olddir, NULL);
    const char * oldfile;
    struct stat sb, *st = &sb;
    int xx;

    if (rpmioExists(output_final_dir, st)) {
	if ((xx = Rename(output_final_dir, output_old_dir)) != 0)
	    repo_error(_("Error moving final %s to old dir %s"),
			output_final_dir, output_old_dir);
    }

    {	const char * output_temp_dir =
		rpmGetPath(repo->outputdir, "/", repo->tempdir, NULL);
	if ((xx = Rename(output_temp_dir, output_final_dir)) != 0) {
	    xx = Rename(output_old_dir, output_final_dir);
	    repo_error(_("Error moving final metadata into place"));
	}
	output_temp_dir = _free(output_temp_dir);
    }

  { static const char * files[] =
	{ "primaryfile", "filelistsfile", "otherfile", "repomdfile", "groupfile", NULL };
    const char ** filep;

    for (filep = files; *filep != NULL; filep++) {
	oldfile = rpmGetPath(output_old_dir, "/", *filep, NULL);
	if (rpmioExists(oldfile, st)) {
	    if (Unlink(oldfile))
		repo_error(_("Could not remove old metadata file: %s: %s"),
			oldfile, strerror(errno));
	}
	oldfile = _free(oldfile);
    }
  }

  { DIR * dir = Opendir(output_old_dir);
    struct dirent * dp;

    if (dir != NULL)
    while ((dp = Readdir(dir)) != NULL) {
	const char * finalfile;

	if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
	    continue;

	finalfile = rpmGetPath(output_final_dir, "/", dp->d_name, NULL);
	oldfile = rpmGetPath(output_old_dir, "/", dp->d_name, NULL);

	if (!strcmp(dp->d_name, "filelists.sqlite.bz2")
	 || !strcmp(dp->d_name, "other.sqlite.bz2")
	 || !strcmp(dp->d_name, "primary.sqlite.bz2"))
	{
	    xx = Unlink(oldfile);
	    oldfile = _free(oldfile);
	    continue;
	}

	if (rpmioExists(finalfile, st)) {
	    if (!S_ISDIR(st->st_mode)) {
		if ((xx = Unlink(oldfile)) != 0)
		    repo_error(_("Could not remove old metadata file: %s: %s"),
				oldfile, strerror(errno));
	    }
#ifdef	NOTYET
	    else {
		shutil.rmtree(oldfile)
	    }
#endif
	} else {
	    if ((xx = Rename(oldfile, finalfile)) != 0) {
		repo_error(_("Could not restore old non-metadata file: %s -> %s: %s"),
			oldfile, finalfile, strerror(errno));
	    }
	}
	oldfile = _free(oldfile);
    }
    xx = Closedir(dir);
  }

    if ((xx = Rmdir(output_old_dir)) != 0) {
	repo_error(_("Could not remove old metadata dir: %s: %s"),
		repo->olddir, strerror(errno));
    }
    output_old_dir = _free(output_old_dir);
    output_final_dir = _free(output_final_dir);

    return 0;
}

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

    if (repo->directories == NULL || repo->directories[0] == NULL)
	repo_error(_("Must specify a directory to index."));

    if (repo->directories[1] != NULL && !repo->split)
	repo_error(_("Only one directory allowed per run."));

    if (repo->split && repo->checkts)
	repo_error(_("--split and --checkts options are mutually exclusive"));

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
	if (rc == 0) {
	    fprintf(stdout, _("repo is up to date\n"));
	    goto exit;
	}
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

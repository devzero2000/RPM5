/**
 * \file rpmio/rpmrepo.c
 */

#include "system.h"

#if defined(WITH_SQLITE)
#include <sqlite3.h>
#ifdef	__LCLINT__
/*@-incondefs -redecl @*/
extern const char *sqlite3_errmsg(sqlite3 *db)
	/*@*/;
extern int sqlite3_open(
  const char *filename,		   /* Database filename (UTF-8) */
  /*@out@*/ sqlite3 **ppDb	   /* OUT: SQLite db handle */
)
	/*@modifies *ppDb @*/;
extern int sqlite3_exec(
  sqlite3 *db,			   /* An open database */
  const char *sql,		   /* SQL to be evaluted */
  int (*callback)(void*,int,char**,char**),  /* Callback function */
  void *,			   /* 1st argument to callback */
  /*@out@*/ char **errmsg	   /* Error msg written here */
)
	/*@modifies db, *errmsg @*/;
extern int sqlite3_prepare(
  sqlite3 *db,			   /* Database handle */
  const char *zSql,		   /* SQL statement, UTF-8 encoded */
  int nByte,			   /* Maximum length of zSql in bytes. */
  /*@out@*/ sqlite3_stmt **ppStmt, /* OUT: Statement handle */
  /*@out@*/ const char **pzTail	   /* OUT: Pointer to unused portion of zSql */
)
	/*@modifies *ppStmt, *pzTail @*/;
extern int sqlite3_reset(sqlite3_stmt *pStmt)
	/*@modifies pStmt @*/;
extern int sqlite3_step(sqlite3_stmt *pStmt)
	/*@modifies pStmt @*/;
extern int sqlite3_finalize(/*@only@*/ sqlite3_stmt *pStmt)
	/*@modifies pStmt @*/;
extern int sqlite3_close(sqlite3 * db)
	/*@modifies db @*/;
/*@=incondefs =redecl @*/
#endif	/* __LCLINT__ */
#endif	/* WITH_SQLITE */

#include <rpmio_internal.h>	/* XXX fdInitDigest() et al */
#include <rpmdir.h>
#include <fts.h>
#include <poptIO.h>

#define _RPMREPO_INTERNAL
#include <rpmrepo.h>

#include <rpmtypes.h>
#include <rpmtag.h>
#include <pkgio.h>
#include <rpmts.h>

#include "debug.h"

/*@access FD_t @*/
/*@access miRE @*/

extern rpmrepo _rpmrepo;

/*==============================================================*/
/**
 * Print an error message and exit (if requested).
 * @param lvl		error level (non-zero exits)
 * @param fmt		msg format
 */
/*@mayexit@*/
static void
repo_error(int lvl, const char *fmt, ...)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    va_list ap;

    va_start(ap, fmt);
    (void) fflush(NULL);
    (void) fprintf(stderr, "%s: ", __progname);
    (void) vfprintf(stderr, fmt, ap);
    va_end (ap);
    (void) fprintf(stderr, "\n");
    if (lvl)
	exit(EXIT_FAILURE);
}

/**
 * Display progress.
 * @param repo		repository 
 * @param item		repository item (usually a file path)
 * @param current	current iteration index
 * @param total		maximum iteration index
 */
static void repoProgress(/*@unused@*/ rpmrepo repo,
		/*@null@*/ const char * item, int current, int total)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    static size_t ncols = 80 - 1;	/* XXX TIOCGWINSIZ */
    const char * bn = (item != NULL ? strrchr(item, '/') : NULL);
    size_t nb;

    if (bn != NULL)
	bn++;
    else
	bn = item;
    nb = fprintf(stdout, "\r%s: %d/%d", __progname, current, total);
    if (bn != NULL)
	nb += fprintf(stdout, " - %s", bn);
    nb--;
    if (nb < ncols)
	fprintf(stdout, "%*s", (int)(ncols - nb), "");
    ncols = nb;
    (void) fflush(stdout);
}

/**
 * Return stat(2) for a file.
 * @retval st		stat(2) buffer
 * @return		0 on success
 */
static int rpmioExists(const char * fn, /*@out@*/ struct stat * st)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies st, fileSystem, internalState @*/
{
    return (Stat(fn, st) == 0);
}

/**
 * Return stat(2) creation time of a file.
 * @param fn		file path
 * @return		st_ctime
 */
static time_t rpmioCtime(const char * fn)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    struct stat sb;
    time_t stctime = 0;

    if (rpmioExists(fn, &sb))
	stctime = sb.st_ctime;
    return stctime;
}

/**
 * Return realpath(3) canonicalized absolute path.
 * @param lpath		file path
 * @retrun		canonicalized absolute path
 */
/*@null@*/
static const char * repoRealpath(const char * lpath)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    /* XXX GLIBC: realpath(path, NULL) return malloc'd */
    const char *rpath = Realpath(lpath, NULL);
    if (rpath == NULL) {
	char fullpath[MAXPATHLEN];
	rpath = Realpath(lpath, fullpath);
	if (rpath != NULL)
	    rpath = xstrdup(rpath);
    }
    return rpath;
}

/*==============================================================*/
/**
 * Create directory path.
 * @param repo		repository 
 * @param dn		directory path
 * @return		0 on success
 */
static int repoMkdir(rpmrepo repo, const char * dn)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext, fileSystem, internalState @*/
{
    const char * dnurl = rpmGetPath(repo->outputdir, "/", dn, NULL);
/*@-mods@*/
    int ut = urlPath(dnurl, &dn);
/*@=mods@*/
    int rc = 0;;

    /* XXX todo: rpmioMkpath doesn't grok URI's */
    if (ut == URL_IS_UNKNOWN)
	rc = rpmioMkpath(dn, 0755, (uid_t)-1, (gid_t)-1);
    else
	rc = (Mkdir(dnurl, 0755) == 0 || errno == EEXIST ? 0 : -1);
    if (rc)
	repo_error(0, _("Cannot create/verify %s: %s"), dnurl, strerror(errno));
    dnurl = _free(dnurl);
    return rc;
}

/**
 * Return /repository/directory/component.markup.compression path.
 * @param repo		repository
 * @param dir		directory
 * @param type		file
 * @return		repository file path
 */
static const char * repoGetPath(rpmrepo repo, const char * dir,
		const char * type, int compress)
	/*@globals h_errno, rpmGlobalMacroContext, internalState @*/
	/*@modifies rpmGlobalMacroContext, internalState @*/
{
    return rpmGetPath(repo->outputdir, "/", dir, "/", type,
		(repo->markup != NULL ? repo->markup : ""),
		(repo->suffix != NULL && compress ? repo->suffix : ""), NULL);
}

/**
 * Test for repository sanity.
 * @param repo		repository
 * @return		0 on success
 */
static int repoTestSetupDirs(rpmrepo repo)
	/*@globals h_errno, rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies repo, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    const char ** directories = repo->directories;
    struct stat sb, *st = &sb;
    const char * dn;
    const char * fn;
    int rc = 0;

    /* XXX todo: check repo->pkglist existence? */

    if (directories != NULL)
    while ((dn = *directories++) != NULL) {
	if (!rpmioExists(dn, st) || !S_ISDIR(st->st_mode)) {
	    repo_error(0, _("Directory %s must exist"), dn);
	    rc = 1;
	}
    }

    /* XXX todo create outputdir if it doesn't exist? */
    if (!rpmioExists(repo->outputdir, st)) {
	repo_error(0, _("Directory %s does not exist."), repo->outputdir);
	rc = 1;
    }
    if (Access(repo->outputdir, W_OK)) {
	repo_error(0, _("Directory %s must be writable."), repo->outputdir);
	rc = 1;
    }

    if (repoMkdir(repo, repo->tempdir)
     || repoMkdir(repo, repo->finaldir))
	rc = 1;

    dn = rpmGetPath(repo->outputdir, "/", repo->olddir, NULL);
    if (rpmioExists(dn, st)) {
	repo_error(0, _("Old data directory exists, please remove: %s"), dn);
	rc = 1;
    }
    dn = _free(dn);

  { /*@observer@*/
    static const char * dirs[] = { ".repodata", "repodata", NULL };
    /*@observer@*/
    static const char * types[] =
	{ "primary", "filelists", "other", "repomd", NULL };
    const char ** dirp, ** typep;
    for (dirp = dirs; *dirp != NULL; dirp++) {
	for (typep = types; *typep != NULL; typep++) {
	    fn = repoGetPath(repo, *dirp, *typep, strcmp(*typep, "repomd"));
	    if (rpmioExists(fn, st)) {
		if (Access(fn, W_OK)) {
		    repo_error(0, _("Path must be writable: %s"), fn);
		    rc = 1;
		} else
		if (REPO_ISSET(CHECKTS) && st->st_ctime > repo->mdtimestamp)
		    repo->mdtimestamp = st->st_ctime;
	    }
	    fn = _free(fn);
	}
    }
  }

#ifdef	NOTYET		/* XXX repo->package_dir needs to go away. */
    if (repo->groupfile != NULL) {
	if (REPO_ISSET(SPLIT) || repo->groupfile[0] != '/') {
	    fn = rpmGetPath(repo->package_dir, "/", repo->groupfile, NULL);
	    repo->groupfile = _free(repo->groupfile);
	    repo->groupfile = fn;
	    fn = NULL;
	}
	if (!rpmioExists(repo->groupfile, st)) {
	    repo_error(0, _("groupfile %s cannot be found."), repo->groupfile);
	    rc = 1;
	}
    }
#endif
    return rc;
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

/**
 * Walk file/directory trees, looking for files with an extension.
 * @param repo		repository
 * @param roots		file/directory trees to search
 * @param ext		file extension to match (usually ".rpm")
 * @return		list of files with the extension
 */
/*@null@*/
static const char ** repoGetFileList(rpmrepo repo, const char *roots[],
		const char * ext)
	/*@globals fileSystem, internalState @*/
	/*@modifies repo, fileSystem, internalState @*/
{
    const char ** pkglist = NULL;
    FTS * t;
    FTSENT * p;
    int xx;

    if ((t = Fts_open((char *const *)roots, repo->ftsoptions, NULL)) == NULL)
	repo_error(1, _("Fts_open: %s"), strerror(errno));

    while ((p = Fts_read(t)) != NULL) {
#ifdef	NOTYET
	const char * fts_name = p->fts_name;
	size_t fts_namelen = p->fts_namelen;

	/* XXX fts(3) (and Fts(3)) have fts_name = "" with pesky trailing '/' */
	if (p->fts_level == 0 && fts_namelen == 0) {
            fts_name = ".";
            fts_namelen = sizeof(".") - 1;
	}
#endif

	/* Should this element be excluded/included? */
	/* XXX todo: apply globs to fts_path rather than fts_name? */
/*@-onlytrans@*/
	if (mireApply(repo->excludeMire, repo->nexcludes, p->fts_name, 0, -1) >= 0)
	    continue;
	if (mireApply(repo->includeMire, repo->nincludes, p->fts_name, 0, +1) < 0)
	    continue;
/*@=onlytrans@*/

	switch (p->fts_info) {
	case FTS_D:
	case FTS_DP:
	default:
	    continue;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	case FTS_SL:
	    if (REPO_ISSET(NOFOLLOW))
		continue;
	    /* XXX todo: fuss with symlinks */
	    /*@notreached@*/ /*@switchbreak@*/ break;
	case FTS_F:
	    /* Is this a *.rpm file? */
	    if (chkSuffix(p->fts_name, ext))
		xx = argvAdd(&pkglist, p->fts_path);
	    /*@switchbreak@*/ break;
	}
    }

    (void) Fts_close(t);

if (_rpmrepo_debug)
argvPrint("pkglist", pkglist, NULL);

    return pkglist;
}

/**
 * Check that repository time stamp is newer than any contained package.
 * @param repo		repository
 * @return		0 on success
 */
static int repoCheckTimeStamps(rpmrepo repo)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    int rc = 0;

    if (REPO_ISSET(CHECKTS)) {
	const char ** pkg;

	if (repo->pkglist != NULL)
	for (pkg = repo->pkglist; *pkg != NULL ; pkg++) {
	    struct stat sb, *st = &sb;
	    if (!rpmioExists(*pkg, st)) {
		repo_error(0, _("cannot get to file: %s"), *pkg);
		rc = 1;
	    } else if (st->st_ctime > repo->mdtimestamp)
		rc = 1;
	}
    } else
	rc = 1;

    return rc;
}

/**
 * Write to a repository metadata file.
 * @param rfile		repository metadata file
 * @param spew		contents
 * @return		0 on success
 */
static int rfileXMLWrite(rpmrfile rfile, /*@only@*/ /*@null@*/ const char * spew)
	/*@globals fileSystem @*/
	/*@modifies rfile, fileSystem @*/
{
    size_t nspew = (spew != NULL ? strlen(spew) : 0);
/*@-nullpass@*/	/* XXX spew != NULL @*/
    size_t nb = (nspew > 0 ? Fwrite(spew, 1, nspew, rfile->fd) : 0);
/*@=nullpass@*/
    int rc = 0;
    if (nspew != nb) {
	repo_error(0, _("Fwrite failed: expected write %u != %u bytes: %s\n"),
		(unsigned)nspew, (unsigned)nb, Fstrerror(rfile->fd));
	rc = 1;
    }
    spew = _free(spew);
    return rc;
}

/**
 * Close an I/O stream, accumulating uncompress/digest statistics.
 * @param repo		repository
 * @param fd		I/O stream
 * @return		0 on success
 */
static int repoFclose(rpmrepo repo, FD_t fd)
	/*@modifies repo, fd @*/
{
    int rc = 0;

    if (fd != NULL) {
	rpmts ts = repo->_ts;
	if (ts != NULL) {
	    (void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_UNCOMPRESS),
			fdstat_op(fd, FDSTAT_READ));
	    (void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_DIGEST),
			fdstat_op(fd, FDSTAT_DIGEST));
	}
	rc = Fclose(fd);
    }
    return rc;
}

/**
 * Open a repository metadata file.
 * @param repo		repository
 * @param rfile		repository metadata file
 * @return		0 on success
 */
static int repoOpenMDFile(const rpmrepo repo, rpmrfile rfile)
	/*@globals h_errno, rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies rfile, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    const char * spew = rfile->xml_init;
    size_t nspew = strlen(spew);
    const char * fn = repoGetPath(repo, repo->tempdir, rfile->type, 1);
    const char * tail;
    size_t nb;
    int rc = 0;

    rfile->fd = Fopen(fn, repo->wmode);
assert(rfile->fd != NULL);

    if (repo->algo != PGPHASHALGO_NONE)
	fdInitDigest(rfile->fd, repo->algo, 0);

    if ((tail = strstr(spew, " packages=\"0\">\n")) != NULL)
	nspew -= strlen(tail);

    nb = Fwrite(spew, 1, nspew, rfile->fd);

    if (tail != NULL) {
	char buf[64];
	size_t tnb = snprintf(buf, sizeof(buf), " packages=\"%d\">\n",
				repo->pkgcount);
	nspew += tnb;
	nb += Fwrite(buf, 1, tnb, rfile->fd);
    }
    if (nspew != nb) {
	repo_error(0, _("Fwrite failed: expected write %u != %u bytes: %s\n"),
		(unsigned)nspew, (unsigned)nb, Fstrerror(rfile->fd));
	rc = 1;
    }

    fn = _free(fn);

#if defined(WITH_SQLITE)
    if (REPO_ISSET(DATABASE)) {
	const char ** stmt;
	int xx;
	fn = rpmGetPath(repo->outputdir, "/", repo->tempdir, "/",
		rfile->type, ".sqlite", NULL);
	if ((xx = sqlite3_open(fn, &rfile->sqldb)) != SQLITE_OK)
	    repo_error(1, "sqlite3_open(%s): %s", fn, sqlite3_errmsg(rfile->sqldb));
	for (stmt = rfile->sql_init; *stmt != NULL; stmt++) {
	    char * msg;
	    xx = sqlite3_exec(rfile->sqldb, *stmt, NULL, NULL, &msg);
	    if (xx != SQLITE_OK)
		repo_error(1, "sqlite3_exec(%s, \"%s\"): %s\n", fn, *stmt,
			(msg != NULL ? msg : "failed"));
	}
	fn = _free(fn);
    }
#endif

    return rc;
}

/**
 * Read a header from a repository package file, computing package file digest.
 * @param repo		repository
 * @param path		package file path
 * @return		header (NULL on error)
 */
static Header repoReadHeader(rpmrepo repo, const char * path)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies repo, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    /* XXX todo: read the payload and collect the blessed file digest. */
    FD_t fd = Fopen(path, "r.ufdio");
    Header h = NULL;

    if (fd != NULL) {
	rpmts ts = repo->_ts;
	uint32_t algo = repo->pkgalgo;
	rpmRC rpmrc;

	if (algo != PGPHASHALGO_NONE)
	    fdInitDigest(fd, algo, 0);

	/* XXX what if path needs expansion? */
	rpmrc = rpmReadPackageFile(ts, fd, path, &h);
	if (algo != PGPHASHALGO_NONE) {
	    char buffer[32 * BUFSIZ];
	    size_t nb = sizeof(buffer);
	    size_t nr;
	    while ((nr = Fread(buffer, sizeof(buffer[0]), nb, fd)) == nb)
		{};
	    if (Ferror(fd)) {
		fprintf(stderr, _("%s: Fread(%s) failed: %s\n"),
			__progname, path, Fstrerror(fd));
		rpmrc = RPMRC_FAIL;
	    } else {
		static int asAscii = 1;
		const char *digest = NULL;
		fdFiniDigest(fd, algo, &digest, NULL, asAscii);
		(void) headerSetDigest(h, digest);
		digest = _free(digest);
	    }
	}

	(void) Fclose(fd);

	switch (rpmrc) {
	case RPMRC_NOTFOUND:
	case RPMRC_FAIL:
	default:
	    (void) headerFree(h);
	    h = NULL;
	    break;
	case RPMRC_NOTTRUSTED:
	case RPMRC_NOKEY:
	case RPMRC_OK:
	    if (repo->baseurl)
		(void) headerSetBaseURL(h, repo->baseurl);
	    break;
	}
    }
    return h;
}

/**
 * Return header query.
 * @param h		header
 * @param qfmt		query format
 * @return		query format result
 */
static const char * rfileHeaderSprintf(Header h, const char * qfmt)
	/*@globals fileSystem @*/
	/*@modifies h, fileSystem @*/
{
    const char * msg = NULL;
    const char * s = headerSprintf(h, qfmt, NULL, NULL, &msg);
    if (s == NULL)
	repo_error(1, _("headerSprintf(%s): %s"), qfmt, msg);
assert(s != NULL);
    return s;
}

#if defined(WITH_SQLITE)
/**
 * Check sqlite3 return, displaying error messages.
 * @param rfile		repository metadata file
 * @return		SQLITE_OK on success
 */
static int rfileSQL(rpmrfile rfile, const char * msg, int rc)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    if (rc != SQLITE_OK || _rpmrepo_debug)
	repo_error(0, "sqlite3_%s(%s): %s", msg, rfile->type,
		sqlite3_errmsg(rfile->sqldb));
    return rc;
}

/**
 * Execute a compiled SQL command.
 * @param rfile		repository metadata file
 * @return		SQLITE_OK on success
 */
static int rfileSQLStep(rpmrfile rfile, sqlite3_stmt * stmt)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    int loop = 1;
    int rc = 0;
    int xx;

/*@-infloops@*/
    while (loop) {
	rc = sqlite3_step(stmt);
	switch (rc) {
	default:
	    rc = rfileSQL(rfile, "step", rc);
	    /*@fallthrough@*/
	case SQLITE_DONE:
	    loop = 0;
	    /*@switchbreak@*/ break;
	}
    }
/*@=infloops@*/

    xx = rfileSQL(rfile, "reset",
	sqlite3_reset(stmt));

    return rc;
}

/**
 * Return header query, with "XXX" replaced by rpmdb header instance.
 * @param h		header
 * @param qfmt		query format
 * @return		query format result
 */
static const char * rfileHeaderSprintfHack(Header h, const char * qfmt)
	/*@globals fileSystem @*/
	/*@modifies h, fileSystem @*/
{
    static const char mark[] = "'XXX'";
    static size_t nmark = sizeof("'XXX'") - 1;
    const char * msg = NULL;
    char * s = (char *) headerSprintf(h, qfmt, NULL, NULL, &msg);
    char * f, * fe;
    int nsubs = 0;

    if (s == NULL)
	repo_error(1, _("headerSprintf(%s): %s"), qfmt, msg);
assert(s != NULL);

    /* XXX Find & replace 'XXX' with '%{DBINSTANCE}' the hard way. */
/*@-nullptrarith@*/
    for (f = s; *f != '\0' && (fe = strstr(f, "'XXX'")) != NULL; fe += nmark, f = fe)
	nsubs++;
/*@=nullptrarith@*/

    if (nsubs > 0) {
	char instance[64];
	int xx = snprintf(instance, sizeof(instance), "'%u'",
		(uint32_t) headerGetInstance(h));
	size_t tlen = strlen(s) + nsubs * ((int)strlen(instance) - (int)nmark);
	char * t = xmalloc(tlen + 1);
	char * te = t;

	xx = xx;
/*@-nullptrarith@*/
	for (f = s; *f != '\0' && (fe = strstr(f, mark)) != NULL; fe += nmark, f = fe) {
	    *fe = '\0';
	    te = stpcpy( stpcpy(te, f), instance);
	}
/*@=nullptrarith@*/
	if (*f != '\0')
	    te = stpcpy(te, f);
	s = _free(s);
	s = t;
    }
 
    return s;
}

/**
 * Run a sqlite3 command.
 * @param rfile		repository metadata file
 * @param cmd		sqlite3 command to run
 * @return		0 always
 */
static int rfileSQLWrite(rpmrfile rfile, /*@only@*/ const char * cmd)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    sqlite3_stmt * stmt;
    const char * tail;
    int xx;

    xx = rfileSQL(rfile, "prepare",
	sqlite3_prepare(rfile->sqldb, cmd, (int)strlen(cmd), &stmt, &tail));

    xx = rfileSQL(rfile, "reset",
	sqlite3_reset(stmt));

    xx = rfileSQLStep(rfile, stmt);

    xx = rfileSQL(rfile, "finalize",
	sqlite3_finalize(stmt));

    cmd = _free(cmd);

    return 0;
}
#endif

/**
 * Export a single package's metadata to repository metadata file(s).
 * @param repo		repository
 * @param rfile		repository metadata file
 * @param h		header
 * @return		0 on success
 */
static int repoWriteMDFile(rpmrepo repo, rpmrfile rfile, Header h)
	/*@globals fileSystem @*/
	/*@modifies rfile, h, fileSystem @*/
{
    int rc = 0;

    if (rfile->xml_qfmt != NULL) {
	if (rfileXMLWrite(rfile, rfileHeaderSprintf(h, rfile->xml_qfmt)))
	    rc = 1;
    }

#if defined(WITH_SQLITE)
    if (REPO_ISSET(DATABASE)) {
	if (rfileSQLWrite(rfile, rfileHeaderSprintfHack(h, rfile->sql_qfmt)))
	    rc = 1;
    }
#endif

    return rc;
}

/**
 * Export all package metadata to repository metadata file(s).
 * @param repo		repository
 * @param pkglist	repository packages
 * @return		0 on success
 */
static int repoWriteMetadataDocs(rpmrepo repo, /*@null@*/ const char ** pkglist)
	/*@globals h_errno, rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies repo, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    const char * pkg;
    int rc = 0;

    while ((pkg = *pkglist++) != NULL) {
	Header h = repoReadHeader(repo, pkg);

	repo->current++;
	if (h == NULL) {
	/* XXX repoReadHeader() displays error. Continuing is foolish */
	    rc = 1;
	    break;
	}
	(void) headerSetInstance(h, (uint32_t)repo->current);

#ifdef	NOTYET
	/* XXX todo: rpmGetPath(mydir, "/", filematrix[mydir], NULL); */
	reldir = (pkgpath != NULL ? pkgpath : rpmGetPath(repo->basedir, "/", repo->directories[0], NULL));
	self.primaryfile.write(po.do_primary_xml_dump(reldir, baseurl=repo->baseurl))
	self.flfile.write(po.do_filelists_xml_dump())
	self.otherfile.write(po.do_other_xml_dump())
#endif
	if (repoWriteMDFile(repo, &repo->primary, h)
	 || repoWriteMDFile(repo, &repo->filelists, h)
	 || repoWriteMDFile(repo, &repo->other, h))
	    rc = 1;

	(void) headerFree(h);
	h = NULL;
	if (rc) break;

	if (!repo->quiet) {
	    if (repo->verbose)
		repo_error(0, "%d/%d - %s", repo->current, repo->pkgcount, pkg);
	    else
		repoProgress(repo, pkg, repo->current, repo->pkgcount);
	}
    }
    return rc;
}

/**
 * Compute digest of a file.
 * @return		0 on success
 */
static int repoRfileDigest(const rpmrepo repo, rpmrfile rfile,
		const char ** digestp)
	/*@modifies *digestp @*/
{
    static int asAscii = 1;
    struct stat sb, *st = &sb;
    const char * fn = repoGetPath(repo, repo->tempdir, rfile->type, 1);
    const char * path = NULL;
    int ut = urlPath(fn, &path);
    FD_t fd = NULL;
    int rc = 1;
    int xx;

    memset(st, 0, sizeof(*st));
    if (!rpmioExists(fn, st))
	goto exit;
    fd = Fopen(fn, "r.ufdio");
    if (fd == NULL || Ferror(fd))
	goto exit;

    switch (ut) {
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
#if defined(HAVE_MMAP)
    {   void * mapped = (void *)-1;

	if (st->st_size > 0)
	    mapped = mmap(NULL, st->st_size, PROT_READ, MAP_SHARED, Fileno(fd), 0);
	if (mapped != (void *)-1) {
	    rpmts ts = repo->_ts;
	    rpmop op = rpmtsOp(ts, RPMTS_OP_DIGEST);
	    rpmtime_t tstamp = rpmswEnter(op, 0);
	    DIGEST_CTX ctx = rpmDigestInit(repo->algo, RPMDIGEST_NONE);
	    xx = rpmDigestUpdate(ctx, mapped, st->st_size);
	    xx = rpmDigestFinal(ctx, digestp, NULL, asAscii);
	    tstamp = rpmswExit(op, st->st_size);
	    xx = munmap(mapped, st->st_size);
	    break;
	}
    }	/*@fallthrough@*/
#endif
    default:
    {	char buf[64 * BUFSIZ];
	size_t nb;
	size_t fsize = 0;

	fdInitDigest(fd, repo->algo, 0);
	while ((nb = Fread(buf, sizeof(buf[0]), sizeof(buf), fd)) > 0)
	    fsize += nb;
	if (Ferror(fd))
	    goto exit;
	fdFiniDigest(fd, repo->algo, digestp, NULL, asAscii);
    }	break;
    }

    rc = 0;

exit:
    if (fd)
	xx = repoFclose(repo, fd);
    fn = _free(fn);
    return rc;
}

/**
 * Close a repository metadata file.
 * @param repo		repository
 * @param rfile		repository metadata file
 * @return		0 on success
 */
static int repoCloseMDFile(const rpmrepo repo, rpmrfile rfile)
	/*@globals h_errno, rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies rfile, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    static int asAscii = 1;
    char * xmlfn = xstrdup(fdGetOPath(rfile->fd));
    int rc = 0;

    if (!repo->quiet)
	repo_error(0, _("Saving %s metadata"), basename(xmlfn));

    if (rfileXMLWrite(rfile, xstrdup(rfile->xml_fini)))
	rc = 1;

    if (repo->algo > 0)
	fdFiniDigest(rfile->fd, repo->algo, &rfile->digest, NULL, asAscii);
    else
	rfile->digest = xstrdup("");

    (void) repoFclose(repo, rfile->fd);
    rfile->fd = NULL;

    /* Compute the (usually compressed) ouput file digest too. */
    rfile->Zdigest = NULL;
    (void) repoRfileDigest(repo, rfile, &rfile->Zdigest);

#if defined(WITH_SQLITE)
    if (REPO_ISSET(DATABASE) && rfile->sqldb != NULL) {
	const char *dbfn = rpmGetPath(repo->outputdir, "/", repo->tempdir, "/",
		rfile->type, ".sqlite", NULL);
	int xx;
	if ((xx = sqlite3_close(rfile->sqldb)) != SQLITE_OK)
	    repo_error(1, "sqlite3_close(%s): %s", dbfn, sqlite3_errmsg(rfile->sqldb));
	rfile->sqldb = NULL;
	dbfn = _free(dbfn);
    }
#endif

    rfile->ctime = rpmioCtime(xmlfn);
    xmlfn = _free(xmlfn);

    return rc;
}

/**
 * Write repository metadata files.
 * @param repo		repository
 * @return		0 on success
 */
static int repoDoPkgMetadata(rpmrepo repo)
	/*@globals h_errno, rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies repo, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    int rc = 0;

    repo->current = 0;

#ifdef	NOTYET
    def _getFragmentUrl(self, url, fragment):
        import urlparse
        urlparse.uses_fragment.append('media')
        if not url:
            return url
        (scheme, netloc, path, query, fragid) = urlparse.urlsplit(url)
        return urlparse.urlunsplit((scheme, netloc, path, query, str(fragment)))

    def doPkgMetadata(self):
        """all the heavy lifting for the package metadata"""
        if (argvCount(repo->directories) == 1) {
            MetaDataGenerator.doPkgMetadata(self)
            return
	}

    ARGV_t roots = NULL;
    filematrix = {}
    for mydir in repo->directories {
	if (mydir[0] == '/')
	    thisdir = xstrdup(mydir);
	else if (mydir[0] == '.' && mydir[1] == '.' && mydir[2] == '/')
	    thisdir = Realpath(mydir, NULL);
	else
	    thisdir = rpmGetPath(repo->basedir, "/", mydir, NULL);

	xx = argvAdd(&roots, thisdir);
	thisdir = _free(thisdir);

	filematrix[mydir] = repoGetFileList(repo, roots, '.rpm')
	self.trimRpms(filematrix[mydir])
	repo->pkgcount = argvCount(filematrix[mydir]);
	roots = argvFree(roots);
    }

    mediano = 1;
    repo->baseurl = self._getFragmentUrl(repo->baseurl, mediano)
#endif

    if (repoOpenMDFile(repo, &repo->primary)
     || repoOpenMDFile(repo, &repo->filelists)
     || repoOpenMDFile(repo, &repo->other))
	rc = 1;
    if (rc) return rc;

#ifdef	NOTYET
    for mydir in repo->directories {
	repo->baseurl = self._getFragmentUrl(repo->baseurl, mediano)
	/* XXX todo: rpmGetPath(mydir, "/", filematrix[mydir], NULL); */
	if (repoWriteMetadataDocs(repo, filematrix[mydir]))
	    rc = 1;
	mediano++;
    }
    repo->baseurl = self._getFragmentUrl(repo->baseurl, 1)
#else
    if (repoWriteMetadataDocs(repo, repo->pkglist))
	rc = 1;
#endif

    if (!repo->quiet)
	fprintf(stderr, "\n");
    if (repoCloseMDFile(repo, &repo->primary)
     || repoCloseMDFile(repo, &repo->filelists)
     || repoCloseMDFile(repo, &repo->other))
	rc = 1;

    return rc;
}

/**
 */
static /*@observer@*/ /*@null@*/ const char *
algo2tagname(uint32_t algo)
	/*@*/
{
    const char * tagname = NULL;

    switch (algo) {
    case PGPHASHALGO_NONE:	tagname = "none";	break;
    case PGPHASHALGO_MD5:	tagname = "md5";	break;
    /* XXX todo: should be "sha1" */
    case PGPHASHALGO_SHA1:	tagname = "sha";	break;
    case PGPHASHALGO_RIPEMD160:	tagname = "rmd160";	break;
    case PGPHASHALGO_MD2:	tagname = "md2";	break;
    case PGPHASHALGO_TIGER192:	tagname = "tiger192";	break;
    case PGPHASHALGO_HAVAL_5_160: tagname = "haval160";	break;
    case PGPHASHALGO_SHA256:	tagname = "sha256";	break;
    case PGPHASHALGO_SHA384:	tagname = "sha384";	break;
    case PGPHASHALGO_SHA512:	tagname = "sha512";	break;
    case PGPHASHALGO_MD4:	tagname = "md4";	break;
    case PGPHASHALGO_RIPEMD128:	tagname = "rmd128";	break;
    case PGPHASHALGO_CRC32:	tagname = "crc32";	break;
    case PGPHASHALGO_ADLER32:	tagname = "adler32";	break;
    case PGPHASHALGO_CRC64:	tagname = "crc64";	break;
    case PGPHASHALGO_JLU32:	tagname = "jlu32";	break;
    case PGPHASHALGO_SHA224:	tagname = "sha224";	break;
    case PGPHASHALGO_RIPEMD256:	tagname = "rmd256";	break;
    case PGPHASHALGO_RIPEMD320:	tagname = "rmd320";	break;
    case PGPHASHALGO_SALSA10:	tagname = "salsa10";	break;
    case PGPHASHALGO_SALSA20:	tagname = "salsa20";	break;
    default:			tagname = NULL;		break;
    }
    return tagname;
}

/**
 * Return a repository metadata file item.
 * @param repo		repository
 * @return		repository metadata file item
 */
static const char * repoMDExpand(rpmrepo repo, rpmrfile rfile)
	/*@globals h_errno, rpmGlobalMacroContext, internalState @*/
	/*@modifies rpmGlobalMacroContext, internalState @*/
{
    const char * spewalgo = algo2tagname(repo->algo);
    char spewtime[64];

    (void) snprintf(spewtime, sizeof(spewtime), "%u", (unsigned)rfile->ctime);
    return rpmExpand("\
  <data type=\"", rfile->type, "\">\n\
    <checksum type=\"", spewalgo, "\">", rfile->Zdigest, "</checksum>\n\
    <timestamp>", spewtime, "</timestamp>\n\
    <open-checksum type=\"",spewalgo,"\">", rfile->digest, "</open-checksum>\n\
    <location href=\"", repo->finaldir, "/", rfile->type, (repo->markup != NULL ? repo->markup : ""), (repo->suffix != NULL ? repo->suffix : ""), "\"/>\n\
  </data>\n", NULL);
}

/**
 * Write repository manifest.
 * @param repo		repository
 * @return		0 on success.
 */
static int repoDoRepoMetadata(rpmrepo repo)
	/*@globals h_errno, rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies repo, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    rpmrfile rfile = &repo->repomd;
    const char * fn = repoGetPath(repo, repo->tempdir, rfile->type, 0);
    int rc = 0;

    if ((rfile->fd = Fopen(fn, "w.ufdio")) != NULL) {	/* no compression */
	if (rfileXMLWrite(rfile, xstrdup(rfile->xml_init))
	 || rfileXMLWrite(rfile, repoMDExpand(repo, &repo->other))
	 || rfileXMLWrite(rfile, repoMDExpand(repo, &repo->filelists))
	 || rfileXMLWrite(rfile, repoMDExpand(repo, &repo->primary))
	 || rfileXMLWrite(rfile, xstrdup(rfile->xml_fini)))
	    rc = 1;
	(void) repoFclose(repo, rfile->fd);
	rfile->fd = NULL;
    }

    fn = _free(fn);
    if (rc) return rc;

#ifdef	NOTYET
    def doRepoMetadata(self):
        """wrapper to generate the repomd.xml file that stores the info on the other files"""
    const char * repopath =
		rpmGetPath(repo->outputdir, "/", repo->tempdir, NULL);
        repodoc = libxml2.newDoc("1.0")
        reporoot = repodoc.newChild(None, "repomd", None)
        repons = reporoot.newNs("http://linux.duke.edu/metadata/repo", None)
        reporoot.setNs(repons)
        repopath = rpmGetPath(repo->outputdir, "/", repo->tempdir, NULL);
        fn = repoGetPath(repo, repo->tempdir, repo->repomd.type, 1);

        repoid = "garbageid";

        if (REPO_ISSET(DATABASE)) {
            if (!repo->quiet) repo_error(0, _("Generating sqlite DBs"));
            try:
                dbversion = str(sqlitecachec.DBVERSION)
            except AttributeError:
                dbversion = "9"
            rp = sqlitecachec.RepodataParserSqlite(repopath, repoid, None)
	}

  { static const char * types[] =
	{ "primary", "filelists", "other", NULL };
    const char ** typep;
	for (typep = types; *typep != NULL; typep++) {
	    complete_path = repoGetPath(repo, repo->tempdir, *typep, 1);

            zfo = _gzipOpen(complete_path)
            uncsum = misc.checksum(algo2tagname(repo->algo), zfo)
            zfo.close()
            csum = misc.checksum(algo2tagname(repo->algo), complete_path)
	    (void) rpmioExists(complete_path, st)
            timestamp = os.stat(complete_path)[8]

            db_csums = {}
            db_compressed_sums = {}

            if (REPO_ISSET(repo)) {
                if (repo->verbose) {
		    time_t now = time(NULL);
                    repo_error(0, _("Starting %s db creation: %s"),
			*typep, ctime(&now));
		}

                if (!strcmp(*typep, "primary"))
                    rp.getPrimary(complete_path, csum)
                else if (!strcmp(*typep, "filelists"));
                    rp.getFilelists(complete_path, csum)
                else if (!strcmp(*typep, "other"))
                    rp.getOtherdata(complete_path, csum)

		{   const char *  tmp_result_path =
			rpmGetPath(repo->outputdir, "/", repo->tempdir, "/",
				*typep, ".xml.gz.sqlite", NULL);
		    const char * resultpath =
			rpmGetPath(repo->outputdir, "/", repo->tempdir, "/",
				*typep, ".sqlite", NULL);

		    /* rename from silly name to not silly name */
		    xx = Rename(tmp_result_path, resultpath);
		    tmp_result_path = _free(tmp_result_path);
		    result_compressed =
			rpmGetPath(repo->outputdir, "/", repo->tempdir, "/",
				*typep, ".sqlite.bz2", NULL);
		    db_csums[*typep] = misc.checksum(algo2tagname(repo->algo), resultpath)

		    /* compress the files */
		    bzipFile(resultpath, result_compressed)
		    /* csum the compressed file */
		    db_compressed_sums[*typep] = misc.checksum(algo2tagname(repo->algo), result_compressed)
		    /* remove the uncompressed file */
		    xx = Unlink(resultpath);
		    resultpath = _free(resultpath);
		}

                if (REPO_ISSET(UNIQUEMDFN)) {
                    const char * csum_result_compressed =
			rpmGetPath(repo->outputdir, "/", repo->tempdir, "/",
				db_compressed_sums[*typep], "-", *typep, ".sqlite.bz2", NULL);
                    xx = Rename(result_compressed, csum_result_compressed);
		    result_compressed = _free(result_compressed);
		    result_compressed = csum_result_compressed;
		}

                /* timestamp the compressed file */
		(void) rpmioExists(result_compressed, st)
                db_timestamp = os.stat(result_compressed)[8]

                /* add this data as a section to the repomdxml */
                db_data_type = rpmExpand(*typep, "_db", NULL);
                data = reporoot.newChild(None, 'data', None)
                data.newProp('type', db_data_type)
                location = data.newChild(None, 'location', None)
                if (repo->baseurl != NULL) {
                    location.newProp('xml:base', repo->baseurl)
		}

                location.newProp('href', rpmGetPath(repo->finaldir, "/", *typep, ".sqlite.bz2", NULL));
                checksum = data.newChild(None, 'checksum', db_compressed_sums[*typep])
                checksum.newProp('type', algo2tagname(repo->algo))
                db_tstamp = data.newChild(None, 'timestamp', str(db_timestamp))
                unchecksum = data.newChild(None, 'open-checksum', db_csums[*typep])
                unchecksum.newProp('type', algo2tagname(repo->algo))
                database_version = data.newChild(None, 'database_version', dbversion)
                if (repo->verbose) {
		   time_t now = time(NULL);
                   repo_error(0, _("Ending %s db creation: %s"),
			*typep, ctime(&now));
		}
	    }

            data = reporoot.newChild(None, 'data', None)
            data.newProp('type', *typep)

            checksum = data.newChild(None, 'checksum', csum)
            checksum.newProp('type', algo2tagname(repo->algo))
            timestamp = data.newChild(None, 'timestamp', str(timestamp))
            unchecksum = data.newChild(None, 'open-checksum', uncsum)
            unchecksum.newProp('type', algo2tagname(repo->algo))
            location = data.newChild(None, 'location', None)
            if (repo->baseurl != NULL)
                location.newProp('xml:base', repo->baseurl)
            if (REPO_ISSET(UNIQUEMDFN)) {
                orig_file = repoGetPath(repo, repo->tempdir, *typep, strcmp(*typep, "repomd"));
                res_file = rpmExpand(csum, "-", *typep,
			(repo->markup ? repo->markup : ""),
			(repo->suffix && strcmp(*typep, "repomd") ? repo->suffix : ""), NULL);
                dest_file = rpmGetPath(repo->outputdir, "/", repo->tempdir, "/", res_file, NULL);
                xx = Rename(orig_file, dest_file);

            } else
                res_file = rpmExpand(*typep,
			(repo->markup ? repo->markup : ""),
			(repo->suffix && strcmp(*typep, "repomd") ? repo->suffix : ""), NULL);

            location.newProp('href', rpmGetPath(repo->finaldir, "/", res_file, NULL));
	}
  }

        if (!repo->quiet && REPO_ISSET(DATABASE))
	    repo_error(0, _("Sqlite DBs complete"));

        if (repo->groupfile != NULL) {
            self.addArbitraryMetadata(repo->groupfile, 'group_gz', reporoot)
            self.addArbitraryMetadata(repo->groupfile, 'group', reporoot, compress=False)
	}

        /* save it down */
        try:
            repodoc.saveFormatFileEnc(fn, 'UTF-8', 1)
        except:
            repo_error(0, _("Error saving temp file for %s%s%s: %s"),
		rfile->type,
		(repo->markup ? repo->markup : ""),
		(repo->suffix && strcmp(*typep, "repomd") ? repo->suffix : ""),
		fn);
            repo_error(1, _("Could not save temp file: %s"), fn);

        del repodoc
#endif

    return rc;
}

/**
 * Rename temporary repository to final paths.
 * @param repo		repository
 * @return		0 always
 */
static int repoDoFinalMove(rpmrepo repo)
	/*@globals h_errno, rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext, fileSystem, internalState @*/
{
    char * output_final_dir =
		rpmGetPath(repo->outputdir, "/", repo->finaldir, NULL);
    char * output_old_dir =
		rpmGetPath(repo->outputdir, "/", repo->olddir, NULL);
    struct stat sb, *st = &sb;
    int xx;

    if (rpmioExists(output_final_dir, st)) {
	if ((xx = Rename(output_final_dir, output_old_dir)) != 0)
	    repo_error(1, _("Error moving final %s to old dir %s"),
			output_final_dir, output_old_dir);
    }

    {	const char * output_temp_dir =
		rpmGetPath(repo->outputdir, "/", repo->tempdir, NULL);
	if ((xx = Rename(output_temp_dir, output_final_dir)) != 0) {
	    xx = Rename(output_old_dir, output_final_dir);
	    repo_error(1, _("Error moving final metadata into place"));
	}
	output_temp_dir = _free(output_temp_dir);
    }

    /* Merge old tree into new, unlink/rename as needed. */
  { 
    char *const _av[] = { output_old_dir, NULL };
    int _ftsoptions = FTS_NOCHDIR | FTS_PHYSICAL | FTS_XDEV;
    int (*_compar)(const FTSENT **, const FTSENT **) = NULL;
    FTS * t = Fts_open(_av, _ftsoptions, _compar);
    FTSENT * p = NULL;

    if (t != NULL)
    while ((p = Fts_read(t)) != NULL) {
	const char * opath = p->fts_accpath;
	const char * ofn = p->fts_path;
	const char * obn = p->fts_name;
	const char * nfn = NULL;
	switch (p->fts_info) {
	default:
	    break;
	case FTS_DP:
	    /* Remove empty directories on post-traversal visit. */
	    if ((xx = Rmdir(opath)) != 0)
		repo_error(1, _("Could not remove old metadata directory: %s: %s"),
				ofn, strerror(errno));
	    break;
	case FTS_F:
	    /* Remove all non-toplevel files. */
	    if (p->fts_level > 0) {
		if ((xx = Unlink(opath)) != 0)
		    repo_error(1, _("Could not remove old metadata file: %s: %s"),
				ofn, strerror(errno));
		break;
	    }

	    /* Remove/rename toplevel files, dependent on target existence. */
	    nfn = rpmGetPath(output_final_dir, "/", obn, NULL);
	    if (rpmioExists(nfn, st)) {
		if ((xx = Unlink(opath)) != 0)
		    repo_error(1, _("Could not remove old metadata file: %s: %s"),
				ofn, strerror(errno));
	    } else {
		if ((xx = Rename(opath, nfn)) != 0)
		    repo_error(1, _("Could not restore old non-metadata file: %s -> %s: %s"),
				ofn, nfn, strerror(errno));
	    }
	    nfn = _free(nfn);
	    break;
	case FTS_SL:
	case FTS_SLNONE:
	    /* Remove all symlinks. */
	    if ((xx = Unlink(opath)) != 0)
		repo_error(1, _("Could not remove old metadata symlink: %s: %s"),
				ofn, strerror(errno));
	    break;
	}
    }
    if (t != NULL)
        Fts_close(t);
  }

    output_old_dir = _free(output_old_dir);
    output_final_dir = _free(output_final_dir);

    return 0;
}

/*==============================================================*/

/*@unchecked@*/
static int compression = -1;

/*@unchecked@*/ /*@observer@*/
static struct poptOption repoCompressionPoptTable[] = {
 { "uncompressed", '\0', POPT_ARG_VAL,		&compression, 0,
	N_("don't compress"), NULL },
 { "gzip", 'Z', POPT_ARG_VAL,			&compression, 1,
	N_("use gzip compression"), NULL },
 { "bzip2", '\0', POPT_ARG_VAL,			&compression, 2,
	N_("use bzip2 compression"), NULL },
 { "lzma", '\0', POPT_ARG_VAL,			&compression, 3,
	N_("use lzma compression"), NULL },
 { "xz", '\0', POPT_ARG_VAL,			&compression, 4,
	N_("use xz compression"), NULL },
  POPT_TABLEEND
};

/*@unchecked@*/ /*@observer@*/
static struct poptOption rpmrepoOptionsTable[] = {

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, _rpmrepoOptions, 0,
	N_("Repository options:"), NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioFtsPoptTable, 0,
	N_("Fts(3) traversal options:"), NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, repoCompressionPoptTable, 0,
	N_("Available compressions:"), NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioDigestPoptTable, 0,
	N_("Available digests:"), NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"),
	NULL },

  POPT_AUTOALIAS
  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, char *argv[])
	/*@globals _rpmrepo,
		rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies _rpmrepo,
		rpmGlobalMacroContext, fileSystem, internalState @*/
{
    rpmrepo repo = _rpmrepo;
    poptContext optCon;
    const char ** av = NULL;
    int ndirs = 0;
    int nfiles = 0;
    int rc = 1;		/* assume failure. */
    int xx;
    int i;

#if !defined(__LCLINT__)	/* XXX force "rpmrepo" name. */
    __progname = "rpmrepo";
#endif

    /* Process options. */
    optCon = rpmioInit(argc, argv, rpmrepoOptionsTable);

    /* XXX Impedanace match against poptIO common code. */
    if (rpmIsVerbose())
	repo->verbose++;
    if (rpmIsDebug())
	repo->verbose++;

    repo->ftsoptions = (rpmioFtsOpts ? rpmioFtsOpts : FTS_PHYSICAL);
    switch (repo->ftsoptions & (FTS_LOGICAL|FTS_PHYSICAL)) {
    case (FTS_LOGICAL|FTS_PHYSICAL):
	repo_error(1, "FTS_LOGICAL and FTS_PYSICAL are mutually exclusive");
        /*@notreached@*/ break;
    case 0:
        repo->ftsoptions |= FTS_PHYSICAL;
        break;
    }

    repo->algo = (rpmioDigestHashAlgo >= 0 ? (rpmioDigestHashAlgo & 0xff)  : PGPHASHALGO_SHA1);

    repo->compression = (compression >= 0 ? compression : 1);
    switch (repo->compression) {
    case 0:
	repo->suffix = NULL;
	repo->wmode = "w.ufdio";
	break;
    default:
	/*@fallthrough@*/
    case 1:
	repo->suffix = ".gz";
	repo->wmode = "w9.gzdio";
	break;
    case 2:
	repo->suffix = ".bz2";
	repo->wmode = "w9.bzdio";
	break;
    case 3:
	repo->suffix = ".lzma";
	repo->wmode = "w.lzdio";
	break;
    case 4:
	repo->suffix = ".xz";
	repo->wmode = "w.xzdio";
	break;
    }

    av = poptGetArgs(optCon);
    if (av == NULL || av[0] == NULL) {
	repo_error(0, _("Must specify path(s) to index."));
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }

    if (av != NULL)
    for (i = 0; av[i] != NULL; i++) {
	char fullpath[MAXPATHLEN];
	struct stat sb;
	const char * rpath;
	const char * lpath = NULL;
	int ut = urlPath(av[i], &lpath);
	size_t nb = (size_t)(lpath - av[i]);
	int isdir = (lpath[strlen(lpath)-1] == '/');
	
	/* Convert to absolute/clean/malloc'd path. */
	if (lpath[0] != '/') {
	    if ((rpath = repoRealpath(lpath)) == NULL)
		repo_error(1, _("Realpath(%s): %s"), lpath, strerror(errno));
	    lpath = rpmGetPath(rpath, NULL);
	    rpath = _free(rpath);
	} else
	    lpath = rpmGetPath(lpath, NULL);

	/* Reattach the URI to the absolute/clean path. */
	/* XXX todo: rpmGenPath was confused by file:///path/file URI's. */
	switch (ut) {
	case URL_IS_DASH:
	case URL_IS_UNKNOWN:
	    rpath = lpath;
	    lpath = NULL;
	    /*@switchbreak@*/ break;
	default:
assert(nb < sizeof(fullpath));
	    strncpy(fullpath, av[i], nb);
	    fullpath[nb] = '\0';
	    rpath = rpmGenPath(fullpath, lpath, NULL);
	    lpath = _free(lpath);
	    /*@switchbreak@*/ break;
	}

	/* Add a trailing '/' on directories. */
	lpath = (isdir || (!Stat(rpath, &sb) && S_ISDIR(sb.st_mode))
		? "/" : NULL);
	if (lpath != NULL) {
	    lpath = rpmExpand(rpath, lpath, NULL);
	    xx = argvAdd(&repo->directories, lpath);
	    lpath = _free(lpath);
	    ndirs++;
	} else {
	    xx = argvAdd(&repo->pkglist, rpath);
	    nfiles++;
	}
	rpath = _free(rpath);
    }

if (_rpmrepo_debug || REPO_ISSET(DRYRUN))
argvPrint("repo->directories", repo->directories, NULL);

#ifdef	NOTYET
    if (repo->basedir == NULL)
	repo->basedir = xstrdup(repo->directories[0]);
#endif

    if (repo->outputdir == NULL) {
	if (repo->directories != NULL && repo->directories[0] != NULL)
	    repo->outputdir = xstrdup(repo->directories[0]);
	else {
	    repo->outputdir = repoRealpath(".");
	    if (repo->outputdir == NULL)
		repo_error(1, _("Realpath(%s): %s"), ".", strerror(errno));
	}
    }

    if (REPO_ISSET(SPLIT) && REPO_ISSET(CHECKTS))
	repo_error(1, _("--split and --checkts options are mutually exclusive"));

#ifdef	NOTYET
    /* Add manifest(s) contents to rpm list. */
    if (repo->manifests != NULL) {
	const char ** av = repo->manifests;
	const char * fn;
	/* Load the rpm list from manifest(s). */
	while ((fn = *av++) != NULL) {
	    /* XXX todo: parse paths from files. */
	    /* XXX todo: convert to absolute paths. */
	    /* XXX todo: check for existence. */
	    xx = argvAdd(&repo->pkglist, fn);
	}
    }
#endif

    /* Set up mire patterns (no error returns with globs, easy pie). */
    if (mireLoadPatterns(RPMMIRE_GLOB, 0, repo->exclude_patterns, NULL,
                &repo->excludeMire, &repo->nexcludes))
	repo_error(1, _("Error loading exclude glob patterns."));
    if (mireLoadPatterns(RPMMIRE_GLOB, 0, repo->include_patterns, NULL,
                &repo->includeMire, &repo->nincludes))
	repo_error(1, _("Error loading include glob patterns."));

    /* Load the rpm list from a multi-rooted directory traversal. */
    if (repo->directories != NULL) {
	ARGV_t pkglist = repoGetFileList(repo, repo->directories, ".rpm");
	xx = argvAppend(&repo->pkglist, pkglist);
	pkglist = argvFree(pkglist);
    }

    /* XXX todo: check for duplicates in repo->pkglist? */
    xx = argvSort(repo->pkglist, NULL);

if (_rpmrepo_debug || REPO_ISSET(DRYRUN))
argvPrint("repo->pkglist", repo->pkglist, NULL);

    repo->pkgcount = argvCount(repo->pkglist);

    /* XXX enable --stats using transaction set. */
    {	rpmts ts = repo->_ts;
	_rpmts_stats = _rpmsw_stats;
	repo->_ts = ts = rpmtsCreate();

    /* XXX todo wire up usual rpm CLI options. hotwire --nosignature for now */
	(void) rpmtsSetVSFlags(ts, _RPMVSF_NOSIGNATURES);
    }

    rc = repoTestSetupDirs(repo);
	
    if (rc || REPO_ISSET(DRYRUN))
	goto exit;

    if (!REPO_ISSET(SPLIT)) {
	rc = repoCheckTimeStamps(repo);
	if (rc == 0) {
	    fprintf(stdout, _("repo is up to date\n"));
	    goto exit;
	}
    }

    if ((rc = repoDoPkgMetadata(repo)) != 0)
	goto exit;
    if ((rc = repoDoRepoMetadata(repo)) != 0)
	goto exit;
    if ((rc = repoDoFinalMove(repo)) != 0)
	goto exit;

exit:
    {	rpmts ts = repo->_ts;
	(void) rpmtsFree(ts); 
	repo->_ts = NULL;
    }

    repo->primary.digest = _free(repo->primary.digest);
    repo->primary.Zdigest = _free(repo->primary.Zdigest);
    repo->filelists.digest = _free(repo->filelists.digest);
    repo->filelists.Zdigest = _free(repo->filelists.Zdigest);
    repo->other.digest = _free(repo->other.digest);
    repo->other.Zdigest = _free(repo->other.Zdigest);
    repo->repomd.digest = _free(repo->repomd.digest);
    repo->repomd.Zdigest = _free(repo->repomd.Zdigest);
    repo->outputdir = _free(repo->outputdir);
    repo->pkglist = argvFree(repo->pkglist);
    repo->directories = argvFree(repo->directories);
    repo->manifests = argvFree(repo->manifests);
/*@-onlytrans -refcounttrans @*/
    repo->excludeMire = mireFreeAll(repo->excludeMire, repo->nexcludes);
    repo->includeMire = mireFreeAll(repo->includeMire, repo->nincludes);
/*@=onlytrans =refcounttrans @*/
    repo->exclude_patterns = argvFree(repo->exclude_patterns);
    repo->include_patterns = argvFree(repo->include_patterns);

    tagClean(NULL);
    optCon = rpmioFini(optCon);

    return rc;
}

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

/*==============================================================*/

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
	rpmrepoError(1, _("headerSprintf(%s): %s"), qfmt, msg);
assert(s != NULL);
    return s;
}

#if defined(WITH_SQLITE)
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
	rpmrepoError(1, _("headerSprintf(%s): %s"), qfmt, msg);
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
#endif

/**
 * Export a single package's metadata to repository metadata file(s).
 * @param repo		repository
 * @param rfile		repository metadata file
 * @param h		header
 * @return		0 on success
 */
static int rpmrepoWriteMDFile(rpmrepo repo, rpmrfile rfile, Header h)
	/*@globals fileSystem @*/
	/*@modifies rfile, h, fileSystem @*/
{
    int rc = 0;

    if (rfile->xml_qfmt != NULL) {
	if (rpmrfileXMLWrite(rfile, rfileHeaderSprintf(h, rfile->xml_qfmt)))
	    rc = 1;
    }

#if defined(WITH_SQLITE)
    if (REPO_ISSET(DATABASE)) {
	if (rpmrfileSQLWrite(rfile, rfileHeaderSprintfHack(h, rfile->sql_qfmt)))
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
	if (rpmrepoWriteMDFile(repo, &repo->primary, h)
	 || rpmrepoWriteMDFile(repo, &repo->filelists, h)
	 || rpmrepoWriteMDFile(repo, &repo->other, h))
	    rc = 1;

	(void) headerFree(h);
	h = NULL;
	if (rc) break;

	if (!repo->quiet) {
	    if (repo->verbose)
		rpmrepoError(0, "%d/%d - %s", repo->current, repo->pkgcount, pkg);
	    else
		rpmrepoProgress(repo, pkg, repo->current, repo->pkgcount);
	}
    }
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

	filematrix[mydir] = rpmrepoGetFileList(repo, roots, '.rpm')
	self.trimRpms(filematrix[mydir])
	repo->pkgcount = argvCount(filematrix[mydir]);
	roots = argvFree(roots);
    }

    mediano = 1;
    repo->baseurl = self._getFragmentUrl(repo->baseurl, mediano)
#endif

    if (rpmrepoOpenMDFile(repo, &repo->primary)
     || rpmrepoOpenMDFile(repo, &repo->filelists)
     || rpmrepoOpenMDFile(repo, &repo->other))
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
    if (rpmrepoCloseMDFile(repo, &repo->primary)
     || rpmrepoCloseMDFile(repo, &repo->filelists)
     || rpmrepoCloseMDFile(repo, &repo->other))
	rc = 1;

    return rc;
}

/*==============================================================*/

int
main(int argc, char *argv[])
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext, fileSystem, internalState @*/
{
    rpmrepo repo;
    int rc = 1;		/* assume failure. */
    int xx;

#if !defined(__LCLINT__)	/* XXX force "rpmrepo" name. */
    __progname = "rpmrepo";
#endif
    repo = rpmrepoNew(argv, 0);
    if (repo == NULL)
	goto exit;

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
	    repo->outputdir = rpmrepoRealpath(".");
	    if (repo->outputdir == NULL)
		rpmrepoError(1, _("Realpath(%s): %s"), ".", strerror(errno));
	}
    }

    if (REPO_ISSET(SPLIT) && REPO_ISSET(CHECKTS))
	rpmrepoError(1, _("--split and --checkts options are mutually exclusive"));

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
	rpmrepoError(1, _("Error loading exclude glob patterns."));
    if (mireLoadPatterns(RPMMIRE_GLOB, 0, repo->include_patterns, NULL,
                &repo->includeMire, &repo->nincludes))
	rpmrepoError(1, _("Error loading include glob patterns."));

    /* Load the rpm list from a multi-rooted directory traversal. */
    if (repo->directories != NULL) {
	ARGV_t pkglist = rpmrepoGetFileList(repo, repo->directories, ".rpm");
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

    rc = rpmrepoTestSetupDirs(repo);
	
    if (rc || REPO_ISSET(DRYRUN))
	goto exit;

    if (!REPO_ISSET(SPLIT)) {
	rc = rpmrepoCheckTimeStamps(repo);
	if (rc == 0) {
	    fprintf(stdout, _("repo is up to date\n"));
	    goto exit;
	}
    }

    if ((rc = repoDoPkgMetadata(repo)) != 0)
	goto exit;
    if ((rc = rpmrepoDoRepoMetadata(repo)) != 0)
	goto exit;
    if ((rc = rpmrepoDoFinalMove(repo)) != 0)
	goto exit;

exit:
    {	rpmts ts = repo->_ts;
	(void) rpmtsFree(ts); 
	repo->_ts = NULL;
    }

    repo = rpmrepoFree(repo);

    tagClean(NULL);

    return rc;
}

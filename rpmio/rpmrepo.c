/**
 * \file rpmio/rpmrepo.c
 */

#include "system.h"

#include <rpmio_internal.h>	/* XXX fdInitDigest() et al */
#include <fts.h>
#include <argv.h>
#include <mire.h>
#include <poptIO.h>

#include <rpmtag.h>
#include <rpmlib.h>		/* XXX for rpmts typedef */
#include <rpmts.h>

#include "debug.h"

/*==============================================================*/

int _repo_debug;

typedef struct rpmrepo_s * rpmrepo;
typedef struct rpmrfile_s * rpmrfile;

struct rpmrfile_s {
    const char * type;
    const char * init;
    const char * qfmt;
    const char * fini;
    FD_t fd;
    const char * digest;
    time_t ctime;
};

struct rpmrepo_s {
    int quiet;
    int verbose;
    int dryrun;
/*@null@*/
    ARGV_t exclude_patterns;
/*@null@*/
    miRE excludeMire;
    int nexcludes;
/*@null@*/
    ARGV_t include_patterns;
/*@null@*/
    miRE includeMire;
    int nincludes;
#ifdef	NOTYET
/*@null@*/
    const char * basedir;
/*@null@*/
    const char * baseurl;
/*@null@*/
    const char * groupfile;
#endif
    int split;
    int database;
    int pretty;
    int checkts;
/*@null@*/
    const char * outputdir;

    int skipsymlinks;
/*@null@*/
    ARGV_t manifests;

/*@null@*/
    const char * tempdir;
/*@null@*/
    const char * finaldir;
/*@null@*/
    const char * olddir;

    time_t mdtimestamp;

    int uniquemdfilenames;

/*@null@*/
    rpmts ts;
/*@null@*/
    ARGV_t pkglist;
    int pkgcount;

/*@null@*/
    ARGV_t directories;
    int ftsoptions;
    uint32_t algo;
    int compression;
    const char * markup;
    const char * suffix;
    const char * wmode;

    struct rpmrfile_s primary;
    struct rpmrfile_s filelists;
    struct rpmrfile_s other;
    struct rpmrfile_s repomd;

};

static const char init_primary[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<metadata xmlns=\"http://linux.duke.edu/metadata/common\" xmlns:rpm=\"http://linux.duke.edu/metadata/rpm\" packages=\"0\">\n";
static const char fini_primary[] = "</metadata>\n";

static const char init_filelists[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<filelists xmlns=\"http://linux.duke.edu/metadata/filelists\" packages=\"0\">\n";
static const char fini_filelists[] = "</filelists>\n";

static const char init_other[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<otherdata xmlns=\"http://linux.duke.edu/metadata/other\" packages=\"0\">\n";
static const char fini_other[] = "</otherdata>\n";

static const char init_repomd[] = "\
<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\
<repomd xmlns=\"http://linux.duke.edu/metadata/repo\">\n";
static const char fini_repomd[] = "</repomd>\n";

/* XXX todo: wire up popt aliases and bury the --queryformat glop externally. */
/*@unchecked@*/ /*@observer@*/
static const char qfmt_primary[] = "\
<package type=\"rpm\">\n\
  <name>%{NAME:cdata}</name>\n\
  <arch>%{ARCH:cdata}</arch>\n\
  <version epoch=\"%|EPOCH?{%{EPOCH}}:{0}|\" ver=\"%{VERSION:cdata}\" rel=\"%{RELEASE:cdata}\"/>\n\
  <checksum type=\"sha\" pkgid=\"NO\">%|HDRID?{%{HDRID}}|</checksum>\n\
  <summary>%{SUMMARY:cdata}</summary>\n\
  <description>%{DESCRIPTION:cdata}</description>\n\
  <packager>%|PACKAGER?{%{PACKAGER:cdata}}:{}|</packager>\n\
  <url>%|URL?{%{URL:cdata}}:{}|</url>\n\
  <time file=\"%{PACKAGETIME}\" build=\"%{BUILDTIME}\"/>\n\
  <size package=\"%{SIZE}\" installed=\"%{SIZE}\" archive=\"%{ARCHIVESIZE}\"/>\n\
  <location href=\"%{PACKAGEORIGIN:cdata}\"/>\n\
  <format>\n\
%|license?{\
    <rpm:license>%{LICENSE:cdata}</rpm:license>\n\
}:{\
    <rpm:license/>\n\
}|\
%|vendor?{\
    <rpm:vendor>%{VENDOR:cdata}</rpm:vendor>\n\
}:{\
    <rpm:vendor/>\n\
}|\
%|group?{\
    <rpm:group>%{GROUP:cdata}</rpm:group>\n\
}:{\
    <rpm:group/>\n\
}|\
%|buildhost?{\
    <rpm:buildhost>%{BUILDHOST:cdata}</rpm:buildhost>\n\
}:{\
    <rpm:buildhost/>\n\
}|\
%|sourcerpm?{\
    <rpm:sourcerpm>%{SOURCERPM:cdata}</rpm:sourcerpm>\n\
}|\
    <rpm:header-range start=\"%{HEADERSTARTOFF}\" end=\"%{HEADERENDOFF}\"/>\n\
%|provideentry?{\
    <rpm:provides>\n\
[\
      %{provideentry}\n\
]\
    </rpm:provides>\n\
}:{\
    <rpm:provides/>\n\
}|\
%|requireentry?{\
    <rpm:requires>\n\
[\
      %{requireentry}\n\
]\
    </rpm:requires>\n\
}:{\
    <rpm:requires/>\n\
}|\
%|conflictentry?{\
    <rpm:conflicts>\n\
[\
      %{conflictentry}\n\
]\
    </rpm:conflicts>\n\
}:{\
    <rpm:conflicts/>\n\
}|\
%|obsoleteentry?{\
    <rpm:obsoletes>\n\
[\
      %{obsoleteentry}\n\
]\
    </rpm:obsoletes>\n\
}:{\
    <rpm:obsoletes/>\n\
}|\
%|filesentry1?{\
[\
    %{filesentry1}\n\
]\
}|\
  </format>\n\
</package>\n\
";

/*@unchecked@*/ /*@observer@*/
static const char qfmt_filelists[] = "\
<package pkgid=\"%|HDRID?{%{HDRID}}:{XXX}|\" name=\"%{NAME:cdata}\" arch=\"%{ARCH:cdata}\">\n\
  <version epoch=\"%|EPOCH?{%{EPOCH}}:{0}|\" ver=\"%{VERSION:cdata}\" rel=\"%{RELEASE:cdata}\"/>\n\
%|filesentry2?{\
[\
  %{filesentry2}\n\
]\
}|\
</package>\n\
";

/*@unchecked@*/ /*@observer@*/
static const char qfmt_other[] = "\
<package pkgid=\"%|HDRID?{%{HDRID}}:{XXX}|\" name=\"%{NAME:cdata}\" arch=\"%{ARCH:cdata}\">\n\
  <version epoch=\"%|EPOCH?{%{EPOCH}}:{0}|\" ver=\"%{VERSION:cdata}\" rel=\"%{RELEASE:cdata}\"/>\n\
%|changelogname?{\
[\
  <changelog author=\"%{CHANGELOGNAME:cdata}\" date=\"%{CHANGELOGTIME}\">%{CHANGELOGTEXT:cdata}</changelog>\n\
]\
}:{\
  <changelog/>\n\
}|\
</package>\n\
";

/*@unchecked@*/
static struct rpmrepo_s __rpmrepo = {
    .pretty	= 1,
    .tempdir	= ".repodata",
    .finaldir	= "repodata",
    .olddir	= ".olddata",
    .markup	= ".xml",
    .algo	= PGPHASHALGO_SHA1,
    .primary	= {
	.type	= "primary",
	.init	= init_primary,
	.qfmt	= qfmt_primary,
	.fini	= fini_primary
    },
    .filelists	= {
	.type	= "filelists",
	.init	= init_filelists,
	.qfmt	= qfmt_filelists,
	.fini	= fini_filelists
    },
    .other	= {
	.type	= "other",
	.init	= init_other,
	.qfmt	= qfmt_other,
	.fini	= fini_other
    },
    .repomd	= {
	.type	= "repomd",
	.init	= init_repomd,
	.qfmt	= NULL,
	.fini	= fini_repomd
    }
};

/*@unchecked@*/
static rpmrepo _rpmrepo = &__rpmrepo;

/*==============================================================*/
/*@exist@*/
static void
repo_error(int lvl, const char *fmt, ...)
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

static void repoProgress(rpmrepo repo, const char * item,
		int current, int total)
	/*@*/
{
    static size_t ncols = 80 - 1;	/* XXX TIOCGWINSIZ */
    size_t nb = fprintf(stdout, "\r%s: %d/%d - %s", __progname, current, total, item);
    if (nb < ncols)
	fprintf(stderr, "%*s", (ncols - nb), "");
    ncols = nb;
    fflush(stdout);
}

static int rpmioExists(const char * fn, struct stat * st)
	/*@*/
{
    return (Stat(fn, st) == 0);
}

static time_t rpmioCtime(const char * fn)
	/*@*/
{
    struct stat sb;
    time_t stctime = 0;

    if (rpmioExists(fn, &sb))
	stctime = sb.st_ctime;
    return stctime;
}

/*@null@*/
static const char * repoRealpath(const char * lpath)
	/*@*/
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
static int repoMkdir(rpmrepo repo, const char * dn)
	/*@*/
{
    const char * dnurl = rpmGetPath(repo->outputdir, "/", dn, NULL);
    int ut = urlPath(dnurl, &dn);
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

static const char * repoGetPath(rpmrepo repo, const char * dir,
		const char * type, int compress)
	/*@*/
{
    return rpmGetPath(repo->outputdir, "/", dir, "/", type,
		(repo->markup ? repo->markup : ""),
		(repo->suffix && compress ? repo->suffix : ""), NULL);
}

static int repoTestSetupDirs(rpmrepo repo)
	/*@modifies repo @*/
{
    const char ** dirs = repo->directories;
    struct stat sb, *st = &sb;
    const char * dn;
    const char * fn;
    int rc = 0;
    int xx;

if (_repo_debug)
fprintf(stderr, "\trepoTestSetupDirs(%p)\n", repo);

    /* XXX todo: check repo->pkglist existence? */

    if (dirs != NULL)
    while ((dn = *dirs++) != NULL) {
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

    if ((xx = repoMkdir(repo, repo->tempdir)) != 0)
	rc = 1;
    if ((xx = repoMkdir(repo, repo->finaldir)) != 0)
	rc = 1;

    dn = rpmGetPath(repo->outputdir, "/", repo->olddir, NULL);
    if (rpmioExists(dn, st)) {
	repo_error(0, _("Old data directory exists, please remove: %s"), dn);
	rc = 1;
    }
    dn = _free(dn);

  { static const char * dirs[] = { ".repodata", "repodata", NULL };
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
		if (repo->checkts && st->st_ctime > repo->mdtimestamp)
		    repo->mdtimestamp = st->st_ctime;
	    }
	    fn = _free(fn);
	}
    }
  }

#ifdef	NOTYET		/* XXX repo->package_dir needs to go away. */
    if (repo->groupfile != NULL) {
	if (repo->split || repo->groupfile[0] != '/') {
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

static int repoMetaDataGenerator(rpmrepo repo)
	/*@modifies repo @*/
{
    int rc = 0;

if (_repo_debug)
fprintf(stderr, "==> repoMetaDataGenerator(%p)\n", repo);

    repo->ts = rpmtsCreate();
    /* XXX todo wire up usual rpm CLI options. hotwire --nosignature for now */
    (void) rpmtsSetVSFlags(repo->ts, _RPMVSF_NOSIGNATURES);

    /* XXX repoParseDirectory(repo); */
#ifdef	NOTYET
    if (repo->basedir == NULL)
	repo->basedir = xstrdup(repo->directories[0]);
#endif
    if (repo->outputdir == NULL)
	repo->outputdir = xstrdup(repo->directories[0]);

    rc = repoTestSetupDirs(repo);
	
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

/*@null@*/
static const char ** repoGetFileList(rpmrepo repo, const char *roots[],
		const char * ext)
	/*@*/
{
    const char ** files = NULL;
    FTS * t;
    FTSENT * p;
    int xx;

if (_repo_debug)
fprintf(stderr, "\trepoGetFileList(%p, %p, %s) directory %s\n", repo, roots, ext, roots[0]);

    if ((t = Fts_open((char *const *)roots, repo->ftsoptions, NULL)) == NULL)
	repo_error(1, _("Fts_open: %s"), strerror(errno));

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
	    /* Is this a *.rpm file? */
	    if (!chkSuffix(p->fts_name, ext))
		continue;

	    /* Should this file be excluded/included? */
	    /* XXX todo: apply globs to fts_path rather than fts_name? */
/*@-onlytrans@*/
	    if (mireApply(repo->excludeMire, repo->nexcludes, p->fts_name, 0, -1) >= 0)
		continue;
	    if (mireApply(repo->includeMire, repo->nincludes, p->fts_name, 0, +1) < 0)
		continue;
/*@=onlytrans@*/

	    xx = argvAdd(&files, p->fts_path);
	    break;
	}
    }

if (_repo_debug)
argvPrint("files", files, NULL);

    return files;
}

static int repoCheckTimeStamps(rpmrepo repo)
	/*@*/
{
    int rc = 0;

if (_repo_debug)
fprintf(stderr, "\trepoCheckTimeStamps(%p)\n", repo);

    if (repo->checkts) {
	const char ** pkg;

	if (repo->pkglist != NULL)
	for (pkg = repo->pkglist; *pkg; pkg++) {
	    struct stat sb, *st = &sb;
	    if (!rpmioExists(*pkg, st))
		repo_error(0, _("cannot get to file: %s"), *pkg);
	    else if (st->st_ctime > repo->mdtimestamp)
		rc = 1;
	}
    } else
	rc = 1;

    return rc;
}

static int repoOpenMDFile(rpmrepo repo, rpmrfile rfile)
	/*@modifies repo @*/
{
    const char * spew = rfile->init;
    size_t nspew = strlen(spew);
    const char * fn = repoGetPath(repo, repo->tempdir, rfile->type, 1);
    const char * tail;
    size_t nb;

    rfile->fd = Fopen(fn, repo->wmode);
    if (repo->algo > 0)
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
    if (nspew != nb)
	repo_error(1, _("Fwrite failed: expected write %u != %u bytes: %s\n"),
		(unsigned)nspew, (unsigned)nb, Fstrerror(rfile->fd));

if (_repo_debug)
fprintf(stderr, "\trepoOpenMDFile(%p, %p) %s nb %u\n", repo, rfile, fn, (unsigned)nb);

    fn = _free(fn);
    return 0;
}

static Header repoReadHeader(rpmrepo repo, const char * path)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies repo, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    FD_t fd = Fopen(path, "r.ufdio");
    Header h = NULL;

    if (fd != NULL) {
	/* XXX what if path needs expansion? */
	rpmRC rpmrc = rpmReadPackageFile(repo->ts, fd, path, &h);

	(void) Fclose(fd);

	switch (rpmrc) {
	case RPMRC_NOTFOUND:
	    /* XXX Read a package manifest. Restart ftswalk on success. */
	case RPMRC_FAIL:
	default:
	    h = headerFree(h);
	    break;
	case RPMRC_NOTTRUSTED:
	case RPMRC_NOKEY:
	case RPMRC_OK:
	    break;
	}
    }
    return h;
}

/*@null@*/
static Header repoReadPackage(rpmrepo repo, const char * rpmfile)
	/*@*/
{
    Header h = NULL;

if (_repo_debug)
fprintf(stderr, "\trepoReadPackage(%p, %s)\n", repo, rpmfile);

    /*
     * TODO/FIXME
     * consider adding a routine to download the package from a remote location
     * to a tempdir, operate on it, then use that location as a the baseurl
     * for the package. That would make it possible to have repos entirely 
     * comprised of remote packages.
     */

    h = repoReadHeader(repo, rpmfile);

#ifdef	NOTYET
        try:
            po = yumbased.CreateRepoPackage(self.ts, rpmfile)
        except Errors.MiscError, e:
            raise MDError, "Unable to open package: %s" % e

    /*
     * if we are going to add anything in from outside, here is where
     * you can do it
     */

    /*
     * FIXME if we wanted to put in a baseurl-per-package here is where 
     * we should do it
     * it would be easy to have a lookup dict in the MetaDataConfig object
     * and work down from there for the baseurl
     */

    if po.checksum in (None, ""):
	repo_error(1, _("No Package ID found for package %s, not going to add it"),
		rpmfile);
#endif

    return h;
}

static int repoWriteMDFile(rpmrepo repo, rpmrfile rfile, Header h)
	/*@modifies repo @*/
{
    const char * qfmt = rfile->qfmt;

    if (qfmt != NULL) {
	const char * msg = NULL;
	const char * spew = headerSprintf(h, qfmt, NULL, NULL, &msg);
	size_t nspew = (spew != NULL ? strlen(spew) : 0);
	size_t nb = (nspew > 0 ? Fwrite(spew, 1, nspew, rfile->fd) : 0);
	if (spew == NULL)
	    repo_error(1, _("incorrect format: %s\n"),
		(msg ? msg : "headerSprintf() msg is AWOL!"));
	if (nspew != nb)
	    repo_error(1,
		_("Fwrite failed: expected write %u != %u bytes: %s\n"),
		(unsigned)nspew, (unsigned)nb, Fstrerror(rfile->fd));
	spew = _free(spew);
    }
    return 0;
}

static unsigned repoWriteMetadataDocs(rpmrepo repo,
		/*@null@*/ const char ** pkglist, unsigned current)
	/*@modifies repo @*/
{
    const char * pkg;

if (_repo_debug)
fprintf(stderr, "\trepoWriteMetadataDocs(%p, %p, %u)\n", repo, pkglist, current);

    while ((pkg = *pkglist++) != NULL) {
	Header h = repoReadPackage(repo, pkg);
	int xx;

	current++;
	if (h == NULL) {
	    repo_error(0, _("\nError %s: %s\n"), pkg, strerror(errno));
	    continue;
	}

#ifdef	NOTYET
	/* XXX todo: rpmGetPath(mydir, "/", filematrix[mydir], NULL); */
	reldir = (pkgpath != NULL ? pkgpath : rpmGetPath(repo->basedir, "/", repo->directories[0], NULL));
	self.primaryfile.write(po.do_primary_xml_dump(reldir, baseurl=repo->baseurl))
	self.flfile.write(po.do_filelists_xml_dump())
	self.otherfile.write(po.do_other_xml_dump())
#endif
	xx = repoWriteMDFile(repo, &repo->primary, h);
	xx = repoWriteMDFile(repo, &repo->filelists, h);
	xx = repoWriteMDFile(repo, &repo->other, h);

	h = headerFree(h);

	if (!repo->quiet) {
	    if (repo->verbose)
		repo_error(0, "%d/%d - %s", current, repo->pkgcount, pkg);
	    else
		repoProgress(repo, pkg, current, repo->pkgcount);
	}
    }

    return current;
}

static int repoCloseMDFile(rpmrepo repo, rpmrfile rfile)
	/*@modifies repo @*/
{
    static int asAscii = 1;
    const char * fn;
    const char * spew = rfile->fini;
    size_t nspew = strlen(spew);
    size_t nb;
    int xx;

    if (!repo->quiet)
	repo_error(0, _("Saving %s%s%s metadata"), rfile->type,
		(repo->markup ? repo->markup : ""),
		(repo->suffix ? repo->suffix : ""));
    nb = Fwrite(spew, 1, nspew, rfile->fd);
    if (repo->algo > 0)
	fdFiniDigest(rfile->fd, repo->algo, &rfile->digest, NULL, asAscii);
    else
	rfile->digest = xstrdup("");
    xx = Fclose(rfile->fd);
    rfile->fd = NULL;

    fn = repoGetPath(repo, repo->tempdir, rfile->type, 1);
    rfile->ctime = rpmioCtime(fn);
    fn = _free(fn);

    return 0;
}

static int repoDoPkgMetadata(rpmrepo repo)
	/*@*/
{
    int xx;

if (_repo_debug)
fprintf(stderr, "==> repoDoPkgMetadata(%p)\n", repo);

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
    current = 0;
    repo->baseurl = self._getFragmentUrl(repo->baseurl, mediano)

    xx = repoOpenMDFile(repo, &repo->primary);
    xx = repoOpenMDFile(repo, &repo->filelists);
    xx = repoOpenMDFile(repo, &repo->other);

    for mydir in repo->directories {
	repo->baseurl = self._getFragmentUrl(repo->baseurl, mediano)
	/* XXX todo: rpmGetPath(mydir, "/", filematrix[mydir], NULL); */
	current = repoWriteMetadataDocs(filematrix[mydir], current)
	mediano++;
    }
    repo->baseurl = self._getFragmentUrl(repo->baseurl, 1)

    if (!repo->quiet)
	fprintf(stderr, "\n");
    xx = repoCloseMDFile(repo, &repo->primary);
    xx = repoCloseMDFile(repo, &repo->filelists);
    xx = repoCloseMDFile(repo, &repo->other);
#else

    xx = repoOpenMDFile(repo, &repo->primary);
    xx = repoOpenMDFile(repo, &repo->filelists);
    xx = repoOpenMDFile(repo, &repo->other);

    repoWriteMetadataDocs(repo, repo->pkglist, 0);

    if (!repo->quiet)
	fprintf(stderr, "\n");
    xx = repoCloseMDFile(repo, &repo->primary);
    xx = repoCloseMDFile(repo, &repo->filelists);
    xx = repoCloseMDFile(repo, &repo->other);

#endif

    return 0;
}

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

static const char * repoMDExpand(rpmrepo repo, rpmrfile rfile)
	/*@*/
{
    const char * spewalgo = algo2tagname(repo->algo);
    char spewtime[64];
    snprintf(spewtime, sizeof(spewtime), "%u", (unsigned)rfile->ctime);
    return rpmExpand("\
  <data type=\"", rfile->type, "\">\n\
    <checksum type=\"", spewalgo, "\">", rfile->digest, "</checksum>\n\
    <timestamp>", spewtime, "</timestamp>\n\
    <open-checksum type=\"",spewalgo,"\">", rfile->digest, "</open-checksum>\n\
    <location href=\"", repo->finaldir, "/", rfile->type, (repo->markup ? repo->markup : ""), (repo->suffix ? repo->suffix : ""), "\"/>\n\
  </data>\n", NULL);
}

static int repoDoRepoMetadata(rpmrepo repo)
	/*@*/
{
    rpmrfile rfile = &repo->repomd;
    const char * fn;
    const char * spew;
    size_t nspew;
    size_t nb;
    FD_t fd;

if (_repo_debug)
fprintf(stderr, "==> repoDoRepoMetadata(%p)\n", repo);

    fn = repoGetPath(repo, repo->tempdir, rfile->type, 0);
    fd = Fopen(fn, "w.ufdio"); /* no compression */
assert(fd != NULL);
    spew = init_repomd;
    nspew = strlen(spew);
    nb = Fwrite(spew, 1, nspew, fd);

    spew = repoMDExpand(repo, &repo->other);
    nspew = strlen(spew);
    nb = Fwrite(spew, 1, nspew, fd);
    spew = _free(spew);

    spew = repoMDExpand(repo, &repo->filelists);
    nspew = strlen(spew);
    nb = Fwrite(spew, 1, nspew, fd);
    spew = _free(spew);

    spew = repoMDExpand(repo, &repo->primary);
    nspew = strlen(spew);
    nb = Fwrite(spew, 1, nspew, fd);
    spew = _free(spew);

    spew = fini_repomd;
    nspew = strlen(spew);
    nb = Fwrite(spew, 1, nspew, fd);

    (void) Fclose(fd);

    fn = _free(fn);

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

        if (repo->database) {
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

            if (repo->database) {
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

                if (repo->uniquemdfilenames) {
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
            if (repo->uniquemdfilenames) {
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

        if (!repo->quiet && repo->database)
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

if (_repo_debug)
fprintf(stderr, "==> repoDoFinalMove(%p)\n", repo);

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

  { static const char * types[] =
	{ "primary", "filelists", "other", "repomd", "group", NULL };
    const char ** typep;

    for (typep = types; *typep != NULL; typep++) {
	oldfile = rpmGetPath(output_old_dir, "/", *typep,
		(repo->markup ? repo->markup : ""),
		(repo->suffix && strcmp(*typep, "repomd") ? repo->suffix : ""), NULL);
	if (rpmioExists(oldfile, st)) {
	    if (Unlink(oldfile))
		repo_error(1, _("Could not remove old metadata file: %s: %s"),
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
		    repo_error(1, _("Could not remove old metadata file: %s: %s"),
				oldfile, strerror(errno));
	    }
#ifdef	NOTYET
	    else {
		shutil.rmtree(oldfile)
	    }
#endif
	} else {
	    if ((xx = Rename(oldfile, finalfile)) != 0) {
		repo_error(1, _("Could not restore old non-metadata file: %s -> %s: %s"),
			oldfile, finalfile, strerror(errno));
	    }
	}
	oldfile = _free(oldfile);
    }
    xx = Closedir(dir);
  }

    if ((xx = Rmdir(output_old_dir)) != 0) {
	repo_error(1, _("Could not remove old metadata dir: %s: %s"),
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
        xx = _poptSaveString(&repo->exclude_patterns, opt->argInfo, arg);
	break;
    case 'i':			/* --includes */
assert(arg != NULL);
        xx = _poptSaveString(&repo->include_patterns, opt->argInfo, arg);
	break;
    case 'l':			/* --pkglist */
assert(arg != NULL);
        xx = _poptSaveString(&repo->manifests, opt->argInfo, arg);
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
    POPT_TABLEEND
};

/*@unchecked@*/ /*@observer@*/
static struct poptOption optionsTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        repoArgCallback, 0, NULL, NULL },
/*@=type@*/

 { "repodebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_repo_debug, -1,
	N_("debug repo handling"), NULL },

 { "quiet", 'q', POPT_ARG_VAL,			&__rpmrepo.quiet, 0,
	N_("output nothing except for serious errors"), NULL },
 { "verbose", 'v', 0,				NULL, (int)'v',
	N_("output more debugging info."), NULL },
 { "dryrun", '\0', POPT_ARG_VAL,		&__rpmrepo.dryrun, 1,
	N_("sanity check arguments, don't create metadata"), NULL },
#if defined(POPT_ARG_ARGV)
 { "excludes", 'x', POPT_ARG_ARGV,		&__rpmrepo.exclude_patterns, 0,
	N_("glob PATTERN(s) to exclude"), N_("PATTERN") },
#else
 { "excludes", 'x', POPT_ARG_STRING,		NULL, 'x',
	N_("glob PATTERN(s) to exclude"), N_("PATTERN") },
#endif
#if defined(POPT_ARG_ARGV)
 { "includes", 'i', POPT_ARG_ARGV,		&__rpmrepo.include_patterns, 0,
	N_("glob PATTERN(s) to include"), N_("PATTERN") },
#else
 { "includes", 'i', POPT_ARG_STRING,		NULL, 'i',
	N_("glob PATTERN(s) to include"), N_("PATTERN") },
#endif
#ifdef	NOTYET
 { "basedir", '\0', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmrepo.basedir, 0,
	N_("top level directory"), N_("DIR") },
 { "baseurl", 'u', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmrepo.baseurl, 0,
	N_("baseurl to append on all files"), N_("BASEURL") },
 { "groupfile", 'g', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmrepo.groupfile, 0,
	N_("path to groupfile to include in metadata"), N_("FILE") },
#endif
 { "pretty", 'p', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,		&__rpmrepo.pretty, 1,
	N_("make sure all xml generated is formatted"), NULL },
 { "checkts", 'C', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmrepo.checkts, 1,
	N_("check timestamps on files vs the metadata to see if we need to update"), NULL },
 { "database", 'd', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmrepo.database, 1,
	N_("create sqlite database files"), NULL },
 { "split", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,		&__rpmrepo.split, 1,
	N_("generate split media"), NULL },
#if defined(POPT_ARG_ARGV)
 { "pkglist", 'l', POPT_ARG_ARGV|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmrepo.manifests, 0,
	N_("use only the files listed in this file from the directory specified"), N_("FILE") },
#else
 { "pkglist", 'l', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN,	NULL, 'l',
	N_("use only the files listed in this file from the directory specified"), N_("FILE") },
#endif
 { "outputdir", 'o', POPT_ARG_STRING,		&__rpmrepo.outputdir, 0,
	N_("<dir> = optional directory to output to"), N_("DIR") },
 { "skip-symlinks", 'S', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmrepo.skipsymlinks, 1,
	N_("ignore symlinks of packages"), NULL },
 { "unique-md-filenames", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &__rpmrepo.uniquemdfilenames, 1,
	N_("include the file's checksum in the filename, helps with proxies"), NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioFtsPoptTable, 0,
	N_("Fts(3) traversal options:"), NULL },

#ifdef	NOTYET
 { "version", '\0', 0, NULL, POPT_SHOWVERSION,
	N_("print the version"), NULL },
#endif

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
	/*@globals __assert_program_name,
		rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies __assert_program_name,
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

    __progname = "rpmmrepo";

    /* Process options. */
    optCon = rpmioInit(argc, argv, optionsTable);

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

if (_repo_debug || repo->dryrun)
argvPrint("repo->directories", repo->directories, NULL);

    /* XXX todo: convert relative -> absolute for repo->outputdir */
    if (repo->outputdir == NULL) {
	repo->outputdir = repoRealpath(".");
	if (repo->outputdir == NULL)
	    repo_error(1, _("Realpath(%s): %s"), ".", strerror(errno));
    }

    if (repo->split && repo->checkts)
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

if (_repo_debug || repo->dryrun)
argvPrint("repo->pkglist", repo->pkglist, NULL);

    repo->pkgcount = argvCount(repo->pkglist);
    rc = repoMetaDataGenerator(repo);
    if (rc || repo->dryrun)
	goto exit;

    if (!repo->split) {
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
    repo->ts = rpmtsFree(repo->ts);
    repo->pkglist = argvFree(repo->pkglist);
    repo->directories = argvFree(repo->directories);
    repo->manifests = argvFree(repo->manifests);
    repo->excludeMire = mireFreeAll(repo->excludeMire, repo->nexcludes);
    repo->exclude_patterns = argvFree(repo->exclude_patterns);
    repo->includeMire = mireFreeAll(repo->includeMire, repo->nincludes);
    repo->include_patterns = argvFree(repo->include_patterns);

    optCon = rpmioFini(optCon);

    return rc;
}

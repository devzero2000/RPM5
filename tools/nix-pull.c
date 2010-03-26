#include "system.h"

#include <rpmiotypes.h>
#include <rpmio.h>
#include <rpmlog.h>
#include <rpmmacro.h>
#include <argv.h>
#include <poptIO.h>

#include "debug.h"

static int _debug = -1;


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

    RPMNIX_FLAGS_EVALONLY	= _DFB(16),	/*    --eval-only */
    RPMNIX_FLAGS_PARSEONLY	= _DFB(17),	/*    --parse-only */
    RPMNIX_FLAGS_ADDROOT	= _DFB(18),	/*    --add-root */
    RPMNIX_FLAGS_XML		= _DFB(19),	/*    --xml */
    RPMNIX_FLAGS_STRICT		= _DFB(20),	/*    --strict */
    RPMNIX_FLAGS_SHOWTRACE	= _DFB(21),	/*    --show-trace */

    RPMNIX_FLAGS_SKIPWRONGSTORE	= _DFB(24)	/*    --skip-wrong-store */
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

    const char * attr;

    const char * manifest;

    const char ** narFiles;
    const char ** localPaths;
    const char ** patches;
};

/**
 */
static struct rpmnix_s _nix = {
	.flags = RPMNIX_FLAGS_NOOUTLINK
};

static const char * tmpDir;
static const char * binDir	= "/usr/bin";
static const char * libexecDir	= "/usr/libexec";
static const char * storeDir	= "/nix/store";
static const char * stateDir	= "/nix/var/nix";
static const char * manifestDir;

#define	DBG(_l)	if (_debug) fprintf _l
/*==============================================================*/

static char * _freeCmd(const char * cmd)
{
DBG((stderr, "\t%s\n", cmd));
    cmd = _free(cmd);
    return NULL;
}

static int addPatch(rpmnix nix, const char * storePath, const char * patch)
	/*@*/
{
    int found = 0;

DBG((stderr, "--> %s(%p, \"%s\", \"%s\")\n", __FUNCTION__, nix, storePath, patch));
#ifdef	REFERENCE
/*
    my ($patches, $storePath, $patch) = @_;
*/
#endif

#ifdef	REFERENCE
/*
    $$patches{$storePath} = []
        unless defined $$patches{$storePath};
*/
#endif

#ifdef	REFERENCE
/*
    my $patchList = $$patches{$storePath};
*/
#endif

#ifdef	REFERENCE
/*
    foreach my $patch2 (@{$patchList}) {
        $found = 1 if
            $patch2->{url} eq $patch->{url} &&
            $patch2->{basePath} eq $patch->{basePath};
    }
*/
#endif
    
#ifdef	REFERENCE
/*
    push @{$patchList}, $patch if !$found;
*/
#endif

    return !found;
}


static int readManifest(rpmnix nix, const char * manifest)
	/*@*/
{

#ifdef	REFERENCE
/*
    my ($manifest, $narFiles, $localPaths, $patches) = @_;
*/
#endif
    int inside = 0;
    const char * type;
    int manifestVersion = 2;
    int xx;

    const char * storePath = NULL;
    const char * url = NULL;
    const char * hash = NULL;
    const char * size = NULL;
    const char * basePath = NULL;
    const char * baseHash = NULL;
    const char * patchType = NULL;
    const char * narHash = NULL;
    const char * references = NULL;
    const char * deriver = NULL;
    const char * hashAlgo = NULL;
    const char * copyFrom = NULL;

    FD_t fd = Fopen(manifest, "r");

DBG((stderr, "--> %s(%p, \"%s\")\n", __FUNCTION__, nix, manifest));
    if (fd == NULL || Ferror(fd)) {
	fprintf(stderr, "Fopen(%s, \"r\") failed\n", manifest);
	if (fd) xx = Fclose(fd);
	exit(1);
    }

#ifdef	REFERENCE
/*
    while (<MANIFEST>) {
*/
#endif
#ifdef	REFERENCE
/*
        chomp;
        s/\#.*$//g;
        next if (/^$/);
*/
#endif

	if (!inside) {
#ifdef	REFERENCE
/*
            if (/^\s*(\w*)\s*\{$/) {
                $type = $1;
                $type = "narfile" if $type eq "";
                $inside = 1;
                undef $storePath;
                undef $url;
                undef $hash;
                undef $size;
                undef $narHash;
                undef $basePath;
                undef $baseHash;
                undef $patchType;
                $references = "";
                $deriver = "";
                $hashAlgo = "md5";
	    }
*/
#endif

        } else {
            
#ifdef	REFERENCE
/*
            if (/^\}$/) {
                $inside = 0;

                if ($type eq "narfile") {

                    $$narFiles{$storePath} = []
                        unless defined $$narFiles{$storePath};

                    my $narFileList = $$narFiles{$storePath};

                    my $found = 0;
                    foreach my $narFile (@{$narFileList}) {
                        $found = 1 if $narFile->{url} eq $url;
                    }
                    if (!$found) {
                        push @{$narFileList},
                            { url => $url, hash => $hash, size => $size
                            , narHash => $narHash, references => $references
                            , deriver => $deriver, hashAlgo => $hashAlgo
                            };
                    }
                
                }

                elsif ($type eq "patch") {
                    addPatch $patches, $storePath,
                        { url => $url, hash => $hash, size => $size
                        , basePath => $basePath, baseHash => $baseHash
                        , narHash => $narHash, patchType => $patchType
                        , hashAlgo => $hashAlgo
                        };
                }

                elsif ($type eq "localPath") {

                    $$localPaths{$storePath} = []
                        unless defined $$localPaths{$storePath};

                    my $localPathsList = $$localPaths{$storePath};

                    # !!! remove duplicates
                    
                    push @{$localPathsList},
                        { copyFrom => $copyFrom, references => $references
                        , deriver => ""
                        };
                }

            }
            
            elsif (/^\s*StorePath:\s*(\/\S+)\s*$/) { $storePath = $1; }
            elsif (/^\s*CopyFrom:\s*(\/\S+)\s*$/) { $copyFrom = $1; }
            elsif (/^\s*Hash:\s*(\S+)\s*$/) { $hash = $1; }
            elsif (/^\s*URL:\s*(\S+)\s*$/) { $url = $1; }
            elsif (/^\s*Size:\s*(\d+)\s*$/) { $size = $1; }
            elsif (/^\s*SuccOf:\s*(\/\S+)\s*$/) { } # obsolete
            elsif (/^\s*BasePath:\s*(\/\S+)\s*$/) { $basePath = $1; }
            elsif (/^\s*BaseHash:\s*(\S+)\s*$/) { $baseHash = $1; }
            elsif (/^\s*Type:\s*(\S+)\s*$/) { $patchType = $1; }
            elsif (/^\s*NarHash:\s*(\S+)\s*$/) { $narHash = $1; }
            elsif (/^\s*References:\s*(.*)\s*$/) { $references = $1; }
            elsif (/^\s*Deriver:\s*(\S+)\s*$/) { $deriver = $1; }
            elsif (/^\s*ManifestVersion:\s*(\d+)\s*$/) { $manifestVersion = $1; }

            # Compatibility;
            elsif (/^\s*NarURL:\s*(\S+)\s*$/) { $url = $1; }
            elsif (/^\s*MD5:\s*(\S+)\s*$/) { $hash = "md5:$1"; }

*/
#endif
        }
#ifdef	REFERENCE
/*
    }
*/
#endif

    if (fd) xx = Fclose(fd);

    return manifestVersion;
}

/*==============================================================*/

static char * downloadFile(rpmnix nix, const char * url)
	/*@*/
{
    const char * cmd;
    const char * rval;
    char * path;
    int xx;

    xx = setenv("PRINT_PATH", "1", 0);
    xx = setenv("QUIET", "1", 0);
    cmd = rpmExpand(binDir, "/nix-prefetch-url '", url, "'", NULL);

    rval = rpmExpand("%(", cmd, ")", NULL);
    /* XXX The 1st line is the hash, the 2nd line is the path ... */
    if ((path = strchr(rval, '\n')) == NULL) {
	fprintf(stderr, "nix-prefetch-url did not return a path");
	exit(1);
    }
#ifdef	REFERENCE
/*
    chomp $path;
*/
#endif
    path = xstrdup(path+1);
DBG((stderr, "<-- %s(%p, \"%s\") path %s\n", __FUNCTION__, nix, url, path));
    rval = _free(rval);
    cmd = _freeCmd(cmd);
    return path;
}

static int processURL(rpmnix nix, const char * url)
	/*@*/
{
    const char * cmd;
    const char * rval;
    int version;
    const char * baseName;
    const char * urlFile;
    const char * finalPath;
    const char * hash;

    FD_t fd;
    char * globpat;
    char * fn;
    char * manifest;
    struct stat sb;
    ARGV_t gav;
    int gac;
    int xx;
    int i;

#ifdef	REFERENCE
/*
    my $url = shift;

    $url =~ s/\/$//;

    my $manifest;
*/
#endif

DBG((stderr, "--> %s(%p, \"%s\")\n", __FUNCTION__, nix, url));
    /* First see if a bzipped manifest is available. */
    fn = rpmGetPath(url, ".bz2", NULL);
#ifdef	REFERENCE
/*
    if (system("/usr/bin/curl --fail --silent --head '$url'.bz2 > /dev/null") == 0)
*/
#else
    if (!Stat(fn, &sb))
#endif
    {
	const char * bzipped;

        fprintf(stdout, _("fetching list of Nix archives at `%s'...\n"), fn);

	bzipped = downloadFile(nix, fn);

	manifest = rpmExpand(tmpDir, "/MANIFEST", NULL);

	cmd = rpmExpand("/usr/libexec/nix/bunzip2 < ", bzipped,
		" > ", manifest, "; echo $?", NULL);
	rval = rpmExpand("%(", cmd, ")", NULL);
	if (strcmp(rval, "0")) {
	    fprintf(stderr, "cannot decompress manifest\n");
	    exit(1);
	}
	rval = _free(rval);
	cmd = _freeCmd(cmd);

	cmd = rpmExpand(binDir, "/nix-store --add ", manifest, NULL);
	manifest = _free(manifest);
	manifest = rpmExpand("%(", cmd, ")", NULL);
	cmd = _freeCmd(cmd);

#ifdef	REFERENCE
/*
        chomp $manifest;
*/
#endif

    } else {	/* Otherwise, just get the uncompressed manifest. */
        fprintf(stdout, _("obtaining list of Nix archives at `%s'...\n"), url);
	manifest = downloadFile(nix, url);
    }
    fn = _free(fn);

    version = readManifest(nix, manifest);
    if (version < 3) {
	fprintf(stderr, "`%s' is not a manifest or it is too old (i.e., for Nix <= 0.7)\n", url);
	exit (1);
    }
    if (version >= 5) {
	fprintf(stderr, "manifest `%s' is too new\n", url);
	exit (1);
    }

    if (F_ISSET(nix, SKIPWRONGSTORE)) {
	size_t ns = strlen(storeDir);
	int nac = argvCount(nix->narFiles);
	int j;
	for (j = 0; j < nac; j++) {
	    const char * path = nix->narFiles[j];
	    size_t np = strlen(path);

	    if (np > ns && !strncmp(path, storeDir, ns) && path[ns] == '/')
		continue;
	    fprintf(stderr, "warning: manifest `%s' assumes a Nix store at a different location than %s, skipping...\n", url, storeDir);
	    exit(0);
	}
    }

    fn = xstrdup(url);
    baseName = xstrdup(basename(fn));
    fn = _free(fn);

    cmd = rpmExpand(binDir, "/nix-hash --flat ", manifest, NULL);
    hash = rpmExpand("%(", cmd, ")", NULL);
    cmd = _freeCmd(cmd);
    if (hash == NULL) {
	fprintf(stderr, "cannot hash `%s'\n", manifest);
	exit(1);
    }
#ifdef	REFERENCE
/*
    chomp $hash;
*/
#endif

    urlFile = rpmGetPath(manifestDir, "/",
			baseName, "-", hash, ".url", NULL);

    fd = Fopen(urlFile, "w");
    if (fd == NULL || Ferror(fd)) {
	fprintf(stderr, "cannot create `%s'\n", urlFile);
	if (fd) xx = Fclose(fd);
	exit(1);
    }
    (void) Fwrite(url, 1, strlen(url), fd);
    xx = Fclose(fd);
    
    finalPath = rpmGetPath(manifestDir, "/",
			baseName, "-", hash, ".nixmanifest", NULL);

    if (!Lstat(finalPath, &sb))
	xx = Unlink(finalPath);
        
    if (Symlink(manifest, finalPath)) {
	fprintf(stderr, _("cannot link `%s' to `%s'\n"), finalPath, manifest);
	exit(1);
    }
    finalPath = _free(finalPath);

    /* Delete all old manifests downloaded from this URL. */
    globpat = rpmGetPath(manifestDir, "/*.url", NULL);
    gav = NULL;
    gac = 0;
    if (!rpmGlob(globpat, &gac, &gav)) {
	for (i = 0; i < gac; i++) {
	    const char * urlFile2 = gav[i];
	    char * base, * be;
	    ARGV_t uav;
	    const char * url2;

	    if (!strcmp(urlFile, urlFile2))
		continue;

	    uav = NULL;
	    fd = Fopen(urlFile2, "r.fpio");
	    if (fd == NULL || Ferror(fd)) {
		fprintf(stderr, "cannot create `%s'\n", urlFile2);
		if (fd) xx = Fclose(fd);
		exit(1);
	    }
	    xx = argvFgets(&uav, fd);
	    xx = Fclose(fd);
	    url2 = xstrdup(uav[0]);
	    uav = argvFree(uav);

	    if (strcmp(url, url2))
		continue;

	    base = xstrdup(urlFile2);
	    be = base + strlen(base) - sizeof(".url") + 1;
	    if (be > base && !strcmp(be, ".url")) *be = '\0';

	    fn = rpmGetPath(base, ".url", NULL);
	    xx = Unlink(fn);
	    fn = _free(fn);

	    fn = rpmGetPath(base, ".nixmanifest", NULL);
	    xx = Unlink(fn);
	    fn = _free(fn);

	    base = _free(base);
	}
    }
    gav = argvFree(gav);
    globpat = _free(globpat);

    return 0;
}

/*==============================================================*/

#ifdef	UNUSED
static int verbose = 0;
#endif

static void nixInstantiateArgCallback(poptContext con,
                /*@unused@*/ enum poptCallbackReason reason,
                const struct poptOption * opt, const char * arg,
                /*@unused@*/ void * data)
	/*@*/
{
#ifdef	UNUSED
    rpmnix nix = &_nix;
#endif

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
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

 { "skip-wrong-store", '\0', POPT_BIT_SET,	&_nix.flags, RPMNIX_FLAGS_SKIPWRONGSTORE,
	N_("FIXME"), NULL },

#ifndef	NOTYET
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"), NULL },
#endif

  POPT_AUTOHELP

#ifdef	DYING
  { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
	N_("\
"), NULL },
#endif

  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    rpmnix nix = &_nix;
    poptContext optCon = rpmioInit(argc, argv, nixInstantiateOptions);
    const char * s;
    int ec = 1;		/* assume failure */
    ARGV_t av = poptGetArgs(optCon);
    int ac = argvCount(av);
    int xx;
    int i;

    if (!((s = getenv("TMPDIR")) != NULL && *s != '\0'))
	s = "/tmp";
    tmpDir = mkdtemp(rpmGetPath(s, "/nix-pull.XXXXXX", NULL));
    if (tmpDir == NULL) {
	fprintf(stderr, _("cannot create a temporary directory\n"));
	goto exit;
    }

    if ((s = getenv("NIX_BIN_DIR"))) binDir = s;
    if ((s = getenv("NIX_LIBEXEC_DIR"))) libexecDir = s;
    if ((s = getenv("NIX_STORE_DIR"))) storeDir = s;
    if ((s = getenv("NIX_STATE_DIR"))) stateDir = s;
    if ((s = getenv("NIX_MANIFESTS_DIR")))
	manifestDir = xstrdup(s);
    else
	manifestDir = rpmGetPath(stateDir, "/manifests", NULL);

    /* Prevent access problems in shared-stored installations. */
    xx = umask(0022);

    /* Create the manifests directory if it doesn't exist. */
    if (rpmioMkpath(manifestDir, (mode_t)0755, (uid_t)-1, (gid_t)-1)) {
	fprintf(stderr, _("cannot create directory `%s'\n"), manifestDir);
	goto exit;
    }

    for (i = 0; i < ac; i++) {
	xx = processURL(nix, av[i]);
    }

    fprintf(stdout, "%d store paths in manifest\n", 
		argvCount(nix->narFiles) + argvCount(nix->localPaths));

    ec = 0;	/* XXX success */

exit:
#ifdef	REFERENCE
/*
my $tmpDir = tempdir("nix-pull.XXXXXX", CLEANUP => 1, TMPDIR => 1)
    or die "cannot create a temporary directory";
*/
#endif

    manifestDir = _free(manifestDir);

    optCon = rpmioFini(optCon);

    return ec;
}

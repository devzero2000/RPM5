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

    RPMNIX_FLAGS_EVALONLY	= _DFB(16),	/*    --eval-only */
    RPMNIX_FLAGS_PARSEONLY	= _DFB(17),	/*    --parse-only */
    RPMNIX_FLAGS_ADDROOT	= _DFB(18),	/*    --add-root */
    RPMNIX_FLAGS_XML		= _DFB(19),	/*    --xml */
    RPMNIX_FLAGS_STRICT		= _DFB(20),	/*    --strict */
    RPMNIX_FLAGS_SHOWTRACE	= _DFB(21),	/*    --show-trace */

    RPMNIX_FLAGS_SKIPWRONGSTORE	= _DFB(30)	/*    --skip-wrong-store */
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
static const char * binDir = "/usr/bin";
static const char * libexecDir = "/usr/libexec";
static const char * storeDir = "/nix/store";
static const char * stateDir = "/nix/var/nix";
static const char * manifestDir;

/*==============================================================*/
static int addPatch(rpmnix nix, const char * storePath, const char * patch)
	/*@*/
{
    int found = 0;

#ifdef	REFERENCE
/*
    my ($patches, $storePath, $patch) = @_;

    $$patches{$storePath} = []
        unless defined $$patches{$storePath};

    my $patchList = $$patches{$storePath};

    my $found = 0;
    foreach my $patch2 (@{$patchList}) {
        $found = 1 if
            $patch2->{url} eq $patch->{url} &&
            $patch2->{basePath} eq $patch->{basePath};
    }
    
    push @{$patchList}, $patch if !$found;

    return !$found;
*/
#endif

    return !found;
}


static int readManifest(rpmnix nix, const char * manifest)
	/*@*/
{
    int manifestVersion = 2;

#ifdef	REFERENCE
/*
    my ($manifest, $narFiles, $localPaths, $patches) = @_;

    open MANIFEST, "<$manifest"
        or die "cannot open `$manifest': $!";

    my $inside = 0;
    my $type;

    my $manifestVersion = 2;

    my $storePath;
    my $url;
    my $hash;
    my $size;
    my $basePath;
    my $baseHash;
    my $patchType;
    my $narHash;
    my $references;
    my $deriver;
    my $hashAlgo;
    my $copyFrom;

    while (<MANIFEST>) {
        chomp;
        s/\#.*$//g;
        next if (/^$/);

        if (!$inside) {

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

        } else {
            
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

        }
    }

    close MANIFEST;

*/
#endif

    return manifestVersion;
}

/*==============================================================*/

#ifdef	REFERENCE
/*
# Process the URLs specified on the command line.
my %narFiles;
my %localPaths;
my %patches;

my $skipWrongStore = 0;
*/
#endif

static char * downloadFile(rpmnix nix, const char * url)
	/*@*/
{
    const char * cmd;
    const char * rval;
    char * path;

#ifdef	REFERENCE
/*
    $ENV{"PRINT_PATH"} = 1;
    $ENV{"QUIET"} = 1;
    my ($dummy, $path) = `$binDir/nix-prefetch-url '$url'`;
    die "cannot fetch `$url'" if $? != 0;
    die "nix-prefetch-url did not return a path" unless defined $path;
    chomp $path;
}
*/
#endif
    cmd = rpmExpand(binDir, "nix-pre-fetch-url '", url, "'", NULL);
    rval = rpmExpand("%(", cmd, ")", NULL);
    cmd = _free(cmd);
    path = xstrdup(rval);
    rval = _free(rval);
    return path;
}

static int processURL(rpmnix nix, const char * url)
	/*@*/
{
    const char * cmd;
    const char * rval;
    int version;
    const char * baseName = "unnamed";
    const char * urlFile;
    const char * finalPath;
    const char * hash;

    char * globpat;
    char * fn;
    char * manifest;
    struct stat sb;
    ARGV_t av;
    int ac;
    int xx;
    int i;

#ifdef	REFERENCE
/*
    my $url = shift;

    $url =~ s/\/$//;

    my $manifest;
*/
#endif

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

#ifdef	REFERENCE
/*
        my $bzipped = downloadFile "$url.bz2";
*/
#endif
	bzipped = downloadFile(nix, fn);

#ifdef	REFERENCE
/*
        $manifest = "$tmpDir/MANIFEST";
*/
#endif
	manifest = rpmExpand(tmpDir, "/MANIFEST", NULL);

#ifdef	REFERENCE
/*
        system("/usr/libexec/nix/bunzip2 < $bzipped > $manifest") == 0
            or die "cannot decompress manifest";
*/
#endif
	cmd = rpmExpand("/usr/libexec/nix/bunzip2 < ", bzipped,
		" > ", manifest, "; echo $?", NULL);
	rval = rpmExpand("%(", cmd, ")", NULL);
	cmd = _free(cmd);
	if (strcmp(rval, "0")) {
	    fprintf(stderr, "cannot decompress manifest\n");
	    exit(1);
	}
	rval = _free(rval);

#ifdef	REFERENCE
/*
        $manifest = (`$binDir/nix-store --add $manifest`
                     or die "cannot copy $manifest to the store");
*/
#endif
	cmd = rpmExpand(binDir, "/nix-store --add ", manifest, NULL);
	manifest = _free(manifest);
	manifest = rpmExpand("%(", cmd, ")", NULL);
	cmd = _free(cmd);

#ifdef	REFERENCE
/*
        chomp $manifest;
*/
#endif

    } else {	/* Otherwise, just get the uncompressed manifest. */
        fprintf(stdout, _("obtaining list of Nix archives at `%s'...\n"), url);
#ifdef	REFERENCE
/*
        $manifest = downloadFile $url;
*/
#endif
	manifest = downloadFile(nix, url);
    }
    fn = _free(fn);

#ifdef	REFERENCE
/*
    my $version = readManifest($manifest, \%narFiles, \%localPaths, \%patches);
    
    die "`$url' is not a manifest or it is too old (i.e., for Nix <= 0.7)\n" if $version < 3;
    die "manifest `$url' is too new\n" if $version >= 5;
*/
#endif
    version = readManifest(nix, manifest);
    if (version < 3) {
	fprintf(stderr, "`%s' is not a manifest or it is too old (i.e., for Nix <= 0.7)\n", url);
	exit (1);
    }
    if (version >= 5) {
	fprintf(stderr, "`%s' is too new\n", url);
	exit (1);
    }

#ifdef	REFERENCE
/*
    if ($skipWrongStore) {
        foreach my $path (keys %narFiles) {
            if (substr($path, 0, length($storeDir) + 1) ne "$storeDir/") {
                print STDERR "warning: manifest `$url' assumes a Nix store at a different location than $storeDir, skipping...\n";
                exit 0;
            }
        }
    }
*/
#endif
    if (F_ISSET(nix, SKIPWRONGSTORE)) {
    }

#ifdef	REFERENCE
/*
    my $baseName = "unnamed";
    if ($url =~ /\/([^\/]+)\/[^\/]+$/) { # get the forelast component
        $baseName = $1;
    }
*/
#endif
    baseName = "unnamed";

#ifdef	REFERENCE
/*
    my $hash = `$binDir/nix-hash --flat '$manifest'`
        or die "cannot hash `$manifest'";
*/
#endif
    cmd = rpmExpand(binDir, "/nix-hash --flat ", manifest, NULL);
    hash = rpmExpand("%(", cmd, ")", NULL);
    cmd = _free(cmd);
    if (hash == NULL) {
	fprintf(stderr, "cannot hash `%s'\n", manifest);
	exit(1);
    }
#ifdef	REFERENCE
/*
    chomp $hash;
*/
#endif

#ifdef	REFERENCE
/*
    my $urlFile = "$manifestDir/$baseName-$hash.url";
*/
#endif
    urlFile = rpmGetPath(manifestDir, "/",
			baseName, "-", hash, ".url", NULL);

#ifdef	REFERENCE
/*
    open URL, ">$urlFile" or die "cannot create `$urlFile'";
    print URL "$url";
    close URL;
*/
#endif
    
#ifdef	REFERENCE
/*
    my $finalPath = "$manifestDir/$baseName-$hash.nixmanifest";
*/
#endif
    finalPath = rpmGetPath(manifestDir, "/",
			baseName, "-", hash, ".nixmanifest", NULL);

#ifdef	REFERENCE
/*
    unlink $finalPath if -e $finalPath;
*/
#endif
    if (!Stat(finalPath, &sb))
	xx = Unlink(finalPath);
        
#ifdef	REFERENCE
/*
    symlink("$manifest", "$finalPath")
        or die "cannot link `$finalPath to `$manifest'";
*/
#endif
    if (Symlink(manifest, finalPath)) {
	fprintf(stderr, _("cannot link `%s' to `%s'\n"), finalPath, manifest);
	exit(1);
    }
    finalPath = _free(finalPath);

    /* Delete all old manifests downloaded from this URL. */
#ifdef	REFERENCE
/*
    for my $urlFile2 (glob "$manifestDir/*.url")
*/
#endif
    globpat = rpmGetPath(manifestDir, "/*.url", NULL);
    av = NULL;
    ac = 0;
    if (!rpmGlob(globpat, &ac, &av)) {
	for (i = 0; i < ac; i++) {
	    const char * urlFile2 = av[i];
	    char * base, * be;

#ifdef	REFERENCE
/*
        next if $urlFile eq $urlFile2;
*/
#endif
	    if (!strcmp(urlFile, urlFile2))
		continue;

#ifdef	REFERENCE
/*
        open URL, "<$urlFile2" or die;
        my $url2 = <URL>;
        chomp $url2;
        close URL;
        next unless $url eq $url2;
*/
#endif

#ifdef	REFERENCE
/*
        my $base = $urlFile2; $base =~ s/.url$//;
        unlink "${base}.url";
        unlink "${base}.nixmanifest";
*/
#endif
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

#ifdef	NOTYET
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"), NULL },
#endif

  POPT_AUTOHELP

  { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
	N_("\
"), NULL },

  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    rpmnix nix = &_nix;
    poptContext optCon = rpmioInit(argc, argv, nixInstantiateOptions);
    const char * s;
    char * dn;
    int ec = 1;		/* assume failure */
    ARGV_t av = poptGetArgs(optCon);
    int ac = argvCount(av);
    int xx;
    int i;

#ifdef	REFERENCE
/*
#! /usr/bin/perl -w -I/usr/libexec/nix

use strict;
use File::Temp qw(tempdir);
use readmanifest;

my $tmpDir = tempdir("nix-pull.XXXXXX", CLEANUP => 1, TMPDIR => 1)
    or die "cannot create a temporary directory";

my $binDir = $ENV{"NIX_BIN_DIR"} || "/usr/bin";
my $libexecDir = ($ENV{"NIX_LIBEXEC_DIR"} or "/usr/libexec");
my $storeDir = ($ENV{"NIX_STORE_DIR"} or "/nix/store");
my $stateDir = ($ENV{"NIX_STATE_DIR"} or "/nix/var/nix");
my $manifestDir = ($ENV{"NIX_MANIFESTS_DIR"} or "$stateDir/manifests");


# Prevent access problems in shared-stored installations.
umask 0022;

# Create the manifests directory if it doesn't exist.
if (! -e $manifestDir) {
    mkdir $manifestDir, 0755 or die "cannot create directory `$manifestDir'";
}
*/
#endif

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

#ifdef	REFERENCE
/*
while (@ARGV) {
    my $url = shift @ARGV;
    if ($url eq "--skip-wrong-store") {
        $skipWrongStore = 1;
    } else {
        processURL $url;
    }
}
*/
#endif

    for (i = 0; i < ac; i++) {
	const char * url = av[i];
	xx = processURL(nix, av[i]);
    }

#ifdef	REFERENCE
/*
my $size = scalar (keys %narFiles) + scalar (keys %localPaths);
print "$size store paths in manifest\n";
*/
#endif

    ec = 0;	/* XXX success */

exit:

    manifestDir = _free(manifestDir);

    optCon = rpmioFini(optCon);

    return ec;
}

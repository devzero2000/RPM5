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

    RPMNIX_FLAGS_COPY		= _DFB(30)	/*    --copy */
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

    const char ** narFiles;
    const char ** localPaths;
    const char ** patches;

    const char ** storePaths;
    const char ** narArchives;

    const char * localArchivesDir;
    const char * localManifestFile;
    const char * targetArchivesUrl;
    const char * archivesPutURL;
    const char * archivesGetURL;
    const char * manifestPutURL;
};

/**
 */
static struct rpmnix_s _nix = {
	.flags = RPMNIX_FLAGS_NOOUTLINK
};

static const char * tmpDir;
static const char * binDir = "/usr/bin";

static const char * dataDir = "/usr/share";

/*==============================================================*/

static void writeManifest(rpmnix nix, const char * manifest)
	/*@*/
{
    char * fn = rpmGetPath(manifest, NULL);
    char * tfn = rpmGetPath(fn, ".tmp", NULL);
    const char * s;
    FD_t fd;
    ssize_t nw;
    int xx;
    int i;

#ifdef	REFERENCE
/*
    my ($manifest, $narFiles, $patches) = @_;
*/
#endif


#ifdef	REFERENCE
/*
    open MANIFEST, ">$manifest.tmp"; # !!! check exclusive
*/
#endif
    fd = Fopen(tfn, "w");	/* XXX check exclusive */
    if (fd == NULL || Ferror(fd)) {
	if (fd) xx = Fclose(fd);
	goto exit;
    }
    

#ifdef	REFERENCE
/*
    print MANIFEST "version {\n";
    print MANIFEST "  ManifestVersion: 3\n";
    print MANIFEST "}\n";
*/
#endif
    s = "\
version {\n\
  ManifestVersion: 3\n\
}\n\
";
    nw = Fwrite(s, 1, strlen(s), fd);

#ifdef	REFERENCE
/*
    foreach my $storePath (sort (keys %{$narFiles})) {
        my $narFileList = $$narFiles{$storePath};
        foreach my $narFile (@{$narFileList}) {
            print MANIFEST "{\n";
            print MANIFEST "  StorePath: $storePath\n";
            print MANIFEST "  NarURL: $narFile->{url}\n";
            print MANIFEST "  Hash: $narFile->{hash}\n" if defined $narFile->{hash};
            print MANIFEST "  NarHash: $narFile->{narHash}\n";
            print MANIFEST "  Size: $narFile->{size}\n" if defined $narFile->{size};
            print MANIFEST "  References: $narFile->{references}\n"
                if defined $narFile->{references} && $narFile->{references} ne "";
            print MANIFEST "  Deriver: $narFile->{deriver}\n"
                if defined $narFile->{deriver} && $narFile->{deriver} ne "";
            print MANIFEST "}\n";
        }
    }
*/
#endif
    
#ifdef	REFERENCE
/*
    foreach my $storePath (sort (keys %{$patches})) {
        my $patchList = $$patches{$storePath};
        foreach my $patch (@{$patchList}) {
            print MANIFEST "patch {\n";
            print MANIFEST "  StorePath: $storePath\n";
            print MANIFEST "  NarURL: $patch->{url}\n";
            print MANIFEST "  Hash: $patch->{hash}\n";
            print MANIFEST "  NarHash: $patch->{narHash}\n";
            print MANIFEST "  Size: $patch->{size}\n";
            print MANIFEST "  BasePath: $patch->{basePath}\n";
            print MANIFEST "  BaseHash: $patch->{baseHash}\n";
            print MANIFEST "  Type: $patch->{patchType}\n";
            print MANIFEST "}\n";
        }
    }
*/
#endif
    
    
#ifdef	REFERENCE
/*
    close MANIFEST;
*/
#endif
    if (fd)
	xx = Fclose(fd);

#ifdef	REFERENCE
/*
    rename("$manifest.tmp", $manifest)
        or die "cannot rename $manifest.tmp: $!";
*/
#endif
    if (Rename(tfn, fn) < 0) {
	fprintf(stderr, "Rename(%s, %s) failed\n", tfn, fn);
	exit(1);
    }


#ifdef	REFERENCE
/*
    # Create a bzipped manifest.
    system("/usr/libexec/nix/bzip2 < $manifest > $manifest.bz2.tmp") == 0
        or die "cannot compress manifest";
*/
#endif

#ifdef	REFERENCE
/*
    rename("$manifest.bz2.tmp", "$manifest.bz2")
        or die "cannot rename $manifest.bz2.tmp: $!";
*/
#endif

exit:
    tfn = _free(tfn);
    fn = _free(fn);
    return;
}

/*==============================================================*/

static int copyFile(const char * src, const char * dst)
	/*@*/
{
    const char * tfn = rpmGetPath(dst, ".tmp", NULL);

#ifdef	REFERENCE
/*
    system("/bin/cp", $src, $tmp) == 0 or die "cannot copy file";
*/
#endif

    if (Rename(tfn, dst) < 0) {
	fprintf(stderr, "Rename(%s, %s) failed\n", tfn, dst);
	exit(1);
    }
    tfn = _free(tfn);
    return 0;
}

static int archiveExists(const char * name)
	/*@*/
{
#ifdef	REFERENCE
/*
    my $name = shift;
    print STDERR "  HEAD on $archivesGetURL/$name\n";
    return system("$curl --head $archivesGetURL/$name > /dev/null") == 0;
*/
#endif
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

 { "copy", '\0', POPT_BIT_SET,		&_nix.flags, RPMNIX_FLAGS_COPY,
        N_("FIXME"), NULL },
 { "target", '\0', POPT_ARG_STRING,	&_nix.targetArchivesUrl, 0,
        N_("FIXME"), NULL },

#ifdef	NOTYET
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"), NULL },
#endif

  POPT_AUTOHELP

  { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
	N_("\
Usage: nix-push --copy ARCHIVES_DIR MANIFEST_FILE PATHS...\n\
   or: nix-push ARCHIVES_PUT_URL ARCHIVES_GET_URL MANIFEST_PUT_URL PATHS...\n\
\n\
`nix-push' copies or uploads the closure of PATHS to the given\n\
destination.\n\
"), NULL },

  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    rpmnix nix = &_nix;
    poptContext optCon = rpmioInit(argc, argv, nixInstantiateOptions);
    int ec = 1;		/* assume failure */
    const char * s;
    ARGV_t av = poptGetArgs(optCon);
    int ac = argvCount(av);
    int xx;
    int i;

    const char * nixExpr = NULL;
    const char * manifest = NULL;
    int localCopy;
    int nac;

#ifdef	REFERENCE
/*
#! /usr/bin/perl -w -I/usr/libexec/nix

use strict;
use File::Temp qw(tempdir);
use readmanifest;

my $hashAlgo = "sha256";
*/
#endif

#ifdef	REFERENCE
/*
my $tmpDir = tempdir("nix-push.XXXXXX", CLEANUP => 1, TMPDIR => 1)
    or die "cannot create a temporary directory";
*/
#endif
    if (!((s = getenv("TMPDIR")) != NULL && *s != '\0'))
	s = "/tmp";
    tmpDir = mkdtemp(rpmGetPath(s, "/nix-push.XXXXXX", NULL));
    if (tmpDir == NULL) {
	fprintf(stderr, _("cannot create a temporary directory\n"));
	goto exit;
    }

#ifdef	REFERENCE
/*
my $nixExpr = "$tmpDir/create-nars.nix";
my $manifest = "$tmpDir/MANIFEST";
*/
#endif
    nixExpr = rpmGetPath(tmpDir, "/create-nars.nix", NULL);
    manifest = rpmGetPath(tmpDir, "/MANIFEST", NULL);

#ifdef	REFERENCE
/*
my $curl = "/usr/bin/curl --fail --silent";
my $extraCurlFlags = ${ENV{'CURL_FLAGS'}};
$curl = "$curl $extraCurlFlags" if defined $extraCurlFlags;
*/
#endif

#ifdef	REFERENCE
/*
my $binDir = $ENV{"NIX_BIN_DIR"} || "/usr/bin";

my $dataDir = $ENV{"NIX_DATA_DIR"};
$dataDir = "/usr/share" unless defined $dataDir;
*/
#endif
    if ((s = getenv("NIX_BIN_DIR"))) binDir = s;
    if ((s = getenv("NIX_DATA_DIR"))) dataDir = s;

    /* Parse the command line. */
#ifdef	REFERENCE
/*
my $localCopy;
my $localArchivesDir;
my $localManifestFile;

my $targetArchivesUrl;

my $archivesPutURL;
my $archivesGetURL;
my $manifestPutURL;
*/
#endif

#ifdef	REFERENCE
/*
showSyntax if scalar @ARGV < 1;
*/
#endif
    if (ac < 1) {
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }

#ifdef	REFERENCE
/*
if ($ARGV[0] eq "--copy") {
    showSyntax if scalar @ARGV < 3;
    $localCopy = 1;
    shift @ARGV;
    $localArchivesDir = shift @ARGV;
    $localManifestFile = shift @ARGV;
    if ($ARGV[0] eq "--target") {
       shift @ARGV;
       $targetArchivesUrl = shift @ARGV;
    }
    else {
       $targetArchivesUrl = "file://$localArchivesDir";
    }
}
else {
    showSyntax if scalar @ARGV < 3;
    $localCopy = 0;
    $archivesPutURL = shift @ARGV;
    $archivesGetURL = shift @ARGV;
    $manifestPutURL = shift @ARGV;
}
*/
#endif
    if (F_ISSET(nix, COPY)) {
	if (ac < 2) {
	    poptPrintUsage(optCon, stderr, 0);
	    goto exit;
	}
	localCopy = 1;
	nix->localArchivesDir = av[0];
	nix->localManifestFile = av[1];
	if (nix->targetArchivesUrl == NULL)
	    nix->targetArchivesUrl = "file://$localArchivesDir";
    } else {
	if (ac < 3) {
	    poptPrintUsage(optCon, stderr, 0);
	    goto exit;
	}
	localCopy = 0;
	nix->archivesPutURL = av[0];
	nix->archivesGetURL = av[1];
	nix->manifestPutURL = av[2];
    }

    /*
     * From the given store paths, determine the set of requisite store
     * paths, i.e, the paths required to realise them.
     */
    for (i = 0; i < ac; i++) {
	const char * path = av[i];
#ifdef	REFERENCE
/*
    die unless $path =~ /^\//;
*/
#endif
assert(*path == '/');
	/*
	 * Get all paths referenced by the normalisation of the given 
	 * Nix expression.
	 */
#ifdef	REFERENCE
/*
    my $pid = open(READ,
        "$binDir/nix-store --query --requisites --force-realise " .
        "--include-outputs '$path'|") or die;
    
    while (<READ>) {
        chomp;
        die "bad: $_" unless /^\//;
        $storePaths{$_} = "";
    }

    close READ or die "nix-store failed: $?";
*/
#endif
    }

    /*
     * For each path, create a Nix expression that turns the path into
     * a Nix archive.
     */
#ifdef	REFERENCE
/*
my @storePaths = keys %storePaths;

open NIX, ">$nixExpr";
print NIX "[";

foreach my $storePath (@storePaths) {
    die unless ($storePath =~ /\/[0-9a-z]{32}[^\"\\\$]*$/);

    # Construct a Nix expression that creates a Nix archive.
    my $nixexpr = 
        "((import $dataDir/nix/corepkgs/nar/nar.nix) " .
        "{storePath = builtins.storePath \"$storePath\"; system = \"i686-linux\"; hashAlgo = \"$hashAlgo\";}) ";
    
    print NIX $nixexpr;
}

print NIX "]";
close NIX;
*/
#endif


    /* Instantiate store derivations from the Nix expression. */
    fprintf(stderr, "instantiating store derivations...\n");

#ifdef	REFERENCE
/*
my @storeExprs;
my $pid = open(READ, "$binDir/nix-instantiate $nixExpr|")
    or die "cannot run nix-instantiate";
while (<READ>) {
    chomp;
    die unless /^\//;
    push @storeExprs, $_;
}
close READ or die "nix-instantiate failed: $?";
*/
#endif

    /* Build the derivations. */
    fprintf(stderr, "creating archives...\n");

#ifdef	REFERENCE
/*
my @narPaths;

my @tmp = @storeExprs;
while (scalar @tmp > 0) {
    my $n = scalar @tmp;
    if ($n > 256) { $n = 256 };
    my @tmp2 = @tmp[0..$n - 1];
    @tmp = @tmp[$n..scalar @tmp - 1];

    my $pid = open(READ, "$binDir/nix-store --realise @tmp2|")
        or die "cannot run nix-store";
    while (<READ>) {
        chomp;
        die unless (/^\//);
        push @narPaths, "$_";
    }
    close READ or die "nix-store failed: $?";
}
*/
#endif

    /* Create the manifest. */
    fprintf(stderr, "creating manifest...\n");

#ifdef	REFERENCE
/*

my %narFiles;
my %patches;

my @narArchives;
for (my $n = 0; $n < scalar @storePaths; $n++) {
    my $storePath = $storePaths[$n];
    my $narDir = $narPaths[$n];
    
    $storePath =~ /\/([^\/]*)$/;
    my $basename = $1;
    defined $basename or die;

    open HASH, "$narDir/narbz2-hash" or die "cannot open narbz2-hash";
    my $narbz2Hash = <HASH>;
    chomp $narbz2Hash;
    $narbz2Hash =~ /^[0-9a-z]+$/ or die "invalid hash";
    close HASH;

    open HASH, "$narDir/nar-hash" or die "cannot open nar-hash";
    my $narHash = <HASH>;
    chomp $narHash;
    $narHash =~ /^[0-9a-z]+$/ or die "invalid hash";
    close HASH;
    
    my $narName = "$narbz2Hash.nar.bz2";

    my $narFile = "$narDir/$narName";
    (-f $narFile) or die "narfile for $storePath not found";
    push @narArchives, $narFile;

    my $narbz2Size = (stat $narFile)[7];

    my $references = `$binDir/nix-store --query --references '$storePath'`;
    die "cannot query references for `$storePath'" if $? != 0;
    $references = join(" ", split(" ", $references));

    my $deriver = `$binDir/nix-store --query --deriver '$storePath'`;
    die "cannot query deriver for `$storePath'" if $? != 0;
    chomp $deriver;
    $deriver = "" if $deriver eq "unknown-deriver";

    my $url;
    if ($localCopy) {
        $url = "$targetArchivesUrl/$narName";
    } else {
        $url = "$archivesGetURL/$narName";
    }
    $narFiles{$storePath} = [
        { url => $url
        , hash => "$hashAlgo:$narbz2Hash"
        , size => $narbz2Size
        , narHash => "$hashAlgo:$narHash"
        , references => $references
        , deriver => $deriver
        }
    ];
}
*/
#endif

#ifdef	REFERENCE
/*
writeManifest $manifest, \%narFiles, \%patches;
*/
#endif
    writeManifest(nix, manifest);

    /* Upload/copy the archives. */
    fprintf(stderr, "uploading/copying archives...\n");

    nac = argvCount(nix->narArchives);
    for (i = 0; i < nac; i++) {
	char * narArchive = xstrdup(av[i]);
#ifdef	REFERENCE
/*
    $narArchive =~ /\/([^\/]*)$/;
    my $basename = $1;
*/
#endif
	char * bn = basename(narArchive);

	if (localCopy) {
	    const char * dst = rpmGetPath(nix->localArchivesDir, "/", bn, NULL);
	    struct stat sb;

	    /*
	     * Since nix-push creates $dst atomically, if it exists we
	     * don't have to copy again.
	     */
	    if (Stat(dst, &sb) < 0) {
		fprintf(stderr, "  %s\n", narArchive);
		xx = copyFile(narArchive, dst);
	    }
	} else {
	    if (!archiveExists(bn)) {
		fprintf(stderr, "  %s\n", narArchive);
#ifdef	REFERENCE
/*
            system("$curl --show-error --upload-file " .
                   "'$narArchive' '$archivesPutURL/$basename' > /dev/null") == 0 or
                   die "curl failed on $narArchive: $?";
*/
#endif
	    }
	}
	narArchive = _free(narArchive);
    }

    /* Upload the manifest. */
    fprintf(stderr, "uploading manifest...\n");

    if (localCopy) {
#ifdef	REFERENCE
/*
    copyFile $manifest, $localManifestFile;
*/
#endif
	xx = copyFile(manifest, nix->localManifestFile);
#ifdef	REFERENCE
/*
    copyFile "$manifest.bz2", "$localManifestFile.bz2";
*/
#endif
    } else {
#ifdef	REFERENCE
/*
    system("$curl --show-error --upload-file " .
           "'$manifest' '$manifestPutURL' > /dev/null") == 0 or
           die "curl failed on $manifest: $?";
*/
#endif
#ifdef	REFERENCE
/*
    system("$curl --show-error --upload-file " .
           "'$manifest'.bz2 '$manifestPutURL'.bz2 > /dev/null") == 0 or
           die "curl failed on $manifest: $?";
*/
#endif
    }

    ec = 0;	/* XXX success */

exit:
#ifdef	REFERENCE
/*
my $tmpDir = tempdir("nix-push.XXXXXX", CLEANUP => 1, TMPDIR => 1)
    or die "cannot create a temporary directory";
*/
#endif

    manifest = _free(manifest);
    nixExpr = _free(nixExpr);

    optCon = rpmioFini(optCon);

    return ec;
}

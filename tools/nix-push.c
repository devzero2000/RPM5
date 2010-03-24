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
};

/**
 */
static struct rpmnix_s _nix = {
	.flags = RPMNIX_FLAGS_NOOUTLINK
};

/*==============================================================*/
#ifdef	REFERENCE
/*
use strict;


sub addPatch {
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
}


sub readManifest {
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

    return $manifestVersion;
}


sub writeManifest {
    my ($manifest, $narFiles, $patches) = @_;

    open MANIFEST, ">$manifest.tmp"; # !!! check exclusive

    print MANIFEST "version {\n";
    print MANIFEST "  ManifestVersion: 3\n";
    print MANIFEST "}\n";

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
    
    
    close MANIFEST;

    rename("$manifest.tmp", $manifest)
        or die "cannot rename $manifest.tmp: $!";


    # Create a bzipped manifest.
    system("/usr/libexec/nix/bzip2 < $manifest > $manifest.bz2.tmp") == 0
        or die "cannot compress manifest";

    rename("$manifest.bz2.tmp", "$manifest.bz2")
        or die "cannot rename $manifest.bz2.tmp: $!";
}


return 1;
*/
#endif

/*==============================================================*/
#ifdef	REFERENCE
/*
#! /usr/bin/perl -w -I/usr/libexec/nix

use strict;
use File::Temp qw(tempdir);
use readmanifest;

my $hashAlgo = "sha256";

my $tmpDir = tempdir("nix-push.XXXXXX", CLEANUP => 1, TMPDIR => 1)
    or die "cannot create a temporary directory";

my $nixExpr = "$tmpDir/create-nars.nix";
my $manifest = "$tmpDir/MANIFEST";

my $curl = "/usr/bin/curl --fail --silent";
my $extraCurlFlags = ${ENV{'CURL_FLAGS'}};
$curl = "$curl $extraCurlFlags" if defined $extraCurlFlags;

my $binDir = $ENV{"NIX_BIN_DIR"} || "/usr/bin";

my $dataDir = $ENV{"NIX_DATA_DIR"};
$dataDir = "/usr/share" unless defined $dataDir;


# Parse the command line.
my $localCopy;
my $localArchivesDir;
my $localManifestFile;

my $targetArchivesUrl;

my $archivesPutURL;
my $archivesGetURL;
my $manifestPutURL;

sub showSyntax {
    print STDERR <<EOF
Usage: nix-push --copy ARCHIVES_DIR MANIFEST_FILE PATHS...
   or: nix-push ARCHIVES_PUT_URL ARCHIVES_GET_URL MANIFEST_PUT_URL PATHS...

`nix-push' copies or uploads the closure of PATHS to the given
destination.
EOF
    ; # `
    exit 1;
}

showSyntax if scalar @ARGV < 1;

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


# From the given store paths, determine the set of requisite store
# paths, i.e, the paths required to realise them.
my %storePaths;

foreach my $path (@ARGV) {
    die unless $path =~ /^\//;

    # Get all paths referenced by the normalisation of the given 
    # Nix expression.
    my $pid = open(READ,
        "$binDir/nix-store --query --requisites --force-realise " .
        "--include-outputs '$path'|") or die;
    
    while (<READ>) {
        chomp;
        die "bad: $_" unless /^\//;
        $storePaths{$_} = "";
    }

    close READ or die "nix-store failed: $?";
}

my @storePaths = keys %storePaths;


# For each path, create a Nix expression that turns the path into
# a Nix archive.
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


# Instantiate store derivations from the Nix expression.
my @storeExprs;
print STDERR "instantiating store derivations...\n";
my $pid = open(READ, "$binDir/nix-instantiate $nixExpr|")
    or die "cannot run nix-instantiate";
while (<READ>) {
    chomp;
    die unless /^\//;
    push @storeExprs, $_;
}
close READ or die "nix-instantiate failed: $?";


# Build the derivations.
print STDERR "creating archives...\n";

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


# Create the manifest.
print STDERR "creating manifest...\n";

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

writeManifest $manifest, \%narFiles, \%patches;


sub copyFile {
    my $src = shift;
    my $dst = shift;
    my $tmp = "$dst.tmp.$$";
    system("/bin/cp", $src, $tmp) == 0 or die "cannot copy file";
    rename($tmp, $dst) or die "cannot rename file: $!";
}


# Upload/copy the archives.
print STDERR "uploading/copying archives...\n";

sub archiveExists {
    my $name = shift;
    print STDERR "  HEAD on $archivesGetURL/$name\n";
    return system("$curl --head $archivesGetURL/$name > /dev/null") == 0;
}

foreach my $narArchive (@narArchives) {

    $narArchive =~ /\/([^\/]*)$/;
    my $basename = $1;

    if ($localCopy) {
        # Since nix-push creates $dst atomically, if it exists we
        # don't have to copy again.
        my $dst = "$localArchivesDir/$basename";
        if (! -f "$localArchivesDir/$basename") {
            print STDERR "  $narArchive\n";
            copyFile $narArchive, $dst;
        }
    }
    else {
        if (!archiveExists("$basename")) {
            print STDERR "  $narArchive\n";
            system("$curl --show-error --upload-file " .
                   "'$narArchive' '$archivesPutURL/$basename' > /dev/null") == 0 or
                   die "curl failed on $narArchive: $?";
        }
    }
}


# Upload the manifest.
print STDERR "uploading manifest...\n";
if ($localCopy) {
    copyFile $manifest, $localManifestFile;
    copyFile "$manifest.bz2", "$localManifestFile.bz2";
} else {
    system("$curl --show-error --upload-file " .
           "'$manifest' '$manifestPutURL' > /dev/null") == 0 or
           die "curl failed on $manifest: $?";
    system("$curl --show-error --upload-file " .
           "'$manifest'.bz2 '$manifestPutURL'.bz2 > /dev/null") == 0 or
           die "curl failed on $manifest: $?";
}
*/
#endif

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
    poptContext optCon = rpmioInit(argc, argv, nixInstantiateOptions);
    int ec = 1;		/* assume failure */
#ifdef	UNUSED
    ARGV_t av = poptGetArgs(optCon);
    int ac = argvCount(av);
    rpmnix nix = &_nix;
    int xx;
#endif

    ec = 0;	/* XXX success */

    optCon = rpmioFini(optCon);

    return ec;
}

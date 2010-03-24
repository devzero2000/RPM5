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
#! /usr/bin/perl -w

use strict;

my $rootsDir = "/nix/var/nix/gcroots";

my $stateDir = $ENV{"NIX_STATE_DIR"};
$stateDir = "/nix/var/nix" unless defined $stateDir;


# Turn on caching in nix-prefetch-url.
my $channelCache = "$stateDir/channel-cache";
mkdir $channelCache, 0755 unless -e $channelCache;
$ENV{'NIX_DOWNLOAD_CACHE'} = $channelCache if -W $channelCache;


# Figure out the name of the `.nix-channels' file to use.
my $home = $ENV{"HOME"};
die '$HOME not set' unless defined $home;
my $channelsList = "$home/.nix-channels";

my $nixDefExpr = "$home/.nix-defexpr";
    

my @channels;


# Reads the list of channels from the file $channelsList;
sub readChannels {
    return if (!-f $channelsList);
    open CHANNELS, "<$channelsList" or die "cannot open `$channelsList': $!";
    while (<CHANNELS>) {
        chomp;
        next if /^\s*\#/;
        push @channels, $_;
    }
    close CHANNELS;
}


# Writes the list of channels to the file $channelsList;
sub writeChannels {
    open CHANNELS, ">$channelsList" or die "cannot open `$channelsList': $!";
    foreach my $url (@channels) {
        print CHANNELS "$url\n";
    }
    close CHANNELS;
}


# Adds a channel to the file $channelsList;
sub addChannel {
    my $url = shift;
    readChannels;
    foreach my $url2 (@channels) {
        return if $url eq $url2;
    }
    push @channels, $url;
    writeChannels;
}


# Remove a channel from the file $channelsList;
sub removeChannel {
    my $url = shift;
    my @left = ();
    readChannels;
    foreach my $url2 (@channels) {
        push @left, $url2 if $url ne $url2;
    }
    @channels = @left;
    writeChannels;
}


# Fetch Nix expressions and pull cache manifests from the subscribed
# channels.
sub update {
    readChannels;

    # Create the manifests directory if it doesn't exist.
    mkdir "$stateDir/manifests", 0755 unless -e "$stateDir/manifests";

    # Do we have write permission to the manifests directory?  If not,
    # then just skip pulling the manifest and just download the Nix
    # expressions.  If the user is a non-privileged user in a
    # multi-user Nix installation, he at least gets installation from
    # source.
    if (-W "$stateDir/manifests") {

        # Pull cache manifests.
        foreach my $url (@channels) {
            #print "pulling cache manifest from `$url'\n";
            system("/usr/bin/nix-pull", "--skip-wrong-store", "$url/MANIFEST") == 0
                or die "cannot pull cache manifest from `$url'";
        }

    }

    # Create a Nix expression that fetches and unpacks the channel Nix
    # expressions.

    my $inputs = "[";
    foreach my $url (@channels) {
        $url =~ /\/([^\/]+)\/?$/;
        my $channelName = $1;
        $channelName = "unnamed" unless defined $channelName;

        my $fullURL = "$url/nixexprs.tar.bz2";
        print "downloading Nix expressions from `$fullURL'...\n";
        $ENV{"PRINT_PATH"} = 1;
        $ENV{"QUIET"} = 1;
        my ($hash, $path) = `/usr/bin/nix-prefetch-url '$fullURL'`;
        die "cannot fetch `$fullURL'" if $? != 0;
        chomp $path;
        $inputs .= '"' . $channelName . '"' . " " . $path . " ";
    }
    $inputs .= "]";

    # Figure out a name for the GC root.
    my $userName = getpwuid($<);
    die "who ARE you? go away" unless defined $userName;

    my $rootFile = "$rootsDir/per-user/$userName/channels";
    
    # Build the Nix expression.
    print "unpacking channel Nix expressions...\n";
    my $outPath = `\\
        /usr/bin/nix-build --out-link '$rootFile' --drv-link '$rootFile'.tmp \\
        /usr/share/nix/corepkgs/channels/unpack.nix \\
        --argstr system i686-linux --arg inputs '$inputs'`
        or die "cannot unpack the channels";
    chomp $outPath;

    unlink "$rootFile.tmp";

    # Make the channels appear in nix-env.
    unlink $nixDefExpr if -l $nixDefExpr; # old-skool ~/.nix-defexpr
    mkdir $nixDefExpr or die "cannot create directory `$nixDefExpr'" if !-e $nixDefExpr;
    my $channelLink = "$nixDefExpr/channels";
    unlink $channelLink; # !!! not atomic
    symlink($outPath, $channelLink) or die "cannot symlink `$channelLink' to `$outPath'";
}


sub usageError {
    print STDERR <<EOF;
Usage:
  nix-channel --add URL
  nix-channel --remove URL
  nix-channel --list
  nix-channel --update
EOF
    exit 1;
}


usageError if scalar @ARGV == 0;


while (scalar @ARGV) {
    my $arg = shift @ARGV;

    if ($arg eq "--add") {
        usageError if scalar @ARGV != 1;
        addChannel (shift @ARGV);
        last;
    }

    if ($arg eq "--remove") {
        usageError if scalar @ARGV != 1;
        removeChannel (shift @ARGV);
        last;
    }

    if ($arg eq "--list") {
        usageError if scalar @ARGV != 0;
        readChannels;
        foreach my $url (@channels) {
            print "$url\n";
        }
        last;
    }

    elsif ($arg eq "--update") {
        usageError if scalar @ARGV != 0;
        update;
        last;
    }
    
    elsif ($arg eq "--help") {
        usageError;
    }

    else {
        die "unknown argument `$arg'; try `--help'";
    }
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

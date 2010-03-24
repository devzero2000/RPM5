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

    RPMNIX_FLAGS_SIGN		= _DFB(29),	/*    --sign */
    RPMNIX_FLAGS_GZIP		= _DFB(30)	/*    --gzip */
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

    int op;
    const char * host;
};

/**
 */
static struct rpmnix_s _nix = {
	.flags = RPMNIX_FLAGS_NOOUTLINK
};

/*==============================================================*/
#ifdef	REFERENCE
/*
#! /usr/bin/perl -w -I/usr/libexec/nix

use ssh;

my $binDir = $ENV{"NIX_BIN_DIR"} || "/usr/bin";


if (scalar @ARGV < 1) {
    print STDERR <<EOF
Usage: nix-copy-closure [--from | --to] HOSTNAME [--sign] [--gzip] PATHS...
EOF
    ;
    exit 1;
}


# Get the target host.
my $sshHost;

my $sign = 0;

my $compressor = "";
my $decompressor = "";

my $toMode = 1;


# !!! Copied from nix-pack-closure, should put this in a module.
my @storePaths = ();

while (@ARGV) {
    my $arg = shift @ARGV;
    
    if ($arg eq "--sign") {
        $sign = 1;
    }
    elsif ($arg eq "--gzip") {
        $compressor = "| gzip";
        $decompressor = "gunzip |";
    }
    elsif ($arg eq "--from") {
        $toMode = 0;
    }
    elsif ($arg eq "--to") {
        $toMode = 1;
    }
    elsif (!defined $sshHost) {
        $sshHost = $arg;
    }
    else {
        push @storePaths, $arg;
    }
}


openSSHConnection $sshHost or die "$0: unable to start SSH\n";


if ($toMode) { # Copy TO the remote machine.

    my @allStorePaths;

    # Get the closure of this path.
    my $pid = open(READ, "$binDir/nix-store --query --requisites @storePaths|") or die;
    
    while (<READ>) {
        chomp;
        die "bad: $_" unless /^\//;
        push @allStorePaths, $_;
    }

    close READ or die "nix-store failed: $?";


    # Ask the remote host which paths are invalid.
    open(READ, "ssh $sshHost @sshOpts nix-store --check-validity --print-invalid @allStorePaths|");
    my @missing = ();
    while (<READ>) {
        chomp;
        push @missing, $_;
    }
    close READ or die;


    # Export the store paths and import them on the remote machine.
    if (scalar @missing > 0) {
        print STDERR "copying these missing paths:\n";
        print STDERR "  $_\n" foreach @missing;
        my $extraOpts = "";
        $extraOpts .= "--sign" if $sign == 1;
        system("nix-store --export $extraOpts @missing $compressor | ssh $sshHost @sshOpts '$decompressor nix-store --import'") == 0
            or die "copying store paths to remote machine `$sshHost' failed: $?";
    }

}


else { # Copy FROM the remote machine.

    # Query the closure of the given store paths on the remote
    # machine.  Paths are assumed to be store paths; there is no
    # resolution (following of symlinks).
    my $pid = open(READ,
        "ssh @sshOpts $sshHost nix-store --query --requisites @storePaths|") or die;
    
    my @allStorePaths;

    while (<READ>) {
        chomp;
        die "bad: $_" unless /^\//;
        push @allStorePaths, $_;
    }

    close READ or die "nix-store on remote machine `$sshHost' failed: $?";


    # What paths are already valid locally?
    open(READ, "/usr/bin/nix-store --check-validity --print-invalid @allStorePaths|");
    my @missing = ();
    while (<READ>) {
        chomp;
        push @missing, $_;
    }
    close READ or die;
    

    # Export the store paths on the remote machine and import them on locally.
    if (scalar @missing > 0) {
        print STDERR "copying these missing paths:\n";
        print STDERR "  $_\n" foreach @missing;
        my $extraOpts = "";
        $extraOpts .= "--sign" if $sign == 1;
        system("ssh $sshHost @sshOpts 'nix-store --export $extraOpts @missing $compressor' | $decompressor /usr/bin/nix-store --import") == 0
            or die "copying store paths from remote machine `$sshHost' failed: $?";
    }

}
*/
#endif

/*==============================================================*/

#ifdef	UNUSED
static int verbose = 0;
#endif

enum {
    NIX_FROM_HOST = 1,
    NIX_TO_HOST,
};

static void nixInstantiateArgCallback(poptContext con,
                /*@unused@*/ enum poptCallbackReason reason,
                const struct poptOption * opt, const char * arg,
                /*@unused@*/ void * data)
	/*@*/
{
    rpmnix nix = &_nix;

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
    case NIX_FROM_HOST:
	nix->op = opt->val;
	nix->host = xstrdup(arg);
	break;
    case NIX_TO_HOST:
	nix->op = opt->val;
	nix->host = xstrdup(arg);
	break;
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

 { "from", '\0', POPT_ARG_STRING,	0, NIX_FROM_HOST,
	N_("FIXME"), NULL },
 { "to", '\0', POPT_ARG_STRING,		0, NIX_TO_HOST,
	N_("FIXME"), NULL },
 { "gzip", '\0', POPT_BIT_SET,		&_nix.flags, RPMNIX_FLAGS_GZIP,
	N_("FIXME"), NULL },
 { "sign", '\0', POPT_BIT_SET,		&_nix.flags, RPMNIX_FLAGS_SIGN,
	N_("FIXME"), NULL },

#ifdef	NOTYET
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"), NULL },
#endif

  POPT_AUTOHELP

  { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
	N_("\
Usage: nix-copy-closure [--from | --to] HOSTNAME [--sign] [--gzip] PATHS...\n\
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

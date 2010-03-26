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

    RPMNIX_FLAGS_DELETEOLD	= _DFB(24)	/* -d,--delete-old */
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

static const char * binDir	= "/usr/bin";

static const char * profilesDir	= "/nix/var/nix/profiles";

#define DBG(_l) if (_debug) fprintf _l
/*==============================================================*/

static char * _freeCmd(const char * cmd)
{
DBG((stderr, "\t%s\n", cmd));
    cmd = _free(cmd);
    return NULL;
}

/*
 * If `-d' was specified, remove all old generations of all profiles.
 * Of course, this makes rollbacks to before this point in time
 * impossible.
 */
static int removeOldGenerations(rpmnix nix, const char * dir)
	/*@*/
{
#ifdef	REFERENCE
/*
    my $dh;
    opendir $dh, $dir or die;

    foreach my $name (sort (readdir $dh)) {
        next if $name eq "." || $name eq "..";
        $name = $dir . "/" . $name;
        if (-l $name && (readlink($name) =~ /link/)) {
            print STDERR "removing old generations of profile $name\n";
            system("$binDir/nix-env", "-p", $name, "--delete-generations", "old");
        }
        elsif (! -l $name && -d $name) {
            removeOldGenerations $name;
        }
    }
    
    closedir $dh or die;
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

 { "delete-old", 'd', POPT_BIT_SET,	&_nix.flags, RPMNIX_FLAGS_DELETEOLD,
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
    int ec = 1;		/* assume failure */
    const char * rval;
    const char * cmd;
    const char * s;
    ARGV_t av = poptGetArgs(optCon);
#ifdef	UNUSED
    int ac = argvCount(av);
#endif
    int xx;

    if ((s = getenv("NIX_BIN_DIR"))) binDir = s;

    /* Process the command line arguments. */
#ifdef	REFERENCE
/*
my @args = ();
my $removeOld = 0;

for my $arg (@ARGV) {
    if ($arg eq "--delete-old" || $arg eq "-d") {
        $removeOld = 1;
    } else {
        push @args, $arg;
    }
}
*/
#endif

#ifdef	REFERENCE
/*
removeOldGenerations $profilesDir if $removeOld;
*/
#endif
    if (F_ISSET(nix, DELETEOLD))
	xx = removeOldGenerations(nix, profilesDir);

#ifdef	REFERENCE
/*
# Run the actual garbage collector.
exec "$binDir/nix-store", "--gc", @args;
*/
#endif
    s = argvJoin(av, ' ');
    cmd = rpmExpand(binDir, "/nix-store --gc ", s, "; echo $?", NULL);
    s = _free(s);
    rval = rpmExpand("%(", cmd, ")", NULL);
    if (!strcmp(rval, "0"))
	ec = 0;	/* XXX success */
    rval = _free(rval);
    cmd = _freeCmd(cmd);

    optCon = rpmioFini(optCon);

    return ec;
}

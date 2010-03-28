#include "system.h"

#define	_RPMNIX_INTERNAL
#include <rpmnix.h>
#include <poptIO.h>

#include "debug.h"

/*==============================================================*/

enum {
    NIX_FROM_HOST = 1,
    NIX_TO_HOST,
};

static void nixCopyClosureArgCallback(poptContext con,
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
	nix->sshHost = xstrdup(arg);
	break;
    case NIX_TO_HOST:
	nix->op = opt->val;
	nix->sshHost = xstrdup(arg);
	break;
    default:
	fprintf(stderr, _("%s: Unknown callback(0x%x)\n"), __FUNCTION__, (unsigned) opt->val);
	poptPrintUsage(con, stderr, 0);
	/*@-exitarg@*/ exit(2); /*@=exitarg@*/
	/*@notreached@*/ break;
    }
}

static struct poptOption nixCopyClosureOptions[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
	nixCopyClosureArgCallback, 0, NULL, NULL },
/*@=type@*/

 { "from", '\0', POPT_ARG_STRING,	0, NIX_FROM_HOST,
	N_("FIXME"), NULL },
 { "to", '\0', POPT_ARG_STRING,		0, NIX_TO_HOST,
	N_("FIXME"), NULL },
 { "gzip", '\0', POPT_BIT_SET,		&_nix.flags, RPMNIX_FLAGS_GZIP,
	N_("FIXME"), NULL },
 { "sign", '\0', POPT_BIT_SET,		&_nix.flags, RPMNIX_FLAGS_SIGN,
	N_("FIXME"), NULL },

#ifndef	NOTYET
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
    rpmnix nix = rpmnixNew(argv, RPMNIX_FLAGS_NONE, nixCopyClosureOptions);
    ARGV_t av = poptGetArgs((poptContext)nix->I);
    int ac = argvCount(av);
    int ec = 1;		/* assume failure */
    const char * s;
    const char * cmd;
    const char * rval;
    const char * sshOpts = "";			/* XXX HACK */
    const char * compressor = "";
    const char * decompressor = "";
    const char * extraOpts = "";
    int nac;
    int xx;

    if (ac < 1) {
	poptPrintUsage((poptContext)nix->I, stderr, 0);
	goto exit;
    }

    if (nix->op == 0)
	nix->op = NIX_TO_HOST;

    xx = argvAppend(&nix->storePaths, av);
    if (F_ISSET(nix, GZIP)) {
	compressor = "| gzip";
	decompressor = "gunzip |";
    }
    if (F_ISSET(nix, SIGN)) {
	extraOpts = " --sign";
    }

#ifdef	REFERENCE
/*
openSSHConnection $sshHost or die "$0: unable to start SSH\n";
*/
#endif


    switch (nix->op) {
    case NIX_TO_HOST:		/* Copy TO the remote machine. */

	/* Get the closure of this path. */
#ifdef	REFERENCE
/*
    my $pid = open(READ, "$binDir/nix-store --query --requisites @storePaths|") or die;
    
    while (<READ>) {
        chomp;
        die "bad: $_" unless /^\//;
        push @allStorePaths, $_;
    }

    close READ or die "nix-store failed: $?";
*/
#endif
	s = argvJoin(nix->storePaths, ' ');
        cmd = rpmExpand(nix->binDir, "/nix-store --query --requisites ", s, NULL);
	s = _free(s);
        rval = rpmExpand("%(", cmd, ")", NULL);
        xx = argvSplit(&nix->allStorePaths, rval, NULL);
        rval = _free(rval);
        cmd = _freeCmd(cmd);

	/* Ask the remote host which paths are invalid. */
#ifdef	REFERENCE
/*
    open(READ, "ssh $sshHost @sshOpts nix-store --check-validity --print-invalid @allStorePaths|");
    my @missing = ();
    while (<READ>) {
        chomp;
        push @missing, $_;
    }
    close READ or die;
*/
#endif
	s = argvJoin(nix->allStorePaths, ' ');
        cmd = rpmExpand("ssh ", nix->sshHost, " ", sshOpts, " nix-store --check-validity --print-invalid ", s, NULL);
	s = _free(s);
#ifdef	NOTYET
        rval = rpmExpand("%(", cmd, ")", NULL);
        xx = argvSplit(&nix->missing, rval, NULL);
        rval = _free(rval);
#else
nix->missing = NULL;
fprintf(stderr, "<-- missing assumed NULL\n");
#endif
        cmd = _freeCmd(cmd);

	/* Export the store paths and import them on the remote machine. */
	nac = argvCount(nix->missing);
	if (nac > 0) {
argvPrint("copying these missing paths:", nix->missing, NULL);
#ifdef	REFERENCE
/*
        my $extraOpts = "";
        $extraOpts .= "--sign" if $sign == 1;
        system("nix-store --export $extraOpts @missing $compressor | ssh $sshHost @sshOpts '$decompressor nix-store --import'") == 0
            or die "copying store paths to remote machine `$sshHost' failed: $?";
*/
#endif
	    s = argvJoin(nix->missing, ' ');
	    cmd = rpmExpand(nix->binDir, "/nix-store --export ", extraOpts, " ", s, " ", compressor,
		" | ssh ", nix->sshHost, " ", sshOpts, " '", decompressor, " nix-store --import'", NULL);
	    s = _free(s);
	    cmd = _freeCmd(cmd);
	}
	break;
    case NIX_FROM_HOST:		/* Copy FROM the remote machine. */

	/*
	 * Query the closure of the given store paths on the remote
	 * machine.  Paths are assumed to be store paths; there is no
	 * resolution (following of symlinks).
	 */
#ifdef	REFERENCE
/*
    my $pid = open(READ,
        "ssh @sshOpts $sshHost nix-store --query --requisites @storePaths|") or die;
    
    my @allStorePaths;

    while (<READ>) {
        chomp;
        die "bad: $_" unless /^\//;
        push @allStorePaths, $_;
    }

    close READ or die "nix-store on remote machine `$sshHost' failed: $?";
*/
#endif
	s = argvJoin(nix->storePaths, ' ');
        cmd = rpmExpand("ssh ", sshOpts, " ", nix->sshHost, " nix-store --query --requisites ", s, NULL);
	s = _free(s);
#ifdef	NOTYET
        rval = rpmExpand("%(", cmd, ")", NULL);
        xx = argvSplit(&nix->missing, rval, NULL);
        rval = _free(rval);
#else
nix->allStorePaths = NULL;
fprintf(stderr, "<-- allStorePaths assumed NULL\n");
#endif
	cmd = _freeCmd(cmd);

	/* What paths are already valid locally? */
#ifdef	REFERENCE
/*
    open(READ, "/usr/bin/nix-store --check-validity --print-invalid @allStorePaths|");
    my @missing = ();
    while (<READ>) {
        chomp;
        push @missing, $_;
    }
    close READ or die;
*/
#endif
	s = argvJoin(nix->allStorePaths, ' ');
        cmd = rpmExpand(nix->binDir, "/nix-store --check-validity --print-invalid ", s, NULL);
	s = _free(s);
        rval = rpmExpand("%(", cmd, ")", NULL);
        xx = argvSplit(&nix->missing, rval, NULL);
        rval = _free(rval);
	cmd = _freeCmd(cmd);

	/* Export the store paths on the remote machine and import them on locally. */
	nac = argvCount(nix->missing);
	if (nac > 0) {
argvPrint("copying these missing paths:", nix->missing, NULL);
#ifdef	REFERENCE
/*
        system("ssh $sshHost @sshOpts 'nix-store --export $extraOpts @missing $compressor' | $decompressor /usr/bin/nix-store --import") == 0
            or die "copying store paths from remote machine `$sshHost' failed: $?";
*/
#endif
	    s = argvJoin(nix->missing, ' ');
	    cmd = rpmExpand("ssh ", nix->sshHost, " ", sshOpts,
		" 'nix-store --export ", extraOpts, " ", s, " ", compressor,
		"' | ", decompressor, " ", nix->binDir, "/nix-store --import", NULL);
	    s = _free(s);
	    cmd = _freeCmd(cmd);
	}
	break;
    }

    ec = 0;	/* XXX success */

exit:

    nix->storePaths = argvFree(nix->storePaths);
    nix->sshHost = _free(nix->sshHost);

    nix = rpmnixFree(nix);

    return ec;
}

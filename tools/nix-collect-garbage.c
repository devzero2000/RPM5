#include "system.h"

#define	_RPMNIX_INTERNAL
#include <rpmnix.h>
#include <rpmdir.h>
#include <poptIO.h>

/**
 */
static struct rpmnix_s _nix = {
	.flags = RPMNIX_FLAGS_NONE
};

static const char * binDir	= "/usr/bin";

static const char * profilesDir	= "/nix/var/nix/profiles";

/*==============================================================*/

/*
 * If `-d' was specified, remove all old generations of all profiles.
 * Of course, this makes rollbacks to before this point in time
 * impossible.
 */
static int removeOldGenerations(rpmnix nix, const char * dn)
	/*@*/
{
    DIR * dir;
    struct dirent * dp;
    struct stat sb;
    int xx;

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
    dir = Opendir(dn);
    if (dn == NULL) {
	fprintf(stderr, "Opendir(%s) failed\n", dn);
	exit(1);
    }
    while ((dp = Readdir(dir)) != NULL) {
	const char * fn;
	if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
	    continue;
	fn = rpmGetPath(dn, "/", dp->d_name, NULL);
	if (Lstat(fn, &sb) < 0) {
	    fn = _free(fn);
	    continue;
	}
	/* Recurse on sub-directories. */
	if (S_ISDIR(sb.st_mode)) {
	    xx = removeOldGenerations(nix, fn);
	    fn = _free(fn);
	    continue;
	}
	if (S_ISLNK(sb.st_mode)) {
	    char buf[BUFSIZ];
	    ssize_t nr = Readlink(fn, buf, sizeof(buf));
	    if (nr >= 0)
		buf[nr] = '\0';
	    if (!strcmp(buf, "link")) {	/* XXX correct test? */
		const char * cmd;
		const char * rval;

		fprintf(stderr, "removing old generations of profile %s\n", fn);
		cmd = rpmExpand(binDir, "/nix-env -p ", fn,
				" --delete-generations old", NULL);
		rval = rpmExpand("%(", cmd, ")", NULL);
		rval = _free(rval);
		cmd = _freeCmd(cmd);
	    }
	    fn = _free(fn);
	    continue;
	}
	fn = _free(fn);
    }
    xx = Closedir(dir);

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

#ifndef	NOTYET
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

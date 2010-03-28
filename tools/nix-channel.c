#include "system.h"

#define	_RPMNIX_INTERNAL
#include <rpmnix.h>
#include <ugid.h>
#include <poptIO.h>

#include "debug.h"

/*==============================================================*/

/* Reads the list of channels from the file $channelsList. */
static void readChannels(rpmnix nix)
	/*@*/
{
    FD_t fd;
    struct stat sb;
    int xx;

DBG((stderr, "--> %s(%p)\n", __FUNCTION__, nix));

#ifdef	REFERENCE
/*
    return if (!-f $channelsList);
*/
#endif
    if (nix->channelsList == NULL || Stat(nix->channelsList, &sb) < 0)
	return;

#ifdef	REFERENCE
/*
    open CHANNELS, "<$channelsList" or die "cannot open `$channelsList': $!";
    while (<CHANNELS>) {
        chomp;
        next if /^\s*\#/;
        push @channels, $_;
    }
    close CHANNELS;
*/
#endif
    fd = Fopen(nix->channelsList, "r.fpio");
    if (fd == NULL || Ferror(fd)) {
	fprintf(stderr, "Fopen(%s, \"r\") failed.\n", nix->channelsList);
	if (fd) xx = Fclose(fd);
	exit(1);
    }
    /* XXX skip comments todo++ */
    nix->channels = argvFree(nix->channels);
    xx = argvFgets(&nix->channels, fd);
    xx = Fclose(fd);
}

/* Writes the list of channels to the file $channelsList */
static void writeChannels(rpmnix nix)
	/*@*/
{
    FD_t fd;
    int ac = argvCount(nix->channels);
    ssize_t nw;
    int xx;
    int i;

DBG((stderr, "--> %s(%p)\n", __FUNCTION__, nix));
    if (Access(nix->channelsList, W_OK)) {
	fprintf(stderr, "file %s is not writable.\n", nix->channelsList);
	return;
    }
    fd = Fopen(nix->channelsList, "w");
    if (fd == NULL || Ferror(fd)) {
	fprintf(stderr, "Fopen(%s, \"w\") failed.\n", nix->channelsList);
	if (fd) xx = Fclose(fd);
	exit(1);
    }
    for (i = 0; i < ac; i++) {
	const char * url = nix->channels[i];
	nw = Fwrite(url, 1, strlen(url), fd);
	nw = Fwrite("\n", 1, sizeof("\n")-1, fd);
    }
    xx = Fclose(fd);
}

/* Adds a channel to the file $channelsList */
static void addChannel(rpmnix nix, const char * url)
	/*@*/
{
    int ac;
    int xx;
    int i;

DBG((stderr, "--> %s(%p, \"%s\")\n", __FUNCTION__, nix, url));
    readChannels(nix);
    ac = argvCount(nix->channels);
    for (i = 0; i < ac; i++) {
	if (!strcmp(nix->channels[i], url))
	    return;
    }
    xx = argvAdd(&nix->channels, url);
    writeChannels(nix);
}

/* Remove a channel from the file $channelsList */
static void removeChannel(rpmnix nix, const char * url)
	/*@*/
{
    const char ** nchannels = NULL;
    int ac;
    int xx;
    int i;

DBG((stderr, "--> %s(%p, \"%s\")\n", __FUNCTION__, nix, url));
    readChannels(nix);
    ac = argvCount(nix->channels);
    for (i = 0; i < ac; i++) {
	if (!strcmp(nix->channels[i], url))
	    continue;
	xx = argvAdd(&nchannels, nix->channels[i]);
    }
    nix->channels = argvFree(nix->channels);
    nix->channels = nchannels;
    writeChannels(nix);
}

/*
 * Fetch Nix expressions and pull cache manifests from the subscribed
 * channels.
 */
static void updateChannels(rpmnix nix)
	/*@*/
{
    uid_t uid = getuid();
    struct stat sb;
    const char * userName = uidToUname(uid);
    const char * rootFile;
    const char * outPath;
    const char * rval;
    const char * cmd;
    const char * fn;

const char * inputs = "[]";	/* XXX FIXME */
    int xx;

DBG((stderr, "--> %s(%p)\n", __FUNCTION__, nix));

    readChannels(nix);

    /* Create the manifests directory if it doesn't exist. */
    xx = rpmioMkpath(nix->manifestsPath, (mode_t)0755, (uid_t)-1, (gid_t)-1);

    /*
     * Do we have write permission to the manifests directory?  If not,
     * then just skip pulling the manifest and just download the Nix
     * expressions.  If the user is a non-privileged user in a
     * multi-user Nix installation, he at least gets installation from
     * source.
     */
    if (!Access(nix->manifestsPath, W_OK)) {
	int ac = argvCount(nix->channels);
	int i;

	/* Pull cache manifests. */
#ifdef	REFERENCE
/*
        foreach my $url (@channels) {
            #print "pulling cache manifest from `$url'\n";
            system("/usr/bin/nix-pull", "--skip-wrong-store", "$url/MANIFEST") == 0
                or die "cannot pull cache manifest from `$url'";
        }

*/
#endif
	for (i = 0; i < ac; i++) {
	    const char * url = nix->channels[i];
            cmd = rpmExpand(nix->binDir, "/nix-pull --skip-wrong-store ",
			url, "/MANIFEST", "; echo $?", NULL);
	    rval = rpmExpand("%(", cmd, ")", NULL);
	    if (strcmp(rval, "0")) {
		fprintf(stderr, "cannot pull cache manifest from `%s'\n", url);
		exit(1);
	    }
	    rval = _free(rval);
	    cmd = _freeCmd(cmd);
	}
    }

    /*
     * Create a Nix expression that fetches and unpacks the channel Nix
     * expressions.
     */
#ifdef	REFERENCE
/*
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
*/
#endif

    /* Figure out a name for the GC root. */
    rootFile = rpmGetPath(nix->rootsPath,
			"/per-user/", userName, "/channels", NULL);
    
    /* Build the Nix expression. */
    fprintf(stdout, "unpacking channel Nix expressions...\n");
#ifdef	REFERENCE
/*
    my $outPath = `\\
        /usr/bin/nix-build --out-link '$rootFile' --drv-link '$rootFile'.tmp \\
        /usr/share/nix/corepkgs/channels/unpack.nix \\
        --argstr system i686-linux --arg inputs '$inputs'`
        or die "cannot unpack the channels";
    chomp $outPath;
*/
#endif
    fn = rpmGetPath(rootFile, ".tmp", NULL);
    outPath = rpmExpand(nix->binDir, "/nix-build --out-link '", rootFile, "'",
		" --drv-link '", fn, "'", 
		"/usr/share/nix/corepkgs/channels/unpack.nix --argstr system i686-linux --arg inputs '", inputs, "'", NULL);
    outPath = rpmExpand("%(", cmd, ")", NULL);
    cmd = _freeCmd(cmd);

    xx = Unlink(fn);
    fn = _free(fn);

    /* Make the channels appear in nix-env. */
#ifdef	REFERENCE
/*
    unlink $nixDefExpr if -l $nixDefExpr; # old-skool ~/.nix-defexpr
    mkdir $nixDefExpr or die "cannot create directory `$nixDefExpr'" if !-e $nixDefExpr;
    my $channelLink = "$nixDefExpr/channels";
    unlink $channelLink; # !!! not atomic
    symlink($outPath, $channelLink) or die "cannot symlink `$channelLink' to `$outPath'";
*/
#endif
    if (Lstat(nix->nixDefExpr, &sb) == 0 && S_ISLNK(sb.st_mode))
	xx = Unlink(nix->nixDefExpr);
    if (Lstat(nix->nixDefExpr, &sb) < 0 && errno == ENOENT) {
	mode_t dmode = 0755;
	if (Mkdir(nix->nixDefExpr, dmode)) {
	    fprintf(stderr, "Mkdir(%s, 0%o) failed\n", nix->nixDefExpr, dmode);
	    exit(1);
	}
    }
    fn = rpmGetPath(nix->nixDefExpr, "/channels", NULL);
    xx = Unlink(fn);	/* !!! not atomic */
    if (Symlink(outPath, fn)) {
	fprintf(stderr, "Symlink(%s, %s) failed\n", outPath, fn);
	exit(1);
    }
    fn = _free(fn);

    rootFile = _free(rootFile);
}

/*==============================================================*/

enum {
    NIX_CHANNEL_ADD = 1,
    NIX_CHANNEL_REMOVE,
    NIX_CHANNEL_LIST,
    NIX_CHANNEL_UPDATE,
};

static void nixChannelArgCallback(poptContext con,
                /*@unused@*/ enum poptCallbackReason reason,
                const struct poptOption * opt, const char * arg,
                /*@unused@*/ void * data)
	/*@*/
{
    rpmnix nix = &_nix;

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
    case NIX_CHANNEL_ADD:
    case NIX_CHANNEL_REMOVE:
	nix->url = xstrdup(arg);
	/*@fallthrough@*/
    case NIX_CHANNEL_LIST:
    case NIX_CHANNEL_UPDATE:
	nix->op = opt->val;
	break;
    default:
	fprintf(stderr, _("%s: Unknown callback(0x%x)\n"), __FUNCTION__, (unsigned) opt->val);
	poptPrintUsage(con, stderr, 0);
	/*@-exitarg@*/ exit(2); /*@=exitarg@*/
	/*@notreached@*/ break;
    }
}

static struct poptOption nixChannelOptions[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
	nixChannelArgCallback, 0, NULL, NULL },
/*@=type@*/

 { "add", '\0', POPT_ARG_STRING,		0, NIX_CHANNEL_ADD,
        N_("subscribe to a Nix channel"), N_("URL") },
 { "remove", '\0', POPT_ARG_STRING,		0, NIX_CHANNEL_REMOVE,
        N_("unsubscribe from a Nix channel"), N_("URL") },
 { "list", '\0', POPT_ARG_NONE,			0, NIX_CHANNEL_LIST,
        N_("list subscribed channels"), NULL },
 { "update", '\0', POPT_ARG_NONE,		0, NIX_CHANNEL_UPDATE,
        N_("download latest Nix expressions"), NULL },

#ifndef	NOTYET
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"), NULL },
#endif

  POPT_AUTOHELP

#ifdef	DYING
  { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
	N_("\
Usage:\n\
  nix-channel --add URL\n\
  nix-channel --remove URL\n\
  nix-channel --list\n\
  nix-channel --update\n\
"), NULL },
#endif

  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    rpmnix nix = rpmnixNew(argv, RPMNIX_FLAGS_NONE, nixChannelOptions);
    int ac = 0;
    ARGV_t av = rpmnixArgv(nix, &ac);
    int ec = 1;		/* assume failure */
    int xx;

    /* Turn on caching in nix-prefetch-url. */
    nix->channelCache = rpmGetPath(nix->stateDir, "/channel-cache", NULL);
    xx = rpmioMkpath(nix->channelCache, 0755, (uid_t)-1, (gid_t)-1);
    if (!Access(nix->channelCache, W_OK))
	xx = setenv("NIX_DOWNLOAD_CACHE", nix->channelCache, 0);

    /* Figure out the name of the `.nix-channels' file to use. */
    nix->channelsList = rpmGetPath(nix->homeDir, "/.nix-channels", NULL);
    nix->nixDefExpr = rpmGetPath(nix->homeDir, "/.nix-defexpr", NULL);

    if (nix->op == 0 || (av && av[0] != NULL) || ac != 0) {
	poptPrintUsage(nix->con, stderr, 0);
	goto exit;
    }

    switch (nix->op) {
    case NIX_CHANNEL_ADD:
assert(nix->url != NULL);	/* XXX proper exit */
	addChannel(nix, nix->url);
	break;
    case NIX_CHANNEL_REMOVE:
assert(nix->url != NULL);	/* XXX proper exit */
	removeChannel(nix, nix->url);
	break;
    case NIX_CHANNEL_LIST:
	readChannels(nix);
	argvPrint(nix->channelsList, nix->channels, NULL);
	break;
    case NIX_CHANNEL_UPDATE:
	updateChannels(nix);
	break;
    }

    ec = 0;	/* XXX success */

exit:

    nix = rpmnixFree(nix);

    return ec;
}

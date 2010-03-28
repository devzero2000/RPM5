#include "system.h"

#define	_RPMNIX_INTERNAL
#include <rpmnix.h>
#include <rpmdir.h>
#include <ugid.h>
#include <poptIO.h>

#include "debug.h"

/*@unchecked@*/
int _rpmnix_debug = -1;

/*@unchecked@*/ /*@relnull@*/
rpmnix _rpmnixI = NULL;

/*@unchecked@*/
struct rpmnix_s _nix;

/*==============================================================*/

static int rpmnixBuildInstantiate(rpmnix nix, const char * expr, ARGV_t * drvPathsP)
	/*@*/
{
    ARGV_t argv = NULL;
    const char * argv0 = rpmGetPath(nix->binDir, "/nix-instantiate", NULL);
    const char * cmd;
    int rc = 1;		/* assume failure */
    int xx;

assert(drvPathsP);
    *drvPathsP = NULL;

argvPrint(__FUNCTION__, nix->instArgs, NULL);

    /* Construct the nix-instantiate command to run. */
    xx = argvAdd(&argv, argv0);
    argv0 = _free(argv0);

    xx = argvAdd(&argv, "--add-root");
    xx = argvAdd(&argv, nix->drvLink);
    xx = argvAdd(&argv, "--indirect");
    xx = argvAppend(&argv, nix->instArgs);
    xx = argvAdd(&argv, expr);
    cmd = argvJoin(argv, ' ');

    /* Chomp nix-instantiate spewage into *drvPathsP */
    argv0 = rpmExpand("%(", cmd, ")", NULL);
    xx = argvSplit(drvPathsP, argv0, NULL);
    argv0 = _free(argv0);
    rc = 0;

    cmd = _free(cmd);
    argv = argvFree(argv);

    return rc;
}

static int rpmnixBuildStore(rpmnix nix, ARGV_t drvPaths, ARGV_t * outPathsP)
	/*@*/
{
    ARGV_t argv = NULL;
    const char * argv0 = rpmGetPath(nix->binDir, "/nix-store", NULL);
    const char * cmd;
    int rc = 1;		/* assume failure */
    int xx;

assert(outPathsP);
    *outPathsP = NULL;

    /* Construct the nix-store command to run. */
    xx = argvAdd(&argv, argv0);
    argv0 = _free(argv0);

    xx = argvAdd(&argv, "--add-root");
    xx = argvAdd(&argv, nix->outLink);
    xx = argvAdd(&argv, "--indirect");
    xx = argvAdd(&argv, "-rv");
    xx = argvAppend(&argv, nix->buildArgs);
    xx = argvAppend(&argv, drvPaths);
    cmd = argvJoin(argv, ' ');

    /* Chomp nix-store spewage into *outPathsP */
    argv0 = rpmExpand("%(", cmd, ")", NULL);
    xx = argvSplit(outPathsP, argv0, NULL);
    argv0 = _free(argv0);
    rc = 0;

    cmd = _free(cmd);
    argv = argvFree(argv);

    return rc;
}

int rpmnixBuild(rpmnix nix)
	/*@*/
{
    static const char _default_nix[] = "./default.nix";
    static const char _build_tmp[] = ".nix-build-tmp-";
    int ac = 0;
    ARGV_t av = rpmnixArgv(nix, &ac);
    ARGV_t drvPaths = NULL;
    int ndrvPaths = 0;
    ARGV_t outPaths = NULL;
    int noutPaths = 0;
    int nexpr = 0;
    int ec = 1;		/* assume failure */
    int xx;
    int i;

#ifdef	REFERENCE
sub intHandler {
    exit 1;
}
$SIG{'INT'} = 'intHandler';
#endif

    if (ac == 0)
	xx = argvAdd(&nix->exprs, _default_nix);
    else
	xx = argvAppend(&nix->exprs, av);

    if (nix->drvLink == NULL) {
	const char * _pre = !F_ISSET(nix, ADDDRVLINK) ? _build_tmp : "";
	nix->drvLink = rpmExpand(_pre, "derivation", NULL);
    }
    if (nix->outLink == NULL) {
	const char * _pre = !F_ISSET(nix, NOOUTLINK) ? _build_tmp : "";
	nix->outLink = rpmExpand(_pre, "result", NULL);
    }

    /* Loop over all the Nix expression arguments. */
    nexpr = argvCount(nix->exprs);
    for (i = 0; i < nexpr; i++) {
	const char * expr = nix->exprs[i];

	/* Instantiate. */
	if (rpmnixBuildInstantiate(nix, expr, &drvPaths))
	    goto exit;

	/* Verify that the derivations exist, print if verbose. */
	ndrvPaths = argvCount(drvPaths);
	for (i = 0; i < ndrvPaths; i++) {
	    const char * drvPath = drvPaths[i];
	    char target[BUFSIZ];
	    ssize_t nb = Readlink(drvPath, target, sizeof(target));
	    if (nb < 0) {
		fprintf(stderr, _("%s: cannot read symlink `%s'\n"),
			__progname, drvPath);
		goto exit;
	    }
	    target[nb] = '\0';
	    if (nix->verbose)
		fprintf(stderr, "derivation is %s\n", target);
	}

	/* Build. */
	if (rpmnixBuildStore(nix, drvPaths, &outPaths))
	    goto exit;

	if (F_ISSET(nix, DRYRUN))
	    continue;

	noutPaths = argvCount(outPaths);
	for (i = 0; i < noutPaths; i++) {
	    const char * outPath = outPaths[i];
	    char target[BUFSIZ];
	    ssize_t nb = Readlink(outPath, target, sizeof(target));
	    if (nb < 0) {
		fprintf(stderr, _("%s: cannot read symlink `%s'\n"),
			__progname, outPath);
		goto exit;
	    }
	    target[nb] = '\0';
	    fprintf(stdout, "%s\n", target);
	}

    }

    ec = 0;	/* XXX success */

exit:
    /* Clean up any generated files. */
    av = NULL;
    ac = 0;
    if (!rpmGlob(".nix-build-tmp-*", &ac, &av)) {
	for (i = 0; i < ac; i++)
	    xx = Unlink(av[i]);
	av = argvFree(av);
	ac = 0;
    }

    nix = rpmnixFree(nix);

    return ec;
}

/*==============================================================*/
/* Reads the list of channels from the file $channelsList. */
static void rpmnixReadChannels(rpmnix nix)
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
static void rpmnixWriteChannels(rpmnix nix)
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
static void rpmnixAddChannel(rpmnix nix, const char * url)
	/*@*/
{
    int ac;
    int xx;
    int i;

DBG((stderr, "--> %s(%p, \"%s\")\n", __FUNCTION__, nix, url));
    rpmnixReadChannels(nix);
    ac = argvCount(nix->channels);
    for (i = 0; i < ac; i++) {
	if (!strcmp(nix->channels[i], url))
	    return;
    }
    xx = argvAdd(&nix->channels, url);
    rpmnixWriteChannels(nix);
}

/* Remove a channel from the file $channelsList */
static void rpmnixRemoveChannel(rpmnix nix, const char * url)
	/*@*/
{
    const char ** nchannels = NULL;
    int ac;
    int xx;
    int i;

DBG((stderr, "--> %s(%p, \"%s\")\n", __FUNCTION__, nix, url));
    rpmnixReadChannels(nix);
    ac = argvCount(nix->channels);
    for (i = 0; i < ac; i++) {
	if (!strcmp(nix->channels[i], url))
	    continue;
	xx = argvAdd(&nchannels, nix->channels[i]);
    }
    nix->channels = argvFree(nix->channels);
    nix->channels = nchannels;
    rpmnixWriteChannels(nix);
}

/*
 * Fetch Nix expressions and pull cache manifests from the subscribed
 * channels.
 */
static void rpmnixUpdateChannels(rpmnix nix)
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

    rpmnixReadChannels(nix);

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

int rpmnixChannel(rpmnix nix)
{
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
	rpmnixAddChannel(nix, nix->url);
	break;
    case NIX_CHANNEL_REMOVE:
assert(nix->url != NULL);	/* XXX proper exit */
	rpmnixRemoveChannel(nix, nix->url);
	break;
    case NIX_CHANNEL_LIST:
	rpmnixReadChannels(nix);
	argvPrint(nix->channelsList, nix->channels, NULL);
	break;
    case NIX_CHANNEL_UPDATE:
	rpmnixUpdateChannels(nix);
	break;
    }

    ec = 0;	/* XXX success */

exit:

    return ec;
}

/*==============================================================*/

/*
 * If `-d' was specified, remove all old generations of all profiles.
 * Of course, this makes rollbacks to before this point in time
 * impossible.
 */
static int rpmnixRemoveOldGenerations(rpmnix nix, const char * dn)
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
            rpmnixRemoveOldGenerations $name;
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
	    xx = rpmnixRemoveOldGenerations(nix, fn);
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
		cmd = rpmExpand(nix->binDir, "/nix-env -p ", fn,
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

int
rpmnixCollectGarbage(rpmnix nix)
{
    ARGV_t av = rpmnixArgv(nix, NULL);
    int ec = 1;		/* assume failure */
    const char * rval;
    const char * cmd;
    int xx;

    if (F_ISSET(nix, DELETEOLD))
	xx = rpmnixRemoveOldGenerations(nix, nix->profilesPath);

#ifdef	REFERENCE
/*
# Run the actual garbage collector.
exec "$binDir/nix-store", "--gc", @args;
*/
#endif
    rval = argvJoin(av, ' ');
    cmd = rpmExpand(nix->binDir, "/nix-store --gc ", rval, "; echo $?", NULL);
    rval = _free(rval);
    rval = rpmExpand("%(", cmd, ")", NULL);
    if (!strcmp(rval, "0"))
	ec = 0;	/* XXX success */
    rval = _free(rval);
    cmd = _freeCmd(cmd);

    return ec;
}

/*==============================================================*/

static void rpmnixFini(void * _nix)
	/*@globals fileSystem @*/
	/*@modifies *_nix, fileSystem @*/
{
    rpmnix nix = _nix;

DBG((stderr, "==> %s(%p)\n", __FUNCTION__, nix));

    nix->tmpPath = _free(nix->tmpPath);
    nix->manifestsPath = _free(nix->manifestsPath);
    nix->rootsPath = _free(nix->rootsPath);

    nix->url = _free(nix->url);

    /* nix-build */
    nix->outLink = _free(nix->outLink);
    nix->drvLink = _free(nix->drvLink);
    nix->instArgs = argvFree(nix->instArgs);
    nix->buildArgs = argvFree(nix->buildArgs);
    nix->exprs = argvFree(nix->exprs);

    /* nix-channel */
    nix->channels = argvFree(nix->channels);
    nix->nixDefExpr = _free(nix->nixDefExpr);
    nix->channelsList = _free(nix->channelsList);
    nix->channelCache = _free(nix->channelCache);

    /* nix-collect-garbage */
    nix->profilesPath = _free(nix->profilesPath);

    /* nix-copy-closure */
    nix->missing = argvFree(nix->missing);
    nix->allStorePaths = argvFree(nix->allStorePaths);
    nix->storePaths = argvFree(nix->storePaths);
    nix->sshHost = _free(nix->sshHost);

    /* nix-install-package */
    nix->profiles = argvFree(nix->profiles);

    /* nix-prefetch-url */
    nix->cacheFlags = _free(nix->cacheFlags);
    nix->hash = _free(nix->hash);
    nix->finalPath = _free(nix->finalPath);
    nix->tmpFile = _free(nix->tmpFile);
    nix->name = _free(nix->name);

    /* nix-push */
    nix->curl = _free(nix->curl);
    nix->manifest = _free(nix->manifest);
    nix->nixExpr = _free(nix->nixExpr);

    if (nix->con)
	nix->con = rpmioFini(nix->con);
    nix->con = NULL;
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmnixPool;

static rpmnix rpmnixGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmnixPool, fileSystem @*/
	/*@modifies pool, _rpmnixPool, fileSystem @*/
{
    rpmnix nix;

    if (_rpmnixPool == NULL) {
	_rpmnixPool = rpmioNewPool("nix", sizeof(*nix), -1, _rpmnix_debug,
			NULL, NULL, rpmnixFini);
	pool = _rpmnixPool;
    }
    nix = (rpmnix) rpmioGetPool(pool, sizeof(*nix));
    memset(((char *)nix)+sizeof(nix->_item), 0, sizeof(*nix)-sizeof(nix->_item));
    return nix;
}

#ifdef	NOTYET
static rpmnix rpmnixI(void)
	/*@globals _rpmnixI @*/
	/*@modifies _rpmnixI @*/
{
    if (_rpmnixI == NULL) {
	_rpmnixI = rpmnixNew(NULL, 0, NULL);
    }
DBG((stderr, "<== %s() _rpmnixI %p\n", __FUNCTION__, _rpmnixI));
    return _rpmnixI;
}
#endif

const char ** rpmnixArgv(rpmnix nix, int * argcp)
{
    const char ** av = (nix->con ? poptGetArgs(nix->con) : NULL);

    if (argcp)
	*argcp = argvCount(av);
    return av;
}

static void rpmnixInitEnv(rpmnix nix)
	/*@modifies nix @*/
{
    const char * s;

    s = getenv("TMPDIR");		nix->tmpDir = (s ? s : "/tmp");

    s = getenv("HOME");			nix->homeDir = (s ? s : "~");
    s = getenv("NIX_BIN_DIR");		nix->binDir = (s ? s : "/usr/bin");
    s = getenv("NIX_DATA_DIR");		nix->dataDir = (s ? s : "/usr/share");
    s = getenv("NIX_LIBEXEC_DIR");	nix->libexecDir = (s ? s : "/usr/libexec");

    s = getenv("NIX_STORE_DIR");	nix->storeDir = (s ? s : "/nix/store");
    s = getenv("NIX_STATE_DIR");	nix->stateDir = (s ? s : "/nix/var/nix");

    s = getenv("NIX_MANIFESTS_DIR");
    if (s)
	nix->manifestsPath = rpmGetPath(s, NULL);
    else
	nix->manifestsPath = rpmGetPath(nix->stateDir, "/manifests", NULL);
    /* XXX NIX_ROOTS_DIR? */
    nix->rootsPath = rpmGetPath(nix->stateDir, "/gcroots", NULL);
    /* XXX NIX_PROFILES_DIR? */
    nix->profilesPath = rpmGetPath(nix->stateDir, "/profiles", NULL);

#ifdef	NOTYET
    s = getenv("NIX_HAVE_TERMINAL");

    /* Hack to support the mirror:// scheme from Nixpkgs. */
    s = getenv("NIXPKGS_ALL");

    s = getenv("CURL_FLAGS");
#endif

    /* XXX nix-prefetch-url */
    s = getenv("QUIET");		nix->quiet = (s && *s ? 1 : 0);
    s = getenv("PRINT_PATHS");		nix->print = (s && *s ? 1 : 0);
    s = getenv("NIX_HASH_ALGO");	nix->hashAlgo = (s ? s : "sha256");
    s = getenv("NIX_DOWNLOAD_CACHE");	nix->downloadCache = (s ? s : NULL);
}

rpmnix rpmnixNew(char ** av, uint32_t flags, void * _tbl)
{
    rpmnix nix = rpmnixGetPool(_rpmnixPool);

    if (_tbl) {
	const poptOption tbl = (poptOption) _tbl;
	poptContext con;
        void *use =  nix->_item.use;
        void *pool = nix->_item.pool;

	memset(&_nix, 0, sizeof(_nix));
	_nix.flags = flags;
	con = rpmioInit(argvCount((ARGV_t)av), av, tbl);
	*nix = _nix;	/* structure assignment */
	memset(&_nix, 0, sizeof(_nix));

	nix->_item.use = use;
	nix->_item.pool = pool;
	nix->con = (void *) con;
	rpmnixInitEnv(nix);
    }

    return rpmnixLink(nix);
}

#ifdef	NOTYET
rpmRC rpmnixRun(rpmnix nix, const char * str, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

    if (nix == NULL) nix = rpmnixI();

    if (str != NULL) {
    }

DBG((stderr, "<== %s(%p,%p[%u]) rc %d\n", __FUNCTION__, nix, str, (unsigned)(str ? strlen(str) : 0), rc));

    return rc;
}
#endif

#include "system.h"

#define	_RPMNIX_INTERNAL
#include <rpmnix.h>
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

/*==============================================================*/

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

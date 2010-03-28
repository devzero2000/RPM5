#include "system.h"

#define	_RPMNIX_INTERNAL
#include <rpmnix.h>
#include <poptIO.h>

#include "debug.h"

/*==============================================================*/

static void writeManifest(rpmnix nix, const char * manifest)
	/*@*/
{
    char * fn = rpmGetPath(manifest, NULL);
    char * tfn = rpmGetPath(fn, ".tmp", NULL);
    const char * rval;
    const char * cmd;
    const char * s;
    FD_t fd;
    ssize_t nw;
    int xx;

DBG((stderr, "--> %s(%p, \"%s\")\n", __FUNCTION__, nix, manifest));

#ifdef	REFERENCE
/*
    my ($manifest, $narFiles, $patches) = @_;
*/
#endif

    fd = Fopen(tfn, "w");	/* XXX check exclusive */
    if (fd == NULL || Ferror(fd)) {
	if (fd) xx = Fclose(fd);
	goto exit;
    }
    
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

    if (fd)
	xx = Fclose(fd);

    if (Rename(tfn, fn) < 0) {
	fprintf(stderr, "Rename(%s, %s) failed\n", tfn, fn);
	exit(1);
    }
    tfn = _free(tfn);
    tfn = rpmGetPath(manifest, ".bz2.tmp", NULL);

    /* Create a bzipped manifest. */
    cmd = rpmExpand("/usr/libexec/nix/bzip2 < ", fn,
                " > ", tfn, "; echo $?", NULL);
    rval = rpmExpand("%(", cmd, ")", NULL);
    cmd = _free(cmd);
    if (strcmp(rval, "0")) {
	fprintf(stderr, "cannot compress manifest\n");
	exit(1);
    }
    rval = _free(rval);

    fn = _free(fn);
    fn = rpmGetPath(manifest, ".bz2", NULL);

    if (Rename(tfn, fn) < 0) {
	fprintf(stderr, "Rename(%s, %s) failed\n", tfn, fn);
	exit(1);
    }

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
    const char * rval;
    const char * cmd;

DBG((stderr, "--> %s(\"%s\", \"%s\")\n", __FUNCTION__, src, dst));
    /* XXX Ick. */
    cmd = rpmExpand("/bin/cp '", src, "' '", tfn, "'; echo $?", NULL);
    rval = rpmExpand("%(", cmd, ")", NULL);
    cmd = _free(cmd);
    if (strcmp(rval, "0")) {
	fprintf(stderr, "cannot copy file\n");
	exit(1);
    }
    rval = _free(rval);

    if (Rename(tfn, dst) < 0) {
	fprintf(stderr, "Rename(%s, %s) failed\n", tfn, dst);
	exit(1);
    }

    tfn = _free(tfn);
    return 0;
}

static int archiveExists(rpmnix nix, const char * name)
	/*@*/
{
DBG((stderr, "--> %s(%p, \"%s\")\n", __FUNCTION__, nix, name));
#ifdef	REFERENCE
/*
    my $name = shift;
    print STDERR "  HEAD on $archivesGetURL/$name\n";
    return system("$curl --head $archivesGetURL/$name > /dev/null") == 0;
*/
#endif
    const char * fn = rpmGetPath(nix->archivesGetURL, "/", name, NULL);
    struct stat sb;
    int rc = Stat(fn, &sb);
    fn = _free(fn);
    return (rc != 0);	/* XXX 0 on success */
}

/*==============================================================*/

#ifdef	UNUSED
static int verbose = 0;
#endif

static void nixPushArgCallback(poptContext con,
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

static struct poptOption nixPushOptions[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
	nixPushArgCallback, 0, NULL, NULL },
/*@=type@*/

 { "copy", '\0', POPT_BIT_SET,		&_nix.flags, RPMNIX_FLAGS_COPY,
        N_("FIXME"), NULL },
 { "target", '\0', POPT_ARG_STRING,	&_nix.targetArchivesUrl, 0,
        N_("FIXME"), NULL },

#ifndef	NOTYET
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
    rpmnix nix = rpmnixNew(argv, RPMNIX_FLAGS_NONE, nixPushOptions);
    int ac = 0;
    ARGV_t av = rpmnixArgv(nix, &ac);
    const char * s;
    int ec = 1;		/* assume failure */
    int xx;
    int i;

    const char * cmd;
    const char * rval;
    FD_t fd;
    ssize_t nw;
    ARGV_t tav;

    const char * curlDefault = "/usr/bin/curl --fail --silent";
    int localCopy;
    int nac;

    nix->tmpPath = mkdtemp(rpmGetPath(nix->tmpDir, "/nix-push.XXXXXX", NULL));
    if (nix->tmpPath == NULL) {
	fprintf(stderr, _("cannot create a temporary directory\n"));
	goto exit;
    }

    nix->nixExpr = rpmGetPath(nix->tmpPath, "/create-nars.nix", NULL);
    nix->manifest = rpmGetPath(nix->tmpPath, "/MANIFEST", NULL);

#ifdef	REFERENCE
/*
my $curl = "/usr/bin/curl --fail --silent";
my $extraCurlFlags = ${ENV{'CURL_FLAGS'}};
$curl = "$curl $extraCurlFlags" if defined $extraCurlFlags;
*/
#endif
    if ((s = getenv("CURL_FLAGS")))
	nix->curl = rpmExpand(curlDefault, " ", s, NULL);
    else
	nix->curl = rpmExpand(curlDefault, NULL);

    /* Parse the command line. */
    if (ac < 1) {
	poptPrintUsage(nix->con, stderr, 0);
	goto exit;
    }

    if (F_ISSET(nix, COPY)) {
	if (ac < 2) {
	    poptPrintUsage(nix->con, stderr, 0);
	    goto exit;
	}
	localCopy = 1;
	nix->localArchivesDir = av[0];
	nix->localManifestFile = av[1];
	if (nix->targetArchivesUrl == NULL)
	    nix->targetArchivesUrl = rpmExpand("file://", nix->localArchivesDir, NULL);
    } else {
	if (ac < 3) {
	    poptPrintUsage(nix->con, stderr, 0);
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
	cmd = rpmExpand(nix->binDir, "/nix-store --query --requisites --force-realise --include-outputs '", path, "'", NULL);
	rval = rpmExpand("%(", cmd, ")", NULL);
	xx = argvSplit(&nix->storePaths, rval, NULL);
	rval = _free(rval);
	cmd = _freeCmd(cmd);
    }

    /*
     * For each path, create a Nix expression that turns the path into
     * a Nix archive.
     */
#ifdef	REFERENCE
/*
my @storePaths = keys %storePaths;
*/
#endif

#ifdef	REFERENCE
/*
open NIX, ">$nixExpr";
print NIX "[";
*/
#endif
    fd = Fopen(nix->nixExpr, "w");
    if (fd == NULL || Ferror(fd)) {
	fprintf(stderr, "Fopen(%s, \"w\") failed.\n", nix->nixExpr);
	if (fd) xx = Fclose(fd);
	exit(1);
    }
    nw = Fwrite("[\n", 1, sizeof("[\n")-1, fd);

#ifdef	REFERENCE
/*
foreach my $storePath (@storePaths) {
    die unless ($storePath =~ /\/[0-9a-z]{32}[^\"\\\$]*$/);

    # Construct a Nix expression that creates a Nix archive.
    my $nixexpr = 
        "((import $dataDir/nix/corepkgs/nar/nar.nix) " .
        "{storePath = builtins.storePath \"$storePath\"; system = \"i686-linux\"; hashAlgo = \"$hashAlgo\";}) ";
    
    print NIX $nixexpr;
}
*/
#endif
    nac = argvCount(nix->storePaths);
    for (i = 0; i < nac; i++) {
	const char * storePath = nix->storePaths[i];
	const char * nixexpr = rpmExpand("(",
		"(import ", nix->dataDir, "/nix/corepkgs/nar/nar.nix)",
		" {",
		    "storePath = builtins.storePath \"", storePath, "\";",
		    "system = \"i686-linux\";",
		    "hashAlgo = \"", nix->hashAlgo, "\";",
		" }", ")", NULL);
	nw = Fwrite(nixexpr, 1, strlen(nixexpr), fd);
	nw = Fwrite("\n", 1, sizeof("\n")-1, fd);
	nixexpr = _free(nixexpr);
    }

#ifdef	REFERENCE
/*
print NIX "]";
close NIX;
*/
#endif
    nw = Fwrite("]\n", 1, sizeof("]\n")-1, fd);
    xx = Fclose(fd);

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
    cmd = rpmExpand(nix->binDir, "/nix-instantiate ", nix->nixExpr, NULL);
    rval = rpmExpand("%(", cmd, ")", NULL);
    xx = argvSplit(&nix->storeExprs, rval, NULL);
    rval = _free(rval);
    cmd = _freeCmd(cmd);

    /* Build the derivations. */
    fprintf(stderr, "creating archives...\n");

#ifdef	REFERENCE
/*
my @tmp = @storeExprs;
*/
#endif
    tav = nix->storeExprs;

#ifdef	REFERENCE
/*
while (scalar @tmp > 0)
*/
#endif
    nac = argvCount(tav);
    for (i = 0; i < nac; i++) {
	const char * tmp2;
	
	/* XXX this appears to attempt chunks < 256 for --realise */
#ifdef	REFERENCE
/*
    my $n = scalar @tmp;
    if ($n > 256) { $n = 256 };
    my @tmp2 = @tmp[0..$n - 1];
    @tmp = @tmp[$n..scalar @tmp - 1];
*/
#endif
	/* XXX HACK: do one-by-one for now. */
	tmp2 = tav[i];

#ifdef	REFERENCE
/*
    my $pid = open(READ, "$binDir/nix-store --realise @tmp2|")
        or die "cannot run nix-store";
    while (<READ>) {
        chomp;
        die unless (/^\//);
        push @narPaths, "$_";
    }
    close READ or die "nix-store failed: $?";
*/
#endif
	cmd = rpmExpand(nix->binDir, "/nix-store --realise ", tmp2, NULL);
	rval = rpmExpand("%(", cmd, ")", NULL);
	xx = argvSplit(&nix->narPaths, rval, NULL);
	rval = _free(rval);
	cmd = _freeCmd(cmd);
    }

    /* Create the manifest. */
    fprintf(stderr, "creating manifest...\n");

#ifdef	REFERENCE
/*
for (my $n = 0; $n < scalar @storePaths; $n++)
*/
#endif
    nac = argvCount(nix->storePaths);
    for (i = 0; i < nac; i++) {
	const char * storePath = nix->storePaths[i];
	const char * narDir = nix->narPaths[i];

	struct stat sb;
	const char * references;
	const char * deriver;
	const char * url;
	const char * narbz2Hash;
	const char * narName;
	const char * narFile;
	off_t narbz2Size;

#ifdef	REFERENCE
/*
    open HASH, "$narDir/narbz2-hash" or die "cannot open narbz2-hash";
    my $narbz2Hash = <HASH>;
    chomp $narbz2Hash;
    $narbz2Hash =~ /^[0-9a-z]+$/ or die "invalid hash";
    close HASH;
*/
#endif
	/* XXX Ick. */
	cmd = rpmExpand("/bin/cat ", narDir, "/narbz2-hash", NULL);
	narbz2Hash = rpmExpand("%(", cmd, ")", NULL);
	cmd = _freeCmd(cmd);

#ifdef	REFERENCE
/*
    open HASH, "$narDir/nar-hash" or die "cannot open nar-hash";
    my $narHash = <HASH>;
    chomp $narHash;
    $narHash =~ /^[0-9a-z]+$/ or die "invalid hash";
    close HASH;
*/
#endif
	/* XXX Ick. */
	cmd = rpmExpand("/bin/cat ", narDir, "/nar-hash", NULL);
	narbz2Hash = rpmExpand("%(", cmd, ")", NULL);
	cmd = _freeCmd(cmd);
    
	narName = rpmExpand(narbz2Hash, ".nar.bz2", NULL);
	narFile = rpmGetPath(narDir, "/", narName, NULL);
	if (Lstat(narFile, &sb) < 0) {
	    fprintf(stderr, "narfile for %s not found\n", storePath);
	    exit(1);
	}
	xx = argvAdd(&nix->narArchives, narFile);

	xx = Stat(narFile, &sb);
	narbz2Size = sb.st_size;

#ifdef	REFERENCE
/*
    my $references = `$binDir/nix-store --query --references '$storePath'`;
    die "cannot query references for `$storePath'" if $? != 0;
    $references = join(" ", split(" ", $references));
*/
#endif
	cmd = rpmExpand(nix->binDir, "/nix-store --query --references '",
		storePath, "'", NULL);
	references = rpmExpand("%(", cmd, ")", NULL);
	cmd = _freeCmd(cmd);

#ifdef	REFERENCE
/*
    my $deriver = `$binDir/nix-store --query --deriver '$storePath'`;
    die "cannot query deriver for `$storePath'" if $? != 0;
    chomp $deriver;
    $deriver = "" if $deriver eq "unknown-deriver";
*/
#endif
	cmd = rpmExpand(nix->binDir, "/nix-store --query",
		" --deriver '", storePath, "'", NULL);
	deriver = rpmExpand("%(", cmd, ")", NULL);
	if (!strcmp(deriver, "unknown-deriver")) {
	    deriver = _free(deriver);
	    deriver = xstrdup("");
	}
	cmd = _freeCmd(cmd);

	if (localCopy)
	    url = rpmGetPath(nix->targetArchivesUrl, "/", narName, NULL);
	else
	    url = rpmGetPath(nix->archivesGetURL, "/", narName, NULL);

#ifdef	REFERENCE
/*
    $narFiles{$storePath} = [
        { url => $url
        , hash => "$hashAlgo:$narbz2Hash"
        , size => $narbz2Size
        , narHash => "$hashAlgo:$narHash"
        , references => $references
        , deriver => $deriver
        }
    ];
*/
#endif
    }

    writeManifest(nix, nix->manifest);

    /* Upload/copy the archives. */
    fprintf(stderr, "uploading/copying archives...\n");

    nac = argvCount(nix->narArchives);
    for (i = 0; i < nac; i++) {
	char * narArchive = xstrdup(av[i]);
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
	    if (!archiveExists(nix, bn)) {
		fprintf(stderr, "  %s\n", narArchive);
#ifdef	REFERENCE
/*
            system("$curl --show-error --upload-file " .
                   "'$narArchive' '$archivesPutURL/$basename' > /dev/null") == 0 or
                   die "curl failed on $narArchive: $?";
*/
#endif
		cmd = rpmExpand(nix->curl, " --show-error",
			" --upload-file '", narArchive, "' '", nix->manifestPutURL, "/", bn, "'",
			" > /dev/null; echo $?", NULL);
		rval = rpmExpand("%(", cmd, ")", NULL);
		if (strcmp(rval, "0")) {
		    fprintf(stderr, "curl failed on %s: %s\n", narArchive, rval);
		    exit(1);
		}
		rval = _free(rval);
		cmd = _freeCmd(cmd);
	    }
	}
	narArchive = _free(narArchive);
    }

    /* Upload the manifest. */
    fprintf(stderr, "uploading manifest...\n");

    if (localCopy) {
	const char * bmanifest = rpmGetPath(nix->manifest, ".bz2", NULL);
	const char * blocalManifestFile =
		rpmGetPath(nix->localManifestFile, ".bz2", NULL);

	xx = copyFile(nix->manifest, nix->localManifestFile);
	xx = copyFile(bmanifest, blocalManifestFile);
	bmanifest = _free(bmanifest);
	blocalManifestFile = _free(blocalManifestFile);
    } else {
#ifdef	REFERENCE
/*
    system("$curl --show-error --upload-file " .
           "'$manifest' '$manifestPutURL' > /dev/null") == 0 or
           die "curl failed on $manifest: $?";
*/
#endif
	cmd = rpmExpand(nix->curl, " --show-error",
		" --upload-file '", nix->manifest, "' '", nix->manifestPutURL, "'",
		" > /dev/null; echo $?", NULL);
	rval = rpmExpand("%(", cmd, ")", NULL);
	if (strcmp(rval, "0")) {
	    fprintf(stderr, "curl failed on %s: %s\n", nix->manifest, rval);
	    exit(1);
	}
	rval = _free(rval);
	cmd = _freeCmd(cmd);

#ifdef	REFERENCE
/*
    system("$curl --show-error --upload-file " .
           "'$manifest'.bz2 '$manifestPutURL'.bz2 > /dev/null") == 0 or
           die "curl failed on $manifest: $?";
*/
#endif
	cmd = rpmExpand(nix->curl, " --show-error",
		" --upload-file '", nix->manifest, ".bz2' '", nix->manifestPutURL, ".bz2'",
		" > /dev/null; echo $?", NULL);
	rval = rpmExpand("%(", cmd, ")", NULL);
	if (strcmp(rval, "0")) {
	    fprintf(stderr, "curl failed on %s.bz2: %s\n", nix->manifest, rval);
	    exit(1);
	}
	rval = _free(rval);
	cmd = _freeCmd(cmd);
    }

    ec = 0;	/* XXX success */

exit:
#ifdef	REFERENCE
/*
my $tmpDir = tempdir("nix-push.XXXXXX", CLEANUP => 1, TMPDIR => 1)
    or die "cannot create a temporary directory";
*/
#endif

    nix = rpmnixFree(nix);

    return ec;
}

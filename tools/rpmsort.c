#include "system.h"

#include <rpmlib.h>
#include <rpmmacro.h>
#include <rpmurl.h>

#include "depends.h"

#include "manifest.h"
#include "misc.h"
#include "debug.h"

static int _depends_debug;

static int noAvailable = 1;
static const char * avdbpath =
	"/usr/lib/rpmdb/%{_arch}-%{_vendor}-%{_os}/redhat";
static int noChainsaw = 0;
static int noDeps = 0;

/**
 * Compare two available index entries by name (qsort/bsearch).
 * @param one		1st available index entry
 * @param two		2nd available index entry
 * @return		result of comparison
 */
static int indexcmp(const void * one, const void * two)		/*@*/
{
    const struct availableIndexEntry * a = one;
    const struct availableIndexEntry * b = two;
    int lenchk = a->entryLen - b->entryLen;

    if (lenchk)
	return lenchk;

    return strcmp(a->entry, b->entry);
}

/**
 * Compare two directory info entries by name (qsort/bsearch).
 * @param one		1st directory info
 * @param two		2nd directory info
 * @return		result of comparison
 */
static int dirInfoCompare(const void * one, const void * two)	/*@*/
{
    const dirInfo a = (const dirInfo) one;
    const dirInfo b = (const dirInfo) two;
    int lenchk = a->dirNameLen - b->dirNameLen;

    if (lenchk)
	return lenchk;

    /* XXX FIXME: this might do "backward" strcmp for speed */
    return strcmp(a->dirName, b->dirName);
}

/**
 * Check added package file lists for package(s) that provide a file.
 * @param al		available list
 * @param keyType	type of dependency
 * @param fileName	file name to search for
 * @return		available package pointer
 */
static /*@only@*/ /*@null@*/ struct availablePackage **
alAllFileSatisfiesDepend(const availableList al,
		const char * keyType, const char * fileName)
	/*@*/
{
    int i, found;
    const char * dirName;
    const char * baseName;
    struct dirInfo_s dirNeedle;
    dirInfo dirMatch;
    struct availablePackage ** ret;

    /* Solaris 2.6 bsearch sucks down on this. */
    if (al->numDirs == 0 || al->dirs == NULL || al->list == NULL)
	return NULL;

    {	char * t;
	dirName = t = xstrdup(fileName);
	if ((t = strrchr(t, '/')) != NULL) {
	    t++;		/* leave the trailing '/' */
	    *t = '\0';
	}
    }

    dirNeedle.dirName = (char *) dirName;
    dirNeedle.dirNameLen = strlen(dirName);
    dirMatch = bsearch(&dirNeedle, al->dirs, al->numDirs,
		       sizeof(dirNeedle), dirInfoCompare);
    if (dirMatch == NULL) {
	dirName = _free(dirName);
	return NULL;
    }

    /* rewind to the first match */
    while (dirMatch > al->dirs && dirInfoCompare(dirMatch-1, &dirNeedle) == 0)
	dirMatch--;

    /*@-nullptrarith@*/		/* FIX: fileName NULL ??? */
    baseName = strrchr(fileName, '/') + 1;
    /*@=nullptrarith@*/

    for (found = 0, ret = NULL;
	 dirMatch <= al->dirs + al->numDirs &&
		dirInfoCompare(dirMatch, &dirNeedle) == 0;
	 dirMatch++)
    {
	/* XXX FIXME: these file lists should be sorted and bsearched */
	for (i = 0; i < dirMatch->numFiles; i++) {
	    if (dirMatch->files[i].baseName == NULL ||
			strcmp(dirMatch->files[i].baseName, baseName))
		continue;

	    /*
	     * If a file dependency would be satisfied by a file
	     * we are not going to install, skip it.
	     */
	    if (al->list[dirMatch->files[i].pkgNum].multiLib &&
			!isFileMULTILIB(dirMatch->files[i].fileFlags))
	        continue;

	    if (keyType)
		rpmMessage(RPMMESS_DEBUG, _("%s: %-45s YES (added files)\n"),
			keyType, fileName);

	    ret = xrealloc(ret, (found+2) * sizeof(*ret));
	    if (ret)	/* can't happen */
		ret[found++] = al->list + dirMatch->files[i].pkgNum;
	    /*@innerbreak@*/ break;
	}
    }

    dirName = _free(dirName);
    /*@-mods@*/		/* FIX: al->list might be modified. */
    if (ret)
	ret[found] = NULL;
    /*@=mods@*/
    return ret;
}

/**
 * Check added package file lists for package(s) that have a provide.
 * @param al		available list
 * @param keyType	type of dependency
 * @param keyDepend	dependency string representation
 * @param keyName	dependency name string
 * @param keyEVR	dependency [epoch:]version[-release] string
 * @param keyFlags	dependency logical range qualifiers
 * @return		available package pointer
 */
static /*@only@*/ /*@null@*/ struct availablePackage **
alAllSatisfiesDepend(const availableList al,
		const char * keyType, const char * keyDepend,
		const char * keyName, const char * keyEVR, int keyFlags)
	/*@*/
{
    struct availableIndexEntry needle, * match;
    struct availablePackage * p, ** ret = NULL;
    int i, rc, found;

    if (*keyName == '/') {
	ret = alAllFileSatisfiesDepend(al, keyType, keyName);
	/* XXX Provides: /path was broken with added packages (#52183). */
	if (ret != NULL && *ret != NULL)
	    return ret;
    }

    if (!al->index.size || al->index.index == NULL) return NULL;

    needle.entry = keyName;
    needle.entryLen = strlen(keyName);
    match = bsearch(&needle, al->index.index, al->index.size,
		    sizeof(*al->index.index), indexcmp);

    if (match == NULL) return NULL;

    /* rewind to the first match */
    while (match > al->index.index && indexcmp(match-1, &needle) == 0)
	match--;

    for (ret = NULL, found = 0;
	 match <= al->index.index + al->index.size &&
		indexcmp(match, &needle) == 0;
	 match++)
    {

	p = match->package;
	rc = 0;
	switch (match->type) {
	case IET_PROVIDES:
	    i = match->entryIx;
	    {	const char * proEVR;
		int proFlags;

		proEVR = (p->providesEVR ? p->providesEVR[i] : NULL);
		proFlags = (p->provideFlags ? p->provideFlags[i] : 0);
		rc = rpmRangesOverlap(p->provides[i], proEVR, proFlags,
				keyName, keyEVR, keyFlags);
		if (rc)
		    /*@innerbreak@*/ break;
	    }
	    if (keyType && keyDepend && rc)
		rpmMessage(RPMMESS_DEBUG, _("%s: %-45s YES (added provide)\n"),
				keyType, keyDepend+2);
	    break;
	}

	if (rc) {
	    ret = xrealloc(ret, (found + 2) * sizeof(*ret));
	    if (ret)	/* can't happen */
		ret[found++] = p;
	}
    }

    if (ret)
	ret[found] = NULL;

    return ret;
}

/**
 * Check added package file lists for first package that has a provide.
 * @todo Eliminate.
 * @param al		available list
 * @param keyType	type of dependency
 * @param keyDepend	dependency string representation
 * @param keyName	dependency name string
 * @param keyEVR	dependency [epoch:]version[-release] string
 * @param keyFlags	dependency logical range qualifiers
 * @return		available package pointer
 */
static inline /*@only@*/ /*@null@*/ struct availablePackage *
alSatisfiesDepend(const availableList al,
		const char * keyType, const char * keyDepend,
		const char * keyName, const char * keyEVR, int keyFlags)
	/*@*/
{
    struct availablePackage * ret;
    struct availablePackage ** tmp =
	alAllSatisfiesDepend(al, keyType, keyDepend, keyName, keyEVR, keyFlags);

    if (tmp) {
	ret = tmp[0];
	tmp = _free(tmp);
	return ret;
    }
    return NULL;
}

static int
do_tsort(const char *fileArgv[])
{
    const char * rootdir = "/";
    rpmdb rpmdb = NULL;
    rpmTransactionSet ts = NULL;
    const char ** pkgURL = NULL;
    char * pkgState = NULL;
    const char ** fnp;
    const char * fileURL = NULL;
    int numPkgs = 0;
    int numFailed = 0;
    int prevx;
    int pkgx;
    const char ** argv = NULL;
    int argc = 0;
    const char ** av = NULL;
    int ac = 0;
    Header h;
    int rc = 0;
    int i;

    if (fileArgv == NULL)
	return 0;

    rc = rpmdbOpen(rootdir, &rpmdb, O_RDONLY, 0644);
    if (rc) {
	rpmMessage(RPMMESS_ERROR, _("cannot open Packages database\n"));
	rc = -1;
	goto exit;
    }

    ts = rpmtransCreateSet(rpmdb, rootdir);
    if (!noChainsaw)
	ts->transFlags = RPMTRANS_FLAG_CHAINSAW;

    /* Load all the available packages. */
    if (!(noDeps || noAvailable)) {
	rpmdbMatchIterator mi = NULL;
	struct rpmdb_s * avdb = NULL;

	addMacro(NULL, "_dbpath", NULL, avdbpath, RMIL_CMDLINE);
	rc = rpmdbOpen(rootdir, &avdb, O_RDONLY, 0644);
	delMacro(NULL, "_dbpath");
	if (rc) {
	    rpmMessage(RPMMESS_ERROR, _("cannot open Available database\n"));
	    goto endavail;
	}
        mi = rpmdbInitIterator(avdb, RPMDBI_PACKAGES, NULL, 0);
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    rpmtransAvailablePackage(ts, h, NULL);
	}

endavail:
	if (mi) rpmdbFreeIterator(mi);
	if (avdb) rpmdbClose(avdb);
    }

    /* Build fully globbed list of arguments in argv[argc]. */
    for (fnp = fileArgv; *fnp; fnp++) {
	av = _free(av);
	ac = 0;
	rc = rpmGlob(*fnp, &ac, &av);
	if (rc || ac == 0) continue;

	argv = (argc == 0)
	    ? xmalloc((argc+2) * sizeof(*argv))
	    : xrealloc(argv, (argc+2) * sizeof(*argv));
	memcpy(argv+argc, av, ac * sizeof(*av));
	argc += ac;
	argv[argc] = NULL;
    }
    av = _free(av);

    numPkgs = 0;
    prevx = 0;
    pkgx = 0;

restart:
    /* Allocate sufficient storage for next set of args. */
    if (pkgx >= numPkgs) {
	numPkgs = pkgx + argc;
	pkgURL = (pkgURL == NULL)
	    ? xmalloc( (numPkgs + 1) * sizeof(*pkgURL))
	    : xrealloc(pkgURL, (numPkgs + 1) * sizeof(*pkgURL));
	memset(pkgURL + pkgx, 0, ((argc + 1) * sizeof(*pkgURL)));
	pkgState = (pkgState == NULL)
	    ? xmalloc( (numPkgs + 1) * sizeof(*pkgState))
	    : xrealloc(pkgState, (numPkgs + 1) * sizeof(*pkgState));
	memset(pkgState + pkgx, 0, ((argc + 1) * sizeof(*pkgState)));
    }

    /* Copy next set of args. */
    for (i = 0; i < argc; i++) {
	fileURL = _free(fileURL);
	fileURL = argv[i];
	argv[i] = NULL;
	pkgURL[pkgx] = fileURL;
	fileURL = NULL;
	pkgx++;
    }
    fileURL = _free(fileURL);

    /* Continue processing file arguments, building transaction set. */
    for (fnp = pkgURL+prevx; *fnp; fnp++, prevx++) {
	const char * fileName;
	int isSource;
	FD_t fd;

	(void) urlPath(*fnp, &fileName);

	/* Try to read the header from a package file. */
	fd = Fopen(*fnp, "r.ufdio");
	if (fd == NULL || Ferror(fd)) {
	    rpmError(RPMERR_OPEN, _("open of %s failed: %s\n"), *fnp,
			Fstrerror(fd));
	    if (fd) Fclose(fd);
	    numFailed++; *fnp = NULL;
	    continue;
	}

	rc = rpmReadPackageHeader(fd, &h, &isSource, NULL, NULL);
	Fclose(fd);

	if (rc == 2) {
	    numFailed++; *fnp = NULL;
	    continue;
	}

	if (rc == 0) {
	    rc = rpmtransAddPackage(ts, h, NULL, fileName, 0, NULL);
	    headerFree(h);  /* XXX reference held by transaction set */
	    continue;
	}

	if (rc != 1) {
	    rpmMessage(RPMMESS_ERROR, _("%s cannot be installed\n"), *fnp);
	    numFailed++; *fnp = NULL;
	    break;
	}

	/* Try to read a package manifest. */
	fd = Fopen(*fnp, "r.fpio");
	if (fd == NULL || Ferror(fd)) {
	    rpmError(RPMERR_OPEN, _("open of %s failed: %s\n"), *fnp,
			Fstrerror(fd));
	    if (fd) Fclose(fd);
	    numFailed++; *fnp = NULL;
	    break;
	}

	/* Read list of packages from manifest. */
	rc = rpmReadPackageManifest(fd, &argc, &argv);
	if (rc)
	    rpmError(RPMERR_MANIFEST, _("%s: read manifest failed: %s\n"),
			fileURL, Fstrerror(fd));
	Fclose(fd);

	/* If successful, restart the query loop. */
	if (rc == 0) {
	    prevx++;
	    goto restart;
	}

	numFailed++; *fnp = NULL;
	break;
    }

    if (numFailed) goto exit;

    if (!noDeps) {
	rpmDependencyConflict conflicts = NULL;
	int numConflicts = 0;

	rc = rpmdepCheck(ts, &conflicts, &numConflicts);

	if (conflicts) {
	    rpmMessage(RPMMESS_ERROR, _("failed dependencies:\n"));
	    printDepProblems(stderr, conflicts, numConflicts);
	    conflicts = rpmdepFreeConflicts(conflicts, numConflicts);
	    rc = -1;
	    goto exit;
	}
    }

    rc = rpmdepOrder(ts);
    if (rc)
	goto exit;

    {	int oc;
	int npkgs = ts->orderCount;
	struct availablePackage * p;
	struct availablePackage * q;
	unsigned char * selected = alloca(sizeof(*selected) * (npkgs + 1));
	int i, j;

fprintf(stdout, "digraph XXX {\n");

	for (oc = 0; oc < npkgs; oc++) {
	    p = NULL;

	    switch (ts->order[oc].type) {
	    case TR_ADDED:
		i = ts->order[oc].u.addedIndex;
		p = ts->addedPackages.list + ts->order[oc].u.addedIndex;
		break;
	    case TR_REMOVED:
		continue;
		break;
	    }

fprintf(stdout, "\"%s\"\n", p->name);

	}

	for (oc = 0; oc < npkgs; oc++) {
	    int matchNum;

	    p = NULL;
	    switch (ts->order[oc].type) {
	    case TR_ADDED:
		i = ts->order[oc].u.addedIndex;
		p = ts->addedPackages.list + ts->order[oc].u.addedIndex;
		break;
	    case TR_REMOVED:
		continue;
		break;
	    }

	    if (p->requiresCount <= 0)
		continue;

	    memset(selected, 0, sizeof(*selected) * npkgs);
	    matchNum = p - ts->addedPackages.list;
	    selected[matchNum] = 1;

	    for (j = 0; j < p->requiresCount; j++) {
		q = alSatisfiesDepend(&ts->addedPackages, NULL, NULL,
			p->requires[j], p->requiresEVR[j], p->requireFlags[j]);
		if (q == NULL)
		    continue;
		if (!strncmp(p->requires[j], "rpmlib(", sizeof("rpmlib(")-1))
		    continue;

		matchNum = q - ts->addedPackages.list;
		if (selected[matchNum] != 0)
		    continue;
		selected[matchNum] = 1;

fprintf(stdout, "\"%s\" -> \"%s\"\n", p->name, q->name);

	    }

	}

fprintf(stdout, "}\n");

    }

    rc = 0;

exit:
    if (ts) rpmtransFree(ts);
    for (i = 0; i < numPkgs; i++) {
        pkgURL[i] = _free(pkgURL[i]);
    }
    pkgState = _free(pkgState);
    pkgURL = _free(pkgURL);
    argv = _free(argv);
    if (rpmdb) rpmdbClose(rpmdb);
    return rc;
}

static struct poptOption optionsTable[] = {
 { "noavailable", '\0', 0, &noAvailable, 0,	NULL, NULL},
 { "nochainsaw", '\0', 0, &noChainsaw, 0,	NULL, NULL},
 { "nodeps", '\0', 0, &noDeps, 0,		NULL, NULL},
 { "verbose", 'v', 0, 0, 'v',			NULL, NULL},
 { NULL,	0, 0, 0, 0,			NULL, NULL}
};

int
main(int argc, const char *argv[])
{
    poptContext optCon;
    const char * optArg;
    int arg;
    int ec = 0;

#if HAVE_MCHECK_H && HAVE_MTRACE
    mtrace();	/* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif
    setprogname(argv[0]);	/* Retrofit glibc __progname */
    (void)setlocale(LC_ALL, "" );

#ifdef  __LCLINT__
#define LOCALEDIR	"/usr/share/locale"
#endif
    (void)bindtextdomain(PACKAGE, LOCALEDIR);
    (void)textdomain(PACKAGE);

    _depends_debug = 1;

    optCon = poptGetContext("rpmsort", argc, argv, optionsTable, 0);
    poptReadDefaultConfig(optCon, 1);

    while ((arg = poptGetNextOpt(optCon)) > 0) {
	optArg = poptGetOptArg(optCon);
	switch (arg) {
	case 'v':
	    rpmIncreaseVerbosity();
	    break;
	default:
	    fprintf(stderr, _("unknown popt return (%d)"), arg);
	    exit (EXIT_FAILURE);
	    /*@notreached@*/ break;
	}
    }

    rpmReadConfigFiles(NULL, NULL);

    ec = do_tsort(poptGetArgs(optCon));

    optCon = poptFreeContext(optCon);

    return ec;
}

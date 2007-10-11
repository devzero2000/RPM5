/** \ingroup rpmcli
 * \file lib/rpminstall.c
 */

#include "system.h"

#include <rpmio.h>
#include <rpmcli.h>

#include "rpmdb.h"
#ifdef	NOTYET
#include "rpmds.h"		/* XXX ts->suggests, +foo -foo =foo args */
#endif

#include "rpmte.h"		/* XXX rpmtsPrint() */
#define	_RPMTS_INTERNAL		/* XXX ts->suggests */
#include "rpmts.h"

#include "manifest.h"
#include "rpmgi.h"
#include "debug.h"

/*@access FD_t @*/	/* XXX void * arg */
/*@access rpmts @*/	/* XXX ts->suggests */
/*@access fnpyKey @*/	/* XXX cast */

/*@unchecked@*/
int rpmcliPackagesTotal = 0;
/*@unchecked@*/
int rpmcliHashesCurrent = 0;
/*@unchecked@*/
int rpmcliHashesTotal = 0;
/*@unchecked@*/
unsigned long long rpmcliProgressCurrent = 0;
/*@unchecked@*/
unsigned long long rpmcliProgressTotal = 0;

/**
 * Print a CLI progress bar.
 * @todo Unsnarl isatty(STDOUT_FILENO) from the control flow.
 * @param amount	current
 * @param total		final
 */
static void printHash(const unsigned long long amount, const unsigned long long total)
	/*@globals rpmcliHashesCurrent, rpmcliHashesTotal,
		rpmcliProgressCurrent, fileSystem @*/
	/*@modifies rpmcliHashesCurrent, rpmcliHashesTotal,
		rpmcliProgressCurrent, fileSystem @*/
{
    int hashesNeeded;

    rpmcliHashesTotal = (isatty (STDOUT_FILENO) ? 44 : 50);

    if (rpmcliHashesCurrent != rpmcliHashesTotal) {
/*@+relaxtypes@*/
	float pct = (total ? (((float) amount) / total) : 1.0);
	hashesNeeded = (rpmcliHashesTotal * pct) + 0.5;
/*@=relaxtypes@*/
	while (hashesNeeded > rpmcliHashesCurrent) {
	    if (isatty (STDOUT_FILENO)) {
		int i;
		for (i = 0; i < rpmcliHashesCurrent; i++)
		    (void) putchar ('#');
		for (; i < rpmcliHashesTotal; i++)
		    (void) putchar (' ');
		fprintf(stdout, "(%3d%%)", (int)((100 * pct) + 0.5));
		for (i = 0; i < (rpmcliHashesTotal + 6); i++)
		    (void) putchar ('\b');
	    } else
		fprintf(stdout, "#");

	    rpmcliHashesCurrent++;
	}
	(void) fflush(stdout);

	if (rpmcliHashesCurrent == rpmcliHashesTotal) {
	    int i;
	    rpmcliProgressCurrent++;
	    if (isatty(STDOUT_FILENO)) {
	        for (i = 1; i < rpmcliHashesCurrent; i++)
		    (void) putchar ('#');
/*@+relaxtypes@*/
		pct = (rpmcliProgressTotal
		    ? (((float) rpmcliProgressCurrent) / rpmcliProgressTotal)
		    : 1);
/*@=relaxtypes@*/
		fprintf(stdout, " [%3d%%]", (int)((100 * pct) + 0.5));
	    }
	    fprintf(stdout, "\n");
	}
	(void) fflush(stdout);
    }
}

void * rpmShowProgress(/*@null@*/ const void * arg,
			const rpmCallbackType what,
			const unsigned long long amount,
			const unsigned long long total,
			/*@null@*/ fnpyKey key,
			/*@null@*/ void * data)
	/*@globals rpmcliHashesCurrent, rpmcliProgressCurrent, rpmcliProgressTotal,
		fileSystem @*/
	/*@modifies rpmcliHashesCurrent, rpmcliProgressCurrent, rpmcliProgressTotal,
		fileSystem @*/
{
/*@-abstract -castexpose @*/
    Header h = (Header) arg;
/*@=abstract =castexpose @*/
    char * s;
    int flags = (int) ((long)data);
    void * rc = NULL;
/*@-abstract -assignexpose @*/
    const char * filename = (const char *)key;
/*@=abstract =assignexpose @*/
    static FD_t fd = NULL;
    int xx;

    switch (what) {
    case RPMCALLBACK_INST_OPEN_FILE:
	if (filename == NULL || filename[0] == '\0')
	    return NULL;
	fd = Fopen(filename, "r.fdio");
	/*@-type@*/ /* FIX: still necessary? */
	if (fd == NULL || Ferror(fd)) {
	    rpmlog(RPMLOG_ERR, _("open of %s failed: %s\n"), filename,
			Fstrerror(fd));
	    if (fd != NULL) {
		xx = Fclose(fd);
		fd = NULL;
	    }
	} else
	    fd = fdLink(fd, "persist (showProgress)");
	/*@=type@*/
/*@+voidabstract@*/
	return (void *)fd;
/*@=voidabstract@*/
	/*@notreached@*/ break;

    case RPMCALLBACK_INST_CLOSE_FILE:
	/*@-type@*/ /* FIX: still necessary? */
	fd = fdFree(fd, "persist (showProgress)");
	/*@=type@*/
	if (fd != NULL) {
	    xx = Fclose(fd);
	    fd = NULL;
	}
	break;

    case RPMCALLBACK_INST_START:
	rpmcliHashesCurrent = 0;
	if (h == NULL || !(flags & INSTALL_LABEL))
	    break;
	/* @todo Remove headerSprintf() on a progress callback. */
	if (flags & INSTALL_HASH) {
	    s = headerSprintf(h, "%{NAME}",
				rpmTagTable, rpmHeaderFormats, NULL);
	    if (isatty (STDOUT_FILENO))
		fprintf(stdout, "%4d:%-23.23s", (int)rpmcliProgressCurrent + 1, s);
	    else
		fprintf(stdout, "%-28.28s", s);
	    (void) fflush(stdout);
	    s = _free(s);
	} else {
	    s = headerSprintf(h, "%{NAME}-%{VERSION}-%{RELEASE}",
				  rpmTagTable, rpmHeaderFormats, NULL);
	    fprintf(stdout, "%s\n", s);
	    (void) fflush(stdout);
	    s = _free(s);
	}
	break;

    case RPMCALLBACK_TRANS_PROGRESS:
    case RPMCALLBACK_INST_PROGRESS:
/*@+relaxtypes@*/
	if (flags & INSTALL_PERCENT)
	    fprintf(stdout, "%%%% %f\n", (double) (total
				? ((((float) amount) / total) * 100)
				: 100.0));
	else if (flags & INSTALL_HASH)
	    printHash(amount, total);
/*@=relaxtypes@*/
	(void) fflush(stdout);
	break;

    case RPMCALLBACK_TRANS_START:
	rpmcliHashesCurrent = 0;
	rpmcliProgressTotal = 1;
	rpmcliProgressCurrent = 0;
	if (!(flags & INSTALL_LABEL))
	    break;
	if (flags & INSTALL_HASH)
	    fprintf(stdout, "%-28s", _("Preparing..."));
	else
	    fprintf(stdout, "%s\n", _("Preparing packages for installation..."));
	(void) fflush(stdout);
	break;

    case RPMCALLBACK_TRANS_STOP:
	if (flags & INSTALL_HASH)
	    printHash(1, 1);	/* Fixes "preparing..." progress bar */
	rpmcliProgressTotal = rpmcliPackagesTotal;
	rpmcliProgressCurrent = 0;
	break;

    case RPMCALLBACK_REPACKAGE_START:
	rpmcliHashesCurrent = 0;
	rpmcliProgressTotal = total;
	rpmcliProgressCurrent = 0;
	if (!(flags & INSTALL_LABEL))
	    break;
	if (flags & INSTALL_HASH)
	    fprintf(stdout, "%-28s\n", _("Repackaging..."));
	else
	    fprintf(stdout, "%s\n", _("Repackaging erased files..."));
	(void) fflush(stdout);
	break;

    case RPMCALLBACK_REPACKAGE_PROGRESS:
	if (amount && (flags & INSTALL_HASH))
	    printHash(1, 1);	/* Fixes "preparing..." progress bar */
	break;

    case RPMCALLBACK_REPACKAGE_STOP:
	rpmcliProgressTotal = total;
	rpmcliProgressCurrent = total;
	if (flags & INSTALL_HASH)
	    printHash(1, 1);	/* Fixes "preparing..." progress bar */
	rpmcliProgressTotal = rpmcliPackagesTotal;
	rpmcliProgressCurrent = 0;
	if (!(flags & INSTALL_LABEL))
	    break;
	if (flags & INSTALL_HASH)
	    fprintf(stdout, "%-28s\n", _("Upgrading..."));
	else
	    fprintf(stdout, "%s\n", _("Upgrading packages..."));
	(void) fflush(stdout);
	break;

    case RPMCALLBACK_UNINST_PROGRESS:
	break;
    case RPMCALLBACK_UNINST_START:
	break;
    case RPMCALLBACK_UNINST_STOP:
	break;
    case RPMCALLBACK_UNPACK_ERROR:
	break;
    case RPMCALLBACK_CPIO_ERROR:
	break;
    case RPMCALLBACK_UNKNOWN:
    default:
	break;
    }

    return rc;
}	

int rpmcliInstallProblems(rpmts ts, const char * msg, int rc)
	/*@globals fileSystem @*/
	/*@modifies ts, fileSystem @*/
{
    rpmps ps = rpmtsProblems(ts);

    if (rc && rpmpsNumProblems(ps) > 0) {
	if (msg)
	    rpmlog(RPMLOG_ERR, "%s:\n", msg);
	rpmpsPrint(NULL, ps);
    }
    ps = rpmpsFree(ps);
    return rc;
}

int rpmcliInstallSuggests(rpmts ts)
{
    if (ts->suggests != NULL && ts->nsuggests > 0) {
	const char * s;
	int i;

	rpmlog(RPMLOG_NOTICE, _("    Suggested resolutions:\n"));
	for (i = 0; i < ts->nsuggests && (s = ts->suggests[i]) != NULL;
	    ts->suggests[i++] = s = _free(s))
	{
	    rpmlog(RPMLOG_NOTICE, "\t%s\n", s);
	}
	ts->suggests = _free(ts->suggests);
    }
    return 0;
}

int rpmcliInstallCheck(rpmts ts)
{
    return rpmcliInstallProblems(ts, _("Failed dependencies"), rpmtsCheck(ts));
}

int rpmcliInstallOrder(rpmts ts)
{
    return rpmcliInstallProblems(ts, _("Ordering problems"), rpmtsOrder(ts));
}

int rpmcliInstallRun(rpmts ts, rpmps okProbs, rpmprobFilterFlags ignoreSet)
{
    return rpmcliInstallProblems(ts, _("Install/Erase problems"),
			rpmtsRun(ts, okProbs, ignoreSet));
}


/** @todo Generalize --freshen policies. */
int rpmcliInstall(rpmts ts, QVA_t ia, const char ** argv)
{
    int numFailed = 0;
    int numRPMS = 0;
    rpmRelocation relocations = NULL;
    rpmVSFlags vsflags, ovsflags;
    int rc;
    int xx;

    if (argv == NULL) goto exit;

    (void) rpmtsSetGoal(ts, TSM_INSTALL);
    rpmcliPackagesTotal = 0;

    if (rpmExpandNumeric("%{?_repackage_all_erasures}"))
	ia->transFlags |= RPMTRANS_FLAG_REPACKAGE;

    /* Initialize security context patterns (if not already done). */
    if (!(ia->transFlags & RPMTRANS_FLAG_NOCONTEXTS)) {
	const char *fn = rpmGetPath("%{?_install_file_context_path}", NULL);
/*@-moduncon@*/
	if (fn != NULL && *fn != '\0')
	    xx = matchpathcon_init(fn);
/*@=moduncon@*/
	fn = _free(fn);
    }
    (void) rpmtsSetFlags(ts, ia->transFlags);
    (void) rpmtsSetDFlags(ts, ia->depFlags);

    /* Display and set autorollback goal. */
    if (rpmExpandNumeric("%{?_rollback_transaction_on_failure}")) {
	if (ia->arbtid) {
	    time_t ttid = (time_t)ia->arbtid;
	    rpmlog(RPMLOG_DEBUG, D_("Autorollback Goal: %-24.24s (0x%08x)\n"), 
		ctime(&ttid), ia->arbtid);
	    rpmtsSetARBGoal(ts, ia->arbtid);
	}	
    }

    if (ia->installInterfaceFlags & INSTALL_UPGRADE)
	vsflags = rpmExpandNumeric("%{?_vsflags_erase}");
    else
	vsflags = rpmExpandNumeric("%{?_vsflags_install}");
    if (ia->qva_flags & VERIFY_DIGEST)
	vsflags |= _RPMVSF_NODIGESTS;
    if (ia->qva_flags & VERIFY_SIGNATURE)
	vsflags |= _RPMVSF_NOSIGNATURES;
    if (ia->qva_flags & VERIFY_HDRCHK)
	vsflags |= RPMVSF_NOHDRCHK;
    ovsflags = rpmtsSetVSFlags(ts, (vsflags | RPMVSF_NEEDPAYLOAD));

    {	int notifyFlags;
	notifyFlags = ia->installInterfaceFlags | (rpmIsVerbose() ? INSTALL_LABEL : 0 );
	xx = rpmtsSetNotifyCallback(ts,
			rpmShowProgress, (void *) ((long)notifyFlags));
    }

    if ((relocations = ia->relocations) != NULL) {
	while (relocations->oldPath)
	    relocations++;
	if (relocations->newPath == NULL)
	    relocations = NULL;
    }

#if defined(REFERENCE_FORNOW)
if (fileURL[0] == '=') {
    rpmds this = rpmdsSingle(RPMTAG_REQUIRENAME, fileURL+1, NULL, 0);

    xx = rpmtsSolve(ts, this, NULL);
    if (ts->suggests && ts->nsuggests > 0) {
	fileURL = _free(fileURL);
	fileURL = ts->suggests[0];
	ts->suggests[0] = NULL;
	while (ts->nsuggests-- > 0) {
	    if (ts->suggests[ts->nsuggests] == NULL)
		continue;
	    ts->suggests[ts->nsuggests] = _free(ts->suggests[ts->nsuggests]);
	}
	ts->suggests = _free(ts->suggests);
	rpmlog(RPMLOG_DEBUG, D_("Adding goal: %s\n"), fileURL);
	pkgURL[pkgx] = fileURL;
	fileURL = NULL;
	pkgx++;
    }
    this = rpmdsFree(this);
} else
#endif

{	/* start-of-transaction-build */
    int tag = (ia->qva_source == RPMQV_FTSWALK)
	? RPMDBI_FTSWALK : RPMDBI_ARGLIST;
    rpmgi gi = rpmgiNew(ts, tag, NULL, 0);
    rpmgiFlags _giFlags = RPMGI_NONE;

/*@-mods@*/
    if (ftsOpts == 0)
	ftsOpts = (FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOSTAT);
/*@=mods@*/
    rc = rpmgiSetArgs(gi, argv, ftsOpts, _giFlags);
    while (rpmgiNext(gi) == RPMRC_OK) {
	Header h = rpmgiHeader(gi);
	const char * fn;

	if (h == NULL) {
	    numFailed++;
	    continue;
	}
	fn = rpmgiHdrPath(gi);

	/* === Check for relocatable package. */
	if (relocations) {
	    const char ** paths;
	    int pft;
	    int c;

	    if (headerGetEntry(h, RPMTAG_PREFIXES, &pft, &paths, &c)
	     && c == 1)
	    {
		relocations->oldPath = xstrdup(paths[0]);
		paths = headerFreeData(paths, pft);
	    } else {
		const char * NVRA = NULL;
		xx = headerGetExtension(h, RPMTAG_NVRA, NULL, &NVRA, NULL);
		rpmlog(RPMLOG_ERR,
			       _("package %s is not relocatable\n"), NVRA);
		NVRA = _free(NVRA);
		numFailed++;
		goto exit;
		/*@notreached@*/
	    }
	}

	/* === On --freshen, verify package is installed and newer. */
	if (ia->installInterfaceFlags & INSTALL_FRESHEN) {
	    rpmdbMatchIterator mi;
	    const char * name = NULL;
	    Header oldH;
	    int count;

	    xx = headerGetEntry(h, RPMTAG_NAME, NULL, &name, NULL);
	    mi = rpmtsInitIterator(ts, RPMTAG_NAME, name, 0);
	    count = rpmdbGetIteratorCount(mi);
	    while ((oldH = rpmdbNextIterator(mi)) != NULL) {
		if (rpmVersionCompare(oldH, h) < 0)
		    /*@innercontinue@*/ continue;
		/* same or newer package already installed */
		count = 0;
		/*@innerbreak@*/ break;
	    }
	    mi = rpmdbFreeIterator(mi);
	    if (count == 0)
		continue;
	    /* Package is newer than those currently installed. */
	}

	/* === Add binary package to transaction set. */
	rc = rpmtsAddInstallElement(ts, h, (fnpyKey)xstrdup(fn),
			(ia->installInterfaceFlags & INSTALL_UPGRADE) != 0,
			ia->relocations);

	if (relocations)
	    relocations->oldPath = _free(relocations->oldPath);

	numRPMS++;
    }

    gi = rpmgiFree(gi);

}	/* end-of-transaction-build */

    if (numFailed) goto exit;

    if (numRPMS) {
	if (!(ia->installInterfaceFlags & INSTALL_NODEPS)
	 && (rc = rpmcliInstallCheck(ts)) != 0) {
	    numFailed = numRPMS;
	    (void) rpmcliInstallSuggests(ts);
	}

	if (!(ia->installInterfaceFlags & INSTALL_NOORDER)
	 && (rc = rpmcliInstallOrder(ts)) != 0)
	    numFailed = numRPMS;

	/* Drop added/available package indices and dependency sets. */
	rpmtsClean(ts);

	if (numFailed == 0
	 && (rc = rpmcliInstallRun(ts, NULL, ia->probFilter)) != 0)
	    numFailed += (rc < 0 ? numRPMS : rc);
    }

    if (numFailed) goto exit;

exit:

#ifdef	NOTYET	/* XXX grrr, segfault in selabel_close */
    if (!(ia->transFlags & RPMTRANS_FLAG_NOCONTEXTS))
	matchpathcon_fini();
#endif

    rpmtsEmpty(ts);

    return numFailed;
}

int rpmErase(rpmts ts, QVA_t ia, const char ** argv)
{
    int count;
    const char ** arg;
    int numFailed = 0;
    int stopUninstall = 0;
    int numPackages = 0;
    rpmVSFlags vsflags, ovsflags;
    rpmps ps;

    if (argv == NULL) return 0;

    vsflags = rpmExpandNumeric("%{?_vsflags_erase}");
    if (ia->qva_flags & VERIFY_DIGEST)
	vsflags |= _RPMVSF_NODIGESTS;
    if (ia->qva_flags & VERIFY_SIGNATURE)
	vsflags |= _RPMVSF_NOSIGNATURES;
    if (ia->qva_flags & VERIFY_HDRCHK)
	vsflags |= RPMVSF_NOHDRCHK;
    ovsflags = rpmtsSetVSFlags(ts, vsflags);

    if (rpmExpandNumeric("%{?_repackage_all_erasures}"))
	ia->transFlags |= RPMTRANS_FLAG_REPACKAGE;

    (void) rpmtsSetFlags(ts, ia->transFlags);
    (void) rpmtsSetDFlags(ts, ia->depFlags);

    /* Display and set autorollback goal. */
    if (rpmExpandNumeric("%{?_rollback_transaction_on_failure}")) {
	if (ia->arbtid) {
	    time_t ttid = (time_t)ia->arbtid;
	    rpmlog(RPMLOG_DEBUG, D_("Autorollback Goal: %-24.24s (0x%08x)\n"), 
		ctime(&ttid), ia->arbtid);
	    rpmtsSetARBGoal(ts, ia->arbtid);
	}	
    }

#ifdef	NOTYET	/* XXX no callbacks on erase yet */
    {	int notifyFlags;
	notifyFlags = ia->installInterfaceFlags | (rpmIsVerbose() ? INSTALL_LABEL : 0 );
	xx = rpmtsSetNotifyCallback(ts,
			rpmShowProgress, (void *) ((long)notifyFlags));
    }
#endif

    (void) rpmtsSetGoal(ts, TSM_ERASE);

    for (arg = argv; *arg; arg++) {
	rpmdbMatchIterator mi;

	/* XXX HACK to get rpmdbFindByLabel out of the API */
	mi = rpmtsInitIterator(ts, RPMDBI_LABEL, *arg, 0);
	if (mi == NULL) {
	    rpmlog(RPMLOG_ERR, _("package %s is not installed\n"), *arg);
	    numFailed++;
	} else {
	    Header h;	/* XXX iterator owns the reference */
	    count = 0;
	    while ((h = rpmdbNextIterator(mi)) != NULL) {
		unsigned int recOffset = rpmdbGetIteratorOffset(mi);

		if (!(count++ == 0 || (ia->installInterfaceFlags & INSTALL_ALLMATCHES))) {
		    rpmlog(RPMLOG_ERR, _("\"%s\" specifies multiple packages\n"),
			*arg);
		    numFailed++;
		    /*@innerbreak@*/ break;
		}
		if (recOffset) {
		    (void) rpmtsAddEraseElement(ts, h, recOffset);
		    numPackages++;
		}
	    }
	}
	mi = rpmdbFreeIterator(mi);
    }

    if (numFailed) goto exit;

    if (!(ia->installInterfaceFlags & INSTALL_NODEPS)) {

	if (rpmtsCheck(ts)) {
	    numFailed = numPackages;
	    stopUninstall = 1;
	}

	ps = rpmtsProblems(ts);
	if (!stopUninstall && rpmpsNumProblems(ps) > 0) {
	    rpmlog(RPMLOG_ERR, _("Failed dependencies:\n"));
	    rpmpsPrint(NULL, ps);
	    numFailed += numPackages;
	    stopUninstall = 1;
	}
	ps = rpmpsFree(ps);
    }

    if (!stopUninstall && !(ia->installInterfaceFlags & INSTALL_NOORDER)) {
	if (rpmtsOrder(ts)) {
	    numFailed += numPackages;
	    stopUninstall = 1;
	}
    }

    if (numPackages > 0 && !stopUninstall) {

	/* Drop added/available package indices and dependency sets. */
	rpmtsClean(ts);

	numPackages = rpmtsRun(ts, NULL, 0);
	ps = rpmtsProblems(ts);
	if (rpmpsNumProblems(ps) > 0)
	    rpmpsPrint(NULL, ps);
	numFailed += numPackages;
	stopUninstall = 1;
	ps = rpmpsFree(ps);
    }

exit:
    rpmtsEmpty(ts);

    return numFailed;
}

int rpmInstallSource(rpmts ts, const char * arg,
		const char ** specFilePtr, const char ** cookie)
{
    FD_t fd;
    int rc;

    fd = Fopen(arg, "r.fdio");
    if (fd == NULL || Ferror(fd)) {
	rpmlog(RPMLOG_ERR, _("cannot open %s: %s\n"), arg, Fstrerror(fd));
	if (fd != NULL) (void) Fclose(fd);
	return 1;
    }

    if (rpmIsVerbose())
	fprintf(stdout, _("Installing %s\n"), arg);

    {
	rpmVSFlags ovsflags =
		rpmtsSetVSFlags(ts, (rpmtsVSFlags(ts) | RPMVSF_NEEDPAYLOAD));
	rpmRC rpmrc = rpmInstallSourcePackage(ts, fd, specFilePtr, cookie);
	rc = (rpmrc == RPMRC_OK ? 0 : 1);
	ovsflags = rpmtsSetVSFlags(ts, ovsflags);
    }
    if (rc != 0) {
	rpmlog(RPMLOG_ERR, _("%s cannot be installed\n"), arg);
	/*@-unqualifiedtrans@*/
	if (specFilePtr && *specFilePtr)
	    *specFilePtr = _free(*specFilePtr);
	if (cookie && *cookie)
	    *cookie = _free(*cookie);
	/*@=unqualifiedtrans@*/
    }

    (void) Fclose(fd);

    return rc;
}

/** \ingroup rpmcli
 * \file lib/verify.c
 * Verify installed payload files from package metadata.
 */

#include "system.h"

#include <rpmio.h>
#include <rpmiotypes.h>
#include <rpmcb.h>
#include "ugid.h"

#include <rpmtypes.h>
#include <rpmtag.h>

#include <rpmfi.h>

#define	_RPMSQ_INTERNAL
#include "psm.h"

#include "legacy.h"	/* XXX dodigest(), uidToUname(), gnameToGid */

#define	_RPMPS_INTERNAL	/* XXX rpmps needs iterator. */
#define	_RPMTS_INTERNAL	/* XXX expose rpmtsSetScriptFd */
#include <rpmcli.h>

#include "debug.h"

/*@access rpmts @*/	/* XXX cast */
/*@access rpmpsm @*/	/* XXX for %verifyscript through rpmpsmStage() */

#define S_ISDEV(m) (S_ISBLK((m)) || S_ISCHR((m)))

/*@unchecked@*/
extern int _rpmds_unspecified_epoch_noise;

/** \ingroup rpmcli
 * Verify file attributes (including file digest).
 * @todo gnorpm and python bindings prevent this from being static.
 * @param ts		transaction set
 * @param fi		file info (with linked header and current file index)
 * @retval *res		bit(s) returned to indicate failure
 * @param omitMask	bit(s) to disable verify checks
 * @return		0 on success (or not installed), 1 on error
 */
static int rpmVerifyFile(const rpmts ts, const rpmfi fi,
		/*@out@*/ rpmVerifyAttrs * res, rpmVerifyAttrs omitMask)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies fi, *res, fileSystem, internalState @*/
	/*@requires maxSet(res) >= 0 @*/
{
    unsigned short fmode = rpmfiFMode(fi);
    rpmfileAttrs fileAttrs = rpmfiFFlags(fi);
    rpmVerifyAttrs flags = rpmfiVFlags(fi);
    const char * fn = rpmfiFN(fi);
    const char * rootDir = rpmtsRootDir(ts);
    struct stat sb;
    int rc;

    /* Prepend the path to root (if specified). */
    if (rootDir && *rootDir != '\0'
     && !(rootDir[0] == '/' && rootDir[1] == '\0'))
    {
	int nb = strlen(fn) + strlen(rootDir) + 1;
	char * tb = alloca(nb);
	char * t;

	t = tb;
	*t = '\0';
	t = stpcpy(t, rootDir);
	while (t > tb && t[-1] == '/') {
	    --t;
	    *t = '\0';
	}
	t = stpcpy(t, fn);
	fn = tb;
    }

    *res = RPMVERIFY_NONE;

    /*
     * Check to see if the file was installed - if not pretend all is OK.
     */
    switch (rpmfiFState(fi)) {
    case RPMFILE_STATE_NETSHARED:
    case RPMFILE_STATE_REPLACED:
    case RPMFILE_STATE_NOTINSTALLED:
    case RPMFILE_STATE_WRONGCOLOR:
	return 0;
	/*@notreached@*/ break;
    case RPMFILE_STATE_NORMAL:
	break;
    }

    if (fn == NULL || Lstat(fn, &sb) != 0) {
	*res |= RPMVERIFY_LSTATFAIL;
	return 1;
    }

    /*
     * Not all attributes of non-regular files can be verified.
     */
    if (S_ISDIR(sb.st_mode))
	flags &= ~(RPMVERIFY_FDIGEST | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME |
			RPMVERIFY_LINKTO);
    else if (S_ISLNK(sb.st_mode)) {
	flags &= ~(RPMVERIFY_FDIGEST | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME |
		RPMVERIFY_MODE);
#if CHOWN_FOLLOWS_SYMLINK
	    flags &= ~(RPMVERIFY_USER | RPMVERIFY_GROUP);
#endif
    }
    else if (S_ISFIFO(sb.st_mode))
	flags &= ~(RPMVERIFY_FDIGEST | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME |
			RPMVERIFY_LINKTO);
    else if (S_ISCHR(sb.st_mode))
	flags &= ~(RPMVERIFY_FDIGEST | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME |
			RPMVERIFY_LINKTO);
    else if (S_ISBLK(sb.st_mode))
	flags &= ~(RPMVERIFY_FDIGEST | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME |
			RPMVERIFY_LINKTO);
    else
	flags &= ~(RPMVERIFY_LINKTO);

    /*
     * Content checks of %ghost files are meaningless.
     */
    if (fileAttrs & RPMFILE_GHOST)
	flags &= ~(RPMVERIFY_FDIGEST | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME |
			RPMVERIFY_LINKTO);

    /*
     * Don't verify any features in omitMask.
     */
    flags &= ~(omitMask | RPMVERIFY_FAILURES);


    if (flags & RPMVERIFY_FDIGEST) {
	int dalgo = 0;
	size_t dlen = 0;
	const unsigned char * digest = rpmfiDigest(fi, &dalgo, &dlen);

	if (digest == NULL)
	    *res |= RPMVERIFY_FDIGEST;
	else {
	/* XXX If --nofdigest, then prelinked library sizes fail to verify. */
	    unsigned char * fdigest = memset(alloca(dlen), 0, dlen);
	    size_t fsize;
	    rc = dodigest(dalgo, fn, fdigest, 0, &fsize);
	    sb.st_size = fsize;
	    if (rc)
		*res |= (RPMVERIFY_READFAIL|RPMVERIFY_FDIGEST);
	    else
	    if (memcmp(fdigest, digest, dlen))
		*res |= RPMVERIFY_FDIGEST;
	}
    }

    if (flags & RPMVERIFY_LINKTO) {
	char linkto[1024+1];
	int size = 0;

	if ((size = Readlink(fn, linkto, sizeof(linkto)-1)) == -1)
	    *res |= (RPMVERIFY_READLINKFAIL|RPMVERIFY_LINKTO);
	else {
	    const char * flink = rpmfiFLink(fi);
	    linkto[size] = '\0';
	    if (flink == NULL || strcmp(linkto, flink))
		*res |= RPMVERIFY_LINKTO;
	}
    }

    if (flags & RPMVERIFY_FILESIZE) {
	if (sb.st_size != rpmfiFSize(fi))
	    *res |= RPMVERIFY_FILESIZE;
    }

    if (flags & RPMVERIFY_MODE) {
	unsigned short metamode = fmode;
	unsigned short filemode;

	/*
	 * Platforms (like AIX) where sizeof(unsigned short) != sizeof(mode_t)
	 * need the (unsigned short) cast here.
	 */
	filemode = (unsigned short)sb.st_mode;

	/*
	 * Comparing the type of %ghost files is meaningless, but perms are OK.
	 */
	if (fileAttrs & RPMFILE_GHOST) {
	    metamode &= ~0xf000;
	    filemode &= ~0xf000;
	}

	if (metamode != filemode)
	    *res |= RPMVERIFY_MODE;
    }

    if (flags & RPMVERIFY_RDEV) {
	if (S_ISCHR(fmode) != S_ISCHR(sb.st_mode)
	 || S_ISBLK(fmode) != S_ISBLK(sb.st_mode))
	{
	    *res |= RPMVERIFY_RDEV;
	} else if (S_ISDEV(fmode) && S_ISDEV(sb.st_mode)) {
	    rpmuint16_t st_rdev = (rpmuint16_t)(sb.st_rdev & 0xffff);
	    rpmuint16_t frdev = (rpmuint16_t)(rpmfiFRdev(fi) & 0xffff);
	    if (st_rdev != frdev)
		*res |= RPMVERIFY_RDEV;
	}
    }

    if (flags & RPMVERIFY_MTIME) {
	if ((rpmuint32_t)sb.st_mtime != rpmfiFMtime(fi))
	    *res |= RPMVERIFY_MTIME;
    }

    if (flags & RPMVERIFY_USER) {
	const char * name = uidToUname(sb.st_uid);
	const char * fuser = rpmfiFUser(fi);
	if (name == NULL || fuser == NULL || strcmp(name, fuser))
	    *res |= RPMVERIFY_USER;
    }

    if (flags & RPMVERIFY_GROUP) {
	const char * name = gidToGname(sb.st_gid);
	const char * fgroup = rpmfiFGroup(fi);
	if (name == NULL || fgroup == NULL || strcmp(name, fgroup))
	    *res |= RPMVERIFY_GROUP;
    }

    return 0;
}

/**
 * Return exit code from running verify script from header.
 * @todo malloc/free/refcount handling is fishy here.
 * @param qva		parsed query/verify options
 * @param ts		transaction set
 * @param fi		file info set
 * @param scriptFd      file handle to use for stderr (or NULL)
 * @return              0 on success
 */
static int rpmVerifyScript(/*@unused@*/ QVA_t qva, rpmts ts,
		rpmfi fi, /*@null@*/ FD_t scriptFd)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, fi, scriptFd, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    rpmpsm psm = rpmpsmNew(ts, NULL, fi);
    int rc = 0;

    if (psm == NULL)	/* XXX can't happen */
	return rc;

    if (scriptFd != NULL)
	rpmtsSetScriptFd(psm->ts, scriptFd);

    psm->stepName = "verify";
    psm->scriptTag = RPMTAG_VERIFYSCRIPT;
    psm->progTag = RPMTAG_VERIFYSCRIPTPROG;
    rc = rpmpsmStage(psm, PSM_SCRIPT);

    psm->stepName = "sanitycheck";
    psm->scriptTag = RPMTAG_SANITYCHECK;
    psm->progTag = RPMTAG_SANITYCHECKPROG;
    rc = rpmpsmStage(psm, PSM_SCRIPT);

    if (scriptFd != NULL)
	rpmtsSetScriptFd(psm->ts, NULL);

    psm = rpmpsmFree(psm, "rpmVerifyScript");

    return rc;
}

/**
 * Check file info from header against what's actually installed.
 * @param qva		parsed query/verify options
 * @param ts		transaction set
 * @param fi		file info set
 * @return		0 no problems, 1 problems found
 */
static int verifyHeader(QVA_t qva, const rpmts ts, rpmfi fi)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies fi, fileSystem, internalState  @*/
{
    rpmVerifyAttrs verifyResult = 0;
    /*@-type@*/ /* FIX: union? */
    rpmVerifyAttrs omitMask = ((qva->qva_flags & VERIFY_ATTRS) ^ VERIFY_ATTRS);
    /*@=type@*/
    int ec = 0;		/* assume no problems */
    char * t, * te;
    char buf[BUFSIZ];
    int i;

    te = t = buf;
    *te = '\0';

    fi = rpmfiLink(fi, "verifyHeader");
    fi = rpmfiInit(fi, 0);
    if (fi != NULL)	/* XXX lclint */
    while ((i = rpmfiNext(fi)) >= 0) {
	rpmfileAttrs fflags;
	int rc;

	fflags = rpmfiFFlags(fi);

	/* If not querying %config, skip config files. */
	if ((qva->qva_fflags & RPMFILE_CONFIG) && (fflags & RPMFILE_CONFIG))
	    continue;

	/* If not querying %doc, skip doc files. */
	if ((qva->qva_fflags & RPMFILE_DOC) && (fflags & RPMFILE_DOC))
	    continue;

	/* If not verifying %ghost, skip ghost files. */
	/* XXX the broken!!! logic disables %ghost queries always. */
	if (!(qva->qva_fflags & RPMFILE_GHOST) && (fflags & RPMFILE_GHOST))
	    continue;

	rc = rpmVerifyFile(ts, fi, &verifyResult, omitMask);
	if (rc) {
	    if (qva->qva_mode != 'v')	/* XXX no output w verify(...) probe. */
	    if (!(fflags & (RPMFILE_MISSINGOK|RPMFILE_GHOST)) || rpmIsVerbose()) {
		sprintf(te, _("missing   %c %s"),
			((fflags & RPMFILE_CONFIG)	? 'c' :
			 (fflags & RPMFILE_DOC)		? 'd' :
			 (fflags & RPMFILE_GHOST)	? 'g' :
			 (fflags & RPMFILE_LICENSE)	? 'l' :
			 (fflags & RPMFILE_PUBKEY)	? 'P' :
			 (fflags & RPMFILE_README)	? 'r' : ' '),
			rpmfiFN(fi));
		te += strlen(te);
                if ((verifyResult & RPMVERIFY_LSTATFAIL) != 0 && errno != ENOENT) {
                    sprintf(te, " (%s)", strerror(errno));
                    te += strlen(te);
                }
		ec = rc;
	    }
	} else if (verifyResult || rpmIsVerbose()) {
	    const char * size, * digest, * link, * mtime, * mode;
	    const char * group, * user, * rdev;
	    /*@observer@*/ static const char *const aok = ".";
	    /*@observer@*/ static const char *const unknown = "?";

	    if (!ec)
		ec = (verifyResult != 0);

	    if (qva->qva_mode != 'v') {	/* XXX no output w verify(...) probe. */
#define	_verify(_RPMVERIFY_F, _C)	\
	((verifyResult & _RPMVERIFY_F) ? _C : aok)
#define	_verifylink(_RPMVERIFY_F, _C)	\
	((verifyResult & RPMVERIFY_READLINKFAIL) ? unknown : \
	 (verifyResult & _RPMVERIFY_F) ? _C : aok)
#define	_verifyfile(_RPMVERIFY_F, _C)	\
	((verifyResult & RPMVERIFY_READFAIL) ? unknown : \
	 (verifyResult & _RPMVERIFY_F) ? _C : aok)
	
		digest = _verifyfile(RPMVERIFY_FDIGEST, "5");
		size = _verify(RPMVERIFY_FILESIZE, "S");
		link = _verifylink(RPMVERIFY_LINKTO, "L");
		mtime = _verify(RPMVERIFY_MTIME, "T");
		rdev = _verify(RPMVERIFY_RDEV, "D");
		user = _verify(RPMVERIFY_USER, "U");
		group = _verify(RPMVERIFY_GROUP, "G");
		mode = _verify(RPMVERIFY_MODE, "M");

#undef _verifyfile
#undef _verifylink
#undef _verify

		sprintf(te, "%s%s%s%s%s%s%s%s  %c %s",
		    size, mode, digest, rdev, link, user, group, mtime,
			((fflags & RPMFILE_CONFIG)	? 'c' :
			 (fflags & RPMFILE_DOC)	? 'd' :
			 (fflags & RPMFILE_GHOST)	? 'g' :
			 (fflags & RPMFILE_LICENSE)	? 'l' :
			 (fflags & RPMFILE_PUBKEY)	? 'P' :
			 (fflags & RPMFILE_README)	? 'r' : ' '),
			rpmfiFN(fi));
		te += strlen(te);
	    }
	}

	if (qva->qva_mode != 'v')	/* XXX no output w verify(...) probe. */
	if (te > t) {
	    *te++ = '\n';
	    *te = '\0';
	    rpmlog(RPMLOG_NOTICE, "%s", t);
	    te = t = buf;
	    *t = '\0';
	}
    }
    fi = rpmfiUnlink(fi, "verifyHeader");
	
    return ec;
}

/**
 * Check installed package dependencies for problems.
 * @param qva		parsed query/verify options
 * @param ts		transaction set
 * @param h		header
 * @return		number of problems found (0 for no problems)
 */
static int verifyDependencies(/*@unused@*/ QVA_t qva, rpmts ts,
		Header h)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, h, rpmGlobalMacroContext, fileSystem, internalState @*/
{
#ifdef	NOTYET
    int instance = headerGetInstance(h);
#endif
    rpmps ps;
    int rc = 0;		/* assume no problems */
    int xx;

    rpmtsEmpty(ts);
#ifdef	NOTYET
    if (instance > 0)
	(void) rpmtsAddEraseElement(ts, h, instance);
    else
#endif
	(void) rpmtsAddInstallElement(ts, h, NULL, 0, NULL);

    xx = rpmtsCheck(ts);
    ps = rpmtsProblems(ts);

    if (rpmpsNumProblems(ps) > 0) {
	const char * altNEVR;
	const char * pkgNEVR = NULL;
	rpmpsi psi;
	rpmProblem prob;
	char * t, * te;
	int nb = 512;

	psi = rpmpsInitIterator(ps);
	while (rpmpsNextIterator(psi) >= 0) {
	    prob = rpmpsProblem(psi);
	    if (pkgNEVR == NULL)
		pkgNEVR = rpmProblemGetPkgNEVR(prob);

	    altNEVR = rpmProblemGetAltNEVR(prob);
assert(altNEVR != NULL);
	    if (altNEVR[0] == 'R' && altNEVR[1] == ' ')
		nb += sizeof("\tRequires: ")-1;
	    if (altNEVR[0] == 'C' && altNEVR[1] == ' ')
		nb += sizeof("\tConflicts: ")-1;
	    nb += strlen(altNEVR+2) + sizeof("\n") - 1;

	}
	psi = rpmpsFreeIterator(psi);

	te = t = alloca(nb);
	*te = '\0';
	sprintf(te, _("Unsatisfied dependencies for %s:\n"), pkgNEVR);
	te += strlen(te);

	psi = rpmpsInitIterator(ps);
	while (rpmpsNextIterator(psi) >= 0) {
	    prob = rpmpsProblem(psi);

	    if ((altNEVR = rpmProblemGetAltNEVR(prob)) == NULL)
		altNEVR = "? ?altNEVR?";
	    if (altNEVR[0] == 'R' && altNEVR[1] == ' ')
		te = stpcpy(te, "\tRequires: ");
	    if (altNEVR[0] == 'C' && altNEVR[1] == ' ')
		te = stpcpy(te, "\tConflicts: ");
	    te = stpcpy( stpcpy(te, altNEVR+2), "\n");

	    rc++;
	}
	psi = rpmpsFreeIterator(psi);

	if (te > t) {
	    *te++ = '\n';
	    *te = '\0';
	    rpmlog(RPMLOG_NOTICE, "%s", t);
	    te = t;
	    *t = '\0';
	}
    }

    ps = rpmpsFree(ps);

    rpmtsEmpty(ts);

    return rc;
}

int showVerifyPackage(QVA_t qva, rpmts ts, Header h)
{
    int scareMem = 0;
    rpmfi fi = NULL;
    int ec = 0;
    int rc;

    fi = rpmfiNew(ts, h, RPMTAG_BASENAMES, scareMem);
    if (fi != NULL) {
	if (qva->qva_flags & VERIFY_DEPS) {
	    int save_noise = _rpmds_unspecified_epoch_noise;
/*@-mods@*/
	    if (rpmIsVerbose())
		_rpmds_unspecified_epoch_noise = 1;
	    if ((rc = verifyDependencies(qva, ts, h)) != 0)
		ec = rc;
	    _rpmds_unspecified_epoch_noise = save_noise;
/*@=mods@*/
	}
	if (qva->qva_flags & VERIFY_FILES) {
	    if ((rc = verifyHeader(qva, ts, fi)) != 0)
		ec = rc;
	}
	if ((qva->qva_flags & VERIFY_SCRIPT)
	 && (headerIsEntry(h, RPMTAG_VERIFYSCRIPT) ||
	     headerIsEntry(h, RPMTAG_SANITYCHECK)))
	{
	    FD_t fdo = fdDup(STDOUT_FILENO);

	    rc = rpmfiSetHeader(fi, h);
	    if ((rc = rpmVerifyScript(qva, ts, fi, fdo)) != 0)
		ec = rc;
	    if (fdo != NULL)
		rc = Fclose(fdo);
	    rc = rpmfiSetHeader(fi, NULL);
	}

	fi = rpmfiFree(fi);
    }

    return ec;
}

int rpmcliVerify(rpmts ts, QVA_t qva, const char ** argv)
{
    rpmdepFlags depFlags = qva->depFlags, odepFlags;
    rpmtransFlags transFlags = qva->transFlags, otransFlags;
    rpmVSFlags vsflags, ovsflags;
    int ec = 0;

    if (qva->qva_showPackage == NULL)
        qva->qva_showPackage = showVerifyPackage;

    /* XXX verify flags are inverted from query. */
    vsflags = rpmExpandNumeric("%{?_vsflags_verify}");
    if (!(qva->qva_flags & VERIFY_DIGEST))
	vsflags |= _RPMVSF_NODIGESTS;
    if (!(qva->qva_flags & VERIFY_SIGNATURE))
	vsflags |= _RPMVSF_NOSIGNATURES;
    if (!(qva->qva_flags & VERIFY_HDRCHK))
	vsflags |= RPMVSF_NOHDRCHK;
    vsflags &= ~RPMVSF_NEEDPAYLOAD;

    odepFlags = rpmtsSetDFlags(ts, depFlags);
    otransFlags = rpmtsSetFlags(ts, transFlags);
    ovsflags = rpmtsSetVSFlags(ts, vsflags);
    ec = rpmcliArgIter(ts, qva, argv);
    vsflags = rpmtsSetVSFlags(ts, ovsflags);
    transFlags = rpmtsSetFlags(ts, otransFlags);
    depFlags = rpmtsSetDFlags(ts, odepFlags);

    if (qva->qva_showPackage == showVerifyPackage)
        qva->qva_showPackage = NULL;

    rpmtsEmpty(ts);

    return ec;
}

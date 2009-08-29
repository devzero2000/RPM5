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

#define	_RPMFI_INTERNAL
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

typedef struct rpmvf_s * rpmvf;
struct rpmvf_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
    const char * fn;
    const char * flink;
    struct stat  sb;
    rpmfileAttrs fflags;
    rpmfileState fstate;
    rpmVerifyAttrs vflags;
    int dalgo;
    size_t dlen;
    const unsigned char * digest;
    const char * fuser;
    const char * fgroup;
#if defined(__LCLINT__)
/*@refs@*/
    int nrefs;			/*!< (unused) keep splint happy */
#endif
};

static rpmvf rpmvfFree(rpmvf vf)
{
    if (vf) {
#ifdef	NOTYET
	yarnPossess(vf->_item.use);
	if (yarnPeekLock(vf->_item.use) <= 1L) {
	    yarnLock use = vf->_item.use;
	    vf->fn = _free(vf->fn);
	    vf = _free(vf);
	    yarnTwist(use, TO, 0);
	    use = yarnFreeLock(use);
	} else
	    yarnTwist(vf->_item.use, BY, -1);
#else
	vf->fn = _free(vf->fn);
	vf = _free(vf);
#endif
    }
    return NULL;
}

static rpmvf rpmvfNew(rpmts ts, rpmfi fi, int i, rpmVerifyAttrs omitMask)
{
    rpmvf vf = xcalloc(1, sizeof(*vf));

#ifdef	NOTYET
    vf->_item.use = yarnNewLock(1);
    vf->_item.pool = NULL;
#endif

    vf->fn = rpmGetPath(rpmtsRootDir(ts), fi->dnl[fi->dil[i]], fi->bnl[i], NULL);
    vf->flink = fi->flinks[i];
    vf->fuser = fi->fuser[i];
    vf->fgroup = fi->fgroup[i];

    {   struct stat *st = &vf->sb;
        st->st_dev =
        st->st_rdev = fi->frdevs[i];
        st->st_ino = fi->finodes[i];
        st->st_mode = fi->fmodes[i];
#ifdef	NOTNEEDED
        st->st_nlink = rpmfiFNlink(fi) + (int)S_ISDIR(st->st_mode);
#endif
        if (unameToUid(vf->fuser, &st->st_uid) == -1)
            st->st_uid = 0;             /* XXX */
        if (gnameToGid(vf->fgroup, &st->st_gid) == -1)
            st->st_gid = 0;             /* XXX */
        st->st_size = fi->fsizes[i];
        st->st_blksize = 4 * 1024;      /* XXX */
        st->st_blocks = (st->st_size + (st->st_blksize - 1)) / st->st_blksize;
        st->st_atime =
        st->st_ctime =
        st->st_mtime = fi->fmtimes[i];
    }

    vf->fflags = fi->fflags[i];
    vf->fstate = fi->fstates[i];
    vf->vflags = fi->vflags[i];
    vf->dalgo = fi->fdigestalgos
		? fi->fdigestalgos[i]
		: fi->digestalgo;
    vf->dlen = fi->digestlen;
    vf->digest = fi->digests + (fi->digestlen * i);

    /* Don't verify any features in omitMask. */
    vf->vflags &= ~(omitMask | RPMVERIFY_FAILURES);

    /* Content checks of %ghost files are meaningless. */
    if (vf->fflags & RPMFILE_GHOST)
	vf->vflags &= ~(RPMVERIFY_FDIGEST | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME |
			RPMVERIFY_LINKTO | RPMVERIFY_HMAC);

    return vf;
}

/** \ingroup rpmcli
 * Verify file attributes (including file digest).
 * @param vf		file data to verify
 * #param spew		should verify results be printed?
 * @return		0 on success (or not installed), 1 on error
 */
static int rpmvfVerify(rpmvf vf, int spew)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies vf, te, fileSystem, internalState @*/
	/*@requires maxSet(res) >= 0 @*/
{
    rpmVerifyAttrs res = RPMVERIFY_NONE;
    struct stat sb;
    int ec = 0;

    /* Check to see if the file was installed - if not pretend all is OK. */
    switch (vf->fstate) {
    default:
    case RPMFILE_STATE_NETSHARED:
    case RPMFILE_STATE_REPLACED:
    case RPMFILE_STATE_NOTINSTALLED:
    case RPMFILE_STATE_WRONGCOLOR:
	goto exit;
	/*@notreached@*/ break;
    case RPMFILE_STATE_NORMAL:
	break;
    }

assert(vf->fn != NULL);
    if (vf->fn == NULL || Lstat(vf->fn, &sb) != 0) {
	res |= RPMVERIFY_LSTATFAIL;
	ec = 1;
	goto exit;
    }

    /* Not all attributes of non-regular files can be verified. */
    if (S_ISDIR(sb.st_mode))
	vf->vflags &= ~(RPMVERIFY_FDIGEST | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME |
			RPMVERIFY_LINKTO | RPMVERIFY_HMAC);
    else if (S_ISLNK(sb.st_mode)) {
	vf->vflags &= ~(RPMVERIFY_FDIGEST | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME |
		RPMVERIFY_MODE | RPMVERIFY_HMAC);
#if CHOWN_FOLLOWS_SYMLINK
	vf->vflags &= ~(RPMVERIFY_USER | RPMVERIFY_GROUP);
#endif
    }
    else if (S_ISFIFO(sb.st_mode))
	vf->vflags &= ~(RPMVERIFY_FDIGEST | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME |
			RPMVERIFY_LINKTO | RPMVERIFY_HMAC);
    else if (S_ISCHR(sb.st_mode))
	vf->vflags &= ~(RPMVERIFY_FDIGEST | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME |
			RPMVERIFY_LINKTO | RPMVERIFY_HMAC);
    else if (S_ISBLK(sb.st_mode))
	vf->vflags &= ~(RPMVERIFY_FDIGEST | RPMVERIFY_FILESIZE | RPMVERIFY_MTIME |
			RPMVERIFY_LINKTO | RPMVERIFY_HMAC);
    else
	vf->vflags &= ~(RPMVERIFY_LINKTO);

    if (vf->vflags & (RPMVERIFY_FDIGEST | RPMVERIFY_HMAC)) {
	if (vf->digest == NULL || vf->dlen == 0)
	    res |= RPMVERIFY_FDIGEST;
	else {
	/* XXX If --nofdigest, then prelinked library sizes fail to verify. */
	    unsigned char * fdigest = memset(alloca(vf->dlen), 0, vf->dlen);
	    size_t fsize = 0;
#define	_mask	(RPMVERIFY_FDIGEST|RPMVERIFY_HMAC)
	    unsigned dflags = (vf->vflags & _mask) == RPMVERIFY_HMAC
		? 0x2 : 0x0;
#undef	_mask
	    int rc = dodigest(vf->dalgo, vf->fn, fdigest, dflags, &fsize);
	    sb.st_size = fsize;
	    if (rc)
		res |= (RPMVERIFY_READFAIL|RPMVERIFY_FDIGEST);
	    else
	    if (memcmp(fdigest, vf->digest, vf->dlen))
		res |= RPMVERIFY_FDIGEST;
	}
    }

    if (vf->vflags & RPMVERIFY_LINKTO) {
	char linkto[1024+1];
	int size = 0;

	if ((size = Readlink(vf->fn, linkto, sizeof(linkto)-1)) == -1)
	    res |= (RPMVERIFY_READLINKFAIL|RPMVERIFY_LINKTO);
	else {
	    linkto[size] = '\0';
	    if (vf->flink == NULL || strcmp(linkto, vf->flink))
		res |= RPMVERIFY_LINKTO;
	}
    }

    if (vf->vflags & RPMVERIFY_FILESIZE) {
	if (sb.st_size != vf->sb.st_size)
	    res |= RPMVERIFY_FILESIZE;
    }

    if (vf->vflags & RPMVERIFY_MODE) {
	/* XXX AIX has sizeof(mode_t) > sizeof(unsigned short) */
	unsigned short metamode = (unsigned short)vf->sb.st_mode;
	unsigned short filemode = (unsigned short)sb.st_mode;

	/* Comparing type of %ghost files is meaningless, but perms are OK. */
	if (vf->fflags & RPMFILE_GHOST) {
	    metamode &= ~0xf000;
	    filemode &= ~0xf000;
	}
	if (metamode != filemode)
	    res |= RPMVERIFY_MODE;
    }

    if (vf->vflags & RPMVERIFY_RDEV) {
	if (S_ISCHR(vf->sb.st_mode) != S_ISCHR(sb.st_mode)
	 || S_ISBLK(vf->sb.st_mode) != S_ISBLK(sb.st_mode))
	    res |= RPMVERIFY_RDEV;
	else if (S_ISDEV(vf->sb.st_mode) && S_ISDEV(sb.st_mode)) {
	    rpmuint16_t st_rdev = (rpmuint16_t)(sb.st_rdev & 0xffff);
	    rpmuint16_t frdev = (rpmuint16_t)(vf->sb.st_rdev & 0xffff);
	    if (st_rdev != frdev)
		res |= RPMVERIFY_RDEV;
	}
    }

    if (vf->vflags & RPMVERIFY_MTIME) {
	if (sb.st_mtime != vf->sb.st_mtime)
	    res |= RPMVERIFY_MTIME;
    }

    if (vf->vflags & RPMVERIFY_USER) {
	const char * fuser = uidToUname(sb.st_uid);
	if (fuser == NULL || vf->fuser == NULL || strcmp(fuser, vf->fuser))
	    res |= RPMVERIFY_USER;
    }

    if (vf->vflags & RPMVERIFY_GROUP) {
	const char * fgroup = gidToGname(sb.st_gid);
	if (fgroup == NULL || vf->fgroup == NULL || strcmp(fgroup, vf->fgroup))
	    res |= RPMVERIFY_GROUP;
    }

exit:

    if (spew) {	/* XXX no output w verify(...) probe. */
	char buf[BUFSIZ];
	char * t = buf;
	char * te = t;
	*te = '\0';
	if (ec) {
	    if (!(vf->fflags & (RPMFILE_MISSINGOK|RPMFILE_GHOST))
	     || rpmIsVerbose())
	    {
		sprintf(te, _("missing   %c %s"),
			((vf->fflags & RPMFILE_CONFIG)	? 'c' :
			 (vf->fflags & RPMFILE_DOC)	? 'd' :
			 (vf->fflags & RPMFILE_GHOST)	? 'g' :
			 (vf->fflags & RPMFILE_LICENSE)	? 'l' :
			 (vf->fflags & RPMFILE_PUBKEY)	? 'P' :
			 (vf->fflags & RPMFILE_README)	? 'r' : ' '),
			vf->fn);
                if ((res & RPMVERIFY_LSTATFAIL) != 0 && errno != ENOENT) {
		    te += strlen(te);
                    sprintf(te, " (%s)", strerror(errno));
                }
	    }
	} else if (res || rpmIsVerbose()) {
	    /*@observer@*/ static const char aok[] = ".";
	    /*@observer@*/ static const char unknown[] = "?";

#define	_verify(_RPMVERIFY_F, _C)	\
	((res & _RPMVERIFY_F) ? _C : aok)
#define	_verifylink(_RPMVERIFY_F, _C)	\
	((res & RPMVERIFY_READLINKFAIL) ? unknown : \
	 (res & _RPMVERIFY_F) ? _C : aok)
#define	_verifyfile(_RPMVERIFY_F, _C)	\
	((res & RPMVERIFY_READFAIL) ? unknown : \
	 (res & _RPMVERIFY_F) ? _C : aok)
	
	    const char * digest = _verifyfile(RPMVERIFY_FDIGEST, "5");
	    const char * size = _verify(RPMVERIFY_FILESIZE, "S");
	    const char * link = _verifylink(RPMVERIFY_LINKTO, "L");
	    const char * mtime = _verify(RPMVERIFY_MTIME, "T");
	    const char * rdev = _verify(RPMVERIFY_RDEV, "D");
	    const char * user = _verify(RPMVERIFY_USER, "U");
	    const char * group = _verify(RPMVERIFY_GROUP, "G");
	    const char * mode = _verify(RPMVERIFY_MODE, "M");

#undef _verifyfile
#undef _verifylink
#undef _verify

	    sprintf(te, "%s%s%s%s%s%s%s%s  %c %s",
		    size, mode, digest, rdev, link, user, group, mtime,
			((vf->fflags & RPMFILE_CONFIG)	? 'c' :
			 (vf->fflags & RPMFILE_DOC)	? 'd' :
			 (vf->fflags & RPMFILE_GHOST)	? 'g' :
			 (vf->fflags & RPMFILE_LICENSE)	? 'l' :
			 (vf->fflags & RPMFILE_PUBKEY)	? 'P' :
			 (vf->fflags & RPMFILE_README)	? 'r' : ' '),
			vf->fn);

	}

	if (t && *t)
	    rpmlog(RPMLOG_NOTICE, "%s\n", t);

    }

    return (res != 0);
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
    rpmVerifyAttrs omitMask = ((qva->qva_flags & VERIFY_ATTRS) ^ VERIFY_ATTRS);
    int spew = (qva->qva_mode != 'v');	/* XXX no output w verify(...) probe. */
    static int scareMem = 0;
    rpmfi fi = rpmfiNew(ts, h, RPMTAG_BASENAMES, scareMem);
    rpmvf vf;
    int ec = 0;
    int rc;
    int i;

  if (fi != NULL)
#if defined(_OPENMP)
  #pragma omp parallel
#endif
  {
    if (qva->qva_flags & VERIFY_FILES)
#if defined(_OPENMP)
    #pragma omp for reduction(+:ec) private(vf,rc,i) nowait
#endif
    for (i = 0; i < rpmfiFC(fi); i++) {
	int fflags = fi->fflags[i];

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

	/* Gather per-file data into a carrier. */
	vf = rpmvfNew(ts, fi, i, omitMask);

	/* Verify per-file metadata. */
	rc = rpmvfVerify(vf, spew);
	if (rc)
	    ec += rc;

	(void) rpmvfFree(vf);
    }
    if (qva->qva_flags & VERIFY_SCRIPT)
#if defined(_OPENMP)
    #pragma omp master
#endif
    {
	if (headerIsEntry(h, RPMTAG_VERIFYSCRIPT) ||
	    headerIsEntry(h, RPMTAG_SANITYCHECK))
	{
	    FD_t fdo = fdDup(STDOUT_FILENO);

	    rc = rpmfiSetHeader(fi, h);
	    if ((rc = rpmVerifyScript(qva, ts, fi, fdo)) != 0)
		ec += rc;
	    if (fdo != NULL)
		rc = Fclose(fdo);
	    rc = rpmfiSetHeader(fi, NULL);
	}
    }
    if (qva->qva_flags & VERIFY_DEPS)
#if defined(_OPENMP)
    #pragma omp master
#endif
    {
	int save_noise = _rpmds_unspecified_epoch_noise;
/*@-mods@*/
	if (rpmIsVerbose())
	    _rpmds_unspecified_epoch_noise = 1;
	if ((rc = verifyDependencies(qva, ts, h)) != 0)
	    ec += rc;
	_rpmds_unspecified_epoch_noise = save_noise;
/*@=mods@*/
    }
  }

    fi = rpmfiFree(fi);

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

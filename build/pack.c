/** \ingroup rpmbuild
 * \file build/pack.c
 *  Assemble components of an RPM package.
 */

#include "system.h"

#include <rpmio_internal.h>
#define	_RPMEVR_INTERNAL	/* XXX RPMSENSE_ANY */
#include <rpmbuild.h>
#include "signature.h"		/* XXX rpmTempFile */

#include "rpmps.h"

#include "cpio.h"
#include "fsm.h"
#include "psm.h"

#define	_RPMFI_INTERNAL		/* XXX fi->fsm */
#include "rpmfi.h"
#include "rpmts.h"

#include "buildio.h"

#include "signature.h"
#include <pkgio.h>
#include "debug.h"

/*@access rpmts @*/
/*@access rpmfi @*/	/* compared with NULL */
/*@access Header @*/	/* compared with NULL */
/*@access FD_t @*/	/* compared with NULL */
/*@access StringBuf @*/	/* compared with NULL */
/*@access CSA_t @*/

extern int _nolead;	/* disable writing lead. */
extern int _nosigh;	/* disable writing signature header. */

/**
 */
static inline int genSourceRpmName(Spec spec)
	/*@modifies spec->sourceRpmName @*/
{
    if (spec->sourceRpmName == NULL) {
	const char *name, *version, *release;
	char fileName[BUFSIZ];

	(void) headerNEVRA(spec->packages->header, &name, NULL, &version, &release, NULL);
	sprintf(fileName, "%s-%s-%s.%ssrc.rpm", name, version, release,
	    spec->noSource ? "no" : "");
	spec->sourceRpmName = xstrdup(fileName);
    }

    return 0;
}

/**
 * @todo Create transaction set *much* earlier.
 */
static int cpio_doio(FD_t fdo, /*@unused@*/ Header h, CSA_t csa,
		const char * payload_format, const char * fmodeMacro)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies fdo, csa, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    rpmts ts = rpmtsCreate();
    rpmfi fi = csa->cpioList;
    const char *failedFile = NULL;
    FD_t cfd;
    int rc, ec;

/*@-boundsread@*/
    {	const char *fmode = rpmExpand(fmodeMacro, NULL);
	if (!(fmode && fmode[0] == 'w'))
	    fmode = xstrdup("w9.gzdio");
	/*@-nullpass@*/
	(void) Fflush(fdo);
	cfd = Fdopen(fdDup(Fileno(fdo)), fmode);
	/*@=nullpass@*/
	fmode = _free(fmode);
    }
/*@=boundsread@*/
    if (cfd == NULL)
	return 1;

    rc = fsmSetup(fi->fsm, FSM_PKGBUILD, payload_format, ts, fi, cfd,
		&csa->cpioArchiveSize, &failedFile);
    (void) Fclose(cfd);
    ec = fsmTeardown(fi->fsm);
    if (!rc) rc = ec;

    if (rc) {
	if (failedFile)
	    rpmlog(RPMLOG_ERR, _("create archive failed on file %s: %s\n"),
		failedFile, cpioStrerror(rc));
	else
	    rpmlog(RPMLOG_ERR, _("create archive failed: %s\n"),
		cpioStrerror(rc));
      rc = 1;
    }

    failedFile = _free(failedFile);
    ts = rpmtsFree(ts);

    return rc;
}

/**
 */
static int cpio_copy(FD_t fdo, CSA_t csa)
	/*@globals fileSystem, internalState @*/
	/*@modifies fdo, csa, fileSystem, internalState @*/
{
    char buf[BUFSIZ];
    size_t nb;

    while((nb = Fread(buf, sizeof(buf[0]), sizeof(buf), csa->cpioFdIn)) > 0) {
	if (Fwrite(buf, sizeof(buf[0]), nb, fdo) != nb) {
	    rpmlog(RPMLOG_ERR, _("cpio_copy write failed: %s\n"),
			Fstrerror(fdo));
	    return 1;
	}
	csa->cpioArchiveSize += nb;
    }
    if (Ferror(csa->cpioFdIn)) {
	rpmlog(RPMLOG_ERR, _("cpio_copy read failed: %s\n"),
		Fstrerror(csa->cpioFdIn));
	return 1;
    }
    return 0;
}

/**
 */
static /*@only@*/ /*@null@*/ StringBuf addFileToTagAux(Spec spec,
		const char * file, /*@only@*/ StringBuf sb)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext, fileSystem, internalState @*/
{
    char buf[BUFSIZ];
    const char * fn = buf;
    FILE * f;
    FD_t fd;

    fn = rpmGetPath("%{_builddir}/%{?buildsubdir:%{buildsubdir}/}", file, NULL);

    fd = Fopen(fn, "r.fdio");
    if (fn != buf) fn = _free(fn);
    if (fd == NULL || Ferror(fd)) {
	sb = freeStringBuf(sb);
	return NULL;
    }
    /*@-type@*/ /* FIX: cast? */
    if ((f = fdGetFp(fd)) != NULL)
    /*@=type@*/
    while (fgets(buf, sizeof(buf), f)) {
	/* XXX display fn in error msg */
	if (expandMacros(spec, spec->macros, buf, sizeof(buf))) {
	    rpmlog(RPMLOG_ERR, _("line: %s\n"), buf);
	    sb = freeStringBuf(sb);
	    break;
	}
	appendStringBuf(sb, buf);
    }
    (void) Fclose(fd);

    return sb;
}

/**
 */
static int addFileToTag(Spec spec, const char * file, Header h, int tag)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies h, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    StringBuf sb = newStringBuf();
    char *s;

    if (hge(h, tag, NULL, &s, NULL)) {
	appendLineStringBuf(sb, s);
	(void) headerRemoveEntry(h, tag);
    }

    if ((sb = addFileToTagAux(spec, file, sb)) == NULL)
	return 1;
    
    (void) headerAddEntry(h, tag, RPM_STRING_TYPE, getStringBuf(sb), 1);

    sb = freeStringBuf(sb);
    return 0;
}

/**
 */
static int addFileToArrayTag(Spec spec, const char *file, Header h, int tag)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies h, rpmGlobalMacroContext, fileSystem, internalState  @*/
{
    StringBuf sb = newStringBuf();
    char *s;

    if ((sb = addFileToTagAux(spec, file, sb)) == NULL)
	return 1;

    s = getStringBuf(sb);
    (void) headerAddOrAppendEntry(h, tag, RPM_STRING_ARRAY_TYPE, &s, 1);

    sb = freeStringBuf(sb);
    return 0;
}

int processScriptFiles(Spec spec, Package pkg)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies pkg->header, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    struct TriggerFileEntry *p;
    
    if (pkg->preInFile) {
	if (addFileToTag(spec, pkg->preInFile, pkg->header, RPMTAG_PREIN)) {
	    rpmlog(RPMLOG_ERR,
		     _("Could not open PreIn file: %s\n"), pkg->preInFile);
	    return RPMRC_FAIL;
	}
    }
    if (pkg->preUnFile) {
	if (addFileToTag(spec, pkg->preUnFile, pkg->header, RPMTAG_PREUN)) {
	    rpmlog(RPMLOG_ERR,
		     _("Could not open PreUn file: %s\n"), pkg->preUnFile);
	    return RPMRC_FAIL;
	}
    }
    if (pkg->preTransFile) {
	if (addFileToTag(spec, pkg->preTransFile, pkg->header, RPMTAG_PRETRANS)) {
	    rpmlog(RPMLOG_ERR,
		     _("Could not open PreIn file: %s\n"), pkg->preTransFile);
	    return RPMRC_FAIL;
	}
    }
    if (pkg->postInFile) {
	if (addFileToTag(spec, pkg->postInFile, pkg->header, RPMTAG_POSTIN)) {
	    rpmlog(RPMLOG_ERR,
		     _("Could not open PostIn file: %s\n"), pkg->postInFile);
	    return RPMRC_FAIL;
	}
    }
    if (pkg->postUnFile) {
	if (addFileToTag(spec, pkg->postUnFile, pkg->header, RPMTAG_POSTUN)) {
	    rpmlog(RPMLOG_ERR,
		     _("Could not open PostUn file: %s\n"), pkg->postUnFile);
	    return RPMRC_FAIL;
	}
    }
    if (pkg->postTransFile) {
	if (addFileToTag(spec, pkg->postTransFile, pkg->header, RPMTAG_POSTTRANS)) {
	    rpmlog(RPMLOG_ERR,
		     _("Could not open PostUn file: %s\n"), pkg->postTransFile);
	    return RPMRC_FAIL;
	}
    }
    if (pkg->verifyFile) {
	if (addFileToTag(spec, pkg->verifyFile, pkg->header,
			 RPMTAG_VERIFYSCRIPT)) {
	    rpmlog(RPMLOG_ERR,
		     _("Could not open VerifyScript file: %s\n"), pkg->verifyFile);
	    return RPMRC_FAIL;
	}
    }

    for (p = pkg->triggerFiles; p != NULL; p = p->next) {
	(void) headerAddOrAppendEntry(pkg->header, RPMTAG_TRIGGERSCRIPTPROG,
			       RPM_STRING_ARRAY_TYPE, &(p->prog), 1);
	if (p->script) {
	    (void) headerAddOrAppendEntry(pkg->header, RPMTAG_TRIGGERSCRIPTS,
				   RPM_STRING_ARRAY_TYPE, &(p->script), 1);
	} else if (p->fileName) {
	    if (addFileToArrayTag(spec, p->fileName, pkg->header,
				  RPMTAG_TRIGGERSCRIPTS)) {
		rpmlog(RPMLOG_ERR,
			 _("Could not open Trigger script file: %s\n"),
			 p->fileName);
		return RPMRC_FAIL;
	    }
	} else {
	    /* This is dumb.  When the header supports NULL string */
	    /* this will go away.                                  */
	    char *bull = "";
	    (void) headerAddOrAppendEntry(pkg->header, RPMTAG_TRIGGERSCRIPTS,
				   RPM_STRING_ARRAY_TYPE, &bull, 1);
	}
    }

    return 0;
}

#if defined(DEAD)
/*@-boundswrite@*/
int readRPM(const char *fileName, Spec *specp, void * l,
		Header *sigs, CSA_t csa)
{
    const char * msg = "";
    FD_t fdi;
    Spec spec;
    rpmRC rc;

    fdi = (fileName != NULL)
	? Fopen(fileName, "r.fdio")
	: fdDup(STDIN_FILENO);

    if (fdi == NULL || Ferror(fdi)) {
	rpmlog(RPMLOG_ERR, _("readRPM: open %s: %s\n"),
		(fileName ? fileName : "<stdin>"),
		Fstrerror(fdi));
	if (fdi) (void) Fclose(fdi);
	return RPMRC_FAIL;
    }

    {	const char item[] = "Lead";
	size_t nl = rpmpkgSizeof(item, NULL);

	if (nl == 0) {
	    rc = RPMRC_FAIL;
	    msg = "item size is zero";
	} else {
	    l = xcalloc(1, nl);		/* XXX memory leak */
	    msg = NULL;
	    rc = rpmpkgRead(item, fdi, l, &msg);
	    if (rc != RPMRC_OK && msg == NULL)
		msg = Fstrerror(fdi);
	}
    }

    if (rc != RPMRC_OK) {
	rpmlog(RPMLOG_ERR, _("readRPM: read %s: %s\n"),
		(fileName ? fileName : "<stdin>"), msg);
	return RPMRC_FAIL;
    }
    /*@=sizeoftype@*/

    /* XXX FIXME: EPIPE on <stdin> */
    if (Fseek(fdi, 0, SEEK_SET) == -1) {
	rpmlog(RPMLOG_ERR, _("%s: Fseek failed: %s\n"),
			(fileName ? fileName : "<stdin>"), Fstrerror(fdi));
	return RPMRC_FAIL;
    }

    /* Reallocate build data structures */
    spec = newSpec();
    spec->packages = newPackage(spec);

    /* XXX the header just allocated will be allocated again */
    spec->packages->header = headerFree(spec->packages->header);

    /* Read the rpm lead, signatures, and header */
    {	rpmts ts = rpmtsCreate();

	/* XXX W2DO? pass fileName? */
	/*@-mustmod@*/      /* LCL: segfault */
	rc = rpmReadPackageFile(ts, fdi, "readRPM",
			 &spec->packages->header);
	/*@=mustmod@*/

	ts = rpmtsFree(ts);

	if (sigs) *sigs = NULL;			/* XXX HACK */
    }

    switch (rc) {
    case RPMRC_OK:
    case RPMRC_NOKEY:
    case RPMRC_NOTTRUSTED:
	break;
    case RPMRC_NOTFOUND:
	rpmlog(RPMLOG_ERR, _("readRPM: %s is not an RPM package\n"),
		(fileName ? fileName : "<stdin>"));
	return RPMRC_FAIL;
    case RPMRC_FAIL:
    default:
	rpmlog(RPMLOG_ERR, _("readRPM: reading header from %s\n"),
		(fileName ? fileName : "<stdin>"));
	return RPMRC_FAIL;
	/*@notreached@*/ break;
    }

    /*@-branchstate@*/
    if (specp)
	*specp = spec;
    else
	spec = freeSpec(spec);
    /*@=branchstate@*/

    if (csa != NULL)
	csa->cpioFdIn = fdi;
    else
	(void) Fclose(fdi);

    return 0;
}
/*@=boundswrite@*/
#endif

#if defined(DEAD)
#define	RPMPKGVERSION_MIN	30004
#define	RPMPKGVERSION_MAX	40003
/*@unchecked@*/
static int rpmpkg_version = -1;

static int rpmLeadVersion(void)
	/*@globals rpmpkg_version, rpmGlobalMacroContext, h_errno @*/
	/*@modifies rpmpkg_version, rpmGlobalMacroContext @*/
{
    int rpmlead_version;

    /* Intitialize packaging version from macro configuration. */
    if (rpmpkg_version < 0) {
	rpmpkg_version = rpmExpandNumeric("%{_package_version}");
	if (rpmpkg_version < RPMPKGVERSION_MIN)
	    rpmpkg_version = RPMPKGVERSION_MIN;
	if (rpmpkg_version > RPMPKGVERSION_MAX)
	    rpmpkg_version = RPMPKGVERSION_MAX;
    }

    rpmlead_version = rpmpkg_version / 10000;
    /* XXX silly sanity check. */
    if (rpmlead_version < 3 || rpmlead_version > 4)
	rpmlead_version = 3;
    return rpmlead_version;
}
#endif

void providePackageNVR(Header h)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    const char *name, *version, *release;
    int_32 * epoch;
    const char *pEVR;
    char *p;
    int_32 pFlags = RPMSENSE_EQUAL;
    const char ** provides = NULL;
    const char ** providesEVR = NULL;
    rpmTagType pnt, pvt;
    int_32 * provideFlags = NULL;
    int providesCount;
    int i, xx;
    int bingo = 1;

    /* Generate provides for this package name-version-release. */
    xx = headerNEVRA(h, &name, NULL, &version, &release, NULL);
    if (!(name && version && release))
	return;
    pEVR = p = alloca(21 + strlen(version) + 1 + strlen(release) + 1);
    *p = '\0';
    if (hge(h, RPMTAG_EPOCH, NULL, &epoch, NULL)) {
	sprintf(p, "%d:", *epoch);
	while (*p != '\0')
	    p++;
    }
    (void) stpcpy( stpcpy( stpcpy(p, version) , "-") , release);

    /*
     * Rpm prior to 3.0.3 does not have versioned provides.
     * If no provides at all are available, we can just add.
     */
    if (!hge(h, RPMTAG_PROVIDENAME, &pnt, &provides, &providesCount))
	goto exit;

    /*
     * Otherwise, fill in entries on legacy packages.
     */
    if (!hge(h, RPMTAG_PROVIDEVERSION, &pvt, &providesEVR, NULL)) {
	for (i = 0; i < providesCount; i++) {
	    char * vdummy = "";
	    int_32 fdummy = RPMSENSE_ANY;
	    xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEVERSION, RPM_STRING_ARRAY_TYPE,
			&vdummy, 1);
	    xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEFLAGS, RPM_INT32_TYPE,
			&fdummy, 1);
	}
	goto exit;
    }

    xx = hge(h, RPMTAG_PROVIDEFLAGS, NULL, &provideFlags, NULL);

    /*@-nullderef@*/	/* LCL: providesEVR is not NULL */
    if (provides && providesEVR && provideFlags)
    for (i = 0; i < providesCount; i++) {
        if (!(provides[i] && providesEVR[i]))
            continue;
	if (!(provideFlags[i] == RPMSENSE_EQUAL &&
	    !strcmp(name, provides[i]) && !strcmp(pEVR, providesEVR[i])))
	    continue;
	bingo = 0;
	break;
    }
    /*@=nullderef@*/

exit:
    provides = hfd(provides, pnt);
    providesEVR = hfd(providesEVR, pvt);

    if (bingo) {
	xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDENAME, RPM_STRING_ARRAY_TYPE,
		&name, 1);
	xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEFLAGS, RPM_INT32_TYPE,
		&pFlags, 1);
	xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEVERSION, RPM_STRING_ARRAY_TYPE,
		&pEVR, 1);
    }
}

/*@-boundswrite@*/
int writeRPM(Header *hdrp, unsigned char ** pkgidp, const char *fileName,
		CSA_t csa, char *passPhrase, const char **cookie)
{
    FD_t fd = NULL;
    FD_t ifd = NULL;
    int_32 count, sigtag;
    const char * sigtarget;
    const char * rpmio_flags = NULL;
    const char * payload_format = NULL;
    const char * SHA1 = NULL;
    char *s;
    char buf[BUFSIZ];
    Header h;
    Header sigh = NULL;
    int addsig = 0;
    int isSource;
    int rc = 0;

    /* Transfer header reference form *hdrp to h. */
    h = headerLink(*hdrp);
    *hdrp = headerFree(*hdrp);

    if (pkgidp)
	*pkgidp = NULL;

#ifdef	DYING
    if (Fileno(csa->cpioFdIn) < 0) {
	csa->cpioArchiveSize = 0;
	/* Add a bogus archive size to the Header */
	(void) headerAddEntry(h, RPMTAG_ARCHIVESIZE, RPM_INT32_TYPE,
		&csa->cpioArchiveSize, 1);
    }
#endif

    /* Save payload information */
    isSource =
	(headerIsEntry(h, RPMTAG_SOURCERPM) == 0 &&
	 headerIsEntry(h, RPMTAG_ARCH) != 0);
    if (isSource) {
	payload_format = rpmExpand("%{?_source_payload_format}", NULL);
	rpmio_flags = rpmExpand("%{?_source_payload}", NULL);
    } else {
	payload_format = rpmExpand("%{?_binary_payload_format}", NULL);
	rpmio_flags = rpmExpand("%{?_binary_payload}", NULL);
    }

    if (!(payload_format && *payload_format)) {
	payload_format = _free(payload_format);
	payload_format = xstrdup("cpio");
    }
    if (!(rpmio_flags && *rpmio_flags)) {
	rpmio_flags = _free(rpmio_flags);
	rpmio_flags = xstrdup("w9.gzdio");
    }
    s = strchr(rpmio_flags, '.');
    if (s) {

	if (payload_format) {
	    if (!strcmp(payload_format, "tar")
	     || !strcmp(payload_format, "ustar")) {
		/* XXX addition to header is too late to be displayed/sorted. */
		/* Add prereq on rpm version that understands tar payloads */
		(void) rpmlibNeedsFeature(h, "PayloadIsUstar", "4.4.4-1");
	    }

	    (void) headerAddEntry(h, RPMTAG_PAYLOADFORMAT, RPM_STRING_TYPE,
			payload_format, 1);
	}

	/* XXX addition to header is too late to be displayed/sorted. */
	if (s[1] == 'g' && s[2] == 'z')
	    (void) headerAddEntry(h, RPMTAG_PAYLOADCOMPRESSOR, RPM_STRING_TYPE,
		"gzip", 1);
	else if (s[1] == 'b' && s[2] == 'z')
	    (void) headerAddEntry(h, RPMTAG_PAYLOADCOMPRESSOR, RPM_STRING_TYPE,
		"bzip2", 1);
	else if (s[1] == 'l' && s[2] == 'z') {
	    (void) headerAddEntry(h, RPMTAG_PAYLOADCOMPRESSOR, RPM_STRING_TYPE,
		"lzma", 1);
	    (void) rpmlibNeedsFeature(h, "PayloadIsLzma", "4.4.6-1");
	}
	strcpy(buf, rpmio_flags);
	buf[s - rpmio_flags] = '\0';
	(void) headerAddEntry(h, RPMTAG_PAYLOADFLAGS, RPM_STRING_TYPE, buf+1, 1);
    }

    /* Create and add the cookie */
    if (cookie) {
	sprintf(buf, "%s %d", buildHost(), (int) (*getBuildTime()));
	*cookie = xstrdup(buf);
	(void) headerAddEntry(h, RPMTAG_COOKIE, RPM_STRING_TYPE, *cookie, 1);
    }
    
    /* Reallocate the header into one contiguous region. */
    h = headerReload(h, RPMTAG_HEADERIMMUTABLE);
    if (h == NULL) {	/* XXX can't happen */
	rpmlog(RPMLOG_ERR, _("Unable to create immutable header region.\n"));
	rc = RPMRC_FAIL;
	goto exit;
    }
    /* Re-reference reallocated header. */
    *hdrp = headerLink(h);

    /*
     * Write the header+archive into a temp file so that the size of
     * archive (after compression) can be added to the header.
     */
    sigtarget = NULL;
    if (rpmTempFile(NULL, &sigtarget, &fd)) {
	rpmlog(RPMLOG_ERR, _("Unable to open temp file.\n"));
	rc = RPMRC_FAIL;
	goto exit;
    }

    fdInitDigest(fd, PGPHASHALGO_SHA1, 0);
    if (headerWrite(fd, h)) {
	rc = RPMRC_FAIL;
	rpmlog(RPMLOG_ERR, _("Unable to write temp header\n"));
    } else { /* Write the archive and get the size */
	(void) Fflush(fd);
	fdFiniDigest(fd, PGPHASHALGO_SHA1, &SHA1, NULL, 1);
	if (csa->cpioList != NULL) {
	    rc = cpio_doio(fd, h, csa, payload_format, rpmio_flags);
	} else if (Fileno(csa->cpioFdIn) >= 0) {
	    rc = cpio_copy(fd, csa);
	} else {
assert(0);
	}
    }
    rpmio_flags = _free(rpmio_flags);
    payload_format = _free(payload_format);

    if (rc)
	goto exit;

    (void) Fclose(fd);
    fd = NULL;
    (void) Unlink(fileName);

    if (rc)
	goto exit;

    /* Generate the signature */
    (void) fflush(stdout);
    sigh = headerNew();
    (void) rpmAddSignature(sigh, sigtarget, RPMSIGTAG_SIZE, passPhrase);
    (void) rpmAddSignature(sigh, sigtarget, RPMSIGTAG_MD5, passPhrase);

#if defined(SUPPORT_PGP_SIGNING)
    sigtag = rpmLookupSignatureType(RPMLOOKUPSIG_QUERY);
    addsig = (sigtag > 0);
#else
    sigtag = RPMSIGTAG_GPG;
    addsig = 0;	/* XXX breaks --sign */
#endif

    if (addsig) {
	rpmlog(RPMLOG_NOTICE, _("Generating signature: %d\n"), sigtag);
	(void) rpmAddSignature(sigh, sigtarget, sigtag, passPhrase);
    }
    
    if (SHA1) {
	(void) headerAddEntry(sigh, RPMSIGTAG_SHA1, RPM_STRING_TYPE, SHA1, 1);
	SHA1 = _free(SHA1);
    }

    {	int_32 payloadSize = csa->cpioArchiveSize;
	(void) headerAddEntry(sigh, RPMSIGTAG_PAYLOADSIZE, RPM_INT32_TYPE,
			&payloadSize, 1);
    }

    /* Reallocate the signature into one contiguous region. */
    sigh = headerReload(sigh, RPMTAG_HEADERSIGNATURES);
    if (sigh == NULL) {	/* XXX can't happen */
	rpmlog(RPMLOG_ERR, _("Unable to reload signature header.\n"));
	rc = RPMRC_FAIL;
	goto exit;
    }

    /* Open the output file */
    fd = Fopen(fileName, "w.fdio");
    if (fd == NULL || Ferror(fd)) {
	rpmlog(RPMLOG_ERR, _("Could not open %s: %s\n"),
		fileName, Fstrerror(fd));
	rc = RPMRC_FAIL;
	goto exit;
    }

    /* Write the lead section into the package. */
    if (!_nolead) {
	const char item[] = "Lead";
	size_t nl = rpmpkgSizeof(item, NULL);
	rpmRC _rc;

	if (nl == 0)
	    _rc = RPMRC_FAIL;
	else {
	    void * l = memset(alloca(nl), 0, nl);
	    const char * msg = buf;
	    const char *N, *V, *R;
	    (void) headerNEVRA(h, &N, NULL, &V, &R, NULL);
	    sprintf(buf, "%s-%s-%s", N, V, R);
	    _rc = rpmpkgWrite(item, fd, l, &msg);
	}

	if (_rc != RPMRC_OK) {
	    rc = RPMRC_FAIL;
	    rpmlog(RPMLOG_ERR, _("Unable to write package: %s\n"),
		 Fstrerror(fd));
	    goto exit;
	}
    }

    /* Write the signature section into the package. */
    if (!_nosigh) {
	const char item[] = "Signature";
	rpmRC _rc;

	_rc = rpmpkgWrite(item, fd, sigh, NULL);
	if (_rc != RPMRC_OK) {
	    rc = RPMRC_FAIL;
	    goto exit;
	}
    }

    /* Append the header and archive */
    ifd = Fopen(sigtarget, "r.fdio");
    if (ifd == NULL || Ferror(ifd)) {
	rc = RPMERR_READ;
	rpmlog(RPMLOG_ERR, _("Unable to open sigtarget %s: %s\n"),
		sigtarget, Fstrerror(ifd));
	goto exit;
    }

    /* Add signatures to header, and write header into the package. */
    /* XXX header+payload digests/signatures might be checked again here. */
    {	Header nh = headerRead(ifd);

	if (nh == NULL) {
	    rc = RPMERR_READ;
	    rpmlog(RPMLOG_ERR, _("Unable to read header from %s: %s\n"),
			sigtarget, Fstrerror(ifd));
	    goto exit;
	}

#ifdef	NOTYET
	(void) headerMergeLegacySigs(nh, sigh);
#endif

	rc = headerWrite(fd, nh);
	nh = headerFree(nh);

	if (rc) {
	    rc = RPMRC_FAIL;
	    rpmlog(RPMLOG_ERR, _("Unable to write header to %s: %s\n"),
			fileName, Fstrerror(fd));
	    goto exit;
	}
    }
	
    /* Write the payload into the package. */
    while ((count = Fread(buf, sizeof(buf[0]), sizeof(buf), ifd)) > 0) {
	if (count == -1) {
	    rpmlog(RPMLOG_ERR, _("Unable to read payload from %s: %s\n"),
		     sigtarget, Fstrerror(ifd));
	    rc = RPMRC_FAIL;
	    goto exit;
	}
	if (Fwrite(buf, sizeof(buf[0]), count, fd) != count) {
	    rpmlog(RPMLOG_ERR, _("Unable to write payload to %s: %s\n"),
		     fileName, Fstrerror(fd));
	    rc = RPMRC_FAIL;
	    goto exit;
	}
    }
    rc = 0;

exit:
    SHA1 = _free(SHA1);
    h = headerFree(h);

    /* XXX Fish the pkgid out of the signature header. */
    if (sigh != NULL && pkgidp != NULL) {
	int_32 tagType;
	unsigned char * MD5 = NULL;
	int_32 c;
	int xx;
	xx = headerGetEntry(sigh, RPMSIGTAG_MD5, &tagType, &MD5, &c);
	if (tagType == RPM_BIN_TYPE && MD5 != NULL && c == 16)
	    *pkgidp = MD5;
    }

    sigh = headerFree(sigh);
    if (ifd) {
	(void) Fclose(ifd);
	ifd = NULL;
    }
    if (fd) {
	(void) Fclose(fd);
	fd = NULL;
    }
    if (sigtarget) {
	(void) Unlink(sigtarget);
	sigtarget = _free(sigtarget);
    }

    if (rc == 0)
	rpmlog(RPMLOG_NOTICE, _("Wrote: %s\n"), fileName);
    else
	(void) Unlink(fileName);

    return rc;
}
/*@=boundswrite@*/

/*@unchecked@*/
static int_32 copyTags[] = {
    RPMTAG_CHANGELOGTIME,
    RPMTAG_CHANGELOGNAME,
    RPMTAG_CHANGELOGTEXT,
    0
};

/*@-boundswrite@*/
int packageBinaries(Spec spec)
{
    struct cpioSourceArchive_s csabuf;
    CSA_t csa = &csabuf;
    int rc;
    const char *errorString;
    Package pkg;

    for (pkg = spec->packages; pkg != NULL; pkg = pkg->next) {
	const char *fn;

	if (pkg->fileList == NULL)
	    continue;

	if (spec->cookie) {
	    (void) headerAddEntry(pkg->header, RPMTAG_COOKIE,
			   RPM_STRING_TYPE, spec->cookie, 1);
	}

	/* Copy changelog from src rpm */
	headerCopyTags(spec->packages->header, pkg->header, copyTags);
	
	(void) headerAddEntry(pkg->header, RPMTAG_RPMVERSION,
		       RPM_STRING_TYPE, VERSION, 1);
	(void) headerAddEntry(pkg->header, RPMTAG_BUILDHOST,
		       RPM_STRING_TYPE, buildHost(), 1);
	(void) headerAddEntry(pkg->header, RPMTAG_BUILDTIME,
		       RPM_INT32_TYPE, getBuildTime(), 1);

    {	const char * optflags = rpmExpand("%{optflags}", NULL);
	(void) headerAddEntry(pkg->header, RPMTAG_OPTFLAGS, RPM_STRING_TYPE,
			optflags, 1);
	optflags = _free(optflags);
    }

	(void) genSourceRpmName(spec);
	(void) headerAddEntry(pkg->header, RPMTAG_SOURCERPM, RPM_STRING_TYPE,
		       spec->sourceRpmName, 1);
	if (spec->sourcePkgId != NULL) {
	(void) headerAddEntry(pkg->header, RPMTAG_SOURCEPKGID, RPM_BIN_TYPE,
		       spec->sourcePkgId, 16);
	}
	
	{   const char *binFormat = rpmGetPath("%{_rpmfilename}", NULL);
	    char *binRpm, *binDir;
	    binRpm = headerSprintf(pkg->header, binFormat, rpmTagTable,
			       rpmHeaderFormats, &errorString);
	    binFormat = _free(binFormat);
	    if (binRpm == NULL) {
		const char *NVRA = NULL;
		(void) headerGetExtension(pkg->header, RPMTAG_NVRA,
			NULL, &NVRA, NULL);
		rpmlog(RPMLOG_ERR, _("Could not generate output "
		     "filename for package %s: %s\n"), NVRA, errorString);
		NVRA = _free(NVRA);
		return RPMRC_FAIL;
	    }
	    fn = rpmGetPath("%{_rpmdir}/", binRpm, NULL);
	    if ((binDir = strchr(binRpm, '/')) != NULL) {
		struct stat st;
		const char *dn;
		*binDir = '\0';
		dn = rpmGetPath("%{_rpmdir}/", binRpm, NULL);
		if (Stat(dn, &st) < 0) {
		    switch(errno) {
		    case  ENOENT:
			if (Mkdir(dn, 0755) == 0)
			    /*@switchbreak@*/ break;
			/*@fallthrough@*/
		    default:
			rpmlog(RPMLOG_ERR,_("cannot create %s: %s\n"),
			    dn, strerror(errno));
			/*@switchbreak@*/ break;
		    }
		}
		dn = _free(dn);
	    }
	    binRpm = _free(binRpm);
	}

	memset(csa, 0, sizeof(*csa));
	csa->cpioArchiveSize = 0;
	/*@-type@*/ /* LCL: function typedefs */
	csa->cpioFdIn = fdNew("init (packageBinaries)");
	/*@-assignexpose -newreftrans@*/
	csa->cpioList = rpmfiLink(pkg->cpioList, "packageBinaries");
	/*@=assignexpose =newreftrans@*/

	rc = writeRPM(&pkg->header, NULL, fn,
		    csa, spec->passPhrase, NULL);

	csa->cpioList->te = _free(csa->cpioList->te);	/* XXX memory leak */
	csa->cpioList = rpmfiFree(csa->cpioList);
	csa->cpioFdIn = fdFree(csa->cpioFdIn, "init (packageBinaries)");
	/*@=type@*/
	fn = _free(fn);
	if (rc)
	    return rc;
    }
    
    return 0;
}
/*@=boundswrite@*/

/*@-boundswrite@*/
int packageSources(Spec spec)
{
    struct cpioSourceArchive_s csabuf;
    CSA_t csa = &csabuf;
    int rc;

    /* Add some cruft */
    (void) headerAddEntry(spec->sourceHeader, RPMTAG_RPMVERSION,
		   RPM_STRING_TYPE, VERSION, 1);
    (void) headerAddEntry(spec->sourceHeader, RPMTAG_BUILDHOST,
		   RPM_STRING_TYPE, buildHost(), 1);
    (void) headerAddEntry(spec->sourceHeader, RPMTAG_BUILDTIME,
		   RPM_INT32_TYPE, getBuildTime(), 1);

    (void) genSourceRpmName(spec);

    spec->cookie = _free(spec->cookie);
    
    /* XXX this should be %_srpmdir */
    {	const char *fn = rpmGetPath("%{_srcrpmdir}/", spec->sourceRpmName,NULL);

	memset(csa, 0, sizeof(*csa));
	csa->cpioArchiveSize = 0;
	/*@-type@*/ /* LCL: function typedefs */
	csa->cpioFdIn = fdNew("init (packageSources)");
	/*@-assignexpose -newreftrans@*/
	csa->cpioList = rpmfiLink(spec->sourceCpioList, "packageSources");
	/*@=assignexpose =newreftrans@*/

	spec->sourcePkgId = NULL;
	rc = writeRPM(&spec->sourceHeader, &spec->sourcePkgId, fn,
		csa, spec->passPhrase, &(spec->cookie));

	csa->cpioList->te = _free(csa->cpioList->te);	/* XXX memory leak */
	csa->cpioList = rpmfiFree(csa->cpioList);
	csa->cpioFdIn = fdFree(csa->cpioFdIn, "init (packageSources)");
	/*@=type@*/
	fn = _free(fn);
    }
    return rc;
}
/*@=boundswrite@*/

/** \ingroup rpmcli
 * \file lib/rpmchecksig.c
 * Verify the signature of a package.
 */

#include "system.h"

#include <rpmio_internal.h>
#include <poptIO.h>
#include <rpmbc.h>		/* XXX beecrypt base64 */
#include <rpmtag.h>
#include <rpmtypes.h>
#define	_RPMEVR_INTERNAL	/* XXX RPMSENSE_KEYRING */
#include <rpmevr.h>
#include <rpmdb.h>
#include <rpmxar.h>
#include <pkgio.h>
#include "signature.h"

#include <rpmts.h>

#include "rpmgi.h"

#include <rpmversion.h>
#include <rpmcli.h>

#include "debug.h"

/*@access FD_t @*/		/* XXX stealing digests */
/*@access Header @*/		/* XXX void * arg */
/*@access pgpDig @*/
/*@access pgpDigParams @*/

/*@unchecked@*/
int _print_pkts = 0;

/**
 */
static int manageFile(/*@out@*/ FD_t *fdp,
		/*@null@*/ /*@out@*/ const char **fnp,
		int flags, /*@unused@*/ int rc)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies *fdp, *fnp, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    const char *fn;
    FD_t fd;

    if (fdp == NULL)	/* programmer error */
	return 1;

    /* close and reset *fdp to NULL */
    if (*fdp && (fnp == NULL || *fnp == NULL)) {
	(void) Fclose(*fdp);
	*fdp = NULL;
	return 0;
    }

    /* open a file and set *fdp */
    if (*fdp == NULL && fnp != NULL && *fnp != NULL) {
	fd = Fopen(*fnp, ((flags & O_WRONLY) ? "w.fdio" : "r.fdio"));
	if (fd == NULL || Ferror(fd)) {
	    rpmlog(RPMLOG_ERR, _("%s: open failed: %s\n"), *fnp,
		Fstrerror(fd));
	    return 1;
	}
	*fdp = fd;
	return 0;
    }

    /* open a temp file */
    if (*fdp == NULL && (fnp == NULL || *fnp == NULL)) {
	fn = NULL;
	if (rpmTempFile(NULL, (fnp ? &fn : NULL), &fd)) {
	    rpmlog(RPMLOG_ERR, _("rpmTempFile failed\n"));
	    return 1;
	}
	if (fnp != NULL)
	    *fnp = fn;
/*@-refcounttrans@*/	/* FIX: XfdLink/XfdFree annotation */
	*fdp = fdLink(fd, "manageFile return");
	fd = fdFree(fd, "manageFile return");
/*@=refcounttrans@*/
	return 0;
    }

    /* no operation */
    if (*fdp != NULL && fnp != NULL && *fnp != NULL)
	return 0;

    /* XXX never reached */
    return 1;
}

/**
 * Copy header+payload, calculating digest(s) on the fly.
 */
static int copyFile(FD_t *sfdp, const char **sfnp,
		FD_t *tfdp, const char **tfnp)
	/*@globals rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies *sfdp, *sfnp, *tfdp, *tfnp, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    unsigned char buf[BUFSIZ];
    ssize_t count;
    int rc = 1;

    if (manageFile(sfdp, sfnp, O_RDONLY, 0))
	goto exit;
    if (manageFile(tfdp, tfnp, O_WRONLY|O_CREAT|O_TRUNC, 0))
	goto exit;

    while ((count = Fread(buf, sizeof(buf[0]), sizeof(buf), *sfdp)) > 0)
    {
	if (Fwrite(buf, sizeof(buf[0]), count, *tfdp) != (size_t)count) {
	    rpmlog(RPMLOG_ERR, _("%s: Fwrite failed: %s\n"), *tfnp,
		Fstrerror(*tfdp));
	    goto exit;
	}
    }
    if (count < 0) {
	rpmlog(RPMLOG_ERR, _("%s: Fread failed: %s\n"), *sfnp, Fstrerror(*sfdp));
	goto exit;
    }
    if (Fflush(*tfdp) != 0) {
	rpmlog(RPMLOG_ERR, _("%s: Fflush failed: %s\n"), *tfnp,
	    Fstrerror(*tfdp));
	goto exit;
    }

    rc = 0;

exit:
    if (*sfdp)	(void) manageFile(sfdp, NULL, 0, rc);
    if (*tfdp)	(void) manageFile(tfdp, NULL, 0, rc);
    return rc;
}

/**
 * Retrieve signer fingerprint from an OpenPGP signature tag.
 * @param sigh		signature header
 * @param sigtag	signature tag
 * @retval signid	signer fingerprint
 * @return		0 on success
 */
static int getSignid(Header sigh, rpmSigTag sigtag, unsigned char * signid)
	/*@globals fileSystem, internalState @*/
	/*@modifies *signid, fileSystem, internalState @*/
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    int rc = 1;
    int xx;

    he->tag = (rpmTag) sigtag;
    xx = headerGet(sigh, he, 0);
    if (xx && he->p.ptr != NULL) {
	pgpDig dig = pgpDigNew(0);

	if (!pgpPrtPkts(he->p.ptr, he->c, dig, 0)) {
	    memcpy(signid, dig->signature.signid, sizeof(dig->signature.signid));
	    rc = 0;
	}
     
	he->p.ptr = _free(he->p.ptr);
	dig = pgpDigFree(dig, "getSignid");
    }
    return rc;
}

/** \ingroup rpmcli
 * Create/modify elements in signature header.
 * @param ts		transaction set
 * @param qva		mode flags and parameters
 * @param argv		array of package file names (NULL terminated)
 * @return		0 on success
 */
static int rpmReSign(/*@unused@*/ rpmts ts,
		QVA_t qva, const char ** argv)
        /*@globals rpmGlobalMacroContext, h_errno,
                fileSystem, internalState @*/
        /*@modifies ts, rpmGlobalMacroContext,
                fileSystem, internalState @*/
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    rpmgi gi = NULL;
    FD_t fd = NULL;
    FD_t ofd = NULL;
    struct rpmlead *lead = NULL;
    rpmSigTag sigtag;
    const char *sigtarget = NULL;
    char tmprpm[1024+1];
    Header sigh = NULL;
    const char * msg = NULL;
    int res = EXIT_FAILURE;
    int deleting = (qva->qva_mode == RPMSIGN_DEL_SIGNATURE);
    rpmRC rc;
    int xx;
    int i;
    
    tmprpm[0] = '\0';

    if (argv)
 {       /* start-of-arg-iteration */
    rpmuint32_t tag = (qva->qva_source == RPMQV_FTSWALK)
	? RPMDBI_FTSWALK : RPMDBI_ARGLIST;
    rpmgiFlags _giFlags = RPMGI_NONE;

    gi = rpmgiNew(ts, tag, NULL, 0);
/*@-mods@*/
    if (rpmioFtsOpts == 0)
	rpmioFtsOpts = (FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOSTAT);
/*@=mods@*/
    rc = rpmgiSetArgs(gi, argv, rpmioFtsOpts, (_giFlags|RPMGI_NOHEADER));

    while (rpmgiNext(gi) == RPMRC_OK) {
	const char * fn = rpmgiHdrPath(gi);
	const char * tfn;

	fprintf(stdout, "%s:\n", fn);

/*@-modobserver@*/	/* XXX rpmgiHdrPath should not be observer */
	if (manageFile(&fd, &fn, O_RDONLY, 0))
	    goto exit;
/*@=modobserver@*/

    {	const char item[] = "Lead";
	msg = NULL;
	rc = rpmpkgRead(item, fd, &lead, &msg);
	if (rc != RPMRC_OK) {
	    rpmlog(RPMLOG_ERR, "%s: %s: %s\n", fn, item, msg);
	    msg = _free(msg);
	    goto exit;
	}
	msg = _free(msg);
    }

    {	const char item[] = "Signature";
	msg = NULL;
	rc = rpmpkgRead(item, fd, &sigh, &msg);
	switch (rc) {
	default:
	    rpmlog(RPMLOG_ERR, "%s: %s: %s\n", fn, item,
			(msg && *msg ? msg : ""));
	    msg = _free(msg);
	    goto exit;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	case RPMRC_OK:
	    if (sigh == NULL) {
		rpmlog(RPMLOG_ERR, _("%s: No signature available\n"), fn);
		goto exit;
	    }
	    /*@switchbreak@*/ break;
	}
	msg = _free(msg);
    }

	/* Write the header and archive to a temp file */
	/* ASSERT: ofd == NULL && sigtarget == NULL */
/*@-modobserver@*/	/* XXX rpmgiHdrPath should not be observer */
	if (copyFile(&fd, &fn, &ofd, &sigtarget))
	    goto exit;
/*@=modobserver@*/
	/* Both fd and ofd are now closed. sigtarget contains tempfile name. */
	/* ASSERT: fd == NULL && ofd == NULL */

	/* Lose the immutable region (if present). */
	he->tag = RPMTAG_HEADERSIGNATURES;
	xx = headerGet(sigh, he, 0);
	if (xx) {
	    HE_t ohe = memset(alloca(sizeof(*ohe)), 0, sizeof(*ohe));
	    HeaderIterator hi;
	    Header oh;
	    Header nh;

	    nh = headerNew();
	    if (nh == NULL) {
		he->p.ptr = _free(he->p.ptr);
		goto exit;
	    }

	    oh = headerCopyLoad(he->p.ptr);
	    for (hi = headerInit(oh);
		headerNext(hi, ohe, 0);
		ohe->p.ptr = _free(ohe->p.ptr))
	    {
		if (ohe->p.ptr) {
		    xx = headerPut(nh, ohe, 0);
		}
	    }
	    hi = headerFini(hi);
	    (void)headerFree(oh);
	    oh = NULL;

	    (void)headerFree(sigh);
	    sigh = NULL;
	    sigh = headerLink(nh);
	    (void)headerFree(nh);
	    nh = NULL;
	}

if (sigh != NULL) {
	/* Eliminate broken digest values. */
	he->tag = (rpmTag)RPMSIGTAG_LEMD5_1;
	xx = headerDel(sigh, he, 0);
	he->tag = (rpmTag)RPMSIGTAG_LEMD5_2;
	xx = headerDel(sigh, he, 0);
	he->tag = (rpmTag)RPMSIGTAG_BADSHA1_1;
	xx = headerDel(sigh, he, 0);
	he->tag = (rpmTag)RPMSIGTAG_BADSHA1_2;
	xx = headerDel(sigh, he, 0);

	/* Toss and recalculate header+payload size and digests. */
	{   static const rpmuint32_t sigs[] =
		{ RPMSIGTAG_SIZE, RPMSIGTAG_MD5, RPMSIGTAG_SHA1 };
	    size_t nsigs = sizeof(sigs) / sizeof(sigs[0]);
	    for (i = 0; i < (int)nsigs; i++) {
		he->tag = (rpmTag)sigs[i];
		xx = headerDel(sigh, he, 0);
		xx = rpmAddSignature(sigh, sigtarget, (rpmSigTag) he->tag, qva->passPhrase);
		if (xx)
		    goto exit;
	    }
	}

	if (deleting) {
	    /* Nuke all the signature tags. */
	    static const rpmuint32_t sigs[] =
		{ RPMSIGTAG_GPG, RPMSIGTAG_PGP5, RPMSIGTAG_PGP,
		  RPMSIGTAG_DSA, RPMSIGTAG_RSA };
	    size_t nsigs = sizeof(sigs) / sizeof(sigs[0]);
	    for (i = 0; i < (int)nsigs; i++) {
		he->tag = (rpmTag)sigs[i];
		xx = headerDel(sigh, he, 0);
	    }
	} else {		/* If gpg/pgp is configured, replace the signature. */
	  int addsig = 0;
	  sigtag = RPMSIGTAG_GPG;
	  addsig = 1;

	  if (addsig) {
	    unsigned char oldsignid[8], newsignid[8];

	    /* Grab the old signature fingerprint (if any) */
	    memset(oldsignid, 0, sizeof(oldsignid));
	    xx = getSignid(sigh, sigtag, oldsignid);

	    switch (sigtag) {
	    default:
		/*@switchbreak@*/ break;
	    case RPMSIGTAG_DSA:
		he->tag = (rpmTag)RPMSIGTAG_GPG;
		xx = headerDel(sigh, he, 0);
		/*@switchbreak@*/ break;
	    case RPMSIGTAG_RSA:
		he->tag = (rpmTag)RPMSIGTAG_PGP;
		xx = headerDel(sigh, he, 0);
		/*@switchbreak@*/ break;
	    case RPMSIGTAG_GPG:
		he->tag = (rpmTag)RPMSIGTAG_PGP;
		xx = headerDel(sigh, he, 0);
		he->tag = (rpmTag)RPMSIGTAG_DSA;
		xx = headerDel(sigh, he, 0);
		/*@fallthrough@*/
	    case RPMSIGTAG_PGP5:
	    case RPMSIGTAG_PGP:
		he->tag = (rpmTag)RPMSIGTAG_RSA;
		xx = headerDel(sigh, he, 0);
		/*@switchbreak@*/ break;
	    }

	    he->tag = (rpmTag)sigtag;
	    xx = headerDel(sigh, he, 0);
	    xx = rpmAddSignature(sigh, sigtarget, sigtag, qva->passPhrase);
	    if (xx)
		goto exit;

	    /* If package was previously signed, check for same signer. */
	    memset(newsignid, 0, sizeof(newsignid));
	    if (memcmp(oldsignid, newsignid, sizeof(oldsignid))) {

		/* Grab the new signature fingerprint */
		xx = getSignid(sigh, sigtag, newsignid);

		/* If same signer, skip resigning the package. */
		if (!memcmp(oldsignid, newsignid, sizeof(oldsignid))) {

		    rpmlog(RPMLOG_WARNING,
			_("%s: was already signed by key ID %s, skipping\n"),
			fn, pgpHexStr(newsignid+4, sizeof(newsignid)-4));

		    /* Clean up intermediate target */
		    xx = Unlink(sigtarget);
		    sigtarget = _free(sigtarget);
		    continue;
		}
	    }
	  }
	}

	/* Reallocate the signature into one contiguous region. */
	sigh = headerReload(sigh, RPMTAG_HEADERSIGNATURES);
	if (sigh == NULL)	/* XXX can't happen */
	    goto exit;
}

	/* Write the lead/signature of the output rpm */
	(void) stpcpy( stpcpy(tmprpm, fn), ".XXXXXX");

#if defined(HAVE_MKSTEMP)
	(void) close(mkstemp(tmprpm));
#else
	(void) mktemp(tmprpm);
#endif
	tfn = tmprpm;

	if (manageFile(&ofd, &tfn, O_WRONLY|O_CREAT|O_TRUNC, 0))
	    goto exit;

	{   const char item[] = "Lead";
	    rc = rpmpkgWrite(item, ofd, lead, NULL);
	    if (rc != RPMRC_OK) {
		rpmlog(RPMLOG_ERR, "%s: %s: %s\n", tfn, item, Fstrerror(ofd));
		goto exit;
	    }
	}

	{   const char item[] = "Signature";
	    rc = rpmpkgWrite(item, ofd, sigh, NULL);
	    if (rc != RPMRC_OK) {
		rpmlog(RPMLOG_ERR, "%s: %s: %s\n", tfn, item, Fstrerror(ofd));
		goto exit;
	    }
	}

	/* Append the header and archive from the temp file */
	/* ASSERT: fd == NULL && ofd != NULL */
	if (copyFile(&fd, &sigtarget, &ofd, &tfn))
	    goto exit;
	/* Both fd and ofd are now closed. */
	/* ASSERT: fd == NULL && ofd == NULL */

	/* Move final target into place. */
	xx = Unlink(fn);
	xx = Rename(tfn, fn);
	tmprpm[0] = '\0';

	/* Clean up intermediate target */
	xx = Unlink(sigtarget);
	sigtarget = _free(sigtarget);
    }

 }	/* end-of-arg-iteration */

    res = 0;

exit:
    if (fd)	(void) manageFile(&fd, NULL, 0, res);
    if (ofd)	(void) manageFile(&ofd, NULL, 0, res);

    lead = _free(lead);
    (void)headerFree(sigh);
    sigh = NULL;

    gi = rpmgiFree(gi);

    if (sigtarget) {
	xx = Unlink(sigtarget);
	sigtarget = _free(sigtarget);
    }
    if (tmprpm[0] != '\0') {
	xx = Unlink(tmprpm);
	tmprpm[0] = '\0';
    }

    return res;
}

rpmRC rpmcliImportPubkey(const rpmts ts, const unsigned char * pkt, ssize_t pktlen)
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    static unsigned char zeros[] =
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    const char * afmt = "%{pubkeys:armor}";
    const char * group = "Public Keys";
    const char * license = "pubkey";
    const char * buildhost = "localhost";
    rpmuint32_t pflags = (RPMSENSE_KEYRING|RPMSENSE_EQUAL);
    rpmuint32_t zero = 0;
    pgpDig dig = NULL;
    pgpDigParams pubp = NULL;
    const char * d = NULL;
    const char * enc = NULL;
    const char * n = NULL;
    const char * u = NULL;
    const char * v = NULL;
    const char * r = NULL;
    const char * evr = NULL;
    Header h = NULL;
    rpmRC rc = RPMRC_FAIL;		/* assume failure */
    char * t;
    int xx;

    if (pkt == NULL || pktlen <= 0)
	return RPMRC_FAIL;
    if (rpmtsOpenDB(ts, (O_RDWR|O_CREAT)))
	return RPMRC_FAIL;

/*@-moduncon@*/
    if ((enc = b64encode(pkt, pktlen)) == NULL)
	goto exit;
/*@=moduncon@*/

    dig = pgpDigNew(0);

    /* Build header elements. */
    (void) pgpPrtPkts(pkt, pktlen, dig, 0);
    pubp = pgpGetPubkey(dig);

    if (!memcmp(pubp->signid, zeros, sizeof(pubp->signid))
     || !memcmp(pubp->time, zeros, sizeof(pubp->time))
     || pubp->userid == NULL)
	goto exit;

    v = t = xmalloc(16+1);
    t = stpcpy(t, pgpHexStr(pubp->signid, sizeof(pubp->signid)));

    r = t = xmalloc(8+1);
    t = stpcpy(t, pgpHexStr(pubp->time, sizeof(pubp->time)));

    n = t = xmalloc(sizeof("gpg()")+8);
    t = stpcpy( stpcpy( stpcpy(t, "gpg("), v+8), ")");

    /*@-nullpass@*/ /* FIX: pubp->userid may be NULL */
    u = t = xmalloc(sizeof("gpg()")+strlen(pubp->userid));
    t = stpcpy( stpcpy( stpcpy(t, "gpg("), pubp->userid), ")");
    /*@=nullpass@*/

    evr = t = xmalloc(sizeof("4X:-")+strlen(v)+strlen(r));
    t = stpcpy(t, (pubp->version == 4 ? "4:" : "3:"));
    t = stpcpy( stpcpy( stpcpy(t, v), "-"), r);

    /* Check for pre-existing header. */

    /* Build pubkey header. */
    h = headerNew();

    he->append = 1;

    he->tag = RPMTAG_PUBKEYS;
    he->t = RPM_STRING_ARRAY_TYPE;
    he->p.argv = &enc;
    he->c = 1;
    xx = headerPut(h, he, 0);

    he->append = 0;

    d = headerSprintf(h, afmt, NULL, rpmHeaderFormats, NULL);
    if (d == NULL)
	goto exit;

    he->t = RPM_STRING_TYPE;
    he->c = 1;
    he->tag = RPMTAG_NAME;
    he->p.str = xstrdup("gpg-pubkey");
    xx = headerPut(h, he, 0);
    he->p.ptr = _free(he->p.ptr);
    he->tag = RPMTAG_VERSION;
    he->p.str = v+8;
    xx = headerPut(h, he, 0);
    he->tag = RPMTAG_RELEASE;
    he->p.str = xstrdup(r);
    xx = headerPut(h, he, 0);
    he->p.ptr = _free(he->p.ptr);

    /* Add Summary/Description/Group. */
    he->tag = RPMTAG_DESCRIPTION;
    he->p.str = xstrdup(d);
#if defined(SUPPORT_IMPLICIT_TAG_DATA_TYPES)
    xx = headerAddI18NString(h, he->tag, he->p.ptr, "C");
#else
    xx = headerPut(h, he, 0);
#endif
    he->p.ptr = _free(he->p.ptr);

    he->tag = RPMTAG_GROUP;
    he->p.str = xstrdup(group);
#if defined(SUPPORT_IMPLICIT_TAG_DATA_TYPES)
    xx = headerAddI18NString(h, he->tag, he->p.ptr, "C");
#else
    xx = headerPut(h, he, 0);
#endif
    he->p.ptr = _free(he->p.ptr);

    he->tag = RPMTAG_SUMMARY;
    he->p.str = xstrdup(u);
#if defined(SUPPORT_IMPLICIT_TAG_DATA_TYPES)
    xx = headerAddI18NString(h, he->tag, he->p.ptr, "C");
#else
    xx = headerPut(h, he, 0);
#endif
    he->p.ptr = _free(he->p.ptr);

#ifdef	NOTYET	/* XXX can't erase pubkeys with "pubkey" arch. */
    /* Add a "pubkey" arch/os to avoid missing value NULL ptrs. */
    he->tag = RPMTAG_ARCH;
    he->p.str = "pubkey";
    xx = headerPut(h, he, 0);
    he->tag = RPMTAG_OS;
    he->p.str = "pubkey";
    xx = headerPut(h, he, 0);
#endif

    he->tag = RPMTAG_LICENSE;
    he->p.str = xstrdup(license);
    xx = headerPut(h, he, 0);
    he->p.ptr = _free(he->p.ptr);

    he->tag = RPMTAG_SIZE;
    he->t = RPM_UINT32_TYPE;
    he->p.ui32p = &zero;
    he->c = 1;
    xx = headerPut(h, he, 0);

    he->append = 1;

    he->tag = RPMTAG_PROVIDENAME;
    he->t = RPM_STRING_ARRAY_TYPE;
    he->p.argv = &u;
    he->c = 1;
    xx = headerPut(h, he, 0);
    he->tag = RPMTAG_PROVIDEVERSION;
    he->t = RPM_STRING_ARRAY_TYPE;
    he->p.argv = &evr;
    he->c = 1;
    xx = headerPut(h, he, 0);
    he->tag = RPMTAG_PROVIDEFLAGS;
    he->t = RPM_UINT32_TYPE;
    he->p.ui32p = &pflags;
    he->c = 1;
    xx = headerPut(h, he, 0);

    he->tag = RPMTAG_PROVIDENAME;
    he->t = RPM_STRING_ARRAY_TYPE;
    he->p.argv = &n;
    he->c = 1;
    xx = headerPut(h, he, 0);
    he->tag = RPMTAG_PROVIDEVERSION;
    he->t = RPM_STRING_ARRAY_TYPE;
    he->p.argv = &evr;
    he->c = 1;
    xx = headerPut(h, he, 0);
    he->tag = RPMTAG_PROVIDEFLAGS;
    he->t = RPM_UINT32_TYPE;
    he->p.ui32p = &pflags;
    he->c = 1;
    xx = headerPut(h, he, 0);

    he->append = 0;

    he->tag = RPMTAG_RPMVERSION;
    he->t = RPM_STRING_TYPE;
    he->p.str = xstrdup(RPMVERSION);
    he->c = 1;
    xx = headerPut(h, he, 0);
    he->p.ptr = _free(he->p.ptr);

    /* XXX W2DO: tag value inherited from parent? */
    he->tag = RPMTAG_BUILDHOST;
    he->t = RPM_STRING_TYPE;
    he->p.str = xstrdup(buildhost);
    he->c = 1;
    xx = headerPut(h, he, 0);
    he->p.ptr = _free(he->p.ptr);

    {   rpmuint32_t tid = rpmtsGetTid(ts);
	he->tag = RPMTAG_INSTALLTIME;
	he->t = RPM_UINT32_TYPE;
	he->p.ui32p = &tid;
	he->c = 1;
	xx = headerPut(h, he, 0);
	/* XXX W2DO: tag value inherited from parent? */
	he->tag = RPMTAG_BUILDTIME;
	he->t = RPM_UINT32_TYPE;
	he->p.ui32p = &tid;
	he->c = 1;
	xx = headerPut(h, he, 0);
    }

#ifdef	NOTYET
    /* XXX W2DO: tag value inherited from parent? */
    he->tag = RPMTAG_SOURCERPM;
    he->t = RPM_STRING_TYPE;
    he->p.str = fn;
    he->c = 1;
    xx = headerPut(h, he, 0);
#endif

    /* Add header to database. */
    xx = rpmdbAdd(rpmtsGetRdb(ts), rpmtsGetTid(ts), h, NULL);
    if (xx != 0)
	goto exit;
    rc = RPMRC_OK;

exit:
    /* Clean up. */
    (void)headerFree(h);
    h = NULL;
    dig = pgpDigFree(dig, "rpmcliImportPubkey");
    n = _free(n);
    u = _free(u);
    v = _free(v);
    r = _free(r);
    evr = _free(evr);
    enc = _free(enc);
    d = _free(d);
    
    return rc;
}

/** \ingroup rpmcli
 * Import public key(s).
 * @todo Implicit --update policy for gpg-pubkey headers.
 * @param ts		transaction set
 * @param qva		mode flags and parameters
 * @param argv		array of pubkey file names (NULL terminated)
 * @return		0 on success
 */
static int rpmcliImportPubkeys(const rpmts ts,
		/*@unused@*/ QVA_t qva,
		/*@null@*/ const char ** argv)
	/*@globals rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies ts, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    const char * fn;
    rpmuint8_t * pkt = NULL;
    size_t pktlen = 0;
    char * t = NULL;
    int res = 0;
    rpmRC rpmrc;
    int rc;

    if (argv == NULL) return res;

    while ((fn = *argv++) != NULL) {

	rpmtsClean(ts);
	pkt = _free(pkt);
	t = _free(t);

	/* If arg looks like a keyid, then attempt keyserver retrieve. */
	if (fn[0] == '0' && fn[1] == 'x') {
	    const char * s;
	    int i;
	    for (i = 0, s = fn+2; *s && isxdigit(*s); s++, i++)
		{};
	    if (i == 8 || i == 16) {
		t = rpmExpand("%{_hkp_keyserver_query}", fn+2, NULL);
		if (t && *t != '%')
		    fn = t;
	    }
	}

	/* Read pgp packet. */
	if ((rc = pgpReadPkts(fn, &pkt, &pktlen)) <= 0) {
	    rpmlog(RPMLOG_ERR, _("%s: import read failed(%d).\n"), fn, rc);
	    res++;
	    continue;
	}
	if (rc != PGPARMOR_PUBKEY) {
	    rpmlog(RPMLOG_ERR, _("%s: not an armored public key.\n"), fn);
	    res++;
	    continue;
	}

	/* Import pubkey packet(s). */
	if ((rpmrc = rpmcliImportPubkey(ts, pkt, pktlen)) != RPMRC_OK) {
	    rpmlog(RPMLOG_ERR, _("%s: import failed.\n"), fn);
	    res++;
	    continue;
	}

    }
    
rpmtsClean(ts);
    pkt = _free(pkt);
    t = _free(t);
    return res;
}

/**
 * @todo If the GPG key was known available, the md5 digest could be skipped.
 */
static rpmRC readFile(FD_t fd, const char * fn)
	/*@globals fileSystem, internalState @*/
	/*@modifies fd, fileSystem, internalState @*/
{
rpmxar xar = fdGetXAR(fd);
pgpDig dig = fdGetDig(fd);
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    unsigned char buf[4*BUFSIZ];
    ssize_t count;
    rpmRC rc;
    int xx;

    dig->nbytes = 0;

    /* Read the header from the package. */
    {	Header h = NULL;
	const char item[] = "Header";
	const char * msg = NULL;
	rc = rpmpkgRead(item, fd, &h, &msg);
	if (rc != RPMRC_OK) {
	    rpmlog(RPMLOG_ERR, "%s: %s: %s\n", fn, item, msg);
	    msg = _free(msg);
	    goto exit;
	}
	msg = _free(msg);

	dig->nbytes += headerSizeof(h);

	if (headerIsEntry(h, RPMTAG_HEADERIMMUTABLE)) {
	    unsigned char * hmagic = NULL;
	    size_t nmagic = 0;
	
	    he->tag = RPMTAG_HEADERIMMUTABLE;
	    xx = headerGet(h, he, 0);
	    if (!xx || he->p.ptr == NULL) {
		(void)headerFree(h);
		h = NULL;
		rpmlog(RPMLOG_ERR, "%s: %s: %s\n", fn, _("headerGet failed"),
			_("failed to retrieve original header\n"));
		rc = RPMRC_FAIL;
		goto exit;
	    }
	    (void) headerGetMagic(NULL, &hmagic, &nmagic);
	    dig->hdrsha1ctx = rpmDigestInit(PGPHASHALGO_SHA1, RPMDIGEST_NONE);
	    if (hmagic && nmagic > 0)
		(void) rpmDigestUpdate(dig->hdrsha1ctx, hmagic, nmagic);
	    (void) rpmDigestUpdate(dig->hdrsha1ctx, he->p.ptr, he->c);
	    dig->hdrctx = rpmDigestInit(dig->signature.hash_algo, RPMDIGEST_NONE);
	    if (hmagic && nmagic > 0)
		(void) rpmDigestUpdate(dig->hdrctx, hmagic, nmagic);
	    (void) rpmDigestUpdate(dig->hdrctx, he->p.ptr, he->c);
	    he->p.ptr = _free(he->p.ptr);
	}
	(void)headerFree(h);
	h = NULL;
    }

    if (xar != NULL) {
	const char item[] = "Payload";
	if ((xx = rpmxarNext(xar)) != 0 || (xx = rpmxarPull(xar, item)) != 0) {
	    rpmlog(RPMLOG_ERR, "%s: %s: %s\n", fn, item,
		_("XAR file not found (or no XAR support)"));
	    rc = RPMRC_NOTFOUND;
	    goto exit;
	}
    }

    /* Read the payload from the package. */
    while ((count = Fread(buf, sizeof(buf[0]), sizeof(buf), fd)) > 0)
	dig->nbytes += count;
    if (count < 0 || Ferror(fd)) {
	rpmlog(RPMLOG_ERR, "%s: %s: %s\n", fn, _("Fread failed"), Fstrerror(fd));
	rc = RPMRC_FAIL;
	goto exit;
    }

    /* XXX Steal the digest-in-progress from the file handle. */
    fdStealDigest(fd, dig);

    rc = RPMRC_OK;	/* XXX unnecessary */

exit:
    return rc;
}

int rpmVerifySignatures(QVA_t qva, rpmts ts, void * _fd, const char * fn)
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    HE_t she = memset(alloca(sizeof(*she)), 0, sizeof(*she));
/*@-castexpose@*/
    FD_t fd = (FD_t)_fd;
/*@=castexpose@*/
    char result[1024];
    char buf[8192], * b;
    char missingKeys[7164], * m;
    char untrustedKeys[7164], * u;
    pgpDig dig;
    pgpDigParams sigp;
    Header sigh = NULL;
    HeaderIterator hi = NULL;
    const char * msg = NULL;
    int res = 0;
    int xx;
    rpmRC rc, sigres;
    int failed;
    int nodigests = !(qva->qva_flags & VERIFY_DIGEST);
    int nosignatures = !(qva->qva_flags & VERIFY_SIGNATURE);

    {
	{   const char item[] = "Lead";
	    msg = NULL;
/*@-mods@*/	/* LCL: avoid void * _fd annotation for now. */
	    rc = rpmpkgRead(item, fd, NULL, &msg);
/*@=mods@*/
	    if (rc != RPMRC_OK) {
		rpmlog(RPMLOG_ERR, "%s: %s: %s\n", fn, item, msg);
		msg = _free(msg);
		res++;
		goto exit;
	    }
	    msg = _free(msg);
	}

	{   const char item[] = "Signature";
	    msg = NULL;
/*@-mods@*/	/* LCL: avoid void * _fd annotation for now. */
	    rc = rpmpkgRead(item, fd, &sigh, &msg);
/*@=mods@*/
	    switch (rc) {
	    default:
		rpmlog(RPMLOG_ERR, "%s: %s: %s\n", fn, item,
			(msg && *msg ? msg : ""));
		msg = _free(msg);
		res++;
		goto exit;
		/*@notreached@*/ /*@switchbreak@*/ break;
	    case RPMRC_OK:
		if (sigh == NULL) {
		    rpmlog(RPMLOG_ERR, _("%s: No signature available\n"), fn);
		    res++;
		    goto exit;
		}
		/*@switchbreak@*/ break;
	    }
	    msg = _free(msg);
	}

	/* Grab a hint of what needs doing to avoid duplication. */
	she->tag = 0;
	if (she->tag == 0 && !nosignatures) {
	    if (headerIsEntry(sigh, (rpmTag) RPMSIGTAG_DSA))
		she->tag = (rpmTag) RPMSIGTAG_DSA;
	    else if (headerIsEntry(sigh, (rpmTag) RPMSIGTAG_RSA))
		she->tag = (rpmTag) RPMSIGTAG_RSA;
	}
	if (she->tag == 0 && !nodigests) {
	    if (headerIsEntry(sigh, (rpmTag) RPMSIGTAG_MD5))
		she->tag = (rpmTag) RPMSIGTAG_MD5;
	    else if (headerIsEntry(sigh, (rpmTag) RPMSIGTAG_SHA1))
		she->tag = (rpmTag) RPMSIGTAG_SHA1;	/* XXX never happens */
	}

	dig = rpmtsDig(ts);
/*@-mods@*/	/* LCL: avoid void * _fd annotation for now. */
	(void) fdSetDig(fd, dig);
/*@=mods@*/
	sigp = pgpGetSignature(dig);

	/* XXX RSA needs the hash_algo, so decode early. */
	if ((rpmSigTag) she->tag == RPMSIGTAG_RSA) {
	    he->tag = she->tag;
	    xx = headerGet(sigh, he, 0);
	    xx = pgpPrtPkts(he->p.ptr, he->c, dig, 0);
	    he->p.ptr = _free(he->p.ptr);
	}

/*@-mods@*/	/* LCL: avoid void * _fd annotation for now. */
	if (headerIsEntry(sigh, (rpmTag)RPMSIGTAG_MD5))
	    fdInitDigest(fd, PGPHASHALGO_MD5, 0);
/*@=mods@*/

	/* Read the file, generating digest(s) on the fly. */
/*@-mods@*/	/* LCL: avoid void * _fd annotation for now. */
	if (dig == NULL || sigp == NULL
	 || readFile(fd, fn) != RPMRC_OK)
	{
	    res++;
	    goto exit;
	}
/*@=mods@*/

	failed = 0;
	b = buf;		*b = '\0';
	m = missingKeys;	*m = '\0';
	u = untrustedKeys;	*u = '\0';
	sprintf(b, "%s:%c", fn, (rpmIsVerbose() ? '\n' : ' ') );
	b += strlen(b);

	if (sigh != NULL)
	for (hi = headerInit(sigh);
	    headerNext(hi, she, 0) != 0;
	    she->p.ptr = _free(she->p.ptr))
	{

assert(she->p.ptr != NULL);

	    /* Clean up parameters from previous she->tag. */
	    pgpDigClean(dig);

/*@-ownedtrans -noeffect@*/
	    xx = pgpSetSig(dig, she->tag, she->t, she->p.ptr, she->c);
/*@=ownedtrans =noeffect@*/

	    switch ((rpmSigTag)she->tag) {
	    case RPMSIGTAG_RSA:
	    case RPMSIGTAG_DSA:
		if (nosignatures)
		     continue;
		xx = pgpPrtPkts(she->p.ptr, she->c, dig,
			(_print_pkts & rpmIsDebug()));

		if (sigp->version != 3 && sigp->version != 4) {
		    rpmlog(RPMLOG_ERR,
		_("skipping package %s with unverifiable V%u signature\n"),
			fn, sigp->version);
		    res++;
		    goto exit;
		}
		/*@switchbreak@*/ break;
	    case RPMSIGTAG_SHA1:
		if (nodigests)
		     continue;
		/* XXX Don't bother with header sha1 if header dsa. */
		if (!nosignatures && (rpmSigTag)she->tag == RPMSIGTAG_DSA)
		    continue;
		/*@switchbreak@*/ break;
	    case RPMSIGTAG_MD5:
		if (nodigests)
		     continue;
		/*@switchbreak@*/ break;
	    default:
		continue;
		/*@notreached@*/ /*@switchbreak@*/ break;
	    }

	    sigres = rpmVerifySignature(dig, result);

	    if (sigres) {
		failed = 1;
		if (rpmIsVerbose())
		    b = stpcpy( stpcpy( stpcpy(b, "    "), result), "\n");
		else
		switch ((rpmSigTag)she->tag) {
		case RPMSIGTAG_SIZE:
		    b = stpcpy(b, "SIZE ");
		    /*@switchbreak@*/ break;
		case RPMSIGTAG_SHA1:
		    b = stpcpy(b, "SHA1 ");
		    /*@switchbreak@*/ break;
		case RPMSIGTAG_MD5:
		    b = stpcpy(b, "MD5 ");
		    /*@switchbreak@*/ break;
		case RPMSIGTAG_RSA:
		    b = stpcpy(b, "RSA ");
		    /*@switchbreak@*/ break;
		case RPMSIGTAG_DSA:
		    b = stpcpy(b, "(SHA1) DSA ");
		    /*@switchbreak@*/ break;
		default:
		    b = stpcpy(b, "?UnknownSignatureType? ");
		    /*@switchbreak@*/ break;
		}
	    } else {
		if (rpmIsVerbose())
		    b = stpcpy( stpcpy( stpcpy(b, "    "), result), "\n");
		else
		switch ((rpmSigTag)she->tag) {
		case RPMSIGTAG_SIZE:
		    b = stpcpy(b, "size ");
		    /*@switchbreak@*/ break;
		case RPMSIGTAG_SHA1:
		    b = stpcpy(b, "sha1 ");
		    /*@switchbreak@*/ break;
		case RPMSIGTAG_MD5:
		    b = stpcpy(b, "md5 ");
		    /*@switchbreak@*/ break;
		case RPMSIGTAG_RSA:
		    b = stpcpy(b, "rsa ");
		    /*@switchbreak@*/ break;
		case RPMSIGTAG_DSA:
		    b = stpcpy(b, "(sha1) dsa ");
		    /*@switchbreak@*/ break;
		default:
		    b = stpcpy(b, "??? ");
		    /*@switchbreak@*/ break;
		}
	    }
	}
	hi = headerFini(hi);
	/* XXX clear the already free'd signature data. */
/*@-noeffect@*/
	xx = pgpSetSig(dig, 0, 0, NULL, 0);
/*@=noeffect@*/

	res += failed;

	if (failed) {
	    if (rpmIsVerbose()) {
		rpmlog(RPMLOG_NOTICE, "%s", buf);
	    } else {
		rpmlog(RPMLOG_NOTICE, "%s%s%s%s%s%s%s%s\n", buf,
			_("NOT_OK"),
			(missingKeys[0] != '\0') ? _(" (MISSING KEYS:") : "",
			missingKeys,
			(missingKeys[0] != '\0') ? _(") ") : "",
			(untrustedKeys[0] != '\0') ? _(" (UNTRUSTED KEYS:") : "",
			untrustedKeys,
			(untrustedKeys[0] != '\0') ? _(")") : "");

	    }
	} else {
	    if (rpmIsVerbose()) {
		rpmlog(RPMLOG_NOTICE, "%s", buf);
	    } else {
		rpmlog(RPMLOG_NOTICE, "%s%s%s%s%s%s%s%s\n", buf,
			_("OK"),
			(missingKeys[0] != '\0') ? _(" (MISSING KEYS:") : "",
			missingKeys,
			(missingKeys[0] != '\0') ? _(") ") : "",
			(untrustedKeys[0] != '\0') ? _(" (UNTRUSTED KEYS:") : "",
			untrustedKeys,
			(untrustedKeys[0] != '\0') ? _(")") : "");
	    }
	}

    }

exit:
    rpmtsCleanDig(ts);
    (void)headerFree(sigh);
    sigh = NULL;
    return res;
}

int rpmcliSign(rpmts ts, QVA_t qva, const char ** argv)
	/*@globals rpmioFtsOpts @*/
	/*@modifies rpmioFtsOpts @*/
{
    int res = 0;

    if (argv == NULL) return res;

    switch (qva->qva_mode) {
    case RPMSIGN_CHK_SIGNATURE:
	break;
    case RPMSIGN_IMPORT_PUBKEY:
	return rpmcliImportPubkeys(ts, qva, argv);
	/*@notreached@*/ break;
    case RPMSIGN_NEW_SIGNATURE:
    case RPMSIGN_ADD_SIGNATURE:
    case RPMSIGN_DEL_SIGNATURE:
	return rpmReSign(ts, qva, argv);
	/*@notreached@*/ break;
    case RPMSIGN_NONE:
    default:
	return -1;
	/*@notreached@*/ break;
    }

{       /* start-of-arg-iteration */

    int tag = (qva->qva_source == RPMQV_FTSWALK)
	? RPMDBI_FTSWALK : RPMDBI_ARGLIST;
    rpmgi gi = rpmgiNew(ts, tag, NULL, 0);
    rpmgiFlags _giFlags = RPMGI_NONE;
    rpmRC rc;

    if (rpmioFtsOpts == 0)
	rpmioFtsOpts = (FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOSTAT);
    rc = rpmgiSetArgs(gi, argv, rpmioFtsOpts, (_giFlags|RPMGI_NOHEADER));
    while (rpmgiNext(gi) == RPMRC_OK) {
	const char * fn = rpmgiHdrPath(gi);
	FD_t fd;
	int xx;

	fd = Fopen(fn, "r.fdio");
	if (fd == NULL || Ferror(fd)) {
	    rpmlog(RPMLOG_ERR, _("%s: open failed: %s\n"), 
		     fn, Fstrerror(fd));
	    res++;
	} else if (rpmVerifySignatures(qva, ts, fd, fn)) {
	    res++;
	}

	if (fd != NULL) {
	    xx = Fclose(fd);
	}
    }

    gi = rpmgiFree(gi);

}	/* end-of-arg-iteration */

    return res;
}

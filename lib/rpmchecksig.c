/** \ingroup rpmcli
 * \file lib/rpmchecksig.c
 * Verify the signature of a package.
 */

#include "system.h"

#include "rpmio_internal.h"
#include <rpmcli.h>
#define	_RPMEVR_INTERNAL	/* XXX RPMSENSE_KEYRING */
#include <rpmevr.h>

#include <rpmts.h>

#include "rpmdb.h"
#include "rpmgi.h"

#include <pkgio.h>
#include "signature.h"
#include "debug.h"

/*@access FD_t @*/		/* XXX stealing digests */
/*@access Header @*/		/* XXX void * arg */
/*@access pgpDig @*/
/*@access pgpDigParams @*/

/*@unchecked@*/
int _print_pkts = 0;

extern int _nolead;
extern int _nosigh;

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
	    rpmError(RPMERR_OPEN, _("%s: open failed: %s\n"), *fnp,
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
	    rpmError(RPMERR_MAKETEMP, _("rpmTempFile failed\n"));
	    return 1;
	}
	if (fnp != NULL)
	    *fnp = fn;
	*fdp = fdLink(fd, "manageFile return");
	fd = fdFree(fd, "manageFile return");
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
	if (Fwrite(buf, sizeof(buf[0]), count, *tfdp) != count) {
	    rpmError(RPMERR_FWRITE, _("%s: Fwrite failed: %s\n"), *tfnp,
		Fstrerror(*tfdp));
	    goto exit;
	}
    }
    if (count < 0) {
	rpmError(RPMERR_FREAD, _("%s: Fread failed: %s\n"), *sfnp, Fstrerror(*sfdp));
	goto exit;
    }
    if (Fflush(*tfdp) != 0) {
	rpmError(RPMERR_FWRITE, _("%s: Fflush failed: %s\n"), *tfnp,
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
 * @param sig		signature header
 * @param sigtag	signature tag
 * @retval signid	signer fingerprint
 * @return		0 on success
 */
static int getSignid(Header sig, int sigtag, unsigned char * signid)
	/*@globals fileSystem, internalState @*/
	/*@modifies *signid, fileSystem, internalState @*/
{
    void * pkt = NULL;
    int_32 pkttyp = 0;
    int_32 pktlen = 0;
    int rc = 1;

    if (headerGetEntry(sig, sigtag, &pkttyp, &pkt, &pktlen) && pkt != NULL) {
	pgpDig dig = pgpNewDig(0);

	if (!pgpPrtPkts(pkt, pktlen, dig, 0)) {
	    memcpy(signid, dig->signature.signid, sizeof(dig->signature.signid));
	    rc = 0;
	}
     
	dig = pgpFreeDig(dig);
    }
    pkt = headerFreeData(pkt, pkttyp);
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
    rpmgi gi = NULL;
    FD_t fd = NULL;
    FD_t ofd = NULL;
    struct rpmlead *lead = NULL;
    int_32 sigtag;
    const char *sigtarget = NULL;
    char tmprpm[1024+1];
    Header sigh = NULL;
    const char * msg = NULL;
    void * uh = NULL;
    int_32 uht, uhc;
    int res = EXIT_FAILURE;
    int deleting = (qva->qva_mode == RPMSIGN_DEL_SIGNATURE);
    rpmRC rc;
    int xx;
    
    tmprpm[0] = '\0';

    if (argv)
{       /* start-of-arg-iteration */
    int tag = (qva->qva_source == RPMQV_FTSWALK)
	? RPMDBI_FTSWALK : RPMDBI_ARGLIST;
    rpmgiFlags _giFlags = RPMGI_NONE;

    gi = rpmgiNew(ts, tag, NULL, 0);
/*@-mods@*/
    if (ftsOpts == 0)
	ftsOpts = (FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOSTAT);
/*@=mods@*/
    rc = rpmgiSetArgs(gi, argv, ftsOpts, (_giFlags|RPMGI_NOHEADER));

    while (rpmgiNext(gi) == RPMRC_OK) {
	const char * fn = rpmgiHdrPath(gi);
	const char * tfn;

	fprintf(stdout, "%s:\n", fn);

/*@-modobserver@*/	/* XXX rpmgiHdrPath should not be observer */
	if (manageFile(&fd, &fn, O_RDONLY, 0))
	    goto exit;
/*@=modobserver@*/

if (!_nolead) {
	const char item[] = "Lead";
	msg = NULL;
	rc = rpmpkgRead(item, fd, &lead, &msg);
	if (rc != RPMRC_OK) {
	    rpmError(RPMERR_READLEAD, "%s: %s: %s\n", fn, item, msg);
	    msg = _free(msg);
	    goto exit;
	}
	msg = _free(msg);
}

if (!_nosigh) {
	const char item[] = "Signature";
	msg = NULL;
	rc = rpmpkgRead(item, fd, &sigh, &msg);
	switch (rc) {
	default:
	    rpmError(RPMERR_SIGGEN, "%s: %s: %s", fn, item,
			(msg && *msg ? msg : "\n"));
	    msg = _free(msg);
	    goto exit;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	case RPMRC_OK:
	    if (sigh == NULL) {
		rpmError(RPMERR_SIGGEN, _("%s: No signature available\n"), fn);
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

	/* Dump the immutable region (if present). */
	if (headerGetEntry(sigh, RPMTAG_HEADERSIGNATURES, &uht, &uh, &uhc)) {
	    HeaderIterator hi;
	    int_32 htag, type, count;
	    hPTR_t ptr;
	    Header oh;
	    Header nh;

	    nh = headerNew();
	    if (nh == NULL) {
		uh = headerFreeData(uh, uht);
		goto exit;
	    }

	    oh = headerCopyLoad(uh);
	    for (hi = headerInitIterator(oh);
		headerNextIterator(hi, &htag, &type, &ptr, &count);
		ptr = headerFreeData(ptr, type))
	    {
		if (ptr)
		    xx = headerAddEntry(nh, htag, type, ptr, count);
	    }
	    hi = headerFreeIterator(hi);
	    oh = headerFree(oh);

	    sigh = headerFree(sigh);
	    sigh = headerLink(nh);
	    nh = headerFree(nh);
	}

if (sigh != NULL) {
	/* Eliminate broken digest values. */
	xx = headerRemoveEntry(sigh, RPMSIGTAG_LEMD5_1);
	xx = headerRemoveEntry(sigh, RPMSIGTAG_LEMD5_2);
	xx = headerRemoveEntry(sigh, RPMSIGTAG_BADSHA1_1);
	xx = headerRemoveEntry(sigh, RPMSIGTAG_BADSHA1_2);

	/* Toss and recalculate header+payload size and digests. */
	xx = headerRemoveEntry(sigh, RPMSIGTAG_SIZE);
	xx = rpmAddSignature(sigh, sigtarget, RPMSIGTAG_SIZE, qva->passPhrase);
	xx = headerRemoveEntry(sigh, RPMSIGTAG_MD5);
	xx = rpmAddSignature(sigh, sigtarget, RPMSIGTAG_MD5, qva->passPhrase);
	xx = headerRemoveEntry(sigh, RPMSIGTAG_SHA1);
	xx = rpmAddSignature(sigh, sigtarget, RPMSIGTAG_SHA1, qva->passPhrase);

	if (deleting) {	/* Nuke all the signature tags. */
	    xx = headerRemoveEntry(sigh, RPMSIGTAG_GPG);
	    xx = headerRemoveEntry(sigh, RPMSIGTAG_PGP5);
	    xx = headerRemoveEntry(sigh, RPMSIGTAG_PGP);
	    xx = headerRemoveEntry(sigh, RPMSIGTAG_DSA);
	    xx = headerRemoveEntry(sigh, RPMSIGTAG_RSA);
	} else {		/* If gpg/pgp is configured, replace the signature. */
	  int addsig = 0;
#if defined(SUPPORT_PGP_SIGNING)
	  sigtag = rpmLookupSignatureType(RPMLOOKUPSIG_QUERY);
	  addsig = (sigtag > 0);
#else
	  sigtag = RPMSIGTAG_GPG;
	  addsig = 1;
#endif
	  if (addsig) {
	    unsigned char oldsignid[8], newsignid[8];

	    /* Grab the old signature fingerprint (if any) */
	    memset(oldsignid, 0, sizeof(oldsignid));
	    xx = getSignid(sigh, sigtag, oldsignid);

	    switch (sigtag) {
	    case RPMSIGTAG_DSA:
		xx = headerRemoveEntry(sigh, RPMSIGTAG_GPG);
		/*@switchbreak@*/ break;
	    case RPMSIGTAG_RSA:
		xx = headerRemoveEntry(sigh, RPMSIGTAG_PGP);
		/*@switchbreak@*/ break;
	    case RPMSIGTAG_GPG:
		xx = headerRemoveEntry(sigh, RPMSIGTAG_DSA);
		/*@fallthrough@*/
	    case RPMSIGTAG_PGP5:
	    case RPMSIGTAG_PGP:
		xx = headerRemoveEntry(sigh, RPMSIGTAG_RSA);
		/*@switchbreak@*/ break;
	    }

	    xx = headerRemoveEntry(sigh, sigtag);
	    xx = rpmAddSignature(sigh, sigtarget, sigtag, qva->passPhrase);

	    /* If package was previously signed, check for same signer. */
	    memset(newsignid, 0, sizeof(newsignid));
	    if (memcmp(oldsignid, newsignid, sizeof(oldsignid))) {

		/* Grab the new signature fingerprint */
		xx = getSignid(sigh, sigtag, newsignid);

		/* If same signer, skip resigning the package. */
		if (!memcmp(oldsignid, newsignid, sizeof(oldsignid))) {

		    rpmMessage(RPMMESS_WARNING,
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

if (!_nolead) {
	const char item[] = "Lead";
	rc = rpmpkgWrite(item, ofd, lead, NULL);
	if (rc != RPMRC_OK) {
	    rpmError(RPMERR_WRITELEAD, "%s: %s: %s\n", tfn, item,
		Fstrerror(ofd));
	    goto exit;
	}
}

if (!_nosigh) {
	const char item[] = "Signature";
	rc = rpmpkgWrite(item, ofd, sigh, NULL);
	if (rc != RPMRC_OK) {
	    rpmError(RPMERR_SIGGEN, "%s: %s: %s\n", tfn, item,
		Fstrerror(ofd));
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
    sigh = headerFree(sigh);

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
    static unsigned char zeros[] =
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    const char * afmt = "%{pubkeys:armor}";
    const char * group = "Public Keys";
    const char * license = "pubkey";
    const char * buildhost = "localhost";
    int_32 pflags = (RPMSENSE_KEYRING|RPMSENSE_EQUAL);
    int_32 zero = 0;
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

    dig = pgpNewDig(0);

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

    xx = headerAddOrAppendEntry(h, RPMTAG_PUBKEYS,
			RPM_STRING_ARRAY_TYPE, &enc, 1);

    d = headerSprintf(h, afmt, rpmTagTable, rpmHeaderFormats, NULL);
    if (d == NULL)
	goto exit;

    xx = headerAddEntry(h, RPMTAG_NAME, RPM_STRING_TYPE, "gpg-pubkey", 1);
    xx = headerAddEntry(h, RPMTAG_VERSION, RPM_STRING_TYPE, v+8, 1);
    xx = headerAddEntry(h, RPMTAG_RELEASE, RPM_STRING_TYPE, r, 1);
    xx = headerAddEntry(h, RPMTAG_DESCRIPTION, RPM_STRING_TYPE, d, 1);
    xx = headerAddEntry(h, RPMTAG_GROUP, RPM_STRING_TYPE, group, 1);
    xx = headerAddEntry(h, RPMTAG_LICENSE, RPM_STRING_TYPE, license, 1);
    xx = headerAddEntry(h, RPMTAG_SUMMARY, RPM_STRING_TYPE, u, 1);

    xx = headerAddEntry(h, RPMTAG_SIZE, RPM_INT32_TYPE, &zero, 1);

    xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDENAME,
			RPM_STRING_ARRAY_TYPE, &u, 1);
    xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEVERSION,
			RPM_STRING_ARRAY_TYPE, &evr, 1);
    xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEFLAGS,
			RPM_INT32_TYPE, &pflags, 1);

    xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDENAME,
			RPM_STRING_ARRAY_TYPE, &n, 1);
    xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEVERSION,
			RPM_STRING_ARRAY_TYPE, &evr, 1);
    xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEFLAGS,
			RPM_INT32_TYPE, &pflags, 1);

    xx = headerAddEntry(h, RPMTAG_RPMVERSION, RPM_STRING_TYPE, RPMVERSION, 1);

    /* XXX W2DO: tag value inheirited from parent? */
    xx = headerAddEntry(h, RPMTAG_BUILDHOST, RPM_STRING_TYPE, buildhost, 1);
    {   int_32 tid = rpmtsGetTid(ts);
	xx = headerAddEntry(h, RPMTAG_INSTALLTIME, RPM_INT32_TYPE, &tid, 1);
	/* XXX W2DO: tag value inheirited from parent? */
	xx = headerAddEntry(h, RPMTAG_BUILDTIME, RPM_INT32_TYPE, &tid, 1);
    }

#ifdef	NOTYET
    /* XXX W2DO: tag value inheirited from parent? */
    xx = headerAddEntry(h, RPMTAG_SOURCERPM, RPM_STRING_TYPE, fn, 1);
#endif

    /* Add header to database. */
    xx = rpmdbAdd(rpmtsGetRdb(ts), rpmtsGetTid(ts), h, NULL);
    if (xx != 0)
	goto exit;
    rc = RPMRC_OK;

exit:
    /* Clean up. */
    h = headerFree(h);
    dig = pgpFreeDig(dig);
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
	/*@globals RPMVERSION, rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies ts, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    const char * fn;
    const unsigned char * pkt = NULL;
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
	    rpmError(RPMERR_IMPORT, _("%s: import read failed(%d).\n"), fn, rc);
	    res++;
	    continue;
	}
	if (rc != PGPARMOR_PUBKEY) {
	    rpmError(RPMERR_IMPORT, _("%s: not an armored public key.\n"), fn);
	    res++;
	    continue;
	}

	/* Import pubkey packet(s). */
	if ((rpmrc = rpmcliImportPubkey(ts, pkt, pktlen)) != RPMRC_OK) {
	    rpmError(RPMERR_IMPORT, _("%s: import failed.\n"), fn);
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
static int readFile(FD_t fd, const char * fn, pgpDig dig)
	/*@globals fileSystem, internalState @*/
	/*@modifies fd, *dig, fileSystem, internalState @*/
{
    unsigned char buf[4*BUFSIZ];
    ssize_t count;
    int rc = 1;
    int i;

    dig->nbytes = 0;

    /* Read the header from the package. */
    {	Header h = headerRead(fd);
	if (h == NULL) {
	    rpmError(RPMERR_FREAD, _("%s: headerRead failed\n"), fn);
	    goto exit;
	}

	dig->nbytes += headerSizeof(h);

	if (headerIsEntry(h, RPMTAG_HEADERIMMUTABLE)) {
	    unsigned char * hmagic = NULL;
	    size_t nmagic = 0;
	    void * uh;
	    int_32 uht, uhc;
	
	    if (!headerGetEntry(h, RPMTAG_HEADERIMMUTABLE, &uht, &uh, &uhc)
	    ||   uh == NULL)
	    {
		h = headerFree(h);
		rpmError(RPMERR_FREAD, _("%s: headerGetEntry failed\n"), fn);
		goto exit;
	    }
	    (void) headerGetMagic(NULL, &hmagic, &nmagic);
	    dig->hdrsha1ctx = rpmDigestInit(PGPHASHALGO_SHA1, RPMDIGEST_NONE);
	    if (hmagic && nmagic > 0)
		(void) rpmDigestUpdate(dig->hdrsha1ctx, hmagic, nmagic);
	    (void) rpmDigestUpdate(dig->hdrsha1ctx, uh, uhc);
	    dig->hdrmd5ctx = rpmDigestInit(dig->signature.hash_algo, RPMDIGEST_NONE);
	    if (hmagic && nmagic > 0)
		(void) rpmDigestUpdate(dig->hdrmd5ctx, hmagic, nmagic);
	    (void) rpmDigestUpdate(dig->hdrmd5ctx, uh, uhc);
	    uh = headerFreeData(uh, uht);
	}
	h = headerFree(h);
    }

    /* Read the payload from the package. */
    while ((count = Fread(buf, sizeof(buf[0]), sizeof(buf), fd)) > 0)
	dig->nbytes += count;
    if (count < 0) {
	rpmError(RPMERR_FREAD, _("%s: Fread failed: %s\n"), fn, Fstrerror(fd));
	goto exit;
    }

    /* XXX Steal the digest-in-progress from the file handle. */
    for (i = fd->ndigests - 1; i >= 0; i--) {
	FDDIGEST_t fddig = fd->digests + i;
	if (fddig->hashctx != NULL)
	switch (fddig->hashalgo) {
	case PGPHASHALGO_MD5:
assert(dig->md5ctx == NULL);
	    dig->md5ctx = fddig->hashctx;
	    fddig->hashctx = NULL;
	    /*@switchbreak@*/ break;
	case PGPHASHALGO_SHA1:
	case PGPHASHALGO_RIPEMD160:
#if defined(HAVE_BEECRYPT_API_H)
	case PGPHASHALGO_SHA256:
	case PGPHASHALGO_SHA384:
	case PGPHASHALGO_SHA512:
#endif
assert(dig->sha1ctx == NULL);
	    dig->sha1ctx = fddig->hashctx;
	    fddig->hashctx = NULL;
	    /*@switchbreak@*/ break;
	default:
	    /*@switchbreak@*/ break;
	}
    }

    rc = 0;

exit:
    return rc;
}

int rpmVerifySignatures(QVA_t qva, rpmts ts, FD_t fd,
		const char * fn)
{
    int res2, res3;
    char result[1024];
    char buf[8192], * b;
    char missingKeys[7164], * m;
    char untrustedKeys[7164], * u;
    int_32 sigtag;
    int_32 sigtype;
    const void * sig;
    pgpDig dig;
    pgpDigParams sigp;
    int_32 siglen;
    Header sigh = NULL;
    HeaderIterator hi = NULL;
    const char * msg = NULL;
    int res = 0;
    int xx;
    rpmRC rc;
    int nodigests = !(qva->qva_flags & VERIFY_DIGEST);
    int nosignatures = !(qva->qva_flags & VERIFY_SIGNATURE);

    {

if (!_nolead) {
	const char item[] = "Lead";
	msg = NULL;
	rc = rpmpkgRead(item, fd, NULL, &msg);
	if (rc != RPMRC_OK) {
	    rpmError(RPMERR_READLEAD, "%s: %s: %s\n", fn, item, msg);
	    msg = _free(msg);
	    res++;
	    goto exit;
	}
}

if (!_nosigh) {
	const char item[] = "Signature";
	msg = NULL;
	rc = rpmpkgRead(item, fd, &sigh, &msg);
	switch (rc) {
	default:
	    rpmError(RPMERR_SIGGEN, "%s: %s: %s", fn, item,
			(msg && *msg ? msg : "\n"));
	    msg = _free(msg);
	    res++;
	    goto exit;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	case RPMRC_OK:
	    if (sigh == NULL) {
		rpmError(RPMERR_SIGGEN, _("%s: No signature available\n"), fn);
		res++;
		goto exit;
	    }
	    /*@switchbreak@*/ break;
	}
	msg = _free(msg);
} else {
nosignatures = 1;
nodigests = 1;
}

	/* Grab a hint of what needs doing to avoid duplication. */
	sigtag = 0;
	if (sigtag == 0 && !nosignatures) {
	    if (headerIsEntry(sigh, RPMSIGTAG_DSA))
		sigtag = RPMSIGTAG_DSA;
	    else if (headerIsEntry(sigh, RPMSIGTAG_RSA))
		sigtag = RPMSIGTAG_RSA;
#if defined(SUPPORT_RPMV3_VERIFY_DSA)
	    else if (headerIsEntry(sigh, RPMSIGTAG_GPG))
		sigtag = RPMSIGTAG_GPG;
#endif
#if defined(SUPPORT_RPMV3_VERIFY_RSA)
	    else if (headerIsEntry(sigh, RPMSIGTAG_PGP))
		sigtag = RPMSIGTAG_PGP;
#endif
	}
	if (sigtag == 0 && !nodigests) {
	    if (headerIsEntry(sigh, RPMSIGTAG_MD5))
		sigtag = RPMSIGTAG_MD5;
	    else if (headerIsEntry(sigh, RPMSIGTAG_SHA1))
		sigtag = RPMSIGTAG_SHA1;	/* XXX never happens */
	}

	dig = rpmtsDig(ts);
assert(dig != NULL);
	sigp = pgpGetSignature(dig);

	/* XXX RSA needs the hash_algo, so decode early. */
	if (sigtag == RPMSIGTAG_RSA
#if defined(SUPPORT_RPMV3_VERIFY_RSA)
	 || sigtag == RPMSIGTAG_PGP
	 || sigtag == RPMSIGTAG_PGP5
#endif
	) {
	    xx = headerGetEntry(sigh, sigtag, &sigtype, (void **)&sig, &siglen);
	    xx = pgpPrtPkts(sig, siglen, dig, 0);
	    sig = headerFreeData(sig, sigtype);
#if defined(SUPPORT_RPMV3_VERIFY_RSA)
	    /* XXX assume same hash_algo in header-only and header+payload */
	    if ((headerIsEntry(sigh, RPMSIGTAG_PGP)
	      || headerIsEntry(sigh, RPMSIGTAG_PGP5))
	     && dig->signature.hash_algo != PGPHASHALGO_MD5)
		fdInitDigest(fd, dig->signature.hash_algo, 0);
#endif
	}

	if (headerIsEntry(sigh, RPMSIGTAG_MD5)
#if defined(SUPPORT_RPMV3_VERIFY_RSA)
	 || headerIsEntry(sigh, RPMSIGTAG_PGP)
	 || headerIsEntry(sigh, RPMSIGTAG_PGP5)
#endif
	)
	    fdInitDigest(fd, PGPHASHALGO_MD5, 0);
#if defined(SUPPORT_RPMV3_VERIFY_DSA)
	if (headerIsEntry(sigh, RPMSIGTAG_GPG))
	    fdInitDigest(fd, PGPHASHALGO_SHA1, 0);
#endif

	/* Read the file, generating digest(s) on the fly. */
	if (dig == NULL || sigp == NULL || readFile(fd, fn, dig)) {
	    res++;
	    goto exit;
	}

	res2 = 0;
	b = buf;		*b = '\0';
	m = missingKeys;	*m = '\0';
	u = untrustedKeys;	*u = '\0';
	sprintf(b, "%s:%c", fn, (rpmIsVerbose() ? '\n' : ' ') );
	b += strlen(b);

	if (sigh != NULL)
	for (hi = headerInitIterator(sigh);
	    headerNextIterator(hi, &sigtag, &sigtype, &sig, &siglen) != 0;
	    /*@-noeffect@*/(void) rpmtsSetSig(ts, sigtag, sigtype, NULL, siglen)/*@=noeffect@*/)
	{

	    if (sig == NULL) /* XXX can't happen */
		continue;

/*@-noeffect@*/
	    (void) rpmtsSetSig(ts, sigtag, sigtype, sig, siglen);
/*@=noeffect@*/

	    /* Clean up parameters from previous sigtag. */
	    pgpCleanDig(dig);

	    switch (sigtag) {
	    case RPMSIGTAG_RSA:
	    case RPMSIGTAG_DSA:
#if defined(SUPPORT_RPMV3_VERIFY_DSA)
	    case RPMSIGTAG_GPG:
#endif
#if defined(SUPPORT_RPMV3_VERIFY_RSA)
	    case RPMSIGTAG_PGP5:	/* XXX legacy */
	    case RPMSIGTAG_PGP:
#endif
		if (nosignatures)
		     continue;
		xx = pgpPrtPkts(sig, siglen, dig,
			(_print_pkts & rpmIsDebug()));

		if (sigp->version != 3 && sigp->version != 4) {
		    rpmError(RPMERR_SIGVFY,
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
		if (!nosignatures && sigtag == RPMSIGTAG_DSA)
		    continue;
		/*@switchbreak@*/ break;
#if defined(SUPPORT_RPMV3_BROKEN)
	    case RPMSIGTAG_LEMD5_2:
	    case RPMSIGTAG_LEMD5_1:
#endif
	    case RPMSIGTAG_MD5:
		if (nodigests)
		     continue;
#if defined(SUPPORT_RPMV3_VERIFY_RSA)
		/*
		 * Don't bother with md5 if pgp, as RSA/MD5 is more reliable
		 * than the -- now unsupported -- legacy md5 breakage.
		 */
		if (!nosignatures && sigtag == RPMSIGTAG_PGP)
		    continue;
#endif
		/*@switchbreak@*/ break;
	    default:
		continue;
		/*@notreached@*/ /*@switchbreak@*/ break;
	    }

	    res3 = rpmVerifySignature(dig, result);

	    if (res3) {
		if (rpmIsVerbose()) {
		    b = stpcpy(b, "    ");
		    b = stpcpy(b, result);
		    res2 = 1;
		} else {
#if defined(SUPPORT_RPMV3_VERIFY_RSA) || defined(SUPPORT_RPMV3_VERIFY_DSA)
		    char *tempKey;
#endif
		    switch (sigtag) {
		    case RPMSIGTAG_SIZE:
			b = stpcpy(b, "SIZE ");
			res2 = 1;
			/*@switchbreak@*/ break;
		    case RPMSIGTAG_SHA1:
			b = stpcpy(b, "SHA1 ");
			res2 = 1;
			/*@switchbreak@*/ break;
#if defined(SUPPORT_RPMV3_BROKEN)
		    case RPMSIGTAG_LEMD5_2:
		    case RPMSIGTAG_LEMD5_1:
#endif
		    case RPMSIGTAG_MD5:
			b = stpcpy(b, "MD5 ");
			res2 = 1;
			/*@switchbreak@*/ break;
		    case RPMSIGTAG_RSA:
			b = stpcpy(b, "RSA ");
			res2 = 1;
			/*@switchbreak@*/ break;
#if defined(SUPPORT_RPMV3_VERIFY_RSA)
		    case RPMSIGTAG_PGP5:	/* XXX legacy */
		    case RPMSIGTAG_PGP:
			switch (res3) {
			case RPMRC_NOKEY:
			    res2 = 1;
			    /*@fallthrough@*/
			case RPMRC_NOTTRUSTED:
			{   int offset = 6;
			    b = stpcpy(b, "(MD5) (PGP) ");
			    tempKey = strstr(result, "ey ID");
			    if (tempKey == NULL) {
			        tempKey = strstr(result, "keyid:");
				offset = 9;
			    }
			    if (tempKey) {
			      if (res3 == RPMRC_NOKEY) {
				m = stpcpy(m, " PGP#");
				m = stpncpy(m, tempKey + offset, 8);
				*m = '\0';
			      } else {
			        u = stpcpy(u, " PGP#");
				u = stpncpy(u, tempKey + offset, 8);
				*u = '\0';
			      }
			    }
			}   /*@innerbreak@*/ break;
			default:
			    b = stpcpy(b, "MD5 PGP ");
			    res2 = 1;
			    /*@innerbreak@*/ break;
			}
			/*@switchbreak@*/ break;
#endif
		    case RPMSIGTAG_DSA:
			b = stpcpy(b, "(SHA1) DSA ");
			res2 = 1;
			/*@switchbreak@*/ break;
#if defined(SUPPORT_RPMV3_VERIFY_DSA)
		    case RPMSIGTAG_GPG:
			/* Do not consider this a failure */
			switch (res3) {
			case RPMRC_NOKEY:
			    b = stpcpy(b, "(GPG) ");
			    m = stpcpy(m, " GPG#");
			    tempKey = strstr(result, "ey ID");
			    if (tempKey) {
				m = stpncpy(m, tempKey+6, 8);
				*m = '\0';
			    }
			    res2 = 1;
			    /*@innerbreak@*/ break;
			default:
			    b = stpcpy(b, "GPG ");
			    res2 = 1;
			    /*@innerbreak@*/ break;
			}
			/*@switchbreak@*/ break;
#endif
		    default:
			b = stpcpy(b, "?UnknownSignatureType? ");
			res2 = 1;
			/*@switchbreak@*/ break;
		    }
		}
	    } else {
		if (rpmIsVerbose()) {
		    b = stpcpy(b, "    ");
		    b = stpcpy(b, result);
		} else {
		    switch (sigtag) {
		    case RPMSIGTAG_SIZE:
			b = stpcpy(b, "size ");
			/*@switchbreak@*/ break;
		    case RPMSIGTAG_SHA1:
			b = stpcpy(b, "sha1 ");
			/*@switchbreak@*/ break;
#if defined(SUPPORT_RPMV3_BROKEN)
		    case RPMSIGTAG_LEMD5_2:
		    case RPMSIGTAG_LEMD5_1:
#endif
		    case RPMSIGTAG_MD5:
			b = stpcpy(b, "md5 ");
			/*@switchbreak@*/ break;
		    case RPMSIGTAG_RSA:
			b = stpcpy(b, "rsa ");
			/*@switchbreak@*/ break;
#if defined(SUPPORT_RPMV3_VERIFY_RSA)
		    case RPMSIGTAG_PGP5:	/* XXX legacy */
		    case RPMSIGTAG_PGP:
			b = stpcpy(b, "(md5) pgp ");
			/*@switchbreak@*/ break;
#endif
		    case RPMSIGTAG_DSA:
			b = stpcpy(b, "(sha1) dsa ");
			/*@switchbreak@*/ break;
#if defined(SUPPORT_RPMV3_VERIFY_DSA)
		    case RPMSIGTAG_GPG:
			b = stpcpy(b, "gpg ");
			/*@switchbreak@*/ break;
#endif
		    default:
			b = stpcpy(b, "??? ");
			/*@switchbreak@*/ break;
		    }
		}
	    }
	}
	if (hi != NULL)
	    hi = headerFreeIterator(hi);

	res += res2;

	if (res2) {
	    if (rpmIsVerbose()) {
		rpmError(RPMERR_SIGVFY, "%s", buf);
	    } else {
		rpmError(RPMERR_SIGVFY, "%s%s%s%s%s%s%s%s\n", buf,
			_("NOT OK"),
			(missingKeys[0] != '\0') ? _(" (MISSING KEYS:") : "",
			missingKeys,
			(missingKeys[0] != '\0') ? _(") ") : "",
			(untrustedKeys[0] != '\0') ? _(" (UNTRUSTED KEYS:") : "",
			untrustedKeys,
			(untrustedKeys[0] != '\0') ? _(")") : "");

	    }
	} else {
	    if (rpmIsVerbose()) {
		rpmError(RPMERR_SIGVFY, "%s", buf);
	    } else {
		rpmError(RPMERR_SIGVFY, "%s%s%s%s%s%s%s%s\n", buf,
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
    sigh = headerFree(sigh);
    return res;
}

int rpmcliSign(rpmts ts, QVA_t qva, const char ** argv)
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

/*@-mods@*/
    if (ftsOpts == 0)
	ftsOpts = (FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOSTAT);
/*@=mods@*/
    rc = rpmgiSetArgs(gi, argv, ftsOpts, (_giFlags|RPMGI_NOHEADER));
    while (rpmgiNext(gi) == RPMRC_OK) {
	const char * fn = rpmgiHdrPath(gi);
	FD_t fd;
	int xx;

	fd = Fopen(fn, "r.fdio");
	if (fd == NULL || Ferror(fd)) {
	    rpmError(RPMERR_OPEN, _("%s: open failed: %s\n"), 
		     fn, Fstrerror(fd));
	    res++;
	} else if (rpmVerifySignatures(qva, ts, fd, fn)) {
	    res++;
	}

	if (fd != NULL) xx = Fclose(fd);
    }

    gi = rpmgiFree(gi);

}	/* end-of-arg-iteration */

    return res;
}

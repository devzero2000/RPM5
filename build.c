/** \ingroup rpmcli
 * Parse spec file and build package.
 */

#include "system.h"

#include <rpmio.h>
#include <rpmcli.h>
#include <rpmbuild.h>

#include "rpmps.h"
#include "rpmte.h"
#include "rpmts.h"

#include "build.h"
#include "debug.h"

/*@access rpmts @*/	/* XXX compared with NULL @*/
/*@access rpmdb @*/		/* XXX compared with NULL @*/
/*@access FD_t @*/		/* XXX compared with NULL @*/

/**
 */
static int checkSpec(rpmts ts, Header h)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, h, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    rpmps ps;
    int rc;

    if (!headerIsEntry(h, RPMTAG_REQUIRENAME)
     && !headerIsEntry(h, RPMTAG_CONFLICTNAME))
	return 0;

    rc = rpmtsAddInstallElement(ts, h, NULL, 0, NULL);

    rc = rpmtsCheck(ts);

    ps = rpmtsProblems(ts);
    if (rc == 0 && rpmpsNumProblems(ps) > 0) {
	rpmlog(RPMLOG_ERR, _("Failed build dependencies:\n"));
	rpmpsPrint(NULL, ps);
	rc = 1;
    }
    ps = rpmpsFree(ps);

    /* XXX nuke the added package. */
    rpmtsClean(ts);

    return rc;
}

/*
 * Kurwa, durni ameryka?ce sobe zawsze my?l?, ?e ca?y ?wiat mówi po
 * angielsku...
 */
/* XXX this is still a dumb test but at least it's i18n aware */
/**
 */
static int isSpecFile(const char * specfile)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    char buf[256];
    const char * s;
    FD_t fd;
    int count;
    int checking;

    fd = Fopen(specfile, "r");
    if (fd == NULL || Ferror(fd)) {
	rpmlog(RPMLOG_ERR, _("Unable to open spec file %s: %s\n"),
		specfile, Fstrerror(fd));
	return 0;
    }
    count = Fread(buf, sizeof(buf[0]), sizeof(buf), fd);
    (void) Fclose(fd);

    checking = 1;
    for (s = buf; count--; s++) {
	switch (*s) {
	case '\r':
	case '\n':
	    checking = 1;
	    /*@switchbreak@*/ break;
	case ':':
	    checking = 0;
	    /*@switchbreak@*/ break;
/*@-boundsread@*/
	default:
	    if (checking && !(isprint(*s) || isspace(*s))) return 0;
	    /*@switchbreak@*/ break;
/*@=boundsread@*/
	}
    }
    return 1;
}

/**
 */
/*@-boundswrite@*/
static int buildForTarget(rpmts ts, const char * arg, BTA_t ba)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    const char * passPhrase = ba->passPhrase;
    const char * cookie = ba->cookie;
    int buildAmount = ba->buildAmount;
    const char * specFile = NULL;
    const char * specURL = NULL;
    int specut;
    const char * s;
    char * se;
    size_t nb = strlen(arg) + BUFSIZ;
    char * buf = alloca(nb);
    Spec spec = NULL;
    int verify = 1;
    int xx;
    int rc;

#ifndef	DYING
    rpmSetTables(RPM_MACHTABLE_BUILDARCH, RPM_MACHTABLE_BUILDOS);
#endif

    if (ba->buildMode == 't') {
	static const char * sfpats[] = { "Specfile", "\\*.spec", NULL };
	static const char _specfn[] = "%{mkstemp:%{_specdir}/rpm-spec.XXXXXX}";
	char * tmpSpecFile = (char *) rpmGetPath(_specfn, NULL);
	FILE *fp;
	int bingo = 0;
	int i;

	for (i = 0; sfpats[i]; i++) {
	    se = rpmExpand("%{uncompress: %{u2p:", arg, "}}",
		" | %{__tar} -xOvf - %{?__tar_wildcards} ", sfpats[i],
		" 2>&1 > '", tmpSpecFile, "'", NULL);
	    fp = popen(se, "r");
	    se = _free(se);
	    if (fp== NULL)
		continue;
	    s = fgets(buf, nb - 1, fp);
	    xx = pclose(fp);
	    if (!s || !*s || strstr(s, ": Not found in archive"))
		continue;
	    bingo = 1;
	    break;
	}
	if (!bingo) {
	    rpmlog(RPMLOG_ERR, _("Failed to read spec file from %s\n"), arg);
	    xx = Unlink(tmpSpecFile);
	    tmpSpecFile = _free(tmpSpecFile);
	    return 1;
	}

	s = se = basename(buf);
	se += strlen(se);
	while (--se > s && strchr("\r\n", *se) != NULL)
	    *se = '\0';
	specURL = rpmGetPath("%{_specdir}/", s, NULL);
	specut = urlPath(specURL, &specFile);
	xx = Rename(tmpSpecFile, specFile);
	if (xx) {
	    rpmlog(RPMLOG_ERR, _("Failed to rename %s to %s: %m\n"),
			tmpSpecFile, s);
	    (void) Unlink(tmpSpecFile);
	}
	tmpSpecFile = _free(tmpSpecFile);
	if (xx)
	    return 1;

	se = buf; *se = '\0';
	se = stpcpy(se, "_sourcedir ");
	(void) urlPath(arg, &s);
	if (*s != '/') {
	    if (getcwd(se, nb - sizeof("_sourcedir ")) > 0)
		se += strlen(se);
	    else
		se = stpcpy(se, ".");
	} else
	    se = stpcpy(se, dirname(strcpy(se, s)));
	while (se > buf && se[-1] == '/')
	    *se-- = '0';
	rpmCleanPath(buf + sizeof("_sourcedir ") - 1);
	rpmDefineMacro(NULL, buf, RMIL_TARBALL);
    } else {
	specut = urlPath(arg, &s);
	se = buf; *se = '\0';
	if (*s != '/') {
	    if (getcwd(se, nb - sizeof("_sourcedir ")) > 0)
		se += strlen(se);
	    else
		se = stpcpy(se, ".");
	    *se++ = '/';
	    se += strlen(strcpy(se,strcpy(se, s)));
	} else
	    se = stpcpy(se, s);
	specURL = rpmGetPath(buf, NULL);
	specut = urlPath(specURL, &specFile);
    }

    if (specut != URL_IS_DASH) {
	struct stat sb;
	if (Stat(specURL, &sb) < 0) {
	    rpmlog(RPMLOG_ERR, _("failed to stat %s: %m\n"), specURL);
	    rc = 1;
	    goto exit;
	}
	if (! S_ISREG(sb.st_mode)) {
	    rpmlog(RPMLOG_ERR, _("File %s is not a regular file.\n"),
		specURL);
	    rc = 1;
	    goto exit;
	}

	/* Try to verify that the file is actually a specfile */
	if (!isSpecFile(specURL)) {
	    rpmlog(RPMLOG_ERR,
		_("File %s does not appear to be a specfile.\n"), specURL);
	    rc = 1;
	    goto exit;
	}
    }
    
    /* Parse the spec file */
#define	_anyarch(_f)	\
(((_f)&(RPMBUILD_PREP|RPMBUILD_BUILD|RPMBUILD_INSTALL|RPMBUILD_PACKAGEBINARY)) == 0)
    if (parseSpec(ts, specURL, ba->rootdir, 0, passPhrase,
		cookie, _anyarch(buildAmount), 0, verify))
    {
	rc = 1;
	goto exit;
    }
#undef	_anyarch
    if ((spec = rpmtsSetSpec(ts, NULL)) == NULL) {
	rc = 1;
	goto exit;
    }

    /* Assemble source header from parsed components */
    xx = initSourceHeader(spec, NULL);

    /* Check build prerequisites */
    if (!ba->noDeps && checkSpec(ts, spec->sourceHeader)) {
	rc = 1;
	goto exit;
    }

    if (buildSpec(ts, spec, buildAmount, ba->noBuild)) {
	rc = 1;
	goto exit;
    }
    
    if (ba->buildMode == 't')
	(void) Unlink(specURL);
    rc = 0;

exit:
    spec = freeSpec(spec);
    specURL = _free(specURL);
    return rc;
}
/*@=boundswrite@*/

int build(rpmts ts, const char * arg, BTA_t ba, const char * rcfile)
{
    const char *t, *te;
    int rc = 0;
    const char * targets = rpmcliTargets;
    char *target;
#define	buildCleanMask	(RPMBUILD_RMSOURCE|RPMBUILD_RMSPEC)
    int cleanFlags = ba->buildAmount & buildCleanMask;
    rpmVSFlags vsflags, ovsflags;
    int nbuilds = 0;

    vsflags = rpmExpandNumeric("%{_vsflags_build}");
    if (ba->qva_flags & VERIFY_DIGEST)
	vsflags |= _RPMVSF_NODIGESTS;
    if (ba->qva_flags & VERIFY_SIGNATURE)
	vsflags |= _RPMVSF_NOSIGNATURES;
    if (ba->qva_flags & VERIFY_HDRCHK)
	vsflags |= RPMVSF_NOHDRCHK;
    ovsflags = rpmtsSetVSFlags(ts, vsflags);

    if (targets == NULL) {
	rc =  buildForTarget(ts, arg, ba);
	nbuilds++;
	goto exit;
    }

    /* parse up the build operators */

    printf(_("Building target platforms: %s\n"), targets);

    ba->buildAmount &= ~buildCleanMask;
    for (t = targets; *t != '\0'; t = te) {
	/* Parse out next target platform. */ 
	if ((te = strchr(t, ',')) == NULL)
	    te = t + strlen(t);
	target = alloca(te-t+1);
	strncpy(target, t, (te-t));
	target[te-t] = '\0';
	if (*te != '\0')
	    te++;
	else	/* XXX Perform clean-up after last target build. */
	    ba->buildAmount |= cleanFlags;

	rpmlog(RPMLOG_DEBUG, _("    target platform: %s\n"), target);

	/* Read in configuration for target. */
	if (t != targets) {
	    rpmFreeMacros(NULL);
	    rpmFreeRpmrc();
	    (void) rpmReadConfigFiles(rcfile, target);
	}
	rc = buildForTarget(ts, arg, ba);
	nbuilds++;
	if (rc)
	    break;
    }

exit:
    /* Restore original configuration. */
    if (nbuilds > 1) {
	t = targets;
	if ((te = strchr(t, ',')) == NULL)
	    te = t + strlen(t);
	target = alloca(te-t+1);
	strncpy(target, t, (te-t));
	target[te-t] = '\0';
	if (*te != '\0')
	    te++;
	rpmFreeMacros(NULL);
	rpmFreeRpmrc();
	(void) rpmReadConfigFiles(rcfile, target);
    }
    vsflags = rpmtsSetVSFlags(ts, ovsflags);

    return rc;
}

/** \ingroup rpmbuild
 * \file build/parsePrep.c
 *  Parse %prep section from spec file.
 */

#include "system.h"

#include <rpmio.h>
#ifdef	NOTYET
#include <rpmmg.h>
#endif
#include <rpmbuild.h>
#include "debug.h"

/*@access StringBuf @*/	/* compared with NULL */

/* These have to be global to make up for stupid compilers */
/*@unchecked@*/
    static int leaveDirs, skipDefaultAction;
/*@unchecked@*/
    static int createDir, quietly;
/*@unchecked@*/ /*@observer@*/ /*@null@*/
    static const char * dirName = NULL;
/*@unchecked@*/ /*@observer@*/
    static struct poptOption optionsTable[] = {
	    { NULL, 'a', POPT_ARG_STRING, NULL, 'a',	NULL, NULL},
	    { NULL, 'b', POPT_ARG_STRING, NULL, 'b',	NULL, NULL},
	    { NULL, 'c', 0, &createDir, 0,		NULL, NULL},
	    { NULL, 'D', 0, &leaveDirs, 0,		NULL, NULL},
	    { NULL, 'n', POPT_ARG_STRING, &dirName, 0,	NULL, NULL},
	    { NULL, 'T', 0, &skipDefaultAction, 0,	NULL, NULL},
	    { NULL, 'q', 0, &quietly, 0,		NULL, NULL},
	    { 0, 0, 0, 0, 0,	NULL, NULL}
    };

/**
 * Check that file owner and group are known.
 * @param urlfn		file url
 * @return		RPMRC_OK on success
 */
static rpmRC checkOwners(const char * urlfn)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    struct stat sb;

    if (Lstat(urlfn, &sb)) {
	rpmlog(RPMLOG_ERR, _("Bad source: %s: %s\n"),
		urlfn, strerror(errno));
	return RPMRC_FAIL;
    }
    if (!getUname(sb.st_uid) || !getGname(sb.st_gid)) {
	rpmlog(RPMLOG_ERR, _("Bad owner/group: %s\n"), urlfn);
	return RPMRC_FAIL;
    }

    return RPMRC_OK;
}

#ifndef	DYING
/**
 * Expand %patchN macro into %prep scriptlet.
 * @param spec		build info
 * @param c		patch index
 * @param strip		patch level (i.e. patch -p argument)
 * @param db		saved file suffix (i.e. patch --suffix argument)
 * @param reverse	include -R?
 * @param removeEmpties	include -E?
 * @param fuzz		include -F?
 * @return		expanded %patch macro (NULL on error)
 */
/*@observer@*/
static char *doPatch(Spec spec, int c, int strip, const char *db,
		     int reverse, int removeEmpties, int fuzz, const char *subdir)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext, fileSystem, internalState @*/
{
    const char *fn, *Lurlfn;
    static char buf[BUFSIZ];
    char args[BUFSIZ], *t = args;
    struct Source *sp;
    rpmCompressedMagic compressed = COMPRESSED_NOT;
    int urltype;
    const char *patch;

    *t = '\0';
    if (db)
	t = stpcpy( stpcpy(t, "-b --suffix "), db);
#if defined(RPM_VENDOR_OPENPKG) /* always-backup-on-patching */
    /* always create backup files in OpenPKG */
    else
	t = stpcpy(t, "-b --suffix .orig ");
#endif
    if (subdir)
	t = stpcpy( stpcpy(t, "-d "), subdir);
    if (fuzz) {
	t = stpcpy(t, "-F ");
	sprintf(t, "%10.10d", fuzz);
	t += strlen(t);
    }
    if (reverse)
	t = stpcpy(t, " -R");
    if (removeEmpties)
	t = stpcpy(t, " -E");

    for (sp = spec->sources; sp != NULL; sp = sp->next) {
	if ((sp->flags & RPMFILE_PATCH) && (sp->num == c))
	    break;
    }
    if (sp == NULL) {
	rpmlog(RPMLOG_ERR, _("No patch number %d\n"), c);
	return NULL;
    }

    Lurlfn = rpmGenPath(NULL, "%{_patchdir}/", sp->source);

    /* XXX On non-build parse's, file cannot be stat'd or read */
    if (!spec->force && (isCompressed(Lurlfn, &compressed) || checkOwners(Lurlfn))) {
	Lurlfn = _free(Lurlfn);
	return NULL;
    }

    fn = NULL;
    urltype = urlPath(Lurlfn, &fn);
    switch (urltype) {
    case URL_IS_HTTPS:	/* XXX WRONG WRONG WRONG */
    case URL_IS_HTTP:	/* XXX WRONG WRONG WRONG */
    case URL_IS_FTP:	/* XXX WRONG WRONG WRONG */
    case URL_IS_HKP:	/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
	Lurlfn = _free(Lurlfn);
	return NULL;
	/*@notreached@*/ break;
    }

    patch = rpmGetPath("%{__patch}", NULL);
    if (strcmp(patch, "%{__patch}") == 0)
        patch = xstrdup("patch");

    if (compressed) {
	const char *zipper;

	switch (compressed) {
	default:
	case COMPRESSED_NOT:	/* XXX can't happen */
	case COMPRESSED_OTHER:
	case COMPRESSED_ZIP:	/* XXX wrong */
	    zipper = "%{__gzip}";
	    break;
	case COMPRESSED_BZIP2:
	    zipper = "%{__bzip2}";
	    break;
	case COMPRESSED_LZOP:
	    zipper = "%{__lzop}";
	    break;
	case COMPRESSED_LZMA:
	    zipper = "%{__lzma}";
	    break;
	}
	zipper = rpmGetPath(zipper, NULL);

	sprintf(buf,
		"echo \"Patch #%d (%s):\"\n"
		"%s -d < '%s' | %s -p%d %s -s\n"
		"STATUS=$?\n"
		"if [ $STATUS -ne 0 ]; then\n"
		"  exit $STATUS\n"
		"fi",
		c,
/*@-moduncon@*/
		(const char *) basename((char *)fn),
/*@=moduncon@*/
		zipper,
		fn, patch, strip, args);
	zipper = _free(zipper);
    } else {
	sprintf(buf,
		"echo \"Patch #%d (%s):\"\n"
		"%s -p%d %s -s < '%s'", c,
/*@-moduncon@*/
		(const char *) basename((char *)fn),
/*@=moduncon@*/
		patch, strip, args, fn);
    }

    patch = _free(patch);
    Lurlfn = _free(Lurlfn);
    return buf;
}
#endif

/**
 * Expand %setup macro into %prep scriptlet.
 * @param spec		build info
 * @param c		source index
 * @param quietly	should -vv be omitted from tar?
 * @return		expanded %setup macro (NULL on error)
 */
/*@observer@*/
static const char *doUntar(Spec spec, int c, int quietly)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext, fileSystem, internalState @*/
{
    const char *fn, *Lurlfn;
    static char buf[BUFSIZ];
    char *taropts;
    char *t = NULL;
    struct Source *sp;
    rpmCompressedMagic compressed = COMPRESSED_NOT;
    int urltype;
    const char *tar;

    for (sp = spec->sources; sp != NULL; sp = sp->next) {
	if ((sp->flags & RPMFILE_SOURCE) && (sp->num == c)) {
	    break;
	}
    }
    if (sp == NULL) {
	rpmlog(RPMLOG_ERR, _("No source number %d\n"), c);
	return NULL;
    }

    /*@-internalglobs@*/ /* FIX: shrug */
    taropts = ((rpmIsVerbose() && !quietly) ? "-xvvf" : "-xf");
    /*@=internalglobs@*/

#if defined(RPM_VENDOR_OPENPKG) /* splitted-source-directory */
    Lurlfn = rpmGenPath(NULL, getSourceDir(sp->flags, sp->source), sp->source);
#else
    Lurlfn = rpmGenPath(NULL, getSourceDir(sp->flags), sp->source);
#endif

    /* XXX On non-build parse's, file cannot be stat'd or read */
    if (!spec->force && (isCompressed(Lurlfn, &compressed) || checkOwners(Lurlfn))) {
	Lurlfn = _free(Lurlfn);
	return NULL;
    }

    fn = NULL;
    urltype = urlPath(Lurlfn, &fn);
    switch (urltype) {
    case URL_IS_HTTPS:	/* XXX WRONG WRONG WRONG */
    case URL_IS_HTTP:	/* XXX WRONG WRONG WRONG */
    case URL_IS_FTP:	/* XXX WRONG WRONG WRONG */
    case URL_IS_HKP:	/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
	Lurlfn = _free(Lurlfn);
	return NULL;
	/*@notreached@*/ break;
    }
#ifdef	NOTYET
    {	rpmmg mg;
	
_rpmmg_debug = 1;
	mg = rpmmgNew(NULL, 0);
	t = (char *) rpmmgFile(mg, fn);
	mg = rpmmgFree(mg);
fprintf(stderr, "==> %s: %s\n", fn, t);
	t = _free(t);
_rpmmg_debug = 0;
    }
#endif

    tar = rpmGetPath("%{__tar}", NULL);
    if (strcmp(tar, "%{__tar}") == 0)
        tar = xstrdup("tar");

    if (compressed != COMPRESSED_NOT) {
	const char *zipper;
	int needtar = 1;

	switch (compressed) {
	case COMPRESSED_NOT:	/* XXX can't happen */
	case COMPRESSED_OTHER:
	    t = "%{__gzip} -dc";
	    break;
	case COMPRESSED_BZIP2:
	    t = "%{__bzip2} -dc";
	    break;
	case COMPRESSED_LZOP:
	    t = "%{__lzop} -dc";
	    break;
	case COMPRESSED_LZMA:
	    t = "%{__lzma} -dc";
	    break;
	case COMPRESSED_ZIP:
	    if (rpmIsVerbose() && !quietly)
		t = "%{__unzip}";
	    else
		t = "%{__unzip} -qq";
	    needtar = 0;
	    break;
	}
	zipper = rpmGetPath(t, NULL);
	buf[0] = '\0';
	t = stpcpy(buf, zipper);
	zipper = _free(zipper);
	*t++ = ' ';
	*t++ = '\'';
	t = stpcpy(t, fn);
	*t++ = '\'';
	if (needtar) {
	    t = stpcpy(t, " | ");
	    t = stpcpy(t, tar);
	    t = stpcpy(t, " ");
	    t = stpcpy(t, taropts);
	    t = stpcpy(t, " -");
        }
	t = stpcpy(t,
		"\n"
		"STATUS=$?\n"
		"if [ $STATUS -ne 0 ]; then\n"
		"  exit $STATUS\n"
		"fi");
    } else {
	buf[0] = '\0';
	t = stpcpy(buf, tar);
	t = stpcpy(t, " ");
	t = stpcpy(t, taropts);
	*t++ = ' ';
	t = stpcpy(t, fn);
    }

    tar = _free(tar);
    Lurlfn = _free(Lurlfn);
    return buf;
}

/**
 * Parse %setup macro.
 * @todo FIXME: Option -q broken when not immediately after %setup.
 * @param spec		build info
 * @param line		current line from spec file
 * @return		0 on success
 */
static int doSetupMacro(Spec spec, char *line)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies spec->buildSubdir, spec->macros, spec->prep,
		spec->packages->header,
		rpmGlobalMacroContext, fileSystem, internalState @*/
{
    char buf[BUFSIZ];
    StringBuf before;
    StringBuf after;
    poptContext optCon;
    int argc;
    const char ** argv;
    int arg;
    const char * optArg;
    int rc;
    uint32_t num;

    /*@-mods@*/
    leaveDirs = skipDefaultAction = 0;
    createDir = quietly = 0;
    dirName = NULL;
    /*@=mods@*/

    if ((rc = poptParseArgvString(line, &argc, &argv))) {
	rpmlog(RPMLOG_ERR, _("Error parsing %%setup: %s\n"),
			poptStrerror(rc));
	return RPMRC_FAIL;
    }

    before = newStringBuf();
    after = newStringBuf();

    optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
    while ((arg = poptGetNextOpt(optCon)) > 0) {
	optArg = poptGetOptArg(optCon);

	/* We only parse -a and -b here */

	if (parseNum(optArg, &num)) {
	    rpmlog(RPMLOG_ERR, _("line %d: Bad arg to %%setup: %s\n"),
		     spec->lineNum, (optArg ? optArg : "???"));
	    before = freeStringBuf(before);
	    after = freeStringBuf(after);
	    optCon = poptFreeContext(optCon);
	    argv = _free(argv);
	    return RPMRC_FAIL;
	}

	{   const char *chptr = doUntar(spec, num, quietly);
	    if (chptr == NULL)
		return RPMRC_FAIL;

	    appendLineStringBuf((arg == 'a' ? after : before), chptr);
	}
    }

    if (arg < -1) {
	rpmlog(RPMLOG_ERR, _("line %d: Bad %%setup option %s: %s\n"),
		 spec->lineNum,
		 poptBadOption(optCon, POPT_BADOPTION_NOALIAS), 
		 poptStrerror(arg));
	before = freeStringBuf(before);
	after = freeStringBuf(after);
	optCon = poptFreeContext(optCon);
	argv = _free(argv);
	return RPMRC_FAIL;
    }

    if (dirName) {
	spec->buildSubdir = xstrdup(dirName);
    } else {
	const char *N, *V;
	(void) headerNEVRA(spec->packages->header, &N, NULL, &V, NULL, NULL);
	(void) snprintf(buf, sizeof(buf), "%s-%s", N, V);
	buf[sizeof(buf)-1] = '\0';
	N = _free(N);
	V = _free(V);
	spec->buildSubdir = xstrdup(buf);
    }
    addMacro(spec->macros, "buildsubdir", NULL, spec->buildSubdir, RMIL_SPEC);
    
    optCon = poptFreeContext(optCon);
    argv = _free(argv);

    /* cd to the build dir */
    {	const char * buildDirURL = rpmGenPath(spec->rootURL, "%{_builddir}", "");
	const char *buildDir;

	(void) urlPath(buildDirURL, &buildDir);
	sprintf(buf, "cd '%s'", buildDir);
	appendLineStringBuf(spec->prep, buf);
	buildDirURL = _free(buildDirURL);
    }
    
    /* delete any old sources */
    if (!leaveDirs) {
	sprintf(buf, "rm -rf '%s'", spec->buildSubdir);
	appendLineStringBuf(spec->prep, buf);
    }

    /* if necessary, create and cd into the proper dir */
    if (createDir) {
	char *mkdir_p;
	mkdir_p = rpmExpand("%{?__mkdir_p}%{!?__mkdir_p:mkdir -p}", NULL);
	if (!mkdir_p)
	    mkdir_p = xstrdup("mkdir -p");
	sprintf(buf, "%s '%s'\ncd '%s'",
		mkdir_p, spec->buildSubdir, spec->buildSubdir);
	mkdir_p = _free(mkdir_p);
	appendLineStringBuf(spec->prep, buf);
    }

    /* do the default action */
   if (!createDir && !skipDefaultAction) {
	const char *chptr = doUntar(spec, 0, quietly);
	if (!chptr)
	    return RPMRC_FAIL;
	appendLineStringBuf(spec->prep, chptr);
    }

    appendStringBuf(spec->prep, getStringBuf(before));
    before = freeStringBuf(before);

    if (!createDir) {
	sprintf(buf, "cd '%s'", spec->buildSubdir);
	appendLineStringBuf(spec->prep, buf);
    }

    if (createDir && !skipDefaultAction) {
	const char * chptr = doUntar(spec, 0, quietly);
	if (chptr == NULL)
	    return RPMRC_FAIL;
	appendLineStringBuf(spec->prep, chptr);
    }
    
    appendStringBuf(spec->prep, getStringBuf(after));
    after = freeStringBuf(after);

    /* XXX FIXME: owner & group fixes were conditioned on !geteuid() */
    /* Fix the owner, group, and permissions of the setup build tree */
    {	/*@observer@*/ static const char *fixmacs[] =
		{ "%{_fixowner}", "%{_fixgroup}", "%{_fixperms}", NULL };
	const char ** fm;

	for (fm = fixmacs; *fm; fm++) {
	    const char *fix;
	    fix = rpmExpand(*fm, " .", NULL);
	    if (fix && *fix != '%')
		appendLineStringBuf(spec->prep, fix);
	    fix = _free(fix);
	}
    }
    
    return 0;
}

#ifndef	DYING
/**
 * Parse %patch line.
 * @param spec		build info
 * @param line		current line from spec file
 * @return		RPMRC_OK on success
 */
static rpmRC doPatchMacro(Spec spec, char *line)
	/*@globals rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies spec->prep, rpmGlobalMacroContext,
		fileSystem, internalState  @*/
{
    char *s;
    char *opt_b;
    char *opt_d;
    uint32_t opt_P, opt_p, opt_R, opt_E, opt_F;
    char buf[BUFSIZ], *bp;
    uint32_t patch_nums[1024];  /* XXX - we can only handle 1024 patches! */
    int patch_index, x;

    memset(patch_nums, 0, sizeof(patch_nums));
    opt_P = opt_p = opt_R = opt_E = opt_F = 0;
    opt_b = NULL;
    opt_d = NULL;
    patch_index = 0;

    if (! strchr(" \t\n", line[6])) {
	/* %patchN */
	sprintf(buf, "%%patch -P %s", line + 6);
    } else {
	strcpy(buf, line);
    }
    
    /*@-internalglobs@*/	/* FIX: strtok has state */
    for (bp = buf; (s = strtok(bp, " \t\n")) != NULL;) {
	if (bp) {	/* remove 1st token (%patch) */
	    bp = NULL;
	    continue;
	}
	if (!strcmp(s, "-P")) {
	    opt_P = 1;
	} else if (!strcmp(s, "-R")) {
	    opt_R = 1;
	} else if (!strcmp(s, "-E")) {
	    opt_E = 1;
	} else if (!strcmp(s, "-b")) {
	    /* orig suffix */
	    opt_b = strtok(NULL, " \t\n");
	    if (! opt_b) {
		rpmlog(RPMLOG_ERR,
			_("line %d: Need arg to %%patch -b: %s\n"),
			spec->lineNum, spec->line);
		return RPMRC_FAIL;
	    }
	} else if (!strcmp(s, "-z")) {
	    /* orig suffix */
	    opt_b = strtok(NULL, " \t\n");
	    if (! opt_b) {
		rpmlog(RPMLOG_ERR,
			_("line %d: Need arg to %%patch -z: %s\n"),
			spec->lineNum, spec->line);
		return RPMRC_FAIL;
	    }
	} else if (!strcmp(s, "-F")) {
	    /* fuzz factor */
	    const char * fnum = (!strchr(" \t\n", s[2])
				? s+2 : strtok(NULL, " \t\n"));
	    char * end = NULL;

	    opt_F = (fnum ? strtol(fnum, &end, 10) : 0);
	    if (! opt_F || *end) {
		rpmlog(RPMLOG_ERR,
			_("line %d: Bad arg to %%patch -F: %s\n"),
			spec->lineNum, spec->line);
		return RPMRC_FAIL;
	    }
	} else if (!strcmp(s, "-d")) {
	    /* subdirectory */
	    opt_d = strtok(NULL, " \t\n");
	    if (! opt_d) {
		rpmlog(RPMLOG_ERR,
			_("line %d: Need arg to %%patch -d: %s\n"),
			spec->lineNum, spec->line);
		return RPMRC_FAIL;
	    }
	} else if (!strncmp(s, "-p", sizeof("-p")-1)) {
	    /* unfortunately, we must support -pX */
	    if (! strchr(" \t\n", s[2])) {
		s = s + 2;
	    } else {
		s = strtok(NULL, " \t\n");
		if (s == NULL) {
		    rpmlog(RPMLOG_ERR,
			     _("line %d: Need arg to %%patch -p: %s\n"),
			     spec->lineNum, spec->line);
		    return RPMRC_FAIL;
		}
	    }
	    if (parseNum(s, &opt_p)) {
		rpmlog(RPMLOG_ERR,
			_("line %d: Bad arg to %%patch -p: %s\n"),
			spec->lineNum, spec->line);
		return RPMRC_FAIL;
	    }
	} else {
	    /* Must be a patch num */
	    if (patch_index == 1024) {
		rpmlog(RPMLOG_ERR, _("Too many patches!\n"));
		return RPMRC_FAIL;
	    }
	    if (parseNum(s, &(patch_nums[patch_index]))) {
		rpmlog(RPMLOG_ERR, _("line %d: Bad arg to %%patch: %s\n"),
			 spec->lineNum, spec->line);
		return RPMRC_FAIL;
	    }
	    patch_index++;
	}
    }
    /*@=internalglobs@*/

    /* All args processed */

    if (! opt_P) {
	s = doPatch(spec, 0, opt_p, opt_b, opt_R, opt_E, opt_F, opt_d);
	if (s == NULL)
	    return RPMRC_FAIL;
	appendLineStringBuf(spec->prep, s);
    }

    for (x = 0; x < patch_index; x++) {
	s = doPatch(spec, patch_nums[x], opt_p, opt_b, opt_R, opt_E, opt_F, opt_d);
	if (s == NULL)
	    return RPMRC_FAIL;
	appendLineStringBuf(spec->prep, s);
    }
    
    return RPMRC_OK;
}
#endif

/**
 * Check that all sources/patches/icons exist locally, fetching if necessary.
 */
static int prepFetch(Spec spec)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext, fileSystem, internalState @*/
{
#if defined(RPM_VENDOR_OPENPKG) /* splitted-source-directory */
    const char *Smacro;
#endif
    const char *Lmacro, *Lurlfn = NULL;
    const char *Rmacro, *Rurlfn = NULL;
    struct Source *sp;
    struct stat st;
    rpmRC rpmrc;
    int ec, rc;

    /* XXX insure that %{_sourcedir} exists */
    rpmrc = RPMRC_OK;
    Lurlfn = rpmGenPath(NULL, "%{?_sourcedir}", NULL);
    if (Lurlfn != NULL && *Lurlfn != '\0')
	rpmrc = rpmMkdirPath(Lurlfn, "_sourcedir");
    Lurlfn = _free(Lurlfn);
    if (rpmrc != RPMRC_OK)
	return -1;

    /* XXX insure that %{_patchdir} exists */
    rpmrc = RPMRC_OK;
    Lurlfn = rpmGenPath(NULL, "%{?_patchdir}", NULL);
    if (Lurlfn != NULL && *Lurlfn != '\0')
	rpmrc = rpmMkdirPath(Lurlfn, "_patchdir");
    Lurlfn = _free(Lurlfn);
    if (rpmrc != RPMRC_OK)
	return -1;

    /* XXX insure that %{_icondir} exists */
    rpmrc = RPMRC_OK;
    Lurlfn = rpmGenPath(NULL, "%{?_icondir}", NULL);
    if (Lurlfn != NULL && *Lurlfn != '\0')
	rpmrc = rpmMkdirPath(Lurlfn, "_icondir");
    Lurlfn = _free(Lurlfn);
    if (rpmrc != RPMRC_OK)
	return -1;

    ec = 0;
    for (sp = spec->sources; sp != NULL; sp = sp->next) {
    
#if defined(RPM_VENDOR_OPENPKG) /* splitted-source-directory */
	Smacro = "%{?_specdir}/";
#endif
#if defined(RPM_VENDOR_OPENPKG) /* splitted-source-directory */
    if (! (Lmacro = getSourceDir(sp->flags, sp->source)))
#else
    if (! (Lmacro = getSourceDir(sp->flags)))
#endif
        continue;
	if (sp->flags & RPMFILE_SOURCE) {
	    Rmacro = "%{?_Rsourcedir}/";
	} else
	if (sp->flags & RPMFILE_PATCH) {
	    Rmacro = "%{?_Rpatchdir}/";
	} else
	if (sp->flags & RPMFILE_ICON) {
	    Rmacro = "%{?_Ricondir}/";
	} else
	    continue;

#if defined(RPM_VENDOR_OPENPKG) /* splitted-source-directory */
	/* support splitted source directories, i.e., source files which
	   are alternatively placed into the .spec directory and picked
	   up from there, too. */
	Lurlfn = rpmGenPath(NULL, Smacro, sp->source);
	rc = Lstat(Lurlfn, &st);
	if (rc == 0)
	    goto bottom;
#endif
	Lurlfn = rpmGenPath(NULL, Lmacro, sp->source);
	rc = Lstat(Lurlfn, &st);
	if (rc == 0)
	    goto bottom;
	if (errno != ENOENT) {
	    ec++;
	    rpmlog(RPMLOG_ERR, _("Missing %s%d %s: %s\n"),
		((sp->flags & RPMFILE_SOURCE) ? "Source" : "Patch"),
		sp->num, sp->source, strerror(ENOENT));
	    goto bottom;
	}

	Rurlfn = rpmGenPath(NULL, Rmacro, sp->source);
	if (Rurlfn == NULL || Rurlfn[0] == '\0' || !strcmp(Rurlfn, "/") || !strcmp(Lurlfn, Rurlfn)) {
	    rpmlog(RPMLOG_ERR, _("file %s missing: %s\n"),
		Lurlfn, strerror(errno));
	    ec++;
	    goto bottom;
	}

	rc = urlGetFile(Rurlfn, Lurlfn);
	if (rc != 0) {
	    rpmlog(RPMLOG_ERR, _("Fetching %s failed: %s\n"),
		Rurlfn, ftpStrerror(rc));
	    ec++;
	    goto bottom;
	}

bottom:
	Lurlfn = _free(Lurlfn);
	Rurlfn = _free(Rurlfn);
    }

    return ec;
}

int parsePrep(Spec spec, int verify)
{
    rpmParseState nextPart;
    int res, rc;
    StringBuf sb;
    char **lines, **saveLines;
    char *cp;

    if (spec->prep != NULL) {
	rpmlog(RPMLOG_ERR, _("line %d: second %%prep\n"), spec->lineNum);
	return RPMRC_FAIL;
    }

    spec->prep = newStringBuf();

    /* There are no options to %prep */
    if ((rc = readLine(spec, STRIP_NOTHING)) > 0)
	return PART_NONE;
    if (rc)
	return rc;

    /* Check to make sure that all sources/patches are present. */
    if (verify) {
	rc = prepFetch(spec);
	if (rc)
	    return RPMRC_FAIL;
    }
    
    sb = newStringBuf();
    
    while ((nextPart = isPart(spec->line)) == PART_NONE) {
	/* Need to expand the macros inline.  That way we  */
	/* can give good line number information on error. */
	appendStringBuf(sb, spec->line);
	if ((rc = readLine(spec, STRIP_NOTHING)) > 0) {
	    nextPart = PART_NONE;
	    break;
	}
	if (rc)
	    return rc;
    }

    saveLines = splitString(getStringBuf(sb), strlen(getStringBuf(sb)), '\n');
    /*@-usereleased@*/
    for (lines = saveLines; *lines; lines++) {
	res = 0;
	for (cp = *lines; *cp == ' ' || *cp == '\t'; cp++)
	    {};
	if (! strncmp(cp, "%setup", sizeof("%setup")-1)) {
	    res = doSetupMacro(spec, cp);
#ifndef	DYING
	} else if (! strncmp(cp, "%patch", sizeof("%patch")-1)) {
	    res = doPatchMacro(spec, cp);
#endif
	} else {
	    appendLineStringBuf(spec->prep, *lines);
	}
	if (res && !spec->force) {
	    freeSplitString(saveLines);
	    sb = freeStringBuf(sb);
	    return res;
	}
    }
    /*@=usereleased@*/

    freeSplitString(saveLines);
    sb = freeStringBuf(sb);

    return nextPart;
}

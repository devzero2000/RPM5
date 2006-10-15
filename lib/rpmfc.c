#include "system.h"

#include <signal.h>	/* getOutputFrom() */

#include <rpmbuild.h>
#include <argv.h>

#define	_RPMFC_INTERNAL
#include <rpmfc.h>

#define	_RPMDS_INTERNAL
#include <rpmds.h>
#include <rpmfi.h>

#include "debug.h"

/*@access rpmds @*/

/**
 */
static int rpmfcExpandAppend(/*@out@*/ ARGV_t * argvp, const ARGV_t av)
	/*@globals rpmGlobalMacroContext, h_errno @*/
	/*@modifies *argvp, rpmGlobalMacroContext @*/
	/*@requires maxRead(argvp) >= 0 @*/
{
    ARGV_t argv = *argvp;
    int argc = argvCount(argv);
    int ac = argvCount(av);
    int i;

/*@-bounds@*/	/* LCL: internal error */
    argv = xrealloc(argv, (argc + ac + 1) * sizeof(*argv));
/*@=bounds@*/
    for (i = 0; i < ac; i++)
	argv[argc + i] = rpmExpand(av[i], NULL);
    argv[argc + ac] = NULL;
    *argvp = argv;
    return 0;
}

/** \ingroup rpmbuild
 * Return output from helper script.
 * @todo Use poll(2) rather than select(2), if available.
 * @param dir		directory to run in (or NULL)
 * @param argv		program and arguments to run
 * @param writePtr	bytes to feed to script on stdin (or NULL)
 * @param writeBytesLeft no. of bytes to feed to script on stdin
 * @param failNonZero	is script failure an error?
 * @return		buffered stdout from script, NULL on error
 */     
/*@null@*/
static StringBuf getOutputFrom(/*@null@*/ const char * dir, ARGV_t argv,
                        const char * writePtr, int writeBytesLeft,
                        int failNonZero)
        /*@globals fileSystem, internalState@*/
        /*@modifies fileSystem, internalState@*/
{
    pid_t child, reaped;
    int toProg[2];
    int fromProg[2];
    int status;
    void *oldhandler;
    StringBuf readBuff;
    int done;

    /*@-type@*/ /* FIX: cast? */
    oldhandler = signal(SIGPIPE, SIG_IGN);
    /*@=type@*/

    toProg[0] = toProg[1] = 0;
    (void) pipe(toProg);
    fromProg[0] = fromProg[1] = 0;
    (void) pipe(fromProg);
    
    if (!(child = fork())) {
	(void) close(toProg[1]);
	(void) close(fromProg[0]);
	
	(void) dup2(toProg[0], STDIN_FILENO);   /* Make stdin the in pipe */
	(void) dup2(fromProg[1], STDOUT_FILENO); /* Make stdout the out pipe */

	(void) close(toProg[0]);
	(void) close(fromProg[1]);

	if (dir) {
	    (void) Chdir(dir);
	}
	
	rpmMessage(RPMMESS_DEBUG, _("\texecv(%s) pid %d\n"),
                        argv[0], (unsigned)getpid());

	unsetenv("MALLOC_CHECK_");
	(void) execvp(argv[0], (char *const *)argv);
	/* XXX this error message is probably not seen. */
	rpmError(RPMERR_EXEC, _("Couldn't exec %s: %s\n"),
		argv[0], strerror(errno));
	_exit(RPMERR_EXEC);
    }
    if (child < 0) {
	rpmError(RPMERR_FORK, _("Couldn't fork %s: %s\n"),
		argv[0], strerror(errno));
	return NULL;
    }

    (void) close(toProg[0]);
    (void) close(fromProg[1]);

    /* Do not block reading or writing from/to prog. */
    (void) fcntl(fromProg[0], F_SETFL, O_NONBLOCK);
    (void) fcntl(toProg[1], F_SETFL, O_NONBLOCK);
    
    readBuff = newStringBuf();

    do {
	fd_set ibits, obits;
	struct timeval tv;
	int nfd, nbw, nbr;
	int rc;

	done = 0;
top:
	FD_ZERO(&ibits);
	FD_ZERO(&obits);
	if (fromProg[0] >= 0) {
	    FD_SET(fromProg[0], &ibits);
	}
	if (toProg[1] >= 0) {
	    FD_SET(toProg[1], &obits);
	}
	/* XXX values set to limit spinning with perl doing ~100 forks/sec. */
	tv.tv_sec = 0;
	tv.tv_usec = 10000;
	nfd = ((fromProg[0] > toProg[1]) ? fromProg[0] : toProg[1]);
	if ((rc = select(nfd, &ibits, &obits, NULL, &tv)) < 0) {
	    if (errno == EINTR)
		goto top;
	    break;
	}

	/* Write any data to program */
	if (toProg[1] >= 0 && FD_ISSET(toProg[1], &obits)) {
          if (writePtr && writeBytesLeft > 0) {
	    if ((nbw = write(toProg[1], writePtr,
		    (1024<writeBytesLeft) ? 1024 : writeBytesLeft)) < 0) {
	        if (errno != EAGAIN) {
		    perror("getOutputFrom()");
	            exit(EXIT_FAILURE);
		}
	        nbw = 0;
	    }
	    writeBytesLeft -= nbw;
	    writePtr += nbw;
	  } else if (toProg[1] >= 0) {	/* close write fd */
	    (void) close(toProg[1]);
	    toProg[1] = -1;
	  }
	}
	
	/* Read any data from prog */
/*@-boundswrite@*/
	{   char buf[BUFSIZ+1];
	    while ((nbr = read(fromProg[0], buf, sizeof(buf)-1)) > 0) {
		buf[nbr] = '\0';
		appendStringBuf(readBuff, buf);
	    }
	}
/*@=boundswrite@*/

	/* terminate on (non-blocking) EOF or error */
	done = (nbr == 0 || (nbr < 0 && errno != EAGAIN));

    } while (!done);

    /* Clean up */
    if (toProg[1] >= 0)
    	(void) close(toProg[1]);
    if (fromProg[0] >= 0)
	(void) close(fromProg[0]);
    /*@-type@*/ /* FIX: cast? */
    (void) signal(SIGPIPE, oldhandler);
    /*@=type@*/

    /* Collect status from prog */
    reaped = waitpid(child, &status, 0);
    rpmMessage(RPMMESS_DEBUG, _("\twaitpid(%d) rc %d status %x\n"),
        (unsigned)child, (unsigned)reaped, status);

    if (failNonZero && (!WIFEXITED(status) || WEXITSTATUS(status))) {
	rpmError(RPMERR_EXEC, _("%s failed\n"), argv[0]);
	return NULL;
    }
    if (writeBytesLeft) {
	rpmError(RPMERR_EXEC, _("failed to write all data to %s\n"), argv[0]);
	return NULL;
    }
    return readBuff;
}

int rpmfcExec(ARGV_t av, StringBuf sb_stdin, StringBuf * sb_stdoutp,
		int failnonzero)
{
    const char * s = NULL;
    ARGV_t xav = NULL;
    ARGV_t pav = NULL;
    int pac = 0;
    int ec = -1;
    StringBuf sb = NULL;
    const char * buf_stdin = NULL;
    int buf_stdin_len = 0;
    int xx;

    if (sb_stdoutp)
	*sb_stdoutp = NULL;
    if (!(av && *av))
	goto exit;

    /* Find path to executable with (possible) args. */
    s = rpmExpand(av[0], NULL);
    if (!(s && *s))
	goto exit;

    /* Parse args buried within expanded executable. */
    pac = 0;
    xx = poptParseArgvString(s, &pac, (const char ***)&pav);
    if (!(xx == 0 && pac > 0 && pav != NULL))
	goto exit;

    /* Build argv, appending args to the executable args. */
    xav = NULL;
/*@-boundswrite@*/
    xx = argvAppend(&xav, pav);
    if (av[1])
	xx = rpmfcExpandAppend(&xav, av + 1);
/*@=boundswrite@*/

    if (sb_stdin != NULL) {
	buf_stdin = getStringBuf(sb_stdin);
	buf_stdin_len = strlen(buf_stdin);
    }

    /* Read output from exec'd helper. */
    sb = getOutputFrom(NULL, xav, buf_stdin, buf_stdin_len, failnonzero);

/*@-branchstate@*/
    if (sb_stdoutp != NULL) {
	*sb_stdoutp = sb;
	sb = NULL;	/* XXX don't free */
    }
/*@=branchstate@*/

    ec = 0;

exit:
    sb = freeStringBuf(sb);
    xav = argvFree(xav);
    pav = _free(pav);	/* XXX popt mallocs in single blob. */
    s = _free(s);
    return ec;
}

/**
 */
static int rpmfcSaveArg(/*@out@*/ ARGV_t * argvp, const char * key)
	/*@modifies *argvp @*/
	/*@requires maxSet(argvp) >= 0 @*/
{
    int rc = 0;

    if (argvSearch(*argvp, key, NULL) == NULL) {
	rc = argvAdd(argvp, key);
	rc = argvSort(*argvp, NULL);
    }
    return rc;
}

/**
 */
static char * rpmfcFileDep(/*@returned@*/ char * buf, int ix,
		/*@null@*/ rpmds ds)
	/*@modifies buf @*/
	/*@requires maxSet(buf) >= 0 @*/
	/*@ensures maxRead(buf) == 0 @*/
{
    int_32 tagN = rpmdsTagN(ds);
    char deptype = 'X';

    buf[0] = '\0';
    switch (tagN) {
    case RPMTAG_PROVIDENAME:
	deptype = 'P';
	break;
    case RPMTAG_REQUIRENAME:
	deptype = 'R';
	break;
    }
/*@-nullpass@*/
    if (ds != NULL)
	sprintf(buf, "%08d%c %s %s 0x%08x", ix, deptype,
		rpmdsN(ds), rpmdsEVR(ds), rpmdsFlags(ds));
/*@=nullpass@*/
    return buf;
};

/**
 * Run per-interpreter dependency helper.
 * @param fc		file classifier
 * @param deptype	'P' == Provides:, 'R' == Requires:, helper
 * @param nsdep		class name for interpreter (e.g. "perl")
 * @return		0 on success
 */
static int rpmfcHelper(rpmfc fc, unsigned char deptype, const char * nsdep)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies fc, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    const char * fn = fc->fn[fc->ix];
    char buf[BUFSIZ];
    StringBuf sb_stdout = NULL;
    StringBuf sb_stdin;
    const char *av[2];
    rpmds * depsp, ds;
    const char * N;
    const char * EVR;
    int_32 Flags, dsContext, tagN;
    ARGV_t pav;
    const char * s;
    int pac;
    int xx;
    int i;

    switch (deptype) {
    default:
	return -1;
	/*@notreached@*/ break;
    case 'P':
	if (fc->skipProv)
	    return 0;
	xx = snprintf(buf, sizeof(buf), "%%{?__%s_provides}", nsdep);
	depsp = &fc->provides;
	dsContext = RPMSENSE_FIND_PROVIDES;
	tagN = RPMTAG_PROVIDENAME;
	break;
    case 'R':
	if (fc->skipReq)
	    return 0;
	xx = snprintf(buf, sizeof(buf), "%%{?__%s_requires}", nsdep);
	depsp = &fc->requires;
	dsContext = RPMSENSE_FIND_REQUIRES;
	tagN = RPMTAG_REQUIRENAME;
	break;
    }
    buf[sizeof(buf)-1] = '\0';
    av[0] = buf;
    av[1] = NULL;

    sb_stdin = newStringBuf();
    appendLineStringBuf(sb_stdin, fn);
    sb_stdout = NULL;
/*@-boundswrite@*/
    xx = rpmfcExec(av, sb_stdin, &sb_stdout, 0);
/*@=boundswrite@*/
    sb_stdin = freeStringBuf(sb_stdin);

    if (xx == 0 && sb_stdout != NULL) {
	pav = NULL;
	xx = argvSplit(&pav, getStringBuf(sb_stdout), " \t\n\r");
	pac = argvCount(pav);
	if (pav)
	for (i = 0; i < pac; i++) {
	    N = pav[i];
	    EVR = "";
	    Flags = dsContext;
/*@-branchstate@*/
	    if (pav[i+1] && strchr("=<>", *pav[i+1])) {
		i++;
		for (s = pav[i]; *s; s++) {
		    switch(*s) {
		    default:
assert(*s != '\0');
			/*@switchbreak@*/ break;
		    case '=':
			Flags |= RPMSENSE_EQUAL;
			/*@switchbreak@*/ break;
		    case '<':
			Flags |= RPMSENSE_LESS;
			/*@switchbreak@*/ break;
		    case '>':
			Flags |= RPMSENSE_GREATER;
			/*@switchbreak@*/ break;
		    }
		}
		i++;
		EVR = pav[i];
assert(EVR != NULL);
	    }
/*@=branchstate@*/


	    /* Add tracking dependency for versioned Provides: */
	    if (!fc->tracked && deptype == 'P' && *EVR != '\0') {
		ds = rpmdsSingle(RPMTAG_REQUIRENAME,
			"rpmlib(VersionedDependencies)", "3.0.3-1",
			RPMSENSE_RPMLIB|(RPMSENSE_LESS|RPMSENSE_EQUAL));
		xx = rpmdsMerge(&fc->requires, ds);
		ds = rpmdsFree(ds);
		fc->tracked = 1;
	    }

	    ds = rpmdsSingle(tagN, N, EVR, Flags);

	    /* Add to package dependencies. */
	    xx = rpmdsMerge(depsp, ds);

	    /* Add to file dependencies. */
/*@-boundswrite@*/
	    xx = rpmfcSaveArg(&fc->ddict, rpmfcFileDep(buf, fc->ix, ds));
/*@=boundswrite@*/

	    ds = rpmdsFree(ds);
	}

	pav = argvFree(pav);
    }
    sb_stdout = freeStringBuf(sb_stdout);

    return 0;
}

/**
 */
/*@unchecked@*/ /*@observer@*/
static struct rpmfcTokens_s rpmfcTokens[] = {
  { "directory",		RPMFC_DIRECTORY|RPMFC_INCLUDE },

  { " shared object",		RPMFC_LIBRARY },
  { " executable",		RPMFC_EXECUTABLE },
  { " statically linked",	RPMFC_STATIC },
  { " not stripped",		RPMFC_NOTSTRIPPED },
  { " archive",			RPMFC_ARCHIVE },

  { "ELF 32-bit",		RPMFC_ELF32|RPMFC_INCLUDE },
  { "ELF 64-bit",		RPMFC_ELF64|RPMFC_INCLUDE },

  { " script",			RPMFC_SCRIPT },
  { " text",			RPMFC_TEXT },
  { " document",		RPMFC_DOCUMENT },

  { " compressed",		RPMFC_COMPRESSED },

  { "troff or preprocessor input",	RPMFC_MANPAGE|RPMFC_INCLUDE },
  { "GNU Info",			RPMFC_MANPAGE|RPMFC_INCLUDE },

  { "perl script text",		RPMFC_PERL|RPMFC_INCLUDE },
  { "Perl5 module source text", RPMFC_PERL|RPMFC_MODULE|RPMFC_INCLUDE },

  /* XXX "a /usr/bin/python -t script text executable" */
  /* XXX "python 2.3 byte-compiled" */
  { " /usr/bin/python",		RPMFC_PYTHON|RPMFC_INCLUDE },
  { "python ",			RPMFC_PYTHON|RPMFC_INCLUDE },

  { "libtool library ",		RPMFC_LIBTOOL|RPMFC_INCLUDE },
  { "pkgconfig ",		RPMFC_PKGCONFIG|RPMFC_INCLUDE },

  { "Bourne ",			RPMFC_BOURNE|RPMFC_INCLUDE },
  { "Bourne-Again ",		RPMFC_BOURNE|RPMFC_INCLUDE },

  { "Java ",			RPMFC_JAVA|RPMFC_INCLUDE },

  { "current ar archive",	RPMFC_STATIC|RPMFC_LIBRARY|RPMFC_ARCHIVE|RPMFC_INCLUDE },

  { "Zip archive data",		RPMFC_COMPRESSED|RPMFC_ARCHIVE|RPMFC_INCLUDE },
  { "tar archive",		RPMFC_ARCHIVE|RPMFC_INCLUDE },
  { "cpio archive",		RPMFC_ARCHIVE|RPMFC_INCLUDE },
  { "RPM v3",			RPMFC_ARCHIVE|RPMFC_INCLUDE },
  { "RPM v4",			RPMFC_ARCHIVE|RPMFC_INCLUDE },

  { " image",			RPMFC_IMAGE|RPMFC_INCLUDE },
  { " font",			RPMFC_FONT|RPMFC_INCLUDE },
  { " Font",			RPMFC_FONT|RPMFC_INCLUDE },

  { " commands",		RPMFC_SCRIPT|RPMFC_INCLUDE },
  { " script",			RPMFC_SCRIPT|RPMFC_INCLUDE },

  { "empty",			RPMFC_WHITE|RPMFC_INCLUDE },

  { "HTML",			RPMFC_WHITE|RPMFC_INCLUDE },
  { "SGML",			RPMFC_WHITE|RPMFC_INCLUDE },
  { "XML",			RPMFC_WHITE|RPMFC_INCLUDE },

  { " program text",		RPMFC_WHITE|RPMFC_INCLUDE },
  { " source",			RPMFC_WHITE|RPMFC_INCLUDE },
  { "GLS_BINARY_LSB_FIRST",	RPMFC_WHITE|RPMFC_INCLUDE },
  { " DB ",			RPMFC_WHITE|RPMFC_INCLUDE },

  { "ASCII English text",	RPMFC_WHITE|RPMFC_INCLUDE },
  { "ASCII text",		RPMFC_WHITE|RPMFC_INCLUDE },
  { "ISO-8859 text",		RPMFC_WHITE|RPMFC_INCLUDE },

  { "symbolic link to",		RPMFC_SYMLINK },
  { "socket",			RPMFC_DEVICE },
  { "special",			RPMFC_DEVICE },

  { "ASCII",			RPMFC_WHITE },
  { "ISO-8859",			RPMFC_WHITE },

  { "data",			RPMFC_WHITE },

  { "application",		RPMFC_WHITE },
  { "boot",			RPMFC_WHITE },
  { "catalog",			RPMFC_WHITE },
  { "code",			RPMFC_WHITE },
  { "file",			RPMFC_WHITE },
  { "format",			RPMFC_WHITE },
  { "message",			RPMFC_WHITE },
  { "program",			RPMFC_WHITE },

  { "broken symbolic link to ",	RPMFC_WHITE|RPMFC_ERROR },
  { "can't read",		RPMFC_WHITE|RPMFC_ERROR },
  { "can't stat",		RPMFC_WHITE|RPMFC_ERROR },
  { "executable, can't read",	RPMFC_WHITE|RPMFC_ERROR },
  { "core file",		RPMFC_WHITE|RPMFC_ERROR },

  { NULL,			RPMFC_BLACK }
};

int rpmfcColoring(const char * fmstr)
{
    rpmfcToken fct;
    int fcolor = RPMFC_BLACK;

    for (fct = rpmfcTokens; fct->token != NULL; fct++) {
	if (strstr(fmstr, fct->token) == NULL)
	    continue;
	fcolor |= fct->colors;
	if (fcolor & RPMFC_INCLUDE)
	    return fcolor;
    }
    return fcolor;
}

void rpmfcPrint(const char * msg, rpmfc fc, FILE * fp)
{
    int fcolor;
    int ndx;
    int cx;
    int dx;
    int fx;

int nprovides;
int nrequires;

    if (fp == NULL) fp = stderr;

    if (msg)
	fprintf(fp, "===================================== %s\n", msg);

nprovides = rpmdsCount(fc->provides);
nrequires = rpmdsCount(fc->requires);

    if (fc)
    for (fx = 0; fx < fc->nfiles; fx++) {
assert(fx < fc->fcdictx->nvals);
	cx = fc->fcdictx->vals[fx];
assert(fx < fc->fcolor->nvals);
	fcolor = fc->fcolor->vals[fx];

	fprintf(fp, "%3d %s", fx, fc->fn[fx]);
	if (fcolor != RPMFC_BLACK)
		fprintf(fp, "\t0x%x", fc->fcolor->vals[fx]);
	else
		fprintf(fp, "\t%s", fc->cdict[cx]);
	fprintf(fp, "\n");

	if (fc->fddictx == NULL || fc->fddictn == NULL)
	    continue;

assert(fx < fc->fddictx->nvals);
	dx = fc->fddictx->vals[fx];
assert(fx < fc->fddictn->nvals);
	ndx = fc->fddictn->vals[fx];

	while (ndx-- > 0) {
	    const char * depval;
	    unsigned char deptype;
	    unsigned ix;

	    ix = fc->ddictx->vals[dx++];
	    deptype = ((ix >> 24) & 0xff);
	    ix &= 0x00ffffff;
	    depval = NULL;
	    switch (deptype) {
	    default:
assert(depval != NULL);
		/*@switchbreak@*/ break;
	    case 'P':
		if (nprovides > 0) {
assert(ix < nprovides);
		    (void) rpmdsSetIx(fc->provides, ix-1);
		    if (rpmdsNext(fc->provides) >= 0)
			depval = rpmdsDNEVR(fc->provides);
		}
		/*@switchbreak@*/ break;
	    case 'R':
		if (nrequires > 0) {
assert(ix < nrequires);
		    (void) rpmdsSetIx(fc->requires, ix-1);
		    if (rpmdsNext(fc->requires) >= 0)
			depval = rpmdsDNEVR(fc->requires);
		}
		/*@switchbreak@*/ break;
	    }
	    if (depval)
		fprintf(fp, "\t%s\n", depval);
	}
    }
}

rpmfc rpmfcFree(rpmfc fc)
{
    if (fc) {
	fc->fn = argvFree(fc->fn);
	fc->fcolor = argiFree(fc->fcolor);
	fc->fcdictx = argiFree(fc->fcdictx);
	fc->fddictx = argiFree(fc->fddictx);
	fc->fddictn = argiFree(fc->fddictn);
	fc->cdict = argvFree(fc->cdict);
	fc->ddict = argvFree(fc->ddict);
	fc->ddictx = argiFree(fc->ddictx);

	fc->provides = rpmdsFree(fc->provides);
	fc->requires = rpmdsFree(fc->requires);

	fc->sb_java = freeStringBuf(fc->sb_java);
	fc->sb_perl = freeStringBuf(fc->sb_perl);
	fc->sb_python = freeStringBuf(fc->sb_python);

    }
    fc = _free(fc);
    return NULL;
}

rpmfc rpmfcNew(void)
{
    rpmfc fc = xcalloc(1, sizeof(*fc));
    return fc;
}

/**
 * Extract script dependencies.
 * @param fc		file classifier
 * @return		0 on success
 */
static int rpmfcSCRIPT(rpmfc fc)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies fc, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    const char * fn = fc->fn[fc->ix];
    const char * bn;
    rpmds ds;
    char buf[BUFSIZ];
    FILE * fp;
    char * s, * se;
    int i;
    int is_executable;
    int xx;

    /* Extract dependencies only from files with executable bit set. */
    {	struct stat sb, * st = &sb;
	if (stat(fn, st) != 0)
	    return -1;
	is_executable = (st->st_mode & (S_IXUSR|S_IXGRP|S_IXOTH));
    }

    fp = fopen(fn, "r");
    if (fp == NULL || ferror(fp)) {
	if (fp) (void) fclose(fp);
	return -1;
    }

    /* Look for #! interpreter in first 10 lines. */
/*@-boundswrite@*/
    for (i = 0; i < 10; i++) {

	s = fgets(buf, sizeof(buf) - 1, fp);
	if (s == NULL || ferror(fp) || feof(fp))
	    break;
	s[sizeof(buf)-1] = '\0';
	if (!(s[0] == '#' && s[1] == '!'))
	    continue;
	s += 2;

	while (*s && strchr(" \t\n\r", *s) != NULL)
	    s++;
	if (*s == '\0')
	    continue;
	if (*s != '/')
	    continue;

	for (se = s+1; *se; se++) {
	    if (strchr(" \t\n\r", *se) != NULL)
		/*@innerbreak@*/ break;
	}
	*se = '\0';
	se++;

	if (is_executable) {
	    /* Add to package requires. */
	    ds = rpmdsSingle(RPMTAG_REQUIRENAME, s, "", RPMSENSE_FIND_REQUIRES);
	    xx = rpmdsMerge(&fc->requires, ds);

	    /* Add to file requires. */
	    xx = rpmfcSaveArg(&fc->ddict, rpmfcFileDep(se, fc->ix, ds));

	    ds = rpmdsFree(ds);
	}

	/* Set color based on interpreter name. */
	/* XXX magic token should have already done this?!? */
	bn = basename(s);
	if (!strcmp(bn, "perl"))
	    fc->fcolor->vals[fc->ix] |= RPMFC_PERL;
	else if (!strncmp(bn, "python", sizeof("python")-1))
	    fc->fcolor->vals[fc->ix] |= RPMFC_PYTHON;

	break;
    }
/*@=boundswrite@*/

    (void) fclose(fp);

    if (fc->fcolor->vals[fc->ix] & RPMFC_PERL) {
	if (fc->fcolor->vals[fc->ix] & RPMFC_MODULE)
	    xx = rpmfcHelper(fc, 'P', "perl");
	if (is_executable || (fc->fcolor->vals[fc->ix] & RPMFC_MODULE))
	    xx = rpmfcHelper(fc, 'R', "perl");
    } else
    if (fc->fcolor->vals[fc->ix] & RPMFC_PYTHON) {
	xx = rpmfcHelper(fc, 'P', "python");
#ifdef	NOTYET
	if (is_executable)
#endif
	    xx = rpmfcHelper(fc, 'R', "python");
    } else
    if (fc->fcolor->vals[fc->ix] & RPMFC_LIBTOOL) {
	xx = rpmfcHelper(fc, 'P', "libtool");
#ifdef	NOTYET
	if (is_executable)
#endif
	    xx = rpmfcHelper(fc, 'R', "libtool");
    } else
    if (fc->fcolor->vals[fc->ix] & RPMFC_PKGCONFIG) {
	xx = rpmfcHelper(fc, 'P', "pkgconfig");
#ifdef	NOTYET
	if (is_executable)
#endif
	    xx = rpmfcHelper(fc, 'R', "pkgconfig");
    } else
    if (fc->fcolor->vals[fc->ix] & RPMFC_BOURNE) {
#ifdef	NOTYET
	xx = rpmfcHelper(fc, 'P', "executable");
#endif
	if (is_executable)
	    xx = rpmfcHelper(fc, 'R', "executable");
    }

    return 0;
}


/**
 * Merge provides/requires dependencies into a rpmfc container.
 * @param context	merge dependency set(s) container
 * @param ds		dependency set to merge
 * @return		0 on success
 */
static int rpmfcMergePR(void * context, rpmds ds)
	/*@modifies ds @*/
{
    rpmfc fc = context;
    char buf[BUFSIZ];
    int rc = -1;

/*@-modfilesys@*/
if (_rpmfc_debug < 0)
fprintf(stderr, "*** %s(%p, %p) %s\n", __FUNCTION__, context, ds, tagName(rpmdsTagN(ds)));
/*@=modfilesys@*/
    switch(rpmdsTagN(ds)) {
    default:
	break;
    case RPMTAG_PROVIDENAME:
	/* Add to package provides. */
	rc = rpmdsMerge(&fc->provides, ds);

	/* Add to file dependencies. */
	buf[0] = '\0';
	rc = rpmfcSaveArg(&fc->ddict, rpmfcFileDep(buf, fc->ix, ds));
	break;
    case RPMTAG_REQUIRENAME:
	/* Add to package requires. */
	rc = rpmdsMerge(&fc->requires, ds);

	/* Add to file dependencies. */
	buf[0] = '\0';
	rc = rpmfcSaveArg(&fc->ddict, rpmfcFileDep(buf, fc->ix, ds));
	break;
    }
    return rc;
}

/**
 * Extract Elf dependencies.
 * @param fc		file classifier
 * @return		0 on success
 */
static int rpmfcELF(rpmfc fc)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext, fileSystem, internalState @*/
{
    const char * fn = fc->fn[fc->ix];
    int flags = 0;

    if (fc->skipProv)
	flags |= RPMELF_FLAG_SKIPPROVIDES;
    if (fc->skipReq)
	flags |= RPMELF_FLAG_SKIPREQUIRES;

    return rpmdsELF(fn, flags, rpmfcMergePR, fc);
}

typedef struct rpmfcApplyTbl_s {
    int (*func) (rpmfc fc);
    int colormask;
} * rpmfcApplyTbl;

/**
 * XXX Having two entries for rpmfcSCRIPT may be unnecessary duplication.
 */
/*@unchecked@*/
static struct rpmfcApplyTbl_s rpmfcApplyTable[] = {
    { rpmfcELF,		RPMFC_ELF },
    { rpmfcSCRIPT,	(RPMFC_SCRIPT|RPMFC_PERL|RPMFC_PYTHON|RPMFC_LIBTOOL|RPMFC_PKGCONFIG|RPMFC_BOURNE|RPMFC_JAVA) },
    { NULL, 0 }
};

int rpmfcApply(rpmfc fc)
{
    rpmfcApplyTbl fcat;
    const char * s;
    char * se;
    rpmds ds;
    const char * N;
    const char * EVR;
    int_32 Flags;
    unsigned char deptype;
    int nddict;
    int previx;
    unsigned int val;
    int dix;
    int ix;
    int i;
    int xx;
    int skipping;

    /* Generate package and per-file dependencies. */
    for (fc->ix = 0; fc->fn[fc->ix] != NULL; fc->ix++) {

	/* XXX Insure that /usr/lib{,64}/python files are marked RPMFC_PYTHON */
	/* XXX HACK: classification by path is intrinsically stupid. */
	{   const char *fn = strstr(fc->fn[fc->ix], "/usr/lib");
	    if (fn) {
		fn += sizeof("/usr/lib")-1;
		if (fn[0] == '6' && fn[1] == '4')
		    fn += 2;
		if (!strncmp(fn, "/python", sizeof("/python")-1))
		    fc->fcolor->vals[fc->ix] |= RPMFC_PYTHON;
	    }
	}

	if (fc->fcolor->vals[fc->ix])
	for (fcat = rpmfcApplyTable; fcat->func != NULL; fcat++) {
	    if (!(fc->fcolor->vals[fc->ix] & fcat->colormask))
		/*@innercontinue@*/ continue;
	    xx = (*fcat->func) (fc);
	}
    }

/*@-boundswrite@*/
    /* Generate per-file indices into package dependencies. */
    nddict = argvCount(fc->ddict);
    previx = -1;
    for (i = 0; i < nddict; i++) {
	s = fc->ddict[i];

	/* Parse out (file#,deptype,N,EVR,Flags) */
	ix = strtol(s, &se, 10);
assert(se != NULL);
	deptype = *se++;
	se++;
	N = se;
	while (*se && *se != ' ')
	    se++;
	*se++ = '\0';
	EVR = se;
	while (*se && *se != ' ')
	    se++;
	*se++ = '\0';
	Flags = strtol(se, NULL, 16);

	dix = -1;
	skipping = 0;
	switch (deptype) {
	default:
	    /*@switchbreak@*/ break;
	case 'P':	
	    skipping = fc->skipProv;
	    ds = rpmdsSingle(RPMTAG_PROVIDENAME, N, EVR, Flags);
	    dix = rpmdsFind(fc->provides, ds);
	    ds = rpmdsFree(ds);
	    /*@switchbreak@*/ break;
	case 'R':
	    skipping = fc->skipReq;
	    ds = rpmdsSingle(RPMTAG_REQUIRENAME, N, EVR, Flags);
	    dix = rpmdsFind(fc->requires, ds);
	    ds = rpmdsFree(ds);
	    /*@switchbreak@*/ break;
	}

/* XXX assertion incorrect while generating -debuginfo deps. */
#if 0
assert(dix >= 0);
#else
	if (dix < 0)
	    continue;
#endif

	val = (deptype << 24) | (dix & 0x00ffffff);
	xx = argiAdd(&fc->ddictx, -1, val);

	if (previx != ix) {
	    previx = ix;
	    xx = argiAdd(&fc->fddictx, ix, argiCount(fc->ddictx)-1);
	}
	if (fc->fddictn && fc->fddictn->vals && !skipping)
	    fc->fddictn->vals[ix]++;
    }
/*@=boundswrite@*/

    return 0;
}

int rpmfcClassify(rpmfc fc, ARGV_t argv, int_16 * fmode)
{
    ARGV_t fcav = NULL;
    ARGV_t dav;
    const char * s, * se;
    size_t slen;
    int fcolor;
    int xx;
    const char * magicfile;
    int msflags = MAGIC_CHECK;	/* XXX MAGIC_COMPRESS flag? */
    magic_t ms = NULL;

    if (fc == NULL || argv == NULL)
	return 0;

    magicfile = rpmExpand("%{?_rpmfc_magic_path}", NULL);
    if (magicfile == NULL || *magicfile == '\0' || *magicfile == '%')
	goto exit;

    fc->nfiles = argvCount(argv);

    /* Initialize the per-file dictionary indices. */
    xx = argiAdd(&fc->fddictx, fc->nfiles-1, 0);
    xx = argiAdd(&fc->fddictn, fc->nfiles-1, 0);

    /* Build (sorted) file class dictionary. */
    xx = argvAdd(&fc->cdict, "");
    xx = argvAdd(&fc->cdict, "directory");

    ms = magic_open(msflags);
    if (ms == NULL) {
	xx = RPMERR_EXEC;
	rpmError(xx, _("magic_open(0x%x) failed: %s\n"),
		msflags, strerror(errno));
assert(ms != NULL);	/* XXX figger a proper return path. */
    }

    xx = magic_load(ms, magicfile);
    if (xx == -1) {
	xx = RPMERR_EXEC;
	rpmError(xx, _("magic_load(ms, \"%s\") failed: %s\n"),
		magicfile, magic_error(ms));
assert(xx != -1);	/* XXX figger a proper return path. */
    }

    for (fc->ix = 0; fc->ix < fc->nfiles; fc->ix++) {
	const char * ftype;
	int_16 mode = (fmode ? fmode[fc->ix] : 0);
	int urltype;

	urltype = urlPath(argv[fc->ix], &s);
assert(s != NULL && *s == '/');
	slen = strlen(s);

/*@-branchstate@*/
	switch (mode & S_IFMT) {
	case S_IFCHR:	ftype = "character special";	/*@switchbreak@*/ break;
	case S_IFBLK:	ftype = "block special";	/*@switchbreak@*/ break;
#if defined(S_IFIFO)
	case S_IFIFO:	ftype = "fifo (named pipe)";	/*@switchbreak@*/ break;
#endif
#if defined(S_IFSOCK)
/*@-unrecog@*/
	case S_IFSOCK:	ftype = "socket";		/*@switchbreak@*/ break;
/*@=unrecog@*/
#endif
	case S_IFDIR:
	case S_IFLNK:
	case S_IFREG:
	default:
	    /* XXX all files with extension ".pm" are perl modules for now. */
	    if (slen >= sizeof(".pm") && !strcmp(s+slen-(sizeof(".pm")-1), ".pm"))
		ftype = "Perl5 module source text";

	    /* XXX all files with extension ".jar" are java for now. */
	    else if (slen >= sizeof(".jar") && !strcmp(s+slen-(sizeof(".jar")-1), ".pm"))
		ftype = "Java archive file";

	    /* XXX all files with extension ".jar" are java for now. */
	    else if (slen >= sizeof(".class") && !strcmp(s+slen-(sizeof(".class")-1), ".pm"))
		ftype = "Java class file";

	    /* XXX all files with extension ".la" are libtool for now. */
	    else if (slen >= sizeof(".la") && !strcmp(s+slen-(sizeof(".la")-1), ".pm"))
		ftype = "libtool library file";

	    /* XXX all files with extension ".pc" are pkgconfig for now. */
	    else if (slen >= sizeof(".pc") && !strcmp(s+slen-(sizeof(".pc")-1), ".pm"))
		ftype = "pkgconfig file";

	    /* XXX skip all files in /dev/ which are (or should be) %dev dummies. */
	    else if (slen >= fc->brlen+sizeof("/dev/") && !strncmp(s+fc->brlen, "/dev/", sizeof("/dev/")-1))
		ftype = "";
	    else
		ftype = magic_file(ms, s);

	    if (ftype == NULL) {
		xx = RPMERR_EXEC;
		rpmError(xx, _("magic_file(ms, \"%s\") failed: mode %06o %s\n"),
			s, mode, magic_error(ms));
assert(ftype != NULL);	/* XXX figger a proper return path. */
	    }
	    /*@switchbreak@*/ break;
	}
/*@=branchstate@*/

	se = ftype;
        rpmMessage(RPMMESS_DEBUG, "%s: %s\n", s, se);

	/* Save the path. */
	xx = argvAdd(&fc->fn, s);

	/* Save the file type string. */
	xx = argvAdd(&fcav, se);

	/* Add (filtered) entry to sorted class dictionary. */
	fcolor = rpmfcColoring(se);
	xx = argiAdd(&fc->fcolor, fc->ix, fcolor);

/*@-boundswrite@*/
	if (fcolor != RPMFC_WHITE && (fcolor & RPMFC_INCLUDE))
	    xx = rpmfcSaveArg(&fc->cdict, se);
/*@=boundswrite@*/
    }

    /* Build per-file class index array. */
    fc->fknown = 0;
    for (fc->ix = 0; fc->ix < fc->nfiles; fc->ix++) {
	se = fcav[fc->ix];
assert(se != NULL);

	dav = argvSearch(fc->cdict, se, NULL);
	if (dav) {
	    xx = argiAdd(&fc->fcdictx, fc->ix, (dav - fc->cdict));
	    fc->fknown++;
	} else {
	    xx = argiAdd(&fc->fcdictx, fc->ix, 0);
	    fc->fwhite++;
	}
    }

    fcav = argvFree(fcav);

    if (ms != NULL)
	magic_close(ms);

exit:
    magicfile = _free(magicfile);

    return 0;
}

/**
 */
typedef struct DepMsg_s * DepMsg_t;

/**
 */
struct DepMsg_s {
/*@observer@*/ /*@null@*/
    const char * msg;
/*@observer@*/
    const char * argv[4];
    rpmTag ntag;
    rpmTag vtag;
    rpmTag ftag;
    int mask;
    int xor;
};

/**
 */
/*@unchecked@*/
static struct DepMsg_s depMsgs[] = {
  { "Provides",		{ "%{?__find_provides}", NULL, NULL, NULL },
	RPMTAG_PROVIDENAME, RPMTAG_PROVIDEVERSION, RPMTAG_PROVIDEFLAGS,
	0, -1 },
  { "Requires(interp)",	{ NULL, "interp", NULL, NULL },
	RPMTAG_REQUIRENAME, RPMTAG_REQUIREVERSION, RPMTAG_REQUIREFLAGS,
	_notpre(RPMSENSE_INTERP), 0 },
  { "Requires(rpmlib)",	{ NULL, "rpmlib", NULL, NULL },
	-1, -1, RPMTAG_REQUIREFLAGS,
	_notpre(RPMSENSE_RPMLIB), 0 },
  { "Requires(verify)",	{ NULL, "verify", NULL, NULL },
	-1, -1, RPMTAG_REQUIREFLAGS,
	RPMSENSE_SCRIPT_VERIFY, 0 },
  { "Requires(pre)",	{ NULL, "pre", NULL, NULL },
	-1, -1, RPMTAG_REQUIREFLAGS,
	_notpre(RPMSENSE_SCRIPT_PRE), 0 },
  { "Requires(post)",	{ NULL, "post", NULL, NULL },
	-1, -1, RPMTAG_REQUIREFLAGS,
	_notpre(RPMSENSE_SCRIPT_POST), 0 },
  { "Requires(preun)",	{ NULL, "preun", NULL, NULL },
	-1, -1, RPMTAG_REQUIREFLAGS,
	_notpre(RPMSENSE_SCRIPT_PREUN), 0 },
  { "Requires(postun)",	{ NULL, "postun", NULL, NULL },
	-1, -1, RPMTAG_REQUIREFLAGS,
	_notpre(RPMSENSE_SCRIPT_POSTUN), 0 },
  { "Requires",		{ "%{?__find_requires}", NULL, NULL, NULL },
	-1, -1, RPMTAG_REQUIREFLAGS,	/* XXX inherit name/version arrays */
	RPMSENSE_PREREQ, RPMSENSE_PREREQ },
  { "Conflicts",	{ "%{?__find_conflicts}", NULL, NULL, NULL },
	RPMTAG_CONFLICTNAME, RPMTAG_CONFLICTVERSION, RPMTAG_CONFLICTFLAGS,
	0, -1 },
  { "Obsoletes",	{ "%{?__find_obsoletes}", NULL, NULL, NULL },
	RPMTAG_OBSOLETENAME, RPMTAG_OBSOLETEVERSION, RPMTAG_OBSOLETEFLAGS,
	0, -1 },
  { NULL,		{ NULL, NULL, NULL, NULL },	0, 0, 0, 0, 0 }
};

/*@unchecked@*/
static DepMsg_t DepMsgs = depMsgs;

/**
 * Print dependencies in a header.
 * @param h		header
 */
static void printDeps(Header h)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies h, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    DepMsg_t dm;
    rpmds ds = NULL;
    int flags = 0x2;	/* XXX no filtering, !scareMem */
    const char * DNEVR;
    int_32 Flags;
    int bingo = 0;

    for (dm = DepMsgs; dm->msg != NULL; dm++) {
	if (dm->ntag != -1) {
	    ds = rpmdsFree(ds);
	    ds = rpmdsNew(h, dm->ntag, flags);
	}
	if (dm->ftag == 0)
	    continue;

	ds = rpmdsInit(ds);
	if (ds == NULL)
	    continue;	/* XXX can't happen */

	bingo = 0;
	while (rpmdsNext(ds) >= 0) {

	    Flags = rpmdsFlags(ds);
	
	    if (!((Flags & dm->mask) ^ dm->xor))
		/*@innercontinue@*/ continue;
	    if (bingo == 0) {
		rpmMessage(RPMMESS_NORMAL, "%s:", (dm->msg ? dm->msg : ""));
		bingo = 1;
	    }
	    if ((DNEVR = rpmdsDNEVR(ds)) == NULL)
		/*@innercontinue@*/ continue;	/* XXX can't happen */
	    rpmMessage(RPMMESS_NORMAL, " %s", DNEVR+2);
	}
	if (bingo)
	    rpmMessage(RPMMESS_NORMAL, "\n");
    }
    ds = rpmdsFree(ds);
}

/**
 */
static int rpmfcGenerateDependsHelper(const Spec spec, Package pkg, rpmfi fi)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
        /*@modifies fi, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    StringBuf sb_stdin;
    StringBuf sb_stdout;
    DepMsg_t dm;
    int failnonzero = 0;
    int rc = 0;

    /*
     * Create file manifest buffer to deliver to dependency finder.
     */
    sb_stdin = newStringBuf();
    fi = rpmfiInit(fi, 0);
    if (fi != NULL)
    while (rpmfiNext(fi) >= 0)
	appendLineStringBuf(sb_stdin, rpmfiFN(fi));

    for (dm = DepMsgs; dm->msg != NULL; dm++) {
	int tag, tagflags;
	char * s;
	int xx;

	tag = (dm->ftag > 0) ? dm->ftag : dm->ntag;
	tagflags = 0;
	s = NULL;

	switch(tag) {
	case RPMTAG_PROVIDEFLAGS:
	    if (!pkg->autoProv)
		continue;
	    failnonzero = 1;
	    tagflags = RPMSENSE_FIND_PROVIDES;
	    /*@switchbreak@*/ break;
	case RPMTAG_REQUIREFLAGS:
	    if (!pkg->autoReq)
		continue;
	    failnonzero = 0;
	    tagflags = RPMSENSE_FIND_REQUIRES;
	    /*@switchbreak@*/ break;
	default:
	    continue;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	}

/*@-boundswrite@*/
	xx = rpmfcExec(dm->argv, sb_stdin, &sb_stdout, failnonzero);
/*@=boundswrite@*/
	if (xx == -1)
	    continue;

	s = rpmExpand(dm->argv[0], NULL);
	rpmMessage(RPMMESS_NORMAL, _("Finding  %s: %s\n"), dm->msg,
		(s ? s : ""));
	s = _free(s);

	if (sb_stdout == NULL) {
	    rc = RPMERR_EXEC;
	    rpmError(rc, _("Failed to find %s:\n"), dm->msg);
	    break;
	}

	/* Parse dependencies into header */
	if (spec->_parseRCPOT)
	    rc = spec->_parseRCPOT(spec, pkg, getStringBuf(sb_stdout), tag,
				0, tagflags);
	sb_stdout = freeStringBuf(sb_stdout);

	if (rc) {
	    rpmError(rc, _("Failed to find %s:\n"), dm->msg);
	    break;
	}
    }

    sb_stdin = freeStringBuf(sb_stdin);

    return rc;
}

/**
 */
/*@unchecked@*/
static struct DepMsg_s scriptMsgs[] = {
  { "Requires(pre)",	{ "%{?__scriptlet_requires}", NULL, NULL, NULL },
	RPMTAG_PREINPROG, RPMTAG_PREIN, RPMTAG_REQUIREFLAGS,
	RPMSENSE_SCRIPT_PRE, 0 },
  { "Requires(post)",	{ "%{?__scriptlet_requires}", NULL, NULL, NULL },
	RPMTAG_POSTINPROG, RPMTAG_POSTIN, RPMTAG_REQUIREFLAGS,
	RPMSENSE_SCRIPT_POST, 0 },
  { "Requires(preun)",	{ "%{?__scriptlet_requires}", NULL, NULL, NULL },
	RPMTAG_PREUNPROG, RPMTAG_PREUN, RPMTAG_REQUIREFLAGS,
	RPMSENSE_SCRIPT_PREUN, 0 },
  { "Requires(postun)",	{ "%{?__scriptlet_requires}", NULL, NULL, NULL },
	RPMTAG_POSTUNPROG, RPMTAG_POSTUN, RPMTAG_REQUIREFLAGS,
	RPMSENSE_SCRIPT_POSTUN, 0 },
  { NULL,		{ NULL, NULL, NULL, NULL },	0, 0, 0, 0, 0 }
};

/*@unchecked@*/
static DepMsg_t ScriptMsgs = scriptMsgs;

/**
 */
static int rpmfcGenerateScriptletDeps(const Spec spec, Package pkg)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
        /*@modifies rpmGlobalMacroContext, fileSystem, internalState @*/
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    StringBuf sb_stdin = newStringBuf();
    StringBuf sb_stdout = NULL;
    DepMsg_t dm;
    int failnonzero = 0;
    int rc = 0;

/*@-branchstate@*/
    for (dm = ScriptMsgs; dm->msg != NULL; dm++) {
	int tag, tagflags;
	char * s;
	int xx;

	tag = dm->ftag;
	tagflags = RPMSENSE_FIND_REQUIRES | dm->mask;

	/* Retrieve scriptlet interpreter. */
	s = NULL;
	if (!hge(pkg->header, dm->ntag, NULL, (void **)&s, NULL) || s == NULL)
	    continue;
	if (strcmp(s, "/bin/sh") && strcmp(s, "/bin/bash"))
	    continue;

	/* Retrieve scriptlet body. */
	s = NULL;
	if (!hge(pkg->header, dm->vtag, NULL, (void **)&s, NULL) || s == NULL)
	    continue;
	truncStringBuf(sb_stdin);
	appendLineStringBuf(sb_stdin, s);
	stripTrailingBlanksStringBuf(sb_stdin);

/*@-boundswrite@*/
	xx = rpmfcExec(dm->argv, sb_stdin, &sb_stdout, failnonzero);
/*@=boundswrite@*/
	if (xx == -1)
	    continue;

	/* Parse dependencies into header */
	s = getStringBuf(sb_stdout);
	if (s != NULL && *s != '\0') {
	    char * se = s;
	    /* XXX Convert "executable(/path/to/file)" to "/path/to/file". */
	    while ((se = strstr(se, "executable(/")) != NULL) {
/*@-modobserver@*/	/* FIX: getStringBuf should not be observer */
		se = stpcpy(se,     "           ");
		*se = '/';	/* XXX stpcpy truncates the '/' */
/*@=modobserver@*/
		se = strchr(se, ')');
		if (se == NULL)
		    /*@innerbreak@*/ break;
		*se++ = ' ';
	    }
	    if (spec->_parseRCPOT)
		rc = spec->_parseRCPOT(spec, pkg, s, tag, 0, tagflags);
	}
	sb_stdout = freeStringBuf(sb_stdout);

    }
/*@=branchstate@*/

    sb_stdin = freeStringBuf(sb_stdin);

    return rc;
}

int rpmfcGenerateDepends(void * specp, void * pkgp)
{
    const Spec spec = specp;
    Package pkg = pkgp;
    rpmfi fi = pkg->cpioList;
    rpmfc fc = NULL;
    rpmds ds;
    int flags = 0x2;	/* XXX no filtering, !scareMem */
    ARGV_t av;
    int_16 * fmode;
    int ac = rpmfiFC(fi);
    const void ** p;
    char buf[BUFSIZ];
    const char * N;
    const char * EVR;
    int genConfigDeps, internaldeps;
    int c;
    int rc = 0;
    int xx;

    /* Skip packages with no files. */
    if (ac <= 0)
	return 0;

    /* Skip packages that have dependency generation disabled. */
    if (! (pkg->autoReq || pkg->autoProv))
	return 0;

    /* If new-fangled dependency generation is disabled ... */
    internaldeps = rpmExpandNumeric("%{?_use_internal_dependency_generator}");
    if (internaldeps == 0) {
	/* ... then generate dependencies using %{__find_requires} et al. */
	rc = rpmfcGenerateDependsHelper(spec, pkg, fi);
	printDeps(pkg->header);
	return rc;
    }

    /* Generate scriptlet Dependencies. */
    if (internaldeps > 1)
	xx = rpmfcGenerateScriptletDeps(spec, pkg);

    /* Extract absolute file paths in argv format. */
    av = xcalloc(ac+1, sizeof(*av));
    fmode = xcalloc(ac+1, sizeof(*fmode));

/*@-boundswrite@*/
    genConfigDeps = 0;
    fi = rpmfiInit(fi, 0);
    if (fi != NULL)
    while ((c = rpmfiNext(fi)) >= 0) {
	rpmfileAttrs fileAttrs;

	/* Does package have any %config files? */
	fileAttrs = rpmfiFFlags(fi);
	genConfigDeps |= (fileAttrs & RPMFILE_CONFIG);

	av[c] = xstrdup(rpmfiFN(fi));
	fmode[c] = rpmfiFMode(fi);
    }
    av[ac] = NULL;
/*@=boundswrite@*/

    fc = rpmfcNew();
    fc->skipProv = !pkg->autoProv;
    fc->skipReq = !pkg->autoReq;
    fc->tracked = 0;

    {	const char * buildRootURL;
	const char * buildRoot;
	buildRootURL = rpmGenPath(spec->rootURL, "%{?buildroot}", NULL);
	(void) urlPath(buildRootURL, &buildRoot);
	if (buildRoot && !strcmp(buildRoot, "/")) buildRoot = NULL;
	fc->brlen = (buildRoot ? strlen(buildRoot) : 0);
	buildRootURL = _free(buildRootURL);
    }

    /* Copy (and delete) manually generated dependencies to dictionary. */
    if (!fc->skipProv) {
	ds = rpmdsNew(pkg->header, RPMTAG_PROVIDENAME, flags);
	xx = rpmdsMerge(&fc->provides, ds);
	ds = rpmdsFree(ds);
	xx = headerRemoveEntry(pkg->header, RPMTAG_PROVIDENAME);
	xx = headerRemoveEntry(pkg->header, RPMTAG_PROVIDEVERSION);
	xx = headerRemoveEntry(pkg->header, RPMTAG_PROVIDEFLAGS);

	/* Add config dependency, Provides: config(N) = EVR */
	if (genConfigDeps) {
	    N = rpmdsN(pkg->ds);
assert(N != NULL);
	    EVR = rpmdsEVR(pkg->ds);
assert(EVR != NULL);
	    sprintf(buf, "config(%s)", N);
	    ds = rpmdsSingle(RPMTAG_PROVIDENAME, buf, EVR,
			(RPMSENSE_EQUAL|RPMSENSE_CONFIG));
	    xx = rpmdsMerge(&fc->provides, ds);
	    ds = rpmdsFree(ds);
	}
    }

    if (!fc->skipReq) {
	ds = rpmdsNew(pkg->header, RPMTAG_REQUIRENAME, flags);
	xx = rpmdsMerge(&fc->requires, ds);
	ds = rpmdsFree(ds);
	xx = headerRemoveEntry(pkg->header, RPMTAG_REQUIRENAME);
	xx = headerRemoveEntry(pkg->header, RPMTAG_REQUIREVERSION);
	xx = headerRemoveEntry(pkg->header, RPMTAG_REQUIREFLAGS);

	/* Add config dependency,  Requires: config(N) = EVR */
	if (genConfigDeps) {
	    N = rpmdsN(pkg->ds);
assert(N != NULL);
	    EVR = rpmdsEVR(pkg->ds);
assert(EVR != NULL);
	    sprintf(buf, "config(%s)", N);
	    ds = rpmdsSingle(RPMTAG_REQUIRENAME, buf, EVR,
			(RPMSENSE_EQUAL|RPMSENSE_CONFIG));
	    xx = rpmdsMerge(&fc->requires, ds);
	    ds = rpmdsFree(ds);
	}
    }

    /* Build file class dictionary. */
    xx = rpmfcClassify(fc, av, fmode);

    /* Build file/package dependency dictionary. */
    xx = rpmfcApply(fc);

    /* Add per-file colors(#files) */
    p = (const void **) argiData(fc->fcolor);
    c = argiCount(fc->fcolor);
assert(ac == c);
    if (p != NULL && c > 0) {
	int_32 * fcolors = (int_32 *)p;
	int i;

	/* XXX Make sure only primary (i.e. Elf32/Elf64) colors are added. */
	for (i = 0; i < c; i++)
	    fcolors[i] &= 0x0f;
	xx = headerAddEntry(pkg->header, RPMTAG_FILECOLORS, RPM_INT32_TYPE,
			p, c);
    }

    /* Add classes(#classes) */
    p = (const void **) argvData(fc->cdict);
    c = argvCount(fc->cdict);
    if (p != NULL && c > 0)
	xx = headerAddEntry(pkg->header, RPMTAG_CLASSDICT, RPM_STRING_ARRAY_TYPE,
			p, c);

    /* Add per-file classes(#files) */
    p = (const void **) argiData(fc->fcdictx);
    c = argiCount(fc->fcdictx);
assert(ac == c);
    if (p != NULL && c > 0)
	xx = headerAddEntry(pkg->header, RPMTAG_FILECLASS, RPM_INT32_TYPE,
			p, c);

    /* Add Provides: */
/*@-branchstate@*/
    if (fc->provides != NULL && (c = rpmdsCount(fc->provides)) > 0 && !fc->skipProv) {
	p = (const void **) fc->provides->N;
	xx = headerAddEntry(pkg->header, RPMTAG_PROVIDENAME, RPM_STRING_ARRAY_TYPE,
			p, c);
	/* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
/*@-nullpass@*/
	p = (const void **) fc->provides->EVR;
assert(p != NULL);
	xx = headerAddEntry(pkg->header, RPMTAG_PROVIDEVERSION, RPM_STRING_ARRAY_TYPE,
			p, c);
	p = (const void **) fc->provides->Flags;
assert(p != NULL);
	xx = headerAddEntry(pkg->header, RPMTAG_PROVIDEFLAGS, RPM_INT32_TYPE,
			p, c);
/*@=nullpass@*/
    }
/*@=branchstate@*/

    /* Add Requires: */
/*@-branchstate@*/
    if (fc->requires != NULL && (c = rpmdsCount(fc->requires)) > 0 && !fc->skipReq) {
	p = (const void **) fc->requires->N;
	xx = headerAddEntry(pkg->header, RPMTAG_REQUIRENAME, RPM_STRING_ARRAY_TYPE,
			p, c);
	/* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
/*@-nullpass@*/
	p = (const void **) fc->requires->EVR;
assert(p != NULL);
	xx = headerAddEntry(pkg->header, RPMTAG_REQUIREVERSION, RPM_STRING_ARRAY_TYPE,
			p, c);
	p = (const void **) fc->requires->Flags;
assert(p != NULL);
	xx = headerAddEntry(pkg->header, RPMTAG_REQUIREFLAGS, RPM_INT32_TYPE,
			p, c);
/*@=nullpass@*/
    }
/*@=branchstate@*/

    /* Add dependency dictionary(#dependencies) */
    p = (const void **) argiData(fc->ddictx);
    c = argiCount(fc->ddictx);
    if (p != NULL)
	xx = headerAddEntry(pkg->header, RPMTAG_DEPENDSDICT, RPM_INT32_TYPE,
			p, c);

    /* Add per-file dependency (start,number) pairs (#files) */
    p = (const void **) argiData(fc->fddictx);
    c = argiCount(fc->fddictx);
assert(ac == c);
    if (p != NULL)
	xx = headerAddEntry(pkg->header, RPMTAG_FILEDEPENDSX, RPM_INT32_TYPE,
			p, c);

    p = (const void **) argiData(fc->fddictn);
    c = argiCount(fc->fddictn);
assert(ac == c);
    if (p != NULL)
	xx = headerAddEntry(pkg->header, RPMTAG_FILEDEPENDSN, RPM_INT32_TYPE,
			p, c);

    printDeps(pkg->header);

if (fc != NULL && _rpmfc_debug) {
char msg[BUFSIZ];
sprintf(msg, "final: files %d cdict[%d] %d%% ddictx[%d]", fc->nfiles, argvCount(fc->cdict), ((100 * fc->fknown)/fc->nfiles), argiCount(fc->ddictx));
rpmfcPrint(msg, fc, NULL);
}

    /* Clean up. */
    fmode = _free(fmode);
    fc = rpmfcFree(fc);
    av = argvFree(av);

    return rc;
}

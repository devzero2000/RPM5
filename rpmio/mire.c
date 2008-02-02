/** \ingroup rpmds
 * \file lib/rpmds.c
 */
#include "system.h"

#include <rpmio.h>	/* XXX _free */
#include <rpmlog.h>
#define	_MIRE_INTERNAL
#include <mire.h>

#include "debug.h"

/*@access regex_t @*/

/*@unchecked@*/
int _mire_debug = 0;

int mireClean(miRE mire)
{
    if (mire == NULL) return 0;
/*@-modfilesys@*/
if (_mire_debug)
fprintf(stderr, "--> mireClean(%p)\n", mire);
/*@=modfilesys@*/
    mire->pattern = _free(mire->pattern);
    if (mire->preg != NULL) {
	regfree(mire->preg);
	/*@+voidabstract -usereleased @*/ /* LCL: regfree has bogus only */
	mire->preg = _free(mire->preg);
	/*@=voidabstract =usereleased @*/
    }
    mire->fnflags = 0;
    mire->cflags = 0;
    mire->eflags = 0;
    mire->notmatch = 0;
    return 0;
}

miRE XmireUnlink(miRE mire, const char * msg, const char * fn, unsigned ln)
{
    if (mire == NULL) return NULL;

/*@-modfilesys@*/
if (_mire_debug && msg != NULL)
fprintf(stderr, "--> mire %p -- %d %s at %s:%u\n", mire, mire->nrefs, msg, fn, ln);
/*@=modfilesys@*/

    mire->nrefs--;
    return NULL;
}

miRE XmireLink(miRE mire, const char * msg, const char * fn, unsigned ln)
{
    if (mire == NULL) return NULL;
    mire->nrefs++;

/*@-modfilesys@*/
if (_mire_debug && msg != NULL)
fprintf(stderr, "--> mire %p ++ %d %s at %s:%u\n", mire, mire->nrefs, msg, fn, ln);
/*@=modfilesys@*/

    /*@-refcounttrans@*/ return mire; /*@=refcounttrans@*/
}

miRE mireFree(miRE mire)
{
    if (mire == NULL)
	return NULL;

    if (mire->nrefs > 1)
	return mireUnlink(mire, "mireFree");

    (void) mireClean(mire);
    (void) mireUnlink(mire, "mireFree");
/*@-refcounttrans -usereleased @*/
    memset(mire, 0, sizeof(*mire));
    mire = _free(mire);
/*@=refcounttrans =usereleased @*/
    return NULL;
}

miRE mireNew(rpmMireMode mode, int tag)
{
    miRE mire = xcalloc(1, sizeof(*mire));
    mire->mode = mode;
    mire->tag = tag;
    return mireLink(mire,"mireNew");
}

int mireRegexec(miRE mire, const char * val)
{
    int rc = 0;

    switch (mire->mode) {
    case RPMMIRE_STRCMP:
	rc = strcmp(mire->pattern, val);
	if (rc) rc = 1;
	break;
    case RPMMIRE_DEFAULT:
    case RPMMIRE_REGEX:
/*@-nullpass@*/
	rc = regexec(mire->preg, val, 0, NULL, mire->eflags);
/*@=nullpass@*/
	if (rc && rc != REG_NOMATCH) {
	    char msg[256];
	    (void) regerror(rc, mire->preg, msg, sizeof(msg)-1);
	    msg[sizeof(msg)-1] = '\0';
	    rpmlog(RPMLOG_ERR, _("%s: regexec failed: %s\n"),
			mire->pattern, msg);
	    rc = -1;
	}
	break;
    case RPMMIRE_GLOB:
	rc = fnmatch(mire->pattern, val, mire->fnflags);
	if (rc && rc != FNM_NOMATCH)
	    rc = -1;
	break;
    default:
	rc = -1;
	break;
    }

/*@-modfilesys@*/
if (_mire_debug)
fprintf(stderr, "--> mireRegexec(%p, \"%s\") rc %d\n", mire, val, rc);
/*@=modfilesys@*/
    return rc;
}

int mireRegcomp(miRE mire, const char * pattern)
{
    int rc = 0;

    mire->pattern = xstrdup(pattern);

    switch (mire->mode) {
    case RPMMIRE_DEFAULT:
    case RPMMIRE_STRCMP:
	break;
    case RPMMIRE_REGEX:
	mire->preg = xcalloc(1, sizeof(*mire->preg));
	if (mire->cflags == 0)
	    mire->cflags = (REG_EXTENDED | REG_NOSUB);
	rc = regcomp(mire->preg, mire->pattern, mire->cflags);
	if (rc) {
	    char msg[256];
	    (void) regerror(rc, mire->preg, msg, sizeof(msg)-1);
	    msg[sizeof(msg)-1] = '\0';
	    rpmlog(RPMLOG_ERR, _("%s: regcomp failed: %s\n"),
			mire->pattern, msg);
	}
	break;
    case RPMMIRE_GLOB:
	if (mire->fnflags == 0)
	    mire->fnflags = FNM_PATHNAME | FNM_PERIOD;
	break;
    default:
	rc = -1;
	break;
    }

    if (rc)
	(void) mireClean(mire);

/*@-modfilesys@*/
if (_mire_debug)
fprintf(stderr, "--> mireRegcomp(%p, \"%s\") rc %d\n", mire, pattern, rc);
/*@=modfilesys@*/
    return rc;
}

/* =============================================================== */

#if defined(STANDALONE)

#include <rpmcb.h>
#include <argv.h>
#include <popt.h>

static int _debug = 0;

static struct poptOption optionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL,	&_debug, -1,		NULL, NULL },
  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    poptContext optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
    miRE mire = NULL;
    ARGV_t av = NULL;
    int ac = 0;
    int rc;
    int xx;
    int i;

    while ((rc = poptGetNextOpt(optCon)) > 0) {
        const char * optArg = poptGetOptArg(optCon);
        optArg = _free(optArg);
	switch (rc) {
	case 'v':
	    rpmIncreaseVerbosity();
	    /*@switchbreak@*/ break;
	default:
	    poptPrintUsage(optCon, stderr, 0);
	    goto exit;
            /*@switchbreak@*/ break;
	}
    }

    if (_debug) {
	rpmIncreaseVerbosity();
	rpmIncreaseVerbosity();
_mire_debug = 1;
    }

    av = poptGetArgs(optCon);
    ac = argvCount(av);
    if (ac != 1) {
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }

    mire = mireNew(RPMMIRE_REGEX, 0);
    if ((xx = mireRegcomp(mire, argv[1])) != 0)
	goto exit;
    
    xx = argvFgets(&av, NULL);
    ac = argvCount(av);

    for (i = 0; i < ac; i++) {
	xx = mireRegexec(mire, av[i]);
	if (xx == 0)
	    fprintf(stdout, "%s\n", av[i]);
    }

exit:
    mire = mireFree(mire);

    av = argvFree(av);

    optCon = poptFreeContext(optCon);

    return 0;
}

#endif	/* STANDALONE */

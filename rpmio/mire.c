/** \ingroup rpmds
 * \file lib/rpmds.c
 */
#include "system.h"

#ifdef	WITH_PCRE
#include <pcre.h>
#endif

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
    if (mire->pcre != NULL) {
#ifdef	WITH_PCRE
	pcre_free(mire->pcre);
#endif
	mire->pcre = NULL;
    }
    mire->errmsg = NULL;
    mire->erroff = 0;
    mire->errcode = 0;
    mire->fnflags = 0;
    mire->cflags = 0;
    mire->eflags = 0;
    mire->coptions = 0;
    mire->eoptions = 0;
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
#ifdef	WITH_PCRE
    case RPMMIRE_PCRE:
	rc = pcre_exec(mire->pcre, NULL, val, (int)strlen(val), 0,
		mire->eoptions, NULL, 0);
	if (rc && rc != PCRE_ERROR_NOMATCH) {
	    rpmlog(RPMLOG_ERR, _("%s: pcre_exec failed: return %d\n"), rc);
	    rc = -1;
	}
#else
	rc = -1;
#endif
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
#ifdef	WITH_PCRE
    case RPMMIRE_PCRE:
	if (mire->coptions == 0)
	    mire->coptions = 0;		/* XXX defaults? */
	mire->pcre = pcre_compile2(mire->pattern, mire->coptions,
		&mire->errcode, &mire->errmsg, &mire->erroff, NULL);
	if (mire->pcre == NULL) {
	    rpmlog(RPMLOG_ERR,
		_("%s: pcre_compile2 failed: %s(%d) at offset %d\n"),
		mire->pattern, mire->errmsg, mire->errcode, mire->erroff);
	    rc = -1;
	}
#else
	rc = -1;
#endif
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

#include <poptIO.h>

static rpmMireMode mireMode = RPMMIRE_REGEX;

static struct poptOption optionsTable[] = {

 { "strcmp", '\0', POPT_ARG_VAL,	&mireMode, RPMMIRE_STRCMP,
	N_("use strcmp matching"), NULL},
 { "regex", '\0', POPT_ARG_VAL,		&mireMode, RPMMIRE_REGEX,
	N_("use regex matching"), NULL},
 { "fnmatch", '\0', POPT_ARG_VAL,	&mireMode, RPMMIRE_GLOB,
	N_("use fnmatch matching"), NULL},
 { "pcre", '\0', POPT_ARG_VAL,		&mireMode, RPMMIRE_PCRE,
	N_("use pcre matching"), NULL},

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"),
	NULL },

  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    poptContext optCon = rpmioInit(argc, argv, optionsTable);
    miRE mire = NULL;
    ARGV_t av = NULL;
    int ac = 0;
    int rc = 1;
    int xx;
    int i;

    if (__debug) {
_mire_debug = 1;
    }

    av = poptGetArgs(optCon);
    if ((ac = argvCount(av)) != 1)
	goto exit;

    mire = mireNew(mireMode, 0);
    if ((rc = mireRegcomp(mire, av[0])) != 0)
	goto exit;
    
    av = NULL;
    if ((rc = argvFgets(&av, NULL)) != 0)
	goto exit;

    rc = 1;	/* assume nomatch failure. */
    ac = argvCount(av);
    if (av && *av)
    for (i = 0; i < ac; i++) {
	xx = mireRegexec(mire, av[i]);
	if (xx == 0) {
	    fprintf(stdout, "%s\n", av[i]);
	    rc = 0;
	}
    }
    av = argvFree(av);

exit:
    mire = mireFree(mire);

    optCon = rpmioFini(optCon);

    return rc;
}

#endif	/* STANDALONE */

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
    if (mire->hints != NULL) {
#ifdef	WITH_PCRE
	pcre_free(mire->hints);
#endif
	mire->hints = NULL;
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

int mireRegexec(miRE mire, const char * val, size_t vallen)
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
    case RPMMIRE_PCRE:
#ifdef	WITH_PCRE
	if (vallen == 0)
	    vallen = strlen(val);
	rc = pcre_exec(mire->pcre, mire->hints, val, vallen, mire->startoff,
		mire->eoptions, mire->offsets, mire->noffsets);
	if (_mire_debug && rc < 0 && rc != PCRE_ERROR_NOMATCH) {
	    rpmlog(RPMLOG_ERR, _("pcre_exec failed: return %d\n"), rc);
	}
#else
	rc = -99;
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
fprintf(stderr, "--> mireRegexec(%p, %p[%u]) rc %d mode %d \"%.*s\"\n", mire, val, (unsigned)vallen, rc, mire->mode, (vallen < 20 ? vallen : 20), val);
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
    case RPMMIRE_PCRE:
#ifdef	WITH_PCRE
	if (mire->coptions == 0)
	    mire->coptions = 0;		/* XXX defaults? */
	mire->errcode = 0;
	mire->errmsg = NULL;
	mire->erroff = 0;
	mire->pcre = pcre_compile2(mire->pattern, mire->coptions,
		&mire->errcode, &mire->errmsg, &mire->erroff, mire->table);
	if (mire->pcre == NULL) {
	    if (_mire_debug)
		rpmlog(RPMLOG_ERR,
		    _("pcre_compile2 failed: %s(%d) at offset %d of \"%s\"\n"),
		    mire->errmsg, mire->errcode, mire->erroff, mire->pattern);
	    rc = -1;
	    goto exit;	/* XXX HACK: rpmgrep is not expecting mireClean. */
	}
#else
	rc = -99;
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

#ifdef	WITH_PCRE
exit:
#endif
/*@-modfilesys@*/
if (_mire_debug)
fprintf(stderr, "--> mireRegcomp(%p, \"%s\") rc %d\n", mire, pattern, rc);
/*@=modfilesys@*/
    return rc;
}

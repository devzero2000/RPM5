/** \ingroup rpmds
 * \file lib/rpmds.c
 */
#include "system.h"

#include <rpmerr.h>
#define	_MIRE_INTERNAL
#include <mire.h>

#include "debug.h"

/*@access regex_t @*/

/*@unchecked@*/
int _mire_debug = 0;

/**
 * Wrapper to free(3), hides const compilation noise, permit NULL, return NULL.
 * @param p		memory to free
 * @retval		NULL always
 */
/*@unused@*/ static inline /*@null@*/ void *
_free(/*@only@*/ /*@null@*/ const void * p) /*@modifies p@*/
{
    if (p != NULL)	free((void *)p);
    return NULL;
}

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
    memset(mire, 0, sizeof(*mire));	/* XXX trash and burn */
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
	    rpmError(RPMERR_REGEXEC, "%s: regexec failed: %s\n",
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
	    rpmError(RPMERR_REGCOMP, "%s: regcomp failed: %s\n",
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

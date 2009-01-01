/** \ingroup rpmds
 * \file lib/rpmevr.c
 */
#include "system.h"

#include <rpmiotypes.h>
#include <rpmmacro.h>
#include <mire.h>

#include <rpmtag.h>
#define	_RPMEVR_INTERNAL
#include <rpmevr.h>

#include "debug.h"

/*@unchecked@*/
int _rpmevr_debug = 0;

#if !defined(MAX)
#define MAX(x, y) ( ((x)>(y))?(x):(y) )
#endif

/* XXX Force digits to beat alphas. See bugzilla #50977. */
/*@unchecked@*/
#if defined(RPM_VENDOR_MANDRIVA) /* old-comparision-behaviour */
static int _invert_digits_alphas_comparison = -1;
#else
static int _invert_digits_alphas_comparison = 1;
#endif

/* XXX Punctuation characters that are not treated as alphas */
/*@unchecked@*/ /*@observer@*/
static const char * _rpmnotalpha = ".:-";

/**
 * Return rpm's analogue of xisalpha.
 * @param c		character to test
 * @return		is this an alpha character?
 */
static inline int xisrpmalpha(int c)
	/*@*/
{
    int rc = xisalpha(c);
    if (!rc)
	rc = xispunct(c);
    if (rc && _rpmnotalpha && *_rpmnotalpha)
	rc = (strchr(_rpmnotalpha, c) == NULL);
    return rc;
}

int rpmEVRcmp(const char * a, const char * b)
	/*@*/
{
    const char * ae = NULL, * be = NULL;
    int rc = 0;		/* assume equal */

    /* Compare version strings segment by segment. */
    for (; *a && *b && rc == 0; a = ae, b = be) {

	/* Skip leading non-alpha, non-digit characters. */
	while (*a && !(xisdigit((int)*a) || xisrpmalpha((int)*a))) a++;
	while (*b && !(xisdigit((int)*b) || xisrpmalpha((int)*b))) b++;

	/* Wildcard comparison? */
	/* Note: limited to suffix-only wildcard matching at the moment. */
	if (a[0] == '*' && a[1] == '\0') {
            be = strchr(b, '\0');	/* XXX be = b + strlen(b); */
	} else
	if (b[0] == '*' && b[1] == '\0') {
            ae = strchr(a, '\0');	/* XXX ae = a + strlen(a); */
	} else
	/* Digit string comparison? */
	if (xisdigit((int)*a) || xisdigit((int)*b)) {
	    /* Discard leading zeroes. */
	    while (a[0] == '0' && xisdigit((int)a[1])) a++;
	    while (b[0] == '0' && xisdigit((int)b[1])) b++;

	    /* Find end of digit strings. */
	    ae = a; while (xisdigit((int)*ae)) ae++;
	    be = b; while (xisdigit((int)*be)) be++;

	    /* Calculate digit comparison return code. */
	    if (a == ae || b == be)
		rc = (int)(*a - *b) * _invert_digits_alphas_comparison;
	    else {
		rc = (ae - a) - (be - b);
		if (!rc)
		    rc = strncmp(a, b, (ae - a));
	    }
	} else {
	    /* Find end of alpha strings. */
	    ae = a; while (xisrpmalpha((int)*ae)) ae++;
	    be = b; while (xisrpmalpha((int)*be)) be++;

	    /* Calculate alpha comparison return code. */
	    rc = strncmp(a, b, MAX((ae - a), (be - b)));
	}
    }

    /* Longer string wins. */
    if (!rc)
	rc = (int)(*a - *b);

    /* Force strict -1, 0, 1 return. */
    rc = (rc > 0 ? 1
	: rc < 0 ? -1
	: 0);
    return rc;
}

/*@unchecked@*/ /*@observer@*/ /*@null@*/
static const char * _evr_tuple_match = "i^(?:([^:-]+):)?([^:-]+)(?:-([^:-]+))?(?::([^:-]+))?$";
/*@unchecked@*/ /*@null@*/
static const char * evr_tuple_match = NULL;
/*@unchecked@*/ /*@null@*/
static miRE evr_tuple_mire = NULL;
/*@unchecked@*/
static int evr_tuple_nmire;

static miRE rpmEVRmire(void)
	/*@*/
{
    if (evr_tuple_mire == NULL) {
	int xx;
	evr_tuple_match = rpmExpand("%{?evr_tuple_match}", NULL);
	if (evr_tuple_match == NULL || evr_tuple_match[0] == '\0')
	    evr_tuple_match = xstrdup(_evr_tuple_match);
	xx = mireAppend(RPMMIRE_REGEX, 0, evr_tuple_match, NULL,
		&evr_tuple_mire, &evr_tuple_nmire);
    }
assert(evr_tuple_match != NULL && evr_tuple_mire != NULL);
    return evr_tuple_mire;
}

int rpmEVRparse(const char * evrstr, EVR_t evr)
	/*@modifies evrstr, evr @*/
{
    char *s = xstrdup(evrstr);
    char *se;
#ifdef	RPM_VENDOR_MANDRIVA
    char *se2;
#endif

    evr->str = se = s;
#ifdef	RPM_VENDOR_MANDRIVA
    se2 = se;
#endif
    while (*se && xisdigit((int)*se)) se++;	/* se points to epoch terminator */

    if (*se == ':') {
	evr->F[RPMEVR_E] = s;
	*se++ = '\0';
	evr->F[RPMEVR_V] = se;
#ifdef	RPM_VENDOR_MANDRIVA
	se2 = se;
#endif
	if (*evr->F[RPMEVR_E] == '\0') evr->F[RPMEVR_E] = "0";
	evr->Elong = strtoul(evr->F[RPMEVR_E], NULL, 10);
    } else {
	evr->F[RPMEVR_E] = NULL; /* XXX disable epoch compare if missing */
	evr->F[RPMEVR_V] = s;
	evr->Elong = 0;
    }
#if defined(NOTYET) || defined(RPM_VENDOR_MANDRIVA)    
    se = strrchr(se, ':');		/* se points to release terminator */
    if (se) {
	*se++ = '\0';
	evr->F[RPMEVR_D] = se;
    } else {
	evr->F[RPMEVR_D] = NULL;
    }
    se = strrchr(se2, '-');		/* se points to version terminator */
#else
    se = strrchr(se, '-');		/* se points to version terminator */
#endif
    if (se) {
	*se++ = '\0';
	evr->F[RPMEVR_R] = se;
    } else {
	evr->F[RPMEVR_R] = NULL;
    }

    {	miRE mire = rpmEVRmire();
	int noffsets = 4 * 3;
	int offsets[4 * 3];
	int xx;

	memset(offsets, 0, sizeof(offsets));
	xx = mireSetEOptions(mire, offsets, noffsets);

	xx = mireRegexec(mire, evr->str, strlen(evr->str));

	xx = mireSetEOptions(mire, NULL, 0);
    }
    return 0;
}

/**
 * Dressed rpmEVRcmp, handling missing values.
 * @param a		1st string
 * @param b		2nd string
 * @return		+1 if a is "newer", 0 if equal, -1 if b is "newer"
 */
static int compare_values(const char *a, const char *b)
	/*@*/
{
    return rpmvercmp(a, b);
}

/*@unchecked@*/ /*@null@*/
static const char * evr_tuple_order = NULL;

static const char * rpmEVRorder(void)
	/*@*/
{
    if (evr_tuple_order == NULL) {
	evr_tuple_order = rpmExpand("%{?evr_tuple_order}", NULL);
	if (evr_tuple_order == NULL || evr_tuple_order[0] == '\0')
	    evr_tuple_order = xstrdup("EVR");
    }
assert(evr_tuple_order != NULL && evr_tuple_order[0] != '\0');
    return evr_tuple_order;
}

int rpmEVRcompare(const EVR_t a, const EVR_t b)
{
    const char * s;
    int rc = 0;

assert(a->F[RPMEVR_E] != NULL);
assert(a->F[RPMEVR_V] != NULL);
assert(a->F[RPMEVR_R] != NULL);
assert(b->F[RPMEVR_E] != NULL);
assert(b->F[RPMEVR_V] != NULL);
assert(b->F[RPMEVR_R] != NULL);

    for (s = rpmEVRorder(); *s; s++) {
	int ix;
	switch ((int)*s) {
	default:	continue;	/*@notreached@*/ break;
	case 'E':	ix = RPMEVR_E;	break;
	case 'V':	ix = RPMEVR_V;	break;
	case 'R':	ix = RPMEVR_R;	break;
	case 'D':	ix = RPMEVR_D;	break;
	}
	rc = compare_values(a->F[ix], b->F[ix]);
	if (rc)
	    break;
    }
    return rc;
}

/*@-redecl@*/
int (*rpmvercmp) (const char *a, const char *b) = rpmEVRcmp;
/*@=redecl@*/

/**
 */
/*@unchecked@*/ /*@observer@*/
static struct EVRop_s {
/*@observer@*/ /*@null@*/
    const char * operator;
    rpmsenseFlags sense;
} cops[] = {
    { "<=", RPMSENSE_LESS ^ RPMSENSE_EQUAL},
    { "=<", RPMSENSE_LESS ^ RPMSENSE_EQUAL},

    { "==", RPMSENSE_EQUAL},
    { "!=", RPMSENSE_NOTEQUAL},
    
    { ">=", RPMSENSE_GREATER ^ RPMSENSE_EQUAL},
    { "=>", RPMSENSE_GREATER ^ RPMSENSE_EQUAL},

    { "<", RPMSENSE_LESS},
    { "=", RPMSENSE_EQUAL},
    { ">", RPMSENSE_GREATER},

    { NULL, 0 },
};

rpmsenseFlags rpmEVRflags(const char *op, const char **end)
{
    rpmsenseFlags Flags = 0;
    struct EVRop_s *cop;

    if (op == NULL || *op == '\0')
	Flags = RPMSENSE_EQUAL;
    else
    for (cop = cops; cop->operator != NULL; cop++) {
	if (strncmp(op, cop->operator, strlen(cop->operator)))
	    continue;
	Flags = cop->sense;
	if (end)
	    *end = op + strlen(cop->operator);
	break;
    }
    return Flags;
}

int rpmVersionCompare(Header first, Header second)
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    const char * one, * two;
    rpmuint32_t Eone, Etwo;
    int rc;
    int xx;

    he->tag = RPMTAG_EPOCH;
    xx = headerGet(first, he, 0);
    Eone = (xx && he->p.ui32p ? he->p.ui32p[0] : 0);
    he->p.ptr = _free(he->p.ptr);
    he->tag = RPMTAG_EPOCH;
    xx = headerGet(second, he, 0);
    Etwo = (xx && he->p.ui32p ? he->p.ui32p[0] : 0);
    he->p.ptr = _free(he->p.ptr);

    if (Eone < Etwo)
	return -1;
    else if (Eone > Etwo)
	return 1;

    he->tag = RPMTAG_VERSION;
    xx = headerGet(first, he, 0);
    one = he->p.str;
    he->tag = RPMTAG_VERSION;
    xx = headerGet(second, he, 0);
    two = he->p.str;
    rc = rpmvercmp(one, two);
    if (rc)
	goto exit;

    he->tag = RPMTAG_RELEASE;
    xx = headerGet(first, he, 0);
    one = he->p.str;
    he->tag = RPMTAG_RELEASE;
    xx = headerGet(second, he, 0);
    two = he->p.str;
    rc = rpmvercmp(one, two);
#if defined(NOTYET) || defined(RPM_VENDOR_MANDRIVA)

    if (rc)
	goto exit;

    he->tag = RPMTAG_DISTEPOCH;
    xx = headerGet(first, he, 0);
    one = (xx && he->p.str ? he->p.str : xstrdup(""));
    he->tag = RPMTAG_DISTEPOCH;
    xx = headerGet(second, he, 0);
    two = (xx && he->p.str ? he->p.str : xstrdup(""));
    rc = rpmvercmp(one, two);
#endif

exit:
    one = _free(one);
/*@-usereleased@*/
    two = _free(two);
/*@=usereleased@*/
    return rc;
}

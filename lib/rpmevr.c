/** \ingroup rpmds
 * \file lib/rpmevr.c
 */
#include "system.h"

#include <rpmio.h>

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
static int _invert_digits_alphas_comparison = -1;

/* XXX Punctuation characters that are not treated as alphas */
/*@unchecked@*/
static const char * _rpmnotalpha = ".:-";

/**
 * Return rpm's analogue of xisalpha.
 * @param c		character to test
 * @return		is this an alpha character?
 */
static inline int xisrpmalpha(int c)
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
    const char * ae, * be;
    int rc = 0;

    /* Compare version strings segment by segment. */
    for (; *a && *b && rc == 0; a = ae, b = be) {

	/* Skip leading non-alpha, non-digit characters. */
	while (*a && !(xisdigit(*a) || xisrpmalpha(*a))) a++;
	while (*b && !(xisdigit(*b) || xisrpmalpha(*b))) b++;

	/* Digit string comparison? */
	if (xisdigit(*a) || xisdigit(*b)) {
	    /* Discard leading zeroes. */
	    while (*a == '0') a++;
	    while (*b == '0') b++;

	    /* Find end of digit strings. */
	    for (ae = a; xisdigit(*ae); ae++);
	    for (be = b; xisdigit(*be); be++);

	    /* Calculate digit comparison return code. */
	    if (a == ae && b == be)
		rc = 0;
	    else if (a == ae || b == be)
		rc = (*b - *a) * _invert_digits_alphas_comparison;
	    else {
		rc = (ae - a) - (be - b);
		if (!rc)
		    rc = strncmp(a, b, (ae - a));
	    }
	} else {
	    /* Find end of alpha strings. */
	    for (ae = a; xisrpmalpha(*ae); ae++);
	    for (be = b; xisrpmalpha(*be); be++);

	    /* Calculate alpha comparison return code. */
	    rc = strncmp(a, b, MAX((ae - a), (be - b)));
	}
    }

    /* Longer string wins. */
    if (!rc)
	rc = (*a - *b);

    /* Force strict -1, 0, 1 return. */
    rc = (rc > 0 ? 1
	: rc < 0 ? -1
	: 0);
    return rc;
}

int rpmEVRparse(const char * evrstr, EVR_t evr)
	/*@modifies evrstr, evr @*/
{
    char *s = xstrdup(evrstr);
    char *se;

    evr->str = se = s;
    while (*se && xisdigit(*se)) se++;	/* s points to epoch terminator */

    if (*se == ':') {
	evr->E = s;
	*se++ = '\0';
	evr->V = se;
/*@-branchstate@*/
	if (*evr->E == '\0') evr->E = "0";
/*@=branchstate@*/
	evr->Elong = strtoul(evr->E, NULL, 10);
    } else {
	evr->E = NULL;	/* XXX disable epoch compare if missing */
	evr->V = evrstr;
	evr->Elong = 0;
    }
    se = strrchr(se, '-');		/* se points to version terminator */
    if (se) {
	*se++ = '\0';
	evr->R = se;
    } else {
	evr->R = NULL;
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
{
    if (!a && !b)
	return 0;
    else if (a && !b)
	return 1;
    else if (!a && b)
	return -1;
    return rpmEVRcmp(a, b);
}

int rpmEVRcompare(const EVR_t a, const EVR_t b)
{
    int rc = 0;

    if (!rc)
	rc = compare_values(a->E, b->E);
    if (!rc)
	rc = compare_values(a->V, b->V);
    if (!rc)
	rc = compare_values(a->R, b->R);
    return rc;
}

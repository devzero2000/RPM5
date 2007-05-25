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
	    while (a[0] == '0' && xisdigit(a[1])) a++;
	    while (b[0] == '0' && xisdigit(b[1])) b++;

	    /* Find end of digit strings. */
	    ae = a; while (xisdigit(*ae)) ae++;
	    be = b; while (xisdigit(*be)) be++;

	    /* Calculate digit comparison return code. */
	    if (a == ae || b == be)
		rc = (*b - *a) * _invert_digits_alphas_comparison;
	    else {
		rc = (ae - a) - (be - b);
		if (!rc)
		    rc = strncmp(a, b, (ae - a));
	    }
	} else {
	    /* Find end of alpha strings. */
	    ae = a; while (xisrpmalpha(*ae)) ae++;
	    be = b; while (xisrpmalpha(*be)) be++;

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
    while (*se && xisdigit(*se)) se++;	/* se points to epoch terminator */

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
	evr->V = s;
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
	/*@*/
{
    return rpmvercmp(a, b);
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

int (*rpmvercmp) (const char *a, const char *b) = rpmEVRcmp;

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

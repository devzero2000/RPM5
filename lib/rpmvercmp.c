/** \ingroup rpmts
 * \file lib/rpmvercmp.c
 */

#include "system.h"

#include <rpmio.h>

#include "debug.h"

#if !defined(MAX)
#define MAX(x, y) ( ((x)>(y))?(x):(y) )
#endif

/* XXX Force digits to beat alphas. See bugzilla #50977. */
static int invert_digits_alphas_comparison = -1;

/* XXX Punctuation characters that are not treated as alphas */
static const char * rpmnotalpha = ".";

static inline int xisrpmalpha(int c)
{
    int rc = xisalpha(c);
    if (!rc)
	rc = xispunct(c);
    if (rc && rpmnotalpha && *rpmnotalpha)
	rc = (strchr(rpmnotalpha, c) == NULL);
    return rc;
}

static int ___rpmvercmp(const char * a, const char * b)
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
		rc = (*b - *a) * invert_digits_alphas_comparison;
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

/* compare alpha and numeric segments of two versions */
/* return 1: a is newer than b */
/*        0: a and b are the same version */
/*       -1: b is newer than a */
static int __rpmvercmp(const char * a, const char * b)
	/*@*/
{
    char oldch1, oldch2;
    char * str1, * str2;
    char * one, * two;
    int rc;
    int isnum;

    /* easy comparison to see if versions are identical */
    if (!strcmp(a, b)) return 0;

    str1 = alloca(strlen(a) + 1);
    str2 = alloca(strlen(b) + 1);

    strcpy(str1, a);
    strcpy(str2, b);

    one = str1;
    two = str2;

    /* loop through each version segment of str1 and str2 and compare them */
    /*@-branchstate@*/
/*@-boundsread@*/
    while (*one && *two) {
	while (*one && !xisalnum(*one)) one++;
	while (*two && !xisalnum(*two)) two++;

	/* If we ran to the end of either, we are finished with the loop */
	if (!(*one && *two)) break;

	str1 = one;
	str2 = two;

	/* grab first completely alpha or completely numeric segment */
	/* leave one and two pointing to the start of the alpha or numeric */
	/* segment and walk str1 and str2 to end of segment */
	if (xisdigit(*str1)) {
	    while (*str1 && xisdigit(*str1)) str1++;
	    while (*str2 && xisdigit(*str2)) str2++;
	    isnum = 1;
	} else {
	    while (*str1 && xisalpha(*str1)) str1++;
	    while (*str2 && xisalpha(*str2)) str2++;
	    isnum = 0;
	}

	/* save character at the end of the alpha or numeric segment */
	/* so that they can be restored after the comparison */
/*@-boundswrite@*/
	oldch1 = *str1;
	*str1 = '\0';
	oldch2 = *str2;
	*str2 = '\0';
/*@=boundswrite@*/

	/* this cannot happen, as we previously tested to make sure that */
	/* the first string has a non-null segment */
assert(str1 > one);
	if (one == str1) return -1;	/* arbitrary */

	/* take care of the case where the two version segments are */
	/* different types: one numeric, the other alpha (i.e. empty) */
	/* numeric segments are always newer than alpha segments */
	/* XXX See patch #60884 (and details) from bugzilla #50977. */
	if (two == str2) return (isnum ? 1 : -1);

	if (isnum) {
	    /* this used to be done by converting the digit segments */
	    /* to ints using atoi() - it's changed because long  */
	    /* digit segments can overflow an int - this should fix that. */

	    /* throw away any leading zeros - it's a number, right? */
	    while (*one == '0') one++;
	    while (*two == '0') two++;

	    /* whichever number has more digits wins */
assert((str1 - one) == strlen(one));
assert((str2 - two) == strlen(two));
	    if ((str1 - one) > (str2 - two)) return 1;
	    if ((str2 - two) > (str1 - one)) return -1;
	}

	/* strcmp will return which one is greater - even if the two */
	/* segments are alpha or if they are numeric.  don't return  */
	/* if they are equal because there might be more segments to */
	/* compare */
	rc = strcmp(one, two);
	if (rc) return (rc < 1 ? -1 : 1);

	/* restore character that was replaced by null above */
/*@-boundswrite@*/
	*str1 = oldch1;
	one = str1;
	*str2 = oldch2;
	two = str2;
/*@=boundswrite@*/
    }
    /*@=branchstate@*/
/*@=boundsread@*/

    /* this catches the case where all numeric and alpha segments have */
    /* compared identically but the segment sepparating characters were */
    /* different */
/*@-boundsread@*/
    if ((!*one) && (!*two)) return 0;

    /* whichever version still has characters left over wins */
    if (!*one) return -1; else return 1;
/*@=boundsread@*/
}

int _rpmvercmp(const char * a, const char * b)
{
    int Orc = __rpmvercmp(a, b);
    int Nrc = ___rpmvercmp(a, b);
if (Nrc != Orc) fprintf(stderr, "==> %s(%s,%s) N %d O %d\n", __FUNCTION__, a, b, Nrc, Orc);
assert(Nrc == Orc);
    return Orc;
}

int (*rpmvercmp)(const char *a, const char *b) = _rpmvercmp;

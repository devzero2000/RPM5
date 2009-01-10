/** \ingroup rpmds
 * \file lib/rpmdpkg.c
 */
#include "system.h"

#include <rpmio.h>

#define	_RPMEVR_INTERNAL
#include <rpmdpkg.h>

#include "debug.h"

/*@access EVR_t @*/

/*@unchecked@*/
int _rpmdpkg_debug = 0;

/* assume ascii */
static inline int dpkgEVRctype(char x)
	/*@*/
{
    return (
	  x == '~' ? -1 
	: xisdigit(x) ? 0
	: !x ? 0 \
	: xisalpha(x) ? x
	: x + 256
    );
}

int dpkgEVRcmp(const char *a, const char *b)
{
    if (!a) a = "";
    if (!b) b = "";

    while (*a || *b) {
	int first_diff= 0;

	while ( (*a && !xisdigit(*a)) || (*b && !xisdigit(*b)) ) {
	    int vc = dpkgEVRctype(*a);
	    int rc = dpkgEVRctype(*b);
	    if (vc != rc) return vc - rc;
	    a++; b++;
	}

	while (*a == '0') a++;
	while (*b == '0') b++;
	while (xisdigit(*a) && xisdigit(*b)) {
	    if (!first_diff) first_diff = *a - *b;
	    a++; b++;
	}
	if (xisdigit(*a)) return 1;
	if (xisdigit(*b)) return -1;
	if (first_diff) return first_diff;
    }
    return 0;
}

int dpkgEVRparse(const char * evrstr, EVR_t evr)
{
    return rpmEVRparse(evrstr, evr);
}

int dpkgEVRcompare(const EVR_t a, const EVR_t b)
{
    int r;

    if (a->Elong > b->Elong) return 1;
    if (a->Elong < b->Elong) return -1;
    r = dpkgEVRcmp(a->V, b->V);  if (r) return r;
    return dpkgEVRcmp(a->R, b->R);
}

/** \ingroup rpmds
 * \file lib/rpmns.c
 */
#include "system.h"

#include <rpmio.h>

#define	_RPMNS_INTERNAL
#include <rpmns.h>

#include "debug.h"

/*@unchecked@*/
int _rpmns_debug = 0;

/*@unchecked@*/
const char *_rpmns_N_at_A = ".";

/*@-nullassign@*/
/*@observer@*/
static const char *rpmnsArches[] = {
    "i386", "i486", "i586", "i686", "athlon", "pentium3", "pentium4", "x86_64", "amd64", "ia32e",
    "alpha", "alphaev5", "alphaev56", "alphapca56", "alphaev6", "alphaev67",
    "sparc", "sun4", "sun4m", "sun4c", "sun4d", "sparcv8", "sparcv9",
    "sparc64", "sun4u",
    "mips", "mipsel", "IP",
    "ppc", "ppciseries", "ppcpseries",
    "ppc64", "ppc64iseries", "ppc64pseries",
    "m68k",
    "rs6000",
    "ia64",
    "armv3l", "armv4b", "armv4l",
    "armv5teb", "armv5tel",
    "s390", "i370", "s390x",
    "sh", "xtensa",
    "noarch",
    NULL,
};
/*@=nullassign@*/

int rpmnsArch(const char * str)
{
    const char ** av;
    for (av = rpmnsArches; *av != NULL; av++) {
	if (!strcmp(str, *av))
	    return 1;
    }
    return 0;
}

/*@=boundsread@*/
nsType rpmnsClassify(const char * str)
{
    const char * s;
    nsType Type = RPMNS_TYPE_STRING;

    if (*str == '/')
	return RPMNS_TYPE_PATH;
    if (*str == '%')
	return RPMNS_TYPE_MACRO;
    s = str + strlen(str);
    if ((s - str) > 3 && s[-3] == '.' && s[-2] == 's' && s[-1] == 'o')
	return RPMNS_TYPE_SONAME;
    for (s = str; *s; s++) {
	if (s[0] == '(' || s[strlen(s)-1] == ')')
	    return RPMNS_TYPE_NAMESPACE;
	if (s[0] == '.' && s[1] == 's' && s[2] == 'o')
	    return RPMNS_TYPE_SONAME;
	if (s[0] == '.' && xisdigit(s[-1]) && xisdigit(s[1]))
	    return RPMNS_TYPE_VERSION;
	if (_rpmns_N_at_A && _rpmns_N_at_A[0]) {
	    if (s[0] == _rpmns_N_at_A[0] && rpmnsArch(s+1))
		return RPMNS_TYPE_ARCH;
	}
	if (s[0] == '.')
	    return RPMNS_TYPE_COMPOUND;
    }
    return RPMNS_TYPE_STRING;
}

int rpmnsParse(const char * str, rpmns ns)
{
    char *t;
    ns->str = t = xstrdup(str);
    ns->Type = rpmnsClassify(ns->str);
    ns->NS = NULL;
    ns->N = ns->str;
    ns->A = NULL;
    if (ns->Type == RPMNS_TYPE_ARCH) {
	t = strrchr(t, _rpmns_N_at_A[0]);
	if (t) {
	    *t++ = '\0';
	    ns->A = t;
	}
    }
    return 0;
}

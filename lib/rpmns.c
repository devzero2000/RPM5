/** \ingroup rpmds
 * \file lib/rpmns.c
 */
#include "system.h"

#include <rpmio.h>
#include <rpmmacro.h>

#define	_RPMEVR_INTERNAL
#include <rpmevr.h>
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
    "noarch", "fat",
    NULL,
};
/*@=nullassign@*/

nsType rpmnsArch(const char * str)
{
    const char ** av;
    for (av = rpmnsArches; *av != NULL; av++) {
	if (!strcmp(str, *av))
	    return RPMNS_TYPE_ARCH;
    }
    return RPMNS_TYPE_UNKNOWN;
}

/**
 * Dependency probe table.
 */
/*@unchecked@*/ /*@observer@*/
static struct _rpmnsProbes_s {
/*@observer@*/ /*@relnull@*/
    const char * NS;
    nsType Type;
} rpmnsProbes[] = {
    { "rpmlib",		RPMNS_TYPE_RPMLIB },
    { "cpuinfo",	RPMNS_TYPE_CPUINFO },
    { "getconf",	RPMNS_TYPE_GETCONF },
    { "uname",		RPMNS_TYPE_UNAME },
    { "soname",		RPMNS_TYPE_SONAME },
    { "user",		RPMNS_TYPE_USER },
    { "group",		RPMNS_TYPE_GROUP },
    { "mounted",	RPMNS_TYPE_MOUNTED },
    { "diskspace",	RPMNS_TYPE_DISKSPACE },
    { "digest",		RPMNS_TYPE_DIGEST },
    { "gnupg",		RPMNS_TYPE_GNUPG },
    { "exists",		RPMNS_TYPE_ACCESS },
    { "executable",	RPMNS_TYPE_ACCESS },
    { "readable",	RPMNS_TYPE_ACCESS },
    { "writable",	RPMNS_TYPE_ACCESS },
    { "RWX",		RPMNS_TYPE_ACCESS },
    { "RWx",		RPMNS_TYPE_ACCESS },
    { "RW_",		RPMNS_TYPE_ACCESS },
    { "RwX",		RPMNS_TYPE_ACCESS },
    { "Rwx",		RPMNS_TYPE_ACCESS },
    { "Rw_",		RPMNS_TYPE_ACCESS },
    { "R_X",		RPMNS_TYPE_ACCESS },
    { "R_x",		RPMNS_TYPE_ACCESS },
    { "R__",		RPMNS_TYPE_ACCESS },
    { "rWX",		RPMNS_TYPE_ACCESS },
    { "rWx",		RPMNS_TYPE_ACCESS },
    { "rW_",		RPMNS_TYPE_ACCESS },
    { "rwX",		RPMNS_TYPE_ACCESS },
    { "rwx",		RPMNS_TYPE_ACCESS },
    { "rw_",		RPMNS_TYPE_ACCESS },
    { "r_X",		RPMNS_TYPE_ACCESS },
    { "r_x",		RPMNS_TYPE_ACCESS },
    { "r__",		RPMNS_TYPE_ACCESS },
    { "_WX",		RPMNS_TYPE_ACCESS },
    { "_Wx",		RPMNS_TYPE_ACCESS },
    { "_W_",		RPMNS_TYPE_ACCESS },
    { "_wX",		RPMNS_TYPE_ACCESS },
    { "_wx",		RPMNS_TYPE_ACCESS },
    { "_w_",		RPMNS_TYPE_ACCESS },
    { "__X",		RPMNS_TYPE_ACCESS },
    { "__x",		RPMNS_TYPE_ACCESS },
    { "___",		RPMNS_TYPE_ACCESS },
    { NULL, 0 }
};

nsType rpmnsProbe(const char * str)
{
    const struct _rpmnsProbes_s * av;
    size_t sn = strlen(str);
    size_t nb;

    if (sn >= 5 && str[sn-1] == ')')
    for (av = rpmnsProbes; av->NS != NULL; av++) {
	nb = strlen(av->NS);
	if (sn > nb && str[nb] == '(' && !strncmp(str, av->NS, nb))
	    return av->Type;
    }
    return RPMNS_TYPE_UNKNOWN;
}

/*@=boundsread@*/
nsType rpmnsClassify(const char * str)
{
    const char * s;
    nsType Type = RPMNS_TYPE_STRING;

    if (*str == '/')
	return RPMNS_TYPE_PATH;
    s = str + strlen(str);
    if (str[0] == '%' && str[1] == '{' && s[-1] == '}')
	return RPMNS_TYPE_FUNCTION;
    if ((s - str) > 3 && s[-3] == '.' && s[-2] == 's' && s[-1] == 'o')
	return RPMNS_TYPE_DSO;
    Type = rpmnsProbe(str);
    if (Type != RPMNS_TYPE_UNKNOWN)
	return Type;
    for (s = str; *s; s++) {
	if (s[0] == '(' || s[strlen(s)-1] == ')')
	    return RPMNS_TYPE_NAMESPACE;
	if (s[0] == '.' && s[1] == 's' && s[2] == 'o')
	    return RPMNS_TYPE_DSO;
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
    ns->str = t = rpmExpand(str, NULL);
    ns->Type = rpmnsClassify(ns->str);
    switch (ns->Type) {
    case RPMNS_TYPE_ARCH:
	ns->NS = NULL;
	ns->N = ns->str;
	if (ns->N[0] == '!')
	    ns->N++;
	if ((t = strrchr(t, _rpmns_N_at_A[0])) != NULL)
	    *t++ = '\0';
	ns->A = t;
	break;
    case RPMNS_TYPE_RPMLIB:
    case RPMNS_TYPE_CPUINFO:
    case RPMNS_TYPE_GETCONF:
    case RPMNS_TYPE_UNAME:
    case RPMNS_TYPE_SONAME:
    case RPMNS_TYPE_ACCESS:
    case RPMNS_TYPE_USER:
    case RPMNS_TYPE_GROUP:
    case RPMNS_TYPE_MOUNTED:
    case RPMNS_TYPE_DISKSPACE:
    case RPMNS_TYPE_DIGEST:
    case RPMNS_TYPE_GNUPG:
	ns->NS = ns->str;
	if (ns->NS[0] == '!')
	    ns->NS++;
	if ((t = strchr(t, '(')) != NULL)
	    *t++ = '\0';
	ns->N = t;
	t[strlen(t)-1] = '\0';
	ns->A = NULL;
	break;
    case RPMNS_TYPE_UNKNOWN:
    case RPMNS_TYPE_STRING:
    case RPMNS_TYPE_PATH:
    case RPMNS_TYPE_DSO:
    case RPMNS_TYPE_FUNCTION:
    case RPMNS_TYPE_VERSION:
    case RPMNS_TYPE_COMPOUND:
    case RPMNS_TYPE_NAMESPACE:
    case RPMNS_TYPE_TAG:
    default:
	ns->NS = NULL;
	ns->N = ns->str;
	if (ns->N[0] == '!')
	    ns->N++;
	ns->A = NULL;
	break;
    }
    return 0;
}

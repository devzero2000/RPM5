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

nsType rpmnsClassify(const char * str)
{
    const char * s;
    nsType Type = RPMNS_TYPE_STRING;

    if (*str == '/')
	return RPMNS_TYPE_PATH;
    if (*str == '%')
	return RPMNS_TYPE_MACRO;
    for (s = str; *s; s++) {
	if (s[0] == '(' || s[strlen(s)-1] == ')')
	    return RPMNS_TYPE_NAMESPACE;
	if (s[0] == '.' && s[1] == 's' && s[2] == 'o')
	    return RPMNS_TYPE_SONAME;
	if (s[0] == '.' && !strcmp(s+1, "arch"))
	    return RPMNS_TYPE_COMPOUND;
    }
    return RPMNS_TYPE_STRING;
}

int rpmnsParse(const char * str, rpmns ns)
{
    ns->str = xstrdup(str);
    ns->Type = rpmnsClassify(ns->str);
    ns->NS = NULL;
    ns->N = ns->str;
    ns->A = NULL;
    return 0;
}

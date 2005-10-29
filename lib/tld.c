#include "system.h"

#include <rpmlib.h>

#include <rpmds.h>

#include "debug.h"

extern int _rpmds_debug;

#ifdef	DYING
#define	_SBIN_LDCONFIG	"/sbin/ldconfig -p"
/*@unchecked@*/ /*@observer@*/
static const char * _sbin_ldconfig_p = _SBIN_LDCONFIG;

#define	_LD_SO_CACHE	"/etc/ld.so.cache"
/*@unchecked@*/ /*@observer@*/
static const char * _ld_so_cache = _LD_SO_CACHE;

#define	_isspace(_c)	\
	((_c) == ' ' || (_c) == '\t' || (_c) == '\r' || (_c) == '\n')

int rpmdsLdconfig(rpmds * dsp, const char * fn)
{
    char buf[BUFSIZ];
    const char *N, *EVR;
    int_32 Flags = 0;
    rpmds ds;
    char * f, * fe;
    char * g, * ge;
    char * t;
    FILE * fp = NULL;
    int rc = -1;
    int xx;

    if (fn == NULL)
	fn = _ld_so_cache;

    fp = popen(_sbin_ldconfig_p, "r");
    if (fp == NULL)
	goto exit;

    while((f = fgets(buf, sizeof(buf), fp)) != NULL) {
	EVR = NULL;
	/* rtrim on line. */
	ge = f + strlen(f);
	while (--ge > f && _isspace(*ge))
	    *ge = '\0';

	/* ltrim on line. */
	while (*f && _isspace(*f))
	    f++;

	/* split on '=>' */
	fe = f;
	while (*fe && !(fe[0] == '=' && fe[1] == '>'))
            fe++;
	if (*fe == '\0')
	    continue;
	if (fe > f && fe[-1] == ' ') fe[-1] = '\0';
	*fe++ = '\0';
	*fe++ = '\0';
	g = fe;

	/* ltrim on field 2. */
	while (*g && _isspace(*g))
            g++;
	if (*g == '\0')
	    continue;

	/* split out flags */
	for (t = f; *t != '\0'; t++) {
	    if (!_isspace(*t))
		continue;
	    *t++ = '\0';
	    break;
	}
	/* XXX "libc4" "ELF" "libc5" "libc6" _("unknown") */
	/* XXX use flags to generate soname color */
	/* ",64bit" ",IA-64" ",x86-64", ",64bit" are color = 2 */
	/* ",N32" for mips64/libn32 */

	/* XXX use flags and LDASSUME_KERNEL to skip sonames? */
	/* "Linux" "Hurd" "Solaris" "FreeBSD" "kNetBSD" N_("Unknown OS") */
	/* ", OS ABI: %s %d.%d.%d" */

	N = f;
	if (EVR == NULL)
	    EVR = "";
	Flags |= RPMSENSE_PROBE;
	ds = rpmdsSingle(RPMTAG_PROVIDENAME, N, EVR , Flags);
	xx = rpmdsMerge(dsp, ds);
	ds = rpmdsFree(ds);
    }

exit:
    if (fp != NULL) (void) pclose(fp);
    return rc;
}
#endif

int main(int argc, char *argv[])
{
    rpmds ds = NULL;
    int rc;

    rc = rpmdsLdconfig(&ds, NULL);
    
    ds = rpmdsInit(ds);
    while (rpmdsNext(ds) >= 0)
	fprintf(stderr, "%d %s\n", rpmdsIx(ds), rpmdsDNEVR(ds)+2);

    ds = rpmdsFree(ds);

    return 0;
}

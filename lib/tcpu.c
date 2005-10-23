#include "system.h"
#include <rpmio_internal.h>
#include <rpmlib.h>
#include <rpmmacro.h>
#include "rpmds.h"
#include "debug.h"

#define	_PROC_CPUINFO	"/proc/cpuinfo"
static const char * cpuinfo = _PROC_CPUINFO;

#define	_isspace(_c)	\
	((_c) == ' ' || (_c) == '\t' || (_c) == '\r' || (_c) == '\n')

/*
=================
 * processor	: 0
 * vendor_id	: GenuineIntel
 * cpu family	: 6
 * model		: 8
 * model name	: Pentium III (Coppermine)
 * stepping	: 1
 * cpu MHz		: 664.597
 * cache size	: 256 KB
 * physical id	: 0
 * siblings	: 1
 * fdiv_bug	: no
 * hlt_bug		: no
 * f00f_bug	: no
 * coma_bug	: no
 * fpu		: yes
 * fpu_exception	: yes
 * cpuid level	: 2
 * wp		: yes
 * flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 mmx fxsr sse
 * bogomips	: 1327.10
=================
 * processor	: 0
 * vendor_id	: GenuineIntel
 * cpu family	: 15
 * model		: 2
 * model name	: Intel(R) Pentium(R) 4 CPU 2.40GHz
 * stepping	: 9
 * cpu MHz		: 2394.624
 * cache size	: 512 KB
 * physical id	: 0
 * siblings	: 2
 * core id		: 0
 * cpu cores	: 1
 * fdiv_bug	: no
 * hlt_bug		: no
 * f00f_bug	: no
 * coma_bug	: no
 * fpu		: yes
 * fpu_exception	: yes
 * cpuid level	: 2
 * wp		: yes
 * flags		: fpu vme de pse tsc msr pae mce cx8 apic mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe cid xtpr
 * bogomips	: 4793.72
=================
*/

struct cpuinfo_s {
    const char *name;
    int done;
    int flags;
};

struct cpuinfo_s ctags[] = {
    { "processor",	0,  0 },
    { "vendor_id",	0,  0 },
    { "cpu_family",	0,  1 },
    { "model",		0,  1 },
    { "model_name",	0,  0 },
    { "stepping",	0,  1 },
    { "cpu_MHz",	0,  1 },
    { "cache_size",	0,  1 },
    { "physical_id",	0,  0 },
    { "siblings",	0,  0 },
    { "core_id",	0,  0 },
    { "cpu_cores",	0,  0 },
    { "fdiv_bug",	0,  3 },
    { "hlt_bug",	0,  3 },
    { "f00f_bug",	0,  3 },
    { "coma_bug",	0,  3 },
    { "fpu",		0,  0 },	/* XXX use flags attribute instead. */
    { "fpu_exception",	0,  3 },
    { "cpuid_level",	0,  0 },
    { "wp",		0,  3 },
    { "flags",		0,  4 },
    { "bogomips",	0,  1 },
    { NULL,		0, -1 }
};

static int rpmdsCpuinfoCtagFlags(char * name)
{
    struct cpuinfo_s * ct;
    int flags = -1;

    for (ct = ctags; ct->name != NULL; ct++) {
	if (strcmp(ct->name, name))
	    continue;
	if (ct->done)
	    continue;
	ct->done = 1;		/* XXX insure single occurrence */
	flags = ct->flags;
	break;
    }
    return flags;
}

static void rpmdsCpuinfoAdd(rpmds *depsp, const char * NS,
		const char *N, const char *EVR, int_32 Flags)
{
    char *t;
    rpmds ds;
    int xx;

    t = alloca(strlen(NS)+sizeof("()")+strlen(N));
    *t = '\0';
    (void) stpcpy( stpcpy( stpcpy( stpcpy(t, NS), "("), N), ")");

    ds = rpmdsSingle(RPMTAG_PROVIDENAME, t, EVR, Flags);
    xx = rpmdsMerge(depsp, ds);
    ds = rpmdsFree(ds);
}

static int rpmdsCpuinfo(rpmds *depsp, const char * NS)
{
    char buf[BUFSIZ];
    char * f, * fe;
    char * g, * ge;
    char * t;
    FD_t fd = NULL;
    FILE * fp;
    int rc = -1;

    fd = Fopen(cpuinfo, "r.fpio");
    if (fd == NULL || Ferror(fd))
	goto exit;
    fp = fdGetFILE(fd);

    while((f = fgets(buf, sizeof(buf), fp)) != NULL) {
	/* rtrim on line. */
	ge = f + strlen(f);
	while (--ge > f && _isspace(*ge))
	    *ge = '\0';

	/* ltrim on line. */
	while (*f && _isspace(*f))
	    f++;

	/* split on ':' */
	fe = f;
	while (*fe && *fe != ':')
            fe++;
	if (*fe == '\0')
	    continue;
	g = fe + 1;

	/* rtrim on field 1. */
	*fe = '\0';
	while (--fe > f && _isspace(*fe))
	    *fe = '\0';
	if (*f == '\0')
	    continue;

	/* ltrim on field 2. */
	while (*g && _isspace(*g))
            g++;
	if (*g == '\0')
	    continue;

	for (t = f; *t != '\0'; t++) {
	    if (_isspace(*t))
		*t = '_';
	}

	switch (rpmdsCpuinfoCtagFlags(f)) {
	case -1:	/* not found */
	case 0:		/* ignore */
	default:
	    continue;
	    break;
	case 1:		/* Provides: cpuinfo(f) = g */
	    for (t = g; *t != '\0'; t++) {
		if (_isspace(*t) || *t == '(' || *t == ')')
		    *t = '_';
	    }
	    rpmdsCpuinfoAdd(depsp, NS, f, g, RPMSENSE_PROBE|RPMSENSE_EQUAL);
	    break;
	case 2:		/* Provides: cpuinfo(g) */
	    for (t = g; *t != '\0'; t++) {
		if (_isspace(*t) || *t == '(' || *t == ')')
		    *t = '_';
	    }
	    rpmdsCpuinfoAdd(depsp, NS, g, "", RPMSENSE_PROBE);
	    break;
	case 3:		/* if ("yes") Provides: cpuinfo(f) */
	   if (!strcmp(g, "yes"))
		rpmdsCpuinfoAdd(depsp, NS, f, "", RPMSENSE_PROBE);
	    break;
	case 4:		/* Provides: cpuinfo(g[i]) */
	{   char ** av = NULL;
	    int i;
	    i = 0;
	    rc = poptParseArgvString(g, NULL, (const char ***)&av);
	    if (!rc && av != NULL)
	    while ((f = av[i++]) != NULL)
		rpmdsCpuinfoAdd(depsp, NS, f, "", RPMSENSE_PROBE);
	    if (av != NULL)
		free(av);
	}   break;
	}
    }

exit:
    if (fd) (void) Fclose(fd);
    return rc;
}

int main (int argc, const char * argv[])
{
    rpmds cpuinfods = NULL;
    int rc;

_rpmio_debug = 0;
    rc = rpmdsCpuinfo(&cpuinfods, "cpuinfo");

    cpuinfods = rpmdsInit(cpuinfods);
    if (cpuinfods != NULL)
    while (rpmdsNext(cpuinfods) >= 0)
	fprintf(stderr, "Provides: %s\n", rpmdsDNEVR(cpuinfods)+2);

    cpuinfods = rpmdsFree(cpuinfods);

    return rc;
}

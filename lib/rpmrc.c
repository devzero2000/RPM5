#include "system.h"

#include <stdarg.h>

#if defined(HAVE_SYS_SYSTEMCFG_H)
#include <sys/systemcfg.h>
#else
#define __power_pc() 0
#endif

#define	_RPMIOB_INTERNAL	/* XXX for rpmiobSlurp */
#include <rpmio.h>
#include <rpmcb.h>
#define _MIRE_INTERNAL
#include <mire.h>
#include <rpmlua.h>
#include <rpmluaext.h>
#include <rpmmacro.h>

#include <rpmtypes.h>
#include <rpmtag.h>
#define _RPMEVR_INTERNAL
#include <rpmevr.h>

#define _RPMDS_INTERNAL
#include <rpmds.h>

#include <rpmcli.h>

#include "debug.h"

/*@access miRE@*/

/*@unchecked@*/ /*@null@*/
static const char * configTarget = NULL;

/*@observer@*/ /*@unchecked@*/
static const char * platform = SYSCONFIGDIR "/platform";

/*@only@*/ /*@relnull@*/ /*@unchecked@*/
void * platpat = NULL;
/*@unchecked@*/
int nplatpat = 0;


/** \ingroup rpmrc
 * Build and install arch/os table identifiers.
 * @deprecated Eliminate from API.
 * @todo	Eliminate in rpm-5.1.
 */
enum rpm_machtable_e {
    RPM_MACHTABLE_INSTARCH	= 0,	/*!< Install platform architecture. */
    RPM_MACHTABLE_INSTOS	= 1,	/*!< Install platform operating system. */
    RPM_MACHTABLE_BUILDARCH	= 2,	/*!< Build platform architecture. */
    RPM_MACHTABLE_BUILDOS	= 3	/*!< Build platform operating system. */
};
#define	RPM_MACHTABLE_COUNT	4	/*!< No. of arch/os tables. */

typedef /*@owned@*/ const char * cptr_t;

typedef struct machCacheEntry_s {
    const char * name;
    int count;
    cptr_t * equivs;
    int visited;
} * machCacheEntry;

typedef struct machCache_s {
    machCacheEntry cache;
    int size;
} * machCache;

typedef struct machEquivInfo_s {
    const char * name;
    int score;
} * machEquivInfo;

typedef struct machEquivTable_s {
    int count;
    machEquivInfo list;
} * machEquivTable;

typedef struct defaultEntry_s {
/*@owned@*/ /*@null@*/ const char * name;
/*@owned@*/ /*@null@*/ const char * defName;
} * defaultEntry;

typedef struct canonEntry_s {
/*@owned@*/ const char * name;
/*@owned@*/ const char * short_name;
    short num;
} * canonEntry;

/* tags are 'key'canon, 'key'translate, 'key'compat
 *
 * for giggles, 'key'_canon, 'key'_compat, and 'key'_canon will also work
 */
typedef struct tableType_s {
/*@observer@*/ const char * const key;
    const int hasCanon;
    const int hasTranslate;
    struct machEquivTable_s equiv;
    struct machCache_s cache;
    defaultEntry defaults;
    canonEntry canons;
    int defaultsLength;
    int canonsLength;
} * tableType;

/*@-fullinitblock@*/
/*@unchecked@*/
static struct tableType_s tables[RPM_MACHTABLE_COUNT] = {
    { "arch", 1, 0  },
    { "os", 1, 0 },
    { "buildarch", 0, 1 },
    { "buildos", 0, 1 }
};
/*@=fullinitblock@*/

#define OS	0
#define ARCH	1

/*@unchecked@*/
static cptr_t current[2];

/*@unchecked@*/
static int currTables[2] = { RPM_MACHTABLE_INSTOS, RPM_MACHTABLE_INSTARCH };

/*@unchecked@*/
static int defaultsInitialized = 0;

/* prototypes */
static void rpmRebuildTargetVars(/*@null@*/ const char **target, /*@null@*/ const char ** canontarget)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies *canontarget, rpmGlobalMacroContext,
		fileSystem, internalState @*/;

static /*@observer@*/ /*@null@*/ machCacheEntry
machCacheFindEntry(const machCache cache, const char * key)
	/*@*/
{
    int i;

    for (i = 0; i < cache->size; i++)
	if (!strcmp(cache->cache[i].name, key)) return cache->cache + i;

    return NULL;
}

static void machAddEquiv(machEquivTable table, const char * name,
			   int distance)
	/*@modifies table->list, table->count @*/
{
    machEquivInfo equiv;

    {	int i;
	equiv = NULL;
	for (i = 0; i < table->count; i++) {
	    if (xstrcasecmp(table->list[i].name, name))
		continue;
	    equiv = table->list + i;
	    break;
	}
    }

    if (!equiv) {
	if (table->count)
	    table->list = xrealloc(table->list, (table->count + 1)
				    * sizeof(*table->list));
	else
	    table->list = xmalloc(sizeof(*table->list));

	table->list[table->count].name = xstrdup(name);
	table->list[table->count++].score = distance;
    }
}

static void machCacheEntryVisit(machCache cache,
		machEquivTable table, const char * name, int distance)
	/*@modifies table->list, table->count @*/
{
    machCacheEntry entry;
    int i;

    entry = machCacheFindEntry(cache, name);
    if (!entry || entry->visited) return;

    entry->visited = 1;

    for (i = 0; i < entry->count; i++) {
	machAddEquiv(table, entry->equivs[i], distance);
    }

    for (i = 0; i < entry->count; i++) {
	machCacheEntryVisit(cache, table, entry->equivs[i], distance + 1);
    }
}

static void rebuildCompatTables(int type, const char * name)
	/*@globals tables, internalState @*/
	/*@modifies tables, internalState @*/
{
    machCache cache = &tables[currTables[type]].cache;
    machEquivTable table = &tables[currTables[type]].equiv;
    const char * key = name;
    int i;

    for (i = 0; i < cache->size; i++)
	cache->cache[i].visited = 0;

    while (table->count > 0) {
	--table->count;
	table->list[table->count].name = _free(table->list[table->count].name);
    }
    table->count = 0;
    table->list = _free(table->list);

    /*
     *	We have a general graph built using strings instead of pointers.
     *	Yuck. We have to start at a point at traverse it, remembering how
     *	far away everything is.
     */
    /*@-nullstate@*/	/* FIX: table->list may be NULL. */
    machAddEquiv(table, key, 1);
    machCacheEntryVisit(cache, table, key, 2);
    return;
    /*@=nullstate@*/
}

static /*@null@*/ canonEntry lookupInCanonTable(const char * name,
		const canonEntry table, int tableLen)
	/*@*/
{
    while (tableLen) {
	tableLen--;
	if (strcmp(name, table[tableLen].name))
	    continue;
	/*@-immediatetrans -retalias@*/
	return &(table[tableLen]);
	/*@=immediatetrans =retalias@*/
    }

    return NULL;
}

static /*@observer@*/ /*@null@*/
const char * lookupInDefaultTable(const char * name,
		const defaultEntry table, int tableLen)
	/*@*/
{
    while (tableLen) {
	tableLen--;
	if (table[tableLen].name && !strcmp(name, table[tableLen].name))
	    return table[tableLen].defName;
    }

    return name;
}

static void addMacroDefault(const char * macroname,
		const char * val, /*@null@*/ const char * body)
	/*@globals rpmGlobalMacroContext, internalState @*/
	/*@modifies rpmGlobalMacroContext, internalState @*/
{
    if (body == NULL)
	body = val;
    addMacro(NULL, macroname, NULL, body, RMIL_DEFAULT);
}

static void setPathDefault(const char * macroname, const char * subdir)
	/*@globals rpmGlobalMacroContext, internalState @*/
	/*@modifies rpmGlobalMacroContext, internalState @*/
{
    if (macroname != NULL) {
#define	_TOPDIRMACRO	"%{_topdir}/"
	char *body = alloca(sizeof(_TOPDIRMACRO) + strlen(subdir));
	strcpy(body, _TOPDIRMACRO);
	strcat(body, subdir);
	addMacro(NULL, macroname, NULL, body, RMIL_DEFAULT);
#undef _TOPDIRMACRO
    }
}

/*@observer@*/ /*@unchecked@*/
static const char * ___build_pre = "\n\
RPM_SOURCE_DIR=\"%{_sourcedir}\"\n\
RPM_BUILD_DIR=\"%{_builddir}\"\n\
RPM_OPT_FLAGS=\"%{optflags}\"\n\
RPM_ARCH=\"%{_arch}\"\n\
RPM_OS=\"%{_os}\"\n\
export RPM_SOURCE_DIR RPM_BUILD_DIR RPM_OPT_FLAGS RPM_ARCH RPM_OS\n\
RPM_DOC_DIR=\"%{_docdir}\"\n\
export RPM_DOC_DIR\n\
RPM_PACKAGE_NAME=\"%{name}\"\n\
RPM_PACKAGE_VERSION=\"%{version}\"\n\
RPM_PACKAGE_RELEASE=\"%{release}\"\n\
export RPM_PACKAGE_NAME RPM_PACKAGE_VERSION RPM_PACKAGE_RELEASE\n\
%{?buildroot:RPM_BUILD_ROOT=\"%{buildroot}\"\n\
export RPM_BUILD_ROOT\n}\
";

#if defined(RPM_VENDOR_WINDRIVER)
/*@unchecked@*/
extern const char * __usrlibrpm;
/*@unchecked@*/
extern const char * __etcrpm;
#endif

static void setDefaults(void)
	/*@globals rpmGlobalMacroContext, internalState @*/
	/*@modifies rpmGlobalMacroContext, internalState @*/
{

#if defined(RPM_VENDOR_WINDRIVER)
    addMacro(NULL, "_usrlibrpm", NULL, __usrlibrpm, RMIL_DEFAULT);
    addMacro(NULL, "_etcrpm", NULL, __etcrpm, RMIL_DEFAULT);
    addMacro(NULL, "_vendor", NULL, "%{?_host_vendor}%{!?_host_vendor:wrs}", RMIL_DEFAULT);
#endif

    addMacro(NULL, "_usr", NULL, USRPREFIX, RMIL_DEFAULT);
    addMacro(NULL, "_var", NULL, VARPREFIX, RMIL_DEFAULT);
    addMacro(NULL, "_prefix", NULL, "%{_usr}", RMIL_DEFAULT);

    addMacro(NULL, "___build_pre", NULL, ___build_pre, RMIL_DEFAULT);

    addMacroDefault("_topdir",
		"%{_usr}/src/rpm",      NULL);
    addMacroDefault("_tmppath",
		"%{_var}/tmp",          NULL);
    addMacroDefault("_dbpath",
		"%{_var}/lib/rpm",      NULL);
    addMacroDefault("_defaultdocdir",
		"%{_usr}/share/doc",    NULL);

    addMacroDefault("_rpmfilename",
	"%%{ARCH}/%%{NAME}-%%{VERSION}-%%{RELEASE}.%%{ARCH}.rpm",NULL);

    addMacroDefault("optflags",
		"-O2 -g",			NULL);
    addMacroDefault("sigtype",
		"none",			NULL);
    addMacroDefault("_buildshell",
		"/bin/sh",		NULL);

    setPathDefault("_builddir",	"BUILD");
    setPathDefault("_rpmdir",	"RPMS");
    setPathDefault("_srcrpmdir",	"SRPMS");
    setPathDefault("_sourcedir",	"SOURCES");
    setPathDefault("_specdir",	"SPECS");

}

typedef struct cpu_vendor_os_gnu {
/*@owned@*/
    const char * str;
/*@observer@*/
    const char * cpu;
/*@observer@*/
    const char * vendor;
/*@observer@*/
    const char * os;
/*@observer@*/
    const char * gnu;
} * CVOG_t;

/**
 */
static int parseCVOG(const char * str, CVOG_t *cvogp)
	/*@modifies *cvogp @*/
{
    CVOG_t cvog = xcalloc(1, sizeof(*cvog));
    char * p, * pe;

    cvog->str = p = xstrdup(str);
    pe = p + strlen(p);
    while (pe-- > p && isspace(*pe))
	*pe = '\0';

    cvog->cpu = p;
    cvog->vendor = "unknown";
    cvog->os = "unknown";
    cvog->gnu = "";
    while (*p && !(*p == '-' || isspace(*p)))
	    p++;
    if (*p != '\0') *p++ = '\0';

    cvog->vendor = p;
    while (*p && !(*p == '-' || isspace(*p)))
	p++;
    if (*p != '-') {
	if (*p != '\0') *p++ = '\0';
	cvog->os = cvog->vendor;
	cvog->vendor = "unknown";
    } else {
	if (*p != '\0') *p++ = '\0';

	cvog->os = p;
	while (*p && !(*p == '-' || isspace(*p)))
	    p++;
	if (*p == '-') {
	    *p++ = '\0';

	    cvog->gnu = p;
	    while (*p && !(*p == '-' || isspace(*p)))
		p++;
	}
	if (*p != '\0') *p++ = '\0';
    }

    if (cvogp)
	*cvogp = cvog;
    else {
	cvog->str = _free(cvog->str);
	cvog = _free(cvog);
    }
    return 0;
}

/**
 * Read and configure /etc/rpm/platform patterns.
 * @param platform	path to platform patterns
 * @return		RPMRC_OK on success
 */
/*@-onlytrans@*/	/* XXX miRE array, not refcounted. */
static rpmRC rpmPlatform(const char * platform)
	/*@globals nplatpat, platpat,
		rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies nplatpat, platpat,
		rpmGlobalMacroContext, fileSystem, internalState @*/
{
    CVOG_t cvog = NULL;
    rpmiob iob = NULL;
    int init_platform = 0;
    miRE mi_re = NULL;
    int mi_nre = 0;
    char * p, * pe;
    rpmRC rc;
    int xx;

    rc = rpmiobSlurp(platform, &iob);

    if (rc || iob == NULL) {
	rc = RPMRC_FAIL;
	goto exit;
    }

    p = (char *)iob->b;
    for (pe = p; p && *p; p = pe) {
	pe = strchr(p, '\n');
	if (pe)
	    *pe++ = '\0';

	while (*p && xisspace(*p))
	    p++;
	if (*p == '\0' || *p == '#')
	    continue;

	if (init_platform) {
	    char * t = p + strlen(p);
	    while (--t > p && xisspace(*t))
		*t = '\0';
	    if (t > p) {
		xx = mireAppend(RPMMIRE_REGEX, 0, p, NULL, &mi_re, &mi_nre);
	    }
	    continue;
	}

	if (!parseCVOG(p, &cvog) && cvog != NULL) {
	    addMacro(NULL, "_host_cpu", NULL, cvog->cpu, -1);
	    addMacro(NULL, "_host_vendor", NULL, cvog->vendor, -1);
	    addMacro(NULL, "_host_os", NULL, cvog->os, -1);
	}

#if defined(RPM_VENDOR_OPENPKG) /* explicit-platform */
	/* do not use vendor and GNU attribution */
	p = rpmExpand("%{_host_cpu}-%{_host_os}", NULL);
#else
	p = rpmExpand("%{_host_cpu}-%{_host_vendor}-%{_host_os}",
		(cvog && *cvog->gnu ? "-" : NULL),
		(cvog ? cvog->gnu : NULL), NULL);
#endif
	xx = mireAppend(RPMMIRE_STRCMP, 0, p, NULL, &mi_re, &mi_nre);
	p = _free(p);
	
	init_platform++;
    }
    rc = (init_platform ? RPMRC_OK : RPMRC_FAIL);

exit:
    if (cvog) {
	cvog->str = _free(cvog->str);
	cvog = _free(cvog);
    }
    iob = rpmiobFree(iob);
    if (rc == RPMRC_OK) {
	platpat = mireFreeAll(platpat, nplatpat);
	platpat = mi_re;
	nplatpat = mi_nre;
    }
    return rc;
}
/*@=onlytrans@*/

#if defined(SUPPORT_LIBCPUINFO)
static inline int rpmCpuinfoMatch(const char * feature, rpmds cpuinfo)
{
    rpmds cpufeature = rpmdsSingle(RPMTAG_REQUIRENAME, feature, "", RPMSENSE_PROBE);
    int ret = rpmdsMatch(cpufeature, cpuinfo);
    cpufeature = rpmdsFree(cpufeature);
    return ret;
}

static rpmRC rpmCpuinfo(void)
{
    rpmRC rc = RPMRC_FAIL;
    static struct utsname un;    
    const char *cpu;
    miRE mi_re = NULL;
    int mi_nre = 0;
    int xx;
    CVOG_t cvog = NULL;
    rpmds cpuinfo = NULL;

    uname(&un);
    xx = rpmdsCpuinfo(&cpuinfo, NULL);

#if defined(__i386__) || defined(__x86_64__)
    /* XXX: kinda superfluous, same as with cpuinfo([arch]), do nicer..? */
    if(rpmCpuinfoMatch("cpuinfo(64bit)", cpuinfo) && strcmp(un.machine, "x86_64") == 0)
	xx = mireAppend(RPMMIRE_REGEX, 0, "x86_64", NULL, &mi_re, &mi_nre);
    if(rpmCpuinfoMatch("cpuinfo(cmov)", cpuinfo))
    {
	if(rpmCpuinfoMatch("cpuinfo(mmx)", cpuinfo))
	{
	    if(rpmCpuinfoMatch("cpuinfo(sse)", cpuinfo))
	    {
	    	if(rpmCpuinfoMatch("cpuinfo(sse2)", cpuinfo))
	    	    xx = mireAppend(RPMMIRE_REGEX, 0, "pentium4", NULL, &mi_re, &mi_nre);
		xx = mireAppend(RPMMIRE_REGEX, 0, "pentium3", NULL, &mi_re, &mi_nre);
	    }
	    if(rpmCpuinfoMatch("cpuinfo(3dnow+)", cpuinfo))
		xx = mireAppend(RPMMIRE_REGEX, 0, "athlon", NULL, &mi_re, &mi_nre);
	    xx = mireAppend(RPMMIRE_REGEX, 0, "pentium2", NULL, &mi_re, &mi_nre);
	}
	xx = mireAppend(RPMMIRE_REGEX, 0, "i686", NULL, &mi_re, &mi_nre);
    }
    if(rpmCpuinfoMatch("cpuinfo(3dnow)", cpuinfo) && rpmCpuinfoMatch("cpuinfo(mmx)", cpuinfo))
	xx = mireAppend(RPMMIRE_REGEX, 0, "geode", NULL, &mi_re, &mi_nre);
    if(rpmCpuinfoMatch("cpuinfo(tsc)", cpuinfo))
	xx = mireAppend(RPMMIRE_REGEX, 0, "i586", NULL, &mi_re, &mi_nre);
    if(rpmCpuinfoMatch("cpuinfo(ac)", cpuinfo))
	xx = mireAppend(RPMMIRE_REGEX, 0, "i486", NULL, &mi_re, &mi_nre);
    if(rpmCpuinfoMatch("cpuinfo([x86])", cpuinfo))
	xx = mireAppend(RPMMIRE_REGEX, 0, "i386", NULL, &mi_re, &mi_nre);
#endif

#if defined(__powerpc__)
    if(rpmCpuinfoMatch("cpuinfo(64bit)", cpuinfo) && strcmp(un.machine, "ppc64") == 0)
	xx = mireAppend(RPMMIRE_REGEX, 0, "ppc64", NULL, &mi_re, &mi_nre);
    if(rpmCpuinfoMatch("cpuinfo([ppc])", cpuinfo))
	xx = mireAppend(RPMMIRE_REGEX, 0, "ppc", NULL, &mi_re, &mi_nre);
#endif

#if defined(__powerpc__) || defined(__i386__) || defined(__x86_64__)
    if(rpmCpuinfoMatch("cpuinfo([ppc])", cpuinfo) || rpmCpuinfoMatch("cpuinfo([ppc])", cpuinfo))
	xx = mireAppend(RPMMIRE_REGEX, 0, "fat", NULL, &mi_re, &mi_nre);
#endif    

#if defined(__ia64__)
    if(rpmCpuinfoMatch("cpuinfo([ia64])", cpuinfo))
	xx = mireAppend(RPMMIRE_REGEX, 0, "ia64", NULL, &mi_re, &mi_nre);
#endif
    
#if defined(__mips__)
#if defined(__MIPSEL__)
#define mips32 "mipsel"
#define mips64 "mips64el"
#elif define(__MIPSEB__)
#define mips32 "mips"
#define mips64 "mips64"
#endif
    /* XXX: libcpuinfo only have irix support for now.. */
    if(strcmp(un.machine, "mips64") == 0)
	xx = mireAppend(RPMMIRE_REGEX, 0, mips64, NULL, &mi_re, &mi_nre);
    xx = mireAppend(RPMMIRE_REGEX, 0, mips32, NULL, &mi_re, &mi_nre);
#endif

    xx = mireAppend(RPMMIRE_REGEX, 0, "noarch", NULL, &mi_re, &mi_nre);

    cpuinfo = rpmdsFree(cpuinfo);

    cpu = mi_re[0].pattern;
    if(cpu != NULL)
    {
	if (!parseCVOG(cpu, &cvog) && cvog != NULL) {
	    addMacro(NULL, "_host_cpu", NULL, cvog->cpu, -1);
	    addMacro(NULL, "_host_vendor", NULL, cvog->vendor, -1);
	    addMacro(NULL, "_host_os", NULL, cvog->os, -1);
	}
	cvog->str = _free(cvog->str);
	cvog = _free(cvog);

	rc = RPMRC_OK;
	if (rc == RPMRC_OK) {
	    platpat = mireFreeAll(platpat, nplatpat);
	    platpat = mi_re;
	    nplatpat = mi_nre;
	}

    }
    return rc;
}
#endif

/*@-onlytrans@*/	/* XXX miRE array, not refcounted. */
int rpmPlatformScore(const char * platform, void * mi_re, int mi_nre)
{
    miRE mire;
    int i;

    if (mi_re == NULL) {
	mi_re = platpat;
	mi_nre = nplatpat;
    }

    if ((mire = mi_re) != NULL)
    for (i = 0; i < mi_nre; i++) {
	if (mireRegexec(mire + i, platform, 0) >= 0)
	    return (i + 1);
    }
    return 0;
}
/*@=onlytrans@*/

/**
 */
static void defaultMachine(/*@out@*/ const char ** arch,
		/*@out@*/ const char ** os)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies *arch, *os, rpmGlobalMacroContext, fileSystem, internalState @*/
{
#if defined(RPM_VENDOR_OPENPKG) /* larger-utsname */
    /* utsname fields on some platforms (like HP-UX) are very small
       (just about 8 characters). This is too small for OpenPKG, so cheat! */
    static struct utsname un_real;
    static struct {
        char sysname[32];
        char nodename[32];
        char release[32];
        char version[32];
        char machine[32];
    } un;
#else
    static struct utsname un;
#endif
    static int gotDefaults = 0;
    int rc;

    while (!gotDefaults) {
#if defined(RPM_VENDOR_WINDRIVER)
	const char * _platform = rpmGetPath(__etcrpm, "/platform", NULL);
#else
	const char * _platform = platform;
#endif
	CVOG_t cvog = NULL;
#if defined(RPM_VENDOR_OPENPKG) /* larger-utsname */
	const char *cp;
#endif
#if defined(RPM_VENDOR_OPENPKG) /* larger-utsname */
	/* utsname fields on some platforms (like HP-UX) are very small
	   (just about 8 characters). This is too small for OpenPKG, so cheat! */
	rc = uname(&un_real);
	strncpy(un.sysname,  un_real.sysname,  sizeof(un.sysname));  un.sysname [sizeof(un.sysname) -1] = '\0';
	strncpy(un.nodename, un_real.nodename, sizeof(un.nodename)); un.nodename[sizeof(un.nodename)-1] = '\0';
	strncpy(un.release,  un_real.release,  sizeof(un.release));  un.release [sizeof(un.release) -1] = '\0';
	strncpy(un.version,  un_real.version,  sizeof(un.version));  un.version [sizeof(un.version) -1] = '\0';
	strncpy(un.machine,  un_real.machine,  sizeof(un.machine));  un.machine [sizeof(un.machine) -1] = '\0';
#else
	rc = uname(&un);
#endif
	if (rc < 0) return;

#if defined(RPM_VENDOR_OPENPKG) /* platform-major-minor-only */
    /* Reduce the platform version to major and minor version numbers */
    {
        char *cp;
        char *cpR;
        int n;
        cpR = un.release;
        if ((n = strcspn(cpR, "0123456789")) > 0)
            cpR += n;
        if ((n = strspn(cpR, "0123456789.")) > 0) {
            /* terminate after "N.N.N...." prefix */
            cpR[n] = '\0';
            /* shorten to "N.N" if longer */
            if ((cp = strchr(cpR, '.')) != NULL) {
                if ((cp = strchr(cp+1, '.')) != NULL)
                    *cp = '\0';
            }
            strcat(un.sysname, cpR);
        }
        /* fix up machine hardware name containing white-space as it
           happens to be on Power Macs running MacOS X */
        if (!strncmp(un.machine, "Power Macintosh", 15))
            sprintf(un.machine, "powerpc");
    }
#endif

	if (!strncmp(un.machine, "Power Macintosh", 15)) {
	    sprintf(un.machine, "ppc");
	}

#if defined(RPM_VENDOR_OPENPKG) /* explicit-platform */
	/* allow the path to the "platforms" file be overridden under run-time */
	cp = rpmExpand("%{?__platform}", NULL);
	if (cp == NULL || cp[0] == '\0')
	    cp = _platform;
	if (rpmPlatform(cp) == RPMRC_OK) {
#elif defined(SUPPORT_LIBCPUINFO)
	if (rpmPlatform(_platform) == RPMRC_OK || rpmCpuinfo() == RPMRC_OK) {
#else
	if (rpmPlatform(_platform) == RPMRC_OK) {
#endif
	    const char * s;
	    gotDefaults = 1;
	    s = rpmExpand("%{?_host_cpu}", NULL);
	    if (s && *s != '\0') {
		strncpy(un.machine, s, sizeof(un.machine));
		un.machine[sizeof(un.machine)-1] = '\0';
	    }
	    s = _free(s);
	    s = rpmExpand("%{?_host_os}", NULL);
	    if (s && *s != '\0') {
		strncpy(un.sysname, s, sizeof(un.sysname));
		un.sysname[sizeof(un.sysname)-1] = '\0';
	    }
	    s = _free(s);
	}

#if defined(RPM_VENDOR_OPENPKG) /* explicit-platform */
	/* cleanup after above processing */
	if (cp != NULL && cp != _platform)
	    cp = _free(cp);
#endif
#if defined(RPM_VENDOR_WINDRIVER)
	_platform = _free(_platform);
#endif

	if (configTarget && !parseCVOG(configTarget, &cvog) && cvog != NULL) {
	    gotDefaults = 1;
	    if (cvog->cpu && cvog->cpu[0] != '\0') {
		strncpy(un.machine, cvog->cpu, sizeof(un.machine));
		un.machine[sizeof(un.machine)-1] = '\0';
	    }
	    if (cvog->os && cvog->os[0] != '\0') {
		strncpy(un.sysname, cvog->os, sizeof(un.sysname));
		un.sysname[sizeof(un.sysname)-1] = '\0';
	    }
	    cvog->str = _free(cvog->str);
	    cvog = _free(cvog);
	}
	if (gotDefaults)
	    break;
	gotDefaults = 1;
	break;
    }

    if (arch) *arch = un.machine;
    if (os) *os = un.sysname;
}

/** \ingroup rpmrc
 * @deprecated Use addMacro to set _target_* macros.
 * @todo	Eliminate in rpm-5.1.
 # @note Only used by build code.
 * @param archTable
 * @param osTable
 */
static void rpmSetTables(int archTable, int osTable)
	/*@globals currTables, rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies currTables, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    const char * arch, * os;

    defaultMachine(&arch, &os);

    if (currTables[ARCH] != archTable) {
	currTables[ARCH] = archTable;
	rebuildCompatTables(ARCH, arch);
    }

    if (currTables[OS] != osTable) {
	currTables[OS] = osTable;
	rebuildCompatTables(OS, os);
    }
}

static void rpmSetMachine(const char * arch, const char * os)
	/*@globals current, rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies current, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    if (arch == NULL) {
/*@i@*/	defaultMachine(&arch, NULL);
	if (tables[currTables[ARCH]].hasTranslate)
	    arch = lookupInDefaultTable(arch,
			    tables[currTables[ARCH]].defaults,
			    tables[currTables[ARCH]].defaultsLength);
    }
assert(arch != NULL);

    if (os == NULL) {
/*@i@*/	defaultMachine(NULL, &os);
	if (tables[currTables[OS]].hasTranslate)
	    os = lookupInDefaultTable(os,
			    tables[currTables[OS]].defaults,
			    tables[currTables[OS]].defaultsLength);
    }
assert(os != NULL);


    if (!current[ARCH] || strcmp(arch, current[ARCH])) {
	current[ARCH] = _free(current[ARCH]);
	current[ARCH] = xstrdup(arch);
	rebuildCompatTables(ARCH, arch);
    }

    if (!current[OS] || strcmp(os, current[OS])) {
	char * t = xstrdup(os);
	current[OS] = _free(current[OS]);
	if (!strcmp(t, "linux"))
	    *t = 'L';
	current[OS] = t;
	rebuildCompatTables(OS, os);
    }
}

static void getMachineInfo(int type, /*@null@*/ /*@out@*/ const char ** name,
			/*@null@*/ /*@out@*/int * num)
	/*@modifies *name, *num @*/
{
    canonEntry canon;
    int which = currTables[type];

    /* use the normal canon tables, even if we're looking up build stuff */
    if (which >= 2) which -= 2;

    canon = lookupInCanonTable(current[type],
			       tables[which].canons,
			       tables[which].canonsLength);

    if (canon) {
	if (num) *num = canon->num;
	if (name) *name = canon->short_name;
    } else {
	if (num) *num = 255;
	if (name) *name = current[type];
    }
}

static void rpmRebuildTargetVars(const char ** target, const char ** canontarget)
{

    char *ca = NULL, *co = NULL, *ct = NULL;
    int x;

    /* Rebuild the compat table to recalculate the current target arch.  */

    rpmSetMachine(NULL, NULL);
    rpmSetTables(RPM_MACHTABLE_INSTARCH, RPM_MACHTABLE_INSTOS);
    rpmSetTables(RPM_MACHTABLE_BUILDARCH, RPM_MACHTABLE_BUILDOS);

    if (target && *target) {
	char *c;
	/* Set arch and os from specified build target */
	ca = xstrdup(*target);
	if ((c = strchr(ca, '-')) != NULL) {
	    *c++ = '\0';
	    
	    if ((co = strrchr(c, '-')) == NULL) {
		co = c;
	    } else {
		if (!xstrcasecmp(co, "-gnu"))
		    *co = '\0';
		if ((co = strrchr(c, '-')) == NULL)
		    co = c;
		else
		    co++;
	    }
	    if (co != NULL) co = xstrdup(co);
	}
    } else {
	const char *a = NULL;
	const char *o = NULL;
	/* Set build target from rpm arch and os */
	getMachineInfo(ARCH, &a, NULL);
	ca = (a) ? xstrdup(a) : NULL;
	getMachineInfo(OS, &o, NULL);
	co = (o) ? xstrdup(o) : NULL;
    }

    /* If still not set, Set target arch/os from default uname(2) values */
    if (ca == NULL) {
	const char *a = NULL;
	defaultMachine(&a, NULL);
	ca = (a) ? xstrdup(a) : NULL;
    }
    if (ca != NULL)
    for (x = 0; ca[x] != '\0'; x++)
	ca[x] = (char)xtolower(ca[x]);

    if (co == NULL) {
	const char *o = NULL;
	defaultMachine(NULL, &o);
	co = (o) ? xstrdup(o) : NULL;
    }
    if (co != NULL)
    for (x = 0; co[x] != '\0'; x++)
	co[x] = (char)xtolower(co[x]);

    /* XXX For now, set canonical target to arch-os */
    if (ct == NULL) {
	ct = xmalloc(strlen(ca) + sizeof("-") + strlen(co));
	sprintf(ct, "%s-%s", ca, co);
    }

/*
 * XXX All this macro pokery/jiggery could be achieved by doing a delayed
 *	rpmInitMacros(NULL, PER-PLATFORM-MACRO-FILE-NAMES);
 */
    delMacro(NULL, "_target");
    addMacro(NULL, "_target", NULL, ct, RMIL_RPMRC);
    delMacro(NULL, "_target_cpu");
    addMacro(NULL, "_target_cpu", NULL, ca, RMIL_RPMRC);
    delMacro(NULL, "_target_os");
    addMacro(NULL, "_target_os", NULL, co, RMIL_RPMRC);

    if (canontarget)
	*canontarget = ct;
    else
	ct = _free(ct);
    ca = _free(ca);
    /*@-usereleased@*/
    co = _free(co);
    /*@=usereleased@*/
}

void rpmFreeRpmrc(void)
	/*@globals current, tables, defaultsInitialized @*/
	/*@modifies current, tables, defaultsInitialized @*/
{
    int i, j, k;

    platpat = mireFreeAll(platpat, nplatpat);
    nplatpat = 0;

    for (i = 0; i < RPM_MACHTABLE_COUNT; i++) {
	tableType t;
	t = tables + i;
	if (t->equiv.list) {
	    for (j = 0; j < t->equiv.count; j++)
		t->equiv.list[j].name = _free(t->equiv.list[j].name);
	    t->equiv.list = _free(t->equiv.list);
	    t->equiv.count = 0;
	}
	if (t->cache.cache) {
	    for (j = 0; j < t->cache.size; j++) {
		machCacheEntry e;
		e = t->cache.cache + j;
		if (e == NULL)
		    /*@innercontinue@*/ continue;
		e->name = _free(e->name);
		if (e->equivs) {
		    for (k = 0; k < e->count; k++)
			e->equivs[k] = _free(e->equivs[k]);
		    e->equivs = _free(e->equivs);
		}
	    }
	    t->cache.cache = _free(t->cache.cache);
	    t->cache.size = 0;
	}
	if (t->defaults) {
	    for (j = 0; j < t->defaultsLength; j++) {
		t->defaults[j].name = _free(t->defaults[j].name);
		t->defaults[j].defName = _free(t->defaults[j].defName);
	    }
	    t->defaults = _free(t->defaults);
	    t->defaultsLength = 0;
	}
	if (t->canons) {
	    for (j = 0; j < t->canonsLength; j++) {
		t->canons[j].name = _free(t->canons[j].name);
		t->canons[j].short_name = _free(t->canons[j].short_name);
	    }
	    t->canons = _free(t->canons);
	    t->canonsLength = 0;
	}
    }

    current[OS] = _free(current[OS]);
    current[ARCH] = _free(current[ARCH]);
    defaultsInitialized = 0;
/*@-globstate -nullstate@*/ /* FIX: platpat/current may be NULL */
    return;
/*@=globstate =nullstate@*/
}

/** \ingroup rpmrc
 * Read macro configuration file(s).
 * @return		0 on success
 */
static int rpmReadRC(void)
	/*@globals defaultsInitialized, rpmMacrofiles,
		rpmGlobalMacroContext, rpmCLIMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies defaultsInitialized, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    int rc = 0;

    if (!defaultsInitialized) {
	setDefaults();
	defaultsInitialized = 1;
    }

    /* Read macro files. */
    {	const char *mfpath = rpmExpand(rpmMacrofiles, NULL);
	    
	if (mfpath != NULL) {
	    rpmInitMacros(NULL, mfpath);
	    mfpath = _free(mfpath);
	}
    }

    return rc;
}

int rpmReadConfigFiles(/*@unused@*/ const char * file,
		const char * target)
	/*@globals configTarget @*/
	/*@modifies configTarget @*/
{
    mode_t mode = 0022;

    /* Reset umask to its default umask(2) value. */
    mode = umask(mode);

    configTarget = target;

    /* Preset target macros */
    /*@-nullstate@*/	/* FIX: target can be NULL */
    rpmRebuildTargetVars(&target, NULL);

    /* Read the files */
/*@-globs@*/
    if (rpmReadRC()) return -1;
/*@=globs@*/

    /* Reset target macros */
    rpmRebuildTargetVars(&target, NULL);
    /*@=nullstate@*/

    /* Finally set target platform */
    {	const char *cpu = rpmExpand("%{_target_cpu}", NULL);
	const char *os = rpmExpand("%{_target_os}", NULL);
	rpmSetMachine(cpu, os);
	cpu = _free(cpu);
	os = _free(os);
    }
    configTarget = NULL;

    /* Force Lua state initialization */
#ifdef WITH_LUA
    (void)rpmluaGetPrintBuffer(NULL);
#if defined(RPM_VENDOR_OPENPKG) /* rpm-lua-extensions-based-on-rpm-lib-functionality */
    (void)rpmluaextActivate(rpmluaGetGlobalState());
#endif /* RPM_VENDOR_OPENPKG */
#endif

    return 0;
}

int rpmShowRC(FILE * fp)
{
    rpmds ds = NULL;
    int i;
    machEquivTable equivTable;
    int xx;

    /* the caller may set the build arch which should be printed here */
    fprintf(fp, "ARCHITECTURE AND OS:\n");
    fprintf(fp, "build arch            : %s\n", current[ARCH]);

    fprintf(fp, "compatible build archs:");
    equivTable = &tables[RPM_MACHTABLE_BUILDARCH].equiv;
    for (i = 0; i < equivTable->count; i++)
	fprintf(fp," %s", equivTable->list[i].name);
    fprintf(fp, "\n");

    fprintf(fp, "build os              : %s\n", current[OS]);

    fprintf(fp, "compatible build os's :");
    equivTable = &tables[RPM_MACHTABLE_BUILDOS].equiv;
    for (i = 0; i < equivTable->count; i++)
	fprintf(fp," %s", equivTable->list[i].name);
    fprintf(fp, "\n");

    fprintf(fp, "install arch          : %s\n", current[ARCH]);
    fprintf(fp, "install os            : %s\n", current[OS]);

    fprintf(fp, "compatible archs      :");
    equivTable = &tables[RPM_MACHTABLE_INSTARCH].equiv;
    for (i = 0; i < equivTable->count; i++)
	fprintf(fp," %s", equivTable->list[i].name);
    fprintf(fp, "\n");

    fprintf(fp, "compatible os's       :");
    equivTable = &tables[RPM_MACHTABLE_INSTOS].equiv;
    for (i = 0; i < equivTable->count; i++)
	fprintf(fp," %s", equivTable->list[i].name);
    fprintf(fp, "\n");

    {	const char * s = rpmExpand("%{?optflags}", NULL);
	fprintf(fp, "%-21s : %s\n", "optflags", ((s && *s) ? s : "(not set)"));
	s = _free(s);

#ifdef	WITH_LUA
	fprintf(fp, "\nLUA MODULES:\n");
/*@-globs@*/
	s = rpmExpand(rpmluaFiles, NULL);
/*@=globs@*/
	fprintf(fp, "%-21s : %s\n", "luafiles", ((s && *s) ? s : "(not set)"));
	s = _free(s);
/*@-globs@*/
	s = rpmExpand(rpmluaPath, NULL);
/*@=globs@*/
	fprintf(fp, "%-21s : %s\n", "luapath", ((s && *s) ? s : "(not set)"));
	s = _free(s);
#endif

	fprintf(fp, "\nMACRO DEFINITIONS:\n");
/*@-globs@*/
	s = rpmExpand(rpmMacrofiles, NULL);
/*@=globs@*/
	fprintf(fp, "%-21s : %s\n", "macrofiles", ((s && *s) ? s : "(not set)"));
	s = _free(s);
    }

    if (rpmIsVerbose()) {
	rpmPRCO PRCO = rpmdsNewPRCO(NULL);
	xx = rpmdsSysinfo(PRCO, NULL);
	ds = rpmdsFromPRCO(PRCO, RPMTAG_PROVIDENAME);
	if (ds != NULL) {
	    const char * fn = (_sysinfo_path ? _sysinfo_path : "/etc/rpm/sysinfo");
	    fprintf(fp, _("Configured system provides (from %s):\n"), fn);
	    ds = rpmdsInit(ds);
	    while (rpmdsNext(ds) >= 0) {
		const char * DNEVR = rpmdsDNEVR(ds);
		if (DNEVR != NULL)
		    fprintf(fp, "    %s\n", DNEVR+2);
	    }
	    ds = rpmdsFree(ds);
	    fprintf(fp, "\n");
	}
	PRCO = rpmdsFreePRCO(PRCO);
    }

    if (rpmIsVerbose()) {
	fprintf(fp, _("Features provided by rpmlib installer:\n"));
	xx = rpmdsRpmlib(&ds, NULL);
	ds = rpmdsInit(ds);
	while (rpmdsNext(ds) >= 0) {
	    const char * DNEVR = rpmdsDNEVR(ds);
	    if (DNEVR != NULL)
		fprintf(fp, "    %s\n", DNEVR+2);
	}
	ds = rpmdsFree(ds);
	fprintf(fp, "\n");

	xx = rpmdsCpuinfo(&ds, NULL);
	if (ds != NULL) {
#if defined(SUPPORT_LIBCPUINFO)
	    const char * fn = "libcpuinfo";
#else
	    const char * fn = (_cpuinfo_path ? _cpuinfo_path : "/proc/cpuinfo");
#endif
	    fprintf(fp,
		_("Features provided by current cpuinfo (from %s):\n"), fn);
	    ds = rpmdsInit(ds);
	    while (rpmdsNext(ds) >= 0) {
		const char * DNEVR = rpmdsDNEVR(ds);
		if (DNEVR != NULL)
		    fprintf(fp, "    %s\n", DNEVR+2);
	    }
	    ds = rpmdsFree(ds);
	    fprintf(fp, "\n");
	}
    }

    if (rpmIsDebug()) {
	xx = rpmdsGetconf(&ds, NULL);
	if (ds != NULL) {
	    fprintf(fp,
		_("Features provided by current getconf:\n"));
	    ds = rpmdsInit(ds);
	    while (rpmdsNext(ds) >= 0) {
		const char * DNEVR = rpmdsDNEVR(ds);
		if (DNEVR != NULL)
		    fprintf(fp, "    %s\n", DNEVR+2);
	    }
	    ds = rpmdsFree(ds);
	    fprintf(fp, "\n");
	}

	xx = rpmdsUname(&ds, NULL);
	if (ds != NULL) {
	    fprintf(fp,
		_("Features provided by current uname:\n"));
	    ds = rpmdsInit(ds);
	    while (rpmdsNext(ds) >= 0) {
		const char * DNEVR = rpmdsDNEVR(ds);
		if (DNEVR != NULL)
		    fprintf(fp, "    %s\n", DNEVR+2);
	    }
	    ds = rpmdsFree(ds);
	    fprintf(fp, "\n");
	}
    }

    rpmDumpMacroTable(NULL, fp);

    return 0;
}

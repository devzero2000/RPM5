/*@-bounds@*/
#include "system.h"

#include <stdarg.h>

#if HAVE_SYS_SYSTEMCFG_H
#include <sys/systemcfg.h>
#else
#define __power_pc() 0
#endif

#include <rpmio_internal.h> /* for rpmioSlurp() */
#include <rpmcli.h>
#include <rpmmacro.h>
#include <rpmlua.h>
#include <rpmds.h>

#define _MIRE_INTERNAL
#include <mire.h>

#include "misc.h"
#include "debug.h"

/*@unchecked@*/ /*@null@*/
static const char * configTarget = NULL;

/*@observer@*/ /*@unchecked@*/
static const char * platform = SYSCONFIGDIR "/platform";
/*@only@*/ /*@relnull@*/ /*@unchecked@*/
void * platpat = NULL;
/*@unchecked@*/
int nplatpat = 0;

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

struct rpmvarValue {
    const char * value;
    /* eventually, this arch will be replaced with a generic condition */
    const char * arch;
/*@only@*/ /*@null@*/ struct rpmvarValue * next;
};

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
    { "arch", 1, 0 },
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

static /*@observer@*/ /*@null@*/ machEquivInfo
machEquivSearch(const machEquivTable table, const char * name)
	/*@*/
{
    int i;

    for (i = 0; i < table->count; i++)
	if (!xstrcasecmp(table->list[i].name, name))
	    return table->list + i;

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

static void setVarDefault(int var, const char * macroname, const char * val,
		/*@null@*/ const char * body)
	/*@globals rpmGlobalMacroContext, internalState @*/
	/*@modifies rpmGlobalMacroContext, internalState @*/
{
    if (body == NULL)
	body = val;
    addMacro(NULL, macroname, NULL, body, RMIL_DEFAULT);
}

static void setPathDefault(int var, const char * macroname, const char * subdir)
	/*@globals rpmGlobalMacroContext, h_errno, internalState @*/
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

static void setDefaults(void)
	/*@globals rpmGlobalMacroContext, h_errno, internalState @*/
	/*@modifies rpmGlobalMacroContext, internalState @*/
{

    addMacro(NULL, "_usr", NULL, "/usr", RMIL_DEFAULT);
    addMacro(NULL, "_var", NULL, "/var", RMIL_DEFAULT);
    addMacro(NULL, "_prefix", NULL, "%{_usr}", RMIL_DEFAULT);

    addMacro(NULL, "___build_pre", NULL, ___build_pre, RMIL_DEFAULT);

    setVarDefault(-1,			"_topdir",
		"/usr/src/rpm",		"%{_usr}/src/rpm");
    setVarDefault(-1,			"_tmppath",
		"/var/tmp",		"%{_var}/tmp");
    setVarDefault(-1,			"_dbpath",
		"/var/lib/rpm",		"%{_var}/lib/rpm");
    setVarDefault(-1,			"_defaultdocdir",
		"/usr/share/doc",	"%{_usr}/share/doc");

    setVarDefault(-1,			"_rpmfilename",
	"%%{ARCH}/%%{NAME}-%%{VERSION}-%%{RELEASE}.%%{ARCH}.rpm",NULL);

    setVarDefault(-1,			"optflags",
		"-O2 -g",			NULL);
    setVarDefault(-1,			"sigtype",
		"none",			NULL);
    setVarDefault(-1,			"_buildshell",
		"/bin/sh",		NULL);

    setPathDefault(-1,			"_builddir",	"BUILD");
    setPathDefault(-1,			"_rpmdir",	"RPMS");
    setPathDefault(-1,			"_srcrpmdir",	"SRPMS");
    setPathDefault(-1,			"_sourcedir",	"SOURCES");
    setPathDefault(-1,			"_specdir",	"SPECS");

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
/*@-bounds@*/
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
/*@-branchstate@*/
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
/*@=branchstate@*/
    return 0;
}
/*@=bounds@*/

/**
 * Destroy platform patterns.
 * @param mire		platform pattern array
 * @param nre		no of patterns in array
 * @return		NULL always 
 */
/*@null@*/
static void * mireFreeAll(/*@only@*/ /*@null@*/ miRE mire, int nre)
	/*@modifies mire@*/
{
    if (mire != NULL) {
	int i;
	for (i = 0; i < nre; i++)
	    (void) mireClean(mire + i);
	mire = _free(mire);
    }
    return NULL;
}

/**
 * Append pattern to array.
 * @param mode		type of pattern match
 * @param tag		identifier (like an rpmTag)
 * @param pattern	pattern to compile
 * @retval *mi_rep	platform pattern array
 * @retval *mi_nrep	no. of patterns in array
 */
/*@null@*/
static int mireAppend(rpmMireMode mode, int tag, const char * pattern,
		miRE * mi_rep, int * mi_nrep)
	/*@modifies *mi_rep, *mi_nrep @*/
{
    miRE mire;

    mire = (*mi_rep);
    mire = xrealloc(mire, ((*mi_nrep) + 1) * sizeof(*mire));
    (*mi_rep) = mire;
    mire += (*mi_nrep);
    (*mi_nrep)++;
    memset(mire, 0, sizeof(*mire));
    mire->mode = mode;
    mire->tag = tag;
    return mireRegcomp(mire, pattern);
}

/**
 * Read and configure /etc/rpm/platform patterns.
 * @param platform	path to platform patterns
 * @return		0 on success
 */
/*@-bounds@*/
static int rpmPlatform(const char * platform)
	/*@globals nplatpat, platpat,
		rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies nplatpat, platpat,
		rpmGlobalMacroContext, fileSystem, internalState @*/
{
    CVOG_t cvog = NULL;
    char * b = NULL;
    ssize_t blen = 0;
    int init_platform = 0;
    miRE mi_re = NULL;
    int mi_nre = 0;
    char * p, * pe;
    int rc;
    int xx;

    rc = rpmioSlurp(platform, &b, &blen);

    if (rc || b == NULL || blen <= 0) {
	rc = -1;
	goto exit;
    }

    p = b;
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
	    if (t > p)
		xx = mireAppend(RPMMIRE_REGEX, 0, p, &mi_re, &mi_nre);
	    continue;
	}

	if (!parseCVOG(p, &cvog) && cvog != NULL) {
	    addMacro(NULL, "_host_cpu", NULL, cvog->cpu, -1);
	    addMacro(NULL, "_host_vendor", NULL, cvog->vendor, -1);
	    addMacro(NULL, "_host_os", NULL, cvog->os, -1);
	}

	p = rpmExpand("%{_host_cpu}-%{_host_vendor}-%{_host_os}",
		(cvog && *cvog->gnu ? "-" : NULL),
		(cvog ? cvog->gnu : NULL), NULL);
	xx = mireAppend(RPMMIRE_STRCMP, 0, p, &mi_re, &mi_nre);
	p = _free(p);
	
	init_platform++;
    }
    rc = (init_platform ? 0 : -1);

exit:
    if (cvog) {
	cvog->str = _free(cvog->str);
	cvog = _free(cvog);
    }
/*@-modobserver@*/
    b = _free(b);
/*@=modobserver@*/
    if (rc == 0) {
	platpat = mireFreeAll(platpat, nplatpat);
	platpat = mi_re;
	nplatpat = mi_nre;
    }
    return rc;
}
/*@=bounds@*/

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
	if (!mireRegexec(mire + i, platform))
	    return (i + 1);
    }
    return 0;
}

/**
 */
static void defaultMachine(/*@out@*/ const char ** arch,
		/*@out@*/ const char ** os)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies *arch, *os, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    static struct utsname un;
    static int gotDefaults = 0;
    int rc;

    while (!gotDefaults) {
	CVOG_t cvog = NULL;
	rc = uname(&un);
	if (rc < 0) return;

	if (!rpmPlatform(platform)) {
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

void rpmSetTables(int archTable, int osTable)
	/*@globals currTables @*/
	/*@modifies currTables @*/
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

int rpmMachineScore(int type, const char * name)
{
    machEquivInfo info = machEquivSearch(&tables[type].equiv, name);
    return (info != NULL ? info->score : 0);
}

/*@-modnomods@*/
void rpmGetMachine(const char ** arch, const char ** os)
{
    if (arch)
	*arch = current[ARCH];

    if (os)
	*os = current[OS];
}
/*@=modnomods@*/

void rpmSetMachine(const char * arch, const char * os)
	/*@globals current @*/
	/*@modifies current @*/
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

void rpmGetArchInfo(const char ** name, int * num)
{
    getMachineInfo(ARCH, name, num);
}

void rpmGetOsInfo(const char ** name, int * num)
{
    getMachineInfo(OS, name, num);
}

static void rpmRebuildTargetVars(const char ** target, const char ** canontarget)
{

    char *ca = NULL, *co = NULL, *ct = NULL;
    int x;

    /* Rebuild the compat table to recalculate the current target arch.  */

    rpmSetMachine(NULL, NULL);
    rpmSetTables(RPM_MACHTABLE_INSTARCH, RPM_MACHTABLE_INSTOS);
    rpmSetTables(RPM_MACHTABLE_BUILDARCH, RPM_MACHTABLE_BUILDOS);

    /*@-branchstate@*/
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
    /*@=branchstate@*/

    /* If still not set, Set target arch/os from default uname(2) values */
    if (ca == NULL) {
	const char *a = NULL;
	defaultMachine(&a, NULL);
	ca = (a) ? xstrdup(a) : NULL;
    }
    if (ca != NULL)
    for (x = 0; ca[x] != '\0'; x++)
	ca[x] = xtolower(ca[x]);

    if (co == NULL) {
	const char *o = NULL;
	defaultMachine(NULL, &o);
	co = (o) ? xstrdup(o) : NULL;
    }
    if (co != NULL)
    for (x = 0; co[x] != '\0'; x++)
	co[x] = xtolower(co[x]);

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

    /*@-branchstate@*/
    if (canontarget)
	*canontarget = ct;
    else
	ct = _free(ct);
    /*@=branchstate@*/
    ca = _free(ca);
    /*@-usereleased@*/
    co = _free(co);
    /*@=usereleased@*/
}

void rpmFreeRpmrc(void)
	/*@globals current, tables, values, defaultsInitialized @*/
	/*@modifies current, tables, values, defaultsInitialized @*/
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
 * @return		0 on succes
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
	    
	/*@-branchstate@*/
	if (mfpath != NULL) {
	    rpmInitMacros(NULL, mfpath);
	    mfpath = _free(mfpath);
	}
	/*@=branchstate@*/
    }

    return rc;
}

int rpmReadConfigFiles(const char * file, const char * target)
	/*@globals configTarget @*/
	/*@modifies configTarget @*/
{

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
/*@-globs@*/
	s = rpmExpand(rpmMacrofiles, NULL);
/*@=globs@*/
	fprintf(fp, "\nMACRO DEFINITIONS:\n");
	fprintf(fp, "%-21s : %s\n", "macrofiles", ((s && *s) ? s : "(not set)"));
	s = _free(s);
    }

    if (rpmIsVerbose()) {
	rpmPRCO PRCO = rpmdsNewPRCO(NULL);
	xx = rpmdsSysinfo(PRCO, NULL);
	ds = rpmdsFromPRCO(PRCO, RPMTAG_PROVIDENAME);
	if (ds != NULL) {
	    fprintf(fp, _("Configured system provides (from /etc/rpm/sysinfo):\n"));
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
	    fprintf(fp,
		_("Features provided by current cpuinfo (from /proc/cpuinfo):\n"));
	    ds = rpmdsInit(ds);
	    while (rpmdsNext(ds) >= 0) {
		const char * DNEVR = rpmdsDNEVR(ds);
		if (DNEVR != NULL)
		    fprintf(fp, "    %s\n", DNEVR+2);
	    }
	    ds = rpmdsFree(ds);
	    fprintf(fp, "\n");
	}

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
/*@=bounds@*/

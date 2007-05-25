#ifndef H_RPMDS
#define H_RPMDS

/** \ingroup rpmds
 * \file lib/rpmds.h
 * Structure(s) used for dependency tag sets.
 */

#include "rpmevr.h"
#define	_RPMNS_INTERNAL
#include "rpmns.h"
#include "rpmps.h"

/**
 */
/*@-exportlocal@*/
/*@unchecked@*/
extern int _rpmds_debug;
/*@=exportlocal@*/

/**
 */
/*@-exportlocal@*/
/*@unchecked@*/
extern int _rpmds_nopromote;
/*@=exportlocal@*/

#if defined(_RPMDS_INTERNAL)
/** \ingroup rpmds
 * A dependency set.
 */
struct rpmds_s {
/*@observer@*/
    const char * Type;		/*!< Tag name. */
/*@only@*/ /*@null@*/
    const char * DNEVR;		/*!< Formatted dependency string. */
/*@refcounted@*/ /*@null@*/
    Header h;			/*!< Header for dependency set (or NULL) */
/*@only@*/ /*@relnull@*/
    const char ** N;		/*!< Name. */
/*@only@*/ /*@relnull@*/
    const char ** EVR;		/*!< Epoch-Version-Release. */
/*@only@*/ /*@relnull@*/
    int_32 * Flags;		/*!< Bit(s) identifying context/comparison. */
/*@only@*/ /*@null@*/
    uint_32 * Color;		/*!< Bit(s) calculated from file color(s). */
/*@only@*/ /*@null@*/
    int_32 * Refs;		/*!< No. of file refs. */
/*@only@*/ /*@null@*/
    int_32 * Result;		/*!< Dependency check result. */
/*@null@*/
    int (*EVRparse) (const char *evrstr, EVR_t evr);	 /* EVR parsing. */
    int (*EVRcmp) (const char *a, const char *b);	 /* EVR comparison. */
    struct rpmns_s ns;		/*!< Name (split). */
/*@only@*/ /*@null@*/
    const char * A;		/*!< Arch (from containing package). */
    int_32 BT;			/*!< Package build time tie breaker. */
    rpmTag tagN;		/*!< Header tag. */
    rpmTagType Nt, EVRt, Ft;	/*!< Tag data types. */
    int_32 Count;		/*!< No. of elements */
    int i;			/*!< Element index. */
    unsigned l;			/*!< Low element (bsearch). */
    unsigned u;			/*!< High element (bsearch). */
    int nopromote;		/*!< Don't promote Epoch: in rpmdsCompare()? */
/*@refs@*/
    int nrefs;			/*!< Reference count. */
};
#endif	/* _RPMDS_INTERNAL */

#if defined(_RPMPRCO_INTERNAL)
/** \ingroup rpmds
 * Container for provides/requires/conflicts/obsoletes dependency set(s).
 */
struct rpmPRCO_s {
/*@dependent@*/ /*@relnull@*/
    rpmds * Pdsp;		/*!< Provides: collector. */
/*@dependent@*/ /*@relnull@*/
    rpmds * Rdsp;		/*!< Requires: collector. */
/*@dependent@*/ /*@relnull@*/
    rpmds * Cdsp;		/*!< Conflicts: collector. */
/*@dependent@*/ /*@relnull@*/
    rpmds * Odsp;		/*!< Obsoletes: collector. */
/*@dependent@*/ /*@relnull@*/
    rpmds * Tdsp;		/*!< Triggers collector. */
/*@dependent@*/ /*@relnull@*/
    rpmds * Ddsp;		/*!< Dirnames collector. */
/*@dependent@*/ /*@relnull@*/
    rpmds * Ldsp;		/*!< Linktos collector. */
/*@refcounted@*/ /*@null@*/
    rpmds this;		/*!< N = EVR */
/*@refcounted@*/ /*@null@*/
    rpmds P;		/*!< Provides: */
/*@refcounted@*/ /*@null@*/
    rpmds R;		/*!< Requires: */
/*@refcounted@*/ /*@null@*/
    rpmds C;		/*!< Conflicts: */
/*@refcounted@*/ /*@null@*/
    rpmds O;		/*!< Obsoletes: */
/*@refcounted@*/ /*@null@*/
    rpmds T;		/*!< Triggers */
/*@refcounted@*/ /*@null@*/
    rpmds D;		/*!< Dirnames */
/*@refcounted@*/ /*@null@*/
    rpmds L;		/*!< Linktos */
};
#endif	/* _RPMPRCO_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/** \name RPMDS */
/*@{*/
/**
 * Unreference a dependency set instance.
 * @param ds		dependency set
 * @param msg
 * @return		NULL always
 */
/*@unused@*/ /*@null@*/
rpmds rpmdsUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmds ds,
		/*@null@*/ const char * msg)
	/*@modifies ds @*/;

/** @todo Remove debugging entry from the ABI. */
/*@-exportlocal@*/
/*@null@*/
rpmds XrpmdsUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmds ds,
		/*@null@*/ const char * msg, const char * fn, unsigned ln)
	/*@modifies ds @*/;
/*@=exportlocal@*/
#define	rpmdsUnlink(_ds, _msg)	XrpmdsUnlink(_ds, _msg, __FILE__, __LINE__)

/**
 * Reference a dependency set instance.
 * @param ds		dependency set
 * @param msg
 * @return		new dependency set reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmds rpmdsLink (/*@null@*/ rpmds ds, /*@null@*/ const char * msg)
	/*@modifies ds @*/;

/** @todo Remove debugging entry from the ABI. */
/*@newref@*/ /*@null@*/
rpmds XrpmdsLink (/*@null@*/ rpmds ds, /*@null@*/ const char * msg,
		const char * fn, unsigned ln)
        /*@modifies ds @*/;
#define	rpmdsLink(_ds, _msg)	XrpmdsLink(_ds, _msg, __FILE__, __LINE__)

/**
 * Destroy a dependency set.
 * @param ds		dependency set
 * @return		NULL always
 */
/*@null@*/
rpmds rpmdsFree(/*@killref@*/ /*@only@*/ /*@null@*/ rpmds ds)
	/*@modifies ds @*/;
/**
 * Create and load a dependency set.
 * @param h		header
 * @param tagN		type of dependency
 * @param flags		scareMem(0x1), nofilter(0x2)
 * @return		new dependency set
 */
/*@null@*/
rpmds rpmdsNew(Header h, rpmTag tagN, int flags)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies h, rpmGlobalMacroContext, fileSystem, internalState @*/;

/**
 * Return N string, expanded if necessary.
 * @param ds		dependency set
 * @return		new N string (malloc'ed)
 */
const char * rpmdsNewN(rpmds ds)
	/*@globals rpmGlobalMacroContext, h_errno @*/
	/*@modifies ds, rpmGlobalMacroContext @*/;

/**
 * Return new formatted dependency string.
 * @param dspfx		formatted dependency string prefix
 * @param ds		dependency set
 * @return		new formatted dependency (malloc'ed)
 */
char * rpmdsNewDNEVR(const char * dspfx, rpmds ds)
	/*@globals rpmGlobalMacroContext, h_errno @*/
	/*@modifies ds, rpmGlobalMacroContext @*/;

/**
 * Create, load and initialize a dependency for this header. 
 * @param h		header
 * @param tagN		type of dependency
 * @param Flags		comparison flags
 * @return		new dependency set
 */
/*@null@*/
rpmds rpmdsThis(Header h, rpmTag tagN, int_32 Flags)
	/*@*/;

/**
 * Create, load and initialize a dependency set of size 1.
 * @param tagN		type of dependency
 * @param N		name
 * @param EVR		epoch:version-release
 * @param Flags		comparison/context flags
 * @return		new dependency set
 */
/*@null@*/
rpmds rpmdsSingle(rpmTag tagN, const char * N, const char * EVR, int_32 Flags)
	/*@*/;

/**
 * Return dependency set count.
 * @param ds		dependency set
 * @return		current count
 */
int rpmdsCount(/*@null@*/ const rpmds ds)
	/*@*/;

/**
 * Return dependency set index.
 * @param ds		dependency set
 * @return		current index
 */
int rpmdsIx(/*@null@*/ const rpmds ds)
	/*@*/;

/**
 * Set dependency set index.
 * @param ds		dependency set
 * @param ix		new index
 * @return		current index
 */
int rpmdsSetIx(/*@null@*/ rpmds ds, int ix)
	/*@modifies ds @*/;

/**
 * Return current formatted dependency string.
 * @param ds		dependency set
 * @return		current dependency DNEVR, NULL on invalid
 */
/*@observer@*/ /*@relnull@*/
extern const char * rpmdsDNEVR(/*@null@*/ const rpmds ds)
	/*@*/;

/**
 * Return current dependency name.
 * @param ds		dependency set
 * @return		current dependency name, NULL on invalid
 */
/*@observer@*/ /*@null@*/
extern const char * rpmdsN(/*@null@*/ rpmds ds)
	/*@*/;

/**
 * Return current dependency epoch-version-release.
 * @param ds		dependency set
 * @return		current dependency EVR, NULL on invalid
 */
/*@observer@*/ /*@relnull@*/
extern const char * rpmdsEVR(/*@null@*/ const rpmds ds)
	/*@*/;

/**
 * Return current dependency flags.
 * @param ds		dependency set
 * @return		current dependency flags, 0 on invalid
 */
int_32 rpmdsFlags(/*@null@*/ const rpmds ds)
	/*@*/;

/**
 * Return current dependency type.
 * @param ds		dependency set
 * @return		current dependency type, 0 on invalid
 */
rpmTag rpmdsTagN(/*@null@*/ const rpmds ds)
	/*@*/;

/**
 * Return current dependency arch.
 * @param ds		dependency set
 * @return		current dependency arch, NULL on invalid
 */
/*@observer@*/ /*@relnull@*/
extern const char * rpmdsA(/*@null@*/ const rpmds ds)
	/*@*/;

/**
 * Return dependency build time.
 * @param ds		dependency set
 * @return		dependency build time, 0 on invalid
 */
time_t rpmdsBT(/*@null@*/ const rpmds ds)
	/*@*/;

/**
 * Set dependency build time.
 * @param ds		dependency set
 * @param BT		build time
 * @return		dependency build time, 0 on invalid
 */
time_t rpmdsSetBT(/*@null@*/ const rpmds ds, time_t BT)
	/*@modifies ds @*/;

/**
 * Return dependency class type.
 * @param ds		dependency set
 * @return		dependency class type
 */
nsType rpmdsNSType(/*@null@*/ const rpmds ds)
	/*@*/;

/**
 * Return current "Don't promote Epoch:" flag.
 *
 * This flag controls for Epoch: promotion when a dependency set is
 * compared. If the flag is set (for already installed packages), then
 * an unspecified value will be treated as Epoch: 0. Otherwise (for added
 * packages), the Epoch: portion of the comparison is skipped if the value
 * is not specified, i.e. an unspecified Epoch: is assumed to be equal
 * in dependency comparisons.
 *
 * @param ds		dependency set
 * @return		current "Don't promote Epoch:" flag
 */
int rpmdsNoPromote(/*@null@*/ const rpmds ds)
	/*@*/;

/**
 * Set "Don't promote Epoch:" flag.
 * @param ds		dependency set
 * @param nopromote	Should an unspecified Epoch: be treated as Epoch: 0?
 * @return		previous "Don't promote Epoch:" flag
 */
int rpmdsSetNoPromote(/*@null@*/ rpmds ds, int nopromote)
	/*@modifies ds @*/;

/**
 * Set EVR parsing function.
 * @param ds		dependency set
 * @param EVRparse	EVR parsing function (NULL uses default)
 * @return		previous EVR parsing function
 */
/*@null@*/
void * rpmdsSetEVRparse(/*@null@*/ rpmds ds,
		/*@null@*/ int (*EVRparse)(const char *everstr, EVR_t evr))
	/*@modifies ds @*/;

/**
 * Set EVR comparison function.
 * @param ds		dependency set
 * @param EVRcmp	EVR comparison function (NULL uses default)
 * @return		previous EVR comparison function
 */
/*@null@*/
void * rpmdsSetEVRcmp(/*@null@*/ rpmds ds,
		/*@null@*/ int (*EVRcmp)(const char *a, const char *b))
	/*@modifies ds @*/;

/**
 * Return current dependency color.
 * @param ds		dependency set
 * @return		current dependency color (0 if not set)
 */
uint_32 rpmdsColor(/*@null@*/ const rpmds ds)
	/*@*/;

/**
 * Set current dependency color.
 * @param ds		dependency set
 * @param color		new dependency color
 * @return		previous dependency color
 */
uint_32 rpmdsSetColor(/*@null@*/ const rpmds ds, uint_32 color)
	/*@modifies ds @*/;

/**
 * Return current dependency file refs.
 * @param ds		dependency set
 * @return		current dependency file refs (0 if not set)
 */
int_32 rpmdsRefs(/*@null@*/ const rpmds ds)
	/*@*/;

/**
 * Set current dependency file refs.
 * @param ds		dependency set
 * @param refs		new dependency refs
 * @return		previous dependency refs
 */
int_32 rpmdsSetRefs(/*@null@*/ const rpmds ds, int_32 refs)
	/*@modifies ds @*/;

/**
 * Return current dependency comparison result.
 * @param ds		dependency set
 * @return		current dependency result (0 if not set)
 */
int_32 rpmdsResult(/*@null@*/ const rpmds ds)
	/*@*/;

/**
 * Set current dependency comparison result.
 * @param ds		dependency set
 * @param result	new dependency result
 * @return		previous dependency result
 */
int_32 rpmdsSetResult(/*@null@*/ const rpmds ds, int_32 result)
	/*@modifies ds @*/;

/**
 * Notify of results of dependency match.
 * @param ds		dependency set
 * @param where		where dependency was resolved (or NULL)
 * @param rc		0 == YES, otherwise NO
 */
/*@-globuse@*/ /* FIX: rpmMessage annotation is a lie */
void rpmdsNotify(/*@null@*/ rpmds ds, /*@null@*/ const char * where, int rc)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
/*@=globuse@*/

/**
 * Return next dependency set iterator index.
 * @param ds		dependency set
 * @return		dependency set iterator index, -1 on termination
 */
int rpmdsNext(/*@null@*/ rpmds ds)
	/*@modifies ds @*/;

/**
 * Initialize dependency set iterator.
 * @param ds		dependency set
 * @return		dependency set
 */
/*@null@*/
rpmds rpmdsInit(/*@null@*/ rpmds ds)
	/*@modifies ds @*/;

/**
 * Find a dependency set element using binary search.
 * @param ds		dependency set to search
 * @param ods		dependency set element to find.
 * @return		dependency index (or -1 if not found)
 */
int rpmdsFind(rpmds ds, /*@null@*/ const rpmds ods)
	/*@modifies ds @*/;

/**
 * Merge a dependency set maintaining (N,EVR,Flags) sorted order.
 * @retval *dsp		(merged) dependency set
 * @param ods		dependency set to merge
 * @return		0 on success
 */
int rpmdsMerge(/*@null@*/ /*@out@*/ rpmds * dsp, /*@null@*/ rpmds ods)
	/*@modifies *dsp, ods @*/;

/**
 * Search a sorted dependency set for an element that overlaps.
 * A boolean result is saved (if allocated) and accessible through
 * rpmdsResult(ods) afterwards.
 * @param ds		dependency set to search
 * @param ods		dependency set element to find.
 * @return		dependency index (or -1 if not found)
 */
int rpmdsSearch(/*@null@*/ rpmds ds, /*@null@*/ rpmds ods)
	/*@modifies ds, ods @*/;

/**
 * Load /proc/cpuinfo provides into a dependency set.
 * @retval *dsp		(loaded) depedency set
 * @param fn		path to file (NULL uses /proc/cpuinfo)
 * @return		0 on success
 */
int rpmdsCpuinfo(/*@out@*/ rpmds * dsp, /*@null@*/ const char * fn)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies *dsp, rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/;

/**
 * Load rpmlib provides into a dependency set.
 * @retval *dsp		(loaded) depedency set
 * @param tblp		rpmlib provides table (NULL uses internal table)
 * @return		0 on success
 */
int rpmdsRpmlib(rpmds * dsp, /*@null@*/ void * tblp)
	/*@modifies *dsp @*/;

/**
 * Load sysinfo dependencies into a dependency set.
 * @retval *PRCO	provides/requires/conflicts/obsoletes depedency set(s)
 * @param fn		path to file (NULL uses /etc/rpm/sysinfo)
 * @return		0 on success
 */
int rpmdsSysinfo(rpmPRCO PRCO, /*@null@*/ const char * fn)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies PRCO, rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/;

/**
 * Load getconf provides into a dependency set.
 * @retval *dsp		(loaded) depedency set
 * @param path		getconf path (NULL uses /)
 * @return		0 on success
 */
int rpmdsGetconf(rpmds * dsp, /*@null@*/ const char * path)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies *dsp, rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/;

/**
 * Merge provides/requires/conflicts/obsoletes dependencies.
 * @param context	merge dependency set(s) container
 * @param ds		dependency set to merge
 * @return		0 on success
 */
int rpmdsMergePRCO(void * context, rpmds ds)
	/*@modifies context, ds @*/;

/**
 * Free dependency set(s) container.
 * @param PRCO		dependency set(s) container
 * @return		NULL
 */
/*@null@*/
rpmPRCO rpmdsFreePRCO(/*@only@*/ /*@null@*/ rpmPRCO PRCO)
	/*@modifies PRCO @*/;

/**
 * Create dependency set(s) container.
 * @param h		header
 * @return		0 on success
 */
rpmPRCO rpmdsNewPRCO(/*@null@*/ Header h)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies h, rpmGlobalMacroContext, fileSystem, internalState @*/;

/**
 * Retrieve a dependency set from container.
 * @param PRCO		dependency set(s) container
 * @param tagN		type of dependency set
 * @return		dependency set (or NULL)
 */
/*@null@*/
rpmds rpmdsFromPRCO(/*@null@*/ rpmPRCO PRCO, rpmTag tagN)
	/*@*/;

/**
 * Extract ELF dependencies from a file.
 * @param fn		file name
 * @param flags		1: skip provides 2: skip requires
 * @param *add		add(arg, ds) saves next provide/require elf dependency.
 * @param context	add() callback context
 * @return		0 on success
 */
int rpmdsELF(const char * fn, int flags,
		int (*add) (void * context, rpmds ds), void * context)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext, fileSystem, internalState @*/;
#define RPMELF_FLAG_SKIPPROVIDES	0x1	/*<! rpmdsELF: skip provides */
#define RPMELF_FLAG_SKIPREQUIRES	0x2	/*<! rpmdsELF: skip requires */

/**
 * Load /etc/ld.so.cache provides into a dependency set.
 * @todo Add dependency colors, and attach to file.
 * @retval *PRCO	provides/requires/conflicts/obsoletes depedency set(s)
 * @param fn		cache file name (NULL uses /etc/ld.so.cache)
 * @return		0 on success
 */
int rpmdsLdconfig(rpmPRCO PRCO, /*@null@*/ const char * fn)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies *PRCO, rpmGlobalMacroContext, fileSystem, internalState @*/;

#if defined(__sun)
/**
 * Given a colon-separated list of directories, search each of those
 * directories for (ELF or ELF64) shared objects, and load the provided
 * shared objects into a dependency set.
 * @todo Add dependency colors, and attach to file.
 * @retval *PRCO	provides/requires/conflicts/obsoletes depedency set(s)
 * @param rldp		:-sep string of dirs (NULL uses /lib:/usr/lib)
 * @return		0 on success
 */
int rpmdsRldpath(rpmPRCO PRCO, /*@null@*/ const char * rldp)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies *PRCO, rpmGlobalMacroContext, fileSystem, internalState @*/;

/**
 * Use Solaris' crle(1) command to find the ELF (or ELF64) loader path.
 * calls rpmdsRldpath once it has the loader path.
 * @todo Add dependency colors, and attach to file.
 * @retval *PRCO	provides/requires/conflicts/obsoletes depedency set(s)
 * @param fn		unused.
 * @return		0 on success
 */
int rpmdsCrle(rpmPRCO PRCO, /*@null@*/ const char * fn)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies *PRCO, rpmGlobalMacroContext, fileSystem, internalState @*/;
#endif

/**
 * Load uname(2) provides into a dependency set.
 * @retval *dsp		(loaded) depedency set
 * @param un		utsname struct (NULL calls uname(2))
 * @return		0 on success
 */
int rpmdsUname(rpmds * dsp, /*@null@*/ const struct utsname * un)
	/*@globals internalState @*/
	/*@modifies *dsp, internalState @*/;

/**
 * Load provides from a pipe into a dependency set.
 * @retval *dsp		(loaded) depedency set
 * @param tagN		rpmds tag (<= 0 uses RPMTAG_PROVIDENAME).
 * @param cmd		popen cmd to run (NULL loads perl provides)
 * @return		0 on success
 */
int rpmdsPipe(rpmds * dsp, int_32 tagN, /*@null@*/ const char * cmd)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies *dsp, rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/;

/**
 * Compare two versioned dependency ranges, looking for overlap.
 * @param A		1st dependency
 * @param B		2nd dependency
 * @return		1 if dependencies overlap, 0 otherwise
 */
int rpmdsCompare(const rpmds A, const rpmds B)
	/*@*/;

/**
 * Report a Requires: or Conflicts: dependency problem.
 * @param ps		transaction set problems
 * @param pkgNEVR	package name/epoch/version/release
 * @param ds		dependency set
 * @param suggestedKeys	filename or python object address
 * @param adding	dependency problem is from added package set?
 */
void rpmdsProblem(/*@null@*/ rpmps ps, const char * pkgNEVR, const rpmds ds,
		/*@only@*/ /*@null@*/ const fnpyKey * suggestedKeys,
		int adding)
	/*@modifies ps @*/;

/**
 * Compare package provides dependencies from header with a single dependency.
 * @param h		header
 * @param req		dependency set
 * @param nopromote	Don't promote Epoch: in comparison?
 * @return		1 if any dependency overlaps, 0 otherwise
 */
int rpmdsAnyMatchesDep (const Header h, const rpmds req, int nopromote)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies h, rpmGlobalMacroContext, fileSystem, internalState @*/;

/**
 * Compare package name-version-release from header with a single dependency.
 * @param h		header
 * @param req		dependency set
 * @param nopromote	Don't promote Epoch: in comparison?
 * @return		1 if dependency overlaps, 0 otherwise
 */
int rpmdsNVRMatchesDep(const Header h, const rpmds req, int nopromote)
	/*@*/;

/**
 * Negate return code for negated comparisons.
 * @param ds		dependency set
 * @param rc		postive return code
 * @return		return code
 */
int rpmdsNegateRC(const rpmds ds, int rc)
	/*@*/;

#if !defined(SWIG)
/**
 * Return current dependency type name.
 * @param ds		dependency set
 * @return		current dependency type name
 */
/*@unused@*/ static inline /*@observer@*/
const char * rpmdsTagName(/*@null@*/ const rpmds ds)
	/*@*/
{
    int tagN = rpmdsTagN(ds);

    switch (tagN) {
    case RPMTAG_PROVIDENAME:	return "Provides";	/*@notreached@*/ break;
    case RPMTAG_REQUIRENAME:	return "Requires";	/*@notreached@*/ break;
    case RPMTAG_CONFLICTNAME:	return "Conflicts";	/*@notreached@*/ break;
    case RPMTAG_OBSOLETENAME:	return "Obsoletes";	/*@notreached@*/ break;
    case RPMTAG_TRIGGERNAME:	return "Triggers";	/*@notreached@*/ break;
    case RPMTAG_DIRNAMES:	return "Dirnames";	/*@notreached@*/ break;
    }
    return tagName(tagN);
}

/**
 * Print current dependency set contents.
 * @param ds		dependency set
 * @param fp		file handle (NULL uses stderr)
 * @return		0 always
 */
/*@unused@*/ static inline
int rpmdsPrint(/*@null@*/ rpmds ds, /*@null@*/ FILE * fp)
	/*@globals fileSystem @*/
	/*@modifies ds, *fp, fileSystem @*/
{
    if (fp == NULL)
	fp = stderr;
    ds = rpmdsInit(ds);
    while (rpmdsNext(ds) >= 0)
        fprintf(fp, "%6d\t%s: %s\n", rpmdsIx(ds), rpmdsTagName(ds), rpmdsDNEVR(ds)+2);
    return 0;
}

/**
 * Print current dependency set results.
 * @param ds		dependency set
 * @param fp		file handle (NULL uses stderr)
 * @return		0 always
 */
/*@unused@*/ static inline
int rpmdsPrintResults(/*@null@*/ rpmds ds, /*@null@*/ FILE * fp)
	/*@globals fileSystem @*/
	/*@modifies ds, *fp, fileSystem @*/
{
    if (fp == NULL)
	fp = stderr;
    ds = rpmdsInit(ds);
    while (rpmdsNext(ds) >= 0) {
	int rc = rpmdsResult(ds);
	if (rc > 0)
	    continue;
        fprintf(fp, "%6d\t%s: %s\n", rpmdsIx(ds), rpmdsTagName(ds), rpmdsDNEVR(ds)+2);
    }
    return 0;
}

/**
 * Check Provides: against Requires: and print closure results.
 * @param P		Provides: dependency set
 * @param R		Requires: dependency set
 * @param fp		file handle (NULL uses stderr)
 * @return		0 always
 */
/*@-mods@*/	/* XXX LCL wonky */
/*@unused@*/ static inline
int rpmdsPrintClosure(/*@null@*/ rpmds P, /*@null@*/ rpmds R,
		/*@null@*/ FILE * fp)
	/*@globals fileSystem @*/
	/*@modifies P, R, *fp, fileSystem @*/
{
    int rc;

    /* Allocate the R results array (to be filled in by rpmdsSearch). */
    (void) rpmdsSetResult(R, 0);	/* allocate result array. */

    /* Collect the rpmdsSearch results (in the R dependency set). */
    R = rpmdsInit(R);
    while (rpmdsNext(R) >= 0)
        rc = rpmdsSearch(P, R);

    return rpmdsPrintResults(R, fp);
}
/*@=mods@*/
#endif
/*@}*/

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMDS */

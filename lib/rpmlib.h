#ifndef H_RPMLIB
#define	H_RPMLIB

/** \ingroup rpmcli rpmrc rpmts rpmte rpmds rpmfi rpmdb lead signature header payload dbi
 * \file lib/rpmlib.h
 *
 * In Memoriam: Steve Taylor <staylor@redhat.com> was here, now he's not.
 *
 */

#include <rpmtag.h>
#include <rpmversion.h>

#define RPM_FORMAT_VERSION 5
#define RPM_MAJOR_VERSION 0
#define RPM_MINOR_VERSION 0

/**
 * Package read return codes.
 */
typedef	enum rpmRC_e {
    RPMRC_OK		= 0,	/*!< Generic success code */
    RPMRC_NOTFOUND	= 1,	/*!< Generic not found code. */
    RPMRC_FAIL		= 2,	/*!< Generic failure code. */
    RPMRC_NOTTRUSTED	= 3,	/*!< Signature is OK, but key is not trusted. */
    RPMRC_NOKEY		= 4	/*!< Public key is unavailable. */
} rpmRC;

/*@-redecl@*/
/*@checked@*/
extern struct MacroContext_s * rpmGlobalMacroContext;

/*@checked@*/
extern struct MacroContext_s * rpmCLIMacroContext;

/*@unchecked@*/ /*@observer@*/
extern const char * RPMVERSION;

/*@unchecked@*/ /*@observer@*/
extern const char * rpmNAME;

/*@unchecked@*/ /*@observer@*/
extern const char * rpmEVR;

/*@unchecked@*/
extern int rpmFLAGS;
/*@=redecl@*/

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmts
 * The RPM Transaction Set.
 * Transaction sets are inherently unordered! RPM may reorder transaction
 * sets to reduce errors. In general, installs/upgrades are done before
 * strict removals, and prerequisite ordering is done on installs/upgrades.
 */
typedef /*@abstract@*/ /*@refcounted@*/ struct rpmts_s * rpmts;

/** \ingroup rpmbuild
 */
typedef struct Spec_s * Spec;

/** \ingroup rpmts
 * An added/available package retrieval key.
 */
typedef /*@abstract@*/ void * alKey;
#define	RPMAL_NOMATCH	((alKey)-1L)

/** \ingroup rpmts
 * An added/available package retrieval index.
 */
/*@-mutrep@*/
typedef /*@abstract@*/ int alNum;
/*@=mutrep@*/

/** \ingroup rpmds 
 * Dependency tag sets from a header, so that a header can be discarded early.
 */
typedef /*@abstract@*/ /*@refcounted@*/ struct rpmds_s * rpmds;

/** \ingroup rpmds 
 * Container for commonly extracted dependency set(s).
 */
typedef struct rpmPRCO_s * rpmPRCO;

/** \ingroup rpmfi
 * File info tag sets from a header, so that a header can be discarded early.
 */
typedef /*@abstract@*/ /*@refcounted@*/ struct rpmfi_s * rpmfi;

/** \ingroup rpmte
 * An element of a transaction set, i.e. a TR_ADDED or TR_REMOVED package.
 */
typedef /*@abstract@*/ struct rpmte_s * rpmte;

/** \ingroup rpmdb
 * Database of headers and tag value indices.
 */
typedef /*@abstract@*/ /*@refcounted@*/ struct rpmdb_s * rpmdb;

/** \ingroup rpmdb
 * Database iterator.
 */
typedef /*@abstract@*/ struct rpmdbMatchIterator_s * rpmdbMatchIterator;

/** \ingroup rpmgi
 * Generalized iterator.
 */
typedef /*@abstract@*/ /*@refcounted@*/ struct rpmgi_s * rpmgi;

/**
 * Table of query format extensions.
 * @note Chains *headerCompoundFormats -> *headerDefaultFormats.
 */
/*@-redecl@*/
/*@unchecked@*/
extern headerSprintfExtension rpmHeaderFormats;
/*@=redecl@*/

/**
 * Scriptlet identifiers.
 */
typedef enum rpmScriptID_e {
    RPMSCRIPT_UNKNOWN		=  0,	/*!< unknown scriptlet */
    RPMSCRIPT_PRETRANS		=  1,	/*!< %pretrans scriptlet */
    RPMSCRIPT_TRIGGERPREIN	=  2,	/*!< %triggerprein scriptlet */
    RPMSCRIPT_PREIN		=  3,	/*!< %pre scriptlet */
    RPMSCRIPT_POSTIN		=  4,	/*!< %post scriptlet  */
    RPMSCRIPT_TRIGGERIN		=  5,	/*!< %triggerin scriptlet  */
    RPMSCRIPT_TRIGGERUN		=  6,	/*!< %triggerun scriptlet  */
    RPMSCRIPT_PREUN		=  7,	/*!< %preun scriptlet  */
    RPMSCRIPT_POSTUN		=  8,	/*!< %postun scriptlet  */
    RPMSCRIPT_TRIGGERPOSTUN	=  9,	/*!< %triggerpostun scriptlet  */
    RPMSCRIPT_POSTTRANS		= 10,	/*!< %posttrans scriptlet  */
	/* 11-15 unused */
    RPMSCRIPT_VERIFY		= 16,	/*!< %verify scriptlet  */
    RPMSCRIPT_MAX		= 32
} rpmScriptID;

/**
 * Scriptlet states (when installed).
 */
typedef enum rpmScriptState_e {
    RPMSCRIPT_STATE_UNKNOWN	= 0,
	/* 0-15 reserved for waitpid return. */
    RPMSCRIPT_STATE_EXEC	= (1 << 16), /*!< scriptlet was exec'd */
    RPMSCRIPT_STATE_REAPED	= (1 << 17), /*!< scriptlet was reaped */
	/* 18-23 unused */
    RPMSCRIPT_STATE_SELINUX	= (1 << 24), /*!< scriptlet exec by SELinux */
    RPMSCRIPT_STATE_EMULATOR	= (1 << 25), /*!< scriptlet exec in emulator */
    RPMSCRIPT_STATE_LUA		= (1 << 26)  /*!< scriptlet exec with lua */
} rpmScriptState;

/* ==================================================================== */
/** \name RPMRC */
/*@{*/

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

/** \ingroup rpmrc
 * Read macro configuration file(s) for a target.
 * @param file		NULL always
 * @param target	target platform (NULL uses default)
 * @return		0 on success, -1 on error
 */
int rpmReadConfigFiles(/*@null@*/ const char * file,
		/*@null@*/ const char * target)
	/*@globals rpmGlobalMacroContext, rpmCLIMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext, rpmCLIMacroContext,
		fileSystem, internalState @*/;

/*@only@*/ /*@null@*/ /*@unchecked@*/
extern void * platpat;
/*@unchecked@*/
extern int nplatpat;

/** \ingroup rpmrc
 * Return score of a platform string.
 * A platform score measures the "nearness" of a platform string wrto
 * configured platform patterns. The returned score is the line number
 * of the 1st pattern in /etc/rpm/platform that matches the input string.
 *
 * @param platform	cpu-vendor-os platform string
 * @param mi_re		pattern array (NULL uses /etc/rpm/platform patterns)
 * @param mi_nre	no. of patterns
 * @return		platform score (0 is no match, lower is preferred)
 */
int rpmPlatformScore(const char * platform, /*@null@*/ void * mi_re, int mi_nre)
	/*@modifies mi_re @*/;

/** \ingroup rpmrc
 * Display current rpmrc (and macro) configuration.
 * @param fp		output file handle
 * @return		0 always
 */
int rpmShowRC(FILE * fp)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies *fp, rpmGlobalMacroContext, fileSystem, internalState  @*/;

/** \ingroup rpmrc
 * @deprecated Use addMacro to set _target_* macros.
 * @todo	Eliminate in rpm-5.1.
 # @note Only used by build code.
 * @param archTable
 * @param osTable
 */
void rpmSetTables(int archTable, int osTable)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext, fileSystem, internalState @*/;

/** \ingroup rpmrc
 * @todo	Eliminate in rpm-5.1.
 * Destroy rpmrc arch/os compatibility tables.
 * @todo Eliminate from API.
 */
void rpmFreeRpmrc(void)
	/*@globals platpat, nplatpat, internalState @*/
	/*@modifies platpat, nplatpat, internalState @*/;

/*@}*/
/* ==================================================================== */
/** \name RPMTS */
/*@{*/

/**
 * We pass these around as an array with a sentinel.
 */
typedef struct rpmRelocation_s * rpmRelocation;
#if !defined(SWIG)
struct rpmRelocation_s {
/*@only@*/ /*@null@*/
    const char * oldPath;	/*!< NULL here evals to RPMTAG_DEFAULTPREFIX, */
/*@only@*/ /*@null@*/
    const char * newPath;	/*!< NULL means to omit the file completely! */
};
#endif

/**
 * Compare headers to determine which header is "newer".
 * @deprecated Use rpmdsCompare instead.
 * @todo	Eliminate in rpm-5.1.
 * @param first		1st header
 * @param second	2nd header
 * @return		result of comparison
 */
int rpmVersionCompare(Header first, Header second)
	/*@*/;

/** \ingroup payload
 * Iterator across package file info, forward on install, backward on erase.
 */
typedef /*@abstract@*/ struct fsmIterator_s * FSMI_t;

/** \ingroup payload
 * File state machine data.
 */
typedef /*@abstract@*/ struct fsm_s * FSM_t;

/** \ingroup rpmts
 * Package state machine data.
 */
typedef /*@abstract@*/ /*@refcounted@*/ struct rpmpsm_s * rpmpsm;

/**
 * Return package header from file handle, verifying digests/signatures.
 * @param ts		transaction set
 * @param _fd		file handle
 * @param fn		file name
 * @retval hdrp		address of header (or NULL)
 * @return		RPMRC_OK on success
 */
rpmRC rpmReadPackageFile(rpmts ts, void * _fd,
		const char * fn, /*@null@*/ /*@out@*/ Header * hdrp)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, *_fd, *hdrp, rpmGlobalMacroContext,
		fileSystem, internalState @*/;

/**
 * Install source package.
 * @deprecated	This routine needs to DIE! DIE! DIE!.
 * @todo	Eliminate in rpm-5.1, insturment rpmtsRun() state machine instead.
 * @param ts		transaction set
 * @param _fd		file handle
 * @retval specFilePtr	address of spec file name (or NULL)
 * @retval cookie	address of cookie pointer (or NULL)
 * @return		rpmRC return code
 */
rpmRC rpmInstallSourcePackage(rpmts ts, void * _fd,
			/*@null@*/ /*@out@*/ const char ** specFilePtr,
			/*@null@*/ /*@out@*/ const char ** cookie)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, _fd, *specFilePtr, *cookie, rpmGlobalMacroContext,
		fileSystem, internalState @*/;

/*@}*/

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMLIB */

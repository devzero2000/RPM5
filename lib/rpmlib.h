#ifndef H_RPMLIB
#define	H_RPMLIB

/** \ingroup rpmcli rpmrc rpmts rpmte rpmds rpmfi rpmdb lead signature header payload dbi
 * \file lib/rpmlib.h
 *
 * In Memoriam: Steve Taylor <staylor@redhat.com> was here, now he's not.
 *
 */

#include "rpmmessages.h"
#include "rpmerr.h"
#include <rpmtag.h>
#include "popt.h"

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
typedef /*@abstract@*/ struct _rpmdbMatchIterator * rpmdbMatchIterator;

/** \ingroup rpmgi
 * Generalized iterator.
 */
typedef /*@abstract@*/ /*@refcounted@*/ struct rpmgi_s * rpmgi;

/**
 * Table of query format extensions.
 * @note Chains headerCompoundFormats[] -> headerDefaultFormats[].
 */
/*@-redecl@*/
/*@unchecked@*/
extern const struct headerSprintfExtension_s rpmHeaderFormats[];
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

/**
 * File States (when installed).
 */
typedef enum rpmfileState_e {
    RPMFILE_STATE_NORMAL 	= 0,
    RPMFILE_STATE_REPLACED 	= 1,
    RPMFILE_STATE_NOTINSTALLED	= 2,
    RPMFILE_STATE_NETSHARED	= 3,
    RPMFILE_STATE_WRONGCOLOR	= 4
} rpmfileState;
#define	RPMFILE_STATE_MISSING	-1	/* XXX used for unavailable data */

/**
 * File Attributes.
 */
typedef	enum rpmfileAttrs_e {
/*@-enummemuse@*/
    RPMFILE_NONE	= 0,
/*@=enummemuse@*/
    RPMFILE_CONFIG	= (1 <<  0),	/*!< from %%config */
    RPMFILE_DOC		= (1 <<  1),	/*!< from %%doc */
    RPMFILE_ICON	= (1 <<  2),	/*!< from Icon: */
    RPMFILE_MISSINGOK	= (1 <<  3),	/*!< from %%config(missingok) */
    RPMFILE_NOREPLACE	= (1 <<  4),	/*!< from %%config(noreplace) */
    RPMFILE_SPECFILE	= (1 <<  5),	/*!< the specfile (srpm only). */
    RPMFILE_GHOST	= (1 <<  6),	/*!< from %%ghost */
    RPMFILE_LICENSE	= (1 <<  7),	/*!< from %%license */
    RPMFILE_README	= (1 <<  8),	/*!< from %%readme */
    RPMFILE_EXCLUDE	= (1 <<  9),	/*!< from %%exclude, internal */
    RPMFILE_UNPATCHED	= (1 << 10),	/*!< (deprecated) placeholder (SuSE) */
    RPMFILE_PUBKEY	= (1 << 11),	/*!< from %%pubkey */
    RPMFILE_POLICY	= (1 << 12),	/*!< from %%policy */
    RPMFILE_EXISTS	= (1 << 13),	/*!< did lstat(fn, st) succeed? */
    RPMFILE_SPARSE	= (1 << 14),	/*!< was ((512*st->st_blocks) < st->st_size) ? */
    RPMFILE_TYPED	= (1 << 15),	/*!< (unimplemented) from %%spook */
    RPMFILE_SOURCE	= (1 << 16),	/*!< from SourceN: (srpm only). */
    RPMFILE_PATCH	= (1 << 17),	/*!< from PatchN: (srpm only). */
    RPMFILE_OPTIONAL	= (1 << 18)	/*!< from %%optional. */
} rpmfileAttrs;

#define	RPMFILE_SPOOK	(RPMFILE_GHOST|RPMFILE_TYPED)
#define	RPMFILE_ALL	~(RPMFILE_NONE)

/* ==================================================================== */
/** \name RPMRC */
/*@{*/

/** \ingroup rpmrc
 * Build and install arch/os table identifiers.
 * @deprecated Eliminate from API.
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
 * @todo Eliminate from API.
 # @note Only used by build code.
 * @param archTable
 * @param osTable
 */
void rpmSetTables(int archTable, int osTable)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext, fileSystem, internalState @*/;

/** \ingroup rpmrc
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
 * @todo Generalize filter mechanism.
 */
typedef enum rpmprobFilterFlags_e {
    RPMPROB_FILTER_NONE		= 0,
    RPMPROB_FILTER_IGNOREOS	= (1 << 0),	/*!< from --ignoreos */
    RPMPROB_FILTER_IGNOREARCH	= (1 << 1),	/*!< from --ignorearch */
    RPMPROB_FILTER_REPLACEPKG	= (1 << 2),	/*!< from --replacepkgs */
    RPMPROB_FILTER_FORCERELOCATE= (1 << 3),	/*!< from --badreloc */
    RPMPROB_FILTER_REPLACENEWFILES= (1 << 4),	/*!< from --replacefiles */
    RPMPROB_FILTER_REPLACEOLDFILES= (1 << 5),	/*!< from --replacefiles */
    RPMPROB_FILTER_OLDPACKAGE	= (1 << 6),	/*!< from --oldpackage */
    RPMPROB_FILTER_DISKSPACE	= (1 << 7),	/*!< from --ignoresize */
    RPMPROB_FILTER_DISKNODES	= (1 << 8)	/*!< from --ignoresize */
} rpmprobFilterFlags;

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
 * @param first		1st header
 * @param second	2nd header
 * @return		result of comparison
 */
int rpmVersionCompare(Header first, Header second)
	/*@*/;

/**
 * File disposition(s) during package install/erase transaction.
 */
typedef enum fileAction_e {
    FA_UNKNOWN = 0,	/*!< initial action for file ... */
    FA_CREATE,		/*!< ... copy in from payload. */
    FA_COPYIN,		/*!< ... copy in from payload. */
    FA_COPYOUT,		/*!< ... copy out to payload. */
    FA_BACKUP,		/*!< ... renamed with ".rpmorig" extension. */
    FA_SAVE,		/*!< ... renamed with ".rpmsave" extension. */
    FA_SKIP, 		/*!< ... already replaced, don't remove. */
    FA_ALTNAME,		/*!< ... create with ".rpmnew" extension. */
    FA_ERASE,		/*!< ... to be removed. */
    FA_SKIPNSTATE,	/*!< ... untouched, state "not installed". */
    FA_SKIPNETSHARED,	/*!< ... untouched, state "netshared". */
    FA_SKIPCOLOR	/*!< ... untouched, state "wrong color". */
} fileAction;

#define XFA_SKIPPING(_a)	\
    ((_a) == FA_SKIP || (_a) == FA_SKIPNSTATE || (_a) == FA_SKIPNETSHARED || (_a) == FA_SKIPCOLOR)

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

/** \ingroup rpmts
 * Bit(s) to control rpmtsCheck() and rpmtsOrder() operation.
 * @todo Move to rpmts.h.
 */
typedef enum rpmdepFlags_e {
    RPMDEPS_FLAG_NONE		= 0,
    RPMDEPS_FLAG_NOUPGRADE	= (1 <<  0),	/*!< from --noupgrade */
    RPMDEPS_FLAG_NOREQUIRES	= (1 <<  1),	/*!< from --norequires */
    RPMDEPS_FLAG_NOCONFLICTS	= (1 <<  2),	/*!< from --noconflicts */
    RPMDEPS_FLAG_NOOBSOLETES	= (1 <<  3),	/*!< from --noobsoletes */
    RPMDEPS_FLAG_NOPARENTDIRS	= (1 <<  4),	/*!< from --noparentdirs */
    RPMDEPS_FLAG_NOLINKTOS	= (1 <<  5),	/*!< from --nolinktos */
    RPMDEPS_FLAG_ANACONDA	= (1 <<  6),	/*!< from --anaconda */
    RPMDEPS_FLAG_NOSUGGEST	= (1 <<  7),	/*!< from --nosuggest */
    RPMDEPS_FLAG_ADDINDEPS	= (1 <<  8),	/*!< from --aid */
    RPMDEPS_FLAG_DEPLOOPS	= (1 <<  9)	/*!< from --deploops */
} rpmdepFlags;

/** \ingroup rpmts
 * Bit(s) to control rpmtsRun() operation.
 * @todo Move to rpmts.h.
 */
typedef enum rpmtransFlags_e {
    RPMTRANS_FLAG_NONE		= 0,
    RPMTRANS_FLAG_TEST		= (1 <<  0),	/*!< from --test */
    RPMTRANS_FLAG_BUILD_PROBS	= (1 <<  1),	/*!< don't process payload */
    RPMTRANS_FLAG_NOSCRIPTS	= (1 <<  2),	/*!< from --noscripts */
    RPMTRANS_FLAG_JUSTDB	= (1 <<  3),	/*!< from --justdb */
    RPMTRANS_FLAG_NOTRIGGERS	= (1 <<  4),	/*!< from --notriggers */
    RPMTRANS_FLAG_NODOCS	= (1 <<  5),	/*!< from --excludedocs */
    RPMTRANS_FLAG_ALLFILES	= (1 <<  6),	/*!< from --allfiles */
/*@-enummemuse@*/
    RPMTRANS_FLAG_KEEPOBSOLETE	= (1 <<  7),	/*!< @todo Document. */
/*@=enummemuse@*/
    RPMTRANS_FLAG_NOCONTEXTS	= (1 <<  8),	/*!< from --nocontexts */
    RPMTRANS_FLAG_DIRSTASH	= (1 <<  9),	/*!< from --dirstash */
    RPMTRANS_FLAG_REPACKAGE	= (1 << 10),	/*!< from --repackage */

    RPMTRANS_FLAG_PKGCOMMIT	= (1 << 11),
/*@-enummemuse@*/
    RPMTRANS_FLAG_PKGUNDO	= (1 << 12),
/*@=enummemuse@*/
    RPMTRANS_FLAG_COMMIT	= (1 << 13),
/*@-enummemuse@*/
    RPMTRANS_FLAG_UNDO		= (1 << 14),
/*@=enummemuse@*/
    /* 15 unused */

    RPMTRANS_FLAG_NOTRIGGERPREIN= (1 << 16),	/*!< from --notriggerprein */
    RPMTRANS_FLAG_NOPRE		= (1 << 17),	/*!< from --nopre */
    RPMTRANS_FLAG_NOPOST	= (1 << 18),	/*!< from --nopost */
    RPMTRANS_FLAG_NOTRIGGERIN	= (1 << 19),	/*!< from --notriggerin */
    RPMTRANS_FLAG_NOTRIGGERUN	= (1 << 20),	/*!< from --notriggerun */
    RPMTRANS_FLAG_NOPREUN	= (1 << 21),	/*!< from --nopreun */
    RPMTRANS_FLAG_NOPOSTUN	= (1 << 22),	/*!< from --nopostun */
    RPMTRANS_FLAG_NOTRIGGERPOSTUN = (1 << 23),	/*!< from --notriggerpostun */
/*@-enummemuse@*/
    RPMTRANS_FLAG_NOPAYLOAD	= (1 << 24),
/*@=enummemuse@*/
    RPMTRANS_FLAG_APPLYONLY	= (1 << 25),

    /* 26 unused */
    RPMTRANS_FLAG_NOFDIGESTS	= (1 << 27),	/*!< from --nofdigests */
    /* 28-29 unused */
    RPMTRANS_FLAG_NOCONFIGS	= (1 << 30),	/*!< from --noconfigs */
    /* 31 unused */
} rpmtransFlags;

#define	_noTransScripts		\
  ( RPMTRANS_FLAG_NOPRE |	\
    RPMTRANS_FLAG_NOPOST |	\
    RPMTRANS_FLAG_NOPREUN |	\
    RPMTRANS_FLAG_NOPOSTUN	\
  )

#define	_noTransTriggers	\
  ( RPMTRANS_FLAG_NOTRIGGERPREIN | \
    RPMTRANS_FLAG_NOTRIGGERIN |	\
    RPMTRANS_FLAG_NOTRIGGERUN |	\
    RPMTRANS_FLAG_NOTRIGGERPOSTUN \
  )

/*@}*/

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMLIB */

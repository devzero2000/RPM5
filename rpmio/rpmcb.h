#ifndef H_RPMCB
#define H_RPMCB

/** \ingroup rpmio
 * \file rpmio/rpmcb.h
 */

#include <rpmlog.h>

/**
 * @todo These convenience macros belong somewhere else.
 */
#define	rpmSetVerbosity(_lvl)	\
	((void)rpmlogSetMask( RPMLOG_UPTO( RPMLOG_PRI(_lvl))))
#define	rpmIncreaseVerbosity()	\
    ((void)rpmlogSetMask(((((unsigned)(rpmlogSetMask(0) & 0xff)) << 1) | 1)))
#define	rpmDecreaseVerbosity()	\
	((void)rpmlogSetMask((((int)(rpmlogSetMask(0) & 0xff)) >> 1)))
#define	rpmIsNormal()		\
	(rpmlogSetMask(0) >= RPMLOG_MASK( RPMLOG_NOTICE ))
#define	rpmIsVerbose()		\
	(rpmlogSetMask(0) >= RPMLOG_MASK( RPMLOG_INFO ))
#define	rpmIsDebug()		\
	(rpmlogSetMask(0) >= RPMLOG_MASK( RPMLOG_DEBUG ))

/*@-redef@*/ /* LCL: ??? */
typedef /*@abstract@*/ const void * fnpyKey;
/*@=redef@*/

/**
 * Bit(s) to identify progress callbacks.
 */
typedef enum rpmCallbackType_e {
    RPMCALLBACK_UNKNOWN		= 0,
    RPMCALLBACK_INST_PROGRESS	= (1 <<  0),
    RPMCALLBACK_INST_START	= (1 <<  1),
    RPMCALLBACK_INST_OPEN_FILE	= (1 <<  2),
    RPMCALLBACK_INST_CLOSE_FILE	= (1 <<  3),
    RPMCALLBACK_TRANS_PROGRESS	= (1 <<  4),
    RPMCALLBACK_TRANS_START	= (1 <<  5),
    RPMCALLBACK_TRANS_STOP	= (1 <<  6),
    RPMCALLBACK_UNINST_PROGRESS	= (1 <<  7),
    RPMCALLBACK_UNINST_START	= (1 <<  8),
    RPMCALLBACK_UNINST_STOP	= (1 <<  9),
    RPMCALLBACK_REPACKAGE_PROGRESS = (1 << 10),
    RPMCALLBACK_REPACKAGE_START	= (1 << 11),
    RPMCALLBACK_REPACKAGE_STOP	= (1 << 12),
    RPMCALLBACK_UNPACK_ERROR	= (1 << 13),
    RPMCALLBACK_CPIO_ERROR	= (1 << 14)
} rpmCallbackType;

/**
 */
typedef void * rpmCallbackData;

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
typedef void * (*rpmCallbackFunction)
		(/*@null@*/ const void * h, 
		const rpmCallbackType what, 
		const uint64_t amount, 
		const uint64_t total,
		/*@null@*/ fnpyKey key,
		/*@null@*/ rpmCallbackData data)
	/*@globals internalState@*/
	/*@modifies internalState@*/;

/**
 */
/*@unused@*/
void urlSetCallback(rpmCallbackFunction notify, rpmCallbackData notifyData,
		int notifyCount);

#ifdef __cplusplus
}
#endif

#endif  /* H_RPMCB */

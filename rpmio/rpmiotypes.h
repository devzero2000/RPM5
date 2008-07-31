#ifndef _H_RPMIOTYPES_
#define	_H_RPMIOTYPES_

/** \ingroup rpmio
 * \file rpmio/rpmiotypes.h
 */

/**
 * Private int typedefs to avoid C99 portability issues.
 */
typedef	unsigned char		rpmuint8_t;
typedef unsigned short		rpmuint16_t;
typedef unsigned int		rpmuint32_t;
typedef unsigned long long	rpmuint64_t;

typedef int			rpmint32_t;

/*@-redef@*/ /* LCL: ??? */
typedef /*@abstract@*/ const void * fnpyKey;
/*@=redef@*/

/**
 * Bit(s) to identify progress callbacks.
 */
typedef enum rpmCallbackType_e {
    RPMCALLBACK_UNKNOWN         = 0,
    RPMCALLBACK_INST_PROGRESS   = (1 <<  0),
    RPMCALLBACK_INST_START      = (1 <<  1),
    RPMCALLBACK_INST_OPEN_FILE  = (1 <<  2),
    RPMCALLBACK_INST_CLOSE_FILE = (1 <<  3),
    RPMCALLBACK_TRANS_PROGRESS  = (1 <<  4),
    RPMCALLBACK_TRANS_START     = (1 <<  5),
    RPMCALLBACK_TRANS_STOP      = (1 <<  6),
    RPMCALLBACK_UNINST_PROGRESS = (1 <<  7),
    RPMCALLBACK_UNINST_START    = (1 <<  8),
    RPMCALLBACK_UNINST_STOP     = (1 <<  9),
    RPMCALLBACK_REPACKAGE_PROGRESS = (1 << 10),
    RPMCALLBACK_REPACKAGE_START = (1 << 11),
    RPMCALLBACK_REPACKAGE_STOP  = (1 << 12),
    RPMCALLBACK_UNPACK_ERROR    = (1 << 13),
    RPMCALLBACK_CPIO_ERROR      = (1 << 14),
    RPMCALLBACK_SCRIPT_ERROR    = (1 << 15)
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
                const rpmuint64_t amount,
                const rpmuint64_t total,
                /*@null@*/ fnpyKey key,
                /*@null@*/ rpmCallbackData data)
        /*@globals internalState@*/
        /*@modifies internalState@*/;

#ifdef __cplusplus
}
#endif

#endif /* _H_RPMIOTYPES_ */

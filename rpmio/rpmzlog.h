#ifndef _H_RPMZLOG_
#define _H_RPMZLOG_

/** \ingroup rpmio
 * \file rpmio/rpmzlog.h
 */

/** trace log pointer */
typedef /*@abstract@*/ struct rpmzLog_s * rpmzLog;

#ifdef	_RPMZLOG_INTERNAL
/** trace msg pointer */
typedef /*@abstract@*/ struct rpmzMsg_s * rpmzMsg;

/** trace msg */
struct rpmzMsg_s {
    struct timeval when;	/* time of entry */
    char *msg;			/* message */
    rpmzMsg next;		/* next entry */
};

/** trace log */
struct rpmzLog_s {
    struct timeval start;	/*!< starting time of day for tracing */
/*@null@*/
    rpmzMsg msg_head;
/*@shared@*/ /*@relnull@*/
    rpmzMsg *msg_tail;
/*@only@*/ /*@null@*/
    yarnLock msg_lock;
};
#endif	/* _RPMZLOG_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Set up log (call from main thread before other threads launched).
 */
/*@only@*/
rpmzLog rpmzLogInit(/*@null@*/ struct timeval *tv)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

/**
 * Add entry to trace log.
 */
void rpmzLogAdd(/*@null@*/ rpmzLog zlog, char *fmt, ...)
	/*@globals fileSystem, internalState @*/
	/*@modifies zlog, fileSystem, internalState @*/;

/**
 * Pull entry from trace log and print it, return false if empty.
 */
int rpmzMsgShow(/*@null@*/ rpmzLog zlog, /*@null@*/ FILE * fp)
	/*@globals fileSystem, internalState @*/
	/*@modifies zlog, *fp, fileSystem, internalState @*/;

/**
 * Release log resources (need to do rpmzLogInit() to use again).
 */
/*@null@*/
rpmzLog rpmzLogFree(/*@only@*/ /*@null@*/ rpmzLog zlog)
	/*@globals fileSystem, internalState @*/
	/*@modifies zlog, fileSystem, internalState @*/;

/**
 * Show entries until no more, free log.
 */
/*@null@*/
rpmzLog rpmzLogDump(rpmzLog zlog, /*@null@*/ FILE * fp)
	/*@globals fileSystem, internalState @*/
	/*@modifies zlog, *fp, fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif /* _H_RPMZLOG_ */

#ifndef _H_RPMZQ_
#define _H_RPMZQ_

/** \ingroup rpmio
 * \file rpmio/rpmzlog.h
 * Job queue and buffer pool management.
 */

/**
 */
/*@unchecked@*/
extern int _rpmzq_debug;

/**
 */
typedef /*@abstract@*/ struct rpmzSpace_s * rpmzSpace;
/**
 */
typedef /*@abstract@*/ struct rpmzPool_s * rpmzPool;
/**
 */
typedef /*@abstract@*/ struct rpmzQueue_s * rpmzQueue;
/**
 */
typedef /*@abstract@*/ struct rpmzJob_s * rpmzJob;

#ifdef	_RPMZQ_INTERNAL
/** a space (one buffer for each space) */
struct rpmzSpace_s {
    yarnLock use;		/*!< use count -- return to pool when zero */
    void * buf;			/*!< buffer of size pool->size */
    size_t len;			/*!< for application usage */
    rpmzPool pool;		/*!< pool to return to */
    rpmzSpace next;		/*!< for pool linked list */
};

/** pool of spaces (one pool for each size needed) */
struct rpmzPool_s {
    yarnLock have;		/*!< unused spaces available, lock for list */
/*@relnull@*/
    rpmzSpace head;		/*!< linked list of available buffers */
    size_t size;		/*!< size of all buffers in this pool */
    int limit;			/*!< number of new spaces allowed, or -1 */
    int made;			/*!< number of buffers made */
};

/**
 */
struct rpmzJob_s {
    long seq;			/*!< sequence number */
    int more;			/*!< true if this is not the last chunk */
/*@relnull@*/
    rpmzSpace in;		/*!< input data to compress */
/*@relnull@*/
    rpmzSpace out;		/*!< dictionary or resulting compressed data */
    unsigned long check;	/*!< check value for input data */
    yarnLock calc;		/*!< released when check calculation complete */
/*@dependent@*/ /*@null@*/
    rpmzJob next;		/*!< next job in the list (either list) */
};

struct rpmzQueue_s {
    int flags;			/*!< Control bits. */
/*@null@*/
    const char *ifn;		/*!< input file name */
    int ifdno;			/*!< input file descriptor */
/*@null@*/
    const char *ofn;		/*!< output file name */
    int ofdno;			/*!< output file descriptor */

    long lastseq;		/*!< Last seq. */
    unsigned int level;		/*!< Compression level. */
    mode_t omode;		/*!< O_RDONLY=decompress, O_WRONLY=compress */
    size_t iblocksize;
    int ilimit;
    size_t oblocksize;
    int olimit;

/*@relnull@*/
    rpmzPool in_pool;		/*!< input buffer pool (malloc'd). */
/*@relnull@*/
    rpmzPool out_pool;		/*!< output buffer pool (malloc'd). */

    int verbosity;        /*!< 0 = quiet, 1 = normal, 2 = verbose, 3 = trace */
/*@owned@*/ /*@null@*/
    rpmzLog zlog;		/*!< trace logging */

    /* list of compress jobs (with tail for appending to list) */
/*@only@*/ /*@null@*/
    yarnLock compress_have;	/*!< no. compress/decompress jobs waiting */
/*@null@*/
    rpmzJob compress_head;
/*@shared@*/
    rpmzJob * compress_tail;

    int cthreads;		/*!< number of compression threads running */

/*@only@*/ /*@null@*/
    yarnLock write_first;	/*!< lowest sequence number in list */
/*@null@*/
    rpmzJob write_head;		/*!< list of write jobs */

/*@only@*/ /*@null@*/
    yarnThread writeth;		/*!< write thread if running */
};
#endif	/* _RPMZQ_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
int rpmbzCompressBlock(void * _bz, rpmzJob job)
	/*@globals fileSystem @*/
	/*@modifies job, fileSystem @*/;

/* -- pool of spaces for buffer management -- */

/* These routines manage a pool of spaces.  Each pool specifies a fixed size
   buffer to be contained in each space.  Each space has a use count, which
   when decremented to zero returns the space to the pool.  If a space is
   requested from the pool and the pool is empty, a space is immediately
   created unless a specified limit on the number of spaces has been reached.
   Only if the limit is reached will it wait for a space to be returned to the
   pool.  Each space knows what pool it belongs to, so that it can be returned.
 */

/* initialize a pool (pool structure itself provided, not allocated) -- the
   limit is the maximum number of spaces in the pool, or -1 to indicate no
   limit, i.e., to never wait for a buffer to return to the pool */
rpmzPool rpmzNewPool(size_t size, int limit)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

/* get a space from a pool -- the use count is initially set to one, so there
   is no need to call rpmzUseSpace() for the first use */
rpmzSpace rpmzNewSpace(rpmzPool pool)
	/*@globals fileSystem, internalState @*/
	/*@modifies pool, fileSystem, internalState @*/;

/* increment the use count to require one more drop before returning this space
   to the pool */
void rpmzUseSpace(rpmzSpace space)
	/*@globals fileSystem, internalState @*/
	/*@modifies space, fileSystem, internalState @*/;

/* drop a space, returning it to the pool if the use count is zero */
/*@null@*/
rpmzSpace rpmzDropSpace(/*@only@*/ rpmzSpace space)
	/*@globals fileSystem, internalState @*/
	/*@modifies space, fileSystem, internalState @*/;

/* free the memory and lock resources of a pool -- return number of spaces for
   debugging and resource usage measurement */
/*@null@*/
rpmzPool rpmzFreePool(/*@only@*/ rpmzPool pool, /*@null@*/ int *countp)
	/*@globals fileSystem, internalState @*/
	/*@modifies pool, *countp, fileSystem, internalState @*/;

/**
 */
/*@null@*/
rpmzJob rpmzFreeJob(/*@only@*/ rpmzJob job)
        /*@globals fileSystem, internalState @*/
        /*@modifies job, fileSystem, internalState @*/;

/**
 */
/*@only@*/
rpmzJob rpmzNewJob(long seq)
        /*@globals fileSystem, internalState @*/
        /*@modifies fileSystem, internalState @*/;

/* compress or write job (passed from compress list to write list) -- if seq is
   equal to -1, rpmzqCompressThread() is instructed to return; if more is false then
   this is the last chunk, which after writing tells write_thread to return */

/** command the compress threads to all return, then join them all (call from
   main thread), free all the thread-related resources */
void rpmzqFini(rpmzQueue zq)
        /*@globals fileSystem, internalState @*/
        /*@modifies zq, fileSystem, internalState @*/;

/** setup job lists (call from main thread) */
void rpmzqInit(rpmzQueue zq)
        /*@globals fileSystem, internalState @*/
        /*@modifies zq, fileSystem, internalState @*/;

/**
 */
/*@null@*/
rpmzQueue rpmzqFree(/*@only@*/ rpmzQueue zq)
        /*@modifies zq @*/;

/**
 */
/*@only@*/
rpmzQueue rpmzqNew(rpmzLog zlog, int flags,
		int verbosity, unsigned int level, size_t blocksize, int limit)
	/*@*/;

/**
 */
/*@null@*/
rpmzJob rpmzqDelCJob(rpmzQueue zq)
	/*@globals fileSystem, internalState @*/
	/*@modifies zq, fileSystem, internalState @*/;

/**
 */
void rpmzqAddCJob(rpmzQueue zq, rpmzJob job)
	/*@globals fileSystem, internalState @*/
	/*@modifies zq, job, fileSystem, internalState @*/;

/**
 */
rpmzJob rpmzqDelWJob(rpmzQueue zq, long seq)
	/*@globals fileSystem, internalState @*/
	/*@modifies zq, fileSystem, internalState @*/;

/**
 */
void rpmzqAddWJob(rpmzQueue zq, rpmzJob job)
	/*@globals fileSystem, internalState @*/
	/*@modifies zq, job, fileSystem, internalState @*/;

/** start another compress/decompress thread if needed */
void rpmzqLaunch(rpmzQueue zq, long seq, unsigned int threads)
	/*@globals fileSystem, internalState @*/
	/*@modifies zq, fileSystem, internalState @*/;

/** verify no more jobs, prepare for next use */
void rpmzqVerify(rpmzQueue zq)
	/*@globals fileSystem, internalState @*/
	/*@modifies zq, fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif /* _H_RPMZQ_ */

#ifndef _H_RPMZQ_
#define _H_RPMZQ_

/** \ingroup rpmio
 * \file rpmio/rpmzlog.h
 * Job queue and buffer pool management.
 */

#include <popt.h>

#include <yarn.h>

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

/**
 */
/*@unchecked@*/
extern int _rpmzq_debug;

/**
 */
/*@unchecked@*/
extern rpmzQueue _rpmzq;

/**
 */
/*@unchecked@*/
extern struct poptOption rpmzqOptionsPoptTable[];

/**
 */
enum rpmzMode_e {
    RPMZ_MODE_COMPRESS		= 0,
    RPMZ_MODE_DECOMPRESS	= 1,
    RPMZ_MODE_TEST		= 2,
};

/**
 */
enum rpmzFormat_e {
    RPMZ_FORMAT_AUTO		= 0,
    RPMZ_FORMAT_XZ		= 1,
    RPMZ_FORMAT_LZMA		= 2,
    RPMZ_FORMAT_RAW		= 3,
    RPMZ_FORMAT_GZIP		= 4,
    RPMZ_FORMAT_ZLIB		= 5,
    RPMZ_FORMAT_ZIP2		= 6,
    RPMZ_FORMAT_ZIP3		= 7,
    RPMZ_FORMAT_BZIP2		= 8,
};

#define _ZKFB(n) (1U << (n))
#define _ZFB(n) (_ZKFB(n) | 0x40000000)

/**
 */
enum rpmzFlags_e {
    RPMZ_FLAGS_NONE		= 0,
    RPMZ_FLAGS_STDOUT		= _ZFB( 0),	/*!< -c, --stdout ... */
	/* 1 unused */
    RPMZ_FLAGS_KEEP		= _ZFB( 2),	/*!< -k, --keep ... */
    RPMZ_FLAGS_RECURSE		= _ZFB( 3),	/*!< -r, --recursive ... */
    RPMZ_FLAGS_EXTREME		= _ZFB( 4),	/*!< -e, --extreme ... */
  /* XXX compressor specific flags need to be set some other way. */
  /* XXX unused */
    RPMZ_FLAGS_FAST		= _ZFB( 5),	/*!<     --fast ... */
    RPMZ_FLAGS_BEST		= _ZFB( 6),	/*!<     --best ... */

  /* XXX logic is reversed, disablers should clear with toggle. */
    RPMZ_FLAGS_HNAME		= _ZFB( 7),	/*!< -n, --no-name ... */
    RPMZ_FLAGS_HTIME		= _ZFB( 8),	/*!< -T, --no-time ... */

  /* XXX unimplemented */
    RPMZ_FLAGS_RSYNCABLE	= _ZFB( 9),	/*!< -R, --rsyncable ... */
  /* XXX logic is reversed. */
    RPMZ_FLAGS_INDEPENDENT	= _ZFB(10),	/*!< -i, --independent ... */
    RPMZ_FLAGS_LIST		= _ZFB(11),	/*!< -l, --list ... */

    RPMZ_FLAGS_OVERWRITE	= _ZFB(12),	/*!<     --overwrite ... */
    RPMZ_FLAGS_ALREADY		= _ZFB(13),	/*!<     --recompress ... */
    RPMZ_FLAGS_SYMLINKS		= _ZFB(14),	/*!<     --symlinks ... */
    RPMZ_FLAGS_TTY		= _ZFB(15),	/*!<     --tty ... */
#define	RPMZ_FLAGS_FORCE	\
    (RPMZ_FLAGS_OVERWRITE|RPMZ_FLAGS_ALREADY|RPMZ_FLAGS_SYMLINKS|RPMZ_FLAGS_TTY)

#ifdef	NOTYET
    RPMZ_FLAGS_SUBBLOCK		= INT_MIN,
    RPMZ_FLAGS_DELTA,
    RPMZ_FLAGS_LZMA1,
    RPMZ_FLAGS_LZMA2,
#endif

    RPMZ_FLAGS_X86		= _ZFB(16),
    RPMZ_FLAGS_POWERPC		= _ZFB(17),
    RPMZ_FLAGS_IA64		= _ZFB(18),
    RPMZ_FLAGS_ARM		= _ZFB(19),
    RPMZ_FLAGS_ARMTHUMB		= _ZFB(20),
    RPMZ_FLAGS_SPARC		= _ZFB(21),

};

#ifdef	_RPMZQ_INTERNAL
/** a space (one buffer for each space) */
struct rpmzSpace_s {
    yarnLock use;		/*!< use count -- return to pool when zero */
    void *ptr;			/*!< original buffer allocation address */
    size_t ix;			/*!< current buffer index (application) */
    unsigned char *buf;		/*!< current buffer address (application) */
    size_t len;			/*!< current buffer length (application) */
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
    yarnLock use;		/*!< use count -- return to pool when zero */
    long seq;			/*!< sequence number */
    int more;			/*!< true if this is not the last chunk */
/*@relnull@*/
    rpmzSpace in;		/*!< input data to compress */
/*@relnull@*/
    rpmzSpace out;		/*!< dictionary or resulting compressed data */
    unsigned long check;	/*!< check value for input data */
    yarnLock calc;		/*!< released when check calculation complete */
/*@dependent@*/ /*@null@*/
    rpmzJob next;		/*!< for job linked list */
};

/**
 */
typedef	struct rpmzh_s * rpmzh;

/**
 */
struct rpmzh_s {
/*@observer@*/ /*@null@*/
    const char * name;		/*!< name for gzip header */
    time_t mtime;		/*!< time stamp for gzip header */
    unsigned long head;		/*!< header length */
    unsigned long ulen;		/*!< total uncompressed size (overflow ok) */
    unsigned long clen;		/*!< total compressed size (overflow ok) */
    unsigned long check;	/*!< check value of uncompressed data */
/* saved gzip/zip header data for decompression, testing, and listing */
/*@only@*/ /*@null@*/
    char * hname;		/*!< name from header (allocated) */
    time_t stamp;		/*!< time stamp from gzip header */
	/* XXX zip_head?!? */
    unsigned long zip_ulen;	/*!< header uncompressed length */
    unsigned long zip_clen;	/*!< header compressed length */
    unsigned long zip_crc;	/*!< header crc */
};

/**
 */
struct rpmzQueue_s {
/* --- globals (modified by main thread only when it's the only thread) */
    enum rpmzFlags_e flags;	/*!< Control bits. */
    enum rpmzFormat_e format;	/*!< Compression format. */
    enum rpmzMode_e mode;	/*!< Operation mode. */
    unsigned int level;		/*!< Compression level. */
    unsigned int threads;	/*!< No. or threads to use. */
#ifdef	NOTYET	/* XXX popt has sizeof(size_t) != sizeof(unsigned int) issues */
    size_t blocksize;		/*!< uncompressed input size per thread */
#else
    unsigned int blocksize;	/*!< uncompressed input size per thread */
#endif

/*@null@*/ /*@observer@*/
    const char * suffix;	/*!< -S, --suffix ... */
/*@null@*/
    const char * ifn;
/*@null@*/
    const char * ofn;		/*!< output file name (allocated if not NULL) */
    int verbosity;		/*!< 0:quiet, 1:normal, 2:verbose, 3:trace */

    struct timeval start;	/*!< starting time of day for tracing */
/*@owned@*/ /*@null@*/
    rpmzLog zlog;		/*!< trace logging */

    int ifdno;			/*!< input file descriptor */
    int ofdno;			/*!< output file descriptor */

    off_t in_tot;		/*!< total bytes read from input */
    off_t out_tot;		/*!< total bytes written to output */

/*@relnull@*/
    rpmzPool in_pool;		/*!< input buffer pool (malloc'd). */
/*@relnull@*/
    rpmzPool out_pool;		/*!< output buffer pool (malloc'd). */

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

    /* parallel reading */
/*@only@*/ /*@null@*/
    yarnLock load_state;	/*!< value = 0 to wait, 1 to read a buffer */
/*@only@*/ /*@null@*/
    yarnThread load_thread;	/*!< load_read_thread() thread for joining */

/*@only@*/ /*@null@*/
    yarnLock outb_write_more;	/*!< outb write threads states */
/*@only@*/ /*@null@*/
    yarnLock outb_check_more;	/*!< outb check threads states */

#ifndef	DYING	/* XXX this cruft is going away */
    long lastseq;		/*!< Last seq. */
    mode_t omode;		/*!< O_RDONLY=decompress, O_WRONLY=compress */
    size_t iblocksize;
    int ilimit;
    size_t oblocksize;
    int olimit;
#endif
/*@only@*/ /*@relnull@*/
    rpmzh _zh;			/*!< compressed file header info (malloc'd). */
/*@owned@*/ /*@relnull@*/
    rpmzJob _job;		/*!< decompress job (malloc'd). */

/* --- globals for decompression and listing buffered reading */
    int _in_which;		/*!< -1: start */

/*@only@*/ /*@null@*/
    yarnLock read_first;	/*!< lowest sequence number in list */
/*@null@*/
    rpmzJob read_head;		/*!< list of read jobs */

#define IN_BUF_ALLOCATED 32768U	/* input buffer size */
    size_t _in_buf_allocated;
/*@relnull@*/
    rpmzPool load_ipool;	/*!< load/read input buffer pool (malloc'd). */
    rpmzPool load_opool;	/*!< load/read ouput buffer pool (malloc'd). */

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
rpmzPool rpmzqNewPool(size_t size, int limit)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

/**
 * Get a space from a pool (or malloc if pool == NULL).
 * The use count is initially set to one, so there is no need to call
 * rpmzUseSpace() for the first use.
 */
rpmzSpace rpmzqNewSpace(/*@null@*/ rpmzPool pool, size_t len)
	/*@globals fileSystem, internalState @*/
	/*@modifies pool, fileSystem, internalState @*/;

/**
 * Increment the use count to require one more drop before returning
 * this space to the pool.
 */
void rpmzqUseSpace(rpmzSpace space)
	/*@globals fileSystem, internalState @*/
	/*@modifies space, fileSystem, internalState @*/;

/**
 * Drop a space, returning to the pool (or free'ing if no pool) when the
 * use count is zero. If space == NULL, nothing is done.
 */
/*@null@*/
rpmzSpace rpmzqDropSpace(/*@only@*/ /*@null@*/ rpmzSpace space)
	/*@globals fileSystem, internalState @*/
	/*@modifies space, fileSystem, internalState @*/;

/* free the memory and lock resources of a pool -- return number of spaces for
   debugging and resource usage measurement */
/*@null@*/
rpmzPool rpmzqFreePool(/*@only@*/ rpmzPool pool, /*@null@*/ int *countp)
	/*@globals fileSystem, internalState @*/
	/*@modifies pool, *countp, fileSystem, internalState @*/;

/**
 */
/*@null@*/
rpmzJob rpmzqFreeJob(/*@only@*/ rpmzJob job)
        /*@globals fileSystem, internalState @*/
        /*@modifies job, fileSystem, internalState @*/;

/**
 */
/*@only@*/
rpmzJob rpmzqNewJob(long seq)
        /*@globals fileSystem, internalState @*/
        /*@modifies fileSystem, internalState @*/;

/**
 */
void rpmzqUseJob(rpmzJob job)
	/*@globals fileSystem, internalState @*/
	/*@modifies job, fileSystem, internalState @*/;

/**
 */
/*@null@*/
rpmzJob rpmzqDropJob(/*@only@*/ /*@null@*/ rpmzJob job)
	/*@globals fileSystem, internalState @*/
	/*@modifies job, fileSystem, internalState @*/;

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
        /*@*/;

/**
 */
rpmzQueue rpmzqNew(/*@returned@*/rpmzQueue zq, rpmzLog zlog, int limit)
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

/**
 */
rpmzJob rpmzqDelRJob(rpmzQueue zq, long seq)
	/*@globals fileSystem, internalState @*/
	/*@modifies zq, fileSystem, internalState @*/;

/**
 */
void rpmzqAddRJob(rpmzQueue zq, rpmzJob job)
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

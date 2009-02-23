/* pigz.c -- parallel implementation of gzip
 * Copyright (C) 2007, 2008 Mark Adler
 * Version 2.1.4  9 Nov 2008  Mark Adler
 */

/*
  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the author be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Mark Adler
  madler@alumni.caltech.edu

  Mark accepts donations for providing this software.  Donations are not
  required or expected.  Any amount that you feel is appropriate would be
  appreciated.  You can use this link:

  https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=536055

 */

/* Version history:
   1.0    17 Jan 2007  First version, pipe only
   1.1    28 Jan 2007  Avoid void * arithmetic (some compilers don't get that)
                       Add note about requiring zlib 1.2.3
                       Allow compression level 0 (no compression)
                       Completely rewrite parallelism -- add a write thread
                       Use deflateSetDictionary() to make use of history
                       Tune argument defaults to best performance on four cores
   1.2.1   1 Feb 2007  Add long command line options, add all gzip options
                       Add debugging options
   1.2.2  19 Feb 2007  Add list (--list) function
                       Process file names on command line, write .gz output
                       Write name and time in gzip header, set output file time
                       Implement all command line options except --recursive
                       Add --keep option to prevent deleting input files
                       Add thread tracing information with -vv used
                       Copy crc32_combine() from zlib (possible thread issue)
   1.3    25 Feb 2007  Implement --recursive
                       Expand help to show all options
                       Show help if no arguments or output piping are provided
                       Process options in GZIP environment variable
                       Add progress indicator to write thread if --verbose
   1.4     4 Mar 2007  Add --independent to facilitate damaged file recovery
                       Reallocate jobs for new --blocksize or --processes
                       Do not delete original if writing to stdout
                       Allow --processes 1, which does no threading
                       Add NOTHREAD define to compile without threads
                       Incorporate license text from zlib in source code
   1.5    25 Mar 2007  Reinitialize jobs for new compression level
                       Copy attributes and owner from input file to output file
                       Add decompression and testing
                       Add -lt (or -ltv) to show all entries and proper lengths
                       Add decompression, testing, listing of LZW (.Z) files
                       Only generate and show trace log if DEBUG defined
                       Take "-" argument to mean read file from stdin
   1.6    30 Mar 2007  Add zlib stream compression (--zlib), and decompression
   1.7    29 Apr 2007  Decompress first entry of a zip file (if deflated)
                       Avoid empty deflate blocks at end of deflate stream
                       Show zlib check value (Adler-32) when listing
                       Don't complain when decompressing empty file
                       Warn about trailing junk for gzip and zlib streams
                       Make listings consistent, ignore gzip extra flags
                       Add zip stream compression (--zip)
   1.8    13 May 2007  Document --zip option in help output
   2.0    19 Oct 2008  Complete rewrite of thread usage and synchronization
                       Use polling threads and a pool of memory buffers
                       Remove direct pthread library use, hide in yarn.c
   2.0.1  20 Oct 2008  Check version of zlib at compile time, need >= 1.2.3
   2.1    24 Oct 2008  Decompress with read, write, inflate, and check threads
                       Remove spurious use of ctime_r(), ctime() more portable
                       Change application of job->calc lock to be a semaphore
                       Detect size of off_t at run time to select %lu vs. %llu
                       #define large file support macro even if not __linux__
                       Remove _LARGEFILE64_SOURCE, _FILE_OFFSET_BITS is enough
                       Detect file-too-large error and report, blame build
                       Replace check combination routines with those from zlib
   2.1.1  28 Oct 2008  Fix a leak for files with an integer number of blocks
                       Update for yarn 1.1 (yarn_prefix and yarn_abort)
   2.1.2  30 Oct 2008  Work around use of beta zlib in production systems
   2.1.3   8 Nov 2008  Don't use zlib combination routines, put back in pigz
   2.1.4   9 Nov 2008  Fix bug when decompressing very short files
 */

#define PIGZVERSION "pigz 2.1.4\n"

/* To-do:
    - add --rsyncable (or -R) [use my own algorithm, set min/max block size]
    - make source portable for Windows, VMS, etc. (see gzip source code)
    - make build portable (currently good for Unixish)
 */

/*
   pigz compresses using threads to make use of multiple processors and cores.
   The input is broken up into 128 KB chunks with each compressed in parallel.
   The individual check value for each chunk is also calculated in parallel.
   The compressed data is written in order to the output, and a combined check
   value is calculated from the individual check values.

   The compressed data format generated is in the gzip, zlib, or single-entry
   zip format using the deflate compression method.  The compression produces
   partial raw deflate streams which are concatenated by a single write thread
   and wrapped with the appropriate header and trailer, where the trailer
   contains the combined check value.

   Each partial raw deflate stream is terminated by an empty stored block
   (using the Z_SYNC_FLUSH option of zlib), in order to end that partial bit
   stream at a byte boundary.  That allows the partial streams to be
   concatenated simply as sequences of bytes.  This adds a very small four to
   five byte overhead to the output for each input chunk.

   The default input block size is 128K, but can be changed with the -b option.
   The number of compress threads is set by default to 8, which can be changed
   using the -p option.  Specifying -p 1 avoids the use of threads entirely.

   The input blocks, while compressed independently, have the last 32K of the
   previous block loaded as a preset dictionary to preserve the compression
   effectiveness of deflating in a single thread.  This can be turned off using
   the --independent or -i option, so that the blocks can be decompressed
   independently for partial error recovery or for random access.

   Decompression can't be parallelized, at least not without specially prepared
   deflate streams for that purpose.  As a result, pigz uses a single thread
   (the main thread) for decompression, but will create three other threads for
   reading, writing, and check calculation, which can speed up decompression
   under some circumstances.  Parallel decompression can be turned off by
   specifying one process (-dp 1 or -tp 1).

   pigz requires zlib 1.2.1 or later to allow setting the dictionary when doing
   raw deflate.  Since zlib 1.2.3 corrects security vulnerabilities in zlib
   version 1.2.1 and 1.2.2, conditionals check for zlib 1.2.3 or later during
   the compilation of pigz.c.

   pigz uses the POSIX pthread library for thread control and communication,
   through the yarn.h interface to yarn.c.  yarn.c can be replaced with
   equivalent implementations using other thread libraries.  pigz can be
   compiled with NOTHREAD #defined to not use threads at all (in which case
   pigz will not be able to live up to the "parallel" in its name).
 */

/*
   Details of parallel compression implementation:

   When doing parallel compression, pigz uses the main thread to read the input
   in 'size' sized chunks (see -b), and puts those in a compression job list,
   each with a sequence number to keep track of the ordering.  If it is not the
   first chunk, then that job also points to the previous input buffer, from
   which the last 32K will be used as a dictionary (unless -i is specified).
   This sets a lower limit of 32K on 'size'.

   pigz launches up to 'z->threads' compression threads (see --threads).  Each compression
   thread continues to look for jobs in the compression list and perform those
   jobs until instructed to return.  When a job is pulled, the dictionary, if
   provided, will be loaded into the deflate engine and then that input buffer
   is dropped for reuse.  Then the input data is compressed into an output
   buffer sized to assure that it can contain maximally expanded deflate data.
   The job is then put into the write job list, sorted by the sequence number.
   The compress thread however continues to calculate the check value on the
   input data, either a CRC-32 or Adler-32, possibly in parallel with the write
   thread writing the output data.  Once that's done, the compress thread drops
   the input buffer and also releases the lock on the check value so that the
   write thread can combine it with the previous check values.  The compress
   thread has then completed that job, and goes to look for another.

   All of the compress threads are left running and waiting even after the last
   chunk is processed, so that they can support the next input to be compressed
   (more than one input file on the command line).  Once pigz is done, it will
   call all the compress threads home (that'll do pig, that'll do).

   Before starting to read the input, the main thread launches the write thread
   so that it is ready pick up jobs immediately.  The compress thread puts the
   write jobs in the list in sequence sorted order, so that the first job in
   the list is always has the lowest sequence number.  The write thread waits
   for the next write job in sequence, and then gets that job.  The job still
   holds its input buffer, from which the write thread gets the input buffer
   length for use in check value combination.  Then the write thread drops that
   input buffer to allow its reuse.  Holding on to the input buffer until the
   write thread starts also has the benefit that the read and compress threads
   can't get way ahead of the write thread and build up a large backlog of
   unwritten compressed data.  The write thread will write the compressed data,
   drop the output buffer, and then wait for the check value to be unlocked
   by the compress thread.  Then the write thread combines the check value for
   this chunk with the total check value for eventual use in the trailer.  If
   this is not the last chunk, the write thread then goes back to look for the
   next output chunk in sequence.  After the last chunk, the write thread
   returns and joins the main thread.  Unlike the compress threads, a new write
   thread is launched for each input stream.  The write thread writes the
   appropriate header and trailer around the compressed data.

   The input and output buffers are reused through their collection in pools.
   Each buffer has a use count, which when decremented to zero returns the
   buffer to the respective pool.  Each input buffer has up to three parallel
   uses: as the input for compression, as the data for the check value
   calculation, and as a dictionary for compression.  Each output buffer has
   only one use, which is as the output of compression followed serially as
   data to be written.  The input pool is limited in the number of buffers, so
   that reading does not get way ahead of compression and eat up memory with
   more input than can be used.  The limit is approximately two times the
   number of compression threads.  In the case that reading is fast as compared
   to compression, that number allows a second set of buffers to be read while
   the first set of compressions are being performed.  The number of output
   buffers is not directly limited, but is indirectly limited by the release of
   input buffers to the same number.
 */

#define	DEBUG	/* XXX this shouldn't be here. */

#include "system.h"

#include "zlib.h"       /* deflateInit2(), deflateReset(), deflate(), */
                        /* deflateEnd(), deflateSetDictionary(), crc32(),
                           Z_DEFAULT_COMPRESSION, Z_DEFAULT_STRATEGY,
                           Z_DEFLATED, Z_NO_FLUSH, Z_NULL, Z_OK,
                           Z_SYNC_FLUSH, z_stream */

#if !defined(ZLIB_VERNUM) || ZLIB_VERNUM < 0x1230
#  error Need zlib version 1.2.3 or later
#endif

#if defined(__LCLINT__)
/*@-incondefs@*/
ZEXTERN int ZEXPORT deflateInit2_ OF((z_streamp strm, int  level, int  method,
                                      int windowBits, int memLevel,
                                      int strategy, const char *version,
                                      int stream_size))
	/*@modifies strm @*/;
ZEXTERN int ZEXPORT deflateReset OF((z_streamp strm))
	/*@modifies strm @*/;
ZEXTERN int ZEXPORT deflateParams OF((z_streamp strm,
				      int level, int strategy))
	/*@modifies strm @*/;
ZEXTERN int ZEXPORT deflateSetDictionary OF((z_streamp strm,
                                             const Bytef *dictionary,
                                             uInt  dictLength))
	/*@modifies strm @*/;
ZEXTERN int ZEXPORT deflate OF((z_streamp strm, int flush))
	/*@modifies strm @*/;
ZEXTERN int ZEXPORT deflateEnd OF((z_streamp strm))
	/*@modifies strm @*/;

ZEXTERN int ZEXPORT inflateBackInit_ OF((z_streamp strm, int windowBits,
                                         unsigned char FAR *window,
                                         const char *version,
                                         int stream_size))
	/*@modifies strm @*/;
ZEXTERN int ZEXPORT inflateBack OF((z_streamp strm,
                                    in_func in, void FAR *in_desc,
                                    out_func out, void FAR *out_desc))
	/*@modifies strm @*/;
ZEXTERN int ZEXPORT inflateBackEnd OF((z_streamp strm))
	/*@modifies strm @*/;

ZEXTERN uLong ZEXPORT adler32 OF((uLong adler, const Bytef *buf, uInt len))
	/*@*/;
ZEXTERN uLong ZEXPORT crc32   OF((uLong crc, const Bytef *buf, uInt len))
	/*@*/;
/*@=incondefs@*/

#endif

#if defined(DEBUG) || defined(__LCLINT__)
/* trace log */
struct log {
    struct timeval when;	/* time of entry */
    char *msg;			/* message */
    struct log *next;		/* next entry */
};
#endif

#define	_RPMIOB_INTERNAL
#define	_RPMZ_INTERNAL
#include "rpmz.h"

#include "debug.h"

/*@unchecked@*/
static int _debug = 0;

#define F_ISSET(_f, _FLAG) (((_f) & ((RPMZ_FLAGS_##_FLAG) & ~0x40000000)) != RPMZ_FLAGS_NONE)
#define RZ_ISSET(_FLAG) F_ISSET(z->flags, _FLAG)

/**
 */
struct rpmz_s {
    enum rpmzFlags_e flags;	/*!< Control bits. */
    enum rpmzFormat_e format;	/*!< Compression format. */
    enum rpmzMode_e mode;	/*!< Operation mode. */
    unsigned int level;		/*!< Compression level. */

#ifdef	NOTYET
    rpmuint64_t mem;		/*!< Physical memory. */
    rpmuint64_t memlimit_encoder;
    rpmuint64_t memlimit_decoder;
    rpmuint64_t memlimit_custom;
#endif

    unsigned int threads;	/*!< No. or threads to use. */

#ifdef	NOTYET
/*@observer@*/
    const char *stdin_filename;	/*!< Display name for stdin. */
/*@observer@*/
    const char *stdout_filename;/*!< Display name for stdout. */

/*@relnull@*/
    rpmiob iob;			/*!< Buffer for I/O. */
    size_t nb;			/*!< Buffer size (in bytes) */

/*@null@*/
    ARGV_t argv;		/*!< URI's to process. */
/*@null@*/
    ARGV_t manifests;		/*!<    --files ... */
/*@null@*/
    const char * base_prefix;

/*@null@*/
    const char * isuffix;
    enum rpmzFormat_e ifmt;
    FDIO_t idio;
/*@null@*/
    const char * ifn;
    char ifmode[32];
/*@null@*/
    FD_t ifd;
    struct stat isb;

/*@null@*/
    const char * osuffix;
    enum rpmzFormat_e ofmt;
    FDIO_t odio;
/*@null@*/
    const char * ofn;		/*!< output file name (allocated if not NULL) */
    char ofmode[32];
/*@null@*/
    FD_t ofd;
    struct stat osb;
#endif	/* NOTYET */

/*@null@*/ /*@observer@*/
    const char * suffix;	/*!< -S, --suffix ... */

    /* PIGZ specific configuration. */
    int verbosity;        /*!< 0 = quiet, 1 = normal, 2 = verbose, 3 = trace */

/* --- globals (modified by main thread only when it's the only thread) */
    char _ifn[PATH_MAX+1];	/*!< input file name (accommodate recursion) */
/*@relnull@*/
    char * _ofn;		/*!< output file name (allocated if not NULL) */
    int ifdno;			/*!< input file descriptor */
    int ofdno;			/*!< output file descriptor */

    /* XXX PIGZ used size_t not unsigned int. */
    unsigned int blocksize; /*!< uncompressed input size per thread (>= 32K) */

/*@relnull@*/
    rpmzPool in_pool;		/*!< input buffer pool (malloc'd). */
/*@relnull@*/
    rpmzPool out_pool;		/*!< output buffer pool (malloc'd). */

    /* list of compress jobs (with tail for appending to list) */
/*@only@*/ /*@null@*/
    yarnLock compress_have;	/*!< number of compress jobs waiting */
/*@null@*/
    rpmzJob compress_head;
/*@shared@*/
    rpmzJob * compress_tail;

/*@only@*/ /*@null@*/
    yarnLock write_first;	/*!< lowest sequence number in list */
/*@null@*/
    rpmzJob write_head;		/*!< list of write jobs */
    int cthreads;		/*!< number of compression threads running */

/*@only@*/ /*@null@*/
    yarnThread writeth;		/*!< write thread if running */

/* --- globals for decompression and listing buffered reading */
    int in_which;		/*!< -1: start, 0: in_buf2, 1: in_buf */
#define IN_BUF_ALLOCATED 32768U	/* input buffer size */
    size_t in_buf_allocated;
    unsigned char in_buf[IN_BUF_ALLOCATED];	/*!< input buffer */
    unsigned char in_buf2[IN_BUF_ALLOCATED];	/*! second buffer for parallel reads */

/*@shared@*/
    unsigned char * in_next;	/*!< next unused byte in buffer */
    size_t in_left;		/*!< number of unused bytes in buffer */
    int in_eof;			/*!< true if reached end of file on input */
    int in_short;		/*!< true if last read didn't fill buffer */
    off_t in_tot;		/*!< total bytes read from input */
    off_t out_tot;		/*!< total bytes written to output */
    unsigned long out_check;	/*!< check value of output */

    /* parallel reading */
    size_t in_len;		/*!< data waiting in next buffer */
/*@only@*/ /*@null@*/
    yarnLock load_state;	/*!< value = 0 to wait, 1 to read a buffer */
/*@only@*/ /*@null@*/
    yarnThread load_thread;	/*!< load_read_thread() thread for joining */

    /* output buffers/window for rpmzInflateCheck() and rpmzDecompressLZW() */
    size_t out_buf_allocated;
#define OUT_BUF_ALLOCATED 32768U /*!< must be at least 32K for inflateBack() window */
    unsigned char out_buf[OUT_BUF_ALLOCATED];
    /* output data for parallel write and check */
    unsigned char out_copy[OUT_BUF_ALLOCATED];
    size_t out_len;

/*@only@*/ /*@null@*/
    yarnLock outb_write_more;	/*!< outb write threads states */
/*@only@*/ /*@null@*/
    yarnLock outb_check_more;	/*!< outb check threads states */

/* --- memory for rpmzDecompressLZW()
 * the first 256 entries of prefix[] and suffix[] are never used, could
 * have offset the index, but it's faster to waste the memory
 */
    unsigned short _prefix[65536];	/*!< index to LZW prefix string */
    unsigned char _suffix[65536];	/*!< one-character LZW suffix */
    unsigned char _match[65280 + 2];	/*!< buffer for reversed match */

/*@observer@*/ /*@null@*/
    const char * name;		/*!< name for gzip header */
    time_t mtime;		/*!< time stamp for gzip header */

/* saved gzip/zip header data for decompression, testing, and listing */
    time_t stamp;		/*!< time stamp from gzip header */
/*@only@*/ /*@null@*/
    char * hname;		/*!< name from header (allocated) */
    unsigned long zip_crc;	/*!< header crc */
    unsigned long zip_clen;	/*!< header compressed length */
    unsigned long zip_ulen;	/*!< header uncompressed length */

#if defined(DEBUG) || defined(__LCLINT__)
    struct timeval start;	/*!< starting time of day for tracing */
/*@null@*/
    struct log *log_head;
/*@shared@*/ /*@relnull@*/
    struct log **log_tail;
/*@only@*/ /*@null@*/
    yarnLock log_lock;
#endif	/* DEBUG */

};

/*@-fullinitblock@*/
/*@unchecked@*/
static struct rpmz_s __rpmz = {
  /* XXX logic is reversed, disablers should clear with toggle. */
    .flags	= RPMZ_FLAGS_INDEPENDENT   /* initialize dictionary each thread */
      |	(RPMZ_FLAGS_HNAME|RPMZ_FLAGS_HTIME),/* store/restore name and timestamp */
    .format	= RPMZ_FORMAT_GZIP,	/* use gzip format */
    .mode	= RPMZ_MODE_COMPRESS,	/* compress */
    .level	= Z_DEFAULT_COMPRESSION,/* XXX level is format specific. */

    .blocksize	= 131072UL,
#ifdef _PIGZNOTHREAD
    .threads	= 1,
#else
    .threads	= 8,
#endif
    .verbosity	= 1,		/* normal message level */
    .suffix	= "gz",		/* compressed file suffix */

#ifdef	NOTYET
    .stdin_filename = "(stdin)",
    .stdout_filename = "(stdout)",
#endif	/* NOTYET */

    .in_buf_allocated = IN_BUF_ALLOCATED,
    .out_buf_allocated = OUT_BUF_ALLOCATED,

};
/*@=fullinitblock@*/

/* set option defaults */
static void rpmzDefaults(rpmz z)
	/*@modifies z @*/
{
    z->level = Z_DEFAULT_COMPRESSION;
#ifdef _PIGZNOTHREAD
    z->threads = 1;
#else
    z->threads = 8;
#endif
    z->blocksize = 131072UL;
    z->flags &= ~RPMZ_FLAGS_RSYNCABLE; /* don't do rsync blocking */
    z->flags |= RPMZ_FLAGS_INDEPENDENT; /* initialize dictionary each thread */
    z->verbosity = 1;                  /* normal message level */
    z->flags |= (RPMZ_FLAGS_HNAME|RPMZ_FLAGS_HTIME); /* store/restore name and timestamp */
    z->flags &= ~RPMZ_FLAGS_STDOUT; /* don't force output to stdout */
    z->suffix = ".gz";              /* compressed file suffix */
    z->mode = RPMZ_MODE_COMPRESS;   /* compress */
    z->flags &= ~RPMZ_FLAGS_LIST;   /* compress */
    z->flags &= ~RPMZ_FLAGS_KEEP;   /* delete input file once compressed */
    z->flags &= ~RPMZ_FLAGS_FORCE;  /* don't overwrite, don't compress links */
    z->flags &= ~RPMZ_FLAGS_RECURSE;/* don't go into directories */
    z->format = RPMZ_FORMAT_GZIP;   /* use gzip format */
}

/*@unchecked@*/
rpmz _rpmz = &__rpmz;

/*==============================================================*/

/* prevent end-of-line conversions on MSDOSish operating systems */
#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <io.h>       /* setmode(), O_BINARY */
#  define SET_BINARY_MODE(_fdno) setmode(_fdno, O_BINARY)
#else
#  define SET_BINARY_MODE(_fdno)
#endif

/* exit with error, delete output file if in the middle of writing it */
/*@exits@*/
static int bail(char *why, char *what)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    rpmz z = _rpmz;

/*@-globs@*/
    if (z->ofdno != -1 && z->_ofn != NULL)
	Unlink(z->_ofn);
/*@=globs@*/
    if (z->verbosity > 0)
	fprintf(stderr, "pigz abort: %s%s\n", why, what);
    exit(EXIT_FAILURE);
/*@notreached@*/
    return 0;
}

/*==============================================================*/

#if defined(DEBUG) || defined(__LCLINT__)

/* maximum log entry length */
#define _PIGZMAXMSG 256

/* set up log (call from main thread before other threads launched) */
static void rpmzLogInit(rpmz z)
	/*@globals fileSystem, internalState @*/
	/*@modifies z, fileSystem, internalState @*/
{
    if (z->log_tail == NULL) {
/*@-mustfreeonly@*/
#ifndef _PIGZNOTHREAD
	z->log_lock = yarnNewLock(0);
#endif
	z->log_head = NULL;
	z->log_tail = &z->log_head;
/*@=mustfreeonly@*/
    }
}

/* add entry to trace log */
static void rpmzLogAdd(rpmz z, char *fmt, ...)
	/*@globals fileSystem, internalState @*/
	/*@modifies z, fileSystem, internalState @*/
{
    struct timeval now;
    struct log *me;
    va_list ap;
    char msg[_PIGZMAXMSG];

    gettimeofday(&now, NULL);
    me = xmalloc(sizeof(*me));
    me->when = now;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg)-1, fmt, ap);
    va_end(ap);
    msg[sizeof(msg)-1] = '\0';

/*@-mustfreeonly@*/
    me->msg = xmalloc(strlen(msg) + 1);
/*@=mustfreeonly@*/
    strcpy(me->msg, msg);
/*@-mustfreeonly@*/
    me->next = NULL;
/*@=mustfreeonly@*/
#ifndef _PIGZNOTHREAD
assert(z->log_lock != NULL);
    yarnPossess(z->log_lock);
#endif
    *z->log_tail = me;
    z->log_tail = &(me->next);
#ifndef _PIGZNOTHREAD
    yarnTwist(z->log_lock, BY, 1);
#endif
}

/* pull entry from trace log and print it, return false if empty */
static int rpmzLogShow(rpmz z)
	/*@globals fileSystem, internalState @*/
	/*@modifies z, fileSystem, internalState @*/
{
    struct log *me;
    struct timeval diff;

    if (z->log_tail == NULL)
	return 0;
#ifndef _PIGZNOTHREAD
    yarnPossess(z->log_lock);
#endif
    me = z->log_head;
    if (me == NULL) {
#ifndef _PIGZNOTHREAD
	yarnRelease(z->log_lock);
#endif
	return 0;
    }
    z->log_head = me->next;
    if (me->next == NULL)
	z->log_tail = &z->log_head;
#ifndef _PIGZNOTHREAD
    yarnTwist(z->log_lock, BY, -1);
#endif
    diff.tv_usec = me->when.tv_usec - z->start.tv_usec;
    diff.tv_sec = me->when.tv_sec - z->start.tv_sec;
    if (diff.tv_usec < 0) {
	diff.tv_usec += 1000000L;
	diff.tv_sec--;
    }
    fprintf(stderr, "trace %ld.%06ld %s\n",
	    (long)diff.tv_sec, (long)diff.tv_usec, me->msg);
    fflush(stderr);
    free(me->msg);
    free(me);
    return 1;
}

/* release log resources (need to do rpmzLogInit() to use again) */
static void rpmzLogFree(rpmz z)
	/*@globals fileSystem, internalState @*/
	/*@modifies z, fileSystem, internalState @*/
{
    struct log *me;

    if (z->log_tail != NULL) {
#ifndef _PIGZNOTHREAD
	yarnPossess(z->log_lock);
#endif
	while ((me = z->log_head) != NULL) {
	    z->log_head = me->next;
	    me->msg = _free(me->msg);
/*@-compdestroy@*/
	    me = _free(me);
/*@=compdestroy@*/
	}
#ifndef _PIGZNOTHREAD
	yarnTwist(z->log_lock, TO, 0);
	z->log_lock = yarnFreeLock(z->log_lock);
#endif
	z->log_tail = NULL;
    }
}

/* show entries until no more, free log */
static void rpmzLogDump(rpmz z)
	/*@globals fileSystem, internalState @*/
	/*@modifies z, fileSystem, internalState @*/
{
    if (z->log_tail == NULL)
	return;
    while (rpmzLogShow(z))
	;
    rpmzLogFree(z);
}

/* debugging macro */
#define Trace(x) \
    do { \
	if (z->verbosity > 2) { \
	    rpmzLogAdd x; \
	} \
    } while (0)

#else	/* !DEBUG */

#define rpmzLogDump(_z)
#define Trace(x)

#endif	/* DEBUG */

/*==============================================================*/

/* read up to len bytes into buf, repeating read() calls as needed */
static size_t rpmzRead(rpmz z, unsigned char *buf, size_t len)
	/*@globals fileSystem, internalState @*/
	/*@modifies *buf, fileSystem, internalState @*/
{
    int fdno = z->ifdno;
    ssize_t ret;
    size_t got;

    got = 0;
    while (len) {
	ret = read(fdno, buf, len);
	if (ret < 0)
	    bail("read error on ", z->_ifn);
	if (ret == 0)
	    break;
	buf += ret;
	len -= ret;
	got += ret;
    }
    return got;
}

/* write len bytes, repeating write() calls as needed */
static void rpmzWrite(rpmz z, unsigned char *buf, size_t len)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    int fdno = z->ofdno;
    ssize_t ret;

    while (len) {
	ret = write(fdno, buf, len);
	if (ret < 1)
	    fprintf(stderr, "write error code %d\n", errno);
	if (ret < 1)
	    bail("write error on ", z->_ofn);
	buf += ret;
	len -= ret;
    }
}

/* sliding dictionary size for deflate */
#define _PIGZDICT 32768U

/* largest power of 2 that fits in an unsigned int -- used to limit requests
   to zlib functions that use unsigned int lengths */
#define _PIGZMAX ((((unsigned)0 - 1) >> 1) + 1)

/* convert Unix time to MS-DOS date and time, assuming current timezone
   (you got a better idea?) */
static unsigned long time2dos(time_t t)
	/*@*/
{
    struct tm *tm;
    unsigned long dos;

    if (t == 0)
	t = time(NULL);
    tm = localtime(&t);
    if (tm->tm_year < 80 || tm->tm_year > 207)
	return 0;
    dos = (tm->tm_year - 80) << 25;
    dos += (tm->tm_mon + 1) << 21;
    dos += tm->tm_mday << 16;
    dos += tm->tm_hour << 11;
    dos += tm->tm_min << 5;
    dos += (tm->tm_sec + 1) >> 1;   /* round to double-seconds */
    return dos;
}

/* put a 4-byte integer into a byte array in LSB order or MSB order */
#define PUT2L(a,b) (*(a)=(b)&0xff,(a)[1]=(b)>>8)
#define PUT4L(a,b) (PUT2L(a,(b)&0xffff),PUT2L((a)+2,(b)>>16))
#define PUT4M(a,b) (*(a)=(b)>>24,(a)[1]=(b)>>16,(a)[2]=(b)>>8,(a)[3]=(b))

/* write a gzip, zlib, or zip header using the information in the globals */
static unsigned long rpmzPutHeader(rpmz z)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    unsigned long len;
    unsigned char head[30];

    switch (z->format) {
    case RPMZ_FORMAT_ZIP2:	/* zip */
    case RPMZ_FORMAT_ZIP3:
	/* write local header */
	PUT4L(head, 0x04034b50UL);  /* local header signature */
	PUT2L(head + 4, 20);        /* version needed to extract (2.0) */
	PUT2L(head + 6, 8);         /* flags: data descriptor follows data */
	PUT2L(head + 8, 8);         /* deflate */
	PUT4L(head + 10, time2dos(z->mtime));
	PUT4L(head + 14, 0);        /* crc (not here) */
	PUT4L(head + 18, 0);        /* compressed length (not here) */
	PUT4L(head + 22, 0);        /* uncompressed length (not here) */
	PUT2L(head + 26, z->name == NULL ? 1 : strlen(z->name));  /* name length */
	PUT2L(head + 28, 9);        /* length of extra field (see below) */
	rpmzWrite(z, head, 30);     /* write local header */
	len = 30;

	/* write file name (use "-" for stdin) */
	if (z->name == NULL)
	    rpmzWrite(z, (unsigned char *)"-", 1);
	else
	    rpmzWrite(z, (unsigned char *)z->name, strlen(z->name));
	len += z->name == NULL ? 1 : strlen(z->name);

	/* write extended timestamp extra field block (9 bytes) */
	PUT2L(head, 0x5455);        /* extended timestamp signature */
	PUT2L(head + 2, 5);         /* number of data bytes in this block */
	head[4] = 1;                /* flag presence of mod time */
	PUT4L(head + 5, z->mtime);  /* mod time */
	rpmzWrite(z, head, 9);      /* write extra field block */
	len += 9;
	break;
    case RPMZ_FORMAT_ZLIB:	/* zlib */
	head[0] = 0x78;             /* deflate, 32K window */
	head[1] = (z->level == 9 ? 3 : (z->level == 1 ? 0 :
	    (z->level >= 6 || z->level == Z_DEFAULT_COMPRESSION ? 1 :  2))) << 6;
	head[1] += 31 - (((head[0] << 8) + head[1]) % 31);
	rpmzWrite(z, head, 2);
	len = 2;
	break;
    case RPMZ_FORMAT_GZIP:	/* gzip */
	head[0] = 31;
	head[1] = 139;
	head[2] = 8;                /* deflate */
	head[3] = z->name != NULL ? 8 : 0;
	PUT4L(head + 4, z->mtime);
	head[8] = z->level == 9 ? 2 : (z->level == 1 ? 4 : 0);
	head[9] = 3;                /* unix */
	rpmzWrite(z, head, 10);
	len = 10;
	if (z->name != NULL)
	    rpmzWrite(z, (unsigned char *)z->name, strlen(z->name) + 1);
	if (z->name != NULL)
	    len += strlen(z->name) + 1;
	break;
    default:
assert(0);
	break;
    }
    return len;
}

/* write a gzip, zlib, or zip trailer */
static void rpmzPutTrailer(rpmz z, unsigned long ulen, unsigned long clen,
		unsigned long check, unsigned long head)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    unsigned char tail[46];

    switch (z->format) {
    case RPMZ_FORMAT_ZIP2:	/* zip */
    case RPMZ_FORMAT_ZIP3:
    {	unsigned long cent;

	/* write data descriptor (as promised in local header) */
	PUT4L(tail, check);
	PUT4L(tail + 4, clen);
	PUT4L(tail + 8, ulen);
	rpmzWrite(z, tail, 12);

	/* write central file header */
	PUT4L(tail, 0x02014b50UL);  /* central header signature */
	tail[4] = 63;               /* obeyed version 6.3 of the zip spec */
	tail[5] = 255;              /* ignore external attributes */
	PUT2L(tail + 6, 20);        /* version needed to extract (2.0) */
	PUT2L(tail + 8, 8);         /* data descriptor is present */
	PUT2L(tail + 10, 8);        /* deflate */
	PUT4L(tail + 12, time2dos(z->mtime));
	PUT4L(tail + 16, check);    /* crc */
	PUT4L(tail + 20, clen);     /* compressed length */
	PUT4L(tail + 24, ulen);     /* uncompressed length */
	PUT2L(tail + 28, z->name == NULL ? 1 : strlen(z->name));  /* name length */
	PUT2L(tail + 30, 9);        /* length of extra field (see below) */
	PUT2L(tail + 32, 0);        /* no file comment */
	PUT2L(tail + 34, 0);        /* disk number 0 */
	PUT2L(tail + 36, 0);        /* internal file attributes */
	PUT4L(tail + 38, 0);        /* external file attributes (ignored) */
	PUT4L(tail + 42, 0);        /* offset of local header */
	rpmzWrite(z, tail, 46);     /* write central file header */
	cent = 46;

	/* write file name (use "-" for stdin) */
	if (z->name == NULL)
	    rpmzWrite(z, (unsigned char *)"-", 1);
	else
	    rpmzWrite(z, (unsigned char *)z->name, strlen(z->name));
	cent += z->name == NULL ? 1 : strlen(z->name);

	/* write extended timestamp extra field block (9 bytes) */
	PUT2L(tail, 0x5455);        /* extended timestamp signature */
	PUT2L(tail + 2, 5);         /* number of data bytes in this block */
	tail[4] = 1;                /* flag presence of mod time */
	PUT4L(tail + 5, z->mtime);  /* mod time */
	rpmzWrite(z, tail, 9);      /* write extra field block */
	cent += 9;

	/* write end of central directory record */
	PUT4L(tail, 0x06054b50UL);  /* end of central directory signature */
	PUT2L(tail + 4, 0);         /* number of this disk */
	PUT2L(tail + 6, 0);         /* disk with start of central directory */
	PUT2L(tail + 8, 1);         /* number of entries on this disk */
	PUT2L(tail + 10, 1);        /* total number of entries */
	PUT4L(tail + 12, cent);     /* size of central directory */
	PUT4L(tail + 16, head + clen + 12); /* offset of central directory */
	PUT2L(tail + 20, 0);        /* no zip file comment */
	rpmzWrite(z, tail, 22);     /* write end of central directory record */
    }	break;
    case RPMZ_FORMAT_ZLIB:	/* zlib */
	PUT4M(tail, check);
	rpmzWrite(z, tail, 4);
	break;
    case RPMZ_FORMAT_GZIP:	/* gzip */
	PUT4L(tail, check);
	PUT4L(tail + 4, ulen);
	rpmzWrite(z, tail, 8);
	break;
    default:
assert(0);
	break;
    }
}

/* compute check value depending on format */
#define CHECK(a,b,c) \
    (z->format == RPMZ_FORMAT_ZLIB ? adler32(a,b,c) : crc32(a,b,c))

#ifndef _PIGZNOTHREAD
/* -- threaded portions of pigz -- */

/* -- check value combination routines for parallel calculation -- */

#define COMB(a,b,c) \
    (z->format == RPMZ_FORMAT_ZLIB ? adler32_comb(a,b,c) : crc32_comb(a,b,c))
/* combine two crc-32's or two adler-32's (copied from zlib 1.2.3 so that pigz
   can be compatible with older versions of zlib) */

/* we copy the combination routines from zlib here, in order to avoid
   linkage issues with the zlib builds on Sun, Ubuntu, and others */

static unsigned long gf2_matrix_times(unsigned long *mat, unsigned long vec)
	/*@*/
{
    unsigned long sum;

    sum = 0;
    while (vec) {
	if (vec & 1)
	    sum ^= *mat;
	vec >>= 1;
	mat++;
    }
    return sum;
}

static void gf2_matrix_square(unsigned long *square, unsigned long *mat)
	/*@modifies square @*/
{
    int n;

    for (n = 0; n < 32; n++)
	square[n] = gf2_matrix_times(mat, mat[n]);
}

static unsigned long crc32_comb(unsigned long crc1, unsigned long crc2,
		size_t len2)
	/*@*/
{
    int n;
    unsigned long row;
    unsigned long even[32];     /* even-power-of-two zeros operator */
    unsigned long odd[32];      /* odd-power-of-two zeros operator */

    /* degenerate case */
    if (len2 == 0)
	return crc1;

    /* put operator for one zero bit in odd */
    odd[0] = 0xedb88320UL;          /* CRC-32 polynomial */
    row = 1;
    for (n = 1; n < 32; n++) {
	odd[n] = row;
	row <<= 1;
    }

    /* put operator for two zero bits in even */
    gf2_matrix_square(even, odd);

    /* put operator for four zero bits in odd */
    gf2_matrix_square(odd, even);

    /* apply len2 zeros to crc1 (first square will put the operator for one
       zero byte, eight zero bits, in even) */
    do {
	/* apply zeros operator for this bit of len2 */
	gf2_matrix_square(even, odd);
	if (len2 & 1)
	    crc1 = gf2_matrix_times(even, crc1);
	len2 >>= 1;

	/* if no more bits set, then done */
	if (len2 == 0)
	    break;

	/* another iteration of the loop with odd and even swapped */
	gf2_matrix_square(odd, even);
	if (len2 & 1)
	    crc1 = gf2_matrix_times(odd, crc1);
	len2 >>= 1;

	/* if no more bits set, then done */
    } while (len2 != 0);

    /* return combined crc */
    crc1 ^= crc2;
    return crc1;
}

#define BASE 65521U     /* largest prime smaller than 65536 */
#define LOW16 0xffff    /* mask lower 16 bits */

static unsigned long adler32_comb(unsigned long adler1, unsigned long adler2,
		size_t len2)
	/*@*/
{
    unsigned long sum1;
    unsigned long sum2;
    unsigned rem;

    /* the derivation of this formula is left as an exercise for the reader */
    rem = (unsigned)(len2 % BASE);
    sum1 = adler1 & LOW16;
    sum2 = (rem * sum1) % BASE;
    sum1 += (adler2 & LOW16) + BASE - 1;
    sum2 += ((adler1 >> 16) & LOW16) + ((adler2 >> 16) & LOW16) + BASE - rem;
    if (sum1 >= BASE) sum1 -= BASE;
    if (sum1 >= BASE) sum1 -= BASE;
    if (sum2 >= (BASE << 1)) sum2 -= (BASE << 1);
    if (sum2 >= BASE) sum2 -= BASE;
    return sum1 | (sum2 << 16);
}

/*==============================================================*/

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
static rpmzPool rpmzNewPool(size_t size, int limit)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    rpmzPool pool = xmalloc(sizeof(*pool));
/*@-mustfreeonly@*/
    pool->have = yarnNewLock(0);
    pool->head = NULL;
/*@=mustfreeonly@*/
    pool->size = size;
    pool->limit = limit;
    pool->made = 0;
    return pool;
}

/* get a space from a pool -- the use count is initially set to one, so there
   is no need to call rpmzUseSpace() for the first use */
static rpmzSpace rpmzNewSpace(rpmzPool pool)
	/*@globals fileSystem, internalState @*/
	/*@modifies pool, fileSystem, internalState @*/
{
    rpmzSpace space;

    /* if can't create any more, wait for a space to show up */
    yarnPossess(pool->have);
    if (pool->limit == 0)
	yarnWaitFor(pool->have, NOT_TO_BE, 0);

    /* if a space is available, pull it from the list and return it */
    if (pool->head != NULL) {
	space = pool->head;
	yarnPossess(space->use);
	pool->head = space->next;
	yarnTwist(pool->have, BY, -1);      /* one less in pool */
	yarnTwist(space->use, TO, 1);       /* initially one user */
	return space;
    }

    /* nothing available, don't want to wait, make a new space */
assert(pool->limit != 0);
    if (pool->limit > 0)
	pool->limit--;
    pool->made++;
    yarnRelease(pool->have);

    space = xmalloc(sizeof(*space));
/*@-mustfreeonly@*/
    space->use = yarnNewLock(1);           /* initially one user */
    space->buf = xmalloc(pool->size);
    space->pool = pool;                 /* remember the pool this belongs to */
/*@=mustfreeonly@*/
/*@-nullret@*/
    return space;
/*@=nullret@*/
}

/* increment the use count to require one more drop before returning this space
   to the pool */
static void rpmzUseSpace(rpmzSpace space)
	/*@globals fileSystem, internalState @*/
	/*@modifies space, fileSystem, internalState @*/
{
    yarnPossess(space->use);
    yarnTwist(space->use, BY, 1);
}

/* drop a space, returning it to the pool if the use count is zero */
/*@null@*/
static rpmzSpace rpmzDropSpace(/*@only@*/ rpmzSpace space)
	/*@globals fileSystem, internalState @*/
	/*@modifies space, fileSystem, internalState @*/
{
    int use;

    yarnPossess(space->use);
    use = yarnPeekLock(space->use);
assert(use != 0);
    if (use == 1) {
	rpmzPool pool = space->pool;
	yarnPossess(pool->have);
/*@-mustfreeonly@*/
	space->next = pool->head;
/*@=mustfreeonly@*/
	pool->head = space;
	yarnTwist(pool->have, BY, 1);
    }
    yarnTwist(space->use, BY, -1);
    return NULL;
}

/* free the memory and lock resources of a pool -- return number of spaces for
   debugging and resource usage measurement */
/*@null@*/
static rpmzPool rpmzFreePool(/*@only@*/ rpmzPool pool, /*@null@*/ int *countp)
	/*@globals fileSystem, internalState @*/
	/*@modifies pool, *countp, fileSystem, internalState @*/
{
    rpmzSpace space;
    int count;

    yarnPossess(pool->have);
    count = 0;
    while ((space = pool->head) != NULL) {
	pool->head = space->next;
	space->buf = _free(space->buf);
	space->use = yarnFreeLock(space->use);
/*@-compdestroy@*/
	space = _free(space);
/*@=compdestroy@*/
	count++;
    }
    yarnRelease(pool->have);
    pool->have = yarnFreeLock(pool->have);
assert(count == pool->made);
/*@-compdestroy@*/
    pool = _free(pool);
/*@=compdestroy@*/
    if (countp)
	*countp = count;
    return NULL;
}

/*==============================================================*/

/* -- parallel compression -- */

/* compress or write job (passed from compress list to write list) -- if seq is
   equal to -1, compress_thread() is instructed to return; if more is false then
   this is the last chunk, which after writing tells write_thread to return */

/* setup job lists (call from main thread) */
static void rpmzSetupJobs(rpmz z)
	/*@globals fileSystem, internalState @*/
	/*@modifies z, fileSystem, internalState @*/
{
    /* set up only if not already set up*/
    if (z->compress_have != NULL)
	return;

    /* allocate locks and initialize lists */
/*@-mustfreeonly@*/
    z->compress_have = yarnNewLock(0);
    z->compress_head = NULL;
    z->compress_tail = &z->compress_head;
    z->write_first = yarnNewLock(-1);
    z->write_head = NULL;

    /* initialize buffer pools */
    z->in_pool = rpmzNewPool(z->blocksize, (z->threads << 1) + 2);
    z->out_pool = rpmzNewPool(z->blocksize + (z->blocksize >> 11) + 10, -1);
/*@=mustfreeonly@*/
}

/* command the compress threads to all return, then join them all (call from
   main thread), free all the thread-related resources */
static void rpmzFinishJobs(rpmz z)
	/*@globals fileSystem, internalState @*/
	/*@modifies z, fileSystem, internalState @*/
{
    struct rpmzJob_s job;
    int caught;

    /* only do this once */
    if (z->compress_have == NULL)
	return;

    /* command all of the extant compress threads to return */
    yarnPossess(z->compress_have);
    job.seq = -1;
    job.next = NULL;
/*@-immediatetrans -mustfreeonly@*/
    z->compress_head = &job;
/*@=immediatetrans =mustfreeonly@*/
    z->compress_tail = &(job.next);
    yarnTwist(z->compress_have, BY, 1);       /* will wake them all up */

    /* join all of the compress threads, verify they all came back */
    caught = yarnJoinAll();
    Trace((z, "-- joined %d compress threads", caught));
assert(caught == z->cthreads);
    z->cthreads = 0;

    /* free the resources */
    z->out_pool = rpmzFreePool(z->out_pool, &caught);
    Trace((z, "-- freed %d output buffers", caught));
    z->in_pool = rpmzFreePool(z->in_pool, &caught);
    Trace((z, "-- freed %d input buffers", caught));
    z->write_first = yarnFreeLock(z->write_first);
    z->compress_have = yarnFreeLock(z->compress_have);
}

/* get the next compression job from the head of the list, compress and compute
   the check value on the input, and put a job in the write list with the
   results -- keep looking for more jobs, returning when a job is found with a
   sequence number of -1 (leave that job in the list for other incarnations to
   find) */
/*@-nullstate@*/
static void compress_thread(void *_z)
	/*@globals fileSystem, internalState @*/
	/*@modifies _z, fileSystem, internalState @*/
{
    rpmz z = _z;
    rpmzJob job;                    /* job pulled and working on */ 
    rpmzJob here;                   /* pointers for inserting in write list */ 
    rpmzJob * prior;                /* pointers for inserting in write list */ 
    unsigned long check;            /* check value of input */
    unsigned char *next;            /* pointer for check value data */
    size_t len;                     /* remaining bytes to compress/check */
    z_stream strm;                  /* deflate stream */

    /* initialize the deflate stream for this thread */
    strm.zfree = Z_NULL;
    strm.zalloc = Z_NULL;
    strm.opaque = Z_NULL;
    if (deflateInit2(&strm, z->level, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) !=
	    Z_OK)
	bail("not enough memory", "");

    /* keep looking for work */
    for (;;) {
	/* get a job (like I tell my son) */
	yarnPossess(z->compress_have);
	yarnWaitFor(z->compress_have, NOT_TO_BE, 0);
	job = z->compress_head;
assert(job != NULL);
	if (job->seq == -1)
	    break;
/*@-dependenttrans@*/
	z->compress_head = job->next;
/*@=dependenttrans@*/
	if (job->next == NULL)
	    z->compress_tail = &z->compress_head;
	yarnTwist(z->compress_have, BY, -1);

	/* got a job -- initialize and set the compression level (note that if
	 * deflateParams() is called immediately after deflateReset(), there is
	 * no need to initialize the input/output for the stream) */
	Trace((z, "-- compressing #%ld", job->seq));
/*@-noeffect@*/
	(void)deflateReset(&strm);
	(void)deflateParams(&strm, z->level, Z_DEFAULT_STRATEGY);
/*@=noeffect@*/

	/* set dictionary if provided, release that input buffer (only provided
	 * if F_ISSET(z->flags, INDEPENDENT) is true and if this is not the first work unit) -- the
	 * amount of data in the buffer is assured to be >= _PIGZDICT */
	if (job->out != NULL) {
	    len = job->out->len;
/*@-noeffect@*/
	    deflateSetDictionary(&strm,
		(unsigned char *)(job->out->buf) + (len - _PIGZDICT), _PIGZDICT);
/*@=noeffect@*/
	    rpmzDropSpace(job->out);
	}

	/* set up input and output (the output size is assured to be big enough
	 * for the worst case expansion of the input buffer size, plus five
	 * bytes for the terminating stored block) */
/*@-mustfreeonly@*/
	job->out = rpmzNewSpace(z->out_pool);
/*@=mustfreeonly@*/
	strm.next_in = job->in->buf;
	strm.next_out = job->out->buf;

	/* run _PIGZMAX-sized amounts of input through deflate -- this loop is
	 * needed for those cases where the integer type is smaller than the
	 * size_t type, or when len is close to the limit of the size_t type */
	len = job->in->len;
	while (len > _PIGZMAX) {
	    strm.avail_in = _PIGZMAX;
	    strm.avail_out = (unsigned)-1;
/*@-noeffect@*/
	    (void)deflate(&strm, Z_NO_FLUSH);
/*@=noeffect@*/
assert(strm.avail_in == 0 && strm.avail_out != 0);
	    len -= _PIGZMAX;
	}

	/* run the last piece through deflate -- terminate with a sync marker,
	 * or finish deflate stream if this is the last block */
	strm.avail_in = (unsigned)len;
	strm.avail_out = (unsigned)-1;
/*@-noeffect@*/
	(void)deflate(&strm, job->more ? Z_SYNC_FLUSH :  Z_FINISH);
/*@=noeffect@*/
assert(strm.avail_in == 0 && strm.avail_out != 0);
	job->out->len = strm.next_out - (unsigned char *)(job->out->buf);
	Trace((z, "-- compressed #%ld%s", job->seq, job->more ? "" : " (last)"));

	/* reserve input buffer until check value has been calculated */
	rpmzUseSpace(job->in);

	/* insert write job in list in sorted order, alert write thread */
	yarnPossess(z->write_first);
	prior = &z->write_head;
	while ((here = *prior) != NULL) {
	    if (here->seq > job->seq)
		break;
	    prior = &(here->next);
	}
	job->next = here;
	*prior = job;
	yarnTwist(z->write_first, TO, z->write_head->seq);

	/* calculate the check value in parallel with writing, alert the write
	 * thread that the calculation is complete, and drop this usage of the
	 * input buffer */
	len = job->in->len;
	next = job->in->buf;
	check = CHECK(0L, Z_NULL, 0);
	while (len > _PIGZMAX) {
	    check = CHECK(check, next, _PIGZMAX);
	    len -= _PIGZMAX;
	    next += _PIGZMAX;
	}
	check = CHECK(check, next, (unsigned)len);
	rpmzDropSpace(job->in);
	job->check = check;
	yarnPossess(job->calc);
	yarnTwist(job->calc, TO, 1);
	Trace((z, "-- checked #%ld%s", job->seq, job->more ? "" : " (last)"));

	/* done with that one -- go find another job */
    }

    /* found job with seq == -1 -- free deflate memory and return to join */
    yarnRelease(z->compress_have);
/*@-noeffect@*/
    deflateEnd(&strm);
/*@=noeffect@*/
/*@-mustfreeonly@*/	/* XXX z->compress_head not released */
    return;
/*@=mustfreeonly@*/
}
/*@=nullstate@*/

/* collect the write jobs off of the list in sequence order and write out the
   compressed data until the last chunk is written -- also write the header and
   trailer and combine the individual check values of the input buffers */
static void write_thread(void *_z)
	/*@globals fileSystem, internalState @*/
	/*@modifies _z, fileSystem, internalState @*/
{
    rpmz z = _z;
    long seq;                       /* next sequence number looking for */
    rpmzJob job;                    /* job pulled and working on */
    size_t len;                     /* input length */
    int more;                       /* true if more chunks to write */
    unsigned long head;             /* header length */
    unsigned long ulen;             /* total uncompressed size (overflow ok) */
    unsigned long clen;             /* total compressed size (overflow ok) */
    unsigned long check;            /* check value of uncompressed data */

    /* build and write header */
    Trace((z, "-- write thread running"));
    head = rpmzPutHeader(z);

    /* process output of compress threads until end of input */    
    ulen = clen = 0;
    check = CHECK(0L, Z_NULL, 0);
    seq = 0;
    do {
	/* get next write job in order */
	yarnPossess(z->write_first);
	yarnWaitFor(z->write_first, TO_BE, seq);
	job = z->write_head;
assert(job != NULL);
/*@-dependenttrans@*/
	z->write_head = job->next;
/*@=dependenttrans@*/
	yarnTwist(z->write_first, TO, z->write_head == NULL ? -1 : z->write_head->seq);

	/* update lengths, save uncompressed length for COMB */
	more = job->more;
	len = job->in->len;
	rpmzDropSpace(job->in);
	ulen += (unsigned long)len;
	clen += (unsigned long)(job->out->len);

	/* write the compressed data and drop the output buffer */
	Trace((z, "-- writing #%ld", seq));
	rpmzWrite(z, job->out->buf, job->out->len);
	rpmzDropSpace(job->out);
	Trace((z, "-- wrote #%ld%s", seq, more ? "" : " (last)"));

	/* wait for check calculation to complete, then combine, once
	 * the compress thread is done with the input, release it */
	yarnPossess(job->calc);
	yarnWaitFor(job->calc, TO_BE, 1);
	yarnRelease(job->calc);
	check = COMB(check, job->check, len);

	/* free the job */
	job->calc = yarnFreeLock(job->calc);
/*@-compdestroy@*/
	job = _free(job);
/*@=compdestroy@*/

	/* get the next buffer in sequence */
	seq++;
    } while (more);

    /* write trailer */
    rpmzPutTrailer(z, ulen, clen, check, head);

    /* verify no more jobs, prepare for next use */
    yarnPossess(z->compress_have);
assert(z->compress_head == NULL && yarnPeekLock(z->compress_have) == 0);
    yarnRelease(z->compress_have);
    yarnPossess(z->write_first);
assert(z->write_head == NULL);
    yarnTwist(z->write_first, TO, -1);
/*@-globstate@*/	/* XXX z->write_head is reachable */
    return;
/*@=globstate@*/
}

/* compress z->ifdno to z->ofdno, using multiple threads for the compression and check
   value calculations and one other thread for writing the output -- compress
   threads will be launched and left running (waiting actually) to support
   subsequent calls of rpmzParallelCompress() */
static void rpmzParallelCompress(rpmz z)
	/*@globals fileSystem, internalState @*/
	/*@modifies z, fileSystem, internalState @*/
{
    long seq;                       /* sequence number */
    rpmzSpace prev;                 /* previous input space */
    rpmzSpace next;                 /* next input space */
    rpmzJob job;                    /* job for compress, then write */
    int more;                       /* true if more input to read */

    /* if first time or after an option change, setup the job lists */
    rpmzSetupJobs(z);

    /* start write thread */
/*@-mustfreeonly@*/
    z->writeth = yarnLaunch(write_thread, z);
/*@=mustfreeonly@*/

    /* read from input and start compress threads (write thread will pick up
       the output of the compress threads) */
    seq = 0;
    prev = NULL;
    next = rpmzNewSpace(z->in_pool);
    next->len = rpmzRead(z, next->buf, next->pool->size);
    do {
	/* create a new job, use next input chunk, previous as dictionary */
	job = xmalloc(sizeof(*job));
	job->seq = seq;
/*@-mustfreeonly@*/
	job->in = next;
	job->out = F_ISSET(z->flags, INDEPENDENT) ? prev : NULL;  /* dictionary for compression */
	job->calc = yarnNewLock(0);
/*@=mustfreeonly@*/

	/* check for end of file, reading next chunk if not sure */
	if (next->len < next->pool->size)
	    more = 0;
	else {
	    next = rpmzNewSpace(z->in_pool);
	    next->len = rpmzRead(z, next->buf, next->pool->size);
	    more = next->len != 0;
	    if (!more)
		rpmzDropSpace(next);  /* won't be using it */
	    if (F_ISSET(z->flags, INDEPENDENT) && more) {
		rpmzUseSpace(job->in);    /* hold as dictionary for next loop */
		prev = job->in;
	    }
	}
	job->more = more;
	Trace((z, "-- read #%ld%s", seq, more ? "" : " (last)"));
	if (++seq < 1)
	    bail("input too long: ", z->_ifn);

	/* start another compress thread if needed */
	if (z->cthreads < seq && z->cthreads < z->threads) {
	    (void)yarnLaunch(compress_thread, z);
	    z->cthreads++;
	}

	/* put job at end of compress list, let all the compressors know */
	yarnPossess(z->compress_have);
	job->next = NULL;
	*z->compress_tail = job;
	z->compress_tail = &(job->next);
	yarnTwist(z->compress_have, BY, 1);

	/* do until end of input */
    } while (more);

    /* wait for the write thread to complete (we leave the compress threads out
       there and waiting in case there is another stream to compress) */
    z->writeth = yarnJoin(z->writeth);
    Trace((z, "-- write thread joined"));
}

#endif

/* do a simple compression in a single thread from z->ifdno to z->ofdno -- if reset is
   true, instead free the memory that was allocated and retained for input,
   output, and deflate */
/*@-nullstate@*/
static void rpmzSingleCompress(rpmz z, int reset)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    size_t got;                     /* amount read */
    size_t more;                    /* amount of next read (0 if eof) */
    unsigned long head;             /* header length */
    unsigned long ulen;             /* total uncompressed size (overflow ok) */
    unsigned long clen;             /* total compressed size (overflow ok) */
    unsigned long check;            /* check value of uncompressed data */
    static unsigned out_size;       /* size of output buffer */
/*@only@*/
    static unsigned char *in;       /* reused i/o buffers */
/*@only@*/
    static unsigned char *next;     /* reused i/o buffers */
/*@only@*/
    static unsigned char *out;      /* reused i/o buffers */
/*@only@*/ /*@null@*/
    static z_stream *strm = NULL;   /* reused deflate structure */

    /* if requested, just release the allocations and return */
    if (reset) {
	if (strm != NULL) {
/*@-noeffect@*/
	    deflateEnd(strm);
/*@=noeffect@*/
	    out = _free(out);
	    next = _free(next);
	    in = _free(in);
	    strm = _free(strm);
	}
	return;
    }

    /* initialize the deflate structure if this is the first time */
    if (strm == NULL) {
	out_size = z->blocksize > _PIGZMAX ? _PIGZMAX : (unsigned)z->blocksize;

/*@-mustfreeonly@*/
	in = xmalloc(z->blocksize);
	next = xmalloc(z->blocksize);
	out = xmalloc(out_size);
	strm = xmalloc(sizeof(*strm));
/*@=mustfreeonly@*/

/*@-mustfreeonly@*/
	strm->zfree = Z_NULL;
	strm->zalloc = Z_NULL;
	strm->opaque = Z_NULL;
/*@=mustfreeonly@*/
	if (deflateInit2(strm, z->level, Z_DEFLATED, -15, 8,
		Z_DEFAULT_STRATEGY) != Z_OK)
	    bail("not enough memory", "");
    }

    /* write header */
    head = rpmzPutHeader(z);

    /* set compression level in case it changed */
/*@-noeffect@*/
    (void)deflateReset(strm);
    (void)deflateParams(strm, z->level, Z_DEFAULT_STRATEGY);
/*@=noeffect@*/

    /* do raw deflate and calculate check value */
    ulen = clen = 0;
    check = CHECK(0L, Z_NULL, 0);
    more = rpmzRead(z, next, z->blocksize);
    do {
	/* get data to compress, see if there is any more input */
	got = more;
	{ unsigned char *temp; temp = in; in = next; next = temp; }
	more = got < z->blocksize ? 0 : rpmzRead(z, next, z->blocksize);
	ulen += (unsigned long)got;
/*@-mustfreeonly@*/
	strm->next_in = in;
/*@=mustfreeonly@*/

	/* compress _PIGZMAX-size chunks in case unsigned type is small */
	while (got > _PIGZMAX) {
	    strm->avail_in = _PIGZMAX;
	    check = CHECK(check, strm->next_in, strm->avail_in);
	    do {
		strm->avail_out = out_size;
/*@-mustfreeonly@*/
		strm->next_out = out;
/*@=mustfreeonly@*/
/*@-noeffect@*/
		(void)deflate(strm, Z_NO_FLUSH);
/*@=noeffect@*/
		rpmzWrite(z, out, out_size - strm->avail_out);
		clen += out_size - strm->avail_out;
	    } while (strm->avail_out == 0);
assert(strm->avail_in == 0);
	    got -= _PIGZMAX;
	}

	/* compress the remainder, finishing if end of input -- when not -i,
	 * use a Z_SYNC_FLUSH so that this and parallel compression produce the
	 * same output */
	strm->avail_in = (unsigned)got;
	check = CHECK(check, strm->next_in, strm->avail_in);
	do {
	    strm->avail_out = out_size;
/*@-kepttrans -mustfreeonly@*/
	    strm->next_out = out;
/*@=kepttrans =mustfreeonly@*/
/*@-noeffect@*/
	    (void)deflate(strm,
		more ? (F_ISSET(z->flags, INDEPENDENT) ? Z_SYNC_FLUSH : Z_FULL_FLUSH) : Z_FINISH);
/*@=noeffect@*/
	    rpmzWrite(z, out, out_size - strm->avail_out);
	    clen += out_size - strm->avail_out;
	} while (strm->avail_out == 0);
assert(strm->avail_in == 0);

	/* do until no more input */
    } while (more);

    /* write trailer */
    rpmzPutTrailer(z, ulen, clen, check, head);
}
/*@=nullstate@*/

/*==============================================================*/

/* --- decompression --- */

#ifndef _PIGZNOTHREAD
/* parallel read thread */
static void load_read_thread(void *_z)
	/*@globals fileSystem, internalState @*/
	/*@modifies _z, fileSystem, internalState @*/
{
    rpmz z = _z;
    size_t len;

    Trace((z, "-- launched decompress read thread"));
    do {
	yarnPossess(z->load_state);
	yarnWaitFor(z->load_state, TO_BE, 1);
	z->in_len = len = rpmzRead(z, z->in_which ? z->in_buf : z->in_buf2, z->in_buf_allocated);
	Trace((z, "-- decompress read thread read %lu bytes", len));
	yarnTwist(z->load_state, TO, 0);
    } while (len == z->in_buf_allocated);
    Trace((z, "-- exited decompress read thread"));
}
#endif

/* load() is called when in_left has gone to zero in order to provide more
   input data: load the input buffer with z->in_buf_allocated or less bytes (less if at end of
   file) from the file z->ifdno, set in_next to point to the in_left bytes read,
   update in_tot, and return in_left -- in_eof is set to true when in_left has
   gone to zero and there is no more data left to read from z->ifdno */
static size_t load(rpmz z)
	/*@globals fileSystem, internalState @*/
	/*@modifies z, fileSystem, internalState @*/
{
    /* if already detected end of file, do nothing */
    if (z->in_short) {
	z->in_eof = 1;
	return 0;
    }

#ifndef _PIGZNOTHREAD
    /* if first time in or z->threads == 1, read a buffer to have something to
       return, otherwise wait for the previous read job to complete */
    if (z->threads > 1) {
	/* if first time, fire up the read thread, ask for a read */
	if (z->in_which == -1) {
	    z->in_which = 1;
/*@-mustfreeonly@*/
	    z->load_state = yarnNewLock(1);
	    z->load_thread = yarnLaunch(load_read_thread, z);
/*@=mustfreeonly@*/
	}

	/* wait for the previously requested read to complete */
	yarnPossess(z->load_state);
	yarnWaitFor(z->load_state, TO_BE, 0);
	yarnRelease(z->load_state);

	/* set up input buffer with the data just read */
	z->in_next = z->in_which ? z->in_buf : z->in_buf2;
	z->in_left = z->in_len;

	/* if not at end of file, alert read thread to load next buffer,
	 * alternate between in_buf and in_buf2 */
	if (z->in_len == z->in_buf_allocated) {
	    z->in_which = 1 - z->in_which;
	    yarnPossess(z->load_state);
	    yarnTwist(z->load_state, TO, 1);
	}

	/* at end of file -- join read thread (already exited), clean up */
	else {
	    z->load_thread = yarnJoin(z->load_thread);
	    z->load_state = yarnFreeLock(z->load_state);
	    z->in_which = -1;
	}
    }
    else
#endif
    {
	/* don't use threads -- simply read a buffer into z->in_buf */
	z->in_left = rpmzRead(z, z->in_next = z->in_buf, z->in_buf_allocated);
    }

    /* note end of file */
    if (z->in_left < z->in_buf_allocated) {
	z->in_short = 1;

	/* if we got bupkis, now is the time to mark eof */
	if (z->in_left == 0)
	    z->in_eof = 1;
    }

    /* update the total and return the available bytes */
    z->in_tot += z->in_left;
    return z->in_left;
}

/* initialize for reading new input */
static void in_init(rpmz z)
	/*@modifies z @*/
{
    z->in_left = 0;
    z->in_eof = 0;
    z->in_short = 0;
    z->in_tot = 0;
#ifndef _PIGZNOTHREAD
    z->in_which = -1;
#endif
}

/* buffered reading macros for decompression and listing */
#define GET() (z->in_eof || (z->in_left == 0 && load(z) == 0) ? EOF : \
               (z->in_left--, *z->in_next++))
#define GET2() (tmp2 = GET(), tmp2 + (GET() << 8))
#define GET4() (tmp4 = GET2(), tmp4 + ((unsigned long)(GET2()) << 16))
#define SKIP(dist) \
    do { \
	size_t togo = (dist); \
	while (togo > z->in_left) { \
	    togo -= z->in_left; \
	    if (load(z) == 0) \
		return -1; \
	} \
	z->in_left -= togo; \
	z->in_next += togo; \
    } while (0)

/* convert MS-DOS date and time to a Unix time, assuming current timezone
   (you got a better idea?) */
static time_t dos2time(unsigned long dos)
	/*@*/
{
    struct tm tm;

    if (dos == 0)
	return time(NULL);
    tm.tm_year = ((int)(dos >> 25) & 0x7f) + 80;
    tm.tm_mon  = ((int)(dos >> 21) & 0xf) - 1;
    tm.tm_mday = (int)(dos >> 16) & 0x1f;
    tm.tm_hour = (int)(dos >> 11) & 0x1f;
    tm.tm_min  = (int)(dos >> 5) & 0x3f;
    tm.tm_sec  = (int)(dos << 1) & 0x3e;
    tm.tm_isdst = -1;           /* figure out if DST or not */
    return mktime(&tm);
}

/* convert an unsigned 32-bit integer to signed, even if long > 32 bits */
static long tolong(unsigned long val)
	/*@*/
{
    return (long)(val & 0x7fffffffUL) - (long)(val & 0x80000000UL);
}

#define LOW32 0xffffffffUL

/* process zip extra field to extract zip64 lengths and Unix mod time */
static int rpmzReadExtra(rpmz z, unsigned len, int save)
	/*@globals fileSystem, internalState @*/
	/*@modifies z, fileSystem, internalState @*/
{
    unsigned id;
    unsigned size;
    unsigned tmp2;
    unsigned long tmp4;

    /* process extra blocks */
    while (len >= 4) {
	id = GET2();
	size = GET2();
	if (z->in_eof)
	    return -1;
	len -= 4;
	if (size > len)
	    break;
	len -= size;
	if (id == 0x0001) {
	    /* Zip64 Extended Information Extra Field */
	    if (z->zip_ulen == LOW32 && size >= 8) {
		z->zip_ulen = GET4();
		SKIP(4);
		size -= 8;
	    }
	    if (z->zip_clen == LOW32 && size >= 8) {
		z->zip_clen = GET4();
		SKIP(4);
		size -= 8;
	    }
	}
	if (save) {
	    if ((id == 0x000d || id == 0x5855) && size >= 8) {
		/* PKWare Unix or Info-ZIP Type 1 Unix block */
		SKIP(4);
		z->stamp = tolong(GET4());
		size -= 8;
	    }
	    if (id == 0x5455 && size >= 5) {
		/* Extended Timestamp block */
		size--;
		if (GET() & 1) {
		    z->stamp = tolong(GET4());
		    size -= 4;
		}
	    }
	}
	SKIP(size);
    }
    SKIP(len);
    return 0;
}

/* read a gzip, zip, zlib, or lzw header from z->ifdno and extract useful
   information, return the method -- or on error return negative: -1 is
   immediate EOF, -2 is not a recognized compressed format, -3 is premature EOF
   within the header, -4 is unexpected header flag values; a method of 256 is
   lzw -- set z->format to indicate gzip, zlib, or zip */
static int rpmzGetHeader(rpmz z, int save)
	/*@globals fileSystem, internalState @*/
	/*@modifies z, fileSystem, internalState @*/
{
    unsigned magic;             /* magic header */
    int method;                 /* compression method */
    int flags;                  /* header flags */
    unsigned fname, extra;      /* name and extra field lengths */
    unsigned tmp2;              /* for macro */
    unsigned long tmp4;         /* for macro */

    /* clear return information */
    if (save) {
	z->stamp = 0;
	z->hname = _free(z->hname);
    }

    /* see if it's a gzip, zlib, or lzw file */
    z->format = RPMZ_FORMAT_GZIP;
    magic = GET() << 8;
    if (z->in_eof)
	return -1;
    magic += GET();
    if (z->in_eof)
	return -2;
    if (magic % 31 == 0) {          /* it's zlib */
	z->format = RPMZ_FORMAT_ZLIB;
	return (int)((magic >> 8) & 0xf);
    }
    if (magic == 0x1f9d)            /* it's lzw */
	return 256;
    if (magic == 0x504b) {          /* it's zip */
	if (GET() != 3 || GET() != 4)
	    return -3;
	SKIP(2);
	flags = GET2();
	if (z->in_eof)
	    return -3;
	if (flags & 0xfff0)
	    return -4;
	method = GET2();
	if (flags & 1)              /* encrypted */
	    method = 255;           /* mark as unknown method */
	if (z->in_eof)
	    return -3;
	if (save)
	    z->stamp = dos2time(GET4());
	else
	    SKIP(4);
	z->zip_crc = GET4();
	z->zip_clen = GET4();
	z->zip_ulen = GET4();
	fname = GET2();
	extra = GET2();
	if (save) {
/*@-mustfreeonly@*/
	    char *next = z->hname = xmalloc(fname + 1);
/*@=mustfreeonly@*/
	    while (fname > z->in_left) {
		memcpy(next, z->in_next, z->in_left);
		fname -= z->in_left;
		next += z->in_left;
		if (load(z) == 0)
		    return -3;
	    }
	    memcpy(next, z->in_next, fname);
	    z->in_left -= fname;
	    z->in_next += fname;
	    next += fname;
	    *next = '\0';
	}
	else
	    SKIP(fname);
	rpmzReadExtra(z, extra, save);
	z->format = (flags & 8) ? RPMZ_FORMAT_ZIP3 : RPMZ_FORMAT_ZIP2;
	return z->in_eof ? -3 : method;
    }
    if (magic != 0x1f8b)            /* not gzip */
	return -2;

    /* it's gzip -- get method and flags */
    method = GET();
    flags = GET();
    if (z->in_eof)
	return -1;
    if (flags & 0xe0)
	return -4;

    /* get time stamp */
    if (save)
	z->stamp = tolong(GET4());
    else
	SKIP(4);

    /* skip extra field and OS */
    SKIP(2);

    /* skip extra field, if present */
    if (flags & 4) {
	extra = GET2();
	if (z->in_eof)
	    return -3;
	SKIP(extra);
    }

    /* read file name, if present, into allocated memory */
    if ((flags & 8) && save) {
	unsigned char *end;
	size_t copy, have, size = 128;
/*@-mustfreeonly@*/
	z->hname = xmalloc(size);
/*@=mustfreeonly@*/
	have = 0;
	do {
	    if (z->in_left == 0 && load(z) == 0)
		return -3;
	    end = memchr(z->in_next, 0, z->in_left);
	    copy = end == NULL ? z->in_left : (end - z->in_next) + 1;
	    if (have + copy > size) {
		while (have + copy > (size <<= 1))
		    ;
		z->hname = realloc(z->hname, size);
		if (z->hname == NULL)
		    bail("not enough memory", "");
	    }
	    memcpy(z->hname + have, z->in_next, copy);
	    have += copy;
	    z->in_left -= copy;
	    z->in_next += copy;
	} while (end == NULL);
    }
    else if (flags & 8)
	while (GET() != 0)
	    if (z->in_eof)
		return -3;

    /* skip comment */
    if (flags & 16)
	while (GET() != 0)
	    if (z->in_eof)
		return -3;

    /* skip header crc */
    if (flags & 2)
	SKIP(2);

    /* return compression method */
    return method;
}

/* --- list contents of compressed input (gzip, zlib, or lzw) */

/* find standard compressed file suffix, return length of suffix */
static size_t compressed_suffix(char *nm)
	/*@*/
{
    size_t len;

    len = strlen(nm);
    if (len > 4) {
	nm += len - 4;
	len = 4;
	if (strcmp(nm, ".zip") == 0 || strcmp(nm, ".ZIP") == 0)
	    return 4;
    }
    if (len > 3) {
	nm += len - 3;
	len = 3;
	if (strcmp(nm, ".gz") == 0 || strcmp(nm, "-gz") == 0 ||
	    strcmp(nm, ".zz") == 0 || strcmp(nm, "-zz") == 0)
	    return 3;
    }
    if (len > 2) {
	nm += len - 2;
	if (strcmp(nm, ".z") == 0 || strcmp(nm, "-z") == 0 ||
	    strcmp(nm, "_z") == 0 || strcmp(nm, ".Z") == 0)
	    return 2;
    }
    return 0;
}

/* listing file name lengths for -l and -lv */
#define NAMEMAX1 48     /* name display limit at verbosity 1 */
#define NAMEMAX2 16     /* name display limit at verbosity 2 */

/* print gzip or lzw file information */
static void rpmzShowInfo(rpmz z, int method, unsigned long check, off_t len, int cont)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    static int oneshot = 1; /* true if we need to print listing header */
    int max;                /* maximum name length for current verbosity */
    size_t n;               /* name length without suffix */
    time_t now;             /* for getting current year */
    char mod[26];           /* modification time in text */
    char name[NAMEMAX1+1];  /* header or file name, possibly truncated */

    /* create abbreviated name from header file name or actual file name */
    max = z->verbosity > 1 ? NAMEMAX2 : NAMEMAX1;
    memset(name, 0, max + 1);
    if (cont)
	strncpy(name, "<...>", max + 1);
    else if (z->hname == NULL) {
	n = strlen(z->_ifn) - compressed_suffix(z->_ifn);
	strncpy(name, z->_ifn, n > max + 1 ? max + 1 : n);
    }
    else
	strncpy(name, z->hname, max + 1);
    if (name[max])
	strcpy(name + max - 3, "...");

    /* convert time stamp to text */
    if (z->stamp) {
	strcpy(mod, ctime(&z->stamp));
	now = time(NULL);
/*@-aliasunique@*/
	if (strcmp(mod + 20, ctime(&now) + 20) != 0)
	    strcpy(mod + 11, mod + 19);
/*@=aliasunique@*/
    }
    else
	strcpy(mod + 4, "------ -----");
    mod[16] = 0;

    /* if first time, print header */
    if (oneshot) {
	if (z->verbosity > 1)
	    fputs("method    check    timestamp    ", stdout);
	if (z->verbosity > 0)
	    puts("compressed   original reduced  name");
	oneshot = 0;
    }

    /* print information */
    if (z->verbosity > 1) {
	if (z->format == RPMZ_FORMAT_ZIP3 && z->mode == RPMZ_MODE_COMPRESS)
	    printf("zip%3d  --------  %s  ", method, mod + 4);
	else if (z->format == RPMZ_FORMAT_ZIP2 || z->format == RPMZ_FORMAT_ZIP3)
	    printf("zip%3d  %08lx  %s  ", method, check, mod + 4);
	else if (z->format == RPMZ_FORMAT_ZLIB)
	    printf("zlib%2d  %08lx  %s  ", method, check, mod + 4);
	else if (method == 256)
	    printf("lzw     --------  %s  ", mod + 4);
	else
	    printf("gzip%2d  %08lx  %s  ", method, check, mod + 4);
    }
    if (z->verbosity > 0) {
	if ((z->format == RPMZ_FORMAT_ZIP3 && z->mode == RPMZ_MODE_COMPRESS) ||
	    (method == 8 && z->in_tot > (len + (len >> 10) + 12)) ||
	    (method == 256 && z->in_tot > len + (len >> 1) + 3))
	    printf(sizeof(off_t) == 4 ? "%10lu %10lu?  unk    %s\n" :
                                        "%10llu %10llu?  unk    %s\n",
                   z->in_tot, len, name);
	else
	    printf(sizeof(off_t) == 4 ? "%10lu %10lu %6.1f%%  %s\n" :
                                        "%10llu %10llu %6.1f%%  %s\n",
		    z->in_tot, len,
		    len == 0 ? 0 : 100 * (len - z->in_tot)/(double)len,
		    name);
    }
}

/* list content information about the gzip file at z->ifdno (only works if the gzip
   file contains a single gzip stream with no junk at the end, and only works
   well if the uncompressed length is less than 4 GB) */
static void rpmzListInfo(rpmz z)
	/*@globals fileSystem, internalState @*/
	/*@modifies z, fileSystem, internalState @*/
{
    int method;             /* rpmzGetHeader() return value */
    size_t n;               /* available trailer bytes */
    off_t at;               /* used to calculate compressed length */
    unsigned char tail[8];  /* trailer containing check and length */
    unsigned long check;    /* check value from trailer */
    unsigned long len;      /* check length from trailer */

    /* initialize input buffer */
    in_init(z);

    /* read header information and position input after header */
    method = rpmzGetHeader(z, 1);
    if (method < 0) {
	z->hname = _free(z->hname);
	if (method != -1 && z->verbosity > 1)
	    fprintf(stderr, "%s not a compressed file -- skipping\n", z->_ifn);
	return;
    }

    /* list zip file */
    if (z->format == RPMZ_FORMAT_ZIP2 || z->format == RPMZ_FORMAT_ZIP3) {
	z->in_tot = z->zip_clen;
	rpmzShowInfo(z, method, z->zip_crc, z->zip_ulen, 0);
	return;
    }

    /* list zlib file */
    if (z->format == RPMZ_FORMAT_ZLIB) {
	at = lseek(z->ifdno, 0, SEEK_END);
	if (at == -1) {
	    check = 0;
	    do {
		len = z->in_left < 4 ? z->in_left : 4;
		z->in_next += z->in_left - len;
		while (len--)
		    check = (check << 8) + *z->in_next++;
	    } while (load(z) != 0);
	    check &= LOW32;
	}
	else {
	    z->in_tot = at;
	    lseek(z->ifdno, -4, SEEK_END);
	    rpmzRead(z, tail, 4);
	    check = (*tail << 24) + (tail[1] << 16) + (tail[2] << 8) + tail[3];
	}
	z->in_tot -= 6;
	rpmzShowInfo(z, method, check, 0, 0);
	return;
    }

    /* list lzw file */
    if (method == 256) {
	at = lseek(z->ifdno, 0, SEEK_END);
	if (at == -1)
	    while (load(z) != 0)
		;
	else
	    z->in_tot = at;
	z->in_tot -= 3;
	rpmzShowInfo(z, method, 0, 0, 0);
	return;
    }

    /* skip to end to get trailer (8 bytes), compute compressed length */
    if (z->in_short) {                  /* whole thing already read */
	if (z->in_left < 8) {
	    if (z->verbosity > 0)
		fprintf(stderr, "%s not a valid gzip file -- skipping\n",
			z->_ifn);
	    return;
	}
	z->in_tot = z->in_left - 8;     /* compressed size */
	memcpy(tail, z->in_next + (z->in_left - 8), 8);
    }
    else if ((at = lseek(z->ifdno, -8, SEEK_END)) != -1) {
	z->in_tot = at - z->in_tot + z->in_left; /* compressed size */
	rpmzRead(z, tail, 8);           /* get trailer */
    }
    else {                              /* can't seek */
	at = z->in_tot - z->in_left;    /* save header size */
	do {
	    n = z->in_left < 8 ? z->in_left : 8;
	    memcpy(tail, z->in_next + (z->in_left - n), n);
	    load(z);
	} while (z->in_left == z->in_buf_allocated);       /* read until end */
	if (z->in_left < 8) {
	    if (n + z->in_left < 8) {
		if (z->verbosity > 0)
		    fprintf(stderr, "%s not a valid gzip file -- skipping\n",
				z->_ifn);
		return;
	    }
	    if (z->in_left) {
/*@-aliasunique@*/
		if (n + z->in_left > 8)
		    memcpy(tail, tail + n - (8 - z->in_left), 8 - z->in_left);
/*@=aliasunique@*/
		memcpy(tail + 8 - z->in_left, z->in_next, z->in_left);
	    }
	}
	else
	    memcpy(tail, z->in_next + (z->in_left - 8), 8);
	z->in_tot -= at + 8;
    }
    if (z->in_tot < 2) {
	if (z->verbosity > 0)
	    fprintf(stderr, "%s not a valid gzip file -- skipping\n", z->_ifn);
	return;
    }

    /* convert trailer to check and uncompressed length (modulo 2^32) */
    check = tail[0] + (tail[1] << 8) + (tail[2] << 16) + (tail[3] << 24);
    len = tail[4] + (tail[5] << 8) + (tail[6] << 16) + (tail[7] << 24);

    /* list information about contents */
    rpmzShowInfo(z, method, check, len, 0);
    z->hname = _free(z->hname);
}

/* --- decompress deflate input --- */

/* call-back input function for inflateBack() */
static unsigned inb(void *_z, unsigned char **buf)
	/*@globals fileSystem, internalState @*/
	/*@modifies _z, *buf, fileSystem, internalState @*/
{
    rpmz z = _z;
    load(z);
    *buf = z->in_next;
    return z->in_left;
}

#ifndef	_PIGZNOTHREAD
/* output write thread */
static void outb_write(void *_z)
	/*@globals fileSystem, internalState @*/
	/*@modifies _z, fileSystem, internalState @*/
{
    rpmz z = _z;
    size_t len;

    Trace((z, "-- launched decompress write thread"));
    do {
	yarnPossess(z->outb_write_more);
	yarnWaitFor(z->outb_write_more, TO_BE, 1);
	len = z->out_len;
	if (len && z->mode == RPMZ_MODE_DECOMPRESS)
	    rpmzWrite(z, z->out_copy, len);
	Trace((z, "-- decompress wrote %lu bytes", len));
	yarnTwist(z->outb_write_more, TO, 0);
    } while (len);
    Trace((z, "-- exited decompress write thread"));
}

/* output check thread */
static void outb_check(void *_z)
	/*@globals fileSystem, internalState @*/
	/*@modifies _z, fileSystem, internalState @*/
{
    rpmz z = _z;
    size_t len;

    Trace((z, "-- launched decompress check thread"));
    do {
	yarnPossess(z->outb_check_more);
	yarnWaitFor(z->outb_check_more, TO_BE, 1);
	len = z->out_len;
	z->out_check = CHECK(z->out_check, z->out_copy, len);
	Trace((z, "-- decompress checked %lu bytes", len));
	yarnTwist(z->outb_check_more, TO, 0);
    } while (len);
    Trace((z, "-- exited decompress check thread"));
}
#endif	/* _PIGZNOTHREAD */

/* call-back output function for inflateBack() -- wait for the last write and
   check calculation to complete, copy the write buffer, and then alert the
   write and check threads and return for more decompression while that's
   going on (or just write and check if no threads or if proc == 1) */
static int outb(void *_z, /*@null@*/ unsigned char *buf, unsigned len)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    rpmz z = _z;
#ifndef _PIGZNOTHREAD
/*@only@*/ /*@relnull@*/
    static yarnThread wr;
/*@only@*/ /*@relnull@*/
    static yarnThread ch;

    if (z->threads > 1) {
	/* if first time, initialize state and launch threads */
	if (z->outb_write_more == NULL) {
/*@-mustfreeonly@*/
	    z->outb_write_more = yarnNewLock(0);
	    z->outb_check_more = yarnNewLock(0);
	    wr = yarnLaunch(outb_write, z);
	    ch = yarnLaunch(outb_check, z);
/*@=mustfreeonly@*/
	}

	/* wait for previous write and check threads to complete */
	yarnPossess(z->outb_check_more);
	yarnWaitFor(z->outb_check_more, TO_BE, 0);
	yarnPossess(z->outb_write_more);
	yarnWaitFor(z->outb_write_more, TO_BE, 0);

	/* copy the output and alert the worker bees */
	z->out_len = len;
	z->out_tot += len;
/*@-mayaliasunique@*/
	memcpy(z->out_copy, buf, len);
/*@=mayaliasunique@*/
	yarnTwist(z->outb_write_more, TO, 1);
	yarnTwist(z->outb_check_more, TO, 1);

	/* if requested with len == 0, clean up -- terminate and join write and
	 * check threads, free lock */
	if (len == 0) {
	    ch = yarnJoin(ch);
	    wr = yarnJoin(wr);
	    z->outb_check_more = yarnFreeLock(z->outb_check_more);
	    z->outb_write_more = yarnFreeLock(z->outb_write_more);
	}

	/* return for more decompression while last buffer is being written
	 * and having its check value calculated -- we wait for those to finish
	 * the next time this function is called */
	return 0;
    }
#endif

    /* if just one process or no threads, then do it without threads */
    if (len) {
	if (z->mode == RPMZ_MODE_DECOMPRESS)
	    rpmzWrite(z, buf, len);
	z->out_check = CHECK(z->out_check, buf, len);
	z->out_tot += len;
    }
    return 0;
}

/* inflate for decompression or testing -- decompress from z->ifdno to z->ofdno unless
   z->mode != RPMZ_MODE_DECOMPRESS, in which case just test z->ifdno, and then also list if z->mode == RPMZ_MODE_LIST;
   look for and decode multiple, concatenated gzip and/or zlib streams;
   read and check the gzip, zlib, or zip trailer */
/*@-nullstate@*/
static void rpmzInflateCheck(rpmz z)
	/*@globals fileSystem, internalState @*/
	/*@modifies z, fileSystem, internalState @*/
{
    int ret;
    int cont;
    unsigned long check;
    unsigned long len;
    z_stream strm;
    unsigned tmp2;
    unsigned long tmp4;
    off_t clen;

    cont = 0;
    do {
	/* header already read -- set up for decompression */
	z->in_tot = z->in_left;               /* track compressed data length */
	z->out_tot = 0;
	z->out_check = CHECK(0L, Z_NULL, 0);
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	ret = inflateBackInit(&strm, 15, z->out_buf);
	if (ret != Z_OK)
	    bail("not enough memory", "");

	/* decompress, compute lengths and check value */
	strm.avail_in = z->in_left;
/*@-sharedtrans@*/
	strm.next_in = z->in_next;
/*@=sharedtrans@*/
	ret = inflateBack(&strm, inb, z, outb, z);
	if (ret != Z_STREAM_END)
	    bail("corrupted input -- invalid deflate data: ", z->_ifn);
	z->in_left = strm.avail_in;
/*@-onlytrans@*/
	z->in_next = strm.next_in;
/*@=onlytrans@*/
/*@-noeffect@*/
	inflateBackEnd(&strm);
/*@=noeffect@*/
	outb(z, NULL, 0);        /* finish off final write and check */

	/* compute compressed data length */
	clen = z->in_tot - z->in_left;

	/* read and check trailer */
	switch (z->format) {
	case RPMZ_FORMAT_ZIP3:	/* data descriptor follows */
	    /* read original version of data descriptor*/
	    z->zip_crc = GET4();
	    z->zip_clen = GET4();
	    z->zip_ulen = GET4();
	    if (z->in_eof)
		bail("corrupted zip entry -- missing trailer: ", z->_ifn);

	    /* if crc doesn't match, try info-zip variant with sig */
	    if (z->zip_crc != z->out_check) {
		if (z->zip_crc != 0x08074b50UL || z->zip_clen != z->out_check)
		    bail("corrupted zip entry -- crc32 mismatch: ", z->_ifn);
		z->zip_crc = z->zip_clen;
		z->zip_clen = z->zip_ulen;
		z->zip_ulen = GET4();
	    }

	    /* if second length doesn't match, try 64-bit lengths */
	    if (z->zip_ulen != (z->out_tot & LOW32)) {
		z->zip_ulen = GET4();
		GET4();
	    }
	    if (z->in_eof)
		bail("corrupted zip entry -- missing trailer: ", z->_ifn);
	    /*@fallthrough@*/
	case RPMZ_FORMAT_ZIP2:
	    if (z->zip_clen != (clen & LOW32) || z->zip_ulen != (z->out_tot & LOW32))
		bail("corrupted zip entry -- length mismatch: ", z->_ifn);
	    check = z->zip_crc;
	    break;
	case RPMZ_FORMAT_ZLIB:	/* zlib (big-endian) trailer */
	    check = GET() << 24;
	    check += GET() << 16;
	    check += GET() << 8;
	    check += GET();
	    if (z->in_eof)
		bail("corrupted zlib stream -- missing trailer: ", z->_ifn);
	    if (check != z->out_check)
		bail("corrupted zlib stream -- adler32 mismatch: ", z->_ifn);
	    break;
	case RPMZ_FORMAT_GZIP:	/* gzip trailer */
	    check = GET4();
	    len = GET4();
	    if (z->in_eof)
		bail("corrupted gzip stream -- missing trailer: ", z->_ifn);
	    if (check != z->out_check)
		bail("corrupted gzip stream -- crc32 mismatch: ", z->_ifn);
	    if (len != (z->out_tot & LOW32))
		bail("corrupted gzip stream -- length mismatch: ", z->_ifn);
	    break;
	default:
assert(0);
	    break;
	}

	/* show file information if requested */
	if (F_ISSET(z->flags, LIST)) {
	    z->in_tot = clen;
	    rpmzShowInfo(z, 8, check, z->out_tot, cont);
	    cont = 1;
	}

	/* if a gzip or zlib entry follows a gzip or zlib entry, decompress it
	 * (don't replace saved header information from first entry) */
    } while ((z->format == RPMZ_FORMAT_GZIP || z->format == RPMZ_FORMAT_ZLIB) && (ret = rpmzGetHeader(z, 0)) == 8);
    if (ret != -1 && (z->format == RPMZ_FORMAT_GZIP || z->format == RPMZ_FORMAT_ZLIB))
	fprintf(stderr, "%s OK, has trailing junk which was ignored\n", z->_ifn);
}
/*@=nullstate@*/

/* --- decompress Unix compress (LZW) input --- */

/* throw out what's left in the current bits byte buffer (this is a vestigial
   aspect of the compressed data format derived from an implementation that
   made use of a special VAX machine instruction!) */
#define FLUSHCODE() \
    do { \
	left = 0; \
	rem = 0; \
	if (chunk > z->in_left) { \
	    chunk -= z->in_left; \
	    if (load(z) == 0) \
		break; \
	    if (chunk > z->in_left) { \
		chunk = z->in_left = 0; \
		break; \
	    } \
	} \
	z->in_left -= chunk; \
	z->in_next += chunk; \
	chunk = 0; \
    } while (0)

/* Decompress a compress (LZW) file from z->ifdno to z->ofdno.  The compress magic
   header (two bytes) has already been read and verified. */
static void rpmzDecompressLZW(rpmz z)
	/*@globals fileSystem, internalState @*/
	/*@modifies z, fileSystem, internalState @*/
{
    int got;                    /* byte just read by GET() */
    int chunk;                  /* bytes left in current chunk */
    int left;                   /* bits left in rem */
    unsigned rem;               /* unused bits from input */
    int bits;                   /* current bits per code */
    unsigned code;              /* code, table traversal index */
    unsigned mask;              /* mask for current bits codes */
    int max;                    /* maximum bits per code for this stream */
    int flags;                  /* compress flags, then block compress flag */
    unsigned end;               /* last valid entry in prefix/suffix tables */
    unsigned temp;              /* current code */
    unsigned prev;              /* previous code */
    unsigned final;             /* last character written for previous code */
    unsigned stack;             /* next position for reversed string */
    unsigned outcnt;            /* bytes in output buffer */
    unsigned char *p;

    /* process remainder of compress header -- a flags byte */
    z->out_tot = 0;
    flags = GET();
    if (z->in_eof)
	bail("missing lzw data: ", z->_ifn);
    if (flags & 0x60)
	bail("unknown lzw flags set: ", z->_ifn);
    max = flags & 0x1f;
    if (max < 9 || max > 16)
	bail("lzw bits out of range: ", z->_ifn);
    if (max == 9)                           /* 9 doesn't really mean 9 */
	max = 10;
    flags &= 0x80;                          /* true if block compress */

    /* clear table */
    bits = 9;
    mask = 0x1ff;
    end = flags ? 256 : 255;

    /* set up: get first 9-bit code, which is the first decompressed byte, but
       don't create a table entry until the next code */
    got = GET();
    if (z->in_eof)                          /* no compressed data is ok */
	return;
    final = prev = (unsigned)got;           /* low 8 bits of code */
    got = GET();
    if (z->in_eof || (got & 1) != 0)        /* missing a bit or code >= 256 */
	bail("invalid lzw code: ", z->_ifn);
    rem = (unsigned)got >> 1;               /* remaining 7 bits */
    left = 7;
    chunk = bits - 2;                       /* 7 bytes left in this chunk */
    z->out_buf[0] = (unsigned char)final;   /* write first decompressed byte */
    outcnt = 1;

    /* decode codes */
    stack = 0;
    for (;;) {
	/* if the table will be full after this, increment the code size */
	if (end >= mask && bits < max) {
	    FLUSHCODE();
	    bits++;
	    mask <<= 1;
	    mask++;
	}

	/* get a code of length bits */
	if (chunk == 0)                     /* decrement chunk modulo bits */
	    chunk = bits;
	code = rem;                         /* low bits of code */
	got = GET();
	if (z->in_eof) {                    /* EOF is end of compressed data */
	    /* write remaining buffered output */
	    z->out_tot += outcnt;
	    if (outcnt && z->mode == RPMZ_MODE_DECOMPRESS)
		rpmzWrite(z, z->out_buf, outcnt);
	    return;
	}
	code += (unsigned)got << left;      /* middle (or high) bits of code */
	left += 8;
	chunk--;
	if (bits > left) {                  /* need more bits */
	    got = GET();
	    if (z->in_eof)                  /* can't end in middle of code */
		bail("invalid lzw code: ", z->_ifn);
	    code += (unsigned)got << left;  /* high bits of code */
	    left += 8;
	    chunk--;
	}
	code &= mask;                       /* mask to current code length */
	left -= bits;                       /* number of unused bits */
	rem = (unsigned)got >> (8 - left);  /* unused bits from last byte */

	/* process clear code (256) */
	if (code == 256 && flags) {
	    FLUSHCODE();
	    bits = 9;                       /* initialize bits and mask */
	    mask = 0x1ff;
	    end = 255;                      /* empty table */
	    continue;                       /* get next code */
	}

	/* special code to reuse last match */
	temp = code;                        /* save the current code */
	if (code > end) {
	    /*
	     * Be picky on the allowed code here, and make sure that the code
	     * we drop through (prev) will be a valid index so that random
	     * input does not cause an exception.  The code != end + 1 check is
	     * empirically derived, and not checked in the original uncompress
	     * code.  If this ever causes a problem, that check could be safely
	     * removed.  Leaving this check in greatly improves pigz's ability
	     * to detect random or corrupted input after a compress header.
	     * In any case, the prev > end check must be retained.
	     */
	    if (code != end + 1 || prev > end)
		bail("invalid lzw code: ", z->_ifn);
	    z->_match[stack++] = (unsigned char)final;
	    code = prev;
	}

	/* walk through linked list to generate output in reverse order */
	p = z->_match + stack;
	while (code >= 256) {
	    *p++ = z->_suffix[code];
	    code = z->_prefix[code];
	}
	stack = p - z->_match;
	z->_match[stack++] = (unsigned char)code;
	final = code;

	/* link new table entry */
	if (end < mask) {
	    end++;
	    z->_prefix[end] = (unsigned short)prev;
	    z->_suffix[end] = (unsigned char)final;
	}

	/* set previous code for next iteration */
	prev = temp;

	/* write output in forward order */
	while (stack > z->out_buf_allocated - outcnt) {
	    while (outcnt < z->out_buf_allocated)
		z->out_buf[outcnt++] = z->_match[--stack];
	    z->out_tot += outcnt;
	    if (z->mode == RPMZ_MODE_DECOMPRESS)
		rpmzWrite(z, z->out_buf, outcnt);
	    outcnt = 0;
	}
	p = z->_match + stack;
	do {
	    z->out_buf[outcnt++] = *--p;
	} while (p > z->_match);
	stack = 0;

	/* loop for next code with final and prev as the last match, rem and
	 * left provide the first 0..7 bits of the next code, end is the last
	 * valid table entry */
    }
}

/* --- file processing --- */

/* extract file name from path */
/*@observer@*/
static const char *justname(const char *path)
	/*@*/
{
    const char * p = path + strlen(path);

    while (--p >= path)
	if (*p == '/')
	    break;
    return p + 1;
}

/* Copy file attributes, from -> to, as best we can.  This is best effort, so
   no errors are reported.  The mode bits, including suid, sgid, and the sticky
   bit are copied (if allowed), the owner's user id and group id are copied
   (again if allowed), and the access and modify times are copied. */
static void copymeta(char *from, char *to)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    struct stat sb, *st = &sb;
    struct timeval times[2];
    int xx;

    /* get all of from's Unix meta data, return if not a regular file */
    if (Stat(from, st) != 0 || !S_ISREG(st->st_mode))
	return;

    /* set to's mode bits, ignore errors */
    xx = Chmod(to, st->st_mode & 07777);

    /* copy owner's user and group, ignore errors */
    xx = Chown(to, st->st_uid, st->st_gid);

    /* copy access and modify times, ignore errors */
    times[0].tv_sec = st->st_atime;
    times[0].tv_usec = 0;
    times[1].tv_sec = st->st_mtime;
    times[1].tv_usec = 0;
    xx = Utimes(to, times);
}

/* set the access and modify times of fd to t */
static void touch(char *path, time_t t)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    struct timeval times[2];
    int xx;

    times[0].tv_sec = t;
    times[0].tv_usec = 0;
    times[1].tv_sec = t;
    times[1].tv_usec = 0;
    xx = Utimes(path, times);
}

/* process provided input file, or stdin if path is NULL -- rpmzProcess() can
   call itself for recursive directory processing */
static void rpmzProcess(rpmz z, /*@null@*/ char *path)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies z, fileSystem, internalState @*/
{
    int method = -1;                /* rpmzGetHeader() return value */
    size_t len;                     /* length of base name (minus suffix) */
    struct stat sb, *st = &sb;      /* to get file type and mod time */

    /* open input file with name in, descriptor z->ifdno -- set name and mtime */
    if (path == NULL) {
	strcpy(z->_ifn, "<stdin>");
	z->ifdno = STDIN_FILENO;
/*@-mustfreeonly@*/
	z->name = NULL;
/*@=mustfreeonly@*/
	z->mtime = F_ISSET(z->flags, HTIME) ?
		(fstat(z->ifdno, st) ? time(NULL) : st->st_mtime) : 0;
	len = 0;
    }
    else {
	/* set input file name (already set if recursed here) */
	if (path != z->_ifn) {
/*@-mayaliasunique@*/
	    strncpy(z->_ifn, path, sizeof(z->_ifn));
/*@=mayaliasunique@*/
	    if (z->_ifn[sizeof(z->_ifn) - 1])
		bail("name too long: ", path);
	}
	len = strlen(z->_ifn);

	/* only process regular files, but allow symbolic links if -f,
	 * recurse into directory if -r */
	if (Lstat(z->_ifn, st)) {
#ifdef EOVERFLOW
	    if (errno == EOVERFLOW || errno == EFBIG)
		bail(z->_ifn, " too large -- pigz not compiled with large file support");
#endif
	    if (z->verbosity > 0)
		fprintf(stderr, "%s does not exist -- skipping\n", z->_ifn);
	    return;
	}
	if (!S_ISREG(st->st_mode) && !S_ISLNK(st->st_mode) && !S_ISDIR(st->st_mode)) {
	    if (z->verbosity > 0)
		fprintf(stderr, "%s is a special file or device -- skipping\n", z->_ifn);
	    return;
	}
	if (S_ISLNK(st->st_mode) && !F_ISSET(z->flags, FORCE)) {
	    if (z->verbosity > 0)
		fprintf(stderr, "%s is a symbolic link -- skipping\n", z->_ifn);
	    return;
	}

	/* Recurse into directory */
	if (S_ISDIR(st->st_mode)) {
	    struct dirent * dp;
	    DIR * dir;
	    ARGV_t av = NULL;
	    char * te;
	    int xx;
	    int i;

	    if (!F_ISSET(z->flags, RECURSE)) {
		if (z->verbosity > 0)
		    fprintf(stderr, "%s is a directory -- skipping\n", z->_ifn);
		return;
	    }
	    te = z->_ifn + len;
	    if ((dir = Opendir(z->_ifn)) == NULL)
		return;
	    while ((dp = Readdir(dir)) != NULL) {
		if (dp->d_name[0] == '\0' ||
		   (dp->d_name[0] == '.' && (dp->d_name[1] == '\0' ||
		   (dp->d_name[1] == '.' && dp->d_name[2] == '\0'))))
		    continue;
		xx = argvAdd(&av, dp->d_name);
	    }
	    xx = Closedir(dir);
	    if (av != NULL)
	    for (i = 0; av[i] != NULL; i++) {
		*te = '/';
		strncpy(te+1, av[i], sizeof(z->_ifn) - len - 1);
assert(z->_ifn[sizeof(z->_ifn) - 1] == '\0');
		(void) rpmzProcess(z, z->_ifn);
		*te = '\0';
	    }
	    av = argvFree(av);
	    return;
	}

	/* don't compress .gz (or provided suffix) files, unless -f */
	if (!(F_ISSET(z->flags, FORCE) || F_ISSET(z->flags, LIST) || z->mode != RPMZ_MODE_COMPRESS) && len >= strlen(z->suffix) &&
		strcmp(z->_ifn + len - strlen(z->suffix), z->suffix) == 0) {
	    if (z->verbosity > 0)
		fprintf(stderr, "%s ends with %s -- skipping\n", z->_ifn, z->suffix);
	    return;
	}

	/* only decompress or list files with compressed suffix */
	if (F_ISSET(z->flags, LIST) || z->mode != RPMZ_MODE_COMPRESS) {
	    int suf = compressed_suffix(z->_ifn);
	    if (suf == 0) {
		if (z->verbosity > 0)
		    fprintf(stderr, "%s does not have compressed suffix -- skipping\n",
                            z->_ifn);
		return;
	    }
	    len -= suf;
	}

	/* open input file */
	z->ifdno = open(z->_ifn, O_RDONLY, 0);
	if (z->ifdno < 0)
	    bail("read error on ", z->_ifn);

	/* prepare gzip header information for compression */
/*@-mustfreeonly@*/
	z->name = F_ISSET(z->flags, HNAME) ? justname(z->_ifn) : NULL;
/*@=mustfreeonly@*/
	z->mtime = F_ISSET(z->flags, HTIME) ? st->st_mtime : 0;
    }
    SET_BINARY_MODE(z->ifdno);

    /* if decoding or testing, try to read gzip header */
assert(z->hname == NULL);
    z->hname = _free(z->hname);
    if (z->mode != RPMZ_MODE_COMPRESS) {
	in_init(z);
	method = rpmzGetHeader(z, 1);
	if (method != 8 && method != 256) {
	    z->hname = _free(z->hname);
	    if (z->ifdno != STDIN_FILENO)
		close(z->ifdno);
	    if (method != -1 && z->verbosity > 0)
		fprintf(stderr,
		    method < 0 ? "%s is not compressed -- skipping\n" :
			"%s has unknown compression method -- skipping\n",
			z->_ifn);
	    return;
	}

	/* if requested, test input file (possibly a special list) */
	if (z->mode == RPMZ_MODE_TEST) {
	    if (method == 8)
		rpmzInflateCheck(z);
	    else {
		rpmzDecompressLZW(z);
		if (F_ISSET(z->flags, LIST)) {
		    z->in_tot -= 3;
		    rpmzShowInfo(z, method, 0, z->out_tot, 0);
		}
	    }
	    z->hname = _free(z->hname);
	    if (z->ifdno != STDIN_FILENO)
		close(z->ifdno);
	    return;
	}
    }

    /* if requested, just list information about input file */
    if (F_ISSET(z->flags, LIST)) {
	rpmzListInfo(z);
	z->hname = _free(z->hname);
	if (z->ifdno != STDIN_FILENO)
	    close(z->ifdno);
	return;
    }

    /* create output file out, descriptor z->ofdno */
    if (path == NULL || F_ISSET(z->flags, STDOUT)) {
	/* write to stdout */
/*@-mustfreeonly@*/
	z->_ofn = xmalloc(strlen("<stdout>") + 1);
/*@=mustfreeonly@*/
	strcpy(z->_ofn, "<stdout>");
	z->ofdno = STDOUT_FILENO;
	if (z->mode == RPMZ_MODE_COMPRESS && !F_ISSET(z->flags, FORCE) && isatty(z->ofdno))
	    bail("trying to write compressed data to a terminal",
		" (use -f to force)");
    }
    else {
	char *to;

	/* use header name for output when decompressing with -N */
	to = z->_ifn;
	if (z->mode != RPMZ_MODE_COMPRESS && F_ISSET(z->flags, HNAME) && z->hname != NULL) {
	    to = z->hname;
	    len = strlen(z->hname);
	}

	/* create output file and open to write */
/*@-mustfreeonly@*/
	z->_ofn = xmalloc(len + (z->mode != RPMZ_MODE_COMPRESS ? 0 : strlen(z->suffix)) + 1);
/*@=mustfreeonly@*/
	memcpy(z->_ofn, to, len);
	strcpy(z->_ofn + len, z->mode != RPMZ_MODE_COMPRESS ? "" : z->suffix);
	z->ofdno = open(z->_ofn, O_CREAT | O_TRUNC | O_WRONLY |
                         (F_ISSET(z->flags, FORCE) ? 0 : O_EXCL), 0666);

	/* if exists and not -f, give user a chance to overwrite */
	if (z->ofdno < 0 && errno == EEXIST && isatty(0) && z->verbosity) {
	    int ch, reply;

	    fprintf(stderr, "%s exists -- overwrite (y/n)? ", z->_ofn);
	    fflush(stderr);
	    reply = -1;
	    do {
		ch = getchar();
		if (reply < 0 && ch != ' ' && ch != '\t')
		    reply = ch == 'y' || ch == 'Y' ? 1 : 0;
	    } while (ch != EOF && ch != '\n' && ch != '\r');
	    if (reply == 1)
		z->ofdno = open(z->_ofn, O_CREAT | O_TRUNC | O_WRONLY, 0666);
	}

	/* if exists and no overwrite, report and go on to next */
	if (z->ofdno < 0 && errno == EEXIST) {
	    if (z->verbosity > 0)
		fprintf(stderr, "%s exists -- skipping\n", z->_ofn);
	    z->_ofn = _free(z->_ofn);
	    z->hname = _free(z->hname);
	    if (z->ifdno != STDIN_FILENO)
		close(z->ifdno);
	    return;
	}

	/* if some other error, give up */
	if (z->ofdno < 0)
	    bail("write error on ", z->_ofn);
    }
    SET_BINARY_MODE(z->ofdno);
    z->hname = _free(z->hname);

    /* process z->ifdno to z->ofdno */
    if (z->verbosity > 1)
	fprintf(stderr, "%s to %s ", z->_ifn, z->_ofn);
    if (z->mode != RPMZ_MODE_COMPRESS) {
	if (method == 8)
	    rpmzInflateCheck(z);
	else
	    rpmzDecompressLZW(z);
    }
#ifndef _PIGZNOTHREAD
    else if (z->threads > 1)
	rpmzParallelCompress(z);
#endif
    else
	rpmzSingleCompress(z, 0);
    if (z->verbosity > 1) {
	putc('\n', stderr);
	fflush(stderr);
    }

    /* finish up, copy attributes, set times, delete original */
    if (z->ifdno != STDIN_FILENO)
	close(z->ifdno);
    if (z->ofdno != STDOUT_FILENO) {
	if (close(z->ofdno))
	    bail("write error on ", z->_ofn);
	z->ofdno = -1;              /* now prevent deletion on interrupt */
	if (z->ifdno != STDIN_FILENO) {
	    copymeta(z->_ifn, z->_ofn);
	    if (!F_ISSET(z->flags, KEEP))
		Unlink(z->_ifn);
	}
	if (z->mode != RPMZ_MODE_COMPRESS && F_ISSET(z->flags, HTIME) && z->stamp)
	    touch(z->_ofn, z->stamp);
    }
    z->_ofn = _free(z->_ofn);
}

/*==============================================================*/

/* either new buffer size, new compression level, or new number of processes --
   get rid of old buffers and threads to force the creation of new ones with
   the new settings */
static void rpmzNewOpts(rpmz z)
	/*@globals fileSystem, internalState @*/
	/*@modifies z, fileSystem, internalState @*/
{
    rpmzSingleCompress(z, 1);
#ifndef _PIGZNOTHREAD
    rpmzFinishJobs(z);
#endif
}

/* catch termination signal */
/*@exits@*/
static void rpmzAbort(/*@unused@*/ int sig)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    rpmz z = _rpmz;

    Trace((z, "termination by user"));
    if (z->ofdno != -1 && z->_ofn != NULL)
	Unlink(z->_ofn);
    rpmzLogDump(z);
    _exit(EXIT_FAILURE);
}

/**
 */
static void rpmzArgCallback(poptContext con,
		/*@unused@*/ enum poptCallbackReason reason,
		const struct poptOption * opt, /*@unused@*/ const char * arg,
		/*@unused@*/ void * data)
	/*@globals _rpmz, fileSystem, internalState @*/
	/*@modifies _rpmz, fileSystem, internalState @*/
{
    rpmz z = _rpmz;

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
    case 'L':
	fputs(PIGZVERSION, stderr);
	fputs("Copyright (C) 2007 Mark Adler\n", stderr);
	fputs("Subject to the terms of the zlib license.\n", stderr);
	fputs("No warranty is provided or implied.\n", stderr);
	exit(EXIT_SUCCESS);
	/*@notreached@*/ break;
    case 'V':	fputs(PIGZVERSION, stderr); exit(EXIT_SUCCESS);
    case 'Z':	bail("invalid option: LZW output not supported", "");
    case 'a':	bail("invalid option: ascii conversion not supported", "");
    case 'b':
	z->blocksize = (size_t)(atol(arg)) << 10;   /* chunk size */
	if (z->blocksize < _PIGZDICT)
	    bail("block size too small (must be >= 32K)", "");
	if (z->blocksize + (z->blocksize >> 11) + 10 < (z->blocksize >> 11) + 10 ||
	    (ssize_t)(z->blocksize + (z->blocksize >> 11) + 10) < 0)
	    bail("block size too large", "");
	rpmzNewOpts(z);
	break;
    case 'p':
	z->threads = atoi(arg);                  /* # processes */
	if (z->threads < 1)
	    bail("need at least one process", "");
	if ((2 + (z->threads << 1)) < 1)
	    bail("too many processes", "");
#ifdef _PIGZNOTHREAD
	if (z->threads > 1)
	    bail("this pigz compiled without threads", "");
#endif
	rpmzNewOpts(z);
	break;
    case 'q':	z->verbosity = 0; break;
    case 'v':	z->verbosity++; break;
    default:
	/* XXX really need to display longName/shortName instead. */
	fprintf(stderr, _("Unknown option -%c\n"), (char)opt->val);
	poptPrintUsage(con, stderr, 0);
	/*@-exitarg@*/ exit(2); /*@=exitarg@*/
	/*@notreached@*/ break;
    }
}

/*==============================================================*/

/*@unchecked@*/ /*@observer@*/
static struct poptOption rpmzOptionsPoptTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
	rpmzArgCallback, 0, NULL, NULL },
/*@=type@*/

#ifdef	REFERENCE
Usage: pigz [options] [files ...]
  will compress files in place, adding the suffix '.gz'.  If no files are
  specified, stdin will be compressed to stdout.  pigz does what gzip does,
  but spreads the work over multiple processors and cores when compressing.

Options:
  -0 to -9, --fast, --best   Compression levels, --fast is -1, --best is -9
  -b, --blocksize mmm  Set compression block size to mmmK (default 128K)
  -p, --processes n    Allow up to n compression threads (default 8)
  -i, --independent    Compress blocks independently for damage recovery
  -R, --rsyncable      Input-determined block locations for rsync
  -d, --decompress     Decompress the compressed input
  -t, --test           Test the integrity of the compressed input
  -l, --list           List the contents of the compressed input
  -f, --force          Force overwrite, compress .gz, links, and to terminal
  -r, --recursive      Process the contents of all subdirectories
  -s, --suffix .sss    Use suffix .sss instead of .gz (for compression)
  -z, --zlib           Compress to zlib (.zz) instead of gzip format
  -K, --zip            Compress to PKWare zip (.zip) single entry format
  -k, --keep           Do not delete original file after processing
  -c, --stdout         Write all processed output to stdout (wont delete)
  -N, --name           Store/restore file name and mod time in/from header
  -n, --no-name        Do not store or restore file name in/from header
  -T, --no-time        Do not store or restore mod time in/from header
  -q, --quiet          Print no messages, even on error
  -v, --verbose        Provide more verbose output
#endif

  { "fast", '\0', POPT_ARG_VAL,				&__rpmz.level,  1,
	N_("fast compression"), NULL },
  { "best", '\0', POPT_ARG_VAL,				&__rpmz.level,  9,
	N_("best compression"), NULL },
  { NULL, '0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmz.level,  0,
	NULL, NULL },
  { NULL, '1', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmz.level,  1,
	NULL, NULL },
  { NULL, '2', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmz.level,  2,
	NULL, NULL },
  { NULL, '3', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmz.level,  3,
	NULL, NULL },
  { NULL, '4', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmz.level,  4,
	NULL, NULL },
  { NULL, '5', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmz.level,  5,
	NULL, NULL },
  { NULL, '6', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmz.level,  6,
	NULL, NULL },
  { NULL, '7', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmz.level,  7,
	NULL, NULL },
  { NULL, '8', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmz.level,  8,
	NULL, NULL },
  { NULL, '9', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmz.level,  9,
	NULL, NULL },

#ifdef	NOTYET	/* XXX --blocksize/--processes validate arg */
  { "blocksize", 'b', POPT_ARG_INT|POPT_ARGFLAG_SHOW_DEFAULT,	&__rpmz.blocksize, 0,
	N_("Set compression block size to mmmK"), N_("mmm") },
  /* XXX same as --threads */
  { "processes", 'p', POPT_ARG_INT|POPT_ARGFLAG_SHOW_DEFAULT,	&__rpmz.threads, 0,
	N_("Allow up to n compression threads"), N_("n") },
#else
  { "blocksize", 'b', POPT_ARG_VAL|POPT_ARGFLAG_SHOW_DEFAULT,	NULL, 'b',
	N_("Set compression block size to mmmK"), N_("mmm") },
  /* XXX same as --threads */
  { "processes", 'p', POPT_ARG_INT|POPT_ARGFLAG_SHOW_DEFAULT,	NULL, 'p',
	N_("Allow up to n compression threads"), N_("n") },
#endif
  { "independent", 'i', POPT_BIT_SET|POPT_ARGFLAG_TOGGLE,	&__rpmz.flags, RPMZ_FLAGS_INDEPENDENT,
	N_("Compress blocks independently for damage recovery"), NULL },
  { "rsyncable", 'R', POPT_BIT_SET|POPT_ARGFLAG_TOGGLE,		&__rpmz.flags, RPMZ_FLAGS_RSYNCABLE,
	N_("Input-determined block locations for rsync"), NULL },

  /* ===== Operation modes */
#ifdef	NOTYET
  { "compress", 'z', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmz.mode, RPMZ_MODE_COMPRESS,
	N_("force compression"), NULL },
#endif
  { "decompress", 'd', POPT_ARG_VAL,		&__rpmz.mode, RPMZ_MODE_DECOMPRESS,
	N_("Decompress the compressed input"), NULL },
  { "test", 't', POPT_ARG_VAL,			&__rpmz.mode,  RPMZ_MODE_TEST,
	N_("Test the integrity of the compressed input"), NULL },
  { "list", 'l', POPT_BIT_SET,			&__rpmz.flags,  RPMZ_FLAGS_LIST,
	N_("List the contents of the compressed input"), NULL },
  { "force", 'f', POPT_BIT_SET,			&__rpmz.flags,  RPMZ_FLAGS_FORCE,
	N_("Force overwrite, compress .gz, links, and to terminal"), NULL },

  /* ===== Operation modifiers */
  /* XXX unimplemented */
  { "recursive", 'r', POPT_BIT_SET,	&__rpmz.flags, RPMZ_FLAGS_RECURSE,
	N_("Process the contents of all subdirectories"), NULL },
  { "suffix", 'S', POPT_ARG_STRING,		&__rpmz.suffix, 0,
	N_("Use suffix .sss instead of .gz (for compression)"), N_(".sss") },
  { "ascii", 'a', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	NULL, 'a',
	N_("Compress to LZW (.Z) instead of gzip format"), NULL },
  { "bits", 'Z', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	NULL, 'Z',
	N_("Compress to LZW (.Z) instead of gzip format"), NULL },
  { "zlib", 'z', POPT_ARG_VAL,		&__rpmz.format, RPMZ_FORMAT_ZLIB,
	N_("Compress to zlib (.zz) instead of gzip format"), NULL },
  { "zip", 'K', POPT_ARG_VAL,		&__rpmz.format, RPMZ_FORMAT_ZIP2,
	N_("Compress to PKWare zip (.zip) single entry format"), NULL },
  { "keep", 'k', POPT_BIT_SET,			&__rpmz.flags, RPMZ_FLAGS_KEEP,
	N_("Do not delete original file after processing"), NULL },
  { "stdout", 'c', POPT_BIT_SET,		&__rpmz.flags,  RPMZ_FLAGS_STDOUT,
	N_("Write all processed output to stdout (won't delete)"), NULL },
  { "to-stdout", 'c', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN, &__rpmz.flags,  RPMZ_FLAGS_STDOUT,
	N_("write to standard output and don't delete input files"), NULL },

  /* ===== Metadata options */
  /* XXX logic is reversed, disablers should clear with toggle. */
  { "name", 'N', POPT_BIT_SET,		&__rpmz.flags, (RPMZ_FLAGS_HNAME|RPMZ_FLAGS_HTIME),
	N_("Store/restore file name and mod time in/from header"), NULL },
  { "no-name", 'n', POPT_BIT_CLR,	&__rpmz.flags, RPMZ_FLAGS_HNAME,
	N_("Do not store or restore file name in/from header"), NULL },
  /* XXX -T collides with xz -T,--threads */
  { "no-time", 'T', POPT_BIT_CLR,	&__rpmz.flags, RPMZ_FLAGS_HTIME,
	N_("Do not store or restore mod time in/from header"), NULL },

  /* ===== Other options */
  { "quiet", 'q',	POPT_ARG_VAL,				NULL,  'q',
	N_("Print no messages, even on error"), NULL },
  { "verbose", 'v',	POPT_ARG_VAL,				NULL,  'v',
	N_("Provide more verbose output"), NULL },
  { "version", 'V',	POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	NULL,  'V',
	N_("?version?"), NULL },
  { "license", 'L',	POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	NULL,  'L',
	N_("?license?"), NULL },

  POPT_TABLEEND

};
/*@unchecked@*/ /*@observer@*/
static struct poptOption optionsTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
	rpmzArgCallback, 0, NULL, NULL },
/*@=type@*/

  { "debug", 'D', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_debug, -1,
	N_("debug spewage"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmzOptionsPoptTable, 0,
        N_("\
  will compress files in place, adding the suffix '.gz'.  If no files are\n\
  specified, stdin will be compressed to stdout.  pigz does what gzip does,\n\
  but spreads the work over multiple processors and cores when compressing.\n\
\n\
Options:\
"), NULL },

#ifdef	NOTYET
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"),
	NULL },
#endif

  POPT_AUTOALIAS
  POPT_AUTOHELP

  { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
	N_("\
Usage: pigz [options] [files ...]\n\
  will compress files in place, adding the suffix '.gz'.  If no files are\n\
  specified, stdin will be compressed to stdout.  pigz does what gzip does,\n\
  but spreads the work over multiple processors and cores when compressing.\n\
"), NULL },

  POPT_TABLEEND

};

/**
 */
static rpmRC rpmzParseEnv(/*@unused@*/ rpmz z, /*@null@*/ const char * envvar)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    static char whitespace[] = " \f\n\r\t\v,";
    static char _envvar[] = "RPMZ";
    char * s = getenv((envvar ? envvar : _envvar));
    ARGV_t av = NULL;
    poptContext optCon = NULL;
    rpmRC rc = RPMRC_OK;
    int ac;
    int xx;

    if (s == NULL)
	goto exit;

    /* XXX todo: argvSplit() assumes single separator between args. */
/*@-nullstate@*/
    xx = argvSplit(&av, s, whitespace);
/*@=nullstate@*/
    ac = argvCount(av);
    if (ac < 1)
	goto exit;

    optCon = poptGetContext(__progname, ac, (const char **)av,
		optionsTable, POPT_CONTEXT_KEEP_FIRST);

    /* Process all options, whine if unknown. */
    while ((xx = poptGetNextOpt(optCon)) > 0) {
	const char * optArg = poptGetOptArg(optCon);
/*@-dependenttrans -modobserver -observertrans @*/
	optArg = _free(optArg);
/*@=dependenttrans =modobserver =observertrans @*/
	switch (xx) {
	default:
/*@-nullpass@*/
	    fprintf(stderr, _("%s: option table misconfigured (%d)\n"),
		__progname, xx);
/*@=nullpass@*/
	    rc = RPMRC_FAIL;
	    goto exit;
	    /*@notreached@*/ /*@switchbreak@*/ break;
        }
    }

    if (xx < -1) {
/*@-nullpass@*/
	fprintf(stderr, "%s: %s: %s\n", __progname,
		poptBadOption(optCon, POPT_BADOPTION_NOALIAS),
		poptStrerror(xx));
/*@=nullpass@*/
	rc = RPMRC_FAIL;
    }

    /* Check that only options were in the envvar. */
    if (argvCount(poptGetArgs(optCon)))
	bail("cannot provide files in GZIP environment variable", "");

exit:
    if (optCon)
	optCon = poptFreeContext(optCon);
    av = argvFree(av);
    return rc;
}

#ifdef	NOTYET
/**
 */
static rpmRC rpmzParseArgv0(rpmz z, /*@null@*/ const char * argv0)
	/*@*/
{
    const char * s = strrchr(argv0, '/');
    const char * name = (s ? (s + 1) : argv0);
    rpmRC rc = RPMRC_OK;

#if defined(WITH_XZ)
    if (strstr(name, "xz") != NULL) {
	z->_format_compress_auto = RPMZ_FORMAT_XZ;
	z->format = RPMZ_FORMAT_XZ;	/* XXX eliminate */
    } else
    if (strstr(name, "lz") != NULL) {
	z->_format_compress_auto = RPMZ_FORMAT_LZMA;
	z->format = RPMZ_FORMAT_LZMA;	/* XXX eliminate */
    } else
#endif	/* WITH_XZ */
#if defined(WITH_BZIP2)
    if (strstr(name, "bz") != NULL) {
	z->_format_compress_auto = RPMZ_FORMAT_BZIP2;
	z->format = RPMZ_FORMAT_BZIP2;	/* XXX eliminate */
    } else
#endif	/* WITH_BZIP2 */
#if defined(WITH_ZLIB)
    if (strstr(name, "gz") != NULL) {
	z->_format_compress_auto = RPMZ_FORMAT_GZIP;
	z->format = RPMZ_FORMAT_GZIP;	/* XXX eliminate */
    } else
    if (strstr(name, "zlib") != NULL) {
	z->_format_compress_auto = RPMZ_FORMAT_ZLIB;
	z->format = RPMZ_FORMAT_ZLIB;	/* XXX eliminate */
    } else
	/* XXX watchout for "bzip2" matching */
    if (strstr(name, "zip") != NULL) {
	z->_format_compress_auto = RPMZ_FORMAT_ZIP2;
	z->format = RPMZ_FORMAT_ZIP2;	/* XXX eliminate */
    } else
#endif	/* WITH_ZLIB */
    {
	z->_format_compress_auto = RPMZ_FORMAT_AUTO;
	z->format = RPMZ_FORMAT_AUTO;	/* XXX eliminate */
    }

    if (strstr(name, "cat") != NULL) {
	z->mode = RPMZ_MODE_DECOMPRESS;
	z->flags |= RPMZ_FLAGS_STDOUT;
    } else if (strstr(name, "un") != NULL) {
	z->mode = RPMZ_MODE_DECOMPRESS;
    }

    return rc;
}
#endif	/* NOTYET */

/* Process arguments, compress in the gzip format.  Note that z->threads must be at
   least two in order to provide a dictionary in one work unit for the other
   work unit, and that size must be at least 32K to store a full dictionary. */
int main(int argc, char **argv)
	/*@globals __progname, yarnPrefix, h_errno, rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@modifies __progname, yarnPrefix, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    rpmz z = _rpmz;
    poptContext optCon;
    const char ** av;
    int ac;
    int rc = 1;		/* assume failure */
    int i;

/*@-observertrans -readonlytrans @*/
    __progname = "rpmpigz";
/*@=observertrans =readonlytrans @*/

    /* prepare for interrupts and logging */
    signal(SIGINT, rpmzAbort);

#ifndef _PIGZNOTHREAD
    yarnPrefix = "rpmpigz";         /* prefix for yarn error messages */
    yarnAbort = rpmzAbort;          /* call on thread error */
#endif
#if defined(DEBUG) || defined(__LCLINT__)
    gettimeofday(&z->start, NULL);  /* starting time for log entries */
    rpmzLogInit(z);                     /* initialize logging */
#endif

    /* set all options to defaults */
    rpmzDefaults(z);

    optCon = rpmioInit(argc, argv, optionsTable);

    /* process user environment variable defaults */
    if (rpmzParseEnv(z, "GZIP"))
        goto exit;

    /* if no command line arguments and stdout is a terminal, show help */
    av = poptGetArgs(optCon);
    if ((av == NULL || av[0] == NULL) && isatty(STDOUT_FILENO)) {
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }
    ac = argvCount(av);

    /* process command-line arguments */
    if (av == NULL || av[0] == NULL) {
	/* list stdin or compress stdin to stdout if no file names provided */
	rpmzProcess(z, NULL);
    } else {
	for (i = 0; i < ac; i++) {
	    if (i == 1 && F_ISSET(z->flags, STDOUT) && z->mode == RPMZ_MODE_COMPRESS && !F_ISSET(z->flags, LIST) && (z->format == RPMZ_FORMAT_ZIP2 || z->format == RPMZ_FORMAT_ZIP3)) {
		fprintf(stderr, "warning: output is concatenated zip files ");
		fprintf(stderr, "-- pigz will not be able to extract\n");
	    }
	    rpmzProcess(z, strcmp(av[i], "-") ? (char *)av[i] : NULL);
	}
    }

    rc = 0;

    /* done -- release resources, show log */
exit:
    rpmzNewOpts(z);
    rpmzLogDump(z);

    optCon = rpmioFini(optCon);
    
/*@-observertrans@*/	/* XXX __progname silliness */
    return rc;
/*@=observertrans@*/
}

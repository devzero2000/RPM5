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

#define	_RPMIOB_INTERNAL
#define	_RPMZQ_INTERNAL
#define	_RPMZ_INTERNAL
#define	_RPMZ_INTERNAL_PIGZ
#include "rpmz.h"

#include "debug.h"

#if defined(DEBUG) || defined(__LCLINT__)
#define Trace(x) \
    do { \
	if (zq->verbosity > 2) { \
	    rpmzLogAdd x; \
	} \
    } while (0)
#else	/* !DEBUG */
#define rpmzLogDump(_zlog, _fp)
#define Trace(x)
#endif	/* DEBUG */

/*@unchecked@*/
static int _debug = 0;

#define F_ISSET(_f, _FLAG) (((_f) & ((RPMZ_FLAGS_##_FLAG) & ~0x40000000)) != RPMZ_FLAGS_NONE)

/*@-fullinitblock@*/
/*@unchecked@*/
struct rpmz_s __rpmz = {
    .stdin_fn	= "<stdin>",
    .stdout_fn	= "<stdout>",

    .in_buf_allocated = IN_BUF_ALLOCATED,
    .out_buf_allocated = OUT_BUF_ALLOCATED,

};
/*@=fullinitblock@*/

/* set option defaults */
static void rpmzDefaults(rpmzQueue zq)
	/*@modifies z @*/
{
    zq->suffix = ".gz";               /* compressed file suffix */
    zq->ifn = NULL;
    zq->ifdno = -1;
    zq->ofn = NULL;
    zq->ofdno = -1;

    zq->level = Z_DEFAULT_COMPRESSION;	/* XXX level is format specific. */
#ifdef _PIGZNOTHREAD
    zq->threads = 1;
#else
    zq->threads = 8;
#endif
    zq->verbosity = 1;              /* normal message level */
    zq->blocksize = 131072UL;
    zq->flags &= ~RPMZ_FLAGS_RSYNCABLE; /* don't do rsync blocking */
  /* XXX logic is reversed, disablers should clear with toggle. */
    zq->flags |= RPMZ_FLAGS_INDEPENDENT; /* initialize dictionary each thread */
    zq->flags |= (RPMZ_FLAGS_HNAME|RPMZ_FLAGS_HTIME); /* store/restore name and timestamp */
    zq->flags &= ~RPMZ_FLAGS_STDOUT; /* don't force output to stdout */
    zq->mode = RPMZ_MODE_COMPRESS;   /* compress */
    zq->flags &= ~RPMZ_FLAGS_LIST;   /* compress */
    zq->flags &= ~RPMZ_FLAGS_KEEP;   /* delete input file once compressed */
    zq->flags &= ~RPMZ_FLAGS_FORCE;  /* don't overwrite, don't compress links */
    zq->flags &= ~RPMZ_FLAGS_RECURSE;/* don't go into directories */
    zq->format = RPMZ_FORMAT_GZIP;   /* use gzip format */
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
static int bail(const char *why, const char *what)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    rpmzQueue zq = _rpmz->zq;

/*@-globs@*/
    if (zq->ofdno != -1 && zq->ofn != NULL)
	Unlink(zq->ofn);
/*@=globs@*/
    if (zq->verbosity > 0)
	fprintf(stderr, "%s abort: %s%s\n", "pigz", why, what);
    exit(EXIT_FAILURE);
/*@notreached@*/
    return 0;
}

/*==============================================================*/

/* read up to len bytes into buf, repeating read() calls as needed */
static size_t rpmzRead(rpmzQueue zq, unsigned char *buf, size_t len)
	/*@globals fileSystem, internalState @*/
	/*@modifies *buf, fileSystem, internalState @*/
{
    int fdno = zq->ifdno;
    ssize_t ret;
    size_t got;

    got = 0;
    while (len) {
	ret = read(fdno, buf, len);
	if (ret < 0)
	    bail("read error on ", zq->ifn);
	if (ret == 0)
	    break;
	buf += ret;
	len -= ret;
	got += ret;
    }
    return got;
}

/* write len bytes, repeating write() calls as needed */
static void rpmzWrite(rpmzQueue zq, unsigned char *buf, size_t len)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    int fdno = zq->ofdno;
    ssize_t ret;

    while (len) {
	ret = write(fdno, buf, len);
	if (ret < 1)
	    fprintf(stderr, "write error code %d\n", errno);
	if (ret < 1)
	    bail("write error on ", zq->ofn);
	buf += ret;
	len -= ret;
    }
}

/* sliding dictionary size for deflate */
#define _PIGZDICT 32768U

/* largest power of 2 that fits in an unsigned int -- used to limit requests
   to zlib functions that use unsigned int lengths */
#define _PIGZMAX ((((unsigned)0 - 1) >> 1) + 1)

typedef	struct rpmzh_s * rpmzh;
struct rpmzh_s {
    const char * name;		/*!< name for gzip header */
    time_t mtime;		/*!< time stamp for gzip header */
    unsigned long head;		/*!< header length */
    unsigned long ulen;		/*!< total uncompressed size (overflow ok) */
    unsigned long clen;		/*!< total compressed size (overflow ok) */
    unsigned long check;	/*!< check value of uncompressed data */
};

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
static unsigned long rpmzPutHeader(rpmzQueue zq, rpmzh zh)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    unsigned long len;
    unsigned char head[30];

    switch (zq->format) {
    case RPMZ_FORMAT_ZIP2:	/* zip */
    case RPMZ_FORMAT_ZIP3:
	/* write local header */
	PUT4L(head, 0x04034b50UL);  /* local header signature */
	PUT2L(head + 4, 20);        /* version needed to extract (2.0) */
	PUT2L(head + 6, 8);         /* flags: data descriptor follows data */
	PUT2L(head + 8, 8);         /* deflate */
	PUT4L(head + 10, time2dos(zh->mtime));
	PUT4L(head + 14, 0);        /* crc (not here) */
	PUT4L(head + 18, 0);        /* compressed length (not here) */
	PUT4L(head + 22, 0);        /* uncompressed length (not here) */
	PUT2L(head + 26, zh->name == NULL ? 1 : strlen(zh->name));  /* name length */
	PUT2L(head + 28, 9);        /* length of extra field (see below) */
	rpmzWrite(zq, head, 30);    /* write local header */
	len = 30;

	/* write file name (use "-" for stdin) */
	if (zh->name == NULL)
	    rpmzWrite(zq, (unsigned char *)"-", 1);
	else
	    rpmzWrite(zq, (unsigned char *)zh->name, strlen(zh->name));
	len += zh->name == NULL ? 1 : strlen(zh->name);

	/* write extended timestamp extra field block (9 bytes) */
	PUT2L(head, 0x5455);        /* extended timestamp signature */
	PUT2L(head + 2, 5);         /* number of data bytes in this block */
	head[4] = 1;                /* flag presence of mod time */
	PUT4L(head + 5, zh->mtime); /* mod time */
	rpmzWrite(zq, head, 9);     /* write extra field block */
	len += 9;
	break;
    case RPMZ_FORMAT_ZLIB:	/* zlib */
	head[0] = 0x78;             /* deflate, 32K window */
	head[1] = (zq->level == 9U ? 3 : (zq->level == 1U ? 0 :
	    (zq->level >= 6U || zq->level == (unsigned)Z_DEFAULT_COMPRESSION ? 1 :  2))) << 6;
	head[1] += 31 - (((head[0] << 8) + head[1]) % 31);
	rpmzWrite(zq, head, 2);
	len = 2;
	break;
    case RPMZ_FORMAT_GZIP:	/* gzip */
	head[0] = 31;
	head[1] = 139;
	head[2] = 8;                /* deflate */
	head[3] = zh->name != NULL ? 8 : 0;
	PUT4L(head + 4, zh->mtime);
	head[8] = zq->level == 9 ? 2 : (zq->level == 1 ? 4 : 0);
	head[9] = 3;                /* unix */
	rpmzWrite(zq, head, 10);
	len = 10;
	if (zh->name != NULL)
	    rpmzWrite(zq, (unsigned char *)zh->name, strlen(zh->name) + 1);
	if (zh->name != NULL)
	    len += strlen(zh->name) + 1;
	break;
    default:
assert(0);
	break;
    }
    return len;
}

/* write a gzip, zlib, or zip trailer */
static void rpmzPutTrailer(rpmzQueue zq, rpmzh zh)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    unsigned char tail[46];

    switch (zq->format) {
    case RPMZ_FORMAT_ZIP2:	/* zip */
    case RPMZ_FORMAT_ZIP3:
    {	unsigned long cent;

	/* write data descriptor (as promised in local header) */
	PUT4L(tail, zh->check);
	PUT4L(tail + 4, zh->clen);
	PUT4L(tail + 8, zh->ulen);
	rpmzWrite(zq, tail, 12);

	/* write central file header */
	PUT4L(tail, 0x02014b50UL);  /* central header signature */
	tail[4] = 63;               /* obeyed version 6.3 of the zip spec */
	tail[5] = 255;              /* ignore external attributes */
	PUT2L(tail + 6, 20);        /* version needed to extract (2.0) */
	PUT2L(tail + 8, 8);         /* data descriptor is present */
	PUT2L(tail + 10, 8);        /* deflate */
	PUT4L(tail + 12, time2dos(zh->mtime));
	PUT4L(tail + 16, zh->check);/* crc */
	PUT4L(tail + 20, zh->clen); /* compressed length */
	PUT4L(tail + 24, zh->ulen); /* uncompressed length */
	PUT2L(tail + 28, zh->name == NULL ? 1 : strlen(zh->name));  /* name length */
	PUT2L(tail + 30, 9);        /* length of extra field (see below) */
	PUT2L(tail + 32, 0);        /* no file comment */
	PUT2L(tail + 34, 0);        /* disk number 0 */
	PUT2L(tail + 36, 0);        /* internal file attributes */
	PUT4L(tail + 38, 0);        /* external file attributes (ignored) */
	PUT4L(tail + 42, 0);        /* offset of local header */
	rpmzWrite(zq, tail, 46);    /* write central file header */
	cent = 46;

	/* write file name (use "-" for stdin) */
	if (zh->name == NULL)
	    rpmzWrite(zq, (unsigned char *)"-", 1);
	else
	    rpmzWrite(zq, (unsigned char *)zh->name, strlen(zh->name));
	cent += zh->name == NULL ? 1 : strlen(zh->name);

	/* write extended timestamp extra field block (9 bytes) */
	PUT2L(tail, 0x5455);        /* extended timestamp signature */
	PUT2L(tail + 2, 5);         /* number of data bytes in this block */
	tail[4] = 1;                /* flag presence of mod time */
	PUT4L(tail + 5, zh->mtime); /* mod time */
	rpmzWrite(zq, tail, 9);     /* write extra field block */
	cent += 9;

	/* write end of central directory record */
	PUT4L(tail, 0x06054b50UL);  /* end of central directory signature */
	PUT2L(tail + 4, 0);         /* number of this disk */
	PUT2L(tail + 6, 0);         /* disk with start of central directory */
	PUT2L(tail + 8, 1);         /* number of entries on this disk */
	PUT2L(tail + 10, 1);        /* total number of entries */
	PUT4L(tail + 12, cent);     /* size of central directory */
	PUT4L(tail + 16, zh->head + zh->clen + 12); /* offset of central directory */
	PUT2L(tail + 20, 0);        /* no zip file comment */
	rpmzWrite(zq, tail, 22);    /* write end of central directory record */
    }	break;
    case RPMZ_FORMAT_ZLIB:	/* zlib */
	PUT4M(tail, zh->check);
	rpmzWrite(zq, tail, 4);
	break;
    case RPMZ_FORMAT_GZIP:	/* gzip */
	PUT4L(tail, zh->check);
	PUT4L(tail + 4, zh->ulen);
	rpmzWrite(zq, tail, 8);
	break;
    default:
assert(0);
	break;
    }
}

/* compute check value depending on format */
#define CHECK(a,b,c) \
    (zq->format == RPMZ_FORMAT_ZLIB ? adler32(a,b,c) : crc32(a,b,c))

#ifndef _PIGZNOTHREAD
/* -- threaded portions of pigz -- */

/* -- check value combination routines for parallel calculation -- */

#define COMB(a,b,c) \
    (zq->format == RPMZ_FORMAT_ZLIB ? adler32_comb(a,b,c) : crc32_comb(a,b,c))
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

/* -- parallel compression -- */

/* compress or write job (passed from compress list to write list) -- if seq is
   equal to -1, compress_thread() is instructed to return; if more is false then
   this is the last chunk, which after writing tells write_thread to return */

#ifndef	DYING	/* XXX only the in_pool/out_pool scaling remains todo++ ... */
/* command the compress threads to all return, then join them all (call from
   main thread), free all the thread-related resources */
static void _rpmzqFini(rpmzQueue zq)
	/*@globals fileSystem, internalState @*/
	/*@modifies zq, fileSystem, internalState @*/
{
    rpmzLog zlog = zq->zlog;

    struct rpmzJob_s job;
    int caught;

    /* only do this once */
    if (zq->compress_have == NULL)
	return;

    /* command all of the extant compress threads to return */
    yarnPossess(zq->compress_have);
    job.seq = -1;
    job.next = NULL;
/*@-immediatetrans -mustfreeonly@*/
    zq->compress_head = &job;
/*@=immediatetrans =mustfreeonly@*/
    zq->compress_tail = &(job.next);
    yarnTwist(zq->compress_have, BY, 1);       /* will wake them all up */

    /* join all of the compress threads, verify they all came back */
    caught = yarnJoinAll();
    Trace((zlog, "-- joined %d compress threads", caught));
assert(caught == zq->cthreads);
    zq->cthreads = 0;

    /* free the resources */
    zq->out_pool = rpmzqFreePool(zq->out_pool, &caught);
    Trace((zlog, "-- freed %d output buffers", caught));
    zq->in_pool = rpmzqFreePool(zq->in_pool, &caught);
    Trace((zlog, "-- freed %d input buffers", caught));
    zq->write_first = yarnFreeLock(zq->write_first);
    zq->compress_have = yarnFreeLock(zq->compress_have);
}

/* setup job lists (call from main thread) */
static void _rpmzqInit(rpmzQueue zq)
	/*@globals fileSystem, internalState @*/
	/*@modifies zq, fileSystem, internalState @*/
{
    /* set up only if not already set up*/
    if (zq->compress_have != NULL)
	return;

    /* allocate locks and initialize lists */
/*@-mustfreeonly@*/
    zq->compress_have = yarnNewLock(0);
    zq->compress_head = NULL;
    zq->compress_tail = &zq->compress_head;
    zq->write_first = yarnNewLock(-1);
    zq->write_head = NULL;

    /* initialize buffer pools */
    zq->in_pool = rpmzqNewPool(zq->blocksize, (zq->threads << 1) + 2);
    zq->out_pool = rpmzqNewPool(zq->blocksize + (zq->blocksize >> 11) + 10, -1);
/*@=mustfreeonly@*/
}
#endif	/* DYING */

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
    rpmzQueue zq = ((rpmz)_z)->zq;
    rpmzLog zlog = zq->zlog;

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
    if (deflateInit2(&strm, zq->level, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) !=
	    Z_OK)
	bail("not enough memory", "");

    /* keep looking for work */
    for (;;) {
	/* get a job (like I tell my son) */
#ifndef	DYING	/* XXX something buggy here */
	yarnPossess(zq->compress_have);
	yarnWaitFor(zq->compress_have, NOT_TO_BE, 0);
	job = zq->compress_head;
assert(job != NULL);
	if (job->seq == -1)
	    break;
/*@-dependenttrans@*/
	zq->compress_head = job->next;
/*@=dependenttrans@*/
	if (job->next == NULL)
	    zq->compress_tail = &zq->compress_head;
	yarnTwist(zq->compress_have, BY, -1);
#else
	job = rpmzqDelCJob(zq);
	if (job == NULL)
	    break;
#endif

	/* got a job -- initialize and set the compression level (note that if
	 * deflateParams() is called immediately after deflateReset(), there is
	 * no need to initialize the input/output for the stream) */
	Trace((zlog, "-- compressing #%ld", job->seq));
/*@-noeffect@*/
	(void)deflateReset(&strm);
	(void)deflateParams(&strm, zq->level, Z_DEFAULT_STRATEGY);
/*@=noeffect@*/

	/* set dictionary if provided, release that input buffer (only provided
	 * if F_ISSET(zq->flags, INDEPENDENT) is true and if this is not the first work unit) -- the
	 * amount of data in the buffer is assured to be >= _PIGZDICT */
	if (job->out != NULL) {
	    len = job->out->len;
/*@-noeffect@*/
	    deflateSetDictionary(&strm,
		(unsigned char *)(job->out->buf) + (len - _PIGZDICT), _PIGZDICT);
/*@=noeffect@*/
	    rpmzqDropSpace(job->out);
	}

	/* set up input and output (the output size is assured to be big enough
	 * for the worst case expansion of the input buffer size, plus five
	 * bytes for the terminating stored block) */
/*@-mustfreeonly@*/
	job->out = rpmzqNewSpace(zq->out_pool);
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
#ifndef	DYING	/* XXX eliminate zq->omode first */
	Trace((zlog, "-- compressed #%ld%s", job->seq, job->more ? "" : " (last)"));
#endif

	/* reserve input buffer until check value has been calculated */
	rpmzqUseSpace(job->in);

	/* insert write job in list in sorted order, alert write thread */
#ifndef	DYING	/* XXX eliminate zq->omode first */
	yarnPossess(zq->write_first);
	prior = &zq->write_head;
	while ((here = *prior) != NULL) {
	    if (here->seq > job->seq)
		break;
	    prior = &(here->next);
	}
	job->next = here;
	*prior = job;
	yarnTwist(zq->write_first, TO, zq->write_head->seq);
#else
	rpmzqAddWJob(zq, job);
#endif

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
	rpmzqDropSpace(job->in);
	job->check = check;
	yarnPossess(job->calc);
	Trace((zlog, "-- checked #%ld%s", job->seq, job->more ? "" : " (last)"));
	yarnTwist(job->calc, TO, 1);
	job = NULL;	/* XXX job is already free'd here. */

	/* done with that one -- go find another job */
    }

    /* found job with seq == -1 -- free deflate memory and return to join */
    yarnRelease(zq->compress_have);
/*@-noeffect@*/
    deflateEnd(&strm);
/*@=noeffect@*/
/*@-mustfreeonly@*/	/* XXX zq->compress_head not released */
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
    rpmzQueue zq = z->zq;
    rpmzLog zlog = zq->zlog;

    long seq;                       /* next sequence number looking for */
    rpmzJob job;                    /* job pulled and working on */
    size_t len;                     /* input length */
    int more;                       /* true if more chunks to write */
#ifdef	DYING
    unsigned long head;             /* header length */
    unsigned long ulen;             /* total uncompressed size (overflow ok) */
    unsigned long clen;             /* total compressed size (overflow ok) */
    unsigned long check;            /* check value of uncompressed data */
#else
    struct rpmzh_s _zh;
    rpmzh zh = &_zh;
    zh->name = z->name;
    zh->mtime = z->mtime;
#endif

    /* build and write header */
    Trace((zlog, "-- write thread running"));
    zh->head = rpmzPutHeader(zq, zh);

    /* process output of compress threads until end of input */    
    zh->ulen = zh->clen = 0;
    zh->check = CHECK(0L, Z_NULL, 0);
    seq = 0;
    do {
	/* get next write job in order */
	job = rpmzqDelWJob(zq, seq);
assert(job != NULL);

	/* update lengths, save uncompressed length for COMB */
	more = job->more;
	len = job->in->len;
	rpmzqDropSpace(job->in);
	zh->ulen += (unsigned long)len;
	zh->clen += (unsigned long)(job->out->len);

	/* write the compressed data and drop the output buffer */
	Trace((zlog, "-- writing #%ld", seq));
	rpmzWrite(zq, job->out->buf, job->out->len);
	rpmzqDropSpace(job->out);
	Trace((zlog, "-- wrote #%ld%s", seq, more ? "" : " (last)"));

	/* wait for check calculation to complete, then combine, once
	 * the compress thread is done with the input, release it */
	yarnPossess(job->calc);
	yarnWaitFor(job->calc, TO_BE, 1);
	yarnRelease(job->calc);
	zh->check = COMB(zh->check, job->check, len);

	/* free the job */
	job = rpmzqFreeJob(job);

	/* get the next buffer in sequence */
	seq++;
    } while (more);

    /* write trailer */
    rpmzPutTrailer(zq, zh);

    /* verify no more jobs, prepare for next use */
    rpmzqVerify(zq);

/*@-globstate@*/	/* XXX zq->write_head is reachable */
    return;
/*@=globstate@*/
}

/* compress zq->ifdno to zq->ofdno, using multiple threads for the compression and check
   value calculations and one other thread for writing the output -- compress
   threads will be launched and left running (waiting actually) to support
   subsequent calls of rpmzParallelCompress() */
static void rpmzParallelCompress(rpmz z)
	/*@globals fileSystem, internalState @*/
	/*@modifies z, fileSystem, internalState @*/
{
    rpmzQueue zq = z->zq;
    rpmzLog zlog = zq->zlog;

    long seq;                       /* sequence number */
    rpmzSpace prev;                 /* previous input space */
    rpmzSpace next;                 /* next input space */
    rpmzJob job;                    /* job for compress, then write */
    int more;                       /* true if more input to read */

    /* if first time or after an option change, setup the job lists */
    _rpmzqInit(zq);

    /* start write thread */
/*@-mustfreeonly@*/
    zq->writeth = yarnLaunch(write_thread, z);
/*@=mustfreeonly@*/

    /* read from input and start compress threads (write thread will pick up
       the output of the compress threads) */
    seq = 0;
    prev = NULL;
    next = rpmzqNewSpace(zq->in_pool);
    next->len = rpmzRead(zq, next->buf, next->pool->size);
    do {
	/* create a new job, use next input chunk, previous as dictionary */
	job = rpmzqNewJob(seq);
/*@-mustfreeonly@*/
	job->in = next;
	job->out = F_ISSET(zq->flags, INDEPENDENT) ? prev : NULL;  /* dictionary for compression */
/*@=mustfreeonly@*/

	/* check for end of file, reading next chunk if not sure */
	if (next->len < next->pool->size)
	    more = 0;
	else {
	    next = rpmzqNewSpace(zq->in_pool);
	    next->len = rpmzRead(zq, next->buf, next->pool->size);
	    more = next->len != 0;
	    if (!more)
		rpmzqDropSpace(next);  /* won't be using it */
	    if (F_ISSET(zq->flags, INDEPENDENT) && more) {
		rpmzqUseSpace(job->in);    /* hold as dictionary for next loop */
		prev = job->in;
	    }
	}
	job->more = more;
	Trace((zlog, "-- read #%ld%s", seq, more ? "" : " (last)"));
	if (++seq < 1)
	    bail("input too long: ", zq->ifn);

	/* start another compress thread if needed */
	if (zq->cthreads < (int)seq && zq->cthreads < (int)zq->threads) {
	    (void)yarnLaunch(compress_thread, z);
	    zq->cthreads++;
	}

	/* put job at end of compress list, let all the compressors know */
	rpmzqAddCJob(zq, job);

	/* do until end of input */
    } while (more);

    /* wait for the write thread to complete (we leave the compress threads out
       there and waiting in case there is another stream to compress) */
    zq->writeth = yarnJoin(zq->writeth);
    Trace((zlog, "-- write thread joined"));
}

#endif

/* do a simple compression in a single thread from zq->ifdno to zq->ofdno -- if reset is
   true, instead free the memory that was allocated and retained for input,
   output, and deflate */
/*@-nullstate@*/
static void rpmzSingleCompress(rpmz z, int reset)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    rpmzQueue zq = z->zq;
    size_t got;                     /* amount read */
    size_t more;                    /* amount of next read (0 if eof) */
    static unsigned out_size;       /* size of output buffer */
/*@only@*/
    static unsigned char *in;       /* reused i/o buffers */
/*@only@*/
    static unsigned char *next;     /* reused i/o buffers */
/*@only@*/
    static unsigned char *out;      /* reused i/o buffers */
/*@only@*/ /*@null@*/
    static z_stream *strm = NULL;   /* reused deflate structure */
#ifdef	DYING
    unsigned long head;             /* header length */
    unsigned long ulen;             /* total uncompressed size (overflow ok) */
    unsigned long clen;             /* total compressed size (overflow ok) */
    unsigned long check;            /* check value of uncompressed data */
#else
    struct rpmzh_s _zh;
    rpmzh zh = &_zh;
    zh->name = z->name;
    zh->mtime = z->mtime;
#endif

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
	out_size = zq->blocksize > _PIGZMAX ? _PIGZMAX : (unsigned)zq->blocksize;

/*@-mustfreeonly@*/
	in = xmalloc(zq->blocksize);
	next = xmalloc(zq->blocksize);
	out = xmalloc(out_size);
	strm = xmalloc(sizeof(*strm));
/*@=mustfreeonly@*/

/*@-mustfreeonly@*/
	strm->zfree = Z_NULL;
	strm->zalloc = Z_NULL;
	strm->opaque = Z_NULL;
/*@=mustfreeonly@*/
	if (deflateInit2(strm, zq->level, Z_DEFLATED, -15, 8,
		Z_DEFAULT_STRATEGY) != Z_OK)
	    bail("not enough memory", "");
    }

    /* write header */
    zh->head = rpmzPutHeader(zq, zh);

    /* set compression level in case it changed */
/*@-noeffect@*/
    (void)deflateReset(strm);
    (void)deflateParams(strm, zq->level, Z_DEFAULT_STRATEGY);
/*@=noeffect@*/

    /* do raw deflate and calculate check value */
    zh->ulen = zh->clen = 0;
    zh->check = CHECK(0L, Z_NULL, 0);
    more = rpmzRead(zq, next, zq->blocksize);
    do {
	/* get data to compress, see if there is any more input */
	got = more;
	{ unsigned char *temp; temp = in; in = next; next = temp; }
	more = got < zq->blocksize ? 0 : rpmzRead(zq, next, zq->blocksize);
	zh->ulen += (unsigned long)got;
/*@-mustfreeonly@*/
	strm->next_in = in;
/*@=mustfreeonly@*/

	/* compress _PIGZMAX-size chunks in case unsigned type is small */
	while (got > _PIGZMAX) {
	    strm->avail_in = _PIGZMAX;
	    zh->check = CHECK(zh->check, strm->next_in, strm->avail_in);
	    do {
		strm->avail_out = out_size;
/*@-mustfreeonly@*/
		strm->next_out = out;
/*@=mustfreeonly@*/
/*@-noeffect@*/
		(void)deflate(strm, Z_NO_FLUSH);
/*@=noeffect@*/
		rpmzWrite(zq, out, out_size - strm->avail_out);
		zh->clen += out_size - strm->avail_out;
	    } while (strm->avail_out == 0);
assert(strm->avail_in == 0);
	    got -= _PIGZMAX;
	}

	/* compress the remainder, finishing if end of input -- when not -i,
	 * use a Z_SYNC_FLUSH so that this and parallel compression produce the
	 * same output */
	strm->avail_in = (unsigned)got;
	zh->check = CHECK(zh->check, strm->next_in, strm->avail_in);
	do {
	    strm->avail_out = out_size;
/*@-kepttrans -mustfreeonly@*/
	    strm->next_out = out;
/*@=kepttrans =mustfreeonly@*/
/*@-noeffect@*/
	    (void)deflate(strm,
		more ? (F_ISSET(zq->flags, INDEPENDENT) ? Z_SYNC_FLUSH : Z_FULL_FLUSH) : Z_FINISH);
/*@=noeffect@*/
	    rpmzWrite(zq, out, out_size - strm->avail_out);
	    zh->clen += out_size - strm->avail_out;
	} while (strm->avail_out == 0);
assert(strm->avail_in == 0);

	/* do until no more input */
    } while (more);

    /* write trailer */
    rpmzPutTrailer(zq, zh);
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
    rpmzQueue zq = z->zq;
    rpmzLog zlog = zq->zlog;
    size_t nread;

    Trace((zlog, "-- launched decompress read thread"));
    do {
	yarnPossess(z->load_state);
	yarnWaitFor(z->load_state, TO_BE, 1);
assert(z->in_pend == 0);
assert(z->in_next == NULL);
	z->in_pend = z->in_buf_allocated;
	z->in_next = z->in_which ? z->in_buf : z->in_buf2;
	nread = rpmzRead(zq, z->in_next, z->in_pend);
	z->in_pend = nread;

	Trace((zlog, "-- decompress read thread read %lu bytes", nread));
	yarnTwist(z->load_state, TO, 0);
    } while (nread == z->in_buf_allocated);
    Trace((zlog, "-- exited decompress read thread"));
}
#endif

/* load() is called when job->in->len has gone to zero in order to provide more
   input data: load the input buffer with z->in_buf_allocated (or fewer if at end of file) bytes
   from the file zq->ifdno, set job->in->buf to point to the job->in->len bytes read,
   update z->in_tot, and return job->in->len -- z->in_eof is set to true when job->in->len has
   gone to zero and there is no more data left to read from zq->ifdno */
static size_t load(rpmz z)
	/*@globals fileSystem, internalState @*/
	/*@modifies z, fileSystem, internalState @*/
{
    rpmzQueue zq = z->zq;
    rpmzJob job = &zq->_job;

    /* if already detected end of file, do nothing */
    if (z->in_short) {
	z->in_eof = 1;
	return 0;
    }

assert(job->in != NULL);
#if 0		/* XXX hmmm, the comment above is incorrect. todo++ */
assert(job->in->len == 0);
#endif

#ifndef _PIGZNOTHREAD
    /* if first time in or z->threads == 1, read a buffer to have something to
       return, otherwise wait for the previous read job to complete */
    if (zq->threads > 1) {
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
	job->in->len = z->in_pend;
	job->in->buf = z->in_next;
	z->in_pend = 0;
	z->in_next = NULL;

	/* if not at end of file, alert read thread to load next buffer,
	 * alternate between in_buf and in_buf2 */
	if (job->in->len == z->in_buf_allocated) {
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
    {	size_t nread;
	/* don't use threads -- simply read a buffer into z->in_buf */
assert(z->in_pend == 0);
assert(z->in_next == NULL);
	z->in_pend = z->in_buf_allocated;
	z->in_next = z->in_buf;
	nread = rpmzRead(zq, z->in_next, z->in_pend);
	z->in_pend = nread;
	job->in->len = z->in_pend;
	job->in->buf = z->in_next;
	z->in_pend = 0;
	z->in_next = NULL;
    }

    /* note end of file */
    if (job->in->len < z->in_buf_allocated) {
	z->in_short = 1;

	/* if we got bupkis, now is the time to mark eof */
	if (job->in->len == 0)
	    z->in_eof = 1;
    }

    /* update the total and return the available bytes */
    z->in_tot += job->in->len;
    return job->in->len;
}

/* initialize for reading new input */
static void in_init(rpmz z)
	/*@modifies z @*/
{
    rpmzQueue zq = z->zq;
    rpmzJob job = &zq->_job;
    job->in->len = 0;
    job->in->buf = NULL;
    z->in_eof = 0;
    z->in_short = 0;
    z->in_tot = 0;
#ifndef _PIGZNOTHREAD
    z->in_which = -1;
#endif
}

/* buffered reading macros for decompression and listing */
#define GET() (z->in_eof || (job->in->len == 0 && load(z) == 0) ? EOF : \
               (job->in->len--, *job->in->buf++))
#define GET2() (tmp2 = GET(), tmp2 + (GET() << 8))
#define GET4() (tmp4 = GET2(), tmp4 + ((unsigned long)(GET2()) << 16))
#define SKIP(dist) \
    do { \
	size_t togo = (dist); \
	while (togo > job->in->len) { \
	    togo -= job->in->len; \
	    if (load(z) == 0) \
		return -1; \
	} \
	job->in->len -= togo; \
	job->in->buf += togo; \
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
    rpmzQueue zq = z->zq;
    rpmzJob job = &zq->_job;
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

/* read a gzip, zip, zlib, or lzw header from zq->ifdno and extract useful
   information, return the method -- or on error return negative: -1 is
   immediate EOF, -2 is not a recognized compressed format, -3 is premature EOF
   within the header, -4 is unexpected header flag values; a method of 256 is
   lzw -- set zq->format to indicate gzip, zlib, or zip */
static int rpmzGetHeader(rpmz z, int save)
	/*@globals fileSystem, internalState @*/
	/*@modifies z, fileSystem, internalState @*/
{
    rpmzQueue zq = z->zq;
    rpmzJob job = &zq->_job;
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
    zq->format = RPMZ_FORMAT_GZIP;
    magic = GET() << 8;
    if (z->in_eof)
	return -1;
    magic += GET();
    if (z->in_eof)
	return -2;
    if (magic % 31 == 0) {          /* it's zlib */
	zq->format = RPMZ_FORMAT_ZLIB;
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
	    while (fname > job->in->len) {
		memcpy(next, job->in->buf, job->in->len);
		fname -= job->in->len;
		next += job->in->len;
		if (load(z) == 0)
		    return -3;
	    }
	    memcpy(next, job->in->buf, fname);
	    job->in->len -= fname;
	    job->in->buf += fname;
	    next += fname;
	    *next = '\0';
	}
	else
	    SKIP(fname);
	rpmzReadExtra(z, extra, save);
	zq->format = (flags & 8) ? RPMZ_FORMAT_ZIP3 : RPMZ_FORMAT_ZIP2;
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
	    if (job->in->len == 0 && load(z) == 0)
		return -3;
	    end = memchr(job->in->buf, 0, job->in->len);
	    copy = end == NULL ? job->in->len : (size_t)(end - job->in->buf) + 1;
	    if (have + copy > size) {
		while (have + copy > (size <<= 1))
		    ;
		z->hname = realloc(z->hname, size);
		if (z->hname == NULL)
		    bail("not enough memory", "");
	    }
	    memcpy(z->hname + have, job->in->buf, copy);
	    have += copy;
	    job->in->len -= copy;
	    job->in->buf += copy;
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
static size_t compressed_suffix(const char *fn)
	/*@*/
{
    size_t len;

    len = strlen(fn);
    if (len > 4) {
	fn += len - 4;
	len = 4;
	if (strcmp(fn, ".zip") == 0 || strcmp(fn, ".ZIP") == 0)
	    return 4;
    }
    if (len > 3) {
	fn += len - 3;
	len = 3;
	if (strcmp(fn, ".gz") == 0 || strcmp(fn, "-gz") == 0 ||
	    strcmp(fn, ".zz") == 0 || strcmp(fn, "-zz") == 0)
	    return 3;
    }
    if (len > 2) {
	fn += len - 2;
	if (strcmp(fn, ".z") == 0 || strcmp(fn, "-z") == 0 ||
	    strcmp(fn, "_z") == 0 || strcmp(fn, ".Z") == 0)
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
    rpmzQueue zq = z->zq;
    static int oneshot = 1; /* true if we need to print listing header */
    size_t max;             /* maximum name length for current verbosity */
    size_t n;               /* name length without suffix */
    time_t now;             /* for getting current year */
    char mod[26];           /* modification time in text */
    char name[NAMEMAX1+1];  /* header or file name, possibly truncated */

    /* create abbreviated name from header file name or actual file name */
    max = zq->verbosity > 1 ? NAMEMAX2 : NAMEMAX1;
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
	if (zq->verbosity > 1)
	    fputs("method    check    timestamp    ", stdout);
	if (zq->verbosity > 0)
	    puts("compressed   original reduced  name");
	oneshot = 0;
    }

    /* print information */
    if (zq->verbosity > 1) {
	if (zq->format == RPMZ_FORMAT_ZIP3 && zq->mode == RPMZ_MODE_COMPRESS)
	    printf("zip%3d  --------  %s  ", method, mod + 4);
	else if (zq->format == RPMZ_FORMAT_ZIP2 || zq->format == RPMZ_FORMAT_ZIP3)
	    printf("zip%3d  %08lx  %s  ", method, check, mod + 4);
	else if (zq->format == RPMZ_FORMAT_ZLIB)
	    printf("zlib%2d  %08lx  %s  ", method, check, mod + 4);
	else if (method == 256)
	    printf("lzw     --------  %s  ", mod + 4);
	else
	    printf("gzip%2d  %08lx  %s  ", method, check, mod + 4);
    }
    if (zq->verbosity > 0) {
	if ((zq->format == RPMZ_FORMAT_ZIP3 && zq->mode == RPMZ_MODE_COMPRESS) ||
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

/* list content information about the gzip file at zq->ifdno (only works if the gzip
   file contains a single gzip stream with no junk at the end, and only works
   well if the uncompressed length is less than 4 GB) */
static void rpmzListInfo(rpmz z)
	/*@globals fileSystem, internalState @*/
	/*@modifies z, fileSystem, internalState @*/
{
    rpmzQueue zq = z->zq;
    rpmzJob job = &zq->_job;
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
	if (method != -1 && zq->verbosity > 1)
	    fprintf(stderr, "%s not a compressed file -- skipping\n", z->_ifn);
	return;
    }

    /* list zip file */
    if (zq->format == RPMZ_FORMAT_ZIP2 || zq->format == RPMZ_FORMAT_ZIP3) {
	z->in_tot = z->zip_clen;
	rpmzShowInfo(z, method, z->zip_crc, z->zip_ulen, 0);
	return;
    }

    /* list zlib file */
    if (zq->format == RPMZ_FORMAT_ZLIB) {
	at = lseek(zq->ifdno, 0, SEEK_END);
	if (at == -1) {
	    check = 0;
	    do {
		len = job->in->len < 4 ? job->in->len : 4;
		job->in->buf += job->in->len - len;
		while (len--)
		    check = (check << 8) + *job->in->buf++;
	    } while (load(z) != 0);
	    check &= LOW32;
	}
	else {
	    z->in_tot = at;
	    lseek(zq->ifdno, -4, SEEK_END);
	    rpmzRead(zq, tail, 4);
	    check = (*tail << 24) + (tail[1] << 16) + (tail[2] << 8) + tail[3];
	}
	z->in_tot -= 6;
	rpmzShowInfo(z, method, check, 0, 0);
	return;
    }

    /* list lzw file */
    if (method == 256) {
	at = lseek(zq->ifdno, 0, SEEK_END);
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
	if (job->in->len < 8) {
	    if (zq->verbosity > 0)
		fprintf(stderr, "%s not a valid gzip file -- skipping\n",
			z->_ifn);
	    return;
	}
	z->in_tot = job->in->len - 8;     /* compressed size */
	memcpy(tail, job->in->buf + (job->in->len - 8), 8);
    }
    else if ((at = lseek(zq->ifdno, -8, SEEK_END)) != -1) {
	z->in_tot = at - z->in_tot + job->in->len; /* compressed size */
	rpmzRead(zq, tail, 8);           /* get trailer */
    }
    else {                              /* can't seek */
	at = z->in_tot - job->in->len;    /* save header size */
	do {
	    n = job->in->len < 8 ? job->in->len : 8;
	    memcpy(tail, job->in->buf + (job->in->len - n), n);
	    load(z);
	} while (job->in->len == z->in_buf_allocated);       /* read until end */
	if (job->in->len < 8) {
	    if (n + job->in->len < 8) {
		if (zq->verbosity > 0)
		    fprintf(stderr, "%s not a valid gzip file -- skipping\n",
				z->_ifn);
		return;
	    }
	    if (job->in->len) {
/*@-aliasunique@*/
		if (n + job->in->len > 8)
		    memcpy(tail, tail + n - (8 - job->in->len), 8 - job->in->len);
/*@=aliasunique@*/
		memcpy(tail + 8 - job->in->len, job->in->buf, job->in->len);
	    }
	}
	else
	    memcpy(tail, job->in->buf + (job->in->len - 8), 8);
	z->in_tot -= at + 8;
    }
    if (z->in_tot < 2) {
	if (zq->verbosity > 0)
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
    rpmzQueue zq = z->zq;
    rpmzJob job = &zq->_job;
    load(z);
    *buf = job->in->buf;
    return job->in->len;
}

#ifndef	_PIGZNOTHREAD
/* output write thread */
static void outb_write(void *_z)
	/*@globals fileSystem, internalState @*/
	/*@modifies _z, fileSystem, internalState @*/
{
    rpmz z = _z;
    rpmzQueue zq = z->zq;
    rpmzLog zlog = zq->zlog;
    size_t len;

    Trace((zlog, "-- launched decompress write thread"));
    do {
	yarnPossess(z->outb_write_more);
	yarnWaitFor(z->outb_write_more, TO_BE, 1);
	len = z->out_len;
	if (len && zq->mode == RPMZ_MODE_DECOMPRESS)
	    rpmzWrite(zq, z->out_copy, len);
	Trace((zlog, "-- decompress wrote %lu bytes", len));
	yarnTwist(z->outb_write_more, TO, 0);
    } while (len);
    Trace((zlog, "-- exited decompress write thread"));
}

/* output check thread */
static void outb_check(void *_z)
	/*@globals fileSystem, internalState @*/
	/*@modifies _z, fileSystem, internalState @*/
{
    rpmz z = _z;
    rpmzQueue zq = z->zq;
    rpmzLog zlog = zq->zlog;
    size_t len;

    Trace((zlog, "-- launched decompress check thread"));
    do {
	yarnPossess(z->outb_check_more);
	yarnWaitFor(z->outb_check_more, TO_BE, 1);
	len = z->out_len;
	z->out_check = CHECK(z->out_check, z->out_copy, len);
	Trace((zlog, "-- decompress checked %lu bytes", len));
	yarnTwist(z->outb_check_more, TO, 0);
    } while (len);
    Trace((zlog, "-- exited decompress check thread"));
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
    rpmzQueue zq = z->zq;
#ifndef _PIGZNOTHREAD
/*@only@*/ /*@relnull@*/
    static yarnThread wr;
/*@only@*/ /*@relnull@*/
    static yarnThread ch;

    if (zq->threads > 1) {
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
	if (zq->mode == RPMZ_MODE_DECOMPRESS)
	    rpmzWrite(zq, buf, len);
	z->out_check = CHECK(z->out_check, buf, len);
	z->out_tot += len;
    }
    return 0;
}

/* inflate for decompression or testing -- decompress from zq->ifdno to zq->ofdno unless
   zq->mode != RPMZ_MODE_DECOMPRESS, in which case just test zq->ifdno, and then also list if zq->mode == RPMZ_MODE_LIST;
   look for and decode multiple, concatenated gzip and/or zlib streams;
   read and check the gzip, zlib, or zip trailer */
/*@-nullstate@*/
static void rpmzInflateCheck(rpmz z)
	/*@globals fileSystem, internalState @*/
	/*@modifies z, fileSystem, internalState @*/
{
    rpmzQueue zq = z->zq;
    rpmzJob job = &zq->_job;
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
	z->in_tot = job->in->len;               /* track compressed data length */
	z->out_tot = 0;
	z->out_check = CHECK(0L, Z_NULL, 0);
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	ret = inflateBackInit(&strm, 15, z->out_buf);
	if (ret != Z_OK)
	    bail("not enough memory", "");

	/* decompress, compute lengths and check value */
	strm.avail_in = job->in->len;
/*@-sharedtrans@*/
	strm.next_in = job->in->buf;
/*@=sharedtrans@*/
	ret = inflateBack(&strm, inb, z, outb, z);
	if (ret != Z_STREAM_END)
	    bail("corrupted input -- invalid deflate data: ", z->_ifn);
	job->in->len = strm.avail_in;
/*@-onlytrans@*/
	job->in->buf = strm.next_in;
/*@=onlytrans@*/
/*@-noeffect@*/
	inflateBackEnd(&strm);
/*@=noeffect@*/
	outb(z, NULL, 0);        /* finish off final write and check */

	/* compute compressed data length */
	clen = z->in_tot - job->in->len;

	/* read and check trailer */
	switch (zq->format) {
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
	if (F_ISSET(zq->flags, LIST)) {
	    z->in_tot = clen;
	    rpmzShowInfo(z, 8, check, z->out_tot, cont);
	    cont = 1;
	}

	/* if a gzip or zlib entry follows a gzip or zlib entry, decompress it
	 * (don't replace saved header information from first entry) */
    } while ((zq->format == RPMZ_FORMAT_GZIP || zq->format == RPMZ_FORMAT_ZLIB) && (ret = rpmzGetHeader(z, 0)) == 8);
    if (ret != -1 && (zq->format == RPMZ_FORMAT_GZIP || zq->format == RPMZ_FORMAT_ZLIB))
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
	if (chunk > job->in->len) { \
	    chunk -= job->in->len; \
	    if (load(z) == 0) \
		break; \
	    if (chunk > job->in->len) { \
		chunk = job->in->len = 0; \
		break; \
	    } \
	} \
	job->in->len -= chunk; \
	job->in->buf += chunk; \
	chunk = 0; \
    } while (0)

/* Decompress a compress (LZW) file from zq->ifdno to zq->ofdno.  The compress magic
   header (two bytes) has already been read and verified. */
static void rpmzDecompressLZW(rpmz z)
	/*@globals fileSystem, internalState @*/
	/*@modifies z, fileSystem, internalState @*/
{
    rpmzQueue zq = z->zq;
    rpmzJob job = &zq->_job;
    int got;                    /* byte just read by GET() */
    unsigned int chunk;         /* bytes left in current chunk */
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
	    if (outcnt && zq->mode == RPMZ_MODE_DECOMPRESS)
		rpmzWrite(zq, z->out_buf, outcnt);
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
	    if (zq->mode == RPMZ_MODE_DECOMPRESS)
		rpmzWrite(zq, z->out_buf, outcnt);
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
static void copymeta(const char *from, const char *to)
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
static void touch(const char *path, time_t t)
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
    rpmzQueue zq = z->zq;
    int method = -1;                /* rpmzGetHeader() return value */
    size_t len;                     /* length of base name (minus suffix) */
    struct stat sb, *st = &sb;      /* to get file type and mod time */

    /* open input file with name in, descriptor zq->ifdno -- set name and mtime */
    if (path == NULL) {
	strcpy(z->_ifn, z->stdin_fn);
	zq->ifdno = STDIN_FILENO;
/*@-mustfreeonly@*/
	z->name = NULL;
/*@=mustfreeonly@*/
	z->mtime = F_ISSET(zq->flags, HTIME) ?
		(fstat(zq->ifdno, st) ? time(NULL) : st->st_mtime) : 0;
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
	    if (zq->verbosity > 0)
		fprintf(stderr, "%s does not exist -- skipping\n", z->_ifn);
	    return;
	}
	if (!S_ISREG(st->st_mode) && !S_ISLNK(st->st_mode) && !S_ISDIR(st->st_mode)) {
	    if (zq->verbosity > 0)
		fprintf(stderr, "%s is a special file or device -- skipping\n", z->_ifn);
	    return;
	}
	if (S_ISLNK(st->st_mode) && !F_ISSET(zq->flags, SYMLINKS)) {
	    if (zq->verbosity > 0)
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

	    if (!F_ISSET(zq->flags, RECURSE)) {
		if (zq->verbosity > 0)
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
	if (!(F_ISSET(zq->flags, ALREADY) || F_ISSET(zq->flags, LIST) || zq->mode != RPMZ_MODE_COMPRESS) && len >= strlen(zq->suffix) &&
		strcmp(z->_ifn + len - strlen(zq->suffix), zq->suffix) == 0) {
	    if (zq->verbosity > 0)
		fprintf(stderr, "%s ends with %s -- skipping\n", z->_ifn, zq->suffix);
	    return;
	}

	/* only decompress or list files with compressed suffix */
	if (F_ISSET(zq->flags, LIST) || zq->mode != RPMZ_MODE_COMPRESS) {
	    int suf = compressed_suffix(z->_ifn);
	    if (suf == 0) {
		if (zq->verbosity > 0)
		    fprintf(stderr, "%s does not have compressed suffix -- skipping\n",
                            z->_ifn);
		return;
	    }
	    len -= suf;
	}

	/* open input file */
	zq->ifdno = open(z->_ifn, O_RDONLY, 0);
	if (zq->ifdno < 0)
	    bail("read error on ", z->_ifn);

	/* prepare gzip header information for compression */
/*@-mustfreeonly@*/
	z->name = F_ISSET(zq->flags, HNAME) ? justname(z->_ifn) : NULL;
/*@=mustfreeonly@*/
	z->mtime = F_ISSET(zq->flags, HTIME) ? st->st_mtime : 0;
    }
    SET_BINARY_MODE(zq->ifdno);

    /* if decoding or testing, try to read gzip header */
assert(z->hname == NULL);
    z->hname = _free(z->hname);
    if (zq->mode != RPMZ_MODE_COMPRESS) {
	in_init(z);
	method = rpmzGetHeader(z, 1);
	if (method != 8 && method != 256) {
	    z->hname = _free(z->hname);
	    if (zq->ifdno != STDIN_FILENO)
		close(zq->ifdno);
	    if (method != -1 && zq->verbosity > 0)
		fprintf(stderr,
		    method < 0 ? "%s is not compressed -- skipping\n" :
			"%s has unknown compression method -- skipping\n",
			z->_ifn);
	    return;
	}

	/* if requested, test input file (possibly a special list) */
	if (zq->mode == RPMZ_MODE_TEST) {
	    if (method == 8)
		rpmzInflateCheck(z);
	    else {
		rpmzDecompressLZW(z);
		if (F_ISSET(zq->flags, LIST)) {
		    z->in_tot -= 3;
		    rpmzShowInfo(z, method, 0, z->out_tot, 0);
		}
	    }
	    z->hname = _free(z->hname);
	    if (zq->ifdno != STDIN_FILENO)
		close(zq->ifdno);
	    return;
	}
    }

    /* if requested, just list information about input file */
    if (F_ISSET(zq->flags, LIST)) {
	rpmzListInfo(z);
	z->hname = _free(z->hname);
	if (zq->ifdno != STDIN_FILENO)
	    close(zq->ifdno);
	return;
    }

    /* create output file out, descriptor zq->ofdno */
    if (path == NULL || F_ISSET(zq->flags, STDOUT)) {
	/* write to stdout */
/*@-mustfreeonly@*/
	zq->ofn = xstrdup(z->stdout_fn);
/*@=mustfreeonly@*/
	zq->ofdno = STDOUT_FILENO;
	if (zq->mode == RPMZ_MODE_COMPRESS && !F_ISSET(zq->flags, TTY) && isatty(zq->ofdno))
	    bail("trying to write compressed data to a terminal",
		" (use -f to force)");
    }
    else {
	char *to;

	/* use header name for output when decompressing with -N */
	to = z->_ifn;
	if (zq->mode != RPMZ_MODE_COMPRESS && F_ISSET(zq->flags, HNAME) && z->hname != NULL) {
	    to = z->hname;
	    len = strlen(z->hname);
	}

	/* create output file and open to write */
	{   size_t nb = len +
		(zq->mode != RPMZ_MODE_COMPRESS ? 0 : strlen(zq->suffix)) + 1;
	    char * t = xmalloc(nb);
	    memcpy(t, to, len);
	    strcpy(t + len, zq->mode != RPMZ_MODE_COMPRESS ? "" : zq->suffix);
	    zq->ofn = t;
	}
	zq->ofdno = open(zq->ofn, O_CREAT | O_TRUNC | O_WRONLY |
                         (F_ISSET(zq->flags, OVERWRITE) ? 0 : O_EXCL), 0666);

	/* if exists and not -f, give user a chance to overwrite */
	if (zq->ofdno < 0 && errno == EEXIST && isatty(0) && zq->verbosity) {
	    int ch, reply;

	    fprintf(stderr, "%s exists -- overwrite (y/n)? ", zq->ofn);
	    fflush(stderr);
	    reply = -1;
	    do {
		ch = getchar();
		if (reply < 0 && ch != ' ' && ch != '\t')
		    reply = ch == 'y' || ch == 'Y' ? 1 : 0;
	    } while (ch != EOF && ch != '\n' && ch != '\r');
	    if (reply == 1)
		zq->ofdno = open(zq->ofn, O_CREAT | O_TRUNC | O_WRONLY, 0666);
	}

	/* if exists and no overwrite, report and go on to next */
	if (zq->ofdno < 0 && errno == EEXIST) {
	    if (zq->verbosity > 0)
		fprintf(stderr, "%s exists -- skipping\n", zq->ofn);
	    zq->ofn = _free(zq->ofn);
	    z->hname = _free(z->hname);
	    if (zq->ifdno != STDIN_FILENO)
		close(zq->ifdno);
	    return;
	}

	/* if some other error, give up */
	if (zq->ofdno < 0)
	    bail("write error on ", zq->ofn);
    }
    SET_BINARY_MODE(zq->ofdno);
    z->hname = _free(z->hname);

    /* process zq->ifdno to zq->ofdno */
    if (zq->verbosity > 1)
	fprintf(stderr, "%s to %s ", z->_ifn, zq->ofn);
    if (zq->mode != RPMZ_MODE_COMPRESS) {
	if (method == 8)
	    rpmzInflateCheck(z);
	else
	    rpmzDecompressLZW(z);
    }
#ifndef _PIGZNOTHREAD
    else if (zq->threads > 1)
	rpmzParallelCompress(z);
#endif
    else
	rpmzSingleCompress(z, 0);
    if (zq->verbosity > 1) {
	putc('\n', stderr);
	fflush(stderr);
    }

    /* finish up, copy attributes, set times, delete original */
    if (zq->ifdno != STDIN_FILENO)
	close(zq->ifdno);
    if (zq->ofdno != STDOUT_FILENO) {
	if (close(zq->ofdno))
	    bail("write error on ", zq->ofn);
	zq->ofdno = -1;              /* now prevent deletion on interrupt */
	if (zq->ifdno != STDIN_FILENO) {
	    copymeta(z->_ifn, zq->ofn);
	    if (!F_ISSET(zq->flags, KEEP))
		Unlink(z->_ifn);
	}
	if (zq->mode != RPMZ_MODE_COMPRESS && F_ISSET(zq->flags, HTIME) && z->stamp)
	    touch(zq->ofn, z->stamp);
    }
    zq->ofn = _free(zq->ofn);
}

/*==============================================================*/

/* either new buffer size, new compression level, or new number of processes --
   get rid of old buffers and threads to force the creation of new ones with
   the new settings */
static void rpmzNewOpts(rpmz z)
	/*@globals fileSystem, internalState @*/
	/*@modifies z, fileSystem, internalState @*/
{
    rpmzQueue zq = z->zq;

    rpmzSingleCompress(z, 1);
#ifndef _PIGZNOTHREAD
    _rpmzqFini(zq);
#endif
}

/* catch termination signal */
/*@exits@*/
static void rpmzAbort(/*@unused@*/ int sig)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    rpmz z = _rpmz;
    rpmzQueue zq = z->zq;

    Trace((zq->zlog, "termination by user"));
    if (zq->ofdno != -1 && zq->ofn != NULL)
		Unlink(zq->ofn);
    zq->zlog = rpmzLogDump(zq->zlog, NULL);
    _exit(EXIT_FAILURE);
}

/**
 */
static void rpmzqArgCallback(poptContext con,
		/*@unused@*/ enum poptCallbackReason reason,
		const struct poptOption * opt, /*@unused@*/ const char * arg,
		/*@unused@*/ void * data)
	/*@globals _rpmz, fileSystem, internalState @*/
	/*@modifies _rpmz, fileSystem, internalState @*/
{
    rpmz z = _rpmz;
    rpmzQueue zq = z->zq;

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
	zq->blocksize = (size_t)(atol(arg)) << 10;   /* chunk size */
	if (zq->blocksize < _PIGZDICT)
	    bail("block size too small (must be >= 32K)", "");
	if (zq->blocksize + (zq->blocksize >> 11) + 10 < (zq->blocksize >> 11) + 10 ||
	    (ssize_t)(zq->blocksize + (zq->blocksize >> 11) + 10) < 0)
	    bail("block size too large", "");
	rpmzNewOpts(z);
	break;
    case 'p':
	zq->threads = atoi(arg);                  /* # processes */
	if (zq->threads < 1)
	    bail("need at least one process", "");
	if ((2 + (zq->threads << 1)) < 1)
	    bail("too many processes", "");
#ifdef _PIGZNOTHREAD
	if (zq->threads > 1)
	    bail("this pigz compiled without threads", "");
#endif
	rpmzNewOpts(z);
	break;
    case 'q':	zq->verbosity = 0; break;
    case 'v':	zq->verbosity++; break;
    default:
	/* XXX really need to display longName/shortName instead. */
	fprintf(stderr, _("Unknown option -%c\n"), (char)opt->val);
	poptPrintUsage(con, stderr, 0);
	/*@-exitarg@*/ exit(2); /*@=exitarg@*/
	/*@notreached@*/ break;
    }
}

/*==============================================================*/
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

/* XXX grrr, popt needs better way to insert text strings in --help. */
/* XXX fixme: popt does post order recursion into sub-tables. */
/*@unchecked@*/ /*@observer@*/
static struct poptOption rpmzPrivatePoptTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
	rpmzqArgCallback, 0, NULL, NULL },
/*@=type@*/

  { "debug", 'D', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_debug, -1,
	N_("debug spewage"), NULL },

	/* XXX POPT_ARG_ARGV portability. */
  { "files", '\0', POPT_ARG_ARGV,		&__rpmz.manifests, 0,
	N_("Read file names from MANIFEST"), N_("MANIFEST") },
  { "quiet", 'q',	POPT_ARG_VAL,				NULL,  'q',
	N_("Print no messages, even on error"), NULL },
  { "verbose", 'v',	POPT_ARG_VAL,				NULL,  'v',
	N_("Provide more verbose output"), NULL },
  { "version", 'V',	POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	NULL,  'V',
	N_("Display software version"), NULL },
  { "license", 'L',	POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	NULL,  'L',
	N_("Display softwre license"), NULL },

  POPT_TABLEEND
};

/*@unchecked@*/ /*@observer@*/
static struct poptOption optionsTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
	rpmzqArgCallback, 0, NULL, NULL },
/*@=type@*/

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmzPrivatePoptTable, 0,
        N_("\
  rpmpigz will compress files in place, adding the suffix '.gz'. If no files are\n\
  specified, stdin will be compressed to stdout.  rpmpigz does what gzip does,\n\
  but spreads the work over multiple processors and cores when compressing.\n\
\n\
Options:\
"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmzqOptionsPoptTable, 0,
        N_("Compression options: "), NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options:"),
	NULL },

  POPT_AUTOALIAS
  POPT_AUTOHELP

#ifdef	DYING
  { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
	N_("\
Usage: rpmpigz [options] [files ...]\n\
  rpmpigz will compress files in place, adding the suffix '.gz'. If no files are\n\
  specified, stdin will be compressed to stdout.  rpmpigz does what gzip does,\n\
  but spreads the work over multiple processors and cores when compressing.\n\
"), NULL },
#endif

  POPT_TABLEEND
};

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
    rpmzQueue zq = _rpmzq;
    poptContext optCon;
    int ac;
    int rc = 1;		/* assume failure */
    int xx;
    int i;

/*@-observertrans -readonlytrans @*/
    __progname = "rpmpigz";
/*@=observertrans =readonlytrans @*/
    zq->_job.in = &zq->_job_in;
    z->zq = zq;		/* XXX initialize rpmzq */

    /* XXX sick hack to initialize the popt callback. */
    rpmzqOptionsPoptTable[0].arg = (void *)&rpmzqArgCallback;

    /* prepare for interrupts and logging */
    signal(SIGINT, rpmzAbort);

#ifndef _PIGZNOTHREAD
    yarnPrefix = __progname;	/* prefix for yarn error messages */
    yarnAbort = rpmzAbort;	/* call on thread error */
#endif
    /* XXX add POPT_ARG_TIMEOFDAY oneshot? */
    gettimeofday(&z->start, NULL);  /* starting time for log entries */
#if defined(DEBUG) || defined(__LCLINT__)
    zq->zlog = rpmzLogInit(&z->start);/* initialize logging */
#endif

    /* set all options to defaults */
    rpmzDefaults(zq);

    /* process user environment variable defaults */
    if (rpmzParseEnv(z, "GZIP", optionsTable))
        goto exit;

    optCon = rpmioInit(argc, argv, optionsTable);

    /* Add files from CLI. */
    {	ARGV_t av = poptGetArgs(optCon);
	if (av != NULL)
	    xx = argvAppend(&z->argv, av);
    }

    /* Add files from --files manifest(s). */
    if (z->manifests != NULL)
        xx = rpmzLoadManifests(z);

if (_debug)
argvPrint("input args", z->argv, NULL);

    ac = argvCount(z->argv);
    /* if no command line arguments and stdout is a terminal, show help */
    if ((z->argv == NULL || z->argv[0] == NULL) && isatty(STDOUT_FILENO)) {
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }

    /* process command-line arguments */
    if (z->argv == NULL || z->argv[0] == NULL) {
	/* list stdin or compress stdin to stdout if no file names provided */
	rpmzProcess(z, NULL);
    } else {
	for (i = 0; i < ac; i++) {
	    if (i == 1 && F_ISSET(zq->flags, STDOUT) && zq->mode == RPMZ_MODE_COMPRESS && !F_ISSET(zq->flags, LIST) && (zq->format == RPMZ_FORMAT_ZIP2 || zq->format == RPMZ_FORMAT_ZIP3)) {
		fprintf(stderr, "warning: output is concatenated zip files ");
		fprintf(stderr, "-- %s will not be able to extract\n", __progname);
	    }
	    rpmzProcess(z, strcmp(z->argv[i], "-") ? (char *)z->argv[i] : NULL);
	}
    }

    rc = 0;

    /* done -- release resources, show log */
exit:
    rpmzNewOpts(z);
    z->zq->zlog = rpmzLogDump(z->zq->zlog, NULL);

    z->manifests = argvFree(z->manifests);
    z->argv = argvFree(z->argv);
#ifdef	NOTYET
    z->iob = rpmiobFree(z->iob);
#endif

    optCon = rpmioFini(optCon);
    
/*@-observertrans@*/	/* XXX __progname silliness */
    return rc;
/*@=observertrans@*/
}

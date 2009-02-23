#ifndef	H_RPMZ
#define	H_RPMZ

/**
 * \file rpmio/rpmz.h
 * RPM parallel compressor internals.
 */

#include <rpmiotypes.h>
#include <rpmio.h>
#include <rpmlog.h>
#include <argv.h>

#include <rpmsq.h>
/*@-bufferoverflowhigh -mustfreeonly@*/
#include <poptIO.h>
/*@=bufferoverflowhigh =mustfreeonly@*/
#if !defined(POPT_ARGFLAG_TOGGLE)	/* XXX compat with popt < 1.15 */
#define	POPT_ARGFLAG_TOGGLE	0
#endif

#ifndef _PIGZNOTHREAD
#  include <yarn.h>     /* yarnThread, yarnLaunch(), yarnJoin(), yarnJoinAll(), */
                        /* yarnLock, yarnNewLock(), yarnPossess(), yarnTwist(), yarnWaitFor(),
                           yarnRelease(), yarnPeekLock(), yarnFreeLock(), yarnPrefix */
#endif

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

#define _KFB(n) (1U << (n))
#define _ZFB(n) (_KFB(n) | 0x40000000)

/**
 */
enum rpmzFlags_e {
    RPMZ_FLAGS_NONE		= 0,
    RPMZ_FLAGS_STDOUT		= _ZFB( 0),	/*!< -c, --stdout ... */
    RPMZ_FLAGS_FORCE		= _ZFB( 1),	/*!< -f, --force ... */
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

/**
 */
typedef struct rpmz_s * rpmz;
typedef struct rpmzSpace_s * rpmzSpace;
typedef struct rpmzPool_s * rpmzPool;
typedef struct rpmzJob_s * rpmzJob;

#if defined(_RPMZ_INTERNAL)
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
    rpmzSpace in;		/*!< input data to compress */
    rpmzSpace out;		/*!< dictionary or resulting compressed data */
    unsigned long check;	/*!< check value for input data */
    yarnLock calc;		/*!< released when check calculation complete */
/*@dependent@*/ /*@null@*/
    rpmzJob next;		/*!< next job in the list (either list) */
};

/**
 */
struct rpmz_s {
    enum rpmzFlags_e flags;	/*!< Control bits. */
    enum rpmzFormat_e format;	/*!< Compression format. */
    enum rpmzMode_e mode;	/*!< Operation mode. */
    unsigned int level;		/*!< Compression level. */

    unsigned int threads;	/*!< No. or threads to use. */
/*@null@*/ /*@observer@*/
    const char * suffix;	/*!< -S, --suffix ... */

    char _ifn[PATH_MAX+1];	/*!< input file name (accommodate recursion) */

#if defined(_RPMZ_INTERNAL_XZ)
    rpmuint64_t mem;		/*!< Physical memory. */
    rpmuint64_t memlimit_encoder;
    rpmuint64_t memlimit_decoder;
    rpmuint64_t memlimit_custom;

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

    int _auto_adjust;
    enum rpmzFormat_e _format_compress_auto;
    lzma_options_lzma _options;
    lzma_check _checksum;
    lzma_filter _filters[LZMA_FILTERS_MAX + 1];
    size_t _filters_count;
#endif	/* _RPMZ_INTERNAL_XZ */

#if defined(_RPMZ_INTERNAL_PIGZ)	/* PIGZ specific configuration. */
    int verbosity;        /*!< 0 = quiet, 1 = normal, 2 = verbose, 3 = trace */

/* --- globals (modified by main thread only when it's the only thread) */
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
#endif	/* _RPMZ_INTERNAL_PIGZ */

};
#endif	/* _RPMZ_INTERNAL */

/*==============================================================*/

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_RPMZ_INTERNAL)
/*@unchecked@*/
extern struct rpmz_s __rpmz;

/*@unchecked@*/
extern rpmz _rpmz;

/**
 */
extern void rpmzArgCallback(poptContext con,
		/*@unused@*/ enum poptCallbackReason reason,
		const struct poptOption * opt, /*@unused@*/ const char * arg,
		/*@unused@*/ void * data)
	/*@globals _rpmz, fileSystem, internalState @*/
	/*@modifies _rpmz, fileSystem, internalState @*/;

/*==============================================================*/

/*@unchecked@*/ /*@observer@*/
static struct poptOption rpmzOptionsPoptTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
	rpmzArgCallback, 0, NULL, NULL },
/*@=type@*/

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

#if defined(_RPMZ_INTERNAL_PIGZ)
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
#endif	/* _RPMZ_INTERNAL_PIGZ */
#if defined(_RPMZ_INTERNAL_XZ)
  /* XXX -T collides with pigz -T,--no-time */
  { "threads", '\0', POPT_ARG_INT|POPT_ARGFLAG_SHOW_DEFAULT,	&__rpmz.threads, 0,
	N_("Allow up to n compression threads"), N_("n") },
#endif	/* _RPMZ_INTERNAL_XZ */

  /* ===== Operation modes */
#if defined(_RPMZ_INTERNAL_XZ)
  { "compress", 'z', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmz.mode, RPMZ_MODE_COMPRESS,
	N_("force compression"), NULL },
  { "uncompress", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmz.mode, RPMZ_MODE_DECOMPRESS,
	N_("force decompression"), NULL },
#endif	/* _RPMZ_INTERNAL_XZ */
  { "decompress", 'd', POPT_ARG_VAL,		&__rpmz.mode, RPMZ_MODE_DECOMPRESS,
	N_("Decompress the compressed input"), NULL },
  { "test", 't', POPT_ARG_VAL,			&__rpmz.mode,  RPMZ_MODE_TEST,
	N_("Test the integrity of the compressed input"), NULL },
  { "list", 'l', POPT_BIT_SET,			&__rpmz.flags,  RPMZ_FLAGS_LIST,
	N_("List the contents of the compressed input"), NULL },
#if defined(_RPMZ_INTERNAL_XZ)
  { "info", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmz.flags,  RPMZ_FLAGS_LIST,
	N_("list block sizes, total sizes, and possible metadata"), NULL },
#endif	/* _RPMZ_INTERNAL_XZ */
  { "force", 'f', POPT_BIT_SET,			&__rpmz.flags,  RPMZ_FLAGS_FORCE,
	N_("Force overwrite, compress .gz, links, and to terminal"), NULL },

  /* ===== Operation modifiers */
  { "recursive", 'r', POPT_BIT_SET,	&__rpmz.flags, RPMZ_FLAGS_RECURSE,
	N_("Process the contents of all subdirectories"), NULL },
  { "suffix", 'S', POPT_ARG_STRING,		&__rpmz.suffix, 0,
	N_("Use suffix .sss instead of .gz (for compression)"), N_(".sss") },
#if defined(_RPMZ_INTERNAL_PIGZ)
  { "ascii", 'a', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	NULL, 'a',
	N_("Compress to LZW (.Z) instead of gzip format"), NULL },
  { "bits", 'Z', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	NULL, 'Z',
	N_("Compress to LZW (.Z) instead of gzip format"), NULL },
  { "zlib", 'z', POPT_ARG_VAL,		&__rpmz.format, RPMZ_FORMAT_ZLIB,
	N_("Compress to zlib (.zz) instead of gzip format"), NULL },
  { "zip", 'K', POPT_ARG_VAL,		&__rpmz.format, RPMZ_FORMAT_ZIP2,
	N_("Compress to PKWare zip (.zip) single entry format"), NULL },
#endif	/* _RPMZ_INTERNAL_PIGZ */
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
#if defined(_RPMZ_INTERNAL_PIGZ)
  { "quiet", 'q',	POPT_ARG_VAL,				NULL,  'q',
	N_("Print no messages, even on error"), NULL },
  { "verbose", 'v',	POPT_ARG_VAL,				NULL,  'v',
	N_("Provide more verbose output"), NULL },
  { "version", 'V',	POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	NULL,  'V',
	N_("?version?"), NULL },
  { "license", 'L',	POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	NULL,  'L',
	N_("?license?"), NULL },
#endif	/* _RPMZ_INTERNAL_PIGZ */

  POPT_TABLEEND
};
#endif	/* _RPMZ_INTERNAL */

#if defined(_RPMZ_INTERNAL)
/**
 */
static rpmRC rpmzParseEnv(/*@unused@*/ rpmz z, /*@null@*/ const char * envvar,
		struct poptOption optionsTable[])
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
    } else	/* Check no args were in the envvar. */
    if (argvCount(poptGetArgs(optCon))) {
	fprintf(stderr, "%s: cannot provide files in %s envvar\n",
		 __progname, (envvar ? envvar : _envvar));
	rc = RPMRC_FAIL;
    }

exit:
    if (optCon)
	optCon = poptFreeContext(optCon);
    av = argvFree(av);
    return rc;
}
#endif	/* _RPMZ_INTERNAL */

#if defined(_RPMZ_INTERNAL_XZ)
/**
 */
static rpmRC rpmzParseArgv0(rpmz z, const char * argv0)
	/*@globals fileSystem, internalState @*/
	/*@modifies z, fileSystem, internalState @*/
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
#endif	/* _RPMZ_INTERNAL_XZ */

#ifdef __cplusplus
}
#endif

#endif

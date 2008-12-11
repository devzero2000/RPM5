/** \ingroup rpmio
 * \file rpmio/lzdio.c
 * Support for LZMA compression library.
 */

#include "system.h"
#include "rpmio_internal.h"
#include <rpmmacro.h>
#include <rpmcb.h>

#if defined(HAVE_LZMA_H)

/* provide necessary defines for inclusion of <lzma.h>
   similar to LZMAUtils's internal <common.h> and as
   explicitly stated in the top-level comment of <lzma.h> */
#ifndef UINT32_C
#	define UINT32_C(n) n ## U
#endif
#ifndef UINT32_MAX
#	define UINT32_MAX UINT32_C(4294967295)
#endif
#if SIZEOF_UNSIGNED_LONG == 4
#	ifndef UINT64_C
#		define UINT64_C(n) n ## ULL
#	endif
#else
#	ifndef UINT64_C
#		define UINT64_C(n) n ## UL
#	endif
#endif
#ifndef UINT64_MAX
#	define UINT64_MAX UINT64_C(18446744073709551615)
#endif

#include "lzma.h"

#include "debug.h"

/*@access FD_t @*/

#define	LZDONLY(fd)	assert(fdGetIo(fd) == lzdio)

#define kBufferSize (1 << 15)

typedef struct lzfile {
/*@only@*/
    rpmuint8_t buf[kBufferSize];	/*!< IO buffer */
    lzma_stream strm;		/*!< LZMA stream */
/*@dependent@*/
    FILE * fp;
    int encoding;
    int eof;
} LZFILE;

/*@-globstate@*/
/*@null@*/
static LZFILE *lzopen_internal(const char *path, const char *mode, int fd)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    int level = 5;
    int encoding = 0;
    FILE *fp;
    LZFILE *lzfile;
    lzma_stream tmp;
    lzma_ret ret;

    for (; *mode != '\0'; mode++) {
	if (*mode == 'w')
	    encoding = 1;
	else if (*mode == 'r')
	    encoding = 0;
	else if (*mode >= '1' && *mode <= '9')
	    level = (int)(*mode - '0');
    }
    if (fd != -1)
	fp = fdopen(fd, encoding ? "w" : "r");
    else
	fp = fopen(path, encoding ? "w" : "r");
    if (!fp)
	return NULL;
    lzfile = calloc(1, sizeof(*lzfile));
    if (!lzfile) {
	(void) fclose(fp);
	return NULL;
    }
    lzfile->fp = fp;
    lzfile->encoding = encoding;
    lzfile->eof = 0;
#if LZMA_VERSION == 49990030
    tmp = LZMA_STREAM_INIT_VAR;
#else
    tmp = LZMA_STREAM_INIT;
#endif
    lzfile->strm = tmp;
    if (encoding) {
#if LZMA_VERSION == 49990030
	lzma_options_alone options;
/*@-unrecog@*/
	options.uncompressed_size = LZMA_VLI_VALUE_UNKNOWN;
/*@=unrecog@*/
	memcpy(&options.lzma, &lzma_preset_lzma[level - 1], sizeof(options.lzma));
#else
	lzma_options_lzma options;
	lzma_lzma_preset(&options, level);
#endif
	ret = lzma_alone_encoder(&lzfile->strm, &options);
    } else {
#if LZMA_VERSION == 49990030
	ret = lzma_auto_decoder(&lzfile->strm, NULL, 0);
#else
	/* FIXME: second argument now sets memory limit, setting it to
	 * '-1' means unlimited and isn't really recommended. A sane
	 * default value when setting it to '0' will probably be
	 * implemented in liblzma soon, so then we should switch
	 * back to '0'.
	 */
	ret = lzma_auto_decoder(&lzfile->strm, -1, 0);
#endif
    }
    if (ret != LZMA_OK) {
	(void) fclose(fp);
	memset(lzfile, 0, sizeof(*lzfile));
	free(lzfile);
	return NULL;
    }
    return lzfile;
}
/*@=globstate@*/

/*@null@*/
static LZFILE *lzopen(const char *path, const char *mode)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    return lzopen_internal(path, mode, -1);
}

/*@null@*/
static LZFILE *lzdopen(int fd, const char *mode)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    if (fd < 0)
	return NULL;
    return lzopen_internal(0, mode, fd);
}

static int lzflush(LZFILE *lzfile)
	/*@globals fileSystem @*/
	/*@modifies lzfile, fileSystem @*/
{
    return fflush(lzfile->fp);
}

static int lzclose(/*@only@*/ LZFILE *lzfile)
	/*@globals fileSystem @*/
	/*@modifies *lzfile, fileSystem @*/
{
    lzma_ret ret;
    size_t n;
    int rc;

    if (!lzfile)
	return -1;
    if (lzfile->encoding) {
	for (;;) {
	    lzfile->strm.avail_out = kBufferSize;
	    lzfile->strm.next_out = (uint8_t *)lzfile->buf;
	    ret = lzma_code(&lzfile->strm, LZMA_FINISH);
	    if (ret != LZMA_OK && ret != LZMA_STREAM_END)
		return -1;
	    n = kBufferSize - lzfile->strm.avail_out;
	    if (n && fwrite(lzfile->buf, 1, n, lzfile->fp) != n)
		return -1;
	    if (ret == LZMA_STREAM_END)
		break;
	}
    }
    lzma_end(&lzfile->strm);
    rc = fclose(lzfile->fp);
    memset(lzfile, 0, sizeof(*lzfile));
    free(lzfile);
    return rc;
}

/*@-mustmod@*/
static ssize_t lzread(LZFILE *lzfile, void *buf, size_t len)
	/*@globals fileSystem @*/
	/*@modifies lzfile, *buf, fileSystem @*/
{
    lzma_ret ret;
    int eof = 0;

    if (!lzfile || lzfile->encoding)
      return -1;
    if (lzfile->eof)
      return 0;
/*@-temptrans@*/
    lzfile->strm.next_out = buf;
/*@=temptrans@*/
    lzfile->strm.avail_out = len;
    for (;;) {
	if (!lzfile->strm.avail_in) {
	    lzfile->strm.next_in = (uint8_t *)lzfile->buf;
	    lzfile->strm.avail_in = fread(lzfile->buf, 1, kBufferSize, lzfile->fp);
	    if (!lzfile->strm.avail_in)
		eof = 1;
	}
	ret = lzma_code(&lzfile->strm, LZMA_RUN);
	if (ret == LZMA_STREAM_END) {
	    lzfile->eof = 1;
	    return len - lzfile->strm.avail_out;
	}
	if (ret != LZMA_OK)
	    return -1;
	if (!lzfile->strm.avail_out)
	    return len;
	if (eof)
	    return -1;
      }
    /*@notreached@*/
}
/*@=mustmod@*/

static ssize_t lzwrite(LZFILE *lzfile, void *buf, size_t len)
	/*@globals fileSystem @*/
	/*@modifies lzfile, fileSystem @*/
{
    lzma_ret ret;
    size_t n;

    if (!lzfile || !lzfile->encoding)
	return -1;
    if (!len)
	return 0;
/*@-temptrans@*/
    lzfile->strm.next_in = buf;
/*@=temptrans@*/
    lzfile->strm.avail_in = len;
    for (;;) {
	lzfile->strm.next_out = (uint8_t *)lzfile->buf;
	lzfile->strm.avail_out = kBufferSize;
	ret = lzma_code(&lzfile->strm, LZMA_RUN);
	if (ret != LZMA_OK)
	    return -1;
	n = kBufferSize - lzfile->strm.avail_out;
	if (n && fwrite(lzfile->buf, 1, n, lzfile->fp) != n)
	    return -1;
	if (!lzfile->strm.avail_in)
	    return len;
    }
    /*@notreached@*/
}

/* =============================================================== */

static inline /*@dependent@*/ /*@null@*/ void * lzdFileno(FD_t fd)
	/*@*/
{
    void * rc = NULL;
    int i;

    FDSANE(fd);
    for (i = fd->nfps; i >= 0; i--) {
/*@-boundsread@*/
	    FDSTACK_t * fps = &fd->fps[i];
/*@=boundsread@*/
	    if (fps->io != lzdio)
		continue;
	    rc = fps->fp;
	break;
    }
    
    return rc;
}

/*@-globuse@*/
static /*@null@*/ FD_t lzdOpen(const char * path, const char * fmode)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    FD_t fd;
    mode_t mode = (fmode && fmode[0] == 'w' ? O_WRONLY : O_RDONLY);
    LZFILE * lzfile = lzopen(path, fmode);

    if (lzfile == NULL)
	return NULL;
    fd = fdNew("open (lzdOpen)");
    fdPop(fd); fdPush(fd, lzdio, lzfile, -1);
    fdSetOpen(fd, path, fileno(lzfile->fp), mode);
    return fdLink(fd, "lzdOpen");
}
/*@=globuse@*/

/*@-globuse@*/
static /*@null@*/ FD_t lzdFdopen(void * cookie, const char * fmode)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    FD_t fd = c2f(cookie);
    int fdno = fdFileno(fd);
    LZFILE *lzfile;

assert(fmode != NULL);
    fdSetFdno(fd, -1);          /* XXX skip the fdio close */
    if (fdno < 0) return NULL;
    lzfile = lzdopen(fdno, fmode);
    if (lzfile == NULL) return NULL;
    fdPush(fd, lzdio, lzfile, fdno);
    return fdLink(fd, "lzdFdopen");
}
/*@=globuse@*/

/*@-globuse@*/
static int lzdFlush(void * cookie)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    FD_t fd = c2f(cookie);
    return lzflush(lzdFileno(fd));
}
/*@=globuse@*/

/* =============================================================== */
/*@-globuse@*/
/*@-mustmod@*/          /* LCL: *buf is modified */
static ssize_t lzdRead(void * cookie, /*@out@*/ char * buf, size_t count)
	/*@globals fileSystem, internalState @*/
	/*@modifies *buf, fileSystem, internalState @*/
{
    FD_t fd = c2f(cookie);
    LZFILE *lzfile;
    ssize_t rc = -1;

assert(fd != NULL);
    if (fd->bytesRemain == 0) return 0; /* XXX simulate EOF */
    lzfile = lzdFileno(fd);
assert(lzfile != NULL);
    fdstat_enter(fd, FDSTAT_READ);
/*@-compdef@*/
    rc = lzread(lzfile, buf, count);
/*@=compdef@*/
DBGIO(fd, (stderr, "==>\tlzdRead(%p,%p,%u) rc %lx %s\n", cookie, buf, (unsigned)count, (unsigned long)rc, fdbg(fd)));
    if (rc == -1) {
	fd->errcookie = "Lzma: decoding error";
    } else if (rc >= 0) {
	fdstat_exit(fd, FDSTAT_READ, rc);
	/*@-compdef@*/
	if (fd->ndigests && rc > 0) fdUpdateDigests(fd, (void *)buf, rc);
	/*@=compdef@*/
    }
    return rc;
}
/*@=mustmod@*/
/*@=globuse@*/

/*@-globuse@*/
static ssize_t lzdWrite(void * cookie, const char * buf, size_t count)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    FD_t fd = c2f(cookie);
    LZFILE *lzfile;
    ssize_t rc = 0;

    if (fd == NULL || fd->bytesRemain == 0) return 0;   /* XXX simulate EOF */

    if (fd->ndigests && count > 0) fdUpdateDigests(fd, (void *)buf, count);

    lzfile = lzdFileno(fd);

    fdstat_enter(fd, FDSTAT_WRITE);
    rc = lzwrite(lzfile, (void *)buf, count);
DBGIO(fd, (stderr, "==>\tlzdWrite(%p,%p,%u) rc %lx %s\n", cookie, buf, (unsigned)count, (unsigned long)rc, fdbg(fd)));
    if (rc < 0) {
	fd->errcookie = "Lzma: encoding error";
    } else if (rc > 0) {
	fdstat_exit(fd, FDSTAT_WRITE, rc);
    }
    return rc;
}

static inline int lzdSeek(void * cookie, /*@unused@*/ _libio_pos_t pos,
			/*@unused@*/ int whence)
	/*@*/
{
    FD_t fd = c2f(cookie);

    LZDONLY(fd);
    return -2;
}

static int lzdClose( /*@only@*/ void * cookie)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    FD_t fd = c2f(cookie);
    LZFILE *lzfile;
    const char * errcookie;
    int rc;

    lzfile = lzdFileno(fd);

    if (lzfile == NULL) return -2;
    errcookie = strerror(ferror(lzfile->fp));

    fdstat_enter(fd, FDSTAT_CLOSE);
    /*@-dependenttrans@*/
    rc = lzclose(lzfile);
    /*@=dependenttrans@*/
    fdstat_exit(fd, FDSTAT_CLOSE, rc);

    if (fd && rc == -1)
	fd->errcookie = errcookie;

DBGIO(fd, (stderr, "==>\tlzdClose(%p) rc %lx %s\n", cookie, (unsigned long)rc, fdbg(fd)));

    if (_rpmio_debug || rpmIsDebug()) fdstat_print(fd, "LZDIO", stderr);
    /*@-branchstate@*/
    if (rc == 0)
	fd = fdFree(fd, "open (lzdClose)");
    /*@=branchstate@*/
    return rc;
}

/*@-type@*/ /* LCL: function typedefs */
static struct FDIO_s lzdio_s = {
  lzdRead, lzdWrite, lzdSeek, lzdClose, lzdOpen, lzdFdopen, lzdFlush,
};
/*@=type@*/

FDIO_t lzdio = /*@-compmempass@*/ &lzdio_s /*@=compmempass@*/ ;

#endif


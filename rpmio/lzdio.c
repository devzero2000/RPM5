/* Support for LZMA library.
 */

#include "system.h"
#include "rpmio_internal.h"
#include <rpmmacro.h>
#include <rpmcb.h>

#include <lzma.h>

#include "debug.h"

#define	LZDONLY(fd)	assert(fdGetIo(fd) == lzdio)

#define kBufferSize (1 << 15)

typedef struct lzfile {
  /* IO buffer */
    uint8_t buf[kBufferSize];

    lzma_stream strm;

    FILE *fp;

    int encoding;
    int eof;

} LZFILE;

static LZFILE *lzopen_internal(const char *path, const char *mode, int fd)
{
    int level = 5;
    int encoding = 0;
    FILE *fp;
    LZFILE *lzfile;
    lzma_ret ret;

    for (; *mode; mode++) {
	if (*mode == 'w')
	    encoding = 1;
	else if (*mode == 'r')
	    encoding = 0;
	else if (*mode >= '1' && *mode <= '9')
	    level = *mode - '0';
    }
    if (fd != -1)
	fp = fdopen(fd, encoding ? "w" : "r");
    else
	fp = fopen(path, encoding ? "w" : "r");
    if (!fp)
	return 0;
    lzfile = calloc(1, sizeof(*lzfile));
    if (!lzfile) {
	fclose(fp);
	return 0;
    }
    lzfile->fp = fp;
    lzfile->encoding = encoding;
    lzfile->eof = 0;
    lzfile->strm = LZMA_STREAM_INIT_VAR;
    if (encoding) {
	lzma_options_alone alone;
	alone.uncompressed_size = LZMA_VLI_VALUE_UNKNOWN;
	memcpy(&alone.lzma, &lzma_preset_lzma[level - 1], sizeof(alone.lzma));
	ret = lzma_alone_encoder(&lzfile->strm, &alone);
    } else {
	ret = lzma_auto_decoder(&lzfile->strm, 0, 0);
    }
    if (ret != LZMA_OK) {
	fclose(fp);
	free(lzfile);
	return 0;
    }
    return lzfile;
}

static LZFILE *lzopen(const char *path, const char *mode)
{
    return lzopen_internal(path, mode, -1);
}

static LZFILE *lzdopen(int fd, const char *mode)
{
    if (fd < 0)
	return 0;
    return lzopen_internal(0, mode, fd);
}

#ifdef	UNUSED
static int lzflush(LZFILE *lzfile)
{
    return fflush(lzfile->fp);
}
#endif

static int lzclose(LZFILE *lzfile)
{
    lzma_ret ret;
    size_t n;

    if (!lzfile)
	return -1;
    if (lzfile->encoding) {
	for (;;) {
	    lzfile->strm.avail_out = kBufferSize;
	    lzfile->strm.next_out = lzfile->buf;
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
    return fclose(lzfile->fp);
    free(lzfile);
}

static ssize_t lzread(LZFILE *lzfile, void *buf, size_t len)
{
    lzma_ret ret;
    int eof = 0;

    if (!lzfile || lzfile->encoding)
      return -1;
    if (lzfile->eof)
      return 0;
    lzfile->strm.next_out = buf;
    lzfile->strm.avail_out = len;
    for (;;) {
	if (!lzfile->strm.avail_in) {
	    lzfile->strm.next_in = lzfile->buf;
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
}

static ssize_t lzwrite(LZFILE *lzfile, void *buf, size_t len)
{
    lzma_ret ret;
    size_t n;

    if (!lzfile || !lzfile->encoding)
	return -1;
    if (!len)
	return 0;
    lzfile->strm.next_in = buf;
    lzfile->strm.avail_in = len;
    for (;;) {
	lzfile->strm.next_out = lzfile->buf;
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
}

/* =============================================================== */

static inline /*@dependent@*/ void * lzdFileno(FD_t fd)
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

#ifdef	UNUSED
/*@-globuse@*/
static int lzdFlush(FD_t fd)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    return lzflush(lzdFileno(fd));
}
/*@=globuse@*/
#endif

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
    int rc;

    lzfile = lzdFileno(fd);

    if (lzfile == NULL) return -2;
    fdstat_enter(fd, FDSTAT_CLOSE);
    /*@-dependenttrans@*/
    rc = lzclose(lzfile);
    /*@=dependenttrans@*/

    /* XXX TODO: preserve fd if errors */

    if (fd) {
	if (rc == -1) {
	    fd->errcookie = strerror(ferror(lzfile->fp));
	} else if (rc >= 0) {
	    fdstat_exit(fd, FDSTAT_CLOSE, rc);
	}
    }

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
  lzdRead, lzdWrite, lzdSeek, lzdClose, lzdOpen, lzdFdopen,
};
/*@=type@*/

FDIO_t lzdio = /*@-compmempass@*/ &lzdio_s /*@=compmempass@*/ ;

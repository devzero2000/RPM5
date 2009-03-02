/** \ingroup rpmio
 * \file rpmio/bzdio.c
 * Support for BZIP2 compression library.
 */

#include "system.h"
#include "rpmio_internal.h"
#include <rpmmacro.h>
#include <rpmcb.h>

#if defined(HAVE_BZLIB_H)

#include <bzlib.h>

#if defined(__LCLINT__)
/*@-incondefs =protoparammatch@*/
/*@-exportheader@*/

BZ_EXTERN BZFILE* BZ_API(BZ2_bzReadOpen) (
/*@out@*/
      int*  bzerror,
      FILE* f,
      int   verbosity,
      int   small,
/*@out@*/
      void* unused,
      int   nUnused
   )
	/*@modifies *bzerror, f @*/;

BZ_EXTERN void BZ_API(BZ2_bzReadClose) (
/*@out@*/
      int*    bzerror,
      BZFILE* b
   )
	/*@modifies *bzerror, b @*/;

BZ_EXTERN void BZ_API(BZ2_bzReadGetUnused) (
/*@out@*/
      int*    bzerror,
      BZFILE* b,
/*@out@*/
      void**  unused,
      int*    nUnused
   )
	/*@modifies *bzerror, b, *unused, *nUnused @*/;

BZ_EXTERN int BZ_API(BZ2_bzRead) (
/*@out@*/
      int*    bzerror,
      BZFILE* b,
/*@out@*/
      void*   buf,
      int     len
   )
	/*@modifies *bzerror, b, *buf @*/;

/*@=exportheader@*/
/*@=incondefs =protoparammatch@*/
#endif	/* __LCLINT__ */

#include "debug.h"

/*@access FD_t @*/

#define	BZDONLY(fd)	assert(fdGetIo(fd) == bzdio)

typedef	struct rpmbz_s {
    BZFILE *bzfile;	
    int bzerr;
    FILE * fp;		/*!< file pointer */
    int B;		/*!< blockSize100K (default: 9) */
    int S;		/*!< small (default: 0) */
    int V;		/*!< verboisty (default: 0) */
    int W;		/*!< workFactor (default: 30) */
} * rpmbz;

/*@unchecked@*/
static int _bzdB = 9;
/*@unchecked@*/
static int _bzdS = 0;
/*@unchecked@*/
static int _bzdV = 1;
/*@unchecked@*/
static int _bzdW = 30;

/* =============================================================== */

/*@-moduncon@*/

static inline /*@dependent@*/ /*@null@*/ void * bzdFileno(FD_t fd)
	/*@*/
{
    void * rc = NULL;
    int i;

    FDSANE(fd);
    for (i = fd->nfps; i >= 0; i--) {
	FDSTACK_t * fps = &fd->fps[i];
	if (fps->io != bzdio)
	    continue;
	rc = fps->fp;
	break;
    }

    return rc;
}

/*@-globuse@*/
static /*@null@*/ FD_t bzdOpen(const char * path, const char * fmode)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    FD_t fd;
    BZFILE *bzfile;
    mode_t mode = (fmode && fmode[0] == 'w' ? O_WRONLY : O_RDONLY);

    if ((bzfile = BZ2_bzopen(path, fmode)) == NULL)
	return NULL;
    fd = fdNew("open (bzdOpen)");
    fdPop(fd); fdPush(fd, bzdio, bzfile, -1);
    fdSetOpen(fd, path, -1, mode);
    return fdLink(fd, "bzdOpen");
}
/*@=globuse@*/

/*@-globuse@*/
static /*@null@*/ FD_t bzdFdopen(void * cookie, const char * fmode)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    FD_t fd = c2f(cookie);
    int fdno;
    BZFILE *bzfile;

    if (fmode == NULL) return NULL;
    fdno = fdFileno(fd);
    fdSetFdno(fd, -1);		/* XXX skip the fdio close */
    if (fdno < 0) return NULL;
    bzfile = BZ2_bzdopen(fdno, fmode);
    if (bzfile == NULL) return NULL;

    fdPush(fd, bzdio, bzfile, fdno);		/* Push bzdio onto stack */

    return fdLink(fd, "bzdFdopen");
}
/*@=globuse@*/

/*@-globuse@*/
static int bzdFlush(void * cookie)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    FD_t fd = c2f(cookie);
    return BZ2_bzflush(bzdFileno(fd));
}
/*@=globuse@*/

/* =============================================================== */
/*@-globuse@*/
/*@-mustmod@*/		/* LCL: *buf is modified */
static ssize_t bzdRead(void * cookie, /*@out@*/ char * buf, size_t count)
	/*@globals fileSystem, internalState @*/
	/*@modifies *buf, fileSystem, internalState @*/
{
    FD_t fd = c2f(cookie);
    BZFILE *bzfile;
    ssize_t rc = 0;

    if (fd->bytesRemain == 0) return 0;	/* XXX simulate EOF */
    bzfile = bzdFileno(fd);
    fdstat_enter(fd, FDSTAT_READ);
    if (bzfile)
	/*@-compdef@*/
	rc = BZ2_bzread(bzfile, buf, (int)count);
	/*@=compdef@*/
    if (rc == -1) {
	int zerror = 0;
	if (bzfile)
	    fd->errcookie = BZ2_bzerror(bzfile, &zerror);
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
static ssize_t bzdWrite(void * cookie, const char * buf, size_t count)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    FD_t fd = c2f(cookie);
    BZFILE *bzfile;
    ssize_t rc;

    if (fd->bytesRemain == 0) return 0;	/* XXX simulate EOF */

    if (fd->ndigests && count > 0) fdUpdateDigests(fd, (void *)buf, count);

    bzfile = bzdFileno(fd);
    fdstat_enter(fd, FDSTAT_WRITE);
    rc = BZ2_bzwrite(bzfile, (void *)buf, (int)count);
    if (rc == -1) {
	int zerror = 0;
	fd->errcookie = BZ2_bzerror(bzfile, &zerror);
    } else if (rc > 0) {
	fdstat_exit(fd, FDSTAT_WRITE, rc);
    }
    return rc;
}
/*@=globuse@*/

static inline int bzdSeek(void * cookie, /*@unused@*/ _libio_pos_t pos,
			/*@unused@*/ int whence)
	/*@*/
{
    FD_t fd = c2f(cookie);

    BZDONLY(fd);
    return -2;
}

static int bzdClose( /*@only@*/ void * cookie)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    FD_t fd = c2f(cookie);
    BZFILE *bzfile;
    int rc;

    bzfile = bzdFileno(fd);

    if (bzfile == NULL) return -2;
    fdstat_enter(fd, FDSTAT_CLOSE);
    /*@-noeffectuncon@*/ /* FIX: check rc */
    BZ2_bzclose(bzfile);
    /*@=noeffectuncon@*/
    rc = 0;	/* XXX FIXME */

    /* XXX TODO: preserve fd if errors */

    if (fd) {
	if (rc == -1) {
	    int zerror = 0;
	    fd->errcookie = BZ2_bzerror(bzfile, &zerror);
	} else if (rc >= 0) {
	    fdstat_exit(fd, FDSTAT_CLOSE, rc);
	}
    }

DBGIO(fd, (stderr, "==>\tbzdClose(%p) rc %lx %s\n", cookie, (unsigned long)rc, fdbg(fd)));

    if (_rpmio_debug || rpmIsDebug()) fdstat_print(fd, "BZDIO", stderr);
    if (rc == 0)
	fd = fdFree(fd, "open (bzdClose)");
    return rc;
}

/*@-type@*/ /* LCL: function typedefs */
static struct FDIO_s bzdio_s = {
  bzdRead, bzdWrite, bzdSeek, bzdClose,	bzdOpen, bzdFdopen, bzdFlush,
};
/*@=type@*/

FDIO_t bzdio = /*@-compmempass@*/ &bzdio_s /*@=compmempass@*/ ;

/*@=moduncon@*/
#endif


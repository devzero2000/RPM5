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
   /*@only@*/
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

BZ_EXTERN BZFILE* BZ_API(BZ2_bzWriteOpen) (
      int*  bzerror,
      FILE* f,
      int   blockSize100k,
      int   verbosity,
      int   workFactor
   )
	/*@modifies *bzerror @*/;

BZ_EXTERN void BZ_API(BZ2_bzWrite) (
   /*@out@*/
      int*    bzerror,
      BZFILE* b,
      void*   buf,
      int     len
   )
	/*@modifies *bzerror, b @*/;

BZ_EXTERN void BZ_API(BZ2_bzWriteClose) (
   /*@out@*/
      int*          bzerror,
   /*@only@*/
      BZFILE*       b,
      int           abandon,
   /*@out@*/
      unsigned int* nbytes_in,
   /*@out@*/
      unsigned int* nbytes_out
   )
	/*@modifies *bzerror, b, *nbytes_in, *nbytes_out @*/;

BZ_EXTERN int BZ_API(BZ2_bzflush) (
      BZFILE* b
   )
	/*@modifies b @*/;

BZ_EXTERN const char * BZ_API(BZ2_bzerror) (
      BZFILE *b,
   /*@out@*/
      int    *errnum
   );

/*@=exportheader@*/
/*@=incondefs =protoparammatch@*/
#endif	/* __LCLINT__ */

#include "debug.h"

/*@access FD_t @*/

#define	BZDONLY(fd)	assert(fdGetIo(fd) == bzdio)

typedef	struct rpmbz_s {
    BZFILE *bzfile;	
    int bzerr;
    int omode;		/*!< open mode: O_RDONLY | O_WRONLY */
    FILE * fp;		/*!< file pointer */
    int B;		/*!< blockSize100K (default: 9) */
    int S;		/*!< small (default: 0) */
    int V;		/*!< verboisty (default: 0) */
    int W;		/*!< workFactor (default: 30) */
    unsigned int nbytes_in;
    unsigned int nbytes_out;
} * rpmbz;

/*@unchecked@*/
static int _bzdB = 9;
/*@unchecked@*/
static int _bzdS = 0;
/*@unchecked@*/
static int _bzdV = 1;
/*@unchecked@*/
static int _bzdW = 30;

static void rpmbzClose(rpmbz bz, int abort)
	/*@modifies bz @*/
{
    if (bz->bzfile != NULL) {
	if (bz->omode == O_RDONLY)
	    BZ2_bzReadClose(&bz->bzerr, bz->bzfile);
	else
	    BZ2_bzWriteClose(&bz->bzerr, bz->bzfile, abort,
		&bz->nbytes_in, &bz->nbytes_out);
    }
    bz->bzfile = NULL;
}

/*@only@*/
static rpmbz rpmbzFree(/*@only@*/ rpmbz bz, int abort)
	/*@modifies bz @*/
{
    rpmbzClose(bz, abort);
    if (bz->fp != NULL) {
	(void) fclose(bz->fp);
	bz->fp = NULL;
    }
    bz = _free(bz);
    return NULL;
}

/*@only@*/
static rpmbz rpmbzNew(const char * path, const char * fmode, int fdno)
	/*@*/
{
    rpmbz bz;
    const char * s = fmode;
    char stdio[20];
    char *t = stdio;
    char *te = t + sizeof(stdio) - 2;
    int c;

assert(fmode != NULL);		/* XXX return NULL instead? */
    bz = xcalloc(1, sizeof(*bz));
    bz->B = _bzdB;
    bz->S = _bzdS;
    bz->V = _bzdV;
    bz->W = _bzdW;

    switch ((c = *s++)) {
    case 'a':
    case 'w':
	bz->omode = O_WRONLY;
	*t++ = (char)c;
	break;
    case 'r':
	bz->omode = O_RDONLY;
	*t++ = (char)c;
	break;
    }
	
    while ((c = *s++) != 0)
    switch (c) {
    case '.':
	break;
    case '+':
    case 'x':
    case 'm':
    case 'c':
    case 'b':
	if (t < te) *t++ = c;
	break;
    default:
	if (xisdigit(c))
	    bz->B = c - (int)'0';
	break;
    }
    *t = '\0';

    if (fdno >= 0) {
	if ((bz->fp = fdopen(fdno, stdio)) != NULL)
	    bz->bzfile = (bz->omode == O_RDONLY)
		? BZ2_bzReadOpen(&bz->bzerr, bz->fp, bz->V, bz->S, NULL, 0)
		: BZ2_bzWriteOpen(&bz->bzerr, bz->fp, bz->B, bz->V, bz->W);
    } else if (path != NULL) {
	if ((bz->fp = fopen(path, stdio)) != NULL)
	    bz->bzfile = (bz->omode == O_RDONLY)
		? BZ2_bzReadOpen(&bz->bzerr, bz->fp, bz->V, bz->S, NULL, 0)
		: BZ2_bzWriteOpen(&bz->bzerr, bz->fp, bz->B, bz->V, bz->W);
    }

    return (bz->bzfile != NULL ? bz : rpmbzFree(bz, 0));
}

/* =============================================================== */

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
    rpmbz bz = rpmbzNew(path, fmode, -1);

    if (bz == NULL)
	return NULL;
    fd = fdNew("open (bzdOpen)");
#ifdef	NOTYET
    fdPop(fd); fdPush(fd, bzdio, bz, fileno(bz->fp));
#else
    fdPop(fd); fdPush(fd, bzdio, bz, -1);
#endif
    fdSetOpen(fd, path, -1, bz->omode);
    return fdLink(fd, "bzdOpen");
}
/*@=globuse@*/

/*@-globuse@*/
static /*@null@*/ FD_t bzdFdopen(void * cookie, const char * fmode)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    FD_t fd = c2f(cookie);
    int fdno = fdFileno(fd);
    rpmbz bz = rpmbzNew(NULL, fmode, fdno);

    if (bz == NULL)
	return NULL;
#ifdef	NOTYET
    fdPop(fd); fdPush(fd, bzdio, bz, fileno(bz->fp));
#else
    fdSetFdno(fd, -1);		/* XXX skip the fdio close */
    fdPush(fd, bzdio, bz, fdno);		/* Push bzdio onto stack */
#endif
    return fdLink(fd, "bzdFdopen");
}
/*@=globuse@*/

/*@-globuse@*/
static int bzdFlush(void * cookie)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    FD_t fd = c2f(cookie);
    int rc;
    rc = BZ2_bzflush(bzdFileno(fd));
    return rc;
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
    rpmbz bz = bzdFileno(fd);
    ssize_t rc = 0;

assert(bz != NULL);
    if (fd->bytesRemain == 0) return 0;	/* XXX simulate EOF */
    fdstat_enter(fd, FDSTAT_READ);
    if (bz->bzfile != NULL) {
	rc = BZ2_bzRead(&bz->bzerr, bz->bzfile, buf, (int)count);
	switch (bz->bzerr) {
	case BZ_STREAM_END: {
	    void * unused = NULL;
	    int nUnused = 0;
	    
	    BZ2_bzReadGetUnused(&bz->bzerr, bz->bzfile, &unused, &nUnused);
	    if (unused != NULL && nUnused > 0)
		unused = memcpy(xmalloc(nUnused), unused, nUnused);
	    else {
		unused = NULL;
		nUnused = 0;
	    }
	    rpmbzClose(bz, 0);
	    bz->bzfile = BZ2_bzReadOpen(&bz->bzerr, bz->fp, bz->V, bz->S,
			unused, nUnused);
	    unused = _free(unused);
	}   /*@fallthrough@*/
	case BZ_OK:
	    break;
	default:
	    rc = -1;	/* XXX just in case. */
	    fd->errcookie = BZ2_bzerror(bz->bzfile, &bz->bzerr);
	    rpmbzClose(bz, 1);
	    break;
	}
    }
    if (rc >= 0) {
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
    rpmbz bz = bzdFileno(fd);
    ssize_t rc;

assert(bz != NULL);
    if (fd->bytesRemain == 0) return 0;	/* XXX simulate EOF */

    if (fd->ndigests && count > 0) fdUpdateDigests(fd, (void *)buf, count);

    fdstat_enter(fd, FDSTAT_WRITE);
    BZ2_bzWrite(&bz->bzerr, bz->bzfile, (void *)buf, (int)count);
    switch (bz->bzerr) {
    case BZ_OK:
	rc = count;
	break;
    default:
	rc = -1;
	fd->errcookie = BZ2_bzerror(bz->bzfile, &bz->bzerr);
	rpmbzClose(bz, 1);
	break;
    }
    if (rc >= 0)
	fdstat_exit(fd, FDSTAT_WRITE, rc);
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
    rpmbz bz = bzdFileno(fd);
    int rc;

assert(bz != NULL);
    if (bz->bzfile == NULL)	/* XXX memory leak w errors? */
	return -2;

    fdstat_enter(fd, FDSTAT_CLOSE);
    /*@-noeffectuncon@*/ /* FIX: check rc */
    rpmbzClose(bz, 0);
    /*@=noeffectuncon@*/
    rc = 0;	/* XXX FIXME */

    /* XXX TODO: preserve fd if errors */

    if (fd) {
	if (rc == -1) {
	    fd->errcookie = BZ2_bzerror(bz->bzfile, &bz->bzerr);
	} else
	if (rc >= 0) {
	    fdstat_exit(fd, FDSTAT_CLOSE, rc);
	}
    }

DBGIO(fd, (stderr, "==>\tbzdClose(%p) rc %lx %s\n", cookie, (unsigned long)rc, fdbg(fd)));

    if (_rpmio_debug || rpmIsDebug()) fdstat_print(fd, "BZDIO", stderr);

    if (rc == 0) {
	bz = rpmbzFree(bz, 0);
	fd = fdFree(fd, "open (bzdClose)");
    }
    return rc;
}

/*@-type@*/ /* LCL: function typedefs */
static struct FDIO_s bzdio_s = {
  bzdRead, bzdWrite, bzdSeek, bzdClose,	bzdOpen, bzdFdopen, bzdFlush,
};
/*@=type@*/

FDIO_t bzdio = /*@-compmempass@*/ &bzdio_s /*@=compmempass@*/ ;

#endif


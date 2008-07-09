/** \ingroup rpmio
 * \file rpmio/gzdio.c
 * Support for ZLIB compression library.
 */

#include "system.h"
#include "rpmio_internal.h"
#include <rpmmacro.h>
#include <rpmcb.h>

#ifdef	HAVE_ZLIB_H

/*@-noparams@*/
#include <zlib.h>
/*@=noparams@*/

#include "debug.h"

/*@access FD_t @*/

#define	GZDONLY(fd)	assert(fdGetIo(fd) == gzdio)

typedef struct rpmGZFILE_s {
    gzFile gz;			/* gzFile is a pointer */
} * rpmGZFILE;			/* like FILE, to use with star */

/* =============================================================== */
/*@-moduncon@*/

static inline /*@dependent@*/ /*@null@*/ void * gzdFileno(FD_t fd)
	/*@*/
{
    void * rc = NULL;
    int i;

    FDSANE(fd);
    for (i = fd->nfps; i >= 0; i--) {
	FDSTACK_t * fps = &fd->fps[i];
	if (fps->io != gzdio)
	    continue;
	rc = fps->fp;
	break;
    }

    return rc;
}

static /*@null@*/
FD_t gzdOpen(const char * path, const char * fmode)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    FD_t fd;
    rpmGZFILE rpmgz;
    mode_t mode = (fmode && fmode[0] == 'w' ? O_WRONLY : O_RDONLY);

    rpmgz = calloc(1, sizeof(*rpmgz));
    if (rpmgz == NULL)
	return NULL;
    rpmgz->gz = gzopen(path, fmode);
    if (rpmgz->gz == NULL) {
	rpmgz = _free(rpmgz);
	return NULL;
    }
    fd = fdNew("open (gzdOpen)");
    fdPop(fd); fdPush(fd, gzdio, rpmgz, mode);
    fdSetOpen(fd, path, -1, mode);

DBGIO(fd, (stderr, "==>\tgzdOpen(\"%s\", \"%s\") fd %p %s\n", path, fmode, (fd ? fd : NULL), fdbg(fd)));
    return fdLink(fd, "gzdOpen");
}

static /*@null@*/ FD_t gzdFdopen(void * cookie, const char *fmode)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    FD_t fd = c2f(cookie);
    int fdno;
    rpmGZFILE rpmgz;

    if (fmode == NULL) return NULL;
    fdno = fdFileno(fd);
    fdSetFdno(fd, -1);		/* XXX skip the fdio close */
    if (fdno < 0) return NULL;
    rpmgz = calloc(1, sizeof(*rpmgz));
    if (rpmgz == NULL)
	return NULL;
    rpmgz->gz = gzdopen(fdno, fmode);
    if (rpmgz->gz == NULL) {
	rpmgz = _free(rpmgz);
	return NULL;
    }

    fdPush(fd, gzdio, rpmgz, fdno);		/* Push gzdio onto stack */

    return fdLink(fd, "gzdFdopen");
}

static int gzdFlush(void * cookie)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    FD_t fd = c2f(cookie);
    rpmGZFILE rpmgz;
    rpmgz = gzdFileno(fd);
    if (rpmgz == NULL) return -2;
    return gzflush(rpmgz->gz, Z_SYNC_FLUSH);	/* XXX W2DO? */
}

/* =============================================================== */
static ssize_t gzdRead(void * cookie, /*@out@*/ char * buf, size_t count)
	/*@globals fileSystem, internalState @*/
	/*@modifies buf, fileSystem, internalState @*/
{
    FD_t fd = c2f(cookie);
    rpmGZFILE rpmgz;
    ssize_t rc;

    if (fd == NULL || fd->bytesRemain == 0) return 0;	/* XXX simulate EOF */

    rpmgz = gzdFileno(fd);
    if (rpmgz == NULL) return -2;	/* XXX can't happen */

    fdstat_enter(fd, FDSTAT_READ);
    rc = gzread(rpmgz->gz, buf, (unsigned)count);
DBGIO(fd, (stderr, "==>\tgzdRead(%p,%p,%u) rc %lx %s\n", cookie, buf, (unsigned)count, (unsigned long)rc, fdbg(fd)));
    if (rc < 0) {
	int zerror = 0;
	fd->errcookie = gzerror(rpmgz->gz, &zerror);
	if (zerror == Z_ERRNO) {
	    fd->syserrno = errno;
	    fd->errcookie = strerror(fd->syserrno);
	}
    } else if (rc >= 0) {
	fdstat_exit(fd, FDSTAT_READ, rc);
	if (fd->ndigests && rc > 0) fdUpdateDigests(fd, (void *)buf, rc);
    }
    return rc;
}

static ssize_t gzdWrite(void * cookie, const char * buf, size_t count)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    FD_t fd = c2f(cookie);
    rpmGZFILE rpmgz;
    ssize_t rc;

    if (fd == NULL || fd->bytesRemain == 0) return 0;	/* XXX simulate EOF */

    if (fd->ndigests && count > 0) fdUpdateDigests(fd, (void *)buf, count);

    rpmgz = gzdFileno(fd);
    if (rpmgz == NULL) return -2;	/* XXX can't happen */

    fdstat_enter(fd, FDSTAT_WRITE);
    rc = gzwrite(rpmgz->gz, (void *)buf, (unsigned)count);
DBGIO(fd, (stderr, "==>\tgzdWrite(%p,%p,%u) rc %lx %s\n", cookie, buf, (unsigned)count, (unsigned long)rc, fdbg(fd)));
    if (rc < 0) {
	int zerror = 0;
	fd->errcookie = gzerror(rpmgz->gz, &zerror);
	if (zerror == Z_ERRNO) {
	    fd->syserrno = errno;
	    fd->errcookie = strerror(fd->syserrno);
	}
    } else if (rc > 0) {
	fdstat_exit(fd, FDSTAT_WRITE, rc);
    }
    return rc;
}

/* XXX zlib-1.0.4 has not */
#define	HAVE_GZSEEK	/* XXX autoFu doesn't set this anymore. */
static inline int gzdSeek(void * cookie, _libio_pos_t pos, int whence)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    int rc;
#if defined(HAVE_GZSEEK)
#ifdef USE_COOKIE_SEEK_POINTER
    _IO_off64_t p = *pos;
#else
    off_t p = pos;
#endif
    FD_t fd = c2f(cookie);
    rpmGZFILE rpmgz;

    if (fd == NULL) return -2;
    assert(fd->bytesRemain == -1);	/* XXX FIXME */

    rpmgz = gzdFileno(fd);
    if (rpmgz == NULL) return -2;	/* XXX can't happen */

    fdstat_enter(fd, FDSTAT_SEEK);
    rc = gzseek(rpmgz->gz, (long)p, whence);
DBGIO(fd, (stderr, "==>\tgzdSeek(%p,%ld,%d) rc %lx %s\n", cookie, (long)p, whence, (unsigned long)rc, fdbg(fd)));
    if (rc < 0) {
	int zerror = 0;
	fd->errcookie = gzerror(rpmgz->gz, &zerror);
	if (zerror == Z_ERRNO) {
	    fd->syserrno = errno;
	    fd->errcookie = strerror(fd->syserrno);
	}
    } else if (rc >= 0) {
	fdstat_exit(fd, FDSTAT_SEEK, rc);
    }
#else
    rc = -2;
#endif
    return rc;
}

static int gzdClose( /*@only@*/ void * cookie)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    FD_t fd = c2f(cookie);
    rpmGZFILE rpmgz;
    int rc;

    rpmgz = gzdFileno(fd);
    if (rpmgz == NULL) return -2;	/* XXX can't happen */

    fdstat_enter(fd, FDSTAT_CLOSE);
    /*@-dependenttrans@*/
    rc = gzclose(rpmgz->gz);
    /*@=dependenttrans@*/
    rpmgz = _free(rpmgz);

    /* XXX TODO: preserve fd if errors */

    if (fd) {
DBGIO(fd, (stderr, "==>\tgzdClose(%p) zerror %d %s\n", cookie, rc, fdbg(fd)));
	if (rc < 0) {
	    fd->errcookie = "gzclose error";
	    if (rc == Z_ERRNO) {
		fd->syserrno = errno;
		fd->errcookie = strerror(fd->syserrno);
	    }
	} else if (rc >= 0) {
	    fdstat_exit(fd, FDSTAT_CLOSE, rc);
	}
    }

DBGIO(fd, (stderr, "==>\tgzdClose(%p) rc %lx %s\n", cookie, (unsigned long)rc, fdbg(fd)));

    if (_rpmio_debug || rpmIsDebug()) fdstat_print(fd, "GZDIO", stderr);
    if (rc == 0)
	fd = fdFree(fd, "open (gzdClose)");
    return rc;
}

/*@-type@*/ /* LCL: function typedefs */
static struct FDIO_s gzdio_s = {
  gzdRead, gzdWrite, gzdSeek, gzdClose,	gzdOpen, gzdFdopen, gzdFlush,
};
/*@=type@*/

FDIO_t gzdio = /*@-compmempass@*/ &gzdio_s /*@=compmempass@*/ ;

/*@=moduncon@*/
#endif	/* HAVE_ZLIB_H */

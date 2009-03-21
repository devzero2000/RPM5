#ifndef	H_RPMIO_INTERNAL
#define	H_RPMIO_INTERNAL

/** \ingroup rpmio
 * \file rpmio/rpmio_internal.h
 */

#include <rpmiotypes.h>
#include <rpmlog.h>
#include <rpmio.h>
#include <rpmurl.h>

#define	_RPMPGP_INTERNAL
#include <rpmpgp.h>

#include <rpmxar.h>

/** \ingroup rpmio
 */
typedef struct _FDSTACK_s {
/*@exposed@*/
    FDIO_t		io;
/*@dependent@*/
    void *		fp;
    int			fdno;
} FDSTACK_t;

/** \ingroup rpmio
 * Identify per-desciptor I/O operation statistics.
 */
typedef enum fdOpX_e {
    FDSTAT_READ		= 0,	/*!< Read statistics index. */
    FDSTAT_WRITE	= 1,	/*!< Write statistics index. */
    FDSTAT_SEEK		= 2,	/*!< Seek statistics index. */
    FDSTAT_CLOSE	= 3,	/*!< Close statistics index */
    FDSTAT_DIGEST	= 4,	/*!< Digest statistics index. */
    FDSTAT_MAX		= 5
} fdOpX;

/** \ingroup rpmio
 * Cumulative statistics for a descriptor.
 */
typedef	/*@abstract@*/ struct {
    struct rpmop_s	ops[FDSTAT_MAX];	/*!< Cumulative statistics. */
} * FDSTAT_t;

/** \ingroup rpmio
 */
typedef struct _FDDIGEST_s {
    pgpHashAlgo		hashalgo;
    DIGEST_CTX		hashctx;
} * FDDIGEST_t;

/** \ingroup rpmio
 * The FD_t File Handle data structure.
 */
struct _FD_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
    int		flags;
#define	RPMIO_DEBUG_IO		0x40000000
#define	RPMIO_DEBUG_REFS	0x20000000
    int		magic;
#define	FDMAGIC			0x04463138
    int		nfps;
    FDSTACK_t	fps[8];
    int		urlType;	/* ufdio: */

/*@dependent@*/
    void *	url;		/* ufdio: URL info */
/*@relnull@*/
    void *	req;		/* ufdio: HTTP request */

    int		rd_timeoutsecs;	/* ufdRead: per FD_t timer */
    ssize_t	bytesRemain;	/* ufdio: */
    ssize_t	contentLength;	/* ufdio: */
    int		persist;	/* ufdio: */
    int		wr_chunked;	/* ufdio: */

    int		syserrno;	/* last system errno encountered */
/*@observer@*/
    const void *errcookie;	/* gzdio/bzdio/ufdio: */

/*null@*/
    const char *opath;		/* open(2) args. */
    int		oflags;
    mode_t	omode;

/*@refcounted@*/ /*@relnull@*/
    rpmxar	xar;		/* xar archive wrapper */
/*@refcounted@*/ /*@relnull@*/
    pgpDig	dig;		/* signature parameters */

    FDSTAT_t	stats;		/* I/O statistics */

    int		ndigests;
#define	FDDIGEST_MAX	32
    struct _FDDIGEST_s	digests[FDDIGEST_MAX];

/*null@*/
    const char *contentType;	/* ufdio: (HTTP) */
/*null@*/
    const char *contentDisposition;	/* ufdio: (HTTP) */
    time_t	lastModified;	/* ufdio: (HTTP) */
    int		ftpFileDoneNeeded; /* ufdio: (FTP) */
    unsigned long long	fd_cpioPos;	/* cpio: */
};
/*@access FD_t@*/

#define	FDSANE(fd)	assert(fd != NULL && fd->magic == FDMAGIC)

/*@-redecl@*/
/*@unchecked@*/
extern int _rpmio_debug;
/*@=redecl@*/

/*@-redecl@*/
/*@unchecked@*/
extern int _av_debug;
/*@=redecl@*/

/*@-redecl@*/
/*@unchecked@*/
extern int _ftp_debug;
/*@=redecl@*/

/*@-redecl@*/
/*@unchecked@*/
extern int _dav_debug;
/*@=redecl@*/

#define DBG(_f, _m, _x) \
    /*@-modfilesys@*/ \
    if ((_rpmio_debug | ((_f) ? ((FD_t)(_f))->flags : 0)) & (_m)) fprintf _x \
    /*@=modfilesys@*/

#if defined(__LCLINT__XXX)
#define DBGIO(_f, _x)
#define DBGREFS(_f, _x)
#else
#define DBGIO(_f, _x)   DBG((_f), RPMIO_DEBUG_IO, _x)
#define DBGREFS(_f, _x) DBG((_f), RPMIO_DEBUG_REFS, _x)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmio
 */
/*@observer@*/ const char * fdbg(/*@null@*/ FD_t fd)
        /*@*/;

/** \ingroup rpmio
 */
int fdFgets(FD_t fd, char * buf, size_t len)
	/*@globals errno, fileSystem @*/
	/*@modifies *buf, fd, errno, fileSystem @*/;

/** \ingroup rpmio
 */
/*@null@*/ FD_t ftpOpen(const char *url, /*@unused@*/ int flags,
                /*@unused@*/ mode_t mode, /*@out@*/ urlinfo *uret)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies *uret, fileSystem, internalState @*/;

/** \ingroup rpmio
 */
int ftpReq(FD_t data, const char * ftpCmd, const char * ftpArg)
	/*@globals fileSystem, internalState @*/
	/*@modifies data, fileSystem, internalState @*/;

/** \ingroup rpmio
 */
int ftpCmd(const char * cmd, const char * url, const char * arg2)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

/** \ingroup rpmio
 */
int ufdClose( /*@only@*/ void * cookie)
	/*@globals fileSystem, internalState @*/
	/*@modifies cookie, fileSystem, internalState @*/;

/** \ingroup rpmio
 */
/*@unused@*/ static inline
void fdSetOpen(FD_t fd, const char * path, int flags, mode_t mode)
	/*@modifies fd @*/
{
    FDSANE(fd);
    if (fd->opath != NULL) {
	free((void *)fd->opath);
	fd->opath = NULL;
    }
    fd->opath = xstrdup(path);
    fd->oflags = flags;
    fd->omode = mode;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
/*@null@*/ /*@observer@*/ const char * fdGetOPath(FD_t fd)
	/*@*/
{
    FDSANE(fd);
    return fd->opath;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
int fdGetOFlags(FD_t fd)
	/*@*/
{
    FDSANE(fd);
    return fd->oflags;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
mode_t fdGetOMode(FD_t fd)
	/*@*/
{
    FDSANE(fd);
    return fd->omode;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
void fdSetDig(FD_t fd, pgpDig dig)
	/*@modifies fd, dig @*/
{
    FDSANE(fd);
    fd->dig = pgpDigFree(fd->dig, "fdSetDig");
    fd->dig = pgpDigLink(dig, "fdSetDig");
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
/*@null@*/ pgpDig fdGetDig(FD_t fd)
	/*@*/
{
    FDSANE(fd);
    /*@-compdef -retexpose -refcounttrans -usereleased @*/
    return fd->dig;
    /*@=compdef =retexpose =refcounttrans =usereleased @*/
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
void fdSetXAR(FD_t fd, rpmxar xar)
	/*@modifies fd, xar @*/
{
    FDSANE(fd);
    fd->xar = rpmxarLink(xar, "fdSetXAR");
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
/*@null@*/ rpmxar fdGetXAR(FD_t fd)
	/*@*/
{
    FDSANE(fd);
    /*@-compdef -refcounttrans -retexpose -usereleased @*/
    return fd->xar;
    /*@=compdef =refcounttrans =retexpose =usereleased @*/
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
/*@null@*/ FDIO_t fdGetIo(FD_t fd)
	/*@*/
{
    FDSANE(fd);
    return fd->fps[fd->nfps].io;
}

/** \ingroup rpmio
 */
/*@-nullstate@*/ /* FIX: io may be NULL */
/*@unused@*/ static inline
void fdSetIo(FD_t fd, /*@kept@*/ /*@null@*/ FDIO_t io)
	/*@modifies fd @*/
{
    FDSANE(fd);
    /*@-assignexpose@*/
    fd->fps[fd->nfps].io = io;
    /*@=assignexpose@*/
}
/*@=nullstate@*/

/** \ingroup rpmio
 */
/*@unused@*/ static inline
/*@exposed@*/ /*@dependent@*/ /*@null@*/ FILE * fdGetFILE(FD_t fd)
	/*@*/
{
    FDSANE(fd);
    /*@+voidabstract@*/
    return ((FILE *)fd->fps[fd->nfps].fp);
    /*@=voidabstract@*/
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
/*@exposed@*/ /*@dependent@*/ /*@null@*/ void * fdGetFp(FD_t fd)
	/*@*/
{
    FDSANE(fd);
    return fd->fps[fd->nfps].fp;
}

/** \ingroup rpmio
 */
/*@-nullstate@*/ /* FIX: fp may be NULL */
/*@unused@*/ static inline
void fdSetFp(FD_t fd, /*@kept@*/ /*@null@*/ void * fp)
	/*@modifies fd @*/
{
    FDSANE(fd);
    /*@-assignexpose@*/
    fd->fps[fd->nfps].fp = fp;
    /*@=assignexpose@*/
}
/*@=nullstate@*/

/** \ingroup rpmio
 */
/*@unused@*/ static inline
int fdGetFdno(FD_t fd)
	/*@*/
{
    FDSANE(fd);
    return fd->fps[fd->nfps].fdno;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
void fdSetFdno(FD_t fd, int fdno)
	/*@modifies fd @*/
{
    FDSANE(fd);
    fd->fps[fd->nfps].fdno = fdno;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
void fdSetContentLength(FD_t fd, ssize_t contentLength)
	/*@modifies fd @*/
{
    FDSANE(fd);
    fd->contentLength = fd->bytesRemain = contentLength;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
void fdPush(FD_t fd, FDIO_t io, void * fp, int fdno)
	/*@modifies fd @*/
{
    FDSANE(fd);
    if (fd->nfps >= (int)(sizeof(fd->fps)/sizeof(fd->fps[0]) - 1))
	return;
    fd->nfps++;
    fdSetIo(fd, io);
    fdSetFp(fd, fp);
    fdSetFdno(fd, fdno);
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
void fdPop(FD_t fd)
	/*@modifies fd @*/
{
    FDSANE(fd);
    if (fd->nfps < 0) return;
    fdSetIo(fd, NULL);
    fdSetFp(fd, NULL);
    fdSetFdno(fd, -1);
    fd->nfps--;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline /*@null@*/
rpmop fdstat_op(/*@null@*/ FD_t fd, fdOpX opx)
	/*@*/
{
    rpmop op = NULL;

    if (fd != NULL && fd->stats != NULL && (int)opx >= 0 && opx < FDSTAT_MAX)
        op = fd->stats->ops + opx;
    return op;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
void fdstat_enter(/*@null@*/ FD_t fd, int opx)
	/*@globals internalState @*/
	/*@modifies internalState @*/
{
    if (fd == NULL) return;
    if (fd->stats != NULL)
	(void) rpmswEnter(fdstat_op(fd, opx), 0);
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
void fdstat_exit(/*@null@*/ FD_t fd, int opx, ssize_t rc)
	/*@globals internalState @*/
	/*@modifies fd, internalState @*/
{
    if (fd == NULL) return;
    if (rc == -1)
	fd->syserrno = errno;
    else if (rc > 0 && fd->bytesRemain > 0)
	switch (opx) {
	case FDSTAT_READ:
	case FDSTAT_WRITE:
	fd->bytesRemain -= rc;
	    break;
	default:
	    break;
	}
    if (fd->stats != NULL)
	(void) rpmswExit(fdstat_op(fd, opx), rc);
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
void fdstat_print(/*@null@*/ FD_t fd, const char * msg, FILE * fp)
	/*@globals fileSystem @*/
	/*@modifies *fp, fileSystem @*/
{
    static int usec_scale = (1000*1000);
    int opx;

    if (fd == NULL || fd->stats == NULL) return;
    for (opx = 0; opx < 4; opx++) {
	rpmop op = &fd->stats->ops[opx];
	if (op->count <= 0) continue;
	switch (opx) {
	case FDSTAT_READ:
	    if (msg != NULL) fprintf(fp, "%s:", msg);
	    fprintf(fp, "%8d reads, %8lu total bytes in %d.%06d secs\n",
		op->count, (unsigned long)op->bytes,
		(int)(op->usecs/usec_scale), (int)(op->usecs%usec_scale));
	    /*@switchbreak@*/ break;
	case FDSTAT_WRITE:
	    if (msg != NULL) fprintf(fp, "%s:", msg);
	    fprintf(fp, "%8d writes, %8lu total bytes in %d.%06d secs\n",
		op->count, (unsigned long)op->bytes,
		(int)(op->usecs/usec_scale), (int)(op->usecs%usec_scale));
	    /*@switchbreak@*/ break;
	case FDSTAT_SEEK:
	    /*@switchbreak@*/ break;
	case FDSTAT_CLOSE:
	    /*@switchbreak@*/ break;
	}
    }
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
void fdSetSyserrno(FD_t fd, int syserrno, /*@kept@*/ const void * errcookie)
	/*@modifies fd @*/
{
    FDSANE(fd);
    fd->syserrno = syserrno;
    /*@-assignexpose@*/
    fd->errcookie = errcookie;
    /*@=assignexpose@*/
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
int fdGetRdTimeoutSecs(FD_t fd)
	/*@*/
{
    FDSANE(fd);
    return fd->rd_timeoutsecs;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
unsigned long long fdGetCpioPos(FD_t fd)
	/*@*/
{
    FDSANE(fd);
    return fd->fd_cpioPos;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
void fdSetCpioPos(FD_t fd, long int cpioPos)
	/*@modifies fd @*/
{
    FDSANE(fd);
    fd->fd_cpioPos = cpioPos;
}

/** \ingroup rpmio
 */
/*@mayexit@*/ /*@unused@*/ static inline
FD_t c2f(/*@null@*/ void * cookie)
	/*@*/
{
    /*@-castexpose@*/
    FD_t fd = (FD_t) cookie;
    /*@=castexpose@*/
    FDSANE(fd);
    /*@-refcounttrans -retalias@*/ return fd; /*@=refcounttrans =retalias@*/
}

/** \ingroup rpmio
 * Attach digest to fd.
 */
/*@unused@*/ static inline
void fdInitDigest(FD_t fd, pgpHashAlgo hashalgo, int flags)
	/*@globals internalState @*/
	/*@modifies fd, internalState @*/
{
    FDDIGEST_t fddig = fd->digests + fd->ndigests;
    if (fddig != (fd->digests + FDDIGEST_MAX)) {
	fd->ndigests++;
	fddig->hashalgo = hashalgo;
	fdstat_enter(fd, FDSTAT_DIGEST);
	fddig->hashctx = rpmDigestInit(hashalgo, flags);
	fdstat_exit(fd, FDSTAT_DIGEST, 0);
    }
}

/** \ingroup rpmio
 * Update digest(s) attached to fd.
 */
/*@unused@*/ static inline
void fdUpdateDigests(FD_t fd, const unsigned char * buf, ssize_t buflen)
	/*@globals internalState @*/
	/*@modifies fd, internalState @*/
{
    int i;

    if (buf != NULL && buflen > 0)
    for (i = fd->ndigests - 1; i >= 0; i--) {
	FDDIGEST_t fddig = fd->digests + i;
	if (fddig->hashctx == NULL)
	    continue;
	fdstat_enter(fd, FDSTAT_DIGEST);
	(void) rpmDigestUpdate(fddig->hashctx, buf, buflen);
	fdstat_exit(fd, FDSTAT_DIGEST, buflen);
    }
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
void fdFiniDigest(FD_t fd, pgpHashAlgo hashalgo,
		/*@null@*/ /*@out@*/ void * datap,
		/*@null@*/ /*@out@*/ size_t * lenp,
		int asAscii)
	/*@globals internalState @*/
	/*@modifies fd, *datap, *lenp, internalState @*/
{
    int imax = -1;
    int i;

    for (i = fd->ndigests - 1; i >= 0; i--) {
	FDDIGEST_t fddig = fd->digests + i;
	if (fddig->hashctx == NULL)
	    continue;
	if (i > imax) imax = i;
	if (fddig->hashalgo != hashalgo)
	    continue;
	fdstat_enter(fd, FDSTAT_DIGEST);
	(void) rpmDigestFinal(fddig->hashctx, datap, lenp, asAscii);
	fdstat_exit(fd, FDSTAT_DIGEST, 0);
	fddig->hashctx = NULL;
	break;
    }
    if (i < 0) {
	if (datap != NULL) *(void **)datap = NULL;
	if (lenp != NULL) *lenp = 0;
    }

    fd->ndigests = imax;
    if (i < imax)
	fd->ndigests++;		/* convert index to count */
}

/** \ingroup rpmio
 */
/*@-mustmod@*/
/*@unused@*/ static inline
void fdStealDigest(FD_t fd, pgpDig dig)
	/*@modifies fd, dig @*/
{
    int i;
/*@-type@*/	/* FIX: getters for pgpDig internals */
    for (i = fd->ndigests - 1; i >= 0; i--) {
	FDDIGEST_t fddig = fd->digests + i;
	if (fddig->hashctx != NULL)
	switch (fddig->hashalgo) {
	case PGPHASHALGO_MD5:
assert(dig->md5ctx == NULL);
/*@-onlytrans@*/
	    dig->md5ctx = fddig->hashctx;
/*@=onlytrans@*/
	    fddig->hashctx = NULL;
	    /*@switchbreak@*/ break;
	case PGPHASHALGO_SHA1:
	case PGPHASHALGO_RIPEMD160:
#if defined(HAVE_BEECRYPT_API_H)
	case PGPHASHALGO_SHA256:
	case PGPHASHALGO_SHA384:
	case PGPHASHALGO_SHA512:
#endif
assert(dig->sha1ctx == NULL);
/*@-onlytrans@*/
	    dig->sha1ctx = fddig->hashctx;
/*@=onlytrans@*/
	    fddig->hashctx = NULL;
	    /*@switchbreak@*/ break;
	default:
	    /*@switchbreak@*/ break;
	}
    }
/*@=type@*/
}
/*@=mustmod@*/

/*@-shadow@*/
/** \ingroup rpmio
 */
/*@unused@*/ static inline
int fdFileno(/*@null@*/ void * cookie)
	/*@*/
{
    FD_t fd;
    if (cookie == NULL) return -2;
    fd = c2f(cookie);
    return fd->fps[0].fdno;
}
/*@=shadow@*/

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMIO_INTERNAL */

#ifndef RPMDAV_H
#define RPMDAV_H

/** \ingroup rpmio
 * \file rpmio/rpmdav.h
 */

#include <argv.h>

/**
*/
/*@unchecked@*/
extern int rpmioHttpReadTimeoutSecs;
/*@unchecked@*/
extern int rpmioHttpConnectTimeoutSecs;
/*@unchecked@*/ /*@null@*/
extern const char * rpmioHttpAccept;
/*@unchecked@*/ /*@null@*/
extern const char * rpmioHttpUserAgent;

#if defined(_RPMDAV_INTERNAL)
struct __dirstream {
    int fd;			/* File descriptor.  */
    char * data;		/* Directory block.  */
    size_t allocation;		/* Space allocated for the block.  */
    size_t size;		/* Total valid data in the block.  */
    size_t offset;		/* Current offset into the block.  */
    off_t filepos;		/* Position of next entry to read.  */
#if defined(WITH_PTHREADS)
    pthread_mutex_t lock;	/* Mutex lock for this structure.  */
#endif
};
#endif

#if !defined(DT_DIR) || defined(__APPLE__)
# define DT_UNKNOWN	0
# define DT_FIFO	1
# define DT_CHR		2
# define DT_DIR		4
# define DT_BLK		6
# define DT_REG		8
# define DT_LNK		10
# define DT_SOCK	12
# define DT_WHT		14
typedef struct __dirstream *	AVDIR;
typedef struct __dirstream *	DAVDIR;
#else
# if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
typedef struct __dirstream *	AVDIR;
typedef struct __dirstream *	DAVDIR;
#else	/* __FreeBSD__ */
typedef DIR *			AVDIR;
typedef DIR *			DAVDIR;
#endif	/* __FreeBSD__ */
#endif


#ifdef __cplusplus
extern "C" {
#endif

#if defined(_RPMAV_INTERNAL)
/**
 */
typedef struct avContext_s * avContext;

/**
 */
/*@unchecked@*/
extern int avmagicdir;
#define ISAVMAGIC(_dir) (!memcmp((_dir), &avmagicdir, sizeof(avmagicdir)))

/**
 */
struct avContext_s {
/*@relnull@*/ /*@dependent@*/
    void ** resrock;
    const char *uri;
/*@refcounted@*/
    urlinfo u;
    int ac;
    int nalloced;
    ARGV_t av;
/*@relnull@*/ /*@shared@*/
    struct stat *st;
    rpmuint16_t * modes;	/* XXX sizeof(mode_t) != sizeof(rpmmode_t) */
    size_t * sizes;
    time_t * mtimes;
};

/**
 */
/*@null@*/
void * avContextDestroy(/*@only@*/ /*@null@*/ avContext ctx)
        /*@globals internalState @*/
        /*@modifies ctx, internalState @*/;

/**
 */
/*@null@*/
void * avContextCreate(const char *uri, /*@null@*/ struct stat *st)
        /*@globals internalState @*/
        /*@modifies *st, internalState @*/;

/**
 */
int avContextAdd(avContext ctx, const char * path,
		mode_t mode, size_t size, time_t mtime)
        /*@globals internalState @*/
        /*@modifies ctx, internalState @*/;

/**
 * Close an argv directory.
 * @param dir		argv DIR
 * @return 		0 always
 */
int avClosedir(/*@only@*/ DIR * dir)
	/*@globals fileSystem @*/
	/*@modifies dir, fileSystem @*/;

/**
 * Return next entry from an argv directory.
 * @param dir		argv DIR
 * @return 		next entry
 */
/*@dependent@*/ /*@null@*/
struct dirent * avReaddir(DIR * dir)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;

/**
 * Create an argv directory from an argv array.
 * @param path		directory path
 * @param av		argv array
 * @param modes		element modes (NULL will use pesky trailing '/')
 * @return 		argv DIR
 */
/*@null@*/
DIR * avOpendir(const char * path,
		/*@null@*/ const char ** av, /*@null@*/ rpmuint16_t * modes)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;
#endif

/**
 * Close active neon transfer(s) (if any).
 * @param _u		URL container
 * @return		0 on sucess
 */
int davDisconnect(void * _u)
	/*@globals internalState @*/
	/*@modifies _u, internalState @*/;

/**
 * Free persistent neon session state.
 * @param u		URL container
 * @return		0 on success
 */
int davFree(urlinfo u)
	/*@globals internalState @*/
	/*@modifies u, internalState @*/;

/**
 * Free global neon+openSSL state memory.
 */
void davDestroy(void)
	/*@globals internalState @*/
	/*@modifies internalState @*/;

/**
 * Send a http request.
 * @param ctrl		connection stream
 * @param httpCmd	http command
 * @param httpArg	http command argument (NULL if none)
 * @returns		0 on success
 */
int davReq(FD_t ctrl, const char * httpCmd, /*@null@*/ const char * httpArg)
	/*@globals fileSystem, internalState @*/
	/*@modifies ctrl, fileSystem, internalState @*/;

/**
 * Read a http response.
 * @param u		URL container
 * @param ctrl		connection stream
 * @retval *str		error msg		
 * @returns		0 on success
 */
/*@-exportlocal@*/
int davResp(urlinfo u, FD_t ctrl, /*@out@*/ /*@null@*/ char *const * str)
	/*@globals fileSystem, internalState @*/
	/*@modifies ctrl, *str, fileSystem, internalState @*/;
/*@=exportlocal@*/

/**
 * Open a URL.
 * @param url
 * @param flags
 * @param mode
 * @retval *uret
 * @return
 */
/*@null@*/
FD_t davOpen(const char * url, /*@unused@*/ int flags,
		/*@unused@*/ mode_t mode, /*@out@*/ urlinfo * uret)
        /*@globals internalState @*/
        /*@modifies *uret, internalState @*/;

/*@null@*/
FD_t httpOpen(const char * url, /*@unused@*/ int flags,
                /*@unused@*/ mode_t mode, /*@out@*/ urlinfo * uret)
        /*@globals internalState @*/
        /*@modifies *uret, internalState @*/;

/**
 */
/*@-incondefs@*/
ssize_t davRead(void * cookie, /*@out@*/ char * buf, size_t count)
        /*@globals errno, fileSystem, internalState @*/
        /*@modifies buf, errno, fileSystem, internalState @*/
	/*@requires maxSet(buf) >= (count - 1) @*/
	/*@ensures maxRead(buf) == result @*/;
/*@=incondefs@*/

/**
 */
ssize_t davWrite(void * cookie, const char * buf, size_t count)
        /*@globals fileSystem, internalState @*/
        /*@modifies fileSystem, internalState @*/;

/**
 */
int davSeek(void * cookie, _libio_pos_t pos, int whence)
        /*@globals fileSystem, internalState @*/
        /*@modifies fileSystem, internalState @*/;

/**
 */
int davClose(void * cookie)
	/*@globals fileSystem, internalState @*/
	/*@modifies cookie, fileSystem, internalState @*/;

/**
 */
int davMkdir(const char * path, mode_t mode)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

/**
 */
int davRmdir(const char * path)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

/**
 */
int davRename(const char * oldpath, const char * newpath)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

/**
 */
int davUnlink(const char * path)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

/**
 * Close a DAV collection.
 * @param dir		argv DIR
 * @return 		0 always
 */
int davClosedir(/*@only@*/ DIR * dir)
	/*@globals fileSystem @*/
	/*@modifies dir, fileSystem @*/;

/**
 * Return next entry from a DAV collection.
 * @param dir		argv DIR
 * @return 		next entry
 */
/*@dependent@*/ /*@null@*/
struct dirent * davReaddir(DIR * dir)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;

/**
 * Create an argv directory from DAV collection.
 * @param path		URL for DAV collection path
 * @return 		argv DIR
 */
/*@null@*/
DIR * davOpendir(const char * path)
	/*@globals errno, fileSystem, internalState @*/
	/*@modifies errno, fileSystem, internalState @*/;

/**
 * stat(2) clone.
 */
int davStat(const char * path, /*@out@*/ struct stat * st)
	/*@globals errno, fileSystem, internalState @*/
	/*@modifies *st, errno, fileSystem, internalState @*/;

/**
 * lstat(2) clone.
 */
int davLstat(const char * path, /*@out@*/ struct stat * st)
	/*@globals errno, fileSystem, internalState @*/
	/*@modifies *st, errno, fileSystem, internalState @*/;

/**
 * realpath(3) clone.
 */
char * davRealpath(const char * path, /*@out@*/ /*@null@*/ char * resolved_path)
	/*@modifies *resolved_path @*/;

#ifdef __cplusplus
}
#endif

#endif /* RPMDAV_H */

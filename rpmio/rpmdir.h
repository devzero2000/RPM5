#ifndef	H_RPMDIR
#define	H_RPMDIR

/** \ingroup rpmio
 * \file rpmio/rpmdir.h
 */

#include <sys/types.h>
#include <dirent.h>

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

/*@unchecked@*/
extern int _av_debug;

/**
 */
/*@unchecked@*/
extern int avmagicdir;
#define ISAVMAGIC(_dir) (!memcmp((_dir), &avmagicdir, sizeof(avmagicdir)))

#if defined(_RPMDIR_INTERNAL)
#include <argv.h>

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

/**
 */
typedef struct avContext_s * avContext;

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
#endif	/* _RPMDIR_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_RPMDIR_INTERNAL)
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

/**
 * Return next entry from an argv directory.
 * @param dir		argv DIR
 * @return 		next entry
 */
/*@dependent@*/ /*@null@*/
struct dirent * avReaddir(DIR * dir)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
#endif	/* _RPMDIR_INTERNAL */

/**
 * closedir(3) clone.
 */
int Closedir(/*@only@*/ DIR * dir)
	/*@globals errno, fileSystem @*/
	/*@modifies *dir, errno, fileSystem @*/;

/**
 * opendir(3) clone.
 */
/*@null@*/
DIR * Opendir(const char * path)
	/*@globals errno, h_errno, fileSystem, internalState @*/
	/*@modifies errno, fileSystem, internalState @*/;

/**
 * readdir(3) clone.
 */
/*@dependent@*/ /*@null@*/
struct dirent * Readdir(DIR * dir)
	/*@globals errno, fileSystem @*/
	/*@modifies *dir, errno, fileSystem @*/;

/**
 * rewinddir(3) clone.
 */
void Rewinddir(DIR * dir)
	/*@modifies dir @*/;

/**
 * scandir(3) clone.
 */
int Scandir(const char * path, struct dirent *** nl,
		int (*filter) (const struct dirent *),
		int (*compar) (const void *, const void *))
	/*@modifies dir @*/;
int Alphasort(const void * a, const void * b)
	/*@*/;
int Versionsort(const void * a, const void * b)
	/*@*/;

/**
 * seekdir(3) clone.
 */
void Seekdir(DIR * dir, off_t offset)
	/*@modifies dir @*/;

/**
 * telldir(3) clone.
 */
off_t Telldir(DIR * dir)
	/*@globals errno @*/
	/*@modifies dir, errno @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMDIR */

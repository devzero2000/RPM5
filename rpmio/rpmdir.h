#ifndef	H_RPMDIR
#define	H_RPMDIR

/** \ingroup rpmio
 * \file rpmio/rpmdir.h
 */

#include <sys/types.h>
#include <dirent.h>

#ifdef __cplusplus
extern "C" {
#endif

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

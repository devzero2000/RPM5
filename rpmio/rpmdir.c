/** \ingroup rpmio
 * \file rpmio/rpmdir.c
 */

#include "system.h"

#include "rpmio_internal.h"
#include <rpmdir.h>

#define	_RPMAV_INTERNAL
#define	_RPMDAV_INTERNAL
#include <rpmdav.h>

#include "debug.h"

/*@access DIR @*/

int Closedir(DIR * dir)
{
if (_rpmio_debug)
fprintf(stderr, "*** Closedir(%p)\n", (void *)dir);
    if (dir == NULL)
	return 0;
    if (ISAVMAGIC(dir))
	return avClosedir(dir);
    return closedir(dir);
}

DIR * Opendir(const char * path)
	/*@globals ftpBufAlloced, ftpBuf @*/
	/*@modifies ftpBufAlloced, ftpBuf @*/
{
    const char * lpath;
    int ut = urlPath(path, &lpath);

if (_rpmio_debug)
fprintf(stderr, "*** Opendir(%s)\n", path);
    switch (ut) {
    case URL_IS_FTP:
	return ftpOpendir(path);
	/*@notreached@*/ break;
    case URL_IS_HTTPS:	
    case URL_IS_HTTP:
#ifdef WITH_NEON
	return davOpendir(path);
#endif
	/*@notreached@*/ break;
    case URL_IS_PATH:
	path = lpath;
	/*@fallthrough@*/
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    case URL_IS_HKP:
    default:
	return NULL;
	/*@notreached@*/ break;
    }
    /*@-dependenttrans@*/
    return opendir(path);
    /*@=dependenttrans@*/
}

struct dirent * Readdir(DIR * dir)
{
if (_rpmio_debug)
fprintf(stderr, "*** Readdir(%p)\n", (void *)dir);
    if (dir == NULL)
	return NULL;
    if (ISAVMAGIC(dir))
	return avReaddir(dir);
    return readdir(dir);
}

static void avRewinddir(DIR * dir)
	/*@*/
{
    return;
}

void Rewinddir(DIR * dir)
{
if (_rpmio_debug)
fprintf(stderr, "*** Rewinddir(%p)\n", (void *)dir);
    if (ISAVMAGIC(dir))
	return avRewinddir(dir);
    return rewinddir(dir);
}

static int avScandir(const char * path, struct dirent *** nl,
		int (*filter) (const struct dirent *),
		int (*compar) (const void *, const void *))
	/*@*/
{
    int rc = -1;
    return rc;
}

int Scandir(const char * path, struct dirent *** nl,
		int (*filter) (const struct dirent *),
		int (*compar) (const void *, const void *))
{
    const char * lpath;
    int ut = urlPath(path, &lpath);
    int rc = 0;

    switch (ut) {
    case URL_IS_FTP:
    case URL_IS_HTTPS:	
    case URL_IS_HTTP:
    case URL_IS_DASH:
    case URL_IS_HKP:
    default:
	rc = avScandir(path, nl, filter, compar);
	break;
    case URL_IS_UNKNOWN:
	lpath = path;
	/*@fallthrough@*/
    case URL_IS_PATH:
	break;
    }

    if (!rc)
	rc = scandir(lpath, nl, filter, compar);

if (_rpmio_debug)
fprintf(stderr, "*** Scandir(\"%s\", %p, %p, %p) rc %d\n", path, nl, filter, compar, rc);
    return rc;
}

int Alphasort(const void * a, const void * b)
{
    const struct dirent * adp = a;
    const struct dirent * bdp = b;
#if defined(HAVE_STRCOLL)
    return strcoll(adp->d_name, bdp->d_name);
#else
    return strcmp(adp->d_name, bdp->d_name);
#endif
}

int Versionsort(const void * a, const void * b)
{
    const struct dirent * adp = a;
    const struct dirent * bdp = b;
#if defined(HAVE_STRVERSCMP)
    return strverscmp(adp->d_name, bdp->d_name);
#else
    return strcmp(adp->d_name, bdp->d_name);
#endif
}

static void avSeekdir(DIR * dir, off_t offset)
	/*@*/
{
    return;
}

void Seekdir(DIR * dir, off_t offset)
{
if (_rpmio_debug)
fprintf(stderr, "*** Seekdir(%p,%ld)\n", (void *)dir, (long)offset);
    if (ISAVMAGIC(dir))
	return avSeekdir(dir, offset);
    return seekdir(dir, offset);
}

static off_t avTelldir(DIR * dir)
	/*@globals errno @*/
	/*@modifies errno @*/
{
    off_t offset = -1;
    errno = EBADF;
    return offset;
}

off_t Telldir(DIR * dir)
{
    off_t offset = 0;

    offset = (ISAVMAGIC(dir) ? avTelldir(dir) : telldir(dir));
if (_rpmio_debug)
fprintf(stderr, "*** Telldir(%p) off %ld\n", (void *)dir, (long)offset);
    return offset;
}

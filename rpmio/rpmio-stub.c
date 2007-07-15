/** \ingroup rpmio
 * \file rpmio/rpmio-stub.c
 */

#include "system.h"
#include <rpmio.h>
#include "rpmio-stub.h"
#include "debug.h"

const char * (*_Fstrerror) (void * fd)
	= (void *)Fstrerror;
size_t (*_Fread) (void * buf, size_t size, size_t nmemb, void * fd)
	= (void *)Fread;
size_t (*_Fwrite) (const void * buf, size_t size, size_t nmemb, void * fd)
	= (void *)Fwrite;
int (*_Fseek) (void * fd, _libio_off_t offset, int whence)
	= (void *)Fseek;
int (*_Fclose) (void * fd)
	= (void *)Fclose;
void * (*_Fdopen) (void * ofd, const char * fmode)
	= (void *)Fdopen;
void * (*_Fopen) (const char * path, const char * fmode)
	= (void *)Fopen;
int (*_Fflush) (void * fd)
	= (void *)Fflush;
int (*_Ferror) (void * fd)
	= (void *)Ferror;
int (*_Fileno) (void * fd)
	= (void *)Fileno;
int (*_Fcntl) (void * fd, int op, void *lip)
	= (void *)Fcntl;

int (*_Mkdir) (const char * path, mode_t mode) = Mkdir;
int (*_Chdir) (const char * path) = Chdir;
int (*_Rmdir) (const char * path) = Rmdir;
int (*_Chroot) (const char * path) = Chroot;
int (*_Rename) (const char * oldpath, const char * newpath) = Rename;
int (*_Link) (const char * oldpath, const char * newpath) = Link;
int (*_Unlink) (const char * path) = Unlink;
int (*_Stat) (const char * path, struct stat * st) = Stat;
int (*_Lstat) (const char * path, struct stat * st) = Lstat;
int (*_Chown) (const char * path, uid_t owner, gid_t group) = Chown;
int (*_Lchown) (const char * path, uid_t owner, gid_t group) = Lchown;
int (*_Chmod) (const char * path, mode_t mode) = Chmod;
int (*_Mkfifo) (const char * path, mode_t mode) = Mkfifo;
int (*_Mknod) (const char * path, mode_t mode, dev_t dev) = Mknod;
int (*_Utime) (const char * path, const struct utimbuf * buf) = Utime;
int (*_Utimes) (const char * path, const struct timeval * times) = Utimes;
int (*_Symlink) (const char * oldpath, const char * newpath) = Symlink;
int (*_Readlink) (const char * path, char * buf, size_t bufsiz) = Readlink;
int (*_Access) (const char * path, int amode) = Access;
int (*_Glob_pattern_p) (const char *pattern, int quote) = Glob_pattern_p;
int (*_Glob_error) (const char * epath, int eerrno) = Glob_error;
int (*_Glob) (const char * pattern, int flags,
		int errfunc(const char * epath, int eerrno),
		glob_t * pglob) = Glob;
void (*_Globfree) (glob_t * pglob) = Globfree;
DIR * (*_Opendir) (const char * path) = Opendir;
struct dirent * (*_Readdir) (DIR * dir) = Readdir;
int (*_Closedir) (DIR * dir) = Closedir;
off_t (*_Lseek) (int fdno, off_t offset, int whence) = Lseek;

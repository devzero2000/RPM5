/** \ingroup rpmio
 * \file rpmio/rpmdir.c
 */

#include "system.h"

#include <rpmio.h>
#include <rpmurl.h>
#include <argv.h>

#define	_RPMDIR_INTERNAL
#include <rpmdir.h>
#include <rpmdav.h>		/* ftpOpendir/davOpendir */
#include <rpmhash.h>		/* rpmHashFunctionString */

#include "debug.h"

/*@access DIR @*/

/* =============================================================== */
void * avContextDestroy(avContext ctx)
{
    if (ctx == NULL)
	return NULL;
    if (ctx->av != NULL)
	ctx->av = argvFree(ctx->av);
    ctx->modes = _free(ctx->modes);
    ctx->sizes = _free(ctx->sizes);
    ctx->mtimes = _free(ctx->mtimes);
    ctx->u = urlFree(ctx->u, "avContextDestroy");
    ctx->uri = _free(ctx->uri);
    memset(ctx, 0, sizeof(*ctx));	/* trash & burn */
    ctx = _free(ctx);
    return NULL;
}

void * avContextCreate(const char *uri, struct stat *st)
{
    avContext ctx;
    urlinfo u;

/*@-globs@*/	/* FIX: h_errno annoyance. */
    if (urlSplit(uri, &u))
	return NULL;
/*@=globs@*/

    ctx = xcalloc(1, sizeof(*ctx));
    ctx->uri = xstrdup(uri);
    ctx->u = urlLink(u, "avContextCreate");
/*@-temptrans@*/	/* XXX note the assignment */
    if ((ctx->st = st) != NULL)
	memset(ctx->st, 0, sizeof(*ctx->st));
/*@=temptrans@*/
    return ctx;
}

int avContextAdd(avContext ctx, const char * path,
		mode_t mode, size_t size, time_t mtime)
{
    int xx;

if (_av_debug < 0)
fprintf(stderr, "*** avContextAdd(%p,\"%s\", %06o, 0x%x, 0x%x)\n", ctx, path, (unsigned)mode, (unsigned)size, (unsigned)mtime);

    xx = argvAdd(&ctx->av, path);

    while (ctx->ac >= ctx->nalloced) {
	if (ctx->nalloced <= 0)
	    ctx->nalloced = 1;
	ctx->nalloced *= 2;
	ctx->modes = xrealloc(ctx->modes,
				(sizeof(*ctx->modes) * ctx->nalloced));
	ctx->sizes = xrealloc(ctx->sizes,
				(sizeof(*ctx->sizes) * ctx->nalloced));
	ctx->mtimes = xrealloc(ctx->mtimes,
				(sizeof(*ctx->mtimes) * ctx->nalloced));
    }

    ctx->modes[ctx->ac] = (rpmuint16_t)mode;
    ctx->sizes[ctx->ac] = size;
    ctx->mtimes[ctx->ac] = mtime;
    ctx->ac++;
    return 0;
}

int avClosedir(/*@only@*/ DIR * dir)
{
    AVDIR avdir = (AVDIR)dir;

if (_av_debug)
fprintf(stderr, "*** avClosedir(%p)\n", avdir);

#if defined(WITH_PTHREADS)
/*@-moduncon -noeffectuncon @*/
    (void) pthread_mutex_destroy(&avdir->lock);
/*@=moduncon =noeffectuncon @*/
#endif

    avdir = _free(avdir);
    return 0;
}

DIR * avOpendir(const char * path, const char ** av, rpmuint16_t * modes)
{
    AVDIR avdir;
    struct dirent * dp;
    size_t nb;
    const char ** nav;
    unsigned char * dt;
    char * t;
    int ac, nac;

if (_av_debug)
fprintf(stderr, "*** avOpendir(%s, %p, %p)\n", path, av, modes);

    nb = 0;
    ac = 0;
    if (av != NULL)
    while (av[ac] != NULL)
	nb += strlen(av[ac++]) + 1;
    ac += 2;	/* for "." and ".." */
    nb += sizeof(".") + sizeof("..");

    nb += sizeof(*avdir) + sizeof(*dp) + ((ac + 1) * sizeof(*av)) + (ac + 1);
    avdir = xcalloc(1, nb);
/*@-abstract@*/
    dp = (struct dirent *) (avdir + 1);
    nav = (const char **) (dp + 1);
    dt = (unsigned char *) (nav + (ac + 1));
    t = (char *) (dt + ac + 1);
/*@=abstract@*/

    avdir->fd = avmagicdir;
/*@-usereleased@*/
    avdir->data = (char *) dp;
/*@=usereleased@*/
    avdir->allocation = nb;
    avdir->size = ac;
    avdir->offset = -1;
    /* Hash the directory path for a d_ino analogue. */
    avdir->filepos = hashFunctionString(0, path, 0);

#if defined(WITH_PTHREADS)
/*@-moduncon -noeffectuncon -nullpass @*/
    (void) pthread_mutex_init(&avdir->lock, NULL);
/*@=moduncon =noeffectuncon =nullpass @*/
#endif

    nac = 0;
    /*@-dependenttrans -unrecog@*/
    dt[nac] = (unsigned char)DT_DIR; nav[nac++] = t; t = stpcpy(t, "."); t++;
    dt[nac] = (unsigned char)DT_DIR; nav[nac++] = t; t = stpcpy(t, ".."); t++;
    /*@=dependenttrans =unrecog@*/

    /* Append av strings to DIR elements. */
    ac = 0;
    if (av != NULL)
    while (av[ac] != NULL) {
	if (modes != NULL)
	    switch (modes[ac] & S_IFMT) {
	    case S_IFIFO: dt[nac]=(unsigned char)DT_FIFO;/*@switchbreak@*/break;
	    case S_IFCHR: dt[nac]=(unsigned char)DT_CHR; /*@switchbreak@*/break;
	    case S_IFDIR: dt[nac]=(unsigned char)DT_DIR; /*@switchbreak@*/break;
	    case S_IFBLK: dt[nac]=(unsigned char)DT_BLK; /*@switchbreak@*/break;
	    case S_IFREG: dt[nac]=(unsigned char)DT_REG; /*@switchbreak@*/break;
	    case S_IFLNK: dt[nac]=(unsigned char)DT_LNK; /*@switchbreak@*/break;
/*@-unrecog@*/
	    case S_IFSOCK:dt[nac]=(unsigned char)DT_SOCK;/*@switchbreak@*/break;
/*@=unrecog@*/
	    default:	  dt[nac]=(unsigned char)DT_UNKNOWN;/*@switchbreak@*/break;
	    }
	else
	    dt[nac] = (unsigned char)DT_UNKNOWN;
/*@-dependenttrans@*/
	nav[nac++] = t;
/*@=dependenttrans@*/
	t = stpcpy(t, av[ac++]);
	t++;	/* trailing \0 */
    }
    nav[nac] = NULL;

/*@-kepttrans@*/
    return (DIR *) avdir;
/*@=kepttrans@*/
}

struct dirent * avReaddir(DIR * dir)
{
    AVDIR avdir = (AVDIR)dir;
    struct dirent * dp;
    const char ** av;
    unsigned char * dt;
    int ac;
    int i;

    if (avdir == NULL || !ISAVMAGIC(avdir) || avdir->data == NULL) {
	/* XXX TODO: EBADF errno. */
	return NULL;
    }

    dp = (struct dirent *) avdir->data;
    av = (const char **) (dp + 1);
    ac = (int)avdir->size;
    dt = (unsigned char *) (av + (ac + 1));
    i = avdir->offset + 1;

    if (i < 0 || i >= ac || av[i] == NULL)
	return NULL;

    avdir->offset = i;

    /* XXX glob(3) uses REAL_DIR_ENTRY(dp) test on d_ino */
/*@-type@*/
    /* Hash the name (starting with parent hash) for a d_ino analogue. */
    dp->d_ino = hashFunctionString(avdir->filepos, dp->d_name, 0);

#if !defined(__DragonFly__) && !defined(__CYGWIN__)
    dp->d_reclen = 0;		/* W2DO? */
#endif

#if !(defined(hpux) || defined(__hpux) || defined(sun) || defined(RPM_OS_AIX) || defined(__CYGWIN__) || defined(__QNXNTO__))
#if !defined(__APPLE__) && !defined(__FreeBSD_kernel__) && !defined(__FreeBSD__) && !defined(__NetBSD__) && !defined(__DragonFly__) && !defined(__OpenBSD__)
    dp->d_off = (off_t)i;
#endif
    dp->d_type = dt[i];
#endif
/*@=type@*/

    strncpy(dp->d_name, av[i], sizeof(dp->d_name));
if (_av_debug)
fprintf(stderr, "*** avReaddir(%p) %p %s\n", (void *)avdir, dp, dp->d_name);

    return dp;
}

static void avRewinddir(DIR * dir)
	/*@*/
{
    AVDIR avdir = (AVDIR)dir;

    if (avdir != NULL && ISAVMAGIC(avdir))
	avdir->offset = (off_t)-1;
    return;
}

static int avScandir(const char * path, struct dirent *** nl,
		int (*filter) (const struct dirent *),
		int (*compar) (const void *, const void *))
	/*@*/
{
    int rc = -1;
    return rc;
}

static void avSeekdir(DIR * dir, off_t offset)
	/*@*/
{
    AVDIR avdir = (AVDIR)dir;
    struct dirent * dp;
    const char ** av;
    int ac;

    if (avdir == NULL || !ISAVMAGIC(avdir) || avdir->data == NULL)
	return;

    dp = (struct dirent *) avdir->data;
    av = (const char **) (dp + 1);
    ac = (int)avdir->size;

    if (offset < 0 || offset >= ac || av[offset] == NULL)
	return;

    avdir->offset = offset - 1;		/* XXX set to previous entry */

    return;
}

static off_t avTelldir(DIR * dir)
	/*@globals errno @*/
	/*@modifies errno @*/
{
    AVDIR avdir = (AVDIR)dir;
    off_t offset = -1;
    struct dirent * dp;
    const char ** av;
    int ac;

    if (avdir != NULL && ISAVMAGIC(avdir) && avdir->data != NULL) {
	dp = (struct dirent *) avdir->data;
	av = (const char **) (dp + 1);
	ac = (int)avdir->size;
	offset = avdir->offset;
    }

    if (offset < 0 || offset >= ac || av[offset] == NULL)
	errno = EBADF;

    return offset;
}

/* =============================================================== */
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

void Rewinddir(DIR * dir)
{
if (_rpmio_debug)
fprintf(stderr, "*** Rewinddir(%p)\n", (void *)dir);
    if (ISAVMAGIC(dir))
	return avRewinddir(dir);
    return rewinddir(dir);
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

void Seekdir(DIR * dir, off_t offset)
{
if (_rpmio_debug)
fprintf(stderr, "*** Seekdir(%p,%ld)\n", (void *)dir, (long)offset);
    if (ISAVMAGIC(dir))
	return avSeekdir(dir, offset);
    return seekdir(dir, offset);
}

off_t Telldir(DIR * dir)
{
    off_t offset = 0;

    offset = (ISAVMAGIC(dir) ? avTelldir(dir) : telldir(dir));
if (_rpmio_debug)
fprintf(stderr, "*** Telldir(%p) off %ld\n", (void *)dir, (long)offset);
    return offset;
}

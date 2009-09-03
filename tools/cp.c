/*-
 * Copyright (c) 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * David Hitz of Auspex Systems Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "system.h"

#include <rpmio.h>
#include <rpmacl.h>
#include <rpmlog.h>
#include <argv.h>
#include <fts.h>
#include <poptIO.h>

#if defined(__linux__)
#include <utime.h>
#define st_atimespec    st_atim
#define st_mtimespec    st_mtim
/* XXX FIXME */
#define	lchmod(_fn, _mode)	Chmod((_fn), (_mode))
#endif

#include "debug.h"

#if !defined(TIMESPEC_TO_TIMEVAL)
#define TIMESPEC_TO_TIMEVAL(tv, ts)                                     \
        do {                                                            \
                (tv)->tv_sec = (ts)->tv_sec;                            \
                (tv)->tv_usec = (ts)->tv_nsec / 1000;                   \
        } while (0)
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__APPLE__)
#define HAVE_ST_FLAGS   1       /* XXX TODO: should be AutoFu test */
#else
#undef  HAVE_ST_FLAGS           /* XXX TODO: should be AutoFu test */
#endif

#if !defined(S_ISTXT) && defined(S_ISVTX)       /* XXX linux != BSD */
#define S_ISTXT         S_ISVTX
#endif

#define	STRIP_TRAILING_SLASH(p) {					\
        while ((p).p_end > (p).p_path + 1 && (p).p_end[-1] == '/')	\
                *--(p).p_end = '\0';					\
}

/*==============================================================*/

#define	_KFB(n)	(1U << (n))
#define	_MFB(n)	(_KFB(n) | 0x40000000)

/**
 * Bit field enum for copy CLI options.
 */
enum rpmctFlags_e {
    COPY_FLAGS_NONE		= 0,
    COPY_FLAGS_FOLLOWARGS	= _MFB( 0), /*!< -H ... */
    COPY_FLAGS_FOLLOW		= _MFB( 1), /*!< -L ... */
    COPY_FLAGS_RECURSE		= _MFB( 2), /*!< -R ... */
    COPY_FLAGS_ARCHIVE		= _MFB( 3), /*!< -a,--archive ... */
    COPY_FLAGS_FORCE		= _MFB( 4), /*!< -f,--force ... */
    COPY_FLAGS_INTERACTIVE	= _MFB( 5), /*!< -i,--interactive ... */
    COPY_FLAGS_HARDLINK		= _MFB( 6), /*!< -l,--link ... */
    COPY_FLAGS_NOCLOBBER	= _MFB( 7), /*!< -n,--noclobber ... */
    COPY_FLAGS_PRESERVE		= _MFB( 8), /*!< -p,--preserve ... */

    COPY_FLAGS_VERBOSE		= _MFB( 9), /*!< -v,--verbose ... */
	/* 10-31 unused */
};

#define CP_ISSET(_FLAG) ((ct->flags & ((COPY_FLAGS_##_FLAG) & ~0x40000000)) != COPY_FLAGS_NONE)

/**
 */
typedef struct rpmct_s * rpmct;

enum rpmctType_e { FILE_TO_FILE, FILE_TO_DIR, DIR_TO_DNE };

/*
 * Cp copies source files to target files.
 *
 * The global PATH_T structure "to" always contains the path to the
 * current target file.  Since fts(3) does not change directories,
 * this path can be either absolute or dot-relative.
 *
 * The basic algorithm is to initialize "to" and use fts(3) to traverse
 * the file hierarchy rooted in the argument list.  A trivial case is the
 * case of 'cp file1 file2'.  The more interesting case is the case of
 * 'cp file1 file2 ... fileN dir' where the hierarchy is traversed and the
 * path (relative to the root of the traversal) is appended to dir (stored
 * in "to") to form the final target path.
 */
struct rpmct_s {
    enum rpmctFlags_e flags;
    enum rpmctType_e type;
    const char ** av;
    int ac;
    int ftsoptions;
    FTS * t;
    FTSENT * p;
    struct stat sb;
    char * b;
    size_t blen;
    size_t ballocated;
    struct timeval tv[2];
    char * p_end;		/* pointer to NULL at end of path */
    char * target_end;		/* pointer to end of target base */
    char p_path[PATH_MAX];	/* pointer to the start of a path */
};

/**
 */
static struct rpmct_s __ct = {
        .flags = COPY_FLAGS_NONE
};

static rpmct _ct = &__ct;

static volatile sig_atomic_t info;

/* ----- utils.c */

#if !defined(MAXPHYS)
#define	MAXPHYS		(128 * 1024)	/* max raw I/O transfer size */
#endif

#define	cp_pct(x, y)	((y == 0) ? 0 : (int)(100.0 * (x) / (y)))

/* Memory strategy threshold, in pages: if physmem is larger then this, use a 
 * large buffer */
#define PHYSPAGES_THRESHOLD (32*1024)

/* Maximum buffer size in bytes - do not allow it to grow larger than this */
#define BUFSIZE_MAX (2*1024*1024)

/* Small (default) buffer size in bytes. It's inefficient for this to be
 * smaller than MAXPHYS */
#define BUFSIZE_SMALL (MAXPHYS)

static rpmRC
rpmctSetFile(rpmct ct, FD_t fd)
{
    struct stat * st = ct->p->fts_statp;
    struct stat ts;
    rpmRC rval = RPMRC_OK;
    int fdno = (fd ? Fileno(fd) : -1);
    int fdval = fdno >= 0;
    int islink = !fdval && S_ISLNK(st->st_mode);
    int gotstat;

    st->st_mode &= S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO;

    TIMESPEC_TO_TIMEVAL(&ct->tv[0], &st->st_atimespec);
    TIMESPEC_TO_TIMEVAL(&ct->tv[1], &st->st_mtimespec);
    if (islink ? lutimes(ct->p_path, ct->tv) : Utimes(ct->p_path, ct->tv)) {
	warn("%sutimes: %s", islink ? "l" : "", ct->p_path);
	rval = RPMRC_FAIL;
    }
    if (fdval ? Fstat(fd, &ts) :
	(islink ? Lstat(ct->p_path, &ts) : Stat(ct->p_path, &ts)))
	    gotstat = 0;
    else {
	gotstat = 1;
	ts.st_mode &= S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO;
    }

    /*
    * Changing the ownership probably won't succeed, unless we're root
    * or POSIX_CHOWN_RESTRICTED is not set.  Set uid/gid before setting
    * the mode; current BSD behavior is to remove all setuid bits on
    * chown.  If chown fails, lose setuid/setgid bits.
    */
    if (!gotstat || st->st_uid != ts.st_uid || st->st_gid != ts.st_gid)
	if (fdval ? Fchown(fd, st->st_uid, st->st_gid) :
	   (islink ? Lchown(ct->p_path, st->st_uid, st->st_gid) :
	   Chown(ct->p_path, st->st_uid, st->st_gid)))
	{
	    if (errno != EPERM) {
		warn("chown: %s", ct->p_path);
		rval = RPMRC_FAIL;
	    }
	    st->st_mode &= ~(S_ISUID | S_ISGID);
	}

    if (!gotstat || st->st_mode != ts.st_mode)
	if (fdval ? Fchmod(fd, st->st_mode) :
	   (islink ? lchmod(ct->p_path, st->st_mode) :
	   Chmod(ct->p_path, st->st_mode)))
	{
	    warn("Chmod: %s", ct->p_path);
	    rval = RPMRC_FAIL;
	}

#if defined(HAVE_ST_FLAGS)
    if (!gotstat || st->st_flags != ts.st_flags)
	if (fdval ?  fchflags(fdno, st->st_flags) :
	   (islink ? lchflags(ct->p_path, st->st_flags) :
	   chflags(ct->p_path, st->st_flags)))
	{
	    warn("chflags: %s", ct->p_path);
	    rval = RPMRC_FAIL;
	}
#endif

    return rval;
}

static rpmRC
rpmctCopyFile(rpmct ct, int dne)
{
    struct stat * st = ct->p->fts_statp;
    ssize_t wcount;
    size_t wresid;
    off_t wtotal;
    int ch;
    int checkch;
    FD_t ifd = NULL;
    int rcount;
    rpmRC rval;
    FD_t ofd = NULL;
    char *bufp;
#ifdef VM_AND_BUFFER_CACHE_SYNCHRONIZED
    char *p;
#endif

    ifd = Fopen(ct->p->fts_path, "r.ufdio");
    if (ifd == NULL || Ferror(ifd)) {
	if (ifd) (void) Fclose(ifd);
	warn("%s", ct->p->fts_path);
	return RPMRC_FAIL;
    }

    /*
     * If the file exists and we're interactive, verify with the user.
     * If the file DNE, set the mode to be the from file, minus setuid
     * bits, modified by the umask; arguably wrong, but it makes copying
     * executables work right and it's been that way forever.  (The
     * other choice is 666 or'ed with the execute bits on the from file
     * modified by the umask.)
     */
    if (!dne) {
#define YESNO "(y/n [n]) "
	if (CP_ISSET(NOCLOBBER)) {
	    if (CP_ISSET(VERBOSE))
		fprintf(stdout, "%s not overwritten\n", ct->p_path);
	    (void) Fclose(ifd);
	    return RPMRC_OK;
	} else if (CP_ISSET(INTERACTIVE)) {
	    (void)fprintf(stderr, "overwrite %s? %s", ct->p_path, YESNO);
	    checkch = ch = getchar();
	    while (ch != '\n' && ch != EOF)
		ch = getchar();
	    if (checkch != 'y' && checkch != 'Y') {
		(void) Fclose(ifd);
		(void)fprintf(stderr, "not overwritten\n");
		return RPMRC_FAIL;
	    }
	}

	if (CP_ISSET(FORCE)) {
	    /* remove existing destination file name, 
	     * create a new file  */
	    (void)Unlink(ct->p_path);
	    if (!CP_ISSET(HARDLINK))
		ofd = Fopen(ct->p_path, "wb");
	} else {
	    if (!CP_ISSET(HARDLINK))
		/* overwrite existing destination file name */
		ofd = Fopen(ct->p_path, "wb");
	}
    } else {
	if (!CP_ISSET(HARDLINK))
	    ofd = Fopen(ct->p_path, "wb");
    }
	
    if (ofd == NULL || Ferror(ofd)
     || Fchmod(ofd, st->st_mode & ~(S_ISUID | S_ISGID)))
    {
	warn("%s", ct->p_path);
	if (ofd) (void) Fclose(ofd);
	(void) Fclose(ifd);
	return RPMRC_FAIL;
    }

    rval = RPMRC_OK;

    if (!CP_ISSET(HARDLINK)) {
	/*
	 * Mmap and write if less than 8M (the limit is so we don't totally
	 * trash memory on big files.  This is really a minor hack, but it
	 * wins some CPU back.
	 * Some filesystems, such as smbnetfs, don't support mmap,
	 * so this is a best-effort attempt.
	 */
#ifdef VM_AND_BUFFER_CACHE_SYNCHRONIZED
	if (S_ISREG(st->st_mode) && st->st_size > 0
	 && st->st_size <= 8 * 1024 * 1024
	 && (p = Mmap(NULL, (size_t)st->st_size, PROT_READ,
		    MAP_SHARED, Fileno(ifd), (off_t)0)) != MAP_FAILED)
	{
	    wtotal = 0;
	    for (bufp = p, wresid = st->st_size; ;
		bufp += wcount, wresid -= (size_t)wcount)
	    {
		wcount = Fwrite(bufp, 1, wresid, ofd);
		if (Ferror(ofd) || wcount <= 0)
		    break;
		wtotal += wcount;
		if (info) {
		    info = 0;
		    (void)fprintf(stderr, "%s -> %s %3d%%\n",
			    ct->p->fts_path, ct->p_path,
			    cp_pct(wtotal, st->st_size));
		}
		if (wcount >= (ssize_t)wresid)
		    break;
	    }
	    if (wcount != (ssize_t)wresid) {
		warn("%s", ct->p_path);
		rval = RPMRC_FAIL;
	    }
	    /* Some systems don't unmap on close(2). */
	    if (Munmap(p, st->st_size) < 0) {
		warn("%s", ct->p->fts_path);
		rval = RPMRC_FAIL;
	    }
	} else
#endif
	{
	    if (ct->b == NULL) {
		ct->b = xmalloc(ct->ballocated);
		ct->blen = ct->ballocated;
	    }
	    wtotal = 0;
	    while ((rcount = Fread(ct->b, 1, ct->blen, ifd)) > 0 && !Ferror(ifd)) {
		for (bufp = ct->b, wresid = rcount; ;
	    		bufp += wcount, wresid -= wcount)
		{
		    wcount = Fwrite(bufp, 1, wresid, ofd);
		    if (Ferror(ofd) || wcount <= 0)
			break;
		    wtotal += wcount;
		    if (info) {
			info = 0;
			(void)fprintf(stderr, "%s -> %s %3d%%\n",
			    ct->p->fts_path, ct->p_path,
			    cp_pct(wtotal, st->st_size));
		    }
		    if (wcount >= (ssize_t)wresid)
			break;
		}
		if (wcount != (ssize_t)wresid) {
		    warn("%s", ct->p_path);
		    rval = RPMRC_FAIL;
		    break;
		}
	    }
	    if (rcount < 0) {
		warn("%s", ct->p->fts_path);
		rval = RPMRC_FAIL;
	    }
	}
    } else {
	if (Link(ct->p->fts_path, ct->p_path)) {
	    warn("%s", ct->p_path);
	    rval = RPMRC_FAIL;
	}
    }
	
    /*
     * Don't remove the target even after an error.  The target might
     * not be a regular file, or its attributes might be important,
     * or its contents might be irreplaceable.  It would only be safe
     * to remove it if we created it and its length is 0.
     */

    if (!CP_ISSET(HARDLINK)) {
	if (CP_ISSET(PRESERVE) && rpmctSetFile(ct, ofd))
	    rval = RPMRC_FAIL;
	if (CP_ISSET(PRESERVE) && rpmaclCopyFd(ifd, ofd) != 0)
	    rval = RPMRC_FAIL;
	if (ofd && Fclose(ofd)) {
	    warn("%s", ct->p_path);
	    rval = RPMRC_FAIL;
	}
    }

    (void) Fclose(ifd);

    return rval;
}

static rpmRC
rpmctCopyLink(rpmct ct, int exists)
{
    char llink[PATH_MAX];
    int len;

    if ((len = Readlink(ct->p->fts_path, llink, sizeof(llink) - 1)) == -1) {
	warn("Readlink: %s", ct->p->fts_path);
	return RPMRC_FAIL;
    }
    llink[len] = '\0';
    if (exists && Unlink(ct->p_path)) {
	warn("Unlink: %s", ct->p_path);
	return RPMRC_FAIL;
    }
    if (Symlink(llink, ct->p_path)) {
	warn("symlink: %s", llink);
	return RPMRC_FAIL;
    }
    return (CP_ISSET(PRESERVE) ? rpmctSetFile(ct, NULL) : RPMRC_OK);
}

static rpmRC
rpmctCopyFifo(rpmct ct, int exists)
{
    struct stat * st = ct->p->fts_statp;
    if (exists && Unlink(ct->p_path)) {
	warn("Unlink: %s", ct->p_path);
	return RPMRC_FAIL;
    }
    if (Mkfifo(ct->p_path, st->st_mode)) {
	warn("Mkfifo: %s", ct->p_path);
	return RPMRC_FAIL;
    }
    return (CP_ISSET(PRESERVE) ? rpmctSetFile(ct, NULL) : RPMRC_OK);
}

static rpmRC
rpmctCopySpecial(rpmct ct, int exists)
{
    struct stat * st = ct->p->fts_statp;
    if (exists && Unlink(ct->p_path)) {
	warn("Unlink: %s", ct->p_path);
	return RPMRC_FAIL;
    }
    if (Mknod(ct->p_path, st->st_mode, st->st_rdev)) {
	warn("mknod: %s", ct->p_path);
	return RPMRC_FAIL;
    }
    return (CP_ISSET(PRESERVE) ? rpmctSetFile(ct, NULL) : RPMRC_OK);
}

/* ----- */

/*
 * mastercmp --
 *	The comparison function for the copy order.  The order is to copy
 *	non-directory files before directory files.  The reason for this
 *	is because files tend to be in the same cylinder group as their
 *	parent directory, whereas directories tend not to be.  Copying the
 *	files first reduces seeking.
 */
static int
mastercmp(const FTSENT ** a, const FTSENT ** b)
{
    int a_info = (*a)->fts_info;
    int b_info = (*b)->fts_info;

    if (a_info == FTS_ERR || a_info == FTS_NS || a_info == FTS_DNR)
	return 0;
    if (b_info == FTS_ERR || b_info == FTS_NS || b_info == FTS_DNR)
	return 0;
    if (a_info == FTS_D)
	return -1;
    if (b_info == FTS_D)
	return 1;
    return 0;
}

#if defined(SIGINFO)
static void
siginfo(int sig __attribute__((__unused__)))
{
	info = 1;
}
#endif

static rpmRC
rpmctCopy(rpmct ct)
{
    int base = 0;
    int dne;
    int badcp = 0;
    rpmRC rval = RPMRC_OK;
    size_t nlen;
    char *p;
    char *target_mid;
    mode_t mask;
    mode_t mode;

    /*
     * Keep an inverted copy of the umask, for use in correcting
     * permissions on created directories when not using -p.
     */
    mask = ~umask(0777);
    umask(~mask);

    if ((ct->t = Fts_open((char *const *)ct->av, ct->ftsoptions, mastercmp)) == NULL)
	err(1, "Fts_open");
    while ((ct->p = Fts_read(ct->t)) != NULL) {
	switch (ct->p->fts_info) {
	case FTS_NS:
	case FTS_DNR:
	case FTS_ERR:
	    warnx("%s: %s", ct->p->fts_path, strerror(ct->p->fts_errno));
	    rval = RPMRC_FAIL;
	    continue;
	case FTS_DC:			/* Warn, continue. */
	    warnx("%s: directory causes a cycle", ct->p->fts_path);
	    rval = RPMRC_FAIL;
	    continue;
	default:
	    ;
	}

	/*
	 * If we are in case (2) or (3) above, we need to append the
	 * source name to the target name.
	 */
	if (ct->type != FILE_TO_FILE) {
	    /*
	     * Need to remember the roots of traversals to create
	     * correct pathnames.  If there's a directory being
	     * copied to a non-existent directory, e.g.
	     *	cp -R a/dir noexist
	     * the resulting path name should be noexist/foo, not
	     * noexist/dir/foo (where foo is a file in dir), which
	     * is the case where the target exists.
	     *
	     * Also, check for "..".  This is for correct path
	     * concatenation for paths ending in "..", e.g.
	     *	cp -R .. /tmp
	     * Paths ending in ".." are changed to ".".  This is
	     * tricky, but seems the easiest way to fix the problem.
	     *
	     * XXX
	     * Since the first level MUST be FTS_ROOTLEVEL, base
	     * is always initialized.
	     */
	    if (ct->p->fts_level == FTS_ROOTLEVEL) {
		if (ct->type != DIR_TO_DNE) {
		    p = strrchr(ct->p->fts_path, '/');
		    base = (p == NULL) ? 0 : (int)(p - ct->p->fts_path + 1);
		    if (!strcmp(&ct->p->fts_path[base], ".."))
			base += 1;
		} else
		    base = ct->p->fts_pathlen;
	    }

	    p = &ct->p->fts_path[base];
	    nlen = ct->p->fts_pathlen - base;
	    target_mid = ct->target_end;
	    if (*p != '/' && target_mid[-1] != '/')
		*target_mid++ = '/';
	    *target_mid = 0;
	    if (target_mid - ct->p_path + nlen >= PATH_MAX) {
		warnx("%s%s: name too long (not copied)", ct->p_path, p);
		rval = RPMRC_FAIL;
		continue;
	    }
	    (void)strncat(target_mid, p, nlen);
	    ct->p_end = target_mid + nlen;
	    *ct->p_end = '\0';
	    while (ct->p_end > ct->p_path + 1 && ct->p_end[-1] == '/')
		*--ct->p_end = '\0';
	}

	if (ct->p->fts_info == FTS_DP) {
	    /*
	     * We are nearly finished with this directory.  If we
	     * didn't actually copy it, or otherwise don't need to
	     * change its attributes, then we are done.
	     */
	    if (!ct->p->fts_number)
		continue;
	    /*
	     * If -p is in effect, set all the attributes.
	     * Otherwise, set the correct permissions, limited
	     * by the umask.  Optimise by avoiding a Chmod()
	     * if possible (which is usually the case if we
	     * made the directory).  Note that Mkdir() does not
	     * honour setuid, setgid and sticky bits, but we
	     * normally want to preserve them on directories.
	     */
	    mode = ct->p->fts_statp->st_mode;
	    if (CP_ISSET(PRESERVE)) {
		if (rpmctSetFile(ct, NULL))
		    rval = RPMRC_FAIL;
		if (rpmaclCopyDir(ct->p->fts_accpath, ct->p_path, mode) != 0)
		    rval = RPMRC_FAIL;
	    } else {
		if ((mode & (S_ISUID | S_ISGID | S_ISTXT))
		 || ((mode | S_IRWXU) & mask) != (mode & mask))
		    if (Chmod(ct->p_path, mode & mask) != 0) {
			warn("Chmod: %s", ct->p_path);
			rval = RPMRC_FAIL;
		    }
	    }
	    continue;
	}

	/* Not an error but need to remember it happened */
	if (Stat(ct->p_path, &ct->sb) == -1)
	    dne = 1;
	else {
	    if (ct->sb.st_dev == ct->p->fts_statp->st_dev &&
		ct->sb.st_ino == ct->p->fts_statp->st_ino)
	    {
		warnx("%s and %s are identical (not copied).",
		    ct->p_path, ct->p->fts_path);
		rval = RPMRC_FAIL;
		if (S_ISDIR(ct->p->fts_statp->st_mode))
		    (void)Fts_set(ct->t, ct->p, FTS_SKIP);
		continue;
	    }
	    if (!S_ISDIR(ct->p->fts_statp->st_mode) && S_ISDIR(ct->sb.st_mode)) {
		warnx("cannot overwrite directory %s with non-directory %s",
		    ct->p_path, ct->p->fts_path);
		rval = RPMRC_FAIL;
		continue;
	    }
	    dne = 0;
	}

	badcp = 0;
	switch (ct->p->fts_statp->st_mode & S_IFMT) {
	case S_IFLNK:
	    /* Catch special case of a non-dangling symlink */
	    if ((ct->ftsoptions & FTS_LOGICAL)
	     || ((ct->ftsoptions & FTS_COMFOLLOW) && ct->p->fts_level == 0))
	    {
		if (rpmctCopyFile(ct, dne))
		    badcp = 1;
	    } else {	
		if (rpmctCopyLink(ct, !dne))
		    badcp = 1;
	    }
	    break;
	case S_IFDIR:
	    if (!CP_ISSET(RECURSE)) {
		warnx("%s is a directory (not copied).", ct->p->fts_path);
		(void)Fts_set(ct->t, ct->p, FTS_SKIP);
		badcp = 1;
		break;
	    }
	    /*
	     * If the directory doesn't exist, create the new
	     * one with the from file mode plus owner RWX bits,
	     * modified by the umask.  Trade-off between being
	     * able to write the directory (if from directory is
	     * 555) and not causing a permissions race.  If the
	     * umask blocks owner writes, we fail..
	     */
	    if (dne) {
		if (Mkdir(ct->p_path, ct->p->fts_statp->st_mode | S_IRWXU) < 0)
		    err(1, "%s", ct->p_path);
	    } else if (!S_ISDIR(ct->sb.st_mode)) {
		errno = ENOTDIR;
		err(1, "%s", ct->p_path);
	    }
	    /*
	     * Arrange to correct directory attributes later
	     * (in the post-order phase) if this is a new
	     * directory, or if the -p flag is in effect.
	     */
	    ct->p->fts_number = CP_ISSET(PRESERVE) || dne;
	    break;
	case S_IFBLK:
	case S_IFCHR:
	    if (CP_ISSET(RECURSE)) {
		if (rpmctCopySpecial(ct, !dne))
		    badcp = 1;
	    } else {
		if (rpmctCopyFile(ct, dne))
		    badcp = 1;
	    }
	    break;
	case S_IFSOCK:
	    warnx("%s is a socket (not copied).", ct->p->fts_path);
	case S_IFIFO:
	    if (CP_ISSET(RECURSE)) {
		if (rpmctCopyFifo(ct, !dne))
		    badcp = 1;
	    } else {
		if (rpmctCopyFile(ct, dne))
		    badcp = 1;
	    }
	    break;
	default:
	    if (rpmctCopyFile(ct, dne))
		badcp = 1;
	    break;
	}
	if (badcp)
	    rval = RPMRC_FAIL;
	else if (CP_ISSET(VERBOSE))
	    (void)fprintf(stdout, "%s -> %s\n", ct->p->fts_path, ct->p_path);
    }

    if (errno)
	err(1, "Fts_read");
    Fts_close(ct->t);
    ct->t = NULL;
    ct->p = NULL;
    return rval;
}


/*==============================================================*/

/**
 */
static void copyArgCallback(poptContext con,
                /*@unused@*/ enum poptCallbackReason reason,
                const struct poptOption * opt, const char * arg,
                /*@unused@*/ void * data)
        /*@globals _rpmfts, rpmioFtsOpts, h_errno, fileSystem, internalState @*/
        /*@modifies _rpmfts, rpmioFtsOpts, fileSystem, internalState @*/
{
    rpmct ct = _ct;

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
    case 'H':
	ct->flags |= COPY_FLAGS_FOLLOWARGS;
	ct->flags &= ~COPY_FLAGS_FOLLOW;
	break;
    case 'L':
	ct->flags |= COPY_FLAGS_FOLLOW;
	ct->flags &= ~COPY_FLAGS_FOLLOWARGS;
	break;
    case 'P':
	ct->flags &= ~COPY_FLAGS_FOLLOWARGS;
	ct->flags &= ~COPY_FLAGS_FOLLOW;
	break;
    case 'R':
	ct->flags |= COPY_FLAGS_RECURSE;
	break;
    case 'a':
	ct->flags |= COPY_FLAGS_PRESERVE;
	ct->flags |= COPY_FLAGS_RECURSE;
	ct->flags &= ~COPY_FLAGS_FOLLOWARGS;
	ct->flags &= ~COPY_FLAGS_FOLLOW;
	break;
    case 'f':
	ct->flags |= COPY_FLAGS_FORCE;
	ct->flags &= ~COPY_FLAGS_INTERACTIVE;
	ct->flags &= ~COPY_FLAGS_NOCLOBBER;
	break;
    case 'i':
	ct->flags |= COPY_FLAGS_INTERACTIVE;
	ct->flags &= ~COPY_FLAGS_FORCE;
	ct->flags &= ~COPY_FLAGS_NOCLOBBER;
	break;
    case 'l':
	ct->flags |= COPY_FLAGS_HARDLINK;
	break;
    case 'n':
	ct->flags |= COPY_FLAGS_NOCLOBBER;
	ct->flags &= ~COPY_FLAGS_FORCE;
	ct->flags &= ~COPY_FLAGS_INTERACTIVE;
	break;
    case 'p':
	ct->flags |= COPY_FLAGS_PRESERVE;
	break;
    case 'r':
	ct->flags |= COPY_FLAGS_RECURSE;
	ct->flags |= COPY_FLAGS_FOLLOW;
	ct->flags &= ~COPY_FLAGS_FOLLOWARGS;
	break;
    case 'v':
	ct->flags |= COPY_FLAGS_VERBOSE;
	break;

    case '?':
    default:
	fprintf(stderr, _("%s: Unknown option -%c\n"), __progname, opt->val);
	poptPrintUsage(con, stderr, 0);
	exit(EXIT_FAILURE);
	/*@notreached@*/ break;
    }
}

/*@unchecked@*/ /*@observer@*/
static struct poptOption optionsTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        copyArgCallback, 0, NULL, NULL },
/*@=type@*/

  { NULL,'H', POPT_ARG_NONE,		NULL, (int)'H',
	N_("Follow command-line symbolic links"), NULL },
  { "dereference",'L', POPT_ARG_NONE,		NULL, (int)'L',
	N_("Always follow symbolic links"), NULL },
  { "nodereference",'P', POPT_ARG_NONE,		NULL, (int)'P',
	N_("Never follow symbolic links"), NULL },
  { "recursive",'R', POPT_ARG_NONE,		NULL, (int)'R',
	N_("Copy directories recursively"), NULL },
  { "archive",'a', POPT_ARG_NONE,	NULL, (int)'a',
	N_("Archive mode (same as -RpP)"), NULL },
  { "force",'f', POPT_ARG_NONE,		NULL, (int)'f',
	N_("Unlink and retry if 1st open fails"), NULL },
  { "interactive",'i', POPT_ARG_NONE,	NULL, (int)'i',
	N_("Prompt before overwriting a file"), NULL },
  { "link",'l', POPT_ARG_NONE,		NULL, (int)'l',
	N_("Link files instead of copying"), NULL },
  { "noclobber",'n', POPT_ARG_NONE,	NULL, (int)'n',
	N_("Do not overwrite an existing file"), NULL },
  { "preserve",'p', POPT_ARG_NONE,	NULL, (int)'p',
	N_("Preserve mode/ownership/timestamps"), NULL },
#ifdef	DYING
  /* XXX conflicts with -r, --root */
  { NULL,'r', POPT_ARG_NONE,		NULL, (int)'r',
	N_("Copy directories recursively"), NULL },
#endif
  { "verbose",'v', POPT_ARG_NONE,	NULL, (int)'v',
	N_("Explain what is being done"), NULL },

#ifdef	NOTYET
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioFtsPoptTable, 0,
	N_("Fts(3) traversal options:"), NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioDigestPoptTable, 0,
	N_("Available digests:"), NULL },
#endif

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"),
	NULL },

  POPT_AUTOALIAS
  POPT_AUTOHELP

  { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
        "\
Usage: cp [-R [-H | -L | -P]] [-f | -i | -n] [-alpv] source_file target_file\n\
       cp [-R [-H | -L | -P]] [-f | -i | -n] [-alpv] source_file ... target_directory\n\
", NULL },

  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    poptContext optCon = rpmioInit(argc, argv, optionsTable);
    rpmct ct = _ct;
    int r, have_trailing_slash;
    rpmRC rc = -1;

    if (sysconf(_SC_PHYS_PAGES) > PHYSPAGES_THRESHOLD)
	ct->ballocated = MIN(BUFSIZE_MAX, MAXPHYS * 8);
    else
	ct->ballocated = BUFSIZE_SMALL;

    ct->p_path[0] = '\0';
    ct->target_end = "";
    ct->p_end = ct->p_path;

    ct->av = poptGetArgs(optCon);
    ct->ac = argvCount(ct->av);
    if (ct->ac < 2) {
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }

    ct->ftsoptions = FTS_NOCHDIR | FTS_PHYSICAL;
    if (CP_ISSET(RECURSE)) {
	if (CP_ISSET(FOLLOWARGS))
	    ct->ftsoptions |= FTS_COMFOLLOW;
	if (CP_ISSET(FOLLOW)) {
	    ct->ftsoptions &= ~FTS_PHYSICAL;
	    ct->ftsoptions |= FTS_LOGICAL;
	}
    } else {
	ct->ftsoptions &= ~FTS_PHYSICAL;
	ct->ftsoptions |= FTS_LOGICAL | FTS_COMFOLLOW;
    }

#if defined(SIGINFO)
    (void)signal(SIGINFO, siginfo);
#endif

    /* Save the target base in "to". */
    {	const char * target = ct->av[--ct->ac];
	if (strlen(target) > sizeof(ct->p_path) - 2)
	    errx(1, "%s: name too long", target);
	(void) strcpy(ct->p_path, target);
    }
    ct->p_end = ct->p_path + strlen(ct->p_path);
    if (ct->p_path == ct->p_end) {
	*ct->p_end++ = '.';
	*ct->p_end = '\0';
    }
    have_trailing_slash = (ct->p_end[-1] == '/');
    if (have_trailing_slash)
	while (ct->p_end > ct->p_path + 1 && ct->p_end[-1] == '/')
	    *--ct->p_end = '\0';
    ct->target_end = ct->p_end;

    /* Set end of argument list for fts(3). */
    ct->av[ct->ac] = NULL;

    /*
     * Cp has two distinct cases:
     *
     * cp [-R] source target
     * cp [-R] source1 ... sourceN directory
     *
     * In both cases, source can be either a file or a directory.
     *
     * In (1), the target becomes a copy of the source. That is, if the
     * source is a file, the target will be a file, and likewise for
     * directories.
     *
     * In (2), the real target is not directory, but "directory/source".
     */
    r = Stat(ct->p_path, &ct->sb);
    if (r == -1 && errno != ENOENT)
	err(1, "%s", ct->p_path);
    if (r == -1 || !S_ISDIR(ct->sb.st_mode)) {
	/* Case (1).  Target is not a directory. */
	if (ct->ac > 1)
	    errx(1, "%s is not a directory", ct->p_path);

	/*
	 * Need to detect the case:
	 *	cp -R dir foo
	 * Where dir is a directory and foo does not exist, where
	 * we want pathname concatenations turned on but not for
	 * the initial Mkdir().
	 */
	if (r == -1) {
	    struct stat sb;
	    if (CP_ISSET(RECURSE) && (CP_ISSET(FOLLOW) || CP_ISSET(FOLLOWARGS)))
		Stat(ct->av[0], &sb);
	    else
		Lstat(ct->av[0], &sb);

	    if (S_ISDIR(sb.st_mode) && CP_ISSET(RECURSE))
		ct->type = DIR_TO_DNE;
	    else
		ct->type = FILE_TO_FILE;
	} else
	    ct->type = FILE_TO_FILE;

	if (have_trailing_slash && ct->type == FILE_TO_FILE) {
	    if (r == -1)
		errx(1, "directory %s does not exist", ct->p_path);
	    else
		errx(1, "%s is not a directory", ct->p_path);
	}
    } else
	/* Case (2).  Target is a directory. */
	ct->type = FILE_TO_DIR;

    rc = rpmctCopy(ct);

exit:
    ct->b = _free(ct->b);
    optCon = rpmioFini(optCon);
    return (rc == RPMRC_OK ? EXIT_SUCCESS : EXIT_FAILURE);
}

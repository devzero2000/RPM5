/*-
 * Copyright (c) 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1989, 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#include "system.h"

#include <err.h>		/* XXX warnx (needs to removed). */
#include <stdarg.h>		/* XXX <err.h> implies <stdarg.h> */
#if defined(__LCLINT__)
/*@-exportheader -incondefs @*/
extern void warnx(__const char * __format, ...)
#ifdef __GNUC__
__attribute__ ((format (printf, 1, 2)))
#endif
	/*@globals stderr, fileSystem @*/
	/*@modifies stderr, fileSystem @*/;
/*@=exportheader =incondefs @*/
#endif

#include <signal.h>
#include <fnmatch.h>

#include <rpmio_internal.h>	/* XXX fdInitDigest() et al */
#include <fts.h>
#include <poptIO.h>

/*==============================================================*/

#define	KEYDEFAULT \
	(F_GID | F_MODE | F_NLINK | F_SIZE | F_SLINK | F_TIME | F_UID)

#define	MISMATCHEXIT	2

#if !defined(S_ISTXT) && defined(S_ISVTX)	/* XXX linux != BSD */
#define	S_ISTXT		S_ISVTX
#endif

typedef struct _node {
	struct _node	*parent, *child;	/* up, down */
	struct _node	*prev, *next;		/* left, right */
	off_t	st_size;			/* size */
	struct timespec	st_mtimespec;		/* last modification time */
	uint32_t cksum;				/* check sum */
	char	*md5digest;			/* MD5 digest */
	char	*sha1digest;			/* SHA-1 digest */
	char	*sha256digest;			/* SHA-256 digest */
	char	*rmd160digest;			/* RIPEMD-160 digest */
	char	*slink;				/* symbolic link reference */
	uid_t	st_uid;				/* uid */
	gid_t	st_gid;				/* gid */
#define	MBITS	(S_ISUID|S_ISGID|S_ISTXT|S_IRWXU|S_IRWXG|S_IRWXO)
	mode_t	st_mode;			/* mode */
	nlink_t	st_nlink;			/* link count */
	unsigned long st_flags;			/* file flags */

#define	F_CKSUM		0x00000001		/* checksum */
#define	F_DONE		0x00000002		/* directory done */
#define	F_GID		0x00000004		/* gid */
#define	F_GNAME		0x00000008		/* group name */
#define	F_IGN		0x00000010		/* ignore */
#define	F_MAGIC		0x00000020		/* name has magic chars */
#define	F_MODE		0x00000040		/* mode */
#define	F_NLINK		0x00000080		/* number of links */
#define	F_SIZE		0x00000100		/* size */
#define	F_SLINK		0x00000200		/* link count */
#define	F_TIME		0x00000400		/* modification time */
#define	F_TYPE		0x00000800		/* file type */
#define	F_UID		0x00001000		/* uid */
#define	F_UNAME		0x00002000		/* user name */
#define	F_VISIT		0x00004000		/* file visited */
#define	F_FLAGS		0x00008000		/* file flags */
#define	F_NOCHANGE	0x00010000		/* do not change owner/mode */
#define F_OPT		0x00020000		/* existence optional */

#define	F_MD5		0x01000000		/* MD5 digest */
#define	F_RMD160	0x02000000		/* RIPEMD-160 digest */
#define	F_SHA1		0x04000000		/* SHA-1 digest */
#define	F_SHA256	0x08000000		/* SHA-256 digest */

	uint32_t flags;				/* items set */

#define	F_BLOCK	0x001				/* block special */
#define	F_CHAR	0x002				/* char special */
#define	F_DIR	0x004				/* directory */
#define	F_FIFO	0x008				/* fifo */
#define	F_FILE	0x010				/* regular file */
#define	F_LINK	0x020				/* symbolic link */
#define	F_SOCK	0x040				/* socket */
	uint8_t	type;				/* file type */

	char	name[1];			/* file name (must be last) */
} NODE;

#define	RP(p)	\
	((p)->fts_path[0] == '.' && (p)->fts_path[1] == '/' ? \
	    (p)->fts_path + 2 : (p)->fts_path)


#define	_MFB(n)	((1U << (n)) | 0x40000000)
#define MF_ISSET(_FLAG) ((mtreeFlags & ((MTREE_FLAGS_##_FLAG) & ~0x40000000)) != MTREE_FLAGS_NONE)

enum mtreeFlags_e {
    MTREE_FLAGS_NONE		= 0,
    MTREE_FLAGS_QUIET		= _MFB( 0), /*!< -q,--quiet ... */
    MTREE_FLAGS_WARN		= _MFB( 1), /*!< -w,--warn ... */
    MTREE_FLAGS_SPEC		= _MFB( 2), /*!< -c,--spec ... */
    MTREE_FLAGS_DIRSONLY	= _MFB( 3), /*!< -d,--dirs ... */
    MTREE_FLAGS_IGNORE		= _MFB( 4), /*!< -e,--ignore ... */
    MTREE_FLAGS_INDENT		= _MFB( 5), /*!< -i,--indent ... */
    MTREE_FLAGS_LOOSE		= _MFB( 6), /*!< -l,--loose ... */
    MTREE_FLAGS_NOCOMMENT	= _MFB( 7), /*!< -n,--nocomment ... */
    MTREE_FLAGS_REMOVE		= _MFB( 8), /*!< -r,--remove ... */
    MTREE_FLAGS_SEEDED		= _MFB( 9), /*!< -s,--seed ... */
    MTREE_FLAGS_TOUCH		= _MFB(10), /*!< -t,--touch ... */
    MTREE_FLAGS_UPDATE		= _MFB(11), /*!< -u,--update ... */
    MTREE_FLAGS_MISMATCHOK	= _MFB(12), /*!< -U,--mismatch ... */
};

/*@unchecked@*/
static enum mtreeFlags_e mtreeFlags = MTREE_FLAGS_NONE;

typedef struct rpmfts_s * rpmfts;

struct rpmfts_s {
    FTS * t;
    FTSENT * p;
    uid_t uid;
    gid_t gid;
    mode_t mode;
    unsigned long fflags;
    uint32_t crc_total;
    unsigned lineno;
/*@null@*/
    NODE * root;
/*@null@*/
    ARGV_t paths;
    unsigned keys;
/*@null@*/
    const char * fullpath;
/*@null@*/
    char * path;
    int ftsoptions;
};

/*@unchecked@*/
static struct rpmfts_s __rpmfts;
/*@unchecked@*/
static rpmfts _rpmfts = &__rpmfts;;

#ifdef __cplusplus
extern "C" {
#endif

static int mtreeCWalk(rpmfts fts)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies fts, fileSystem, internalState @*/;

/*@null@*/
static NODE * mtreeSpec(rpmfts fts)
	/*@globals fileSystem, internalState @*/
	/*@modifies fts, fileSystem, internalState @*/;

static int mtreeVWalk(rpmfts fts)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies fts, fileSystem, internalState @*/;

static void mtreeMiss(rpmfts fts, /*@null@*/ NODE * p, char * tail)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies p, tail, fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

/*==============================================================*/

#include "debug.h"

#if !defined(POPT_ARG_ARGV)
static int _poptSaveString(const char ***argvp, unsigned int argInfo, const char * val)
	/*@*/
{
    ARGV_t argv;
    int argc = 0;
    if (argvp == NULL)
	return -1;
    if (*argvp)
    while ((*argvp)[argc] != NULL)
	argc++;
    *argvp = xrealloc(*argvp, (argc + 1 + 1) * sizeof(**argvp));
    if ((argv = *argvp) != NULL) {
	argv[argc++] = xstrdup(val);
	argv[argc  ] = NULL;
    }
    return 0;
}
#endif

/*==============================================================*/

/*@exits@*/
static void
mtree_error(const char *fmt, ...)
#ifdef __GNUC__
__attribute__ ((format (printf, 1, 2)))
#endif
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;

void
mtree_error(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    (void) fflush(NULL);
    (void) fprintf(stderr, "\n%s: ", __progname);
    (void) vfprintf(stderr, fmt, ap);
    va_end (ap);
    (void) fprintf(stderr, "\n");
    if (_rpmfts->lineno)
	(void)fprintf(stderr, _("%s: failed at line %d of the specification\n"),
		__progname, _rpmfts->lineno);
    exit(EXIT_FAILURE);
    /*@notreached@*/
}

typedef struct _key {
/*@observer@*/
	const char *name;		/* key name */
	unsigned val;			/* value */
#define	NEEDVALUE	0x01
	unsigned flags;
} KEY;

/* NB: the following table must be sorted lexically. */
/*@unchecked@*/ /*@observer@*/
static KEY keylist[] = {
	{"cksum",	F_CKSUM,	NEEDVALUE},
	{"flags",	F_FLAGS,	NEEDVALUE},
	{"gid",		F_GID,		NEEDVALUE},
	{"gname",	F_GNAME,	NEEDVALUE},
	{"ignore",	F_IGN,		0},
	{"link",	F_SLINK,	NEEDVALUE},
	{"md5digest",	F_MD5,		NEEDVALUE},
	{"mode",	F_MODE,		NEEDVALUE},
	{"nlink",	F_NLINK,	NEEDVALUE},
	{"nochange",	F_NOCHANGE,	0},
	{"optional",	F_OPT,		0},
	{"rmd160digest",F_RMD160,	NEEDVALUE},
	{"sha1digest",	F_SHA1,		NEEDVALUE},
	{"sha256digest",F_SHA256,	NEEDVALUE},
	{"size",	F_SIZE,		NEEDVALUE},
	{"time",	F_TIME,		NEEDVALUE},
	{"type",	F_TYPE,		NEEDVALUE},
	{"uid",		F_UID,		NEEDVALUE},
	{"uname",	F_UNAME,	NEEDVALUE},
};

static int
keycompare(const void * a, const void * b)
	/*@*/
{
    return strcmp(((KEY *)a)->name, ((KEY *)b)->name);
}

static unsigned
parsekey(char *name, /*@out@*/ int *needvaluep)
	/*@globals fileSystem @*/
	/*@modifies *needvaluep, fileSystem @*/

{
    KEY *k, tmp;

    tmp.name = name;
    k = (KEY *)bsearch(&tmp, keylist, sizeof(keylist) / sizeof(keylist[0]),
	    sizeof(keylist[0]), keycompare);
    if (k == NULL)
	mtree_error("unknown keyword %s", name);

    if (needvaluep != NULL)
	*needvaluep = k->flags & NEEDVALUE ? 1 : 0;
    return k->val;
}

#ifdef	NOTYET
static char *
flags_to_string(u_long fflags)
{
    char *string;

    string = fflagstostr(fflags);
    if (string != NULL && *string == '\0') {
	free(string);
	string = strdup("none");
    }
    if (string == NULL)
	err(1, NULL);

    return string;
}
#endif

/*==============================================================*/

/*@unchecked@*/ /*@observer@*/
static const uint32_t crctab[] = {
    0x0,
    0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
    0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6,
    0x2b4bcb61, 0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
    0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9, 0x5f15adac,
    0x5bd4b01b, 0x569796c2, 0x52568b75, 0x6a1936c8, 0x6ed82b7f,
    0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3, 0x709f7b7a,
    0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
    0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58,
    0xbaea46ef, 0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033,
    0xa4ad16ea, 0xa06c0b5d, 0xd4326d90, 0xd0f37027, 0xddb056fe,
    0xd9714b49, 0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
    0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1, 0xe13ef6f4,
    0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
    0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5,
    0x2ac12072, 0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16,
    0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca, 0x7897ab07,
    0x7c56b6b0, 0x71159069, 0x75d48dde, 0x6b93dddb, 0x6f52c06c,
    0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1,
    0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
    0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b,
    0xbb60adfc, 0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698,
    0x832f1041, 0x87ee0df6, 0x99a95df3, 0x9d684044, 0x902b669d,
    0x94ea7b2a, 0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e,
    0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2, 0xc6bcf05f,
    0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
    0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80,
    0x644fc637, 0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
    0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f, 0x5c007b8a,
    0x58c1663d, 0x558240e4, 0x51435d53, 0x251d3b9e, 0x21dc2629,
    0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5, 0x3f9b762c,
    0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
    0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e,
    0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65,
    0xeba91bbc, 0xef68060b, 0xd727bbb6, 0xd3e6a601, 0xdea580d8,
    0xda649d6f, 0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
    0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7, 0xae3afba2,
    0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
    0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74,
    0x857130c3, 0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640,
    0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c, 0x7b827d21,
    0x7f436096, 0x7200464f, 0x76c15bf8, 0x68860bfd, 0x6c47164a,
    0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e, 0x18197087,
    0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
    0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d,
    0x2056cd3a, 0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce,
    0xcc2b1d17, 0xc8ea00a0, 0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb,
    0xdbee767c, 0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18,
    0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4, 0x89b8fd09,
    0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
    0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf,
    0xa2f33668, 0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

/*
 * Compute a POSIX 1003.2 checksum.  This routine has been broken out so that
 * other programs can use it.  It takes a file descriptor to read from and
 * locations to store the crc and the number of bytes read.  It returns 0 on
 * success and 1 on failure.  Errno is set on failure.
 */
static int
crc(FD_t fd, /*@out@*/ uint32_t * cval, /*@out@*/ uint32_t * clen)
	/*@globals _rpmfts, fileSystem @*/
	/*@modifies fd, *clen, *cval, _rpmfts, fileSystem @*/
{
    uint32_t crc = 0;
    uint32_t len = 0;

#define	COMPUTE(var, ch)	(var) = (var) << 8 ^ crctab[(var) >> 24 ^ (ch)]

    _rpmfts->crc_total ^= 0xffffffff;

    {   uint8_t buf[16 * 1024];
	size_t nr;
	while ((nr = Fread(buf, sizeof(buf[0]), sizeof(buf), fd)) != 0) {
	    uint8_t *p;
	    for (len += nr, p = buf; nr--; ++p) {
		COMPUTE(crc, *p);
		COMPUTE(_rpmfts->crc_total, *p);
	    }
	}
	if (Ferror(fd))
	    return 1;
    }

    *clen = len;

    /* Include the length of the file. */
    for (; len != 0; len >>= 8) {
	COMPUTE(crc, len & 0xff);
	COMPUTE(_rpmfts->crc_total, len & 0xff);
    }

    *cval = (crc ^ 0xffffffff);
    _rpmfts->crc_total ^= 0xffffffff;
    return 0;
}

/*==============================================================*/

/*
 * to select alternate encoding format
 */
#define	VIS_OCTAL	0x01	/* use octal \ddd format */
#define	VIS_CSTYLE	0x02	/* use \[nrft0..] where appropriate */

/*
 * to alter set of characters encoded (default is to encode all
 * non-graphic except space, tab, and newline).
 */
#define	VIS_SP		0x04	/* also encode space */
#define	VIS_TAB		0x08	/* also encode tab */
#define	VIS_NL		0x10	/* also encode newline */
#define	VIS_WHITE	(VIS_SP | VIS_TAB | VIS_NL)
#define	VIS_SAFE	0x20	/* only encode "unsafe" characters */

/*
 * other
 */
#define	VIS_NOSLASH	0x40	/* inhibit printing '\' */

/*
 * unvis return codes
 */
#define	UNVIS_VALID	 1	/* character valid */
#define	UNVIS_VALIDPUSH	 2	/* character valid, push back passed char */
#define	UNVIS_NOCHAR	 3	/* valid sequence, no character produced */
#define	UNVIS_SYNBAD	-1	/* unrecognized escape sequence */
#define	UNVIS_ERROR	-2	/* decoder in unknown state (unrecoverable) */

/*
 * unvis flags
 */
#define	UNVIS_END	1	/* no more characters */

static char *vis(/*@returned@*/ /*@out@*/ char *dst, int c, int flag, int nextc)
	/*@modifies dst @*/;
static int strvis(/*@out@*/ char *dst, const char *src, int flag)
	/*@modifies dst @*/;
#ifdef	NOTUSED
static int strnvis(/*@out@*/ char *dst, const char *src, size_t siz, int flag)
	/*@modifies dst @*/;
static int strvisx(/*@out@*/ char *dst, const char *src, size_t len, int flag)
	/*@modifies dst @*/;
#endif
static int strunvis(/*@out@*/ char *dst, const char *src)
	/*@modifies dst @*/;
static int unvis(/*@out@*/ char *cp, char c, int *astate, int flag)
	/*@modifies cp, astate @*/;

#define	isoctal(c) (((unsigned char)(c)) >= '0' && ((unsigned char)(c)) <= '7')
#define isvisible(c)	\
  (((unsigned)(c) <= (unsigned)UCHAR_MAX && isascii((unsigned char)(c)) && \
	isgraph((unsigned char)(c)))			\
   ||	((flag & VIS_SP) == 0 && (c) == (int)' ')	\
   ||	((flag & VIS_TAB) == 0 && (c) == (int)'\t')	\
   ||	((flag & VIS_NL) == 0 && (c) == (int)'\n')	\
   ||	((flag & VIS_SAFE) \
	 && ((c) == (int)'\b' || (c) == (int)'\007' || (c) == (int)'\r')))

/*
 * vis - visually encode characters
 */
char *
vis(char * dst, int c, int flag, int nextc)
{
    if (isvisible(c)) {
	*dst++ = (char)c;
	if (c == (int)'\\' && (flag & VIS_NOSLASH) == 0)
	    *dst++ = '\\';
	*dst = '\0';
	return dst;
    }

    if (flag & VIS_CSTYLE) {
	switch(c) {
	case '\n':
	    *dst++ = '\\';
	    *dst++ = 'n';
	    goto done;
	case '\r':
	    *dst++ = '\\';
	    *dst++ = 'r';
	    goto done;
	case '\b':
	    *dst++ = '\\';
	    *dst++ = 'b';
	    goto done;
	case '\a':
	    *dst++ = '\\';
	    *dst++ = 'a';
	    goto done;
	case '\v':
	    *dst++ = '\\';
	    *dst++ = 'v';
	    goto done;
	case '\t':
	    *dst++ = '\\';
	    *dst++ = 't';
	    goto done;
	case '\f':
	    *dst++ = '\\';
	    *dst++ = 'f';
	    goto done;
	case ' ':
	    *dst++ = '\\';
	    *dst++ = 's';
	    goto done;
	case '\0':
	    *dst++ = '\\';
	    *dst++ = '0';
	    if (isoctal(nextc)) {
		*dst++ = '0';
		*dst++ = '0';
	    }
	    goto done;
	}
    }
    if (((c & 0177) == (int)' ') || (flag & VIS_OCTAL)) {	
	*dst++ = '\\';
	*dst++ = ((unsigned char)c >> 6 & 07) + '0';
	*dst++ = ((unsigned char)c >> 3 & 07) + '0';
	*dst++ = ((unsigned char)c & 07) + '0';
	goto done;
    }
    if ((flag & VIS_NOSLASH) == 0)
	*dst++ = '\\';
    if (c & 0200) {
	c &= 0177;
	*dst++ = 'M';
    }
    if (iscntrl(c)) {
	*dst++ = '^';
	if (c == 0177)
	    *dst++ = '?';
	else
	    *dst++ = (char)(c + (int)'@');
    } else {
	*dst++ = '-';
	*dst++ = (char)c;
    }

done:
    *dst = '\0';
    return dst;
}

/*
 * strvis, strnvis, strvisx - visually encode characters from src into dst
 *	
 *	Dst must be 4 times the size of src to account for possible
 *	expansion.  The length of dst, not including the trailing NULL,
 *	is returned. 
 *
 *	Strnvis will write no more than siz-1 bytes (and will NULL terminate).
 *	The number of bytes needed to fully encode the string is returned.
 *
 *	Strvisx encodes exactly len bytes from src into dst.
 *	This is useful for encoding a block of data.
 */
int
strvis(char * dst, const char * src, int flag)
{
    char c;
    char *start;

    for (start = dst; (c = *src) != '\0';)
	dst = vis(dst, (int)c, flag, (int)*++src);
    *dst = '\0';
    return (dst - start);
}

#ifdef	NOTUSED
int
strnvis(char * dst, const char * src, size_t siz, int flag)
{
    char c;
    char *start, *end;

    for (start = dst, end = start + siz - 1; (c = *src) && dst < end; ) {
	if (isvisible((int)c)) {
	    *dst++ = c;
	    if (c == '\\' && (flag & VIS_NOSLASH) == 0) {
		/* need space for the extra '\\' */
		if (dst < end)
		    *dst++ = '\\';
		else {
		    dst--;
		    break;
		}
	    }
	    src++;
	} else {
	    /* vis(3) requires up to 4 chars */
	    if (dst + 3 < end)
		dst = vis(dst, (int)c, flag, (int)*++src);
	    else
		break;
	}
    }
    *dst = '\0';
    if (dst >= end) {
	char tbuf[5];

	/* adjust return value for truncation */
	while ((c = *src) != '\0')
	    dst += vis(tbuf, (int)c, flag, (int)*++src) - tbuf;
    }
    return (dst - start);
}
#endif	/* NOTUSED */

#ifdef	NOTUSED
int
strvisx(char * dst, const char * src, size_t len, int flag)
{
    char c;
    char *start;

    for (start = dst; len > 1; len--) {
	c = *src;
	dst = vis(dst, (int)c, flag, (int)*++src);
    }
    if (len)
	dst = vis(dst, (int)*src, flag, (int)'\0');
    *dst = '\0';
    return (dst - start);
}
#endif	/* NOTUSED */

/*
 * decode driven by state machine
 */
#define	S_GROUND	0	/* haven't seen escape char */
#define	S_START		1	/* start decoding special sequence */
#define	S_META		2	/* metachar started (M) */
#define	S_META1		3	/* metachar more, regular char (-) */
#define	S_CTRL		4	/* control char started (^) */
#define	S_OCTAL2	5	/* octal digit 2 */
#define	S_OCTAL3	6	/* octal digit 3 */

#if !defined(isoctal)
#define	isoctal(c) (((unsigned char)(c)) >= '0' && ((unsigned char)(c)) <= '7')
#endif

/*
 * unvis - decode characters previously encoded by vis
 */
int
unvis(char *cp, char c, int *astate, int flag)
{

    if (flag & UNVIS_END) {
	if (*astate == S_OCTAL2 || *astate == S_OCTAL3) {
	    *astate = S_GROUND;
	    return (UNVIS_VALID);
	} 
	return (*astate == S_GROUND ? UNVIS_NOCHAR : UNVIS_SYNBAD);
    }

    switch (*astate) {

    case S_GROUND:
	*cp = '\0';
	if (c == '\\') {
	    *astate = S_START;
	    return (0);
	} 
	*cp = c;
	return (UNVIS_VALID);

    case S_START:
	switch(c) {
	case '\\':
	    *cp = c;
	    *astate = S_GROUND;
	    return (UNVIS_VALID);
	case '0': case '1': case '2': case '3':
	case '4': case '5': case '6': case '7':
	    *cp = (c - '0');
	    *astate = S_OCTAL2;
	    return (0);
	case 'M':
	    *cp = (char) 0200;
	    *astate = S_META;
	    return (0);
	case '^':
	    *astate = S_CTRL;
	    return (0);
	case 'n':
	    *cp = '\n';
	    *astate = S_GROUND;
	    return (UNVIS_VALID);
	case 'r':
	    *cp = '\r';
	    *astate = S_GROUND;
	    return (UNVIS_VALID);
	case 'b':
	    *cp = '\b';
	    *astate = S_GROUND;
	    return (UNVIS_VALID);
	case 'a':
	    *cp = '\007';
	    *astate = S_GROUND;
	    return (UNVIS_VALID);
	case 'v':
	    *cp = '\v';
	    *astate = S_GROUND;
	    return (UNVIS_VALID);
	case 't':
	    *cp = '\t';
	    *astate = S_GROUND;
	    return (UNVIS_VALID);
	case 'f':
	    *cp = '\f';
	    *astate = S_GROUND;
	    return (UNVIS_VALID);
	case 's':
	    *cp = ' ';
	    *astate = S_GROUND;
	    return (UNVIS_VALID);
	case 'E':
	    *cp = '\033';
	    *astate = S_GROUND;
	    return (UNVIS_VALID);
	case '\n':	/* hidden newline */
	    *astate = S_GROUND;
	    return (UNVIS_NOCHAR);
	case '$':	/* hidden marker */
	    *astate = S_GROUND;
	    return (UNVIS_NOCHAR);
	}
	*astate = S_GROUND;
	return (UNVIS_SYNBAD);
		 
    case S_META:
	if (c == '-')
	    *astate = S_META1;
	else if (c == '^')
	    *astate = S_CTRL;
	else {
	    *astate = S_GROUND;
	    return (UNVIS_SYNBAD);
	}
	return (0);
		 
    case S_META1:
	*astate = S_GROUND;
	*cp |= c;
	return (UNVIS_VALID);
		 
    case S_CTRL:
	if (c == '?')
	    *cp |= 0177;
	else
	    *cp |= c & 037;
	*astate = S_GROUND;
	return (UNVIS_VALID);

    case S_OCTAL2:	/* second possible octal digit */
	if (isoctal(c)) {
	    /* 
	     * yes - and maybe a third 
	     */
	    *cp = (*cp << 3) + (c - '0');
	    *astate = S_OCTAL3;	
	    return (0);
	} 
	/* 
	 * no - done with current sequence, push back passed char 
	 */
	*astate = S_GROUND;
	return (UNVIS_VALIDPUSH);

    case S_OCTAL3:	/* third possible octal digit */
	*astate = S_GROUND;
	if (isoctal(c)) {
	    *cp = (*cp << 3) + (c - '0');
	    return (UNVIS_VALID);
	}
	/*
	 * we were done, push back passed char
	 */
	return (UNVIS_VALIDPUSH);
			
    default:	
	/* 
	 * decoder in unknown state - (probably uninitialized) 
	 */
	*astate = S_GROUND;
	return (UNVIS_SYNBAD);
    }
}

/*
 * strunvis - decode src into dst 
 *
 *	Number of chars decoded into dst is returned, -1 on error.
 *	Dst is null terminated.
 */

int
strunvis(char * dst, const char * src)
{
    char c;
    char *start = dst;
    int state = 0;

    while ((c = *src++) != '\0') {
    again:
	switch (unvis(dst, c, &state, 0)) {
	case UNVIS_VALID:
	    dst++;
	    /*@switchbreak@*/ break;
	case UNVIS_VALIDPUSH:
	    dst++;
	    goto again;
	case 0:
	case UNVIS_NOCHAR:
	    /*@switchbreak@*/ break;
	default:
	    return (-1);
	}
    }
    if (unvis(dst, c, &state, UNVIS_END) == UNVIS_VALID)
	dst++;
    *dst = '\0';
    return (dst - start);
}

/*==============================================================*/

/* XXX *BSD systems already have getmode(3) and setmode(3) */
#if defined(__linux__) || defined(__LCLINT__)
#if !defined(HAVE_GETMODE) || !defined(HAVE_SETMODE)

#define	SET_LEN	6		/* initial # of bitcmd struct to malloc */
#define	SET_LEN_INCR 4		/* # of bitcmd structs to add as needed */

typedef struct bitcmd {
    char	cmd;
    char	cmd2;
    mode_t	bits;
} BITCMD;

#define	CMD2_CLR	0x01
#define	CMD2_SET	0x02
#define	CMD2_GBITS	0x04
#define	CMD2_OBITS	0x08
#define	CMD2_UBITS	0x10

#if !defined(HAVE_GETMODE)
/*
 * Given the old mode and an array of bitcmd structures, apply the operations
 * described in the bitcmd structures to the old mode, and return the new mode.
 * Note that there is no '=' command; a strict assignment is just a '-' (clear
 * bits) followed by a '+' (set bits).
 */
static mode_t
getmode(const void * bbox, mode_t omode)
	/*@*/
{
    const BITCMD *set;
    mode_t clrval, newmode, value;

    set = (const BITCMD *)bbox;
    newmode = omode;
    for (value = 0;; set++)
	switch(set->cmd) {
	/*
	 * When copying the user, group or other bits around, we "know"
	 * where the bits are in the mode so that we can do shifts to
	 * copy them around.  If we don't use shifts, it gets real
	 * grundgy with lots of single bit checks and bit sets.
	 */
	case 'u':
	    value = (newmode & S_IRWXU) >> 6;
	    goto common;

	case 'g':
	    value = (newmode & S_IRWXG) >> 3;
	    goto common;

	case 'o':
	    value = newmode & S_IRWXO;
common:	    if ((set->cmd2 & CMD2_CLR) != '\0') {
		clrval = (set->cmd2 & CMD2_SET) != '\0' ?  S_IRWXO : value;
		if ((set->cmd2 & CMD2_UBITS) != '\0')
		    newmode &= ~((clrval<<6) & set->bits);
		if ((set->cmd2 & CMD2_GBITS) != '\0')
		    newmode &= ~((clrval<<3) & set->bits);
		if ((set->cmd2 & CMD2_OBITS) != '\0')
		    newmode &= ~(clrval & set->bits);
	    }
	    if ((set->cmd2 & CMD2_SET) != '\0') {
		if ((set->cmd2 & CMD2_UBITS) != '\0')
		    newmode |= (value<<6) & set->bits;
		if ((set->cmd2 & CMD2_GBITS) != '\0')
		    newmode |= (value<<3) & set->bits;
		if ((set->cmd2 & CMD2_OBITS) != '\0')
		    newmode |= value & set->bits;
	    }
	    /*@switchbreak@*/ break;

	case '+':
	    newmode |= set->bits;
	    /*@switchbreak@*/ break;

	case '-':
	    newmode &= ~set->bits;
	    /*@switchbreak@*/ break;

	case 'X':
	    if (omode & (S_IFDIR|S_IXUSR|S_IXGRP|S_IXOTH))
			newmode |= set->bits;
	    /*@switchbreak@*/ break;

	case '\0':
	default:
#ifdef SETMODE_DEBUG
	    (void) printf("getmode:%04o -> %04o\n", omode, newmode);
#endif
	    return newmode;
	}
}
#endif	/* !defined(HAVE_GETMODE) */

#if !defined(HAVE_SETMODE)
#ifdef SETMODE_DEBUG
static void
dumpmode(BITCMD *set)
{
    for (; set->cmd; ++set)
	(void) printf("cmd: '%c' bits %04o%s%s%s%s%s%s\n",
	    set->cmd, set->bits, set->cmd2 ? " cmd2:" : "",
	    set->cmd2 & CMD2_CLR ? " CLR" : "",
	    set->cmd2 & CMD2_SET ? " SET" : "",
	    set->cmd2 & CMD2_UBITS ? " UBITS" : "",
	    set->cmd2 & CMD2_GBITS ? " GBITS" : "",
	    set->cmd2 & CMD2_OBITS ? " OBITS" : "");
}
#endif

#define	ADDCMD(a, b, c, d)					\
    if (set >= endset) {					\
	BITCMD *newset;						\
	setlen += SET_LEN_INCR;					\
	newset = realloc(saveset, sizeof(*newset) * setlen);	\
	if (newset == NULL) {					\
		if (saveset != NULL)				\
			free(saveset);				\
		saveset = NULL;					\
		return (NULL);					\
	}							\
	set = newset + (set - saveset);				\
	saveset = newset;					\
	endset = newset + (setlen - 2);				\
    }								\
    set = addcmd(set, (a), (b), (c), (d))

#define	STANDARD_BITS	(S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO)

static BITCMD *
addcmd(/*@returned@*/ BITCMD *set, int op, int who, int oparg, unsigned mask)
	/*@modifies set @*/
{
    switch (op) {
    case '=':
	set->cmd = '-';
	set->bits = who ? who : (int) STANDARD_BITS;
	set++;

	op = (int)'+';
	/*@fallthrough@*/
    case '+':
    case '-':
    case 'X':
	set->cmd = (char)op;
	set->bits = (who ? (unsigned)who : mask) & oparg;
	break;

    case 'u':
    case 'g':
    case 'o':
	set->cmd = (char)op;
	if (who) {
	    set->cmd2 = (char)( ((who & S_IRUSR) ? CMD2_UBITS : 0) |
				((who & S_IRGRP) ? CMD2_GBITS : 0) |
				((who & S_IROTH) ? CMD2_OBITS : 0));
	    set->bits = (mode_t)~0;
	} else {
	    set->cmd2 =(char)(CMD2_UBITS | CMD2_GBITS | CMD2_OBITS);
	    set->bits = mask;
	}
	
	if (oparg == (int)'+')
	    set->cmd2 |= CMD2_SET;
	else if (oparg == (int)'-')
	    set->cmd2 |= CMD2_CLR;
	else if (oparg == (int)'=')
	    set->cmd2 |= CMD2_SET|CMD2_CLR;
	break;
    }
    return set + 1;
}

/*
 * Given an array of bitcmd structures, compress by compacting consecutive
 * '+', '-' and 'X' commands into at most 3 commands, one of each.  The 'u',
 * 'g' and 'o' commands continue to be separate.  They could probably be
 * compacted, but it's not worth the effort.
 */
static void
compress_mode(/*@out@*/ BITCMD *set)
	/*@modifies set @*/
{
    BITCMD *nset;
    int setbits, clrbits, Xbits, op;

    for (nset = set;;) {
	/* Copy over any 'u', 'g' and 'o' commands. */
	while ((op = (int)nset->cmd) != (int)'+' && op != (int)'-' && op != (int)'X') {
	    *set++ = *nset++;
	    if (!op)
		return;
	}

	for (setbits = clrbits = Xbits = 0;; nset++) {
	    if ((op = (int)nset->cmd) == (int)'-') {
		clrbits |= nset->bits;
		setbits &= ~nset->bits;
		Xbits &= ~nset->bits;
	    } else if (op == (int)'+') {
		setbits |= nset->bits;
		clrbits &= ~nset->bits;
		Xbits &= ~nset->bits;
	    } else if (op == (int)'X')
		Xbits |= nset->bits & ~setbits;
	    else
		/*@innerbreak@*/ break;
	}
	if (clrbits) {
	    set->cmd = '-';
	    set->cmd2 = '\0';
	    set->bits = clrbits;
	    set++;
	}
	if (setbits) {
	    set->cmd = '+';
	    set->cmd2 = '\0';
	    set->bits = setbits;
	    set++;
	}
	if (Xbits) {
	    set->cmd = 'X';
	    set->cmd2 = '\0';
	    set->bits = Xbits;
	    set++;
	}
    }
}

/*@-usereleased@*/
/*@null@*/
static void *
setmode(const char * p)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    int perm, who;
    char op;
    BITCMD *set, *saveset, *endset;
    sigset_t sigset, sigoset;
    mode_t mask;
    int equalopdone = 0;
    int permXbits, setlen;
    long perml;

    if (!*p)
	return (NULL);

    /*
     * Get a copy of the mask for the permissions that are mask relative.
     * Flip the bits, we want what's not set.  Since it's possible that
     * the caller is opening files inside a signal handler, protect them
     * as best we can.
     */
    (void) sigfillset(&sigset);
    (void) sigprocmask(SIG_BLOCK, &sigset, &sigoset);
    (void) umask(mask = umask(0));
    mask = ~mask;
    (void) sigprocmask(SIG_SETMASK, &sigoset, NULL);

    setlen = SET_LEN + 2;
	
    if ((set = malloc((unsigned)(sizeof(*set) * setlen))) == NULL)
	return (NULL);
    saveset = set;
    endset = set + (setlen - 2);

    /*
     * If an absolute number, get it and return; disallow non-octal digits
     * or illegal bits.
     */
    if (isdigit(*p)) {
	perml = strtol(p, NULL, 8);
/*@-unrecog@*/
	if (perml < 0 || (perml & ~(STANDARD_BITS|S_ISTXT)))
/*@=unrecog@*/
	{
	    free(saveset);
	    return (NULL);
	}
	perm = (int)(mode_t)perml;
	while (*++p != '\0')
	    if (*p < '0' || *p > '7') {
		free(saveset);
		return (NULL);
	    }
	ADDCMD((int)'=', (int)(STANDARD_BITS|S_ISTXT), perm, (unsigned)mask);
	return (saveset);
    }

    /*
     * Build list of structures to set/clear/copy bits as described by
     * each clause of the symbolic mode.
     */
    for (;;) {
	/* First, find out which bits might be modified. */
	for (who = 0;; ++p) {
	    switch (*p) {
	    case 'a':
		who |= STANDARD_BITS;
		/*@switchbreak@*/ break;
	    case 'u':
		who |= S_ISUID|S_IRWXU;
		/*@switchbreak@*/ break;
	    case 'g':
		who |= S_ISGID|S_IRWXG;
		/*@switchbreak@*/ break;
	    case 'o':
		who |= S_IRWXO;
		/*@switchbreak@*/ break;
	    default:
		goto getop;
	    }
	}

getop:	if ((op = *p++) != '+' && op != '-' && op != '=') {
	    free(saveset);
	    return (NULL);
	}
	if (op == '=')
	    equalopdone = 0;

	who &= ~S_ISTXT;
	for (perm = 0, permXbits = 0;; ++p) {
	    switch (*p) {
	    case 'r':
		perm |= S_IRUSR|S_IRGRP|S_IROTH;
		/*@switchbreak@*/ break;
	    case 's':
		/*
		 * If specific bits where requested and
		 * only "other" bits ignore set-id.
		 */
		if (who == 0 || (who & ~S_IRWXO))
		    perm |= S_ISUID|S_ISGID;
		/*@switchbreak@*/ break;
	    case 't':
		/*
		 * If specific bits where requested and
		 * only "other" bits ignore sticky.
		 */
		if (who == 0 || (who & ~S_IRWXO)) {
		    who |= S_ISTXT;
		    perm |= S_ISTXT;
		}
		/*@switchbreak@*/ break;
	    case 'w':
		perm |= S_IWUSR|S_IWGRP|S_IWOTH;
		/*@switchbreak@*/ break;
	    case 'X':
		permXbits = (int)(S_IXUSR|S_IXGRP|S_IXOTH);
		/*@switchbreak@*/ break;
	    case 'x':
		perm |= S_IXUSR|S_IXGRP|S_IXOTH;
		/*@switchbreak@*/ break;
	    case 'u':
	    case 'g':
	    case 'o':
		/*
		 * When ever we hit 'u', 'g', or 'o', we have
		 * to flush out any partial mode that we have,
		 * and then do the copying of the mode bits.
		 */
		if (perm) {
		    ADDCMD((int)op, who, perm, (unsigned)mask);
		    perm = 0;
		}
		if (op == '=')
		    equalopdone = 1;
		if (op == '+' && permXbits) {
		    ADDCMD((int)'X', who, permXbits, (unsigned)mask);
		    permXbits = 0;
		}
		ADDCMD((int)*p, who, (int)op, (unsigned)mask);
		/*@switchbreak@*/ break;

	    default:
		/*
		 * Add any permissions that we haven't already
		 * done.
		 */
		if (perm || (op == '=' && !equalopdone)) {
		    if (op == '=')
			equalopdone = 1;
		    ADDCMD((int)op, who, perm, (unsigned)mask);
		    perm = 0;
		}
		if (permXbits) {
		    ADDCMD((int)'X', who, permXbits, (unsigned)mask);
		    permXbits = 0;
		}
		goto apply;
	    }
	}

apply:	if (!*p)
	    break;
	if (*p != ',')
	    goto getop;
	++p;
    }
    set->cmd = '\0';
#ifdef SETMODE_DEBUG
    (void) printf("Before compress_mode()\n");
    dumpmode(saveset);
#endif
    compress_mode(saveset);
#ifdef SETMODE_DEBUG
    (void) printf("After compress_mode()\n");
    dumpmode(saveset);
#endif
    return saveset;
}
/*@=usereleased@*/
#endif	/* !defined(HAVE_SETMODE) */
#endif	/* !defined(HAVE_GETMODE) || !defined(HAVE_SETMODE) */
#endif	/* __linux__ */

/*==============================================================*/

static void
set(char * t, NODE * ip)
	/*@globals fileSystem, internalState @*/
	/*@modifies t, ip, fileSystem, internalState @*/
{
    int type;
    char *kw, *val = NULL;
    struct group *gr;
    struct passwd *pw;
    mode_t *m;
    int value;
    char *ep;

    for (; (kw = strtok(t, "= \t\n")) != NULL; t = NULL) {
	ip->flags |= type = parsekey(kw, &value);
	if (value && (val = strtok(NULL, " \t\n")) == NULL)
		mtree_error("missing value");
	switch(type) {
	case F_CKSUM:
	    ip->cksum = strtoul(val, &ep, 10);
	    if (*ep != '\0')
		mtree_error("invalid checksum %s", val);
	    /*@switchbreak@*/ break;
	case F_MD5:
	    ip->md5digest = xstrdup(val);
	    /*@switchbreak@*/ break;
	case F_FLAGS:
	    if (!strcmp(val, "none")) {
		ip->st_flags = 0;
		/*@switchbreak@*/ break;
	    }
#if defined(__linux__)
	    ip->st_flags = 0;	/* XXX W2DO? */
#else
	    {	unsigned long fset, fclr;
		if (strtofflags(&val, &fset, &fclr))
		    mtree_error("%s", strerror(errno));
		ip->st_flags = fset;
	    }
#endif
	    /*@switchbreak@*/ break; 
	case F_GID:
	    ip->st_gid = strtoul(val, &ep, 10);
	    if (*ep != '\0')
		mtree_error("invalid gid %s", val);
	    /*@switchbreak@*/ break;
	case F_GNAME:
	    if ((gr = getgrnam(val)) == NULL)
	 	mtree_error("unknown group %s", val);
	    ip->st_gid = gr->gr_gid;
	    /*@switchbreak@*/ break;
	case F_IGN:
	    /* just set flag bit */
	    /*@switchbreak@*/ break;
	case F_MODE:
	    if ((m = setmode(val)) == NULL)
		mtree_error("invalid file mode %s", val);
	    ip->st_mode = getmode(m, 0);
	    free(m);
	    /*@switchbreak@*/ break;
	case F_NLINK:
	    ip->st_nlink = strtoul(val, &ep, 10);
	    if (*ep != '\0')
		mtree_error("invalid link count %s", val);
	    /*@switchbreak@*/ break;
	case F_RMD160:
	    ip->rmd160digest = xstrdup(val);
	    /*@switchbreak@*/ break;
	case F_SHA1:
	    ip->sha1digest = xstrdup(val);
	    /*@switchbreak@*/ break;
	case F_SHA256:
	    ip->sha256digest = xstrdup(val);
	    /*@switchbreak@*/ break;
	case F_SIZE:
/*@-unrecog@*/
	    ip->st_size = strtouq(val, &ep, 10);
/*@=unrecog@*/
	    if (*ep != '\0')
		mtree_error("invalid size %s", val);
	    /*@switchbreak@*/ break;
	case F_SLINK:
	    ip->slink = xmalloc(strlen(val) + 1);
	    if (strunvis(ip->slink, val) == -1) {
		fprintf(stderr, _("%s: filename (%s) encoded incorrectly\n"),
			__progname, val);
		strcpy(ip->slink, val);
	    }
	    /*@switchbreak@*/ break;
	case F_TIME:
/*@-type@*/
	    ip->st_mtimespec.tv_sec = strtoul(val, &ep, 10);
/*@=type@*/
	    if (*ep != '.')
		mtree_error("invalid time %s", val);
	    val = ep + 1;
/*@-type@*/
	    ip->st_mtimespec.tv_nsec = strtoul(val, &ep, 10);
/*@=type@*/
	    if (*ep != '\0')
		mtree_error("invalid time %s", val);
	    /*@switchbreak@*/ break;
	case F_TYPE:
	    switch(*val) {
	    case 'b':
		if (!strcmp(val, "block"))
		    ip->type = F_BLOCK;
		/*@innerbreak@*/ break;
	    case 'c':
		if (!strcmp(val, "char"))
		    ip->type = F_CHAR;
		/*@innerbreak@*/ break;
	    case 'd':
		if (!strcmp(val, "dir"))
		    ip->type = F_DIR;
		/*@innerbreak@*/ break;
	    case 'f':
		if (!strcmp(val, "file"))
		    ip->type = F_FILE;
		if (!strcmp(val, "fifo"))
		    ip->type = F_FIFO;
		/*@innerbreak@*/ break;
	    case 'l':
		if (!strcmp(val, "link"))
		    ip->type = F_LINK;
		/*@innerbreak@*/ break;
	    case 's':
		if (!strcmp(val, "socket"))
		    ip->type = F_SOCK;
		/*@innerbreak@*/ break;
	    default:
		mtree_error("unknown file type %s", val);
	    }
	    /*@switchbreak@*/ break;
	case F_UID:
	    ip->st_uid = strtoul(val, &ep, 10);
	    if (*ep != '\0')
		mtree_error("invalid uid %s", val);
	    /*@switchbreak@*/ break;
	case F_UNAME:
	    if ((pw = getpwnam(val)) == NULL)
	 	mtree_error("unknown user %s", val);
	    ip->st_uid = pw->pw_uid;
	    /*@switchbreak@*/ break;
	}
    }
}

static void
unset(char * t, NODE * ip)
	/*@globals fileSystem, internalState @*/
	/*@modifies t, ip, fileSystem, internalState @*/
{
    char *p;

    while ((p = strtok(t, "\n\t ")) != NULL)
	ip->flags &= ~parsekey(p, NULL);
}

NODE *
mtreeSpec(rpmfts fts)
{
    NODE *centry = NULL;
    NODE *last = NULL;
    char *p;
    NODE ginfo;
    NODE *myroot = NULL;
    int c_cur, c_next;
    char buf[2048];

    memset(&ginfo, 0, sizeof(ginfo));
    c_cur = c_next = 0;
    for (fts->lineno = 1; fgets(buf, (int)sizeof(buf), stdin) != NULL;
	    ++fts->lineno, c_cur = c_next, c_next = 0)
    {
	/* Skip empty lines. */
	if (buf[0] == '\n')
	    continue;

	/* Find end of line. */
	if ((p = strchr(buf, '\n')) == NULL)
	    mtree_error("line %d too long", fts->lineno);

	/* See if next line is continuation line. */
	if (p[-1] == '\\') {
	    --p;
	    c_next = 1;
	}

	/* Null-terminate the line. */
	*p = '\0';

	/* Skip leading whitespace. */
	for (p = buf; *p && isspace(*p); ++p);

	/* If nothing but whitespace or comment char, continue. */
	if (!*p || *p == '#')
	    continue;

#ifdef DEBUG
	(void)fprintf(stderr, "line %d: {%s}\n", fts->lineno, p);
#endif
	if (c_cur) {
	    set(p, centry);
	    continue;
	}
			
	/* Grab file name, "$", "set", or "unset". */
	if ((p = strtok(p, "\n\t ")) == NULL)
	    mtree_error("missing field");

	if (p[0] == '/')
	    switch(p[1]) {
	    case 's':
		if (strcmp(p + 1, "set"))
		    /*@switchbreak@*/ break;
		set(NULL, &ginfo);
		continue;
	    case 'u':
		if (strcmp(p + 1, "unset"))
		    /*@switchbreak@*/ break;
		unset(NULL, &ginfo);
		continue;
	    }

	if (strchr(p, '/') != NULL)
	    mtree_error("slash character in file name");

	if (!strcmp(p, "..")) {
	    /* Don't go up, if haven't gone down. */
	    if (myroot == NULL)
		    goto noparent;
	    if (last->type != F_DIR || last->flags & F_DONE) {
		if (last == myroot)
		    goto noparent;
		last = last->parent;
	    }
	    last->flags |= F_DONE;
	    continue;

noparent:    mtree_error("no parent node");
	}

	if ((centry = calloc(1, sizeof(*centry) + strlen(p))) == NULL)
	    mtree_error("%s", strerror(errno));
	*centry = ginfo;
#define	MAGIC	"?*["
	if (strpbrk(p, MAGIC) != NULL)
	    centry->flags |= F_MAGIC;
	if (strunvis(centry->name, p) == -1) {
	    fprintf(stderr, _("%s: filename (%s) encoded incorrectly\n"),
			__progname, p);
	    strcpy(centry->name, p);
	}
	set(NULL, centry);

	if (fts->root == NULL) {
	    last = myroot = centry;
	    myroot->parent = myroot;
	} else if (last->type == F_DIR && !(last->flags & F_DONE)) {
	    centry->parent = last;
	    last = last->child = centry;
	} else {
	    centry->parent = last->parent;
	    centry->prev = last;
	    last = last->next = centry;
	}
    }
    return myroot;
}

/*==============================================================*/

#define FILE_BUFFER			0x1000
typedef struct rpmdc_s * rpmdc;

#define _DFB(n) ((1 << (n)) | 0x40000000)
#define F_ISSET(_dc, _FLAG) ((_dc)->flags & ((RPMDC_FLAGS_##_FLAG) & ~0x40000000))

enum dcFlags_e {
    RPMDC_FLAGS_BINARY	= _DFB( 0),	/*!< -b,--binary ... */
    RPMDC_FLAGS_WARN	= _DFB( 1),	/*!< -w,--warn ... */
    RPMDC_FLAGS_STATUS	= _DFB( 2)	/*!<    --status ... */
};

struct rpmdc_s {
    enum dcFlags_e flags;
    uint32_t algo;		/*!< default digest algorithm. */
/*@null@*/
    const char * digest;
    size_t digestlen;
/*@observer@*/
    const char * fn;
    FD_t fd;
/*@null@*/
    ARGV_t manifests;		/*!< array of file manifests to verify. */
/*@null@*/
    ARGI_t algos;		/*!< array of file digest algorithms. */
/*@null@*/
    ARGV_t digests;		/*!< array of file digests. */
/*@null@*/
    ARGV_t paths;		/*!< array of file paths. */
    unsigned char buf[BUFSIZ];
    ssize_t nb;
    int ix;
    int nfails;
};

/*@unchecked@*/
static struct rpmdc_s __dc;

/*@unchecked@*/
static rpmdc _dc = &__dc;

/*@unchecked@*/
static struct rpmop_s dc_totalops;

/*@unchecked@*/
static struct rpmop_s dc_readops;

/*@unchecked@*/
static struct rpmop_s dc_digestops;

static int rpmdcPrintFile(rpmdc dc, uint32_t algo, const char * algoName)
	/*@globals fileSystem, internalState @*/
	/*@modifies dc, fileSystem, internalState @*/
{
    static int asAscii = 1;
    int rc = 0;

    fdFiniDigest(dc->fd, algo, &dc->digest, &dc->digestlen, asAscii);
assert(dc->digest != NULL);
    if (dc->manifests != NULL) {
	const char * msg = "OK";
	if ((rc = strcmp(dc->digest, dc->digests[dc->ix])) != 0) {
	    msg = "FAILED";
	    dc->nfails++;
	}
	if (rc || !F_ISSET(dc, STATUS))
	    fprintf(stdout, "%s: %s\n", dc->fn, msg);
    } else {
	if (!F_ISSET(dc, STATUS)) {
	    if (algoName != NULL) fprintf(stdout, "%s:", algoName);
	    fprintf(stdout, "%s %c%s\n", dc->digest,
		(F_ISSET(dc, BINARY) ? '*' : ' '), dc->fn);
	    (void) fflush(stdout);
	}
	dc->digest = _free(dc->digest);
    }
    return rc;
}

static int rpmdcFiniFile(rpmdc dc)
	/*@globals fileSystem, internalState @*/
	/*@modifies dc, fileSystem, internalState @*/
{
    uint32_t algo= (dc->manifests != NULL ? dc->algos->vals[dc->ix] : dc->algo);
    int rc = 0;
    int xx;

    switch (algo) {
    default:
	xx = rpmdcPrintFile(dc, algo, NULL);
	if (xx) rc = xx;
	break;
    case 256:		/* --all digests requested. */
      {	struct poptOption * opt = rpmioDigestPoptTable;
	for (; (opt->longName || opt->shortName || opt->arg) ; opt++) {
	    if ((opt->argInfo & POPT_ARG_MASK) != POPT_ARG_VAL)
		continue;
	    if (opt->arg != (void *)&rpmioDigestHashAlgo)
		continue;
	    dc->algo = opt->val;
	    if (!(dc->algo > 0 && dc->algo < 256))
		continue;
	    xx = rpmdcPrintFile(dc, dc->algo, opt->longName);
	    if (xx) rc = xx;
	}
      }	break;
    }
    (void) rpmswAdd(&dc_readops, fdstat_op(dc->fd, FDSTAT_READ));
    (void) rpmswAdd(&dc_digestops, fdstat_op(dc->fd, FDSTAT_DIGEST));
    (void) Fclose(dc->fd);
    dc->fd = NULL;
    return rc;
}

static int rpmdcCalcFile(rpmdc dc)
	/*@globals fileSystem @*/
	/*@modifies dc, fileSystem @*/
{
    int rc = 0;

    do {
	dc->nb = Fread(dc->buf, sizeof(dc->buf[0]), sizeof(dc->buf), dc->fd);
	if (Ferror(dc->fd)) {
	    rc = 2;
	    break;
	}
    } while (dc->nb > 0);

    return rc;
}

static int rpmdcInitFile(rpmdc dc)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies dc, fileSystem, internalState @*/
{
    uint32_t algo= (dc->manifests != NULL ? dc->algos->vals[dc->ix] : dc->algo);
    int rc = 0;

    /* XXX Stat(2) to insure files only? */
    dc->fd = Fopen(dc->fn, "r.ufdio");
    if (dc->fd == NULL || Ferror(dc->fd)) {
	fprintf(stderr, _("open of %s failed: %s\n"), dc->fn, Fstrerror(dc->fd));
	if (dc->fd != NULL) (void) Fclose(dc->fd);
	dc->fd = NULL;
	rc = 2;
	goto exit;
    }

    switch (dc->algo) {
    default:
	/* XXX TODO: instantiate verify digests for all identical paths. */
	fdInitDigest(dc->fd, algo, 0);
	break;
    case 256:		/* --all digests requested. */
      {	struct poptOption * opt = rpmioDigestPoptTable;
	for (; (opt->longName || opt->shortName || opt->arg) ; opt++) {
	    if ((opt->argInfo & POPT_ARG_MASK) != POPT_ARG_VAL)
		continue;
	    if (opt->longName == NULL)
		continue;
	    if (!(opt->val > 0 && opt->val < 256))
		continue;
	    algo = opt->val;
	    fdInitDigest(dc->fd, algo, 0);
	}
      }	break;
    }

exit:
    return rc;
}

/*==============================================================*/

/*@observer@*/
static const char *
ftype(unsigned type)
	/*@*/
{
    switch(type) {
    case F_BLOCK: return "block";
    case F_CHAR:  return "char";
    case F_DIR:   return "dir";
    case F_FIFO:  return "fifo";
    case F_FILE:  return "file";
    case F_LINK:  return "link";
    case F_SOCK:  return "socket";
    default:      return "unknown";
    }
    /*@notreached@*/
}

/*@observer@*/
static const char *
inotype(mode_t mode)
	/*@*/
{
    switch(mode & S_IFMT) {
    case S_IFBLK:  return "block";
    case S_IFCHR:  return "char";
    case S_IFDIR:  return "dir";
    case S_IFIFO:  return "fifo";
    case S_IFREG:  return "file";
    case S_IFLNK:  return "link";
/*@-unrecog@*/
    case S_IFSOCK: return "socket";
/*@=unrecog@*/
    default:       return "unknown";
    }
    /*@notreached@*/
}

/*@observer@*/
static const char *
rlink(char * name)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/

{
    static char lbuf[MAXPATHLEN];
    int len;

    if ((len = Readlink(name, lbuf, sizeof(lbuf)-1)) == -1)
	mtree_error("%s: %s", name, strerror(errno));
    lbuf[len] = '\0';
    return lbuf;
}

#define	COMPAREINDENTNAMELEN	8
#define	LABEL 			\
    if (!label++) {		 \
	len = printf("%s: ", RP(p)); \
	if (len > COMPAREINDENTNAMELEN) { \
	    tab = "\t"; 	\
	    (void) printf("\n"); \
	} else { 		\
	    tab = ""; 		\
	    (void) printf("%*s", COMPAREINDENTNAMELEN - (int)len, ""); \
	} 			\
    }

#define REPLACE_COMMA(x)	\
    do {			\
	char *l;		\
	for (l = x; *l; l++) {	\
	    if (*l == ',')	\
		*l = ' ';	\
	}			\
    } while (0)			\

static int
compare(char * name, NODE * s, FTSENT * p)
	/*@globals _dc, errno, h_errno, fileSystem, internalState @*/
	/*@modifies _dc, errno, fileSystem, internalState @*/

{
    static int asAscii = 1;
    uint32_t len, val;
    int label = 0;
    const char *cp;
    const char *tab = "";
    int xx;

    switch(s->type) {
    case F_BLOCK:
	if (!S_ISBLK(p->fts_statp->st_mode))
	    goto typeerr;
	break;
    case F_CHAR:
	if (!S_ISCHR(p->fts_statp->st_mode))
	    goto typeerr;
	break;
    case F_DIR:
	if (!S_ISDIR(p->fts_statp->st_mode))
	    goto typeerr;
	break;
    case F_FIFO:
	if (!S_ISFIFO(p->fts_statp->st_mode))
	    goto typeerr;
	break;
    case F_FILE:
	if (!S_ISREG(p->fts_statp->st_mode))
	    goto typeerr;
	break;
    case F_LINK:
	if (!S_ISLNK(p->fts_statp->st_mode))
	    goto typeerr;
	break;
    case F_SOCK:
/*@-unrecog@*/
	if (!S_ISSOCK(p->fts_statp->st_mode)) {
typeerr:    LABEL;
	    (void) printf("\ttype (%s, %s)\n",
		    ftype((unsigned)s->type), inotype(p->fts_statp->st_mode));
	}
/*@=unrecog@*/
	break;
    }
    /* Set the uid/gid first, then set the mode. */
    if (s->flags & (F_UID | F_UNAME) && s->st_uid != p->fts_statp->st_uid) {
	LABEL;
	(void) printf("%suser (%u, %u", tab,
		(unsigned)s->st_uid, (unsigned)p->fts_statp->st_uid);
	if (MF_ISSET(UPDATE)) {
	    if (Chown(p->fts_accpath, s->st_uid, -1))
		(void) printf(", not modified: %s)\n", strerror(errno));
	    else
		(void) printf(", modified)\n");
	} else
	    (void) printf(")\n");
	tab = "\t";
    }
    if (s->flags & (F_GID | F_GNAME) && s->st_gid != p->fts_statp->st_gid) {
	LABEL;
	(void) printf("%sgid (%u, %u", tab,
		(unsigned)s->st_gid, (unsigned)p->fts_statp->st_gid);
	if (MF_ISSET(UPDATE)) {
	    if (Chown(p->fts_accpath, -1, s->st_gid))
		(void) printf(", not modified: %s)\n", strerror(errno));
	    else
		(void) printf(", modified)\n");
	} else
	    (void) printf(")\n");
	tab = "\t";
    }
    if (s->flags & F_MODE && s->st_mode != (p->fts_statp->st_mode & MBITS)) {
	if (MF_ISSET(LOOSE)) {
	    mode_t tmode = s->st_mode;
	    mode_t mode = (p->fts_statp->st_mode & MBITS);

	    /*
	    * if none of the suid/sgid/etc bits are set,
	    * then if the mode is a subset of the target,
	    * skip.
	    */
	    if (!((tmode & ~(S_IRWXU|S_IRWXG|S_IRWXO))
	     ||    (mode & ~(S_IRWXU|S_IRWXG|S_IRWXO))))
		if ((mode | tmode) == tmode)
		    goto skip;
	}
	LABEL;
	(void) printf("%spermissions (%#o, %#o", tab,
		(unsigned)s->st_mode, (unsigned)(p->fts_statp->st_mode & MBITS));
	if (MF_ISSET(UPDATE)) {
	    if (Chmod(p->fts_accpath, s->st_mode))
		(void) printf(", not modified: %s)\n", strerror(errno));
	    else
		(void) printf(", modified)\n");
	} else
	    (void) printf(")\n");
	tab = "\t";
 	skip:
	    ;
    }
    if (s->flags & F_NLINK && s->type != F_DIR &&
        s->st_nlink != p->fts_statp->st_nlink)
    {
	LABEL;
	(void) printf("%slink count (%u, %u)\n", tab,
		(unsigned)s->st_nlink, (unsigned)p->fts_statp->st_nlink);
	tab = "\t";
    }
    if (s->flags & F_SIZE && s->st_size != p->fts_statp->st_size) {
	LABEL;
	(void) printf("%ssize (%lu, %lu)\n", tab,
		(unsigned long)s->st_size, (unsigned long)p->fts_statp->st_size);
	tab = "\t";
    }
    /*
     * XXX
     * Since utimes(2) only takes a timeval, there's no point in
     * comparing the low bits of the timespec nanosecond field.  This
     * will only result in mismatches that we can never fix.
     *
     * Doesn't display microsecond differences.
     */
    if (s->flags & F_TIME) {
	struct timeval tv[2];

/*@-noeffectuncon -unrecog@*/
	TIMESPEC_TO_TIMEVAL(&tv[0], &s->st_mtimespec);
#if defined(__linux__)
	tv[1].tv_sec = (long)p->fts_statp->st_mtime;
	tv[1].tv_usec = 0;
#else
	TIMESPEC_TO_TIMEVAL(&tv[1], &p->fts_statp->st_mtimespec);
#endif
/*@=noeffectuncon =unrecog@*/
	if (tv[0].tv_sec != tv[1].tv_sec || tv[0].tv_usec != tv[1].tv_usec) {
	    LABEL;
	    (void) printf("%smodification time (%.24s, ", tab,
			ctime(&tv[0].tv_sec));
	    (void) printf("%.24s", ctime(&tv[1].tv_sec));
	    if (MF_ISSET(TOUCH)) {
		tv[1] = tv[0];
		if (Utimes(p->fts_accpath, tv))
		    (void) printf(", not modified: %s)\n", strerror(errno));
		else  
		    (void) printf(", modified)\n");  
	    } else
		(void) printf(")\n");
	    tab = "\t";   
	}
    }
    if (s->flags & F_CKSUM) {
	FD_t fd = Fopen(p->fts_accpath, "r.ufdio");
	if (fd == NULL || Ferror(fd)) {
	    LABEL;
	    (void) printf("%scksum: %s: %s\n", tab,
			p->fts_accpath, Fstrerror(fd));
	} else if (crc(fd, &val, &len)) {
	    LABEL;
	    (void) printf("%scksum: %s: %s\n", tab,
			p->fts_accpath, Fstrerror(fd));
	} else {
	    if (s->cksum != val) {
		LABEL;
		(void) printf("%scksum (%u, %u)\n", tab,
				(unsigned) s->cksum, (unsigned) val);
	    }
	}
	if (fd != NULL) (void) Fclose(fd);
	tab = "\t";
    }
/*@-mods@*/	/* dc->fn might modify p->fts_accpath */
    if (s->flags & F_MD5) {
	rpmdc dc = _dc;
	dc->fn = p->fts_accpath;
	dc->algo = PGPHASHALGO_MD5;
	xx = rpmdcInitFile(dc);
	xx = rpmdcCalcFile(dc);
	/* XXX hotwire around rpmdcFini() for now. */
	fdFiniDigest(dc->fd, dc->algo, &dc->digest, &dc->digestlen, asAscii);
	if (dc->digest == NULL) {
	    LABEL;
	    printf("%sMD5File: %s: %s\n", tab, p->fts_accpath, strerror(errno));
	    tab = "\t";
	} else if (strcmp(dc->digest, s->md5digest)) {
	    LABEL;
	    printf("%sMD5 (%s, %s)\n", tab, s->md5digest, dc->digest);
	    tab = "\t";
	}
	if (dc->fd != NULL) {
	    (void) rpmswAdd(&dc_readops, fdstat_op(dc->fd, FDSTAT_READ));
	    (void) rpmswAdd(&dc_digestops, fdstat_op(dc->fd, FDSTAT_DIGEST));
	    (void) Fclose(dc->fd);
	    dc->fd = NULL;
	    dc->digest = _free(dc->digest);
	    dc->digestlen = 0;
	}
	dc->algo = 0;
	dc->fn = NULL;
    }
    if (s->flags & F_RMD160) {
	rpmdc dc = _dc;
	dc->fn = p->fts_accpath;
	dc->algo = PGPHASHALGO_RIPEMD160;
	xx = rpmdcInitFile(dc);
	xx = rpmdcCalcFile(dc);
	/* XXX hotwire around rpmdcFini() for now. */
	fdFiniDigest(dc->fd, dc->algo, &dc->digest, &dc->digestlen, asAscii);
	if (dc->digest == NULL) {
	    LABEL;
	    printf("%sRMD160File: %s: %s\n", tab, p->fts_accpath, strerror(errno));
	    tab = "\t";
	} else if (strcmp(dc->digest, s->rmd160digest)) {
	    LABEL;
	    printf("%sRMD160 (%s, %s)\n", tab, s->rmd160digest, dc->digest);
	    tab = "\t";
	}
	if (dc->fd != NULL) {
	    (void) rpmswAdd(&dc_readops, fdstat_op(dc->fd, FDSTAT_READ));
	    (void) rpmswAdd(&dc_digestops, fdstat_op(dc->fd, FDSTAT_DIGEST));
	    (void) Fclose(dc->fd);
	    dc->fd = NULL;
	    dc->digest = _free(dc->digest);
	    dc->digestlen = 0;
	}
	dc->algo = 0;
	dc->fn = NULL;
    }
    if (s->flags & F_SHA1) {
	rpmdc dc = _dc;
	dc->fn = p->fts_accpath;
	dc->algo = PGPHASHALGO_SHA1;
	xx = rpmdcInitFile(dc);
	xx = rpmdcCalcFile(dc);
	/* XXX hotwire around rpmdcFini() for now. */
	fdFiniDigest(dc->fd, dc->algo, &dc->digest, &dc->digestlen, asAscii);
	if (dc->digest == NULL) {
	    LABEL;
	    printf("%sSHA1File: %s: %s\n", tab, p->fts_accpath, strerror(errno));
	    tab = "\t";
	} else if (strcmp(dc->digest, s->sha1digest)) {
	    LABEL;
	    printf("%sSHA1 (%s, %s)\n", tab, s->sha1digest, dc->digest);
	    tab = "\t";
	}
	if (dc->fd != NULL) {
	    (void) rpmswAdd(&dc_readops, fdstat_op(dc->fd, FDSTAT_READ));
	    (void) rpmswAdd(&dc_digestops, fdstat_op(dc->fd, FDSTAT_DIGEST));
	    (void) Fclose(dc->fd);
	    dc->fd = NULL;
	    dc->digest = _free(dc->digest);
	    dc->digestlen = 0;
	}
	dc->algo = 0;
	dc->fn = NULL;
    }
    if (s->flags & F_SHA256) {
	rpmdc dc = _dc;
	dc->fn = p->fts_accpath;
	dc->algo = PGPHASHALGO_SHA256;
	xx = rpmdcInitFile(dc);
	xx = rpmdcCalcFile(dc);
	/* XXX hotwire around rpmdcFini() for now. */
	fdFiniDigest(dc->fd, dc->algo, &dc->digest, &dc->digestlen, asAscii);
	if (dc->digest == NULL) {
	    LABEL;
	    printf("%sSHA256File: %s: %s\n", tab, p->fts_accpath, strerror(errno));
	    tab = "\t";
	} else if (strcmp(dc->digest, s->sha1digest)) {
	    LABEL;
	    printf("%sSHA256 (%s, %s)\n", tab, s->sha1digest, dc->digest);
	    tab = "\t";
	}
	if (dc->fd != NULL) {
	    (void) rpmswAdd(&dc_readops, fdstat_op(dc->fd, FDSTAT_READ));
	    (void) rpmswAdd(&dc_digestops, fdstat_op(dc->fd, FDSTAT_DIGEST));
	    (void) Fclose(dc->fd);
	    dc->fd = NULL;
	    dc->digest = _free(dc->digest);
	    dc->digestlen = 0;
	}
	dc->algo = 0;
	dc->fn = NULL;
    }
/*@=mods@*/
    if (s->flags & F_SLINK && strcmp(cp = rlink(name), s->slink)) {
	LABEL;
	(void) printf("%slink ref (%s, %s)\n", tab, cp, s->slink);
    }
#if !defined(__linux__)
    if (s->flags & F_FLAGS && s->st_flags != p->fts_statp->st_flags) {
	char *db_flags = NULL;
	char *cur_flags = NULL;

	if ((db_flags = fflagstostr(s->st_flags)) == NULL ||
	    (cur_flags = fflagstostr(p->fts_statp->st_flags)) == NULL)
	{
	    LABEL;
	    (void) printf("%sflags: %s %s\n", tab, p->fts_accpath, strerror(errno));
	    tab = "\t";
	    if (db_flags != NULL)
		free(db_flags);
	    if (cur_flags != NULL)
		free(cur_flags);
	} else {
	    LABEL;
	    REPLACE_COMMA(db_flags);
	    REPLACE_COMMA(cur_flags);
	    printf("%sflags (%s, %s", tab,
			(*db_flags == '\0') ?  "-" : db_flags,
			(*cur_flags == '\0') ?  "-" : cur_flags);
	    tab = "\t";
	    if (MF_ISSET(UPDATE)) {
		if (chflags(p->fts_accpath, s->st_flags))
		    (void) printf(", not modified: %s)\n", strerror(errno));
		else	
		    (void) printf(", modified)\n");
	    } else
		(void) printf(")\n");
	    tab = "\t"; 

	    free(db_flags);
	    free(cur_flags);
	}
    }
#endif
    return label;
}

/*==============================================================*/

#define	CWALKINDENTNAMELEN	15
#define	MAXLINELEN	80

static int
dsort(const FTSENT ** a, const FTSENT ** b)
	/*@*/
{
    if (S_ISDIR((*a)->fts_statp->st_mode)) {
	if (!S_ISDIR((*b)->fts_statp->st_mode))
	    return 1;
    } else if (S_ISDIR((*b)->fts_statp->st_mode))
	return -1;
    return strcmp((*a)->fts_name, (*b)->fts_name);
}

static void
output(int indent, int * offset, const char * fmt, ...)
	/*@globals fileSystem @*/
	/*@modifies *offset, fileSystem @*/
{
    char buf[1024];
    va_list ap;

    va_start(ap, fmt);
    (void) vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (*offset + strlen(buf) > MAXLINELEN - 3) {
	(void)printf(" \\\n%*s", CWALKINDENTNAMELEN + indent, "");
	*offset = CWALKINDENTNAMELEN + indent;
    }
    *offset += printf(" %s", buf) + 1;
}

#define	MAXGID	5000
#define	MAXUID	5000
#define	MAXMODE	MBITS + 1
#define	MAXFLAGS 256
#define MAXS 16

static int
mtreeVisitD(rpmfts fts)
	/*@globals fileSystem, internalState @*/
	/*@modifies fts, fileSystem, internalState @*/
{
    unsigned keys = fts->keys;
    FTS * t = fts->t;
    FTSENT * parent = fts->p;
    uid_t * puid = &fts->uid;
    gid_t * pgid = &fts->gid;
    mode_t * pmode = &fts->mode;
    unsigned long *pflags = &fts->fflags;
    FTSENT *p;
    gid_t sgid;
    uid_t suid;
    mode_t smode;
#ifdef	NOTYET
    unsigned long sflags;
#endif
    struct group *gr;
    struct passwd *pw;
    gid_t savegid = *pgid;
    uid_t saveuid = *puid;
    mode_t savemode = *pmode;
    unsigned long saveflags = *pflags;
    gid_t maxgid = 0;
    uid_t maxuid = 0;
    mode_t maxmode = 0;
#ifdef	NOTYET
    unsigned long maxflags = 0;
#endif
    gid_t g[MAXGID];
    uid_t u[MAXUID];
    mode_t m[MAXMODE];
    unsigned long f[MAXFLAGS];
    static int first = 1;

/*@-unrecog@*/
    if ((p = Fts_children(t, 0)) == NULL) {
	if (errno)
	    mtree_error("%s: %s", RP(parent), strerror(errno));
	return 1;
    }
/*@=unrecog@*/

    memset(g, 0, sizeof(g));
    memset(u, 0, sizeof(u));
    memset(m, 0, sizeof(m));
    memset(f, 0, sizeof(f));

    for (; p != NULL; p = p->fts_link) {
	if (!MF_ISSET(DIRSONLY) || (MF_ISSET(DIRSONLY) && S_ISDIR(p->fts_statp->st_mode))) {
	    smode = p->fts_statp->st_mode & MBITS;
	    if (smode < MAXMODE && ++m[smode] > maxmode) {
		savemode = smode;
		maxmode = m[smode];
	    }
	    sgid = p->fts_statp->st_gid;
	    if (sgid < MAXGID && ++g[sgid] > maxgid) {
		savegid = sgid;
		maxgid = g[sgid];
	    }
	    suid = p->fts_statp->st_uid;
	    if (suid < MAXUID && ++u[suid] > maxuid) {
		saveuid = suid;
		maxuid = u[suid];
	    }
#ifdef	NOTYET
	    /*
 	    * XXX
 	    * note that the below will break when file flags
	    * are extended beyond the first 4 bytes of each
	    * half word of the flags
	    */
#define FLAGS2IDX(f) ((f & 0xf) | ((f >> 12) & 0xf0))
	    sflags = p->fts_statp->st_flags;
	    if (FLAGS2IDX(sflags) < MAXFLAGS &&
		++f[FLAGS2IDX(sflags)] > maxflags)
	    {
		saveflags = sflags;
		maxflags = f[FLAGS2IDX(sflags)];
 	    }
#endif
	}
    }
    /*
     * If the /set record is the same as the last one we do not need to output
     * a new one.  So first we check to see if anything changed.  Note that we
     * always output a /set record for the first directory.
     */
    if ((((keys & F_UNAME) | (keys & F_UID)) && (*puid != saveuid))
     || (((keys & F_GNAME) | (keys & F_GID)) && (*pgid != savegid))
     ||  ((keys & F_MODE) && (*pmode != savemode))
     ||  ((keys & F_FLAGS) && (*pflags != saveflags))
     || first)
    {
	first = 0;
	if (MF_ISSET(DIRSONLY))
	    (void) printf("/set type=dir");
	else
	    (void) printf("/set type=file");
	if (keys & F_UNAME) {
	    if ((pw = getpwuid(saveuid)) != NULL)
		(void) printf(" uname=%s", pw->pw_name);
	    else if (MF_ISSET(WARN))
		warnx( "Could not get uname for uid=%u", saveuid);
	    else
		mtree_error("could not get uname for uid=%u", saveuid);
	}
	if (keys & F_UID)
	    (void) printf(" uid=%u", (unsigned)saveuid);
	if (keys & F_GNAME) {
	    if ((gr = getgrgid(savegid)) != NULL)
		(void) printf(" gname=%s", gr->gr_name);
	    else if (MF_ISSET(WARN))
		warnx("Could not get gname for gid=%u", savegid);
	    else
		mtree_error("could not get gname for gid=%u", savegid);
	}
	if (keys & F_GID)
	    (void) printf(" gid=%u", (unsigned)savegid);
	if (keys & F_MODE)
	    (void) printf(" mode=%#o", (unsigned)savemode);
	if (keys & F_NLINK)
	    (void) printf(" nlink=1");
	if (keys & F_FLAGS) {
#if defined(__linux__) || !defined(NOTYET)
	    (void) printf(" flags=none");
#else
	    const char * fflags = flags_to_string(saveflags);
	    (void) printf(" flags=%s", fflags);
	    fflags = _free(fflags);
#endif
	}
	(void) printf("\n");
	*puid = saveuid;
	*pgid = savegid;
	*pmode = savemode;
	*pflags = saveflags;
    }
    return (0);
}

static void
mtreeVisitF(rpmfts fts)
	/*@globals _dc, errno, h_errno, fileSystem, internalState @*/
	/*@modifies _dc, errno, fileSystem, internalState @*/
{
    static int asAscii = 1;
    unsigned keys = fts->keys;
    int indent = (MF_ISSET(INDENT) ? fts->p->fts_level * 4 : 0);
    struct group *gr;
    struct passwd *pw;
    uint32_t len, val;
    int offset;
    char * escaped_name = xmalloc(fts->p->fts_namelen * 4  +  1);
    int xx;

    (void) strvis(escaped_name, fts->p->fts_name, VIS_WHITE | VIS_OCTAL);

    if (MF_ISSET(INDENT) || S_ISDIR(fts->p->fts_statp->st_mode))
	offset = printf("%*s%s", indent, "", escaped_name);
    else
	offset = printf("%*s    %s", indent, "", escaped_name);

    escaped_name = _free(escaped_name);

    if (offset > (CWALKINDENTNAMELEN + indent))
	offset = MAXLINELEN;
    else
	offset += printf("%*s", (CWALKINDENTNAMELEN + indent) - offset, "");

    if (!S_ISREG(fts->p->fts_statp->st_mode) && !MF_ISSET(DIRSONLY))
	output(indent, &offset, "type=%s", inotype(fts->p->fts_statp->st_mode));
    if (fts->p->fts_statp->st_uid != fts->uid) {
	if (keys & F_UNAME) {
	    if ((pw = getpwuid(fts->p->fts_statp->st_uid)) != NULL)
		output(indent, &offset, "uname=%s", pw->pw_name);
	    else if (MF_ISSET(WARN))
		warnx("Could not get uname for uid=%u", fts->p->fts_statp->st_uid);
	    else
		mtree_error("could not get uname for uid=%u", fts->p->fts_statp->st_uid);
	}
	if (keys & F_UID)
	    output(indent, &offset, "uid=%u", fts->p->fts_statp->st_uid);
    }
    if (fts->p->fts_statp->st_gid != fts->gid) {
	if (keys & F_GNAME) {
	    if ((gr = getgrgid(fts->p->fts_statp->st_gid)) != NULL)
		output(indent, &offset, "gname=%s", gr->gr_name);
	    else if (MF_ISSET(WARN))
		warnx("Could not get gname for gid=%u", fts->p->fts_statp->st_gid);
	    else
		mtree_error("could not get gname for gid=%u", fts->p->fts_statp->st_gid);
	}
	if (keys & F_GID)
	   output(indent, &offset, "gid=%u", fts->p->fts_statp->st_gid);
    }
    if (keys & F_MODE && (fts->p->fts_statp->st_mode & MBITS) != fts->mode)
	output(indent, &offset, "mode=%#o", fts->p->fts_statp->st_mode & MBITS);
    if (keys & F_NLINK && fts->p->fts_statp->st_nlink != 1)
	output(indent, &offset, "nlink=%u", fts->p->fts_statp->st_nlink);
    if (keys & F_SIZE && S_ISREG(fts->p->fts_statp->st_mode))
	output(indent, &offset, "size=%lu", (unsigned long)fts->p->fts_statp->st_size);
    if (keys & F_TIME)
	output(indent, &offset, "time=%ld.%ld",
#if defined(__linux__)
	    fts->p->fts_statp->st_mtime, 0L
#else
	    fts->p->fts_statp->st_mtimespec.tv_sec,
	    fts->p->fts_statp->st_mtimespec.tv_nsec
#endif
	    );
    if (keys & F_CKSUM && S_ISREG(fts->p->fts_statp->st_mode)) {
	FD_t fd = Fopen(fts->p->fts_accpath, "r.ufdio");
	if (fd == NULL || Ferror(fd) || crc(fd, &val, &len))
	    mtree_error("%s: %s", fts->p->fts_accpath, Fstrerror(fd));
	if (fd != NULL) (void) Fclose(fd);
	output(indent, &offset, "cksum=%lu", (unsigned long)val);
    }
/*@-mods@*/	/* dc->fn might modify fts->p->fts_accpath */
    if (keys & F_MD5 && S_ISREG(fts->p->fts_statp->st_mode)) {
	rpmdc dc = _dc;
	dc->fn = fts->p->fts_accpath;
	dc->algo = PGPHASHALGO_MD5;
	xx = rpmdcInitFile(dc);
	xx = rpmdcCalcFile(dc);
	/* XXX hotwire around rpmdcFini() for now. */
	fdFiniDigest(dc->fd, dc->algo, &dc->digest, &dc->digestlen, asAscii);
	if (dc->digest == NULL)
	    mtree_error("%s: %s", fts->p->fts_accpath, strerror(errno));
	else
	    output(indent, &offset, "md5digest=%s", dc->digest);
	if (dc->fd != NULL) {
	    (void) rpmswAdd(&dc_readops, fdstat_op(dc->fd, FDSTAT_READ));
	    (void) rpmswAdd(&dc_digestops, fdstat_op(dc->fd, FDSTAT_DIGEST));
	    (void) Fclose(dc->fd);
	    dc->fd = NULL;
	    dc->digest = _free(dc->digest);
	    dc->digestlen = 0;
	}
	dc->algo = 0;
	dc->fn = NULL;
    }
    if (keys & F_RMD160 && S_ISREG(fts->p->fts_statp->st_mode)) {
	rpmdc dc = _dc;
	dc->fn = fts->p->fts_accpath;
	dc->algo = PGPHASHALGO_RIPEMD160;
	xx = rpmdcInitFile(dc);
	xx = rpmdcCalcFile(dc);
	/* XXX hotwire around rpmdcFini() for now. */
	fdFiniDigest(dc->fd, dc->algo, &dc->digest, &dc->digestlen, asAscii);
	if (dc->digest == NULL)
	    mtree_error("%s: %s", fts->p->fts_accpath, strerror(errno));
	else
	    output(indent, &offset, "rmd160digest=%s", dc->digest);
	if (dc->fd != NULL) {
	    (void) rpmswAdd(&dc_readops, fdstat_op(dc->fd, FDSTAT_READ));
	    (void) rpmswAdd(&dc_digestops, fdstat_op(dc->fd, FDSTAT_DIGEST));
	    (void) Fclose(dc->fd);
	    dc->fd = NULL;
	    dc->digest = _free(dc->digest);
	    dc->digestlen = 0;
	}
	dc->algo = 0;
	dc->fn = NULL;
    }
    if (keys & F_SHA1 && S_ISREG(fts->p->fts_statp->st_mode)) {
	rpmdc dc = _dc;
	dc->fn = fts->p->fts_accpath;
	dc->algo = PGPHASHALGO_SHA1;
	xx = rpmdcInitFile(dc);
	xx = rpmdcCalcFile(dc);
	/* XXX hotwire around rpmdcFini() for now. */
	fdFiniDigest(dc->fd, dc->algo, &dc->digest, &dc->digestlen, asAscii);
	if (dc->digest == NULL)
	    mtree_error("%s: %s", fts->p->fts_accpath, strerror(errno));
	else
	    output(indent, &offset, "sha1digest=%s", dc->digest);
	if (dc->fd != NULL) {
	    (void) rpmswAdd(&dc_readops, fdstat_op(dc->fd, FDSTAT_READ));
	    (void) rpmswAdd(&dc_digestops, fdstat_op(dc->fd, FDSTAT_DIGEST));
	    (void) Fclose(dc->fd);
	    dc->fd = NULL;
	    dc->digest = _free(dc->digest);
	    dc->digestlen = 0;
	}
	dc->algo = 0;
	dc->fn = NULL;
    }
    if (keys & F_SHA256 && S_ISREG(fts->p->fts_statp->st_mode)) {
	rpmdc dc = _dc;
	dc->fn = fts->p->fts_accpath;
	dc->algo = PGPHASHALGO_SHA256;
	xx = rpmdcInitFile(dc);
	xx = rpmdcCalcFile(dc);
	/* XXX hotwire around rpmdcFini() for now. */
	fdFiniDigest(dc->fd, dc->algo, &dc->digest, &dc->digestlen, asAscii);
	if (dc->digest == NULL)
	    mtree_error("%s: %s", fts->p->fts_accpath, strerror(errno));
	else
	    output(indent, &offset, "sha256digest=%s", dc->digest);
	if (dc->fd != NULL) {
	    (void) rpmswAdd(&dc_readops, fdstat_op(dc->fd, FDSTAT_READ));
	    (void) rpmswAdd(&dc_digestops, fdstat_op(dc->fd, FDSTAT_DIGEST));
	    (void) Fclose(dc->fd);
	    dc->fd = NULL;
	    dc->digest = _free(dc->digest);
	    dc->digestlen = 0;
	}
	dc->algo = 0;
	dc->fn = NULL;
    }
/*@=mods@*/
    if (keys & F_SLINK
     && (fts->p->fts_info == FTS_SL || fts->p->fts_info == FTS_SLNONE))
    {
	const char * name = rlink(fts->p->fts_accpath);
	escaped_name = xmalloc(strlen(name) * 4  +  1);
	(void) strvis(escaped_name, name, VIS_WHITE | VIS_OCTAL);
	output(indent, &offset, "link=%s", escaped_name);
	escaped_name = _free(escaped_name);
    }
    if (keys & F_FLAGS && !S_ISLNK(fts->p->fts_statp->st_mode)) {
#if defined(__linux__)
	output(indent, &offset, "flags=none");
#else
	char *file_flags;

	file_flags = fflagstostr(fts->p->fts_statp->st_flags);
	if (file_flags == NULL)
	    mtree_error("%s", strerror(errno));
	if (*file_flags != '\0')
	    output(indent, &offset, "flags=%s", file_flags);
	else
	    output(indent, &offset, "flags=none");
	free(file_flags);
#endif
    }
    (void) putchar('\n');
}

#ifdef	NOTYET
/*
 * We're assuming that there won't be a whole lot of excludes,
 * so it's OK to use a stupid algorithm.
 */
struct exclude {
	LIST_ENTRY(exclude) link;
	const char *glob;
	int pathname;
};

static LIST_HEAD(, exclude) excludes;

void
init_excludes(void)
{
	LIST_INIT(&excludes);
}

void
read_excludes_file(const char *name)
{
	FILE *fp;
	char *line, *str;
	struct exclude *e;
	size_t len;

	fp = fopen(name, "r");
	if (fp == 0)
		err(1, "%s", name);

	while ((line = fgetln(fp, &len)) != 0) {
		if (line[len - 1] == '\n')
			len--;
		if (len == 0)
			continue;

		str = malloc(len + 1);
		e = malloc(sizeof *e);
		if (str == 0 || e == 0)
			errx(1, "memory allocation error");
		e->glob = str;
		memcpy(str, line, len);
		str[len] = '\0';
		if (strchr(str, '/'))
			e->pathname = 1;
		else
			e->pathname = 0;
		LIST_INSERT_HEAD(&excludes, e, link);
	}
	fclose(fp);
}

int
check_excludes(const char *fname, const char *path)
{
	struct exclude *e;

	/* fnmatch(3) has a funny return value convention... */
#define MATCH(g, n) (fnmatch((g), (n), FNM_PATHNAME) == 0)

	LIST_FOREACH(e, &excludes, link) {
		if ((e->pathname && MATCH(e->glob, path))
		    || MATCH(e->glob, fname))
			return 1;
	}
	return 0;
}
#endif

#if defined(__linux__)
/*@null@*/ /*@observer@*/
static const char *my_getlogin(void)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    const char *s = getlogin();

    if (s && *s) {
	return (char *) s;
    } else {
	struct passwd *pw = getpwuid(geteuid());
	char *ss = NULL;
/*@-unrecog@*/
	if (pw != NULL && pw->pw_name != NULL) {
	    if (asprintf(&ss, _("(no controlling terminal) %s"), pw->pw_name) < 0) {
		perror("asprintf");
		return NULL;
	    }
	} else {
	    if (asprintf(&ss, _("(no controlling terminal) #%d"), geteuid()) < 0) {
		perror("asprintf");
		return NULL;
	    }
	}
/*@=unrecog@*/
	return ss;
    }
}
#define	__getlogin	my_getlogin
#else
#define	__getlogin	getlogin
#endif

int
mtreeCWalk(rpmfts fts)
{
    time_t clock;
    char host[MAXHOSTNAMELEN];
    int indent = 0;
    ARGV_t av;

    if (!MF_ISSET(NOCOMMENT)) {
	(void) time(&clock);
	(void) gethostname(host, sizeof(host));
	(void) printf(
	    "#\t   user: %s\n#\tmachine: %s\n#\t   tree: %s\n#\t   date: %s",
	    __getlogin(), host, fts->fullpath, ctime(&clock));
    }

    /* XXX should be done in main(). */
    if ((av = fts->paths) == NULL) {
	av = alloca(2 * sizeof(*av));
	av[0] = ".";
	av[1] = NULL;
    }

    if ((fts->t = Fts_open((char *const *)av, fts->ftsoptions, dsort)) == NULL)
	mtree_error("Fts_open: %s", strerror(errno));
    while ((fts->p = Fts_read(fts->t)) != NULL) {
	if (MF_ISSET(INDENT))
	    indent = fts->p->fts_level * 4;
#ifdef	NOTYET
	if (check_excludes(fts->p->fts_name, fts->p->fts_path)) {
	    Fts_set(fts->t, fts->p, FTS_SKIP);
	    continue;
	}
#endif
	switch(fts->p->fts_info) {
	case FTS_D:
	    if (!MF_ISSET(DIRSONLY))
		(void) printf("\n");
	    if (!MF_ISSET(NOCOMMENT))
		(void) printf("# %s\n", fts->p->fts_path);
	    (void) mtreeVisitD(fts);
	    mtreeVisitF(fts);
	    /*@switchbreak@*/ break;
	case FTS_DP:
	    if (!MF_ISSET(NOCOMMENT) && (fts->p->fts_level > 0))
		(void) printf("%*s# %s\n", indent, "", fts->p->fts_path);
	    (void) printf("%*s..\n", indent, "");
	    if (!MF_ISSET(DIRSONLY))
		(void) printf("\n");
	    /*@switchbreak@*/ break;
	case FTS_DNR:
	case FTS_ERR:
	case FTS_NS:
	    /* XXX TODO: warnx? */
	    (void) fprintf(stderr, "%s: %s: %s\n",
		    __progname, fts->p->fts_path, strerror(fts->p->fts_errno));
	    /*@switchbreak@*/ break;
	default:
	    if (!MF_ISSET(DIRSONLY))
		mtreeVisitF(fts);
	    /*@switchbreak@*/ break;
	}
    }
    (void) Fts_close(fts->t);
    fts->p = NULL;
    fts->t = NULL;

    if (MF_ISSET(SEEDED) && fts->keys & F_CKSUM)
	/* XXX TODO: warnx? */
	(void) fprintf(stderr, _("%s: %s checksum: %u\n"),
		__progname, fts->fullpath, (unsigned)fts->crc_total);

    return 0;
}

/*==============================================================*/

void
mtreeMiss(rpmfts fts, /*@null@*/ NODE * p, char * tail)
{
    int create;
    char *tp;

    for (; p != NULL; p = p->next) {
	if ((p->flags & F_OPT) && !(p->flags & F_VISIT))
	    continue;
	if (p->type != F_DIR && (MF_ISSET(DIRSONLY) || p->flags & F_VISIT))
	    continue;
	(void) strcpy(tail, p->name);
	if (!(p->flags & F_VISIT)) {
	    /* Don't print missing message if file exists as a
	       symbolic link and the -q flag is set. */
	    struct stat statbuf;

	    if (MF_ISSET(QUIET) && Stat(fts->path, &statbuf) == 0)
		p->flags |= F_VISIT;
	    else
		(void) printf("missing: %s", fts->path);
	}
	if (p->type != F_DIR) {
	    (void) putchar('\n');
	    continue;
	}

	create = 0;
	if (!(p->flags & F_VISIT) && MF_ISSET(UPDATE)) {
	    if (!(p->flags & (F_UID | F_UNAME)))
		(void) printf(" (not created: user not specified)");
	    else if (!(p->flags & (F_GID | F_GNAME)))
		(void) printf(" (not created: group not specified)");
	    else if (!(p->flags & F_MODE))
		(void) printf(" (not created: mode not specified)");
	    else if (Mkdir(fts->path, S_IRWXU))
		(void) printf(" (not created: %s)", strerror(errno));
	    else {
		create = 1;
		(void) printf(" (created)");
	    }
	}

	if (!(p->flags & F_VISIT))
	    (void) putchar('\n');

	for (tp = tail; *tp != '\0'; ++tp);
	*tp = '/';
	mtreeMiss(fts, p->child, tp + 1);
	*tp = '\0';

	if (!create)
	    continue;
	if (Chown(fts->path, p->st_uid, p->st_gid)) {
	    (void) printf("%s: user/group/mode not modified: %s\n",
		    fts->path, strerror(errno));
	    continue;
	}
	if (Chmod(fts->path, p->st_mode))
	    (void) printf("%s: permissions not set: %s\n",
		    fts->path, strerror(errno));
    }
}

int
mtreeVWalk(rpmfts fts)
{
    NODE *ep, *level;
    int specdepth, rval;
    ARGV_t av;

    /* XXX should be done in main(). */
    if ((av = fts->paths) == NULL) {
	av = alloca(2 * sizeof(*av));
	av[0] = ".";
	av[1] = NULL;
    }

    if ((fts->t = Fts_open((char *const *)av, fts->ftsoptions, NULL)) == NULL)
	mtree_error("Fts_open: %s", strerror(errno));
    level = fts->root;
    specdepth = rval = 0;
    while ((fts->p = Fts_read(fts->t)) != NULL) {
	switch(fts->p->fts_info) {
	case FTS_D:
	   /*@switchbreak@*/ break;
	case FTS_DP:
	    if (specdepth > fts->p->fts_level) {
		for (level = level->parent; level->prev != NULL; level = level->prev);  
		--specdepth;
	    }
	    continue;
	case FTS_DNR:
	case FTS_ERR:
	case FTS_NS:
	    (void) fprintf(stderr, "%s: %s: %s\n",
			__progname, RP(fts->p), strerror(fts->p->fts_errno));
	    continue;
	default:
	    if (MF_ISSET(DIRSONLY))
		continue;
	}

	if (specdepth != fts->p->fts_level)
	    goto extra;
	for (ep = level; ep != NULL; ep = ep->next)
	    if ((ep->flags & F_MAGIC &&
/*@-moduncon@*/
		!fnmatch(ep->name, fts->p->fts_name, FNM_PATHNAME)) ||
/*@=moduncon@*/
		!strcmp(ep->name, fts->p->fts_name))
	    {
		ep->flags |= F_VISIT;
		if ((ep->flags & F_NOCHANGE) == 0 &&
		    compare(ep->name, ep, fts->p))
			rval = MISMATCHEXIT;
		if (ep->flags & F_IGN)
			(void) Fts_set(fts->t, fts->p, FTS_SKIP);
		else if (ep->child && ep->type == F_DIR &&
		    fts->p->fts_info == FTS_D) {
			level = ep->child;
			++specdepth;
		}
		/*@innerbreak@*/ break;
	    }

	if (ep != NULL)
	    continue;
extra:
	if (!MF_ISSET(IGNORE)) {
	    (void) printf("extra: %s", RP(fts->p));
	    if (MF_ISSET(REMOVE)) {
		if ((S_ISDIR(fts->p->fts_statp->st_mode)
		    ? Rmdir : Unlink)(fts->p->fts_accpath)) {
			(void) printf(", not removed: %s", strerror(errno));
		} else
			(void) printf(", removed");
	    }
	    (void) putchar('\n');
	}
	(void) Fts_set(fts->t, fts->p, FTS_SKIP);
    }
    (void) Fts_close(fts->t);
    fts->p = NULL;
    fts->t = NULL;
    if (MF_ISSET(SEEDED))
	(void) fprintf(stderr, "%s: %s checksum: %u\n",
		__progname, fts->fullpath, (unsigned) fts->crc_total);
    return rval;
}

/*==============================================================*/

/**
 */
static void mtreeArgCallback(poptContext con,
                /*@unused@*/ enum poptCallbackReason reason,
                const struct poptOption * opt, const char * arg,
                /*@unused@*/ void * data)
        /*@globals _rpmfts, rpmioFtsOpts, fileSystem @*/
        /*@modifies _rpmfts, rpmioFtsOpts, fileSystem @*/
{
    char * p;

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {

    /* XXX TODO: Mac OS X differs here. */
    case 'f':
	if (!(freopen(arg, "r", stdin)))
	    mtree_error("%s: %s", arg, strerror(errno));
	break;
    case 'K':
/*@-unrecog@*/
	while ((p = strsep((char **)&arg, " \t,")) != NULL)
	    if (*p != '\0')
		_rpmfts->keys |= parsekey(p, NULL);
/*@=unrecog@*/
	break;
    case 'k':
	_rpmfts->keys = F_TYPE;
	while ((p = strsep((char **)&arg, " \t,")) != NULL)
	    if (*p != '\0')
		_rpmfts->keys |= parsekey(p, NULL);
	break;
#if !defined(POPT_ARG_ARGV)
    case 'p':
assert(arg != NULL);
	(void) _poptSaveString(&_rpmfts->paths, opt->argInfo, arg);
	break;
#endif

    /* XXX redundant with --logical. */
    case 'L':
	rpmioFtsOpts &= ~FTS_PHYSICAL;
	rpmioFtsOpts |= FTS_LOGICAL;
	break;
    /* XXX redundant with --physical. */
    case 'P':
	rpmioFtsOpts &= ~FTS_LOGICAL;
	rpmioFtsOpts |= FTS_PHYSICAL;
	break;
#ifdef	NOTYET
    case 'X':
	read_excludes_file(arg);
	break;
#endif

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
        mtreeArgCallback, 0, NULL, NULL },
/*@=type@*/

    /* XXX redundant with --logical. */
  { NULL,'L', POPT_ARG_NONE,	NULL, (int)'L',
	N_("Follow symlinks"), NULL },
    /* XXX redundant with --physical. */
  { NULL,'P', POPT_ARG_NONE,	NULL, (int)'P',
	N_("Don't follow symlinks"), NULL },
#ifdef	NOTYET
  { NULL,'X', POPT_ARG_NONE,	NULL, (int)'X',
	N_("Read fnmatch(3) exclude patterns from <file>"), N_("<file>") },
#endif

  { "spec",'c', POPT_BIT_SET,		&mtreeFlags, MTREE_FLAGS_SPEC,
	N_("Print file tree specification to stdout"), NULL },
  { "dirs",'d', POPT_BIT_SET,		&mtreeFlags, MTREE_FLAGS_DIRSONLY,
	N_("Directories only"), NULL },
  { "ignore",'e', POPT_BIT_SET,		&mtreeFlags, MTREE_FLAGS_IGNORE,
	N_("Don't complain about files not in the specification"), NULL },
  { "file",'f', POPT_ARG_STRING,	NULL, (int)'f',
	N_("Read file tree <spec>"), N_("<spec>") },
  { "indent",'i', POPT_BIT_SET,		&mtreeFlags, MTREE_FLAGS_INDENT,
	N_("Indent sub-directories"), NULL },
  { "add",'K', POPT_ARG_STRING,	NULL, (int)'K',
	N_("Add <key> to specification"), N_("<key>") },
  { "key",'k', POPT_ARG_STRING,	NULL, (int)'k',
	N_("Use \"type\" keywords instead"), N_("<key>") },
  { "loose",'l', POPT_BIT_SET,		&mtreeFlags, MTREE_FLAGS_LOOSE,
	N_("Loose permissions check"), NULL },
  { "nocomment",'n', POPT_BIT_SET,	&mtreeFlags, MTREE_FLAGS_NOCOMMENT,
	N_("Don't include sub-directory comments"), NULL },
#if defined(POPT_ARG_ARGV)
  { "path",'p', POPT_ARG_ARGV,	&__rpmfts.paths, 0,
	N_("Use <path> rather than current directory"), N_("<path>") },
#else
  { "path",'p', POPT_ARG_STRING,	NULL, 'p',
	N_("Use <path> rather than current directory"), N_("<path>") },
#endif
  /* XXX --quiet collides w poptIO */
  { "quiet",'q', POPT_BIT_SET,		&mtreeFlags, MTREE_FLAGS_QUIET,
	N_("Quiet mode"), NULL },
  { "remove",'r', POPT_BIT_SET,		&mtreeFlags, MTREE_FLAGS_REMOVE,
	N_("Remove files not in specification"), NULL },
  { "seed",'s', POPT_ARG_INT,		&__rpmfts.crc_total, 0,
	N_("Display crc for file(s) with <seed>"), N_("<seed>") },
  { "touch",'t', POPT_BIT_SET,		&mtreeFlags, MTREE_FLAGS_TOUCH,
	N_("Touch files iff timestamp differs"), NULL },
  { "update",'u', POPT_BIT_SET,		&mtreeFlags, MTREE_FLAGS_UPDATE,
	N_("Update owner/group/permissions to match specification"), NULL },
  { "mismatch",'U', POPT_BIT_SET, &mtreeFlags, (MTREE_FLAGS_UPDATE|MTREE_FLAGS_MISMATCHOK),
	N_("Same as -u, but ignore match status on exit"), NULL },
  { "warn",'w', POPT_BIT_SET,		&mtreeFlags, MTREE_FLAGS_WARN,
	N_("Treat missing uid/gid as warning"), NULL },
    /* XXX duplicated with --xdev. */
  { "xdev",'x', POPT_BIT_SET,  &rpmioFtsOpts, FTS_XDEV,
	N_("Don't cross mount points"), NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioFtsPoptTable, 0,
	N_("Fts(3) traversal options:"), NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioDigestPoptTable, 0,
	N_("Available digests:"), NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"),
	NULL },

  POPT_AUTOALIAS
  POPT_AUTOHELP

  { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
        "\
Usage: mtree [-cdeilnqrtUux] [-f spec] [-K key] [-k key] [-p path] [-s seed]\n\
", NULL },

  POPT_TABLEEND
};

int
main(int argc, char *argv[])
	/*@globals _rpmfts, rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies _rpmfts, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    poptContext optCon = rpmioInit(argc, argv, optionsTable);
    int rc;

#ifdef	NOTYET
    init_excludes();
#endif
    _rpmfts->keys = KEYDEFAULT;
    _rpmfts->fflags = 0xffffffff;

    /* Process all options, whine if unknown. */
    while ((rc = poptGetNextOpt(optCon)) > 0) {
        const char * optArg = poptGetOptArg(optCon);
/*@-dependenttrans -modobserver -observertrans @*/
	optArg = _free(optArg);
/*@=dependenttrans =modobserver =observertrans @*/
        switch (rc) {
        default:
	    mtree_error("Option table misconfigured");
            /*@notreached@*/ /*@switchbreak@*/ break;
        }
    }

    argv = (char **) poptGetArgs(optCon);
    if (!(argv == NULL || argv[0] == NULL)) {
	poptPrintUsage(optCon, stderr, 0);
	exit(EXIT_FAILURE);
    }

    if (MF_ISSET(SPEC) || MF_ISSET(SEEDED)) {
	char fullpath[MAXPATHLEN];
	if (!getcwd(fullpath, sizeof fullpath))
	    mtree_error("getcwd: %s", strerror(errno));
	_rpmfts->fullpath = xstrdup(fullpath);
    }

    if (MF_ISSET(LOOSE) && MF_ISSET(UPDATE))
	mtree_error("-l and -u flags are mutually exclusive");

    /*
     * Either FTS_PHYSICAL or FTS_LOGICAL must be set. Don't follow symlinks
     * unless explicitly overridden with FTS_LOGICAL.
     */
    _rpmfts->ftsoptions = rpmioFtsOpts;
    switch (_rpmfts->ftsoptions & (FTS_LOGICAL|FTS_PHYSICAL)) {
    case (FTS_LOGICAL|FTS_PHYSICAL):
	mtree_error("-L and -P flags are mutually exclusive");
	/*@notreached@*/ break;
    case 0:
	_rpmfts->ftsoptions |= FTS_PHYSICAL;
	break;
    }

    /* XXX prohibits -s 0 invocation */
    if (_rpmfts->crc_total)
	mtreeFlags |= MTREE_FLAGS_SEEDED;
    _rpmfts->crc_total ^= 0xffffffff;

    (void) rpmswEnter(&dc_totalops, -1);

    if (MF_ISSET(SPEC))
	rc = mtreeCWalk(_rpmfts);
    else {
/*@-evalorder@*/
	_rpmfts->root = mtreeSpec(_rpmfts);
/*@=evalorder@*/
	_rpmfts->path = xmalloc(MAXPATHLEN);
	rc = mtreeVWalk(_rpmfts);
	mtreeMiss(_rpmfts, _rpmfts->root, _rpmfts->path);
	_rpmfts->path = _free(_rpmfts->path);
    }

    if (MF_ISSET(MISMATCHOK) && (rc == MISMATCHEXIT))
	rc = 0;

    (void) rpmswExit(&dc_totalops, 0);
    if (_rpmsw_stats) {
        rpmswPrint(" total:", &dc_totalops);
        rpmswPrint("  read:", &dc_readops);
        rpmswPrint("digest:", &dc_digestops);
    }

    _rpmfts->paths = argvFree(_rpmfts->paths);
    _rpmfts->fullpath = _free(_rpmfts->fullpath);

    optCon = rpmioFini(optCon);

    return rc;
}

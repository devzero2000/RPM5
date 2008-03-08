/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define __FBSDID(_s)	
#define BSDTAR_VERSION_STRING "2.4.12"

#define ARCHIVE_STAT_CTIME_NANOS(st)    (st)->st_ctim.tv_nsec
#define ARCHIVE_STAT_MTIME_NANOS(st)    (st)->st_mtim.tv_nsec

#define	HAVE_LIBZ		1
#define	HAVE_LIBBZ2		1

struct archive_entry;
#define	archive_version()	BSDTAR_VERSION_STRING

/* Default: Do not try to set owner/group. */
#define ARCHIVE_EXTRACT_OWNER   (1)
/* Default: Do obey umask, do not restore SUID/SGID/SVTX bits. */
#define ARCHIVE_EXTRACT_PERM    (2)
/* Default: Do not restore mtime/atime. */
#define ARCHIVE_EXTRACT_TIME    (4)
/* Default: Replace existing files. */
#define ARCHIVE_EXTRACT_NO_OVERWRITE (8)
/* Default: Try create first, unlink only if create fails with EEXIST. */
#define ARCHIVE_EXTRACT_UNLINK  (16)
/* Default: Do not restore ACLs. */
#define ARCHIVE_EXTRACT_ACL     (32)
/* Default: Do not restore fflags. */
#define ARCHIVE_EXTRACT_FFLAGS  (64)
/* Default: Do not restore xattrs. */
#define ARCHIVE_EXTRACT_XATTR   (128)
/* Default: Do not try to guard against extracts redirected by symlinks. */
/* Note: With ARCHIVE_EXTRACT_UNLINK, will remove any intermediate symlink. */
#define ARCHIVE_EXTRACT_SECURE_SYMLINKS (256)
/* Default: Do not reject entries with '..' as path elements. */
#define ARCHIVE_EXTRACT_SECURE_NODOTDOT (512)
/* Default: Create parent directories as needed. */
#define ARCHIVE_EXTRACT_NO_AUTODIR (1024)
/* Default: Overwrite files, even if one on disk is newer. */
#define ARCHIVE_EXTRACT_NO_OVERWRITE_NEWER (2048)

#include "system.h"
__FBSDID("$FreeBSD: src/usr.bin/tar/bsdtar.c,v 1.79 2008/01/22 07:23:44 kientzle Exp $");

#include <rpmio.h>
#include <argv.h>

#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif

#include <popt.h>
#include "rpmtar.h"

#include "debug.h"

/*
 * Per POSIX.1-1988, tar defaults to reading/writing archives to/from
 * the default tape device for the system.  Pick something reasonable here.
 */
#if defined(__linux__)
#define	_PATH_DEFTAPE "/dev/st0"
#endif

#if !defined(_PATH_DEFTAPE)
#define	_PATH_DEFTAPE "/dev/tape"
#endif

/*@unchecked@*/
static int _debug = 1;

/* A basic set of security flags to request from libarchive. */
#define	SECURITY					\
	(ARCHIVE_EXTRACT_SECURE_SYMLINKS		\
	 | ARCHIVE_EXTRACT_SECURE_NODOTDOT)

/*@unchecked@*/
static struct bsdtar bsdtar_storage = {
	.progname = "rpmtar",	/* Need bsdtar->progname for bsdtar_warnc. */
	.fd = -1,		/* Mark as "unused" */
	/* Default: preserve mod time on extract */
	/* Default: Perform basic security checks. */
	.extract_flags = ARCHIVE_EXTRACT_TIME | SECURITY,

};

/*==============================================================*/
/* External function to parse a date/time string (from getdate.y) */
static
time_t get_date(const char *p)
{
    return time(NULL);
}

static void
bsdtar_vwarnc(struct bsdtar *bsdtar, int code, const char *fmt, va_list ap)
	/*@globals fileSystem @*/
	/*@modifies ap, fileSystem @*/
{
	fprintf(stderr, "%s: ", bsdtar->progname);
	vfprintf(stderr, fmt, ap);
	if (code != 0)
		fprintf(stderr, ": %s", strerror(code));
	fprintf(stderr, "\n");
}

void
bsdtar_warnc(struct bsdtar *bsdtar, int code, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	bsdtar_vwarnc(bsdtar, code, fmt, ap);
	va_end(ap);
}

void
bsdtar_errc(struct bsdtar *bsdtar, int eval, int code, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	bsdtar_vwarnc(bsdtar, code, fmt, ap);
	va_end(ap);
	exit(eval);
}

/*-
 * The logic here for -C DIR attempts to avoid
 * chdir() as long as possible.  For example:
 * "-C /foo -C /bar file"          needs chdir("/bar") but not chdir("/foo")
 * "-C /foo -C bar file"           needs chdir("/foo/bar")
 * "-C /foo -C bar /file1"         does not need chdir()
 * "-C /foo -C bar /file1 file2"   needs chdir("/foo/bar") before file2
 *
 * The only correct way to handle this is to record a "pending" chdir
 * request and combine multiple requests intelligently until we
 * need to process a non-absolute file.  set_chdir() adds the new dir
 * to the pending list; do_chdir() actually executes any pending chdir.
 *
 * This way, programs that build tar command lines don't have to worry
 * about -C with non-existent directories; such requests will only
 * fail if the directory must be accessed.
 */
void
set_chdir(struct bsdtar *bsdtar, const char *newdir)
{
	if (newdir[0] == '/') {
		/* The -C /foo -C /bar case; dump first one. */
		free(bsdtar->pending_chdir);
		bsdtar->pending_chdir = NULL;
	}
	if (bsdtar->pending_chdir == NULL)
		/* Easy case: no previously-saved dir. */
		bsdtar->pending_chdir = strdup(newdir);
	else {
		/* The -C /foo -C bar case; concatenate */
		char *old_pending = bsdtar->pending_chdir;
		size_t old_len = strlen(old_pending);
		bsdtar->pending_chdir = malloc(old_len + strlen(newdir) + 2);
		if (old_pending[old_len - 1] == '/')
			old_pending[old_len - 1] = '\0';
		if (bsdtar->pending_chdir != NULL)
			sprintf(bsdtar->pending_chdir, "%s/%s",
			    old_pending, newdir);
		free(old_pending);
	}
	if (bsdtar->pending_chdir == NULL)
		bsdtar_errc(bsdtar, 1, errno, "No memory");
}

/*
 * The matching logic here needs to be re-thought.  I started out to
 * try to mimic gtar's matching logic, but it's not entirely
 * consistent.  In particular 'tar -t' and 'tar -x' interpret patterns
 * on the command line as anchored, but --exclude doesn't.
 */

/*
 * Utility functions to manage exclusion/inclusion patterns
 */

int
exclude(struct bsdtar *bsdtar, const char *pattern)
{
#ifdef	NOTYET
	struct matching *matching;

	if (bsdtar->matching == NULL)
		initialize_matching(bsdtar);
	matching = bsdtar->matching;
	add_pattern(bsdtar, &(matching->exclusions), pattern);
	matching->exclusions_count++;
#endif
	return (0);
}

int
exclude_from_file(struct bsdtar *bsdtar, const char *pathname)
{
#ifdef	NOTYET
	return (process_lines(bsdtar, pathname, &exclude));
#else
	return (0);
#endif
}

int
include(struct bsdtar *bsdtar, const char *pattern)
{
#ifdef	NOTYET
	struct matching *matching;

	if (bsdtar->matching == NULL)
		initialize_matching(bsdtar);
	matching = bsdtar->matching;
	add_pattern(bsdtar, &(matching->inclusions), pattern);
	matching->inclusions_count++;
	matching->inclusions_unmatched_count++;
#endif
	return (0);
}

void
cleanup_exclusions(struct bsdtar *bsdtar)
{
#ifdef	NOTYET
	struct match *p, *q;

	if (bsdtar->matching) {
		p = bsdtar->matching->inclusions;
		while (p != NULL) {
			q = p;
			p = p->next;
			free(q);
		}
		p = bsdtar->matching->exclusions;
		while (p != NULL) {
			q = p;
			p = p->next;
			free(q);
		}
		free(bsdtar->matching);
	}
#endif
}

/*==============================================================*/

void	tar_mode_c(struct bsdtar *bsdtar)
	/*@globals fileSystem @*/
	/*@modifies bsdtar, fileSystem @*/
{ fprintf(stderr, "==> tar_mode_c: %s\n", (bsdtar->filename ?: "")); return; }
void	tar_mode_r(struct bsdtar *bsdtar)
	/*@globals fileSystem @*/
	/*@modifies bsdtar, fileSystem @*/
{ fprintf(stderr, "==> tar_mode_r: %s\n", (bsdtar->filename ?: "")); return; }
void	tar_mode_t(struct bsdtar *bsdtar)
	/*@globals fileSystem @*/
	/*@modifies bsdtar, fileSystem @*/
{ fprintf(stderr, "==> tar_mode_t: %s\n", (bsdtar->filename ?: "")); return; }
void	tar_mode_u(struct bsdtar *bsdtar)
	/*@globals fileSystem @*/
	/*@modifies bsdtar, fileSystem @*/
{ fprintf(stderr, "==> tar_mode_u: %s\n", (bsdtar->filename ?: "")); return; }
void	tar_mode_x(struct bsdtar *bsdtar)
	/*@globals fileSystem @*/
	/*@modifies bsdtar, fileSystem @*/
{ fprintf(stderr, "==> tar_mode_x: %s\n", (bsdtar->filename ?: "")); return; }

/*==============================================================*/

/*-
 * Convert traditional tar arguments into new-style.
 * For example,
 *     tar tvfb file.tar 32 --exclude FOO
 * will be converted to
 *     tar -t -v -f file.tar -b 32 --exclude FOO
 *
 * This requires building a new argv array.  The initial bundled word
 * gets expanded into a new string that looks like "-t\0-v\0-f\0-b\0".
 * The new argv array has pointers into this string intermingled with
 * pointers to the existing arguments.  Arguments are moved to
 * immediately follow their options.
 *
 * The optstring argument here is the same one passed to getopt(3).
 * It is used to determine which option letters have trailing arguments.
 */
/*@null@*/
static char **
rewrite_argv(struct bsdtar *bsdtar, int *argc, char **src_argv,
		const char *optstring)
	/*@globals fileSystem @*/
	/*@modifies *argc, fileSystem @*/
{
	char **new_argv, **dest_argv;
	const char *p;
	char *src, *dest;

	if (src_argv[0] == NULL ||
	    src_argv[1] == NULL || src_argv[1][0] == '-')
		return (src_argv);

	*argc += strlen(src_argv[1]) - 1;
	new_argv = malloc((*argc + 1) * sizeof(new_argv[0]));
	if (new_argv == NULL)
		bsdtar_errc(bsdtar, 1, errno, "No Memory");

	dest_argv = new_argv;
	*dest_argv++ = *src_argv++;

	dest = malloc(strlen(*src_argv) * 3);
	if (dest == NULL)
		bsdtar_errc(bsdtar, 1, errno, "No memory");
	for (src = *src_argv++; *src != '\0'; src++) {
		*dest_argv++ = dest;
		*dest++ = '-';
		*dest++ = *src;
		*dest++ = '\0';
		/* If option takes an argument, insert that into the list. */
		for (p = optstring; p != NULL && *p != '\0'; p++) {
			if (*p != *src)
				/*@innercontinue@*/ continue;
			if (p[1] != ':')	/* No arg required, done. */
				/*@innerbreak@*/ break;
			if (*src_argv == NULL)	/* No arg available? Error. */
				bsdtar_errc(bsdtar, 1, 0,
				    "Option %c requires an argument",
				    *src);
			*dest_argv++ = *src_argv++;
			/*@innerbreak@*/ break;
		}
	}

	/* Copy remaining arguments, including trailing NULL. */
	while ((*dest_argv++ = *src_argv++) != NULL)
		;

	return (new_argv);
}

/*
 * The leading '+' here forces the GNU version of getopt() (as well as
 * both the GNU and BSD versions of getopt_long) to stop at the first
 * non-option.  Otherwise, GNU getopt() permutes the arguments and
 * screws up -C processing.
 */
/*@unchecked@*/ /*@observer@*/
static const char *tar_opts = "+Bb:C:cf:HhI:jkLlmnOoPprtT:UuvW:wX:xyZz";

/*
 * Most of these long options are deliberately not documented.  They
 * are provided only to make life easier for people who also use GNU tar.
 * The only long options documented in the manual page are the ones
 * with no corresponding short option, such as --exclude, --nodump,
 * and --fast-read.
 *
 * On systems that lack getopt_long, long options can be specified
 * using -W longopt and -W longopt=value, e.g. "-W nodump" is the same
 * as "--nodump" and "-W exclude=pattern" is the same as "--exclude
 * pattern".  This does not rely the GNU getopt() "W;" extension, so
 * should work correctly on any system with a POSIX-compliant getopt().
 */

/* Fake short equivalents for long options that otherwise lack them. */
enum {
	OPTION_CHECK_LINKS=1,
	OPTION_EXCLUDE,
	OPTION_FAST_READ,
	OPTION_FORMAT,
	OPTION_INCLUDE,
	OPTION_NEWER_CTIME,
	OPTION_NEWER_CTIME_THAN,
	OPTION_NEWER_MTIME,
	OPTION_NEWER_MTIME_THAN,
	OPTION_NODUMP,
	OPTION_NO_SAME_OWNER,
	OPTION_NO_SAME_PERMISSIONS,
	OPTION_NULL,
	OPTION_ONE_FILE_SYSTEM,
	OPTION_POSIX,
	OPTION_STRIP_COMPONENTS,
	OPTION_TOTALS,
	OPTION_USE_COMPRESS_PROGRAM,
	OPTION_VERSION
};

static void
set_mode(struct bsdtar *bsdtar, char opt)
	/*@globals fileSystem @*/
	/*@modifies bsdtar, fileSystem @*/
{
	if (bsdtar->mode != '\0' && bsdtar->mode != opt)
		bsdtar_errc(bsdtar, 1, 0,
		    "Can't specify both -%c and -%c", opt, bsdtar->mode);
	bsdtar->mode = opt;
}

/*@unchecked@*/
static char option_o = 0;

static void bsdtarArgCallback(/*@unused@*/ poptContext con,
                /*@unused@*/ enum poptCallbackReason reason,
                const struct poptOption * opt, const char * arg,
                void * data)
	/*@modifies *data @*/
{
	struct bsdtar * bsdtar = data;
	int val = opt->val;
	int t;

if (_debug)
fprintf(stderr, "--> bsdtarArgCallback(%p, %d, %p, %p, %p) val %d\n", con, reason, opt, arg, data, val);
	/*
	 * Comments following each option indicate where that option
	 * originated:  SUSv2, POSIX, GNU tar, star, etc.  If there's
	 * no such comment, then I don't know of anyone else who
	 * implements that option.
	 */

	/* XXX avoid accidental collisions with POPT_BIT_SET for flags */
	if (opt->arg == NULL)
	switch (val) {
	case 'B': /* GNU tar */
		/* libarchive doesn't need this; just ignore it. */
		break;
	case 'b': /* SUSv2 */
		t = atoi(arg);
		if (t <= 0 || t > 1024)
			bsdtar_errc(bsdtar, 1, 0,
			    "Argument to -b is out of range (1..1024)");
		bsdtar->bytes_per_block = 512 * t;
		break;
	case 'C': /* GNU tar */
		set_chdir(bsdtar, arg);
		break;
	case 'c': /* SUSv2 */
		set_mode(bsdtar, val);
		break;
	case OPTION_EXCLUDE: /* GNU tar */
		if (exclude(bsdtar, arg))
			bsdtar_errc(bsdtar, 1, 0,
			    "Couldn't exclude %s\n", arg);
		break;
	case OPTION_FORMAT: /* GNU tar, others */
		bsdtar->create_format = arg;
		break;
	case 'f': /* SUSv2 */
		bsdtar->filename = arg;
		break;
	case OPTION_FAST_READ: /* GNU tar */
		bsdtar->option_fast_read = 1;
		break;
	case 'H': /* BSD convention */
		bsdtar->symlink_mode = 'H';
		break;
	case 'h': /* Linux Standards Base, gtar; synonym for -L */
		bsdtar->symlink_mode = 'L';
		break;
	case 'I': /* GNU tar */
		/*
		 * TODO: Allow 'names' to come from an archive,
		 * not just a text file.  Design a good UI for
		 * allowing names and mode/owner to be read
		 * from an archive, with contents coming from
		 * disk.  This can be used to "refresh" an
		 * archive or to design archives with special
		 * permissions without having to create those
		 * permissions on disk.
		 */
		bsdtar->names_from_file = arg;
		break;
	case OPTION_INCLUDE:
		/*
		 * Noone else has the @archive extension, so
		 * noone else needs this to filter entries
		 * when transforming archives.
		 */
		if (include(bsdtar, arg))
			bsdtar_errc(bsdtar, 1, 0,
			    "Failed to add %s to inclusion list",
			    arg);
		break;
	case 'j': /* GNU tar */
#if HAVE_LIBBZ2
		if (bsdtar->create_compression != '\0')
			bsdtar_errc(bsdtar, 1, 0,
			    "Can't specify both -%c and -%c", val,
			    bsdtar->create_compression);
		bsdtar->create_compression = val;
#else
		bsdtar_warnc(bsdtar, 0, "-j compression not supported by this version of rpmtar");
		poptPrintUsage(con, stderr, 0);
		exit(EXIT_FAILURE);
#endif
		break;
	case 'k': /* GNU tar */
		bsdtar->extract_flags |= ARCHIVE_EXTRACT_NO_OVERWRITE;
		break;
	case 'L': /* BSD convention */
		bsdtar->symlink_mode = 'L';
		break;
        case 'l': /* SUSv2 and GNU tar beginning with 1.16 */
		/* GNU tar 1.13  used -l for --one-file-system */
		bsdtar->option_warn_links = 1;
		break;
	case 'm': /* SUSv2 */
		bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_TIME;
		break;
	case 'n': /* GNU tar */
		bsdtar->option_no_subdirs = 1;
		break;
        /*
	 * Selecting files by time:
	 *    --newer-?time='date' Only files newer than 'date'
	 *    --newer-?time-than='file' Only files newer than time
	 *         on specified file (useful for incremental backups)
	 * TODO: Add corresponding "older" options to reverse these.
	 */
	case OPTION_NEWER_CTIME: /* GNU tar */
		bsdtar->newer_ctime_sec = get_date(arg);
		break;
	case OPTION_NEWER_CTIME_THAN:
	{	struct stat st;
		if (stat(arg, &st) != 0)
			bsdtar_errc(bsdtar, 1, 0, "Can't open file %s", arg);
		bsdtar->newer_ctime_sec = st.st_ctime;
		bsdtar->newer_ctime_nsec = ARCHIVE_STAT_CTIME_NANOS(&st);
	}	break;
	case OPTION_NEWER_MTIME: /* GNU tar */
		bsdtar->newer_mtime_sec = get_date(arg);
		break;
	case OPTION_NEWER_MTIME_THAN:
	{	struct stat st;
		if (stat(arg, &st) != 0)
			bsdtar_errc(bsdtar, 1, 0, "Can't open file %s", arg);
		bsdtar->newer_mtime_sec = st.st_mtime;
		bsdtar->newer_mtime_nsec = ARCHIVE_STAT_MTIME_NANOS(&st);
	}	break;
	case OPTION_NODUMP: /* star */
		bsdtar->option_honor_nodump = 1;
		break;
	case OPTION_NO_SAME_OWNER: /* GNU tar */
		bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_OWNER;
		break;
	case OPTION_NO_SAME_PERMISSIONS: /* GNU tar */
		bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_PERM;
		bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_ACL;
		bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_XATTR;
		bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_FFLAGS;
		break;
	case OPTION_NULL: /* GNU tar */
		bsdtar->option_null++;
		break;
	case 'O': /* GNU tar */
		bsdtar->option_stdout = 1;
		break;
	case 'o': /* SUSv2 and GNU conflict here, but not fatally */
		option_o = 1; /* Record it and resolve it later. */
		break;
	case OPTION_ONE_FILE_SYSTEM: /* GNU tar */
		bsdtar->option_dont_traverse_mounts = 1;
		break;
#if 0
	/*
	 * The common BSD -P option is not necessary, since
	 * our default is to archive symlinks, not follow
	 * them.  This is convenient, as -P conflicts with GNU
	 * tar anyway.
	 */
	case 'P': /* BSD convention */
		/* Default behavior, no option necessary. */
		break;
#endif
	case 'P': /* GNU tar */
		bsdtar->extract_flags &= ~SECURITY;
		bsdtar->option_absolute_paths = 1;
		break;
	case 'p': /* GNU tar, star */
		bsdtar->extract_flags |= ARCHIVE_EXTRACT_PERM;
		bsdtar->extract_flags |= ARCHIVE_EXTRACT_ACL;
		bsdtar->extract_flags |= ARCHIVE_EXTRACT_XATTR;
		bsdtar->extract_flags |= ARCHIVE_EXTRACT_FFLAGS;
		break;
	case OPTION_POSIX: /* GNU tar */
		bsdtar->create_format = "pax";
		break;
	case 'r': /* SUSv2 */
		set_mode(bsdtar, val);
		break;
	case OPTION_STRIP_COMPONENTS: /* GNU tar 1.15 */
		bsdtar->strip_components = atoi(arg);
		break;
	case 'T': /* GNU tar */
		bsdtar->names_from_file = arg;
		break;
	case 't': /* SUSv2 */
		set_mode(bsdtar, val);
		bsdtar->verbose++;
		break;
	case OPTION_TOTALS: /* GNU tar */
		bsdtar->option_totals++;
		break;
	case 'U': /* GNU tar */
		bsdtar->extract_flags |= ARCHIVE_EXTRACT_UNLINK;
		bsdtar->option_unlink_first = 1;
		break;
	case 'u': /* SUSv2 */
		set_mode(bsdtar, val);
		break;
	case 'v': /* SUSv2 */
		bsdtar->verbose++;
		break;
	case OPTION_VERSION: /* GNU convention */
		printf("%s %s (libarchive %s)\n", bsdtar->progname,
			BSDTAR_VERSION_STRING, archive_version());
		exit(EXIT_FAILURE);
		break;
#if 0
	/*
	 * The -W longopt feature is handled inside of
	 * bsdtar_getop(), so -W is not available here.
	 */
	case 'W': /* Obscure, but useful GNU convention. */
		break;
#endif
	case 'w': /* SUSv2 */
		bsdtar->option_interactive = 1;
		break;
	case 'X': /* GNU tar */
		if (exclude_from_file(bsdtar, arg))
			bsdtar_errc(bsdtar, 1, 0,
			    "failed to process exclusions from file %s",
			    arg);
		break;
	case 'x': /* SUSv2 */
		set_mode(bsdtar, val);
		break;
	case 'y': /* FreeBSD version of GNU tar */
#if HAVE_LIBBZ2
		if (bsdtar->create_compression != '\0')
			bsdtar_errc(bsdtar, 1, 0,
			    "Can't specify both -%c and -%c", val,
			    bsdtar->create_compression);
		bsdtar->create_compression = val;
#else
		bsdtar_warnc(bsdtar, 0, "-y compression not supported by this version of rpmtar");
		poptPrintUsage(con, stderr, 0);
		exit(EXIT_FAILURE);
#endif
		break;
	case 'Z': /* GNU tar */
		if (bsdtar->create_compression != '\0')
			bsdtar_errc(bsdtar, 1, 0,
			    "Can't specify both -%c and -%c", val,
			    bsdtar->create_compression);
		bsdtar->create_compression = val;
		break;
	case 'z': /* GNU tar, star, many others */
#if HAVE_LIBZ
		if (bsdtar->create_compression != '\0')
			bsdtar_errc(bsdtar, 1, 0,
			    "Can't specify both -%c and -%c", val,
			    bsdtar->create_compression);
		bsdtar->create_compression = val;
#else
		bsdtar_warnc(bsdtar, 0, "-z compression not supported by this version of rpmtar");
		poptPrintUsage(con, stderr, 0);
		exit(EXIT_FAILURE);
#endif
		break;
	case OPTION_USE_COMPRESS_PROGRAM:
		bsdtar->compress_program = arg;
		break;
	default:
		poptPrintUsage(con, stderr, 0);
		exit(EXIT_FAILURE);
		break;
	}
}

static struct poptOption optionsTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        bsdtarArgCallback, 0, (const char *)&bsdtar_storage, NULL },
/*@=type@*/

  { "absolute-paths",'P',	POPT_ARG_NONE,	NULL, 'P',
	N_("Don't strip leading `/'s from paths"), NULL },
  { "append",'r',		POPT_ARG_NONE,	NULL, 'r',
	N_("Append files to the end of an archive"), NULL },
  { "block-size",'b',		POPT_ARG_STRING, NULL, 'b',
	N_("Use # 512-byte records per I/O block"), N_("#") },
  { "bunzip2",'j',	POPT_ARG_NONE|POPT_ARGFLAG_DOC_HIDDEN,	NULL, 'j',
	N_("Uncompress archive using bzip2"), NULL },
  { "bzip",'j',		POPT_ARG_NONE|POPT_ARGFLAG_DOC_HIDDEN,	NULL, 'j',
	N_("Compress archive using bzip2"), NULL },
  { "bzip2",'j',		POPT_ARG_NONE,	NULL, 'j',
	N_("Compress archive using bzip2"), NULL },
  { "cd",'C',			POPT_ARG_STRING,NULL, 'C',
	N_("Change to DIR before processing remaining files"), N_("DIR") },
  { "confirmation",'w',	 POPT_ARG_NONE|POPT_ARGFLAG_DOC_HIDDEN,	NULL, 'w',
	N_("Interactive"), NULL },
  { "create",'c',		POPT_ARG_NONE,	NULL, 'c',
	N_("Create a new archive"), NULL },
  { "dereference",'L', 		POPT_ARG_NONE,  NULL, 'L',
	N_("Follow symlinks; archive/dump the files they point to"), NULL },
  { "directory",'C', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN,	NULL, 'C',
	N_("Change to DIR before processing remaining files"), N_("DIR") },
  { "exclude",'\0',		POPT_ARG_STRING,NULL, OPTION_EXCLUDE,
	N_("Skip files that match PATTERN"), N_("PATTERN") },
  { "exclude-from",'X',		POPT_ARG_STRING,NULL, 'X',
	N_("Exclude patterns listed in FILE"), N_("FILE") },
  { "extract",'x',		POPT_ARG_NONE,	NULL, 'x',
	N_("Extract files from an archive"), NULL },
  { "get",'x',	POPT_ARG_NONE|POPT_ARGFLAG_DOC_HIDDEN,	NULL, 'x',
	N_("Extract files from an archive"), NULL },
  { "fast-read",'\0',		POPT_ARG_NONE,	NULL, OPTION_FAST_READ,
	NULL, NULL },
  { "file",'f',			POPT_ARG_STRING,NULL, 'f',
	N_("Location of archive"), N_("FILE") },
  { "files-from",'T',		POPT_ARG_STRING,NULL, 'T',
	N_("Get names to extract/create from FILE"), N_("FILE") },
  { "format",'\0',		POPT_ARG_STRING,NULL, OPTION_FORMAT,
	N_("Select archive format"), N_("{ustar|pax|cpio|shar}") },
  { "gunzip",'z',	POPT_ARG_NONE|POPT_ARGFLAG_DOC_HIDDEN,	NULL, 'z',
	N_("Uncompress archive using gzip"), NULL },
  { "gzip",'z',			POPT_ARG_NONE,	NULL, 'z',
	N_("Compress archive using gzip"), NULL },
  { NULL,'H',		POPT_ARG_NONE,	NULL, 'H',
	NULL, NULL },
  { NULL,'h',	POPT_ARG_NONE|POPT_ARGFLAG_DOC_HIDDEN,	NULL, 'h',
	N_("Follow symlinks; archive/dump the files they point to"), NULL },
  { NULL,'I',	POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN,	NULL, 'I',
	N_("Get names to extract/create from FILE"), N_("FILE") },
  { "include",'\0',		POPT_ARG_STRING,NULL, OPTION_INCLUDE,
	N_("Add files that match PATTERN"), N_("PATTERN") },
  { "interactive",'w',		POPT_ARG_NONE,	NULL, 'w',
	N_("Interactive"), NULL },
  { "keep-old-files",'k',	POPT_ARG_NONE,	NULL, 'k',
	N_("Keep (don't overwrite) existing files"), NULL },
  { "check-links",'l',		POPT_ARG_NONE,	NULL, 'l',
	N_("Warn if not all links are included"), NULL },
  { "list",'t',			POPT_ARG_NONE,	NULL, 't',
	N_("List the contents of an archive"), NULL },
  { "modification-time",'m',	POPT_ARG_NONE,	NULL, 'm',
	N_("Don't restore modification times"), NULL },
  { "newer",'\0', 		POPT_ARG_STRING,NULL, OPTION_NEWER_CTIME,
	NULL, N_("DATE") },
  { "newer-ctime",'\0', 	POPT_ARG_STRING,NULL, OPTION_NEWER_CTIME,
	NULL, N_("DATE") },
  { "newer-ctime-than",'\0', 	POPT_ARG_STRING,NULL, OPTION_NEWER_CTIME_THAN,
	NULL, N_("DATE") },
  { "newer-mtime",'\0', 	POPT_ARG_STRING,NULL, OPTION_NEWER_MTIME,
	N_("Compare date and time when data changed only"), N_("DATE") },
  { "newer-mtime-than",'\0', 	POPT_ARG_STRING,NULL, OPTION_NEWER_MTIME_THAN,
	NULL, N_("DATE") },
  { "newer-than",'\0', 		POPT_ARG_STRING,NULL, OPTION_NEWER_CTIME_THAN,
	NULL, N_("DATE") },
  { "nodump",'\0',		POPT_ARG_NONE,	NULL, OPTION_NODUMP,
	N_("Skip files marked with nodump flag"), NULL },
  { "norecurse",'n',	 POPT_ARG_NONE,	NULL, 'n',
	N_("Avoid descending automatically in directories"), NULL },
  { "no-recursion",'n',	 POPT_ARG_NONE|POPT_ARGFLAG_DOC_HIDDEN,	NULL, 'n',
	N_("Avoid descending automatically in directories"), NULL },
  { "no-same-owner",'\0', 	POPT_ARG_NONE,	NULL, OPTION_NO_SAME_OWNER,
	N_("Extract files as yourself"), NULL },
  { "no-same-permissions",'\0',	POPT_ARG_NONE,	NULL, OPTION_NO_SAME_PERMISSIONS,
	N_("Apply the user's umask when extracting permissions"), NULL },
  { "null",'\0', 		POPT_ARG_NONE,	NULL, OPTION_NULL,
	N_("-T reads null-terminated names, disable -C"), NULL },
  { NULL,'o',	POPT_ARG_NONE|POPT_ARGFLAG_DOC_HIDDEN,	NULL, 'o',
	N_("Creating: same as --old-archive; Extracting: same as --no-same-owner"), NULL },
  { "one-file-system",'\0', 	POPT_ARG_NONE,	NULL, OPTION_ONE_FILE_SYSTEM,
	N_("Do not cross mount points"), NULL },
  { "posix",'\0', 		POPT_ARG_NONE,	NULL, OPTION_POSIX,
	N_("Same as --format=pax"), NULL },
  { "preserve-permissions",'p',	POPT_ARG_NONE|POPT_ARGFLAG_DOC_HIDDEN, NULL,'p',
	N_("Restore permissions (including ACLs, owner, file flags)"), NULL },
  { "read-full-blocks",'B', 	POPT_ARG_NONE,	NULL, 'B',
	N_("Reblock while reading (for 4.2BSD pipes)"), NULL },
  { "same-permissions",'p',	POPT_ARG_NONE,	NULL, 'p',
	N_("Restore permissions (including ACLs, owner, file flags)"), NULL },
  { "strip-components",'\0', 	POPT_ARG_STRING,NULL, OPTION_STRIP_COMPONENTS,
	N_("Strip # leading components from file names on extraction"), N_("#") },
  { "to-stdout",'O',		POPT_ARG_NONE,	NULL, 'O',
	N_("Write entries to stdout, don't restore to disk"), NULL },
  { "totals",'\0', 		POPT_ARG_NONE,	NULL, OPTION_TOTALS,
	N_("Print total bytes after processing the archive"), NULL },
  { "unlink",'U', 		POPT_ARG_NONE,	NULL, 'U',
	N_("Remove file(s) & re-create; don't overwrite"), NULL },
  { "unlink-first",'U',  POPT_ARG_NONE|POPT_ARGFLAG_DOC_HIDDEN,	NULL, 'U',
	N_("Remove file(s) & re-create; don't overwrite"), NULL },
  { "update",'u',		POPT_ARG_NONE,	NULL, 'u',
	N_("Only append files newer than copy in archive"), NULL },
  { "use-compress-program",'\0',POPT_ARG_STRING,NULL, OPTION_USE_COMPRESS_PROGRAM,
	N_("Filter archive through PROGRAM (must accept -d)"), N_("PROGRAM") },
  { "verbose",'v',		POPT_ARG_NONE,	NULL, 'v',
	N_("Verbose"), NULL },
  { "version",'\0',		POPT_ARG_NONE,	NULL, OPTION_VERSION,
	N_("Print program version"), NULL },
#ifdef	NOTYET	/* XXX TODO: does popt need -W longName=value option? */
  { NULL,'W',		POPT_ARG_STRING,	NULL, 'W',
	NULL, NULL },
#endif
  { NULL,'y',	POPT_ARG_NONE|POPT_ARGFLAG_DOC_HIDDEN,	NULL, 'y',
	N_("Compress archive using bzip2"), NULL },
  { "uncompress",'Z',	POPT_ARG_NONE|POPT_ARGFLAG_DOC_HIDDEN,	NULL, 'Z',
	N_("Filter the archive through compress"), NULL },
  { "compress",'Z',	POPT_ARG_NONE|POPT_ARGFLAG_DOC_HIDDEN,	NULL, 'Z',
	N_("Filter the archive through compress"), NULL },
    POPT_AUTOALIAS
    POPT_AUTOHELP

  { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
	N_("\
  First option must be a mode specifier:\n\
    -c Create  -r Add/Replace  -t List  -u Update  -x Extract\n\
  Common Options:\n\
    -b #     Use # 512-byte records per I/O block\n\
    -f FILE  Location of archive (default " _PATH_DEFTAPE ")\n\
    -v       Verbose\n\
    -w       Interactive\n\
  Create: rpmtar -c [options] [FILE | DIR | @ARCHIVE | -C DIR ]\n\
    FILE, DIR  add these items to archive\n\
    -z, -j  Compress archive with gzip/bzip2\n\
    --format {ustar|pax|cpio|shar}  Select archive format\n\
    --exclude PATTERN  Skip files that match pattern\n\
    -C DIR  Change to DIR before processing remaining files\n\
    @ARCHIVE  Add entries from ARCHIVE to output\n\
  List: rpmtar -t [options] [PATTERN ...]\n\
    PATTERN ...  If specified, list only entries that match\n\
  Extract: rpmtar -x [options] [PATTERN ...]\n\
    PATTERN ...  If specified, extract only entries that match\n\
    -k    Keep (don't overwrite) existing files\n\
    -m    Don't restore modification times\n\
    -O    Write entries to stdout, don't restore to disk\n\
    -p    Restore permissions (including ACLs, owner, file flags)\n\
") , NULL },

    POPT_TABLEEND
};

#ifdef	REFERENCE	/* XXX TODO: does popt need -W longName=value option? */
static const struct option *
tar_opt_W(struct bsdtar *bsdtar, const char * arg)
	/*@globals fileSystem @*/
	/*@modifies bsdtar, fileSystem @*/
{
	const char *p, *q;
	const struct option * option = NULL;
	int opt;
	size_t option_length;

	/* Support long options through -W longopt=value */
	p = arg;
	q = strchr(arg, '=');
	if (q != NULL) {
		option_length = (size_t)(q - p);
		arg = q + 1;
	} else {
		option_length = strlen(p);
		arg = NULL;
	}
	option = tar_longopts;
	while (option->name != NULL &&
	    (strlen(option->name) < option_length ||
	    strncmp(p, option->name, option_length) != 0 )) {
		option++;
	}

	if (option->name != NULL) {
		const struct option * o = option;
		opt = o->val;

		/* If the first match was exact, we're done. */
		if (!strncmp(p, o->name, strlen(o->name))) {
			while (o->name != NULL)
				o++;
		} else {
			/* Check if there's another match. */
			o++;
			while (o->name != NULL &&
			    (strlen(o->name) < option_length ||
			    strncmp(p, o->name, option_length))) {
				o++;
			}
		}
		if (o->name != NULL)
			bsdtar_errc(bsdtar, 1, 0,
			    "Ambiguous option %s "
			    "(matches both %s and %s)",
			    p, option->name, o->name);

		if (option->has_arg == required_argument && arg == NULL)
			bsdtar_errc(bsdtar, 1, 0,
			    "Option \"%s\" requires argument", p);
	} else {
		opt = '?';
		/* TODO: Set up a fake 'struct option' for
		 * error reporting... ? ? ? */
		option = NULL;
	}
	return option;
}
#endif

/*
 * Verify that the mode is correct.
 */
static void
only_mode(struct bsdtar *bsdtar, const char *opt, const char *valid_modes)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
	if (strchr(valid_modes, bsdtar->mode) == NULL)
		bsdtar_errc(bsdtar, 1, 0,
		    "Option %s is not permitted in mode -%c",
		    opt, bsdtar->mode);
}

int
main(int argc, char **argv)
	/*@globals fileSystem @*/
	/*@modifies argc, *argv, fileSystem @*/
{
	struct bsdtar *bsdtar = &bsdtar_storage;
	poptContext optCon;
	char buff[16];
	int rc;

	if (setlocale(LC_ALL, "") == NULL)
		bsdtar_warnc(bsdtar, 0, "Failed to set default locale");

	/* Rewrite traditional-style tar arguments, if used. */
	argv = rewrite_argv(bsdtar, &argc, argv, tar_opts);

	optCon = poptGetContext(bsdtar->progname, argc, (const char **)argv, optionsTable, 0);

	/* Process all options, whine if unknown. */
	while ((rc = poptGetNextOpt(optCon)) > 0) {
		const char * optArg = poptGetOptArg(optCon);
/*@-dependenttrans -modobserver -observertrans @*/
		optArg = _free(optArg);
/*@=dependenttrans =modobserver =observertrans @*/
		switch (rc) {
		default:
			bsdtar_errc(bsdtar, 1, 0, "Option table misconfigured");
			/*@notreached@*/ /*@switchbreak@*/ break;
		}
	}
	bsdtar->argv = (char **) poptGetArgs(optCon);
	bsdtar->argc = argvCount((ARGV_t)bsdtar->argv);

	/*
	 * Sanity-check options.
	 */

	/* A mode is required. */
	if (bsdtar->mode == '\0') {
		poptPrintUsage(optCon, stderr, 0);
		bsdtar_errc(bsdtar, 1, 0,
		    "Must specify one of -c, -r, -t, -u, -x");
	}

	/* Check boolean options only permitted in certain modes. */
	if (bsdtar->option_dont_traverse_mounts)
		only_mode(bsdtar, "--one-file-system", "cru");
	if (bsdtar->option_fast_read)
		only_mode(bsdtar, "--fast-read", "xt");
	if (bsdtar->option_honor_nodump)
		only_mode(bsdtar, "--nodump", "cru");
	if (option_o > 0) {
		switch (bsdtar->mode) {
		case 'c':
			/*
			 * In GNU tar, -o means "old format."  The
			 * "ustar" format is the closest thing
			 * supported by libarchive.
			 */
			bsdtar->create_format = "ustar";
			/* TODO: bsdtar->create_format = "v7"; */
			break;
		case 'x':
			/* POSIX-compatible behavior. */
			bsdtar->option_no_owner = 1;
			bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_OWNER;
			break;
		default:
			only_mode(bsdtar, "-o", "xc");
			break;
		}
	}
	if (bsdtar->option_no_subdirs)
		only_mode(bsdtar, "-n", "cru");
	if (bsdtar->option_stdout)
		only_mode(bsdtar, "-O", "xt");
	if (bsdtar->option_unlink_first)
		only_mode(bsdtar, "-U", "x");
	if (bsdtar->option_warn_links)
		only_mode(bsdtar, "--check-links", "cr");

	/* Check other parameters only permitted in certain modes. */
	if (bsdtar->create_compression == 'Z' && bsdtar->mode == 'c') {
		bsdtar_warnc(bsdtar, 0, ".Z compression not supported");
		poptPrintUsage(optCon, stderr, 0);
		exit(EXIT_FAILURE);
	}
	if (bsdtar->create_compression != '\0') {
		strcpy(buff, "-?");
		buff[1] = bsdtar->create_compression;
		only_mode(bsdtar, buff, "cxt");
	}
	if (bsdtar->create_format != NULL)
		only_mode(bsdtar, "--format", "c");
	if (bsdtar->symlink_mode != '\0') {
		strcpy(buff, "-?");
		buff[1] = bsdtar->symlink_mode;
		only_mode(bsdtar, buff, "cru");
	}
	if (bsdtar->strip_components != 0)
		only_mode(bsdtar, "--strip-components", "xt");

	/* Look up uid of current user. */
	/* Defaults for root user: */
	bsdtar->user_uid = geteuid();
	if (bsdtar->user_uid == 0) {
		/* --same-owner */
		bsdtar->extract_flags |= ARCHIVE_EXTRACT_OWNER;
		/* -p */
		bsdtar->extract_flags |= ARCHIVE_EXTRACT_PERM;
		bsdtar->extract_flags |= ARCHIVE_EXTRACT_ACL;
		bsdtar->extract_flags |= ARCHIVE_EXTRACT_XATTR;
		bsdtar->extract_flags |= ARCHIVE_EXTRACT_FFLAGS;
	}

#if defined(HAVE_NL_LANGINFO) && defined(HAVE_D_MD_ORDER)
	bsdtar->day_first = (*nl_langinfo(D_MD_ORDER) == 'd');
#endif

	/* Default: open tape drive. */
	if (bsdtar->filename == NULL) {
		if ((bsdtar->filename = getenv("TAPE")) == NULL)
			bsdtar->filename = _PATH_DEFTAPE;
	} else
	if (bsdtar->filename[0] == '-' && bsdtar->filename[1] == '\0')
		bsdtar->filename = NULL;

	switch(bsdtar->mode) {
	case 'c':
		tar_mode_c(bsdtar);
		break;
	case 'r':
		tar_mode_r(bsdtar);
		break;
	case 't':
		tar_mode_t(bsdtar);
		break;
	case 'u':
		tar_mode_u(bsdtar);
		break;
	case 'x':
		tar_mode_x(bsdtar);
		break;
	}

	cleanup_exclusions(bsdtar);
	if (bsdtar->return_value != 0)
		bsdtar_warnc(bsdtar, 0,
		    "Error exit delayed from previous errors.");

	optCon = poptFreeContext(optCon);

/*@-globstate@*/
	return (bsdtar->return_value);
/*@=globstate@*/
}

/**
 * \file rpmio/rpmrepo.c
 */

///////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2007 Lasse Collin
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
///////////////////////////////////////////////////////////////////////////////

#include "system.h"

#include <rpmio.h>
#include <poptIO.h>
#include <lzma.h>

#include "debug.h"

static int _debug = 1;

enum tool_mode {
    MODE_COMPRESS,
    MODE_DECOMPRESS,
    MODE_TEST,
    MODE_LIST,
};
/*@unchecked@*/
static enum tool_mode opt_mode = MODE_COMPRESS;

// NOTE: The order of these is significant in suffix.c.
enum format_type {
    FORMAT_AUTO,
    FORMAT_XZ,
    FORMAT_LZMA,
    FORMAT_RAW,
    FORMAT_GZIP,
    FORMAT_BZIP2,
};
/*@unchecked@*/
static enum format_type opt_format = FORMAT_AUTO;

/*@unchecked@*/
static char *opt_suffix = NULL;

/*@unchecked@*/
static const char *opt_files_name = NULL;
/*@unchecked@*/
static char opt_files_split = '\0';
/*@unchecked@*/
static FILE *opt_files_file = NULL;

/*@unchecked@*/
static int opt_stdout;
/*@unchecked@*/
static int opt_force;
/*@unchecked@*/
static int opt_keep_original;
/*@unchecked@*/
static int opt_preserve_name;
/*@unchecked@*/
static int opt_recursive;

/*@unchecked@*/
static lzma_check opt_check = LZMA_CHECK_CRC64;
/*@unchecked@*/
static lzma_filter filters[LZMA_FILTERS_MAX + 1];
/*@unchecked@*/
static size_t filters_count = 0;

/*@unchecked@*/ /*@observer@*/
static const char *stdin_filename = "(stdin)";
/*@unchecked@*/ /*@observer@*/
static const char *stdout_filename = "(stdout)";

/*@unchecked@*/
static int opt_threads;
/*@unchecked@*/
static rpmuint64_t memlimit_encoder = 0;
/*@unchecked@*/
static rpmuint64_t memlimit_decoder = 0;
/*@unchecked@*/
static rpmuint64_t memlimit_custom = 0;

/*@unchecked@*/
static size_t preset_number = 6;
/*@unchecked@*/
static size_t filter_count = 0;

enum {
    OPT_SUBBLOCK = INT_MIN,
    OPT_X86,
    OPT_POWERPC,
    OPT_IA64,
    OPT_ARM,
    OPT_ARMTHUMB,
    OPT_SPARC,
    OPT_DELTA,
    OPT_LZMA1,
    OPT_LZMA2,

    OPT_FILES,
    OPT_FILES0,
};

/**
 */
typedef struct rpmz_s * rpmz;

/**
 */
struct rpmz_s {
    char * b;
    size_t nb;

/*@null@*/
    const char * isuffix;
    enum format_type ifmt;
    FDIO_t idio;
/*@null@*/
    const char * ifn;
    char ifmode[32];
/*@null@*/
    FD_t ifd;

/*@null@*/
    const char * osuffix;
    enum format_type ofmt;
    FDIO_t odio;
/*@null@*/
    const char * ofn;
    char ofmode[32];
/*@null@*/
    FD_t ofd;

/*@null@*/
    const char * suffix;
};

/*@unchecked@*/
static struct rpmz_s __rpmz = {
    .nb		= 16 * BUFSIZ,
    .ifmode	= "rb",
    .ofmode	= "wb",
    .suffix	= "",
};
/*@unchecked@*/
static rpmz _rpmz = &__rpmz;

/*==============================================================*/
static int checkfd(const char * devnull, int fdno, int flags)
	/*@*/
{
    struct stat sb;
    int ret = 0;

    if (fstat(fdno, &sb) == -1 && errno == EBADF)
	ret = (open(devnull, flags) == fdno) ? 1 : 2;
    return ret;
}

static void io_init(void)
	/*@*/
{
    static int _oneshot = 0;

    /* Insure that stdin/stdout/stderr are open, lest stderr end up in rpmdb. */
   if (!_oneshot) {
	static const char _devnull[] = "/dev/null";
#if defined(STDIN_FILENO)
	(void) checkfd(_devnull, STDIN_FILENO, O_RDONLY);
#endif
#if defined(STDOUT_FILENO)
	(void) checkfd(_devnull, STDOUT_FILENO, O_WRONLY);
#endif
#if defined(STDERR_FILENO)
	(void) checkfd(_devnull, STDERR_FILENO, O_WRONLY);
#endif
	_oneshot++;
    }
}

/*==============================================================*/
#define	HAVE_PHYSMEM_SYSCONF	1	/* XXX hotwired for linux */

static rpmuint64_t physmem(void)
	/*@*/
{
    rpmuint64_t ret = 0;

#if defined(HAVE_PHYSMEM_SYSCONF)
    {	const long pagesize = sysconf(_SC_PAGESIZE);
	const long pages = sysconf(_SC_PHYS_PAGES);
	if (pagesize != -1 || pages != -1)
	    ret = (rpmuint64_t)(pagesize) * (rpmuint64_t)(pages);
    }
#elif defined(HAVE_PHYSMEM_SYSCTL)
    {	int name[2] = { CTL_HW, HW_PHYSMEM };
	unsigned long mem;
	size_t mem_ptr_size = sizeof(mem);
	if (!sysctl(name, 2, &mem, &mem_ptr_size, NULL, NULL)) {
	    if (mem_ptr_size != sizeof(mem)) {
		if (mem_ptr_size == sizeof(unsigned int))
		    ret = *(unsigned int *)(&mem);
	    } else {
		ret = mem;
	    }
	}
    }
#endif
    return ret;
}

#define	HAVE_NCPU_SYSCONF	1	/* XXX hotwired for linux */

static void
hw_cores(void)
	/*@globals opt_threads @*/
	/*@modifies opt_threads @*/
{
#if defined(HAVE_NCPU_SYSCONF)
    {	const long cpus = sysconf(_SC_NPROCESSORS_ONLN);
	if (cpus > 0)
	    opt_threads = (size_t)(cpus);
    }

#elif defined(HAVE_NCPU_SYSCTL)
    {	int name[2] = { CTL_HW, HW_NCPU };
	int cpus;
	size_t cpus_size = sizeof(cpus);
	if (!sysctl(name, &cpus, &cpus_size, NULL, NULL)
	 && cpus_size == sizeof(cpus) && cpus > 0)
	    opt_threads = (size_t)(cpus);
    }
#endif

#if defined(_SC_THREAD_THREADS_MAX)
    {	const long threads_max = sysconf(_SC_THREAD_THREADS_MAX);
	if (threads_max > 0 && (int)(threads_max) < opt_threads)
	    opt_threads = (int)(threads_max);
    }
#elif defined(PTHREAD_THREADS_MAX)
    if (opt_threads > PTHREAD_THREADS_MAX)
	opt_threads = PTHREAD_THREADS_MAX;
#endif

    return;
}

static void
hw_memlimit_init(void)
	/*@globals memlimit_decoder, memlimit_encoder @*/
	/*@modifies memlimit_decoder, memlimit_encoder @*/
{
    rpmuint64_t mem = physmem();

    /* Assume 16MB as minimum memory. */
    if (mem == 0)
	    mem = 16 * 1024 * 1024;

    /* Use 90% of RAM when encoding, 33% when decoding. */
    memlimit_encoder = mem - mem / 10;
    memlimit_decoder = mem / 3;
    return;
}

static void
hw_init(void)
{
    hw_memlimit_init();
    hw_cores();
    return;
}

/*==============================================================*/

/*@unchecked@*/ /*@observer@*/
static struct suffixPairs_s {
    enum format_type format;
/*@null@*/
    const char * csuffix;
/*@relnull@*/
    const char * usuffix;
} suffixPairs[] = {
    { FORMAT_XZ,	".xz",	"" },
    { FORMAT_XZ,	".txz",	".tar" },
    { FORMAT_LZMA,	".lzma","" },
    { FORMAT_LZMA,	".tlz",	".tar" },
    { FORMAT_GZIP,	".gz",	"" },
    { FORMAT_GZIP,	".tgz",	".tar" },
    { FORMAT_BZIP2,	".bz2",	"" },
    { FORMAT_RAW,	NULL,	NULL }
};

/**
 * Check string for a suffix.
 * @param fn		string
 * @param suffix	suffix
 * @return		1 if string ends with suffix
 */
static int chkSuffix(const char * fn, const char * suffix)
	/*@*/
{
    size_t flen = strlen(fn);
    size_t slen = strlen(suffix);
    int rc = (flen > slen && !strcmp(fn + flen - slen, suffix));
if (_debug)
fprintf(stderr, "\tchk(%s,%s) ret %d\n", fn, suffix, rc);
    return rc;
}

/**
 * Return (malloc'd) string with new suffix substituted for old.
 * @param fn		string
 * @param old		old suffix
 * @param new		new suffix
 * @return		new string
 */
static char * changeSuffix(const char * fn,
		/*@null@*/ const char * old, 
		/*@null@*/ const char * new)
	/*@*/
{
    size_t nfn = strlen(fn);
    size_t nos = (old ? strlen(old) : 0);
    size_t nns = (new ? strlen(new) : 0);
    char * t = xmalloc(nfn - nos + nns + 1);

    {	char * te = t;

	strncpy(te, fn, (nfn - nos));
	te += (nfn - nos);
	if (new)
	    te = stpcpy(te, new);
	*te = '\0';
    }

    return t;
}

/**
 * Return (malloc'd) uncompressed file name.
 * @param z		rpmz container
 * @return		uncompressed file name
 */
static const char * uncompressedFN(rpmz z)
	/*@*/
{
    struct suffixPairs_s * sp;
    const char * fn = z->ifn;
    const char * t = NULL;

    for (sp = suffixPairs; sp->csuffix != NULL; sp++) {
	if (opt_format != sp->format)
	    continue;
	if (!chkSuffix(fn, sp->csuffix))
	    continue;
	t = changeSuffix(fn, sp->csuffix, sp->usuffix);
	break;
    }

    if (t == NULL)
	t = xstrdup(fn);

if (_debug)
fprintf(stderr, "==> uncompressedFN: %s\n", t);
    return t;
}

/**
 * Return (malloc'd) compressed file name.
 * @param z		rpmz container
 * @return		compressed file name
 */
static const char * compressedFN(rpmz z)
	/*@*/
{
    struct suffixPairs_s * sp;
    const char * fn = z->ifn;
    const char * t = NULL;

    for (sp = suffixPairs; sp->csuffix != NULL; sp++) {
	if (opt_format != sp->format || *sp->usuffix == '\0')
	    continue;
	if (!chkSuffix(fn, sp->usuffix))
	    continue;
	t = changeSuffix(fn, sp->usuffix, sp->csuffix);
	break;
    }

    if (t == NULL)
	t = rpmGetPath(fn, (z->suffix ? z->suffix : z->osuffix), NULL);

if (_debug)
fprintf(stderr, "==>   compressedFN: %s\n", t);
    return t;
}

/*
 * Copy input to output.
 */
static rpmRC rpmzCopy(rpmz z)
{
    for (;;) {
	size_t len = Fread(z->b, 1, z->nb, z->ifd);

	if (Ferror(z->ifd))
	    return RPMRC_FAIL;

	if (len == 0) break;

	if (Fwrite(z->b, 1, len, z->ofd) != len)
	    return RPMRC_FAIL;
    }
    return RPMRC_OK;
}

static rpmRC rpmzFini(rpmz z, rpmRC rc)
{
    int xx;

    if (z->ifd != NULL) {
	xx = Fclose(z->ifd);
	z->ifd = NULL;
    }
    if (z->ofd != NULL) {
	xx = Fclose(z->ofd);
	z->ofd = NULL;
    }

    /* Clean up input/output files as needed. */
    switch (rc) {
    default:
	break;
    case RPMRC_FAIL:
	if (z->ofn != NULL && strcmp(z->ofn, stdout_filename)) {
	    xx = Unlink(z->ofn);
if (_debug)
fprintf(stderr, "==> Unlink(%s) FAIL\n", z->ofn);
	}
	break;
    case RPMRC_OK:
	if (!opt_keep_original && z->ifn != stdin_filename) {
	    xx = Unlink(z->ifn);
if (_debug)
fprintf(stderr, "==> Unlink(%s)\n", z->ifn);
	}
	break;
    }
    z->ofn = _free(z->ofn);

    return rc;
}

/*
 * Compress the given file: create a corresponding .gz file and remove the
 * original.
 */
static rpmRC rpmzCompress(rpmz z)
{
    struct stat sb;
    rpmRC rc = RPMRC_FAIL;

if (_debug)
fprintf(stderr, "==> rpmzCompress(%p) ifn %s ofmode %s\n", z, z->ifn, z->ofmode);
    if (z->ifn == NULL) {
	z->ifn = stdin_filename;
	z->ifd = fdDup(STDIN_FILENO);
    } else {
	if (Stat(z->ifn, &sb) < 0) {
	    fprintf(stderr, "%s: input %s doesn't exist, skipping\n",
			__progname, z->ifn);
	    goto exit;
	}
	if (!S_ISREG(sb.st_mode)) {
	    fprintf(stderr, "%s: input %s is not a regular file, skipping\n",
			__progname, z->ifn);
	    goto exit;
	}
	z->ifd = Fopen(z->ifn, z->ifmode);
    }
    if (z->ifd == NULL || Ferror(z->ifd)) {
	fprintf(stderr, "%s: can't open %s\n", __progname, z->ifn);
	goto exit;
    }

    if (opt_stdout) {
	z->ofn = xstrdup(stdout_filename);
	z->ofd = z->odio->_fdopen(fdDup(STDOUT_FILENO), z->ofmode);
    } else {
	if (z->ifn == stdin_filename)	/* XXX error needed here. */
	    goto exit;
	z->ofn = compressedFN(z);
	if (!opt_force && Stat(z->ofn, &sb) == 0) {
	    fprintf(stderr, "%s: output file %s already exists\n",
			__progname, z->ofn);
	    /* XXX TODO: ok to overwrite(y/N)? */
	    goto exit;
	}
	z->ofd = z->odio->_fopen(z->ofn, z->ofmode);
	fdFree(z->ofd, NULL);		/* XXX adjust refcounts. */
    }
    if (z->ofd == NULL || Ferror(z->ofd)) {
	fprintf(stderr, "%s: can't open %s\n", __progname, z->ofn);
	goto exit;
    }

    rc = rpmzCopy(z);

exit:
    return rpmzFini(z, rc);
}

/*
 * Uncompress the given file and remove the original.
 */
static rpmRC rpmzUncompress(rpmz z)
{
    struct stat sb;
    rpmRC rc = RPMRC_FAIL;

if (_debug)
fprintf(stderr, "==> rpmzUncompress(%p) ifn %s ofmode %s\n", z, z->ifn, z->ofmode);
    if (z->ifn == NULL) {
	z->ifn = stdin_filename;
	z->ifd = z->idio->_fdopen(fdDup(STDIN_FILENO), z->ifmode);
    } else {
	if (Stat(z->ifn, &sb) < 0) {
	    fprintf(stderr, "%s: input %s doesn't exist, skipping\n",
			__progname, z->ifn);
	    goto exit;
	}
	if (!S_ISREG(sb.st_mode)) {
	    fprintf(stderr, "%s: input %s is not a regular file, skipping\n",
			__progname, z->ifn);
	    goto exit;
	}
	z->ifd = z->idio->_fopen(z->ifn, z->ifmode);
	fdFree(z->ifd, NULL);		/* XXX adjust refcounts. */
    }
    if (z->ifd == NULL || Ferror(z->ifd)) {
	fprintf(stderr, "%s: can't open %s\n", __progname, z->ifn);
	goto exit;
    }

    if (opt_stdout)  {
	z->ofn = xstrdup(stdout_filename);
	z->ofd = fdDup(STDOUT_FILENO);
    } else {
	if (z->ifn == stdin_filename)	/* XXX error needed here. */
	    goto exit;
	z->ofn = uncompressedFN(z);
	if (!opt_force && Stat(z->ofn, &sb) == 0) {
	    fprintf(stderr, "%s: output file %s already exists\n",
			__progname, z->ofn);
	    /* XXX TODO: ok to overwrite(y/N)? */
	    goto exit;
	}
	z->ofd = Fopen(z->ofn, z->ofmode);
    }
    if (z->ofd == NULL || Ferror(z->ofd)) {
	fprintf(stderr, "%s: can't Fopen %s\n", __progname, z->ofn);
	goto exit;
    }

    rc = rpmzCopy(z);

exit:
    return rpmzFini(z, rc);
}

/*==============================================================*/

typedef struct {
    const char *name;
    rpmuint64_t id;
} name_id_map;

typedef struct {
    const char *name;
    const name_id_map *map;
    rpmuint64_t min;
    rpmuint64_t max;
} option_map;

/// Parses option=value pairs that are separated with colons, semicolons,
/// or commas: opt=val:opt=val;opt=val,opt=val
///
/// Each option is a string, that is converted to an integer using the
/// index where the option string is in the array.
///
/// Value can be either a number with minimum and maximum value limit, or
/// a string-id map mapping a list of possible string values to integers.
///
/// When parsing both option and value succeed, a filter-specific function
/// is called, which should update the given value to filter-specific
/// options structure.
///
/// \param      str     String containing the options from the command line
/// \param      opts    Filter-specific option map
/// \param      set     Filter-specific function to update filter_options
/// \param      filter_options  Pointer to filter-specific options structure
///
/// \return     Returns only if no errors occur.
///
static void
parse_options(const char *str, const option_map *opts,
		void (*set)(void *filter_options,
			rpmuint32_t key, rpmuint64_t value),
		void *filter_options)
	/*@*/
{
    char * s;
    char *name;

    if (str == NULL || str[0] == '\0')
	return;

    s = xstrdup(str);
    name = s;

    while (1) {
	char *split = strchr(name, ',');
	char *value;
	int found;
	size_t i;

	if (split != NULL)
	    *split = '\0';

	value = strchr(name, '=');
	if (value != NULL)
	    *value++ = '\0';

	if (value == NULL || value[0] == '\0') {
	    fprintf(stderr, _("%s: %s: Options must be `name=value' "
					"pairs separated with commas"),
					__progname, str);
	    /*@-exitarg@*/ exit(2); /*@=exitarg@*/
	}

	// Look for the option name from the option map.
	found = 0;
	for (i = 0; opts[i].name != NULL; ++i) {
	    if (strcmp(name, opts[i].name) != 0)
		continue;

	    if (opts[i].map == NULL) {
		// value is an integer.
		rpmuint64_t v;
#ifdef NOTYET
		v = str_to_uint64(name, value, opts[i].min, opts[i].max);
#else
		v = strtoull(value, NULL, 0);
#endif
		set(filter_options, i, v);
	    } else {
		// value is a string which we should map
		// to an integer.
		size_t j;
		for (j = 0; opts[i].map[j].name != NULL; ++j) {
		    if (strcmp(opts[i].map[j].name, value) == 0)
			break;
		}

		if (opts[i].map[j].name == NULL) {
		    fprintf(stderr, _("%s: %s: Invalid option value"),
			__progname, value);
		    /*@-exitarg@*/ exit(2); /*@=exitarg@*/
		}

		set(filter_options, i, opts[i].map[j].id);
	    }

	    found = 1;
	    break;
	}

	if (!found) {
	    fprintf(stderr, _("%s: %s: Invalid option name"), __progname, name);
	    /*@-exitarg@*/ exit(2); /*@=exitarg@*/
	}

	if (split == NULL)
	    break;

	name = split + 1;
    }

    s = _free(s);
    return;
}

/* ===== Subblock ===== */
enum {
    OPT_SIZE,
    OPT_RLE,
    OPT_ALIGN,
};

static void
set_subblock(void *options, rpmuint32_t key, rpmuint64_t value)
	/*@*/
{
    lzma_options_subblock *opt = options;

    switch (key) {
    case OPT_SIZE:	opt->subblock_data_size = value;	break;
    case OPT_RLE:	opt->rle = value;			break;
    case OPT_ALIGN:	opt->alignment = value;			break;
    }
}

static lzma_options_subblock *
options_subblock(const char *str)
	/*@*/
{
    static const option_map opts[] = {
	{ "size", NULL,   LZMA_SUBBLOCK_DATA_SIZE_MIN,
	                  LZMA_SUBBLOCK_DATA_SIZE_MAX },
	{ "rle",  NULL,   LZMA_SUBBLOCK_RLE_OFF,
	                  LZMA_SUBBLOCK_RLE_MAX },
	{ "align",NULL,   LZMA_SUBBLOCK_ALIGNMENT_MIN,
	                  LZMA_SUBBLOCK_ALIGNMENT_MAX },
	{ NULL,   NULL,   0, 0 }
    };

    lzma_options_subblock *options = xmalloc(sizeof(lzma_options_subblock));
    options->allow_subfilters = 0;
    options->alignment = LZMA_SUBBLOCK_ALIGNMENT_DEFAULT;
    options->subblock_data_size = LZMA_SUBBLOCK_DATA_SIZE_DEFAULT;
    options->rle = LZMA_SUBBLOCK_RLE_OFF;

    parse_options(str, opts, &set_subblock, options);

    return options;
}

/* ===== Delta ===== */

enum {
	OPT_DIST,
};

static void
set_delta(void *options, rpmuint32_t key, rpmuint64_t value)
	/*@*/
{
    lzma_options_delta *opt = options;
    switch (key) {
    case OPT_DIST:	opt->dist = value;		break;
    }
}

static lzma_options_delta *
options_delta(const char *str)
{
    static const option_map opts[] = {
	{ "dist",     NULL,  LZMA_DELTA_DIST_MIN,
	                     LZMA_DELTA_DIST_MAX },
	{ NULL,       NULL,  0, 0 }
    };

    lzma_options_delta *options = xmalloc(sizeof(lzma_options_delta));
    // It's hard to give a useful default for this.
    *options = (lzma_options_delta) {
	// It's hard to give a useful default for this.
	.type = LZMA_DELTA_TYPE_BYTE,
	.dist = LZMA_DELTA_DIST_MIN,
    };

    parse_options(str, opts, &set_delta, options);

    return options;
}

/* ===== LZMA ===== */
enum {
    OPT_PRESET,
    OPT_DICT,
    OPT_LC,
    OPT_LP,
    OPT_PB,
    OPT_MODE,
    OPT_NICE,
    OPT_MF,
    OPT_DEPTH,
};

static void
set_lzma(void *options, rpmuint32_t key, rpmuint64_t value)
	/*@*/
{
    lzma_options_lzma *opt = options;

    switch (key) {
    case OPT_PRESET:
	if (lzma_lzma_preset(options, (uint32_t)(value))) {
	    fprintf(stderr, _("LZMA1/LZMA2 preset %u is not supported"),
					(unsigned int)(value));
	    /*@-exitarg@*/ exit(2); /*@=exitarg@*/
	}
	break;

    case OPT_DICT:	opt->dict_size = value;		break;
    case OPT_LC:	opt->lc = value;		break;
    case OPT_LP:	opt->lp = value;		break;
    case OPT_PB:	opt->pb = value;		break;
    case OPT_MODE:	opt->mode = value;		break;
    case OPT_NICE:	opt->nice_len = value;		break;
    case OPT_MF:	opt->mf = value;		break;
    case OPT_DEPTH:	opt->depth = value;		break;
    }
}

static lzma_options_lzma *
options_lzma(const char *str)
{
    /*@unchecked@*/ /*@observer@*/
    static const name_id_map modes[] = {
	{ "fast",   LZMA_MODE_FAST },
	{ "normal", LZMA_MODE_NORMAL },
	{ NULL,     0 }
    };

    /*@unchecked@*/ /*@observer@*/
    static const name_id_map mfs[] = {
	{ "hc3", LZMA_MF_HC3 },
	{ "hc4", LZMA_MF_HC4 },
	{ "bt2", LZMA_MF_BT2 },
	{ "bt3", LZMA_MF_BT3 },
	{ "bt4", LZMA_MF_BT4 },
	{ NULL,  0 }
    };

    /*@unchecked@*/ /*@observer@*/
    static const option_map opts[] = {
	{ "preset", NULL,   1, 9 },
	{ "dict",   NULL,   LZMA_DICT_SIZE_MIN,
			(UINT32_C(1) << 30) + (UINT32_C(1) << 29) },
	{ "lc",     NULL,   LZMA_LCLP_MIN, LZMA_LCLP_MAX },
	{ "lp",     NULL,   LZMA_LCLP_MIN, LZMA_LCLP_MAX },
	{ "pb",     NULL,   LZMA_PB_MIN, LZMA_PB_MAX },
	{ "mode",   modes,  0, 0 },
	{ "nice",   NULL,   2, 273 },
	{ "mf",     mfs,    0, 0 },
	{ "depth",  NULL,   0, UINT32_MAX },
	{ NULL,     NULL,   0, 0 }
    };

    lzma_options_lzma *options = xmalloc(sizeof(lzma_options_lzma));

    options->dict_size = LZMA_DICT_SIZE_DEFAULT;
    options->preset_dict = NULL;
    options->preset_dict_size = 0,
    options->lc = LZMA_LC_DEFAULT;
    options->lp = LZMA_LP_DEFAULT;
    options->pb = LZMA_PB_DEFAULT;
    options->persistent = 0;
    options->mode = LZMA_MODE_NORMAL;
    options->nice_len = 64;
    options->mf = LZMA_MF_BT4;
    options->depth = 0;

    parse_options(str, opts, &set_lzma, options);

    return options;
}

static void
coder_add_filter(lzma_vli id, void *options)
	/*@globals filter_count, filters @*/
	/*@modifies filter_count, filters @*/
{
    if (filter_count == 4) {
	fprintf(stderr, _("%s: Maximum number of filters is four\n"),
		__progname);
	/*@-exitarg@*/ exit(2); /*@=exitarg@*/
    }

    filters[filters_count].id = id;
    filters[filters_count].options = options;
    ++filters_count;

    return;
}

/**
 */
static void rpmzArgCallback(poptContext con,
		/*@unused@*/ enum poptCallbackReason reason,
		const struct poptOption * opt, const char * arg,
		/*@unused@*/ void * data)
	/*@*/
{
    rpmz z = _rpmz;

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
    case 'M':
    {	char * end = NULL;
	memlimit_custom = (rpmuint64_t) strtoll(arg, &end, 0);
	if (end == NULL || *end == '\0')
	    break;
	if (!strcmp(end, "k") || !strcmp(end, "kB"))
	    memlimit_custom *= 1000;
	else if (!strcmp(end, "M") || !strcmp(end, "MB"))
	    memlimit_custom *= 1000 * 1000;
	else if (!strcmp(end, "G") || !strcmp(end, "GB"))
	    memlimit_custom *= 1000 * 1000 * 1000;
	else if (!strcmp(end, "Ki") || !strcmp(end, "KiB"))
	    memlimit_custom *= 1024;
	else if (!strcmp(end, "Mi") || !strcmp(end, "MiB"))
	    memlimit_custom *= 1024 * 1024;
	else if (!strcmp(end, "Gi") || !strcmp(end, "GiB"))
	    memlimit_custom *= 1024 * 1024 * 1024;
	else {
	    fprintf(stderr, _("%s: Invalid memory scaling suffix \"%s\"\n"),
		__progname, arg);
	    /*@-exitarg@*/ exit(2); /*@=exitarg@*/
	    /*@notreached@*/
	}
    }	break;
    case 'C':
	if (!strcmp(arg, "none"))
	    opt_check = LZMA_CHECK_NONE;
	else if (!strcmp(arg, "crc32"))
	    opt_check = LZMA_CHECK_CRC32;
	else if (!strcmp(arg, "crc64"))
	    opt_check = LZMA_CHECK_CRC64;
	else if (!strcmp(arg, "sha256"))
	    opt_check = LZMA_CHECK_SHA256;
	else {
	    fprintf(stderr, _("%s: Unknown integrity check method \"%s\"\n"),
		__progname, arg);
	    /*@-exitarg@*/ exit(2); /*@=exitarg@*/
	    /*@notreached@*/
	}
	break;
    case 'F':
	if (!strcmp(arg, "auto")) {
	    opt_format = FORMAT_AUTO;
	} else if (!strcmp(arg, "xz")) {
	    z->idio = z->odio = xzdio;
	    z->osuffix = ".xz";
	    opt_format = FORMAT_XZ;
	} else if (!strcmp(arg, "lzma") || !strcmp(arg, "alone")) {
	    z->idio = z->odio = lzdio;
	    z->osuffix = ".lzma";
	    opt_format = FORMAT_LZMA;
	} else if (!strcmp(arg, "raw")) {
	    opt_format = FORMAT_RAW;
	} else if (!strcmp(arg, "gzip") || !strcmp(arg, "gz")) {
	    z->idio = z->odio = gzdio;
	    z->osuffix = ".gz";
	    opt_format = FORMAT_GZIP;
	} else if (!strcmp(arg, "bzip2") || !strcmp(arg, "bz2")) {
	    z->idio = z->odio = bzdio;
	    z->osuffix = ".bz2";
	    opt_format = FORMAT_BZIP2;
	} else {
	    fprintf(stderr, _("%s: Unknown file format type \"%s\"\n"),
		__progname, arg);
	    /*@-exitarg@*/ exit(2); /*@=exitarg@*/
	    /*@notreached@*/
	}
	break;
    case OPT_SUBBLOCK:
	coder_add_filter(LZMA_FILTER_SUBBLOCK, options_subblock(optarg));
	break;
    case OPT_X86:
	coder_add_filter(LZMA_FILTER_X86, NULL);
	break;
    case OPT_POWERPC:
	coder_add_filter(LZMA_FILTER_POWERPC, NULL);
	break;
    case OPT_IA64:
	coder_add_filter(LZMA_FILTER_IA64, NULL);
	break;
    case OPT_ARM:
	coder_add_filter(LZMA_FILTER_ARM, NULL);
	break;
    case OPT_ARMTHUMB:
	coder_add_filter(LZMA_FILTER_ARMTHUMB, NULL);
	break;
    case OPT_SPARC:
	coder_add_filter(LZMA_FILTER_SPARC, NULL);
	break;
    case OPT_DELTA:
	coder_add_filter(LZMA_FILTER_DELTA, options_delta(optarg));
	break;
    case OPT_LZMA1:
	coder_add_filter(LZMA_FILTER_LZMA1, options_lzma(optarg));
	break;
    case OPT_LZMA2:
	coder_add_filter(LZMA_FILTER_LZMA2, options_lzma(optarg));
	break;
    case OPT_FILES:
	opt_files_split = '\n';
	/*@fallthrough@*/
    case OPT_FILES0:
	if (arg == NULL || !strcmp(arg, "-")) {
	    opt_files_name = xstrdup(stdin_filename);
	    opt_files_file = stdin;
	} else {
	    opt_files_name = xstrdup(arg);
	    opt_files_file = fopen(opt_files_name,
			opt->val == OPT_FILES ? "r" : "rb");
	    if (opt_files_file == NULL || ferror(opt_files_file)) {
		fprintf(stderr, _("%s: %s: %s\n"),
			__progname, opt_files_name, strerror(errno));
		/*@-exitarg@*/ exit(2); /*@=exitarg@*/
		/*@notreached@*/
	    }
	}
	break;

    default:
	fprintf(stderr, _("%s: Unknown option -%c\n"), __progname, opt->val);
	poptPrintUsage(con, stderr, 0);
	/*@-exitarg@*/ exit(2); /*@=exitarg@*/
	/*@notreached@*/ break;
    }
}

/*==============================================================*/

/*@unchecked@*/ /*@observer@*/
static struct poptOption optionsTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
	rpmzArgCallback, 0, NULL, NULL },
/*@=type@*/

  /* ===== Operation modes */
  { "compress", 'z', POPT_ARG_VAL,		&opt_mode, MODE_COMPRESS,
	N_("force compression"), NULL },
  { "decompress", 'd', POPT_ARG_VAL,		&opt_mode, MODE_DECOMPRESS,
	N_("force decompression"), NULL },
  { "uncompress", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &opt_mode, MODE_DECOMPRESS,

	N_("force decompression"), NULL },
  { "test", 't', POPT_ARG_VAL,			&opt_mode,  MODE_TEST,
	N_("test compressed file integrity"), NULL },
  { "list", 'l', POPT_ARG_VAL,			&opt_mode,  MODE_LIST,
	N_("list block sizes, total sizes, and possible metadata"), NULL },
  { "info", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&opt_mode,  MODE_LIST,
	N_("list block sizes, total sizes, and possible metadata"), NULL },

  /* ===== Operation modifiers */
  { "keep", 'k', POPT_ARG_VAL,			&opt_keep_original, 1,
	N_("keep (don't delete) input files"), NULL },
  { "force", 'f', POPT_ARG_VAL,			&opt_force,  1,
	N_("force overwrite of output file and (de)compress links"), NULL },
  { "stdout", 'c', POPT_ARG_VAL,		&opt_stdout,  1,
	N_("write to standard output and don't delete input files"), NULL },
  { "to-stdout", 'c', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &opt_stdout,  1,
	N_("write to standard output and don't delete input files"), NULL },
  { "suffix", 'S', POPT_ARG_STRING,		&opt_suffix, 0,
	N_("use suffix `.SUF' on compressed files instead"), N_(".SUF") },
  /* XXX unimplemented */
  { "recursive", 'r', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &opt_recursive, 1,
	N_("?recursive?"), NULL },
  { "files", '\0', POPT_ARG_STRING|POPT_ARGFLAG_OPTIONAL, NULL, OPT_FILES,
	N_("read filenames to process from FILE"), N_("FILE") },
  { "files0", '\0', POPT_ARG_STRING|POPT_ARGFLAG_OPTIONAL, NULL, OPT_FILES0,
	N_("like --files but use the nul byte as terminator"), N_("FILE") },

  /* ===== Basic compression settings */
  { "format", 'F', POPT_ARG_STRING,		NULL, 'F',
	N_("file FORMAT {auto|xz|lzma|alone|gzip|gz|raw}"), N_("FORMAT") },
  { "check", 'C', POPT_ARG_STRING,		NULL, 'C',
	N_("integrity check METHOD {none|crc32|crc64|sha256}"), N_("METHOD") },
  { "memory", 'M', POPT_ARG_STRING,		NULL,  'M',
	N_("use roughly NUM Mbytes of memory at maximum"), N_("NUM") },
  { "threads", 'T', POPT_ARG_INT,		&opt_threads, 0,
	N_("use maximum of NUM (de)compression threads"), N_("NUM") },

  /* ===== Compression options */
#ifdef	NOTYET
  { "extreme", 'e', POPT_ARG_VAL,			&preset_number,  1,
	N_("extreme compression"), NULL },
#endif
  { "fast", '\0', POPT_ARG_VAL,				&preset_number,  1,
	N_("fast compression"), NULL },
  { "best", '\0', POPT_ARG_VAL,				&preset_number,  9,
	N_("best compression"), NULL },

  { NULL, '1', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&preset_number,  1,
	NULL, NULL },
  { NULL, '2', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&preset_number,  2,
	NULL, NULL },
  { NULL, '3', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&preset_number,  3,
	NULL, NULL },
  { NULL, '4', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&preset_number,  4,
	NULL, NULL },
  { NULL, '5', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&preset_number,  5,
	NULL, NULL },
  { NULL, '6', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&preset_number,  6,
	NULL, NULL },
  { NULL, '7', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&preset_number,  7,
	NULL, NULL },
  { NULL, '8', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&preset_number,  8,
	NULL, NULL },
  { NULL, '9', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&preset_number,  9,
	NULL, NULL },

  /* ===== Custom filters */
#ifdef	REFERENCE
"  --lzma=[OPTS]       LZMA filter; OPTS is a comma-separated list of zero or\n"
"                      more of the following options (valid values; default):\n"
"                        dict=NUM   dictionary size in bytes (1 - 1Gi; 8Mi)\n"
"                        lc=NUM     number of literal context bits (0-8; 3)\n"
"                        lp=NUM     number of literal position bits (0-4; 0)\n"
"                        pb=NUM     number of position bits (0-4; 2)\n"
"                        mode=MODE  compression mode (`fast' or `best'; `best')\n"
"                        nice_len=NUM     number of fast bytes (5-273; 128)\n"
"                        mf=NAME    match finder (hc3, hc4, bt2, bt3, bt4; bt4)\n"
"                        depth=NUM    match finder cycles; 0=automatic (default)\n"
#endif
  { "lzma1", '\0', POPT_ARG_STRING|POPT_ARGFLAG_OPTIONAL,	NULL, OPT_LZMA1,
	N_("set lzma1 filter"), N_("OPTS") },
  { "lzma2", '\0', POPT_ARG_STRING|POPT_ARGFLAG_OPTIONAL,	NULL, OPT_LZMA2,
	N_("set lzma2 filter"), N_("OPTS") },

  { "x86", '\0', 0,				NULL, OPT_X86,
	N_("ix86 filter (sometimes called BCJ filter)"), NULL },
  { "bcj", '\0', POPT_ARGFLAG_DOC_HIDDEN,	NULL, OPT_X86,
	N_("x86 filter (sometimes called BCJ filter)"), NULL },
  { "powerpc", '\0', 0,				NULL, OPT_POWERPC,
	N_("PowerPC (big endian) filter"), NULL },
  { "ppc", '\0', POPT_ARGFLAG_DOC_HIDDEN,	NULL, OPT_POWERPC,
	N_("PowerPC (big endian) filter"), NULL },
  { "ia64", '\0', 0,				NULL, OPT_IA64,
	N_("IA64 (Itanium) filter"), NULL },
  { "itanium", '\0', POPT_ARGFLAG_DOC_HIDDEN,	NULL, OPT_IA64,
	N_("IA64 (Itanium) filter"), NULL },
  { "arm", '\0', 0,				NULL, OPT_ARM,
	N_("ARM filter"), NULL },
  { "armthumb", '\0', 0,			NULL, OPT_ARMTHUMB,
	N_("ARM-Thumb filter"), NULL },
  { "sparc", '\0', 0,				NULL, OPT_SPARC,
	N_("SPARC filter"), NULL },

#ifdef	REFERENCE
"  --delta=[OPTS]      Delta filter; valid OPTS (valid values; default):\n"
"                        dist=NUM  Distance between bytes being\n"
"                                      subtracted from each other (1-256; 1)\n"
#endif
  { "delta", '\0', POPT_ARG_STRING|POPT_ARGFLAG_OPTIONAL,	NULL, OPT_DELTA,
	N_("set delta filter"), N_("OPTS") },

#ifdef	REFERENCE
"  --subblock=[OPTS]   Subblock filter; valid OPTS (valid values; default):\n"
"                        size=NUM    number of bytes of data per subblock\n"
"                                    (1 - 256Mi; 4Ki)\n"
"                        rle=NUM     run-length encoder chunk size (0-256; 0)\n"
#endif
  { "subblock", '\0', POPT_ARG_STRING|POPT_ARGFLAG_OPTIONAL,	NULL, OPT_SUBBLOCK,
	N_("set subblock filter"), N_("OPTS") },

  /* ===== Metadata options */
  { "name", 'N', POPT_ARG_VAL,			&opt_preserve_name, 1,
	N_("save or restore the original filename and time stamp"), NULL },
  { "no-name", 'n', POPT_ARG_VAL,		&opt_preserve_name, 0,
	N_("do not save or restore filename and time stamp (default)"), NULL },
#ifdef	NOTYET
  { "sign", 'S', POPT_ARG_STRING,		NULL, 0,
	N_("sign the data with GnuPG when compressing, or verify the signature when decompressing"), N_("PUBKEY") },
#endif

  /* ===== Other options */
#ifdef	NOTYET
  { "quiet", 'q',		POPT_ARG_VAL,       NULL,  'q',
	N_("suppress warnings; specify twice to suppress errors too"), NULL },
  { "verbose", 'v',		POPT_ARG_VAL,       NULL,  'v',
	N_("be verbose; specify twice for even more verbose"), NULL },
  { "help", 'h',		POPT_ARG_VAL,       NULL,  'h',
	N_("display this help and exit"), NULL },
  { "long-help", 'h',		POPT_ARG_VAL,       NULL,  'H',
	N_("display this help and exit"), NULL },
  { "version", 'V',		POPT_ARG_VAL,       NULL,  'V',
	N_("display version and license information and exit"), NULL },
#endif

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"),
	NULL },

  POPT_AUTOALIAS
  POPT_AUTOHELP

  { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
	N_("\
Usage: rpmz [OPTION]... [FILE]...\n\
Compress or decompress FILEs.\n\
\n\
"), NULL },

  POPT_TABLEEND

};

int
main(int argc, char *argv[])
	/*@globals __assert_program_name,
		rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies __assert_program_name, _rpmrepo,
		rpmGlobalMacroContext, fileSystem, internalState @*/
{
    poptContext optCon;
    rpmz z = _rpmz;
    const char ** av;
    int ac;
    int rc = 1;		/* assume failure. */
    int xx;
    int i;

/*@-observertrans -readonlytrans @*/
    __progname = "rpmz";
/*@=observertrans =readonlytrans @*/

    /* XXX TODO: Set modes and format based on argv[0]. */

    z->b = xmalloc(z->nb);
#ifndef	DYING
    z->idio = z->odio = gzdio;
    z->osuffix = ".gz";
    opt_format = FORMAT_GZIP;
#endif

    /* Make sure that stdin/stdout/stderr are open. */
    io_init();

    /* Set hardware specific parameters. */
    hw_init();

    /* XXX TODO: Parse environment options. */

    /* Parse CLI options. */
    optCon = rpmioInit(argc, argv, optionsTable);
    av = poptGetArgs(optCon);
    ac = argvCount(av);

    /* With no arguments, act as a stdin/stdout filter. */
    if (av == NULL || av[0] == NULL)
	ac++;

    if (opt_mode == MODE_COMPRESS) {
	xx = snprintf(z->ofmode, sizeof(z->ofmode)-1, "wb%u",
		(unsigned)(preset_number % 10));
    }
    z->suffix = opt_suffix;

    /* Process arguments. */
    for (i = 0; i < ac; i++) {
	rpmRC frc = RPMRC_OK;

	/* Use NULL for stdin. */
	z->ifn = (av && strcmp(av[i], "-") ? av[i] : NULL);

	switch (opt_mode) {
	case MODE_COMPRESS:
	    frc = rpmzCompress(z);
	    break;
	case MODE_DECOMPRESS:
	    frc = rpmzUncompress(z);
	    break;
	case MODE_TEST:
	    break;
	case MODE_LIST:
	    break;
	}

	if (frc != RPMRC_OK)
	    goto exit;
    }

    rc = 0;

exit:
    z->b = _free(z->b);
    optCon = rpmioFini(optCon);

    return rc;
}

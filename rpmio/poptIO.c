/** \ingroup rpmio
 * \file rpmio/poptIO.c
 *  Popt tables for all rpmio-only executables.
 */

#include "system.h"

#include <poptIO.h>

#define _RPMPGP_INTERNAL
#if defined(WITH_BEECRYPT)
#define _RPMBC_INTERNAL
#include <rpmbc.h>
#endif
#if defined(WITH_GCRYPT)
#define _RPMGC_INTERNAL
#include <rpmgc.h>
#endif
#if defined(WITH_NSS)
#define _RPMNSS_INTERNAL
#include <rpmnss.h>
#endif
#if defined(WITH_SSL)
#define _RPMSSL_INTERNAL
#include <rpmssl.h>
#endif

#include "debug.h"

const char *__progname;

#if !defined(POPT_ARGFLAG_TOGGLE)	/* XXX compat with popt < 1.15 */
#define	POPT_ARGFLAG_TOGGLE	0
#endif
#define POPT_SHOWVERSION	-999
#define POPT_UNDEFINE		-994
#define	POPT_CRYPTO		-993

/*@unchecked@*/
int __debug = 0;

/*@-exportheadervar@*/
/*@-redecl@*/
/*@unchecked@*/
extern int _ar_debug;
/*@unchecked@*/
extern int _av_debug;
/*@unchecked@*/
extern int _cpio_debug;
/*@unchecked@*/
extern int _dav_debug;
/*@unchecked@*/
extern int _ftp_debug;
/*@unchecked@*/
extern int _fts_debug;
/*@unchecked@*/
extern int _ht_debug;
/*@unchecked@*/
extern int _iosm_debug;
/*@unchecked@*/
extern int noLibio;
/*@unchecked@*/
extern int _pgp_debug;
/*@unchecked@*/
extern int _rpmio_debug;
/*@unchecked@*/
extern int _rpmiob_debug;
/*@unchecked@*/
extern int _rpmsq_debug;
/*@unchecked@*/
extern int _rpmtcl_debug;
/*@unchecked@*/
extern int _rpmzq_debug;
/*@unchecked@*/
extern int _tar_debug;
/*@unchecked@*/
extern int _xar_debug;
/*@=redecl@*/
/*@=exportheadervar@*/

/*@unchecked@*/ /*@null@*/
const char * rpmioPipeOutput = NULL;

/*@unchecked@*/
const char * rpmioRootDir = "/";

/*@observer@*/ /*@unchecked@*/
const char *rpmioEVR = VERSION;

/*@unchecked@*/
static int rpmioInitialized = -1;

#ifdef	NOTYET
#ifdef WITH_LUA
/*@unchecked@*/
extern const char *rpmluaFiles;
#endif

/*@unchecked@*/
static char *rpmpoptfiles = RPMPOPTFILES;
#endif

pgpHashAlgo rpmioDigestHashAlgo = -1;

/**
 * Digest options using popt.
 */
struct poptOption rpmioDigestPoptTable[] = {
 { "md2", '\0', POPT_ARG_VAL, 	&rpmioDigestHashAlgo, PGPHASHALGO_MD2,
	N_("MD2 digest (RFC-1319)"), NULL },
 { "md4", '\0', POPT_ARG_VAL, 	&rpmioDigestHashAlgo, PGPHASHALGO_MD4,
	N_("MD4 digest"), NULL },
 { "md5", '\0', POPT_ARG_VAL, 	&rpmioDigestHashAlgo, PGPHASHALGO_MD5,
	N_("MD5 digest (RFC-1321)"), NULL },
 { "sha1",'\0', POPT_ARG_VAL, 	&rpmioDigestHashAlgo, PGPHASHALGO_SHA1,
	N_("SHA-1 digest (FIPS-180-1)"), NULL },
 { "sha224",'\0', POPT_ARG_VAL, &rpmioDigestHashAlgo, PGPHASHALGO_SHA224,
	N_("SHA-224 digest (FIPS-180-2)"), NULL },
 { "sha256",'\0', POPT_ARG_VAL, &rpmioDigestHashAlgo, PGPHASHALGO_SHA256,
	N_("SHA-256 digest (FIPS-180-2)"), NULL },
 { "sha384",'\0', POPT_ARG_VAL, &rpmioDigestHashAlgo, PGPHASHALGO_SHA384,
	N_("SHA-384 digest (FIPS-180-2)"), NULL },
 { "sha512",'\0', POPT_ARG_VAL, &rpmioDigestHashAlgo, PGPHASHALGO_SHA512,
	N_("SHA-512 digest (FIPS-180-2)"), NULL },
 { "salsa10",'\0', POPT_ARG_VAL,&rpmioDigestHashAlgo, PGPHASHALGO_SALSA10,
	N_("SALSA-10 hash"), NULL },
 { "salsa20",'\0', POPT_ARG_VAL,&rpmioDigestHashAlgo, PGPHASHALGO_SALSA20,
	N_("SALSA-20 hash"), NULL },
 { "rmd128",'\0', POPT_ARG_VAL,	&rpmioDigestHashAlgo, PGPHASHALGO_RIPEMD128,
	N_("RIPEMD-128 digest"), NULL },
 { "rmd160",'\0', POPT_ARG_VAL,	&rpmioDigestHashAlgo, PGPHASHALGO_RIPEMD160,
	N_("RIPEMD-160 digest"), NULL },
 { "rmd256",'\0', POPT_ARG_VAL,	&rpmioDigestHashAlgo, PGPHASHALGO_RIPEMD256,
	N_("RIPEMD-256 digest"), NULL },
 { "rmd320",'\0', POPT_ARG_VAL,	&rpmioDigestHashAlgo, PGPHASHALGO_RIPEMD320,
	N_("RIPEMD-320 digest"), NULL },
 { "tiger",'\0', POPT_ARG_VAL,	&rpmioDigestHashAlgo, PGPHASHALGO_TIGER192,
	N_("TIGER digest"), NULL },
 { "crc32",'\0', POPT_ARG_VAL,	&rpmioDigestHashAlgo, PGPHASHALGO_CRC32,
	N_("CRC-32 checksum"), NULL },
 { "crc64",'\0', POPT_ARG_VAL,	&rpmioDigestHashAlgo, PGPHASHALGO_CRC64,
	N_("CRC-64 checksum"), NULL },
 { "adler32",'\0', POPT_ARG_VAL,&rpmioDigestHashAlgo, PGPHASHALGO_ADLER32,
	N_("ADLER-32 checksum"), NULL },
 { "jlu32",'\0', POPT_ARG_VAL,	&rpmioDigestHashAlgo, PGPHASHALGO_JLU32,
	N_("Lookup3 hash"), NULL },
 { "nodigest",'\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &rpmioDigestHashAlgo, PGPHASHALGO_NONE,
	N_("No hash algorithm"), NULL },
 { "alldigests",'\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &rpmioDigestHashAlgo, 256,
	N_("All hash algorithm(s)"), NULL },
    POPT_TABLEEND
};

/**
 * Display rpm version.
 */
static void printVersion(FILE * fp)
	/*@globals rpmioEVR, fileSystem @*/
	/*@modifies *fp, fileSystem @*/
{
    fprintf(fp, _("%s (" RPM_NAME ") %s\n"), __progname, rpmioEVR);
}

void rpmioConfigured(void)
	/*@globals rpmioInitialized @*/
	/*@modifies rpmioInitialized @*/
{

    if (rpmioInitialized < 0) {
	/* XXX TODO: add initialization side-effects. */
	rpmioInitialized = 0;
    }
    if (rpmioInitialized)
	exit(EXIT_FAILURE);
}

/**
 */
static void rpmioAllArgCallback(poptContext con,
                /*@unused@*/ enum poptCallbackReason reason,
                const struct poptOption * opt, const char * arg,
                /*@unused@*/ const void * data)
	/*@globals pgpImplVecs,
 		rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@modifies con, pgpImplVecs,
 		rpmGlobalMacroContext,
		fileSystem, internalState @*/
{

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
    case 'q':
	rpmSetVerbosity(RPMLOG_WARNING);
	break;
    case 'v':
	rpmIncreaseVerbosity();
	break;
#ifdef	NOTYET
    case 'D':
    {	char *s, *t;
	/* XXX Convert '-' in macro name to underscore, skip leading %. */
	s = t = xstrdup(arg);
	while (*t && !xisspace((int)*t)) {
	    if (*t == '-') *t = '_';
	    t++;
	}
	t = s;
	if (*t == '%') t++;
	rpmioConfigured();
/*@-type@*/
	/* XXX adding macro to global context isn't Right Thing Todo. */
	(void) rpmDefineMacro(NULL, t, RMIL_CMDLINE);
	(void) rpmDefineMacro(rpmCLIMacroContext, t, RMIL_CMDLINE);
/*@=type@*/
	s = _free(s);
    }	break;
    case POPT_UNDEFINE:
    {	char *s, *t;
	/* XXX Convert '-' in macro name to underscore, skip leading %. */
	s = t = xstrdup(arg);
	while (*t && !xisspace((int)*t)) {
	    if (*t == '-') *t = '_';
	    t++;
	}
	t = s;
	if (*t == '%') t++;
/*@-type@*/
	rpmioConfigured();
	(void) rpmUndefineMacro(NULL, t);
	(void) rpmUndefineMacro(rpmCLIMacroContext, t);
/*@=type@*/
	s = _free(s);
    }	break;
    case 'E':
	rpmioConfigured();
	{   const char *val = rpmExpand(arg, NULL);
            size_t val_len;
            val_len = strlen(val);
            if (val[val_len - 1] == '\n')
                fwrite(val, val_len, 1, stdout);
            else
		fprintf(stdout, "%s\n", val);
	    val = _free(val);
	}
	break;
#endif	/* NOTYET */
    case POPT_CRYPTO:
	{   const char *val;
#ifdef	NOTYET
	    rpmioConfigured();
	    val = rpmExpand(arg, NULL);
#else
	    val = xstrdup(arg);
#endif	/* NOTYET */
	    if (!xstrcasecmp(val, "beecrypt") || !xstrcasecmp(val, "bc")) {
#if defined(WITH_BEECRYPT)
		pgpImplVecs = &rpmbcImplVecs;
#else
                rpmlog(RPMLOG_ERR, "BeeCrypt (\"beecrypt\") based cryptography implementation not available\n");
                exit(EXIT_FAILURE);
#endif
            }
	    else if (!xstrcasecmp(val, "gcrypt") || !xstrcasecmp(val, "gc")) {
#if defined(WITH_GCRYPT)
		pgpImplVecs = &rpmgcImplVecs;
#else
                rpmlog(RPMLOG_ERR, "GCrypt (\"gcrypt\") based cryptography implementation not available\n");
                exit(EXIT_FAILURE);
#endif
            }
	    else if (!xstrcasecmp(val, "NSS")) {
#if defined(WITH_NSS)
		pgpImplVecs = &rpmnssImplVecs;
#else
                rpmlog(RPMLOG_ERR, "Mozilla NSS (\"nss\") based cryptography implementation not available\n");
                exit(EXIT_FAILURE);
#endif
            }
	    else if (!xstrcasecmp(val, "OpenSSL") || !xstrcasecmp(val, "ssl")) {
#if defined(WITH_SSL)
		pgpImplVecs = &rpmsslImplVecs;
#else
                rpmlog(RPMLOG_ERR, "OpenSSL (\"openssl\") based cryptography implementation not available\n");
                exit(EXIT_FAILURE);
#endif
            }
            else {
                rpmlog(RPMLOG_ERR, "cryptography implementation \"%s\" not known\n", val);
                exit(EXIT_FAILURE);
            }
	    val = _free(val);
	}
	break;
    case POPT_SHOWVERSION:
	printVersion(stdout);
/*@i@*/	con = rpmioFini(con);
	exit(EXIT_SUCCESS);
	/*@notreached@*/ break;
    }
}

/*@unchecked@*/
int rpmioFtsOpts = 0;

/*@unchecked@*/
struct poptOption rpmioFtsPoptTable[] = {
 { "comfollow", '\0', POPT_BIT_SET,	&rpmioFtsOpts, FTS_COMFOLLOW,
	N_("FTS_COMFOLLOW: follow command line symlinks"), NULL },
 { "logical", '\0', POPT_BIT_SET,	&rpmioFtsOpts, FTS_LOGICAL,
	N_("FTS_LOGICAL: logical walk"), NULL },
 { "nochdir", '\0', POPT_BIT_SET,	&rpmioFtsOpts, FTS_NOCHDIR,
	N_("FTS_NOCHDIR: don't change directories"), NULL },
 { "nostat", '\0', POPT_BIT_SET,	&rpmioFtsOpts, FTS_NOSTAT,
	N_("FTS_NOSTAT: don't get stat info"), NULL },
 { "physical", '\0', POPT_BIT_SET,	&rpmioFtsOpts, FTS_PHYSICAL,
	N_("FTS_PHYSICAL: physical walk"), NULL },
 { "seedot", '\0', POPT_BIT_SET,	&rpmioFtsOpts, FTS_SEEDOT,
	N_("FTS_SEEDOT: return dot and dot-dot"), NULL },
 { "xdev", '\0', POPT_BIT_SET,		&rpmioFtsOpts, FTS_XDEV,
	N_("FTS_XDEV: don't cross devices"), NULL },
 { "whiteout", '\0', POPT_BIT_SET,	&rpmioFtsOpts, FTS_WHITEOUT,
	N_("FTS_WHITEOUT: return whiteout information"), NULL },
   POPT_TABLEEND
};

/*@-bitwisesigned -compmempass @*/
/*@unchecked@*/
struct poptOption rpmioAllPoptTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmioAllArgCallback, 0, NULL, NULL },
/*@=type@*/

 { "debug", 'd', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &__debug, -1,
	N_("Debug generic operations"), NULL },

#ifdef	NOTYET
 { "define", 'D', POPT_ARG_STRING, NULL, (int)'D',
	N_("Define MACRO with value EXPR"),
	N_("'MACRO EXPR'") },
 { "undefine", '\0', POPT_ARG_STRING, NULL, POPT_UNDEFINE,
	N_("Undefine MACRO"),
	N_("'MACRO'") },
 { "eval", 'E', POPT_ARG_STRING, NULL, (int)'E',
	N_("Print macro expansion of EXPR"),
	N_("'EXPR'") },

 { "macros", '\0', POPT_ARG_STRING, &rpmMacrofiles, 0,
	N_("Read <FILE:...> instead of default file(s)"),
	N_("<FILE:...>") },
#ifdef WITH_LUA
 { "rpmlua", '\0', POPT_ARG_STRING, &rpmluaFiles, 0,
	N_("Read <FILE:...> instead of default RPM Lua file(s)"),
	N_("<FILE:...>") },
#endif
 { "rpmpopt", '\0', POPT_ARG_STRING, NULL, 0,
	N_("Read <FILE:...> instead of default POPT file(s)"),
	N_("<FILE:...>") },
#endif	/* NOTYET */

 { "pipe", '\0', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN, &rpmioPipeOutput, 0,
	N_("Send stdout to CMD"),
	N_("CMD") },
 { "root", 'r', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT, &rpmioRootDir, 0,
	N_("Use ROOT as top level directory"),
	N_("ROOT") },

 { "quiet", '\0', 0, NULL, (int)'q',
	N_("Provide less detailed output"), NULL},
 { "verbose", 'v', 0, NULL, (int)'v',
	N_("Provide more detailed output"), NULL},
 { "version", '\0', 0, NULL, POPT_SHOWVERSION,
	N_("Print the version"), NULL },

#if defined(HAVE_LIBIO_H) && defined(_G_IO_IO_FILE_VERSION)
 { "nolibio", '\0', POPT_ARG_VAL|POPT_ARGFLAG_TOGGLE|POPT_ARGFLAG_DOC_HIDDEN, &noLibio, -1,
	N_("Disable use of libio(3) API"), NULL},
#endif

 { "usecrypto",'\0', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN, NULL, POPT_CRYPTO,
        N_("Select cryptography implementation"),
	N_("CRYPTO") },

 { "ardebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_ar_debug, -1,
	N_("Debug ar archives"), NULL},
 { "avdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_av_debug, -1,
	N_("Debug argv collections"), NULL},
 { "cpiodebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_cpio_debug, -1,
	N_("Debug cpio archives"), NULL},
 { "davdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_dav_debug, -1,
	N_("Debug WebDAV data stream"), NULL},
 { "ftpdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_ftp_debug, -1,
	N_("Debug FTP/HTTP data stream"), NULL},
 { "ftsdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_fts_debug, -1,
	N_("Debug Fts(3) traverse"), NULL},
 { "htdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_ht_debug, -1,
	N_("Debug hash tables"), NULL},
 { "iosmdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_iosm_debug, -1,
	N_("Debug I/O state machine"), NULL},
 { "miredebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_mire_debug, -1,
	N_("Debug miRE patterns"), NULL},
 { "pgpdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_pgp_debug, -1,
	N_("Debug PGP usage"), NULL},
 { "rpmiodebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmio_debug, -1,
	N_("Debug rpmio I/O"), NULL},
 { "rpmiobdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmiob_debug, -1,
	N_("Debug rpmio I/O buffers"), NULL},
 { "rpmmgdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmmg_debug, -1,
	N_("Debug rpmmg magic"), NULL},
 { "rpmsqdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmsq_debug, -1,
	N_("Debug rpmsq Signal Queue"), NULL},
#ifdef WITH_TCL
 { "rpmtcldebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmtcl_debug, -1,
	N_("Debug rpmtcl TCL interpreter"), NULL},
#endif
 { "rpmzqdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmzq_debug, -1,
	N_("Debug rpmzq Job Queuing"), NULL},
 { "xardebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_xar_debug, -1,
	N_("Debug xar archives"), NULL},
 { "tardebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_tar_debug, -1,
	N_("Debug tar archives"), NULL},
 { "stats", '\0', POPT_ARG_VAL,				&_rpmsw_stats, -1,
	N_("Display operation statistics"), NULL},
 { "urldebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_url_debug, -1,
	N_("Debug URL cache handling"), NULL},

   POPT_TABLEEND
};
/*@=bitwisesigned =compmempass @*/

poptContext
rpmioFini(poptContext optCon)
{
    /* XXX this should be done in the rpmioClean() wrapper. */
    /* keeps memory leak checkers quiet */
    rpmFreeMacros(NULL);
/*@i@*/	rpmFreeMacros(rpmCLIMacroContext);

    rpmioClean();

    optCon = poptFreeContext(optCon);

#if defined(HAVE_MCHECK_H) && defined(HAVE_MTRACE)
    /*@-noeffect@*/
    muntrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
    /*@=noeffect@*/
#endif

    return NULL;
}

static inline int checkfd(const char * devnull, int fdno, int flags)
	/*@*/
{
    struct stat sb;
    int ret = 0;

    if (fstat(fdno, &sb) == -1 && errno == EBADF)
	ret = (open(devnull, flags) == fdno) ? 1 : 2;
    return ret;
}

/*@-globstate@*/
poptContext
rpmioInit(int argc, char *const argv[], struct poptOption * optionsTable)
{
    poptContext optCon;
    int rc;
#ifdef	NOTYET
    int i;
#endif

#if defined(HAVE_MCHECK_H) && defined(HAVE_MTRACE)
    /*@-noeffect@*/
    mtrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
    /*@=noeffect@*/
#endif
/*@-globs -mods@*/
    setprogname(argv[0]);       /* Retrofit glibc __progname */

    /* XXX glibc churn sanity */
    if (__progname == NULL) {
	if ((__progname = strrchr(argv[0], '/')) != NULL) __progname++;
	else __progname = argv[0];
    }
/*@=globs =mods@*/

    /* Insure that stdin/stdout/stderr are open, lest stderr end up in rpmdb. */
   {	static const char _devnull[] = "/dev/null";
#if defined(STDIN_FILENO)
	(void) checkfd(_devnull, STDIN_FILENO, O_RDONLY);
#endif
#if defined(STDOUT_FILENO)
	(void) checkfd(_devnull, STDOUT_FILENO, O_WRONLY);
#endif
#if defined(STDERR_FILENO)
	(void) checkfd(_devnull, STDERR_FILENO, O_WRONLY);
#endif
   }

#if defined(ENABLE_NLS) && !defined(__LCLINT__)
    (void) setlocale(LC_ALL, "" );
    (void) bindtextdomain(PACKAGE, LOCALEDIR);
    (void) textdomain(PACKAGE);
#endif

    rpmSetVerbosity(RPMLOG_NOTICE);

    if (optionsTable == NULL) {
	/* Read rpm configuration (if not already read). */
	rpmioConfigured();
	return NULL;
    }

#ifdef	NOTYET
    /* read all RPM POPT configuration files */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--rpmpopt") == 0 && i+1 < argc) {
            rpmpoptfiles = argv[i+1];
            break;
        }
        else if (strncmp(argv[i], "--rpmpopt=", 10) == 0) {
            rpmpoptfiles = argv[i]+10;
            break;
        }
    }
#endif	/* NOTYET */

/*@-nullpass -temptrans@*/
    optCon = poptGetContext(__progname, argc, (const char **)argv, optionsTable, 0);
/*@=nullpass =temptrans@*/

#ifdef	NOTYET
#if !defined(POPT_ERROR_BADCONFIG)	/* XXX popt-1.15- retrofit */
  { char * path_buf = xstrdup(rpmpoptfiles);
    char *path;
    char *path_next;

    for (path = path_buf; path != NULL && *path != '\0'; path = path_next) {
        const char **av;
        int ac, i;

        /* locate start of next path element */
        path_next = strchr(path, ':');
        if (path_next != NULL && *path_next == ':')
            *path_next++ = '\0';
        else
            path_next = path + strlen(path);

        /* glob-expand the path element */
        ac = 0;
        av = NULL;
        if ((i = rpmGlob(path, &ac, &av)) != 0)
            continue;

        /* work-off each resulting file from the path element */
        for (i = 0; i < ac; i++) {
            const char *fn = av[i];
#if defined(RPM_VENDOR_OPENPKG) /* security-sanity-check-rpmpopt-and-rpmmacros */
            if (fn[0] == '@' /* attention */) {
                fn++;
                if (!rpmSecuritySaneFile(fn)) {
                    rpmlog(RPMLOG_WARNING, "existing POPT configuration file \"%s\" considered INSECURE -- not loaded\n", fn);
                    continue;
                }
            }
#endif
            (void)poptReadConfigFile(optCon, fn);
            av[i] = _free(av[i]);
        }
        av = _free(av);
    }
    path_buf = _free(path_buf);
  }
#else
    /* XXX FIXME: better error message is needed. */
    if ((xx = poptReadConfigFiles(optCon, rpmpoptfiles)) != 0)
       rpmlog(RPMLOG_WARNING, "existing POPT configuration file \"%s\" considered INSECURE -- not loaded\n", rpmpoptfiles);
#endif

    /* read standard POPT configuration files */
    /* XXX FIXME: the 2nd arg useEnv flag is UNUSED. */
    (void) poptReadDefaultConfig(optCon, 1);

    poptSetExecPath(optCon, USRLIBRPM, 1);
#endif	/* NOTYET */

    /* Process all options, whine if unknown. */
    while ((rc = poptGetNextOpt(optCon)) > 0) {
	const char * optArg = poptGetOptArg(optCon);
/*@-dependenttrans -modobserver -observertrans @*/
	optArg = _free(optArg);
/*@=dependenttrans =modobserver =observertrans @*/
	switch (rc) {
	default:
/*@-nullpass@*/
	    fprintf(stderr, _("%s: option table misconfigured (%d)\n"),
		__progname, rc);
/*@=nullpass@*/
	    exit(EXIT_FAILURE);

	    /*@notreached@*/ /*@switchbreak@*/ break;
        }
    }

    if (rc < -1) {
/*@-nullpass@*/
	fprintf(stderr, "%s: %s: %s\n", __progname,
		poptBadOption(optCon, POPT_BADOPTION_NOALIAS),
		poptStrerror(rc));
/*@=nullpass@*/
	exit(EXIT_FAILURE);
    }

    /* Read rpm configuration (if not already read). */
    rpmioConfigured();

    if (__debug) {
	rpmIncreaseVerbosity();
	rpmIncreaseVerbosity();
    }

    return optCon;
}
/*@=globstate@*/

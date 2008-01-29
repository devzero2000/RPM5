/** \ingroup rpmcli
 * \file lib/poptALL.c
 *  Popt tables for all rpm modes.
 */

#include "system.h"
const char *__progname;

#include <rpmio.h>
#include <rpmcli.h>
#include <fs.h>			/* XXX rpmFreeFilesystems() */
#include <rpmns.h>		/* XXX rpmnsClean() */
#include <fts.h>
#include <mire.h>

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

#define POPT_SHOWVERSION	-999
#define POPT_SHOWRC		-998
#define POPT_QUERYTAGS		-997
#define POPT_PREDEFINE		-996
#ifdef  DEAD	/* XXX remember the previous definition however. */
#define POPT_RCFILE		-995
#endif
#define POPT_UNDEFINE		-994
#define	POPT_CRYPTO		-993

/*@access headerTagIndices @*/		/* XXX rpmcliFini */
/*@access headerTagTableEntry @*/	/* XXX rpmcliFini */

/*@unchecked@*/
static int _debug = 0;

/*@-exportheadervar@*/
/*@unchecked@*/
extern int _rpmds_nopromote;

/*@unchecked@*/
extern int _cpio_debug;

/*@unchecked@*/
extern int _fps_debug;

/*@unchecked@*/
extern int _fsm_debug;

/*@unchecked@*/
extern int _fsm_threads;

/*@unchecked@*/
extern int _hdr_debug;

/*@unchecked@*/
extern int _mire_debug;

/*@unchecked@*/
extern int _print_pkts;

/*@unchecked@*/
extern int _psm_debug;

/*@unchecked@*/
extern int _psm_threads;

/*@unchecked@*/
extern int _rpmal_debug;

/*@unchecked@*/
extern int _rpmdb_debug;

/*@unchecked@*/
extern int _rpmds_debug;

/* XXX avoid -lrpmbuild linkage. */
/*@unchecked@*/
       int _rpmfc_debug;

/*@unchecked@*/
extern int _rpmfi_debug;

/*@unchecked@*/
extern int _rpmgi_debug;

/*@unchecked@*/
extern int _rpmmg_debug;

/*@unchecked@*/
extern int _rpmps_debug;

/*@unchecked@*/
extern int _rpmsq_debug;

/*@unchecked@*/
extern int _rpmsx_debug;

/*@unchecked@*/
extern int _rpmte_debug;

/*@unchecked@*/
extern int _rpmts_debug;

/*@unchecked@*/
extern int _rpmwf_debug;

/*@unchecked@*/
extern int _xar_debug;

/*@unchecked@*/
extern int _rpmts_macros;

/*@unchecked@*/
extern int _rpmts_stats;

/*@unchecked@*/
extern int _hdr_stats;

/*@unchecked@*/
extern int _tar_debug;

/*@unchecked@*/
extern int noLibio;
/*@=exportheadervar@*/

/*@unchecked@*/ /*@null@*/
const char * rpmcliPipeOutput = NULL;

/*@unchecked@*/
const char * rpmcliRootDir = "/";

/*@unchecked@*/
rpmQueryFlags rpmcliQueryFlags;

/*@unchecked@*/ /*@null@*/
const char * rpmcliTargets = NULL;

/*@-exportheadervar@*/
/*@unchecked@*/
extern int _ftp_debug;
/*@unchecked@*/
extern int _av_debug;
/*@unchecked@*/
extern int _dav_debug;

/*@unchecked@*/
extern int noLibio;

/*@unchecked@*/
extern int _rpmio_debug;
/*@=exportheadervar@*/

/*@unchecked@*/
static int rpmcliInitialized = -1;

#if defined(RPM_VENDOR_OPENPKG) /* support-rpmlua-option */
#ifdef WITH_LUA
/*@unchecked@*/
extern const char *rpmluaFiles;
#endif
#endif

#if defined(RPM_VENDOR_OPENPKG) /* support-rpmpopt-option */
/*@unchecked@*/
static char *rpmpoptfiles = RPMPOPTFILES;
#endif

/**
 * Display rpm version.
 */
static void printVersion(FILE * fp)
	/*@globals rpmEVR, fileSystem @*/
	/*@modifies *fp, fileSystem @*/
{
    fprintf(fp, _("%s (" RPM_NAME ") %s\n"), __progname, rpmEVR);
    if (rpmIsVerbose())
	fprintf(fp, "rpmlib 0x%08x,0x%08x,0x%08x\n",
	    rpmlibVersion(), rpmlibTimestamp(), rpmlibVendor());
}

void rpmcliConfigured(void)
	/*@globals rpmcliInitialized, rpmCLIMacroContext, rpmGlobalMacroContext,
		h_errno, fileSystem, internalState @*/
	/*@modifies rpmcliInitialized, rpmCLIMacroContext, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{

    if (rpmcliInitialized < 0) {
	char * t = NULL;
	if (rpmcliTargets != NULL) {
	    char *te;
	    t = xstrdup(rpmcliTargets);
	    if ((te = strchr(t, ',')) != NULL)
		*te = '\0';
	}
	rpmcliInitialized = rpmReadConfigFiles(NULL, t);
	t = _free(t);
    }
    if (rpmcliInitialized)
	exit(EXIT_FAILURE);
}

/**
 */
static void rpmcliAllArgCallback(poptContext con,
                /*@unused@*/ enum poptCallbackReason reason,
                const struct poptOption * opt, const char * arg,
                /*@unused@*/ const void * data)
	/*@globals rpmcliTargets, rpmcliQueryFlags, rpmCLIMacroContext,
		rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies con, rpmcliTargets, rpmcliQueryFlags, rpmCLIMacroContext,
		rpmGlobalMacroContext, fileSystem, internalState @*/
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
    case POPT_PREDEFINE:
	(void) rpmDefineMacro(NULL, arg, RMIL_CMDLINE);
	break;
    case 'D':
    {	char *s, *t;
	/* XXX Convert '-' in macro name to underscore, skip leading %. */
	s = t = xstrdup(arg);
	while (*t && !xisspace(*t)) {
	    if (*t == '-') *t = '_';
	    t++;
	}
	t = s;
	if (*t == '%') t++;
	rpmcliConfigured();
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
	while (*t && !xisspace(*t)) {
	    if (*t == '-') *t = '_';
	    t++;
	}
	t = s;
	if (*t == '%') t++;
/*@-type@*/
	rpmcliConfigured();
	(void) rpmUndefineMacro(NULL, t);
	(void) rpmUndefineMacro(rpmCLIMacroContext, t);
/*@=type@*/
	s = _free(s);
    }	break;
    case POPT_CRYPTO:
	rpmcliConfigured();
	{   const char *val = rpmExpand(arg, NULL);
#if defined(WITH_BEECRYPT)
	    if (!xstrcasecmp(val, "beecrypt") || !xstrcasecmp(val, "bc"))
		pgpImplVecs = &rpmbcImplVecs;
#endif
#if defined(WITH_GCRYPT)
	    if (!xstrcasecmp(val, "gcrypt") || !xstrcasecmp(val, "gc"))
		pgpImplVecs = &rpmgcImplVecs;
#endif
#if defined(WITH_NSS)
	    if (!xstrcasecmp(val, "NSS"))
		pgpImplVecs = &rpmnssImplVecs;
#endif
#if defined(WITH_SSL)
	    if (!xstrcasecmp(val, "OpenSSL") || !xstrcasecmp(val, "ssl"))
		pgpImplVecs = &rpmsslImplVecs;
#endif
	    val = _free(val);
	}
	break;
    case 'E':
	rpmcliConfigured();
	{   const char *val = rpmExpand(arg, NULL);
#if defined(RPM_VENDOR_OPENPKG) /* no-extra-terminating-newline-on-eval */
            size_t val_len;
            val_len = strlen(val);
            if (val[val_len - 1] == '\n')
                fwrite(val, val_len, 1, stdout);
            else
#endif
	    fprintf(stdout, "%s\n", val);
	    val = _free(val);
	}
	break;
    case POPT_SHOWVERSION:
	printVersion(stdout);
/*@i@*/	con = rpmcliFini(con);
	exit(EXIT_SUCCESS);
	/*@notreached@*/ break;
    case POPT_SHOWRC:
	rpmcliConfigured();
	(void) rpmShowRC(stdout);
/*@i@*/	con = rpmcliFini(con);
	exit(EXIT_SUCCESS);
	/*@notreached@*/ break;
    case POPT_QUERYTAGS:
	rpmDisplayQueryTags(NULL, NULL, NULL);
/*@i@*/	con = rpmcliFini(con);
	exit(EXIT_SUCCESS);
	/*@notreached@*/ break;
#if defined(POPT_RCFILE)
    case POPT_RCFILE:		/* XXX FIXME: noop for now */
	break;
#endif
    case RPMCLI_POPT_NODIGEST:
	rpmcliQueryFlags |= VERIFY_DIGEST;
	pgpDigVSFlags |= _RPMVSF_NODIGESTS;
	break;

    case RPMCLI_POPT_NOSIGNATURE:
	rpmcliQueryFlags |= VERIFY_SIGNATURE;
	pgpDigVSFlags |= _RPMVSF_NOSIGNATURES;
	break;

    case RPMCLI_POPT_NOHDRCHK:
	rpmcliQueryFlags |= VERIFY_HDRCHK;
	pgpDigVSFlags |= RPMVSF_NOHDRCHK;
	break;

    case RPMCLI_POPT_TARGETPLATFORM:
	if (rpmcliTargets == NULL)
	    rpmcliTargets = xstrdup(arg);
	else {
/*@-modobserver @*/
	    char * t = (char *) rpmcliTargets;
	    size_t nb = strlen(t) + (sizeof(",")-1) + strlen(arg) + 1;
/*@i@*/	    t = xrealloc(t, nb);
	    (void) stpcpy( stpcpy(t, ","), arg);
	    rpmcliTargets = t;
/*@=modobserver @*/
	}
	break;
    }
}

/*@unchecked@*/
int ftsOpts = 0;

/*@unchecked@*/
struct poptOption rpmcliFtsPoptTable[] = {
 { "comfollow", '\0', POPT_BIT_SET,	&ftsOpts, FTS_COMFOLLOW,
	N_("FTS_COMFOLLOW: follow command line symlinks"), NULL },
 { "logical", '\0', POPT_BIT_SET,	&ftsOpts, FTS_LOGICAL,
	N_("FTS_LOGICAL: logical walk"), NULL },
 { "nochdir", '\0', POPT_BIT_SET,	&ftsOpts, FTS_NOCHDIR,
	N_("FTS_NOCHDIR: don't change directories"), NULL },
 { "nostat", '\0', POPT_BIT_SET,	&ftsOpts, FTS_NOSTAT,
	N_("FTS_NOSTAT: don't get stat info"), NULL },
 { "physical", '\0', POPT_BIT_SET,	&ftsOpts, FTS_PHYSICAL,
	N_("FTS_PHYSICAL: physical walk"), NULL },
 { "seedot", '\0', POPT_BIT_SET,	&ftsOpts, FTS_SEEDOT,
	N_("FTS_SEEDOT: return dot and dot-dot"), NULL },
 { "xdev", '\0', POPT_BIT_SET,		&ftsOpts, FTS_XDEV,
	N_("FTS_XDEV: don't cross devices"), NULL },
 { "whiteout", '\0', POPT_BIT_SET,	&ftsOpts, FTS_WHITEOUT,
	N_("FTS_WHITEOUT: return whiteout information"), NULL },
   POPT_TABLEEND
};

/*@unchecked@*/
int global_depFlags;

/*@unchecked@*/
struct poptOption rpmcliDepFlagsPoptTable[] = {
 { "aid", '\0', POPT_BIT_SET, &global_depFlags, RPMDEPS_FLAG_ADDINDEPS,
	N_("add suggested packages to transaction"), NULL },
 { "anaconda", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
 	&global_depFlags, RPMDEPS_FLAG_ANACONDA|RPMDEPS_FLAG_DEPLOOPS,
	N_("use anaconda \"presentation order\""), NULL},
 { "deploops", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
 	&global_depFlags, RPMDEPS_FLAG_DEPLOOPS,
	N_("print dependency loops as warning"), NULL},
 { "nosuggest", '\0', POPT_BIT_SET,
	&global_depFlags, RPMDEPS_FLAG_NOSUGGEST,
	N_("do not suggest missing dependency resolution(s)"), NULL},
 { "noconflicts", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&global_depFlags, RPMDEPS_FLAG_NOCONFLICTS,
	N_("do not check added package conflicts"), NULL},
 { "nolinktos", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&global_depFlags, RPMDEPS_FLAG_NOLINKTOS,
	N_("ignore added package requires on symlink targets"), NULL},
 { "noobsoletes", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&global_depFlags, RPMDEPS_FLAG_NOOBSOLETES,
	N_("ignore added package obsoletes"), NULL},
 { "noparentdirs", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&global_depFlags, RPMDEPS_FLAG_NOPARENTDIRS,
	N_("ignore added package requires on file parent directory"), NULL},
 { "norequires", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&global_depFlags, RPMDEPS_FLAG_NOREQUIRES,
	N_("do not check added package requires"), NULL},
 { "noupgrade", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,
	&global_depFlags, RPMDEPS_FLAG_NOUPGRADE,
	N_("ignore added package upgrades"), NULL},
   POPT_TABLEEND
};

/*@-bitwisesigned -compmempass @*/
/*@unchecked@*/
struct poptOption rpmcliAllPoptTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmcliAllArgCallback, 0, NULL, NULL },
/*@=type@*/

 { "debug", 'd', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_debug, -1,
        NULL, NULL },

 { "predefine", '\0', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN, NULL, POPT_PREDEFINE,
	N_("predefine MACRO with value EXPR"),
	N_("'MACRO EXPR'") },
 { "define", 'D', POPT_ARG_STRING, NULL, 'D',
	N_("define MACRO with value EXPR"),
	N_("'MACRO EXPR'") },
 { "undefine", '\0', POPT_ARG_STRING, NULL, POPT_UNDEFINE,
	N_("undefine MACRO"),
	N_("'MACRO'") },
 { "eval", 'E', POPT_ARG_STRING, NULL, 'E',
	N_("print macro expansion of EXPR"),
	N_("'EXPR'") },
 { "macros", '\0', POPT_ARG_STRING, &rpmMacrofiles, 0,
	N_("read <FILE:...> instead of default file(s)"),
	N_("<FILE:...>") },
#if defined(RPM_VENDOR_OPENPKG) /* support-rpmlua-option */
#ifdef WITH_LUA
 { "rpmlua", '\0', POPT_ARG_STRING, &rpmluaFiles, 0,
	N_("read <FILE:...> instead of default RPM Lua file(s)"),
	N_("<FILE:...>") },
#endif
#endif
#if defined(RPM_VENDOR_OPENPKG) /* support-rpmpopt-option */
 { "rpmpopt", '\0', POPT_ARG_STRING, NULL, 0,
	N_("read <FILE:...> instead of default POPT file(s)"),
	N_("<FILE:...>") },
#endif

 { "target", '\0', POPT_ARG_STRING, NULL,  RPMCLI_POPT_TARGETPLATFORM,
        N_("specify target platform"), N_("CPU-VENDOR-OS") },

 { "nodigest", '\0', 0, NULL, RPMCLI_POPT_NODIGEST,
        N_("don't verify package digest(s)"), NULL },
 { "nohdrchk", '\0', POPT_ARGFLAG_DOC_HIDDEN, NULL, RPMCLI_POPT_NOHDRCHK,
        N_("don't verify database header(s) when retrieved"), NULL },
#if defined(HAVE_LIBIO_H) && defined(_G_IO_IO_FILE_VERSION)
 { "nolibio", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &noLibio, 1,
	N_("disable use of libio(3) API"), NULL},
#endif
 { "nosignature", '\0', 0, NULL, RPMCLI_POPT_NOSIGNATURE,
        N_("don't verify package signature(s)"), NULL },

 { "pipe", '\0', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN, &rpmcliPipeOutput, 0,
	N_("send stdout to CMD"),
	N_("CMD") },
 { "root", 'r', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT, &rpmcliRootDir, 0,
	N_("use ROOT as top level directory"),
	N_("ROOT") },

 { "querytags", '\0', 0, NULL, POPT_QUERYTAGS,
        N_("display known query tags"), NULL },
 { "quiet", '\0', 0, NULL, 'q',
	N_("provide less detailed output"), NULL},
 { "showrc", '\0', 0, NULL, POPT_SHOWRC,
	N_("display final rpmrc and macro configuration"), NULL },
 { "verbose", 'v', 0, NULL, 'v',
	N_("provide more detailed output"), NULL},
 { "version", '\0', 0, NULL, POPT_SHOWVERSION,
	N_("print the version of rpm being used"), NULL },

#if defined(HAVE_LIBIO_H) && defined(_G_IO_IO_FILE_VERSION)
 { "nolibio", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &noLibio, 1,
       N_("disable use of libio(3) API"), NULL},
#endif

 { "promoteepoch", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmds_nopromote, 0,
	NULL, NULL},

 { "usecrypto",'\0', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN, NULL, POPT_CRYPTO,
        N_("select cryptography implementation"),
	N_("CRYPTO") },

 { "avdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_av_debug, -1,
	N_("debug argv collections"), NULL},
 { "cpiodebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_cpio_debug, -1,
	N_("debug cpio payloads"), NULL},
 { "davdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_dav_debug, -1,
	N_("debug WebDAV data stream"), NULL},
 { "fpsdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_fps_debug, -1,
	NULL, NULL},
 { "fsmdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_fsm_debug, -1,
	N_("debug payload file state machine"), NULL},
 { "fsmthreads", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_fsm_threads, -1,
	N_("use threads for file state machine"), NULL},
 { "ftpdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_ftp_debug, -1,
	N_("debug FTP/HTTP data stream"), NULL},
 { "hdrdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_hdr_debug, -1,
	NULL, NULL},
 { "macrosused", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmts_macros, -1,
	NULL, NULL},
 { "miredebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_mire_debug, -1,
	NULL, NULL},
#ifdef	DYING
 { "poptdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_popt_debug, -1,
	N_("debug option/argument processing"), NULL},
#endif
 { "prtpkts", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_print_pkts, -1,
	NULL, NULL},
 { "psmdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_psm_debug, -1,
	N_("debug package state machine"), NULL},
 { "psmthreads", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_psm_threads, -1,
	N_("use threads for package state machine"), NULL},
 { "rpmaldebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmal_debug, -1,
	NULL, NULL},
 { "rpmdbdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmdb_debug, -1,
	NULL, NULL},
 { "rpmdsdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmds_debug, -1,
	NULL, NULL},
 { "rpmfcdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmfc_debug, -1,
	NULL, NULL},
 { "rpmfidebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmfi_debug, -1,
	NULL, NULL},
 { "rpmgidebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmgi_debug, -1,
	NULL, NULL},
 { "rpmiodebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmio_debug, -1,
	N_("debug rpmio I/O"), NULL},
 { "rpmmgdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmmg_debug, -1,
	NULL, NULL},
 { "rpmnsdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmns_debug, -1,
	NULL, NULL},
 { "rpmpsdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmps_debug, -1,
	NULL, NULL},
 { "rpmsqdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmsq_debug, -1,
	NULL, NULL},
 { "rpmsxdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmsx_debug, -1,
	NULL, NULL},
 { "rpmtedebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmte_debug, -1,
	NULL, NULL},
 { "rpmtsdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmts_debug, -1,
	NULL, NULL},
 { "rpmwfdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmwf_debug, -1,
	NULL, NULL},
 { "xardebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_xar_debug, -1,
	NULL, NULL},
 { "tardebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_tar_debug, -1,
	N_("debug tar payloads"), NULL},
 { "stats", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmts_stats, -1,
	NULL, NULL},
 { "urldebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_url_debug, -1,
	N_("debug URL cache handling"), NULL},

   POPT_TABLEEND
};
/*@=bitwisesigned =compmempass @*/

poptContext
rpmcliFini(poptContext optCon)
{
    /* XXX this should be done in the rpmioClean() wrapper. */
    /* keeps memory leak checkers quiet */
    rpmFreeMacros(NULL);
/*@i@*/	rpmFreeMacros(rpmCLIMacroContext);

    rpmioClean();

    rpmnsClean();

    rpmFreeRpmrc();

    rpmFreeFilesystems();
/*@i@*/	rpmcliTargets = _free(rpmcliTargets);

    keyids = _free(keyids);

    tagClean(NULL);	/* Free header tag indices. */

    optCon = poptFreeContext(optCon);

#if defined(HAVE_MCHECK_H) && defined(HAVE_MTRACE)
    /*@-noeffect@*/
    muntrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
    /*@=noeffect@*/
#endif

    return NULL;
}

static inline int checkfd(const char * devnull, int fdno, int flags)
{
    struct stat sb;
    int ret = 0;

    if (fstat(fdno, &sb) == -1 && errno == EBADF)
	ret = (open(devnull, flags) == fdno) ? 1 : 2;
    return ret;
}

/*@-globstate@*/
poptContext
rpmcliInit(int argc, char *const argv[], struct poptOption * optionsTable)
{
    poptContext optCon;
    char *path_buf, *path, *path_next;
    int rc;
#if defined(RPM_VENDOR_OPENPKG) /* support-rpmpopt-option */
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
	rpmcliConfigured();
	return NULL;
    }

/*@-nullpass -temptrans@*/
    optCon = poptGetContext(__progname, argc, (const char **)argv, optionsTable, 0);
/*@=nullpass =temptrans@*/

    /* read all RPM POPT configuration files */
#if defined(RPM_VENDOR_OPENPKG) /* support-rpmpopt-option */
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
    path_buf = xstrdup(rpmpoptfiles);
#else
    path_buf = xstrdup(RPMPOPTFILES);
#endif
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

    /* read standard POPT configuration files */
    (void) poptReadDefaultConfig(optCon, 1);

    poptSetExecPath(optCon, USRLIBRPM, 1);

    /* Process all options, whine if unknown. */
    while ((rc = poptGetNextOpt(optCon)) > 0) {
	const char * optArg = poptGetOptArg(optCon);
	optArg = _free(optArg);
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
    rpmcliConfigured();

    if (_debug) {
	rpmIncreaseVerbosity();
	rpmIncreaseVerbosity();
    }

    /* Initialize header stat collection. */
/*@-mods@*/
    _hdr_stats = _rpmts_stats;
/*@=mods@*/

    return optCon;
}
/*@=globstate@*/

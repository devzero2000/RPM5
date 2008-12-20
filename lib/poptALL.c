/** \ingroup rpmcli
 * \file lib/poptALL.c
 *  Popt tables for all rpm modes.
 */

#include "system.h"
extern const char *__progname;

#if defined(RPM_VENDOR_WINDRIVER)
const char *__usrlibrpm = USRLIBRPM;
const char *__etcrpm = SYSCONFIGDIR;
#endif
#if defined(ENABLE_NLS) && !defined(__LCLINT__)
const char *__localedir = LOCALEDIR;
#endif

#include <rpmio.h>
#include <rpmiotypes.h>
#include <fts.h>
#include <mire.h>
#include <poptIO.h>

#include <rpmtag.h>
#include <rpmtypes.h>
#include <rpmrc.h>
#include <rpmversion.h>
#include <rpmcli.h>

#include <rpmns.h>		/* XXX rpmnsClean() */

#include <fs.h>			/* XXX rpmFreeFilesystems() */

#include "debug.h"

/*@unchecked@*/ /*@only@*/ /*@null@*/
extern unsigned int * keyids;

#define POPT_SHOWVERSION	-999
#define POPT_SHOWRC		-998
#define POPT_QUERYTAGS		-997
#define POPT_PREDEFINE		-996
#define POPT_UNDEFINE		-994

/*@access headerTagIndices @*/		/* XXX rpmcliFini */
/*@access headerTagTableEntry @*/	/* XXX rpmcliFini */

/*@unchecked@*/
static int _debug = 0;

/*@-exportheadervar@*/
/*@unchecked@*/
extern int _rpmds_nopromote;

/*@unchecked@*/
extern int _fps_debug;

/*@unchecked@*/
extern int _fsm_debug;

/*@unchecked@*/
extern int _fsm_threads;

/*@unchecked@*/
extern int _hdr_debug;
/*@unchecked@*/
extern int _hdr_fastdatalength;
/*@unchecked@*/
extern int _hdr_lazytagswab;

/*@unchecked@*/
extern int _pkgio_debug;

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
extern int _rpmts_macros;

/*@unchecked@*/
extern int _rpmts_stats;

/*@unchecked@*/
extern int _hdr_stats;

/*@unchecked@*/
rpmQueryFlags rpmcliQueryFlags;

/*@unchecked@*/ /*@null@*/
const char * rpmcliTargets = NULL;

/*@unchecked@*/
static int rpmcliInitialized = -1;

#ifdef WITH_LUA
/*@unchecked@*/
extern const char *rpmluaFiles;
#endif

/*@-readonlytrans@*/	/* argv loading prevents observer, xstrdup needed. */
/*@unchecked@*/
static char *rpmpoptfiles = RPMPOPTFILES;
/*@=readonlytrans@*/

/**
 * Display rpm version.
 */
static void printVersion(FILE * fp)
	/*@globals rpmEVR, fileSystem, internalState @*/
	/*@modifies *fp, fileSystem, internalState @*/
{
    fprintf(fp, _("%s (" RPM_NAME ") %s\n"), __progname, rpmEVR);
    if (rpmIsVerbose())
	fprintf(fp, "rpmlib 0x%08x,0x%08x,0x%08x\n", (unsigned)rpmlibVersion(),
		(unsigned)rpmlibTimestamp(), (unsigned)rpmlibVendor());
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
	/*@globals pgpDigVSFlags, rpmcliTargets, rpmcliQueryFlags, rpmCLIMacroContext,
		rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies con, pgpDigVSFlags, rpmcliTargets, rpmcliQueryFlags, rpmCLIMacroContext,
		rpmGlobalMacroContext, fileSystem, internalState @*/
{

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
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
    case 'E':
	rpmcliConfigured();
	{   const char *val = rpmExpand(arg, NULL);
            size_t val_len;
            val_len = strlen(val);
            if (val[val_len - 1] == '\n')
                val_len = fwrite(val, val_len, 1, stdout);
            else
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
#ifdef WITH_LUA
 { "rpmlua", '\0', POPT_ARG_STRING, &rpmluaFiles, 0,
	N_("read <FILE:...> instead of default RPM Lua file(s)"),
	N_("<FILE:...>") },
#endif
 { "rpmpopt", '\0', POPT_ARG_STRING, NULL, 0,
	N_("read <FILE:...> instead of default POPT file(s)"),
	N_("<FILE:...>") },

 { "target", '\0', POPT_ARG_STRING, NULL,  RPMCLI_POPT_TARGETPLATFORM,
        N_("specify target platform"), N_("CPU-VENDOR-OS") },

 { "nodigest", '\0', 0, NULL, RPMCLI_POPT_NODIGEST,
        N_("don't verify package digest(s)"), NULL },
 { "nohdrchk", '\0', POPT_ARGFLAG_DOC_HIDDEN, NULL, RPMCLI_POPT_NOHDRCHK,
        N_("don't verify database header(s) when retrieved"), NULL },
 { "nosignature", '\0', 0, NULL, RPMCLI_POPT_NOSIGNATURE,
        N_("don't verify package signature(s)"), NULL },

 { "querytags", '\0', 0, NULL, POPT_QUERYTAGS,
        N_("display known query tags"), NULL },
 { "showrc", '\0', 0, NULL, POPT_SHOWRC,
	N_("display macro and configuration values"), NULL },
 { "version", '\0', POPT_ARGFLAG_DOC_HIDDEN, NULL, POPT_SHOWVERSION,
	N_("print the version"), NULL },

 { "promoteepoch", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmds_nopromote, 0,
	NULL, NULL},

 { "fpsdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_fps_debug, -1,
	NULL, NULL},
 { "fsmdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_fsm_debug, -1,
	N_("debug payload file state machine"), NULL},
 { "fsmthreads", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_fsm_threads, -1,
	N_("use threads for file state machine"), NULL},
 { "hdrdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_hdr_debug, -1,
	NULL, NULL},
 { "hdrfastdatalength", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_hdr_fastdatalength, -1,
	NULL, NULL},
 { "hdrlazytagswab", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_hdr_lazytagswab, -1,
	NULL, NULL},
 { "macrosused", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmts_macros, -1,
	NULL, NULL},
 { "pkgiodebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_pkgio_debug, -1,
	NULL, NULL},
 { "prtpkts", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_print_pkts, -1,
	NULL, NULL},
 { "psmdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_psm_debug, -1,
	N_("debug package state machine"), NULL},
 { "psmthreads", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_psm_threads, -1,
	N_("use threads for package state machine"), NULL},
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
 { "rpmnsdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmns_debug, -1,
	NULL, NULL},
 { "rpmpsdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmps_debug, -1,
	NULL, NULL},
 { "rpmsxdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmsx_debug, -1,
	NULL, NULL},
 { "rpmtedebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmte_debug, -1,
	NULL, NULL},
 { "rpmtsdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmts_debug, -1,
	NULL, NULL},
 { "rpmwfdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmwf_debug, -1,
	NULL, NULL},
 { "stats", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmts_stats, -1,
	NULL, NULL},

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	NULL, NULL},

   POPT_TABLEEND
};
/*@=bitwisesigned =compmempass @*/

poptContext
rpmcliFini(poptContext optCon)
	/*@globals keyids @*/
	/*@modifies keyids @*/
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
	/*@*/
{
    struct stat sb;
    int ret = 0;

    if (fstat(fdno, &sb) == -1 && errno == EBADF)
	ret = (open(devnull, flags) == fdno) ? 1 : 2;
    return ret;
}

#if defined(RPM_VENDOR_WINDRIVER)
void setRuntimeRelocPaths(void)
{
    /* 
     * This is just an example of setting the values using env
     * variables....  if they're not set, we make sure they get set
     * for helper apps...  We probably want to escape "%" in the path
     * to avoid macro expansion.. someone might have a % in a path...
     */

    __usrlibrpm = getenv("RPM_USRLIBRPM");
    __etcrpm = getenv("RPM_ETCRPM");
#if defined(ENABLE_NLS) && !defined(__LCLINT__)
    __localedir = getenv("RPM_LOCALEDIR");
#endif

    if ( __usrlibrpm == NULL ) {
	__usrlibrpm = USRLIBRPM ;
	setenv("RPM_USRLIBRPM", USRLIBRPM, 0);
    }

    if ( __etcrpm == NULL ) {
	__etcrpm = SYSCONFIGDIR ;
	setenv("RPM_ETCRPM", SYSCONFIGDIR, 0);
    }

#if defined(ENABLE_NLS) && !defined(__LCLINT__)
    if ( __localedir == NULL ) {
	__localedir = LOCALEDIR ;
	setenv("RPM_LOCALEDIR", LOCALEDIR, 0);
    }
#endif
}
#endif

/*@-globstate@*/
poptContext
rpmcliInit(int argc, char *const argv[], struct poptOption * optionsTable)
	/*@globals rpmpoptfiles @*/
	/*@modifies rpmpoptfiles @*/
{
    poptContext optCon;
    int rc;
    int xx;
    int i;

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

#if defined(RPM_VENDOR_WINDRIVER)
    (void) setRuntimeRelocPaths();
#endif

#if defined(ENABLE_NLS) && !defined(__LCLINT__)
    (void) setlocale(LC_ALL, "" );
    (void) bindtextdomain(PACKAGE, __localedir);
    (void) textdomain(PACKAGE);
#endif

    rpmSetVerbosity(RPMLOG_NOTICE);

    if (optionsTable == NULL) {
	/* Read rpm configuration (if not already read). */
	rpmcliConfigured();
	return NULL;
    }

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

/*@-nullpass -temptrans@*/
    optCon = poptGetContext(__progname, argc, (const char **)argv, optionsTable, 0);
/*@=nullpass =temptrans@*/

#if !defined(POPT_ERROR_BADCONFIG)	/* XXX popt-1.15- retrofit */
  { char * path_buf = xstrdup(rpmpoptfiles);
    char *path;
    char *path_next;

    for (path = path_buf; path != NULL && *path != '\0'; path = path_next) {
        const char **av;
        int ac;

        /* locate start of next path element */
        path_next = strchr(path, ':');
        if (path_next != NULL && *path_next == ':')
            *path_next++ = '\0';
        else
            path_next = path + strlen(path);

        /* glob-expand the path element */
        ac = 0;
        av = NULL;
        if ((xx = rpmGlob(path, &ac, &av)) != 0)
            continue;

        /* work-off each resulting file from the path element */
        for (i = 0; i < ac; i++) {
	    const char *fn = av[i];
	    if (fn[0] == '@' /* attention */) {
		fn++;
		if (!rpmSecuritySaneFile(fn)) {
		    rpmlog(RPMLOG_WARNING, "existing POPT configuration file \"%s\" considered INSECURE -- not loaded\n", fn);
		    /*@innercontinue@*/ continue;
		}
	    }
	    (void) poptReadConfigFile(optCon, fn);
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

#if defined(RPM_VENDOR_WINDRIVER)
    {	const char * poptAliasFn = rpmGetPath(__usrlibrpm, "/rpmpopt", NULL);
	(void) poptReadConfigFile(optCon, poptAliasFn);
	poptAliasFn = _free(poptAliasFn);
    }
#endif

    /* read standard POPT configuration files */
    /* XXX FIXME: the 2nd arg useEnv flag is UNUSED. */
    (void) poptReadDefaultConfig(optCon, 1);

#if defined(RPM_VENDOR_WINDRIVER)
    {	const char * poptExecPath = rpmGetPath(__usrlibrpm, NULL);
	poptSetExecPath(optCon, poptExecPath, 1);
	poptExecPath = _free(poptExecPath);
    }
#else
    poptSetExecPath(optCon, USRLIBRPM, 1);
#endif

    /* Process all options, whine if unknown. */
    while ((rc = poptGetNextOpt(optCon)) > 0) {
	const char * optArg = poptGetOptArg(optCon);
/*@-dependenttrans -observertrans@*/	/* Avoid popt memory leaks. */
	optArg = _free(optArg);
/*@=dependenttrans =observertrans @*/
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

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Initial Developer of the Original Code is PageMail, Inc.
 *
 * Portions created by the Initial Developer are 
 * Copyright (c) 2007-2009, PageMail, Inc. All Rights Reserved.
 *
 * Contributor(s): 
 * 
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** 
 */

/** 
 * @file	gsr.c		GPSEE Script Runner ("scripting host")
 * @author	Wes Garland
 * @date	Aug 27 2007
 * @version	$Id$
 *
 * This program is designed to interpret a JavaScript program as much like
 * a shell script as possible.
 *
 * @see exec(2) system call
 *
 * When launching as a file interpreter, a single argument may follow the
 * interpreter's filename. This argument starts with a dash and is a series
 * of argumentless flags.
 *
 * All other command line options will be passed along to the JavaScript program.
 *
 * The "official documentation" for the prescence and meaning of flags and switch
 * is the usage() function.
 */

static __attribute__ ((unused))
const char rcsid[] = "$Id$";

#include "system.h"

#include <rpmiotypes.h>
#include <rpmio.h>
#include <poptIO.h>

#define PRODUCT_SHORTNAME	"gsr"
#define PRODUCT_VERSION		"1.0-pre1"

#include <prinit.h>
#include "gpsee.h"
#if defined(GPSEE_DARWIN_SYSTEM)
#include <crt_externs.h>
#endif

#if defined(__SURELYNX__)
# define NO_APR_SURELYNX_NAMESPACE_POISONING
# define NO_SURELYNX_INT_TYPEDEFS	/* conflicts with mozilla's NSPR protypes.h */
#endif

#if defined(__SURELYNX__)
static apr_pool_t *permanent_pool;
#endif

#if defined(__SURELYNX__)
# define whenSureLynx(a,b)	a
#else
# define whenSureLynx(a,b)	b
#endif

#include "debug.h"

/*==============================================================*/

static int _rpmgsr_debug = 0;

#define _KFB(n) (1U << (n))
#define _DFB(n) (_KFB(n) | 0x40000000)

#define F_ISSET(_gsr, _FLAG) ((_gsr)->flags & ((RPMGSR_FLAGS_##_FLAG) & ~0x40000000))

/**
 * Bit field enum for CLI options.
 */
enum gsrFlags_e {
    RPMGSR_FLAGS_NONE		= 0,
    RPMGSR_FLAGS_NOEXEC		= _DFB(0),	/*!< -n */
    RPMGSR_FLAGS_SKIPSHEBANG	= _DFB(1),	/*!< -F */
};

/**
 */
typedef struct rpmgsr_s *rpmgsr;

/**
 */
struct rpmgsr_s {
    enum gsrFlags_e flags;	/*!< control bits. */
    int options;
    int gcZeal;

    gpsee_interpreter_t * I;	/* Handle describing JS interpreter */

    const char * code;		/* String with JavaScript program in it */
    const char * fn;		/* Filename with JavaScript program in it */
    char *const *argv;		/* Becomes arguments array in JS program */
    char *const *environ;	/* Environment to pass to script */

    int verbosity;		/* 0 = no debug, bigger = more debug */
    int fiArg;
};

/**
 */
static struct rpmgsr_s _gsr = {
    .flags = RPMGSR_FLAGS_NONE,
    .options =
      JSOPTION_ANONFUNFIX | JSOPTION_STRICT | JSOPTION_RELIMIT | JSOPTION_JIT,
};

/*==============================================================*/

extern rc_list rc;

/** Handler for fatal errors. Generate a fatal error
 *  message to surelog, stdout, or stderr depending on
 *  whether our controlling terminal is a tty or not.
 *
 *  @param	message		Arbitrary text describing the
 *				fatal condition
 *  @note	Exits with status 1
 */
static void __attribute__ ((noreturn)) fatal(const char *message)
{
    int haveTTY;
#if defined(HAVE_APR)
    apr_os_file_t currentStderrFileno;

    if (!apr_stderr
	|| (apr_os_file_get(&currentStderrFileno, apr_stderr) !=
	    APR_SUCCESS))
#else
    int currentStderrFileno;
#endif
    currentStderrFileno = STDERR_FILENO;

    haveTTY = isatty(currentStderrFileno);

    if (!message)
	message = "UNDEFINED MESSAGE - OUT OF MEMORY?";

    if (haveTTY) {
#if defined(HAVE_APR)
	if (apr_stderr)
	    apr_file_printf(apr_stderr,
			    "\007Fatal Error in " PRODUCT_SHORTNAME
			    ": %s\n", message);
	else
#endif
	    fprintf(stderr,
		    "\007Fatal Error in " PRODUCT_SHORTNAME ": %s\n",
		    message);
    } else
	gpsee_log(SLOG_EMERG, "Fatal Error: %s", message);

    exit(1);
}

/** GPSEE uses panic() to panic, expects embedder to provide */
void __attribute__ ((noreturn)) panic(const char *message)
{
    fatal(message);
}

/*==============================================================*/
/**
 */
static void rpmgsrArgCallback(poptContext con,
                /*@unused@*/ enum poptCallbackReason reason,
                const struct poptOption * opt, /*@unused@*/ const char * arg,
                /*@unused@*/ void * data)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    rpmgsr gsr = &_gsr;

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
    case 'F':
	gsr->flags |= RPMGSR_FLAGS_SKIPSHEBANG;
	gsr->fn = xstrdup(arg);
	break;
    case 'h':
	poptPrintHelp(con, stderr, 0);
#ifdef	NOTYET
/*@-exitarg@*/
	exit(0);
/*@=exitarg@*/
#endif
	break;
#if defined(__SURELYNX__)
    case 'D':
	redirect_output(permanent_pool, arg);
	break;
#ifdef	NOTYET	/* -r file appears defunct */
    case 'r':
#endif
#endif
    case 'a':
	break;
    case 'C':
	break;
    case 'd':
	break;
    case 'e':
	break;
    case 'J':
	break;
    case 'S':
	break;
    case 'R':
	break;
    case 'U':
	break;
    case 'W':
	break;
    case 'x':
	break;
    case 'z':
	gsr->gcZeal++;
	break;
    default:
	fprintf(stderr, _("%s: Unknown option -%c\n"), __progname, opt->val);
	poptPrintUsage(con, stderr, 0);
/*@-exitarg@*/
	exit(2);
/*@=exitarg@*/
	/*@notreached@*/ break;
    }
}

static struct poptOption _gsrOptionsTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmgsrArgCallback, 0, NULL, NULL },
/*@=type@*/

  { NULL, 'c', POPT_ARG_STRING,      &_gsr.code, 0,
        N_("Specifies literal JavaScript code to execute"), N_("code") },
  { NULL, 'f', POPT_ARG_STRING,      &_gsr.fn, 0,
        N_("Specifies the filename containing code to run"), N_("filename") },
  { NULL, 'F', POPT_ARG_STRING,      NULL, 'F',
        N_("Like -f, but skip shebang if present."), N_("filename") },
  { NULL, 'h', POPT_ARG_NONE,      NULL, 'h',
        N_("Display this help"), NULL },
  { NULL, 'n', POPT_BIT_SET,		&_gsr.flags, RPMGSR_FLAGS_NOEXEC,
	N_("Engine will load and parse, but not run, the script"), NULL },
#if defined(__SURELYNX__)
  { NULL, 'D', POPT_ARG_STRING,      NULL, 'D',
        N_("Specifies a debug output file"), N_("file") },
#ifdef	NOTYET	/* -r file appears defunct */
  { NULL, 'r', POPT_ARG_STRING,      NULL, 'r',
        N_("Specifies alternate interpreter RC file"), N_("file") },
#endif
#endif

  POPT_TABLEEND
};

static struct poptOption _jsOptionsTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmgsrArgCallback, 0, NULL, NULL },
/*@=type@*/

  { NULL, 'a', POPT_ARG_NONE,      NULL, 'a',
        N_("Allow (read-only) access to caller's environmen"), NULL },
  { NULL, 'C', POPT_ARG_NONE,      NULL, 'C',
        N_("Disables compiler caching via JSScript XDR serialization"), NULL },
  { NULL, 'd', POPT_ARG_NONE,      NULL, 'd',
        N_("Increase verbosity"), NULL },
  { NULL, 'e', POPT_ARG_NONE,      NULL, 'e',
        N_("Do not limit regexps to n^3 levels of backtracking"), NULL },
  { NULL, 'J', POPT_ARG_NONE,      NULL, 'J',
        N_("Disable nanojit"), NULL },
  { NULL, 'S', POPT_ARG_NONE,      NULL, 'S',
        N_("Disable Strict mode"), NULL },
  { NULL, 'R', POPT_ARG_NONE,      NULL, 'R',
        N_("Load RC file for interpreter (" PRODUCT_SHORTNAME ") based on script filename."), NULL },
  { NULL, 'U', POPT_ARG_NONE,      NULL, 'U',
        N_("Disable UTF-8 C string processing"), NULL },
  { NULL, 'W', POPT_ARG_NONE,      NULL, 'W',
        N_("Do not report warnings"), NULL },
  { NULL, 'x', POPT_ARG_NONE,      NULL, 'x',
        N_("Parse <!-- comments --> as E4X tokens"), NULL },
  { NULL, 'z', POPT_ARG_NONE,      NULL, 'z',
        N_("Increase GC Zealousness"), NULL },

  POPT_TABLEEND
};

static struct poptOption _optionsTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmgsrArgCallback, 0, NULL, NULL },
/*@=type@*/

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, _gsrOptionsTable, 0,
        N_("\
" PRODUCT_SHORTNAME " " PRODUCT_VERSION " - GPSEE Script Runner for GPSEE " GPSEE_CURRENT_VERSION_STRING "\n\
Copyright (c) 2007-2009 PageMail, Inc. All Rights Reserved.\n\
\n\
As an interpreter: #! gsr {-/*flags*/}\n\
As a command:      gsr {-r file} [-D file] [-z #] [-n] <[-c code]|[-f filename]>\n\
                   gsr {-/*flags*/} {[--] [arg...]}\n\
\n\
Command Options:\
"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, _jsOptionsTable, 0,
        N_("\
Valid Flags:\
"), NULL },

#ifdef	NOTYET
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"), NULL },

  POPT_AUTOALIAS
#endif
  POPT_AUTOHELP

  { NULL, -1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
        N_("\
" PRODUCT_SHORTNAME " " PRODUCT_VERSION " - GPSEE Script Runner for GPSEE " GPSEE_CURRENT_VERSION_STRING "\n\
Copyright (c) 2007-2009 PageMail, Inc. All Rights Reserved.\n\
\n\
As an interpreter: #! gsr {-/*flags*/}\n\
As a command:      gsr {-r file} [-D file] [-z #] [-n] <[-c code]|[-f filename]>\n\
                   gsr {-/*flags*/} {[--] [arg...]}\n\
Command Options:\n\
    -c code     Specifies literal JavaScript code to execute\n\
    -f filename Specifies the filename containing code to run\n\
    -F filename Like -f, but skip shebang if present.\n\
    -h          Display this help\n\
    -n          Engine will load and parse, but not run, the script\n\
    -D file     Specifies a debug output file\n\
    -r file     Specifies alternate interpreter RC file\n\
    flags       A series of one-character flags which can be used\n\
                in either file interpreter or command mode\n\
    --          Arguments after -- are passed to the script\n\
\n\
Valid Flags:\n\
    a - Allow (read-only) access to caller's environment\n\
    C - Disables compiler caching via JSScript XDR serialization\n\
    d - Increase verbosity\n\
    e - Do not limit regexps to n^3 levels of backtracking\n\
    J - Disable nanojit\n\
    S - Disable Strict mode\n\
    R - Load RC file for interpreter (" PRODUCT_SHORTNAME ") based on\n\
        script filename.\n\
    U - Disable UTF-8 C string processing\n\
    W - Do not report warnings\n\
    x - Parse <!-- comments --> as E4X tokens\n\
    z - Increase GC Zealousness\n\
"), NULL },

  POPT_TABLEEND
};

static struct poptOption *optionsTable = &_optionsTable[0];
/*==============================================================*/

/** Help text for this program, which doubles as the "official"
 *  documentation for the command-line parameters.
 *
 *  @param	argv_zero	How this program was invoked.
 *
 *  @note	Exits with status 1
 */
static void __attribute__ ((noreturn)) usage(const char *argv_zero)
{
    char spaces[strlen(argv_zero) + 1];

    memset(spaces, (int) (' '), sizeof(spaces) - 1);
    spaces[sizeof(spaces) - 1] = (char) 0;

    gpsee_printf("\n"
#if defined(__SURELYNX__)
		 "SureLynx "
#endif
		 PRODUCT_SHORTNAME " " PRODUCT_VERSION
		 " - GPSEE Script Runner for GPSEE "
		 GPSEE_CURRENT_VERSION_STRING "\n"
		 "Copyright (c) 2007-2009 PageMail, Inc. All Rights Reserved.\n"
		 "\n" "As an interpreter: #! %s {-/*flags*/}\n"
		 "As a command:      %s "
#if defined(__SURELYNX__)
		 "{-r file} [-D file] "
#endif
		 "[-z #] [-n] <[-c code]|[-f filename]>\n"
		 "                   %s {-/*flags*/} {[--] [arg...]}\n"
		 "Command Options:\n"
		 "    -c code     Specifies literal JavaScript code to execute\n"
		 "    -f filename Specifies the filename containing code to run\n"
		 "    -F filename Like -f, but skip shebang if present.\n"
		 "    -h          Display this help\n"
		 "    -n          Engine will load and parse, but not run, the script\n"
#if defined(__SURELYNX__)
		 "    -D file     Specifies a debug output file\n"
		 "    -r file     Specifies alternate interpreter RC file\n"
#endif
		 "    flags       A series of one-character flags which can be used\n"
		 "                in either file interpreter or command mode\n"
		 "    --          Arguments after -- are passed to the script\n"
		 "\n"
		 "Valid Flags:\n"
		 "    a - Allow (read-only) access to caller's environment\n"
		 "    C - Disables compiler caching via JSScript XDR serialization\n"
		 "    d - Increase verbosity\n"
		 "    e - Do not limit regexps to n^3 levels of backtracking\n"
		 "    J - Disable nanojit\n"
		 "    S - Disable Strict mode\n"
		 "    R - Load RC file for interpreter (" PRODUCT_SHORTNAME
		 ") based on\n" "        script filename.\n"
		 "    U - Disable UTF-8 C string processing\n"
		 "    W - Do not report warnings\n"
		 "    x - Parse <!-- comments --> as E4X tokens\n"
#ifdef JS_GC_ZEAL
		 "    z - Increase GC Zealousness\n"
#endif				/* JS_GC_ZEAL */
		 "\n", argv_zero, argv_zero, spaces);
    exit(1);
}

/** Process the script interpreter flags.
 *
 *  @param	flags	An array of flags, in no particular order.
 */
static void processFlags(rpmgsr gsr, const char *flags)
{
    gpsee_interpreter_t * I = gsr->I;
#ifdef JS_GC_ZEAL
    int gcZeal = 0;
#endif				/* JS_GC_ZEAL */
    int jsOptions;
    int verbosity =
	max(whenSureLynx(sl_get_debugLevel(), 0), gpsee_verbosity(0));
    const char *f;

    jsOptions =
	JS_GetOptions(I->cx) | JSOPTION_ANONFUNFIX | JSOPTION_STRICT |
	JSOPTION_RELIMIT | JSOPTION_JIT;

    /* Iterate over each flag */
    for (f = flags; *f; f++) {
	switch (*f) {
	    /* 'C' flag disables compiler cache */
	case 'C':
	    I->useCompilerCache = 0;
	    break;

	case 'a':		/* Handled in prmain() */
	case 'R':		/* Handled in loadRuntimeConfig() */
	case 'U':		/* Must be handled before 1st JS runtime */
	    break;

	case 'x':		/* Parse <!-- comments --> as E4X tokens */
	    jsOptions |= JSOPTION_XML;
	    break;

	case 'S':		/* Disable Strict JS */
	    jsOptions &= ~JSOPTION_STRICT;
	    break;

	case 'W':		/* Suppress JS Warnings */
	    I->errorReport |= er_noWarnings;
	    break;

	case 'e':		/* Allow regexps that are more than O(n^3) */
	    jsOptions &= ~JSOPTION_RELIMIT;
	    break;

	case 'J':		/* Disable Nanojit */
	    jsOptions &= ~JSOPTION_JIT;
	    break;

#ifdef JS_GC_ZEAL
	case 'z':		/* GC Zeal */
	    gcZeal++;
	    break;
#endif				/* JS_GC_ZEAL */

	case 'd':		/* debug */
	    gsr->verbosity++;
	    break;

	default:
	    gpsee_log(SLOG_WARNING, "Error: Unrecognized option flag %c!",
		      *f);
	    break;
	}
    }

#ifdef JS_GC_ZEAL
    JS_SetGCZeal(I->cx, gcZeal);
#endif				/* JS_GC_ZEAL */
    JS_SetOptions(I->cx, jsOptions);

    gpsee_verbosity(verbosity);
#if defined(__SURELYNX__)
    sl_set_debugLevel(verbosity);
    enableTerminalLogs(permanent_pool, verbosity > 0, NULL);
#endif
}

static FILE *openScriptFile(rpmgsr gsr)
{
    FILE * fp = fopen(gsr->fn, "r");
    gpsee_interpreter_t * I = gsr->I;
    char line[64];		/* #! args can be longer than 64 on some unices, but no matter */

    if (!fp)
	return NULL;

    gpsee_flock(fileno(fp), GPSEE_LOCK_SH);

    if (!F_ISSET(gsr, SKIPSHEBANG))
	return fp;

    if (fgets(line, sizeof(line), fp)) {
	if ((line[0] != '#') || (line[1] != '!')) {
	    gpsee_log(SLOG_NOTICE,
		      PRODUCT_SHORTNAME ": Warning: First line of "
		      "file-interpreter script does not contain #!");
	    rewind(fp);
	} else {
	    I->linenoOffset += 1;

	    do {		/* consume entire first line, regardless of length */
		if (strchr(line, '\n'))
		    break;
	    } while (fgets(line, sizeof(line), fp));
	}
    }

    return fp;
}

/**
 * Detect if we've been asked to run as a file interpreter.
 *
 * File interpreters always have the script filename within 
 * the first two arguments. It is never "not found", except
 * due to an annoying OS race in exec(2), which is very hard
 * to exploit.
 *
 * @returns - 0 if we're not a file interpreter
 *	    - 1 if the script filename is on argv[1]
 *          - 2 if the script filename is on argv[2]
 */
PRIntn findFileToInterpret(PRIntn argc, char **argv)
{
    if (argc == 1)
	return 0;

    if ((argv[1][0] == '-') && ((argv[1][1] == '-')
				|| strchr(argv[1] + 1, 'c')
				|| strchr(argv[1] + 1, 'f')
				|| strchr(argv[1] + 1, 'F')
				|| strchr(argv[1] + 1, 'h')
				|| strchr(argv[1] + 1, 'n')
#if defined(__SURELYNX__)
				|| strchr(argv[1], 'r')
				|| strchr(argv[1], 'D')
#endif
	))
	return 0;		/* -h, -c, -f, -F, -r will always be invalid flags & force command mode */

    if ((argc > 1) && argv[1][0] != '-')
	if (access(argv[1], F_OK) == 0)
	    return 1;

    if ((argc > 2) && (argv[2][0] != '-'))
	if (access(argv[2], F_OK) == 0)
	    return 2;

    return 0;
}

/** Load RC file based on filename.
 *  Filename to use is script's filename, if -R option flag is thrown.
 *  Otherwise, file interpreter's filename (i.e. gsr) is used.
 */
void
loadRuntimeConfig(rpmgsr gsr, const char *flags, int argc, char *const *argv)
{
    const char *scriptFilename = gsr->fn;
    rcFILE *rc_file;

    if (strchr(flags, 'R')) {
	char fake_argv_zero[FILENAME_MAX];
	char *fake_argv[2];
	char *s;

	if (!scriptFilename)
	    fatal
		("Cannot use script-filename RC file when script-filename is unknown!");

	fake_argv[0] = fake_argv_zero;

	gpsee_cpystrn(fake_argv_zero, scriptFilename,
		      sizeof(fake_argv_zero));

	if ((s = strstr(fake_argv_zero, ".js")) && (s[3] == (char) 0))
	    *s = (char) 0;

	if ((s = strstr(fake_argv_zero, ".es3")) && (s[4] == (char) 0))
	    *s = (char) 0;

	if ((s = strstr(fake_argv_zero, ".e4x")) && (s[4] == (char) 0))
	    *s = (char) 0;

	rc_file = rc_openfile(1, fake_argv);
    } else {
	rc_file = rc_openfile(argc, argv);
    }

    if (!rc_file)
	fatal("Could not open interpreter's RC file!");

    rc = rc_readfile(rc_file);
    rc_close(rc_file);
}

PRIntn prmain(PRIntn argc, char **argv)
{
    poptContext optCon;
    rpmgsr gsr = &_gsr;
    char * flags;		/* Flags found on command line */
    int ec;

    flags = xmalloc(8);
    flags[0] = '\0';
    gsr->verbosity =
	max(whenSureLynx(sl_get_debugLevel(), 0), gpsee_verbosity(0));

    gpsee_openlog(gpsee_basename(argv[0]));

#if defined(__SURELYNX__)
    permanent_pool = apr_initRuntime();
#endif

    optCon = rpmioInit(argc, argv, optionsTable);

    /* Print usage and exit if no arguments were given */
    if (argc < 2)
	usage(argv[0]);

    gsr->fiArg = findFileToInterpret(argc, argv);

    if (gsr->fiArg == 0) {	/* Not a script file interpreter (shebang) */
	char *fe = flags;
	int c;

	while ((c =
		getopt(argc, argv,
		       whenSureLynx("D:r:", "") "v:c:hnf:F:aCRxSUWdeJ"
#ifdef JS_GC_ZEAL
		       "z"
#endif
		)) != -1) {
	    switch (c) {
#if defined(__SURELYNX__)
	    case 'D':
		redirect_output(permanent_pool, optarg);
		break;
	    case 'r':		/* handled by rc_openfile() */
		break;
#endif

	    case 'c':
		gsr->code = xstrdup(optarg);
		break;

	    case 'h':
		usage(argv[0]);
		break;

	    case 'n':
		gsr->flags |= RPMGSR_FLAGS_NOEXEC;
		break;

	    case 'F':
		gsr->flags |= RPMGSR_FLAGS_SKIPSHEBANG;
	    case 'f':
		gsr->fn = xstrdup(optarg);
		break;

	    case 'a':
	    case 'C':
	    case 'd':
	    case 'e':
	    case 'J':
	    case 'S':
	    case 'R':
	    case 'U':
	    case 'W':
	    case 'x':
	    case 'z':
		{
		    char *flags_storage = xrealloc(flags, (fe - flags) + 1 + 1);

		    if (flags_storage != flags) {
			fe = (fe - flags) + flags_storage;
			flags = flags_storage;
		    }

		    *fe++ = c;
		    *fe = '\0';
		}
		break;

	    case '?':
		{
		    char buf[32];

		    snprintf(buf, sizeof(buf), "Invalid option: %c\n",
			     optopt);
		    fatal(buf);
		}

	    case ':':
		{
		    char buf[32];

		    snprintf(buf, sizeof(buf),
			     "Missing parameter to option '%c'\n", optopt);
		    fatal(buf);
		}

	    }
	}			/* getopt wend */

	if (optind < argc)
	    gsr->argv = argv + optind;

	*fe = '\0';

	if (gsr->verbosity) {
#if defined(__SURELYNX__)
	    sl_set_debugLevel(gsr->verbosity);
#endif
	    gpsee_verbosity(gsr->verbosity);
	}
    } else {
	gsr->fn = xstrdup(argv[gsr->fiArg]);

	if (gsr->fiArg == 2) {
	    if (argv[1][0] != '-')
		fatal
		    ("Invalid syntax for file-interpreter flags! (Must begin with '-')");

	    flags = xrealloc(flags, strlen(argv[1]) + 1);
	    strcpy(flags, argv[1] + 1);
	}

	gsr->argv = argv + gsr->fiArg;
    }

    if (strchr(flags, 'a')) {
#if defined(GPSEE_DARWIN_SYSTEM)
	gsr->environ = (char *const *) _NSGetEnviron();
#else
	extern char **environ;
	gsr->environ = (char *const *) environ;
#endif
    }

    loadRuntimeConfig(gsr, flags, argc, argv);
    if (strchr(flags, 'U')
	|| (rc_bool_value(rc, "gpsee_force_no_utf8_c_strings") == rc_true)
	|| getenv("GPSEE_NO_UTF8_C_STRINGS")) {
	JS_DestroyRuntime(JS_NewRuntime(1024));
	putenv((char *) "GPSEE_NO_UTF8_C_STRINGS=1");
    }

    if (gsr->argv == NULL) {
	static const char const *empty_argv[] = { NULL };
	gsr->argv = (char *const *) empty_argv;
    }

    gsr->I = gpsee_createInterpreter(gsr->argv, gsr->environ);
    processFlags(gsr, flags);
    flags = _free(flags);

    /* Run JavaScript specified with -c */
    if (gsr->code) {
	gpsee_interpreter_t * I = gsr->I;
	jsval v;
	JS_EvaluateScript(I->cx, I->globalObj, gsr->code,
			  strlen(gsr->code), "command_line", 1, &v);
	if (JS_IsExceptionPending(I->cx))
	    goto exit;
    }

#if !defined(SYSTEM_GSR)
#define	SYSTEM_GSR	"/usr/bin/gsr"
#endif
    if ((argv[0][0] == '/') && (strcmp(argv[0], SYSTEM_GSR) != 0)
     && rc_bool_value(rc, "no_gsr_preload_script") != rc_true)
    {
	gpsee_interpreter_t * I = gsr->I;
	char preloadScriptFilename[FILENAME_MAX];
	char mydir[FILENAME_MAX];
	int i;

	i = snprintf(preloadScriptFilename, sizeof(preloadScriptFilename),
		     "%s/.%s_preload", gpsee_dirname(argv[0], mydir,
						     sizeof(mydir)),
		     gpsee_basename(argv[0]));
	if ((i == 0) || (i == (sizeof(preloadScriptFilename) - 1)))
	    gpsee_log(SLOG_EMERG,
		      PRODUCT_SHORTNAME
		      ": Unable to create preload script filename!");
	else
	    errno = 0;

	if (access(preloadScriptFilename, F_OK) == 0) {
	    jsval v;
	    const char *errmsg;
	    JSScript *script;
	    JSObject *scrobj;

	    if (gpsee_compileScript
		(I->cx, preloadScriptFilename, NULL, &script,
		 I->globalObj, &scrobj, &errmsg)) {
		gpsee_log(SLOG_EMERG,
			  PRODUCT_SHORTNAME
			  ": Unable to compile preload script '%s' - %s",
			  preloadScriptFilename, errmsg);
		goto exit;
	    }

	    if (!script || !scrobj)
		goto exit;

	    JS_AddNamedRoot(I->cx, &scrobj, "preload_scrobj");
	    JS_ExecuteScript(I->cx, I->globalObj, script, &v);
	    if (JS_IsExceptionPending(I->cx)) {
		I->exitType = et_exception;
		JS_ReportPendingException(I->cx);
	    }
	    JS_RemoveRoot(I->cx, &scrobj);
	}

	if (I->exitType & et_exception)
	    goto exit;
    }


    if (gsr->fiArg != 0)
	gsr->flags |= RPMGSR_FLAGS_SKIPSHEBANG;

    if (gsr->fn == NULL) {
	ec = gsr->code ? 0 : 1;
    } else {
	FILE * fp = openScriptFile(gsr);
	if (fp == NULL) {
	    gpsee_log(SLOG_NOTICE,
		      PRODUCT_SHORTNAME
		      ": Unable to open' script '%s'! (%m)",
		      gsr->fn);
	    ec = 1;
	} else {
	    gpsee_runProgramModule(gsr->I->cx, gsr->fn, fp);
	    fclose(fp);
	    if ((gsr->I->exitType & et_successMask) == gsr->I->exitType)
		ec = gsr->I->exitCode;
	    else
		ec = 1;
	}
    }

exit:
    gsr->code = _free(gsr->code);
    gsr->fn = _free(gsr->fn);
    gsr->argv = NULL;
    gsr->environ = NULL;

    optCon = rpmioFini(optCon);

    gpsee_destroyInterpreter(gsr->I);
    gsr->I = NULL;
    JS_ShutDown();

    return ec;
}

int main(int argc, char *argv[])
{
    return PR_Initialize(prmain, argc, argv, 0);
}

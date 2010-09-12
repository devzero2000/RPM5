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
 * Copyright (c) 2007-2010, PageMail, Inc. All Rights Reserved.
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

#define PRODUCT_VERSION		"1.0-pre3"

#if !defined(GPSEE_DEBUGGER)
# define PRODUCT_SUMMARY        "Script Runner for GPSEE"
# define PRODUCT_SHORTNAME	"gsr"
#else
# define PRODUCT_SUMMARY        "Script Debugger for GPSEE"
# define PRODUCT_SHORTNAME	"gsrdb"
#endif

#if !defined(SYSTEM_GSR)
#define	SYSTEM_GSR	"/usr/bin/" PRODUCT_SHORTNAME
#endif

#include <prinit.h>
#include "gpsee.h"
#if defined(GPSEE_DARWIN_SYSTEM)
#include <crt_externs.h>
#endif

#define xstr(s) str(s)
#define str(s) #s

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
	fprintf(stderr, "\007Fatal Error in " PRODUCT_SHORTNAME ": %s\n",
		message);
    } else
	gpsee_log(NULL, SLOG_EMERG, "Fatal Error: %s", message);

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
    spaces[sizeof(spaces) - 1] = '\0';

    printf("\n"
	   PRODUCT_SHORTNAME " " PRODUCT_VERSION " - " PRODUCT_SUMMARY " "
	   GPSEE_CURRENT_VERSION_STRING "\n"
	   "Copyright (c) 2007-2010 PageMail, Inc. All Rights Reserved.\n"
	   "\n" "As an interpreter: #! %s {-/*flags*/}\n"
	   "As a command:      %s "
	   "[-z #] [-n] <[-c code] [-f filename]>\n"
	   "                   %s {-/*flags*/} {[--] [arg...]}\n"
	   "Command Options:\n"
	   "    -c code     Specifies literal JavaScript code to execute (runs before -f)\n"
	   "    -f filename Specifies the filename containing code to run\n"
	   "    -F filename Like -f, but skip shebang if present.\n"
	   "    -h          Display this help\n"
	   "    -H          Display more help\n"
	   "    -n          Engine will load and parse, but not run, the script\n"
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
	   "    z - Increase GC Zealousness\n" "\n", argv_zero, argv_zero,
	   spaces);
    exit(1);
}


/** More help text for this program, which doubles as the "official"
 *  documentation for the more subtle behaviours of this embedding.
 *
 *  @param	argv_zero	How this program was invoked.
 *
 *  @note	Exits with status 1
 */
static void __attribute__ ((noreturn)) moreHelp(const char *argv_zero)
{
    char spaces[strlen(argv_zero) + 1];

    memset(spaces, (int) (' '), sizeof(spaces) - 1);
    spaces[sizeof(spaces) - 1] = '\0';

    printf("\n"
	   PRODUCT_SHORTNAME " " PRODUCT_VERSION " - " PRODUCT_SUMMARY " "
	   GPSEE_CURRENT_VERSION_STRING "\n"
	   "Copyright (c) 2007-2010 PageMail, Inc. All Rights Reserved.\n"
	   "\n" "More Help: Additional information beyond basic usage.\n"
	   "\n" "Verbosity\n"
	   "  Verbosity is a measure of how much output GPSEE and "
	   PRODUCT_SHORTNAME " send to stderr.\n"
	   "  To request verbosity N, specify the d flag N times when invoking "
	   PRODUCT_SHORTNAME ".\n"
	   "  Requests in both the shebang (#!) and comment-embedded options is additive.\n"
	   "\n" "  If you invoke " PRODUCT_SHORTNAME
	   " such that stderr is a tty, verbosity will be automatically\n"
	   "  set to " xstr(GSR_MIN_TTY_VERBOSITY)
	   ", unless your -d flags indicate an even higher level.\n" "\n"
	   "  Before your program runs, i.e. you are running script code with -c or a\n"
	   "  preload script, verbosity will be set to "
	   xstr(GSR_PREPROGRAM_TTY_VERBOSITY) " when stderr is a tty,\n"
	   "  and " xstr(GSR_PREPROGRAM_NOTTY_VERBOSITY) " otherwise.\n"
	   "\n" "Uncaught Exceptions\n"
	   "  - Errors are output to stderr when verbosity >= "
	   xstr(GPSEE_ERROR_OUTPUT_VERBOSITY) "\n"
	   "  - Warnings are output to stderr when verbosity >= "
	   xstr(GPSEE_ERROR_OUTPUT_VERBOSITY) "\n"
	   "  - Stack is dumped when error output is enabled and stderr is a tty,\n"
	   "    or verbosity >= " xstr(GSR_FORCE_STACK_DUMP_VERBOSITY) "\n"
	   "  - Syntax errors will have their location within the source shown when stderr\n"
	   "    is a tty, and verbosity >= "
	   xstr(GPSEE_ERROR_POINTER_VERBOSITY) "\n" "\n"
	   "GPSEE-core debugging\n"
	   "  - The module system will generate debug output whe verbosity >= "
	   xstr(GPSEE_MODULE_DEBUG_VERBOSITY) "\n"
	   "  - The script precompilation sub-system will generate debug output\n"
	   "    when verbosity >= " xstr(GPSEE_XDR_DEBUG_VERBOSITY) "\n"
	   "\n" "Miscellaneous\n"
	   "  - Exit codes 0 and 1 are reserved for 'success' and 'error' respectively.\n"
	   "    Application programs can return any exit code they wish, from 0-127,\n"
	   "    with either require('gpsee').exit() or by throwing a number literal.\n"
	   "  - Preload scripts will only be processed when "
	   PRODUCT_SHORTNAME " is not invoked\n" "    as " SYSTEM_GSR ".\n"
	   "\n");
    exit(1);
};

/** Process the script interpreter flags.
 *
 *  @param	flags	An array of flags, in no particular order.
 */
static void processFlags(rpmgsr gsr, const char *flags,
			 signed int *verbosity_p)
{
    JSContext * cx = gsr->I->cx;
    int gcZeal = 0;
    int jsOptions;
    const char *f;
    gpsee_runtime_t *grt = JS_GetRuntimePrivate(JS_GetRuntime(cx));

    jsOptions =
	JS_GetOptions(cx) | JSOPTION_ANONFUNFIX | JSOPTION_STRICT |
	JSOPTION_RELIMIT | JSOPTION_JIT;
    *verbosity_p = 0;

    /* Iterate over each flag */
    for (f = flags; *f; f++) {
	switch (*f) {
	    /* 'C' flag disables compiler cache */
	case 'C':
	    grt->useCompilerCache = 0;
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
	    grt->errorReport |= er_noWarnings;
	    break;

	case 'e':		/* Allow regexps that are more than O(n^3) */
	    jsOptions &= ~JSOPTION_RELIMIT;
	    break;

	case 'J':		/* Disable Nanojit */
	    jsOptions &= ~JSOPTION_JIT;
	    break;

	case 'z':		/* GC Zeal */
	    gcZeal++;
	    break;

	case 'd':		/* increase debug level */
	    (*verbosity_p)++;;
	    break;

	default:
	    gpsee_log(cx, SLOG_WARNING,
		      "Error: Unrecognized option flag %c!", *f);
	    break;
	}
    }

#ifdef JSFEATURE_GC_ZEAL
    if (JS_HasFeature(JSFEATURE_GC_ZEAL) == JS_TRUE)
	JS_SetGCZeal(cx, gcZeal);
#else
# ifdef JS_GC_ZEAL
    JS_SetGCZeal(cx, gcZeal);
# else
#  warning JS_SetGCZeal not available when building with this version of SpiderMonkey (try a debug build?)
# endif
#endif

    JS_SetOptions(cx, jsOptions);
}

static void processInlineFlags(rpmgsr gsr, FILE * fp, signed int *verbosity_p)
{
    char buf[256];
    off_t offset;

    offset = ftello(fp);

    while (fgets(buf, sizeof(buf), fp)) {
	char *s, *e;

	if ((buf[0] != '/') || (buf[1] != '/'))
	    break;

	for (s = buf + 2; *s == ' ' || *s == '\t'; s++);
	if (strncmp(s, "gpsee:", 6) != 0)
	    continue;

	for (s = s + 6; *s == ' ' || *s == '\t'; s++);

	for (e = s; *e; e++) {
	    switch (*e) {
	    case '\r':
	    case '\n':
	    case '\t':
	    case ' ':
		*e = '\0';
		break;
	    }
	}

	if (s[0])
	    processFlags(gsr, s, verbosity_p);
    }

    fseeko(fp, offset, SEEK_SET);
}

static FILE *openScriptFile(rpmgsr gsr)
{
    JSContext * cx = gsr->I->cx;
    FILE *file = fopen(gsr->fn, "r");
    char line[64];		/* #! args can be longer than 64 on some unices, but no matter */

    if (!file)
	return NULL;

    gpsee_flock(fileno(file), GPSEE_LOCK_SH);

    if (!F_ISSET(gsr, SKIPSHEBANG))
	return file;

    if (fgets(line, sizeof(line), file)) {
	if ((line[0] != '#') || (line[1] != '!')) {
	    gpsee_log(cx, SLOG_NOTICE,
		      PRODUCT_SHORTNAME ": Warning: First line of "
		      "file-interpreter script does not contain #!");
	    rewind(file);
	} else {
	    do {		/* consume entire first line, regardless of length */
		if (strchr(line, '\n'))
		    break;
	    } while (fgets(line, sizeof(line), file));

	    ungetc('\n', file);	/* Make spidermonkey think the script starts with a blank line, to keep line numbers in sync */
	}
    }

    return file;
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
	)
	)
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
void loadRuntimeConfig(rpmgsr gsr, const char *flags,
		       int argc, char *const *argv)
{
    rcFILE *rc_file;

    if (strchr(flags, 'R')) {
	char fake_argv_zero[FILENAME_MAX];
	char *fake_argv[2];
	char *s;

	if (gsr->fn == NULL)
	    fatal
		("Cannot use script-filename RC file when script-filename is unknown!");

	fake_argv[0] = fake_argv_zero;

	gpsee_cpystrn(fake_argv_zero, gsr->fn, sizeof(fake_argv_zero));

	if ((s = strstr(fake_argv_zero, ".js")) && s[3] == '\0')
	    *s = '\0';

	if ((s = strstr(fake_argv_zero, ".es3")) && s[4] == '\0')
	    *s = '\0';

	if ((s = strstr(fake_argv_zero, ".e4x")) && s[4] == '\0')
	    *s = '\0';

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
    gpsee_interpreter_t * I = NULL;
    gpsee_realm_t * realm;	/* Interpreter's primordial realm */
    JSContext * cx;		/* A context in realm */

    void *stackBasePtr;
    char *flags;		/* Flags found on command line */
    int ec = 1;
#ifdef GPSEE_DEBUGGER
    JSDContext *jsdc;
#endif

    flags = xmalloc(8);
    flags[0] = '\0';

    gpsee_setVerbosity(isatty(STDERR_FILENO) ? GSR_PREPROGRAM_TTY_VERBOSITY
		       : GSR_PREPROGRAM_NOTTY_VERBOSITY);
    gpsee_openlog(gpsee_basename(argv[0]));

    optCon = rpmioInit(argc, argv, optionsTable);

    /* Print usage and exit if no arguments were given */
    if (argc < 2)
	usage(argv[0]);

    gsr->fiArg = findFileToInterpret(argc, argv);

    if (gsr->fiArg == 0) {		/* Not a script file interpreter (shebang) */
	char *fe = flags;
	int c;

	while ((c = getopt(argc, argv, "v:c:hHnf:F:aCRxSUWdeJz")) != -1) {
	    switch (c) {

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
		    char *flags_storage =
			realloc(flags, (fe - flags) + 1 + 1);

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

	/* Create the script's argument vector with the script name as arguments[0] */
	{
	    char **nc_script_argv =
		malloc(sizeof(argv[0]) * (2 + (argc - optind)));
	    nc_script_argv[0] = (char *) gsr->fn;
	    memcpy(nc_script_argv + 1, argv + optind,
		   sizeof(argv[0]) * ((1 + argc) - optind));
	    gsr->argv = nc_script_argv;
	}

	*fe = '\0';
    } else {
	gsr->fn = argv[gsr->fiArg];

	if (gsr->fiArg == 2) {
	    if (argv[1][0] != '-')
		fatal
		    ("Invalid syntax for file-interpreter flags! (Must begin with '-')");

	    flags = realloc(flags, strlen(argv[1]) + 1);
	    strcpy(flags, argv[1] + 1);
	}

	/* This is a file interpreter; script_argv is correct at offset fiArg. */
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
     || getenv("GPSEE_NO_UTF8_C_STRINGS"))
    {
	JS_DestroyRuntime(JS_NewRuntime(1024));
	putenv((char *) "GPSEE_NO_UTF8_C_STRINGS=1");
    }

    gsr->I = I = gpsee_createInterpreter();
    realm = I->realm;
    cx = I->cx;
#if defined(GPSEE_DEBUGGER)
    jsdc = gpsee_initDebugger(cx, realm, DEBUGGER_JS);
#endif

    gpsee_setThreadStackLimit(cx, &stackBasePtr,
		strtol(rc_default_value(rc, "gpsee_thread_stack_limit_bytes",
			      "0x80000"), NULL, 0));

    processFlags(gsr, flags, &gsr->verbosity);
    free(flags);

    if (gsr->verbosity < GSR_MIN_TTY_VERBOSITY && isatty(STDERR_FILENO))
	gsr->verbosity = GSR_MIN_TTY_VERBOSITY;

    if (gpsee_verbosity(0) < gsr->verbosity)
	gpsee_setVerbosity(gsr->verbosity);

    /* Run JavaScript specified with -c */
    if (gsr->code) {
	jsval v;

	if (JS_EvaluateScript(cx, realm->globalObject,
		gsr->code, strlen(gsr->code),
		"command_line", 1, &v) == JS_FALSE)
	{
	    v = JSVAL_NULL;
	    goto exit;
	}
	v = JSVAL_NULL;
    }

    if (argv[0][0] == '/' && strcmp(argv[0], SYSTEM_GSR)
     && rc_bool_value(rc, "no_gsr_preload_script") != rc_true)
    {
	char preloadScriptFilename[FILENAME_MAX];
	char mydir[FILENAME_MAX];
	int i;

	i = snprintf(preloadScriptFilename, sizeof(preloadScriptFilename),
		     "%s/.%s_preload", gpsee_dirname(argv[0], mydir,
						     sizeof(mydir)),
		     gpsee_basename(argv[0]));
	if (i == 0 || i == (sizeof(preloadScriptFilename) - 1))
	    gpsee_log(cx, SLOG_EMERG,
		      PRODUCT_SHORTNAME
		      ": Unable to create preload script filename!");
	else
	    errno = 0;

	if (access(preloadScriptFilename, F_OK) == 0) {
	    jsval v;
	    JSScript *script;
	    JSObject *scrobj;

	    if (!gpsee_compileScript(cx, preloadScriptFilename,
			NULL, NULL, &script, realm->globalObject, &scrobj))
	    {
		gpsee_log(cx, SLOG_EMERG,
			  PRODUCT_SHORTNAME
			  ": Unable to compile preload script '%s'",
			  preloadScriptFilename);
		goto exit;
	    }

	    if (!script || !scrobj)
		goto exit;

	    if (!F_ISSET(gsr, NOEXEC)) {
		JS_AddNamedObjectRoot(cx, &scrobj, "preload_scrobj");
		if (JS_ExecuteScript(cx, realm->globalObject, script, &v) == JS_FALSE)
		{
		    if (JS_IsExceptionPending(cx))
			I->grt->exitType = et_exception;
		    JS_ReportPendingException(cx);
		}
		JS_RemoveObjectRoot(cx, &scrobj);
		v = JSVAL_NULL;
	    }
	}

	if (I->grt->exitType & et_exception)
	    goto exit;
    }

    /* Setup for main-script running -- cancel preprogram verbosity and use our own error reporting system in gsr that does not use error reporter */
    gpsee_setVerbosity(gsr->verbosity);
    JS_SetOptions(cx, JS_GetOptions(cx) | JSOPTION_DONT_REPORT_UNCAUGHT);

    if (gsr->fiArg != 0)
	gsr->flags |= RPMGSR_FLAGS_SKIPSHEBANG;

    if (gsr->fn == NULL) {
	ec = gsr->code ? 0 : 1;
    } else {
	FILE *fp = openScriptFile(gsr);

	if (fp == NULL) {
	    gpsee_log(cx, SLOG_NOTICE,
		      PRODUCT_SHORTNAME
		      ": Unable to open' script '%s'! (%m)",
		      gsr->fn);
	    ec = 1;
	    goto exit;
	}

	processInlineFlags(gsr, fp, &gsr->verbosity);
	gpsee_setVerbosity(gsr->verbosity);

	/* Just compile and exit? */
	if (F_ISSET(gsr, NOEXEC)) {
	    JSScript *script;
	    JSObject *scrobj;

	    if (!gpsee_compileScript(cx, gsr->fn,
			fp, NULL, &script, realm->globalObject, &scrobj))
	    {
		gpsee_reportUncaughtException(cx, JSVAL_NULL,
			(gpsee_verbosity(0) >= GSR_FORCE_STACK_DUMP_VERBOSITY)
			||
			((gpsee_verbosity(0) >= GPSEE_ERROR_OUTPUT_VERBOSITY)
				&& isatty(STDERR_FILENO)));
		ec = 1;
	    } else {
		ec = 0;
	    }
	} else {		/* noRunScript is false; run the program */

	    if (!gpsee_runProgramModule(cx, gsr->fn,
			NULL, fp, gsr->argv, gsr->environ))
	    {
		int code = gpsee_getExceptionExitCode(cx);
		if (code >= 0) {
		    ec = code;
		} else {
		    gpsee_reportUncaughtException(cx, JSVAL_NULL,
			(gpsee_verbosity(0) >= GSR_FORCE_STACK_DUMP_VERBOSITY)
			||
			((gpsee_verbosity(0) >= GPSEE_ERROR_OUTPUT_VERBOSITY)
				&& isatty(STDERR_FILENO)));
		    ec = 1;
		}
	    } else {
		ec = 0;
	    }
	}
	fclose(fp);
	goto exit;
    }

exit:
    gsr->code = _free(gsr->code);
    gsr->fn = _free(gsr->fn);
    gsr->argv = NULL;
    gsr->environ = NULL;

    optCon = rpmioFini(optCon);

#ifdef GPSEE_DEBUGGER
    gpsee_finiDebugger(jsdc);
#endif

    gpsee_destroyInterpreter(gsr->I);
    gsr->I = I = NULL;

    JS_ShutDown();

    return ec;
}

int main(int argc, char *argv[])
{
    return PR_Initialize(prmain, argc, argv, 0);
}

#ifdef GPSEE_DEBUGGER
/**
 *  Entry point for native-debugger debugging. Set this function as a break point
 *  in your native-language debugger debugging gsrdb, and you can set breakpoints
 *  on JSNative (and JSFastNative) functions from the gsrdb user interface.
 */
extern void __attribute__ ((noinline)) gpsee_breakpoint(void)
{
    return;
}
#endif

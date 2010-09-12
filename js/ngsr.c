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

#define	_RPMJS_INTERNAL
#include <rpmjs.h>

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

extern rc_list rc;	/* XXX unfortunate variable name choice */

#define xstr(s) str(s)
#define str(s) #s

#include "debug.h"

/*==============================================================*/

#define F_ISSET(_flags, _FLAG) ((_flags) & RPMJS_FLAGS_##_FLAG)

static const char * Icode;	/* String with JavaScript program in it */
static const char * Ifn;	/* Filename with JavaScript program in it */
static int verbosity;		/* 0 = no debug, bigger = more debug */

/*==============================================================*/

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
static void rpmjsArgCallback(poptContext con,
                /*@unused@*/ enum poptCallbackReason reason,
                const struct poptOption * opt, /*@unused@*/ const char * arg,
                /*@unused@*/ void * data)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    rpmjs js = &_rpmjs;

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
    case 'F':
	js->flags |= RPMJS_FLAGS_SKIPSHEBANG;
	Ifn = xstrdup(arg);
	break;
    case 'h':
	poptPrintHelp(con, stderr, 0);
	exit(EXIT_SUCCESS);
	/*@notreached@*/ break;
    case 'd':	verbosity++;	break;
    case 'z':	_rpmjs_zeal++;	break;
    default:
	fprintf(stderr, _("%s: Unknown option -%c\n"), __progname, opt->val);
	poptPrintUsage(con, stderr, 0);
	exit(EXIT_FAILURE);
	/*@notreached@*/ break;
    }
}

static struct poptOption _gsrOptionsTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmjsArgCallback, 0, NULL, NULL },
/*@=type@*/

  { NULL, 'c', POPT_ARG_STRING,      &Icode, 0,
        N_("Specifies literal JavaScript code to execute"), N_("code") },
  { NULL, 'f', POPT_ARG_STRING,      &Ifn, 0,
        N_("Specifies the filename containing code to run"), N_("filename") },
  { NULL, 'F', POPT_ARG_STRING,      NULL, 'F',
        N_("Like -f, but skip shebang if present"), N_("filename") },
  { NULL, 'h', POPT_ARG_NONE,      NULL, 'h',
        N_("Display this help"), NULL },
  { "noexec", 'n', POPT_BIT_SET,	&_rpmjs.flags, RPMJS_FLAGS_NOEXEC,
	N_("Engine will load and parse, but not run, the script"), NULL },
#if defined(__SURELYNX__)
  { NULL, 'D', POPT_ARG_STRING,      NULL, 'D',
        N_("Specifies a debug output file"), N_("file") },
#ifdef	NOTYET	/* -r file appears defunct */
  { NULL, 'r', POPT_ARG_STRING,      NULL, 'r',
        N_("Specifies alternate interpreter RC file"), N_("file") },
#endif	/* NOTYET */
#endif	/* __SURELYNX__ */

  POPT_TABLEEND
};

extern struct poptOption rpmjsIPoptTable[];	/* XXX in rpmjs.h? */
static struct poptOption _jsOptionsTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmjsArgCallback, 0, NULL, NULL },
/*@=type@*/

  { NULL, 'd', POPT_ARG_NONE,      NULL, 'd',
        N_("Increase verbosity"), NULL },
  { "gczeal", 'z', POPT_ARG_NONE,	NULL, 'z',
        N_("Increase GC Zealousness"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmjsIPoptTable, 0,
        N_("JS shell options"), NULL },

  POPT_TABLEEND
};

static struct poptOption _optionsTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmjsArgCallback, 0, NULL, NULL },
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
#endif

  POPT_AUTOALIAS
  POPT_AUTOHELP
  POPT_TABLEEND
};

static struct poptOption *optionsTable = &_optionsTable[0];
/*==============================================================*/


#ifdef	DYING
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
#endif

#ifdef	NOTYET	/* XXX FIXME */
static void processInlineFlags(rpmjs js, FILE * fp, signed int *verbosity_p)
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
#endif

static PRIntn prmain(PRIntn argc, char **argv)
{
    poptContext optCon;
    rpmjs js;
    char *const * Iargv = NULL;	/* Becomes arguments array in JS program */
    char *const * Ienviron = NULL;	/* Environment to pass to script */
    int ac = 0;
    int ec = 1;

    void *stackBasePtr = NULL;
    gpsee_interpreter_t * I = NULL;
gpsee_realm_t * realm;	/* Interpreter's primordial realm */
JSContext * cx;		/* A context in realm */
#ifdef GPSEE_DEBUGGER
JSDContext *jsdc;
#endif

    _rpmjs.flags = _rpmjs_options;	/* XXX necessary? */

    gpsee_setVerbosity(isatty(STDERR_FILENO) ? GSR_PREPROGRAM_TTY_VERBOSITY
		       : GSR_PREPROGRAM_NOTTY_VERBOSITY);
    gpsee_openlog(gpsee_basename(argv[0]));

    optCon = rpmioInit(argc, argv, optionsTable);

    /* Add files from CLI. */
    Iargv = (char *const *)poptGetArgs(optCon);
    ac = argvCount((ARGV_t)Iargv);

    switch (ac) {
    case 0:
	if (Icode || Ifn)
 	    break;
	/*@fallthrough@*/
    default:
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
	/*@notreached@*/ break;
    }

    rpmjsLoad(&_rpmjs, Ifn, argc, argv);
    if (F_ISSET(_rpmjs.flags, NOUTF8)
     || (rc_bool_value(rc, "gpsee_force_no_utf8_c_strings") == rc_true)
     || getenv("GPSEE_NO_UTF8_C_STRINGS"))
    {
	JS_DestroyRuntime(JS_NewRuntime(1024));
	putenv((char *) "GPSEE_NO_UTF8_C_STRINGS=1");
    }

    js = rpmjsNew((char **)Iargv, _rpmjs.flags);
    I = js->I;
    realm = I->realm;
    cx = I->cx;

#if defined(GPSEE_DEBUGGER)
    jsdc = gpsee_initDebugger(cx, realm, DEBUGGER_JS);
#endif
    gpsee_setThreadStackLimit(cx, &stackBasePtr,
		strtol(rc_default_value(rc, "gpsee_thread_stack_limit_bytes",
			      "0x80000"), NULL, 0));

    if (verbosity < GSR_MIN_TTY_VERBOSITY && isatty(STDERR_FILENO))
	verbosity = GSR_MIN_TTY_VERBOSITY;

    if (gpsee_verbosity(0) < verbosity)
	gpsee_setVerbosity(verbosity);

    /* Run JavaScript specified with -c */
    if (Icode) {
	const char * result = NULL;
	rpmRC rc = rpmjsRun(js, Icode, &result);
	ec = (rc == RPMRC_OK ? 0 : 1);
	goto finish;
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
		goto finish;
	    }

	    if (!script || !scrobj)
		goto finish;

	    if (!F_ISSET(js->flags, NOEXEC)) {
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
	    goto finish;
    }

    /* Setup for main-script running -- cancel preprogram verbosity and use our own error reporting system in gsr that does not use error reporter */
    gpsee_setVerbosity(verbosity);
    JS_SetOptions(cx, JS_GetOptions(cx) | JSOPTION_DONT_REPORT_UNCAUGHT);

    if (ac != 0)	/* XXX FIXME */
	js->flags |= RPMJS_FLAGS_SKIPSHEBANG;

    if (Ifn == NULL) {
	ec = Icode ? 0 : 1;
    } else {
	FILE *fp = rpmjsOpenFile(js, Ifn, NULL);

	if (fp == NULL) {
	    gpsee_log(cx, SLOG_NOTICE,
		      PRODUCT_SHORTNAME
		      ": Unable to open' script '%s'! (%m)",
		      Ifn);
	    ec = 1;
	    goto finish;
	}

	/* Just compile and exit? */
	if (F_ISSET(js->flags, NOEXEC)) {
	    JSScript *script;
	    JSObject *scrobj;

	    if (!gpsee_compileScript(cx, Ifn,
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

	    if (!gpsee_runProgramModule(cx, Ifn,
			NULL, fp, (char * const*)Iargv, Ienviron))
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
	goto finish;
    }

finish:
#ifdef GPSEE_DEBUGGER
    gpsee_finiDebugger(jsdc);
#endif
    js = rpmjsFree(js);

exit:
    Icode = _free(Icode);
    Ifn = _free(Ifn);

    optCon = rpmioFini(optCon);

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

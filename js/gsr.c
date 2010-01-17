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
#include <argv.h>
#include <poptIO.h>

#define	_RPMJS_INTERNAL
#include <rpmjs.h>

#define PRODUCT_SHORTNAME	"gsr"
#define PRODUCT_VERSION		"1.0-pre1"

#include <prinit.h>
#include "gpsee.h"

#if defined(GPSEE_DARWIN_SYSTEM)
#include <crt_externs.h>
#else
extern char **environ;
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

#define F_ISSET(_js, _FLAG) ((_js)->flags & RPMJS_FLAGS_##_FLAG)

/**
 */
static struct rpmjs_s _js = {
    .flags =
	(RPMJS_FLAGS_STRICT |
	 RPMJS_FLAGS_RELIMIT |
	 RPMJS_FLAGS_ANONFUNFIX |
	 RPMJS_FLAGS_JIT ),
};

static const char * Icode;	/* String with JavaScript program in it */
static const char * Ifn;	/* Filename with JavaScript program in it */
static int verbosity;		/* 0 = no debug, bigger = more debug */
static int gcZeal;

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
    rpmjs js = &_js;

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
    case 'F':
	js->flags |= RPMJS_FLAGS_SKIPSHEBANG;
	Ifn = xstrdup(arg);
	break;
    case 'h':
	poptPrintHelp(con, stderr, 0);
/*@-exitarg@*/
	exit(0);
/*@=exitarg@*/
	break;
#if defined(__SURELYNX__)
    case 'D':
	redirect_output(permanent_pool, arg);
	break;
#ifdef	NOTYET	/* -r file appears defunct */
    case 'r':
#endif
#endif
    case 'd':	verbosity++;	break;
    case 'z':	gcZeal++;	break;
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

  { NULL, 'c', POPT_ARG_STRING,      &Icode, 0,
        N_("Specifies literal JavaScript code to execute"), N_("code") },
  { NULL, 'f', POPT_ARG_STRING,      &Ifn, 0,
        N_("Specifies the filename containing code to run"), N_("filename") },
  { NULL, 'F', POPT_ARG_STRING,      NULL, 'F',
        N_("Like -f, but skip shebang if present"), N_("filename") },
  { NULL, 'h', POPT_ARG_NONE,      NULL, 'h',
        N_("Display this help"), NULL },
  { "noexec", 'n', POPT_BIT_SET,	&_js.flags, RPMJS_FLAGS_NOEXEC,
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

  { "allow", 'a', POPT_BIT_SET,		&_js.flags, RPMJS_FLAGS_ALLOW,
        N_("Allow (read-only) access to caller's environmen"), NULL },
  { "nocache", 'C', POPT_BIT_SET,	&_js.flags, RPMJS_FLAGS_NOCACHE,
        N_("Disables compiler caching via JSScript XDR serialization"), NULL },
  { "loadrc", 'R', POPT_BIT_SET,	&_js.flags, RPMJS_FLAGS_LOADRC,
        N_("Load RC file for interpreter (" PRODUCT_SHORTNAME ") based on script filename."), NULL },
  { "nowarn", 'W', POPT_BIT_SET,	&_js.flags, RPMJS_FLAGS_NOWARN,
        N_("Do not report warnings"), NULL },

  { "norelimit", 'e', POPT_BIT_CLR,	&_js.flags, RPMJS_FLAGS_RELIMIT,
        N_("Do not limit regexps to n^3 levels of backtracking"), NULL },
  { "nojit", 'J', POPT_BIT_CLR,		&_js.flags, RPMJS_FLAGS_JIT,
        N_("Disable nanojit"), NULL },
  { "nostrict", 'S', POPT_BIT_CLR,	&_js.flags, RPMJS_FLAGS_STRICT,
        N_("Disable Strict mode"), NULL },
  { "noutf8", 'U', POPT_BIT_SET,	&_js.flags, RPMJS_FLAGS_NOUTF8,
        N_("Disable UTF-8 C string processing"), NULL },
  { "xml", 'x', POPT_BIT_SET,		&_js.flags, RPMJS_FLAGS_XML,
        N_("Parse <!-- comments --> as E4X tokens"), NULL },

  { "anonfunfix", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,	&_js.flags, RPMJS_FLAGS_ANONFUNFIX,
        N_("Parse //@line number [\"filename\"] for XUL"), NULL },
  { "atline", 'A', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,	&_js.flags, RPMJS_FLAGS_ATLINE,
        N_("Parse //@line number [\"filename\"] for XUL"), NULL },
  { "werror", 'w', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,	&_js.flags, RPMJS_FLAGS_WERROR,
        N_("Convert warnings to errors"), NULL },

  { NULL, 'd', POPT_ARG_NONE,		NULL, 'd',
        N_("Increase verbosity"), NULL },
  { "gczeal", 'z', POPT_ARG_NONE,	NULL, 'z',
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
As an interpreter: #!/usr/bin/gsr {-/*flags*/}\n\
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

static FILE *openScriptFile(rpmjs js, const char * fn)
{
    FILE * fp = fopen(fn, "r");
    gpsee_interpreter_t * I = js->I;
    char line[64];		/* #! args can be longer than 64 on some unices, but no matter */

    if (!fp)
	return NULL;

    gpsee_flock(fileno(fp), GPSEE_LOCK_SH);

    if (!F_ISSET(js, SKIPSHEBANG))
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

/** Load RC file based on filename.
 *  Filename to use is script's filename, if -R option flag is thrown.
 *  Otherwise, file interpreter's filename (i.e. gsr) is used.
 */
void
loadRuntimeConfig(rpmjs js, const char * fn, int argc, char *const *argv)
{
    const char *scriptFilename = fn;
    rcFILE *rc_file;

    if (F_ISSET(js, LOADRC)) {
	char fake_argv_zero[FILENAME_MAX];
	char *fake_argv[2];
	char *s;

	if (!scriptFilename)
	    fatal
		("Cannot use script-filename RC file when script-filename is unknown!");

	fake_argv[0] = fake_argv_zero;

	gpsee_cpystrn(fake_argv_zero, scriptFilename,
		      sizeof(fake_argv_zero));

	if ((s = strstr(fake_argv_zero, ".js")) && (s[3] == '\0'))
	    *s = '\0';

	if ((s = strstr(fake_argv_zero, ".es3")) && (s[4] == '\0'))
	    *s = '\0';

	if ((s = strstr(fake_argv_zero, ".e4x")) && (s[4] == '\0'))
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
    rpmjs js = &_js;
    char *const * Iargv;	/* Becomes arguments array in JS program */
    char *const * Ienviron = NULL;	/* Environment to pass to script */
    int ac = 0;
    int ec;

    verbosity = max(whenSureLynx(sl_get_debugLevel(), 0), gpsee_verbosity(0));

    gpsee_openlog(gpsee_basename(argv[0]));

#if defined(__SURELYNX__)
    permanent_pool = apr_initRuntime();
#endif

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

    loadRuntimeConfig(js, Ifn, argc, argv);
    if (F_ISSET(js, NOUTF8)
     || (rc_bool_value(rc, "gpsee_force_no_utf8_c_strings") == rc_true)
     || getenv("GPSEE_NO_UTF8_C_STRINGS"))
    {
	JS_DestroyRuntime(JS_NewRuntime(1024));
	putenv((char *) "GPSEE_NO_UTF8_C_STRINGS=1");
    }

    js = rpmjsNew((const char **)Iargv, js->flags);

    /* Run JavaScript specified with -c */
    if (Icode) {
	const char * result = NULL;
	rpmRC rc = rpmjsRun(js, Icode, &result);
	ec = (rc == RPMRC_OK ? 0 : 1);
	goto finish;
    }

#if !defined(SYSTEM_GSR)
#define	SYSTEM_GSR	"/usr/bin/gsr"
#endif
    if ((argv[0][0] == '/') && (strcmp(argv[0], SYSTEM_GSR) != 0)
     && rc_bool_value(rc, "no_gsr_preload_script") != rc_true)
    {
	gpsee_interpreter_t * I = js->I;
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
		goto finish;
	    }

	    if (!script || !scrobj)
		goto finish;

	    JS_AddNamedRoot(I->cx, &scrobj, "preload_scrobj");
	    JS_ExecuteScript(I->cx, I->globalObj, script, &v);
	    if (JS_IsExceptionPending(I->cx)) {
		I->exitType = et_exception;
		JS_ReportPendingException(I->cx);
	    }
	    JS_RemoveRoot(I->cx, &scrobj);
	}

	if (I->exitType & et_exception)
	    goto finish;
    }

    if (ac != 0)	/* XXX FIXME */
	js->flags |= RPMJS_FLAGS_SKIPSHEBANG;

    if (Ifn == NULL) {
	ec = Icode ? 0 : 1;
    } else {
	FILE * fp = openScriptFile(js, Ifn);
	if (fp == NULL) {
	    gpsee_log(SLOG_NOTICE,
		      PRODUCT_SHORTNAME
		      ": Unable to open' script '%s'! (%m)",
		      Ifn);
	    ec = 1;
	} else {
	    gpsee_interpreter_t * I = js->I;
	    gpsee_runProgramModule(I->cx, Ifn, fp);
	    fclose(fp);
	    if ((I->exitType & et_successMask) == I->exitType)
		ec = I->exitCode;
	    else
		ec = 1;
	}
    }

finish:
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

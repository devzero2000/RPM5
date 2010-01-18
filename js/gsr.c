
#include "system.h"

#include <rpmiotypes.h>
#include <rpmio.h>
#include <rpmlog.h>
#include <argv.h>
#include <poptIO.h>

#define	_RPMJS_INTERNAL
#include <rpmjs.h>

#include "gpsee.h"

#include "debug.h"

extern const char * __progname;

/*==============================================================*/

#define F_ISSET(_js, _FLAG) ((_js)->flags & RPMJS_FLAGS_##_FLAG)

#define _RPMJS_OPTIONS  \
    (RPMJS_FLAGS_STRICT | RPMJS_FLAGS_RELIMIT | RPMJS_FLAGS_ANONFUNFIX | RPMJS_FLAGS_JIT)

extern struct rpmjs_s _rpmgsr;

static const char * Icode;	/* String with JavaScript program in it */
static const char * Ifn;	/* Filename with JavaScript program in it */
static int _debug;		/* 0 = no debug, bigger = more debug */

/*==============================================================*/

/** GPSEE uses panic() to panic, expects embedder to provide */
void __attribute__ ((noreturn)) panic(const char *message)
{
    rpmlog(RPMLOG_EMERG, "%s: Fatal Error: %s\n", __progname, message);
    exit(1);
}

/*==============================================================*/
static void rpmgsrArgCallback(poptContext con,
                /*@unused@*/ enum poptCallbackReason reason,
                const struct poptOption * opt, /*@unused@*/ const char * arg,
                /*@unused@*/ void * data)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    rpmjs js = &_rpmgsr;

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
    case 'F':
	js->flags |= RPMJS_FLAGS_SKIPSHEBANG;
	Ifn = xstrdup(arg);
	break;
    case 'd':	_debug++;	break;
    case 'z':	_rpmjs_zeal++;	break;
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
        N_("Specifies literal JavaScript code to execute"), N_(" code") },
  { NULL, 'f', POPT_ARG_STRING,      &Ifn, 0,
        N_("Specifies the filename containing code to run"), N_(" filename") },
  { NULL, 'F', POPT_ARG_STRING,      NULL, 'F',
        N_("Like -f, but skip shebang if present"), N_(" filename") },
  { "noexec", 'n', POPT_BIT_SET,	&_rpmgsr.flags, RPMJS_FLAGS_NOEXEC,
	N_("Engine will load and parse, but not run, the script"), NULL },
  { "debug", 'd', POPT_ARG_NONE|POPT_ARGFLAG_DOC_HIDDEN,	NULL, 'd',
        N_("Increase debuuging"), NULL },
  { "zeal", 'z', POPT_ARG_NONE,	NULL, 'z',
        N_("Increase GC Zealousness"), NULL },

  POPT_TABLEEND
};

extern struct poptOption rpmjsIPoptTable[];

static struct poptOption _optionsTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmgsrArgCallback, 0, NULL, NULL },
/*@=type@*/

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, _gsrOptionsTable, 0,
        N_("Command options:"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmjsIPoptTable, 0,
        N_("JS interpreter options:"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"), NULL },

  POPT_AUTOALIAS
  POPT_AUTOHELP

  POPT_TABLEEND
};

static struct poptOption *optionsTable = &_optionsTable[0];

/*==============================================================*/

int main(int argc, char *argv[])
{
    poptContext optCon;
    rpmjs js = &_rpmgsr;
    char *const * Iargv;	/* Becomes arguments array in JS program */
    int ac = 0;
    int ec = 1;		/* assume failure */

    js->flags = _rpmjs_options | _RPMJS_OPTIONS;

    _debug = gpsee_verbosity(0);

    _rpmio_popt_context_flags = POPT_CONTEXT_POSIXMEHARDER;
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

    if (F_ISSET(js, NOUTF8) || getenv("GPSEE_NO_UTF8_C_STRINGS"))
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

    /* Pre-compile JavaScript specified with -f */
    /* Run JavaScript specified with -f */
#if !defined(SYSTEM_GSR)
#define	SYSTEM_GSR	"/usr/bin/gsr"
#endif
    if ((argv[0][0] == '/') && strcmp(argv[0], SYSTEM_GSR)) {
	gpsee_interpreter_t * I = js->I;
	const char * preloadfn =
		rpmGetPath(dirname(argv[0]), "/.", basename(argv[0]), NULL);

	if (!(preloadfn && *preloadfn)) {
	    rpmlog(RPMLOG_EMERG,
		"%s: Unable to create preload script filename!\n", __progname);
	    preloadfn = _free(preloadfn);
	    goto finish;
	}
	errno = 0;

	if (access(preloadfn, F_OK) == 0) {
	    jsval v;
	    const char *errmsg;
	    JSScript *script;
	    JSObject *scrobj;

	    if (gpsee_compileScript
		(I->cx, preloadfn, NULL, &script,
		 I->globalObj, &scrobj, &errmsg)) {
		rpmlog(RPMLOG_EMERG,
			  "%s: Unable to compile preload script '%s' - %s\n",
			  __progname, preloadfn, errmsg);
		preloadfn = _free(preloadfn);
		goto finish;
	    }
	    preloadfn = _free(preloadfn);

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

    if (Ifn == NULL) {
	ec = Icode ? 0 : 1;
	goto finish;
    }

    /* Run (pre-compiled) JavaScript specified with -f */
  { gpsee_interpreter_t * I = js->I;
    const char * result = NULL;

    if (ac != 0)	/* XXX FIXME */
	js->flags |= RPMJS_FLAGS_SKIPSHEBANG;

    switch (rpmjsRunFile(js, Ifn, &result)) {
    default:
	rpmlog(RPMLOG_ERR, "%s: Unable to open' script '%s'! (%s)\n",
		      __progname, Ifn, result);
	result = _free(result);
	ec = 1;
	break;
    case RPMRC_OK:
	ec = ((I->exitType & et_successMask) == I->exitType)
		? I->exitCode : 1;
	break;
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

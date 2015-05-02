#include "system.h"

#include "rpmio_internal.h"
#include <argv.h>
#include <popt.h>

#if defined(__APPLE__)
#include <crt_externs.h>
#else
extern char ** environ;
#endif

#ifdef  __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++11-extensions"
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#pragma clang diagnostic ignored "-Wuninitialized"
#endif

#if defined(WITH_GPSEE) || defined(WITH_MOZJS185) || defined(WITH_MOZJS24) || defined(WITH_MOZJS31)
#define	WITH_MOZJS	1
#endif

#if defined(WITH_MOZJS)
#define	JS_THREADSAFE	1
#include <jsapi.h>
#endif	/* WITH_MOZJS */

#define _RPMJS_INTERNAL
#include "rpmjs.h"

#include "debug.h"

#define F_ISSET(_flags, _FLAG) ((_flags) & RPMJS_FLAGS_##_FLAG)

int _rpmjs_debug = 0;

#define RPMJSDBG(_t, _l) \
  if ((_t) || _rpmjs_debug) fprintf _l

rpmjs _rpmjsI = NULL;

uint32_t _rpmjs_options = 0;

int _rpmjs_zeal = 2;

struct rpmjs_s __rpmjs;
#ifdef	NOTYET
rpmjs _rpmjs = &_rpmjs;
#endif

static int rpmjs_nopens;

/*==============================================================*/
#if defined(WITH_MOZJS)

typedef struct JSI_s * JSI_t;
struct JSI_s {
    JSRuntime	*rt;
    JSContext	*cx;
    JSObject	*global;
};

static void
rpmjsReportError(JSContext *cx, const char *message, JSErrorReport *report)
{
    fprintf(stderr, "%s:%u:%s\n",
	report->filename ? report->filename : "<no filename=\"filename\">",
	(unsigned int) report->lineno, message);
}

/*==============================================================*/
static JSBool compileOnly = JS_FALSE;

static JSBool
Version(JSContext *cx, uintN argc, jsval *vp)
{
    jsval *argv = JS_ARGV(cx, vp);
    if (argc > 0 && JSVAL_IS_INT(argv[0]))
        *vp = INT_TO_JSVAL(JS_SetVersion(cx, (JSVersion) JSVAL_TO_INT(argv[0])));
    else
        *vp = INT_TO_JSVAL(JS_GetVersion(cx));
    return JS_TRUE;
}

static JSBool
Load(JSContext *cx, uintN argc, jsval *vp)
{
    jsval *argv = JS_ARGV(cx, vp);
    JSObject *thisobj = JS_THIS_OBJECT(cx, vp);
    if (!thisobj)
        return JS_FALSE;

    for (uintN i = 0; i < argc; i++) {
        JSString *str = JS_ValueToString(cx, argv[i]);
        if (!str)
            return false;
        argv[i] = STRING_TO_JSVAL(str);
        JSAutoByteString filename(cx, str);
        if (!filename)
            return JS_FALSE;
        errno = 0;
        uint32 oldopts = JS_GetOptions(cx);
        JS_SetOptions(cx, oldopts | JSOPTION_COMPILE_N_GO | JSOPTION_NO_SCRIPT_RVAL);
        JSObject *scriptObj = JS_CompileFile(cx, thisobj, filename.ptr());
        JS_SetOptions(cx, oldopts);
        if (!scriptObj)
            return false;

        if (!compileOnly && !JS_ExecuteScript(cx, thisobj, scriptObj, NULL))
            return false;
    }

    return true;
}

static JSBool
Evaluate(JSContext *cx, uintN argc, jsval *vp)
{
    if (argc != 1 || !JSVAL_IS_STRING(JS_ARGV(cx, vp)[0])) {
#ifdef	FIXME
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL,
                             (argc != 1) ? JSSMSG_NOT_ENOUGH_ARGS : JSSMSG_INVALID_ARGS,
                             "evaluate");
#endif
        return false;
    }

    JSString *code = JSVAL_TO_STRING(JS_ARGV(cx, vp)[0]);

    size_t codeLength;
    const jschar *codeChars = JS_GetStringCharsAndLength(cx, code, &codeLength);
    if (!codeChars)
        return false;

    JSObject *thisobj = JS_THIS_OBJECT(cx, vp);
    if (!thisobj)
        return false;

    if ((JS_GET_CLASS(cx, thisobj)->flags & JSCLASS_IS_GLOBAL) != JSCLASS_IS_GLOBAL) {
#ifdef	FIXME
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_UNEXPECTED_TYPE,
                             "this-value passed to evaluate()", "not a global object");
#endif
        return false;
    }

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_EvaluateUCScript(cx, thisobj, codeChars, codeLength, "@evaluate", 0, NULL);
}

static JSString *
FileAsString(JSContext *cx, const char *pathname)
{
    FILE *file;
    JSString *str = NULL;
    size_t len, cc;
    char *buf;

    file = fopen(pathname, "rb");
    if (!file) {
        JS_ReportError(cx, "can't open %s: %s", pathname, strerror(errno));
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) == EOF) {
        JS_ReportError(cx, "can't seek end of %s", pathname);
    } else {
        len = ftell(file);
        if (fseek(file, 0, SEEK_SET) == EOF) {
            JS_ReportError(cx, "can't seek start of %s", pathname);
        } else {
            buf = (char*) JS_malloc(cx, len + 1);
            if (buf) {
                cc = fread(buf, 1, len, file);
                if (cc != len) {
                    JS_ReportError(cx, "can't read %s: %s", pathname,
                                   (ptrdiff_t(cc) < 0) ? strerror(errno) : "short read");
                } else {
                    len = (size_t)cc;
                    str = JS_NewStringCopyN(cx, buf, len);
                }
                JS_free(cx, buf);
            }
        }
    }
    fclose(file);

    return str;
}

/*
 * Function to run scripts and return compilation + execution time. Semantics
 * are closely modelled after the equivalent function in WebKit, as this is used
 * to produce benchmark timings by SunSpider.
 */
static JSBool
Run(JSContext *cx, uintN argc, jsval *vp)
{
    if (argc != 1) {
#ifdef	FIXME
        JS_ReportErrorNumber(cx, my_GetErrorMessage, NULL, JSSMSG_INVALID_ARGS, "run");
#endif
        return false;
    }

    JSObject *thisobj = JS_THIS_OBJECT(cx, vp);
    if (!thisobj)
        return false;

    jsval *argv = JS_ARGV(cx, vp);
    JSString *str = JS_ValueToString(cx, argv[0]);
    if (!str)
        return false;
    argv[0] = STRING_TO_JSVAL(str);
    JSAutoByteString filename(cx, str);
    if (!filename)
        return false;

    const jschar *ucbuf = NULL;
    size_t buflen;
    str = FileAsString(cx, filename.ptr());
    if (str)
        ucbuf = JS_GetStringCharsAndLength(cx, str, &buflen);
    if (!ucbuf)
        return false;

    JS::Anchor<JSString *> a_str(str);
    uint32 oldopts = JS_GetOptions(cx);
    JS_SetOptions(cx, oldopts | JSOPTION_COMPILE_N_GO | JSOPTION_NO_SCRIPT_RVAL);

#ifdef	FIXME
    int64 startClock = PRMJ_Now();
#endif
    JSObject *scriptObj = JS_CompileUCScript(cx, thisobj, ucbuf, buflen, filename.ptr(), 1);
    JS_SetOptions(cx, oldopts);
    if (!scriptObj || !JS_ExecuteScript(cx, thisobj, scriptObj, NULL))
        return false;

#ifdef	FIXME
    int64 endClock = PRMJ_Now();
    JS_SET_RVAL(cx, vp, DOUBLE_TO_JSVAL((endClock - startClock) / double(PRMJ_USEC_PER_MSEC)));
#endif
    return true;
}

#ifdef	NOTYET
/*
 * function readline()
 * Provides a hook for scripts to read a line from stdin.
 */
static JSBool
ReadLine(JSContext *cx, uintN argc, jsval *vp)
{
#define BUFSIZE 256
    FILE *from;
    char *buf, *tmp;
    size_t bufsize, buflength, gotlength;
    JSBool sawNewline;
    JSString *str;

    from = stdin;
    buflength = 0;
    bufsize = BUFSIZE;
    buf = (char *) JS_malloc(cx, bufsize);
    if (!buf)
        return JS_FALSE;

    sawNewline = JS_FALSE;
    while ((gotlength =
            js_fgets(buf + buflength, bufsize - buflength, from)) > 0) {
        buflength += gotlength;

        /* Are we done? */
        if (buf[buflength - 1] == '\n') {
            buf[buflength - 1] = '\0';
            sawNewline = JS_TRUE;
            break;
        } else if (buflength < bufsize - 1) {
            break;
        }

        /* Else, grow our buffer for another pass. */
        bufsize *= 2;
        if (bufsize > buflength) {
            tmp = (char *) JS_realloc(cx, buf, bufsize);
        } else {
            JS_ReportOutOfMemory(cx);
            tmp = NULL;
        }

        if (!tmp) {
            JS_free(cx, buf);
            return JS_FALSE;
        }

        buf = tmp;
    }

    /* Treat the empty string specially. */
    if (buflength == 0) {
        *vp = feof(from) ? JSVAL_NULL : JS_GetEmptyStringValue(cx);
        JS_free(cx, buf);
        return JS_TRUE;
    }

    /* Shrink the buffer to the real size. */
    tmp = (char *) JS_realloc(cx, buf, buflength);
    if (!tmp) {
        JS_free(cx, buf);
        return JS_FALSE;
    }

    buf = tmp;

    /*
     * Turn buf into a JSString. Note that buflength includes the trailing null
     * character.
     */
    str = JS_NewStringCopyN(cx, buf, sawNewline ? buflength - 1 : buflength);
    JS_free(cx, buf);
    if (!str)
        return JS_FALSE;

    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}
#endif

static JSBool
PutStr(JSContext *cx, uintN argc, jsval *vp)
{
    FILE * gOutFile = stdout;
    jsval * argv = JS_ARGV(cx, vp);
    JSString *str;
    char *bytes;

    if (argc != 0) {
        str = JS_ValueToString(cx, argv[0]);
        if (!str)
            return JS_FALSE;
        bytes = JS_EncodeString(cx, str);
        if (!bytes)
            return JS_FALSE;
        fputs(bytes, gOutFile);
        JS_free(cx, bytes);
        fflush(gOutFile);
    }

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
Now(JSContext *cx, uintN argc, jsval *vp)
{
#ifdef	FIXME
    jsdouble now = PRMJ_Now() / double(PRMJ_USEC_PER_MSEC);
#else
    jsdouble now = time(NULL) * 1000;
#endif
    JS_SET_RVAL(cx, vp, DOUBLE_TO_JSVAL(now));
    return true;
}

static JSBool
Print(JSContext *cx, uintN argc, jsval *vp)
{
    FILE * gOutFile = stdout;
    jsval * argv = JS_ARGV(cx, vp);
    uintN i;
    JSString *str;
    char *bytes;

    for (i = 0; i < argc; i++) {
        str = JS_ValueToString(cx, argv[i]);
        if (!str)
            return JS_FALSE;
        bytes = JS_EncodeString(cx, str);
        if (!bytes)
            return JS_FALSE;
        fprintf(gOutFile, "%s%s", i ? " " : "", bytes);
        JS_free(cx, bytes);
    }

    fputc('\n', gOutFile);
    fflush(gOutFile);

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
Require(JSContext *cx, uintN argc, jsval *vp)
{
    jsval * argv = JS_ARGV(cx, vp);
if (_rpmjs_debug < 0)
fprintf(stderr, "==> %s(%p,%p[%u],%p)\n", __FUNCTION__, cx, argv, (unsigned)argc, vp);

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSFunctionSpec global_functions[] = {
    JS_FN("version",	Version,	0,0),
    JS_FN("load",	Load,		1,0),
    JS_FN("evaluate",	Evaluate,	1,0),
    JS_FN("run",	Run,		1,0),
#ifdef	NOTYET
    JS_FN("readline",	ReadLine,	0,0),
#endif
    JS_FN("print",	Print,		0,0),
    JS_FN("putstr",	PutStr,		0,0),
    JS_FN("dateNow",	Now,		0,0),
    JS_FS("require",	Require,	0,0),
    JS_FS_END
};
/*==============================================================*/

#if defined(WITH_MOZJS185)
static const JSClass global_class = {
    "global", JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};
#endif
#if defined(WITH_MOZJS24) || defined(WITH_MOZJS31)
static const JSClass global_class = {
    "global", JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub,  JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
};
#endif

static void mozFini(rpmjs js)
{
    JSI_t I = (JSI_t) js->I;

#if defined(WITH_GPSEE)

    (void) gpsee_destroyInterpreter(I);

#else	/* WITH_GPSEE */

    if (I->cx)
	JS_DestroyContext(I->cx);
    I->cx = NULL;
    if (I->rt)
	JS_DestroyRuntime(I->rt);
    I->rt = NULL;
    if (--rpmjs_nopens <= 0)  {
	JS_ShutDown();
	rpmjs_nopens = 0;
    }
    free(I);
#endif	/* WITH_GPSEE */
}

static JSI_t mozInit(rpmjs js)
{
    JSI_t I = NULL;
    uint32_t flags = js->flags;

#if defined(WITH_GPSEE)

    if (F_ISSET(flags, NOUTF8) || getenv("GPSEE_NO_UTF8_C_STRINGS")) {
	JS_DestroyRuntime(JS_NewRuntime(1024));
	putenv((char *) "GPSEE_NO_UTF8_C_STRINGS=1");
    }

    /* XXX FIXME: js->Iargv/js->Ienviron for use by rpmjsRunFile() */
    I = gpsee_createInterpreter();

#ifdef	NOTYET	/* FIXME: dig out where NOCACHE has moved. */
    if (F_ISSET(flags, NOCACHE))
	I->useCompilerCache = 0;
#endif
    if (F_ISSET(flags, NOWARN)) {
	gpsee_runtime_t * grt = JS_GetRuntimePrivate(JS_GetRuntime(I->cx));
	grt->errorReport |= er_noWarnings;
    }

    JS_SetOptions(I->cx, (flags & 0xffff));
#if defined(JS_GC_ZEAL)
    JS_SetGCZeal(I->cx, _rpmjs_zeal);
#endif

#else	/* WITH_GPSEE */

    static uint32_t _maxbytes = 8L * 1024L * 1024L;
    static size_t _stackChunkSize = 8192;
    JSClass * _clasp = (JSClass *)&global_class;
    JSFunctionSpec * _functions = global_functions;
    JSPrincipals * _principals = NULL;
    JSBool ok;

    if (rpmjs_nopens++ == 0) {
#if defined(WITH_MOZJS31)
	JS_Init();
#endif
    }

    I = (JSI_t) xcalloc(1, sizeof(*I));

#if defined(WITH_MOZJS185)
    I->rt = JS_NewRuntime(_maxbytes);
#endif
#if defined(WITH_MOZJS24) || defined(WITH_MOZJS31)
    I->rt = JS_NewRuntime(_maxbytes, JS_NO_HELPER_THREADS);
#endif
assert(I->rt);
    JS_SetRuntimePrivate(I->rt, (void *)js);

    I->cx = JS_NewContext(I->rt, _stackChunkSize);
assert(I->cx);
    JS_SetContextPrivate(I->cx, (void *)js);

#if defined(WITH_MOZJS185)
    JS_SetOptions(I->cx,
		JSOPTION_VAROBJFIX | JSOPTION_JIT | JSOPTION_METHODJIT);
    JS_SetVersion(I->cx, JSVERSION_LATEST);
    JS_SetErrorReporter(I->cx, rpmjsReportError);
#endif

#if defined(WITH_MOZJS185)
    I->global = JS_NewCompartmentAndGlobalObject(I->cx,
			_clasp, _principals);
#endif
#if defined(WITH_MOZJS24) || defined(WITH_MOZJS31)
    JS::RootedObject global(I->cx, JS_NewGlobalObject(I->cx,
		_clasp, _principals));
    I->global = global;
#endif
assert(I->global);
    JS_SetGlobalObject(I->cx, I->global);

#if defined(WITH_MOZJS185)
    ok = JS_InitStandardClasses(I->cx, I->global);
assert(ok);
#ifdef	JS_HAS_CTYPES
    ok = JS_InitCTypesClass(I->cx, I->global);
assert(ok);
#endif
#endif

#if defined(WITH_MOZJS24) || defined(WITH_MOZJS31)
    JSAutoCompartment ac(I->cx, I->global);
    ok = JS_InitStandardClasses(I->cx, I->global);
assert(ok);
#endif

    ok = JS_DefineFunctions(I->cx, I->global, _functions);
assert(ok);

#endif	/* WITH_GPSEE */

    return I;
}
#endif	/* WITH_MOZJS */

/*==============================================================*/
struct poptOption rpmjsIPoptTable[] = {
  { "allow", 'a', POPT_BIT_SET,		&__rpmjs.flags, RPMJS_FLAGS_ALLOW,
        N_("Allow (read-only) access to caller's environment"), NULL },
  { "nocache", 'C', POPT_BIT_SET,	&__rpmjs.flags, RPMJS_FLAGS_NOCACHE,
        N_("Disables compiler caching via JSScript XDR serialization"), NULL },
  { "loadrc", 'R', POPT_BIT_SET,	&__rpmjs.flags, RPMJS_FLAGS_LOADRC,
        N_("Load RC file for interpreter based on script filename."), NULL },
  { "nowarn", 'W', POPT_BIT_SET,	&__rpmjs.flags, RPMJS_FLAGS_NOWARN,
        N_("Do not report warnings"), NULL },

  { "norelimit", 'e', POPT_BIT_CLR,	&__rpmjs.flags, RPMJS_FLAGS_RELIMIT,
        N_("Do not limit regexps to n^3 levels of backtracking"), NULL },
  { "nojit", 'J', POPT_BIT_CLR,		&__rpmjs.flags, RPMJS_FLAGS_JIT,
        N_("Disable nanojit"), NULL },
  { "nostrict", 'S', POPT_BIT_CLR,	&__rpmjs.flags, RPMJS_FLAGS_STRICT,
        N_("Disable Strict mode"), NULL },
  { "noutf8", 'U', POPT_BIT_SET,	&__rpmjs.flags, RPMJS_FLAGS_NOUTF8,
        N_("Disable UTF-8 C string processing"), NULL },
  { "xml", 'x', POPT_BIT_SET,		&__rpmjs.flags, RPMJS_FLAGS_XML,
        N_("Parse <!-- comments --> as E4X tokens"), NULL },

  { "anonfunfix", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmjs.flags, RPMJS_FLAGS_ANONFUNFIX,
        N_("Parse //@line number [\"filename\"] for XUL"), NULL },
  { "atline", 'A', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmjs.flags, RPMJS_FLAGS_ATLINE,
        N_("Parse //@line number [\"filename\"] for XUL"), NULL },
  { "werror", 'w', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmjs.flags, RPMJS_FLAGS_WERROR,
        N_("Convert warnings to errors"), NULL },

  POPT_TABLEEND
};

static void rpmjsFini(void * _js)
{
    rpmjs js = (rpmjs) _js;

RPMJSDBG(0, (stderr, "==> %s(%p) I %p\n", __FUNCTION__, js, js->I));

#if defined(WITH_MOZJS)
    mozFini(js);
#endif
    js->I = NULL;
}

rpmioPool _rpmjsPool;

static rpmjs rpmjsGetPool(rpmioPool pool)
{
    rpmjs js;

    if (_rpmjsPool == NULL) {
	_rpmjsPool = rpmioNewPool("js", sizeof(*js), -1, _rpmjs_debug,
			NULL, NULL, rpmjsFini);
	pool = _rpmjsPool;
    }
    return (rpmjs) rpmioGetPool(pool, sizeof(*js));
}

static rpmjs rpmjsI(void)
{
    if (_rpmjsI == NULL)
	_rpmjsI = rpmjsNew(NULL, 0);

RPMJSDBG(0, (stderr, "<== %s() _rpmjsI %p\n", __FUNCTION__, _rpmjsI));
    return _rpmjsI;
}

/* XXX FIXME: Iargv/Ienviron are now associated with running. */
rpmjs rpmjsNew(char ** av, uint32_t flags)
{
    rpmjs js =
#ifdef	NOTYET
	(flags & 0x80000000) ? rpmjsI() :
#endif
	rpmjsGetPool(_rpmjsPool);

    if (flags == 0)
	flags = _rpmjs_options;
    js->flags = flags;

#if defined(WITH_MOZJS)
    js->I = mozInit(js);
#endif

    return rpmjsLink(js);
}

static FILE * rpmjsOpenFile(rpmjs js, const char * fn, const char ** msgp)
	/*@modifies js @*/
{
    FILE * fp = NULL;

    fp = fopen(fn, "r");
    if (fp == NULL || ferror(fp)) {
	if (fp) {
	    if (msgp)
		*msgp = xstrdup(strerror(errno));
	    (void) fclose(fp);
	    fp = NULL;
	} else {
	    if (msgp)	/* XXX FIXME: add __FUNCTION__ identifier? */
		*msgp = xstrdup("unknown error");
	}
	goto exit;
    }

#if defined(WITH_GPSEE)
    gpsee_flock(fileno(fp), GPSEE_LOCK_SH);
#endif

    if (F_ISSET(js->flags, SKIPSHEBANG)) {
	char buf[BUFSIZ];
	
	if (fgets(buf, sizeof(buf), fp)) {
	    if (!(buf[0] == '#' && buf[1] == '!')) {
		/* XXX FIXME: return through *msgp */
		rpmlog(RPMLOG_WARNING, "%s: %s: no \'#!\' on 1st line\n",
			__FUNCTION__, fn);
		rewind(fp);
	    } else {
#ifdef	NOTYET	/* XXX FIXME */
gpsee_interpreter_t * I = js->I;
		I->linenoOffset += 1;
#endif	/* NOTYET */
		do {	/* consume entire first line, regardless of length */
		    if (strchr(buf, '\n'))
			break;
		} while (fgets(buf, sizeof(buf), fp));
		/*
		 * Make spidermonkey think the script starts with a blank line,
		 * to keep line numbers in sync.
		 */
		ungetc('\n', fp);
	    }
	}
    }

exit:

RPMJSDBG(0, (stderr, "<== %s(%p,%s,%p) fp %p\n", __FUNCTION__, js, fn, msgp, fp));

    return fp;
}

rpmRC rpmjsRunFile(rpmjs js, const char * fn,
		char *const * Iargv,
		const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

    if (js == NULL) js = rpmjsI();

    if (fn != NULL) {
	FILE * fp = rpmjsOpenFile(js, fn, resultp);
#if defined(WITH_GPSEE)
	gpsee_interpreter_t * I = (gpsee_interpreter_t *) js->I;
#endif	/* WITH_GPSEE */

	if (fp == NULL) {
#if defined(WITH_GPSEE)
	    I->grt->exitType = et_execFailure;
#endif	/* WITH_GPSEE */
	    /* XXX FIXME: strerror in *resultp */
	    goto exit;
	}

#ifdef	NOTYET	/* XXX FIXME */
	processInlineFlags(js, fp, &verbosity);
	gpsee_setVerbosity(verbosity);
#endif

	/* Just compile and exit? */
	if (F_ISSET(js->flags, NOEXEC)) {
#
#if defined(WITH_GPSEE)
	    JSScript *script = NULL;
	    JSObject *scrobj = NULL;

	    if (!gpsee_compileScript(I->cx, fn,
			fp, NULL, &script, I->realm->globalObject, &scrobj))
	    {
		I->grt->exitType = et_exception;
		/* XXX FIXME: isatty(3) */
		gpsee_reportUncaughtException(I->cx, JSVAL_NULL,
			(gpsee_verbosity(0) >= GSR_FORCE_STACK_DUMP_VERBOSITY)
			||
			((gpsee_verbosity(0) >= GPSEE_ERROR_OUTPUT_VERBOSITY)
				&& isatty(STDERR_FILENO)));
	    } else {
		I->grt->exitType = et_finished;
		rc = RPMRC_OK;
	    }
#endif	/* WITH_GPSEE */

	} else {
	    char *const * Ienviron = NULL;

	    if (F_ISSET(js->flags, ALLOW)) {
#if defined(__APPLE__)
		Ienviron = (char *const *) _NSGetEnviron();
#else
		Ienviron = environ;
#endif
	    }

#if defined(WITH_GPSEE)
	    I->grt->exitType = et_execFailure;
	    if (gpsee_runProgramModule(I->cx, fn,
			NULL, fp, Iargv, Ienviron) == JS_FALSE)
	    {
		int code = gpsee_getExceptionExitCode(I->cx);
		if (code >= 0) {
		    I->grt->exitType = et_requested;
		    I->grt->exitCode = code;
		    /* XXX FIXME: format and return code in *resultp. */
		    /* XXX hack tp get code into rc -> ec by negating */
		    rc = -code;
		} else
		if (JS_IsExceptionPending(I->cx)) {
		    /* XXX FIXME: isatty(3) */
		    gpsee_reportUncaughtException(I->cx, JSVAL_NULL,
			(gpsee_verbosity(0) >= GSR_FORCE_STACK_DUMP_VERBOSITY)
			||
			((gpsee_verbosity(0) >= GPSEE_ERROR_OUTPUT_VERBOSITY)
				&& isatty(STDERR_FILENO)));
		}
	    } else {
		I->grt->exitType = et_finished;
		rc = RPMRC_OK;
	    }
#endif	/* WITH_GPSEE */

	}
	fclose(fp);
	fp = NULL;
    }

exit:

RPMJSDBG(0, (stderr, "<== %s(%p,%s) rc %d |%s|\n", __FUNCTION__, js, fn, rc, (resultp ? *resultp :"")));

    return rc;
}
rpmRC rpmjsRun(rpmjs js, const char * str, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

    if (!(str && *str))
	goto exit;

    if (js == NULL) js = rpmjsI();

#if defined(WITH_MOZJS)
    {	/* js-1.8.5 */
	JSI_t I = (JSI_t) js->I;
	jsval v = JSVAL_VOID;
	JSBool ok = JS_EvaluateScript(I->cx, I->global,
				str, strlen(str), __FILE__, __LINE__, &v);
	if (!ok)	/* XXX if (v == JS_NULL || v == JS_FALSE) */
	    goto exit;
	rc = RPMRC_OK;
	if (resultp) {
	    char b[128];
	    size_t nb = sizeof(b);
	    char * t = NULL;
	    if (JSVAL_IS_NULL(v)) {
		t = xstrdup("");
	    } else
	    if (JSVAL_IS_VOID(v)) {
		t = xstrdup("");
	    } else
	    if (JSVAL_IS_INT(v)) {	/* int32_t */
		snprintf(b, nb, "%ld", (long)JSVAL_TO_INT(v));
		t = xstrdup(b);
	    } else
	    if (JSVAL_IS_DOUBLE(v)) {
		snprintf(b, nb, "%.16g", (double)JSVAL_TO_DOUBLE(v));
		t = xstrdup(b);
	    } else
#ifdef	NOTYET
	    if (JSVAL_IS_NUMBER(v)) {
	    } else
#endif
	    if (JSVAL_IS_STRING(v)) {
		JSString * rstr = JSVAL_TO_STRING(v);
		size_t nt = JS_GetStringEncodingLength(I->cx, rstr);
		t = (char *) xmalloc(nt+1);
#if defined(WITH_MOZJS185)
		nt = JS_EncodeStringToBuffer(rstr, t, nt);
#endif
#if defined(WITH_MOZJS24) || defined(WITH_MOZJS31)
		nt = JS_EncodeStringToBuffer(I->cx, rstr, t, nt);
#endif
		t[nt] = '\0';
	    } else
#ifdef	NOTYET
	    if (JSVAL_IS_OBJECT(v)) {
	    } else
#endif
	    if (JSVAL_IS_BOOLEAN(v)) {
		snprintf(b, nb, "%c", (JSVAL_TO_BOOLEAN(v) ? 'T' : 'F'));
		t = xstrdup(b);
	    } else
#ifdef	NOTYET
	    if (JSVAL_IS_PRIMITIVE(v)) {
	    } else
	    if (JSVAL_IS_GCTHING(v)) {
	    } else
#endif
#if defined(WITH_MOZJS185)
	    {
		jsval_layout l;
		l.asBits = JSVAL_BITS(v);
		snprintf(b, nb, "FIXME: JSVAL tag 0x%x payload 0x%llx",
			(unsigned)(l.asBits >> JSVAL_TAG_SHIFT),
			(unsigned long long)(l.asBits & JSVAL_PAYLOAD_MASK)
		);
		t = xstrdup(b);
	    }
#endif
#if defined(WITH_MOZJS24) || defined(WITH_MOZJS31)
	    {
		t = xstrdup("FIXME: JSVAL unknown");
	    }
#endif
	    *resultp = t;
	}
    }
#endif	/* WITH_MOZJS */

exit:
RPMJSDBG(0, (stderr, "<== %s(%p,%p[%u]) rc %d |%s|\n", __FUNCTION__, js, str, (unsigned)(str ? strlen(str) : 0), rc, str));

    return rc;
}

#ifdef  __clang__
#pragma clang diagnostic pop
#endif

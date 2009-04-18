#include "system.h"

#include <argv.h>

#ifdef	WITH_JS
#define	XP_UNIX	1
#include "jsstddef.h"
#include "jstypes.h"
#include "jsarena.h"
#include "jsutil.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsdbgapi.h"
#include "jsemit.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsparse.h"
#include "jsscope.h"
#include "jsscript.h"
#endif
#define _RPMJS_INTERNAL
#include "rpmjs.h"

#include "debug.h"

/*@unchecked@*/
int _rpmjs_debug = 1;

/*@unchecked@*/ /*@relnull@*/
rpmjs _rpmjsI = NULL;

#if defined(WITH_JS)

FILE *gErrFile = NULL;
FILE *gOutFile = NULL;

static JSBool
Print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    FILE * fp = (gOutFile ? gOutFile : stdout);
    uintN i;

if (_rpmjs_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval);

    for (i = 0; i < argc; i++) {
	JSString *str;
	char *bytes;
        if ((str = JS_ValueToString(cx, argv[i])) == NULL
         || (bytes = JS_EncodeString(cx, str)) == NULL)
            return JS_FALSE;
        fprintf(fp, "%s%s", i ? " " : "", bytes);
        JS_free(cx, bytes);
    }
    fputc('\n', fp);
    fflush(fp);
    return JS_TRUE;
}

static JSFunctionSpec shell_functions[] = {
    JS_FS("print",          Print,          0,0,0),
    JS_FS_END
};

/*@unchecked@*/ /*@observer@*/
static JSClass global_class = {
    "global", JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

/* The error reporter callback. */
static void reportError(JSContext *cx, const char *msg, JSErrorReport *report)
	/*@*/
{
    fprintf(stderr, "%s:%u:%s\n",
	report->filename ? report->filename : "<no filename>",
	(unsigned int) report->lineno, msg);
}
#endif

static void rpmjsFini(void * _js)
        /*@globals fileSystem @*/
        /*@modifies *_js, fileSystem @*/
{
    rpmjs js = _js;

#if defined(WITH_JS)
if (_rpmjs_debug)
fprintf(stderr, "==> %s(%p) glob %p cx %p rt %p\n", __FUNCTION__, js, js->glob, js->cx, js->rt);
#ifdef	NOTYET
    JS_EndRequest((JSContext *)js->cx);
#endif
    JS_DestroyContext((JSContext *)js->cx);	js->cx = NULL;
    JS_DestroyRuntime((JSRuntime *)js->rt);	js->rt = NULL;
    JS_ShutDown();
#endif
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmjsPool;

static rpmjs rpmjsGetPool(/*@null@*/ rpmioPool pool)
        /*@globals _rpmjsPool, fileSystem @*/
        /*@modifies pool, _rpmjsPool, fileSystem @*/
{
    rpmjs js;

    if (_rpmjsPool == NULL) {
        _rpmjsPool = rpmioNewPool("js", sizeof(*js), -1, _rpmjs_debug,
                        NULL, NULL, rpmjsFini);
        pool = _rpmjsPool;
    }
    return (rpmjs) rpmioGetPool(pool, sizeof(*js));
}

rpmjs rpmjsNew(const char ** av, int flags)
{
    rpmjs js = rpmjsGetPool(_rpmjsPool);

#if defined(WITH_JS)
    static const char * _av[] = { "rpmjs", NULL };
    JSRuntime *rt;
    JSContext *cx;
    JSObject *glob;
    int ac;
    int xx;

    if (av == NULL) av = _av;
    ac = argvCount(av);

    rt = JS_NewRuntime(8L * 1024L * 1024L);
assert(rt != NULL);
#ifdef	NOTYET
    JS_SetContextCallback(rt, ContextCallback);
#endif
    js->rt = rt;

    cx = JS_NewContext(rt, 8192);
assert(cx != NULL);
#ifdef	NOTYET
    JS_BeginRequest(cx);
#endif
    JS_SetOptions(cx, JSOPTION_VAROBJFIX);
    JS_SetVersion(cx, JSVERSION_LATEST);
    JS_SetErrorReporter(cx, reportError);
    js->cx = cx;

    glob = JS_NewObject(cx, &global_class, NULL, NULL);
    xx = JS_InitStandardClasses(cx, glob);
    xx = JS_DefineFunctions(cx, glob, shell_functions);
#ifdef	NOTYET
    {	JSObject * env =
		JS_DefineObject(cx, glob, "environment", &env_class, NULL, 0);
assert(env != NULL);
	xx = JS_SetPrivate(cx, env, environ);
	js->env = env;
    }
#endif
    js->glob = glob;
#endif

    return rpmjsLink(js);
}

static rpmjs rpmjsI(void)
	/*@globals _rpmjsI @*/
	/*@modifies _rpmjsI @*/
{
    if (_rpmjsI == NULL)
	_rpmjsI = rpmjsNew(NULL, 0);
    return _rpmjsI;
}

rpmRC rpmjsRunFile(rpmjs js, const char * fn, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

if (_rpmjs_debug)
fprintf(stderr, "==> %s(%p,%s)\n", __FUNCTION__, js, fn);

    if (js == NULL) js = rpmjsI();

    if (fn != NULL) {
#if defined(WITH_JS)
	rc = RPMRC_OK;
#ifdef	NOTYET
	if (resultp)
	    *resultp = rpmiobStr(js->iob);
#endif
#endif
    }
    return rc;
}

rpmRC rpmjsRun(rpmjs js, const char * str, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

if (_rpmjs_debug)
fprintf(stderr, "==> %s(%p,%s)\n", __FUNCTION__, js, str);

    if (js == NULL) js = rpmjsI();

    if (str != NULL) {
#if defined(WITH_JS)
	JSContext *cx = js->cx;
	JSObject *glob = js->glob;
	jsval rval;
	JSBool ok;

	ok = JS_EvaluateScript(cx, glob, str, strlen(str),
					__FILE__, __LINE__, &rval);
	if (ok) {
	    rc = RPMRC_OK;
	    if (resultp) {
		JSString *str = JS_ValueToString(cx, rval);
		*resultp = JS_GetStringBytes(str);
	    }
	}
#endif
    }
    return rc;
}

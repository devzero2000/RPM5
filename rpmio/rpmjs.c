#if defined(__APPLE__)
/* workaround for "uuid_t" type conflict, between <unistd.h> and "uuid.h" */
#define _UUID_T
#define uuid_t __darwin_uuid_t
#include <unistd.h>
#undef uuid_t
#undef _UUID_T
#endif

#include "system.h"

#include <argv.h>

#ifdef	WITH_JS

#define	XP_UNIX	1
#include "jsprf.h"
#include "jsapi.h"

#if defined(WITH_GPSEE)
#define	MAKEDEPEND	/* XXX hack-o-round JS_THREADSAFE check */
#include <gpsee/gpsee.h>
typedef	gpsee_interpreter_t * JSI_t;
#else
typedef struct JSI_s {
    JSRuntime	* rt;
    JSContext	* cx;
    JSObject	* globalObj;
} * JSI_t;
#endif

#endif

#define _RPMJS_INTERNAL
#include "rpmjs.h"

#include "debug.h"

/*@unchecked@*/
int _rpmjs_debug = 0;

/*@unchecked@*/ /*@relnull@*/
rpmjs _rpmjsI = NULL;

#define	WITH_TRACEMONKEY

#if defined(WITH_JS)
static JSBool
Version(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
if (_rpmjs_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval);

    *rval = (argc > 0 && JSVAL_IS_INT(argv[0]))
	? INT_TO_JSVAL(JS_SetVersion(cx, (JSVersion) JSVAL_TO_INT(argv[0])))
	: INT_TO_JSVAL(JS_GetVersion(cx));
    return JS_TRUE;
}

static struct {
    const char  *name;
    uint32      flag;
} js_options[] = {
    {"strict",		JSOPTION_STRICT},
    {"werror",		JSOPTION_WERROR},
    {"atline",		JSOPTION_ATLINE},
    {"xml",		JSOPTION_XML},
#ifdef JSOPTION_RELIMIT
    {"relimit",		JSOPTION_RELIMIT},
#endif
#ifdef JSOPTION_ANONFUNFIX
    {"anonfunfix",	JSOPTION_ANONFUNFIX},
#endif
    {NULL,		0}
};

static JSBool
Options(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uint32 optset = 0;
    uintN found = 0;
    uintN i, j;
    JSString *str;
    const char *opt;
    char *names = NULL;
    JSBool ok = JS_FALSE;

if (_rpmjs_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval);

#ifdef	REFERENCE	/* XXX needs to be set globally. */
	case 'o':
	    for (j = 0; js_options[j].name; ++j) {
		if (strcmp(js_options[j].name, argv[i]))
		    continue;
		JS_ToggleOptions(cx, js_options[j].flag);
		break;
	    }
	    break;
#endif
    for (i = 0; i < argc; i++) {
	if ((str = JS_ValueToString(cx, argv[i])) == NULL)
	    goto exit;
	opt = JS_GetStringBytes(str);
	for (j = 0; js_options[j].name; j++) {
	    if (strcmp(js_options[j].name, opt))
		continue;
	    optset |= js_options[j].flag;
	    break;
	}
    }
    optset = JS_ToggleOptions(cx, optset);

    while (optset != 0) {
	uint32 flag = optset;
	optset &= optset - 1;
	flag &= ~optset;
	for (j = 0; js_options[j].name; j++) {
	    if (js_options[j].flag != flag)
		continue;
	    names = JS_sprintf_append(names, "%s%s",
				names ? "," : "", js_options[j].name);
	    found++;
	    break;
	}
    }
    if (!found)
	names = xstrdup("");

    if ((str = JS_NewString(cx, names, strlen(names))) == NULL)
	goto exit;
    *rval = STRING_TO_JSVAL(str);
    ok = JS_TRUE;
exit:
    if (!ok)
	names = _free(names);
    return ok;
}

FILE *gErrFile = NULL;
FILE *gOutFile = NULL;

static JSBool
Print(JSContext *cx, uintN argc, jsval *vp)
{
    FILE * fp = (gOutFile ? gOutFile : stdout);
    jsval *argv;
    uintN i;
    JSString *str;
    char *bytes;

    argv = JS_ARGV(cx, vp);
    for (i = 0; i < argc; i++) {
        str = JS_ValueToString(cx, argv[i]);
        if (!str)
            return JS_FALSE;
        bytes = JS_EncodeString(cx, str);
        if (!bytes)
            return JS_FALSE;
        fprintf(fp, "%s%s", i ? " " : "", bytes);
        JS_free(cx, bytes);
    }

    fputc('\n', fp);
    fflush(fp);

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool compileOnly = JS_FALSE;

static JSBool
Load(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN i;
    JSBool ok = JS_FALSE;

if (_rpmjs_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval);

    for (i = 0; i < argc; i++) {
	JSString *str;
	JSScript *script;
	const char *fn;
	uint32 oldopts;
	jsval result;	/* XXX why not returned through *rval?!? */

	if ((str = JS_ValueToString(cx, argv[i])) == NULL)
	    goto exit;
	argv[i] = STRING_TO_JSVAL(str);
	fn = JS_GetStringBytes(str);
	errno = 0;

	oldopts = JS_GetOptions(cx);
#if !defined(WITH_TRACEMONKEY)
#define	JSOPTION_NO_SCRIPT_RVAL	0	/* XXX TraceMonkey */
#endif
	JS_SetOptions(cx, oldopts | JSOPTION_COMPILE_N_GO | JSOPTION_NO_SCRIPT_RVAL);
	script = JS_CompileFile(cx, obj, fn);
	JS_SetOptions(cx, oldopts);

	if (script == NULL)
	    goto exit;
	ok = !compileOnly
		? JS_ExecuteScript(cx, obj, script, &result)
		: JS_TRUE;
	JS_DestroyScript(cx, script);
	if (!ok)
	    goto exit;
    }
    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
Require(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{

if (_rpmjs_debug < 0)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval);

    return JS_TRUE;
}

#ifndef JS_FS
#define JS_FS(name,call,nargs,flags,extra) \
    {name, call, nargs, flags, extra}
#endif
#ifndef JS_FS_END
#define JS_FS_END JS_FS(NULL,NULL,0,0,0)
#endif

static JSFunctionSpec shell_functions[] = {
    JS_FS("version",	Version,	0,0,0),
    JS_FS("options",	Options,	0,0,0),
    JS_FS("load",	Load,		1,0,0),
#if defined(WITH_TRACEMONKEY)
    JS_FN("print",	Print,		0,0),
#else
    JS_FN("print",	Print,		0,0,0),
#endif
    JS_FS("loadModule",	Require,	0,0,0),
    JS_FS("require",	Require,	0,0,0),
    JS_FS_END
};

static JSBool
env_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSString *idstr = JS_ValueToString(cx, id);
    JSString *valstr = JS_ValueToString(cx, *vp);
    JSBool ok = JS_FALSE;

if (_rpmjs_debug)
fprintf(stderr, "==> %s(%p,%p,0x%llx,%p)\n", __FUNCTION__, cx, obj, (unsigned long long)id, vp);

    if (idstr && valstr) {
	const char * name = JS_GetStringBytes(idstr);
	const char * value = JS_GetStringBytes(valstr);

	if (setenv(name, value, 1) < 0)
	    JS_ReportError(cx, "can't set envvar %s to %s", name, value);
	else {
	    *vp = STRING_TO_JSVAL(valstr);
	    ok = JS_TRUE;
	}
    }
    return ok;
}

static JSBool
env_enumerate(JSContext *cx, JSObject *obj)
{
    static JSBool reflected;
    char **evp, *name;
    JSBool ok = JS_FALSE;

if (_rpmjs_debug)
fprintf(stderr, "==> %s(%p,%p)\n", __FUNCTION__, cx, obj);

    if (reflected)
	return JS_TRUE;

    for (evp = (char **)JS_GetPrivate(cx, obj); (name = *evp) != NULL; evp++) {
	char *value;
	JSString *valstr;

	if ((value = strchr(name, '=')) == NULL)
	    continue;
	*value++ = '\0';
	if ((valstr = JS_NewStringCopyZ(cx, value)) != NULL)
	    ok = JS_DefineProperty(cx, obj, name, STRING_TO_JSVAL(valstr),
			NULL, NULL, JSPROP_ENUMERATE);
	value[-1] = '=';
	if (!ok)
	    goto exit;
    }

    reflected = JS_TRUE;
    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
env_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags, JSObject **objp)
{
    JSString *idstr, *valstr;
    const char *name, *value;
    JSBool ok = JS_FALSE;

if (_rpmjs_debug)
fprintf(stderr, "==> %s(%p,%p,0x%llx,%u,%p)\n", __FUNCTION__, cx, obj, (unsigned long long)id, flags, objp);

    if (flags & JSRESOLVE_ASSIGNING)
	return JS_TRUE;

    if ((idstr = JS_ValueToString(cx, id)) == NULL)
	goto exit;
    name = JS_GetStringBytes(idstr);
    if ((value = getenv(name)) != NULL) {
	if ((valstr = JS_NewStringCopyZ(cx, value)) == NULL
	 || !JS_DefineProperty(cx, obj, name, STRING_TO_JSVAL(valstr),
			NULL, NULL, JSPROP_ENUMERATE))
	    goto exit;
	*objp = obj;
    }
    ok = JS_TRUE;
exit:
    return ok;
}

/*@unchecked@*/ /*@observer@*/
static JSClass env_class = {
    "environment", JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE,
    JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub,  env_setProperty,
    env_enumerate, (JSResolveOp) env_resolve, JS_ConvertStub,   JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

/*@unchecked@*/ /*@observer@*/
static JSClass global_class = {
    "global", JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

/* The error reporter callback. */
static void reportError(JSContext *cx, const char *msg, JSErrorReport *report)
	/*@*/
{
    FILE * fp = (gErrFile ? gErrFile : stderr);
    fprintf(fp, "%s:%u:%s\n",
	report->filename ? report->filename : "<no filename>",
	(unsigned int) report->lineno, msg);
}
#endif

static
int rpmjs_destroyInterpreter(/*@only@*/ /*@null@*/ JSI_t I)
	/*@modifies I @*/
{
    int rc = 0;

    if (I != NULL) {
#if defined(WITH_JS)
if (_rpmjs_debug)
fprintf(stderr, "==> %s(%p) glob %p cx %p rt %p\n", __FUNCTION__, I, I->globalObj, I->cx, I->rt);
#ifdef	NOTYET
	JS_EndRequest((JSContext *)I->cx);
#endif
	JS_DestroyContext((JSContext *)I->cx);	I->cx = NULL;
	JS_DestroyRuntime((JSRuntime *)I->rt);	I->rt = NULL;
	JS_ShutDown();
	I = _free(I);
#endif
    }

    return rc;
}

static /*@only@*/
JSI_t rpmjs_createInterpreter(const char ** av,
		/*@null@*/ /*@unused@*/ const char ** ev)
	/*@*/
{
    JSI_t I = NULL;
#if defined(WITH_JS)
    static const char * _av[] = { "rpmjs", NULL };
    JSRuntime *rt;
    JSContext *cx;
    JSObject *glob;
    int ac;
    int xx;

if (_rpmjs_debug)
fprintf(stderr, "==> %s(%p,%p)\n", __FUNCTION__, av, ev);

    if (av == NULL) av = _av;
    ac = argvCount(av);

    /* Create an interpreter container. */
    I = xcalloc(1, sizeof(*I));

    /* Initialize JS runtime. */
    rt = JS_NewRuntime(8L * 1024L * 1024L);
assert(rt != NULL);
#ifdef	NOTYET
    JS_SetContextCallback(rt, ContextCallback);
#endif
    /* Set maximum memory for JS engine to infinite. */
    JS_SetGCParameter(rt, JSGC_MAX_BYTES, (size_t)-1);
    I->rt = rt;

    /* Initialize JS context. */
    cx = JS_NewContext(rt, 8192);
assert(cx != NULL);
#ifdef	NOTYET
    JS_BeginRequest(cx);
#endif
    JS_SetOptions(cx, JSOPTION_VAROBJFIX);
#if JS_VERSION < 180 
    JS_SetVersion(cx, JSVERSION_1_7);
#else
    JS_SetVersion(cx, JSVERSION_LATEST);
#endif
    JS_SetErrorReporter(cx, reportError);
#ifdef	NOTYET
    JS_CStringsAreUTF8();
#endif
    I->cx = cx;

    /* Initialize JS global object. */
    glob = JS_NewObject(cx, &global_class, NULL, NULL);
assert(glob != NULL);

#ifdef	NOTYET
    JS_AddNamedRoot(cx, &glob, "glob");
#endif

    xx = JS_InitStandardClasses(cx, glob);

    xx = JS_DefineFunctions(cx, glob, shell_functions);
    I->globalObj = glob;

    {	JSObject * env =
		JS_DefineObject(cx, glob, "environment", &env_class, NULL, 0);
assert(env != NULL);
	xx = JS_SetPrivate(cx, env, environ);
    }

    {	JSObject * args = JS_NewArrayObject(cx, 0, NULL);
	int i;

assert(args != NULL);
	xx = JS_DefineProperty(cx, glob, "arguments", OBJECT_TO_JSVAL(args),
		NULL, NULL, 0);
	for (i = 0; i < ac; i++) {
	    JSString *str = JS_NewStringCopyZ(cx, av[i]);
assert(str != NULL);
	    xx = JS_DefineElement(cx, args, i, STRING_TO_JSVAL(str),
			NULL, NULL, JSPROP_ENUMERATE);
	}
    }
#endif	/* WITH_JS */
    return I;
}

static void rpmjsFini(void * _js)
	/*@globals fileSystem @*/
	/*@modifies *_js, fileSystem @*/
{
    rpmjs js = _js;

if (_rpmjs_debug)
fprintf(stderr, "==> %s(%p) I %p\n", __FUNCTION__, js, js->I);

    (void) rpmjs_destroyInterpreter(js->I);
    js->I = NULL;

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
    JSI_t I = rpmjs_createInterpreter(av, NULL);
    js->I = I;
    if (I != NULL)
	JS_SetRuntimePrivate(I->rt, js);
#endif

    return rpmjsLink(js);
}

static rpmjs rpmjsI(void)
	/*@globals _rpmjsI @*/
	/*@modifies _rpmjsI @*/
{
    if (_rpmjsI == NULL)
	_rpmjsI = rpmjsNew(NULL, 0);
if (_rpmjs_debug)
fprintf(stderr, "<== %s() _rpmjsI %p\n", __FUNCTION__, _rpmjsI);
    return _rpmjsI;
}

rpmRC rpmjsRunFile(rpmjs js, const char * fn, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

    if (js == NULL) js = rpmjsI();

    if (fn != NULL) {
#if defined(WITH_JS)
	JSI_t I = js->I;
	JSContext *cx = I->cx;
	JSObject *glob = I->globalObj;
	JSScript *script = JS_CompileFile(cx, glob, fn);
	jsval rval;

	if (script) {
	    if (JS_ExecuteScript(cx, glob, script, &rval)) {
		rc = RPMRC_OK;
		if (resultp) {
		    JSString *rstr = JS_ValueToString(cx, rval);
		    *resultp = JS_GetStringBytes(rstr);
		}
	    }
	    JS_DestroyScript(cx, script);
	}
#endif
    }

if (_rpmjs_debug)
fprintf(stderr, "<== %s(%p,%s) rc %d\n", __FUNCTION__, js, fn, rc);

    return rc;
}

rpmRC rpmjsRun(rpmjs js, const char * str, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

    if (js == NULL) js = rpmjsI();

    if (str != NULL) {
#if defined(WITH_JS)
	JSI_t I = js->I;
	JSContext *cx = I->cx;
	JSObject *glob = I->globalObj;
	jsval rval = JSVAL_VOID;
	JSBool ok;

	ok = JS_EvaluateScript(cx, glob, str, strlen(str),
					__FILE__, __LINE__, &rval);
	if (ok) {
	    rc = RPMRC_OK;
	    if (resultp) {
		JSString *rstr = JS_ValueToString(cx, rval);
		*resultp = JS_GetStringBytes(rstr);
	    }
	}
#endif
    }

if (_rpmjs_debug)
fprintf(stderr, "<== %s(%p,%p[%u]) rc %d\n", __FUNCTION__, js, str, (str ? strlen(str) : 0), rc);

    return rc;
}

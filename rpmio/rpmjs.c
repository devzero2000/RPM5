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
#undef	JS_HAS_FILE_OBJECT

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
#ifdef	JS_HAS_FILE_OBJECT
#include "rpmjsio.h"
#endif
#endif
#define _RPMJS_INTERNAL
#include "rpmjs.h"

#include "debug.h"

/*@unchecked@*/
int _rpmjs_debug = 0;

/*@unchecked@*/ /*@relnull@*/
rpmjs _rpmjsI = NULL;

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
    {"relimit",		JSOPTION_RELIMIT},
    {"anonfunfix",	JSOPTION_ANONFUNFIX},
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
	JS_SetOptions(cx, oldopts | JSOPTION_COMPILE_N_GO);
	if ((script = JS_CompileFile(cx, obj, fn)) == NULL)
	    goto exit;
	ok = !compileOnly
		? JS_ExecuteScript(cx, obj, script, &result)
		: JS_TRUE;
	JS_DestroyScript(cx, script);
	JS_SetOptions(cx, oldopts);
	if (!ok)
	    goto exit;
    }
    ok = JS_TRUE;
exit:
    return ok;
}

FILE *gErrFile = NULL;
FILE *gOutFile = NULL;

static JSBool
Print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    FILE * fp = (gOutFile ? gOutFile : stdout);
    uintN i;

if (_rpmjs_debug < 0)
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
    JS_FS("version",	Version,	0,0,0),
    JS_FS("options",	Options,	0,0,0),
    JS_FS("load",	Load,		1,0,0),
    JS_FS("print",	Print,		0,0,0),
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
    "global", JSCLASS_GLOBAL_FLAGS,
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
#ifdef	JS_HAS_FILE_OBJECT
    (void) js_InitFileClass(cx, glob);
#endif
    xx = JS_DefineFunctions(cx, glob, shell_functions);
    js->glob = glob;

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
	JSContext *cx = js->cx;
	JSObject *glob = js->glob;
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
		JSString *rstr = JS_ValueToString(cx, rval);
		*resultp = JS_GetStringBytes(rstr);
	    }
	}
#endif
    }
    return rc;
}

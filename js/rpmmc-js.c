/** \ingroup js_c
 * \file js/rpmmc-js.c
 */

#include "system.h"

#include "rpmmc-js.h"
#include "rpmjs-debug.h"

#define	_MACRO_INTERNAL
#include <rpmmacro.h>

#include "debug.h"

/*@unchecked@*/
static int _debug = 0;

typedef	MacroContext rpmmc;

/* --- helpers */

/* --- Object methods */
static JSBool
rpmmc_add(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmcClass, NULL);
    rpmmc mc = ptr;
    char * s = NULL;
    int lvl = 0;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "s", &s)))
        goto exit;

    (void) rpmDefineMacro(mc, s, lvl);
    ok = JS_TRUE;
exit:
    *rval = BOOLEAN_TO_JSVAL(ok);
    return ok;
}

static JSBool
rpmmc_del(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmcClass, NULL);
    rpmmc mc = ptr;
    char * s = NULL;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "s", &s)))
        goto exit;

    (void) rpmUndefineMacro(mc, s);
    ok = JS_TRUE;
exit:
    *rval = BOOLEAN_TO_JSVAL(ok);
    return ok;
}

static JSBool
rpmmc_list(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmcClass, NULL);
    rpmmc mc = ptr;
    void * _mire = NULL;
    int used = -1;
    const char ** av = NULL;
    int ac = 0;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

/*@-globs@*/
    ac = rpmGetMacroEntries(mc, _mire, used, &av);
/*@=globs@*/
    if (ac > 0 && av != NULL && av[0] != NULL) {
	jsval *vec = xmalloc(ac * sizeof(*vec));
	JSString *valstr;
	int i;
	for (i = 0; i < ac; i++) {
	    /* XXX lua splits into {name,opts,body} triple. */
	    if ((valstr = JS_NewStringCopyZ(cx, av[i])) == NULL)
		goto exit;
	    vec[i] = STRING_TO_JSVAL(valstr);
	}
	*rval = OBJECT_TO_JSVAL(JS_NewArrayObject(cx, ac, vec));
	vec = _free(vec);
    } else
	*rval = JSVAL_NULL;	/* XXX JSVAL_VOID? */

    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmmc_expand(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmcClass, NULL);
    rpmmc mc = ptr;
    char * s;
    char * t;
    JSString *valstr;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "s", &s)))
        goto exit;

    t = rpmMCExpand(mc, s, NULL);

    if ((valstr = JS_NewStringCopyZ(cx, t)) == NULL)
	goto exit;
    t = _free(t);
    *rval = STRING_TO_JSVAL(valstr);

    ok = JS_TRUE;
exit:
    return ok;
}

static JSFunctionSpec rpmmc_funcs[] = {
    JS_FS("",	rpmmc_add,		0,0,0),
    JS_FS("",	rpmmc_del,		0,0,0),
    JS_FS("",	rpmmc_list,		0,0,0),
    JS_FS("",	rpmmc_expand,		0,0,0),
    JS_FS_END
};

/* --- Object properties */
enum rpmmc_tinyid {
    _DEBUG	= -2,
};

static JSPropertySpec rpmmc_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
    {NULL, 0, 0, NULL, NULL}
};

static JSBool
rpmmc_addprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmcClass, NULL);
_PROP_DEBUG_ENTRY(_debug < 0);
    return JS_TRUE;
}

static JSBool
rpmmc_delprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmcClass, NULL);
_PROP_DEBUG_ENTRY(_debug < 0);
    return JS_TRUE;
}

static JSBool
rpmmc_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmcClass, NULL);
    jsint tiny = JSVAL_TO_INT(id);
    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    JSBool ok = (ptr ? JS_FALSE : JS_TRUE);

    switch (tiny) {
    case _DEBUG:
	*vp = INT_TO_JSVAL(_debug);
	ok = JS_TRUE;
	break;
    default:
	break;
    }

    if (!ok) {
_PROP_DEBUG_EXIT(_debug);
    }

ok = JS_TRUE;   /* XXX avoid immediate interp exit by always succeeding. */

    return ok;
}

static JSBool
rpmmc_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmcClass, NULL);
    jsint tiny = JSVAL_TO_INT(id);
    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    JSBool ok = (ptr ? JS_FALSE : JS_TRUE);
    int myint;

    switch (tiny) {
    case _DEBUG:
	if (JS_ValueToInt32(cx, *vp, &_debug))
	    ok = JS_TRUE;
	break;
    default:
	break;
    }

    if (!ok) {
_PROP_DEBUG_EXIT(_debug);
    }
    return ok;
}

static JSBool
rpmmc_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
	JSObject **objp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmcClass, NULL);
    JSBool ok = JS_FALSE;

_RESOLVE_DEBUG_ENTRY(_debug < 0);

    if ((flags & JSRESOLVE_ASSIGNING)
     || (ptr == NULL)) { /* don't resolve to parent prototypes objects. */
	*objp = NULL;
	ok = JS_TRUE;
	goto exit;
    }

    *objp = obj;	/* XXX always resolve in this object. */

    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmmc_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
		  jsval *statep, jsid *idp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmcClass, NULL);
    JSBool ok = JS_FALSE;

_ENUMERATE_DEBUG_ENTRY(_debug < 0);

    switch (op) {
    case JSENUMERATE_INIT:
	*statep = JSVAL_VOID;
        if (idp)
            *idp = JSVAL_ZERO;
        break;
    case JSENUMERATE_NEXT:
	*statep = JSVAL_VOID;
        if (*idp != JSVAL_VOID)
            break;
        /*@fallthrough@*/
    case JSENUMERATE_DESTROY:
	*statep = JSVAL_NULL;
        break;
    }
    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmmc_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmcClass, NULL);

_CONVERT_DEBUG_ENTRY(_debug);

    return JS_TRUE;
}

/* --- Object ctors/dtors */
static rpmmc
rpmmc_init(JSContext *cx, JSObject *obj, JSObject *o)
{
    rpmmc mc = NULL;	/* XXX FIXME: only global context for now. */

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p) mc %p\n", __FUNCTION__, cx, obj, o, mc);

    if (o == NULL) {
if (_debug)
fprintf(stderr, "\tinitMacros() mc %p\n", mc);
    } else
    if (OBJ_IS_STRING(cx, o)) {
	const char * s =
		JS_GetStringBytes(JS_ValueToString(cx, OBJECT_TO_JSVAL(o)));
        if (!strcmp(s, "global"))
            mc = rpmGlobalMacroContext;
	else if (!strcmp(s, "cli"))
            mc = rpmCLIMacroContext;
	else {
	    mc = xcalloc(1, sizeof(*mc));
	    if (s && *s)
		rpmInitMacros(mc, s);
	    else
		s = "";
	}
if (_debug)
fprintf(stderr, "\tinitMacros(\"%s\") mc %p\n", s, mc);
    }

    if (!JS_SetPrivate(cx, obj, (void *)mc)) {
	/* XXX error msg */
	return NULL;
    }
    return mc;
}

static void
rpmmc_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmcClass, NULL);
    rpmmc mc = ptr;

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);
    if (!(mc == rpmGlobalMacroContext || mc == rpmCLIMacroContext)) {
	rpmFreeMacros(mc);
	mc = _free(mc);
    }
}

static JSBool
rpmmc_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_FALSE;
    JSObject *o = NULL;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)%s\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ((cx->fp->flags & JSFRAME_CONSTRUCTING) ? " constructing" : ""));

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "/o", &o)))
        goto exit;

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	(void) rpmmc_init(cx, obj, o);
    } else {
	if ((obj = JS_NewObject(cx, &rpmmcClass, NULL, NULL)) == NULL)
	    goto exit;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

/* --- Class initialization */
JSClass rpmmcClass = {
    "Mc", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE,
    rpmmc_addprop,   rpmmc_delprop, rpmmc_getprop, rpmmc_setprop,
    (JSEnumerateOp)rpmmc_enumerate, (JSResolveOp)rpmmc_resolve,
    rpmmc_convert,	rpmmc_dtor,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *
rpmjs_InitMcClass(JSContext *cx, JSObject* obj)
{
    JSObject *proto;

if (_debug)
fprintf(stderr, "==> %s(%p,%p)\n", __FUNCTION__, cx, obj);

    proto = JS_InitClass(cx, obj, NULL, &rpmmcClass, rpmmc_ctor, 1,
		rpmmc_props, rpmmc_funcs, NULL, NULL);
assert(proto != NULL);
    return proto;
}

JSObject *
rpmjs_NewMcObject(JSContext *cx, JSObject *o)
{
    JSObject *obj;
    rpmmc mc;

    if ((obj = JS_NewObject(cx, &rpmmcClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((mc = rpmmc_init(cx, obj, o)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

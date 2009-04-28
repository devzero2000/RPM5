/** \ingroup js_c
 * \file js/rpmmacro-js.c
 */

#include "system.h"

#include "rpmmacro-js.h"
#include "rpmjs-debug.h"

#include <rpmmacro.h>

#include "debug.h"

/*@unchecked@*/
static int _debug = 0;

typedef	void ** rpmmacro;

/* --- helpers */

/* --- Object methods */
static JSBool
rpmmacro_expand(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmacroClass, NULL);
    char * s;
    char * t;
    JSString *valstr;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "s", &s)))
        goto exit;

    t = rpmExpand(s, NULL);
    if ((valstr = JS_NewStringCopyZ(cx, t)) == NULL)
	goto exit;
    t = _free(t);
    *rval = STRING_TO_JSVAL(valstr);

    ok = JS_TRUE;
exit:
    return ok;
}

static JSFunctionSpec rpmmacro_funcs[] = {
    {"expand",	rpmmacro_expand,		0,0,0},
    JS_FS_END
};

/* --- Object properties */
enum rpmmacro_tinyid {
    _DEBUG	= -2,
};

static JSPropertySpec rpmmacro_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
    {NULL, 0, 0, NULL, NULL}
};

static JSBool
rpmmacro_addprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmacroClass, NULL);
_PROP_DEBUG_ENTRY(_debug < 0);
    return JS_TRUE;
}

static JSBool
rpmmacro_delprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmacroClass, NULL);
_PROP_DEBUG_ENTRY(_debug < 0);
    return JS_TRUE;
}

static JSBool
rpmmacro_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmacroClass, NULL);
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
rpmmacro_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmacroClass, NULL);
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
rpmmacro_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
	JSObject **objp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmacroClass, NULL);
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
rpmmacro_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
		  jsval *statep, jsid *idp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmacroClass, NULL);
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
rpmmacro_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmacroClass, NULL);

_CONVERT_DEBUG_ENTRY(_debug);

    return JS_TRUE;
}

/* --- Object ctors/dtors */
static rpmmacro
rpmmacro_init(JSContext *cx, JSObject *obj)
{
    rpmmacro macro = xcalloc(1, sizeof(*macro));
    int xx;

    if (!JS_SetPrivate(cx, obj, (void *)macro)) {
	/* XXX error msg */
	return NULL;
    }
    return macro;
}

static void
rpmmacro_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmacroClass, NULL);

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);

    (void) _free(ptr);
}

static JSBool
rpmmacro_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_FALSE;
    JSObject *o = NULL;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)%s\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ((cx->fp->flags & JSFRAME_CONSTRUCTING) ? " constructing" : ""));

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	if (rpmmacro_init(cx, obj) == NULL)
	    goto exit;
    } else {
	if ((obj = JS_NewObject(cx, &rpmmacroClass, NULL, NULL)) == NULL)
	    goto exit;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

/* --- Class initialization */
JSClass rpmmacroClass = {
    "Macro", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE,
    rpmmacro_addprop,   rpmmacro_delprop, rpmmacro_getprop, rpmmacro_setprop,
    (JSEnumerateOp)rpmmacro_enumerate, (JSResolveOp)rpmmacro_resolve,
    rpmmacro_convert,	rpmmacro_dtor,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *
rpmjs_InitMacroClass(JSContext *cx, JSObject* obj)
{
    JSObject * o;

if (_debug)
fprintf(stderr, "==> %s(%p,%p)\n", __FUNCTION__, cx, obj);

    o = JS_InitClass(cx, obj, NULL, &rpmmacroClass, rpmmacro_ctor, 1,
		rpmmacro_props, rpmmacro_funcs, NULL, NULL);
assert(o != NULL);
    return o;
}

JSObject *
rpmjs_NewMacroObject(JSContext *cx, JSObject *o)
{
    JSObject *obj;
    rpmmacro macro;

    if ((obj = JS_NewObject(cx, &rpmmacroClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((macro = rpmmacro_init(cx, obj)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

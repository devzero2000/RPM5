/** \ingroup js_c
 * \file js/rpmsx-js.c
 */

#include "system.h"

#include "rpmsx-js.h"
#include "rpmjs-debug.h"

#define	_RPMSX_INTERNAL
#include <rpmsx.h>

#include "debug.h"

/*@unchecked@*/
static int _debug = 0;

/* Required JSClass vectors */
#define	rpmsx_addprop		JS_PropertyStub
#define	rpmsx_delprop		JS_PropertyStub
#define	rpmsx_convert		JS_ConvertStub

/* Optional JSClass vectors */
#define	rpmsx_getobjectops	NULL
#define	rpmsx_checkaccess	NULL
#define	rpmsx_call		NULL
#define	rpmsx_construct		rpmsx_ctor
#define	rpmsx_xdrobject		NULL
#define	rpmsx_hasinstance	NULL
#define	rpmsx_mark		NULL
#define	rpmsx_reserveslots	NULL

/* Extended JSClass vectors */
#define rpmsx_equality		NULL
#define rpmsx_outerobject	NULL
#define rpmsx_innerobject	NULL
#define rpmsx_iteratorobject	NULL
#define rpmsx_wrappedobject	NULL

/* --- helpers */

/* --- Object methods */

static JSFunctionSpec rpmsx_funcs[] = {
    JS_FS_END
};

/* --- Object properties */
enum rpmsx_tinyid {
    _DEBUG	= -2,
};

static JSPropertySpec rpmsx_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
    {NULL, 0, 0, NULL, NULL}
};

static JSBool
rpmsx_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmsxClass, NULL);
    jsint tiny = JSVAL_TO_INT(id);

    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    if (ptr == NULL)
	return JS_TRUE;

    switch (tiny) {
    case _DEBUG:
	*vp = INT_TO_JSVAL(_debug);
	break;
    default:
	break;
    }

    return JS_TRUE;
}

static JSBool
rpmsx_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmsxClass, NULL);
    jsint tiny = JSVAL_TO_INT(id);

    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    if (ptr == NULL)
	return JS_TRUE;

    switch (tiny) {
    case _DEBUG:
	if (!JS_ValueToInt32(cx, *vp, &_debug))
	    break;
	break;
    default:
	break;
    }

    return JS_TRUE;
}

static JSBool
rpmsx_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
	JSObject **objp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmsxClass, NULL);

_RESOLVE_DEBUG_ENTRY(_debug < 0);

    if ((flags & JSRESOLVE_ASSIGNING)
     || (ptr == NULL)) { /* don't resolve to parent prototypes objects. */
	*objp = NULL;
	goto exit;
    }

    *objp = obj;	/* XXX always resolve in this object. */

exit:
    return JS_TRUE;
}

static JSBool
rpmsx_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
		  jsval *statep, jsid *idp)
{

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
    return JS_TRUE;
}

/* --- Object ctors/dtors */
static rpmsx
rpmsx_init(JSContext *cx, JSObject *obj)
{
    const char * _fn = NULL;
    int _flags = 0;
    rpmsx sx = rpmsxNew(_fn, _flags);

if (_debug)
fprintf(stderr, "==> %s(%p,%p) sx %p\n", __FUNCTION__, cx, obj, sx);

    if (!JS_SetPrivate(cx, obj, (void *)sx)) {
	/* XXX error msg */
	return NULL;
    }
    return sx;
}

static void
rpmsx_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmsxClass, NULL);
    rpmsx sx = ptr;

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);
    sx = rpmsxFree(sx);
}

static JSBool
rpmsx_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)%s\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ((cx->fp->flags & JSFRAME_CONSTRUCTING) ? " constructing" : ""));

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	(void) rpmsx_init(cx, obj);
    } else {
	if ((obj = JS_NewObject(cx, &rpmsxClass, NULL, NULL)) == NULL)
	    goto exit;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

/* --- Class initialization */
JSClass rpmsxClass = {
    "Sx", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE,
    rpmsx_addprop,   rpmsx_delprop, rpmsx_getprop, rpmsx_setprop,
    (JSEnumerateOp)rpmsx_enumerate, (JSResolveOp)rpmsx_resolve,
    rpmsx_convert,	rpmsx_dtor,

    rpmsx_getobjectops,	rpmsx_checkaccess,
    rpmsx_call,		rpmsx_construct,
    rpmsx_xdrobject,	rpmsx_hasinstance,
    rpmsx_mark,		rpmsx_reserveslots,
};

JSObject *
rpmjs_InitSxClass(JSContext *cx, JSObject* obj)
{
    JSObject *proto;

if (_debug)
fprintf(stderr, "==> %s(%p,%p)\n", __FUNCTION__, cx, obj);

    proto = JS_InitClass(cx, obj, NULL, &rpmsxClass, rpmsx_ctor, 1,
		rpmsx_props, rpmsx_funcs, NULL, NULL);
assert(proto != NULL);
    return proto;
}

JSObject *
rpmjs_NewSxObject(JSContext *cx)
{
    JSObject *obj;
    rpmsx sx;

    if ((obj = JS_NewObject(cx, &rpmsxClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((sx = rpmsx_init(cx, obj)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

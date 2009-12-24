/** \ingroup js_c
 * \file js/rpmcudf-js.c
 */

#include "system.h"

#include "rpmcudf-js.h"
#include "rpmjs-debug.h"

#define	_RPMCUDF_INTERNAL
#include <rpmcudf.h>

#include "debug.h"

/*@unchecked@*/
static int _debug = 0;

/* Required JSClass vectors */
#define	rpmcudf_addprop		JS_PropertyStub
#define	rpmcudf_delprop		JS_PropertyStub
#define	rpmcudf_convert		JS_ConvertStub

/* Optional JSClass vectors */
#define	rpmcudf_getobjectops	NULL
#define	rpmcudf_checkaccess	NULL
#define	rpmcudf_call		NULL
#define	rpmcudf_construct		rpmcudf_ctor
#define	rpmcudf_xdrobject		NULL
#define	rpmcudf_hasinstance	NULL
#define	rpmcudf_mark		NULL
#define	rpmcudf_reserveslots	NULL

/* Extended JSClass vectors */
#define rpmcudf_equality		NULL
#define rpmcudf_outerobject	NULL
#define rpmcudf_innerobject	NULL
#define rpmcudf_iteratorobject	NULL
#define rpmcudf_wrappedobject	NULL

/* --- helpers */

/* --- Object methods */

static JSFunctionSpec rpmcudf_funcs[] = {
    JS_FS_END
};

/* --- Object properties */
enum rpmcudf_tinyid {
    _DEBUG	= -2,
};

static JSPropertySpec rpmcudf_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
    {NULL, 0, 0, NULL, NULL}
};

static JSBool
rpmcudf_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmcudfClass, NULL);
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
rpmcudf_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmcudfClass, NULL);
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
rpmcudf_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
	JSObject **objp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmcudfClass, NULL);

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
rpmcudf_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
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
static rpmcudf
rpmcudf_init(JSContext *cx, JSObject *obj)
{
    rpmcudf cudf = rpmcudfNew(NULL, RPMCUDV_CUDF);

if (_debug)
fprintf(stderr, "==> %s(%p,%p) cudf %p\n", __FUNCTION__, cx, obj, cudf);

    if (!JS_SetPrivate(cx, obj, (void *)cudf)) {
	/* XXX error msg */
	return NULL;
    }
    return cudf;
}

static void
rpmcudf_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmcudfClass, NULL);
    rpmcudf cudf = ptr;

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);
    cudf = rpmcudfFree(cudf);
}

static JSBool
rpmcudf_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)%s\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ((cx->fp->flags & JSFRAME_CONSTRUCTING) ? " constructing" : ""));

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	(void) rpmcudf_init(cx, obj);
    } else {
	if ((obj = JS_NewObject(cx, &rpmcudfClass, NULL, NULL)) == NULL)
	    goto exit;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

/* --- Class initialization */
JSClass rpmcudfClass = {
    "Cudf", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE,
    rpmcudf_addprop,   rpmcudf_delprop, rpmcudf_getprop, rpmcudf_setprop,
    (JSEnumerateOp)rpmcudf_enumerate, (JSResolveOp)rpmcudf_resolve,
    rpmcudf_convert,	rpmcudf_dtor,

    rpmcudf_getobjectops,	rpmcudf_checkaccess,
    rpmcudf_call,		rpmcudf_construct,
    rpmcudf_xdrobject,	rpmcudf_hasinstance,
    rpmcudf_mark,		rpmcudf_reserveslots,
};

JSObject *
rpmjs_InitCudfClass(JSContext *cx, JSObject* obj)
{
    JSObject *proto;

if (_debug)
fprintf(stderr, "==> %s(%p,%p)\n", __FUNCTION__, cx, obj);

    proto = JS_InitClass(cx, obj, NULL, &rpmcudfClass, rpmcudf_ctor, 1,
		rpmcudf_props, rpmcudf_funcs, NULL, NULL);
assert(proto != NULL);
    return proto;
}

JSObject *
rpmjs_NewCudfObject(JSContext *cx)
{
    JSObject *obj;
    rpmcudf cudf;

    if ((obj = JS_NewObject(cx, &rpmcudfClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((cudf = rpmcudf_init(cx, obj)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

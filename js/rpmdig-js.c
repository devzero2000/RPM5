/** \ingroup js_c
 * \file js/rpmdig-js.c
 */

#include "system.h"

#include "rpmdig-js.h"
#include "rpmjs-debug.h"

#include <rpmpgp.h>

#include "debug.h"

/*@unchecked@*/
static int _debug = 0;

/* Required JSClass vectors */
#define	rpmdig_addprop		JS_PropertyStub
#define	rpmdig_delprop		JS_PropertyStub
#define	rpmdig_convert		JS_ConvertStub

/* Optional JSClass vectors */
#define	rpmdig_getobjectops	NULL
#define	rpmdig_checkaccess	NULL
#define	rpmdig_call		NULL
#define	rpmdig_construct	rpmdig_ctor
#define	rpmdig_xdrobject	NULL
#define	rpmdig_hasinstance	NULL
#define	rpmdig_mark		NULL
#define	rpmdig_reserveslots	NULL

/* Extended JSClass vectors */
#define rpmdig_equality		NULL
#define rpmdig_outerobject	NULL
#define rpmdig_innerobject	NULL
#define rpmdig_iteratorobject	NULL
#define rpmdig_wrappedobject	NULL

/* --- helpers */

/* --- Object methods */

static JSFunctionSpec rpmdig_funcs[] = {
    JS_FS_END
};

/* --- Object properties */
enum rpmdig_tinyid {
    _DEBUG	= -2,
};

static JSPropertySpec rpmdig_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
    {NULL, 0, 0, NULL, NULL}
};

static JSBool
rpmdig_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdigClass, NULL);
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
rpmdig_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdigClass, NULL);
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
rpmdig_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
	JSObject **objp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdigClass, NULL);

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
rpmdig_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
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
static pgpDig
rpmdig_init(JSContext *cx, JSObject *obj)
{
    pgpDig dig = pgpDigNew(0);

if (_debug)
fprintf(stderr, "==> %s(%p,%p) dig %p\n", __FUNCTION__, cx, obj, dig);

    if (!JS_SetPrivate(cx, obj, (void *)dig)) {
	/* XXX error msg */
	return NULL;
    }
    return dig;
}

static void
rpmdig_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdigClass, NULL);
    pgpDig dig = ptr;

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);
    if (dig)
	dig = pgpDigFree(dig);
}

static JSBool
rpmdig_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)%s\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ((cx->fp->flags & JSFRAME_CONSTRUCTING) ? " constructing" : ""));

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	(void) rpmdig_init(cx, obj);
    } else {
	if ((obj = JS_NewObject(cx, &rpmdigClass, NULL, NULL)) == NULL)
	    goto exit;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

/* --- Class initialization */
JSClass rpmdigClass = {
    "Dig", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE,
    rpmdig_addprop,   rpmdig_delprop, rpmdig_getprop, rpmdig_setprop,
    (JSEnumerateOp)rpmdig_enumerate, (JSResolveOp)rpmdig_resolve,
    rpmdig_convert,	rpmdig_dtor,

    rpmdig_getobjectops,	rpmdig_checkaccess,
    rpmdig_call,		rpmdig_construct,
    rpmdig_xdrobject,	rpmdig_hasinstance,
    rpmdig_mark,		rpmdig_reserveslots,
};

JSObject *
rpmjs_InitDigClass(JSContext *cx, JSObject* obj)
{
    JSObject *proto;

if (_debug)
fprintf(stderr, "==> %s(%p,%p)\n", __FUNCTION__, cx, obj);

    proto = JS_InitClass(cx, obj, NULL, &rpmdigClass, rpmdig_ctor, 1,
		rpmdig_props, rpmdig_funcs, NULL, NULL);
assert(proto != NULL);
    return proto;
}

JSObject *
rpmjs_NewDigObject(JSContext *cx)
{
    JSObject *obj;
    pgpDig dig;

    if ((obj = JS_NewObject(cx, &rpmdigClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((dig = rpmdig_init(cx, obj)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

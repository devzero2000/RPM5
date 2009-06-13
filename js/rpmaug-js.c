/** \ingroup js_c
 * \file js/rpmaug-js.c
 */

#include "system.h"

#include "rpmaug-js.h"
#include "rpmjs-debug.h"

#define	_RPMPS_INTERNAL
#include <rpmaug.h>

#include "debug.h"

/*@unchecked@*/
static int _debug = 0;

#define	rpmaug_addprop	JS_PropertyStub
#define	rpmaug_delprop	JS_PropertyStub
#define	rpmaug_convert	JS_ConvertStub

/* --- helpers */

/* --- Object methods */
#ifdef	REFERENCE
static JSBool
rpmaug_print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmaugClass, NULL);
    rpmaug aug = ptr;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    rpmaugPrint(NULL, aug);

    ok = JS_TRUE;
    return ok;
}
#endif

static JSFunctionSpec rpmaug_funcs[] = {
#ifdef	REFERENCE
    JS_FS("push",	rpmaug_push,		0,0,0),
    JS_FS("print",	rpmaug_print,		0,0,0),
#endif
    JS_FS_END
};

/* --- Object properties */
enum rpmaug_tinyid {
    _DEBUG	= -2,
    _LENGTH	= -3,
};

static JSPropertySpec rpmaug_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
    {NULL, 0, 0, NULL, NULL}
};

static JSBool
rpmaug_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmaugClass, NULL);
    rpmaug aug = ptr;
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
rpmaug_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmaugClass, NULL);
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
rpmaug_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
	JSObject **objp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmaugClass, NULL);
    rpmaug aug = ptr;

_RESOLVE_DEBUG_ENTRY(_debug);

    if ((flags & JSRESOLVE_ASSIGNING)
     || (aug == NULL)) { /* don't resolve to parent prototypes objects. */
	*objp = NULL;
	goto exit;
    }

    *objp = obj;	/* XXX always resolve in this object. */

exit:
    return JS_TRUE;
}

static JSBool
rpmaug_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
		  jsval *statep, jsid *idp)
{

_ENUMERATE_DEBUG_ENTRY(_debug);

exit:
    return JS_TRUE;
}

/* --- Object ctors/dtors */
static rpmaug
rpmaug_init(JSContext *cx, JSObject *obj)
{
    rpmaug aug;

    if ((aug = rpmaugNew(NULL, NULL, 0)) == NULL)
	return NULL;
    if (!JS_SetPrivate(cx, obj, aug)) {
	/* XXX error msg */
	(void) rpmaugFree(aug);
	return NULL;
    }
    return aug;
}

static void
rpmaug_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmaugClass, NULL);
    rpmaug aug = ptr;

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);

    aug = rpmaugFree(aug);
}

static JSBool
rpmaug_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval);

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	if (rpmaug_init(cx, obj) == NULL)
	    goto exit;
    } else {
	if ((obj = JS_NewObject(cx, &rpmaugClass, NULL, NULL)) == NULL)
	    goto exit;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

/* --- Class initialization */
JSClass rpmaugClass = {
    "Aug", JSCLASS_NEW_RESOLVE | JSCLASS_HAS_PRIVATE,
    rpmaug_addprop,   rpmaug_delprop, rpmaug_getprop, rpmaug_setprop,
    (JSEnumerateOp)rpmaug_enumerate, (JSResolveOp)rpmaug_resolve,
    rpmaug_convert,	rpmaug_dtor,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *
rpmjs_InitAugClass(JSContext *cx, JSObject* obj)
{
    JSObject * o;

if (_debug)
fprintf(stderr, "==> %s(%p,%p)\n", __FUNCTION__, cx, obj);

    o = JS_InitClass(cx, obj, NULL, &rpmaugClass, rpmaug_ctor, 1,
		rpmaug_props, rpmaug_funcs, NULL, NULL);
assert(o != NULL);
    return o;
}

JSObject *
rpmjs_NewAugObject(JSContext *cx)
{
    JSObject *obj;
    rpmaug aug;

    if ((obj = JS_NewObject(cx, &rpmaugClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((aug = rpmaug_init(cx, obj)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

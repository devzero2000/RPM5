/** \ingroup js_c
 * \file js/rpmts-js.c
 */

#include "system.h"

#include "rpmts-js.h"

#include <rpmts.h>

#include "debug.h"

/*@unchecked@*/
static int _rpmjs_debug = 0;

/*@unchecked@*/
extern int _rpmts_debug;

/* --- Object methods */
static JSFunctionSpec rpmts_funcs[] = {
    {0}
};

/* --- Object properties */
enum rpmts_tinyid {
    TSID_DEBUG		= -2,
};

static JSPropertySpec rpmts_props[] = {
   {"debug",		TSID_DEBUG,	JSPROP_ENUMERATE },
   {0}
};

static JSBool
rpmts_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    rpmts ts = (rpmts) JS_GetInstancePrivate(cx, obj, &rpmtsClass, NULL);
    jsint tiny = JSVAL_TO_INT(id);
    JSBool ok = (ts ? JS_FALSE : JS_TRUE);

if (_rpmjs_debug)
fprintf(stderr, "==> %s(%p,%p,0x%llx,%p) %s = %s\n", __FUNCTION__, cx, obj, (unsigned long long)id, vp, JS_GetStringBytes(JS_ValueToString(cx, id)), JS_GetStringBytes(JS_ValueToString(cx, *vp)));

    /* XXX return JS_TRUE if private == NULL */

    switch (tiny) {
    case TSID_DEBUG:
	*vp = INT_TO_JSVAL(_rpmts_debug);
	ok = JS_TRUE;
	break;
    default:
	break;
    }
    return ok;
}

static JSBool
rpmts_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    rpmts ts = (rpmts) JS_GetInstancePrivate(cx, obj, &rpmtsClass, NULL);
    jsint tiny = JSVAL_TO_INT(id);
    JSBool ok = (ts ? JS_FALSE : JS_TRUE);

if (_rpmjs_debug)
fprintf(stderr, "==> %s(%p,%p,0x%llx,%p) %s = %s\n", __FUNCTION__, cx, obj, (unsigned long long)id, vp, JS_GetStringBytes(JS_ValueToString(cx, id)), JS_GetStringBytes(JS_ValueToString(cx, *vp)));

    /* XXX return JS_TRUE if ... ? */

    switch (tiny) {
    case TSID_DEBUG:
	if (JS_ValueToInt32(cx, *vp, &_rpmts_debug))
	    ok = JS_TRUE;
	break;
    default:
	break;
    }
    return ok;
}

/* --- Object ctors/dtors */
static void
rpmts_dtor(JSContext *cx, JSObject *obj)
{
    rpmts ts = (rpmts) JS_GetInstancePrivate(cx, obj, &rpmtsClass, NULL);

if (_rpmjs_debug)
fprintf(stderr, "==> %s(%p,%p)\n", __FUNCTION__, cx, obj);

    (void)rpmtsFree(ts);
}

static JSBool
rpmts_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_FALSE;

if (_rpmjs_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval);

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	rpmts ts = rpmtsCreate();

	if (ts == NULL || !JS_SetPrivate(cx, obj, ts)) {
	    (void) rpmtsFree(ts);
	    goto exit;
	}
    } else {
	if ((obj = JS_NewObject(cx, &rpmtsClass, NULL, NULL)) == NULL)
	    goto exit;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

/* --- Class initialization */
JSClass rpmtsClass = {
    "Ts", JSCLASS_HAS_PRIVATE | JSCLASS_HAS_CACHED_PROTO(JSProto_Object),
    JS_PropertyStub,  JS_PropertyStub,  rpmts_getprop,  rpmts_setprop,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,    rpmts_dtor
};

JSObject *
rpmjs_InitTsClass(JSContext *cx, JSObject* obj)
{
    JSObject * tso;
    tso = JS_InitClass(cx, obj, NULL, &rpmtsClass, rpmts_ctor, 1,
        rpmts_props, rpmts_funcs, NULL, NULL);
assert(tso != NULL);
    return tso;
}

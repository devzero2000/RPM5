/** \ingroup js_c
 * \file js/rpmmi-js.c
 */

#include "system.h"

#include "rpmts-js.h"
#include "rpmmi-js.h"
#include "rpmhdr-js.h"
#include "rpmjs-debug.h"

#include <rpmdb.h>
#include <rpmts.h>

#include "debug.h"

/*@unchecked@*/
static int _debug = 1;

/* --- helpers */

/* --- Object methods */
static JSBool
rpmmi_next(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmiClass, NULL);
    rpmdbMatchIterator mi = ptr;
    Header h;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    if ((h = rpmdbNextIterator(mi)) != NULL) {
	JSObject *Hdr = rpmjs_NewHdrObject(cx, h);
	*rval = OBJECT_TO_JSVAL(Hdr);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

static JSFunctionSpec rpmmi_funcs[] = {
    {"next",	rpmmi_next,		0,0,0},
    JS_FS_END
};

/* --- Object properties */
enum rpmmi_tinyid {
    _DEBUG	= -2,
};

static JSPropertySpec rpmmi_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
    {NULL, 0, 0, NULL, NULL}
};

static JSBool
rpmmi_addprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmiClass, NULL);
_PROP_DEBUG_ENTRY(_debug < 0);
    return JS_TRUE;
}

static JSBool
rpmmi_delprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmiClass, NULL);
_PROP_DEBUG_ENTRY(_debug < 0);
    return JS_TRUE;
}
static JSBool
rpmmi_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmiClass, NULL);
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
    return ok;
}

static JSBool
rpmmi_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmiClass, NULL);
    jsint tiny = JSVAL_TO_INT(id);
    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    JSBool ok = (ptr ? JS_FALSE : JS_TRUE);

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
rpmmi_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
	JSObject **objp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmiClass, NULL);
_RESOLVE_DEBUG_ENTRY(_debug);
    return JS_TRUE;
}

static JSBool
rpmmi_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
		  jsval *statep, jsid *idp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmiClass, NULL);
    JSObject *iterator = NULL;
    JSBool ok = JS_FALSE;

_ENUMERATE_DEBUG_ENTRY(_debug);

    switch (op) {
    case JSENUMERATE_INIT:
	if ((iterator = JS_NewPropertyIterator(cx, obj)) == NULL)
	    goto exit;
	*statep = OBJECT_TO_JSVAL(iterator);
	if (idp)
	    *idp = JSVAL_ZERO;
fprintf(stderr, "\tINIT iter %p\n", iterator);
	break;
    case JSENUMERATE_NEXT:
	iterator = (JSObject *) JSVAL_TO_OBJECT(*statep);
fprintf(stderr, "\tNEXT iter %p\n", iterator);
	if (!JS_NextProperty(cx, iterator, idp))
	    goto exit;
	if (*idp != JSVAL_VOID)
	    break;
	/*@fallthrough@*/
    case JSENUMERATE_DESTROY:
fprintf(stderr, "\tFINI iter %p\n", iterator);
	/* Allow our iterator object to be GC'd. */
	*statep = JSVAL_NULL;
	break;
    }
    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmmi_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmiClass, NULL);
_CONVERT_DEBUG_ENTRY(_debug);
    return JS_TRUE;
}

/* --- Object ctors/dtors */
static rpmdbMatchIterator
rpmmi_init(JSContext *cx, JSObject *obj, rpmts ts, int _tag, void * _key, int _keylen)
{
    rpmdbMatchIterator mi;

    if ((mi = rpmtsInitIterator(ts, _tag, _key, _keylen)) == NULL)
	return NULL;
    if (!JS_SetPrivate(cx, obj, (void *)mi)) {
	/* XXX error msg */
	mi = rpmdbFreeIterator(mi);
	return NULL;
    }
    return mi;
}

static void
rpmmi_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmiClass, NULL);
    rpmdbMatchIterator mi = ptr;

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);

#ifdef	BUGGY
    mi = rpmdbFreeIterator(mi);
#endif
}

static JSBool
rpmmi_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_FALSE;
    JSObject *tso = NULL;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "o", &tso)))
	goto exit;

fprintf(stderr, "\ttso %p ptr %p\n", tso, JS_GetPrivate(cx, tso));

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	rpmts ts = JS_GetInstancePrivate(cx, tso, &rpmtsClass, NULL);
	rpmTag tag = RPMDBI_PACKAGES;
	char * key = NULL;
	int keylen = 0;
	if (ts == NULL || rpmmi_init(cx, obj, ts, tag, key, keylen))
	    goto exit;
    } else {
	if ((obj = JS_NewObject(cx, &rpmmiClass, NULL, NULL)) == NULL)
	    goto exit;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

/* --- Class initialization */
JSClass rpmmiClass = {
    "Mi", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE,
    rpmmi_addprop,   rpmmi_delprop, rpmmi_getprop, rpmmi_setprop,
    (JSEnumerateOp)rpmmi_enumerate, (JSResolveOp)rpmmi_resolve,
    rpmmi_convert,	rpmmi_dtor,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *
rpmjs_InitMiClass(JSContext *cx, JSObject* obj)
{
    JSObject * o;

if (_debug)
fprintf(stderr, "==> %s(%p,%p)\n", __FUNCTION__, cx, obj);

    o = JS_InitClass(cx, obj, NULL, &rpmmiClass, rpmmi_ctor, 1,
		rpmmi_props, rpmmi_funcs, NULL, NULL);
assert(o != NULL);
    return o;
}

JSObject *
rpmjs_NewMiObject(JSContext *cx, void * _ts, int _tag, void *_key, int _keylen)
{
    JSObject *obj;
    rpmdbMatchIterator mi;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%s(%u),%p[%u]) _key %s\n", __FUNCTION__, cx, _ts, tagName(_tag), (unsigned)_tag, _key, (unsigned)_keylen, (const char *)(_key ? _key : ""));

    if ((obj = JS_NewObject(cx, &rpmmiClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((mi = rpmmi_init(cx, obj, _ts, _tag, _key, _keylen)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

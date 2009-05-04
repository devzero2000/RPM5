/** \ingroup js_c
 * \file js/rpmte-js.c
 */

#include "system.h"

#include "rpmts-js.h"
#include "rpmte-js.h"
#include "rpmhdr-js.h"
#include "rpmjs-debug.h"

#include <argv.h>
#include <mire.h>

#include <rpmdb.h>

#include <rpmal.h>
#include <rpmts.h>
#define	_RPMTE_INTERNAL		/* XXX for rpmteNew/rpmteFree */
#include <rpmte.h>

#include "debug.h"

/*@unchecked@*/
static int _debug = -1;

/* --- helpers */

/* --- Object methods */

static JSFunctionSpec rpmte_funcs[] = {
    JS_FS_END
};

/* --- Object properties */
enum rpmte_tinyid {
    _DEBUG	= -2,
};

static JSPropertySpec rpmte_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
    {NULL, 0, 0, NULL, NULL}
};

static JSBool
rpmte_addprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmteClass, NULL);
_PROP_DEBUG_ENTRY(_debug < 0);
    return JS_TRUE;
}

static JSBool
rpmte_delprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmteClass, NULL);
_PROP_DEBUG_ENTRY(_debug < 0);
    return JS_TRUE;
}

static JSBool
rpmte_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmteClass, NULL);
    rpmte te = ptr;
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
rpmte_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmteClass, NULL);
    rpmte te = (rpmte)ptr;
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
rpmte_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
	JSObject **objp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmteClass, NULL);
    JSBool ok = JS_FALSE;

_RESOLVE_DEBUG_ENTRY(_debug);

    if (flags & JSRESOLVE_ASSIGNING) {
	*objp = NULL;
	ok = JS_TRUE;
	goto exit;
    }

    *objp = obj;        /* XXX always resolve in this object. */

    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmte_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
		  jsval *statep, jsid *idp)
{
    JSBool ok = JS_FALSE;

_ENUMERATE_DEBUG_ENTRY(_debug);

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
rpmte_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmteClass, NULL);
_CONVERT_DEBUG_ENTRY(_debug);
    return JS_TRUE;
}

/* --- Object ctors/dtors */
static rpmte
rpmte_init(JSContext *cx, JSObject *obj, rpmts ts, JSObject *hdro)
{
    rpmte te = NULL;
    rpmElementType etype = TR_ADDED;
    fnpyKey key = NULL;
    rpmRelocation relocs = NULL;
    int dboffset = 0;
    alKey pkgKey = NULL;

    if (hdro != NULL) {
	Header h = JS_GetInstancePrivate(cx, hdro, &rpmhdrClass, NULL);
	if ((te = rpmteNew(ts, h, etype, key, relocs, dboffset, pkgKey)) == NULL)
	    return NULL;
    }
    if (!JS_SetPrivate(cx, obj, (void *)te)) {
	/* XXX error msg */
	(void) rpmteFree(te);
	return NULL;
    }
    return te;
}

static void
rpmte_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmteClass, NULL);
    rpmte te = ptr;

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);

    if (te != NULL)
	(void) rpmteFree(te);
}

static JSBool
rpmte_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_FALSE;
    JSObject *tso = NULL;
    JSObject *hdro = NULL;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "o/o", &tso, &hdro)))
	goto exit;

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	rpmts ts = JS_GetInstancePrivate(cx, tso, &rpmtsClass, NULL);
	if (rpmte_init(cx, obj, ts, hdro) == NULL)
	    goto exit;
    } else {
	if ((obj = JS_NewObject(cx, &rpmteClass, NULL, NULL)) == NULL)
	    goto exit;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

/* --- Class initialization */
JSClass rpmteClass = {
    "Te", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE,
    rpmte_addprop,   rpmte_delprop, rpmte_getprop, rpmte_setprop,
    (JSEnumerateOp)rpmte_enumerate, (JSResolveOp)rpmte_resolve,
    rpmte_convert,	rpmte_dtor,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *
rpmjs_InitTeClass(JSContext *cx, JSObject* obj)
{
    JSObject * o;

if (_debug)
fprintf(stderr, "==> %s(%p,%p)\n", __FUNCTION__, cx, obj);

    o = JS_InitClass(cx, obj, NULL, &rpmteClass, rpmte_ctor, 1,
		rpmte_props, rpmte_funcs, NULL, NULL);
assert(o != NULL);
    return o;
}

JSObject *
rpmjs_NewTeObject(JSContext *cx, void * _ts, void * _hdro)
{
    JSObject *obj;
    rpmte te;

    if ((obj = JS_NewObject(cx, &rpmteClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((te = rpmte_init(cx, obj, _ts, _hdro)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

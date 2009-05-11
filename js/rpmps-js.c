/** \ingroup js_c
 * \file js/rpmps-js.c
 */

#include "system.h"

#include "rpmps-js.h"
#include "rpmjs-debug.h"

#define	_RPMPS_INTERNAL
#include <rpmps.h>

#include "debug.h"

/*@unchecked@*/
static int _debug = 0;

#define	rpmps_addprop	JS_PropertyStub
#define	rpmps_delprop	JS_PropertyStub
#define	rpmps_convert	JS_ConvertStub

/* --- helpers */

/* --- Object methods */
static JSBool
rpmps_push(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmpsClass, NULL);
    rpmps ps = ptr;
    char *pkgNEVR;
    char *altNEVR;
    JSObject *o;
    rpmProblemType type;
    char *dn;
    char *bn;
    uint32 ui;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "ssoissu",
		&pkgNEVR, &altNEVR, &o, &type, &dn, &bn, &ui)))
	goto exit;
    rpmpsAppend(ps, type, pkgNEVR, o, dn, bn, altNEVR, ui);

    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmps_print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmpsClass, NULL);
    rpmps ps = ptr;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    rpmpsPrint(NULL, ps);

    ok = JS_TRUE;
    return ok;
}

static JSFunctionSpec rpmps_funcs[] = {
    JS_FS("push",	rpmps_push,		0,0,0),
    JS_FS("print",	rpmps_print,		0,0,0),
    JS_FS_END
};

/* --- Object properties */
enum rpmps_tinyid {
    _DEBUG	= -2,
    _LENGTH	= -3,
};

static JSPropertySpec rpmps_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"length",	_LENGTH,	JSPROP_ENUMERATE,	NULL,	NULL},
    {NULL, 0, 0, NULL, NULL}
};

static JSBool
rpmps_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmpsClass, NULL);
    rpmps ps = ptr;
    jsint tiny = JSVAL_TO_INT(id);

    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    if (ptr == NULL)
	return JS_TRUE;

    switch (tiny) {
    case _DEBUG:
	*vp = INT_TO_JSVAL(_debug);
	break;
    case _LENGTH:
	*vp = INT_TO_JSVAL(rpmpsNumProblems(ps));
	break;
    default:
	break;
    }

    return JS_TRUE;
}

static JSBool
rpmps_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmpsClass, NULL);
    rpmps ps = (rpmps)ptr;
    jsint tiny = JSVAL_TO_INT(id);
    int myint;

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
rpmps_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
	JSObject **objp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmpsClass, NULL);
    rpmps ps = ptr;

_RESOLVE_DEBUG_ENTRY(_debug);

    if ((flags & JSRESOLVE_ASSIGNING)
     || (ps == NULL)) {	/* don't resolve to parent prototypes objects. */
	*objp = NULL;
	goto exit;
    }

    *objp = obj;	/* XXX always resolve in this object. */

exit:
    return JS_TRUE;
}

static JSBool
rpmps_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
		  jsval *statep, jsid *idp)
{
    JSObject *iterator;
    JSBool ok = JS_FALSE;

_ENUMERATE_DEBUG_ENTRY(_debug);

#ifdef	DYING
    switch (op) {
    case JSENUMERATE_INIT:
	if ((iterator = JS_NewPropertyIterator(cx, obj)) == NULL)
	    goto exit;
	*statep = OBJECT_TO_JSVAL(iterator);
	if (idp)
	    *idp = JSVAL_ZERO;
	break;
    case JSENUMERATE_NEXT:
	iterator = (JSObject *) JSVAL_TO_OBJECT(*statep);
	if (!JS_NextProperty(cx, iterator, idp))
	    goto exit;
	if (*idp != JSVAL_VOID)
	    break;
	/*@fallthrough@*/
    case JSENUMERATE_DESTROY:
	/* Allow our iterator object to be GC'd. */
	*statep = JSVAL_NULL;
	break;
    }
#else
    {	static const char hex[] = "0123456789abcdef";
	const char * s;
	char name[2];
	JSString * valstr;
	char value[5];
	for (s = "AaBbCc"; *s != '\0'; s++) {
	    name[0] = s[0]; name[1] = '\0';
	    value[0] = '0'; value[1] = 'x';
	    value[2] = hex[(name[0] >> 4) & 0xf];
	    value[3] = hex[(name[0]     ) & 0xf];
	    value[4] = '\0';
 	    if ((valstr = JS_NewStringCopyZ(cx, value)) == NULL
	     || !JS_DefineProperty(cx, obj, name, STRING_TO_JSVAL(valstr),
				NULL, NULL, JSPROP_ENUMERATE))
		goto exit;
	}
    }
#endif
exit:
    return JS_TRUE;
}

/* --- Object ctors/dtors */
static rpmps
rpmps_init(JSContext *cx, JSObject *obj)
{
    rpmps ps;

    if ((ps = rpmpsCreate()) == NULL)
	return NULL;
    if (!JS_SetPrivate(cx, obj, ps)) {
	/* XXX error msg */
	(void) rpmpsFree(ps);
	return NULL;
    }
    return ps;
}

static void
rpmps_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmpsClass, NULL);
    rpmps ps = ptr;

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);

    ps = rpmpsFree(ps);
}

static JSBool
rpmps_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval);

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	if (rpmps_init(cx, obj) == NULL)
	    goto exit;
    } else {
	if ((obj = JS_NewObject(cx, &rpmpsClass, NULL, NULL)) == NULL)
	    goto exit;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

/* --- Class initialization */
JSClass rpmpsClass = {
    "Ps", JSCLASS_NEW_RESOLVE | JSCLASS_HAS_PRIVATE,
    rpmps_addprop,   rpmps_delprop, rpmps_getprop, rpmps_setprop,
    (JSEnumerateOp)rpmps_enumerate, (JSResolveOp)rpmps_resolve,
    rpmps_convert,	rpmps_dtor,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *
rpmjs_InitPsClass(JSContext *cx, JSObject* obj)
{
    JSObject * o;

if (_debug)
fprintf(stderr, "==> %s(%p,%p)\n", __FUNCTION__, cx, obj);

    o = JS_InitClass(cx, obj, NULL, &rpmpsClass, rpmps_ctor, 1,
		rpmps_props, rpmps_funcs, NULL, NULL);
assert(o != NULL);
    return o;
}

JSObject *
rpmjs_NewPsObject(JSContext *cx)
{
    JSObject *obj;
    rpmps ps;

    if ((obj = JS_NewObject(cx, &rpmpsClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((ps = rpmps_init(cx, obj)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

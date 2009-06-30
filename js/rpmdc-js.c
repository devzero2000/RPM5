/** \ingroup js_c
 * \file js/rpmdc-js.c
 */

#include "system.h"

#include "rpmdc-js.h"
#include "rpmjs-debug.h"

#include "debug.h"

/*@unchecked@*/
static int _debug = 0;

#define	rpmdc_addprop	JS_PropertyStub
#define	rpmdc_delprop	JS_PropertyStub
#define	rpmdc_convert	JS_ConvertStub

typedef	DIGEST_CTX rpmdc;	/* XXX use rpmdc from tools/digest.c instead */

/* --- helpers */

/* --- Object methods */
static JSBool
rpmdc_Init(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdcClass, NULL);
    rpmdc dc = ptr;
    JSBool ok = JS_FALSE;
    unsigned int _dalgo = PGPHASHALGO_MD5;
    unsigned int _flags = RPMDIGEST_NONE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "/uu", &_dalgo, &_flags)))
        goto exit;

    if (dc) {
	(void) rpmDigestFinal(dc, NULL, NULL, 0);
	(void) JS_SetPrivate(cx, obj, (void *)NULL);
    }
    dc = rpmDigestInit(_dalgo, _flags);
    (void) JS_SetPrivate(cx, obj, (void *)dc);

    *rval = JSVAL_TRUE;
    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmdc_Update(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdcClass, NULL);
    rpmdc dc = ptr;
    JSBool ok = JS_FALSE;
    const char * s = NULL;
    size_t ns = 0;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    *rval = JSVAL_FALSE;
    if (!(ok = JS_ConvertArguments(cx, argc, argv, "s", &s)))
        goto exit;
    if (dc == NULL)
	goto exit;

    if (s && (ns = strlen(s)) > 0)
	(void) rpmDigestUpdate(dc, s, ns);
    *rval = JSVAL_TRUE;

    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmdc_Fini(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdcClass, NULL);
    rpmdc dc = ptr;
    JSBool ok = JS_FALSE;
    const char * s = NULL;
    size_t ns = 0;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    *rval = JSVAL_FALSE;
    if (dc == NULL)
	goto exit;

    (void) rpmDigestFinal(dc, &s, &ns, 1);
    *rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, s));

    (void) JS_SetPrivate(cx, obj, (void *)NULL);

    ok = JS_TRUE;
exit:
    return ok;
}

static JSFunctionSpec rpmdc_funcs[] = {
    JS_FS("init",	rpmdc_Init,		0,0,0),
    JS_FS("update",	rpmdc_Update,		0,0,0),
    JS_FS("fini",	rpmdc_Fini,		0,0,0),
    JS_FS_END
};

/* --- Object properties */
enum rpmdc_tinyid {
    _DEBUG	= -2,
};

static JSPropertySpec rpmdc_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
    {NULL, 0, 0, NULL, NULL}
};

static JSBool
rpmdc_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdcClass, NULL);
    jsint tiny = JSVAL_TO_INT(id);

_PROP_DEBUG_ENTRY(_debug < 0);

    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    if (ptr == NULL)
	return JS_TRUE;

    switch (tiny) {
    case _DEBUG:	*vp = INT_TO_JSVAL(_debug);		break;
    default:
	break;
    }

    return JS_TRUE;
}

static JSBool
rpmdc_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdcClass, NULL);
    jsint tiny = JSVAL_TO_INT(id);

_PROP_DEBUG_ENTRY(_debug < 0);

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

#ifdef	NOTYET
static JSBool
rpmdc_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
	JSObject **objp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdcClass, NULL);

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
rpmdc_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
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
#else
#define	rpmdc_resolve	JS_ResolveStub
#define	rpmdc_enumerate	JS_EnumerateStub
#endif

/* --- Object ctors/dtors */
static rpmdc
rpmdc_init(JSContext *cx, JSObject *obj, unsigned int _dalgo,
		unsigned int _flags)
{
    rpmdc dc = (_dalgo > 0 ? rpmDigestInit(_dalgo, _flags) : NULL);

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%u,%u) dc %p\n", __FUNCTION__, cx, obj, _dalgo, _flags, dc);

    if (!JS_SetPrivate(cx, obj, (void *)dc)) {
	/* XXX error msg */
	return NULL;
    }
    return dc;
}

static void
rpmdc_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdcClass, NULL);
    rpmdc dc = ptr;

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);
    if (dc) {
	(void) rpmDigestFinal(dc, NULL, NULL, 0);
    }
}

static JSBool
rpmdc_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_FALSE;
    unsigned int _dalgo = PGPHASHALGO_NONE;
    unsigned int _flags = RPMDIGEST_NONE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)%s\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ((cx->fp->flags & JSFRAME_CONSTRUCTING) ? " constructing" : ""));

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "/uu", &_dalgo, &_flags)))
        goto exit;

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	(void) rpmdc_init(cx, obj, _dalgo, _flags);
    } else {
	if ((obj = JS_NewObject(cx, &rpmdcClass, NULL, NULL)) == NULL)
	    goto exit;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

/* --- Class initialization */
JSClass rpmdcClass = {
    /* XXX class should be "Digest" eventually, avoid name conflicts for now */
#ifdef	NOTYET
    "Dc", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE,
#else
    "Dc", JSCLASS_HAS_PRIVATE,
#endif
    rpmdc_addprop,   rpmdc_delprop, rpmdc_getprop, rpmdc_setprop,
    (JSEnumerateOp)rpmdc_enumerate, (JSResolveOp)rpmdc_resolve,
    rpmdc_convert,	rpmdc_dtor,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *
rpmjs_InitDcClass(JSContext *cx, JSObject* obj)
{
    JSObject *proto;

if (_debug)
fprintf(stderr, "==> %s(%p,%p)\n", __FUNCTION__, cx, obj);

    proto = JS_InitClass(cx, obj, NULL, &rpmdcClass, rpmdc_ctor, 1,
		rpmdc_props, rpmdc_funcs, NULL, NULL);
assert(proto != NULL);
    return proto;
}

JSObject *
rpmjs_NewDcObject(JSContext *cx, unsigned int _dalgo, unsigned int _flags)
{
    JSObject *obj;
    rpmdc dc;

    if ((obj = JS_NewObject(cx, &rpmdcClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((dc = rpmdc_init(cx, obj, _dalgo, _flags)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

/** \ingroup js_c
 * \file js/rpmfi-js.c
 */

#include "system.h"

#include "rpmfi-js.h"
#include "rpmhdr-js.h"

#include <argv.h>
#include <mire.h>

#include <rpmdb.h>

#include <rpmfi.h>

#include "debug.h"

/*@unchecked@*/
extern int _rpmjs_debug;

/*@unchecked@*/
static int _debug = 1;

/* --- Object methods */

static JSFunctionSpec rpmfi_funcs[] = {
    JS_FS_END
};

/* --- Object properties */
enum rpmfi_tinyid {
    _DEBUG	= -2,
};

static JSPropertySpec rpmfi_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
    {NULL, 0, 0, NULL, NULL}
};

static JSBool
rpmfi_addprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmfiClass, NULL);

if (_debug < 0)
fprintf(stderr, "==> %s(%p,%p,0x%lx[%u],%p) ptr %p %s = %s\n", __FUNCTION__, cx, obj, (unsigned long)id, (unsigned)JSVAL_TAG(id), vp, ptr, JS_GetStringBytes(JS_ValueToString(cx, id)), JS_GetStringBytes(JS_ValueToString(cx, *vp)));

    return JS_TRUE;
}

static JSBool
rpmfi_delprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmfiClass, NULL);

if (_debug)
fprintf(stderr, "==> %s(%p,%p,0x%lx[%u],%p) ptr %p %s = %s\n", __FUNCTION__, cx, obj, (unsigned long)id, (unsigned)JSVAL_TAG(id), vp, ptr, JS_GetStringBytes(JS_ValueToString(cx, id)), JS_GetStringBytes(JS_ValueToString(cx, *vp)));

    return JS_TRUE;
}
static JSBool
rpmfi_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmfiClass, NULL);
    rpmfi fi = ptr;
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
if (_debug) {
fprintf(stderr, "==> %s(%p,%p,0x%lx[%u],%p) ptr %p %s = %s\n", __FUNCTION__, cx, obj, (unsigned long)id, (unsigned)JSVAL_TAG(id), vp, ptr, JS_GetStringBytes(JS_ValueToString(cx, id)), JS_GetStringBytes(JS_ValueToString(cx, *vp)));
ok = JS_TRUE;		/* XXX return JS_TRUE iff ... ? */
}
    }
    return ok;
}

static JSBool
rpmfi_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmfiClass, NULL);
    rpmfi fi = (rpmfi)ptr;
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
if (_debug) {
fprintf(stderr, "==> %s(%p,%p,0x%lx[%u],%p) ptr %p %s = %s\n", __FUNCTION__, cx, obj, (unsigned long)id, (unsigned)JSVAL_TAG(id), vp, ptr, JS_GetStringBytes(JS_ValueToString(cx, id)), JS_GetStringBytes(JS_ValueToString(cx, *vp)));
ok = JS_TRUE;		/* XXX return JS_TRUE iff ... ? */
}
    }
    return ok;
}

static JSBool
rpmfi_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
	JSObject **objp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmfiClass, NULL);
    static char hex[] = "0123456789abcdef";
    JSString *idstr;
    const char * name;
    JSString * valstr;
    char value[5];
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,0x%lx[%u],0x%x,%p) ptr %p property %s flags 0x%x{%s,%s,%s,%s,%s}\n", __FUNCTION__, cx, obj, (unsigned long)id, (unsigned)JSVAL_TAG(id), (unsigned)flags, objp, ptr,
		JS_GetStringBytes(JS_ValueToString(cx, id)), flags,
		(flags & JSRESOLVE_QUALIFIED) ? "qualified" : "",
		(flags & JSRESOLVE_ASSIGNING) ? "assigning" : "",
		(flags & JSRESOLVE_DETECTING) ? "detecting" : "",
		(flags & JSRESOLVE_DETECTING) ? "declaring" : "",
		(flags & JSRESOLVE_DETECTING) ? "classname" : "");

    if (flags & JSRESOLVE_ASSIGNING) {
	ok = JS_TRUE;
	goto exit;
    }

    if ((idstr = JS_ValueToString(cx, id)) == NULL)
	goto exit;

    name = JS_GetStringBytes(idstr);
    if (name[1] == '\0' && xisalpha(name[0])) {
	value[0] = '0'; value[1] = 'x';
	value[2] = hex[(name[0] >> 4) & 0xf];
	value[3] = hex[(name[0]     ) & 0xf];
	value[4] = '\0';
 	if ((valstr = JS_NewStringCopyZ(cx, value)) == NULL
	 || !JS_DefineProperty(cx, obj, name, STRING_TO_JSVAL(valstr),
				NULL, NULL, JSPROP_ENUMERATE))
	    goto exit;
	*objp = obj;
    }
    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmfi_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
		  jsval *statep, jsid *idp)
{
    JSObject *iterator;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%d,%p,%p)\n", __FUNCTION__, cx, obj, op, statep, idp);

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
    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmfi_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmfiClass, NULL);
if (_debug)
fprintf(stderr, "==> %s(%p,%p,%d,%p) ptr %p convert to %s\n", __FUNCTION__, cx, obj, type, vp, ptr, JS_GetTypeName(cx, type));
    return JS_TRUE;
}

/* --- Object ctors/dtors */
static rpmfi
rpmfi_init(JSContext *cx, JSObject *obj, rpmts ts, Header h, int _tagN)
{
    rpmfi fi;
    int flags = 0;

    if ((fi = rpmfiNew(ts, h, _tagN, flags)) == NULL)
	return NULL;
    if (!JS_SetPrivate(cx, obj, (void *)fi)) {
	/* XXX error msg */
	(void) rpmfiFree(fi);
	return NULL;
    }
    return fi;
}

static void
rpmfi_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmfiClass, NULL);
    rpmfi fi = ptr;

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);

    (void) rpmfiFree(fi);
}

static JSBool
rpmfi_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_FALSE;
    rpmts ts = NULL;	/* XXX FIXME: should be a ts method? */
    JSObject *hdro = NULL;
    int tagN = RPMTAG_BASENAMES;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "o/i", &hdro, &tagN)))
	goto exit;

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	Header h = JS_GetInstancePrivate(cx, hdro, &rpmhdrClass, NULL);
	if (rpmfi_init(cx, obj, ts, h, tagN) == NULL)
	    goto exit;
    } else {
	if ((obj = JS_NewObject(cx, &rpmfiClass, NULL, NULL)) == NULL)
	    goto exit;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

/* --- Class initialization */
#ifdef	HACKERY
JSClass rpmfiClass = {
    "Fi", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE | JSCLASS_HAS_CACHED_PROTO(JSProto_Object),
    rpmfi_addprop,   rpmfi_delprop, rpmfi_getprop, rpmfi_setprop,
    (JSEnumerateOp)rpmfi_enumerate, (JSResolveOp)rpmfi_resolve,
    rpmfi_convert,	rpmfi_dtor,
    JSCLASS_NO_OPTIONAL_MEMBERS
};
#else
JSClass rpmfiClass = {
    "Fi", JSCLASS_NEW_RESOLVE | JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,   JS_PropertyStub, rpmfi_getprop, JS_PropertyStub,
    (JSEnumerateOp)rpmfi_enumerate, (JSResolveOp)rpmfi_resolve,
    JS_ConvertStub,	rpmfi_dtor,
    JSCLASS_NO_OPTIONAL_MEMBERS
};
#endif

JSObject *
rpmjs_InitFiClass(JSContext *cx, JSObject* obj)
{
    JSObject * o;

if (_debug)
fprintf(stderr, "==> %s(%p,%p)\n", __FUNCTION__, cx, obj);

    o = JS_InitClass(cx, obj, NULL, &rpmfiClass, rpmfi_ctor, 1,
		rpmfi_props, rpmfi_funcs, NULL, NULL);
assert(o != NULL);
    return o;
}

JSObject *
rpmjs_NewFiObject(JSContext *cx, void * _h, int _tagN)
{
    JSObject *obj;
    rpmts ts = NULL;	/* XXX FIXME: should be a ts method? */
    rpmfi fi;

    if ((obj = JS_NewObject(cx, &rpmfiClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((fi = rpmfi_init(cx, obj, ts, _h, _tagN)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

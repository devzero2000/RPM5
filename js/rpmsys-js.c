/** \ingroup js_c
 * \file js/rpmsys-js.c
 */

#include "system.h"

#include "rpmsys-js.h"
#include "rpmjs-debug.h"
#include <rpmio.h>

#include "debug.h"

/*@unchecked@*/
static int _debug = 0;

/* Required JSClass vectors */
#define	rpmsys_addprop		JS_PropertyStub
#define	rpmsys_delprop		JS_PropertyStub
#define	rpmsys_convert		JS_ConvertStub

/* Optional JSClass vectors */
#define	rpmsys_getobjectops	NULL
#define	rpmsys_checkaccess	NULL
#define	rpmsys_call		NULL
#define	rpmsys_construct	rpmsys_ctor
#define	rpmsys_xdrobject	NULL
#define	rpmsys_hasinstance	NULL
#define	rpmsys_mark		NULL
#define	rpmsys_reserveslots	NULL

/* Extended JSClass vectors */
#define rpmsys_equality		NULL
#define rpmsys_outerobject	NULL
#define rpmsys_innerobject	NULL
#define rpmsys_iteratorobject	NULL
#define rpmsys_wrappedobject	NULL

typedef void * rpmsys;

/* --- helpers */
static rpmsys
rpmsys_init(JSContext *cx, JSObject *obj)
{
    rpmsys sys = xcalloc(1, sizeof(sys)); /* XXX distinguish class/instance? */

    if (!JS_SetPrivate(cx, obj, (void *)sys)) {
	/* XXX error msg */
	sys = NULL;
    }

if (_debug)
fprintf(stderr, "<== %s(%p,%p) sys %p\n", __FUNCTION__, cx, obj, sys);

    return sys;
}

/* --- Object methods */
static JSBool
rpmsys_mkdir(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmsysClass, NULL);
    rpmsys sys = ptr;
    const char * _path = NULL;
    jsuint _mode = 0755;
    mode_t mode;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "s/u", &_path, &_mode)))
        goto exit;

    mode = _mode;
    *rval = (sys && !Mkdir(_path, mode) ? JSVAL_ZERO : INT_TO_JSVAL(errno));

    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmsys_rmdir(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmsysClass, NULL);
    rpmsys sys = ptr;
    const char * _path = NULL;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "s", &_path)))
        goto exit;

    *rval = (sys && !Rmdir(_path) ? JSVAL_ZERO : INT_TO_JSVAL(errno));

    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmsys_unlink(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmsysClass, NULL);
    rpmsys sys = ptr;
    const char * _path = NULL;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "s", &_path)))
        goto exit;

    *rval = (sys && !Unlink(_path) ? JSVAL_ZERO : INT_TO_JSVAL(errno));

    ok = JS_TRUE;
exit:
    return ok;
}

static JSFunctionSpec rpmsys_funcs[] = {
    JS_FS("mkdir",	rpmsys_mkdir,		0,0,0),
    JS_FS("rmdir",	rpmsys_rmdir,		0,0,0),
    JS_FS("unlink",	rpmsys_unlink,		0,0,0),
    JS_FS_END
};

/* --- Object properties */
enum rpmsys_tinyid {
    _DEBUG	= -2,
};

static JSPropertySpec rpmsys_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
    {NULL, 0, 0, NULL, NULL}
};

static JSBool
rpmsys_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmsysClass, NULL);
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
rpmsys_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmsysClass, NULL);
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

static JSBool
rpmsys_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
		JSObject **objp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmsysClass, NULL);

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
rpmsys_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
		  jsval *statep, jsid *idp)
{
#ifdef	NOTYET
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmsysClass, NULL);
    rpmsys sys = ptr;
#endif

    /* XXX VG: JS_Enumerate (jsobj.c:4211) doesn't initialize some fields. */
_ENUMERATE_DEBUG_ENTRY(_debug < 0);

#ifdef	NOTYET
    switch (op) {
    case JSENUMERATE_INIT:
	if (idp)
	    *idp = JSVAL_ZERO;
	*statep = INT_TO_JSVAL(ix);
if (_debug)
fprintf(stderr, "\tINIT sys %p\n", sys);
        break;
    case JSENUMERATE_NEXT:
	ix = JSVAL_TO_INT(*statep);
	if ((dp = Readdir(dir)) != NULL) {
	    (void) JS_DefineElement(cx, obj,
			ix, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, dp->d_name)),
			NULL, NULL, JSPROP_ENUMERATE);
	    JS_ValueToId(cx, *statep, idp);
if (_debug)
fprintf(stderr, "\tNEXT sys %p[%u] dirent %p \"%s\"\n", sys, ix, dp, dp->d_name);
	    *statep = INT_TO_JSVAL(ix+1);
	} else
	    *idp = JSVAL_VOID;
        if (*idp != JSVAL_VOID)
            break;
        /*@fallthrough@*/
    case JSENUMERATE_DESTROY:
	ix = JSVAL_TO_INT(*statep);
	(void) JS_DefineProperty(cx, obj, "length", INT_TO_JSVAL(ix),
			NULL, NULL, JSPROP_ENUMERATE);
if (_debug)
fprintf(stderr, "\tFINI sys %p[%u]\n", sys, ix);
	*statep = JSVAL_NULL;
        break;
    }
#endif
    return JS_TRUE;
}

/* --- Object ctors/dtors */
static void
rpmsys_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmsysClass, NULL);
    rpmsys sys = ptr;

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);
    sys = ptr = _free(sys);
}

static JSBool
rpmsys_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)%s\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ((cx->fp->flags & JSFRAME_CONSTRUCTING) ? " constructing" : ""));

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	(void) rpmsys_init(cx, obj);
    } else {
	if ((obj = JS_NewObject(cx, &rpmsysClass, NULL, NULL)) == NULL)
	    goto exit;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

/* --- Class initialization */
JSClass rpmsysClass = {
    "Sys", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE,
    rpmsys_addprop,   rpmsys_delprop, rpmsys_getprop, rpmsys_setprop,
    (JSEnumerateOp)rpmsys_enumerate, (JSResolveOp)rpmsys_resolve,
    rpmsys_convert,	rpmsys_dtor,

    rpmsys_getobjectops,	rpmsys_checkaccess,
    rpmsys_call,		rpmsys_construct,
    rpmsys_xdrobject,	rpmsys_hasinstance,
    rpmsys_mark,		rpmsys_reserveslots,
};

JSObject *
rpmjs_InitSysClass(JSContext *cx, JSObject* obj)
{
    JSObject * proto = JS_InitClass(cx, obj, NULL, &rpmsysClass, rpmsys_ctor, 1,
		rpmsys_props, rpmsys_funcs, NULL, NULL);

if (_debug)
fprintf(stderr, "<== %s(%p,%p) proto %p\n", __FUNCTION__, cx, obj, proto);

assert(proto != NULL);
    return proto;
}

JSObject *
rpmjs_NewSysObject(JSContext *cx)
{
    JSObject *obj;
    rpmsys sys;

    if ((obj = JS_NewObject(cx, &rpmsysClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((sys = rpmsys_init(cx, obj)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

/** \ingroup js_c
 * \file js/rpmbf-js.c
 */

#include "system.h"

#include "rpmbf-js.h"
#include "rpmjs-debug.h"

#include <rpmmacro.h>
#include <rpmbf.h>

#include "debug.h"

/*@unchecked@*/
static int _debug = 0;

#define	rpmbf_addprop	JS_PropertyStub
#define	rpmbf_delprop	JS_PropertyStub
#define	rpmbf_convert	JS_ConvertStub

static JSBool
rpmbf_add(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmbfClass, NULL);
    rpmbf bf = ptr;
    JSBool ok = JS_FALSE;
    const char * _s = NULL;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "s", &_s)))
        goto exit;

    (void) rpmbfAdd(bf, _s);
    *rval = JSVAL_TRUE;		/* XXX return s? */
    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmbf_chk(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmbfClass, NULL);
    rpmbf bf = ptr;
    JSBool ok = JS_FALSE;
    const char * _s = NULL;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "s", &_s)))
        goto exit;

    *rval = (rpmbfChk(bf, _s) ? JSVAL_TRUE : JSVAL_FALSE);
    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmbf_clr(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmbfClass, NULL);
    rpmbf bf = ptr;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    *rval = (rpmbfClr(bf) ? JSVAL_FALSE : JSVAL_TRUE);
    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmbf_del(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmbfClass, NULL);
    rpmbf bf = ptr;
    JSBool ok = JS_FALSE;
    const char * _s = NULL;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "s", &_s)))
        goto exit;

    *rval = (rpmbfDel(bf, _s) ? JSVAL_FALSE : JSVAL_TRUE);
    ok = JS_TRUE;
exit:
    return ok;
}

static JSFunctionSpec rpmbf_funcs[] = {
    JS_FS("add",	rpmbf_add,		0,0,0),
    JS_FS("chk",	rpmbf_chk,		0,0,0),
    JS_FS("clr",	rpmbf_clr,		0,0,0),
    JS_FS("del",	rpmbf_del,		0,0,0),
    JS_FS_END
};

/* --- Object properties */
enum rpmbf_tinyid {
    _DEBUG	= -2,
    _LENGTH	= -3,
};

static JSPropertySpec rpmbf_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
    {NULL, 0, 0, NULL, NULL}
};

static JSBool
rpmbf_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmbfClass, NULL);
    rpmbf bf = ptr;
    jsint tiny = JSVAL_TO_INT(id);

#ifdef	NOTYET
_PROP_DEBUG_ENTRY(_debug < 0);
#endif

    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    if (ptr == NULL)
	return JS_TRUE;

    switch (tiny) {
    case _DEBUG:
	*vp = INT_TO_JSVAL(_debug);
	break;
    default:
#ifdef	NOTYET
        if (JSVAL_IS_STRING(id) && *vp == JSVAL_VOID) {
	    const char * name = JS_GetStringBytes(JS_ValueToString(cx, id));
	    const char * _path = rpmGetPath(_defvar, "/", name, NULL);
	    const char * _value = NULL;
	    if (rpmbfGet(bf, _path, &_value) >= 0 && _value != NULL)
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, _value));
	    _path = _free(_path);
	    _value = _free(_value);
            break;
        }
#endif
	break;
    }

    return JS_TRUE;
}

static JSBool
rpmbf_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmbfClass, NULL);
    rpmbf bf = ptr;
    jsint tiny = JSVAL_TO_INT(id);

#ifdef	NOTYET
_PROP_DEBUG_ENTRY(_debug < 0);
#endif

    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    if (ptr == NULL)
	return JS_TRUE;

    switch (tiny) {
    case _DEBUG:
	if (!JS_ValueToInt32(cx, *vp, &_debug))
	    break;
	break;
    default:
#ifdef	NOTYET
	/* XXX expr = undefined same as deleting? */
        if (JSVAL_IS_STRING(id)) {
            const char * name = JS_GetStringBytes(JS_ValueToString(cx, id));
            const char * expr = JS_GetStringBytes(JS_ValueToString(cx, *vp));
	    /* XXX should *setprop be permitted to delete NAME?!? */
	    /* XXX return is no. nodes in EXPR match. */
	    (void) rpmbfDefvar(bf, name, expr);
            break;
        }
#endif
	break;
    }

    return JS_TRUE;
}

static JSBool
rpmbf_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
	JSObject **objp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmbfClass, NULL);
#ifdef	NOTYET
    rpmbf bf = ptr;

_RESOLVE_DEBUG_ENTRY(_debug);
#endif

    if (ptr == NULL) {	/* don't resolve to parent prototypes objects. */
	*objp = NULL;
	goto exit;
    }

#ifdef	NOTYET
    /* Lazily resolve new strings, with duplication to Augeas defvar too. */
    if ((flags & JSRESOLVE_ASSIGNING) && JSVAL_IS_STRING(id)) {
        const char *name = JS_GetStringBytes(JS_ValueToString(cx, id));
	const char * _path;
	const char * _value;
	int xx;
	JSFunctionSpec *fsp;

	/* XXX avoid "bf.print" method namess duped into defvar space? */
	for (fsp = rpmbf_funcs; fsp->name != NULL; fsp++) {
	    if (!strcmp(fsp->name, name)) {
		*objp = obj;	/* XXX always resolve in this object. */
		goto exit;
	    }
	}

	/* See if NAME exists in DEFVAR path. */
	_path = rpmGetPath(_defvar, "/", name, NULL);
	_value = NULL;

	switch (rpmbfGet(bf, _path, &_value)) {
	/* XXX For now, force all defvar's to be single valued strings. */
	case -1:/* multiply defined */
	case 0:	/* not found */
	    /* Create an empty Augeas defvar for NAME */
	    xx = rpmbfDefvar(bf, name, "");
	    /*@fallthrough@*/
	case 1:	/* found */
	    /* Reflect the Augeas defvar NAME into JS Aug properties. */
	    if (JS_DefineProperty(cx, obj, name,
			(_value != NULL
			    ? STRING_TO_JSVAL(JS_NewStringCopyZ(cx, _value))
			    : JSVAL_VOID),
                        NULL, NULL, JSPROP_ENUMERATE))
		break;
	    /*@fallthrough@*/
	default:
assert(0);
	    break;
	}

	_path = _free(_path);
	_value = _free(_value);
    }
#endif
    *objp = obj;	/* XXX always resolve in this object. */

exit:
    return JS_TRUE;
}

static JSBool
rpmbf_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
		  jsval *statep, jsid *idp)
{

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
    return JS_TRUE;
}

/* --- Object ctors/dtors */
static rpmbf
rpmbf_init(JSContext *cx, JSObject *obj, size_t _n, size_t _m, size_t _k,
		unsigned int _flags)
{
    rpmbf bf;

    if ((bf = rpmbfNew(_n, _m, _k, _flags)) == NULL)
	return NULL;
    if (!JS_SetPrivate(cx, obj, bf)) {
	/* XXX error msg */
	(void) rpmbfFree(bf);
	return NULL;
    }
    return bf;
}

static void
rpmbf_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmbfClass, NULL);
    rpmbf bf = ptr;

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);

    bf = rpmbfFree(bf);
}

static JSBool
rpmbf_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)%s\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ((cx->fp->flags & JSFRAME_CONSTRUCTING) ? " constructing" : ""));

#ifdef	NOTYET
    if (!(ok = JS_ConvertArguments(cx, argc, argv, "/ssu", &_root, &_loadpath, &_flags)))
	goto exit;
#endif

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	size_t _n = 0;
	size_t _m = 0;
	size_t _k = 0;
	unsigned int _flags = 0;
	if (rpmbf_init(cx, obj, _n, _m, _k, _flags) == NULL)
	    goto exit;
    } else {
	if ((obj = JS_NewObject(cx, &rpmbfClass, NULL, NULL)) == NULL)
	    goto exit;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

/* --- Class initialization */
JSClass rpmbfClass = {
    /* XXX class should be "Bloom" eventually, avoid name conflicts for now */
    "Bf", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE,
    rpmbf_addprop,   rpmbf_delprop, rpmbf_getprop, rpmbf_setprop,
    (JSEnumerateOp)rpmbf_enumerate, (JSResolveOp)rpmbf_resolve,
    rpmbf_convert,	rpmbf_dtor,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *
rpmjs_InitBfClass(JSContext *cx, JSObject* obj)
{
    JSObject * o;

if (_debug)
fprintf(stderr, "==> %s(%p,%p)\n", __FUNCTION__, cx, obj);

    o = JS_InitClass(cx, obj, NULL, &rpmbfClass, rpmbf_ctor, 1,
		rpmbf_props, rpmbf_funcs, NULL, NULL);
assert(o != NULL);
    return o;
}

JSObject *
rpmjs_NewBfObject(JSContext *cx, size_t _n, size_t _m, size_t _k,
		unsigned int _flags)
{
    JSObject *obj;
    rpmbf bf;

    if ((obj = JS_NewObject(cx, &rpmbfClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((bf = rpmbf_init(cx, obj, _n, _m, _k, _flags)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

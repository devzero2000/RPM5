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
static JSBool
rpmaug_get(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmaugClass, NULL);
    rpmaug aug = ptr;
    JSBool ok = JS_FALSE;
    const char * _path = NULL;
    const char * _value = NULL;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "s", &_path)))
        goto exit;

    switch (rpmaugGet(aug, _path, &_value)) {
    case 1:	/* found */
if (_debug)
fprintf(stderr, "\tgot \"%s\"\n", _value);
	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, _value));
#ifdef	NOTYET
	_value = _free(_value);
#endif
	break;
    default:
    case 0:	/* not found */
    case -1:	/* multiply defined */
	*rval = JSVAL_VOID;
	break;
    }
    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmaug_set(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmaugClass, NULL);
    rpmaug aug = ptr;
    JSBool ok = JS_FALSE;
    const char * _path = NULL;
    const char * _value = NULL;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "ss", &_path, &_value)))
        goto exit;

    switch (rpmaugSet(aug, _path, _value)) {
    case 0:	/* found */
if (_debug)
fprintf(stderr, "\tput \"%s\"\n", _value);
	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, _value));
	break;
    default:
    case -1:	/* multiply defined */
	*rval = JSVAL_VOID;
	break;
    }
    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmaug_insert(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmaugClass, NULL);
    rpmaug aug = ptr;
    JSBool ok = JS_FALSE;
    const char * _path = NULL;
    const char * _label = NULL;
    int _before;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "ssi", &_path, &_label, &_before)))
        goto exit;

    switch (rpmaugInsert(aug, _path, _label, _before)) {
    case 0:	/* success */
	*rval = JSVAL_TRUE;
	break;
    default:
    case -1:	/* failure */
	*rval = JSVAL_FALSE;
	break;
    }
    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmaug_rm(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmaugClass, NULL);
    rpmaug aug = ptr;
    JSBool ok = JS_FALSE;
    const char * _path = NULL;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "s", &_path)))
        goto exit;

    switch (rpmaugRm(aug, _path)) {
    case 0:	/* success */
	*rval = JSVAL_TRUE;
	break;
    default:
    case -1:	/* failure */
	*rval = JSVAL_FALSE;
	break;
    }
    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmaug_mv(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmaugClass, NULL);
    rpmaug aug = ptr;
    JSBool ok = JS_FALSE;
    const char * _src = NULL;
    const char * _dst = NULL;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "ss", &_src, &_dst)))
        goto exit;

    switch (rpmaugMv(aug, _src, _dst)) {
    case 0:	/* success */
	*rval = JSVAL_TRUE;
	break;
    default:
    case -1:	/* failure */
	*rval = JSVAL_FALSE;
	break;
    }
    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmaug_match(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmaugClass, NULL);
    rpmaug aug = ptr;
    JSBool ok = JS_FALSE;
    const char * _path = NULL;
    char ** _matches = NULL;
    int nmatches;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "s", &_path)))
        goto exit;

    nmatches = rpmaugMatch(aug, _path, &_matches);
    if (nmatches <= 0) {	/* not found */
	*rval = JSVAL_VOID;
	goto exit;
    } else {
	jsval * vec = xmalloc(nmatches * sizeof(*vec));
	int i;
	for (i = 0; i < nmatches; i++) {
	    vec[i] = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, _matches[i]));
#ifdef	NOTYET
	    _matches[i] = _free(_matches[i]);
#endif
	}
	*rval = OBJECT_TO_JSVAL(JS_NewArrayObject(cx, nmatches, vec));
#ifdef	NOTYET
	_matches = _free(_matches);
	vec = _free(vec);
#endif
    }
    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmaug_save(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmaugClass, NULL);
    rpmaug aug = ptr;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    switch (rpmaugSave(aug)) {
    case 0:	/* success */
	*rval = JSVAL_TRUE;
	break;
    default:
    case -1:	/* failure */
	*rval = JSVAL_FALSE;
	break;
    }
    ok = JS_TRUE;
    return ok;
}

static JSBool
rpmaug_load(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmaugClass, NULL);
    rpmaug aug = ptr;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    switch (rpmaugLoad(aug)) {
    case 0:	/* success */
	*rval = JSVAL_TRUE;
	break;
    default:
    case -1:	/* failure */
	*rval = JSVAL_FALSE;
	break;
    }
    ok = JS_TRUE;
    return ok;
}

/* XXX print is uselss method because of FILE * in Augeas API. */
static JSBool
rpmaug_print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmaugClass, NULL);
    rpmaug aug = ptr;
    JSBool ok = JS_FALSE;
    const char * _path = NULL;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "s", &_path)))
        goto exit;

    switch (rpmaugPrint(aug, NULL, _path)) {
    case 0:	/* success */
	*rval = JSVAL_TRUE;
	break;
    default:
    case -1:	/* failure */
	*rval = JSVAL_FALSE;
	break;
    }
    ok = JS_TRUE;
exit:
    return ok;
}

static JSFunctionSpec rpmaug_funcs[] = {
    JS_FS("get",	rpmaug_get,		0,0,0),
    JS_FS("set",	rpmaug_set,		0,0,0),
    JS_FS("insert",	rpmaug_insert,		0,0,0),
    JS_FS("rm",		rpmaug_rm,		0,0,0),
    JS_FS("remove",	rpmaug_rm,		0,0,0),
    JS_FS("mv",		rpmaug_mv,		0,0,0),
    JS_FS("move",	rpmaug_mv,		0,0,0),
    JS_FS("match",	rpmaug_match,		0,0,0),
    JS_FS("save",	rpmaug_save,		0,0,0),
    JS_FS("load",	rpmaug_load,		0,0,0),
    JS_FS("print",	rpmaug_print,		0,0,0),
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
#ifdef	NOTYET
    rpmaug aug = ptr;
#endif
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

    return JS_TRUE;
}

/* --- Object ctors/dtors */
static rpmaug
rpmaug_init(JSContext *cx, JSObject *obj, const char *_root,
		const char *_loadpath, unsigned int _flags)
{
    rpmaug aug;

    if ((aug = rpmaugNew(_root, _loadpath, _flags)) == NULL)
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
    const char * _root = _rpmaugRoot;
    const char * _loadpath = _rpmaugLoadpath;
    unsigned int _flags = _rpmaugFlags;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)%s\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ((cx->fp->flags & JSFRAME_CONSTRUCTING) ? " constructing" : ""));

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "/ssu", &_root, &_loadpath, &_flags)))
	goto exit;


    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	if (rpmaug_init(cx, obj, _root, _loadpath, _flags) == NULL)
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
    /* XXX class should be "Augeas" eventually, avoid name conflicts for now */
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
rpmjs_NewAugObject(JSContext *cx, const char * _root,
		const char * _loadpath, unsigned int _flags)
{
    JSObject *obj;
    rpmaug aug;

    if ((obj = JS_NewObject(cx, &rpmaugClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((aug = rpmaug_init(cx, obj, _root, _loadpath, _flags)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

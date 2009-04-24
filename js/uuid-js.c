/** \ingroup js_c
 * \file js/uuid-js.c
 */

#include "system.h"

#if defined(WITH_UUID)
#include <uuid.h>
#include "uuid-js.h"

#include "debug.h"

/*@unchecked@*/
extern int _rpmjs_debug;

/*@unchecked@*/
static int _debug = 1;

typedef struct uuid_s {
    const char * ns_str;
    uuid_t  * ns;
} * JSUuid;

/* --- Object methods */

static JSBool
uuid_generate(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    int32 version = 0;
    const char *uuid_ns_str = NULL;
    const char *data = NULL;
    uuid_t *uuid = NULL;
    uuid_t *uuid_ns = NULL;
    uuid_rc_t rc;
    char buf[UUID_LEN_STR+1];
    char *b = buf;
    size_t blen = sizeof(buf);
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval);

    ok = JS_ConvertArguments(cx, argc, argv, "i/ss",
		&version, &uuid_ns_str, &data);
    if (!ok)
	goto exit;

    switch (version) {
    default:	goto exit;	/*@notreached@*/ break;
    case 1:
    case 4:
	break;
    case 3:
    case 5:
	if (uuid_ns_str == NULL || data == NULL
	 || (rc = uuid_create(&uuid_ns)) != UUID_RC_OK)
	    goto exit;
        if ((rc = uuid_load(uuid_ns, uuid_ns_str)) != UUID_RC_OK
	 && (rc = uuid_import(uuid_ns, UUID_FMT_STR, uuid_ns_str, strlen(uuid_ns_str))) != UUID_RC_OK)
	    goto exit;
	break;
    }

    if ((rc = uuid_create(&uuid)) != UUID_RC_OK)
	goto exit;

    switch (version) {
    default:	goto exit;		/*@notreached@*/break;
    case 1:	rc = uuid_make(uuid, UUID_MAKE_V1);	break;
    case 3:	rc = uuid_make(uuid, UUID_MAKE_V3, uuid_ns, data);	break;
    case 4:	rc = uuid_make(uuid, UUID_MAKE_V4);	break;
    case 5:	rc = uuid_make(uuid, UUID_MAKE_V5, uuid_ns, data);	break;
    }
    if (rc != UUID_RC_OK)
	goto exit;

    if ((rc = uuid_export(uuid, UUID_FMT_STR, &b, &blen)) == UUID_RC_OK) {
	JSString *str;
	if ((str = JS_NewStringCopyZ(cx, b)) != NULL) {
	    *rval = STRING_TO_JSVAL(str);
	    ok = JS_TRUE;
	}
    }

exit:
    if (uuid != NULL)
	uuid_destroy(uuid);
    if (uuid_ns != NULL)
	uuid_destroy(uuid_ns);
    return ok;
}

static JSBool
uuid_describe(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uuid_t *uuid = NULL;
    const char *uuid_str = NULL;
    uuid_rc_t rc;
    char *b = NULL;
    size_t blen = 0;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval);

    ok = JS_ConvertArguments(cx, argc, argv, "s", &uuid_str);
    if (!ok)
	goto exit;

    if ((rc = uuid_create(&uuid)) == UUID_RC_OK
     && (rc = uuid_import(uuid, UUID_FMT_STR, uuid_str, strlen(uuid_str))) == UUID_RC_OK
     && (rc = uuid_export(uuid, UUID_FMT_TXT, &b, &blen)) == UUID_RC_OK)
    {	JSString *str;
	if ((str = JS_NewStringCopyZ(cx, b)) != NULL) {
	    *rval = STRING_TO_JSVAL(str);
	    ok = JS_TRUE;
	}
    }

exit:
    b = _free(b);
    if (uuid != NULL)
	uuid_destroy(uuid);
    return ok;
}

static JSFunctionSpec uuid_funcs[] = {
    {"generate",	uuid_generate,		0,0,0},
    {"describe",	uuid_describe,		0,0,0},
    {NULL,		NULL,			0,0,0}
};

/* --- Object properties */
enum uuid_tinyid {
    _DEBUG		= -2,
};

static JSPropertySpec uuid_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
    {NULL, 0, 0, NULL, NULL}
};

static JSBool
uuid_addprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &uuidClass, NULL);

if (_debug)
fprintf(stderr, "==> %s(%p,%p,0x%lx[%u],%p) ptr %p %s = %s\n", __FUNCTION__, cx, obj, (unsigned long)id, (unsigned)JSVAL_TAG(id), vp, ptr, JS_GetStringBytes(JS_ValueToString(cx, id)), JS_GetStringBytes(JS_ValueToString(cx, *vp)));

    return JS_TRUE;
}

static JSBool
uuid_delprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &uuidClass, NULL);

if (_debug)
fprintf(stderr, "==> %s(%p,%p,0x%lx[%u],%p) ptr %p %s = %s\n", __FUNCTION__, cx, obj, (unsigned long)id, (unsigned)JSVAL_TAG(id), vp, ptr, JS_GetStringBytes(JS_ValueToString(cx, id)), JS_GetStringBytes(JS_ValueToString(cx, *vp)));

    return JS_TRUE;
}

static JSBool
uuid_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &uuidClass, NULL);
    jsint tiny = JSVAL_TO_INT(id);
    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    JSBool ok = (ptr ? JS_FALSE : JS_TRUE);

    if (JSVAL_IS_STRING(id)) {
	char * str = JS_GetStringBytes(JSVAL_TO_STRING(id));
	const JSFunctionSpec *fsp;
	for (fsp = uuid_funcs; fsp->name != NULL; fsp++) {
	    if (strcmp(fsp->name, str))
		continue;
	    ok = JS_TRUE;
	    break;
	}
	goto exit;
    }

    switch (tiny) {
    case _DEBUG:
	*vp = INT_TO_JSVAL(_debug);
	ok = JS_TRUE;
	break;
    default:
	break;
    }
exit:
    if (!ok) {
if (_debug) {
fprintf(stderr, "==> %s(%p,%p,0x%lx[%u],%p) ptr %p %s = %s\n", __FUNCTION__, cx, obj, (unsigned long)id, (unsigned)JSVAL_TAG(id), vp, ptr, JS_GetStringBytes(JS_ValueToString(cx, id)), JS_GetStringBytes(JS_ValueToString(cx, *vp)));
ok = JS_TRUE;		/* XXX return JS_TRUE iff ... ? */
}
    }
    return ok;
}

static JSBool
uuid_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &uuidClass, NULL);
    jsint tiny = JSVAL_TO_INT(id);
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
if (_debug) {
fprintf(stderr, "==> %s(%p,%p,0x%lx[%u],%p) ptr %p %s = %s\n", __FUNCTION__, cx, obj, (unsigned long)id, (unsigned)JSVAL_TAG(id), vp, ptr, JS_GetStringBytes(JS_ValueToString(cx, id)), JS_GetStringBytes(JS_ValueToString(cx, *vp)));
ok = JS_TRUE;		/* XXX return JS_TRUE iff ... ? */
}
    }
    return ok;
}

static JSBool
uuid_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
		  jsval *statep, jsid *idp)
{
    JSObject *iterator;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%d,%p,%p)\n", __FUNCTION__, cx, obj, op, statep, idp);

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
    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
uuid_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
	JSObject **objp)
{
if (_debug)
fprintf(stderr, "==> %s(%p,%p,0x%lx[%u],0x%x,%p) property %s flags 0x%x{%s,%s,%s,%s,%s}\n", __FUNCTION__, cx, obj, (unsigned long)id, (unsigned)JSVAL_TAG(id), (unsigned)flags, objp,
		JS_GetStringBytes(JS_ValueToString(cx, id)), flags,
		(flags & JSRESOLVE_QUALIFIED) ? "qualified" : "",
		(flags & JSRESOLVE_ASSIGNING) ? "assigning" : "",
		(flags & JSRESOLVE_DETECTING) ? "detecting" : "",
		(flags & JSRESOLVE_DETECTING) ? "declaring" : "",
		(flags & JSRESOLVE_DETECTING) ? "classname" : "");
    return JS_TRUE;
}

static JSBool
uuid_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
if (_debug)
fprintf(stderr, "==> %s(%p,%p,%d,%p) convert to %s\n", __FUNCTION__, cx, obj, type, vp, JS_GetTypeName(cx, type));
    return JS_TRUE;
}

/* --- Object ctors/dtors */
static void
uuid_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &uuidClass, NULL);

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);

    ptr = _free(ptr);
}

static JSBool
uuid_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval);

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	JSUuid ptr = xcalloc(0, sizeof(*ptr));

	if (ptr == NULL || !JS_SetPrivate(cx, obj, ptr)) {
	    ptr = _free(ptr);
	    goto exit;
	}
    } else {
	if ((obj = JS_NewObject(cx, &uuidClass, NULL, NULL)) == NULL)
	    goto exit;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

/* --- Class initialization */
JSClass uuidClass = {
    "Uuid", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE | JSCLASS_HAS_CACHED_PROTO(JSProto_Object),
    uuid_addprop,   uuid_delprop, uuid_getprop, uuid_setprop,
    (JSEnumerateOp)uuid_enumerate, (JSResolveOp)uuid_resolve,
    uuid_convert,	uuid_dtor,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *
rpmjs_InitUuidClass(JSContext *cx, JSObject* obj)
{
    JSObject * o;

if (_debug)
fprintf(stderr, "==> %s(%p,%p)\n", __FUNCTION__, cx, obj);

    o = JS_InitClass(cx, obj, NULL, &uuidClass, uuid_ctor, 1,
		uuid_props, uuid_funcs, NULL, NULL);
assert(o != NULL);
    return o;
}
#endif	/* WITH_UUID */

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
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval);

    ok = JS_ConvertArguments(cx, argc, argv, "s", &uuid_str);
    if (!ok)
	goto exit;

    if ((rc = uuid_create(&uuid)) == UUID_RC_OK
     && (rc = uuid_import(uuid, UUID_FMT_STR, uuid_str, strlen(uuid_str))) == UUID_RC_OK
     && (rc = uuid_export(uuid, UUID_FMT_TXT, &b, NULL)) == UUID_RC_OK)
    {	JSString *str;
	if ((str = JS_NewStringCopyZ(cx, b)) != NULL) {
	    *rval = STRING_TO_JSVAL(str);
	    ok = JS_TRUE;
	}
    }

exit:
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
uuid_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &uuidClass, NULL);
    jsint tiny = JSVAL_TO_INT(id);
    JSBool ok = (ptr ? JS_FALSE : JS_TRUE);

if (_debug)
fprintf(stderr, "==> %s(%p,%p,0x%llx,%p) %s = %s\n", __FUNCTION__, cx, obj, (unsigned long long)id, vp, JS_GetStringBytes(JS_ValueToString(cx, id)), JS_GetStringBytes(JS_ValueToString(cx, *vp)));

    /* XXX return JS_TRUE if private == NULL */

    switch (tiny) {
    case _DEBUG:
	*vp = INT_TO_JSVAL(_debug);
	ok = JS_TRUE;
	break;
    default:
	break;
    }
    return ok;
}

static JSBool
uuid_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &uuidClass, NULL);
    jsint tiny = JSVAL_TO_INT(id);
    JSBool ok = (ptr ? JS_FALSE : JS_TRUE);

if (_debug)
fprintf(stderr, "==> %s(%p,%p,0x%llx,%p) %s = %s\n", __FUNCTION__, cx, obj, (unsigned long long)id, vp, JS_GetStringBytes(JS_ValueToString(cx, id)), JS_GetStringBytes(JS_ValueToString(cx, *vp)));

    /* XXX return JS_TRUE if ... ? */

    switch (tiny) {
    case _DEBUG:
	if (JS_ValueToInt32(cx, *vp, &_debug))
	    ok = JS_TRUE;
	break;
    default:
	break;
    }
    return ok;
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
    "Uuid", JSCLASS_HAS_PRIVATE | JSCLASS_HAS_CACHED_PROTO(JSProto_Object),
    JS_PropertyStub,  JS_PropertyStub,  uuid_getprop,   uuid_setprop,
    JS_EnumerateStub, JS_ResolveStub,	JS_ConvertStub,	uuid_dtor
};

JSObject *
js_InitUuidClass(JSContext *cx, JSObject* obj)
{
    JSObject * o;
    o = JS_InitClass(cx, obj, NULL, &uuidClass, uuid_ctor, 1,
        uuid_props, uuid_funcs, NULL, NULL);
assert(o != NULL);
    return o;
}
#endif	/* WITH_UUID */

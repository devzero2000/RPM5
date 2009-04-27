/** \ingroup js_c
 * \file js/rpmhdr-js.c
 */

#include "system.h"

#include "rpmhdr-js.h"
#include "rpmjs-debug.h"

#include "debug.h"

/*@unchecked@*/
static int _debug = 0;

/* --- helpers */
static JSObject *
rpmhdrLoadTag(JSContext *cx, JSObject *obj, Header h, const char * name, jsval *vp)
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    JSString * valstr;
    JSObject * arr;
    JSObject * retobj = NULL;
    jsval * vec = NULL;
    int i;

    if (!strcmp(name, "toString")
     || !strcmp(name, "valueOf")
     || !strcmp(name, "__iterator__")
     || !strcmp(name, "toJSON"))
	goto exit;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p,%s)\n", __FUNCTION__, cx, obj, h, name);

    he->tag = tagValue(name);
    if (headerGet(h, he, 0)) {
if (_debug < 0)
fprintf(stderr, "\t%s(%u) %u %p[%u]\n", name, (unsigned)he->tag, (unsigned)he->t, he->p.ptr, (unsigned)he->c);
	switch (he->t) {
	case RPM_BIN_TYPE:		/* XXX FIXME */
	case RPM_I18NSTRING_TYPE:	/* XXX FIXME */
	default:
	    goto exit;
	    /*@notreached@*/ break;
	case RPM_UINT8_TYPE:
	    vec = xmalloc(he->c * sizeof(he->c));
	    for (i = 0; i < (int)he->c; i++)
		vec[i] = INT_TO_JSVAL(he->p.ui8p[i]);
	    arr = JS_NewArrayObject(cx, he->c, vec);
	    if (!JS_DefineProperty(cx, obj, name, OBJECT_TO_JSVAL(arr),
				NULL, NULL, JSPROP_ENUMERATE))
		goto exit;
	    retobj = obj;
	    if (vp) *vp = OBJECT_TO_JSVAL(arr);
	    break;
	case RPM_UINT16_TYPE:
	    vec = xmalloc(he->c * sizeof(he->c));
	    for (i = 0; i < (int)he->c; i++)
		vec[i] = INT_TO_JSVAL(he->p.ui16p[i]);
	    arr = JS_NewArrayObject(cx, he->c, vec);
	    if (!JS_DefineProperty(cx, obj, name, OBJECT_TO_JSVAL(arr),
				NULL, NULL, JSPROP_ENUMERATE))
		goto exit;
	    retobj = obj;
	    if (vp) *vp = OBJECT_TO_JSVAL(arr);
	    break;
	case RPM_UINT32_TYPE:
	    vec = xmalloc(he->c * sizeof(he->c));
	    for (i = 0; i < (int)he->c; i++)
		vec[i] = INT_TO_JSVAL(he->p.ui32p[i]);
	    arr = JS_NewArrayObject(cx, he->c, vec);
	    if (!JS_DefineProperty(cx, obj, name, OBJECT_TO_JSVAL(arr),
				NULL, NULL, JSPROP_ENUMERATE))
		goto exit;
	    retobj = obj;
	    if (vp) *vp = OBJECT_TO_JSVAL(arr);
	    break;
	case RPM_UINT64_TYPE:
	    vec = xmalloc(he->c * sizeof(he->c));
	    for (i = 0; i < (int)he->c; i++)
		vec[i] = INT_TO_JSVAL(he->p.ui64p[i]);
	    arr = JS_NewArrayObject(cx, he->c, vec);
	    if (!JS_DefineProperty(cx, obj, name, OBJECT_TO_JSVAL(arr),
				NULL, NULL, JSPROP_ENUMERATE))
		goto exit;
	    retobj = obj;
	    if (vp) *vp = OBJECT_TO_JSVAL(arr);
	    break;
	case RPM_STRING_ARRAY_TYPE:
	    vec = xmalloc(he->c * sizeof(he->c));
	    for (i = 0; i < (int)he->c; i++)
		vec[i] = STRING_TO_JSVAL(he->p.argv[i]);
	    arr = JS_NewArrayObject(cx, he->c, vec);
	    if (!JS_DefineProperty(cx, obj, name, OBJECT_TO_JSVAL(arr),
				NULL, NULL, JSPROP_ENUMERATE))
		goto exit;
	    retobj = obj;
	    if (vp) *vp = OBJECT_TO_JSVAL(arr);
	    break;
	case RPM_STRING_TYPE:
	    if ((valstr = JS_NewStringCopyZ(cx, he->p.str)) == NULL) {
if (_debug < 0)
fprintf(stderr, "\tvalstr %p\n", valstr);
		goto exit;
	    }
	    if (!JS_DefineProperty(cx, obj, name, STRING_TO_JSVAL(valstr),
				NULL, NULL, JSPROP_ENUMERATE)) {
if (_debug < 0)
fprintf(stderr, "\tJS_DefineProperty: %s = %p\n", name, valstr);
		goto exit;
	    }
	    retobj = obj;
	    if (vp) *vp = STRING_TO_JSVAL(valstr);
	    break;
	}
    }

exit:
    vec = _free(vec);
if (_debug < 0)
fprintf(stderr, "\tretobj %p vp %p *vp 0x%lx(%u)\n", retobj, vp, (unsigned long)(vp ? *vp : 0), (unsigned)(vp ? JSVAL_TAG(*vp) : 0));
    return retobj;
}

/* --- Object methods */
static JSFunctionSpec rpmhdr_funcs[] = {
    JS_FS_END
};

/* --- Object properties */
enum rpmhdr_tinyid {
    _DEBUG	= -2,
};

static JSPropertySpec rpmhdr_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
    {NULL, 0, 0, NULL, NULL}
};

static JSBool
rpmhdr_addprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmhdrClass, NULL);
_PROP_DEBUG_ENTRY(_debug < 0);
    return JS_TRUE;
}

static JSBool
rpmhdr_delprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmhdrClass, NULL);
_PROP_DEBUG_ENTRY(_debug < 0);
    return JS_TRUE;
}
static JSBool
rpmhdr_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmhdrClass, NULL);
    Header h = ptr;
    jsint tiny = JSVAL_TO_INT(id);
    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    JSBool ok = (ptr ? JS_FALSE : JS_TRUE);

    switch (tiny) {
    case _DEBUG:
	*vp = INT_TO_JSVAL(_debug);
	ok = JS_TRUE;
	break;
    default:
      {	JSString * str = JS_ValueToString(cx, id);
	JSObject * retobj = rpmhdrLoadTag(cx, obj, h, JS_GetStringBytes(str), vp);
	if (retobj != NULL) {
            ok = JS_TRUE;
            break;
        }
      } break;
    }

    if (!ok) {
_PROP_DEBUG_EXIT(_debug);
    }
    return ok;
}

static JSBool
rpmhdr_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmhdrClass, NULL);
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
rpmhdr_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
	JSObject **objp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmhdrClass, NULL);
    Header h = ptr;
    JSString *idstr;
    char * name;
    JSObject * retobj = NULL;
    JSBool ok = JS_FALSE;

_RESOLVE_DEBUG_ENTRY(_debug);

   if (flags & JSRESOLVE_ASSIGNING) {
	ok = JS_TRUE;
	goto exit;
    }

    if ((idstr = JS_ValueToString(cx, id)) == NULL)
	goto exit;

    name = JS_GetStringBytes(idstr);
    if (!strcmp(name, "toString")
     || !strcmp(name, "valueOf")
     || !strcmp(name, "__iterator__")
     || !strcmp(name, "toJSON"))
	goto exit;

    if ((retobj = rpmhdrLoadTag(cx, obj, h, name, NULL)) == NULL)
	goto exit;
    *objp = retobj;
    ok = JS_TRUE;

exit:
    return ok;
}

static JSBool
rpmhdr_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
		  jsval *statep, jsid *idp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmhdrClass, NULL);
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
rpmhdr_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmhdrClass, NULL);
_CONVERT_DEBUG_ENTRY(_debug);
    return JS_TRUE;
}

/* --- Object ctors/dtors */
static Header
rpmhdr_init(JSContext *cx, JSObject *obj, void * _h)
{
    Header h = (_h ? _h : headerNew());

    if (h == NULL)
	return NULL;
    if (!JS_SetPrivate(cx, obj, (void *)h)) {
	/* XXX error msg */
	h = headerFree(h);
	return NULL;
    }
    return h;
}

static void
rpmhdr_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmhdrClass, NULL);

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);

    (void) headerFree((Header)ptr);
}

static JSBool
rpmhdr_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_FALSE;
    JSObject *tso = NULL;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "o", &tso)))
	goto exit;

fprintf(stderr, "\ttso %p ptr %p\n", tso, JS_GetPrivate(cx, tso));

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	if (rpmhdr_init(cx, obj, NULL))
	    goto exit;
    } else {
	if ((obj = JS_NewObject(cx, &rpmhdrClass, NULL, NULL)) == NULL)
	    goto exit;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

/* --- Class initialization */
#ifdef	HACKERY
JSClass rpmhdrClass = {
    "Hdr", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE,
    rpmhdr_addprop,   rpmhdr_delprop, rpmhdr_getprop, rpmhdr_setprop,
    (JSEnumerateOp)rpmhdr_enumerate, (JSResolveOp)rpmhdr_resolve,
    rpmhdr_convert,	rpmhdr_dtor,
    JSCLASS_NO_OPTIONAL_MEMBERS
};
#else
JSClass rpmhdrClass = {
    "Hdr", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,   JS_PropertyStub, rpmhdr_getprop, JS_PropertyStub,
    (JSEnumerateOp)JS_EnumerateStub, (JSResolveOp)JS_ResolveStub,
    JS_ConvertStub,	rpmhdr_dtor,
    JSCLASS_NO_OPTIONAL_MEMBERS
};
#endif

JSObject *
rpmjs_InitHdrClass(JSContext *cx, JSObject* obj)
{
    JSObject * o;

if (_debug)
fprintf(stderr, "==> %s(%p,%p)\n", __FUNCTION__, cx, obj);

    o = JS_InitClass(cx, obj, NULL, &rpmhdrClass, rpmhdr_ctor, 1,
		rpmhdr_props, rpmhdr_funcs, NULL, NULL);
assert(o != NULL);
    return o;
}

JSObject *
rpmjs_NewHdrObject(JSContext *cx, void * _h)
{
    JSObject *obj;
    Header h;

if (_debug)
fprintf(stderr, "==> %s(%p,%p)\n", __FUNCTION__, cx, _h);

    if ((obj = JS_NewObject(cx, &rpmhdrClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((h = rpmhdr_init(cx, obj, _h)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

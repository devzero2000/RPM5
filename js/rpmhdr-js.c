/** \ingroup js_c
 * \file js/rpmhdr-js.c
 */

#include "system.h"

#include "rpmhdr-js.h"
#include "rpmds-js.h"
#include "rpmfi-js.h"
#include "rpmjs-debug.h"

#include <rpmcli.h>	/* XXX rpmHeaderFormats */

#include "debug.h"

/*@unchecked@*/
static int _debug = 0;

/* --- helpers */
static JSObject *
rpmhdrLoadTag(JSContext *cx, JSObject *obj, Header h, rpmTag tag, jsval *vp)
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    JSString * valstr;
    JSObject * arr;
    JSObject * retobj = NULL;
    jsval * vec = NULL;
const char * name = tagName(tag);
    int i;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p,%s)\n", __FUNCTION__, cx, obj, h, name);

    he->tag = tag;
    if (headerGet(h, he, 0)) {
if (_debug < 0)
fprintf(stderr, "\t%s(%u) %u %p[%u]\n", name, (unsigned)he->tag, (unsigned)he->t, he->p.ptr, (unsigned)he->c);
	switch (he->t) {
	default:
	    goto exit;
	    /*@notreached@*/ break;
	case RPM_BIN_TYPE:	/* XXX return as array of octets for now. */
	case RPM_UINT8_TYPE:
	    vec = xmalloc(he->c * sizeof(*vec));
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
	    vec = xmalloc(he->c * sizeof(*vec));
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
	    vec = xmalloc(he->c * sizeof(*vec));
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
	    vec = xmalloc(he->c * sizeof(*vec));
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
	    vec = xmalloc(he->c * sizeof(*vec));
	    for (i = 0; i < (int)he->c; i++) {
		if ((valstr = JS_NewStringCopyZ(cx, he->p.argv[i])) == NULL)
		    goto exit;
		vec[i] = STRING_TO_JSVAL(valstr);
	    }
	    arr = JS_NewArrayObject(cx, he->c, vec);
	    if (!JS_DefineProperty(cx, obj, name, OBJECT_TO_JSVAL(arr),
				NULL, NULL, JSPROP_ENUMERATE))
		goto exit;
	    retobj = obj;
	    if (vp) *vp = OBJECT_TO_JSVAL(arr);
	    break;
	case RPM_I18NSTRING_TYPE:	/* XXX FIXME: is this ever seen? */
fprintf(stderr, "==> FIXME: %s(%d) t %d %p[%u]\n", tagName(he->tag), he->tag, he->t, he->p.ptr, he->c);
#ifdef	DYING
	    if ((valstr = JS_NewStringCopyZ(cx, "FIXME")) == NULL
	     || !JS_DefineProperty(cx, obj, name, STRING_TO_JSVAL(valstr),
				NULL, NULL, JSPROP_ENUMERATE))
		goto exit;
	    retobj = obj;
	    if (vp) *vp = STRING_TO_JSVAL(valstr);
	    break;
#else
	    /*@fallthrough@*/
#endif
	
	case RPM_STRING_TYPE:
	    if ((valstr = JS_NewStringCopyZ(cx, he->p.str)) == NULL
	     || !JS_DefineProperty(cx, obj, name, STRING_TO_JSVAL(valstr),
				NULL, NULL, JSPROP_ENUMERATE))
		goto exit;
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
static JSBool
rpmhdr_ds(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmhdrClass, NULL);
    rpmTag tagN = RPMTAG_NAME;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "/i", &tagN)))
        goto exit;
    *rval = OBJECT_TO_JSVAL(rpmjs_NewDsObject(cx, obj, tagN));
    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmhdr_fi(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmhdrClass, NULL);
    Header h = ptr;
    rpmTag tagN = RPMTAG_BASENAMES;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "/i", &tagN)))
        goto exit;
    *rval = OBJECT_TO_JSVAL(rpmjs_NewFiObject(cx, h, tagN));
    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmhdr_sprintf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmhdrClass, NULL);
    Header h = ptr;
    char * qfmt = NULL;
    const char * s = NULL;
    const char * errstr = NULL;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "s", &qfmt)))
        goto exit;

    if ((s = headerSprintf(h, qfmt, NULL, rpmHeaderFormats, &errstr)) == NULL)
	s = errstr; 	/* XXX FIXME: returning errstr in-band. */
    *rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, s));
    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmhdr_getorigin(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmhdrClass, NULL);
    Header h = ptr;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    *rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, headerGetOrigin(h)));
    ok = JS_TRUE;
    return ok;
}

static JSBool
rpmhdr_setorigin(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmhdrClass, NULL);
    Header h = ptr;
    char * s = NULL;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "s", &s)))
        goto exit;

    (void) headerSetOrigin(h, s);
    *rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, headerGetOrigin(h)));
    ok = JS_TRUE;
exit:
    return ok;
}

static JSFunctionSpec rpmhdr_funcs[] = {
    {"ds",		rpmhdr_ds,		0,0,0},
    {"fi",		rpmhdr_fi,		0,0,0},
    {"sprintf",		rpmhdr_sprintf,		0,0,0},
    {"getorigin",	rpmhdr_getorigin,	0,0,0},
    {"setorigin",	rpmhdr_setorigin,	0,0,0},
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

_PROP_DEBUG_ENTRY(_debug < 0);
    switch (tiny) {
    case _DEBUG:
	*vp = INT_TO_JSVAL(_debug);
	ok = JS_TRUE;
	break;
    default: {
	rpmTag tag = JSVAL_IS_INT(id)
		? (rpmTag) JSVAL_TO_INT(id)
		: tagValue(JS_GetStringBytes(JS_ValueToString(cx, id)));
	if (rpmhdrLoadTag(cx, obj, h, tag, vp) != NULL)
            ok = JS_TRUE;
      } break;
    }

    if (!ok) {
_PROP_DEBUG_EXIT(_debug);
    }
ok = JS_TRUE;  /* XXX avoid immediate interp exit by always succeeding. */
    return ok;
}

static JSBool
rpmhdr_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmhdrClass, NULL);
    jsint tiny = JSVAL_TO_INT(id);
    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    JSBool ok = (ptr ? JS_FALSE : JS_TRUE);

_PROP_DEBUG_ENTRY(_debug < 0);
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
    JSBool ok = JS_FALSE;

_RESOLVE_DEBUG_ENTRY(_debug);

    if ((flags & JSRESOLVE_ASSIGNING)
     || (h == NULL)) {	/* don't resolve to parent prototypes objects. */
	*objp = NULL;
	ok = JS_TRUE;
	goto exit;
    }

    if (JSVAL_IS_INT(id) || JSVAL_IS_STRING(id)) {
	rpmTag tag = JSVAL_IS_INT(id)
		? (rpmTag) JSVAL_TO_INT(id)
		: tagValue(JS_GetStringBytes(JS_ValueToString(cx, id)));
	JSObject * arr = rpmhdrLoadTag(cx, obj, h, tag, NULL);

	if (!JS_DefineElement(cx, obj, tag, OBJECT_TO_JSVAL(arr),
			NULL, NULL, JSPROP_ENUMERATE)) {
	    *objp = NULL;
	    goto exit;
	}
	*objp = obj;
    } else
	*objp = NULL;
    ok = JS_TRUE;
exit:
    return ok;
}

static void
rpmhi_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmhiClass, NULL);
    HeaderIterator hi = ptr;

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);

    hi = headerFini(hi);
}

JSClass rpmhiClass = {
    "Hi",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,  JS_ConvertStub,  rpmhi_dtor,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool
rpmhdr_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
		  jsval *statep, jsid *idp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmhdrClass, NULL);
    Header h = ptr;
    HeaderIterator hi = NULL;
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    JSObject *ho = NULL;
    JSBool ok = JS_FALSE;

_ENUMERATE_DEBUG_ENTRY(_debug);

    switch (op) {
    case JSENUMERATE_INIT:
	if ((ho = js_NewObject(cx, &rpmhiClass, NULL, obj, 0)) == NULL)
	    goto exit;
	if ((hi = headerInit(h)) == NULL)
	    goto exit;
	if (!JS_SetPrivate(cx, ho, (void *)hi)) {
	    hi = headerFini(hi);
	    goto exit;
	}
	*statep = OBJECT_TO_JSVAL(ho);
	if (idp)
	    *idp = JSVAL_ZERO;
if (_debug)
fprintf(stderr, "\tINIT ho %p hi %p\n", ho, hi);
	break;
    case JSENUMERATE_NEXT:
	ho = (JSObject *) JSVAL_TO_OBJECT(*statep);
	hi = JS_GetInstancePrivate(cx, ho, &rpmhiClass, NULL);
if (_debug)
fprintf(stderr, "\tNEXT ho %p hi %p\n", ho, hi);
	if (headerNext(hi, he, 0)) {
	    JS_ValueToId(cx, INT_TO_JSVAL(he->tag), idp);
	    he->p.ptr = _free(he->p.ptr);
	} else
	    *idp = JSVAL_VOID;
	if (*idp != JSVAL_VOID)
	    break;
	/*@fallthrough@*/
    case JSENUMERATE_DESTROY:
	ho = (JSObject *) JSVAL_TO_OBJECT(*statep);
	hi = JS_GetInstancePrivate(cx, ho, &rpmhiClass, NULL);
if (_debug)
fprintf(stderr, "\tFINI ho %p hi %p\n", ho, hi);
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

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "/o", &tso)))
	goto exit;

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
JSClass rpmhdrClass = {
    "Hdr", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE,
    rpmhdr_addprop, rpmhdr_delprop, rpmhdr_getprop, rpmhdr_setprop,
    (JSEnumerateOp)rpmhdr_enumerate, (JSResolveOp)rpmhdr_resolve,
    rpmhdr_convert,	rpmhdr_dtor,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

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

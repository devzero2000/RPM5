/** \ingroup js_c
 * \file js/rpmmi-js.c
 */

#include "system.h"

#include "rpmts-js.h"
#include "rpmmi-js.h"
#include "rpmhdr-js.h"
#include "rpmjs-debug.h"

#include <rpmdb.h>
#include <rpmts.h>

#include "debug.h"

/*@unchecked@*/
static int _debug = 0;

#define	rpmmi_addprop	JS_PropertyStub
#define	rpmmi_delprop	JS_PropertyStub
#define	rpmmi_convert	JS_ConvertStub

/* --- helpers */

/* --- Object methods */
static JSBool
rpmmi_pattern(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmiClass, NULL);
    rpmdbMatchIterator mi = ptr;
    int tag = RPMTAG_NAME;
    rpmMireMode type = RPMMIRE_REGEX;
    char * pattern = NULL;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "is", &tag, &pattern)))
	goto exit;

    rpmdbSetIteratorRE(mi, tag, type, pattern);

    ok = JS_TRUE;
    *rval = BOOLEAN_TO_JSVAL(ok);

exit:
    return ok;
}

static JSFunctionSpec rpmmi_funcs[] = {
    JS_FS("pattern",	rpmmi_pattern,		0,0,0),
    JS_FS_END
};

/* --- Object properties */
enum rpmmi_tinyid {
    _DEBUG	= -2,
    _LENGTH	= -3,
    _COUNT	= -4,	/* XXX is _LENGTH enuf? */
    _INSTANCE	= -5,
};

static JSPropertySpec rpmmi_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"length",	_LENGTH,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"count",	_COUNT,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"instance",_INSTANCE,	JSPROP_ENUMERATE,	NULL,	NULL},
    {NULL, 0, 0, NULL, NULL}
};

static JSBool
rpmmi_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmiClass, NULL);
    rpmdbMatchIterator mi = ptr;
    jsint tiny = JSVAL_TO_INT(id);
    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    JSBool ok = (ptr ? JS_FALSE : JS_TRUE);

_PROP_DEBUG_ENTRY(_debug < 0);
    switch (tiny) {
    case _DEBUG:
	*vp = INT_TO_JSVAL(_debug);
	ok = JS_TRUE;
	break;
    case _LENGTH:
    case _COUNT:	/* XXX is _LENGTH enuf? */
	*vp = INT_TO_JSVAL(rpmdbGetIteratorCount(mi));
	ok = JS_TRUE;
        break;
    case _INSTANCE:
	*vp = INT_TO_JSVAL(rpmdbGetIteratorOffset(mi));
	ok = JS_TRUE;
        break;
    default:
      {	JSObject *o = (JSVAL_IS_OBJECT(id) ? JSVAL_TO_OBJECT(id) : NULL);
	Header h = JS_GetInstancePrivate(cx, o, &rpmhdrClass, NULL);
	rpmuint32_t ix = headerGetInstance(h);
	if (ix != 0) {
	    *vp = id;
	    ok = JS_TRUE;
if (_debug)
fprintf(stderr, "\tGET  %p[%d] h %p\n", mi, ix, h);
	}
      }	break;
    }

    if (!ok) {
_PROP_DEBUG_EXIT(_debug);
    }
ok = JS_TRUE;	/* XXX avoid immediate interp exit by always succeeding. */
    return ok;
}

static JSBool
rpmmi_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmiClass, NULL);
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
rpmmi_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
	JSObject **objp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmiClass, NULL);
    rpmdbMatchIterator mi = ptr;
    JSObject *o = (JSVAL_IS_OBJECT(id) ? JSVAL_TO_OBJECT(id) : NULL);
    JSClass *c = (o ? OBJ_GET_CLASS(cx, o) : NULL);
    JSBool ok = JS_FALSE;

_RESOLVE_DEBUG_ENTRY(_debug);

    if ((flags & JSRESOLVE_ASSIGNING)
     || (mi == NULL)) {	/* don't resolve to parent prototypes objects. */
	*objp = NULL;
	ok = JS_TRUE;
	goto exit;
    }

    if (c == &rpmhdrClass) {
	Header h = JS_GetInstancePrivate(cx, o, &rpmhdrClass, NULL);
	rpmuint32_t ix = headerGetInstance(h);
	if (ix == 0
	 || !JS_DefineElement(cx, obj, ix, id, NULL, NULL, JSPROP_ENUMERATE))
	{
	    *objp = NULL;
	    ok = JS_TRUE;
            goto exit;
	}
if (_debug)
fprintf(stderr, "\tRESOLVE %p[%d] h %p\n", mi, ix, h);
	*objp = obj;
    } else
	*objp = NULL;

    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmmi_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
		  jsval *statep, jsid *idp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmiClass, NULL);
    rpmdbMatchIterator mi = ptr;
    Header h;
    JSBool ok = JS_FALSE;

_ENUMERATE_DEBUG_ENTRY(_debug);

    switch (op) {
    case JSENUMERATE_INIT:
	*statep = JSVAL_VOID;
	if (idp)
	    *idp = JSVAL_ZERO;
if (_debug)
fprintf(stderr, "\tINIT mi %p\n", mi);
	break;
    case JSENUMERATE_NEXT:
	*statep = JSVAL_VOID;		/* XXX needed? */
	if ((h = rpmdbNextIterator(mi)) != NULL) {
            JS_ValueToId(cx, OBJECT_TO_JSVAL(rpmjs_NewHdrObject(cx, h)), idp);
if (_debug)
fprintf(stderr, "\tNEXT mi %p h %p\n", mi, h);
	} else
	    *idp = JSVAL_VOID;
	if (*idp != JSVAL_VOID)
	    break;
	/*@fallthrough@*/
    case JSENUMERATE_DESTROY:
if (_debug)
fprintf(stderr, "\tFINI mi %p\n", mi);
	/* XXX Allow our iterator object to be GC'd. */
	*statep = JSVAL_NULL;
	break;
    }
    ok = JS_TRUE;
    return ok;
}

/* --- Object ctors/dtors */
static rpmdbMatchIterator
rpmmi_init(JSContext *cx, JSObject *obj, rpmts ts, int _tag, void * _key, int _keylen)
{
    rpmdbMatchIterator mi;

    if ((mi = rpmtsInitIterator(ts, _tag, _key, _keylen)) == NULL)
	return NULL;
    if (!JS_SetPrivate(cx, obj, (void *)mi)) {
	/* XXX error msg */
	mi = rpmdbFreeIterator(mi);
	return NULL;
    }
    return mi;
}

static void
rpmmi_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmmiClass, NULL);

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);

#ifdef	BUGGY
    {	rpmdbMatchIterator mi = ptr;
	mi = rpmdbFreeIterator(mi);
    }
#endif
}

static JSBool
rpmmi_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSObject *tso = NULL;
    jsval tagid = JSVAL_VOID;
    rpmTag tag = RPMDBI_PACKAGES;
    char * key = NULL;
    int keylen = 0;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "o/vs", &tso, &tagid, &key)))
	goto exit;

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	rpmts ts = JS_GetInstancePrivate(cx, tso, &rpmtsClass, NULL);

	if (!JSVAL_IS_VOID(tagid)) {
	    /* XXX TODO: handle key object as non-string. */
	    /* XXX TODO: make sure both tag and key were specified. */
	    tag = JSVAL_IS_INT(tagid)
		? (rpmTag) JSVAL_TO_INT(tagid)
		: tagValue(JS_GetStringBytes(JS_ValueToString(cx, tagid)));
	}

	if (ts == NULL || rpmmi_init(cx, obj, ts, tag, key, keylen))
	    goto exit;		/* XXX error msg */
    } else {
	if ((obj = JS_NewObject(cx, &rpmmiClass, NULL, NULL)) == NULL)
	    goto exit;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

/* --- Class initialization */
JSClass rpmmiClass = {
    "Mi", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE,
    rpmmi_addprop,   rpmmi_delprop, rpmmi_getprop, rpmmi_setprop,
    (JSEnumerateOp)rpmmi_enumerate, (JSResolveOp)rpmmi_resolve,
    rpmmi_convert,	rpmmi_dtor,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *
rpmjs_InitMiClass(JSContext *cx, JSObject* obj)
{
    JSObject * o;

if (_debug)
fprintf(stderr, "==> %s(%p,%p)\n", __FUNCTION__, cx, obj);

    o = JS_InitClass(cx, obj, NULL, &rpmmiClass, rpmmi_ctor, 1,
		rpmmi_props, rpmmi_funcs, NULL, NULL);
assert(o != NULL);
    return o;
}

JSObject *
rpmjs_NewMiObject(JSContext *cx, void * _ts, int _tag, void *_key, int _keylen)
{
    JSObject *obj;
    rpmdbMatchIterator mi;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%s(%u),%p[%u]) _key %s\n", __FUNCTION__, cx, _ts, tagName(_tag), (unsigned)_tag, _key, (unsigned)_keylen, (const char *)(_key ? _key : ""));

    if ((obj = JS_NewObject(cx, &rpmmiClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((mi = rpmmi_init(cx, obj, _ts, _tag, _key, _keylen)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

/** \ingroup js_c
 * \file js/rpmfts-js.c
 */

#include "system.h"

#include "rpmfts-js.h"
#include "rpmjs-debug.h"

#include <fts.h>

#include "debug.h"

/*@unchecked@*/
static int _debug = 0;

/* Required JSClass vectors */
#define	rpmfts_addprop		JS_PropertyStub
#define	rpmfts_delprop		JS_PropertyStub
#define	rpmfts_convert		JS_ConvertStub

/* Optional JSClass vectors */
#define	rpmfts_getobjectops	NULL
#define	rpmfts_checkaccess	NULL
#define	rpmfts_call		rpmfts_call
#define	rpmfts_construct	rpmfts_ctor
#define	rpmfts_xdrobject	NULL
#define	rpmfts_hasinstance	NULL
#define	rpmfts_mark		NULL
#define	rpmfts_reserveslots	NULL

/* Extended JSClass vectors */
#define rpmfts_equality		NULL
#define rpmfts_outerobject	NULL
#define rpmfts_innerobject	NULL
#define rpmfts_iteratorobject	NULL
#define rpmfts_wrappedobject	NULL

/* --- helpers */

/* --- Object methods */

static JSFunctionSpec rpmfts_funcs[] = {
    JS_FS_END
};

/* --- Object properties */
enum rpmfts_tinyid {
    _DEBUG	= -2,
};

static JSPropertySpec rpmfts_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
    {NULL, 0, 0, NULL, NULL}
};

static JSBool
rpmfts_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmftsClass, NULL);
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
rpmfts_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmftsClass, NULL);
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
rpmfts_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
		JSObject **objp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmftsClass, NULL);

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
rpmfts_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
		  jsval *statep, jsid *idp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmftsClass, NULL);
    FTS * fts = ptr;
    FTSENT * p;
    unsigned int ix = 0;

_ENUMERATE_DEBUG_ENTRY(_debug < 0);

    switch (op) {
    case JSENUMERATE_INIT:
        if (idp)
            *idp = JSVAL_ZERO;
	*statep = INT_TO_JSVAL(ix);
if (_debug)
fprintf(stderr, "\tINIT fts %p\n", fts);
        break;
    case JSENUMERATE_NEXT:
	ix = JSVAL_TO_INT(*statep);
	if ((p = Fts_read(fts)) != NULL) {
	    (void) JS_DefineElement(cx, obj,
			ix, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, p->fts_name)),
			NULL, NULL, JSPROP_ENUMERATE);
	    JS_ValueToId(cx, *statep, idp);
if (_debug)
fprintf(stderr, "\tNEXT fts %p[%u] ftsent %p \"%s\"\n", fts, ix, p, p->fts_name);
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
fprintf(stderr, "\tFINI fts %p[%u]\n", fts, ix);
	*statep = JSVAL_NULL;
        break;
    }
    return JS_TRUE;
}

/* --- Object ctors/dtors */
static FTS *
rpmfts_init(JSContext *cx, JSObject *obj, const char * _dn)
{
    FTS * fts = NULL;

    if (_dn) {
	static int ftsoptions = FTS_NOSTAT;
	const char *paths[2];
	
	paths[0] = _dn;
	paths[1] = NULL;
	fts = Fts_open(paths, ftsoptions, NULL);
	/* XXX error msg */
	if (!JS_SetPrivate(cx, obj, (void *)fts)) {
	    /* XXX error msg */
	    if (fts) {
		(void) Fts_close(fts);
		/* XXX error msg */
	    }
	    fts = NULL;
	}
    }

if (_debug)
fprintf(stderr, "==> %s(%p,%p,\"%s\") fts %p\n", __FUNCTION__, cx, obj, _dn, fts);

    return fts;
}

static void
rpmfts_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmftsClass, NULL);
    FTS * fts = ptr;

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);
    if (fts) {
	(void) Fts_close(fts);
	/* XXX error msg */
    }
}

static JSBool
rpmfts_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_FALSE;
    const char * _dn = NULL;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)%s\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ((cx->fp->flags & JSFRAME_CONSTRUCTING) ? " constructing" : ""));

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "/s", &_dn)))
        goto exit;

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	(void) rpmfts_init(cx, obj, _dn);
    } else {
	if ((obj = JS_NewObject(cx, &rpmftsClass, NULL, NULL)) == NULL)
	    goto exit;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

static JSBool
rpmfts_call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmftsClass, NULL);
    FTS * fts = ptr;
    JSBool ok = JS_FALSE;
    const char * _dn = NULL;

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "/s", &_dn)))
        goto exit;

    if (fts) {
	(void) Fts_close(fts);
	/* XXX error msg */
	fts = ptr = NULL;
    }

    fts = ptr = rpmfts_init(cx, obj, _dn);

    *rval = OBJECT_TO_JSVAL(obj);

    ok = JS_TRUE;

exit:
if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ptr);

    return ok;
}

/* --- Class initialization */
JSClass rpmftsClass = {
    "Fts", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE,
    rpmfts_addprop,   rpmfts_delprop, rpmfts_getprop, rpmfts_setprop,
    (JSEnumerateOp)rpmfts_enumerate, (JSResolveOp)rpmfts_resolve,
    rpmfts_convert,	rpmfts_dtor,

    rpmfts_getobjectops,rpmfts_checkaccess,
    rpmfts_call,	rpmfts_construct,
    rpmfts_xdrobject,	rpmfts_hasinstance,
    rpmfts_mark,	rpmfts_reserveslots,
};

JSObject *
rpmjs_InitFtsClass(JSContext *cx, JSObject* obj)
{
    JSObject * proto = JS_InitClass(cx, obj, NULL, &rpmftsClass, rpmfts_ctor, 1,
		rpmfts_props, rpmfts_funcs, NULL, NULL);

if (_debug)
fprintf(stderr, "==> %s(%p,%p) proto %p\n", __FUNCTION__, cx, obj, proto);

assert(proto != NULL);
    return proto;
}

JSObject *
rpmjs_NewFtsObject(JSContext *cx, const char * _dn)
{
    JSObject *obj;
    FTS * fts;

    if ((obj = JS_NewObject(cx, &rpmftsClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((fts = rpmfts_init(cx, obj, _dn)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

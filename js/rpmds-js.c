/** \ingroup js_c
 * \file js/rpmds-js.c
 */

#include "system.h"

#include "rpmds-js.h"
#include "rpmhdr-js.h"

#include <argv.h>
#include <mire.h>

#include <rpmdb.h>

#include <rpmds.h>

#include "debug.h"

/*@unchecked@*/
extern int _rpmjs_debug;

/*@unchecked@*/
static int _debug = 1;

/* --- Object methods */

static JSFunctionSpec rpmds_funcs[] = {
    JS_FS_END
};

/* --- Object properties */
enum rpmds_tinyid {
    _DEBUG	= -2,
};

static JSPropertySpec rpmds_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
    {NULL, 0, 0, NULL, NULL}
};

static JSBool
rpmds_addprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdsClass, NULL);

if (_debug < 0)
fprintf(stderr, "==> %s(%p,%p,0x%lx[%u],%p) ptr %p %s = %s\n", __FUNCTION__, cx, obj, (unsigned long)id, (unsigned)JSVAL_TAG(id), vp, ptr, JS_GetStringBytes(JS_ValueToString(cx, id)), JS_GetStringBytes(JS_ValueToString(cx, *vp)));

    return JS_TRUE;
}

static JSBool
rpmds_delprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdsClass, NULL);

if (_debug)
fprintf(stderr, "==> %s(%p,%p,0x%lx[%u],%p) ptr %p %s = %s\n", __FUNCTION__, cx, obj, (unsigned long)id, (unsigned)JSVAL_TAG(id), vp, ptr, JS_GetStringBytes(JS_ValueToString(cx, id)), JS_GetStringBytes(JS_ValueToString(cx, *vp)));

    return JS_TRUE;
}
static JSBool
rpmds_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdsClass, NULL);
    rpmds ds = ptr;
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
rpmds_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdsClass, NULL);
    rpmds ds = (rpmds)ptr;
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
rpmds_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
	JSObject **objp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdsClass, NULL);
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
rpmds_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
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
rpmds_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdsClass, NULL);
if (_debug)
fprintf(stderr, "==> %s(%p,%p,%d,%p) ptr %p convert to %s\n", __FUNCTION__, cx, obj, type, vp, ptr, JS_GetTypeName(cx, type));
    return JS_TRUE;
}

/* --- Object ctors/dtors */
#define	OBJ_IS_STRING(_cx, _o)	(OBJ_GET_CLASS(_cx, _o) == &js_StringClass)
#define	OBJ_IS_HEADER(_cx, _o)	(OBJ_GET_CLASS(_cx, _o) == &rpmhdrClass)

static rpmds
rpmds_init(JSContext *cx, JSObject *obj, JSObject *o, int _tagN)
{
    rpmds ds = NULL;
    int xx;

    if (OBJ_IS_HEADER(cx, o)) {
	Header h = JS_GetPrivate(cx, o);
	int flags = 0;
	if (_tagN == RPMTAG_NAME) {
	    flags = RPMSENSE_EQUAL;
	    _tagN = RPMTAG_PROVIDENAME;
	    ds = rpmdsThis(h, _tagN, flags);
if (_debug)
fprintf(stderr, "\trpmdsThis(%p, %s(%d), 0x%x) ds %p\n", h, tagName(_tagN), _tagN, flags, ds);
	} else {
	    ds = rpmdsNew(h, _tagN, flags);
if (_debug)
fprintf(stderr, "\trpmdsNew(%p, %s(%d), 0x%x) ds %p\n", h, tagName(_tagN), _tagN, flags, ds);
	}
	if (ds == NULL)
	    return NULL;
    } else
    if (OBJ_IS_STRING(cx, o)) {
	const char * s =
		JS_GetStringBytes(JS_ValueToString(cx, OBJECT_TO_JSVAL(o)));
	if (!strcmp(s, "cpuinfo")) {
	    xx = rpmdsCpuinfo(&ds, NULL);
if (_debug)
fprintf(stderr, "\trpmdsCpuinfo() ret %d ds %p\n", xx, ds);
	} else
	if (!strcmp(s, "rpmlib")) {
	    xx = rpmdsRpmlib(&ds, NULL);
if (_debug)
fprintf(stderr, "\trpmdsRpmlib() ret %d ds %p\n", xx, ds);
	} else
	if (!strcmp(s, "getconf")) {
	    xx = rpmdsGetconf(&ds, NULL);
if (_debug)
fprintf(stderr, "\trpmdsGetconf() ret %d ds %p\n", xx, ds);
	} else
	if (!strcmp(s, "uname")) {
	    xx = rpmdsUname(&ds, NULL);
if (_debug)
fprintf(stderr, "\trpmdsUname() ret %d ds %p\n", xx, ds);
	} else {
if (_debug)
fprintf(stderr, "\tstring \"%s\" is unknown. ds %p\n", s, ds);
	    return NULL;
	}
    } else
    if (OBJ_IS_ARRAY(cx, o)) {
	jsuint length = 0;
	jsuint i;
	JSBool ok = JS_GetArrayLength(cx, o, &length);
	const char * N = NULL;
	const char * EVR = NULL;
	uint32_t F = 0;

	if (!ok)
	    return NULL;
	if (length != 3)
	    return NULL;
	for (i = 0; i < length; i++) {
	    jsval v;
	    if (!(ok = JS_GetElement(cx, o, (jsint)i, &v)))
		return NULL;
	    switch (i) {
	    default:
		return NULL;
		/*@notreached@*/ break;
	    case 0:
		N = JS_GetStringBytes(JSVAL_TO_STRING(v));
		break;
	    case 1:
		EVR = JS_GetStringBytes(JSVAL_TO_STRING(v));
		break;
	    case 2:
		F = JSVAL_TO_INT(v);
		break;
	    }
	}
	ds = rpmdsSingle(_tagN, N, EVR, F);
if (_debug)
fprintf(stderr, "\trpmdsSingle(%s(%d), %s, %s, 0x%x) ds %p\n", tagName(_tagN), _tagN, N, EVR, F, ds);
	return NULL;
    } else {
if (_debug)
fprintf(stderr, "\tobject class %p is unknown. ds %p\n", OBJ_GET_CLASS(cx, o), ds);
	return NULL;
    }
    if (!JS_SetPrivate(cx, obj, (void *)ds)) {
	/* XXX error msg */
	(void) rpmdsFree(ds);
	return NULL;
    }
    return ds;
}

static void
rpmds_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdsClass, NULL);
    rpmds ds = ptr;

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);

    (void) rpmdsFree(ds);
}

static JSBool
rpmds_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_FALSE;
    JSObject *o = NULL;
    int tagN = RPMTAG_REQUIRENAME;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)%s\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ((cx->fp->flags & JSFRAME_CONSTRUCTING) ? " constructing" : ""));

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "o/i", &o, &tagN)))
	goto exit;

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	if (rpmds_init(cx, obj, o, tagN) == NULL)
	    goto exit;
    } else {
	if ((obj = JS_NewObject(cx, &rpmdsClass, NULL, NULL)) == NULL)
	    goto exit;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

/* --- Class initialization */
#ifdef	HACKERY
JSClass rpmdsClass = {
    "Ds", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE | JSCLASS_HAS_CACHED_PROTO(JSProto_Object),
    rpmds_addprop,   rpmds_delprop, rpmds_getprop, rpmds_setprop,
    (JSEnumerateOp)rpmds_enumerate, (JSResolveOp)rpmds_resolve,
    rpmds_convert,	rpmds_dtor,
    JSCLASS_NO_OPTIONAL_MEMBERS
};
#else
JSClass rpmdsClass = {
    "Ds", JSCLASS_NEW_RESOLVE | JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,   JS_PropertyStub, rpmds_getprop, JS_PropertyStub,
    (JSEnumerateOp)rpmds_enumerate, (JSResolveOp)rpmds_resolve,
    JS_ConvertStub,	rpmds_dtor,
    JSCLASS_NO_OPTIONAL_MEMBERS
};
#endif

JSObject *
rpmjs_InitDsClass(JSContext *cx, JSObject* obj)
{
    JSObject * o;

if (_debug)
fprintf(stderr, "==> %s(%p,%p)\n", __FUNCTION__, cx, obj);

    o = JS_InitClass(cx, obj, NULL, &rpmdsClass, rpmds_ctor, 1,
		rpmds_props, rpmds_funcs, NULL, NULL);
assert(o != NULL);
    return o;
}

JSObject *
rpmjs_NewDsObject(JSContext *cx, JSObject *o, int _tagN)
{
    JSObject *obj;
    rpmds ds;

    if ((obj = JS_NewObject(cx, &rpmdsClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((ds = rpmds_init(cx, obj, o, _tagN)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

/** \ingroup js_c
 * \file js/rpmds-js.c
 */

#include "system.h"

#include "rpmds-js.h"
#include "rpmhdr-js.h"

#include <argv.h>
#include <mire.h>

#include <rpmdb.h>

#define	_RPMDS_INTERNAL
#include <rpmds.h>

#include "debug.h"

/*@unchecked@*/
extern int _rpmjs_debug;

/*@unchecked@*/
static int _debug = 1;

static const char * v2s(JSContext *cx, jsval v)
{
    if (JSVAL_IS_NULL(v))	return "null";
    if (JSVAL_IS_VOID(v))	return "void";
    if (JSVAL_IS_INT(v))	return "integer";
    if (JSVAL_IS_DOUBLE(v))	return "double";
    if (JSVAL_IS_STRING(v))	return "string";
    if (JSVAL_IS_BOOLEAN(v))	return "boolean";
    if (JSVAL_IS_OBJECT(v)) {
	return OBJ_GET_CLASS(cx, JSVAL_TO_OBJECT(v))->name;
    }
    return "other";
}

/* --- Object methods */

static JSBool
rpmds_next(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject * obj = JS_THIS_OBJECT(cx, vp);
    rpmds ds = (rpmds)JS_GetInstancePrivate(cx, obj, &rpmdsClass, NULL);
    JSBool ok = (rpmdsNext(ds) >= 0) ? JS_TRUE : JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%u,%p) ds %p[%d:%d] %s\n", __FUNCTION__, cx, argc, vp, ds, ds->i, ds->Count, JS_GetStringBytes(JS_ValueToString(cx, *vp)));

    if (!ok)
	goto exit;

    {   JSString *valstr;
	const char *N = rpmdsN(ds);
	const char *EVR = rpmdsEVR(ds);
	unsigned Flags = rpmdsFlags(ds);

	if ((valstr = JS_NewStringCopyZ(cx, N)) == NULL
	 || !JS_DefineProperty(cx, obj, "N", STRING_TO_JSVAL(valstr),
				NULL, NULL, JSPROP_ENUMERATE))
	    goto exit;
 	if ((valstr = JS_NewStringCopyZ(cx, EVR)) == NULL
	 || !JS_DefineProperty(cx, obj, "EVR", STRING_TO_JSVAL(valstr),
				NULL, NULL, JSPROP_ENUMERATE))
	    goto exit;
 	if (!JS_DefineProperty(cx, obj, "F", INT_TO_JSVAL(Flags),
				NULL, NULL, JSPROP_ENUMERATE))
	    goto exit;
    }

exit:
    return ok;
}

static JSBool
rpmds_self(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject * obj = JS_THIS_OBJECT(cx, vp);
    rpmds ds = (rpmds) JS_GetInstancePrivate(cx, obj, &rpmdsClass, NULL);

    ds = rpmdsInit(ds);

    *vp = JS_THIS(cx, vp);

if (_debug)
fprintf(stderr, "==> %s(%p,%u,%p) ds %p[%d:%d] %s\n", __FUNCTION__, cx, argc, vp, ds, ds->i, ds->Count, JS_GetStringBytes(JS_ValueToString(cx, *vp)));

    return !JSVAL_IS_NULL(*vp);
}

static JSFunctionSpec rpmds_funcs[] = {
#ifdef	NOTYET
#define	JSPROP_ROPERM	(JSPROP_READONLY | JSPROP_PERMANENT)
    JS_FN(js_iterator_str,	rpmds_self,	0,0,JSPROP_ROPERM),
    JS_FN(js_next_str,		rpmds_next,	0,0,JSPROP_ROPERM),
#endif
    JS_FS_END
};

/* --- Object properties */
enum rpmds_tinyid {
    _DEBUG	= -2,
    _LENGTH	= -3,
    _TYPE	= -4,
    _IX		= -5,
    _BUILDTIME	= -6,
    _COLOR	= -7,
    _NOPROMOTE	= -8,
    _N		= -9,
    _EVR	= -10,
    _F		= -11,
    _DNEVR	= -12,
};

static JSPropertySpec rpmds_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"length",	_LENGTH,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"type",	_TYPE,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"ix",	_IX,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"buildtime",_BUILDTIME,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"color",	_COLOR,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"nopromote",_NOPROMOTE,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"N",	_N,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"EVR",	_EVR,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"F",	_F,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"DNEVR",	_DNEVR,		JSPROP_ENUMERATE,	NULL,	NULL},
    {NULL, 0, 0, NULL, NULL}
};

static JSBool
rpmds_addprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdsClass, NULL);

if (_debug < 0)
fprintf(stderr, "==> %s(%p,%p,0x%lx[%s],%p) ptr %p %s = %s\n", __FUNCTION__, cx, obj, (unsigned long)id, v2s(cx, id), vp, ptr, JS_GetStringBytes(JS_ValueToString(cx, id)), JS_GetStringBytes(JS_ValueToString(cx, *vp)));

    return JS_TRUE;
}

static JSBool
rpmds_delprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdsClass, NULL);

if (_debug < 0)
fprintf(stderr, "==> %s(%p,%p,0x%lx[%s],%p) ptr %p %s = %s\n", __FUNCTION__, cx, obj, (unsigned long)id, v2s(cx, id), vp, ptr, JS_GetStringBytes(JS_ValueToString(cx, id)), JS_GetStringBytes(JS_ValueToString(cx, *vp)));

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
    case _LENGTH:
	*vp = INT_TO_JSVAL(rpmdsCount(ds));
	ok = JS_TRUE;
	break;
    case _TYPE:
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, rpmdsType(ds)));
        ok = JS_TRUE;
        break;
    case _IX:
	*vp = INT_TO_JSVAL(rpmdsIx(ds));
        ok = JS_TRUE;
        break;
    case _BUILDTIME:
	*vp = INT_TO_JSVAL(rpmdsBT(ds));
        ok = JS_TRUE;
        break;
    case _COLOR:
	*vp = INT_TO_JSVAL(rpmdsColor(ds));
        ok = JS_TRUE;
        break;
    case _NOPROMOTE:
	*vp = INT_TO_JSVAL(rpmdsNoPromote(ds));
        ok = JS_TRUE;
        break;
    case _N:
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, rpmdsN(ds)));
        ok = JS_TRUE;
        break;
    case _EVR:
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, rpmdsEVR(ds)));
        ok = JS_TRUE;
        break;
    case _F:
	*vp = INT_TO_JSVAL(rpmdsFlags(ds));
        ok = JS_TRUE;
        break;
    case _DNEVR:
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, rpmdsDNEVR(ds)));
        ok = JS_TRUE;
        break;
    default:
	break;
    }

    if (!ok) {
if (_debug) {
fprintf(stderr, "==> %s(%p,%p,0x%lx[%s],%p) ptr %p %s = %s\n", __FUNCTION__, cx, obj, (unsigned long)id, v2s(cx, id), vp, ptr, JS_GetStringBytes(JS_ValueToString(cx, id)), JS_GetStringBytes(JS_ValueToString(cx, *vp)));
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
    case _IX:
	if (!JS_ValueToInt32(cx, *vp, &myint))
	    break;
	if (myint != ds->i) {
	    (void) rpmdsSetIx(ds, myint-1);
	    /* XXX flush and recreate N and DNEVR with s rpmdsNext() step */
	    (void) rpmdsNext(ds);
	}
	ok = JS_TRUE;
        break;
    case _BUILDTIME:
	if (!JS_ValueToInt32(cx, *vp, &myint))
	    break;
	(void) rpmdsSetBT(ds, myint);
	ok = JS_TRUE;
        break;
    case _NOPROMOTE:
	if (!JS_ValueToInt32(cx, *vp, &myint))
	    break;
	(void) rpmdsSetNoPromote(ds, myint);
	ok = JS_TRUE;
        break;
    default:
	break;
    }

    if (!ok) {
if (_debug) {
fprintf(stderr, "==> %s(%p,%p,0x%lx[%s],%p) ptr %p %s = %s\n", __FUNCTION__, cx, obj, (unsigned long)id, v2s(cx, id), vp, ptr, JS_GetStringBytes(JS_ValueToString(cx, id)), JS_GetStringBytes(JS_ValueToString(cx, *vp)));
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
    rpmds ds = ptr;
    JSBool ok = JS_FALSE;

if (_debug < 0)
fprintf(stderr, "==> %s(%p,%p,0x%lx[%s],0x%x,%p) ptr %p property %s flags 0x%x{%s,%s,%s,%s,%s}\n", __FUNCTION__, cx, obj, (unsigned long)id, v2s(cx, id), (unsigned)flags, objp, ptr,
		JS_GetStringBytes(JS_ValueToString(cx, id)), flags,
		(flags & JSRESOLVE_QUALIFIED) ? "qualified" : "",
		(flags & JSRESOLVE_ASSIGNING) ? "assigning" : "",
		(flags & JSRESOLVE_DETECTING) ? "detecting" : "",
		(flags & JSRESOLVE_DECLARING) ? "declaring" : "",
		(flags & JSRESOLVE_CLASSNAME) ? "classname" : "");

#ifdef	NOTYET
    if (flags & JSRESOLVE_ASSIGNING) {
	ok = JS_TRUE;
	goto exit;
    }

    if (JSVAL_IS_OBJECT(id) && OBJ_IS_ARRAY(cx, JSVAL_TO_OBJECT(id))) {
	ok = JS_DefineProperty(cx, obj, "tuple", id,
			NULL, NULL, JSPROP_ENUMERATE);
fprintf(stderr, "\tok %d %s = [%s]\n", ok, "tuple", JS_GetStringBytes(JS_ValueToString(cx, id)));
	ok = JS_TRUE;
	goto exit;
    }

    {   JSString *valstr;
	const char *N = rpmdsN(ds);
	const char *EVR = rpmdsEVR(ds);
	unsigned Flags = rpmdsFlags(ds);

	if ((valstr = JS_NewStringCopyZ(cx, N)) == NULL
	 || !JS_DefineProperty(cx, obj, "N", STRING_TO_JSVAL(valstr),
				NULL, NULL, JSPROP_ENUMERATE))
	    goto exit;
 	if ((valstr = JS_NewStringCopyZ(cx, EVR)) == NULL
	 || !JS_DefineProperty(cx, obj, "EVR", STRING_TO_JSVAL(valstr),
				NULL, NULL, JSPROP_ENUMERATE))
	    goto exit;
 	if (!JS_DefineProperty(cx, obj, "F", INT_TO_JSVAL(Flags),
				NULL, NULL, JSPROP_ENUMERATE))
	    goto exit;
    }
#endif

    ok = JS_TRUE;
exit:
    return ok;
}
static JSBool
rpmds_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
		  jsval *statep, jsid *idp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdsClass, NULL);
    rpmds ds = (rpmds)ptr;
    JSObject *iterobj;
    JSBool ok = JS_FALSE;
    int ix;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%d,%p,%p) *statep 0x%lx *idp 0x%lx\n", __FUNCTION__, cx, obj, op, statep, idp, (unsigned long)(statep ? *statep : 0xfeedface), (unsigned long)(idp ? *idp : 0xdeadbeef));

    switch (op) {
    case JSENUMERATE_INIT:
	*statep = JSVAL_VOID;
	(void) rpmdsInit(ds);
        if (idp)
            *idp = JSVAL_ZERO;
        break;
    case JSENUMERATE_NEXT:
	*statep = JSVAL_VOID;
	if ((ix = rpmdsNext(ds)) >= 0) {
	    JSObject * arr = JS_NewArrayObject(cx, 3, NULL);
	    JSString *valstr;
	    jsval v;
	    const char *N = rpmdsN(ds);
	    const char *EVR = rpmdsEVR(ds);
	    unsigned Flags = rpmdsFlags(ds);

	    if ((valstr = JS_NewStringCopyZ(cx, N)) == NULL)
		goto exit;
	    v = STRING_TO_JSVAL(valstr);
	    JS_SetElement(cx, arr, 0, &v);
 	    if ((valstr = JS_NewStringCopyZ(cx, EVR)) == NULL)
		goto exit;
	    v = STRING_TO_JSVAL(valstr);
	    JS_SetElement(cx, arr, 1, &v);
	    v = INT_TO_JSVAL(Flags);
	    JS_SetElement(cx, arr, 2, &v);
	    v = OBJECT_TO_JSVAL(arr);
	    JS_ValueToId(cx, v, idp);
	} else
	    *idp = JSVAL_VOID;
        if (*idp != JSVAL_VOID)
            break;
        /*@fallthrough@*/
    case JSENUMERATE_DESTROY:
	*statep = JSVAL_NULL;
        break;
    }
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
    "Ds", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE,
    rpmds_addprop,   rpmds_delprop, rpmds_getprop, rpmds_setprop,
    (JSEnumerateOp)rpmds_enumerate, (JSResolveOp)rpmds_resolve,
    rpmds_convert,	rpmds_dtor,
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

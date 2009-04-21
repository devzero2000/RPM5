/** \ingroup js_c
 * \file js/rpmts-js.c
 */

#include "system.h"

#include "rpmts-js.h"

#define	_RPMTS_INTERNAL
#include <rpmts.h>

#include "debug.h"

/*@unchecked@*/
extern int _rpmjs_debug;

/*@unchecked@*/
static int _debug = 1;

/* --- Object methods */
static JSFunctionSpec rpmts_funcs[] = {
    JS_FS_END
};

/* --- Object properties */
enum rpmts_tinyid {
    _DEBUG	= -2,
    _VSFLAGS	= -3,
    _TYPE	= -4,
    _ARBGOAL	= -5,
    _ROOTDIR	= -6,
    _CURRDIR	= -7,
    _SELINUX	= -8,
    _CHROOTDONE	= -9,
    _TID	= -10,
    _NELEMENTS	= -11,
    _PROBFILTER	= -12,
    _FLAGS	= -13,
    _DFLAGS	= -14,
    _GOAL	= -15,
    _DBMODE	= -16,
    _COLOR	= -17,
    _PREFCOLOR	= -18,
};

static JSPropertySpec rpmts_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"vsflags",	_VSFLAGS,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"type",	_TYPE,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"arbgoal",	_ARBGOAL,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"rootdir",	_ROOTDIR,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"currdir",	_CURRDIR,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"selinux",	_SELINUX,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"chrootdone",_CHROOTDONE,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"tid",	_TID,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"nelements",_NELEMENTS,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"probfilter",_PROBFILTER,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"flags",	_FLAGS,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"dflags",	_DFLAGS,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"goal",	_GOAL,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"dbmode",	_DBMODE,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"color",	_COLOR,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"prefcolor",_PREFCOLOR,	JSPROP_ENUMERATE,	NULL,	NULL},
    {NULL, 0, 0, NULL, NULL}
};

static JSBool
rpmts_addprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmtsClass, NULL);

if (_debug)
fprintf(stderr, "==> %s(%p,%p,0x%lx[%u],%p) ptr %p %s = %s\n", __FUNCTION__, cx, obj, (unsigned long)id, (unsigned)JSVAL_TAG(id), vp, ptr, JS_GetStringBytes(JS_ValueToString(cx, id)), JS_GetStringBytes(JS_ValueToString(cx, *vp)));

    return JS_TRUE;
}

static JSBool
rpmts_delprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmtsClass, NULL);

if (_debug)
fprintf(stderr, "==> %s(%p,%p,0x%lx[%u],%p) ptr %p %s = %s\n", __FUNCTION__, cx, obj, (unsigned long)id, (unsigned)JSVAL_TAG(id), vp, ptr, JS_GetStringBytes(JS_ValueToString(cx, id)), JS_GetStringBytes(JS_ValueToString(cx, *vp)));

    return JS_TRUE;
}
static JSBool
rpmts_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmtsClass, NULL);
    rpmts ts = ptr;
    jsint tiny = JSVAL_TO_INT(id);
    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    JSBool ok = (ptr ? JS_FALSE : JS_TRUE);

    switch (tiny) {
    case _DEBUG:
	*vp = INT_TO_JSVAL(_debug);
	ok = JS_TRUE;
	break;
    case _VSFLAGS:
	*vp = INT_TO_JSVAL((jsint)rpmtsVSFlags(ts));
	ok = JS_TRUE;
	break;
    case _TYPE:
	*vp = INT_TO_JSVAL((jsint)rpmtsType(ts));
	ok = JS_TRUE;
	break;
    case _ARBGOAL:
	*vp = INT_TO_JSVAL((jsint)rpmtsARBGoal(ts));
	ok = JS_TRUE;
	break;
    case _ROOTDIR:
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, rpmtsRootDir(ts)));
	ok = JS_TRUE;
	break;
    case _CURRDIR:
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, rpmtsCurrDir(ts)));
	ok = JS_TRUE;
	break;
    case _SELINUX:
	*vp = INT_TO_JSVAL((jsint)rpmtsSELinuxEnabled(ts));
	ok = JS_TRUE;
	break;
    case _CHROOTDONE:
	*vp = INT_TO_JSVAL((jsint)rpmtsChrootDone(ts));
	ok = JS_TRUE;
	break;
    case _TID:
	*vp = INT_TO_JSVAL((jsint)rpmtsGetTid(ts));
	ok = JS_TRUE;
	break;
    case _NELEMENTS:
	*vp = INT_TO_JSVAL((jsint)rpmtsNElements(ts));
	ok = JS_TRUE;
	break;
    case _PROBFILTER:
	*vp = INT_TO_JSVAL((jsint)rpmtsFilterFlags(ts));
	ok = JS_TRUE;
	break;
    case _FLAGS:
	*vp = INT_TO_JSVAL((jsint)rpmtsFlags(ts));
	ok = JS_TRUE;
	break;
    case _DFLAGS:
	*vp = INT_TO_JSVAL((jsint)rpmtsDFlags(ts));
	ok = JS_TRUE;
	break;
    case _GOAL:
	*vp = INT_TO_JSVAL((jsint)rpmtsGoal(ts));
	ok = JS_TRUE;
	break;
    case _DBMODE:
	*vp = INT_TO_JSVAL((jsint)rpmtsDBMode(ts));
	ok = JS_TRUE;
	break;
    case _COLOR:
	*vp = INT_TO_JSVAL((jsint)rpmtsColor(ts));
	ok = JS_TRUE;
	break;
    case _PREFCOLOR:
	*vp = INT_TO_JSVAL((jsint)rpmtsPrefColor(ts));
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
rpmts_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmtsClass, NULL);
    rpmts ts = (rpmts)ptr;
    jsint tiny = JSVAL_TO_INT(id);
    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    JSBool ok = (ptr ? JS_FALSE : JS_TRUE);
    int myint;

    switch (tiny) {
    case _DEBUG:
	if (JS_ValueToInt32(cx, *vp, &_debug))
	    ok = JS_TRUE;
	break;
    case _VSFLAGS:
	if (!JS_ValueToInt32(cx, *vp, &myint))
	    break;
	(void) rpmtsSetVSFlags(ts, (rpmVSFlags)myint);
	ok = JS_TRUE;
	break;
    case _TYPE:
	if (!JS_ValueToInt32(cx, *vp, &myint))
	    break;
	(void) rpmtsSetType(ts, (rpmTSType)myint);
	ok = JS_TRUE;
	break;
    case _ARBGOAL:
	if (!JS_ValueToInt32(cx, *vp, &myint))
	    break;
	(void) rpmtsSetARBGoal(ts, (uint32_t)myint);
	ok = JS_TRUE;
	break;
    case _ROOTDIR:
	(void) rpmtsSetRootDir(ts, JS_GetStringBytes(JS_ValueToString(cx, *vp)));
	ok = JS_TRUE;
	break;
    case _CURRDIR:
	(void) rpmtsSetCurrDir(ts, JS_GetStringBytes(JS_ValueToString(cx, *vp)));
	ok = JS_TRUE;
	break;
    case _CHROOTDONE:
	if (!JS_ValueToInt32(cx, *vp, &myint))
	    break;
	(void) rpmtsSetChrootDone(ts, myint);
	ok = JS_TRUE;
	break;
    case _TID:
	if (!JS_ValueToInt32(cx, *vp, &myint))
	    break;
	(void) rpmtsSetTid(ts, (uint32_t)myint);
	ok = JS_TRUE;
	break;
    case _FLAGS:
	if (!JS_ValueToInt32(cx, *vp, &myint))
	    break;
	(void) rpmtsSetFlags(ts, myint);
	ok = JS_TRUE;
	break;
    case _DFLAGS:
	if (!JS_ValueToInt32(cx, *vp, &myint))
	    break;
	(void) rpmtsSetDFlags(ts, (rpmdepFlags)myint);
	ok = JS_TRUE;
	break;
    case _GOAL:
	if (!JS_ValueToInt32(cx, *vp, &myint))
	    break;
	(void) rpmtsSetGoal(ts, (tsmStage)myint);
	ok = JS_TRUE;
	break;
    case _DBMODE:
	if (!JS_ValueToInt32(cx, *vp, &myint))
	    break;
	(void) rpmtsSetDBMode(ts, myint);
	ok = JS_TRUE;
	break;
    case _COLOR:
	if (!JS_ValueToInt32(cx, *vp, &myint))
	    break;
	(void) rpmtsSetColor(ts, (uint32_t)(myint & 0x0f));
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
rpmts_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
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
rpmts_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
	JSObject **objp)
{
if (_debug)
fprintf(stderr, "==> %s(%p,%p,0x%llx,0x%x,%p) poperty %s flags 0x%x{%s,%s,%s,%s,%s}\n", __FUNCTION__, cx, obj, (unsigned long long)id, (unsigned)flags, objp,
		JS_GetStringBytes(JS_ValueToString(cx, id)), flags,
		(flags & JSRESOLVE_QUALIFIED) ? "qualified" : "",
		(flags & JSRESOLVE_ASSIGNING) ? "assigning" : "",
		(flags & JSRESOLVE_DETECTING) ? "detecting" : "",
		(flags & JSRESOLVE_DETECTING) ? "declaring" : "",
		(flags & JSRESOLVE_DETECTING) ? "classname" : "");
    return JS_TRUE;
}

static JSBool
rpmts_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
if (_debug)
fprintf(stderr, "==> %s(%p,%p,%d,%p) convert to %s\n", __FUNCTION__, cx, obj, type, vp, JS_GetTypeName(cx, type));
    return JS_TRUE;
}

/* --- Object ctors/dtors */
static void
rpmts_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmtsClass, NULL);

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);

#ifdef	NOTYET	/* XXX FIXME: avoid yarn assertion failure. */
    (void) rpmtsFree((rpmts)ptr);
#endif
}

static JSBool
rpmts_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval);

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	void * ptr = rpmtsCreate();

	if (ptr == NULL || !JS_SetPrivate(cx, obj, ptr)) {
	    (void) rpmtsFree((void *)ptr);
	    goto exit;
	}
    } else {
	if ((obj = JS_NewObject(cx, &rpmtsClass, NULL, NULL)) == NULL)
	    goto exit;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

/* --- Class initialization */
JSClass rpmtsClass = {
    "Ts", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE | JSCLASS_HAS_CACHED_PROTO(JSProto_Object),
    rpmts_addprop,   rpmts_delprop, rpmts_getprop, rpmts_setprop,
    (JSEnumerateOp)rpmts_enumerate, (JSResolveOp)rpmts_resolve,
    rpmts_convert,	rpmts_dtor,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

JSObject *
rpmjs_InitTsClass(JSContext *cx, JSObject* obj)
{
    JSObject * o;

if (_debug)
fprintf(stderr, "==> %s(%p,%p)\n", __FUNCTION__, cx, obj);

    o = JS_InitClass(cx, obj, NULL, &rpmtsClass, rpmts_ctor, 1,
		rpmts_props, rpmts_funcs, NULL, NULL);
assert(o != NULL);
    return o;
}

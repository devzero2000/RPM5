/** \ingroup js_c
 * \file js/rpmts-js.c
 */

#include "system.h"

#include "rpmts-js.h"

#include <argv.h>
#include <mire.h>

#include <rpmdb.h>

#define	_RPMTS_INTERNAL
#include <rpmts.h>

#include "debug.h"

/*@unchecked@*/
extern int _rpmjs_debug;

/*@unchecked@*/
static int _debug = 1;

static JSObject *
rpmtsLoadNVRA(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmtsClass, NULL);
    rpmts ts = ptr;
    JSObject * NVRA = JS_NewArrayObject(cx, 0, NULL);
    ARGV_t keys = NULL;
    int nkeys;
    int xx;
    int i;

    if (ts->rdb == NULL)
	(void) rpmtsOpenDB(ts, O_RDONLY);

    xx = rpmdbMireApply(rpmtsGetRdb(ts), RPMTAG_NVRA,
		RPMMIRE_STRCMP, NULL, &keys);
    nkeys = argvCount(keys);

    if (keys)
    for (i = 0; i < nkeys; i++) {
	JSString * valstr = JS_NewStringCopyZ(cx, keys[i]);
	jsval id = STRING_TO_JSVAL(valstr);
	JS_SetElement(cx, NVRA, i, &id);
    }

    JS_DefineProperty(cx, obj, "NVRA", OBJECT_TO_JSVAL(NVRA),
				NULL, NULL, JSPROP_ENUMERATE);

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p NVRA %p\n", __FUNCTION__, cx, obj, ptr, NVRA);

    return NVRA;
}

/* --- Object methods */

static JSBool
rpmts_mi(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    rpmts ts = (rpmts) JS_GetInstancePrivate(cx, obj, &rpmtsClass, NULL);
    JSObject *TagN = NULL;
    rpmTag tag = RPMDBI_PACKAGES;
    JSObject *Key = NULL;
    char * key = NULL;
    int keylen = 0;
    JSObject *Mi;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "/oo", &TagN, &Key)))
        goto exit;

    if (TagN) {
    }

    if (Key) {
    }

    if ((Mi = rpmjs_NewMiObject(cx, ts, tag, key, keylen)) == NULL)
	goto exit;

    *rval = OBJECT_TO_JSVAL(Mi);
    ok = JS_TRUE;

exit:
    return ok;
}

static JSFunctionSpec rpmts_funcs[] = {
    {"mi",	rpmts_mi,		0,0,0},
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

if (_debug < 0)
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
      {	JSString * str = JS_ValueToString(cx, id);
	const char * name = JS_GetStringBytes(str);
	if (!strcmp(name, "NVRA")) {
	    JSObject * NVRA = rpmtsLoadNVRA(cx, obj);
	    *vp = OBJECT_TO_JSVAL(NVRA);
	    ok = JS_TRUE;
	    break;
	}
      }	break;
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
rpmts_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
	JSObject **objp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmtsClass, NULL);
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
		(flags & JSRESOLVE_DECLARING) ? "declaring" : "",
		(flags & JSRESOLVE_CLASSNAME) ? "classname" : "");

    if (flags & JSRESOLVE_ASSIGNING) {
	ok = JS_TRUE;
	goto exit;
    }

    if ((idstr = JS_ValueToString(cx, id)) == NULL)
	goto exit;

    name = JS_GetStringBytes(idstr);
    if (ptr != NULL && !strcmp(name, "NVRA"))
	*objp = rpmtsLoadNVRA(cx, obj);
    else
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
rpmts_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
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
rpmts_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmtsClass, NULL);
if (_debug)
fprintf(stderr, "==> %s(%p,%p,%d,%p) ptr %p convert to %s\n", __FUNCTION__, cx, obj, type, vp, ptr, JS_GetTypeName(cx, type));
    return JS_TRUE;
}

/* --- Object ctors/dtors */
static rpmts
rpmts_init(JSContext *cx, JSObject *obj)
{
    rpmts ts;

    if ((ts = rpmtsCreate()) == NULL)
	return NULL;
    if (!JS_SetPrivate(cx, obj, (void *)ts)) {
	/* XXX error msg */
	(void) rpmtsFree(ts);
	return NULL;
    }
    return ts;
}

static void
rpmts_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmtsClass, NULL);
    rpmts ts = ptr;

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);

    (void) rpmtsFree(ts);
}

static JSBool
rpmts_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval);

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	if (rpmts_init(cx, obj) == NULL)
	    goto exit;
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
#ifdef	HACKERY
JSClass rpmtsClass = {
    "Ts", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE | JSCLASS_HAS_CACHED_PROTO(JSProto_Object),
    rpmts_addprop,   rpmts_delprop, rpmts_getprop, rpmts_setprop,
    (JSEnumerateOp)rpmts_enumerate, (JSResolveOp)rpmts_resolve,
    rpmts_convert,	rpmts_dtor,
    JSCLASS_NO_OPTIONAL_MEMBERS
};
#else
JSClass rpmtsClass = {
    "Ts", JSCLASS_NEW_RESOLVE | JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,   JS_PropertyStub, rpmts_getprop, JS_PropertyStub,
    (JSEnumerateOp)rpmts_enumerate, (JSResolveOp)rpmts_resolve,
    JS_ConvertStub,	rpmts_dtor,
    JSCLASS_NO_OPTIONAL_MEMBERS
};
#endif

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

JSObject *
rpmjs_NewTsObject(JSContext *cx)
{
    JSObject *obj;
    rpmts ts;

    if ((obj = JS_NewObject(cx, &rpmtsClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((ts = rpmts_init(cx, obj)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

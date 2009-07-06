/** \ingroup js_c
 * \file js/rpmst-js.c
 */

#include "system.h"

#include "rpmst-js.h"
#include "rpmjs-debug.h"

#include "debug.h"

/*@unchecked@*/
static int _debug = 0;

/* Required JSClass vectors */
#define	rpmst_addprop	JS_PropertyStub
#define	rpmst_delprop	JS_PropertyStub
#define	rpmst_convert	JS_ConvertStub

/* Optional JSClass vectors */
#define	rpmst_getobjectops	NULL
#define	rpmst_checkaccess	NULL
#define	rpmst_call		rpmst_call
#define	rpmst_construct		rpmst_ctor
#define	rpmst_xdrobject		NULL
#define	rpmst_hasinstance	NULL
#define	rpmst_mark		NULL
#define	rpmst_reserveslots	NULL

/* Extended JSClass vectors */
#define rpmst_equality		NULL
#define rpmst_outerobject	NULL
#define rpmst_innerobject	NULL
#define rpmst_iteratorobject	NULL
#define rpmst_wrappedobject	NULL

/* --- helpers */

/* --- Object methods */
static JSFunctionSpec rpmst_funcs[] = {
    JS_FS_END
};

/* --- Object properties */
enum rpmst_tinyid {
    _DEBUG	= -2,
    _DEV	= -3,
    _INO	= -4,
    _MODE	= -5,
    _NLINK	= -6,
    _UID	= -7,
    _GID	= -8,
    _RDEV	= -9,
    _SIZE	= -10,
    _BLKSIZE	= -11,
    _BLOCKS	= -12,
    _ATIME	= -13,
    _MTIME	= -14,
    _CTIME	= -15,
};

static JSPropertySpec rpmst_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"dev",	_DEV,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"ino",	_INO,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"mode",	_MODE,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"nlink",	_NLINK,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"uid",	_UID,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"gid",	_GID,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"rdev",	_RDEV,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"size",	_SIZE,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"blksize",	_BLKSIZE,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"blocks",	_BLOCKS,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"atime",	_ATIME,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"mtime",	_MTIME,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"ctime",	_CTIME,		JSPROP_ENUMERATE,	NULL,	NULL},
    {NULL, 0, 0, NULL, NULL}
};

#define	_GET_I(_p, _f)   ((_p) ? INT_TO_JSVAL((int)(_p)->_f) : JSVAL_VOID)

static JSBool
rpmst_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmstClass, NULL);
    struct stat * st = ptr;
    jsint tiny = JSVAL_TO_INT(id);
    time_t mytime = (time_t)0xffffffff;

_PROP_DEBUG_ENTRY(_debug < 0);

    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    if (ptr == NULL)
	return JS_TRUE;

    switch (tiny) {
    case _DEBUG:	*vp = INT_TO_JSVAL(_debug);		break;
    case _DEV:		*vp = _GET_I(st, st_dev);		break;
    case _INO:		*vp = _GET_I(st, st_ino);		break;
    case _MODE:		*vp = _GET_I(st, st_mode);	break;
    case _NLINK:	*vp = _GET_I(st, st_nlink);	break;
    case _UID:		*vp = _GET_I(st, st_uid);		break;
    case _GID:		*vp = _GET_I(st, st_gid);		break;
    case _RDEV:		*vp = _GET_I(st, st_rdev);	break;
    case _SIZE:		*vp = _GET_I(st, st_size);	break;
    case _BLKSIZE:	*vp = _GET_I(st, st_blksize);	break;
    case _BLOCKS:	*vp = _GET_I(st, st_blocks);	break;
    case _ATIME:	if (st) mytime = st->st_atime;	break;
    case _MTIME:	if (st) mytime = st->st_mtime;	break;
    case _CTIME:	if (st) mytime = st->st_ctime;	break;
    default:
	break;
    }

    if (mytime != (time_t)0xffffffff) {
	struct tm *tm = gmtime(&mytime);
	*vp = OBJECT_TO_JSVAL(js_NewDateObject(cx,
					tm->tm_year,
					tm->tm_mon,
					tm->tm_mday,
					tm->tm_hour,
					tm->tm_min,
					tm->tm_sec));
    }

    return JS_TRUE;
}

static JSBool
rpmst_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmstClass, NULL);
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

#ifdef	NOTYET
static JSBool
rpmst_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
	JSObject **objp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmstClass, NULL);

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
rpmst_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
		  jsval *statep, jsid *idp)
{

_ENUMERATE_DEBUG_ENTRY(_debug < 0);

    switch (op) {
    case JSENUMERATE_INIT:
	*statep = JSVAL_VOID;
        if (idp)
            *idp = JSVAL_ZERO;
        break;
    case JSENUMERATE_NEXT:
	*statep = JSVAL_VOID;
        if (*idp != JSVAL_VOID)
            break;
        /*@fallthrough@*/
    case JSENUMERATE_DESTROY:
	*statep = JSVAL_NULL;
        break;
    }
    return JS_TRUE;
}
#else
#define	rpmst_resolve	JS_ResolveStub
#define	rpmst_enumerate	JS_EnumerateStub
#endif

/* --- Object ctors/dtors */
static struct stat *
rpmst_init(JSContext *cx, JSObject *obj, JSObject *fno)
{
    struct stat * st = xcalloc(1, sizeof(*st));

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p) st %p\n", __FUNCTION__, cx, obj, fno, st);

    if (fno && OBJ_IS_STRING(cx, fno)) {
	const char * fn =
		JS_GetStringBytes(JS_ValueToString(cx, OBJECT_TO_JSVAL(fno)));
	if (Stat(fn, st) < 0) {
	    /* XXX error msg */
	    st = _free(st);
	}
    } else {
	st = _free(st);
    }

    if (!JS_SetPrivate(cx, obj, (void *)st)) {
	/* XXX error msg */
	st = _free(st);
    }
    return st;
}

static void
rpmst_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmstClass, NULL);
    struct stat * st = ptr;

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);
    st = _free(st);
}

static JSBool
rpmst_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_FALSE;
    JSObject *fno = NULL;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)%s\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ((cx->fp->flags & JSFRAME_CONSTRUCTING) ? " constructing" : ""));

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "/o", &fno)))
        goto exit;

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	(void) rpmst_init(cx, obj, fno);
    } else {
	if ((obj = JS_NewObject(cx, &rpmstClass, NULL, NULL)) == NULL)
	    goto exit;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

static JSBool
rpmst_call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    /* XXX obj is the global object so lookup "this" object. */
    JSObject * o = JSVAL_TO_OBJECT(argv[-2]);
    void * ptr = JS_GetInstancePrivate(cx, o, &rpmstClass, NULL);
    struct stat * st = ptr;
    JSBool ok = JS_FALSE;
    JSObject *_fno = NULL;

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "/o", &_fno)))
        goto exit;

    if (st) {
	st = _free(st);
	st = ptr = NULL;
	(void) JS_SetPrivate(cx, o, (void *)st);
    }

    st = ptr = rpmst_init(cx, o, _fno);

    *rval = OBJECT_TO_JSVAL(o);

    ok = JS_TRUE;

exit:
if (_debug)
fprintf(stderr, "<== %s(%p,%p,%p[%u],%p) o %p ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, o, ptr);

    return ok;
}

/* --- Class initialization */
JSClass rpmstClass = {
    /* XXX class should be "Stat" eventually, avoid name conflicts for now */
#ifdef	NOTYET
    "St", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE,
#else
    "St", JSCLASS_HAS_PRIVATE,
#endif
    rpmst_addprop,   rpmst_delprop, rpmst_getprop, rpmst_setprop,
    (JSEnumerateOp)rpmst_enumerate, (JSResolveOp)rpmst_resolve,
    rpmst_convert,	rpmst_dtor,

    rpmst_getobjectops,	rpmst_checkaccess,
    rpmst_call,		rpmst_construct,
    rpmst_xdrobject,	rpmst_hasinstance,
    rpmst_mark,		rpmst_reserveslots,
};

JSObject *
rpmjs_InitStClass(JSContext *cx, JSObject* obj)
{
    JSObject *proto;

if (_debug)
fprintf(stderr, "==> %s(%p,%p)\n", __FUNCTION__, cx, obj);

    proto = JS_InitClass(cx, obj, NULL, &rpmstClass, rpmst_ctor, 1,
		rpmst_props, rpmst_funcs, NULL, NULL);
assert(proto != NULL);
    return proto;
}

JSObject *
rpmjs_NewStObject(JSContext *cx, JSObject *fno)
{
    JSObject *obj;
    struct stat * st;

    if ((obj = JS_NewObject(cx, &rpmstClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((st = rpmst_init(cx, obj, fno)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

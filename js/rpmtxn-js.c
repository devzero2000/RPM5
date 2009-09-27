/** \ingroup js_c
 * \file js/rpmtxn-js.c
 */

#include "system.h"

#include "rpmtxn-js.h"
#include "rpmjs-debug.h"

#include <argv.h>

#include <db.h>

#include "debug.h"

/*@unchecked@*/
static int _debug = 0;

/* Required JSClass vectors */
#define	rpmtxn_addprop		JS_PropertyStub
#define	rpmtxn_delprop		JS_PropertyStub
#define	rpmtxn_convert		JS_ConvertStub

/* Optional JSClass vectors */
#define	rpmtxn_getobjectops	NULL
#define	rpmtxn_checkaccess	NULL
#define	rpmtxn_call		rpmtxn_call
#define	rpmtxn_construct		rpmtxn_ctor
#define	rpmtxn_xdrobject		NULL
#define	rpmtxn_hasinstance	NULL
#define	rpmtxn_mark		NULL
#define	rpmtxn_reserveslots	NULL

/* Extended JSClass vectors */
#define rpmtxn_equality		NULL
#define rpmtxn_outerobject	NULL
#define rpmtxn_innerobject	NULL
#define rpmtxn_iteratorobject	NULL
#define rpmtxn_wrappedobject	NULL

/* --- helpers */

/* --- Object methods */

static JSBool
rpmtxn_Abort(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmtxnClass, NULL);
    DB_TXN * txn = ptr;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    if (txn == NULL) goto exit;
    *rval = JSVAL_FALSE;

    {	int ret = txn->abort(txn);
        if (ret)
	    fprintf(stderr, "DB_TXN->abort: %s", db_strerror(ret));
        else
            *rval = JSVAL_TRUE;
	txn = ptr = NULL;
	(void) JS_SetPrivate(cx, obj, ptr);
    }

    ok = JS_TRUE;

exit:
    return ok;
}

static JSBool
rpmtxn_Commit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmtxnClass, NULL);
    DB_TXN * txn = ptr;
    uint32_t _flags = 0;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    if (txn == NULL) goto exit;
    *rval = JSVAL_FALSE;

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "/u", &_flags)))
	goto exit;

    {	int ret = txn->commit(txn, _flags);
        if (ret)
	    fprintf(stderr, "DB_TXN->commit: %s", db_strerror(ret));
        else
            *rval = JSVAL_TRUE;
	txn = ptr = NULL;
	(void) JS_SetPrivate(cx, obj, ptr);
    }

    ok = JS_TRUE;

exit:
    return ok;
}

static JSBool
rpmtxn_Discard(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmtxnClass, NULL);
    DB_TXN * txn = ptr;
    uint32_t _flags = 0;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    if (txn == NULL) goto exit;
    *rval = JSVAL_FALSE;

    {	int ret = txn->discard(txn, _flags);
        if (ret)
	    fprintf(stderr, "DB_TXN->discard: %s", db_strerror(ret));
        else
            *rval = JSVAL_TRUE;
	txn = ptr = NULL;
	(void) JS_SetPrivate(cx, obj, ptr);
    }

    ok = JS_TRUE;

exit:
    return ok;
}

static JSBool
rpmtxn_Prepare(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmtxnClass, NULL);
    DB_TXN * txn = ptr;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    if (txn == NULL) goto exit;
    *rval = JSVAL_FALSE;

	/* FIXME: todo++ used with DB_ENV->txn_recover */

    ok = JS_TRUE;

exit:
    return ok;
}

static JSFunctionSpec rpmtxn_funcs[] = {
    JS_FS("abort",	rpmtxn_Abort,		0,0,0),
    JS_FS("commit",	rpmtxn_Commit,		0,0,0),
    JS_FS("discard",	rpmtxn_Discard,		0,0,0),
    JS_FS("prepare",	rpmtxn_Prepare,		0,0,0),
    JS_FS_END
};

/* --- Object properties */

#define	_TABLE(_v)	#_v, _##_v, JSPROP_ENUMERATE, NULL, NULL

enum rpmtxn_tinyid {
    _DEBUG	= -2,
    _NAME	= -3,
    _ID		= -4,
    _DB_SET_LOCK_TIMEOUT	= -5,
    _DB_SET_TXN_TIMEOUT		= -6,
};

static JSPropertySpec rpmtxn_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"name",	_NAME,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"id",	_ID,		JSPROP_ENUMERATE,	NULL,	NULL},
    { _TABLE(DB_SET_LOCK_TIMEOUT) },
    { _TABLE(DB_SET_TXN_TIMEOUT) },

    {NULL, 0, 0, NULL, NULL}
};

#define	_RET_B(_bool)	((_bool) > 0 ? JSVAL_TRUE: JSVAL_FALSE)
#define	_RET_S(_str)	\
    ((_str) ? STRING_TO_JSVAL(JS_NewStringCopyZ(cx, (_str))) : JSVAL_NULL)
#define	_GET_S(_test)	((_test) ? _RET_S(_s) : JSVAL_VOID)
#define	_GET_U(_test)	((_test) ? INT_TO_JSVAL(_u) : JSVAL_VOID)
#define	_GET_I(_test)	((_test) ? INT_TO_JSVAL(_i) : JSVAL_VOID)
#define	_GET_B(_test)	((_test) ? _RET_B(_i) : JSVAL_VOID)

static JSBool
rpmtxn_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmtxnClass, NULL);
    DB_TXN * txn = ptr;
    const char * _s = NULL;
    jsint tiny = JSVAL_TO_INT(id);

    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    if (ptr == NULL)
	return JS_TRUE;

    switch (tiny) {
    case _DEBUG:
	*vp = INT_TO_JSVAL(_debug);
	break;
    case _NAME:	*vp = _GET_S(!txn->get_name(txn, &_s));	break;
    case _ID:	*vp = INT_TO_JSVAL(txn->id(txn));	break;
    default:
	break;
    }

    return JS_TRUE;
}

#define	_PUT_S(_put)	(JSVAL_IS_STRING(*vp) && !(_put) \
	? JSVAL_TRUE : JSVAL_FALSE)
#define	_PUT_U(_put)	(JSVAL_IS_INT(*vp) && !(_put) \
	? JSVAL_TRUE : JSVAL_FALSE)
#define	_PUT_I(_put)	(JSVAL_IS_INT(*vp) && !(_put) \
	? JSVAL_TRUE : JSVAL_FALSE)
#define	_PUT_L(_put)	(JSVAL_IS_INT(*vp) && !(_put) \
	? JSVAL_TRUE : JSVAL_FALSE)

static JSBool
rpmtxn_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmtxnClass, NULL);
    DB_TXN * txn = ptr;
    const char * _s = NULL;
    uint32_t _u = 0;
    uint32_t _nc = 0;
    jsint tiny = JSVAL_TO_INT(id);

    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    if (ptr == NULL)
	return JS_TRUE;

    if (JSVAL_IS_STRING(*vp))
	_s = JS_GetStringBytes(JS_ValueToString(cx, *vp));
#ifdef	NOTYET
    if (JSVAL_IS_INT(*vp))
	_u = JSVAL_TO_INT(*vp);
#endif

    switch (tiny) {
    case _DEBUG:
	if (!JS_ValueToInt32(cx, *vp, &_debug))
	    break;
	break;
    case _NAME:	*vp = _PUT_S(txn->set_name(txn, _s));	break;
#define	_JUMP(_v, _lbl)	_##_v:	_nc = _v;	goto _lbl
    case _JUMP(DB_SET_LOCK_TIMEOUT,		_set_timeout);
    case _JUMP(DB_SET_TXN_TIMEOUT,		_set_timeout);
#undef	_JUMP
    _set_timeout:
	*vp = _PUT_U(txn->set_timeout(txn, (db_timeout_t)_u, _nc));
	break;

    default:
	break;
    }

    return JS_TRUE;
}

static JSBool
rpmtxn_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
	JSObject **objp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmtxnClass, NULL);

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
rpmtxn_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
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

/* --- Object ctors/dtors */
static DB_TXN *
rpmtxn_init(JSContext *cx, JSObject *obj)
{
    DB_TXN * txn = NULL;
#ifdef	NOTYET
    uint32_t _flags = 0;

    if (rpmtxn_env_create(&db, _flags) || txn == NULL
     || !JS_SetPrivate(cx, obj, (void *)txn))
#else
    if (!JS_SetPrivate(cx, obj, (void *)txn))
#endif
    {
#ifdef	NOTYET
	if (txn)
	    (void) txn->close(txn, _flags);
#endif

	/* XXX error msg */
	txn = NULL;
    }

if (_debug)
fprintf(stderr, "<== %s(%p,%p) txn %p\n", __FUNCTION__, cx, obj, txn);

    return txn;
}

static void
rpmtxn_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmtxnClass, NULL);
#ifdef	NOTYET
    DB_TXN * txn = ptr;
    uint32_t _flags = 0;
#endif

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);
#ifdef	NOTYET
    if (db) {
	(void) db->close(db, _flags);
    }
#endif
}

static JSBool
rpmtxn_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)%s\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ((cx->fp->flags & JSFRAME_CONSTRUCTING) ? " constructing" : ""));

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	(void) rpmtxn_init(cx, obj);
    } else {
	if ((obj = JS_NewObject(cx, &rpmtxnClass, NULL, NULL)) == NULL)
	    goto exit;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

static JSBool
rpmtxn_call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    /* XXX obj is the global object so lookup "this" object. */
    JSObject * o = JSVAL_TO_OBJECT(argv[-2]);
    void * ptr = JS_GetInstancePrivate(cx, o, &rpmtxnClass, NULL);
#ifdef	NOTYET
    DB_TXN * txn = ptr;
    const char *_fn = NULL;
    const char * _con = NULL;
#endif
    JSBool ok = JS_FALSE;

#ifdef	NOTYET
    if (!(ok = JS_ConvertArguments(cx, argc, argv, "s", &_fn)))
        goto exit;

    *rval = (txn && _fn && (_con = rpmtxnLgetfilecon(txn, _fn)) != NULL)
	? STRING_TO_JSVAL(JS_NewStringCopyZ(cx, _con)) : JSVAL_VOID;
    _con = _free(_con);

    ok = JS_TRUE;

exit:
#endif

if (_debug)
fprintf(stderr, "<== %s(%p,%p,%p[%u],%p) o %p ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, o, ptr);

    return ok;
}

/* --- Class initialization */
JSClass rpmtxnClass = {
    "Txn", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE,
    rpmtxn_addprop,   rpmtxn_delprop, rpmtxn_getprop, rpmtxn_setprop,
    (JSEnumerateOp)rpmtxn_enumerate, (JSResolveOp)rpmtxn_resolve,
    rpmtxn_convert,	rpmtxn_dtor,

    rpmtxn_getobjectops,	rpmtxn_checkaccess,
    rpmtxn_call,		rpmtxn_construct,
    rpmtxn_xdrobject,	rpmtxn_hasinstance,
    rpmtxn_mark,		rpmtxn_reserveslots,
};

JSObject *
rpmjs_InitTxnClass(JSContext *cx, JSObject* obj)
{
    JSObject *proto;

if (_debug)
fprintf(stderr, "==> %s(%p,%p)\n", __FUNCTION__, cx, obj);

    proto = JS_InitClass(cx, obj, NULL, &rpmtxnClass, rpmtxn_ctor, 1,
		rpmtxn_props, rpmtxn_funcs, NULL, NULL);
assert(proto != NULL);
    return proto;
}

JSObject *
rpmjs_NewTxnObject(JSContext *cx)
{
    JSObject *obj;
    DB_TXN * txn;

    if ((obj = JS_NewObject(cx, &rpmtxnClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((txn = rpmtxn_init(cx, obj)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

/** \ingroup js_c
 * \file js/rpmseq-js.c
 */

#include "system.h"

#include "rpmseq-js.h"
#include "rpmtxn-js.h"
#include "rpmjs-debug.h"

#include <argv.h>

#include <db.h>

#include "debug.h"

/*@unchecked@*/
static int _debug = 0;

/* Required JSClass vectors */
#define	rpmseq_addprop		JS_PropertyStub
#define	rpmseq_delprop		JS_PropertyStub
#define	rpmseq_convert		JS_ConvertStub

/* Optional JSClass vectors */
#define	rpmseq_getobjectops	NULL
#define	rpmseq_checkaccess	NULL
#define	rpmseq_call		rpmseq_call
#define	rpmseq_construct		rpmseq_ctor
#define	rpmseq_xdrobject		NULL
#define	rpmseq_hasinstance	NULL
#define	rpmseq_mark		NULL
#define	rpmseq_reserveslots	NULL

/* Extended JSClass vectors */
#define rpmseq_equality		NULL
#define rpmseq_outerobject	NULL
#define rpmseq_innerobject	NULL
#define rpmseq_iteratorobject	NULL
#define rpmseq_wrappedobject	NULL

/* --- helpers */

/* --- Object methods */

#define	OBJ_IS_RPMTXN(_cx, _o)	(OBJ_GET_CLASS(_cx, _o) == &rpmtxnClass)

#define	DBT_INIT	{0}

static JSBool
rpmseq_Close(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmseqClass, NULL);
    DB_SEQUENCE * seq = ptr;
    uint32_t _flags = 0;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    if (seq == NULL) goto exit;
    *rval = JSVAL_FALSE;

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "/u", &_flags)))
	goto exit;

    {	int ret = seq->close(seq, _flags);
	if (ret)
	    fprintf(stderr, "DB_SEQUENCE->close: %s\n", db_strerror(ret));
	else
	    *rval = JSVAL_TRUE;
	seq = ptr = NULL;
	(void) JS_SetPrivate(cx, obj, ptr);
    }

    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmseq_Get(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmseqClass, NULL);
    DB_SEQUENCE * seq = ptr;
    JSObject * o = NULL;
    DB_TXN * _txn = NULL;
    int32_t _delta = 1;
    uint32_t _flags = 0;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    if (seq == NULL) goto exit;
    *rval = JSVAL_FALSE;

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "o/iu", &o, &_delta, &_flags)))
	goto exit;

    if (o && OBJ_IS_RPMTXN(cx, o))
	_txn = JS_GetInstancePrivate(cx, o, &rpmtxnClass, NULL);
    if (_delta <= 0)
	goto exit;

    {	db_seq_t _seqno = 0;
	int ret = seq->get(seq, _txn, _delta, &_seqno, _flags);
	switch (ret) {
	default:
	    fprintf(stderr, "DB_SEQUENCE->get: %s\n", db_strerror(ret));
	    goto exit;
	    break;
	case 0:
	    *rval = INT_TO_JSVAL(_seqno);
	    break;
	}
    }

    ok = JS_TRUE;

exit:
    return ok;
}

static JSBool
rpmseq_InitialValue(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmseqClass, NULL);
    DB_SEQUENCE * seq = ptr;
    uint32_t _value = 0;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    if (seq == NULL) goto exit;
    *rval = JSVAL_FALSE;

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "/u", &_value)))
	goto exit;

    {	db_seq_t value = _value;
	int ret = seq->initial_value(seq, value);
	if (ret)
	    fprintf(stderr, "DB_SEQUENCE->initial_value: %s\n", db_strerror(ret));
	else
	    *rval = JSVAL_TRUE;
    }

    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmseq_Open(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmseqClass, NULL);
    DB_SEQUENCE * seq = ptr;
    JSObject * o = NULL;
    DB_TXN * _txn = NULL;
    DBT _k = DBT_INIT;
    uint32_t _flags = DB_CREATE;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    if (seq == NULL) goto exit;
    *rval = JSVAL_FALSE;

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "os/u", &o, &_k.data, &_flags)))
	goto exit;

    if (o && OBJ_IS_RPMTXN(cx, o))
	_txn = JS_GetInstancePrivate(cx, o, &rpmtxnClass, NULL);
    if (_k.data)
	_k.size = strlen(_k.data)+1;

    {	int ret = seq->open(seq, _txn, &_k, _flags);
	switch (ret) {
	default:
	    fprintf(stderr, "DB_SEQUENCE->open: %s\n", db_strerror(ret));
	    goto exit;
	    break;
	case 0:
	    *rval = JSVAL_TRUE;
	    break;
	}
    }

    ok = JS_TRUE;

exit:
    return ok;
}

static JSBool
rpmseq_Remove(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmseqClass, NULL);
    DB_SEQUENCE * seq = ptr;
    JSObject * o = NULL;
    DB_TXN * _txn = NULL;
    uint32_t _flags = 0;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    if (seq == NULL) goto exit;
    *rval = JSVAL_FALSE;

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "o/u", &o, &_flags)))
	goto exit;

    if (o && OBJ_IS_RPMTXN(cx, o))
	_txn = JS_GetInstancePrivate(cx, o, &rpmtxnClass, NULL);

    {	int ret = seq->remove(seq, _txn, _flags);
	switch (ret) {
	default:
	    fprintf(stderr, "DB_SEQUENCE->remove: %s\n", db_strerror(ret));
	    goto exit;
	    break;
	case 0:
	    *rval = JSVAL_TRUE;
	    break;
	}
    }

    ok = JS_TRUE;

exit:
    return ok;
}

static JSBool
rpmseq_Stat(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmseqClass, NULL);
    DB_SEQUENCE * seq = ptr;
    uint32_t _flags = 0;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    if (seq == NULL) goto exit;
    *rval = JSVAL_FALSE;

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "/u", &_flags)))
	goto exit;

    {	DB_SEQUENCE_STAT ** _spp = NULL;
	int ret = seq->stat(seq, _spp, _flags);
	if (ret)
	    fprintf(stderr, "DB_SEQUENCE->stat: %s\n", db_strerror(ret));
	else
	    *rval = JSVAL_TRUE;
    }

    ok = JS_TRUE;

exit:
    return ok;
}

static JSBool
rpmseq_StatPrint(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmseqClass, NULL);
    DB_SEQUENCE * seq = ptr;
    uint32_t _flags = 0;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    if (seq == NULL) goto exit;
    *rval = JSVAL_FALSE;

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "/u", &_flags)))
	goto exit;

    {	int ret = seq->stat_print(seq, _flags);
	if (ret)
	    fprintf(stderr, "DB_SEQUENCE->stat_print: %s\n", db_strerror(ret));
	else
	    *rval = JSVAL_TRUE;
    }

    ok = JS_TRUE;

exit:
    return ok;
}

static JSFunctionSpec rpmseq_funcs[] = {
    JS_FS("close",		rpmseq_Close,		0,0,0),
    JS_FS("get",		rpmseq_Get,		0,0,0),
    JS_FS("initial_value",	rpmseq_InitialValue,	0,0,0),
    JS_FS("open",		rpmseq_Open,		0,0,0),
    JS_FS("remove",		rpmseq_Remove,		0,0,0),
    JS_FS("stat",		rpmseq_Stat,		0,0,0),
    JS_FS("stat_print",		rpmseq_StatPrint,	0,0,0),
    JS_FS_END
};

/* --- Object properties */

#define	_TABLE(_v)	#_v, _##_v, JSPROP_ENUMERATE, NULL, NULL

enum rpmseq_tinyid {
    _DEBUG	= -2,
};

static JSPropertySpec rpmseq_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},

    {NULL, 0, 0, NULL, NULL}
};

#define	_RET_B(_bool)	((_bool) > 0 ? JSVAL_TRUE: JSVAL_FALSE)
#define	_RET_S(_str)	\
    ((_str) ? STRING_TO_JSVAL(JS_NewStringCopyZ(cx, (_str))) : JSVAL_NULL)
#define	_GET_S(_test)	((_test) ? _RET_S(_s) : JSVAL_VOID)
#define	_GET_U(_test)	((_test) ? INT_TO_JSVAL(_u) : JSVAL_VOID)
#define	_GET_I(_test)	((_test) ? INT_TO_JSVAL(_i) : JSVAL_VOID)
#define	_GET_B(_test)	((_test) ? _RET_B(_i) : JSVAL_VOID)
#define	_GET_L(_test)	((_test) ? DOUBLE_TO_JSVAL((double)_l) : JSVAL_VOID)

static JSBool
rpmseq_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmseqClass, NULL);
    jsint tiny = JSVAL_TO_INT(id);

    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    if (ptr == NULL)
	return JS_TRUE;

    switch (tiny) {
    case _DEBUG:
	*vp = INT_TO_JSVAL(_debug);
	break;

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
rpmseq_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmseqClass, NULL);
    jsint tiny = JSVAL_TO_INT(id);

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
rpmseq_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
	JSObject **objp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmseqClass, NULL);

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
rpmseq_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
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
static DB_SEQUENCE *
rpmseq_init(JSContext *cx, JSObject *obj)
{
    DB_SEQUENCE * seq = NULL;
#ifdef	NOTYET
    uint32_t _flags = 0;

    if (rpmseq_env_create(&db, _flags) || seq == NULL
     || !JS_SetPrivate(cx, obj, (void *)seq))
#else
    if (!JS_SetPrivate(cx, obj, (void *)seq))
#endif
    {
	if (seq)
	    (void) seq->close(seq, 0);
	/* XXX error msg */
	seq = NULL;
    }

if (_debug)
fprintf(stderr, "<== %s(%p,%p) seq %p\n", __FUNCTION__, cx, obj, seq);

    return seq;
}

static void
rpmseq_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmseqClass, NULL);
    DB_SEQUENCE * seq = ptr;

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);
    if (seq)
	(void) seq->close(seq, 0);
}

static JSBool
rpmseq_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)%s\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ((cx->fp->flags & JSFRAME_CONSTRUCTING) ? " constructing" : ""));

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	(void) rpmseq_init(cx, obj);
    } else {
	if ((obj = JS_NewObject(cx, &rpmseqClass, NULL, NULL)) == NULL)
	    goto exit;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

static JSBool
rpmseq_call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    /* XXX obj is the global object so lookup "this" object. */
    JSObject * o = JSVAL_TO_OBJECT(argv[-2]);
    void * ptr = JS_GetInstancePrivate(cx, o, &rpmseqClass, NULL);
#ifdef	NOTYET
    DB_SEQUENCE * seq = ptr;
    const char *_fn = NULL;
    const char * _con = NULL;
#endif
    JSBool ok = JS_FALSE;

#ifdef	NOTYET
    if (!(ok = JS_ConvertArguments(cx, argc, argv, "s", &_fn)))
        goto exit;

    *rval = (seq && _fn && (_con = rpmseqLgetfilecon(seq, _fn)) != NULL)
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
JSClass rpmseqClass = {
    "Seq", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE,
    rpmseq_addprop,   rpmseq_delprop, rpmseq_getprop, rpmseq_setprop,
    (JSEnumerateOp)rpmseq_enumerate, (JSResolveOp)rpmseq_resolve,
    rpmseq_convert,	rpmseq_dtor,

    rpmseq_getobjectops,	rpmseq_checkaccess,
    rpmseq_call,		rpmseq_construct,
    rpmseq_xdrobject,	rpmseq_hasinstance,
    rpmseq_mark,		rpmseq_reserveslots,
};

JSObject *
rpmjs_InitSeqClass(JSContext *cx, JSObject* obj)
{
    JSObject *proto;

if (_debug)
fprintf(stderr, "==> %s(%p,%p)\n", __FUNCTION__, cx, obj);

    proto = JS_InitClass(cx, obj, NULL, &rpmseqClass, rpmseq_ctor, 1,
		rpmseq_props, rpmseq_funcs, NULL, NULL);
assert(proto != NULL);
    return proto;
}

JSObject *
rpmjs_NewSeqObject(JSContext *cx)
{
    JSObject *obj;
    DB_SEQUENCE * seq;

    if ((obj = JS_NewObject(cx, &rpmseqClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((seq = rpmseq_init(cx, obj)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

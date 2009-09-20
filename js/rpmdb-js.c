/** \ingroup js_c
 * \file js/rpmdb-js.c
 */

#include "system.h"

#include "rpmdb-js.h"
#include "rpmjs-debug.h"

#include <db.h>

#include "debug.h"

/*@unchecked@*/
static int _debug = 0;

/* Required JSClass vectors */
#define	rpmdb_addprop		JS_PropertyStub
#define	rpmdb_delprop		JS_PropertyStub
#define	rpmdb_convert		JS_ConvertStub

/* Optional JSClass vectors */
#define	rpmdb_getobjectops	NULL
#define	rpmdb_checkaccess	NULL
#define	rpmdb_call		rpmdb_call
#define	rpmdb_construct		rpmdb_ctor
#define	rpmdb_xdrobject		NULL
#define	rpmdb_hasinstance	NULL
#define	rpmdb_mark		NULL
#define	rpmdb_reserveslots	NULL

/* Extended JSClass vectors */
#define rpmdb_equality		NULL
#define rpmdb_outerobject	NULL
#define rpmdb_innerobject	NULL
#define rpmdb_iteratorobject	NULL
#define rpmdb_wrappedobject	NULL

/* --- helpers */
static const char _home[] = "./rpmdb";
static uint64_t _cachesize = 1024 * 1024;
static int _ncaches = 1;
static int _eflags = DB_CREATE | DB_INIT_MPOOL;

static int rpmdb_env_create(DB_ENV ** dbenvp, uint32_t flags)
{
    FILE * _errfile = stderr;
    int ret;

    if ((ret = db_env_create(dbenvp, flags)) != 0) {
	fprintf(stderr, "db_env_create: %s", db_strerror(ret));
	ret = 1;
	goto exit;
    }

    (*dbenvp)->set_errfile(*dbenvp, _errfile);

    (*dbenvp)->set_cachesize(*dbenvp,
	(_cachesize >> 32), (_cachesize & 0xffffffff), _ncaches);

    if ((ret = (*dbenvp)->open(*dbenvp, _home, _eflags, 0)) != 0) {
	(*dbenvp)->err(*dbenvp, ret, "DB_ENV->open: %s", _home);
	ret = 1;
	goto exit;
    }

exit:
    return ret;
}

/* --- Object methods */

static JSFunctionSpec rpmdb_funcs[] = {
    JS_FS_END
};

/* --- Object properties */
enum rpmdb_tinyid {
    _DEBUG	= -2,
    _HOME	= -3,
    _OPENFLAGS	= -4,
    _DATADIRS	= -5,
    _CREATEDIR	= -6,
    _ENCRYPTFLAGS = -7,
    _ERRFILE	= -8,
    _ERRPFX	= -9,
    _FLAGS	= -10,
    _IDIRMODE	= -11,
    _MSGFILE	= -12,
    _SHMKEY	= -13,
    _THREADCNT	= -14,
    _TIMEOUT	= -15,
    _TMPDIR	= -16,
    _VERBOSE	= -17,
    _LKCONFLICTS = -18,
    _LKDETECT	= -19,
    _LKMAXLOCKERS = -20,
    _LKMAXLOCKS	= -21,
    _LKMAXOBJS	= -22,
    _LKPARTITIONS = -23,
    _LOGCONFIG	= -24,
    _LGBSIZE	= -25,
    _LGDIR	= -26,
    _LGFILEMODE	= -27,
    _LGMAX	= -28,
    _LGREGIONMAX = -29,
};

static JSPropertySpec rpmdb_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"home",	_HOME,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"open_flags", _OPENFLAGS,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"data_dirs", _DATADIRS,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"create_dir", _CREATEDIR,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"encrypt_flags", _ENCRYPTFLAGS, JSPROP_ENUMERATE,	NULL,	NULL},
    {"errfile",	_ERRFILE,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"errpfx",	_ERRPFX,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"flags",	_FLAGS,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"idirmode", _IDIRMODE,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"msgfile", _MSGFILE,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"shm_key", _SHMKEY,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"thread_count", _THREADCNT, JSPROP_ENUMERATE,	NULL,	NULL},
    {"timeout", _TIMEOUT,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"tmp_dir", _TMPDIR,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"verbose", _VERBOSE,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"lk_conflicts", _LKCONFLICTS, JSPROP_ENUMERATE,	NULL,	NULL},
    {"lk_detect", _LKDETECT,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"lk_max_lockers", _LKMAXLOCKERS, JSPROP_ENUMERATE,	NULL,	NULL},
    {"lk_max_locks", _LKMAXLOCKS, JSPROP_ENUMERATE,	NULL,	NULL},
    {"lk_max_objects", _LKMAXOBJS, JSPROP_ENUMERATE,	NULL,	NULL},
    {"lk_partitions", _LKPARTITIONS, JSPROP_ENUMERATE,	NULL,	NULL},
    {"log_config", _LOGCONFIG, JSPROP_ENUMERATE,	NULL,	NULL},
    {"lg_bsize", _LGBSIZE,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"lg_dir", _LGDIR,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"lg_filemode", _LGFILEMODE, JSPROP_ENUMERATE,	NULL,	NULL},
    {"lg_max", _LGMAX,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"lg_regionmax", _LGREGIONMAX, JSPROP_ENUMERATE,	NULL,	NULL},
    {NULL, 0, 0, NULL, NULL}
};

#define	_RET_S(_str)	\
    ((_str) ? STRING_TO_JSVAL(JS_NewStringCopyZ(cx, (_str))) : JSVAL_NULL)
#define	_GET_S(_test)	((_test) ? _RET_S(_s) : JSVAL_VOID)
#define	_GET_U(_test)	((_test) ? INT_TO_JSVAL(_u) : JSVAL_VOID)
#define	_GET_L(_test)	((_test) ? DOUBLE_TO_JSVAL((double)_l) : JSVAL_VOID)

static JSBool
rpmdb_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdbClass, NULL);
    DB_ENV * dbenv = ptr;
    const char * _s = NULL;
    uint32_t _u = 0;
    long _l = 0;
    FILE * _fp = NULL;
    jsint tiny = JSVAL_TO_INT(id);

    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    if (ptr == NULL)
	return JS_TRUE;

    switch (tiny) {
    case _DEBUG:
	*vp = INT_TO_JSVAL(_debug);
	break;
    case _HOME:		*vp = _GET_S(!dbenv->get_home(dbenv, &_s));	break;
    case _OPENFLAGS:	*vp = _GET_U(!dbenv->get_open_flags(dbenv, &_u)); break;
    case _DATADIRS:	*vp = JSVAL_VOID;	break;
    case _CREATEDIR:	*vp = _GET_S(!dbenv->get_create_dir(dbenv, &_s)); break;
    case _ENCRYPTFLAGS:	*vp = _GET_U(!dbenv->get_encrypt_flags(dbenv, &_u)); break;
    case _ERRFILE:
	dbenv->get_errfile(dbenv, &_fp);
	_s = (_fp ? "other" : NULL);
	if (_fp == stdin)	_s = "stdin";
	if (_fp == stdout)	_s = "stdout";
	if (_fp == stderr)	_s = "stderr";
	*vp = _RET_S(_s);
	break;
    case _ERRPFX:
	dbenv->get_errpfx(dbenv, &_s);
	*vp = _RET_S(_s);
	break;
    case _FLAGS:	*vp = _GET_U(!dbenv->get_flags(dbenv, &_u)); break;
    case _IDIRMODE:	*vp = _GET_S(!dbenv->get_intermediate_dir_mode(dbenv, &_s)); break;
    case _MSGFILE:
	dbenv->get_msgfile(dbenv, &_fp);
	_s = (_fp ? "other" : NULL);
	if (_fp == stdin)	_s = "stdin";
	if (_fp == stdout)	_s = "stdout";
	if (_fp == stderr)	_s = "stderr";
	*vp = _RET_S(_s);
	break;
    case _SHMKEY:	*vp = _GET_L(!dbenv->get_shm_key(dbenv, &_l));	break;
    case _THREADCNT:	*vp = _GET_U(!dbenv->get_thread_count(dbenv, &_u)); break;
    case _TIMEOUT:	*vp = JSVAL_VOID;	break;	/* XXX FIXME db_timeout_t * */
    case _TMPDIR:	*vp = _GET_S(!dbenv->get_tmp_dir(dbenv, &_s));	break;
    case _VERBOSE:	*vp = JSVAL_VOID;	break;	/* XXX FIXME enum */
    case _LKCONFLICTS:	*vp = JSVAL_VOID;	break;
    case _LKDETECT:	*vp = JSVAL_VOID;	break;
    case _LKMAXLOCKERS:	*vp = JSVAL_VOID;	break;
    case _LKMAXLOCKS:	*vp = JSVAL_VOID;	break;
    case _LKMAXOBJS:	*vp = JSVAL_VOID;	break;
    case _LKPARTITIONS:	*vp = JSVAL_VOID;	break;
    case _LOGCONFIG:	*vp = JSVAL_VOID;	break;
    case _LGBSIZE:	*vp = JSVAL_VOID;	break;
    case _LGDIR:	*vp = JSVAL_VOID;	break;
    case _LGFILEMODE:	*vp = JSVAL_VOID;	break;
    case _LGMAX:	*vp = JSVAL_VOID;	break;
    case _LGREGIONMAX:	*vp = JSVAL_VOID;	break;
    default:
	break;
    }

    return JS_TRUE;
}

#define	_PUT_CON(_put)	(JSVAL_IS_STRING(*vp) && !(_put) \
	? JSVAL_TRUE : JSVAL_FALSE)
#define	_PUT_INT(_put)	(JSVAL_IS_INT(*vp) && !(_put) \
	? JSVAL_TRUE : JSVAL_FALSE)

static JSBool
rpmdb_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdbClass, NULL);
    jsint tiny = JSVAL_TO_INT(id);

    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    if (ptr == NULL)
	return JS_TRUE;

    switch (tiny) {
    case _DEBUG:
	if (!JS_ValueToInt32(cx, *vp, &_debug))
	    break;
	break;
    /* dbenv->add_data_dir() */
    case _DATADIRS:
    case _CREATEDIR:
    case _ENCRYPTFLAGS:
    case _ERRFILE:
    case _ERRPFX:
    case _FLAGS:
    case _IDIRMODE:
    case _MSGFILE:
    case _SHMKEY:
    case _THREADCNT:
    case _TIMEOUT:
    case _TMPDIR:
    case _VERBOSE:
    case _LKCONFLICTS:
    case _LKDETECT:
    case _LKMAXLOCKERS:
    case _LKMAXLOCKS:
    case _LKMAXOBJS:
    case _LKPARTITIONS:
    case _LOGCONFIG:
    case _LGBSIZE:
    case _LGDIR:
    case _LGFILEMODE:
    case _LGMAX:
    case _LGREGIONMAX:
    default:
	break;
    }

    return JS_TRUE;
}

static JSBool
rpmdb_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
	JSObject **objp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdbClass, NULL);

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
rpmdb_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
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
static DB_ENV *
rpmdb_init(JSContext *cx, JSObject *obj)
{
    DB_ENV * dbenv = NULL;
    uint32_t _flags = 0;

    if (rpmdb_env_create(&dbenv, _flags) || dbenv == NULL
     ||!JS_SetPrivate(cx, obj, (void *)dbenv))
    {
	if (dbenv)
	    (void) dbenv->close(dbenv, _flags);

	/* XXX error msg */
	dbenv = NULL;
    }

if (_debug)
fprintf(stderr, "<== %s(%p,%p) dbenv %p\n", __FUNCTION__, cx, obj, dbenv);

    return dbenv;
}

static void
rpmdb_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdbClass, NULL);
    DB_ENV * dbenv = ptr;
    uint32_t _flags = 0;

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);
    if (dbenv) {
	(void) dbenv->close(dbenv, _flags);
    }
}

static JSBool
rpmdb_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)%s\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ((cx->fp->flags & JSFRAME_CONSTRUCTING) ? " constructing" : ""));

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	(void) rpmdb_init(cx, obj);
    } else {
	if ((obj = JS_NewObject(cx, &rpmdbClass, NULL, NULL)) == NULL)
	    goto exit;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

static JSBool
rpmdb_call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    /* XXX obj is the global object so lookup "this" object. */
    JSObject * o = JSVAL_TO_OBJECT(argv[-2]);
    void * ptr = JS_GetInstancePrivate(cx, o, &rpmdbClass, NULL);
#ifdef	NOTYET
    DB_ENV * dbenv = ptr;
    const char *_fn = NULL;
    const char * _con = NULL;
#endif
    JSBool ok = JS_FALSE;

#ifdef	NOTYET
    if (!(ok = JS_ConvertArguments(cx, argc, argv, "s", &_fn)))
        goto exit;

    *rval = (db && _fn && (_con = rpmdbLgetfilecon(db, _fn)) != NULL)
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
JSClass rpmdbClass = {
    "Db", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE,
    rpmdb_addprop,   rpmdb_delprop, rpmdb_getprop, rpmdb_setprop,
    (JSEnumerateOp)rpmdb_enumerate, (JSResolveOp)rpmdb_resolve,
    rpmdb_convert,	rpmdb_dtor,

    rpmdb_getobjectops,	rpmdb_checkaccess,
    rpmdb_call,		rpmdb_construct,
    rpmdb_xdrobject,	rpmdb_hasinstance,
    rpmdb_mark,		rpmdb_reserveslots,
};

JSObject *
rpmjs_InitDbClass(JSContext *cx, JSObject* obj)
{
    JSObject *proto;

if (_debug)
fprintf(stderr, "==> %s(%p,%p)\n", __FUNCTION__, cx, obj);

    proto = JS_InitClass(cx, obj, NULL, &rpmdbClass, rpmdb_ctor, 1,
		rpmdb_props, rpmdb_funcs, NULL, NULL);
assert(proto != NULL);
    return proto;
}

JSObject *
rpmjs_NewDbObject(JSContext *cx)
{
    JSObject *obj;
    DB_ENV * dbenv;

    if ((obj = JS_NewObject(cx, &rpmdbClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((dbenv = rpmdb_init(cx, obj)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

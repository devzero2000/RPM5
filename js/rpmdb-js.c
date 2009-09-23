/** \ingroup js_c
 * \file js/rpmdb-js.c
 */

#include "system.h"

#include "rpmdb-js.h"
#include "rpmjs-debug.h"

#include <argv.h>

#include <db.h>

#include "debug.h"

/*@unchecked@*/
static int _debug = -1;

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
#ifdef	NOTYET
static int
rpmdb_cmpint(DB *dbp, const DBT *a, const DBT *b)
{
	int ai, bi;

	memcpy(&ai, a->data, sizeof(int));
	memcpy(&bi, b->data, sizeof(int));
	return (ai - bi);
}

static int
rpmdb_cmplong(DB *dbp, const DBT *a, const DBT *b)
{
	long ai, bi;

	memcpy(&ai, a->data, sizeof(long));
	memcpy(&bi, b->data, sizeof(long));
	return (ai - bi);
}
#endif

static uint32_t _preflags = 0;
static int (*_bt_compare) (DB *, const DBT *, const DBT *) = NULL;

static int
rpmdb_create(DB ** dbpp, DB_ENV * dbenv, uint32_t flags)
{
    FILE * _errfile = stderr;
    int ret = db_create(dbpp, dbenv, flags);

    if (ret) {
	if (dbenv)
	    dbenv->err(dbenv, ret, "db_create");
	else
	    fprintf(stderr, "db_create: %s", db_strerror(ret));
	goto exit;
    }

    if (_preflags != 0)
	(*dbpp)->set_flags(*dbpp, _preflags);

    if (_bt_compare != NULL)
	(*dbpp)->set_bt_compare(*dbpp, _bt_compare);

    if (dbenv)
	dbenv->get_errfile(dbenv, &_errfile);
    if (_errfile)
	(*dbpp)->set_errfile(*dbpp, _errfile);

    (*dbpp)->app_private = NULL;

exit:
    return ret;
}

static int
rpmdb_open(DB * db, DB_TXN * txnid, const char * file, const char * database,
		uint32_t type, uint32_t oflags, int mode)
{
    int ret = db->open(db, txnid, file, database, type, oflags, mode);

    if (ret) db->err(db, ret, "DB->open: %s", file);
    return ret;
}

/* --- Object methods */

#define	_zalloca(_p)	memset(alloca(sizeof(*_p)), 0, sizeof(*_p))

static JSBool
rpmdb_Close(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdbClass, NULL);
    DB * db = ptr;
    uint32_t _flags = 0;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    *rval = JSVAL_TRUE;

    if (db) {
	int ret = db->close(db, _flags);
	if (ret)
	    db->err(db, ret, "DB->close");
	db = ptr = NULL;
	(void) JS_SetPrivate(cx, obj, ptr);
    }

    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmdb_Cursor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdbClass, NULL);
    DB * db = ptr;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    *rval = JSVAL_TRUE;

    if (db) {
	DB_TXN * _txnid = NULL;
	DBC * _dbc;
	uint32_t _flags = 0;
	int ret = db->cursor(db, _txnid, &_dbc, _flags);
	if (ret)
	    db->err(db, ret, "DB->cursor");
    }

    ok = JS_TRUE;

    return ok;
}

static JSBool
rpmdb_Del(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdbClass, NULL);
    DB * db = ptr;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    *rval = JSVAL_TRUE;

    if (db) {
	DB_TXN * _txnid = NULL;
	DBT * _key = _zalloca(_key);
	uint32_t _flags = 0;
	int ret = db->del(db, _txnid, _key, _flags);
	if (ret)
	    db->err(db, ret, "DB->del");
    }

    ok = JS_TRUE;

    return ok;
}

static JSBool
rpmdb_Exists(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdbClass, NULL);
    DB * db = ptr;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    *rval = JSVAL_TRUE;

    if (db) {
	DB_TXN * _txnid = NULL;
	DBT * _key = _zalloca(_key);
	uint32_t _flags = 0;
	int ret = db->exists(db, _txnid, _key, _flags);
	if (ret)
	    db->err(db, ret, "DB->exists");
    }

    ok = JS_TRUE;

    return ok;
}

static JSBool
rpmdb_Get(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdbClass, NULL);
    DB * db = ptr;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    *rval = JSVAL_TRUE;

    if (db) {
	DB_TXN * _txnid = NULL;
	DBT * _key = _zalloca(_key);
	DBT * _data = _zalloca(_key);
	uint32_t _flags = 0;
	int ret = db->get(db, _txnid, _key, _data, _flags);
	if (ret)
	    db->err(db, ret, "DB->get");
    }

    ok = JS_TRUE;

    return ok;
}

static JSBool
rpmdb_Open(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdbClass, NULL);
    DB * db = ptr;
    const char * _file = NULL;
    DBTYPE _type = DB_HASH;
    uint32_t _oflags = DB_CREATE;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    *rval = JSVAL_FALSE;

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "s/uu", &_file, &_type, &_oflags)))
	goto exit;

    if (db->app_private == NULL) {
	DB_TXN * _txnid = NULL;
	const char * _database = NULL;
	int _mode = 0644;
	int ret = rpmdb_open(db, _txnid, _file, _database, _type, _oflags, _mode);
	if (ret) {
	    db->err(db, ret, "DB->open");
	    goto exit;
	}
	db->app_private = obj;
	*rval = JSVAL_TRUE;
    }

exit:
    ok = JS_TRUE;
    return ok;
}

static JSBool
rpmdb_Put(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdbClass, NULL);
    DB * db = ptr;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    *rval = JSVAL_TRUE;

    if (db) {
	DB_TXN * _txnid = NULL;
	DBT * _key = _zalloca(_key);
	DBT * _data = _zalloca(_key);
	uint32_t _flags = 0;
	int ret = db->put(db, _txnid, _key, _data, _flags);
	if (ret)
	    db->err(db, ret, "DB->put");
    }

    ok = JS_TRUE;

    return ok;
}

static JSBool
rpmdb_Remove(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdbClass, NULL);
    DB * db = ptr;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    *rval = JSVAL_TRUE;

    if (db) {
	const char * _file = NULL;
	const char * _database = NULL;
	uint32_t _flags = 0;
	int ret = db->remove(db, _file, _database, _flags);
	if (ret)
	    db->err(db, ret, "DB->remove");
    }

    ok = JS_TRUE;

    return ok;
}

static JSBool
rpmdb_Rename(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdbClass, NULL);
    DB * db = ptr;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    *rval = JSVAL_TRUE;

    if (db) {
	const char * _file = NULL;
	const char * _database = NULL;
	const char * _newname = NULL;
	uint32_t _flags = 0;
	int ret = db->rename(db, _file, _database, _newname, _flags);
	if (ret)
	    db->err(db, ret, "DB->rename");
    }

    ok = JS_TRUE;

    return ok;
}

static JSBool
rpmdb_Stat(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdbClass, NULL);
    DB * db = ptr;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    *rval = JSVAL_TRUE;

    if (db) {
	DB_TXN * _txnid = NULL;
	void * _sp = NULL;
	uint32_t _flags = 0;
	int ret = db->stat(db, _txnid, _sp, _flags);
	if (ret)
	    db->err(db, ret, "DB->stat");
    }

    ok = JS_TRUE;

    return ok;
}

static JSBool
rpmdb_Sync(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdbClass, NULL);
    DB * db = ptr;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    *rval = JSVAL_TRUE;

    if (db) {
	uint32_t _flags = 0;
	int ret = db->sync(db, _flags);
	if (ret)
	    db->err(db, ret, "DB->sync");
    }

    ok = JS_TRUE;

    return ok;
}

static JSBool
rpmdb_Truncate(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdbClass, NULL);
    DB * db = ptr;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    *rval = JSVAL_TRUE;

    if (db) {
	DB_TXN * _txnid = NULL;
	uint32_t _count = 0;
	uint32_t _flags = 0;
	int ret = db->truncate(db, _txnid, &_count, _flags);
	if (ret)
	    db->err(db, ret, "DB->truncate");
    }

    ok = JS_TRUE;

    return ok;
}

static JSBool
rpmdb_Upgrade(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdbClass, NULL);
    DB * db = ptr;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    *rval = JSVAL_TRUE;

    if (db) {
	const char * _file = NULL;
	uint32_t _flags = 0;
	int ret = db->upgrade(db, _file, _flags);
	if (ret)
	    db->err(db, ret, "DB->upgrade");
    }

    ok = JS_TRUE;

    return ok;
}

static JSBool
rpmdb_Verify(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdbClass, NULL);
    DB * db = ptr;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    *rval = JSVAL_TRUE;

    if (db) {
	const char * _file = NULL;
	const char * _database = NULL;
	FILE * _outfile = NULL;
	uint32_t _flags = 0;
	int ret = db->verify(db, _file, _database, _outfile, _flags);
	if (ret)
	    db->err(db, ret, "DB->verify");
    }

    ok = JS_TRUE;

    return ok;
}

static JSFunctionSpec rpmdb_funcs[] = {
    JS_FS("close",	rpmdb_Close,		0,0,0),
    JS_FS("cursor",	rpmdb_Cursor,		0,0,0),
    JS_FS("del",	rpmdb_Del,		0,0,0),
    JS_FS("exists",	rpmdb_Exists,		0,0,0),
    JS_FS("get",	rpmdb_Get,		0,0,0),
    JS_FS("open",	rpmdb_Open,		0,0,0),
    JS_FS("put",	rpmdb_Put,		0,0,0),
    JS_FS("remove",	rpmdb_Remove,		0,0,0),
    JS_FS("rename",	rpmdb_Rename,		0,0,0),
    JS_FS("stat",	rpmdb_Stat,		0,0,0),
    JS_FS("sync",	rpmdb_Sync,		0,0,0),
    JS_FS("truncate",	rpmdb_Truncate,		0,0,0),
    JS_FS("upgrade",	rpmdb_Upgrade,		0,0,0),
    JS_FS("verify",	rpmdb_Verify,		0,0,0),
    JS_FS_END
};

/* --- Object properties */

#define	_TABLE(_v)	#_v, _##_v, JSPROP_ENUMERATE, NULL, NULL

enum rpmdb_tinyid {
    _DEBUG		= -2,
    _BYTESWAPPED	= -3,
    _DBFILE		= -4,
    _DBNAME		= -5,
    _MULTIPLE		= -6,
    _OPEN_FLAGS		= -7,
    _TYPE		= -8,
    _BT_MINKEY		= -9,
    _CACHESIZE		= -10,
    _CREATE_DIR		= -11,
    _ENCRYPT		= -12,
    _ERRFILE		= -13,
    _ERRPFX		= -14,
    _FLAGS		= -15,
    _H_FFACTOR		= -16,
    _H_NELEM		= -17,
    _LORDER		= -18,
    _MSGFILE		= -19,
    _PAGESIZE		= -20,
    _PARTITION_DIRS	= -21,
    _PRIORITY		= -22,
    _Q_EXTENTSIZE	= -23,
    _RE_DELIM		= -24,
    _RE_LEN		= -25,
    _RE_PAD		= -26,
    _RE_SOURCE		= -27,
};

static JSPropertySpec rpmdb_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"byteswapped", _BYTESWAPPED, JSPROP_ENUMERATE,	NULL,	NULL},
    {"dbfile", _DBFILE,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"dbname", _DBNAME,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"multiple", _MULTIPLE,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"open_flags", _OPEN_FLAGS,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"type",	_TYPE,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"bt_minkey", _BT_MINKEY,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"cachesize", _CACHESIZE,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"create_dir", _CREATE_DIR,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"encrypt",	_ENCRYPT,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"errfile",	_ERRFILE,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"errpfx",	_ERRPFX,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"flags",	_FLAGS,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"h_ffactor", _H_FFACTOR,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"h_nelem",	_H_NELEM,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"lorder",	_LORDER,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"msgfile",	_MSGFILE,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"pagesize", _PAGESIZE,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"partition_dirs", _PARTITION_DIRS,	JSPROP_ENUMERATE, NULL,	NULL},
    {"priority", _PRIORITY,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"q_extentsize", _Q_EXTENTSIZE, JSPROP_ENUMERATE,	NULL,	NULL},
    {"re_delim", _RE_DELIM,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"re_len",	_RE_LEN,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"re_pad",	_RE_PAD,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"re_source", _RE_SOURCE,	JSPROP_ENUMERATE,	NULL,	NULL},
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
rpmdb_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdbClass, NULL);
    DB * db = ptr;
    const char ** _av = NULL;
    int _ac = 0;
    const char * _s = NULL;
    const char * _t = NULL;
    uint32_t _gb = 0;
    uint32_t _u = 0;
    int _i = 0;
    FILE * _fp = NULL;
    jsint tiny = JSVAL_TO_INT(id);

    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    if (ptr == NULL)
	return JS_TRUE;

    switch (tiny) {
    case _DEBUG:
	*vp = INT_TO_JSVAL(_debug);
	break;

    case _BYTESWAPPED:	*vp = _GET_I(!db->get_byteswapped(db, &_i));	break;
    case _DBFILE:	*vp = _GET_S(!db->get_dbname(db, &_s, &_t));	break;
    case _DBNAME:	*vp = _GET_S(!db->get_dbname(db, &_t, &_s));	break;
    case _MULTIPLE:	*vp = _RET_B(db->get_multiple(db));		break;
    case _OPEN_FLAGS:	*vp = _GET_U(!db->get_open_flags(db, &_u));	break;
    case _TYPE:		*vp = _GET_U(!db->get_type(db, &_u));	break;
    case _BT_MINKEY:	*vp = _GET_U(!db->get_bt_minkey(db, &_u));	break;
    case _CACHESIZE:	*vp = _GET_U(!db->get_cachesize(db, &_gb, &_u, &_i)); break;
    case _CREATE_DIR:	*vp = _GET_S(!db->get_create_dir(db, &_s)); break;
    case _ENCRYPT:	*vp = _GET_U(!db->get_encrypt_flags(db, &_u));	break;
    case _ERRFILE:
	db->get_errfile(db, &_fp);
	_s = (_fp ? "other" : NULL);
	if (_fp == stdin)	_s = "stdin";
	if (_fp == stdout)	_s = "stdout";
	if (_fp == stderr)	_s = "stderr";
	*vp = _RET_S(_s);
	break;
    case _ERRPFX:
	db->get_errpfx(db, &_s);
	*vp = _RET_S(_s);
	break;
    case _FLAGS:	*vp = _GET_U(!db->get_flags(db, &_u));		break;
    case _H_FFACTOR:	*vp = _GET_U(!db->get_h_ffactor(db, &_u));	break;
    case _H_NELEM:	*vp = _GET_U(!db->get_h_nelem(db, &_u));	break;
    case _LORDER:	*vp = _GET_I(!db->get_lorder(db, &_i));		break;
    case _MSGFILE:
	db->get_msgfile(db, &_fp);
	_s = (_fp ? "other" : NULL);
	if (_fp == stdin)	_s = "stdin";
	if (_fp == stdout)	_s = "stdout";
	if (_fp == stderr)	_s = "stderr";
	*vp = _RET_S(_s);
	break;
    case _PAGESIZE:	*vp = _GET_U(!db->get_pagesize(db, &_u));	break;
    case _PARTITION_DIRS:	break;
	if (!db->get_partition_dirs(db, &_av)
	 && (_ac = argvCount(_av)) > 0)
	{
	    _s = _av[0];	/* XXX FIXME: return array */
	    *vp = _RET_S(_s);
	} else
	    *vp = JSVAL_VOID;
	break;
    case _PRIORITY:	*vp = _GET_U(!db->get_priority(db, &_u));	break;
    case _Q_EXTENTSIZE:	*vp = _GET_U(!db->get_q_extentsize(db, &_u));	break;
    case _RE_DELIM:	*vp = _GET_I(!db->get_re_delim(db, &_i));	break;
    case _RE_LEN:	*vp = _GET_U(!db->get_re_len(db, &_u));		break;
    case _RE_PAD:	*vp = _GET_I(!db->get_re_pad(db, &_i));		break;
    case _RE_SOURCE:	*vp = _GET_S(!db->get_re_source(db, &_s));	break;
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
rpmdb_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdbClass, NULL);
    DB * db = ptr;
    const char * _s = NULL;
    uint32_t _gb = 0;
    uint32_t _b = 0;
    uint32_t _u = 0;
    int _i = 0;
    int _nc = 0;
    FILE * _fp = NULL;
    jsint tiny = JSVAL_TO_INT(id);

    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    if (ptr == NULL)
	return JS_TRUE;

    if (JSVAL_IS_STRING(*vp))
	_s = JS_GetStringBytes(JS_ValueToString(cx, *vp));
    if (JSVAL_IS_INT(*vp))
	_u = _i = JSVAL_TO_INT(*vp);

    switch (tiny) {
    case _DEBUG:
	if (!JS_ValueToInt32(cx, *vp, &_debug))
	    break;
	break;

#ifdef	NOTYET
    case _DBFILE:	break;
    case _DBNAME:	break;
#endif

    case _BT_MINKEY:	*vp = _PUT_U(db->set_bt_minkey(db, _u));	break;
    case _CACHESIZE:
	if (!db->get_cachesize(db, &_gb, &_b, &_nc)) {
	    _b = _u;
	    *vp = !db->set_cachesize(db, _gb, _b, _nc)
		? JSVAL_TRUE : JSVAL_FALSE;
	} else
	    *vp = JSVAL_FALSE;
	break;
    case _CREATE_DIR:	*vp = _PUT_S(db->set_create_dir(db, _s));	break;
    case _ENCRYPT:	*vp = _PUT_S(db->set_encrypt(db, _s, DB_ENCRYPT_AES)); break;
    case _ERRFILE:
	/* XXX FIXME: cleaner typing */
	_fp = NULL;
	if (!strcmp(_s, "stdout")) _fp = stdout;
	if (!strcmp(_s, "stderr")) _fp = stderr;
	db->set_errfile(db, _fp);
	*vp = JSVAL_TRUE;
	break;
    case _ERRPFX:
	db->set_errpfx(db, _s);
	*vp = JSVAL_TRUE;
	break;
    case _FLAGS:	break;	/* XXX FIXME */
    case _H_FFACTOR:	*vp = _PUT_U(db->set_h_ffactor(db, _u));	break;
    case _H_NELEM:	*vp = _PUT_U(db->set_h_nelem(db, _u));		break;
    case _LORDER:	*vp = _PUT_I(db->set_h_nelem(db, _i));		break;
    case _MSGFILE:
	/* XXX FIXME: cleaner typing */
	_fp = NULL;
	if (_s != NULL) {
	    if (!strcmp(_s, "stdout")) _fp = stdout;
	    else if (!strcmp(_s, "stderr")) _fp = stderr;
	    /* XXX FIXME: fp = fopen(_s, O_RDWR); with error checking */
	}
	db->set_errfile(db, _fp);
	*vp = JSVAL_TRUE;
	break;
    case _PAGESIZE:	*vp = _PUT_U(db->set_pagesize(db, _u));		break;
    case _PARTITION_DIRS:	break;	/* XXX FIXME */
    case _PRIORITY:	*vp = _PUT_U(db->set_priority(db, _u));		break;
    case _Q_EXTENTSIZE:	*vp = _PUT_U(db->set_q_extentsize(db, _u));	break;
    case _RE_DELIM:	*vp = _PUT_I(db->set_re_delim(db, _i));		break;
    case _RE_LEN:	*vp = _PUT_U(db->set_re_len(db, _u));	break;
    case _RE_PAD:	*vp = _PUT_I(db->set_re_pad(db, _i));		break;
    case _RE_SOURCE:	*vp = _PUT_S(db->set_re_source(db, _s));	break;

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
static DB *
rpmdb_init(JSContext *cx, JSObject *obj, DB_ENV * dbenv, uint32_t flags)
{
    DB * db = NULL;

    if (rpmdb_create(&db, dbenv, flags) || db == NULL
     || !JS_SetPrivate(cx, obj, (void *)db))
    {
	uint32_t _flags = 0;
	if (db)
	    (void) db->close(db, _flags);
	/* XXX error msg */
	db = NULL;
    }

if (_debug)
fprintf(stderr, "<== %s(%p,%p) db %p\n", __FUNCTION__, cx, obj, db);
    return db;
}

static void
rpmdb_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdbClass, NULL);
    DB * db = ptr;
    uint32_t _flags = 0;

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);
    if (db)
	(void) db->close(db, _flags);
}

static JSBool
rpmdb_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    DB_ENV * _dbenv = NULL;
    uint32_t _flags = 0;
    JSBool ok = JS_FALSE;

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)%s\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ((cx->fp->flags & JSFRAME_CONSTRUCTING) ? " constructing" : ""));

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	(void) rpmdb_init(cx, obj, _dbenv, _flags);
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
    DB * db = ptr;
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
    DB_ENV * _dbenv = NULL;
    uint32_t _flags = 0;
    DB * db;

    if ((obj = JS_NewObject(cx, &rpmdbClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((db = rpmdb_init(cx, obj, _dbenv, _flags)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

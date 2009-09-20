/** \ingroup js_c
 * \file js/rpmdb-js.c
 */

#include "system.h"

#include "rpmdb-js.h"
#include "rpmjs-debug.h"

#define	_RPMDB_INTERNAL
#include <rpmdb.h>

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

/* --- Object methods */

static JSFunctionSpec rpmdb_funcs[] = {
    JS_FS_END
};

/* --- Object properties */
enum rpmdb_tinyid {
    _DEBUG	= -2,
#ifdef	DYING
    _CURRENT	= -3,
    _PID	= -4,
    _PPID	= -5,
    _PREV	= -6,
    _EXEC	= -7,
    _FSCREATE	= -8,
    _KEYCREATE	= -9,
    _SOCKCREATE	= -10,
    _ENFORCE	= -11,
    _DENY	= -12,
    _POLICYVERS	= -13,
    _ENABLED	= -14,
    _MLSENABLED	= -15,
    _BOOLS	= -16,		/* todo++ */

    _ROOT	= -30,
    _BINARY	= -31,
    _FAILSAFE	= -32,
    _REMOVABLE	= -33,
    _DEFAULT	= -34,
    _USER	= -35,
    _FCON	= -36,
    _FCONHOME	= -37,
    _FCONLOCAL	= -38,
    _FCONSUBS	= -39,
    _HOMEDIR	= -40,
    _MEDIA	= -41,
    _VIRTDOMAIN	= -42,
    _VIRTIMAGE	= -43,
    _X		= -44,
    _CONTEXTS	= -45,
    _SECURETTY	= -46,
    _BOOLEANS	= -47,
    _CUSTOMTYPES= -48,
    _USERS	= -49,
    _USERSCONF	= -50,
    _XLATIONS	= -51,
    _COLORS	= -52,
    _NETFILTER	= -53,
    _PATH	= -54,
#endif	/* DYING */
};

static JSPropertySpec rpmdb_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
#ifdef	DYING
    {"current",	_CURRENT,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"pid",	_PID,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"ppid",	_PPID,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"prev",	_PREV,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"exec",	_EXEC,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"fscreate",_FSCREATE,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"keycreate",_KEYCREATE,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"sockcreate",_SOCKCREATE,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"enforce",	_ENFORCE,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"deny",	_DENY,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"policyvers",_POLICYVERS,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"enabled",	_ENABLED,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"mlsenabled",_MLSENABLED,	JSPROP_ENUMERATE,	NULL,	NULL},
#ifdef	NOTYET
    {"bools",	_BOOLS,		JSPROP_ENUMERATE,	NULL,	NULL},
#endif

    {"root",	_ROOT,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"binary",	_BINARY,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"failsafe",_FAILSAFE,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"removable",_REMOVABLE,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"default",	_DEFAULT,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"user",	_USER,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"fcon",	_FCON,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"fconhome",_FCONHOME,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"fconlocal",_FCONLOCAL,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"fconsubs",_FCONSUBS,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"homedir",	_HOMEDIR,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"media",	_MEDIA,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"virtdomain",_VIRTDOMAIN,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"virtimage",_VIRTIMAGE,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"X",	_X,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"contexts",_CONTEXTS,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"securetty",_SECURETTY,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"booleans",_BOOLEANS,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"customtypes",_CUSTOMTYPES,JSPROP_ENUMERATE,	NULL,	NULL},
    {"users",	_USERS,		JSPROP_ENUMERATE,	NULL,	NULL},
    {"usersconf",_USERSCONF,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"xlations",_XLATIONS,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"colors",	_COLORS,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"netfilter",_NETFILTER,	JSPROP_ENUMERATE,	NULL,	NULL},
    {"path",	_PATH,	JSPROP_ENUMERATE,	NULL,	NULL},
#endif	/* DYING */
    {NULL, 0, 0, NULL, NULL}
};

#define	_GET_STR(_str)	\
    ((_str) ? STRING_TO_JSVAL(JS_NewStringCopyZ(cx, (_str))) : JSVAL_VOID)
#define	_GET_CON(_test)	((_test) ? _GET_STR(con) : JSVAL_VOID)

static JSBool
rpmdb_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdbClass, NULL);
    jsint tiny = JSVAL_TO_INT(id);
#ifdef	DYING
    security_context_t con = NULL;
#endif

    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    if (ptr == NULL)
	return JS_TRUE;

    switch (tiny) {
    case _DEBUG:
	*vp = INT_TO_JSVAL(_debug);
	break;
#ifdef	DYING
    case _CURRENT:	*vp = _GET_CON(!getcon(&con));			break;
    case _PID:		*vp = _GET_CON(!getpidcon(getpid(), &con));	break;
    case _PPID:		*vp = _GET_CON(!getpidcon(getppid(), &con));	break;
    case _PREV:		*vp = _GET_CON(!getprevcon(&con));		break;
    case _EXEC:		*vp = _GET_CON(!getexeccon(&con));		break;
    case _FSCREATE:	*vp = _GET_CON(!getfscreatecon(&con));		break;
    case _KEYCREATE:	*vp = _GET_CON(!getkeycreatecon(&con));		break;
    case _SOCKCREATE:	*vp = _GET_CON(!getsockcreatecon(&con));	break;
    case _ENFORCE:	*vp = INT_TO_JSVAL(security_getenforce());	break;
    case _DENY:		*vp = INT_TO_JSVAL(security_deny_unknown());	break;
    case _POLICYVERS:	*vp = INT_TO_JSVAL(security_policyvers());	break;
    case _ENABLED:	*vp = INT_TO_JSVAL(is_selinux_enabled());	break;
    case _MLSENABLED:	*vp = INT_TO_JSVAL(is_selinux_mls_enabled());	break;
#ifdef	NOTYET
    case _BOOLS:	*vp = ;	break;
#endif
    case _ROOT:		*vp = _GET_STR(selinux_policy_root());		break;
    case _BINARY:	*vp = _GET_STR(selinux_binary_policy_path());	break;
    case _FAILSAFE:	*vp = _GET_STR(selinux_failsafe_context_path());break;
    case _REMOVABLE:	*vp = _GET_STR(selinux_removable_context_path());break;
    case _DEFAULT:	*vp = _GET_STR(selinux_default_context_path());	break;
    case _USER:		*vp = _GET_STR(selinux_user_contexts_path());	break;
    case _FCON:		*vp = _GET_STR(selinux_file_context_path());	break;
    case _FCONHOME:	*vp = _GET_STR(selinux_file_context_homedir_path());break;
    case _FCONLOCAL:	*vp = _GET_STR(selinux_file_context_local_path());break;
    case _FCONSUBS:	*vp = _GET_STR(selinux_file_context_subs_path());break;
    case _HOMEDIR:	*vp = _GET_STR(selinux_homedir_context_path());	break;
    case _MEDIA:	*vp = _GET_STR(selinux_media_context_path());	break;
    case _VIRTDOMAIN:	*vp = _GET_STR(selinux_virtual_domain_context_path());break;
    case _VIRTIMAGE:	*vp = _GET_STR(selinux_virtual_image_context_path());break;
    case _X:		*vp = _GET_STR(selinux_x_context_path());	break;
    case _CONTEXTS:	*vp = _GET_STR(selinux_contexts_path());	break;
    case _SECURETTY:	*vp = _GET_STR(selinux_securetty_types_path());	break;
    case _BOOLEANS:	*vp = _GET_STR(selinux_booleans_path());	break;
    case _CUSTOMTYPES:	*vp = _GET_STR(selinux_customizable_types_path());break;
    case _USERS:	*vp = _GET_STR(selinux_users_path());		break;
    case _USERSCONF:	*vp = _GET_STR(selinux_usersconf_path());	break;
    case _XLATIONS:	*vp = _GET_STR(selinux_translations_path());	break;
    case _COLORS:	*vp = _GET_STR(selinux_colors_path());		break;
    case _NETFILTER:	*vp = _GET_STR(selinux_netfilter_context_path());break;
    case _PATH:		*vp = _GET_STR(selinux_path());			break;
#endif	/* DYING */
    default:
	break;
    }

#ifdef	DYING
    if (con) {
	freecon(con);
	con = NULL;
    }
#endif

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
#ifdef	DYING
    security_context_t con = NULL;
    int myint = 0xdeadbeef;
#endif
    JSBool ok = JS_TRUE;

    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    if (ptr == NULL)
	return JS_TRUE;

#ifdef	DYING
    if (JSVAL_IS_STRING(*vp))
	con = (security_context_t) JS_GetStringBytes(JS_ValueToString(cx, *vp));
    if (JSVAL_IS_INT(*vp))
	myint = JSVAL_TO_INT(*vp);
#endif	/* DYING */

    switch (tiny) {
    case _DEBUG:
	if (!JS_ValueToInt32(cx, *vp, &_debug))
	    break;
	break;
#ifdef	DYING
    case _CURRENT:	ok = _PUT_CON(setcon(con));			break;
    case _EXEC:		ok = _PUT_CON(setexeccon(con));			break;
    case _FSCREATE:	ok = _PUT_CON(setfscreatecon(con));		break;
    case _KEYCREATE:	ok = _PUT_CON(setkeycreatecon(con));		break;
    case _SOCKCREATE:	ok = _PUT_CON(setsockcreatecon(con));		break;
    case _ENFORCE:	ok = _PUT_INT(security_setenforce(myint));	break;
#endif	/* DYING */
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
static rpmdb
rpmdb_init(JSContext *cx, JSObject *obj)
{
    const char * _root = "/";
    const char * _home = "/var/lib/rpm";
    int _mode = O_RDONLY;
    int _perms = 0644;
    int _flags = 0;
    rpmdb db = rpmdbNew(_root, _home, _mode, _perms, _flags);

if (_debug)
fprintf(stderr, "==> %s(%p,%p) db %p\n", __FUNCTION__, cx, obj, db);

    if (!JS_SetPrivate(cx, obj, (void *)db)) {
	/* XXX error msg */
	return NULL;
    }
    return db;
}

static void
rpmdb_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmdbClass, NULL);
    rpmdb db = ptr;

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);
#ifdef	NOTYET
    db = rpmdbFree(db);
#else
    (void) rpmdbClose(db);
#endif
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
    rpmdb db = ptr;
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
#endif

exit:
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
    rpmdb db;

    if ((obj = JS_NewObject(cx, &rpmdbClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((db = rpmdb_init(cx, obj)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

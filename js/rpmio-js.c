/** \ingroup js_c
 * \file js/rpmio-js.c
 */

#include "system.h"

#include "rpmio-js.h"
#include "rpmst-js.h"
#include "rpmjs-debug.h"
#include <rpmio.h>

#include "debug.h"

/*@unchecked@*/
static int _debug = 0;

/* Required JSClass vectors */
#define	rpmio_addprop		JS_PropertyStub
#define	rpmio_delprop		JS_PropertyStub
#define	rpmio_convert		JS_ConvertStub

/* Optional JSClass vectors */
#define	rpmio_getobjectops	NULL
#define	rpmio_checkaccess	NULL
#define	rpmio_call		rpmio_call
#define	rpmio_construct	rpmio_ctor
#define	rpmio_xdrobject	NULL
#define	rpmio_hasinstance	NULL
#define	rpmio_mark		NULL
#define	rpmio_reserveslots	NULL

/* Extended JSClass vectors */
#define rpmio_equality		NULL
#define rpmio_outerobject	NULL
#define rpmio_innerobject	NULL
#define rpmio_iteratorobject	NULL
#define rpmio_wrappedobject	NULL

/* --- helpers */
static FD_t
rpmio_init(JSContext *cx, JSObject *obj, const char * _fn, const char * _fmode)
{
    FD_t fd = NULL;

    if (_fn) {
	fd = Fopen(_fn, _fmode);
	/* XXX error msg */
    }
    if (!JS_SetPrivate(cx, obj, (void *)fd)) {
	/* XXX error msg */
	if (fd) {
	    (void) Fclose(fd);
	    /* XXX error msg */
	}
	fd = NULL;
    }

if (_debug)
fprintf(stderr, "<== %s(%p,%p) Fopen(%s,%s) fd %p\n", __FUNCTION__, cx, obj, _fn, _fmode, fd);

    return fd;
}

/* --- Object methods */
static JSBool
rpmio_fchown(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmioClass, NULL);
    FD_t fd = ptr;
    jsint _uid = -1;
    jsint _gid = -1;
    JSBool ok;

_METHOD_DEBUG_ENTRY(_debug);

    if ((ok = JS_ConvertArguments(cx, argc, argv, "/ii", &_uid, &_gid))) {
        uid_t uid = _uid;
        uid_t gid = _gid;
        *rval = (fd && !Fchown(fd, uid, gid)
                ? JSVAL_ZERO : INT_TO_JSVAL(errno));
    }
    return ok;
}

static JSBool
rpmio_fclose(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmioClass, NULL);
    FD_t fd = ptr;
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    /* XXX FIXME: _fmode is not persistent across io.fclose() */
    if (fd) {
	(void) Fclose(fd);
	/* XXX error msg */
	fd = ptr = NULL;
	(void) JS_SetPrivate(cx, obj, (void *)fd);
    }
    *rval = OBJECT_TO_JSVAL(obj);

    ok = JS_TRUE;
    return ok;
}

static JSBool
rpmio_ferror(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmioClass, NULL);
    FD_t fd = ptr;

_METHOD_DEBUG_ENTRY(_debug);
    *rval = (fd && !Ferror(fd) ? JSVAL_TRUE : JSVAL_FALSE);
    return JS_TRUE;
}

static JSBool
rpmio_fflush(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmioClass, NULL);
    FD_t fd = ptr;

_METHOD_DEBUG_ENTRY(_debug);
    *rval = (fd && !Fflush(fd) ? JSVAL_TRUE : JSVAL_FALSE);
    return JS_TRUE;
}

static JSBool
rpmio_fileno(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmioClass, NULL);
    FD_t fd = ptr;
    int fdno;

_METHOD_DEBUG_ENTRY(_debug);
    *rval = (fd && (fdno = Fileno(fd)) >= 0
		? INT_TO_JSVAL(fdno) : INT_TO_JSVAL(-1));
    return JS_TRUE;
}

static JSBool
rpmio_fopen(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmioClass, NULL);
    FD_t fd = ptr;
    const char * _fn = NULL;
    const char * _fmode = "r.ufdio";
    JSBool ok = JS_FALSE;

_METHOD_DEBUG_ENTRY(_debug);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "s/u", &_fn, &_fmode)))
        goto exit;

    if (fd) {
	(void) Fclose(fd);
	/* XXX error msg */
	fd = ptr = NULL;
	(void) JS_SetPrivate(cx, obj, (void *)fd);
    }

    fd = ptr = rpmio_init(cx, obj, _fn, _fmode);

    *rval = OBJECT_TO_JSVAL(obj);

    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmio_fread(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmioClass, NULL);
    FD_t fd = ptr;
    JSBool ok = JS_FALSE;
    static jsuint _nbmax = 16 * BUFSIZ;
    jsuint _nb = BUFSIZ;

_METHOD_DEBUG_ENTRY(_debug);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "/u", &_nb)))
        goto exit;

    if (_nb > _nbmax) _nb = _nbmax;

    if (fd) {
	size_t nb = _nb;
	char * b = alloca(nb);
	size_t nr = Fread(b, 1, nb, fd);
	if (nr == 0)
	    *rval = JSVAL_VOID;		/* XXX goofy? */
	else
	if (Ferror(fd))
	    *rval = JSVAL_FALSE;	/* XXX goofy? */
	else {
	    b[nr] = '\0';
	    *rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, b));
	}
    } else
	*rval = JSVAL_VOID;

    ok = JS_TRUE;
exit:
    return ok;
}

static JSBool
rpmio_fstat(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmioClass, NULL);
    FD_t fd = ptr;
    JSBool ok = JS_FALSE;
    struct stat sb;

_METHOD_DEBUG_ENTRY(_debug);

    if (fd && !Fstat(fd, &sb)) {
	JSObject *o;
	struct stat *st = NULL;
	size_t nb = sizeof(*st);
	if ((st = memcpy(xmalloc(nb), &sb, nb)) != NULL
	 && (o = JS_NewObject(cx, &rpmstClass, NULL, NULL)) != NULL
	 && JS_SetPrivate(cx, o, (void *)st))
	    *rval = OBJECT_TO_JSVAL(o);
	else {
	    st = _free(st);
	    *rval = JSVAL_VOID;		/* XXX goofy? */
	}
    } else
	*rval = JSVAL_VOID;		/* XXX goofy? */

    ok = JS_TRUE;
    return ok;
}

static JSBool
rpmio_fwrite(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmioClass, NULL);
    FD_t fd = ptr;
    JSBool ok = JS_FALSE;
    const char * b = NULL;
    size_t nb;

_METHOD_DEBUG_ENTRY(_debug);

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "s", &b)))
        goto exit;
assert(b != NULL);
    nb = strlen(b);

    if (fd) {
	size_t nw = Fwrite(b, 1, nb, fd);
	if (nw == 0)
	    *rval = JSVAL_VOID;		/* XXX goofy? */
	else
	    *rval = (nw != nb || Ferror(fd) ? JSVAL_FALSE : JSVAL_TRUE);
    } else
	*rval = JSVAL_FALSE;

    ok = JS_TRUE;
exit:
    return ok;
}

static JSFunctionSpec rpmio_funcs[] = {
    JS_FS("fchown",	rpmio_fchown,		0,0,0),
    JS_FS("fclose",	rpmio_fclose,		0,0,0),
    JS_FS("ferror",	rpmio_ferror,		0,0,0),
    JS_FS("fileno",	rpmio_fileno,		0,0,0),
    JS_FS("fflush",	rpmio_fflush,		0,0,0),
    JS_FS("fopen",	rpmio_fopen,		0,0,0),
    JS_FS("fread",	rpmio_fread,		0,0,0),
    JS_FS("fstat",	rpmio_fstat,		0,0,0),
    JS_FS("fwrite",	rpmio_fwrite,		0,0,0),
    JS_FS_END
};

/* --- Object properties */
enum rpmio_tinyid {
    _DEBUG	= -2,
};

static JSPropertySpec rpmio_props[] = {
    {"debug",	_DEBUG,		JSPROP_ENUMERATE,	NULL,	NULL},
    {NULL, 0, 0, NULL, NULL}
};

static JSBool
rpmio_getprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmioClass, NULL);
    jsint tiny = JSVAL_TO_INT(id);

_PROP_DEBUG_ENTRY(_debug < 0);

    /* XXX the class has ptr == NULL, instances have ptr != NULL. */
    if (ptr == NULL)
	return JS_TRUE;

    switch (tiny) {
    case _DEBUG:	*vp = INT_TO_JSVAL(_debug);		break;
    default:
	break;
    }

    return JS_TRUE;
}

static JSBool
rpmio_setprop(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmioClass, NULL);
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

static JSBool
rpmio_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
		JSObject **objp)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmioClass, NULL);

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
rpmio_enumerate(JSContext *cx, JSObject *obj, JSIterateOp op,
		  jsval *statep, jsid *idp)
{
#ifdef	NOTYET
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmioClass, NULL);
    FD_t fd = ptr;
    unsigned int ix = 0;
#endif

    /* XXX VG: JS_Enumerate (jsobj.c:4211) doesn't initialize some fields. */
_ENUMERATE_DEBUG_ENTRY(_debug < 0);

#ifdef	NOTYET
    switch (op) {
    case JSENUMERATE_INIT:
	if (idp)
	    *idp = JSVAL_ZERO;
	*statep = INT_TO_JSVAL(ix);
if (_debug)
fprintf(stderr, "\tINIT fd %p\n", fd);
        break;
    case JSENUMERATE_NEXT:
	ix = JSVAL_TO_INT(*statep);
	if ((dp = Readdir(dir)) != NULL) {
	    (void) JS_DefineElement(cx, obj,
			ix, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, dp->d_name)),
			NULL, NULL, JSPROP_ENUMERATE);
	    JS_ValueToId(cx, *statep, idp);
if (_debug)
fprintf(stderr, "\tNEXT fd %p[%u] dirent %p \"%s\"\n", fd, ix, dp, dp->d_name);
	    *statep = INT_TO_JSVAL(ix+1);
	} else
	    *idp = JSVAL_VOID;
        if (*idp != JSVAL_VOID)
            break;
        /*@fallthrough@*/
    case JSENUMERATE_DESTROY:
	ix = JSVAL_TO_INT(*statep);
	(void) JS_DefineProperty(cx, obj, "length", INT_TO_JSVAL(ix),
			NULL, NULL, JSPROP_ENUMERATE);
if (_debug)
fprintf(stderr, "\tFINI fd %p[%u]\n", fd, ix);
	*statep = JSVAL_NULL;
        break;
    }
#endif
    return JS_TRUE;
}

/* --- Object ctors/dtors */
static void
rpmio_dtor(JSContext *cx, JSObject *obj)
{
    void * ptr = JS_GetInstancePrivate(cx, obj, &rpmioClass, NULL);
    FD_t fd = ptr;

if (_debug)
fprintf(stderr, "==> %s(%p,%p) ptr %p\n", __FUNCTION__, cx, obj, ptr);
    if (fd) {
	(void) Fclose(fd);
	/* XXX error msg */
    }
}

static JSBool
rpmio_ctor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_FALSE;
    const char * _fn = NULL;
    const char * _fmode = "r.ufdio";

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p)%s\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, ((cx->fp->flags & JSFRAME_CONSTRUCTING) ? " constructing" : ""));

    if (!(ok = JS_ConvertArguments(cx, argc, argv, "/ss", &_fn, &_fmode)))
        goto exit;

    if (cx->fp->flags & JSFRAME_CONSTRUCTING) {
	(void) rpmio_init(cx, obj, _fn, _fmode);
    } else {
	if ((obj = JS_NewObject(cx, &rpmioClass, NULL, NULL)) == NULL)
	    goto exit;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    ok = JS_TRUE;

exit:
    return ok;
}

static JSBool
rpmio_call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    /* XXX obj is the global object so lookup "this" object. */
    JSObject * o = JSVAL_TO_OBJECT(argv[-2]);
    void * ptr = JS_GetInstancePrivate(cx, o, &rpmioClass, NULL);
    FD_t fd = ptr;
    JSBool ok = JS_FALSE;
    const char * _fn = NULL;
    const char * _fmode = "r.ufdio";

if (_debug)
fprintf(stderr, "==> %s(%p,%p,%p[%u],%p) o %p ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, o, ptr);
    if (!(ok = JS_ConvertArguments(cx, argc, argv, "/ss", &_fn, &_fmode)))
        goto exit;

    if (fd) {
	(void) Fclose(fd);
	/* XXX error msg */
	fd = ptr = NULL;
	(void) JS_SetPrivate(cx, o, (void *)fd);
    }

    fd = ptr = rpmio_init(cx, o, _fn, _fmode);

    *rval = OBJECT_TO_JSVAL(o);

    ok = JS_TRUE;

exit:
if (_debug)
fprintf(stderr, "<== %s(%p,%p,%p[%u],%p) o %p ptr %p\n", __FUNCTION__, cx, obj, argv, (unsigned)argc, rval, o, ptr);

    return ok;
}

/* --- Class initialization */
JSClass rpmioClass = {
    "Io", JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE | JSCLASS_HAS_PRIVATE,
    rpmio_addprop,   rpmio_delprop, rpmio_getprop, rpmio_setprop,
    (JSEnumerateOp)rpmio_enumerate, (JSResolveOp)rpmio_resolve,
    rpmio_convert,	rpmio_dtor,

    rpmio_getobjectops,	rpmio_checkaccess,
    rpmio_call,		rpmio_construct,
    rpmio_xdrobject,	rpmio_hasinstance,
    rpmio_mark,		rpmio_reserveslots,
};

JSObject *
rpmjs_InitIoClass(JSContext *cx, JSObject* obj)
{
    JSObject * proto = JS_InitClass(cx, obj, NULL, &rpmioClass, rpmio_ctor, 1,
		rpmio_props, rpmio_funcs, NULL, NULL);

if (_debug)
fprintf(stderr, "<== %s(%p,%p) proto %p\n", __FUNCTION__, cx, obj, proto);

assert(proto != NULL);
    return proto;
}

JSObject *
rpmjs_NewIoObject(JSContext *cx, const char * _fn, const char * _fmode)
{
    JSObject *obj;
    FD_t fd;

    if ((obj = JS_NewObject(cx, &rpmioClass, NULL, NULL)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    if ((fd = rpmio_init(cx, obj, _fn, _fmode)) == NULL) {
	/* XXX error msg */
	return NULL;
    }
    return obj;
}

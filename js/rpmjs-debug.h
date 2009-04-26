#ifndef H_RPMJS_DEBUG
#define H_RPMJS_DEBUG

/**
 * \file js/rpmjs-debug.h
 */
#include <rpm-js.h>

#define	OBJ_IS_STRING(_cx, _o)	(OBJ_GET_CLASS(_cx, _o) == &js_StringClass)
#define	OBJ_IS_RPMHDR(_cx, _o)	(OBJ_GET_CLASS(_cx, _o) == &rpmhdrClass)

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

#define	_PROP_DEBUG_ENTRY(_test)\
    if (_test) \
	fprintf(stderr, "==> %s(%p,%p,0x%lx[%s],%p) ptr %p %s = %s\n", \
	    __FUNCTION__, cx, obj, (unsigned long)id, v2s(cx, id), vp, ptr, \
	    JS_GetStringBytes(JS_ValueToString(cx, id)), \
	    JS_GetStringBytes(JS_ValueToString(cx, *vp)))

#define	_PROP_DEBUG_EXIT(_test)	\
    if (_test) { \
	fprintf(stderr, "==> %s(%p,%p,0x%lx[%s],%p) ptr %p %s = %s\n", \
	    __FUNCTION__, cx, obj, (unsigned long)id, v2s(cx, id), vp, ptr, \
	    JS_GetStringBytes(JS_ValueToString(cx, id)), \
	    JS_GetStringBytes(JS_ValueToString(cx, *vp))); \
	ok = JS_TRUE;		/* XXX return JS_TRUE iff ... ? */ \
    }

#define	_RESOLVE_DEBUG_ENTRY(_test) \
    if (_test) \
	fprintf(stderr, "==> %s(%p,%p,0x%lx[%s],0x%x,%p) ptr %p property %s flags 0x%x{%s,%s,%s,%s,%s}\n", \
	    __FUNCTION__, cx, obj, (unsigned long)id, v2s(cx, id), \
	    (unsigned)flags, objp, ptr, \
	    JS_GetStringBytes(JS_ValueToString(cx, id)), \
	    flags, \
		(flags & JSRESOLVE_QUALIFIED) ? "qualified" : "", \
		(flags & JSRESOLVE_ASSIGNING) ? "assigning" : "", \
		(flags & JSRESOLVE_DETECTING) ? "detecting" : "", \
		(flags & JSRESOLVE_DECLARING) ? "declaring" : "", \
		(flags & JSRESOLVE_CLASSNAME) ? "classname" : "")

#define	_ENUMERATE_DEBUG_ENTRY(_test) \
    if (_test) \
	fprintf(stderr, "==> %s(%p,%p,%d,%p,%p) *statep 0x%lx *idp 0x%lx\n", \
	    __FUNCTION__, cx, obj, op, statep, idp, \
	    (unsigned long)(statep ? *statep : 0xfeedface), \
	    (unsigned long)(idp ? *idp : 0xdeadbeef))

#define	_CONVERT_DEBUG_ENTRY(_test) \
    if (_test) \
	fprintf(stderr, "==> %s(%p,%p,%d,%p) ptr %p convert to %s\n", \
	    __FUNCTION__, cx, obj, type, vp, ptr, JS_GetTypeName(cx, type))

/*@unchecked@*/
extern int _rpmjs_debug;

#endif	/* H_RPMJS_DEBUG */

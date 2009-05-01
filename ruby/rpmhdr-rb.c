/** \ingroup rb_c
 * \file ruby/rpmhdr-rb.c
 */

#include "system.h"

#include "rpm-rb.h"
#include "rpmhdr-rb.h"

#ifdef	NOTYET
#include <argv.h>
#include <mire.h>
#endif

#include <rpmdb.h>

#include "../debug.h"

typedef	Header rpmhdr;

VALUE rpmhdrClass;

/*@unchecked@*/
static int _debug = -1;

/* --- helpers */
static void *
rpmhdr_ptr(VALUE s)
{
    void *ptr;
    Data_Get_Struct(s, void, ptr);
    return ptr;
}

/* --- Object methods */

static void
initMethods(VALUE klass)
{
}

/* --- Object properties */
static VALUE
rpmhdr_debug_get(VALUE s)
{
if (_debug)
fprintf(stderr, "==> %s(0x%lx)\n", __FUNCTION__, s);
    return INT2FIX(_debug);
}

static VALUE
rpmhdr_debug_set(VALUE s, VALUE v)
{
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx)\n", __FUNCTION__, s, v);
    return INT2FIX(_debug = FIX2INT(v));
}

#ifdef	NOTYET
static VALUE
rpmhdr_rootdir_get(VALUE s)
{
    void *ptr = rpmhdr_ptr(s);
    rpmhdr h = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    return rb_str_new2(rpmhdrRootDir(h));
}

static VALUE
rpmhdr_rootdir_set(VALUE s, VALUE v)
{
    void *ptr = rpmhdr_ptr(s);
    rpmhdr h = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx) ptr %p\n", __FUNCTION__, s, v, ptr);
    rpmhdrSetRootDir(h, StringValueCStr(v));
    return rb_str_new2(rpmhdrRootDir(h));
}

static VALUE
rpmhdr_vsflags_get(VALUE s)
{
    void *ptr = rpmhdr_ptr(s);
    rpmhdr h = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    return INT2FIX(_debug);
}

static VALUE
rpmhdr_vsflags_set(VALUE s, VALUE v)
{
    void *ptr = rpmhdr_ptr(s);
    rpmhdr h = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx) ptr %p\n", __FUNCTION__, s, v, ptr);
    rpmhdrSetVSFlags(h, FIX2INT(v));
    return INT2FIX(rpmhdrVSFlags(h));
}
#endif	/* NOTYET */

static void
initProperties(VALUE klass)
{
    rb_define_method(klass, "debug", rpmhdr_debug_get, 0);
    rb_define_method(klass, "debug=", rpmhdr_debug_set, 1);
#ifdef	NOTYET
    rb_define_method(klass, "rootdir", rpmhdr_rootdir_get, 0);
    rb_define_method(klass, "rootdir=", rpmhdr_rootdir_set, 1);
    rb_define_method(klass, "vsflags", rpmhdr_vsflags_get, 0);
    rb_define_method(klass, "vsflags=", rpmhdr_vsflags_set, 1);
#endif	/* NOTYET */
}

/* --- Object ctors/dtors */
static void
rpmhdr_free(rpmhdr h)
{
if (_debug)
fprintf(stderr, "==> %s(%p)\n", __FUNCTION__, h);

    h = headerFree(h);
}

static VALUE
rpmhdr_alloc(VALUE klass)
{
#ifdef	NOTYET
    rpmhdr h = NULL;
#else
    rpmhdr h = headerNew();
#endif	/* NOTYET */
    VALUE obj = Data_Wrap_Struct(klass, 0, rpmhdr_free, h);
if (_debug)
fprintf(stderr, "==> %s(0x%lx) obj 0x%lx h %p\n", __FUNCTION__, klass, obj, h);
    return obj;
}

/* --- Class initialization */

void
Init_rpmhdr(void)
{
    rpmhdrClass = rb_define_class("Hdr", rb_cObject);
if (_debug)
fprintf(stderr, "==> %s() rpmhdrClass 0x%lx\n", __FUNCTION__, rpmhdrClass);
    rb_define_alloc_func(rpmhdrClass, rpmhdr_alloc);
    initProperties(rpmhdrClass);
    initMethods(rpmhdrClass);
}

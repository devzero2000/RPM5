/** \ingroup rb_c
 * \file ruby/rpmfi-rb.c
 */

#include "system.h"

#include "rpm-rb.h"
#include "rpmfi-rb.h"

#ifdef	NOTYET
#include <argv.h>
#include <mire.h>
#endif

#include <rpmfi.h>

#include "../debug.h"

VALUE rpmfiClass;

/*@unchecked@*/
static int _debug = -1;

/* --- helpers */
static void *
rpmfi_ptr(VALUE s)
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
rpmfi_debug_get(VALUE s)
{
if (_debug)
fprintf(stderr, "==> %s(0x%lx)\n", __FUNCTION__, s);
    return INT2FIX(_debug);
}

static VALUE
rpmfi_debug_set(VALUE s, VALUE v)
{
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx)\n", __FUNCTION__, s, v);
    return INT2FIX(_debug = FIX2INT(v));
}

#ifdef	NOTYET
static VALUE
rpmfi_rootdir_get(VALUE s)
{
    void *ptr = rpmfi_ptr(s);
    rpmfi fi = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    return rb_str_new2(rpmfiRootDir(fi));
}

static VALUE
rpmfi_rootdir_set(VALUE s, VALUE v)
{
    void *ptr = rpmfi_ptr(s);
    rpmfi fi = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx) ptr %p\n", __FUNCTION__, s, v, ptr);
    rpmfiSetRootDir(fi, StringValueCStr(v));
    return rb_str_new2(rpmfiRootDir(fi));
}

static VALUE
rpmfi_vsflags_get(VALUE s)
{
    void *ptr = rpmfi_ptr(s);
    rpmfi fi = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    return INT2FIX(_debug);
}

static VALUE
rpmfi_vsflags_set(VALUE s, VALUE v)
{
    void *ptr = rpmfi_ptr(s);
    rpmfi fi = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx) ptr %p\n", __FUNCTION__, s, v, ptr);
    rpmfiSetVSFlags(fi, FIX2INT(v));
    return INT2FIX(rpmfiVSFlags(fi));
}
#endif	/* NOTYET */

static void
initProperties(VALUE klass)
{
    rb_define_method(klass, "debug", rpmfi_debug_get, 0);
    rb_define_method(klass, "debug=", rpmfi_debug_set, 1);
#ifdef	NOTYET
    rb_define_method(klass, "rootdir", rpmfi_rootdir_get, 0);
    rb_define_method(klass, "rootdir=", rpmfi_rootdir_set, 1);
    rb_define_method(klass, "vsflags", rpmfi_vsflags_get, 0);
    rb_define_method(klass, "vsflags=", rpmfi_vsflags_set, 1);
#endif	/* NOTYET */
}

/* --- Object ctors/dtors */
static void
rpmfi_free(rpmfi fi)
{
if (_debug)
fprintf(stderr, "==> %s(%p)\n", __FUNCTION__, fi);
#ifdef	NOTYET
    fi = rpmfiFree(fi);
#else
    fi = _free(fi);
#endif
}

static VALUE
rpmfi_alloc(VALUE klass)
{
#ifdef	NOTYET
    rpmfi fi = NULL;
#else
    rpmfi fi = xcalloc(1, sizeof(void *));
#endif
    VALUE obj = Data_Wrap_Struct(klass, 0, rpmfi_free, fi);
if (_debug)
fprintf(stderr, "==> %s(0x%lx) obj 0x%lx fi %p\n", __FUNCTION__, klass, obj, fi);
    return obj;
}

/* --- Class initialization */

void
Init_rpmfi(void)
{
    rpmfiClass = rb_define_class("Fi", rb_cObject);
if (_debug)
fprintf(stderr, "==> %s() rpmfiClass 0x%lx\n", __FUNCTION__, rpmfiClass);
    rb_define_alloc_func(rpmfiClass, rpmfi_alloc);
    initProperties(rpmfiClass);
    initMethods(rpmfiClass);
}

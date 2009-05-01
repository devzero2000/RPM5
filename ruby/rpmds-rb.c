/** \ingroup rb_c
 * \file ruby/rpmds-rb.c
 */

#include "system.h"

#include "rpm-rb.h"
#include "rpmds-rb.h"

#ifdef	NOTYET
#include <argv.h>
#include <mire.h>
#endif

#include <rpmds.h>

#include "../debug.h"

VALUE rpmdsClass;

/*@unchecked@*/
static int _debug = -1;

/* --- helpers */
static void *
rpmds_ptr(VALUE s)
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
rpmds_debug_get(VALUE s)
{
if (_debug)
fprintf(stderr, "==> %s(0x%lx)\n", __FUNCTION__, s);
    return INT2FIX(_debug);
}

static VALUE
rpmds_debug_set(VALUE s, VALUE v)
{
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx)\n", __FUNCTION__, s, v);
    return INT2FIX(_debug = FIX2INT(v));
}

#ifdef	NOTYET
static VALUE
rpmds_rootdir_get(VALUE s)
{
    void *ptr = rpmds_ptr(s);
    rpmds ds = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    return rb_str_new2(rpmdsRootDir(ds));
}

static VALUE
rpmds_rootdir_set(VALUE s, VALUE v)
{
    void *ptr = rpmds_ptr(s);
    rpmds ds = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx) ptr %p\n", __FUNCTION__, s, v, ptr);
    rpmdsSetRootDir(ds, StringValueCStr(v));
    return rb_str_new2(rpmdsRootDir(ds));
}

static VALUE
rpmds_vsflags_get(VALUE s)
{
    void *ptr = rpmds_ptr(s);
    rpmds ds = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    return INT2FIX(_debug);
}

static VALUE
rpmds_vsflags_set(VALUE s, VALUE v)
{
    void *ptr = rpmds_ptr(s);
    rpmds ds = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx) ptr %p\n", __FUNCTION__, s, v, ptr);
    rpmdsSetVSFlags(ds, FIX2INT(v));
    return INT2FIX(rpmdsVSFlags(ds));
}
#endif	/* NOTYET */

static void
initProperties(VALUE klass)
{
    rb_define_method(klass, "debug", rpmds_debug_get, 0);
    rb_define_method(klass, "debug=", rpmds_debug_set, 1);
#ifdef	NOTYET
    rb_define_method(klass, "rootdir", rpmds_rootdir_get, 0);
    rb_define_method(klass, "rootdir=", rpmds_rootdir_set, 1);
    rb_define_method(klass, "vsflags", rpmds_vsflags_get, 0);
    rb_define_method(klass, "vsflags=", rpmds_vsflags_set, 1);
#endif	/* NOTYET */
}

/* --- Object ctors/dtors */
static void
rpmds_free(rpmds ds)
{
if (_debug)
fprintf(stderr, "==> %s(%p)\n", __FUNCTION__, ds);
    ds = rpmdsFree(ds);
}

static VALUE
rpmds_alloc(VALUE klass)
{
    rpmds ds = NULL;
    int xx = rpmdsRpmlib(&ds, NULL);
    VALUE obj = Data_Wrap_Struct(klass, 0, rpmds_free, ds);
if (_debug)
fprintf(stderr, "==> %s(0x%lx) obj 0x%lx ds %p\n", __FUNCTION__, klass, obj, ds);
    return obj;
}

/* --- Class initialization */

void
Init_rpmds(void)
{
    rpmdsClass = rb_define_class("Ds", rb_cObject);
if (_debug)
fprintf(stderr, "==> %s() rpmdsClass 0x%lx\n", __FUNCTION__, rpmdsClass);
    rb_define_alloc_func(rpmdsClass, rpmds_alloc);
    initProperties(rpmdsClass);
    initMethods(rpmdsClass);
}

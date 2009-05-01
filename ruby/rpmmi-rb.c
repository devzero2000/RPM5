/** \ingroup rb_c
 * \file ruby/rpmmi-rb.c
 */

#include "system.h"

#include "rpm-rb.h"
#include "rpmmi-rb.h"

#ifdef	NOTYET
#include <argv.h>
#include <mire.h>
#endif

#include <rpmdb.h>

#include "../debug.h"

typedef	rpmdbMatchIterator rpmmi;

VALUE rpmmiClass;

/*@unchecked@*/
static int _debug = -1;

/* --- helpers */
static void *
rpmmi_ptr(VALUE s)
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
rpmmi_debug_get(VALUE s)
{
if (_debug)
fprintf(stderr, "==> %s(0x%lx)\n", __FUNCTION__, s);
    return INT2FIX(_debug);
}

static VALUE
rpmmi_debug_set(VALUE s, VALUE v)
{
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx)\n", __FUNCTION__, s, v);
    return INT2FIX(_debug = FIX2INT(v));
}

#ifdef	NOTYET
static VALUE
rpmmi_rootdir_get(VALUE s)
{
    void *ptr = rpmmi_ptr(s);
    rpmmi mi = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    return rb_str_new2(rpmmiRootDir(mi));
}

static VALUE
rpmmi_rootdir_set(VALUE s, VALUE v)
{
    void *ptr = rpmmi_ptr(s);
    rpmmi mi = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx) ptr %p\n", __FUNCTION__, s, v, ptr);
    rpmmiSetRootDir(mi, StringValueCStr(v));
    return rb_str_new2(rpmmiRootDir(mi));
}

static VALUE
rpmmi_vsflags_get(VALUE s)
{
    void *ptr = rpmmi_ptr(s);
    rpmmi mi = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    return INT2FIX(_debug);
}

static VALUE
rpmmi_vsflags_set(VALUE s, VALUE v)
{
    void *ptr = rpmmi_ptr(s);
    rpmmi mi = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx) ptr %p\n", __FUNCTION__, s, v, ptr);
    rpmmiSetVSFlags(mi, FIX2INT(v));
    return INT2FIX(rpmmiVSFlags(mi));
}
#endif	/* NOTYET */

static void
initProperties(VALUE klass)
{
    rb_define_method(klass, "debug", rpmmi_debug_get, 0);
    rb_define_method(klass, "debug=", rpmmi_debug_set, 1);
#ifdef	NOTYET
    rb_define_method(klass, "rootdir", rpmmi_rootdir_get, 0);
    rb_define_method(klass, "rootdir=", rpmmi_rootdir_set, 1);
    rb_define_method(klass, "vsflags", rpmmi_vsflags_get, 0);
    rb_define_method(klass, "vsflags=", rpmmi_vsflags_set, 1);
#endif	/* NOTYET */
}

/* --- Object ctors/dtors */
static void
rpmmi_free(rpmmi mi)
{
if (_debug)
fprintf(stderr, "==> %s(%p)\n", __FUNCTION__, mi);

#ifdef	BUGGY
    mi = rpmdbFreeIterator(mi);
#else
    mi = _free(mi);
#endif
}

static VALUE
rpmmi_alloc(VALUE klass)
{
#ifdef	NOTYET
    rpmmi mi = rpmtsInitIterator(ts, _tag, _key, _len);
#else
    rpmmi mi = xcalloc(1, sizeof(void *));
#endif	/* NOTYET */
    VALUE obj = Data_Wrap_Struct(klass, 0, rpmmi_free, mi);
if (_debug)
fprintf(stderr, "==> %s(0x%lx) obj 0x%lx mi %p\n", __FUNCTION__, klass, obj, mi);
    return obj;
}

/* --- Class initialization */

void
Init_rpmmi(void)
{
    rpmmiClass = rb_define_class("Mi", rb_cObject);
if (_debug)
fprintf(stderr, "==> %s() rpmmiClass 0x%lx\n", __FUNCTION__, rpmmiClass);
    rb_define_alloc_func(rpmmiClass, rpmmi_alloc);
    initProperties(rpmmiClass);
    initMethods(rpmmiClass);
}

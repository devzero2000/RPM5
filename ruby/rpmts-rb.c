/** \ingroup rb_c
 * \file ruby/rpmts-rb.c
 */

#include "system.h"

#include "rpm-rb.h"
#include "rpmts-rb.h"

#ifdef	NOTYET
#include <argv.h>
#include <mire.h>

#include <rpmdb.h>
#endif

#define	_RPMTS_INTERNAL
#include <rpmts.h>

#include "../debug.h"

VALUE rpmtsClass;

/*@unchecked@*/
static int _debug = -1;

/* --- helpers */
static void *
rpmts_ptr(VALUE s)
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
rpmts_debug_get(VALUE s)
{
if (_debug)
fprintf(stderr, "==> %s(0x%lx)\n", __FUNCTION__, s);
    return INT2FIX(_debug);
}

static VALUE
rpmts_debug_set(VALUE s, VALUE v)
{
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx)\n", __FUNCTION__, s, v);
    return INT2FIX(_debug = FIX2INT(v));
}

static VALUE
rpmts_rootdir_get(VALUE s)
{
    void *ptr = rpmts_ptr(s);
    rpmts ts = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    return rb_str_new2(rpmtsRootDir(ts));
}

static VALUE
rpmts_rootdir_set(VALUE s, VALUE v)
{
    void *ptr = rpmts_ptr(s);
    rpmts ts = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx) ptr %p\n", __FUNCTION__, s, v, ptr);
    rpmtsSetRootDir(ts, StringValueCStr(v));
    return rb_str_new2(rpmtsRootDir(ts));
}

static VALUE
rpmts_vsflags_get(VALUE s)
{
    void *ptr = rpmts_ptr(s);
    rpmts ts = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    return INT2FIX(_debug);
}

static VALUE
rpmts_vsflags_set(VALUE s, VALUE v)
{
    void *ptr = rpmts_ptr(s);
    rpmts ts = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx) ptr %p\n", __FUNCTION__, s, v, ptr);
    rpmtsSetVSFlags(ts, FIX2INT(v));
    return INT2FIX(rpmtsVSFlags(ts));
}

static void
initProperties(VALUE klass)
{
    rb_define_method(klass, "debug", rpmts_debug_get, 0);
    rb_define_method(klass, "debug=", rpmts_debug_set, 1);
    rb_define_method(klass, "rootdir", rpmts_rootdir_get, 0);
    rb_define_method(klass, "rootdir=", rpmts_rootdir_set, 1);
    rb_define_method(klass, "vsflags", rpmts_vsflags_get, 0);
    rb_define_method(klass, "vsflags=", rpmts_vsflags_set, 1);
}

/* --- Object ctors/dtors */
static void
rpmts_free(rpmts ts)
{
if (_debug)
fprintf(stderr, "==> %s(%p)\n", __FUNCTION__, ts);
    ts = rpmtsFree(ts);
}

static VALUE
rpmts_alloc(VALUE klass)
{
    rpmts ts = rpmtsCreate();
    VALUE obj = Data_Wrap_Struct(klass, 0, rpmts_free, ts);
if (_debug)
fprintf(stderr, "==> %s(0x%lx) obj 0x%lx ts %p\n", __FUNCTION__, klass, obj, ts);
    return obj;
}

/* --- Class initialization */

void
Init_rpmts(void)
{
    rpmtsClass = rb_define_class("Ts", rb_cObject);
if (_debug)
fprintf(stderr, "==> %s() rpmtsClass 0x%lx\n", __FUNCTION__, rpmtsClass);
    rb_define_alloc_func(rpmtsClass, rpmts_alloc);
    initProperties(rpmts_Class);
    initMethods(rpmts_Class);
}

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

/* --- Object methods */

/* --- Object properties */
static VALUE
rpmts_debug_get(VALUE self)
{
if (_debug)
fprintf(stderr, "==> %s(0x%lx)\n", __FUNCTION__, self);
    return INT2FIX(_debug);
}

static VALUE
rpmts_debug_set(VALUE self, VALUE v)
{
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx)\n", __FUNCTION__, self, v);
    return INT2FIX(_debug = FIX2INT(v));
}

static VALUE
rpmts_rootdir_get(VALUE self)
{
    void *ptr;
    rpmts ts;
    Data_Get_Struct(self, void, ptr);
    ts = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, self, ptr);
    return rb_str_new2(rpmtsRootDir(ts));
}

static VALUE
rpmts_rootdir_set(VALUE self, VALUE v)
{
    void *ptr;
    rpmts ts;
    Data_Get_Struct(self, void, ptr);
    ts = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx) ptr %p\n", __FUNCTION__, self, v, ptr);
    rpmtsSetRootDir(ts, StringValueCStr(v));
    return rb_str_new2(rpmtsRootDir(ts));
}

static VALUE
rpmts_vsflags_get(VALUE self)
{
    void *ptr;
    rpmts ts;
    Data_Get_Struct(self, void, ptr);
    ts = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, self, ptr);
    return INT2FIX(_debug);
}

static VALUE
rpmts_vsflags_set(VALUE self, VALUE v)
{
    void *ptr;
    rpmts ts;
    Data_Get_Struct(self, void, ptr);
    ts = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx) ptr %p\n", __FUNCTION__, self, v, ptr);
    rpmtsSetVSFlags(ts, FIX2INT(v));
    return INT2FIX(rpmtsVSFlags(ts));
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
    rb_define_method(rpmtsClass, "debug", rpmts_debug_get, 0);
    rb_define_method(rpmtsClass, "debug=", rpmts_debug_set, 1);
    rb_define_method(rpmtsClass, "rootdir", rpmts_rootdir_get, 0);
    rb_define_method(rpmtsClass, "rootdir=", rpmts_rootdir_set, 1);
    rb_define_method(rpmtsClass, "vsflags", rpmts_vsflags_get, 0);
    rb_define_method(rpmtsClass, "vsflags=", rpmts_vsflags_set, 1);
}

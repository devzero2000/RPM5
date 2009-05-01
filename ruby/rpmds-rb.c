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

static VALUE
rpmds_Count_get(VALUE s)
{
    void *ptr = rpmds_ptr(s);
    rpmds ds = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    return INT2FIX(rpmdsCount(ds));
}

/* XXX FIXME: what to call the method? */
static VALUE
rpmds_Type_get(VALUE s)
{
    void *ptr = rpmds_ptr(s);
    rpmds ds = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    return rb_str_new2(rpmdsType(ds));
}

static VALUE
rpmds_Ix_get(VALUE s)
{
    void *ptr = rpmds_ptr(s);
    rpmds ds = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    return INT2FIX(rpmdsIx(ds));
}

static VALUE
rpmds_Ix_set(VALUE s, VALUE v)
{
    void *ptr = rpmds_ptr(s);
    rpmds ds = ptr;
    int ix = FIX2INT(v);
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    if (ix != rpmdsIx(ds)) {
	(void) rpmdsSetIx(ds, ix-1);
	/* XXX flush and recreate N and DNEVR with a rpmdsNext() step */
        (void) rpmdsNext(ds);
    }
    return INT2FIX(rpmdsIx(ds));
}

static VALUE
rpmds_BT_get(VALUE s)
{
    void *ptr = rpmds_ptr(s);
    rpmds ds = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    return INT2FIX(rpmdsBT(ds));
}

static VALUE
rpmds_BT_set(VALUE s, VALUE v)
{
    void *ptr = rpmds_ptr(s);
    rpmds ds = ptr;
    int BT = FIX2INT(v);
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    (void) rpmdsSetBT(ds, BT);
    return INT2FIX(rpmdsBT(ds));
}

static VALUE
rpmds_Color_get(VALUE s)
{
    void *ptr = rpmds_ptr(s);
    rpmds ds = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    return INT2FIX(rpmdsColor(ds));
}

static VALUE
rpmds_Color_set(VALUE s, VALUE v)
{
    void *ptr = rpmds_ptr(s);
    rpmds ds = ptr;
    int color = FIX2INT(v);
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    (void) rpmdsSetColor(ds, color);
    return INT2FIX(rpmdsColor(ds));
}

static VALUE
rpmds_NoPromote_get(VALUE s)
{
    void *ptr = rpmds_ptr(s);
    rpmds ds = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    return INT2FIX(rpmdsNoPromote(ds));
}

static VALUE
rpmds_NoPromote_set(VALUE s, VALUE v)
{
    void *ptr = rpmds_ptr(s);
    rpmds ds = ptr;
    int nopromote = FIX2INT(v);
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    (void) rpmdsSetNoPromote(ds, nopromote);
    return INT2FIX(rpmdsNoPromote(ds));
}

static VALUE
rpmds_N_get(VALUE s)
{
    void *ptr = rpmds_ptr(s);
    rpmds ds = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    return rb_str_new2(rpmdsN(ds));
}

static VALUE
rpmds_EVR_get(VALUE s)
{
    void *ptr = rpmds_ptr(s);
    rpmds ds = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    return rb_str_new2(rpmdsEVR(ds));
}

static VALUE
rpmds_Flags_get(VALUE s)
{
    void *ptr = rpmds_ptr(s);
    rpmds ds = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    return INT2FIX(rpmdsFlags(ds));
}

static VALUE
rpmds_DNEVR_get(VALUE s)
{
    void *ptr = rpmds_ptr(s);
    rpmds ds = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    return rb_str_new2(rpmdsDNEVR(ds));
}

static void
initProperties(VALUE klass)
{
    rb_define_method(klass, "debug", rpmds_debug_get, 0);
    rb_define_method(klass, "debug=", rpmds_debug_set, 1);
    rb_define_method(klass, "length", rpmds_Count_get, 0);
    rb_define_method(klass, "ix", rpmds_Ix_get, 0);
    rb_define_method(klass, "ix=", rpmds_Ix_set, 1);
    rb_define_method(klass, "Type", rpmds_Type_get, 0);
    rb_define_method(klass, "buildtime", rpmds_BT_get, 0);
    rb_define_method(klass, "buildtime=", rpmds_BT_set, 1);
    rb_define_method(klass, "color", rpmds_Color_get, 0);
    rb_define_method(klass, "color=", rpmds_Color_set, 1);
    rb_define_method(klass, "nopromote", rpmds_NoPromote_get, 0);
    rb_define_method(klass, "nopromote=", rpmds_NoPromote_set, 1);
    rb_define_method(klass, "N", rpmds_N_get, 0);
    rb_define_method(klass, "EVR", rpmds_EVR_get, 0);
    rb_define_method(klass, "F", rpmds_Flags_get, 0);
    rb_define_method(klass, "DNEVR", rpmds_DNEVR_get, 0);
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

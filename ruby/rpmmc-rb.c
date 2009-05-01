/** \ingroup rb_c
 * \file ruby/rpmmc-rb.c
 */

#include "system.h"

#include "rpm-rb.h"
#include "rpmmc-rb.h"

#ifdef	NOTYET
#include <argv.h>
#include <mire.h>
#endif

#include <rpmmacro.h>

#include "../debug.h"

typedef MacroContext rpmmc;

VALUE rpmmcClass;

/*@unchecked@*/
static int _debug = -1;

/* --- helpers */
static void *
rpmmc_ptr(VALUE s)
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
rpmmc_debug_get(VALUE s)
{
if (_debug)
fprintf(stderr, "==> %s(0x%lx)\n", __FUNCTION__, s);
    return INT2FIX(_debug);
}

static VALUE
rpmmc_debug_set(VALUE s, VALUE v)
{
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx)\n", __FUNCTION__, s, v);
    return INT2FIX(_debug = FIX2INT(v));
}

#ifdef	NOTYET
static VALUE
rpmmc_rootdir_get(VALUE s)
{
    void *ptr = rpmmc_ptr(s);
    rpmmc mc = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    return rb_str_new2(rpmmcRootDir(mc));
}

static VALUE
rpmmc_rootdir_set(VALUE s, VALUE v)
{
    void *ptr = rpmmc_ptr(s);
    rpmmc mc = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx) ptr %p\n", __FUNCTION__, s, v, ptr);
    rpmmcSetRootDir(mc, StringValueCStr(v));
    return rb_str_new2(rpmmcRootDir(mc));
}

static VALUE
rpmmc_vsflags_get(VALUE s)
{
    void *ptr = rpmmc_ptr(s);
    rpmmc mc = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    return INT2FIX(_debug);
}

static VALUE
rpmmc_vsflags_set(VALUE s, VALUE v)
{
    void *ptr = rpmmc_ptr(s);
    rpmmc mc = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx) ptr %p\n", __FUNCTION__, s, v, ptr);
    rpmmcSetVSFlags(mc, FIX2INT(v));
    return INT2FIX(rpmmcVSFlags(mc));
}
#endif	/* NOTYET */

static void
initProperties(VALUE klass)
{
    rb_define_method(klass, "debug", rpmmc_debug_get, 0);
    rb_define_method(klass, "debug=", rpmmc_debug_set, 1);
#ifdef	NOTYET
    rb_define_method(klass, "rootdir", rpmmc_rootdir_get, 0);
    rb_define_method(klass, "rootdir=", rpmmc_rootdir_set, 1);
    rb_define_method(klass, "vsflags", rpmmc_vsflags_get, 0);
    rb_define_method(klass, "vsflags=", rpmmc_vsflags_set, 1);
#endif	/* NOTYET */
}

/* --- Object ctors/dtors */
static void
rpmmc_free(rpmmc mc)
{
if (_debug)
fprintf(stderr, "==> %s(%p)\n", __FUNCTION__, mc);
#ifdef	NOTYET
    mc = rpmmcFree(mc);
#else
    mc = _free(mc);
#endif
}

static VALUE
rpmmc_alloc(VALUE klass)
{
#ifdef	NOTYET
    rpmmc mc = NULL;
#else
    rpmmc mc = xcalloc(1, sizeof(void *));
#endif
    VALUE obj = Data_Wrap_Struct(klass, 0, rpmmc_free, mc);
if (_debug)
fprintf(stderr, "==> %s(0x%lx) obj 0x%lx mc %p\n", __FUNCTION__, klass, obj, mc);
    return obj;
}

/* --- Class initialization */

void
Init_rpmmc(void)
{
    rpmmcClass = rb_define_class("Mc", rb_cObject);
if (_debug)
fprintf(stderr, "==> %s() rpmmcClass 0x%lx\n", __FUNCTION__, rpmmcClass);
    rb_define_alloc_func(rpmmcClass, rpmmc_alloc);
    initProperties(rpmmcClass);
    initMethods(rpmmcClass);
}

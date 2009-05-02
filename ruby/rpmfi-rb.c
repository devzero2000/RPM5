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
#include <rpmts.h>

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

static void
initProperties(VALUE klass)
{
    rb_define_method(klass, "debug", rpmfi_debug_get, 0);
    rb_define_method(klass, "debug=", rpmfi_debug_set, 1);
}

/* --- Object ctors/dtors */
static void
rpmfi_free(rpmfi fi)
{
if (_debug)
fprintf(stderr, "==> %s(%p)\n", __FUNCTION__, fi);
    fi = rpmfiFree(fi);
}

static VALUE
rpmfi_new(int argc, VALUE *argv, VALUE s)
{
    VALUE v_ts, v_hdr, v_tag;
    rpmts ts;
    Header h;
    rpmTag tag = RPMTAG_BASENAMES;
    int flags = 0;
    rpmfi fi;

    rb_scan_args(argc, argv, "21", &v_ts, &v_hdr, &v_tag);

    ts = rpmfi_ptr(v_ts);
    h = rpmfi_ptr(v_hdr);

    if (!NIL_P(v_tag))
	tag = FIX2INT(v_tag);

    fi = rpmfiNew(ts, h, tag, flags);

if (_debug)
fprintf(stderr, "==> %s(%p[%d], 0x%lx) mi %p\n", __FUNCTION__, argv, argc, s, fi);
    return Data_Wrap_Struct(s, 0, rpmfi_free, fi);
}

/* --- Class initialization */

void
Init_rpmfi(void)
{
    rpmfiClass = rb_define_class("Fi", rb_cObject);
if (_debug)
fprintf(stderr, "==> %s() rpmfiClass 0x%lx\n", __FUNCTION__, rpmfiClass);
#ifdef  NOTYET
    rb_include_module(rpmfiClass, rb_mEnumerable);
#endif
    rb_define_singleton_method(rpmfiClass, "new", rpmfi_new, -1);
    initProperties(rpmfiClass);
    initMethods(rpmfiClass);
}

VALUE
rpmrb_NewFi(void *_fi)
{
    return Data_Wrap_Struct(rpmfiClass, 0, rpmfi_free, _fi);
}


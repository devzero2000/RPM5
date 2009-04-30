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
rpmts_debug(VALUE self)
{
if (_debug)
fprintf(stderr, "==> %s(0x%lx)\n", __FUNCTION__, self);
    return rb_int_new(_debug);
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
if (_debug)
fprintf(stderr, "==> %s() rpmtsClass 0x%lx\n", __FUNCTION__, rpmtsClass);
    rpmtsClass = rb_define_class("Ts", rb_cObject);
    rb_define_alloc_func(rpmtsClass, rpmts_alloc);
    rb_define_method(rpmtsClass, "debug", rpmts_debug, 1);
}

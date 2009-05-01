/** \ingroup rb_c
 * \file ruby/rpmmc-rb.c
 */

#include "system.h"

#include "rpm-rb.h"
#include "rpmmc-rb.h"

#define	_MACRO_INTERNAL
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
static VALUE
rpmmc_add(VALUE s, VALUE v)
{
    void *ptr = rpmmc_ptr(s);
    rpmmc mc = ptr;
    int lvl = 0;
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx) ptr %p\n", __FUNCTION__, s, v, ptr);
    (void) rpmDefineMacro(mc, StringValueCStr(v), lvl);
    return Qtrue;
}

static VALUE
rpmmc_del(VALUE s, VALUE v)
{
    void *ptr = rpmmc_ptr(s);
    rpmmc mc = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx) ptr %p\n", __FUNCTION__, s, v, ptr);
    (void) rpmUndefineMacro(mc, StringValueCStr(v));
    return Qtrue;
}

static VALUE
rpmmc_list(VALUE s)
{
    void *ptr = rpmmc_ptr(s);
    rpmmc mc = ptr;
    VALUE v = rb_ary_new();
    void * _mire = NULL;
    int used = -1;
    const char ** av = NULL;
    int ac = rpmGetMacroEntries(mc, _mire, used, &av);

if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);

    if (ac > 0 && av != NULL && av[0] != NULL) {
	int i;
	for (i = 0; i < ac; i++) {
	    /* XXX lua splits into {name,opts,body} triple. */
	    rb_ary_push(v, rb_str_new2(av[i]));
	}
    }
    return v;
}

static VALUE
rpmmc_expand(VALUE s, VALUE v)
{
    void *ptr = rpmmc_ptr(s);
    rpmmc mc = ptr;
    char * vstr = StringValueCStr(v);
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx) ptr %p \"%s\"\n", __FUNCTION__, s, v, ptr, vstr);
    return rb_str_new2(rpmMCExpand(mc, vstr, NULL));
}

static void
initMethods(VALUE klass)
{
    rb_define_method(klass, "add", rpmmc_add, 1);
    rb_define_method(klass, "del", rpmmc_del, 1);
    rb_define_method(klass, "list", rpmmc_list, 0);
    rb_define_method(klass, "expand", rpmmc_expand, 1);
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

static void
initProperties(VALUE klass)
{
    rb_define_method(klass, "debug", rpmmc_debug_get, 0);
    rb_define_method(klass, "debug=", rpmmc_debug_set, 1);
}

/* --- Object ctors/dtors */
static void
rpmmc_free(rpmmc mc)
{
if (_debug)
fprintf(stderr, "==> %s(%p)\n", __FUNCTION__, mc);

    if (!(mc == rpmGlobalMacroContext || mc == rpmCLIMacroContext)) {
	rpmFreeMacros(mc);
	mc = _free(mc);
    }
}

static VALUE
rpmmc_alloc(VALUE klass)
{
    rpmmc mc = xcalloc(1, sizeof(*mc));
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

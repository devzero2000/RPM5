/** \ingroup rb_c
 * \file ruby/rpmmc-rb.c
 */

#include "system.h"

#include "rpm-rb.h"
#include "rpmmc-rb.h"

#define	_MACRO_INTERNAL
#include <rpmmacro.h>
#include <rpmio/argv.h>

#include "../debug.h"


typedef MacroContext rpmmc;

VALUE rpmmcClass;

/*@unchecked@*/
static int _debug = 0;


/* --- helpers */

static void *
rpmmc_ptr(VALUE s)
{
    void *ptr;
    Data_Get_Struct(s, void, ptr);
    return ptr;
}


/* --- Object methods */

/**
 * Adds a new macro definition to the Macro Context.
 *
 * call-seq:
 *  RPM::Mc#add(macro) -> nil
 *
 * @param macro The macro definition in string form just like it would be done
 *  in a macro definition file, but minus the %define stanza.
 * @see         rpmDefineMacro()
 */
static VALUE
rpmmc_add(VALUE s, VALUE v)
{
    Check_Type(v, T_STRING);

    rpmmc mc = NULL;
    if(TYPE(s) == T_DATA)
        mc = rpmmc_ptr(s);
    int lvl = 0;
    
    if (_debug)
        fprintf(stderr, "==> %s(0x%lx, 0x%lx) ptr %p\n",
            __FUNCTION__, s, v, mc);
    
    (void) rpmDefineMacro(mc, StringValueCStr(v), lvl);
    return Qnil;
}


/**
 * Deletes a macro definition.
 *
 * call-seq:
 *  RPM::Mc#del(macro) -> nil
 *
 * @param macro The macro name
 * @see         rpmUndefineMacro()
 */
static VALUE
rpmmc_del(VALUE s, VALUE v)
{
    Check_Type(v, T_STRING);

    rpmmc mc = NULL;
    if(TYPE(s) == T_DATA)
        mc = rpmmc_ptr(s);
    
    if (_debug)
        fprintf(stderr, "==> %s(0x%lx, 0x%lx) ptr %p\n",
            __FUNCTION__, s, v, mc);
    
    (void) rpmUndefineMacro(mc, StringValueCStr(v));
    return Qnil;
}


/**
 * List all macro definitions in the corresponding Macro Context.
 *
 * call-seq:
 *  RPM::Mc#list() -> Array
 *
 * @return  A list of all macro definitions in form of an array of arrays,
 *  where each nested arry contains the macro's name, arguments (or an empty
 *  string) and the macro body.
 */
static VALUE
rpmmc_list(VALUE s)
{
    rpmmc mc = NULL;
    if(TYPE(s) == T_DATA)
        mc = rpmmc_ptr(s);

    void * _mire = NULL;
    VALUE v = rb_ary_new();
    int used = -1;
    const char ** av = NULL;
    int ac = rpmGetMacroEntries(mc, _mire, used, &av);

    if (_debug)
        fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, mc);

    if (ac > 0 && av != NULL && av[0] != NULL) {
        int i;
        for (i = 0; i < ac; i++) {
            /* Parse out "%name(opts)\tbody" into n/o/b strings. */

            char *name = (char *)av[i];
            char *body = strchr(name, '\t');
            assert(body != NULL);
            char *opts = ((body > name && body[-1] == ')') ?
                strchr(name, '(') : NULL);

        	if (*name == '%') name++;
        	if (opts != NULL && *opts == '(') {
                body[-1] = '\0';
                opts++;
                opts[-1] = '\0';
            } else {
                body[0] = '\0';
                opts = "";
            }
            body++;

            VALUE nob_ary = rb_ary_new3(3, rb_str_new2(name),
                rb_str_new2(opts), rb_str_new2(body));
            rb_ary_push(v, nob_ary);
        }
    }

    argvFree(av);
    return v;
}


/**
 * Expands a macro.
 *
 * call-seq:
 *  RPM::Mc#expand(macro) -> String
 *
 * @param macro The macro name (with leading % sign)
 * @return      The result of the expansion
 */
static VALUE
rpmmc_expand(VALUE s, VALUE v)
{
    rpmmc mc = NULL;
    if(TYPE(s) == T_DATA)
        mc = rpmmc_ptr(s);
    char *vstr = StringValueCStr(v);

    if (_debug)
        fprintf(stderr, "==> %s(0x%lx, 0x%lx) ptr %p \"%s\"\n",
            __FUNCTION__, s, v, mc, vstr);
    return rb_str_new2(rpmMCExpand(mc, vstr, NULL));
}


static void
initMethods(VALUE klass)
{
    rb_define_method(klass, "add", &rpmmc_add, 1);
    rb_define_singleton_method(klass, "add", &rpmmc_add, 1);
    rb_define_method(klass, "del", &rpmmc_del, 1);
    rb_define_singleton_method(klass, "del", &rpmmc_del, 1);
    rb_define_method(klass, "list", &rpmmc_list, 0);
    rb_define_singleton_method(klass, "list", &rpmmc_list, 0);
    rb_define_method(klass, "expand", &rpmmc_expand, 1);
    rb_define_singleton_method(klass, "expand", &rpmmc_expand, 1);
}


/* --- Object properties */

/**
 * Get debugging log level
 *
 * call-seq:
 *  RPM::Mc#debug -> Fixnum
 *
 * @return The debugging level
 */
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
        fprintf(stderr, "==> %s(0x%lx) obj 0x%lx mc %p\n",
            __FUNCTION__, klass, obj, mc);
    return obj;
}


/* --- Class initialization */

void
Init_rpmmc(void)
{
    rpmmcClass = rb_define_class_under(rpmModule, "Mc", rb_cObject);
    if (_debug)
        fprintf(stderr, "==> %s() rpmmcClass 0x%lx\n", __FUNCTION__, rpmmcClass);
    rb_define_alloc_func(rpmmcClass, rpmmc_alloc);
    initProperties(rpmmcClass);
    initMethods(rpmmcClass);
}

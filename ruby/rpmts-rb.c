/** \ingroup rb_c
 * \file ruby/rpmts-rb.c
 */

#include "system.h"

#include "rpm-rb.h"
#include "rpmts-rb.h"
#include "rpmmi-rb.h"

#include <argv.h>
#include <mire.h>

#include <rpmdb.h>

#define	_RPMTS_INTERNAL
#include <rpmts.h>

#include "../debug.h"

VALUE rpmtsClass;

/*@unchecked@*/
static int _debug = 0;

/* --- helpers */
static void *
rpmts_ptr(VALUE s)
{
    void *ptr;
    Data_Get_Struct(s, void, ptr);
    return ptr;
}

static VALUE
rpmtsLoadNVRA(VALUE s)
{
    void *ptr = rpmts_ptr(s);
    rpmts ts = ptr;
    VALUE NVRA = rb_ary_new();
    ARGV_t keys = NULL;
    int nkeys;
    int xx;
    int i;

    if (ts->rdb == NULL)
	(void) rpmtsOpenDB(ts, O_RDONLY);

    xx = rpmdbMireApply(rpmtsGetRdb(ts), RPMTAG_NVRA,
		RPMMIRE_STRCMP, NULL, &keys);
    nkeys = argvCount(keys);

    if (keys)
    for (i = 0; i < nkeys; i++)
	rb_ary_push(NVRA, rb_str_new2(keys[i]));

if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p NVRA 0x%lx\n", __FUNCTION__, s, ptr, NVRA);

    keys = argvFree(keys);
    return NVRA;
}

/* --- Object methods */
static VALUE
rpmts_mi(int argc, VALUE *argv, VALUE s)
{
    VALUE v_tag, v_key;
    rpmts ts = rpmts_ptr(s);
    rpmTag _tag = RPMDBI_PACKAGES;
    void * _key = NULL;
    int _len = 0;

    rb_scan_args(argc, argv, "02", &v_tag, &v_key);

    if (!NIL_P(v_tag))
        _tag = FIX2INT(v_tag);
    if (!NIL_P(v_key))
        _key = StringValueCStr(v_key);

    return rpmrb_NewMi(ts, _tag, _key, _len);
}

static void
initMethods(VALUE klass)
{
    rb_define_method(klass, "mi", rpmts_mi, -1);
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
    return INT2FIX(rpmtsVSFlags(ts));
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

static VALUE
rpmts_NVRA_get(VALUE s)
{
    return rpmtsLoadNVRA(s);
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
    rb_define_method(klass, "NVRA", rpmts_NVRA_get, 0);
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
rpmts_new(int argc, VALUE *argv, VALUE s)
{
    VALUE v_rootdir;
    char * _rootdir = "/";
    rpmts ts;

    rb_scan_args(argc, argv, "01", &v_rootdir);

    if (!NIL_P(v_rootdir))
        _rootdir = StringValueCStr(v_rootdir);

    ts = rpmtsCreate();
    rpmtsSetRootDir(ts, _rootdir);

if (_debug)
fprintf(stderr, "==> %s(%p[%d], 0x%lx) ts %p\n", __FUNCTION__, argv, argc, s, ts
);
    return Data_Wrap_Struct(s, 0, rpmts_free, ts);
}

/* --- Class initialization */

void
Init_rpmts(void)
{
    rpmtsClass = rb_define_class("Ts", rb_cObject);
if (_debug)
fprintf(stderr, "==> %s() rpmtsClass 0x%lx\n", __FUNCTION__, rpmtsClass);
#ifdef	NOTYET
    rb_include_module(rpmtsClass, rb_mEnumerable);
#endif
    rb_define_singleton_method(rpmtsClass, "new", rpmts_new, -1);
    initProperties(rpmtsClass);
    initMethods(rpmtsClass);
}

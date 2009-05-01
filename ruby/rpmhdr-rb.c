/** \ingroup rb_c
 * \file ruby/rpmhdr-rb.c
 */

#include "system.h"

#include "rpm-rb.h"
#include "rpmhdr-rb.h"

#ifdef	NOTYET
#include <argv.h>
#include <mire.h>
#endif

#include <rpmcli.h>	/* XXX rpmHeaderFormats */

#include "../debug.h"

typedef	Header rpmhdr;

VALUE rpmhdrClass;

/*@unchecked@*/
static int _debug = -1;

/* --- helpers */
static void *
rpmhdr_ptr(VALUE s)
{
    void *ptr;
    Data_Get_Struct(s, void, ptr);
    return ptr;
}

static VALUE
rpmhdrLoadTag(Header h, const char * name)
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    VALUE v = Qfalse;
    int i;

if (_debug)
fprintf(stderr, "==> %s(%p,%s)\n", __FUNCTION__, h, name);

    he->tag = tagValue(name);
    if (headerGet(h, he, 0)) {
if (_debug < 0)
fprintf(stderr, "\t%s(%u) %u %p[%u]\n", name, (unsigned)he->tag, (unsigned)he->t, he->p.ptr, (unsigned)he->c);
	switch (he->t) {
	default:
	    goto exit;
	    /*@notreached@*/ break;
	case RPM_BIN_TYPE:	/* XXX return as array of octets for now. */
	case RPM_UINT8_TYPE:
	    v = rb_ary_new();
	    for (i = 0; i < (int)he->c; i++)
		rb_ary_push(v, INT2FIX(he->p.ui8p[i]));
	    break;
	case RPM_UINT16_TYPE:
	    v = rb_ary_new();
	    for (i = 0; i < (int)he->c; i++)
		rb_ary_push(v, INT2FIX(he->p.ui16p[i]));
	    break;
	case RPM_UINT32_TYPE:
	    v = rb_ary_new();
	    for (i = 0; i < (int)he->c; i++)
		rb_ary_push(v, INT2FIX(he->p.ui32p[i]));
	    break;
	case RPM_UINT64_TYPE:
	    v = rb_ary_new();
	    for (i = 0; i < (int)he->c; i++)
		rb_ary_push(v, INT2FIX(he->p.ui64p[i]));
	    break;
	case RPM_STRING_ARRAY_TYPE:
	    v = rb_ary_new();
	    for (i = 0; i < (int)he->c; i++)
		rb_ary_push(v, rb_str_new2(he->p.argv[i]));
	    break;
	case RPM_I18NSTRING_TYPE:	/* XXX FIXME: is this ever seen? */
	case RPM_STRING_TYPE:
	    v = rb_str_new2(he->p.str);
	    break;
	}
    }

exit:
    he->p.ptr = _free(he->p.ptr);
    return v;
}
/* --- Object methods */

static VALUE
rpmhdr_sprintf(VALUE s, VALUE v)
{
    void *ptr = rpmhdr_ptr(s);
    rpmhdr h = ptr;
    char * qfmt = StringValueCStr(v);
    char * q;
    const char *errstr = NULL;

if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx) ptr %p\n", __FUNCTION__, s, v, ptr);

    if ((q = headerSprintf(h, qfmt, NULL, rpmHeaderFormats, &errstr)) == NULL)
	q = errstr;

    return rb_str_new2(q);
}

static VALUE
rpmhdr_getorigin(VALUE s)
{
    void *ptr = rpmhdr_ptr(s);
    rpmhdr h = ptr;

if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);

    return rb_str_new2(headerGetOrigin(h));
}

static VALUE
rpmhdr_setorigin(VALUE s, VALUE v)
{
    void *ptr = rpmhdr_ptr(s);
    rpmhdr h = ptr;

if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx) ptr %p\n", __FUNCTION__, s, v, ptr);

    (void) headerSetOrigin(h, StringValueCStr(v));
    return rb_str_new2(headerGetOrigin(h));
}

static void
initMethods(VALUE klass)
{
    rb_define_method(klass, "sprintf", rpmhdr_sprintf, 1);
    rb_define_method(klass, "getorigin", rpmhdr_getorigin, 0);
    rb_define_method(klass, "setorigin", rpmhdr_setorigin, 1);
}

/* --- Object properties */
static VALUE
rpmhdr_debug_get(VALUE s)
{
if (_debug)
fprintf(stderr, "==> %s(0x%lx)\n", __FUNCTION__, s);
    return INT2FIX(_debug);
}

static VALUE
rpmhdr_debug_set(VALUE s, VALUE v)
{
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx)\n", __FUNCTION__, s, v);
    return INT2FIX(_debug = FIX2INT(v));
}

static VALUE
rpmhdr__get(VALUE s, VALUE v)
{
    void *ptr = rpmhdr_ptr(s);
    rpmhdr h = ptr;
    char * vstr = StringValueCStr(v);
    
if (_debug)
fprintf(stderr, "==> %s(0x%lx) %s\n", __FUNCTION__, s, vstr);

    return rpmhdrLoadTag(h, vstr);
}

#ifdef	NOTYET
static VALUE
rpmhdr_rootdir_get(VALUE s)
{
    void *ptr = rpmhdr_ptr(s);
    rpmhdr h = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    return rb_str_new2(rpmhdrRootDir(h));
}

static VALUE
rpmhdr_rootdir_set(VALUE s, VALUE v)
{
    void *ptr = rpmhdr_ptr(s);
    rpmhdr h = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx) ptr %p\n", __FUNCTION__, s, v, ptr);
    rpmhdrSetRootDir(h, StringValueCStr(v));
    return rb_str_new2(rpmhdrRootDir(h));
}

static VALUE
rpmhdr_vsflags_get(VALUE s)
{
    void *ptr = rpmhdr_ptr(s);
    rpmhdr h = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx) ptr %p\n", __FUNCTION__, s, ptr);
    return INT2FIX(_debug);
}

static VALUE
rpmhdr_vsflags_set(VALUE s, VALUE v)
{
    void *ptr = rpmhdr_ptr(s);
    rpmhdr h = ptr;
if (_debug)
fprintf(stderr, "==> %s(0x%lx, 0x%lx) ptr %p\n", __FUNCTION__, s, v, ptr);
    rpmhdrSetVSFlags(h, FIX2INT(v));
    return INT2FIX(rpmhdrVSFlags(h));
}
#endif	/* NOTYET */

static void
initProperties(VALUE klass)
{
    rb_define_method(klass, "debug", rpmhdr_debug_get, 0);
    rb_define_method(klass, "debug=", rpmhdr_debug_set, 1);
    rb_define_method(klass, "[]", rpmhdr__get, 1);
#ifdef	NOTYET
    rb_define_method(klass, "rootdir", rpmhdr_rootdir_get, 0);
    rb_define_method(klass, "rootdir=", rpmhdr_rootdir_set, 1);
    rb_define_method(klass, "vsflags", rpmhdr_vsflags_get, 0);
    rb_define_method(klass, "vsflags=", rpmhdr_vsflags_set, 1);
#endif	/* NOTYET */
}

/* --- Object ctors/dtors */
static void
rpmhdr_free(rpmhdr h)
{
if (_debug)
fprintf(stderr, "==> %s(%p)\n", __FUNCTION__, h);

    h = headerFree(h);
}

static VALUE
rpmhdr_alloc(VALUE klass)
{
#ifdef	NOTYET
    rpmhdr h = NULL;
#else
    rpmhdr h = headerNew();
#endif	/* NOTYET */
    VALUE obj = Data_Wrap_Struct(klass, 0, rpmhdr_free, h);
if (_debug)
fprintf(stderr, "==> %s(0x%lx) obj 0x%lx h %p\n", __FUNCTION__, klass, obj, h);
    return obj;
}

/* --- Class initialization */

void
Init_rpmhdr(void)
{
    rpmhdrClass = rb_define_class("Hdr", rb_cObject);
if (_debug)
fprintf(stderr, "==> %s() rpmhdrClass 0x%lx\n", __FUNCTION__, rpmhdrClass);
    rb_define_alloc_func(rpmhdrClass, rpmhdr_alloc);
    initProperties(rpmhdrClass);
    initMethods(rpmhdrClass);
}

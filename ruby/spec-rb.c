/**
 * \ingroup rb_c
 * \file spec-rb.c
 *
 * Ruby bindings for spec file access
 */


#include "system.h"

#include "rpm-rb.h"
#include "spec-rb.h"
#include "rpmts-rb.h"
#include "rpmmc-rb.h"
#include "rpmhdr-rb.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>

#define	_RPMTS_INTERNAL
#define _RPMFI_INTERNAL
#include <rpmtag.h>
#include <rpmtypes.h>
#include <rpmio.h>
#include <rpmbuild.h>
#include <rpmspec.h>
#include <rpmmacro.h>


VALUE rpmrbSpecClass;


/**
 * The struct representing instances of RPM::Spec Ruby classes.
 */
struct rpmrbSpecInstance_s {
    Spec spec;  /*< The referenced and wrapped Spec_s:: pointer */
    VALUE ts;   /*< Transaction Set (RPM::Ts) used */
};

typedef struct rpmrbSpecInstance_s* rpmrbSpecInstance;


/**
 * C destructor for the Spec class.
 */
static void
rpmrbSpecFree(rpmrbSpecInstance speci)
{
    speci->spec = freeSpec(speci->spec);
    if(NULL == speci->spec) free(speci);
}


static void
rpmrbSpecMark(rpmrbSpecInstance speci)
{
    if(0 != speci->ts) rb_gc_mark(speci->ts);
}


/**
 * Unwraps the rpmrbSpecInstance_s:: structure.
 */
static rpmrbSpecInstance
rpmrbSpeciUnwrap(VALUE self)
{
    rpmrbSpecInstance speci = NULL;
    Data_Get_Struct(self, struct rpmrbSpecInstance_s, speci);
    return speci;
}


Spec
rpmrbSpecUnwrap(VALUE self)
{
    rpmrbSpecInstance speci = rpmrbSpeciUnwrap(self);
    return speci->spec;
}


VALUE
rpmrbSpecWrap(Spec spec, VALUE ts)
{
    rpmrbSpecInstance speci = malloc(sizeof(struct rpmrbSpecInstance_s));
    speci->spec = spec;
    speci->ts = ts;

    VALUE rpmrbSpec = Data_Wrap_Struct(rpmrbSpecClass, 
        &rpmrbSpecMark, &rpmrbSpecFree, speci);
    return rpmrbSpec;
}


/**
 * A helper routine that returns a Ruby array containing all sources that
 * match a specific set of OR'ed flags.
 */
static VALUE
_rpmrbSpecGetSources(VALUE self, int flags)
{
    Spec spec = rpmrbSpecUnwrap(self);

    VALUE ary = rb_ary_new();

    SpecSource src;
    for(src = spec->sources; src != NULL; src = src->next)
        if(src->flags & flags)
            rb_ary_push(ary, rb_str_new2(src->fullSource));

    return ary;
}


/**
 * Returns an array of all sources defined in the spec file.
 *
 * call-seq:
 *  RPM::Spec#sources -> Array
 *
 * @return An array containing the full name of all source URIs
 */
static
VALUE spec_get_sources(VALUE self)
{
    return _rpmrbSpecGetSources(self, RPMFILE_SOURCE);
}


/**
 * Returns an array of all patches defined in the spec file.
 *
 * call-seq:
 *  RPM::Spec#patches -> Array
 *
 * @return An array containing the full name of all patch URIs
 */
static VALUE
spec_get_patches(VALUE self)
{
    return _rpmrbSpecGetSources(self, RPMFILE_PATCH);
}


/**
 * Returns all package headers for the binary RPMs associated with the specfile.
 *
 * This returns all packages that are the result of the spec file would it be
 * built. It serves as a factory method creating RPM::Header classes.
 *
 * call-seq:
 *  RPM::Spec#binheaders -> Array
 *
 * @return An array of headers for every binary RPM created during build
 * @see headerToken_s::
 */
static VALUE
rpmrbSpecGetBinHeaders(VALUE self)
{
    VALUE hdr_ary = rb_ary_new();
    Spec spec = rpmrbSpecUnwrap(self);
    
    Package pkg;
    for(pkg = spec->packages; pkg != NULL; pkg = pkg->next)
        rb_ary_push(hdr_ary, rpmrb_NewHdr(headerLink(pkg->header)));

    return hdr_ary;
}


/**
 * Returns the macro context of the spec file.
 *
 * call-seq:
 *  RPM::Spec#mc -> RPM::Mc
 *
 * @return The macro context associated with the spec file.
 */
static VALUE
rpmrbSpecGetMc(VALUE self)
{
    Spec spec = rpmrbSpecUnwrap(self);
    return rpmmc_wrap(spec->macros);
}


/**
 * Builds a part of the spec file.
 *
 * This method will call buildSpec() to build parts of the spec file. Its mode
 * of operation is determined by the constants in ::rpmBuildFlags_e, which are
 * mapped in the RPM Ruby module. Will throw an exception if the build fails.
 *
 * call-seq:
 *  RPM::Spec#build(flags, test = false) -> RPM::Spec
 *
 * @param flags A combination of flags that control the build.
 * @param test  If set to true, the scriptlets won't actually be run.
 * @return      The RPM::Spec instance
 * @see         buildSpec, ::rpmBuildFlags_e
 */
static VALUE
rpmrbSpecBuild(VALUE argc, VALUE *argv, VALUE self)
{
    VALUE test_v = T_FALSE, flags_v;
    rb_scan_args(argc, argv, "11", &flags_v, &test_v);

    int test = 0;
    switch(TYPE(test_v)) {
        case T_TRUE:
            test = 1;
            break;
        case T_NIL:
        case T_FALSE:
            test = 0;
            break;
        default:
            rb_raise(rb_eTypeError,
                "Value for test must be either true or false");
            break;
    }

    Check_Type(flags_v, T_FIXNUM);
    int flags = FIX2INT(flags_v);

    rpmrbSpecInstance speci = rpmrbSpeciUnwrap(self);
    rpmts ts = rpmrbTsUnwrap(speci->ts);

    rpmRC error = buildSpec(ts, speci->spec, flags, test);
    if(error) rpm_rb_raise(error, "Building spec file failed");

    return self;
}


void
Init_spec(void)
{
    rpmrbSpecClass = rb_define_class_under(rpmModule, "Spec", rb_cObject);

    rb_define_method(rpmrbSpecClass, "sources", &spec_get_sources, 0);
    rb_define_method(rpmrbSpecClass, "patches", &spec_get_patches, 0);
    rb_define_method(rpmrbSpecClass, "binheaders", &rpmrbSpecGetBinHeaders, 0);
    rb_define_method(rpmrbSpecClass, "mc", &rpmrbSpecGetMc, 0);
    rb_define_method(rpmrbSpecClass, "build", &rpmrbSpecBuild, -1);
}

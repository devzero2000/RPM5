/**
 * \ingroup rb_c
 * \file spec-rb.c
 *
 * Ruby bindings for spec file access
 */


#include "system.h"

#include "rpm-rb.h"
#include "spec-rb.h"
#include "rpmmc-rb.h"
#include "package-rb.h"

#include <rpmdb.h>
#include <rpmbuild.h>
#include <rpmspec.h>
#include <rpmmacro.h>


VALUE specClass;


/**
 * Returns the wrapped Spec structure.
 */
static Spec
_spec_get_spec(VALUE self)
{
    Spec spec;
    Data_Get_Struct(self, struct Spec_s, spec);
    return spec;
}


/**
 * Returns the hiddenly associated transaction set.
 */
static rpmts
_spec_get_ts(VALUE self)
{
    rpmts ts;
    Data_Get_Struct(rb_iv_get(self, "ts"), struct rpmts_s, ts);
    return ts;
}


/**
 * A helper routine that returns a Ruby array containing all sources that
 * match a specific set of OR'ed flags.
 */
static VALUE
_spec_get_sources(VALUE self, int flags)
{
    Spec spec = _spec_get_spec(self);

    VALUE ary = rb_ary_new();

    SpecSource src;
    for(src = spec->sources; src != NULL; src = src->next)
        if(src->flags & flags)
            rb_ary_push(ary, rb_str_new2(src->fullSource));

    return ary;
}


/**
 * Does the actual call to buildSpec.
 */
static rpmRC
_spec_build(VALUE argc, VALUE *argv, VALUE self, int flag)
{
    VALUE test_v;
    rb_scan_args(argc, argv, "01", &test_v);

    int test = 0;
    if(TYPE(test_v) == T_TRUE) test = 1;

    rpmRC rc = buildSpec(_spec_get_ts(self), _spec_get_spec(self),
        flag, test);
    return rc;
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
    return _spec_get_sources(self, RPMFILE_SOURCE);
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
    return _spec_get_sources(self, RPMFILE_PATCH);
}


/**
 * Returns all packages associated with the spec file.
 *
 * This returns all packages that are the result of the spec file would it be
 * built. It serves as a factory method creating RPM::Package classes.
 *
 * call-seq:
 *  RPM::Spec#packages -> Array
 *
 * @return An array of all packages
 * @see packageClass
 */
static VALUE
spec_get_packages(VALUE self)
{
    VALUE pkg_ary = rb_ary_new();
    Spec spec = _spec_get_spec(self);
    
    Package pkg;
    for(pkg = spec->packages; pkg != NULL; pkg = pkg->next)
        rb_ary_push(pkg_ary, Data_Wrap_Struct(packageClass, 0, -1, pkg));

    return pkg_ary;
}


/**
 * Returns the macro context of the spec file.
 *
 * call-seq:
 *  RPM::Spec#macros -> RPM::Mc
 *
 * @return The macro context associated with the spec file.
 */
static VALUE
spec_get_macros(VALUE self)
{
    Spec spec = _spec_get_spec(self);
    return rpmmc_wrap(spec->macros);
}


/**
 * Run the %prep section of the spec file.
 *
 * call-seq:
 *  RPM::Spec#prep(test = false) -> RPM::Spec
 *
 * @param test  If set to true, the scriptlet won't actually be run
 * @return      The class instance itself for method chaining
 */
static VALUE
spec_prep(VALUE argc, VALUE *argv, VALUE self)
{
    rpmRC rc = _spec_build(argc, argv, self, RPMBUILD_PREP);
    return self;
}


/**
 * Run the %build section of the spec file.
 *
 * call-seq:
 *  RPM::Spec#build(test = false) -> RPM::Spec
 *
 * @param test  If set to true, the scriptlet won't actually be run
 * @return      The class instance itself for method chaining
 */
static VALUE
spec_build(VALUE argc, VALUE *argv, VALUE self)
{
    rpmRC rc = _spec_build(argc, argv, self, RPMBUILD_BUILD);
    return self;
}


/**
 * Run the %install section of the spec file.
 *
 * call-seq:
 *  RPM::Spec#install(test = false) -> RPM::Spec
 *
 * @param test  If set to true, the scriptlet won't actually be run
 * @return      The class instance itself for method chaining
 */
static VALUE
spec_install(VALUE argc, VALUE *argv, VALUE self)
{
    rpmRC rc = _spec_build(argc, argv, self, RPMBUILD_INSTALL);
    return self;
}


/**
 * Run the %check section of the spec file.
 *
 * call-seq:
 *  RPM::Spec#check(test = false) -> RPM::Spec
 *
 * @param test  If set to true, the scriptlet won't actually be run
 * @return      The class instance itself for method chaining
 */
static VALUE
spec_check(VALUE argc, VALUE *argv, VALUE self)
{
    rpmRC rc = _spec_build(argc, argv, self, RPMBUILD_CHECK);
    return self;
}


/**
 * Runs the %clean section of the spec file
 *
 * call-seq:
 *  RPM::Spec#clean(test = false) -> RPM::Spec
 *
 * @param test  If set to true, the scriptlet won't actually be run
 * @return      The class instance itself for method chaining
 */
static VALUE
spec_clean(VALUE argc, VALUE *argv, VALUE self)
{
    rpmRC rc = _spec_build(argc, argv, self, RPMBUILD_CLEAN);
    return self;
}


/**
 * Create the binary packages specified in the spec file.
 *
 * call-seq:
 *  RPM::Spec#package_binaries -> RPM::Spec
 *
 * @return      The class instance itself for method chaining
 */
static VALUE
spec_package_binaries(VALUE self)
{
    rpmRC rc = packageBinaries(_spec_get_spec(self));
    return self;
}


/**
 * Create the source package associated with the spec file.
 *
 * call-seq:
 *  RPM::Spec#package_sources -> RPM::Spec
 *
 * @return      The class instance itself for method chaining
 */
static VALUE
spec_package_sources(VALUE self)
{
    rpmRC rc = packageSources(_spec_get_spec(self));
    return self;
}



VALUE
spec_wrap(Spec spec)
{
    return Data_Wrap_Struct(specClass, 0, &closeSpec, spec);
}


void
Init_spec(void)
{
    specClass = rb_define_class_under(rpmModule, "Spec", rb_cObject);

    rb_define_method(specClass, "sources", &spec_get_sources, 0);
    rb_define_method(specClass, "patches", &spec_get_patches, 0);
    rb_define_method(specClass, "packages", &spec_get_packages, 0);
    rb_define_method(specClass, "macros", &spec_get_macros, 0);
    rb_define_method(specClass, "prep", &spec_prep, -1);
    rb_define_method(specClass, "build", &spec_build, -1);
    rb_define_method(specClass, "install", &spec_install, -1);
    rb_define_method(specClass, "check", &spec_check, -1);
    rb_define_method(specClass, "clean", &spec_clean, -1);
    rb_define_method(specClass, "package_binaries", &spec_package_binaries, 0);
    rb_define_method(specClass, "package_sources", &spec_package_sources, 0);
}

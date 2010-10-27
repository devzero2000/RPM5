/**
 * \ingroup rb_c
 * \file spec-rb.c
 *
 * Ruby bindings for spec file access
 */


#include "system.h"

#include "rpm-rb.h"
#include "spec-rb.h"

#include <rpmdb.h>
#include <rpmbuild.h>


VALUE specClass;


/**
 * A helper routine that returns a Ruby array containing all sources that
 * match a specific set of OR'ed flags.
 */
static VALUE _spec_get_sources(VALUE self, int flags)
{
    Spec spec;
    Data_Get_Struct(self, struct Spec_s, spec);

    VALUE ary = rb_ary_new();

    SpecSource src;
    for(src = spec->sources; src != NULL; src = src->next)
        if(src->flags & flags)
            rb_ary_push(ary, rb_str_new2(src->fullSource));

    return ary;
}



/**
 * Returns an array of all sources defined in the spec file.
 */
static VALUE spec_get_sources(VALUE self)
{
    return _spec_get_sources(self, RPMFILE_SOURCE);
}


/**
 * Returns an array of all patches defined in the spec file.
 */
static VALUE spec_get_patches(VALUE self)
{
    return _spec_get_sources(self, RPMFILE_PATCH);
}


void Init_spec(void)
{
    specClass = rb_define_class_under(rpmModule, "Spec", rb_cObject);

    /* Init properties */
    rb_define_method(specClass, "sources", &spec_get_sources, 0);
    rb_define_method(specClass, "patches", &spec_get_patches, 0);
}

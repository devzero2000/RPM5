/*! 
 * \ingroup rb_c
 * \file rpm-rb.c
 * Ruby Bindings initialization file
 *
 * This file is the entry point for RPM's Ruby Bindings and contains the
 * infamous Init_rpm() function that starts every Ruby binding. It also
 * stores the ::rpmModule variable used for referencing the newly created
 * Ruby module, e.g. to add methods.
 */


#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <ruby.h>
#pragma GCC diagnostic warning "-Wstrict-prototypes"

#include "system.h"

#include "rpm-rb.h"
#include "rpmts-rb.h"
#include "spec-rb.h"

#include "debug.h"


VALUE rpmModule;


void Init_rpm(void)
{
    /* The "RPM" Ruby module. */
    rpmModule = rb_define_module("RPM");

    Init_rpmts();
    Init_spec();
}


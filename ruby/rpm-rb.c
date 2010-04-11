/*! 
 * \ingroup rb_c
 * \file rpm-rb.c
 * Ruby Bindings initialization file
 *
 * This file is the entry point for RPM's Ruby Bindings and contains the
 * infamous \ref Init_rpm function that starts every Ruby binding. It also
 * stores the \ref rpmModule variable used for referencing the newly created
 * Ruby module, e.g. to add methods.
 */


#include "system.h"

#include "rpm-rb.h"
#include "rpmts-rb.h"

#include "debug.h"


/*!
 * \summary Ruby module reference variable
 */
VALUE rpmModule;


/*!
 * \summary Initialization function for RPM Ruby Bindings
 */
void
Init_rpm(void)
{
    rpmModule = rb_define_module("rpm");
}

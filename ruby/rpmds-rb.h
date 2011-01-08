#ifndef H_RPMDS_RB
#define H_RPMDS_RB

/**
 * @ingroup rb_c
 * @file ruby/rpmds-rb.h
 *
 * Ruby-C interface to RPM's Dependency Set functions.
 */


#include "system.h"


/**
 * Definition of the Ruby RPM::Ds class.
 */
extern VALUE rpmrbDsClass;


#ifdef __cplusplus
extern "C" {
#endif



/**
 * The Ruby "new" method of the RPM::Ds class.
 *
 * Creates a new RPM::Ds object instance given zero or two parameters.
 *
 * call-seq:
 *  RPM::Ds.new(header = nil, tag = nil) -> RPM::Ds
 *
 * @param argc  Number of arguments in *argv
 * @param *argv An array containing nothing, a header object, a tag object, or
 *  both. See the Ruby method signature for more information.
 * @param klass The Ruby class (rpmrbDsClass)
 * @return      The new RPM::Ds object instance.
 */
VALUE
rpmrbDsNew(int argc, VALUE *argv, VALUE klass);


/**
 * Wraps an already existing rpmds_s:: struct in a RPM::Ds Ruby object.
 *
 * @param ds    The struct pointer
 * @return      The wrapped Ruby object
 */
VALUE
rpmrbDsWrap(rpmds ds);


/**
 * Helper function that returns the rpmds pointer from an instance struct.
 *
 * @param self  The RPM::Ds class instance
 * @return      The wrapped rpmds_s:: struct
 */
rpmds
rpmrbDsUnwrap(VALUE self);


/**
 * Initializes the RPM::Ds class
 */
void
Init_rpmds(void);


#ifdef __cplusplus      
}
#endif

#endif	/* H_RPMDS_RB */

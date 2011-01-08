/**
 * \ingroup rb_c
 * \file spec-rb.h
 *
 * Ruby bindings for spec file access
 */

#ifndef H_SPEC_RB
#define H_SPEC_RB


#include "system.h"

#define	_RPMTS_INTERNAL
#define _RPMFI_INTERNAL
#include <rpmtag.h>
#include <rpmtypes.h>
#include <rpmio.h>
#include <rpmspec.h>


/** 
 * The Ruby class representation of the ::Spec_s structure and methods.
 *
 * This is the RPM::Spec class. It is generated from an existing transaction
 * set via RPM::Ts#parse_spec. Instances of RPM::Spec provide access to
 * information about the spec file itself (such as a list of sources and
 * patches, or access to the macro context associated with the spec file), and
 * also allow building the spec file.
 */
extern VALUE rpmrbSpecClass;


#ifdef __cplusplus
extern "C" {
#endif


/**
 * Wraps an already existing Spec_s:: structure in a Ruby class.
 *
 * @param spec  The initialized Spec_s:: structure
 * @param ts    The RPM::Ts instance used when parsing the spec. May be 0.
 * @return      The newly wrapped class instance
 */
VALUE
rpmrbSpecWrap(Spec spec, VALUE ts);


/**
 * Returns the wrapped Spec structure.
 */
Spec
rpmrbSpecUnwrap(VALUE self);


/**
 * Initializes the Ruby class
 */
void Init_spec(void);


#ifdef __cplusplus
}
#endif

#endif /* ifndef H_SPEC_RB */


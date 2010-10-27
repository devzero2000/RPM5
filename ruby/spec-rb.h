/**
 * \ingroup rb_c
 * \file spec-rb.h
 *
 * Ruby bindings for spec file access
 */

#ifndef H_SPEC_RB
#define H_SPEC_RB


#include "rpm-rb.h"
#include <ruby.h>


/** Spec file class reference */
extern VALUE specClass;


#ifdef __cplusplus
extern "C" {
#endif


/**
 * Initializes the Ruby class
 */
void Init_spec(void);


#ifdef __cplusplus
}
#endif

#endif /* ifndef H_SPEC_RB */


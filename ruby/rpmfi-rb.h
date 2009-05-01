#ifndef H_RPMFI_RB
#define H_RPMFI_RB

/**
 * \file ruby/rpmfi-rb.h
 */

#include "rpm-rb.h"

extern VALUE rpmfiClass;

#ifdef __cplusplus
extern "C" {
#endif

void
Init_rpmfi(void);

#ifdef __cplusplus      
}
#endif

#endif	/* H_RPMFI_RB */

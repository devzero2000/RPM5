#ifndef H_RPMTS_RB
#define H_RPMTS_RB

/**
 * \file ruby/rpmts-rb.h
 */

#include "rpm-rb.h"

extern VALUE rpmtsClass;

#ifdef __cplusplus
extern "C" {
#endif

void
Init_rpmts(void);

#ifdef __cplusplus      
}
#endif

#endif	/* H_RPMTS_RB */

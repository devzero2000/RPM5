#ifndef H_RPMDS_RB
#define H_RPMDS_RB

/**
 * \file ruby/rpmds-rb.h
 */

#include "rpm-rb.h"

extern VALUE rpmdsClass;

#ifdef __cplusplus
extern "C" {
#endif

void
Init_rpmds(void);

#ifdef __cplusplus      
}
#endif

#endif	/* H_RPMDS_RB */

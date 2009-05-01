#ifndef H_RPMMC_RB
#define H_RPMMC_RB

/**
 * \file ruby/rpmmc-rb.h
 */

#include "rpm-rb.h"

extern VALUE rpmmcClass;

#ifdef __cplusplus
extern "C" {
#endif

void
Init_rpmmc(void);

#ifdef __cplusplus      
}
#endif

#endif	/* H_RPMMC_RB */

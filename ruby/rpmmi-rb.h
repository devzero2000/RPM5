#ifndef H_RPMMI_RB
#define H_RPMMI_RB

/**
 * \file ruby/rpmmi-rb.h
 */

#include "rpm-rb.h"

extern VALUE rpmmiClass;

#ifdef __cplusplus
extern "C" {
#endif

void
Init_rpmmi(void);

#ifdef __cplusplus      
}
#endif

#endif	/* H_RPMMI_RB */

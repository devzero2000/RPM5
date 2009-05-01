#ifndef H_RPMTE_RB
#define H_RPMTE_RB

/**
 * \file ruby/rpmte-rb.h
 */

#include "rpm-rb.h"

extern VALUE rpmteClass;

#ifdef __cplusplus
extern "C" {
#endif

void
Init_rpmte(void);

#ifdef __cplusplus      
}
#endif

#endif	/* H_RPMTE_RB */

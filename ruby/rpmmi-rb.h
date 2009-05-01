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

extern void
Init_rpmmi(void);

extern VALUE
rpmrb_NewMi(VALUE s, void * _ts, int _tag, void * _key, int _keylen);

#ifdef __cplusplus      
}
#endif

#endif	/* H_RPMMI_RB */

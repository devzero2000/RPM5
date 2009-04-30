#ifndef H_RPM_RB
#define H_RPM_RB

/**
 * \file ruby/rpm-rb.h
 */

#include <rpmiotypes.h> 
#include <rpmio.h>

#include <rpmtypes.h>
#include <rpmtag.h>

extern VALUE rpmModule;

#ifdef __cplusplus
extern "C" {
#endif

void
Init_rpm(void);

#ifdef __cplusplus      
}
#endif

#endif	/* H_RPM_RB */

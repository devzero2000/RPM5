/** \ingroup rpmversion
 * \file lib/rpmversion.c
 */

#include "system.h"
#include "rpmversion.h"
#include "debug.h"

/* XXX Which one is it, RPM or RPMLIB ? */
#ifndef RPMLIB_VERSION
#define RPMLIB_VERSION RPM_VERSION
#endif
#ifndef RPMLIB_TIMESTAMP
#define RPMLIB_TIMESTAMP RPM_TIMESTAMP
#endif
#ifndef RPMLIB_VENDOR
#define RPMLIB_VENDOR RPM_VENDOR
#endif

/*@-shiftimplementation @*/
uint32_t rpmlibVersion(void)
{
    return (uint32_t)RPMLIB_VERSION;
}

uint32_t rpmlibTimestamp(void)
{
    return (uint32_t)RPMLIB_TIMESTAMP;
}

uint32_t rpmlibVendor(void)
{
    return (uint32_t)RPMLIB_VENDOR;
}
/*@=shiftimplementation @*/


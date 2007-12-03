/** \ingroup rpmversion
 * \file lib/rpmversion.c
 */

#include "system.h"
#include "rpmversion.h"
#include "debug.h"

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


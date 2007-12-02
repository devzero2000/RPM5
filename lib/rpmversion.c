/** \ingroup rpmversion
 * \file lib/rpmversion.c
 */

#include "system.h"
#include "rpmversion.h"
#include "debug.h"

unsigned long rpm_version(void)
{
    return (unsigned long)RPM_VERSION;
}

unsigned long rpm_timestamp(void)
{
    return (unsigned long)RPM_TIMESTAMP;
}

unsigned long rpm_vendor(void)
{
    return (unsigned long)RPM_VENDOR;
}


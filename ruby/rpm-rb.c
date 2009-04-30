#include "system.h"

#include "rpm-rb.h"
#include "rpmts-rb.h"

#include "debug.h"

VALUE rpmModule;

void
Init_rpm(void)
{
    rpmModule = rb_define_module ("rpm");
}

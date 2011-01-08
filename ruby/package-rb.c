/**
 * @file package-rb.c
 * @ingroup rb_c
 */


#include "system.h"
#include "rpm-rb.h"
#include "rpmds-rb.h"
#include "package-rb.h"

#define _RPMFI_INTERNAL
#define _RPMDS_INTERNAL
#include <rpmtag.h>
#include <rpmtypes.h>
#include <rpmio.h>
#include <rpmfi.h>
#include <rpmspec.h>
#include <rpmds.h>


VALUE rpmrbPackageClass;


struct rpmrbPackageInstance_s {
    Package pkg;    /*< The wrapped Package_s:: pointer */
    VALUE ds;       /*< Referenced RPM::Ds class instance */
};

typedef struct rpmrbPackageInstance_s* rpmrbPackageInstance;


static void
rpmrbPackageMark(rpmrbPackageInstance pkgi)
{
    if(0 != pkgi->ds) rb_gc_mark(pkgi->ds);
}


static void
rpmrbPackageFree(rpmrbPackageInstance pkgi)
{
    free(pkgi);
}


VALUE
rpmrbPackageWrap(Package pkg)
{
    rpmrbPackageInstance pkgi = malloc(sizeof(struct rpmrbPackageInstance_s));
    pkgi = memset(pkgi, 0, sizeof(struct rpmrbPackageInstance_s));
    pkgi->pkg = pkg;

    VALUE rpmrbPackage = Data_Wrap_Struct(rpmrbPackageClass, 
        &rpmrbPackageMark, &rpmrbPackageFree, pkgi);
    return rpmrbPackage;
}


/**
 * Returns the dependency set associated with the package.
 *
 * call-seq:
 *  RPM::Package#ds -> RPM::Ds
 *
 * @return The package's dependency set.
 * @see rpmdsClass
 */
static VALUE
rpmrbPackageGetDs(VALUE self)
{
    rpmrbPackageInstance pkgi = NULL;
    Data_Get_Struct(self, struct rpmrbPackageInstance_s, pkgi);
    pkgi->ds = rpmrbDsWrap(pkgi->pkg->ds);
    return pkgi->ds;
}


void
Init_Package(void)
{
    rpmrbPackageClass = rb_define_class_under(rpmModule, "Package", rb_cObject);

    rb_define_method(rpmrbPackageClass, "ds", &rpmrbPackageGetDs, 0);
}

#include "system.h"

#include <rpmcli.h>
#include <pkgio.h>

#include "debug.h"

static rpmRC inst(const char *fn)
{
    rpmRC rc = RPMRC_FAIL;
    rpmts ts = rpmtsCreate();
    FD_t fd = Fopen(fn, "r");
    Header h = NULL;
    int upgrade = 1;
    rpmRelocation relocs = NULL;
    rpmps ps = NULL;
    rpmprobFilterFlags probFilter = 0;

assert(ts);
    (void) rpmtsSetNotifyCallback(ts, rpmShowProgress,  (void *) ((long)0));
    if (fd == NULL || Ferror(fd))
	goto exit;
     (void) rpmReadPackageFile(ts, fd, fn, &h);

    if ((rc = rpmtsAddInstallElement(ts, h, fn, upgrade, relocs)) != 0
     || (rc = rpmcliInstallCheck(ts)) != 0
     || (rc = rpmcliInstallOrder(ts)) != 0
     || (rc = rpmcliInstallRun(ts, ps, probFilter)) != 0)
	goto exit;

exit:
    h = headerFree(h);
    if (fd) (void) Fclose(fd);
    fd = NULL;
    ts = rpmtsFree(ts);
    if (rc)
fprintf(stderr, "<== %s(%s) rc %d\n", __FUNCTION__, fn, rc);
    return rc;
}

static struct poptOption optionsTable[] = {

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
        N_("Common options:"), NULL },

  POPT_AUTOALIAS
  POPT_AUTOHELP
  POPT_TABLEEND
};

int main(int argc, char *argv[])
{
    poptContext con = rpmcliInit(argc, argv, optionsTable);
    int ec = 0;

    inst("edos-test/tyre-1-0.noarch.rpm");
    inst("edos-test/wheel-2-0.noarch.rpm");
    inst("edos-test/tyre-2-0.noarch.rpm");
    inst("edos-test/wheel-3-0.noarch.rpm");

    con = rpmcliFini(con);

    return ec;
}

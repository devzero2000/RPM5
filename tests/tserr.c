#include "system.h"

#include <rpmio.h>
#include <rpmlog.h>
#include <rpmcb.h>
#include <popt.h>

#include <rpmtag.h>
#include <rpmdb.h>
#include <pkgio.h>

#include <rpmds.h>
#include <rpmts.h>
#include <rpmcli.h>

#include "debug.h"

static int inst(char *fname)
{
    int fdno;
    rpmts ts;
    rpmRC rc;
    Header h;
    FD_t fd;
    rpmps ps;
    int xx;
    int notifyFlags = 0;

    ts = rpmtsCreate();
    (void) rpmtsSetRootDir(ts, "/");
    (void) rpmtsSetVSFlags(ts, rpmExpandNumeric("%{?_vsflags}"));

    fdno = open(fname, O_RDONLY);
    fd = fdDup(fdno);
    rc = rpmReadPackageFile(ts, fd, "inst", &h);
    fprintf(stderr, "rpmReadPackageFile: %d\n", rc);

    close(fdno);
    Fclose(fd);

    rpmtsAddInstallElement(ts, h, fname, 1, NULL);

    xx = rpmtsCheck(ts);
    ps = rpmtsProblems(ts);

    rc = rpmtsOrder(ts);

    (void) rpmtsSetNotifyCallback(ts, rpmShowProgress,  (void *) ((long)notifyFlags));

    rc = rpmtsRun(ts, NULL, 0);
    fprintf(stderr, "rpmtsRun: %d\n", rc);
    ps = rpmtsProblems(ts);


    ps = rpmpsFree(ps);
    ts = rpmtsFree(ts);

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

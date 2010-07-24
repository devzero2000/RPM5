#include "system.h"

#include <rpmio.h>
#include <poptIO.h>

#define	_RPMTPM_INTERNAL
#include <rpmtpm.h>

#include "debug.h"

/*@access pgpDig @*/
/*@access pgpDigParams @*/

/*@-redecl@*/
/*@unchecked@*/
extern int _pgp_debug;

/*@unchecked@*/
extern int _pgp_print;
/*@=redecl@*/

static int use_fips;
static int selftest_only;

/*==============================================================*/
static struct poptOption optionsTable[] = {
 { "fips", '\0', POPT_ARG_VAL,          &use_fips, 1,
	N_("test in FIPS 140-2 mode"), NULL },
 { "selftest", '\0', POPT_ARG_VAL,      &selftest_only, 1,
	N_("test in FIPS 140-2 mode"), NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
        N_("Common options:"), NULL },

  POPT_AUTOHELP
  POPT_TABLEEND
};


int main(int argc, char *argv[])
{
    poptContext con = rpmioInit(argc, argv, optionsTable);
    rpmtpm tpm;
    int ec = 0;

    tpm = rpmtpmNew(NULL, 0);

    tpm = rpmtpmFree(tpm);

    con = rpmioFini(con);

    return ec;
}

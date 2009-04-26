#include "system.h"

#include <rpmio.h>
#include <argv.h>

#define	_RPMJS_INTERNAL
#include <rpmjs.h>

#include "rpmds-js.h"
#include "rpmfi-js.h"
#include "rpmte-js.h"
#include "rpmts-js.h"
#include "rpmmi-js.h"
#include "rpmps-js.h"
#include "rpmhdr-js.h"
#include "uuid-js.h"
#include "syck-js.h"
#include "rpmjsfile.h"

#include <rpmcli.h>

#include "debug.h"

static struct poptOption optionsTable[] = {

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
	N_("Common options for all rpm executables:"), NULL },

  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    poptContext optCon = rpmcliInit(argc, argv, optionsTable);
    ARGV_t av = poptGetArgs(optCon);
    int ac = argvCount(av);
    const char * fn;
    const char * str = NULL;
    const char * result;
    int rc = 1;		/* assume failure */

    if (ac < 1) {
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }

_rpmts_debug = 0;

    /* XXX FIXME: resultp != NULL to actually execute?!? */
    (void) rpmjsRun(NULL, "print(\"loading RPM classes:\");", &result);
    {	rpmjs js = _rpmjsI;
#if 0
	(void) rpmjs_InitSyckClass(js->cx, js->glob);
	(void) rpmjsRun(NULL, "print(\"\tSyck\");", &result);
	(void) rpmjs_InitUuidClass(js->cx, js->glob);
	(void) rpmjsRun(NULL, "print(\"\tUuid\");", &result);
#endif
	(void) rpmjs_InitHdrClass(js->cx, js->glob);
	(void) rpmjsRun(NULL, "print(\"\tHdr\");", &result);
	(void) rpmjs_InitMiClass(js->cx, js->glob);
	(void) rpmjsRun(NULL, "print(\"\tMi\");", &result);
	(void) rpmjs_InitPsClass(js->cx, js->glob);
	(void) rpmjsRun(NULL, "print(\"\tPs\");", &result);
	(void) rpmjs_InitTsClass(js->cx, js->glob);
	(void) rpmjsRun(NULL, "print(\"\tTs\");", &result);
	(void) rpmjs_InitTeClass(js->cx, js->glob);
	(void) rpmjsRun(NULL, "print(\"\tTe\");", &result);
	(void) rpmjs_InitDsClass(js->cx, js->glob);
	(void) rpmjsRun(NULL, "print(\"\tDs\");", &result);
	(void) rpmjs_InitFiClass(js->cx, js->glob);
	(void) rpmjsRun(NULL, "print(\"\tFi\");", &result);
	(void) js_InitFileClass(js->cx, js->glob);
	(void) rpmjsRun(NULL, "print(\"\tFile\");", &result);
    }

    while ((fn = *av++) != NULL) {
	rpmRC ret;
	str = rpmExpand("load(\"", fn, "\");", NULL);
	result = NULL;
	if ((ret = rpmjsRun(NULL, str, &result)) != RPMRC_OK)
	    goto exit;
	if (result != NULL && *result != '\0')
	    fprintf(stdout, "%s\n", result);
	str = _free(str);
    }

    rc = 0;

exit:
    str = _free(str);
    optCon = rpmcliFini(optCon);

    return rc;
}

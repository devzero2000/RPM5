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

/*@unchecked@*/
static int _debug = 1;

/*@unchecked@*/
static int _test = 1;

typedef struct rpmjsClassTable_s {
/*@observer@*/
    const char *name;
    JSObject * (*init) (JSContext *cx, JSObject *obj);
    int ix;
} * rpmjsClassTable;

/*@unchecked@*/ /*@observer@*/
static struct rpmjsClassTable_s classTable[] = {
    { "Ds",		rpmjs_InitDsClass,	13 },
    { "Fi",		rpmjs_InitFiClass,	14 },
    { "File",		   js_InitFileClass,	1 },
    { "Hdr",		rpmjs_InitHdrClass,	12 },
    { "Mi",		rpmjs_InitMiClass,	11 },
    { "Ps",		rpmjs_InitPsClass,	16 },
#ifdef	NOTYET
    { "Syck",		rpmjs_InitSyckClass,	0 },
#endif
    { "Te",		rpmjs_InitTeClass,	15 },
    { "Ts",		rpmjs_InitTsClass,	10 },
    { "Uuid",		rpmjs_InitUuidClass,	2 },
};

/*@unchecked@*/
static size_t nclassTable = sizeof(classTable) / sizeof(classTable[0]);

/*@unchecked@*/
static const char tscripts[] = "./tscripts/";

/*@unchecked@*/
static const char * _acknack = "\
function ack(cmd, expected) {\n\
  actual = eval(cmd);\n\
  if (actual != expected)\n\
    print(\"NACK: eval(\"+cmd+\") got '\"+actual+\"' not '\"+expected+\"'\");\n\
}\n\
function nack(cmd, expected) {\n\
  actual = eval(cmd);\n\
  if (actual == expected)\n\
    print(\" ACK: eval(\"+cmd+\") got '\"+actual+\"' not '\"+expected+\"'\");\n\
}\n\
";

static rpmRC
rpmjsLoadFile(rpmjs js, const char * pre, const char * fn)
{
    const char * result = NULL;
    char * str;
    rpmRC ret;

    if (pre == NULL)
	pre = "";
    str = rpmExpand(pre, "load(\"", fn, "\");", NULL);
if (_debug)
fprintf(stderr, "\trunning:\n%s\n", str);
    result = NULL;
    ret = rpmjsRun(NULL, str, &result);
    if (result != NULL && *result != '\0')
	fprintf(stdout, "%s\n", result);
    str = _free(str);
    return ret;
}

static void
rpmjsLoadClasses(void)
{
    const char * acknack = _acknack;
    int * order = NULL;
    size_t norder = 64;
    rpmjsClassTable tbl;
    rpmjs js;
    const char * result;
    int ix;
    size_t i;

    i = norder * sizeof(*order);
    order = memset(alloca(i), 0, i);

    /* XXX FIXME: resultp != NULL to actually execute?!? */
    (void) rpmjsRun(NULL, "print(\"loading RPM classes.\");", &result);
    js = _rpmjsI;
    for (i = 0, tbl = classTable; i < nclassTable; i++, tbl++) {
	if (tbl->ix <= 0)
	    continue;
if (_debug < 0)
fprintf(stderr, "==> order[%2d] = %2d %s\n", (tbl->ix & (norder -1)), i, tbl->name);
	order[tbl->ix & (norder - 1)] = i + 1;
    }

    for (i = 0; i < norder; i++) {
	const char * fn;
	struct stat sb;

	if (order[i] <= 0)
	    continue;
	ix = order[i] - 1;
	tbl = &classTable[ix];
if (_debug < 0)
fprintf(stderr, "<== order[%2d] = %2d %s\n", i, ix, tbl->name);
	if (tbl->init != NULL)
	    (void) (*tbl->init) (js->cx, js->glob);
	fn = rpmGetPath(tscripts, "/", tbl->name, ".js", NULL);
	if (Stat(fn, &sb) == 0) {
	    (void) rpmjsLoadFile(NULL, acknack, fn);
	    acknack = NULL;
	}
	fn = _free(fn);
    }

    return;
}

static struct poptOption optionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL,	&_debug, -1,		NULL, NULL },
 { "test", 'd', POPT_ARG_VAL,	&_test, -1,		NULL, NULL },

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
    int rc = 1;		/* assume failure */

    if (!_test && ac < 1) {
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }

_rpmts_debug = 0;

_rpmjs_debug = 0;
    rpmjsLoadClasses();
_rpmjs_debug = 1;

    if (av != NULL)
    while ((fn = *av++) != NULL) {
	rpmRC ret = rpmjsLoadFile(NULL, NULL, fn);
	if (ret != RPMRC_OK)
	    goto exit;
    }

    rc = 0;

exit:
    optCon = rpmcliFini(optCon);

    return rc;
}

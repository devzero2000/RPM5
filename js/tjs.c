#include "system.h"

#include <rpmio.h>
#include <rpmlog.h>
#include <rpmcb.h>
#include <argv.h>

#define	_RPMJS_INTERNAL
#include <rpmjs.h>

#include "rpmaug-js.h"
#include "rpmbf-js.h"
#include "rpmdc-js.h"
#include "rpmdir-js.h"
#include "rpmds-js.h"
#include "rpmfi-js.h"
#include "rpmfts-js.h"
#include "rpmhdr-js.h"
#include "rpmio-js.h"
#include "rpmmc-js.h"
#include "rpmmg-js.h"
#include "rpmmi-js.h"
#include "rpmps-js.h"
#include "rpmst-js.h"
#include "rpmte-js.h"
#include "rpmts-js.h"
#include "syck-js.h"
#include "uuid-js.h"

#include "rpmjsfile.h"

#include <rpmcli.h>

#include "debug.h"

/*@unchecked@*/
static int _debug = 0;

/*@unchecked@*/
static int _loglvl = 0;

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
    { "Aug",		rpmjs_InitAugClass,	 25 },
    { "Bf",		rpmjs_InitBfClass,	 26 },
    { "Dc",		rpmjs_InitDcClass,	 28 },
    { "Dir",		rpmjs_InitDirClass,	 29 },
    { "Ds",		rpmjs_InitDsClass,	 13 },
    { "Fi",		rpmjs_InitFiClass,	 14 },
    { "File",		   js_InitFileClass,	  1 },
    { "Fts",		rpmjs_InitFtsClass,	 30 },
    { "Hdr",		rpmjs_InitHdrClass,	 12 },
    { "Io",		rpmjs_InitIoClass,	 32 },
    { "Mc",		rpmjs_InitMcClass,	 24 },
    { "Mg",		rpmjs_InitMgClass,	 31 },
    { "Mi",		rpmjs_InitMiClass,	 11 },
    { "Ps",		rpmjs_InitPsClass,	 16 },
    { "St",		rpmjs_InitStClass,	 27 },
#ifdef WITH_SYCK
    { "Syck",		rpmjs_InitSyckClass,	 -3 },
#endif
    { "Te",		rpmjs_InitTeClass,	 15 },
    { "Ts",		rpmjs_InitTsClass,	 10 },
#ifdef WITH_UUID
    { "Uuid",		rpmjs_InitUuidClass,	  2 },
#endif
};

/*@unchecked@*/
static size_t nclassTable = sizeof(classTable) / sizeof(classTable[0]);

/*@unchecked@*/
static const char tscripts[] = "./tscripts/";

/*@unchecked@*/
static const char * _acknack = "\
function ack(cmd, expected) {\n\
  try {\n\
    actual = eval(cmd);\n\
  } catch(e) {\n\
    print(\"NACK:  ack(\"+cmd+\")\tcaught '\"+e+\"'\");\n\
    return;\n\
  }\n\
  if (actual != expected && expected != undefined)\n\
    print(\"NACK:  ack(\"+cmd+\")\tgot '\"+actual+\"' not '\"+expected+\"'\");\n\
  else if (loglvl)\n\
    print(\"       ack(\"+cmd+\")\tgot '\"+actual+\"'\");\n\
}\n\
function nack(cmd, expected) {\n\
  try {\n\
    actual = eval(cmd);\n\
  } catch(e) {\n\
    print(\" ACK: nack(\"+cmd+\")\tcaught '\"+e+\"'\");\n\
    return;\n\
  }\n\
  if (actual == expected)\n\
    print(\" ACK: nack(\"+cmd+\")\tgot '\"+actual+\"' not '\"+expected+\"'\");\n\
  else if (loglvl)\n\
    print(\"      nack(\"+cmd+\")\tgot '\"+actual+\"'\");\n\
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
fprintf(stderr, "\trunning:%s%s\n", (*pre ? "\n" : " "), str);
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
    const char * pre = NULL;
    int * order = NULL;
    size_t norder = 64;
    rpmjsClassTable tbl;
    rpmjs js;
    const char * result;
    int ix;
    size_t i;

    i = norder * sizeof(*order);
    order = memset(alloca(i), 0, i);

    /* Inject _debug and _loglvl into the interpreter context. */
    {	char dstr[32];
	char lstr[32];
	sprintf(dstr, "%d", _debug);
	sprintf(lstr, "%d", _loglvl);
	pre = rpmExpand("var debug = ", dstr, ";\n"
			"var loglvl = ", lstr, ";\n",
			_acknack, NULL);
    }

    /* Load requested classes and initialize the test order. */
    /* XXX FIXME: resultp != NULL to actually execute?!? */
    (void) rpmjsRun(NULL, "print(\"loading RPM classes.\");", &result);
    js = _rpmjsI;
    for (i = 0, tbl = classTable; i < nclassTable; i++, tbl++) {
	if (tbl->ix <= 0)
	    continue;
	order[tbl->ix & (norder - 1)] = i + 1;
	if (tbl->init != NULL)
	    (void) (*tbl->init) (js->cx, js->glob);
    }

    /* Test requested classes in order. */
    for (i = 0; i < norder; i++) {
	const char * fn;
	struct stat sb;

	if (order[i] <= 0)
	    continue;
	ix = order[i] - 1;
	tbl = &classTable[ix];
	fn = rpmGetPath(tscripts, "/", tbl->name, ".js", NULL);
	if (Stat(fn, &sb) == 0) {
	    (void) rpmjsLoadFile(NULL, pre, fn);
	    pre = _free(pre);
	}
	fn = _free(fn);
    }

    pre = _free(pre);
    return;
}

static struct poptOption optionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL,	&_debug, -1,		NULL, NULL },
 { "test", 't', POPT_ARG_VAL,	&_test, -1,		NULL, NULL },

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

if (_debug) {
rpmSetVerbosity(RPMLOG_DEBUG);
} else {
rpmSetVerbosity(RPMLOG_NOTICE);
}

_rpmjs_debug = 0;
    if (_debug && !_loglvl) _loglvl = 1;
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
_rpmjs_debug = 0;
    optCon = rpmcliFini(optCon);

    return rc;
}

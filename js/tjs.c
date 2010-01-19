#include "system.h"

#include <rpmio.h>
#include <rpmlog.h>
#include <rpmcb.h>
#include <argv.h>

#define	_RPMJS_INTERNAL
#include <rpmjs.h>

#include "rpmaug-js.h"
#include "rpmbc-js.h"
#include "rpmbf-js.h"

#include "rpmcudf-js.h"

#include "rpmdb-js.h"
#include "rpmdbc-js.h"
#include "rpmdbe-js.h"
#include "rpmmpf-js.h"
#include "rpmseq-js.h"
#include "rpmtxn-js.h"

#include "rpmdc-js.h"
#include "rpmdig-js.h"
#include "rpmdir-js.h"
#include "rpmds-js.h"
#include "rpmfc-js.h"
#include "rpmfi-js.h"
#include "rpmfts-js.h"
#include "rpmgi-js.h"
#include "rpmhdr-js.h"
#include "rpmio-js.h"
#include "rpmiob-js.h"
#include "rpmmc-js.h"
#include "rpmmg-js.h"
#include "rpmmi-js.h"
#include "rpmmpw-js.h"
#include "rpmps-js.h"
#include "rpmsm-js.h"
#include "rpmsp-js.h"
#include "rpmst-js.h"
#include "rpmsw-js.h"
#include "rpmsx-js.h"
#include "rpmsys-js.h"
#include "rpmte-js.h"
#include "rpmts-js.h"
#include "rpmxar-js.h"
#include "syck-js.h"
#include "uuid-js.h"

#include <rpmcli.h>

#include "debug.h"

/*@unchecked@*/
static int _debug = 0;

/*@unchecked@*/
static int _loglvl = 0;

/*@unchecked@*/
static int _test = 1;

/*@unchecked@*/
static int _zeal = 2;

#if defined(WITH_GPSEE)
#include <gpsee.h>
typedef	gpsee_interpreter_t * JSI_t;
#else
typedef struct JSI_s {
    JSRuntime	* rt;
    JSContext	* cx;
    JSObject	* globalObj;
} * JSI_t;
#endif

typedef struct rpmjsClassTable_s {
/*@observer@*/
    const char *name;
    JSObject * (*init) (JSContext *cx, JSObject *obj);
    unsigned flags;
    int ix;
} * rpmjsClassTable;

/*@unchecked@*/ /*@observer@*/
static struct rpmjsClassTable_s classTable[] = {
    { "Aug",		rpmjs_InitAugClass,	 1,	-25 },
    { "Bc",		rpmjs_InitBcClass,	 1,	-40 },	/* todo++ */
    { "Bf",		rpmjs_InitBfClass,	 1,	-26 },

    { "Cudf",		rpmjs_InitCudfClass,	 1,	-50 },	/* todo++ */

    { "Db",		rpmjs_InitDbClass,	 1,	-45 },
    { "Dbc",		rpmjs_InitDbcClass,	 1,	-46 },
    { "Dbe",		rpmjs_InitDbeClass,	 1,	-44 },
    { "Mpf",		rpmjs_InitMpfClass,	 1,	-48 },
    { "Seq",		rpmjs_InitSeqClass,	 1,	-49 },
    { "Txn",		rpmjs_InitTxnClass,	 1,	-47 },

    { "Dc",		rpmjs_InitDcClass,	 1,	-28 },
    { "Dig",		rpmjs_InitDigClass,	 1,	-37 },	/* todo++ */
    { "Dir",		rpmjs_InitDirClass,	 1,	-29 },
    { "Ds",		rpmjs_InitDsClass,	 1,	-13 },
    { "Fc",		rpmjs_InitFcClass,	 1,	-34 },	/* todo++ */
    { "Fi",		rpmjs_InitFiClass,	 1,	-14 },
    { "Fts",		rpmjs_InitFtsClass,	 1,	-30 },
    { "Gi",		rpmjs_InitGiClass,	 1,	-35 },	/* todo++ */
    { "Hdr",		rpmjs_InitHdrClass,	 1,	-12 },
    { "Io",		rpmjs_InitIoClass,	 1,	-32 },
    { "Iob",		rpmjs_InitIobClass,	 1,	-36 },	/* todo++ */
    { "Mc",		rpmjs_InitMcClass,	 1,	-24 },
    { "Mg",		rpmjs_InitMgClass,	 1,	-31 },
    { "Mi",		rpmjs_InitMiClass,	 1,	-11 },
    { "Mpw",		rpmjs_InitMpwClass,	 1,	-41 },
    { "Ps",		rpmjs_InitPsClass,	 1,	-16 },
    { "Sm",		rpmjs_InitSmClass,	 1,	-43 },	/* todo++ */
    { "Sp",		rpmjs_InitSpClass,	 1,	-42 },	/* todo++ */
    { "St",		rpmjs_InitStClass,	 1,	27 },
    { "Sw",		rpmjs_InitSwClass,	 1,	-38 },	/* todo++ */
    { "Sx",		rpmjs_InitSxClass,	 1,	-39 },
#if defined(WITH_SYCK)
    { "Syck",		rpmjs_InitSyckClass,	 1,	 -3 },	/* todo++ */
#endif
    { "Sys",		rpmjs_InitSysClass,	 1,	33 },
    { "Te",		rpmjs_InitTeClass,	 1,	-15 },
    { "Ts",		rpmjs_InitTsClass,	 1,	-10 },
    { "Xar",		rpmjs_InitXarClass,	 1,	-51 },	/* todo++ */
#if defined(WITH_UUID)
    { "Uuid",		rpmjs_InitUuidClass,	 1,	 -2 },
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
rpmjsLoadFile(const char * pre, const char * fn, unsigned flags, int bingo)
{
    char * str;
    rpmRC ret = RPMRC_FAIL;
    const char * result = NULL;

#if defined(WITH_GPSEE)
    if (flags) {
	rpmjs js = _rpmjsI;

	if (pre == NULL)
	    pre = "";
	str = rpmExpand(pre, "\ntry { require('system').include('", fn, "'); } catch(e) { print(e); print(e.stack); throw e; }", NULL);
if (_debug)
fprintf(stderr, "\trunning:\n%s\n", str);

	result = NULL;
	ret = rpmjsRun(NULL, str, &result);

	if (result != NULL && *result != '\0') {
	    fprintf(stdout, "%s\n", result);
	    fflush(stdout);
	}
	str = _free(str);
    } else
#endif
    {
	if (bingo || pre == NULL)
	    pre = "";
	str = rpmExpand(pre, "load(\"", fn, "\");", NULL);
if (_debug)
fprintf(stderr, "\trunning:\n%s%s\n", (*pre ? "\n" : " "), str);
	result = NULL;
	ret = rpmjsRun(NULL, str, &result);
	if (result != NULL && *result != '\0') {
	    fprintf(stdout, "%s\n", result);
	    fflush(stdout);
	}
	str = _free(str);
    }
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
    JSI_t I;
    const char * result;
    int ix;
    size_t i;
    int bingo = 0;

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
    I = js->I;
#ifdef JS_GC_ZEAL
    (void) JS_SetGCZeal(I->cx, _zeal);
#endif

    for (i = 0, tbl = classTable; i < nclassTable; i++, tbl++) {
	if (tbl->ix <= 0)
	    continue;
	order[tbl->ix & (norder - 1)] = i + 1;
	if (!tbl->flags && tbl->init != NULL)
	    (void) (*tbl->init) (I->cx, I->globalObj);
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
	    (void) rpmjsLoadFile(pre, fn, tbl->flags, bingo);
	    bingo = 1;
	}
	fn = _free(fn);
    }

    pre = _free(pre);
    return;
}

static struct poptOption optionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL,	&_debug, -1,		NULL, NULL },
 { "test", 't', POPT_ARG_VAL,	&_test, -1,		NULL, NULL },
 { "zeal", 'z', POPT_ARG_INT,	&_zeal, 0,		NULL, NULL },

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
	rpmRC ret = rpmjsLoadFile(NULL, fn, 0, 1);
	if (ret != RPMRC_OK)
	    goto exit;
    }

    rc = 0;

exit:
_rpmjs_debug = 0;
    optCon = rpmcliFini(optCon);

    return rc;
}

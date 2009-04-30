#include "system.h"

#include <rpmio.h>
#include <argv.h>

#define	_RPMRUBY_INTERNAL
#include <rpmruby.h>

#ifdef	NOTYET
#include "rpmds-rb.h"
#include "rpmfi-rb.h"
#include "rpmhdr-rb.h"
#include "rpmmc-rb.h"
#include "rpmmi-rb.h"
#include "rpmps-rb.h"
#include "rpmte-rb.h"
#include "rpmts-rb.h"
#include "syck-rb.h"
#include "uuid-rb.h"
#endif

#include <rpmcli.h>

#include "debug.h"

/*@unchecked@*/
static int _debug = 0;

/*@unchecked@*/
static int _loglvl = 0;

/*@unchecked@*/
static int _test = 1;

typedef struct rpmrbClassTable_s {
/*@observer@*/
    const char *name;
    void * (*init) (void * _foocx, void * _fooglob);
    int ix;
} * rpmrbClassTable;

/*@unchecked@*/ /*@observer@*/
static struct rpmrbClassTable_s classTable[] = {
};

/*@unchecked@*/
static size_t nclassTable = sizeof(classTable) / sizeof(classTable[0]);

/*@unchecked@*/
static const char tscripts[] = "./tscripts/";

/*@unchecked@*/
static const char * _acknack = "\
";

static rpmRC
rpmrbLoadFile(rpmruby rb, const char * pre, const char * fn)
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
    ret = rpmrubyRun(NULL, str, &result);
    if (result != NULL && *result != '\0')
	fprintf(stdout, "%s\n", result);
    str = _free(str);
    return ret;
}

static void
rpmrbLoadClasses(void)
{
    const char * pre = NULL;
    int * order = NULL;
    size_t norder = 64;
    rpmrbClassTable tbl;
    rpmruby rb;
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
    (void) rpmrubyRun(NULL, "print(\"loading RPM classes.\");", &result);
    rb = _rpmrubyI;
    for (i = 0, tbl = classTable; i < nclassTable; i++, tbl++) {
	if (tbl->ix <= 0)
	    continue;
	order[tbl->ix & (norder - 1)] = i + 1;
#ifdef	NOTYET
	if (tbl->init != NULL)
	    (void) (*tbl->init) (rb->cx, rb->glob);
#endif
    }

    /* Test requested classes in order. */
    for (i = 0; i < norder; i++) {
	const char * fn;
	struct stat sb;

	if (order[i] <= 0)
	    continue;
	ix = order[i] - 1;
	tbl = &classTable[ix];
	fn = rpmGetPath(tscripts, "/", tbl->name, ".rb", NULL);
	if (Stat(fn, &sb) == 0) {
	    (void) rpmrbLoadFile(NULL, pre, fn);
	    pre = _free(pre);
	}
	fn = _free(fn);
    }

    pre = _free(pre);
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

_rpmruby_debug = 0;
    if (_debug && !_loglvl) _loglvl = 1;
    rpmrbLoadClasses();
_rpmruby_debug = 1;

    if (av != NULL)
    while ((fn = *av++) != NULL) {
	rpmRC ret = rpmrbLoadFile(NULL, NULL, fn);
	if (ret != RPMRC_OK)
	    goto exit;
    }

    rc = 0;

exit:
_rpmruby_debug = 0;
    optCon = rpmcliFini(optCon);

    return rc;
}

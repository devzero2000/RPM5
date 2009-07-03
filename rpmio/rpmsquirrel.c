#include "system.h"

#include <argv.h>

#ifdef	WITH_SQUIRREL
#include <squirrel.h>
#endif
#define _RPMSQUIRREL_INTERNAL
#include "rpmsquirrel.h"

#include "debug.h"

/*@unchecked@*/
int _rpmsquirrel_debug = 0;

/*@unchecked@*/ /*@relnull@*/
rpmsquirrel _rpmsquirrelI = NULL;

static void rpmsquirrelFini(void * _squirrel)
        /*@globals fileSystem @*/
        /*@modifies *_squirrel, fileSystem @*/
{
    rpmsquirrel squirrel = _squirrel;

#if defined(WITH_SQUIRREL)
    sq_close((HSQUIRRELVM)squirrel->I);
#endif
    squirrel->I = NULL;
    (void)rpmiobFree(squirrel->iob);
    squirrel->iob = NULL;
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmsquirrelPool;

static rpmsquirrel rpmsquirrelGetPool(/*@null@*/ rpmioPool pool)
        /*@globals _rpmsquirrelPool, fileSystem @*/
        /*@modifies pool, _rpmsquirrelPool, fileSystem @*/
{
    rpmsquirrel squirrel;

    if (_rpmsquirrelPool == NULL) {
        _rpmsquirrelPool = rpmioNewPool("squirrel", sizeof(*squirrel), -1, _rpmsquirrel_debug,
                        NULL, NULL, rpmsquirrelFini);
        pool = _rpmsquirrelPool;
    }
    return (rpmsquirrel) rpmioGetPool(pool, sizeof(*squirrel));
}

#if defined(WITH_SQUIRREL)
static void rpmsquirrelPrint(HSQUIRRELVM v, const SQChar *s, ...)
{
    rpmsquirrel squirrel = sq_getforeignptr(v);
    size_t nb = 1024;
    char * b = xmalloc(nb);
    va_list va;

    va_start(va, s);
    while(1) {
	int nw = vsnprintf(b, nb, s, va);
	if (nw > -1 && (size_t)nw < nb)
	    break;
	if (nw > -1)		/* glibc 2.1 (and later) */
	    nb = nw+1;
	else			/* glibc 2.0 */
	    nb *= 2;
	b = xrealloc(b, nb);
    }
    va_end(va);

    (void) rpmiobAppend(squirrel->iob, b, 0);
    b = _free(b);
}
#endif

rpmsquirrel rpmsquirrelNew(const char ** av, int flags)
{
    rpmsquirrel squirrel = rpmsquirrelGetPool(_rpmsquirrelPool);

#if defined(WITH_SQUIRREL)
    static const char * _av[] = { "rpmsquirrel", NULL };
    SQInteger stacksize = 1024;
    HSQUIRRELVM v = sq_open(stacksize);
    int ac;

    if (av == NULL) av = _av;
    ac = argvCount(av);

    squirrel->I = v;
    sq_setforeignptr(v, squirrel);
    sq_setprintfunc(v, rpmsquirrelPrint);

#ifdef	NOTYET
    {	int i;
	sq_pushroottable(v);
	sc_pushstring(v, "ARGS", -1);
	sq_newarray(v, 0);
	for (i = 0, i < ac; i++) {
	    sq_pushstring(v, av[i], -1);
	    sq_arrayappend(v, -2);
	}
	sq_createslot(v, -3);
	sq_pop(v, 1);
    }
#endif
#endif
    squirrel->iob = rpmiobNew(0);

    return rpmsquirrelLink(squirrel);
}

static rpmsquirrel rpmsquirrelI(void)
	/*@globals _rpmsquirrelI @*/
	/*@modifies _rpmsquirrelI @*/
{
    if (_rpmsquirrelI == NULL)
	_rpmsquirrelI = rpmsquirrelNew(NULL, 0);
    return _rpmsquirrelI;
}

rpmRC rpmsquirrelRunFile(rpmsquirrel squirrel, const char * fn, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

if (_rpmsquirrel_debug)
fprintf(stderr, "==> %s(%p,%s)\n", __FUNCTION__, squirrel, fn);

    if (squirrel == NULL) squirrel = rpmsquirrelI();

#if defined(NOTYET)
    if (fn != NULL && Tcl_EvalFile((Tcl_Interp *)squirrel->I, fn) == SQUIRREL_OK) {
	rc = RPMRC_OK;
	if (resultp)
	    *resultp = rpmiobStr(squirrel->iob);
    }
#endif
    return rc;
}

rpmRC rpmsquirrelRun(rpmsquirrel squirrel, const char * str, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

if (_rpmsquirrel_debug)
fprintf(stderr, "==> %s(%p,%s)\n", __FUNCTION__, squirrel, str);

    if (squirrel == NULL) squirrel = rpmsquirrelI();

#if defined(WITH_SQUIRREL)
    if (str != NULL) {
	size_t ns = strlen(str);
	if (ns > 0) {
	    HSQUIRRELVM v = squirrel->I;
	    SQBool raise = SQFalse;
	    SQInteger oldtop = sq_gettop(v);
	    SQRESULT res = sq_compilebuffer(v, str, ns, __FUNCTION__, raise);

	    if (SQ_SUCCEEDED(res)) {
		SQInteger retval = 0;
		sq_pushroottable(v);
		res = sq_call(v, 1, retval, raise);
	    }

	    sq_settop(v, oldtop);
	}
	rc = RPMRC_OK;
	if (resultp)
	    *resultp = rpmiobStr(squirrel->iob);
    }
#endif
    return rc;
}

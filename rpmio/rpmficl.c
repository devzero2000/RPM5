#include "system.h"

#include <argv.h>

#ifdef	WITH_FICL
#include <ficl.h>
#endif
#define _RPMFICL_INTERNAL
#include "rpmficl.h"

#include "debug.h"

/*@unchecked@*/
int _rpmficl_debug = 1;

/*@unchecked@*/ /*@relnull@*/
rpmficl _rpmficlI = NULL;

static void rpmficlFini(void * _ficl)
        /*@globals fileSystem @*/
        /*@modifies *_ficl, fileSystem @*/
{
    rpmficl ficl = _ficl;

#if defined(WITH_FICL)
    ficlSystem * sys = ficl->sys;
    ficlSystemDestroy(sys);
#endif
    ficl->vm = NULL;
    ficl->sys = NULL;
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmficlPool;

static rpmficl rpmficlGetPool(/*@null@*/ rpmioPool pool)
        /*@globals _rpmficlPool, fileSystem @*/
        /*@modifies pool, _rpmficlPool, fileSystem @*/
{
    rpmficl ficl;

    if (_rpmficlPool == NULL) {
        _rpmficlPool = rpmioNewPool("ficl", sizeof(*ficl), -1, _rpmficl_debug,
                        NULL, NULL, rpmficlFini);
        pool = _rpmficlPool;
    }
    return (rpmficl) rpmioGetPool(pool, sizeof(*ficl));
}

rpmficl rpmficlNew(const char ** av, int flags)
{
    rpmficl ficl = rpmficlGetPool(_rpmficlPool);

#if defined(WITH_FICL)
    static const char * _av[] = { "rpmficl", NULL };
    ficlSystem *sys;
    ficlVm *vm;
    int ac;
    int xx;

    if (av == NULL) av = _av;
    ac = argvCount(av);

    sys = ficlSystemCreate(NULL);
    ficl->sys = sys;
    ficlSystemCompileExtras(sys);
    vm = ficlSystemCreateVm(sys);
    ficl->vm = vm;

    xx = ficlVmEvaluate(vm, ".ver .( " __DATE__ " ) cr quit");
#ifdef	NOTYET
    if (ac > 1) {
	char b[256];
        sprintf(b, ".( loading %s ) cr load %s\n cr", av[1], av[1]);
        xx = ficlVmEvaluate(vm, b);
    }
#endif

#endif

    return rpmficlLink(ficl);
}

static rpmficl rpmficlI(void)
	/*@globals _rpmficlI @*/
	/*@modifies _rpmficlI @*/
{
    if (_rpmficlI == NULL)
	_rpmficlI = rpmficlNew(NULL, 0);
    return _rpmficlI;
}

rpmRC rpmficlRunFile(rpmficl ficl, const char * fn, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

if (_rpmficl_debug)
fprintf(stderr, "==> %s(%p,%s)\n", __FUNCTION__, ficl, fn);

    if (ficl == NULL) ficl = rpmficlI();

    if (fn != NULL) {
#if defined(WITH_FICL)
	rc = RPMRC_OK;
#endif
    }
    return rc;
}

rpmRC rpmficlRun(rpmficl ficl, const char * str, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

if (_rpmficl_debug)
fprintf(stderr, "==> %s(%p,%s)\n", __FUNCTION__, ficl, str);

    if (ficl == NULL) ficl = rpmficlI();

    if (str != NULL) {
#if defined(WITH_FICL)
	ficlVm *vm = ficl->vm;
	int xx = ficlVmEvaluate(vm, (char *)str);
	rc = (xx ? RPMRC_OK : RPMRC_FAIL);
#endif
    }
    return rc;
}

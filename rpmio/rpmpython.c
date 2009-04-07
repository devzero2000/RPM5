#include "system.h"

#define _RPMPYTHON_INTERNAL
#include "rpmpython.h"

#include "debug.h"

/*@unchecked@*/
int _rpmpython_debug = 0;

static void rpmpythonFini(void * _python)
        /*@globals fileSystem @*/
        /*@modifies *_python, fileSystem @*/
{
    rpmpython python = _python;

    python->fn = _free(python->fn);
    python->flags = 0;
#if defined(WITH_PYTHONEMBED)
    Py_Finalize();
#endif
    python->I = NULL;
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmpythonPool;

static rpmpython rpmpythonGetPool(/*@null@*/ rpmioPool pool)
        /*@globals _rpmpythonPool, fileSystem @*/
        /*@modifies pool, _rpmpythonPool, fileSystem @*/
{
    rpmpython python;

    if (_rpmpythonPool == NULL) {
        _rpmpythonPool = rpmioNewPool("python", sizeof(*python), -1, _rpmpython_debug,
                        NULL, NULL, rpmpythonFini);
        pool = _rpmpythonPool;
    }
    return (rpmpython) rpmioGetPool(pool, sizeof(*python));
}

rpmpython rpmpythonNew(const char * fn, int flags)
{
    rpmpython python = rpmpythonGetPool(_rpmpythonPool);

    if (fn)
	python->fn = xstrdup(fn);
    python->flags = flags;

#if defined(WITH_PYTHONEMBED)
    Py_Initialize();
#endif

    return rpmpythonLink(python);
}

rpmRC rpmpythonRunFile(rpmpython python, const char * fn, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

if (_rpmpython_debug)
fprintf(stderr, "==> %s(%p,%s)\n", __FUNCTION__, python, fn);

    if (fn != NULL) {
#if defined(WITH_PYTHONEMBED)
	FILE * fp = fopen(fn, "rb");
	if (fp != NULL) {
	    PyRun_SimpleFileExFlags(fp, fn, 1, NULL);
	    rc = RPMRC_OK;
	}
#endif
#ifdef	NOTYET
	if (resultp)
	    *resultp = Tcl_GetStringResult(python->I);
#endif
    }
exit:
    return rc;
}

rpmRC rpmpythonRun(rpmpython python, const char * str, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

if (_rpmpython_debug)
fprintf(stderr, "==> %s(%p,%s)\n", __FUNCTION__, python, str);

    if (str != NULL) {
#if defined(WITH_PYTHONEMBED)
	PyRun_SimpleStringFlags(str, NULL);
	rc = RPMRC_OK;
#ifdef	NOTYET
	if (resultp)
	    *resultp = Tcl_GetStringResult(python->I);
#endif
#endif
    }
    return rc;
}

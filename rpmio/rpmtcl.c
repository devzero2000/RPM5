#include "system.h"

#ifdef	WITH_TCL
#define _RPMTCL_INTERNAL
#include "rpmtcl.h"

#include "debug.h"

/*@unchecked@*/
int _rpmtcl_debug = 0;

static void rpmtclFini(void * _tcl)
        /*@globals fileSystem @*/
        /*@modifies *_tcl, fileSystem @*/
{
    rpmtcl tcl = _tcl;

    tcl->fn = _free(tcl->fn);
    tcl->flags = 0;
#if defined(WITH_TCL)
    Tcl_DeleteInterp(tcl->I);
#endif
    tcl->I = NULL;
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmtclPool;

static rpmtcl rpmtclGetPool(/*@null@*/ rpmioPool pool)
        /*@globals _rpmtclPool, fileSystem @*/
        /*@modifies pool, _rpmtclPool, fileSystem @*/
{
    rpmtcl tcl;

    if (_rpmtclPool == NULL) {
        _rpmtclPool = rpmioNewPool("tcl", sizeof(*tcl), -1, _rpmtcl_debug,
                        NULL, NULL, rpmtclFini);
        pool = _rpmtclPool;
    }
    return (rpmtcl) rpmioGetPool(pool, sizeof(*tcl));
}

rpmtcl rpmtclNew(const char * fn, int flags)
{
    rpmtcl tcl = rpmtclGetPool(_rpmtclPool);

    if (fn)
	tcl->fn = xstrdup(fn);
    tcl->flags = flags;

    tcl->I = Tcl_CreateInterp();

    return rpmtclLink(tcl);
}

rpmRC rpmtclRunFile(rpmtcl tcl, const char * fn, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

if (_rpmtcl_debug)
fprintf(stderr, "==> %s(%p,%s)\n", __FUNCTION__, tcl, fn);

    if (fn != NULL && Tcl_EvalFile(tcl->I, fn) == TCL_OK) {
	rc = RPMRC_OK;
	if (resultp)
	    *resultp = Tcl_GetStringResult(tcl->I); /* XXX Tcl_GetObjResult */
    }
    return rc;
}

rpmRC rpmtclRun(rpmtcl tcl, const char * str, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

if (_rpmtcl_debug)
fprintf(stderr, "==> %s(%p,%s)\n", __FUNCTION__, tcl, str);

    if (str != NULL && Tcl_Eval(tcl->I, str) == TCL_OK) {
	rc = RPMRC_OK;
	if (resultp)
	    *resultp = Tcl_GetStringResult(tcl->I); /* XXX Tcl_GetObjResult */
    }
    return rc;
}
#endif	/* WITH_TCL */

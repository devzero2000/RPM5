#include "system.h"

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
/*@=mustmod@*/

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

rpmRC rpmtclRunFile(rpmtcl tcl, const char * fn)
{
    int rc;

if (_rpmtcl_debug)
fprintf(stderr, "==> %s(%p,%s)\n", __FUNCTION__, tcl, fn);

    rc = (fn != NULL ? Tcl_EvalFile(tcl->I, fn) : 0);
    return (rc == TCL_OK ? RPMRC_OK : RPMRC_FAIL);
}

/** \ingroup rpmio
 * \file rpmio/rpmtpm.c
 */

#include "system.h"

#if defined(WITH_TPM)
#include <tpm/tpm.h>
#endif

#include <rpmiotypes.h>
#include <rpmio.h>	/* for *Pool methods */
#define	_RPMTPM_INTERNAL
#include <rpmtpm.h>

#include "debug.h"

/*@unchecked@*/
int _rpmtpm_debug = 0;

/*@-mustmod@*/	/* XXX splint on crack */
static void rpmtpmFini(void * _tpm)
	/*@globals fileSystem @*/
	/*@modifies *_tpm, fileSystem @*/
{
    rpmtpm tpm = _tpm;

    tpm->fn = _free(tpm->fn);
}
/*@=mustmod@*/

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmtpmPool = NULL;

static rpmtpm rpmtpmGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmtpmPool, fileSystem @*/
	/*@modifies pool, _rpmtpmPool, fileSystem @*/
{
    rpmtpm tpm;

    if (_rpmtpmPool == NULL) {
	_rpmtpmPool = rpmioNewPool("tpm", sizeof(*tpm), -1, _rpmtpm_debug,
			NULL, NULL, rpmtpmFini);
	pool = _rpmtpmPool;
    }
    return (rpmtpm) rpmioGetPool(pool, sizeof(*tpm));
}

rpmtpm rpmtpmNew(const char * fn, int flags)
{
    rpmtpm tpm = rpmtpmGetPool(_rpmtpmPool);
    int xx;

    if (fn)
	tpm->fn = xstrdup(fn);

    return rpmtpmLink(tpm);
}

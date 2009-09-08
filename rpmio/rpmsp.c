/** \ingroup rpmio
 * \file rpmio/rpmsp.c
 */

#include "system.h"

#if defined(WITH_SEPOL)
#include <sepol/sepol.h>
#endif

#define	_RPMSP_INTERNAL
#include <rpmsp.h>
#include <rpmlog.h>
#include <rpmmacro.h>

#include "debug.h"

/*@unchecked@*/
int _rpmsp_debug = 0;

static void rpmspFini(void * _sm)
	/*@globals fileSystem @*/
	/*@modifies *_sm, fileSystem @*/
{
    rpmsp sm = _sm;

#if defined(WITH_SEPOL)
#endif
    sm->fn = _free(sm->fn);
    sm->flags = 0;
    sm->I = NULL;
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmspPool = NULL;

static rpmsp rpmspGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmspPool, fileSystem @*/
	/*@modifies pool, _rpmspPool, fileSystem @*/
{
    rpmsp sm;

    if (_rpmspPool == NULL) {
	_rpmspPool = rpmioNewPool("sm", sizeof(*sm), -1, _rpmsp_debug,
			NULL, NULL, rpmspFini);
	pool = _rpmspPool;
    }
    return (rpmsp) rpmioGetPool(pool, sizeof(*sm));
}

rpmsp rpmspNew(const char * fn, unsigned int flags)
{
    rpmsp sm = rpmspGetPool(_rpmspPool);

#if defined(WITH_SEPOL)
#endif

    return rpmspLink(sm);
}

/*@unchecked@*/ /*@null@*/
static const char * _rpmspI_fn;
/*@unchecked@*/
static int _rpmspI_flags;

static rpmsp rpmspI(void)
	/*@globals _rpmspI @*/
	/*@modifies _rpmspI @*/
{
    if (_rpmspI == NULL)
	_rpmspI = rpmspNew(_rpmspI_fn, _rpmspI_flags);
    return _rpmspI;
}

#ifdef	REFERENCE	/* <sepol/sepol.h> */
#endif

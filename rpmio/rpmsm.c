/** \ingroup rpmio
 * \file rpmio/rpmsm.c
 */

#include "system.h"

#if defined(WITH_SELINUX)
#include <selinux/selinux.h>
#endif
#if defined(WITH_SEMANAGE)
#include <semanage/semanage.h>
#endif

#define	_RPMSM_INTERNAL
#include <rpmsm.h>
#include <rpmsx.h>
#include <rpmlog.h>
#include <rpmmacro.h>

#include "debug.h"

/*@unchecked@*/
int _rpmsm_debug = 0;

static void rpmsmFini(void * _sm)
	/*@globals fileSystem @*/
	/*@modifies *_sm, fileSystem @*/
{
    rpmsm sm = _sm;

#if defined(WITH_SEMANAGE)
#endif
    sm->flags = 0;
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmsmPool = NULL;

static rpmsm rpmsmGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmsmPool, fileSystem @*/
	/*@modifies pool, _rpmsmPool, fileSystem @*/
{
    rpmsm sm;

    if (_rpmsmPool == NULL) {
	_rpmsmPool = rpmioNewPool("sm", sizeof(*sm), -1, _rpmsm_debug,
			NULL, NULL, rpmsmFini);
	pool = _rpmsmPool;
    }
    return (rpmsm) rpmioGetPool(pool, sizeof(*sm));
}

rpmsm rpmsmNew(const char * fn, unsigned int flags)
{
    rpmsm sm = rpmsmGetPool(_rpmsmPool);

    sm->flags = flags;

#if defined(WITH_SEMANAGE)
#endif
    return rpmsmLink(sm);
}

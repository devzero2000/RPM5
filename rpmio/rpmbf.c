/** \ingroup rpmio
 * \file rpmio/rpmbf.c
 */

#include "system.h"

#include <rpmiotypes.h>
#include <rpmio.h>	/* for *Pool methods */
#include <rpmlog.h>
#define	_RPMBF_INTERNAL
#include <rpmbf.h>

#include "debug.h"

/*@unchecked@*/
int _rpmbf_debug = 0;

/*@-mustmod@*/	/* XXX splint on crack */
static void rpmbfFini(void * _bf)
	/*@globals fileSystem @*/
	/*@modifies *_bf, fileSystem @*/
{
    rpmbf bf = _bf;

    bf->bits = PBM_FREE(bf->bits);
}
/*@=mustmod@*/

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmbfPool = NULL;

static rpmbf rpmbfGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmbfPool, fileSystem @*/
	/*@modifies pool, _rpmbfPool, fileSystem @*/
{
    rpmbf bf;

    if (_rpmbfPool == NULL) {
	_rpmbfPool = rpmioNewPool("bf", sizeof(*bf), -1, _rpmbf_debug,
			NULL, NULL, rpmbfFini);
	pool = _rpmbfPool;
    }
    return (rpmbf) rpmioGetPool(pool, sizeof(*bf));
}

rpmbf rpmbfNew(size_t n, size_t m, size_t k, unsigned flags)
{
    rpmbf bf = rpmbfGetPool(_rpmbfPool);

    bf->n = (n > 0 ? n : 1024);
    bf->m = (m > 0 ? m : 8192);
    bf->k = (k > 0 ? k : 1);
    bf->bits = PBM_ALLOC(bf->m);

    return rpmbfLink(bf);
}

int rpmbfAdd(rpmbf bf, const char * s)
{
    return 0;
}

int rpmbfChk(rpmbf bf, const char * s)
{
    return 0;
}

int rpmbfClr(rpmbf bf)
{
    return 0;
}

int rpmbfDel(rpmbf bf, const char * s)
{
    return 0;
}

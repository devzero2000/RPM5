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

/* Any pair of 32 bit hashes can be used. lookup3.c generates pairs, will do. */
#define	_JLU3_jlu32lpair	1
#include "lookup3.c"

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

    if (n == 0)	n = 1024;
    if (k == 0) k = 16;
    if (m == 0) m = (3 * n * k) / 2;

    bf->n = n;
    bf->k = k;
    bf->m = m;
    bf->bits = PBM_ALLOC(bf->m-1);

    return rpmbfLink(bf);
}

int rpmbfAdd(rpmbf bf, const char * s)
{
    size_t ns = (s ? strlen(s) : 0);
    rpmuint32_t h0 = 0;
    rpmuint32_t h1 = 0;

assert(ns > 0);
    jlu32lpair(s, ns, &h0, &h1);

    for (ns = 0; ns < bf->k; ns++) {
	rpmuint32_t h = h0 + ns * h1;
	rpmuint32_t ix = (h % bf->m);
	PBM_SET(ix, bf);
    }
    return 0;
}

int rpmbfChk(rpmbf bf, const char * s)
{
    size_t ns = (s ? strlen(s) : 0);
    rpmuint32_t h0 = 0;
    rpmuint32_t h1 = 0;
    int rc = 1;

assert(ns > 0);
    jlu32lpair(s, ns, &h0, &h1);

    for (ns = 0; ns < bf->k; ns++) {
	rpmuint32_t h = h0 + ns * h1;
	rpmuint32_t ix = (h % bf->m);
	if (PBM_ISSET(ix, bf))
	    continue;
	rc = 0;
	break;
    }
    return rc;
}

int rpmbfClr(rpmbf bf)
{
    memset(__PBM_BITS(bf), 0, (__PBM_IX(bf->m-1) + 1) * (__PBM_NBITS/8));
    return 0;
}

int rpmbfDel(rpmbf bf, const char * s)
{
    size_t ns = (s ? strlen(s) : 0);
    rpmuint32_t h0 = 0;
    rpmuint32_t h1 = 0;

assert(ns > 0);
    jlu32lpair(s, ns, &h0, &h1);

    for (ns = 0; ns < bf->k; ns++) {
	rpmuint32_t h = h0 + ns * h1;
	rpmuint32_t ix = (h % bf->m);
	PBM_CLR(ix, bf);
    }
    return 0;
}

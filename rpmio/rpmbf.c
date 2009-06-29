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

rpmbf rpmbfNew(size_t m, size_t k, unsigned flags)
{
    static size_t nestimate = 1024;	/* XXX default estimated population. */
    rpmbf bf = rpmbfGetPool(_rpmbfPool);

    if (k == 0) k = 16;
    if (m == 0) m = (3 * nestimate * k) / 2;

    bf->k = k;
    bf->m = m;
    bf->n = 0;
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
    bf->n++;
if (_rpmbf_debug)
fprintf(stderr, "--> %s(%p,\"%s\") bf{%u,%u}[%u]\n", __FUNCTION__, bf, s, (unsigned)bf->m, (unsigned)bf->k, (unsigned)bf->n);
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
if (_rpmbf_debug)
fprintf(stderr, "--> %s(%p,\"%s\") bf{%u,%u}[%u]\n", __FUNCTION__, bf, s, (unsigned)bf->m, (unsigned)bf->k, (unsigned)bf->n);
    return rc;
}

int rpmbfClr(rpmbf bf)
{
    __pbm_bits * bits = __PBM_BITS(bf);
    static size_t nbw = (__PBM_NBITS/8);
    size_t nw = (__PBM_IX(bf->m-1) + 1);
    memset(bits, 0, nw * nbw);
    bf->n = 0;
if (_rpmbf_debug)
fprintf(stderr, "--> %s(%p) bf{%u,%u}[%u]\n", __FUNCTION__, bf, (unsigned)bf->m, (unsigned)bf->k, (unsigned)bf->n);
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
    if (bf->n != 0)
	bf->n--;
if (_rpmbf_debug)
fprintf(stderr, "--> %s(%p,\"%s\") bf{%u,%u}[%u]\n", __FUNCTION__, bf, s, (unsigned)bf->m, (unsigned)bf->k, (unsigned)bf->n);
    return 0;
}

int rpmbfIntersect(rpmbf a, const rpmbf b)
{
    __pbm_bits * abits = __PBM_BITS(a);
    __pbm_bits * bbits = __PBM_BITS(b);
    size_t nw = (__PBM_IX(a->m-1) + 1);
    size_t i;

    if (!(a->m == b->m && a->k == b->k))
	return -1;
    for (i = 0; i < nw; i++)
	abits[i] &= bbits[i];
    a->n = 1;		/* XXX what is population estimate? */
if (_rpmbf_debug)
fprintf(stderr, "--> %s(%p,%p) bf{%u,%u}[%u]\n", __FUNCTION__, a, b, (unsigned)a->m, (unsigned)a->k, (unsigned)a->n);
    return 0;
}

int rpmbfUnion(rpmbf a, const rpmbf b)
{
    __pbm_bits * abits = __PBM_BITS(a);
    __pbm_bits * bbits = __PBM_BITS(b);
    size_t nw = (__PBM_IX(a->m-1) + 1);
    size_t i;

    if (!(a->m == b->m && a->k == b->k))
	return -1;
    for (i = 0; i < nw; i++)
	abits[i] |= bbits[i];
    a->n += b->n;
if (_rpmbf_debug)
fprintf(stderr, "--> %s(%p,%p) bf{%u,%u}[%u]\n", __FUNCTION__, a, b, (unsigned)a->m, (unsigned)a->k, (unsigned)a->n);
    return 0;
}

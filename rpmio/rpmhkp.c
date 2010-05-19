#include "system.h"

#define _RPMHKP_INTERNAL
#include <rpmhkp.h>

#include "debug.h"

/*@unchecked@*/
int _rpmhkp_debug = 0;

/*@unchecked@*/ /*@relnull@*/
rpmhkp _rpmhkpI = NULL;

struct _filter_s {
    rpmbf bf;
    size_t n;
    double e;
    size_t m;
    size_t k;
};

static struct _filter_s awol	= { .n = 100000, .e = 1.0e-4 };
static struct _filter_s crl	= { .n = 100000, .e = 1.0e-4 };

static void rpmhkpFini(void * _hkp)
        /*@globals fileSystem @*/
        /*@modifies *_hkp, fileSystem @*/
{
    rpmhkp hkp = _hkp;

assert(hkp);
    hkp->pkt = _free(hkp->pkt);
    hkp->pktlen = 0;
    hkp->pkts = _free(hkp->pkts);
    hkp->npkts = 0;
    hkp->awol = rpmbfFree(hkp->awol);
    hkp->crl = rpmbfFree(hkp->crl);
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmhkpPool;

static rpmhkp rpmhkpGetPool(/*@null@*/ rpmioPool pool)
        /*@globals _rpmhkpPool, fileSystem @*/
        /*@modifies pool, _rpmhkpPool, fileSystem @*/
{
    rpmhkp hkp;

    if (_rpmhkpPool == NULL) {
        _rpmhkpPool = rpmioNewPool("hkp", sizeof(*hkp), -1, _rpmhkp_debug,
                        NULL, NULL, rpmhkpFini);
        pool = _rpmhkpPool;
    }
    hkp = (rpmhkp) rpmioGetPool(pool, sizeof(*hkp));
    memset(((char *)hkp)+sizeof(hkp->_item), 0, sizeof(*hkp)-sizeof(hkp->_item));
    return hkp;
}

#ifdef	NOTYET
static rpmhkp rpmhkpI(void)
	/*@globals _rpmhkpI @*/
	/*@modifies _rpmhkpI @*/
{
    if (_rpmhkpI == NULL)
	_rpmhkpI = rpmhkpNew(NULL, 0);
    return _rpmhkpI;
}
#endif

rpmhkp rpmhkpNew(const rpmuint8_t * keyid, uint32_t flags)
{
    rpmhkp hkp =
#ifdef	NOTYET
	(flags & 0x80000000) ? rpmhkpI() :
#endif
	rpmhkpGetPool(_rpmhkpPool);

    if (keyid)
	memcpy(hkp->keyid, keyid, sizeof(hkp->keyid));

    if (awol.bf)
	hkp->awol = rpmbfLink(awol.bf);
    if (crl.bf)
	hkp->crl = rpmbfLink(crl.bf);

hkp->pubx = -1;
hkp->uidx = -1;
hkp->subx = -1;
hkp->sigx = -1;

hkp->tvalid = 0;
hkp->uvalidx = -1;

    return rpmhkpLink(hkp);
}

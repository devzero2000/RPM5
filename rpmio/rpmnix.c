#include "system.h"

#define	_RPMNIX_INTERNAL
#include <rpmnix.h>
#include <poptIO.h>

#include "debug.h"

/*@unchecked@*/
int _rpmnix_debug = -1;

/*@unchecked@*/ /*@relnull@*/
rpmnix _rpmnixI = NULL;

/*@unchecked@*/
struct rpmnix_s _nix;

static void rpmnixFini(void * _nix)
	/*@globals fileSystem @*/
	/*@modifies *_nix, fileSystem @*/
{
    rpmnix nix = _nix;

DBG((stderr, "==> %s(%p) I %p\n", __FUNCTION__, nix, nix->I));

    /* nix-build */
    nix->outLink = _free(nix->outLink);
    nix->drvLink = _free(nix->drvLink);
    nix->instArgs = argvFree(nix->instArgs);
    nix->buildArgs = argvFree(nix->buildArgs);
    nix->exprs = argvFree(nix->exprs);

    /* nix-channel */
    nix->url = _free(nix->url);

    if (nix->I) {
	poptContext optCon = (poptContext) nix->I;
	optCon = rpmioFini(optCon);
    }
    nix->I = NULL;
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmnixPool;

static rpmnix rpmnixGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmnixPool, fileSystem @*/
	/*@modifies pool, _rpmnixPool, fileSystem @*/
{
    rpmnix nix;

    if (_rpmnixPool == NULL) {
	_rpmnixPool = rpmioNewPool("nix", sizeof(*nix), -1, _rpmnix_debug,
			NULL, NULL, rpmnixFini);
	pool = _rpmnixPool;
    }
    nix = (rpmnix) rpmioGetPool(pool, sizeof(*nix));
    memset(((char *)nix)+sizeof(nix->_item), 0, sizeof(*nix)-sizeof(nix->_item));
    return nix;
}

#ifdef	NOTYET
static rpmnix rpmnixI(void)
	/*@globals _rpmnixI @*/
	/*@modifies _rpmnixI @*/
{
    if (_rpmnixI == NULL) {
	_rpmnixI = rpmnixNew(NULL, 0, NULL);
    }
DBG((stderr, "<== %s() _rpmnixI %p\n", __FUNCTION__, _rpmnixI));
    return _rpmnixI;
}
#endif

static void rpmnixInitEnv(rpmnix nix)
	/*@modifies nix @*/
{
    const char * s;

    s = getenv("NIX_BIN_DIR");		nix->binDir = (s ? s : "/usr/bin");
    s = getenv("NIX_STATE_DIR");	nix->stateDir = (s ? s : "/nix/var/nix");
}

rpmnix rpmnixNew(char ** av, uint32_t flags, void * _tbl)
{
    rpmnix nix = rpmnixGetPool(_rpmnixPool);

    if (_tbl) {
	const poptOption tbl = (poptOption) _tbl;
	poptContext optCon;
        void *use =  nix->_item.use;
        void *pool = nix->_item.pool;

	memset(&_nix, 0, sizeof(_nix));
	_nix.flags = flags;
	optCon = rpmioInit(argvCount((ARGV_t)av), av, tbl);
	*nix = _nix;	/* structure assignment */
	memset(&_nix, 0, sizeof(_nix));

	nix->_item.use = use;
	nix->_item.pool = pool;
	nix->I = (void *) optCon;
	rpmnixInitEnv(nix);
    }

    return rpmnixLink(nix);
}

#ifdef	NOTYET
rpmRC rpmnixRun(rpmnix nix, const char * str, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

    if (nix == NULL) nix = rpmnixI();

    if (str != NULL) {
    }

DBG((stderr, "<== %s(%p,%p[%u]) rc %d\n", __FUNCTION__, nix, str, (unsigned)(str ? strlen(str) : 0), rc));

    return rc;
}
#endif

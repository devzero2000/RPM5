#ifndef RPMHKP_H
#define RPMHKP_H

/** \ingroup rpmio
 * \file rpmio/rpmhkp.h
 */

#include <rpmiotypes.h>
#include <rpmio.h>

typedef /*@abstract@*/ /*@refcounted@*/ struct rpmhkp_s * rpmhkp;

/*@unchecked@*/
extern int _rpmhkp_debug;

/*@unchecked@*/
extern rpmhkp _rpmhkpI;

#if defined(_RPMHKP_INTERNAL)
#include <rpmbf.h>
struct _filter_s {
    rpmbf bf;
    size_t n;
    double e;
    size_t m;
    size_t k;
};
extern struct _filter_s _rpmhkp_awol;
extern struct _filter_s _rpmhkp_crl;

extern int _rpmhkp_lvl;

struct rpmhkp_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
    rpmuint8_t * pkt;
    size_t pktlen;
    rpmuint8_t ** pkts;
    int npkts;

    int pubx;
    int uidx;
    int subx;
    int sigx;

    rpmuint8_t keyid[8];
    rpmuint8_t subid[8];
    rpmuint8_t signid[8];	/* XXX replaces ts->pksignid */
    rpmuint8_t goop[6];

    rpmuint32_t tvalid;
    int uvalidx;

    rpmbf awol;
    rpmbf crl;

#if defined(__LCLINT__)
/*@refs@*/
    int nrefs;			/*!< (unused) keep splint happy */
#endif
};
#endif /* _RPMHKP_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a hkp handle instance.
 * @param hkp		hkp handle
 * @return		NULL on last dereference
 */
/*@unused@*/ /*@null@*/
rpmhkp rpmhkpUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmhkp hkp)
	/*@modifies hkp @*/;
#define	rpmhkpUnlink(_hkp)	\
    ((rpmhkp)rpmioUnlinkPoolItem((rpmioItem)(_hkp), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a hkp handle instance.
 * @param hkp		hkp handle
 * @return		new hkp handle reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmhkp rpmhkpLink (/*@null@*/ rpmhkp hkp)
	/*@modifies hkp @*/;
#define	rpmhkpLink(_hkp)	\
    ((rpmhkp)rpmioLinkPoolItem((rpmioItem)(_hkp), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a hkp handle.
 * @param hkp		hkp handle
 * @return		NULL on last dereference
 */
/*@null@*/
rpmhkp rpmhkpFree(/*@killref@*/ /*@null@*/rpmhkp hkp)
	/*@globals fileSystem @*/
	/*@modifies hkp, fileSystem @*/;
#define	rpmhkpFree(_hkp)	\
    ((rpmhkp)rpmioFreePoolItem((rpmioItem)(_hkp), __FUNCTION__, __FILE__, __LINE__))

/**
 * Create and load a hkp handle.
 * @param keyid		pubkey fingerprint (or NULL)
 * @param flags		hkp handle flags ((1<<31): use global handle)
 * @return		new hkp handle
 */
/*@newref@*/ /*@null@*/
rpmhkp rpmhkpNew(/*@null@*/ const rpmuint8_t * keyid, uint32_t flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

rpmhkp rpmhkpLookup(const char * keyname)
	/*@*/;

rpmRC rpmhkpValidate(/*@null@*/ rpmhkp hkp, /*@null@*/ const char * keyname)
	/*@*/;

#if defined(_RPMHKP_INTERNAL)
int rpmhkpLoadKey(rpmhkp hkp, pgpDig dig,
                int keyx, rpmuint8_t pubkey_algo)
	/*@*/;
int rpmhkpLoadSignature(/*@null@*/ rpmhkp hkp, pgpDig dig, pgpPkt pp)
	/*@*/;
int rpmhkpFindKey(rpmhkp hkp, pgpDig dig,
                const rpmuint8_t * signid, rpmuint8_t pubkey_algo)
	/*@*/;
void _rpmhkpDumpDigParams(const char * msg, pgpDigParams sigp)
	/*@*/;
void _rpmhkpDumpDig(const char * msg, pgpDig dig)
	/*@*/;
int rpmhkpUpdate(/*@null@*/ DIGEST_CTX ctx, const void * data, size_t len)
	/*@*/;
#endif /* _RPMHKP_INTERNAL */

void _rpmhkpPrintStats(/*@null@*/ FILE * fp)
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif /* RPMHKP_H */

#ifndef H_RPMTXN
#define H_RPMTXN

/** \ingroup rpmdb
 * \file rpmdb/rpmtxn.h
 * Database transaction wrappers.
 */

/*@-exportlocal@*/
/*@unchecked@*/
extern int _rpmtxn_debug;
/*@=exportlocal@*/

typedef /*@abstract@*/ void * rpmtxn;

#ifdef __cplusplus
extern "C" {
#endif

uint32_t rpmtxnId(rpmtxn txn)
	/*@*/;

/*@null@*/
const char * rpmtxnName(rpmtxn txn)
	/*@*/;

int rpmtxnSetName(rpmtxn txn, const char * N)
	/*@*/;

int rpmtxnAbort(/*@only@*/ rpmtxn txn)
	/*@*/;

int rpmtxnBegin(rpmdb rpmdb, /*@null@*/ rpmtxn * txnp)
	/*@*/;

int rpmtxnCommit(/*@only@*/ rpmtxn txn)
	/*@*/;

#ifdef	NOTYET
int rpmtxnCheckpoint(rpmdb rpmdb)
	/*@*/;

int rpmtxnDiscard(/*@only@*/ rpmtxn txn)
	/*@*/;

int rpmtxnPrepare(rpmtxn txn)
	/*@*/;

int rpmtxnRecover(rpmdb rpmdb)
	/*@*/;
#endif	/* NOTYET */

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMTXN */

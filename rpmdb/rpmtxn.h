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

#ifdef __cplusplus
extern "C" {
#endif

uint32_t rpmtxnId(rpmdb rpmdb)
	/*@*/;

/*@null@*/
const char * rpmtxnName(rpmdb rpmdb)
	/*@*/;

int rpmtxnSetName(rpmdb rpmdb, const char * N)
	/*@*/;

int rpmtxnAbort(rpmdb rpmdb)
	/*@*/;

int rpmtxnBegin(rpmdb rpmdb)
	/*@*/;

int rpmtxnCommit(rpmdb rpmdb)
	/*@*/;

#ifdef	NOTYET
int rpmtxnCheckpoint(rpmdb rpmdb)
	/*@*/;

int rpmtxnDiscard(rpmdb rpmdb)
	/*@*/;

int rpmtxnPrepare(rpmdb rpmdb)
	/*@*/;

int rpmtxnRecover(rpmdb rpmdb)
	/*@*/;
#endif	/* NOTYET */

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMTXN */

/** \ingroup rpmdb
 * \file rpmdb/rpmtxn.c
 */

#include "system.h"

#include <rpmiotypes.h>
#include <rpmtypes.h>

#include <rpmtag.h>
#define	_RPMDB_INTERNAL
#include <rpmdb.h>
#include <rpmtxn.h>

#include "debug.h"

int _rpmtxn_debug = 0;

uint32_t rpmtxnId(rpmtxn txn)
{
    DB_TXN * _txn = txn;
    uint32_t rc = (_txn ? _txn->id(_txn) : 0);
    return rc;
}

const char * rpmtxnName(rpmtxn txn)
{
    DB_TXN * _txn = txn;
    const char * N = NULL;
    int rc = (_txn ? _txn->get_name(_txn, &N) : ENOTSUP);
    rc = rc;
    return N;
}

int rpmtxnSetName(rpmtxn txn, const char * N)
{
    DB_TXN * _txn = txn;
    int rc = (_txn ? _txn->set_name(_txn, N) : ENOTSUP);
if (_rpmtxn_debug)
fprintf(stderr, "<-- %s(%p,%s) rc %d\n", "txn->set_name", _txn, N, rc);
    return rc;
}

int rpmtxnAbort(rpmtxn txn)
{
    DB_TXN * _txn = txn;
    int rc = (_txn ? _txn->abort(_txn) : ENOTSUP);
if (_rpmtxn_debug)
fprintf(stderr, "<-- %s(%p) rc %d\n", "txn->abort", _txn, rc);
    return rc;
}

int rpmtxnBegin(rpmdb rpmdb, rpmtxn * txnp)
{
    DB_ENV * dbenv = (rpmdb ? rpmdb->db_dbenv : NULL);
    DB_TXN * _parent = NULL;
    DB_TXN * _txn = NULL;
    uint32_t _flags = 0;
    int rc = (dbenv && rpmdb->_dbi[0]->dbi_eflags & 0x800)
	? dbenv->txn_begin(dbenv, _parent, &_txn, _flags) : ENOTSUP;
    if (!rc) {
	if (txnp != NULL)
	    *txnp = _txn;
	else
	    rpmdb->db_txn = _txn;
    }
if (_rpmtxn_debug)
fprintf(stderr, "<-- %s(%p,%p,%p,0x%x) txn %p rc %d\n", "dbenv->txn_begin", dbenv, _parent, &_txn, _flags, _txn, rc);
    return rc;
}

int rpmtxnCommit(rpmtxn txn)
{
    DB_TXN * _txn = txn;
    uint32_t _flags = 0;
    int rc = (_txn ? _txn->commit(_txn, _flags) : ENOTSUP);
if (_rpmtxn_debug)
fprintf(stderr, "<-- %s(%p,0x%x) rc %d\n", "txn->commit", _txn, _flags, rc);
    return rc;
}

#ifdef	NOTYET
int rpmtxnCheckpoint(rpmdb rpmdb)
{
    DB_ENV * dbenv = rpmdb->db_dbenv;
    uint32_t _kbytes = 0;
    uint32_t _minutes = 0;
    uint32_t _flags = 0;
    int rc = dbenv->txn_checkpoint(dbenv, _kbytes, _minutes, _flags);
if (_rpmtxn_debug)
fprintf(stderr, "<-- %s(%p,%u,%u,0x%x) rc %d\n", "dbenv->txn_checkpoint", dbenv, _kbytes, _minutes, _flags, rc);
    return rc;
}

int rpmtxnDiscard(rpmtxn txn)
{
    DB_TXN * _txn = txn;
    uint32_t _flags = 0;
    int rc = (_txn ? _txn->discard(_txn, _flags) : ENOTSUP);
    return rc;
}

int rpmtxnPrepare(rpmtxn txn)
{
    DB_TXN * _txn = txn;
    uint8_t _gid[DB_GID_SIZE] = {0};
    int rc = (_txn ? _txn->prepare(_txn, _gid) : ENOTSUP);
    return rc;
}

int rpmtxnRecover(rpmdb rpmdb)
{
    DB_ENV * dbenv = rpmdb->db_dbenv;
    DB_PREPLIST _preplist[32];
    long _count = (sizeof(_preplist) / sizeof(_preplist[0]));
    long _got = 0;
    uint32_t _flags = DB_FIRST;
    int rc = 0;
    int i;

    while (1) {
	rc = dbenv->txn_recover(dbenv, _preplist, _count, &_got, _flags);
	_flags = DB_NEXT;
	if (rc || _got == 0)
	    break;
	for (i = 0; i < _got; i++) {
	    DB_TXN * _txn = _preplist[i].txn;
	    uint32_t _tflags = 0;
	    (void) _txn->discard(_txn, _tflags);
	}
    }
    return rc;
}

#endif	/* NOTYET */

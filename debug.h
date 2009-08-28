/**
 * To be included after all other includes.
 */
#ifndef	H_DEBUG
#define	H_DEBUG

#include <assert.h>

#ifdef  __LCLINT__
#define	ntohl(_x)	(_x)
#define	ntohs(_x)	(_x)
#define	htonl(_x)	(_x)
#define	htons(_x)	(_x)
#endif

#ifdef	WITH_DMALLOC
#include <dmalloc.h>
#endif

#if defined(WITH_VALGRIND) && defined(HAVE_VALGRIND_VALGRIND_H)

#include <valgrind/valgrind.h>
#include <valgrind/memcheck.h>
#include <valgrind/callgrind.h>
#include <valgrind/helgrind.h>
#include <valgrind/drd.h>

#else

#define	VALGRIND_CREATE_MEMPOOL(pool, rzB, is_zeroed)
#define	VALGRIND_DESTROY_MEMPOOL(pool)
#define	VALGRIND_MEMPOOL_ALLOC(pool, addr, size)
#define	VALGRIND_MEMPOOL_FREE(pool, addr)
#define	VALGRIND_MEMPOOL_TRIM(pool, addr, size)
#define	VALGRIND_MOVE_MEMPOOL(poolA, poolB)
#define	VALGRIND_MEMPOOL_CHANGE(pool, addrA, addrB, size)
#define	VALGRIND_MEMPOOL_EXISTS(pool)	(0)

#define	VALGRIND_HG_CLEAN_MEMORY(_qzz_start, _qzz_len)

#define	CALLGRIND_DUMP_STATS
#define	CALLGRIND_DUMP_STATS_AT(pos_str)
#define	CALLGRIND_ZERO_STATS
#define	CALLGRIND_TOGGLE_COLLECT
#define	CALLGRIND_START_INSTRUMENTATION
#define	CALLGRIND_STOP_INSTRUMENTATION

#endif

#endif	/* H_DEBUG */

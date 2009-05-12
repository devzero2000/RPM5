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
#else
#define CALLGRIND_DUMP_STATS
#define CALLGRIND_DUMP_STATS_AT(pos_str)
#define CALLGRIND_ZERO_STATS
#define CALLGRIND_TOGGLE_COLLECT
#define CALLGRIND_START_INSTRUMENTATION
#define CALLGRIND_STOP_INSTRUMENTATION
#endif

#endif	/* H_DEBUG */

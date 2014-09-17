#ifndef	H_RPMDATE
#define	H_RPMDATE

/** \ingroup rpmio
 * \file rpmio/rpmdate.h
 */

#include <rpmiotypes.h>
#include <argv.h>

extern int _rpmdate_debug;

typedef struct rpmdate_s * rpmdate;

#if defined(_RPMDATE_INTERNAL)
struct rpmdate_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
    unsigned flags;
    ARGV_t results;
#if defined(__LCLINT__)
/*@refs@*/
    int nrefs;			/*!< (unused) keep splint happy */
#endif
};
#endif	/* _RPMDATE_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a date wrapper instance.
 * @param date		date wrapper
 * @return		NULL on last dereference
 */
rpmdate rpmdateUnlink (rpmdate date);
#define	rpmdateUnlink(_date)	\
    ((rpmdate)rpmioUnlinkPoolItem((rpmioItem)(_date), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a date wrapper instance.
 * @param date		date wrapper
 * @return		new date wrapper reference
 */
rpmdate rpmdateLink (rpmdate date);
#define	rpmdateLink(_date)	\
    ((rpmdate)rpmioLinkPoolItem((rpmioItem)(_date), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a date wrapper.
 * @param date		date wrapper
 * @return		NULL on last dereference
 */
rpmdate rpmdateFree(rpmdate date);
#define	rpmdateFree(_date)	\
    ((rpmdate)rpmioFreePoolItem((rpmioItem)(_date), __FUNCTION__, __FILE__, __LINE__))

/**
 * Create and load a date wrapper.
 * @param argv		date arguments
 * @param flags		date flags
 * @return		new date wrapper
 */
rpmdate rpmdateNew(char ** argv, unsigned flags);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMDATE */

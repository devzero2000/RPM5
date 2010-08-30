#ifndef	H_RPMREPO
#define	H_RPMREPO

/** \ingroup rpmio
 * \file rpmio/rpmrepo.h
 */

/** \ingroup rpmio
 */
/*@unchecked@*/
extern int _rpmrepo_debug;

/** \ingroup rpmio
 */
typedef /*@refcounted@*/ struct rpmrepo_s * rpmrepo;

#if defined(_RPMREPO_INTERNAL)

/** \ingroup rpmio
 */
struct rpmrepo_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
    const char * fn;
#if defined(__LCLINT__)
/*@refs@*/
    int nrefs;			/*!< (unused) keep splint happy */
#endif
};
#endif	/* _RPMREPO_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a repo wrapper instance.
 * @param repo		repo wrapper
 * @return		NULL on last dereference
 */
/*@unused@*/ /*@null@*/
rpmrepo rpmrepoUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmrepo repo)
	/*@modifies repo @*/;
#define	rpmrepoUnlink(_repo)	\
    ((rpmrepo)rpmioUnlinkPoolItem((rpmioItem)(_repo), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a repo wrapper instance.
 * @param repo		repo wrapper
 * @return		new repo wrapper reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmrepo rpmrepoLink (/*@null@*/ rpmrepo repo)
	/*@modifies repo @*/;
#define	rpmrepoLink(_repo)	\
    ((rpmrepo)rpmioLinkPoolItem((rpmioItem)(_repo), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a repo wrapper.
 * @param repo		repo wrapper
 * @return		NULL on last dereference
 */
/*@null@*/
rpmrepo rpmrepoFree(/*@killref@*/ /*@null@*/rpmrepo repo)
	/*@globals fileSystem @*/
	/*@modifies repo, fileSystem @*/;
#define	rpmrepoFree(_repo)	\
    ((rpmrepo)rpmioFreePoolItem((rpmioItem)(_repo), __FUNCTION__, __FILE__, __LINE__))

/**
 * Create and load a repo wrapper.
 * @param fn		repo file
 * @param flags		repo flags
 * @return		new repo wrapper
 */
/*@newref@*/ /*@null@*/
rpmrepo rpmrepoNew(const char * fn, int flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMREPO */

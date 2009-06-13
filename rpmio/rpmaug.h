#ifndef	H_RPMAUG
#define	H_RPMAUG

/** \ingroup rpmio
 * \file rpmio/rpmaug.h
 */

/** \ingroup rpmio
 */
/*@unchecked@*/
extern int _rpmaug_debug;

/** \ingroup rpmio
 */
typedef /*@refcounted@*/ struct rpmaug_s * rpmaug;

#if defined(_RPMAUG_INTERNAL)
/** \ingroup rpmio
 */
struct rpmaug_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
    const char * root;
    const char * loadpath;
    unsigned int flags;
/*@relnull@*/
    void * I;
#if defined(__LCLINT__)
/*@refs@*/
    int nrefs;			/*!< (unused) keep splint happy */
#endif
};
#endif	/* _RPMAUG_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a augeas wrapper instance.
 * @param aug		augeas wrapper
 * @return		NULL on last dereference
 */
/*@unused@*/ /*@null@*/
rpmaug rpmaugUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmaug aug)
	/*@modifies aug @*/;
#define	rpmaugUnlink(_ds)	\
    ((rpmaug)rpmioUnlinkPoolItem((rpmioItem)(_aug), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a augeas wrapper instance.
 * @param aug		augeas wrapper
 * @return		new augeas wrapper reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmaug rpmaugLink (/*@null@*/ rpmaug aug)
	/*@modifies aug @*/;
#define	rpmaugLink(_aug)	\
    ((rpmaug)rpmioLinkPoolItem((rpmioItem)(_aug), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a augeas wrapper.
 * @param aug		augeas wrapper
 * @return		NULL on last dereference
 */
/*@null@*/
rpmaug rpmaugFree(/*@killref@*/ /*@null@*/rpmaug aug)
	/*@globals fileSystem @*/
	/*@modifies aug, fileSystem @*/;
#define	rpmaugFree(_aug)	\
    ((rpmaug)rpmioFreePoolItem((rpmioItem)(_aug), __FUNCTION__, __FILE__, __LINE__))

/**
 * Create and load a augeas wrapper.
 * @param root		augeas filesystem root
 * @param loadpath	augeas load path (colon separated)
 * @param flags		augeas flags
 * @return		new augeas wrapper
 */
/*@newref@*/ /*@null@*/
rpmaug rpmaugNew(/*@null@*/ const char * root, /*@null@*/ const char * loadpath,
		unsigned int flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMAUG */

#ifndef	H_RPMSM
#define	H_RPMSM

/** \ingroup rpmio
 * \file rpmio/rpmsm.h
 */

#include <rpmiotypes.h>
#include <rpmio.h>

typedef /*@refcounted@*/ struct rpmsm_s * rpmsm;

/*@unchecked@*/
extern int _rpmsm_debug;

#if defined(_RPMSM_INTERNAL)
/** \ingroup rpmio
 */
struct rpmsm_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
    unsigned int flags;
#if defined(__LCLINT__)
/*@refs@*/
    int nrefs;			/*!< (unused) keep splint happy */
#endif
};
#endif	/* _RPMSM_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a semanage wrapper instance.
 * @param sm		semanage wrapper
 * @return		NULL on last dereference
 */
/*@unused@*/ /*@null@*/
rpmsm rpmsmUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmsm sm)
	/*@modifies sm @*/;
#define	rpmsmUnlink(_sm)	\
    ((rpmsm)rpmioUnlinkPoolItem((rpmioItem)(_sm), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a semanage wrapper instance.
 * @param sm		semanage wrapper
 * @return		new semanage wrapper reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmsm rpmsmLink (/*@null@*/ rpmsm sm)
	/*@modifies sm @*/;
#define	rpmsmLink(_sm)	\
    ((rpmsm)rpmioLinkPoolItem((rpmioItem)(_sm), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a semanage wrapper.
 * @param sm		semanage wrapper
 * @return		NULL on last dereference
 */
/*@null@*/
rpmsm rpmsmFree(/*@killref@*/ /*@null@*/rpmsm sm)
	/*@globals fileSystem @*/
	/*@modifies sm, fileSystem @*/;
#define	rpmsmFree(_sm)	\
    ((rpmsm)rpmioFreePoolItem((rpmioItem)(_sm), __FUNCTION__, __FILE__, __LINE__))

/**
 * Create and load a semanage wrapper.
 * @param fn		semanage file (unused).
 * @param flags		semanage flags
 * @return		new semanage wrapper
 */
/*@newref@*/ /*@null@*/
rpmsm rpmsmNew(/*@null@*/ const char * fn, unsigned int flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMSM */

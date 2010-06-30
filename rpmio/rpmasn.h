#ifndef	H_RPMASN
#define	H_RPMASN

/** \ingroup rpmio
 * \file rpmio/rpmasn.h
 */

/** \ingroup rpmio
 */
/*@unchecked@*/
extern int _rpmasn_debug;

/** \ingroup rpmio
 */
typedef /*@refcounted@*/ struct rpmasn_s * rpmasn;

#if defined(_RPMASN_INTERNAL)
/** \ingroup rpmio
 */
struct rpmasn_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
    const char * fn;
    int flags;
/*@relnull@*/
    void * ms;
#if defined(__LCLINT__)
/*@refs@*/
    int nrefs;			/*!< (unused) keep splint happy */
#endif
};
#endif	/* _RPMASN_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a ASN.1 wrapper instance.
 * @param asn		ASN.1 wrapper
 * @return		NULL on last dereference
 */
/*@unused@*/ /*@null@*/
rpmasn rpmasnUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmasn asn)
	/*@modifies asn @*/;
#define	rpmasnUnlink(_asn)	\
    ((rpmasn)rpmioUnlinkPoolItem((rpmioItem)(_asn), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a ASN.1 wrapper instance.
 * @param asn		ASN.1 wrapper
 * @return		new ASN.1 wrapper reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmasn rpmasnLink (/*@null@*/ rpmasn asn)
	/*@modifies asn @*/;
#define	rpmasnLink(_asn)	\
    ((rpmasn)rpmioLinkPoolItem((rpmioItem)(_asn), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a ASN.1 wrapper.
 * @param asn		ASN.1 wrapper
 * @return		NULL on last dereference
 */
/*@null@*/
rpmasn rpmasnFree(/*@killref@*/ /*@null@*/rpmasn asn)
	/*@globals fileSystem @*/
	/*@modifies asn, fileSystem @*/;
#define	rpmasnFree(_asn)	\
    ((rpmasn)rpmioFreePoolItem((rpmioItem)(_asn), __FUNCTION__, __FILE__, __LINE__))

/**
 * Create and load a ASN.1 wrapper.
 * @param fn		ASN.1 file
 * @param flags		ASN.1 flags
 * @return		new ASN.1 wrapper
 */
/*@newref@*/ /*@null@*/
rpmasn rpmasnNew(const char * fn, int flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMASN */

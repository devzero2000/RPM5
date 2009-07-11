#ifndef	H_RPMSEX
#define	H_RPMSEX

/** \ingroup rpmio
 * \file rpmio/rpmsex.h
 */

/** \ingroup rpmio
 */
/*@unchecked@*/
extern int _rpmsex_debug;

/** \ingroup rpmio
 */
typedef /*@refcounted@*/ struct rpmsex_s * rpmsex;

#if defined(_RPMSEX_INTERNAL)
/** \ingroup rpmio
 */
struct rpmsex_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
    const char * fn;
    int flags;
#if defined(__LCLINT__)
/*@refs@*/
    int nrefs;			/*!< (unused) keep splint happy */
#endif
};
#endif	/* _RPMSEX_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a SELinux wrapper instance.
 * @param mg		SELinux wrapper
 * @return		NULL on last dereference
 */
/*@unused@*/ /*@null@*/
rpmsex rpmsexUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmsex mg)
	/*@modifies mg @*/;
#define	rpmsexUnlink(_mg)	\
    ((rpmsex)rpmioUnlinkPoolItem((rpmioItem)(_mg), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a SELinux wrapper instance.
 * @param mg		SELinux wrapper
 * @return		new SELinux wrapper reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmsex rpmsexLink (/*@null@*/ rpmsex mg)
	/*@modifies mg @*/;
#define	rpmsexLink(_mg)	\
    ((rpmsex)rpmioLinkPoolItem((rpmioItem)(_mg), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a SELinux wrapper.
 * @param mg		SELinux wrapper
 * @return		NULL on last dereference
 */
/*@null@*/
rpmsex rpmsexFree(/*@killref@*/ /*@null@*/rpmsex mg)
	/*@globals fileSystem @*/
	/*@modifies mg, fileSystem @*/;
#define	rpmsexFree(_mg)	\
    ((rpmsex)rpmioFreePoolItem((rpmioItem)(_mg), __FUNCTION__, __FILE__, __LINE__))

/**
 * Create and load a SELinux wrapper.
 * @param fn		SELinux file
 * @param flags		SELinux flags
 * @return		new SELinux wrapper
 */
/*@newref@*/ /*@null@*/
rpmsex rpmsexNew(const char * fn, int flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

/**
 * Return security context for a file.
 * @param sex		SELinux wrapper
 * @param fn		file path
 * @param mode		file mode
 * @return		file security context
 */
/*@only@*/
const char * rpmsexMatch(rpmsex sex, const char * fn, mode_t mode)
	/*@globals fileSystem, internalState @*/
	/*@modifies sex, fileSystem, internalState @*/;

/**
 * Execute a package scriptlet within SELinux context.
 * @param sex		SELinux wrapper
 * @param verified	Scriptlet came from signature verified header? (unused)
 * @param argv		scriptlet helper
 * @return		0 on success
 */
/*@only@*/
int rpmsexExec(rpmsex sex, int verified, const char ** argv)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMSEX */

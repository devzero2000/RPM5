#ifndef	H_RPMMDB
#define	H_RPMMDB

/** \ingroup rpmio
 * \file rpmdb/rpmmdb.h
 */

/** \ingroup rpmio
 */
typedef /*@refcounted@*/ struct rpmmdb_s * rpmmdb;

/** \ingroup rpmdb
 */
/*@unchecked@*/
extern int _rpmmdb_debug;

/*@unchecked@*/ /*@relnull@*/
extern rpmmdb _rpmmdbI;

#if defined(_RPMMDB_INTERNAL)

/** \ingroup rpmdb
 */
struct rpmmdb_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
    const char * fn;
#if defined(__LCLINT__)
/*@refs@*/
    int nrefs;			/*!< (unused) keep splint happy */
#endif
};
#endif	/* _RPMMDB_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a mdb wrapper instance.
 * @param mdb		mdb wrapper
 * @return		NULL on last dereference
 */
/*@unused@*/ /*@null@*/
rpmmdb rpmmdbUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmmdb mdb)
	/*@modifies mdb @*/;
#define	rpmmdbUnlink(_mdb)	\
    ((rpmmdb)rpmioUnlinkPoolItem((rpmioItem)(_mdb), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a mdb wrapper instance.
 * @param mdb		mdb wrapper
 * @return		new mdb wrapper reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmmdb rpmmdbLink (/*@null@*/ rpmmdb mdb)
	/*@modifies mdb @*/;
#define	rpmmdbLink(_mdb)	\
    ((rpmmdb)rpmioLinkPoolItem((rpmioItem)(_mdb), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a mdb wrapper.
 * @param mdb		mdb wrapper
 * @return		NULL on last dereference
 */
/*@null@*/
rpmmdb rpmmdbFree(/*@killref@*/ /*@null@*/rpmmdb mdb)
	/*@globals fileSystem @*/
	/*@modifies mdb, fileSystem @*/;
#define	rpmmdbFree(_mdb)	\
    ((rpmmdb)rpmioFreePoolItem((rpmioItem)(_mdb), __FUNCTION__, __FILE__, __LINE__))

/**
 * Create and load a mdb wrapper.
 * @param fn		mdb file
 * @param flags		mdb flags
 * @return		new mdb wrapper
 */
/*@newref@*/ /*@null@*/
rpmmdb rpmmdbNew(const char * fn, int flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMMDB */

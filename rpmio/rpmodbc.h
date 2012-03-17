#ifndef	H_RPMODBC
#define	H_RPMODBC

/** \ingroup rpmio
 * \file rpmio/rpmodbc.h
 */

/** \ingroup rpmio
 */
/*@unchecked@*/
extern int _rpmodbc_debug;

/** \ingroup rpmio
 */
typedef /*@refcounted@*/ struct rpmodbc_s * rpmodbc;

#if defined(_RPMODBC_INTERNAL)
/** \ingroup rpmio
 */
struct rpmodbc_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
    const char * fn;
    int flags;

    const char * db;
    const char * u;
    const char * pw;

    void * env;
    void * dbc;
    void * stmt;
    void * desc;

    int ncols;
    int nrows;
    int cx;
    int rx;

#if defined(__LCLINT__)
/*@refs@*/
    int nrefs;			/*!< (unused) keep splint happy */
#endif
};
#endif	/* _RPMODBC_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a odbc wrapper instance.
 * @param odbc		odbc wrapper
 * @return		NULL on last dereference
 */
/*@unused@*/ /*@null@*/
rpmodbc rpmodbcUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmodbc odbc)
	/*@modifies odbc @*/;
#define	rpmodbcUnlink(_odbc)	\
    ((rpmodbc)rpmioUnlinkPoolItem((rpmioItem)(_odbc), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a odbc wrapper instance.
 * @param odbc		odbc wrapper
 * @return		new odbc wrapper reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmodbc rpmodbcLink (/*@null@*/ rpmodbc odbc)
	/*@modifies odbc @*/;
#define	rpmodbcLink(_odbc)	\
    ((rpmodbc)rpmioLinkPoolItem((rpmioItem)(_odbc), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a odbc wrapper.
 * @param odbc		odbc wrapper
 * @return		NULL on last dereference
 */
/*@null@*/
rpmodbc rpmodbcFree(/*@killref@*/ /*@null@*/rpmodbc odbc)
	/*@globals fileSystem @*/
	/*@modifies odbc, fileSystem @*/;
#define	rpmodbcFree(_odbc)	\
    ((rpmodbc)rpmioFreePoolItem((rpmioItem)(_odbc), __FUNCTION__, __FILE__, __LINE__))

/**
 * Create and load a odbc wrapper.
 * @param fn		odbc file
 * @param flags		odbc flags
 * @return		new odbc wrapper
 */
/*@newref@*/ /*@null@*/
rpmodbc rpmodbcNew(const char * fn, int flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

int rpmodbcConnect(rpmodbc odbc,
		const char * db, const char * u, const char * pw)
	/*@*/;

int rpmodbcDisconnect(rpmodbc odbc)
	/*@*/;

int rpmodbcListDataSources(rpmodbc odbc, void *_fp)
	/*@*/;

int rpmodbcListDrivers(rpmodbc odbc, void *_fp)
	/*@*/;

int rpmodbcTables(rpmodbc odbc)
	/*@*/;

int rpmodbcColumns(rpmodbc odbc)
	/*@*/;

int rpmodbcNCols(rpmodbc odbc)
	/*@*/;

int rpmodbcExecDirect(rpmodbc odbc, const char * s, size_t ns)
	/*@*/;

int rpmodbcPrepare(rpmodbc odbc, const char * s, size_t ns)
	/*@*/;

int rpmodbcExecute(rpmodbc odbc)
	/*@*/;

int rpmodbcFetch(rpmodbc odbc)
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMODBC */

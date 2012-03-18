#ifndef	H_RPMODBC
#define	H_RPMODBC

/** \ingroup rpmio
 * \file rpmio/rpmodbc.h
 */

/** \ingroup rpmio
 */
/*@unchecked@*/
extern int _odbc_debug;

/** \ingroup rpmio
 */
typedef /*@refcounted@*/ struct ODBC_s * ODBC_t;

#if defined(_RPMODBC_INTERNAL)
/** \ingroup rpmio
 */
struct ODBC_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
    const char * fn;
    int flags;

    void * u;
    const char * db;

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
ODBC_t odbcUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ ODBC_t odbc)
	/*@modifies odbc @*/;
#define	odbcUnlink(_odbc)	\
    ((ODBC_t)rpmioUnlinkPoolItem((rpmioItem)(_odbc), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a odbc wrapper instance.
 * @param odbc		odbc wrapper
 * @return		new odbc wrapper reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
ODBC_t odbcLink (/*@null@*/ ODBC_t odbc)
	/*@modifies odbc @*/;
#define	odbcLink(_odbc)	\
    ((ODBC_t)rpmioLinkPoolItem((rpmioItem)(_odbc), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a odbc wrapper.
 * @param odbc		odbc wrapper
 * @return		NULL on last dereference
 */
/*@null@*/
ODBC_t odbcFree(/*@killref@*/ /*@null@*/ODBC_t odbc)
	/*@globals fileSystem @*/
	/*@modifies odbc, fileSystem @*/;
#define	odbcFree(_odbc)	\
    ((ODBC_t)rpmioFreePoolItem((rpmioItem)(_odbc), __FUNCTION__, __FILE__, __LINE__))

/**
 * Create and load a odbc wrapper.
 * @param fn		odbc file
 * @param flags		odbc flags
 * @return		new odbc wrapper
 */
/*@newref@*/ /*@null@*/
ODBC_t odbcNew(const char * fn, int flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

int odbcConnect(ODBC_t odbc, /*@null@*/ const char * uri)
	/*@*/;

int odbcDisconnect(ODBC_t odbc)
	/*@*/;

int odbcListDataSources(ODBC_t odbc, void *_fp)
	/*@*/;

int odbcListDrivers(ODBC_t odbc, void *_fp)
	/*@*/;

int odbcTables(ODBC_t odbc)
	/*@*/;

int odbcColumns(ODBC_t odbc)
	/*@*/;

int odbcNCols(ODBC_t odbc)
	/*@*/;

int odbcExecDirect(ODBC_t odbc, const char * s, size_t ns)
	/*@*/;

int odbcPrepare(ODBC_t odbc, const char * s, size_t ns)
	/*@*/;

int odbcExecute(ODBC_t odbc)
	/*@*/;

int odbcFetch(ODBC_t odbc)
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMODBC */

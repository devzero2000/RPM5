#ifndef RPMSQL_H
#define RPMSQL_H

/** \ingroup rpmio
 * \file rpmio/rpmsql.h
 */

#include <rpmiotypes.h>
#include <rpmio.h>
#include <argv.h>

typedef /*@abstract@*/ /*@refcounted@*/ struct rpmsql_s * rpmsql;

/*@unchecked@*/
extern int _rpmsql_debug;

/*@unchecked@*/
extern rpmsql _rpmsqlI;

#if defined(_RPMSQL_INTERNAL)

#define F_ISSET(_sql, _FLAG) ((_sql)->flags & (RPMSQL_FLAGS_##_FLAG))
#define SQLDBG(_l) if (_rpmsql_debug) fprintf _l

/**
 * Interpreter flags.
 */
enum rpmsqlFlags_e {
    RPMSQL_FLAGS_NONE		= 0,
};

struct rpmsql_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
    uint32_t flags;		/*!< control bits */
    const char ** av;		/*!< arguments */
    void * I;

    int stdin_is_interactive;
    int bail_on_error;

    const char * zInitFile;

#if defined(__LCLINT__)
/*@refs@*/
    int nrefs;			/*!< (unused) keep splint happy */
#endif
};

/*@unchecked@*/
extern struct rpmsql_s _sql;

#endif /* _RPMSQL_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a sql interpreter instance.
 * @param sql		sql interpreter
 * @return		NULL on last dereference
 */
/*@unused@*/ /*@null@*/
rpmsql rpmsqlUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmsql sql)
	/*@modifies sql @*/;
#define	rpmsqlUnlink(_ds)	\
    ((rpmsql)rpmioUnlinkPoolItem((rpmioItem)(_sql), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a sql interpreter instance.
 * @param sql		sql interpreter
 * @return		new sql interpreter reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmsql rpmsqlLink (/*@null@*/ rpmsql sql)
	/*@modifies sql @*/;
#define	rpmsqlLink(_sql)	\
    ((rpmsql)rpmioLinkPoolItem((rpmioItem)(_sql), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a sql interpreter.
 * @param sql		sql interpreter
 * @return		NULL on last dereference
 */
/*@null@*/
rpmsql rpmsqlFree(/*@killref@*/ /*@null@*/rpmsql sql)
	/*@globals fileSystem @*/
	/*@modifies sql, fileSystem @*/;
#define	rpmsqlFree(_sql)	\
    ((rpmsql)rpmioFreePoolItem((rpmioItem)(_sql), __FUNCTION__, __FILE__, __LINE__))

/**
 * Create and load a sql interpreter.
 * @param av		sql interpreter args (or NULL)
 * @param flags		sql interpreter flags
 * @return		new sql interpreter
 */
/*@newref@*/ /*@null@*/
rpmsql rpmsqlNew(/*@null@*/ char ** av, uint32_t flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

#ifdef	NOTYET
/**
 * Execute sql from a file.
 * @param sql		sql interpreter (NULL uses global interpreter)
 * @param fn		sql file to run (NULL returns RPMRC_FAIL)
 * @param *resultp	sql exec result
 * @return		RPMRC_OK on success
 */
rpmRC rpmsqlRunFile(rpmsql sql, /*@null@*/ const char * fn,
		/*@null@*/ const char ** resultp)
	/*@globals fileSystem, internalState @*/
	/*@modifies sql, fileSystem, internalState @*/;
#endif

/**
 * Execute sql string.
 * @param sql		sql interpreter (NULL uses global interpreter)
 * @param str		sql string to execute (NULL returns RPMRC_FAIL)
 * @param *resultp	sql interpreter result
 * @return		RPMRC_OK on success
 */
rpmRC rpmsqlRun(rpmsql sql, /*@null@*/ const char * str,
		/*@null@*/ const char ** resultp)
	/*@globals fileSystem, internalState @*/
	/*@modifies sql, *resultp, fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif /* RPMSQL_H */

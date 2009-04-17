#ifndef RPMJS_H
#define RPMJS_H

/** \ingroup rpmio
 * \file rpmio/rpmjs.h
 */

#include <rpmiotypes.h>
#include <rpmio.h>

typedef /*@abstract@*/ struct rpmjs_s * rpmjs;

/*@unchecked@*/
extern int _rpmjs_debug;

/*@unchecked@*/ /*@relnull@*/
extern rpmjs _rpmjsI;

#if defined(_RPMJS_INTERNAL)
struct rpmjs_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
    void * rt;
    void * cx;
    void * glob;
};
#endif /* _RPMJS_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a js interpreter instance.
 * @param js		js interpreter
 * @return		NULL on last dereference
 */
/*@unused@*/ /*@null@*/
rpmjs rpmjsUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmjs js)
	/*@modifies js @*/;
#define	rpmjsUnlink(_ds)	\
    ((rpmjs)rpmioUnlinkPoolItem((rpmioItem)(_js), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a js interpreter instance.
 * @param js		js interpreter
 * @return		new js interpreter reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmjs rpmjsLink (/*@null@*/ rpmjs js)
	/*@modifies js @*/;
#define	rpmjsLink(_js)	\
    ((rpmjs)rpmioLinkPoolItem((rpmioItem)(_js), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a js interpreter.
 * @param js		js interpreter
 * @return		NULL on last dereference
 */
/*@null@*/
rpmjs rpmjsFree(/*@killref@*/ /*@null@*/rpmjs js)
	/*@globals fileSystem @*/
	/*@modifies js, fileSystem @*/;
#define	rpmjsFree(_js)	\
    ((rpmjs)rpmioFreePoolItem((rpmioItem)(_js), __FUNCTION__, __FILE__, __LINE__))

/**
 * Create and load a js interpreter.
 * @param av		js interpreter args (or NULL)
 * @param flags		js interpreter flags (1 == use global interpreter)
 * @return		new js interpreter
 */
/*@newref@*/ /*@null@*/
rpmjs rpmjsNew(/*@null@*/ const char ** av, int flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

/**
 * Execute js from a file.
 * @param js		js interpreter (NULL uses global interpreter)
 * @param fn		js file to run (NULL returns RPMRC_FAIL)
 * @param *resultp	js exec result
 * @return		RPMRC_OK on success
 */
rpmRC rpmjsRunFile(rpmjs js, /*@null@*/ const char * fn,
		/*@null@*/ const char ** resultp)
	/*@globals fileSystem, internalState @*/
	/*@modifies js, fileSystem, internalState @*/;

/**
 * Execute js string.
 * @param js		js interpreter (NULL uses global interpreter)
 * @param str		js string to execute (NULL returns RPMRC_FAIL)
 * @param *resultp	js exec result
 * @return		RPMRC_OK on success
 */
rpmRC rpmjsRun(rpmjs js, /*@null@*/ const char * str,
		/*@null@*/ const char ** resultp)
	/*@globals fileSystem, internalState @*/
	/*@modifies js, *resultp, fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif /* RPMJS_H */

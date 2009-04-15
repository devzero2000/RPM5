#ifndef RPMRUBY_H
#define RPMRUBY_H

/** \ingroup rpmio
 * \file rpmio/rpmruby.h
 */

#include <rpmiotypes.h>
#include <rpmio.h>

typedef /*@abstract@*/ struct rpmruby_s * rpmruby;

/*@unchecked@*/
extern int _rpmruby_debug;

/*@unchecked@*/ /*@relnull@*/
extern rpmruby _rpmrubyI;

#if defined(_RPMRUBY_INTERNAL)
struct rpmruby_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
    int flags;
    void * I;
    unsigned long state;
};
#endif /* _RPMRUBY_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a ruby interpreter instance.
 * @param ruby		ruby interpreter
 * @return		NULL on last dereference
 */
/*@unused@*/ /*@null@*/
rpmruby rpmrubyUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmruby ruby)
	/*@modifies ruby @*/;
#define	rpmrubyUnlink(_ds)	\
    ((rpmruby)rpmioUnlinkPoolItem((rpmioItem)(_ruby), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a ruby interpreter instance.
 * @param ruby		ruby interpreter
 * @return		new ruby interpreter reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmruby rpmrubyLink (/*@null@*/ rpmruby ruby)
	/*@modifies ruby @*/;
#define	rpmrubyLink(_ruby)	\
    ((rpmruby)rpmioLinkPoolItem((rpmioItem)(_ruby), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a ruby interpreter.
 * @param ruby		ruby interpreter
 * @return		NULL on last dereference
 */
/*@null@*/
rpmruby rpmrubyFree(/*@killref@*/ /*@null@*/rpmruby ruby)
	/*@globals fileSystem @*/
	/*@modifies ruby, fileSystem @*/;
#define	rpmrubyFree(_ruby)	\
    ((rpmruby)rpmioFreePoolItem((rpmioItem)(_ruby), __FUNCTION__, __FILE__, __LINE__))

/**
 * Create and load a ruby interpreter.
 * @param av		ruby interpreter args (or NULL)
 * @param flags		ruby interpreter flags (1 == use global interpreter)
 * @return		new ruby interpreter
 */
/*@newref@*/ /*@null@*/
rpmruby rpmrubyNew(/*@null@*/ const char ** av, int flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

/**
 * Execute ruby from a file.
 * @param ruby		ruby interpreter (NULL uses global interpreter)
 * @param fn		ruby file to run (NULL returns RPMRC_FAIL)
 * @param *resultp	ruby exec result
 * @return		RPMRC_OK on success
 */
rpmRC rpmrubyRunFile(rpmruby ruby, /*@null@*/ const char * fn,
		/*@null@*/ const char ** resultp)
	/*@globals fileSystem, internalState @*/
	/*@modifies ruby, fileSystem, internalState @*/;

/**
 * Execute ruby string.
 * @param ruby		ruby interpreter (NULL uses global interpreter)
 * @param str		ruby string to execute (NULL returns RPMRC_FAIL)
 * @param *resultp	ruby exec result
 * @return		RPMRC_OK on success
 */
rpmRC rpmrubyRun(rpmruby ruby, /*@null@*/ const char * str,
		/*@null@*/ const char ** resultp)
	/*@globals fileSystem, internalState @*/
	/*@modifies ruby, *resultp, fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif /* RPMRUBY_H */

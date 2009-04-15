#ifndef RPMPYTHON_H
#define RPMPYTHON_H

/** \ingroup rpmio
 * \file rpmio/rpmpython.h
 */

#include <rpmiotypes.h>
#include <rpmio.h>

typedef /*@abstract@*/ struct rpmpython_s * rpmpython;

/*@unchecked@*/
extern int _rpmpython_debug;

/*@unchecked@*/ /*@relnull@*/
extern rpmpython _rpmpythonI;

#if defined(_RPMPYTHON_INTERNAL)
struct rpmpython_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
    const char * fn;
    int flags;
    void * I;			/* (unused) */
};
#endif /* _RPMPYTHON_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a python interpreter instance.
 * @param python	python interpreter
 * @return		NULL on last dereference
 */
/*@unused@*/ /*@null@*/
rpmpython rpmpythonUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmpython python)
	/*@modifies python @*/;
#define	rpmpythonUnlink(_ds)	\
    ((rpmpython)rpmioUnlinkPoolItem((rpmioItem)(_python), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a python interpreter instance.
 * @param python	python interpreter
 * @return		new python interpreter reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmpython rpmpythonLink (/*@null@*/ rpmpython python)
	/*@modifies python @*/;
#define	rpmpythonLink(_python)	\
    ((rpmpython)rpmioLinkPoolItem((rpmioItem)(_python), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a python interpreter.
 * @param python	python interpreter
 * @return		NULL on last dereference
 */
/*@null@*/
rpmpython rpmpythonFree(/*@killref@*/ /*@null@*/rpmpython python)
	/*@globals fileSystem @*/
	/*@modifies python, fileSystem @*/;
#define	rpmpythonFree(_python)	\
    ((rpmpython)rpmioFreePoolItem((rpmioItem)(_python), __FUNCTION__, __FILE__, __LINE__))

/**
 * Create and load a python interpreter.
 * @param fn		(unimplemented) python file to load (or NULL)
 * @param flags		(unimplemented) python interpreter flags
 * @return		new python interpreter
 */
/*@newref@*/ /*@null@*/
rpmpython rpmpythonNew(/*@null@*/ const char * fn, int flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

/**
 * Execute python from a file.
 * @param python	python interpreter
 * @param fn		python file to run (or NULL)
 * @param *resultp	python exec result
 * @return		RPMRC_OK on success
 */
rpmRC rpmpythonRunFile(rpmpython python, /*@null@*/ const char * fn,
		/*@null@*/ const char ** resultp)
	/*@globals fileSystem, internalState @*/
	/*@modifies python, fileSystem, internalState @*/;

/**
 * Execute python string.
 * @param python	python interpreter
 * @param str		python string to execute (or NULL)
 * @param *resultp	python exec result
 * @return		RPMRC_OK on success
 */
rpmRC rpmpythonRun(rpmpython python, /*@null@*/ const char * str,
		/*@null@*/ const char ** resultp)
	/*@globals fileSystem, internalState @*/
	/*@modifies python, *resultp, fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif /* RPMPYTHON_H */

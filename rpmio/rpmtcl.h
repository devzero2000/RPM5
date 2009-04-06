#ifndef RPMTCL_H
#define RPMTCL_H

/** \ingroup rpmio
 * \file rpmio/rpmtcl.h
 */

#include <rpmiotypes.h>
#include <rpmio.h>

typedef /*@abstract@*/ struct rpmtcl_s * rpmtcl;

/*@unchecked@*/
extern int _rpmtcl_debug;

#if defined(_RPMTCL_INTERNAL)
#include <tcl.h>

struct rpmtcl_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
    const char * fn;
    int flags;
    Tcl_Interp * I;
};
#endif /* _RPMTCL_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a tcl interpreter instance.
 * @param tcl		tcl interpreter
 * @return		NULL on last dereference
 */
/*@unused@*/ /*@null@*/
rpmtcl rpmtclUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmtcl tcl)
	/*@modifies tcl @*/;
#define	rpmtclUnlink(_ds)	\
    ((rpmtcl)rpmioUnlinkPoolItem((rpmioItem)(_tcl), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a tcl interpreter instance.
 * @param tcl		tcl interpreter
 * @return		new tcl interpreter reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmtcl rpmtclLink (/*@null@*/ rpmtcl tcl)
	/*@modifies tcl @*/;
#define	rpmtclLink(_tcl)	\
    ((rpmtcl)rpmioLinkPoolItem((rpmioItem)(_tcl), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a tcl interpreter.
 * @param tcl		tcl interpreter
 * @return		NULL on last dereference
 */
/*@null@*/
rpmtcl rpmtclFree(/*@killref@*/ /*@null@*/rpmtcl tcl)
	/*@globals fileSystem @*/
	/*@modifies tcl, fileSystem @*/;
#define	rpmtclFree(_tcl)	\
    ((rpmtcl)rpmioFreePoolItem((rpmioItem)(_tcl), __FUNCTION__, __FILE__, __LINE__))

/**
 * Create and load a tcl interpreter.
 * @param fn		tcl file to load (or NULL)
 * @param flags		tcl interpretr flags
 * @return		new tcl interpreter
 */
/*@newref@*/ /*@null@*/
rpmtcl rpmtclNew(/*@null@*/ const char * fn, int flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

/**
 * Execute tcl from a file.
 * @param tcl		tcl interpreter
 * @param fn		tcl file to run (or NULL)
 * @return		RPMRC_OK on success
 */
rpmRC rpmtclRunFile(rpmtcl tcl, /*@null@*/ const char * fn)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif /* RPMTCL_H */

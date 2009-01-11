#ifndef	H_RPMMG
#define	H_RPMMG

/** \ingroup rpmio
 * \file rpmio/rpmmg.h
 */


/** \ingroup rpmio
 */
/*@unchecked@*/
extern int _rpmmg_debug;

/** \ingroup rpmio
 */
typedef struct rpmmg_s * rpmmg;

#if defined(_RPMMG_INTERNAL)
/** \ingroup rpmio
 */
struct rpmmg_s {
    const char * fn;
    int flags;
/*@relnull@*/
    void * ms;
};
#endif	/* _RPMMG_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Destroy a magic wrapper.
 * @param mg		magic wrapper
 * @return		NULL always
 */
/*@null@*/
rpmmg rpmmgFree(/*@only@*/ /*@null@*/rpmmg mg)
	/*@globals fileSystem @*/
	/*@modifies mg, fileSystem @*/;

/**
 * Create and load a magic wrapper.
 * @param fn		magic file
 * @param flags		magic flags
 * @return		new magic wrapper
 */
/*@only@*/ /*@null@*/
rpmmg rpmmgNew(const char * fn, int flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

/**
 * Return magic string for a file.
 * @param mg		magic wrapper
 * @param fn		file path
 * @return		file magic string
 */
/*@only@*/
const char * rpmmgFile(rpmmg mg, const char *fn)
	/*@globals fileSystem, internalState @*/
	/*@modifies mg, fileSystem, internalState @*/;

/**
 * Return magic string for a buffer.
 * @param mg		magic wrapper
 * @param b		buffer
 * @param nb		no. bytes in buffer
 * @return		buffer magic string
 */
/*@only@*/
const char * rpmmgBuffer(rpmmg mg, const char * b, size_t nb)
	/*@globals fileSystem, internalState @*/
	/*@modifies mg, fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMMG */

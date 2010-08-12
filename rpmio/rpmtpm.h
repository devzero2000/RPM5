#ifndef	H_RPMTPM
#define	H_RPMTPM

/** \ingroup rpmio
 * \file rpmio/rpmtpm.h
 */

#include <stdlib.h>		/* XXX libtpm bootstrapping */
#include <stdint.h>		/* XXX libtpm bootstrapping */
#include <rpmiotypes.h>
#include <rpmlog.h>
#include <rpmcb.h>
#include <argv.h>

/** \ingroup rpmio
 */
/*@unchecked@*/
extern int _rpmtpm_debug;

/** \ingroup rpmio
 */
typedef /*@refcounted@*/ struct rpmtpm_s * rpmtpm;

#if defined(_RPMTPM_INTERNAL)
/** \ingroup rpmio
 */
struct rpmtpm_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */

    int in_fips_mode;	/* XXX trsa */
    int nbits;		/* XXX trsa */
    int qbits;		/* XXX trsa */
    int badok;		/* XXX trsa */
    int err;

    void * digest;
    size_t digestlen;

    int enabled;

#if defined(__LCLINT__)
/*@refs@*/
    int nrefs;			/*!< (unused) keep splint happy */
#endif
};

extern struct rpmtpm_s __tpm;
extern rpmtpm _tpm;

#endif	/* _RPMTPM_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a TPM wrapper instance.
 * @param tpm		TPM wrapper
 * @return		NULL on last dereference
 */
/*@unused@*/ /*@null@*/
rpmtpm rpmtpmUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmtpm tpm)
	/*@modifies tpm @*/;
#define	rpmtpmUnlink(_tpm)	\
    ((rpmtpm)rpmioUnlinkPoolItem((rpmioItem)(_tpm), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a TPM wrapper instance.
 * @param tpm		TPM wrapper
 * @return		new TPM wrapper reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmtpm rpmtpmLink (/*@null@*/ rpmtpm tpm)
	/*@modifies tpm @*/;
#define	rpmtpmLink(_tpm)	\
    ((rpmtpm)rpmioLinkPoolItem((rpmioItem)(_tpm), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a TPM wrapper.
 * @param tpm		TPM wrapper
 * @return		NULL on last dereference
 */
/*@null@*/
rpmtpm rpmtpmFree(/*@killref@*/ /*@null@*/rpmtpm tpm)
	/*@globals fileSystem @*/
	/*@modifies tpm, fileSystem @*/;
#define	rpmtpmFree(_tpm)	\
    ((rpmtpm)rpmioFreePoolItem((rpmioItem)(_tpm), __FUNCTION__, __FILE__, __LINE__))

/**
 * Create and load a TPM wrapper.
 * @param fn		TPM file
 * @param flags		TPM flags
 * @return		new TPM wrapper
 */
/*@newref@*/ /*@null@*/
rpmtpm rpmtpmNew(const char * fn, int flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

int rpmtpmErr(rpmtpm tpm, const char * msg, uint32_t mask, uint32_t rc)
	/*@*/;
void rpmtpmDump(rpmtpm tpm, const char * msg, unsigned char * b, size_t nb)
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMTPM */

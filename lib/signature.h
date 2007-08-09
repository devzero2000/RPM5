#ifndef H_SIGNATURE
#define	H_SIGNATURE

/** \ingroup signature
 * \file lib/signature.h
 * Generate and verify signatures.
 */

#include <header.h>

/** \ingroup signature
 * Signature types stored in rpm lead.
 */
typedef	enum sigType_e {
    RPMSIGTYPE_HEADERSIG= 5	/*!< Header style signature */
} sigType;

/** \ingroup signature
 * Identify PGP versions.
 * @note Greater than 0 is a valid PGP version.
 */
typedef enum pgpVersion_e {
    PGP_NOTDETECTED	= -1,
    PGP_UNKNOWN		= 0,
    PGP_2		= 2,
    PGP_5		= 5
} pgpVersion;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup signature
 * Return new, empty (signature) header instance.
 * @return		signature header
 */
Header rpmNewSignature(void)
	/*@*/;

/** \ingroup signature
 * Read (and verify header+payload size) signature header.
 * If an old-style signature is found, we emulate a new style one.
 * @param _fd		file handle
 * @retval sighp	address of (signature) header (or NULL)
 * @param sig_type	type of signature header to read (from lead)
 * @retval msg		failure msg
 * @return		rpmRC return code
 */
rpmRC rpmReadSignature(void * _fd, /*@null@*/ /*@out@*/ Header *sighp,
		sigType sig_type, /*@null@*/ /*@out@*/ const char ** msg)
	/*@globals fileSystem @*/
	/*@modifies _fd, *sighp, *msg, fileSystem @*/;

/** \ingroup signature
 * Write signature header.
 * @param _fd		file handle
 * @param sigh		(signature) header
 * @return		0 on success, 1 on error
 */
int rpmWriteSignature(void * _fd, Header sigh)
	/*@globals fileSystem @*/
	/*@modifies _fd, sigh, fileSystem @*/;

/** \ingroup signature
 * Generate signature(s) from a header+payload file, save in signature header.
 * @param sigh		signature header
 * @param file		header+payload file name
 * @param sigTag	type of signature(s) to add
 * @param passPhrase	private key pass phrase
 * @return		0 on success, -1 on failure
 */
int rpmAddSignature(Header sigh, const char * file,
		    int_32 sigTag, /*@null@*/ const char * passPhrase)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies sigh, sigTag, rpmGlobalMacroContext, fileSystem, internalState @*/;

/**
 * Check for valid pass phrase by invoking a helper.
 * @param passPhrase	pass phrase
 * @return		0 on valid, 1 on invalid
 */
int rpmCheckPassPhrase(const char * passPhrase)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext, fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_SIGNATURE */

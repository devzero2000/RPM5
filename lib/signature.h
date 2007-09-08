#ifndef H_SIGNATURE
#define	H_SIGNATURE

/** \ingroup signature
 * \file lib/signature.h
 * Generate and verify rpm package signatures.
 */

#include <header.h>

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
 * Return new, empty signature header instance.
 * @return		signature header
 */
Header rpmNewSignature(void)
	/*@*/;

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

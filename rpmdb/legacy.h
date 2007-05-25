#ifndef H_LEGACY
#define H_LEGACY

/**
 * \file rpmdb/legacy.h
 *
 */

/**
 */
/*@-redecl@*/
/*@unchecked@*/
extern int _noDirTokens;
/*@=redecl@*/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return digest and size of a file.
 * @param digestalgo	digest algorithm to use
 * @param fn		file name
 * @retval digest	address of md5sum
 * @param asAscii	return md5sum as ascii string?
 * @retval *fsizep	file size pointer (or NULL)
 * @return		0 on success, 1 on error
 */
int dodigest(int digestalgo, const char * fn, /*@out@*/ unsigned char * digest,
		int asAscii, /*@null@*/ /*@out@*/ size_t *fsizep)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies digest, *fsizep, fileSystem, internalState @*/;

/**
 * Return MD5 digest and size of a file.
 * @param fn		file name
 * @retval digest	address of md5sum
 * @param asAscii	return md5sum as ascii string?
 * @retval *fsizep	file size pointer (or NULL)
 * @return		0 on success, 1 on error
 */
int domd5(const char * fn, /*@out@*/ unsigned char * digest, int asAscii,
		/*@null@*/ /*@out@*/ size_t *fsizep)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies digest, *fsizep, fileSystem, internalState @*/;

/**
 * Retrieve file names from header.
 *
 * The representation of file names in package headers changed in rpm-4.0.
 * Originally, file names were stored as an array of absolute paths.
 * In rpm-4.0, file names are stored as separate arrays of dirname's and
 * basename's, * with a dirname index to associate the correct dirname
 * with each basname.
 *
 * This function is used to retrieve file names independent of how the
 * file names are represented in the package header.
 * 
 * @param h		header
 * @param tagN		RPMTAG_BASENAMES | PMTAG_ORIGBASENAMES
 * @retval *fnp		array of file names
 * @retval *fcp		number of files
 */
void rpmfiBuildFNames(Header h, rpmTag tagN,
		/*@out@*/ const char *** fnp, /*@out@*/ int * fcp)
	/*@modifies *fnp, *fcp @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_LEGACY */

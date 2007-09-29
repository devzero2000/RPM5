#ifndef _H_PKGIO
#define _H_PKGIO

/** \ingroup lead
 * \file rpmdb/pkgio.h
 * Methods to handle package elements.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*@unchecked@*/
extern int _nolead;

/**
 * Return size of item in bytes.
 * @param fn		item name
 * @param ptr		item buffer
 * @return		size of item in bytes.
 */
size_t rpmpkgSizeof(const char * fn, /*@null@*/ const void * ptr)
	/*@*/;

/**
 * Write item onto file descriptor.
 * @param fn		item name
 * @param fd		file handle
 * @param ptr		item buffer
 * @retval *msg		item check failure message
 * @return		RPMRC_OK on success
 */
rpmRC rpmpkgWrite(const char * fn, FD_t fd, void * ptr, const char ** msg)
	/*@globals fileSystem @*/
	/*@modifies fd, fileSystem @*/;

/**
 * Read item from file descriptor.
 * @param fn		item name
 * @param fd		file handle
 * @retval *ptr		item buffer
 * @retval *msg		item check failure message
 * @return		RPMRC_OK on success
 */
rpmRC rpmpkgRead(const char * fn, FD_t fd, /*@null@*/ /*@out@*/ void * ptr,
		const char ** msg)
	/*@modifies fd, *ptr, *msg @*/;

/**
 * Verify item integrity.
 * @param fn		item name
 * @param fd		file handle
 * @param ptr		item buffer
 * @retval *msg		item check failure message
 * @return		RPMRC_OK on success
 */
rpmRC rpmpkgCheck(const char * fn, FD_t fd, const void * ptr, const char ** msg)
	/*@modifies fd, *msg @*/;

#ifdef __cplusplus
}
#endif

#endif	/* _H_PKGIO */

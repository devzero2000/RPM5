#ifndef _H_RPMKU_
#define	_H_RPMKU_

/** \ingroup rpmio
 * \file rpmio/rpmku.h
 */

/**
 * Keyutils keyring to use.
 */
/*@unchecked@*/
extern int32_t _kuKeyring;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Lookup pubkey in keyutils keyring.
 * @param sigp		signature packet
 * @retval *bp		pubkey I/O buffer
 * @retval *blenp	pubkey I/O buffer length
 * @return		RPMRC_OK on success
 */
rpmRC rpmkuFindPubkey(pgpDigParams sigp, /*@out@*/ uint8_t ** bp, /*@out@*/ size_t * blenp)
	/*@modifies *bp, *blenp @*/;

/**
 * Store pubkey in keyutils keyring.
 * @param sigp		signature packet
 * @param b		pubkey I/O buffer
 * @param blen		pubkey I/O buffer length
 * @return		RPMRC_OK on success
 */
rpmRC rpmkuStorePubkey(pgpDigParams sigp, /*@only@*/ uint8_t * b, size_t blen)
	/*@modifies b @*/;

/**
 * Return pass phrase from keyutils keyring.
 * @param passPhrase	pass phrase
 * @return		(malloc'd) pass phrase
 */
/*@null@*/
const char * rpmkuPassPhrase(const char * passPhrase)
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif /* _H_RPMKU_ */

/*!\file md4.h
 * \brief MD4 hash function.
 * \ingroup HASH_m HASH_md4_m 
 */

#ifndef  _MD4_H
#define  _MD4_H

#include "beecrypt.h"

/*!\brief Holds all the parameters necessary for the MD4 algorithm.
 * \ingroup HASH_md4_h
 */
typedef struct
{
	/*!\var cksum
	 */
	uint32_t h[4];
	/*!\var buf
	 */
	uint32_t data[16];
	/*!\var length
	 * \brief Multi-precision integer counter for the bits that have been
	 *  processed so far.
	 */
	#if (MP_WBITS == 64)
	mpw length[1];
	#elif (MP_WBITS == 32)
	mpw length[2];
	#else
	# error
	#endif
    /*!\var offset
     * \brief Offset into \a data; points to the place where new data will be
     *  copied before it is processed.
     */
	uint32_t offset;
} md4Param;

#ifdef __cplusplus
extern "C" {
#endif

/*!\var md4
 * \brief Holds the full API description of the MD4 algorithm.
 */
/*@unchecked@*/ /*@observer@*/
extern BEECRYPTAPI const hashFunction md4;

/*!\fn int md4Reset(md4Param* mp)
 * \brief This function resets the parameter block so that it's ready for a
 *  new hash.
 * \param mp The hash function's parameter block.
 * \retval 0 on success.
 */
BEECRYPTAPI
void md4Process(md4Param* mp)
	/*@modifies mp @*/;

/*!\fn int md4Reset(md4Param* mp)
 * \brief This function resets the parameter block so that it's ready for a
 *  new hash.
 * \param mp The hash function's parameter block.
 * \retval 0 on success.
 */
BEECRYPTAPI
int md4Reset   (md4Param* mp)
	/*@modifies mp @*/;

/*!\fn int md4Update(md4Param* mp, const byte* data, size_t size)
 * \brief This function should be used to pass successive blocks of data
 *  to be hashed.
 * \param mp The hash function's parameter block.
 * \param data
 * \param size
 * \retval 0 on success.
 */
BEECRYPTAPI
int md4Update  (md4Param* mp, const byte* data, size_t size)
	/*@modifies mp @*/;

/*!\fn int md4Digest(md4Param* mp, byte* digest)
 * \brief This function finishes the current hash computation and copies
 *  the digest value into \a digest.
 * \param mp The hash function's parameter block.
 * \param digest The place to store the 16-byte digest.
 * \retval 0 on success.
 */
BEECRYPTAPI
int md4Digest  (md4Param* mp, byte* digest)
	/*@modifies mp, digest @*/;

#ifdef __cplusplus
}
#endif

#endif

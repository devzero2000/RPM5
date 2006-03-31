/*!\file crc32.h
 * \brief CRC-32 function.
 */

#ifndef  _CRC32_H
#define  _CRC32_H

#include "beecrypt.h"

/*!\brief Holds all the parameters necessary for the CRC-32 algorithm.
 */
typedef struct
{
	/*!\var h
	 */
	uint32_t h[1];
	uint32_t polynomial;
	uint32_t table[256];
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
} crc32Param;

#ifdef __cplusplus
extern "C" {
#endif

/*!\var crc32
 * \brief Holds the full API description of the RIPEMD-128 algorithm.
 */
/*@unchecked@*/ /*@observer@*/
extern BEECRYPTAPI const hashFunction crc32;

/*!\fn int crc32Reset(crc32Param* mp)
 * \brief This function resets the parameter block so that it's ready for a
 *  new hash.
 * \param mp The hash function's parameter block.
 * \retval 0 on success.
 */
BEECRYPTAPI
int crc32Reset   (crc32Param* mp)
	/*@modifies mp @*/;

/*!\fn int crc32Update(crc32Param* mp, const byte* data, size_t size)
 * \brief This function should be used to pass successive blocks of data
 *  to be hashed.
 * \param mp The hash function's parameter block.
 * \param data
 * \param size
 * \retval 0 on success.
 */
BEECRYPTAPI
int crc32Update  (crc32Param* mp, const byte* data, size_t size)
	/*@modifies mp @*/;

/*!\fn int crc32Digest(crc32Param* mp, byte* digest)
 * \brief This function finishes the current hash computation and copies
 *  the digest value into \a digest.
 * \param mp The hash function's parameter block.
 * \param digest The place to store the 20-byte digest.
 * \retval 0 on success.
 */
BEECRYPTAPI
int crc32Digest  (crc32Param* mp, byte* digest)
	/*@modifies mp, digest @*/;

#ifdef __cplusplus
}
#endif

#endif

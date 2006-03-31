#include "system.h"

#include "crc32.h"      
#include "mp.h"
#include "endianness.h"

#include "debug.h"

/*@unchecked@*/ /*@observer@*/
static uint32_t crc32hinit[1] = { 0x04c11db7 };

/*@-sizeoftype@*/
/*@unchecked@*/ /*@observer@*/
const hashFunction crc32 = {
	"CRC-32",
	sizeof(crc32Param),
	1,
	32/8,
	(hashFunctionReset) crc32Reset,
	(hashFunctionUpdate) crc32Update,
	(hashFunctionDigest) crc32Digest
};
/*@=sizeoftype@*/

int crc32Reset(register crc32Param* mp)
{
/*@-sizeoftype@*/
	memset(mp->h, -1, 1 * sizeof(uint32_t));
	mp->polynomial = crc32hinit[0];
	/* generate the table of CRC remainders for all possible bytes */
	{   uint32_t crc;
	    uint32_t i, j;
	    for (i = 0;  i < 256;  i++) {
		crc = (i << 24);
		for (j = 0;  j < 8;  j++) {
	             if (crc & 0x80000000)
	                crc = (crc << 1) ^ mp->polynomial;
	             else
	                crc = (crc << 1);
		}
		#if WORDS_BIGENDIAN
	        mp->table[i] = crc;
		#else
	        mp->table[i] = swapu32(&crc);
		#endif
	    }
	}
/*@=sizeoftype@*/
	#if (MP_WBITS == 64)
	mpzero(1, mp->length);
	#elif (MP_WBITS == 32)
	mpzero(2, mp->length);
	#else
	# error
	#endif
	return 0;
}

int crc32Update(crc32Param* mp, const byte* data, size_t size)
{
	register uint32_t *b = (uint32_t *) data;
	register uint32_t *be = (uint32_t *) (data + size);
	uint32_t result;

	#if (MP_WBITS == 64)
	mpw add[1];
	mpsetw(1, add, size);
	mplshift(1, add, 3);
	mpadd(1, mp->length, add);
	#elif (MP_WBITS == 32)
	mpw add[2];
	mpsetw(2, add, size);
	mplshift(2, add, 3);
	(void) mpadd(2, mp->length, add);
	#else
	# error
	#endif

	result = ~*b++;
	while (b < be) {
		#if WORDS_BIGENDIAN
		result = mp->table[result >> 24] ^ result << 8;
		result = mp->table[result >> 24] ^ result << 8;
		result = mp->table[result >> 24] ^ result << 8;
		result = mp->table[result >> 24] ^ result << 8;
		result ^= *b++;
		#else
		result = mp->table[result & 0xff] ^ result >> 8;
		result = mp->table[result & 0xff] ^ result >> 8;
		result = mp->table[result & 0xff] ^ result >> 8;
		result = mp->table[result & 0xff] ^ result >> 8;
		result ^= *b++;
		#endif
	}
	result = ~result;

	memcpy(mp->h, &result, 4);

	return 0;
}

static void crc32Finish(crc32Param* mp)
	/*@modifies mp @*/
{
#ifdef	NOTYET
	byte b[1+8];
	register byte *ptr = b;

	*(ptr++) = 0x80;

	#if (MP_WBITS == 64)
	ptr[0] = (byte)(mp->length[0]      );
	ptr[1] = (byte)(mp->length[0] >>  8);
	ptr[2] = (byte)(mp->length[0] >> 16);
	ptr[3] = (byte)(mp->length[0] >> 24);
	ptr[4] = (byte)(mp->length[0] >> 32);
	ptr[5] = (byte)(mp->length[0] >> 40);
	ptr[6] = (byte)(mp->length[0] >> 48);
	ptr[7] = (byte)(mp->length[0] >> 56);
	#elif (MP_WBITS == 32)
	ptr[0] = (byte)(mp->length[1]      );
	ptr[1] = (byte)(mp->length[1] >>  8);
	ptr[2] = (byte)(mp->length[1] >> 16);
	ptr[3] = (byte)(mp->length[1] >> 24);
	ptr[4] = (byte)(mp->length[0]      );
	ptr[5] = (byte)(mp->length[0] >>  8);
	ptr[6] = (byte)(mp->length[0] >> 16);
	ptr[7] = (byte)(mp->length[0] >> 24);
	#else
	# error
	#endif
	ptr += 8;

	crc32Update(mp, b, (ptr-b));
#endif
}

/*@-protoparammatch@*/
int crc32Digest(crc32Param* mp, byte* data)
{
	crc32Finish(mp);

	/* encode 1 integer little-endian style */
	data[ 0] = (byte)(mp->h[0]      );
	data[ 1] = (byte)(mp->h[0] >>  8);
	data[ 2] = (byte)(mp->h[0] >> 16);
	data[ 3] = (byte)(mp->h[0] >> 24);

	(void) crc32Reset(mp);
	return 0;
}
/*@=protoparammatch@*/

/*!\}
 */

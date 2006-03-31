/********************************************************************\
 *
 *      FILE:     rmd128.c
 *
 *      CONTENTS: A sample C-implementation of the RIPEMD-128
 *                hash-function. This function is a plug-in substitute 
 *                for RIPEMD. A 160-bit hash result is obtained using
 *                RIPEMD-160.
 *      TARGET:   any computer with an ANSI C compiler
 *
 *      AUTHOR:   Antoon Bosselaers, ESAT-COSIC
 *      DATE:     1 March 1996
 *      VERSION:  1.0
 *
 *      Copyright (c) Katholieke Universiteit Leuven
 *      1996, All Rights Reserved
 *
 *  Conditions for use of the RIPEMD-160 Software
 *
 *  The RIPEMD-160 software is freely available for use under the terms and
 *  conditions described hereunder, which shall be deemed to be accepted by
 *  any user of the software and applicable on any use of the software:
 * 
 *  1. K.U.Leuven Department of Electrical Engineering-ESAT/COSIC shall for
 *     all purposes be considered the owner of the RIPEMD-160 software and of
 *     all copyright, trade secret, patent or other intellectual property
 *     rights therein.
 *  2. The RIPEMD-160 software is provided on an "as is" basis without
 *     warranty of any sort, express or implied. K.U.Leuven makes no
 *     representation that the use of the software will not infringe any
 *     patent or proprietary right of third parties. User will indemnify
 *     K.U.Leuven and hold K.U.Leuven harmless from any claims or liabilities
 *     which may arise as a result of its use of the software. In no
 *     circumstances K.U.Leuven R&D will be held liable for any deficiency,
 *     fault or other mishappening with regard to the use or performance of
 *     the software.
 *  3. User agrees to give due credit to K.U.Leuven in scientific publications 
 *     or communications in relation with the use of the RIPEMD-160 software 
 *     as follows: RIPEMD-160 software written by Antoon Bosselaers, 
 *     available at http://www.esat.kuleuven.be/~cosicart/ps/AB-9601/.
 *
\********************************************************************/

#include "system.h"

#include "rmd128.h"      
#include "mp.h"
#include "endianness.h"

#include "debug.h"

/*!\addtogroup HASH_rmd128_m
 * \{
 */

/*@unchecked@*/ /*@observer@*/
static uint32_t rmd128hinit[4] =
	{ 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };

/*@-sizeoftype@*/
/*@unchecked@*/ /*@observer@*/
const hashFunction rmd128 = {
	"RIPEMD128",
	sizeof(rmd128Param),
	64,
	16,
	(hashFunctionReset) rmd128Reset,
	(hashFunctionUpdate) rmd128Update,
	(hashFunctionDigest) rmd128Digest
};
/*@=sizeoftype@*/

int rmd128Reset(register rmd128Param* mp)
{
/*@-sizeoftype@*/
	memcpy(mp->h, rmd128hinit, 4 * sizeof(uint32_t));
	memset(mp->data, 0, 16 * sizeof(uint32_t));
/*@=sizeoftype@*/
	#if (MP_WBITS == 64)
	mpzero(1, mp->length);
	#elif (MP_WBITS == 32)
	mpzero(2, mp->length);
	#else
	# error
	#endif
	mp->offset = 0;
	return 0;
}

/* ROL(x, n) cyclically rotates x over n bits to the left */
/* x must be of an unsigned 32 bits type and 0 <= n < 32. */
#define ROL(x, n)        (((x) << (n)) | ((x) >> (32-(n))))

/* the four basic functions F(), G() and H() */
#define F(x, y, z)        ((x) ^ (y) ^ (z)) 
#define G(x, y, z)        (((x) & (y)) | (~(x) & (z))) 
#define H(x, y, z)        (((x) | ~(y)) ^ (z))
#define I(x, y, z)        (((x) & (z)) | ((y) & ~(z))) 
  
/* the eight basic operations FF() through III() */
#define FF(a, b, c, d, x, s)        {\
      (a) += F((b), (c), (d)) + (x);\
      (a) = ROL((a), (s));\
   }
#define GG(a, b, c, d, x, s)        {\
      (a) += G((b), (c), (d)) + (x) + 0x5a827999UL;\
      (a) = ROL((a), (s));\
   }
#define HH(a, b, c, d, x, s)        {\
      (a) += H((b), (c), (d)) + (x) + 0x6ed9eba1UL;\
      (a) = ROL((a), (s));\
   }
#define II(a, b, c, d, x, s)        {\
      (a) += I((b), (c), (d)) + (x) + 0x8f1bbcdcUL;\
      (a) = ROL((a), (s));\
   }
#define FFF(a, b, c, d, x, s)        {\
      (a) += F((b), (c), (d)) + (x);\
      (a) = ROL((a), (s));\
   }
#define GGG(a, b, c, d, x, s)        {\
      (a) += G((b), (c), (d)) + (x) + 0x6d703ef3UL;\
      (a) = ROL((a), (s));\
   }
#define HHH(a, b, c, d, x, s)        {\
      (a) += H((b), (c), (d)) + (x) + 0x5c4dd124UL;\
      (a) = ROL((a), (s));\
   }
#define III(a, b, c, d, x, s)        {\
      (a) += I((b), (c), (d)) + (x) + 0x50a28be6UL;\
      (a) = ROL((a), (s));\
   }

#ifndef ASM_RMD128PROCESS
void rmd128Process(rmd128Param* mp)
{
	register uint32_t aa,bb,cc,dd;
	register uint32_t aaa,bbb,ccc,ddd;
	register uint32_t* X;
	#if WORDS_BIGENDIAN
	register byte t;
	#endif

	X = mp->data;
	#if WORDS_BIGENDIAN
	t = 16;
	while (t--)
	{
		register uint32_t temp = swapu32(*w);
		*(w++) = temp;
	}
	X = mp->data;
	#endif

	aa = mp->h[0]; bb = mp->h[1]; cc = mp->h[2]; dd = mp->h[3];
	aaa = mp->h[0]; bbb = mp->h[1]; ccc = mp->h[2]; ddd = mp->h[3];

	/* round 1 */
	FF(aa, bb, cc, dd, X[ 0], 11);
	FF(dd, aa, bb, cc, X[ 1], 14);
	FF(cc, dd, aa, bb, X[ 2], 15);
	FF(bb, cc, dd, aa, X[ 3], 12);
	FF(aa, bb, cc, dd, X[ 4],  5);
	FF(dd, aa, bb, cc, X[ 5],  8);
	FF(cc, dd, aa, bb, X[ 6],  7);
	FF(bb, cc, dd, aa, X[ 7],  9);
	FF(aa, bb, cc, dd, X[ 8], 11);
	FF(dd, aa, bb, cc, X[ 9], 13);
	FF(cc, dd, aa, bb, X[10], 14);
	FF(bb, cc, dd, aa, X[11], 15);
	FF(aa, bb, cc, dd, X[12],  6);
	FF(dd, aa, bb, cc, X[13],  7);
	FF(cc, dd, aa, bb, X[14],  9);
	FF(bb, cc, dd, aa, X[15],  8);
	                          
	/* round 2 */
	GG(aa, bb, cc, dd, X[ 7],  7);
	GG(dd, aa, bb, cc, X[ 4],  6);
	GG(cc, dd, aa, bb, X[13],  8);
	GG(bb, cc, dd, aa, X[ 1], 13);
	GG(aa, bb, cc, dd, X[10], 11);
	GG(dd, aa, bb, cc, X[ 6],  9);
	GG(cc, dd, aa, bb, X[15],  7);
	GG(bb, cc, dd, aa, X[ 3], 15);
	GG(aa, bb, cc, dd, X[12],  7);
	GG(dd, aa, bb, cc, X[ 0], 12);
	GG(cc, dd, aa, bb, X[ 9], 15);
	GG(bb, cc, dd, aa, X[ 5],  9);
	GG(aa, bb, cc, dd, X[ 2], 11);
	GG(dd, aa, bb, cc, X[14],  7);
	GG(cc, dd, aa, bb, X[11], 13);
	GG(bb, cc, dd, aa, X[ 8], 12);

	/* round 3 */
	HH(aa, bb, cc, dd, X[ 3], 11);
	HH(dd, aa, bb, cc, X[10], 13);
	HH(cc, dd, aa, bb, X[14],  6);
	HH(bb, cc, dd, aa, X[ 4],  7);
	HH(aa, bb, cc, dd, X[ 9], 14);
	HH(dd, aa, bb, cc, X[15],  9);
	HH(cc, dd, aa, bb, X[ 8], 13);
	HH(bb, cc, dd, aa, X[ 1], 15);
	HH(aa, bb, cc, dd, X[ 2], 14);
	HH(dd, aa, bb, cc, X[ 7],  8);
	HH(cc, dd, aa, bb, X[ 0], 13);
	HH(bb, cc, dd, aa, X[ 6],  6);
	HH(aa, bb, cc, dd, X[13],  5);
	HH(dd, aa, bb, cc, X[11], 12);
	HH(cc, dd, aa, bb, X[ 5],  7);
	HH(bb, cc, dd, aa, X[12],  5);

	/* round 4 */
	II(aa, bb, cc, dd, X[ 1], 11);
	II(dd, aa, bb, cc, X[ 9], 12);
	II(cc, dd, aa, bb, X[11], 14);
	II(bb, cc, dd, aa, X[10], 15);
	II(aa, bb, cc, dd, X[ 0], 14);
	II(dd, aa, bb, cc, X[ 8], 15);
	II(cc, dd, aa, bb, X[12],  9);
	II(bb, cc, dd, aa, X[ 4],  8);
	II(aa, bb, cc, dd, X[13],  9);
	II(dd, aa, bb, cc, X[ 3], 14);
	II(cc, dd, aa, bb, X[ 7],  5);
	II(bb, cc, dd, aa, X[15],  6);
	II(aa, bb, cc, dd, X[14],  8);
	II(dd, aa, bb, cc, X[ 5],  6);
	II(cc, dd, aa, bb, X[ 6],  5);
	II(bb, cc, dd, aa, X[ 2], 12);

	/* parallel round 1 */
	III(aaa, bbb, ccc, ddd, X[ 5],  8); 
	III(ddd, aaa, bbb, ccc, X[14],  9);
	III(ccc, ddd, aaa, bbb, X[ 7],  9);
	III(bbb, ccc, ddd, aaa, X[ 0], 11);
	III(aaa, bbb, ccc, ddd, X[ 9], 13);
	III(ddd, aaa, bbb, ccc, X[ 2], 15);
	III(ccc, ddd, aaa, bbb, X[11], 15);
	III(bbb, ccc, ddd, aaa, X[ 4],  5);
	III(aaa, bbb, ccc, ddd, X[13],  7);
	III(ddd, aaa, bbb, ccc, X[ 6],  7);
	III(ccc, ddd, aaa, bbb, X[15],  8);
	III(bbb, ccc, ddd, aaa, X[ 8], 11);
	III(aaa, bbb, ccc, ddd, X[ 1], 14);
	III(ddd, aaa, bbb, ccc, X[10], 14);
	III(ccc, ddd, aaa, bbb, X[ 3], 12);
	III(bbb, ccc, ddd, aaa, X[12],  6);

	/* parallel round 2 */
	HHH(aaa, bbb, ccc, ddd, X[ 6],  9);
	HHH(ddd, aaa, bbb, ccc, X[11], 13);
	HHH(ccc, ddd, aaa, bbb, X[ 3], 15);
	HHH(bbb, ccc, ddd, aaa, X[ 7],  7);
	HHH(aaa, bbb, ccc, ddd, X[ 0], 12);
	HHH(ddd, aaa, bbb, ccc, X[13],  8);
	HHH(ccc, ddd, aaa, bbb, X[ 5],  9);
	HHH(bbb, ccc, ddd, aaa, X[10], 11);
	HHH(aaa, bbb, ccc, ddd, X[14],  7);
	HHH(ddd, aaa, bbb, ccc, X[15],  7);
	HHH(ccc, ddd, aaa, bbb, X[ 8], 12);
	HHH(bbb, ccc, ddd, aaa, X[12],  7);
	HHH(aaa, bbb, ccc, ddd, X[ 4],  6);
	HHH(ddd, aaa, bbb, ccc, X[ 9], 15);
	HHH(ccc, ddd, aaa, bbb, X[ 1], 13);
	HHH(bbb, ccc, ddd, aaa, X[ 2], 11);

	/* parallel round 3 */   
	GGG(aaa, bbb, ccc, ddd, X[15],  9);
	GGG(ddd, aaa, bbb, ccc, X[ 5],  7);
	GGG(ccc, ddd, aaa, bbb, X[ 1], 15);
	GGG(bbb, ccc, ddd, aaa, X[ 3], 11);
	GGG(aaa, bbb, ccc, ddd, X[ 7],  8);
	GGG(ddd, aaa, bbb, ccc, X[14],  6);
	GGG(ccc, ddd, aaa, bbb, X[ 6],  6);
	GGG(bbb, ccc, ddd, aaa, X[ 9], 14);
	GGG(aaa, bbb, ccc, ddd, X[11], 12);
	GGG(ddd, aaa, bbb, ccc, X[ 8], 13);
	GGG(ccc, ddd, aaa, bbb, X[12],  5);
	GGG(bbb, ccc, ddd, aaa, X[ 2], 14);
	GGG(aaa, bbb, ccc, ddd, X[10], 13);
	GGG(ddd, aaa, bbb, ccc, X[ 0], 13);
	GGG(ccc, ddd, aaa, bbb, X[ 4],  7);
	GGG(bbb, ccc, ddd, aaa, X[13],  5);

	/* parallel round 4 */
	FFF(aaa, bbb, ccc, ddd, X[ 8], 15);
	FFF(ddd, aaa, bbb, ccc, X[ 6],  5);
	FFF(ccc, ddd, aaa, bbb, X[ 4],  8);
	FFF(bbb, ccc, ddd, aaa, X[ 1], 11);
	FFF(aaa, bbb, ccc, ddd, X[ 3], 14);
	FFF(ddd, aaa, bbb, ccc, X[11], 14);
	FFF(ccc, ddd, aaa, bbb, X[15],  6);
	FFF(bbb, ccc, ddd, aaa, X[ 0], 14);
	FFF(aaa, bbb, ccc, ddd, X[ 5],  6);
	FFF(ddd, aaa, bbb, ccc, X[12],  9);
	FFF(ccc, ddd, aaa, bbb, X[ 2], 12);
	FFF(bbb, ccc, ddd, aaa, X[13],  9);
	FFF(aaa, bbb, ccc, ddd, X[ 9], 12);
	FFF(ddd, aaa, bbb, ccc, X[ 7],  5);
	FFF(ccc, ddd, aaa, bbb, X[10], 15);
	FFF(bbb, ccc, ddd, aaa, X[14],  8);

	/* combine results */
	ddd += cc + mp->h[1];
	mp->h[1] = mp->h[2] + dd + aaa;
	mp->h[2] = mp->h[3] + aa + bbb;
	mp->h[3] = mp->h[0] + bb + ccc;
	mp->h[0] = ddd;
}
#endif

int rmd128Update(rmd128Param* mp, const byte* data, size_t size)
{
	register uint32_t proclength;

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

	while (size > 0)
	{
		proclength = ((mp->offset + size) > 64U) ? (64U - mp->offset) : size;
/*@-mayaliasunique@*/
		memcpy(((byte *) mp->data) + mp->offset, data, proclength);
/*@=mayaliasunique@*/
		size -= proclength;
		data += proclength;
		mp->offset += proclength;

		if (mp->offset == 64U)
		{
			rmd128Process(mp);
			mp->offset = 0;
		}
	}
	return 0;
}

static void rmd128Finish(rmd128Param* mp)
	/*@modifies mp @*/
{
	register byte *ptr = ((byte *) mp->data) + mp->offset++;

	*(ptr++) = 0x80;

	if (mp->offset > 56)
	{
		while (mp->offset++ < 64)
			*(ptr++) = 0;

		rmd128Process(mp);
		mp->offset = 0;
	}

	ptr = ((byte *) mp->data) + mp->offset;
	while (mp->offset++ < 56)
		*(ptr++) = 0;

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

	rmd128Process(mp);

	mp->offset = 0;
}

/*@-protoparammatch@*/
int rmd128Digest(rmd128Param* mp, byte* data)
{
	rmd128Finish(mp);

	/* encode 4 integers little-endian style */
	data[ 0] = (byte)(mp->h[0]      );
	data[ 1] = (byte)(mp->h[0] >>  8);
	data[ 2] = (byte)(mp->h[0] >> 16);
	data[ 3] = (byte)(mp->h[0] >> 24);
	data[ 4] = (byte)(mp->h[1]      );
	data[ 5] = (byte)(mp->h[1] >>  8);
	data[ 6] = (byte)(mp->h[1] >> 16);
	data[ 7] = (byte)(mp->h[1] >> 24);
	data[ 8] = (byte)(mp->h[2]      );
	data[ 9] = (byte)(mp->h[2] >>  8);
	data[10] = (byte)(mp->h[2] >> 16);
	data[11] = (byte)(mp->h[2] >> 24);
	data[12] = (byte)(mp->h[3]      );
	data[13] = (byte)(mp->h[3] >>  8);
	data[14] = (byte)(mp->h[3] >> 16);
	data[15] = (byte)(mp->h[3] >> 24);

	(void) rmd128Reset(mp);
	return 0;
}
/*@=protoparammatch@*/

/*!\}
 */

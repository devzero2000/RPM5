/********************************************************************\
 *
 *      FILE:     rmd160.c
 *
 *      CONTENTS: A sample C-implementation of the RIPEMD-160
 *                hash-function.
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

#include "rmd160.h"      
#include "mp.h"
#include "endianness.h"

#include "debug.h"

/*!\addtogroup HASH_rmd160_m
 * \{
 */

/*@unchecked@*/ /*@observer@*/
static uint32_t rmd160hinit[5] =
	{ 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0 };

/*@-sizeoftype@*/
/*@unchecked@*/ /*@observer@*/
const hashFunction rmd160 = {
	"RIPEMD-160",
	sizeof(rmd160Param),
	64,
	160/8,
	(hashFunctionReset) rmd160Reset,
	(hashFunctionUpdate) rmd160Update,
	(hashFunctionDigest) rmd160Digest
};
/*@=sizeoftype@*/

int rmd160Reset(register rmd160Param* mp)
{
/*@-sizeoftype@*/
	memcpy(mp->h, rmd160hinit, 5 * sizeof(uint32_t));
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

/* the five basic functions F(), G() and H() */
#define F(x, y, z)        ((x) ^ (y) ^ (z)) 
#define G(x, y, z)        (((x) & (y)) | (~(x) & (z))) 
#define H(x, y, z)        (((x) | ~(y)) ^ (z))
#define I(x, y, z)        (((x) & (z)) | ((y) & ~(z))) 
#define J(x, y, z)        ((x) ^ ((y) | ~(z)))
  
/* the ten basic operations FF() through III() */
#define FF(a, b, c, d, e, x, s)        {\
      (a) += F((b), (c), (d)) + (x);\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }
#define GG(a, b, c, d, e, x, s)        {\
      (a) += G((b), (c), (d)) + (x) + 0x5a827999;\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }
#define HH(a, b, c, d, e, x, s)        {\
      (a) += H((b), (c), (d)) + (x) + 0x6ed9eba1;\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }
#define II(a, b, c, d, e, x, s)        {\
      (a) += I((b), (c), (d)) + (x) + 0x8f1bbcdc;\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }
#define JJ(a, b, c, d, e, x, s)        {\
      (a) += J((b), (c), (d)) + (x) + 0xa953fd4e;\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }
#define FFF(a, b, c, d, e, x, s)        {\
      (a) += F((b), (c), (d)) + (x);\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }
#define GGG(a, b, c, d, e, x, s)        {\
      (a) += G((b), (c), (d)) + (x) + 0x7a6d76e9;\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }
#define HHH(a, b, c, d, e, x, s)        {\
      (a) += H((b), (c), (d)) + (x) + 0x6d703ef3;\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }
#define III(a, b, c, d, e, x, s)        {\
      (a) += I((b), (c), (d)) + (x) + 0x5c4dd124;\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }
#define JJJ(a, b, c, d, e, x, s)        {\
      (a) += J((b), (c), (d)) + (x) + 0x50a28be6;\
      (a) = ROL((a), (s)) + (e);\
      (c) = ROL((c), 10);\
   }

#ifndef ASM_RMD160PROCESS
void rmd160Process(rmd160Param* mp)
{
	register uint32_t aa,bb,cc,dd,ee;
	register uint32_t aaa,bbb,ccc,ddd,eee;
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

	aa = mp->h[0]; bb = mp->h[1]; cc = mp->h[2]; dd = mp->h[3]; ee = mp->h[4];
	aaa = mp->h[0]; bbb = mp->h[1]; ccc = mp->h[2]; ddd = mp->h[3]; eee = mp->h[4];

	/* round 1 */
	FF(aa, bb, cc, dd, ee, X[ 0], 11);
	FF(ee, aa, bb, cc, dd, X[ 1], 14);
	FF(dd, ee, aa, bb, cc, X[ 2], 15);
	FF(cc, dd, ee, aa, bb, X[ 3], 12);
	FF(bb, cc, dd, ee, aa, X[ 4],  5);
	FF(aa, bb, cc, dd, ee, X[ 5],  8);
	FF(ee, aa, bb, cc, dd, X[ 6],  7);
	FF(dd, ee, aa, bb, cc, X[ 7],  9);
	FF(cc, dd, ee, aa, bb, X[ 8], 11);
	FF(bb, cc, dd, ee, aa, X[ 9], 13);
	FF(aa, bb, cc, dd, ee, X[10], 14);
	FF(ee, aa, bb, cc, dd, X[11], 15);
	FF(dd, ee, aa, bb, cc, X[12],  6);
	FF(cc, dd, ee, aa, bb, X[13],  7);
	FF(bb, cc, dd, ee, aa, X[14],  9);
	FF(aa, bb, cc, dd, ee, X[15],  8);
	                          
	/* round 2 */
	GG(ee, aa, bb, cc, dd, X[ 7],  7);
	GG(dd, ee, aa, bb, cc, X[ 4],  6);
	GG(cc, dd, ee, aa, bb, X[13],  8);
	GG(bb, cc, dd, ee, aa, X[ 1], 13);
	GG(aa, bb, cc, dd, ee, X[10], 11);
	GG(ee, aa, bb, cc, dd, X[ 6],  9);
	GG(dd, ee, aa, bb, cc, X[15],  7);
	GG(cc, dd, ee, aa, bb, X[ 3], 15);
	GG(bb, cc, dd, ee, aa, X[12],  7);
	GG(aa, bb, cc, dd, ee, X[ 0], 12);
	GG(ee, aa, bb, cc, dd, X[ 9], 15);
	GG(dd, ee, aa, bb, cc, X[ 5],  9);
	GG(cc, dd, ee, aa, bb, X[ 2], 11);
	GG(bb, cc, dd, ee, aa, X[14],  7);
	GG(aa, bb, cc, dd, ee, X[11], 13);
	GG(ee, aa, bb, cc, dd, X[ 8], 12);

	/* round 3 */
	HH(dd, ee, aa, bb, cc, X[ 3], 11);
	HH(cc, dd, ee, aa, bb, X[10], 13);
	HH(bb, cc, dd, ee, aa, X[14],  6);
	HH(aa, bb, cc, dd, ee, X[ 4],  7);
	HH(ee, aa, bb, cc, dd, X[ 9], 14);
	HH(dd, ee, aa, bb, cc, X[15],  9);
	HH(cc, dd, ee, aa, bb, X[ 8], 13);
	HH(bb, cc, dd, ee, aa, X[ 1], 15);
	HH(aa, bb, cc, dd, ee, X[ 2], 14);
	HH(ee, aa, bb, cc, dd, X[ 7],  8);
	HH(dd, ee, aa, bb, cc, X[ 0], 13);
	HH(cc, dd, ee, aa, bb, X[ 6],  6);
	HH(bb, cc, dd, ee, aa, X[13],  5);
	HH(aa, bb, cc, dd, ee, X[11], 12);
	HH(ee, aa, bb, cc, dd, X[ 5],  7);
	HH(dd, ee, aa, bb, cc, X[12],  5);

	/* round 4 */
	II(cc, dd, ee, aa, bb, X[ 1], 11);
	II(bb, cc, dd, ee, aa, X[ 9], 12);
	II(aa, bb, cc, dd, ee, X[11], 14);
	II(ee, aa, bb, cc, dd, X[10], 15);
	II(dd, ee, aa, bb, cc, X[ 0], 14);
	II(cc, dd, ee, aa, bb, X[ 8], 15);
	II(bb, cc, dd, ee, aa, X[12],  9);
	II(aa, bb, cc, dd, ee, X[ 4],  8);
	II(ee, aa, bb, cc, dd, X[13],  9);
	II(dd, ee, aa, bb, cc, X[ 3], 14);
	II(cc, dd, ee, aa, bb, X[ 7],  5);
	II(bb, cc, dd, ee, aa, X[15],  6);
	II(aa, bb, cc, dd, ee, X[14],  8);
	II(ee, aa, bb, cc, dd, X[ 5],  6);
	II(dd, ee, aa, bb, cc, X[ 6],  5);
	II(cc, dd, ee, aa, bb, X[ 2], 12);

	/* round 5 */
	JJ(bb, cc, dd, ee, aa, X[ 4],  9);
	JJ(aa, bb, cc, dd, ee, X[ 0], 15);
	JJ(ee, aa, bb, cc, dd, X[ 5],  5);
	JJ(dd, ee, aa, bb, cc, X[ 9], 11);
	JJ(cc, dd, ee, aa, bb, X[ 7],  6);
	JJ(bb, cc, dd, ee, aa, X[12],  8);
	JJ(aa, bb, cc, dd, ee, X[ 2], 13);
	JJ(ee, aa, bb, cc, dd, X[10], 12);
	JJ(dd, ee, aa, bb, cc, X[14],  5);
	JJ(cc, dd, ee, aa, bb, X[ 1], 12);
	JJ(bb, cc, dd, ee, aa, X[ 3], 13);
	JJ(aa, bb, cc, dd, ee, X[ 8], 14);
	JJ(ee, aa, bb, cc, dd, X[11], 11);
	JJ(dd, ee, aa, bb, cc, X[ 6],  8);
	JJ(cc, dd, ee, aa, bb, X[15],  5);
	JJ(bb, cc, dd, ee, aa, X[13],  6);

	/* parallel round 1 */
	JJJ(aaa, bbb, ccc, ddd, eee, X[ 5],  8);
	JJJ(eee, aaa, bbb, ccc, ddd, X[14],  9);
	JJJ(ddd, eee, aaa, bbb, ccc, X[ 7],  9);
	JJJ(ccc, ddd, eee, aaa, bbb, X[ 0], 11);
	JJJ(bbb, ccc, ddd, eee, aaa, X[ 9], 13);
	JJJ(aaa, bbb, ccc, ddd, eee, X[ 2], 15);
	JJJ(eee, aaa, bbb, ccc, ddd, X[11], 15);
	JJJ(ddd, eee, aaa, bbb, ccc, X[ 4],  5);
	JJJ(ccc, ddd, eee, aaa, bbb, X[13],  7);
	JJJ(bbb, ccc, ddd, eee, aaa, X[ 6],  7);
	JJJ(aaa, bbb, ccc, ddd, eee, X[15],  8);
	JJJ(eee, aaa, bbb, ccc, ddd, X[ 8], 11);
	JJJ(ddd, eee, aaa, bbb, ccc, X[ 1], 14);
	JJJ(ccc, ddd, eee, aaa, bbb, X[10], 14);
	JJJ(bbb, ccc, ddd, eee, aaa, X[ 3], 12);
	JJJ(aaa, bbb, ccc, ddd, eee, X[12],  6);

	/* parallel round 2 */
	III(eee, aaa, bbb, ccc, ddd, X[ 6],  9); 
	III(ddd, eee, aaa, bbb, ccc, X[11], 13);
	III(ccc, ddd, eee, aaa, bbb, X[ 3], 15);
	III(bbb, ccc, ddd, eee, aaa, X[ 7],  7);
	III(aaa, bbb, ccc, ddd, eee, X[ 0], 12);
	III(eee, aaa, bbb, ccc, ddd, X[13],  8);
	III(ddd, eee, aaa, bbb, ccc, X[ 5],  9);
	III(ccc, ddd, eee, aaa, bbb, X[10], 11);
	III(bbb, ccc, ddd, eee, aaa, X[14],  7);
	III(aaa, bbb, ccc, ddd, eee, X[15],  7);
	III(eee, aaa, bbb, ccc, ddd, X[ 8], 12);
	III(ddd, eee, aaa, bbb, ccc, X[12],  7);
	III(ccc, ddd, eee, aaa, bbb, X[ 4],  6);
	III(bbb, ccc, ddd, eee, aaa, X[ 9], 15);
	III(aaa, bbb, ccc, ddd, eee, X[ 1], 13);
	III(eee, aaa, bbb, ccc, ddd, X[ 2], 11);

	/* parallel round 3 */
	HHH(ddd, eee, aaa, bbb, ccc, X[15],  9);
	HHH(ccc, ddd, eee, aaa, bbb, X[ 5],  7);
	HHH(bbb, ccc, ddd, eee, aaa, X[ 1], 15);
	HHH(aaa, bbb, ccc, ddd, eee, X[ 3], 11);
	HHH(eee, aaa, bbb, ccc, ddd, X[ 7],  8);
	HHH(ddd, eee, aaa, bbb, ccc, X[14],  6);
	HHH(ccc, ddd, eee, aaa, bbb, X[ 6],  6);
	HHH(bbb, ccc, ddd, eee, aaa, X[ 9], 14);
	HHH(aaa, bbb, ccc, ddd, eee, X[11], 12);
	HHH(eee, aaa, bbb, ccc, ddd, X[ 8], 13);
	HHH(ddd, eee, aaa, bbb, ccc, X[12],  5);
	HHH(ccc, ddd, eee, aaa, bbb, X[ 2], 14);
	HHH(bbb, ccc, ddd, eee, aaa, X[10], 13);
	HHH(aaa, bbb, ccc, ddd, eee, X[ 0], 13);
	HHH(eee, aaa, bbb, ccc, ddd, X[ 4],  7);
	HHH(ddd, eee, aaa, bbb, ccc, X[13],  5);

	/* parallel round 4 */   
	GGG(ccc, ddd, eee, aaa, bbb, X[ 8], 15);
	GGG(bbb, ccc, ddd, eee, aaa, X[ 6],  5);
	GGG(aaa, bbb, ccc, ddd, eee, X[ 4],  8);
	GGG(eee, aaa, bbb, ccc, ddd, X[ 1], 11);
	GGG(ddd, eee, aaa, bbb, ccc, X[ 3], 14);
	GGG(ccc, ddd, eee, aaa, bbb, X[11], 14);
	GGG(bbb, ccc, ddd, eee, aaa, X[15],  6);
	GGG(aaa, bbb, ccc, ddd, eee, X[ 0], 14);
	GGG(eee, aaa, bbb, ccc, ddd, X[ 5],  6);
	GGG(ddd, eee, aaa, bbb, ccc, X[12],  9);
	GGG(ccc, ddd, eee, aaa, bbb, X[ 2], 12);
	GGG(bbb, ccc, ddd, eee, aaa, X[13],  9);
	GGG(aaa, bbb, ccc, ddd, eee, X[ 9], 12);
	GGG(eee, aaa, bbb, ccc, ddd, X[ 7],  5);
	GGG(ddd, eee, aaa, bbb, ccc, X[10], 15);
	GGG(ccc, ddd, eee, aaa, bbb, X[14],  8);

	/* parallel round 5 */
	FFF(bbb, ccc, ddd, eee, aaa, X[12] ,  8);
	FFF(aaa, bbb, ccc, ddd, eee, X[15] ,  5);
	FFF(eee, aaa, bbb, ccc, ddd, X[10] , 12);
	FFF(ddd, eee, aaa, bbb, ccc, X[ 4] ,  9);
	FFF(ccc, ddd, eee, aaa, bbb, X[ 1] , 12);
	FFF(bbb, ccc, ddd, eee, aaa, X[ 5] ,  5);
	FFF(aaa, bbb, ccc, ddd, eee, X[ 8] , 14);
	FFF(eee, aaa, bbb, ccc, ddd, X[ 7] ,  6);
	FFF(ddd, eee, aaa, bbb, ccc, X[ 6] ,  8);
	FFF(ccc, ddd, eee, aaa, bbb, X[ 2] , 13);
	FFF(bbb, ccc, ddd, eee, aaa, X[13] ,  6);
	FFF(aaa, bbb, ccc, ddd, eee, X[14] ,  5);
	FFF(eee, aaa, bbb, ccc, ddd, X[ 0] , 15);
	FFF(ddd, eee, aaa, bbb, ccc, X[ 3] , 13);
	FFF(ccc, ddd, eee, aaa, bbb, X[ 9] , 11);
	FFF(bbb, ccc, ddd, eee, aaa, X[11] , 11);

	/* combine results */
	ddd += cc + mp->h[1];               /* final result for MDbuf[0] */
	mp->h[1] = mp->h[2] + dd + eee;
	mp->h[2] = mp->h[3] + ee + aaa;
	mp->h[3] = mp->h[4] + aa + bbb;
	mp->h[4] = mp->h[0] + bb + ccc;
	mp->h[0] = ddd;
}
#endif

int rmd160Update(rmd160Param* mp, const byte* data, size_t size)
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
			rmd160Process(mp);
			mp->offset = 0;
		}
	}
	return 0;
}

static void rmd160Finish(rmd160Param* mp)
	/*@modifies mp @*/
{
	register byte *ptr = ((byte *) mp->data) + mp->offset++;

	*(ptr++) = 0x80;

	if (mp->offset > 56)
	{
		while (mp->offset++ < 64)
			*(ptr++) = 0;

		rmd160Process(mp);
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

	rmd160Process(mp);

	mp->offset = 0;
}

/*@-protoparammatch@*/
int rmd160Digest(rmd160Param* mp, byte* data)
{
	rmd160Finish(mp);

	/* encode 5 integers little-endian style */
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
	data[16] = (byte)(mp->h[4]      );
	data[17] = (byte)(mp->h[4] >>  8);
	data[18] = (byte)(mp->h[4] >> 16);
	data[19] = (byte)(mp->h[4] >> 24);

	(void) rmd160Reset(mp);
	return 0;
}
/*@=protoparammatch@*/

/*!\}
 */

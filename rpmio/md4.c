/* LibTomCrypt, modular cryptographic library -- Tom St Denis
 *
 * LibTomCrypt is a library that provides various cryptographic
 * algorithms in a highly modular and flexible manner.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tomstdenis@gmail.com, http://libtomcrypt.com
 */
#include "system.h"

#if defined(HAVE_STDINT_H)
#include <stdint.h>
#endif

#include "md4.h"
#include <beecrypt/mp.h>
#include <beecrypt/endianness.h>

#include "debug.h"

/*@unchecked@*/ /*@observer@*/
static uint32_t md4hinit[4] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };       

/*@-sizeoftype@*/
/*@unchecked@*/ /*@observer@*/
const hashFunction md4 = {
	"MD4",
	sizeof(md4Param),
	64,
	128/8,
	(hashFunctionReset) md4Reset,
	(hashFunctionUpdate) md4Update,
	(hashFunctionDigest) md4Digest,
};
/*@=sizeoftype@*/

int md4Reset(md4Param* mp)
{
/*@-sizeoftype@*/
	memcpy(mp->h, md4hinit, 4 * sizeof(uint32_t));
	memset(mp->data, 0, 16 * sizeof(uint32_t));
/*@=sizeoftype@*/
	#if (MP_WBITS == 64)
	mpzero(1, mp->length);
	#elif (MP_WBITS == 32)
	mpzero(2, mp->length);
	#else   
	# error 
	#endif          
	mp->offset  = 0;
	return 0;
}

#define F(x, y, z) (z ^ (x & (y ^ z)))
#define G(x, y, z) ((x & y) | (z & (x | y)))
#define H(x, y, z) ((x) ^ (y) ^ (z))

#define FF(a, b, c, d, x, s) { \
    (a) += F ((b), (c), (d)) + (x); \
    (a) = ROTL32 ((a), (s)); \
  }
#define GG(a, b, c, d, x, s) { \
    (a) += G ((b), (c), (d)) + (x) + 0x5a827999UL; \
    (a) = ROTL32 ((a), (s)); \
  }
#define HH(a, b, c, d, x, s) { \
    (a) += H ((b), (c), (d)) + (x) + 0x6ed9eba1UL; \
    (a) = ROTL32 ((a), (s)); \
  }

#ifndef ASM_MD4PROCESS
void md4Process(md4Param *mp)
	/*@modifies mp @*/
{
	uint32_t a, b, c, d;

	register uint32_t* w;
	#if WORDS_BIGENDIAN
	register byte t;
	#endif

	w = mp->data;
	#if WORDS_BIGENDIAN
	t = 16;
	while (t--)
	{
		register uint32_t temp = swapu32(*w);
		*(w++) = temp;
	}
	w = mp->data;
	#endif

	a = mp->h[0]; b = mp->h[1]; c = mp->h[2]; d = mp->h[3];

	FF (a, b, c, d, w[ 0],  3);
	FF (d, a, b, c, w[ 1],  7);
	FF (c, d, a, b, w[ 2], 11);
	FF (b, c, d, a, w[ 3], 19);
	FF (a, b, c, d, w[ 4],  3);
	FF (d, a, b, c, w[ 5],  7);
	FF (c, d, a, b, w[ 6], 11);
	FF (b, c, d, a, w[ 7], 19);
	FF (a, b, c, d, w[ 8],  3);
	FF (d, a, b, c, w[ 9],  7);
	FF (c, d, a, b, w[10], 11);
	FF (b, c, d, a, w[11], 19);
	FF (a, b, c, d, w[12],  3);
	FF (d, a, b, c, w[13],  7);
	FF (c, d, a, b, w[14], 11);
	FF (b, c, d, a, w[15], 19);
    
	GG (a, b, c, d, w[ 0],  3);
	GG (d, a, b, c, w[ 4],  5);
	GG (c, d, a, b, w[ 8],  9);
	GG (b, c, d, a, w[12], 13);
	GG (a, b, c, d, w[ 1],  3);
	GG (d, a, b, c, w[ 5],  5);
	GG (c, d, a, b, w[ 9],  9);
	GG (b, c, d, a, w[13], 13);
	GG (a, b, c, d, w[ 2],  3);
	GG (d, a, b, c, w[ 6],  5);
	GG (c, d, a, b, w[10],  9);
	GG (b, c, d, a, w[14], 13);
	GG (a, b, c, d, w[ 3],  3);
	GG (d, a, b, c, w[ 7],  5);
	GG (c, d, a, b, w[11],  9);
	GG (b, c, d, a, w[15], 13);
    
	HH (a, b, c, d, w[ 0],  3);
	HH (d, a, b, c, w[ 8],  9);
	HH (c, d, a, b, w[ 4], 11);
	HH (b, c, d, a, w[12], 15);
	HH (a, b, c, d, w[ 2],  3);
	HH (d, a, b, c, w[10],  9);
	HH (c, d, a, b, w[ 6], 11);
	HH (b, c, d, a, w[14], 15);
	HH (a, b, c, d, w[ 1],  3);
	HH (d, a, b, c, w[ 9],  9);
	HH (c, d, a, b, w[ 5], 11);
	HH (b, c, d, a, w[13], 15);
	HH (a, b, c, d, w[ 3],  3);
	HH (d, a, b, c, w[11],  9);
	HH (c, d, a, b, w[ 7], 11);
	HH (b, c, d, a, w[15], 15);

	mp->h[0] +=  a;
	mp->h[1] +=  b;
	mp->h[2] +=  c;
	mp->h[3] +=  d;
}
#endif

int md4Update(md4Param* mp, const byte* data, size_t size)
{
	register uint32_t proclength;

	#if (MP_WBITS == 64)
	mpw add[1];
	mpsetw(1, add, size);
	mplshift(1, add, 3);
	(void) mpadd(1, mp->length, add);
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
			md4Process(mp);
			mp->offset = 0;
		}
	}
	return 0;
}

static void md4Finish(md4Param* mp)
	/*@modifies mp @*/
{
	register byte *ptr = ((byte *) mp->data) + mp->offset++;

	*(ptr++) = 0x80;

	if (mp->offset > 56)
	{
		while (mp->offset++ < 64)
			*(ptr++) = 0;

		md4Process(mp);
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

	md4Process(mp);

	mp->offset = 0;
}

/*@-protoparammatch@*/
int md4Digest(md4Param* mp, byte* data)
{
	md4Finish(mp);

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

	(void) md4Reset(mp);
	return 0;
}
/*@=protoparammatch@*/

#if 0
/**
  Self-test the hash
  @return 0 if successful, CRYPT_NOP if self-tests have been disabled
*/  
int md4_test(void)
{
    static const struct md4_test_case {
        char *input;
        byte digest[16];
    } cases[] = {
        { "", 
          {0x31, 0xd6, 0xcf, 0xe0, 0xd1, 0x6a, 0xe9, 0x31,
           0xb7, 0x3c, 0x59, 0xd7, 0xe0, 0xc0, 0x89, 0xc0} },
        { "a",
          {0xbd, 0xe5, 0x2c, 0xb3, 0x1d, 0xe3, 0x3e, 0x46,
           0x24, 0x5e, 0x05, 0xfb, 0xdb, 0xd6, 0xfb, 0x24} },
        { "abc",
          {0xa4, 0x48, 0x01, 0x7a, 0xaf, 0x21, 0xd8, 0x52, 
           0x5f, 0xc1, 0x0a, 0xe8, 0x7a, 0xa6, 0x72, 0x9d} },
        { "message digest", 
          {0xd9, 0x13, 0x0a, 0x81, 0x64, 0x54, 0x9f, 0xe8, 
           0x18, 0x87, 0x48, 0x06, 0xe1, 0xc7, 0x01, 0x4b} },
        { "abcdefghijklmnopqrstuvwxyz", 
          {0xd7, 0x9e, 0x1c, 0x30, 0x8a, 0xa5, 0xbb, 0xcd, 
           0xee, 0xa8, 0xed, 0x63, 0xdf, 0x41, 0x2d, 0xa9} },
        { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", 
          {0x04, 0x3f, 0x85, 0x82, 0xf2, 0x41, 0xdb, 0x35, 
           0x1c, 0xe6, 0x27, 0xe1, 0x53, 0xe7, 0xf0, 0xe4} },
        { "12345678901234567890123456789012345678901234567890123456789012345678901234567890", 
          {0xe3, 0x3b, 0x4d, 0xdc, 0x9c, 0x38, 0xf2, 0x19, 
           0x9c, 0x3e, 0x7b, 0x16, 0x4f, 0xcc, 0x05, 0x36} },
    };
    int i;
    md4Param md;
    byte digest[16];

    for(i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
        md4Reset(&md);
        md4Update(&md, (byte *)cases[i].input, strlen(cases[i].input));
        md4Digest(&md, digest);
        if (memcmp(digest, cases[i].digest, 16) != 0) {
           return 5;
        }

    }
    return 0;
}

int main() {
    int rc = md4_test();
    switch(rc) {
    case 0:	printf("OK\n");		break;
    default:	printf("FAIL\n");	break;
    }
    return rc;
}
#endif

/** \ingroup rpmio
 * \file rpmio/mongo.c
 */

/*    Copyright 2009, 2010 10gen Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "system.h"

#include <rpmiotypes.h>
#include <rpmio.h>	/* for *Pool methods */
#include <rpmlog.h>
#include <rpmurl.h>

#define	_RPMMGO_INTERNAL
#include <mongo.h>

#include "debug.h"

/*@unchecked@*/
int _rpmmgo_debug = 0;

/*@unchecked@*/ /*@relnull@*/
rpmmgo _rpmmgoI;

/*==============================================================*/

/* only need one of these */
static const int zero = 0;
static const int one = 1;

/*==============================================================*/
/* --- platform_hacks.h */
/*    Copyright 2009, 2010 10gen Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/* big endian is only used for OID generation. little is used everywhere else */
#ifdef MONGO_BIG_ENDIAN
#define bson_little_endian64(out, in) ( bson_swap_endian64(out, in) )
#define bson_little_endian32(out, in) ( bson_swap_endian32(out, in) )
#define bson_big_endian64(out, in) ( memcpy(out, in, 8) )
#define bson_big_endian32(out, in) ( memcpy(out, in, 4) )
#else
#define bson_little_endian64(out, in) ( memcpy(out, in, 8) )
#define bson_little_endian32(out, in) ( memcpy(out, in, 4) )
#define bson_big_endian64(out, in) ( bson_swap_endian64(out, in) )
#define bson_big_endian32(out, in) ( bson_swap_endian32(out, in) )
#endif

/*==============================================================*/
/*
  Copyright (C) 1999, 2002 Aladdin Enterprises.  All rights reserved.

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  L. Peter Deutsch
  ghost@aladdin.com

 */

/*
  Independent implementation of MD5 (RFC 1321).

  This code implements the MD5 Algorithm defined in RFC 1321, whose
  text is available at
	http://www.ietf.org/rfc/rfc1321.txt
  The code is derived from the text of the RFC, including the test suite
  (section A.5) but excluding the rest of Appendix A.  It does not include
  any code or documentation that is identified in the RFC as being
  copyrighted.

  The original and principal author of md5.h is L. Peter Deutsch
  <ghost@aladdin.com>.  Other authors are noted in the change history
  that follows (in reverse chronological order):

  2002-04-13 lpd Removed support for non-ANSI compilers; removed
	references to Ghostscript; clarified derivation from RFC 1321;
	now handles byte order either statically or dynamically.
  1999-11-04 lpd Edited comments slightly for automatic TOC extraction.
  1999-10-18 lpd Fixed typo in header comment (ansi2knr rather than md5);
	added conditionalization for C++ compilation from Martin
	Purschke <purschke@bnl.gov>.
  1999-05-03 lpd Original version.
 */

/*
 * This package supports both compile-time and run-time determination of CPU
 * byte order.  If ARCH_IS_BIG_ENDIAN is defined as 0, the code will be
 * compiled to run only on little-endian CPUs; if ARCH_IS_BIG_ENDIAN is
 * defined as non-zero, the code will be compiled to run only on big-endian
 * CPUs; if ARCH_IS_BIG_ENDIAN is not defined, the code will be compiled to
 * run on either big- or little-endian CPUs, but will run slightly less
 * efficiently on either one than if ARCH_IS_BIG_ENDIAN is defined.
 */

typedef unsigned char mongo_md5_byte_t; /* 8-bit byte */
typedef unsigned int mongo_md5_word_t; /* 32-bit word */

/* Define the state of the MD5 Algorithm. */
typedef struct mongo_md5_state_s {
    mongo_md5_word_t count[2];	/* message length in bits, lsw first */
    mongo_md5_word_t abcd[4];		/* digest buffer */
    mongo_md5_byte_t buf[64];		/* accumulate block */
} mongo_md5_state_t;

#ifdef __cplusplus
extern "C"
{
#endif

    /* Initialize the algorithm. */
    void mongo_md5_init(mongo_md5_state_t *pms);

    /* Append a string to the message. */
    void mongo_md5_append(mongo_md5_state_t *pms, const mongo_md5_byte_t *data, int nbytes);

    /* Finish the message and return the digest. */
    void mongo_md5_finish(mongo_md5_state_t *pms, mongo_md5_byte_t digest[16]);

#ifdef __cplusplus
}  /* end extern "C" */
#endif

/*==============================================================*/
/* --- md5.c */

/*
  Independent implementation of MD5 (RFC 1321).

  This code implements the MD5 Algorithm defined in RFC 1321, whose
  text is available at
	http://www.ietf.org/rfc/rfc1321.txt
  The code is derived from the text of the RFC, including the test suite
  (section A.5) but excluding the rest of Appendix A.  It does not include
  any code or documentation that is identified in the RFC as being
  copyrighted.

  The original and principal author of md5.c is L. Peter Deutsch
  <ghost@aladdin.com>.  Other authors are noted in the change history
  that follows (in reverse chronological order):

  2002-04-13 lpd Clarified derivation from RFC 1321; now handles byte order
	either statically or dynamically; added missing #include <string.h>
	in library.
  2002-03-11 lpd Corrected argument list for main(), and added int return
	type, in test program and T value program.
  2002-02-21 lpd Added missing #include <stdio.h> in test program.
  2000-07-03 lpd Patched to eliminate warnings about "constant is
	unsigned in ANSI C, signed in traditional"; made test program
	self-checking.
  1999-11-04 lpd Edited comments slightly for automatic TOC extraction.
  1999-10-18 lpd Fixed typo in header comment (ansi2knr rather than md5).
  1999-05-03 lpd Original version.
 */

#undef BYTE_ORDER	/* 1 = big-endian, -1 = little-endian, 0 = unknown */
#ifdef MONGO_BIG_ENDIAN
#  define BYTE_ORDER 1
#else
#  define BYTE_ORDER -1
#endif

#define T_MASK ((mongo_md5_word_t)~0)
#define T1 /* 0xd76aa478 */ (T_MASK ^ 0x28955b87)
#define T2 /* 0xe8c7b756 */ (T_MASK ^ 0x173848a9)
#define T3    0x242070db
#define T4 /* 0xc1bdceee */ (T_MASK ^ 0x3e423111)
#define T5 /* 0xf57c0faf */ (T_MASK ^ 0x0a83f050)
#define T6    0x4787c62a
#define T7 /* 0xa8304613 */ (T_MASK ^ 0x57cfb9ec)
#define T8 /* 0xfd469501 */ (T_MASK ^ 0x02b96afe)
#define T9    0x698098d8
#define T10 /* 0x8b44f7af */ (T_MASK ^ 0x74bb0850)
#define T11 /* 0xffff5bb1 */ (T_MASK ^ 0x0000a44e)
#define T12 /* 0x895cd7be */ (T_MASK ^ 0x76a32841)
#define T13    0x6b901122
#define T14 /* 0xfd987193 */ (T_MASK ^ 0x02678e6c)
#define T15 /* 0xa679438e */ (T_MASK ^ 0x5986bc71)
#define T16    0x49b40821
#define T17 /* 0xf61e2562 */ (T_MASK ^ 0x09e1da9d)
#define T18 /* 0xc040b340 */ (T_MASK ^ 0x3fbf4cbf)
#define T19    0x265e5a51
#define T20 /* 0xe9b6c7aa */ (T_MASK ^ 0x16493855)
#define T21 /* 0xd62f105d */ (T_MASK ^ 0x29d0efa2)
#define T22    0x02441453
#define T23 /* 0xd8a1e681 */ (T_MASK ^ 0x275e197e)
#define T24 /* 0xe7d3fbc8 */ (T_MASK ^ 0x182c0437)
#define T25    0x21e1cde6
#define T26 /* 0xc33707d6 */ (T_MASK ^ 0x3cc8f829)
#define T27 /* 0xf4d50d87 */ (T_MASK ^ 0x0b2af278)
#define T28    0x455a14ed
#define T29 /* 0xa9e3e905 */ (T_MASK ^ 0x561c16fa)
#define T30 /* 0xfcefa3f8 */ (T_MASK ^ 0x03105c07)
#define T31    0x676f02d9
#define T32 /* 0x8d2a4c8a */ (T_MASK ^ 0x72d5b375)
#define T33 /* 0xfffa3942 */ (T_MASK ^ 0x0005c6bd)
#define T34 /* 0x8771f681 */ (T_MASK ^ 0x788e097e)
#define T35    0x6d9d6122
#define T36 /* 0xfde5380c */ (T_MASK ^ 0x021ac7f3)
#define T37 /* 0xa4beea44 */ (T_MASK ^ 0x5b4115bb)
#define T38    0x4bdecfa9
#define T39 /* 0xf6bb4b60 */ (T_MASK ^ 0x0944b49f)
#define T40 /* 0xbebfbc70 */ (T_MASK ^ 0x4140438f)
#define T41    0x289b7ec6
#define T42 /* 0xeaa127fa */ (T_MASK ^ 0x155ed805)
#define T43 /* 0xd4ef3085 */ (T_MASK ^ 0x2b10cf7a)
#define T44    0x04881d05
#define T45 /* 0xd9d4d039 */ (T_MASK ^ 0x262b2fc6)
#define T46 /* 0xe6db99e5 */ (T_MASK ^ 0x1924661a)
#define T47    0x1fa27cf8
#define T48 /* 0xc4ac5665 */ (T_MASK ^ 0x3b53a99a)
#define T49 /* 0xf4292244 */ (T_MASK ^ 0x0bd6ddbb)
#define T50    0x432aff97
#define T51 /* 0xab9423a7 */ (T_MASK ^ 0x546bdc58)
#define T52 /* 0xfc93a039 */ (T_MASK ^ 0x036c5fc6)
#define T53    0x655b59c3
#define T54 /* 0x8f0ccc92 */ (T_MASK ^ 0x70f3336d)
#define T55 /* 0xffeff47d */ (T_MASK ^ 0x00100b82)
#define T56 /* 0x85845dd1 */ (T_MASK ^ 0x7a7ba22e)
#define T57    0x6fa87e4f
#define T58 /* 0xfe2ce6e0 */ (T_MASK ^ 0x01d3191f)
#define T59 /* 0xa3014314 */ (T_MASK ^ 0x5cfebceb)
#define T60    0x4e0811a1
#define T61 /* 0xf7537e82 */ (T_MASK ^ 0x08ac817d)
#define T62 /* 0xbd3af235 */ (T_MASK ^ 0x42c50dca)
#define T63    0x2ad7d2bb
#define T64 /* 0xeb86d391 */ (T_MASK ^ 0x14792c6e)

static void
mongo_md5_process(mongo_md5_state_t *pms, const mongo_md5_byte_t *data /*[64]*/)
{
    mongo_md5_word_t
	a = pms->abcd[0], b = pms->abcd[1],
	c = pms->abcd[2], d = pms->abcd[3];
    mongo_md5_word_t t;
#if BYTE_ORDER > 0
    /* Define storage only for big-endian CPUs. */
    mongo_md5_word_t X[16];
#else
    /* Define storage for little-endian or both types of CPUs. */
    mongo_md5_word_t xbuf[16];
    const mongo_md5_word_t *X;
#endif

    {
#if BYTE_ORDER == 0
	/*
	 * Determine dynamically whether this is a big-endian or
	 * little-endian machine, since we can use a more efficient
	 * algorithm on the latter.
	 */
	static const int w = 1;

	if (*((const mongo_md5_byte_t *)&w)) /* dynamic little-endian */
#endif
#if BYTE_ORDER <= 0		/* little-endian */
	{
	    /*
	     * On little-endian machines, we can process properly aligned
	     * data without copying it.
	     */
	    if (!((data - (const mongo_md5_byte_t *)0) & 3)) {
		/* data are properly aligned */
		X = (const mongo_md5_word_t *)data;
	    } else {
		/* not aligned */
		memcpy(xbuf, data, 64);
		X = xbuf;
	    }
	}
#endif
#if BYTE_ORDER == 0
	else			/* dynamic big-endian */
#endif
#if BYTE_ORDER >= 0		/* big-endian */
	{
	    /*
	     * On big-endian machines, we must arrange the bytes in the
	     * right order.
	     */
	    const mongo_md5_byte_t *xp = data;
	    int i;

#  if BYTE_ORDER == 0
	    X = xbuf;		/* (dynamic only) */
#  else
#    define xbuf X		/* (static only) */
#  endif
	    for (i = 0; i < 16; ++i, xp += 4)
		xbuf[i] = xp[0] + (xp[1] << 8) + (xp[2] << 16) + (xp[3] << 24);
	}
#endif
    }

#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

    /* Round 1. */
    /* Let [abcd k s i] denote the operation
       a = b + ((a + F(b,c,d) + X[k] + T[i]) <<< s). */
#define F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define SET(a, b, c, d, k, s, Ti)\
  t = a + F(b,c,d) + X[k] + Ti;\
  a = ROTATE_LEFT(t, s) + b
    /* Do the following 16 operations. */
    SET(a, b, c, d,  0,  7,  T1);
    SET(d, a, b, c,  1, 12,  T2);
    SET(c, d, a, b,  2, 17,  T3);
    SET(b, c, d, a,  3, 22,  T4);
    SET(a, b, c, d,  4,  7,  T5);
    SET(d, a, b, c,  5, 12,  T6);
    SET(c, d, a, b,  6, 17,  T7);
    SET(b, c, d, a,  7, 22,  T8);
    SET(a, b, c, d,  8,  7,  T9);
    SET(d, a, b, c,  9, 12, T10);
    SET(c, d, a, b, 10, 17, T11);
    SET(b, c, d, a, 11, 22, T12);
    SET(a, b, c, d, 12,  7, T13);
    SET(d, a, b, c, 13, 12, T14);
    SET(c, d, a, b, 14, 17, T15);
    SET(b, c, d, a, 15, 22, T16);
#undef SET

     /* Round 2. */
     /* Let [abcd k s i] denote the operation
          a = b + ((a + G(b,c,d) + X[k] + T[i]) <<< s). */
#define G(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define SET(a, b, c, d, k, s, Ti)\
  t = a + G(b,c,d) + X[k] + Ti;\
  a = ROTATE_LEFT(t, s) + b
     /* Do the following 16 operations. */
    SET(a, b, c, d,  1,  5, T17);
    SET(d, a, b, c,  6,  9, T18);
    SET(c, d, a, b, 11, 14, T19);
    SET(b, c, d, a,  0, 20, T20);
    SET(a, b, c, d,  5,  5, T21);
    SET(d, a, b, c, 10,  9, T22);
    SET(c, d, a, b, 15, 14, T23);
    SET(b, c, d, a,  4, 20, T24);
    SET(a, b, c, d,  9,  5, T25);
    SET(d, a, b, c, 14,  9, T26);
    SET(c, d, a, b,  3, 14, T27);
    SET(b, c, d, a,  8, 20, T28);
    SET(a, b, c, d, 13,  5, T29);
    SET(d, a, b, c,  2,  9, T30);
    SET(c, d, a, b,  7, 14, T31);
    SET(b, c, d, a, 12, 20, T32);
#undef SET

     /* Round 3. */
     /* Let [abcd k s t] denote the operation
          a = b + ((a + H(b,c,d) + X[k] + T[i]) <<< s). */
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define SET(a, b, c, d, k, s, Ti)\
  t = a + H(b,c,d) + X[k] + Ti;\
  a = ROTATE_LEFT(t, s) + b
     /* Do the following 16 operations. */
    SET(a, b, c, d,  5,  4, T33);
    SET(d, a, b, c,  8, 11, T34);
    SET(c, d, a, b, 11, 16, T35);
    SET(b, c, d, a, 14, 23, T36);
    SET(a, b, c, d,  1,  4, T37);
    SET(d, a, b, c,  4, 11, T38);
    SET(c, d, a, b,  7, 16, T39);
    SET(b, c, d, a, 10, 23, T40);
    SET(a, b, c, d, 13,  4, T41);
    SET(d, a, b, c,  0, 11, T42);
    SET(c, d, a, b,  3, 16, T43);
    SET(b, c, d, a,  6, 23, T44);
    SET(a, b, c, d,  9,  4, T45);
    SET(d, a, b, c, 12, 11, T46);
    SET(c, d, a, b, 15, 16, T47);
    SET(b, c, d, a,  2, 23, T48);
#undef SET

     /* Round 4. */
     /* Let [abcd k s t] denote the operation
          a = b + ((a + I(b,c,d) + X[k] + T[i]) <<< s). */
#define I(x, y, z) ((y) ^ ((x) | ~(z)))
#define SET(a, b, c, d, k, s, Ti)\
  t = a + I(b,c,d) + X[k] + Ti;\
  a = ROTATE_LEFT(t, s) + b
     /* Do the following 16 operations. */
    SET(a, b, c, d,  0,  6, T49);
    SET(d, a, b, c,  7, 10, T50);
    SET(c, d, a, b, 14, 15, T51);
    SET(b, c, d, a,  5, 21, T52);
    SET(a, b, c, d, 12,  6, T53);
    SET(d, a, b, c,  3, 10, T54);
    SET(c, d, a, b, 10, 15, T55);
    SET(b, c, d, a,  1, 21, T56);
    SET(a, b, c, d,  8,  6, T57);
    SET(d, a, b, c, 15, 10, T58);
    SET(c, d, a, b,  6, 15, T59);
    SET(b, c, d, a, 13, 21, T60);
    SET(a, b, c, d,  4,  6, T61);
    SET(d, a, b, c, 11, 10, T62);
    SET(c, d, a, b,  2, 15, T63);
    SET(b, c, d, a,  9, 21, T64);
#undef SET

     /* Then perform the following additions. (That is increment each
        of the four registers by the value it had before this block
        was started.) */
    pms->abcd[0] += a;
    pms->abcd[1] += b;
    pms->abcd[2] += c;
    pms->abcd[3] += d;
}

void
mongo_md5_init(mongo_md5_state_t *pms)
{
    pms->count[0] = pms->count[1] = 0;
    pms->abcd[0] = 0x67452301;
    pms->abcd[1] = /*0xefcdab89*/ T_MASK ^ 0x10325476;
    pms->abcd[2] = /*0x98badcfe*/ T_MASK ^ 0x67452301;
    pms->abcd[3] = 0x10325476;
}

void
mongo_md5_append(mongo_md5_state_t *pms, const mongo_md5_byte_t *data, int nbytes)
{
    const mongo_md5_byte_t *p = data;
    int left = nbytes;
    int offset = (pms->count[0] >> 3) & 63;
    mongo_md5_word_t nbits = (mongo_md5_word_t)(nbytes << 3);

    if (nbytes <= 0)
	return;

    /* Update the message length. */
    pms->count[1] += nbytes >> 29;
    pms->count[0] += nbits;
    if (pms->count[0] < nbits)
	pms->count[1]++;

    /* Process an initial partial block. */
    if (offset) {
	int copy = (offset + nbytes > 64 ? 64 - offset : nbytes);

	memcpy(pms->buf + offset, p, copy);
	if (offset + copy < 64)
	    return;
	p += copy;
	left -= copy;
	mongo_md5_process(pms, pms->buf);
    }

    /* Process full blocks. */
    for (; left >= 64; p += 64, left -= 64)
	mongo_md5_process(pms, p);

    /* Process a final partial block. */
    if (left)
	memcpy(pms->buf, p, left);
}

void
mongo_md5_finish(mongo_md5_state_t *pms, mongo_md5_byte_t digest[16])
{
    static const mongo_md5_byte_t pad[64] = {
	0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    mongo_md5_byte_t data[8];
    int i;

    /* Save the length before padding. */
    for (i = 0; i < 8; ++i)
	data[i] = (mongo_md5_byte_t)(pms->count[i >> 2] >> ((i & 3) << 3));
    /* Pad to 56 bytes mod 64. */
    mongo_md5_append(pms, pad, ((55 - (pms->count[0] >> 3)) & 63) + 1);
    /* Append the length. */
    mongo_md5_append(pms, data, 8);
    for (i = 0; i < 16; ++i)
	digest[i] = (mongo_md5_byte_t)(pms->abcd[i >> 2] >> ((i & 3) << 3));
}

/*==============================================================*/
/* --- mongo_except.h */

/* This file is based loosely on cexcept (http://www.nicemice.net/cexcept/). I
 * have modified it to work better with mongo's API.
 *
 * The MONGO_TRY, MONGO_CATCH, and MONGO_TROW macros assume that a pointer to
 * the current connection is available as 'conn'. If you would like to use a
 * different name, use the _GENERIC version of these macros.
 *
 * WARNING: do not return or otherwise jump (excluding MONGO_TRHOW()) out of a
 * MONGO_TRY block as the nessesary clean-up code will not be called. Jumping
 * out of the MONGO_CATCH block is OK.
 */

#ifdef MONGO_CODE_EXAMPLE
    mongo_connection conn[1]; /* makes conn a ptr to the connection */

    MONGO_TRY{
        mongo_find_one(...);
        MONGO_THROW(conn, MONGO_EXCEPT_NETWORK);
    }MONGO_CATCH{
        switch(conn->exception->type){
            case MONGO_EXCEPT_NETWORK:
                do_something();
            case MONGO_EXCEPT_FIND_ERR:
                do_something();
            default:
                MONGO_RETHROW();
        }
    }
#endif

 /* ORIGINAL CEXEPT COPYRIGHT:
cexcept: README 2.0.1 (2008-Jul-23-Wed)
http://www.nicemice.net/cexcept/
Adam M. Costello
http://www.nicemice.net/amc/

The package is both free-as-in-speech and free-as-in-beer:

    Copyright (c) 2000-2008 Adam M. Costello and Cosmin Truta.
    This package may be modified only if its author and version
    information is updated accurately, and may be redistributed
    only if accompanied by this unaltered notice.  Subject to those
    restrictions, permission is granted to anyone to do anything with
    this package.  The copyright holders make no guarantees regarding
    this package, and are not responsible for any damage resulting from
    its use.
 */

#define MONGO_TRY MONGO_TRY_GENERIC(conn)
#define MONGO_CATCH MONGO_CATCH_GENERIC(conn)
#define MONGO_THROW(e) MONGO_THROW_GENERIC(conn, e)
#define MONGO_RETHROW() MONGO_RETHROW_GENERIC(conn)

/* the rest of this file is implementation details */

/* this is done in mongo_connect */
#define MONGO_INIT_EXCEPTION(exception_ptr) \
    do{ \
        mongo_exception_type t; /* exception_ptr won't be available */\
        (exception_ptr)->penv = &(exception_ptr)->base_handler; \
        if ((t = setjmp((exception_ptr)->base_handler))) { /* yes, '=' is correct */ \
            switch(t){ \
                case MONGO_EXCEPT_NETWORK: bson_fatal_msg(0, "network error"); \
                case MONGO_EXCEPT_FIND_ERR: bson_fatal_msg(0, "error in find"); \
                default: bson_fatal_msg(0, "unknown exception"); \
            } \
        } \
    }while(0)

#define MONGO_TRY_GENERIC(connection) \
  { \
    jmp_buf *exception__prev, exception__env; \
    exception__prev = (connection)->exception.penv; \
    (connection)->exception.penv = &exception__env; \
    if (setjmp(exception__env) == 0) { \
      do

#define MONGO_CATCH_GENERIC(connection) \
      while ((connection)->exception.caught = 0, \
             (connection)->exception.caught); \
    } \
    else { \
      (connection)->exception.caught = 1; \
    } \
    (connection)->exception.penv = exception__prev; \
  } \
  if (!(connection)->exception.caught ) { } \
  else

/* Try ends with do, and Catch begins with while(0) and ends with     */
/* else, to ensure that Try/Catch syntax is similar to if/else        */
/* syntax.                                                            */
/*                                                                    */
/* The 0 in while(0) is expressed as x=0,x in order to appease        */
/* compilers that warn about constant expressions inside while().     */
/* Most compilers should still recognize that the condition is always */
/* false and avoid generating code for it.                            */

#define MONGO_THROW_GENERIC(connection, type_in) \
  for (;; longjmp(*(connection)->exception.penv, type_in)) \
    (connection)->exception.type = type_in

#define MONGO_RETHROW_GENERIC(connection) \
    MONGO_THROW_GENERIC(connection, (connection)->exception.type)

/*==============================================================*/
/* --- mongo.c */

/* ----------------------------
   message stuff
   ------------------------------ */

static void looping_write(mongo_connection * conn, const void* buf, int len){
    const char* cbuf = buf;
    while (len){
        int sent = send(conn->sock, cbuf, len, 0);
        if (sent == -1) MONGO_THROW(MONGO_EXCEPT_NETWORK);
        cbuf += sent;
        len -= sent;
    }
}

static void looping_read(mongo_connection * conn, void* buf, int len){
    char* cbuf = buf;
    while (len){
        int sent = recv(conn->sock, cbuf, len, 0);
        if (sent == 0 || sent == -1) MONGO_THROW(MONGO_EXCEPT_NETWORK);
        cbuf += sent;
        len -= sent;
    }
}

/* Always calls free(mm) */
void mongo_message_send(mongo_connection * conn, mongo_message* mm){
    mongo_header head; /* little endian */
    bson_little_endian32(&head.len, &mm->head.len);
    bson_little_endian32(&head.id, &mm->head.id);
    bson_little_endian32(&head.responseTo, &mm->head.responseTo);
    bson_little_endian32(&head.op, &mm->head.op);
    
    MONGO_TRY{
        looping_write(conn, &head, sizeof(head));
        looping_write(conn, &mm->data, mm->head.len - sizeof(head));
    }MONGO_CATCH{
        free(mm);
        MONGO_RETHROW();
    }
    free(mm);
}

char * mongo_data_append( char * start , const void * data , int len ){
    memcpy( start , data , len );
    return start + len;
}

char * mongo_data_append32( char * start , const void * data){
    bson_little_endian32( start , data );
    return start + 4;
}

char * mongo_data_append64( char * start , const void * data){
    bson_little_endian64( start , data );
    return start + 8;
}

mongo_message * mongo_message_create( int len , int id , int responseTo , int op ){
    mongo_message * mm = (mongo_message*)bson_malloc( len );

    if (!id)
        id = rand();

    /* native endian (converted on send) */
    mm->head.len = len;
    mm->head.id = id;
    mm->head.responseTo = responseTo;
    mm->head.op = op;

    return mm;
}

/* ----------------------------
   connection stuff
   ------------------------------ */
static int mongo_connect_helper( mongo_connection * conn ){
    /* setup */
    conn->sock = 0;
    conn->connected = 0;

    memset( conn->sa.sin_zero , 0 , sizeof(conn->sa.sin_zero) );
    conn->sa.sin_family = AF_INET;
    conn->sa.sin_port = htons(conn->left_opts->port);
    conn->sa.sin_addr.s_addr = inet_addr( conn->left_opts->host );
    conn->addressSize = sizeof(conn->sa);

    /* connect */
    conn->sock = socket( AF_INET, SOCK_STREAM, 0 );
    if ( conn->sock <= 0 ){
        return mongo_conn_no_socket;
    }

    if ( connect( conn->sock , (struct sockaddr*)&conn->sa , conn->addressSize ) ){
        return mongo_conn_fail;
    }

    /* nagle */
    setsockopt( conn->sock, IPPROTO_TCP, TCP_NODELAY, (char *) &one, sizeof(one) );

    /* TODO signals */

    conn->connected = 1;
    return 0;
}

mongo_conn_return mongo_connect( mongo_connection * conn , mongo_connection_options * options ){
    MONGO_INIT_EXCEPTION(&conn->exception);

    conn->left_opts = bson_malloc(sizeof(mongo_connection_options));
    conn->right_opts = NULL;

    if ( options ){
        memcpy( conn->left_opts , options , sizeof( mongo_connection_options ) );
    } else {
        strcpy( conn->left_opts->host , "127.0.0.1" );
        conn->left_opts->port = 27017;
    }

    return mongo_connect_helper(conn);
}

static void swap_repl_pair(mongo_connection * conn){
    mongo_connection_options * tmp = conn->left_opts;
    conn->left_opts = conn->right_opts;
    conn->right_opts = tmp;
}

mongo_conn_return mongo_connect_pair( mongo_connection * conn , mongo_connection_options * left, mongo_connection_options * right ){
    conn->connected = 0;
    MONGO_INIT_EXCEPTION(&conn->exception);

    conn->left_opts = NULL;
    conn->right_opts = NULL;

    if ( !left || !right )
        return mongo_conn_bad_arg;

    conn->left_opts = bson_malloc(sizeof(mongo_connection_options));
    conn->right_opts = bson_malloc(sizeof(mongo_connection_options));

    memcpy( conn->left_opts,  left,  sizeof( mongo_connection_options ) );
    memcpy( conn->right_opts, right, sizeof( mongo_connection_options ) );
    
    return mongo_reconnect(conn);
}

mongo_conn_return mongo_reconnect( mongo_connection * conn ){
    mongo_conn_return ret;
    mongo_disconnect(conn);

    /* single server */
    if(conn->right_opts == NULL)
        return mongo_connect_helper(conn);

    /* repl pair */
    ret = mongo_connect_helper(conn);
    if (ret == mongo_conn_success && mongo_cmd_ismaster(conn, NULL)){
        return mongo_conn_success;
    }

    swap_repl_pair(conn);

    ret = mongo_connect_helper(conn);
    if (ret == mongo_conn_success){
        if(mongo_cmd_ismaster(conn, NULL))
            return mongo_conn_success;
        else
            return mongo_conn_not_master;
    }

    /* failed to connect to both servers */
    return ret;
}

void mongo_insert_batch( mongo_connection * conn , const char * ns , bson ** bsons, int count){
    int size =  16 + 4 + strlen( ns ) + 1;
    int i;
    mongo_message * mm;
    char* data;

    for(i=0; i<count; i++){
        size += bson_size(bsons[i]);
    }

    mm = mongo_message_create( size , 0 , 0 , mongo_op_insert );

    data = &mm->data;
    data = mongo_data_append32(data, &zero);
    data = mongo_data_append(data, ns, strlen(ns) + 1);

    for(i=0; i<count; i++){
        data = mongo_data_append(data, bsons[i]->data, bson_size( bsons[i] ) );
    }

    mongo_message_send(conn, mm);
}

void mongo_insert( mongo_connection * conn , const char * ns , bson * bson ){
    char * data;
    mongo_message * mm = mongo_message_create( 16 /* header */
                                             + 4 /* ZERO */
                                             + strlen(ns)
                                             + 1 + bson_size(bson)
                                             , 0, 0, mongo_op_insert);

    data = &mm->data;
    data = mongo_data_append32(data, &zero);
    data = mongo_data_append(data, ns, strlen(ns) + 1);
    data = mongo_data_append(data, bson->data, bson_size(bson));

    mongo_message_send(conn, mm);
}

void mongo_update(mongo_connection* conn, const char* ns, const bson* cond, const bson* op, int flags){
    char * data;
    mongo_message * mm = mongo_message_create( 16 /* header */
                                             + 4  /* ZERO */
                                             + strlen(ns) + 1
                                             + 4  /* flags */
                                             + bson_size(cond)
                                             + bson_size(op)
                                             , 0 , 0 , mongo_op_update );

    data = &mm->data;
    data = mongo_data_append32(data, &zero);
    data = mongo_data_append(data, ns, strlen(ns) + 1);
    data = mongo_data_append32(data, &flags);
    data = mongo_data_append(data, cond->data, bson_size(cond));
    data = mongo_data_append(data, op->data, bson_size(op));

    mongo_message_send(conn, mm);
}

void mongo_remove(mongo_connection* conn, const char* ns, const bson* cond){
    char * data;
    mongo_message * mm = mongo_message_create( 16 /* header */
                                             + 4  /* ZERO */
                                             + strlen(ns) + 1
                                             + 4  /* ZERO */
                                             + bson_size(cond)
                                             , 0 , 0 , mongo_op_delete );

    data = &mm->data;
    data = mongo_data_append32(data, &zero);
    data = mongo_data_append(data, ns, strlen(ns) + 1);
    data = mongo_data_append32(data, &zero);
    data = mongo_data_append(data, cond->data, bson_size(cond));

    mongo_message_send(conn, mm);
}

mongo_reply * mongo_read_response( mongo_connection * conn ){
    mongo_header head; /* header from network */
    mongo_reply_fields fields; /* header from network */
    mongo_reply * out; /* native endian */
    int len;

    looping_read(conn, &head, sizeof(head));
    looping_read(conn, &fields, sizeof(fields));

    bson_little_endian32(&len, &head.len);
    out = (mongo_reply*)bson_malloc(len);

    out->head.len = len;
    bson_little_endian32(&out->head.id, &head.id);
    bson_little_endian32(&out->head.responseTo, &head.responseTo);
    bson_little_endian32(&out->head.op, &head.op);

    bson_little_endian32(&out->fields.flag, &fields.flag);
    bson_little_endian64(&out->fields.cursorID, &fields.cursorID);
    bson_little_endian32(&out->fields.start, &fields.start);
    bson_little_endian32(&out->fields.num, &fields.num);

    MONGO_TRY{
        looping_read(conn, &out->objs, len-sizeof(head)-sizeof(fields));
    }MONGO_CATCH{
        free(out);
        MONGO_RETHROW();
    }

    return out;
}

mongo_cursor* mongo_find(mongo_connection* conn, const char* ns, bson* query, bson* fields, int nToReturn, int nToSkip, int options){
    int sl;
    mongo_cursor * cursor;
    char * data;
    mongo_message * mm = mongo_message_create( 16 + /* header */
                                               4 + /*  options */
                                               strlen( ns ) + 1 + /* ns */
                                               4 + 4 + /* skip,return */
                                               bson_size( query ) +
                                               bson_size( fields ) ,
                                               0 , 0 , mongo_op_query );


    data = &mm->data;
    data = mongo_data_append32( data , &options );
    data = mongo_data_append( data , ns , strlen( ns ) + 1 );    
    data = mongo_data_append32( data , &nToSkip );
    data = mongo_data_append32( data , &nToReturn );
    data = mongo_data_append( data , query->data , bson_size( query ) );    
    if ( fields )
        data = mongo_data_append( data , fields->data , bson_size( fields ) );    
    
    bson_fatal_msg( (data == ((char*)mm) + mm->head.len), "query building fail!" );

    mongo_message_send( conn , mm );

    cursor = (mongo_cursor*)bson_malloc(sizeof(mongo_cursor));

    MONGO_TRY{
        cursor->mm = mongo_read_response(conn);
    }MONGO_CATCH{
        free(cursor);
        MONGO_RETHROW();
    }

    sl = strlen(ns)+1;
    cursor->ns = bson_malloc(sl);
    if (!cursor->ns){
        free(cursor->mm);
        free(cursor);
        return 0;
    }
    memcpy((void*)cursor->ns, ns, sl); /* cast needed to silence GCC warning */
    cursor->conn = conn;
    cursor->current.data = NULL;
    return cursor;
}

bson_bool_t mongo_find_one(mongo_connection* conn, const char* ns, bson* query, bson* fields, bson* out){
    mongo_cursor* cursor = mongo_find(conn, ns, query, fields, 1, 0, 0);

    if (cursor && mongo_cursor_next(cursor)){
        bson_copy(out, &cursor->current);
        mongo_cursor_destroy(cursor);
        return 1;
    }else{
        mongo_cursor_destroy(cursor);
        return 0;
    }
}

int64_t mongo_count(mongo_connection* conn, const char* db, const char* ns, bson* query){
    bson_buffer bb;
    bson cmd;
    bson out;
    int64_t count = -1;

    bson_buffer_init(&bb);
    bson_append_string(&bb, "count", ns);
    if (query && bson_size(query) > 5) /* not empty */
        bson_append_bson(&bb, "query", query);
    bson_from_buffer(&cmd, &bb);

    MONGO_TRY{
        if(mongo_run_command(conn, db, &cmd, &out)){
            bson_iterator it;
            if(bson_find(&it, &out, "n"))
                count = bson_iterator_long(&it);
        }
    }MONGO_CATCH{
        bson_destroy(&cmd);
        MONGO_RETHROW();
    }
    
    bson_destroy(&cmd);
    bson_destroy(&out);
    return count;
}

bson_bool_t mongo_disconnect( mongo_connection * conn ){
    if ( ! conn->connected )
        return 1;

#ifdef _WIN32
    closesocket( conn->sock );
#else
    close( conn->sock );
#endif
    
    conn->sock = 0;
    conn->connected = 0;
    
    return 0;
}

bson_bool_t mongo_destroy( mongo_connection * conn ){
    free(conn->left_opts);
    free(conn->right_opts);
    conn->left_opts = NULL;
    conn->right_opts = NULL;

    return mongo_disconnect( conn );
}

bson_bool_t mongo_cursor_get_more(mongo_cursor* cursor){
    if (cursor->mm && cursor->mm->fields.cursorID){
        mongo_connection* conn = cursor->conn;
        char* data;
        int sl = strlen(cursor->ns)+1;
        mongo_message * mm = mongo_message_create(16 /*header*/
                                                 +4 /*ZERO*/
                                                 +sl
                                                 +4 /*numToReturn*/
                                                 +8 /*cursorID*/
                                                 , 0, 0, mongo_op_get_more);
        data = &mm->data;
        data = mongo_data_append32(data, &zero);
        data = mongo_data_append(data, cursor->ns, sl);
        data = mongo_data_append32(data, &zero);
        data = mongo_data_append64(data, &cursor->mm->fields.cursorID);
        mongo_message_send(conn, mm);

        free(cursor->mm);

        MONGO_TRY{
            cursor->mm = mongo_read_response(cursor->conn);
        }MONGO_CATCH{
            cursor->mm = NULL;
            mongo_cursor_destroy(cursor);
            MONGO_RETHROW();
        }

        return cursor->mm && cursor->mm->fields.num;
    } else{
        return 0;
    }
}

bson_bool_t mongo_cursor_next(mongo_cursor* cursor){
    char* bson_addr;

    /* no data */
    if (!cursor->mm || cursor->mm->fields.num == 0)
        return 0;

    /* first */
    if (cursor->current.data == NULL){
        bson_init(&cursor->current, &cursor->mm->objs, 0);
        return 1;
    }

    bson_addr = cursor->current.data + bson_size(&cursor->current);
    if (bson_addr >= ((char*)cursor->mm + cursor->mm->head.len)){
        if (!mongo_cursor_get_more(cursor))
            return 0;
        bson_init(&cursor->current, &cursor->mm->objs, 0);
    } else {
        bson_init(&cursor->current, bson_addr, 0);
    }

    return 1;
}

void mongo_cursor_destroy(mongo_cursor* cursor){
    if (!cursor) return;

    if (cursor->mm && cursor->mm->fields.cursorID){
        mongo_connection* conn = cursor->conn;
        mongo_message * mm = mongo_message_create(16 /*header*/
                                                 +4 /*ZERO*/
                                                 +4 /*numCursors*/
                                                 +8 /*cursorID*/
                                                 , 0, 0, mongo_op_kill_cursors);
        char* data = &mm->data;
        data = mongo_data_append32(data, &zero);
        data = mongo_data_append32(data, &one);
        data = mongo_data_append64(data, &cursor->mm->fields.cursorID);
        
        MONGO_TRY{
            mongo_message_send(conn, mm);
        }MONGO_CATCH{
            free(cursor->mm);
            free((void*)cursor->ns);
            free(cursor);
            MONGO_RETHROW();
        }
    }
        
    free(cursor->mm);
    free((void*)cursor->ns);
    free(cursor);
}

bson_bool_t mongo_create_index(mongo_connection * conn, const char * ns, bson * key, int options, bson * out){
    bson_buffer bb;
    bson b;
    bson_iterator it;
    char name[255] = {'_'};
    int i = 1;
    char idxns[1024];

    bson_iterator_init(&it, key->data);
    while(i < 255 && bson_iterator_next(&it)){
        strncpy(name + i, bson_iterator_key(&it), 255 - i);
        i += strlen(bson_iterator_key(&it));
    }
    name[254] = '\0';

    bson_buffer_init(&bb);
    bson_append_bson(&bb, "key", key);
    bson_append_string(&bb, "ns", ns);
    bson_append_string(&bb, "name", name);
    if (options & MONGO_INDEX_UNIQUE)
        bson_append_bool(&bb, "unique", 1);
    if (options & MONGO_INDEX_DROP_DUPS)
        bson_append_bool(&bb, "dropDups", 1);
    
    bson_from_buffer(&b, &bb);

    strncpy(idxns, ns, 1024-16);
    strcpy(strchr(idxns, '.'), ".system.indexes");
    mongo_insert(conn, idxns, &b);
    bson_destroy(&b);

    *strchr(idxns, '.') = '\0'; /* just db not ns */
    return !mongo_cmd_get_last_error(conn, idxns, out);
}
bson_bool_t mongo_create_simple_index(mongo_connection * conn, const char * ns, const char* field, int options, bson * out){
    bson_buffer bb;
    bson b;
    bson_bool_t success;

    bson_buffer_init(&bb);
    bson_append_int(&bb, field, 1);
    bson_from_buffer(&b, &bb);

    success = mongo_create_index(conn, ns, &b, options, out);
    bson_destroy(&b);
    return success;
}

bson_bool_t mongo_run_command(mongo_connection * conn, const char * db, bson * command, bson * out){
    bson fields;
    int sl = strlen(db);
    char* ns = bson_malloc(sl + 5 + 1); /* ".$cmd" + nul */
    bson_bool_t success;

    strcpy(ns, db);
    strcpy(ns+sl, ".$cmd");

    success = mongo_find_one(conn, ns, command, bson_empty(&fields), out);
    free(ns);
    return success;
}
bson_bool_t mongo_simple_int_command(mongo_connection * conn, const char * db, const char* cmdstr, int arg, bson * realout){
    bson out;
    bson cmd;
    bson_buffer bb;
    bson_bool_t success = 0;

    bson_buffer_init(&bb);
    bson_append_int(&bb, cmdstr, arg);
    bson_from_buffer(&cmd, &bb);

    if(mongo_run_command(conn, db, &cmd, &out)){
        bson_iterator it;
        if(bson_find(&it, &out, "ok"))
            success = bson_iterator_bool(&it);
    }
    
    bson_destroy(&cmd);

    if (realout)
        *realout = out;
    else
        bson_destroy(&out);

    return success;
}

bson_bool_t mongo_simple_str_command(mongo_connection * conn, const char * db, const char* cmdstr, const char* arg, bson * realout){
    bson out;
    bson cmd;
    bson_buffer bb;
    bson_bool_t success = 0;

    bson_buffer_init(&bb);
    bson_append_string(&bb, cmdstr, arg);
    bson_from_buffer(&cmd, &bb);

    if(mongo_run_command(conn, db, &cmd, &out)){
        bson_iterator it;
        if(bson_find(&it, &out, "ok"))
            success = bson_iterator_bool(&it);
    }
    
    bson_destroy(&cmd);

    if (realout)
        *realout = out;
    else
        bson_destroy(&out);

    return success;
}

bson_bool_t mongo_cmd_drop_db(mongo_connection * conn, const char * db){
    return mongo_simple_int_command(conn, db, "dropDatabase", 1, NULL);
}

bson_bool_t mongo_cmd_drop_collection(mongo_connection * conn, const char * db, const char * collection, bson * out){
    return mongo_simple_str_command(conn, db, "drop", collection, out);
}

void mongo_cmd_reset_error(mongo_connection * conn, const char * db){
    mongo_simple_int_command(conn, db, "reseterror", 1, NULL);
}

static bson_bool_t mongo_cmd_get_error_helper(mongo_connection * conn, const char * db, bson * realout, const char * cmdtype){
    bson out = {NULL,0};
    bson_bool_t haserror = 1;


    if(mongo_simple_int_command(conn, db, cmdtype, 1, &out)){
        bson_iterator it;
        haserror = (bson_find(&it, &out, "err") != bson_null);
    }
    
    if(realout)
        *realout = out; /* transfer of ownership */
    else
        bson_destroy(&out);

    return haserror;
}

bson_bool_t mongo_cmd_get_prev_error(mongo_connection * conn, const char * db, bson * out){
    return mongo_cmd_get_error_helper(conn, db, out, "getpreverror");
}
bson_bool_t mongo_cmd_get_last_error(mongo_connection * conn, const char * db, bson * out){
    return mongo_cmd_get_error_helper(conn, db, out, "getlasterror");
}

bson_bool_t mongo_cmd_ismaster(mongo_connection * conn, bson * realout){
    bson out = {NULL,0};
    bson_bool_t ismaster = 0;

    if (mongo_simple_int_command(conn, "admin", "ismaster", 1, &out)){
        bson_iterator it;
        bson_find(&it, &out, "ismaster");
        ismaster = bson_iterator_bool(&it);
    }

    if(realout)
        *realout = out; /* transfer of ownership */
    else
        bson_destroy(&out);

    return ismaster;
}

static void digest2hex(mongo_md5_byte_t digest[16], char hex_digest[33]){
    static const char hex[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
    int i;
    for (i=0; i<16; i++){
        hex_digest[2*i]     = hex[(digest[i] & 0xf0) >> 4];
        hex_digest[2*i + 1] = hex[ digest[i] & 0x0f      ];
    }
    hex_digest[32] = '\0';
}

static void mongo_pass_digest(const char* user, const char* pass, char hex_digest[33]){
    mongo_md5_state_t st;
    mongo_md5_byte_t digest[16];

    mongo_md5_init(&st);
    mongo_md5_append(&st, (const mongo_md5_byte_t*)user, strlen(user));
    mongo_md5_append(&st, (const mongo_md5_byte_t*)":mongo:", 7);
    mongo_md5_append(&st, (const mongo_md5_byte_t*)pass, strlen(pass));
    mongo_md5_finish(&st, digest);
    digest2hex(digest, hex_digest);
}

void mongo_cmd_add_user(mongo_connection* conn, const char* db, const char* user, const char* pass){
    bson_buffer bb;
    bson user_obj;
    bson pass_obj;
    char hex_digest[33];
    char* ns = malloc(strlen(db) + strlen(".system.users") + 1);

    strcpy(ns, db);
    strcpy(ns+strlen(db), ".system.users");

    mongo_pass_digest(user, pass, hex_digest);

    bson_buffer_init(&bb);
    bson_append_string(&bb, "user", user);
    bson_from_buffer(&user_obj, &bb);

    bson_buffer_init(&bb);
    bson_append_start_object(&bb, "$set");
    bson_append_string(&bb, "pwd", hex_digest);
    bson_append_finish_object(&bb);
    bson_from_buffer(&pass_obj, &bb);


    MONGO_TRY{
        mongo_update(conn, ns, &user_obj, &pass_obj, MONGO_UPDATE_UPSERT);
    }MONGO_CATCH{
        free(ns);
        bson_destroy(&user_obj);
        bson_destroy(&pass_obj);
        MONGO_RETHROW();
    }

    free(ns);
    bson_destroy(&user_obj);
    bson_destroy(&pass_obj);
}

bson_bool_t mongo_cmd_authenticate(mongo_connection* conn, const char* db, const char* user, const char* pass){
    bson_buffer bb;
    bson from_db, auth_cmd;
    const char* nonce;
    bson_bool_t success = 0;

    mongo_md5_state_t st;
    mongo_md5_byte_t digest[16];
    char hex_digest[33];

    if (mongo_simple_int_command(conn, db, "getnonce", 1, &from_db)){
        bson_iterator it;
        bson_find(&it, &from_db, "nonce");
        nonce = bson_iterator_string(&it);
    }else{
        return 0;
    }

    mongo_pass_digest(user, pass, hex_digest);

    mongo_md5_init(&st);
    mongo_md5_append(&st, (const mongo_md5_byte_t*)nonce, strlen(nonce));
    mongo_md5_append(&st, (const mongo_md5_byte_t*)user, strlen(user));
    mongo_md5_append(&st, (const mongo_md5_byte_t*)hex_digest, 32);
    mongo_md5_finish(&st, digest);
    digest2hex(digest, hex_digest);

    bson_buffer_init(&bb);
    bson_append_int(&bb, "authenticate", 1);
    bson_append_string(&bb, "user", user);
    bson_append_string(&bb, "nonce", nonce);
    bson_append_string(&bb, "key", hex_digest);
    bson_from_buffer(&auth_cmd, &bb);

    bson_destroy(&from_db);

    MONGO_TRY{
        if(mongo_run_command(conn, db, &auth_cmd, &from_db)){
            bson_iterator it;
            if(bson_find(&it, &from_db, "ok"))
                success = bson_iterator_bool(&it);
        }
    }MONGO_CATCH{
        bson_destroy(&auth_cmd);
        MONGO_RETHROW();
    }

    bson_destroy(&from_db);
    bson_destroy(&auth_cmd);

    return success;
}

/*==============================================================*/

static void rpmmgoFini(void * _mgo)
	/*@globals fileSystem @*/
	/*@modifies *_mgo, fileSystem @*/
{
    rpmmgo mgo = _mgo;

    mgo->fn = _free(mgo->fn);
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmmgoPool = NULL;

static rpmmgo rpmmgoGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmmgoPool, fileSystem @*/
	/*@modifies pool, _rpmmgoPool, fileSystem @*/
{
    rpmmgo mgo;

    if (_rpmmgoPool == NULL) {
	_rpmmgoPool = rpmioNewPool("mgo", sizeof(*mgo), -1, _rpmmgo_debug,
			NULL, NULL, rpmmgoFini);
	pool = _rpmmgoPool;
    }
    mgo = (rpmmgo) rpmioGetPool(pool, sizeof(*mgo));
    memset(((char *)mgo)+sizeof(mgo->_item), 0, sizeof(*mgo)-sizeof(mgo->_item));
    return mgo;
}

rpmmgo rpmmgoNew(const char * fn, int flags)
{
    rpmmgo mgo = rpmmgoGetPool(_rpmmgoPool);
    int xx;

    if (fn)
	mgo->fn = xstrdup(fn);

    return rpmmgoLink(mgo);
}

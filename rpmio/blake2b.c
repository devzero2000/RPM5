/*
   BLAKE2 reference source code package - reference C implementations

   Written in 2012 by Samuel Neves <sneves@dei.uc.pt>

   To the extent possible under law, the author(s) have dedicated all copyright
   and related and neighboring rights to this software to the public domain
   worldwide. This software is distributed without any warranty.

   You should have received a copy of the CC0 Public Domain Dedication along with
   this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/
#undef	XXXSSE

#include "system.h"

#include "blake2.h"
#include "blake2-impl.h"

#ifdef	XXXSSE	/* compiler intrinsic defines may not be portable */
#if defined(__SSE2__)
#define HAVE_SSE2
#endif

#if defined(__SSSE3__)
#define HAVE_SSSE3
#endif

#if defined(__SSE4_1__)
#define HAVE_SSE41
#endif

#if defined(__AVX__)
#define HAVE_AVX
#endif

#if defined(__XOP__)
#define HAVE_XOP
#endif


#ifdef HAVE_AVX2
#ifndef HAVE_AVX
#define HAVE_AVX
#endif
#endif

#ifdef HAVE_XOP
#ifndef HAVE_AVX
#define HAVE_AVX
#endif
#endif

#ifdef HAVE_AVX
#ifndef HAVE_SSE41
#define HAVE_SSE41
#endif
#endif

#ifdef HAVE_SSE41
#ifndef HAVE_SSSE3
#define HAVE_SSSE3
#endif
#endif

#ifdef HAVE_SSSE3
#define HAVE_SSE2
#endif

#if !defined(HAVE_SSE2)
#error "This code requires at least SSE2."
#endif

#include <emmintrin.h>
#if defined(HAVE_SSSE3)
#include <tmmintrin.h>
#endif
#if defined(HAVE_SSE41)
#include <smmintrin.h>
#endif
#if defined(HAVE_AVX)
#include <immintrin.h>
#endif
#if defined(HAVE_XOP)
#include <x86intrin.h>
#endif
#endif	/* XXXSSE */

#include "debug.h"

#ifdef	XXXSSE
/*==============================================================*/
/* --- blake2b-round.h */

#define LOAD(p)  _mm_load_si128( (__m128i *)(p) )
#define STORE(p,r) _mm_store_si128((__m128i *)(p), r)

#define LOADU(p)  _mm_loadu_si128( (__m128i *)(p) )
#define STOREU(p,r) _mm_storeu_si128((__m128i *)(p), r)

#define TOF(reg) _mm_castsi128_ps((reg))
#define TOI(reg) _mm_castps_si128((reg))

#define LIKELY(x) __builtin_expect((x),1)


/* Microarchitecture-specific macros */
#ifndef HAVE_XOP
#ifdef HAVE_SSSE3
#define _mm_roti_epi64(x, c) \
    (-(c) == 32) ? _mm_shuffle_epi32((x), _MM_SHUFFLE(2,3,0,1))  \
    : (-(c) == 24) ? _mm_shuffle_epi8((x), r24) \
    : (-(c) == 16) ? _mm_shuffle_epi8((x), r16) \
    : (-(c) == 63) ? _mm_xor_si128(_mm_srli_epi64((x), -(c)), _mm_add_epi64((x), (x)))  \
    : _mm_xor_si128(_mm_srli_epi64((x), -(c)), _mm_slli_epi64((x), 64-(-(c))))
#else
#define _mm_roti_epi64(r, c) _mm_xor_si128(_mm_srli_epi64( (r), -(c) ),_mm_slli_epi64( (r), 64-(-c) ))
#endif
#else
/* ... */
#endif



#define G1(row1l,row2l,row3l,row4l,row1h,row2h,row3h,row4h,b0,b1) \
  row1l = _mm_add_epi64(_mm_add_epi64(row1l, b0), row2l); \
  row1h = _mm_add_epi64(_mm_add_epi64(row1h, b1), row2h); \
  \
  row4l = _mm_xor_si128(row4l, row1l); \
  row4h = _mm_xor_si128(row4h, row1h); \
  \
  row4l = _mm_roti_epi64(row4l, -32); \
  row4h = _mm_roti_epi64(row4h, -32); \
  \
  row3l = _mm_add_epi64(row3l, row4l); \
  row3h = _mm_add_epi64(row3h, row4h); \
  \
  row2l = _mm_xor_si128(row2l, row3l); \
  row2h = _mm_xor_si128(row2h, row3h); \
  \
  row2l = _mm_roti_epi64(row2l, -24); \
  row2h = _mm_roti_epi64(row2h, -24); \

#define G2(row1l,row2l,row3l,row4l,row1h,row2h,row3h,row4h,b0,b1) \
  row1l = _mm_add_epi64(_mm_add_epi64(row1l, b0), row2l); \
  row1h = _mm_add_epi64(_mm_add_epi64(row1h, b1), row2h); \
  \
  row4l = _mm_xor_si128(row4l, row1l); \
  row4h = _mm_xor_si128(row4h, row1h); \
  \
  row4l = _mm_roti_epi64(row4l, -16); \
  row4h = _mm_roti_epi64(row4h, -16); \
  \
  row3l = _mm_add_epi64(row3l, row4l); \
  row3h = _mm_add_epi64(row3h, row4h); \
  \
  row2l = _mm_xor_si128(row2l, row3l); \
  row2h = _mm_xor_si128(row2h, row3h); \
  \
  row2l = _mm_roti_epi64(row2l, -63); \
  row2h = _mm_roti_epi64(row2h, -63); \

#if defined(HAVE_SSSE3)
#define DIAGONALIZE(row1l,row2l,row3l,row4l,row1h,row2h,row3h,row4h) \
  t0 = _mm_alignr_epi8(row2h, row2l, 8); \
  t1 = _mm_alignr_epi8(row2l, row2h, 8); \
  row2l = t0; \
  row2h = t1; \
  \
  t0 = row3l; \
  row3l = row3h; \
  row3h = t0;    \
  \
  t0 = _mm_alignr_epi8(row4h, row4l, 8); \
  t1 = _mm_alignr_epi8(row4l, row4h, 8); \
  row4l = t1; \
  row4h = t0;

#define UNDIAGONALIZE(row1l,row2l,row3l,row4l,row1h,row2h,row3h,row4h) \
  t0 = _mm_alignr_epi8(row2l, row2h, 8); \
  t1 = _mm_alignr_epi8(row2h, row2l, 8); \
  row2l = t0; \
  row2h = t1; \
  \
  t0 = row3l; \
  row3l = row3h; \
  row3h = t0; \
  \
  t0 = _mm_alignr_epi8(row4l, row4h, 8); \
  t1 = _mm_alignr_epi8(row4h, row4l, 8); \
  row4l = t1; \
  row4h = t0;
#else

#define DIAGONALIZE(row1l,row2l,row3l,row4l,row1h,row2h,row3h,row4h) \
  t0 = row4l;\
  t1 = row2l;\
  row4l = row3l;\
  row3l = row3h;\
  row3h = row4l;\
  row4l = _mm_unpackhi_epi64(row4h, _mm_unpacklo_epi64(t0, t0)); \
  row4h = _mm_unpackhi_epi64(t0, _mm_unpacklo_epi64(row4h, row4h)); \
  row2l = _mm_unpackhi_epi64(row2l, _mm_unpacklo_epi64(row2h, row2h)); \
  row2h = _mm_unpackhi_epi64(row2h, _mm_unpacklo_epi64(t1, t1))

#define UNDIAGONALIZE(row1l,row2l,row3l,row4l,row1h,row2h,row3h,row4h) \
  t0 = row3l;\
  row3l = row3h;\
  row3h = t0;\
  t0 = row2l;\
  t1 = row4l;\
  row2l = _mm_unpackhi_epi64(row2h, _mm_unpacklo_epi64(row2l, row2l)); \
  row2h = _mm_unpackhi_epi64(t0, _mm_unpacklo_epi64(row2h, row2h)); \
  row4l = _mm_unpackhi_epi64(row4l, _mm_unpacklo_epi64(row4h, row4h)); \
  row4h = _mm_unpackhi_epi64(row4h, _mm_unpacklo_epi64(t1, t1))

#endif

#if defined(HAVE_SSE41)
/*==============================================================*/
/* --- blake2b-load-sse41.h */

#define LOAD_MSG_0_1(b0, b1) \
do \
{ \
b0 = _mm_unpacklo_epi64(m0, m1); \
b1 = _mm_unpacklo_epi64(m2, m3); \
} while(0)


#define LOAD_MSG_0_2(b0, b1) \
do \
{ \
b0 = _mm_unpackhi_epi64(m0, m1); \
b1 = _mm_unpackhi_epi64(m2, m3); \
} while(0)


#define LOAD_MSG_0_3(b0, b1) \
do \
{ \
b0 = _mm_unpacklo_epi64(m4, m5); \
b1 = _mm_unpacklo_epi64(m6, m7); \
} while(0)


#define LOAD_MSG_0_4(b0, b1) \
do \
{ \
b0 = _mm_unpackhi_epi64(m4, m5); \
b1 = _mm_unpackhi_epi64(m6, m7); \
} while(0)


#define LOAD_MSG_1_1(b0, b1) \
do \
{ \
b0 = _mm_unpacklo_epi64(m7, m2); \
b1 = _mm_unpackhi_epi64(m4, m6); \
} while(0)


#define LOAD_MSG_1_2(b0, b1) \
do \
{ \
b0 = _mm_unpacklo_epi64(m5, m4); \
b1 = _mm_alignr_epi8(m3, m7, 8); \
} while(0)


#define LOAD_MSG_1_3(b0, b1) \
do \
{ \
b0 = _mm_shuffle_epi32(m0, _MM_SHUFFLE(1,0,3,2)); \
b1 = _mm_unpackhi_epi64(m5, m2); \
} while(0)


#define LOAD_MSG_1_4(b0, b1) \
do \
{ \
b0 = _mm_unpacklo_epi64(m6, m1); \
b1 = _mm_unpackhi_epi64(m3, m1); \
} while(0)


#define LOAD_MSG_2_1(b0, b1) \
do \
{ \
b0 = _mm_alignr_epi8(m6, m5, 8); \
b1 = _mm_unpackhi_epi64(m2, m7); \
} while(0)


#define LOAD_MSG_2_2(b0, b1) \
do \
{ \
b0 = _mm_unpacklo_epi64(m4, m0); \
b1 = _mm_blend_epi16(m1, m6, 0xF0); \
} while(0)


#define LOAD_MSG_2_3(b0, b1) \
do \
{ \
b0 = _mm_blend_epi16(m5, m1, 0xF0); \
b1 = _mm_unpackhi_epi64(m3, m4); \
} while(0)


#define LOAD_MSG_2_4(b0, b1) \
do \
{ \
b0 = _mm_unpacklo_epi64(m7, m3); \
b1 = _mm_alignr_epi8(m2, m0, 8); \
} while(0)


#define LOAD_MSG_3_1(b0, b1) \
do \
{ \
b0 = _mm_unpackhi_epi64(m3, m1); \
b1 = _mm_unpackhi_epi64(m6, m5); \
} while(0)


#define LOAD_MSG_3_2(b0, b1) \
do \
{ \
b0 = _mm_unpackhi_epi64(m4, m0); \
b1 = _mm_unpacklo_epi64(m6, m7); \
} while(0)


#define LOAD_MSG_3_3(b0, b1) \
do \
{ \
b0 = _mm_blend_epi16(m1, m2, 0xF0); \
b1 = _mm_blend_epi16(m2, m7, 0xF0); \
} while(0)


#define LOAD_MSG_3_4(b0, b1) \
do \
{ \
b0 = _mm_unpacklo_epi64(m3, m5); \
b1 = _mm_unpacklo_epi64(m0, m4); \
} while(0)


#define LOAD_MSG_4_1(b0, b1) \
do \
{ \
b0 = _mm_unpackhi_epi64(m4, m2); \
b1 = _mm_unpacklo_epi64(m1, m5); \
} while(0)


#define LOAD_MSG_4_2(b0, b1) \
do \
{ \
b0 = _mm_blend_epi16(m0, m3, 0xF0); \
b1 = _mm_blend_epi16(m2, m7, 0xF0); \
} while(0)


#define LOAD_MSG_4_3(b0, b1) \
do \
{ \
b0 = _mm_blend_epi16(m7, m5, 0xF0); \
b1 = _mm_blend_epi16(m3, m1, 0xF0); \
} while(0)


#define LOAD_MSG_4_4(b0, b1) \
do \
{ \
b0 = _mm_alignr_epi8(m6, m0, 8); \
b1 = _mm_blend_epi16(m4, m6, 0xF0); \
} while(0)


#define LOAD_MSG_5_1(b0, b1) \
do \
{ \
b0 = _mm_unpacklo_epi64(m1, m3); \
b1 = _mm_unpacklo_epi64(m0, m4); \
} while(0)


#define LOAD_MSG_5_2(b0, b1) \
do \
{ \
b0 = _mm_unpacklo_epi64(m6, m5); \
b1 = _mm_unpackhi_epi64(m5, m1); \
} while(0)


#define LOAD_MSG_5_3(b0, b1) \
do \
{ \
b0 = _mm_blend_epi16(m2, m3, 0xF0); \
b1 = _mm_unpackhi_epi64(m7, m0); \
} while(0)


#define LOAD_MSG_5_4(b0, b1) \
do \
{ \
b0 = _mm_unpackhi_epi64(m6, m2); \
b1 = _mm_blend_epi16(m7, m4, 0xF0); \
} while(0)


#define LOAD_MSG_6_1(b0, b1) \
do \
{ \
b0 = _mm_blend_epi16(m6, m0, 0xF0); \
b1 = _mm_unpacklo_epi64(m7, m2); \
} while(0)


#define LOAD_MSG_6_2(b0, b1) \
do \
{ \
b0 = _mm_unpackhi_epi64(m2, m7); \
b1 = _mm_alignr_epi8(m5, m6, 8); \
} while(0)


#define LOAD_MSG_6_3(b0, b1) \
do \
{ \
b0 = _mm_unpacklo_epi64(m0, m3); \
b1 = _mm_shuffle_epi32(m4, _MM_SHUFFLE(1,0,3,2)); \
} while(0)


#define LOAD_MSG_6_4(b0, b1) \
do \
{ \
b0 = _mm_unpackhi_epi64(m3, m1); \
b1 = _mm_blend_epi16(m1, m5, 0xF0); \
} while(0)


#define LOAD_MSG_7_1(b0, b1) \
do \
{ \
b0 = _mm_unpackhi_epi64(m6, m3); \
b1 = _mm_blend_epi16(m6, m1, 0xF0); \
} while(0)


#define LOAD_MSG_7_2(b0, b1) \
do \
{ \
b0 = _mm_alignr_epi8(m7, m5, 8); \
b1 = _mm_unpackhi_epi64(m0, m4); \
} while(0)


#define LOAD_MSG_7_3(b0, b1) \
do \
{ \
b0 = _mm_unpackhi_epi64(m2, m7); \
b1 = _mm_unpacklo_epi64(m4, m1); \
} while(0)


#define LOAD_MSG_7_4(b0, b1) \
do \
{ \
b0 = _mm_unpacklo_epi64(m0, m2); \
b1 = _mm_unpacklo_epi64(m3, m5); \
} while(0)


#define LOAD_MSG_8_1(b0, b1) \
do \
{ \
b0 = _mm_unpacklo_epi64(m3, m7); \
b1 = _mm_alignr_epi8(m0, m5, 8); \
} while(0)


#define LOAD_MSG_8_2(b0, b1) \
do \
{ \
b0 = _mm_unpackhi_epi64(m7, m4); \
b1 = _mm_alignr_epi8(m4, m1, 8); \
} while(0)


#define LOAD_MSG_8_3(b0, b1) \
do \
{ \
b0 = m6; \
b1 = _mm_alignr_epi8(m5, m0, 8); \
} while(0)


#define LOAD_MSG_8_4(b0, b1) \
do \
{ \
b0 = _mm_blend_epi16(m1, m3, 0xF0); \
b1 = m2; \
} while(0)


#define LOAD_MSG_9_1(b0, b1) \
do \
{ \
b0 = _mm_unpacklo_epi64(m5, m4); \
b1 = _mm_unpackhi_epi64(m3, m0); \
} while(0)


#define LOAD_MSG_9_2(b0, b1) \
do \
{ \
b0 = _mm_unpacklo_epi64(m1, m2); \
b1 = _mm_blend_epi16(m3, m2, 0xF0); \
} while(0)


#define LOAD_MSG_9_3(b0, b1) \
do \
{ \
b0 = _mm_unpackhi_epi64(m7, m4); \
b1 = _mm_unpackhi_epi64(m1, m6); \
} while(0)


#define LOAD_MSG_9_4(b0, b1) \
do \
{ \
b0 = _mm_alignr_epi8(m7, m5, 8); \
b1 = _mm_unpacklo_epi64(m6, m0); \
} while(0)


#define LOAD_MSG_10_1(b0, b1) \
do \
{ \
b0 = _mm_unpacklo_epi64(m0, m1); \
b1 = _mm_unpacklo_epi64(m2, m3); \
} while(0)


#define LOAD_MSG_10_2(b0, b1) \
do \
{ \
b0 = _mm_unpackhi_epi64(m0, m1); \
b1 = _mm_unpackhi_epi64(m2, m3); \
} while(0)


#define LOAD_MSG_10_3(b0, b1) \
do \
{ \
b0 = _mm_unpacklo_epi64(m4, m5); \
b1 = _mm_unpacklo_epi64(m6, m7); \
} while(0)


#define LOAD_MSG_10_4(b0, b1) \
do \
{ \
b0 = _mm_unpackhi_epi64(m4, m5); \
b1 = _mm_unpackhi_epi64(m6, m7); \
} while(0)


#define LOAD_MSG_11_1(b0, b1) \
do \
{ \
b0 = _mm_unpacklo_epi64(m7, m2); \
b1 = _mm_unpackhi_epi64(m4, m6); \
} while(0)


#define LOAD_MSG_11_2(b0, b1) \
do \
{ \
b0 = _mm_unpacklo_epi64(m5, m4); \
b1 = _mm_alignr_epi8(m3, m7, 8); \
} while(0)


#define LOAD_MSG_11_3(b0, b1) \
do \
{ \
b0 = _mm_shuffle_epi32(m0, _MM_SHUFFLE(1,0,3,2)); \
b1 = _mm_unpackhi_epi64(m5, m2); \
} while(0)


#define LOAD_MSG_11_4(b0, b1) \
do \
{ \
b0 = _mm_unpacklo_epi64(m6, m1); \
b1 = _mm_unpackhi_epi64(m3, m1); \
} while(0)
/*==============================================================*/
#else	/* defined(HAVE_SSE41) */
/*==============================================================*/
/* --- blake2b-load-sse2.h */

#define LOAD_MSG_0_1(b0, b1) b0 = _mm_set_epi64x(m2, m0); b1 = _mm_set_epi64x(m6, m4)
#define LOAD_MSG_0_2(b0, b1) b0 = _mm_set_epi64x(m3, m1); b1 = _mm_set_epi64x(m7, m5)
#define LOAD_MSG_0_3(b0, b1) b0 = _mm_set_epi64x(m10, m8); b1 = _mm_set_epi64x(m14, m12)
#define LOAD_MSG_0_4(b0, b1) b0 = _mm_set_epi64x(m11, m9); b1 = _mm_set_epi64x(m15, m13)
#define LOAD_MSG_1_1(b0, b1) b0 = _mm_set_epi64x(m4, m14); b1 = _mm_set_epi64x(m13, m9)
#define LOAD_MSG_1_2(b0, b1) b0 = _mm_set_epi64x(m8, m10); b1 = _mm_set_epi64x(m6, m15)
#define LOAD_MSG_1_3(b0, b1) b0 = _mm_set_epi64x(m0, m1); b1 = _mm_set_epi64x(m5, m11)
#define LOAD_MSG_1_4(b0, b1) b0 = _mm_set_epi64x(m2, m12); b1 = _mm_set_epi64x(m3, m7)
#define LOAD_MSG_2_1(b0, b1) b0 = _mm_set_epi64x(m12, m11); b1 = _mm_set_epi64x(m15, m5)
#define LOAD_MSG_2_2(b0, b1) b0 = _mm_set_epi64x(m0, m8); b1 = _mm_set_epi64x(m13, m2)
#define LOAD_MSG_2_3(b0, b1) b0 = _mm_set_epi64x(m3, m10); b1 = _mm_set_epi64x(m9, m7)
#define LOAD_MSG_2_4(b0, b1) b0 = _mm_set_epi64x(m6, m14); b1 = _mm_set_epi64x(m4, m1)
#define LOAD_MSG_3_1(b0, b1) b0 = _mm_set_epi64x(m3, m7); b1 = _mm_set_epi64x(m11, m13)
#define LOAD_MSG_3_2(b0, b1) b0 = _mm_set_epi64x(m1, m9); b1 = _mm_set_epi64x(m14, m12)
#define LOAD_MSG_3_3(b0, b1) b0 = _mm_set_epi64x(m5, m2); b1 = _mm_set_epi64x(m15, m4)
#define LOAD_MSG_3_4(b0, b1) b0 = _mm_set_epi64x(m10, m6); b1 = _mm_set_epi64x(m8, m0)
#define LOAD_MSG_4_1(b0, b1) b0 = _mm_set_epi64x(m5, m9); b1 = _mm_set_epi64x(m10, m2)
#define LOAD_MSG_4_2(b0, b1) b0 = _mm_set_epi64x(m7, m0); b1 = _mm_set_epi64x(m15, m4)
#define LOAD_MSG_4_3(b0, b1) b0 = _mm_set_epi64x(m11, m14); b1 = _mm_set_epi64x(m3, m6)
#define LOAD_MSG_4_4(b0, b1) b0 = _mm_set_epi64x(m12, m1); b1 = _mm_set_epi64x(m13, m8)
#define LOAD_MSG_5_1(b0, b1) b0 = _mm_set_epi64x(m6, m2); b1 = _mm_set_epi64x(m8, m0)
#define LOAD_MSG_5_2(b0, b1) b0 = _mm_set_epi64x(m10, m12); b1 = _mm_set_epi64x(m3, m11)
#define LOAD_MSG_5_3(b0, b1) b0 = _mm_set_epi64x(m7, m4); b1 = _mm_set_epi64x(m1, m15)
#define LOAD_MSG_5_4(b0, b1) b0 = _mm_set_epi64x(m5, m13); b1 = _mm_set_epi64x(m9, m14)
#define LOAD_MSG_6_1(b0, b1) b0 = _mm_set_epi64x(m1, m12); b1 = _mm_set_epi64x(m4, m14)
#define LOAD_MSG_6_2(b0, b1) b0 = _mm_set_epi64x(m15, m5); b1 = _mm_set_epi64x(m10, m13)
#define LOAD_MSG_6_3(b0, b1) b0 = _mm_set_epi64x(m6, m0); b1 = _mm_set_epi64x(m8, m9)
#define LOAD_MSG_6_4(b0, b1) b0 = _mm_set_epi64x(m3, m7); b1 = _mm_set_epi64x(m11, m2)
#define LOAD_MSG_7_1(b0, b1) b0 = _mm_set_epi64x(m7, m13); b1 = _mm_set_epi64x(m3, m12)
#define LOAD_MSG_7_2(b0, b1) b0 = _mm_set_epi64x(m14, m11); b1 = _mm_set_epi64x(m9, m1)
#define LOAD_MSG_7_3(b0, b1) b0 = _mm_set_epi64x(m15, m5); b1 = _mm_set_epi64x(m2, m8)
#define LOAD_MSG_7_4(b0, b1) b0 = _mm_set_epi64x(m4, m0); b1 = _mm_set_epi64x(m10, m6)
#define LOAD_MSG_8_1(b0, b1) b0 = _mm_set_epi64x(m14, m6); b1 = _mm_set_epi64x(m0, m11)
#define LOAD_MSG_8_2(b0, b1) b0 = _mm_set_epi64x(m9, m15); b1 = _mm_set_epi64x(m8, m3)
#define LOAD_MSG_8_3(b0, b1) b0 = _mm_set_epi64x(m13, m12); b1 = _mm_set_epi64x(m10, m1)
#define LOAD_MSG_8_4(b0, b1) b0 = _mm_set_epi64x(m7, m2); b1 = _mm_set_epi64x(m5, m4)
#define LOAD_MSG_9_1(b0, b1) b0 = _mm_set_epi64x(m8, m10); b1 = _mm_set_epi64x(m1, m7)
#define LOAD_MSG_9_2(b0, b1) b0 = _mm_set_epi64x(m4, m2); b1 = _mm_set_epi64x(m5, m6)
#define LOAD_MSG_9_3(b0, b1) b0 = _mm_set_epi64x(m9, m15); b1 = _mm_set_epi64x(m13, m3)
#define LOAD_MSG_9_4(b0, b1) b0 = _mm_set_epi64x(m14, m11); b1 = _mm_set_epi64x(m0, m12)
#define LOAD_MSG_10_1(b0, b1) b0 = _mm_set_epi64x(m2, m0); b1 = _mm_set_epi64x(m6, m4)
#define LOAD_MSG_10_2(b0, b1) b0 = _mm_set_epi64x(m3, m1); b1 = _mm_set_epi64x(m7, m5)
#define LOAD_MSG_10_3(b0, b1) b0 = _mm_set_epi64x(m10, m8); b1 = _mm_set_epi64x(m14, m12)
#define LOAD_MSG_10_4(b0, b1) b0 = _mm_set_epi64x(m11, m9); b1 = _mm_set_epi64x(m15, m13)
#define LOAD_MSG_11_1(b0, b1) b0 = _mm_set_epi64x(m4, m14); b1 = _mm_set_epi64x(m13, m9)
#define LOAD_MSG_11_2(b0, b1) b0 = _mm_set_epi64x(m8, m10); b1 = _mm_set_epi64x(m6, m15)
#define LOAD_MSG_11_3(b0, b1) b0 = _mm_set_epi64x(m0, m1); b1 = _mm_set_epi64x(m5, m11)
#define LOAD_MSG_11_4(b0, b1) b0 = _mm_set_epi64x(m2, m12); b1 = _mm_set_epi64x(m3, m7)
/*==============================================================*/
#endif	/* defined(HAVE_SSE41) */

#define ROUND(r) \
  LOAD_MSG_ ##r ##_1(b0, b1); \
  G1(row1l,row2l,row3l,row4l,row1h,row2h,row3h,row4h,b0,b1); \
  LOAD_MSG_ ##r ##_2(b0, b1); \
  G2(row1l,row2l,row3l,row4l,row1h,row2h,row3h,row4h,b0,b1); \
  DIAGONALIZE(row1l,row2l,row3l,row4l,row1h,row2h,row3h,row4h); \
  LOAD_MSG_ ##r ##_3(b0, b1); \
  G1(row1l,row2l,row3l,row4l,row1h,row2h,row3h,row4h,b0,b1); \
  LOAD_MSG_ ##r ##_4(b0, b1); \
  G2(row1l,row2l,row3l,row4l,row1h,row2h,row3h,row4h,b0,b1); \
  UNDIAGONALIZE(row1l,row2l,row3l,row4l,row1h,row2h,row3h,row4h);
/*==============================================================*/
#else	/* XXXSSE */
/*==============================================================*/
static const uint8_t blake2b_sigma[12][16] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    {14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3},
    {11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4},
    {7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8},
    {9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13},
    {2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9},
    {12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11},
    {13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10},
    {6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5},
    {10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    {14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3}
};

#define G(r,i,a,b,c,d) \
  do { \
    a = a + b + m[blake2b_sigma[r][2*i+0]]; \
    d = rotr64(d ^ a, 32); \
    c = c + d; \
    b = rotr64(b ^ c, 24); \
    a = a + b + m[blake2b_sigma[r][2*i+1]]; \
    d = rotr64(d ^ a, 16); \
    c = c + d; \
    b = rotr64(b ^ c, 63); \
  } while(0)
#define ROUND(r)  \
  do { \
    G(r,0,v[ 0],v[ 4],v[ 8],v[12]); \
    G(r,1,v[ 1],v[ 5],v[ 9],v[13]); \
    G(r,2,v[ 2],v[ 6],v[10],v[14]); \
    G(r,3,v[ 3],v[ 7],v[11],v[15]); \
    G(r,4,v[ 0],v[ 5],v[10],v[15]); \
    G(r,5,v[ 1],v[ 6],v[11],v[12]); \
    G(r,6,v[ 2],v[ 7],v[ 8],v[13]); \
    G(r,7,v[ 3],v[ 4],v[ 9],v[14]); \
  } while(0)
/*==============================================================*/
#endif	/* XXXSSE */

ALIGN(64)
static const uint64_t blake2b_IV[8] = {
    0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
    0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
    0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
    0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
};


static inline int blake2b_set_lastnode(blake2b_state * S)
{
    S->f[1] = ~0ULL;
    return 0;
}

/* Some helper functions, not necessarily useful */
static inline int blake2b_clear_lastnode(blake2b_state * S)
{
    S->f[1] = 0ULL;
    return 0;
}

static inline int blake2b_set_lastblock(blake2b_state * S)
{
    if (S->last_node)
	blake2b_set_lastnode(S);

    S->f[0] = ~0ULL;
    return 0;
}

static inline int blake2b_clear_lastblock(blake2b_state * S)
{
    if (S->last_node)
	blake2b_clear_lastnode(S);

    S->f[0] = 0ULL;
    return 0;
}

static inline int blake2b_increment_counter(blake2b_state * S,
					    const uint64_t inc)
{
#ifdef	XXXSSE
#if __x86_64__
    // ADD/ADC chain
    __uint128_t t = ((__uint128_t) S->t[1] << 64) | S->t[0];
    t += inc;
    S->t[0] = (uint64_t) (t >> 0);
    S->t[1] = (uint64_t) (t >> 64);
#else
    S->t[0] += inc;
    S->t[1] += (S->t[0] < inc);
#endif
#else	/* XXXSSE */
    S->t[0] += inc;
    S->t[1] += (S->t[0] < inc);
#endif	/* XXXSSE */
    return 0;
}



// Parameter-related functions
static inline int blake2b_param_set_digest_length(blake2b_param * P,
						  const uint8_t
						  digest_length)
{
    P->digest_length = digest_length;
    return 0;
}

static inline int blake2b_param_set_fanout(blake2b_param * P,
					   const uint8_t fanout)
{
    P->fanout = fanout;
    return 0;
}

static inline int blake2b_param_set_max_depth(blake2b_param * P,
					      const uint8_t depth)
{
    P->depth = depth;
    return 0;
}

static inline int blake2b_param_set_leaf_length(blake2b_param * P,
						const uint32_t leaf_length)
{
    store32(&P->leaf_length, leaf_length);
    return 0;
}

static inline int blake2b_param_set_node_offset(blake2b_param * P,
						const uint64_t node_offset)
{
    store64(&P->node_offset, node_offset);
    return 0;
}

static inline int blake2b_param_set_node_depth(blake2b_param * P,
					       const uint8_t node_depth)
{
    P->node_depth = node_depth;
    return 0;
}

static inline int blake2b_param_set_inner_length(blake2b_param * P,
						 const uint8_t
						 inner_length)
{
    P->inner_length = inner_length;
    return 0;
}

static inline int blake2b_param_set_salt(blake2b_param * P,
					 const uint8_t
					 salt[BLAKE2B_SALTBYTES])
{
    memcpy(P->salt, salt, BLAKE2B_SALTBYTES);
    return 0;
}

static inline int blake2b_param_set_personal(blake2b_param * P,
					     const uint8_t
					     personal
					     [BLAKE2B_PERSONALBYTES])
{
    memcpy(P->personal, personal, BLAKE2B_PERSONALBYTES);
    return 0;
}

static inline int blake2b_init0(blake2b_state * S)
{
    int i;
    memset(S, 0, sizeof(blake2b_state));

    for (i = 0; i < 8; ++i)
	S->h[i] = blake2b_IV[i];

    return 0;
}

/* init xors IV with input parameter block */
int blake2b_init_param(blake2b_state * S, const blake2b_param * P)
{
    size_t i;
#ifdef	XXXSSE
    uint8_t *p, *h, *v;
    //blake2b_init0( S );
    v = (uint8_t *) (blake2b_IV);
    h = (uint8_t *) (S->h);
    p = (uint8_t *) (P);
    /* IV XOR ParamBlock */
    memset(S, 0, sizeof(blake2b_state));

    for (i = 0; i < BLAKE2B_OUTBYTES; ++i)
	h[i] = v[i] ^ p[i];
#else	/* XXXSSE */
    blake2b_init0(S);
    uint8_t *p = (uint8_t *) (P);

    /* IV XOR ParamBlock */
    for (i = 0; i < 8; ++i)
	S->h[i] ^= load64(p + sizeof(S->h[i]) * i);
#endif	/* XXXSSE */

    return 0;
}



int blake2b_init(blake2b_state * S, const uint8_t outlen)
{
    blake2b_param P[1];

    if (outlen == 0 || outlen > BLAKE2B_OUTBYTES)
	return -1;

#ifdef	XXXSSE
    {	const blake2b_param _P = {
	    .digest_length	= outlen,
	    .key_length		= 0,
	    .fanout		= 1,
	    .depth 		= 1,
	    .leaf_length	= 0,
	    .node_offset	= 0,
	    .node_depth		= 0,
	    .inner_length	= 0,
	    .reserved		= {0},
	    .salt		= {0},
	    .personal		= {0}
	};
	P[0] = *(blake2b_param *)&_P;	/* structure assignment */
    }
#else	/* XXXSSE */
    P->digest_length = outlen;
    P->key_length = 0;
    P->fanout = 1;
    P->depth = 1;
    store32(&P->leaf_length, 0);
    store64(&P->node_offset, 0);
    P->node_depth = 0;
    P->inner_length = 0;
    memset(P->reserved, 0, sizeof(P->reserved) );
    memset(P->salt, 0, sizeof(P->salt));
    memset(P->personal, 0, sizeof(P->personal));
#endif	/* XXXSSE */

    return blake2b_init_param(S, P);
}


int blake2b_init_key(blake2b_state * S, const uint8_t outlen,
		     const void *key, const uint8_t keylen)
{
    blake2b_param P[1];

    if (outlen == 0 || outlen > BLAKE2B_OUTBYTES)
	return -1;

    if (!key || !keylen || keylen > BLAKE2B_KEYBYTES)
	return -1;

#ifdef	XXXSSE
    {	const blake2b_param _P = {
	    .digest_length	= outlen,
	    .key_length		= keylen,
	    .fanout		= 1,
	    .depth 		= 1,
	    .leaf_length	= 0,
	    .node_offset	= 0,
	    .node_depth		= 0,
	    .inner_length	= 0,
	    .reserved		= {0},
	    .salt		= {0},
	    .personal		= {0}
	};
	P[0] = *(blake2b_param *)&_P;	/* structure assignment */
    }
#else	/* XXXSSE */
    P->digest_length = outlen;
    P->key_length = keylen;
    P->fanout = 1;
    P->depth = 1;
    store32(&P->leaf_length, 0);
    store64(&P->node_offset, 0);
    P->node_depth = 0;
    P->inner_length = 0;
    memset(P->reserved, 0, sizeof(P->reserved) );
    memset(P->salt, 0, sizeof(P->salt));
    memset(P->personal, 0, sizeof(P->personal));
#endif	/* XXXSSE */

    if (blake2b_init_param(S, P) < 0)
	return -1;

    {
	uint8_t block[BLAKE2B_BLOCKBYTES];
	memset(block, 0, BLAKE2B_BLOCKBYTES);
	memcpy(block, key, keylen);
	blake2b_update(S, block, BLAKE2B_BLOCKBYTES);
	secure_zero_memory(block, BLAKE2B_BLOCKBYTES);	/* Burn the key from stack */
    }
    return 0;
}

static inline int blake2b_compress(blake2b_state * S,
			    const uint8_t block[BLAKE2B_BLOCKBYTES])
{
#ifdef	XXXSSE
    __m128i row1l, row1h;
    __m128i row2l, row2h;
    __m128i row3l, row3h;
    __m128i row4l, row4h;
    __m128i b0, b1;
    __m128i t0, t1;
#if defined(HAVE_SSSE3) && !defined(HAVE_XOP)
    const __m128i r16 =
	_mm_setr_epi8(2, 3, 4, 5, 6, 7, 0, 1, 10, 11, 12, 13, 14, 15, 8,
		      9);
    const __m128i r24 =
	_mm_setr_epi8(3, 4, 5, 6, 7, 0, 1, 2, 11, 12, 13, 14, 15, 8, 9,
		      10);
#endif
#if defined(HAVE_SSE41)
    const __m128i m0 = LOADU(block + 00);
    const __m128i m1 = LOADU(block + 16);
    const __m128i m2 = LOADU(block + 32);
    const __m128i m3 = LOADU(block + 48);
    const __m128i m4 = LOADU(block + 64);
    const __m128i m5 = LOADU(block + 80);
    const __m128i m6 = LOADU(block + 96);
    const __m128i m7 = LOADU(block + 112);
#else
    const uint64_t m0 = ((uint64_t *) block)[0];
    const uint64_t m1 = ((uint64_t *) block)[1];
    const uint64_t m2 = ((uint64_t *) block)[2];
    const uint64_t m3 = ((uint64_t *) block)[3];
    const uint64_t m4 = ((uint64_t *) block)[4];
    const uint64_t m5 = ((uint64_t *) block)[5];
    const uint64_t m6 = ((uint64_t *) block)[6];
    const uint64_t m7 = ((uint64_t *) block)[7];
    const uint64_t m8 = ((uint64_t *) block)[8];
    const uint64_t m9 = ((uint64_t *) block)[9];
    const uint64_t m10 = ((uint64_t *) block)[10];
    const uint64_t m11 = ((uint64_t *) block)[11];
    const uint64_t m12 = ((uint64_t *) block)[12];
    const uint64_t m13 = ((uint64_t *) block)[13];
    const uint64_t m14 = ((uint64_t *) block)[14];
    const uint64_t m15 = ((uint64_t *) block)[15];
#endif
    row1l = LOADU(&S->h[0]);
    row1h = LOADU(&S->h[2]);
    row2l = LOADU(&S->h[4]);
    row2h = LOADU(&S->h[6]);
    row3l = LOADU(&blake2b_IV[0]);
    row3h = LOADU(&blake2b_IV[2]);
    row4l = _mm_xor_si128(LOADU(&blake2b_IV[4]), LOADU(&S->t[0]));
    row4h = _mm_xor_si128(LOADU(&blake2b_IV[6]), LOADU(&S->f[0]));
#else	/* XXXSSE */
    uint64_t m[16];
    uint64_t v[16];
    int i;

    for (i = 0; i < 16; ++i)
	m[i] = load64(block + i * sizeof(m[i]));

    for (i = 0; i < 8; ++i)
	v[i] = S->h[i];

    v[8] = blake2b_IV[0];
    v[9] = blake2b_IV[1];
    v[10] = blake2b_IV[2];
    v[11] = blake2b_IV[3];
    v[12] = S->t[0] ^ blake2b_IV[4];
    v[13] = S->t[1] ^ blake2b_IV[5];
    v[14] = S->f[0] ^ blake2b_IV[6];
    v[15] = S->f[1] ^ blake2b_IV[7];
#endif	/* XXXSSE */

    ROUND(0);
    ROUND(1);
    ROUND(2);
    ROUND(3);
    ROUND(4);
    ROUND(5);
    ROUND(6);
    ROUND(7);
    ROUND(8);
    ROUND(9);
    ROUND(10);
    ROUND(11);

#ifdef	XXXSSE
    row1l = _mm_xor_si128(row3l, row1l);
    row1h = _mm_xor_si128(row3h, row1h);
    STOREU(&S->h[0], _mm_xor_si128(LOADU(&S->h[0]), row1l));
    STOREU(&S->h[2], _mm_xor_si128(LOADU(&S->h[2]), row1h));
    row2l = _mm_xor_si128(row4l, row2l);
    row2h = _mm_xor_si128(row4h, row2h);
    STOREU(&S->h[4], _mm_xor_si128(LOADU(&S->h[4]), row2l));
    STOREU(&S->h[6], _mm_xor_si128(LOADU(&S->h[6]), row2h));
#else	/* XXXSSE */
    for (i = 0; i < 8; ++i)
	S->h[i] = S->h[i] ^ v[i] ^ v[i + 8];
#endif	/* XXXSSE */

    return 0;
}

int blake2b_update(blake2b_state * S, const uint8_t * in, uint64_t inlen)
{
    while (inlen > 0) {
	size_t left = S->buflen;
	size_t fill = 2 * BLAKE2B_BLOCKBYTES - left;

	if (inlen > fill) {
	    memcpy(S->buf + left, in, fill);	// Fill buffer
	    S->buflen += fill;
	    blake2b_increment_counter(S, BLAKE2B_BLOCKBYTES);
	    blake2b_compress(S, S->buf);	// Compress
	    memcpy(S->buf, S->buf + BLAKE2B_BLOCKBYTES, BLAKE2B_BLOCKBYTES);	// Shift buffer left
	    S->buflen -= BLAKE2B_BLOCKBYTES;
	    in += fill;
	    inlen -= fill;
	} else			// inlen <= fill
	{
	    memcpy(S->buf + left, in, inlen);
	    S->buflen += inlen;	// Be lazy, do not compress
	    in += inlen;
	    inlen -= inlen;
	}
    }

    return 0;
}

int blake2b_final(blake2b_state * S, uint8_t * out, uint8_t outlen)
{

    if (S->buflen > BLAKE2B_BLOCKBYTES) {
	blake2b_increment_counter(S, BLAKE2B_BLOCKBYTES);
	blake2b_compress(S, S->buf);
	S->buflen -= BLAKE2B_BLOCKBYTES;
	memcpy(S->buf, S->buf + BLAKE2B_BLOCKBYTES, S->buflen);
    }

    blake2b_increment_counter(S, S->buflen);
    blake2b_set_lastblock(S);
    memset(S->buf + S->buflen, 0, 2 * BLAKE2B_BLOCKBYTES - S->buflen);	/* Padding */
    blake2b_compress(S, S->buf);

#ifdef	XXXSSE
    memcpy( out, &S->h[0], outlen );
#else	/* XXXSSE */
    {	uint8_t buffer[BLAKE2B_OUTBYTES];
	int i;
	for (i = 0; i < 8; ++i)	/* Output full hash to temp buffer */
	    store64(buffer + sizeof(S->h[i]) * i, S->h[i]);

	memcpy(out, buffer, outlen);
    }
#endif	/* XXXSSE */
    return 0;
}

/* inlen, at least, should be uint64_t. Others can be size_t. */
int blake2b(uint8_t * out, const void *in, const void *key,
	    const uint8_t outlen, const uint64_t inlen, uint8_t keylen)
{
    blake2b_state S[1];

    /* Verify parameters */
    if (NULL == in)
	return -1;

    if (NULL == out)
	return -1;

    if (NULL == key)
	keylen = 0;

    if (keylen > 0) {
	if (blake2b_init_key(S, outlen, key, keylen) < 0)
	    return -1;
    } else {
	if (blake2b_init(S, outlen) < 0)
	    return -1;
    }

    blake2b_update(S, (uint8_t *) in, inlen);
    blake2b_final(S, out, outlen);
    return 0;
}


#if defined(SUPERCOP)
int crypto_hash(unsigned char *out, unsigned char *in,
		unsigned long long inlen)
{
    return blake2b(out, in, NULL, BLAKE2B_OUTBYTES, inlen, 0);
}
#endif


#if defined(BLAKE2B_SELFTEST)
#include "blake2-kat.h"
int main(int argc, char **argv)
{
    uint8_t key[BLAKE2B_KEYBYTES];
    uint8_t buf[KAT_LENGTH];
    size_t i;

    for (i = 0; i < BLAKE2B_KEYBYTES; ++i)
	key[i] = (uint8_t) i;

    for (i = 0; i < KAT_LENGTH; ++i)
	buf[i] = (uint8_t) i;

    for (i = 0; i < KAT_LENGTH; ++i) {
	uint8_t hash[BLAKE2B_OUTBYTES];

	if (blake2b(hash, buf, key, BLAKE2B_OUTBYTES, i, BLAKE2B_KEYBYTES) < 0
	 || memcmp(hash, blake2b_keyed_kat[i], BLAKE2B_OUTBYTES)) {
	    puts("error");
	    return -1;
	}
    }

    puts("ok");
    return 0;
}
#endif

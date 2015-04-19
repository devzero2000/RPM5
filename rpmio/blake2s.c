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
#endif	/* XXXSSE */

#endif

#include "debug.h"

#ifdef	XXXSSE
/*==============================================================*/
/* --- blake2s-round.h */

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
#define _mm_roti_epi32(r, c) ( \
                (8==-(c)) ? _mm_shuffle_epi8(r,r8) \
              : (16==-(c)) ? _mm_shuffle_epi8(r,r16) \
              : _mm_xor_si128(_mm_srli_epi32( (r), -(c) ),_mm_slli_epi32( (r), 32-(-(c)) )) )
#else
#define _mm_roti_epi32(r, c) _mm_xor_si128(_mm_srli_epi32( (r), -(c) ),_mm_slli_epi32( (r), 32-(-c) ))
#endif
#else
/* ... */
#endif


#define G1(row1,row2,row3,row4,buf) \
  row1 = _mm_add_epi32( _mm_add_epi32( row1, buf), row2 ); \
  row4 = _mm_xor_si128( row4, row1 ); \
  row4 = _mm_roti_epi32(row4, -16); \
  row3 = _mm_add_epi32( row3, row4 );   \
  row2 = _mm_xor_si128( row2, row3 ); \
  row2 = _mm_roti_epi32(row2, -12);

#define G2(row1,row2,row3,row4,buf) \
  row1 = _mm_add_epi32( _mm_add_epi32( row1, buf), row2 ); \
  row4 = _mm_xor_si128( row4, row1 ); \
  row4 = _mm_roti_epi32(row4, -8); \
  row3 = _mm_add_epi32( row3, row4 );   \
  row2 = _mm_xor_si128( row2, row3 ); \
  row2 = _mm_roti_epi32(row2, -7);

#define DIAGONALIZE(row1,row2,row3,row4) \
  row4 = _mm_shuffle_epi32( row4, _MM_SHUFFLE(2,1,0,3) ); \
  row3 = _mm_shuffle_epi32( row3, _MM_SHUFFLE(1,0,3,2) ); \
  row2 = _mm_shuffle_epi32( row2, _MM_SHUFFLE(0,3,2,1) );

#define UNDIAGONALIZE(row1,row2,row3,row4) \
  row4 = _mm_shuffle_epi32( row4, _MM_SHUFFLE(0,3,2,1) ); \
  row3 = _mm_shuffle_epi32( row3, _MM_SHUFFLE(1,0,3,2) ); \
  row2 = _mm_shuffle_epi32( row2, _MM_SHUFFLE(2,1,0,3) );

#if defined(HAVE_XOP)
/*==============================================================*/
/* --- blake2s-load-xop.h */

#define TOB(x) ((x)*4*0x01010101 + 0x03020100)	// ..or not TOB

/* Basic VPPERM emulation, for testing purposes */
/*static __m128i _mm_perm_epi8(const __m128i src1, const __m128i src2, const __m128i sel)
{
   const __m128i sixteen = _mm_set1_epi8(16);
   const __m128i t0 = _mm_shuffle_epi8(src1, sel);
   const __m128i s1 = _mm_shuffle_epi8(src2, _mm_sub_epi8(sel, sixteen));
   const __m128i mask = _mm_or_si128(_mm_cmpeq_epi8(sel, sixteen),
                                     _mm_cmpgt_epi8(sel, sixteen)); // (>=16) = 0xff : 00
   return _mm_blendv_epi8(t0, s1, mask);
}*/

#define LOAD_MSG_0_1(buf) \
buf = _mm_perm_epi8(m0, m1, _mm_set_epi32(TOB(6),TOB(4),TOB(2),TOB(0)) );

#define LOAD_MSG_0_2(buf) \
buf = _mm_perm_epi8(m0, m1, _mm_set_epi32(TOB(7),TOB(5),TOB(3),TOB(1)) );

#define LOAD_MSG_0_3(buf) \
buf = _mm_perm_epi8(m2, m3, _mm_set_epi32(TOB(6),TOB(4),TOB(2),TOB(0)) );

#define LOAD_MSG_0_4(buf) \
buf = _mm_perm_epi8(m2, m3, _mm_set_epi32(TOB(7),TOB(5),TOB(3),TOB(1)) );

#define LOAD_MSG_1_1(buf) \
t0 = _mm_perm_epi8(m1, m2, _mm_set_epi32(TOB(0),TOB(5),TOB(0),TOB(0)) ); \
buf = _mm_perm_epi8(t0, m3, _mm_set_epi32(TOB(5),TOB(2),TOB(1),TOB(6)) );

#define LOAD_MSG_1_2(buf) \
t1 = _mm_perm_epi8(m1, m2, _mm_set_epi32(TOB(2),TOB(0),TOB(4),TOB(6)) ); \
buf = _mm_perm_epi8(t1, m3, _mm_set_epi32(TOB(3),TOB(7),TOB(1),TOB(0)) );

#define LOAD_MSG_1_3(buf) \
t0 = _mm_perm_epi8(m0, m1, _mm_set_epi32(TOB(5),TOB(0),TOB(0),TOB(1)) ); \
buf = _mm_perm_epi8(t0, m2, _mm_set_epi32(TOB(3),TOB(7),TOB(1),TOB(0)) );

#define LOAD_MSG_1_4(buf) \
t1 = _mm_perm_epi8(m0, m1, _mm_set_epi32(TOB(3),TOB(7),TOB(2),TOB(0)) ); \
buf = _mm_perm_epi8(t1, m3, _mm_set_epi32(TOB(3),TOB(2),TOB(1),TOB(4)) );

#define LOAD_MSG_2_1(buf) \
t0 = _mm_perm_epi8(m1, m2, _mm_set_epi32(TOB(0),TOB(1),TOB(0),TOB(7)) ); \
buf = _mm_perm_epi8(t0, m3, _mm_set_epi32(TOB(7),TOB(2),TOB(4),TOB(0)) );

#define LOAD_MSG_2_2(buf) \
t1 = _mm_perm_epi8(m0, m2, _mm_set_epi32(TOB(0),TOB(2),TOB(0),TOB(4)) ); \
buf = _mm_perm_epi8(t1, m3, _mm_set_epi32(TOB(5),TOB(2),TOB(1),TOB(0)) );

#define LOAD_MSG_2_3(buf) \
t0 = _mm_perm_epi8(m0, m1, _mm_set_epi32(TOB(0),TOB(7),TOB(3),TOB(0)) ); \
buf = _mm_perm_epi8(t0, m2, _mm_set_epi32(TOB(5),TOB(2),TOB(1),TOB(6)) );

#define LOAD_MSG_2_4(buf) \
t1 = _mm_perm_epi8(m0, m1, _mm_set_epi32(TOB(4),TOB(1),TOB(6),TOB(0)) ); \
buf = _mm_perm_epi8(t1, m3, _mm_set_epi32(TOB(3),TOB(2),TOB(1),TOB(6)) );

#define LOAD_MSG_3_1(buf) \
t0 = _mm_perm_epi8(m0, m1, _mm_set_epi32(TOB(0),TOB(0),TOB(3),TOB(7)) ); \
t0 = _mm_perm_epi8(t0, m2, _mm_set_epi32(TOB(7),TOB(2),TOB(1),TOB(0)) ); \
buf = _mm_perm_epi8(t0, m3, _mm_set_epi32(TOB(3),TOB(5),TOB(1),TOB(0)) );

#define LOAD_MSG_3_2(buf) \
t1 = _mm_perm_epi8(m0, m2, _mm_set_epi32(TOB(0),TOB(0),TOB(1),TOB(5)) ); \
buf = _mm_perm_epi8(t1, m3, _mm_set_epi32(TOB(6),TOB(4),TOB(1),TOB(0)) );

#define LOAD_MSG_3_3(buf) \
t0 = _mm_perm_epi8(m0, m1, _mm_set_epi32(TOB(0),TOB(4),TOB(5),TOB(2)) ); \
buf = _mm_perm_epi8(t0, m3, _mm_set_epi32(TOB(7),TOB(2),TOB(1),TOB(0)) );

#define LOAD_MSG_3_4(buf) \
t1 = _mm_perm_epi8(m0, m1, _mm_set_epi32(TOB(0),TOB(0),TOB(0),TOB(6)) ); \
buf = _mm_perm_epi8(t1, m2, _mm_set_epi32(TOB(4),TOB(2),TOB(6),TOB(0)) );

#define LOAD_MSG_4_1(buf) \
t0 = _mm_perm_epi8(m0, m1, _mm_set_epi32(TOB(0),TOB(2),TOB(5),TOB(0)) ); \
buf = _mm_perm_epi8(t0, m2, _mm_set_epi32(TOB(6),TOB(2),TOB(1),TOB(5)) );

#define LOAD_MSG_4_2(buf) \
t1 = _mm_perm_epi8(m0, m1, _mm_set_epi32(TOB(0),TOB(4),TOB(7),TOB(0)) ); \
buf = _mm_perm_epi8(t1, m3, _mm_set_epi32(TOB(7),TOB(2),TOB(1),TOB(0)) );

#define LOAD_MSG_4_3(buf) \
t0 = _mm_perm_epi8(m0, m1, _mm_set_epi32(TOB(3),TOB(6),TOB(0),TOB(0)) ); \
t0 = _mm_perm_epi8(t0, m2, _mm_set_epi32(TOB(3),TOB(2),TOB(7),TOB(0)) ); \
buf = _mm_perm_epi8(t0, m3, _mm_set_epi32(TOB(3),TOB(2),TOB(1),TOB(6)) );

#define LOAD_MSG_4_4(buf) \
t1 = _mm_perm_epi8(m0, m2, _mm_set_epi32(TOB(0),TOB(4),TOB(0),TOB(1)) ); \
buf = _mm_perm_epi8(t1, m3, _mm_set_epi32(TOB(5),TOB(2),TOB(4),TOB(0)) );

#define LOAD_MSG_5_1(buf) \
t0 = _mm_perm_epi8(m0, m1, _mm_set_epi32(TOB(0),TOB(0),TOB(6),TOB(2)) ); \
buf = _mm_perm_epi8(t0, m2, _mm_set_epi32(TOB(4),TOB(2),TOB(1),TOB(0)) );

#define LOAD_MSG_5_2(buf) \
t1 = _mm_perm_epi8(m0, m2, _mm_set_epi32(TOB(3),TOB(7),TOB(6),TOB(0)) ); \
buf = _mm_perm_epi8(t1, m3, _mm_set_epi32(TOB(3),TOB(2),TOB(1),TOB(4)) );

#define LOAD_MSG_5_3(buf) \
t0 = _mm_perm_epi8(m0, m1, _mm_set_epi32(TOB(1),TOB(0),TOB(7),TOB(4)) ); \
buf = _mm_perm_epi8(t0, m3, _mm_set_epi32(TOB(3),TOB(7),TOB(1),TOB(0)) );

#define LOAD_MSG_5_4(buf) \
t1 = _mm_perm_epi8(m1, m2, _mm_set_epi32(TOB(5),TOB(0),TOB(1),TOB(0)) ); \
buf = _mm_perm_epi8(t1, m3, _mm_set_epi32(TOB(3),TOB(6),TOB(1),TOB(5)) );

#define LOAD_MSG_6_1(buf) \
t0 = _mm_perm_epi8(m0, m1, _mm_set_epi32(TOB(4),TOB(0),TOB(1),TOB(0)) ); \
buf = _mm_perm_epi8(t0, m3, _mm_set_epi32(TOB(3),TOB(6),TOB(1),TOB(4)) );

#define LOAD_MSG_6_2(buf) \
t1 = _mm_perm_epi8(m1, m2, _mm_set_epi32(TOB(6),TOB(0),TOB(0),TOB(1)) ); \
buf = _mm_perm_epi8(t1, m3, _mm_set_epi32(TOB(3),TOB(5),TOB(7),TOB(0)) );

#define LOAD_MSG_6_3(buf) \
t0 = _mm_perm_epi8(m0, m1, _mm_set_epi32(TOB(0),TOB(0),TOB(6),TOB(0)) ); \
buf = _mm_perm_epi8(t0, m2, _mm_set_epi32(TOB(4),TOB(5),TOB(1),TOB(0)) );

#define LOAD_MSG_6_4(buf) \
t1 = _mm_perm_epi8(m0, m1, _mm_set_epi32(TOB(0),TOB(2),TOB(3),TOB(7)) ); \
buf = _mm_perm_epi8(t1, m2, _mm_set_epi32(TOB(7),TOB(2),TOB(1),TOB(0)) );

#define LOAD_MSG_7_1(buf) \
t0 = _mm_perm_epi8(m0, m1, _mm_set_epi32(TOB(3),TOB(0),TOB(7),TOB(0)) ); \
buf = _mm_perm_epi8(t0, m3, _mm_set_epi32(TOB(3),TOB(4),TOB(1),TOB(5)) );

#define LOAD_MSG_7_2(buf) \
t1 = _mm_perm_epi8(m0, m2, _mm_set_epi32(TOB(5),TOB(1),TOB(0),TOB(7)) ); \
buf = _mm_perm_epi8(t1, m3, _mm_set_epi32(TOB(3),TOB(2),TOB(6),TOB(0)) );

#define LOAD_MSG_7_3(buf) \
t0 = _mm_perm_epi8(m0, m1, _mm_set_epi32(TOB(2),TOB(0),TOB(0),TOB(5)) ); \
t0 = _mm_perm_epi8(t0, m2, _mm_set_epi32(TOB(3),TOB(4),TOB(1),TOB(0)) ); \
buf = _mm_perm_epi8(t0, m3, _mm_set_epi32(TOB(3),TOB(2),TOB(7),TOB(0)) );

#define LOAD_MSG_7_4(buf) \
t1 = _mm_perm_epi8(m0, m1, _mm_set_epi32(TOB(0),TOB(6),TOB(4),TOB(0)) ); \
buf = _mm_perm_epi8(t1, m2, _mm_set_epi32(TOB(6),TOB(2),TOB(1),TOB(0)) );

#define LOAD_MSG_8_1(buf) \
t0 = _mm_perm_epi8(m0, m1, _mm_set_epi32(TOB(0),TOB(0),TOB(0),TOB(6)) ); \
t0 = _mm_perm_epi8(t0, m2, _mm_set_epi32(TOB(3),TOB(7),TOB(1),TOB(0)) ); \
buf = _mm_perm_epi8(t0, m3, _mm_set_epi32(TOB(3),TOB(2),TOB(6),TOB(0)) );

#define LOAD_MSG_8_2(buf) \
t1 = _mm_perm_epi8(m0, m2, _mm_set_epi32(TOB(4),TOB(3),TOB(5),TOB(0)) ); \
buf = _mm_perm_epi8(t1, m3, _mm_set_epi32(TOB(3),TOB(2),TOB(1),TOB(7)) );

#define LOAD_MSG_8_3(buf) \
t0 = _mm_perm_epi8(m0, m2, _mm_set_epi32(TOB(6),TOB(1),TOB(0),TOB(0)) ); \
buf = _mm_perm_epi8(t0, m3, _mm_set_epi32(TOB(3),TOB(2),TOB(5),TOB(4)) ); \

#define LOAD_MSG_8_4(buf) \
buf = _mm_perm_epi8(m0, m1, _mm_set_epi32(TOB(5),TOB(4),TOB(7),TOB(2)) );

#define LOAD_MSG_9_1(buf) \
t0 = _mm_perm_epi8(m0, m1, _mm_set_epi32(TOB(1),TOB(7),TOB(0),TOB(0)) ); \
buf = _mm_perm_epi8(t0, m2, _mm_set_epi32(TOB(3),TOB(2),TOB(4),TOB(6)) );

#define LOAD_MSG_9_2(buf) \
buf = _mm_perm_epi8(m0, m1, _mm_set_epi32(TOB(5),TOB(6),TOB(4),TOB(2)) );

#define LOAD_MSG_9_3(buf) \
t0 = _mm_perm_epi8(m0, m2, _mm_set_epi32(TOB(0),TOB(3),TOB(5),TOB(0)) ); \
buf = _mm_perm_epi8(t0, m3, _mm_set_epi32(TOB(5),TOB(2),TOB(1),TOB(7)) );

#define LOAD_MSG_9_4(buf) \
t1 = _mm_perm_epi8(m0, m2, _mm_set_epi32(TOB(0),TOB(0),TOB(0),TOB(7)) ); \
buf = _mm_perm_epi8(t1, m3, _mm_set_epi32(TOB(3),TOB(4),TOB(6),TOB(0)) );

/*==============================================================*/
#elif defined(HAVE_SSE41)
/*==============================================================*/
/* --- blake2s-load-sse41.h */

#define LOAD_MSG_0_1(buf) \
buf = TOI(_mm_shuffle_ps(TOF(m0), TOF(m1), _MM_SHUFFLE(2,0,2,0)));

#define LOAD_MSG_0_2(buf) \
buf = TOI(_mm_shuffle_ps(TOF(m0), TOF(m1), _MM_SHUFFLE(3,1,3,1)));

#define LOAD_MSG_0_3(buf) \
buf = TOI(_mm_shuffle_ps(TOF(m2), TOF(m3), _MM_SHUFFLE(2,0,2,0)));

#define LOAD_MSG_0_4(buf) \
buf = TOI(_mm_shuffle_ps(TOF(m2), TOF(m3), _MM_SHUFFLE(3,1,3,1)));

#define LOAD_MSG_1_1(buf) \
t0 = _mm_blend_epi16(m1, m2, 0x0C); \
t1 = _mm_slli_si128(m3, 4); \
t2 = _mm_blend_epi16(t0, t1, 0xF0); \
buf = _mm_shuffle_epi32(t2, _MM_SHUFFLE(2,1,0,3));

#define LOAD_MSG_1_2(buf) \
t0 = _mm_shuffle_epi32(m2,_MM_SHUFFLE(0,0,2,0)); \
t1 = _mm_blend_epi16(m1,m3,0xC0); \
t2 = _mm_blend_epi16(t0, t1, 0xF0); \
buf = _mm_shuffle_epi32(t2, _MM_SHUFFLE(2,3,0,1));

#define LOAD_MSG_1_3(buf) \
t0 = _mm_slli_si128(m1, 4); \
t1 = _mm_blend_epi16(m2, t0, 0x30); \
t2 = _mm_blend_epi16(m0, t1, 0xF0); \
buf = _mm_shuffle_epi32(t2, _MM_SHUFFLE(2,3,0,1));

#define LOAD_MSG_1_4(buf) \
t0 = _mm_unpackhi_epi32(m0,m1); \
t1 = _mm_slli_si128(m3, 4); \
t2 = _mm_blend_epi16(t0, t1, 0x0C); \
buf = _mm_shuffle_epi32(t2, _MM_SHUFFLE(2,3,0,1));

#define LOAD_MSG_2_1(buf) \
t0 = _mm_unpackhi_epi32(m2,m3); \
t1 = _mm_blend_epi16(m3,m1,0x0C); \
t2 = _mm_blend_epi16(t0, t1, 0x0F); \
buf = _mm_shuffle_epi32(t2, _MM_SHUFFLE(3,1,0,2));

#define LOAD_MSG_2_2(buf) \
t0 = _mm_unpacklo_epi32(m2,m0); \
t1 = _mm_blend_epi16(t0, m0, 0xF0); \
t2 = _mm_slli_si128(m3, 8); \
buf = _mm_blend_epi16(t1, t2, 0xC0);

#define LOAD_MSG_2_3(buf) \
t0 = _mm_blend_epi16(m0, m2, 0x3C); \
t1 = _mm_srli_si128(m1, 12); \
t2 = _mm_blend_epi16(t0,t1,0x03); \
buf = _mm_shuffle_epi32(t2, _MM_SHUFFLE(1,0,3,2));

#define LOAD_MSG_2_4(buf) \
t0 = _mm_slli_si128(m3, 4); \
t1 = _mm_blend_epi16(m0, m1, 0x33); \
t2 = _mm_blend_epi16(t1, t0, 0xC0); \
buf = _mm_shuffle_epi32(t2, _MM_SHUFFLE(0,1,2,3));

#define LOAD_MSG_3_1(buf) \
t0 = _mm_unpackhi_epi32(m0,m1); \
t1 = _mm_unpackhi_epi32(t0, m2); \
t2 = _mm_blend_epi16(t1, m3, 0x0C); \
buf = _mm_shuffle_epi32(t2, _MM_SHUFFLE(3,1,0,2));

#define LOAD_MSG_3_2(buf) \
t0 = _mm_slli_si128(m2, 8); \
t1 = _mm_blend_epi16(m3,m0,0x0C); \
t2 = _mm_blend_epi16(t1, t0, 0xC0); \
buf = _mm_shuffle_epi32(t2, _MM_SHUFFLE(2,0,1,3));

#define LOAD_MSG_3_3(buf) \
t0 = _mm_blend_epi16(m0,m1,0x0F); \
t1 = _mm_blend_epi16(t0, m3, 0xC0); \
buf = _mm_shuffle_epi32(t1, _MM_SHUFFLE(3,0,1,2));

#define LOAD_MSG_3_4(buf) \
t0 = _mm_unpacklo_epi32(m0,m2); \
t1 = _mm_unpackhi_epi32(m1,m2); \
buf = _mm_unpacklo_epi64(t1,t0);

#define LOAD_MSG_4_1(buf) \
t0 = _mm_unpacklo_epi64(m1,m2); \
t1 = _mm_unpackhi_epi64(m0,m2); \
t2 = _mm_blend_epi16(t0,t1,0x33); \
buf = _mm_shuffle_epi32(t2, _MM_SHUFFLE(2,0,1,3));

#define LOAD_MSG_4_2(buf) \
t0 = _mm_unpackhi_epi64(m1,m3); \
t1 = _mm_unpacklo_epi64(m0,m1); \
buf = _mm_blend_epi16(t0,t1,0x33);

#define LOAD_MSG_4_3(buf) \
t0 = _mm_unpackhi_epi64(m3,m1); \
t1 = _mm_unpackhi_epi64(m2,m0); \
buf = _mm_blend_epi16(t1,t0,0x33);

#define LOAD_MSG_4_4(buf) \
t0 = _mm_blend_epi16(m0,m2,0x03); \
t1 = _mm_slli_si128(t0, 8); \
t2 = _mm_blend_epi16(t1,m3,0x0F); \
buf = _mm_shuffle_epi32(t2, _MM_SHUFFLE(1,2,0,3));

#define LOAD_MSG_5_1(buf) \
t0 = _mm_unpackhi_epi32(m0,m1); \
t1 = _mm_unpacklo_epi32(m0,m2); \
buf = _mm_unpacklo_epi64(t0,t1);

#define LOAD_MSG_5_2(buf) \
t0 = _mm_srli_si128(m2, 4); \
t1 = _mm_blend_epi16(m0,m3,0x03); \
buf = _mm_blend_epi16(t1,t0,0x3C);

#define LOAD_MSG_5_3(buf) \
t0 = _mm_blend_epi16(m1,m0,0x0C); \
t1 = _mm_srli_si128(m3, 4); \
t2 = _mm_blend_epi16(t0,t1,0x30); \
buf = _mm_shuffle_epi32(t2, _MM_SHUFFLE(1,2,3,0));

#define LOAD_MSG_5_4(buf) \
t0 = _mm_unpacklo_epi64(m1,m2); \
t1= _mm_shuffle_epi32(m3, _MM_SHUFFLE(0,2,0,1)); \
buf = _mm_blend_epi16(t0,t1,0x33);

#define LOAD_MSG_6_1(buf) \
t0 = _mm_slli_si128(m1, 12); \
t1 = _mm_blend_epi16(m0,m3,0x33); \
buf = _mm_blend_epi16(t1,t0,0xC0);

#define LOAD_MSG_6_2(buf) \
t0 = _mm_blend_epi16(m3,m2,0x30); \
t1 = _mm_srli_si128(m1, 4); \
t2 = _mm_blend_epi16(t0,t1,0x03); \
buf = _mm_shuffle_epi32(t2, _MM_SHUFFLE(2,1,3,0));

#define LOAD_MSG_6_3(buf) \
t0 = _mm_unpacklo_epi64(m0,m2); \
t1 = _mm_srli_si128(m1, 4); \
buf = _mm_shuffle_epi32(_mm_blend_epi16(t0,t1,0x0C), _MM_SHUFFLE(2,3,1,0));

#define LOAD_MSG_6_4(buf) \
t0 = _mm_unpackhi_epi32(m1,m2); \
t1 = _mm_unpackhi_epi64(m0,t0); \
buf = _mm_shuffle_epi32(t1, _MM_SHUFFLE(3,0,1,2));

#define LOAD_MSG_7_1(buf) \
t0 = _mm_unpackhi_epi32(m0,m1); \
t1 = _mm_blend_epi16(t0,m3,0x0F); \
buf = _mm_shuffle_epi32(t1,_MM_SHUFFLE(2,0,3,1));

#define LOAD_MSG_7_2(buf) \
t0 = _mm_blend_epi16(m2,m3,0x30); \
t1 = _mm_srli_si128(m0,4); \
t2 = _mm_blend_epi16(t0,t1,0x03); \
buf = _mm_shuffle_epi32(t2, _MM_SHUFFLE(1,0,2,3));

#define LOAD_MSG_7_3(buf) \
t0 = _mm_unpackhi_epi64(m0,m3); \
t1 = _mm_unpacklo_epi64(m1,m2); \
t2 = _mm_blend_epi16(t0,t1,0x3C); \
buf = _mm_shuffle_epi32(t2,_MM_SHUFFLE(0,2,3,1));

#define LOAD_MSG_7_4(buf) \
t0 = _mm_unpacklo_epi32(m0,m1); \
t1 = _mm_unpackhi_epi32(m1,m2); \
buf = _mm_unpacklo_epi64(t0,t1);

#define LOAD_MSG_8_1(buf) \
t0 = _mm_unpackhi_epi32(m1,m3); \
t1 = _mm_unpacklo_epi64(t0,m0); \
t2 = _mm_blend_epi16(t1,m2,0xC0); \
buf = _mm_shufflehi_epi16(t2,_MM_SHUFFLE(1,0,3,2));

#define LOAD_MSG_8_2(buf) \
t0 = _mm_unpackhi_epi32(m0,m3); \
t1 = _mm_blend_epi16(m2,t0,0xF0); \
buf = _mm_shuffle_epi32(t1,_MM_SHUFFLE(0,2,1,3));

#define LOAD_MSG_8_3(buf) \
t0 = _mm_blend_epi16(m2,m0,0x0C); \
t1 = _mm_slli_si128(t0,4); \
buf = _mm_blend_epi16(t1,m3,0x0F);

#define LOAD_MSG_8_4(buf) \
t0 = _mm_blend_epi16(m1,m0,0x30); \
buf = _mm_shuffle_epi32(t0,_MM_SHUFFLE(1,0,3,2));

#define LOAD_MSG_9_1(buf) \
t0 = _mm_blend_epi16(m0,m2,0x03); \
t1 = _mm_blend_epi16(m1,m2,0x30); \
t2 = _mm_blend_epi16(t1,t0,0x0F); \
buf = _mm_shuffle_epi32(t2,_MM_SHUFFLE(1,3,0,2));

#define LOAD_MSG_9_2(buf) \
t0 = _mm_slli_si128(m0,4); \
t1 = _mm_blend_epi16(m1,t0,0xC0); \
buf = _mm_shuffle_epi32(t1,_MM_SHUFFLE(1,2,0,3));

#define LOAD_MSG_9_3(buf) \
t0 = _mm_unpackhi_epi32(m0,m3); \
t1 = _mm_unpacklo_epi32(m2,m3); \
t2 = _mm_unpackhi_epi64(t0,t1); \
buf = _mm_shuffle_epi32(t2,_MM_SHUFFLE(3,0,2,1));

#define LOAD_MSG_9_4(buf) \
t0 = _mm_blend_epi16(m3,m2,0xC0); \
t1 = _mm_unpacklo_epi32(m0,m3); \
t2 = _mm_blend_epi16(t0,t1,0x0F); \
buf = _mm_shuffle_epi32(t2,_MM_SHUFFLE(0,1,2,3));

/*==============================================================*/
#else	/* defined(HAVE_SSE41) */
/*==============================================================*/
/* --- blake2s-load-sse2.h */

#define LOAD_MSG_0_1(buf) buf = _mm_set_epi32(m6,m4,m2,m0)
#define LOAD_MSG_0_2(buf) buf = _mm_set_epi32(m7,m5,m3,m1)
#define LOAD_MSG_0_3(buf) buf = _mm_set_epi32(m14,m12,m10,m8)
#define LOAD_MSG_0_4(buf) buf = _mm_set_epi32(m15,m13,m11,m9)
#define LOAD_MSG_1_1(buf) buf = _mm_set_epi32(m13,m9,m4,m14)
#define LOAD_MSG_1_2(buf) buf = _mm_set_epi32(m6,m15,m8,m10)
#define LOAD_MSG_1_3(buf) buf = _mm_set_epi32(m5,m11,m0,m1)
#define LOAD_MSG_1_4(buf) buf = _mm_set_epi32(m3,m7,m2,m12)
#define LOAD_MSG_2_1(buf) buf = _mm_set_epi32(m15,m5,m12,m11)
#define LOAD_MSG_2_2(buf) buf = _mm_set_epi32(m13,m2,m0,m8)
#define LOAD_MSG_2_3(buf) buf = _mm_set_epi32(m9,m7,m3,m10)
#define LOAD_MSG_2_4(buf) buf = _mm_set_epi32(m4,m1,m6,m14)
#define LOAD_MSG_3_1(buf) buf = _mm_set_epi32(m11,m13,m3,m7)
#define LOAD_MSG_3_2(buf) buf = _mm_set_epi32(m14,m12,m1,m9)
#define LOAD_MSG_3_3(buf) buf = _mm_set_epi32(m15,m4,m5,m2)
#define LOAD_MSG_3_4(buf) buf = _mm_set_epi32(m8,m0,m10,m6)
#define LOAD_MSG_4_1(buf) buf = _mm_set_epi32(m10,m2,m5,m9)
#define LOAD_MSG_4_2(buf) buf = _mm_set_epi32(m15,m4,m7,m0)
#define LOAD_MSG_4_3(buf) buf = _mm_set_epi32(m3,m6,m11,m14)
#define LOAD_MSG_4_4(buf) buf = _mm_set_epi32(m13,m8,m12,m1)
#define LOAD_MSG_5_1(buf) buf = _mm_set_epi32(m8,m0,m6,m2)
#define LOAD_MSG_5_2(buf) buf = _mm_set_epi32(m3,m11,m10,m12)
#define LOAD_MSG_5_3(buf) buf = _mm_set_epi32(m1,m15,m7,m4)
#define LOAD_MSG_5_4(buf) buf = _mm_set_epi32(m9,m14,m5,m13)
#define LOAD_MSG_6_1(buf) buf = _mm_set_epi32(m4,m14,m1,m12)
#define LOAD_MSG_6_2(buf) buf = _mm_set_epi32(m10,m13,m15,m5)
#define LOAD_MSG_6_3(buf) buf = _mm_set_epi32(m8,m9,m6,m0)
#define LOAD_MSG_6_4(buf) buf = _mm_set_epi32(m11,m2,m3,m7)
#define LOAD_MSG_7_1(buf) buf = _mm_set_epi32(m3,m12,m7,m13)
#define LOAD_MSG_7_2(buf) buf = _mm_set_epi32(m9,m1,m14,m11)
#define LOAD_MSG_7_3(buf) buf = _mm_set_epi32(m2,m8,m15,m5)
#define LOAD_MSG_7_4(buf) buf = _mm_set_epi32(m10,m6,m4,m0)
#define LOAD_MSG_8_1(buf) buf = _mm_set_epi32(m0,m11,m14,m6)
#define LOAD_MSG_8_2(buf) buf = _mm_set_epi32(m8,m3,m9,m15)
#define LOAD_MSG_8_3(buf) buf = _mm_set_epi32(m10,m1,m13,m12)
#define LOAD_MSG_8_4(buf) buf = _mm_set_epi32(m5,m4,m7,m2)
#define LOAD_MSG_9_1(buf) buf = _mm_set_epi32(m1,m7,m8,m10)
#define LOAD_MSG_9_2(buf) buf = _mm_set_epi32(m5,m6,m4,m2)
#define LOAD_MSG_9_3(buf) buf = _mm_set_epi32(m13,m3,m9,m15)
#define LOAD_MSG_9_4(buf) buf = _mm_set_epi32(m0,m12,m14,m11)

/*==============================================================*/
#endif	/* defined(HAVE_SSE41) */

#define ROUND(r)  \
  LOAD_MSG_ ##r ##_1(buf1); \
  G1(row1,row2,row3,row4,buf1); \
  LOAD_MSG_ ##r ##_2(buf2); \
  G2(row1,row2,row3,row4,buf2); \
  DIAGONALIZE(row1,row2,row3,row4); \
  LOAD_MSG_ ##r ##_3(buf3); \
  G1(row1,row2,row3,row4,buf3); \
  LOAD_MSG_ ##r ##_4(buf4); \
  G2(row1,row2,row3,row4,buf4); \
  UNDIAGONALIZE(row1,row2,row3,row4); \

/*==============================================================*/
#else	/* XXXSSE */
/*==============================================================*/
static const uint8_t blake2s_sigma[10][16] = {
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
};

#define G(r,i,a,b,c,d) \
  do { \
    a = a + b + m[blake2s_sigma[r][2*i+0]]; \
    d = rotr32(d ^ a, 16); \
    c = c + d; \
    b = rotr32(b ^ c, 12); \
    a = a + b + m[blake2s_sigma[r][2*i+1]]; \
    d = rotr32(d ^ a, 8); \
    c = c + d; \
    b = rotr32(b ^ c, 7); \
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

static const uint32_t blake2s_IV[8] = {
    0x6A09E667UL, 0xBB67AE85UL, 0x3C6EF372UL, 0xA54FF53AUL,
    0x510E527FUL, 0x9B05688CUL, 0x1F83D9ABUL, 0x5BE0CD19UL
};

static inline int blake2s_set_lastnode(blake2s_state * S)
{
    S->f[1] = ~0U;
    return 0;
}

static inline int blake2s_clear_lastnode(blake2s_state * S)
{
    S->f[1] = 0U;
    return 0;
}

/* Some helper functions, not necessarily useful */
static inline int blake2s_set_lastblock(blake2s_state * S)
{
    if (S->last_node)
	blake2s_set_lastnode(S);

    S->f[0] = ~0U;
    return 0;
}

static inline int blake2s_clear_lastblock(blake2s_state * S)
{
    if (S->last_node)
	blake2s_clear_lastnode(S);

    S->f[0] = 0U;
    return 0;
}

static inline int blake2s_increment_counter(blake2s_state * S,
					    const uint32_t inc)
{
#ifdef	XXXSSE
    uint64_t t = ((uint64_t) S->t[1] << 32) | S->t[0];
    t += inc;
    S->t[0] = (uint32_t) (t >> 0);
    S->t[1] = (uint32_t) (t >> 32);
#else	/* XXXSSE */
    S->t[0] += inc;
    S->t[1] += (S->t[0] < inc);
#endif	/* XXXSSE */
    return 0;
}

// Parameter-related functions
static inline int blake2s_param_set_digest_length(blake2s_param * P,
						  const uint8_t
						  digest_length)
{
    P->digest_length = digest_length;
    return 0;
}

static inline int blake2s_param_set_fanout(blake2s_param * P,
					   const uint8_t fanout)
{
    P->fanout = fanout;
    return 0;
}

static inline int blake2s_param_set_max_depth(blake2s_param * P,
					      const uint8_t depth)
{
    P->depth = depth;
    return 0;
}

static inline int blake2s_param_set_leaf_length(blake2s_param * P,
						const uint32_t leaf_length)
{
    store32(&P->leaf_length, leaf_length);
    return 0;
}

static inline int blake2s_param_set_node_offset(blake2s_param * P,
						const uint64_t node_offset)
{
    store48(P->node_offset, node_offset);
    return 0;
}

static inline int blake2s_param_set_node_depth(blake2s_param * P,
					       const uint8_t node_depth)
{
    P->node_depth = node_depth;
    return 0;
}

static inline int blake2s_param_set_inner_length(blake2s_param * P,
						 const uint8_t
						 inner_length)
{
    P->inner_length = inner_length;
    return 0;
}

static inline int blake2s_param_set_salt(blake2s_param * P,
					 const uint8_t
					 salt[BLAKE2S_SALTBYTES])
{
    memcpy(P->salt, salt, BLAKE2S_SALTBYTES);
    return 0;
}

static inline int blake2s_param_set_personal(blake2s_param * P,
					     const uint8_t
					     personal
					     [BLAKE2S_PERSONALBYTES])
{
    memcpy(P->personal, personal, BLAKE2S_PERSONALBYTES);
    return 0;
}

static inline int blake2s_init0(blake2s_state * S)
{
    int i;

    memset(S, 0, sizeof(blake2s_state));

    for (i = 0; i < 8; ++i)
	S->h[i] = blake2s_IV[i];

    return 0;
}

/* init2 xors IV with input parameter block */
int blake2s_init_param(blake2s_state * S, const blake2s_param * P)
{
    size_t i;
#ifdef	XXXSSE
    uint8_t *p, *h, *v;
    //blake2s_init0( S );
    v = (uint8_t *) (blake2s_IV);
    h = (uint8_t *) (S->h);
    p = (uint8_t *) (P);
    /* IV XOR ParamBlock */
    memset(S, 0, sizeof(blake2s_state));

    for (i = 0; i < BLAKE2S_OUTBYTES; ++i)
	h[i] = v[i] ^ p[i];
#else	/* XXXSSE */
    blake2s_init0(S);
    uint32_t *p = (uint32_t *) (P);

    /* IV XOR ParamBlock */
    for (i = 0; i < 8; ++i)
	S->h[i] ^= load32(&p[i]);
#endif	/* XXXSSE */

    return 0;
}


// Sequential blake2s initialization
int blake2s_init(blake2s_state * S, const uint8_t outlen)
{
    blake2s_param P[1];

    /* Move interval verification here? */
    if (outlen == 0 || outlen > BLAKE2S_OUTBYTES)
	return -1;

#ifdef	XXXSSE
    {	const blake2s_param _P = {
	    .digest_length	= outlen,
	    .key_length		= 0,
	    .fanout		= 1,
	    .depth 		= 1,
	    .leaf_length	= 0,
	    .node_offset	= {0},
	    .node_depth		= 0,
	    .inner_length	= 0,
	    .salt		= {0},
	    .personal		= {0}
	};
	P[0] = *(blake2s_param *)&_P;	/* structure assignment */
    }
#else	/* XXXSSE */
    P->digest_length = outlen;
    P->key_length = 0;
    P->fanout = 1;
    P->depth = 1;
    store32(&P->leaf_length, 0);
    store48(&P->node_offset, 0);
    P->node_depth = 0;
    P->inner_length = 0;
    // memset(P->reserved, 0, sizeof(P->reserved) );
    memset(P->salt, 0, sizeof(P->salt));
    memset(P->personal, 0, sizeof(P->personal));
#endif	/* XXXSSE */

    return blake2s_init_param(S, P);
}

int blake2s_init_key(blake2s_state * S, const uint8_t outlen,
		     const void *key, const uint8_t keylen)
{
    /* Move interval verification here? */
    blake2s_param P[1];

    if (outlen == 0 || outlen > BLAKE2S_OUTBYTES)
	return -1;

    if (!key || !keylen || keylen > BLAKE2S_KEYBYTES)
	return -1;

#ifdef	XXXSSE
    {	const blake2s_param _P = {
	    .digest_length	= outlen,
	    .key_length		= keylen,
	    .fanout		= 1,
	    .depth 		= 1,
	    .leaf_length	= 0,
	    .node_offset	= {0},
	    .node_depth		= 0,
	    .inner_length	= 0,
	    .salt		= {0},
	    .personal		= {0}
	};
	P[0] = *(blake2s_param *)&_P;	/* structure assignment */
    }
#else	/* XXXSSE */
    P->digest_length = outlen;
    P->key_length = keylen;
    P->fanout = 1;
    P->depth = 1;
    store32(&P->leaf_length, 0);
    store48(&P->node_offset, 0);
    P->node_depth = 0;
    P->inner_length = 0;
    // memset(P->reserved, 0, sizeof(P->reserved) );
    memset(P->salt, 0, sizeof(P->salt));
    memset(P->personal, 0, sizeof(P->personal));
#endif	/* XXXSSE */

    if (blake2s_init_param(S, P) < 0)
	return -1;

    {
	uint8_t block[BLAKE2S_BLOCKBYTES];
	memset(block, 0, BLAKE2S_BLOCKBYTES);
	memcpy(block, key, keylen);
	blake2s_update(S, block, BLAKE2S_BLOCKBYTES);
	secure_zero_memory(block, BLAKE2S_BLOCKBYTES);	/* Burn the key from stack */
    }
    return 0;
}

static inline int blake2s_compress(blake2s_state * S,
			    const uint8_t block[BLAKE2S_BLOCKBYTES])
{
#ifdef	XXXSSE
    __m128i row1, row2, row3, row4;
    __m128i buf1, buf2, buf3, buf4;
#if defined(HAVE_SSE41)
    __m128i t0, t1;
#if !defined(HAVE_XOP)
    __m128i t2;
#endif
#endif
    __m128i ff0, ff1;
#if defined(HAVE_SSSE3) && !defined(HAVE_XOP)
    const __m128i r8 =
	_mm_set_epi8(12, 15, 14, 13, 8, 11, 10, 9, 4, 7, 6, 5, 0, 3, 2, 1);
    const __m128i r16 =
	_mm_set_epi8(13, 12, 15, 14, 9, 8, 11, 10, 5, 4, 7, 6, 1, 0, 3, 2);
#endif
#if defined(HAVE_SSE41)
    const __m128i m0 = LOADU(block + 00);
    const __m128i m1 = LOADU(block + 16);
    const __m128i m2 = LOADU(block + 32);
    const __m128i m3 = LOADU(block + 48);
#else
    const uint32_t m0 = ((uint32_t *) block)[0];
    const uint32_t m1 = ((uint32_t *) block)[1];
    const uint32_t m2 = ((uint32_t *) block)[2];
    const uint32_t m3 = ((uint32_t *) block)[3];
    const uint32_t m4 = ((uint32_t *) block)[4];
    const uint32_t m5 = ((uint32_t *) block)[5];
    const uint32_t m6 = ((uint32_t *) block)[6];
    const uint32_t m7 = ((uint32_t *) block)[7];
    const uint32_t m8 = ((uint32_t *) block)[8];
    const uint32_t m9 = ((uint32_t *) block)[9];
    const uint32_t m10 = ((uint32_t *) block)[10];
    const uint32_t m11 = ((uint32_t *) block)[11];
    const uint32_t m12 = ((uint32_t *) block)[12];
    const uint32_t m13 = ((uint32_t *) block)[13];
    const uint32_t m14 = ((uint32_t *) block)[14];
    const uint32_t m15 = ((uint32_t *) block)[15];
#endif
    row1 = ff0 = LOADU(&S->h[0]);
    row2 = ff1 = LOADU(&S->h[4]);
    row3 = _mm_setr_epi32(0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A);
    row4 =
	_mm_xor_si128(_mm_setr_epi32
		      (0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19),
		      LOADU(&S->t[0]));

#else	/* XXXSSE */

    uint32_t m[16];
    uint32_t v[16];
    size_t i;

    for (i = 0; i < 16; ++i)
	m[i] = load32(block + i * sizeof(m[i]));

    for (i = 0; i < 8; ++i)
	v[i] = S->h[i];

    v[8] = blake2s_IV[0];
    v[9] = blake2s_IV[1];
    v[10] = blake2s_IV[2];
    v[11] = blake2s_IV[3];
    v[12] = S->t[0] ^ blake2s_IV[4];
    v[13] = S->t[1] ^ blake2s_IV[5];
    v[14] = S->f[0] ^ blake2s_IV[6];
    v[15] = S->f[1] ^ blake2s_IV[7];
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

#ifdef XXXSSE
  STOREU( &S->h[0], _mm_xor_si128( ff0, _mm_xor_si128( row1, row3 ) ) );
  STOREU( &S->h[4], _mm_xor_si128( ff1, _mm_xor_si128( row2, row4 ) ) );
#else
    for (i = 0; i < 8; ++i)
	S->h[i] = S->h[i] ^ v[i] ^ v[i + 8];
#endif

    return 0;
}


int blake2s_update(blake2s_state * S, const uint8_t * in, uint64_t inlen)
{
    while (inlen > 0) {
	size_t left = S->buflen;
	size_t fill = 2 * BLAKE2S_BLOCKBYTES - left;

	if (inlen > fill) {
	    memcpy(S->buf + left, in, fill);	// Fill buffer
	    S->buflen += fill;
	    blake2s_increment_counter(S, BLAKE2S_BLOCKBYTES);
	    blake2s_compress(S, S->buf);	// Compress
	    memcpy(S->buf, S->buf + BLAKE2S_BLOCKBYTES, BLAKE2S_BLOCKBYTES);	// Shift buffer left
	    S->buflen -= BLAKE2S_BLOCKBYTES;
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

int blake2s_final(blake2s_state * S, uint8_t * out, uint8_t outlen)
{
    uint8_t buffer[BLAKE2S_OUTBYTES];
    int i;

    if (S->buflen > BLAKE2S_BLOCKBYTES) {
	blake2s_increment_counter(S, BLAKE2S_BLOCKBYTES);
	blake2s_compress(S, S->buf);
	S->buflen -= BLAKE2S_BLOCKBYTES;
	memcpy(S->buf, S->buf + BLAKE2S_BLOCKBYTES, S->buflen);
    }

    blake2s_increment_counter(S, (uint32_t) S->buflen);
    blake2s_set_lastblock(S);
    memset(S->buf + S->buflen, 0, 2 * BLAKE2S_BLOCKBYTES - S->buflen);	/* Padding */
    blake2s_compress(S, S->buf);

    for (i = 0; i < 8; ++i)	/* Output full hash to temp buffer */
	store32(buffer + sizeof(S->h[i]) * i, S->h[i]);

    memcpy(out, buffer, outlen);
    return 0;
}

/* inlen, at least, should be uint64_t. Others can be size_t. */
int blake2s(uint8_t * out, const void *in, const void *key,
	    const uint8_t outlen, const uint64_t inlen, uint8_t keylen)
{
    blake2s_state S[1];

    /* Verify parameters */
    if (NULL == in)
	return -1;

    if (NULL == out)
	return -1;

    if (NULL == key)
	keylen = 0;		/* Fail here instead if keylen != 0 and key == NULL? */

    if (keylen > 0) {
	if (blake2s_init_key(S, outlen, key, keylen) < 0)
	    return -1;
    } else {
	if (blake2s_init(S, outlen) < 0)
	    return -1;
    }

    blake2s_update(S, (uint8_t *) in, inlen);
    blake2s_final(S, out, outlen);
    return 0;
}

#if defined(SUPERCOP)
int crypto_hash(unsigned char *out, unsigned char *in,
		unsigned long long inlen)
{
    return blake2s(out, in, NULL, BLAKE2S_OUTBYTES, inlen, 0);
}
#endif

#if defined(BLAKE2S_SELFTEST)
#include "blake2-kat.h"
int main(int argc, char **argv)
{
    uint8_t key[BLAKE2S_KEYBYTES];
    uint8_t buf[KAT_LENGTH];
    size_t i;

    for (i = 0; i < BLAKE2S_KEYBYTES; ++i)
	key[i] = (uint8_t) i;

    for (i = 0; i < KAT_LENGTH; ++i)
	buf[i] = (uint8_t) i;

    for (i = 0; i < KAT_LENGTH; ++i) {
	uint8_t hash[BLAKE2S_OUTBYTES];

	if (blake2s(hash, buf, key, BLAKE2S_OUTBYTES, i, BLAKE2S_KEYBYTES) < 0
	 || memcmp(hash, blake2s_keyed_kat[i], BLAKE2S_OUTBYTES)) {
	    puts("error");
	    return -1;
	}
    }

    puts("ok");
    return 0;
}
#endif

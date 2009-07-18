#define CUBEHASH_ROUNDS 8
#define CUBEHASH_BLOCKBYTES 1

#include "cubehash.h"

#if defined(OPTIMIZE_SSE2)

static void transform(hashState *state)
{
  int r;
  __m128i x0;
  __m128i x1;
  __m128i x2;
  __m128i x3;
  __m128i x4;
  __m128i x5;
  __m128i x6;
  __m128i x7;
  __m128i y0;
  __m128i y1;
  __m128i y2;
  __m128i y3;
  __m128i y4;
  __m128i y5;
  __m128i y6;
  __m128i y7;

  x0 = _mm_load_si128(0 + state->x);
  x1 = _mm_load_si128(1 + state->x);
  x2 = _mm_load_si128(2 + state->x);
  x3 = _mm_load_si128(3 + state->x);
  x4 = _mm_load_si128(4 + state->x);
  x5 = _mm_load_si128(5 + state->x);
  x6 = _mm_load_si128(6 + state->x);
  x7 = _mm_load_si128(7 + state->x);

  for (r = 0;r < state->rounds;++r) {
    x4 = _mm_add_epi32(x0,x4);
    x5 = _mm_add_epi32(x1,x5);
    x6 = _mm_add_epi32(x2,x6);
    x7 = _mm_add_epi32(x3,x7);
    y0 = x2;
    y1 = x3;
    y2 = x0;
    y3 = x1;
    x0 = _mm_xor_si128(_mm_slli_epi32(y0,7),_mm_srli_epi32(y0,25));
    x1 = _mm_xor_si128(_mm_slli_epi32(y1,7),_mm_srli_epi32(y1,25));
    x2 = _mm_xor_si128(_mm_slli_epi32(y2,7),_mm_srli_epi32(y2,25));
    x3 = _mm_xor_si128(_mm_slli_epi32(y3,7),_mm_srli_epi32(y3,25));
    x0 = _mm_xor_si128(x0,x4);
    x1 = _mm_xor_si128(x1,x5);
    x2 = _mm_xor_si128(x2,x6);
    x3 = _mm_xor_si128(x3,x7);
    x4 = _mm_shuffle_epi32(x4,0x4e);
    x5 = _mm_shuffle_epi32(x5,0x4e);
    x6 = _mm_shuffle_epi32(x6,0x4e);
    x7 = _mm_shuffle_epi32(x7,0x4e);
    x4 = _mm_add_epi32(x0,x4);
    x5 = _mm_add_epi32(x1,x5);
    x6 = _mm_add_epi32(x2,x6);
    x7 = _mm_add_epi32(x3,x7);
    y0 = x1;
    y1 = x0;
    y2 = x3;
    y3 = x2;
    x0 = _mm_xor_si128(_mm_slli_epi32(y0,11),_mm_srli_epi32(y0,21));
    x1 = _mm_xor_si128(_mm_slli_epi32(y1,11),_mm_srli_epi32(y1,21));
    x2 = _mm_xor_si128(_mm_slli_epi32(y2,11),_mm_srli_epi32(y2,21));
    x3 = _mm_xor_si128(_mm_slli_epi32(y3,11),_mm_srli_epi32(y3,21));
    x0 = _mm_xor_si128(x0,x4);
    x1 = _mm_xor_si128(x1,x5);
    x2 = _mm_xor_si128(x2,x6);
    x3 = _mm_xor_si128(x3,x7);
    x4 = _mm_shuffle_epi32(x4,0xb1);
    x5 = _mm_shuffle_epi32(x5,0xb1);
    x6 = _mm_shuffle_epi32(x6,0xb1);
    x7 = _mm_shuffle_epi32(x7,0xb1);
  }

  _mm_store_si128(0 + state->x,x0);
  _mm_store_si128(1 + state->x,x1);
  _mm_store_si128(2 + state->x,x2);
  _mm_store_si128(3 + state->x,x3);
  _mm_store_si128(4 + state->x,x4);
  _mm_store_si128(5 + state->x,x5);
  _mm_store_si128(6 + state->x,x6);
  _mm_store_si128(7 + state->x,x7);
}

#else	/* OPTIMIZE_SSE2 */

#define ROTATE(a,b) (((a) << (b)) | ((a) >> (32 - b)))

static void transform(hashState *state)
{
  int i;
  int r;
  myuint32 y[16];

  for (r = 0;r < state->rounds;++r) {
    for (i = 0;i < 16;++i) state->x[i + 16] += state->x[i];
    for (i = 0;i < 16;++i) y[i ^ 8] = state->x[i];
    for (i = 0;i < 16;++i) state->x[i] = ROTATE(y[i],7);
    for (i = 0;i < 16;++i) state->x[i] ^= state->x[i + 16];
    for (i = 0;i < 16;++i) y[i ^ 2] = state->x[i + 16];
    for (i = 0;i < 16;++i) state->x[i + 16] = y[i];
    for (i = 0;i < 16;++i) state->x[i + 16] += state->x[i];
    for (i = 0;i < 16;++i) y[i ^ 4] = state->x[i];
    for (i = 0;i < 16;++i) state->x[i] = ROTATE(y[i],11);
    for (i = 0;i < 16;++i) state->x[i] ^= state->x[i + 16];
    for (i = 0;i < 16;++i) y[i ^ 1] = state->x[i + 16];
    for (i = 0;i < 16;++i) state->x[i + 16] = y[i];
  }
}
#endif	/* OPTIMIZE_SSE2 */

HashReturn Init(hashState *state, int hashbitlen, int rounds, int blockbytes)
{
  int i;

  if (hashbitlen < 8) return BAD_HASHBITLEN;
  if (hashbitlen > 512) return BAD_HASHBITLEN;
  if (hashbitlen != 8 * (hashbitlen / 8)) return BAD_HASHBITLEN;

  /* Sanity checks */
  if (rounds <= 0 || rounds > 32) rounds = CUBEHASH_ROUNDS;
  if (blockbytes <= 0 || blockbytes >= 256) blockbytes = CUBEHASH_BLOCKBYTES;

  state->hashbitlen = hashbitlen;
  state->rounds = rounds;
  state->blockbytes = blockbytes;
#if defined(OPTIMIZE_SSE2)
  for (i = 0;i < 8;++i) state->x[i] = _mm_set_epi32(0,0,0,0);
  state->x[0] = _mm_set_epi32(0,state->rounds,state->blockbytes,hashbitlen / 8);
#else
  for (i = 0;i < 32;++i) state->x[i] = 0;
  state->x[0] = hashbitlen / 8;
  state->x[1] = state->blockbytes;
  state->x[2] = state->rounds;
#endif
  for (i = 0;i < 10;++i) transform(state);
  state->pos = 0;
  return SUCCESS;
}

HashReturn Update(hashState *state, const BitSequence *data,
                  DataLength databitlen)
{
  /* caller promises us that previous data had integral number of bytes */
  /* so state->pos is a multiple of 8 */

  while (databitlen >= 8) {
#if defined(OPTIMIZE_SSE2)
    ((unsigned char *) state->x)[state->pos / 8] ^= *data;
#else
    myuint32 u = *data;
    u <<= 8 * ((state->pos / 8) % 4);
    state->x[state->pos / 32] ^= u;
#endif
    data += 1;
    databitlen -= 8;
    state->pos += 8;
    if (state->pos == 8 * state->blockbytes) {
      transform(state);
      state->pos = 0;
    }
  }
  if (databitlen > 0) {
#if defined(OPTIMIZE_SSE2)
    ((unsigned char *) state->x)[state->pos / 8] ^= *data;
#else
    myuint32 u = *data;
    u <<= 8 * ((state->pos / 8) % 4);
    state->x[state->pos / 32] ^= u;
#endif
    state->pos += databitlen;
  }
  return SUCCESS;
}

HashReturn Final(hashState *state, BitSequence *hashval)
{
  int i;

#if defined(OPTIMIZE_SSE2)
  ((unsigned char *) state->x)[state->pos / 8] ^= (128 >> (state->pos % 8));
  transform(state);
  state->x[7] = _mm_xor_si128(state->x[7],_mm_set_epi32(1,0,0,0));
  for (i = 0;i < 10;++i) transform(state);
  for (i = 0;i < state->hashbitlen / 8;++i)
    hashval[i] = ((unsigned char *) state->x)[i];
#else
  myuint32 u;

  u = (128 >> (state->pos % 8));
  u <<= 8 * ((state->pos / 8) % 4);
  state->x[state->pos / 32] ^= u;
  transform(state);
  state->x[31] ^= 1;
  for (i = 0;i < 10;++i) transform(state);
  for (i = 0;i < state->hashbitlen / 8;++i)
    hashval[i] = state->x[i / 4] >> (8 * (i % 4));
#endif

  return SUCCESS;
}

HashReturn Hash(int hashbitlen, const BitSequence *data,
                DataLength databitlen, BitSequence *hashval)
{
  hashState state;
  if (Init(&state,hashbitlen,0,0) != SUCCESS) return BAD_HASHBITLEN;
  Update(&state,data,databitlen);
  return Final(&state,hashval);
}

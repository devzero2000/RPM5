/* chi.c: CHI hash main implementation */

/*
THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE AND AGAINST
INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "chi.h"

/*
 * Error code for external Interface
 */
enum {
    SUCCESS       = 0,
    FAIL          = 1,
    BAD_HASHLEN   = 2,
    BAD_STATE     = 3,
    BAD_DATA      = 4,
    TOO_MUCH_DATA = 5,
    BAD_HASHVALPTR
};
/*
 * BAD_STATE            State either NULL or corrupted.
 * BAD_DATA             Either NULL passed for data or Update called with
 *                      a partial byte, then called again with more data.
 * TOO_MUCH_DATA        Very unlikely case that too much data was passed to
 *                      hash function.
 * BAD_HASHVALPTR       Pointer for returning hash value invalid.
 */

typedef uint32_t WORD32;
typedef uint64_t WORD64;

/*
 * Function inlining
 */
#if !defined(USE_INLINE) && !defined(DONT_USE_INLINE)
#   define USE_INLINE
#endif

#ifdef USE_INLINE
#   if defined(_WIN32) || defined(_WIN64)
#       define INLINE __inline
#   else
#       define INLINE inline
#   endif
#else
#   define INLINE
#endif

/*
 * Fundamental algorithm constants.
 */
#define _256_STEPS      20      /* Steps in 256 bit hash.                     */
#define _512_STEPS      40      /* Steps in 512 bit hash.                     */
#define _256_MSG_N      (_256_MSG_BLK_LEN / 64)
                                /* The length in 64 bit ints of the message
                                 * block in bits for the 256 bit hash.
                                 */
#define _512_MSG_N      (_512_MSG_BLK_LEN / 64)
                                /* The length in 64 bit ints of the message
                                 * block in bits for the 256 bit hash.
                                 */

/*
 * Valid message block lengths.
 * */
typedef enum {
    _256_MSG_BLK_LEN = 512,
    _512_MSG_BLK_LEN = 1024
} MsgBlkLen;
/*
 * _256_MSG_BLK_LEN     The length in bits of the message block for the 256
 *                      bit hash.
 * _512_MSG_BLK_LEN     The length in bits of the message block for the 512
 *                      bit hash.
 */

/*
 * Valid hash bit lengths.
 */
typedef enum {
    _224_HASH_BIT_LEN = 224,
    _256_HASH_BIT_LEN = 256,
    _384_HASH_BIT_LEN = 384,
    _512_HASH_BIT_LEN = 512
} HashBitLen;

/*
 * Standard MIN macro
 */
#define MIN(x, y)  ((x) < (y) ? (x) : (y))

/*
 * Step constants
 */
static const WORD64
K[] = {
    0x9B05688C2B3E6C1FULL, 0xDBD99E6FF3C90BDCULL,
    0x4DBC64712A5BB168ULL, 0x767E27C3CF76C8E7ULL,
    0x21EE9AC5EF4C823AULL, 0xB36CCFC1204A9BD8ULL,
    0x754C8A7FB36BD941ULL, 0xCF20868F04A825E9ULL,
    0xABB379E0A8838BB0ULL, 0x12D8B70A5959E391ULL,
    0xB59168FA9F69E181ULL, 0x15C7D4918739F18FULL,
    0x4B547673EA8D68E0ULL, 0x3CED7326FCD0EF81ULL,
    0x09F1D77309998460ULL, 0x1AF3937415E91F32ULL,
    0x1F83D9ABFB41BD6BULL, 0x23C4654C2A217583ULL,
    0x2842012131573F2AULL, 0xA59916ACA3991FCAULL,
    0xEC1B7C06FD19D256ULL, 0x1EC785BCDC8BAF26ULL,
    0x69CA4E0FF2E6BDD8ULL, 0xCA2575EE6C950D0EULL,
    0x5BCF66D2FB3D99F6ULL, 0x9D6D08C7BBCA18F3ULL,
    0xEEF64039F2175E83ULL, 0x00ED5AEBAA2AB6E0ULL,
    0x5040712FC29AD308ULL, 0x6DAFE433438D2C43ULL,
    0xBD7FAA3F06C71F15ULL, 0x03B5AA8CE9B6A4DDULL,
    0x5BE0CD19137E2179ULL, 0x867F5E3B72221265ULL,
    0x43B6CBE0D67F4A20ULL, 0xDB99D767CB0E4933ULL,
    0xDC450DBC469248BDULL, 0xFE1E5E4876100D6FULL,
    0xB799D29EA1733137ULL, 0x16EA7ABCF92053C4ULL,
    0xBE3ECE968DBA92ACULL, 0x18538F84D82C318BULL,
    0x38D79F4E9C8A18C0ULL, 0xA8BBC28F1271F1F7ULL,
    0x2796E71067D2C8CCULL, 0xDE1BF2334EDB3FF6ULL,
    0xB094D782A857F9C1ULL, 0x639B484B0C1DAED1ULL,
    0xCBBB9D5DC1059ED8ULL, 0xE7730EAFF25E24A3ULL,
    0xF367F2FC266A0373ULL, 0xFE7A4D34486D08AEULL,
    0xD41670A136851F32ULL, 0x663914B66B4B3C23ULL,
    0x1B9E3D7740A60887ULL, 0x63C11D86D446CB1CULL,
    0xD167D2469049D628ULL, 0xDDDBB606B9A49E38ULL,
    0x557F1C8BEE68A7F7ULL, 0xF99DC58B50F924BDULL,
    0x205ACC9F653512A5ULL, 0x67C66344E4BAB193ULL,
    0x18026E467960D0C8ULL, 0xA2F5D84DAECA8980ULL,
    0x629A292A367CD507ULL, 0x98E67012D90CBB6DULL,
    0xEED758D1D18C7E35ULL, 0x031C02E4437DC71EULL,
    0x79B63D6482198EB7ULL, 0x936A9D7E8C9E4B33ULL,
    0xB30CA682C3E6C65DULL, 0xCC442382BA4262FAULL,
    0x51179BA5A1D37FF6ULL, 0x7202BDE7A98EEA51ULL,
    0x2B9F65D1DF9C610FULL, 0xF56B742B0AF1CE83ULL,
    0xF9989D199B75848BULL, 0xD142F19D8B46D578ULL,
    0x7A7580514D75EA33ULL, 0xB74F9690808E704DULL
};

/*
 * Initial state values for different bit lengths
 */
static const WORD64
_224_init[] = {
    0xA54FF53A5F1D36F1ULL, 0xCEA7E61FC37A20D5ULL,
    0x4A77FE7B78415DFCULL, 0x8E34A6FE8E2DF92AULL,
    0x4E5B408C9C97D4D8ULL, 0x24A05EEE29922401ULL
};

static const WORD64
_256_init[] = {
    0x510E527FADE682D1ULL, 0xDE49E330E42B4CBBULL,
    0x29BA5A455316E0C6ULL, 0x5507CD18E9E51E69ULL,
    0x4F9B11C81009A030ULL, 0xE3D3775F155385C6ULL
};

static const WORD64
_384_init[] = {
    0xA54FF53A5F1D36F1ULL, 0xCEA7E61FC37A20D5ULL,
    0x4A77FE7B78415DFCULL, 0x8E34A6FE8E2DF92AULL,
    0x4E5B408C9C97D4D8ULL, 0x24A05EEE29922401ULL,
    0x5A8176CFFC7C2224ULL, 0xC3EDEBDA29BEC4C8ULL,
    0x8A074C0F4D999610ULL
};

static const WORD64
_512_init[] = {
    0x510E527FADE682D1ULL, 0xDE49E330E42B4CBBULL,
    0x29BA5A455316E0C6ULL, 0x5507CD18E9E51E69ULL,
    0x4F9B11C81009A030ULL, 0xE3D3775F155385C6ULL,
    0x489221632788FB30ULL, 0x41921DB8FEEB38C2ULL,
    0x9AF94A7C48BBD5B6ULL
};


/*
 * Regardless of endian-ness converts a byte to / from a big endian array of
 * bytes.
 */
#define BYTE2WORD(b) (                          \
          (((WORD64)(b)[0] & 0xFF) << 56)       \
        | (((WORD64)(b)[1] & 0xFF) << 48)       \
        | (((WORD64)(b)[2] & 0xFF) << 40)       \
        | (((WORD64)(b)[3] & 0xFF) << 32)       \
        | (((WORD64)(b)[4] & 0xFF) << 24)       \
        | (((WORD64)(b)[5] & 0xFF) << 16)       \
        | (((WORD64)(b)[6] & 0xFF) << 8)        \
        | (((WORD64)(b)[7] & 0xFF))             \
    )

/*
 * Rotate WORD32 to the right by r
 */
static INLINE WORD32
rotr32
(
    WORD32  X,
    int     r
)
{
    return (X >> r) | (X << (32 - r));
}

/*
 * Rotate WORD64 to the right by r
 */
static INLINE WORD64
rotr64
(
    WORD64  X,
    int     r
)
{
    return (X >> r) | (X << (64 - r));
}

/*
 * Rotate Upper and Lower 32 bits of WORD64 to
 * the right by r1 and r2 respectively.
 */
static INLINE WORD64
drotr32
(
    WORD64  X,
    int     r1,
    int     r2
)
{
    return (WORD64)rotr32((WORD32)(X >> 32), r1) << 32 | rotr32((WORD32)X, r2);
}

/*
 * Byte swap
 */
static INLINE WORD64
swap8
(
    WORD64 X
)
{
    X = (X & 0xFFFFFFFF00000000ULL) >> 32 | (X & 0x00000000FFFFFFFFULL) << 32;
    X = (X & 0xFFFF0000FFFF0000ULL) >> 16 | (X & 0x0000FFFF0000FFFFULL) << 16;
    X = (X & 0xFF00FF00FF00FF00ULL) >>  8 | (X & 0x00FF00FF00FF00FFULL) <<  8;

    return X;
}

/*
 * Swap the Upper and Lower 32 bits of WORD64
 */
static INLINE WORD64
swap32
(
    WORD64 X
)
{
    return rotr64(X, 32);
}

static INLINE WORD64
map0
(
    WORD64  R,
    WORD64  S,
    WORD64  T,
    WORD64  U
)
{
    return    ( R & ~S &  T &  U)
            | (     ~S & ~T & ~U)
            | (~R      & ~T &  U)
            | (~R &  S &  T     )
            | ( R      & ~T & ~U);
}

static INLINE WORD64
map1
(
    WORD64  R,
    WORD64  S,
    WORD64  T,
    WORD64  U
)
{
    return    ( R &  S &  T & ~U)
            | ( R & ~S & ~T & ~U)
            | (~R & ~S &  T     )
            | (      S & ~T &  U)
            | (~R           &  U);
}

static INLINE WORD64
map2
(
    WORD64  R,
    WORD64  S,
    WORD64  T,
    WORD64  U
)
{
    return    (~R & ~S &  T &  U)
            | (~R &  S &  T & ~U)
            | (      S & ~T &  U)
            | ( R      & ~T & ~U)
            | ( R &  S      &  U)
            | ( R & ~S      & ~U);
}

/*
 * Non-linear MAP function.
 */
static INLINE void
map
(
    WORD64  R,
    WORD64  S,
    WORD64  T,
    WORD64  U,
    WORD64  *X,
    WORD64  *Y,
    WORD64  *Z,
    int     i
)
{
    WORD64  M[3];

    M[0] = map0(R, S, T, U);
    M[1] = map1(R, S, T, U);
    M[2] = map2(R, S, T, U);

    *X = M[(i + 0) % 3];
    *Y = M[(i + 1) % 3];
    *Z = M[(i + 2) % 3];
}

/*
 * CHI-256
 */
#define _256_MAP map

static INLINE WORD64
_256_theta0
(
    WORD64  X
)
{
    return drotr32(X, 21, 21) ^ drotr32(X, 26, 26) ^ drotr32(X, 30, 30);
}

static INLINE WORD64
_256_theta1
(
    WORD64  X
)
{
    return drotr32(X, 14, 14) ^ drotr32(X, 24, 24) ^ drotr32(X, 31, 31);
}

static INLINE WORD64
_256_mu0
(
    WORD64  X
)
{
    return rotr64(X, 36) ^ rotr64(X, 18) ^ (X >> 1);
}

static INLINE WORD64
_256_mu1
(
    WORD64  X
)
{
    return rotr64(X, 59) ^ rotr64(X, 37) ^ (X >> 10);
}

static void
_256_message_expansion
(
    WORD64  *W
)
{
    int     i;

    for (i = 8; i < 2 * _256_STEPS; ++i)
        W[i] = W[i - 8] ^ _256_mu0(W[i - 7]) ^ _256_mu1(W[i - 2]);
}

static INLINE void
_256_premix
(
    WORD64  A,
    WORD64  B,
    WORD64  D,
    WORD64  E,
    WORD64  *preR,
    WORD64  *preS,
    WORD64  *preT,
    WORD64  *preU
)
{
    *preR = drotr32(B, 8, 8)          ^ drotr32(swap32(D),  5,  1);
    *preS = A                         ^ drotr32(swap32(D), 18, 17);
    *preT = drotr32(swap32(A), 7, 26) ^ drotr32(D, 14, 22);
    *preU = drotr32(A, 17, 12)        ^ drotr32(swap32(E),  2, 23);
}

static INLINE void
_256_datainput
(
    WORD64  preR,
    WORD64  preS,
    WORD64  preT,
    WORD64  preU,
    WORD64  V0,
    WORD64  V1,
    WORD64  *R,
    WORD64  *S,
    WORD64  *T,
    WORD64  *U
)
{
    *R = preR ^ V0;
    *S = preS ^ V1;
    *T = preT ^ _256_theta0(V0);
    *U = preU ^ _256_theta1(V1);
}

static INLINE void
_256_postmix
(
    WORD64  X,
    WORD64  Y,
    WORD64  Z,
    WORD64  *XX,
    WORD64  *YY,
    WORD64  *ZZ,
    WORD64  *AA,
    WORD64  *DD
)
{
    *XX = X;
    *YY = swap8(drotr32(Y, 5, 5));
    *ZZ = swap32(Z);
    *AA = rotr64(*XX + *YY, 16);
    *DD = rotr64(*YY + *ZZ, 48);
}

/*
 * 256 bit update routine.  Called from hash().
 *
 * Parameters:
 *      state   The hash state.
 *      final   1 for the final block in the hash, 0 for intermediate blocks.
 */
static void
_256_update
(
    hashState   *state,
    int         final
)
{
    int         i;

    WORD64      W[2*_256_STEPS];
    WORD64      A, B, C, D, E, F;
    WORD64      preR, preS, preT, preU;
    WORD64      V0, V1;
    WORD64      R, S, T, U;
    WORD64      X, Y, Z, XX, YY, ZZ;
    WORD64      AA, DD, newA, newD;

    A = state->hs_State.small[0];
    B = state->hs_State.small[1];
    C = state->hs_State.small[2];
    D = state->hs_State.small[3];
    E = state->hs_State.small[4];
    F = state->hs_State.small[5];

    if (final)
    {
        A = rotr64(A, 1);
        B = rotr64(B, 1);
        C = rotr64(C, 1);
        D = rotr64(D, 1);
        E = rotr64(E, 1);
        F = rotr64(F, 1);
    }

    for (i = 0; i < _256_MSG_N; ++i)
        W[i] = BYTE2WORD(state->hs_DataBuffer + 8 * i);
    _256_message_expansion(W);

    for (i = 0; i < _256_STEPS; ++i)
    {
        _256_premix(A, B, D, E, &preR, &preS, &preT, &preU);
        V0 = W[2*i  ] ^ K[2*i  ];
        V1 = W[2*i+1] ^ K[2*i+1];
        _256_datainput(preR, preS, preT, preU, V0, V1, &R, &S, &T, &U);
        _256_MAP(R, S, T, U, &X, &Y, &Z, i);
        _256_postmix(X, Y, Z, &XX, &YY, &ZZ, &AA, &DD);

        newA = AA ^ F;
        newD = DD ^ C;
        
        /*
         * Shift
         */
        F = E; E = D; D = newD;
        C = B; B = A; A = newA;
    }

    state->hs_State.small[0] = rotr64(state->hs_State.small[0], 1) ^ A;
    state->hs_State.small[1] = rotr64(state->hs_State.small[1], 1) ^ B;
    state->hs_State.small[2] = rotr64(state->hs_State.small[2], 1) ^ C;
    state->hs_State.small[3] = rotr64(state->hs_State.small[3], 1) ^ D;
    state->hs_State.small[4] = rotr64(state->hs_State.small[4], 1) ^ E;
    state->hs_State.small[5] = rotr64(state->hs_State.small[5], 1) ^ F;
}


/*
 * CHI-512
 */
#define _512_MAP map

static INLINE WORD64
_512_theta0
(
    WORD64  X
)
{
    return rotr64(X,  5) ^ rotr64(X,  6) ^ rotr64(X, 43);
}

static INLINE WORD64
_512_theta1
(
    WORD64  X
)
{
    return rotr64(X, 20) ^ rotr64(X, 30) ^ rotr64(X, 49);
}

static INLINE WORD64
_512_mu0
(
    WORD64  X
)
{
    return rotr64(X, 36) ^ rotr64(X, 18) ^ (X >> 1);
}

static INLINE WORD64
_512_mu1
(
    WORD64  X
)
{
    return rotr64(X, 60) ^ rotr64(X, 30) ^ (X >> 3);
}

static void
_512_message_expansion
(
    WORD64  *W
)
{
    int     i;

    for (i = 16; i < 2 * _512_STEPS; ++i)
    {
        W[i] = W[i - 16]
             ^ W[i -  7]
             ^ _512_mu0(W[i - 15])
             ^ _512_mu1(W[i - 2]);
    }
}

static INLINE void
_512_premix
(
    WORD64 A,
    WORD64 B,
    WORD64 D,
    WORD64 E,
    WORD64 G,
    WORD64 P,
    WORD64 *preR,
    WORD64 *preS,
    WORD64 *preT,
    WORD64 *preU
)
{
    *preR = rotr64(B, 11) ^ rotr64(D,  8) ^ rotr64(G, 13);
    *preS = A             ^ rotr64(D, 21) ^ rotr64(G, 29);
    *preT = rotr64(A, 11) ^ rotr64(D, 38) ^ P;
    *preU = rotr64(A, 26) ^ rotr64(E, 40) ^ rotr64(G, 50);
}

static INLINE void
_512_datainput
(
    WORD64 preR,
    WORD64 preS,
    WORD64 preT,
    WORD64 preU,
    WORD64 V0,
    WORD64 V1,
    WORD64 *R,
    WORD64 *S,
    WORD64 *T,
    WORD64 *U
)
{
    *R = preR ^ V0;
    *S = preS ^ V1;
    *T = preT ^ _512_theta0(V0);
    *U = preU ^ _512_theta1(V1);
}

static INLINE void
_512_postmix
(
    WORD64 X,
    WORD64 Y,
    WORD64 Z,
    WORD64 *XX,
    WORD64 *YY,
    WORD64 *ZZ,
    WORD64 *AA,
    WORD64 *DD,
    WORD64 *GG
)
{
    WORD64 temp;

    *XX = X;
    *YY = swap8(rotr64(Y, 31));
    *ZZ = swap32(Z);

    temp = *XX + *YY;
    *AA = rotr64(temp, 16);

    temp = *YY + *ZZ;
    *DD = rotr64(temp, 48);

    temp = *ZZ + (*XX << 1);
    *GG = temp;
}

/*
 * 512 bit update routine.  Called from hash().
 *
 * Parameters:
 *      state   The hash state.
 *      final   1 for the final block in the hash, 0 for intermediate blocks.
 */
static void
_512_update
(
    hashState   *state,
    int         final
)
{
    int         i;

    WORD64      W[2*_512_STEPS];
    WORD64      A, B, C, D, E, F, G, P, Q;
    WORD64      preR, preS, preT, preU;
    WORD64      V0, V1;
    WORD64      R, S, T, U;
    WORD64      X, Y, Z, XX, YY, ZZ;
    WORD64      AA, DD, GG, newA, newD, newG;

    A = state->hs_State.large[0];
    B = state->hs_State.large[1];
    C = state->hs_State.large[2];
    D = state->hs_State.large[3];
    E = state->hs_State.large[4];
    F = state->hs_State.large[5];
    G = state->hs_State.large[6];
    P = state->hs_State.large[7];
    Q = state->hs_State.large[8];

    if (final)
    {
        A = rotr64(A, 1);
        B = rotr64(B, 1);
        C = rotr64(C, 1);
        D = rotr64(D, 1);
        E = rotr64(E, 1);
        F = rotr64(F, 1);
        G = rotr64(G, 1);
        P = rotr64(P, 1);
        Q = rotr64(Q, 1);
    }

    for (i = 0; i < _512_MSG_N; ++i)
        W[i] = BYTE2WORD(state->hs_DataBuffer + 8*i);
    _512_message_expansion(W);

    for (i = 0; i < _512_STEPS; ++i)
    {
        _512_premix(A, B, D, E, G, P, &preR, &preS, &preT, &preU);
        V0 = W[2*i  ] ^ K[2*i  ];
        V1 = W[2*i+1] ^ K[2*i+1];
        _512_datainput(preR, preS, preT, preU, V0, V1, &R, &S, &T, &U);
        _512_MAP(R, S, T, U, &X, &Y, &Z, i);
        _512_postmix(X, Y, Z, &XX, &YY, &ZZ, &AA, &DD, &GG);

        newA = AA ^ Q;
        newD = DD ^ C;
        newG = GG ^ F;
        
        /*
         * Shift
         */
        Q = P; P = G; G = newG;
        F = E; E = D; D = newD;
        C = B; B = A; A = newA;
    }

    state->hs_State.large[0] = rotr64(state->hs_State.large[0], 1) ^ A;
    state->hs_State.large[1] = rotr64(state->hs_State.large[1], 1) ^ B;
    state->hs_State.large[2] = rotr64(state->hs_State.large[2], 1) ^ C;
    state->hs_State.large[3] = rotr64(state->hs_State.large[3], 1) ^ D;
    state->hs_State.large[4] = rotr64(state->hs_State.large[4], 1) ^ E;
    state->hs_State.large[5] = rotr64(state->hs_State.large[5], 1) ^ F;
    state->hs_State.large[6] = rotr64(state->hs_State.large[6], 1) ^ G;
    state->hs_State.large[7] = rotr64(state->hs_State.large[7], 1) ^ P;
    state->hs_State.large[8] = rotr64(state->hs_State.large[8], 1) ^ Q;
}

/* chi-common.c: CHI hash common functions */

/*
 * Byte i of 64 bit x.  i==0 returns top byte.
 */
#define BYTE(x, i) ((BitSequence)(((x) >> (8 * (7 - i))) & 0xFF))

#define WORD2BYTE(w, b)         \
    do {                        \
        (b)[7] = BYTE(w, 7);    \
        (b)[6] = BYTE(w, 6);    \
        (b)[5] = BYTE(w, 5);    \
        (b)[4] = BYTE(w, 4);    \
        (b)[3] = BYTE(w, 3);    \
        (b)[2] = BYTE(w, 2);    \
        (b)[1] = BYTE(w, 1);    \
        (b)[0] = BYTE(w, 0);    \
    } while(0)

/*
 * Hashes the bytes in hs_DataBuffer.  Either calls te 256 or 512 hash.
 *
 * state    The current state.
 * final    1 for final block in hash, 0 for intermediate blocks.
 */
static void
hash
(
    hashState   *state,
    int         final
)
{
    switch (state->hs_HashBitLen)
    {
    case 224:
    case 256:
        _256_update(state, final);
        break;

    case 384:
    case 512:
        _512_update(state, final);
        break;
    }
}

/*
 * Small routine for incrementing total input data length by hs_DataLen.
 *
 * Parameters:
 *      state       A structure that holds the hashState information
 *
 * Returns:
 *      SUCCESS         - on success.
 *      TOO_MUCH_DATA   - on overflow.
 */
static INLINE HashReturn
inc_total_len
(
    hashState   *state
)
{
    uint64_t    old_len;

    old_len = state->hs_TotalLenLow;
    state->hs_TotalLenLow += state->hs_DataLen;
    if (old_len > state->hs_TotalLenLow)
    {
        switch (state->hs_HashBitLen)
        {
        case 224:
        case 256:
            return TOO_MUCH_DATA;

        case 384:
        case 512:
            if (++state->hs_TotalLenHigh == 0)
                return TOO_MUCH_DATA;
        }
    }

    return SUCCESS;
}

/*
 * Initializes a hashState with the intended hash length of this particular
 * instantiation.
 *
 * Additionally, any data independent setup is performed.
 *
 * Parameters:
 *     state        A structure that holds the hashState information
 *     hashbitlen   An integer value that indicates the length of the hash
 *                  output in bits.
 *
 * Returns:
 *     SUCCESS        - on success
 *     BAD_HASHBITLEN - hashbitlen is invalid (e.g., unknown value)
 *
 */
HashReturn
Init
(
    hashState   *state,
    int         hashbitlen
)
{
    if (state == NULL)
       return BAD_STATE;

    switch (hashbitlen)
    {
    default:
        return BAD_HASHLEN;

    case 224:
        memcpy(state->hs_State.small, _224_init, _256_STATE_N * sizeof(WORD64));
        state->hs_MessageLen = _256_MSG_BLK_LEN;
        break;

    case 256:
        memcpy(state->hs_State.small, _256_init, _256_STATE_N * sizeof(WORD64));
        state->hs_MessageLen = _256_MSG_BLK_LEN;
        break;

    case 384:
        memcpy(state->hs_State.large, _384_init, _512_STATE_N * sizeof(WORD64));
        state->hs_MessageLen = _512_MSG_BLK_LEN;
        break;

    case 512:
        memcpy(state->hs_State.large, _512_init, _512_STATE_N * sizeof(WORD64));
        state->hs_MessageLen = _512_MSG_BLK_LEN;
        break;
    }

    state->hs_HashBitLen = (HashBitLen)hashbitlen;
    state->hs_DataLen      = 0;
    state->hs_TotalLenLow  = 0;
    state->hs_TotalLenHigh = 0;
   
    return SUCCESS;
}

/*
 * Process the supplied data.
 *
 * Parameters:
 *    state         A structure that holds the hashState information
 *    data          The data to be hashed
 *    databitlen    The length, in bits, of the data to be hashed
 *
 * Returns:
 *    SUCCESS  - on success
 *    BAD_DATA - data is NULL, or call made to update and previous
 *               call passed in a partial byte.
 *
 */
HashReturn Update
(
    hashState           *state,
    const BitSequence   *data,
    DataLength          databitlen
)
{
    HashReturn          ret;
    uint32_t            cs;
    /*
     * cs       Amount of data copied out of data into hash block.
     */

    if (state == NULL)
       return BAD_STATE;
    if (data == NULL)
        return BAD_DATA;

    /*
     * Handle partial bytes only if last call to update
     */
    if ((state->hs_DataLen & 0x7) != 0)
        return BAD_DATA;

    while (databitlen > 0)
    {
        cs = MIN((uint32_t)databitlen, state->hs_MessageLen - state->hs_DataLen);

        memcpy(state->hs_DataBuffer+state->hs_DataLen / 8, data, (cs + 7) / 8);
        data += cs / 8;
        databitlen -= cs;
        state->hs_DataLen += cs;

        if (state->hs_DataLen >= (uint32_t)state->hs_MessageLen)
        {
            /*
             * Hash hs_DataBuffer
             */
            hash(state, 0);
            ret = inc_total_len(state);
            if (ret != SUCCESS)
                return ret;
            state->hs_DataLen = 0;
        }
    }

    return SUCCESS;
}

/*
 * Perform any post processing and output filtering required and return
 * the final hash value.
 *
 * Parameters:
 *     state    A structure that holds the hashState information
 *     hashval  The storage for the final hash value to be returned
 *
 * Returns:
 *     SUCCESS - on success
 *
 */
HashReturn
Final
(
    hashState   *state,
    BitSequence *hashval
)
{
    HashReturn  ret;
    uint32_t    whole_bytes;
    uint32_t    last_byte_bits;
    uint32_t    left_over_bytes;
    uint32_t    length_bytes;
    /*
     * whole_bytes      Number of whole bytes in last block with data in them.
     * last_byte_bits   Number of bits in last byte in last block with data in
     *                  them.
     * left_over_bytes  The number of bytes in the last message block that
     *                  have padding & the message length in them.
     * length_bytes     The number of bytes to use right at the end of the
     *                  message to store the message length.
     */

    if (state == NULL)
       return BAD_STATE;
    if (hashval == NULL)
       return BAD_HASHVALPTR;

    ret = inc_total_len(state);
    if (ret != SUCCESS)
        return ret;

    switch (state->hs_HashBitLen)
    {
    case 224:
    case 256:
        length_bytes = sizeof (WORD64);
        break;

    default: /* Suppress compiler warning. */
        assert(0);
    case 384:
    case 512:
        length_bytes = sizeof (WORD64) * 2;
        break;
    }

    whole_bytes = state->hs_DataLen / 8;
    last_byte_bits = state->hs_DataLen % 8;
   
    /*
     * Padding
     *
     * Append a 1 to end of buffer and pad byte with zeros.
     */
    state->hs_DataBuffer[whole_bytes] &= ~((1 << (7-last_byte_bits)) - 1);
    state->hs_DataBuffer[whole_bytes] |= (1 << (7-last_byte_bits));

    /*
     * Skip to end of byte
     */
    state->hs_DataLen += (8 - last_byte_bits);

    left_over_bytes = (state->hs_MessageLen - state->hs_DataLen) / 8;

    if (left_over_bytes < length_bytes)
    {
        /*
         * length doesn't fit in the end of this block.  Pad this block to the
         * end using zeros then write the length in the next block.
         */
        memset(state->hs_DataBuffer + state->hs_DataLen/8, 0, left_over_bytes);
        hash(state, 0);
        state->hs_DataLen = 0;
        left_over_bytes = state->hs_MessageLen / 8;
    }

    memset(state->hs_DataBuffer + state->hs_DataLen / 8, 0, left_over_bytes);

    /*
     * Write 64 bit or 128 bit message length.
     */
    whole_bytes = state->hs_MessageLen / 8 - length_bytes;
    if (length_bytes == sizeof (WORD64) * 2)
    {
        WORD2BYTE(state->hs_TotalLenHigh, state->hs_DataBuffer + whole_bytes);
        whole_bytes += sizeof (WORD64);
    }
    else
    {
        assert(state->hs_TotalLenHigh == 0);
        if (state->hs_TotalLenHigh != 0)
            return BAD_STATE;
    }

    WORD2BYTE(state->hs_TotalLenLow, state->hs_DataBuffer + whole_bytes);
    state->hs_DataLen = state->hs_MessageLen;

    /*
     * Hash final block
     * */
    hash(state, 1);
    state->hs_DataLen = 0;

    /*
     * Write hash output into buffer in network byte order.
     */
    switch (state->hs_HashBitLen)
    {
    case 224:
        WORD2BYTE(state->hs_State.large[0],hashval);
        WORD2BYTE(state->hs_State.large[1],hashval + 8);
        WORD2BYTE(state->hs_State.large[3],hashval + 16);
        hashval[24] = BYTE(state->hs_State.large[4], 0);
        hashval[25] = BYTE(state->hs_State.large[4], 1);
        hashval[26] = BYTE(state->hs_State.large[4], 2);
        hashval[27] = BYTE(state->hs_State.large[4], 3);
        break;

    case 256:
        WORD2BYTE(state->hs_State.large[0],hashval);
        WORD2BYTE(state->hs_State.large[1],hashval + 8);
        WORD2BYTE(state->hs_State.large[3],hashval + 16);
        WORD2BYTE(state->hs_State.large[4],hashval + 24);
        break;

    case 384:
        WORD2BYTE(state->hs_State.large[0],hashval);
        WORD2BYTE(state->hs_State.large[1],hashval + 8);
        WORD2BYTE(state->hs_State.large[2],hashval + 16);
        WORD2BYTE(state->hs_State.large[3],hashval + 24);
        WORD2BYTE(state->hs_State.large[4],hashval + 32);
        WORD2BYTE(state->hs_State.large[5],hashval + 40);
        break;

    case 512:
        WORD2BYTE(state->hs_State.large[0],hashval);
        WORD2BYTE(state->hs_State.large[1],hashval + 8);
        WORD2BYTE(state->hs_State.large[2],hashval + 16);
        WORD2BYTE(state->hs_State.large[3],hashval + 24);
        WORD2BYTE(state->hs_State.large[4],hashval + 32);
        WORD2BYTE(state->hs_State.large[5],hashval + 40);
        WORD2BYTE(state->hs_State.large[6],hashval + 48);
        WORD2BYTE(state->hs_State.large[7],hashval + 56);
        break;
    }

    return SUCCESS;
}

/*
 * Hash the supplied data and provide the resulting hash value. Set return
 * code as appropriate.
 *
 * Parameters:
 *    hashbitlen    The length in bits of the desired hash value
 *    data          The data to be hashed
 *    databitlen    The length, in bits, of the data to be hashed
 *    hashval       The resulting hash value of the provided data
 *
 * Returns:
 *    SUCCESS - on success
 *    FAIL – arbitrary failure
 *    BAD_HASHLEN – unknown hashbitlen requested
 */
HashReturn
Hash
(
    int                 hashbitlen,
    const BitSequence   *data,
    DataLength          databitlen,
    BitSequence         *hashval
)
{
    hashState           hs;
    HashReturn          ret_code;

    if ((ret_code = Init(&hs, hashbitlen)) != SUCCESS)
        return ret_code;

    if ((ret_code = Update(&hs, data, databitlen)) != SUCCESS)
        return ret_code;

    ret_code = Final(&hs, hashval);

    return ret_code;
}


#include <stdint.h>

#define	BitSequence	bmw_BitSequence
#define	DataLength	bmw_DataLength
#define	Data256		bmw_Data256
#define	Data512		bmw_Data512
#define	hashState	bmw_hashState
#define	HashReturn	int

#define	Init		bmw_Init
#define	Update		bmw_Update
#define	Final		bmw_Final
#define	Hash		bmw_Hash

typedef unsigned char BitSequence;
typedef unsigned long long DataLength;

// Specific algorithm definitions
#define BlueMidnightWish224_DIGEST_SIZE  28
#define BlueMidnightWish224_BLOCK_SIZE   64
#define BlueMidnightWish256_DIGEST_SIZE  32
#define BlueMidnightWish256_BLOCK_SIZE   64
#define BlueMidnightWish384_DIGEST_SIZE  48
#define BlueMidnightWish384_BLOCK_SIZE  128
#define BlueMidnightWish512_DIGEST_SIZE  64
#define BlueMidnightWish512_BLOCK_SIZE  128

typedef struct {
    uint32_t DoublePipe[32];
    BitSequence LastPart[BlueMidnightWish256_BLOCK_SIZE * 2];
} Data256;
typedef struct {
    uint64_t DoublePipe[32];
    BitSequence LastPart[BlueMidnightWish512_BLOCK_SIZE * 2];
} Data512;

typedef struct {
    int hashbitlen;

    // + algorithm specific parameters
    uint64_t bits_processed;
    union { 
	Data256  p256[1];
	Data512  p512[1];
    } pipe[1];
    int unprocessed_bits;
} hashState;

HashReturn Init(hashState *state, int hashbitlen);
HashReturn Update(hashState *state, const BitSequence *data, DataLength databitlen);
HashReturn Final(hashState *state, BitSequence *hashval);
HashReturn Hash(int hashbitlen, const BitSequence *data, DataLength databitlen, BitSequence *hashval);

/* Impedance match bytes -> bits length. */
static inline
int _bmw_Update(void * param, const void * _data, size_t _len)
{
    return Update(param, _data, (DataLength)(8 * _len));
}

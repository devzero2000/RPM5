// We use this type definition to ensure that 
// "unsigned long" on 32-bit and 64-bit little-endian 
// operating systems are 4 bytes long.
#if defined( _MSC_VER )
typedef unsigned long u_int32_t;
typedef unsigned long long u_int64_t;
#else
#include <sys/types.h>
#endif

// Specific algorithm definitions
#define EdonR224_DIGEST_SIZE  28
#define EdonR224_BLOCK_SIZE   64
#define EdonR256_DIGEST_SIZE  32
#define EdonR256_BLOCK_SIZE   64
#define EdonR384_DIGEST_SIZE  48
#define EdonR384_BLOCK_SIZE  128
#define EdonR512_DIGEST_SIZE  64
#define EdonR512_BLOCK_SIZE  128

typedef struct
{
	u_int32_t DoublePipe[32];
	unsigned char LastPart[EdonR256_BLOCK_SIZE * 2];
} Data256;
typedef struct
{
	u_int64_t DoublePipe[32];
	unsigned char LastPart[EdonR512_BLOCK_SIZE * 2];
} Data512;

typedef struct {
    int hashbitlen;

	// + algorithm specific parameters
	u_int64_t bits_processed;
	union
    { 
		Data256  p256[1];
		Data512  p512[1];
    } pipe[1];
	int unprocessed_bits;
} edonr_hashState;

int edonr_Init(edonr_hashState *state, int hashbitlen);
int edonr_Update(edonr_hashState *state, const void *_data, size_t _len);
int edonr_Final(edonr_hashState *state, unsigned char *hashval);
int edonr_Hash(int hashbitlen, const void *_data, size_t _len, unsigned char *hashval);

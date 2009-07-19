#ifndef H_TIB3
#define H_TIB3

#define	BitSequence	tib3_BitSequence
#define	DataLength	tib3_DataLength
#define	hashState	tib3_hashState
#define	HashReturn	int

#define	Init		tib3_Init
#define	Update		tib3_Update
#define	Final		tib3_Final
#define	Hash		tib3_Hash

/* Header file for the reference implementation of the candidate algorithm. 
 * All data structures use 64 bits integers 
 *
 * We call a 32 bit integer a WORD, and a 64 bit integer a DWORD
 */

/* Data types mandated by the API (see http://csrc.nist.gov/groups/ST/hash/documents/SHA3-C-API.pdf) */
typedef unsigned char BitSequence;
typedef unsigned long long DataLength;

/* typedefs for 32 and 64 bits unsigned integers, as in C99. They should be replaced
 * by including stdint.h when available, or changed to the suitable values for a
 * given platform. 
 */
#include <stdint.h>

/* 256 bits */
#define STATE_DWORDS_256	4 /* Size of the internal state, in 64 bits units */
#define STATE_BYTES_256		32 /* Size of the internal state, in bytes */
#define BLOCK_DWORDS_256	8 /* Size of the block, in 64 bits units */
#define BLOCK_BYTES_256		64 /* Size of the block, in bytes */
#define HASH_BYTES_256		32 /* Length of the final hash, in bytes */
#define HASH_BITLEN_256		256 /* Length of the final hash, in bits */

/* 224 bits */
#define STATE_DWORDS_224	STATE_DWORDS_256
#define STATE_BYTES_224		STATE_BYTES_256
#define BLOCK_DWORDS_224	BLOCK_DWORDS_256
#define BLOCK_BYTES_224		BLOCK_BYTES_256
#define HASH_BYTES_224		28
#define HASH_BITLEN_224		224

/* 512 bits */
#define STATE_DWORDS_512	8
#define STATE_BYTES_512		64
#define BLOCK_DWORDS_512	16
#define BLOCK_BYTES_512		128
#define HASH_BYTES_512		64
#define HASH_BITLEN_512		512

/* 384 bits */
#define STATE_DWORDS_384	STATE_DWORDS_512
#define STATE_BYTES_384		STATE_BYTES_512
#define BLOCK_DWORDS_384	BLOCK_DWORDS_512
#define BLOCK_BYTES_384		BLOCK_BYTES_512
#define HASH_BYTES_384		48
#define HASH_BITLEN_384		384

#define Update224 Update256
#define Update384 Update512

/* hash state for 224 and 256 bits versions */
typedef struct {
    uint64_t state[STATE_DWORDS_256];		/* internal state */
    uint64_t bits_processed;			
    uint64_t buffer[2*BLOCK_DWORDS_256];    /* buffer for the block and the previous block */
    uint64_t *previous_block;
    uint64_t *data_block;
    uint32_t bits_waiting_for_process;		/* bits awaiting for process in the next call to Update() or Final() */
} hashState256;

/* hash state for 384 and 512 bits versions */
typedef struct {
    uint64_t state[STATE_DWORDS_512];
    uint64_t bits_processed;
    uint64_t buffer[2*BLOCK_DWORDS_512];    
    uint64_t *previous_block;
    uint64_t *data_block;
    uint32_t bits_waiting_for_process;
} hashState512;

/* This is the struct mandated by the the API (see http://csrc.nist.gov/groups/ST/hash/documents/SHA3-C-API.pdf) */
typedef struct {
    int hashbitlen;
    union {
	hashState256 state256[1];
	hashState512 state512[1];
    } uu[1];
} hashState;


/* prototypes of functions required by the API */
HashReturn Init (hashState *state, int hashbitlen);
HashReturn Update (hashState *state , const BitSequence *data, DataLength databitlen);
HashReturn Final (hashState *state, BitSequence *hashval);
HashReturn Hash(int hashbitlen, const BitSequence *data, DataLength databitlen, BitSequence *hashval);


/* prototypes of functions for each bit length */
HashReturn Init256 (hashState256 *state);
HashReturn Update256 (hashState256 *state , const BitSequence *data, DataLength databitlen);
HashReturn Final256 (hashState256 *state, BitSequence *hashval);
HashReturn Hash256(const BitSequence *data, DataLength databitlen, BitSequence *hashval);

HashReturn Init224 (hashState256 *state);
HashReturn Final224 (hashState256 *state, BitSequence *hashval);
HashReturn Hash224(const BitSequence *data, DataLength databitlen, BitSequence *hashval);

HashReturn Init512 (hashState512 *state);
HashReturn Update512 (hashState512 *state , const BitSequence *data, DataLength databitlen);
HashReturn Final512 (hashState512 *state, BitSequence *hashval);
HashReturn Hash512(const BitSequence *data, DataLength databitlen, BitSequence *hashval);

HashReturn Init384 (hashState512 *state);
HashReturn Final384 (hashState512 *state, BitSequence *hashval);
HashReturn Hash384(const BitSequence *data, DataLength databitlen, BitSequence *hashval);

/* Impedance match bytes -> bits length. */
static inline
int _tib3_Update(void * param, const void * _data, size_t _len)
{
    return Update(param, _data, (DataLength)(8 * _len));
}

#endif /* H_TIB3 */

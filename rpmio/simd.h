#ifndef __NIST_H__
#define __NIST_H__

#include <stdint.h>

#define	BitSequence	simd_BitSequence
#define	DataLength	simd_DataLength
#define	hashState	simd_hashState
#define	HashReturn	int

#define	Init		simd_Init
#define	Update		simd_Update
#define	Final		simd_Final
#define	Hash		simd_Hash

/*
 * NIST API Specific types.
 */

typedef unsigned char BitSequence;
typedef uint64_t DataLength;

typedef struct {
  unsigned int hashbitlen;
  unsigned int blocksize;
  unsigned int n_feistels;

  uint64_t count;

  uint32_t *A, *B, *C, *D;
  unsigned char* buffer;
} hashState;

/* 
 * NIST API
 */

HashReturn Init(hashState *state, int hashbitlen);
HashReturn Update(hashState *state, const BitSequence *data, DataLength databitlen);
HashReturn Final(hashState *state, BitSequence *hashval);
HashReturn Hash(int hashbitlen, const BitSequence *data, DataLength databitlen,
                BitSequence *hashval);

/* Impedance match bytes -> bits length. */
static inline
int _simd_Update(void * param, const void * _data, size_t _len)
{
    return Update(param, _data, (DataLength)(8 * _len));
}

#endif

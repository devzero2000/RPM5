/*******************************/
/* luffa.h                     */
/* Version 1.1 (Feb 12th 2009) */
/* Made by Hitachi Ltd.        */
/*******************************/

#include <stdint.h>

#define	BitSequence	luffa_BitSequence
#define	DataLength	luffa_DataLength
#define	hashState	luffa_hashState
#define	HashReturn	int

#define	Init		luffa_Init
#define	Update		luffa_Update
#define	Final		luffa_Final
#define	Hash		luffa_Hash

typedef unsigned char BitSequence;
typedef unsigned long long DataLength;

typedef struct {
    int hashbitlen;
    int width;        /* The number of blocks in chaining values*/
    int limit;        /* The limit of message lengthin in unit of 64bit*/
    uint64_t bitlen[2]; /* Message length in bits */
    uint32_t rembitlen; /* Length of buffer data to be hashed */
    uint32_t buffer[8]; /* Buffer to be hashed */
    uint32_t chainv[40];   /* Chaining values */
} hashState;

HashReturn Init(hashState *state, int hashbitlen);
HashReturn Update(hashState *state, const BitSequence *data, DataLength databitlen);
HashReturn Final(hashState *state, BitSequence *hashval);
HashReturn Hash(int hashbitlen, const BitSequence *data, DataLength databitlen, BitSequence *hashval);

/* Impedance match bytes -> bits length. */
static inline
int _luffa_Update(void * param, const void * _data, size_t _len)
{
    return Update(param, _data, (DataLength)(8 * _len));
}

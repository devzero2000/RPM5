/* laneref.h   v1.1   October 2008
 * Reference ANSI C implementation of LANE
 * Based on the AES reference implementation of Paulo Barreto and Vincent Rijmen.
 * Author: Nicky Mouha
 * This code is placed in the public domain.
 */

#ifndef H_LANE
#define H_LANE

#define	BitSequence	lane_BitSequence
#define	DataLength	lane_DataLength
#define	hashState	lane_hashState
#define	HashReturn	int

#define	Init		lane_Init
#define	Update		lane_Update
#define	Final		lane_Final
#define	Hash		lane_Hash

#include <stdint.h>

typedef uint8_t  BitSequence;
typedef uint64_t DataLength;

typedef struct {
    int hashbitlen;		/* length in bits of the hash value */
    uint64_t ctr;		/* number of data bits processed so far */
    uint32_t h[64/4];		/* intermediate hash value */
    uint8_t buffer[128];	/* space for one block of data to be hashed */
} hashState;

HashReturn Init(hashState *state, int hashbitlen);
HashReturn Update(hashState *state, const BitSequence *data, DataLength databitlen);

HashReturn Final(hashState *state, BitSequence *hashval);

HashReturn Hash(int hashbitlen, const BitSequence *data, DataLength databitlen,
                BitSequence *hashval);

/* Impedance match bytes -> bits length. */
static inline
int _lane_Update(void * param, const void * _data, size_t _len)
{
    return Update(param, _data, (DataLength)(8 * _len));
}

#endif /* H_LANE */

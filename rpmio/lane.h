/* laneref.h   v1.1   October 2008
 * Reference ANSI C implementation of LANE
 *
 * Based on the AES reference implementation of Paulo Barreto and Vincent Rijmen.
 *
 * Author: Nicky Mouha
 *
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

typedef uint8_t  BitSequence;        /* an unsigned  8-bit integer */
typedef uint64_t DataLength;         /* an unsigned 64-bit integer */

typedef struct {
  int hashbitlen;          /* length in bits of the hash value */
  DataLength databitcount; /* number of data bits processed so far */
  uint8_t buffer[128];     /* space for one block of data to be hashed */
  uint8_t hash[64];        /* intermediate hash value */
} hashState;

/* Initialize the hashState structure. */
HashReturn Init(hashState *state, /* structure that holds hashState information */
                int hashbitlen);  /* length in bits of the hash value */

/* Process data using the algorithm's compression function.
 * Precondition: Init() has been called.
 */
HashReturn Update(hashState *state,        /* structure that holds hashState information */
                  const BitSequence *data, /* the data to be hashed */
                  DataLength databitlen);  /* length of the data to be hashed, in bits */

/* Perform post processing and output filtering and return the final hash value.
 * Precondition: Init() has been called.
 * Final() may only be called once.
 */
HashReturn Final(hashState *state,       /* structure that holds hashState information */
                 BitSequence *hashval);  /* storage for the final hash value */

/* Hash the supplied data and provide the resulting hash code. */
HashReturn Hash(int hashbitlen,          /* length in bits of the hash value */
                const BitSequence *data, /* the data to be hashed */
                DataLength databitlen,   /* length of the data to be hashed, in bits */
                BitSequence *hashval);   /* resulting hash value */

/* Impedance match bytes -> bits length. */
static inline
int _lane_Update(void * param, const void * _data, size_t _len)
{
    return Update(param, _data, (DataLength)(8 * _len));
}

#endif /* H_LANE */

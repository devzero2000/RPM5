#ifndef HAMSI_H
#define HAMSI_H

#include <stdint.h>

#define	BitSequence	hamsi_BitSequence
#define	DataLength	hamsi_DataLength
#define	hashState	hamsi_hashState
#define	HashReturn	int

#define	Init		hamsi_Init
#define	Update		hamsi_Update
#define	Final		hamsi_Final
#define	Hash		hamsi_Hash

typedef unsigned char BitSequence;
typedef unsigned long long DataLength;

typedef struct {
    int hashbitlen;
    int leftbits;
    uint8_t leftdata[8];
    uint32_t state[16];     // chain value
    uint32_t counter;

    int cvsize;
    // int blocksize;
    int ROUNDS;
    int PFROUNDS;
} hashState;

HashReturn   Init(hashState *state, int hashbitlen);
HashReturn Update(hashState *state, const BitSequence* data, DataLength databitlen);
HashReturn  Final(hashState *state, BitSequence* hashval);
HashReturn   Hash(int hashbitlen, const BitSequence *data, DataLength databitlen, BitSequence *hashval);

/* Impedance match bytes -> bits length. */
static inline
int _hamsi_Update(void * param, const void * _data, size_t _len)
{
    return Update(param, _data, (DataLength)(8 * _len));
}

#endif

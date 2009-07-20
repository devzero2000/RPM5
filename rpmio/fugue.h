#ifndef _FUGUE_H
#define _FUGUE_H

#include <string.h>
#include <stdint.h>

#define	BitSequence	fugue_BitSequence
#define	DataLength	fugue_DataLength
#define	hashState	fugue_hashState
#define	HashReturn	int

#define	Init		fugue_Init
#define	Update		fugue_Update
#define	Final		fugue_Final
#define	Hash		fugue_Hash

typedef uint8_t  BitSequence;
typedef uint64_t DataLength;

typedef union {
    uint32_t    d;
    uint8_t     b[4];
} hash32_s;
typedef hash32_s* hash32_p;

typedef struct {
    int        n;   /* columns in output */
    int        s;   /* columns in state */
    int        k;   /* number of smix's per TIX or round */
    int        r;   /* number of G1 rounds in final part */
    int        t;   /* number of G2 rounds in final part */
} hashCfg;

typedef struct {
    int        hashbitlen;
    hashCfg*   Cfg;
    int        Base;
    hash32_s   State[36];
    uint32_t     Partial[1];
    uint64_t     TotalBits;
} hashState;

HashReturn Init(hashState *state, int hashbitlen);

HashReturn Update(hashState *state, const BitSequence *data,
                  DataLength databitlen);

HashReturn Final(hashState *state, BitSequence *hashval);

HashReturn Hash(int hashbitlen, const BitSequence *data,
                DataLength databitlen, BitSequence *hashval);

/* Impedance match bytes -> bits length. */
static inline
int _fugue_Update(void * param, const void * _data, size_t _len)
{
    return Update(param, _data, (DataLength)(8 * _len));
}

#endif

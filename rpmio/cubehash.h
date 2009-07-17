#ifndef cubehash_h
#define cubehash_h

typedef unsigned char BitSequence;
typedef unsigned long long DataLength;
typedef enum { SUCCESS = 0, FAIL = 1, BAD_HASHBITLEN = 2 } HashReturn;

typedef unsigned int myuint32; /* must be exactly 32 bits */

typedef struct {
  int hashbitlen;
  int pos; /* number of bits read into x from current block */
  myuint32 x[32];
} hashState;

HashReturn cubehash_Init(hashState *state, int hashbitlen);

HashReturn cubehash_Update(hashState *state, const BitSequence *data,
                  DataLength databitlen);

HashReturn cubehash_Final(hashState *state, BitSequence *hashval);

HashReturn cubehash_Hash(int hashbitlen, const BitSequence *data,
                DataLength databitlen, BitSequence *hashval);

#endif

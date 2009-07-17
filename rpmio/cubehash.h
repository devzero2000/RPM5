#ifndef cubehash_h
#define cubehash_h

typedef unsigned int myuint32; /* must be exactly 32 bits */

typedef struct {
  int hashbitlen;
  int pos; /* number of bits read into x from current block */
  myuint32 x[32];
} cubehash_hashState;

int cubehash_Init(cubehash_hashState *state, int hashbitlen);

int cubehash_Update(cubehash_hashState *state, const unsigned char *_data,
                  size_t _len);

int cubehash_Final(cubehash_hashState *state, unsigned char *hashval);

int cubehash_Hash(int hashbitlen, const unsigned char *_data,
                size_t _len, unsigned char *hashval);

#endif

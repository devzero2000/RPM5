
#include <stdint.h>

#define	BitSequence	blake_BitSequence
#define	DataLength	blake_DataLength
#define	hashState	blake_hashState
#define	HashReturn	int

#define	Init		blake_Init
#define	Update		blake_Update
#define	Final		blake_Final
#define	Hash		blake_Hash

typedef unsigned char BitSequence; 
typedef unsigned long long DataLength; 

typedef struct  { 
  int hashbitlen;  /* length of the hash value (bits) */
  int datalen;     /* amount of remaining data to hash (bits) */
  int init;        /* set to 1 when initialized */
  int nullt;       /* Boolean value for special case \ell_i=0 */
  /* variables for the 32-bit version  */
  uint32_t h32[8];         /* current chain value (initialized to the IV) */
  uint32_t t32[2];         /* number of bits hashed so far */
  BitSequence data32[64];     /* remaining data to hash (less than a block) */
  uint32_t salt32[4];      /* salt (null by default) */
  /* variables for the 64-bit version */
  uint64_t h64[8];      /* current chain value (initialized to the IV) */
  uint64_t t64[2];      /* number of bits hashed so far */
  BitSequence data64[128];  /* remaining data to hash (less than a block) */
  uint64_t salt64[4];   /* salt (null by default) */
} hashState;

HashReturn Init( hashState * state, int hashbitlen );

HashReturn Update( hashState * state, const BitSequence * data, DataLength databitlen );

HashReturn Final( hashState * state, BitSequence * hashval );

HashReturn Hash( int hashbitlen, const BitSequence * data, DataLength databitlen, 
		 BitSequence * hashval );

/* Impedance match bytes -> bits length. */
static inline
int _blake_Update(void * param, const void * _data, size_t _len)
{
    return Update(param, _data, (DataLength)(8 * _len));
}

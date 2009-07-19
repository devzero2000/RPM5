/*This program gives the reference implementation of JH.
  1) The description given in this program is suitable for implementation in hardware
  2) All the operations are operated on bytes, so the description is suitable for implementation on 8-bit processor
*/

#include <string.h>

#define	BitSequence	jh_BitSequence
#define	DataLength	jh_DataLength
#define	hashState	jh_hashState
#define	HashReturn	int

#define	Init		jh_Init
#define	Update		jh_Update
#define	Final		jh_Final
#define	Hash		jh_Hash

typedef unsigned char BitSequence;
typedef unsigned long long DataLength;  

typedef struct {
	int hashbitlen;				    /*the message digest size*/
	unsigned long long databitlen;  /*the message size in bits*/
	unsigned char H[128];			/*the hash value H; 128 bytes;*/
	unsigned char A[256];			/*the temporary round value; 256 4-bit elements*/
    unsigned char roundconstant[64];/*round constant for one round; 64 4-bit elements*/
	unsigned char buffer[64];		/*the message block to be hashed; 64 bytes*/
} hashState;

HashReturn Init(hashState *state, int hashbitlen);
HashReturn Update(hashState *state, const BitSequence *data, DataLength databitlen);
HashReturn Final(hashState *state, BitSequence *hashval);
HashReturn Hash(int hashbitlen, const BitSequence *data,DataLength databitlen, BitSequence *hashval);

/* Impedance match bytes -> bits length. */
static inline
int _jh_Update(void * param, const void * _data, size_t _len)
{
    return Update(param, _data, (DataLength)(8 * _len));
}

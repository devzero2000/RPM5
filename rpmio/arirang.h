////////////////////////////////////////////////////////////////////////////////
//
// PROJECT : Arirang family
//
// DATE    : 2008.10.23
//
////////////////////////////////////////////////////////////////////////////////
//
// FILE  : Arirang_Ref64.h
//
// NOTES : Reference code of Arirang family
// 
//         Based on 64-bit platform
//
////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>

#define	BitSequence	arirang_BitSequence
#define	DataLength	arirang_DataLength
#define	hashState	arirang_hashState
#define	HashReturn	int

#define	Init		arirang_Init
#define	Update		arirang_Update
#define	Final		arirang_Final
#define	Hash		arirang_Hash

#define ARIRANG224_BLOCK_LEN	64
#define ARIRANG224_DIGEST_LEN	28

#define ARIRANG256_BLOCK_LEN	64
#define ARIRANG256_DIGEST_LEN	32

#define ARIRANG384_BLOCK_LEN	128
#define ARIRANG384_DIGEST_LEN	48

#define ARIRANG512_BLOCK_LEN	128
#define ARIRANG512_DIGEST_LEN	64

typedef unsigned char		BYTE;		//  8-bit data type
typedef unsigned short int	WORD;		// 16-bit data type
typedef unsigned int		DWORD;		// 32-bit data type
typedef unsigned long long	QWORD;		// 64-bit data type

typedef unsigned char BitSequence;
typedef unsigned long long DataLength;

typedef struct {

	// Hash length
	int hashbitlen;
	
	// Counter
	QWORD counter[2];

	// Count
	QWORD count[2];

	// Message block
	BYTE block[ARIRANG512_BLOCK_LEN];

	// Working variables
	QWORD workingvar[8];

	// hash block length
	DWORD blocklen;

	// bit_length % 7
	DWORD remainderbit;

} hashState;

HashReturn Init(hashState *state, int hashbitlen);

HashReturn Update(hashState *state, const BitSequence *data, DataLength databitlen);

HashReturn Final(hashState *state, BYTE *hashval);

HashReturn Hash(int hashbitlen, const BitSequence *data, DataLength *databitlen, BitSequence *hashval);

/* Impedance match bytes -> bits length. */
static inline
int _arirang_Update(void * param, const void * _data, size_t _len)
{
    return Update(param, _data, (DataLength)(8 * _len));
}

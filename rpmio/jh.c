/*This program gives the reference implementation of JH.
  1) The description given in this program is suitable for implementation in hardware
  2) All the operations are operated on bytes, so the description is suitable for implementation on 8-bit processor
*/

#include <jh.h>

enum { SUCCESS = 0, FAIL = 1, BAD_HASHLEN = 2 };

/*The constant for the Round 0 of E8*/
const unsigned char roundconstant_zero[64] = {0x6,0xa,0x0,0x9,0xe,0x6,0x6,0x7,0xf,0x3,0xb,0xc,0xc,0x9,0x0,0x8,0xb,0x2,0xf,0xb,0x1,0x3,0x6,0x6,0xe,0xa,0x9,0x5,0x7,0xd,0x3,0xe,0x3,0xa,0xd,0xe,0xc,0x1,0x7,0x5,0x1,0x2,0x7,0x7,0x5,0x0,0x9,0x9,0xd,0xa,0x2,0xf,0x5,0x9,0x0,0xb,0x0,0x6,0x6,0x7,0x3,0x2,0x2,0xa};

/*The two Sboxes S0 and S1*/
unsigned char S[2][16] = {{9,0,4,11,13,12,3,15,1,10,2,6,7,5,8,14},{3,12,6,13,5,7,1,9,15,2,0,4,11,10,14,8}};

/*The linear transformation L*/
#define L(a, b) {														\
	(b) ^= ( ( (a) << 1) ^ ( (a) >> 3) ^ (( (a) >> 2) & 2) ) & 0xf;		\
	(a) ^= ( ( (b) << 1) ^ ( (b) >> 3) ^ (( (b) >> 2) & 2) ) & 0xf;	    \
}   

/*round function*/
static
void R8(hashState *state) 
{
	unsigned int i;
	unsigned char tem[256],t;
	unsigned char roundconstant_expanded[256];  /*the round constant expanded into 256 1-bit element;*/

	/*expand the round constant into 256 one-bit element*/
    for (i = 0; i < 256; i++)  {
		roundconstant_expanded[i] = (state->roundconstant[i >> 2] >> (3 - (i & 3)) ) & 1;
	}

	/*S box layer, each constant bit selects one Sbox from S0 and S1*/
	for (i = 0; i < 256; i++) {
		tem[i] = S[roundconstant_expanded[i]][state->A[i]]; /*constant bits are used to determine which Sbox to use*/
	}

	/*L Layer*/
	for (i = 0; i < 256; i=i+2) L(tem[i], tem[i+1]);

	/*The following is the permuation layer P_8$ */

	/*initial swap Pi_8*/
	for ( i = 0; i < 256; i=i+4) {
			t = tem[i+2];
			tem[i+2] = tem[i+3];
			tem[i+3] = t;
	}

	/*permutation P'_8*/
	for (i = 0; i < 128; i=i+1) {
		state->A[i] = tem[i<<1];
		state->A[i+128] = tem[(i<<1)+1];
	}

	/*final swap Phi_8*/
	for ( i = 128; i < 256; i=i+2) {
			t = state->A[i];
			state->A[i] = state->A[i+1];
			state->A[i+1] = t;
	}

}

/*last half round of R8, only Sbox layer in this last half round*/
static
void last_half_round_R8(hashState *state) 
{
	unsigned int i;
	unsigned char roundconstant_expanded[256];  /*the round constant expanded into 256 1-bit element;*/

	/*expand the round constant into one-bit element*/
    for (i = 0; i < 256; i++)  {
		roundconstant_expanded[i] = (state->roundconstant[i >>2] >> (3 - (i & 3)) ) & 1;
	}

	/*S box layer*/
	for (i = 0; i < 256; i++) {
		state->A[i] = S[roundconstant_expanded[i]][state->A[i]]; /*constant bits are used to determine which Sbox to use*/
	}
}


/*The following function generates the next round constant from 
  the current round constant;  R6 is used for generating round constants for E8, and
  the round constants of R6 is set as 0;
*/
static
void update_roundconstant(hashState *state) {
	int i;
	unsigned char tem[64],t;

	/*Sbox layer*/
    for (i = 0; i < 64; i++)   tem[i] = S[0][state->roundconstant[i]];

	/*Linear transformation layer L*/
	for (i = 0; i < 64; i=i+2) L(tem[i], tem[i+1]);

    /*The following is the permutation layer P_6 */

	/*initial swap Pi_6*/
	for ( i = 0; i < 64; i=i+4) {
			t = tem[i+2];
			tem[i+2] = tem[i+3];
			tem[i+3] = t;
	}

	/*permutation P'_6*/
	for ( i = 0; i < 32; i=i+1) {
		state->roundconstant[i]    = tem[i<<1];
		state->roundconstant[i+32] = tem[(i<<1)+1];
	}

	/*final swap Phi_6*/
	for ( i = 32; i < 64; i=i+2 ) {
			t = state->roundconstant[i];
			state->roundconstant[i] = state->roundconstant[i+1];
			state->roundconstant[i+1] = t;
	}
}

/*Bijective function E*/
static
void E8(hashState *state) 
{
	unsigned int i;
	unsigned char t0,t1,t2,t3;
	unsigned char tem[256];

	/*initial group at the begining of E_8: group the H value into 4-bit elements and store them in A */
	/*t0 is the i-th bit of H*/
	/*t1 is the (i+2^d)-th bit of H*/
	/*t2 is the (i+2*(2^d))-th bit of H*/
	/*t3 is the (i+3*(2^d))-th bit of H*/
	for (i = 0; i < 256; i++) {
		t0 = (state->H[i>>3] >> (7 - (i & 7)) ) & 1;
		t1 = (state->H[(i+256)>>3] >> (7 - (i & 7)) ) & 1;
		t2 = (state->H[(i+ 512 )>>3] >> (7 - (i & 7)) ) & 1;
		t3 = (state->H[(i+ 768 )>>3] >> (7 - (i & 7)) ) & 1;
		tem[i] = (t0 << 3) | (t1 << 2) | (t2 << 1) | (t3 << 0); 
	}

	for (i = 0; i < 128; i++) {
		state->A[i << 1] = tem[i]; 
		state->A[(i << 1)+1] = tem[i+128]; 
	} 

	/* 35 rounds */
	for (i = 0; i < 35; i++) {
		R8(state);
		update_roundconstant(state);
	}

	/*the final half round with only Sbox layer*/
	last_half_round_R8(state);

	/*de-group at the end of E_8:  decompose the 4-bit elements of A into the 1024-bit H*/
	for (i = 0; i < 128; i++) state->H[i] = 0;

	for (i = 0; i < 128; i++) {
		tem[i] = state->A[i << 1]; 
		tem[i+128] = state->A[(i << 1)+1];
	} 

	for (i = 0; i < 256; i++) {
		t0 = (tem[i] >> 3) & 1;
		t1 = (tem[i] >> 2) & 1;
		t2 = (tem[i] >> 1) & 1;
		t3 = (tem[i] >> 0) & 1;

        state->H[i>>3] |= t0 << (7 - (i & 7));
        state->H[(i + 256)>>3] |= t1 << (7 - (i & 7));
		state->H[(i + 512)>>3] |= t2 << (7 - (i & 7));
		state->H[(i + 768)>>3] |= t3 << (7 - (i & 7));
	}

}


/* compression function F8 */
static
void F8(hashState *state) 
{
	unsigned int i;

	/*initialize the round constant*/
	for (i = 0; i < 64; i++) state->roundconstant[i] = roundconstant_zero[i];  

	/*xor the message with the first half of H*/
	for (i = 0; i < 64; i++) state->H[i] ^= state->buffer[i];

	/* Bijective function E8 */
    E8(state);

	/* xor the message with the last half of H */
	for (i = 0; i < 64; i++) state->H[i+64] ^= state->buffer[i];
}


HashReturn Init(hashState *state, int hashbitlen) 
{
	unsigned int i;

	state->hashbitlen = hashbitlen;

	for (i = 0; i < 64; i++) state->buffer[i] = 0;
	for (i = 0; i < 128; i++) state->H[i] = 0;

    /*initialize the initial hash value of JH*/
	state->H[1] = hashbitlen & 0xff;
	state->H[0] = (hashbitlen >> 8) & 0xff;
	
	F8(state);

	return(SUCCESS);
}


HashReturn Update(hashState *state, const BitSequence *data, DataLength databitlen) 
{
	DataLength i;

	state->databitlen = databitlen;

	/*compress the message blocks except the last partial block*/
	for (i = 0; (i+512) <= databitlen; i = i+512) {
		memcpy(state->buffer, data + (i >> 3), 64);
		F8(state);
	}

	/*storing the partial block into buffer*/
	if ( (databitlen & 0x1ff) > 0) {
		for (i = 0; i < 64; i++) state->buffer[i] = 0;
		if ((databitlen & 7) == 0) memcpy(state->buffer, data + ((databitlen >> 9) << 6), (databitlen & 0x1ff) >> 3);
		else memcpy(state->buffer, data + ((databitlen >> 9) << 6), ((databitlen & 0x1ff) >> 3)+1);
	}

	return(SUCCESS);
}


/*padding the message, truncate the hash value H and obtain the message digest*/
HashReturn Final(hashState *state, BitSequence *hashval) 
{
	unsigned int i;

	if ( (state->databitlen & 0x1ff) == 0) {
		/*pad the message when databitlen is multiple of 512 bits, then process the padded block*/
		for (i = 0; i < 64; i++) state->buffer[i] = 0;
		state->buffer[0] = 0x80;
		state->buffer[63] = state->databitlen & 0xff;
		state->buffer[62] = (state->databitlen >> 8) & 0xff;
		state->buffer[61] = (state->databitlen >> 16) & 0xff;
		state->buffer[60] = (state->databitlen >> 24) & 0xff;
		state->buffer[59] = (state->databitlen >> 32) & 0xff;
		state->buffer[58] = (state->databitlen >> 40) & 0xff;
		state->buffer[57] = (state->databitlen >> 48) & 0xff;
		state->buffer[56] = (state->databitlen >> 56) & 0xff;
		F8(state);
	}
	else {
		/*pad and process the partial block*/
		state->buffer[((state->databitlen & 0x1ff) >> 3)] |= 1 << (7- (state->databitlen & 7));
		F8(state);
		for (i = 0; i < 64; i++) state->buffer[i] = 0;
		/*precess the last padded block*/
		state->buffer[63] = state->databitlen & 0xff;
		state->buffer[62] = (state->databitlen >> 8) & 0xff;
		state->buffer[61] = (state->databitlen >> 16) & 0xff;
		state->buffer[60] = (state->databitlen >> 24) & 0xff;
		state->buffer[59] = (state->databitlen >> 32) & 0xff;
		state->buffer[58] = (state->databitlen >> 40) & 0xff;
		state->buffer[57] = (state->databitlen >> 48) & 0xff;
		state->buffer[56] = (state->databitlen >> 56) & 0xff;
		F8(state);
	}

	/*trunacting the final hash value to generate the message digest*/
	if ( state->hashbitlen == 224) 
		memcpy(hashval,state->H+100,28);

	if ( state->hashbitlen == 256)
		memcpy(hashval,state->H+96,32);

	if ( state->hashbitlen == 384) 
		memcpy(hashval,state->H+80,48);

	if ( state->hashbitlen == 512)
		memcpy(hashval,state->H+64,64);

	return(SUCCESS);
}


/*hash the message*/
HashReturn Hash(int hashbitlen, const BitSequence *data,DataLength databitlen, BitSequence *hashval) 
{
	hashState state;

	if ( hashbitlen == 224 || hashbitlen == 256 || hashbitlen == 384 || hashbitlen == 512 ) {	
		Init(&state, hashbitlen);
		Update(&state, data, databitlen);
		Final(&state, hashval);
		return SUCCESS;
	}
	else 
		return(BAD_HASHLEN);
}

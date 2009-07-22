#include <string.h>
#include <stdio.h>
#include "blake.h"

enum { SUCCESS=0, FAIL=1, BAD_HASHBITLEN=2  };


#define NB_ROUNDS32 10
#define NB_ROUNDS64 14

/*
  byte-to-word conversion and vice-versa (little endian)  
*/
#define U8TO32_BE(p) \
  (((uint32_t)((p)[0]) << 24) | \
   ((uint32_t)((p)[1]) << 16) | \
   ((uint32_t)((p)[2]) <<  8) | \
   ((uint32_t)((p)[3])      ))

#define U8TO64_BE(p) \
  (((uint64_t)U8TO32_BE(p) << 32) | (uint64_t)U8TO32_BE((p) + 4))

#define U32TO8_BE(p, v) \
  do { \
    (p)[0] = (BitSequence)((v) >> 24);  \
    (p)[1] = (BitSequence)((v) >> 16); \
    (p)[2] = (BitSequence)((v) >>  8); \
    (p)[3] = (BitSequence)((v)      ); \
  } while (0)

#define U64TO8_BE(p, v) \
  do { \
    U32TO8_BE((p),     (uint32_t)((v) >> 32));	\
    U32TO8_BE((p) + 4, (uint32_t)((v)      ));	\
  } while (0)


/*
  adds a salt to the hash function (OPTIONAL)
  should be called AFTER Init, and BEFORE Update

  INPUT
  state: hashSate structure
  salt: the salt, whose length is determined by hashbitlen

  OUTPUT
  SUCCESS on success
 */
HashReturn AddSalt( hashState * state, const BitSequence * salt );

/*
  the 10 permutations of {0,...15}
*/
static const unsigned char sigma[][16] = {
    {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 } ,
    { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 } ,
    { 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 } ,
    {  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 } ,
    {  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 } ,
    {  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 } ,
    { 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 } ,
    { 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 } ,
    {  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 } ,
    { 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13 , 0 }, 
    {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 } ,
    { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 } ,
    { 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 } ,
    {  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 } ,
    {  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 } ,
    {  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 } ,
    { 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 } ,
    { 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 } ,
    {  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 } ,
    { 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13 , 0 }  
  };

/*
  constants for BLAKE-32 and BLAKE-28
*/
static const uint32_t c32[16] = {
    0x243F6A88, 0x85A308D3,
    0x13198A2E, 0x03707344,
    0xA4093822, 0x299F31D0,
    0x082EFA98, 0xEC4E6C89,
    0x452821E6, 0x38D01377,
    0xBE5466CF, 0x34E90C6C,
    0xC0AC29B7, 0xC97C50DD,
    0x3F84D5B5, 0xB5470917 
};

/*
  constants for BLAKE-64 and BLAKE-48
*/
static const uint64_t c64[16] = {
  0x243F6A8885A308D3ULL,0x13198A2E03707344ULL,
  0xA4093822299F31D0ULL,0x082EFA98EC4E6C89ULL,
  0x452821E638D01377ULL,0xBE5466CF34E90C6CULL,
  0xC0AC29B7C97C50DDULL,0x3F84D5B5B5470917ULL,
  0x9216D5D98979FB1BULL,0xD1310BA698DFB5ACULL,
  0x2FFD72DBD01ADFB7ULL,0xB8E1AFED6A267E96ULL,
  0xBA7C9045F12C7F99ULL,0x24A19947B3916CF7ULL,
  0x0801F2E2858EFC16ULL,0x636920D871574E69ULL
};

/*
  padding data
*/
static const BitSequence padding[129] =
  {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*
  initial values ( IVx for BLAKE-x)
*/
static const uint32_t IV32[8]={
  0x6A09E667, 0xBB67AE85,
  0x3C6EF372, 0xA54FF53A,
  0x510E527F, 0x9B05688C,
  0x1F83D9AB, 0x5BE0CD19
};
static const uint32_t IV28[8]={
  0xC1059ED8, 0x367CD507,
  0x3070DD17, 0xF70E5939,
  0xFFC00B31, 0x68581511,
  0x64F98FA7, 0xBEFA4FA4
};
static const uint64_t IV48[8]={
  0xCBBB9D5DC1059ED8ULL, 0x629A292A367CD507ULL,
  0x9159015A3070DD17ULL, 0x152FECD8F70E5939ULL,
  0x67332667FFC00B31ULL, 0x8EB44A8768581511ULL,
  0xDB0C2E0D64F98FA7ULL, 0x47B5481DBEFA4FA4ULL
};
static const uint64_t IV64[8]={
  0x6A09E667F3BCC908ULL, 0xBB67AE8584CAA73BULL,
  0x3C6EF372FE94F82BULL, 0xA54FF53A5F1D36F1ULL,
  0x510E527FADE682D1ULL, 0x9B05688C2B3E6C1FULL,
  0x1F83D9ABFB41BD6BULL, 0x5BE0CD19137E2179ULL
};


static HashReturn compress32( hashState * state, const BitSequence * datablock )
{

  uint32_t v[16];
  uint32_t m[16];
  int round;

#define ROT32(x,n) (((x)<<(32-n))|( (x)>>(n)))
#define ADD32(x,y)   ((uint32_t)((x) + (y)))
#define XOR32(x,y)    ((uint32_t)((x) ^ (y)))
  
#define G32(a,b,c,d,i)\
  do { \
    v[a] = ADD32(v[a],v[b])+XOR32(m[sigma[round][2*i]], c32[sigma[round][2*i+1]]);\
    v[d] = ROT32(XOR32(v[d],v[a]),16);\
    v[c] = ADD32(v[c],v[d]);\
    v[b] = ROT32(XOR32(v[b],v[c]),12);\
    v[a] = ADD32(v[a],v[b])+XOR32(m[sigma[round][2*i+1]], c32[sigma[round][2*i]]);\
    v[d] = ROT32(XOR32(v[d],v[a]), 8);\
    v[c] = ADD32(v[c],v[d]);\
    v[b] = ROT32(XOR32(v[b],v[c]), 7);\
  } while (0)

  /* get message */
  m[ 0] = U8TO32_BE(datablock + 0);
  m[ 1] = U8TO32_BE(datablock + 4);
  m[ 2] = U8TO32_BE(datablock + 8);
  m[ 3] = U8TO32_BE(datablock +12);
  m[ 4] = U8TO32_BE(datablock +16);
  m[ 5] = U8TO32_BE(datablock +20);
  m[ 6] = U8TO32_BE(datablock +24);
  m[ 7] = U8TO32_BE(datablock +28);
  m[ 8] = U8TO32_BE(datablock +32);
  m[ 9] = U8TO32_BE(datablock +36);
  m[10] = U8TO32_BE(datablock +40);
  m[11] = U8TO32_BE(datablock +44);
  m[12] = U8TO32_BE(datablock +48);
  m[13] = U8TO32_BE(datablock +52);
  m[14] = U8TO32_BE(datablock +56);
  m[15] = U8TO32_BE(datablock +60);

  /* initialization */
  v[ 0] = state->h32[0];
  v[ 1] = state->h32[1];
  v[ 2] = state->h32[2];
  v[ 3] = state->h32[3];
  v[ 4] = state->h32[4];
  v[ 5] = state->h32[5];
  v[ 6] = state->h32[6];
  v[ 7] = state->h32[7];
  v[ 8] = state->salt32[0] ^ c32[0];
  v[ 9] = state->salt32[1] ^ c32[1];
  v[10] = state->salt32[2] ^ c32[2];
  v[11] = state->salt32[3] ^ c32[3];
  if (state->nullt) { /* special case t=0 for the last block */
    v[12] =  c32[4];
    v[13] =  c32[5];
    v[14] =  c32[6];
    v[15] =  c32[7];
  }
  else {
    v[12] = state->t32[0] ^ c32[4];
    v[13] = state->t32[0] ^ c32[5];
    v[14] = state->t32[1] ^ c32[6];
    v[15] = state->t32[1] ^ c32[7];
  }

  /*  do 10 rounds */
  for(round=0; round<NB_ROUNDS32; ++round) {

    /* column step */
    G32( 0, 4, 8,12, 0);
    G32( 1, 5, 9,13, 1);
    G32( 2, 6,10,14, 2);
    G32( 3, 7,11,15, 3);    

    /* diagonal step */
    G32( 0, 5,10,15, 4);
    G32( 1, 6,11,12, 5);
    G32( 2, 7, 8,13, 6);
    G32( 3, 4, 9,14, 7);

  }

  /* finalization */
  state->h32[0] ^= v[ 0]^v[ 8]^state->salt32[0];
  state->h32[1] ^= v[ 1]^v[ 9]^state->salt32[1];
  state->h32[2] ^= v[ 2]^v[10]^state->salt32[2];
  state->h32[3] ^= v[ 3]^v[11]^state->salt32[3];
  state->h32[4] ^= v[ 4]^v[12]^state->salt32[0];
  state->h32[5] ^= v[ 5]^v[13]^state->salt32[1];
  state->h32[6] ^= v[ 6]^v[14]^state->salt32[2];
  state->h32[7] ^= v[ 7]^v[15]^state->salt32[3];

  return SUCCESS;
}


static HashReturn compress64( hashState * state, const BitSequence * datablock )
{

  uint64_t v[16];
  uint64_t m[16];
  int round;

#define ROT64(x,n) (((x)<<(64-n))|( (x)>>(n)))
#define ADD64(x,y)   ((uint64_t)((x) + (y)))
#define XOR64(x,y)    ((uint64_t)((x) ^ (y)))
  
#define G64(a,b,c,d,i)\
  do { \
    v[a] = ADD64(v[a],v[b])+XOR64(m[sigma[round][2*i]], c64[sigma[round][2*i+1]]);\
    v[d] = ROT64(XOR64(v[d],v[a]),32);\
    v[c] = ADD64(v[c],v[d]);\
    v[b] = ROT64(XOR64(v[b],v[c]),25);\
    v[a] = ADD64(v[a],v[b])+XOR64(m[sigma[round][2*i+1]], c64[sigma[round][2*i]]);\
    v[d] = ROT64(XOR64(v[d],v[a]),16);\
    v[c] = ADD64(v[c],v[d]);\
    v[b] = ROT64(XOR64(v[b],v[c]),11);\
  } while (0)

  /* get message */
  m[ 0] = U8TO64_BE(datablock +  0);
  m[ 1] = U8TO64_BE(datablock +  8);
  m[ 2] = U8TO64_BE(datablock + 16);
  m[ 3] = U8TO64_BE(datablock + 24);
  m[ 4] = U8TO64_BE(datablock + 32);
  m[ 5] = U8TO64_BE(datablock + 40);
  m[ 6] = U8TO64_BE(datablock + 48);
  m[ 7] = U8TO64_BE(datablock + 56);
  m[ 8] = U8TO64_BE(datablock + 64);
  m[ 9] = U8TO64_BE(datablock + 72);
  m[10] = U8TO64_BE(datablock + 80);
  m[11] = U8TO64_BE(datablock + 88);
  m[12] = U8TO64_BE(datablock + 96);
  m[13] = U8TO64_BE(datablock +104);
  m[14] = U8TO64_BE(datablock +112);
  m[15] = U8TO64_BE(datablock +120);

  /* initialization */
  v[ 0] = state->h64[0];
  v[ 1] = state->h64[1];
  v[ 2] = state->h64[2];
  v[ 3] = state->h64[3];
  v[ 4] = state->h64[4];
  v[ 5] = state->h64[5];
  v[ 6] = state->h64[6];
  v[ 7] = state->h64[7];
  v[ 8] = state->salt64[0] ^ c64[0];
  v[ 9] = state->salt64[1] ^ c64[1];
  v[10] = state->salt64[2] ^ c64[2];
  v[11] = state->salt64[3] ^ c64[3];
  if (state->nullt) { 
    v[12] =  c64[4];
    v[13] =  c64[5];
    v[14] =  c64[6];
    v[15] =  c64[7];
  }
  else {
    v[12] = state->t64[0] ^ c64[4];
    v[13] = state->t64[0] ^ c64[5];
    v[14] = state->t64[1] ^ c64[6];
    v[15] = state->t64[1] ^ c64[7];
  }  

  /*  do 14 rounds */
  for(round=0; round<NB_ROUNDS64; ++round) {

    /* column step */
    G64( 0, 4, 8,12, 0);
    G64( 1, 5, 9,13, 1);
    G64( 2, 6,10,14, 2);
    G64( 3, 7,11,15, 3);    
    /* diagonal step */
    G64( 0, 5,10,15, 4);
    G64( 1, 6,11,12, 5);
    G64( 2, 7, 8,13, 6);
    G64( 3, 4, 9,14, 7);
  }


  /* finalization */
  state->h64[0] ^= v[ 0]^v[ 8]^state->salt64[0];
  state->h64[1] ^= v[ 1]^v[ 9]^state->salt64[1];
  state->h64[2] ^= v[ 2]^v[10]^state->salt64[2];
  state->h64[3] ^= v[ 3]^v[11]^state->salt64[3];
  state->h64[4] ^= v[ 4]^v[12]^state->salt64[0];
  state->h64[5] ^= v[ 5]^v[13]^state->salt64[1];
  state->h64[6] ^= v[ 6]^v[14]^state->salt64[2];
  state->h64[7] ^= v[ 7]^v[15]^state->salt64[3];

  return SUCCESS;
}



HashReturn Init( hashState * state, int hashbitlen )
{

  int i;

  if ( (hashbitlen == 224) || (hashbitlen == 256) )  {
    /* 224- and 256-bit versions (32-bit words) */

    if (hashbitlen == 224) 
      memcpy( state->h32, IV28, sizeof(IV28) );      
    else 
      memcpy( state->h32, IV32, sizeof(IV32) );

    state->t32[0] = 0;
    state->t32[1] = 0;

    for(i=0; i<64; ++i)
      state->data32[i] = 0;

    state->salt32[0] = 0;
    state->salt32[1] = 0;
    state->salt32[2] = 0;
    state->salt32[3] = 0;
     
  }
  else if ( (hashbitlen == 384) || (hashbitlen == 512) ){
    /* 384- and 512-bit versions (64-bit words) */

    if (hashbitlen == 384) 
      memcpy( state->h64, IV48, sizeof(IV48) );      
    else 
      memcpy( state->h64, IV64, sizeof(IV64) );

    state->t64[0] = 0;
    state->t64[1] = 0;

    for(i=0; i<64; ++i)
      state->data64[i] = 0;
    
    state->salt64[0] = 0;
    state->salt64[1] = 0;
    state->salt64[2] = 0;
    state->salt64[3] = 0;    

    
  }
  else
    return BAD_HASHBITLEN;

  state->hashbitlen = hashbitlen;
  state->datalen = 0;
  state->init = 1;
  state->nullt = 0;

  return SUCCESS;
}



HashReturn AddSalt( hashState * state, const BitSequence * salt )
{


  /* if hashbitlen=224 or 256, then the salt should be 128-bit (16 bytes) */
  /* if hashbitlen=384 or 512, then the salt should be 256-bit (32 bytes) */

  /* fail if Init() was not called before */
  if (state->init != 1) 
    return FAIL;

  if ( state->hashbitlen < 384 ) {
    state->salt32[0] = U8TO32_BE(salt + 0);
    state->salt32[1] = U8TO32_BE(salt + 4);
    state->salt32[2] = U8TO32_BE(salt + 8);
    state->salt32[3] = U8TO32_BE(salt +12);
  }
  else {
    state->salt64[0] = U8TO64_BE(salt + 0);
    state->salt64[1] = U8TO64_BE(salt + 8);
    state->salt64[2] = U8TO64_BE(salt +16);
    state->salt64[3] = U8TO64_BE(salt +24);
  }

  return SUCCESS;
}



static HashReturn Update32(hashState * state, const BitSequence * data, DataLength databitlen )
{


  int fill;
  int left; /* to handle data inputs of up to 2^64-1 bits */
  
  if ( ( databitlen == 0 ) && (state->datalen != 512 ) )
    return SUCCESS;

  left = (state->datalen >> 3); 
  fill = 64 - left;

  /* compress remaining data filled with new bits */
  if( left && ( ((databitlen >> 3) & 0x3F) >= fill ) ) {
    memcpy( (void *) (state->data32 + left),
	    (void *) data, fill );
    /* update counter */
    state->t32[0] += 512;
    if (state->t32[0] == 0)
      state->t32[1]++;
      
    compress32( state, state->data32 );
    data += fill;
    databitlen  -= (fill << 3); 
      
    left = 0;
  }

  /* compress data until enough for a block */
  while( databitlen >= 512 ) {

    /* update counter */
    state->t32[0] += 512;

    if (state->t32[0] == 0)
      state->t32[1]++;
    compress32( state, data );
    data += 64;
    databitlen  -= 512;
  }
  
  if( databitlen > 0 ) {
    memcpy( (void *) (state->data32 + left),
	    (void *) data, databitlen>>3 );
    state->datalen = (left<<3) + databitlen;
    /* when non-8-multiple, add remaining bits (1 to 7)*/
    if ( databitlen & 0x7 )
      state->data32[left + (databitlen>>3)] = data[databitlen>>3];
  }
  else
    state->datalen=0;


  return SUCCESS;
}

static HashReturn Update64(hashState * state, const BitSequence * data, DataLength databitlen )
{


  int fill;
  int left;

  if ( ( databitlen == 0 ) && (state->datalen != 1024 ) )
    return SUCCESS;

  left = (state->datalen >> 3);
  fill = 128 - left;

  /* compress remaining data filled with new bits */
  if( left && ( ((databitlen >> 3) & 0x7F) >= fill ) ) {
    memcpy( (void *) (state->data64 + left),
	    (void *) data, fill );
    /* update counter  */
   state->t64[0] += 1024;

   compress64( state, state->data64 );
   data += fill;
   databitlen  -= (fill << 3); 
      
    left = 0;
  }

  /* compress data until enough for a block */
  while( databitlen >= 1024 ) {
  
    /* update counter */
   state->t64[0] += 1024;
   compress64( state, data );
    data += 128;
    databitlen  -= 1024;
  }

  if( databitlen > 0 ) {
    memcpy( (void *) (state->data64 + left),
	    (void *) data, ( databitlen>>3 ) & 0x7F );
    state->datalen = (left<<3) + databitlen;

    /* when non-8-multiple, add remaining bits (1 to 7)*/
    if ( databitlen & 0x7 )
      state->data64[left + (databitlen>>3)] = data[databitlen>>3];
  }
  else
    state->datalen=0;

  return SUCCESS;
}


HashReturn Update(hashState * state, const BitSequence * data, DataLength databitlen )
{

  if ( state->hashbitlen < 384 )
    return Update32( state, data, databitlen );
  else
    return Update64( state, data, databitlen );
}


static HashReturn Final32( hashState * state, BitSequence * hashval )
{


  unsigned char msglen[8];
  BitSequence zz=0x00,zo=0x01,oz=0x80,oo=0x81;

  /* 
     copy nb. bits hash in total as a 64-bit BE word
  */
  uint32_t low, high;
  low  = state->t32[0] + state->datalen;
  high = state->t32[1];
  if ( low < state->datalen )
    high++;
  U32TO8_BE(  msglen + 0, high );
  U32TO8_BE(  msglen + 4, low  );

  if ( state->datalen % 8 == 0) {
    /* message bitlength multiple of 8 */

    if ( state->datalen == 440 ) {
      /* special case of one padding byte */
      state->t32[0] -= 8;
      if ( state->hashbitlen == 224 ) 
	Update32( state, &oz, 8 );
      else
	Update32( state, &oo, 8 );
    }
    else {
      if ( state->datalen < 440 ) {
	/* use t=0 if no remaining data */
	if ( state->datalen == 0 ) 
	  state->nullt=1;
	/* enough space to fill the block  */
	state->t32[0] -= 440 - state->datalen;
	Update32( state, padding, 440 - state->datalen );
      }
      else {
	/* NOT enough space, need 2 compressions */
	state->t32[0] -= 512 - state->datalen;
	Update32( state, padding, 512 - state->datalen );
	state->t32[0] -= 440;
	Update32( state, padding+1, 440 );  /* padd with zeroes */
	state->nullt = 1; /* raise flag to set t=0 at the next compress */
      }
      if ( state->hashbitlen == 224 ) 
	Update32( state, &zz, 8 );
      else
	Update32( state, &zo, 8 );
      state->t32[0] -= 8;
    }
    state->t32[0] -= 64;
    Update32( state, msglen, 64 );    
  }
  else {  
    /* message bitlength NOT multiple of 8 */

    /*  add '1' */
    state->data32[state->datalen/8] &= (0xFF << (8-state->datalen%8)); 
    state->data32[state->datalen/8] ^= (0x80 >> (state->datalen%8)); 

    if (( state->datalen > 440 ) && ( state->datalen < 447 )) {
      /*  special case of one padding byte */
      if ( state->hashbitlen == 224 ) 
	state->data32[state->datalen/8] ^= 0x00;
      else
	state->data32[state->datalen/8] ^= 0x01;
      state->t32[0] -= (8 - (state->datalen%8));
      /* set datalen to a 8 multiple */
      state->datalen = (state->datalen&(DataLength)0xfffffffffffffff8ULL)+8;
    }
    else { 
      if (state->datalen < 440) {
	/* enough space to fill the block */
	state->t32[0] -= 440 - state->datalen;
	state->datalen = (state->datalen&(DataLength)0xfffffffffffffff8ULL)+8;
	Update( state, padding+1, 440 - state->datalen );
      }
      else { 
	if (state->datalen > 504 ) {
	  /* special case */
	  state->t32[0] -= 512 - state->datalen;
	  state->datalen=512;
	  Update32( state, padding+1, 0 );
	  state->t32[0] -= 440;
	  Update32( state, padding+1, 440 );
	  state->nullt = 1; /* raise flag for t=0 at the next compress */
	}
	else {
	  /* NOT enough space, need 2 compressions */
	  state->t32[0] -= 512 - state->datalen;
	  /* set datalen to a 8 multiple */
	  state->datalen = (state->datalen&(DataLength)0xfffffffffffffff8ULL)+8;
	  Update32( state, padding+1, 512 - state->datalen );
	  state->t32[0] -= 440;
	  Update32( state, padding+1, 440 );
	  state->nullt = 1; /* raise flag for t=0 at the next compress */
	}
      }
      state->t32[0] -= 8;
      if ( state->hashbitlen == 224 ) 
	Update32( state, &zz, 8 );
      else
	Update32( state, &zo, 8 );
    }
    state->t32[0] -= 64;
    Update32( state, msglen, 64 ); 
  }

  U32TO8_BE( hashval + 0, state->h32[0]);
  U32TO8_BE( hashval + 4, state->h32[1]);
  U32TO8_BE( hashval + 8, state->h32[2]);
  U32TO8_BE( hashval +12, state->h32[3]);
  U32TO8_BE( hashval +16, state->h32[4]);
  U32TO8_BE( hashval +20, state->h32[5]);
  U32TO8_BE( hashval +24, state->h32[6]);

  if ( state->hashbitlen == 256 ) {
    U32TO8_BE( hashval +28, state->h32[7]);
  }
  
  return SUCCESS;
}


static HashReturn Final64( hashState * state, BitSequence * hashval )
{


  unsigned char msglen[16];
  BitSequence zz=0x00,zo=0x01,oz=0x80,oo=0x81;

  /* copy nb. bits hash in total as a 128-bit BE word */
  uint64_t low, high;
  low  = state->t64[0] + state->datalen;
  high = state->t64[1];
  if ( low < state->datalen )
    high++;
  U64TO8_BE(  msglen + 0, high );
  U64TO8_BE(  msglen + 8, low  );

  if ( state->datalen % 8 == 0) {
    /* message bitlength multiple of 8 */

    if ( state->datalen == 888 ) {
      /* special case of one padding byte */
      state->t64[0] -= 8; 
      if ( state->hashbitlen == 384 ) 
	Update64( state, &oz, 8 );
      else
	Update64( state, &oo, 8 );
    }
    else {
      if ( state->datalen < 888 ) {
	/* use t=0 if no remaining data */
	if ( state->datalen == 0 ) 
	  state->nullt=1;
	/* enough space to fill the block */
	state->t64[0] -= 888 - state->datalen;
	Update64( state, padding, 888 - state->datalen );
      }
      else { 
	/* NOT enough space, need 2 compressions */
	state->t64[0] -= 1024 - state->datalen; 
	Update64( state, padding, 1024 - state->datalen );
	state->t64[0] -= 888;
	Update64( state, padding+1, 888 );  /* padd with zeros */
	state->nullt = 1; /* raise flag to set t=0 at the next compress */
      }
      if ( state->hashbitlen == 384 ) 
	Update64( state, &zz, 8 );
      else
	Update( state, &zo, 8 );
      state->t64[0] -= 8;
    }
    state->t64[0] -= 128;
    Update( state, msglen, 128 );    
  }
  else {  
    /* message bitlength NOT multiple of 8 */

    /* add '1' */
    state->data64[state->datalen/8] &= (0xFF << (8-state->datalen%8)); 
    state->data64[state->datalen/8] ^= (0x80 >> (state->datalen%8)); 

    if (( state->datalen > 888 ) && ( state->datalen < 895 )) {
      /*  special case of one padding byte */
      if ( state->hashbitlen == 384 ) 
	state->data64[state->datalen/8] ^= zz;
      else
	state->data64[state->datalen/8] ^= zo;
      state->t64[0] -= (8 - (state->datalen%8));
      /* set datalen to a 8 multiple */
      state->datalen = (state->datalen&(DataLength)0xfffffffffffffff8ULL)+8;
    }
    else { 
      if (state->datalen < 888) {
	/* enough space to fill the block */
	state->t64[0] -= 888 - state->datalen;
	state->datalen = (state->datalen&(DataLength)0xfffffffffffffff8ULL)+8;
	Update64( state, padding+1, 888 - state->datalen );
      }
      else {
	if (state->datalen > 1016 ) {
	  /* special case */
	  state->t64[0] -= 1024 - state->datalen;
	  state->datalen=1024;
	  Update64( state, padding+1, 0 );
	  state->t64[0] -= 888;
	  Update64( state, padding+1, 888 );
	  state->nullt = 1; /* raise flag for t=0 at the next compress */
	}
	else {
	  /* NOT enough space, need 2 compressions */
	  state->t64[0] -= 1024 - state->datalen;
	  /* set datalen to a 8 multiple */
	  state->datalen = (state->datalen&(DataLength)0xfffffffffffffff8ULL)+8;
	  Update64( state, padding+1, 1024 - state->datalen );
	  state->t64[0] -= 888;
	  Update64( state, padding+1, 888 );
	  state->nullt = 1; /* raise flag for t=0 at the next compress */
	}
      }
      state->t64[0] -= 8;
      if ( state->hashbitlen == 384 ) 
	Update64( state, &zz, 8 );
      else
	Update64( state, &zo, 8 );
    }
    state->t64[0] -= 128;
    Update( state, msglen, 128 ); 
  }

  U64TO8_BE( hashval + 0, state->h64[0]);
  U64TO8_BE( hashval + 8, state->h64[1]);
  U64TO8_BE( hashval +16, state->h64[2]);
  U64TO8_BE( hashval +24, state->h64[3]);
  U64TO8_BE( hashval +32, state->h64[4]);
  U64TO8_BE( hashval +40, state->h64[5]);

  if ( state->hashbitlen == 512 ) {
    U64TO8_BE( hashval +48, state->h64[6]);
    U64TO8_BE( hashval +56, state->h64[7]);
  }
  
  return SUCCESS;
}

HashReturn Final( hashState * state, BitSequence * hashval )
{
  
  if ( state->hashbitlen < 384 )
    return Final32( state, hashval );
  else
    return Final64( state, hashval );
}

HashReturn Hash( int hashbitlen, const BitSequence * data, DataLength databitlen, 
		 BitSequence * hashval )
{

  HashReturn ret;
  hashState state;

  ret = Init( &state, hashbitlen );
  if ( ret != SUCCESS ) return ret;

  ret = Update( &state, data, databitlen );
  if ( ret != SUCCESS ) return ret;

  ret = Final( &state, hashval );

 return ret;

}


/* uncomment below for test vectors */
/*
int main()
{

  int i;
  BitSequence data[144]; 
  BitSequence hash[64];

  for(i=0; i<144; ++i)
    data[i]=0;

  printf("\none-block message:\n");

  printf("\nBLAKE-32\n");
  Hash( 256, data, 8, hash );    
  for(i=0; i<32; ++i)
    printf("%02X", hash[i]);
  printf("\n");
  printf("\nBLAKE-28\n");
  Hash( 224, data, 8, hash );    
  for(i=0; i<28; ++i)
    printf("%02X", hash[i]);
  printf("\n");
  printf("\nBLAKE-64\n");
  Hash( 512, data, 8, hash );    
  for(i=0; i<64; ++i)
    printf("%02X", hash[i]);
  printf("\n");
  printf("\nBLAKE-48\n");
  Hash( 384, data, 8, hash );    
  for(i=0; i<48; ++i)
    printf("%02X", hash[i]);
  printf("\n");

  printf("\ntwo-block message:\n");

  printf("\nBLAKE-32\n");
  Hash( 256, data, 576, hash );    
  for(i=0; i<32; ++i)
    printf("%02X", hash[i]);
  printf("\n");
  printf("\nBLAKE-28\n");
  Hash( 224, data, 576, hash );    
  for(i=0; i<28; ++i)
    printf("%02X", hash[i]);
  printf("\n");
  printf("\nBLAKE-64\n");
  Hash( 512, data, 1152, hash );    
  for(i=0; i<64; ++i)
    printf("%02X", hash[i]);
  printf("\n");
  printf("\nBLAKE-48\n");
  Hash( 384, data, 1152, hash );    
  for(i=0; i<48; ++i)
    printf("%02X", hash[i]);
  printf("\n");

  return 0;
}
*/

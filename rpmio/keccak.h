/*
Algorithm Name: Keccak
Authors: Guido Bertoni, Joan Daemen, Michaël Peeters and Gilles Van Assche
Date: April 21, 2009

This code, originally by Guido Bertoni, Joan Daemen, Michaël Peeters and
Gilles Van Assche as a part of the SHA-3 submission, is hereby put in the
public domain. It is given as is, without any guarantee.

For more information, feedback or questions, please refer to our website:
http://keccak.noekeon.org/
*/

#ifndef _KeccakNISTInterface_h_
#define _KeccakNISTInterface_h_

#define KeccakPermutationSize 1600
#define KeccakPermutationSizeInBytes (KeccakPermutationSize/8)
#define KeccakMaximumRate 1024
#define KeccakMaximumRateInBytes (KeccakMaximumRate/8)

#if defined(__GNUC__)
#define ALIGN __attribute__ ((aligned(32)))
#elif defined(_MSC_VER)
#define ALIGN __declspec(align(32))
#else
#define ALIGN
#endif

ALIGN typedef struct {
    ALIGN unsigned char state[KeccakPermutationSizeInBytes];
    ALIGN unsigned char dataQueue[KeccakMaximumRateInBytes];
    unsigned int rate;
    unsigned int capacity;
    unsigned char diversifier;
    unsigned int hashbitlen;
    unsigned int bitsInQueue;
    int squeezing;
    unsigned int bitsAvailableForSqueezing;
} keccak_hashState;

int keccak_Init(keccak_hashState *state, int hashbitlen);
int keccak_Update(keccak_hashState *state, const void *_data, size_t _len);
int keccak_Final(keccak_hashState *state, unsigned char *_hashval);
int keccak_Squeeze(keccak_hashState *state, unsigned char *_output, unsigned long long _outputLength);

int keccak_Hash(int hashbitlen, const void *_data, size_t _len, unsigned char *hashval);

#endif

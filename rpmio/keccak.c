/*
Algorithm Name: Keccak
Authors: Guido Bertoni, Joan Daemen, Michaël Peeters and Gilles Van Assche
Date: January 9, 2009

This code, originally by Guido Bertoni, Joan Daemen, Michaël Peeters and
Gilles Van Assche as a part of the SHA-3 submission, is hereby put in the
public domain. It is given as is, without any guarantee.

For more information, feedback or questions, please refer to our website:
http://keccak.noekeon.org/
*/

#include <string.h>
#include "keccak.h"

typedef enum { SUCCESS = 0, FAIL = 1, BAD_HASHLEN = 2 } HashReturn;

/* ===== "KeccakPermutationInterface.h" */

/*
Algorithm Name: Keccak
Authors: Guido Bertoni, Joan Daemen, Michaël Peeters and Gilles Van Assche
Date: January 9, 2009

This code, originally by Guido Bertoni, Joan Daemen, Michaël Peeters and
Gilles Van Assche as a part of the SHA-3 submission, is hereby put in the
public domain. It is given as is, without any guarantee.

For more information, feedback or questions, please refer to our website:
http://keccak.noekeon.org/
*/

#ifndef _KeccakPermutationInterface_h_
#define _KeccakPermutationInterface_h_

void KeccakInitialize(void);
void KeccakInitializeState(unsigned char *state);
void KeccakPermutation(unsigned char *state);
void KeccakAbsorb1024bits(unsigned char *state, const unsigned char *data);
void KeccakAbsorb512bits(unsigned char *state, const unsigned char *data);
void KeccakExtract1024bits(const unsigned char *state, unsigned char *data);
void KeccakExtract512bits(const unsigned char *state, unsigned char *data);

#endif

/* ===== */

#ifdef KeccakReference
#include "displayIntermediateValues.h"
#endif

/* ===== "displayIntermediateValues.h" */
/*
Algorithm Name: Keccak
Authors: Guido Bertoni, Joan Daemen, Michaël Peeters and Gilles Van Assche
Date: October 27, 2008

This code, originally by Guido Bertoni, Joan Daemen, Michaël Peeters and
Gilles Van Assche as a part of the SHA-3 submission, is hereby put in the
public domain. It is given as is, without any guarantee.

For more information, feedback or questions, please refer to our website:
http://keccak.noekeon.org/
*/

#ifndef _displayIntermediateValues_h_
#define _displayIntermediateValues_h_

#include <stdio.h>

void displaySetIntermediateValueFile(FILE *f);
void displaySetLevel(int level);
void displayBytes(int level, const char *text, const unsigned char *bytes, unsigned int size);
void displayBits(int level, const char *text, const unsigned char *data, unsigned int size, int MSBfirst);
void displayStateAsBytes(int level, const char *text, const unsigned char *state);
void displayStateAsWords(int level, const char *text, const unsigned long long int *state);
void displayRoundNumber(int level, unsigned int i);
void displayText(int level, const char *text);

#endif

/* ===== */

/* ===== "displayIntermediateValues.h" */
/*
Algorithm Name: Keccak
Authors: Guido Bertoni, Joan Daemen, Michaël Peeters and Gilles Van Assche
Date: October 27, 2008

This code, originally by Guido Bertoni, Joan Daemen, Michaël Peeters and
Gilles Van Assche as a part of the SHA-3 submission, is hereby put in the
public domain. It is given as is, without any guarantee.

For more information, feedback or questions, please refer to our website:
http://keccak.noekeon.org/
*/

#include <stdio.h>

FILE *intermediateValueFile = 0;
int displayLevel = 0;

void displaySetIntermediateValueFile(FILE *f)
{
    intermediateValueFile = f;
}

void displaySetLevel(int level)
{
    displayLevel = level;
}

void displayBytes(int level, const char *text, const unsigned char *bytes, unsigned int size)
{
    unsigned int i;

    if ((intermediateValueFile) && (level <= displayLevel)) {
        fprintf(intermediateValueFile, "%s:\n", text);
        for(i=0; i<size; i++)
            fprintf(intermediateValueFile, "%02X ", bytes[i]);
        fprintf(intermediateValueFile, "\n");
        fprintf(intermediateValueFile, "\n");
    }
}

void displayBits(int level, const char *text, const unsigned char *data, unsigned int size, int MSBfirst)
{
    unsigned int i, iByte, iBit;

    if ((intermediateValueFile) && (level <= displayLevel)) {
        fprintf(intermediateValueFile, "%s:\n", text);
        for(i=0; i<size; i++) {
            iByte = i/8;
            iBit = i%8;
            if (MSBfirst)
                fprintf(intermediateValueFile, "%d ", ((data[iByte] << iBit) & 0x80) != 0);
            else
                fprintf(intermediateValueFile, "%d ", ((data[iByte] >> iBit) & 0x01) != 0);
        }
        fprintf(intermediateValueFile, "\n");
        fprintf(intermediateValueFile, "\n");
    }
}

void displayStateAsBytes(int level, const char *text, const unsigned char *state)
{
    displayBytes(level, text, state, KeccakPermutationSizeInBytes);
}

void displayStateAsWords(int level, const char *text, const unsigned long long int *state)
{
    unsigned int i;

    if ((intermediateValueFile) && (level <= displayLevel)) {
        fprintf(intermediateValueFile, "%s:\n", text);
        for(i=0; i<KeccakPermutationSize/64; i++) {
            fprintf(intermediateValueFile, "%08X", (unsigned int)(state[i] >> 32));
            fprintf(intermediateValueFile, "%08X", (unsigned int)(state[i] & 0xFFFFFFFFULL));
            if ((i%5) == 4)
                fprintf(intermediateValueFile, "\n");
            else
                fprintf(intermediateValueFile, " ");
        }
    }
}

void displayRoundNumber(int level, unsigned int i)
{
    if ((intermediateValueFile) && (level <= displayLevel)) {
        fprintf(intermediateValueFile, "\n");
        fprintf(intermediateValueFile, "--- Round %d ---\n", i);
        fprintf(intermediateValueFile, "\n");
    }
}

void displayText(int level, const char *text)
{
    if ((intermediateValueFile) && (level <= displayLevel)) {
        fprintf(intermediateValueFile, "%s", text);
        fprintf(intermediateValueFile, "\n");
        fprintf(intermediateValueFile, "\n");
    }
}

/* ===== */

/* ===== "KeccakPermutationReference.c" */

/*
Algorithm Name: Keccak
Authors: Guido Bertoni, Joan Daemen, Michaël Peeters and Gilles Van Assche
Date: January 9, 2009

This code, originally by Guido Bertoni, Joan Daemen, Michaël Peeters and
Gilles Van Assche as a part of the SHA-3 submission, is hereby put in the
public domain. It is given as is, without any guarantee.

For more information, feedback or questions, please refer to our website:
http://keccak.noekeon.org/
*/

#include <stdio.h>
#include <string.h>

typedef unsigned char UINT8;
typedef unsigned long long int UINT64;

#define nrRounds 18
UINT64 KeccakRoundConstants[nrRounds];
#define nrWords 25
unsigned int KeccakRhoOffsets[nrWords];

void KeccakPermutationOnWords(UINT64 *state);
void theta(UINT64 *A);
void rho(UINT64 *A);
void pi(UINT64 *A);
void chi(UINT64 *A);
void iota(UINT64 *A, unsigned int indexRound);

static void fromBytesToWords(UINT64 *stateAsWords, const unsigned char *state)
{
    unsigned int i, j;

    for(i=0; i<(KeccakPermutationSize/64); i++) {
        stateAsWords[i] = 0;
        for(j=0; j<(64/8); j++)
            stateAsWords[i] |= (UINT64)(state[i*(64/8)+j]) << (8*j);
    }
}

static void fromWordsToBytes(unsigned char *state, const UINT64 *stateAsWords)
{
    unsigned int i, j;

    for(i=0; i<(KeccakPermutationSize/64); i++)
        for(j=0; j<(64/8); j++)
            state[i*(64/8)+j] = (stateAsWords[i] >> (8*j)) & 0xFF;
}

void KeccakPermutation(unsigned char *state)
{
#if (PLATFORM_BYTE_ORDER != IS_LITTLE_ENDIAN)
    UINT64 stateAsWords[KeccakPermutationSize/64];
#endif

    displayStateAsBytes(1, "Input of permutation", state);
#if (PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN)
    KeccakPermutationOnWords((UINT64*)state);
#else
    fromBytesToWords(stateAsWords, state);
    KeccakPermutationOnWords(stateAsWords);
    fromWordsToBytes(state, stateAsWords);
#endif
    displayStateAsBytes(1, "State after permutation", state);
}

static void KeccakPermutationAfterXor(unsigned char *state, const unsigned char *data, unsigned int dataLengthInBytes)
{
    unsigned int i;

    for(i=0; i<dataLengthInBytes; i++)
        state[i] ^= data[i];
    KeccakPermutation(state);
}

void KeccakPermutationOnWords(UINT64 *state)
{
    unsigned int i;

    displayStateAsWords(3, "Same, as words", state);

    for(i=0; i<nrRounds; i++) {
        displayRoundNumber(3, i);

        theta(state);
        displayStateAsWords(3, "After theta", state);

        rho(state);
        displayStateAsWords(3, "After rho", state);

        pi(state);
        displayStateAsWords(3, "After pi", state);

        chi(state);
        displayStateAsWords(3, "After chi", state);

        iota(state, i);
        displayStateAsWords(3, "After iota", state);
    }
}

#define index(x, y) (((x)%5)+5*((y)%5))
#define ROL64(a, offset) ((offset != 0) ? ((((UINT64)a) << offset) ^ (((UINT64)a) >> (64-offset))) : a)

void theta(UINT64 *A)
{
    unsigned int x, y;
    UINT64 C[5], D[5];

    for(x=0; x<5; x++) {
        C[x] = 0; 
        for(y=0; y<5; y++) 
            C[x] ^= A[index(x, y)];
        D[x] = ROL64(C[x], 1);
    }
    for(x=0; x<5; x++)
        for(y=0; y<5; y++)
            A[index(x, y)] ^= D[(x+1)%5] ^ C[(x+4)%5];
}

void rho(UINT64 *A)
{
    unsigned int x, y;

    for(x=0; x<5; x++) for(y=0; y<5; y++)
        A[index(x, y)] = ROL64(A[index(x, y)], KeccakRhoOffsets[index(x, y)]);
}

void pi(UINT64 *A)
{
    unsigned int x, y;
    UINT64 tempA[25];

    for(x=0; x<5; x++) for(y=0; y<5; y++)
        tempA[index(x, y)] = A[index(x, y)];
    for(x=0; x<5; x++) for(y=0; y<5; y++)
        A[index(0*x+1*y, 2*x+3*y)] = tempA[index(x, y)];
}

void chi(UINT64 *A)
{
    unsigned int x, y;
    UINT64 C[5];

    for(y=0; y<5; y++) { 
        for(x=0; x<5; x++)
            C[x] = A[index(x, y)] ^ ((~A[index(x+1, y)]) & A[index(x+2, y)]);
        for(x=0; x<5; x++)
            A[index(x, y)] = C[x];
    }
}

void iota(UINT64 *A, unsigned int indexRound)
{
    A[index(0, 0)] ^= KeccakRoundConstants[indexRound];
}

static int LFSR86540(UINT8 *LFSR)
{
    int result = ((*LFSR) & 0x01) != 0;
    if (((*LFSR) & 0x80) != 0)
        // Primitive polynomial over GF(2): x^8+x^6+x^5+x^4+1
        (*LFSR) = ((*LFSR) << 1) ^ 0x71;
    else
        (*LFSR) <<= 1;
    return result;
}

static void KeccakInitializeRoundConstants(void)
{
    UINT8 LFSRstate = 0x01;
    unsigned int i, j, bitPosition;

    for(i=0; i<nrRounds; i++) {
        KeccakRoundConstants[i] = 0;
        for(j=0; j<7; j++) {
            bitPosition = (1<<j)-1; //2^j-1
            if (LFSR86540(&LFSRstate))
                KeccakRoundConstants[i] ^= (UINT64)1<<bitPosition;
        }
    }
}

static void KeccakInitializeRhoOffsets(void)
{
    unsigned int x, y, t, newX, newY;

    KeccakRhoOffsets[index(0, 0)] = 0;
    x = 1;
    y = 0;
    for(t=0; t<24; t++) {
        KeccakRhoOffsets[index(x, y)] = ((t+1)*(t+2)/2) % 64;
        newX = (0*x+1*y) % 5;
        newY = (2*x+3*y) % 5;
        x = newX;
        y = newY;
    }
}

void KeccakInitialize(void)
{
    KeccakInitializeRoundConstants();
    KeccakInitializeRhoOffsets();
}

static void displayRoundConstants(FILE *f)
{
    unsigned int i;

    for(i=0; i<nrRounds; i++) {
        fprintf(f, "RC[%02i][0][0] = ", i);
        fprintf(f, "%08X", (unsigned int) (KeccakRoundConstants[i] >> 32));
        fprintf(f, "%08X", (unsigned int) (KeccakRoundConstants[i] & 0xFFFFFFFFULL));
        fprintf(f, "\n");
    }
    fprintf(f, "\n");
}

static void displayRhoOffsets(FILE *f)
{
    unsigned int x, y;

    for(y=0; y<5; y++) for(x=0; x<5; x++) {
        fprintf(f, "RhoOffset[%i][%i] = ", x, y);
        fprintf(f, "%2i", KeccakRhoOffsets[index(x, y)]);
        fprintf(f, "\n");
    }
    fprintf(f, "\n");
}

void KeccakInitializeState(unsigned char *state)
{
    memset(state, 0, KeccakPermutationSizeInBytes);
}

void KeccakAbsorb1024bits(unsigned char *state, const unsigned char *data)
{
    KeccakPermutationAfterXor(state, data, 128);
}

void KeccakAbsorb512bits(unsigned char *state, const unsigned char *data)
{
    KeccakPermutationAfterXor(state, data, 64);
}

void KeccakExtract1024bits(const unsigned char *state, unsigned char *data)
{
    memcpy(data, state, 128);
}

void KeccakExtract512bits(const unsigned char *state, unsigned char *data)
{
    memcpy(data, state, 64);
}

/* ===== */

int keccak_Init(keccak_hashState *state, int hashbitlen)
{
    KeccakInitialize();
    switch(hashbitlen) {
        case 0: // Arbitrary length output
            state->capacity = 576;
            break;
        case 224:
            state->capacity = 576;
            break;
        case 256:
            state->capacity = 576;
            break;
        case 384:
            state->capacity = 1088;
            break;
        case 512:
            state->capacity = 1088;
            break;
        default:
            return BAD_HASHLEN;
    }
    state->rate = KeccakPermutationSize - state->capacity;
    state->diversifier = hashbitlen/8;
    state->hashbitlen = hashbitlen;
    KeccakInitializeState(state->state);
    memset(state->dataQueue, 0, KeccakMaximumRateInBytes);
    state->bitsInQueue = 0;
    state->squeezing = 0;
    state->bitsAvailableForSqueezing = 0;

    return SUCCESS;
}

static void AbsorbQueue(keccak_hashState *state)
{
    #ifdef KeccakReference
    displayBytes(1, "Data to be absorbed", state->dataQueue, state->bitsInQueue/8);
    #endif
    // state->bitsInQueue is assumed to be equal a multiple of 8
    memset(state->dataQueue+state->bitsInQueue/8, 0, state->rate/8-state->bitsInQueue/8);
    if (state->rate == 1024)
        KeccakAbsorb1024bits(state->state, state->dataQueue);
    else
        KeccakAbsorb512bits(state->state, state->dataQueue);
    state->bitsInQueue = 0;
}

int keccak_Update(keccak_hashState *state, const void *_data, size_t _len)
{
    const unsigned char *data = _data;
    const unsigned long long databitlen = 8 * _len;
    unsigned long long i, j;
    unsigned long long partialBlock, partialByte, wholeBlocks;
    unsigned char lastByte;
    const unsigned char *curData;

    if ((state->bitsInQueue % 8) != 0)
        return FAIL; // Only the last call may contain a partial byte
    if (state->squeezing)
        return FAIL; // Too late for additional input

    i = 0;
    while(i < databitlen) {
        if ((state->bitsInQueue == 0) && (databitlen >= state->rate) && (i <= (databitlen-state->rate))) {
            wholeBlocks = (databitlen-i)/state->rate;
            curData = data+i/8;
            if (state->rate == 1024) {
                for(j=0; j<wholeBlocks; j++, curData+=1024/8) {
                    #ifdef KeccakReference
                    displayBytes(1, "Data to be absorbed", curData, state->rate/8);
                    #endif
                    KeccakAbsorb1024bits(state->state, curData);
                }
            }
            else {
                for(j=0; j<wholeBlocks; j++, curData+=512/8) {
                    #ifdef KeccakReference
                    displayBytes(1, "Data to be absorbed", curData, state->rate/8);
                    #endif
                    KeccakAbsorb512bits(state->state, curData);
                }
            }
            i += wholeBlocks*state->rate;
        }
        else {
            partialBlock = databitlen - i;
            if (partialBlock+state->bitsInQueue > state->rate)
                partialBlock = state->rate-state->bitsInQueue;
            partialByte = partialBlock % 8;
            partialBlock -= partialByte;
            memcpy(state->dataQueue+state->bitsInQueue/8, data+i/8, partialBlock/8);
            state->bitsInQueue += partialBlock;
            i += partialBlock;
            if (state->bitsInQueue == state->rate)
                AbsorbQueue(state);
            if (partialByte > 0) {
                // Align the last partial byte to the least significant bits
                lastByte = data[i/8] >> (8-partialByte);
                state->dataQueue[state->bitsInQueue/8] = lastByte;
                state->bitsInQueue += partialByte;
                i += partialByte;
            }
        }
    }
    return SUCCESS;
}

static void PadAndSwitchToSqueezingPhase(keccak_hashState *state)
{
    if ((state->bitsInQueue % 8) != 0) {
        // The bits are numbered from 0=LSB to 7=MSB
        unsigned char padByte = 1 << (state->bitsInQueue % 8);
        state->dataQueue[state->bitsInQueue/8] |= padByte;
        state->bitsInQueue += 8-(state->bitsInQueue % 8);
    }
    else {
        state->dataQueue[state->bitsInQueue/8] = 0x01;
        state->bitsInQueue += 8;
    }
    if (state->bitsInQueue == state->rate)
        AbsorbQueue(state);
    state->dataQueue[state->bitsInQueue/8] = state->diversifier;
    state->bitsInQueue += 8;
    if (state->bitsInQueue == state->rate)
        AbsorbQueue(state);
    state->dataQueue[state->bitsInQueue/8] = state->rate/8;
    state->bitsInQueue += 8;
    if (state->bitsInQueue == state->rate)
        AbsorbQueue(state);
    state->dataQueue[state->bitsInQueue/8] = 0x01;
    state->bitsInQueue += 8;
    if (state->bitsInQueue > 0)
        AbsorbQueue(state);
    if ((state->rate == 1024) && ((state->hashbitlen > 512) || (state->hashbitlen == 0))) {
        KeccakExtract1024bits(state->state, state->dataQueue);
        state->bitsAvailableForSqueezing = 1024;
    }
    else {
        KeccakExtract512bits(state->state, state->dataQueue);
        state->bitsAvailableForSqueezing = 512;
    }
    state->squeezing = 1;
}

int keccak_Final(keccak_hashState *state, unsigned char *hashval)
{
    if (state->squeezing)
        return FAIL; // Too late, we are already squeezing
    PadAndSwitchToSqueezingPhase(state);
    if (state->hashbitlen > 0)
        memcpy(hashval, state->dataQueue, state->hashbitlen/8);
    return SUCCESS;
}

int keccak_Squeeze(keccak_hashState *state, unsigned char *output, unsigned long long outputLength)
{
    unsigned long long i;
    unsigned long long partialBlock;

    if (!state->squeezing)
        return FAIL; // Too early, we are still absorbing
    if (state->hashbitlen != 0)
        return FAIL; // Arbitrary length output is not permitted in this case
    if ((outputLength % 8) != 0)
        return FAIL; // Only multiple of 8 bits are allowed, truncation can be done at user level

    i = 0;
    while(i < outputLength) {
        if (state->bitsAvailableForSqueezing == 0) {
            KeccakPermutation(state->state);
            if (state->rate == 1024) {
                KeccakExtract1024bits(state->state, state->dataQueue);
                state->bitsAvailableForSqueezing = state->rate;
            }
            else
                return FAIL; // Inconsistent rate
        }
        partialBlock = outputLength - i;
        if (partialBlock > state->bitsAvailableForSqueezing)
            partialBlock = state->bitsAvailableForSqueezing;
        memcpy(output+i/8, state->dataQueue+(state->rate-state->bitsAvailableForSqueezing)/8, partialBlock/8);
        state->bitsAvailableForSqueezing -= partialBlock;
        i += partialBlock;
    }
    return SUCCESS;
}

int keccak_Hash(int hashbitlen, const void *_data, size_t _len, unsigned char *hashval)
{
    keccak_hashState state;
    int result;

    if (hashbitlen == 0)
        return BAD_HASHLEN; // Arbitrary length output not available through this API
    result = keccak_Init(&state, hashbitlen);
    if (result != SUCCESS)
        return result;
    result = keccak_Update(&state, _data, _len);
    if (result != SUCCESS)
        return result;
    result = keccak_Final(&state, hashval);
    return result;
}

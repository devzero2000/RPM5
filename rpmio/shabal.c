/*
 * Implementation of the Shabal hash function (reference code). This
 * code is meant for illustration of the function internals rather than
 * optimization for speed.
 *
 * If this code is compiled with the SHABAL_PRINT_VECTORS macro defined
 * then it produces a stand-alone application which prints out detailed
 * intermediate values for two test messages.
 *
 *
 * (c) 2008 SAPHIR project. This software is provided 'as-is', without
 * any express or implied warranty. In no event will the authors be held
 * liable for any damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to no restriction.
 *
 * Technical remarks and questions can be addressed to:
 * <thomas.pornin@cryptolog.com>
 */

#include <stddef.h>
#include <string.h>
#include "shabal.h"

enum { SUCCESS = 0, FAIL = 1, BAD_HASHBITLEN = 2 };

/*
 * The values of "p" and "e" are defined in the Shabal specification.
 * Manual overrides should be performed only for research purposes,
 * since this would alter the hash output for a given input.
 */
#ifndef SHABAL_PARAM_P
#define SHABAL_PARAM_P   3
#endif
#ifndef SHABAL_PARAM_E
#define SHABAL_PARAM_E   3
#endif

#define sM    SHABAL_BLOCK_SIZE
#define nR    SHABAL_PARAM_R
#define u32   shabal_word32

/*
 * This macro truncates its argument to 32 bits (reduction modulo 2^32).
 */
#define T32(x)   ((x) & (u32)0xFFFFFFFFUL)

#define O1   13
#define O2    9
#define O3    6

#ifdef SHABAL_PRINT_VECTORS

#include <stdio.h>
#include <stdlib.h>

static void
print_array(char *name, u32 *X, int len)
{
	int i;
	size_t u, v;

	u = strlen(name);
	for (i = 0; i < len; i ++) {
		if (i == 0) {
			printf("%s : ", name);
		} else if (i % 8 == 0) {
			printf("\n   ");
			for (v = 0; v < u; v ++)
				printf(" ");
		} else {
			printf(" ");
		}
		printf("%08lX", (unsigned long)X[i]);
	}
	printf("\n\n");
}

static void
print_banner(u32 block_num, char *step)
{
	int bn;

	bn = (block_num == (u32)0xFFFFFFFFUL) ? -1 : (int)block_num;
	printf("block number = %d : %s\n\n", bn, step);
}

static void
print_state(hashState *state, u32 block_num, char *step)
{
	print_banner(block_num, step);
	print_array("A", state->A, nR);
	print_array("B", state->B, sM);
	print_array("C", state->C, sM);
	printf("W : %08lX %08lX\n\n\n",
		(unsigned long)state->Wlow, (unsigned long)state->Whigh);
}

static void
print_message(u32 *m, u32 block_num)
{
	print_banner(block_num, "message block");
	print_array("M", m, sM);
	printf("\n");
}

#endif

/*
 * Swap the B and C state words.
 */
static void
swap_bc(hashState *state)
{
	int i;

	for (i = 0; i < sM; i ++) {
		u32 t;

		t = state->B[i];
		state->B[i] = state->C[i];
		state->C[i] = t;
	}
#ifdef SHABAL_PRINT_VECTORS
	print_state(state, state->Wlow, "swap B with C");
#endif
}

/*
 * Decode the currently accumulated data block (64 bytes) into the m[]
 * array of 16 words.
 */
static void
decode_block(hashState *state, u32 *m)
{
	int i, j;

	for (i = 0, j = 0; i < sM; i ++, j += 4) {
		m[i] = (u32)state->buffer[j]
			+ ((u32)state->buffer[j + 1] << 8)
			+ ((u32)state->buffer[j + 2] << 16)
			+ ((u32)state->buffer[j + 3] << 24);
	}
#ifdef SHABAL_PRINT_VECTORS
	print_message(m, state->Wlow);
#endif
}

/*
 * Add the message block m[] to the B state words.
 */
static void
input_block_add(hashState *state, u32 *m)
{
	int i;

	for (i = 0; i < sM; i ++)
		state->B[i] = T32(state->B[i] + m[i]);
#ifdef SHABAL_PRINT_VECTORS
	print_state(state, state->Wlow, "add M to B");
#endif
}

/*
 * Subtract the message block m[] from the C state words.
 */
static void
input_block_sub(hashState *state, u32 *m)
{
	int i;

	for (i = 0; i < sM; i ++)
		state->C[i] = T32(state->C[i] - m[i]);
#ifdef SHABAL_PRINT_VECTORS
	print_state(state, state->Wlow, "subtract M from C");
#endif
}

/*
 * Combine the block counter with the A state words.
 */
static void
xor_counter(hashState *state)
{
	state->A[0] ^= state->Wlow;
	state->A[1] ^= state->Whigh;
#ifdef SHABAL_PRINT_VECTORS
	print_state(state, state->Wlow, "xor counter W into A");
#endif
}

/*
 * Increment the block counter, for the next block.
 */
static void
incr_counter(hashState *state)
{
	/*
	 * The counter is over two 32-bit words. We manually propagate
	 * the carry when needed.
	 */
	if ((state->Wlow = T32(state->Wlow + 1)) == 0)
		state->Whigh ++;
#ifdef SHABAL_PRINT_VECTORS
	print_state(state, state->Wlow, "increment counter W");
#endif
}

/*
 * Apply the core Shabal permutation. The message block is provided
 * in the m[] parameter.
 */
static void
apply_perm(hashState *state, u32 *m)
{
	/*
	 * We use some local macros to access the A words and compute
	 * the U and V functions with an uncluttered syntax. Note that
	 * this method performs explicit modular reductions of the
	 * access indices, with either 16 or 12 as divisor. Using 12
	 * as divisor, in particular, hurts performance badly (but
	 * divisions by 16 are not free either, since we use the "int"
	 * type which is signed, and thus prevents, in all generality,
	 * the use of a simple bitwise mask by the compiler).
	 */
#define xA     (state->A[(i + sM * j) % nR])
#define xAm1   (state->A[(i + sM * j + (nR - 1)) % nR])
#define U(x)   T32((u32)3 * (u32)(x))
#define V(x)   T32((u32)5 * (u32)(x))

	int i, j;

	for (i = 0; i < sM; i ++) {
		u32 t;

		t = state->B[i];
		state->B[i] = T32(t << 17) | (t >> 15);
	}
#ifdef SHABAL_PRINT_VECTORS
	print_state(state, state->Wlow, "permutation - rotate B");
#endif

	for (j = 0; j < SHABAL_PARAM_P; j ++) {
#ifdef SHABAL_PRINT_VECTORS
		char msg[50];
#endif
		for (i = 0; i < sM; i ++) {
			u32 tB;

			xA = U(xA ^ V(T32(xAm1 << 15) | (xAm1 >> 17))
					^ state->C[(8 + sM - i) % sM])
				^ state->B[(i + O1) % sM]
				^ (state->B[(i + O2) % sM]
					& ~state->B[(i + O3) % sM])
				^ m[i];
			tB = state->B[i];
			state->B[i] = T32(((tB << 1) | (tB >> 31)) ^ ~xA);
		}
#ifdef SHABAL_PRINT_VECTORS
		sprintf(msg, "permutation (j = %d)", j);
		print_state(state, state->Wlow, msg);
#endif
	}

	for (j = 0; j < (3 * nR); j ++)
		state->A[(3 * nR - 1 - j) % nR] = T32(
			state->A[(3 * nR - 1 - j) % nR]
			+ state->C[(3 * nR * sM + 6 - j) % sM]);
#ifdef SHABAL_PRINT_VECTORS
	print_state(state, state->Wlow, "permutation - add C to A");
#endif

#undef xA
#undef xAm1
#undef U
#undef V
}

/* see shabal.h */
HashReturn
Init(hashState *state, int hashbitlen)
{
	int i, j;
	u32 m[sM];

	/*
	 * First, check that the output length is one of the supported
	 * lengths, and set the "hashbitlen" field (mandated by NIST API).
	 * The 224-, 256-, 384- and 512-bit output lengths are NIST
	 * requirements for the SHA-3 competition; the 192-bit output is
	 * a "bonus".
	 */
	switch (hashbitlen) {
	case 192: case 224: case 256: case 384: case 512:
		break;
	default:
		return BAD_HASHBITLEN;
	}
	state->hashbitlen = hashbitlen;

	/*
	 * Initialize the state (all zero, except the counter which is
	 * set to -1).
	 */
	for (i = 0; i < nR; i ++)
		state->A[i] = 0;
	for (i = 0; i < sM; i ++) {
		state->B[i] = 0;
		state->C[i] = 0;
	}
	state->Wlow = (u32)0xFFFFFFFFUL;
	state->Whigh = (u32)0xFFFFFFFFUL;
#ifdef SHABAL_PRINT_VECTORS
	print_state(state, state->Wlow, "init");
#endif

	/*
	 * We compute the first two blocks, corresponding to the prefix.
	 * We process them immediately.
	 */
	for (j = 0; j < (2 * sM); j += sM) {
		for (i = 0; i < sM; i ++)
			m[i] = hashbitlen + j + i;
#ifdef SHABAL_PRINT_VECTORS
		print_message(m, state->Wlow);
#endif

		input_block_add(state, m);
		xor_counter(state);
		apply_perm(state, m);
		input_block_sub(state, m);
		swap_bc(state);
		incr_counter(state);
	}

	/*
	 * We set the fields for input buffer management.
	 */
	state->buffer_ptr = 0;
	state->last_byte_significant_bits = 0;
	return SUCCESS;
}

/* see shabal.h */
HashReturn
Update(hashState *state, const BitSequence *data, DataLength databitlen)
{
	unsigned char *buffer;
	size_t len, ptr;

	/*
	 * "last_byte_significant_bits" contains the number of bits
	 * which are part of the input data in the last buffered
	 * byte. In the state structure, buffer bytes from 0 to
	 * buffer_ptr-1 are "full", and the next byte (buffer[buffer_ptr])
	 * contains the extra non-integral byte, if any. All calls to
	 * Update() shall provide a data bit length multiple of 8,
	 * except possibly the last call. Hence, we know that upon
	 * entry of this function, last_byte_significant_bits is
	 * necessarily zero (if this code was used according to its
	 * specification).
	 *
	 * We process input data by blocks. When we are finished,
	 * we have strictly less than a full block in the buffer
	 * (buffer_ptr is at most 63, and buffer[buffer_ptr] contains
	 * between 0 and 7 significant bits).
	 */
	state->last_byte_significant_bits = (unsigned)databitlen & 0x07;
	len = (size_t)(databitlen >> 3);
	buffer = state->buffer;
	ptr = state->buffer_ptr;
	while (len > 0) {
		size_t clen;

		clen = (sizeof state->buffer) - ptr;
		if (clen > len)
			clen = len;
		memcpy(buffer + ptr, data, clen);
		ptr += clen;
		data += clen;
		len -= clen;
		if (ptr == sizeof state->buffer) {
			u32 m[sM];

			decode_block(state, m);
			input_block_add(state, m);
			xor_counter(state);
			apply_perm(state, m);
			input_block_sub(state, m);
			swap_bc(state);
			incr_counter(state);
			ptr = 0;
		}
	}
	if (state->last_byte_significant_bits != 0)
		buffer[ptr] = *data;
	state->buffer_ptr = ptr;
	return SUCCESS;
}

/* see shabal.h */
HashReturn
Final(hashState *state, BitSequence *hashval)
{
	unsigned char *buffer;
	size_t ptr;
	unsigned lbsb;
	u32 m[sM];
	int i;

	/*
	 * Complete the last block with the padding. Since the buffer data
	 * is always short of a full block by at least one bit, we always
	 * have room for the padding in that block.
	 */
	buffer = state->buffer;
	ptr = state->buffer_ptr;
	lbsb = state->last_byte_significant_bits;
	buffer[ptr] = (buffer[ptr] & ~(0x7F >> lbsb)) | (0x80U >> lbsb);
	memset(buffer + ptr + 1, 0, (sizeof state->buffer) - (ptr + 1));

	/*
	 * Now, process the last (padded) block. We add three extra
	 * permutations, and we optimize away the unnecessary message
	 * additions and subtractions. There is no increment of the
	 * counter either in those last rounds (we keep the counter
	 * value used for the last data block). We transfer the swap to
	 * the next round, because there needs be no final swap.
	 *
	 * Note that we reuse the decoded final block in several calls
	 * to apply_perm().
	 */
	decode_block(state, m);
	input_block_add(state, m);
	xor_counter(state);
	apply_perm(state, m);
	for (i = 0; i < SHABAL_PARAM_E; i ++) {
		swap_bc(state);
		xor_counter(state);
		apply_perm(state, m);
	}

	/*
	 * The ouput consists of the B words, truncated to the requested
	 * length (in the formal description, the C words are used, but
	 * we omitted the last swap, so we have the words in B).
	 */
	for (i = sM - (state->hashbitlen >> 5); i < sM; i ++) {
		u32 tmp;

		tmp = state->B[i];
		*hashval ++ = tmp & 0xFF;
		*hashval ++ = (tmp >> 8) & 0xFF;
		*hashval ++ = (tmp >> 16) & 0xFF;
		*hashval ++ = (tmp >> 24) & 0xFF;
	}
	return SUCCESS;
}

/* see shabal.h */
HashReturn
Hash(int hashbitlen, const BitSequence *data,
	DataLength databitlen, BitSequence *hashval)
{
	hashState st;
	HashReturn r;

	/*
	 * Here, we do not test the output of Update() because we
	 * know that in this implementation, Update() always returns
	 * SUCCESS.
	 */
	r = Init(&st, hashbitlen);
	if (r != SUCCESS)
		return r;
	Update(&st, data, databitlen);
	return Final(&st, hashval);
}

#ifdef SHABAL_PRINT_VECTORS

static void
print_vectors(int hashbitlen, char *name,
	const BitSequence *data, DataLength databitlen)
{
	BitSequence tmp[64];
	int i;

	printf("Intermediate States for Shabal-%d (Message %s)\n",
		hashbitlen, name);
	printf("==============================================\n");
	printf("\n");
	if (Hash(hashbitlen, data, databitlen, tmp) != SUCCESS) {
		fprintf(stderr, "Hash function failed !\n");
		exit(EXIT_FAILURE);
	}
	printf("final output:\n\n");
	for (i = 0; (i << 3) < hashbitlen; i ++) {
		if (i == 0) {
			printf("   ");
		} else if (i % 32 == 0) {
			printf("\n   ");
		} else {
			(void)0;
		}
		printf("%02x", tmp[i]);
	}
	printf("\n\n\n");
}

static const BitSequence message_A[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
};

static const BitSequence message_B[] = {
	0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A,
	0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74,
	0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x2D, 0x30, 0x31, 0x32,
	0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x2D, 0x41, 0x42,
	0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C,
	0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56,
	0x57, 0x58, 0x59, 0x5A, 0x2D, 0x30, 0x31, 0x32, 0x33, 0x34,
	0x35, 0x36, 0x37, 0x38, 0x39, 0x2D, 0x61, 0x62, 0x63, 0x64,
	0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E,
	0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
	0x79, 0x7A
};

int
main(void)
{
	print_vectors(192, "A", message_A, 8 * sizeof message_A);
	print_vectors(192, "B", message_B, 8 * sizeof message_B);
	print_vectors(224, "A", message_A, 8 * sizeof message_A);
	print_vectors(224, "B", message_B, 8 * sizeof message_B);
	print_vectors(256, "A", message_A, 8 * sizeof message_A);
	print_vectors(256, "B", message_B, 8 * sizeof message_B);
	print_vectors(384, "A", message_A, 8 * sizeof message_A);
	print_vectors(384, "B", message_B, 8 * sizeof message_B);
	print_vectors(512, "A", message_A, 8 * sizeof message_A);
	print_vectors(512, "B", message_B, 8 * sizeof message_B);
	return 0;
}

#endif

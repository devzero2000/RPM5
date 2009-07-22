/* Reference code of Arirang family (64 bit). */
#ifndef	_ARIRANG_H
#define	_ARIRANG_H

#include <stdint.h>
#include "beecrypt/beecrypt.h"

#define ARIRANG224_BLOCK_LEN	64
#define ARIRANG224_DIGEST_LEN	28

#define ARIRANG256_BLOCK_LEN	64
#define ARIRANG256_DIGEST_LEN	32

#define ARIRANG384_BLOCK_LEN	128
#define ARIRANG384_DIGEST_LEN	48

#define ARIRANG512_BLOCK_LEN	128
#define ARIRANG512_DIGEST_LEN	64

struct _arirangParam {
    int hashbitlen;		// Hash length
    uint64_t counter[2];	// Counter
    uint64_t count[2];		// Count
    uint8_t block[128];		// Message block
    uint64_t workingvar[8];	// Working variables
    uint32_t blocklen;		// hash block length
    uint32_t remainderbit;	// bit_length % 7
};

#ifndef __cplusplus
typedef struct _arirangParam arirangParam;
#endif

#ifdef __cplusplus
extern "C" {
#endif

BEECRYPTAPI
int arirangInit(arirangParam* sp, int hashbitlen);

BEECRYPTAPI
int arirangReset(arirangParam* sp);

BEECRYPTAPI
int arirangUpdate(arirangParam* sp, const byte *data, size_t size);

BEECRYPTAPI
int arirangDigest(arirangParam* sp, byte *digest);

#ifdef __cplusplus
}
#endif

#endif

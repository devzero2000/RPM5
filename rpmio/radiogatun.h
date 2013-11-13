/* RadioGatún reference code
 * Public domain
 * For more information on RadioGatún, please refer to 
 * http://radiogatun.noekeon.org/
 *
 * Streaming API and padding conventions borrowed from sphlib-3.0.
*/
#ifndef _RADIOGATUN_H_
#define _RADIOGATUN_H_

#include <stdint.h>
#include <beecrypt/beecrypt.h>

#define	RG32_SIZE	256	/* bits */
#ifdef __cplusplus
struct BEECRYPTAPI rg32Param
#else
struct _rg32Param
#endif
{
    unsigned char data[156];
    unsigned ix;
    uint32_t a[19];
    uint32_t b[39];
};
#ifndef __cplusplus
typedef struct _rg32Param rg32Param;
#endif

#define	RG64_SIZE	256	/* bits */
#ifdef __cplusplus
struct BEECRYPTAPI rg64Param
#else
struct _rg64Param
#endif
{
    unsigned char data[2 * 156];
    unsigned ix;
    uint64_t a[19];
    uint64_t b[39];
};
#ifndef __cplusplus
typedef struct _rg64Param rg64Param;
#endif

#ifdef __cplusplus
extern "C" {
#endif

BEECRYPTAPI
int rg32Init(rg32Param* sp, int hashbitlen)
	/*@*/;
BEECRYPTAPI
int rg32Reset(rg32Param* sp)
	/*@*/;
BEECRYPTAPI
int rg32Update(rg32Param* sp, const byte *data, size_t size)
	/*@*/;
BEECRYPTAPI
int rg32Digest(rg32Param* sp, byte *digest)
	/*@*/;

BEECRYPTAPI
int rg64Init(rg64Param* sp, int hashbitlen)
	/*@*/;
BEECRYPTAPI
int rg64Reset(rg64Param* sp)
	/*@*/;
BEECRYPTAPI
int rg64Update(rg64Param* sp, const byte *data, size_t size)
	/*@*/;
BEECRYPTAPI
int rg64Digest(rg64Param* sp, byte *digest)
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif	/* _RADIOGATUN_H */

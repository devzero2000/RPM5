/** \ingroup signature
 * \file rpmio/digest.c
 */

#include "system.h"

#include <zlib.h>

#include "rpmio_internal.h"

#include "md2.h"
#include "md4.h"
#include "sha224.h"
#include "rmd128.h"
#include "rmd160.h"
#include "rmd256.h"
#include "rmd320.h"
#include "salsa10.h"
#include "salsa20.h"
#include "tiger.h"

#include "debug.h"

#ifdef	SHA_DEBUG
#define	DPRINTF(_a)	fprintf _a
#else
#define	DPRINTF(_a)
#endif

#if !defined(ZLIB_H) || defined(__LCLINT__)
/**
 */
/*@-shadow@*/
static uint32_t crc32(uint32_t crc, const byte * data, size_t size)
	/*@*/
{
    static uint32_t polynomial = 0xedb88320;    /* reflected 0x04c11db7 */
    static uint32_t xorout = 0xffffffff;
    static uint32_t table[256];

    crc ^= xorout;

    if (data == NULL) {
	/* generate the table of CRC remainders for all possible bytes */
	uint32_t c;
	uint32_t i, j;
	for (i = 0;  i < 256;  i++) {
	    c = i;
	    for (j = 0;  j < 8;  j++) {
		if (c & 1)
		    c = polynomial ^ (c >> 1);
		else
		    c = (c >> 1);
	    }
	    table[i] = c;
	}
    } else
    while (size) {
	crc = table[(crc ^ *data) & 0xff] ^ (crc >> 8);
	size--;
	data++;
    }

    crc ^= xorout;

    return crc;

}
/*@=shadow@*/
#endif

/* Include Bob Jenkins lookup3 hash */
#define	_JLU3_jlu32l
#include "lookup3.c"

/**
 */
typedef struct {
	uint32_t crc;
	uint32_t (*update)  (uint32_t crc, const byte * data, size_t size);
	uint32_t (*combine) (uint32_t crc1, uint32_t crc2, size_t len2);
} sum32Param;

/**
 */
static int sum32Reset(register sum32Param* mp)
	/*@modifies *mp @*/
{
    if (mp->update)
	mp->crc = (*mp->update) (0, NULL, 0);
    return 0;
}

/**
 */
static int sum32Update(sum32Param* mp, const byte* data, size_t size)
	/*@modifies *mp @*/
{
    if (mp->update)
	mp->crc = (*mp->update) (mp->crc, data, size);
    return 0;
}

/**
 */
static int sum32Digest(sum32Param* mp, byte* data)
	/*@modifies *mp, data @*/
{
	uint32_t c = mp->crc;

	data[ 0] = (byte)(c >> 24);
	data[ 1] = (byte)(c >> 16);
	data[ 2] = (byte)(c >>  8);
	data[ 3] = (byte)(c      );

	(void) sum32Reset(mp);

	return 0;
}

/*
 * ECMA-182 polynomial, see
 *     http://www.ecma-international.org/publications/files/ECMA-ST/Ecma-182.pdf
 */
/**
 */
static uint64_t crc64(uint64_t crc, const byte * data, size_t size)
	/*@*/
{
    static uint64_t polynomial =
	0xc96c5795d7870f42ULL;	/* reflected 0x42f0e1eba9ea3693ULL */
    static uint64_t xorout = 0xffffffffffffffffULL;
    static uint64_t table[256];

    crc ^= xorout;

    if (data == NULL) {
	/* generate the table of CRC remainders for all possible bytes */
	uint64_t c;
	uint32_t i, j;
	for (i = 0;  i < 256;  i++) {
	    c = i;
	    for (j = 0;  j < 8;  j++) {
		if (c & 1)
		    c = polynomial ^ (c >> 1);
		else
		    c = (c >> 1);
	    }
	    table[i] = c;
	}
    } else
    while (size) {
	crc = table[(crc ^ *data) & 0xff] ^ (crc >> 8);
	size--;
	data++;
    }

    crc ^= xorout;

    return crc;

}

/*
 * Swiped from zlib, using uint64_t rather than unsigned long computation.
 * Use at your own risk, uint64_t problems with compilers may exist.
 */
#define	GF2_DIM	64

/**
 */
static uint64_t gf2_matrix_times(uint64_t *mat, uint64_t vec)
	/*@*/
{
    uint64_t sum;

    sum = 0;
    while (vec) {
        if (vec & 1)
            sum ^= *mat;
        vec >>= 1;
        mat++;
    }
    return sum;
}

/**
 */
static void gf2_matrix_square(/*@out@*/ uint64_t *square, uint64_t *mat)
	/*@modifies square @*/
{
    int n;

    for (n = 0; n < GF2_DIM; n++)
        square[n] = gf2_matrix_times(mat, mat[n]);
}

/**
 */
static uint64_t crc64_combine(uint64_t crc1, uint64_t crc2, size_t len2)
	/*@*/
{
    int n;
    uint64_t row;
    uint64_t even[GF2_DIM];    /* even-power-of-two zeros operator */
    uint64_t odd[GF2_DIM];     /* odd-power-of-two zeros operator */

    /* degenerate case */
    if (len2 == 0)
        return crc1;

    /* put operator for one zero bit in odd */
    odd[0] = 0xc96c5795d7870f42ULL;	/* reflected 0x42f0e1eba9ea3693ULL */
    row = 1;
    for (n = 1; n < GF2_DIM; n++) {
        odd[n] = row;
        row <<= 1;
    }

    /* put operator for two zero bits in even */
    gf2_matrix_square(even, odd);

    /* put operator for four zero bits in odd */
    gf2_matrix_square(odd, even);

    /* apply len2 zeros to crc1 (first square will put the operator for one
       zero byte, eight zero bits, in even) */
    do {
        /* apply zeros operator for this bit of len2 */
        gf2_matrix_square(even, odd);
        if (len2 & 1)
            crc1 = gf2_matrix_times(even, crc1);
        len2 >>= 1;

        /* if no more bits set, then done */
        if (len2 == 0)
            break;

        /* another iteration of the loop with odd and even swapped */
        gf2_matrix_square(odd, even);
        if (len2 & 1)
            crc1 = gf2_matrix_times(odd, crc1);
        len2 >>= 1;

        /* if no more bits set, then done */
    } while (len2 != 0);

    /* return combined crc */
    crc1 ^= crc2;
    return crc1;
}

/**
 */
typedef struct {
	uint64_t crc;
	uint64_t (*update)  (uint64_t crc, const byte * data, size_t size);
	uint64_t (*combine) (uint64_t crc1, uint64_t crc2, size_t len2);
} sum64Param;

/**
 */
static int sum64Reset(register sum64Param* mp)
	/*@modifies *mp @*/
{
    if (mp->update)
	mp->crc = (*mp->update) (0, NULL, 0);
    return 0;
}

/**
 */
static int sum64Update(sum64Param* mp, const byte* data, size_t size)
	/*@modifies *mp @*/
{
    if (mp->update)
	mp->crc = (*mp->update) (mp->crc, data, size);
    return 0;
}

/**
 */
static int sum64Digest(sum64Param* mp, byte* data)
	/*@modifies *mp, data @*/
{
	uint64_t c = mp->crc;

	data[ 0] = (byte)(c >> 56);
	data[ 1] = (byte)(c >> 48);
	data[ 2] = (byte)(c >> 40);
	data[ 3] = (byte)(c >> 32);
	data[ 4] = (byte)(c >> 24);
	data[ 5] = (byte)(c >> 16);
	data[ 6] = (byte)(c >>  8);
	data[ 7] = (byte)(c      );

	(void) sum64Reset(mp);

	return 0;
}

/*@access DIGEST_CTX@*/

/**
 * MD5/SHA1 digest private data.
 */
struct DIGEST_CTX_s {
/*@observer@*/
    const char * name;		/*!< Digest name. */
    size_t paramsize;		/*!< No. bytes of digest parameters. */
    size_t datasize;		/*!< No. bytes in block of plaintext data. */
    size_t digestsize;		/*!< No. bytes of digest. */
    int (*Reset) (void * param)
	/*@modifies param @*/;	/*!< Digest initialize. */
    int (*Update) (void * param, const byte * data, size_t size)
	/*@modifies param @*/;	/*!< Digest transform. */
    int (*Digest) (void * param, /*@out@*/ byte * digest)
	/*@modifies param, digest @*/;	/*!< Digest finish. */
    rpmDigestFlags flags;	/*!< Bit(s) to control digest operation. */
    void * param;		/*!< Digest parameters. */
};

/*@-boundsread@*/
DIGEST_CTX
rpmDigestDup(DIGEST_CTX octx)
{
    DIGEST_CTX nctx;
    nctx = memcpy(xcalloc(1, sizeof(*nctx)), octx, sizeof(*nctx));
    nctx->param = memcpy(xcalloc(1, nctx->paramsize), octx->param, nctx->paramsize);
    return nctx;
}
/*@=boundsread@*/

DIGEST_CTX
rpmDigestInit(pgpHashAlgo hashalgo, rpmDigestFlags flags)
{
    DIGEST_CTX ctx = xcalloc(1, sizeof(*ctx));
    int xx;

    ctx->flags = flags;

    switch (hashalgo) {
    case PGPHASHALGO_MD5:
	ctx->name = "MD5";
	ctx->digestsize = 128/8;
	ctx->datasize = 64;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(md5Param);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (void *) md5Reset;
	ctx->Update = (void *) md5Update;
	ctx->Digest = (void *) md5Digest;
/*@=type@*/
	break;
    case PGPHASHALGO_SHA1:
	ctx->name = "SHA-1";
	ctx->digestsize = 160/8;
	ctx->datasize = 64;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(sha1Param);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (void *) sha1Reset;
	ctx->Update = (void *) sha1Update;
	ctx->Digest = (void *) sha1Digest;
/*@=type@*/
	break;
    case PGPHASHALGO_RIPEMD128:
	ctx->name = "RIPEMD-128";
	ctx->digestsize = 128/8;
	ctx->datasize = 64;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(rmd128Param);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (void *) rmd128Reset;
	ctx->Update = (void *) rmd128Update;
	ctx->Digest = (void *) rmd128Digest;
/*@=type@*/
	break;
    case PGPHASHALGO_RIPEMD160:
	ctx->name = "RIPEMD-160";
	ctx->digestsize = 160/8;
	ctx->datasize = 64;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(rmd160Param);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (void *) rmd160Reset;
	ctx->Update = (void *) rmd160Update;
	ctx->Digest = (void *) rmd160Digest;
/*@=type@*/
	break;
    case PGPHASHALGO_RIPEMD256:
	ctx->name = "RIPEMD-256";
	ctx->digestsize = 256/8;
	ctx->datasize = 64;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(rmd256Param);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (void *) rmd256Reset;
	ctx->Update = (void *) rmd256Update;
	ctx->Digest = (void *) rmd256Digest;
/*@=type@*/
	break;
    case PGPHASHALGO_RIPEMD320:
	ctx->name = "RIPEMD-320";
	ctx->digestsize = 320/8;
	ctx->datasize = 64;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(rmd320Param);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (void *) rmd320Reset;
	ctx->Update = (void *) rmd320Update;
	ctx->Digest = (void *) rmd320Digest;
/*@=type@*/
	break;
    case PGPHASHALGO_SALSA10:
	ctx->name = "SALSA-10";
	ctx->digestsize = 512/8;
	ctx->datasize = 64;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(salsa10Param);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (void *) salsa10Reset;
	ctx->Update = (void *) salsa10Update;
	ctx->Digest = (void *) salsa10Digest;
/*@=type@*/
	break;
    case PGPHASHALGO_SALSA20:
	ctx->name = "SALSA-20";
	ctx->digestsize = 512/8;
	ctx->datasize = 64;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(salsa20Param);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (void *) salsa20Reset;
	ctx->Update = (void *) salsa20Update;
	ctx->Digest = (void *) salsa20Digest;
/*@=type@*/
	break;
    case PGPHASHALGO_TIGER192:
	ctx->name = "TIGER-192";
	ctx->digestsize = 192/8;
	ctx->datasize = 64;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(tigerParam);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (void *) tigerReset;
	ctx->Update = (void *) tigerUpdate;
	ctx->Digest = (void *) tigerDigest;
/*@=type@*/
	break;
    case PGPHASHALGO_MD2:
	ctx->name = "MD2";
	ctx->digestsize = 128/8;
	ctx->datasize = 16;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(md2Param);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (void *) md2Reset;
	ctx->Update = (void *) md2Update;
	ctx->Digest = (void *) md2Digest;
/*@=type@*/
	break;
    case PGPHASHALGO_MD4:
	ctx->name = "MD4";
	ctx->digestsize = 128/8;
	ctx->datasize = 64;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(md4Param);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (void *) md4Reset;
	ctx->Update = (void *) md4Update;
	ctx->Digest = (void *) md4Digest;
/*@=type@*/
	break;
    case PGPHASHALGO_CRC32:
	ctx->name = "CRC-32";
	ctx->digestsize = 32/8;
	ctx->datasize = 8;
	{   sum32Param * mp = xcalloc(1, sizeof(*mp));
/*@-type @*/
	    mp->update = (void *) crc32;
#if defined(ZLIB_H)
	    mp->combine = (void *) crc32_combine;
#endif
/*@=type @*/
	    ctx->paramsize = sizeof(*mp);
	    ctx->param = mp;
	}
/*@-type@*/
	ctx->Reset = (void *) sum32Reset;
	ctx->Update = (void *) sum32Update;
	ctx->Digest = (void *) sum32Digest;
/*@=type@*/
	break;
    case PGPHASHALGO_ADLER32:
	ctx->name = "ADLER-32";
	ctx->digestsize = 32/8;
	ctx->datasize = 8;
	{   sum32Param * mp = xcalloc(1, sizeof(*mp));
/*@-type @*/
#if defined(ZLIB_H)
	    mp->update = (void *) adler32;
	    mp->combine = (void *) adler32_combine;
#endif
/*@=type @*/
	    ctx->paramsize = sizeof(*mp);
	    ctx->param = mp;
	}
/*@-type@*/
	ctx->Reset = (void *) sum32Reset;
	ctx->Update = (void *) sum32Update;
	ctx->Digest = (void *) sum32Digest;
/*@=type@*/
	break;
    case PGPHASHALGO_JLU32:
	ctx->name = "JLU-32";
	ctx->digestsize = 32/8;
	ctx->datasize = 8;
	{   sum32Param * mp = xcalloc(1, sizeof(*mp));
/*@-type @*/
	    mp->update = (void *) jlu32l;
/*@=type @*/
	    ctx->paramsize = sizeof(*mp);
	    ctx->param = mp;
	}
/*@-type@*/
	ctx->Reset = (void *) sum32Reset;
	ctx->Update = (void *) sum32Update;
	ctx->Digest = (void *) sum32Digest;
/*@=type@*/
	break;
    case PGPHASHALGO_CRC64:
	ctx->name = "CRC-64";
	ctx->digestsize = 64/8;
	ctx->datasize = 8;
	{   sum64Param * mp = xcalloc(1, sizeof(*mp));
/*@-type@*/
	    mp->update = (void *) crc64;
	    mp->combine = (void *) crc64_combine;
/*@=type@*/
	    ctx->paramsize = sizeof(*mp);
	    ctx->param = mp;
	}
/*@-type@*/
	ctx->Reset = (void *) sum64Reset;
	ctx->Update = (void *) sum64Update;
	ctx->Digest = (void *) sum64Digest;
/*@=type@*/
	break;
#if defined(HAVE_BEECRYPT_API_H)
    case PGPHASHALGO_SHA224:
	ctx->name = "SHA-224";
	ctx->digestsize = 224/8;
	ctx->datasize = 64;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(sha224Param);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (void *) sha224Reset;
	ctx->Update = (void *) sha224Update;
	ctx->Digest = (void *) sha224Digest;
/*@=type@*/
	break;
    case PGPHASHALGO_SHA256:
	ctx->name = "SHA-256";
	ctx->digestsize = 256/8;
	ctx->datasize = 64;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(sha256Param);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (void *) sha256Reset;
	ctx->Update = (void *) sha256Update;
	ctx->Digest = (void *) sha256Digest;
/*@=type@*/
	break;
    case PGPHASHALGO_SHA384:
	ctx->name = "SHA-384";
	ctx->digestsize = 384/8;
	ctx->datasize = 128;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(sha384Param);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (void *) sha384Reset;
	ctx->Update = (void *) sha384Update;
	ctx->Digest = (void *) sha384Digest;
/*@=type@*/
	break;
    case PGPHASHALGO_SHA512:
	ctx->name = "SHA-512";
	ctx->digestsize = 512/8;
	ctx->datasize = 128;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(sha512Param);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (void *) sha512Reset;
	ctx->Update = (void *) sha512Update;
	ctx->Digest = (void *) sha512Digest;
/*@=type@*/
	break;
#endif
    case PGPHASHALGO_HAVAL_5_160:
    default:
	free(ctx);
	return NULL;
	/*@notreached@*/ break;
    }

/*@-boundsread@*/
    xx = (*ctx->Reset) (ctx->param);
/*@=boundsread@*/

DPRINTF((stderr, "*** Init(%x) ctx %p param %p\n", flags, ctx, ctx->param));
    return ctx;
}

/*@-mustmod@*/ /* LCL: ctx->param may be modified, but ctx is abstract @*/
int
rpmDigestUpdate(DIGEST_CTX ctx, const void * data, size_t len)
{
    if (ctx == NULL)
	return -1;

DPRINTF((stderr, "*** Update(%p,%p,%d) param %p \"%s\"\n", ctx, data, len, ctx->param, ((char *)data)));
/*@-boundsread@*/
    return (*ctx->Update) (ctx->param, data, len);
/*@=boundsread@*/
}
/*@=mustmod@*/

/*@-boundswrite@*/
int
rpmDigestFinal(DIGEST_CTX ctx, void * datap, size_t *lenp, int asAscii)
{
    byte * digest;
    char * t;
    int i;

    if (ctx == NULL)
	return -1;
    digest = xmalloc(ctx->digestsize);

DPRINTF((stderr, "*** Final(%p,%p,%p,%d) param %p digest %p\n", ctx, datap, lenp, asAscii, ctx->param, digest));
/*@-noeffectuncon@*/ /* FIX: check rc */
    (void) (*ctx->Digest) (ctx->param, digest);
/*@=noeffectuncon@*/

    /* Return final digest. */
/*@-branchstate@*/
    if (!asAscii) {
	if (lenp) *lenp = ctx->digestsize;
	if (datap) {
	    *(byte **)datap = digest;
	    digest = NULL;
	}
    } else {
	if (lenp) *lenp = (2*ctx->digestsize) + 1;
	if (datap) {
	    const byte * s = (const byte *) digest;
	    static const char hex[] = "0123456789abcdef";

	    *(char **)datap = t = xmalloc((2*ctx->digestsize) + 1);
	    for (i = 0 ; i < ctx->digestsize; i++) {
		*t++ = hex[ (unsigned)((*s >> 4) & 0x0f) ];
		*t++ = hex[ (unsigned)((*s++   ) & 0x0f) ];
	    }
	    *t = '\0';
	}
    }
/*@=branchstate@*/
    if (digest) {
	memset(digest, 0, ctx->digestsize);	/* In case it's sensitive */
	free(digest);
    }
    memset(ctx->param, 0, ctx->paramsize);	/* In case it's sensitive */
    free(ctx->param);
    memset(ctx, 0, sizeof(*ctx));	/* In case it's sensitive */
    free(ctx);
    return 0;
}
/*@=boundswrite@*/

pgpHashAlgo rpmDigestHashAlgo = PGPHASHALGO_MD5;

/**
 * Digest options using popt.
 */
struct poptOption rpmDigestPoptTable[] = {
 { "md2", '\0', POPT_ARG_VAL, 	&rpmDigestHashAlgo, PGPHASHALGO_MD2,
	NULL, NULL },
 { "md4", '\0', POPT_ARG_VAL, 	&rpmDigestHashAlgo, PGPHASHALGO_MD4,
	NULL, NULL },
 { "md5", '\0', POPT_ARG_VAL, 	&rpmDigestHashAlgo, PGPHASHALGO_MD5,
	NULL, NULL },
 { "sha1",'\0', POPT_ARG_VAL, 	&rpmDigestHashAlgo, PGPHASHALGO_SHA1,
	NULL, NULL },
 { "sha224",'\0', POPT_ARG_VAL, &rpmDigestHashAlgo, PGPHASHALGO_SHA224,
	NULL, NULL },
 { "sha256",'\0', POPT_ARG_VAL, &rpmDigestHashAlgo, PGPHASHALGO_SHA256,
	NULL, NULL },
 { "sha384",'\0', POPT_ARG_VAL, &rpmDigestHashAlgo, PGPHASHALGO_SHA384,
	NULL, NULL },
 { "sha512",'\0', POPT_ARG_VAL, &rpmDigestHashAlgo, PGPHASHALGO_SHA512,
	NULL, NULL },
 { "salsa10",'\0', POPT_ARG_VAL, &rpmDigestHashAlgo, PGPHASHALGO_SALSA10,
	NULL, NULL },
 { "salsa20",'\0', POPT_ARG_VAL, &rpmDigestHashAlgo, PGPHASHALGO_SALSA20,
	NULL, NULL },
 { "rmd128",'\0', POPT_ARG_VAL,	&rpmDigestHashAlgo, PGPHASHALGO_RIPEMD128,
	NULL, NULL },
 { "rmd160",'\0', POPT_ARG_VAL,	&rpmDigestHashAlgo, PGPHASHALGO_RIPEMD160,
	NULL, NULL },
 { "rmd256",'\0', POPT_ARG_VAL,	&rpmDigestHashAlgo, PGPHASHALGO_RIPEMD256,
	NULL, NULL },
 { "rmd320",'\0', POPT_ARG_VAL,	&rpmDigestHashAlgo, PGPHASHALGO_RIPEMD320,
	NULL, NULL },
 { "tiger",'\0', POPT_ARG_VAL,	&rpmDigestHashAlgo, PGPHASHALGO_TIGER192,
	NULL, NULL },
 { "crc32",'\0', POPT_ARG_VAL,	&rpmDigestHashAlgo, PGPHASHALGO_CRC32,
	NULL, NULL },
 { "crc64",'\0', POPT_ARG_VAL,	&rpmDigestHashAlgo, PGPHASHALGO_CRC64,
	NULL, NULL },
 { "adler32",'\0', POPT_ARG_VAL,&rpmDigestHashAlgo, PGPHASHALGO_ADLER32,
	NULL, NULL },
 { "jlu32",'\0', POPT_ARG_VAL,&rpmDigestHashAlgo, PGPHASHALGO_JLU32,
	NULL, NULL },
    POPT_TABLEEND
};

/** \ingroup signature
 * \file rpmio/digest.c
 */

#include "system.h"

#include "rpmio_internal.h"

#include <rpmbc.h>

#include "crc.h"
#include "md2.h"
#include "salsa10.h"
#include "salsa20.h"
#include "tiger.h"

#include "debug.h"

/*@unchecked@*/
int _ctx_debug = 0;

#ifdef	_DIGEST_DEBUG
#define	DPRINTF(_a)	if (_ctx_debug < 0) fprintf _a
#else
#define	DPRINTF(_a)
#endif

/* Include Bob Jenkins lookup3 hash */
#define	_JLU3_jlu32l
#include "lookup3.c"

/*@access DIGEST_CTX@*/

/**
 * Digest private data.
 */
struct DIGEST_CTX_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
/*@observer@*/
    const char * name;		/*!< Digest name. */
    size_t paramsize;		/*!< No. bytes of digest parameters. */
    size_t datasize;		/*!< No. bytes in block of plaintext data. */
    size_t digestsize;		/*!< No. bytes of digest. */
    int (*Reset) (void * param)
	/*@modifies param @*/;	/*!< Digest initialize. */
    int (*Update) (void * param, const byte * data, size_t size)
	/*@modifies param @*/;	/*!< Digest update. */
    int (*Digest) (void * param, /*@out@*/ byte * digest)
	/*@modifies param, digest @*/;	/*!< Digest finish. */
    pgpHashAlgo hashalgo;	/*!< RFC 2440/4880 hash algorithm id. */
    rpmDigestFlags flags;	/*!< Bit(s) to control digest operation. */
/*@observer@*/ /*@null@*/
    const char * asn1;		/*!< RFC 3447 ASN1 oid string (in hex). */
    void * param;		/*!< Digest parameters. */
};

static void ctxFini(void * _ctx)
	/*@modifies _ctx @*/
{
    DIGEST_CTX ctx = _ctx;
    if (ctx->param != NULL && ctx->paramsize > 0)
	memset(ctx->param, 0, ctx->paramsize);	/* In case it's sensitive */
    ctx->param = _free(ctx->param);
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _ctxPool;

static DIGEST_CTX ctxGetPool(rpmioPool pool)
{
    DIGEST_CTX ctx;

    if (_ctxPool == NULL) {
	_ctxPool = rpmioNewPool("ctx", sizeof(*ctx), -1, _ctx_debug,
			NULL, NULL, ctxFini);
	pool = _ctxPool;
    }
    return (DIGEST_CTX) rpmioGetPool(pool, sizeof(*ctx));
}

pgpHashAlgo rpmDigestAlgo(DIGEST_CTX ctx)
{
    return (ctx != NULL ? ctx->hashalgo : PGPHASHALGO_NONE);
}

const char * rpmDigestName(DIGEST_CTX ctx)
{
    return (ctx != NULL ? ctx->name : "UNKNOWN");
}

const char * rpmDigestASN1(DIGEST_CTX ctx)
{
    return (ctx != NULL ? ctx->asn1 : NULL);
}

DIGEST_CTX
rpmDigestDup(DIGEST_CTX octx)
{
    DIGEST_CTX nctx = ctxGetPool(_ctxPool);

    nctx->name = octx->name;
    nctx->digestsize = octx->digestsize;
    nctx->datasize = octx->datasize;
    nctx->paramsize = octx->paramsize;
    nctx->Reset = octx->Reset;
    nctx->Update = octx->Update;
    nctx->Digest = octx->Digest;
    nctx->hashalgo = octx->hashalgo;
    nctx->flags = octx->flags;
    nctx->asn1 = octx->asn1;
    nctx->param = memcpy(xcalloc(1, nctx->paramsize), octx->param, nctx->paramsize);
    return (DIGEST_CTX)rpmioLinkPoolItem((rpmioItem)nctx, __FUNCTION__, __FILE__, __LINE__);
}

DIGEST_CTX
rpmDigestInit(pgpHashAlgo hashalgo, rpmDigestFlags flags)
{
    DIGEST_CTX ctx = ctxGetPool(_ctxPool);
    int xx;

    ctx->hashalgo = hashalgo;
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
	ctx->Reset = (int (*)(void *)) md5Reset;
	ctx->Update = (int (*)(void *, const byte *, size_t)) md5Update;
	ctx->Digest = (int (*)(void *, byte *)) md5Digest;
/*@=type@*/
	ctx->asn1 = "3020300c06082a864886f70d020505000410";
	break;
    case PGPHASHALGO_SHA1:
	ctx->name = "SHA1";
	ctx->digestsize = 160/8;
	ctx->datasize = 64;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(sha1Param);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (int (*)(void *)) sha1Reset;
	ctx->Update = (int (*)(void *, const byte *, size_t)) sha1Update;
	ctx->Digest = (int (*)(void *, byte *)) sha1Digest;
/*@=type@*/
	ctx->asn1 = "3021300906052b0e03021a05000414";
	break;
    case PGPHASHALGO_RIPEMD128:
	ctx->name = "RIPEMD128";
	ctx->digestsize = 128/8;
	ctx->datasize = 64;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(ripemd128Param);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (int (*)(void *)) ripemd128Reset;
	ctx->Update = (int (*)(void *, const byte *, size_t)) ripemd128Update;
	ctx->Digest = (int (*)(void *, byte *)) ripemd128Digest;
/*@=type@*/
	break;
    case PGPHASHALGO_RIPEMD160:
	ctx->name = "RIPEMD160";
	ctx->digestsize = 160/8;
	ctx->datasize = 64;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(ripemd160Param);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (int (*)(void *)) ripemd160Reset;
	ctx->Update = (int (*)(void *, const byte *, size_t)) ripemd160Update;
	ctx->Digest = (int (*)(void *, byte *)) ripemd160Digest;
/*@=type@*/
	ctx->asn1 = "3021300906052b2403020105000414";
	break;
    case PGPHASHALGO_RIPEMD256:
	ctx->name = "RIPEMD256";
	ctx->digestsize = 256/8;
	ctx->datasize = 64;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(ripemd256Param);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (int (*)(void *)) ripemd256Reset;
	ctx->Update = (int (*)(void *, const byte *, size_t)) ripemd256Update;
	ctx->Digest = (int (*)(void *, byte *)) ripemd256Digest;
/*@=type@*/
	break;
    case PGPHASHALGO_RIPEMD320:
	ctx->name = "RIPEMD320";
	ctx->digestsize = 320/8;
	ctx->datasize = 64;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(ripemd320Param);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (int (*)(void *)) ripemd320Reset;
	ctx->Update = (int (*)(void *, const byte *, size_t)) ripemd320Update;
	ctx->Digest = (int (*)(void *, byte *)) ripemd320Digest;
/*@=type@*/
	break;
    case PGPHASHALGO_SALSA10:
	ctx->name = "SALSA10";
	ctx->digestsize = 512/8;
	ctx->datasize = 64;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(salsa10Param);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (int (*)(void *)) salsa10Reset;
	ctx->Update = (int (*)(void *, const byte *, size_t)) salsa10Update;
	ctx->Digest = (int (*)(void *, byte *)) salsa10Digest;
/*@=type@*/
	break;
    case PGPHASHALGO_SALSA20:
	ctx->name = "SALSA20";
	ctx->digestsize = 512/8;
	ctx->datasize = 64;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(salsa20Param);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (int (*)(void *)) salsa20Reset;
	ctx->Update = (int (*)(void *, const byte *, size_t)) salsa20Update;
	ctx->Digest = (int (*)(void *, byte *)) salsa20Digest;
/*@=type@*/
	break;
    case PGPHASHALGO_TIGER192:
	ctx->name = "TIGER192";
	ctx->digestsize = 192/8;
	ctx->datasize = 64;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(tigerParam);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (int (*)(void *)) tigerReset;
	ctx->Update = (int (*)(void *, const byte *, size_t)) tigerUpdate;
	ctx->Digest = (int (*)(void *, byte *)) tigerDigest;
/*@=type@*/
	ctx->asn1 = "3029300d06092b06010401da470c0205000418";
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
	ctx->Reset = (int (*)(void *)) md2Reset;
	ctx->Update = (int (*)(void *, const byte *, size_t)) md2Update;
	ctx->Digest = (int (*)(void *, byte *)) md2Digest;
/*@=type@*/
	ctx->asn1 = "3020300c06082a864886f70d020205000410";
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
	ctx->Reset = (int (*)(void *)) md4Reset;
	ctx->Update = (int (*)(void *, const byte *, size_t)) md4Update;
	ctx->Digest = (int (*)(void *, byte *)) md4Digest;
/*@=type@*/
	break;
    case PGPHASHALGO_CRC32:
	ctx->name = "CRC32";
	ctx->digestsize = 32/8;
	ctx->datasize = 8;
	{   sum32Param * mp = xcalloc(1, sizeof(*mp));
/*@-type @*/
	    mp->update = (rpmuint32_t (*)(rpmuint32_t, const byte *, size_t)) __crc32;
	    mp->combine = (rpmuint32_t (*)(rpmuint32_t, rpmuint32_t, size_t)) __crc32_combine;
/*@=type @*/
	    ctx->paramsize = sizeof(*mp);
	    ctx->param = mp;
	}
/*@-type@*/
	ctx->Reset = (int (*)(void *)) sum32Reset;
	ctx->Update = (int (*)(void *, const byte *, size_t)) sum32Update;
	ctx->Digest = (int (*)(void *, byte *)) sum32Digest;
/*@=type@*/
	break;
    case PGPHASHALGO_ADLER32:
	ctx->name = "ADLER32";
	ctx->digestsize = 32/8;
	ctx->datasize = 8;
	{   sum32Param * mp = xcalloc(1, sizeof(*mp));
/*@-type @*/
	    mp->update = (rpmuint32_t (*)(rpmuint32_t, const byte *, size_t)) __adler32;
	    mp->combine = (rpmuint32_t (*)(rpmuint32_t, rpmuint32_t, size_t)) __adler32_combine;
/*@=type @*/
	    ctx->paramsize = sizeof(*mp);
	    ctx->param = mp;
	}
/*@-type@*/
	ctx->Reset = (int (*)(void *)) sum32Reset;
	ctx->Update = (int (*)(void *, const byte *, size_t)) sum32Update;
	ctx->Digest = (int (*)(void *, byte *)) sum32Digest;
/*@=type@*/
	break;
    case PGPHASHALGO_JLU32:
	ctx->name = "JLU32";
	ctx->digestsize = 32/8;
	ctx->datasize = 8;
	{   sum32Param * mp = xcalloc(1, sizeof(*mp));
/*@-type @*/
	    mp->update = (rpmuint32_t (*)(rpmuint32_t, const byte *, size_t)) jlu32l;
/*@=type @*/
	    ctx->paramsize = sizeof(*mp);
	    ctx->param = mp;
	}
/*@-type@*/
	ctx->Reset = (int (*)(void *)) sum32Reset;
	ctx->Update = (int (*)(void *, const byte *, size_t)) sum32Update;
	ctx->Digest = (int (*)(void *, byte *)) sum32Digest;
/*@=type@*/
	break;
    case PGPHASHALGO_CRC64:
	ctx->name = "CRC64";
	ctx->digestsize = 64/8;
	ctx->datasize = 8;
	{   sum64Param * mp = xcalloc(1, sizeof(*mp));
/*@-type@*/
	    mp->update = (rpmuint64_t (*)(rpmuint64_t, const byte *, size_t)) __crc64;
	    mp->combine = (rpmuint64_t (*)(rpmuint64_t, rpmuint64_t, size_t)) __crc64_combine;
/*@=type@*/
	    ctx->paramsize = sizeof(*mp);
	    ctx->param = mp;
	}
/*@-type@*/
	ctx->Reset = (int (*)(void *)) sum64Reset;
	ctx->Update = (int (*)(void *, const byte *, size_t)) sum64Update;
	ctx->Digest = (int (*)(void *, byte *)) sum64Digest;
/*@=type@*/
	break;
#if defined(HAVE_BEECRYPT_API_H)
    case PGPHASHALGO_SHA224:
	ctx->name = "SHA224";
	ctx->digestsize = 224/8;
	ctx->datasize = 64;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(sha224Param);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (int (*)(void *)) sha224Reset;
	ctx->Update = (int (*)(void *, const byte *, size_t)) sha224Update;
	ctx->Digest = (int (*)(void *, byte *)) sha224Digest;
/*@=type@*/
	ctx->asn1 = "302d300d06096086480165030402040500041C";
	break;
    case PGPHASHALGO_SHA256:
	ctx->name = "SHA256";
	ctx->digestsize = 256/8;
	ctx->datasize = 64;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(sha256Param);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (int (*)(void *)) sha256Reset;
	ctx->Update = (int (*)(void *, const byte *, size_t)) sha256Update;
	ctx->Digest = (int (*)(void *, byte *)) sha256Digest;
/*@=type@*/
	ctx->asn1 = "3031300d060960864801650304020105000420";
	break;
    case PGPHASHALGO_SHA384:
	ctx->name = "SHA384";
	ctx->digestsize = 384/8;
	ctx->datasize = 128;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(sha384Param);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (int (*)(void *)) sha384Reset;
	ctx->Update = (int (*)(void *, const byte *, size_t)) sha384Update;
	ctx->Digest = (int (*)(void *, byte *)) sha384Digest;
/*@=type@*/
	ctx->asn1 = "3041300d060960864801650304020205000430";
	break;
    case PGPHASHALGO_SHA512:
	ctx->name = "SHA512";
	ctx->digestsize = 512/8;
	ctx->datasize = 128;
/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->paramsize = sizeof(sha512Param);
/*@=sizeoftype@*/
	ctx->param = xcalloc(1, ctx->paramsize);
/*@-type@*/
	ctx->Reset = (int (*)(void *)) sha512Reset;
	ctx->Update = (int (*)(void *, const byte *, size_t)) sha512Update;
	ctx->Digest = (int (*)(void *, byte *)) sha512Digest;
/*@=type@*/
	ctx->asn1 = "3051300d060960864801650304020305000440";
	break;
#endif
    case PGPHASHALGO_HAVAL_5_160:
    default:
	(void)rpmioFreePoolItem((rpmioItem)ctx, __FUNCTION__, __FILE__, __LINE__);
	return NULL;
	/*@notreached@*/ break;
    }

    xx = (*ctx->Reset) (ctx->param);

DPRINTF((stderr, "==> ctx %p ==== Init(%s, %x) param %p\n", ctx, ctx->name, flags, ctx->param));
    return (DIGEST_CTX)rpmioLinkPoolItem((rpmioItem)ctx, __FUNCTION__, __FILE__, __LINE__);
}

/*@-mustmod@*/ /* LCL: ctx->param may be modified, but ctx is abstract @*/
int
rpmDigestUpdate(DIGEST_CTX ctx, const void * data, size_t len)
{
    if (ctx == NULL)
	return -1;

DPRINTF((stderr, "==> ctx %p ==== Update(%s,%p[%u]) param %p\n", ctx, ctx->name, data, (unsigned)len, ctx->param));
    return (*ctx->Update) (ctx->param, data, len);
}
/*@=mustmod@*/

int
rpmDigestFinal(DIGEST_CTX ctx, void * datap, size_t *lenp, int asAscii)
{
    byte * digest;
    char * t;

    if (ctx == NULL)
	return -1;
    digest = xmalloc(ctx->digestsize);

DPRINTF((stderr, "==> ctx %p ==== Final(%s,%p,%p,%d) param %p digest %p\n", ctx, ctx->name, datap, lenp, asAscii, ctx->param, digest));
/*@-noeffectuncon@*/ /* FIX: check rc */
    (void) (*ctx->Digest) (ctx->param, digest);
/*@=noeffectuncon@*/

    /* Return final digest. */
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
	    size_t i;

	    *(char **)datap = t = xmalloc((2*ctx->digestsize) + 1);
	    for (i = 0 ; i < ctx->digestsize; i++) {
		*t++ = hex[ (unsigned)((*s >> 4) & 0x0f) ];
		*t++ = hex[ (unsigned)((*s++   ) & 0x0f) ];
	    }
	    *t = '\0';
	}
    }
    if (digest) {
	memset(digest, 0, ctx->digestsize);	/* In case it's sensitive */
	free(digest);
    }
    (void)rpmioFreePoolItem((rpmioItem)ctx, __FUNCTION__, __FILE__, __LINE__);
    return 0;
}

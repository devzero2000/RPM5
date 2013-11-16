/** \ingroup rpmio
 * \file rpmio/cphpt.c
 */

#include "system.h"

#include "rpmio_internal.h"

#include <rpmbc.h>

#include "debug.h"

#define	_CIPHER_DEBUG	-1
/*@unchecked@*/
int _cph_debug = _CIPHER_DEBUG;

#ifdef  _CIPHER_DEBUG
#define DPRINTF(_a)     if (_cph_debug < 0) fprintf _a
#else
#define DPRINTF(_a)
#endif

/*@access CIPHER_CTX@*/

/**
 * Cipher private data.
 */
struct CIPHER_CTX_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
/*@observer@*/
    const char * name;		/*!< Cipher name. */
    size_t paramsize;		/*!< No. bytes of digest parameters. */
    size_t blocksize;		/*!< No. bytes in block of plaintext data. */
    size_t keybitsmin;		/*!< Minimum no. of key bits. */
    size_t keybitsmax;		/*!< Maximum no. of key bits. */
    size_t keybitsinc;		/*!< Allowed increment in key bits. */
    int (*Setup) (void *, const byte *, size_t, int);
    int (*SetIV) (void *, const byte *);
    int (*SetCTR) (void *, const byte *, size_t);
    uint32_t * (*Feedback) (void *);
    int (*RAW) (void *, uint32_t *, const uint32_t *);
    int (*ECB) (void *, uint32_t *, const uint32_t *, unsigned int);
    int (*CBC) (void *, uint32_t *, const uint32_t *, unsigned int);
    int (*CTR) (void *, uint32_t *, const uint32_t *, unsigned int);

    pgpSymkeyAlgo symkeyalgo;	/*!< RFC 2440/4880 symkey algorithm id. */
    rpmCipherFlags flags;	/*!< Bit(s) to control cipher operation. */
    void * param;               /*!< Cipher parameters. */
};

static void cphFini(void * _cph)
	/*@modifies _cph @*/
{
    CIPHER_CTX cph = (CIPHER_CTX) _cph;
    if (cph->param != NULL && cph->paramsize > 0)
	memset(cph->param, 0, cph->paramsize);	/* In case it's sensitive */
    cph->param = _free(cph->param);
    cph->name = NULL;
    cph->paramsize = 0;
    cph->blocksize = 0;
    cph->keybitsmin = 0;
    cph->keybitsmax = 0;
    cph->keybitsinc = 0;
    cph->Setup = NULL;
    cph->SetIV = NULL;
    cph->SetCTR = NULL;
    cph->Feedback = NULL;
    cph->RAW = NULL;
    cph->ECB = NULL;
    cph->CBC = NULL;
    cph->CTR = NULL;

    cph->symkeyalgo = (pgpSymkeyAlgo)0;
    cph->flags = (rpmCipherFlags)0;
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _cphPool;

static CIPHER_CTX cphGetPool(rpmioPool pool)
{
    CIPHER_CTX cph;

    if (_cphPool == NULL) {
ANNOTATE_BENIGN_RACE(&_cphPool, "");
	_cphPool = rpmioNewPool("cph", sizeof(*cph), -1, _cph_debug,
			NULL, NULL, cphFini);
	pool = _cphPool;
    }
    cph = (CIPHER_CTX) rpmioGetPool(pool, sizeof(*cph));
    memset(((char *)cph)+sizeof(cph->_item), 0, sizeof(*cph)-sizeof(cph->_item));
    return cph;
}

CIPHER_CTX
rpmCipherInit(pgpSymkeyAlgo symkeyalgo, rpmCipherFlags flags)
{
    CIPHER_CTX cph = cphGetPool(_cphPool);

    cph->symkeyalgo = symkeyalgo;
    cph->flags = flags;

DPRINTF((stderr, "==> cph %p ==== Init(%s, %x) param %p\n", cph, cph->name, flags, cph->param));

    return (CIPHER_CTX)rpmioLinkPoolItem((rpmioItem)cph, __FUNCTION__, __FILE__, __LINE__);
}

int rpmCipherNext(/*@null@*/ CIPHER_CTX cph, void * data, size_t len)
{
    if (cph == NULL)
        return -1;

DPRINTF((stderr, "==> cph %p ==== Next(%s,%p[%u]) param %p\n", cph, cph->name, data, (unsigned)len, cph->param));

    return 0;
}

int rpmCipherFinal(/*@only@*/ /*@null@*/ CIPHER_CTX cph)
{
DPRINTF((stderr, "==> cph %p ==== Final(%s) param %p\n", cph, cph->name, cph->param));
    (void)rpmioFreePoolItem((rpmioItem)cph, __FUNCTION__, __FILE__, __LINE__);
    return 0;
}

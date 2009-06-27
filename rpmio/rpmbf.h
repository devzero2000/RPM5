#ifndef	H_RPMBF
#define	H_RPMBF

/** \ingroup rpmio
 * \file rpmio/rpmbf.h
 */

/** \ingroup rpmio
 */
/*@unchecked@*/
extern int _rpmbf_debug;

/** \ingroup rpmio
 */
typedef /*@refcounted@*/ struct rpmbf_s * rpmbf;

typedef	unsigned int __pbm_bits;

typedef struct {
    __pbm_bits bits[1];
} pbm_set;

#if defined(_RPMBF_INTERNAL)
/** \ingroup rpmio
 */
struct rpmbf_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
    size_t m;
    size_t n;
    size_t k;
/*@relnull@*/
    unsigned char * bits;
#if defined(__LCLINT__)
/*@refs@*/
    int nrefs;			/*!< (unused) keep splint happy */
#endif
};
#endif	/* _RPMBF_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_RPMBF_INTERNAL)

/* Bit mask macros. */
#define	__PBM_NBITS /*@-sizeoftype@*/(8 * sizeof(__pbm_bits))/*@=sizeoftype@*/
#define	__PBM_IX(d)	((d) / __PBM_NBITS)
#define __PBM_MASK(d)	((__pbm_bits) 1 << (((unsigned)(d)) % __PBM_NBITS))
#define	__PBM_BITS(set)	((__pbm_bits *)(set)->bits)

#define	PBM_FREE(s)	_free(s);
#define PBM_SET(d, s)   (__PBM_BITS (s)[__PBM_IX (d)] |= __PBM_MASK (d))
#define PBM_CLR(d, s)   (__PBM_BITS (s)[__PBM_IX (d)] &= ~__PBM_MASK (d))
#define PBM_ISSET(d, s) ((__PBM_BITS (s)[__PBM_IX (d)] & __PBM_MASK (d)) != 0)

#define	PBM_ALLOC(d)	xcalloc(__PBM_IX (d) + 1, __PBM_NBITS/8)

/**
 * Reallocate a bit map.
 * @retval sp		address of bit map pointer
 * @retval odp		no. of bits in map
 * @param nd		desired no. of bits
 */
/*@unused@*/
static inline pbm_set * PBM_REALLOC(pbm_set ** sp, int * odp, int nd)
	/*@modifies *sp, *odp @*/
{
    int i, nb;

    if (nd > (*odp)) {
	nd *= 2;
	nb = __PBM_IX(nd) + 1;
/*@-unqualifiedtrans@*/
	*sp = xrealloc(*sp, nb * (__PBM_NBITS/8));
/*@=unqualifiedtrans@*/
	for (i = __PBM_IX(*odp) + 1; i < nb; i++)
	    __PBM_BITS(*sp)[i] = 0;
	*odp = nd;
    }
/*@-compdef -retalias -usereleased@*/
    return *sp;
/*@=compdef =retalias =usereleased@*/
}

#endif	/* _RPMBF_INTERNAL */

/**
 * Unreference a Bloom filter instance.
 * @param bf		Bloom filter
 * @return		NULL on last dereference
 */
/*@unused@*/ /*@null@*/
rpmbf rpmbfUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmbf bf)
	/*@modifies bf @*/;
#define	rpmbfUnlink(_ds)	\
    ((rpmbf)rpmioUnlinkPoolItem((rpmioItem)(_bf), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a Bloom filter instance.
 * @param bf		Bloom filter
 * @return		new Bloom filter reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmbf rpmbfLink (/*@null@*/ rpmbf bf)
	/*@modifies bf @*/;
#define	rpmbfLink(_bf)	\
    ((rpmbf)rpmioLinkPoolItem((rpmioItem)(_bf), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a Bloom filter.
 * @param bf		Bloom filter
 * @return		NULL on last dereference
 */
/*@null@*/
rpmbf rpmbfFree(/*@killref@*/ /*@null@*/rpmbf bf)
	/*@modifies bf @*/;
#define	rpmbfFree(_bf)	\
    ((rpmbf)rpmioFreePoolItem((rpmioItem)(_bf), __FUNCTION__, __FILE__, __LINE__))

/**
 * Create a Bloom filter.
 * @param n
 * @param m
 * @param k
 * @param flags		flags
 * @return		new Bloom filter
 */
/*@newref@*/ /*@null@*/
rpmbf rpmbfNew(size_t n, size_t m, size_t k, unsigned flags)
	/*@*/;

/**
 * Add string to a Bloom filter.
 * @param bf		Bloom filter
 * @param s		string
 * @return		0 always
 */
int rpmbfAdd(rpmbf bf, const char * s)
	/*@modifies bf @*/;

/**
 * Clear a Bloom filter, discarding all set memberships.
 * @param bf		Bloom filter
 * @return		0 always
 */
int rpmbfClr(rpmbf bf)
	/*@modifies bf @*/;

/**
 * Check for string in a Bloom filter.
 * @param bf		Bloom filter
 * @param s		string
 * @return		1 if string is present, 0 if not
 */
int rpmbfChk(rpmbf bf, const char * s)
	/*@modifies bf @*/;

/**
 * Delete string from a Bloom filter.
 * @todo Counting bloom filter needed.
 * @param bf		Bloom filter
 * @param s		string
 * @return		0 always
 */
int rpmbfDel(rpmbf bf, const char * s)
	/*@modifies bf @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMBF */

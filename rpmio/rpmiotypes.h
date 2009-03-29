#ifndef _H_RPMIOTYPES_
#define	_H_RPMIOTYPES_

/** \ingroup rpmio
 * \file rpmio/rpmiotypes.h
 */

/** \ingroup rpmio
 * RPM return codes.
 */
typedef	enum rpmRC_e {
    RPMRC_OK		= 0,	/*!< Generic success code */
    RPMRC_NOTFOUND	= 1,	/*!< Generic not found code. */
    RPMRC_FAIL		= 2,	/*!< Generic failure code. */
    RPMRC_NOTTRUSTED	= 3,	/*!< Signature is OK, but key is not trusted. */
    RPMRC_NOKEY		= 4	/*!< Public key is unavailable. */
} rpmRC;

/**
 */
typedef	/*@refcounted@*/ struct rpmioItem_s * rpmioItem;
struct rpmioItem_s {
/*@null@*/
    void *use;			/*!< use count -- return to pool when zero */
/*@kept@*/ /*@null@*/
    void *pool;			/*!< pool (or NULL if malloc'd) */
#if defined(__LCLINT__)
/*@refs@*/
    int nrefs;			/*!< (unused) keep splint happy */
#endif
};

/**
 */
typedef struct rpmioPool_s * rpmioPool;

/** \ingroup rpmio
 */
typedef struct rpmiob_s * rpmiob;

/** \ingroup rpmio
 */
/*@unchecked@*/
extern size_t _rpmiob_chunk;

/** \ingroup rpmpgp
 */
typedef /*@abstract@*/ struct DIGEST_CTX_s * DIGEST_CTX;

/** \ingroup rpmpgp
 */
typedef /*@abstract@*/ struct pgpPkt_s * pgpPkt;

/** \ingroup rpmpgp
 */
typedef /*@abstract@*/ /*@refcounted@*/ struct pgpDig_s * pgpDig;

/** \ingroup rpmpgp
 */
typedef /*@abstract@*/ struct pgpDigParams_s * pgpDigParams;

/** \ingroup rpmpgp
 * Bit(s) to control digest and signature verification.
 */
typedef enum pgpVSFlags_e {
    RPMVSF_DEFAULT	= 0,
    RPMVSF_NOHDRCHK	= (1 <<  0),
    RPMVSF_NEEDPAYLOAD	= (1 <<  1),
    /* bit(s) 2-7 unused */
    RPMVSF_NOSHA1HEADER	= (1 <<  8),
    RPMVSF_NOMD5HEADER	= (1 <<  9),	/* unimplemented */
    RPMVSF_NODSAHEADER	= (1 << 10),
    RPMVSF_NORSAHEADER	= (1 << 11),
    /* bit(s) 12-15 unused */
    RPMVSF_NOSHA1	= (1 << 16),	/* unimplemented */
    RPMVSF_NOMD5	= (1 << 17),
    RPMVSF_NODSA	= (1 << 18),
    RPMVSF_NORSA	= (1 << 19)
    /* bit(s) 20-31 unused */
} pgpVSFlags;

#define	_RPMVSF_NODIGESTS	\
  ( RPMVSF_NOSHA1HEADER |	\
    RPMVSF_NOMD5HEADER |	\
    RPMVSF_NOSHA1 |		\
    RPMVSF_NOMD5 )

#define	_RPMVSF_NOSIGNATURES	\
  ( RPMVSF_NODSAHEADER |	\
    RPMVSF_NORSAHEADER |	\
    RPMVSF_NODSA |		\
    RPMVSF_NORSA )

#define	_RPMVSF_NOHEADER	\
  ( RPMVSF_NOSHA1HEADER |	\
    RPMVSF_NOMD5HEADER |	\
    RPMVSF_NODSAHEADER |	\
    RPMVSF_NORSAHEADER )

#define	_RPMVSF_NOPAYLOAD	\
  ( RPMVSF_NOSHA1 |		\
    RPMVSF_NOMD5 |		\
    RPMVSF_NODSA |		\
    RPMVSF_NORSA )

/*@-redef@*/ /* LCL: ??? */
typedef /*@abstract@*/ const void * fnpyKey;
/*@=redef@*/

/**
 * Bit(s) to identify progress callbacks.
 */
typedef enum rpmCallbackType_e {
    RPMCALLBACK_UNKNOWN         = 0,
    RPMCALLBACK_INST_PROGRESS   = (1 <<  0),
    RPMCALLBACK_INST_START      = (1 <<  1),
    RPMCALLBACK_INST_OPEN_FILE  = (1 <<  2),
    RPMCALLBACK_INST_CLOSE_FILE = (1 <<  3),
    RPMCALLBACK_TRANS_PROGRESS  = (1 <<  4),
    RPMCALLBACK_TRANS_START     = (1 <<  5),
    RPMCALLBACK_TRANS_STOP      = (1 <<  6),
    RPMCALLBACK_UNINST_PROGRESS = (1 <<  7),
    RPMCALLBACK_UNINST_START    = (1 <<  8),
    RPMCALLBACK_UNINST_STOP     = (1 <<  9),
    RPMCALLBACK_REPACKAGE_PROGRESS = (1 << 10),
    RPMCALLBACK_REPACKAGE_START = (1 << 11),
    RPMCALLBACK_REPACKAGE_STOP  = (1 << 12),
    RPMCALLBACK_UNPACK_ERROR    = (1 << 13),
    RPMCALLBACK_CPIO_ERROR      = (1 << 14),
    RPMCALLBACK_SCRIPT_ERROR    = (1 << 15)
} rpmCallbackType;

/**
 */
typedef void * rpmCallbackData;

#if defined(_RPMIOB_INTERNAL)
/** \ingroup rpmio
 */
struct rpmiob_s{
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
    uint8_t * b;		/*!< data octects. */
    size_t blen;		/*!< no. of octets used. */
    size_t allocated;		/*!< no. of octets allocated. */
#if defined(__LCLINT__)
/*@refs@*/
    int nrefs;				/*!< (unused) keep splint happy */
#endif
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmio
 */
typedef void * (*rpmCallbackFunction)
                (/*@null@*/ const void * h,
                const rpmCallbackType what,
                const uint64_t amount,
                const uint64_t total,
                /*@null@*/ fnpyKey key,
                /*@null@*/ rpmCallbackData data)
        /*@globals internalState@*/
        /*@modifies internalState@*/;

#if !defined(SWIG)
/** \ingroup rpmio
 * Wrapper to free(3), hides const compilation noise, permit NULL, return NULL.
 * @param p		memory to free
 * @return		NULL always
 */
#if defined(WITH_DMALLOC)
#define _free(p) ((p) != NULL ? free((void *)(p)) : (void)0, NULL)
#else
/*@unused@*/ static inline /*@null@*/
void * _free(/*@only@*/ /*@null@*/ /*@out@*/ const void * p)
	/*@modifies p @*/
{
    if (p != NULL)	free((void *)p);
    return NULL;
}
#endif
#endif

/*@unused@*/ static inline int xislower(int c) /*@*/ {
    return (c >= (int)'a' && c <= (int)'z');
}
/*@unused@*/ static inline int xisupper(int c) /*@*/ {
    return (c >= (int)'A' && c <= (int)'Z');
}
/*@unused@*/ static inline int xisalpha(int c) /*@*/ {
    return (xislower(c) || xisupper(c));
}
/*@unused@*/ static inline int xisdigit(int c) /*@*/ {
    return (c >= (int)'0' && c <= (int)'9');
}
/*@unused@*/ static inline int xisalnum(int c) /*@*/ {
    return (xisalpha(c) || xisdigit(c));
}
/*@unused@*/ static inline int xisblank(int c) /*@*/ {
    return (c == (int)' ' || c == (int)'\t');
}
/*@unused@*/ static inline int xisspace(int c) /*@*/ {
    return (xisblank(c) || c == (int)'\n' || c == (int)'\r' || c == (int)'\f' || c == (int)'\v');
}
/*@unused@*/ static inline int xiscntrl(int c) /*@*/ {
    return (c < (int)' ');
}
/*@unused@*/ static inline int xisascii(int c) /*@*/ {
    return ((c & 0x80) != 0x80);
}
/*@unused@*/ static inline int xisprint(int c) /*@*/ {
    return (c >= (int)' ' && xisascii(c));
}
/*@unused@*/ static inline int xisgraph(int c) /*@*/ {
    return (c > (int)' ' && xisascii(c));
}
/*@unused@*/ static inline int xispunct(int c) /*@*/ {
    return (xisgraph(c) && !xisalnum(c));
}

/*@unused@*/ static inline int xtolower(int c) /*@*/ {
    return ((xisupper(c)) ? (c | ('a' - 'A')) : c);
}
/*@unused@*/ static inline int xtoupper(int c) /*@*/ {
    return ((xislower(c)) ? (c & ~('a' - 'A')) : c);
}

/** \ingroup rpmio
 * Locale insensitive strcasecmp(3).
 */
int xstrcasecmp(const char * s1, const char * s2)		/*@*/;

/** \ingroup rpmio
 * Locale insensitive strncasecmp(3).
 */
int xstrncasecmp(const char *s1, const char * s2, size_t n)	/*@*/;

/** \ingroup rpmio
 * Force encoding of string.
 */
/*@only@*/ /*@null@*/
const char * xstrtolocale(/*@only@*/ const char *str)
	/*@modifies *str @*/;

/**
 * Unreference a I/O buffer instance.
 * @param iob		hash table
 * @return		NULL if free'd
 */
/*@unused@*/ /*@null@*/
rpmiob rpmiobUnlink (/*@killref@*/ /*@null@*/ rpmiob iob)
	/*@modifies iob @*/;
#define	rpmiobUnlink(_iob)	\
    ((rpmiob)rpmioUnlinkPoolItem((rpmioItem)(_iob), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a I/O buffer instance.
 * @param iob		I/O buffer
 * @return		new I/O buffer reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmiob rpmiobLink (/*@null@*/ rpmiob iob)
	/*@modifies iob @*/;
#define	rpmiobLink(_iob)	\
    ((rpmiob)rpmioLinkPoolItem((rpmioItem)(_iob), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a I/O buffer instance.
 * @param iob		I/O buffer
 * @return		NULL on last dereference
 */
/*@null@*/
rpmiob rpmiobFree( /*@only@*/ rpmiob iob)
	/*@modifies iob @*/;
#define	rpmiobFree(_iob)	\
    ((rpmiob)rpmioFreePoolItem((rpmioItem)(_iob), __FUNCTION__, __FILE__, __LINE__))

/**
 * Create an I/O buffer.
 * @param len		no. of octets to allocate
 * @return		new I/O buffer
 */
/*@only@*/
rpmiob rpmiobNew(size_t len)
	/*@*/;

/**
 * Empty an I/O buffer.
 * @param iob		I/O buffer
 * @return		I/O buffer
 */
rpmiob rpmiobEmpty(/*@returned@*/ rpmiob iob)
	/*@modifies iob @*/;

/**
 * Trim trailing white space.
 * @param iob		I/O buffer
 * @return		I/O buffer
 */
rpmiob rpmiobRTrim(/*@returned@*/ rpmiob iob)
	/*@modifies iob @*/;

/**
 * Append string to I/O buffer.
 * @param iob		I/O buffer
 * @param s		string
 * @param nl		append NL?
 * @return		I/O buffer
 */
rpmiob rpmiobAppend(/*@returned@*/ rpmiob iob, const char * s, size_t nl)
	/*@modifies iob @*/;

/**
 * Return I/O buffer.
 * @param iob		I/O buffer
 * @return		I/O buffer (as string)
 */
uint8_t * rpmiobBuf(rpmiob iob)
	/*@*/;

/**
 * Return I/O buffer (as string).
 * @param iob		I/O buffer
 * @return		I/O buffer (as string)
 */
char * rpmiobStr(rpmiob iob)
	/*@*/;

/**
 * Return I/O buffer len.
 * @param iob		I/O buffer
 * @return		I/O buffer length
 */
size_t rpmiobLen(rpmiob iob)
	/*@*/;

#if defined(_RPMIOB_INTERNAL)
/**
 * Read an entire file into a buffer.
 * @param fn		file name to read
 * @retval *iobp	I/O buffer
 * @return		0 on success
 */
int rpmiobSlurp(const char * fn, rpmiob * iobp)
        /*@globals h_errno, fileSystem, internalState @*/
        /*@modifies *iobp, fileSystem, internalState @*/;
#endif

#ifdef __cplusplus
}
#endif

#endif /* _H_RPMIOTYPES_ */

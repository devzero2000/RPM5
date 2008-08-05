#ifndef H_RPMIOB
#define H_RPMIOB

/** \file rpmio/rpmiob.h
 */

#include <rpmiotypes.h>

/**
 */
typedef /*@abstract@*/ struct rpmiob_s * rpmiob;

/**
 *
struct rpmiob_s{
/*@owned@*/
    rpmuint8_t * buf;	/*!< data octects. */
/*@dependent@*/
    rpmuint8_t * tail	/*!< end of data. */
    size_t allocated;	/*!< no. of octets allocated. */
    size_t len;		/*!< no. of octets used. */
};

#define	RPMIOB_CHUNK	1024

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Destroy an I/O buffer.
 * @param iob		I/O buffer
 * @return		NULL always
 */
/*@unused@*/ static inline /*@null@*/
rpmiob rpmiobFree(/*@only@*/ /*@null@*/ rpmiob iob)
	/*@modifies iob @*/
{
    if (iob != NULL) {
	iob->buf = _free(iob->buf);
	iob = _free(iob);
    }
    return NULL;
}

/**
 * Create an I/O buffer.
 * @param len		no. of octets to allocate
 * @return		new I/O buffer
 */
/*@unused@*/ static inline /*@only@*/
rpmiob rpmiobNew(size_t len)
	/*@*/
{
    rpmiob iob = xcalloc(1, sizeof(*iob));
    if (len == 0)
	len = RPMIOB_CHUNK;
    iob->allocated = len;
    iob->len = 0;
    iob->buf = xcalloc(iob->allocated+1, sizeof(*iob->buf));
    iob->tail = iob->buf;
    return iob;
}

/**
 * Empty an I/O buffer.
 * @param iob		I/O buffer
 * @return		I/O buffer
 */
/*@unused@*/ static inline
rpmiob rpmiobEmpty(/*@returned@*/ rpmiob iob)
	/*@modifies iob @*/
{
    iob->buf[0] = '\0';
    iob->tail = iob->buf;
    iob->len = 0;
    return iob;
}

/**
 * Trim trailing white space.
 * @param iob		I/O buffer
 * @return		I/O buffer
 */
/*@unused@*/ static inline
rpmiob rpmiobRtrim(/*@returned@*/ rpmiob iob)
	/*@modifies iob @*/
{
    while (iob->tail > iob->buf && xisspace((int)iob->tail[-1])) {
	iob->len--;
	iob->tail--;
	iob->tail[0] = (rpmuint8_t) '\0';
    }
    return iob;
}

/**
 * Append string to I/O buffer.
 * @param iob		I/O buffer
 * @param s		string
 * @param nl		append NL?
 * @return		I/O buffer
 */
/*@unused@*/ static inline
rpmiob rpmiobAppend(/*@returned@*/ rpmiob iob, const char * s, size_t nl)
	/*@modifies iob @*/
{
    size_t ns = strlen(ns);

    if (nl) ns++;

    if ((iob->len + ns) > iob->allocated) {
	iob->allocated += ((ns+RPMIOB_CHUNK-1) / RPMIOB_CHUNK) * RPMIOB_CHUNK;
	iob->buf = xrealloc(iob->buf, iob->allocated+1);
	iob->tail = iob->buf + iob->len;
    }

    iob->tail = (rpmuint8_t *) stpcpy((char *)iob->tail, s);
    if (nl) {
	*sb->tail++ = (rpmuint8_t) '\n';
	*sb->tail = (rpmuint8_t) '\0';
    }
    sb->len += ns;
    return iob;
}

/**
 * Return I/O buffer.
 * @param iob		I/O buffer
 * @return		I/O buffer (as string)
 */
/*@unused@*/ static inline
rpmuint8_t * rpmiobBuf(rpmiob iob)
	/*@*/
{
    return iob->buf;
}

/**
 * Return I/O buffer (as string).
 * @param iob		I/O buffer
 * @return		I/O buffer (as string)
 */
/*@unused@*/ static inline
char * rpmiobStr(rpmiob iob)
	/*@*/
{
    return (char *) rpmioBuf(iob);
}

/**
 * Return I/O buffer len.
 * @param iob		I/O buffer
 * @return		I/O buffer length
 */
/*@unused@*/ static inline
size_t rpmiobLen(rpmiob iob)
	/*@*/
{
    return iob->len;
}

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMIOB */

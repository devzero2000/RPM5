#ifndef H_RPMXAR
#define H_RPMXAR

/**
 * \file rpmio/rpmxar.h
 * Structure(s)and methods for a XAR archive wrapper format.
 */

#include <rpmiotypes.h>

/*@unchecked@*/
extern int _xar_debug;

typedef /*@abstract@*/ /*@refcounted@*/ struct rpmxar_s * rpmxar;

#ifdef	_RPMXAR_INTERNAL
#include "yarn.h"
struct rpmxar_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
/*@relnull@*/
    const void * x;		/*!< xar_t */
/*@relnull@*/
    const void * f;		/*!< xar_file_t */
/*@relnull@*/
    const void * i;		/*!< xar_iter_t */
/*@null@*/
    const char * member;	/*!< Current archive member. */
/*@null@*/
    unsigned char * b;		/*!< Data buffer. */
    size_t bsize;		/*!< No. bytes of data. */
    size_t bx;			/*!< Data byte index. */
    int first;
#if defined(__LCLINT__)
/*@refs@*/
    int nrefs;			/*!< (unused) keep splint happy */
#endif
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a xar archive instance.
 * @param xar		xar archive
 * @param msg
 * @return		NULL always
 */
/*@unused@*/ /*@null@*/
rpmxar rpmxarUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmxar xar,
		/*@null@*/ const char * msg)
	/*@modifies xar @*/;
#define	rpmxarUnlink(_xar, _msg)	\
    ((rpmxar)rpmioUnlinkPoolItem((rpmioItem)(_xar), _msg, __FILE__, __LINE__))

/**
 * Reference a xar archive instance.
 * @param xar		xar archive
 * @param msg
 * @return		new xar archive reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmxar rpmxarLink (/*@null@*/ rpmxar xar, /*@null@*/ const char * msg)
	/*@modifies xar @*/;
#define	rpmxarLink(_xar, _msg)	\
    ((rpmxar)rpmioLinkPoolItem((rpmioItem)(_xar), _msg, __FILE__, __LINE__))

/*@unused@*/ /*@null@*/
rpmxar rpmxarFree(/*@killref@*/ /*@only@*/ rpmxar xar,
		/*@null@*/ const char * msg)
	/*@modifies xar @*/;
#define	rpmxarFree(_xar, _msg)	\
    ((rpmxar)rpmioFreePoolItem((rpmioItem)(_xar), _msg, __FILE__, __LINE__))

/*@-globuse@*/
/*@relnull@*/
rpmxar rpmxarNew(const char * fn, const char * fmode)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
/*@=globuse@*/

int rpmxarNext(rpmxar xar)
	/*@globals fileSystem @*/
	/*@modifies xar, fileSystem @*/;

int rpmxarPush(rpmxar xar, const char * fn, unsigned char * b, size_t bsize)
	/*@globals fileSystem @*/
	/*@modifies xar, fileSystem @*/;

int rpmxarPull(rpmxar xar, /*@null@*/ const char * fn)
	/*@globals fileSystem @*/
	/*@modifies xar, fileSystem @*/;

int rpmxarSwapBuf(rpmxar xar, /*@null@*/ unsigned char * b, size_t bsize,
		/*@null@*/ unsigned char ** obp, /*@null@*/ size_t * obsizep)
	/*@globals fileSystem @*/
	/*@modifies xar, *obp, *obsizep, fileSystem @*/;

/*@-incondefs@*/
ssize_t xarRead(void * cookie, /*@out@*/ char * buf, size_t count)
        /*@globals fileSystem, internalState @*/
        /*@modifies buf, fileSystem, internalState @*/
	/*@requires maxSet(buf) >= (count - 1) @*/;
/*@=incondefs@*/

#ifdef __cplusplus
}
#endif

#endif  /* H_RPMXAR */

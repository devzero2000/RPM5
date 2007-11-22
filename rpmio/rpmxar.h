#ifndef H_RPMXAR
#define H_RPMXAR

/*@unchecked@*/
extern int _xar_debug;

typedef /*@abstract@*/ /*@refcounted@*/ struct rpmxar_s * rpmxar;

#ifdef	_RPMXAR_INTERNAL
struct rpmxar_s {
#ifdef HAVE_XAR_H
    xar_t x;
    xar_file_t f;
    xar_iter_t i;
#endif
/*@null@*/
    const char * member;	/*!< Current archive member. */
    char * b;			/*!< Data buffer. */
    size_t bsize;		/*!< No. bytes of data. */
    size_t bx;			/*!< Data byte index. */
    int first;
/*@refs@*/
    int nrefs;			/*!< Reference count. */
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

/** @todo Remove debugging entry from the ABI. */
/*@-exportlocal@*/
/*@null@*/
rpmxar XrpmxarUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmxar xar,
		/*@null@*/ const char * msg, const char * fn, unsigned ln)
	/*@modifies xar @*/;
/*@=exportlocal@*/
#define	rpmxarUnlink(_xar, _msg) XrpmxarUnlink(_xar, _msg, __FILE__, __LINE__)

/**
 * Reference a xar archive instance.
 * @param xar		xar archive
 * @param msg
 * @return		new xar archive reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmxar rpmxarLink (/*@null@*/ rpmxar xar, /*@null@*/ const char * msg)
	/*@modifies xar @*/;

/** @todo Remove debugging entry from the ABI. */
/*@newref@*/ /*@null@*/
rpmxar XrpmxarLink (/*@null@*/ rpmxar xar, /*@null@*/ const char * msg,
		const char * fn, unsigned ln)
        /*@modifies xar @*/;
#define	rpmxarLink(_xar, _msg)	XrpmxarLink(_xar, _msg, __FILE__, __LINE__)

/*@null@*/
rpmxar rpmxarFree(/*@only@*/ rpmxar xar)
	/*@modifies xar @*/;

/*@relnull@*/
rpmxar rpmxarNew(const char * fn, const char * fmode)
	/*@globals fileSystem @*/
	/*@modifies xar, fileSystem @*/;

int rpmxarNext(rpmxar xar)
	/*@globals fileSystem @*/
	/*@modifies xar, fileSystem @*/;

int rpmxarPush(rpmxar xar, const char * fn)
	/*@modifies xar @*/;

int rpmxarPull(rpmxar xar, /*@null@*/ const char * fn)
	/*@globals fileSystem @*/
	/*@modifies xar, fileSystem @*/;

int rpmxarSwapBuf(rpmxar xar, /*@null@*/ char * b, size_t bsize,
		/*@null@*/ char ** obp, /*@null@*/ size_t * obsizep)
	/*@modifies xar, *obp, *obsizep @*/;

ssize_t xarRead(void * cookie, /*@out@*/ char * buf, size_t count)
        /*@globals fileSystem, internalState @*/
        /*@modifies buf, fileSystem, internalState @*/
	/*@requires maxSet(buf) >= (count - 1) @*/
	/*@ensures maxRead(buf) == result @*/;

#ifdef __cplusplus
}
#endif

#endif  /* H_RPMXAR */

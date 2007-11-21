#ifndef H_RPMXAR
#define H_RPMXAR

/*@unchecked@*/
extern int _xar_debug;

typedef /*@abstract@*/ /*@refcounted@*/ struct rpmxar_s * rpmxar;

#ifdef	_RPMXAR_INTERNAL
struct rpmxar_s {
    int first;
    xar_t x;
    xar_file_t f;
    xar_iter_t i;
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

int rpmxarPush(rpmxar xar, const char * fn, void * b, size_t nb)
	/*@modifies xar @*/;

int rpmxarPull(rpmxar xar, /*@null@*/ const char * fn,
		/*@null@*/ void * bp, /*@null@*/ size_t * nbp)
	/*@globals fileSystem @*/
	/*@modifies xar, fileSystem @*/;

#ifdef __cplusplus
}
#endif

#endif  /* H_RPMXAR */

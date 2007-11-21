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

int rpmxarFini(rpmxar xar)
	/*@globals fileSystem @*/
	/*@modifies xar, fileSystem @*/;

int rpmxarInit(rpmxar xar, const char * fn, const char * fmode)
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

#ifndef H_RPMWF
#define H_RPMWF

/*@unchecked@*/
extern int _rpmwf_debug;

typedef /*@abstract@*/ /*@refcounted@*/ struct rpmwf_s * rpmwf;

#ifdef	_RPMWF_INTERNAL
struct rpmwf_s {
/*@relnull@*/
    const char * fn;
/*@relnull@*/
    FD_t fd;
/*@relnull@*/ /*@owned@*/
    void * b;
    size_t nb;
/*@relnull@*/ /*@dependent@*/
    char * l;
    size_t nl;
/*@relnull@*/ /*@dependent@*/
    char * s;
    size_t ns;
/*@relnull@*/ /*@dependent@*/
    char * h;
    size_t nh;
/*@relnull@*/ /*@dependent@*/
    char * p;
    size_t np;
#ifdef WITH_XAR
    xar_t x;
    xar_file_t f;
    xar_iter_t i;
#endif
    int first;
/*@refs@*/
    int nrefs;			/*!< Reference count. */
};
#endif


#ifdef __cplusplus
extern "C" {
#endif

rpmRC rpmwfFiniXAR(rpmwf wf)
	/*@globals fileSystem @*/
	/*@modifies wf, fileSystem @*/;

rpmRC rpmwfInitXAR(rpmwf wf, const char * fn, const char * fmode)
	/*@globals fileSystem @*/
	/*@modifies wf, fileSystem @*/;

rpmRC rpmwfNextXAR(rpmwf wf)
	/*@globals fileSystem @*/
	/*@modifies wf, fileSystem @*/;

rpmRC rpmwfPushXAR(rpmwf wf, const char * fn)
	/*@modifies wf @*/;

rpmRC rpmwfPullXAR(rpmwf wf, const char * fn)
	/*@globals fileSystem @*/
	/*@modifies wf, fileSystem @*/;

rpmRC rpmwfFiniRPM(rpmwf wf)
	/*@globals fileSystem @*/
	/*@modifies wf, fileSystem @*/;

rpmRC rpmwfInitRPM(rpmwf wf, const char * fn, const char * fmode)
	/*@globals fileSystem @*/
	/*@modifies wf, fileSystem @*/;

rpmRC rpmwfPushRPM(rpmwf wf, const char * fn)
	/*@globals fileSystem @*/
	/*@modifies wf, fileSystem @*/;

/**
 * Unreference a wrapper format instance.
 * @param wf		wrapper format
 * @param msg
 * @return		NULL always
 */
/*@unused@*/ /*@null@*/
rpmwf rpmwfUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmwf wf,
		/*@null@*/ const char * msg)
	/*@modifies wf @*/;

/** @todo Remove debugging entry from the ABI. */
/*@-exportlocal@*/
/*@null@*/
rpmwf XrpmwfUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmwf wf,
		/*@null@*/ const char * msg, const char * fn, unsigned ln)
	/*@modifies wf @*/;
/*@=exportlocal@*/
#define	rpmwfUnlink(_wf, _msg)	XrpmwfUnlink(_wf, _msg, __FILE__, __LINE__)

/**
 * Reference a wrapper format instance.
 * @param wf		wrapper format
 * @param msg
 * @return		new wrapper format reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmwf rpmwfLink (/*@null@*/ rpmwf wf, /*@null@*/ const char * msg)
	/*@modifies wf @*/;

/** @todo Remove debugging entry from the ABI. */
/*@newref@*/ /*@null@*/
rpmwf XrpmwfLink (/*@null@*/ rpmwf wf, /*@null@*/ const char * msg,
		const char * fn, unsigned ln)
        /*@modifies wf @*/;
#define	rpmwfLink(_wf, _msg)	XrpmwfLink(_wf, _msg, __FILE__, __LINE__)

/*@null@*/
rpmwf rpmwfFree(/*@only@*/ rpmwf wf)
	/*@globals fileSystem @*/
	/*@modifies wf, fileSystem @*/;

/*@relnull@*/
rpmwf rpmwfNew(const char * fn)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;

rpmwf rdRPM(const char * rpmfn)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;

rpmwf rdXAR(const char * xarfn)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;

rpmRC wrXAR(const char * xarfn, rpmwf wf)
	/*@globals fileSystem @*/
	/*@modifies wf, fileSystem @*/;

rpmRC wrRPM(const char * rpmfn, rpmwf wf)
	/*@globals fileSystem @*/
	/*@modifies wf, fileSystem @*/;


#ifdef __cplusplus
}
#endif

#endif  /* H_RPMWF */

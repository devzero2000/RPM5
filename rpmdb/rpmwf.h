#ifndef H_RPMWF
#define H_RPMWF

/*@unchecked@*/
extern int _rpmwf_debug;

typedef /*@abstract@*/ struct rpmwf_s * rpmwf;

#ifdef	_RPMWF_INTERNAL
struct rpmwf_s {
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

rpmwf rpmwfFree(/*@only@*/ rpmwf wf)
	/*@globals fileSystem @*/
	/*@modifies wf, fileSystem @*/;

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

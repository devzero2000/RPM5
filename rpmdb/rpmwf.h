#ifndef H_RPMWF
#define H_RPMWF

/*@unchecked@*/
extern int _rpmwf_debug;

typedef /*@abstract@*/ struct rpmwf_s * rpmwf;

#ifdef	_RPMWF_INTERNAL
struct rpmwf_s {
    const char * fn;
    FD_t fd;
    void * b;
    size_t nb;
    char * l;
    size_t nl;
    char * s;
    size_t ns;
    char * h;
    size_t nh;
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
	/*@*/;

rpmRC rpmwfInitXAR(rpmwf wf, const char * fn, const char * fmode)
	/*@*/;

rpmRC rpmwfNextXAR(rpmwf wf)
	/*@*/;

rpmRC rpmwfPushXAR(rpmwf wf, const char * fn)
	/*@*/;

rpmRC rpmwfPullXAR(rpmwf wf, const char * fn)
	/*@*/;

rpmRC rpmwfFiniRPM(rpmwf wf)
	/*@*/;

rpmRC rpmwfInitRPM(rpmwf wf, const char * fn, const char * fmode)
	/*@*/;

rpmRC rpmwfPushRPM(rpmwf wf, const char * fn)
	/*@*/;

rpmwf rpmwfFree(rpmwf wf)
	/*@*/;

rpmwf rpmwfNew(const char * fn)
	/*@*/;

rpmwf rdRPM(const char * rpmfn)
	/*@*/;

rpmwf rdXAR(const char * xarfn)
	/*@*/;

rpmRC wrXAR(const char * xarfn, rpmwf wf)
	/*@*/;

rpmRC wrRPM(const char * rpmfn, rpmwf wf)
	/*@*/;


#ifdef __cplusplus
}
#endif

#endif  /* H_RPMWF */

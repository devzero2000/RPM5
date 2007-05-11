#ifndef H_RPMEVR
#define H_RPMEVR

/** \ingroup rpmds
 * \file lib/rpmevr.h
 * Structure(s) and routine(s) used for EVR parsing and comparison.
 */

/**
 */
/*@-exportlocal@*/
/*@unchecked@*/
extern int _rpmevr_debug;
/*@=exportlocal@*/

typedef	/*@abstract@*/ struct EVR_s * EVR_t;

#if defined(_RPMEVR_INTERNAL)
/** \ingroup rpmds
 * An EVR parsing container.
 */
struct EVR_s {
    const char * str;		/*!< EVR storage */
/*@observer@*/ /*@null@*/
    const char * E;		/*!< Epoch */
    unsigned long Elong;
/*@observer@*/ /*@null@*/
    const char * V;		/*!< Version */
/*@observer@*/ /*@null@*/
    const char * R;		/*!< Release */
};
#endif	/* _RPMEVR_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmds
 * Segmented string compare.
 * @param a		1st string
 * @param b		2nd string
 * @return		+1 if a is "newer", 0 if equal, -1 if b is "newer"
 */
int rpmEVRcmp(const char *a, const char *b)
	/*@*/;

/** \ingroup rpmds
 * Split EVR string into epoch, version, and release components.
 * @param evrstr	[epoch:]version[-release] string
 * @retval *evr		parse results
 * @return		0 always
 */
int rpmEVRparse(const char * evrstr, EVR_t evr)
	/*@modifies evrstr, evr @*/;

/** \ingroup rpmds
 * Compare EVR containers.
 * @param a		1st EVR container
 * @param b		2nd EVR container
 * @return		+1 if a is "newer", 0 if equal, -1 if b is "newer"
 */
int rpmEVRcompare(const EVR_t a, const EVR_t b)
	/*@*/;

/** \ingroup rpmds
 * Segmented string compare vector.
 * @param a		1st string
 * @param b		2nd string
 * @return		+1 if a is "newer", 0 if equal, -1 if b is "newer"
 */
extern int (*rpmvercmp)(const char *a, const char *b)
	/*@*/;
#ifdef __cplusplus
}
#endif

#endif	/* H_RPMEVR */

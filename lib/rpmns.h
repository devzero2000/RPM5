#ifndef H_RPMNS
#define H_RPMNS

/** \ingroup rpmds
 * \file lib/rpmns.h
 * Structure(s) and routine(s) used for classifying and parsing names.
 */

/**
 */
/*@-exportlocal@*/
/*@unchecked@*/
extern int _rpmns_debug;
/*@=exportlocal@*/

typedef	/*@abstract@*/ struct rpmns_s * rpmns;

/**
 * Dependency types
 */
typedef enum nsType_e {
    RPMNS_TYPE_UNKNOWN	=  0,
    RPMNS_TYPE_STRING	=  (1 << 0),	/*!< unclassified string */
    RPMNS_TYPE_PATH	=  (1 << 1),	/*!< /bin */
    RPMNS_TYPE_SONAME	=  (1 << 2),	/*!< libc.so.6 */
    RPMNS_TYPE_MACRO	=  (1 << 3),	/*!< %{foo} */
    RPMNS_TYPE_COMPOUND	=  (1 << 4),	/*!< foo.arch */
    RPMNS_TYPE_NAMESPACE=  (1 << 8)	/*!< foo(bar) */
} nsType;

#if defined(_RPMNS_INTERNAL)
/** \ingroup rpmds
 * An NS parsing container.
 */
struct rpmns_s {
    const char * str;		/*!< NS storage */
    nsType Type;		/*!< Type */
/*@observer@*/ /*@null@*/
    const char * NS;		/*!< Namespace */
/*@observer@*/ /*@null@*/
    const char * N;		/*!< Name */
/*@observer@*/ /*@null@*/
    const char * A;		/*!< Arch */
};
#endif	/* _RPMNS_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmds
 * Classify a string as a dependency type.
 * @param str		string like "bing(bang).boom"
 * @return		dependency type
 */
nsType rpmnsClassify(const char * str)
	/*@*/;

/** \ingroup rpmds
 * Split NS string into namespace, name and arch components.
 * @param str		string like "bing(bang).boom"
 * @retval *ns		parse results
 * @return		0 always
 */
int rpmnsParse(const char * str, rpmns ns)
	/*@modifies ns @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMNS */

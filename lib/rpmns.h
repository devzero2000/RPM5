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

/*@unchecked@*/
extern const char *_rpmns_N_at_A;

typedef	/*@abstract@*/ struct rpmns_s * rpmns;

/**
 * Dependency types
 */
typedef enum nsType_e {
    RPMNS_TYPE_UNKNOWN	=  0,
    RPMNS_TYPE_STRING	=  (1 <<  0),	/*!< unclassified string */
    RPMNS_TYPE_PATH	=  (1 <<  1),	/*!< /bin */
    RPMNS_TYPE_DSO	=  (1 <<  2),	/*!< libc.so.6 */
    RPMNS_TYPE_FUNCTION	=  (1 <<  3),	/*!< %{foo} */
    RPMNS_TYPE_ARCH	=  (1 <<  4),	/*!< foo.arch */
    RPMNS_TYPE_VERSION	=  (1 <<  5),	/*!< foo-1.2.3-bar */
    RPMNS_TYPE_COMPOUND	=  (1 <<  6),	/*!< foo.bar */
	/* 7 unused */
    RPMNS_TYPE_NAMESPACE=  (1 <<  8),	/*!< foo(bar) */
    RPMNS_TYPE_RPMLIB	=  (1 <<  9),	/*!< rpmlib(bar) */
    RPMNS_TYPE_CPUINFO	=  (1 << 10),	/*!< cpuinfo(bar) */
    RPMNS_TYPE_GETCONF	=  (1 << 11),	/*!< getconf(bar) */
    RPMNS_TYPE_UNAME	=  (1 << 12),	/*!< uname(bar) */
    RPMNS_TYPE_SONAME	=  (1 << 13),	/*!< soname(bar) */
    RPMNS_TYPE_ACCESS	=  (1 << 14),	/*!< exists(bar) */
    RPMNS_TYPE_TAG	=  (1 << 15),	/*!< Tag(bar) */
    RPMNS_TYPE_USER	=  (1 << 16),	/*!< user(bar) */
    RPMNS_TYPE_GROUP	=  (1 << 17),	/*!< group(bar) */
    RPMNS_TYPE_MOUNTED	=  (1 << 18),	/*!< mounted(/path) */
    RPMNS_TYPE_DISKSPACE=  (1 << 19),	/*!< diskspace(/path) */
    RPMNS_TYPE_DIGEST	=  (1 << 20),	/*!< digest(md5:/path) = hex */
    RPMNS_TYPE_GNUPG	=  (1 << 21),	/*!< gnupg(/path/file.asc) */
} nsType;

#if defined(_RPMNS_INTERNAL)
/** \ingroup rpmds
 * An NS parsing container.
 */
struct rpmns_s {
    const char * str;		/*!< string storage */
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
 * Is string a known arch suffix?
 * @param str		string
 * @return		RPMNS_TYPE_ARCH if known arch, else RPMNS_TYPE_UNKNOWN
 */
nsType rpmnsArch(const char * str)
	/*@*/;

/** \ingroup rpmds
 * Is string a known probe namespace?
 * @param str		string
 * @return		nsType if known probe, else RPMNS_TYPE_UNKNOWN
 */
nsType rpmnsProbe(const char * str)
	/*@*/;

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

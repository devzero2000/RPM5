#ifndef H_HEADER
#define H_HEADER

/** \ingroup header
 * \file rpmdb/header.h
 *
 * An rpm header carries all information about a package. A header is
 * a collection of data elements called tags. Each tag has a data type,
 * and includes 1 or more values.
 * 
 * \par Historical Issues
 *
 * Here's a brief description of features/incompatibilities that
 * have been added to headers and tags.
 *
 * - version 1
 *	- Support for version 1 headers was removed in rpm-4.0.
 *
 * - version 2
 *	- @todo Document version2 headers.
 *
 * - version 3	(added in rpm-3.0)
 *	- Added RPM_I18NSTRING_TYPE as an associative array reference
 *	  for i18n locale dependent single element tags (i.e Group).
 *	- Added an 8 byte magic string to headers in packages on-disk. The
 *	  magic string was not added to headers in the database.
 *
 * - version 4	(added in rpm-4.0)
 *	- Represent file names as a (dirname/basename/dirindex) triple
 *	  rather than as an absolute path name. Legacy package headers are
 *	  converted when the header is read. Legacy database headers are
 *	  converted when the database is rebuilt.
 *	- Simplify dependencies by eliminating the implict check on
 *	  package name/version/release in favor of an explict check
 *	  on package provides. Legacy package headers are converted
 *	  when the header is read. Legacy database headers are
 *        converted when the database is rebuilt.
 *	- (rpm-4.0.2) The original package header (and all original
 *	  metadata) is preserved in what's called an "immutable header region".
 *	  The original header can be retrieved as an RPM_BIN_TYPE, just
 *	  like any other tag, and the original header reconstituted using
 *	  headerLoad().
 *	- (rpm-4.0.2) The signature tags are added (and renumbered to avoid
 *	  tag value collisions) to the package header during package
 *	  installation.
 *	- (rpm-4.0.3) A SHA1 digest of the original header is appended
 *	  (i.e. detached digest) to the immutable header region to verify
 *	  changes to the original header.
 *	- (rpm-4.0.3) Private methods (e.g. headerLoad(), headerUnload(), etc.)
 *	  to permit header data to be manipulated opaquely through vectors.
 *	- (rpm-4.0.3) Sanity checks on header data to limit no. of tags to 65K,
 *	  no. of bytes to 16Mb, and total metadata size to 32Mb added.
 *	- with addition of tracking dependencies, the package version has been
 *	  reverted back to 3.
 * .
 *
 * \par Development Issues
 *
 * Here's a brief description of future features/incompatibilities that
 * will be added to headers.
 *
 * - Private header methods.
 *	- Private methods for the transaction element file info rpmfi may
 *	  be used as proof-of-concept, binary XML may be implemented
 *	  as a header format representation soon thereafter.
 * - DSA signature for header metadata.
 *	- The manner in which rpm packages are signed is going to change.
 *	  The SHA1 digest in the header will be signed, equivalent to a DSA
 *	  digital signature on the original header metadata. As the original
 *	  header will contain "trusted" (i.e. because the header is signed
 *	  with DSA) file MD5 digests, there will be little or no reason
 *	  to sign the payload, but that may happen as well. Note that cpio
 *	  headers in the payload are not used to install package metadata,
 *	  only the name field in the cpio header is used to associate an
 *	  archive file member with the corresponding entry for the file
 *	  in header metadata.
 * .
 */

/* RPM - Copyright (C) 1995-2001 Red Hat Software */

#include <rpmsw.h>

#ifdef __cplusplus
extern "C" {
#endif

/* XXX hpux needs -Ae in CFLAGS to grok this */
typedef long long int int_64;
typedef int int_32;
typedef short int int_16;
typedef char int_8;

/* XXX hpux needs -Ae in CFLAGS to grok this */
typedef unsigned long long int uint_64;
typedef unsigned int uint_32;
typedef unsigned short uint_16;
typedef unsigned char uint_8;

/** \ingroup header
 */
typedef const char *	errmsg_t;

/** \ingroup header
 */
typedef /*@abstract@*/ /*@refcounted@*/ struct headerToken_s * Header;

/** \ingroup header
 */
typedef enum rpmTag_e rpmTag;

/** \ingroup header
 * The basic types of data in tags from headers.
 */
enum rpmTagType_e {
    RPM_NULL_TYPE		=  0,
    RPM_CHAR_TYPE		=  1,
    RPM_INT8_TYPE		=  2,
    RPM_INT16_TYPE		=  3,
    RPM_INT32_TYPE		=  4,
    RPM_INT64_TYPE		=  5,
    RPM_STRING_TYPE		=  6,
    RPM_BIN_TYPE		=  7,
    RPM_STRING_ARRAY_TYPE	=  8,
    RPM_I18NSTRING_TYPE		=  9,
    RPM_ASN1_TYPE		= 10,
    RPM_OPENPGP_TYPE		= 11,
    RPM_MASK_TYPE		= 0x0000ffff
};
#define	RPM_MIN_TYPE		0
#define	RPM_MAX_TYPE		11

/** \ingroup header
 */
typedef enum rpmTagType_e rpmTagType;

/** \ingroup header
 */
typedef union rpmDataType_u rpmTagData;

/** \ingroup header
 */
typedef int_32 rpmTagCount;

/** \ingroup header
 */
typedef struct _HE_s * HE_t;		/* tag container. */

/** \ingroup header
 */
/*@-typeuse -fielduse@*/
typedef union rpmDataType_u hRET_t;
#if !defined(SWIG)
union rpmDataType_u {
    void * ptr;
    int_8 * i8p;		/*!< RPM_INT8_TYPE | RPM_CHAR_TYPE */
    int_32 * i32p;		/*!< RPM_INT32_TYPE */
    int_16 * i16p;		/*!< RPM_INT16_TYPE */
    int_64 * i64p;		/*!< RPM_INT64_TYPE */
    const char * str;		/*!< RPM_STRING_TYPE */
    unsigned char * blob;	/*!< RPM_BIN_TYPE */
    const char ** argv;		/*!< RPM_STRING_ARRAY_TYPE */
    uint_8 * ui8p;
    uint_16 * ui16p;
    uint_32 * ui32p;
    uint_64 * ui64p;
    HE_t * he;
};
#endif
/*@=typeuse =fielduse@*/

/** \ingroup header
 */
typedef int_32 *	hTAG_t;
typedef rpmTagType *	hTYP_t;
typedef rpmTagData *	hPTR_t;
typedef rpmTagCount *	hCNT_t;

/** \ingroup header
 */
/*@-typeuse -fielduse@*/
#if !defined(SWIG)
struct _HE_s {
    int_32 tag;
    rpmTagType t;
/*@owned@*/ /*@null@*/
    rpmTagData p;
    rpmTagCount c;
    int ix;
    unsigned int freeData	: 1;
    unsigned int avail		: 1;
    unsigned int append		: 1;
};
typedef struct _HE_s HE_s;
#endif
/*@=typeuse =fielduse@*/

/** \ingroup header
 */
typedef /*@abstract@*/ struct headerIterator_s * HeaderIterator;

/** \ingroup header
 * Associate tag names with numeric values.
 */
typedef /*@abstract@*/ struct headerTagTableEntry_s * headerTagTableEntry;
#if !defined(SWIG)
struct headerTagTableEntry_s {
/*@observer@*/ /*@relnull@*/
    const char * name;		/*!< Tag name. */
    int val;			/*!< Tag numeric value. */
    int type;			/*!< Tag type. */
};
#endif

/**
 */
typedef /*@abstract@*/ struct headerTagIndices_s * headerTagIndices;
#if !defined(SWIG)
struct headerTagIndices_s {
    int (*loadIndex) (headerTagTableEntry ** ipp, int * np,
                int (*cmp) (const void * avp, const void * bvp))
        /*@ modifies *ipp, *np */;	/*!< load sorted tag index. */
/*@relnull@*/
    headerTagTableEntry * byName;	/*!< header tags sorted by name. */
    int byNameSize;			/*!< no. of entries. */
    int (*byNameCmp) (const void * avp, const void * bvp)
        /*@*/;				/*!< compare entries by name. */
    int (*tagValue) (const char * name)
	/*@*/;				/* return value from name. */
/*@relnull@*/
    headerTagTableEntry * byValue;	/*!< header tags sorted by value. */
    int byValueSize;			/*!< no. of entries. */
    int (*byValueCmp) (const void * avp, const void * bvp)
        /*@*/;				/*!< compare entries by value. */
    const char * (*tagName) (int value)
	/*@*/;				/* Return name from value. */
    int (*tagType) (int value)
	/*@*/;				/* Return type from value. */
};
#endif

/** \ingroup header
 */
enum headerSprintfExtensionType {
    HEADER_EXT_LAST = 0,	/*!< End of extension chain. */
    HEADER_EXT_FORMAT,		/*!< headerTagFormatFunction() extension */
    HEADER_EXT_MORE,		/*!< Chain to next table. */
    HEADER_EXT_TAG		/*!< headerTagTagFunction() extension */
};

/** \ingroup header
 * HEADER_EXT_TAG format function prototype.
 *
 * @param he		tag container
 * @return		formatted string
 */
typedef /*only@*/ char * (*headerTagFormatFunction)(HE_t he)
	/*@modifies he @*/;

/** \ingroup header
 * HEADER_EXT_FORMAT format function prototype.
 * This is allowed to fail, which indicates the tag doesn't exist.
 *
 * @param h		header
 * @retval he		tag container
 * @return		0 on success
 */
typedef int (*headerTagTagFunction) (Header h, HE_t he)
	/*@modifies he */;

/** \ingroup header
 * Define header tag output formats.
 */
typedef /*@abstract@*/ struct headerSprintfExtension_s * headerSprintfExtension;
#if !defined(SWIG)
struct headerSprintfExtension_s {
    enum headerSprintfExtensionType type;	/*!< Type of extension. */
/*@observer@*/ /*@null@*/
    const char * name;				/*!< Name of extension. */
    union {
/*@observer@*/ /*@null@*/
	void * generic;				/*!< Private extension. */
	headerTagFormatFunction formatFunction; /*!< HEADER_EXT_TAG extension. */
	headerTagTagFunction tagFunction;	/*!< HEADER_EXT_FORMAT extension. */
	struct headerSprintfExtension_s * more;	/*!< Chained table extension. */
    } u;
};
#endif

/** \ingroup header
 * Supported default header tag output formats.
 */
/*@-redecl@*/
/*@observer@*/
extern const struct headerSprintfExtension_s headerDefaultFormats[];
/*@=redecl@*/

/** \ingroup header
 * Supported default header extension/tag output formats.
 */
/*@-redecl@*/
/*@observer@*/
extern const struct headerSprintfExtension_s headerCompoundFormats[];
/*@=redecl@*/

/** \ingroup header
 * New rpm data types under consideration/development.
 * These data types may (or may not) be added to rpm at some point. In order
 * to avoid incompatibility with legacy versions of rpm, these data (sub-)types
 * are introduced into the header by overloading RPM_BIN_TYPE, with the binary
 * value of the tag a 16 byte image of what should/will be in the header index,
 * followed by per-tag private data.
 */
/*@-enummemuse -typeuse @*/
typedef enum rpmSubTagType_e {
    RPM_REGION_TYPE		= -10,
    RPM_BIN_ARRAY_TYPE		= -11,
  /*!<@todo Implement, kinda like RPM_STRING_ARRAY_TYPE for known (but variable)
	length binary data. */
    RPM_XREF_TYPE		= -12
  /*!<@todo Implement, intent is to to carry a (???,tagNum,valNum) cross
	reference to retrieve data from other tags. */
} rpmSubTagType;
/*@=enummemuse =typeuse @*/

/** \ingroup header
 * Identify how to return the header data type.
 */
/*@-enummemuse -typeuse @*/
typedef enum rpmTagReturnType_e {
    RPM_ANY_RETURN_TYPE		= 0,
    RPM_SCALAR_RETURN_TYPE	= 0x00010000,
    RPM_ARRAY_RETURN_TYPE	= 0x00020000,
    RPM_MAPPING_RETURN_TYPE	= 0x00040000,
    RPM_MASK_RETURN_TYPE	= 0xffff0000
} rpmTagReturnType;
/*@=enummemuse =typeuse @*/

/**
 * Header private tags.
 * @note General use tags should start at 1000 (RPM's tag space starts there).
 */
#define	HEADER_IMAGE		61
#define	HEADER_SIGNATURES	62
#define	HEADER_IMMUTABLE	63
#define	HEADER_REGIONS		64
#define HEADER_I18NTABLE	100
#define	HEADER_SIGBASE		256
#define	HEADER_TAGBASE		1000

/**
 * Prototype for headerFreeData() vector.
 *
 * @param data		address of data (or NULL)
 * @param type		type of data (or -1 to force free)
 * @return		NULL always
 */
typedef /*@null@*/
    void * (*HFD_t) (/*@only@*/ /*@null@*/ const void * data, rpmTagType type)
	/*@modifies data @*/;

/*
 * Retrieve extension or tag value.
 *
 * @param h		header
 * @param he		tag container
 * @param flags		(unused)
 * @return		1 on success, 0 on failure
 */
typedef int (*HGE_t) (Header h, HE_t he, unsigned int flags)
	/*@modifies *he @*/;

/**
 * Prototype for headerAddEntry() vector.
 *
 * Duplicate tags are okay, but only defined for iteration (with the
 * exceptions noted below). While you are allowed to add i18n string
 * arrays through this function, you probably don't mean to. See
 * headerAddI18NString() instead.
 *
 * @param h             header
 * @param tag           tag
 * @param type          tag value data type
 * @param p             tag value(s)
 * @param c             number of values
 * @return              1 on success, 0 on failure
 */
typedef int (*HAE_t) (Header h, rpmTag tag, rpmTagType type,
			const void * p, rpmTagCount c)
	/*@modifies h @*/;

/**
 * Prototype for headerModifyEntry() vector.
 * If there are multiple entries with this tag, the first one gets replaced.
 *
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
typedef int (*HME_t) (Header h, rpmTag tag, rpmTagType type,
			const void * p, rpmTagCount c)
	/*@modifies h @*/;

/**
 * Prototype for headerRemoveEntry() vector.
 * Delete tag in header.
 * Removes all entries of type tag from the header, returns 1 if none were
 * found.
 *
 * @param h		header
 * @param tag		tag
 * @return		0 on success, 1 on failure (INCONSISTENT)
 */
typedef int (*HRE_t) (Header h, int_32 tag)
	/*@modifies h @*/;

/** \ingroup header
 * Create new (empty) header instance.
 * @return		header
 */
typedef
Header (*HDRnew) (void)
	/*@*/;

/** \ingroup header
 * Dereference a header instance.
 * @param h		header
 * @return		NULL always
 */
typedef
/*@null@*/ Header (*HDRfree) (/*@killref@*/ /*@null@*/ Header h)
        /*@modifies h @*/;

/** \ingroup header
 * Reference a header instance.
 * @param h		header
 * @return		referenced header instance
 */
typedef
Header (*HDRlink) (Header h)
        /*@modifies h @*/;

/** \ingroup header
 * Dereference a header instance.
 * @param h		header
 * @return		NULL always
 */
typedef
Header (*HDRunlink) (/*@killref@*/ /*@null@*/ Header h)
        /*@modifies h @*/;

/** \ingroup header
 * Sort tags in header.
 * @todo Eliminate from API.
 * @param h		header
 */
typedef
void (*HDRsort) (Header h)
        /*@modifies h @*/;

/** \ingroup header
 * Restore tags in header to original ordering.
 * @todo Eliminate from API.
 * @param h		header
 */
typedef
void (*HDRunsort) (Header h)
        /*@modifies h @*/;

/** \ingroup header
 * Return size of on-disk header representation in bytes.
 * @param h		header
 * @return		size of on-disk header
 */
typedef
unsigned int (*HDRsizeof) (/*@null@*/ Header h)
        /*@modifies h @*/;

/** \ingroup header
 * Convert header to on-disk representation.
 * @param h		header (with pointers)
 * @retval *lenp	length of header in bytes (or NULL)
 * @return		on-disk header blob (i.e. with offsets)
 */
typedef
/*@only@*/ /*@null@*/ void * (*HDRunload) (Header h, /*@out@*/ /*@null@*/ size_t * lenp)
        /*@modifies h, *lenp @*/;

/** \ingroup header
 * Convert header to on-disk representation, and then reload.
 * This is used to insure that all header data is in one chunk.
 * @param h		header (with pointers)
 * @param tag		region tag
 * @return		on-disk header (with offsets)
 */
typedef
/*@null@*/ Header (*HDRreload) (/*@only@*/ Header h, int tag)
        /*@modifies h @*/;

/** \ingroup header
 * Duplicate a header.
 * @param h		header
 * @return		new header instance
 */
typedef
Header (*HDRcopy) (Header h)
        /*@modifies h @*/;

/** \ingroup header
 * Convert header to in-memory representation.
 * @param uh		on-disk header blob (i.e. with offsets)
 * @return		header
 */
typedef
/*@null@*/ Header (*HDRload) (/*@kept@*/ void * uh)
	/*@modifies uh @*/;

/** \ingroup header
 * Make a copy and convert header to in-memory representation.
 * @param uh		on-disk header blob (i.e. with offsets)
 * @return		header
 */
typedef
/*@null@*/ Header (*HDRcopyload) (const void * uh)
	/*@*/;

/** \ingroup header
 * Read (and load) header from file handle.
 * @param fd		file handle
 * @return		header (or NULL on error)
 */
typedef
/*@null@*/ Header (*HDRread) (void * fd)
	/*@modifies fd @*/;

/** \ingroup header
 * Write (with unload) header to file handle.
 * @param fd		file handle
 * @param h		header
 * @return		0 on success, 1 on error
 */
typedef
int (*HDRwrite) (void * fd, /*@null@*/ Header h)
	/*@globals fileSystem @*/
	/*@modifies fd, h, fileSystem @*/;

/** \ingroup header
 * Check if tag is in header.
 * @param h		header
 * @param tag		tag
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRisentry) (/*@null@*/Header h, int_32 tag)
        /*@*/;  

/** \ingroup header
 * Free data allocated when retrieved from header.
 * @param h		header
 * @param data		address of data (or NULL)
 * @param type		type of data (or -1 to force free)
 * @return		NULL always
 */
typedef
/*@null@*/ void * (*HDRfreetag) (Header h,
		/*@only@*/ /*@null@*/ const void * data, rpmTagType type)
	/*@modifies data @*/;

/** \ingroup header
 * Retrieve extension or tag value.
 *
 * @param h		header
 * @param tag		tag
 * @retval *type	tag value data type (or NULL)
 * @retval *p		tag value(s) (or NULL)
 * @retval *c		number of values (or NULL)
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRext) (Header h, int_32 tag,
			/*@null@*/ /*@out@*/ hTYP_t type,
			/*@null@*/ /*@out@*/ hRET_t * p,
			/*@null@*/ /*@out@*/ hCNT_t c)
	/*@modifies *type, *p, *c @*/;

/** \ingroup header
 * Retrieve tag value.
 * Will never return RPM_I18NSTRING_TYPE! RPM_STRING_TYPE elements with
 * RPM_I18NSTRING_TYPE equivalent entries are translated (if HEADER_I18NTABLE
 * entry is present).
 *
 * @param h		header
 * @param tag		tag
 * @retval *type	tag value data type (or NULL)
 * @retval *p		tag value(s) (or NULL)
 * @retval *c		number of values (or NULL)
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRget) (Header h, int_32 tag,
			/*@null@*/ /*@out@*/ hTYP_t type,
			/*@null@*/ /*@out@*/ hRET_t * p,
			/*@null@*/ /*@out@*/ hCNT_t c)
	/*@modifies *type, *p, *c @*/;

/** \ingroup header
 * Retrieve tag value using header internal array.
 * Get an entry using as little extra RAM as possible to return the tag value.
 * This is only an issue for RPM_STRING_ARRAY_TYPE.
 *
 * @param h		header
 * @param tag		tag
 * @retval *type	tag value data type (or NULL)
 * @retval *p		tag value(s) (or NULL)
 * @retval *c		number of values (or NULL)
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRgetmin) (Header h, int_32 tag,
			/*@null@*/ /*@out@*/ hTYP_t type,
			/*@null@*/ /*@out@*/ hRET_t * p,
			/*@null@*/ /*@out@*/ hCNT_t c)
	/*@modifies *type, *p, *c @*/;

/** \ingroup header
 * Add tag to header.
 * Duplicate tags are okay, but only defined for iteration (with the
 * exceptions noted below). While you are allowed to add i18n string
 * arrays through this function, you probably don't mean to. See
 * headerAddI18NString() instead.
 *
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRadd) (Header h, int_32 tag, rpmTagType type, const void * p, rpmTagCount c)
        /*@modifies h @*/;

/** \ingroup header
 * Append element to tag array in header.
 * Appends item p to entry w/ tag and type as passed. Won't work on
 * RPM_STRING_TYPE. Any pointers into header memory returned from
 * headerGetEntryMinMemory() for this entry are invalid after this
 * call has been made!
 *
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRappend) (Header h, int_32 tag, rpmTagType type, const void * p, rpmTagCount c)
        /*@modifies h @*/;

/** \ingroup header
 * Add or append element to tag array in header.
 * @todo Arg "p" should have const.
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRaddorappend) (Header h, int_32 tag, rpmTagType type, const void * p, rpmTagCount c)
        /*@modifies h @*/;

/** \ingroup header
 * Add locale specific tag to header.
 * A NULL lang is interpreted as the C locale. Here are the rules:
 * \verbatim
 *	- If the tag isn't in the header, it's added with the passed string
 *	   as new value.
 *	- If the tag occurs multiple times in entry, which tag is affected
 *	   by the operation is undefined.
 *	- If the tag is in the header w/ this language, the entry is
 *	   *replaced* (like headerModifyEntry()).
 * \endverbatim
 * This function is intended to just "do the right thing". If you need
 * more fine grained control use headerAddEntry() and headerModifyEntry().
 *
 * @param h		header
 * @param tag		tag
 * @param string	tag value
 * @param lang		locale
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRaddi18n) (Header h, int_32 tag, const char * string,
                const char * lang)
        /*@modifies h @*/;

/** \ingroup header
 * Modify tag in header.
 * If there are multiple entries with this tag, the first one gets replaced.
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRmodify) (Header h, int_32 tag, rpmTagType type, const void * p, rpmTagCount c)
        /*@modifies h @*/;

/** \ingroup header
 * Delete tag in header.
 * Removes all entries of type tag from the header, returns 1 if none were
 * found.
 *
 * @param h		header
 * @param tag		tag
 * @return		0 on success, 1 on failure (INCONSISTENT)
 */
typedef
int (*HDRremove) (Header h, int_32 tag)
        /*@modifies h @*/;

/** \ingroup header
 * Return formatted output string from header tags.
 * The returned string must be free()d.
 *
 * @param h		header
 * @param fmt		format to use
 * @param tags		array of tag name/value/type triples (NULL uses default)
 * @param exts		formatting extensions chained table (NULL uses default)
 * @retval errmsg	error message (if any)
 * @return		formatted output string (malloc'ed)
 */
typedef
/*@only@*/ char * (*HDRsprintf) (Header h, const char * fmt,
		/*@null@*/ const struct headerTagTableEntry_s * tags,
		/*@null@*/ const struct headerSprintfExtension_s * exts,
		/*@null@*/ /*@out@*/ errmsg_t * errmsg)
	/*@modifies *errmsg @*/;

/** \ingroup header
 * Duplicate tag values from one header into another.
 * @param headerFrom	source header
 * @param headerTo	destination header
 * @param tagstocopy	array of tags that are copied
 */
typedef
void (*HDRcopytags) (Header headerFrom, Header headerTo, hTAG_t tagstocopy)
	/*@modifies headerFrom, headerTo @*/;

/** \ingroup header
 * Destroy header tag iterator.
 * @param hi		header tag iterator
 * @return		NULL always
 */
typedef
HeaderIterator (*HDRfreeiter) (/*@only@*/ HeaderIterator hi)
	/*@modifies hi @*/;

/** \ingroup header
 * Create header tag iterator.
 * @param h		header
 * @return		header tag iterator
 */
typedef
HeaderIterator (*HDRinititer) (Header h)
	/*@modifies h */;

/** \ingroup header
 * Return next tag from header.
 * @param hi		header tag iterator
 * @retval *tag		tag
 * @retval *type	tag value data type
 * @retval *p		tag value(s)
 * @retval *c		number of values
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRnextiter) (HeaderIterator hi,
		/*@null@*/ /*@out@*/ hTAG_t tag,
		/*@null@*/ /*@out@*/ hTYP_t type,
		/*@null@*/ /*@out@*/ hRET_t * p,
		/*@null@*/ /*@out@*/ hCNT_t c)
	/*@modifies hi, *tag, *type, *p, *c @*/;

/** \ingroup header
 * Return header magic.
 * @param h		header
 * @param *magicp	magic array
 * @param *nmagicp	no. bytes of magic
 * @return		0 always
 */
typedef
int (*HDRgetmagic)(/*@null@*/ Header h, unsigned char **magicp, size_t *nmagicp)
	/*@*/;

/** \ingroup header
 * Store header magic.
 * @param h		header
 * @param magic		magic array
 * @param nmagic	no. bytes of magic
 * @return		0 always
 */
typedef
int (*HDRsetmagic)(/*@null@*/ Header h, unsigned char * magic, size_t nmagic)
	/*@modifies h @*/;

/** \ingroup header
 * Return header origin (e.g path or URL).
 * @param h		header
 * @return		header origin
 */
typedef /*@observer@*/ /*@null@*/
const char * (*HDRgetorigin) (/*@null@*/ Header h)
	/*@*/;

/** \ingroup header
 * Store header origin (e.g path or URL).
 * @param h		header
 * @param origin	new header origin
 * @return		0 always
 */
typedef
int (*HDRsetorigin) (/*@null@*/ Header h, const char * origin)
	/*@modifies h @*/;

/** \ingroup header
 * Return header instance (if from rpmdb).
 * @param h		header
 * @return		header instance
 */
typedef
int (*HDRgetinstance) (/*@null@*/ Header h)
	/*@*/;

/** \ingroup header
 * Store header instance (e.g path or URL).
 * @param h		header
 * @param origin	new header instance
 * @return		0 always
 */
typedef
int (*HDRsetinstance) (/*@null@*/ Header h, int instance)
	/*@modifies h @*/;

/**
 * Return header stats accumulator structure.
 * @param h		header
 * @param opx		per-header accumulator index (aka rpmtsOpX)
 * @return		per-header accumulator pointer
 */
typedef
/*@null@*/ void * (*HDRgetstats) (Header h, int opx)
        /*@*/;

/** \ingroup header
 * Header method vectors.
 */
typedef /*@abstract@*/ struct HV_s * HV_t;
#if !defined(SWIG)
struct HV_s {
    HDRlink	hdrlink;
    HDRunlink	hdrunlink;
    HDRfree	hdrfree;
    HDRnew	hdrnew;
    HDRsort	hdrsort;
    HDRunsort	hdrunsort;
    HDRsizeof	hdrsizeof;
    HDRunload	hdrunload;
    HDRreload	hdrreload;
    HDRcopy	hdrcopy;
    HDRload	hdrload;
    HDRcopyload	hdrcopyload;
    HDRread	hdrread;
    HDRwrite	hdrwrite;
    HDRisentry	hdrisentry;
    HDRfreetag	hdrfreetag;
    HDRext	hdrext;
    HDRget	hdrget;
    HDRgetmin	hdrgetmin;
    HDRadd	hdradd;
    HDRappend	hdrappend;
    HDRaddorappend hdraddorappend;
    HDRaddi18n	hdraddi18n;
    HDRmodify	hdrmodify;
    HDRremove	hdrremove;
    HDRsprintf	hdrsprintf;
    HDRcopytags	hdrcopytags;
    HDRfreeiter	hdrfreeiter;
    HDRinititer	hdrinititer;
    HDRnextiter	hdrnextiter;
    HDRgetmagic hdrgetmagic;
    HDRsetmagic hdrsetmagic;
    HDRgetorigin hdrgetorigin;
    HDRsetorigin hdrsetorigin;
    HDRgetinstance hdrgetinstance;
    HDRsetinstance hdrsetinstance;
    HDRgetstats hdrgetstats;
/*@null@*/
    void *	hdrvecs;
/*@null@*/
    void *	hdrdata;
    int		hdrversion;
};
#endif

#if !defined(SWIG)
/** \ingroup header
 * Free data allocated when retrieved from header.
 * @deprecated Use headerFreeTag() instead.
 * @todo Remove from API.
 *
 * @param data		address of data (or NULL)
 * @param type		type of data (or -1 to force free)
 * @return		NULL always
 */
/*@unused@*/ static inline /*@null@*/
void * headerFreeData( /*@only@*/ /*@null@*/ const void * data, rpmTagType type)
	/*@modifies data @*/
{
    if (data) {
	/*@-branchstate@*/
	if (type == -1 ||
	    type == RPM_STRING_ARRAY_TYPE ||
	    type == RPM_I18NSTRING_TYPE ||
	    type == RPM_BIN_TYPE)
		free((void *)data);
	/*@=branchstate@*/
    }
    return NULL;
}
#endif

#if !defined(__HEADER_PROTOTYPES__)
#include "hdrinline.h"
#endif

/**
 * Define per-header macros.
 * @param h		header
 * @return		0 always
 */
int headerMacrosLoad(Header h)
	/*@globals internalState @*/
	/*@modifies internalState @*/;

/**
 * Define per-header macros.
 * @param h		header
 * @return		0 always
 */
int headerMacrosUnload(Header h)
	/*@globals internalState @*/
	/*@modifies internalState @*/;

/** \ingroup header
 * Return name, epoch, version, release, arch strings from header.
 * @param h		header
 * @retval *np		name pointer (or NULL)
 * @retval *ep		epoch pointer (or NULL)
 * @retval *vp		version pointer (or NULL)
 * @retval *rp		release pointer (or NULL)
 * @retval *ap		arch pointer (or NULL)
 * @return		0 always
 */
int headerNEVRA(Header h,
		/*@null@*/ /*@out@*/ const char ** np,
		/*@null@*/ /*@out@*/ /*@unused@*/ const char ** ep,
		/*@null@*/ /*@out@*/ const char ** vp,
		/*@null@*/ /*@out@*/ const char ** rp,
		/*@null@*/ /*@out@*/ const char ** ap)
	/*@modifies h, *np, *vp, *rp, *ap @*/;

/**
 * Return header color.
 * @param h		header
 * @return		header color
 */
uint_32 hGetColor(Header h)
	/*@modifies h @*/;

/** \ingroup header
 * Translate and merge legacy signature tags into header.
 * @todo Remove headerSort() through headerInitIterator() modifies sig.
 * @param h		header
 * @param sigh		signature header
 */
void headerMergeLegacySigs(Header h, const Header sigh)
	/*@modifies h, sigh @*/;

/** \ingroup header
 * Regenerate signature header.
 * @todo Remove headerSort() through headerInitIterator() modifies h.
 * @param h		header
 * @param noArchiveSize	don't copy archive size tag (pre rpm-4.1)
 * @return		regenerated signature header
 */
Header headerRegenSigHeader(const Header h, int noArchiveSize)
	/*@modifies h @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_HEADER */

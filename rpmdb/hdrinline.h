/*@-type@*/ /* FIX: cast to HV_t bogus */
#ifndef H_HDRINLINE
#define H_HDRINLINE

/** \ingroup header
 * \file rpmdb/hdrinline.h
 */

#ifdef __cplusplus
extern "C" {
#endif
/*@+voidabstract -nullpass -mustmod -compdef -shadow -predboolothers @*/

/** \ingroup header
 * Header methods for rpm headers.
 */
/*@observer@*/ /*@unchecked@*/
extern struct HV_s * hdrVec;

/** \ingroup header
 */
/*@unused@*/ static inline HV_t h2hv(Header h)
	/*@*/
{
    /*@-abstract -castexpose -refcounttrans@*/
    return ((HV_t)h);
    /*@=abstract =castexpose =refcounttrans@*/
}

/** \ingroup header
 * Create new (empty) header instance.
 * @return		header
 */
/*@unused@*/ static inline
Header headerNew(void)
	/*@*/
{
    return hdrVec->hdrnew();
}

/** \ingroup header
 * Dereference a header instance.
 * @param h		header
 * @return		NULL always
 */
/*@unused@*/ static inline
/*@null@*/ Header headerFree( /*@killref@*/ /*@null@*/ Header h)
	/*@modifies h @*/
{
    /*@-abstract@*/
    if (h == NULL) return NULL;
    /*@=abstract@*/
    return (h2hv(h)->hdrfree) (h);
}

/** \ingroup header
 * Reference a header instance.
 * @param h		header
 * @return		new header reference
 */
/*@unused@*/ static inline
Header headerLink(Header h)
	/*@modifies h @*/
{
    return (h2hv(h)->hdrlink) (h);
}

/** \ingroup header
 * Dereference a header instance.
 * @param h		header
 * @return		new header reference
 */
/*@unused@*/ static inline
Header headerUnlink(/*@killref@*/ /*@null@*/ Header h)
	/*@modifies h @*/
{
    /*@-abstract@*/
    if (h == NULL) return NULL;
    /*@=abstract@*/
    return (h2hv(h)->hdrunlink) (h);
}

/*@-exportlocal@*/
/** \ingroup header
 * Sort tags in header.
 * @param h		header
 */
/*@unused@*/ static inline
void headerSort(Header h)
	/*@modifies h @*/
{
/*@-noeffectuncon@*/ /* FIX: add rc */
    (h2hv(h)->hdrsort) (h);
/*@=noeffectuncon@*/
    return;
}

/** \ingroup header
 * Restore tags in header to original ordering.
 * @param h		header
 */
/*@unused@*/ static inline
void headerUnsort(Header h)
	/*@modifies h @*/
{
/*@-noeffectuncon@*/ /* FIX: add rc */
    (h2hv(h)->hdrunsort) (h);
/*@=noeffectuncon@*/
    return;
}
/*@=exportlocal@*/

/** \ingroup header
 * Return size of on-disk header representation in bytes.
 * @param h		header
 * @return		size of on-disk header
 */
/*@unused@*/ static inline
size_t headerSizeof(/*@null@*/ Header h)
	/*@modifies h @*/
{
    /*@-abstract@*/
    if (h == NULL) return 0;
    /*@=abstract@*/
    return (h2hv(h)->hdrsizeof) (h);
}

/** \ingroup header
 * Convert header to on-disk representation.
 * @param h		header (with pointers)
 * @retval *lenp	length of header in bytes (or NULL);
 * @return		on-disk header blob (i.e. with offsets)
 */
/*@unused@*/ static inline
/*@only@*/ /*@null@*/
void * headerUnload(Header h, /*@out@*/ /*@null@*/ size_t * lenp)
	/*@modifies h @*/
{
    return (h2hv(h)->hdrunload) (h, lenp);
}

/** \ingroup header
 * Convert header to on-disk representation, and then reload.
 * This is used to insure that all header data is in one chunk.
 * @param h		header (with pointers)
 * @param tag		region tag
 * @return		on-disk header (with offsets)
 */
/*@unused@*/ static inline
/*@null@*/ Header headerReload(/*@only@*/ Header h, int tag)
	/*@modifies h @*/
{
    /*@-onlytrans@*/
    return (h2hv(h)->hdrreload) (h, tag);
    /*@=onlytrans@*/
}

/** \ingroup header
 * Duplicate a header.
 * @param h		header
 * @return		new header instance
 */
/*@unused@*/ static inline
/*@null@*/ Header headerCopy(Header h)
	/*@modifies h @*/
{
    return (h2hv(h)->hdrcopy) (h);
}

/** \ingroup header
 * Convert header to in-memory representation.
 * @param uh		on-disk header blob (i.e. with offsets)
 * @return		header
 */
/*@unused@*/ static inline
/*@null@*/ Header headerLoad(/*@kept@*/ void * uh)
	/*@modifies uh @*/
{
    return hdrVec->hdrload(uh);
}

/** \ingroup header
 * Make a copy and convert header to in-memory representation.
 * @param uh		on-disk header blob (i.e. with offsets)
 * @return		header
 */
/*@unused@*/ static inline
/*@null@*/ Header headerCopyLoad(const void * uh)
	/*@*/
{
    return hdrVec->hdrcopyload(uh);
}

/** \ingroup header
 * Check if tag is in header.
 * @param h		header
 * @param tag		tag
 * @return		1 on success, 0 on failure
 */
/*@unused@*/ static inline
int headerIsEntry(/*@null@*/ Header h, uint32_t tag)
	/*@modifies h @*/
{
    /*@-abstract@*/
    if (h == NULL) return 0;
    /*@=abstract@*/
    return (h2hv(h)->hdrisentry) (h, tag);
}

/** \ingroup header
 * Free data allocated when retrieved from header.
 * @param h		header
 * @param data		pointer to tag value(s)
 * @param type		type of data (or -1 to force free)
 * @return		NULL always
 */
/*@unused@*/ static inline
/*@null@*/ void * headerFreeTag(Header h,
		/*@only@*/ /*@null@*/ const void * data, rpmTagType type)
	/*@modifies data @*/
{
    if (h == NULL) return 0;
    return (h2hv(h)->hdrfreetag) (h, data, type);
}

void tagTypeValidate(HE_t he)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;

/*
 * Retrieve extension or tag value.
 *
 * @param h		header
 * @param he		tag container
 * @param flags		(unused)
 * @return		1 on success, 0 on failure
 */
/*@unused@*/ static inline
int headerGetExtension(Header h, HE_t he, /*@unused@*/ unsigned int flags)
	/*@modifies *he @*/
{
    int xx;
    if (h == NULL) return 0;
    xx = (h2hv(h)->hdrext) (h, he->tag, &he->t, &he->p, &he->c);
#if defined(SUPPORT_IMPLICIT_TAG_DATA_TYPES)
/*@-modfilesys@*/
    /* XXX verify that explicit and implicit types are identical. */
    if (xx) tagTypeValidate(he);
/*@=modfilesys@*/
#endif
    return xx;
}

/** \ingroup header
 * Add or append tag container to header.
 *
 * @param h		header
 * @param he		tag container
 * @param flags		(unused)
 * @return		1 on success, 0 on failure
 */
/*@mayexit@*/
/*@unused@*/ static inline
int headerAddExtension(Header h, HE_t he, /*@unused@*/ unsigned int flags)
	/*@modifies h @*/
{
    int xx;
#if defined(SUPPORT_IMPLICIT_TAG_DATA_TYPES)
/*@-modfilesys@*/
    /* XXX verify that explicit and implicit types are identical. */
    tagTypeValidate(he);
/*@=modfilesys@*/
#endif
    if (he->append)
	xx = (h2hv(h)->hdraddorappend) (h, he->tag, he->t, he->p.ptr, he->c);
    else
	xx = (h2hv(h)->hdradd) (h, he->tag, he->t, he->p.ptr, he->c);
    return xx;
}

/** \ingroup header
 * Modify tag container in header.
 * If there are multiple entries with this tag, the first one gets replaced.
 * @param h		header
 * @param he		tag container
 * @param flags		(unused)
 * @return		1 on success, 0 on failure
 */
/*@unused@*/ static inline
int headerModifyExtension(Header h, HE_t he, /*@unused@*/ unsigned int flags)
	/*@modifies h @*/
{
    return (h2hv(h)->hdrmodify) (h, he->tag, he->t, he->p.ptr, he->c);
}

/** \ingroup header
 * Remove tag container from header.
 *
 * @param h		header
 * @param he		tag container
 * @param flags		(unused)
 * @return		1 on success, 0 on failure
 */
/*@mayexit@*/
/*@unused@*/ static inline
int headerRemoveExtension(Header h, HE_t he, /*@unused@*/ unsigned int flags)
	/*@modifies h @*/
{
    return (h2hv(h)->hdrremove) (h, he->tag);
}

/** \ingroup header
 * Destroy header tag container iterator.
 * @param hi		header tag container iterator
 * @return		NULL always
 */
/*@unused@*/ static inline
HeaderIterator headerFreeExtension(/*@only@*/ HeaderIterator hi)
	/*@modifies hi @*/
{
    return hdrVec->hdrfreeiter(hi);
}

/** \ingroup header
 * Create header tag iterator.
 * @param h		header
 * @return		header tag iterator
 */
/*@unused@*/ static inline
HeaderIterator headerInitExtension(Header h)
	/*@modifies h */
{
    return hdrVec->hdrinititer(h);
}

/** \ingroup header
 * Return next tag from header.
 * @param hi		header tag iterator
 * @param he		tag container
 * @param flags		(unused)
 * @return		1 on success, 0 on failure
 */
/*@unused@*/ static inline
int headerNextExtension(HeaderIterator hi, HE_t he, /*@unused@*/ unsigned int flags)
	/*@modifies hi, he @*/
{
    return hdrVec->hdrnextiter(hi, &he->tag, &he->t, &he->p, &he->c);
}

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
/*@unused@*/ static inline
int headerGetEntry(Header h, uint32_t tag,
			/*@null@*/ /*@out@*/ rpmTagType * type,
			/*@null@*/ /*@out@*/ rpmTagData * p,
			/*@null@*/ /*@out@*/ rpmTagCount * c)
	/*@modifies *type, *p, *c @*/
{
    if (h == NULL) return 0;
    return (h2hv(h)->hdrget) (h, tag, type, p, c);
}

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
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
/*@mayexit@*/
/*@unused@*/ static inline
int headerAddEntry(Header h, uint32_t tag, rpmTagType type,
		const void * p, rpmTagCount c)
	/*@modifies h @*/
{
    return (h2hv(h)->hdradd) (h, tag, type, p, c);
}

/** \ingroup header
 * Append element to tag array in header.
 * Appends item p to entry w/ tag and type as passed. Won't work on
 * RPM_STRING_TYPE.
 *
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
/*@unused@*/ static inline
int headerAppendEntry(Header h, uint32_t tag, rpmTagType type,
		const void * p, rpmTagCount c)
	/*@modifies h @*/
{
    return (h2hv(h)->hdrappend) (h, tag, type, p, c);
}

/** \ingroup header
 * Add or append element to tag array in header.
 * @todo Arg "p" should have const.
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
/*@unused@*/ static inline
int headerAddOrAppendEntry(Header h, uint32_t tag, rpmTagType type,
		const void * p, rpmTagCount c)
	/*@modifies h @*/
{
    return (h2hv(h)->hdraddorappend) (h, tag, type, p, c);
}

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
/*@unused@*/ static inline
int headerAddI18NString(Header h, uint32_t tag, const char * string,
		const char * lang)
	/*@modifies h @*/
{
    return (h2hv(h)->hdraddi18n) (h, tag, string, lang);
}

/** \ingroup header
 * Modify tag in header.
 * If there are multiple entries with this tag, the first one gets replaced.
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
/*@unused@*/ static inline
int headerModifyEntry(Header h, uint32_t tag, rpmTagType type,
			const void * p, rpmTagCount c)
	/*@modifies h @*/
{
    return (h2hv(h)->hdrmodify) (h, tag, type, p, c);
}

/** \ingroup header
 * Delete tag in header.
 * Removes all entries of type tag from the header, returns 1 if none were
 * found.
 *
 * @param h		header
 * @param tag		tag
 * @return		0 on success, 1 on failure (INCONSISTENT)
 */
/*@unused@*/ static inline
int headerRemoveEntry(Header h, uint32_t tag)
	/*@modifies h @*/
{
    return (h2hv(h)->hdrremove) (h, tag);
}

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
/*@unused@*/ static inline
/*@only@*/ char * headerSprintf(Header h, const char * fmt,
		/*@null@*/ const struct headerTagTableEntry_s * tags,
		/*@null@*/ const struct headerSprintfExtension_s * exts,
		/*@null@*/ /*@out@*/ errmsg_t * errmsg)
	/*@modifies *errmsg @*/
{
    return (h2hv(h)->hdrsprintf) (h, fmt, tags, exts, errmsg);
}

/** \ingroup header
 * Duplicate tag values from one header into another.
 * @param headerFrom	source header
 * @param headerTo	destination header
 * @param tagstocopy	array of tags that are copied
 */
/*@unused@*/ static inline
void headerCopyTags(Header headerFrom, Header headerTo, hTAG_t tagstocopy)
	/*@modifies headerFrom, headerTo @*/
{
/*@-noeffectuncon@*/ /* FIX: add rc */
    hdrVec->hdrcopytags(headerFrom, headerTo, tagstocopy);
/*@=noeffectuncon@*/
    return;
}

/** \ingroup header
 * Destroy header tag iterator.
 * @param hi		header tag iterator
 * @return		NULL always
 */
/*@unused@*/ static inline
HeaderIterator headerFreeIterator(/*@only@*/ HeaderIterator hi)
	/*@modifies hi @*/
{
    return hdrVec->hdrfreeiter(hi);
}

/** \ingroup header
 * Create header tag iterator.
 * @param h		header
 * @return		header tag iterator
 */
/*@unused@*/ static inline
HeaderIterator headerInitIterator(Header h)
	/*@modifies h */
{
    return hdrVec->hdrinititer(h);
}

/** \ingroup header
 * Return next tag from header.
 * @param hi		header tag iterator
 * @retval *tag		tag
 * @retval *type	tag value data type
 * @retval *p		pointer to tag value(s)
 * @retval *c		number of values
 * @return		1 on success, 0 on failure
 */
/*@unused@*/ static inline
int headerNextIterator(HeaderIterator hi,
		/*@null@*/ /*@out@*/ hTAG_t tag,
		/*@null@*/ /*@out@*/ rpmTagType * type,
		/*@null@*/ /*@out@*/ rpmTagData * p,
		/*@null@*/ /*@out@*/ rpmTagCount * c)
	/*@modifies hi, *tag, *type, *p, *c @*/
{
    return hdrVec->hdrnextiter(hi, tag, type, p, c);
}

/** \ingroup header
 * Return header magic.
 * @param h		header
 * @param *magicp	magic array
 * @param *nmagicp	no. bytes of magic
 * @return		0 always
 */
/*@unused@*/ static inline
int headerGetMagic(/*@null@*/ Header h, unsigned char **magicp, size_t *nmagicp)
	/*@modifies *magicp, *nmagicp @*/
{
    return hdrVec->hdrgetmagic(h, magicp, nmagicp);
}

/** \ingroup header
 * Store header magic.
 * @param h		header
 * @param magic		magic array
 * @param nmagic	no. bytes of magic
 * @return		0 always
 */
/*@unused@*/ static inline
int headerSetMagic(/*@null@*/ Header h, unsigned char * magic, size_t nmagic)
	/*@modifies h @*/
{
    return hdrVec->hdrsetmagic(h, magic, nmagic);
}

/** \ingroup header
 * Return header origin (e.g path or URL).
 * @param h		header
 * @return		header origin
 */
/*@unused@*/ static inline
/*@observer@*/ /*@null@*/ const char * headerGetOrigin(/*@null@*/ Header h)
	/*@*/
{
    return hdrVec->hdrgetorigin(h);
}

/** \ingroup header
 * Store header origin (e.g path or URL).
 * @param h		header
 * @param origin	new header origin
 * @return		0 always
 */
/*@unused@*/ static inline
int headerSetOrigin(/*@null@*/ Header h, const char * origin)
	/*@modifies h @*/
{
    return hdrVec->hdrsetorigin(h, origin);
}

/** \ingroup header
 * Return header instance (if from rpmdb).
 * @param h		header
 * @return		header instance
 */
/*@unused@*/ static inline
uint32_t headerGetInstance(/*@null@*/ Header h)
	/*@*/
{
    return hdrVec->hdrgetinstance(h);
}

/** \ingroup header
 * Store header instance (e.g path or URL).
 * @param h		header
 * @param instance	new header instance
 * @return		0 always
 */
/*@unused@*/ static inline
uint32_t headerSetInstance(/*@null@*/ Header h, uint32_t instance)
	/*@modifies h @*/
{
    return hdrVec->hdrsetinstance(h, instance);
}

/*@=voidabstract =nullpass =mustmod =compdef =shadow =predboolothers @*/

#ifdef __cplusplus
}
#endif

#endif	/* H_HDRINLINE */
/*@=type@*/

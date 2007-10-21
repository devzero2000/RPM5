/** \ingroup header
 * \file rpmdb/header.c
 */

/* Tags are looked up with linear, not binary, search. */
/* XXX work in progress, caveat emptor. */
#define SUPPORT_LINEAR_TAGTABLE_LOOKUP	1

#define	SUPPORT_SPRINTF_TAGTABLE	1

#if 1	/* XXX undefining loses the extensions in lib/formats.c. */
#define	SUPPORT_SPRINTF_EXTTABLE	1
#endif

static int _jbj = 0;    /* XXX private debugging */

/* RPM - Copyright (C) 1995-2002 Red Hat Software */

/* Data written to file descriptors is in network byte order.    */
/* Data read from file descriptors is expected to be in          */
/* network byte order and is converted on the fly to host order. */

#include "system.h"

#define	__HEADER_PROTOTYPES__

#include <rpmio_internal.h>	/* XXX for fdGetOPath() */
#include <header_internal.h>

#include "debug.h"

/*@unchecked@*/
int _hdr_debug = 0;

/*@unchecked@*/
int _newmagic = 0;

/*@unchecked@*/
int _tagcache = 1;

/*@access entryInfo @*/
/*@access indexEntry @*/

/*@access sprintfTag @*/
/*@access sprintfToken @*/
/*@access HV_t @*/
/*@access FD_t @*/		/* XXX void * arg headerRead/headerWrite */

#define PARSER_BEGIN 	0
#define PARSER_IN_ARRAY 1
#define PARSER_IN_EXPR  2

/** \ingroup header
 */
/*@observer@*/ /*@unchecked@*/
static unsigned char header_magic[8] = {
	0x8e, 0xad, 0xe8, 0x01, 0x00, 0x00, 0x00, 0x00
};

/*@observer@*/ /*@unchecked@*/
static unsigned char sigh_magic[8] = {
	0x8e, 0xad, 0xe8, 0x3e, 0x00, 0x00, 0x00, 0x00
};

/*@observer@*/ /*@unchecked@*/
static unsigned char meta_magic[8] = {
	0x8e, 0xad, 0xe8, 0x3f, 0x00, 0x00, 0x00, 0x00
};

/** \ingroup header
 * Size of header data types.
 */
/*@observer@*/ /*@unchecked@*/
static int typeSizes[16] =  { 
    0,	/*!< RPM_NULL_TYPE */
    1,	/*!< RPM_CHAR_TYPE */
    1,	/*!< RPM_INT8_TYPE */
    2,	/*!< RPM_INT16_TYPE */
    4,	/*!< RPM_INT32_TYPE */
    8,	/*!< RPM_INT64_TYPE */
    -1,	/*!< RPM_STRING_TYPE */
    1,	/*!< RPM_BIN_TYPE */
    -1,	/*!< RPM_STRING_ARRAY_TYPE */
    -1,	/*!< RPM_I18NSTRING_TYPE */
    1,  /*!< RPM_ASN1_TYPE */
    1,  /*!< RPM_OPENPGP_TYPE */
    0,
    0,
    0,
    0
};

/** \ingroup header
 * Maximum no. of bytes permitted in a header.
 */
/*@unchecked@*/
static size_t headerMaxbytes = (32*1024*1024);

/*@observer@*/ /*@unchecked@*/
HV_t hdrVec;	/* forward reference */

/**
 * Global header stats enabler.
 */
/*@unchecked@*/
int _hdr_stats = 0;

/*@-compmempass@*/
/*@unchecked@*/
static struct rpmop_s hdr_loadops;
/*@unchecked@*/ /*@relnull@*/
rpmop _hdr_loadops = &hdr_loadops;
/*@unchecked@*/
static struct rpmop_s hdr_getops;
/*@unchecked@*/ /*@relnull@*/
rpmop _hdr_getops = &hdr_getops;
/*@=compmempass@*/

/** \ingroup header
 * Return header stats accumulator structure.
 * @param h		header
 * @param opx		per-header accumulator index (aka rpmtsOpX)
 * @return		per-header accumulator pointer
 */
static /*@null@*/
void * headerGetStats(Header h, int opx)
        /*@*/
{
    rpmop op = NULL;
    if (_hdr_stats)
    switch (opx) {
    case 18:	op = &h->h_loadops;	break;	/* RPMTS_OP_HDRLOAD */
    case 19:	op = &h->h_getops;	break;	/* RPMTS_OP_HDRGET */
    }
    return op;
}

/** \ingroup header
 * Reference a header instance.
 * @param h		header
 * @return		referenced header instance
 */
static
Header headerLink(Header h)
	/*@modifies h @*/
{
/*@-nullret@*/
    if (h == NULL) return NULL;
/*@=nullret@*/

    h->nrefs++;
/*@-modfilesys@*/
if (_hdr_debug)
fprintf(stderr, "--> h  %p ++ %d at %s:%u\n", h, h->nrefs, __FILE__, __LINE__);
/*@=modfilesys@*/

    /*@-refcounttrans @*/
    return h;
    /*@=refcounttrans @*/
}

/** \ingroup header
 * Dereference a header instance.
 * @param h		header
 * @return		NULL always
 */
static /*@null@*/
Header headerUnlink(/*@killref@*/ /*@null@*/ Header h)
	/*@modifies h @*/
{
    if (h == NULL) return NULL;
/*@-modfilesys@*/
if (_hdr_debug)
fprintf(stderr, "--> h  %p -- %d at %s:%u\n", h, h->nrefs, __FILE__, __LINE__);
/*@=modfilesys@*/
    h->nrefs--;
    return NULL;
}

/** \ingroup header
 * Dereference a header instance.
 * @param h		header
 * @return		NULL always
 */
static /*@null@*/
Header headerFree(/*@killref@*/ /*@null@*/ Header h)
	/*@modifies h @*/
{
    (void) headerUnlink(h);

    /*@-usereleased@*/
    if (h == NULL || h->nrefs > 0)
	return NULL;	/* XXX return previous header? */

    if (h->index) {
	indexEntry entry = h->index;
	int i;
	for (i = 0; i < h->indexUsed; i++, entry++) {
	    if ((h->flags & HEADERFLAG_ALLOCATED) && ENTRY_IS_REGION(entry)) {
		if (entry->length > 0) {
		    int_32 * ei = entry->data;
		    if ((ei - 2) == h->blob) h->blob = _free(h->blob);
		    entry->data = NULL;
		}
	    } else if (!ENTRY_IN_REGION(entry)) {
		entry->data = _free(entry->data);
	    }
	    entry->data = NULL;
	}
	h->index = _free(h->index);
    }
    h->origin = _free(h->origin);

/*@-nullstate@*/
    if (_hdr_stats) {
	if (_hdr_loadops)	/* RPMTS_OP_HDRLOAD */
	    (void) rpmswAdd(_hdr_loadops, headerGetStats(h, 18));
	if (_hdr_getops)	/* RPMTS_OP_HDRGET */
	    (void) rpmswAdd(_hdr_getops, headerGetStats(h, 19));
    }
/*@=nullstate@*/

    /*@-refcounttrans@*/ h = _free(h); /*@=refcounttrans@*/
    return h;
    /*@=usereleased@*/
}

/** \ingroup header
 * Create new (empty) header instance.
 * @return		header
 */
static
Header headerNew(void)
	/*@*/
{
    Header h = xcalloc(1, sizeof(*h));

    (void) memcpy(h->magic, header_magic, sizeof(h->magic));
    /*@-assignexpose@*/
    h->hv = *hdrVec;		/* structure assignment */
    /*@=assignexpose@*/
    h->blob = NULL;
    h->origin = NULL;
    h->instance = 0;
    h->indexAlloced = INDEX_MALLOC_SIZE;
    h->indexUsed = 0;
    h->flags |= HEADERFLAG_SORTED;

    h->index = (h->indexAlloced
	? xcalloc(h->indexAlloced, sizeof(*h->index))
	: NULL);

    h->nrefs = 0;
    /*@-globstate -observertrans @*/
    return headerLink(h);
    /*@=globstate =observertrans @*/
}

/**
 */
static int indexCmp(const void * avp, const void * bvp)
	/*@*/
{
    /*@-castexpose@*/
    indexEntry ap = (indexEntry) avp, bp = (indexEntry) bvp;
    /*@=castexpose@*/
    return (ap->info.tag - bp->info.tag);
}

/** \ingroup header
 * Sort tags in header.
 * @param h		header
 */
static
void headerSort(Header h)
	/*@modifies h @*/
{
    if (!(h->flags & HEADERFLAG_SORTED)) {
	qsort(h->index, h->indexUsed, sizeof(*h->index), indexCmp);
	h->flags |= HEADERFLAG_SORTED;
    }
}

/**
 */
static int offsetCmp(const void * avp, const void * bvp) /*@*/
{
    /*@-castexpose@*/
    indexEntry ap = (indexEntry) avp, bp = (indexEntry) bvp;
    /*@=castexpose@*/
    int rc = (ap->info.offset - bp->info.offset);

    if (rc == 0) {
	/* Within a region, entries sort by address. Added drips sort by tag. */
	if (ap->info.offset < 0)
	    rc = (((char *)ap->data) - ((char *)bp->data));
	else
	    rc = (ap->info.tag - bp->info.tag);
    }
    return rc;
}

/** \ingroup header
 * Restore tags in header to original ordering.
 * @param h		header
 */
static
void headerUnsort(Header h)
	/*@modifies h @*/
{
    qsort(h->index, h->indexUsed, sizeof(*h->index), offsetCmp);
}

/** \ingroup header
 * Return size of on-disk header representation in bytes.
 * @param h		header
 * @return		size of on-disk header
 */
static
unsigned int headerSizeof(/*@null@*/ Header h)
	/*@modifies h @*/
{
    indexEntry entry;
    unsigned int size = 0;
    unsigned int pad = 0;
    int i;

    if (h == NULL)
	return size;

    headerSort(h);

    size += sizeof(header_magic);	/* XXX HEADER_MAGIC_YES */

    /*@-sizeoftype@*/
    size += 2 * sizeof(int_32);	/* count of index entries */
    /*@=sizeoftype@*/

    for (i = 0, entry = h->index; i < h->indexUsed; i++, entry++) {
	unsigned diff;
	rpmTagType type;

	/* Regions go in as is ... */
        if (ENTRY_IS_REGION(entry)) {
	    size += entry->length;
	    /* XXX Legacy regions do not include the region tag and data. */
	    /*@-sizeoftype@*/
	    if (i == 0 && (h->flags & HEADERFLAG_LEGACY))
		size += sizeof(struct entryInfo_s) + entry->info.count;
	    /*@=sizeoftype@*/
	    continue;
        }

	/* ... and region elements are skipped. */
	if (entry->info.offset < 0)
	    continue;

	/* Alignment */
	type = entry->info.type;
	if (typeSizes[type] > 1) {
	    diff = typeSizes[type] - (size % typeSizes[type]);
	    if (diff != typeSizes[type]) {
		size += diff;
		pad += diff;
	    }
	}

	/*@-sizeoftype@*/
	size += sizeof(struct entryInfo_s) + entry->length;
	/*@=sizeoftype@*/
    }

    return size;
}

/**
 * Return length of entry data.
 * @param type		entry data type
 * @param p		entry data
 * @param count		entry item count
 * @param onDisk	data is concatenated strings (with NUL's))?
 * @param pend		pointer to end of data (or NULL)
 * @return		no. bytes in data, -1 on failure
 */
static int dataLength(rpmTagType type, const void * p, rpmTagCount count, int onDisk,
		/*@null@*/ const void * pend)
	/*@*/
{
    const unsigned char * s = p;
    const unsigned char * se = pend;
    int length = 0;

    switch (type) {
    case RPM_STRING_TYPE:
	if (count != 1)
	    return -1;
	while (*s++) {
	    if (se && s > se)
		return -1;
	    length++;
	}
	length++;	/* count nul terminator too. */
	break;

    case RPM_STRING_ARRAY_TYPE:
    case RPM_I18NSTRING_TYPE:
	/* These are like RPM_STRING_TYPE, except they're *always* an array */
	/* Compute sum of length of all strings, including nul terminators */

	if (onDisk) {
	    while (count--) {
		length++;       /* count nul terminator too */
               while (*s++) {
		    if (se && s > se)
			return -1;
		    length++;
		}
	    }
	} else {
	    const char ** av = (const char **) p;
	    while (count--) {
		/* add one for null termination */
		length += strlen(*av++) + 1;
	    }
	}
	break;

    default:
	if (typeSizes[type] == -1)
	    return -1;
	length = typeSizes[(type & 0xf)] * count;
	if (length < 0 || (se && (s + length) > se))
	    return -1;
	break;
    }

    return length;
}

/** \ingroup header
 * Swap int_32 and int_16 arrays within header region.
 *
 * This code is way more twisty than I would like.
 *
 * A bug with RPM_I18NSTRING_TYPE in rpm-2.5.x (fixed in August 1998)
 * causes the offset and length of elements in a header region to disagree
 * regarding the total length of the region data.
 *
 * The "fix" is to compute the size using both offset and length and
 * return the larger of the two numbers as the size of the region.
 * Kinda like computing left and right Riemann sums of the data elements
 * to determine the size of a data structure, go figger :-).
 *
 * There's one other twist if a header region tag is in the set to be swabbed,
 * as the data for a header region is located after all other tag data.
 *
 * @param entry		header entry
 * @param il		no. of entries
 * @param dl		start no. bytes of data
 * @param pe		header physical entry pointer (swapped)
 * @param dataStart	header data start
 * @param dataEnd	header data end
 * @param regionid	region offset
 * @return		no. bytes of data in region, -1 on error
 */
/*@-globs@*/	/* XXX rpm_typeAlign usage */
static int regionSwab(/*@null@*/ indexEntry entry, int il, int dl,
		entryInfo pe,
		unsigned char * dataStart,
		/*@null@*/ const unsigned char * dataEnd,
		int regionid)
	/*@modifies *entry, *dataStart @*/
{
    const void * p, * pend;
    unsigned char * tprev = NULL;
    unsigned char * t = NULL;
    int tdel = 0;
    int tl = dl;
    struct indexEntry_s ieprev;

    memset(&ieprev, 0, sizeof(ieprev));
    for (; il > 0; il--, pe++) {
	struct indexEntry_s ie;
	rpmTagType type;

	ie.info.tag = ntohl(pe->tag);
	ie.info.type = ntohl(pe->type);
	ie.info.count = ntohl(pe->count);
	ie.info.offset = ntohl(pe->offset);

	if (hdrchkType(ie.info.type))
	    return -1;
	if (hdrchkData(ie.info.count))
	    return -1;
	if (hdrchkData(ie.info.offset))
	    return -1;
	if (hdrchkAlign(ie.info.type, ie.info.offset))
	    return -1;

	ie.data = t = dataStart + ie.info.offset;
	if (dataEnd && t >= dataEnd)
	    return -1;

	p = ie.data;
	pend = dataEnd;
	ie.length = dataLength(ie.info.type, p, ie.info.count, 1, pend);
	if (ie.length < 0 || hdrchkData(ie.length))
	    return -1;

	ie.rdlen = 0;

	if (entry) {
	    ie.info.offset = regionid;
	    *entry = ie;	/* structure assignment */
	    entry++;
	}

	/* Alignment */
	type = ie.info.type;
	if (typeSizes[type] > 1) {
	    unsigned diff;
	    diff = typeSizes[type] - (dl % typeSizes[type]);
	    if (diff != typeSizes[type]) {
		dl += diff;
		if (ieprev.info.type == RPM_I18NSTRING_TYPE)
		    ieprev.length += diff;
	    }
	}
	tdel = (tprev ? (t - tprev) : 0);
	if (ieprev.info.type == RPM_I18NSTRING_TYPE)
	    tdel = ieprev.length;

	if (ie.info.tag >= HEADER_I18NTABLE) {
	    tprev = t;
	} else {
	    tprev = dataStart;
	    /* XXX HEADER_IMAGE tags don't include region sub-tag. */
	    /*@-sizeoftype@*/
	    if (ie.info.tag == HEADER_IMAGE)
		tprev -= REGION_TAG_COUNT;
	    /*@=sizeoftype@*/
	}

	/* Perform endian conversions */
	switch (ntohl(pe->type)) {
	case RPM_INT64_TYPE:
	{   int_64 * it = (int_64 *)t;
	    int_32 b[2];
	    for (; ie.info.count > 0; ie.info.count--, it += 1) {
		if (dataEnd && ((unsigned char *)it) >= dataEnd)
		    return -1;
		b[1] = htonl(((int_32 *)it)[0]);
		b[0] = htonl(((int_32 *)it)[1]);
		if (b[1] != ((int_32 *)it)[0])
		    memcpy(it, b, sizeof(b));
	    }
	    t = (unsigned char *) it;
	}   /*@switchbreak@*/ break;
	case RPM_INT32_TYPE:
	{   int_32 * it = (int_32 *)t;
	    for (; ie.info.count > 0; ie.info.count--, it += 1) {
		if (dataEnd && ((unsigned char *)it) >= dataEnd)
		    return -1;
		*it = htonl(*it);
	    }
	    t = (unsigned char *) it;
	}   /*@switchbreak@*/ break;
	case RPM_INT16_TYPE:
	{   int_16 * it = (int_16 *) t;
	    for (; ie.info.count > 0; ie.info.count--, it += 1) {
		if (dataEnd && ((unsigned char *)it) >= dataEnd)
		    return -1;
		*it = htons(*it);
	    }
	    t = (unsigned char *) it;
	}   /*@switchbreak@*/ break;
	default:
	    t += ie.length;
	    /*@switchbreak@*/ break;
	}

	dl += ie.length;
	if (dataEnd && dataStart + dl > dataEnd) return -1;
	tl += tdel;
	ieprev = ie;	/* structure assignment */

    }
    tdel = (tprev ? (t - tprev) : 0);
    tl += tdel;

    /* XXX
     * There are two hacks here:
     *	1) tl is 16b (i.e. REGION_TAG_COUNT) short while doing headerReload().
     *	2) the 8/98 rpm bug with inserting i18n tags needs to use tl, not dl.
     */
    /*@-sizeoftype@*/
    if (tl+REGION_TAG_COUNT == dl)
	tl += REGION_TAG_COUNT;
    /*@=sizeoftype@*/

    return dl;
}
/*@=globs@*/

/** \ingroup header
 * @param h		header
 * @retval *lenp	no. bytes in unloaded header blob
 * @return		unloaded header blob (NULL on error)
 */
static /*@only@*/ /*@null@*/
void * headerUnload(Header h, /*@out@*/ /*@null@*/ size_t * lenp)
	/*@modifies h, *lenp @*/
	/*@requires maxSet(lenp) >= 0 @*/
	/*@ensures maxRead(result) == (*lenp) @*/
{
    void * sw;
    int_32 * ei = NULL;
    entryInfo pe;
    char * dataStart;
    char * te;
    unsigned pad;
    unsigned len;
    int_32 il = 0;
    int_32 dl = 0;
    indexEntry entry; 
    rpmTagType type;
    int i;
    int drlen, ndribbles;
    int driplen, ndrips;
    int legacy = 0;

    if ((sw = headerGetStats(h, 18)) != NULL)	/* RPMTS_OP_HDRLOAD */
	(void) rpmswEnter(sw, 0);

    /* Sort entries by (offset,tag). */
    headerUnsort(h);

    /* Compute (il,dl) for all tags, including those deleted in region. */
    pad = 0;
    drlen = ndribbles = driplen = ndrips = 0;
    for (i = 0, entry = h->index; i < h->indexUsed; i++, entry++) {
	if (ENTRY_IS_REGION(entry)) {
	    int_32 rdl = -entry->info.offset;	/* negative offset */
	    int_32 ril = rdl/sizeof(*pe);
	    int rid = entry->info.offset;

	    il += ril;
	    dl += entry->rdlen + entry->info.count;
	    /* XXX Legacy regions do not include the region tag and data. */
	    if (i == 0 && (h->flags & HEADERFLAG_LEGACY))
		il += 1;

	    /* Skip rest of entries in region, but account for dribbles. */
	    for (; i < h->indexUsed && entry->info.offset <= rid+1; i++, entry++) {
		if (entry->info.offset <= rid)
		    /*@innercontinue@*/ continue;

		/* Alignment */
		type = entry->info.type;
		if (typeSizes[type] > 1) {
		    unsigned diff;
		    diff = typeSizes[type] - (dl % typeSizes[type]);
		    if (diff != typeSizes[type]) {
			drlen += diff;
			pad += diff;
			dl += diff;
		    }
		}

		ndribbles++;
		il++;
		drlen += entry->length;
		dl += entry->length;
	    }
	    i--;
	    entry--;
	    continue;
	}

	/* Ignore deleted drips. */
	if (entry->data == NULL || entry->length <= 0)
	    continue;

	/* Alignment */
	type = entry->info.type;
	if (typeSizes[type] > 1) {
	    unsigned diff;
	    diff = typeSizes[type] - (dl % typeSizes[type]);
	    if (diff != typeSizes[type]) {
		driplen += diff;
		pad += diff;
		dl += diff;
	    } else
		diff = 0;
	}

	ndrips++;
	il++;
	driplen += entry->length;
	dl += entry->length;
    }

    /* Sanity checks on header intro. */
    if (hdrchkTags(il) || hdrchkData(dl))
	goto errxit;

    len = sizeof(il) + sizeof(dl) + (il * sizeof(*pe)) + dl;

    ei = xmalloc(len);
    ei[0] = htonl(il);
    ei[1] = htonl(dl);

    pe = (entryInfo) &ei[2];
    dataStart = te = (char *) (pe + il);

    pad = 0;
    for (i = 0, entry = h->index; i < h->indexUsed; i++, entry++) {
	const char * src;
	unsigned char *t;
	int count;
	int rdlen;

	if (entry->data == NULL || entry->length <= 0)
	    continue;

	t = (unsigned char *)te;
	pe->tag = htonl(entry->info.tag);
	pe->type = htonl(entry->info.type);
	pe->count = htonl(entry->info.count);

	if (ENTRY_IS_REGION(entry)) {
	    int_32 rdl = -entry->info.offset;	/* negative offset */
	    int_32 ril = rdl/sizeof(*pe) + ndribbles;
	    int rid = entry->info.offset;

	    src = (char *)entry->data;
	    rdlen = entry->rdlen;

	    /* XXX Legacy regions do not include the region tag and data. */
	    if (i == 0 && (h->flags & HEADERFLAG_LEGACY)) {
		int_32 stei[4];

		legacy = 1;
		memcpy(pe+1, src, rdl);
		memcpy(te, src + rdl, rdlen);
		te += rdlen;

		pe->offset = htonl(te - dataStart);
		stei[0] = pe->tag;
		stei[1] = pe->type;
		stei[2] = htonl(-rdl-entry->info.count);
		stei[3] = pe->count;
		memcpy(te, stei, entry->info.count);
		te += entry->info.count;
		ril++;
		rdlen += entry->info.count;

		count = regionSwab(NULL, ril, 0, pe, t, NULL, 0);
		if (count != rdlen)
		    goto errxit;

	    } else {

		memcpy(pe+1, src + sizeof(*pe), ((ril-1) * sizeof(*pe)));
		memcpy(te, src + (ril * sizeof(*pe)), rdlen+entry->info.count+drlen);
		te += rdlen;
		{   /*@-castexpose@*/
		    entryInfo se = (entryInfo)src;
		    /*@=castexpose@*/
		    int off = ntohl(se->offset);
		    pe->offset = (off) ? htonl(te - dataStart) : htonl(off);
		}
		te += entry->info.count + drlen;

		count = regionSwab(NULL, ril, 0, pe, t, NULL, 0);
		if (count != (rdlen + entry->info.count + drlen))
		    goto errxit;
	    }

	    /* Skip rest of entries in region. */
	    while (i < h->indexUsed && entry->info.offset <= rid+1) {
		i++;
		entry++;
	    }
	    i--;
	    entry--;
	    pe += ril;
	    continue;
	}

	/* Ignore deleted drips. */
	if (entry->data == NULL || entry->length <= 0)
	    continue;

	/* Alignment */
	type = entry->info.type;
	if (typeSizes[type] > 1) {
	    unsigned diff;
	    diff = typeSizes[type] - ((te - dataStart) % typeSizes[type]);
	    if (diff != typeSizes[type]) {
		memset(te, 0, diff);
		te += diff;
		pad += diff;
	    }
	}

	pe->offset = htonl(te - dataStart);

	/* copy data w/ endian conversions */
	switch (entry->info.type) {
	case RPM_INT64_TYPE:
	{   int_32 b[2];
	    count = entry->info.count;
	    src = entry->data;
	    while (count--) {
		b[1] = htonl(((int_32 *)src)[0]);
		b[0] = htonl(((int_32 *)src)[1]);
		if (b[1] == ((int_32 *)src)[0])
		    memcpy(te, src, sizeof(b));
		else
		    memcpy(te, b, sizeof(b));
		te += sizeof(b);
		src += sizeof(b);
	    }
	}   /*@switchbreak@*/ break;

	case RPM_INT32_TYPE:
	    count = entry->info.count;
	    src = entry->data;
	    while (count--) {
		*((int_32 *)te) = htonl(*((int_32 *)src));
		/*@-sizeoftype@*/
		te += sizeof(int_32);
		src += sizeof(int_32);
		/*@=sizeoftype@*/
	    }
	    /*@switchbreak@*/ break;

	case RPM_INT16_TYPE:
	    count = entry->info.count;
	    src = entry->data;
	    while (count--) {
		*((int_16 *)te) = htons(*((int_16 *)src));
		/*@-sizeoftype@*/
		te += sizeof(int_16);
		src += sizeof(int_16);
		/*@=sizeoftype@*/
	    }
	    /*@switchbreak@*/ break;

	default:
	    memcpy(te, entry->data, entry->length);
	    te += entry->length;
	    /*@switchbreak@*/ break;
	}
	pe++;
    }
   
    /* Insure that there are no memcpy underruns/overruns. */
    if (((char *)pe) != dataStart)
	goto errxit;
    if ((((char *)ei)+len) != te)
	goto errxit;

    if (lenp)
	*lenp = len;

    h->flags &= ~HEADERFLAG_SORTED;
    headerSort(h);

    if (sw != NULL)	(void) rpmswExit(sw, len);

    return (void *) ei;

errxit:
    if (sw != NULL)	(void) rpmswExit(sw, len);
    /*@-usereleased@*/
    ei = _free(ei);
    /*@=usereleased@*/
    return (void *) ei;
}

/**
 * Find matching (tag,type) entry in header.
 * @param h		header
 * @param tag		entry tag
 * @param type		entry type
 * @return 		header entry
 */
static /*@null@*/
indexEntry findEntry(/*@null@*/ Header h, int_32 tag, rpmTagType type)
	/*@modifies h @*/
{
    indexEntry entry, entry2, last;
    struct indexEntry_s key;

    if (h == NULL) return NULL;
    if (!(h->flags & HEADERFLAG_SORTED)) headerSort(h);

    key.info.tag = tag;

    entry2 = entry = 
	bsearch(&key, h->index, h->indexUsed, sizeof(*h->index), indexCmp);
    if (entry == NULL)
	return NULL;

    if (type == RPM_NULL_TYPE)
	return entry;

    /* look backwards */
    while (entry->info.tag == tag && entry->info.type != type &&
	   entry > h->index) entry--;

    if (entry->info.tag == tag && entry->info.type == type)
	return entry;

    last = h->index + h->indexUsed;
    /*@-usereleased@*/ /* FIX: entry2 = entry. Code looks bogus as well. */
    while (entry2->info.tag == tag && entry2->info.type != type &&
	   entry2 < last) entry2++;
    /*@=usereleased@*/

    if (entry->info.tag == tag && entry->info.type == type)
	return entry;

    return NULL;
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
static
int headerRemoveEntry(Header h, int_32 tag)
	/*@modifies h @*/
{
    indexEntry last = h->index + h->indexUsed;
    indexEntry entry, first;
    int ne;

    entry = findEntry(h, tag, RPM_NULL_TYPE);
    if (!entry) return 1;

    /* Make sure entry points to the first occurence of this tag. */
    while (entry > h->index && (entry - 1)->info.tag == tag)  
	entry--;

    /* Free data for tags being removed. */
    for (first = entry; first < last; first++) {
	void * data;
	if (first->info.tag != tag)
	    break;
	data = first->data;
	first->data = NULL;
	first->length = 0;
	if (ENTRY_IN_REGION(first))
	    continue;
	data = _free(data);
    }

    ne = (first - entry);
    if (ne > 0) {
	h->indexUsed -= ne;
	ne = last - first;
	if (ne > 0)
	    memmove(entry, first, (ne * sizeof(*entry)));
    }

    return 0;
}

/** \ingroup header
 * Convert header to in-memory representation.
 * @param uh		on-disk header blob (i.e. with offsets)
 * @return		header
 */
static /*@null@*/
Header headerLoad(/*@kept@*/ void * uh)
	/*@modifies uh @*/
{
    void * sw;
    int_32 * ei = (int_32 *) uh;
    int_32 il = ntohl(ei[0]);		/* index length */
    int_32 dl = ntohl(ei[1]);		/* data length */
    /*@-sizeoftype@*/
    size_t pvlen = sizeof(il) + sizeof(dl) +
               (il * sizeof(struct entryInfo_s)) + dl;
    /*@=sizeoftype@*/
    void * pv = uh;
    Header h = NULL;
    entryInfo pe;
    unsigned char * dataStart;
    unsigned char * dataEnd;
    indexEntry entry; 
    int rdlen;
    int i;

    /* Sanity checks on header intro. */
    if (hdrchkTags(il) || hdrchkData(dl))
	goto errxit;

    ei = (int_32 *) pv;
    /*@-castexpose@*/
    pe = (entryInfo) &ei[2];
    /*@=castexpose@*/
    dataStart = (unsigned char *) (pe + il);
    dataEnd = dataStart + dl;

    h = xcalloc(1, sizeof(*h));
    if ((sw = headerGetStats(h, 18)) != NULL)	/* RPMTS_OP_HDRLOAD */
	(void) rpmswEnter(sw, 0);
    {	unsigned char * hmagic = (_newmagic ? meta_magic : header_magic);
	(void) memcpy(h->magic, hmagic, sizeof(h->magic));
    }
    /*@-assignexpose@*/
    h->hv = *hdrVec;		/* structure assignment */
    /*@=assignexpose@*/
    /*@-assignexpose -kepttrans@*/
    h->blob = uh;
    /*@=assignexpose =kepttrans@*/
    h->indexAlloced = il + 1;
    h->indexUsed = il;
    h->index = xcalloc(h->indexAlloced, sizeof(*h->index));
    h->flags |= HEADERFLAG_SORTED;
    h->nrefs = 0;
    h = headerLink(h);

    entry = h->index;
    i = 0;
    if (!(htonl(pe->tag) < HEADER_I18NTABLE)) {
	h->flags |= HEADERFLAG_LEGACY;
	entry->info.type = REGION_TAG_TYPE;
	entry->info.tag = HEADER_IMAGE;
	/*@-sizeoftype@*/
	entry->info.count = REGION_TAG_COUNT;
	/*@=sizeoftype@*/
	entry->info.offset = ((unsigned char *)pe - dataStart); /* negative offset */

	/*@-assignexpose@*/
	entry->data = pe;
	/*@=assignexpose@*/
	entry->length = pvlen - sizeof(il) - sizeof(dl);
	rdlen = regionSwab(entry+1, il, 0, pe, dataStart, dataEnd, entry->info.offset);
#if 0	/* XXX don't check, the 8/98 i18n bug fails here. */
	if (rdlen != dl)
	    goto errxit;
#endif
	entry->rdlen = rdlen;
	entry++;
	h->indexUsed++;
    } else {
	int_32 rdl;
	int_32 ril;

	h->flags &= ~HEADERFLAG_LEGACY;

	entry->info.type = htonl(pe->type);
	entry->info.count = htonl(pe->count);

	if (hdrchkType(entry->info.type))
	    goto errxit;
	if (hdrchkTags(entry->info.count))
	    goto errxit;

	{   int off = ntohl(pe->offset);

	    if (hdrchkData(off))
		goto errxit;
	    if (off) {
/*@-sizeoftype@*/
		size_t nb = REGION_TAG_COUNT;
/*@=sizeoftype@*/
		int_32 * stei = memcpy(alloca(nb), dataStart + off, nb);
		rdl = -ntohl(stei[2]);	/* negative offset */
		ril = rdl/sizeof(*pe);
		if (hdrchkTags(ril) || hdrchkData(rdl))
		    goto errxit;
		entry->info.tag = htonl(pe->tag);
	    } else {
		ril = il;
		/*@-sizeoftype@*/
		rdl = (ril * sizeof(struct entryInfo_s));
		/*@=sizeoftype@*/
		entry->info.tag = HEADER_IMAGE;
	    }
	}
	entry->info.offset = -rdl;	/* negative offset */

	/*@-assignexpose@*/
	entry->data = pe;
	/*@=assignexpose@*/
	entry->length = pvlen - sizeof(il) - sizeof(dl);
	rdlen = regionSwab(entry+1, ril-1, 0, pe+1, dataStart, dataEnd, entry->info.offset);
	if (rdlen < 0)
	    goto errxit;
	entry->rdlen = rdlen;

	if (ril < h->indexUsed) {
	    indexEntry newEntry = entry + ril;
	    int ne = (h->indexUsed - ril);
	    int rid = entry->info.offset+1;
	    int rc;

	    /* Load dribble entries from region. */
	    rc = regionSwab(newEntry, ne, 0, pe+ril, dataStart, dataEnd, rid);
	    if (rc < 0)
		goto errxit;
	    rdlen += rc;

	  { indexEntry firstEntry = newEntry;
	    int save = h->indexUsed;
	    int j;

	    /* Dribble entries replace duplicate region entries. */
	    h->indexUsed -= ne;
	    for (j = 0; j < ne; j++, newEntry++) {
		(void) headerRemoveEntry(h, newEntry->info.tag);
		if (newEntry->info.tag == HEADER_BASENAMES)
		    (void) headerRemoveEntry(h, HEADER_OLDFILENAMES);
	    }

	    /* If any duplicate entries were replaced, move new entries down. */
	    if (h->indexUsed < (save - ne)) {
		memmove(h->index + h->indexUsed, firstEntry,
			(ne * sizeof(*entry)));
	    }
	    h->indexUsed += ne;
	  }
	}
    }

    h->flags &= ~HEADERFLAG_SORTED;
    headerSort(h);

    if (sw != NULL)
	(void) rpmswExit(sw, pvlen);

    /*@-globstate -observertrans @*/
    return h;
    /*@=globstate =observertrans @*/

errxit:
    /*@-usereleased@*/
    if (h) {
	h->index = _free(h->index);
	/*@-refcounttrans@*/
	h = _free(h);
	/*@=refcounttrans@*/
    }
    /*@=usereleased@*/
    /*@-refcounttrans -globstate@*/
    return h;
    /*@=refcounttrans =globstate@*/
}

/** \ingroup header
 * Return header magic.
 * @param h		header
 * @param *magicp	magic array
 * @param *nmagicp	no. bytes of magic
 * @return		0 always
 */
static
int headerGetMagic(/*@null@*/ Header h, unsigned char **magicp, size_t *nmagicp)
	/*@modifies *magicp, *nmagicp @*/
{
    unsigned char * hmagic = (_newmagic ? meta_magic : header_magic);
    if (magicp)
	*magicp = (h ? h->magic : hmagic);
    if (nmagicp)
	*nmagicp = (h ? sizeof(h->magic) : sizeof(header_magic));
    return 0;
}

/** \ingroup header
 * Store header magic.
 * @param h		header
 * @param magic		magic array
 * @param nmagic	no. bytes of magic
 * @return		0 always
 */
static
int headerSetMagic(/*@null@*/ Header h, unsigned char * magic, size_t nmagic)
	/*@modifies h @*/
{
    if (nmagic > sizeof(h->magic))
	nmagic = sizeof(h->magic);
    if (h) {
	memset(h->magic, 0, sizeof(h->magic));
	if (nmagic > 0)
	    memmove(h->magic, magic, nmagic);
    }
    return 0;
}

/** \ingroup header
 * Return header origin (e.g path or URL).
 * @param h		header
 * @return		header origin
 */
static /*@observer@*/ /*@null@*/
const char * headerGetOrigin(/*@null@*/ Header h)
	/*@*/
{
    return (h != NULL ? h->origin : NULL);
}

/** \ingroup header
 * Store header origin (e.g path or URL).
 * @param h		header
 * @param origin	new header origin
 * @return		0 always
 */
static
int headerSetOrigin(/*@null@*/ Header h, const char * origin)
	/*@modifies h @*/
{
    if (h != NULL) {
	h->origin = _free(h->origin);
	h->origin = xstrdup(origin);
    }
    return 0;
}

/** \ingroup header
 * Return header instance (if from rpmdb).
 * @param h		header
 * @return		header instance
 */
static
int headerGetInstance(/*@null@*/ Header h)
	/*@*/
{
    return (h != NULL ? h->instance : 0);
}

/** \ingroup header
 * Store header instance (e.g path or URL).
 * @param h		header
 * @param origin	new header instance
 * @return		0 always
 */
static
int headerSetInstance(/*@null@*/ Header h, int instance)
	/*@modifies h @*/
{
    if (h != NULL)
	h->instance = instance;
    return 0;
}

/** \ingroup header
 * Convert header to on-disk representation, and then reload.
 * This is used to insure that all header data is in one chunk.
 * @param h		header (with pointers)
 * @param tag		region tag
 * @return		on-disk header (with offsets)
 */
static /*@null@*/
Header headerReload(/*@only@*/ Header h, int tag)
	/*@modifies h @*/
{
    Header nh;
    /*@-onlytrans@*/
    void * uh = headerUnload(h, NULL);
    const char * origin;
    int_32 instance = h->instance;
    int xx;

    origin = (h->origin != NULL ? xstrdup(h->origin) : NULL);
    h = headerFree(h);
    /*@=onlytrans@*/
    if (uh == NULL)
	return NULL;
    nh = headerLoad(uh);
    if (nh == NULL) {
	uh = _free(uh);
	return NULL;
    }
    if (nh->flags & HEADERFLAG_ALLOCATED)
	uh = _free(uh);
    nh->flags |= HEADERFLAG_ALLOCATED;
    if (ENTRY_IS_REGION(nh->index)) {
	if (tag == HEADER_SIGNATURES || tag == HEADER_IMMUTABLE)
	    nh->index[0].info.tag = tag;
    }
    if (origin != NULL) {
	xx = headerSetOrigin(nh, origin);
	origin = _free(origin);
    }
    xx = headerSetInstance(nh, instance);
    return nh;
}

/** \ingroup header
 * Make a copy and convert header to in-memory representation.
 * @param uh		on-disk header blob (i.e. with offsets)
 * @return		header
 */
static /*@null@*/
Header headerCopyLoad(const void * uh)
	/*@*/
{
    int_32 * ei = (int_32 *) uh;
    int_32 il = ntohl(ei[0]);		/* index length */
    int_32 dl = ntohl(ei[1]);		/* data length */
    /*@-sizeoftype@*/
    size_t pvlen = sizeof(il) + sizeof(dl) +
			(il * sizeof(struct entryInfo_s)) + dl;
    /*@=sizeoftype@*/
    void * nuh = NULL;
    Header h = NULL;

    /* Sanity checks on header intro. */
    if (!(hdrchkTags(il) || hdrchkData(dl)) && pvlen < headerMaxbytes) {
	nuh = memcpy(xmalloc(pvlen), uh, pvlen);
	if ((h = headerLoad(nuh)) != NULL)
	    h->flags |= HEADERFLAG_ALLOCATED;
    }
    if (h == NULL)
	nuh = _free(nuh);
    return h;
}

/** \ingroup header
 * Read (and load) header from file handle.
 * @param _fd		file handle
 * @return		header (or NULL on error)
 */
static /*@null@*/
Header headerRead(void * _fd)
	/*@modifies _fd @*/
{
    FD_t fd = _fd;
    int_32 block[4];
    int_32 reserved;
    int_32 * ei = NULL;
    int_32 il;
    int_32 dl;
    int_32 magic;
    Header h = NULL;
    size_t len;
    int i;

    memset(block, 0, sizeof(block));
    i = 2;
    i += 2;	/* XXX HEADER_MAGIC_YES */

    /*@-type@*/ /* FIX: cast? */
    if (timedRead(fd, (char *)block, i*sizeof(*block)) != (i * sizeof(*block)))
	goto exit;
    /*@=type@*/

    i = 0;

    {	/* XXX HEADER_MAGIC_YES */
	magic = block[i++];
	if (!(	!memcmp(&magic, header_magic, sizeof(magic))
	 ||	!memcmp(&magic, sigh_magic, sizeof(magic))
	 ||	!memcmp(&magic, meta_magic, sizeof(magic))
	))
	    goto exit;
	reserved = block[i++];
    }
    
    il = ntohl(block[i]);	i++;
    dl = ntohl(block[i]);	i++;

    /*@-sizeoftype@*/
    len = sizeof(il) + sizeof(dl) + (il * sizeof(struct entryInfo_s)) + dl;
    /*@=sizeoftype@*/

    /* Sanity checks on header intro. */
    if (hdrchkTags(il) || hdrchkData(dl) || len > headerMaxbytes)
	goto exit;

    ei = xmalloc(len);
    ei[0] = htonl(il);
    ei[1] = htonl(dl);
    len -= sizeof(il) + sizeof(dl);

    /*@-type@*/ /* FIX: cast? */
    if (timedRead(fd, (char *)&ei[2], len) != len)
	goto exit;
    /*@=type@*/
    
    h = headerLoad(ei);

    {   const char * origin = fdGetOPath(fd);
	if (origin != NULL)
            (void) headerSetOrigin(h, origin);
    }

exit:
    if (h) {
	if (h->flags & HEADERFLAG_ALLOCATED)
	    ei = _free(ei);
	h->flags |= HEADERFLAG_ALLOCATED;
    } else if (ei)
	ei = _free(ei);
    /*@-mustmod@*/	/* FIX: timedRead macro obscures annotation */
    return h;
    /*@-mustmod@*/
}

/** \ingroup header
 * Write (with unload) header to file handle.
 * @param _fd		file handle
 * @param h		header
 * @return		0 on success, 1 on error
 */
static
int headerWrite(void * _fd, /*@null@*/ Header h)
	/*@globals fileSystem @*/
	/*@modifies _fd, h, fileSystem @*/
{
    FD_t fd = _fd;
    ssize_t nb;
    size_t length;
    const void * uh;

    if (h == NULL)
	return 1;
    uh = headerUnload(h, &length);
    if (uh == NULL)
	return 1;
    {	/* XXX HEADER_MAGIC_YES */
	unsigned char mymagic[sizeof(header_magic)];

	/* XXX create new magic from region marker. */
	memcpy(mymagic, header_magic, sizeof(header_magic));
	if (_newmagic && length > 8+3)
	    mymagic[3] = ((unsigned char *)uh)[8+3];
	/*@-sizeoftype@*/
	nb = Fwrite(mymagic, sizeof(char), sizeof(mymagic), fd);
	/*@=sizeoftype@*/
	if (nb != sizeof(header_magic))
	    goto exit;
    }

    /*@-sizeoftype@*/
    nb = Fwrite(uh, sizeof(char), length, fd);
    /*@=sizeoftype@*/

exit:
    uh = _free(uh);
    return (nb == length ? 0 : 1);
}

/** \ingroup header
 * Check if tag is in header.
 * @param h		header
 * @param tag		tag
 * @return		1 on success, 0 on failure
 */
static
int headerIsEntry(/*@null@*/Header h, int_32 tag)
	/*@*/
{
    /*@-mods@*/		/*@ FIX: h modified by sort. */
    return (findEntry(h, tag, RPM_NULL_TYPE) ? 1 : 0);
    /*@=mods@*/	
}

/** \ingroup header
 * Retrieve data from header entry.
 * @todo Permit retrieval of regions other than HEADER_IMUTABLE.
 * @param entry		header entry
 * @retval *type	type (or NULL)
 * @retval *p		data (or NULL)
 * @retval *c		count (or NULL)
 * @param minMem	string pointers refer to header memory?
 * @return		1 on success, otherwise error.
 */
static int copyEntry(const indexEntry entry,
		/*@null@*/ /*@out@*/ hTYP_t type,
		/*@null@*/ /*@out@*/ hRET_t * p,
		/*@null@*/ /*@out@*/ hCNT_t c,
		int minMem)
	/*@modifies *type, *p, *c @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(p) >= 0 /\ maxSet(c) >= 0 @*/
{
    rpmTagCount count = entry->info.count;
    int rc = 1;		/* XXX 1 on success. */

    if (p)
    switch (entry->info.type) {
    case RPM_BIN_TYPE:
	/*
	 * XXX This only works for
	 * XXX 	"sealed" HEADER_IMMUTABLE/HEADER_SIGNATURES/HEADER_IMAGE.
	 * XXX This will *not* work for unsealed legacy HEADER_IMAGE (i.e.
	 * XXX a legacy header freshly read, but not yet unloaded to the rpmdb).
	 */
	if (ENTRY_IS_REGION(entry)) {
	    int_32 * ei = ((int_32 *)entry->data) - 2;
	    /*@-castexpose@*/
	    entryInfo pe = (entryInfo) (ei + 2);
	    /*@=castexpose@*/
	    unsigned char * dataStart = (unsigned char *) (pe + ntohl(ei[0]));
	    int_32 rdl = -entry->info.offset;	/* negative offset */
	    int_32 ril = rdl/sizeof(*pe);

	    /*@-sizeoftype@*/
	    rdl = entry->rdlen;
	    count = 2 * sizeof(*ei) + (ril * sizeof(*pe)) + rdl;
	    if (entry->info.tag == HEADER_IMAGE) {
		ril -= 1;
		pe += 1;
	    } else {
		count += REGION_TAG_COUNT;
		rdl += REGION_TAG_COUNT;
	    }

	    (*p).i32p = ei = xmalloc(count);
	    ei[0] = htonl(ril);
	    ei[1] = htonl(rdl);

	    /*@-castexpose@*/
	    pe = (entryInfo) memcpy(ei + 2, pe, (ril * sizeof(*pe)));
	    /*@=castexpose@*/

	    dataStart = (unsigned char *) memcpy(pe + ril, dataStart, rdl);
	    /*@=sizeoftype@*/

	    rc = regionSwab(NULL, ril, 0, pe, dataStart, NULL, 0);
	    /* XXX 1 on success. */
	    rc = (rc < 0) ? 0 : 1;
	} else {
	    count = entry->length;
	    (*p).ptr = (!minMem
		? memcpy(xmalloc(count), entry->data, count)
		: entry->data);
	}
	break;
    case RPM_STRING_TYPE:
	if (count == 1) {
	    (*p).str = (char *) entry->data;	/* NOCAST */
	    break;
	}
	/*@fallthrough@*/
    case RPM_STRING_ARRAY_TYPE:
    case RPM_I18NSTRING_TYPE:
    {	const char ** ptrEntry;
	/*@-sizeoftype@*/
	int tableSize = count * sizeof(char *);
	/*@=sizeoftype@*/
	char * t;
	int i;

	/*@-mods@*/
	if (minMem) {
	    (*p).argv = ptrEntry = xmalloc(tableSize);
	    t = entry->data;
	} else {
	    (*p).argv = ptrEntry = xmalloc(tableSize + entry->length);
	    t = (char *) &ptrEntry[count];
	    memcpy(t, entry->data, entry->length);
	}
	/*@=mods@*/
	for (i = 0; i < count; i++) {
	    *ptrEntry++ = t;
	    t = strchr(t, 0);
	    t++;
	}
    }	break;

    case RPM_OPENPGP_TYPE:	/* XXX W2DO? */
    case RPM_ASN1_TYPE:		/* XXX W2DO? */
    default:
	(*p).ptr = entry->data;
	break;
    }
    if (type) *type = entry->info.type;
    if (c) *c = count;
    return rc;
}

/**
 * Does locale match entry in header i18n table?
 * 
 * \verbatim
 * The range [l,le) contains the next locale to match:
 *    ll[_CC][.EEEEE][@dddd]
 * where
 *    ll	ISO language code (in lowercase).
 *    CC	(optional) ISO coutnry code (in uppercase).
 *    EEEEE	(optional) encoding (not really standardized).
 *    dddd	(optional) dialect.
 * \endverbatim
 *
 * @param td		header i18n table data, NUL terminated
 * @param l		start of locale	to match
 * @param le		end of locale to match
 * @return		1 on match, 0 on no match
 */
static int headerMatchLocale(const char *td, const char *l, const char *le)
	/*@*/
{
    const char *fe;


#if 0
  { const char *s, *ll, *CC, *EE, *dd;
    char *lbuf, *t.

    /* Copy the buffer and parse out components on the fly. */
    lbuf = alloca(le - l + 1);
    for (s = l, ll = t = lbuf; *s; s++, t++) {
	switch (*s) {
	case '_':
	    *t = '\0';
	    CC = t + 1;
	    break;
	case '.':
	    *t = '\0';
	    EE = t + 1;
	    break;
	case '@':
	    *t = '\0';
	    dd = t + 1;
	    break;
	default:
	    *t = *s;
	    break;
	}
    }

    if (ll)	/* ISO language should be lower case */
	for (t = ll; *t; t++)	*t = tolower(*t);
    if (CC)	/* ISO country code should be upper case */
	for (t = CC; *t; t++)	*t = toupper(*t);

    /* There are a total of 16 cases to attempt to match. */
  }
#endif

    /* First try a complete match. */
    if (strlen(td) == (le-l) && !strncmp(td, l, (le - l)))
	return 1;

    /* Next, try stripping optional dialect and matching.  */
    for (fe = l; fe < le && *fe != '@'; fe++)
	{};
    if (fe < le && !strncmp(td, l, (fe - l)))
	return 1;

    /* Next, try stripping optional codeset and matching.  */
    for (fe = l; fe < le && *fe != '.'; fe++)
	{};
    if (fe < le && !strncmp(td, l, (fe - l)))
	return 1;

    /* Finally, try stripping optional country code and matching. */
    for (fe = l; fe < le && *fe != '_'; fe++)
	{};
    if (fe < le && !strncmp(td, l, (fe - l)))
	return 1;

    return 0;
}

/**
 * Return i18n string from header that matches locale.
 * @param h		header
 * @param entry		i18n string data
 * @return		matching i18n string (or 1st string if no match)
 */
/*@dependent@*/ /*@exposed@*/ static char *
headerFindI18NString(Header h, indexEntry entry)
	/*@*/
{
    const char *lang, *l, *le;
    indexEntry table;

    /* XXX Drepper sez' this is the order. */
    if ((lang = getenv("LANGUAGE")) == NULL &&
	(lang = getenv("LC_ALL")) == NULL &&
	(lang = getenv("LC_MESSAGES")) == NULL &&
	(lang = getenv("LANG")) == NULL)
	    return entry->data;
    
    /*@-mods@*/
    if ((table = findEntry(h, HEADER_I18NTABLE, RPM_STRING_ARRAY_TYPE)) == NULL)
	return entry->data;
    /*@=mods@*/

    for (l = lang; *l != '\0'; l = le) {
	const char *td;
	char *ed;
	int langNum;

	while (*l && *l == ':')			/* skip leading colons */
	    l++;
	if (*l == '\0')
	    break;
	for (le = l; *le && *le != ':'; le++)	/* find end of this locale */
	    {};

	/* For each entry in the header ... */
	for (langNum = 0, td = table->data, ed = entry->data;
	     langNum < entry->info.count;
	     langNum++, td += strlen(td) + 1, ed += strlen(ed) + 1) {

		if (headerMatchLocale(td, l, le))
		    return ed;

	}
    }

    return entry->data;
}

/**
 * Retrieve tag data from header.
 * @param h		header
 * @param tag		tag to retrieve
 * @retval *type	type (or NULL)
 * @retval *p		data (or NULL)
 * @retval *c		count (or NULL)
 * @param minMem	string pointers reference header memory?
 * @return		1 on success, 0 on not found
 */
static int intGetEntry(Header h, int_32 tag,
		/*@null@*/ /*@out@*/ hTYP_t type,
		/*@null@*/ /*@out@*/ hRET_t * p,
		/*@null@*/ /*@out@*/ hCNT_t c,
		int minMem)
	/*@modifies *type, *p, *c @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(p) >= 0 /\ maxSet(c) >= 0 @*/
{
    indexEntry entry;
    int rc;

    /* First find the tag */
    /*@-mods@*/		/*@ FIX: h modified by sort. */
    entry = findEntry(h, tag, RPM_NULL_TYPE);
    /*@mods@*/
    if (entry == NULL) {
	if (type) type = 0;
	if (p) (*p).ptr = NULL;
	if (c) *c = 0;
	return 0;
    }

    switch (entry->info.type) {
    case RPM_I18NSTRING_TYPE:
	rc = 1;
	if (type) *type = RPM_STRING_TYPE;
	if (c) *c = 1;
	/*@-dependenttrans@*/
	if (p) (*p).ptr = headerFindI18NString(h, entry);
	/*@=dependenttrans@*/
	break;
    default:
	rc = copyEntry(entry, type, (*p).ptr, c, minMem);
	break;
    }

    /* XXX 1 on success */
    return ((rc == 1) ? 1 : 0);
}

/** \ingroup header
 * Free data allocated when retrieved from header.
 * @param h		header
 * @param data		data (or NULL)
 * @param type		type of data (or -1 to force free)
 * @return		NULL always
 */
static /*@null@*/ void * headerFreeTag(/*@unused@*/ Header h,
		/*@only@*/ /*@null@*/ const void * data, rpmTagType type)
	/*@modifies data @*/
{
    if (data) {
	if (type == -1 ||
	    type == RPM_STRING_ARRAY_TYPE ||
	    type == RPM_I18NSTRING_TYPE ||
	    type == RPM_BIN_TYPE ||
	    type == RPM_ASN1_TYPE ||
	    type == RPM_OPENPGP_TYPE)
		data = _free(data);
    }
    return NULL;
}

/**
 * Automatically generated table of tag name/value pairs.
 * @todo This should come from #include <rpmtag.h>.
 */
/*@-redecl@*/
/*@observer@*/ /*@unchecked@*/
extern const struct headerTagTableEntry_s * rpmTagTable;
/*@=redecl@*/

/**
 * Number of entries in rpmTagTable.
 * @todo This should come from #include <rpmtag.h>.
 */
/*@-redecl@*/
/*@unchecked@*/
extern const int rpmTagTableSize;

/**
 * @todo This should come from #include <rpmtag.h>.
 */
/*@-redecl@*/
/*@unchecked@*/
extern headerTagIndices rpmTags;
/*@=redecl@*/

/**
 * Return tag name from value.
 * @todo This should come from #include <rpmtag.h>.
 * @param tag		tag value
 * @return		tag name, "(unknown)" on not found
 */
/*@-redecl@*/
/*@unused@*/ static inline /*@observer@*/
const char * tagName(int tag)
	/*@*/
{
/*@-type@*/
    return ((*rpmTags->tagName)(tag));
/*@=type@*/
}

/**
 * Return tag data type from value.
 * @todo This should come from #include <rpmtag.h>.
 * @param tag		tag value
 * @return		tag data type, RPM_NULL_TYPE on not found.
 */
/*@unused@*/ static inline
int tagType(int tag)
	/*@*/
{
/*@-type@*/
    return ((*rpmTags->tagType)(tag));
/*@=type@*/
}

/**
 * Return tag value from name.
 * @todo This should come from #include <rpmtag.h>.
 * @param tagstr	name of tag
 * @return		tag value, -1 on not found
 */
/*@unused@*/ static inline
int tagValue(const char * tagstr)
	/*@*/
{
/*@-type@*/
    return ((*rpmTags->tagValue)(tagstr));
/*@=type@*/
}
/*@=redecl@*/

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
static
int headerGetExtension(Header h, int_32 tag,
			/*@null@*/ /*@out@*/ hTYP_t type,
			/*@null@*/ /*@out@*/ hRET_t * p,
			/*@null@*/ /*@out@*/ hCNT_t c)
	/*@globals headerCompoundFormats @*/
	/*@modifies *type, *p, *c @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(p) >= 0 /\ maxSet(c) >= 0 @*/
{
    void * sw;
    const char * name = tagName(tag);
    headerSprintfExtension exts = (headerSprintfExtension)headerCompoundFormats;
    headerSprintfExtension ext;
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    size_t nb = 0;
    int extNum;
    int rc;

    if ((sw = headerGetStats(h, 19)) != NULL)	/* RPMTS_OP_HDRGET */
	(void) rpmswEnter(sw, 0);

    he->tag = tag;

    /* Search extensions for specific tag override. */
    for (ext = exts, extNum = 0; ext != NULL && ext->type != HEADER_EXT_LAST;
	ext = (ext->type == HEADER_EXT_MORE ? ext->u.more : ext+1), extNum++)
    {
	if (ext->name == NULL || ext->type != HEADER_EXT_TAG)
	    continue;
	if (!xstrcasecmp(ext->name + (sizeof("RPMTAG_")-1), name))
	    break;
    }

    if (ext && ext->name != NULL && ext->type == HEADER_EXT_TAG)
	rc = ext->u.tagFunction(h, he);
    else
	rc = intGetEntry(h, he->tag, &he->t, &he->p, &he->c, 0);

    if (!rc)
	goto exit;

    switch (he->t) {
    case RPM_NULL_TYPE:
    default:
assert(0);	/* XXX stop unimplemented oversights. */
	break;
    case RPM_OPENPGP_TYPE:
    case RPM_ASN1_TYPE:
    case RPM_BIN_TYPE:
	he->freeData = 1;	/* XXX RPM_BIN_TYPE is malloc'd */
	/*@fallthrough@*/
    case RPM_CHAR_TYPE:
    case RPM_INT8_TYPE:
	nb = he->c * sizeof(*he->p.i8p);
	break;
    case RPM_INT16_TYPE:
	nb = he->c * sizeof(*he->p.i16p);
	break;
    case RPM_INT32_TYPE:
	nb = he->c * sizeof(*he->p.i32p);
	break;
    case RPM_INT64_TYPE:
	nb = he->c * sizeof(*he->p.i64p);
	break;
    case RPM_I18NSTRING_TYPE:
    case RPM_STRING_TYPE:
	if (he->p.str)
	    nb = strlen(he->p.str) + 1;
	else
	    rc = 0;
	break;
    case RPM_STRING_ARRAY_TYPE:
	break;
    }

    /* Allocate all returned storage (if not already). */
    if (p && nb && !he->freeData) {
	void * ptr = memcpy(xmalloc(nb), he->p.ptr, nb);
	he->p.ptr = ptr;
    }

exit:
    if (type)
	*type = he->t;
    if (p)
	p->ptr = he->p.ptr;
    if (c)
	*c = he->c;

    if (sw != NULL)	(void) rpmswExit(sw, 0);

    return rc;
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
 * @retval *p		pointer to tag value(s) (or NULL)
 * @retval *c		number of values (or NULL)
 * @return		1 on success, 0 on failure
 */
static
int headerGetEntry(Header h, int_32 tag,
			/*@null@*/ /*@out@*/ hTYP_t type,
			/*@null@*/ /*@out@*/ hRET_t * p,
			/*@null@*/ /*@out@*/ hCNT_t c)
	/*@modifies *type, *p, *c @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(p) >= 0 /\ maxSet(c) >= 0 @*/
{
    void * sw;
    int rc;

    if ((sw = headerGetStats(h, 19)) != NULL)	/* RPMTS_OP_HDRGET */
	(void) rpmswEnter(sw, 0);
    rc = intGetEntry(h, tag, type, p, c, 0);
    if (sw != NULL)	(void) rpmswExit(sw, 0);
    return rc;
}

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
static
int headerGetEntryMinMemory(Header h, int_32 tag,
			/*@null@*/ /*@out@*/ hTYP_t type,
			/*@null@*/ /*@out@*/ hRET_t * p,
			/*@null@*/ /*@out@*/ hCNT_t c)
	/*@modifies *type, *p, *c @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(p) >= 0 /\ maxSet(c) >= 0 @*/
{
    void * sw;
    int rc;

    if ((sw = headerGetStats(h, 19)) != NULL)	/* RPMTS_OP_HDRGET */
	(void) rpmswEnter(sw, 0);
    rc = intGetEntry(h, tag, type, p, c, 1);
    if (sw != NULL)	(void) rpmswExit(sw, 0);
    return rc;
}

int headerGetRawEntry(Header h, int_32 tag, rpmTagType * type, hRET_t * p, rpmTagCount * c)
{
    indexEntry entry;
    int rc;

    if (p == NULL) return headerIsEntry(h, tag);

    /* First find the tag */
    /*@-mods@*/		/*@ FIX: h modified by sort. */
    entry = findEntry(h, tag, RPM_NULL_TYPE);
    /*@=mods@*/
    if (!entry) {
	if (p) *(void **)p = NULL;
	if (c) *c = 0;
	return 0;
    }

    rc = copyEntry(entry, type, p, c, 0);

    /* XXX 1 on success */
    return ((rc == 1) ? 1 : 0);
}

/**
 */
static void copyData(rpmTagType type, /*@out@*/ void * dstPtr, const void * srcPtr,
		rpmTagCount cnt, int dataLength)
	/*@modifies *dstPtr @*/
{
    switch (type) {
    case RPM_STRING_ARRAY_TYPE:
    case RPM_I18NSTRING_TYPE:
    {	const char ** av = (const char **) srcPtr;
	char * t = dstPtr;

	while (cnt-- > 0 && dataLength > 0) {
	    const char * s;
	    if ((s = *av++) == NULL)
		continue;
	    do {
		*t++ = *s++;
	    } while (s[-1] && --dataLength > 0);
	}
    }	break;

    default:
	memmove(dstPtr, srcPtr, dataLength);
	break;
    }
}

/**
 * Return (malloc'ed) copy of entry data.
 * @param type		entry data type
 * @param p		entry data
 * @param c		entry item count
 * @retval *lenp	no. bytes in returned data
 * @return 		(malloc'ed) copy of entry data, NULL on error
 */
/*@null@*/
static void *
grabData(rpmTagType type, const void * p, rpmTagCount c, /*@out@*/ int * lenp)
	/*@modifies *lenp @*/
	/*@requires maxSet(lenp) >= 0 @*/
{
    void * data = NULL;
    int length;

    length = dataLength(type, p, c, 0, NULL);
    if (length > 0) {
	data = xmalloc(length);
	copyData(type, data, p, c, length);
    }

    if (lenp)
	*lenp = length;
    return data;
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
static
int headerAddEntry(Header h, int_32 tag, rpmTagType type, const void * p, rpmTagCount c)
	/*@modifies h @*/
{
    indexEntry entry;
    hPTR_t data;
    int length;

    /* Count must always be >= 1 for headerAddEntry. */
    if (c <= 0)
	return 0;

    if (hdrchkType(type))
	return 0;
    if (hdrchkData(c))
	return 0;

    length = 0;
    data = grabData(type, p, c, &length);
    if (data == NULL || length <= 0)
	return 0;

    /* Allocate more index space if necessary */
    if (h->indexUsed == h->indexAlloced) {
	h->indexAlloced += INDEX_MALLOC_SIZE;
	h->index = xrealloc(h->index, h->indexAlloced * sizeof(*h->index));
    }

    /* Fill in the index */
    entry = h->index + h->indexUsed;
    entry->info.tag = tag;
    entry->info.type = type;
    entry->info.count = c;
    entry->info.offset = 0;
    entry->data = (void *) (*data).ptr;	/* NOCAST */
    entry->length = length;

    if (h->indexUsed > 0 && tag < h->index[h->indexUsed-1].info.tag)
	h->flags &= ~HEADERFLAG_SORTED;
    h->indexUsed++;

    return 1;
}

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
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
static
int headerAppendEntry(Header h, int_32 tag, rpmTagType type,
		const void * p, rpmTagCount c)
	/*@modifies h @*/
{
    indexEntry entry;
    int length;

    if (type == RPM_STRING_TYPE || type == RPM_I18NSTRING_TYPE) {
	/* we can't do this */
	return 0;
    }

    /* Find the tag entry in the header. */
    entry = findEntry(h, tag, type);
    if (!entry)
	return 0;

    length = dataLength(type, p, c, 0, NULL);
    if (length < 0)
	return 0;

    if (ENTRY_IN_REGION(entry)) {
	char * t = xmalloc(entry->length + length);
	memcpy(t, entry->data, entry->length);
	entry->data = t;
	entry->info.offset = 0;
    } else
	entry->data = xrealloc(entry->data, entry->length + length);

    copyData(type, ((char *) entry->data) + entry->length, p, c, length);

    entry->length += length;

    entry->info.count += c;

    return 1;
}

/** \ingroup header
 * Add or append element to tag array in header.
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
static
int headerAddOrAppendEntry(Header h, int_32 tag, rpmTagType type,
		const void * p, rpmTagCount c)
	/*@modifies h @*/
{
    return (findEntry(h, tag, type)
	? headerAppendEntry(h, tag, type, p, c)
	: headerAddEntry(h, tag, type, p, c));
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
static
int headerAddI18NString(Header h, int_32 tag, const char * string,
		const char * lang)
	/*@modifies h @*/
{
    indexEntry table, entry;
    rpmTagData p;
    int length;
    int ghosts;
    int i, langNum;
    char * buf;

    table = findEntry(h, HEADER_I18NTABLE, RPM_STRING_ARRAY_TYPE);
    entry = findEntry(h, tag, RPM_I18NSTRING_TYPE);

    if (!table && entry)
	return 0;		/* this shouldn't ever happen!! */

    if (!table && !entry) {
	const char * argv[2];
	int count = 0;
	p.argv = argv;
	if (!lang || (lang[0] == 'C' && lang[1] == '\0')) {
	    /*@-observertrans -readonlytrans@*/
	    p.argv[count++] = "C";
	    /*@=observertrans =readonlytrans@*/
	} else {
	    /*@-observertrans -readonlytrans@*/
	    p.argv[count++] = "C";
	    /*@=observertrans =readonlytrans@*/
	    p.argv[count++] = lang;
	}
	if (!headerAddEntry(h, HEADER_I18NTABLE, RPM_STRING_ARRAY_TYPE, 
			p.ptr, count))
	    return 0;
	table = findEntry(h, HEADER_I18NTABLE, RPM_STRING_ARRAY_TYPE);
    }

    if (!table)
	return 0;
    if (!lang) lang = "C";

    {	const char * l = table->data;
	for (langNum = 0; langNum < table->info.count; langNum++) {
	    if (!strcmp(l, lang)) break;
	    l += strlen(l) + 1;
	}
    }

    if (langNum >= table->info.count) {
	length = strlen(lang) + 1;
	if (ENTRY_IN_REGION(table)) {
	    char * t = xmalloc(table->length + length);
	    memcpy(t, table->data, table->length);
	    table->data = t;
	    table->info.offset = 0;
	} else
	    table->data = xrealloc(table->data, table->length + length);
	memmove(((char *)table->data) + table->length, lang, length);
	table->length += length;
	table->info.count++;
    }

    if (!entry) {
	p.argv = alloca(sizeof(*p.argv) * (langNum + 1));
	for (i = 0; i < langNum; i++)
	    p.argv[i] = "";
	p.argv[langNum] = string;
	return headerAddEntry(h, tag, RPM_I18NSTRING_TYPE, p.ptr, langNum + 1);
    } else if (langNum >= entry->info.count) {
	ghosts = langNum - entry->info.count;
	
	length = strlen(string) + 1 + ghosts;
	if (ENTRY_IN_REGION(entry)) {
	    char * t = xmalloc(entry->length + length);
	    memcpy(t, entry->data, entry->length);
	    entry->data = t;
	    entry->info.offset = 0;
	} else
	    entry->data = xrealloc(entry->data, entry->length + length);

	memset(((char *)entry->data) + entry->length, '\0', ghosts);
	memmove(((char *)entry->data) + entry->length + ghosts, string, strlen(string)+1);

	entry->length += length;
	entry->info.count = langNum + 1;
    } else {
	char *b, *be, *e, *ee, *t;
	size_t bn, sn, en;

	/* Set beginning/end pointers to previous data */
	b = be = e = ee = entry->data;
	for (i = 0; i < table->info.count; i++) {
	    if (i == langNum)
		be = ee;
	    ee += strlen(ee) + 1;
	    if (i == langNum)
		e  = ee;
	}

	/* Get storage for new buffer */
	bn = (be-b);
	sn = strlen(string) + 1;
	en = (ee-e);
	length = bn + sn + en;
	t = buf = xmalloc(length);

	/* Copy values into new storage */
	memcpy(t, b, bn);
	t += bn;
/*@-mayaliasunique@*/
	memcpy(t, string, sn);
	t += sn;
	memcpy(t, e, en);
	t += en;
/*@=mayaliasunique@*/

	/* Replace i18N string array */
	entry->length -= strlen(be) + 1;
	entry->length += sn;
	
	if (ENTRY_IN_REGION(entry)) {
	    entry->info.offset = 0;
	} else
	    entry->data = _free(entry->data);
	/*@-dependenttrans@*/
	entry->data = buf;
	/*@=dependenttrans@*/
    }

    return 0;
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
static
int headerModifyEntry(Header h, int_32 tag, rpmTagType type,
			const void * p, rpmTagCount c)
	/*@modifies h @*/
{
    indexEntry entry;
    void * oldData;
    rpmTagData data;
    int length;

    /* First find the tag */
    entry = findEntry(h, tag, type);
    if (!entry)
	return 0;

    length = 0;
    data.ptr = grabData(type, p, c, &length);
    if (data.ptr == NULL || length <= 0)
	return 0;

    /* make sure entry points to the first occurence of this tag */
    while (entry > h->index && (entry - 1)->info.tag == tag)  
	entry--;

    /* free after we've grabbed the new data in case the two are intertwined;
       that's a bad idea but at least we won't break */
    oldData = entry->data;

    entry->info.count = c;
    entry->info.type = type;
    entry->data = data.ptr;
    entry->length = length;

    if (ENTRY_IN_REGION(entry)) {
	entry->info.offset = 0;
    } else
	oldData = _free(oldData);

    return 1;
}

/**
 */
static char escapedChar(const char ch)	/*@*/
{
    switch (ch) {
    case 'a': 	return '\a';
    case 'b': 	return '\b';
    case 'f': 	return '\f';
    case 'n': 	return '\n';
    case 'r': 	return '\r';
    case 't': 	return '\t';
    case 'v': 	return '\v';
    default:	return ch;
    }
}

/**
 * Mark a tag container with headerGetEntry() freeData.
 * @param he		tag container
 */
static HE_t rpmheMark(/*@null@*/ HE_t he)
	/*@modifies he @*/
{
    /* Set he->freeData as appropriate for headerGetEntry() . */
    if (he)
    switch (he->t) {
    default:
	he->freeData = 0;
	break;
    case RPM_STRING_ARRAY_TYPE:
    case RPM_BIN_TYPE:
	he->freeData = 1;
	break;
    }
    return he;
}

/**
 * Clean a tag container, free'ing attached malloc's..
 * @param he		tag container
 */
static HE_t rpmheClean(/*@null@*/ HE_t he)
	/*@modifies he @*/
{
    if (he) {
	if (he->freeData) {
if (_jbj)
fprintf(stderr, " ZAP: %p tag %s(%d)\t%d %p[%d:%d] free %d avail %d\n", he, tagName(he->tag), he->tag, he->t, he->p.ptr, he->ix, he->c, he->freeData, he->avail);
	    he->p.ptr = _free(he->p.ptr);
	}
	memset(he, 0, sizeof(*he));
    }
    return he;
}

#ifdef	NOTYET
/**
 * Destroy tag data cache.
 * @param tc		tag data cache
 * @param ntc		no. of elements
 * @return		NULL always
 */
static /*@null@*/ HE_t
rpmtcFree(/*@only@*/ HE_t tc, int ntc)
	/*@modifies ec @*/
{
    int i;

    if (tc != NULL)
    for (i = 0; i < ntc; i++)
	rpmtcClean(tc + i);
    tc = _free(tc);
    return NULL;
}

/**
 * Create tag data cache.
 * @param ntc		no. of elements
 * @return		new tag data cache
 */
static /*@only@*/ HE_t
rpmtcNew(int ntc)
	/*@*/
{
    HE_t ec = xcalloc(ntc+1, sizeof(*ec));
    return ec;
}
#endif

/**
 * Destroy headerSprintf format array.
 * @param format	sprintf format array
 * @param num		number of elements
 * @return		NULL always
 */
static /*@null@*/ sprintfToken
freeFormat( /*@only@*/ /*@null@*/ sprintfToken format, int num)
	/*@modifies *format @*/
{
    int i;

    if (format == NULL) return NULL;

    for (i = 0; i < num; i++) {
	switch (format[i].type) {
	case PTOK_TAG:
	    (void) rpmheClean(&format[i].u.tag.he);
	    break;
	case PTOK_ARRAY:
	    format[i].u.array.format =
		freeFormat(format[i].u.array.format,
			format[i].u.array.numTokens);
	    /*@switchbreak@*/ break;
	case PTOK_COND:
	    format[i].u.cond.ifFormat =
		freeFormat(format[i].u.cond.ifFormat, 
			format[i].u.cond.numIfTokens);
	    format[i].u.cond.elseFormat =
		freeFormat(format[i].u.cond.elseFormat, 
			format[i].u.cond.numElseTokens);
	    (void) rpmheClean(&format[i].u.cond.tag.he);
	    /*@switchbreak@*/ break;
	case PTOK_NONE:
	case PTOK_STRING:
	default:
	    /*@switchbreak@*/ break;
	}
    }
    format = _free(format);
    return NULL;
}

/**
 * Header tag iterator data structure.
 */
struct headerIterator_s {
/*@unused@*/
    Header h;		/*!< Header being iterated. */
/*@unused@*/
    int next_index;	/*!< Next tag index. */
};

/** \ingroup header
 * Destroy header tag iterator.
 * @param hi		header tag iterator
 * @return		NULL always
 */
static /*@null@*/
HeaderIterator headerFreeIterator(/*@only@*/ HeaderIterator hi)
	/*@modifies hi @*/
{
    if (hi != NULL) {
	hi->h = headerFree(hi->h);
	hi = _free(hi);
    }
    return hi;
}

/** \ingroup header
 * Create header tag iterator.
 * @param h		header
 * @return		header tag iterator
 */
static
HeaderIterator headerInitIterator(Header h)
	/*@modifies h */
{
    HeaderIterator hi = xmalloc(sizeof(*hi));

    headerSort(h);

    hi->h = headerLink(h);
    hi->next_index = 0;
    return hi;
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
static
int headerNextIterator(HeaderIterator hi,
		/*@null@*/ /*@out@*/ hTAG_t tag,
		/*@null@*/ /*@out@*/ hTYP_t type,
		/*@null@*/ /*@out@*/ hRET_t * p,
		/*@null@*/ /*@out@*/ hCNT_t c)
	/*@modifies hi, *tag, *type, *p, *c @*/
	/*@requires maxSet(tag) >= 0 /\ maxSet(type) >= 0
		/\ maxSet(p) >= 0 /\ maxSet(c) >= 0 @*/
{
    Header h = hi->h;
    int slot = hi->next_index;
    indexEntry entry = NULL;
    int rc;

    for (slot = hi->next_index; slot < h->indexUsed; slot++) {
	entry = h->index + slot;
	if (!ENTRY_IS_REGION(entry))
	    break;
    }
    hi->next_index = slot;
    if (entry == NULL || slot >= h->indexUsed)
	return 0;

    /*@-noeffect@*/	/* LCL: no clue */
    hi->next_index++;
    /*@=noeffect@*/

    if (tag)
	*tag = entry->info.tag;

    rc = copyEntry(entry, type, p, c, 0);

    /* XXX 1 on success */
    return ((rc == 1) ? 1 : 0);
}

/** \ingroup header
 * Duplicate a header.
 * @param h		header
 * @return		new header instance
 */
static /*@null@*/
Header headerCopy(Header h)
	/*@modifies h @*/
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    Header nh = headerNew();
    HeaderIterator hi;
   
    for (hi = headerInitIterator(h);
	headerNextIterator(hi, &he->tag, &he->t, &he->p, &he->c);
	he->p.ptr = headerFreeData(he->p.ptr, he->t))
    {
	if (he->p.ptr) (void) headerAddEntry(nh, he->tag, he->t, he->p.ptr, he->c);
    }
    hi = headerFreeIterator(hi);

    return headerReload(nh, HEADER_IMAGE);
}

/**
 */
typedef struct headerSprintfArgs_s {
    Header h;
    HE_s he;
    char * fmt;
/*@temp@*/
    headerTagTableEntry tags;
/*@temp@*/
    headerSprintfExtension exts;
/*@observer@*/ /*@null@*/
    const char * errmsg;
    HE_t ec;			/*!< Extension data cache. */
    int nec;			/*!< No. of extension cache items. */
#ifdef	NOTYET
    HE_t tc;			/*!< Tag data cache. */
    int ntc;			/*!< No. of tag cache items. */
#endif
    sprintfToken format;
/*@relnull@*/
    HeaderIterator hi;
/*@owned@*/
    char * val;
    size_t vallen;
    size_t alloced;
    int numTokens;
    int i;
} * headerSprintfArgs;

/**
 * Initialize an hsa iteration.
 * @param hsa		headerSprintf args
 * @return		headerSprintf args
 */
static headerSprintfArgs hsaInit(/*@returned@*/ headerSprintfArgs hsa)
	/*@modifies hsa */
{
    sprintfTag tag =
	(hsa->format->type == PTOK_TAG
	    ? &hsa->format->u.tag :
	(hsa->format->type == PTOK_ARRAY
	    ? &hsa->format->u.array.format->u.tag :
	NULL));

    if (hsa != NULL) {
	hsa->i = 0;
	if (tag != NULL && tag->tagno == -2)
	    hsa->hi = headerInitIterator(hsa->h);
    }
/*@-nullret@*/
    return hsa;
/*@=nullret@*/
}

/**
 * Return next hsa iteration item.
 * @param hsa		headerSprintf args
 * @return		next sprintfToken (or NULL)
 */
/*@null@*/
static sprintfToken hsaNext(/*@returned@*/ headerSprintfArgs hsa)
	/*@modifies hsa */
{
    sprintfToken fmt = NULL;
    sprintfTag tag =
	(hsa->format->type == PTOK_TAG
	    ? &hsa->format->u.tag :
	(hsa->format->type == PTOK_ARRAY
	    ? &hsa->format->u.array.format->u.tag :
	NULL));

    if (hsa != NULL && hsa->i >= 0 && hsa->i < hsa->numTokens) {
	fmt = hsa->format + hsa->i;
	if (hsa->hi == NULL) {
	    hsa->i++;
	} else {
	    HE_t he = rpmheClean(&tag->he);
	    if (!headerNextIterator(hsa->hi, &he->tag, &he->t, &he->p, &he->c))
	    {
		tag->tagno = 0;
		return NULL;
	    }
	    he = rpmheMark(he);
	    he->avail = 1;
if (_jbj)
fprintf(stderr, " NEW: %p tag %s(%d)\t%d %p[%d:%d] free %d avail %d\n", he, tagName(he->tag), he->tag, he->t, he->p.ptr, he->ix, he->c, he->freeData, he->avail);
	    tag->tagno = he->tag;
	}
    }

/*@-dependenttrans -onlytrans@*/
    return fmt;
/*@=dependenttrans =onlytrans@*/
}

/**
 * Finish an hsa iteration.
 * @param hsa		headerSprintf args
 * @return		headerSprintf args
 */
static headerSprintfArgs hsaFini(/*@returned@*/ headerSprintfArgs hsa)
	/*@modifies hsa */
{
    if (hsa != NULL) {
	hsa->hi = headerFreeIterator(hsa->hi);
	hsa->i = 0;
    }
/*@-nullret@*/
    return hsa;
/*@=nullret@*/
}

/**
 * Reserve sufficient buffer space for next output value.
 * @param hsa		headerSprintf args
 * @param need		no. of bytes to reserve
 * @return		pointer to reserved space
 */
/*@dependent@*/ /*@exposed@*/
static char * hsaReserve(headerSprintfArgs hsa, size_t need)
	/*@modifies hsa */
{
    if ((hsa->vallen + need) >= hsa->alloced) {
	if (hsa->alloced <= need)
	    hsa->alloced += need;
	hsa->alloced <<= 1;
	hsa->val = xrealloc(hsa->val, hsa->alloced+1);	
    }
    return hsa->val + hsa->vallen;
}

#if defined(SUPPORT_LINEAR_TAGTABLE_LOOKUP)
/**
 * Return tag name from value.
 * @todo bsearch on sorted value table.
 * @param tbl		tag table
 * @param val		tag value to find
 * @retval *typep	tag type (or NULL)
 * @return		tag name, NULL on not found
 */
/*@observer@*/ /*@null@*/
static const char * myTagName(headerTagTableEntry tbl, int val,
		/*@null@*/ int *typep)
	/*@modifies *typep @*/
{
    static char name[128];
    const char * s;
    char *t;

    for (; tbl->name != NULL; tbl++) {
	if (tbl->val == val)
	    break;
    }
    if ((s = tbl->name) == NULL)
	return NULL;
    s += sizeof("RPMTAG_") - 1;
    t = name;
    *t++ = *s++;
    while (*s != '\0')
	*t++ = xtolower(*s++);
    *t = '\0';
    if (typep)
	*typep = tbl->type;
    return name;
}

/**
 * Return tag value from name.
 * @todo bsearch on sorted name table.
 * @param tbl		tag table
 * @param name		tag name to find
 * @return		tag value, 0 on not found
 */
static int myTagValue(headerTagTableEntry tbl, const char * name)
	/*@*/
{
    for (; tbl->name != NULL; tbl++) {
	if (!xstrcasecmp(tbl->name, name))
	    return tbl->val;
    }
    return 0;
}
#endif

/**
 * Search extensions and tags for a name.
 * @param hsa		headerSprintf args
 * @param token		parsed fields
 * @param name		name to find
 * @return		0 on success, 1 on not found
 */
static int findTag(headerSprintfArgs hsa, sprintfToken token, const char * name)
	/*@modifies token @*/
{
    headerSprintfExtension exts = hsa->exts;
    headerSprintfExtension ext;
    sprintfTag stag = (token->type == PTOK_COND
	? &token->u.cond.tag : &token->u.tag);
    int extNum;

    stag->fmt = NULL;
    stag->ext = NULL;
    stag->extNum = 0;
    stag->tagno = -1;

    if (!strcmp(name, "*")) {
	stag->tagno = -2;
	goto bingo;
    }

    if (strncmp("RPMTAG_", name, sizeof("RPMTAG_")-1)) {
	char * t = alloca(strlen(name) + sizeof("RPMTAG_"));
	(void) stpcpy( stpcpy(t, "RPMTAG_"), name);
	name = t;
    }

    /* Search extensions for specific tag override. */
    for (ext = exts, extNum = 0; ext != NULL && ext->type != HEADER_EXT_LAST;
	ext = (ext->type == HEADER_EXT_MORE ? ext->u.more : ext+1), extNum++)
    {
	if (ext->name == NULL || ext->type != HEADER_EXT_TAG)
	    continue;
	if (!xstrcasecmp(ext->name, name)) {
	    stag->ext = ext->u.tagFunction;
	    stag->extNum = extNum;
	    goto bingo;
	}
    }

    /* Search tag names. */
#if defined(SUPPORT_LINEAR_TAGTABLE_LOOKUP)
    stag->tagno = myTagValue(hsa->tags, name);
#else
    stag->tagno = tagValue(name);
#endif
    if (stag->tagno != 0)
	goto bingo;

    return 1;

bingo:
    /* Search extensions for specific format. */
    if (stag->type != NULL)
    for (ext = exts; ext != NULL && ext->type != HEADER_EXT_LAST;
	    ext = (ext->type == HEADER_EXT_MORE ? ext->u.more : ext+1))
    {
	if (ext->name == NULL || ext->type != HEADER_EXT_FORMAT)
	    continue;
	if (!strcmp(ext->name, stag->type)) {
	    stag->fmt = ext->u.formatFunction;
	    break;
	}
    }
    return 0;
}

/**
 * Convert tag data representation.
 * @param he		tag container
 * @param fmt		output radix (NULL or "" assumes %d)
 * @return		formatted string
 */
static char * intFormat(HE_t he, const char *fmt)
{
    int ix = (he->ix > 0 ? he->ix : 0);
    int_64 ival = 0;
    const char * istr = NULL;
    char * b;
    size_t nb = 0;

    if (fmt == NULL || *fmt == '\0')
	fmt = "d";

    switch (he->t) {
    default:
	return xstrdup(_("(not a number)"));
	break;
    case RPM_CHAR_TYPE:	
    case RPM_INT8_TYPE:
	ival = he->p.i8p[ix];
	break;
    case RPM_INT16_TYPE:
	ival = he->p.ui16p[ix];	/* XXX note unsigned. */
	break;
    case RPM_INT32_TYPE:
	ival = he->p.i32p[ix];
	break;
    case RPM_INT64_TYPE:
	ival = he->p.i64p[ix];
	break;
    case RPM_STRING_TYPE:
	istr = he->p.str;
	break;
    case RPM_STRING_ARRAY_TYPE:
	istr = he->p.argv[ix];
	break;
    case RPM_OPENPGP_TYPE:	/* XXX W2DO? */
    case RPM_ASN1_TYPE:		/* XXX W2DO? */
    case RPM_BIN_TYPE:
	{   static char hex[] = "0123456789abcdef";
	    const char * s = he->p.str;
	    int c = he->c;
	    char * t;

	    nb = 2 * c + 1;
	    t = b = alloca(nb+1);
	    while (c-- > 0) {
		unsigned int i;
		i = *s++;
		*t++ = hex[ (i >> 4) & 0xf ];
		*t++ = hex[ (i     ) & 0xf ];
	    }
	    *t = '\0';
	}   break;
    }

    if (istr) {		/* string */
	b = (char *)istr;	/* NOCAST */
    } else
    if (nb == 0) {	/* number */
	char myfmt[] = "%llX";
	myfmt[3] = *fmt;
	nb = 64;
	b = alloca(nb);
	snprintf(b, nb, myfmt, ival);
	b[nb-1] = '\0';
    }

    return xstrdup(b);
}

/**
 * Return octal formatted data.
 * @param he		tag container
 * @return		formatted string
 */
static char * octFormat(HE_t he)
	/*@*/
{
    return intFormat(he, "o");
}

/**
 * Return hex formatted data.
 * @param he		tag container
 * @return		formatted string
 */
static char * hexFormat(HE_t he)
	/*@*/
{
    return intFormat(he, "x");
}

/**
 * Return decimal formatted data.
 * @param he		tag container
 * @return		formatted string
 */
static char * decFormat(HE_t he)
	/*@*/
{
    return intFormat(he, "d");
}

/**
 * Return strftime formatted data.
 * @param he		tag container
 * @param strftimeFormat strftime(3) format
 * @return		formatted string
 */
static char * realDateFormat(HE_t he, const char * strftimeFormat)
	/*@*/
{
    rpmTagData data = { .ptr = he->p.ptr };
    char * val;

    if (he->t != RPM_INT64_TYPE) {
	val = xstrdup(_("(not a number)"));
    } else {
	struct tm * tstruct;
	char buf[50];

	/* this is important if sizeof(int_64) ! sizeof(time_t) */
	{   time_t dateint = data.ui64p[0];
	    tstruct = localtime(&dateint);
	}
	buf[0] = '\0';
	if (tstruct)
	    (void) strftime(buf, sizeof(buf) - 1, strftimeFormat, tstruct);
	buf[sizeof(buf) - 1] = '\0';
	val = xstrdup(buf);
    }

    return val;
}

/**
 * Return date formatted data.
 * @param he		tag container
 * @return		formatted string
 */
static char * dateFormat(HE_t he)
	/*@*/
{
    return realDateFormat(he, _("%c"));
}

/**
 * Return day formatted data.
 * @param he		tag container
 * @return		formatted string
 */
static char * dayFormat(HE_t he)
	/*@*/
{
    return realDateFormat(he, _("%a %b %d %Y"));
}

/**
 * Return shell escape formatted data.
 * @param he		tag container
 * @return		formatted string
 */
static char * shescapeFormat(HE_t he)
	/*@*/
{
    rpmTagData data = { .ptr = he->p.ptr };
    char * val;
    size_t nb;

    /* XXX one of these integer types is unnecessary. */
    if (he->t == RPM_INT32_TYPE) {
	nb = 20;
	val = xmalloc(nb);
	snprintf(val, nb, "%d", data.i32p[0]);
	val[nb-1] = '\0';
    } else if (he->t == RPM_INT64_TYPE) {
	nb = 40;
	val = xmalloc(40);
	snprintf(val, nb, "%lld", data.i64p[0]);
	val[nb-1] = '\0';
    } else if (he->t == RPM_STRING_TYPE) {
	const char * s = data.str;
	char * t;
	int c;

	nb = strlen(data.str) + 1;
	/* XXX count no. of escapes instead. */
	t = xmalloc(4 * nb + 3);
	*t++ = '\'';
	while ((c = *s++) != 0) {
	    if (c == '\'') {
		*t++ = '\'';
		*t++ = '\\';
		*t++ = '\'';
	    }
	    *t++ = c;
	}
	*t++ = '\'';
	*t = '\0';
	nb = strlen(t) + 1;
	val = xrealloc(t, nb);
    } else
	val = xstrdup(_("invalid type"));

    return val;
}

/*@-type@*/ /* FIX: cast? */
const struct headerSprintfExtension_s headerDefaultFormats[] = {
    { HEADER_EXT_FORMAT, "octal",
	{ .formatFunction = octFormat } },
    { HEADER_EXT_FORMAT, "oct",
	{ .formatFunction = octFormat } },
    { HEADER_EXT_FORMAT, "hex",
	{ .formatFunction = hexFormat } },
    { HEADER_EXT_FORMAT, "decimal",
	{ .formatFunction = decFormat } },
    { HEADER_EXT_FORMAT, "dec",
	{ .formatFunction = decFormat } },
    { HEADER_EXT_FORMAT, "date",
	{ .formatFunction = dateFormat } },
    { HEADER_EXT_FORMAT, "day",
	{ .formatFunction = dayFormat } },
    { HEADER_EXT_FORMAT, "shescape",
	{ .formatFunction = shescapeFormat } },
    { HEADER_EXT_LAST, NULL, { NULL } }
};
/*@=type@*/

/* forward ref */
/**
 * Parse a headerSprintf expression.
 * @param hsa		headerSprintf args
 * @param token
 * @param str
 * @retval *endPtr
 * @return		0 on success
 */
static int parseExpression(headerSprintfArgs hsa, sprintfToken token,
		char * str, /*@out@*/char ** endPtr)
	/*@modifies hsa, str, token, *endPtr @*/
	/*@requires maxSet(endPtr) >= 0 @*/;

/**
 * Parse a headerSprintf term.
 * @param hsa		headerSprintf args
 * @param str
 * @retval *formatPtr
 * @retval *numTokensPtr
 * @retval *endPtr
 * @param state
 * @return		0 on success
 */
static int parseFormat(headerSprintfArgs hsa, /*@null@*/ char * str,
		/*@out@*/sprintfToken * formatPtr, /*@out@*/int * numTokensPtr,
		/*@null@*/ /*@out@*/ char ** endPtr, int state)
	/*@modifies hsa, str, *formatPtr, *numTokensPtr, *endPtr @*/
	/*@requires maxSet(formatPtr) >= 0 /\ maxSet(numTokensPtr) >= 0
		/\ maxSet(endPtr) >= 0 @*/
{
    char * chptr, * start, * next, * dst;
    sprintfToken format;
    sprintfToken token;
    int numTokens;
    int i;
    int done = 0;

    /* upper limit on number of individual formats */
    numTokens = 0;
    if (str != NULL)
    for (chptr = str; *chptr != '\0'; chptr++)
	if (*chptr == '%') numTokens++;
    numTokens = numTokens * 2 + 1;

    format = xcalloc(numTokens, sizeof(*format));
    if (endPtr) *endPtr = NULL;

    /*@-infloops@*/ /* LCL: can't detect done termination */
    dst = start = str;
    numTokens = 0;
    token = NULL;
    if (start != NULL)
    while (*start != '\0') {
	switch (*start) {
	case '%':
	    /* handle %% */
	    if (*(start + 1) == '%') {
		if (token == NULL || token->type != PTOK_STRING) {
		    token = format + numTokens++;
		    token->type = PTOK_STRING;
		    /*@-temptrans -assignexpose@*/
		    dst = token->u.string.string = start;
		    /*@=temptrans =assignexpose@*/
		}
		start++;
		*dst++ = *start++;
		/*@switchbreak@*/ break;
	    } 

	    token = format + numTokens++;
	    *dst++ = '\0';
	    start++;

	    if (*start == '|') {
		char * newEnd;

		start++;
		if (parseExpression(hsa, token, start, &newEnd))
		{
		    format = freeFormat(format, numTokens);
		    return 1;
		}
		start = newEnd;
		/*@switchbreak@*/ break;
	    }

	    /*@-assignexpose@*/
	    token->u.tag.format = start;
	    /*@=assignexpose@*/
	    token->u.tag.pad = 0;
	    token->u.tag.justOne = 0;
	    token->u.tag.arrayCount = 0;

	    chptr = start;
	    while (*chptr && *chptr != '{' && *chptr != '%') chptr++;
	    if (!*chptr || *chptr == '%') {
		hsa->errmsg = _("missing { after %");
		format = freeFormat(format, numTokens);
		return 1;
	    }

	    *chptr++ = '\0';

	    while (start < chptr) {
		if (xisdigit(*start)) {
		    i = strtoul(start, &start, 10);
		    token->u.tag.pad += i;
		    start = chptr;
		    break;
		} else {
		    start++;
		}
	    }

	    if (*start == '=') {
		token->u.tag.justOne = 1;
		start++;
	    } else if (*start == '#') {
		token->u.tag.justOne = 1;
		token->u.tag.arrayCount = 1;
		start++;
	    }

	    next = start;
	    while (*next && *next != '}') next++;
	    if (!*next) {
		hsa->errmsg = _("missing } after %{");
		format = freeFormat(format, numTokens);
		return 1;
	    }
	    *next++ = '\0';

	    chptr = start;
	    while (*chptr && *chptr != ':') chptr++;

	    if (*chptr != '\0') {
		*chptr++ = '\0';
		if (!*chptr) {
		    hsa->errmsg = _("empty tag format");
		    format = freeFormat(format, numTokens);
		    return 1;
		}
		/*@-assignexpose@*/
		token->u.tag.type = chptr;
		/*@=assignexpose@*/
	    } else {
		token->u.tag.type = NULL;
	    }
	    
	    if (!*start) {
		hsa->errmsg = _("empty tag name");
		format = freeFormat(format, numTokens);
		return 1;
	    }

	    i = 0;
	    token->type = PTOK_TAG;

	    if (findTag(hsa, token, start)) {
		hsa->errmsg = _("unknown tag");
		format = freeFormat(format, numTokens);
		return 1;
	    }

	    dst = start = next;
	    /*@switchbreak@*/ break;

	case '[':
	    *start++ = '\0';
	    token = format + numTokens++;

	    if (parseFormat(hsa, start,
			    &token->u.array.format,
			    &token->u.array.numTokens,
			    &start, PARSER_IN_ARRAY))
	    {
		format = freeFormat(format, numTokens);
		return 1;
	    }

	    if (!start) {
		hsa->errmsg = _("] expected at end of array");
		format = freeFormat(format, numTokens);
		return 1;
	    }

	    dst = start;

	    token->type = PTOK_ARRAY;

	    /*@switchbreak@*/ break;

	case ']':
	    if (state != PARSER_IN_ARRAY) {
		hsa->errmsg = _("unexpected ]");
		format = freeFormat(format, numTokens);
		return 1;
	    }
	    *start++ = '\0';
	    if (endPtr) *endPtr = start;
	    done = 1;
	    /*@switchbreak@*/ break;

	case '}':
	    if (state != PARSER_IN_EXPR) {
		hsa->errmsg = _("unexpected }");
		format = freeFormat(format, numTokens);
		return 1;
	    }
	    *start++ = '\0';
	    if (endPtr) *endPtr = start;
	    done = 1;
	    /*@switchbreak@*/ break;

	default:
	    if (token == NULL || token->type != PTOK_STRING) {
		token = format + numTokens++;
		token->type = PTOK_STRING;
		/*@-temptrans -assignexpose@*/
		dst = token->u.string.string = start;
		/*@=temptrans =assignexpose@*/
	    }

	    if (*start == '\\') {
		start++;
		*dst++ = escapedChar(*start++);
	    } else {
		*dst++ = *start++;
	    }
	    /*@switchbreak@*/ break;
	}
	if (done)
	    break;
    }
    /*@=infloops@*/

    if (dst != NULL)
        *dst = '\0';

    for (i = 0; i < numTokens; i++) {
	token = format + i;
	switch(token->type) {
	default:
	    break;
	case PTOK_STRING:
	    token->u.string.len = strlen(token->u.string.string);
	    break;
	}
    }
    for (i = 0; i < numTokens; i++) {
	token = format + i;
if (_jbj)
fprintf(stderr, "%4d: %p", i, token);
	switch(token->type) {
	case PTOK_NONE:
if (_jbj)
fprintf(stderr, " none(%d)", token->type);
	    break;
	case PTOK_TAG:
if (_jbj)
fprintf(stderr, " tag(%d) %s format \"%s\" pad %d", token->type, tagName(token->u.tag.tagno), token->u.tag.format, token->u.tag.pad);
	    break;
	case PTOK_ARRAY:
if (_jbj)
fprintf(stderr, " array(%d)", token->type);
	    break;
	case PTOK_STRING:
	    token->u.string.len = strlen(token->u.string.string);
if (_jbj)
fprintf(stderr, " string(%d) \"%s\"", token->type, token->u.string.string);
	    break;
	case PTOK_COND:
if (_jbj)
fprintf(stderr, " cond(%d)", token->type);
	    break;
	}
if (_jbj)
fprintf(stderr, "\n");
    }

    *numTokensPtr = numTokens;
    *formatPtr = format;

    return 0;
}

static int parseExpression(headerSprintfArgs hsa, sprintfToken token,
		char * str, /*@out@*/ char ** endPtr)
{
    char * chptr;
    char * end;

    hsa->errmsg = NULL;
    chptr = str;
    while (*chptr && *chptr != '?') chptr++;

    if (*chptr != '?') {
	hsa->errmsg = _("? expected in expression");
	return 1;
    }

    *chptr++ = '\0';

    if (*chptr != '{') {
	hsa->errmsg = _("{ expected after ? in expression");
	return 1;
    }

    chptr++;

    if (parseFormat(hsa, chptr, &token->u.cond.ifFormat, 
		    &token->u.cond.numIfTokens, &end, PARSER_IN_EXPR)) 
	return 1;

    /* XXX fix segfault on "rpm -q rpm --qf='%|NAME?{%}:{NAME}|\n'"*/
    if (!(end && *end)) {
	hsa->errmsg = _("} expected in expression");
	token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	return 1;
    }

    chptr = end;
    if (*chptr != ':' && *chptr != '|') {
	hsa->errmsg = _(": expected following ? subexpression");
	token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	return 1;
    }

    if (*chptr == '|') {
	if (parseFormat(hsa, NULL, &token->u.cond.elseFormat, 
		&token->u.cond.numElseTokens, &end, PARSER_IN_EXPR))
	{
	    token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	    return 1;
	}
    } else {
	chptr++;

	if (*chptr != '{') {
	    hsa->errmsg = _("{ expected after : in expression");
	    token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	    return 1;
	}

	chptr++;

	if (parseFormat(hsa, chptr, &token->u.cond.elseFormat, 
			&token->u.cond.numElseTokens, &end, PARSER_IN_EXPR)) 
	    return 1;

	/* XXX fix segfault on "rpm -q rpm --qf='%|NAME?{a}:{%}|{NAME}\n'" */
	if (!(end && *end)) {
	    hsa->errmsg = _("} expected in expression");
	    token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	    return 1;
	}

	chptr = end;
	if (*chptr != '|') {
	    hsa->errmsg = _("| expected at end of expression");
	    token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	    token->u.cond.elseFormat =
		freeFormat(token->u.cond.elseFormat, token->u.cond.numElseTokens);
	    return 1;
	}
    }
	
    chptr++;

    *endPtr = chptr;

    token->type = PTOK_COND;

    (void) findTag(hsa, token, str);

    return 0;
}

#if defined(SUPPORT_SPRINTF_EXTTABLE)
/**
 * Call a header extension only once, saving results.
 * @param hsa		headerSprintf args
 * @param fn		function
 * @retval he		tag container
 * @retval ec		extension cache
 * @return		0 on success, 1 on failure
 */
static int getExtension(headerSprintfArgs hsa, headerTagTagFunction fn,
		HE_t he, HE_t ec)
	/*@modifies he, ec @*/
{
    int rc = 0;
    if (!ec->avail) {
	he = rpmheClean(he);
	rc = fn(hsa->h, he);
	*ec = *he;	/* structure copy. */
	if (!rc)
	    ec->avail = 1;
    } else
	*he = *ec;	/* structure copy. */
    he->freeData = 0;
    return rc;
}
#endif	/* SUPPORT_SPRINTF_EXTTABLE */

/**
 * Format a single item's value.
 * @param hsa		headerSprintf args
 * @param tag		tag
 * @param element	element index
 * @return		end of formatted string (NULL on error)
 */
/*@observer@*/ /*@null@*/
static char * formatValue(headerSprintfArgs hsa, sprintfTag tag, int element)
	/*@modifies hsa @*/
{
    HE_t vhe = memset(alloca(sizeof(*vhe)), 0, sizeof(*vhe));
    HE_t he = &tag->he;
    char * val = NULL;
    size_t need = 0;
    char * t, * te;
    int_64 ival = 0;
    rpmTagCount countBuf;
    int xx;

/* XXX likely need to check !he->avail and he->tag == tag->tagno. */
#if defined(SUPPORT_SPRINTF_EXTTABLE)
    if (tag->ext) {
	if (getExtension(hsa, tag->ext, he, hsa->ec + tag->extNum))
	{
	    he->c = 1;
	    he->t = RPM_STRING_TYPE;	
	    he->p.str = "(none)";
	    he->freeData = 0;
	}
    } else
#endif	/* SUPPORT_SPRINTF_EXTTABLE */
    if (!he->avail || he->tag != tag->tagno) {
	he = rpmheClean(he);
	he->tag = tag->tagno;	/* XXX necessary? */
	xx = headerGetExtension(hsa->h, he->tag, &he->t, &he->p, &he->c);
	if (xx && he->p.ptr != NULL)
	    he->freeData = 1;
	if (!xx) {
	    he->c = 1;
	    he->t = RPM_STRING_TYPE;	
	    he->p.str = "(none)";
	    he->freeData = 0;
	}
if (_jbj)
fprintf(stderr, " NEW: %p tag %s(%d)\t%d %p[%d:%d] free %d avail %d\n", he, tagName(he->tag), he->tag, he->t, he->p.ptr, he->ix, he->c, he->freeData, he->avail);
    }

    if (tag->arrayCount) {
	countBuf = he->c;
	he = rpmheClean(he);
	he->t = RPM_INT32_TYPE;
	he->p.i32p = &countBuf;
	he->c = 1;
	he->freeData = 0;
    }
    he->avail = 1;

    if (he->p.ptr)
    switch (he->t) {
    default:
	val = xstrdup("(unknown type)");
	need = strlen(val) + 1;
	goto exit;
	/*@notreached@*/ break;
    case RPM_STRING_ARRAY_TYPE:
	vhe->p.str = he->p.argv[element];
	vhe->t = RPM_STRING_TYPE;
	vhe->c = he->c;
	/* XXX TODO: force array representation? */
	vhe->ix = (he->c > 1 ? 0 : -1);
	break;
    case RPM_STRING_TYPE:
	vhe->p.str = he->p.str;
	vhe->t = RPM_STRING_TYPE;
	vhe->c = he->c;
	vhe->ix = -1;
	break;
    case RPM_CHAR_TYPE:
    case RPM_INT8_TYPE:
    case RPM_INT16_TYPE:
    case RPM_INT32_TYPE:
    case RPM_INT64_TYPE:
	switch (he->t) {
	default:
assert(0);	/* XXX keep gcc quiet. */
	    break;
	case RPM_CHAR_TYPE:	
	case RPM_INT8_TYPE:
	    ival = he->p.i8p[element];
	    break;
	case RPM_INT16_TYPE:
	    ival = he->p.ui16p[element];	/* XXX note unsigned. */
	    break;
	case RPM_INT32_TYPE:
	    ival = he->p.i32p[element];
	    break;
	case RPM_INT64_TYPE:
	    ival = he->p.i64p[element];
	    break;
	}
	vhe->t = RPM_INT64_TYPE;
	vhe->p.i64p = &ival;
	vhe->c = he->c;
	/* XXX TODO: force array representation? */
	vhe->ix = (he->c > 1 ? 0 : -1);
	break;

    case RPM_OPENPGP_TYPE:	/* XXX W2DO? */
    case RPM_ASN1_TYPE:		/* XXX W2DO? */
    case RPM_BIN_TYPE:
	vhe->t = RPM_BIN_TYPE;
	vhe->p.ptr = he->p.ptr;
	vhe->c = he->c;
	vhe->ix = -1;
	break;
    }

    if (tag->fmt)
	val = tag->fmt(vhe);
    else
	val = intFormat(vhe, NULL);
assert(val);
    if (val)
	need = strlen(val) + 1;

exit:
    if (!_tagcache)
	he = rpmheClean(he);

    if (val && need > 0) {
	if (tag->format && *tag->format && tag->pad) {
	    size_t nb;
	    nb = strlen(tag->format) + sizeof("%s");
	    t = alloca(nb);
	    (void) stpcpy( stpcpy( stpcpy(t, "%"), tag->format), "s");
	    nb = tag->pad + strlen(val) + 1;
	    te = xmalloc(nb);
	    (void) snprintf(te, nb, t, val);
	    te[nb-1] = '\0';
	    val = _free(val);
	    val = te;
	    need += tag->pad;
	}
	t = hsaReserve(hsa, need);
	te = stpcpy(t, val);
	hsa->vallen += (te - t);
	val = _free(val);
    }

    return (hsa->val + hsa->vallen);
}

/**
 * Format a single headerSprintf item.
 * @param hsa		headerSprintf args
 * @param token		item to format
 * @param element	element index
 * @return		end of formatted string (NULL on error)
 */
/*@observer@*/
static char * singleSprintf(headerSprintfArgs hsa, sprintfToken token,
		int element)
	/*@modifies hsa @*/
{
    char numbuf[64];	/* XXX big enuf for "Tag_0x01234567" */
    char * t, * te;
    int i, j;
    int numElements;
    sprintfToken spft;
    sprintfTag tag;
    HE_t he;
    int condNumFormats;
    size_t need;
    int xx;

    /* we assume the token and header have been validated already! */

    switch (token->type) {
    case PTOK_NONE:
	break;

    case PTOK_STRING:
	need = token->u.string.len;
	if (need == 0) break;
	t = hsaReserve(hsa, need);
	te = stpcpy(t, token->u.string.string);
	hsa->vallen += (te - t);
	break;

    case PTOK_TAG:
	t = hsa->val + hsa->vallen;
	te = formatValue(hsa, &token->u.tag,
			(token->u.tag.justOne ? 0 : element));
	if (te == NULL)
	    return NULL;
	break;

    case PTOK_COND:
	if (token->u.cond.tag.ext
	 || headerIsEntry(hsa->h, token->u.cond.tag.tagno))
	{
	    spft = token->u.cond.ifFormat;
	    condNumFormats = token->u.cond.numIfTokens;
	} else {
	    spft = token->u.cond.elseFormat;
	    condNumFormats = token->u.cond.numElseTokens;
	}

	need = condNumFormats * 20;
	if (spft == NULL || need == 0) break;

	t = hsaReserve(hsa, need);
	for (i = 0; i < condNumFormats; i++, spft++) {
	    te = singleSprintf(hsa, spft, element);
	    if (te == NULL)
		return NULL;
	}
	break;

    case PTOK_ARRAY:
	numElements = -1;
	spft = token->u.array.format;
	for (i = 0; i < token->u.array.numTokens; i++, spft++)
	{
	    tag = &spft->u.tag;
	    if (spft->type != PTOK_TAG || tag->arrayCount || tag->justOne)
		continue;

	    he = &tag->he;
/* XXX likely need to check !he->avail and he->tag == tag->tagno. */
#if defined(SUPPORT_SPRINTF_EXTTABLE)
	    if (tag->ext) {
		if (getExtension(hsa, tag->ext, he, hsa->ec + tag->extNum))
		     continue;
	    } else
#endif	/* SUPPORT_SPRINTF_EXTTABLE */
	    if (!he->avail || he->tag != tag->tagno) {
		he = rpmheClean(he);
		he->tag = tag->tagno;
		xx = headerGetExtension(hsa->h, he->tag, &he->t, &he->p, &he->c);
		if (xx && he->p.ptr != NULL)
		    he->freeData = 1;
		if (!xx)
		    continue;
if (_jbj)
fprintf(stderr, " NEW: %p tag %s(%d)\t%d %p[%d:%d] free %d avail %d\n", he, tagName(he->tag), he->tag, he->t, he->p.ptr, he->ix, he->c, he->freeData, he->avail);
	    } 

	    /* Check iteration arrays are same dimension (or scalar). */
	    switch (he->t) {
	    default:
		if (numElements == -1) {
		    numElements = he->c;
		    /*@switchbreak@*/ break;
		}
		if (he->c == numElements)
		    /*@switchbreak@*/ break;
		hsa->errmsg =
			_("array iterator used with different sized arrays");
		he = rpmheClean(he);
		return NULL;
		/*@notreached@*/ /*@switchbreak@*/ break;
	    case RPM_OPENPGP_TYPE:
	    case RPM_ASN1_TYPE:
	    case RPM_BIN_TYPE:
	    case RPM_STRING_TYPE:
		if (numElements == -1)
		    numElements = 1;
		/*@switchbreak@*/ break;
	    }
	}

	if (numElements == -1) {
#ifdef	DYING	/* XXX lots of pugly "(none)" lines with --conflicts. */
	    need = sizeof("(none)\n") - 1;
	    t = hsaReserve(hsa, need);
	    te = stpcpy(t, "(none)\n");
	    hsa->vallen += (te - t);
#endif
	} else {
	    int isxml;
	    int isyaml;

	    need = numElements * token->u.array.numTokens;
	    if (need == 0) break;

	    spft = token->u.array.format;
	    tag = &spft->u.tag;

	    isxml = (spft->type == PTOK_TAG && tag->type != NULL &&
		!strcmp(tag->type, "xml"));
	    isyaml = (spft->type == PTOK_TAG && tag->type != NULL &&
		!strcmp(tag->type, "yaml"));

	    if (isxml) {
#if defined(SUPPORT_LINEAR_TAGTABLE_LOOKUP)
		const char * tagN = myTagName(hsa->tags, tag->tagno, NULL);
#else
		const char * tagN = tagName(tag->tagno);
#endif
		/* XXX display "Tag_0x01234567" for unknown tags. */
		if (tagN == NULL) {
		    (void) snprintf(numbuf, sizeof(numbuf), "Tag_0x%08x",
				tag->tagno);
		    numbuf[sizeof(numbuf)-1] = '\0';
		    tagN = numbuf;
		}
		need = sizeof("  <rpmTag name=\"\">\n") + strlen(tagN);
		te = t = hsaReserve(hsa, need);
		te = stpcpy( stpcpy( stpcpy(te, "  <rpmTag name=\""), tagN), "\">\n");
		hsa->vallen += (te - t);
	    }
	    if (isyaml) {
#if defined(SUPPORT_LINEAR_TAGTABLE_LOOKUP)
		int tagT = -1;
		const char * tagN = myTagName(hsa->tags, tag->tagno, &tagT);
#else
		rpmTagType tagT = tagType(tag->tagno);
		const char * tagN = tagName(tag->tagno);
#endif
		/* XXX display "Tag_0x01234567" for unknown tags. */
		if (tagN == NULL) {
		    (void) snprintf(numbuf, sizeof(numbuf), "Tag_0x%08x",
				tag->tagno);
		    numbuf[sizeof(numbuf)-1] = '\0';
		    tagN = numbuf;
		    tagT = numElements > 1
			?  RPM_ARRAY_RETURN_TYPE : RPM_SCALAR_RETURN_TYPE;
		}
		need = sizeof("  :     - ") + strlen(tagN);
		te = t = hsaReserve(hsa, need);
		*te++ = ' ';
		*te++ = ' ';
		te = stpcpy(te, tagN);
		*te++ = ':';
		*te++ = (((tagT & RPM_MASK_RETURN_TYPE) == RPM_ARRAY_RETURN_TYPE)
			? '\n' : ' ');
		/* XXX Dirnames: in srpms need "    " indent */
		if (((tagT & RPM_MASK_RETURN_TYPE) == RPM_ARRAY_RETURN_TYPE)
		 && numElements == 1) {
		    te = stpcpy(te, "    ");
		    if (tag->tagno != 1118)
			te = stpcpy(te, "- ");
		}
		*te = '\0';
		hsa->vallen += (te - t);
	    }

	    need = numElements * token->u.array.numTokens * 10;
	    t = hsaReserve(hsa, need);
	    for (j = 0; j < numElements; j++) {
		spft = token->u.array.format;
		for (i = 0; i < token->u.array.numTokens; i++, spft++) {
		    te = singleSprintf(hsa, spft, j);
		    if (te == NULL)
			return NULL;
		}
	    }

	    if (isxml) {
		need = sizeof("  </rpmTag>\n") - 1;
		te = t = hsaReserve(hsa, need);
		te = stpcpy(te, "  </rpmTag>\n");
		hsa->vallen += (te - t);
	    }
	    if (isyaml) {
#if 0
		need = sizeof("\n") - 1;
		te = t = hsaReserve(hsa, need);
		te = stpcpy(te, "\n");
		hsa->vallen += (te - t);
#endif
	    }

	}
	break;
    }

    return (hsa->val + hsa->vallen);
}

#if defined(SUPPORT_SPRINTF_EXTTABLE)
/**
 * Create an extension cache.
 * @param exts		headerSprintf extensions
 * @retval *necp	no. of elements
 * @return		new extension cache
 */
static /*@only@*/ HE_t
rpmecNew(const headerSprintfExtension exts, /*@null@*/ int * necp)
	/*@*/
{
    headerSprintfExtension ext;
    HE_t ec;
    int extNum = 0;

    if (exts != NULL)
    for (ext = exts, extNum = 0; ext != NULL && ext->type != HEADER_EXT_LAST;
	ext = (ext->type == HEADER_EXT_MORE ? ext->u.more : ext+1), extNum++)
    {
	;
    }

    ec = xcalloc(extNum+1, sizeof(*ec));
    if (*necp)
	*necp = extNum;
    return ec;
}

/**
 * Destroy an extension cache.
 * @param exts		headerSprintf extensions
 * @param ec		extension cache
 * @return		NULL always
 */
static /*@null@*/ HE_t
rpmecFree(const headerSprintfExtension exts, /*@only@*/ HE_t ec)
	/*@modifies ec @*/
{
    headerSprintfExtension ext;
    int extNum;

    for (ext = exts, extNum = 0; ext != NULL && ext->type != HEADER_EXT_LAST;
	ext = (ext->type == HEADER_EXT_MORE ? ext->u.more : ext+1), extNum++)
    {
	if (ec[extNum].freeData)
	    ec[extNum].p.ptr = _free(ec[extNum].p.ptr);
    }

    ec = _free(ec);
    return NULL;
}
#endif	/* SUPPORT_SPRINTF_EXTTABLE */

/** \ingroup header
 * Return formatted output string from header tags.
 * The returned string must be free()d.
 *
 * @param h		header
 * @param fmt		format to use
 * @param tags		array of tag name/value/type triples (NULL uses default)
 * @param exts		formatting extensions chained table (NULL uses default)
 * @retval *errmsg	error message (if any)
 * @return		formatted output string (malloc'ed)
 */
static /*@only@*/ /*@null@*/
char * headerSprintf(Header h, const char * fmt,
		/*@null@*/ const struct headerTagTableEntry_s * tags,
		/*@null@*/ const struct headerSprintfExtension_s * exts,
		/*@null@*/ /*@out@*/ errmsg_t * errmsg)
	/*@modifies h, *errmsg @*/
	/*@requires maxSet(errmsg) >= 0 @*/
{
    headerSprintfArgs hsa = memset(alloca(sizeof(*hsa)), 0, sizeof(*hsa));
    sprintfToken nextfmt;
    sprintfTag tag;
    char * t, * te;
    int isxml;
    int isyaml;
    int need;

    /* Set some reasonable defaults */
    if (tags == NULL)
	tags = rpmTagTable;
    /* XXX this loses the extensions in lib/formats.c. */
    if (exts == NULL)
	exts = headerCompoundFormats;
 
    hsa->h = headerLink(h);
    hsa->fmt = xstrdup(fmt);
/*@-castexpose@*/	/* FIX: legacy API shouldn't change. */
    hsa->exts = (headerSprintfExtension) exts;
#if defined(SUPPORT_LINEAR_TAGTABLE_LOOKUP)
    hsa->tags = (headerTagTableEntry) tags;
#endif
/*@=castexpose@*/
    hsa->errmsg = NULL;

    if (parseFormat(hsa, hsa->fmt, &hsa->format, &hsa->numTokens, NULL, PARSER_BEGIN))
	goto exit;

#if defined(SUPPORT_SPRINTF_EXTTABLE)
    hsa->nec = 0;
    hsa->ec = rpmecNew(hsa->exts, &hsa->nec);
#endif	/* SUPPORT_SPRINTF_EXTTABLE */
#ifdef	NOTYET
    hsa->ntc = numTokens;
    hsa->tc = rpmtcNew(hsa->ntc);
#endif
    hsa->val = xstrdup("");

    tag =
	(hsa->format->type == PTOK_TAG
	    ? &hsa->format->u.tag :
	(hsa->format->type == PTOK_ARRAY
	    ? &hsa->format->u.array.format->u.tag :
	NULL));
    isxml = (tag != NULL && tag->tagno == -2 && tag->type != NULL && !strcmp(tag->type, "xml"));
    isyaml = (tag != NULL && tag->tagno == -2 && tag->type != NULL && !strcmp(tag->type, "yaml"));

    if (isxml) {
	need = sizeof("<rpmHeader>\n") - 1;
	t = hsaReserve(hsa, need);
	te = stpcpy(t, "<rpmHeader>\n");
	hsa->vallen += (te - t);
    }
    if (isyaml) {
	need = sizeof("- !!omap\n") - 1;
	t = hsaReserve(hsa, need);
	te = stpcpy(t, "- !!omap\n");
	hsa->vallen += (te - t);
    }

    hsa = hsaInit(hsa);
    while ((nextfmt = hsaNext(hsa)) != NULL) {
	te = singleSprintf(hsa, nextfmt, 0);
	if (te == NULL) {
	    hsa->val = _free(hsa->val);
	    break;
	}
    }
    hsa = hsaFini(hsa);

    if (isxml) {
	need = sizeof("</rpmHeader>\n") - 1;
	t = hsaReserve(hsa, need);
	te = stpcpy(t, "</rpmHeader>\n");
	hsa->vallen += (te - t);
    }
    if (isyaml) {
	need = sizeof("\n") - 1;
	t = hsaReserve(hsa, need);
	te = stpcpy(t, "\n");
	hsa->vallen += (te - t);
    }

    if (hsa->val != NULL && hsa->vallen < hsa->alloced)
	hsa->val = xrealloc(hsa->val, hsa->vallen+1);	

#if defined(SUPPORT_SPRINTF_EXTTABLE)
    hsa->ec = rpmecFree(hsa->exts, hsa->ec);
    hsa->nec = 0;
#endif	/* SUPPORT_SPRINTF_EXTTABLE */
#ifdef	NOTYET
    hsa->tc = rpmtcFree(hsa->tc, hsa->ntc);
    hsa->ntc = 0;
#endif
    hsa->format = freeFormat(hsa->format, hsa->numTokens);

exit:
/*@-dependenttrans -observertrans @*/
    if (errmsg)
	*errmsg = hsa->errmsg;
/*@=dependenttrans =observertrans @*/
    hsa->h = headerFree(hsa->h);
    hsa->fmt = _free(hsa->fmt);
    return hsa->val;
}

/** \ingroup header
 * Duplicate tag values from one header into another.
 * @param headerFrom	source header
 * @param headerTo	destination header
 * @param tagstocopy	array of tags that are copied
 */
static
void headerCopyTags(Header headerFrom, Header headerTo, hTAG_t tagstocopy)
	/*@modifies headerTo @*/
{
    int * p;

    if (headerFrom == headerTo)
	return;

    for (p = tagstocopy; *p != 0; p++) {
	rpmTagData ptr;
	rpmTagType type;
	rpmTagCount count;
	if (headerIsEntry(headerTo, *p))
	    continue;
	if (!headerGetEntryMinMemory(headerFrom, *p, &type, &ptr, &count))
	    continue;
	(void) headerAddEntry(headerTo, *p, type, ptr.ptr, count);
	ptr.ptr = headerFreeData(ptr.ptr, type);
    }
}

/*@observer@*/ /*@unchecked@*/
static struct HV_s hdrVec1 = {
    headerLink,
    headerUnlink,
    headerFree,
    headerNew,
    headerSort,
    headerUnsort,
    headerSizeof,
    headerUnload,
    headerReload,
    headerCopy,
    headerLoad,
    headerCopyLoad,
    headerRead,
    headerWrite,
    headerIsEntry,
    headerFreeTag,
    headerGetExtension,
    headerGetEntry,
    headerGetEntryMinMemory,
    headerAddEntry,
    headerAppendEntry,
    headerAddOrAppendEntry,
    headerAddI18NString,
    headerModifyEntry,
    headerRemoveEntry,
    headerSprintf,
    headerCopyTags,
    headerFreeIterator,
    headerInitIterator,
    headerNextIterator,
    headerGetMagic,
    headerSetMagic,
    headerGetOrigin,
    headerSetOrigin,
    headerGetInstance,
    headerSetInstance,
    headerGetStats,
    NULL, NULL,
    1
};

/*@-compmempass -redef@*/
/*@observer@*/ /*@unchecked@*/
HV_t hdrVec = &hdrVec1;
/*@=compmempass =redef@*/

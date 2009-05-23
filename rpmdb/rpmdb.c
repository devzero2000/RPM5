/** \ingroup rpmdb dbi
 * \file rpmdb/rpmdb.c
 */

#include "system.h"

#include <sys/file.h>

#include <rpmiotypes.h>
#include <rpmlog.h>
#include <rpmpgp.h>
#include <rpmurl.h>
#define	_MIRE_INTERNAL
#include <rpmmacro.h>
#include <rpmsq.h>
#include <argv.h>

#include <rpmtypes.h>

#define	_RPMTAG_INTERNAL
#include "header_internal.h"	/* XXX for HEADERFLAG_ALLOCATED */

#define	_RPMEVR_INTERNAL	/* XXX isInstallPrereq */
#include <rpmevr.h>

/* XXX avoid including <rpmts.h> */
/*@-redecl -type @*/
extern pgpDig rpmtsDig(void * ts)
        /*@*/;
extern void rpmtsCleanDig(void * ts)
        /*@modifies ts @*/;
/*@=redecl =type @*/

#define	_RPMDB_INTERNAL
#include "rpmdb.h"
#include "pkgio.h"
#include "fprint.h"
#include "legacy.h"

#include "debug.h"

#if defined(__LCLINT__)
#define	UINT32_T	u_int32_t
#else
#define	UINT32_T	rpmuint32_t
#endif

/* XXX retrofit the *BSD typedef for the deprived. */
#if defined(__QNXNTO__)
typedef	rpmuint32_t	u_int32_t;
#endif

/*@access dbiIndexSet@*/
/*@access dbiIndexItem@*/
/*@access miRE@*/
/*@access Header@*/		/* XXX compared with NULL */
/*@access rpmmi@*/
/*@access rpmts@*/		/* XXX compared with NULL */

/*@unchecked@*/
int _rpmdb_debug = 0;

/*@unchecked@*/
static int _rebuildinprogress = 0;
/*@unchecked@*/
static int _db_filter_dups = 0;

/* Use a path uniqifier in the upper 16 bits of tagNum? */
/* XXX Note: one cannot just choose a value, rpmdb tagNum's need fixing too */
#define	_DB_TAGGED_FILE_INDICES	1
/*@unchecked@*/
static int _db_tagged_file_indices = _DB_TAGGED_FILE_INDICES;

/* Use a path uniqifier while doing -qf? */
#define	_DB_TAGGED_FINDBYFILE	1
/*@unchecked@*/
static int _db_tagged_findbyfile = _DB_TAGGED_FINDBYFILE;

#define	_DBI_FLAGS	0
#define	_DBI_PERMS	0644
#define	_DBI_MAJOR	-1

/* Bit mask macros. */
/*@-exporttype@*/
typedef	unsigned int __pbm_bits;
/*@=exporttype@*/
#define	__PBM_NBITS		/*@-sizeoftype@*/(8 * sizeof(__pbm_bits))/*@=sizeoftype@*/
#define	__PBM_IX(d)		((d) / __PBM_NBITS)
#define __PBM_MASK(d)		((__pbm_bits) 1 << (((unsigned)(d)) % __PBM_NBITS))
/*@-exporttype@*/
typedef struct {
    __pbm_bits bits[1];
} pbm_set;
/*@=exporttype@*/
#define	__PBM_BITS(set)	((set)->bits)

#define	PBM_FREE(s)	_free(s);
#define PBM_SET(d, s)   (__PBM_BITS (s)[__PBM_IX (d)] |= __PBM_MASK (d))
#define PBM_CLR(d, s)   (__PBM_BITS (s)[__PBM_IX (d)] &= ~__PBM_MASK (d))
#define PBM_ISSET(d, s) ((__PBM_BITS (s)[__PBM_IX (d)] & __PBM_MASK (d)) != 0)

#define	PBM_ALLOC(d)	xcalloc(__PBM_IX (d) + 1, __PBM_NBITS/8)

/**
 * Reallocate a bit map.
 * @retval sp		address of bit map pointer
 * @retval odp		no. of bits in map
 * @param nd		desired no. of bits
 */
/*@unused@*/
static inline pbm_set * PBM_REALLOC(pbm_set ** sp, int * odp, int nd)
	/*@modifies *sp, *odp @*/
{
    int i, nb;

    if (nd > (*odp)) {
	nd *= 2;
	nb = __PBM_IX(nd) + 1;
/*@-unqualifiedtrans@*/
	*sp = xrealloc(*sp, nb * (__PBM_NBITS/8));
/*@=unqualifiedtrans@*/
	for (i = __PBM_IX(*odp) + 1; i < nb; i++)
	    __PBM_BITS(*sp)[i] = 0;
	*odp = nd;
    }
/*@-compdef -retalias -usereleased@*/
    return *sp;
/*@=compdef =retalias =usereleased@*/
}

/**
 * Convert hex to binary nibble.
 * @param c		hex character
 * @return		binary nibble
 */
static inline unsigned char nibble(char c)
	/*@*/
{
    if (c >= '0' && c <= '9')
	return (unsigned char)(c - '0');
    if (c >= 'A' && c <= 'F')
	return (unsigned char)((int)(c - 'A') + 10);
    if (c >= 'a' && c <= 'f')
	return (unsigned char)((int)(c - 'a') + 10);
    return '\0';
}

/**
 * Convert binary blob to printable hex string.
 * @param data		binary data
 * @param size		size of data in bytes
 * @return		malloc'd hex string
 */
/*@only@*/
static char * bin2hex(const void *data, size_t size)
	/*@*/
{
    static char hex[] = "0123456789abcdef";
    const char * s = data;
    char * t, * val;
    val = t = xmalloc(size * 2 + 1);
    while (size-- > 0) {
	unsigned i;
	i = (unsigned) *s++;
	*t++ = hex[ (i >> 4) & 0xf ];
	*t++ = hex[ (i     ) & 0xf ];
    }
    *t = '\0';

    return val;
}

#ifdef	DYING
/**
 * Check key for printable characters.
 * @param ptr		key value pointer
 * @param len		key value length
 * @return		1 if only ASCII, 0 otherwise.
 */
static int printable(const void * ptr, size_t len)	/*@*/
{
    const char * s = ptr;
    int i;
    for (i = 0; i < len; i++, s++)
	if (!(*s >= ' ' && *s <= '~')) return 0;
    return 1;
}
#endif

/**
 * Return dbi index used for rpm tag.
 * @param db		rpm database
 * @param tag		rpm tag
 * @return		dbi index, 0xffffffff on error
 */
static size_t dbiTagToDbix(rpmdb db, rpmTag tag)
	/*@*/
{
    size_t dbix;

    if (db->db_tags != NULL)
    for (dbix = 0; dbix < db->db_ndbi; dbix++) {
	if (tag != db->db_tags[dbix].tag)
	    continue;
	return dbix;
    }
    return 0xffffffff;
}

/**
 * Initialize database (index, tag) tuple from configuration.
 */
/*@-exportheader@*/
static void dbiTagsInit(/*@null@*/ tagStore_t * dbiTagsP,
		/*@null@*/ size_t * dbiNTagsP)
	/*@globals rpmGlobalMacroContext, h_errno, internalState @*/
	/*@modifies *dbiTagsP, *dbiNTagsP, rpmGlobalMacroContext, internalState @*/
{
/*@observer@*/
    static const char * const _dbiTagStr_default =
	"Packages:Name:Basenames:Group:Requirename:Providename:Conflictname:Triggername:Dirnames:Requireversion:Provideversion:Installtid:Sigmd5:Sha1header:Filemd5s:Depends:Pubkeys";
    tagStore_t dbiTags = NULL;
    size_t dbiNTags = 0;
    char * dbiTagStr = NULL;
    char * o, * oe;
    rpmTag tag;
    size_t dbix;
    int bingo;

    dbiTagStr = rpmExpand("%{?_dbi_tags}", NULL);
    if (!(dbiTagStr && *dbiTagStr)) {
	dbiTagStr = _free(dbiTagStr);
	dbiTagStr = xstrdup(_dbiTagStr_default);
    }

    /* Always allocate package index */
    dbiTags = xcalloc(1, sizeof(*dbiTags));
    dbiTags[dbiNTags].str = xstrdup("Packages");
    dbiTags[dbiNTags].tag = RPMDBI_PACKAGES;
    dbiTags[dbiNTags].iob = NULL;
    dbiNTags++;

    for (o = dbiTagStr; o && *o; o = oe) {
	while (*o && xisspace((int)*o))
	    o++;
	if (*o == '\0')
	    break;
	for (oe = o; oe && *oe; oe++) {
	    if (xisspace((int)*oe))
		/*@innerbreak@*/ break;
	    if (oe[0] == ':' && !(oe[1] == '/' && oe[2] == '/'))
		/*@innerbreak@*/ break;
	}
	if (oe && *oe)
	    *oe++ = '\0';
	tag = tagValue(o);

	bingo = 0;
	if (dbiTags != NULL)
	for (dbix = 0; dbix < dbiNTags; dbix++) {
	    if (tag == dbiTags[dbix].tag) {
		bingo = 1;
		/*@innerbreak@*/ break;
	    }
	}
	if (bingo)
	    continue;

	dbiTags = xrealloc(dbiTags, (dbiNTags + 1) * sizeof(*dbiTags));
	dbiTags[dbiNTags].str = xstrdup(o);
	dbiTags[dbiNTags].tag = tag;
	dbiTags[dbiNTags].iob = NULL;
	dbiNTags++;
    }

    if (dbiNTagsP != NULL)
	*dbiNTagsP = dbiNTags;
    if (dbiTagsP != NULL)
	*dbiTagsP = dbiTags;
    else
	dbiTags = tagStoreFree(dbiTags, dbiNTags);
    dbiTagStr = _free(dbiTagStr);
}
/*@=exportheader@*/

/*@-redecl@*/
#define	DB1vec		NULL
#define	DB2vec		NULL

#ifdef HAVE_DB_H
/*@-exportheadervar -declundef @*/
/*@observer@*/ /*@unchecked@*/
extern struct _dbiVec db3vec;
/*@=exportheadervar =declundef @*/
#define	DB3vec		&db3vec
/*@=redecl@*/
#else
#define DB3vec		NULL
#endif

#ifdef HAVE_SQLITE3_H
#define	SQLITE_HACK
/*@-exportheadervar -declundef @*/
/*@observer@*/ /*@unchecked@*/
extern struct _dbiVec sqlitevec;
/*@=exportheadervar =declundef @*/
#define	SQLITEvec	&sqlitevec
/*@=redecl@*/
#else
#define SQLITEvec	NULL
#endif

/*@-nullassign@*/
/*@observer@*/ /*@unchecked@*/
static struct _dbiVec *mydbvecs[] = {
    DB1vec, DB1vec, DB2vec, DB3vec, SQLITEvec, NULL
};
/*@=nullassign@*/

static inline int checkfd(const char * devnull, int fdno, int flags)
	/*@*/
{
    struct stat sb;
    int ret = 0;

    if (fstat(fdno, &sb) == -1 && errno == EBADF)
	ret = (open(devnull, flags) == fdno) ? 1 : 2;
    return ret;
}

dbiIndex dbiOpen(rpmdb db, rpmTag tag, /*@unused@*/ unsigned int flags)
{
    static int _oneshot = 0;
    size_t dbix;
    tagStore_t dbiTag;
    const char * dbiBN;
    dbiIndex dbi = NULL;
    int _dbapi, _dbapi_rebuild, _dbapi_wanted;
    int rc = 0;

    /* Insure that stdin/stdout/stderr are open, lest stderr end up in rpmdb. */
   if (!_oneshot) {
	static const char _devnull[] = "/dev/null";
/*@-noeffect@*/
#if defined(STDIN_FILENO)
	(void) checkfd(_devnull, STDIN_FILENO, O_RDONLY);
#endif
#if defined(STDOUT_FILENO)
	(void) checkfd(_devnull, STDOUT_FILENO, O_WRONLY);
#endif
#if defined(STDERR_FILENO)
	(void) checkfd(_devnull, STDERR_FILENO, O_WRONLY);
#endif
/*@=noeffect@*/
	_oneshot++;
   }

/*@-modfilesys@*/
if (_rpmdb_debug)
fprintf(stderr, "==> dbiOpen(%p, %s(%u), 0x%x)\n", db, tagName(tag), tag, flags);
/*@=modfilesys@*/

    if (db == NULL)
	return NULL;

    dbix = dbiTagToDbix(db, tag);
    if (dbix >= db->db_ndbi)
	return NULL;
    dbiTag = db->db_tags + dbix;
    dbiBN = (dbiTag->str != NULL ? dbiTag->str : tagName(tag));

    /* Is this index already open ? */
/*@-compdef@*/ /* FIX: db->_dbi may be NULL */
    if (db->_dbi != NULL && (dbi = db->_dbi[dbix]) != NULL)
	return dbi;
/*@=compdef@*/

    _dbapi_rebuild = rpmExpandNumeric("%{_dbapi_rebuild}");
    if (_dbapi_rebuild < 1 || _dbapi_rebuild > 4)
	_dbapi_rebuild = 4;
/*    _dbapi_wanted = (_rebuildinprogress ? -1 : db->db_api); */
    _dbapi_wanted = (_rebuildinprogress ? _dbapi_rebuild : db->db_api);

    switch (_dbapi_wanted) {
    default:
	_dbapi = _dbapi_wanted;
	if (_dbapi < 0 || _dbapi >= 5 || mydbvecs[_dbapi] == NULL) {
            rpmlog(RPMLOG_DEBUG, D_("dbiOpen: _dbiapi failed\n"));
	    return NULL;
	}
	errno = 0;
	dbi = NULL;
	rc = (*mydbvecs[_dbapi]->open) (db, tag, &dbi);
	if (rc) {
	    static int _printed[32];
	    if (!_printed[dbix & 0x1f]++)
		rpmlog(RPMLOG_ERR,
			_("cannot open %s(%u) index using db%d - %s (%d)\n"),
			dbiBN, tag, _dbapi,
			(rc > 0 ? strerror(rc) : ""), rc);
	    _dbapi = -1;
	}
	break;
    case -1:
	_dbapi = 5;
	while (_dbapi-- > 1) {
	    if (mydbvecs[_dbapi] == NULL)
		continue;
	    errno = 0;
	    dbi = NULL;
	    rc = (*mydbvecs[_dbapi]->open) (db, tag, &dbi);
	    if (rc == 0 && dbi)
		/*@loopbreak@*/ break;
	}
	if (_dbapi <= 0) {
	    static int _printed[32];
	    if (!_printed[dbix & 0x1f]++)
		rpmlog(RPMLOG_ERR, _("cannot open %s(%u) index\n"),
			dbiBN, tag);
	    rc = 1;
	    goto exit;
	}
	if (db->db_api == -1 && _dbapi > 0)
	    db->db_api = _dbapi;
    	break;
    }

exit:
    if (dbi != NULL && rc == 0) {
	if (db->_dbi != NULL)
	    db->_dbi[dbix] = dbi;
/*@-sizeoftype@*/
	if (tag == RPMDBI_PACKAGES && db->db_bits == NULL) {
	    db->db_nbits = 1024;
	    if (!dbiStat(dbi, DB_FAST_STAT)) {
		DB_HASH_STAT * hash = (DB_HASH_STAT *)dbi->dbi_stats;
		if (hash)
		    db->db_nbits += hash->hash_nkeys;
	    }
	    db->db_bits = PBM_ALLOC(db->db_nbits);
	}
/*@=sizeoftype@*/
    }
#ifdef HAVE_DB_H
      else
	dbi = db3Free(dbi);
#endif

/*@-compdef -nullstate@*/ /* FIX: db->_dbi may be NULL */
    return dbi;
/*@=compdef =nullstate@*/
}

/**
 * Create and initialize item for index database set.
 * @param hdrNum	header instance in db
 * @param tagNum	tag index in header
 * @return		new item
 */
static dbiIndexItem dbiIndexNewItem(unsigned int hdrNum, unsigned int tagNum)
	/*@*/
{
    dbiIndexItem rec = xcalloc(1, sizeof(*rec));
    rec->hdrNum = hdrNum;
    rec->tagNum = tagNum;
    return rec;
}

union _dbswap {
    rpmuint32_t ui;
    unsigned char uc[4];
};

#define	_DBSWAP(_a) \
  { unsigned char _b, *_c = (_a).uc; \
    _b = _c[3]; _c[3] = _c[0]; _c[0] = _b; \
    _b = _c[2]; _c[2] = _c[1]; _c[1] = _b; \
  }

/**
 * Convert retrieved data to index set.
 * @param dbi		index database handle
 * @param data		retrieved data
 * @retval setp		(malloc'ed) index set
 * @return		0 on success
 */
static int dbt2set(dbiIndex dbi, DBT * data, /*@out@*/ dbiIndexSet * setp)
	/*@modifies dbi, *setp @*/
{
    int _dbbyteswapped;
    const char * sdbir;
    dbiIndexSet set;
    int i;

    if (dbi == NULL || data == NULL || setp == NULL)
	return -1;
    _dbbyteswapped = dbiByteSwapped(dbi);

    if ((sdbir = data->data) == NULL) {
	*setp = NULL;
	return 0;
    }

    set = xmalloc(sizeof(*set));
    set->count = (int) (data->size / dbi->dbi_jlen);
    set->recs = xmalloc(set->count * sizeof(*(set->recs)));

/*@-sizeoftype @*/
    switch (dbi->dbi_jlen) {
    default:
    case 2*sizeof(rpmuint32_t):
	for (i = 0; i < set->count; i++) {
	    union _dbswap hdrNum, tagNum;

	    memcpy(&hdrNum.ui, sdbir, sizeof(hdrNum.ui));
	    sdbir += sizeof(hdrNum.ui);
	    memcpy(&tagNum.ui, sdbir, sizeof(tagNum.ui));
	    sdbir += sizeof(tagNum.ui);
	    if (_dbbyteswapped) {
		_DBSWAP(hdrNum);
		_DBSWAP(tagNum);
	    }
	    set->recs[i].hdrNum = hdrNum.ui;
	    set->recs[i].tagNum = tagNum.ui;
	    set->recs[i].fpNum = 0;
	}
	break;
    case 1*sizeof(rpmuint32_t):
	for (i = 0; i < set->count; i++) {
	    union _dbswap hdrNum;

	    memcpy(&hdrNum.ui, sdbir, sizeof(hdrNum.ui));
	    sdbir += sizeof(hdrNum.ui);
	    if (_dbbyteswapped) {
		_DBSWAP(hdrNum);
	    }
	    set->recs[i].hdrNum = hdrNum.ui;
	    set->recs[i].tagNum = 0;
	    set->recs[i].fpNum = 0;
	}
	break;
    }
    *setp = set;
/*@=sizeoftype @*/
/*@-compdef@*/
    return 0;
/*@=compdef@*/
}

/**
 * Convert index set to database representation.
 * @param dbi		index database handle
 * @param data		retrieved data
 * @param set		index set
 * @return		0 on success
 */
static int set2dbt(dbiIndex dbi, DBT * data, dbiIndexSet set)
	/*@modifies dbi, *data @*/
{
    int _dbbyteswapped;
    char * tdbir;
    unsigned i;

    if (dbi == NULL || data == NULL || set == NULL)
	return -1;
    _dbbyteswapped = dbiByteSwapped(dbi);

    data->size = (UINT32_T)(set->count * (dbi->dbi_jlen));
    if (data->size == 0) {
	data->data = NULL;
	return 0;
    }
    tdbir = data->data = xmalloc(data->size);

/*@-sizeoftype@*/
    switch (dbi->dbi_jlen) {
    default:
    case 2*sizeof(rpmuint32_t):
	for (i = 0; i < (unsigned)set->count; i++) {
	    union _dbswap hdrNum, tagNum;

	    memset(&hdrNum, 0, sizeof(hdrNum));
	    memset(&tagNum, 0, sizeof(tagNum));
	    hdrNum.ui = set->recs[i].hdrNum;
	    tagNum.ui = set->recs[i].tagNum;
	    if (_dbbyteswapped) {
		_DBSWAP(hdrNum);
		_DBSWAP(tagNum);
	    }
	    memcpy(tdbir, &hdrNum.ui, sizeof(hdrNum.ui));
	    tdbir += sizeof(hdrNum.ui);
	    memcpy(tdbir, &tagNum.ui, sizeof(tagNum.ui));
	    tdbir += sizeof(tagNum.ui);
	}
	break;
    case 1*sizeof(rpmuint32_t):
	for (i = 0; i < (unsigned)set->count; i++) {
	    union _dbswap hdrNum;

	    memset(&hdrNum, 0, sizeof(hdrNum));
	    hdrNum.ui = set->recs[i].hdrNum;
	    if (_dbbyteswapped) {
		_DBSWAP(hdrNum);
	    }
	    memcpy(tdbir, &hdrNum.ui, sizeof(hdrNum.ui));
	    tdbir += sizeof(hdrNum.ui);
	}
	break;
    }
/*@=sizeoftype@*/

/*@-compdef@*/
    return 0;
/*@=compdef@*/
}

/* XXX assumes hdrNum is first int in dbiIndexItem */
static int hdrNumCmp(const void * one, const void * two)
	/*@*/
{
    const int * a = one, * b = two;
    return (*a - *b);
}

/**
 * Append element(s) to set of index database items.
 * @param set		set of index database items
 * @param recs		array of items to append to set
 * @param nrecs		number of items
 * @param recsize	size of an array item
 * @param sortset	should resulting set be sorted?
 * @return		0 success, 1 failure (bad args)
 */
static int dbiAppendSet(dbiIndexSet set, const void * recs,
	int nrecs, size_t recsize, int sortset)
	/*@modifies *set @*/
{
    const char * rptr = recs;
    size_t rlen = (recsize < sizeof(*(set->recs)))
		? recsize : sizeof(*(set->recs));

    if (set == NULL || recs == NULL || nrecs <= 0 || recsize == 0)
	return 1;

    set->recs = xrealloc(set->recs,
			(set->count + nrecs) * sizeof(*(set->recs)));

    memset(set->recs + set->count, 0, nrecs * sizeof(*(set->recs)));

    while (nrecs-- > 0) {
	/*@-mayaliasunique@*/
	memcpy(set->recs + set->count, rptr, rlen);
	/*@=mayaliasunique@*/
	rptr += recsize;
	set->count++;
    }

    if (sortset && set->count > 1)
	qsort(set->recs, set->count, sizeof(*(set->recs)), hdrNumCmp);

    return 0;
}

/**
 * Remove element(s) from set of index database items.
 * @param set		set of index database items
 * @param recs		array of items to remove from set
 * @param nrecs		number of items
 * @param recsize	size of an array item
 * @param sorted	array is already sorted?
 * @return		0 success, 1 failure (no items found)
 */
static int dbiPruneSet(dbiIndexSet set, void * recs, int nrecs,
		size_t recsize, int sorted)
	/*@modifies set, recs @*/
{
    int from;
    int to = 0;
    int num = set->count;
    int numCopied = 0;

assert(set->count > 0);
    if (nrecs > 1 && !sorted)
	qsort(recs, nrecs, recsize, hdrNumCmp);

    for (from = 0; from < num; from++) {
	if (bsearch(&set->recs[from], recs, nrecs, recsize, hdrNumCmp)) {
	    set->count--;
	    continue;
	}
	if (from != to)
	    set->recs[to] = set->recs[from]; /* structure assignment */
	to++;
	numCopied++;
    }
    return (numCopied == num);
}

/* XXX transaction.c */
unsigned int dbiIndexSetCount(dbiIndexSet set) {
    return set->count;
}

/* XXX transaction.c */
unsigned int dbiIndexRecordOffset(dbiIndexSet set, int recno) {
    return (unsigned) set->recs[recno].hdrNum;
}

/* XXX transaction.c */
unsigned int dbiIndexRecordFileNumber(dbiIndexSet set, int recno) {
    return (unsigned) set->recs[recno].tagNum;
}

/* XXX transaction.c */
dbiIndexSet dbiFreeIndexSet(dbiIndexSet set) {
    if (set) {
	set->recs = _free(set->recs);
	set = _free(set);
    }
    return set;
}

struct rpmmi_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
/*@dependent@*/ /*@null@*/
    rpmmi		mi_next;
/*@refcounted@*/
    rpmdb		mi_db;
    rpmTag		mi_rpmtag;
    dbiIndexSet		mi_set;
    DBC *		mi_dbc;
    DBT			mi_key;
    DBT			mi_data;
    int			mi_setx;
/*@refcounted@*/ /*@null@*/
    Header		mi_h;
    int			mi_sorted;
    int			mi_cflags;
    int			mi_modified;
    unsigned int	mi_prevoffset;	/* header instance (native endian) */
    unsigned int	mi_offset;	/* header instance (native endian) */
    unsigned int	mi_filenum;	/* tag element (native endian) */
    int			mi_nre;
/*@only@*/ /*@null@*/
    miRE		mi_re;
/*@null@*/
    rpmts		mi_ts;

};

/*@unchecked@*/
static rpmdb rpmdbRock;

/*@unchecked@*/ /*@exposed@*/ /*@null@*/
static rpmmi rpmmiRock;

int rpmdbCheckTerminate(int terminate)
	/*@globals rpmdbRock, rpmmiRock @*/
	/*@modifies rpmdbRock, rpmmiRock @*/
{
    sigset_t newMask, oldMask;
    static int terminating = 0;

    if (terminating) return 1;

    (void) sigfillset(&newMask);		/* block all signals */
    (void) sigprocmask(SIG_BLOCK, &newMask, &oldMask);

    if (sigismember(&rpmsqCaught, SIGINT)
     || sigismember(&rpmsqCaught, SIGQUIT)
     || sigismember(&rpmsqCaught, SIGHUP)
     || sigismember(&rpmsqCaught, SIGTERM)
     || sigismember(&rpmsqCaught, SIGPIPE)
#ifdef	NOTYET		/* XXX todo++ */
     || sigismember(&rpmsqCaught, SIGXCPU)
     || sigismember(&rpmsqCaught, SIGXFSZ)
#endif
     || terminate)
	terminating = 1;

    if (terminating) {
	rpmdb db;
	rpmmi mi;

	while ((mi = rpmmiRock) != NULL) {
/*@i@*/	    rpmmiRock = mi->mi_next;
	    mi->mi_next = NULL;
/*@i@*/	    mi = rpmmiFree(mi);
	}

/*@-newreftrans@*/
	while ((db = rpmdbRock) != NULL) {
/*@i@*/	    rpmdbRock = db->db_next;
	    db->db_next = NULL;
	    (void) rpmdbClose(db);
	}
/*@=newreftrans@*/
    }

    (void) sigprocmask(SIG_SETMASK, &oldMask, NULL);
    return terminating;
}

int rpmdbCheckSignals(void)
{

    if (rpmdbCheckTerminate(0)) {
/*@-abstract@*/ /* sigset_t is abstract type */
	rpmlog(RPMLOG_DEBUG, D_("Exiting on signal(0x%lx) ...\n"), *((unsigned long *)&rpmsqCaught));
/*@=abstract@*/
	exit(EXIT_FAILURE);
    }
    return 0;
}

/**
 * Block all signals, returning previous signal mask.
 * @param db		rpm database
 * @retval *oldMask	previous sigset
 * @return		0 on success
 */
static int blockSignals(/*@unused@*/ rpmdb db, /*@out@*/ sigset_t * oldMask)
	/*@globals fileSystem @*/
	/*@modifies *oldMask, fileSystem @*/
{
    sigset_t newMask;

    (void) sigfillset(&newMask);		/* block all signals */
    (void) sigprocmask(SIG_BLOCK, &newMask, oldMask);
    (void) sigdelset(&newMask, SIGINT);
    (void) sigdelset(&newMask, SIGQUIT);
    (void) sigdelset(&newMask, SIGHUP);
    (void) sigdelset(&newMask, SIGTERM);
    (void) sigdelset(&newMask, SIGPIPE);
    return sigprocmask(SIG_BLOCK, &newMask, NULL);
}

/**
 * Restore signal mask.
 * @param db		rpm database
 * @param oldMask	previous sigset
 * @return		0 on success
 */
/*@mayexit@*/
static int unblockSignals(/*@unused@*/ rpmdb db, sigset_t * oldMask)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    (void) rpmdbCheckSignals();
    return sigprocmask(SIG_SETMASK, oldMask, NULL);
}

/**
 * Return header query string.
 * @warning Only compound header extensions are available here.
 * @param h		header
 * @param qfmt		header sprintf format
 * @return		header query string
 */
static inline /*@null@*/ const char * queryHeader(Header h, const char * qfmt)
	/*@globals headerCompoundFormats, fileSystem, internalState @*/
	/*@modifies h, fileSystem, internalState @*/
{
    const char * errstr = "(unkown error)";
    const char * str;

/*@-modobserver@*/
    str = headerSprintf(h, qfmt, NULL, headerCompoundFormats, &errstr);
/*@=modobserver@*/
    if (str == NULL)
	rpmlog(RPMLOG_ERR, _("incorrect format: \"%s\": %s\n"), qfmt, errstr);
    return str;
}

/**
 * Write added/removed header info.
 * @param db		rpm database
 * @param h		header
 * @param adding	adding an rpmdb header?
 * @return		0 on success
 */
static int rpmdbExportInfo(/*@unused@*/ rpmdb db, Header h, int adding)
	/*@globals headerCompoundFormats, rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies h, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    const char * fn = NULL;
    int xx;

    {	const char * fnfmt = rpmGetPath("%{?_hrmib_path}", NULL);
	if (fnfmt && *fnfmt)
	    fn = queryHeader(h, fnfmt);
	fnfmt = _free(fnfmt);
    }

    if (fn == NULL)
	goto exit;

    if (adding) {
	FD_t fd = Fopen(fn, "w.fdio");

	if (fd != NULL) {
	    xx = Fclose(fd);
	    fd = NULL;
	    he->tag = RPMTAG_INSTALLTID;
	    if (headerGet(h, he, 0)) {
		struct utimbuf stamp;
		stamp.actime = he->p.ui32p[0];
		stamp.modtime = he->p.ui32p[0];
		if (!Utime(fn, &stamp))
		    rpmlog(RPMLOG_DEBUG, "  +++ %s\n", fn);
	    }
	    he->p.ptr = _free(he->p.ptr);
	}
    } else {
	if (!Unlink(fn))
	    rpmlog(RPMLOG_DEBUG, "  --- %s\n", fn);
    }

exit:
    fn = _free(fn);
    return 0;
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmdbPool;

static rpmdb rpmdbGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmdbPool, fileSystem @*/
	/*@modifies pool, _rpmdbPool, fileSystem @*/
{
    rpmdb db;

    if (_rpmdbPool == NULL) {
	_rpmdbPool = rpmioNewPool("db", sizeof(*db), -1, _rpmdb_debug,
			NULL, NULL, NULL);
	pool = _rpmdbPool;
    }
    return (rpmdb) rpmioGetPool(pool, sizeof(*db));
}

int rpmdbOpenAll(rpmdb db)
{
    size_t dbix;
    int rc = 0;

    if (db == NULL) return -2;

    if (db->db_tags != NULL && db->_dbi != NULL)
    for (dbix = 0; dbix < db->db_ndbi; dbix++) {
	tagStore_t dbiTag = db->db_tags + dbix;
	int tag = dbiTag->tag;
	if (tag < 0)
	    continue;
	if (db->_dbi[dbix] != NULL)
	    continue;
	switch (tag) {
	case RPMDBI_AVAILABLE:
	case RPMDBI_ADDED:
	case RPMDBI_REMOVED:
	case RPMDBI_DEPENDS:
	    continue;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	default:
	    /*@switchbreak@*/ break;
	}
	(void) dbiOpen(db, tag, db->db_flags);
    }
    return rc;
}

int rpmdbBlockDBI(rpmdb db, int tag)
{
    rpmTag tagn = (rpmTag)(tag >= 0 ? tag : -tag);
    size_t dbix;

    if (db == NULL || db->_dbi == NULL)
	return 0;

    if (db->db_tags != NULL)
    for (dbix = 0; dbix < db->db_ndbi; dbix++) {
	if (db->db_tags[dbix].tag != tagn)
	    continue;
	db->db_tags[dbix].tag = tag;
	return 0;
    }
    return 0;
}

int rpmdbCloseDBI(rpmdb db, int tag)
{
    size_t dbix;
    int rc = 0;

    if (db == NULL || db->_dbi == NULL)
	return 0;

    if (db->db_tags != NULL)
    for (dbix = 0; dbix < db->db_ndbi; dbix++) {
	if (db->db_tags[dbix].tag != (rpmTag)tag)
	    continue;
	if (db->_dbi[dbix] != NULL) {
	    int xx;
	    /*@-unqualifiedtrans@*/		/* FIX: double indirection. */
	    xx = dbiClose(db->_dbi[dbix], 0);
	    if (xx && rc == 0) rc = xx;
	    db->_dbi[dbix] = NULL;
	    /*@=unqualifiedtrans@*/
	}
	break;
    }
    return rc;
}

/* XXX query.c, rpminstall.c, verify.c */
/*@-incondefs@*/
int rpmdbClose(rpmdb db)
	/*@globals rpmdbRock @*/
	/*@modifies rpmdbRock @*/
{
    static const char msg[] = "rpmdbClose";
    rpmdb * prev, next;
    size_t dbix;
    int rc = 0;

    if (db == NULL)
	return rc;

    yarnPossess(db->_item.use);
/*@-modfilesys@*/
if (_rpmdb_debug)
fprintf(stderr, "--> db %p -- %ld %s at %s:%u\n", db, yarnPeekLock(db->_item.use), msg, __FILE__, __LINE__);

    /*@-usereleased@*/
    if (yarnPeekLock(db->_item.use) <= 1L) {

	if (db->_dbi)
	for (dbix = db->db_ndbi; dbix;) {
	    int xx;
	    dbix--;
	    if (db->_dbi[dbix] == NULL)
		continue;
	    /*@-unqualifiedtrans@*/		/* FIX: double indirection. */
	    xx = dbiClose(db->_dbi[dbix], 0);
	    if (xx && rc == 0) rc = xx;
	    db->_dbi[dbix] = NULL;
	    /*@=unqualifiedtrans@*/
	}
	db->db_errpfx = _free(db->db_errpfx);
	db->db_root = _free(db->db_root);
	db->db_home = _free(db->db_home);
	db->db_bits = PBM_FREE(db->db_bits);
	db->db_tags = tagStoreFree(db->db_tags, db->db_ndbi);
	db->_dbi = _free(db->_dbi);
	db->db_ndbi = 0;

/*@-newreftrans@*/
	prev = &rpmdbRock;
	while ((next = *prev) != NULL && next != db)
	    prev = &next->db_next;
	if (next) {
/*@i@*/	    *prev = next->db_next;
	    next->db_next = NULL;
	}
/*@=newreftrans@*/

	if (rpmdbRock == NULL && rpmmiRock == NULL) {
	    /* Last close uninstalls special signal handling. */
	    (void) rpmsqEnable(-SIGHUP,	NULL);
	    (void) rpmsqEnable(-SIGINT,	NULL);
	    (void) rpmsqEnable(-SIGTERM,	NULL);
	    (void) rpmsqEnable(-SIGQUIT,	NULL);
	    (void) rpmsqEnable(-SIGPIPE,	NULL);
	    /* Pending signals strike here. */
	    (void) rpmdbCheckSignals();
	}

    /*@=usereleased@*/
	db = (rpmdb)rpmioPutPool((rpmioItem)db);
    } else
	yarnTwist(db->_item.use, BY, -1);

    return rc;
}
/*@=incondefs@*/

int rpmdbSync(rpmdb db)
{
    size_t dbix;
    int rc = 0;

    if (db == NULL) return 0;
    if (db->_dbi != NULL)
    for (dbix = 0; dbix < db->db_ndbi; dbix++) {
	int xx;
	if (db->_dbi[dbix] == NULL)
	    continue;
	if (db->_dbi[dbix]->dbi_no_dbsync)
 	    continue;
    	xx = dbiSync(db->_dbi[dbix], 0);
	if (xx && rc == 0) rc = xx;
    }
    return rc;
}

/**
 * Return macro expanded absolute path to rpmdb.
 * @param uri		desired path
 * @return		macro expanded absolute path
 */
static const char * rpmdbURIPath(const char *uri)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext, fileSystem, internalState @*/
{
    const char * s = rpmGetPath(uri, NULL);
    const char * fn = NULL;
    urltype ut = urlPath(s, &fn);

    switch (ut) {
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
	fn = s;
	s = NULL;
	break;
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    case URL_IS_FTP:
    case URL_IS_HKP:
    case URL_IS_DASH:
    default:
	/* HACK: strip the URI prefix for these schemes. */
	fn = rpmGetPath(fn, NULL);
	break;
    }

    /* Convert relative to absolute paths. */
    if (ut != URL_IS_PATH)	/* XXX permit file:///... URI's */
    if (fn && *fn && *fn != '/') {
	char dn[PATH_MAX];
	char *t;
	dn[0] = '\0';
	if ((t = Realpath(".", dn)) != NULL) {
	    t += strlen(dn);
	    if (t > dn && t[-1] != '/')
		*t++ = '/';
	    t = stpncpy(t, fn, (sizeof(dn) - (t - dn)));
	    *t = '\0';
	    fn = _free(fn);
	    fn = rpmGetPath(dn, NULL);
	}
    }

    s = _free(s);
assert(fn != NULL);
    return fn;
}

#define	_DB_ROOT	"/"
#define	_DB_HOME	"%{?_dbpath}"
#define	_DB_FLAGS	0
#define _DB_MODE	0
#define _DB_PERMS	0644

#define _DB_MAJOR	-1
#define	_DB_ERRPFX	"rpmdb"

/*@-exportheader -globs -mods @*/
/*@only@*/ /*@null@*/
rpmdb rpmdbNew(/*@kept@*/ /*@null@*/ const char * root,
		/*@kept@*/ /*@null@*/ const char * home,
		int mode, int perms, int flags)
	/*@globals _db_filter_dups @*/
	/*@modifies _db_filter_dups @*/
{
    rpmdb db = rpmdbGetPool(_rpmdbPool);
    const char * epfx = _DB_ERRPFX;
    static int oneshot = 0;

/*@-modfilesys@*/ /*@-nullpass@*/
if (_rpmdb_debug)
fprintf(stderr, "==> rpmdbNew(%s, %s, 0x%x, 0%o, 0x%x) db %p\n", root, home, mode, perms, flags, db);
/*@=modfilesys@*/ /*@=nullpass@*/

    if (!oneshot) {
	_db_filter_dups = rpmExpandNumeric("%{?_filterdbdups}");
	oneshot = 1;
    }

    db->db_api = _DB_MAJOR;

    db->_dbi = NULL;

    if (!(perms & 0600)) perms = 0644;	/* XXX sanity */

    db->db_mode = (mode >= 0) ? mode : _DB_MODE;
    db->db_perms = (perms >= 0)	? perms : _DB_PERMS;
    db->db_flags = (flags >= 0) ? flags : _DB_FLAGS;

    db->db_root = rpmdbURIPath( (root && *root ? root : _DB_ROOT) );
    db->db_home = rpmdbURIPath( (home && *home ? home : _DB_HOME) );

    if (!(db->db_home && db->db_home[0] && db->db_home[0] != '%')) {
	rpmlog(RPMLOG_ERR, _("no dbpath has been set\n"));
	db->db_root = _free(db->db_root);
	db->db_home = _free(db->db_home);
	db = (rpmdb) rpmioPutPool((rpmioItem)db);
	/*@-globstate@*/ return NULL; /*@=globstate@*/
    }

    db->db_export = rpmdbExportInfo;
    db->db_errpfx = rpmExpand( (epfx && *epfx ? epfx : _DB_ERRPFX), NULL);
    db->db_remove_env = 0;
    db->db_filter_dups = _db_filter_dups;
    dbiTagsInit(&db->db_tags, &db->db_ndbi);
    db->_dbi = xcalloc(db->db_ndbi, sizeof(*db->_dbi));
    /*@-globstate@*/
    return rpmdbLink(db, "rpmdbNew");
    /*@=globstate@*/
}
/*@=exportheader =globs =mods @*/

/*@-exportheader@*/
int rpmdbOpenDatabase(/*@null@*/ const char * prefix,
		/*@null@*/ const char * dbpath,
		int _dbapi, /*@null@*/ /*@out@*/ rpmdb *dbp,
		int mode, int perms, int flags)
	/*@globals rpmdbRock, rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies rpmdbRock, *dbp, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    rpmdb db;
    int rc, xx;
    int justCheck = flags & RPMDB_FLAG_JUSTCHECK;
    int minimal = flags & RPMDB_FLAG_MINIMAL;

    /* Insure that _dbapi has one of -1, 1, 2, or 3 */
    if (_dbapi < -1 || _dbapi > 4)
	_dbapi = -1;
    if (_dbapi == 0)
	_dbapi = 1;

    if (dbp)
	*dbp = NULL;
    if (mode & O_WRONLY) 
	return 1;

    db = rpmdbNew(prefix, dbpath, mode, perms, flags);
    if (db == NULL)
	return 1;

    if (rpmdbRock == NULL && rpmmiRock == NULL) {
	/* First open installs special signal handling. */
	(void) rpmsqEnable(SIGHUP,	NULL);
	(void) rpmsqEnable(SIGINT,	NULL);
	(void) rpmsqEnable(SIGTERM,	NULL);
	(void) rpmsqEnable(SIGQUIT,	NULL);
	(void) rpmsqEnable(SIGPIPE,	NULL);
    }

/*@-assignexpose -newreftrans@*/
/*@i@*/	db->db_next = rpmdbRock;
	rpmdbRock = db;
/*@=assignexpose =newreftrans@*/

    db->db_api = _dbapi;

    {	size_t dbix;

	rc = 0;
	if (db->db_tags != NULL)
	for (dbix = 0; rc == 0 && dbix < db->db_ndbi; dbix++) {
	    tagStore_t dbiTag = db->db_tags + dbix;
	    rpmTag tag = dbiTag->tag;
	    dbiIndex dbi;

	    /* Filter out temporary databases */
	    switch (tag) {
	    case RPMDBI_AVAILABLE:
	    case RPMDBI_ADDED:
	    case RPMDBI_REMOVED:
	    case RPMDBI_DEPENDS:
		continue;
		/*@notreached@*/ /*@switchbreak@*/ break;
	    default:
		/*@switchbreak@*/ break;
	    }

	    dbi = dbiOpen(db, tag, 0);
	    if (dbi == NULL) {
		rc = -2;
		break;
	    }

	    switch (tag) {
	    case RPMDBI_PACKAGES:
		if (dbi == NULL) rc |= 1;
#if 0
		/* XXX open only Packages, indices created on the fly. */
		if (db->db_api == 3)
#endif
		    goto exit;
		/*@notreached@*/ /*@switchbreak@*/ break;
	    case RPMTAG_NAME:
		if (dbi == NULL) rc |= 1;
		if (minimal)
		    goto exit;
		/*@switchbreak@*/ break;
	    default:
		/*@switchbreak@*/ break;
	    }
	}
    }

exit:
    if (rc || justCheck || dbp == NULL)
	xx = rpmdbClose(db);
    else {
/*@-assignexpose -newreftrans@*/
/*@i@*/	*dbp = db;
/*@=assignexpose =newreftrans@*/
    }

    return rc;
}
/*@=exportheader@*/

/* XXX python/rpmmodule.c */
int rpmdbOpen (const char * prefix, rpmdb *dbp, int mode, int perms)
{
    int _dbapi = rpmExpandNumeric("%{?_dbapi}");
    return rpmdbOpenDatabase(prefix, NULL, _dbapi, dbp, mode, perms, 0);
}

int rpmdbInit (const char * prefix, int perms)
{
    int rc = -1;	/* RPMRC_NOTFOUND somewhen */
#ifdef	SUPPORT_INITDB
    rpmdb db = NULL;
    int _dbapi = rpmExpandNumeric("%{_dbapi}");

    rc = rpmdbOpenDatabase(prefix, NULL, _dbapi, &db, (O_CREAT | O_RDWR),
		perms, RPMDB_FLAG_JUSTCHECK);
    if (db != NULL) {
	int xx;
	xx = rpmdbOpenAll(db);
	if (xx && rc == 0) rc = xx;
	xx = rpmdbClose(db);
	if (xx && rc == 0) rc = xx;
	db = NULL;
    }
#endif
    return rc;
}

int rpmdbVerifyAllDBI(rpmdb db)
{
    int rc = -1;	/* RPMRC_NOTFOUND somewhen */

#if defined(SUPPORT_VERIFYDB)
    if (db != NULL) {
	size_t dbix;
	int xx;
	rc = rpmdbOpenAll(db);

	if (db->_dbi != NULL)
	for (dbix = db->db_ndbi; dbix;) {
	    dbix--;
	    if (db->_dbi[dbix] == NULL)
		continue;
	    /*@-unqualifiedtrans@*/		/* FIX: double indirection. */
	    xx = dbiVerify(db->_dbi[dbix], 0);
	    if (xx && rc == 0) rc = xx;
	    db->_dbi[dbix] = NULL;
	    /*@=unqualifiedtrans@*/
	}

	/*@-nullstate@*/	/* FIX: db->_dbi[] may be NULL. */
	xx = rpmdbClose(db);
	/*@=nullstate@*/
	if (xx && rc == 0) rc = xx;
	db = NULL;
    }
#endif
    return rc;
}

int rpmdbVerify(const char * prefix)
{
    int rc = -1;	/* RPMRC_NOTFOUND somewhen */
#if defined(SUPPORT_VERIFYDB)
    rpmdb db = NULL;
    int _dbapi = rpmExpandNumeric("%{_dbapi}");

    rc = rpmdbOpenDatabase(prefix, NULL, _dbapi, &db, O_RDONLY, 0644, 0);
    if (!rc && db != NULL)
	rc = rpmdbVerifyAllDBI(db);
#endif
    return rc;
}

/**
 * Return a tagnum with hash on the (directory) path in upper 16 bits.
 * @param s		(directory) path
 * @return		tagnum with (directory) path hash
 */
static inline unsigned taghash(const char * s)
	/*@*/
{
    unsigned int r = 0;
    int c;
    while ((c = (int) *s++) != 0) {
	/* XXX Excluding the '/' character may cause hash collisions. */
	if (c != (int) '/')
	    r += (r << 3) + c;
    }
    return ((r & 0x7fff) | 0x8000) << 16;
}

/**
 * Return the intersection of dirName <=> baseName index sets.
 * @param tag		dirName hash tag.
 * @param dnset		dirName's set.
 * @baram bnset		baseName's set.
 * @retval *matches	intersection of dnset and bnset (NULL if disjoint).
 * @return		no. of matches
 */
static int dbiIntersect(unsigned int tag, dbiIndexSet dnset, dbiIndexSet bnset,
			dbiIndexSet *matches)
	/*@modifies *matches @*/
{
    dbiIndexItem drec = dnset->recs;
    dbiIndexItem brec = bnset->recs;
    dbiIndexItem rec = alloca(sizeof(*rec));
    int i = 0;
    int j = 0;
    int xx;

    *matches = NULL;
    while (i < dnset->count) {
	while (j < bnset->count && brec->hdrNum <= drec->hdrNum) {
	    if (brec->hdrNum == drec->hdrNum
	     && tag == (brec->tagNum & 0xffff0000))
		break;
	    brec++;
	    j++;
	}
	if (j >= bnset->count)
	    break;
	if (brec->hdrNum == drec->hdrNum
	 && tag == (brec->tagNum & 0xffff0000))
	{
	    *rec = *brec;	/* structure assignment */
	    rec->tagNum &= 0x0000ffff;
	    if (*matches == NULL)
		*matches = xcalloc(1, sizeof(**matches));
	    xx = dbiAppendSet(*matches, rec, 1, sizeof(*rec), 0);
	    brec++;
	    j++;
	}
	drec++;
	i++;
    }
    return (*matches ? (*matches)->count : 0);
}

/**
 * Find file matches in database.
 * @param db		rpm database
 * @param filespec
 * @param key
 * @param data
 * @param matches
 * @return		0 on success, 1 on not found, -2 on error
 */
static int rpmdbFindByFile(rpmdb db, /*@null@*/ const char * filespec,
		DBT * key, DBT * data, /*@out@*/ dbiIndexSet * matches)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies db, *key, *data, *matches, rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@requires maxSet(matches) >= 0 @*/
{
    const char * dirName;
    const char * baseName;
    dbiIndex dbi;
    DBC * dbcursor;
    dbiIndexSet bnset = NULL;
    int bingo;
    int rc;
    int xx;
    int i;

    *matches = NULL;
    if (filespec == NULL) return -2;

    if ((baseName = strrchr(filespec, '/')) != NULL) {
	size_t len = baseName - filespec + 1;
    	char * t = strncpy(alloca(len + 1), filespec, len);
	t[len] = '\0';
	dirName = t;
	baseName++;
    } else {
	dirName = "";
	baseName = filespec;
    }
assert(*dirName != '\0');
assert(baseName != NULL);

    /* Load the basenames index set. */
    if ((dbi = dbiOpen(db, RPMTAG_BASENAMES, 0)) == NULL)
	return -2;

    dbcursor = NULL;
    xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, 0);

/*@-temptrans@*/
key->data = (void *) baseName;
/*@=temptrans@*/
key->size = (UINT32_T) strlen(baseName);
if (key->size == 0) key->size++;	/* XXX "/" fixup. */

    rc = dbiGet(dbi, dbcursor, key, data, DB_SET);
    if (rc > 0) {
	rpmlog(RPMLOG_ERR,
		_("error(%d) getting records from %s index\n"),
		rc, tagName(dbi->dbi_rpmtag));
    } else
    if (rc == 0)
	(void) dbt2set(dbi, data, &bnset);
    xx = dbiCclose(dbi, dbcursor, 0);
    if (rc)
	return rc;
assert(bnset != NULL);
assert(bnset->count > 0);

    /* Check that all basenames are dir tagged. */
    if (_db_tagged_file_indices) {
	bingo = 1;
	for (i = 0; i < bnset->count; i++) {
	    if (bnset->recs[i].tagNum & 0x80000000)
		continue;
	    bingo = 0;
	    break;
	}
    } else
	bingo = 0;

    /* Plan A: Attempt dirName <-> baseName intersection using index keys. */
    if (bingo && (dbi = dbiOpen(db, RPMTAG_DIRNAMES, 0)) != NULL) {
	dbiIndexSet dnset = NULL;

	/* Load the dirnames index set. */
	dbcursor = NULL;
	xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, 0);

/*@-temptrans@*/
key->data = (void *) dirName;
/*@=temptrans@*/
key->size = (UINT32_T) strlen(dirName);
if (key->size == 0) key->size++;	/* XXX "/" fixup. */

	rc = dbiGet(dbi, dbcursor, key, data, DB_SET);
	if (rc > 0) {
	    rpmlog(RPMLOG_ERR,
		_("error(%d) getting records from %s index\n"),
		rc, tagName(dbi->dbi_rpmtag));
	}
	if (rc == 0)
	    (void) dbt2set(dbi, data, &dnset);
	xx = dbiCclose(dbi, dbcursor, 0);

	/* If dnset is non-empty, then attempt Plan A intersection. */
	if (rc == 0 && dnset && dnset->count > 0) {
	    unsigned int tag = taghash(dirName);
	    xx = dbiIntersect(tag, dnset, bnset, matches);
	    bnset = dbiFreeIndexSet(bnset);
	    dnset = dbiFreeIndexSet(dnset);
	    return (*matches != NULL ? 0 : 1);
	}
	dnset = dbiFreeIndexSet(dnset);
    }

    /* Plan B: Reduce the size of the index set before loading headers. */
    if (_db_tagged_file_indices) {
	if (_db_tagged_findbyfile && bnset->count > 1 && *dirName != '\0') {
	    unsigned int tag = taghash(dirName);
	    int j = 0;

	    /* Prune the bnset using the directory tag. */
	    for (i = 0; i < bnset->count; i++) {
		if (bnset->recs[i].tagNum & 0x80000000) {
		    unsigned int ctag = (bnset->recs[i].tagNum & 0xffff0000);
		    bnset->recs[i].tagNum &= 0x0000ffff;
		    if (ctag != tag)
			continue;
		}
		if (i > j)
		    bnset->recs[j] = bnset->recs[i]; /* structure assignment */
		j++;
	    }
	    /* If bnset was shortened by dir tagging, reset the count. */
	    if (j > 0 && j < bnset->count)
		bnset->count = j;
	} else {
	    /* Strip off directory tags. */
	    for (i = 0; i < bnset->count; i++) {
		if (bnset->recs[i].tagNum & 0x80000000)
		    bnset->recs[i].tagNum &= 0x0000ffff;
	    }
	}
    }

    /* OK, find the file using fingerprints and loading headers. */
  { HE_t BN = memset(alloca(sizeof(*BN)), 0, sizeof(*BN));
    HE_t DN = memset(alloca(sizeof(*DN)), 0, sizeof(*DN));
    HE_t DI = memset(alloca(sizeof(*DI)), 0, sizeof(*DI));
    fingerPrintCache fpc = fpCacheCreate(20);
    fingerPrint fp1 = fpLookup(fpc, dirName, baseName, 1);
    rpmmi mi = NULL;
    unsigned int prevoff = 0;
    Header h;

    /* Create an iterator for the matches. */
    mi = rpmmiInit(db, RPMDBI_PACKAGES, NULL, 0);
    mi->mi_set = bnset;

    prevoff = 0;
    BN->tag = RPMTAG_BASENAMES;
    DN->tag = RPMTAG_DIRNAMES;
    DI->tag = RPMTAG_DIRINDEXES;

    /* Find the file(s) with the same fingerprint. */
    while ((h = rpmmiNext(mi)) != NULL) {
	fingerPrint fp2;
	int num;

	/* Reload tags when header changes. */
	if (prevoff != rpmmiInstance(mi)) {
	    prevoff = rpmmiInstance(mi);
	    BN->p.ptr = _free(BN->p.ptr);
	    xx = headerGet(h, BN, 0);
	    DN->p.ptr = _free(DN->p.ptr);
	    xx = headerGet(h, DN, 0);
	    DI->p.ptr = _free(DI->p.ptr);
	    xx = headerGet(h, DI, 0);
	}

	num = dbiIndexRecordFileNumber(mi->mi_set, mi->mi_setx-1);
assert(num >= 0 && num < (int)BN->c);
	fp2 = fpLookup(fpc, DN->p.argv[DI->p.ui32p[num]], BN->p.argv[num], 1);

	/*@-nullpass@*/
	if (FP_EQUAL(fp1, fp2))
	/*@=nullpass@*/
	{
	    dbiIndexItem rec = &mi->mi_set->recs[mi->mi_setx-1];
	    if (*matches == NULL)
		*matches = xcalloc(1, sizeof(**matches));
	    xx = dbiAppendSet(*matches, rec, 1, sizeof(*rec), 0);
	}
    }

    BN->p.ptr = _free(BN->p.ptr);
    DN->p.ptr = _free(DN->p.ptr);
    DI->p.ptr = _free(DI->p.ptr);
    mi = rpmmiFree(mi);

    fpc = fpCacheFree(fpc);
  }

    return (*matches != NULL ? 0 : 1);
}

int rpmdbCount(rpmdb db, rpmTag tag, const void * keyp, size_t keylen)
{
    DBC * dbcursor = NULL;
    DBT k = DBT_INIT;
    DBT v = DBT_INIT;
    dbiIndex dbi;
    int rc;
    int xx;

    if (db == NULL || keyp == NULL)
	return 0;

    dbi = dbiOpen(db, tag, 0);
    if (dbi == NULL)
	return 0;

    if (keylen == 0)
	keylen = strlen(keyp);

/*@-temptrans@*/
    k.data = (void *) keyp;
/*@=temptrans@*/
    k.size = (UINT32_T) keylen;

    xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, 0);
    rc = dbiGet(dbi, dbcursor, &k, &v, DB_SET);
#ifndef	SQLITE_HACK
    xx = dbiCclose(dbi, dbcursor, 0);
    dbcursor = NULL;
#endif

    if (rc == 0) {		/* success */
	dbiIndexSet matches;
	/*@-nullpass@*/ /* FIX: matches might be NULL */
	matches = NULL;
	(void) dbt2set(dbi, &v, &matches);
	if (matches) {
	    rc = dbiIndexSetCount(matches);
	    matches = dbiFreeIndexSet(matches);
	}
	/*@=nullpass@*/
    } else
    if (rc == DB_NOTFOUND) {	/* not found */
	rc = 0;
    } else {			/* error */
	rpmlog(RPMLOG_ERR,
		_("error(%d) getting records from %s index\n"),
		rc, tagName(dbi->dbi_rpmtag));
	rc = -1;
    }

#ifdef	SQLITE_HACK
    xx = dbiCclose(dbi, dbcursor, 0);
    dbcursor = NULL;
#endif

    return rc;
}

/* XXX python/upgrade.c, install.c, uninstall.c */
int rpmdbCountPackages(rpmdb db, const char * name)
{
    return rpmdbCount(db, RPMTAG_NAME, name, 0);
}

/**
 * Attempt partial matches on name[-version[-release]] strings.
 * @param dbi		index database handle (always RPMTAG_NAME)
 * @param dbcursor	index database cursor
 * @param key		search key/length/flags
 * @param data		search data/length/flags
 * @param name		package name
 * @param version	package version (can be a pattern)
 * @param release	package release (can be a pattern)
 * @retval matches	set of header instances that match
 * @return 		RPMRC_OK on match, RPMRC_NOMATCH or RPMRC_FAIL
 */
static rpmRC dbiFindMatches(dbiIndex dbi, DBC * dbcursor,
		DBT * key, DBT * data,
		const char * name,
		/*@null@*/ const char * version,
		/*@null@*/ const char * release,
		/*@out@*/ dbiIndexSet * matches)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies dbi, *dbcursor, *key, *data, *matches,
		rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@requires maxSet(matches) >= 0 @*/
{
    int gotMatches = 0;
    int rc;
    unsigned i;

/*@-temptrans@*/
key->data = (void *) name;
/*@=temptrans@*/
key->size = (UINT32_T) strlen(name);

    rc = dbiGet(dbi, dbcursor, key, data, DB_SET);

    if (rc == 0) {		/* success */
	(void) dbt2set(dbi, data, matches);
	if (version == NULL && release == NULL)
	    return RPMRC_OK;
    } else
    if (rc == DB_NOTFOUND) {	/* not found */
	return RPMRC_NOTFOUND;
    } else {			/* error */
	rpmlog(RPMLOG_ERR,
		_("error(%d) getting records from %s index\n"),
		rc, tagName(dbi->dbi_rpmtag));
	return RPMRC_FAIL;
    }

    /* Make sure the version and release match. */
    for (i = 0; i < dbiIndexSetCount(*matches); i++) {
	unsigned int recoff = dbiIndexRecordOffset(*matches, i);
	rpmmi mi;
	Header h;

	if (recoff == 0)
	    continue;

	mi = rpmmiInit(dbi->dbi_rpmdb,
			RPMDBI_PACKAGES, &recoff, sizeof(recoff));

	/* Set iterator selectors for version/release if available. */
	if (version &&
	    rpmmiAddPattern(mi, RPMTAG_VERSION, RPMMIRE_DEFAULT, version))
	{
	    rc = RPMRC_FAIL;
	    goto exit;
	}
	if (release &&
	    rpmmiAddPattern(mi, RPMTAG_RELEASE, RPMMIRE_DEFAULT, release))
	{
	    rc = RPMRC_FAIL;
	    goto exit;
	}

	h = rpmmiNext(mi);
	if (h)
	    (*matches)->recs[gotMatches++] = (*matches)->recs[i];
	else
	    (*matches)->recs[i].hdrNum = 0;
	mi = rpmmiFree(mi);
    }

    if (gotMatches) {
	(*matches)->count = gotMatches;
	rc = RPMRC_OK;
    } else
	rc = RPMRC_NOTFOUND;

exit:
/*@-unqualifiedtrans@*/ /* FIX: double indirection */
    if (rc && matches && *matches)
	*matches = dbiFreeIndexSet(*matches);
/*@=unqualifiedtrans@*/
    return rc;
}

/**
 * Lookup by name, name-version, and finally by name-version-release.
 * Both version and release can be patterns.
 * @todo Name must be an exact match, as name is a db key.
 * @param dbi		index database handle (always RPMTAG_NAME)
 * @param dbcursor	index database cursor
 * @param key		search key/length/flags
 * @param data		search data/length/flags
 * @param arg		name[-version[-release]] string
 * @retval matches	set of header instances that match
 * @return 		RPMRC_OK on match, RPMRC_NOMATCH or RPMRC_FAIL
 */
static rpmRC dbiFindByLabel(dbiIndex dbi, DBC * dbcursor, DBT * key, DBT * data,
		/*@null@*/ const char * arg, /*@out@*/ dbiIndexSet * matches)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies dbi, *dbcursor, *key, *data, *matches,
		rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@requires maxSet(matches) >= 0 @*/
{
    const char * release;
    char * localarg;
    char * s;
    char c;
    int brackets;
    rpmRC rc;
 
    if (arg == NULL || strlen(arg) == 0) return RPMRC_NOTFOUND;

    /* did they give us just a name? */
    rc = dbiFindMatches(dbi, dbcursor, key, data, arg, NULL, NULL, matches);
    if (rc != RPMRC_NOTFOUND) return rc;

    /*@-unqualifiedtrans@*/ /* FIX: double indirection */
    *matches = dbiFreeIndexSet(*matches);
    /*@=unqualifiedtrans@*/

    /* maybe a name and a release */
    localarg = alloca(strlen(arg) + 1);
    s = stpcpy(localarg, arg);

    c = '\0';
    brackets = 0;
    for (s -= 1; s > localarg; s--) {
	switch (*s) {
	case '[':
	    brackets = 1;
	    /*@switchbreak@*/ break;
	case ']':
	    if (c != '[') brackets = 0;
	    /*@switchbreak@*/ break;
	}
	c = *s;
	if (!brackets && *s == '-')
	    break;
    }

    /*@-nullstate@*/	/* FIX: *matches may be NULL. */
    if (s == localarg) return RPMRC_NOTFOUND;

    *s = '\0';
    rc = dbiFindMatches(dbi, dbcursor, key, data, localarg, s + 1, NULL, matches);
    /*@=nullstate@*/
    if (rc != RPMRC_NOTFOUND) return rc;

    /*@-unqualifiedtrans@*/ /* FIX: double indirection */
    *matches = dbiFreeIndexSet(*matches);
    /*@=unqualifiedtrans@*/
    
    /* how about name-version-release? */

    release = s + 1;

    c = '\0';
    brackets = 0;
    for (; s > localarg; s--) {
	switch (*s) {
	case '[':
	    brackets = 1;
	    /*@switchbreak@*/ break;
	case ']':
	    if (c != '[') brackets = 0;
	    /*@switchbreak@*/ break;
	}
	c = *s;
	if (!brackets && *s == '-')
	    break;
    }

    if (s == localarg) return RPMRC_NOTFOUND;

    *s = '\0';
    /*@-nullstate@*/	/* FIX: *matches may be NULL. */
    return dbiFindMatches(dbi, dbcursor, key, data, localarg, s + 1, release, matches);
    /*@=nullstate@*/
}

void * dbiStatsAccumulator(dbiIndex dbi, int opx)
{
    void * sw = NULL;
    switch (opx) {
    case 14:	/* RPMTS_OP_DBGET */
	sw = &dbi->dbi_rpmdb->db_getops;
	break;
    case 15:	/* RPMTS_OP_DBPUT */
	sw = &dbi->dbi_rpmdb->db_putops;
	break;
    default:	/* XXX wrong, but let's not return NULL. */
    case 16:	/* RPMTS_OP_DBDEL */
	sw = &dbi->dbi_rpmdb->db_delops;
	break;
    }
    return sw;
}

/**
 * Rewrite a header into packages (if necessary) and free the header.
 *   Note: this is called from a markReplacedFiles iteration, and *must*
 *   preserve the "join key" (i.e. offset) for the header.
 * @param mi		database iterator
 * @param dbi		index database handle
 * @return 		0 on success
 */
static int miFreeHeader(rpmmi mi, dbiIndex dbi)
	/*@globals fileSystem, internalState @*/
	/*@modifies mi, dbi, fileSystem, internalState @*/
{
    int rc = 0;

    if (mi == NULL || mi->mi_h == NULL)
	return 0;

    if (dbi && mi->mi_dbc && mi->mi_modified && mi->mi_prevoffset) {
	DBT k = DBT_INIT;
	DBT v = DBT_INIT;
	rpmRC rpmrc = RPMRC_NOTFOUND;
	int xx;

/*@i@*/	k.data = (void *) &mi->mi_prevoffset;
	k.size = (UINT32_T) sizeof(mi->mi_prevoffset);
	{   size_t len = 0;
	    v.data = headerUnload(mi->mi_h, &len);
	    v.size = (UINT32_T) len;
	}

	/* Check header digest/signature on blob export (if requested). */
	if (mi->mi_ts) {
	    const char * msg = NULL;
	    int lvl;

assert(v.data != NULL);
	    rpmrc = headerCheck(rpmtsDig(mi->mi_ts), v.data, v.size, &msg);
	    rpmtsCleanDig(mi->mi_ts);
	    lvl = (rpmrc == RPMRC_FAIL ? RPMLOG_ERR : RPMLOG_DEBUG);
	    rpmlog(lvl, "%s h#%8u %s",
		(rpmrc == RPMRC_FAIL ? _("miFreeHeader: skipping") : "write"),
			mi->mi_prevoffset, (msg ? msg : "\n"));
	    msg = _free(msg);
	}

	if (v.data != NULL && rpmrc != RPMRC_FAIL) {
	    sigset_t signalMask;
	    (void) blockSignals(dbi->dbi_rpmdb, &signalMask);
	    rc = dbiPut(dbi, mi->mi_dbc, &k, &v, DB_KEYLAST);
	    if (rc) {
		rpmlog(RPMLOG_ERR,
			_("error(%d) storing record #%d into %s\n"),
			rc, mi->mi_prevoffset, tagName(dbi->dbi_rpmtag));
	    }
	    xx = dbiSync(dbi, 0);
	    (void) unblockSignals(dbi->dbi_rpmdb, &signalMask);
	}
	v.data = _free(v.data); /* headerUnload */
	v.size = 0;
    }

    (void)headerFree(mi->mi_h);
    mi->mi_h = NULL;

/*@-nullstate@*/
    return rc;
/*@=nullstate@*/
}

static void rpmmiFini(void * _mi)
	/*@globals rpmmiRock @*/
	/*@modifies _mi, rpmmiRock @*/
{
    rpmmi mi = _mi;
    rpmmi * prev, next;
    dbiIndex dbi;
    int xx;

    prev = &rpmmiRock;
    while ((next = *prev) != NULL && next != mi)
	prev = &next->mi_next;
    if (next) {
/*@i@*/	*prev = next->mi_next;
	next->mi_next = NULL;
    }

    /* XXX there's code that traverses here w mi->mi_db == NULL. b0rked imho. */
    if (mi->mi_db) {
	dbi = dbiOpen(mi->mi_db, RPMDBI_PACKAGES, 0);
assert(dbi != NULL);

	xx = miFreeHeader(mi, dbi);

	if (mi->mi_dbc)
	    xx = dbiCclose(dbi, mi->mi_dbc, 0);
	mi->mi_dbc = NULL;
	/* XXX rpmdbUnlink will not do.
	 * NB: must be called after rpmmiRock cleanup.
	 */
	(void) rpmdbClose(mi->mi_db);
	mi->mi_db = NULL;
    }

    mi->mi_re = mireFreeAll(mi->mi_re, mi->mi_nre);

    mi->mi_set = dbiFreeIndexSet(mi->mi_set);

    /* XXX this needs to be done elsewhere, not within destructor. */
    (void) rpmdbCheckSignals();
}

/*@unchecked@*/
int _rpmmi_debug = 0;

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmmiPool;

static rpmmi rpmmiGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmdbPool, fileSystem @*/
	/*@modifies pool, _rpmdbPool, fileSystem @*/
{
    rpmmi mi;

    if (_rpmmiPool == NULL) {
	_rpmmiPool = rpmioNewPool("mi", sizeof(*mi), -1, _rpmmi_debug,
			NULL, NULL, rpmmiFini);
	pool = _rpmmiPool;
    }
    return (rpmmi) rpmioGetPool(pool, sizeof(*mi));
}

unsigned int rpmmiInstance(rpmmi mi) {
    return (mi ? mi->mi_offset : 0);
}

unsigned int rpmmiFilenum(rpmmi mi) {
    return (mi ? mi->mi_filenum : 0);
}

int rpmmiCount(rpmmi mi) {
    return (mi && mi->mi_set ?  mi->mi_set->count : 0);
}

/**
 * Compare iterator selectors by rpm tag (qsort/bsearch).
 * @param a		1st iterator selector
 * @param b		2nd iterator selector
 * @return		result of comparison
 */
static int mireCmp(const void * a, const void * b)
{
/*@-castexpose @*/
    const miRE mireA = (const miRE) a;
    const miRE mireB = (const miRE) b;
/*@=castexpose @*/
    return (mireA->tag - mireB->tag);
}

/**
 * Copy pattern, escaping for appropriate mode.
 * @param tag		rpm tag
 * @retval modep	type of pattern match
 * @param pattern	pattern to duplicate
 * @return		duplicated pattern
 */
static /*@only@*/ char * mireDup(rpmTag tag, rpmMireMode *modep,
			const char * pattern)
	/*@modifies *modep @*/
	/*@requires maxSet(modep) >= 0 @*/
{
    const char * s;
    char * pat;
    char * t;
    int brackets;
    size_t nb;
    int c;

    switch (*modep) {
    default:
    case RPMMIRE_DEFAULT:
	if (tag == RPMTAG_DIRNAMES || tag == RPMTAG_BASENAMES) {
	    *modep = RPMMIRE_GLOB;
	    pat = xstrdup(pattern);
	    break;
	}

	nb = strlen(pattern) + sizeof("^$");

	/* Find no. of bytes needed for pattern. */
	/* periods and plusses are escaped, splats become '.*' */
	c = (int) '\0';
	brackets = 0;
	for (s = pattern; *s != '\0'; s++) {
	    switch (*s) {
	    case '.':
	    case '+':
	    case '*':
		if (!brackets) nb++;
		/*@switchbreak@*/ break;
	    case '\\':
		s++;
		/*@switchbreak@*/ break;
	    case '[':
		brackets = 1;
		/*@switchbreak@*/ break;
	    case ']':
		if (c != (int) '[') brackets = 0;
		/*@switchbreak@*/ break;
	    }
	    c = (int) *s;
	}

	pat = t = xmalloc(nb);

	if (pattern[0] != '^') *t++ = '^';

	/* Copy pattern, escaping periods, prefixing splats with period. */
	c = (int) '\0';
	brackets = 0;
	for (s = pattern; *s != '\0'; s++, t++) {
	    switch (*s) {
	    case '.':
	    case '+':
		if (!brackets) *t++ = '\\';
		/*@switchbreak@*/ break;
	    case '*':
		if (!brackets) *t++ = '.';
		/*@switchbreak@*/ break;
	    case '\\':
		*t++ = *s++;
		/*@switchbreak@*/ break;
	    case '[':
		brackets = 1;
		/*@switchbreak@*/ break;
	    case ']':
		if (c != (int) '[') brackets = 0;
		/*@switchbreak@*/ break;
	    }
	    *t = *s;
	    c = (int) *t;
	}

	if (s > pattern && s[-1] != '$') *t++ = '$';
	*t = '\0';
	*modep = RPMMIRE_REGEX;
	break;
    case RPMMIRE_STRCMP:
    case RPMMIRE_REGEX:
    case RPMMIRE_GLOB:
	pat = xstrdup(pattern);
	break;
    }

    return pat;
}

int rpmmiAddPattern(rpmmi mi, rpmTag tag,
		rpmMireMode mode, const char * pattern)
{
    static rpmMireMode defmode = (rpmMireMode)-1;
    miRE nmire = NULL;
    miRE mire = NULL;
    const char * allpat = NULL;
    int notmatch = 0;
    int rc = 0;

    if (defmode == (rpmMireMode)-1) {
	const char *t = rpmExpand("%{?_query_selector_match}", NULL);

	if (*t == '\0' || !strcmp(t, "default"))
	    defmode = RPMMIRE_DEFAULT;
	else if (!strcmp(t, "strcmp"))
	    defmode = RPMMIRE_STRCMP;
	else if (!strcmp(t, "regex"))
	    defmode = RPMMIRE_REGEX;
	else if (!strcmp(t, "glob"))
	    defmode = RPMMIRE_GLOB;
	else
	    defmode = RPMMIRE_DEFAULT;
	t = _free(t);
     }

    if (mi == NULL || pattern == NULL)
	return rc;

    /* Leading '!' inverts pattern match sense, like "grep -v". */
    if (*pattern == '!') {
	notmatch = 1;
	pattern++;
    }

    nmire = mireNew(mode, tag);
assert(nmire != NULL);
    allpat = mireDup(nmire->tag, &nmire->mode, pattern);

    if (nmire->mode == RPMMIRE_DEFAULT)
	nmire->mode = defmode;

    rc = mireRegcomp(nmire, allpat);
    if (rc)
	goto exit;

    if (mi->mi_re == NULL) {
	mi->mi_re = mireGetPool(_mirePool);
	mire = mi->mi_re;
    } else {
	void *use =  mi->mi_re->_item.use;
	void *pool = mi->mi_re->_item.pool;
	mi->mi_re = xrealloc(mi->mi_re, (mi->mi_nre + 1) * sizeof(*mi->mi_re));
	mire = mi->mi_re + mi->mi_nre;
	memset(mire, 0, sizeof(*mire));
	/* XXX ensure no segfault, copy the use/pool from 1st item. */
/*@-assignexpose@*/
	mire->_item.use = use;
	mire->_item.pool = pool;
/*@=assignexpose@*/
    }
    mi->mi_nre++;
    
    mire->mode = nmire->mode;
    mire->pattern = nmire->pattern;	nmire->pattern = NULL;
    mire->preg = nmire->preg;		nmire->preg = NULL;
    mire->cflags = nmire->cflags;
    mire->eflags = nmire->eflags;
    mire->fnflags = nmire->fnflags;
    mire->tag = nmire->tag;
    mire->notmatch = notmatch;
    /* XXX todo: permit PCRE patterns to be used. */
    mire->offsets = NULL;
    mire->noffsets = 0;

    if (mi->mi_nre > 1)
	qsort(mi->mi_re, mi->mi_nre, sizeof(*mi->mi_re), mireCmp);

exit:
    allpat = _free(allpat);
    nmire = mireFree(nmire);
    return rc;
}

/**
 * Return iterator selector match.
 * @param mi		rpm database iterator
 * @return		1 if header should be skipped
 */
/*@-onlytrans@*/	/* XXX miRE array, not refcounted. */
static int mireSkip (const rpmmi mi)
	/*@globals internalState @*/
	/*@modifies mi->mi_re, internalState @*/
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    char numbuf[32];
    miRE mire;
    int ntags = 0;
    int nmatches = 0;
    int i;
    int rc;

    if (mi->mi_h == NULL)	/* XXX can't happen */
	return 1;

    /*
     * Apply tag tests, implicitly "||" for multiple patterns/values of a
     * single tag, implicitly "&&" between multiple tag patterns.
     */
    if ((mire = mi->mi_re) == NULL)
	return 0;

    for (i = 0; i < mi->mi_nre; i++, mire++) {
	int anymatch;

	he->tag = mire->tag;

	if (!headerGet(mi->mi_h, he, 0)) {
	    if (he->tag != RPMTAG_EPOCH) {
		ntags++;
		continue;
	    }
	    he->t = RPM_UINT32_TYPE;
	    he->p.ui32p = xcalloc(1, sizeof(*he->p.ui32p));
	    he->c = 1;
	}

	anymatch = 0;		/* no matches yet */
	while (1) {
	    unsigned j;
	    switch (he->t) {
	    case RPM_UINT8_TYPE:
		for (j = 0; j < (unsigned) he->c; j++) {
		    sprintf(numbuf, "%u", (unsigned) he->p.ui8p[j]);
		    rc = mireRegexec(mire, numbuf, 0);
		    if ((rc >= 0 && !mire->notmatch) || (rc < 0 && mire->notmatch))
			anymatch++;
		}
		/*@switchbreak@*/ break;
	    case RPM_UINT16_TYPE:
		for (j = 0; j < (unsigned) he->c; j++) {
		    sprintf(numbuf, "%u", (unsigned) he->p.ui16p[j]);
		    rc = mireRegexec(mire, numbuf, 0);
		    if ((rc >= 0 && !mire->notmatch) || (rc < 0 && mire->notmatch))
			anymatch++;
		}
		/*@switchbreak@*/ break;
	    case RPM_UINT32_TYPE:
		for (j = 0; j < (unsigned) he->c; j++) {
		    sprintf(numbuf, "%u", (unsigned) he->p.ui32p[j]);
		    rc = mireRegexec(mire, numbuf, 0);
		    if ((rc >= 0 && !mire->notmatch) || (rc < 0 && mire->notmatch))
			anymatch++;
		}
		/*@switchbreak@*/ break;
	    case RPM_UINT64_TYPE:
/*@-duplicatequals@*/
		for (j = 0; j < (unsigned) he->c; j++) {
		    sprintf(numbuf, "%llu", (unsigned long long)he->p.ui64p[j]);
		    rc = mireRegexec(mire, numbuf, 0);
		    if ((rc >= 0 && !mire->notmatch) || (rc < 0 && mire->notmatch))
			anymatch++;
		}
/*@=duplicatequals@*/
		/*@switchbreak@*/ break;
	    case RPM_STRING_TYPE:
		rc = mireRegexec(mire, he->p.str, 0);
		if ((rc >= 0 && !mire->notmatch) || (rc < 0 && mire->notmatch))
		    anymatch++;
		/*@switchbreak@*/ break;
	    case RPM_STRING_ARRAY_TYPE:
		for (j = 0; j < (unsigned) he->c; j++) {
		    rc = mireRegexec(mire, he->p.argv[j], 0);
		    if ((rc >= 0 && !mire->notmatch) || (rc < 0 && mire->notmatch)) {
			anymatch++;
			/*@innerbreak@*/ break;
		    }
		}
		/*@switchbreak@*/ break;
	    case RPM_BIN_TYPE:
	    {	const char * s;
assert(he->p.ptr != NULL);
		s = bin2hex(he->p.ptr, he->c);
		rc = mireRegexec(mire, s, 0);
		if ((rc >= 0 && !mire->notmatch) || (rc < 0 && mire->notmatch))
		    anymatch++;
		s = _free(s);
	    }   /*@switchbreak@*/ break;
	    case RPM_I18NSTRING_TYPE:
	    default:
		/*@switchbreak@*/ break;
	    }
	    if ((i+1) < mi->mi_nre && mire[0].tag == mire[1].tag) {
		i++;
		mire++;
		/*@innercontinue@*/ continue;
	    }
	    /*@innerbreak@*/ break;
	}

	he->p.ptr = _free(he->p.ptr);

	if (anymatch)
	    nmatches++;
	ntags++;
    }

    return (ntags > 0 && ntags == nmatches ? 0 : 1);
}
/*@=onlytrans@*/

int rpmmiSetRewrite(rpmmi mi, int rewrite)
{
    int rc;
    if (mi == NULL)
	return 0;
    rc = (mi->mi_cflags & DB_WRITECURSOR) ? 1 : 0;
    if (rewrite)
	mi->mi_cflags |= DB_WRITECURSOR;
    else
	mi->mi_cflags &= ~DB_WRITECURSOR;
    return rc;
}

int rpmmiSetModified(rpmmi mi, int modified)
{
    int rc;
    if (mi == NULL)
	return 0;
    rc = mi->mi_modified;
    mi->mi_modified = modified;
    return rc;
}

int rpmmiSetHdrChk(rpmmi mi, rpmts ts)
{
    int rc = 0;
    if (mi == NULL)
	return 0;
/*@-assignexpose -newreftrans @*/ /* XXX forward linkage prevents rpmtsLink */
/*@i@*/ mi->mi_ts = ts;
/*@=assignexpose =newreftrans @*/
    return rc;
}

static int _rpmmi_usermem = 1;

static int rpmmiGet(dbiIndex dbi, DBC * dbcursor, DBT * kp, DBT * vp,
                unsigned int flags)
{
    int map;
    int rc;

    switch (dbi->dbi_rpmdb->db_api) {
    default:	map = 0;		break;
    case 3:	map = _rpmmi_usermem;	break;	/* Berkeley DB */
    }

    if (map) {
	static const int _prot = PROT_READ | PROT_WRITE;
	static const int _flags = MAP_PRIVATE| MAP_ANONYMOUS;
	static const int _fdno = -1;
	static const off_t _off = 0;

	vp->flags |= DB_DBT_USERMEM;
	rc = dbiGet(dbi, dbcursor, kp, vp, flags);
	if (rc == DB_BUFFER_SMALL) {
	    size_t uhlen = vp->size;
	    void * uh = mmap(NULL, uhlen, _prot, _flags, _fdno, _off);
	    if (uh == NULL || uh == (void *)-1)
		fprintf(stderr,
		    "==> mmap(%p[%u], 0x%x, 0x%x, %d, 0x%x) error(%d): %s\n",
		    NULL, uhlen, _prot, _flags, _fdno, (unsigned)_off,
		    errno, strerror(errno));

	    vp->ulen = (u_int32_t)uhlen;
	    vp->data = uh;
	    rc = dbiGet(dbi, dbcursor, kp, vp, DB_SET);
	    if (rc == 0) {
		if (mprotect(uh, uhlen, PROT_READ) != 0)
		    fprintf(stderr, "==> mprotect(%p[%u],0x%x) error(%d): %s\n",
			uh, uhlen, PROT_READ,
			errno, strerror(errno));
	    } else {
		if (munmap(uh, uhlen) != 0)
		    fprintf(stderr, "==> munmap(%p[%u]) error(%d): %s\n",
                	uh, uhlen, errno, strerror(errno));
	    }
	}
    } else
	rc = dbiGet(dbi, dbcursor, kp, vp, flags);
    return rc;
}

Header rpmmiNext(rpmmi mi)
{
    dbiIndex dbi;
    DBT k = DBT_INIT;
    DBT v = DBT_INIT;
    union _dbswap mi_offset;
    void * uh;
    size_t uhlen;
    int map;
    int rc;
    int xx;

    if (mi == NULL)
	return NULL;

    dbi = dbiOpen(mi->mi_db, RPMDBI_PACKAGES, 0);
    if (dbi == NULL)
	return NULL;

    switch (dbi->dbi_rpmdb->db_api) {
    default:	map = 0;		break;
    case 3:	map = _rpmmi_usermem;	break;	/* Berkeley DB */
    }

    /*
     * Cursors are per-iterator, not per-dbi, so get a cursor for the
     * iterator on 1st call. If the iteration is to rewrite headers, and the
     * CDB model is used for the database, then the cursor needs to
     * marked with DB_WRITECURSOR as well.
     */
    if (mi->mi_dbc == NULL)
	xx = dbiCopen(dbi, dbi->dbi_txnid, &mi->mi_dbc, mi->mi_cflags);

next:
    if (mi->mi_set) {
	/* The set of header instances is known in advance. */
	if (!(mi->mi_setx < mi->mi_set->count))
	    return NULL;
	mi->mi_offset = dbiIndexRecordOffset(mi->mi_set, mi->mi_setx);
	mi->mi_filenum = dbiIndexRecordFileNumber(mi->mi_set, mi->mi_setx);
	mi->mi_setx++;
	/* If next header is identical, return it now. */
	if (mi->mi_offset == mi->mi_prevoffset && mi->mi_h != NULL)
	    return mi->mi_h;
	/* Fetch header by offset. */
	mi_offset.ui = mi->mi_offset;
	if (dbiByteSwapped(dbi) == 1)
	    _DBSWAP(mi_offset);
/*@-immediatetrans@*/
	k.data = &mi_offset.ui;
/*@=immediatetrans@*/
	k.size = (u_int32_t)sizeof(mi_offset.ui);
	rc = rpmmiGet(dbi, mi->mi_dbc, &k, &v, DB_SET);
    }
    else {
	/* Iterating Packages database. */
	assert(mi->mi_rpmtag == RPMDBI_PACKAGES);

	/* Fetch header with DB_NEXT. */
	/* Instance 0 is the largest header instance in the database,
	 * and should be skipped. */
	do {
	    rc = rpmmiGet(dbi, mi->mi_dbc, &k, &v, DB_NEXT);
	    if (rc == 0) {
		memcpy(&mi_offset, k.data, sizeof(mi_offset.ui));
		if (dbiByteSwapped(dbi) == 1)
		    _DBSWAP(mi_offset);
		mi->mi_offset = mi_offset.ui;
	    }
	} while (rc == 0 && mi_offset.ui == 0);
    }

    /* Did the header blob load correctly? */
    if (rc)
	return NULL;

    uh = v.data;
    uhlen = v.size;

    if (uh == NULL)
	return NULL;

    /* Rewrite current header (if necessary) and unlink. */
    xx = miFreeHeader(mi, dbi);

    /* Check header digest/signature once (if requested). */
    if (mi->mi_ts) {
	rpmRC rpmrc = RPMRC_NOTFOUND;

	/* Don't bother re-checking a previously read header. */
	if (mi->mi_db->db_bits) {
	    pbm_set * set;

	    set = PBM_REALLOC((pbm_set **)&mi->mi_db->db_bits,
			&mi->mi_db->db_nbits, mi->mi_offset);
	    if (PBM_ISSET(mi->mi_offset, set))
		rpmrc = RPMRC_OK;
	}

	/* If blob is unchecked, check blob import consistency now. */
	if (rpmrc != RPMRC_OK) {
	    const char * msg = NULL;
	    int lvl;

	    rpmrc = headerCheck(rpmtsDig(mi->mi_ts), uh, uhlen, &msg);
	    rpmtsCleanDig(mi->mi_ts);
	    lvl = (rpmrc == RPMRC_FAIL ? RPMLOG_ERR : RPMLOG_DEBUG);
	    rpmlog(lvl, "%s h#%8u %s\n",
		(rpmrc == RPMRC_FAIL ? _("rpmdb: skipping") : _("rpmdb: read")),
			mi->mi_offset, (msg ? msg : ""));
	    msg = _free(msg);

	    /* Mark header checked. */
	    if (mi->mi_db && mi->mi_db->db_bits && rpmrc == RPMRC_OK) {
		pbm_set * set;

		set = PBM_REALLOC((pbm_set **)&mi->mi_db->db_bits,
			&mi->mi_db->db_nbits, mi->mi_offset);
		PBM_SET(mi->mi_offset, set);
	    }

	    /* Skip damaged and inconsistent headers. */
	    if (rpmrc == RPMRC_FAIL)
		goto next;
	}
    }

    if (map) {
/*@-onlytrans@*/
	mi->mi_h = headerLoad(uh);
/*@=onlytrans@*/
	if (mi->mi_h) {
	    mi->mi_h->flags |= HEADERFLAG_MAPPED;
	    mi->mi_h->flags |= HEADERFLAG_RDONLY;
	}
    } else
	mi->mi_h = headerCopyLoad(uh);

    if (mi->mi_h == NULL || !headerIsEntry(mi->mi_h, RPMTAG_NAME)) {
	rpmlog(RPMLOG_ERR,
		_("rpmdb: damaged header #%u retrieved -- skipping.\n"),
		mi->mi_offset);
	/* damaged header should not be reused */
	if (mi->mi_h) {
	    (void)headerFree(mi->mi_h);
	    mi->mi_h = NULL;
	}
	/* TODO: skip more mi_set records */
	goto next;
    }

    /* Skip this header if iterator selector (if any) doesn't match. */
    if (mireSkip(mi))
	goto next;

    /* Mark header with its instance number. */
    {	char origin[32];
	sprintf(origin, "rpmdb (h#%u)", mi->mi_offset);
	(void) headerSetOrigin(mi->mi_h, origin);
	(void) headerSetInstance(mi->mi_h, mi->mi_offset);
    }

    mi->mi_prevoffset = mi->mi_offset;
    mi->mi_modified = 0;

/*@-compdef -retalias -retexpose -usereleased @*/
    return mi->mi_h;
/*@=compdef =retalias =retexpose =usereleased @*/
}

static void rpmdbSortIterator(/*@null@*/ rpmmi mi)
	/*@modifies mi @*/
{
    if (mi && mi->mi_set && mi->mi_set->recs && mi->mi_set->count > 0) {
    /*
     * mergesort is much (~10x with lots of identical basenames) faster
     * than pure quicksort, but glibc uses msort_with_tmp() on stack.
     */
#if defined(__GLIBC__)
	qsort(mi->mi_set->recs, mi->mi_set->count,
		sizeof(*mi->mi_set->recs), hdrNumCmp);
#else
	rpm_mergesort(mi->mi_set->recs, mi->mi_set->count,
		sizeof(*mi->mi_set->recs), hdrNumCmp);
#endif
	mi->mi_sorted = 1;
    }
}

static int rpmdbGrowIterator(/*@null@*/ rpmmi mi, int fpNum,
		unsigned int exclude, unsigned int tag)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies mi, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    DBC * dbcursor;
    DBT * key;
    DBT * data;
    dbiIndex dbi = NULL;
    dbiIndexSet set;
    int rc;
    int xx;
    int i, j;

    if (mi == NULL)
	return 1;

    dbcursor = mi->mi_dbc;
    key = &mi->mi_key;
    data = &mi->mi_data;
    if (key->data == NULL)
	return 1;

    dbi = dbiOpen(mi->mi_db, mi->mi_rpmtag, 0);
    if (dbi == NULL)
	return 1;

    xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, 0);
    rc = dbiGet(dbi, dbcursor, key, data, DB_SET);
#ifndef	SQLITE_HACK
    xx = dbiCclose(dbi, dbcursor, 0);
    dbcursor = NULL;
#endif

    if (rc) {			/* error/not found */
	if (rc != DB_NOTFOUND)
	    rpmlog(RPMLOG_ERR,
		_("error(%d) getting records from %s index\n"),
		rc, tagName(dbi->dbi_rpmtag));
#ifdef	SQLITE_HACK
	xx = dbiCclose(dbi, dbcursor, 0);
	dbcursor = NULL;
#endif
	return rc;
    }

    set = NULL;
    (void) dbt2set(dbi, data, &set);

    /* prune the set against exclude and tag */
    for (i = j = 0; i < set->count; i++) {
	if (exclude && set->recs[i].hdrNum == exclude)
	    continue;
	if (_db_tagged_file_indices && set->recs[i].tagNum & 0x80000000) {
	    /* tagged entry */
	    if ((set->recs[i].tagNum & 0xffff0000) != tag)
		continue;
	    set->recs[i].tagNum &= 0x0000ffff;
	}
	if (i > j)
	    set->recs[j] = set->recs[i];
	j++;
    }
    if (j == 0) {
#ifdef	SQLITE_HACK
	xx = dbiCclose(dbi, dbcursor, 0);
	dbcursor = NULL;
#endif
	set = dbiFreeIndexSet(set);
	return DB_NOTFOUND;
    }
    set->count = j;

    for (i = 0; i < set->count; i++)
	set->recs[i].fpNum = fpNum;

#ifdef	SQLITE_HACK
    xx = dbiCclose(dbi, dbcursor, 0);
    dbcursor = NULL;
#endif

    if (mi->mi_set == NULL) {
	mi->mi_set = set;
    } else {
#if 0
fprintf(stderr, "+++ %d = %d + %d\t\"%s\"\n", (mi->mi_set->count + set->count), mi->mi_set->count, set->count, ((char *)key->data));
#endif
	mi->mi_set->recs = xrealloc(mi->mi_set->recs,
		(mi->mi_set->count + set->count) * sizeof(*(mi->mi_set->recs)));
	memcpy(mi->mi_set->recs + mi->mi_set->count, set->recs,
		set->count * sizeof(*(mi->mi_set->recs)));
	mi->mi_set->count += set->count;
	set = dbiFreeIndexSet(set);
    }

    return rc;
}

int rpmmiPrune(rpmmi mi, int * hdrNums, int nHdrNums, int sorted)
{
    if (mi == NULL || hdrNums == NULL || nHdrNums <= 0)
	return 1;

    if (mi->mi_set)
	(void) dbiPruneSet(mi->mi_set, hdrNums, nHdrNums, sizeof(*hdrNums), sorted);
    return 0;
}

int rpmmiGrow(rpmmi mi, const int * hdrNums, int nHdrNums)
{
    if (mi == NULL || hdrNums == NULL || nHdrNums <= 0)
	return 1;

    if (mi->mi_set == NULL)
	mi->mi_set = xcalloc(1, sizeof(*mi->mi_set));
    (void) dbiAppendSet(mi->mi_set, hdrNums, nHdrNums, sizeof(*hdrNums), 0);
    return 0;
}

rpmmi rpmmiInit(rpmdb db, rpmTag tag,
		const void * keyp, size_t keylen)
	/*@globals rpmmiRock @*/
	/*@modifies rpmmiRock @*/
{
    rpmmi mi;
    dbiIndexSet set = NULL;
    dbiIndex dbi;
    int isLabel = 0;

    if (db == NULL)
	return NULL;

    (void) rpmdbCheckSignals();

    /* XXX HACK to remove rpmdbFindByLabel/findMatches from the API */
    if (tag == RPMDBI_LABEL) {
	tag = RPMTAG_NAME;
	isLabel = 1;
    }

    dbi = dbiOpen(db, tag, 0);
    if (dbi == NULL)
	return NULL;

    mi = rpmmiGetPool(_rpmmiPool);
    (void)rpmioLinkPoolItem((rpmioItem)mi, __FUNCTION__, __FILE__, __LINE__);

    /* Chain cursors for teardown on abnormal exit. */
    mi->mi_next = rpmmiRock;
    rpmmiRock = mi;

    if (tag == RPMDBI_PACKAGES && keyp == NULL) {
	/* Special case #1: will iterate Packages database. */
	assert(keylen == 0);
	/* This should be the only case when (set == NULL). */
    }
    else if (tag == RPMDBI_PACKAGES) {
	/* Special case #2: will fetch header instance. */
	union _dbswap hdrNum;
	assert(keylen == sizeof(hdrNum.ui));
	memcpy(&hdrNum.ui, keyp, sizeof(hdrNum.ui));
	/* The set has only one element, which is hdrNum. */
	set = xcalloc(1, sizeof(*set));
	set->count = 1;
	set->recs = xcalloc(1, sizeof(set->recs[0]));
	set->recs[0].hdrNum = hdrNum.ui;
    }
    else if (keyp == NULL) {
	/* XXX Special case #3: they want empty iterator,
	 * for use with rpmmiGrow(). */
	assert(keylen == 0);
    }
    else {
	/* Common case: retrieve join keys. */
	DBC * dbcursor = NULL;
	DBT k = DBT_INIT;
	DBT v = DBT_INIT;
	int rc;
	int xx;

	if (isLabel) {
	    xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, 0);
	    rc = dbiFindByLabel(dbi, dbcursor, &k, &v, keyp, &set);
	    xx = dbiCclose(dbi, dbcursor, 0);
	    dbcursor = NULL;
	} else if (tag == RPMTAG_BASENAMES) {
	    rc = rpmdbFindByFile(db, keyp, &k, &v, &set);
	} else {
	    xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, 0);

/*@-temptrans@*/
k.data = (void *) keyp;
/*@=temptrans@*/
k.size = (UINT32_T) keylen;
if (k.data && k.size == 0) k.size = (UINT32_T) strlen((char *)k.data);
if (k.data && k.size == 0) k.size++;	/* XXX "/" fixup. */

/*@-nullstate@*/
	    rc = dbiGet(dbi, dbcursor, &k, &v, DB_SET);
/*@=nullstate@*/
	    if (rc > 0) {
		rpmlog(RPMLOG_ERR,
			_("error(%d) getting records from %s index\n"),
			rc, tagName(dbi->dbi_rpmtag));
	    }

	    /* Join keys need to be native endian internally. */
	    if (rc == 0)
		(void) dbt2set(dbi, &v, &set);

	    xx = dbiCclose(dbi, dbcursor, 0);
	    dbcursor = NULL;
	}
	if (rc || set == NULL || set->count < 1) { /* error/not found */
	    set = dbiFreeIndexSet(set);
	    rpmmiRock = mi->mi_next;
	    mi->mi_next = NULL;
	    mi = (rpmmi)rpmioFreePoolItem((rpmioItem)mi, __FUNCTION__, __FILE__, __LINE__);
	    return NULL;
	}
    }

/*@-assignexpose@*/
    mi->mi_db = rpmdbLink(db, "matchIterator");
/*@=assignexpose@*/
    mi->mi_rpmtag = tag;

    mi->mi_dbc = NULL;
    mi->mi_set = set;
    mi->mi_setx = 0;
    mi->mi_h = NULL;
    mi->mi_sorted = 0;
    mi->mi_cflags = 0;
    mi->mi_modified = 0;
    mi->mi_prevoffset = 0;
    mi->mi_offset = 0;
    mi->mi_filenum = 0;
    mi->mi_nre = 0;
    mi->mi_re = NULL;

    mi->mi_ts = NULL;

/*@i@*/ return mi;
}

int rpmdbMireApply(rpmdb db, rpmTag tag, rpmMireMode mode, const char * pat,
		const char *** argvp)
{
    DBC * dbcursor = NULL;
    DBT * key = memset(alloca(sizeof(*key)), 0, sizeof(*key));
    DBT * data = memset(alloca(sizeof(*data)), 0, sizeof(*data));
    dbiIndex dbi;
    miRE mire = NULL;
    ARGV_t av = NULL;
    int ret = 1;		/* assume error */
    int rc;
    int xx;

    dbi = dbiOpen(db, tag, 0);
    if (dbi == NULL)
	goto exit;

    if (pat) {
	mire = mireNew(mode, 0);
	xx = mireRegcomp(mire, pat);
    }

    xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, 0);

    while ((rc = dbiGet(dbi, dbcursor, key, data, DB_NEXT)) == 0) {
	size_t ns = key->size;
	char * s = memcpy(xmalloc(ns+1), key->data, ns);

	s[ns] = '\0';
	if (mire == NULL || mireRegexec(mire, s, ns) >= 0)
	    xx = argvAdd(&av, s);
	s = _free(s);
    }

    xx = dbiCclose(dbi, dbcursor, 0);
    dbcursor = NULL;

    if (rc > 0) {
	rpmlog(RPMLOG_ERR, _("error(%d) getting keys from %s index\n"),
		rc, tagName(dbi->dbi_rpmtag));
	goto exit;
    }

    ret = 0;

exit:
    if (argvp != NULL)
	xx = argvAppend(argvp, av);
    av = argvFree(av);
    mire = mireFree(mire);
    return ret;
}

/* XXX psm.c */
int rpmdbRemove(rpmdb db, /*@unused@*/ int rid, unsigned int hdrNum,
		/*@unused@*/ rpmts ts)
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    Header h;
    sigset_t signalMask;
    int ret = 0;
    int rc = 0;
    int xx;

    if (db == NULL)
	return 0;

    {	rpmmi mi;
	mi = rpmmiInit(db, RPMDBI_PACKAGES, &hdrNum, sizeof(hdrNum));
	h = rpmmiNext(mi);
	if (h)
	    h = headerLink(h);
	mi = rpmmiFree(mi);
    }

    if (h == NULL) {
	rpmlog(RPMLOG_ERR, _("%s: cannot read header at 0x%x\n"),
	      "rpmdbRemove", hdrNum);
	return 1;
    }

#ifdef	DYING
    /* Add remove transaction id to header. */
    if (rid != 0 && rid != -1) {
	rpmuint32_t tid[2];
	tid[0] = rid;
	tid[1] = 0;
	he->tag = RPMTAG_REMOVETID;
	he->t = RPM_UINT32_TYPE;
	he->p.ui32p = tid;
	he->c = 2;
	xx = headerPut(h, he, 0);
    }
#endif

    he->tag = RPMTAG_NVRA;
    xx = headerGet(h, he, 0);
    rpmlog(RPMLOG_DEBUG, "  --- h#%8u %s\n", hdrNum, he->p.str);
    he->p.ptr = _free(he->p.ptr);

    (void) blockSignals(db, &signalMask);

/*@-nullpass -nullptrarith -nullderef @*/ /* FIX: rpmvals heartburn */
    {	dbiIndexItem rec = dbiIndexNewItem(hdrNum, 0);
	size_t dbix;

	if (db->db_tags != NULL)
	for (dbix = 0; dbix < db->db_ndbi; dbix++) {
	    dbiIndex dbi;
	    DBC * dbcursor = NULL;
	    DBT k = DBT_INIT;
	    DBT v = DBT_INIT;
	    union _dbswap mi_offset;

	    tagStore_t dbiTag = db->db_tags + dbix;
	    rpmTag tag = dbiTag->tag;
	    const char * dbiBN = (dbiTag->str != NULL
		? dbiTag->str : tagName(tag));
	    rpmuint8_t * bin = NULL;
	    int i;

	    dbi = NULL;
	    he->tag = tag;
	    he->t = 0;
	    he->p.ptr = NULL;
	    he->c = 0;

	    switch (he->tag) {
	    /* Filter out temporary databases */
	    case RPMDBI_AVAILABLE:
	    case RPMDBI_ADDED:
	    case RPMDBI_REMOVED:
	    case RPMDBI_DEPENDS:
		continue;
		/*@notreached@*/ /*@switchbreak@*/ break;
	    case RPMDBI_PACKAGES:
		if (db->db_export != NULL)
		    xx = db->db_export(db, h, 0);
		dbi = dbiOpen(db, he->tag, 0);
		if (dbi == NULL)	/* XXX shouldn't happen */
		    continue;
	      
/*@-immediatetrans@*/
mi_offset.ui = hdrNum;
if (dbiByteSwapped(dbi) == 1)
    _DBSWAP(mi_offset);
		k.data = &mi_offset;
/*@=immediatetrans@*/
		k.size = (UINT32_T) sizeof(mi_offset.ui);

		rc = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, DB_WRITECURSOR);
		rc = dbiGet(dbi, dbcursor, &k, &v, DB_SET);
		if (rc) {
		    rpmlog(RPMLOG_ERR,
			_("error(%d) setting header #%d record for %s removal\n"),
			rc, hdrNum, dbiBN);
		} else
		    rc = dbiDel(dbi, dbcursor, &k, &v, 0);
		xx = dbiCclose(dbi, dbcursor, DB_WRITECURSOR);
		dbcursor = NULL;
		if (!dbi->dbi_no_dbsync)
		    xx = dbiSync(dbi, 0);
		continue;
		/*@notreached@*/ /*@switchbreak@*/ break;
	    default:
		xx = headerGet(h, he, 0);
		if (!xx)
		    continue;
		/*@switchbreak@*/ break;
	    }
	
	  dbi = dbiOpen(db, he->tag, 0);
	  if (dbi != NULL) {
	    int printed;

	    /* XXX Coerce strings into header argv return. */
	    if (he->t == RPM_STRING_TYPE) {
		const char * s = he->p.str;
		char * t;
		he->c = 1;
		he->p.argv = xcalloc(1, sizeof(*he->p.argv)+strlen(s)+1);
		he->p.argv[0] = t = (char *) &he->p.argv[1];
		(void) strcpy(t, s);
		s = _free(s);
	    }

	    printed = 0;
	    xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, DB_WRITECURSOR);
	    for (i = 0; i < (int) he->c; i++) {
		dbiIndexSet set;
		int stringvalued;

		bin = _free(bin);
		switch (dbi->dbi_rpmtag) {
		case RPMTAG_FILEDIGESTS:
		    /* Filter out empty file digests. */
		    if (!(he->p.argv[i] && *he->p.argv[i] != '\0'))
			/*@innercontinue@*/ continue;
		    /*@switchbreak@*/ break;
		default:
		    /*@switchbreak@*/ break;
		}

		/* Identify value pointer and length. */
		stringvalued = 0;
		switch (he->t) {
		case RPM_UINT8_TYPE:
		    k.size = (UINT32_T) sizeof(*he->p.ui8p);
/*@i@*/		    k.data = he->p.ui8p + i;
		    /*@switchbreak@*/ break;
		case RPM_UINT16_TYPE:
		    k.size = (UINT32_T) sizeof(*he->p.ui16p);
/*@i@*/		    k.data = he->p.ui16p + i;
		    /*@switchbreak@*/ break;
		case RPM_UINT32_TYPE:
		    k.size = (UINT32_T) sizeof(*he->p.ui32p);
/*@i@*/		    k.data = he->p.ui32p + i;
		    /*@switchbreak@*/ break;
		case RPM_UINT64_TYPE:
		    k.size = (UINT32_T) sizeof(*he->p.ui64p);
/*@i@*/		    k.data = he->p.ui64p + i;
		    /*@switchbreak@*/ break;
		case RPM_BIN_TYPE:
		    k.size = (UINT32_T) he->c;
/*@i@*/		    k.data = he->p.ptr;
		    he->c = 1;		/* XXX break out of loop. */
		    /*@switchbreak@*/ break;
		case RPM_I18NSTRING_TYPE:	/* XXX never occurs. */
		case RPM_STRING_TYPE:
		    he->c = 1;		/* XXX break out of loop. */
		    /*@fallthrough@*/
		case RPM_STRING_ARRAY_TYPE:
		    /* Convert from hex to binary. */
		    if (dbi->dbi_rpmtag == RPMTAG_FILEDIGESTS) {
			const char * s = he->p.argv[i];
			size_t dlen = strlen(s);
			rpmuint8_t * t;
			unsigned j;
assert((dlen & 1) == 0);
			dlen /= 2;
			bin = t = xcalloc(1, dlen);
/*@-type@*/
			for (j = 0; j < (unsigned) dlen; j++, t++, s += 2)
			    *t = (rpmuint8_t) (nibble(s[0]) << 4) | nibble(s[1]);
/*@=type@*/
			k.data = bin;
			k.size = (UINT32_T) dlen;
			/*@switchbreak@*/ break;
		    }
		    /* Extract the pubkey id from the base64 blob. */
		    if (dbi->dbi_rpmtag == RPMTAG_PUBKEYS) {
			int nbin;
			bin = xcalloc(1, 32);
			nbin = pgpExtractPubkeyFingerprint(he->p.argv[i], bin);
			if (nbin <= 0)
			    /*@innercontinue@*/ continue;
			k.data = bin;
			k.size = (UINT32_T) nbin;
			/*@switchbreak@*/ break;
		    }
		    /*@fallthrough@*/
		default:
/*@i@*/		    k.data = (void *) he->p.argv[i];
		    k.size = (UINT32_T) strlen(he->p.argv[i]);
		    stringvalued = 1;
		    /*@switchbreak@*/ break;
		}

		if (!printed) {
		    if (he->c == 1 && stringvalued) {
			rpmlog(RPMLOG_DEBUG,
				D_("removing \"%s\" from %s index.\n"),
				(char *)k.data, dbiBN);
		    } else {
			rpmlog(RPMLOG_DEBUG,
				D_("removing %u entries from %s index.\n"),
				(unsigned) he->c, dbiBN);
		    }
		    printed++;
		}

		/* XXX
		 * This is almost right, but, if there are duplicate tag
		 * values, there will be duplicate attempts to remove
		 * the header instance. It's faster to just ignore errors
		 * than to do things correctly.
		 */

/* XXX with duplicates, an accurate data value and DB_GET_BOTH is needed. */

		set = NULL;

if (k.size == 0) k.size = (UINT32_T) strlen((char *)k.data);
if (k.size == 0) k.size++;	/* XXX "/" fixup. */

/*@-compmempass@*/
		rc = dbiGet(dbi, dbcursor, &k, &v, DB_SET);
		if (rc == 0) {			/* success */
		    (void) dbt2set(dbi, &v, &set);
		} else if (rc == DB_NOTFOUND) {	/* not found */
		    /*@innercontinue@*/ continue;
		} else {			/* error */
		    rpmlog(RPMLOG_ERR,
			_("error(%d) getting records from %s index\n"),
			rc, dbiBN);
		    ret += 1;
		    /*@innercontinue@*/ continue;
		}
/*@=compmempass@*/

		rc = dbiPruneSet(set, rec, 1, sizeof(*rec), 1);

		/* If nothing was pruned, then don't bother updating. */
		if (rc) {
		    set = dbiFreeIndexSet(set);
		    /*@innercontinue@*/ continue;
		}

/*@-compmempass@*/
		if (set->count > 0) {
		    (void) set2dbt(dbi, &v, set);
		    rc = dbiPut(dbi, dbcursor, &k, &v, DB_KEYLAST);
		    if (rc) {
			rpmlog(RPMLOG_ERR,
				_("error(%d) storing record into %s\n"),
				rc, dbiBN);
			ret += 1;
		    }
		    v.data = _free(v.data);
		    v.size = 0;
		} else {
		    rc = dbiDel(dbi, dbcursor, &k, &v, 0);
		    if (rc) {
			rpmlog(RPMLOG_ERR,
				_("error(%d) removing record from %s\n"),
				rc, dbiBN);
			ret += 1;
		    }
		}
/*@=compmempass@*/
		set = dbiFreeIndexSet(set);
	    }

	    xx = dbiCclose(dbi, dbcursor, DB_WRITECURSOR);
	    dbcursor = NULL;

	    if (!dbi->dbi_no_dbsync)
		xx = dbiSync(dbi, 0);
	  }

	    he->tag = 0;
	    he->t = 0;
	    he->p.ptr = _free(he->p.ptr);
	    he->c = 0;
	    bin = _free(bin);
	}

	rec = _free(rec);
    }
/*@=nullpass =nullptrarith =nullderef @*/

    (void) unblockSignals(db, &signalMask);

    (void)headerFree(h);
    h = NULL;

    /* XXX return ret; */
    return 0;
}

/* XXX install.c */
int rpmdbAdd(rpmdb db, int iid, Header h, /*@unused@*/ rpmts ts)
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    sigset_t signalMask;
    const char ** dirNames;
    rpmuint32_t * dirIndexes;
    dbiIndex dbi;
    size_t dbix;
    union _dbswap mi_offset;
    unsigned int hdrNum = 0;
    int ret = 0;
    int rc;
    int xx;

    /* Initialize the header instance */
    (void) headerSetInstance(h, 0);

    if (db == NULL)
	return 0;

#ifdef	NOTYET	/* XXX headerRemoveEntry() broken on dribbles. */
    he->tag = RPMTAG_REMOVETID;
    xx = headerDel(h, he, 0);
#endif
    if (iid != 0 && iid != -1) {
	rpmuint32_t tid[2];
	tid[0] = iid;
	tid[1] = 0;
	he->tag = RPMTAG_INSTALLTID;
	he->t = RPM_UINT32_TYPE;
	he->p.ui32p = tid;
	he->c = 2;
	if (!headerIsEntry(h, he->tag))
/*@-compmempass@*/
	   xx = headerPut(h, he, 0);
/*@=compmempass@*/
    }

    /* Add the package color if not present. */
    if (!headerIsEntry(h, RPMTAG_PACKAGECOLOR)) {
	rpmuint32_t hcolor = hGetColor(h);
	he->tag = RPMTAG_PACKAGECOLOR;
	he->t = RPM_UINT32_TYPE;
	he->p.ui32p = &hcolor;
	he->c = 1;
/*@-compmempass@*/
	xx = headerPut(h, he, 0);
/*@=compmempass@*/
    }

    he->tag = RPMTAG_DIRNAMES;
/*@-compmempass@*/
    xx = headerGet(h, he, 0);
/*@=compmempass@*/
    dirNames = he->p.argv;
    he->tag = RPMTAG_DIRINDEXES;
/*@-compmempass@*/
    xx = headerGet(h, he, 0);
/*@=compmempass@*/
    dirIndexes = he->p.ui32p;

    (void) blockSignals(db, &signalMask);

    dbi = dbiOpen(db, RPMDBI_PACKAGES, 0);
    if (dbi != NULL) {
	/* Header indices are monotonically increasing integer instances
	 * starting with 1.  Header instance #0 is where the monotonically
	 * increasing integer is stored.  */
	unsigned int idx0 = 0;

	DBC * dbcursor = NULL;
	DBT k = DBT_INIT;
	DBT v = DBT_INIT;

	xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, DB_WRITECURSOR);

	/* Retrieve join key for next header instance. */
	k.data = &idx0;
	k.size = (u_int32_t)sizeof(idx0);
	ret = dbiGet(dbi, dbcursor, &k, &v, DB_SET);

	hdrNum = 0;
	if (ret == 0 && v.data) {
	    memcpy(&mi_offset, v.data, sizeof(mi_offset.ui));
	    if (dbiByteSwapped(dbi) == 1)
		_DBSWAP(mi_offset);
	    hdrNum = (unsigned) mi_offset.ui;
	}
	++hdrNum;
	mi_offset.ui = hdrNum;
	if (dbiByteSwapped(dbi) == 1)
	    _DBSWAP(mi_offset);
	if (ret == 0 && v.data) {
	    memcpy(v.data, &mi_offset, sizeof(mi_offset.ui));
	} else {
/*@-immediatetrans@*/
	    v.data = &mi_offset;
/*@=immediatetrans@*/
	    v.size = (u_int32_t)sizeof(mi_offset.ui);
	}

/*@-compmempass@*/
	ret = dbiPut(dbi, dbcursor, &k, &v, DB_KEYLAST);
/*@=compmempass@*/
	xx = dbiSync(dbi, 0);

	xx = dbiCclose(dbi, dbcursor, DB_WRITECURSOR);

    }

    if (ret) {
	rpmlog(RPMLOG_ERR,
		_("error(%d) allocating new package instance\n"), ret);
	goto exit;
    }

    /* Now update the indexes */

    if (hdrNum)
    {	dbiIndexItem rec = dbiIndexNewItem(hdrNum, 0);

	/* Save the header instance. */
	(void) headerSetInstance(h, hdrNum);
	
	if (db->db_tags != NULL)
	for (dbix = 0; dbix < db->db_ndbi; dbix++) {
	    DBC * dbcursor = NULL;
	    DBT k = DBT_INIT;
	    DBT v = DBT_INIT;

	    tagStore_t dbiTag = db->db_tags + dbix;
	    const char * dbiBN = (dbiTag->str != NULL
			? dbiTag->str : tagName(dbiTag->tag));
	    rpmuint8_t * bin = NULL;
	    rpmTagData requireFlags;
	    rpmRC rpmrc;
	    int i;

	    rpmrc = RPMRC_NOTFOUND;
	    requireFlags.ptr = NULL;
	    dbi = NULL;
	    he->tag = dbiTag->tag;
	    he->t = 0;
	    he->p.ptr = NULL;
	    he->c = 0;

	    switch (he->tag) {
	    /* Filter out temporary databases */
	    case RPMDBI_AVAILABLE:
	    case RPMDBI_ADDED:
	    case RPMDBI_REMOVED:
	    case RPMDBI_DEPENDS:
		continue;
		/*@notreached@*/ /*@switchbreak@*/ break;
	    case RPMDBI_PACKAGES:
		if (db->db_export != NULL)
		    xx = db->db_export(db, h, 1);
		dbi = dbiOpen(db, he->tag, 0);
		if (dbi == NULL)	/* XXX shouldn't happen */
		    continue;
		xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, DB_WRITECURSOR);

		mi_offset.ui = hdrNum;
		if (dbiByteSwapped(dbi) == 1)
		    _DBSWAP(mi_offset);
		/*@-immediatetrans@*/
		k.data = (void *) &mi_offset;
		/*@=immediatetrans@*/
		k.size = (UINT32_T) sizeof(mi_offset.ui);
		{   size_t len = 0;
		    v.data = headerUnload(h, &len);
		    v.size = (UINT32_T) len;
		}

		/* Check header digest/signature on blob export. */
		if (ts) {
		    const char * msg = NULL;
		    int lvl;

assert(v.data != NULL);
		    rpmrc = headerCheck(rpmtsDig(ts), v.data, v.size, &msg);
		    rpmtsCleanDig(ts);
		    lvl = (rpmrc == RPMRC_FAIL ? RPMLOG_ERR : RPMLOG_DEBUG);
		    rpmlog(lvl, "%s h#%8u %s\n",
			(rpmrc == RPMRC_FAIL ? _("rpmdbAdd: skipping") : "  +++"),
				hdrNum, (msg ? msg : ""));
		    msg = _free(msg);
		}

		if (v.data != NULL && rpmrc != RPMRC_FAIL) {
/*@-compmempass@*/
		    xx = dbiPut(dbi, dbcursor, &k, &v, DB_KEYLAST);
/*@=compmempass@*/
		    xx = dbiSync(dbi, 0);
		}
		v.data = _free(v.data); /* headerUnload */
		v.size = 0;
		xx = dbiCclose(dbi, dbcursor, DB_WRITECURSOR);
		if (!dbi->dbi_no_dbsync)
		    xx = dbiSync(dbi, 0);
		continue;
		/*@notreached@*/ /*@switchbreak@*/ break;
	    case RPMTAG_REQUIRENAME:
		he->tag = RPMTAG_REQUIREFLAGS;
/*@-compmempass@*/
		xx = headerGet(h, he, 0);
/*@=compmempass@*/
		requireFlags.ptr = he->p.ptr;
		he->tag = RPMTAG_REQUIRENAME;
/*@-compmempass@*/
		xx = headerGet(h, he, 0);
/*@=compmempass@*/
		/*@switchbreak@*/ break;
	    default:
/*@-compmempass@*/
		xx = headerGet(h, he, 0);
/*@=compmempass@*/
		/*@switchbreak@*/ break;
	    }

	    /* Anything to do? */
	    if (he->c == 0)
		continue;

	  dbi = dbiOpen(db, he->tag, 0);
	  if (dbi != NULL) {
	    int printed;

	    /* XXX Coerce strings into header argv return. */
	    if (he->t == RPM_STRING_TYPE) {
		const char * s = he->p.str;
		char * t;
		he->c = 1;
		he->p.argv = xcalloc(1, sizeof(*he->p.argv)+strlen(s)+1);
		he->p.argv[0] = t = (char *) &he->p.argv[1];
		(void) strcpy(t, s);
		s = _free(s);
	    }

	    printed = 0;
	    xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, DB_WRITECURSOR);

	    for (i = 0; i < (int) he->c; i++) {
		dbiIndexSet set;
		int stringvalued;

		bin = _free(bin);
		/*
		 * Include the tagNum in all indices. rpm-3.0.4 and earlier
		 * included the tagNum only for files.
		 */
		rec->tagNum = i;
		switch (dbi->dbi_rpmtag) {
		case RPMTAG_BASENAMES:
		    /* tag index entry with directory hash */
		    if (_db_tagged_file_indices && i < 0x010000)
			rec->tagNum |= taghash(dirNames[dirIndexes[i]]);
		    /*@switchbreak@*/ break;
		case RPMTAG_PUBKEYS:
		    /*@switchbreak@*/ break;
		case RPMTAG_FILEDIGESTS:
		    /* Filter out empty MD5 strings. */
		    if (!(he->p.argv[i] && *he->p.argv[i] != '\0'))
			/*@innercontinue@*/ continue;
		    /*@switchbreak@*/ break;
		case RPMTAG_REQUIRENAME:
		    /* Filter out install prerequisites. */
		    if (requireFlags.ui32p
		     && isInstallPreReq(requireFlags.ui32p[i]))
			/*@innercontinue@*/ continue;
		    /*@switchbreak@*/ break;
		case RPMTAG_TRIGGERNAME:
		    if (i) {	/* don't add duplicates */
			int j;
			for (j = 0; j < i; j++) {
			    if (!strcmp(he->p.argv[i], he->p.argv[j]))
				/*@innerbreak@*/ break;
			}
			if (j < i)
			    /*@innercontinue@*/ continue;
		    }
		    /*@switchbreak@*/ break;
		default:
		    /*@switchbreak@*/ break;
		}

		/* Identify value pointer and length. */
		stringvalued = 0;
		switch (he->t) {
		case RPM_UINT8_TYPE:
		    k.size = (UINT32_T) sizeof(*he->p.ui8p);
/*@i@*/		    k.data = he->p.ui8p + i;
		    /*@switchbreak@*/ break;
		case RPM_UINT16_TYPE:
		    k.size = (UINT32_T) sizeof(*he->p.ui16p);
/*@i@*/		    k.data = he->p.ui16p + i;
		    /*@switchbreak@*/ break;
		case RPM_UINT32_TYPE:
		    k.size = (UINT32_T) sizeof(*he->p.ui32p);
/*@i@*/		    k.data = he->p.ui32p + i;
		    /*@switchbreak@*/ break;
		case RPM_UINT64_TYPE:
		    k.size = (UINT32_T) sizeof(*he->p.ui64p);
/*@i@*/		    k.data = he->p.ui64p + i;
		    /*@switchbreak@*/ break;
		case RPM_BIN_TYPE:
		    k.size = (UINT32_T) he->c;
/*@i@*/		    k.data = he->p.ptr;
		    he->c = 1;		/* XXX break out of loop. */
		    /*@switchbreak@*/ break;
		case RPM_I18NSTRING_TYPE:	/* XXX never occurs */
		case RPM_STRING_TYPE:
		    he->c = 1;		/* XXX break out of loop. */
		    /*@fallthrough@*/
		case RPM_STRING_ARRAY_TYPE:
		    /* Convert from hex to binary. */
		    if (dbi->dbi_rpmtag == RPMTAG_FILEDIGESTS) {
			const char * s = he->p.argv[i];
			size_t dlen = strlen(s);
			rpmuint8_t * t;
			unsigned j;
assert((dlen & 1) == 0);
			dlen /= 2;
			bin = t = xcalloc(1, dlen);
/*@-type@*/
			for (j = 0; j < (unsigned) dlen; j++, t++, s += 2)
			    *t = (rpmuint8_t) (nibble(s[0]) << 4) | nibble(s[1]);
/*@=type@*/
			k.data = bin;
			k.size = (UINT32_T) dlen;
			/*@switchbreak@*/ break;
		    }
		    /* Extract the pubkey id from the base64 blob. */
		    if (dbi->dbi_rpmtag == RPMTAG_PUBKEYS) {
			int nbin;
			bin = xcalloc(1, 32);
			nbin = pgpExtractPubkeyFingerprint(he->p.argv[i], bin);
			if (nbin <= 0)
			    /*@innercontinue@*/ continue;
			k.data = bin;
			k.size = (UINT32_T) nbin;
			/*@switchbreak@*/ break;
		    }
		    /*@fallthrough@*/
		default:
/*@i@*/		    k.data = (void *) he->p.argv[i];
		    k.size = (UINT32_T) strlen(he->p.argv[i]);
		    stringvalued = 1;
		    /*@switchbreak@*/ break;
		}

		if (!printed) {
		    if (he->c == 1 && stringvalued) {
			rpmlog(RPMLOG_DEBUG,
				D_("adding \"%s\" to %s index.\n"),
				(char *)k.data, dbiBN);
		    } else {
			rpmlog(RPMLOG_DEBUG,
				D_("adding %u entries to %s index.\n"),
				(unsigned)he->c, dbiBN);
		    }
		    printed++;
		}

/* XXX with duplicates, an accurate data value and DB_GET_BOTH is needed. */

		set = NULL;

if (k.size == 0) k.size = (UINT32_T) strlen((char *)k.data);
if (k.size == 0) k.size++;	/* XXX "/" fixup. */

/*@-compmempass@*/
		rc = dbiGet(dbi, dbcursor, &k, &v, DB_SET);
		if (rc == 0) {			/* success */
		/* With duplicates, cursor is positioned, discard the record. */
		    if (!dbi->dbi_permit_dups)
			(void) dbt2set(dbi, &v, &set);
		} else if (rc != DB_NOTFOUND) {	/* error */
		    rpmlog(RPMLOG_ERR,
			_("error(%d) getting records from %s index\n"),
			rc, dbiBN);
		    ret += 1;
		    /*@innercontinue@*/ continue;
		}
/*@=compmempass@*/

		if (set == NULL)		/* not found or duplicate */
		    set = xcalloc(1, sizeof(*set));

		(void) dbiAppendSet(set, rec, 1, sizeof(*rec), 0);

/*@-compmempass@*/
		(void) set2dbt(dbi, &v, set);
		rc = dbiPut(dbi, dbcursor, &k, &v, DB_KEYLAST);
/*@=compmempass@*/

		if (rc) {
		    rpmlog(RPMLOG_ERR,
				_("error(%d) storing record into %s\n"),
				rc, dbiBN);
		    ret += 1;
		}
/*@-unqualifiedtrans@*/
		v.data = _free(v.data);
/*@=unqualifiedtrans@*/
		v.size = 0;
		set = dbiFreeIndexSet(set);
	    }

	    xx = dbiCclose(dbi, dbcursor, DB_WRITECURSOR);

	    if (!dbi->dbi_no_dbsync)
		xx = dbiSync(dbi, 0);
	  }

	    he->tag = 0;
	    he->t = 0;
/*@-kepttrans -onlytrans@*/
	    he->p.ptr = _free(he->p.ptr);
/*@=kepttrans =onlytrans@*/
	    he->c = 0;
	    bin = _free(bin);
	    requireFlags.ptr = _free(requireFlags.ptr);
	}

	rec = _free(rec);
    }

exit:
    (void) unblockSignals(db, &signalMask);
    dirIndexes = _free(dirIndexes);
    dirNames = _free(dirNames);

    return ret;
}

/* XXX transaction.c */
/*@-compmempass@*/
int rpmdbFindFpList(void * _db, fingerPrint * fpList, void * _matchList, 
		    int numItems, unsigned int exclude)
{
    rpmdb db = _db;
    dbiIndexSet * matchList = _matchList;
DBT * key;
DBT * data;
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    rpmmi mi;
    fingerPrintCache fpc;
    Header h;
    int i, xx;

    if (db == NULL) return 0;

    mi = rpmmiInit(db, RPMTAG_BASENAMES, NULL, 0);
assert(mi != NULL);	/* XXX will never happen. */
    if (mi == NULL)
	return 2;

key = &mi->mi_key;
data = &mi->mi_data;

    /* Gather all installed headers with matching basename's. */
    for (i = 0; i < numItems; i++) {
	unsigned int tag;

	matchList[i] = xcalloc(1, sizeof(*(matchList[i])));

/*@-dependenttrans@*/
key->data = (void *) fpList[i].baseName;
/*@=dependenttrans@*/
key->size = (UINT32_T) strlen((char *)key->data);
if (key->size == 0) key->size++;	/* XXX "/" fixup. */

	tag = (_db_tagged_file_indices ? taghash(fpList[i].entry->dirName) : 0);
	xx = rpmdbGrowIterator(mi, i, exclude, tag);

    }

    if ((i = rpmmiCount(mi)) == 0) {
	mi = rpmmiFree(mi);
	return 0;
    }
    fpc = fpCacheCreate(i);

    rpmdbSortIterator(mi);
    /* iterator is now sorted by (recnum, filenum) */

    /* For all installed headers with matching basename's ... */
    if (mi != NULL)
    while ((h = rpmmiNext(mi)) != NULL) {
	const char ** dirNames;
	const char ** baseNames;
	const char ** fullBaseNames;
	rpmuint32_t * dirIndexes;
	rpmuint32_t * fullDirIndexes;
	fingerPrint * fps;
	dbiIndexItem im;
	int start;
	int num;
	int end;

	start = mi->mi_setx - 1;
	im = mi->mi_set->recs + start;

	/* Find the end of the set of matched basename's in this package. */
	for (end = start + 1; end < mi->mi_set->count; end++) {
	    if (im->hdrNum != mi->mi_set->recs[end].hdrNum)
		/*@innerbreak@*/ break;
	}
	num = end - start;

	/* Compute fingerprints for this installed header's matches */
	he->tag = RPMTAG_BASENAMES;
	xx = headerGet(h, he, 0);
	fullBaseNames = he->p.argv;
	he->tag = RPMTAG_DIRNAMES;
	xx = headerGet(h, he, 0);
	dirNames = he->p.argv;
	he->tag = RPMTAG_DIRINDEXES;
	xx = headerGet(h, he, 0);
	fullDirIndexes = he->p.ui32p;

	baseNames = xcalloc(num, sizeof(*baseNames));
	dirIndexes = xcalloc(num, sizeof(*dirIndexes));
	for (i = 0; i < num; i++) {
	    baseNames[i] = fullBaseNames[im[i].tagNum];
	    dirIndexes[i] = fullDirIndexes[im[i].tagNum];
	}

	fps = xcalloc(num, sizeof(*fps));
	fpLookupList(fpc, dirNames, baseNames, dirIndexes, num, fps);

	/* Add db (recnum,filenum) to list for fingerprint matches. */
	for (i = 0; i < num; i++, im++) {
	    /*@-nullpass@*/ /* FIX: fpList[].subDir may be NULL */
	    if (!FP_EQUAL(fps[i], fpList[im->fpNum]))
		/*@innercontinue@*/ continue;
	    /*@=nullpass@*/
	    xx = dbiAppendSet(matchList[im->fpNum], im, 1, sizeof(*im), 0);
	}

	fps = _free(fps);
	fullBaseNames = _free(fullBaseNames);
/*@-usereleased@*/
	dirNames = _free(dirNames);
/*@=usereleased@*/
	fullDirIndexes = _free(fullDirIndexes);
	baseNames = _free(baseNames);
	dirIndexes = _free(dirIndexes);

	mi->mi_setx = end;
    }

    mi = rpmmiFree(mi);

    fpc = fpCacheFree(fpc);

    return 0;

}
/*@=compmempass@*/

/**
 * Check if file exists using stat(2).
 * @param urlfn		file name (may be URL)
 * @return		1 if file exists, 0 if not
 */
static int rpmioFileExists(const char * urlfn)
        /*@globals h_errno, fileSystem, internalState @*/
        /*@modifies fileSystem, internalState @*/
{
    struct stat sb;
    const char * fn;
    int urltype = urlPath(urlfn, &fn);
    int rc = 0;		/* assume failure. */

    if (*fn == '\0') fn = "/";
    switch (urltype) {
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    case URL_IS_FTP:
    case URL_IS_HKP:
	rc = Stat(urlfn, &sb) == 0;
	break;
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
	rc = Stat(fn, &sb) == 0;
	break;
    case URL_IS_DASH:
    default:
	break;
    }

    return rc;
}

static int rpmdbRemoveDatabase(const char * prefix,
		const char * dbpath, int _dbapi,
		/*@null@*/ const tagStore_t dbiTags, size_t dbiNTags)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext, fileSystem, internalState @*/
{ 
    const char * fn;
    int xx;

    switch (_dbapi) {
    default:
    case 4:
	/*@fallthrough@*/
    case 3:
    {	char * suffix;
	size_t i;

	if (dbiTags != NULL)
	for (i = 0; i < dbiNTags; i++) {
	    const char * dbiBN = (dbiTags[i].str != NULL
                        ? dbiTags[i].str : tagName(dbiTags[i].tag));
	    fn = rpmGetPath(prefix, dbpath, "/", dbiBN, NULL);
	    if (rpmioFileExists(fn))
		xx = Unlink(fn);
	    fn = _free(fn);
	}

	fn = rpmGetPath(prefix, dbpath, "/", "__db.000", NULL);
	suffix = (char *)(fn + strlen(fn) - (sizeof("000") - 1));
	for (i = 0; i < 16; i++) {
	    (void) snprintf(suffix, sizeof("000"), "%03u", (unsigned)i);
	    if (rpmioFileExists(fn))
		xx = Unlink(fn);
	}
	fn = _free(fn);

    }	break;
    case 2:
    case 1:
    case 0:
	break;
    }

    fn = rpmGetPath(prefix, dbpath, NULL);
    xx = Rmdir(fn);
    fn = _free(fn);

    return 0;
}

static int rpmdbMoveDatabase(const char * prefix,
		const char * olddbpath, int _olddbapi,
		const char * newdbpath, /*@unused@*/ int _newdbapi,
		/*@null@*/ const tagStore_t dbiTags, size_t dbiNTags)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext, fileSystem, internalState @*/
{
    struct stat nsb, * nst = &nsb;
    const char * ofn, * nfn;
    int rc = 0;
    int xx;
/*@-moduncon -noeffectuncon@*/
    int selinux = is_selinux_enabled() > 0 && (matchpathcon_init(NULL) != -1);
/*@=moduncon =noeffectuncon@*/
    sigset_t sigMask;
 
    (void) blockSignals(NULL, &sigMask);
    switch (_olddbapi) {
    default:
    case 4:
        /* Fall through */
    case 3:
    {	char *osuffix, *nsuffix;
	size_t i;
	if (dbiTags != NULL)
	for (i = 0; i < dbiNTags; i++) {
	    rpmTag tag = dbiTags[i].tag;
	    const char * dbiBN = (dbiTags[i].str != NULL
                        ? dbiTags[i].str : tagName(tag));

	    /* Filter out temporary databases */
	    switch (tag) {
	    case RPMDBI_AVAILABLE:
	    case RPMDBI_ADDED:
	    case RPMDBI_REMOVED:
	    case RPMDBI_DEPENDS:
		continue;
		/*@notreached@*/ /*@switchbreak@*/ break;
	    default:
		/*@switchbreak@*/ break;
	    }

	    ofn = rpmGetPath(prefix, olddbpath, "/", dbiBN, NULL);
	    nfn = rpmGetPath(prefix, newdbpath, "/", dbiBN, NULL);

	    if (!rpmioFileExists(ofn)) {
	        if (rpmioFileExists(nfn)) {
		    rpmlog(RPMLOG_DEBUG, D_("removing file \"%s\"\n"), nfn);
		    xx = Unlink(nfn);
                }
		goto bottom;
            }

	    /*
	     * Get uid/gid/mode/mtime. If old doesn't exist, use new.
	     * XXX Yes, the variable names are backwards.
	     */
	    if (Stat(nfn, nst) < 0 && Stat(ofn, nst) < 0)
		goto bottom;

	    rpmlog(RPMLOG_DEBUG, D_("moving file from \"%s\"\n"), ofn);
	    rpmlog(RPMLOG_DEBUG, D_("moving file to   \"%s\"\n"), nfn);
	    if ((xx = Rename(ofn, nfn)) != 0) {
		rc = 1;
		goto bottom;
	    }

	    /* Restore uid/gid/mode/mtime/security context if possible. */
	    xx = Chown(nfn, nst->st_uid, nst->st_gid);
	    xx = Chmod(nfn, (nst->st_mode & 07777));
	    {	struct utimbuf stamp;
/*@-type@*/
		stamp.actime = (time_t)nst->st_atime;
		stamp.modtime = (time_t)nst->st_mtime;
/*@=type@*/
		xx = Utime(nfn, &stamp);
	    }
/*@-moduncon -noeffectuncon@*/
	    if (selinux) {
		security_context_t scon = NULL;
		if (matchpathcon(nfn, nst->st_mode, &scon) != -1)
		    xx = setfilecon(nfn, scon);
		if (scon != NULL)
		    freecon(scon);
	    }
/*@=moduncon =noeffectuncon@*/

bottom:
	    ofn = _free(ofn);
	    nfn = _free(nfn);
	}

	ofn = rpmGetPath(prefix, olddbpath, "/", "__db.000", NULL);
	osuffix = (char *)(ofn + strlen(ofn) - (sizeof("000") - 1));
	nfn = rpmGetPath(prefix, newdbpath, "/", "__db.000", NULL);
	nsuffix = (char *)(nfn + strlen(nfn) - (sizeof("000") - 1));

	for (i = 0; i < 16; i++) {
	    (void) snprintf(osuffix, sizeof("000"), "%03u", (unsigned)i);
	    if (rpmioFileExists(ofn)) {
		rpmlog(RPMLOG_DEBUG, D_("removing region file \"%s\"\n"), ofn);
		xx = Unlink(ofn);
	    }
	    (void) snprintf(nsuffix, sizeof("000"), "%03u", (unsigned)i);
	    if (rpmioFileExists(nfn)) {
		rpmlog(RPMLOG_DEBUG, D_("removing region file \"%s\"\n"), nfn);
		xx = Unlink(nfn);
	    }
	}
	ofn = _free(ofn);
	nfn = _free(ofn);
    }	break;
    case 2:
    case 1:
    case 0:
	break;
    }
    (void) unblockSignals(NULL, &sigMask);

/*@-moduncon -noeffectuncon@*/
    if (selinux)
	matchpathcon_fini();
/*@=moduncon =noeffectuncon@*/
    return rc;
}

int rpmdbRebuild(const char * prefix, rpmts ts)
	/*@globals _rebuildinprogress @*/
	/*@modifies _rebuildinprogress @*/
{
    const char * myprefix = NULL;
    rpmdb olddb;
    const char * dbpath = NULL;
    const char * rootdbpath = NULL;
    rpmdb newdb;
    const char * newdbpath = NULL;
    const char * newrootdbpath = NULL;
    const char * tfn;
    int nocleanup = 1;
    int failed = 0;
    int removedir = 0;
    int rc = 0, xx;
    int _dbapi;
    int _dbapi_rebuild;
    tagStore_t dbiTags = NULL;
    size_t dbiNTags = 0;

    _dbapi = rpmExpandNumeric("%{_dbapi}");
    _dbapi_rebuild = rpmExpandNumeric("%{_dbapi_rebuild}");

    dbiTagsInit(&dbiTags, &dbiNTags);

    /*@-nullpass@*/
    tfn = rpmGetPath("%{?_dbpath}", NULL);
    /*@=nullpass@*/
    if (!(tfn && tfn[0] != '\0'))
    {
	rpmlog(RPMLOG_DEBUG, D_("no dbpath has been set"));
	rc = 1;
	goto exit;
    }

    /* Add --root prefix iff --dbpath is not a URL. */
    switch (urlPath(tfn, NULL)) {
    default:
	myprefix = xstrdup("");
	break;
    case URL_IS_UNKNOWN:
	myprefix = rpmGetPath((prefix ? prefix : "/"), NULL);
	break;
    }

    dbpath = rootdbpath = rpmGetPath(myprefix, tfn, NULL);
    if (!(myprefix[0] == '/' && myprefix[1] == '\0'))
	dbpath += strlen(myprefix);
    tfn = _free(tfn);

    /*@-nullpass@*/
    tfn = rpmGetPath("%{?_dbpath_rebuild}", NULL);
    /*@=nullpass@*/
    if (!(tfn && tfn[0] != '\0' && strcmp(tfn, dbpath)))
    {
	char pidbuf[20];
	char *t;
	sprintf(pidbuf, "rebuilddb.%d", (int) getpid());
	t = xmalloc(strlen(dbpath) + strlen(pidbuf) + 1);
	(void)stpcpy(stpcpy(t, dbpath), pidbuf);
	tfn = _free(tfn);
	tfn = t;
	nocleanup = 0;
    }
    newdbpath = newrootdbpath = rpmGetPath(myprefix, tfn, NULL);
    if (!(myprefix[0] == '/' && myprefix[1] == '\0'))
	newdbpath += strlen(myprefix);
    tfn = _free(tfn);

    rpmlog(RPMLOG_DEBUG, D_("rebuilding database %s into %s\n"),
	rootdbpath, newrootdbpath);

    if (!Access(newrootdbpath, F_OK)) {
	rpmlog(RPMLOG_ERR, _("temporary database %s already exists\n"),
	      newrootdbpath);
	rc = 1;
	goto exit;
    }

    rpmlog(RPMLOG_DEBUG, D_("creating directory %s\n"), newrootdbpath);
    if (Mkdir(newrootdbpath, 0755)) {
	rpmlog(RPMLOG_ERR, _("creating directory %s: %s\n"),
	      newrootdbpath, strerror(errno));
	rc = 1;
	goto exit;
    }
    removedir = 1;

    _rebuildinprogress = 0;

    rpmlog(RPMLOG_DEBUG, D_("opening old database with dbapi %d\n"),
		_dbapi);
    if (rpmdbOpenDatabase(myprefix, dbpath, _dbapi, &olddb, O_RDONLY, 0644,
		     RPMDB_FLAG_MINIMAL)) {
	rc = 1;
	goto exit;
    }
    _dbapi = olddb->db_api;
    _rebuildinprogress = 1;
    rpmlog(RPMLOG_DEBUG, D_("opening new database with dbapi %d\n"),
		_dbapi_rebuild);
    (void) rpmDefineMacro(NULL, "_rpmdb_rebuild %{nil}", -1);
    if (rpmdbOpenDatabase(myprefix, newdbpath, _dbapi_rebuild, &newdb, O_RDWR | O_CREAT, 0644, 0)) {
	rc = 1;
	goto exit;
    }

    _rebuildinprogress = 0;

    _dbapi_rebuild = newdb->db_api;
    
    {	Header h = NULL;
	rpmmi mi;
#define	_RECNUM	rpmmiInstance(mi)

	mi = rpmmiInit(olddb, RPMDBI_PACKAGES, NULL, 0);
	if (ts)
	    (void) rpmmiSetHdrChk(mi, ts);

	while ((h = rpmmiNext(mi)) != NULL) {

	    /* let's sanity check this record a bit, otherwise just skip it */
	    if (!(headerIsEntry(h, RPMTAG_NAME) &&
		headerIsEntry(h, RPMTAG_VERSION) &&
		headerIsEntry(h, RPMTAG_RELEASE) &&
		headerIsEntry(h, RPMTAG_BUILDTIME)))
	    {
		rpmlog(RPMLOG_WARNING,
			_("header #%u in the database is bad -- skipping.\n"),
			_RECNUM);
		continue;
	    }
	    if (!headerIsEntry(h, RPMTAG_SOURCERPM)
	     &&  headerIsEntry(h, RPMTAG_ARCH))
	    {
		rpmlog(RPMLOG_WARNING,
			_("header #%u in the database is SRPM -- skipping.\n"),
			_RECNUM);
		continue;
	    }

	    /* Filter duplicate entries ? (bug in pre rpm-3.0.4) */
	    if (_db_filter_dups || newdb->db_filter_dups) {
		const char * name, * version, * release;
		int skip = 0;

		(void) headerNEVRA(h, &name, NULL, &version, &release, NULL);

		/*@-shadow@*/
		{   rpmmi mi;
		    mi = rpmmiInit(newdb, RPMTAG_NAME, name, 0);
		    (void) rpmmiAddPattern(mi, RPMTAG_VERSION,
				RPMMIRE_DEFAULT, version);
		    (void) rpmmiAddPattern(mi, RPMTAG_RELEASE,
				RPMMIRE_DEFAULT, release);
		    while (rpmmiNext(mi)) {
			skip = 1;
			/*@innerbreak@*/ break;
		    }
		    mi = rpmmiFree(mi);
		}
		/*@=shadow@*/

		if (skip)
		    continue;
	    }

	    /* Deleted entries are eliminated in legacy headers by copy. */
	    {	Header nh = (headerIsEntry(h, RPMTAG_HEADERIMAGE)
				? headerCopy(h) : NULL);
		rc = rpmdbAdd(newdb, -1, (nh ? nh : h), ts);
		(void)headerFree(nh);
		nh = NULL;
	    }

	    if (rc) {
		rpmlog(RPMLOG_ERR,
			_("cannot add record originally at %u\n"), _RECNUM);
		failed = 1;
		break;
	    }
	}

	mi = rpmmiFree(mi);

    }

    xx = rpmdbClose(olddb);
    xx = rpmdbClose(newdb);

    if (failed) {
	rpmlog(RPMLOG_NOTICE, _("failed to rebuild database: original database "
		"remains in place\n"));

	xx = rpmdbRemoveDatabase(myprefix, newdbpath, _dbapi_rebuild,
			dbiTags, dbiNTags);
	rc = 1;
	goto exit;
    } else if (!nocleanup) {
	xx = rpmdbMoveDatabase(myprefix, newdbpath, _dbapi_rebuild, dbpath, _dbapi,
			dbiTags, dbiNTags);

	if (xx) {
	    rpmlog(RPMLOG_ERR, _("failed to replace old database with new "
			"database!\n"));
	    rpmlog(RPMLOG_ERR, _("replace files in %s with files from %s "
			"to recover"), dbpath, newdbpath);
	    rc = 1;
	    goto exit;
	}
    }
    rc = 0;

exit:
    if (removedir && !(rc == 0 && nocleanup)) {
	rpmlog(RPMLOG_DEBUG, D_("removing directory %s\n"), newrootdbpath);
	if (Rmdir(newrootdbpath))
	    rpmlog(RPMLOG_ERR, _("failed to remove directory %s: %s\n"),
			newrootdbpath, strerror(errno));
    }
    newrootdbpath = _free(newrootdbpath);
    rootdbpath = _free(rootdbpath);
    dbiTags = tagStoreFree(dbiTags, dbiNTags);
    myprefix = _free(myprefix);

    return rc;
}

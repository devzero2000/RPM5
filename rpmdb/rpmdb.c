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
#include <rpmsx.h>
#include <argv.h>

#define	_RPMBF_INTERNAL
#include <rpmbf.h>		/* pbm_set macros */

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

#define	_DBI_FLAGS	0
#define	_DBI_PERMS	0644
#define	_DBI_MAJOR	-1

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
	"Packages:Name:Basenames:Group:Requirename:Providename:Conflictname:Triggername:Dirnames:Requireversion:Provideversion:Installtid:Sigmd5:Sha1header:Filedigests:Depends:Pubkeys";
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

if (_rpmdb_debug)
fprintf(stderr, "--> %s(%p, %p) dbiTagStr %s\n", __FUNCTION__, dbiTagsP, dbiNTagsP, dbiTagStr);
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
if (_rpmdb_debug) {
fprintf(stderr, "\t%d %s(", dbiNTags, o);
if (tag & 0x40000000)
    fprintf(stderr, "0x%x)\n", tag);
else
    fprintf(stderr, "%d)\n", tag);
}
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
    static int _dbapi_rebuild;
    size_t dbix;
    dbiIndex dbi = NULL;
    int _dbapi;
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
    _dbapi_rebuild = rpmExpandNumeric("%{?_dbapi_rebuild}%{!?_dbapi_rebuild:4}");
assert(_dbapi_rebuild == 3 || _dbapi_rebuild == 4);
	_oneshot++;
   }

assert(db != NULL);					/* XXX sanity */
assert(db->_dbi != NULL);				/* XXX sanity */

    /* Is this index configured? */
    dbix = dbiTagToDbix(db, tag);
    if (dbix >= db->db_ndbi)
	goto exit;

    /* Is this index already open ? */
    if ((dbi = db->_dbi[dbix]) != NULL)
	goto exit;

    _dbapi = (_rebuildinprogress ? _dbapi_rebuild : db->db_api);
assert(_dbapi == 3 || _dbapi == 4);
assert(mydbvecs[_dbapi] != NULL);

    rc = (*mydbvecs[_dbapi]->open) (db, tag, &dbi);
    if (rc) {
	static uint8_t _printed[128];
	if (!_printed[dbix & 0x1f]++)
	    rpmlog(RPMLOG_ERR,
		_("cannot open %s(%u) index: %s(%d)\n\tDB: %s\n"),
		tagName(tag), tag,
		(rc > 0 ? strerror(rc) : ""), rc,
		((mydbvecs[_dbapi]->dbv_version != NULL)
		? mydbvecs[_dbapi]->dbv_version : "unknown"));
	dbi = db3Free(dbi);
	goto exit;
    }
    db->_dbi[dbix] = dbi;

    /* XXX FIXME: move to rpmdbOpen() instead. */
    if (tag == RPMDBI_PACKAGES && db->db_bf == NULL) {
	size_t _jiggery = 2;		/* XXX todo: Bloom filter tuning? */
	size_t _k = _jiggery * 8;
	size_t _m = _jiggery * (3 * 8192 * _k) / 2;
	db->db_bf = rpmbfNew(_m, _k, 0);
    }

exit:

/*@-modfilesys@*/
if (_rpmdb_debug)
fprintf(stderr, "<== dbiOpen(%p, %s(%u), 0x%x) dbi %p = %p[%u:%u]\n", db, tagName(tag), tag, flags, dbi, db->_dbi, dbix, db->db_ndbi);
/*@=modfilesys@*/

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
    dbiIndexItem rec = xmalloc(sizeof(*rec));
    rec->hdrNum = hdrNum;
    rec->tagNum = tagNum;
    rec->fpNum = 0;
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

typedef struct _setSwap_s {
    union _dbswap hdr;
    union _dbswap tag;
    rpmuint32_t fp;
} * setSwap;

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
    dbiIndexSet set;
    const char * s;
    setSwap T;
    int i;

    if (dbi == NULL || data == NULL || setp == NULL)
	return -1;
    _dbbyteswapped = dbiByteSwapped(dbi);

    if ((s = data->data) == NULL) {
	*setp = NULL;
	return 0;
    }

    set = xmalloc(sizeof(*set));
    set->count = (int) (data->size / dbi->dbi_jlen);
    set->recs = xmalloc(set->count * sizeof(*(set->recs)));

    T = (setSwap)set->recs;

/*@-sizeoftype @*/
    switch (dbi->dbi_jlen) {
    default:
    case 2*sizeof(rpmuint32_t):
	if (_dbbyteswapped) {
	    for (i = 0; i < (int)set->count; i++) {
		T->hdr.uc[3] = *s++;
		T->hdr.uc[2] = *s++;
		T->hdr.uc[1] = *s++;
		T->hdr.uc[0] = *s++;
		T->tag.uc[3] = *s++;
		T->tag.uc[2] = *s++;
		T->tag.uc[1] = *s++;
		T->tag.uc[0] = *s++;
		T->fp = 0;
		T++;
	    }
	} else {
	    for (i = 0; i < (int)set->count; i++) {
		memcpy(&T->hdr.ui, s, sizeof(T->hdr.ui));
		s += sizeof(T->hdr.ui);
		memcpy(&T->tag.ui, s, sizeof(T->tag.ui));
		s += sizeof(T->tag.ui);
		T->fp = 0;
		T++;
	    }
	}
	break;
    case 1*sizeof(rpmuint32_t):
	if (_dbbyteswapped) {
	    for (i = 0; i < (int)set->count; i++) {
		T->hdr.uc[3] = *s++;
		T->hdr.uc[2] = *s++;
		T->hdr.uc[1] = *s++;
		T->hdr.uc[0] = *s++;
		T->tag.ui = 0;
		T->fp = 0;
		T++;
	    }
	} else {
	    for (i = 0; i < (int)set->count; i++) {
		memcpy(&T->hdr.ui, s, sizeof(T->hdr.ui));
		s += sizeof(T->hdr.ui);
		T->tag.ui = 0;
		T->fp = 0;
		T++;
	    }
	}
	break;
    }
    *setp = set;
/*@=sizeoftype @*/
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
    unsigned int	mi_count;
    uint32_t		mi_setx;
    void *		mi_keyp;
    int			mi_index;
    size_t		mi_keylen;
/*@refcounted@*/ /*@null@*/
    Header		mi_h;
    int			mi_sorted;
    int			mi_cflags;
    int			mi_modified;
    unsigned int	mi_prevoffset;	/* header instance (native endian) */
    unsigned int	mi_offset;	/* header instance (native endian) */
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
	db->db_bf = rpmbfFree(db->db_bf);
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
	/*@*/
{
    rpmdb db = rpmdbGetPool(_rpmdbPool);
    const char * epfx = _DB_ERRPFX;

/*@-modfilesys@*/ /*@-nullpass@*/
if (_rpmdb_debug)
fprintf(stderr, "==> rpmdbNew(%s, %s, 0x%x, 0%o, 0x%x) db %p\n", root, home, mode, perms, flags, db);
/*@=modfilesys@*/ /*@=nullpass@*/

    if (!(perms & 0600)) perms = 0644;	/* XXX sanity */

    db->db_root = rpmdbURIPath( (root && *root ? root : _DB_ROOT) );
    db->db_home = rpmdbURIPath( (home && *home ? home : _DB_HOME) );

    if (!(db->db_home && db->db_home[0] && db->db_home[0] != '%')) {
	rpmlog(RPMLOG_ERR, _("no dbpath has been set\n"));
	db->db_root = _free(db->db_root);
	db->db_home = _free(db->db_home);
	db = (rpmdb) rpmioPutPool((rpmioItem)db);
	/*@-globstate@*/ return NULL; /*@=globstate@*/
    }

    db->db_flags = (flags >= 0) ? flags : _DB_FLAGS;
    db->db_mode = (mode >= 0) ? mode : _DB_MODE;
    db->db_perms = (perms >= 0)	? perms : _DB_PERMS;
    db->db_api = _DB_MAJOR;
    db->db_errpfx = rpmExpand( (epfx && *epfx ? epfx : _DB_ERRPFX), NULL);

    db->db_remove_env = 0;
    db->db_verifying = 0;
    db->db_rebuilding = _rebuildinprogress;	/* XXX no thread_count w --rebuilddb */
    db->db_chrootDone = 0;
    db->db_errcall = NULL;
    db->db_errfile = NULL;
    db->db_malloc = NULL;
    db->db_realloc = NULL;
    db->db_free = NULL;
    db->db_export = rpmdbExportInfo;
    db->db_h = NULL;

    db->db_bf = NULL;
    db->db_next = NULL;
    db->db_opens = 0;

    db->db_dbenv = NULL;
    db->db_txn = NULL;
    db->db_logc = NULL;
    db->db_mpf = NULL;

    dbiTagsInit(&db->db_tags, &db->db_ndbi);
    db->_dbi = xcalloc(db->db_ndbi, sizeof(*db->_dbi));

    memset(&db->db_getops, 0, sizeof(db->db_getops));
    memset(&db->db_putops, 0, sizeof(db->db_putops));
    memset(&db->db_delops, 0, sizeof(db->db_delops));

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

int rpmdbCount(rpmdb db, rpmTag tag, const void * keyp, size_t keylen)
{
    unsigned int count = 0;
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

    xx = dbiCopen(dbi, dbiTxnid(dbi), &dbcursor, 0);
    rc = dbiGet(dbi, dbcursor, &k, &v, DB_SET);
    switch (rc) {
    case 0:
	if ((rc = dbiCount(dbi, dbcursor, &count, 0)) != 0) {
	    rc = -1;
	    break;
	}
	/*@fallthrough@*/
    case DB_NOTFOUND:
	rc = count;
	break;
    default:
	rpmlog(RPMLOG_ERR, _("error(%d) getting records from %s index\n"),
		rc, tagName(dbi->dbi_rpmtag));
	rc = -1;
	break;
    }
    xx = dbiCclose(dbi, dbcursor, 0);
    dbcursor = NULL;
    return rc;
}

/* XXX python/upgrade.c, install.c, uninstall.c */
int rpmdbCountPackages(rpmdb db, const char * name)
{
    return rpmdbCount(db, RPMTAG_NAME, name, 0);
}

int rpmdbMireApply(rpmdb db, rpmTag tag, rpmMireMode mode, const char * pat,
		const char *** argvp)
{
    DBC * dbcursor = NULL;
    DBT k = DBT_INIT;
    DBT v = DBT_INIT;
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

    xx = dbiCopen(dbi, dbiTxnid(dbi), &dbcursor, 0);

    /* Iterate over all string keys, collecting pattern matches. */
    while ((rc = dbiGet(dbi, dbcursor, &k, &v, DB_NEXT_NODUP)) == 0) {
	size_t ns = k.size;
	/* XXX TODO: strdup malloc is necessary solely for argvAdd() */
	char * s = memcpy(xmalloc(ns+1), k.data, ns);

	s[ns] = '\0';
	if (mire == NULL || mireRegexec(mire, s, ns) >= 0)
	    xx = argvAdd(&av, s);
	s = _free(s);
    }

    xx = dbiCclose(dbi, dbcursor, 0);
    dbcursor = NULL;

    switch (rc) {
    case 0:
    case DB_NOTFOUND:
	ret = 0;
	break;
    default:
	rpmlog(RPMLOG_ERR, _("error(%d) getting keys from %s index\n"),
		rc, tagName(dbi->dbi_rpmtag));
	break;
    }

exit:
    if (argvp != NULL)
	xx = argvAppend(argvp, av);
    av = argvFree(av);
    mire = mireFree(mire);
    return ret;
}

static int rpmdbMireKeys(rpmdb db, rpmTag tag, rpmMireMode mode,
		const char * pat, dbiIndexSet * matches)
{
    DBC * dbcursor = NULL;
    DBT k = DBT_INIT;
    DBT p = DBT_INIT;
    DBT v = DBT_INIT;
    dbiIndex dbi;
    miRE mire = NULL;
uint32_t _flags = DB_NEXT;
dbiIndexSet set = NULL;
    int ret = 1;		/* assume error */
    int rc;
    int xx;

    dbi = dbiOpen(db, tag, 0);
    if (dbi == NULL)
	goto exit;

    if (pat) {
	mire = mireNew(mode, 0);
	xx = mireRegcomp(mire, pat);

	/* Use the pattern stem to (partially) match on lookup. */
	/* XXX TODO: extend to other patterns. */
	switch (mode) {
	default:
	    break;
	case RPMMIRE_STRCMP:
	    k.data = (void *) pat;
	    k.size = (UINT32_T) strlen(pat);
	    _flags = DB_SET;
	    break;
	}
    }

    xx = dbiCopen(dbi, dbiTxnid(dbi), &dbcursor, 0);

    /* Iterate over all keys, collecting primary keys. */
    while ((rc = dbiPget(dbi, dbcursor, &k, &p, &v, _flags)) == 0) {
	size_t ns = k.size;
	/* XXX TODO: strdup malloc is necessary solely for argvAdd() */
	char * s = memcpy(xmalloc(ns+1), k.data, ns);

	s[ns] = '\0';
	if (mire == NULL || mireRegexec(mire, s, ns) >= 0) {
	    union _dbswap mi_offset;
	    unsigned i;

	    /* Get a native endian copy of the primary package key. */
	    memcpy(&mi_offset, p.data, sizeof(mi_offset.ui));
	    if (dbiByteSwapped(dbi) == 1)
		_DBSWAP(mi_offset);

	    /* Append primary package key to set. */
	    if (set == NULL)
		set = xcalloc(1, sizeof(*set));
	    i = set->count;
	    /* XXX TODO: sort/uniqify set? */
	    (void) dbiAppendSet(set, &mi_offset.ui, 1, sizeof(mi_offset.ui), 0);

	}
	s = _free(s);

	if (_flags == DB_SET) _flags = DB_NEXT_DUP;
    }

    xx = dbiCclose(dbi, dbcursor, 0);
    dbcursor = NULL;

    switch (rc) {
    case 0:
    case DB_NOTFOUND:
	ret = 0;
	break;
    default:
	rpmlog(RPMLOG_ERR, _("error(%d) getting keys from %s index\n"),
		rc, tagName(dbi->dbi_rpmtag));
	break;
    }

exit:
    if (ret == 0 && matches) {
	/* XXX TODO: sort/uniqify set? */
	*matches = set;
	set = NULL;
    }
    set = dbiFreeIndexSet(set);
    mire = mireFree(mire);
    return ret;
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

    rc = rpmdbMireKeys(dbi->dbi_rpmdb, RPMTAG_NAME, RPMMIRE_STRCMP, name, matches);
    switch (rc) {
    case 0:
	if (version == NULL && release == NULL)
	    return RPMRC_OK;
	break;
    case DB_NOTFOUND:
	return RPMRC_NOTFOUND;
	/*@notreached@*/ break;
    default:
	rpmlog(RPMLOG_ERR,
		_("error(%d) getting records from %s index\n"),
		rc, tagName(dbi->dbi_rpmtag));
	return RPMRC_FAIL;
	/*@notreached@*/ break;
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
 * @param arg		name[-version[-release]] string
 * @param keylen
 * @retval matches	set of header instances that match
 * @return 		RPMRC_OK on match, RPMRC_NOMATCH or RPMRC_FAIL
 */
static rpmRC dbiFindByLabel(dbiIndex dbi, /*@null@*/ const char * arg, size_t keylen,
		/*@out@*/ dbiIndexSet * matches)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies dbi, *matches,
		rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@requires maxSet(matches) >= 0 @*/
{
DBC * dbcursor = NULL;
DBT k = DBT_INIT, *key = &k;
DBT v = DBT_INIT, *data = &v;
    const char * release;
    char * localarg;
    char * s;
    char c;
    int brackets;
    rpmRC rc;
    int xx;

    if (arg == NULL || strlen(arg) == 0) return RPMRC_NOTFOUND;

    xx = dbiCopen(dbi, dbiTxnid(dbi), &dbcursor, 0);

    /* did they give us just a name? */
    rc = dbiFindMatches(dbi, dbcursor, key, data, arg, NULL, NULL, matches);
    if (rc != RPMRC_NOTFOUND)
	goto exit;

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

    if (s == localarg) {
	rc = RPMRC_NOTFOUND;
	goto exit;
    }

    *s = '\0';
    rc = dbiFindMatches(dbi, dbcursor, key, data, localarg, s + 1, NULL, matches);
    if (rc != RPMRC_NOTFOUND)
	goto exit;

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

    if (s == localarg) {
	rc = RPMRC_NOTFOUND;
	goto exit;
    }

    *s = '\0';
    rc = dbiFindMatches(dbi, dbcursor, key, data, localarg, s + 1, release, matches);

exit:
    xx = dbiCclose(dbi, dbcursor, 0);
    dbcursor = NULL;
/*@-nullstate@*/	/* FIX: *matches may be NULL. */
    return rc;
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
	int chkhdr = (pgpDigVSFlags & _RPMVSF_NOHEADER) ^ _RPMVSF_NOHEADER;
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
	if (mi->mi_ts && chkhdr) {
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
assert(dbi != NULL);				/* XXX sanity */

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

    (void) mireFreeAll(mi->mi_re, mi->mi_nre);
    mi->mi_re = NULL;

    mi->mi_set = dbiFreeIndexSet(mi->mi_set);

    mi->mi_keyp = _free(mi->mi_keyp);
    mi->mi_keylen = 0;
    mi->mi_index = 0;

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
    mi = (rpmmi) rpmioGetPool(pool, sizeof(*mi));
    memset(((char *)mi)+sizeof(mi->_item), 0, sizeof(*mi)-sizeof(mi->_item));
    return mi;
}

unsigned int rpmmiInstance(rpmmi mi) {
    return (mi ? mi->mi_offset : 0);
}

unsigned int rpmmiCount(rpmmi mi) {
    unsigned int rc;

    /* XXX Secondary db associated with Packages needs cursor record count */
    if (mi && mi->mi_index && mi->mi_dbc == NULL) {
	dbiIndex dbi = dbiOpen(mi->mi_db, mi->mi_rpmtag, 0);
	DBT k = DBT_INIT;
	DBT v = DBT_INIT;
	int xx;
assert(dbi != NULL);	/* XXX dbiCopen doesn't handle dbi == NULL */
	xx = dbiCopen(dbi, dbiTxnid(dbi), &mi->mi_dbc, mi->mi_cflags);
	k.data = mi->mi_keyp;
	k.size = (u_int32_t)mi->mi_keylen;
if (k.data && k.size == 0) k.size = (UINT32_T) strlen((char *)k.data);
if (k.data && k.size == 0) k.size++;	/* XXX "/" fixup. */
	if (!dbiGet(dbi, mi->mi_dbc, &k, &v, DB_SET))
	    xx = dbiCount(dbi, mi->mi_dbc, &mi->mi_count, 0);
	memset(&mi->mi_data, 0, sizeof(mi->mi_data));
    }

    rc = (mi ? mi->mi_count : 0);

    return rc;
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

    /* XXX HACK to remove the existing complexity of RPMTAG_BASENAMES */
    if (tag == RPMTAG_BASENAMES) tag = RPMTAG_FILEPATHS;

    switch (*modep) {
    default:
    case RPMMIRE_DEFAULT:
	if (tag == RPMTAG_DIRNAMES || tag == RPMTAG_BASENAMES
	 || tag == RPMTAG_FILEPATHS)
	{
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
	mire = mireLink(mi->mi_re);
    } else {
	void *use =  mi->mi_re->_item.use;
	void *pool = mi->mi_re->_item.pool;
	mi->mi_re = xrealloc(mi->mi_re, (mi->mi_nre + 1) * sizeof(*mi->mi_re));
if (_mire_debug)
fprintf(stderr, "    mire %p[%u] realloc\n", mi->mi_re, mi->mi_nre+1);
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

static int rpmmiGet(dbiIndex dbi, DBC * dbcursor, DBT * kp, DBT * pk, DBT * vp,
                unsigned int flags)
{
    int map;
    int rc;

if (dbi->dbi_debug) fprintf(stderr, "--> %s(%p(%s),%p,%p,%p,0x%x)\n", __FUNCTION__, dbi, tagName(dbi->dbi_rpmtag), dbcursor, kp, vp, flags);

    switch (dbi->dbi_rpmdb->db_api) {
    default:	map = 0;		break;
    case 3:	map = _rpmmi_usermem;	break;	/* Berkeley DB */
    }

    if (map) {
	static const int _prot = PROT_READ | PROT_WRITE;
	static const int _flags = MAP_PRIVATE| MAP_ANONYMOUS;
	static const int _fdno = -1;
	static const off_t _off = 0;

	memset(vp, 0, sizeof(*vp));
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
	    if (dbi->dbi_index && pk)
		rc = dbiPget(dbi, dbcursor, kp, pk, vp, flags);
	    else
		rc = dbiGet(dbi, dbcursor, kp, vp, flags);
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
    DBT p = DBT_INIT;
    DBT v = DBT_INIT;
    union _dbswap mi_offset;
    void * uh;
    size_t uhlen;
    int chkhdr = (pgpDigVSFlags & _RPMVSF_NOHEADER) ^ _RPMVSF_NOHEADER;
rpmTag tag;
uint32_t _flags;
    int map;
    int rc;
    int xx;

    if (mi == NULL)
	return NULL;

tag = (mi->mi_set == NULL && mi->mi_index ? mi->mi_rpmtag : RPMDBI_PACKAGES);
    dbi = dbiOpen(mi->mi_db, tag, 0);
    if (dbi == NULL)
	return NULL;

    switch (dbi->dbi_rpmdb->db_api) {
    default:	map = 0;		break;
    case 3:	map = _rpmmi_usermem;	break;	/* Berkeley DB */
    }

if (dbi->dbi_debug) fprintf(stderr, "--> %s(%p) dbi %p(%s)\n", __FUNCTION__, mi, dbi, tagName(tag));

    /*
     * Cursors are per-iterator, not per-dbi, so get a cursor for the
     * iterator on 1st call. If the iteration is to rewrite headers, and the
     * CDB model is used for the database, then the cursor needs to
     * marked with DB_WRITECURSOR as well.
     */
    if (mi->mi_dbc == NULL) {
	xx = dbiCopen(dbi, dbiTxnid(dbi), &mi->mi_dbc, mi->mi_cflags);
	k.data = mi->mi_keyp;
	k.size = (u_int32_t)mi->mi_keylen;
if (k.data && k.size == 0) k.size = (UINT32_T) strlen((char *)k.data);
if (k.data && k.size == 0) k.size++;	/* XXX "/" fixup. */
	memset(&mi->mi_data, 0, sizeof(mi->mi_data));
	_flags = DB_SET;
    } else
	_flags = (mi->mi_setx ? DB_NEXT_DUP : DB_SET);

next:
    if (mi->mi_set) {
	/* The set of header instances is known in advance. */
	if (!(mi->mi_setx < mi->mi_set->count))
	    return NULL;
	mi->mi_offset = dbiIndexRecordOffset(mi->mi_set, mi->mi_setx);
	mi->mi_setx++;
	/* If next header is identical, return it now. */
	if (mi->mi_offset == mi->mi_prevoffset && mi->mi_h != NULL)
	    return mi->mi_h;
	/* Fetch header by offset. */
	mi_offset.ui = mi->mi_offset;
	if (dbiByteSwapped(dbi) == 1)
	    _DBSWAP(mi_offset);
	k.data = &mi_offset.ui;
	k.size = (u_int32_t)sizeof(mi_offset.ui);
	rc = rpmmiGet(dbi, mi->mi_dbc, &k, NULL, &v, DB_SET);
    }
    else if (dbi->dbi_index) {
	rc = rpmmiGet(dbi, mi->mi_dbc, &k, &p, &v, _flags);
	switch (rc) {
	default:
assert(0);
	    /*@notreached@*/ break;
	case DB_NOTFOUND:
	    return NULL;
	    /*@notreached@*/ break;
	case 0:
	    mi->mi_setx++;
	    memcpy(&mi_offset, p.data, sizeof(mi_offset.ui));
	    if (dbiByteSwapped(dbi) == 1)
		_DBSWAP(mi_offset);
	    mi->mi_offset = mi_offset.ui;
	    /* If next header is identical, return it now. */
	    if (mi->mi_offset == mi->mi_prevoffset && mi->mi_h != NULL)
		return mi->mi_h;
	    break;
	}
	_flags = DB_NEXT_DUP;
    }
    else {
	/* Iterating Packages database. */
assert(mi->mi_rpmtag == RPMDBI_PACKAGES);

	/* Fetch header with DB_NEXT. */
	/* Instance 0 is the largest header instance in the database,
	 * and should be skipped. */
	do {
	    rc = rpmmiGet(dbi, mi->mi_dbc, &k, NULL, &v, DB_NEXT);
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
    if (mi->mi_ts && chkhdr) {
	rpmRC rpmrc = RPMRC_NOTFOUND;

	/* Don't bother re-checking a previously read header. */
	if (mi->mi_db && mi->mi_db->db_bf
	 && rpmbfChk(mi->mi_db->db_bf,
		(const char *)&mi->mi_offset, sizeof(mi->mi_offset)))
	    rpmrc = RPMRC_OK;

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
	    if (mi->mi_db && mi->mi_db->db_bf && rpmrc == RPMRC_OK)
		rpmbfAdd(mi->mi_db->db_bf,
			(const char *)&mi->mi_offset, sizeof(mi->mi_offset));

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

    xx = dbiCopen(dbi, dbiTxnid(dbi), &dbcursor, 0);
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
    for (i = j = 0; i < (int)set->count; i++) {
	if (exclude && set->recs[i].hdrNum == exclude)
	    continue;
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

    for (i = 0; i < (int)set->count; i++)
	set->recs[i].fpNum = fpNum;

#ifdef	SQLITE_HACK
    xx = dbiCclose(dbi, dbcursor, 0);
    dbcursor = NULL;
#endif

    if (mi->mi_set == NULL) {
	mi->mi_set = set;
    } else {
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

    switch (tag) {
    default:	break;
    /* XXX HACK to remove rpmdbFindByLabel/findMatches from the API */
    case RPMDBI_LABEL:
	tag = RPMTAG_NAME;
	isLabel = 1;
	break;
    /* XXX HACK to remove the existing complexity of RPMTAG_BASENAMES */
    case RPMTAG_BASENAMES:
	tag = RPMTAG_FILEPATHS;
	break;
    }

    dbi = dbiOpen(db, tag, 0);
    if (dbi == NULL)
	return NULL;

    mi = rpmmiGetPool(_rpmmiPool);
    (void)rpmioLinkPoolItem((rpmioItem)mi, __FUNCTION__, __FILE__, __LINE__);

if (dbi->dbi_debug) fprintf(stderr, "--> %s(%p, %s, %p[%u]=\"%s\") dbi %p mi %p\n", __FUNCTION__, db, tagName(tag), keyp, (unsigned)keylen, (keylen == 0 ? (const char *)keyp : "???"), dbi, mi);

    /* Chain cursors for teardown on abnormal exit. */
    mi->mi_next = rpmmiRock;
    rpmmiRock = mi;

    if (tag == RPMDBI_PACKAGES && keyp == NULL) {
	/* Special case #1: sequentially iterate Packages database. */
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
	/* XXX Special case #3: empty iterator with rpmmiGrow() */
	assert(keylen == 0);
    }
    else if (isLabel && tag == RPMTAG_NAME) {
	/* XXX Special case #4: gather primary keys for a NVR label. */
	int rc;

	rc = dbiFindByLabel(dbi, keyp, keylen, &set);

	if ((rc  && rc != DB_NOTFOUND) || set == NULL || set->count < 1) { /* error or empty set */
	    set = dbiFreeIndexSet(set);
	    rpmmiRock = mi->mi_next;
	    mi->mi_next = NULL;
	    mi = (rpmmi)rpmioFreePoolItem((rpmioItem)mi, __FUNCTION__, __FILE__, __LINE__);
	    return NULL;
	}
    }
    else if (dbi->dbi_index) {
	/* XXX Special case #5: secondary db associated with primary Packages */
    }
    else {
	/* Common case: retrieve join keys. */
assert(0);
    }

/*@-assignexpose@*/
    mi->mi_db = rpmdbLink(db, "matchIterator");
/*@=assignexpose@*/
    mi->mi_rpmtag = tag;

    mi->mi_dbc = NULL;
    mi->mi_set = set;
    mi->mi_setx = 0;
    mi->mi_count = (set ? set->count : 0);

    mi->mi_index = (dbi && dbi->dbi_index ? 1 : 0);
    mi->mi_keylen = keylen;
    if (keyp)
	mi->mi_keyp = keylen > 0
		? memcpy(xmalloc(keylen), keyp, keylen) : xstrdup(keyp) ;
    else
	mi->mi_keyp = NULL;

    mi->mi_h = NULL;
    mi->mi_sorted = 0;
    mi->mi_cflags = 0;
    mi->mi_modified = 0;
    mi->mi_prevoffset = 0;
    mi->mi_offset = 0;
    mi->mi_nre = 0;
    mi->mi_re = NULL;

    mi->mi_ts = NULL;

/*@i@*/ return mi;
}

/* XXX psm.c */
int rpmdbRemove(rpmdb db, /*@unused@*/ int rid, unsigned int hdrNum,
		/*@unused@*/ rpmts ts)
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    sigset_t signalMask;
    int rc = 0;
    int xx;

    if (db == NULL)
	return 0;

    /* Retrieve header for use by associated secondary index callbacks. */
    {	rpmmi mi;
	mi = rpmmiInit(db, RPMDBI_PACKAGES, &hdrNum, sizeof(hdrNum));
	db->db_h = rpmmiNext(mi);
	if (db->db_h)
	    db->db_h = headerLink(db->db_h);
	mi = rpmmiFree(mi);
    }

    if (db->db_h == NULL) {
	rpmlog(RPMLOG_ERR, _("%s: cannot read header at 0x%x\n"),
	      "rpmdbRemove", hdrNum);
	return 1;
    }

    he->tag = RPMTAG_NVRA;
    xx = headerGet(db->db_h, he, 0);
    rpmlog(RPMLOG_DEBUG, "  --- h#%8u %s\n", hdrNum, he->p.str);
    he->p.ptr = _free(he->p.ptr);

    (void) blockSignals(db, &signalMask);

/*@-nullpass -nullptrarith -nullderef @*/ /* FIX: rpmvals heartburn */
    {	dbiIndexItem rec = dbiIndexNewItem(hdrNum, 0);
	size_t dbix;

	dbix = db->db_ndbi - 1;
	if (db->db_tags != NULL)
	do {
	    dbiIndex dbi;

	    tagStore_t dbiTag = db->db_tags + dbix;

	    dbi = NULL;
	    (void) memset(he, 0, sizeof(*he));
	    he->tag = dbiTag->tag;

	    switch (he->tag) {
	    default:
		/* Do a lazy open on all necessary secondary indices. */
#ifndef	NOTYET	/* XXX headerGet() sees tag extensions, headerIsEntry doesn't */
		if (headerGet(db->db_h, he, 0)) {
		    dbi = dbiOpen(db, he->tag, 0);
assert(dbi != NULL);				/* XXX sanity */
		}
		he->p.ptr = _free(he->p.ptr);
#else
		if (headerIsEntry(db->db_h, he->tag)) {
		    dbi = dbiOpen(db, he->tag, 0);
assert(dbi != NULL);				/* XXX sanity */
		}
#endif
		/*@switchbreak@*/ break;
	    case RPMDBI_AVAILABLE:	/* Filter out temporary databases */
	    case RPMDBI_ADDED:
	    case RPMDBI_REMOVED:
	    case RPMDBI_DEPENDS:
	    case RPMDBI_SEQNO:
		/*@switchbreak@*/ break;
	    case RPMDBI_PACKAGES:
	    {	DBC * dbcursor = NULL;
		DBT k = DBT_INIT;
		DBT v = DBT_INIT;
	        union _dbswap mi_offset;

		if (db->db_export != NULL)
		    xx = db->db_export(db, db->db_h, 0);

		dbi = dbiOpen(db, he->tag, 0);
		if (dbi == NULL)	/* XXX shouldn't happen */
		    continue;
	      
	        mi_offset.ui = hdrNum;
		if (dbiByteSwapped(dbi) == 1)
		    _DBSWAP(mi_offset);
/*@-immediatetrans@*/
		k.data = &mi_offset;
/*@=immediatetrans@*/
		k.size = (UINT32_T) sizeof(mi_offset.ui);

		rc = dbiCopen(dbi, dbiTxnid(dbi), &dbcursor, DB_WRITECURSOR);
		rc = dbiGet(dbi, dbcursor, &k, &v, DB_SET);
		if (rc) {
		    const char * dbiBN = (dbiTag->str != NULL
			? dbiTag->str : tagName(dbiTag->tag));
		    rpmlog(RPMLOG_ERR,
			_("error(%d) setting header #%d record for %s removal\n"),
			rc, hdrNum, dbiBN);
		} else
		    rc = dbiDel(dbi, dbcursor, &k, &v, 0);
		xx = dbiCclose(dbi, dbcursor, DB_WRITECURSOR);
		dbcursor = NULL;
		if (!dbi->dbi_no_dbsync)
		    xx = dbiSync(dbi, 0);
	    }	/*@switchbreak@*/ break;
	    }
	} while (dbix-- > 0);

	rec = _free(rec);
    }
/*@=nullpass =nullptrarith =nullderef @*/

    /* Unreference header used by associated secondary index callbacks. */
    (void) headerFree(db->db_h);
    db->db_h = NULL;

    (void) unblockSignals(db, &signalMask);

    /* XXX return ret; */
    return 0;
}

/* XXX install.c */
int rpmdbAdd(rpmdb db, int iid, Header h, /*@unused@*/ rpmts ts)
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    sigset_t signalMask;
    dbiIndex dbi;
    size_t dbix;
    union _dbswap mi_offset;
    uint32_t hdrNum = headerGetInstance(h);
    int ret = 0;
    int xx;

    if (db == NULL)
	return 0;

if (_rpmdb_debug)
fprintf(stderr, "--> %s(%p, %u, %p, %p) h# %u\n", __FUNCTION__, db, (unsigned)iid, h, ts, hdrNum);

    /* Reference header for use by associated secondary index callbacks. */
    db->db_h = headerLink(h);

assert(headerIsEntry(h, RPMTAG_REMOVETID) == 0);	/* XXX sanity */

    /* Add the install transaction id. */
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

assert(headerIsEntry(h, RPMTAG_PACKAGECOLOR) != 0);	/* XXX sanity */

    (void) blockSignals(db, &signalMask);

    /* Assign a primary Packages key for new Header's. */
    if (!db->db_rebuilding && hdrNum == 0) {
	int64_t seqno = 0;
	dbi = dbiOpen(db, RPMDBI_SEQNO, 0);
	if ((ret = dbiSeqno(dbi, &seqno, 0)) == 0) {
	    hdrNum = seqno;
	    headerSetInstance(h, hdrNum);
	}
    }

/* XXX ensure that the header instance is set persistently. */
if (hdrNum == 0) {
assert(hdrNum > 0);
assert(hdrNum == headerGetInstance(h));
}

    dbi = dbiOpen(db, RPMDBI_PACKAGES, 0);
assert(dbi != NULL);					/* XXX sanity */
    {
	/* Header indices are monotonically increasing integer instances
	 * starting with 1.  Header instance #0 is where the monotonically
	 * increasing integer is stored.  */
	uint32_t idx0 = 0;

	DBC * dbcursor = NULL;
	DBT k = DBT_INIT;
	DBT v = DBT_INIT;

	xx = dbiCopen(dbi, dbiTxnid(dbi), &dbcursor, DB_WRITECURSOR);

	/* Retrieve largest primary key. */
	k.data = &idx0;
	k.size = (UINT32_T)sizeof(idx0);
	ret = dbiGet(dbi, dbcursor, &k, &v, DB_SET);

	if (ret == 0 && v.data) {
	    memcpy(&mi_offset, v.data, sizeof(mi_offset.ui));
	    if (dbiByteSwapped(dbi) == 1)
		_DBSWAP(mi_offset);
	    idx0 = (unsigned) mi_offset.ui;
	}
	/* Update largest primary key (if necessary). */
	if (hdrNum > idx0) {
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
	    ret = dbiPut(dbi, dbcursor, &k, &v, DB_CURRENT);
/*@=compmempass@*/
	}

	xx = dbiCclose(dbi, dbcursor, DB_WRITECURSOR);
    }

    if (ret) {
	rpmlog(RPMLOG_ERR,
		_("error(%d) updating largest primary key\n"), ret);
	goto exit;
    }

    /* Now update the indexes */

    {	dbiIndexItem rec = dbiIndexNewItem(hdrNum, 0);
	int chkhdr = (pgpDigVSFlags & _RPMVSF_NOHEADER) ^ _RPMVSF_NOHEADER;

	dbix = db->db_ndbi - 1;
	if (db->db_tags != NULL)
	do {

	    tagStore_t dbiTag = db->db_tags + dbix;

	    dbi = NULL;
	    (void) memset(he, 0, sizeof(*he));
	    he->tag = dbiTag->tag;

	    switch (he->tag) {
	    default:
		/* Do a lazy open on all necessary secondary indices. */
#ifndef	NOTYET	/* XXX headerGet() sees tag extensions, headerIsEntry doesn't */
		if (headerGet(h, he, 0)) {
		    dbi = dbiOpen(db, he->tag, 0);
assert(dbi != NULL);					/* XXX sanity */
		}
		he->p.ptr = _free(he->p.ptr);
#else
		if (headerIsEntry(h, he->tag)) {
		    dbi = dbiOpen(db, he->tag, 0);
assert(dbi != NULL);					/* XXX sanity */
		}
#endif
		/*@switchbreak@*/ break;
	    case RPMDBI_AVAILABLE:	/* Filter out temporary databases */
	    case RPMDBI_ADDED:
	    case RPMDBI_REMOVED:
	    case RPMDBI_DEPENDS:
	    case RPMDBI_SEQNO:
		/*@switchbreak@*/ break;
	    case RPMDBI_PACKAGES:
	    {	DBC * dbcursor = NULL;
		DBT k = DBT_INIT;
		DBT v = DBT_INIT;
		rpmRC rpmrc = RPMRC_NOTFOUND;

		if (db->db_export != NULL)
		    xx = db->db_export(db, h, 1);

		dbi = dbiOpen(db, he->tag, 0);
assert(dbi != NULL);					/* XXX sanity */
		xx = dbiCopen(dbi, dbiTxnid(dbi), &dbcursor, DB_WRITECURSOR);

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
		if (ts && chkhdr && !(h->flags & HEADERFLAG_RDONLY)) {
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
		}
		v.data = _free(v.data); /* headerUnload */
		v.size = 0;
		xx = dbiCclose(dbi, dbcursor, DB_WRITECURSOR);
		if (!dbi->dbi_no_dbsync)
		    xx = dbiSync(dbi, 0);

	    }	/*@switchbreak@*/ break;
	    }

	} while (dbix-- > 0);

	rec = _free(rec);
    }

exit:
    /* Unreference header used by associated secondary index callbacks. */
    (void) headerFree(db->db_h);
    db->db_h = NULL;

    (void) unblockSignals(db, &signalMask);

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

	matchList[i] = xcalloc(1, sizeof(*(matchList[i])));

/*@-dependenttrans@*/
key->data = (void *) fpList[i].baseName;
/*@=dependenttrans@*/
key->size = (UINT32_T) strlen((char *)key->data);
if (key->size == 0) key->size++;	/* XXX "/" fixup. */

	/* XXX TODO: eliminate useless exclude/tag arguments. */
	xx = rpmdbGrowIterator(mi, i, exclude, 0);

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
	uint32_t start;
	uint32_t num;
	uint32_t end;

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
	for (i = 0; i < (int)num; i++) {
	    baseNames[i] = fullBaseNames[im[i].tagNum];
	    dirIndexes[i] = fullDirIndexes[im[i].tagNum];
	}

	fps = xcalloc(num, sizeof(*fps));
	fpLookupList(fpc, dirNames, baseNames, dirIndexes, num, fps);

	/* Add db (recnum,filenum) to list for fingerprint matches. */
	for (i = 0; i < (int)num; i++, im++) {
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

	    /* Filter out temporary/persistent databases */
	    switch (tag) {
	    case RPMDBI_AVAILABLE:
	    case RPMDBI_ADDED:
	    case RPMDBI_REMOVED:
	    case RPMDBI_DEPENDS:
	    case RPMDBI_SEQNO:
	    case RPMDBI_BTREE:
	    case RPMDBI_HASH:
	    case RPMDBI_QUEUE:
	    case RPMDBI_RECNO:
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

	    xx = rpmsxSetfilecon(NULL, nfn, nst->st_mode, NULL);

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

    return rc;
}

int rpmdbRebuild(const char * prefix, rpmts ts)
	/*@globals _rebuildinprogress @*/
	/*@modifies _rebuildinprogress @*/
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
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
    int rc = 0;
    int xx;
    int _dbapi = rpmExpandNumeric("%{_dbapi}");
    int _dbapi_rebuild = rpmExpandNumeric("%{_dbapi_rebuild}");
    tagStore_t dbiTags = NULL;
    size_t dbiNTags = 0;

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
    
    newdb->db_rebuilding = 1;	/* XXX disable db->associate hack-o-round */
    {	Header h = NULL;
	rpmmi mi;

	mi = rpmmiInit(olddb, RPMDBI_PACKAGES, NULL, 0);
	if (ts)
	    (void) rpmmiSetHdrChk(mi, ts);

	while ((h = rpmmiNext(mi)) != NULL) {
	    uint32_t hdrNum = headerGetInstance(h);

/* XXX ensure that the header instance is set persistently. */
assert(hdrNum > 0 && hdrNum == rpmmiInstance(mi));

	    he->tag = RPMTAG_NVRA;
	    xx = headerGet(h, he, 0);
/* XXX limit the fiddle up to linux for now. */
#if !defined(HAVE_SETPROCTITLE) && defined(__linux__)
	    if (he->p.str)
		setproctitle("%s", he->p.str);
#endif

	    /* Deleted entries are eliminated in legacy headers by copy. */
	    {	Header nh = (headerIsEntry(h, RPMTAG_HEADERIMAGE)
				? headerCopy(h) : NULL);
if (nh) headerSetInstance(nh, hdrNum);
		rc = rpmdbAdd(newdb, -1, (nh ? nh : h), ts);
		(void)headerFree(nh);
		nh = NULL;
	    }

	    if (rc) {
		rpmlog(RPMLOG_ERR,
			_("cannot add %s (h# %u)\n"), he->p.str, hdrNum);
		failed = 1;
	    }
	    he->p.ptr = _free(he->p.ptr);

	    if (failed)
		break;
	}

	mi = rpmmiFree(mi);

    }
    newdb->db_rebuilding = 0;

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

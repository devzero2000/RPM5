
#include "system.h"

#include <poptIO.h>

#include <rpmio.h>
#include <rpmlog.h>
#include <rpmmacro.h>
#include <argv.h>

#define _RPMTAG_INTERNAL
#define _RPMDB_INTERNAL
#include <rpmdb.h>

#include <rpmts.h>
#include <rpmrc.h>

#include "debug.h"

#ifdef HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#endif
#ifdef __APPLE__
#include <libkern/OSByteOrder.h>

#define htobe32(x) OSSwapHostToBigInt32(x)
#define htole32(x) OSSwapHostToLittleInt32(x)
#endif /* __APPLE__ */
#if BYTE_ORDER == LITTLE_ENDIAN
#define bswap32(x) htobe32(x)
#elif BYTE_ORDER == BIG_ENDIAN
#define bswap32(x) htole32(x)
#endif

static char *rootPath = NULL;
static int dbType = 0;
static int byteOrder = 0;
static int rebuildDb = 0;

static int disable_fsync(int arg)
{
    return 0;
}

static int bdb_log_archive(DB_ENV * dbenv, char ***list, uint32_t flags)
{
    int ret;
    if ((ret = dbenv->log_archive(dbenv, list, flags)) != 0)
	dbenv->err(dbenv, ret, "DB_ENV->log_archive");
    return ret;
}

static int bdb_log_lsn_reset(DB_ENV * dbenv)
{
    char **list = NULL;
    int ret = 0;

    /* Reset log sequence numbers to allow for moving to new environment */
    ret = dbenv->log_archive(dbenv, &list, DB_ARCH_DATA | DB_ARCH_ABS);
    if (!ret) {
	char **p = list;
	for (; *p; p++)
	    if (!ret)
		ret = dbenv->lsn_reset(dbenv, *p, 0);
	list = _free(list);
    }
    return ret;
}

static int
rpmdb_convert(const char *prefix, int dbtype, int swap, int rebuild)
{
    rpmts tsCur = NULL;
    rpmts tsNew = NULL;
    rpmdb rdbNew = NULL;
    DB_ENV *dbenvNew = NULL;
    struct stat sb;

    const char * dbpath = rpmExpand("%{?_dbpath}", NULL);
    const char * __dbi_txn = rpmExpand("%{__dbi_txn}", NULL);
    const char * _dbi_tags = rpmExpand("%{_dbi_tags}", NULL);
    const char * _dbi_config = rpmExpand("%{_dbi_config}", NULL);
    const char * _dbi_config_Packages =
			rpmExpand("%{_dbi_config_Packages}", NULL);

    const char *fn = NULL;
    const char *tmppath = NULL;
    glob_t gl = {.gl_pathc = 0,.gl_pathv = NULL,.gl_offs = 0 };

    DBC *dbcpCur = NULL;
    DBC *dbcpNew = NULL;
    dbiIndex dbiCur = NULL;
    dbiIndex dbiNew = NULL;
    DB_TXN *txnidNew = NULL;

    const char *dest = NULL;
    size_t dbix;

    DBT key;
    DBT data;
    DB_TXN *txnidCur = NULL;
    uint32_t nkeys = 0;

    static int doswap = -1;		/* one shot */
    uint32_t hdrNum = 0;
    float pct = 0;
    uint8_t tmp;

    int xx;
    int yy;
    int i;

    addMacro(NULL, "__dbi_txn", NULL,
	     "create mpool txn thread thread_count=64 nofsync", -1);

    /* (ugly) clear any existing locks */
    fn = rpmGetPath(prefix && prefix[0] ? prefix : "", dbpath, "/", "__db.*", NULL);
    xx = Glob(fn, 0, NULL, &gl);
    for (i = 0; i < (int) gl.gl_pathc; i++)
	xx = Unlink(gl.gl_pathv[i]);
    fn = _free(fn);
    Globfree(&gl);

    tsCur = rpmtsCreate();
    rpmtsSetRootDir(tsCur, prefix && prefix[0] ? prefix : NULL);
    if (rpmtsOpenDB(tsCur, O_RDONLY))
	goto exit;

    if (dbtype == 1) {
	addMacro(NULL, "_dbi_tags", NULL,
	     "Packages:Name:Basenames:Group:Requirename:Providename:Conflictname:Triggername:Dirnames:Requireversion:Provideversion:Installtid:Sigmd5:Sha1header:Filedigests:Depends:Pubkeys",
	     -1);
	addMacro(NULL, "_dbi_config", NULL, "%{_dbi_htconfig}", -1);
	addMacro(NULL, "_dbi_config_Packages", NULL,
	     "%{_dbi_htconfig} lockdbfd", -1);
    }

    tsNew = rpmtsCreate();

    fn = rpmGetPath("%{_dbpath}", NULL);
    tmppath = tempnam(fn, "rpmdb_convert.XXXXXX");
    fn = _free(fn);

    addMacro(NULL, "_dbpath", NULL, tmppath, -1);
    rpmtsSetRootDir(tsNew, prefix && prefix[0] ? prefix : NULL);
    if (rpmtsOpenDB(tsNew, O_RDWR))
	goto exit;

    rdbNew = rpmtsGetRdb(tsNew);
    dbenvNew = rdbNew->db_dbenv;
    dbiCur = dbiOpen(rpmtsGetRdb(tsCur), RPMDBI_PACKAGES, 0);
    dbiNew = dbiOpen(rdbNew, RPMDBI_PACKAGES, 0);
    txnidNew = dbiTxnid(dbiNew);

    if ((xx = dbiCopen(dbiCur, NULL, NULL, 0)))
	goto exit;
    if ((xx = dbiCopen(dbiNew, txnidNew, &dbcpNew, DB_WRITECURSOR))) {
    	(void) dbiCclose(dbiCur, dbcpCur, 0);
	goto exit;
    }

    txnidCur = dbiTxnid(dbiCur);
    nkeys = 0;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    /* Acquire a cursor for the database. */
    xx = dbiCur->dbi_db->cursor(dbiCur->dbi_db, NULL, &dbcpCur, 0);
    if (xx)
	dbiCur->dbi_db->err(dbiCur->dbi_db, xx, "DB->cursor");

    xx = dbiCur->dbi_db->stat(dbiCur->dbi_db, txnidCur, &dbiCur->dbi_stats, 0);
    if (xx)
	goto exit;

    switch (dbiCur->dbi_db->type) {
    case DB_BTREE:
    case DB_RECNO:{
	DB_BTREE_STAT *db_stat = dbiCur->dbi_stats;
	nkeys = db_stat->bt_nkeys;
    }   break;
    case DB_HASH:{
	DB_HASH_STAT *db_stat = dbiCur->dbi_stats;
	nkeys = db_stat->hash_nkeys;
    }   break;
    case DB_QUEUE:{
	DB_QUEUE_STAT *db_stat = dbiCur->dbi_stats;
	nkeys = db_stat->qs_nkeys;
    }   break;
    case DB_UNKNOWN:
    default:
	xx = -1;
	goto exit;
	break;
    }

    hdrNum = 0;
    pct = 0;

    /* 
     * Older rpm places no. of keys in Packages with key = 0.
     * Any package at beginning will be "missing" from rpmdb...
     */
    if (dbtype == 1) {
	key.data = &hdrNum;
	data.data = &nkeys;
	key.size = data.size = sizeof(uint32_t);
	xx = dbiNew->dbi_db->put(dbiNew->dbi_db, NULL, &key, &data, 0);
    }

    while ((xx = dbcpCur->c_get(dbcpCur, &key, &data, DB_NEXT)) == 0) {
	tmp = pct;
	pct = (100 * (float) ++hdrNum / nkeys) + 0.5;
	/* TODO: callbacks for status output? */
	if (tmp < (int) (pct + 0.5)) {
	    fprintf(stderr, "\rconverting %s%s/Packages: %u/%u %d%%",
			prefix && prefix[0] ? prefix : "",
			tmppath, hdrNum, nkeys, (int) pct);
	}
	fflush(stdout);
	if (hdrNum == 1 && !*(uint32_t *) key.data)
	    continue;
	if (doswap < 0) {
	    if ((htole32(*(uint32_t *) key.data) > 10000000 && swap < 0)
	     || (htole32(*(uint32_t *) key.data) < 10000000 && swap > 0))
		doswap = 1;
	    else
		doswap = 0;
	}
	if (doswap) {
	    if (swap)
		*(uint32_t *) key.data = bswap32(*(uint32_t *) key.data);
	}
	xx = dbiNew->dbi_db->put(dbiNew->dbi_db, NULL, &key, &data, 0);
    }
    fprintf(stderr, "\n");

    xx = dbiCclose(dbiNew, dbcpNew, 0);
    yy = dbiCclose(dbiCur, dbcpCur, 0);
    if (xx || yy)
	goto exit;

    if (rebuild) {
	rpmVSFlags vsflags;
	size_t dbix;

	xx = rpmtsCloseDB(tsCur);

	vsflags = rpmExpandNumeric("%{_vsflags_rebuilddb}");
	vsflags |= _RPMVSF_NODIGESTS | _RPMVSF_NOSIGNATURES;
	rpmtsSetVSFlags(tsNew, vsflags);

	fprintf(stderr, "rebuilding rpmdb:\n");
	fflush(stdout);
	for (dbix = 0; dbix < rdbNew->db_ndbi; dbix++) {
	    tagStore_t dbiTags = &rdbNew->db_tags[dbix];

	    /* Remove configured secondary indices. */
	    switch (dbiTags->tag) {
	    case RPMDBI_AVAILABLE:
	    case RPMDBI_ADDED:
	    case RPMDBI_REMOVED:
#if defined(RPMDBI_DEPENDS)
	    case RPMDBI_DEPENDS:
#endif
	    case RPMDBI_BTREE:
	    case RPMDBI_HASH:
	    case RPMDBI_QUEUE:
	    case RPMDBI_RECNO:
		fprintf(stderr, "skipping %s:\t%d%%\n",
	(dbiTags->str != NULL ? dbiTags->str : tagName(dbiTags->tag)),
			(int) (100 * ((float) dbix / rdbNew->db_ndbi)));
	    case RPMDBI_PACKAGES:
	    case RPMDBI_SEQNO:
		    continue;
		    break;
	    default:
		fn = rpmGetPath(rdbNew->db_root, rdbNew->db_home, "/",
	(dbiTags->str != NULL ? dbiTags->str : tagName(dbiTags-> tag)), NULL);
		fprintf(stderr, "%s:\t", fn);
		if (!Stat(fn, &sb))
		    xx = Unlink(fn);
		fn = _free(fn);
		break;
	    }
	    /* TODO: signal handler? */

	    /* Open (and re-create) each index. */
	    (void) dbiOpen(rdbNew, dbiTags->tag, rdbNew->db_flags);
	    fprintf(stderr, "%d%%\n", (int)(100*((float)dbix/rdbNew->db_ndbi)));
	    fflush(stdout);
	}

	/* Unreference header used by associated secondary index callbacks. */
	(void) headerFree(rdbNew->db_h);
	rdbNew->db_h = NULL;

	/* Reset the Seqno counter to the maximum primary key */
	rpmlog(RPMLOG_DEBUG,
		"rpmdb: max. primary key %u\n", (unsigned) rdbNew->db_maxkey);
	fn = rpmGetPath(rdbNew->db_root, rdbNew->db_home, "/Seqno", NULL);
	if (!Stat(fn, &sb))
	    xx = Unlink(fn);
	fprintf(stderr, "%s:\t", fn);
	(void) dbiOpen(rdbNew, RPMDBI_SEQNO, rdbNew->db_flags);
	fprintf(stderr, "100%%\n");

	fn = _free(fn);

	/* Remove no longer required transaction logs */
	xx = bdb_log_archive(dbenvNew, NULL, DB_ARCH_REMOVE);
	if (!xx)
	    xx = bdb_log_lsn_reset(dbenvNew);
	(void) rpmtsCloseDB(tsNew);
    }

    if (xx)
	goto exit;

    if (rpmtsOpenDB(tsNew, O_RDONLY))
	goto exit;

    rdbNew = rpmtsGetRdb(tsNew);
    for (dbix = 0; dbix < rdbNew->db_ndbi; dbix++) {
	tagStore_t dbiTags = &rdbNew->db_tags[dbix];
	fn = rpmGetPath(rdbNew->db_root, rdbNew->db_home,
			"/", dbiTags->str, NULL);
	dest = rpmGetPath(rdbNew->db_root, dbpath, "/", dbiTags->str, NULL);
	if (!Stat(dest, &sb))
	    xx = Unlink(dest);
	if (!Stat(fn, &sb))
	    xx = Rename(fn, dest);
	fn = _free(fn);
	dest = _free(dest);
    }
    dest = rpmGetPath(rdbNew->db_root, NULL);
    xx = rpmtsCloseDB(tsNew);

    /* (ugly) cleanup */
    fn = rpmGetPath(dest, tmppath, "/", "*", NULL);
    xx = Glob(fn, 0, NULL, &gl);
    for (i = 0; i < (int) gl.gl_pathc; i++)
	xx = Unlink(gl.gl_pathv[i]);
    fn = _free(fn);
    Globfree(&gl);

    fn = rpmGetPath(dest, dbpath, "/log/", "*", NULL);
    xx = Glob(fn, 0, NULL, &gl);
    for (i = 0; i < (int) gl.gl_pathc; i++)
	xx = Unlink(gl.gl_pathv[i]);
    fn = _free(fn);
    Globfree(&gl);

    fn = rpmGetPath(dest, tmppath, "/log/", "*", NULL);
    xx = Glob(fn, 0, NULL, &gl);
    for (i = 0; i < (int) gl.gl_pathc; i++)
	xx = Unlink(gl.gl_pathv[i]);
    fn = _free(fn);
    Globfree(&gl);

    fn = rpmGetPath(dest, dbpath, "/tmp/", "*", NULL);
    xx = Glob(fn, 0, NULL, &gl);
    for (i = 0; i < (int) gl.gl_pathc; i++)
	xx = Unlink(gl.gl_pathv[i]);
    fn = _free(fn);
    Globfree(&gl);

    /* remove indices no longer used */
    fn = rpmGetPath(dest, dbpath, "Provideversion", NULL);
    if (!Stat(fn, &sb))
	xx = Unlink(fn);
    fn = _free(fn);
    fn = rpmGetPath(dest, dbpath, "Requireversion", NULL);
    if (!Stat(fn, &sb))
	xx = Unlink(fn);
    fn = _free(fn);

    /* clear locks */
    fn = rpmGetPath(prefix && prefix[0] ? prefix : "", dbpath, "/", "__db.*", NULL);
    xx = Glob(fn, 0, NULL, &gl);
    for (i = 0; i < (int) gl.gl_pathc; i++)
	xx = Unlink(gl.gl_pathv[i]);
    fn = _free(fn);
    Globfree(&gl);

    fn = rpmGetPath(dest, tmppath, "/log", NULL);
    xx = Rmdir(fn);
    fn = _free(fn);
    fn = rpmGetPath(dest, tmppath, NULL);
    xx = Rmdir(fn);
    fn = _free(fn);

    dest = _free(dest);

exit:
    tsNew = rpmtsFree(tsNew);
    tsCur = rpmtsFree(tsCur);

    addMacro(NULL, "_dbpath", NULL, dbpath, -1);
    addMacro(NULL, "__dbi_txn", NULL, __dbi_txn, -1);
    addMacro(NULL, "_dbi_tags", NULL, _dbi_tags, -1);
    addMacro(NULL, "_dbi_config", NULL, _dbi_config, -1);
    addMacro(NULL, "_dbi_config_Packages", NULL, _dbi_config_Packages, -1);

    dbpath = _free(dbpath);
    __dbi_txn = _free(__dbi_txn);
    tmppath = _free(tmppath);
    return xx;
}

static struct poptOption optionsTable[] = {
  { "root", '\0', POPT_ARG_STRING, &rootPath, 0,
	"rpm root path", "path"},

  { "btree", 'b', POPT_ARG_VAL,			&dbType, 0,
	"convert database type to btree", NULL},
  { "hash", 'h', POPT_ARG_VAL,			&dbType, 1,
	"convert database type to hash", NULL},

  { "bigendian", 'B', POPT_ARG_VAL,		&byteOrder, 1,
	"swap indexes to big endian", NULL},
  { "littleendian", 'L', POPT_ARG_VAL,		&byteOrder, -1,
	"swap indexes to little endian", NULL},

  { "rebuilddb", 'r', POPT_ARG_VAL,		&rebuildDb, 1,
	"rebuild rpm database", NULL},

    POPT_AUTOALIAS
    POPT_AUTOHELP
    POPT_TABLEEND
};

int main(int argc, char *argv[])
{
    poptContext optCon = rpmioInit(argc, argv, optionsTable);
    ARGV_t av = poptGetArgs(optCon);
    int ac = argvCount(av);
    int rc = 2;		/* assume failure */

    unsetenv("TMPDIR");
    if (ac) {
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }

    rc = rpmReadConfigFiles(NULL, NULL);

    rc = db_env_set_func_fsync(disable_fsync);

    rc = rpmdb_convert(rootPath, dbType, byteOrder, rebuildDb);

exit:
    optCon = rpmioFini(optCon);
    return rc;
}

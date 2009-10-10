/*@-type@*/ /* FIX: annotate db3 methods */
/** \ingroup db3
 * \file rpmdb/db3.c
 */

/*@unchecked@*/
static int _debug = 1;	/* XXX if < 0 debugging, > 0 unusual error returns */

#include "system.h"

#if defined(HAVE_FTOK) && defined(HAVE_SYS_IPC_H)
#include <sys/ipc.h>
#endif

#include <rpmlog.h>
#include <rpmmacro.h>
#include <rpmbf.h>
#include <rpmpgp.h>		/* XXX pgpExtractPubkeyFingerprint */
#include <rpmurl.h>		/* XXX urlPath proto */

#define	_RPMTAG_INTERNAL
#include <rpmtag.h>

#define _RPMEVR_INTERNAL	/* XXX isInstallPrereq */
#include <rpmevr.h>

#define	_RPMDB_INTERNAL
#include <rpmdb.h>

#include "debug.h"

#if !defined(DB_CLIENT)	/* XXX db-4.2.42 retrofit */
#define	DB_CLIENT	DB_RPCCLIENT
#endif

#define	DBIDEBUG(_dbi, _list)	if ((_dbi)->dbi_debug) fprintf _list

/*@access rpmdb @*/
/*@access dbiIndex @*/
/*@access dbiIndexSet @*/

#ifdef	NOTNOW
static const char * bfstring(unsigned int x, const char * xbf)
{
    const char * s = xbf;
    static char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    static char buf[BUFSIZ];
    char * t, * te;
    unsigned radix;
    unsigned c, i, k;

    radix = (s != NULL ? *s++ : 16);

    if (radix <= 1 || radix >= 32)
	radix = 16;

    t = buf;
    switch (radix) {
    case 8:	*t++ = '0';	break;
    case 16:	*t++ = '0';	*t++ = 'x';	break;
    }

    i = 0;
    k = x;
    do { i++; k /= radix; } while (k);

    te = t + i;

    k = x;
    do { --i; t[i] = digits[k % radix]; k /= radix; } while (k);

    t = te;
    i = '<';
    if (s != NULL)
    while ((c = *s++) != '\0') {
	if (c > ' ') continue;

	k = (1 << (c - 1));
	if (!(x & k)) continue;

	if (t == te) *t++ = '=';

	*t++ = i;
	i = ',';
	while (*s > ' ')
	    *t++ = *s++;
    }
    if (t > te)	*t++ = '>';
    *t = '\0';
    return buf;
}

/* XXX checked with db-4.5.20 */
static const char * dbtFlags =
	"\20\1APPMALLOC\2ISSET\3MALLOC\4PARTIAL\5REALLOC\6USERMEM\7DUPOK";

static const char * dbenvOpenFlags =
	"\20\1CREATE\2DURABLE_UNKNOWN\3FORCE\4MULTIVERSION\5NOMMAP\6RDONLY\7RECOVER\10THREAD\11TRUNCATE\12TXN_NOSYNC\13TXN_NOT_DURABLEi\14TXN_WRITE_NOSYNC\15USE_ENVIRON\16USE_ENVIRON_ROOT\17CDB\20LOCK\21LOG\22MPOOL\23REP\24TXN\25LOCKDOWN\26PRIVATE\27RECOVER_FATAL\30REGISTER\31SYSTEM_MEM";

static const char * dbOpenFlags =
	"\20\1CREATE\2DURABLE_UNKNOWN\3FORCE\4MULTIVERSION\5NOMMAP\6RDONLY\7RECOVER\10THREAD\11TRUNCATE\12TXN_NOSYNC\13TXN_NOT_DURABLEi\14TXN_WRITE_NOSYNC\15USE_ENVIRON\16USE_ENVIRON_ROOT\17EXCL\20FCNTL_LOCKING\21NO_AUTO_COMMIT\22RDWRMASTER\23WRITEOPEN";

static const char * dbenvSetFlags =
	"\20\1CREATE\2DURABLE_UNKNOWN\3FORCE\4MULTIVERSION\5NOMMAP\6RDONLY\7RECOVER\10THREAD\11TRUNCATE\12TXN_NOSYNC\13TXN_NOT_DURABLEi\14TXN_WRITE_NOSYNC\15USE_ENVIRON\16USE_ENVIRON_ROOT\17CDB_ALLDB\20DIRECT_DB\21DIRECT_LOG\22DSYNC_DB\23DSYNC_LOG\24LOG_AUTOREMOVE\25LOG_INMEMORY\26NOLOCKING\27NOPANIC\30OVERWRITE\31PANIC_ENV\36REGION_INIT\37TIME_NOTGRANTED\40YIELDCPU";

static const char * dbSetFlags =
	"\20\1CREATE\2DURABLE_UNKNOWN\3FORCE\4MULTIVERSION\5NOMMAP\6RDONLY\7RECOVER\10THREAD\11TRUNCATE\12TXN_NOSYNC\13TXN_NOT_DURABLEi\14TXN_WRITE_NOSYNC\15USE_ENVIRON\16USE_ENVIRON_ROOT\17CHKSUM\20DUP\21DUPSORT\22ENCRYPT\23INORDER\24RECNUM\25RENUMBER\26REVSPLITOFF\27SNAPSHOT";

static const char * dbiModeFlags =
	"\20\1WRONLY\2RDWR\7CREAT\10EXCL\11NOCTTY\12TRUNC\13APPEND\14NONBLOCK\15SYNC\16ASYNC\17DIRECT\20LARGEFILE\21DIRECTORY\22NOFOLLOW";
#endif	/* NOTNOW */

typedef struct key_s {
    uint32_t	v;
    const char *n;
} KEY;

static const char * tblName(uint32_t v, KEY * tbl, size_t ntbl)
{
    const char * n = NULL;
    static char buf[32];
    size_t i;

    for (i = 0; i < ntbl; i++) {
	if (v != tbl[i].v)
	    continue;
	n = tbl[i].n;
	break;
    }
    if (n == NULL) {
	snprintf(buf, sizeof(buf), "0x%x", v);
	n = buf;
    }
    return n;
}

static const char * fmtBits(uint32_t flags, KEY tbl[], size_t ntbl, char *t)
{
    char pre = '<';
    char * te = t;
    int i;

    sprintf(t, "0x%x", flags);
    te = t;
    te += strlen(te);
    for (i = 0; i < 32; i++) {
	uint32_t mask = (1 << i);
	const char * name;

	if (!(flags & mask))
	    continue;

	name = tblName(mask, tbl, ntbl);
	*te++ = pre;
	pre = ',';
	te = stpcpy(te, name);
    }
    if (pre == ',') *te++ = '>';
    *te = '\0';
    return t;
}

#define _ENTRY(_v)      { DB_##_v, #_v, }

static KEY DBeflags[] = {
    _ENTRY(INIT_CDB),
    _ENTRY(INIT_LOCK),
    _ENTRY(INIT_LOG),
    _ENTRY(INIT_MPOOL),
    _ENTRY(INIT_REP),
    _ENTRY(INIT_TXN),
    _ENTRY(RECOVER),
    _ENTRY(RECOVER_FATAL),
    _ENTRY(USE_ENVIRON),
    _ENTRY(USE_ENVIRON_ROOT),
    _ENTRY(CREATE),
    _ENTRY(LOCKDOWN),
    _ENTRY(FAILCHK),
    _ENTRY(PRIVATE),
    _ENTRY(REGISTER),
    _ENTRY(SYSTEM_MEM),
    _ENTRY(THREAD),
};
static size_t nDBeflags = sizeof(DBeflags) / sizeof(DBeflags[0]);
static const char * fmtDBeflags(uint32_t flags)
{
    static char buf[BUFSIZ];
    char * te = buf;
    te = stpcpy(te, "\n\tflags: ");
    (void) fmtBits(flags, DBeflags, nDBeflags, te);
    return buf;
}
#define	_EFLAGS(_eflags)	fmtDBeflags(_eflags)

static KEY DBoflags[] = {
    _ENTRY(AUTO_COMMIT),
    _ENTRY(CREATE),
    _ENTRY(EXCL),
    _ENTRY(MULTIVERSION),
    _ENTRY(NOMMAP),
    _ENTRY(RDONLY),
    _ENTRY(READ_UNCOMMITTED),
    _ENTRY(THREAD),
    _ENTRY(TRUNCATE),
};
static size_t nDBoflags = sizeof(DBoflags) / sizeof(DBoflags[0]);
static const char * fmtDBoflags(uint32_t flags)
{
    static char buf[BUFSIZ];
    char * te = buf;
    te = stpcpy(te, "\n\tflags: ");
    (void) fmtBits(flags, DBoflags, nDBoflags, te);
    return buf;
}
#define	_OFLAGS(_oflags)	fmtDBoflags(_oflags)

static KEY DBaflags[] = {
    _ENTRY(CREATE),
    _ENTRY(IMMUTABLE_KEY),
};
static size_t nDBaflags = sizeof(DBaflags) / sizeof(DBaflags[0]);
static const char * fmtDBaflags(uint32_t flags)
{
    static char buf[BUFSIZ];
    char * te = buf;
    te = stpcpy(te, "\n\tflags: ");
    (void) fmtBits(flags, DBaflags, nDBaflags, te);
    return buf;
}
#define	_AFLAGS(_aflags)	fmtDBaflags(_aflags)

static KEY DBafflags[] = {
    _ENTRY(FOREIGN_ABORT),
    _ENTRY(FOREIGN_CASCADE),
    _ENTRY(FOREIGN_NULLIFY),
};
static size_t nDBafflags = sizeof(DBafflags) / sizeof(DBafflags[0]);
static const char * fmtDBafflags(uint32_t flags)
{
    static char buf[BUFSIZ];
    char * te = buf;
    te = stpcpy(te, "\n\tflags: ");
    (void) fmtBits(flags, DBafflags, nDBafflags, te);
    return buf;
}
#define	_AFFLAGS(_afflags)	fmtDBafflags(_afflags)

static KEY DBCflags[] = {
    _ENTRY(AFTER),		/* Dbc.put */
    _ENTRY(APPEND),		/* Db.put */
    _ENTRY(BEFORE),		/* Dbc.put */
    _ENTRY(CONSUME),		/* Db.get */
    _ENTRY(CONSUME_WAIT),	/* Db.get */
    _ENTRY(CURRENT),		/* Dbc.get, Dbc.put, DbLogc.get */
    _ENTRY(FIRST),		/* Dbc.get, DbLogc->get */
    _ENTRY(GET_BOTH),		/* Db.get, Dbc.get */
    _ENTRY(GET_BOTHC),		/* Dbc.get (internal) */
    _ENTRY(GET_BOTH_RANGE),	/* Db.get, Dbc.get */
    _ENTRY(GET_RECNO),		/* Dbc.get */
    _ENTRY(JOIN_ITEM),		/* Dbc.get; don't do primary lookup */
    _ENTRY(KEYFIRST),		/* Dbc.put */
    _ENTRY(KEYLAST),		/* Dbc.put */
    _ENTRY(LAST),		/* Dbc.get, DbLogc->get */
    _ENTRY(NEXT),		/* Dbc.get, DbLogc->get */
    _ENTRY(NEXT_DUP),		/* Dbc.get */
    _ENTRY(NEXT_NODUP),		/* Dbc.get */
    _ENTRY(NODUPDATA),		/* Db.put, Dbc.put */
    _ENTRY(NOOVERWRITE),	/* Db.put */
    _ENTRY(NOSYNC),		/* Db.close */
    _ENTRY(OVERWRITE_DUP),	/* Dbc.put, Db.put; no DB_KEYEXIST */
    _ENTRY(POSITION),		/* Dbc.dup */
    _ENTRY(PREV),		/* Dbc.get, DbLogc->get */
    _ENTRY(PREV_DUP),		/* Dbc.get */
    _ENTRY(PREV_NODUP),		/* Dbc.get */
    _ENTRY(SET),		/* Dbc.get, DbLogc->get */
    _ENTRY(SET_RANGE),		/* Dbc.get */
    _ENTRY(SET_RECNO),		/* Db.get, Dbc.get */
    _ENTRY(UPDATE_SECONDARY),	/* Dbc.get, Dbc.del (internal) */
    _ENTRY(SET_LTE),		/* Dbc.get (internal) */
    _ENTRY(GET_BOTH_LTE),	/* Dbc.get (internal) */

    _ENTRY(IGNORE_LEASE),
    _ENTRY(READ_COMMITTED),
    _ENTRY(READ_UNCOMMITTED),
    _ENTRY(MULTIPLE),
    _ENTRY(MULTIPLE_KEY),
    _ENTRY(RMW),
};
static size_t nDBCflags = sizeof(DBCflags) / sizeof(DBCflags[0]);
static const char * fmtDBCflags(uint32_t flags)
{
    static char buf[BUFSIZ];
    char * te = buf;
    uint32_t op = (flags & DB_OPFLAGS_MASK);
    flags &= ~DB_OPFLAGS_MASK;

    te = stpcpy(te, "\n\tflags: ");
    if (op) {
	te = stpcpy( stpcpy(te, "DB_"), tblName(op, DBCflags, nDBCflags));
	*te++ = ' ';
	*te = '\0';
    }
    if (flags)
	(void) fmtBits(flags, DBCflags, nDBCflags, te);
    return buf;
}
#define	_DBCFLAGS(_flags)	fmtDBCflags(_flags)

#define _DBT_ENTRY(_v)      { DB_DBT_##_v, #_v, }
static KEY DBTflags[] = {
    _DBT_ENTRY(MALLOC),
    _DBT_ENTRY(REALLOC),
    _DBT_ENTRY(USERMEM),
    _DBT_ENTRY(PARTIAL),
    _DBT_ENTRY(APPMALLOC),
    _DBT_ENTRY(MULTIPLE),
};
static size_t nDBTflags = sizeof(DBTflags) / sizeof(DBTflags[0]);
static char * fmtDBT(const DBT * K, char * te)
{
    static size_t keymax = 35;
    int unprintable;
    uint32_t i;

    sprintf(te, "%p[%u]\t", K->data, (unsigned)K->size);
    te += strlen(te);
    (void) fmtBits(K->flags, DBTflags, nDBTflags, te);
    te += strlen(te);
    if (K->data && K->size > 0) {
 	uint8_t * _u;
	size_t _nu;

	/* Grab the key data/size. */
	if (K->flags & DB_DBT_MULTIPLE) {
	    DBT * _K = K->data;
	    _u = _K->data;
	    _nu = _K->size;
	} else {
	    _u = K->data;
	    _nu = K->size;
	}
	/* Verify if data is a string. */
	unprintable = 0;
	for (i = 0; i < _nu; i++)
	    unprintable |= !xisprint(_u[i]);

	/* Display the data. */
	if (!unprintable) {
	    size_t nb = (_nu < keymax ? _nu : keymax);
	    char * ellipsis = (_nu < keymax ? "" : "...");
	    sprintf(te, "\t\"%.*s%s\"", nb, (char *)_u, ellipsis);
	} else {
	    switch (_nu) {
	    default: break;
	    case 4:	sprintf(te, "\t%u", *(uint32_t *)_u); break;
	    }
	}

	te += strlen(te);
	*te = '\0';
    }
    return te;
}
static const char * fmtKDR(const DBT * K, const DBT * D, const DBT * R)
{
    static char buf[BUFSIZ];
    char * te = buf;

    if (K) {
	te = stpcpy(te, "\n\t  key: ");
	te = fmtDBT(K, te);
    }
    if (D) {
	te = stpcpy(te, "\n\t data: ");
	te = fmtDBT(D, te);
    }
    if (R) {
	te = stpcpy(te, "\n\t  res: ");
	te = fmtDBT(R, te);
    }
    *te = '\0';
    
    return buf;
}
#define	_KEYDATA(_K, _D, _R)	fmtKDR(_K, _D, _R)

#undef	_ENTRY

/*@-globuse -mustmod @*/	/* FIX: rpmError not annotated yet. */
static int cvtdberr(/*@unused@*/ dbiIndex dbi, const char * msg,
		int error, int printit)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    int rc = error;

    if (printit && rc) {
	/*@-moduncon@*/ /* FIX: annotate db3 methods */
	if (msg)
	    rpmlog(RPMLOG_ERR, _("db%d error(%d) from %s: %s\n"),
		DB_VERSION_MAJOR, rc, msg, db_strerror(error));
	else
	    rpmlog(RPMLOG_ERR, _("db%d error(%d): %s\n"),
		DB_VERSION_MAJOR, rc, db_strerror(error));
	/*@=moduncon@*/
    }

    return rc;
}
/*@=globuse =mustmod @*/

/**
 * Return (possibly renamed) tagName. Handles arbitrary tags.
 * @param rpmdb		rpm database
 * @param dbi		rpm database index
 * @return		tag string
 */
/*@observer@*/
static const char * mapTagName(rpmdb rpmdb, dbiIndex dbi)
	/*@*/
{
    tagStore_t dbiTags = rpmdb->db_tags;
    size_t dbix = 0;

    if (dbiTags != NULL)
    while (dbix < rpmdb->db_ndbi) {
	if (dbi->dbi_rpmtag == dbiTags->tag)
	    return dbiTags->str;
	dbiTags++;
	dbix++;
    }
    /* XXX should never reach here */
    return tagName(dbi->dbi_rpmtag);
}

static int db_fini(dbiIndex dbi, const char * dbhome,
		/*@null@*/ const char * dbfile,
		/*@unused@*/ /*@null@*/ const char * dbsubfile)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    rpmdb rpmdb = dbi->dbi_rpmdb;
    DB_ENV * dbenv = rpmdb->db_dbenv;
    int rc;

DBIDEBUG(dbi, (stderr, "--> %s(%p,%s,%s,%s)\n", __FUNCTION__, dbi, dbhome, dbfile, dbsubfile));

    if (dbenv == NULL)
	return 0;

    rc = dbenv->close(dbenv, 0);
    rc = cvtdberr(dbi, "dbenv->close", rc, _debug);
    rpmdb->db_dbenv = NULL;

    if (dbfile)
	rpmlog(RPMLOG_DEBUG, D_("closed   db environment %s/%s\n"),
			dbhome, dbfile);

    if (rpmdb->db_remove_env) {
	int xx;

	/*@-moduncon@*/ /* FIX: annotate db3 methods */
	xx = db_env_create(&dbenv, 0);
	/*@=moduncon@*/
	if (!xx && dbenv != NULL) {
	    xx = cvtdberr(dbi, "db_env_create", xx, _debug);
#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR != 0) || (DB_VERSION_MAJOR == 4)
	    xx = dbenv->remove(dbenv, dbhome, DB_FORCE);
#else
	    xx = dbenv->remove(dbenv, dbhome, NULL, 0);
#endif
	    xx = cvtdberr(dbi, "dbenv->remove", xx, _debug);

	    if (dbfile)
		rpmlog(RPMLOG_DEBUG, D_("removed  db environment %s/%s\n"),
			dbhome, dbfile);
	}

    }
    return rc;
}

static int db3_fsync_disable(/*@unused@*/ int fd)
	/*@*/
{
    return 0;
}

#if (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 5)
/**
 * Is process/thread still alive?
 * @param dbenv		db environment
 * @param pid		process id
 * @param tid		thread id
 * @param flags		0 or DB_MUTEX_PROCESS_ONLY
 * @return		
 */
static int db3is_alive(/*@unused@*/ DB_ENV *dbenv, pid_t pid,
		/*@unused@*/ db_threadid_t tid,
		rpmuint32_t flags)
	/*@*/
{
    int is_alive = 1;	/* assume all processes are alive */

    switch (flags) {
    case DB_MUTEX_PROCESS_ONLY:
    case 0:
    default:
	is_alive = (!(kill(pid, 0) < 0 && errno == ESRCH));
	break;
    }
    return is_alive;
}
#endif

/*@-moduncon@*/ /* FIX: annotate db3 methods */
static int db_init(dbiIndex dbi, const char * dbhome,
		/*@null@*/ const char * dbfile,
		/*@unused@*/ /*@null@*/ const char * dbsubfile,
		/*@out@*/ DB_ENV ** dbenvp)
	/*@globals rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies dbi, *dbenvp, fileSystem, internalState @*/
{
    static int oneshot = 0;
    rpmdb rpmdb = dbi->dbi_rpmdb;
    DB_ENV *dbenv = NULL;
    int eflags;
    int rc;
    int xx;

    if (!oneshot) {
#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR != 0) || (DB_VERSION_MAJOR == 4)
	xx = db_env_set_func_open((int (*)(const char *, int, ...))Open);
	xx = cvtdberr(dbi, "db_env_set_func_open", xx, _debug);
#endif
	oneshot++;
    }

    if (dbenvp == NULL)
	return 1;

    /* XXX HACK */
    /*@-assignexpose@*/
    if (rpmdb->db_errfile == NULL)
	rpmdb->db_errfile = stderr;
    /*@=assignexpose@*/

    eflags = (dbi->dbi_oeflags | dbi->dbi_eflags);
    /* Try to join, rather than create, the environment. */
    /* XXX DB_JOINENV is defined to 0 in db-4.5.20 */
    if (eflags & DB_JOINENV) eflags &= DB_JOINENV;

    if (dbfile)
	rpmlog(RPMLOG_DEBUG, D_("opening  db environment %s/%s %s\n"),
		dbhome, dbfile, prDbiOpenFlags(eflags, 1));

    /* XXX Can't do RPC w/o host. */
    if (dbi->dbi_host == NULL)
	dbi->dbi_ecflags &= ~DB_CLIENT;

    /* XXX Set a default shm_key. */
    if ((dbi->dbi_eflags & DB_SYSTEM_MEM) && dbi->dbi_shmkey == 0) {
#if defined(HAVE_FTOK)
	dbi->dbi_shmkey = ftok(dbhome, 0);
#else
	dbi->dbi_shmkey = 0x44631380;
#endif
    }

    rc = db_env_create(&dbenv, dbi->dbi_ecflags);
    rc = cvtdberr(dbi, "db_env_create", rc, _debug);
    if (dbenv == NULL || rc)
	goto errxit;

    /*@-noeffectuncon@*/ /* FIX: annotate db3 methods */

 /* 4.1: dbenv->set_app_dispatch(???) */
 /* 4.1: dbenv->set_alloc(???) */
 /* 4.1: dbenv->set_data_dir(???) */
 /* 4.1: dbenv->set_encrypt(???) */

/*@-castfcnptr@*/
    dbenv->set_errcall(dbenv, (void *)rpmdb->db_errcall);
/*@=castfcnptr@*/
    dbenv->set_errfile(dbenv, rpmdb->db_errfile);
    dbenv->set_errpfx(dbenv, rpmdb->db_errpfx);
    /*@=noeffectuncon@*/

 /* 4.1: dbenv->set_feedback(???) */
 /* 4.1: dbenv->set_flags(???) */

 /* dbenv->set_paniccall(???) */

    if ((dbi->dbi_ecflags & DB_CLIENT) && dbi->dbi_host) {
	const char * home;
	int retry = 0;

	if ((home = strrchr(dbhome, '/')) != NULL)
	    dbhome = ++home;

	while (retry++ < 5) {
/* XXX 3.3.4 change. */
#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 3) || (DB_VERSION_MAJOR == 4)
	    xx = dbenv->set_rpc_server(dbenv, NULL, dbi->dbi_host,
		dbi->dbi_cl_timeout, dbi->dbi_sv_timeout, 0);
	    xx = cvtdberr(dbi, "dbenv->set_server", xx, _debug);
#else
	    xx = dbenv->set_server(dbenv, dbi->dbi_host,
		dbi->dbi_cl_timeout, dbi->dbi_sv_timeout, 0);
	    xx = cvtdberr(dbi, "dbenv->set_server", xx, _debug);
#endif
	    if (!xx)
		break;
	    (void) sleep(15);
	}
    } else {

	if (dbi->dbi_tmpdir) {
	    const char * root;
	    const char * tmpdir;

	    root = (dbi->dbi_root ? dbi->dbi_root : rpmdb->db_root);
	    if ((root[0] == '/' && root[1] == '\0') || rpmdb->db_chrootDone)
		root = NULL;
/*@-mods@*/
	    tmpdir = rpmGenPath(root, dbi->dbi_tmpdir, NULL);
/*@=mods@*/
	    xx = dbenv->set_tmp_dir(dbenv, tmpdir);
	    xx = cvtdberr(dbi, "dbenv->set_tmp_dir", xx, _debug);
	    tmpdir = _free(tmpdir);
	}
    }

/* ==== Locking: */
/* ==== Logging: */
/* ==== Memory pool: */
/* ==== Mutexes: */
/* ==== Replication: */
/* ==== Sequences: */
/* ==== Transactions: */

/* ==== Other: */
    if (dbi->dbi_no_fsync) {
#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR != 0) || (DB_VERSION_MAJOR == 4)
	xx = db_env_set_func_fsync(db3_fsync_disable);
#else
	xx = dbenv->set_func_fsync(dbenv, db3_fsync_disable);
#endif
	xx = cvtdberr(dbi, "db_env_set_func_fsync", xx, _debug);
    }

    if (dbi->dbi_shmkey) {
	xx = dbenv->set_shm_key(dbenv, dbi->dbi_shmkey);
	xx = cvtdberr(dbi, "dbenv->set_shm_key", xx, _debug);
    }

#if (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 5)
    /* XXX capture dbenv->falchk output on stderr. */
/*@-noeffectuncon@*/
    dbenv->set_msgfile(dbenv, rpmdb->db_errfile);
/*@=noeffectuncon@*/
    if (dbi->dbi_thread_count >= 8) {
	xx = dbenv->set_thread_count(dbenv, dbi->dbi_thread_count);
	xx = cvtdberr(dbi, "dbenv->set_thread_count", xx, _debug);
    }
#endif

    if (eflags & DB_RECOVER) eflags |= DB_CREATE;
#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR != 0) || (DB_VERSION_MAJOR == 4)
    rc = (dbenv->open)(dbenv, dbhome, eflags, dbi->dbi_perms);
#else
    rc = (dbenv->open)(dbenv, dbhome, NULL, eflags, dbi->dbi_perms);
#endif
    xx = _debug;
#if defined(DB_VERSION_MISMATCH)
    if (rc == DB_VERSION_MISMATCH) xx = 0;
#endif
    if (rc == EINVAL) xx = 0;
    rc = cvtdberr(dbi, "dbenv->open", rc, xx);
    if (rc)
	goto errxit;

#if (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 5)
    if (!rpmdb->db_verifying && !rpmdb->db_rebuilding && dbi->dbi_thread_count >= 8) {
	/* XXX Set pid/tid is_alive probe. */
	xx = dbenv->set_isalive(dbenv, db3is_alive);
	xx = cvtdberr(dbi, "dbenv->set_isalive", xx, _debug);
	/* XXX Clean out stale shared read locks. */
	xx = dbenv->failchk(dbenv, 0);
	xx = cvtdberr(dbi, "dbenv->failchk", xx, _debug);
	if (xx == DB_RUNRECOVERY) {
	    rc = xx;
	    goto errxit;
	}
    }
#endif

    *dbenvp = dbenv;

DBIDEBUG(dbi, (stderr, "<-- %s(%p(%s),%s,%s,%s,%p) dbenv %p %s\n", __FUNCTION__, dbi, tagName(dbi->dbi_rpmtag), dbhome, dbfile, dbsubfile, dbenvp, dbenv, _EFLAGS(eflags)));

    return 0;

errxit:
    if (dbenv) {
	xx = dbenv->close(dbenv, 0);
	xx = cvtdberr(dbi, "dbenv->close", xx, _debug);
    }
    return rc;
}
/*@=moduncon@*/

#ifdef	NOTYET
/*@-mustmod@*/
static int db3remove(dbiIndex dbi, /*@null@*/ const char * dbfile,
		/*@unused@*/ /*@null@*/ const char * dbsubfile,
		unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, fileSystem @*/
{
    DB * db = dbi->dbi_db;
    int rc;

assert(db != NULL);
    rc = db->remove(db, dbfile, dbsubfile, flags);
    rc = cvtdberr(dbi, "db->remove", rc, _debug);

DBIDEBUG(dbi, (stderr, "<-- %s(%p,%s,%s,0x%x) rc %d\n", __FUNCTION__, dbi, dbfile, dbsubfile, flags, rc));

    return rc;
}
/*@=mustmod@*/

/*@-mustmod@*/
static int db3rename(dbiIndex dbi, /*@null@*/ const char * dbfile,
		/*@unused@*/ /*@null@*/ const char * dbsubfile,
		/*@unused@*/ /*@null@*/ const char * newname,
		unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, fileSystem @*/
{
    DB * db = dbi->dbi_db;
    int rc;

assert(db != NULL);
    rc = db->rename(db, dbfile, dbsubfile, newname, flags);
    rc = cvtdberr(dbi, "db->rename", rc, _debug);

DBIDEBUG(dbi, (stderr, "<-- %s(%p,%s,%s,%s,0x%x) rc %d %s\n", __FUNCTION__, dbi, dbfile, dbsubfile, newname, flags, rc, _DBCFLAGS(flags)));

    return rc;
}
/*@=mustmod@*/

/*@-mustmod@*/
static int db3truncate(dbiIndex dbi, unsigned int * countp, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *countp, fileSystem @*/
{
    DB * db = dbi->dbi_db;
    DB_TXN * _txnid = dbiTxnid(dbi);
    int rc;

assert(db != NULL);
    rc = db->truncate(db, _txnid, countp, flags);
    rc = cvtdberr(dbi, "db->truncate", rc, _debug);

DBIDEBUG(dbi, (stderr, "<-- %s(%p,%p,0x%x) rc %d\n", __FUNCTION__, dbi, countp, flags, rc));

    return rc;
}
/*@=mustmod@*/

/*@-mustmod@*/
static int db3upgrade(dbiIndex dbi, /*@null@*/ const char * dbfile,
		unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    DB * db = dbi->dbi_db;
    int rc;

assert(db != NULL);
    rc = db->upgrade(db, dbfile, flags);
    rc = cvtdberr(dbi, "db->upgrade", rc, _debug);

DBIDEBUG(dbi, (stderr, "<-- %s(%p,%s,0x%x) rc %d\n", __FUNCTION__, dbi, dbfile, flags, rc));

    return rc;
}
/*@=mustmod@*/
#endif	/* NOTYET */

/*@-mustmod@*/
static int db3verify(dbiIndex dbi, /*@null@*/ const char * dbfile,
		/*@unused@*/ /*@null@*/ const char * dbsubfile, FILE * fp,
		unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    DB * db = dbi->dbi_db;
    int rc;

assert(db != NULL);
    rc = db->verify(db, dbfile, dbsubfile, fp, flags);
    rc = cvtdberr(dbi, "db->verify", rc, _debug);

DBIDEBUG(dbi, (stderr, "<-- %s(%p,%s,%s,%p,0x%x) rc %d\n", __FUNCTION__, dbi, dbfile, dbsubfile, fp, flags, rc));

    return rc;
}
/*@=mustmod@*/

static int db3sync(dbiIndex dbi, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    DB * db = dbi->dbi_db;
    int rc = 0;
    int _printit;

    if (db != NULL)
	rc = db->sync(db, flags);
#if (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 1)
    _printit = _debug;
#else
    /* XXX DB_INCOMPLETE is returned occaisionally with multiple access. */
    _printit = (rc == DB_INCOMPLETE ? 0 : _debug);
#endif
    rc = cvtdberr(dbi, "db->sync", rc, _printit);

DBIDEBUG(dbi, (stderr, "<-- %s(%p,0x%x) rc %d\n", __FUNCTION__, dbi, flags, rc));

    return rc;
}

/*@-mustmod@*/
static int db3exists(dbiIndex dbi, DBT * key, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    DB * db = dbi->dbi_db;
    DB_TXN * _txnid = dbiTxnid(dbi);
    int _printit;
    int rc;

assert(db != NULL);
    rc = db->exists(db, _txnid, key, flags);
    /* XXX DB_NOTFOUND can be returned */
    _printit = (rc == DB_NOTFOUND ? 0 : _debug);
    rc = cvtdberr(dbi, "db->exists", rc, _printit);

DBIDEBUG(dbi, (stderr, "<-- %s(%p,%p,0x%x) rc %d %s\n", __FUNCTION__, dbi, key, flags, rc, _KEYDATA(key, NULL, NULL)));

    return rc;
}
/*@=mustmod@*/

/*@-mustmod@*/
static int db3seqno(dbiIndex dbi, int64_t * seqnop, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *seqnop, fileSystem @*/
{
    DB * db = dbi->dbi_db;
    DB_TXN * _txnid = dbiTxnid(dbi);
    DB_SEQUENCE * seq = dbi->dbi_seq;
    int32_t _delta = 1;
    db_seq_t seqno = 0;
    int rc;

assert(db != NULL);
assert(seq != NULL);

    if (seqnop && *seqnop)
	_delta = *seqnop;

    rc = seq->get(seq, _txnid, _delta, &seqno, 0);  
    rc = cvtdberr(dbi, "seq->get", rc, _debug);
    if (rc) goto exit;

    if (seqnop)
	*seqnop = seqno;

exit:
DBIDEBUG(dbi, (stderr, "<-- %s(%p,%p,0x%x) seqno %lld rc %d\n", __FUNCTION__, dbi, seqnop, flags, (long long)seqno, rc));

    return rc;
}
/*@=mustmod@*/

static int db3cdup(dbiIndex dbi, DBC * dbcursor, DBC ** dbcp,
		unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *dbcp, fileSystem @*/
{
    int rc;

    if (dbcp) *dbcp = NULL;
#if (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 6)
    rc = dbcursor->dup(dbcursor, dbcp, flags);
    rc = cvtdberr(dbi, "dbcursor->dup", rc, _debug);
#else
    rc = dbcursor->c_dup(dbcursor, dbcp, flags);
    rc = cvtdberr(dbi, "dbcursor->c_dup", rc, _debug);
#endif

DBIDEBUG(dbi, (stderr, "<-- %s(%p,%p,%p,0x%x) rc %d\n", __FUNCTION__, dbi, dbcursor, dbcp, flags, rc));

    return rc;
}

/*@-mustmod@*/
static int db3cclose(dbiIndex dbi, /*@only@*/ /*@null@*/ DBC * dbcursor,
		/*@unused@*/ unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, fileSystem @*/
{
    int rc = -2;

    /* XXX db3copen error pathways come through here. */
    if (dbcursor != NULL) {
#if (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 6)
	rc = dbcursor->close(dbcursor);
	rc = cvtdberr(dbi, "dbcursor->close", rc, _debug);
#else
	rc = dbcursor->c_close(dbcursor);
	rc = cvtdberr(dbi, "dbcursor->c_close", rc, _debug);
#endif
    }

DBIDEBUG(dbi, (stderr, "<-- %s(%p,%p,0x%x) rc %d\n", __FUNCTION__, dbi, dbcursor, flags, rc));

    return rc;
}
/*@=mustmod@*/

static int db3copen(dbiIndex dbi, DB_TXN * txnid,
		/*@null@*/ /*@out@*/ DBC ** dbcp, unsigned int dbiflags)
	/*@globals fileSystem @*/
	/*@modifies dbi, *dbcp, fileSystem @*/
{
    DB * db = dbi->dbi_db;
    DBC * dbcursor = NULL;
    int flags;
    int rc;

   /* XXX DB_WRITECURSOR cannot be used with sunrpc dbenv. */
    assert(db != NULL);
    if ((dbiflags & DB_WRITECURSOR) &&
	(dbi->dbi_eflags & DB_INIT_CDB) && !(dbi->dbi_oflags & DB_RDONLY))
    {
	flags = DB_WRITECURSOR;
    } else
	flags = 0;

    rc = db->cursor(db, txnid, &dbcursor, flags);
    rc = cvtdberr(dbi, "db->cursor", rc, _debug);

    if (dbcp)
	*dbcp = dbcursor;
    else
	(void) db3cclose(dbi, dbcursor, 0);

DBIDEBUG(dbi, (stderr, "<-- %s(%p,%p,%p,0x%x) dbc %p rc %d\n", __FUNCTION__, dbi, txnid, dbcp, dbiflags, dbcursor, rc));
    return rc;
}

static int db3cput(dbiIndex dbi, DBC * dbcursor, DBT * key, DBT * data,
		/*@unused@*/ unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    DB * db = dbi->dbi_db;
    DB_TXN * _txnid = dbiTxnid(dbi);
    int rc;

    assert(db != NULL);
    if (dbcursor == NULL) {
flags = 0;
	rc = db->put(db, _txnid, key, data, flags);
	rc = cvtdberr(dbi, "db->put", rc, _debug);
    } else {
flags = DB_KEYLAST;
#if (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 6)
	rc = dbcursor->put(dbcursor, key, data, flags);
	rc = cvtdberr(dbi, "dbcursor->put", rc, _debug);
#else
	rc = dbcursor->c_put(dbcursor, key, data, flags);
	rc = cvtdberr(dbi, "dbcursor->c_put", rc, _debug);
#endif
    }

DBIDEBUG(dbi, (stderr, "<-- %s(%p,%p,%p,%p,0x%x) rc %d %s%s\n", __FUNCTION__, dbi, dbcursor, key, data, flags, rc, _DBCFLAGS(flags), _KEYDATA(key, data, NULL)));
    return rc;
}

/*@-mustmod@*/
static int db3cget(dbiIndex dbi, DBC * dbcursor, DBT * key, DBT * data,
		unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *dbcursor, *key, *data, fileSystem @*/
{
    DB * db = dbi->dbi_db;
    DB_TXN * _txnid = dbiTxnid(dbi);
    int _printit;
    int rc;

assert(db != NULL);
    if (dbcursor == NULL) {
	/* XXX duplicates require cursors. */
	rc = db->get(db, _txnid, key, data, flags);
	/* XXX DB_NOTFOUND can be returned */
	_printit = (rc == DB_NOTFOUND ? 0 : _debug);
	rc = cvtdberr(dbi, "db->get", rc, _printit);
    } else {
#if (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 6)
	/* XXX db3 does DB_FIRST on uninitialized cursor */
	rc = dbcursor->get(dbcursor, key, data, flags);
	/* XXX DB_NOTFOUND can be returned */
	_printit = (rc == DB_NOTFOUND ? 0 : _debug);
	/* XXX Permit DB_BUFFER_SMALL to be returned (more restrictive?) */
	_printit = (rc == DB_BUFFER_SMALL ? 0 : _printit);
	rc = cvtdberr(dbi, "dbcursor->get", rc, _printit);
#else
	/* XXX db3 does DB_FIRST on uninitialized cursor */
	rc = dbcursor->c_get(dbcursor, key, data, flags);
	/* XXX DB_NOTFOUND can be returned */
	_printit = (rc == DB_NOTFOUND ? 0 : _debug);
	rc = cvtdberr(dbi, "dbcursor->c_get", rc, _printit);
#endif
    }

DBIDEBUG(dbi, (stderr, "<-- %s(%p,%p,%p,%p,0x%x) rc %d %s%s\n", __FUNCTION__, dbi, dbcursor, key, data, flags, rc, _DBCFLAGS(flags), _KEYDATA(key, data, NULL)));
    return rc;
}
/*@=mustmod@*/

/*@-mustmod@*/
static int db3cpget(dbiIndex dbi, DBC * dbcursor, DBT * key, DBT * pkey,
		DBT * data, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *dbcursor, *key, *data, fileSystem @*/
{
    DB * db = dbi->dbi_db;
    DB_TXN * _txnid = dbiTxnid(dbi);
    int _printit;
    int rc;

assert(db != NULL);
    if (dbcursor == NULL) {
	/* XXX duplicates require cursors. */
	rc = db->pget(db, _txnid, key, pkey, data, flags);
	/* XXX DB_NOTFOUND can be returned */
	_printit = (rc == DB_NOTFOUND ? 0 : _debug);
	rc = cvtdberr(dbi, "db->pget", rc, _printit);
    } else {
#if (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 6)
	/* XXX db3 does DB_FIRST on uninitialized cursor */
	rc = dbcursor->pget(dbcursor, key, pkey, data, flags);
	/* XXX DB_NOTFOUND can be returned */
	_printit = (rc == DB_NOTFOUND ? 0 : _debug);
	rc = cvtdberr(dbi, "dbcursor->pget", rc, _printit);
#else
	/* XXX db3 does DB_FIRST on uninitialized cursor */
	rc = dbcursor->c_pget(dbcursor, key, pkey, data, flags);
	/* XXX DB_NOTFOUND can be returned */
	_printit = (rc == DB_NOTFOUND ? 0 : _debug);
	rc = cvtdberr(dbi, "dbcursor->c_pget", rc, _printit);
#endif
    }

DBIDEBUG(dbi, (stderr, "<-- %s(%p,%p,%p,%p,%p,0x%x) rc %d %s%s\n", __FUNCTION__, dbi, dbcursor, key, pkey, data, flags, rc, _DBCFLAGS(flags), _KEYDATA(key, data, NULL)));
    return rc;
}
/*@=mustmod@*/

/*@-mustmod@*/
static int db3cdel(dbiIndex dbi, DBC * dbcursor, DBT * key, DBT * data,
		unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *dbcursor, fileSystem @*/
{
    DB * db = dbi->dbi_db;
    DB_TXN * _txnid = dbiTxnid(dbi);
    int rc;

assert(db != NULL);
    if (dbcursor == NULL) {
	rc = db->del(db, _txnid, key, flags);
	rc = cvtdberr(dbi, "db->del", rc, _debug);
    } else {

	/* XXX TODO: insure that cursor is positioned with duplicates */
	rc = db3cget(dbi, dbcursor, key, data, DB_SET);

	if (rc == 0) {
#if (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 6)
	    rc = dbcursor->del(dbcursor, flags);
	    rc = cvtdberr(dbi, "dbcursor->del", rc, _debug);
#else
	    rc = dbcursor->c_del(dbcursor, flags);
	    rc = cvtdberr(dbi, "dbcursor->c_del", rc, _debug);
#endif
	}
    }

DBIDEBUG(dbi, (stderr, "<-- %s(%p,%p,%p,%p,0x%x) rc %d %s%s\n", __FUNCTION__, dbi, dbcursor, key, data, flags, rc, _DBCFLAGS(flags), _KEYDATA(key, data, NULL)));
    return rc;
}
/*@=mustmod@*/

static int db3ccount(dbiIndex dbi, DBC * dbcursor,
		/*@null@*/ /*@out@*/ unsigned int * countp,
		/*@unused@*/ unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *countp, fileSystem @*/
{
    db_recno_t count = 0;
    int rc = 0;

    flags = 0;
#if (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 6)
    rc = dbcursor->count(dbcursor, &count, flags);
    rc = cvtdberr(dbi, "dbcursor->count", rc, _debug);
#else
    rc = dbcursor->c_count(dbcursor, &count, flags);
    rc = cvtdberr(dbi, "dbcursor->c_count", rc, _debug);
#endif
    if (countp) *countp = (!rc ? count : 0);

DBIDEBUG(dbi, (stderr, "<-- %s(%p,%p,%p,0x%x) count %d\n", __FUNCTION__, dbi, dbcursor, countp, flags, count));

    return rc;
}

static int db3byteswapped(dbiIndex dbi)	/*@*/
{
    DB * db = dbi->dbi_db;
    int rc = 0;

    if (db != NULL) {
#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 3 && DB_VERSION_PATCH >= 11) \
 || (DB_VERSION_MAJOR == 4)
	int isswapped = 0;
	rc = db->get_byteswapped(db, &isswapped);
	if (rc == 0)
	    rc = isswapped;
#else
	rc = db->get_byteswapped(db);
#endif
    }

    return rc;
}

static int db3stat(dbiIndex dbi, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, fileSystem @*/
{
    DB * db = dbi->dbi_db;
#if (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3)
    DB_TXN * _txnid = dbiTxnid(dbi);
#endif
    int rc = 0;

    assert(db != NULL);
#if defined(DB_FAST_STAT)
    if (flags)
	flags = DB_FAST_STAT;
    else
#endif
	flags = 0;
    dbi->dbi_stats = _free(dbi->dbi_stats);
/* XXX 3.3.4 change. */
#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 3) || (DB_VERSION_MAJOR == 4)
#if (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3)
    rc = db->stat(db, _txnid, &dbi->dbi_stats, flags);
#else
    rc = db->stat(db, &dbi->dbi_stats, flags);
#endif
#else
    rc = db->stat(db, &dbi->dbi_stats, NULL, flags);
#endif
    rc = cvtdberr(dbi, "db->stat", rc, _debug);

DBIDEBUG(dbi, (stderr, "<-- %s(%p,0x%x) rc %d\n", __FUNCTION__, dbi, flags, rc));

    return rc;
}

/*@-mustmod@*/
static int db3associate(dbiIndex dbi, dbiIndex dbisecondary,
		int (*callback)(DB *, const DBT *, const DBT *, DBT *),
		unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, fileSystem @*/
{
    DB * db = dbi->dbi_db;
    DB * secondary = dbisecondary->dbi_db;
    DB_TXN * _txnid = dbiTxnid(dbi);
    int rc;

assert(db != NULL);

/*@-moduncon@*/ /* FIX: annotate db3 methods */
#if (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 1)
    rc = db->associate(db, _txnid, secondary, callback, flags);
#else
    rc = db->associate(db, secondary, callback, flags);
#endif
/*@=moduncon@*/
    rc = cvtdberr(dbi, "db->associate", rc, _debug);

    if (dbi->dbi_debug || dbisecondary->dbi_debug) {
    	const char * tag2 = xstrdup(tagName(dbisecondary->dbi_rpmtag));
fprintf(stderr, "<-- %s(%p(%s),%p(%s),%p,0x%x) rc %d %s\n", __FUNCTION__, dbi, tagName(dbi->dbi_rpmtag), dbisecondary, tag2, callback, flags, rc, _AFLAGS(flags));
	tag2 = _free(tag2);
    }

    return rc;
}
/*@=mustmod@*/

/*@-mustmod@*/
static int db3associate_foreign(dbiIndex dbi, dbiIndex dbisecondary,
		int (*callback)(DB *, const DBT *, DBT *, const DBT *, int *),
		unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, fileSystem @*/
{
    DB * db = dbi->dbi_db;
    DB * secondary = dbisecondary->dbi_db;
    int rc;

/*@-moduncon@*/ /* FIX: annotate db3 methods */
#if (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 8)
assert(db != NULL);
    rc = db->associate_foreign(db, secondary, callback, flags);
#else
    rc = EINVAL;
#endif
/*@=moduncon@*/
    rc = cvtdberr(dbi, "db->associate_foreign", rc, _debug);

    if (dbi->dbi_debug || dbisecondary->dbi_debug) {
    	const char * tag2 = xstrdup(tagName(dbisecondary->dbi_rpmtag));
fprintf(stderr, "<-- %s(%p(%s),%p(%s),%p,0x%x) rc %d %s\n", __FUNCTION__, dbi, tagName(dbi->dbi_rpmtag), dbisecondary, tag2, callback, flags, rc, _AFFLAGS(flags));
	tag2 = _free(tag2);
    }

    return rc;
}
/*@=mustmod@*/

/*@-mustmod@*/
static int db3join(dbiIndex dbi, DBC ** curslist, DBC ** dbcp,
		unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, fileSystem @*/
{
    DB * db = dbi->dbi_db;
    int rc;

DBIDEBUG(dbi, (stderr, "--> %s(%p,%p,%p,0x%x)\n", __FUNCTION__, dbi, curslist, dbcp, flags));
assert(db != NULL);
/*@-moduncon@*/ /* FIX: annotate db3 methods */
    rc = db->join(db, curslist, dbcp, flags);
/*@=moduncon@*/
    rc = cvtdberr(dbi, "db->join", rc, _debug);
    return rc;
}
/*@=mustmod@*/

/*@-moduncon@*/ /* FIX: annotate db3 methods */
static int db3close(/*@only@*/ dbiIndex dbi, /*@unused@*/ unsigned int flags)
	/*@globals rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies dbi, fileSystem, internalState @*/
{
    rpmdb rpmdb = dbi->dbi_rpmdb;
    const char * urlfn = NULL;
    const char * root;
    const char * home;
    const char * dbhome;
    const char * dbfile;
    const char * dbsubfile;
    DB * db = dbi->dbi_db;
    DB_SEQUENCE * seq = dbi->dbi_seq;
    const char * dbiBN = mapTagName(rpmdb, dbi);
    int _printit;
    int rc = 0, xx;

    flags = 0;	/* XXX unused */

    /*
     * Get the prefix/root component and directory path.
     */
    root = (dbi->dbi_root ? dbi->dbi_root : rpmdb->db_root);
    if ((root[0] == '/' && root[1] == '\0') || rpmdb->db_chrootDone)
	root = NULL;
    home = (dbi->dbi_home ? dbi->dbi_home : rpmdb->db_home);

    /*
     * Either the root or directory components may be a URL. Concatenate,
     * convert the URL to a path, and add the name of the file.
     */
    /*@-mods@*/
    urlfn = rpmGenPath(root, home, NULL);
    /*@=mods@*/
    (void) urlPath(urlfn, &dbhome);
    if (dbi->dbi_temporary) {
	dbfile = NULL;
	dbsubfile = NULL;
    } else {
#ifdef	HACK	/* XXX necessary to support dbsubfile */
	dbfile = (dbi->dbi_file ? dbi->dbi_file : db3basename);
	dbsubfile = (dbi->dbi_subfile ? dbi->dbi_subfile : dbiBN);
#else
	dbfile = (dbi->dbi_file ? dbi->dbi_file : dbiBN);
	dbsubfile = NULL;
#endif
    }

    if (seq) {
	rc = seq->close(seq, 0);  
	rc = cvtdberr(dbi, "seq->close", rc, _debug);
	seq = dbi->dbi_seq = NULL;

	rpmlog(RPMLOG_DEBUG, D_("closed   db sequence    %s/%s\n"),
		dbhome, (dbfile ? dbfile : dbiBN));

    }

    if (db) {
	rc = db->close(db, 0);
	/* XXX ignore not found error messages. */
	_printit = (rc == ENOENT ? 0 : _debug);
	rc = cvtdberr(dbi, "db->close", rc, _printit);
	db = dbi->dbi_db = NULL;

	rpmlog(RPMLOG_DEBUG, D_("closed   db index       %s/%s\n"),
		dbhome, (dbfile ? dbfile : dbiBN));

    }

    if (rpmdb->db_dbenv != NULL && dbi->dbi_use_dbenv) {
	if (rpmdb->db_opens == 1) {
	    /*@-nullstate@*/
	    xx = db_fini(dbi, (dbhome ? dbhome : ""), dbfile, dbsubfile);
	    /*@=nullstate@*/
	    rpmdb->db_dbenv = NULL;
	}
	rpmdb->db_opens--;
    }

    if (dbi->dbi_verify_on_close && !dbi->dbi_temporary) {
	DB_ENV * dbenv = NULL;
	int eflags;

	/*@-moduncon@*/ /* FIX: annotate db3 methods */
	rc = db_env_create(&dbenv, 0);
	/*@=moduncon@*/
	rc = cvtdberr(dbi, "db_env_create", rc, _debug);
	if (rc || dbenv == NULL) goto exit;

	/*@-noeffectuncon@*/ /* FIX: annotate db3 methods */
/*@-castfcnptr@*/
	dbenv->set_errcall(dbenv, (void *)rpmdb->db_errcall);
/*@=castfcnptr@*/
	dbenv->set_errfile(dbenv, rpmdb->db_errfile);
	dbenv->set_errpfx(dbenv, rpmdb->db_errpfx);
 /*	dbenv->set_paniccall(???) */
	/*@=noeffectuncon@*/

	if (dbi->dbi_tmpdir) {
	    /*@-mods@*/
	    const char * tmpdir = rpmGenPath(root, dbi->dbi_tmpdir, NULL);
	    /*@=mods@*/
	    rc = dbenv->set_tmp_dir(dbenv, tmpdir);
	    rc = cvtdberr(dbi, "dbenv->set_tmp_dir", rc, _debug);
	    tmpdir = _free(tmpdir);
	    if (rc) goto exit;
	}
	    
	eflags = DB_CREATE | DB_INIT_MPOOL | DB_PRIVATE | DB_USE_ENVIRON;
	rc = (dbenv->open) (dbenv, dbhome, eflags, 0);
	rc = cvtdberr(dbi, "dbenv->open", rc, _debug);
	if (rc) goto exit;

	/*@-moduncon -nullstate@*/ /* FIX: annotate db3 methods */
	rc = db_create(&db, dbenv, 0);
	/*@=moduncon =nullstate@*/
	rc = cvtdberr(dbi, "db_create", rc, _debug);

	if (db != NULL) {
		/*@-mods@*/
		const char * dbf = rpmGetPath(dbhome, "/", dbfile, NULL);
		const char * _dbsubfile = NULL;
		FILE * _fp = NULL;
		/*@=mods@*/

		rc = db3verify(dbi, dbf, _dbsubfile, _fp, flags);

		rpmlog(RPMLOG_DEBUG, D_("verified db index       %s/%s\n"),
			(dbhome ? dbhome : ""),
			(dbfile ? dbfile : dbiBN));

		dbf = _free(dbf);
	}
	xx = dbenv->close(dbenv, 0);
	xx = cvtdberr(dbi, "dbenv->close", xx, _debug);
	if (rc == 0 && xx) rc = xx;
    }

exit:

DBIDEBUG(dbi, (stderr, "<-- %s(%p,0x%x) rc %d\n", __FUNCTION__, dbi, flags, rc));

    dbi->dbi_db = NULL;

    urlfn = _free(urlfn);

    dbi = db3Free(dbi);

    return rc;
}
/*@=moduncon@*/

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

static int loadDBT(DBT * _r, rpmTag tag, const void * _s, size_t ns)
{
    const char * s = _s;
    void * data = NULL;
    size_t size = 0;
    uint8_t * t = NULL;
    uint32_t i;
    int xx;

    if (ns == 0) ns = strlen(s);
    switch (tag) {
    case RPMTAG_FILEDIGESTS:
	/* Convert hex to binary, filter out odd hex strings. */
	if (ns > 0 && !(ns & 1)) {
	    ns /= 2;
	    data = t = xmalloc(ns);
	    for (i = 0; i < ns; i++, t++, s += 2) {
		if (!(isxdigit(s[0]) && isxdigit(s[1])))
		    break;
		*t = (uint8_t) (nibble(s[0]) << 4) | nibble(s[1]);
	    }
	    if (i == ns)
		size = ns;
	    else
		data = _free(data);
	}
	break;
    case RPMTAG_PUBKEYS:
	/* Extract pubkey id from the base64 blob. */
	t = xmalloc(32);
	if ((xx = pgpExtractPubkeyFingerprint(s, t)) > 0) {
	    data = t;
	    size = xx;
	} else
	    t = _free(t);
	break;
    default:
	data = (void *) memcpy(xmalloc(ns), _s, ns);
	size = ns;
	break;
    }
    if ((_r->data = data) != NULL) _r->flags |= DB_DBT_APPMALLOC;
    return (_r->size = size);
}

static int uint32Cmp(const void * _a, const void * _b)
{
    const uint32_t * a = _a;
    const uint32_t * b = _b;
    return ((*a < *b) ? -1 :
	   ((*a > *b) ?  1 : 0));
}

static int uint64Cmp(const void * _a, const void * _b)
{
    const uint64_t * a = _a;
    const uint64_t * b = _b;
    return ((*a < *b) ? -1 :
	   ((*a > *b) ?  1 : 0));
}

static int
db3Acallback(DB * db, const DBT * key, const DBT * data, DBT * _r)
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    HE_t Fhe = memset(alloca(sizeof(*Fhe)), 0, sizeof(*Fhe));
#ifdef	NOTYET
    HE_t FMhe = memset(alloca(sizeof(*FMhe)), 0, sizeof(*FMhe));
#endif
    dbiIndex dbi = db->app_private;
    rpmdb rpmdb = NULL;
    Header h = NULL;
    DBT * A = NULL;
    const char * s = NULL;
    size_t ns = 0;
    int rc = DB_DONOTINDEX;	/* assume no-op */
    uint32_t i;

    /* XXX Don't index the header instance counter at record 0. */
    if (key->size == 4 && *(uint32_t *)key->data == 0)
	return rc;

assert(dbi);
    rpmdb = dbi->dbi_rpmdb;
assert(rpmdb);
    h = rpmdb->db_h;
assert(h);

    memset(_r, 0, sizeof(*_r));

    he->tag = dbi->dbi_rpmtag;
    if (!headerGet(h, he, 0))
	goto exit;

    /* Retrieve other tags needed for filtering decisions. */
    switch (he->tag) {
    default:
	break;
#ifdef	NOTYET
    case RPMTAG_BASENAMES:
    case RPMTAG_FILEPATHS:
	/* Add the pesky trailing '/' to directories. */
	FMhe->tag = RPMTAG_FILEMODES;
	(void) headerGet(h, FMhe, 0);
	break;
#endif
    case RPMTAG_REQUIREYAMLENTRY:
    case RPMTAG_REQUIRENAME:
	/* The Requires: F is needed to filter install context dependencies. */
	Fhe->tag = RPMTAG_REQUIREFLAGS;
	(void) headerGet(h, Fhe, 0);
	break;
    }

    switch (he->t) {
    default:
assert(0);
	break;
    case RPM_UINT8_TYPE:	/* XXX coerce to uint32_t */
    {	uint8_t * _u = he->p.ui8p;
	he->p.ui32p = xmalloc(he->c * sizeof(*he->p.ui32p));
	for (i = 0; i < he->c; i++)
	    he->p.ui32p[i] = _u[i];
	_u = _free(_u);
	goto _ifill;
    }	break;
    case RPM_UINT16_TYPE:	/* XXX coerce to uint32_t */
    {	uint16_t * _u = he->p.ui16p;
	he->p.ui32p = xmalloc(he->c * sizeof(*he->p.ui32p));
	for (i = 0; i < he->c; i++)
	    he->p.ui32p[i] = _u[i];
	_u = _free(_u);
	goto _ifill;
    }	break;
    case RPM_UINT32_TYPE:
_ifill:
    {	uint32_t * _u = he->p.ui32p;
	size_t _ulen = sizeof(*_u);

	/* Drop the transaction id usecs field (if present) when indexing. */
	switch (he->tag) {
	case RPMTAG_INSTALLTID:
	case RPMTAG_REMOVETID:
	    he->c = 1;
	    break;
	default:
	    break;
	}
	if (he->c == 1) {
	    /* XXX is it worth avoiding the realloc here? */
	    (void) loadDBT(_r, he->tag, _u, _ulen);
	    break;
	}
	_r->flags = DB_DBT_MULTIPLE | DB_DBT_APPMALLOC;
	_r->data = A = xcalloc(he->c, sizeof(*A));
	_r->size = 0;
	if (he->c > 1)
	    qsort(_u, he->c, _ulen, uint32Cmp);
	for (i = 0; i < he->c; i++, _u++) {
	    /* Don't add identical (key,val) item to secondary. */
	    if (i > 0 && _u[-1] == _u[0])
		continue;
	    if (!loadDBT(A, he->tag, _u, _ulen))
		continue;
	    A++;
	    _r->size++;
	}
    }	break;
    case RPM_UINT64_TYPE:
    {   uint64_t * _u = he->p.ui64p;
	size_t _ulen = sizeof(*_u);
	if (he->c == 1) {
	    /* XXX is it worth avoiding the realloc here? */
	    (void) loadDBT(_r, he->tag, _u, _ulen);
	    break;
	}
	_r->flags = DB_DBT_MULTIPLE | DB_DBT_APPMALLOC;
	_r->data = A = xcalloc(he->c, sizeof(*A));
	_r->size = 0;
	if (he->c > 1)
	    qsort(_u, he->c, _ulen, uint64Cmp);
	for (i = 0; i < he->c; i++, _u++) {
	    /* Don't add identical (key,val) item to secondary. */
	    if (i > 0 && _u[-1] == _u[0])
		continue;
	    if (!loadDBT(A, he->tag, _u, _ulen))
		continue;
	    A++;
	    _r->size++;
	}
    }	break;
    case RPM_BIN_TYPE:
	s = he->p.ptr; ns = he->c;
	/* XXX is it worth avoiding the realloc here? */
	if (ns > 0)			/* No "" empty keys please. */
	    (void) loadDBT(_r, he->tag, s, ns);
	break;
    case RPM_I18NSTRING_TYPE:       /* XXX never occurs */
    case RPM_STRING_TYPE:
	s = he->p.str; ns = strlen(s);
	/* XXX is it worth avoiding the realloc here? */
	if (ns > 0)			/* No "" empty keys please. */
	    (void) loadDBT(_r, he->tag, s, ns);
	break;
    case RPM_STRING_ARRAY_TYPE:
	if (he->c == 1) {
	    s = he->p.argv[0]; ns = strlen(s);
	    if (ns > 0)			/* No "" empty keys please. */
		(void) loadDBT(_r, he->tag, s, ns);
	} else {
	    size_t _jiggery = 2;	/* XXX todo: Bloom filter tuning? */
	    size_t _k = _jiggery * 8;
	    size_t _m = _jiggery * (3 * he->c * _k) / 2;
	    rpmbf bf = rpmbfNew(_m, _k, 0);

	    _r->flags = DB_DBT_MULTIPLE | DB_DBT_APPMALLOC;
	    _r->data = A = xcalloc(he->c, sizeof(*A));
	    _r->size = 0;
	    for (i = 0; i < he->c; i++) {
		s = he->p.argv[i]; ns = strlen(s);

		/* XXX Skip YAML "- ..." lead-in mark up if present. */
		if (s[0] == '-' && s[1] == ' ') {
		    s += 2, ns -= 2;
		}

#ifdef	NOTYET
		/* Add the pesky trailing '/' to directories. */
		if (FMhe->p.ui16p && !S_ISREG((mode_t)FMhe->p.ui16p[i])) {
		    continue;
		}
#endif

		if (ns == 0)		/* No "" empty keys please. */
		    continue;

		/* Filter install context dependencies. */
		if (Fhe->p.ui32p && isInstallPreReq(Fhe->p.ui32p[i]))
		    continue;

		/* Don't add identical (key,val) item to secondary. */
		if (rpmbfChk(bf, s, ns))
		    continue;
		rpmbfAdd(bf, s, ns);

		if (!loadDBT(A, he->tag, s, ns))
		    continue;
		A++;
		_r->size++;
	    }
	    bf = rpmbfFree(bf);
	}
	break;
    }
    if (_r->data && _r->size > 0)
	rc = 0;
    else if (_r->flags & DB_DBT_APPMALLOC) {
	_r->data = _free(_r->data);
	memset(_r, 0, sizeof(*_r));
    }

exit:
#ifdef	NOTYET
    FMhe->p.ptr = _free(FMhe->p.ptr);
#endif
    Fhe->p.ptr = _free(Fhe->p.ptr);
    he->p.ptr = _free(he->p.ptr);

DBIDEBUG(dbi, (stderr, "<-- %s(%p, %p, %p, %p) rc %d\n\tdbi %p(%s) rpmdb %p h %p %s\n", __FUNCTION__, db, key, data, _r, rc, dbi, tagName(dbi->dbi_rpmtag), rpmdb, h, _KEYDATA(key, data, _r)));

    return rc;
}

static int seqid_init(dbiIndex dbi, const char * keyp, size_t keylen,
		DB_SEQUENCE ** seqp)
{
    DB * db = dbi->dbi_db;
    DBT k = {0};
    DB_TXN * _txnid = dbiTxnid(dbi);
    DB_SEQUENCE * seq = NULL;
    db_seq_t _rangemin = -922337203685477600LL;
    db_seq_t _rangemax =  922337203685477600LL;
    db_seq_t _value = 0;
    int32_t _cachesize = 0;
    uint32_t _flags = DB_SEQ_INC;
    uint32_t _oflags = DB_CREATE;
    int rc;

assert(db != NULL);
    if (seqp) *seqp = NULL;

    rc = db_sequence_create(&seq, db, 0);
    rc = cvtdberr(dbi, "db_sequence_create", rc, _debug);
    if (rc) goto exit;

    if (dbi->dbi_seq_cachesize) {
	_cachesize = dbi->dbi_seq_cachesize;
	rc = seq->set_cachesize(seq, _cachesize);
	rc = cvtdberr(dbi, "seq->set_cachesize", rc, _debug);
	if (rc) goto exit;
    }

    if (dbi->dbi_seq_initial)
	_value = dbi->dbi_seq_initial;
    rc = seq->initial_value(seq, _value);
    rc = cvtdberr(dbi, "seq->initial_value", rc, _debug);
    if (rc) goto exit;

    if (dbi->dbi_seq_min)
	_rangemin = dbi->dbi_seq_min;
    if (dbi->dbi_seq_max)
	_rangemax = dbi->dbi_seq_max;
    rc = seq->set_range(seq, _rangemin, _rangemax);
    rc = cvtdberr(dbi, "seq->set_range", rc, _debug);
    if (rc) goto exit;

    if (dbi->dbi_seq_flags)
	_flags = dbi->dbi_seq_flags;
    rc = seq->set_flags(seq, _flags);
    rc = cvtdberr(dbi, "seq->set_flags", rc, _debug);
    if (rc) goto exit;

    k.data = (void *)keyp;
    k.size = (u_int32_t) (keylen > 0 ? keylen : strlen(keyp));
    rc = seq->open(seq, _txnid, &k, _oflags);
    rc = cvtdberr(dbi, "seq->open", rc, _debug);
    if (rc) goto exit;

exit:
    if (rc == 0 && seqp != NULL)
	*seqp = seq;
    else {
	int xx = seq->close(seq, 0);  
	xx = cvtdberr(dbi, "seq->close", xx, _debug);
    }

DBIDEBUG(dbi, (stderr, "<-- %s(%p,%p[%u],%p) seq %p rc %d %s\n", __FUNCTION__, dbi, keyp, keylen, seqp, (seqp ? *seqp : NULL), rc, _KEYDATA(&k, NULL, NULL)));

    return rc;
}

/**
 * Return handle for an index database.
 * @param rpmdb         rpm database
 * @param rpmtag        rpm tag
 * @retval *dbip	index database handle
 * @return              0 on success
 */
static int db3open(rpmdb rpmdb, rpmTag rpmtag, dbiIndex * dbip)
	/*@globals rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies *dbip, fileSystem, internalState @*/
{
    /*@-nestedextern -shadow@*/
    extern struct _dbiVec db3vec;
    /*@=nestedextern =shadow@*/
    const char * urlfn = NULL;
    const char * root;
    const char * home;
    const char * dbhome;
    const char * dbfile;
    const char * dbsubfile;
    const char * dbiBN;
    dbiIndex dbi = NULL;
    int rc = 0;
    int xx;

    DB * db = NULL;
    DB_ENV * dbenv = NULL;
#if (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 1)
    DB_TXN * _txnid = NULL;
#endif
    DBTYPE dbi_type = DB_UNKNOWN;
    rpmuint32_t oflags;
    int _printit;

    if (dbip)
	*dbip = NULL;

    /*
     * Parse db configuration parameters.
     */
    /*@-mods@*/
    if ((dbi = db3New(rpmdb, rpmtag)) == NULL)
	/*@-nullstate@*/
	return 1;
	/*@=nullstate@*/
    /*@=mods@*/
    dbi->dbi_api = DB_VERSION_MAJOR;
    dbiBN = mapTagName(rpmdb, dbi);
    dbi->dbi_txnid = NULL;
    _txnid = NULL;

    /*
     * Get the prefix/root component and directory path.
     */
    root = (dbi->dbi_root ? dbi->dbi_root : rpmdb->db_root);
    if ((root[0] == '/' && root[1] == '\0') || rpmdb->db_chrootDone)
	root = NULL;
    home = (dbi->dbi_home ? dbi->dbi_home : rpmdb->db_home);

    /*
     * Either the root or directory components may be a URL. Concatenate,
     * convert the URL to a path, and add the name of the file.
     */
    /*@-mods@*/
    urlfn = rpmGenPath(root, home, NULL);
    /*@=mods@*/
    (void) urlPath(urlfn, &dbhome);
    if (dbi->dbi_temporary) {
	dbfile = NULL;
	dbsubfile = NULL;
    } else {
#ifdef	HACK	/* XXX necessary to support dbsubfile */
	dbfile = (dbi->dbi_file ? dbi->dbi_file : db3basename);
	dbsubfile = (dbi->dbi_subfile ? dbi->dbi_subfile : dbiBN);
#else
	dbfile = (dbi->dbi_file ? dbi->dbi_file : dbiBN);
	dbsubfile = NULL;
#endif
    }

    oflags = (dbi->dbi_oeflags | dbi->dbi_oflags);
    oflags &= ~DB_TRUNCATE;	/* XXX this is dangerous */

#if 0	/* XXX rpmdb: illegal flag combination specified to DB->open */
    if ( dbi->dbi_mode & O_EXCL) oflags |= DB_EXCL;
#endif

    /*
     * Map open mode flags onto configured database/environment flags.
     */
    if (dbi->dbi_temporary) {
	oflags |= DB_CREATE;
	dbi->dbi_oeflags |= DB_CREATE;
	oflags &= ~DB_RDONLY;
	dbi->dbi_oflags &= ~DB_RDONLY;
    } else {
	if (!(dbi->dbi_mode & (O_RDWR|O_WRONLY))) oflags |= DB_RDONLY;
	if (dbi->dbi_mode & O_CREAT) {
	    oflags |= DB_CREATE;
	    dbi->dbi_oeflags |= DB_CREATE;
	}
#ifdef	DANGEROUS
	if ( dbi->dbi_mode & O_TRUNC) oflags |= DB_TRUNCATE;
#endif
    }

    /*
     * Create the /var/lib/rpm directory if it doesn't exist (root only).
     */
    (void) rpmioMkpath(dbhome, 0755, getuid(), getgid());

    /*
     * Avoid incompatible DB_CREATE/DB_RDONLY flags on DBENV->open.
     */
    if (dbi->dbi_use_dbenv) {

	if (access(dbhome, W_OK) == -1) {

	    /* dbhome is unwritable, don't attempt DB_CREATE on DB->open ... */
	    oflags &= ~DB_CREATE;
	    oflags &= ~DB_AUTO_COMMIT;

	    /* ... but DBENV->open might still need DB_CREATE ... */
	    if (dbi->dbi_eflags & DB_PRIVATE) {
		dbi->dbi_eflags &= ~DB_JOINENV;
	    } else {
		dbi->dbi_eflags |= DB_JOINENV;
		dbi->dbi_oeflags &= ~DB_CREATE;
#ifdef	DYING
		dbi->dbi_oeflags &= ~DB_THREAD;
#endif
		/* ... but, unless DB_PRIVATE is used, skip DBENV. */
		dbi->dbi_use_dbenv = 0;
	    }

	    /* ... DB_RDONLY maps dbhome perms across files ...  */
	    if (dbi->dbi_temporary) {
		oflags |= DB_CREATE;
		dbi->dbi_oeflags |= DB_CREATE;
		oflags &= ~DB_RDONLY;
		dbi->dbi_oflags &= ~DB_RDONLY;
	    } else {
		oflags |= DB_RDONLY;
		/* ... and DB_WRITECURSOR won't be needed ...  */
		dbi->dbi_oflags |= DB_RDONLY;
	    }

	} else {	/* dbhome is writable, check for persistent dbenv. */
	    /*@-mods@*/
	    const char * dbf = rpmGetPath(dbhome, "/__db.001", NULL);
	    /*@=mods@*/

#if defined(RPM_VENDOR_OPENPKG) /* bdb-allow-zero-sized-files */
	    /* Make sure RPM passes DB_CREATE to Berkeley-DB also
	       if file exists, but is (still) zero-sized. */
	    struct stat sb;
	    long size = -1;
	    if (stat(dbf, &sb) == 0)
		size = (long)sb.st_size;
	    if (access(dbf, F_OK) == -1 || size == 0)
#else
	    if (access(dbf, F_OK) == -1)
#endif
	    {
		/* ... non-existent (or unwritable) DBENV, will create ... */
		dbi->dbi_oeflags |= DB_CREATE;
		dbi->dbi_eflags &= ~DB_JOINENV;
	    } else {
		/* ... pre-existent (or bogus) DBENV, will join ... */
		if (dbi->dbi_eflags & DB_PRIVATE) {
		    dbi->dbi_eflags &= ~DB_JOINENV;
		} else {
		    dbi->dbi_eflags |= DB_JOINENV;
		    dbi->dbi_oeflags &= ~DB_CREATE;
#ifdef	DYING
		    dbi->dbi_oeflags &= ~DB_THREAD;
#endif
		}
	    }
	    dbf = _free(dbf);
	}
    }

    /*
     * Avoid incompatible DB_CREATE/DB_RDONLY flags on DB->open.
     */
    if ((oflags & DB_CREATE) && (oflags & DB_RDONLY)) {
	/* dbhome is writable, and DB->open flags may conflict. */
	const char * dbfn = (dbfile ? dbfile : dbiBN);
	/*@-mods@*/
	const char * dbf = rpmGetPath(dbhome, "/", dbfn, NULL);
	/*@=mods@*/

	if (access(dbf, F_OK) == -1) {
	    /* File does not exist, DB->open might create ... */
	    oflags &= ~DB_RDONLY;
	} else {
	    /* File exists, DB->open need not create ... */
	    oflags &= ~DB_CREATE;
	}

	/* Only writers need DB_WRITECURSOR ... */
	if (!(oflags & DB_RDONLY) && access(dbf, W_OK) == 0) {
	    dbi->dbi_oflags &= ~DB_RDONLY;
	} else {
	    dbi->dbi_oflags |= DB_RDONLY;
	}
	dbf = _free(dbf);
    }

    /*
     * Set db type if creating.
     */
    if (oflags & DB_CREATE)
	dbi_type = dbi->dbi_type;

    /*
     * Turn off verify-on-close if opening read-only.
     */
    if (oflags & DB_RDONLY)
	dbi->dbi_verify_on_close = 0;

    if (dbi->dbi_use_dbenv) {
	/*@-mods@*/
	if (rpmdb->db_dbenv == NULL) {
	    static int runrecoverycount = 0;
	    rc = db_init(dbi, dbhome, dbfile, dbsubfile, &dbenv);
	    switch (rc) {
	    default:
		break;

	    case DB_RUNRECOVERY:
		if (runrecoverycount++ >= 1) {
		    rpmlog(RPMLOG_ERR, _("RUNRECOVERY failed, exiting ...\n"));
		   exit(EXIT_FAILURE);
		}
		rpmlog(RPMLOG_ERR, _("Runnning db->verify ...\n"));
		rpmdb = rpmdbLink(rpmdb, "DB_RUNRECOVERY");
assert(rpmdb != NULL);
		rpmdb->db_remove_env = 1;
		rpmdb->db_verifying = 1;
		xx = rpmdbVerifyAllDBI(rpmdb);
		xx = cvtdberr(dbi, "db->verify", xx, _debug);
		rpmdb->db_remove_env = 0;
		rpmdb->db_verifying = 0;

		dbi->dbi_oeflags |= DB_CREATE;
		dbi->dbi_eflags &= ~DB_JOINENV;
		rc = db_init(dbi, dbhome, dbfile, dbsubfile, &dbenv);
		/* XXX db_init EINVAL was masked. */
		rc = cvtdberr(dbi, "dbenv->open", rc, _debug);
		if (rc)
		    break;

assert(dbenv);
		rpmdb->db_dbenv = dbenv;
		rpmdb->db_opens = 1;
		break;

#if defined(DB_VERSION_MISMATCH) /* Nuke __db* files and retry open once. */
	    case DB_VERSION_MISMATCH:
#endif
	    case EINVAL:
		if (getuid() != 0)
		    break;
		{   char * filename = alloca(BUFSIZ);
		    struct stat st;
		    int i;

		    for (i = 0; i < 16; i++) {
			sprintf(filename, "%s/__db.%03d", dbhome, i);
			(void)rpmCleanPath(filename);
			if (Stat(filename, &st)
			  && (errno == ENOENT || errno == EINVAL))
			    continue;
			xx = Unlink(filename);
		    }
		}
		dbi->dbi_oeflags |= DB_CREATE;
		dbi->dbi_eflags &= ~DB_JOINENV;
		rc = db_init(dbi, dbhome, dbfile, dbsubfile, &dbenv);
		/* XXX db_init EINVAL was masked. */
		rc = cvtdberr(dbi, "dbenv->open", rc, _debug);
		if (rc)
		    break;
		/*@fallthrough@*/
	    case 0:
assert(dbenv);
		rpmdb->db_dbenv = dbenv;
		rpmdb->db_opens = 1;
		break;
	    }
	} else {
assert(rpmdb && rpmdb->db_dbenv);
	    dbenv = rpmdb->db_dbenv;
	    rpmdb->db_opens++;
	}
	/*@=mods@*/
    }

    rpmlog(RPMLOG_DEBUG, D_("opening  db index       %s/%s %s mode=0x%x\n"),
		dbhome, (dbfile ? dbfile : dbiBN),
		prDbiOpenFlags(oflags, 0), dbi->dbi_mode);

    if (rc == 0) {
	static int _lockdbfd = 0;

	/*@-moduncon@*/ /* FIX: annotate db3 methods */
	rc = db_create(&db, dbenv, dbi->dbi_cflags);
	/*@=moduncon@*/
	rc = cvtdberr(dbi, "db_create", rc, _debug);
	if (rc == 0 && db != NULL) {

/* XXX 3.3.4 change. */
#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 3) || (DB_VERSION_MAJOR == 4)
	    if (rc == 0 &&
			rpmdb->db_malloc && rpmdb->db_realloc && rpmdb->db_free)
	    {
		rc = db->set_alloc(db,
			rpmdb->db_malloc, rpmdb->db_realloc, rpmdb->db_free);
		rc = cvtdberr(dbi, "db->set_alloc", rc, _debug);
	    }
#else
	    if (rc == 0 && rpmdb->db_malloc) {
		rc = db->set_malloc(db, rpmdb->db_malloc);
		rc = cvtdberr(dbi, "db->set_malloc", rc, _debug);
	    }
#endif

/* 4.1: db->set_cache_priority(???) */
/* 4.1: db->set_encrypt(???) */
/* 4.1: db->set_errcall(dbenv, rpmdb->db_errcall); */
/* 4.1: db->set_errfile(dbenv, rpmdb->db_errfile); */
/* 4.1: db->set_errpfx(dbenv, rpmdb->db_errpfx); */
/* 4.1: db->set_feedback(???) */

	    if (rc == 0 && dbi->dbi_lorder) {
		rc = db->set_lorder(db, dbi->dbi_lorder);
		rc = cvtdberr(dbi, "db->set_lorder", rc, _debug);
	    }
	    if (rc == 0 && dbi->dbi_pagesize) {
		rc = db->set_pagesize(db, dbi->dbi_pagesize);
		rc = cvtdberr(dbi, "db->set_pagesize", rc, _debug);
	    }
 /* 4.1: db->set_paniccall(???) */
	    if (rc == 0 && oflags & DB_CREATE) {
		switch(dbi->dbi_type) {
		default:
		case DB_HASH:
		    if (dbi->dbi_h_ffactor) {
			rc = db->set_h_ffactor(db, dbi->dbi_h_ffactor);
			rc = cvtdberr(dbi, "db->set_h_ffactor", rc, _debug);
			if (rc) break;
		    }
		    if (dbi->dbi_h_nelem) {
			rc = db->set_h_nelem(db, dbi->dbi_h_nelem);
			rc = cvtdberr(dbi, "db->set_h_nelem", rc, _debug);
			if (rc) break;
		    }
		    if (dbi->dbi_h_flags) {
			rc = db->set_flags(db, dbi->dbi_h_flags);
			rc = cvtdberr(dbi, "db->set_h_flags", rc, _debug);
			if (rc) break;
		    }
/* XXX db-3.2.9 has added a DB arg to the call. */
#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR > 2) || (DB_VERSION_MAJOR == 4)
		    if (dbi->dbi_h_hash_fcn) {
			rc = db->set_h_hash(db, dbi->dbi_h_hash_fcn);
			rc = cvtdberr(dbi, "db->set_h_hash", rc, _debug);
			if (rc) break;
		    }
		    if (dbi->dbi_h_dup_compare_fcn) {
			rc = db->set_dup_compare(db, dbi->dbi_h_dup_compare_fcn);
			rc = cvtdberr(dbi, "db->set_dup_compare", rc, _debug);
			if (rc) break;
		    }
#endif
		    break;
		case DB_BTREE:
/* 4.1: db->set_append_recno(???) */
		    if (dbi->dbi_bt_flags) {
			rc = db->set_flags(db, dbi->dbi_bt_flags);
			rc = cvtdberr(dbi, "db->set_bt_flags", rc, _debug);
			if (rc) break;
		    }
		    if (dbi->dbi_bt_minkey) {
			rc = db->set_bt_minkey(db, dbi->dbi_bt_minkey);
			rc = cvtdberr(dbi, "db->set_bt_minkey", rc, _debug);
			if (rc) break;
		    }
/* XXX db-3.2.9 has added a DB arg to the call. */
#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR > 2) || (DB_VERSION_MAJOR == 4)
		    if (dbi->dbi_bt_compare_fcn) {
			rc = db->set_bt_compare(db, dbi->dbi_bt_compare_fcn);
			rc = cvtdberr(dbi, "db->set_bt_compare", rc, _debug);
			if (rc) break;
		    }
		    if (dbi->dbi_bt_dup_compare_fcn) {
			rc = db->set_dup_compare(db, dbi->dbi_bt_dup_compare_fcn);
			rc = cvtdberr(dbi, "db->set_dup_compare", rc, _debug);
			if (rc) break;
		    }
		    if (dbi->dbi_bt_prefix_fcn) {
			rc = db->set_bt_prefix(db, dbi->dbi_bt_prefix_fcn);
			rc = cvtdberr(dbi, "db->set_bt_prefix", rc, _debug);
			if (rc) break;
		    }
#endif
		    break;
		case DB_RECNO:
/* 4.1: db->set_append_recno(???) */
		    if (dbi->dbi_re_delim) {
			rc = db->set_re_delim(db, dbi->dbi_re_delim);
			rc = cvtdberr(dbi, "db->set_re_selim", rc, _debug);
			if (rc) break;
		    }
		    if (dbi->dbi_re_len) {
			rc = db->set_re_len(db, dbi->dbi_re_len);
			rc = cvtdberr(dbi, "db->set_re_len", rc, _debug);
			if (rc) break;
		    }
		    if (dbi->dbi_re_pad) {
			rc = db->set_re_pad(db, dbi->dbi_re_pad);
			rc = cvtdberr(dbi, "db->set_re_pad", rc, _debug);
			if (rc) break;
		    }
		    if (dbi->dbi_re_source) {
			rc = db->set_re_source(db, dbi->dbi_re_source);
			rc = cvtdberr(dbi, "db->set_re_source", rc, _debug);
			if (rc) break;
		    }
		    break;
		case DB_QUEUE:
		    if (dbi->dbi_q_extentsize) {
			rc = db->set_q_extentsize(db, dbi->dbi_q_extentsize);
			rc = cvtdberr(dbi, "db->set_q_extentsize", rc, _debug);
			if (rc) break;
		    }
		    break;
		}
	    }

	    if (rc == 0) {
		const char * dbfullpath;
		const char * dbpath;
		char * t;
		int nb;

		nb = strlen(dbhome);
		if (dbfile)	nb += 1 + strlen(dbfile);
		dbfullpath = t = alloca(nb + 1);

		t = stpcpy(t, dbhome);
		if (dbfile)
		    t = stpcpy( stpcpy( t, "/"), dbfile);
#ifdef	HACK	/* XXX necessary to support dbsubfile */
		dbpath = (!dbi->dbi_use_dbenv && !dbi->dbi_temporary)
			? dbfullpath : dbfile;
#else
#ifdef	PLD_CHROOT
		/* XXX Make dbpath relative. */
		dbpath = (!dbi->dbi_use_dbenv)
			? dbfullpath : dbfile;
#else
		dbpath = (!dbi->dbi_temporary)
			? dbfullpath : dbfile;
#endif	/* PLD_CHROOT */
#endif	/* HACK */

		if (rpmdb->_dbi[0]->dbi_eflags & DB_INIT_TXN)
		    oflags |= DB_AUTO_COMMIT;
#if (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 1)
		rc = (db->open)(db, _txnid, dbpath, dbsubfile,
		    dbi_type, oflags, dbi->dbi_perms);
#else
		rc = (db->open)(db, dbpath, dbsubfile,
		    dbi_type, oflags, dbi->dbi_perms);
#endif

		if (rc == 0 && dbi_type == DB_UNKNOWN) {
#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 3 && DB_VERSION_PATCH >= 11) \
 || (DB_VERSION_MAJOR == 4)
		    xx = db->get_type(db, &dbi_type);
		    if (xx == 0)
			dbi->dbi_type = dbi_type;
#else
		    dbi->dbi_type = db->get_type(db);
#endif
		}
	    }

	    /* XXX return rc == errno without printing */
	    _printit = (rc > 0 ? 0 : _debug);
	    xx = cvtdberr(dbi, "db->open", rc, _printit);

	    /*
	     * Lock a file using fcntl(2). Traditionally this is Packages,
	     * the file used to store metadata of installed header(s),
	     * as Packages is always opened, and should be opened first,
	     * for any rpmdb access.
	     *
	     * If no DBENV is used, then access is protected with a
	     * shared/exclusive locking scheme, as always.
	     *
	     * With a DBENV, the fcntl(2) lock is necessary only to keep
	     * the riff-raff from playing where they don't belong, as
	     * the DBENV should provide it's own locking scheme. So try to
	     * acquire a lock, but permit failures, as some other
	     * DBENV player may already have acquired the lock.
	     *
	     * With NPTL posix mutexes, revert to fcntl lock on non-functioning
	     * glibc/kernel combinations.
	     */
	    if (rc == 0 && dbi->dbi_lockdbfd &&
		!((dbi->dbi_ecflags & DB_CLIENT) && dbi->dbi_host) &&
		(!dbi->dbi_use_dbenv || _lockdbfd++ == 0))
	    {
		int fdno = -1;

		if (!(db->fd(db, &fdno) == 0 && fdno >= 0)) {
		    rc = 1;
		} else {
		    struct flock l;
		    memset(&l, 0, sizeof(l));
		    l.l_whence = 0;
		    l.l_start = 0;
		    l.l_len = 0;
		    l.l_type = (dbi->dbi_mode & (O_RDWR|O_WRONLY))
				? F_WRLCK : F_RDLCK;
		    l.l_pid = 0;

		    rc = fcntl(fdno, F_SETLK, (void *) &l);
		    if (rc) {
			/* Warning iff using non-private CDB locking. */
			rc = ((dbi->dbi_use_dbenv &&
				(dbi->dbi_eflags & DB_INIT_CDB) &&
				!(dbi->dbi_eflags & DB_PRIVATE))
			    ? 0 : 1);
			rpmlog( (rc ? RPMLOG_ERR : RPMLOG_WARNING),
				_("cannot get %s lock on %s/%s\n"),
				((dbi->dbi_mode & (O_RDWR|O_WRONLY))
					? _("exclusive") : _("shared")),
				dbhome, (dbfile ? dbfile : ""));
		    } else if (dbfile) {
			rpmlog(RPMLOG_DEBUG,
				D_("locked   db index       %s/%s\n"),
				dbhome, dbfile);
		    }
		}
	    }
	}
    }

    dbi->dbi_db = db;
    db->app_private = dbi;

DBIDEBUG(dbi, (stderr, "<-- %s(%p,%s,%p) dbi %p rc %d %s\n", __FUNCTION__, rpmdb, tagName(rpmtag), dbip, dbi, rc, _OFLAGS(dbi->dbi_oflags)));

    if (rc == 0 && dbi->dbi_db != NULL && dbip != NULL) {
	dbi->dbi_vec = &db3vec;
	*dbip = dbi;
	if (dbi->dbi_index) {
	    int (*_callback)(DB *, const DBT *, const DBT *, DBT *)
			= db3Acallback;
	    int _flags = (rpmdb->_dbi[0]->dbi_eflags & DB_INIT_TXN)
			? DB_AUTO_COMMIT : 0;
	    xx = db3associate(rpmdb->_dbi[0], dbi, _callback, _flags);
	}
	if (dbi->dbi_seq_id) {
	    char * end = NULL;
	    uint32_t u = (uint32_t) strtoll(dbi->dbi_seq_id, &end, 0);

	    if (*end == '\0')
		xx = seqid_init(dbi,(const char *)&u, sizeof(u), &dbi->dbi_seq);
	    else
		xx = seqid_init(dbi, dbi->dbi_seq_id, 0, &dbi->dbi_seq);
	}
    } else {
	dbi->dbi_verify_on_close = 0;
	(void) db3close(dbi, 0);
	dbi = NULL;
    }

    urlfn = _free(urlfn);

    /*@-nullstate -compmempass@*/
    return rc;
    /*@=nullstate =compmempass@*/
}

/** \ingroup db3
 */
/*@-exportheadervar@*/
/*@observer@*/ /*@unchecked@*/
struct _dbiVec db3vec = {
    DB_VERSION_STRING, DB_VERSION_MAJOR, DB_VERSION_MINOR, DB_VERSION_PATCH,
    db3open, db3close, db3sync, db3associate, db3associate_foreign, db3join,
    db3exists, db3seqno,
    db3copen, db3cclose, db3cdup, db3cdel, db3cget, db3cpget, db3cput, db3ccount,
    db3byteswapped, db3stat
};
/*@=exportheadervar@*/
/*@=type@*/

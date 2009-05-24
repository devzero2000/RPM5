/** \ingroup rpmdb
 * \file rpmdb/dbconfig.c
 */

#include "system.h"

#include <rpmio.h>
#include <popt.h>
#include <rpmlog.h>
#include <rpmmacro.h>

#include <rpmtag.h>
#define	_RPMDB_INTERNAL
#include "rpmdb.h"
#include "debug.h"

/*@access rpmdb@*/
/*@access dbiIndex@*/
/*@access dbiIndexSet@*/

/*@unchecked@*/
int _dbi_debug;

#if defined(WITH_DB) || defined(WITH_SQLITE)

/*@-exportlocal -exportheadervar@*/
/*@unchecked@*/
struct _dbiIndex db3dbi;
/*@=exportlocal =exportheadervar@*/

/*@unchecked@*/
#if defined(WITH_DB)
static int dbi_use_cursors;
#endif

/*@unchecked@*/
#if defined(WITH_DB)
static int dbi_tear_down;
#endif

/*@-compmempass -immediatetrans -exportlocal -exportheadervar -type@*/
/** \ingroup db3
 */
/*@unchecked@*/
struct poptOption rdbOptions[] = {
 /* XXX DB_CXX_NO_EXCEPTIONS */
#if defined(WITH_DB) && defined(DB_CLIENT)
 { "client",	0,POPT_BIT_SET,	&db3dbi.dbi_ecflags, DB_CLIENT,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_RPCCLIENT)
 { "client",	0,POPT_BIT_SET,	&db3dbi.dbi_ecflags, DB_RPCCLIENT,
	NULL, NULL },
 { "rpcclient",	0,POPT_BIT_SET,	&db3dbi.dbi_ecflags, DB_RPCCLIENT,
	NULL, NULL },
#endif

#if defined(WITH_DB) && defined(DB_XA_CREATE)
 { "xa_create",	0,POPT_BIT_SET,	&db3dbi.dbi_cflags, DB_XA_CREATE,
	NULL, NULL },
#endif

/* DB_ENV->open and DB->open */
#if defined(WITH_DB) && defined(DB_AUTO_COMMIT)
 { "auto_commit", 0,POPT_BIT_SET, &db3dbi.dbi_oeflags, DB_AUTO_COMMIT,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_CREATE)
 { "create",	0,POPT_BIT_SET,	&db3dbi.dbi_oeflags, DB_CREATE,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_MULTIVERSION)
 { "multiversion", 0,POPT_BIT_SET, &db3dbi.dbi_oeflags, DB_MULTIVERSION,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_NOMMAP)
 { "nommap",	0,POPT_BIT_SET,	&db3dbi.dbi_oeflags, DB_NOMMAP,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_THREAD)
 { "thread",	0,POPT_BIT_SET,	&db3dbi.dbi_oeflags, DB_THREAD,
	NULL, NULL },
#endif

#if defined(WITH_DB) && defined(DB_FORCE)
 { "force",	0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_FORCE,
	NULL, NULL },
#endif

/* DB_ENV->set_flags */
/* DB_ENV->get_flags */
#if defined(WITH_DB) && defined(DB_INIT_CDB)
 { "cdb",	0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_INIT_CDB,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_INIT_LOCK)
 { "lock",	0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_INIT_LOCK,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_INIT_LOG)
 { "log",	0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_INIT_LOG,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_INIT_MPOOL)
 { "mpool",	0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_INIT_MPOOL,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_INIT_REP)
 { "rep", 0,POPT_BIT_SET, &db3dbi.dbi_eflags, DB_INIT_REP,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_INIT_TXN)
 { "txn",	0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_INIT_TXN,
	NULL, NULL },
#endif

#ifdef	DYING	/* XXX compatibly defined to 0 in db-4.5.20 */
 { "joinenv",	0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_JOINENV,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_LOCKDOWN)
 { "lockdown",	0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_LOCKDOWN,
	NULL, NULL },
#endif
#if (defined(WITH_DB) || defined(WITH_SQLITE)) && defined(DB_PRIVATE)
 { "private",	0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_PRIVATE,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_RECOVER)
 { "recover",	0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_RECOVER,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_RECOVER_FATAL)
 { "recover_fatal", 0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_RECOVER_FATAL,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_REGISTER)
 { "register", 0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_REGISTER,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_SYSTEM_MEM)
 { "shared",	0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_SYSTEM_MEM,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_TXN_NOSYNC)
 { "txn_nosync", 0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_TXN_NOSYNC,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_USE_ENVIRON_ROOT)
 { "use_environ_root", 0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_USE_ENVIRON_ROOT,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_USE_ENVIRON)
 { "use_environ", 0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_USE_ENVIRON,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_IGNORE_LEASE)
 { "ignore_lease", 0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_IGNORE_LEASE,
	NULL, NULL },
#endif

#if defined(WITH_DB) && defined(DB_TXN_SYNC)
 { "txn_sync",	0,POPT_BIT_SET,	&db3dbi.dbi_tflags, DB_TXN_SYNC,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_TXN_NOWAIT)
 { "txn_nowait",0,POPT_BIT_SET,	&db3dbi.dbi_tflags, DB_TXN_NOWAIT,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_TXN_WAIT)
 { "txn_wait",0,POPT_BIT_SET,	&db3dbi.dbi_tflags, DB_TXN_WAIT,
	NULL, NULL },
#endif

#if defined(WITH_DB) && defined(NOTYET)
DB_AUTO_COMMIT
DB_CDB_ALLDB
DB_DIRECT_DB
DB_DIRECT_LOG
DB_DSYNC_DB
DB_DSYNC_LOG
DB_LOG_AUTOREMOVE
DB_LOG_BUFFER_FULL	/* ??? */
DB_LOG_INMEMORY
DB_NOLOCKING
DB_MULTIVERSION
DB_NOMMAP
DB_NOPANIC
DB_OVERWRITE
DB_PANIC_ENVIRONMENT
DB_REGION_INIT
DB_TIME_NOTGRANTED
DB_TXN_NOSYNC
DB_TXN_SNAPSHOT
DB_WRITE_NOSYNC
DB_YIELDCPU
#endif

/* DB->set_flags */
/* DB->get_flags */
#if defined(WITH_DB) && defined(NOTYET)
DB_CHKSUM
DB_ENCRYPT
DB_TXN_NOT_DURABLE

DB_DUP		BTREE HASH
DB_DUPSORT	BTREE HASH
DB_RECNUM	BTREE
DB_REVSPLITOFF	BTREE

DB_INORDER	QUEUE
DB_RENUMBER	RECNO
DB_SNAPSHOT	RECNO
#endif

/* DB->open */
#if (defined(WITH_DB) || defined(WITH_SQLITE)) && defined(DB_EXCL)
 { "excl",	0,POPT_BIT_SET,	&db3dbi.dbi_oflags, DB_EXCL,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_FCNTL_LOCKING)
 { "fcntl_locking",0,POPT_BIT_SET,	&db3dbi.dbi_oflags, DB_FCNTL_LOCKING,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_NO_AUTO_COMMIT) && defined(NOTYET)
 { "noautocommit", 0,POPT_BIT_SET,	&db3dbi.dbi_oflags, DB_NO_AUTO_COMMIT,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_RDONLY)
 { "rdonly",	0,POPT_BIT_SET,	&db3dbi.dbi_oflags, DB_RDONLY,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_RDWRMASTER) && defined(NOTYET)
 { "rdwrmaster", 0,POPT_BIT_SET,	&db3dbi.dbi_oflags, DB_RDWRMASTER,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(NOTYET)
DB_READ_UNCOMITTED
#endif
#if defined(WITH_DB) && defined(DB_TRUNCATE)
 { "truncate",	0,POPT_BIT_SET,	&db3dbi.dbi_oflags, DB_TRUNCATE,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_WRITEOPEN)
 { "writeopen", 0,POPT_BIT_SET,	&db3dbi.dbi_oflags, DB_WRITEOPEN,
	NULL, NULL },
#endif

#if defined(WITH_DB)
 { "btree",	0,POPT_ARG_VAL,		&db3dbi.dbi_type, DB_BTREE,
	NULL, NULL },
 { "hash", 	0,POPT_ARG_VAL,		&db3dbi.dbi_type, DB_HASH,
	NULL, NULL },
 { "recno",	0,POPT_ARG_VAL,		&db3dbi.dbi_type, DB_RECNO,
	NULL, NULL },
 { "queue",	0,POPT_ARG_VAL,		&db3dbi.dbi_type, DB_QUEUE,
	NULL, NULL },
 { "unknown",	0,POPT_ARG_VAL,		&db3dbi.dbi_type, DB_UNKNOWN,
	NULL, NULL },
#endif

 { "root",	0,POPT_ARG_STRING,	&db3dbi.dbi_root, 0,
	NULL, NULL },
 { "home",	0,POPT_ARG_STRING,	&db3dbi.dbi_home, 0,
	NULL, NULL },
 { "file",	0,POPT_ARG_STRING,	&db3dbi.dbi_file, 0,
	NULL, NULL },
 { "subfile",	0,POPT_ARG_STRING,	&db3dbi.dbi_subfile, 0,
	NULL, NULL },
#if defined(WITH_DB)
 { "mode",	0,POPT_ARG_INT,		&db3dbi.dbi_mode, 0,
	NULL, NULL },
#endif
 { "perms",	0,POPT_ARG_INT,		&db3dbi.dbi_perms, 0,
	NULL, NULL },
#if defined(WITH_DB)
 { "shmkey",	0,POPT_ARG_LONG,	&db3dbi.dbi_shmkey, 0,
	NULL, NULL },
#endif
 { "tmpdir",	0,POPT_ARG_STRING,	&db3dbi.dbi_tmpdir, 0,
	NULL, NULL },

#if defined(WITH_DB)
 { "host",	0,POPT_ARG_STRING,	&db3dbi.dbi_host, 0,
	NULL, NULL },
 { "server",	0,POPT_ARG_STRING,	&db3dbi.dbi_host, 0,
	NULL, NULL },
 { "cl_timeout", 0,POPT_ARG_LONG,	&db3dbi.dbi_cl_timeout, 0,
	NULL, NULL },
 { "sv_timeout", 0,POPT_ARG_LONG,	&db3dbi.dbi_sv_timeout, 0,
	NULL, NULL },
#endif

#if defined(WITH_DB)
 { "verify",	0,POPT_ARG_NONE,	&db3dbi.dbi_verify_on_close, 0,
	NULL, NULL },
 { "teardown",	0,POPT_ARG_NONE,	&dbi_tear_down, 0,
	NULL, NULL },
 { "usecursors",0,POPT_ARG_NONE,	&dbi_use_cursors, 0,
	NULL, NULL },
 { "usedbenv",	0,POPT_ARG_NONE,	&db3dbi.dbi_use_dbenv, 0,
	NULL, NULL },
#endif
 { "nofsync",	0,POPT_ARG_NONE,	&db3dbi.dbi_no_fsync, 0,
	NULL, NULL },
#if defined(WITH_DB)
 { "nodbsync",	0,POPT_ARG_NONE,	&db3dbi.dbi_no_dbsync, 0,
	NULL, NULL },
 { "lockdbfd",	0,POPT_ARG_NONE,	&db3dbi.dbi_lockdbfd, 0,
	NULL, NULL },
#endif
 { "noload",	0,POPT_ARG_NONE,	&db3dbi.dbi_noload, 0,
	NULL, NULL },
 { "temporary",	0,POPT_ARG_NONE,	&db3dbi.dbi_temporary, 0,
	NULL, NULL },
#if defined(WITH_DB)
 { "debug",	0,POPT_ARG_NONE,	&db3dbi.dbi_debug, 0,
	NULL, NULL },
#endif

/* XXX set_alloc */
 { "cachesize",	0,POPT_ARG_INT,		&db3dbi.dbi_cachesize, 0,
	NULL, NULL },
#if defined(WITH_DB)
/* XXX set_dup_compare */
/* XXX set_encrypt */
 { "errpfx",	0,POPT_ARG_STRING,	&db3dbi.dbi_errpfx, 0,
	NULL, NULL },
/* XXX set_feedback */
 { "lorder",	0,POPT_ARG_INT,		&db3dbi.dbi_lorder, 0,
	NULL, NULL },
#endif
 { "pagesize",	0,POPT_ARG_INT,		&db3dbi.dbi_pagesize, 0,
	NULL, NULL },

#if defined(WITH_DB)
 { "region_init", 0,POPT_ARG_VAL,	&db3dbi.dbi_region_init, 1,
	NULL, NULL },

 { "thread_count", 0,POPT_ARG_INT,	&db3dbi.dbi_thread_count, 0,
	NULL, NULL },
#endif

#if defined(WITH_DB) && defined(DB_VERB_CHKPOINT)
 { "chkpoint",	0,POPT_BIT_SET,	&db3dbi.dbi_verbose, DB_VERB_CHKPOINT,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_VERB_DEADLOCK)
 { "deadlock",	0,POPT_BIT_SET,	&db3dbi.dbi_verbose, DB_VERB_DEADLOCK,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_VERB_RECOVERY)
 { "recovery",	0,POPT_BIT_SET,	&db3dbi.dbi_verbose, DB_VERB_RECOVERY,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_VERB_REGISTER)
 { "register",	0,POPT_BIT_SET,	&db3dbi.dbi_verbose, DB_VERB_REGISTER,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_VERB_REPLICATION)
 { "replication", 0,POPT_BIT_SET, &db3dbi.dbi_verbose, DB_VERB_REPLICATION,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_VERB_WAITSFOR)
 { "waitsfor",	0,POPT_BIT_SET,	&db3dbi.dbi_verbose, DB_VERB_WAITSFOR,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_VERB_FILEOPS)
 { "fileops",	0,POPT_BIT_SET,	&db3dbi.dbi_verbose, DB_VERB_FILEOPS,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_VERB_FILEOPS_ALL)
 { "fileops_all",0,POPT_BIT_SET,&db3dbi.dbi_verbose, DB_VERB_FILEOPS_ALL,
	NULL, NULL },
#endif
#if defined(WITH_DB)
 { "verbose",	0,POPT_ARG_VAL,		&db3dbi.dbi_verbose, -1,
	NULL, NULL },
#endif

/* ==== Locking: */
/* DB_ENV->lock_detect */
/* DB_ENV->set_lk_detect */
/* DB_ENV->get_lk_detect */
#if defined(WITH_DB) && defined(DB_LOCK_DEFAULT)
 { "lk_default",0,POPT_ARG_VAL,		&db3dbi.dbi_lk_detect, DB_LOCK_DEFAULT,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_LOCK_EXPIRE)
 { "lk_expire",	0,POPT_ARG_VAL,		&db3dbi.dbi_lk_detect, DB_LOCK_EXPIRE,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_LOCK_MAXLOCKS)
 { "lk_maxlocks", 0,POPT_ARG_VAL,	&db3dbi.dbi_lk_detect, DB_LOCK_MAXLOCKS,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_LOCK_MAXWRITE)
 { "lk_maxwrite", 0,POPT_ARG_VAL,	&db3dbi.dbi_lk_detect, DB_LOCK_MAXWRITE,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_LOCK_MINLOCKS)
 { "lk_minlocks", 0,POPT_ARG_VAL,	&db3dbi.dbi_lk_detect, DB_LOCK_MINLOCKS,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_LOCK_MINWRITE)
 { "lk_minwrite", 0,POPT_ARG_VAL,	&db3dbi.dbi_lk_detect, DB_LOCK_MINWRITE,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_LOCK_OLDEST)
 { "lk_oldest",	0,POPT_ARG_VAL,		&db3dbi.dbi_lk_detect, DB_LOCK_OLDEST,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_LOCK_RANDOM)
 { "lk_random",	0,POPT_ARG_VAL,		&db3dbi.dbi_lk_detect, DB_LOCK_RANDOM,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_LOCK_YOUNGEST)
 { "lk_youngest",0, POPT_ARG_VAL,	&db3dbi.dbi_lk_detect, DB_LOCK_YOUNGEST,
	NULL, NULL },
#endif

/* DB_ENV->lock_get */
/* XXX DB_ENV->set_lk_conflicts */
/* XXX DB_ENV->get_lk_conflicts */
#if defined(WITH_DB) && defined(NOTYET)
DB_LOCK_NOWAIT	/* flags */

DB_LOCK_READ	/* mode(s) */
DB_LOCK_WRITE
DB_LOCK_IWRITE
DB_LOCK_IREAD
DB_LOCK_IWR
#endif

#if defined(WITH_DB)
/* XXX DB_ENV->set_lk_max_lockers */
/* XXX DB_ENV->get_lk_max_lockers */
 { "lk_max_lockers", 0,POPT_ARG_INT,	&db3dbi.dbi_lk_max_lockers, 0,
	NULL, NULL },
/* XXX DB_ENV->set_lk_max_locks */
/* XXX DB_ENV->get_lk_max_locks */
 { "lk_max_locks", 0,POPT_ARG_INT,	&db3dbi.dbi_lk_max_locks, 0,
	NULL, NULL },
/* XXX DB_ENV->set_lk_max_objects */
/* XXX DB_ENV->get_lk_max_objects */
 { "lk_max_objects", 0,POPT_ARG_INT,	&db3dbi.dbi_lk_max_objects, 0,
	NULL, NULL },
#endif

/* XXX DB_ENV->set_timeout */
#if defined(WITH_DB) && defined(NOTYET)
DB_SET_LOCK_TIMEOUT
DB_SET_TXN_NOW
DB_SET_TXN_TIMEOUT
#endif
/* XXX DB_ENV->get_timeout */

/* ==== Logging: */
#if defined(WITH_DB)
/* XXX DB_ENV->set_lg_bsize */
/* XXX DB_ENV->get_lg_bsize */
 { "lg_bsize",	0,POPT_ARG_INT,		&db3dbi.dbi_lg_bsize, 0,
	NULL, NULL },
/* XXX DB_ENV->set_lg_dir */
/* XXX DB_ENV->get_lg_dir */
 { "lg_dir",	0,POPT_ARG_STRING,	&db3dbi.dbi_lg_dir, 0,
	NULL, NULL },
/* XXX DB_ENV->set_lg_filemode */
/* XXX DB_ENV->get_lg_filemode */
 { "lg_filemode", 0,POPT_ARG_INT,	&db3dbi.dbi_lg_filemode, 0,
	NULL, NULL },
/* XXX DB_ENV->set_lg_max */
/* XXX DB_ENV->get_lg_max */
 { "lg_max",	0,POPT_ARG_INT,		&db3dbi.dbi_lg_max, 0,
	NULL, NULL },
/* XXX DB_ENV->set_lg_regionmax */
/* XXX DB_ENV->get_lg_regionmax */
 { "lg_regionmax", 0,POPT_ARG_INT,	&db3dbi.dbi_lg_regionmax, 0,
	NULL, NULL },
#endif

/* ==== Memory pool: */
#if defined(WITH_DB)
 { "mp_size",	0,POPT_ARG_INT,		&db3dbi.dbi_cachesize, 0,
	NULL, NULL },
/* XXX DB_ENV->set_mp_max_openfd */
/* XXX DB_ENV->set_mp_max_write */
 { "mmapsize", 0,POPT_ARG_INT,		&db3dbi.dbi_mmapsize, 0,
	NULL, NULL },
 { "mp_mmapsize", 0,POPT_ARG_INT,	&db3dbi.dbi_mmapsize, 0,
	NULL, NULL },
/* XXX DB_MPOOLFILE->set_clear_len */
/* XXX DB_MPOOLFILE->set_fileid */
/* XXX DB_MPOOLFILE->set_ftype */
/* XXX DB_MPOOLFILE->set_lsn_offset */
/* XXX DB_MPOOLFILE->set_maxsize */
/* XXX DB_MPOOLFILE->set_pgcookie */
/* XXX DB_MPOOLFILE->set_priority */
#endif

/* ==== Mutexes: */
#if defined(WITH_DB) && defined(NOTYET)
DB_MUTEX_PROCESS_ONLY	mutex_alloc
DB_MUTEX_SELF_BLOCK	mutex_alloc
DB_STAT_CLEAR		mutex_stat*
#endif
#if defined(WITH_DB)
/* XXX DB_ENV->mutex_set_align */
/* XXX DB_ENV->mutex_get_align */
 { "mutex_align", 0,POPT_ARG_INT,	&db3dbi.dbi_mutex_align, 0,
	NULL, NULL },
/* XXX DB_ENV->mutex_set_increment */
/* XXX DB_ENV->mutex_get_increment */
 { "mutex_increment", 0,POPT_ARG_INT,	&db3dbi.dbi_mutex_increment, 0,
	NULL, NULL },
/* XXX DB_ENV->mutex_set_max */
/* XXX DB_ENV->mutex_get_max */
 { "mutex_max", 0,POPT_ARG_INT,		&db3dbi.dbi_mutex_max, 0,
	NULL, NULL },
/* XXX DB_ENV->mutex_set_tas_spins */
/* XXX DB_ENV->mutex_get_tas_spins */
 { "mutex_tas_spins",	0,POPT_ARG_INT,	&db3dbi.dbi_mutex_tas_spins, 0,
	NULL, NULL },
#endif

/* ==== Replication: */
/* XXX DB_ENV->rep_set_config */
/* XXX DB_ENV->rep_set_limit */
/* XXX DB_ENV->rep_set_nsites */
/* XXX DB_ENV->rep_set_priority */
/* XXX DB_ENV->rep_set_timeout */
/* XXX DB_ENV->rep_set_transport */

/* ==== Sequences: */
#if defined(WITH_DB)
/* XXX DB_SEQUENCE->set_cachesize */
/* XXX DB_SEQUENCE->get_cachesize */
 { "seq_cachesize",	0,POPT_ARG_INT,	&db3dbi.dbi_seq_cachesize, 0,
	NULL, NULL },
#endif
/* XXX DB_SEQUENCE->set_flags */
/* XXX DB_SEQUENCE->get_flags */
#if defined(WITH_DB) && defined(DB_SEQ_DEC)
 { "seq_dec",	0,POPT_BIT_SET,		&db3dbi.dbi_seq_flags, DB_SEQ_DEC,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_SEQ_INC)
 { "seq_inc",	0,POPT_BIT_SET,		&db3dbi.dbi_seq_flags, DB_SEQ_INC,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_SEQ_WRAP)
 { "seq_wrap",	0,POPT_BIT_SET,		&db3dbi.dbi_seq_flags, DB_SEQ_WRAP,
	NULL, NULL },
#endif
/* XXX DB_SEQUENCE->set_range */
/* XXX DB_SEQUENCE->get_range */
#if defined(WITH_DB) && defined(NOTYET)		/* needs signed 64bit type */
 { "seq_min",	0,POPT_ARG_INT,	&db3dbi.dbi_seq_min, 0,
	NULL, NULL },
 { "seq_max",	0,POPT_ARG_INT,	&db3dbi.dbi_seq_max, 0,
	NULL, NULL },
#endif

/* ==== Transactions: */
/* XXX DB_ENV->txn_checkpoint */
/* XXX DB_ENV->txn_recover */
/* XXX DB_ENV->txn_stat */
/* XXX DB_ENV->set_timeout */
/* XXX DB_ENV->get_timeout */
#if defined(WITH_DB)
 { "tx_max",	0,POPT_ARG_INT,		&db3dbi.dbi_tx_max, 0,
	NULL, NULL },
#endif
/* XXX DB_ENV->set_tx_timestamp */

/* XXX set_append_recno */
/* XXX set_bt_compare */
/* XXX set_bt_dup_compare */
/* XXX set_bt_minkey */
/* XXX set_bt_prefix */
#if defined(WITH_DB) && defined(DB_DUP)
 { "bt_dup",	0,POPT_BIT_SET,	&db3dbi.dbi_bt_flags, DB_DUP,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_DUPSORT)
 { "bt_dupsort",0,POPT_BIT_SET,	&db3dbi.dbi_bt_flags, DB_DUPSORT,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_RECNUM)
 { "bt_recnum",	0,POPT_BIT_SET,	&db3dbi.dbi_bt_flags, DB_RECNUM,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_REVSPLITOFF)
 { "bt_revsplitoff", 0,POPT_BIT_SET,	&db3dbi.dbi_bt_flags, DB_REVSPLITOFF,
	NULL, NULL },
#endif

#if defined(WITH_DB) && defined(DB_DUP)
 { "h_dup",	0,POPT_BIT_SET,	&db3dbi.dbi_h_flags, DB_DUP,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_SUPSORT)
 { "h_dupsort",	0,POPT_BIT_SET,	&db3dbi.dbi_h_flags, DB_DUPSORT,
	NULL, NULL },
#endif
#if defined(WITH_DB)
 { "h_ffactor",	0,POPT_ARG_INT,		&db3dbi.dbi_h_ffactor, 0,
	NULL, NULL },
 { "h_nelem",	0,POPT_ARG_INT,		&db3dbi.dbi_h_nelem, 0,
	NULL, NULL },
#endif

#if defined(WITH_DB) && defined(DB_RENUMBER)
 { "re_renumber", 0,POPT_BIT_SET,	&db3dbi.dbi_re_flags, DB_RENUMBER,
	NULL, NULL },
#endif
#if defined(WITH_DB) && defined(DB_SNAPSHOT)
 { "re_snapshot",0,POPT_BIT_SET,	&db3dbi.dbi_re_flags, DB_SNAPSHOT,
	NULL, NULL },
#endif
#if defined(WITH_DB)
 { "re_delim",	0,POPT_ARG_INT,		&db3dbi.dbi_re_delim, 0,
	NULL, NULL },
 { "re_len",	0,POPT_ARG_INT,		&db3dbi.dbi_re_len, 0,
	NULL, NULL },
 { "re_pad",	0,POPT_ARG_INT,		&db3dbi.dbi_re_pad, 0,
	NULL, NULL },
 { "re_source",	0,POPT_ARG_STRING,	&db3dbi.dbi_re_source, 0,
	NULL, NULL },

 { "q_extentsize", 0,POPT_ARG_INT,	&db3dbi.dbi_q_extentsize, 0,
	NULL, NULL },
#endif

    POPT_TABLEEND
};
/*@=compmempass =immediatetrans =exportlocal =exportheadervar =type@*/

dbiIndex db3Free(dbiIndex dbi)
{
    if (dbi != NULL)
	(void)rpmioFreePoolItem((rpmioItem)dbi, __FUNCTION__, __FILE__, __LINE__);
    return NULL;
}

static void dbiFini(void * _dbi)
	/*@modifies _dbi @*/
{
    dbiIndex dbi = _dbi;
    if (dbi) {
	dbi->dbi_root = _free(dbi->dbi_root);
	dbi->dbi_home = _free(dbi->dbi_home);
	dbi->dbi_file = _free(dbi->dbi_file);
	dbi->dbi_subfile = _free(dbi->dbi_subfile);
	dbi->dbi_tmpdir = _free(dbi->dbi_tmpdir);
	dbi->dbi_host = _free(dbi->dbi_host);
	dbi->dbi_errpfx = _free(dbi->dbi_errpfx);
	dbi->dbi_re_source = _free(dbi->dbi_re_source);
	dbi->dbi_stats = _free(dbi->dbi_stats);
    }
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _dbiPool;

static dbiIndex dbiGetPool(rpmioPool pool)
{
    dbiIndex dbi;

    if (_dbiPool == NULL) {
	_dbiPool = rpmioNewPool("dbi", sizeof(*dbi), -1, _dbi_debug,
			NULL, NULL, dbiFini);
	pool = _dbiPool;
    }
    return (dbiIndex) rpmioGetPool(pool, sizeof(*dbi));
}

/*@observer@*/ /*@unchecked@*/
static const char *db3_config_default =
    "hash tmpdir=/var/tmp create cdb mpool mp_mmapsize=16Mb mp_size=1Mb perms=0644";

dbiIndex db3New(rpmdb rpmdb, rpmTag tag)
{
    dbiIndex dbi = dbiGetPool(_dbiPool);
    char * dbOpts = rpmExpand("%{_dbi_config_", tagName(tag), "}", NULL);

    if (!(dbOpts && *dbOpts && *dbOpts != '%')) {
	dbOpts = _free(dbOpts);
	dbOpts = rpmExpand("%{_dbi_config}", NULL);
	if (!(dbOpts && *dbOpts && *dbOpts != '%')) {
	    dbOpts = rpmExpand(db3_config_default, NULL);
	}
    }

    /* Parse the options for the database element(s). */
    if (dbOpts && *dbOpts && *dbOpts != '%') {
	char *o, *oe;
	char *p, *pe;

	memset(&db3dbi, 0, sizeof(db3dbi));
/*=========*/
	for (o = dbOpts; o && *o; o = oe) {
	    struct poptOption *opt;
	    const char * tok;
	    int argInfo;

	    /* Skip leading white space. */
	    while (*o && xisspace((int)*o))
		o++;

	    /* Find and terminate next key=value pair. Save next start point. */
	    for (oe = o; oe && *oe; oe++) {
		if (xisspace((int)*oe))
		    /*@innerbreak@*/ break;
		if (oe[0] == ':' && !(oe[1] == '/' && oe[2] == '/'))
		    /*@innerbreak@*/ break;
	    }
	    if (oe && *oe)
		*oe++ = '\0';
	    if (*o == '\0')
		continue;

	    /* Separate key from value, save value start (if any). */
	    for (pe = o; pe && *pe && *pe != '='; pe++)
		{};
	    p = (pe ? *pe++ = '\0', pe : NULL);

	    /* Skip over negation at start of token. */
	    for (tok = o; *tok == '!'; tok++)
		{};

	    /* Find key in option table. */
	    for (opt = rdbOptions; opt->longName != NULL; opt++) {
		if (strcmp(tok, opt->longName))
		    /*@innercontinue@*/ continue;
		/*@innerbreak@*/ break;
	    }
	    if (opt->longName == NULL) {
		rpmlog(RPMLOG_ERR,
			_("unrecognized db option: \"%s\" ignored.\n"), o);
		continue;
	    }

	    /* Toggle the flags for negated tokens, if necessary. */
	    argInfo = opt->argInfo;
	    if (argInfo == POPT_BIT_SET && *o == '!' && ((tok - o) % 2))
		argInfo = POPT_BIT_CLR;

	    /* Save value in template as appropriate. */
	    switch (argInfo & POPT_ARG_MASK) {

	    case POPT_ARG_NONE:
		(void) poptSaveInt((int *)opt->arg, argInfo, 1L);
		/*@switchbreak@*/ break;
	    case POPT_ARG_VAL:
		(void) poptSaveInt((int *)opt->arg, argInfo, (long)opt->val);
	    	/*@switchbreak@*/ break;
	    case POPT_ARG_STRING:
	    {	const char ** t = opt->arg;
		/*@-mods@*/
		if (t) {
/*@-unqualifiedtrans@*/ /* FIX: opt->arg annotation in popt.h */
		    *t = _free(*t);
/*@=unqualifiedtrans@*/
		    *t = xstrdup( (p ? p : "") );
		}
		/*@=mods@*/
	    }	/*@switchbreak@*/ break;

	    case POPT_ARG_INT:
	    case POPT_ARG_LONG:
	      {	long aLong = strtol(p, &pe, 0);
		if (pe) {
		    if (!xstrncasecmp(pe, "Mb", 2))
			aLong *= 1024 * 1024;
		    else if (!xstrncasecmp(pe, "Kb", 2))
			aLong *= 1024;
		    else if (*pe != '\0') {
			rpmlog(RPMLOG_ERR,
				_("%s has invalid numeric value, skipped\n"),
				opt->longName);
			continue;
		    }
		}

		if ((argInfo & POPT_ARG_MASK) == POPT_ARG_LONG) {
		    if (aLong == LONG_MIN || aLong == LONG_MAX) {
			rpmlog(RPMLOG_ERR,
				_("%s has too large or too small long value, skipped\n"),
				opt->longName);
			continue;
		    }
		    (void) poptSaveLong((long *)opt->arg, argInfo, aLong);
		    /*@switchbreak@*/ break;
		} else {
		    if (aLong > INT_MAX || aLong < INT_MIN) {
			rpmlog(RPMLOG_ERR,
				_("%s has too large or too small integer value, skipped\n"),
				opt->longName);
			continue;
		    }
		    (void) poptSaveInt((int *)opt->arg, argInfo, aLong);
		}
	      }	/*@switchbreak@*/ break;
	    default:
		/*@switchbreak@*/ break;
	    }
	}
/*=========*/
    }

    dbOpts = _free(dbOpts);

/*@-assignexpose@*/
    {	void *use =  dbi->_item.use;
        void *pool = dbi->_item.pool;
/*@i@*/	*dbi = db3dbi;	/* structure assignment */
        dbi->_item.use = use;
        dbi->_item.pool = pool;
    }
/*@=assignexpose@*/

    memset(&db3dbi, 0, sizeof(db3dbi));

    if (!(dbi->dbi_perms & 0600))
	dbi->dbi_perms = 0644;
    dbi->dbi_mode = rpmdb->db_mode;
    /*@-assignexpose -newreftrans@*/ /* FIX: figger rpmdb/dbi refcounts */
/*@i@*/	dbi->dbi_rpmdb = rpmdb;
    /*@=assignexpose =newreftrans@*/
    dbi->dbi_rpmtag = tag;
    
    /*
     * Inverted lists have join length of 2, primary data has join length of 1.
     */
    /*@-sizeoftype@*/
    switch (tag) {
    case RPMDBI_PACKAGES:
    case RPMDBI_DEPENDS:
	dbi->dbi_jlen = 1 * sizeof(rpmuint32_t);
	break;
    default:
	dbi->dbi_jlen = 2 * sizeof(rpmuint32_t);
	break;
    }
    /*@=sizeoftype@*/

    dbi->dbi_byteswapped = -1;	/* -1 unknown, 0 native order, 1 alien order */

#if defined(WITH_DB)
    if (!dbi->dbi_use_dbenv) {		/* db3 dbenv is always used now. */
	dbi->dbi_use_dbenv = 1;
	dbi->dbi_eflags |= (DB_INIT_MPOOL|DB_JOINENV);
	dbi->dbi_mmapsize = 16 * 1024 * 1024;
	dbi->dbi_cachesize = 1 * 1024 * 1024;
    }

    if ((dbi->dbi_bt_flags | dbi->dbi_h_flags) & DB_DUP)
	dbi->dbi_permit_dups = 1;
#endif

    /*@-globstate@*/ /* FIX: *(rdbOptions->arg) reachable */
    return (dbiIndex)rpmioLinkPoolItem((rpmioItem)dbi, __FUNCTION__, __FILE__, __LINE__);
    /*@=globstate@*/
}

const char * prDbiOpenFlags(int dbflags, int print_dbenv_flags)
{
    static char buf[256];
    struct poptOption *opt;
    char * oe;

    oe = buf;
    *oe = '\0';
    for (opt = rdbOptions; opt->longName != NULL; opt++) {
	if (opt->argInfo != POPT_BIT_SET)
	    continue;
	if (print_dbenv_flags) {
	    if (!(opt->arg == &db3dbi.dbi_oeflags ||
		  opt->arg == &db3dbi.dbi_eflags))
		continue;
	} else {
	    if (!(opt->arg == &db3dbi.dbi_oeflags ||
		  opt->arg == &db3dbi.dbi_oflags))
		continue;
	}
	if ((dbflags & opt->val) != opt->val)
	    continue;
	if (oe != buf)
	    *oe++ = ':';
	oe = stpcpy(oe, opt->longName);
	dbflags &= ~opt->val;
    }
    if (dbflags) {
	if (oe != buf)
	    *oe++ = ':';
	    sprintf(oe, "0x%x", (unsigned)dbflags);
    }
    return buf;
}

#endif

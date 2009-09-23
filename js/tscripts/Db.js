if (loglvl) print("--> Db.js");

// ---- access methods
var DB_BTREE		= 1;
var DB_HASH		= 2;
var DB_RECNO		= 3;
var DB_QUEUE		= 4;
var DB_UNKNOWN	= 5;	/* Figure it out on open. */

// -------------------------
var	DB_AGGRESSIVE				= 0x00000001;
var	DB_ARCH_ABS				= 0x00000001;
var	DB_ARCH_DATA				= 0x00000002;
var	DB_ARCH_LOG				= 0x00000004;
var	DB_ARCH_REMOVE				= 0x00000008;
var	DB_AUTO_COMMIT				= 0x00000100;
var	DB_CDB_ALLDB				= 0x00000040;
var	DB_CHKSUM				= 0x00000008;
var	DB_CKP_INTERNAL				= 0x00000002;
var	DB_CREATE				= 0x00000001;
var	DB_CURSOR_BULK				= 0x00000001;
var	DB_CURSOR_TRANSIENT			= 0x00000004;
var	DB_CXX_NO_EXCEPTIONS			= 0x00000002;
var	DB_DIRECT				= 0x00000010;
var	DB_DIRECT_DB				= 0x00000080;
var	DB_DSYNC_DB				= 0x00000200;
var	DB_DUP					= 0x00000010;
var	DB_DUPSORT				= 0x00000004;
var	DB_DURABLE_UNKNOWN			= 0x00000020;
var	DB_ENCRYPT				= 0x00000001;
var	DB_ENCRYPT_AES				= 0x00000001;
var	DB_EXCL					= 0x00000040;
var	DB_EXTENT				= 0x00000040;
var	DB_FAILCHK				= 0x00000020;
var	DB_FAST_STAT				= 0x00000001;
var	DB_FCNTL_LOCKING			= 0x00000800;
var	DB_FLUSH				= 0x00000001;
var	DB_FORCE				= 0x00000001;
var	DB_FOREIGN_ABORT			= 0x00000001;
var	DB_FOREIGN_CASCADE			= 0x00000002;
var	DB_FOREIGN_NULLIFY			= 0x00000004;
var	DB_FREELIST_ONLY			= 0x00000001;
var	DB_FREE_SPACE				= 0x00000002;
var	DB_IGNORE_LEASE				= 0x00002000;
var	DB_IMMUTABLE_KEY			= 0x00000002;
var	DB_INIT_CDB				= 0x00000040;
var	DB_INIT_LOCK				= 0x00000080;
var	DB_INIT_LOG				= 0x00000100;
var	DB_INIT_MPOOL				= 0x00000200;
var	DB_INIT_REP				= 0x00000400;
var	DB_INIT_TXN				= 0x00000800;
var	DB_INORDER				= 0x00000020;
var	DB_JOIN_NOSORT				= 0x00000001;
var	DB_LOCKDOWN				= 0x00001000;
var	DB_LOCK_NOWAIT				= 0x00000001;
var	DB_LOCK_RECORD				= 0x00000002;
var	DB_LOCK_SET_TIMEOUT			= 0x00000004;
var	DB_LOCK_SWITCH				= 0x00000008;
var	DB_LOCK_UPGRADE				= 0x00000010;
var	DB_LOG_AUTO_REMOVE			= 0x00000001;
var	DB_LOG_CHKPNT				= 0x00000002;
var	DB_LOG_COMMIT				= 0x00000004;
var	DB_LOG_DIRECT				= 0x00000002;
var	DB_LOG_DSYNC				= 0x00000004;
var	DB_LOG_IN_MEMORY			= 0x00000008;
var	DB_LOG_NOCOPY				= 0x00000008;
var	DB_LOG_NOT_DURABLE			= 0x00000010;
var	DB_LOG_WRNOSYNC				= 0x00000020;
var	DB_LOG_ZERO				= 0x00000010;
var	DB_MPOOL_CREATE				= 0x00000001;
var	DB_MPOOL_DIRTY				= 0x00000002;
var	DB_MPOOL_DISCARD			= 0x00000001;
var	DB_MPOOL_EDIT				= 0x00000004;
var	DB_MPOOL_FREE				= 0x00000008;
var	DB_MPOOL_LAST				= 0x00000010;
var	DB_MPOOL_NEW				= 0x00000020;
var	DB_MPOOL_NOFILE				= 0x00000001;
var	DB_MPOOL_NOLOCK				= 0x00000002;
var	DB_MPOOL_TRY				= 0x00000040;
var	DB_MPOOL_UNLINK				= 0x00000002;
var	DB_MULTIPLE				= 0x00000800;
var	DB_MULTIPLE_KEY				= 0x00004000;
var	DB_MULTIVERSION				= 0x00000004;
var	DB_MUTEX_ALLOCATED			= 0x00000001;
var	DB_MUTEX_LOCKED				= 0x00000002;
var	DB_MUTEX_LOGICAL_LOCK			= 0x00000004;
var	DB_MUTEX_PROCESS_ONLY			= 0x00000008;
var	DB_MUTEX_SELF_BLOCK			= 0x00000010;
var	DB_MUTEX_SHARED				= 0x00000020;
var	DB_NOLOCKING				= 0x00000400;
var	DB_NOMMAP				= 0x00000008;
var	DB_NOORDERCHK				= 0x00000002;
var	DB_NOPANIC				= 0x00000800;
var	DB_NO_AUTO_COMMIT			= 0x00001000;
var	DB_ODDFILESIZE				= 0x00000080;
var	DB_ORDERCHKONLY				= 0x00000004;
var	DB_OVERWRITE				= 0x00001000;
var	DB_PANIC_ENVIRONMENT			= 0x00002000;
var	DB_PRINTABLE				= 0x00000008;
var	DB_PRIVATE				= 0x00002000;
var	DB_PR_PAGE				= 0x00000010;
var	DB_PR_RECOVERYTEST			= 0x00000020;
var	DB_RDONLY				= 0x00000400;
var	DB_RDWRMASTER				= 0x00002000;
var	DB_READ_COMMITTED			= 0x00000400;
var	DB_READ_UNCOMMITTED			= 0x00000200;
var	DB_RECNUM				= 0x00000040;
var	DB_RECOVER				= 0x00000002;
var	DB_RECOVER_FATAL			= 0x00004000;
var	DB_REGION_INIT				= 0x00004000;
var	DB_REGISTER				= 0x00008000;
var	DB_RENUMBER				= 0x00000080;
var	DB_REPMGR_CONF_2SITE_STRICT		= 0x00000001;
var	DB_REPMGR_PEER				= 0x00000001;
var	DB_REP_ANYWHERE				= 0x00000001;
var	DB_REP_CLIENT				= 0x00000001;
var	DB_REP_CONF_BULK			= 0x00000002;
var	DB_REP_CONF_DELAYCLIENT			= 0x00000004;
var	DB_REP_CONF_INMEM			= 0x00000008;
var	DB_REP_CONF_LEASE			= 0x00000010;
var	DB_REP_CONF_NOAUTOINIT			= 0x00000020;
var	DB_REP_CONF_NOWAIT			= 0x00000040;
var	DB_REP_ELECTION				= 0x00000004;
var	DB_REP_MASTER				= 0x00000002;
var	DB_REP_NOBUFFER				= 0x00000002;
var	DB_REP_PERMANENT			= 0x00000004;
var	DB_REP_REREQUEST			= 0x00000008;
var	DB_REVSPLITOFF				= 0x00000100;
var	DB_RMW					= 0x00001000;
var	DB_RPCCLIENT				= 0x00000001;
var	DB_SALVAGE				= 0x00000040;
var	DB_SA_SKIPFIRSTKEY			= 0x00000080;
var	DB_SA_UNKNOWNKEY			= 0x00000100;
var	DB_SEQ_DEC				= 0x00000001;
var	DB_SEQ_INC				= 0x00000002;
var	DB_SEQ_RANGE_SET			= 0x00000004;
var	DB_SEQ_WRAP				= 0x00000008;
var	DB_SEQ_WRAPPED				= 0x00000010;
var	DB_SET_LOCK_TIMEOUT			= 0x00000001;
var	DB_SET_REG_TIMEOUT			= 0x00000004;
var	DB_SET_TXN_NOW				= 0x00000008;
var	DB_SET_TXN_TIMEOUT			= 0x00000002;
var	DB_SHALLOW_DUP				= 0x00000100;
var	DB_SNAPSHOT				= 0x00000200;
var	DB_STAT_ALL				= 0x00000004;
var	DB_STAT_CLEAR				= 0x00000001;
var	DB_STAT_LOCK_CONF			= 0x00000008;
var	DB_STAT_LOCK_LOCKERS			= 0x00000010;
var	DB_STAT_LOCK_OBJECTS			= 0x00000020;
var	DB_STAT_LOCK_PARAMS			= 0x00000040;
var	DB_STAT_MEMP_HASH			= 0x00000008;
var	DB_STAT_MEMP_NOERROR			= 0x00000010;
var	DB_STAT_SUBSYSTEM			= 0x00000002;
var	DB_ST_DUPOK				= 0x00000200;
var	DB_ST_DUPSET				= 0x00000400;
var	DB_ST_DUPSORT				= 0x00000800;
var	DB_ST_IS_RECNO				= 0x00001000;
var	DB_ST_OVFL_LEAF				= 0x00002000;
var	DB_ST_RECNUM				= 0x00004000;
var	DB_ST_RELEN				= 0x00008000;
var	DB_ST_TOPLEVEL				= 0x00010000;
var	DB_SYSTEM_MEM				= 0x00010000;
var	DB_THREAD				= 0x00000010;
var	DB_TIME_NOTGRANTED			= 0x00008000;
var	DB_TRUNCATE				= 0x00004000;
var	DB_TXN_NOSYNC				= 0x00000001;
var	DB_TXN_NOT_DURABLE			= 0x00000002;
var	DB_TXN_NOWAIT				= 0x00000010;
var	DB_TXN_SNAPSHOT				= 0x00000002;
var	DB_TXN_SYNC				= 0x00000004;
var	DB_TXN_WAIT				= 0x00000008;
var	DB_TXN_WRITE_NOSYNC			= 0x00000020;
var	DB_UNREF				= 0x00020000;
var	DB_UPGRADE				= 0x00000001;
var	DB_USE_ENVIRON				= 0x00000004;
var	DB_USE_ENVIRON_ROOT			= 0x00000008;
var	DB_VERB_DEADLOCK			= 0x00000001;
var	DB_VERB_FILEOPS				= 0x00000002;
var	DB_VERB_FILEOPS_ALL			= 0x00000004;
var	DB_VERB_RECOVERY			= 0x00000008;
var	DB_VERB_REGISTER			= 0x00000010;
var	DB_VERB_REPLICATION			= 0x00000020;
var	DB_VERB_REPMGR_CONNFAIL			= 0x00000040;
var	DB_VERB_REPMGR_MISC			= 0x00000080;
var	DB_VERB_REP_ELECT			= 0x00000100;
var	DB_VERB_REP_LEASE			= 0x00000200;
var	DB_VERB_REP_MISC			= 0x00000400;
var	DB_VERB_REP_MSGS			= 0x00000800;
var	DB_VERB_REP_SYNC			= 0x00001000;
var	DB_VERB_REP_TEST			= 0x00002000;
var	DB_VERB_WAITSFOR			= 0x00004000;
var	DB_VERIFY				= 0x00000002;
var	DB_VERIFY_PARTITION			= 0x00040000;
var	DB_WRITECURSOR				= 0x00000008;
var	DB_WRITELOCK				= 0x00000010;
var	DB_WRITEOPEN				= 0x00008000;
var	DB_YIELDCPU				= 0x00010000;
// -------------------------

// -----
var dbenv = null;
var eoflags = 0;

var errfile = "stdout";
var errpfx = "Stuff";
var pagesize = 1024;
var cachesize = 1024 * 1024;
var little_endian = 1234;
var big_endian = 4321;

var dbfile = "./rpmdb/Stuff";
var dbname = null;
var oflags = DB_CREATE;
var dbtype = DB_HASH;
var dbperms = 0644;

var avg_keysize = 128;
var avg_datasize = 1024;
var h_ffactor = (pagesize - 32) / (avg_keysize + avg_datasize + 8);
var h_nelem = 12345;

var db = new Db(dbenv, eoflags);
ack("typeof db;", "object");
ack("db instanceof Db;", true);
// ack("db.debug = 1;", 1);
// ack("db.debug = 0;", 0);

ack('db.errfile', 'stderr');
ack('db.errfile = errfile', true);
ack('db.errfile', errfile);

ack('db.errpfx', null);
ack('db.errpfx = errpfx', true);
ack('db.errpfx', errpfx);

ack('db.lorder', little_endian);
ack('db.lorder = big_endian', true);
ack('db.lorder', big_endian);
ack('db.lorder = little_endian', true);
ack('db.lorder', little_endian);

ack('db.pagesize', 0);
ack('db.pagesize = pagesize', true);
ack('db.pagesize', pagesize);

ack('db.cachesize', 265564);
ack('db.cachesize = cachesize', true);
ack('db.cachesize >= cachesize', true);

h_ffactor = 50;			// todo: figger the units
ack('db.h_ffactor', 0);
ack('db.h_ffactor = h_ffactor', true);
ack('db.h_ffactor', h_ffactor);

ack('db.h_nelem', 1234);	// todo: why 1234?
ack('db.h_nelem = h_nelem', true);
ack('db.h_nelem', h_nelem);

// -----
ack('db.open(dbfile, dbname, dbtype, oflags,dbperms)', true);
ack('db.dbfile', dbfile);
ack('db.dbname', dbname);
ack('db.type', dbtype);
ack('db.open_flags', oflags);

ack('db.byteswapped', 0);
ack('db.multiple', 0);

// ack('db.bt_minkey', 0);

ack('db.create_dir', null);
// ack('db.create_dir = "."', true);

ack('db.encrypt', 0);
// ack('db.encrypt = "xyzzy"', true);

ack('db.flags', 0);
// ack('db.flags = DB_REGION_INIT', true);

ack('db.msgfile', null);
// ack('db.msgfile = "stdout"', true);

ack('db.priority', 0);
ack('db.q_extentsize', 0);

// ack('db.re_delim', 0);
// ack('db.re_len', 0);
// ack('db.re_pad', 0);
// ack('db.re_source', 0);

ack('db.sync()', true);
ack('db.stat(DB_FAST_STAT)', true);
ack('db.stat_print(DB_FAST_STAT)', true);

ack('db.close(0)', true);
delete db;

var db = new Db(dbenv, eoflags);
ack('typeof db;', 'object');
ack('db instanceof Db;', true);
ack('db.upgrade(dbfile)', true);
ack('db.verify(dbfile,dbname,0)', true);
ack('db.truncate()', true);
delete db;

var db = new Db(dbenv, eoflags);
ack('typeof db;', 'object');
ack('db instanceof Db;', true);
ack('db.remove(dbfile,dbname)', true);
delete db;

if (loglvl) print("<-- Db.js");

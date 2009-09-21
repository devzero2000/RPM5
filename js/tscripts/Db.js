if (loglvl) print("--> Db.js");

// ----- eflags
const DB_INIT_CDB		= 0x00000040;
const DB_INIT_LOCK		= 0x00000080;
const DB_INIT_LOG		= 0x00000100;
const DB_INIT_MPOOL		= 0x00000200;
const DB_INIT_REP		= 0x00000400;
const DB_INIT_TXN		= 0x00000800;

const DB_RECOVER		= 0x00000002;
const DB_RECOVER_FATAL		= 0x00004000;

const DB_CREATE			= 0x00000001;
const DB_LOCKDOWN		= 0x00001000;
const DB_FAILCHK		= 0x00000020;
const DB_PRIVATE		= 0x00002000;
const DB_REGISTER		= 0x00008000;
const DB_SYSTEM_MEM		= 0x00010000;
const DB_THREAD			= 0x00000010;

// ----- flags
const DB_AUTO_COMMIT		= 0x00000100;
const DB_CDB_ALLDB		= 0x00000040;
const DB_DIRECT_DB		= 0x00000080;
const DB_DSYNC_DB		= 0x00000200;
const DB_MULTIVERSION		= 0x00000004;
const DB_NOLOCKING		= 0x00000400;
const DB_NOMMAP			= 0x00000008;
const DB_NOPANIC		= 0x00000800;
const DB_OVERWRITE		= 0x00001000;
const DB_REGION_INIT		= 0x00004000;
const DB_TXN_NOSYNC		= 0x00000001;
const DB_TXN_NOWAIT		= 0x00000010;
const DB_TXN_SNAPSHOT		= 0x00000002;
const DB_TXN_WRITE_NOSYNC	= 0x00000020;
const DB_YIELDCPU		= 0x00010000;

const DB_LOG_IN_MEMORY		= 0x00000008;
const DB_LOG_AUTO_REMOVE	= 0x00000001;

// ----- locking
const DB_LOCK_NORUN		= 0;
const DB_LOCK_DEFAULT		= 1;	/* Default policy. */
const DB_LOCK_EXPIRE		= 2;	/* Only expire locks, no detection. */
const DB_LOCK_MAXLOCKS		= 3;	/* Select locker with max locks. */
const DB_LOCK_MAXWRITE		= 4;	/* Select locker with max writelocks. */
const DB_LOCK_MINLOCKS		= 5;	/* Select locker with min locks. */
const DB_LOCK_MINWRITE		= 6;	/* Select locker with min writelocks. */
const DB_LOCK_OLDEST		= 7;	/* Select oldest locker. */
const DB_LOCK_RANDOM		= 8;	/* Select random locker. */
const DB_LOCK_YOUNGEST		= 9;	/* Select youngest locker. */

// -----
var home = "./rpmdb";
var eflags = DB_CREATE | DB_INIT_LOCK | DB_INIT_MPOOL | DB_INIT_REP | DB_INIT_TXN;

var db = new Db(home, eflags);
ack("typeof db;", "object");
ack("db instanceof Db;", true);
ack("db.debug = 1;", 1);
ack("db.debug = 0;", 0);

ack('db.version', 'Berkeley DB 4.8.24: (August 14, 2009)');
ack('db.major', 4);
ack('db.minor', 8);
ack('db.patch', 24);

ack('db.home', home);
// ack('db.home = home', true);

ack('db.open_flags', eflags);
// ack('db.open_flags = eflags', true);

ack('db.data_dirs', './data');
// ack('db.data_dirs = "./data"', true);	// todo++

ack('db.create_dir', '.');
// ack('db.create_dir = "."', true);

ack('db.encrypt', 0);
// ack('db.encrypt = "xyzzy"', true);

ack('db.errfile', 'stderr');
// ack('db.errfile = "stdout"', true);

ack('db.errpfx', home);
// ack('db.errpfx = "foo"', true);

ack('db.flags', DB_REGION_INIT);
// ack('db.flags = DB_REGION_INIT', true);

ack('db.idirmode', 'rwxr-xr-x');
// ack('db.idirmode = "rwxrwxrwx"', true);

ack('db.msgfile', null);
// ack('db.msgfile = "stdout"', true);

ack('db.shm_key', -1);
// ack('db.shm_key = 0x1234', true);

ack('db.thread_count', 64);
// ack('db.thread_count = 2 * 64', true);

ack('db.cachemax', 1318912);
// ack('db.cachemax = 2 * 1318912', true);

ack('db.cachesize', 1312348);
// ack('db.cachesize = 2 * 1312348', true);

ack('db.ncaches', 1);
// ack('db.ncaches = 2 * 1', true);

ack('db.max_openfd', 100);
// ack('db.max_openfd = 100', true);

ack('db.mmapsize', 16*1024*1024);
// ack('db.mmapsize = 32*1024*1024', true);

ack('db.mutex_align', 4);
// ack('db.mutex_align = 4', true);

ack('db.mutex_inc', 0);
// ack('db.mutex_inc = 0', true);

ack('db.mutex_max', 0);
// ack('db.mutex_max = 0', true);

ack('db.mutex_spins', 1);
// ack('db.mutex_spins = 1', true);

ack('db.lock_timeout', 0);
// ack('db.lock_timeout = 0', true);

ack('db.txn_timeout', 0);
// ack('db.txn_timeout = 0', true);

ack('db.tmp_dir', './tmp');
// ack('db.tmp_dir = "/var/tmp"', true);

ack('db.verbose', false);	// todo++

ack('db.lk_conflicts', false);	// todo++

ack('db.lk_detect', DB_LOCK_DEFAULT);
// ack('db.lk_detect = DB_LOCK_DEFAULT', true);

ack('db.lk_max_lockers', 300000);
// ack('db.lk_max_lockers = 1000', true);

ack('db.lk_max_locks', 300000);
// ack('db.lk_max_locks = 1000', true);

ack('db.lk_max_objects', 300000);
// ack('db.lk_max_objects = 1000', true);

ack('db.lk_partitions', 40);
// ack('db.lk_partitions = 1', true);

ack('db.log_direct', false);
// ack('db.log_direct = 0', true);

ack('db.log_dsync', false);
// ack('db.log_dsync = 0', true);

ack('db.log_autorm', false);
// ack('db.log_autorm = 0', true);

ack('db.log_inmemory', false);
// ack('db.log_inmemory = 0', true);

ack('db.log_zero', false);
// ack('db.log_zero = 0', true);

ack('db.lg_bsize', 65536);
// ack('db.lg_bsize = 2 * 32000', true);

ack('db.lg_dir', './log');
// ack('db.lg_dir = "/var/tmp/log"', true);

ack('db.lg_filemode', 0644);	// todo++

ack('db.lg_max', 10485760);
// ack('db.lg_max = 2 * 10485760', true);

ack('db.lg_regionmax', 130000);
// ack('db.lg_regionmax = 2 * 130000', true);

ack('db.tx_max', 100);
// ack('db.tx_max = 2 * 100', true);

ack('db.tx_timestamp', 0);
// ack('db.tx_timestamp = 0x1234', true);

ack('db.DB_REP_CONF_BULK', 0);
ack('db.DB_REP_CONF_DELAYCLIENT', 0);
ack('db.DB_REP_CONF_INMEM', 0);
ack('db.DB_REP_CONF_LEASE', 0);
ack('db.DB_REP_CONF_NOAUTOINIT', 0);
ack('db.DB_REP_CONF_NOWAIT', 0);
ack('db.DB_REPMGR_CONF_2SITE_STRICT', 0);

ack('db.rep_limit', 0);
ack('db.rep_nsites', 0);
ack('db.rep_priority', 0);

ack('db.DB_REP_ACK_TIMEOUT', 1000000);
ack('db.DB_REP_CHECKPOINT_DELAY', 30000000);
ack('db.DB_REP_CONNECTION_RETRY', 30000000);
ack('db.DB_REP_ELECTION_TIMEOUT', 2000000);
ack('db.DB_REP_ELECTION_RETRY', 10000000);
ack('db.DB_REP_FULL_ELECTION_TIMEOUT', 0);
ack('db.DB_REP_HEARTBEAT_MONITOR', 0);
ack('db.DB_REP_HEARTBEAT_SEND', 0);
ack('db.DB_REP_LEASE_TIMEOUT', 0);

if (loglvl) print("<-- Db.js");

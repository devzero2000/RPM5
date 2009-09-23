if (loglvl) print("--> Dbe.js");

// ----- eflags
var DB_INIT_CDB			= 0x00000040;
var DB_INIT_LOCK		= 0x00000080;
var DB_INIT_LOG			= 0x00000100;
var DB_INIT_MPOOL		= 0x00000200;
var DB_INIT_REP			= 0x00000400;
var DB_INIT_TXN			= 0x00000800;

var DB_RECOVER			= 0x00000002;
var DB_RECOVER_FATAL		= 0x00004000;

var DB_CREATE			= 0x00000001;
var DB_LOCKDOWN			= 0x00001000;
var DB_FAILCHK			= 0x00000020;
var DB_PRIVATE			= 0x00002000;
var DB_REGISTER			= 0x00008000;
var DB_SYSTEM_MEM		= 0x00010000;
var DB_THREAD			= 0x00000010;

// ----- flags
var DB_AUTO_COMMIT		= 0x00000100;
var DB_CDB_ALLDB		= 0x00000040;
var DB_DIRECT_DB		= 0x00000080;
var DB_DSYNC_DB			= 0x00000200;
var DB_MULTIVERSION		= 0x00000004;
var DB_NOLOCKING		= 0x00000400;
var DB_NOMMAP			= 0x00000008;
var DB_NOPANIC			= 0x00000800;
var DB_OVERWRITE		= 0x00001000;
var DB_REGION_INIT		= 0x00004000;
var DB_TXN_NOSYNC		= 0x00000001;
var DB_TXN_NOWAIT		= 0x00000010;
var DB_TXN_SNAPSHOT		= 0x00000002;
var DB_TXN_WRITE_NOSYNC		= 0x00000020;
var DB_YIELDCPU			= 0x00010000;

var DB_LOG_IN_MEMORY		= 0x00000008;
var DB_LOG_AUTO_REMOVE		= 0x00000001;

// ----- locking
var DB_LOCK_NORUN		= 0;
var DB_LOCK_DEFAULT		= 1;	/* Default policy. */
var DB_LOCK_EXPIRE		= 2;	/* Only expire locks, no detection. */
var DB_LOCK_MAXLOCKS		= 3;	/* Select locker with max locks. */
var DB_LOCK_MAXWRITE		= 4;	/* Select locker with max writelocks. */
var DB_LOCK_MINLOCKS		= 5;	/* Select locker with min locks. */
var DB_LOCK_MINWRITE		= 6;	/* Select locker with min writelocks. */
var DB_LOCK_OLDEST		= 7;	/* Select oldest locker. */
var DB_LOCK_RANDOM		= 8;	/* Select random locker. */
var DB_LOCK_YOUNGEST		= 9;	/* Select youngest locker. */

// -----
var home = "./rpmdb";
var eflags = DB_CREATE | DB_INIT_LOCK | DB_INIT_MPOOL | DB_INIT_REP | DB_INIT_TXN;

var dbenv = new Dbe(home, eflags);
ack("typeof dbenv;", "object");
ack("dbenv instanceof Dbe;", true);
ack("dbenv.debug = 1;", 1);
ack("dbenv.debug = 0;", 0);

ack('dbenv.version', 'Berkeley DB 4.8.24: (August 14, 2009)');
ack('dbenv.major', 4);
ack('dbenv.minor', 8);
ack('dbenv.patch', 24);

ack('dbenv.home', home);
// ack('dbenv.home = home', true);

ack('dbenv.open_flags', eflags);
// ack('dbenv.open_flags = eflags', true);

ack('dbenv.data_dirs', './data');
// ack('dbenv.data_dirs = "./data"', true);	// todo++

ack('dbenv.create_dir', '.');
// ack('dbenv.create_dir = "."', true);

ack('dbenv.encrypt', 0);
// ack('dbenv.encrypt = "xyzzy"', true);

ack('dbenv.errfile', 'stderr');
// ack('dbenv.errfile = "stdout"', true);

ack('dbenv.errpfx', home);
// ack('dbenv.errpfx = "foo"', true);

ack('dbenv.flags', DB_REGION_INIT);
// ack('dbenv.flags = DB_REGION_INIT', true);

ack('dbenv.idirmode', 'rwxr-xr-x');
// ack('dbenv.idirmode = "rwxrwxrwx"', true);

ack('dbenv.msgfile', null);
// ack('dbenv.msgfile = "stdout"', true);

ack('dbenv.shm_key', -1);
// ack('dbenv.shm_key = 0x1234', true);

ack('dbenv.thread_count', 64);
// ack('dbenv.thread_count = 2 * 64', true);

ack('dbenv.cachemax', 1318912);
// ack('dbenv.cachemax = 2 * 1318912', true);

ack('dbenv.cachesize', 1312348);
// ack('dbenv.cachesize = 2 * 1312348', true);

ack('dbenv.ncaches', 1);
// ack('dbenv.ncaches = 2 * 1', true);

ack('dbenv.max_openfd', 100);
// ack('dbenv.max_openfd = 100', true);

ack('dbenv.mmapsize', 16*1024*1024);
// ack('dbenv.mmapsize = 32*1024*1024', true);

ack('dbenv.mutex_align', 4);
// ack('dbenv.mutex_align = 4', true);

ack('dbenv.mutex_inc', 0);
// ack('dbenv.mutex_inc = 0', true);

ack('dbenv.mutex_max', 0);
// ack('dbenv.mutex_max = 0', true);

ack('dbenv.mutex_spins', 1);
// ack('dbenv.mutex_spins = 1', true);

ack('dbenv.lock_timeout', 0);
// ack('dbenv.lock_timeout = 0', true);

ack('dbenv.txn_timeout', 0);
// ack('dbenv.txn_timeout = 0', true);

ack('dbenv.tmp_dir', './tmp');
// ack('dbenv.tmp_dir = "/var/tmp"', true);

ack('dbenv.verbose', false);	// todo++

ack('dbenv.lk_conflicts', false);	// todo++

ack('dbenv.lk_detect', DB_LOCK_DEFAULT);
// ack('dbenv.lk_detect = DB_LOCK_DEFAULT', true);

ack('dbenv.lk_max_lockers', 300000);
// ack('dbenv.lk_max_lockers = 1000', true);

ack('dbenv.lk_max_locks', 300000);
// ack('dbenv.lk_max_locks = 1000', true);

ack('dbenv.lk_max_objects', 300000);
// ack('dbenv.lk_max_objects = 1000', true);

ack('dbenv.lk_partitions', 40);
// ack('dbenv.lk_partitions = 1', true);

ack('dbenv.log_direct', false);
// ack('dbenv.log_direct = 0', true);

ack('dbenv.log_dsync', false);
// ack('dbenv.log_dsync = 0', true);

ack('dbenv.log_autorm', false);
// ack('dbenv.log_autorm = 0', true);

ack('dbenv.log_inmemory', false);
// ack('dbenv.log_inmemory = 0', true);

ack('dbenv.log_zero', false);
// ack('dbenv.log_zero = 0', true);

ack('dbenv.lg_bsize', 65536);
// ack('dbenv.lg_bsize = 2 * 32000', true);

ack('dbenv.lg_dir', './log');
// ack('dbenv.lg_dir = "/var/tmp/log"', true);

ack('dbenv.lg_filemode', 0644);	// todo++

ack('dbenv.lg_max', 10485760);
// ack('dbenv.lg_max = 2 * 10485760', true);

ack('dbenv.lg_regionmax', 130000);
// ack('dbenv.lg_regionmax = 2 * 130000', true);

ack('dbenv.tx_max', 100);
// ack('dbenv.tx_max = 2 * 100', true);

ack('dbenv.tx_timestamp', 0);
// ack('dbenv.tx_timestamp = 0x1234', true);

ack('dbenv.DB_REP_CONF_BULK', 0);
ack('dbenv.DB_REP_CONF_DELAYCLIENT', 0);
ack('dbenv.DB_REP_CONF_INMEM', 0);
ack('dbenv.DB_REP_CONF_LEASE', 0);
ack('dbenv.DB_REP_CONF_NOAUTOINIT', 0);
ack('dbenv.DB_REP_CONF_NOWAIT', 0);
ack('dbenv.DB_REPMGR_CONF_2SITE_STRICT', 0);

ack('dbenv.rep_limit', 0);
ack('dbenv.rep_nsites', 0);
ack('dbenv.rep_priority', 0);

ack('dbenv.DB_REP_ACK_TIMEOUT', 1000000);
ack('dbenv.DB_REP_CHECKPOINT_DELAY', 30000000);
ack('dbenv.DB_REP_CONNECTION_RETRY', 30000000);
ack('dbenv.DB_REP_ELECTION_TIMEOUT', 2000000);
ack('dbenv.DB_REP_ELECTION_RETRY', 10000000);
ack('dbenv.DB_REP_FULL_ELECTION_TIMEOUT', 0);
ack('dbenv.DB_REP_HEARTBEAT_MONITOR', 0);
ack('dbenv.DB_REP_HEARTBEAT_SEND', 0);
ack('dbenv.DB_REP_LEASE_TIMEOUT', 0);

dbenv.close(0);

if (loglvl) print("<-- Dbe.js");

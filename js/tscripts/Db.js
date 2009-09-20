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
const DB_ENV_AUTO_COMMIT	= 0x00000001; /* DB_AUTO_COMMIT */
const DB_ENV_CDB_ALLDB		= 0x00000002; /* CDB environment wide locking */
const DB_ENV_FAILCHK		= 0x00000004; /* Failchk is running */
const DB_ENV_DIRECT_DB		= 0x00000008; /* DB_DIRECT_DB set */
const DB_ENV_DSYNC_DB		= 0x00000010; /* DB_DSYNC_DB set */
const DB_ENV_MULTIVERSION	= 0x00000020; /* DB_MULTIVERSION set */
const DB_ENV_NOLOCKING		= 0x00000040; /* DB_NOLOCKING set */
const DB_ENV_NOMMAP		= 0x00000080; /* DB_NOMMAP set */
const DB_ENV_NOPANIC		= 0x00000100; /* Okay if panic set */
const DB_ENV_OVERWRITE		= 0x00000200; /* DB_OVERWRITE set */
const DB_ENV_REGION_INIT	= 0x00000400; /* DB_REGION_INIT set */
const DB_ENV_RPCCLIENT		= 0x00000800; /* DB_RPCCLIENT set */
const DB_ENV_RPCCLIENT_GIVEN	= 0x00001000; /* User-supplied RPC client struct */
const DB_ENV_TIME_NOTGRANTED	= 0x00002000; /* DB_TIME_NOTGRANTED set */
const DB_ENV_TXN_NOSYNC		= 0x00004000; /* DB_TXN_NOSYNC set */
const DB_ENV_TXN_NOWAIT		= 0x00008000; /* DB_TXN_NOWAIT set */
const DB_ENV_TXN_SNAPSHOT	= 0x00010000; /* DB_TXN_SNAPSHOT set */
const DB_ENV_TXN_WRITE_NOSYNC	= 0x00020000; /* DB_TXN_WRITE_NOSYNC set */
const DB_ENV_YIELDCPU		= 0x00040000; /* DB_YIELDCPU set */

// -----
var home = "./rpmdb";
var eflags = DB_CREATE | DB_INIT_MPOOL | DB_INIT_LOCK | DB_INIT_TXN;

var db = new Db(home, eflags);
ack("typeof db;", "object");
ack("db instanceof Db;", true);
ack("db.debug = 1;", 1);
ack("db.debug = 0;", 0);

print("version:	"+db.version);
print("major:	"+db.major);
print("minor:	"+db.minor);
print("patch:	"+db.patch);

print("home:	"+db.home);
print("open_flags:	0x"+db.open_flags.toString(16));
print("data_dirs:	"+db.data_dirs);
print("create_dir:	"+db.create_dir);
print("encrypt:	"+db.encrypt_flags);
print("errfile:	"+db.errfile);
print("errpfx:	"+db.errpfx);
print("flags:	0x"+db.flags.toString(16));
print("idirmode:	"+db.idirmode);
print("msgfile:	"+db.msgfile);
print("shm_key:	"+db.shm_key);
print("thread_count:	"+db.thread_count);

print("cachesize:	"+db.cachesize);
print("ncaches:	"+db.ncaches);

print("lock_timeout:	"+db.lock_timeout);
print("txn_timeout:	"+db.txn_timeout);

print("tmp_dir:	"+db.tmp_dir);
print("verbose:	"+db.verbose);

print("lk_conflicts:	"+db.lk_conflicts);
print("lk_detect:	"+db.lk_detect);
print("lk_max_lockers: "+db.lk_max_lockers);
print("lk_max_locks:	"+db.lk_max_locks);
print("lk_max_objects:	"+db.lk_max_objects);
print("lk_partitions:	"+db.lk_partitions);

print("log_direct:	"+db.log_direct);
print("log_dsync:	"+db.log_dsync);
print("log_autorm:	"+db.log_autorm);
print("log_inmemory:	"+db.log_inmemory);
print("log_zero:	"+db.log_zero);

print("lg_bsize:	"+db.lg_bsize);
print("lg_dir:	"+db.lg_dir);
print("lg_filemode:	"+db.lg_filemode);
print("lg_max:	"+db.lg_max);
print("lg_regionmax:	"+db.lg_regionmax);

if (loglvl) print("<-- Db.js");

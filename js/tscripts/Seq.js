if (loglvl) print("--> Seq.js");

var home = "./rpmdb";
var eflags = DB_CREATE | DB_INIT_LOCK | DB_INIT_MPOOL | DB_INIT_REP | DB_INIT_TXN;
var emode = 0;

var dbenv = new Dbe();
ack("typeof dbenv;", "object");
ack("dbenv instanceof Dbe;", true);
ack('dbenv.open(home, eflags, emode)', true);

var dbfile = "Sequence";
var dbname = "Sequence";
var oflags = DB_CREATE | DB_AUTO_COMMIT;
var dbtype = DB_HASH;
var dbperms = 0644;

var db = new Db(dbenv);
ack("typeof db;", "object");
ack("db instanceof Db;", true);

var txn = null;
ack('db.open(txn, dbfile, dbname, dbtype, oflags, dbperms)', true);

var seq = new Seq(db);
ack("typeof seq;", "object");
ack("seq instanceof Seq;", true);
ack("seq.debug = 1;", 1);
ack("seq.debug = 0;", 0);

ack("seq.db", db);

// only effective at creation time
var initval = 1234;
// var rangemin = -5678;
// var rangemax =  5678;
var rangemin = -9223372036854776000;
var rangemax =  9223372036854776000;
ack('seq.initval = initval', true);

var key = "foo";
var soflags = DB_CREATE | DB_THREAD;
var flags = DB_SEQ_INC;

// must be 0 if txn != null
var cachesize = 0;
ack('seq.cachesize = cachesize', true);

// var txn = dbenv.txn_begin(null, 0);
// ack('typeof txn', "object");
// ack('txn instanceof Txn', true);

ack('seq.open(txn, key, soflags)', true);
ack("seq.key", key);
// ack('seq.rangemin = rangemin', true);
// ack('seq.rangemax = rangemax', true);

ack("seq.cachesize", cachesize);
ack("seq.flags", flags);
ack("seq.rangemin", rangemin);
ack("seq.rangemax", rangemax);

ack("seq.st_wait", 0);
ack("seq.st_nowait", 0);
ack("seq.st_current", seq.st_value);
ack("seq.st_last_value", seq.st_value - 1);
ack("seq.st_min", rangemin);
ack("seq.st_max", rangemax);
ack("seq.st_cachesize", cachesize);
ack("seq.st_flags", flags);

ack('seq.get(txn, 1, DB_TXN_NOSYNC)', seq.st_current);
ack('seq.get(txn, 1, DB_TXN_NOSYNC)', seq.st_current);
ack('seq.get(txn, 1, DB_TXN_NOSYNC)', seq.st_current);

// ack('txn.commit()', true);
// delete txn;

ack('seq.close(0)', true);
delete seq;

ack('db.close(0)', true);
delete db;

ack('dbenv.close(0)', true);
delete dbenv

if (loglvl) print("<-- Seq.js");

if (loglvl) print("--> Db.js");

const DB_CREATE                 = 0x00000001;
const DB_HASH			= 2;

var oflags = DB_CREATE;
var dbtype = DB_HASH;

var db = new Db();
ack("typeof db;", "object");
ack("db instanceof Db;", true);
ack("db.debug = 1;", 1);
ack("db.debug = 0;", 0);

ack('db.byteswapped', 0);
ack('db.dbfile', null);
ack('db.dbname', null);
ack('db.multiple', 0);

ack('db.open_flags', oflags);
// ack('db.open_flags = oflags', true);

ack('db.type', dbtype);

// ack('db.bt_minkey', 0);

ack('db.cachesize', 265564);
// ack('db.cachesize = 2 * 1312348', true);

ack('db.create_dir', null);
// ack('db.create_dir = "."', true);

ack('db.encrypt', 0);
// ack('db.encrypt = "xyzzy"', true);

ack('db.errfile', 'stderr');
// ack('db.errfile = "stdout"', true);

ack('db.errpfx', null);
// ack('db.errpfx = "foo"', true);

ack('db.flags', 0);
// ack('db.flags = DB_REGION_INIT', true);

ack('db.h_ffactor', 0);
ack('db.h_nelem', 0);
ack('db.lorder', 1234);

ack('db.msgfile', null);
// ack('db.msgfile = "stdout"', true);

ack('db.priority', 0);
ack('db.q_extentsize', 0);

// ack('db.re_delim', 0);
// ack('db.re_len', 0);
// ack('db.re_pad', 0);
// ack('db.re_source', 0);

if (loglvl) print("<-- Db.js");

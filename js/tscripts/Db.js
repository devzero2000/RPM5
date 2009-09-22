if (loglvl) print("--> Db.js");

var db = new Db();
ack("typeof db;", "object");
ack("db instanceof Db;", true);
ack("db.debug = 1;", 1);
ack("db.debug = 0;", 0);

// ack('db.version', 'Berkeley DB 4.8.24: (August 14, 2009)');
// ack('db.major', 4);
// ack('db.minor', 8);
// ack('db.patch', 24);

if (loglvl) print("<-- Db.js");

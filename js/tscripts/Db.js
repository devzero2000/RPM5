if (loglvl) print("--> Db.js");

var db = new Db();
ack("typeof db;", "object");
ack("db instanceof Db;", true);
ack("db.debug = 1;", 1);
ack("db.debug = 0;", 0);

if (loglvl) print("<-- Db.js");

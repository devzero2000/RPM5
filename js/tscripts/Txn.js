if (loglvl) print("--> Txn.js");

var txn = new Txn();
ack("typeof txn;", "object");
ack("txn instanceof Txn;", true);
ack("txn.debug = 1;", 1);
ack("txn.debug = 0;", 0);

if (loglvl) print("<-- Txn.js");

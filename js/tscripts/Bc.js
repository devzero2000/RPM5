if (loglvl) print("--> Bc.js");

var bc = new Bc();
ack("typeof bc;", "object");
ack("bc instanceof Bc;", true);
ack("bc.debug = 1;", 1);
ack("bc.debug = 0;", 0);

if (loglvl) print("<-- Bc.js");

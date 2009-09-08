if (loglvl) print("--> Sp.js");

var sp = new Sp();
ack("typeof sp;", "object");
ack("sp instanceof Sp;", true);
ack("sp.debug = 1;", 1);
ack("sp.debug = 0;", 0);

if (loglvl) print("<-- Sp.js");

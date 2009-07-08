if (loglvl) print("--> Fc.js");

var fc = new Fc();
ack("typeof fc;", "object");
ack("fc instanceof Fc;", true);
ack("fc.debug = 1;", 1);
ack("fc.debug = 0;", 0);

if (loglvl) print("<-- Fc.js");

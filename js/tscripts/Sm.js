if (loglvl) print("--> Sm.js");

var dn = "targeted";

var sm = new Sm(dn);
ack("typeof sm;", "object");
ack("sm instanceof Sm;", true);
ack("sm.debug = 1;", 1);
ack("sm.debug = 0;", 0);

delete sm;

if (loglvl) print("<-- Sm.js");

if (loglvl) print("--> Sm.js");

var sm = new Sm();
ack("typeof sm;", "object");
ack("sm instanceof Sm;", true);
ack("sm.debug = 1;", 1);
ack("sm.debug = 0;", 0);

if (loglvl) print("<-- Sm.js");

if (loglvl) print("--> Gi.js");

var gi = new Gi();
ack("typeof gi;", "object");
ack("gi instanceof Gi;", true);
ack("gi.debug = 1;", 1);
ack("gi.debug = 0;", 0);

if (loglvl) print("<-- Gi.js");

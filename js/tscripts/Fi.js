if (loglvl) print("--> Fi.js");

var fi = new Fi(h);
ack("typeof fi;", "object");
ack("fi instanceof Fi;", true);
ack("fi.debug = 1;", 1);
ack("fi.debug = 0;", 0);

if (loglvl) print("<-- Fi.js");

if (loglvl) print("--> Sx.js");

var sx = new Sx();
ack("typeof sx;", "object");
ack("sx instanceof Sx;", true);
ack("sx.debug = 1;", 1);
ack("sx.debug = 0;", 0);

if (loglvl) print("<-- Sx.js");

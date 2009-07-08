if (loglvl) print("--> Xar.js");

var xar = new Xar();
ack("typeof xar;", "object");
ack("xar instanceof Xar;", true);
ack("xar.debug = 1;", 1);
ack("xar.debug = 0;", 0);

if (loglvl) print("<-- Xar.js");

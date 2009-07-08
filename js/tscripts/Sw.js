if (loglvl) print("--> Sw.js");

var sw = new Sw();
ack("typeof sw;", "object");
ack("sw instanceof Sw;", true);
ack("sw.debug = 1;", 1);
ack("sw.debug = 0;", 0);

if (loglvl) print("<-- Sw.js");

if (loglvl) print("--> Mpf.js");

var mpf = new Mpf();
ack("typeof mpf;", "object");
ack("mpf instanceof Mpf;", true);
ack("mpf.debug = 1;", 1);
ack("mpf.debug = 0;", 0);

if (loglvl) print("<-- Mpf.js");

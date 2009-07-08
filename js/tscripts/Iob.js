if (loglvl) print("--> Iob.js");

var iob = new Iob();
ack("typeof iob;", "object");
ack("iob instanceof Iob;", true);
ack("iob.debug = 1;", 1);
ack("iob.debug = 0;", 0);

if (loglvl) print("<-- Iob.js");

if (loglvl) print("--> Seq.js");

var seq = new Seq();
ack("typeof seq;", "object");
ack("seq instanceof Seq;", true);
ack("seq.debug = 1;", 1);
ack("seq.debug = 0;", 0);

if (loglvl) print("<-- Seq.js");

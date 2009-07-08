if (loglvl) print("--> Dig.js");

var dig = new Dig();
ack("typeof dig;", "object");
ack("dig instanceof Dig;", true);
ack("dig.debug = 1;", 1);
ack("dig.debug = 0;", 0);

if (loglvl) print("<-- Dig.js");

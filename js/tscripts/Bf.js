if (loglvl) print("--> Bf.js");

var bf = new Bf();
ack("typeof bf;", "object");
ack("bf instanceof Bf;", true);
// ack("bf = 1;", 1);
// ack("bf = 0;", 0);

ack("bf.add('foo');", true);
ack("bf.add('bar');", true);

ack("bf.chk('foo');", true);
ack("bf.chk('bar');", true);
ack("bf.chk('baz');", false);

ack("bf.del('bar');", true);
ack("bf.del('foo');", true);

ack("bf.clr();", true);

if (loglvl) print("<-- Bf.js");

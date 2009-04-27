if (loglvl) print("--> Ds.js");

var ds = new Ds('rpmlib');
ack("typeof ds;", "object");
ack("ds instanceof Ds;", true);
ack("ds.debug = 1;", 1);
ack("ds.debug = 0;", 0);

ack("ds.length", 22);
ack("ds.type", "Provides");
ack("ds.buildtime = 1234;", 1234);
ack("ds.color = 4;", 4);
ack("ds.nopromote = 1;", 1);
ack("ds.nopromote = 0;", 0);

ack("ds.ix = -1;", -1);
ack("ds.ix++;", -1);
ack("++ds.ix;", 1);
ack("ds.N", "BuiltinJavaScript");
ack("ds.EVR", "5.2-1");
ack("ds.F", 8);
ack("ds.DNEVR", "P rpmlib(BuiltinJavaScript) = 5.2-1");

if (loglvl) print("<-- Ds.js");

if (loglvl) print("--> Te.js");

var ts = new Ts();

var te = new Te(ts)
ack("typeof te;", "object");
ack("te instanceof Te;", true);
ack("te.debug = 1;", 1);
ack("te.debug = 0;", 0);
delete te;

ack("ts.add('+time')", true);
ack("ts.add('+popt')", true);
ack("ts.check()", true);
ack("ts.order()", true);

te = ts[0];

ack("te.N", "popt");
ack("te.E", 0);
ack("te.V", "1.13");
ack("te.R", "5.fc11");
ack("te.A", "i586");
ack("te.O", "linux");
ack("te.NEVR", "popt-1.13-5.fc11.i586");
ack("te.NEVRA", "popt-1.13-5.fc11.i586");
ack("te.pkgid", "239098a0368d5589e9dd432944d5eb06");
ack("te.hdrid", "ca6058bf3cbde1ab84dfdb901a1b32b3dc152f8e");
ack("te.color", 1);
ack("te.fsize", 0);
ack("te.breadth", 0);
ack("te.depth", 0);
ack("te.npreds", 0);
ack("te.degree", 0);
ack("te.parent", 0);
ack("te.tree", 2);
ack("te.addedkey", 1);
ack("te.dboffset", 0);
ack("te.key", 0);
ack("te.sourcerpm", "popt-1.13-5.fc11.src.rpm");

delete te;

delete ts;

if (loglvl) print("<-- Te.js");

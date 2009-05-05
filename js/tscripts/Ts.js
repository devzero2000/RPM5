if (loglvl) print("--> Ts.js");

var ts = new Ts();
ack("typeof ts;", "object");
ack("ts instanceof Ts;", true);
ack("ts.debug = 1;", 1);
ack("ts.debug = 0;", 0);

var rootdir = "/path/to/rootdir";
ack("ts.rootdir = rootdir", rootdir);
ack("ts.rootdir = \"/\"", "/");

var vsflags = 0x1234;
ack("ts.vsflags = vsflags", vsflags);
ack("ts.vsflags = 0", 0);

ack("ts.add('-popt')", true);
ack("ts.add('+bash')", true);
ack("ts.add('glibc')", true);

var ix = 0;
for (var [key,te] in Iterator(ts)) {
    ack("typeof te;", "object");
    ack("te instanceof Te;", true);
    ack("key == ix", true);
    ix++;
}
ack("ts.length == ix", true);

ack("ts.check()", true);
ack("ts.order()", true);
ack("ts.run()", true);

if (loglvl) print("<-- Ts.js");

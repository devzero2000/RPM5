var ts = new Ts();
ack("typeof ts;", "object");
ack("ts instanceof Ts;", true);
ack("ts.debug = 0;", 0);
ack("ts.debug = 1;", 1);

var rootdir = "/path/to/rootdir";
ack("ts.rootdir = rootdir", rootdir);
ack("ts.rootdir = \"/\"", "/");

var vsflags = 0x1234;
ack("ts.vsflags = vsflags", vsflags);
ack("ts.vsflags = 0", 0);

ack("ts.NVRA[37]", false);

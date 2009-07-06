if (loglvl) print("--> Io.js");

var io = new Io();
ack("typeof io;", "object");
ack("io instanceof Io;", true);
// ack("io.debug = 1;", 1);
// ack("io.debug = 0;", 0);

io();
io('/tmp/iotest');
print(io.fread());
io();

if (loglvl) print("<-- Io.js");

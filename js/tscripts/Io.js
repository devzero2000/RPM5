if (loglvl) print("--> Io.js");

var io = new Io();
ack("typeof io;", "object");
ack("io instanceof Io;", true);
// ack("io.debug = 1;", 1);
// ack("io.debug = 0;", 0);

ack("io() instanceof Io;", true);

var tmpdir = '/tmp';
var fn = tmpdir + '/io.file';
var yadda = 'yadda yadda';

ack("io(fn, 'w').fwrite(yadda);", true);
ack("io.fflush();", true);
ack("io.fstat() instanceof St;", true);
ack("io.fchown(-1,-1);", 0);
ack("io.ferror();", true);
ack("io.fileno();", undefined);
ack("io() instanceof Io;", true);

ack("io(fn, 'r') instanceof Io;", true);
ack("io.fread();", yadda);
ack("io.fstat() instanceof St;", true);
ack("io.fchown(-1,-1);", 0);
ack("io.ferror();", true);
ack("io.fileno();", undefined);
ack("io() instanceof Io;", true);

if (loglvl) print("<-- Io.js");

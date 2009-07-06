if (loglvl) print("--> Sys.js");

var sys = new Sys();
ack("typeof sys;", "object");
ack("sys instanceof Sys;", true);
ack("sys.debug = 1;", 1);
ack("sys.debug = 0;", 0);

var tmpdir = "/tmp";
var dn = tmpdir + "/sys.d";
var fn = tmpdir + "/sys.file";

ack("sys.mkdir(dn);", 0);
ack("sys.rmdir(dn);", 0);
ack("sys.unlink(fn);", 0);

if (loglvl) print("<-- Sys.js");

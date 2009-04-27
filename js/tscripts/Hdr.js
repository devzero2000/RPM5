if (loglvl) print("--> Hdr.js");

// var RPMTAG_NAME = 1000;
// var ts = new Ts();
// var mi = ts.mi(RPMTAG_NAME, 'popt');
// var mi = new Mi(ts, RPMTAG_NAME, 'popt');

// var h = mi.next()
var h = new Hdr();
ack("typeof h;", "object");
ack("h instanceof Hdr;", true);
ack("h.debug = 1;", 1);
ack("h.debug = 0;", 0);

// ack("h.name", "popt");
// ack("h.epoch", false);
// ack("h.version", undefined);
// ack("h.release", undefined);
// ack("h.arch", undefined);
// ack("h.os", undefined);

if (loglvl) print("<-- Hdr.js");

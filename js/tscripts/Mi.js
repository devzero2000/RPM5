if (loglvl) print("--> Mi.js");

var RPMTAG_NAME = 1000;
// var mi = ts.mi(RPMTAG_NAME, 'popt');
var mi = new Mi(ts, RPMTAG_NAME, 'popt');
ack("typeof mi;", "object");
ack("mi instanceof Mi;", true);
ack("mi.debug = 1;", 1);
ack("mi.debug = 0;", 0);

// ack("mi.instance", false);
// ack("mi.count", false);

if (loglvl) print("<-- Mi.js");

if (loglvl) print("--> Mi.js");

var RPMTAG_NAME = 1000;
var N = "popt";

// var mi = ts.mi(RPMTAG_NAME, N);
var mi = new Mi(ts, RPMTAG_NAME, N);
ack("typeof mi;", "object");
ack("mi instanceof Mi;", true);
ack("mi.debug = 1;", 1);
ack("mi.debug = 0;", 0);

ack("mi.length", 1);
ack("mi.count", 1);
ack("mi.instance", 0);	// zero before iterating
ack("mi.pattern(RPMTAG_NAME, N)", true);

for (var [key,h] in Iterator(mi)) {
    nack("mi.instance", 0);	// non-zero when iterating
    ack("h.name", N);
}

if (loglvl) print("<-- Mi.js");

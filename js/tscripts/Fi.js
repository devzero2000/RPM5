if (loglvl) print("--> Fi.js");

var RPMTAG_NAME = 1000;
var N = "popt";

var ts = new Ts();
// var mi = ts.mi(RPMTAG_NAME, 'popt');
var mi = new Mi(ts, RPMTAG_NAME, N);
ack("mi.length", 1);
ack("mi.count", 1);
ack("mi.instance", 0);  // zero before iterating

// var h = new Hdr();
for (var [dbkey,h] in Iterator(mi)) {
    nack("mi.instance", 0); // non-zero when iterating
//  ack("mi.instance", h.instance);

    var fi = new Fi(h);
    ack("typeof fi;", "object");
    ack("fi instanceof Fi;", true);
    ack("fi.debug = 1;", 1);
    ack("fi.debug = 0;", 0);

    var files = new Array();
    var ix = 0;
    for (let [fikey,fival] in Iterator(fi)) {
        files[ix++] = fival;
    }
    ack("ix == fi.length", true);

    ix = 0
    for (var [key,val] in Iterator(fi)) {
        ack("key == ix", true);
        ack("files[key] == val", true);
        ack("fi[key] == val", true);
        ack("fi[key] == files[key]", true);
        ix++;
    }
    ack("ix == fi.length", true);

    ack("fi.fc", undefined);
    ack("fi.fx", undefined);
    ack("fi.dc", undefined);
    ack("fi.dx", undefined);

    ack("fi.dx = -1;", -1);
    ack("++fi.dx;", 0);
    ack("++fi.dx;", 1);

    ack("fi.fx = -1;", -1);
    ack("fi.fx += 1;", 0);
    ack("fi.fx += 1;", 1);
    ack("fi.fx += 1;", 2);

    ack("fi.bn", undefined);
    ack("fi.dn", undefined);
    ack("fi.fn", undefined);
    ack("fi.vflags", undefined);
    ack("fi.fflags", undefined);
    ack("fi.fmode", undefined);
    ack("fi.fstate", undefined);
    nack("fi.fdigest", "00000000000000000000000000000000");
    ack("fi.flink", undefined);
    ack("fi.fsize", undefined);
    ack("fi.frdev", undefined);
    ack("fi.fmtime", undefined);
    ack("fi.fuser", undefined);
    ack("fi.fgroup", undefined);
    ack("fi.fcolor", undefined);
    nack("fi.fclass", "symbolic link to `libpopt.so.0.0.0'");
}

delete mi;	// GCZeal?

// for (var [key,val] in Iterator(fi))
//     print(key+": "+val);

// print(JSON.stringify(fi));

if (loglvl) print("<-- Fi.js");

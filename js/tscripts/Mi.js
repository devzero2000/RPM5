if (loglvl) print("--> Mi.js");

var RPMDBI_PACKAGES = 0;
var RPMDBI_LABEL = 2;
var RPMTAG_NAME = 1000;
var RPMTAG_VERSION = 1001;
var RPMTAG_RELEASE = 1002;
var RPMTAG_ARCH = 1022;
var RPMTAG_NVRA = 1196;

var N = "popt";
var V = "";
var R = "";
var A = "";
var NVRA = N;
var hdrNum = 0;

var ts = new Ts();

var mi = new Mi(ts);
ack("typeof mi;", "object");
ack("mi instanceof Mi;", true);
ack("mi.debug = 1;", 1);
ack("mi.debug = 0;", 0);
delete mi;

// --- Iterate over packages, counting, grab hdrNum and NVRA on the fly.
var mi = new Mi(ts);
var bingo = 0;
var npkgs = 0;
for (var [dbkey,h] in Iterator(mi)) {
//    nack("h.instance", 0);
//    ack("h.instance", mi.instance);
    ack("mi.instance != 0", true);
    ack("mi.instance < 0x0000ffff", true);
    if (h[RPMTAG_NAME] == N) {
	hdrNum = mi.instance;
	V = h[RPMTAG_VERSION];
	R = h[RPMTAG_RELEASE];
	A = h[RPMTAG_ARCH];
	NVRA = h[RPMTAG_NVRA];
    }
    npkgs++;
    delete h;
    bingo = 1;
}
ack("bingo", 1);
nack("npkgs", 0);
ack("hdrNum != 0", true);
ack("hdrNum < 0x0000ffff", true);
nack("NVRA", N);
delete mi;

function doITER(ts, tag, key) {
    this.mi = new Mi(ts, tag, key);
    this.bingo = 0;
//  this.dbkey = null;
//  this.h = null;
    for ([this.dbkey,this.h] in Iterator(this.mi)) {
	ack("this.h.name", N);
	ack("this.h.nvra", NVRA);
	ack("this.mi.instance < 0x0000ffff", true);
	ack("this.mi.instance", hdrNum);
	delete this.h;
	this.bingo = 1;
    }
    delete this.mi;
    ack("this.bingo", 1);
    return true;
}

// --- Retrieve by package primary key
doITER(ts, RPMDBI_PACKAGES, hdrNum);

// --- Retrieve by N and NVRA string key(s).
doITER(ts, RPMTAG_NAME, N);
doITER(ts, RPMTAG_NVRA, NVRA);

// --- Retrieve by N, N-V, N-V-R, and N-V-R.A strings.
doITER(ts, RPMDBI_LABEL, N);
doITER(ts, RPMDBI_LABEL, N+"-"+V);
doITER(ts, RPMDBI_LABEL, N+"-"+V+"-"+R);
doITER(ts, RPMDBI_LABEL, N+"-"+V+"-"+R+"."+A);

// --- Retrieve by N pattern.
doITER(ts, RPMDBI_LABEL, "\^"+N+"-[0-9].*$");
doITER(ts, RPMDBI_LABEL, "\^"+N+"-[0-9].*"+"\."+A+"$");

// --- Retrieve by N with a mire pattern selector.
var mi = new Mi(ts, RPMTAG_NAME, N)
bingo = 0;
// XXX FIXME: rpmmi iterator w patterns is busted
// ack("mi.pattern(RPMTAG_VERSION, V)", true);
for (var [dbkey,h] in Iterator(mi)) {
    ack("h.name", N);
    ack("h.nvra", NVRA);
//  ack("h.instance", hdrNum);
    delete h;
    bingo = 1;
}
ack("bingo", 1);
delete mi;

delete ts;

if (loglvl) print("<-- Mi.js");

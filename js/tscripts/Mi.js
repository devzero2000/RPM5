if (loglvl) print("--> Mi.js");

var RPMDBI_PACKAGES = 0;
var RPMDBI_LABEL = 2;
var RPMTAG_NAME = 1000;
var RPMTAG_VERSION = 1001;
var RPMTAG_RELEASE = 1002;
var RPMTAG_GROUP = 1016;
var RPMTAG_OS = 1021;
var RPMTAG_ARCH = 1022;
var RPMTAG_NVRA = 1196;

var N = "popt";
var V = "";
var R = "";
var A = "";
var O = "";
var G = "";
var NVRA = N;
var hdrNum = 0;
var bingo = 0;
var npkgs = 0;

var ts = new Ts();

var mi = new Mi(ts);
ack("typeof mi;", "object");
ack("mi instanceof Mi;", true);
ack("mi.debug = 1;", 1);
ack("mi.debug = 0;", 0);
delete mi;

// --- Iterate over packages, counting, grab hdrNum and NVRA on the fly.
var mi = new Mi(ts);
bingo = 0;
for (var [dbkey,h] in Iterator(mi)) {
    ack("mi.instance != 0", true);
    ack("mi.instance < 0x0000ffff", true);
    ack("mi.instance == h.dbinstance", true);
    if (h.name == N) {
	hdrNum = mi.instance;
	V = h.version;
	R = h.release;
	O = h.os;
	A = h.arch;
	G = h.group;
	NVRA = h.nvra;
    }
    npkgs++;
    delete h;
    bingo = 1;
}
ack("bingo", 1);
delete mi;

function doITER(ts, tag, key) {
    this.mi = new Mi(ts, tag, key);
    this.bingo = 0;
    for ([this.dbkey,this.h] in Iterator(this.mi)) {
	ack("this.h.name", N);
	ack("this.h.nvra", NVRA);
	ack("this.mi.instance == hdrNum", true);
	ack("this.mi.instance == this.h.dbinstance", true);
	delete this.h;
	this.bingo = 1;
    }
    ack("this.bingo", 1);
    delete this.mi;
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

// --- Retrieve by N with a V mire pattern selector.
var mi = new Mi(ts, RPMTAG_NAME, N)
ack("mi.pattern(RPMTAG_VERSION, V)", true);
ack("mi.pattern(RPMTAG_RELEASE, R)", true);
ack("mi.pattern(RPMTAG_OS, O)", true);
ack("mi.pattern(RPMTAG_ARCH, A)", true);
ack("mi.pattern(RPMTAG_GROUP, G)", true);
bingo = 0;
for (var [dbkey,h] in Iterator(mi)) {
    ack("h.name", N);
    ack("h.nvra", NVRA);
    ack("mi.instance == hdrNum", true);
    ack("mi.instance == h.dbinstance", true);
    delete h;
    bingo = 1;
}
ack("bingo", 1);
delete mi;

delete ts;

if (loglvl) print("<-- Mi.js");

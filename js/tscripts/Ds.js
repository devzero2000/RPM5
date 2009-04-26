var ds = new Ds('uname');

print(ds)

print("ds.debug:\t", ds.debug);
ds.debug = 1;
print("ds.debug = 1:\t", ds.debug);

delete ds;

var cpuinfo = new Ds('cpuinfo');
var rpmlib = new Ds('rpmlib');
var getconf = new Ds('getconf');
var uname = new Ds('uname');
print(JSON.stringify(uname));

var RPMSENSE_ANY        = 0;
var RPMSENSE_LESS       = (1 << 1);
var RPMSENSE_GREATER    = (1 << 2);
var RPMSENSE_EQUAL      = (1 << 3);

var RPMSENSE_LE = (RPMSENSE_EQUAL|RPMSENSE_LESS);
var RPMSENSE_GE = (RPMSENSE_EQUAL|RPMSENSE_GREATER);

var single = new Ds(["N", "EVR", RPMSENSE_LE]);
print(JSON.stringify(single));

if (loglvl) print("--> Ds.js");

var ds = new Ds('rpmlib');
ack("typeof ds;", "object");
ack("ds instanceof Ds;", true);
ack("ds.debug = 1;", 1);
ack("ds.debug = 0;", 0);

ack("ds.length", 22);
ack("ds.type", "Provides");
ack("ds.buildtime = 1234;", 1234);
ack("ds.color = 4;", 4);
ack("ds.nopromote = 1;", 1);
ack("ds.nopromote = 0;", 0);

// for (let [key,val] in Iterator(ds))
//    print(key+": "+val);

// print(ds[0]);
// print(ds[0][0]);
// print(ds[0][1]);
// print(ds[0][2]);

// for (let i in ds)
//    print(i);

ack("ds.ix = -1;", -1);
ack("ds[-1] == undefined", true);
ack("ds.N == undefined", true);
// ack("ds.N", ds[-1][0]);
ack("ds.EVR == undefined", true);
// ack("ds.EVR", ds[-1][1]);
ack("ds.F == undefined", true);
// ack("ds.F", ds[-1][2]);
ack("ds.DNEVR == undefined", true);

ack("ds.ix++;", -1);
ack("ds[0] != undefined", true);
ack("ds.N", "BuiltinFiclScripts");
ack("ds.N == ds[0][0]", true);
ack("ds.EVR", "5.2-1");
ack("ds.EVR == ds[0][1]", true);
ack("ds.F", 16777224);
ack("ds.F == ds[0][2]", true);
ack("ds.DNEVR", "P rpmlib(BuiltinFiclScripts) = 5.2-1");

ack("++ds.ix;", 1);
ack("ds[1] != undefined", true);
ack("ds.N", "BuiltinJavaScript");
ack("ds.N == ds[1][0]", true);
ack("ds.EVR", "5.2-1");
ack("ds.EVR == ds[1][1]", true);
ack("ds.F", 16777224);
ack("ds.F == ds[1][2]", true);
ack("ds.DNEVR", "P rpmlib(BuiltinJavaScript) = 5.2-1");

if (loglvl) print("<-- Ds.js");

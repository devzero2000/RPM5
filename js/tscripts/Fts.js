if (loglvl) print("--> Fts.js");

var dn = "./tscripts";
var ix = 0;

var fts = new Fts(dn);
ack("typeof fts;", "object");
ack("fts instanceof Fts;", true);
// ack("fts.debug = 1;", 1);
// ack("fts.debug = 0;", 0);

print(JSON.stringify(fts));

ack("fts();", fts);		// XXX Fts_close() if no arguments given
ack("fts(dn);", fts);

ix = 0;
ack("fts.length", ix);		// XXX ensure undefined or 0 until iterated
for (var [key,val] in Iterator(fts)) {
    print("key: "+key, "val: "+val);
    ix++;
}
ack("fts.length", ix);

ack("fts(dn);", fts);

ix = 0;
ack("fts.length", ix);		// XXX ensure undefined or 0 until iterated
for (var [key,val] in Iterator(fts)) {
    print("key: "+key, "val: "+val);
    ix++;
}
ack("fts.length", ix);

if (loglvl) print("<-- Fts.js");

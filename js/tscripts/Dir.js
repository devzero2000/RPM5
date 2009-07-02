if (loglvl) print("--> Dir.js");

var dn = "./tscripts";
var ix = 0;

var dir = new Dir(dn);
ack("typeof dir;", "object");
ack("dir instanceof Dir;", true);
ack("dir.debug = 1;", 1);
ack("dir.debug = 0;", 0);

print(JSON.stringify(dir));

ack("dir();", dir);		// XXX closedir if no arguments given
ack("dir(dn);", dir);

ix = 0;
ack("dir.length", ix);		// XXX ensure undefined or 0 until iterated
for (var [key,val] in Iterator(dir)) {
    print("key: "+key, "val: "+val);
    ix++;
}
ack("dir.length", ix);

ack("dir(dn);", dir);

ix = 0;
ack("dir.length", ix);		// XXX ensure undefined or 0 until iterated
for (var [key,val] in Iterator(dir)) {
    print("key: "+key, "val: "+val);
    ix++;
}
ack("dir.length", ix);

if (loglvl) print("<-- Dir.js");

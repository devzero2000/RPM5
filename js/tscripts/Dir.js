if (loglvl) print("--> Dir.js");

var dn = "./tscripts";
var ix = 0;

var dir = new Dir(dn);
ack("typeof dir;", "object");
ack("dir instanceof Dir;", true);
ack("dir.debug = 1;", 1);
ack("dir.debug = 0;", 0);

// XXX must be immediately after ctor
print(JSON.stringify(dir));

ack("dir();", dir);		// XXX closedir
ack("dir(dn);", dir);		// XXX not quite same as ctor yet
dir = new Dir(dn);

ix = 0;
ack("dir.length", ix);		// XXX undefined until iterated
for (var [key,val] in Iterator(dir)) {
    print("key: "+key, "val: "+val);
    ix++;
}
ack("dir.length", ix);

ack("dir(dn);", dir);		// XXX not quite same as ctor yet
dir = new Dir(dn);

ix = 0;
ack("dir.length", ix);		// XXX undefined until iterated
for (var [key,val] in Iterator(dir)) {
    print("key: "+key, "val: "+val);
    ix++;
}
ack("dir.length", ix);

if (loglvl) print("<-- Dir.js");

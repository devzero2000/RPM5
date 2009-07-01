if (loglvl) print("--> Dir.js");

var dir = new Dir("./tscripts");
ack("typeof dir;", "object");
ack("dir instanceof Dir;", true);
ack("dir.debug = 1;", 1);
ack("dir.debug = 0;", 0);

// XXX needs JSClass.call populated
// for (var [key,val] in Dir("./tscripts")) {
//     print("key: "+key, "val: "+val);
// }

print(JSON.stringify(dir));

var dir = new Dir("./tscripts");
print("length: "+dir.length);
for (var [key,val] in Iterator(dir)) {
    print("key: "+key, "val: "+val);
}
print("length: "+dir.length);

if (loglvl) print("<-- Dir.js");

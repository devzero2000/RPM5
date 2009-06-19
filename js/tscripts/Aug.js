if (loglvl) print("--> Aug.js");

var aug = new Aug();
ack("typeof aug;", "object");
ack("aug instanceof Aug;", true);
// ack("aug = 1;", 1);
// ack("aug = 0;", 0);

var passwd = "/files/etc/passwd";
var rpc = passwd+"/rpc";
var rpcname = rpc+"/name";
ack("aug.print(rpc)", true);
ack("aug.get(rpcname)", "Rpcbind Daemon");
ack("aug.set(rpcname, 'Yadda Yadda')", "Yadda Yadda");
ack("aug.get(rpcname)", "Yadda Yadda");

var wild = passwd+"/rpc/*";
ack("aug.match(wild)", '/files/etc/passwd/rpc/password,/files/etc/passwd/rpc/uid,/files/etc/passwd/rpc/gid,/files/etc/passwd/rpc/name,/files/etc/passwd/rpc/home,/files/etc/passwd/rpc/shell');

var newname = "newname";
var rpcnewname = rpc+"/"+newname;
ack("aug.insert(rpcname, newname, 0)", true);
ack("aug.set(rpcnewname, 'George')", "George");
ack("aug.get(rpcnewname)", "George");

var newrpc = passwd+"/newrpc";
var newrpcnewname = newrpc+"/"+newname;
ack("aug.mv(rpc, newrpc)", true);
ack("aug.get(newrpcnewname)", "George");
ack("aug.rm(newrpc)", true);

if (loglvl) print("<-- Aug.js");

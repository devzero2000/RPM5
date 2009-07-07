if (loglvl) print("--> St.js");

var st = new St('.');
ack("typeof st;", "object");
ack("st instanceof St;", true);
ack("st.debug = 1;", 1);
ack("st.debug = 0;", 0);

st();

var dndot = '.';
var dnroot = '/';
var dnfurl = 'file:///';

st(dndot);
print("    dev: 0x" + st.dev.toString(16));
print("    ino: " + st.ino);
print("   mode: 0" + st.mode.toString(8));
print("  nlink: " + st.nlink);
print("    uid: " + st.uid);
print("    gid: " + st.gid);
print("   rdev: 0x" + st.rdev.toString(16));
print("   size: " + st.size);
print("blksize: " + st.blksize);
print(" blocks: " + st.blocks);
print("  atime: " + st.atime);
print("  mtime: " + st.mtime);
print("  ctime: " + st.ctime);

// XXX needs enumerate/resolve or custom st.toJSON()
// print(JSON.stringify(st));

// var st = new St('http://rpm5.org/files/popt/popt-1.14.tar.gz');
var rst = new St(dnroot);
var fst = new St(dnfurl);
ack("rst.dev", fst.dev);
ack("rst.ino", fst.ino);
ack("rst.mode", fst.mode);
ack("rst.nlink", fst.nlink);
ack("rst.uid", fst.uid);
ack("rst.gid", fst.gid);
ack("rst.rdev", fst.rdev);
ack("rst.size", fst.size);
ack("rst.blksize", fst.blksize);
ack("rst.blocks", fst.blocks);

// grrr
ack("rst.atime", fst.atime);
ack("rst.mtime", fst.mtime);
ack("rst.ctime", fst.ctime);

if (loglvl) print("<-- St.js");

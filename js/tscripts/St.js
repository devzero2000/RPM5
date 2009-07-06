if (loglvl) print("--> St.js");

var st = new St('.');
ack("typeof st;", "object");
ack("st instanceof St;", true);
ack("st.debug = 1;", 1);
ack("st.debug = 0;", 0);

st();

st('.');
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

var st = new St('/');
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

// var st = new St('http://rpm5.org/files/popt/popt-1.14.tar.gz');
var st = new St('file:///');
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

if (loglvl) print("<-- St.js");

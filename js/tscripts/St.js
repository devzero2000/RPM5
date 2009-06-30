if (loglvl) print("--> St.js");

var st = new St('.');
ack("typeof st;", "object");
ack("st instanceof St;", true);
ack("st.debug = 1;", 1);
ack("st.debug = 0;", 0);

print("    dev: " + st.dev);
print("    ino: " + st.ino);
print("   mode: " + st.mode);
print("  nlink: " + st.nlink);
print("    uid: " + st.uid);
print("    gid: " + st.gid);
print("   rdev: " + st.rdev);
print("   size: " + st.size);
print("blksize: " + st.blksize);
print(" blocks: " + st.blocks);
print("  atime: " + st.atime);
print("  mtime: " + st.mtime);
print("  ctime: " + st.ctime);

var st = new St('file:///');
print("    dev: " + st.dev);
print("    ino: " + st.ino);
print("   mode: " + st.mode);
print("  nlink: " + st.nlink);
print("    uid: " + st.uid);
print("    gid: " + st.gid);
print("   rdev: " + st.rdev);
print("   size: " + st.size);
print("blksize: " + st.blksize);
print(" blocks: " + st.blocks);
print("  atime: " + st.atime);
print("  mtime: " + st.mtime);
print("  ctime: " + st.ctime);

var st = new St('http://rpm5.org/files/popt/popt-1.14.tar.gz');
print("    dev: " + st.dev);
print("    ino: " + st.ino);
print("   mode: " + st.mode);
print("  nlink: " + st.nlink);
print("    uid: " + st.uid);
print("    gid: " + st.gid);
print("   rdev: " + st.rdev);
print("   size: " + st.size);
print("blksize: " + st.blksize);
print(" blocks: " + st.blocks);
print("  atime: " + st.atime);
print("  mtime: " + st.mtime);
print("  ctime: " + st.ctime);

// XXX needs enumerate/resolve or custom st.toJSON()
// print(JSON.stringify(st));

if (loglvl) print("<-- St.js");

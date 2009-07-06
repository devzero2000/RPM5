if (loglvl) print("--> Sys.js");

const R_OK	= 4;		/* Test for read permission.  */
const W_OK	= 2;		/* Test for write permission.  */
const X_OK	= 1;		/* Test for execute permission.  */
const F_OK	= 0;		/* Test for existence.  */

var sys = new Sys();
ack("typeof sys;", "object");
ack("sys instanceof Sys;", true);
ack("sys.debug = 1;", 1);
ack("sys.debug = 0;", 0);

var tmpdir = "/tmp";
var dn = tmpdir + "/sys.d";
var fn = tmpdir + "/sys.file";
var lfn = tmpdir + "/sys.link";
var rfn = tmpdir + "/sys.rename";
var sfn = tmpdir + "/sys.symlink";

nack("sys.access(dn, F_OK);", 0);
ack("sys.mkdir(dn);", 0);
ack("sys.access(dn, F_OK);", 0);
ack("sys.rmdir(dn);", 0);

var st;

ack("sys.creat(fn,0644);", 0);
ack("(st = sys.stat(fn)) instanceof St;", true);
ack("st.uid", sys.uid);
ack("st.gid", sys.gid);
ack("st.mode", 0100644);
ack("sys.chmod(fn,0640);", 0);
ack("(st = sys.lstat(fn)) instanceof St;", true);
ack("st.mode", 0100640);
ack("sys.chown(fn,-1,-1);", 0);
ack("sys.chown(fn,sys.uid,-1);", 0);
ack("sys.chown(fn,-1,sys.gid);", 0);
ack("sys.chown(fn,sys.uid,sys.gid);", 0);
ack("(st = sys.lstat(fn)) instanceof St;", true);
ack("st.uid", sys.uid);
ack("st.gid", sys.gid);

ack("sys.link(fn,lfn);", 0);
ack("sys.symlink(lfn,sfn);", 0);
ack("(st = sys.stat(sfn)) instanceof St;", true);
ack("st.mode", 0100640);
ack("(st = sys.lstat(sfn)) instanceof St;", true);
ack("st.mode", 0120777);
ack("sys.readlink(sfn);", lfn);
ack("sys.rename(lfn,rfn);", 0);

ack("sys.unlink(fn);", 0);
nack("sys.unlink(lfn);", 0);
ack("sys.unlink(rfn);", 0);
ack("sys.unlink(sfn);", 0);

if (loglvl) print("<-- Sys.js");

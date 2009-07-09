if (loglvl) print("--> Xar.js");

var fn = "popt-1.14.xar";
var path = null;
var st = null;
var ix = 0;

var xar = new Xar(fn, "r");
ack("typeof xar;", "object");
ack("xar instanceof Xar;", true);
ack("xar.debug = 1;", 1);
ack("xar.debug = 0;", 0);

ix = 0;
ack("xar.length", ix);		// XXX ensure undefined or 0 until iterated
for (var [key,val] in Iterator(xar)) {
    path = val[0];
    st = val[1];
    print("key: "+key, "val: "+path);
    print('\t0'+st.mode.toString(8)+' '+st.uid+'/'+st.gid+' '+st.size+' '+st.mtime);
    ix++;
}
ack("xar.length", ix);

if (loglvl) print("<-- Xar.js");

if (loglvl) print("--> Mpw.js");

var mpw = new Mpw();
ack("typeof mpw;", "object");
ack("mpw instanceof Mpw;", true);
// ack("mpw.debug = 1;", 1);
// ack("mpw.debug = 0;", 0);

// ===== Basic tests
var wa = mpw('0000000987654321');
ack('wa.toString(16)', '0x987654321');
ack('wa.toString(10)', '40926266145')
ack('wa.toString(8)', '0460731241441')
ack('wa.toString(2)', '2#100110000111011001010100001100100001')

var wb = mpw(0x10);
ack('wb.toString(16)', '0x10');
var wc = mpw('0fedcba000000000');
ack('wc.toString(16)', '0xfedcba000000000');

var za = 0x0000000007654321;
var wa = mpw(za);
var zb = 0x0000000000000010;
var wb = mpw(zb);
var zc = 0;
zc = za + zb;
ack('mpw.__add__(wa, wb).toString(10)', zc.toString(10));
zc = za - zb;
ack('mpw.__sub__(wa, wb).toString(10)', zc.toString(10));
zc = za * zb;
ack('mpw.__mul__(wa, wb).toString(10)', zc.toString(10));
zc = za / zb;
ack('mpw.__div__(wa, wb).toString(10)', Math.floor(zc).toString(10));
zc = za % zb;
ack('mpw.__mod__(wa, wb).toString(10)', zc.toString(10));
zc = Math.pow(za, zb);
nack('mpw.__pow__(wa, wb).toString(10)', zc.toString(10));
zc = za << zb;
ack('mpw.__lshift__(wa, wb).toString(10)', zc.toString(10));
zc = za >> zb;
ack('mpw.__rshift__(wa, wb).toString(10)', zc.toString(10));
zc = za & zb;
ack('mpw.__and__(wa, wb).toString(10)', zc.toString(10));
zc = za ^ zb;
ack('mpw.__xor__(wa, wb).toString(10)', zc.toString(10));
zc = za | zb;
ack('mpw.__or__(wa, wb).toString(10)', zc.toString(10));

var m = mpw(15);
ack('m.toString(10)', '15');
var x = mpw(7);
ack('x.toString(10)', '7');
var y = mpw(13);
ack('y.toString(10)', '13');

// FIXME
// ack('mpw.invm(x, m).toString(10)', '4');

ack('mpw.sqrm(x, m).toString(10)', '4');
ack('mpw.addm(x, y, m).toString(10)', '5');
ack('mpw.subm(x, y, m).toString(10)', '9');
ack('mpw.mulm(x, y, m).toString(10)', '1');
ack('mpw.powm(x, y, m).toString(10)', '7');

var zx = 2*3*5*19;
var zy = 7*11*13*19;
x = mpw(zx);
ack('x.toString(10)', zx.toString(10));
y = mpw(zy);
ack('y.toString(10)', zy.toString(10));

ack('mpw.gcd(x, y).toString(10)', '19');

y = mpw();
ack('x.__nonzero__()', true);
ack('y.__nonzero__()', false);
x = x.__neg__();
zx = -zx;
ack('x.toString(10)', zx.toString(10));
x = x.__abs__();
zx = Math.abs(zx);
ack('x.toString(10)', zx.toString(10));
x = x.__pos__();
zx = +zx;
ack('x.toString(10)', zx.toString(10));
x = x.__invert__();
zx = ~zx;
ack('x.toString(10)', zx.toString(10));

// ===== Carry/borrow tests
var a = 0x7fffffff;
wa = mpw(za);	wa = mpw.__add__(wa, wa);
var za = -a;	za = za + za;
wb = mpw(1);
wb = wb.__neg__();	
var zb = -1;
wc = mpw(1);
var zc = 1;
wd = mpw(a);	wd = mpw.__add__(wd, wd);
var zd = a;	zd = zd + zd;

x = mpw.__add__(wa, wa);
zx = za + za;
ack('x.toString(10)', zx.toString(10));
x = mpw.__add__(wb, wd);
zx = zb + zd;
ack('x.toString(10)', zx.toString(10));
x = mpw.__add__(wc, wa);
zx = zc + za;
ack('x.toString(10)', zx.toString(10));
x = mpw.__add__(wd, wd);
zx = zd + zd;
ack('x.toString(10)', zx.toString(10));

x = mpw.__add__(wb, wa);
zx = zb - za;
ack('x.toString(10)', zx.toString(10));
x = mpw.__add__(wc, wd);
zx = zc - zd;
ack('x.toString(10)', zx.toString(10));

// ===== Signs
var wpa = mpw(13);
var wma = wpa.__neg__();
var wpb = mpw.__sub__(wpa, mpw(3));	// FIXME: coerce args
var wmb = wpb.__neg__();
var zpa = 13;
var zma = -zpa;
var zpb = zpa - 3;
var zmb = -zpb;

x = mpw.__add__(wma, wmb);
zx = zma + zmb;
ack('x.toString(10)', zx.toString(10));
x = mpw.__add__(wma, wpb);
zx = zma + zpb;
ack('x.toString(10)', zx.toString(10));
x = mpw.__add__(wpa, wmb);
zx = zpa + zmb;
ack('x.toString(10)', zx.toString(10));
x = mpw.__add__(wpa, wpb);
zx = zpa + zpb;
ack('x.toString(10)', zx.toString(10));

x = mpw.__sub__(wma, wmb);
zx = zma - zmb;
ack('x.toString(10)', zx.toString(10));
x = mpw.__sub__(wma, wpb);
zx = zma - zpb;
ack('x.toString(10)', zx.toString(10));
x = mpw.__sub__(wpa, wmb);
zx = zpa - zmb;
ack('x.toString(10)', zx.toString(10));
x = mpw.__sub__(wpa, wpb);
zx = zpa - zpb;
ack('x.toString(10)', zx.toString(10));
x = mpw.__sub__(wmb, wma);
zx = zmb - zma;
ack('x.toString(10)', zx.toString(10));
x = mpw.__sub__(wmb, wpa);
zx = zmb - zpa;
ack('x.toString(10)', zx.toString(10));
x = mpw.__sub__(wpb, wma);
zx = zpb - zma;
ack('x.toString(10)', zx.toString(10));
x = mpw.__sub__(wpb, wpa);
zx = zpb - zpa;
ack('x.toString(10)', zx.toString(10));
 
x = mpw.__mul__(wma, wmb);
zx = zma * zmb;
ack('x.toString(10)', zx.toString(10));
x = mpw.__mul__(wma, wpb);
zx = zma * zpb;
ack('x.toString(10)', zx.toString(10));
x = mpw.__mul__(wpa, wmb);
zx = zpa * zmb;
ack('x.toString(10)', zx.toString(10));
x = mpw.__mul__(wpa, wpb);
zx = zpa * zpb;
ack('x.toString(10)', zx.toString(10));
 
x = mpw.__div__(wma, wmb);
zx = zma / zmb;
ack('x.toString(10)', Math.floor(zx).toString(10));
x = mpw.__div__(wma, wpb);
zx = zma / zpb;
ack('x.toString(10)', Math.floor(zx).toString(10));
x = mpw.__div__(wpa, wmb);
zx = zpa / zmb;
ack('x.toString(10)', Math.floor(zx).toString(10));
x = mpw.__div__(wpa, wpb);
zx = zpa / zpb;
ack('x.toString(10)', Math.floor(zx).toString(10));
x = mpw.__div__(wmb, wma);
zx = zmb / zma;
ack('x.toString(10)', Math.floor(zx).toString(10));
x = mpw.__div__(wmb, wpa);
zx = zmb / zpa;
ack('x.toString(10)', Math.floor(zx).toString(10));
x = mpw.__div__(wpb, wma);
zx = zpb / zma;
ack('x.toString(10)', Math.floor(zx).toString(10));
x = mpw.__div__(wpb, wpa);
zx = zpb / zpa;
ack('x.toString(10)', Math.floor(zx).toString(10));
 
x = mpw.__pow__(wma, wmb);
zx = Math.pow(zma, zmb);
ack('x.toString(10)', zx.toString(10));
x = mpw.__pow__(wma, wpb);
zx = Math.pow(zma, zpb);
ack('x.toString(10)', zx.toString(10));
x = mpw.__pow__(wpa, wmb);
zx = Math.pow(zpa, zmb);
ack('x.toString(10)', zx.toString(10));
x = mpw.__pow__(wpa, wpb);
zx = Math.pow(zpa, zpb);
ack('x.toString(10)', zx.toString(10));
x = mpw.__pow__(wmb, wma);
zx = Math.pow(zmb, zma);
ack('x.toString(10)', zx.toString(10));
x = mpw.__pow__(wmb, wpa);
zx = Math.pow(zmb, zpa);
ack('x.toString(10)', zx.toString(10));
x = mpw.__pow__(wpb, wma);
zx = Math.pow(zpb, zma);
ack('x.toString(10)', zx.toString(10));
x = mpw.__pow__(wpb, wpa);
zx = Math.pow(zpb, zpa);
ack('x.toString(10)', zx.toString(10));
 
x = mpw.__mod__(wma, wmb);
zx = zma % zmb;
ack('x.toString(10)', zx.toString(10));
x = mpw.__mod__(wma, wpb);
zx = zma % zpb;
ack('x.toString(10)', zx.toString(10));
x = mpw.__mod__(wpa, wmb);
zx = zpa % zmb;
ack('x.toString(10)', zx.toString(10));
x = mpw.__mod__(wpa, wpb);
zx = zpa % zpb;
ack('x.toString(10)', zx.toString(10));
x = mpw.__mod__(wmb, wma);
zx = zmb % zma;
ack('x.toString(10)', zx.toString(10));
x = mpw.__mod__(wmb, wpa);
zx = zmb % zpa;
ack('x.toString(10)', zx.toString(10));
x = mpw.__mod__(wpb, wma);
zx = zpb % zma;
ack('x.toString(10)', zx.toString(10));
x = mpw.__mod__(wpb, wpa);
zx = zpb % zpa;
ack('x.toString(10)', zx.toString(10));

// ===== Knuth poly
var w1 = mpw(1);
var lo = 2;
var wm = mpw(lo);
var hi = 15;
var wn = mpw(hi);

// var bases = [3, 5, 7, 8, 10, 11, 13, 16];
var bases = [13];

for (var i in bases) {
    var t = bases[i];
    var wt = mpw(t);
    var tm1 = (t - 1);
    var tm2 = (t - 2);
    print("=====\t("+t+"**m - 1) * ("+t+"**n - 1), m,n in ["+lo+","+hi+")");
    for (var m = lo; m < hi; m++) {
        wm = mpw(m);
        wb = mpw.__pow__(wt, wm);
        wb = mpw.__sub__(wb, w1);
	for (var n = m+1; n < hi+1; n++) {
	    wn = mpw(n);
	    wc = mpw.__pow__(wt, wn);
	    wc = mpw.__sub__(wc, w1);
	    wa = mpw.__mul__(wb, wc);
	    print(wa.toString(t));
//	    zs = tm1 * (m - 1) + tm2 + tm1 * (n - m) + "0" * (m - 1) + "1"
//	    if ws != zs:
//		print "(%d**%d - 1) * (%d**%d - 1)\t%s" % (self.t,m,self.t,n,ws)
//		assert ws == zs
	}
    }
}

if (loglvl) print("<-- Mpw.js");

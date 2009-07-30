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

ack('mpw(wa, wb, "+").toString(10)', (za + zb).toString(10));
ack('mpw(wa, wb, "-").toString(10)', (za - zb).toString(10));
ack('mpw(wa, wb, "*").toString(10)', (za * zb).toString(10));
ack('mpw(wa, wb, "/").toString(10)', Math.floor(za / zb).toString(10));
ack('mpw(wa, wb, "%").toString(10)', (za % zb).toString(10));
ack('mpw(wa, wb, "**").toString(10)', Math.pow(za, zb).toString(10));
ack('mpw(wa, wb, "<<").toString(10)', (za << zb).toString(10));
ack('mpw(wa, wb, ">>").toString(10)', (za >> zb).toString(10));
ack('mpw(wa, wb, "&").toString(10)', (za & zb).toString(10));
ack('mpw(wa, wb, "^").toString(10)', (za ^ zb).toString(10));
ack('mpw(wa, wb, "|").toString(10)', (za | zb).toString(10));

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
ack('mpw.gcd(mpw(zx), mpw(zy)).toString(10)', '19');

// y = mpw(0);
// ack('x.__nonzero__()', true);
// ack('y.__nonzero__()', false);
// x = x.__neg__();
// zx = -zx;
// ack('x.toString(10)', zx.toString(10));
// x = x.__abs__();
// zx = Math.abs(zx);
// ack('x.toString(10)', zx.toString(10));
// x = x.__pos__();
// zx = +zx;
// ack('x.toString(10)', zx.toString(10));
// x = x.__invert__();
// zx = ~zx;
// ack('x.toString(10)', zx.toString(10));

// ===== Carry/borrow tests
var a = 0x7fffffff;
wa = mpw(mpw(-a), mpw(-a), "+");
var za = (-a) + (-a);
wb = mpw(-1);
var zb = -1;
wc = mpw(1);
var zc = 1;
wd = mpw(mpw(a), mpw(a), "+");
var zd = a + a;

ack('mpw(wa, wa, "+").toString(10)', (za + za).toString(10));
ack('mpw(wb, wd, "+").toString(10)', (zb + zd).toString(10));
ack('mpw(wc, wa, "+").toString(10)', (zc + za).toString(10));
ack('mpw(wd, wd, "+").toString(10)', (zd + zd).toString(10));
 
ack('mpw(wb, wa, "-").toString(10)', (zb - za).toString(10));
ack('mpw(wc, wd, "-").toString(10)', (zc - zd).toString(10));

// ===== Signs
var wpa = mpw(13);
var wma = mpw(-13);
var wpb = mpw(wpa, mpw(3), "-");	// FIXME: coerce args
var wmb = mpw(-10);
var zpa = 13;
var zma = -zpa;
var zpb = zpa - 3;
var zmb = -zpb;

ack('mpw(wma, wmb, "+").toString(10)', (zma + zmb).toString(10));
ack('mpw(wma, wpb, "+").toString(10)', (zma + zpb).toString(10));
ack('mpw(wpa, wmb, "+").toString(10)', (zpa + zmb).toString(10));
ack('mpw(wpa, wpb, "+").toString(10)', (zpa + zpb).toString(10));

ack('mpw(wma, wmb, "-").toString(10)', (zma - zmb).toString(10));
ack('mpw(wma, wpb, "-").toString(10)', (zma - zpb).toString(10));
ack('mpw(wpa, wmb, "-").toString(10)', (zpa - zmb).toString(10));
ack('mpw(wpa, wpb, "-").toString(10)', (zpa - zpb).toString(10));
ack('mpw(wmb, wma, "-").toString(10)', (zmb - zma).toString(10));
ack('mpw(wmb, wpa, "-").toString(10)', (zmb - zpa).toString(10));
ack('mpw(wpb, wma, "-").toString(10)', (zpb - zma).toString(10));
ack('mpw(wpb, wpa, "-").toString(10)', (zpb - zpa).toString(10));
 
ack('mpw(wma, wmb, "*").toString(10)', (zma * zmb).toString(10));
ack('mpw(wma, wpb, "*").toString(10)', (zma * zpb).toString(10));
ack('mpw(wpa, wmb, "*").toString(10)', (zpa * zmb).toString(10));
ack('mpw(wpa, wpb, "*").toString(10)', (zpa * zpb).toString(10));
 
ack('mpw(wma, wmb, "/").toString(10)', Math.floor(zma/zmb).toString(10));
ack('mpw(wma, wpb, "/").toString(10)', Math.floor(zma/zpb).toString(10));
ack('mpw(wpa, wmb, "/").toString(10)', Math.floor(zpa/zmb).toString(10));
ack('mpw(wpa, wpb, "/").toString(10)', Math.floor(zpa/zpb).toString(10));
ack('mpw(wmb, wma, "/").toString(10)', Math.floor(zmb/zma).toString(10));
ack('mpw(wmb, wpa, "/").toString(10)', Math.floor(zmb/zpa).toString(10));
ack('mpw(wpb, wma, "/").toString(10)', Math.floor(zpb/zma).toString(10));
ack('mpw(wpb, wpa, "/").toString(10)', Math.floor(zpb/zpa).toString(10));
 
ack('mpw(wma, wmb, "**").toString(10)', Math.pow(zma, zmb).toString(10));
ack('mpw(wma, wpb, "**").toString(10)', Math.pow(zma, zpb).toString(10));
ack('mpw(wpa, wmb, "**").toString(10)', Math.pow(zpa, zmb).toString(10));
ack('mpw(wpa, wpb, "**").toString(10)', Math.pow(zpa, zpb).toString(10));
ack('mpw(wmb, wma, "**").toString(10)', Math.pow(zmb, zma).toString(10));
ack('mpw(wmb, wpa, "**").toString(10)', Math.pow(zmb, zpa).toString(10));
ack('mpw(wpb, wma, "**").toString(10)', Math.pow(zpb, zma).toString(10));
ack('mpw(wpb, wpa, "**").toString(10)', Math.pow(zpb, zpa).toString(10));
 
ack('mpw(wma, wmb, "%").toString(10)', (zma % zmb).toString(10));
ack('mpw(wma, wpb, "%").toString(10)', (zma % zpb).toString(10));
ack('mpw(wpa, wmb, "%").toString(10)', (zpa % zmb).toString(10));
ack('mpw(wpa, wpb, "%").toString(10)', (zpa % zpb).toString(10));
ack('mpw(wmb, wma, "%").toString(10)', (zmb % zma).toString(10));
ack('mpw(wmb, wpa, "%").toString(10)', (zmb % zpa).toString(10));
ack('mpw(wpb, wma, "%").toString(10)', (zpb % zma).toString(10));
ack('mpw(wpb, wpa, "%").toString(10)', (zpb % zpa).toString(10));

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
	wb = mpw(wt, wm, "**");
	wb = mpw(wb, w1, "-");
	for (var n = m+1; n < hi+1; n++) {
	    wn = mpw(n);
	    wc = mpw(wt, wn, "**");
	    wc = mpw(wc, w1, "-");
	    wa = mpw(wb, wc, "*");
	    print(wa.toString(t));
//	    zs = tm1 * (m - 1) + tm2 + tm1 * (n - m) + "0" * (m - 1) + "1"
//	    if ws != zs:
//		print "(%d**%d - 1) * (%d**%d - 1)\t%s" % (self.t,m,self.t,n,ws)
//		assert ws == zs
	}
    }
}

if (loglvl) print("<-- Mpw.js");

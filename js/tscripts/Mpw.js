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

ack('mpw.invm(x, m).toString(10)', '4');

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

// ===== RSA example (from "Handbook of Applied Cryptography" 11.20 p434).
// Keygen RSA
var p = mpw(7927);
var q = mpw(6997);
var n = mpw(p, q, "*");
ack('n.toString(10)', '55465219');
var phi = mpw(p, mpw(1), "-", q, mpw(1), "-", "*");
ack('phi.toString(10)', '55450296');
var e = mpw(5);
ack('e.toString(10)', '5');
// print("gcd(e, phi) = "+mpw.gcd(e, phi));
ack('mpw.gcd(e, phi).toString(10)', '1');
var d = mpw.invm(e, phi);
// print("mulm(e, d, phi) = "+mpw.mulm(e, d, phi));
ack('mpw.mulm(e, d, phi).toString(10)', '1');
ack('d.toString(10)', '44360237');

// print("n = "+n.toString(10) + " e = "+e.toString(10) + " d = "+d.toString(10));

// Sign RSA
var m = mpw(31229978);
var s = mpw.powm(m, d, n);
ack('s.toString(10)', '30729435');

// Verify RSA
var mtwiddle = mpw.powm(s, e, n);
ack('mtwiddle.toString(10)', m.toString(10));

// ===== DSA example (from "Handbook of Applied Cryptography" 11.57 p453).
// Keygen DSA
var p = mpw(124540019);
var q = mpw(17389);
ack('mpw(p, mpw(1), "-", q, "%").toString(10)', '0');
var pdivq = mpw(p, mpw(1), "-", q, "/");
ack('pdivq.toString(10)', '7162');
var g = mpw(110217528);
var alpha = mpw.powm(g, pdivq, p);
ack('alpha.toString(10)', '10083255');
var a = mpw(12496);
var y = mpw.powm(alpha, a, p);
ack('y.toString(10)', '119946265');

// Sign DSA
var k = mpw(9557);
var r = mpw(mpw.powm(alpha, k, p), q, "%");
ack('r.toString(10)', '34');
var kinv = mpw.invm(k, q);
ack('kinv.toString(10)', '7631');
var hm = mpw(5246);
var s = mpw.mulm(kinv, mpw(hm, a, r, "*", "+"), q);
ack('s.toString(10)', '13049');

// Verify DSA
var w = mpw.invm(s, q);
ack('w.toString(10)', '1799');
var u1 = mpw.mulm(w, hm, q);
ack('u1.toString(10)', '12716');
var u2 = mpw.mulm(r, w, q);
ack('u2.toString(10)', '8999');
var v1 = mpw.powm(alpha, u1, p);
var v2 = mpw.powm(y, u2, p);
var v3 = mpw.mulm(v1, v2, p);
ack('v3.toString(10)', '27039929');
var v = mpw(v3, q, "%");
ack('v.toString(10)', r.toString(10));

// ===== ElGamal example (from "Handbook of Applied Cryptography" 11.65 p455).
// Keygen ElGamal
var p = mpw(2357);
var alpha = mpw(2);
var a = mpw(1751);
var y = mpw.powm(alpha, a, p);
ack('y.toString(10)', '1185');

// Sign ElGamal
var hm = mpw(1463);
var k = mpw(1529);
var r = mpw.powm(alpha, k, p);
ack('r.toString(10)', '1490');
var pm1 = mpw(p, mpw(1), "-");
var kinv = mpw.invm(k, pm1);
ack('kinv.toString(10)', '245');
var s = mpw.mulm(kinv, mpw.subm(hm, mpw.mulm(a, r, pm1), pm1), pm1);
ack('s.toString(10)', '1777');

// Verify ElGamal
var ytor = mpw.powm(y, r, p);
var rtos = mpw.powm(r, s, p);
var v1 = mpw.mulm(ytor, rtos, p);
ack('v1.toString(10)', '1072');
var v2 = mpw.powm(alpha, hm, p);
ack('v2.toString(10)', v1.toString(10));

if (loglvl) print("<-- Mpw.js");

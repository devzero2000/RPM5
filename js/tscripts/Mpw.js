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
ack('m.toString(10)', m.valueOf());
var x = mpw(7);
ack('x.toString(10)', '7');
ack('x.toString(10)', x.valueOf());
var y = mpw(13);
ack('y.toString(10)', '13');
ack('y.toString(10)', y.valueOf());

ack('mpw(x, m, "invm").toString(10)', '13');
ack('mpw(x, m, "sqrm").toString(10)', '4');

ack('mpw(x, y, m, "addm").toString(10)', '5');
ack('mpw(x, y, m, "subm").toString(10)', '9');
ack('mpw(x, y, m, "mulm").toString(10)', '1');
ack('mpw(x, y, m, "powm").toString(10)', '7');
 
var zx = 2*3*5*19;
var zy = 7*11*13*19;
ack('mpw(zx, zy, "gcd").toString(10)', '19');

x = mpw(zx, "neg");
zx = -zx;
ack('x.toString(10)', zx.toString(10));
x = mpw(zx, "abs");
zx = Math.abs(zx);
ack('x.toString(10)', zx.toString(10));
x = mpw(zx, "~");
zx = ~zx;
ack('x.toString(10)', zx.toString(10));

// ===== Carry/borrow tests
var a = 0x7fffffff;
wa = mpw(-a, -a, "+");
var za = (-a) + (-a);
wb = mpw(-1);
var zb = -1;
wc = mpw(1);
var zc = 1;
wd = mpw(a, a, "+");
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
var wpb = mpw(wpa, 3, "-");
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
 
ack('mpw(wma, wmb, "**").toString(10)', Math.floor(Math.pow(zma, zmb)).toString(10));
ack('mpw(wma, wpb, "**").toString(10)', Math.pow(zma, zpb).toString(10));
ack('mpw(wpa, wmb, "**").toString(10)', Math.floor(Math.pow(zpa, zmb)).toString(10));
ack('mpw(wpa, wpb, "**").toString(10)', Math.pow(zpa, zpb).toString(10));
ack('mpw(wmb, wma, "**").toString(10)', Math.floor(Math.abs(Math.pow(zmb, zma))).toString(10));
ack('mpw(wmb, wpa, "**").toString(10)', Math.pow(zmb, zpa).toString(10));
ack('mpw(wpb, wma, "**").toString(10)', Math.floor(Math.pow(zpb, zma)).toString(10));
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
var lo = 2;
var hi = 10;
// var bases = [8, 10, 16];
var bases = [13];

for (var i in bases) {
    var t = bases[i];
    var tm1 = (t - 1);
    var tm2 = (t - 2);
    print("=====\t("+t+"**m - 1) * ("+t+"**n - 1), m,n in ["+lo+","+hi+")");
    for (var m = lo; m < hi; m++) {
	wb = mpw(t, m, "**", 1, "-");
	for (var n = m+1; n < hi+1; n++) {
	    wc = mpw(t, n, "**", 1, "-");
	    wa = mpw(wb, wc, "*");
//	    print(wa.toString(t));
	    switch (t) {
	    case 8:	zs = '0';	break;
	    case 10:	zs = '';	break;
	    case 16:	zs = '0x';	break;
	    default:	zs = t.toString(10) + "#";	break;
	    }
	    for (var j = 0; j < (m - 1); j++) zs += (t - 1).toString(t);
	    zs += (t - 2).toString(t);
	    for (var j = 0; j < (n - m); j++) zs += (t - 1).toString(t);
	    for (var j = 0; j < (m - 1); j++) zs += "0";
	    zs += "1";
//	    print(zs);
	    ack('wa.toString(t)', zs);
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
ack('mpw(e, phi, "gcd").toString(10)', '1');
var d = mpw(e, phi, "invm");
ack('mpw(e, d, phi, "mulm").toString(10)', '1');
ack('d.toString(10)', '44360237');

// Sign RSA
var m = mpw(31229978);
var s = mpw(m, d, n, "powm");
ack('s.toString(10)', '30729435');

// Verify RSA
var mtwiddle = mpw(s, e, n, "powm");
ack('mtwiddle.toString(10)', m.toString(10));

// ===== DSA example (from "Handbook of Applied Cryptography" 11.57 p453).
// Keygen DSA
var p = mpw(124540019);
var q = mpw(17389);
ack('mpw(p, mpw(1), "-", q, "%").toString(10)', '0');
var pdivq = mpw(p, mpw(1), "-", q, "/");
ack('pdivq.toString(10)', '7162');
var g = mpw(110217528);
var alpha = mpw(g, pdivq, p, "powm");
ack('alpha.toString(10)', '10083255');
var a = mpw(12496);
var y = mpw(alpha, a, p, "powm");
ack('y.toString(10)', '119946265');

// Sign DSA
var k = mpw(9557);
var r = mpw(mpw(alpha, k, p, "powm"), q, "%");
ack('r.toString(10)', '34');
var kinv = mpw(k, q, "invm");
ack('kinv.toString(10)', '7631');
var hm = mpw(5246);
var s = mpw(kinv, mpw(hm, a, r, "*", "+"), q, "mulm");
ack('s.toString(10)', '13049');

// Verify DSA
var w = mpw(s, q, "invm");
ack('w.toString(10)', '1799');
var u1 = mpw(w, hm, q, "mulm");
ack('u1.toString(10)', '12716');
var u2 = mpw(r, w, q, "mulm");
ack('u2.toString(10)', '8999');
var v1 = mpw(alpha, u1, p, "powm");
var v2 = mpw(y, u2, p, "powm");
var v3 = mpw(v1, v2, p, "mulm");
ack('v3.toString(10)', '27039929');
var v = mpw(v3, q, "%");
ack('v.toString(10)', r.toString(10));

// ===== ElGamal example (from "Handbook of Applied Cryptography" 11.65 p455).
// Keygen ElGamal
var p = mpw(2357);
var alpha = mpw(2);
var a = mpw(1751);
var y = mpw(alpha, a, p, "powm");
ack('y.toString(10)', '1185');

// Sign ElGamal
var hm = mpw(1463);
var k = mpw(1529);
var r = mpw(alpha, k, p, "powm");
ack('r.toString(10)', '1490');
var pm1 = mpw(p, mpw(1), "-");
var kinv = mpw(k, pm1, "invm");
ack('kinv.toString(10)', '245');
var s = mpw(kinv, mpw(hm, mpw(a, r, pm1, "mulm"), pm1, "subm"), pm1, "mulm");
ack('s.toString(10)', '1777');

// Verify ElGamal
var ytor = mpw(y, r, p, "powm");
var rtos = mpw(r, s, p, "powm");
var v1 = mpw(ytor, rtos, p, "mulm");
ack('v1.toString(10)', '1072');
var v2 = mpw(alpha, hm, p, "powm");
ack('v2.toString(10)', v1.toString(10));

// ===== DSA example from 
//     http://csrc.nist.gov/groups/ST/toolkit/documents/Examples/DSA2_All.pdf
// Keygen DSA
print("===== DSA");
var p = mpw('e0a67598cd1b763b'+
	'c98c8abb333e5dda0cd3aa0e5e1fb5ba8a7b4eabc10ba338'+
	'fae06dd4b90fda70d7cf0cb0c638be3341bec0af8a7330a3'+
	'307ded2299a0ee606df035177a239c34a912c202aa5f83b9'+
	'c4a7cf0235b5316bfc6efb9a248411258b30b839af172440'+
	'f32563056cb67a861158ddd90e6a894c72a5bbef9e286c6b');
var q = mpw('e950511eab424b9a19a2aeb4e159b7844c589c4f');
ack('mpw(p, mpw(1), "-", q, "%").toString(10)', '0');
var g = mpw('d29d5121b0423c27'+
	'69ab21843e5a3240ff19cacc792264e3bb6be4f78edd1b15'+
	'c4dff7f1d905431f0ab16790e1f773b5ce01c804e509066a'+
	'9919f5195f4abc58189fd9ff987389cb5bedf21b4dab4f8b'+
	'76a055ffe2770988fe2ec2de11ad92219f0b351869ac24da'+
	'3d7ba87011a701ce8ee7bfe49486ed4527b7186ca4610a75');
var c = mpw('f3b4c192'+
	'385620feec46cb7f5d55fe0c231b0404a61539729ea1291c');
var x = mpw('d0ec4e50bb290a42e9e355c73d8809345de2e139');
ack('mpw(c, q, mpw(1), "-", "%", mpw(1), "+").toString(16)', x.toString(16));
var y = mpw('25282217f5730501'+
	'dd8dba3edfcf349aaffec20921128d70fac44110332201bb'+
	'a3f10986140cbb97c726938060473c8ec97b4731db004293'+
	'b5e730363609df9780f8d883d8c4d41ded6a2f1e1bbbdc97'+
	'9e1b9d6d3c940301f4e978d65b19041fcf1e8b518f5c0576'+
	'c770fe5a7a485d8329ee2914a2de1b5da4a6128ceab70f79');
ack('mpw(g, x, p, "powm").toString(16)', y.toString(16));

// Sign DSA
var k    = mpw('349c55648dcf992f3f33e8026cfac87c1d2ba075');
var kinv = mpw('d557a1b4e7346c4a55427a28d47191381c269bde');
ack('mpw(k, q, "invm").toString(16)', kinv.toString(16));
var r    = mpw('636155ac9a4633b4665d179f9e4117df68601f34');
ack('mpw(mpw(g, k, p, "powm"), q, "%").toString(16)', r.toString(16));
var hm   = mpw('a9993e364706816aba3e25717850c26c9cd0d89d');
var s    = mpw('6c540b02d9d4852f89df8cfc99963204f4347704');
ack('mpw(kinv, mpw(hm, mpw(x, r, q, "mulm"), q, "addm"), q, "mulm").toString(16)', s.toString(16));

// Verify DSA
var w  = mpw('7e4f353b71d1a3dc2946f6bae3b9285ef736ce34');
ack('mpw(s, q, "invm").toString(16)', w.toString(16));
var u1 = mpw('00abfd278373edf8ce08347d49cd81308880103e');
ack('mpw(w, hm, q, "mulm").toString(16)', u1.toString(16));
var u2 = mpw('0da3ab84ae3ff412d703a63b41d1ec61d64b061c');
ack('mpw(r, w, q, "mulm").toString(16)', u2.toString(16));
var v1 = mpw('9c26b54969e24166'+
	'd89b06f5bec3b0df8179e4f9cf7606f67162edd150f73a1f'+
	'9e09e49d21ab0d04b4a02e2446c47bc9311e38c80effd862'+
	'fb69fe39eab9fc270b494a575e0d0862bef45df1a826a448'+
	'8bac5b3757e9c3513dd4d965e6b0ee18811bc711013cf4ea'+
	'beb578878c133783f80342cafa147b0c1cc6e51e937c8d11');
ack('mpw(g, u1, p, "powm").toString(16)', v1.toString(16));
var v2 = mpw('841232c0aa641d81'+
	'74aa4619826357f66fdf66e9a9b2a7c832e901d04b07b0e3'+
	'7c35596fe9e873e7888af879aa4795f558d9be9aa6cc302c'+
	'b29b9ccb03d72c5ebcfda30e03f3ab574d3c31c161b2fa41'+
	'd31f49f9d2df72a5a91378455b0a852614b6e40b3c4d1cfe'+
	'81e28ea6ce9d563b7506413cd29bfb7bde64e2f14828a631');
ack('mpw(y, u2, p, "powm").toString(16)', v2.toString(16));
var v3 = mpw(v1, v2, p, "mulm");
var v  = mpw('636155ac9a4633b4665d179f9e4117df68601f34');
ack('mpw(v3, q, "%").toString(16)', v.toString(16));

// =======================================================
// Return y^2 - x^3 + 3x (modulo p)
function vfyPrimeCurve(p, x, y) {
    var t1 = mpw(y, y, "*");
    var t2 = mpw(mpw(mpw(x, x, "*"), p, "%"), x, "*");
    t1 = mpw(t1, t2, "-");
    t1 = mpw(t1, x, "+");
    t1 = mpw(t1, x, "+");
    t1 = mpw(t1, x, "+");
    t1 = mpw(t1, p, "%");
    return t1;
}

// =======================================================
// Return y^2 + xy - x^3 - x^2 (modulo p)
function vfyBinaryCurve(p, x, y) {
    var t1 = mpw(y, y, p, "mulm");
    var t2 = mpw(x, y, p, "mulm");
    t1 = mpw(t1, t2, p, "addm");
    t2 = mpw(x, x, p, "mulm");
    t1 = mpw(t1, t2, p, "subm");
    t2 = mpw(t2, x, p, "mulm");
    t1 = mpw(t1, t2, p, "subm");
    return t1;
}

// =======================================================
// Return y^2 + xy - x^3 - 1 (modulo p)
function vfyKoblitzCurve0(p, x, y) {
    var t1 = mpw(mpw(mpw(x, y, "+"), y, "*"), p, "%");
    var t2 = mpw(mpw(mpw(x, x, "*"), p, "%"), x, "*");
    t1 = mpw(t1, t2, "-");
    t1 = mpw(t1, 1, "-");
    t1 = mpw(t1, p, "%");
    return t1;
}

// =======================================================
// Return y^2 + xy - x^3 - x^2 - 1 (modulo p)
function vfyKoblitzCurve1(p, x, y) {
    var t1 = vfyBinaryCurve(p, x, y);
    t1 = mpw(t1, 1, "-");
    t1 = mpw(t1, p, "%");
    return t1;
}

// ===== secp 112r1
print("===== SECP 112r1");
var p    = mpw('db7c2abf62e35e668076bead208b');
ack('mpw(mpw(mpw(2, 128, "**"), 3, "-"), 76439, "/").toString(16)', p.toString(16));
var b    = mpw('659ef8ba043916eede8911702b22');
var n    = mpw('db7c2abf62e35e7628dfac6561c5');
var gx   = mpw('09487239995a5ee76b55f9c2f098');
var gy   = mpw('a89ce5af8724c0a23e0e0ff77500');
var t1 = vfyPrimeCurve(p, gx, gy);
ack('t1.toString(16)', b.toString(16));

// ===== SECP 128r1
print("===== SECP 128r1");
var p    = mpw('fffffffdffffffffffffffffffffffff');
ack('mpw(mpw(mpw(2, 128, "**"), mpw(2, 97, "**"), "-"), 1, "-").toString(16)', p.toString(16));
var b    = mpw('e87579c11079f43dd824993c2cee5ed3');
var n    = mpw('fffffffe0000000075a30d1b9038a115');
var gx   = mpw('161ff7528b899b2d0c28607ca52c5b86');
var gy   = mpw('cf5ac8395bafeb13c02da292dded7a83');
var t1 = vfyPrimeCurve(p, gx, gy);
ack('t1.toString(16)', b.toString(16));

// ===== SECP 160r1
print("===== SECP 160r1");
var p    = mpw('ffffffffffffffffffffffffffffffff7fffffff');
ack('mpw(mpw(mpw(2, 160, "**"), mpw(2, 31, "**"), "-"), 1, "-").toString(16)', p.toString(16));
var b    = mpw('1c97befc54bd7a8b65acf89f81d4d4adc565fa45');
var n    = mpw('0100000000000000000001f4c8f927aed3ca752257');
var gx   = mpw('4a96b5688ef573284664698968c38bb913cbfc82');
var gy   = mpw('23a628553168947d59dcc912042351377ac5fb32');
var t1 = vfyPrimeCurve(p, gx, gy);
t1 = mpw(t1, p, "+");
ack('t1.toString(16)', b.toString(16));

// ===== ECDSA P-192 example from 
//	http://csrc.nist.gov/groups/ST/toolkit/documents/Examples/ECDSA_Prime.pdf
print("===== P-192");
var p    = mpw(mpw(mpw(2, 192, "**"), mpw(2, 64, "**"), "-"), 1, "-");
ack('p.toString(10)', '6277101735386680763835789423207666416083908700390324961279');
var n    = mpw('ffffffffffffffffffffffff99def836146bc9b1b4d22831');
ack('n.toString(10)', '6277101735386680763835789423176059013767194773182842284081');

var c    = mpw('3099d2bbbfcb2538542dcd5fb078b6ef5f3d6fe2c745de65');
var b    = mpw('64210519e59c80e70fa7e9ab72243049feb8deecc146b9b1');
ack('mpw(mpw(b, p, "sqrm"), c, p, "mulm").toString(10)', mpw(p, 27, "-").toString(10));

var gx   = mpw('188da80eb03090f67cbf20eb43a18800f4ff0afd82ff1012');
var gy   = mpw('07192b95ffc8da78631011ed6b24cdd573f977a11e794811');
var t1 = vfyPrimeCurve(p, gx, gy);
t1 = mpw(t1, p, "+");
ack('t1.toString(16)', b.toString(16));

var d    = mpw('7891686032fd8057f636b44b1f47cce564d2509923a7465b');
var qx   = mpw('fba2aac647884b504eb8cd5a0a1287babcc62163f606a9a2');
var qy   = mpw('dae6d4cc05ef4f27d79ee38b71c9c8ef4865d98850d84aa5');
var t1 = vfyPrimeCurve(p, qx, qy);
t1 = mpw(t1, p, "+");
ack('t1.toString(16)', b.toString(16));

var k    = mpw('d06cb0a0ef2f708b0744f08aa06b6deedea9c0f80a69d847');
var msg  = "Example of ECDSA with P-192";
var e    = mpw('1b376f0b735c615ceeeb31baee654b0a374825db');
var kinv = mpw('5a277943c5a4d34b3c7dd97bdfe9b82c042586701088c00b');
ack('mpw(k, n, "invm").toString(16)', kinv.toString(16));
var r    = mpw('f0ecba72b88cde399cc5a18e2a8b7da54d81d04fb9802821');
var s    = mpw('1e6d3d4ae2b1fab2bd2040f5dabf00f854fa140b6d21e8ed');
ack('mpw(kinv, mpw(e, mpw(r, d, n, "mulm"), n, "addm"), n, "mulm").toString(16)', s.toString(16));

var w    = mpw(s, n, "invm");
var u1   = mpw('785760fd37767d546003fa66933b7d202642352331b15b84');
ack('mpw(e, w, n, "mulm").toString(16)', u1.toString(16));
var u2   = mpw('de0747072e426e307ba1e19bd5c1b57f9e29220ae97cc9bc');
ack('mpw(r, w, n, "mulm").toString(16)', u2.toString(16));
var v    = mpw('f0ecba72b88cde399cc5a18e2a8b7da54d81d04fb9802821');

// ===== ECDSA P-224 example from 
//	http://csrc.nist.gov/groups/ST/toolkit/documents/Examples/ECDSA_Prime.pdf
print("===== P-224");
var p    = mpw(mpw(mpw(2, 224, "**"), mpw(2, 96, "**"), "-"), 1, "+");
ack('p.toString(10)', '26959946667150639794667015087019630673557916260026308143510066298881');
var n    = mpw('ffffffffffffffffffffffffffff16a2e0b8f03e13dd29455c5c2a3d');
ack('n.toString(10)', '26959946667150639794667015087019625940457807714424391721682722368061');

var c    = mpw('5b056c7e11dd68f40469ee7f3c7a7d74f7d121116506d031218291fb');
var b    = mpw('b4050a850c04b3abf54132565044b0b7d7bfd8ba270b39432355ffb4');
ack('mpw(mpw(b, b, "*", p, "%"), c, "*", p, "%").toString(10)', mpw(p, 27, "-").toString(10));
var gx   = mpw('b70e0cbd6bb4bf7f321390b94a03c1d356c21122343280d6115c1d21');
var gy   = mpw('bd376388b5f723fb4c22dfe6cd4375a05a07476444d5819985007e34');
var t1 = vfyPrimeCurve(p, gx, gy);
ack('t1.toString(16)', b.toString(16));

var d    = mpw('3f0c488e987c80be0fee521f8d90be6034ec69ae11ca72aa777481e8');
var qx   = mpw('e84fb0b8e7000cb657d7973cf6b42ed78b301674276df744af130b3e');
var qy   = mpw('4376675c6fc5612c21a0ff2d2a89d2987df7a2bc52183b5982298555');
var t1 = vfyPrimeCurve(p, qx, qy);
t1 = mpw(t1, p, "+");
ack('t1.toString(16)', b.toString(16));

var k    = mpw('a548803b79df17c40cde3ff0e36d025143bcbba146ec32908eb84937');
var msg  = "Example of ECDSA with P-224";
var e    = mpw('1f1e1cf892926cfccfc5a28feef3d807d23f778008dba4b35f04b2fd');
var kinv = mpw('b4d9d81feff7b325e09e770c40bace8b008d6074371967326f39130c');
ack('mpw(k, n, "invm").toString(16)', kinv.toString(16));
var r    = mpw('c3a3f5b82712532004c6f6d1db672f55d931c3409ea1216d0be77380');
var s    = mpw('c5aa1eae6095dea34c9bd84da3852cca41a8bd9d5548f36dabdf6617');
ack('mpw(kinv, mpw(e, mpw(r, d, n, "mulm"), n, "addm"), n, "mulm").toString(16)', s.toString(16));

var w    = mpw(s, n, "invm");
var u1   = mpw('69df611df949498ebe20c1e453cf231cdd2f30adeecba9335481295d');
ack('mpw(e, w, n, "mulm").toString(16)', u1.toString(16));
var u2   = mpw('86daaf97dc9bb13a66ec7b735e69bccd60f395efb2cdfded8a3ccbcf');
ack('mpw(r, w, n, "mulm").toString(16)', u2.toString(16));
var v    = mpw('c3a3f5b82712532004c6f6d1db672f55d931c3409ea1216d0be77380');

// ===== ECDSA P-256 example from 
//	http://csrc.nist.gov/groups/ST/toolkit/documents/Examples/ECDSA_Prime.pdf
print("===== P-256");
var p    = mpw(2, 256, "**");
p = mpw(p, mpw(2, 224, "**"), "-");
p = mpw(p, mpw(2, 192, "**"), "+");
p = mpw(p, mpw(2,  96, "**"), "+");
p = mpw(p, 1, "-");
ack('p.toString(10)', '115792089210356248762697446949407573530086143415290314195533631308867097853951');
var n    = mpw('ffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551');
ack('n.toString(10)', '115792089210356248762697446949407573529996955224135760342422259061068512044369');

var c    = mpw('7efba1662985be9403cb055c75d4f7e0ce8d84a9c5114abcaf3177680104fa0d');
var b    = mpw('5ac635d8aa3a93e7b3ebbd55769886bc651d06b0cc53b0f63bce3c3e27d2604b');
ack('mpw(mpw(b, p, "sqrm"), c, p, "mulm").toString(10)', mpw(p, 27, "-").toString(10));
var gx   = mpw('6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296');
var gy   = mpw('4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5');
var t1 = vfyPrimeCurve(p, gx, gy);
t1 = mpw(t1, p, "+");
ack('t1.toString(16)', b.toString(16));

var d    = mpw('c477f9f65c22cce20657faa5b2d1d8122336f851a508a1ed04e479c34985bf96');
var qx   = mpw('b7e08afdfe94bad3f1dc8c734798ba1c62b3a0ad1e9ea2a38201cd0889bc7a19');
var qy   = mpw('3603f747959dbf7a4bb226e41928729063adc7ae43529e61b563bbc606cc5e09');
var t1 = vfyPrimeCurve(p, qx, qy);
t1 = mpw(t1, p, "+");
ack('t1.toString(16)', b.toString(16));

var k    = mpw('7a1a7e52797fc8caaa435d2a4dace39158504bf204fbe19f14dbb427faee50ae');
var msg  = "Example of ECDSA with P-256";
var e    = mpw('a41a41a12a799548211c410c65d8133afde34d28bdd542e4b680cf2899c8a8c4');
var kinv = mpw('62159e5ba9e712fb098cce8fe20f1bed8346554e98ef3c7c1fc3332ba67d87ef');
ack('mpw(k, n, "invm").toString(16)', kinv.toString(16));
var r    = mpw('2b42f576d07f4165ff65d1f3b1500f81e44c316f1f0b3ef57325b69aca46104f');
var s    = mpw('dc42c2122d6392cd3e3a993a89502a8198c1886fe69d262c4b329bdb6b63faf1');
ack('mpw(mpw(mpw(r, d, "*", n, "%"), e, "+", n, "%"), kinv, "*", n, "%").toString(16)', s.toString(16));

var w    = mpw(s, n, "invm");
var u1   = mpw('b807bf3281dd13849958f444fd9aea808d074c2c48ee8382f6c47a435389a17e');
ack('mpw(e, w, "*", n, "%").toString(16)', u1.toString(16));
var u2   = mpw('1777f73443a4d68c23d1fc4cb5f8b7f2554578ee87f04c253df44efd181c184c');
ack('mpw(r, w, "*", n, "%").toString(16)', u2.toString(16));
var v    = mpw('2b42f576d07f4165ff65d1f3b1500f81e44c316f1f0b3ef57325b69aca46104f');

// ===== ECDSA P-384 example from 
//	http://csrc.nist.gov/groups/ST/toolkit/documents/Examples/ECDSA_Prime.pdf
print("===== P-384");
var p    = mpw(2, 384, "**");
p = mpw(p, mpw(2, 128, "**"), "-");
p = mpw(p, mpw(2,  96, "**"), "-");
p = mpw(p, mpw(2,  32, "**"), "+");
p = mpw(p, 1, "-");
ack('p.toString(10)', '39402006196394479212279040100143613805079739270465446667948293404245721771496870329047266088258938001861606973112319');
var n    = mpw('ffffffffffffffffffffffffffffffffffffffffffffffffc7634d81f4372ddf581a0db248b0a77aecec196accc52973');
ack('n.toString(10)', '39402006196394479212279040100143613805079739270465446667946905279627659399113263569398956308152294913554433653942643');

var c    = mpw('79d1e655f868f02fff48dcdee14151ddb80643c1406d0ca10dfe6fc52009540a495e8042ea5f744f6e184667cc722483');
var b    = mpw('b3312fa7e23ee7e4988e056be3f82d19181d9c6efe8141120314088f5013875ac656398d8a2ed19d2a85c8edd3ec2aef');
ack('mpw(mpw(b, b, "*", p, "%"), c, "*", p, "%").toString(10)', mpw(p, 27, "-").toString(10));
var gx   = mpw('aa87ca22be8b05378eb1c71ef320ad746e1d3b628ba79b9859f741e082542a385502f25dbf55296c3a545e3872760ab7');
var gy   = mpw('3617de4a96262c6f5d9e98bf9292dc29f8f41dbd289a147ce9da3113b5f0b8c00a60b1ce1d7e819d7a431d7c90ea0e5f');
var t1 = vfyPrimeCurve(p, gx, gy);
ack('t1.toString(16)', b.toString(16));

var d    = mpw('f92c02ed629e4b48c0584b1c6ce3a3e3b4faae4afc6acb0455e73dfc392e6a0ae393a8565e6b9714d1224b57d83f8a08');
var qx   = mpw('3bf701bc9e9d36b4d5f1455343f09126f2564390f2b487365071243c61e6471fb9d2ab74657b82f9086489d9ef0f5cb5');
var qy   = mpw('d1a358eafbf952e68d533855ccbdaa6ff75b137a5101443199325583552a6295ffe5382d00cfcda30344a9b5b68db855');
var t1 = vfyPrimeCurve(p, qx, qy);
ack('t1.toString(16)', b.toString(16));

var k    = mpw('2e44ef1f8c0bea8394e3dda81ec6a7842a459b534701749e2ed95f054f0137680878e0749fc43f85edcae06cc2f43fef');
var msg  = "Example of ECDSA with P-384";
var e    = mpw('5aea187d1c4f6e1b35057d20126d836c6adbbc7049ee0299c9529f5e0b3f8b5a7411149d6c30d6cb2b8af70e0a781e89');
var kinv = mpw('ac227da51929533dfc2e9eefb4e0f7bd22392ca73289ed1c6c00b214e8874d8007c8ac46b25d677dfe9b1c6c10a47e4a');
ack('mpw(k, n, "invm").toString(16)', kinv.toString(16));
var r    = mpw('30ea514fc0d38d8208756f068113c7cada9f66a3b40ea3b313d040d9b57dd41a332795d02cc7d507fcef9faf01a27088');
var s    = mpw('cc808e504be414f46c9027bcbf78adf067a43922d6fcaa66c4476875fbb7b94efd1f7d5dbe620bfb821c46d549683ad8');
ack('mpw(kinv, mpw(e, mpw(r, d, n, "mulm"), n, "addm"), n, "mulm").toString(16)', s.toString(16));

var w    = mpw(s, n, "invm");
var u1   = mpw('9c0590ee8000b79832dc4c6776f7e5fd2a74be161741c7c2d2f038d439831696a1b8ece4199d225b12b76dd9e637b250');
ack('mpw(e, w, n, "mulm").toString(16)', u1.toString(16));
var u2   = mpw('8f77be5b0eb32a1a3b9274cfda53518a01aad4afc4bd46a392b7c7de4eed3fe6dee54f3064234fe7fde57ae45532c24d');
ack('mpw(r, w, n, "mulm").toString(16)', u2.toString(16));
var v    = mpw('30ea514fc0d38d8208756f068113c7cada9f66a3b40ea3b313d040d9b57dd41a332795d02cc7d507fcef9faf01a27088');

// ===== ECDSA P-521 example from 
//	http://csrc.nist.gov/groups/ST/toolkit/documents/Examples/ECDSA_Prime.pdf
print("===== P-521");
var p    = mpw(mpw(2, 521, "**"), 1, "-");
ack('p.toString(10)', '6864797660130609714981900799081393217269435300143305409394463459185543183397656052122559640661454554977296311391480858037121987999716643812574028291115057151');
var n    = mpw('01fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffa51868783bf2f966b7fcc0148f709a5d03bb5c9b8899c47aebb6fb71e91386409');
ack('n.toString(10)', '6864797660130609714981900799081393217269435300143305409394463459185543183397655394245057746333217197532963996371363321113864768612440380340372808892707005449');

var c    = mpw('79d1e655f868f02fff48dcdee14151ddb80643c1406d0ca10dfe6fc52009540a495e8042ea5f744f6e184667cc722483');
var b    = mpw('051953eb9618e1c9a1f929a21a0b68540eea2da725b99b315f3b8b489918ef109e156193951ec7e937b1652c0bd3bb1bf073573df883d2c34f1ef451fd46b503f00');
// FIXME: wrong answer
// ack('mpw(mpw(b, p, "sqrm"), c, p, "mulm").toString(10)', mpw(p, 27, "-").toString(10));
var gx   = mpw('c6858e06b70404e9cd9e3ecb662395b4429c648139053fb521f828af606b4d3dbaa14b5e77efe75928fe1dc127a2ffa8de3348b3c1856a429bf97e7e31c2e5bd66');
var gy   = mpw('11839296a789a3bc0045c8a5fb42c7d1bd998f54449579b446817afbd17273e662c97ee72995ef42640c550b9013fad0761353c7086a272c24088be94769fd16650');
var t1 = vfyPrimeCurve(p, gx, gy);
t1 = mpw(t1, p, "+");
ack('t1.toString(16)', b.toString(16));

var d    = mpw('0100085f47b8e1b8b11b7eb33028c0b2888e304bfc98501955b45bba1478dc184eeedf09b86a5f7c21994406072787205e69a63709fe35aa93ba333514b24f961722');
var qx   = mpw('0098e91eef9a68452822309c52fab453f5f117c1da8ed796b255e9ab8f6410cca16e59df403a6bdc6ca467a37056b1e54b3005d8ac030decfeb68df18b171885d5c4');
var qy   = mpw('0164350c321aecfc1cca1ba4364c9b15656150b4b78d6a48d7d28e7f31985ef17be8554376b72900712c4b83ad668327231526e313f5f092999a4632fd50d946bc2e');
var t1 = vfyPrimeCurve(p, qx, qy);
ack('t1.toString(16)', b.toString(16));

var k    = mpw('c91e2349ef6ca22d2de39dd51819b6aad922d3aecdeab452ba172f7d63e370cecd70575f597c09a174ba76bed05a48e562be0625336d16b8703147a6a231d6bf');
var msg  = "Example of ECDSA with P-512";
var e    = mpw('9bf0e1deeda31e00f925b77f7cb6b1ced7368de1dc75bb9f94582c1ca709205d32af90025b02fa132fbebd6cddcd9172c0d66d8e581767a8b6f71de60be1f932');
var kinv = mpw('01eab94335a7ed337bce83c95de95447925edb0ee27f8e8378713e767d6da570fccfb4f13dcf57f898e77ddb540a9453e0c3d5c97ae8d2ec843590bcb1d349044c09');
// FIXME: wrong answer
// ack('mpw(k, n, "invm").toString(16)', kinv.toString(16));
var r    = mpw('0140c8edca57108ce3f7e7a240ddd3ad74d81e2de62451fc1d558fdc79269adacd1c2526eeeef32f8c0432a9d56e2b4a8a732891c37c9b96641a9254ccfe5dc3e2ba');
var s    = mpw('00d72f15229d0096376da6651d9985bfd7c07f8d49583b545db3eab20e0a2c1e8615bd9e298455bdeb6b61378e77af1c54eee2ce37b2c61f5c9a8232951cb988b5b1');
ack('mpw(kinv, mpw(e, mpw(r, d, n, "mulm"), n, "addm"), n, "mulm").toString(16)', s.toString(16));

var w    = mpw(s, n, "invm");
var u1   = mpw('01697eefb6bd3a6db024254fe69fd19c80eb04b71cdd16af72f322106093f971cb08c29f6f8950f0f61e45bf65bac39a590dcb043758c6606907f216a759b4ea4be4');
ack('mpw(e, w, n, "mulm").toString(16)', u1.toString(16));
var u2   = mpw('014e7fc3ee94b91e092f660253dcaf92a70306bdfa317a0ab7efb2c8286944bee5f146114f2c61950f8c8699cce1a22fe632ea89967d33fefcb0e7607b4b66d157b6');
ack('mpw(r, w, n, "mulm").toString(16)', u2.toString(16));
var v    = mpw('0140c8edca57108ce3f7e7a240ddd3ad74d81e2de62451fc1d558fdc79269adacd1c2526eeeef32f8c0432a9d56e2b4a8a732891c37c9b96641a9254ccfe5dc3e2ba');

// ##############################################################
// Curve K-163
// ##############################################################
print("===== K-163");
var n    = mpw('0004000000000000000000020108a2e0cc0d99f8a5ef');
ack('n.toString(10)', '5846006549323611672814741753598448348329118574063');

var d    = mpw('7ff9959e88692406041273ab70e00b88ea18c9e6');
var qx   = mpw('2cc266f6d83736500418d2e00ce2d07c2ff8f734');
var qy   = mpw('00028553ea0676c40d577dcfe5259968b341a1d33af2');

var k    = mpw('1e7ec8c98ab49fc14e1bc841ebf61a0ad0961472');
var msg  = "Example of ECDSA with K-163"

var rx   = mpw('000718ef5ef376b8f5301b5fd2a761989c417807a275');

var e    = mpw('590d17df8bf17246eaf975b41864abcc7a747021');
var kinv = mpw('39252c5d41a936736cae1413da88924935f61b58');

var r    = mpw('000318ef5ef376b8f5301b5dd19ebeb7d033de0efc86');
ack('r.toString(10)', '4526858839996231728501073084029310952934625705094');
var s    = mpw('0002fd6b089af933c881d4b9feb3a550cbded50cd7bd');
ack('s.toString(10)', '4369764869049498617479414634111521353329149794237');

var w    = mpw(s, n, "invm");
var u1   = mpw('000332dd232ef2d65c09309d52c7be1646d41fc9de20');
var u2   = mpw('0003d355dad4b1418780a63a87dafc06d38bfc287312');
var rx   = mpw('000718ef5ef376b8f5301b5fd2a761989c417807a275');
var v    = mpw('000318ef5ef376b8f5301b5dd19ebeb7d033de0efc86');

// ##############################################################
// Curve B-163
// ##############################################################
print("===== B-163");
var p    = mpw(2, 163, "**");
p = mpw(p, mpw(2,   7, "**"), "+");
p = mpw(p, mpw(2,   6, "**"), "+");
p = mpw(p, mpw(2,   3, "**"), "+");
p = mpw(p, 1, "+");
ack('p.toString(16)', '0x800000000000000000000000000000000000000c9');
var n    = mpw('00040000000000000000000292fe77e70c12a4234c33');
ack('n.toString(10)', '5846006549323611672814742442876390689256843201587');

// polynomial basis
var b  = mpw('20a601907b8c953ca1481eb10512f78744a3205fd');
var gx = mpw('3f0eba16286a2d57ea0991168d4994637e8343e36');
var gy = mpw('0d51fbc6c71a0094fa2cdd545b11c5c0c797324f1');
// normal basis
// var b  = mpw('6645f3cacf1638e139c6cd13ef61734fbc9e3d9fb');
// var gx = mpw('0311103c17167564ace77ccb09c681f886ba54ee8');
// var gy = mpw('333ac13c6447f2e67613bf7009daf98c87bb50c7f');

// var t1 = vfyBinaryCurve(p, gx, gy);
// ack('t1.toString(16)', b.toString(16));

var d    = mpw('000348d138c2de9447bd288feed177222ee377fb7bea');
var qx   = mpw('00066b015c0b72b0f81b1ecba6f58e7545d94744644c');
var qy   = mpw('ba6d4d62419155b186a29784f4aa4b8e8e1e7f76');

var k    = mpw('8ed0f93f7d492bb3991847d0e96f9cc3947259aa');
var msg  = "Example of ECDSA with B-163"

var rx   = mpw('000760938a97d88b30fdfb2cce1a4c59783ad0ed8fde');
var e    = mpw('728d59bbe028509dd5d2ce480f458e2925232ac3');
var kinv = mpw('00036dc66491684211373aaa8bd16024dd0a12a8ff11');

var r    = mpw('000360938a97d88b30fdfb2a3b1bd4726c282cca43ab');
ack('r.toString(10)', '4935858308701913308545983330530451819807197774763');
var s    = mpw('00019a7b5043d93a13d714b4717fc0698e6791cf7f7c');
ack('s.toString(10)', '2343436199767730626813973682401889576912314924924');

var e    = mpw('728d59bbe028509dd5d2ce480f458e2925232ac3');

var w    = mpw(s, n, "invm");
var u1   = mpw('0003c4099db241a80c807e02d81be10955b2eb2e5d8c');
var u2   = mpw('000160ccedabb7e3e767a604d8b042a65751708cc262');
var rx   = mpw('000760938a97d88b30fdfb2cce1a4c59783ad0ed8fde');
var v    = mpw('000360938a97d88b30fdfb2a3b1bd4726c282cca43ab');

// ##############################################################
// Curve K-233
// ##############################################################
print("===== K-233");
var n    = mpw('008000000000000000000000000000069d5bb915bcd46efb1ad5f173abdf');
ack('n.toString(10)', '3450873173395281893717377931138512760570940988862252126328087024741343');

var d    = mpw('8434613f4b799b4c26e4d7ab8e9481b04b09e648c94affd14b611a20');
var qx   = mpw('017c9dd766aefbe4de4b15f46db0671dc4ca0767ed51ecea94757d9c662e');
var qy   = mpw('01cdd726084837ae73c11c27d605c6eb2d5e31482358780305c2522b151b');

var k    = mpw('000190da60fe3b179b96611db7c7e5217c9aff0aee435782ebfb2dfff27f');
var msg  = "Example of ECDSA with K-233"

var rx   = mpw('01bea7231662e6516f11e37d59d500eae71d116e9b7bbce5964b88d4cc4d');
var e    = mpw('cb2314b21e913f4d345af272ed611f3eba00388056921da8a450f731');
var kinv = mpw('00759a37371c026e261add7278b81ee15d18e2b0d9bd1a077aebf96aa067');

var r    = mpw('003ea7231662e6516f11e37d59d500d70f09e62d64fe6ff445c9b479c8b0');
ack('r.toString(10)', '1689118280210305605171693773892079201866275012021226933371217775282352');
var s    = mpw('007503ec8e630386ce36a6276f221bc8ee4592cca5960a1eabe3bd2a282a');
ack('s.toString(10)', '3154727010507234306862258506085323724325131083359956345879910870034474');

var w    = mpw(s, n, "invm");
var u1   = mpw('0043a8ea51489b412b0a1ca97865a12491e8e144159e56cdc8bbe201d0d5');
var u2   = mpw('0036d6ac76bcac51707f796e6fa1cb649f7fd2c69aed93d01ab7c8ae35aa');
var rx   = mpw('01bea7231662e6516f11e37d59d500eae71d116e9b7bbce5964b88d4cc4d');
var v    = mpw('003ea7231662e6516f11e37d59d500d70f09e62d64fe6ff445c9b479c8b0');

// ##############################################################
// Curve B-233
// ##############################################################
print("===== B-233");
var n    = mpw('01000000000000000000000000000013e974e72f8a6922031d2603cfe0d7');
ack('n.toString(10)', '6901746346790563787434755862277025555839812737345013555379383634485463');

var d    = mpw('8af6d5a8e875977c7d4ba1f611cf7b6d70b26140bf84a1cc281f1b7b');
var qx   = mpw('01d3ad52d68f8383f582e2ba00f89ce1632211edc24440c31798e0c8ed40');
var qy   = mpw('006c3b96cc0e6bc59355a1294e22dbf1d4b9071c28da1389b6debe0e7f43');

var k    = mpw('8a65482917ba18f1e8b266a3795b0a3a09c439fa6b611e37123baf72');
var msg  = "Example of ECDSA with B-233"

var rx   = mpw('0186806715d9620f0a3e62c1ba593d9817b6dcb23de85bf504c326629e63');
var e    = mpw('6a8389d3644c7fa90d7f57e6049d0dd41b8e473cd296c71d0ff232b3');
var kinv = mpw('0097589e4b9b7c0f0c1e58d2ac61e8297d83fe7d00172e64b7d0df207dd9');

var r    = mpw('0086806715d9620f0a3e62c1ba593d842e41f582b37f39f1e79d2292bd8c');
ack('r.toString(10)', '3626155233584346473011272901749115607875304189783836849535359105809804');
var s    = mpw('0040159539861be6673c0a3b2e49f5f6c9532b60130c6fc78826c9e900ef');
ack('s.toString(10)', '1727709532304725124330354137202758438690663000173119669782159003549935');

var w    = mpw(s, n, "invm");
var u1   = mpw('0009621acb96ba987e2354a4ca3273c142bc963cce4cb84e17f4a6a8a3e1');
var u2   = mpw('006f64d0192b2965749d0ac29c7840245cc70bdb296f7e71cfc59c069515');
var rx   = mpw('0186806715d9620f0a3e62c1ba593d9817b6dcb23de85bf504c326629e63');
var v    = mpw('0086806715d9620f0a3e62c1ba593d842e41f582b37f39f1e79d2292bd8c');

// ##############################################################
// Curve K-283
// ##############################################################
print("===== K-283");
var n    = mpw('01ffffffffffffffffffffffffffffffffffe9ae2ed07577265dff7f94451e061e163c61');
ack('n.toString(10)', '3885337784451458141838923813647037813284811733793061324295874997529815829704422603873');

var d    = mpw('00069e6d19f7e454a83664ff49208f6038eaf842e164df42d0f64948ff9c94b014988329');
var qx   = mpw('01b64a60d4a365409635aaa27e1708d90b839afa2d9820e12b79c3af1094b6010aaef5be');
var qy   = mpw('0334b5f30ca21756bde6d47738f2458f56fbf6bdc76fcfb8f3e591455f041a952ee87a8e');

var k    = mpw('e3084442d66fa9a02c42890163e57ee33ca1f4583c65bcbde92781c7a3c83e89b773');
var msg  = "Example of ECDSA with K-283"

var rx   = mpw('07c973d58fd17a06aa8f39d5ec42e0a6b992f6cc61f157565dd7036c147d9005400c1328');
var e    = mpw('184f9aea741e7668b8b5c72c81617fa4068929628f77bd2f7a713a0a09916b81');
var kinv = mpw('0045d85f04239846deb60444da59f95ca0ca13fb9c30b6972e852e332e223067143d174d');

var r    = mpw('01c973d58fd17a06aa8f39d5ec42e0a6b99339c1d57ff6f0eabd04ed57ae35f2e5c95e05');
ack('r.toString(10)', '3471401162510341375623239443838810951682842412872079937478668905492984462483269443077');
var s    = mpw('014a4ed02cbe4d76ed5ddaa34a9f2d7390af2de327edbc3335119d3e43cbb7fe0384d841');
ack('s.toString(10)', '2506557860315181892752630984589168964256966490874765067882847886248065073786590124097');

var w    = mpw(s, n, "invm");
var u1   = mpw('001950717a3ba3fe8cc4677c5c6a1f9ec4c92a7d405e39e133250abc8038a945b86fbbb8');
var u2   = mpw('00b8d9b42e8b8c1c7b610132b88a9d3ddc8beae7bd8a7e5fc94fc937c46779bf8e33ca2a');
var rx   = mpw('07c973d58fd17a06aa8f39d5ec42e0a6b992f6cc61f157565dd7036c147d9005400c1328');
var v    = mpw('01c973d58fd17a06aa8f39d5ec42e0a6b99339c1d57ff6f0eabd04ed57ae35f2e5c95e05');

// ##############################################################
// Curve B-283
// ##############################################################
print("===== B-283");
var n    = mpw('03ffffffffffffffffffffffffffffffffffef90399660fc938a90165b042a7cefadb307');
ack('n.toString(10)', '7770675568902916283677847627294075626569625924376904889109196526770044277787378692871');

var d    = mpw('010652d37b0a9db64d4033ac6549cd1df37e1eede2612c2363257c6aff6c8cb5dcb63648');
var qx   = mpw('0390858e9327a714c74af0c3adedf4e6c75cafdcc46507a49e415b138a094b6f43e882ac');
var qy   = mpw('00d4a65d973cd150a5221bedf872a4ba207ff4427dfffd4827c5bf169e719162504d0631');

var k    = mpw('0100ec321393e6dd6c4d47be5ae189e5e35408579d0862178f94ccbba3c4049a4d88e297');
var msg  = "Example of ECDSA with B-283"

var rx   = mpw('077cb284ac41e72eda2a93eb8d6dff58620f6c69d528dfe90d909aa5cabc03a34e5d5a76');
var e    = mpw('f0bf4aef3f694ebdde0a79445c897adb2430b91877c772da9b7362cb03aea87f');
var kinv = mpw('00ab6d18af222d8fde7d93894d4faeeb36accd4fb68ec95d9e9bff4c08aff3c631a67be4');

var r    = mpw('037cb284ac41e72eda2a93eb8d6dff58620f7cd99b927eec7a060a8f6fb7d9265eafa76f');
ack('r.toString(10)', '6774278697741420663687521446478692454362079599099886695356533960517936672443416422255');
var s    = mpw('00a37ac10aebfc22fc6e6ee22e8f235e3eeb0555a0f0f9da92d9ffa734ad767956d27f23');
ack('s.toString(10)', '1240572480066211322672976019324920068198845924194402393411512731505068634362155794211');

var w    = mpw(s, n, "invm");
var u1   = mpw('01a9af084b9e04574c178a2d00c97cfe36f25d5f834fcd7044235a92e89ac3346c8c9af3');
var u2   = mpw('009949042a40d7613f3880cfcc3b3e9d22ee9c130651cee1f263efc881cf9a53a564fbce');
var rx   = mpw('077cb284ac41e72eda2a93eb8d6dff58620f6c69d528dfe90d909aa5cabc03a34e5d5a76');
var v    = mpw('037cb284ac41e72eda2a93eb8d6dff58620f7cd99b927eec7a060a8f6fb7d9265eafa76f');

// ##############################################################
// Curve K-409
// ##############################################################
print("===== K-409");
var n    = mpw('007ffffffffffffffffffffffffffffffffffffffffffffffffffe5f83b2d4ea20400ec4557d5ed3e3e7ca5b4b5c83b8e01e5fcf');
ack('n.toString(10)', '330527984395124299475957654016385519914202341482140609642324395022880711289249191050673258457777458014096366590617731358671');

var d    = mpw('00019f5789fe26e0e700c69e253e9f74d76eafb4c979d0b1584d4fe98715d45b7baaa851e02a1ecaed8b96602cf611d8a504bbd5');
var qx   = mpw('0127065590df9265fdfba4ed6edf76a9bc8ce880b58b6f571a1ab62ba3401269441f3b95ecd0909465022240ae45c7b36a91de58');
var qy   = mpw('003c85268d9267302090425bbc14c3d9ae1c1cfc78e0bfcccfc1fb5dca5b195c6f8cfbe2d85e4071b71317aa2b0b65c391f82502');

var k    = mpw('0001592048516ccd793c7b863b00985fdba71c3d1edf449f667ac0d05ef37d15a94ad3282f29f7e9fd9491872f931354a1ccfa39');
var msg  = "Example of ECDSA with K-409"

var rx   = mpw('00ef421a230aa8b471939a77bf4f2c64fc0b4caa39edcd06337a9115af89df725e3c748ae018320e89d77abcd3aa13a6ccf10c34');
var e    = mpw('b18623fe7d6b79c3947651cf64a066400f89dc989d07bfd8c1aaf75e3c9b3d48fc457204168de4ed4eca8e240e009b95');
var kinv = mpw('0064d066b74c9771a843f341a1853f0520696aa57338c21c4a839507b5ee65cb98a7f87c8e53037c02980cf5185300b709901d59');

var r    = mpw('006f421a230aa8b471939a77bf4f2c64fc0b4caa39edcd06337a92b62bd70a883dfc65c68a9ad33aa5efb061884d8fedecd2ac65');
ack('r.toString(10)', '287296502609903755047276062317671877380342987214246771450934811188142631858192434973293671444180485746280616241641144101989');
var s    = mpw('00425f2ff9cbbf1b9e3fc17c4b66303622d7749047373cc9f919758cd88420c4cd0fdf14b819a4ada9961c3e6095000467c2f823');
ack('s.toString(10)', '171388639085828986597824337788600168526139265051294280998358619335390078299394527527443716590351871100863295063908700846115');

var w    = mpw(s, n, "invm");
var u1   = mpw('007fc6bdb66a9d2a23bf69d4f96bb3e7e24e365b45d111c666747a42b39275ed0ba4f2866d3723dd13851a45208c864525d5f530');
var u2   = mpw('0005e16d63457088b1f3012e844c852e23c9f1225aa569a883de8a5828dde30917d23c4d807a371aa1fc5db25149abbcd53f7558');
var rx   = mpw('00ef421a230aa8b471939a77bf4f2c64fc0b4caa39edcd06337a9115af89df725e3c748ae018320e89d77abcd3aa13a6ccf10c34');
var v    = mpw('006f421a230aa8b471939a77bf4f2c64fc0b4caa39edcd06337a92b62bd70a883dfc65c68a9ad33aa5efb061884d8fedecd2ac65');

// ##############################################################
// Curve B-409
// ##############################################################
print("===== B-409");
var n    = mpw('010000000000000000000000000000000000000000000000000001e2aad6a612f33307be5fa47c3c9e052f838164cd37d9a21173');
ack('n.toString(10)', '661055968790248598951915308032771039828404682964281219284648798304157774827374805208143723762179110965979867288366567526771');

var d    = mpw('4af896db379abdf70c8fade9ebd28cd530f2ecb336b4de84bd6e065ef56c8c548c532d00fa55ca8acf3e98adbca9f78d241b');
var qx   = mpw('01951c5e41607e9317f247d49a389d0e120f479d47737543098ae5e1bb62bd59de70e1c584ae655c702d39dd4f7883e1876c4a9b');
var qy   = mpw('016b16b98a3353d75beb4d3576c64568ba381463cf77d4aeb85218d2d546e7a1ee3ab9316d8c7df00d155b7891b2c0bf4b5e942e');

var k    = mpw('6a0b81d9320b5c305d730b1c1e74b03fafb88a7ec355990b75f9b70e8532433296a32492cba06f8583d5b19c5b8c5d6d07ec');
var msg  = "Example of ECDSA with B-409"

var rx   = mpw('01f3e4da3101c64239d76831995c0ec1e56ce4690c42ddd53dbf3ef725d819df090b8632f327499b5b99c280d7f410cd7105c8db');
var e    = mpw('4bbf1bc0ddf9d3b7bfe21fc68642b3e5508ca6ba4d365c1d00abbfabdb0f3ec2b0be995ae803de47d0880bf192649edc');
var kinv = mpw('00a202ea455d0e1a5ef09054b39259c768db76ffd1a77b6281fc7056a4a23a1012cdd604e4d7993e0d9edd422defd782c1225a1a');

var r    = mpw('00f3e4da3101c64239d76831995c0ec1e56ce4690c42ddd53dbf3d147b0173cc15d87e749382cd5ebd9492fd568f43959763b768');
ack('r.toString(10)', '629795133852997848663164416330632008108488131029367711198400899015295569161990628452056613359116571349413663774547185481576');
var s    = mpw('00292fa994dc6ea367236ad73956dbc1eb62b8779df438165407141587e3feed883741cdf5542f255bebc57b9d0c87ad403b8eab');
ack('s.toString(10)', '106353011790980050510402230388010919168837770971007460637281001297135413850335587267331709367101624723953375840680931987115');

var w    = mpw(s, n, "invm");
var u1   = mpw('0033d4f3ce2b7d4f2548f54c92420a8c30e6b02d768cbd6ca4ab020e88be82eefe4d58a9b4db2854561ead8975e1a58fb45c2117');
var u2   = mpw('00eb0671192b38cbc911cade604d9e048c7e660af3c7085d54839c1b7549691e5f7fffdab4003ade05208775d4a0287e448740af');
var rx   = mpw('01f3e4da3101c64239d76831995c0ec1e56ce4690c42ddd53dbf3ef725d819df090b8632f327499b5b99c280d7f410cd7105c8db');
var v    = mpw('00f3e4da3101c64239d76831995c0ec1e56ce4690c42ddd53dbf3d147b0173cc15d87e749382cd5ebd9492fd568f43959763b768');

// ##############################################################
// Curve K-571
// ##############################################################
print("===== K-571");
var n    = mpw('020000000000000000000000000000000000000000000000000000000000000000000000131850e1f19a63e4b391a8db917f4138b630d84be5d639381e91deb45cfe778f637c1001');
ack('n.toString(10)', '1932268761508629172347675945465993672149463664853217499328617625725759571144780212268133978522706711834706712800825351461273674974066617311929682421617092503555733685276673');

var d    = mpw('01042fde4d66e76725e7957e208a85cf23bc0d5b8d001b36aeafb34ad1104004ccf99afdfabca11585a4eb5263c87052cb05ef7fb39d9e5f6cf495e9dce5840b83fbc5ff3ad8b2f3');
var qx   = mpw('04d9cfe0a7338fea703e007f5d10babd2df3f319b47df1e23c4f7e5abf5014c1390b78f117e6af8258a48f56acb9faac788530b5ccdb1ab7e9390ec5dd7a39d5eeaf6c41bf50ac76');
var qy   = mpw('064732c504f81dc5f9b0e882b6da46e124e8241358f077896d25ecf028ad0e6011993c85e68741a07d7817c400cf94b1a3f524f48668b5b970972618616db4362a769d16cac34bf0');

var k    = mpw('0104063c918de62000a3fd87775d8d71398722bd153b8ea33060349c5fe6cf6cb4677957e6ba50d3c8a8b5182b9cf962954a6bbb5f7868b88e5778aa62a0cf8002bf19da3049ff51');
var msg  = "Example of ECDSA with K-571"

var rx   = mpw('0668758c313fbf1945f775d95b25866dbc8d001d9c7ef4ffa53774e8c68927a52707f034cebb3a712be6d164f2ffe1897b069f8fbaec4650b5372ddfa31cdeccfa78569197cf50f1');
var e    = mpw('46e30abdd459269cf19af76900af7131b4f639227414719ebebe548ccd4026b75c1f52618547aa3821f29fcf685e33640bd9e29f7fe46817627d4139eee411c6');
var kinv = mpw('0075a02beaea51660a1d05053b173c9c6dccaeba80f72ca08dec3c32e2a47cbc5674998ad19fc77b6615bfbed482451af7fd9b416b64b0e8f8429449fa9685f2b9c8dc2108544d98');

var r    = mpw('0068758c313fbf1945f775d95b25866dbc8d001d9c7ef4ffa53774e8c68927a52707f034957247cb5717a5b6d84ae6f6c688dbe59859bd6d03b48237476742afe37cefe36d5b20ee');
ack('r.toString(10)', '394224984077781794110496797090243013264310697694235133152929357615210591117311039789635553626060843213156329295090794526694846023847676866900261239702547152597222099198190');
var s    = mpw('00f5c8e975a1da26b2e0acd4f486c4a4231c1e29ee8ecfa03a697761498f5d53ff898afc16945975d328d34a8dcdc6d0613c73fe4f516f5685e23716fe105ed3472c3358b5b4ad41');
ack('s.toString(10)', '927582646247034536177419741157642737320661633591180227914401573622207818585635957265237189528319320264021523961000220604458498430712577999892194455415491822501994555551041');

var w    = mpw(s, n, "invm");
var u1   = mpw('0121b1f8919c4ed41d4676bc6f417dbf7f93c2314c74796642d1b17df4fd04c114ae293707913e2ebb65e173d52c3678c8366eb3c94e7b7ecee53f6b0b625ac62b924e3d6b975ff3');
var u2   = mpw('01afbf8d1d31511413eb6846b32ced90d38e515ff26cb8334ef619677794cc3b92da014e9b0e39bf49d8933d3e998145594128612aeccbfc7004d08777fd90bc197d13dc6238bf0b');
var rx   = mpw('0668758c313fbf1945f775d95b25866dbc8d001d9c7ef4ffa53774e8c68927a52707f034cebb3a712be6d164f2ffe1897b069f8fbaec4650b5372ddfa31cdeccfa78569197cf50f1');
var v    = mpw('0068758c313fbf1945f775d95b25866dbc8d001d9c7ef4ffa53774e8c68927a52707f034957247cb5717a5b6d84ae6f6c688dbe59859bd6d03b48237476742afe37cefe36d5b20ee');

// ##############################################################
// Curve B-571
// ##############################################################
print("===== B-571");
var n    = mpw('03ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffe661ce18ff55987308059b186823851ec7dd9ca1161de93d5174d66e8382e9bb2fe84e47');
ack('n.toString(10)', '3864537523017258344695351890931987344298927329706434998657235251451519142289560424536143999389415773083133881121926944486246872462816813070234528288303332411393191105285703');

var d    = mpw('0003cce32ba00dc3a7eefc9eb6f6cbfb9c5f0e57f532b7ee6826d4a75d0e756fd533900f2cea8cccc50ee22ce079398d371ec4a2ec45cc24b887606678e9c67453d0f5e768e9d752');
var qx   = mpw('00310ead2bef3ddb84f9fc1777a7ee179ffcb77aab497bdc00e290597a5fce306fe419d2f1f208e54850516526db8e03b0519bef60e3a3cc8198fbca8c469acfe46ab70d5c31874f');
var qy   = mpw('0373ce6ea68f55d1501d5203aca03c5ab709a337a8e03b03838f47c06762065fbdd08a102a08c42ff1760145be54d8606d326ea22a54df034fac30988049820beba2b0af9f6404b3');

var k    = mpw('01062ff6d95c49ac610cb9af9900d59c288669c3626306db7eb7f119499ba1d54cb6be888758caada69952675cc0cd4999176879bc302a7e2a5118dfc7d538da114ccac2baf9ad08');
var msg  = "Example of ECDSA with B-571"

var rx   = mpw('00e17447e422a2c0085190354ac149210c1137a92c10f9b14d225e6510da76b19ef44d39390dd9d808c9dfbae67d9cf0e7be79a9e72fa8fa1dfe89f43fb6d093a6ceb30e136eaba3');
var e    = mpw('60edef7da1d9d35a77d1da441ebb63454501f2bb1af8a4c49d281298e5f4d4e6b7e9bce4b66b2512bf590288b57915bfd3aed2c2604a5c574107df674faf9779');
var kinv = mpw('028c3ae12bd7922b837fe05066136bb45eda0337d39e31c3d4b9164c93f17fd7549471eb0385fccea8768dd6e5925adf1d1888826ff6aecc48f3db39905d46a644eb2f0c3a3dcbbd');

var r    = mpw('00e17447e422a2c0085190354ac149210c1137a92c10f9b14d225e6510da76b19ef44d39390dd9d808c9dfbae67d9cf0e7be79a9e72fa8fa1dfe89f43fb6d093a6ceb30e136eaba3');
ack('r.toString(10)', '850855762239502026591058023510563289273348857487181767834743944602362175979470988825265490247148142789747022832891719663177918797433599028444576028042439884754109942639523');
var s    = mpw('00624e852c7b6a061b4b39a907b518200fed380fd692c9ae147c5250f4852434afa24a1ca5062c48e5fc217fb689ab3a7266b5522f176f32a5cea22d6bf2820d349e4193bcee75c8');
ack('s.toString(10)', '371005865765721334746413249075321341103084481747665324048935832930246035334494733931256416306196179464892337407155249561524652370280994639556574699020735912775043796334024');

var w    = mpw(s, n, "invm");
var u1   = mpw('01a73be1676983c3b1583a8504c46e290ac2ffc3fd7866e6242ae2a3dc40f362e2b799173698175f7f094e9fa06ee09b3aa7d284549b8fcf4671051b829aa38ef59d066b3554de19');
var u2   = mpw('010df449e4fb576079a7d63a59f000da5e24da7800fa29bf321084ed549834b1ed95e5baf48fd65830f2bd0957b6f709dac14af90be29993fe428bd7df93206cc9ed9296230b8657');
var rx   = mpw('00e17447e422a2c0085190354ac149210c1137a92c10f9b14d225e6510da76b19ef44d39390dd9d808c9dfbae67d9cf0e7be79a9e72fa8fa1dfe89f43fb6d093a6ceb30e136eaba3');
var v    = mpw('00e17447e422a2c0085190354ac149210c1137a92c10f9b14d225e6510da76b19ef44d39390dd9d808c9dfbae67d9cf0e7be79a9e72fa8fa1dfe89f43fb6d093a6ceb30e136eaba3');

if (loglvl) print("<-- Mpw.js");

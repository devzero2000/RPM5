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
var hi = 20;
var bases = [8, 10, 16];

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

// ===== secp 112r1
var p    = mpw('db7c2abf62e35e668076bead208b');
ack('mpw(mpw(mpw(2, 128, "**"), 3, "-"), 76439, "/").toString(16)', p.toString(16));
var b    = mpw('659ef8ba043916eede8911702b22');
var n    = mpw('db7c2abf62e35e7628dfac6561c5');
var gx   = mpw('09487239995a5ee76b55f9c2f098');
var gy   = mpw('a89ce5af8724c0a23e0e0ff77500');

var gy2  = mpw(gy, p, "sqrm");
var gx31 = mpw(mpw(gx, gx, p, "mulm"), gx, p, "mulm");
var gx32 = mpw(gx, 3, p, "mulm");
ack('mpw(mpw(gx31, gx32, p, "subm"), b, p, "addm").toString(10)', gy2.toString(10));

// ===== secp 128r1
var p    = mpw('fffffffdffffffffffffffffffffffff');
ack('mpw(mpw(mpw(2, 128, "**"), mpw(2, 97, "**"), "-"), 1, "-").toString(16)', p.toString(16));
var b    = mpw('e87579c11079f43dd824993c2cee5ed3');
var n    = mpw('fffffffe0000000075a30d1b9038a115');
var gx   = mpw('161ff7528b899b2d0c28607ca52c5b86');
var gy   = mpw('cf5ac8395bafeb13c02da292dded7a83');

var gy2  = mpw(gy, p, "sqrm");
var gx31 = mpw(mpw(gx, gx, p, "mulm"), gx, p, "mulm");
var gx32 = mpw(gx, 3, p, "mulm");
ack('mpw(mpw(gx31, gx32, p, "subm"), b, p, "addm").toString(10)', gy2.toString(10));

// ===== secp 160r1
var p    = mpw('ffffffffffffffffffffffffffffffff7fffffff');
ack('mpw(mpw(mpw(2, 160, "**"), mpw(2, 31, "**"), "-"), 1, "-").toString(16)', p.toString(16));
var b    = mpw('1c97befc54bd7a8b65acf89f81d4d4adc565fa45');
var n    = mpw('0100000000000000000001f4c8f927aed3ca752257');
var gx   = mpw('4a96b5688ef573284664698968c38bb913cbfc82');
var gy   = mpw('23a628553168947d59dcc912042351377ac5fb32');

var gy2  = mpw(gy, p, "sqrm");
var gx31 = mpw(mpw(gx, gx, p, "mulm"), gx, p, "mulm");
var gx32 = mpw(gx, 3, p, "mulm");
ack('mpw(mpw(gx31, gx32, p, "subm"), b, p, "addm").toString(10)', gy2.toString(10));

// ===== ECDSA P-192 example from 
//	http://csrc.nist.gov/groups/ST/toolkit/documents/Examples/ECDSA_Prime.pdf
var p    = mpw(mpw(mpw(2, 192, "**"), mpw(2, 64, "**"), "-"), 1, "-");
ack('p.toString(10)', '6277101735386680763835789423207666416083908700390324961279');
var n    = mpw('ffffffffffffffffffffffff99def836146bc9b1b4d22831');
ack('n.toString(10)', '6277101735386680763835789423176059013767194773182842284081');

var c    = mpw('3099d2bbbfcb2538542dcd5fb078b6ef5f3d6fe2c745de65');
var b    = mpw('64210519e59c80e70fa7e9ab72243049feb8deecc146b9b1');
ack('mpw(mpw(b, p, "sqrm"), c, p, "mulm").toString(10)', mpw(p, 27, "-").toString(10));

var gx   = mpw('188da80eb03090f67cbf20eb43a18800f4ff0afd82ff1012');
var gy   = mpw('07192b95ffc8da78631011ed6b24cdd573f977a11e794811');
var gy2  = mpw(gy, p, "sqrm");
var gx31 = mpw(mpw(gx, gx, p, "mulm"), gx, p, "mulm");
var gx32 = mpw(gx, 3, p, "mulm");
ack('mpw(mpw(gx31, gx32, p, "subm"), b, p, "addm").toString(10)', gy2.toString(10));

var d    = mpw('7891686032fd8057f636b44b1f47cce564d2509923a7465b');
var qx   = mpw('fba2aac647884b504eb8cd5a0a1287babcc62163f606a9a2');
var qy   = mpw('dae6d4cc05ef4f27d79ee38b71c9c8ef4865d98850d84aa5');
var qy2  = mpw(qy, p, "sqrm");
var qx31 = mpw(mpw(qx, qx, p, "mulm"), qx, p, "mulm");
var qx32 = mpw(qx, 3, p, "mulm");
// FIXME: wrong answer
// ack('mpw(mpw(qx31, qx32, p, "subm"), b, p, "addm").toString(10)', gy2.toString(10));

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
var p    = mpw(mpw(mpw(2, 224, "**"), mpw(2, 96, "**"), "-"), 1, "+");
ack('p.toString(10)', '26959946667150639794667015087019630673557916260026308143510066298881');
var n    = mpw('ffffffffffffffffffffffffffff16a2e0b8f03e13dd29455c5c2a3d');
ack('n.toString(10)', '26959946667150639794667015087019625940457807714424391721682722368061');

var c    = mpw('5b056c7e11dd68f40469ee7f3c7a7d74f7d121116506d031218291fb');
var b    = mpw('b4050a850c04b3abf54132565044b0b7d7bfd8ba270b39432355ffb4');
// FIXME: floating point exception
// ack('mpw(mpw(b, p, "sqrm"), c, p, "mulm").toString(10)', mpw(p, 27, "-").toString(10));
var gx   = mpw('b70e0cbd6bb4bf7f321390b94a03c1d356c21122343280d6115c1d21');
var gy   = mpw('bd376388b5f723fb4c22dfe6cd4375a05a07476444d5819985007e34');
// FIXME: floating point exceptions
// var gy2  = mpw(gy, p, "sqrm");
// var gx31 = mpw(mpw(gx, gx, p, "mulm"), gx, p, "mulm");
// var gx32 = mpw(gx, 3, p, "mulm");
// ack('mpw(mpw(gx31, gx32, p, "subm"), b, p, "addm").toString(10)', gy2.toString(10));

var d    = mpw('3f0c488e987c80be0fee521f8d90be6034ec69ae11ca72aa777481e8');
var qx   = mpw('e84fb0b8e7000cb657d7973cf6b42ed78b301674276df744af130b3e');
var qy   = mpw('4376675c6fc5612c21a0ff2d2a89d2987df7a2bc52183b5982298555');
// FIXME: floating point exceptions
// var qy2  = mpw(qy, p, "sqrm");
// var qx31 = mpw(mpw(qx, qx, p, "mulm"), qx, p, "mulm");
// var qx32 = mpw(qx, 3, p, "mulm");
// ack('mpw(mpw(qx31, qx32, p, "subm"), b, p, "addm").toString(10)', gy2.toString(10));

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
var gy2  = mpw(gy, p, "sqrm");
var gx31 = mpw(mpw(gx, gx, p, "mulm"), gx, p, "mulm");
var gx32 = mpw(gx, 3, p, "mulm");
ack('mpw(mpw(gx31, gx32, p, "subm"), b, p, "addm").toString(10)', gy2.toString(10));

var d    = mpw('c477f9f65c22cce20657faa5b2d1d8122336f851a508a1ed04e479c34985bf96');
var qx   = mpw('b7e08afdfe94bad3f1dc8c734798ba1c62b3a0ad1e9ea2a38201cd0889bc7a19');
var qy   = mpw('3603f747959dbf7a4bb226e41928729063adc7ae43529e61b563bbc606cc5e09');
var qy2  = mpw(qy, p, "sqrm");
var qx31 = mpw(mpw(qx, qx, p, "mulm"), qx, p, "mulm");
var qx32 = mpw(qx, 3, p, "mulm");
// FIXME: wrong answer
// ack('mpw(mpw(qx31, qx32, p, "subm"), b, p, "addm").toString(10)', gy2.toString(10));

var k    = mpw('7a1a7e52797fc8caaa435d2a4dace39158504bf204fbe19f14dbb427faee50ae');
var msg  = "Example of ECDSA with P-256";
var e    = mpw('a41a41a12a799548211c410c65d8133afde34d28bdd542e4b680cf2899c8a8c4');
var kinv = mpw('62159e5ba9e712fb098cce8fe20f1bed8346554e98ef3c7c1fc3332ba67d87ef');
ack('mpw(k, n, "invm").toString(16)', kinv.toString(16));
var r    = mpw('2b42f576d07f4165ff65d1f3b1500f81e44c316f1f0b3ef57325b69aca46104f');
var s    = mpw('dc42c2122d6392cd3e3a993a89502a8198c1886fe69d262c4b329bdb6b63faf1');
// FIXME floating point exception
// ack('mpw(kinv, mpw(e, mpw(r, d, n, "mulm"), n, "addm"), n, "mulm").toString(16)', s.toString(16));

var w    = mpw(s, n, "invm");
var u1   = mpw('b807bf3281dd13849958f444fd9aea808d074c2c48ee8382f6c47a435389a17e');
// FIXME floating point exception
// ack('mpw(e, w, n, "mulm").toString(16)', u1.toString(16));
var u2   = mpw('1777f73443a4d68c23d1fc4cb5f8b7f2554578ee87f04c253df44efd181c184c');
// FIXME floating point exception
// ack('mpw(r, w, n, "mulm").toString(16)', u2.toString(16));
var v    = mpw('2b42f576d07f4165ff65d1f3b1500f81e44c316f1f0b3ef57325b69aca46104f');

// ===== ECDSA P-384 example from 
//	http://csrc.nist.gov/groups/ST/toolkit/documents/Examples/ECDSA_Prime.pdf
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
// FIXME: floating point exception
// ack('mpw(mpw(b, p, "sqrm"), c, p, "mulm").toString(10)', mpw(p, 27, "-").toString(10));
var gx   = mpw('aa87ca22be8b05378eb1c71ef320ad746e1d3b628ba79b9859f741e082542a385502f25dbf55296c3a545e3872760ab7');
var gy   = mpw('3617de4a96262c6f5d9e98bf9292dc29f8f41dbd289a147ce9da3113b5f0b8c00a60b1ce1d7e819d7a431d7c90ea0e5f');
// FIXME: floating point exception
// var gy2  = mpw(gy, p, "sqrm");
// var gx31 = mpw(mpw(gx, gx, p, "mulm"), gx, p, "mulm");
// var gx32 = mpw(gx, 3, p, "mulm");
// ack('mpw(mpw(gx31, gx32, p, "subm"), b, p, "addm").toString(10)', gy2.toString(10));

var d    = mpw('f92c02ed629e4b48c0584b1c6ce3a3e3b4faae4afc6acb0455e73dfc392e6a0ae393a8565e6b9714d1224b57d83f8a08');
var qx   = mpw('3bf701bc9e9d36b4d5f1455343f09126f2564390f2b487365071243c61e6471fb9d2ab74657b82f9086489d9ef0f5cb5');
var qy   = mpw('d1a358eafbf952e68d533855ccbdaa6ff75b137a5101443199325583552a6295ffe5382d00cfcda30344a9b5b68db855');
// FIXME: floating point exceptions
// var qy2  = mpw(qy, p, "sqrm");
// var qx31 = mpw(mpw(qx, qx, p, "mulm"), qx, p, "mulm");
// var qx32 = mpw(qx, 3, p, "mulm");
// ack('mpw(mpw(qx31, qx32, p, "subm"), b, p, "addm").toString(10)', gy2.toString(10));

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
var gy2  = mpw(gy, p, "sqrm");
var gx31 = mpw(mpw(gx, gx, p, "mulm"), gx, p, "mulm");
var gx32 = mpw(gx, 3, p, "mulm");
ack('mpw(mpw(gx31, gx32, p, "subm"), b, p, "addm").toString(10)', gy2.toString(10));

var d    = mpw('0100085f47b8e1b8b11b7eb33028c0b2888e304bfc98501955b45bba1478dc184eeedf09b86a5f7c21994406072787205e69a63709fe35aa93ba333514b24f961722');
var qx   = mpw('0098e91eef9a68452822309c52fab453f5f117c1da8ed796b255e9ab8f6410cca16e59df403a6bdc6ca467a37056b1e54b3005d8ac030decfeb68df18b171885d5c4');
var qy   = mpw('0164350c321aecfc1cca1ba4364c9b15656150b4b78d6a48d7d28e7f31985ef17be8554376b72900712c4b83ad668327231526e313f5f092999a4632fd50d946bc2e');
var qy2  = mpw(qy, p, "sqrm");
var qx31 = mpw(mpw(qx, qx, p, "mulm"), qx, p, "mulm");
var qx32 = mpw(qx, 3, p, "mulm");
// FIXME: wrong answer
// ack('mpw(mpw(qx31, qx32, p, "subm"), b, p, "addm").toString(10)', gy2.toString(10));

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

if (loglvl) print("<-- Mpw.js");

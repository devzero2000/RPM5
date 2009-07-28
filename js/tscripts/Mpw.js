if (loglvl) print("--> Mpw.js");

var mpw = new Mpw();
ack("typeof mpw;", "object");
ack("mpw instanceof Mpw;", true);
// ack("mpw.debug = 1;", 1);
// ack("mpw.debug = 0;", 0);

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

if (loglvl) print("<-- Mpw.js");

if (loglvl) print("--> Mpw.js");

var mpw = new Mpw();
ack("typeof mpw;", "object");
ack("mpw instanceof Mpw;", true);
// ack("mpw.debug = 1;", 1);
// ack("mpw.debug = 0;", 0);

var wa = new Mpw('0000000987654321');
ack('wa.toString(16)', '0x987654321');
ack('wa.toString(10)', '40926266145')
ack('wa.toString(8)', '0460731241441')
ack('wa.toString(2)', '2#100110000111011001010100001100100001')

var wb = new Mpw(0x10);
ack('wb.toString(16)', '0x10');
var wc = new Mpw('0fedcba000000000');
ack('wc.toString(16)', '0xfedcba000000000');

var x = new Mpw(2*3*5*19);
var y = new Mpw(7*11*13*19);
ack('mpw.gcd(x, y).toString(10)', '19');

var x = new Mpw(4);
var m = new Mpw(15);
ack('mpw.invm(x, m).toString(10)', '4');

var x = new Mpw(4);
var m = new Mpw(15);
ack('mpw.sqrm(x, m).toString(10)', '1');

var x = new Mpw(4);
var y = new Mpw(13);
var m = new Mpw(15);
ack('mpw.addm(x, y, m).toString(10)', '2');

var x = new Mpw(23);
var y = new Mpw(6);
var m = new Mpw(15);
ack('mpw.subm(x, y, m).toString(10)', '2');

var x = new Mpw(4);
var y = new Mpw(13);
var m = new Mpw(15);
ack('mpw.mulm(x, y, m).toString(10)', '7');

var x = new Mpw(13);
var y = new Mpw(4);
var m = new Mpw(15);
ack('mpw.powm(x, y, m).toString(10)', '1');

if (loglvl) print("<-- Mpw.js");

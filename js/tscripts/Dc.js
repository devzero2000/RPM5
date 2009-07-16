if (loglvl) print("--> Dc.js");

const PGPHASHALGO_MD5             =  1;   /*!< MD5 */
const PGPHASHALGO_SHA1            =  2;   /*!< SHA-1 */
const PGPHASHALGO_RIPEMD160       =  3;   /*!< RIPEMD-160 */
const PGPHASHALGO_MD2             =  5;   /*!< MD2 */
const PGPHASHALGO_TIGER192        =  6;   /*!< TIGER-192 */
const PGPHASHALGO_HAVAL_5_160     =  7;   /*!< HAVAL-5-160 */
const PGPHASHALGO_SHA256          =  8;   /*!< SHA-256 */
const PGPHASHALGO_SHA384          =  9;   /*!< SHA-384 */
const PGPHASHALGO_SHA512          = 10;   /*!< SHA-512 */
const PGPHASHALGO_SHA224          = 11;   /*!< SHA-224 */

const PGPHASHALGO_MD4             = 104;  /*!< (private) MD4 */
const PGPHASHALGO_RIPEMD128       = 105;  /*!< (private) RIPEMD-128 */
const PGPHASHALGO_CRC32           = 106;  /*!< (private) CRC-32 */
const PGPHASHALGO_ADLER32         = 107;  /*!< (private) ADLER-32 */
const PGPHASHALGO_CRC64           = 108;  /*!< (private) CRC-64 */
const PGPHASHALGO_JLU32           = 109;  /*!< (private) Jenkins lookup3.c */

const PGPHASHALGO_RIPEMD256       = 111;  /*!< (private) RIPEMD-256 */
const PGPHASHALGO_RIPEMD320       = 112;  /*!< (private) RIPEMD-320 */
const PGPHASHALGO_SALSA10         = 113;  /*!< (private) SALSA-10 */
const PGPHASHALGO_SALSA20         = 114;  /*!< (private) SALSA-20 */

const PGPHASHALGO_SKEIN256        = 115;  /*!< (private) SKEIN-256 */
const PGPHASHALGO_SKEIN512        = 116;  /*!< (private) SKEIN-512 */
const PGPHASHALGO_SKEIN1024       = 117;  /*!< (private) SKEIN-1024 */

const RPMDIGEST_FLAGS_128 =  128;
const RPMDIGEST_FLAGS_160 =  160;
const RPMDIGEST_FLAGS_224 =  224;
const RPMDIGEST_FLAGS_256 =  256;
const RPMDIGEST_FLAGS_384 =  384;
const RPMDIGEST_FLAGS_512 =  512;
const RPMDIGEST_FLAGS_1024= 1024;

var dc = new Dc();
ack("typeof dc;", "object");
ack("dc instanceof Dc;", true);
ack("dc.debug = 1;", 1);
ack("dc.debug = 0;", 0);

var str = 'abc';

var md2 = new Dc(PGPHASHALGO_MD2);
ack("md2(str);", 'da853b0d3f88d99b30283a69e6ded6bb');
ack("md2.init(PGPHASHALGO_MD2);", true);
ack("md2.algo", PGPHASHALGO_MD2);
ack("md2.asn1", "3020300c06082a864886f70d020205000410");
ack("md2.name", "MD2");
ack("md2.update(str);", true);
ack("md2.fini();", 'da853b0d3f88d99b30283a69e6ded6bb');

var md4 = new Dc(PGPHASHALGO_MD4);
ack("md4(str);", 'a448017aaf21d8525fc10ae87aa6729d');
ack("md4.init(PGPHASHALGO_MD4);", true);
ack("md4.algo", PGPHASHALGO_MD4);
ack("md4.asn1", null);
ack("md4.name", "MD4");
ack("md4.update(str);", true);
ack("md4.fini();", 'a448017aaf21d8525fc10ae87aa6729d');

var md5 = new Dc(PGPHASHALGO_MD5);
ack("md5(str);", '900150983cd24fb0d6963f7d28e17f72');
ack("md5.init(PGPHASHALGO_MD5);", true);
ack("md5.algo", PGPHASHALGO_MD5);
ack("md5.asn1", "3020300c06082a864886f70d020505000410");
ack("md5.name", "MD5");
ack("md5.update(str);", true);
ack("md5.fini();", '900150983cd24fb0d6963f7d28e17f72');

var sha1 = new Dc(PGPHASHALGO_SHA1);
ack("sha1(str);", 'a9993e364706816aba3e25717850c26c9cd0d89d');
ack("sha1.init(PGPHASHALGO_SHA1);", true);
ack("sha1.algo", PGPHASHALGO_SHA1);
ack("sha1.asn1", "3021300906052b0e03021a05000414");
ack("sha1.name", "SHA1");
ack("sha1.update(str);", true);
ack("sha1.fini();", 'a9993e364706816aba3e25717850c26c9cd0d89d');

var sha224 = new Dc(PGPHASHALGO_SHA224);
ack("sha224(str);", '23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7');
ack("sha224.init(PGPHASHALGO_SHA224);", true);
ack("sha224.algo", PGPHASHALGO_SHA224);
ack("sha224.asn1", "302d300d06096086480165030402040500041C");
ack("sha224.name", "SHA224");
ack("sha224.update(str);", true);
ack("sha224.fini();", '23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7');

var sha256 = new Dc(PGPHASHALGO_SHA256);
ack("sha256(str);", 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad');
ack("sha256.init(PGPHASHALGO_SHA256);", true);
ack("sha256.algo", PGPHASHALGO_SHA256);
ack("sha256.asn1", "3031300d060960864801650304020105000420");
ack("sha256.name", "SHA256");
ack("sha256.update(str);", true);
ack("sha256.fini();", 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad');

var sha384 = new Dc(PGPHASHALGO_SHA384);
ack("sha384(str);", 'cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7');
ack("sha384.init(PGPHASHALGO_SHA384);", true);
ack("sha384.algo", PGPHASHALGO_SHA384);
ack("sha384.asn1", "3041300d060960864801650304020205000430");
ack("sha384.name", "SHA384");
ack("sha384.update(str);", true);
ack("sha384.fini();", 'cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7');

var sha512 = new Dc(PGPHASHALGO_SHA512);
ack("sha512(str);", 'ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f');
ack("sha512.init(PGPHASHALGO_SHA512);", true);
ack("sha512.algo", PGPHASHALGO_SHA512);
ack("sha512.asn1", "3051300d060960864801650304020305000440");
ack("sha512.name", "SHA512");
ack("sha512.update(str);", true);
ack("sha512.fini();", 'ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f');

var salsa10 = new Dc(PGPHASHALGO_SALSA10);
ack("salsa10(str);", '699d0db4d264ce8ba8a1bf74734a29372b689d64c985aa5a037c6e11cbdf328270d58c699523ffc2d10bcc6f9ce167fb63528ea4ef0681fe8172297f9c92d72f');
ack("salsa10.init(PGPHASHALGO_SALSA10);", true);
ack("salsa10.algo", PGPHASHALGO_SALSA10);
ack("salsa10.asn1", null);
ack("salsa10.name", "SALSA10");
ack("salsa10.update(str);", true);
ack("salsa10.fini();", '699d0db4d264ce8ba8a1bf74734a29372b689d64c985aa5a037c6e11cbdf328270d58c699523ffc2d10bcc6f9ce167fb63528ea4ef0681fe8172297f9c92d72f');

var salsa20 = new Dc(PGPHASHALGO_SALSA20);
ack("salsa20(str);", 'fc4f496bb7a2fbe942d170dd42ebaaf6aec7fd9d6563d0563f81ffed6e6ff5cb595237462577ab41efd8c3815f20205a905ad5fd647de1ce2d37894e6671d6a7');
ack("salsa20.init(PGPHASHALGO_SALSA20);", true);
ack("salsa20.algo", PGPHASHALGO_SALSA20);
ack("salsa20.asn1", null);
ack("salsa20.name", "SALSA20");
ack("salsa20.update(str);", true);
ack("salsa20.fini();", 'fc4f496bb7a2fbe942d170dd42ebaaf6aec7fd9d6563d0563f81ffed6e6ff5cb595237462577ab41efd8c3815f20205a905ad5fd647de1ce2d37894e6671d6a7');

var ripemd128 = new Dc(PGPHASHALGO_RIPEMD128);
ack("ripemd128(str);", 'c14a12199c66e4ba84636b0f69144c77');
ack("ripemd128.init(PGPHASHALGO_RIPEMD128);", true);
ack("ripemd128.algo", PGPHASHALGO_RIPEMD128);
ack("ripemd128.asn1", null);
ack("ripemd128.name", "RIPEMD128");
ack("ripemd128.update(str);", true);
ack("ripemd128.fini();", 'c14a12199c66e4ba84636b0f69144c77');

var ripemd160 = new Dc(PGPHASHALGO_RIPEMD160);
ack("ripemd160(str);", '8eb208f7e05d987a9b044a8e98c6b087f15a0bfc');
ack("ripemd160.init(PGPHASHALGO_RIPEMD160);", true);
ack("ripemd160.algo", PGPHASHALGO_RIPEMD160);
ack("ripemd160.asn1", "3021300906052b2403020105000414");
ack("ripemd160.name", "RIPEMD160");
ack("ripemd160.update(str);", true);
ack("ripemd160.fini();", '8eb208f7e05d987a9b044a8e98c6b087f15a0bfc');

var ripemd256 = new Dc(PGPHASHALGO_RIPEMD256);
ack("ripemd256(str);", 'afbd6e228b9d8cbbcef5ca2d03e6dba10ac0bc7dcbe4680e1e42d2e975459b65');
ack("ripemd256.init(PGPHASHALGO_RIPEMD256);", true);
ack("ripemd256.algo", PGPHASHALGO_RIPEMD256);
ack("ripemd256.asn1", null);
ack("ripemd256.name", "RIPEMD256");
ack("ripemd256.update(str);", true);
ack("ripemd256.fini();", 'afbd6e228b9d8cbbcef5ca2d03e6dba10ac0bc7dcbe4680e1e42d2e975459b65');

var ripemd320 = new Dc(PGPHASHALGO_RIPEMD320);
ack("ripemd320(str);", 'de4c01b3054f8930a79d09ae738e92301e5a17085beffdc1b8d116713e74f82fa942d64cdbc4682d');
ack("ripemd320.init(PGPHASHALGO_RIPEMD320);", true);
ack("ripemd320.algo", PGPHASHALGO_RIPEMD320);
ack("ripemd320.asn1", null);
ack("ripemd320.name", "RIPEMD320");
ack("ripemd320.update(str);", true);
ack("ripemd320.fini();", 'de4c01b3054f8930a79d09ae738e92301e5a17085beffdc1b8d116713e74f82fa942d64cdbc4682d');

var skein224 = new Dc(PGPHASHALGO_SKEIN256, RPMDIGEST_FLAGS_224);
ack("skein224('');", '50462a6805b74017ac85237077fb122a507911b737c9a66ff056a823');
ack("skein224.init(PGPHASHALGO_SKEIN256, RPMDIGEST_FLAGS_224);", true);
ack("skein224.algo", PGPHASHALGO_SKEIN256);
ack("skein224.asn1", null);
ack("skein224.name", "SKEIN256");
ack("skein224.update('');", true);
ack("skein224.fini();", '50462a6805b74017ac85237077fb122a507911b737c9a66ff056a823');

var skein256 = new Dc(PGPHASHALGO_SKEIN256);
ack("skein256('');", 'bc2763f707e262b80e0313791543a7ab0a4b6cd083270afb2fce4272e1bb0aa9');
ack("skein256.init(PGPHASHALGO_SKEIN256);", true);
ack("skein256.algo", PGPHASHALGO_SKEIN256);
ack("skein256.asn1", null);
ack("skein256.name", "SKEIN256");
ack("skein256.update('');", true);
ack("skein256.fini();", 'bc2763f707e262b80e0313791543a7ab0a4b6cd083270afb2fce4272e1bb0aa9');

var skein384 = new Dc(PGPHASHALGO_SKEIN512, RPMDIGEST_FLAGS_384);
ack("skein384('');", 'fa35c62fad08435e2727361228a0da5a72b14c2d7da222dc1f5142ae91f5da6c85c441f96888fccf518be2084cc495f4');
ack("skein384.init(PGPHASHALGO_SKEIN512, RPMDIGEST_FLAGS_384);", true);
ack("skein384.algo", PGPHASHALGO_SKEIN512);
ack("skein384.asn1", null);
ack("skein384.name", "SKEIN512");
ack("skein384.update('');", true);
ack("skein384.fini();", 'fa35c62fad08435e2727361228a0da5a72b14c2d7da222dc1f5142ae91f5da6c85c441f96888fccf518be2084cc495f4');

var skein512 = new Dc(PGPHASHALGO_SKEIN512);
ack("skein512('');", 'd3f7263a09837f4ce5c8ef70a5ddffac7b92d6c2ace5a12265bd5b593260a3ff20d8b4b4c5494e945448b37abb1fc526f6b46089208fde938d7f23724c4bdfb7');
ack("skein512.init(PGPHASHALGO_SKEIN512);", true);
ack("skein512.algo", PGPHASHALGO_SKEIN512);
ack("skein512.asn1", null);
ack("skein512.name", "SKEIN512");
ack("skein512.update('');", true);
ack("skein512.fini();", 'd3f7263a09837f4ce5c8ef70a5ddffac7b92d6c2ace5a12265bd5b593260a3ff20d8b4b4c5494e945448b37abb1fc526f6b46089208fde938d7f23724c4bdfb7');

var skein1024 = new Dc(PGPHASHALGO_SKEIN1024);
ack("skein1024('');", '7bc3ce31c035df3c7a7d559bc9d9454f879f48497cc39e6d14e8190498455f396f45590405c4a15bd0ab29f5467d9132802f4354376ee864ad9f200e39a3d09b9ad06e9d0f656cf91ed9deac13a7a67a82ab983f52133129cac2a4dc13fd29c36ca6c72d6dcc6c82c66797f6b7607cb0e0f9006a27b83b60c59e4ba18de233e0');
ack("skein1024.init(PGPHASHALGO_SKEIN1024);", true);
ack("skein1024.algo", PGPHASHALGO_SKEIN1024);
ack("skein1024.asn1", null);
ack("skein1024.name", "SKEIN1024");
ack("skein1024.update('');", true);
ack("skein1024.fini();", '7bc3ce31c035df3c7a7d559bc9d9454f879f48497cc39e6d14e8190498455f396f45590405c4a15bd0ab29f5467d9132802f4354376ee864ad9f200e39a3d09b9ad06e9d0f656cf91ed9deac13a7a67a82ab983f52133129cac2a4dc13fd29c36ca6c72d6dcc6c82c66797f6b7607cb0e0f9006a27b83b60c59e4ba18de233e0');

var tiger192 = new Dc(PGPHASHALGO_TIGER192);
ack("tiger192(str);", '2aab1484e8c158f2bfb8c5ff41b57a525129131c957b5f93');
ack("tiger192.init(PGPHASHALGO_TIGER192);", true);
ack("tiger192.algo", PGPHASHALGO_TIGER192);
ack("tiger192.asn1", "3029300d06092b06010401da470c0205000418");
ack("tiger192.name", "TIGER192");
ack("tiger192.update(str);", true);
ack("tiger192.fini();", '2aab1484e8c158f2bfb8c5ff41b57a525129131c957b5f93');

var crc32 = new Dc(PGPHASHALGO_CRC32);
ack("crc32(str);", '352441c2');
ack("crc32.init(PGPHASHALGO_CRC32);", true);
ack("crc32.algo", PGPHASHALGO_CRC32);
ack("crc32.asn1", null);
ack("crc32.name", "CRC32");
ack("crc32.update(str);", true);
ack("crc32.fini();", '352441c2');

var crc64 = new Dc(PGPHASHALGO_CRC64);
ack("crc64(str);", '2cd8094a1a277627');
ack("crc64.init(PGPHASHALGO_CRC64);", true);
ack("crc64.algo", PGPHASHALGO_CRC64);
ack("crc64.asn1", null);
ack("crc64.name", "CRC64");
ack("crc64.update(str);", true);
ack("crc64.fini();", '2cd8094a1a277627');

var adler32 = new Dc(PGPHASHALGO_ADLER32);
ack("adler32(str);", '024d0127');
ack("adler32.init(PGPHASHALGO_ADLER32);", true);
ack("adler32.algo", PGPHASHALGO_ADLER32);
ack("adler32.asn1", null);
ack("adler32.name", "ADLER32");
ack("adler32.update(str);", true);
ack("adler32.fini();", '024d0127');

var jlu32 = new Dc(PGPHASHALGO_JLU32);
ack("jlu32(str);", '110255fd');
ack("jlu32.init(PGPHASHALGO_JLU32);", true);
ack("jlu32.algo", PGPHASHALGO_JLU32);
ack("jlu32.asn1", null);
ack("jlu32.name", "JLU32");
ack("jlu32.update(str);", true);
ack("jlu32.fini();", '110255fd');

if (loglvl) print("<-- Dc.js");

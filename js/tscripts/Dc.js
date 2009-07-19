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

const PGPHASHALGO_MD6_224	= 128+0;	/*!< (private) MD6-224 */
const PGPHASHALGO_MD6_256	= 128+1;	/*!< (private) MD6-256 */
const PGPHASHALGO_MD6_384	= 128+2;	/*!< (private) MD6-384 */
const PGPHASHALGO_MD6_512	= 128+3;	/*!< (private) MD6-512 */

const PGPHASHALGO_CUBEHASH_224	= 136+0;	/*!< (private) CUBEHASH-224 */
const PGPHASHALGO_CUBEHASH_256	= 136+1;	/*!< (private) CUBEHASH-256 */
const PGPHASHALGO_CUBEHASH_384	= 136+2;	/*!< (private) CUBEHASH-384 */
const PGPHASHALGO_CUBEHASH_512	= 136+3;	/*!< (private) CUBEHASH-512 */

const PGPHASHALGO_KECCAK_224	= 144+0;	/*!< (private) KECCAK-224 */
const PGPHASHALGO_KECCAK_256	= 144+1;	/*!< (private) KECCAK-256 */
const PGPHASHALGO_KECCAK_384	= 144+2;	/*!< (private) KECCAK-384 */
const PGPHASHALGO_KECCAK_512	= 144+3;	/*!< (private) KECCAK-512 */

const PGPHASHALGO_EDONR_224	= 152+0;	/*!< (private) EDONR-224 */
const PGPHASHALGO_EDONR_256	= 152+1;	/*!< (private) EDONR-256 */
const PGPHASHALGO_EDONR_384	= 152+2;	/*!< (private) EDONR-384 */
const PGPHASHALGO_EDONR_512	= 152+3;	/*!< (private) EDONR-512 */

const PGPHASHALGO_SKEIN_224	= 160+0;	/*!< (private) SKEIN-224 */
const PGPHASHALGO_SKEIN_256	= 160+1;	/*!< (private) SKEIN-256 */
const PGPHASHALGO_SKEIN_384	= 160+2;	/*!< (private) SKEIN-384 */
const PGPHASHALGO_SKEIN_512	= 160+3;	/*!< (private) SKEIN-512 */
const PGPHASHALGO_SKEIN_1024	= 160+4;	/*!< (private) SKEIN-1024 */

const PGPHASHALGO_BMW_224	= 168+0;	/*!< (private) BMW-224 */
const PGPHASHALGO_BMW_256	= 168+1;	/*!< (private) BMW-256 */
const PGPHASHALGO_BMW_384	= 168+2;	/*!< (private) BMW-384 */
const PGPHASHALGO_BMW_512	= 168+3;	/*!< (private) BMW-512 */

const PGPHASHALGO_SHABAL_224	= 176+0;	/*!< (private) SHABAL-224 */
const PGPHASHALGO_SHABAL_256	= 176+1;	/*!< (private) SHABAL-256 */
const PGPHASHALGO_SHABAL_384	= 176+2;	/*!< (private) SHABAL-384 */
const PGPHASHALGO_SHABAL_512	= 176+3;	/*!< (private) SHABAL-512 */

const PGPHASHALGO_BLAKE_224	= 184+0;	/*!< (private) BLAKE-224 */
const PGPHASHALGO_BLAKE_256	= 184+1;	/*!< (private) BLAKE-256 */
const PGPHASHALGO_BLAKE_384	= 184+2;	/*!< (private) BLAKE-384 */
const PGPHASHALGO_BLAKE_512	= 184+3;	/*!< (private) BLAKE-512 */

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

var skein224 = new Dc(PGPHASHALGO_SKEIN_224);
ack("skein224('');", '50462a6805b74017ac85237077fb122a507911b737c9a66ff056a823');
ack("skein224.init(PGPHASHALGO_SKEIN_224);", true);
ack("skein224.algo", PGPHASHALGO_SKEIN_224);
ack("skein224.asn1", null);
ack("skein224.name", "SKEIN256");
ack("skein224.update('');", true);
ack("skein224.fini();", '50462a6805b74017ac85237077fb122a507911b737c9a66ff056a823');

var skein256 = new Dc(PGPHASHALGO_SKEIN_256);
ack("skein256('');", 'bc2763f707e262b80e0313791543a7ab0a4b6cd083270afb2fce4272e1bb0aa9');
ack("skein256.init(PGPHASHALGO_SKEIN_256);", true);
ack("skein256.algo", PGPHASHALGO_SKEIN_256);
ack("skein256.asn1", null);
ack("skein256.name", "SKEIN256");
ack("skein256.update('');", true);
ack("skein256.fini();", 'bc2763f707e262b80e0313791543a7ab0a4b6cd083270afb2fce4272e1bb0aa9');

var skein384 = new Dc(PGPHASHALGO_SKEIN_384);
ack("skein384('');", 'fa35c62fad08435e2727361228a0da5a72b14c2d7da222dc1f5142ae91f5da6c85c441f96888fccf518be2084cc495f4');
ack("skein384.init(PGPHASHALGO_SKEIN_384);", true);
ack("skein384.algo", PGPHASHALGO_SKEIN_384);
ack("skein384.asn1", null);
ack("skein384.name", "SKEIN512");
ack("skein384.update('');", true);
ack("skein384.fini();", 'fa35c62fad08435e2727361228a0da5a72b14c2d7da222dc1f5142ae91f5da6c85c441f96888fccf518be2084cc495f4');

var skein512 = new Dc(PGPHASHALGO_SKEIN_512);
ack("skein512('');", 'd3f7263a09837f4ce5c8ef70a5ddffac7b92d6c2ace5a12265bd5b593260a3ff20d8b4b4c5494e945448b37abb1fc526f6b46089208fde938d7f23724c4bdfb7');
ack("skein512.init(PGPHASHALGO_SKEIN_512);", true);
ack("skein512.algo", PGPHASHALGO_SKEIN_512);
ack("skein512.asn1", null);
ack("skein512.name", "SKEIN512");
ack("skein512.update('');", true);
ack("skein512.fini();", 'd3f7263a09837f4ce5c8ef70a5ddffac7b92d6c2ace5a12265bd5b593260a3ff20d8b4b4c5494e945448b37abb1fc526f6b46089208fde938d7f23724c4bdfb7');

var skein1024 = new Dc(PGPHASHALGO_SKEIN_1024);
ack("skein1024('');", '7bc3ce31c035df3c7a7d559bc9d9454f879f48497cc39e6d14e8190498455f396f45590405c4a15bd0ab29f5467d9132802f4354376ee864ad9f200e39a3d09b9ad06e9d0f656cf91ed9deac13a7a67a82ab983f52133129cac2a4dc13fd29c36ca6c72d6dcc6c82c66797f6b7607cb0e0f9006a27b83b60c59e4ba18de233e0');
ack("skein1024.init(PGPHASHALGO_SKEIN_1024);", true);
ack("skein1024.algo", PGPHASHALGO_SKEIN_1024);
ack("skein1024.asn1", null);
ack("skein1024.name", "SKEIN1024");
ack("skein1024.update('');", true);
ack("skein1024.fini();", '7bc3ce31c035df3c7a7d559bc9d9454f879f48497cc39e6d14e8190498455f396f45590405c4a15bd0ab29f5467d9132802f4354376ee864ad9f200e39a3d09b9ad06e9d0f656cf91ed9deac13a7a67a82ab983f52133129cac2a4dc13fd29c36ca6c72d6dcc6c82c66797f6b7607cb0e0f9006a27b83b60c59e4ba18de233e0');

var blake224 = new Dc(PGPHASHALGO_BLAKE_224);
ack("blake224('');", '');
ack("blake224.init(PGPHASHALGO_BLAKE_224);", true);
ack("blake224.algo", PGPHASHALGO_BLAKE_224);
ack("blake224.asn1", null);
ack("blake224.name", "BLAKE");
ack("blake224.update(str);", true);
ack("blake224.fini();", '');

var blake256 = new Dc(PGPHASHALGO_BLAKE_256);
ack("blake256('');", '');
ack("blake256.init(PGPHASHALGO_BLAKE_256);", true);
ack("blake256.algo", PGPHASHALGO_BLAKE_256);
ack("blake256.asn1", null);
ack("blake256.name", "BLAKE");
ack("blake256.update(str);", true);
ack("blake256.fini();", '');

var blake384 = new Dc(PGPHASHALGO_BLAKE_384);
ack("blake384('');", 'e0820c066f522138d5cb3a5773dea16db434afa95e1c48e060de466928bb7044391b3ee77e2bbff6c0cf1e07a8295100');
ack("blake384.init(PGPHASHALGO_BLAKE_384);", true);
ack("blake384.algo", PGPHASHALGO_BLAKE_384);
ack("blake384.asn1", null);
ack("blake384.name", "BLAKE");
ack("blake384.update(str);", true);
ack("blake384.fini();", 'e22ddc662dd6c55ca5928fc954f55d5a288dcd69afd67971b90a383f0e2ef5d086dbca48186b6696dc041c78a4dd202c');

var blake512 = new Dc(PGPHASHALGO_BLAKE_512);
ack("blake512('');", '223d88a8c8308c15d479d1668ba97b1b2737aad82debd7d05d32f77a13f820651c36fc9eb18e2101b8e992717e671400be6a7f158cdd64afed6f81e62bf15c37');
ack("blake512.init(PGPHASHALGO_BLAKE_512);", true);
ack("blake512.algo", PGPHASHALGO_BLAKE_512);
ack("blake512.asn1", null);
ack("blake512.name", "BLAKE");
ack("blake512.update(str);", true);
ack("blake512.fini();", '73d4b67de1bedd9f4c7864d5e8b388a0c317d032c3e82df534f614dc5e91ca5b8a2e8310d92845909193f47a73bb2205a996952abe1e89978e907cd4027c35bb');

var bmw224 = new Dc(PGPHASHALGO_BMW_224);
ack("bmw224('');", 'bdf29a829d3f42d50604acb5ad1a851a8ec684a86ff7c4e791aad501');
ack("bmw224.init(PGPHASHALGO_BMW_224);", true);
ack("bmw224.algo", PGPHASHALGO_BMW_224);
ack("bmw224.asn1", null);
ack("bmw224.name", "BMW");
ack("bmw224.update(str);", true);
ack("bmw224.fini();", '26088053c93cbc480ad53c34fc66fff0b4f1341adb855154fa15ee54');

var bmw256 = new Dc(PGPHASHALGO_BMW_256);
ack("bmw256('');", '4c10f8ad695fff1fb275d175b44cbe41c40a53a166d0470b45bab2d9afb6e5c5');
ack("bmw256.init(PGPHASHALGO_BMW_256);", true);
ack("bmw256.algo", PGPHASHALGO_BMW_256);
ack("bmw256.asn1", null);
ack("bmw256.name", "BMW");
ack("bmw256.update(str);", true);
ack("bmw256.fini();", '919905e2d111e6f57cb42e5e31c9240eb0670c1919718a5c1b10e61fc5124d25');

var bmw384 = new Dc(PGPHASHALGO_BMW_384);
ack("bmw384('');", '62f300cced944e44fdd4e51e809c09eeefd31ee58bf977f29b0f475cb16c2f557b723553b9ab563e01d21a11e9d728e2');
ack("bmw384.init(PGPHASHALGO_BMW_384);", true);
ack("bmw384.algo", PGPHASHALGO_BMW_384);
ack("bmw384.asn1", null);
ack("bmw384.name", "BMW");
ack("bmw384.update(str);", true);
ack("bmw384.fini();", '1ef8b57640c0419e1f8c0d7ebc4196ea31fdb5fb9d2350c80db002882f59d3d1005899cd98353751840e34f619ec7abc');

var bmw512 = new Dc(PGPHASHALGO_BMW_512);
ack("bmw512('');", '73db7b1dc6def4ddf2f94a22e1f6d2162b3123828369ff4fd750832aede94e52d4d5c8b866860424991527175b6f62dbe0f764ac18290b92a26812d641cd5287');
ack("bmw512.init(PGPHASHALGO_BMW_512);", true);
ack("bmw512.algo", PGPHASHALGO_BMW_512);
ack("bmw512.asn1", null);
ack("bmw512.name", "BMW");
ack("bmw512.update(str);", true);
ack("bmw512.fini();", 'ded83592522d34271257b338337559204949bc4dbd9a6e66148393575673bae87334b76a3a3fc2734138f3074985ac8af8ff71ce0ca31cd7645df30849936dcf');

var edonr224 = new Dc(PGPHASHALGO_EDONR_224);
ack("edonr224('');", 'a9c2bc54208be171cdfd054d21d97c1f4c79e822d8d9fcdbcb1d602f');
ack("edonr224.init(PGPHASHALGO_EDONR_224);", true);
ack("edonr224.algo", PGPHASHALGO_EDONR_224);
ack("edonr224.asn1", null);
ack("edonr224.name", "EDON-R");
ack("edonr224.update('');", true);
ack("edonr224.fini();", 'a9c2bc54208be171cdfd054d21d97c1f4c79e822d8d9fcdbcb1d602f');

var edonr256 = new Dc(PGPHASHALGO_EDONR_256);
ack("edonr256('');", '3b71fd43dade942c07842b181f4d987e78d2ac7e3a7e8bb06fec99a60b60eaba');
ack("edonr256.init(PGPHASHALGO_EDONR_256);", true);
ack("edonr256.algo", PGPHASHALGO_EDONR_256);
ack("edonr256.asn1", null);
ack("edonr256.name", "EDON-R");
ack("edonr256.update('');", true);
ack("edonr256.fini();", '3b71fd43dade942c07842b181f4d987e78d2ac7e3a7e8bb06fec99a60b60eaba');

var edonr384 = new Dc(PGPHASHALGO_EDONR_384);
ack("edonr384('');", 'ccd8e612b93a3f8c24867eb204c1dc3f2de24bd54d92908c5f5a73c6a4da14c8742a5fc3a6b5428658e85f0175de95a7');
ack("edonr384.init(PGPHASHALGO_EDONR_384);", true);
ack("edonr384.algo", PGPHASHALGO_EDONR_384);
ack("edonr384.asn1", null);
ack("edonr384.name", "EDON-R");
ack("edonr384.update('');", true);
ack("edonr384.fini();", 'ccd8e612b93a3f8c24867eb204c1dc3f2de24bd54d92908c5f5a73c6a4da14c8742a5fc3a6b5428658e85f0175de95a7');

var edonr512 = new Dc(PGPHASHALGO_EDONR_512);
ack("edonr512('');", 'c57f7e17fdc1ce5074cc748c9bd38f9f51ebe88fbe6eda3190c4314cafb1abb2980fac582d6e4ba8c641947d944bc56b74fb15de5546ad4f77934fca00052719');
ack("edonr512.init(PGPHASHALGO_EDONR_512);", true);
ack("edonr512.algo", PGPHASHALGO_EDONR_512);
ack("edonr512.asn1", null);
ack("edonr512.name", "EDON-R");
ack("edonr512.update('');", true);
ack("edonr512.fini();", 'c57f7e17fdc1ce5074cc748c9bd38f9f51ebe88fbe6eda3190c4314cafb1abb2980fac582d6e4ba8c641947d944bc56b74fb15de5546ad4f77934fca00052719');

var keccak224 = new Dc(PGPHASHALGO_KECCAK_224);
ack("keccak224('');", '6c60c1d4dc10aee01988c45a33b38bc3045971724ce7e83cdda61635');
ack("keccak224.init(PGPHASHALGO_KECCAK_224);", true);
ack("keccak224.algo", PGPHASHALGO_KECCAK_224);
ack("keccak224.asn1", null);
ack("keccak224.name", "KECCAK");
ack("keccak224.update(str);", true);
ack("keccak224.fini();", '78e64e68f6be46a65dfc1a255c3c5ad6e1ab24be22260ddf059d8dd7');

var keccak256 = new Dc(PGPHASHALGO_KECCAK_256);
ack("keccak256('');", 'bcde039a63f98b125e7fe5cb8999c05dab163f857bcae719fb09b8d5e1da6f0c');
ack("keccak256.init(PGPHASHALGO_KECCAK_256);", true);
ack("keccak256.algo", PGPHASHALGO_KECCAK_256);
ack("keccak256.asn1", null);
ack("keccak256.name", "KECCAK");
ack("keccak256.update(str);", true);
ack("keccak256.fini();", '4e51e1369fe550ba6daeba7068701a653c1216762f41427d976789345687f056');

var keccak384 = new Dc(PGPHASHALGO_KECCAK_384);
ack("keccak384('');", '3f65c0fbe79d43f11d844a448a61b8316db1b681c252f9f5f3fd4da255a655187cff6c0ef96c8c9e7df899a36aa783a9');
ack("keccak384.init(PGPHASHALGO_KECCAK_384);", true);
ack("keccak384.algo", PGPHASHALGO_KECCAK_384);
ack("keccak384.asn1", null);
ack("keccak384.name", "KECCAK");
ack("keccak384.update(str);", true);
ack("keccak384.fini();", 'bf1c5ae872a458211fbe9fd263da97341697003d36f315148d17f7d407caad126443581a2c7c86aa91f5de4ee6375963');

var keccak512 = new Dc(PGPHASHALGO_KECCAK_512);
ack("keccak512('');", '8596f8df2e856ec888823da8ccc914139f31baee6aa5c37dbe30bddbfd75c63cdc205f15f30faa348e27b5f90495b339a606e3c84bfcdcd55e88b0e178b56feb');
ack("keccak512.init(PGPHASHALGO_KECCAK_512);", true);
ack("keccak512.algo", PGPHASHALGO_KECCAK_512);
ack("keccak512.asn1", null);
ack("keccak512.name", "KECCAK");
ack("keccak512.update(str);", true);
ack("keccak512.fini();", '4a2e21878d2785dffb751bb0c635e1f5780152922ffe7ef5342f7442d877754a3f866cd5b2d9f2711b02b24f64e437e4484a8d24b7878d288e9c550729ff954e');

var cubehash224 = new Dc(PGPHASHALGO_CUBEHASH_224);
ack("cubehash224('');", 'b5a6f6cb6d4100dcda8f575c694f15b2f7c8c5ed145608a42a89c7ca');
ack("cubehash224.init(PGPHASHALGO_CUBEHASH_224);", true);
ack("cubehash224.algo", PGPHASHALGO_CUBEHASH_224);
ack("cubehash224.asn1", null);
ack("cubehash224.name", "CUBEHASH");
ack("cubehash224.update(str);", true);
ack("cubehash224.fini();", '50151e3b6b2d13a9da38aa1422ab2dacc3500bebd4215d036fdcd5ce');

var cubehash256 = new Dc(PGPHASHALGO_CUBEHASH_256);
ack("cubehash256('');", '38d1e8a22d7baac6fd5262d83de89cacf784a02caa866335299987722aeabc59');
ack("cubehash256.init(PGPHASHALGO_CUBEHASH_256);", true);
ack("cubehash256.algo", PGPHASHALGO_CUBEHASH_256);
ack("cubehash256.asn1", null);
ack("cubehash256.name", "CUBEHASH");
ack("cubehash256.update(str);", true);
ack("cubehash256.fini();", '8de2181ab5ae4365a506cdf748f3af4b52a7b838a2c82550b8329bb6339914d7');

var cubehash384 = new Dc(PGPHASHALGO_CUBEHASH_384);
ack("cubehash384('');", '235e819ebb93af765f7d86df6c6ff283ab24e98a07858a7d1c72604bb10c794d4721ef9ddfccaa93072eee9b53fdc69c');
ack("cubehash384.init(PGPHASHALGO_CUBEHASH_384);", true);
ack("cubehash384.algo", PGPHASHALGO_CUBEHASH_384);
ack("cubehash384.asn1", null);
ack("cubehash384.name", "CUBEHASH");
ack("cubehash384.update(str);", true);
ack("cubehash384.fini();", 'a6cd4e9e01f83d75a3c5b30f7c9700216a453a2c9a9399181a0ad7c52a902f12f0a3301fec05428cda66abc16c5ca7e3');

var cubehash512 = new Dc(PGPHASHALGO_CUBEHASH_512);
ack("cubehash512('');", '90bc3f2948f7374065a811f1e47a208a53b1a2f3be1c0072759ed49c9c6c7f28f26eb30d5b0658c563077d599da23f97df0c2c0ac6cce734ffe87b2e76ff7294');
ack("cubehash512.init(PGPHASHALGO_CUBEHASH_512);", true);
ack("cubehash512.algo", PGPHASHALGO_CUBEHASH_512);
ack("cubehash512.asn1", null);
ack("cubehash512.name", "CUBEHASH");
ack("cubehash512.update(str);", true);
ack("cubehash512.fini();", 'f83d39f3f4213dbe240aa14740b214741163f37be49750cc9bf64aaa58be8f8adee7874186475cec08f7993ca7e35839291816ccc377d6173987eb95e355ee73');

// XXX md6sum, not NIST, values used as test vectors.
var md6_224 = new Dc(PGPHASHALGO_MD6_224);
ack("md6_224('');", 'd2091aa2ad17f38c51ade2697f24cafc3894c617c77ffe10fdc7abcb');
ack("md6_224.init(PGPHASHALGO_MD6_224);", true);
ack("md6_224.algo", PGPHASHALGO_MD6_224);
ack("md6_224.asn1", null);
ack("md6_224.name", "MD6");
ack("md6_224.update(str);", true);
ack("md6_224.fini();", '510c30e4202a5cdd8a4f2ae9beebb6f5988128897937615d52e6d228');

var md6_256 = new Dc(PGPHASHALGO_MD6_256);
ack("md6_256('');", 'bca38b24a804aa37d821d31af00f5598230122c5bbfc4c4ad5ed40e4258f04ca');
ack("md6_256.init(PGPHASHALGO_MD6_256);", true);
ack("md6_256.algo", PGPHASHALGO_MD6_256);
ack("md6_256.asn1", null);
ack("md6_256.name", "MD6");
ack("md6_256.update(str);", true);
ack("md6_256.fini();", '230637d4e6845cf0d092b558e87625f03881dd53a7439da34cf3b94ed0d8b2c5');

var md6_384 = new Dc(PGPHASHALGO_MD6_384);
ack("md6_384('');", 'b0bafffceebe856c1eff7e1ba2f539693f828b532ebf60ae9c16cbc3499020401b942ac25b310b2227b2954ccacc2f1f');
ack("md6_384.init(PGPHASHALGO_MD6_384);", true);
ack("md6_384.algo", PGPHASHALGO_MD6_384);
ack("md6_384.asn1", null);
ack("md6_384.name", "MD6");
ack("md6_384.update(str);", true);
ack("md6_384.fini();", 'e2c6d31dd8872cbd5a1207481cdac581054d13a4d4fe6854331cd8cf3e7cbafbaddd6e2517972b8ff57cdc4806d09190');

var md6_512 = new Dc(PGPHASHALGO_MD6_512);
ack("md6_512('');", '6b7f33821a2c060ecdd81aefddea2fd3c4720270e18654f4cb08ece49ccb469f8beeee7c831206bd577f9f2630d9177979203a9489e47e04df4e6deaa0f8e0c0');
ack("md6_512.init(PGPHASHALGO_MD6_512);", true);
ack("md6_512.algo", PGPHASHALGO_MD6_512);
ack("md6_512.asn1", null);
ack("md6_512.name", "MD6");
ack("md6_512.update(str);", true);
ack("md6_512.fini();", '00918245271e377a7ffb202b90f3bda5477d8feab12d8a3a8994ebc55fe6e74ca8341520032eeea3fdef892f2882378f636212af4b2683ccf80bf025b7d9b457');

var shabal224 = new Dc(PGPHASHALGO_SHABAL_224);
ack("shabal224('');", '562b4fdbe1706247552927f814b66a3d74b465a090af23e277bf8029');
ack("shabal224.init(PGPHASHALGO_SHABAL_224);", true);
ack("shabal224.algo", PGPHASHALGO_SHABAL_224);
ack("shabal224.asn1", null);
ack("shabal224.name", "SHABAL");
ack("shabal224.update(str);", true);
ack("shabal224.fini();", 'f47578239607af492d5f7df9241818adf6fba4180ddcbef6e39ac1e9');

var shabal256 = new Dc(PGPHASHALGO_SHABAL_256);
ack("shabal256('');", 'aec750d11feee9f16271922fbaf5a9be142f62019ef8d720f858940070889014');
ack("shabal256.init(PGPHASHALGO_SHABAL_256);", true);
ack("shabal256.algo", PGPHASHALGO_SHABAL_256);
ack("shabal256.asn1", null);
ack("shabal256.name", "SHABAL");
ack("shabal256.update(str);", true);
ack("shabal256.fini();", '07225fab83ca48fb480d22219410d5ca008359efbfd315829029afe2cb3f0404');

var shabal384 = new Dc(PGPHASHALGO_SHABAL_384);
ack("shabal384('');", 'ff093d67d22b06a674b5f384719150d617e0ff9c8923569a2ab60cda886df63c91a25f33cd71cc22c9eebc5cd6aee52a');
ack("shabal384.init(PGPHASHALGO_SHABAL_384);", true);
ack("shabal384.algo", PGPHASHALGO_SHABAL_384);
ack("shabal384.asn1", null);
ack("shabal384.name", "SHABAL");
ack("shabal384.update(str);", true);
ack("shabal384.fini();", '66613058865271722c0295774aa77258a5082bebbb5a02f9d6aee9ad303fc71cbf19e2f599ddfde88cf0bf30a028e530');

var shabal512 = new Dc(PGPHASHALGO_SHABAL_512);
ack("shabal512('');", 'fc2d5dff5d70b7f6b1f8c2fcc8c1f9fe9934e54257eded0cf2b539a2ef0a19ccffa84f8d9fa135e4bd3c09f590f3a927ebd603ac29eb729e6f2a9af031ad8dc6');
ack("shabal512.init(PGPHASHALGO_SHABAL_512);", true);
ack("shabal512.algo", PGPHASHALGO_SHABAL_512);
ack("shabal512.asn1", null);
ack("shabal512.name", "SHABAL");
ack("shabal512.update(str);", true);
ack("shabal512.fini();", '4a7f0f707c1b0c1d12ddcfa8aa0f9d2410dd9bab57c2d56705fc1acb02066f99678738cedb20a2aba94842a441e77bc02656fe5690f98b421d029bfc4df09f91');

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

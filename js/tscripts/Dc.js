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

var dc = new Dc();
ack("typeof dc;", "object");
ack("dc instanceof Dc;", true);
ack("dc.debug = 1;", 1);
ack("dc.debug = 0;", 0);

ack("dc.init(PGPHASHALGO_MD5);", true);
ack("dc.update('abc');", true);
ack("dc.fini();", '900150983cd24fb0d6963f7d28e17f72');

ack("dc.init(PGPHASHALGO_SHA1);", true);
ack("dc.update('abc');", true);
ack("dc.fini();", 'a9993e364706816aba3e25717850c26c9cd0d89d');

if (loglvl) print("<-- Dc.js");

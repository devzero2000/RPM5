/** \ingroup rpmio
 * \file rpmio/rpmmdb.c
 */

#include "system.h"

#include <setjmp.h>	/* XXX FIXME: AutoFu? */
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <rpmiotypes.h>
#include <rpmio.h>	/* for *Pool methods */
#include <rpmlog.h>
#include <rpmurl.h>
#define	_RPMMDB_INTERNAL
#include <rpmmdb.h>

#include "debug.h"

/*@unchecked@*/
int _rpmmdb_debug = 0;

/*@unchecked@*/ /*@relnull@*/
rpmmdb _rpmmdbI;


/*==============================================================*/
const int initialBufferSize = 128;

/* only need one of these */
static const int zero = 0;
static const int one = 1;

/*==============================================================*/
/* --- platform_hacks.h */
/*    Copyright 2009, 2010 10gen Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */


/* all platform-specific ifdefs should go here */

#ifdef __GNUC__
#define MONGO_INLINE static __inline__
#else
#define MONGO_INLINE static
#endif

#ifdef __cplusplus
#define MONGO_EXTERN_C_START extern "C" {
#define MONGO_EXTERN_C_END }
#else
#define MONGO_EXTERN_C_START
#define MONGO_EXTERN_C_END
#endif

/* big endian is only used for OID generation. little is used everywhere else */
#ifdef MONGO_BIG_ENDIAN
#define bson_little_endian64(out, in) ( bson_swap_endian64(out, in) )
#define bson_little_endian32(out, in) ( bson_swap_endian32(out, in) )
#define bson_big_endian64(out, in) ( memcpy(out, in, 8) )
#define bson_big_endian32(out, in) ( memcpy(out, in, 4) )
#else
#define bson_little_endian64(out, in) ( memcpy(out, in, 8) )
#define bson_little_endian32(out, in) ( memcpy(out, in, 4) )
#define bson_big_endian64(out, in) ( bson_swap_endian64(out, in) )
#define bson_big_endian32(out, in) ( bson_swap_endian32(out, in) )
#endif

MONGO_EXTERN_C_START

MONGO_INLINE void bson_swap_endian64(void* outp, const void* inp){
    const char *in = (const char*)inp;
    char *out = (char*)outp;

    out[0] = in[7];
    out[1] = in[6];
    out[2] = in[5];
    out[3] = in[4];
    out[4] = in[3];
    out[5] = in[2];
    out[6] = in[1];
    out[7] = in[0];

}
MONGO_INLINE void bson_swap_endian32(void* outp, const void* inp){
    const char *in = (const char*)inp;
    char *out = (char*)outp;

    out[0] = in[3];
    out[1] = in[2];
    out[2] = in[1];
    out[3] = in[0];
}

MONGO_EXTERN_C_END

/*==============================================================*/
/* --- numbers.c */

/* all the numbers that fit in a 4 byte string */
const char bson_numstrs[1000][4] = {
    "0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",  "8",  "9",
    "10", "11", "12", "13", "14", "15", "16", "17", "18", "19",
    "20", "21", "22", "23", "24", "25", "26", "27", "28", "29",
    "30", "31", "32", "33", "34", "35", "36", "37", "38", "39",
    "40", "41", "42", "43", "44", "45", "46", "47", "48", "49",
    "50", "51", "52", "53", "54", "55", "56", "57", "58", "59",
    "60", "61", "62", "63", "64", "65", "66", "67", "68", "69",
    "70", "71", "72", "73", "74", "75", "76", "77", "78", "79",
    "80", "81", "82", "83", "84", "85", "86", "87", "88", "89",
    "90", "91", "92", "93", "94", "95", "96", "97", "98", "99",

    "100", "101", "102", "103", "104", "105", "106", "107", "108", "109",
    "110", "111", "112", "113", "114", "115", "116", "117", "118", "119",
    "120", "121", "122", "123", "124", "125", "126", "127", "128", "129",
    "130", "131", "132", "133", "134", "135", "136", "137", "138", "139",
    "140", "141", "142", "143", "144", "145", "146", "147", "148", "149",
    "150", "151", "152", "153", "154", "155", "156", "157", "158", "159",
    "160", "161", "162", "163", "164", "165", "166", "167", "168", "169",
    "170", "171", "172", "173", "174", "175", "176", "177", "178", "179",
    "180", "181", "182", "183", "184", "185", "186", "187", "188", "189",
    "190", "191", "192", "193", "194", "195", "196", "197", "198", "199",

    "200", "201", "202", "203", "204", "205", "206", "207", "208", "209",
    "210", "211", "212", "213", "214", "215", "216", "217", "218", "219",
    "220", "221", "222", "223", "224", "225", "226", "227", "228", "229",
    "230", "231", "232", "233", "234", "235", "236", "237", "238", "239",
    "240", "241", "242", "243", "244", "245", "246", "247", "248", "249",
    "250", "251", "252", "253", "254", "255", "256", "257", "258", "259",
    "260", "261", "262", "263", "264", "265", "266", "267", "268", "269",
    "270", "271", "272", "273", "274", "275", "276", "277", "278", "279",
    "280", "281", "282", "283", "284", "285", "286", "287", "288", "289",
    "290", "291", "292", "293", "294", "295", "296", "297", "298", "299",

    "300", "301", "302", "303", "304", "305", "306", "307", "308", "309",
    "310", "311", "312", "313", "314", "315", "316", "317", "318", "319",
    "320", "321", "322", "323", "324", "325", "326", "327", "328", "329",
    "330", "331", "332", "333", "334", "335", "336", "337", "338", "339",
    "340", "341", "342", "343", "344", "345", "346", "347", "348", "349",
    "350", "351", "352", "353", "354", "355", "356", "357", "358", "359",
    "360", "361", "362", "363", "364", "365", "366", "367", "368", "369",
    "370", "371", "372", "373", "374", "375", "376", "377", "378", "379",
    "380", "381", "382", "383", "384", "385", "386", "387", "388", "389",
    "390", "391", "392", "393", "394", "395", "396", "397", "398", "399",

    "400", "401", "402", "403", "404", "405", "406", "407", "408", "409",
    "410", "411", "412", "413", "414", "415", "416", "417", "418", "419",
    "420", "421", "422", "423", "424", "425", "426", "427", "428", "429",
    "430", "431", "432", "433", "434", "435", "436", "437", "438", "439",
    "440", "441", "442", "443", "444", "445", "446", "447", "448", "449",
    "450", "451", "452", "453", "454", "455", "456", "457", "458", "459",
    "460", "461", "462", "463", "464", "465", "466", "467", "468", "469",
    "470", "471", "472", "473", "474", "475", "476", "477", "478", "479",
    "480", "481", "482", "483", "484", "485", "486", "487", "488", "489",
    "490", "491", "492", "493", "494", "495", "496", "497", "498", "499",

    "500", "501", "502", "503", "504", "505", "506", "507", "508", "509",
    "510", "511", "512", "513", "514", "515", "516", "517", "518", "519",
    "520", "521", "522", "523", "524", "525", "526", "527", "528", "529",
    "530", "531", "532", "533", "534", "535", "536", "537", "538", "539",
    "540", "541", "542", "543", "544", "545", "546", "547", "548", "549",
    "550", "551", "552", "553", "554", "555", "556", "557", "558", "559",
    "560", "561", "562", "563", "564", "565", "566", "567", "568", "569",
    "570", "571", "572", "573", "574", "575", "576", "577", "578", "579",
    "580", "581", "582", "583", "584", "585", "586", "587", "588", "589",
    "590", "591", "592", "593", "594", "595", "596", "597", "598", "599",

    "600", "601", "602", "603", "604", "605", "606", "607", "608", "609",
    "610", "611", "612", "613", "614", "615", "616", "617", "618", "619",
    "620", "621", "622", "623", "624", "625", "626", "627", "628", "629",
    "630", "631", "632", "633", "634", "635", "636", "637", "638", "639",
    "640", "641", "642", "643", "644", "645", "646", "647", "648", "649",
    "650", "651", "652", "653", "654", "655", "656", "657", "658", "659",
    "660", "661", "662", "663", "664", "665", "666", "667", "668", "669",
    "670", "671", "672", "673", "674", "675", "676", "677", "678", "679",
    "680", "681", "682", "683", "684", "685", "686", "687", "688", "689",
    "690", "691", "692", "693", "694", "695", "696", "697", "698", "699",

    "700", "701", "702", "703", "704", "705", "706", "707", "708", "709",
    "710", "711", "712", "713", "714", "715", "716", "717", "718", "719",
    "720", "721", "722", "723", "724", "725", "726", "727", "728", "729",
    "730", "731", "732", "733", "734", "735", "736", "737", "738", "739",
    "740", "741", "742", "743", "744", "745", "746", "747", "748", "749",
    "750", "751", "752", "753", "754", "755", "756", "757", "758", "759",
    "760", "761", "762", "763", "764", "765", "766", "767", "768", "769",
    "770", "771", "772", "773", "774", "775", "776", "777", "778", "779",
    "780", "781", "782", "783", "784", "785", "786", "787", "788", "789",
    "790", "791", "792", "793", "794", "795", "796", "797", "798", "799",

    "800", "801", "802", "803", "804", "805", "806", "807", "808", "809",
    "810", "811", "812", "813", "814", "815", "816", "817", "818", "819",
    "820", "821", "822", "823", "824", "825", "826", "827", "828", "829",
    "830", "831", "832", "833", "834", "835", "836", "837", "838", "839",
    "840", "841", "842", "843", "844", "845", "846", "847", "848", "849",
    "850", "851", "852", "853", "854", "855", "856", "857", "858", "859",
    "860", "861", "862", "863", "864", "865", "866", "867", "868", "869",
    "870", "871", "872", "873", "874", "875", "876", "877", "878", "879",
    "880", "881", "882", "883", "884", "885", "886", "887", "888", "889",
    "890", "891", "892", "893", "894", "895", "896", "897", "898", "899",

    "900", "901", "902", "903", "904", "905", "906", "907", "908", "909",
    "910", "911", "912", "913", "914", "915", "916", "917", "918", "919",
    "920", "921", "922", "923", "924", "925", "926", "927", "928", "929",
    "930", "931", "932", "933", "934", "935", "936", "937", "938", "939",
    "940", "941", "942", "943", "944", "945", "946", "947", "948", "949",
    "950", "951", "952", "953", "954", "955", "956", "957", "958", "959",
    "960", "961", "962", "963", "964", "965", "966", "967", "968", "969",
    "970", "971", "972", "973", "974", "975", "976", "977", "978", "979",
    "980", "981", "982", "983", "984", "985", "986", "987", "988", "989",
    "990", "991", "992", "993", "994", "995", "996", "997", "998", "999",
};

/*==============================================================*/
/* --- bson.h */

MONGO_EXTERN_C_START

typedef enum {
    bson_eoo=0 ,
    bson_double=1,
    bson_string=2,
    bson_object=3,
    bson_array=4,
    bson_bindata=5,
    bson_undefined=6,
    bson_oid=7,
    bson_bool=8,
    bson_date=9,
    bson_null=10,
    bson_regex=11,
    bson_dbref=12, /* deprecated */
    bson_code=13,
    bson_symbol=14,
    bson_codewscope=15,
    bson_int = 16,
    bson_timestamp = 17,
    bson_long = 18
} bson_type;

typedef int bson_bool_t;

typedef struct {
    char * data;
    bson_bool_t owned;
} bson;

typedef struct {
    const char * cur;
    bson_bool_t first;
} bson_iterator;

typedef struct {
    char * buf;
    char * cur;
    int bufSize;
    bson_bool_t finished;
    int stack[32];
    int stackPos;
} bson_buffer;

#pragma pack(1)
typedef union{
    char bytes[12];
    int ints[3];
} bson_oid_t;
#pragma pack()

typedef int64_t bson_date_t; /* milliseconds since epoch UTC */

/* ----------------------------
   READING
   ------------------------------ */


bson * bson_empty(bson * obj); /* returns pointer to static empty bson object */
void bson_copy(bson* out, const bson* in); /* puts data in new buffer. NOOP if out==NULL */
bson * bson_from_buffer(bson * b, bson_buffer * buf);
bson * bson_init( bson * b , char * data , bson_bool_t mine );
int bson_size(const bson * b );
void bson_destroy( bson * b );

void bson_print( bson * b );
void bson_print_raw( const char * bson , int depth );

/* advances iterator to named field */
/* returns bson_eoo (which is false) if field not found */
bson_type bson_find(bson_iterator* it, const bson* obj, const char* name);

void bson_iterator_init( bson_iterator * i , const char * bson );

/* more returns true for eoo. best to loop with bson_iterator_next(&it) */
bson_bool_t bson_iterator_more( const bson_iterator * i );
bson_type bson_iterator_next( bson_iterator * i );

bson_type bson_iterator_type( const bson_iterator * i );
const char * bson_iterator_key( const bson_iterator * i );
const char * bson_iterator_value( const bson_iterator * i );

/* these convert to the right type (return 0 if non-numeric) */
double bson_iterator_double( const bson_iterator * i );
int bson_iterator_int( const bson_iterator * i );
int64_t bson_iterator_long( const bson_iterator * i );

/* false: boolean false, 0 in any type, or null */
/* true: anything else (even empty strings and objects) */
bson_bool_t bson_iterator_bool( const bson_iterator * i );

/* these assume you are using the right type */
double bson_iterator_double_raw( const bson_iterator * i );
int bson_iterator_int_raw( const bson_iterator * i );
int64_t bson_iterator_long_raw( const bson_iterator * i );
bson_bool_t bson_iterator_bool_raw( const bson_iterator * i );
bson_oid_t* bson_iterator_oid( const bson_iterator * i );

/* these can also be used with bson_code and bson_symbol*/
const char * bson_iterator_string( const bson_iterator * i );
int bson_iterator_string_len( const bson_iterator * i );

/* works with bson_code, bson_codewscope, and bson_string */
/* returns NULL for everything else */
const char * bson_iterator_code(const bson_iterator * i);

/* calls bson_empty on scope if not a bson_codewscope */
void bson_iterator_code_scope(const bson_iterator * i, bson * scope);

/* both of these only work with bson_date */
bson_date_t bson_iterator_date(const bson_iterator * i);
time_t bson_iterator_time_t(const bson_iterator * i);

int bson_iterator_bin_len( const bson_iterator * i );
char bson_iterator_bin_type( const bson_iterator * i );
const char * bson_iterator_bin_data( const bson_iterator * i );

const char * bson_iterator_regex( const bson_iterator * i );
const char * bson_iterator_regex_opts( const bson_iterator * i );

/* these work with bson_object and bson_array */
void bson_iterator_subobject(const bson_iterator * i, bson * sub);
void bson_iterator_subiterator(const bson_iterator * i, bson_iterator * sub);

/* str must be at least 24 hex chars + null byte */
void bson_oid_from_string(bson_oid_t* oid, const char* str);
void bson_oid_to_string(const bson_oid_t* oid, char* str);
void bson_oid_gen(bson_oid_t* oid);

time_t bson_oid_generated_time(bson_oid_t* oid); /* Gives the time the OID was created */

/* ----------------------------
   BUILDING
   ------------------------------ */

bson_buffer * bson_buffer_init( bson_buffer * b );
bson_buffer * bson_ensure_space( bson_buffer * b , const int bytesNeeded );

/**
 * @return the raw data.  you either should free this OR call bson_destroy not both
 */
char * bson_buffer_finish( bson_buffer * b );
void bson_buffer_destroy( bson_buffer * b );

bson_buffer * bson_append_oid( bson_buffer * b , const char * name , const bson_oid_t* oid );
bson_buffer * bson_append_new_oid( bson_buffer * b , const char * name );
bson_buffer * bson_append_int( bson_buffer * b , const char * name , const int i );
bson_buffer * bson_append_long( bson_buffer * b , const char * name , const int64_t i );
bson_buffer * bson_append_double( bson_buffer * b , const char * name , const double d );
bson_buffer * bson_append_string( bson_buffer * b , const char * name , const char * str );
bson_buffer * bson_append_symbol( bson_buffer * b , const char * name , const char * str );
bson_buffer * bson_append_code( bson_buffer * b , const char * name , const char * str );
bson_buffer * bson_append_code_w_scope( bson_buffer * b , const char * name , const char * code , const bson * scope);
bson_buffer * bson_append_binary( bson_buffer * b, const char * name, char type, const char * str, int len );
bson_buffer * bson_append_bool( bson_buffer * b , const char * name , const bson_bool_t v );
bson_buffer * bson_append_null( bson_buffer * b , const char * name );
bson_buffer * bson_append_undefined( bson_buffer * b , const char * name );
bson_buffer * bson_append_regex( bson_buffer * b , const char * name , const char * pattern, const char * opts );
bson_buffer * bson_append_bson( bson_buffer * b , const char * name , const bson* bson);
bson_buffer * bson_append_element( bson_buffer * b, const char * name_or_null, const bson_iterator* elem);

/* these both append a bson_date */
bson_buffer * bson_append_date(bson_buffer * b, const char * name, bson_date_t millis);
bson_buffer * bson_append_time_t(bson_buffer * b, const char * name, time_t secs);

bson_buffer * bson_append_start_object( bson_buffer * b , const char * name );
bson_buffer * bson_append_start_array( bson_buffer * b , const char * name );
bson_buffer * bson_append_finish_object( bson_buffer * b );

void bson_numstr(char* str, int i);
void bson_incnumstr(char* str);


/* ------------------------------
   ERROR HANDLING - also used in mongo code
   ------------------------------ */

void * bson_malloc(int size); /* checks return value */

/* bson_err_handlers shouldn't return!!! */
typedef void(*bson_err_handler)(const char* errmsg);

/* returns old handler or NULL */
/* default handler prints error then exits with failure*/
bson_err_handler set_bson_err_handler(bson_err_handler func);



/* does nothing is ok != 0 */
void bson_fatal( int ok );
void bson_fatal_msg( int ok, const char* msg );

MONGO_EXTERN_C_END

/*==============================================================*/
/* --- bson.c */

/* ----------------------------
   READING
   ------------------------------ */

bson * bson_empty(bson * obj){
    static char * data = "\005\0\0\0\0";
    return bson_init(obj, data, 0);
}

void bson_copy(bson* out, const bson* in){
    if (!out) return;
    out->data = bson_malloc(bson_size(in));
    out->owned = 1;
    memcpy(out->data, in->data, bson_size(in));
}

bson * bson_from_buffer(bson * b, bson_buffer * buf){
    return bson_init(b, bson_buffer_finish(buf), 1);
}

bson * bson_init( bson * b , char * data , bson_bool_t mine ){
    b->data = data;
    b->owned = mine;
    return b;
}
int bson_size(const bson * b ){
    int i;
    if ( ! b || ! b->data )
        return 0;
    bson_little_endian32(&i, b->data);
    return i;
}
void bson_destroy( bson * b ){
    if ( b->owned && b->data )
        free( b->data );
    b->data = 0;
    b->owned = 0;
}

static char hexbyte(char hex){
    switch (hex){
        case '0': return 0x0;
        case '1': return 0x1;
        case '2': return 0x2;
        case '3': return 0x3;
        case '4': return 0x4;
        case '5': return 0x5;
        case '6': return 0x6;
        case '7': return 0x7;
        case '8': return 0x8;
        case '9': return 0x9;
        case 'a': 
        case 'A': return 0xa;
        case 'b':
        case 'B': return 0xb;
        case 'c':
        case 'C': return 0xc;
        case 'd':
        case 'D': return 0xd;
        case 'e':
        case 'E': return 0xe;
        case 'f':
        case 'F': return 0xf;
        default: return 0x0; /* something smarter? */
    }
}

void bson_oid_from_string(bson_oid_t* oid, const char* str){
    int i;
    for (i=0; i<12; i++){
        oid->bytes[i] = (hexbyte(str[2*i]) << 4) | hexbyte(str[2*i + 1]);
    }
}
void bson_oid_to_string(const bson_oid_t* oid, char* str){
    static const char hex[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
    int i;
    for (i=0; i<12; i++){
        str[2*i]     = hex[(oid->bytes[i] & 0xf0) >> 4];
        str[2*i + 1] = hex[ oid->bytes[i] & 0x0f      ];
    }
    str[24] = '\0';
}
void bson_oid_gen(bson_oid_t* oid){
    static int incr = 0;
    static int fuzz = 0;
    int i = incr++; /*TODO make atomic*/
    int t = time(NULL);

    /* TODO rand sucks. find something better */
    if (!fuzz){
        srand(t);
        fuzz = rand();
    }
    
    bson_big_endian32(&oid->ints[0], &t);
    oid->ints[1] = fuzz;
    bson_big_endian32(&oid->ints[2], &i);
}

time_t bson_oid_generated_time(bson_oid_t* oid){
    time_t out;
    bson_big_endian32(&out, &oid->ints[0]);
    return out;
}

void bson_print( bson * b ){
    bson_print_raw( b->data , 0 );
}

void bson_print_raw( const char * data , int depth ){
    bson_iterator i;
    const char * key;
    int temp;
    char oidhex[25];
    bson_iterator_init( &i , data );

    while ( bson_iterator_next( &i ) ){
        bson_type t = bson_iterator_type( &i );
        if ( t == 0 )
            break;
        key = bson_iterator_key( &i );
        
        for ( temp=0; temp<=depth; temp++ )
            printf( "\t" );
        printf( "%s : %d \t " , key , t );
        switch ( t ){
        case bson_int: printf( "%d" , bson_iterator_int( &i ) ); break;
        case bson_double: printf( "%f" , bson_iterator_double( &i ) ); break;
        case bson_bool: printf( "%s" , bson_iterator_bool( &i ) ? "true" : "false" ); break;
        case bson_string: printf( "%s" , bson_iterator_string( &i ) ); break;
        case bson_null: printf( "null" ); break;
        case bson_oid: bson_oid_to_string(bson_iterator_oid(&i), oidhex); printf( "%s" , oidhex ); break;
        case bson_object:
        case bson_array:
            printf( "\n" );
            bson_print_raw( bson_iterator_value( &i ) , depth + 1 );
            break;
        default:
            fprintf( stderr , "can't print type : %d\n" , t );
        }
        printf( "\n" );
    }
}

/* ----------------------------
   ITERATOR
   ------------------------------ */

void bson_iterator_init( bson_iterator * i , const char * bson ){
    i->cur = bson + 4;
    i->first = 1;
}

bson_type bson_find(bson_iterator* it, const bson* obj, const char* name){
    bson_iterator_init(it, obj->data);
    while(bson_iterator_next(it)){
        if (strcmp(name, bson_iterator_key(it)) == 0)
            break;
    }
    return bson_iterator_type(it);
}

bson_bool_t bson_iterator_more( const bson_iterator * i ){
    return *(i->cur);
}

bson_type bson_iterator_next( bson_iterator * i ){
    int ds;

    if ( i->first ){
        i->first = 0;
        return (bson_type)(*i->cur);
    }
    
    switch ( bson_iterator_type(i) ){
    case bson_eoo: return bson_eoo; /* don't advance */
    case bson_undefined:
    case bson_null: ds = 0; break;
    case bson_bool: ds = 1; break;
    case bson_int: ds = 4; break;
    case bson_long:
    case bson_double:
    case bson_timestamp:
    case bson_date: ds = 8; break;
    case bson_oid: ds = 12; break;
    case bson_string:
    case bson_symbol:
    case bson_code: ds = 4 + bson_iterator_int_raw(i); break;
    case bson_bindata: ds = 5 + bson_iterator_int_raw(i); break;
    case bson_object:
    case bson_array:
    case bson_codewscope: ds = bson_iterator_int_raw(i); break;
    case bson_dbref: ds = 4+12 + bson_iterator_int_raw(i); break;
    case bson_regex:
        {
            const char * s = bson_iterator_value(i);
            const char * p = s;
            p += strlen(p)+1;
            p += strlen(p)+1;
            ds = p-s;
            break;
        }

    default: 
        {
            char msg[] = "unknown type: 000000000000";
            bson_numstr(msg+14, (unsigned)(i->cur[0]));
            bson_fatal_msg(0, msg);
            return 0;
        }
    }
    
    i->cur += 1 + strlen( i->cur + 1 ) + 1 + ds;

    return (bson_type)(*i->cur);
}

bson_type bson_iterator_type( const bson_iterator * i ){
    return (bson_type)i->cur[0];
}
const char * bson_iterator_key( const bson_iterator * i ){
    return i->cur + 1;
}
const char * bson_iterator_value( const bson_iterator * i ){
    const char * t = i->cur + 1;
    t += strlen( t ) + 1;
    return t;
}

/* types */

int bson_iterator_int_raw( const bson_iterator * i ){
    int out;
    bson_little_endian32(&out, bson_iterator_value( i ));
    return out;
}
double bson_iterator_double_raw( const bson_iterator * i ){
    double out;
    bson_little_endian64(&out, bson_iterator_value( i ));
    return out;
}
int64_t bson_iterator_long_raw( const bson_iterator * i ){
    int64_t out;
    bson_little_endian64(&out, bson_iterator_value( i ));
    return out;
}

bson_bool_t bson_iterator_bool_raw( const bson_iterator * i ){
    return bson_iterator_value( i )[0];
}

bson_oid_t * bson_iterator_oid( const bson_iterator * i ){
    return (bson_oid_t*)bson_iterator_value(i);
}

int bson_iterator_int( const bson_iterator * i ){
    switch (bson_iterator_type(i)){
        case bson_int: return bson_iterator_int_raw(i);
        case bson_long: return bson_iterator_long_raw(i);
        case bson_double: return bson_iterator_double_raw(i);
        default: return 0;
    }
}
double bson_iterator_double( const bson_iterator * i ){
    switch (bson_iterator_type(i)){
        case bson_int: return bson_iterator_int_raw(i);
        case bson_long: return bson_iterator_long_raw(i);
        case bson_double: return bson_iterator_double_raw(i);
        default: return 0;
    }
}
int64_t bson_iterator_long( const bson_iterator * i ){
    switch (bson_iterator_type(i)){
        case bson_int: return bson_iterator_int_raw(i);
        case bson_long: return bson_iterator_long_raw(i);
        case bson_double: return bson_iterator_double_raw(i);
        default: return 0;
    }
}

bson_bool_t bson_iterator_bool( const bson_iterator * i ){
    switch (bson_iterator_type(i)){
        case bson_bool: return bson_iterator_bool_raw(i);
        case bson_int: return bson_iterator_int_raw(i) != 0;
        case bson_long: return bson_iterator_long_raw(i) != 0;
        case bson_double: return bson_iterator_double_raw(i) != 0;
        case bson_eoo:
        case bson_null: return 0;
        default: return 1;
    }
}

const char * bson_iterator_string( const bson_iterator * i ){
    return bson_iterator_value( i ) + 4;
}
int bson_iterator_string_len( const bson_iterator * i ){
    return bson_iterator_int_raw( i );
}

const char * bson_iterator_code( const bson_iterator * i ){
    switch (bson_iterator_type(i)){
        case bson_string:
        case bson_code: return bson_iterator_value(i) + 4;
        case bson_codewscope: return bson_iterator_value(i) + 8;
        default: return NULL;
    }
}

void bson_iterator_code_scope(const bson_iterator * i, bson * scope){
    if (bson_iterator_type(i) == bson_codewscope){
        int code_len;
        bson_little_endian32(&code_len, bson_iterator_value(i)+4);
        bson_init(scope, (void*)(bson_iterator_value(i)+8+code_len), 0);
    }else{
        bson_empty(scope);
    }
}

bson_date_t bson_iterator_date(const bson_iterator * i){
    return bson_iterator_long_raw(i);
}

time_t bson_iterator_time_t(const bson_iterator * i){
    return bson_iterator_date(i) / 1000;
}

int bson_iterator_bin_len( const bson_iterator * i ){
    return bson_iterator_int_raw( i );
}

char bson_iterator_bin_type( const bson_iterator * i ){
    return bson_iterator_value(i)[4];
}
const char * bson_iterator_bin_data( const bson_iterator * i ){
    return bson_iterator_value( i ) + 5;
}

const char * bson_iterator_regex( const bson_iterator * i ){
    return bson_iterator_value( i );
}
const char * bson_iterator_regex_opts( const bson_iterator * i ){
    const char* p = bson_iterator_value( i );
    return p + strlen(p) + 1;

}

void bson_iterator_subobject(const bson_iterator * i, bson * sub){
    bson_init(sub, (char*)bson_iterator_value(i), 0);
}
void bson_iterator_subiterator(const bson_iterator * i, bson_iterator * sub){
    bson_iterator_init(sub, bson_iterator_value(i));
}

/* ----------------------------
   BUILDING
   ------------------------------ */

bson_buffer * bson_buffer_init( bson_buffer * b ){
    b->buf = (char*)bson_malloc( initialBufferSize );
    b->bufSize = initialBufferSize;
    b->cur = b->buf + 4;
    b->finished = 0;
    b->stackPos = 0;
    return b;
}

void bson_append_byte( bson_buffer * b , char c ){
    b->cur[0] = c;
    b->cur++;
}
void bson_append( bson_buffer * b , const void * data , int len ){
    memcpy( b->cur , data , len );
    b->cur += len;
}
void bson_append32(bson_buffer * b, const void * data){
    bson_little_endian32(b->cur, data);
    b->cur += 4;
}
void bson_append64(bson_buffer * b, const void * data){
    bson_little_endian64(b->cur, data);
    b->cur += 8;
}

bson_buffer * bson_ensure_space( bson_buffer * b , const int bytesNeeded ){
    int pos = b->cur - b->buf;
    char * orig = b->buf;
    int new_size;

    if (b->finished)
        bson_fatal_msg(!!b->buf, "trying to append to finished buffer");

    if (pos + bytesNeeded <= b->bufSize)
        return b;

    new_size = 1.5 * (b->bufSize + bytesNeeded);
    b->buf = realloc(b->buf, new_size);
    if (!b->buf)
        bson_fatal_msg(!!b->buf, "realloc() failed");

    b->bufSize = new_size;
    b->cur += b->buf - orig;

    return b;
}

char * bson_buffer_finish( bson_buffer * b ){
    int i;
    if ( ! b->finished ){
        if ( ! bson_ensure_space( b , 1 ) ) return 0;
        bson_append_byte( b , 0 );
        i = b->cur - b->buf;
        bson_little_endian32(b->buf, &i);
        b->finished = 1;
    }
    return b->buf;
}

void bson_buffer_destroy( bson_buffer * b ){
    free( b->buf );
    b->buf = 0;
    b->cur = 0;
    b->finished = 1;
}

static bson_buffer * bson_append_estart( bson_buffer * b , int type , const char * name , const int dataSize ){
    const int sl = strlen(name) + 1;
    if ( ! bson_ensure_space( b , 1 + sl + dataSize ) )
        return 0;
    bson_append_byte( b , (char)type );
    bson_append( b , name , sl );
    return b;
}

/* ----------------------------
   BUILDING TYPES
   ------------------------------ */

bson_buffer * bson_append_int( bson_buffer * b , const char * name , const int i ){
    if ( ! bson_append_estart( b , bson_int , name , 4 ) ) return 0;
    bson_append32( b , &i );
    return b;
}
bson_buffer * bson_append_long( bson_buffer * b , const char * name , const int64_t i ){
    if ( ! bson_append_estart( b , bson_long , name , 8 ) ) return 0;
    bson_append64( b , &i );
    return b;
}
bson_buffer * bson_append_double( bson_buffer * b , const char * name , const double d ){
    if ( ! bson_append_estart( b , bson_double , name , 8 ) ) return 0;
    bson_append64( b , &d );
    return b;
}
bson_buffer * bson_append_bool( bson_buffer * b , const char * name , const bson_bool_t i ){
    if ( ! bson_append_estart( b , bson_bool , name , 1 ) ) return 0;
    bson_append_byte( b , i != 0 );
    return b;
}
bson_buffer * bson_append_null( bson_buffer * b , const char * name ){
    if ( ! bson_append_estart( b , bson_null , name , 0 ) ) return 0;
    return b;
}
bson_buffer * bson_append_undefined( bson_buffer * b , const char * name ){
    if ( ! bson_append_estart( b , bson_undefined , name , 0 ) ) return 0;
    return b;
}
bson_buffer * bson_append_string_base( bson_buffer * b , const char * name , const char * value , bson_type type){
    int sl = strlen( value ) + 1;
    if ( ! bson_append_estart( b , type , name , 4 + sl ) ) return 0;
    bson_append32( b , &sl);
    bson_append( b , value , sl );
    return b;
}
bson_buffer * bson_append_string( bson_buffer * b , const char * name , const char * value ){
    return bson_append_string_base(b, name, value, bson_string);
}
bson_buffer * bson_append_symbol( bson_buffer * b , const char * name , const char * value ){
    return bson_append_string_base(b, name, value, bson_symbol);
}
bson_buffer * bson_append_code( bson_buffer * b , const char * name , const char * value ){
    return bson_append_string_base(b, name, value, bson_code);
}

bson_buffer * bson_append_code_w_scope( bson_buffer * b , const char * name , const char * code , const bson * scope){
    int sl = strlen(code) + 1;
    int size = 4 + 4 + sl + bson_size(scope);
    if (!bson_append_estart(b, bson_codewscope, name, size)) return 0;
    bson_append32(b, &size);
    bson_append32(b, &sl);
    bson_append(b, code, sl);
    bson_append(b, scope->data, bson_size(scope));
    return b;
}

bson_buffer * bson_append_binary( bson_buffer * b, const char * name, char type, const char * str, int len ){
    if ( ! bson_append_estart( b , bson_bindata , name , 4+1+len ) ) return 0;
    bson_append32(b, &len);
    bson_append_byte(b, type);
    bson_append(b, str, len);
    return b;
}
bson_buffer * bson_append_oid( bson_buffer * b , const char * name , const bson_oid_t * oid ){
    if ( ! bson_append_estart( b , bson_oid , name , 12 ) ) return 0;
    bson_append( b , oid , 12 );
    return b;
}
bson_buffer * bson_append_new_oid( bson_buffer * b , const char * name ){
    bson_oid_t oid;
    bson_oid_gen(&oid);
    return bson_append_oid(b, name, &oid);
}

bson_buffer * bson_append_regex( bson_buffer * b , const char * name , const char * pattern, const char * opts ){
    const int plen = strlen(pattern)+1;
    const int olen = strlen(opts)+1;
    if ( ! bson_append_estart( b , bson_regex , name , plen + olen ) ) return 0;
    bson_append( b , pattern , plen );
    bson_append( b , opts , olen );
    return b;
}

bson_buffer * bson_append_bson( bson_buffer * b , const char * name , const bson* bson){
    if ( ! bson_append_estart( b , bson_object , name , bson_size(bson) ) ) return 0;
    bson_append( b , bson->data , bson_size(bson) );
    return b;
}

bson_buffer * bson_append_element( bson_buffer * b, const char * name_or_null, const bson_iterator* elem){
    bson_iterator next = *elem;
    int size;

    bson_iterator_next(&next);
    size = next.cur - elem->cur;

    if (name_or_null == NULL){
        bson_ensure_space(b, size);
        bson_append(b, elem->cur, size);
    }else{
        int data_size = size - 1 - strlen(bson_iterator_key(elem));
        bson_append_estart(b, elem->cur[0], name_or_null, data_size);
        bson_append(b, name_or_null, strlen(name_or_null));
        bson_append(b, bson_iterator_value(elem), data_size);
    }

    return b;
}

bson_buffer * bson_append_date( bson_buffer * b , const char * name , bson_date_t millis ){
    if ( ! bson_append_estart( b , bson_date , name , 8 ) ) return 0;
    bson_append64( b , &millis );
    return b;
}

bson_buffer * bson_append_time_t( bson_buffer * b , const char * name , time_t secs){
    return bson_append_date(b, name, (bson_date_t)secs * 1000);
}

bson_buffer * bson_append_start_object( bson_buffer * b , const char * name ){
    if ( ! bson_append_estart( b , bson_object , name , 5 ) ) return 0;
    b->stack[ b->stackPos++ ] = b->cur - b->buf;
    bson_append32( b , &zero );
    return b;
}

bson_buffer * bson_append_start_array( bson_buffer * b , const char * name ){
    if ( ! bson_append_estart( b , bson_array , name , 5 ) ) return 0;
    b->stack[ b->stackPos++ ] = b->cur - b->buf;
    bson_append32( b , &zero );
    return b;
}

bson_buffer * bson_append_finish_object( bson_buffer * b ){
    char * start;
    int i;
    if ( ! bson_ensure_space( b , 1 ) ) return 0;
    bson_append_byte( b , 0 );
    
    start = b->buf + b->stack[ --b->stackPos ];
    i = b->cur - start;
    bson_little_endian32(start, &i);

    return b;
}

void* bson_malloc(int size){
    void* p = malloc(size);
    bson_fatal_msg(!!p, "malloc() failed");
    return p;
}

static bson_err_handler err_handler = NULL;

bson_err_handler set_bson_err_handler(bson_err_handler func){
    bson_err_handler old = err_handler;
    err_handler = func;
    return old;
}

void bson_fatal( int ok ){
    bson_fatal_msg(ok, "");
}

void bson_fatal_msg( int ok , const char* msg){
    if (ok)
        return;

    if (err_handler){
        err_handler(msg);
    }

    fprintf( stderr , "error: %s\n" , msg );
    exit(-5);
}

void bson_numstr(char* str, int i){
    if(i < 1000)
        memcpy(str, bson_numstrs[i], 4);
    else
        sprintf(str,"%d", i);
}

/*==============================================================*/
/*
  Copyright (C) 1999, 2002 Aladdin Enterprises.  All rights reserved.

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  L. Peter Deutsch
  ghost@aladdin.com

 */

/*
  Independent implementation of MD5 (RFC 1321).

  This code implements the MD5 Algorithm defined in RFC 1321, whose
  text is available at
	http://www.ietf.org/rfc/rfc1321.txt
  The code is derived from the text of the RFC, including the test suite
  (section A.5) but excluding the rest of Appendix A.  It does not include
  any code or documentation that is identified in the RFC as being
  copyrighted.

  The original and principal author of md5.h is L. Peter Deutsch
  <ghost@aladdin.com>.  Other authors are noted in the change history
  that follows (in reverse chronological order):

  2002-04-13 lpd Removed support for non-ANSI compilers; removed
	references to Ghostscript; clarified derivation from RFC 1321;
	now handles byte order either statically or dynamically.
  1999-11-04 lpd Edited comments slightly for automatic TOC extraction.
  1999-10-18 lpd Fixed typo in header comment (ansi2knr rather than md5);
	added conditionalization for C++ compilation from Martin
	Purschke <purschke@bnl.gov>.
  1999-05-03 lpd Original version.
 */

/*
 * This package supports both compile-time and run-time determination of CPU
 * byte order.  If ARCH_IS_BIG_ENDIAN is defined as 0, the code will be
 * compiled to run only on little-endian CPUs; if ARCH_IS_BIG_ENDIAN is
 * defined as non-zero, the code will be compiled to run only on big-endian
 * CPUs; if ARCH_IS_BIG_ENDIAN is not defined, the code will be compiled to
 * run on either big- or little-endian CPUs, but will run slightly less
 * efficiently on either one than if ARCH_IS_BIG_ENDIAN is defined.
 */

typedef unsigned char mongo_md5_byte_t; /* 8-bit byte */
typedef unsigned int mongo_md5_word_t; /* 32-bit word */

/* Define the state of the MD5 Algorithm. */
typedef struct mongo_md5_state_s {
    mongo_md5_word_t count[2];	/* message length in bits, lsw first */
    mongo_md5_word_t abcd[4];		/* digest buffer */
    mongo_md5_byte_t buf[64];		/* accumulate block */
} mongo_md5_state_t;

#ifdef __cplusplus
extern "C"
{
#endif

    /* Initialize the algorithm. */
    void mongo_md5_init(mongo_md5_state_t *pms);

    /* Append a string to the message. */
    void mongo_md5_append(mongo_md5_state_t *pms, const mongo_md5_byte_t *data, int nbytes);

    /* Finish the message and return the digest. */
    void mongo_md5_finish(mongo_md5_state_t *pms, mongo_md5_byte_t digest[16]);

#ifdef __cplusplus
}  /* end extern "C" */
#endif

/*==============================================================*/
/* --- md5.c */

/*
  Independent implementation of MD5 (RFC 1321).

  This code implements the MD5 Algorithm defined in RFC 1321, whose
  text is available at
	http://www.ietf.org/rfc/rfc1321.txt
  The code is derived from the text of the RFC, including the test suite
  (section A.5) but excluding the rest of Appendix A.  It does not include
  any code or documentation that is identified in the RFC as being
  copyrighted.

  The original and principal author of md5.c is L. Peter Deutsch
  <ghost@aladdin.com>.  Other authors are noted in the change history
  that follows (in reverse chronological order):

  2002-04-13 lpd Clarified derivation from RFC 1321; now handles byte order
	either statically or dynamically; added missing #include <string.h>
	in library.
  2002-03-11 lpd Corrected argument list for main(), and added int return
	type, in test program and T value program.
  2002-02-21 lpd Added missing #include <stdio.h> in test program.
  2000-07-03 lpd Patched to eliminate warnings about "constant is
	unsigned in ANSI C, signed in traditional"; made test program
	self-checking.
  1999-11-04 lpd Edited comments slightly for automatic TOC extraction.
  1999-10-18 lpd Fixed typo in header comment (ansi2knr rather than md5).
  1999-05-03 lpd Original version.
 */

#undef BYTE_ORDER	/* 1 = big-endian, -1 = little-endian, 0 = unknown */
#ifdef MONGO_BIG_ENDIAN
#  define BYTE_ORDER 1
#else
#  define BYTE_ORDER -1
#endif

#define T_MASK ((mongo_md5_word_t)~0)
#define T1 /* 0xd76aa478 */ (T_MASK ^ 0x28955b87)
#define T2 /* 0xe8c7b756 */ (T_MASK ^ 0x173848a9)
#define T3    0x242070db
#define T4 /* 0xc1bdceee */ (T_MASK ^ 0x3e423111)
#define T5 /* 0xf57c0faf */ (T_MASK ^ 0x0a83f050)
#define T6    0x4787c62a
#define T7 /* 0xa8304613 */ (T_MASK ^ 0x57cfb9ec)
#define T8 /* 0xfd469501 */ (T_MASK ^ 0x02b96afe)
#define T9    0x698098d8
#define T10 /* 0x8b44f7af */ (T_MASK ^ 0x74bb0850)
#define T11 /* 0xffff5bb1 */ (T_MASK ^ 0x0000a44e)
#define T12 /* 0x895cd7be */ (T_MASK ^ 0x76a32841)
#define T13    0x6b901122
#define T14 /* 0xfd987193 */ (T_MASK ^ 0x02678e6c)
#define T15 /* 0xa679438e */ (T_MASK ^ 0x5986bc71)
#define T16    0x49b40821
#define T17 /* 0xf61e2562 */ (T_MASK ^ 0x09e1da9d)
#define T18 /* 0xc040b340 */ (T_MASK ^ 0x3fbf4cbf)
#define T19    0x265e5a51
#define T20 /* 0xe9b6c7aa */ (T_MASK ^ 0x16493855)
#define T21 /* 0xd62f105d */ (T_MASK ^ 0x29d0efa2)
#define T22    0x02441453
#define T23 /* 0xd8a1e681 */ (T_MASK ^ 0x275e197e)
#define T24 /* 0xe7d3fbc8 */ (T_MASK ^ 0x182c0437)
#define T25    0x21e1cde6
#define T26 /* 0xc33707d6 */ (T_MASK ^ 0x3cc8f829)
#define T27 /* 0xf4d50d87 */ (T_MASK ^ 0x0b2af278)
#define T28    0x455a14ed
#define T29 /* 0xa9e3e905 */ (T_MASK ^ 0x561c16fa)
#define T30 /* 0xfcefa3f8 */ (T_MASK ^ 0x03105c07)
#define T31    0x676f02d9
#define T32 /* 0x8d2a4c8a */ (T_MASK ^ 0x72d5b375)
#define T33 /* 0xfffa3942 */ (T_MASK ^ 0x0005c6bd)
#define T34 /* 0x8771f681 */ (T_MASK ^ 0x788e097e)
#define T35    0x6d9d6122
#define T36 /* 0xfde5380c */ (T_MASK ^ 0x021ac7f3)
#define T37 /* 0xa4beea44 */ (T_MASK ^ 0x5b4115bb)
#define T38    0x4bdecfa9
#define T39 /* 0xf6bb4b60 */ (T_MASK ^ 0x0944b49f)
#define T40 /* 0xbebfbc70 */ (T_MASK ^ 0x4140438f)
#define T41    0x289b7ec6
#define T42 /* 0xeaa127fa */ (T_MASK ^ 0x155ed805)
#define T43 /* 0xd4ef3085 */ (T_MASK ^ 0x2b10cf7a)
#define T44    0x04881d05
#define T45 /* 0xd9d4d039 */ (T_MASK ^ 0x262b2fc6)
#define T46 /* 0xe6db99e5 */ (T_MASK ^ 0x1924661a)
#define T47    0x1fa27cf8
#define T48 /* 0xc4ac5665 */ (T_MASK ^ 0x3b53a99a)
#define T49 /* 0xf4292244 */ (T_MASK ^ 0x0bd6ddbb)
#define T50    0x432aff97
#define T51 /* 0xab9423a7 */ (T_MASK ^ 0x546bdc58)
#define T52 /* 0xfc93a039 */ (T_MASK ^ 0x036c5fc6)
#define T53    0x655b59c3
#define T54 /* 0x8f0ccc92 */ (T_MASK ^ 0x70f3336d)
#define T55 /* 0xffeff47d */ (T_MASK ^ 0x00100b82)
#define T56 /* 0x85845dd1 */ (T_MASK ^ 0x7a7ba22e)
#define T57    0x6fa87e4f
#define T58 /* 0xfe2ce6e0 */ (T_MASK ^ 0x01d3191f)
#define T59 /* 0xa3014314 */ (T_MASK ^ 0x5cfebceb)
#define T60    0x4e0811a1
#define T61 /* 0xf7537e82 */ (T_MASK ^ 0x08ac817d)
#define T62 /* 0xbd3af235 */ (T_MASK ^ 0x42c50dca)
#define T63    0x2ad7d2bb
#define T64 /* 0xeb86d391 */ (T_MASK ^ 0x14792c6e)

static void
mongo_md5_process(mongo_md5_state_t *pms, const mongo_md5_byte_t *data /*[64]*/)
{
    mongo_md5_word_t
	a = pms->abcd[0], b = pms->abcd[1],
	c = pms->abcd[2], d = pms->abcd[3];
    mongo_md5_word_t t;
#if BYTE_ORDER > 0
    /* Define storage only for big-endian CPUs. */
    mongo_md5_word_t X[16];
#else
    /* Define storage for little-endian or both types of CPUs. */
    mongo_md5_word_t xbuf[16];
    const mongo_md5_word_t *X;
#endif

    {
#if BYTE_ORDER == 0
	/*
	 * Determine dynamically whether this is a big-endian or
	 * little-endian machine, since we can use a more efficient
	 * algorithm on the latter.
	 */
	static const int w = 1;

	if (*((const mongo_md5_byte_t *)&w)) /* dynamic little-endian */
#endif
#if BYTE_ORDER <= 0		/* little-endian */
	{
	    /*
	     * On little-endian machines, we can process properly aligned
	     * data without copying it.
	     */
	    if (!((data - (const mongo_md5_byte_t *)0) & 3)) {
		/* data are properly aligned */
		X = (const mongo_md5_word_t *)data;
	    } else {
		/* not aligned */
		memcpy(xbuf, data, 64);
		X = xbuf;
	    }
	}
#endif
#if BYTE_ORDER == 0
	else			/* dynamic big-endian */
#endif
#if BYTE_ORDER >= 0		/* big-endian */
	{
	    /*
	     * On big-endian machines, we must arrange the bytes in the
	     * right order.
	     */
	    const mongo_md5_byte_t *xp = data;
	    int i;

#  if BYTE_ORDER == 0
	    X = xbuf;		/* (dynamic only) */
#  else
#    define xbuf X		/* (static only) */
#  endif
	    for (i = 0; i < 16; ++i, xp += 4)
		xbuf[i] = xp[0] + (xp[1] << 8) + (xp[2] << 16) + (xp[3] << 24);
	}
#endif
    }

#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

    /* Round 1. */
    /* Let [abcd k s i] denote the operation
       a = b + ((a + F(b,c,d) + X[k] + T[i]) <<< s). */
#define F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define SET(a, b, c, d, k, s, Ti)\
  t = a + F(b,c,d) + X[k] + Ti;\
  a = ROTATE_LEFT(t, s) + b
    /* Do the following 16 operations. */
    SET(a, b, c, d,  0,  7,  T1);
    SET(d, a, b, c,  1, 12,  T2);
    SET(c, d, a, b,  2, 17,  T3);
    SET(b, c, d, a,  3, 22,  T4);
    SET(a, b, c, d,  4,  7,  T5);
    SET(d, a, b, c,  5, 12,  T6);
    SET(c, d, a, b,  6, 17,  T7);
    SET(b, c, d, a,  7, 22,  T8);
    SET(a, b, c, d,  8,  7,  T9);
    SET(d, a, b, c,  9, 12, T10);
    SET(c, d, a, b, 10, 17, T11);
    SET(b, c, d, a, 11, 22, T12);
    SET(a, b, c, d, 12,  7, T13);
    SET(d, a, b, c, 13, 12, T14);
    SET(c, d, a, b, 14, 17, T15);
    SET(b, c, d, a, 15, 22, T16);
#undef SET

     /* Round 2. */
     /* Let [abcd k s i] denote the operation
          a = b + ((a + G(b,c,d) + X[k] + T[i]) <<< s). */
#define G(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define SET(a, b, c, d, k, s, Ti)\
  t = a + G(b,c,d) + X[k] + Ti;\
  a = ROTATE_LEFT(t, s) + b
     /* Do the following 16 operations. */
    SET(a, b, c, d,  1,  5, T17);
    SET(d, a, b, c,  6,  9, T18);
    SET(c, d, a, b, 11, 14, T19);
    SET(b, c, d, a,  0, 20, T20);
    SET(a, b, c, d,  5,  5, T21);
    SET(d, a, b, c, 10,  9, T22);
    SET(c, d, a, b, 15, 14, T23);
    SET(b, c, d, a,  4, 20, T24);
    SET(a, b, c, d,  9,  5, T25);
    SET(d, a, b, c, 14,  9, T26);
    SET(c, d, a, b,  3, 14, T27);
    SET(b, c, d, a,  8, 20, T28);
    SET(a, b, c, d, 13,  5, T29);
    SET(d, a, b, c,  2,  9, T30);
    SET(c, d, a, b,  7, 14, T31);
    SET(b, c, d, a, 12, 20, T32);
#undef SET

     /* Round 3. */
     /* Let [abcd k s t] denote the operation
          a = b + ((a + H(b,c,d) + X[k] + T[i]) <<< s). */
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define SET(a, b, c, d, k, s, Ti)\
  t = a + H(b,c,d) + X[k] + Ti;\
  a = ROTATE_LEFT(t, s) + b
     /* Do the following 16 operations. */
    SET(a, b, c, d,  5,  4, T33);
    SET(d, a, b, c,  8, 11, T34);
    SET(c, d, a, b, 11, 16, T35);
    SET(b, c, d, a, 14, 23, T36);
    SET(a, b, c, d,  1,  4, T37);
    SET(d, a, b, c,  4, 11, T38);
    SET(c, d, a, b,  7, 16, T39);
    SET(b, c, d, a, 10, 23, T40);
    SET(a, b, c, d, 13,  4, T41);
    SET(d, a, b, c,  0, 11, T42);
    SET(c, d, a, b,  3, 16, T43);
    SET(b, c, d, a,  6, 23, T44);
    SET(a, b, c, d,  9,  4, T45);
    SET(d, a, b, c, 12, 11, T46);
    SET(c, d, a, b, 15, 16, T47);
    SET(b, c, d, a,  2, 23, T48);
#undef SET

     /* Round 4. */
     /* Let [abcd k s t] denote the operation
          a = b + ((a + I(b,c,d) + X[k] + T[i]) <<< s). */
#define I(x, y, z) ((y) ^ ((x) | ~(z)))
#define SET(a, b, c, d, k, s, Ti)\
  t = a + I(b,c,d) + X[k] + Ti;\
  a = ROTATE_LEFT(t, s) + b
     /* Do the following 16 operations. */
    SET(a, b, c, d,  0,  6, T49);
    SET(d, a, b, c,  7, 10, T50);
    SET(c, d, a, b, 14, 15, T51);
    SET(b, c, d, a,  5, 21, T52);
    SET(a, b, c, d, 12,  6, T53);
    SET(d, a, b, c,  3, 10, T54);
    SET(c, d, a, b, 10, 15, T55);
    SET(b, c, d, a,  1, 21, T56);
    SET(a, b, c, d,  8,  6, T57);
    SET(d, a, b, c, 15, 10, T58);
    SET(c, d, a, b,  6, 15, T59);
    SET(b, c, d, a, 13, 21, T60);
    SET(a, b, c, d,  4,  6, T61);
    SET(d, a, b, c, 11, 10, T62);
    SET(c, d, a, b,  2, 15, T63);
    SET(b, c, d, a,  9, 21, T64);
#undef SET

     /* Then perform the following additions. (That is increment each
        of the four registers by the value it had before this block
        was started.) */
    pms->abcd[0] += a;
    pms->abcd[1] += b;
    pms->abcd[2] += c;
    pms->abcd[3] += d;
}

void
mongo_md5_init(mongo_md5_state_t *pms)
{
    pms->count[0] = pms->count[1] = 0;
    pms->abcd[0] = 0x67452301;
    pms->abcd[1] = /*0xefcdab89*/ T_MASK ^ 0x10325476;
    pms->abcd[2] = /*0x98badcfe*/ T_MASK ^ 0x67452301;
    pms->abcd[3] = 0x10325476;
}

void
mongo_md5_append(mongo_md5_state_t *pms, const mongo_md5_byte_t *data, int nbytes)
{
    const mongo_md5_byte_t *p = data;
    int left = nbytes;
    int offset = (pms->count[0] >> 3) & 63;
    mongo_md5_word_t nbits = (mongo_md5_word_t)(nbytes << 3);

    if (nbytes <= 0)
	return;

    /* Update the message length. */
    pms->count[1] += nbytes >> 29;
    pms->count[0] += nbits;
    if (pms->count[0] < nbits)
	pms->count[1]++;

    /* Process an initial partial block. */
    if (offset) {
	int copy = (offset + nbytes > 64 ? 64 - offset : nbytes);

	memcpy(pms->buf + offset, p, copy);
	if (offset + copy < 64)
	    return;
	p += copy;
	left -= copy;
	mongo_md5_process(pms, pms->buf);
    }

    /* Process full blocks. */
    for (; left >= 64; p += 64, left -= 64)
	mongo_md5_process(pms, p);

    /* Process a final partial block. */
    if (left)
	memcpy(pms->buf, p, left);
}

void
mongo_md5_finish(mongo_md5_state_t *pms, mongo_md5_byte_t digest[16])
{
    static const mongo_md5_byte_t pad[64] = {
	0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    mongo_md5_byte_t data[8];
    int i;

    /* Save the length before padding. */
    for (i = 0; i < 8; ++i)
	data[i] = (mongo_md5_byte_t)(pms->count[i >> 2] >> ((i & 3) << 3));
    /* Pad to 56 bytes mod 64. */
    mongo_md5_append(pms, pad, ((55 - (pms->count[0] >> 3)) & 63) + 1);
    /* Append the length. */
    mongo_md5_append(pms, data, 8);
    for (i = 0; i < 16; ++i)
	digest[i] = (mongo_md5_byte_t)(pms->abcd[i >> 2] >> ((i & 3) << 3));
}

/*==============================================================*/
/* --- mongo_except.h */

/* This file is based loosely on cexcept (http://www.nicemice.net/cexcept/). I
 * have modified it to work better with mongo's API.
 *
 * The MONGO_TRY, MONGO_CATCH, and MONGO_TROW macros assume that a pointer to
 * the current connection is available as 'conn'. If you would like to use a
 * different name, use the _GENERIC version of these macros.
 *
 * WARNING: do not return or otherwise jump (excluding MONGO_TRHOW()) out of a
 * MONGO_TRY block as the nessesary clean-up code will not be called. Jumping
 * out of the MONGO_CATCH block is OK.
 */

#ifdef MONGO_CODE_EXAMPLE
    mongo_connection conn[1]; /* makes conn a ptr to the connection */

    MONGO_TRY{
        mongo_find_one(...);
        MONGO_THROW(conn, MONGO_EXCEPT_NETWORK);
    }MONGO_CATCH{
        switch(conn->exception->type){
            case MONGO_EXCEPT_NETWORK:
                do_something();
            case MONGO_EXCEPT_FIND_ERR:
                do_something();
            default:
                MONGO_RETHROW();
        }
    }
#endif

 /* ORIGINAL CEXEPT COPYRIGHT:
cexcept: README 2.0.1 (2008-Jul-23-Wed)
http://www.nicemice.net/cexcept/
Adam M. Costello
http://www.nicemice.net/amc/

The package is both free-as-in-speech and free-as-in-beer:

    Copyright (c) 2000-2008 Adam M. Costello and Cosmin Truta.
    This package may be modified only if its author and version
    information is updated accurately, and may be redistributed
    only if accompanied by this unaltered notice.  Subject to those
    restrictions, permission is granted to anyone to do anything with
    this package.  The copyright holders make no guarantees regarding
    this package, and are not responsible for any damage resulting from
    its use.
 */

/* always non-zero */
typedef enum{
    MONGO_EXCEPT_NETWORK=1,
    MONGO_EXCEPT_FIND_ERR
}mongo_exception_type;


typedef struct {
  jmp_buf base_handler;
  jmp_buf *penv;
  int caught;
  volatile mongo_exception_type type;
}mongo_exception_context;

#define MONGO_TRY MONGO_TRY_GENERIC(conn)
#define MONGO_CATCH MONGO_CATCH_GENERIC(conn)
#define MONGO_THROW(e) MONGO_THROW_GENERIC(conn, e)
#define MONGO_RETHROW() MONGO_RETHROW_GENERIC(conn)

/* the rest of this file is implementation details */

/* this is done in mongo_connect */
#define MONGO_INIT_EXCEPTION(exception_ptr) \
    do{ \
        mongo_exception_type t; /* exception_ptr won't be available */\
        (exception_ptr)->penv = &(exception_ptr)->base_handler; \
        if ((t = setjmp((exception_ptr)->base_handler))) { /* yes, '=' is correct */ \
            switch(t){ \
                case MONGO_EXCEPT_NETWORK: bson_fatal_msg(0, "network error"); \
                case MONGO_EXCEPT_FIND_ERR: bson_fatal_msg(0, "error in find"); \
                default: bson_fatal_msg(0, "unknown exception"); \
            } \
        } \
    }while(0)

#define MONGO_TRY_GENERIC(connection) \
  { \
    jmp_buf *exception__prev, exception__env; \
    exception__prev = (connection)->exception.penv; \
    (connection)->exception.penv = &exception__env; \
    if (setjmp(exception__env) == 0) { \
      do

#define MONGO_CATCH_GENERIC(connection) \
      while ((connection)->exception.caught = 0, \
             (connection)->exception.caught); \
    } \
    else { \
      (connection)->exception.caught = 1; \
    } \
    (connection)->exception.penv = exception__prev; \
  } \
  if (!(connection)->exception.caught ) { } \
  else

/* Try ends with do, and Catch begins with while(0) and ends with     */
/* else, to ensure that Try/Catch syntax is similar to if/else        */
/* syntax.                                                            */
/*                                                                    */
/* The 0 in while(0) is expressed as x=0,x in order to appease        */
/* compilers that warn about constant expressions inside while().     */
/* Most compilers should still recognize that the condition is always */
/* false and avoid generating code for it.                            */

#define MONGO_THROW_GENERIC(connection, type_in) \
  for (;; longjmp(*(connection)->exception.penv, type_in)) \
    (connection)->exception.type = type_in

#define MONGO_RETHROW_GENERIC(connection) \
    MONGO_THROW_GENERIC(connection, (connection)->exception.type)

/*==============================================================*/
/* --- mongo.h */

MONGO_EXTERN_C_START

typedef struct mongo_connection_options {
    char host[255];
    int port;
} mongo_connection_options;

typedef struct {
    mongo_connection_options* left_opts; /* always current server */
    mongo_connection_options* right_opts; /* unused with single server */
    struct sockaddr_in sa;
    socklen_t addressSize;
    int sock;
    bson_bool_t connected;
    mongo_exception_context exception;
} mongo_connection;

#pragma pack(1)
typedef struct {
    int len;
    int id;
    int responseTo;
    int op;
} mongo_header;

typedef struct {
    mongo_header head;
    char data;
} mongo_message;

typedef struct {
    int flag; /* non-zero on failure */
    int64_t cursorID;
    int start;
    int num;
} mongo_reply_fields;

typedef struct {
    mongo_header head;
    mongo_reply_fields fields;
    char objs;
} mongo_reply;
#pragma pack()

typedef struct {
    mongo_reply * mm; /* message is owned by cursor */
    mongo_connection * conn; /* connection is *not* owned by cursor */
    const char* ns; /* owned by cursor */
    bson current;
} mongo_cursor;

enum mongo_operations {
    mongo_op_msg = 1000,    /* generic msg command followed by a string */
    mongo_op_update = 2001, /* update object */
    mongo_op_insert = 2002,
    mongo_op_query = 2004,
    mongo_op_get_more = 2005,
    mongo_op_delete = 2006,
    mongo_op_kill_cursors = 2007
};

/* ----------------------------
   CONNECTION STUFF
   ------------------------------ */

typedef enum {
    mongo_conn_success = 0,
    mongo_conn_bad_arg,
    mongo_conn_no_socket,
    mongo_conn_fail,
    mongo_conn_not_master /* leaves conn connected to slave */
} mongo_conn_return;

/**
 * @param options can be null
 */
mongo_conn_return mongo_connect( mongo_connection * conn , mongo_connection_options * options );
mongo_conn_return mongo_connect_pair( mongo_connection * conn , mongo_connection_options * left, mongo_connection_options * right );
mongo_conn_return mongo_reconnect( mongo_connection * conn ); /* you will need to reauthenticate after calling */
bson_bool_t mongo_disconnect( mongo_connection * conn ); /* use this if you want to be able to reconnect */
bson_bool_t mongo_destroy( mongo_connection * conn ); /* you must call this even if connection failed */

/* ----------------------------
   CORE METHODS - insert update remove query getmore
   ------------------------------ */

void mongo_insert( mongo_connection * conn , const char * ns , bson * data );
void mongo_insert_batch( mongo_connection * conn , const char * ns , bson ** data , int num );

static const int MONGO_UPDATE_UPSERT = 0x1;
static const int MONGO_UPDATE_MULTI = 0x2;
void mongo_update(mongo_connection* conn, const char* ns, const bson* cond, const bson* op, int flags);

void mongo_remove(mongo_connection* conn, const char* ns, const bson* cond);

mongo_cursor* mongo_find(mongo_connection* conn, const char* ns, bson* query, bson* fields ,int nToReturn ,int nToSkip, int options);
bson_bool_t mongo_cursor_next(mongo_cursor* cursor);
void mongo_cursor_destroy(mongo_cursor* cursor);

/* out can be NULL if you don't care about results. useful for commands */
bson_bool_t mongo_find_one(mongo_connection* conn, const char* ns, bson* query, bson* fields, bson* out);

int64_t mongo_count(mongo_connection* conn, const char* db, const char* coll, bson* query);

/* ----------------------------
   HIGHER LEVEL - indexes - command helpers eval
   ------------------------------ */

/* Returns true on success */
/* WARNING: Unlike other drivers these do not cache results */

static const int MONGO_INDEX_UNIQUE = 0x1;
static const int MONGO_INDEX_DROP_DUPS = 0x2;
bson_bool_t mongo_create_index(mongo_connection * conn, const char * ns, bson * key, int options, bson * out);
bson_bool_t mongo_create_simple_index(mongo_connection * conn, const char * ns, const char* field, int options, bson * out);

/* ----------------------------
   COMMANDS
   ------------------------------ */

bson_bool_t mongo_run_command(mongo_connection * conn, const char * db, bson * command, bson * out);

/* for simple commands with a single k-v pair */
bson_bool_t mongo_simple_int_command(mongo_connection * conn, const char * db, const char* cmd,         int arg, bson * out);
bson_bool_t mongo_simple_str_command(mongo_connection * conn, const char * db, const char* cmd, const char* arg, bson * out);

bson_bool_t mongo_cmd_drop_db(mongo_connection * conn, const char * db);
bson_bool_t mongo_cmd_drop_collection(mongo_connection * conn, const char * db, const char * collection, bson * out);

void mongo_cmd_add_user(mongo_connection* conn, const char* db, const char* user, const char* pass);
bson_bool_t mongo_cmd_authenticate(mongo_connection* conn, const char* db, const char* user, const char* pass);

/* return value is master status */
bson_bool_t mongo_cmd_ismaster(mongo_connection * conn, bson * out);

/* true return indicates error */
bson_bool_t mongo_cmd_get_last_error(mongo_connection * conn, const char * db, bson * out);
bson_bool_t mongo_cmd_get_prev_error(mongo_connection * conn, const char * db, bson * out);
void        mongo_cmd_reset_error(mongo_connection * conn, const char * db);

/* ----------------------------
   UTILS
   ------------------------------ */

MONGO_EXTERN_C_END

/*==============================================================*/
/* --- mongo.c */

/* ----------------------------
   message stuff
   ------------------------------ */

static void looping_write(mongo_connection * conn, const void* buf, int len){
    const char* cbuf = buf;
    while (len){
        int sent = send(conn->sock, cbuf, len, 0);
        if (sent == -1) MONGO_THROW(MONGO_EXCEPT_NETWORK);
        cbuf += sent;
        len -= sent;
    }
}

static void looping_read(mongo_connection * conn, void* buf, int len){
    char* cbuf = buf;
    while (len){
        int sent = recv(conn->sock, cbuf, len, 0);
        if (sent == 0 || sent == -1) MONGO_THROW(MONGO_EXCEPT_NETWORK);
        cbuf += sent;
        len -= sent;
    }
}

/* Always calls free(mm) */
void mongo_message_send(mongo_connection * conn, mongo_message* mm){
    mongo_header head; /* little endian */
    bson_little_endian32(&head.len, &mm->head.len);
    bson_little_endian32(&head.id, &mm->head.id);
    bson_little_endian32(&head.responseTo, &mm->head.responseTo);
    bson_little_endian32(&head.op, &mm->head.op);
    
    MONGO_TRY{
        looping_write(conn, &head, sizeof(head));
        looping_write(conn, &mm->data, mm->head.len - sizeof(head));
    }MONGO_CATCH{
        free(mm);
        MONGO_RETHROW();
    }
    free(mm);
}

char * mongo_data_append( char * start , const void * data , int len ){
    memcpy( start , data , len );
    return start + len;
}

char * mongo_data_append32( char * start , const void * data){
    bson_little_endian32( start , data );
    return start + 4;
}

char * mongo_data_append64( char * start , const void * data){
    bson_little_endian64( start , data );
    return start + 8;
}

mongo_message * mongo_message_create( int len , int id , int responseTo , int op ){
    mongo_message * mm = (mongo_message*)bson_malloc( len );

    if (!id)
        id = rand();

    /* native endian (converted on send) */
    mm->head.len = len;
    mm->head.id = id;
    mm->head.responseTo = responseTo;
    mm->head.op = op;

    return mm;
}

/* ----------------------------
   connection stuff
   ------------------------------ */
static int mongo_connect_helper( mongo_connection * conn ){
    /* setup */
    conn->sock = 0;
    conn->connected = 0;

    memset( conn->sa.sin_zero , 0 , sizeof(conn->sa.sin_zero) );
    conn->sa.sin_family = AF_INET;
    conn->sa.sin_port = htons(conn->left_opts->port);
    conn->sa.sin_addr.s_addr = inet_addr( conn->left_opts->host );
    conn->addressSize = sizeof(conn->sa);

    /* connect */
    conn->sock = socket( AF_INET, SOCK_STREAM, 0 );
    if ( conn->sock <= 0 ){
        return mongo_conn_no_socket;
    }

    if ( connect( conn->sock , (struct sockaddr*)&conn->sa , conn->addressSize ) ){
        return mongo_conn_fail;
    }

    /* nagle */
    setsockopt( conn->sock, IPPROTO_TCP, TCP_NODELAY, (char *) &one, sizeof(one) );

    /* TODO signals */

    conn->connected = 1;
    return 0;
}

mongo_conn_return mongo_connect( mongo_connection * conn , mongo_connection_options * options ){
    MONGO_INIT_EXCEPTION(&conn->exception);

    conn->left_opts = bson_malloc(sizeof(mongo_connection_options));
    conn->right_opts = NULL;

    if ( options ){
        memcpy( conn->left_opts , options , sizeof( mongo_connection_options ) );
    } else {
        strcpy( conn->left_opts->host , "127.0.0.1" );
        conn->left_opts->port = 27017;
    }

    return mongo_connect_helper(conn);
}

static void swap_repl_pair(mongo_connection * conn){
    mongo_connection_options * tmp = conn->left_opts;
    conn->left_opts = conn->right_opts;
    conn->right_opts = tmp;
}

mongo_conn_return mongo_connect_pair( mongo_connection * conn , mongo_connection_options * left, mongo_connection_options * right ){
    conn->connected = 0;
    MONGO_INIT_EXCEPTION(&conn->exception);

    conn->left_opts = NULL;
    conn->right_opts = NULL;

    if ( !left || !right )
        return mongo_conn_bad_arg;

    conn->left_opts = bson_malloc(sizeof(mongo_connection_options));
    conn->right_opts = bson_malloc(sizeof(mongo_connection_options));

    memcpy( conn->left_opts,  left,  sizeof( mongo_connection_options ) );
    memcpy( conn->right_opts, right, sizeof( mongo_connection_options ) );
    
    return mongo_reconnect(conn);
}

mongo_conn_return mongo_reconnect( mongo_connection * conn ){
    mongo_conn_return ret;
    mongo_disconnect(conn);

    /* single server */
    if(conn->right_opts == NULL)
        return mongo_connect_helper(conn);

    /* repl pair */
    ret = mongo_connect_helper(conn);
    if (ret == mongo_conn_success && mongo_cmd_ismaster(conn, NULL)){
        return mongo_conn_success;
    }

    swap_repl_pair(conn);

    ret = mongo_connect_helper(conn);
    if (ret == mongo_conn_success){
        if(mongo_cmd_ismaster(conn, NULL))
            return mongo_conn_success;
        else
            return mongo_conn_not_master;
    }

    /* failed to connect to both servers */
    return ret;
}

void mongo_insert_batch( mongo_connection * conn , const char * ns , bson ** bsons, int count){
    int size =  16 + 4 + strlen( ns ) + 1;
    int i;
    mongo_message * mm;
    char* data;

    for(i=0; i<count; i++){
        size += bson_size(bsons[i]);
    }

    mm = mongo_message_create( size , 0 , 0 , mongo_op_insert );

    data = &mm->data;
    data = mongo_data_append32(data, &zero);
    data = mongo_data_append(data, ns, strlen(ns) + 1);

    for(i=0; i<count; i++){
        data = mongo_data_append(data, bsons[i]->data, bson_size( bsons[i] ) );
    }

    mongo_message_send(conn, mm);
}

void mongo_insert( mongo_connection * conn , const char * ns , bson * bson ){
    char * data;
    mongo_message * mm = mongo_message_create( 16 /* header */
                                             + 4 /* ZERO */
                                             + strlen(ns)
                                             + 1 + bson_size(bson)
                                             , 0, 0, mongo_op_insert);

    data = &mm->data;
    data = mongo_data_append32(data, &zero);
    data = mongo_data_append(data, ns, strlen(ns) + 1);
    data = mongo_data_append(data, bson->data, bson_size(bson));

    mongo_message_send(conn, mm);
}

void mongo_update(mongo_connection* conn, const char* ns, const bson* cond, const bson* op, int flags){
    char * data;
    mongo_message * mm = mongo_message_create( 16 /* header */
                                             + 4  /* ZERO */
                                             + strlen(ns) + 1
                                             + 4  /* flags */
                                             + bson_size(cond)
                                             + bson_size(op)
                                             , 0 , 0 , mongo_op_update );

    data = &mm->data;
    data = mongo_data_append32(data, &zero);
    data = mongo_data_append(data, ns, strlen(ns) + 1);
    data = mongo_data_append32(data, &flags);
    data = mongo_data_append(data, cond->data, bson_size(cond));
    data = mongo_data_append(data, op->data, bson_size(op));

    mongo_message_send(conn, mm);
}

void mongo_remove(mongo_connection* conn, const char* ns, const bson* cond){
    char * data;
    mongo_message * mm = mongo_message_create( 16 /* header */
                                             + 4  /* ZERO */
                                             + strlen(ns) + 1
                                             + 4  /* ZERO */
                                             + bson_size(cond)
                                             , 0 , 0 , mongo_op_delete );

    data = &mm->data;
    data = mongo_data_append32(data, &zero);
    data = mongo_data_append(data, ns, strlen(ns) + 1);
    data = mongo_data_append32(data, &zero);
    data = mongo_data_append(data, cond->data, bson_size(cond));

    mongo_message_send(conn, mm);
}

mongo_reply * mongo_read_response( mongo_connection * conn ){
    mongo_header head; /* header from network */
    mongo_reply_fields fields; /* header from network */
    mongo_reply * out; /* native endian */
    int len;

    looping_read(conn, &head, sizeof(head));
    looping_read(conn, &fields, sizeof(fields));

    bson_little_endian32(&len, &head.len);
    out = (mongo_reply*)bson_malloc(len);

    out->head.len = len;
    bson_little_endian32(&out->head.id, &head.id);
    bson_little_endian32(&out->head.responseTo, &head.responseTo);
    bson_little_endian32(&out->head.op, &head.op);

    bson_little_endian32(&out->fields.flag, &fields.flag);
    bson_little_endian64(&out->fields.cursorID, &fields.cursorID);
    bson_little_endian32(&out->fields.start, &fields.start);
    bson_little_endian32(&out->fields.num, &fields.num);

    MONGO_TRY{
        looping_read(conn, &out->objs, len-sizeof(head)-sizeof(fields));
    }MONGO_CATCH{
        free(out);
        MONGO_RETHROW();
    }

    return out;
}

mongo_cursor* mongo_find(mongo_connection* conn, const char* ns, bson* query, bson* fields, int nToReturn, int nToSkip, int options){
    int sl;
    mongo_cursor * cursor;
    char * data;
    mongo_message * mm = mongo_message_create( 16 + /* header */
                                               4 + /*  options */
                                               strlen( ns ) + 1 + /* ns */
                                               4 + 4 + /* skip,return */
                                               bson_size( query ) +
                                               bson_size( fields ) ,
                                               0 , 0 , mongo_op_query );


    data = &mm->data;
    data = mongo_data_append32( data , &options );
    data = mongo_data_append( data , ns , strlen( ns ) + 1 );    
    data = mongo_data_append32( data , &nToSkip );
    data = mongo_data_append32( data , &nToReturn );
    data = mongo_data_append( data , query->data , bson_size( query ) );    
    if ( fields )
        data = mongo_data_append( data , fields->data , bson_size( fields ) );    
    
    bson_fatal_msg( (data == ((char*)mm) + mm->head.len), "query building fail!" );

    mongo_message_send( conn , mm );

    cursor = (mongo_cursor*)bson_malloc(sizeof(mongo_cursor));

    MONGO_TRY{
        cursor->mm = mongo_read_response(conn);
    }MONGO_CATCH{
        free(cursor);
        MONGO_RETHROW();
    }

    sl = strlen(ns)+1;
    cursor->ns = bson_malloc(sl);
    if (!cursor->ns){
        free(cursor->mm);
        free(cursor);
        return 0;
    }
    memcpy((void*)cursor->ns, ns, sl); /* cast needed to silence GCC warning */
    cursor->conn = conn;
    cursor->current.data = NULL;
    return cursor;
}

bson_bool_t mongo_find_one(mongo_connection* conn, const char* ns, bson* query, bson* fields, bson* out){
    mongo_cursor* cursor = mongo_find(conn, ns, query, fields, 1, 0, 0);

    if (cursor && mongo_cursor_next(cursor)){
        bson_copy(out, &cursor->current);
        mongo_cursor_destroy(cursor);
        return 1;
    }else{
        mongo_cursor_destroy(cursor);
        return 0;
    }
}

int64_t mongo_count(mongo_connection* conn, const char* db, const char* ns, bson* query){
    bson_buffer bb;
    bson cmd;
    bson out;
    int64_t count = -1;

    bson_buffer_init(&bb);
    bson_append_string(&bb, "count", ns);
    if (query && bson_size(query) > 5) /* not empty */
        bson_append_bson(&bb, "query", query);
    bson_from_buffer(&cmd, &bb);

    MONGO_TRY{
        if(mongo_run_command(conn, db, &cmd, &out)){
            bson_iterator it;
            if(bson_find(&it, &out, "n"))
                count = bson_iterator_long(&it);
        }
    }MONGO_CATCH{
        bson_destroy(&cmd);
        MONGO_RETHROW();
    }
    
    bson_destroy(&cmd);
    bson_destroy(&out);
    return count;
}

bson_bool_t mongo_disconnect( mongo_connection * conn ){
    if ( ! conn->connected )
        return 1;

#ifdef _WIN32
    closesocket( conn->sock );
#else
    close( conn->sock );
#endif
    
    conn->sock = 0;
    conn->connected = 0;
    
    return 0;
}

bson_bool_t mongo_destroy( mongo_connection * conn ){
    free(conn->left_opts);
    free(conn->right_opts);
    conn->left_opts = NULL;
    conn->right_opts = NULL;

    return mongo_disconnect( conn );
}

bson_bool_t mongo_cursor_get_more(mongo_cursor* cursor){
    if (cursor->mm && cursor->mm->fields.cursorID){
        mongo_connection* conn = cursor->conn;
        char* data;
        int sl = strlen(cursor->ns)+1;
        mongo_message * mm = mongo_message_create(16 /*header*/
                                                 +4 /*ZERO*/
                                                 +sl
                                                 +4 /*numToReturn*/
                                                 +8 /*cursorID*/
                                                 , 0, 0, mongo_op_get_more);
        data = &mm->data;
        data = mongo_data_append32(data, &zero);
        data = mongo_data_append(data, cursor->ns, sl);
        data = mongo_data_append32(data, &zero);
        data = mongo_data_append64(data, &cursor->mm->fields.cursorID);
        mongo_message_send(conn, mm);

        free(cursor->mm);

        MONGO_TRY{
            cursor->mm = mongo_read_response(cursor->conn);
        }MONGO_CATCH{
            cursor->mm = NULL;
            mongo_cursor_destroy(cursor);
            MONGO_RETHROW();
        }

        return cursor->mm && cursor->mm->fields.num;
    } else{
        return 0;
    }
}

bson_bool_t mongo_cursor_next(mongo_cursor* cursor){
    char* bson_addr;

    /* no data */
    if (!cursor->mm || cursor->mm->fields.num == 0)
        return 0;

    /* first */
    if (cursor->current.data == NULL){
        bson_init(&cursor->current, &cursor->mm->objs, 0);
        return 1;
    }

    bson_addr = cursor->current.data + bson_size(&cursor->current);
    if (bson_addr >= ((char*)cursor->mm + cursor->mm->head.len)){
        if (!mongo_cursor_get_more(cursor))
            return 0;
        bson_init(&cursor->current, &cursor->mm->objs, 0);
    } else {
        bson_init(&cursor->current, bson_addr, 0);
    }

    return 1;
}

void mongo_cursor_destroy(mongo_cursor* cursor){
    if (!cursor) return;

    if (cursor->mm && cursor->mm->fields.cursorID){
        mongo_connection* conn = cursor->conn;
        mongo_message * mm = mongo_message_create(16 /*header*/
                                                 +4 /*ZERO*/
                                                 +4 /*numCursors*/
                                                 +8 /*cursorID*/
                                                 , 0, 0, mongo_op_kill_cursors);
        char* data = &mm->data;
        data = mongo_data_append32(data, &zero);
        data = mongo_data_append32(data, &one);
        data = mongo_data_append64(data, &cursor->mm->fields.cursorID);
        
        MONGO_TRY{
            mongo_message_send(conn, mm);
        }MONGO_CATCH{
            free(cursor->mm);
            free((void*)cursor->ns);
            free(cursor);
            MONGO_RETHROW();
        }
    }
        
    free(cursor->mm);
    free((void*)cursor->ns);
    free(cursor);
}

bson_bool_t mongo_create_index(mongo_connection * conn, const char * ns, bson * key, int options, bson * out){
    bson_buffer bb;
    bson b;
    bson_iterator it;
    char name[255] = {'_'};
    int i = 1;
    char idxns[1024];

    bson_iterator_init(&it, key->data);
    while(i < 255 && bson_iterator_next(&it)){
        strncpy(name + i, bson_iterator_key(&it), 255 - i);
        i += strlen(bson_iterator_key(&it));
    }
    name[254] = '\0';

    bson_buffer_init(&bb);
    bson_append_bson(&bb, "key", key);
    bson_append_string(&bb, "ns", ns);
    bson_append_string(&bb, "name", name);
    if (options & MONGO_INDEX_UNIQUE)
        bson_append_bool(&bb, "unique", 1);
    if (options & MONGO_INDEX_DROP_DUPS)
        bson_append_bool(&bb, "dropDups", 1);
    
    bson_from_buffer(&b, &bb);

    strncpy(idxns, ns, 1024-16);
    strcpy(strchr(idxns, '.'), ".system.indexes");
    mongo_insert(conn, idxns, &b);
    bson_destroy(&b);

    *strchr(idxns, '.') = '\0'; /* just db not ns */
    return !mongo_cmd_get_last_error(conn, idxns, out);
}
bson_bool_t mongo_create_simple_index(mongo_connection * conn, const char * ns, const char* field, int options, bson * out){
    bson_buffer bb;
    bson b;
    bson_bool_t success;

    bson_buffer_init(&bb);
    bson_append_int(&bb, field, 1);
    bson_from_buffer(&b, &bb);

    success = mongo_create_index(conn, ns, &b, options, out);
    bson_destroy(&b);
    return success;
}

bson_bool_t mongo_run_command(mongo_connection * conn, const char * db, bson * command, bson * out){
    bson fields;
    int sl = strlen(db);
    char* ns = bson_malloc(sl + 5 + 1); /* ".$cmd" + nul */
    bson_bool_t success;

    strcpy(ns, db);
    strcpy(ns+sl, ".$cmd");

    success = mongo_find_one(conn, ns, command, bson_empty(&fields), out);
    free(ns);
    return success;
}
bson_bool_t mongo_simple_int_command(mongo_connection * conn, const char * db, const char* cmdstr, int arg, bson * realout){
    bson out;
    bson cmd;
    bson_buffer bb;
    bson_bool_t success = 0;

    bson_buffer_init(&bb);
    bson_append_int(&bb, cmdstr, arg);
    bson_from_buffer(&cmd, &bb);

    if(mongo_run_command(conn, db, &cmd, &out)){
        bson_iterator it;
        if(bson_find(&it, &out, "ok"))
            success = bson_iterator_bool(&it);
    }
    
    bson_destroy(&cmd);

    if (realout)
        *realout = out;
    else
        bson_destroy(&out);

    return success;
}

bson_bool_t mongo_simple_str_command(mongo_connection * conn, const char * db, const char* cmdstr, const char* arg, bson * realout){
    bson out;
    bson cmd;
    bson_buffer bb;
    bson_bool_t success = 0;

    bson_buffer_init(&bb);
    bson_append_string(&bb, cmdstr, arg);
    bson_from_buffer(&cmd, &bb);

    if(mongo_run_command(conn, db, &cmd, &out)){
        bson_iterator it;
        if(bson_find(&it, &out, "ok"))
            success = bson_iterator_bool(&it);
    }
    
    bson_destroy(&cmd);

    if (realout)
        *realout = out;
    else
        bson_destroy(&out);

    return success;
}

bson_bool_t mongo_cmd_drop_db(mongo_connection * conn, const char * db){
    return mongo_simple_int_command(conn, db, "dropDatabase", 1, NULL);
}

bson_bool_t mongo_cmd_drop_collection(mongo_connection * conn, const char * db, const char * collection, bson * out){
    return mongo_simple_str_command(conn, db, "drop", collection, out);
}

void mongo_cmd_reset_error(mongo_connection * conn, const char * db){
    mongo_simple_int_command(conn, db, "reseterror", 1, NULL);
}

static bson_bool_t mongo_cmd_get_error_helper(mongo_connection * conn, const char * db, bson * realout, const char * cmdtype){
    bson out = {NULL,0};
    bson_bool_t haserror = 1;


    if(mongo_simple_int_command(conn, db, cmdtype, 1, &out)){
        bson_iterator it;
        haserror = (bson_find(&it, &out, "err") != bson_null);
    }
    
    if(realout)
        *realout = out; /* transfer of ownership */
    else
        bson_destroy(&out);

    return haserror;
}

bson_bool_t mongo_cmd_get_prev_error(mongo_connection * conn, const char * db, bson * out){
    return mongo_cmd_get_error_helper(conn, db, out, "getpreverror");
}
bson_bool_t mongo_cmd_get_last_error(mongo_connection * conn, const char * db, bson * out){
    return mongo_cmd_get_error_helper(conn, db, out, "getlasterror");
}

bson_bool_t mongo_cmd_ismaster(mongo_connection * conn, bson * realout){
    bson out = {NULL,0};
    bson_bool_t ismaster = 0;

    if (mongo_simple_int_command(conn, "admin", "ismaster", 1, &out)){
        bson_iterator it;
        bson_find(&it, &out, "ismaster");
        ismaster = bson_iterator_bool(&it);
    }

    if(realout)
        *realout = out; /* transfer of ownership */
    else
        bson_destroy(&out);

    return ismaster;
}

static void digest2hex(mongo_md5_byte_t digest[16], char hex_digest[33]){
    static const char hex[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
    int i;
    for (i=0; i<16; i++){
        hex_digest[2*i]     = hex[(digest[i] & 0xf0) >> 4];
        hex_digest[2*i + 1] = hex[ digest[i] & 0x0f      ];
    }
    hex_digest[32] = '\0';
}

static void mongo_pass_digest(const char* user, const char* pass, char hex_digest[33]){
    mongo_md5_state_t st;
    mongo_md5_byte_t digest[16];

    mongo_md5_init(&st);
    mongo_md5_append(&st, (const mongo_md5_byte_t*)user, strlen(user));
    mongo_md5_append(&st, (const mongo_md5_byte_t*)":mongo:", 7);
    mongo_md5_append(&st, (const mongo_md5_byte_t*)pass, strlen(pass));
    mongo_md5_finish(&st, digest);
    digest2hex(digest, hex_digest);
}

void mongo_cmd_add_user(mongo_connection* conn, const char* db, const char* user, const char* pass){
    bson_buffer bb;
    bson user_obj;
    bson pass_obj;
    char hex_digest[33];
    char* ns = malloc(strlen(db) + strlen(".system.users") + 1);

    strcpy(ns, db);
    strcpy(ns+strlen(db), ".system.users");

    mongo_pass_digest(user, pass, hex_digest);

    bson_buffer_init(&bb);
    bson_append_string(&bb, "user", user);
    bson_from_buffer(&user_obj, &bb);

    bson_buffer_init(&bb);
    bson_append_start_object(&bb, "$set");
    bson_append_string(&bb, "pwd", hex_digest);
    bson_append_finish_object(&bb);
    bson_from_buffer(&pass_obj, &bb);


    MONGO_TRY{
        mongo_update(conn, ns, &user_obj, &pass_obj, MONGO_UPDATE_UPSERT);
    }MONGO_CATCH{
        free(ns);
        bson_destroy(&user_obj);
        bson_destroy(&pass_obj);
        MONGO_RETHROW();
    }

    free(ns);
    bson_destroy(&user_obj);
    bson_destroy(&pass_obj);
}

bson_bool_t mongo_cmd_authenticate(mongo_connection* conn, const char* db, const char* user, const char* pass){
    bson_buffer bb;
    bson from_db, auth_cmd;
    const char* nonce;
    bson_bool_t success = 0;

    mongo_md5_state_t st;
    mongo_md5_byte_t digest[16];
    char hex_digest[33];

    if (mongo_simple_int_command(conn, db, "getnonce", 1, &from_db)){
        bson_iterator it;
        bson_find(&it, &from_db, "nonce");
        nonce = bson_iterator_string(&it);
    }else{
        return 0;
    }

    mongo_pass_digest(user, pass, hex_digest);

    mongo_md5_init(&st);
    mongo_md5_append(&st, (const mongo_md5_byte_t*)nonce, strlen(nonce));
    mongo_md5_append(&st, (const mongo_md5_byte_t*)user, strlen(user));
    mongo_md5_append(&st, (const mongo_md5_byte_t*)hex_digest, 32);
    mongo_md5_finish(&st, digest);
    digest2hex(digest, hex_digest);

    bson_buffer_init(&bb);
    bson_append_int(&bb, "authenticate", 1);
    bson_append_string(&bb, "user", user);
    bson_append_string(&bb, "nonce", nonce);
    bson_append_string(&bb, "key", hex_digest);
    bson_from_buffer(&auth_cmd, &bb);

    bson_destroy(&from_db);

    MONGO_TRY{
        if(mongo_run_command(conn, db, &auth_cmd, &from_db)){
            bson_iterator it;
            if(bson_find(&it, &from_db, "ok"))
                success = bson_iterator_bool(&it);
        }
    }MONGO_CATCH{
        bson_destroy(&auth_cmd);
        MONGO_RETHROW();
    }

    bson_destroy(&from_db);
    bson_destroy(&auth_cmd);

    return success;
}

/*==============================================================*/

static void rpmmdbFini(void * _mdb)
	/*@globals fileSystem @*/
	/*@modifies *_mdb, fileSystem @*/
{
    rpmmdb mdb = _mdb;

    mdb->fn = _free(mdb->fn);
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmmdbPool = NULL;

static rpmmdb rpmmdbGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmmdbPool, fileSystem @*/
	/*@modifies pool, _rpmmdbPool, fileSystem @*/
{
    rpmmdb mdb;

    if (_rpmmdbPool == NULL) {
	_rpmmdbPool = rpmioNewPool("mdb", sizeof(*mdb), -1, _rpmmdb_debug,
			NULL, NULL, rpmmdbFini);
	pool = _rpmmdbPool;
    }
    mdb = (rpmmdb) rpmioGetPool(pool, sizeof(*mdb));
    memset(((char *)mdb)+sizeof(mdb->_item), 0, sizeof(*mdb)-sizeof(mdb->_item));
    return mdb;
}

rpmmdb rpmmdbNew(const char * fn, int flags)
{
    rpmmdb mdb = rpmmdbGetPool(_rpmmdbPool);
    int xx;

    if (fn)
	mdb->fn = xstrdup(fn);

    return rpmmdbLink(mdb);
}

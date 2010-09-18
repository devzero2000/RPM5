#define ASSERT(x) \
    do{ \
        if(!(x)){ \
            printf("failed assert (%d): %s\n", __LINE__,  #x); \
            exit(1); \
        }\
    }while(0)

#define	TEST_SERVER	"127.0.0.1"

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

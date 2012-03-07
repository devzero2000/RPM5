/* helpers.c */

#include "system.h"

#include "test.h"
#include "mongo.h"

#include "debug.h"

void test_index_helper( mongo *conn ) {

    bson b, out;
    bson_iterator it;

    bson_init( &b );
    bson_append_int( &b, "foo", 1 );
    bson_finish( &b );

    mongo_create_index( conn, "test.bar", &b, MONGO_INDEX_SPARSE | MONGO_INDEX_UNIQUE, &out );

    bson_destroy( &b );

    bson_init( &b );
    bson_append_start_object( &b, "key" );
    bson_append_int( &b, "foo", 1 );
    bson_append_finish_object( &b );

    bson_finish( &b );

    mongo_find_one( conn, "test.system.indexes", &b, NULL, &out );

    bson_print( &out );

    bson_iterator_init( &it, &out );

    ASSERT( bson_find( &it, &out, "unique" ) );
    ASSERT( bson_find( &it, &out, "sparse" ) );
}

int main(int argc, char *argv[])
{
    const char * test_server = (argc > 1 ? argv[1] : TEST_SERVER);

    mongo conn[1];

    if( mongo_connect( conn, test_server, 27017 ) != MONGO_OK ) {
        printf( "Failed to connect" );
        exit( 1 );
    }


    test_index_helper( conn );

    return 0;
}

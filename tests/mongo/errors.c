#include "system.h"

#include "test.h"
#include "mongo.h"

#include "debug.h"

static mongo_connection conn[1];
static mongo_connection_options opts;
static const char* db = "test";
static const char* ns = "test.c.error";

int main(int argc, char *argv[])
{
    bson obj;

    strncpy(opts.host, (argc > 1 ? argv[1] : TEST_SERVER), 255);
    opts.host[254] = '\0';
    opts.port = 27017;

    if (mongo_connect( conn , &opts )){
        printf("failed to connect\n");
        exit(1);
    }


    /*********************/
    ASSERT(!mongo_cmd_get_prev_error(conn, db, NULL));
    ASSERT(!mongo_cmd_get_last_error(conn, db, NULL));

    ASSERT(!mongo_cmd_get_prev_error(conn, db, &obj));
    bson_destroy(&obj);

    ASSERT(!mongo_cmd_get_last_error(conn, db, &obj));
    bson_destroy(&obj);

    /*********************/
    mongo_simple_int_command(conn, db, "forceerror", 1, NULL);

    ASSERT(mongo_cmd_get_prev_error(conn, db, NULL));
    ASSERT(mongo_cmd_get_last_error(conn, db, NULL));

    ASSERT(mongo_cmd_get_prev_error(conn, db, &obj));
    bson_destroy(&obj);

    ASSERT(mongo_cmd_get_last_error(conn, db, &obj));
    bson_destroy(&obj);

    /* should clear lasterror but not preverror */
    mongo_find_one(conn, ns, bson_empty(&obj), bson_empty(&obj), NULL);

    ASSERT(mongo_cmd_get_prev_error(conn, db, NULL));
    ASSERT(!mongo_cmd_get_last_error(conn, db, NULL));

    ASSERT(mongo_cmd_get_prev_error(conn, db, &obj));
    bson_destroy(&obj);

    ASSERT(!mongo_cmd_get_last_error(conn, db, &obj));
    bson_destroy(&obj);

    /*********************/
    mongo_cmd_reset_error(conn, db);

    ASSERT(!mongo_cmd_get_prev_error(conn, db, NULL));
    ASSERT(!mongo_cmd_get_last_error(conn, db, NULL));

    ASSERT(!mongo_cmd_get_prev_error(conn, db, &obj));
    bson_destroy(&obj);

    ASSERT(!mongo_cmd_get_last_error(conn, db, &obj));
    bson_destroy(&obj);

    return 0;
}

/* resize.c */

#include "system.h"

#include "test.h"
#include "bson.h"

#include "debug.h"

/* 64 Xs */
const char* bigstring = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";

int main(int argc, char *argv[])
{
    bson_buffer bb;
    bson b;

    bson_buffer_init(&bb);
    bson_append_string(&bb, "a", bigstring);
    bson_append_start_object(&bb, "sub");
        bson_append_string(&bb,"a", bigstring);
        bson_append_start_object(&bb, "sub");
            bson_append_string(&bb,"a", bigstring);
            bson_append_start_object(&bb, "sub");
                bson_append_string(&bb,"a", bigstring);
                bson_append_string(&bb,"b", bigstring);
                bson_append_string(&bb,"c", bigstring);
                bson_append_string(&bb,"d", bigstring);
                bson_append_string(&bb,"e", bigstring);
                bson_append_string(&bb,"f", bigstring);
            bson_append_finish_object(&bb);
        bson_append_finish_object(&bb);
    bson_append_finish_object(&bb);
    bson_from_buffer(&b, &bb);

    /* bson_print(&b); */
    bson_destroy(&b);
    return 0;
}

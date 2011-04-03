/* test.c */

#include "system.h"

#include "test.h"
#include "mongo.h"

#include "debug.h"


int main(int argc, char *argv[])
{
    mongo_connection conn[1];

#ifdef	NOTYET
    INIT_SOCKETS_FOR_WINDOWS;
#else
    strncpy(conn.host, (argc > 1 ? argv[1] : TEST_SERVER), 255);
    conn.host[254] = '\0';
    conn.port = 27017;
#endif

    mongo_replset_init_conn( conn );
    mongo_replset_add_seed( conn, TEST_SERVER, 30000 );
    mongo_replset_add_seed( conn, TEST_SERVER, 30001 );
    mongo_host_port* p = conn->seeds;

    while( p != NULL ) {
      p = p->next;
    }

    if( mongo_replset_connect( conn ) ) {
      printf("Failed to connect.");
      exit(1);
    }

    return 0;
}

/** \ingroup rpmio
 * \file rpmio/mongo.c
 */

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

#include "system.h"

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>

#include <rpmiotypes.h>
#include <rpmio.h>	/* for *Pool methods */
#include <rpmlog.h>
#include <rpmurl.h>

#define	_RPMMGO_INTERNAL
#include <mongo.h>

#include "debug.h"

/*@unchecked@*/
int _rpmmgo_debug = 0;

/*@unchecked@*/ /*@relnull@*/
rpmmgo _rpmmgoI;

/* only need one of these */
static const int zero = 0;
static const int one = 1;

/*==============================================================*/
/* --- gridfs.h */

enum {DEFAULT_CHUNK_SIZE = 256 * 1024};

typedef uint64_t gridfs_offset;

/* A GridFS represents a single collection of GridFS files in the database. */
typedef struct {
    mongo *client; /**> The client to db-connection. */
    const char *dbname; /**> The root database name */
    const char *prefix; /**> The prefix of the GridFS's collections, default is NULL */
    const char *files_ns; /**> The namespace where the file's metadata is stored */
    const char *chunks_ns; /**. The namespace where the files's data is stored in chunks */
} gridfs;

/* A GridFile is a single GridFS file. */
typedef struct {
    gridfs *gfs;        /**> The GridFS where the GridFile is located */
    bson *meta;         /**> The GridFile's bson object where all its metadata is located */
    gridfs_offset pos;  /**> The position is the offset in the file */
    bson_oid_t id;      /**> The files_id of the gridfile */
    char *remote_name;  /**> The name of the gridfile as a string */
    char *content_type; /**> The gridfile's content type */
    gridfs_offset length; /**> The length of this gridfile */
    int chunk_num;      /**> The number of the current chunk being written to */
    char *pending_data; /**> A buffer storing data still to be written to chunks */
    int pending_len;    /**> Length of pending_data buffer */
} gridfile;

MONGO_EXPORT gridfs* gridfs_create(void);
MONGO_EXPORT void gridfs_dispose(gridfs* gfs);
MONGO_EXPORT gridfile* gridfile_create(void);
MONGO_EXPORT void gridfile_dispose(gridfile* gf);
MONGO_EXPORT void gridfile_get_descriptor(gridfile* gf, bson* out);

/**
 *  Initializes a GridFS object
 *  @param client - db connection
 *  @param dbname - database name
 *  @param prefix - collection prefix, default is fs if NULL or empty
 *  @param gfs - the GridFS object to initialize
 *
 *  @return - MONGO_OK or MONGO_ERROR.
 */
MONGO_EXPORT int gridfs_init( mongo *client, const char *dbname,
                 const char *prefix, gridfs *gfs );

/**
 * Destroys a GridFS object. Call this when finished with
 * the object..
 *
 * @param gfs a grid
 */
MONGO_EXPORT void gridfs_destroy( gridfs *gfs );

/**
 *  Initializes a gridfile for writing incrementally with gridfs_write_buffer.
 *  Once initialized, you can write any number of buffers with gridfs_write_buffer.
 *  When done, you must call gridfs_writer_done to save the file metadata.
 *
 */
MONGO_EXPORT void gridfile_writer_init( gridfile *gfile, gridfs *gfs, const char *remote_name,
                           const char *content_type );

/**
 *  Write to a GridFS file incrementally. You can call this function any number
 *  of times with a new buffer each time. This allows you to effectively
 *  stream to a GridFS file. When finished, be sure to call gridfs_writer_done.
 *
 */
MONGO_EXPORT void gridfile_write_buffer( gridfile *gfile, const char *data,
                            gridfs_offset length );

/**
 *  Signal that writing of this gridfile is complete by
 *  writing any buffered chunks along with the entry in the
 *  files collection.
 *
 *  @return - MONGO_OK or MONGO_ERROR.
 */
MONGO_EXPORT int gridfile_writer_done( gridfile *gfile );

/**
 *  Store a buffer as a GridFS file.
 *  @param gfs - the working GridFS
 *  @param data - pointer to buffer to store in GridFS
 *  @param length - length of the buffer
 *  @param remotename - filename for use in the database
 *  @param contenttype - optional MIME type for this object
 *
 *  @return - MONGO_OK or MONGO_ERROR.
 */
MONGO_EXPORT int gridfs_store_buffer( gridfs *gfs, const char *data, gridfs_offset length,
                          const char *remotename,
                          const char *contenttype );

/**
 *  Open the file referenced by filename and store it as a GridFS file.
 *  @param gfs - the working GridFS
 *  @param filename - local filename relative to the process
 *  @param remotename - optional filename for use in the database
 *  @param contenttype - optional MIME type for this object
 *
 *  @return - MONGO_OK or MONGO_ERROR.
 */
MONGO_EXPORT int gridfs_store_file( gridfs *gfs, const char *filename,
                        const char *remotename, const char *contenttype );

/**
 *  Removes the files referenced by filename from the db
 *  @param gfs - the working GridFS
 *  @param filename - the filename of the file/s to be removed
 */
MONGO_EXPORT void gridfs_remove_filename( gridfs *gfs, const char *filename );

/**
 *  Find the first file matching the provided query within the
 *  GridFS files collection, and return the file as a GridFile.
 *
 *  @param gfs - the working GridFS
 *  @param query - a pointer to the bson with the query data
 *  @param gfile - the output GridFile to be initialized
 *
 *  @return MONGO_OK if successful, MONGO_ERROR otherwise
 */
MONGO_EXPORT int gridfs_find_query( gridfs *gfs, bson *query, gridfile *gfile );

/**
 *  Find the first file referenced by filename within the GridFS
 *  and return it as a GridFile
 *  @param gfs - the working GridFS
 *  @param filename - filename of the file to find
 *  @param gfile - the output GridFile to be intialized
 *
 *  @return MONGO_OK or MONGO_ERROR.
 */
int gridfs_find_filename( gridfs *gfs, const char *filename, gridfile *gfile );

/**
 *  Initializes a GridFile containing the GridFS and file bson
 *  @param gfs - the GridFS where the GridFile is located
 *  @param meta - the file object
 *  @param gfile - the output GridFile that is being initialized
 *
 *  @return - MONGO_OK or MONGO_ERROR.
 */
int gridfile_init( gridfs *gfs, bson *meta, gridfile *gfile );

/**
 *  Destroys the GridFile
 *
 *  @param oGridFIle - the GridFile being destroyed
 */
MONGO_EXPORT void gridfile_destroy( gridfile *gfile );

/**
 *  Returns whether or not the GridFile exists
 *  @param gfile - the GridFile being examined
 */
bson_bool_t gridfile_exists( gridfile *gfile );

/**
 *  Returns the filename of GridFile
 *  @param gfile - the working GridFile
 *
 *  @return - the filename of the Gridfile
 */
MONGO_EXPORT const char *gridfile_get_filename( gridfile *gfile );

/**
 *  Returns the size of the chunks of the GridFile
 *  @param gfile - the working GridFile
 *
 *  @return - the size of the chunks of the Gridfile
 */
MONGO_EXPORT int gridfile_get_chunksize( gridfile *gfile );

/**
 *  Returns the length of GridFile's data
 *
 *  @param gfile - the working GridFile
 *
 *  @return - the length of the Gridfile's data
 */
MONGO_EXPORT gridfs_offset gridfile_get_contentlength( gridfile *gfile );

/**
 *  Returns the MIME type of the GridFile
 *
 *  @param gfile - the working GridFile
 *
 *  @return - the MIME type of the Gridfile
 *            (NULL if no type specified)
 */
MONGO_EXPORT const char *gridfile_get_contenttype( gridfile *gfile );

/**
 *  Returns the upload date of GridFile
 *
 *  @param gfile - the working GridFile
 *
 *  @return - the upload date of the Gridfile
 */
MONGO_EXPORT bson_date_t gridfile_get_uploaddate( gridfile *gfile );

/**
 *  Returns the MD5 of GridFile
 *
 *  @param gfile - the working GridFile
 *
 *  @return - the MD5 of the Gridfile
 */
MONGO_EXPORT const char *gridfile_get_md5( gridfile *gfile );

/**
 *  Returns the field in GridFile specified by name
 *
 *  @param gfile - the working GridFile
 *  @param name - the name of the field to be returned
 *
 *  @return - the data of the field specified
 *            (NULL if none exists)
 */
const char *gridfile_get_field( gridfile *gfile,
                                const char *name );

/**
 *  Returns a boolean field in GridFile specified by name
 *  @param gfile - the working GridFile
 *  @param name - the name of the field to be returned
 *
 *  @return - the boolean of the field specified
 *            (NULL if none exists)
 */
bson_bool_t gridfile_get_boolean( gridfile *gfile,
                                  const char *name );

/**
 *  Returns the metadata of GridFile
 *  @param gfile - the working GridFile
 *
 *  @return - the metadata of the Gridfile in a bson object
 *            (an empty bson is returned if none exists)
 */
MONGO_EXPORT void gridfile_get_metadata( gridfile *gfile, bson* out );

/**
 *  Returns the number of chunks in the GridFile
 *  @param gfile - the working GridFile
 *
 *  @return - the number of chunks in the Gridfile
 */
MONGO_EXPORT int gridfile_get_numchunks( gridfile *gfile );

/**
 *  Returns chunk n of GridFile
 *  @param gfile - the working GridFile
 *
 *  @return - the nth chunk of the Gridfile
 */
MONGO_EXPORT void gridfile_get_chunk( gridfile *gfile, int n, bson* out );

/**
 *  Returns a mongo_cursor of *size* chunks starting with chunk *start*
 *
 *  @param gfile - the working GridFile
 *  @param start - the first chunk in the cursor
 *  @param size - the number of chunks to be returned
 *
 *  @return - mongo_cursor of the chunks (must be destroyed after use)
 */
MONGO_EXPORT mongo_cursor *gridfile_get_chunks( gridfile *gfile, int start, int size );

/**
 *  Writes the GridFile to a stream
 *
 *  @param gfile - the working GridFile
 *  @param stream - the file stream to write to
 */
gridfs_offset gridfile_write_file( gridfile *gfile, FILE *stream );

/**
 *  Reads length bytes from the GridFile to a buffer
 *  and updates the position in the file.
 *  (assumes the buffer is large enough)
 *  (if size is greater than EOF gridfile_read reads until EOF)
 *
 *  @param gfile - the working GridFile
 *  @param size - the amount of bytes to be read
 *  @param buf - the buffer to read to
 *
 *  @return - the number of bytes read
 */
MONGO_EXPORT gridfs_offset gridfile_read( gridfile *gfile, gridfs_offset size, char *buf );

/**
 *  Updates the position in the file
 *  (If the offset goes beyond the contentlength,
 *  the position is updated to the end of the file.)
 *
 *  @param gfile - the working GridFile
 *  @param offset - the position to update to
 *
 *  @return - resulting offset location
 */
MONGO_EXPORT gridfs_offset gridfile_seek( gridfile *gfile, gridfs_offset offset );

/*==============================================================*/
/* --- gridfs.c */

MONGO_EXPORT gridfs* gridfs_create(void) {
    return (gridfs*)bson_malloc(sizeof(gridfs));
}

MONGO_EXPORT void gridfs_dispose(gridfs* gfs) {
    free(gfs);
}

MONGO_EXPORT gridfile* gridfile_create() {
    return (gridfile*)bson_malloc(sizeof(gridfile));
}

MONGO_EXPORT void gridfile_dispose(gridfile* gf) {
    free(gf);
}

MONGO_EXPORT void gridfile_get_descriptor(gridfile* gf, bson* out) {
    *out = *gf->meta;
}


static bson *chunk_new( bson_oid_t id, int chunkNumber,
                        const char *data, int len ) {
    bson *b = (bson *) bson_malloc( sizeof( bson ) );

    bson_init( b );
    bson_append_oid( b, "files_id", &id );
    bson_append_int( b, "n", chunkNumber );
    bson_append_binary( b, "data", BSON_BIN_BINARY, data, len );
    bson_finish( b );
    return  b;
}

static void chunk_free( bson *oChunk ) {
    bson_destroy( oChunk );
    bson_free( oChunk );
}

int gridfs_init( mongo *client, const char *dbname, const char *prefix,
    gridfs *gfs ) {

    int options;
    bson b;
    bson_bool_t success;

    gfs->client = client;

    /* Allocate space to own the dbname */
    gfs->dbname = ( const char * )bson_malloc( strlen( dbname )+1 );
    strcpy( ( char * )gfs->dbname, dbname );

    /* Allocate space to own the prefix */
    if ( prefix == NULL ) prefix = "fs";
    gfs->prefix = ( const char * )bson_malloc( strlen( prefix )+1 );
    strcpy( ( char * )gfs->prefix, prefix );

    /* Allocate space to own files_ns */
    gfs->files_ns =
        ( const char * ) bson_malloc ( strlen( prefix )+strlen( dbname )+strlen( ".files" )+2 );
    strcpy( ( char * )gfs->files_ns, dbname );
    strcat( ( char * )gfs->files_ns, "." );
    strcat( ( char * )gfs->files_ns, prefix );
    strcat( ( char * )gfs->files_ns, ".files" );

    /* Allocate space to own chunks_ns */
    gfs->chunks_ns = ( const char * ) bson_malloc( strlen( prefix ) + strlen( dbname )
                     + strlen( ".chunks" ) + 2 );
    strcpy( ( char * )gfs->chunks_ns, dbname );
    strcat( ( char * )gfs->chunks_ns, "." );
    strcat( ( char * )gfs->chunks_ns, prefix );
    strcat( ( char * )gfs->chunks_ns, ".chunks" );

    bson_init( &b );
    bson_append_int( &b, "filename", 1 );
    bson_finish( &b );
    options = 0;
    success = ( mongo_create_index( gfs->client, gfs->files_ns, &b, options, NULL ) == MONGO_OK );
    bson_destroy( &b );
    if ( !success ) {
        bson_free( ( char * )gfs->dbname );
        bson_free( ( char * )gfs->prefix );
        bson_free( ( char * )gfs->files_ns );
        bson_free( ( char * )gfs->chunks_ns );
        return MONGO_ERROR;
    }

    bson_init( &b );
    bson_append_int( &b, "files_id", 1 );
    bson_append_int( &b, "n", 1 );
    bson_finish( &b );
    options = MONGO_INDEX_UNIQUE;
    success = ( mongo_create_index( gfs->client, gfs->chunks_ns, &b, options, NULL ) == MONGO_OK );
    bson_destroy( &b );
    if ( !success ) {
        bson_free( ( char * )gfs->dbname );
        bson_free( ( char * )gfs->prefix );
        bson_free( ( char * )gfs->files_ns );
        bson_free( ( char * )gfs->chunks_ns );
        return MONGO_ERROR;
    }

    return MONGO_OK;
}

MONGO_EXPORT void gridfs_destroy( gridfs *gfs ) {
    if ( gfs == NULL ) return;
    if ( gfs->dbname ) bson_free( ( char * )gfs->dbname );
    if ( gfs->prefix ) bson_free( ( char * )gfs->prefix );
    if ( gfs->files_ns ) bson_free( ( char * )gfs->files_ns );
    if ( gfs->chunks_ns ) bson_free( ( char * )gfs->chunks_ns );
}

static int gridfs_insert_file( gridfs *gfs, const char *name,
                                const bson_oid_t id, gridfs_offset length,
                                const char *contenttype ) {
    bson command;
    bson ret;
    bson res;
    bson_iterator it;
    int result;
    int64_t d;

    /* Check run md5 */
    bson_init( &command );
    bson_append_oid( &command, "filemd5", &id );
    bson_append_string( &command, "root", gfs->prefix );
    bson_finish( &command );
    result = mongo_run_command( gfs->client, gfs->dbname, &command, &res );
    bson_destroy( &command );
    if (result != MONGO_OK)
        return result;

    /* Create and insert BSON for file metadata */
    bson_init( &ret );
    bson_append_oid( &ret, "_id", &id );
    if ( name != NULL && *name != '\0' ) {
        bson_append_string( &ret, "filename", name );
    }
    bson_append_long( &ret, "length", length );
    bson_append_int( &ret, "chunkSize", DEFAULT_CHUNK_SIZE );
    d = ( bson_date_t )1000*time( NULL );
    bson_append_date( &ret, "uploadDate", d);
    bson_find( &it, &res, "md5" );
    bson_append_string( &ret, "md5", bson_iterator_string( &it ) );
    bson_destroy( &res );
    if ( contenttype != NULL && *contenttype != '\0' ) {
        bson_append_string( &ret, "contentType", contenttype );
    }
    bson_finish( &ret );
    result = mongo_insert( gfs->client, gfs->files_ns, &ret );
    bson_destroy( &ret );

    return result;
}

MONGO_EXPORT int gridfs_store_buffer( gridfs *gfs, const char *data,
                          gridfs_offset length, const char *remotename,
                          const char *contenttype ) {

    char const *end = data + length;
    const char *data_ptr = data;
    bson_oid_t id;
    int chunkNumber = 0;
    int chunkLen;
    bson *oChunk;

    /* Large files Assertion */
    /* assert( length <= 0xffffffff ); */

    /* Generate and append an oid*/
    bson_oid_gen( &id );

    /* Insert the file's data chunk by chunk */
    while ( data_ptr < end ) {
        chunkLen = DEFAULT_CHUNK_SIZE < ( unsigned int )( end - data_ptr ) ?
                   DEFAULT_CHUNK_SIZE : ( unsigned int )( end - data_ptr );
        oChunk = chunk_new( id, chunkNumber, data_ptr, chunkLen );
        mongo_insert( gfs->client, gfs->chunks_ns, oChunk );
        chunk_free( oChunk );
        chunkNumber++;
        data_ptr += chunkLen;
    }

    /* Inserts file's metadata */
    return gridfs_insert_file( gfs, remotename, id, length, contenttype );
}

MONGO_EXPORT void gridfile_writer_init( gridfile *gfile, gridfs *gfs,
                           const char *remote_name, const char *content_type ) {
    gfile->gfs = gfs;

    bson_oid_gen( &( gfile->id ) );
    gfile->chunk_num = 0;
    gfile->length = 0;
    gfile->pending_len = 0;
    gfile->pending_data = NULL;

    gfile->remote_name = ( char * )bson_malloc( strlen( remote_name ) + 1 );
    strcpy( ( char * )gfile->remote_name, remote_name );

    gfile->content_type = ( char * )bson_malloc( strlen( content_type ) + 1 );
    strcpy( ( char * )gfile->content_type, content_type );
}

MONGO_EXPORT void gridfile_write_buffer( gridfile *gfile, const char *data,
    gridfs_offset length ) {

    int bytes_left = 0;
    int data_partial_len = 0;
    int chunks_to_write = 0;
    char *buffer;
    bson *oChunk;
    gridfs_offset to_write = length + gfile->pending_len;

    if ( to_write < DEFAULT_CHUNK_SIZE ) { /* Less than one chunk to write */
        if( gfile->pending_data ) {
            gfile->pending_data = ( char * )bson_realloc( ( void * )gfile->pending_data, gfile->pending_len + to_write );
            memcpy( gfile->pending_data + gfile->pending_len, data, length );
        } else if ( to_write > 0 ) {
            gfile->pending_data = ( char * )bson_malloc( to_write );
            memcpy( gfile->pending_data, data, length );
        }
        gfile->pending_len += length;

    } else { /* At least one chunk of data to write */

        /* If there's a pending chunk to be written, we need to combine
         * the buffer provided up to DEFAULT_CHUNK_SIZE.
         */
        if ( gfile->pending_len > 0 ) {
            chunks_to_write = to_write / DEFAULT_CHUNK_SIZE;
            bytes_left = to_write % DEFAULT_CHUNK_SIZE;

            data_partial_len = DEFAULT_CHUNK_SIZE - gfile->pending_len;
            buffer = ( char * )bson_malloc( DEFAULT_CHUNK_SIZE );
            memcpy( buffer, gfile->pending_data, gfile->pending_len );
            memcpy( buffer + gfile->pending_len, data, data_partial_len );

            oChunk = chunk_new( gfile->id, gfile->chunk_num, buffer, DEFAULT_CHUNK_SIZE );
            mongo_insert( gfile->gfs->client, gfile->gfs->chunks_ns, oChunk );
            chunk_free( oChunk );
            gfile->chunk_num++;
            gfile->length += DEFAULT_CHUNK_SIZE;
            data += data_partial_len;

            chunks_to_write--;

            bson_free( buffer );
        }

        while( chunks_to_write > 0 ) {
            oChunk = chunk_new( gfile->id, gfile->chunk_num, data, DEFAULT_CHUNK_SIZE );
            mongo_insert( gfile->gfs->client, gfile->gfs->chunks_ns, oChunk );
            chunk_free( oChunk );
            gfile->chunk_num++;
            chunks_to_write--;
            gfile->length += DEFAULT_CHUNK_SIZE;
            data += DEFAULT_CHUNK_SIZE;
        }

        bson_free( gfile->pending_data );

        /* If there are any leftover bytes, store them as pending data. */
        if( bytes_left == 0 )
            gfile->pending_data = NULL;
        else {
            gfile->pending_data = ( char * )bson_malloc( bytes_left );
            memcpy( gfile->pending_data, data, bytes_left );
        }

        gfile->pending_len = bytes_left;
    }
}

MONGO_EXPORT int gridfile_writer_done( gridfile *gfile ) {

    /* write any remaining pending chunk data.
     * pending data will always take up less than one chunk */
    bson *oChunk;
    int response;
    if( gfile->pending_data ) {
        oChunk = chunk_new( gfile->id, gfile->chunk_num, gfile->pending_data, gfile->pending_len );
        mongo_insert( gfile->gfs->client, gfile->gfs->chunks_ns, oChunk );
        chunk_free( oChunk );
        bson_free( gfile->pending_data );
        gfile->length += gfile->pending_len;
    }

    /* insert into files collection */
    response = gridfs_insert_file( gfile->gfs, gfile->remote_name, gfile->id,
                                   gfile->length, gfile->content_type );

    bson_free( gfile->remote_name );
    bson_free( gfile->content_type );

    return response;
}

int gridfs_store_file( gridfs *gfs, const char *filename,
                        const char *remotename, const char *contenttype ) {

    char buffer[DEFAULT_CHUNK_SIZE];
    FILE *fd;
    bson_oid_t id;
    int chunkNumber = 0;
    gridfs_offset length = 0;
    gridfs_offset chunkLen = 0;
    bson *oChunk;

    /* Open the file and the correct stream */
    if ( strcmp( filename, "-" ) == 0 ) fd = stdin;
    else {
        fd = fopen( filename, "rb" );
        if (fd == NULL)
            return MONGO_ERROR;
    }

    /* Generate and append an oid*/
    bson_oid_gen( &id );

    /* Insert the file chunk by chunk */
    chunkLen = fread( buffer, 1, DEFAULT_CHUNK_SIZE, fd );
    do {
        oChunk = chunk_new( id, chunkNumber, buffer, chunkLen );
        mongo_insert( gfs->client, gfs->chunks_ns, oChunk );
        chunk_free( oChunk );
        length += chunkLen;
        chunkNumber++;
        chunkLen = fread( buffer, 1, DEFAULT_CHUNK_SIZE, fd );
    } while ( chunkLen != 0 );

    /* Close the file stream */
    if ( fd != stdin ) fclose( fd );

    /* Large files Assertion */
    /* assert(length <= 0xffffffff); */

    /* Optional Remote Name */
    if ( remotename == NULL || *remotename == '\0' ) {
        remotename = filename;
    }

    /* Inserts file's metadata */
    return gridfs_insert_file( gfs, remotename, id, length, contenttype );
}

MONGO_EXPORT void gridfs_remove_filename( gridfs *gfs, const char *filename ) {
    bson query;
    mongo_cursor *files;
    bson file;
    bson_iterator it;
    bson_oid_t id;
    bson b;

    bson_init( &query );
    bson_append_string( &query, "filename", filename );
    bson_finish( &query );
    files = mongo_find( gfs->client, gfs->files_ns, &query, NULL, 0, 0, 0 );
    bson_destroy( &query );

    /* Remove each file and it's chunks from files named filename */
    while ( mongo_cursor_next( files ) == MONGO_OK ) {
        file = files->current;
        bson_find( &it, &file, "_id" );
        id = *bson_iterator_oid( &it );

        /* Remove the file with the specified id */
        bson_init( &b );
        bson_append_oid( &b, "_id", &id );
        bson_finish( &b );
        mongo_remove( gfs->client, gfs->files_ns, &b );
        bson_destroy( &b );

        /* Remove all chunks from the file with the specified id */
        bson_init( &b );
        bson_append_oid( &b, "files_id", &id );
        bson_finish( &b );
        mongo_remove( gfs->client, gfs->chunks_ns, &b );
        bson_destroy( &b );
    }

    mongo_cursor_destroy( files );
}

int gridfs_find_query( gridfs *gfs, bson *query,
                       gridfile *gfile ) {

    bson uploadDate;
    bson finalQuery;
    bson out;
    int i;

    bson_init( &uploadDate );
    bson_append_int( &uploadDate, "uploadDate", -1 );
    bson_finish( &uploadDate );

    bson_init( &finalQuery );
    bson_append_bson( &finalQuery, "query", query );
    bson_append_bson( &finalQuery, "orderby", &uploadDate );
    bson_finish( &finalQuery );

    i = ( mongo_find_one( gfs->client, gfs->files_ns,
                          &finalQuery, NULL, &out ) == MONGO_OK );
    bson_destroy( &uploadDate );
    bson_destroy( &finalQuery );
    if ( !i )
        return MONGO_ERROR;
    else {
        gridfile_init( gfs, &out, gfile );
        bson_destroy( &out );
        return MONGO_OK;
    }
}

int gridfs_find_filename( gridfs *gfs, const char *filename,
                          gridfile *gfile )

{
    bson query;
    int i;

    bson_init( &query );
    bson_append_string( &query, "filename", filename );
    bson_finish( &query );
    i = gridfs_find_query( gfs, &query, gfile );
    bson_destroy( &query );
    return i;
}

int gridfile_init( gridfs *gfs, bson *meta, gridfile *gfile )

{
    gfile->gfs = gfs;
    gfile->pos = 0;
    gfile->meta = ( bson * )bson_malloc( sizeof( bson ) );
    if ( gfile->meta == NULL ) return MONGO_ERROR;
    bson_copy( gfile->meta, meta );
    return MONGO_OK;
}

MONGO_EXPORT void gridfile_destroy( gridfile *gfile )

{
    bson_destroy( gfile->meta );
    bson_free( gfile->meta );
}

bson_bool_t gridfile_exists( gridfile *gfile ) {
    return ( bson_bool_t )( gfile != NULL || gfile->meta == NULL );
}

MONGO_EXPORT const char *gridfile_get_filename( gridfile *gfile ) {
    bson_iterator it;

    bson_find( &it, gfile->meta, "filename" );
    return bson_iterator_string( &it );
}

MONGO_EXPORT int gridfile_get_chunksize( gridfile *gfile ) {
    bson_iterator it;

    bson_find( &it, gfile->meta, "chunkSize" );
    return bson_iterator_int( &it );
}

MONGO_EXPORT gridfs_offset gridfile_get_contentlength( gridfile *gfile ) {
    bson_iterator it;

    bson_find( &it, gfile->meta, "length" );

    if( bson_iterator_type( &it ) == BSON_INT )
        return ( gridfs_offset )bson_iterator_int( &it );
    else
        return ( gridfs_offset )bson_iterator_long( &it );
}

MONGO_EXPORT const char *gridfile_get_contenttype( gridfile *gfile ) {
    bson_iterator it;

    if ( bson_find( &it, gfile->meta, "contentType" ) )
        return bson_iterator_string( &it );
    else return NULL;
}

MONGO_EXPORT bson_date_t gridfile_get_uploaddate( gridfile *gfile ) {
    bson_iterator it;

    bson_find( &it, gfile->meta, "uploadDate" );
    return bson_iterator_date( &it );
}

MONGO_EXPORT const char *gridfile_get_md5( gridfile *gfile ) {
    bson_iterator it;

    bson_find( &it, gfile->meta, "md5" );
    return bson_iterator_string( &it );
}

const char *gridfile_get_field( gridfile *gfile, const char *name ) {
    bson_iterator it;

    bson_find( &it, gfile->meta, name );
    return bson_iterator_value( &it );
}

bson_bool_t gridfile_get_boolean( gridfile *gfile, const char *name ) {
    bson_iterator it;

    bson_find( &it, gfile->meta, name );
    return bson_iterator_bool( &it );
}

MONGO_EXPORT void gridfile_get_metadata( gridfile *gfile, bson* out ) {
    bson_iterator it;

    if ( bson_find( &it, gfile->meta, "metadata" ) )
        bson_iterator_subobject( &it, out );
    else
        bson_empty( out );
}

MONGO_EXPORT int gridfile_get_numchunks( gridfile *gfile ) {
    bson_iterator it;
    gridfs_offset length;
    gridfs_offset chunkSize;
    double numchunks;

    bson_find( &it, gfile->meta, "length" );

    if( bson_iterator_type( &it ) == BSON_INT )
        length = ( gridfs_offset )bson_iterator_int( &it );
    else
        length = ( gridfs_offset )bson_iterator_long( &it );

    bson_find( &it, gfile->meta, "chunkSize" );
    chunkSize = bson_iterator_int( &it );
    numchunks = ( ( double )length/( double )chunkSize );
    return ( numchunks - ( int )numchunks > 0 )
           ? ( int )( numchunks+1 )
           : ( int )( numchunks );
}

MONGO_EXPORT void gridfile_get_chunk( gridfile *gfile, int n, bson* out ) {
    bson query;
    
    bson_iterator it;
    bson_oid_t id;
    int result;

    bson_init( &query );
    bson_find( &it, gfile->meta, "_id" );
    id = *bson_iterator_oid( &it );
    bson_append_oid( &query, "files_id", &id );
    bson_append_int( &query, "n", n );
    bson_finish( &query );

    result = (mongo_find_one(gfile->gfs->client,
                             gfile->gfs->chunks_ns,
                             &query, NULL, out ) == MONGO_OK );
    bson_destroy( &query );
    if (!result) {
        bson empty;
        bson_empty(&empty);
        bson_copy(out, &empty);
    }
}

MONGO_EXPORT mongo_cursor *gridfile_get_chunks( gridfile *gfile, int start, int size ) {
    bson_iterator it;
    bson_oid_t id;
    bson gte;
    bson query;
    bson orderby;
    bson command;
    mongo_cursor *cursor;

    bson_find( &it, gfile->meta, "_id" );
    id = *bson_iterator_oid( &it );

    bson_init( &query );
    bson_append_oid( &query, "files_id", &id );
    if ( size == 1 ) {
        bson_append_int( &query, "n", start );
    } else {
        bson_init( &gte );
        bson_append_int( &gte, "$gte", start );
        bson_finish( &gte );
        bson_append_bson( &query, "n", &gte );
        bson_destroy( &gte );
    }
    bson_finish( &query );

    bson_init( &orderby );
    bson_append_int( &orderby, "n", 1 );
    bson_finish( &orderby );

    bson_init( &command );
    bson_append_bson( &command, "query", &query );
    bson_append_bson( &command, "orderby", &orderby );
    bson_finish( &command );

    cursor = mongo_find( gfile->gfs->client, gfile->gfs->chunks_ns,
                         &command, NULL, size, 0, 0 );

    bson_destroy( &command );
    bson_destroy( &query );
    bson_destroy( &orderby );

    return cursor;
}

gridfs_offset gridfile_write_file( gridfile *gfile, FILE *stream ) {
    int i;
    size_t len;
    bson chunk;
    bson_iterator it;
    const char *data;
    const int num = gridfile_get_numchunks( gfile );
    size_t nw;

    for ( i=0; i<num; i++ ) {
        gridfile_get_chunk( gfile, i, &chunk );
        bson_find( &it, &chunk, "data" );
        len = bson_iterator_bin_len( &it );
        data = bson_iterator_bin_data( &it );
        nw = fwrite( data , sizeof( char ), len, stream );
        bson_destroy( &chunk );
    }

    return gridfile_get_contentlength( gfile );
}

MONGO_EXPORT gridfs_offset gridfile_read( gridfile *gfile, gridfs_offset size, char *buf ) {
    mongo_cursor *chunks;
    bson chunk;

    int first_chunk;
    int last_chunk;
    int total_chunks;
    gridfs_offset chunksize;
    gridfs_offset contentlength;
    gridfs_offset bytes_left;
    int i;
    bson_iterator it;
    gridfs_offset chunk_len;
    const char *chunk_data;

    contentlength = gridfile_get_contentlength( gfile );
    chunksize = gridfile_get_chunksize( gfile );
    size = ( contentlength - gfile->pos < size )
           ? contentlength - gfile->pos
           : size;
    bytes_left = size;

    first_chunk = ( gfile->pos )/chunksize;
    last_chunk = ( gfile->pos+size-1 )/chunksize;
    total_chunks = last_chunk - first_chunk + 1;
    chunks = gridfile_get_chunks( gfile, first_chunk, total_chunks );

    for ( i = 0; i < total_chunks; i++ ) {
        mongo_cursor_next( chunks );
        chunk = chunks->current;
        bson_find( &it, &chunk, "data" );
        chunk_len = bson_iterator_bin_len( &it );
        chunk_data = bson_iterator_bin_data( &it );
        if ( i == 0 ) {
            chunk_data += ( gfile->pos )%chunksize;
            chunk_len -= ( gfile->pos )%chunksize;
        }
        if ( bytes_left > chunk_len ) {
            memcpy( buf, chunk_data, chunk_len );
            bytes_left -= chunk_len;
            buf += chunk_len;
        } else {
            memcpy( buf, chunk_data, bytes_left );
        }
    }

    mongo_cursor_destroy( chunks );
    gfile->pos = gfile->pos + size;

    return size;
}

MONGO_EXPORT gridfs_offset gridfile_seek( gridfile *gfile, gridfs_offset offset ) {
    gridfs_offset length;

    length = gridfile_get_contentlength( gfile );
    gfile->pos = length < offset ? length : offset;
    return gfile->pos;
}

/*==============================================================*/
/* --- net.h */

#define mongo_close_socket(sock) ( close(sock) )
MONGO_EXPORT int mongo_sock_init(void);

/*==============================================================*/
/* --- net.c */

#ifndef NI_MAXSERV
# define NI_MAXSERV 32
#endif

static int mongo_write_socket( mongo *conn, const void *buf, int len ) {
    const char *cbuf = (const char *) buf;
#if !defined(MSG_NOSIGNAL)
    int flags = 0;
#else
    int flags = MSG_NOSIGNAL;
#endif

    while ( len ) {
        int sent = send( conn->sock, cbuf, len, flags );
        if ( sent == -1 ) {
            if (errno == EPIPE) 
                conn->connected = 0;
            conn->err = MONGO_IO_ERROR;
            return MONGO_ERROR;
        }
        cbuf += sent;
        len -= sent;
    }

    return MONGO_OK;
}

static int mongo_read_socket( mongo *conn, void *buf, int len ) {
    char *cbuf = (char *) buf;
    while ( len ) {
        int sent = recv( conn->sock, cbuf, len, 0 );
        if ( sent == 0 || sent == -1 ) {
            conn->err = MONGO_IO_ERROR;
            return MONGO_ERROR;
        }
        cbuf += sent;
        len -= sent;
    }

    return MONGO_OK;
}

/* This is a no-op in the generic implementation. */
static int mongo_set_socket_op_timeout( mongo *conn, int millis ) {
    return MONGO_OK;
}

#define	_MONGO_USE_GETADDRINFO
#ifdef _MONGO_USE_GETADDRINFO
static int mongo_socket_connect( mongo *conn, const char *host, int port ) {
    char port_str[NI_MAXSERV];
    int status;

    struct addrinfo ai_hints;
    struct addrinfo *ai_list = NULL;
    struct addrinfo *ai_ptr = NULL;

    conn->sock = 0;
    conn->connected = 0;

    bson_sprintf( port_str, "%d", port );

    memset( &ai_hints, 0, sizeof( ai_hints ) );
#ifdef AI_ADDRCONFIG
    ai_hints.ai_flags = AI_ADDRCONFIG;
#endif
    ai_hints.ai_family = AF_UNSPEC;
    ai_hints.ai_socktype = SOCK_STREAM;

    status = getaddrinfo( host, port_str, &ai_hints, &ai_list );
    if ( status != 0 ) {
        bson_errprintf( "getaddrinfo failed: %s", gai_strerror( status ) );
        conn->err = MONGO_CONN_ADDR_FAIL;
        return MONGO_ERROR;
    }

    for ( ai_ptr = ai_list; ai_ptr != NULL; ai_ptr = ai_ptr->ai_next ) {
        conn->sock = socket( ai_ptr->ai_family, ai_ptr->ai_socktype, ai_ptr->ai_protocol );
        if ( conn->sock < 0 ) {
            conn->sock = 0;
            continue;
        }

        status = connect( conn->sock, ai_ptr->ai_addr, ai_ptr->ai_addrlen );
        if ( status != 0 ) {
            mongo_close_socket( conn->sock );
            conn->sock = 0;
            continue;
        }

        if ( ai_ptr->ai_protocol == IPPROTO_TCP ) {
            int flag = 1;

            setsockopt( conn->sock, IPPROTO_TCP, TCP_NODELAY,
                        ( void * ) &flag, sizeof( flag ) );
            if ( conn->op_timeout_ms > 0 )
                mongo_set_socket_op_timeout( conn, conn->op_timeout_ms );
        }

        conn->connected = 1;
        break;
    }

    freeaddrinfo( ai_list );

    if ( ! conn->connected ) {
        conn->err = MONGO_CONN_FAIL;
        return MONGO_ERROR;
    }

    return MONGO_OK;
}
#else
int mongo_socket_connect( mongo *conn, const char *host, int port ) {
    struct sockaddr_in sa;
    socklen_t addressSize;
    int flag = 1;

    if ( ( conn->sock = socket( AF_INET, SOCK_STREAM, 0 ) ) < 0 ) {
        conn->sock = 0;
        conn->err = MONGO_CONN_NO_SOCKET;
        return MONGO_ERROR;
    }

    memset( sa.sin_zero , 0 , sizeof( sa.sin_zero ) );
    sa.sin_family = AF_INET;
    sa.sin_port = htons( port );
    sa.sin_addr.s_addr = inet_addr( host );
    addressSize = sizeof( sa );

    if ( connect( conn->sock, ( struct sockaddr * )&sa, addressSize ) == -1 ) {
        mongo_close_socket( conn->sock );
        conn->connected = 0;
        conn->sock = 0;
        conn->err = MONGO_CONN_FAIL;
        return MONGO_ERROR;
    }

    setsockopt( conn->sock, IPPROTO_TCP, TCP_NODELAY, ( char * ) &flag, sizeof( flag ) );

    if( conn->op_timeout_ms > 0 )
        mongo_set_socket_op_timeout( conn, conn->op_timeout_ms );

    conn->connected = 1;

    return MONGO_OK;
}

#endif

MONGO_EXPORT int mongo_sock_init(void) {

#if defined(SIGPIPE)
    struct sigaction act;
#endif

    static int called_once;
    static int retval;
    if (called_once) return retval;
    called_once = 1;

#if defined(SIGPIPE)
    retval = 1;
    if (sigaction(SIGPIPE, (struct sigaction *)NULL, &act) < 0)
        retval = 0;
    else if (act.sa_handler == SIG_DFL) {
        act.sa_handler = SIG_IGN;
        if (sigaction(SIGPIPE, &act, (struct sigaction *)NULL) < 0)
            retval = 0;
    }
#endif
    return retval;
}

/*==============================================================*/
/* --- mongo.c */

MONGO_EXPORT mongo* mongo_create() {
    return (mongo*)bson_malloc(sizeof(mongo));
}


MONGO_EXPORT void mongo_dispose(mongo* conn) {
    free(conn);
}

MONGO_EXPORT int mongo_get_err(mongo* conn) {
    return conn->err;
}


MONGO_EXPORT int mongo_is_connected(mongo* conn) {
    return conn->connected != 0;
}


MONGO_EXPORT int mongo_get_op_timeout(mongo* conn) {
    return conn->op_timeout_ms;
}


static const char* _get_host_port(mongo_host_port* hp) {
    static char _hp[sizeof(hp->host)+12];
    sprintf(_hp, "%s:%d", hp->host, hp->port);
    return _hp;
}


MONGO_EXPORT const char* mongo_get_primary(mongo* conn) {
    mongo* conn_ = (mongo*)conn;
    return _get_host_port(conn_->primary);
}


MONGO_EXPORT int mongo_get_socket(mongo* conn) {
    mongo* conn_ = (mongo*)conn;
    return conn_->sock;
}


MONGO_EXPORT int mongo_get_host_count(mongo* conn) {
    mongo_replset* r = conn->replset;
    mongo_host_port* hp;
    int count = 0;
    if (!r) return 0;
    for (hp = r->hosts; hp; hp = hp->next)
        ++count;
    return count;
}


MONGO_EXPORT const char* mongo_get_host(mongo* conn, int i) {
    mongo_replset* r = conn->replset;
    mongo_host_port* hp;
    int count = 0;
    if (!r) return 0;
    for (hp = r->hosts; hp; hp = hp->next) {
        if (count == i)
            return _get_host_port(hp);
        ++count;
    }
    return 0;
}


MONGO_EXPORT mongo_cursor* mongo_cursor_create() {
    return (mongo_cursor*)bson_malloc(sizeof(mongo_cursor));
}


MONGO_EXPORT void mongo_cursor_dispose(mongo_cursor* cursor) {
    free(cursor);
}


MONGO_EXPORT int  mongo_get_server_err(mongo* conn) {
    return conn->lasterrcode;
}


MONGO_EXPORT const char*  mongo_get_server_err_string(mongo* conn) {
    return conn->lasterrstr;
}


static const int ZERO = 0;
static const int ONE = 1;
static mongo_message *mongo_message_create( int len , int id , int responseTo , int op ) {
    mongo_message *mm = ( mongo_message * )bson_malloc( len );

    if ( !id )
        id = rand();

    /* native endian (converted on send) */
    mm->head.len = len;
    mm->head.id = id;
    mm->head.responseTo = responseTo;
    mm->head.op = op;

    return mm;
}

/* Always calls bson_free(mm) */
static int mongo_message_send( mongo *conn, mongo_message *mm ) {
    mongo_header head; /* little endian */
    int res;
    bson_little_endian32( &head.len, &mm->head.len );
    bson_little_endian32( &head.id, &mm->head.id );
    bson_little_endian32( &head.responseTo, &mm->head.responseTo );
    bson_little_endian32( &head.op, &mm->head.op );

    res = mongo_write_socket( conn, &head, sizeof( head ) );
    if( res != MONGO_OK ) {
        bson_free( mm );
        return res;
    }

    res = mongo_write_socket( conn, &mm->data, mm->head.len - sizeof( head ) );
    if( res != MONGO_OK ) {
        bson_free( mm );
        return res;
    }

    bson_free( mm );
    return MONGO_OK;
}

static int mongo_read_response( mongo *conn, mongo_reply **reply ) {
    mongo_header head; /* header from network */
    mongo_reply_fields fields; /* header from network */
    mongo_reply *out;  /* native endian */
    unsigned int len;
    int res;

    mongo_read_socket( conn, &head, sizeof( head ) );
    mongo_read_socket( conn, &fields, sizeof( fields ) );

    bson_little_endian32( &len, &head.len );

    if ( len < sizeof( head )+sizeof( fields ) || len > 64*1024*1024 )
        return MONGO_READ_SIZE_ERROR;  /* most likely corruption */

    out = ( mongo_reply * )bson_malloc( len );

    out->head.len = len;
    bson_little_endian32( &out->head.id, &head.id );
    bson_little_endian32( &out->head.responseTo, &head.responseTo );
    bson_little_endian32( &out->head.op, &head.op );

    bson_little_endian32( &out->fields.flag, &fields.flag );
    bson_little_endian64( &out->fields.cursorID, &fields.cursorID );
    bson_little_endian32( &out->fields.start, &fields.start );
    bson_little_endian32( &out->fields.num, &fields.num );

    res = mongo_read_socket( conn, &out->objs, len-sizeof( head )-sizeof( fields ) );
    if( res != MONGO_OK ) {
        bson_free( out );
        return res;
    }

    *reply = out;

    return MONGO_OK;
}


static char *mongo_data_append( char *start , const void *data , int len ) {
    memcpy( start , data , len );
    return start + len;
}

static char *mongo_data_append32( char *start , const void *data ) {
    bson_little_endian32( start , data );
    return start + 4;
}

static char *mongo_data_append64( char *start , const void *data ) {
    bson_little_endian64( start , data );
    return start + 8;
}

/* Connection API */

static int mongo_check_is_master( mongo *conn ) {
    bson out;
    bson_iterator it;
    bson_bool_t ismaster = 0;

    out.data = NULL;

    if ( mongo_simple_int_command( conn, "admin", "ismaster", 1, &out ) == MONGO_OK ) {
        if( bson_find( &it, &out, "ismaster" ) )
            ismaster = bson_iterator_bool( &it );
    } else {
        return MONGO_ERROR;
    }

    bson_destroy( &out );

    if( ismaster )
        return MONGO_OK;
    else {
        conn->err = MONGO_CONN_NOT_MASTER;
        return MONGO_ERROR;
    }
}

void mongo_init( mongo *conn ) {
    memset( conn, 0, sizeof( mongo ) );
}

MONGO_EXPORT int mongo_connect( mongo *conn , const char *host, int port ) {
    mongo_init( conn );

    conn->primary = (mongo_host_port *)
	bson_malloc( sizeof( mongo_host_port ) );
    strncpy( conn->primary->host, host, strlen( host ) + 1 );
    conn->primary->port = port;
    conn->primary->next = NULL;

    if( mongo_socket_connect( conn, host, port ) != MONGO_OK )
        return MONGO_ERROR;

    if( mongo_check_is_master( conn ) != MONGO_OK )
        return MONGO_ERROR;
    else
        return MONGO_OK;
}

MONGO_EXPORT void mongo_replset_init( mongo *conn, const char *name ) {
    mongo_init( conn );

    conn->replset = (mongo_replset *)
	bson_malloc( sizeof( mongo_replset ) );
    conn->replset->primary_connected = 0;
    conn->replset->seeds = NULL;
    conn->replset->hosts = NULL;
    conn->replset->name = ( char * )bson_malloc( strlen( name ) + 1 );
    memcpy( conn->replset->name, name, strlen( name ) + 1  );

    conn->primary = (mongo_host_port *)
	bson_malloc( sizeof( mongo_host_port ) );
}

static void mongo_replset_add_node( mongo_host_port **list, const char *host, int port ) {
    mongo_host_port *host_port = (mongo_host_port *)
		bson_malloc( sizeof( mongo_host_port ) );
    host_port->port = port;
    host_port->next = NULL;
    strncpy( host_port->host, host, strlen( host ) + 1 );

    if( *list == NULL )
        *list = host_port;
    else {
        mongo_host_port *p = *list;
        while( p->next != NULL )
            p = p->next;
        p->next = host_port;
    }
}

static void mongo_replset_free_list( mongo_host_port **list ) {
    mongo_host_port *node = *list;
    mongo_host_port *prev;

    while( node != NULL ) {
        prev = node;
        node = node->next;
        bson_free( prev );
    }

    *list = NULL;
}

MONGO_EXPORT void mongo_replset_add_seed( mongo *conn, const char *host, int port ) {
    mongo_replset_add_node( &conn->replset->seeds, host, port );
}

void mongo_parse_host( const char *host_string, mongo_host_port *host_port ) {
    int len, idx, split;
    len = split = idx = 0;

    /* Split the host_port string at the ':' */
    while( 1 ) {
        if( *( host_string + len ) == '\0' )
            break;
        if( *( host_string + len ) == ':' )
            split = len;

        len++;
    }

    /* If 'split' is set, we know the that port exists;
     * Otherwise, we set the default port. */
    idx = split ? split : len;
    memcpy( host_port->host, host_string, idx );
    memcpy( host_port->host + idx, "\0", 1 );
    if( split )
        host_port->port = atoi( host_string + idx + 1 );
    else
        host_port->port = MONGO_DEFAULT_PORT;
}

static void mongo_replset_check_seed( mongo *conn ) {
    bson out;
    bson hosts;
    const char *data;
    bson_iterator it;
    bson_iterator it_sub;
    const char *host_string;
    mongo_host_port *host_port = NULL;

    out.data = NULL;

    hosts.data = NULL;

    if( mongo_simple_int_command( conn, "admin", "ismaster", 1, &out ) == MONGO_OK ) {

        if( bson_find( &it, &out, "hosts" ) ) {
            data = bson_iterator_value( &it );
            bson_iterator_from_buffer( &it_sub, data );

            /* Iterate over host list, adding each host to the
             * connection's host list. */
            while( bson_iterator_next( &it_sub ) ) {
                host_string = bson_iterator_string( &it_sub );

                host_port = (mongo_host_port *)
			bson_malloc( sizeof( mongo_host_port ) );
                mongo_parse_host( host_string, host_port );

                if( host_port ) {
                    mongo_replset_add_node( &conn->replset->hosts,
                                            host_port->host, host_port->port );

                    bson_free( host_port );
                    host_port = NULL;
                }
            }
        }
    }

    bson_destroy( &out );
    bson_destroy( &hosts );
    mongo_close_socket( conn->sock );
    conn->sock = 0;
    conn->connected = 0;

}

/* Find out whether the current connected node is master, and
 * verify that the node's replica set name matched the provided name
 */
static int mongo_replset_check_host( mongo *conn ) {

    bson out;
    bson_iterator it;
    bson_bool_t ismaster = 0;
    const char *set_name;

    out.data = NULL;

    if ( mongo_simple_int_command( conn, "admin", "ismaster", 1, &out ) == MONGO_OK ) {
        if( bson_find( &it, &out, "ismaster" ) )
            ismaster = bson_iterator_bool( &it );

        if( bson_find( &it, &out, "setName" ) ) {
            set_name = bson_iterator_string( &it );
            if( strcmp( set_name, conn->replset->name ) != 0 ) {
                bson_destroy( &out );
                conn->err = MONGO_CONN_BAD_SET_NAME;
                return MONGO_ERROR;
            }
        }
    }

    bson_destroy( &out );

    if( ismaster ) {
        conn->replset->primary_connected = 1;
    } else {
        mongo_close_socket( conn->sock );
    }

    return MONGO_OK;
}

MONGO_EXPORT int mongo_replset_connect( mongo *conn ) {

    int res = 0;
    mongo_host_port *node;

    conn->sock = 0;
    conn->connected = 0;

    /* First iterate over the seed nodes to get the canonical list of hosts
     * from the replica set. Break out once we have a host list.
     */
    node = conn->replset->seeds;
    while( node != NULL ) {
        res = mongo_socket_connect( conn, ( const char * )&node->host, node->port );
        if( res == MONGO_OK ) {
            mongo_replset_check_seed( conn );
            if( conn->replset->hosts )
                break;
        }
        node = node->next;
    }

    /* Iterate over the host list, checking for the primary node. */
    if( !conn->replset->hosts ) {
        conn->err = MONGO_CONN_NO_PRIMARY;
        return MONGO_ERROR;
    } else {
        node = conn->replset->hosts;

        while( node != NULL ) {
            res = mongo_socket_connect( conn, ( const char * )&node->host, node->port );

            if( res == MONGO_OK ) {
                if( mongo_replset_check_host( conn ) != MONGO_OK )
                    return MONGO_ERROR;

                /* Primary found, so return. */
                else if( conn->replset->primary_connected ) {
                    strncpy( conn->primary->host, node->host, strlen( node->host ) + 1 );
                    conn->primary->port = node->port;
                    return MONGO_OK;
                }

                /* No primary, so close the connection. */
                else {
                    mongo_close_socket( conn->sock );
                    conn->sock = 0;
                    conn->connected = 0;
                }
            }

            node = node->next;
        }
    }


    conn->err = MONGO_CONN_NO_PRIMARY;
    return MONGO_ERROR;
}

MONGO_EXPORT int mongo_set_op_timeout( mongo *conn, int millis ) {
    conn->op_timeout_ms = millis;
    if( conn->sock && conn->connected )
        mongo_set_socket_op_timeout( conn, millis );

    return MONGO_OK;
}

MONGO_EXPORT int mongo_reconnect( mongo *conn ) {
    int res;
    mongo_disconnect( conn );

    if( conn->replset ) {
        conn->replset->primary_connected = 0;
        mongo_replset_free_list( &conn->replset->hosts );
        conn->replset->hosts = NULL;
        res = mongo_replset_connect( conn );
        return res;
    } else
        return mongo_socket_connect( conn, conn->primary->host, conn->primary->port );
}

MONGO_EXPORT int mongo_check_connection( mongo *conn ) {
    if( ! conn->connected )
        return MONGO_ERROR;

    if( mongo_simple_int_command( conn, "admin", "ping", 1, NULL ) == MONGO_OK )
        return MONGO_OK;
    else
        return MONGO_ERROR;
}

MONGO_EXPORT void mongo_disconnect( mongo *conn ) {
    if( ! conn->connected )
        return;

    if( conn->replset ) {
        conn->replset->primary_connected = 0;
        mongo_replset_free_list( &conn->replset->hosts );
        conn->replset->hosts = NULL;
    }

    mongo_close_socket( conn->sock );

    conn->sock = 0;
    conn->connected = 0;
}

MONGO_EXPORT void mongo_destroy( mongo *conn ) {
    mongo_disconnect( conn );

    if( conn->replset ) {
        mongo_replset_free_list( &conn->replset->seeds );
        mongo_replset_free_list( &conn->replset->hosts );
        bson_free( conn->replset->name );
        bson_free( conn->replset );
        conn->replset = NULL;
    }

    bson_free( conn->primary );
    bson_free( conn->errstr );
    bson_free( conn->lasterrstr );

    conn->err = (mongo_error_t)0;
    conn->errstr = NULL;
    conn->lasterrcode = 0;
    conn->lasterrstr = NULL;
}

/* Determine whether this BSON object is valid for the given operation.  */
static int mongo_bson_valid( mongo *conn, bson *bson, int write ) {
    if( ! bson->finished ) {
        conn->err = MONGO_BSON_NOT_FINISHED;
        return MONGO_ERROR;
    }

    if( bson->err & BSON_NOT_UTF8 ) {
        conn->err = MONGO_BSON_INVALID;
        return MONGO_ERROR;
    }

    if( write ) {
        if( ( bson->err & BSON_FIELD_HAS_DOT ) ||
                ( bson->err & BSON_FIELD_INIT_DOLLAR ) ) {

            conn->err = MONGO_BSON_INVALID;
            return MONGO_ERROR;

        }
    }

    conn->err = (mongo_error_t)0;
    conn->errstr = NULL;

    return MONGO_OK;
}

/* Determine whether this BSON object is valid for the given operation.  */
static int mongo_cursor_bson_valid( mongo_cursor *cursor, bson *bson ) {
    if( ! bson->finished ) {
        cursor->err = MONGO_CURSOR_BSON_ERROR;
        cursor->conn->err = MONGO_BSON_NOT_FINISHED;
        return MONGO_ERROR;
    }

    if( bson->err & BSON_NOT_UTF8 ) {
        cursor->err = MONGO_CURSOR_BSON_ERROR;
        cursor->conn->err = MONGO_BSON_INVALID;
        return MONGO_ERROR;
    }

    return MONGO_OK;
}

/* MongoDB CRUD API */

MONGO_EXPORT int mongo_insert_batch( mongo *conn, const char *ns,
                        bson **bsons, int count ) {

    int size =  16 + 4 + strlen( ns ) + 1;
    int i;
    mongo_message *mm;
    char *data;

    for( i=0; i<count; i++ ) {
        size += bson_size( bsons[i] );
        if( mongo_bson_valid( conn, bsons[i], 1 ) != MONGO_OK )
            return MONGO_ERROR;
    }

    mm = mongo_message_create( size , 0 , 0 , MONGO_OP_INSERT );

    data = &mm->data;
    data = mongo_data_append32( data, &ZERO );
    data = mongo_data_append( data, ns, strlen( ns ) + 1 );

    for( i=0; i<count; i++ ) {
        data = mongo_data_append( data, bsons[i]->data, bson_size( bsons[i] ) );
    }

    return mongo_message_send( conn, mm );
}

MONGO_EXPORT int mongo_insert( mongo *conn , const char *ns , bson *bson ) {

    char *data;
    mongo_message *mm;

    /* Make sure that BSON is valid for insert. */
    if( mongo_bson_valid( conn, bson, 1 ) != MONGO_OK ) {
        return MONGO_ERROR;
    }

    mm = mongo_message_create( 16 /* header */
                               + 4 /* ZERO */
                               + strlen( ns )
                               + 1 + bson_size( bson )
                               , 0, 0, MONGO_OP_INSERT );

    data = &mm->data;
    data = mongo_data_append32( data, &ZERO );
    data = mongo_data_append( data, ns, strlen( ns ) + 1 );
    data = mongo_data_append( data, bson->data, bson_size( bson ) );

    return mongo_message_send( conn, mm );
}

MONGO_EXPORT int mongo_update( mongo *conn, const char *ns, const bson *cond,
                  const bson *op, int flags ) {

    char *data;
    mongo_message *mm;

    /* Make sure that the op BSON is valid UTF-8.
     * TODO: decide whether to check cond as well.
     * */
    if( mongo_bson_valid( conn, ( bson * )op, 0 ) != MONGO_OK ) {
        return MONGO_ERROR;
    }

    mm = mongo_message_create( 16 /* header */
                               + 4  /* ZERO */
                               + strlen( ns ) + 1
                               + 4  /* flags */
                               + bson_size( cond )
                               + bson_size( op )
                               , 0 , 0 , MONGO_OP_UPDATE );

    data = &mm->data;
    data = mongo_data_append32( data, &ZERO );
    data = mongo_data_append( data, ns, strlen( ns ) + 1 );
    data = mongo_data_append32( data, &flags );
    data = mongo_data_append( data, cond->data, bson_size( cond ) );
    data = mongo_data_append( data, op->data, bson_size( op ) );

    return mongo_message_send( conn, mm );
}

MONGO_EXPORT int mongo_remove( mongo *conn, const char *ns, const bson *cond ) {
    char *data;
    mongo_message *mm;

    /* Make sure that the BSON is valid UTF-8.
     * TODO: decide whether to check cond as well.
     * */
    if( mongo_bson_valid( conn, ( bson * )cond, 0 ) != MONGO_OK ) {
        return MONGO_ERROR;
    }

    mm = mongo_message_create( 16  /* header */
                              + 4  /* ZERO */
                              + strlen( ns ) + 1
                              + 4  /* ZERO */
                              + bson_size( cond )
                              , 0 , 0 , MONGO_OP_DELETE );

    data = &mm->data;
    data = mongo_data_append32( data, &ZERO );
    data = mongo_data_append( data, ns, strlen( ns ) + 1 );
    data = mongo_data_append32( data, &ZERO );
    data = mongo_data_append( data, cond->data, bson_size( cond ) );

    return mongo_message_send( conn, mm );
}


static int mongo_cursor_op_query( mongo_cursor *cursor ) {
    int res;
    bson empty;
    char *data;
    mongo_message *mm;
    bson temp;
    bson_iterator it;

    /* Clear any errors. */
    bson_free( cursor->conn->lasterrstr );
    cursor->conn->lasterrstr = NULL;
    cursor->conn->lasterrcode = 0;
    cursor->conn->err = (mongo_error_t)0;
    cursor->err = (mongo_cursor_error_t)0;

    /* Set up default values for query and fields, if necessary. */
    if( ! cursor->query )
        cursor->query = bson_empty( &empty );
    else if( mongo_cursor_bson_valid( cursor, cursor->query ) != MONGO_OK )
        return MONGO_ERROR;

    if( ! cursor->fields )
        cursor->fields = bson_empty( &empty );
    else if( mongo_cursor_bson_valid( cursor, cursor->fields ) != MONGO_OK )
        return MONGO_ERROR;

    mm = mongo_message_create( 16 + /* header */
                               4 + /*  options */
                               strlen( cursor->ns ) + 1 + /* ns */
                               4 + 4 + /* skip,return */
                               bson_size( cursor->query ) +
                               bson_size( cursor->fields ) ,
                               0 , 0 , MONGO_OP_QUERY );

    data = &mm->data;
    data = mongo_data_append32( data , &cursor->options );
    data = mongo_data_append( data , cursor->ns , strlen( cursor->ns ) + 1 );
    data = mongo_data_append32( data , &cursor->skip );
    data = mongo_data_append32( data , &cursor->limit );
    data = mongo_data_append( data , cursor->query->data , bson_size( cursor->query ) );
    if ( cursor->fields )
        data = mongo_data_append( data , cursor->fields->data , bson_size( cursor->fields ) );

    bson_fatal_msg( ( data == ( ( char * )mm ) + mm->head.len ), "query building fail!" );

    res = mongo_message_send( cursor->conn , mm );
    if( res != MONGO_OK ) {
        return MONGO_ERROR;
    }

    res = mongo_read_response( cursor->conn, ( mongo_reply ** )&( cursor->reply ) );
    if( res != MONGO_OK ) {
        return MONGO_ERROR;
    }

    if( cursor->reply->fields.num == 1 ) {
        bson_init_data( &temp, &cursor->reply->objs );
        if( bson_find( &it, &temp, "$err" ) ) {
            cursor->conn->lasterrstr =
              (char *)bson_malloc( bson_iterator_string_len( &it ) );
            strcpy( cursor->conn->lasterrstr, bson_iterator_string( &it ) );
            bson_find( &it, &temp, "code" );
            cursor->conn->lasterrcode = bson_iterator_int( &it );
            cursor->err = MONGO_CURSOR_QUERY_FAIL;
            return MONGO_ERROR;
        }
    }

    cursor->seen += cursor->reply->fields.num;
    cursor->flags |= MONGO_CURSOR_QUERY_SENT;
    return MONGO_OK;
}

static int mongo_cursor_get_more( mongo_cursor *cursor ) {
    int res;

    if( cursor->limit > 0 && cursor->seen >= cursor->limit ) {
        cursor->err = MONGO_CURSOR_EXHAUSTED;
        return MONGO_ERROR;
    } else if( ! cursor->reply ) {
        cursor->err = MONGO_CURSOR_INVALID;
        return MONGO_ERROR;
    } else if( ! cursor->reply->fields.cursorID ) {
        cursor->err = MONGO_CURSOR_EXHAUSTED;
        return MONGO_ERROR;
    } else {
        char *data;
        int sl = strlen( cursor->ns )+1;
        int limit = 0;
        mongo_message *mm;

        if( cursor->limit > 0 )
            limit = cursor->limit - cursor->seen;

        mm = mongo_message_create( 16 /*header*/
                                   +4 /*ZERO*/
                                   +sl
                                   +4 /*numToReturn*/
                                   +8 /*cursorID*/
                                   , 0, 0, MONGO_OP_GET_MORE );
        data = &mm->data;
        data = mongo_data_append32( data, &ZERO );
        data = mongo_data_append( data, cursor->ns, sl );
        data = mongo_data_append32( data, &limit );
        data = mongo_data_append64( data, &cursor->reply->fields.cursorID );

        bson_free( cursor->reply );
        res = mongo_message_send( cursor->conn, mm );
        if( res != MONGO_OK ) {
            mongo_cursor_destroy( cursor );
            return MONGO_ERROR;
        }

        res = mongo_read_response( cursor->conn, &( cursor->reply ) );
        if( res != MONGO_OK ) {
            mongo_cursor_destroy( cursor );
            return MONGO_ERROR;
        }
        cursor->current.data = NULL;
        cursor->seen += cursor->reply->fields.num;

        return MONGO_OK;
    }
}

MONGO_EXPORT mongo_cursor *mongo_find( mongo *conn, const char *ns, bson *query,
                          bson *fields, int limit, int skip, int options ) {

    mongo_cursor *cursor = ( mongo_cursor * )bson_malloc( sizeof( mongo_cursor ) );
    mongo_cursor_init( cursor, conn, ns );
    cursor->flags |= MONGO_CURSOR_MUST_FREE;

    mongo_cursor_set_query( cursor, query );
    mongo_cursor_set_fields( cursor, fields );
    mongo_cursor_set_limit( cursor, limit );
    mongo_cursor_set_skip( cursor, skip );
    mongo_cursor_set_options( cursor, options );

    if( mongo_cursor_op_query( cursor ) == MONGO_OK )
        return cursor;
    else {
        mongo_cursor_destroy( cursor );
        return NULL;
    }
}

MONGO_EXPORT int mongo_find_one( mongo *conn, const char *ns, bson *query,
                    bson *fields, bson *out ) {

    mongo_cursor cursor[1];
    mongo_cursor_init( cursor, conn, ns );
    mongo_cursor_set_query( cursor, query );
    mongo_cursor_set_fields( cursor, fields );
    mongo_cursor_set_limit( cursor, 1 );

    if ( mongo_cursor_next( cursor ) == MONGO_OK ) {
        bson_init_size( out, bson_size( (bson *)&cursor->current ) );
        memcpy( out->data, cursor->current.data,
            bson_size( (bson *)&cursor->current ) );
        out->finished = 1;
        mongo_cursor_destroy( cursor );
        return MONGO_OK;
    } else {
        mongo_cursor_destroy( cursor );
        return MONGO_ERROR;
    }
}

void mongo_cursor_init( mongo_cursor *cursor, mongo *conn, const char *ns ) {
    memset( cursor, 0, sizeof( mongo_cursor ) );
    cursor->conn = conn;
    cursor->ns = ( const char * )bson_malloc( strlen( ns ) + 1 );
    strncpy( ( char * )cursor->ns, ns, strlen( ns ) + 1 );
    cursor->current.data = NULL;
}

void mongo_cursor_set_query( mongo_cursor *cursor, bson *query ) {
    cursor->query = query;
}

void mongo_cursor_set_fields( mongo_cursor *cursor, bson *fields ) {
    cursor->fields = fields;
}

void mongo_cursor_set_skip( mongo_cursor *cursor, int skip ) {
    cursor->skip = skip;
}

void mongo_cursor_set_limit( mongo_cursor *cursor, int limit ) {
    cursor->limit = limit;
}

void mongo_cursor_set_options( mongo_cursor *cursor, int options ) {
    cursor->options = options;
}

const char *mongo_cursor_data( mongo_cursor *cursor ) {
    return cursor->current.data;
}

MONGO_EXPORT const bson *mongo_cursor_bson( mongo_cursor *cursor ) {
    return (const bson *)&(cursor->current);
}

MONGO_EXPORT int mongo_cursor_next( mongo_cursor *cursor ) {
    char *next_object;
    char *message_end;

    if( ! ( cursor->flags & MONGO_CURSOR_QUERY_SENT ) )
        if( mongo_cursor_op_query( cursor ) != MONGO_OK )
            return MONGO_ERROR;

    if( !cursor->reply )
        return MONGO_ERROR;

    /* no data */
    if ( cursor->reply->fields.num == 0 ) {

        /* Special case for tailable cursors. */
        if( cursor->reply->fields.cursorID ) {
            if( ( mongo_cursor_get_more( cursor ) != MONGO_OK ) ||
                    cursor->reply->fields.num == 0 ) {
                return MONGO_ERROR;
            }
        }

        else
            return MONGO_ERROR;
    }

    /* first */
    if ( cursor->current.data == NULL ) {
        bson_init_finished_data( &cursor->current, &cursor->reply->objs );
        return MONGO_OK;
    }

    next_object = cursor->current.data + bson_size( &cursor->current );
    message_end = ( char * )cursor->reply + cursor->reply->head.len;

    if ( next_object >= message_end ) {
        if( mongo_cursor_get_more( cursor ) != MONGO_OK )
            return MONGO_ERROR;

        /* If there's still a cursor id, then the message should be pending. */
        if( cursor->reply->fields.num == 0 && cursor->reply->fields.cursorID ) {
            cursor->err = MONGO_CURSOR_PENDING;
            return MONGO_ERROR;
        }

        bson_init_finished_data( &cursor->current, &cursor->reply->objs );
    } else {
        bson_init_finished_data( &cursor->current, next_object );
    }

    return MONGO_OK;
}

MONGO_EXPORT int mongo_cursor_destroy( mongo_cursor *cursor ) {
    int result = MONGO_OK;

    if ( !cursor ) return result;

    /* Kill cursor if live. */
    if ( cursor->reply && cursor->reply->fields.cursorID ) {
        mongo *conn = cursor->conn;
        mongo_message *mm = mongo_message_create( 16 /*header*/
                            +4 /*ZERO*/
                            +4 /*numCursors*/
                            +8 /*cursorID*/
                            , 0, 0, MONGO_OP_KILL_CURSORS );
        char *data = &mm->data;
        data = mongo_data_append32( data, &ZERO );
        data = mongo_data_append32( data, &ONE );
        data = mongo_data_append64( data, &cursor->reply->fields.cursorID );

        result = mongo_message_send( conn, mm );
    }

    bson_free( cursor->reply );
    bson_free( ( void * )cursor->ns );

    if( cursor->flags & MONGO_CURSOR_MUST_FREE )
        bson_free( cursor );

    return result;
}

/* MongoDB Helper Functions */

MONGO_EXPORT int mongo_create_index( mongo *conn, const char *ns, bson *key, int options, bson *out ) {
    bson b;
    bson_iterator it;
    char name[255] = {'_'};
    int i = 1;
    char idxns[1024];

    bson_iterator_init( &it, key );
    while( i < 255 && bson_iterator_next( &it ) ) {
        strncpy( name + i, bson_iterator_key( &it ), 255 - i );
        i += strlen( bson_iterator_key( &it ) );
    }
    name[254] = '\0';

    bson_init( &b );
    bson_append_bson( &b, "key", key );
    bson_append_string( &b, "ns", ns );
    bson_append_string( &b, "name", name );
    if ( options & MONGO_INDEX_UNIQUE )
        bson_append_bool( &b, "unique", 1 );
    if ( options & MONGO_INDEX_DROP_DUPS )
        bson_append_bool( &b, "dropDups", 1 );
    if ( options & MONGO_INDEX_BACKGROUND )
        bson_append_bool( &b, "background", 1 );
    if ( options & MONGO_INDEX_SPARSE )
        bson_append_bool( &b, "sparse", 1 );
    bson_finish( &b );

    strncpy( idxns, ns, 1024-16 );
    strcpy( strchr( idxns, '.' ), ".system.indexes" );
    mongo_insert( conn, idxns, &b );
    bson_destroy( &b );

    *strchr( idxns, '.' ) = '\0'; /* just db not ns */
    return mongo_cmd_get_last_error( conn, idxns, out );
}

bson_bool_t mongo_create_simple_index( mongo *conn, const char *ns, const char *field, int options, bson *out ) {
    bson b;
    bson_bool_t success;

    bson_init( &b );
    bson_append_int( &b, field, 1 );
    bson_finish( &b );

    success = mongo_create_index( conn, ns, &b, options, out );
    bson_destroy( &b );
    return success;
}

MONGO_EXPORT double mongo_count( mongo *conn, const char *db, const char *ns, bson *query ) {
    bson cmd;
    bson out = {NULL, 0};
    double count = -1;

    bson_init( &cmd );
    bson_append_string( &cmd, "count", ns );
    if ( query && bson_size( query ) > 5 ) /* not empty */
        bson_append_bson( &cmd, "query", query );
    bson_finish( &cmd );

    if( mongo_run_command( conn, db, &cmd, &out ) == MONGO_OK ) {
        bson_iterator it;
        if( bson_find( &it, &out, "n" ) )
            count = bson_iterator_double( &it );
        bson_destroy( &cmd );
        bson_destroy( &out );
        return count;
    } else {
        bson_destroy( &out );
        bson_destroy( &cmd );
        return MONGO_ERROR;
    }
}

MONGO_EXPORT int mongo_run_command( mongo *conn, const char *db, bson *command,
                       bson *out ) {

    bson response = {NULL, 0};
    bson fields;
    int sl = strlen( db );
    char *ns = (char *)
	bson_malloc( sl + 5 + 1 ); /* ".$cmd" + nul */
    int res, success = 0;

    strcpy( ns, db );
    strcpy( ns+sl, ".$cmd" );

    res = mongo_find_one( conn, ns, command, bson_empty( &fields ), &response );
    bson_free( ns );

    if( res != MONGO_OK )
        return MONGO_ERROR;
    else {
        bson_iterator it;
        if( bson_find( &it, &response, "ok" ) )
            success = bson_iterator_bool( &it );

        if( !success ) {
            conn->err = MONGO_COMMAND_FAILED;
            return MONGO_ERROR;
        } else {
            if( out )
              *out = response;
            return MONGO_OK;
        }
    }
}

int mongo_simple_int_command( mongo *conn, const char *db,
                              const char *cmdstr, int arg, bson *realout ) {

    bson out = {NULL, 0};
    bson cmd;
    int result;

    bson_init( &cmd );
    bson_append_int( &cmd, cmdstr, arg );
    bson_finish( &cmd );

    result = mongo_run_command( conn, db, &cmd, &out );

    bson_destroy( &cmd );

    if ( realout )
        *realout = out;
    else
        bson_destroy( &out );

    return result;
}

int mongo_simple_str_command( mongo *conn, const char *db,
                              const char *cmdstr, const char *arg, bson *realout ) {

    bson out = {NULL, 0};
    int result;

    bson cmd;
    bson_init( &cmd );
    bson_append_string( &cmd, cmdstr, arg );
    bson_finish( &cmd );

    result = mongo_run_command( conn, db, &cmd, &out );

    bson_destroy( &cmd );

    if ( realout )
        *realout = out;
    else
        bson_destroy( &out );

    return result;
}

MONGO_EXPORT int mongo_cmd_drop_db( mongo *conn, const char *db ) {
    return mongo_simple_int_command( conn, db, "dropDatabase", 1, NULL );
}

MONGO_EXPORT int mongo_cmd_drop_collection( mongo *conn, const char *db, const char *collection, bson *out ) {
    return mongo_simple_str_command( conn, db, "drop", collection, out );
}

void mongo_cmd_reset_error( mongo *conn, const char *db ) {
    mongo_simple_int_command( conn, db, "reseterror", 1, NULL );
}

static int mongo_cmd_get_error_helper( mongo *conn, const char *db,
                                       bson *realout, const char *cmdtype ) {

    bson out = {NULL,0};
    bson_bool_t haserror = 0;

    /* Reset last error codes. */
    conn->lasterrcode = 0;
    bson_free( conn->lasterrstr );
    conn->lasterrstr = NULL;

    /* If there's an error, store its code and string in the connection object. */
    if( mongo_simple_int_command( conn, db, cmdtype, 1, &out ) == MONGO_OK ) {
        bson_iterator it;
        haserror = ( bson_find( &it, &out, "err" ) != BSON_NULL );
        if( haserror ) {
            conn->lasterrstr = ( char * )bson_malloc( bson_iterator_string_len( &it ) );
            if( conn->lasterrstr ) {
                strcpy( conn->lasterrstr, bson_iterator_string( &it ) );
            }

            if( bson_find( &it, &out, "code" ) != BSON_NULL )
                conn->lasterrcode = bson_iterator_int( &it );
        }
    }

    if( realout )
        *realout = out; /* transfer of ownership */
    else
        bson_destroy( &out );

    if( haserror )
        return MONGO_ERROR;
    else
        return MONGO_OK;
}

MONGO_EXPORT int mongo_cmd_get_prev_error( mongo *conn, const char *db, bson *out ) {
    return mongo_cmd_get_error_helper( conn, db, out, "getpreverror" );
}

MONGO_EXPORT int mongo_cmd_get_last_error( mongo *conn, const char *db, bson *out ) {
    return mongo_cmd_get_error_helper( conn, db, out, "getlasterror" );
}

MONGO_EXPORT bson_bool_t mongo_cmd_ismaster( mongo *conn, bson *realout ) {
    bson out = {NULL,0};
    bson_bool_t ismaster = 0;

    if ( mongo_simple_int_command( conn, "admin", "ismaster", 1, &out ) == MONGO_OK ) {
        bson_iterator it;
        bson_find( &it, &out, "ismaster" );
        ismaster = bson_iterator_bool( &it );
    }

    if( realout )
        *realout = out; /* transfer of ownership */
    else
        bson_destroy( &out );

    return ismaster;
}

static void mongo_pass_digest( const char *user, const char *pass, char hex_digest[33] ) {
    DIGEST_CTX ctx = rpmDigestInit(PGPHASHALGO_MD5, RPMDIGEST_NONE);
    const char * _digest = NULL;
    int xx;
    xx = rpmDigestUpdate(ctx, user, strlen(user));
    xx = rpmDigestUpdate(ctx, ":mongo:", 7);
    xx = rpmDigestUpdate(ctx, pass, strlen(pass));
    xx = rpmDigestFinal(ctx, &_digest, NULL, 1);
    strncpy(hex_digest, _digest, 32+1);
    _digest = _free(_digest);
}

MONGO_EXPORT int mongo_cmd_add_user( mongo *conn, const char *db, const char *user, const char *pass ) {
    bson user_obj;
    bson pass_obj;
    char hex_digest[33];
    char *ns = (char *)
	bson_malloc( strlen( db ) + strlen( ".system.users" ) + 1 );
    int res;

    strcpy( ns, db );
    strcpy( ns+strlen( db ), ".system.users" );

    mongo_pass_digest( user, pass, hex_digest );

    bson_init( &user_obj );
    bson_append_string( &user_obj, "user", user );
    bson_finish( &user_obj );

    bson_init( &pass_obj );
    bson_append_start_object( &pass_obj, "$set" );
    bson_append_string( &pass_obj, "pwd", hex_digest );
    bson_append_finish_object( &pass_obj );
    bson_finish( &pass_obj );

    res = mongo_update( conn, ns, &user_obj, &pass_obj, MONGO_UPDATE_UPSERT );

    bson_free( ns );
    bson_destroy( &user_obj );
    bson_destroy( &pass_obj );

    return res;
}

MONGO_EXPORT bson_bool_t mongo_cmd_authenticate( mongo *conn, const char *db, const char *user, const char *pass ) {
    bson from_db;
    bson cmd;
    bson out;
    const char *nonce;
    int result;

    char hex_digest[32+1];

    if( mongo_simple_int_command( conn, db, "getnonce", 1, &from_db ) == MONGO_OK ) {
        bson_iterator it;
        bson_find( &it, &from_db, "nonce" );
        nonce = bson_iterator_string( &it );
    } else {
        return MONGO_ERROR;
    }

    mongo_pass_digest( user, pass, hex_digest );

    {   DIGEST_CTX ctx = rpmDigestInit(PGPHASHALGO_MD5, RPMDIGEST_NONE);
	const char * _digest = NULL;
	int xx;
	xx = rpmDigestUpdate(ctx, nonce, strlen(nonce));
	xx = rpmDigestUpdate(ctx, user, strlen(user));
	xx = rpmDigestUpdate(ctx, hex_digest, 32);
	xx = rpmDigestFinal(ctx, &_digest, NULL, 1);
	strncpy(hex_digest, _digest, 32+1);
	_digest = _free(_digest);
    }

    bson_init( &cmd );
    bson_append_int( &cmd, "authenticate", 1 );
    bson_append_string( &cmd, "user", user );
    bson_append_string( &cmd, "nonce", nonce );
    bson_append_string( &cmd, "key", hex_digest );
    bson_finish( &cmd );

    bson_destroy( &from_db );

    result = mongo_run_command( conn, db, &cmd, &out );

    bson_destroy( &from_db );
    bson_destroy( &cmd );

    return result;
}

/*==============================================================*/

static void rpmmgoFini(void * _mgo)
	/*@globals fileSystem @*/
	/*@modifies *_mgo, fileSystem @*/
{
    rpmmgo mgo = (rpmmgo) _mgo;

    mgo->fn = _free(mgo->fn);
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmmgoPool = NULL;

static rpmmgo rpmmgoGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmmgoPool, fileSystem @*/
	/*@modifies pool, _rpmmgoPool, fileSystem @*/
{
    rpmmgo mgo;

    if (_rpmmgoPool == NULL) {
	_rpmmgoPool = rpmioNewPool("mgo", sizeof(*mgo), -1, _rpmmgo_debug,
			NULL, NULL, rpmmgoFini);
	pool = _rpmmgoPool;
    }
    mgo = (rpmmgo) rpmioGetPool(pool, sizeof(*mgo));
    memset(((char *)mgo)+sizeof(mgo->_item), 0, sizeof(*mgo)-sizeof(mgo->_item));
    return mgo;
}

rpmmgo rpmmgoNew(const char * fn, int flags)
{
    rpmmgo mgo = rpmmgoGetPool(_rpmmgoPool);

    if (fn)
	mgo->fn = xstrdup(fn);

    return rpmmgoLink(mgo);
}

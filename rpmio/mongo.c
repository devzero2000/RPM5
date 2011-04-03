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

/* A GridFS contains a db connection, a root database name, and an
   optional prefix */
typedef struct {
  /* The client to db-connection. */
  mongo_connection* client;
  /* The root database name */
  const char* dbname;
  /* The prefix of the GridFS's collections, default is NULL */
  const char* prefix;
  /* The namespace where the file's metadata is stored */
  const char* files_ns;
  /* The namespace where the files's data is stored in chunks */
  const char* chunks_ns;

} gridfs;

/* The state of a gridfile. This is used for incrementally writing buffers
 * to a single GridFS file.
 */

/* A GridFile contains the GridFS it is located in and the file
   metadata */
typedef struct {
  /* The GridFS where the GridFile is located */
  gridfs* gfs;
  /* The GridFile's bson object where all its metadata is located */
  bson* meta;
  /* The position is the offset in the file */
  gridfs_offset pos;
  /* The files_id of the gridfile */
  bson_oid_t id;
  /* The name of the gridfile as a string */
  const char* remote_name;
  /* The gridfile's content type */
  const char* content_type;
  /* The length of this gridfile */
  gridfs_offset length;
  /* The number of the current chunk being written to */
  int chunk_num;
  /* A buffer storing data still to be written to chunks */
  char* pending_data;
  /* Length of pending data */
  int pending_len;

} gridfile;

/*--------------------------------------------------------------------*/

/** Initializes a GridFS object
 *  @param client - db connection
 *  @param dbname - database name
 *  @param prefix - collection prefix, default is fs if NULL or empty
 *  @param gfs - the GridFS object to initialize
 *  @return - 1 if successful, 0 otherwise
 */
int gridfs_init(mongo_connection* client, const char* dbname,
  const char* prefix, gridfs* gfs);

/** Destroys a GridFS object
 */
void gridfs_destroy( gridfs* gfs );

/** Initializes a gridfile for writing incrementally with gridfs_write_buffer.
 *  Once initialized, you can write any number of buffers with gridfs_write_buffer.
 *  When done, you must call gridfs_writer_done to save the file metadata.
 *
 *  @return - 1 if successful, 0 otherwise
 */
void gridfile_writer_init( gridfile* gfile, gridfs* gfs, const char* remote_name, const char* content_type );

/** Write to a GridFS file incrementally. You can call this function any number
 *  of times with a new buffer each time. This allows you to effectively
 *  stream to a GridFS file. When finished, be sure to call gridfs_writer_done.
 *
 *  @return - 1 if successful, 0 otherwise
 */
void gridfile_write_buffer( gridfile* gfile, const char* data, gridfs_offset length );

/** Signal that writing of this gridfile is complete by
 *  writing any buffered chunks along with the entry in the
 *  files collection.
 *
 *  @return - the file object if successful; otherwise 0.
 */
bson gridfile_writer_done( gridfile* gfile );

/** Store a buffer as a GridFS file.
 *  @param gfs - the working GridFS
 *  @param data - pointer to buffer to store in GridFS
 *  @param length - length of the buffer
 *  @param remotename - filename for use in the database
 *  @param contenttype - optional MIME type for this object
 *  @return - the file object
 */
bson gridfs_store_buffer(gridfs* gfs, const char* data, gridfs_offset length,
    const char* remotename,
    const char * contenttype);

/** Open the file referenced by filename and store it as a GridFS file.
 *  @param gfs - the working GridFS
 *  @param filename - local filename relative to the process
 *  @param remotename - optional filename for use in the database
 *  @param contenttype - optional MIME type for this object
 *  @return - the file object
 */
bson gridfs_store_file(gridfs* gfs, const char* filename,
         const char* remotename, const char* contenttype);

/** Removes the files referenced by filename from the db
 *  @param gfs - the working GridFS
 *  @param filename - the filename of the file/s to be removed
 */
void gridfs_remove_filename(gridfs* gfs, const char* filename);

/** Find the first query within the GridFS and return it as a GridFile
 *  @param gfs - the working GridFS
 *  @param query - a pointer to the bson with the query data
 *  @param gfile - the output GridFile to be initialized
 *  @return 1 if successful, 0 otherwise
 */
int gridfs_find_query(gridfs* gfs, bson* query, gridfile* gfile );

/** Find the first file referenced by filename within the GridFS
 *  and return it as a GridFile
 *  @param gfs - the working GridFS
 *  @param filename - filename of the file to find
 *  @param gfile - the output GridFile to be intialized
 *  @return 1 if successful, 0 otherwise
 */
int gridfs_find_filename(gridfs* gfs, const char *filename,
         gridfile* gfile);

/*--------------------------------------------------------------------*/


/** Initializes a  GridFile containing the GridFS and file bson
 *  @param gfs - the GridFS where the GridFile is located
 *  @param meta - the file object
 *  @param gfile - the output GridFile that is being initialized
 *  @return 1 if successful, 0 otherwise
 */
int gridfile_init(gridfs* gfs, bson* meta, gridfile* gfile);

/** Destroys the GridFile
 *  @param oGridFIle - the GridFile being destroyed
 */
void gridfile_destroy(gridfile* gfile);

/** Returns whether or not the GridFile exists
 *  @param gfile - the GridFile being examined
 */
int gridfile_exists(gridfile* gfile);

/** Returns the filename of GridFile
 *  @param gfile - the working GridFile
 *  @return - the filename of the Gridfile
 */
const char * gridfile_get_filename(gridfile* gfile);

/** Returns the size of the chunks of the GridFile
 *  @param gfile - the working GridFile
 *  @return - the size of the chunks of the Gridfile
 */
int gridfile_get_chunksize(gridfile* gfile);

/** Returns the length of GridFile's data
 *  @param gfile - the working GridFile
 *  @return - the length of the Gridfile's data
 */
gridfs_offset gridfile_get_contentlength(gridfile* gfile);

/** Returns the MIME type of the GridFile
 *  @param gfile - the working GridFile
 *  @return - the MIME type of the Gridfile
 *            (NULL if no type specified)
 */
const char* gridfile_get_contenttype(gridfile* gfile);

/** Returns the upload date of GridFile
 *  @param gfile - the working GridFile
 *  @return - the upload date of the Gridfile
 */
bson_date_t gridfile_get_uploaddate(gridfile* gfile);

/** Returns the MD5 of GridFile
 *  @param gfile - the working GridFile
 *  @return - the MD5 of the Gridfile
 */
const char* gridfile_get_md5(gridfile* gfile);

/** Returns the field in GridFile specified by name
 *  @param gfile - the working GridFile
 *  @param name - the name of the field to be returned
 *  @return - the data of the field specified
 *            (NULL if none exists)
 */
const char *gridfile_get_field(gridfile* gfile,
                                 const char* name);

/** Returns a boolean field in GridFile specified by name
 *  @param gfile - the working GridFile
 *  @param name - the name of the field to be returned
 *  @return - the boolean of the field specified
 *            (NULL if none exists)
 */
bson_bool_t gridfile_get_boolean(gridfile* gfile,
                                 const char* name);

/** Returns the metadata of GridFile
 *  @param gfile - the working GridFile
 *  @return - the metadata of the Gridfile in a bson object
 *            (an empty bson is returned if none exists)
 */
bson gridfile_get_metadata(gridfile* gfile);

/** Returns the number of chunks in the GridFile
 *  @param gfile - the working GridFile
 *  @return - the number of chunks in the Gridfile
 */
int gridfile_get_numchunks(gridfile* gfile);

/** Returns chunk n of GridFile
 *  @param gfile - the working GridFile
 *  @return - the nth chunk of the Gridfile
 */
bson gridfile_get_chunk(gridfile* gfile, int n);

/** Returns a mongo_cursor of *size* chunks starting with chunk *start*
 *  @param gfile - the working GridFile
 *  @param start - the first chunk in the cursor
 *  @param size - the number of chunks to be returned
 *  @return - mongo_cursor of the chunks (must be destroyed after use)
 */
mongo_cursor* gridfile_get_chunks(gridfile* gfile, int start, int size);

/** Writes the GridFile to a stream
 *  @param gfile - the working GridFile
 *  @param stream - the file stream to write to
 */
gridfs_offset gridfile_write_file(gridfile* gfile, FILE* stream);

/** Reads length bytes from the GridFile to a buffer
 *  and updates the position in the file.
 *  (assumes the buffer is large enough)
 *  (if size is greater than EOF gridfile_read reads until EOF)
 *  @param gfile - the working GridFile
 *  @param size - the amount of bytes to be read
 *  @param buf - the buffer to read to
 *  @return - the number of bytes read
 */
gridfs_offset gridfile_read(gridfile* gfile, gridfs_offset size, char* buf);

/** Updates the position in the file
 *  (If the offset goes beyond the contentlength,
 *  the position is updated to the end of the file.)
 *  @param gfile - the working GridFile
 *  @param offset - the position to update to
 *  @return - resulting offset location
 */
gridfs_offset gridfile_seek(gridfile* gfile, gridfs_offset offset);

/*==============================================================*/
/* --- gridfs.c */
#define TRUE 1
#define FALSE 0

/*--------------------------------------------------------------------*/

static bson * chunk_new(bson_oid_t id, int chunkNumber,
    const char * data, int len)

{
  bson * b;
  bson_buffer buf;

  b = (bson *)bson_malloc(sizeof(bson));
  if (b == NULL) return NULL;

  bson_buffer_init(&buf);
  bson_append_oid(&buf, "files_id", &id);
  bson_append_int(&buf, "n", chunkNumber);
  bson_append_binary(&buf, "data", 2, data, len);
  bson_from_buffer(b, &buf);
  return  b;
}

/*--------------------------------------------------------------------*/

static void chunk_free(bson * oChunk)

{
  bson_destroy(oChunk);
  free(oChunk);
}

/*--------------------------------------------------------------------*/

int gridfs_init(mongo_connection * client, const char * dbname,
    const char * prefix, gridfs* gfs)
{
  int options;
  bson_buffer bb;
  bson b;
  bson out;
  bson_bool_t success;

  gfs->client = client;

  /* Allocate space to own the dbname */
  gfs->dbname = (const char *)bson_malloc(strlen(dbname)+1);
  if (gfs->dbname == NULL) {
    return FALSE;
  }
  strcpy((char*)gfs->dbname, dbname);

  /* Allocate space to own the prefix */
  if (prefix == NULL) prefix = "fs";
  gfs->prefix = (const char *)bson_malloc(strlen(prefix)+1);
  if (gfs->prefix == NULL) {
    free((char*)gfs->dbname);
    return FALSE;
  }
  strcpy((char *)gfs->prefix, prefix);

  /* Allocate space to own files_ns */
  gfs->files_ns =
    (const char *) bson_malloc (strlen(prefix)+strlen(dbname)+strlen(".files")+2);
  if (gfs->files_ns == NULL) {
    free((char*)gfs->dbname);
    free((char*)gfs->prefix);
    return FALSE;
  }
  strcpy((char*)gfs->files_ns, dbname);
  strcat((char*)gfs->files_ns, ".");
  strcat((char*)gfs->files_ns, prefix);
  strcat((char*)gfs->files_ns, ".files");

  /* Allocate space to own chunks_ns */
  gfs->chunks_ns = (const char *) bson_malloc(strlen(prefix) + strlen(dbname)
              + strlen(".chunks") + 2);
  if (gfs->chunks_ns == NULL) {
    free((char*)gfs->dbname);
    free((char*)gfs->prefix);
    free((char*)gfs->files_ns);
    return FALSE;
  }
  strcpy((char*)gfs->chunks_ns, dbname);
  strcat((char*)gfs->chunks_ns, ".");
  strcat((char*)gfs->chunks_ns, prefix);
  strcat((char*)gfs->chunks_ns, ".chunks");

  bson_buffer_init(&bb);
  bson_append_int(&bb, "filename", 1);
  bson_from_buffer(&b, &bb);
  options = 0;
  success = mongo_create_index(gfs->client, gfs->files_ns, &b, options, &out);
  bson_destroy(&b);
  if (!success) {
    free((char*)gfs->dbname);
    free((char*)gfs->prefix);
    free((char*)gfs->files_ns);
    free((char*)gfs->chunks_ns);
    return FALSE;
  }

  bson_buffer_init(&bb);
  bson_append_int(&bb, "files_id", 1);
  bson_append_int(&bb, "n", 1);
  bson_from_buffer(&b, &bb);
  options = MONGO_INDEX_UNIQUE;
  success = mongo_create_index(gfs->client, gfs->chunks_ns, &b, options, &out);
  bson_destroy(&b);
  if (!success) {
    free((char*)gfs->dbname);
    free((char*)gfs->prefix);
    free((char*)gfs->files_ns);
    free((char*)gfs->chunks_ns);
    return FALSE;
  }

  return TRUE;
}

/*--------------------------------------------------------------------*/

void gridfs_destroy(gridfs* gfs)

{
  if (gfs == NULL) return;
  if (gfs->dbname) free((char*)gfs->dbname);
  if (gfs->prefix) free((char*)gfs->prefix);
  if (gfs->files_ns) free((char*)gfs->files_ns);
  if (gfs->chunks_ns) free((char*)gfs->chunks_ns);
}

/*--------------------------------------------------------------------*/

static bson gridfs_insert_file( gridfs* gfs, const char* name,
             const bson_oid_t id, gridfs_offset length,
             const char* contenttype)
{
  bson command;
  bson res;
  bson ret;
  bson_buffer buf;
  bson_iterator it;

  /* Check run md5 */
  bson_buffer_init(&buf);
  bson_append_oid(&buf, "filemd5", &id);
  bson_append_string(&buf, "root", gfs->prefix);
  bson_from_buffer(&command, &buf);
  assert(mongo_run_command(gfs->client, gfs->dbname,
         &command, &res));
  bson_destroy(&command);

  /* Create and insert BSON for file metadata */
  bson_buffer_init(&buf);
  bson_append_oid(&buf, "_id", &id);
  if (name != NULL && *name != '\0') {
    bson_append_string(&buf, "filename", name);
  }
  bson_append_long(&buf, "length", length);
  bson_append_int(&buf, "chunkSize", DEFAULT_CHUNK_SIZE);
  bson_append_date(&buf, "uploadDate", (bson_date_t)1000*time(NULL));
  bson_find(&it, &res, "md5");
  bson_append_string(&buf, "md5", bson_iterator_string(&it));
  bson_destroy(&res);
  if (contenttype != NULL && *contenttype != '\0') {
    bson_append_string(&buf, "contentType", contenttype);
  }
  bson_from_buffer(&ret, &buf);
  mongo_insert(gfs->client, gfs->files_ns, &ret);

  return ret;
}

/*--------------------------------------------------------------------*/

bson gridfs_store_buffer( gridfs* gfs, const char* data,
        gridfs_offset length, const char* remotename,
        const char * contenttype)

{
  char const * const end = data + length;
  bson_oid_t id;
  int chunkNumber = 0;
  int chunkLen;
  bson * oChunk;

  /* Large files Assertion */
  assert(length <= 0xffffffff);

  /* Generate and append an oid*/
  bson_oid_gen(&id);

  /* Insert the file's data chunk by chunk */
  while (data < end) {
    chunkLen = DEFAULT_CHUNK_SIZE < (unsigned int)(end - data) ?
      DEFAULT_CHUNK_SIZE : (unsigned int)(end - data);
    oChunk = chunk_new( id, chunkNumber, data, chunkLen );
    mongo_insert(gfs->client, gfs->chunks_ns, oChunk);
    chunk_free(oChunk);
    chunkNumber++;
    data += chunkLen;
  }

  /* Inserts file's metadata */
  return gridfs_insert_file(gfs, remotename, id, length, contenttype);
}

/*--------------------------------------------------------------------*/

void gridfile_writer_init( gridfile* gfile, gridfs* gfs,
    const char* remote_name, const char* content_type )
{
    gfile->gfs = gfs;

    bson_oid_gen( &(gfile->id) );
    gfile->chunk_num = 0;
    gfile->length = 0;
    gfile->pending_len = 0;
    gfile->pending_data = NULL;

    gfile->remote_name = (const char *)bson_malloc( strlen( remote_name ) + 1 );
    strcpy( (char *)gfile->remote_name, remote_name );

    gfile->content_type = (const char *)bson_malloc( strlen( content_type ) );
    strcpy( (char *)gfile->content_type, content_type );
}

/*--------------------------------------------------------------------*/

void gridfile_write_buffer( gridfile* gfile, const char* data, gridfs_offset length )
{

  int bytes_left = 0;
  int data_partial_len = 0;
  int chunks_to_write = 0;
  char* buffer;
  bson* oChunk;
  gridfs_offset to_write = length + gfile->pending_len;

  if ( to_write < DEFAULT_CHUNK_SIZE ) { /* Less than one chunk to write */
    if( gfile->pending_data ) {
      gfile->pending_data = (char *)bson_realloc((void *)gfile->pending_data, gfile->pending_len + to_write);
      memcpy( gfile->pending_data + gfile->pending_len, data, length );
    } else if (to_write > 0) {
      gfile->pending_data = (char *)bson_malloc(to_write);
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
      buffer = (char *)bson_malloc( DEFAULT_CHUNK_SIZE );
      memcpy(buffer, gfile->pending_data, gfile->pending_len);
      memcpy(buffer + gfile->pending_len, data, data_partial_len);

      oChunk = chunk_new(gfile->id, gfile->chunk_num, buffer, DEFAULT_CHUNK_SIZE);
      mongo_insert(gfile->gfs->client, gfile->gfs->chunks_ns, oChunk);
      chunk_free(oChunk);
      gfile->chunk_num++;
      gfile->length += DEFAULT_CHUNK_SIZE;
      data += data_partial_len;

      chunks_to_write--;

      free(buffer);
    }

    while( chunks_to_write > 0 ) {
      oChunk = chunk_new(gfile->id, gfile->chunk_num, data, DEFAULT_CHUNK_SIZE);
      mongo_insert(gfile->gfs->client, gfile->gfs->chunks_ns, oChunk);
      chunk_free(oChunk);
      gfile->chunk_num++;
      chunks_to_write--;
      gfile->length += DEFAULT_CHUNK_SIZE;
      data += DEFAULT_CHUNK_SIZE;
    }

    free(gfile->pending_data);

    /* If there are any leftover bytes, store them as pending data. */
    if( bytes_left == 0 )
      gfile->pending_data = NULL;
    else {
      gfile->pending_data = (char *)bson_malloc( bytes_left );
      memcpy( gfile->pending_data, data, bytes_left );
    }

    gfile->pending_len = bytes_left;
  }
}

/*--------------------------------------------------------------------*/

bson gridfile_writer_done( gridfile* gfile )
{

  /* write any remaining pending chunk data.
   * pending data will always take up less than one chunk
   */
  bson* oChunk;
  if( gfile->pending_data )
  {
    oChunk = chunk_new(gfile->id, gfile->chunk_num, gfile->pending_data, gfile->pending_len);
    mongo_insert(gfile->gfs->client, gfile->gfs->chunks_ns, oChunk);
    chunk_free(oChunk);
    free(gfile->pending_data);
    gfile->length += gfile->pending_len;
  }

  /* insert into files collection */
  return gridfs_insert_file(gfile->gfs, gfile->remote_name, gfile->id,
      gfile->length, gfile->content_type);
}

/*--------------------------------------------------------------------*/

bson gridfs_store_file(gridfs* gfs, const char* filename,
      const char* remotename, const char* contenttype)
{
  char buffer[DEFAULT_CHUNK_SIZE];
  FILE * fd;
  bson_oid_t id;
  int chunkNumber = 0;
  gridfs_offset length = 0;
  gridfs_offset chunkLen = 0;
  bson* oChunk;

  /* Open the file and the correct stream */
  if (strcmp(filename, "-") == 0) fd = stdin;
  else fd = fopen(filename, "rb");
  assert(fd != NULL); /* No such file */

  /* Generate and append an oid*/
  bson_oid_gen(&id);

  /* Insert the file chunk by chunk */
  chunkLen = fread(buffer, 1, DEFAULT_CHUNK_SIZE, fd);
  do {
    oChunk = chunk_new( id, chunkNumber, buffer, chunkLen );
    mongo_insert(gfs->client, gfs->chunks_ns, oChunk);
    chunk_free(oChunk);
    length += chunkLen;
    chunkNumber++;
    chunkLen = fread(buffer, 1, DEFAULT_CHUNK_SIZE, fd);
  } while (chunkLen != 0);

  /* Close the file stream */
  if (fd != stdin) fclose(fd);

  /* Large files Assertion */
  /* assert(length <= 0xffffffff); */

  /* Optional Remote Name */
  if (remotename == NULL || *remotename == '\0') {
    remotename = filename; }

  /* Inserts file's metadata */
  return gridfs_insert_file(gfs, remotename, id, length, contenttype);
}

/*--------------------------------------------------------------------*/

void gridfs_remove_filename(gridfs* gfs, const char* filename )

{
  bson query;
  bson_buffer buf;
  mongo_cursor* files;
  bson file;
  bson_iterator it;
  bson_oid_t id;
  bson b;

  bson_buffer_init(&buf);
  bson_append_string(&buf, "filename", filename);
  bson_from_buffer(&query, &buf);
  files = mongo_find(gfs->client, gfs->files_ns, &query, NULL, 0, 0, 0);
  bson_destroy(&query);

  /* Remove each file and it's chunks from files named filename */
  while (mongo_cursor_next(files)) {
    file = files->current;
    bson_find(&it, &file, "_id");
    id = *bson_iterator_oid(&it);

    /* Remove the file with the specified id */
    bson_buffer_init(&buf);
    bson_append_oid(&buf, "_id", &id);
    bson_from_buffer(&b, &buf);
    mongo_remove( gfs->client, gfs->files_ns, &b);
    bson_destroy(&b);

    /* Remove all chunks from the file with the specified id */
    bson_buffer_init(&buf);
    bson_append_oid(&buf, "files_id", &id);
    bson_from_buffer(&b, &buf);
    mongo_remove( gfs->client, gfs->chunks_ns, &b);
    bson_destroy(&b);
  }

}

/*--------------------------------------------------------------------*/

int gridfs_find_query(gridfs* gfs, bson* query,
         gridfile* gfile )

{
  bson_buffer date_buffer;
  bson uploadDate;
  bson_buffer buf;
  bson finalQuery;
  bson out;
  int i;

  bson_buffer_init(&date_buffer);
  bson_append_int(&date_buffer, "uploadDate", -1);
  bson_from_buffer(&uploadDate, &date_buffer);
  bson_buffer_init(&buf);
  bson_append_bson(&buf, "query", query);
  bson_append_bson(&buf, "orderby", &uploadDate);
  bson_from_buffer(&finalQuery, &buf);


  i = (mongo_find_one(gfs->client, gfs->files_ns,
         &finalQuery, NULL, &out));
  bson_destroy(&uploadDate);
  bson_destroy(&finalQuery);
  if (!i)
    return FALSE;
  else {
    gridfile_init(gfs, &out, gfile);
    bson_destroy(&out);
    return TRUE;
  }
}

/*--------------------------------------------------------------------*/

int gridfs_find_filename(gridfs* gfs, const char* filename,
       gridfile* gfile)

{
  bson query;
  bson_buffer buf;
  int i;

  bson_buffer_init(&buf);
  bson_append_string(&buf, "filename", filename);
  bson_from_buffer(&query, &buf) ;
  i = gridfs_find_query(gfs, &query, gfile);
  bson_destroy(&query);
  return i;
}

/*--------------------------------------------------------------------*/

int gridfile_init(gridfs* gfs, bson* meta, gridfile* gfile)

{
  gfile->gfs = gfs;
  gfile->pos = 0;
  gfile->meta = (bson*)bson_malloc(sizeof(bson));
  if (gfile->meta == NULL) return FALSE;
  bson_copy(gfile->meta, meta);
  return TRUE;
}

/*--------------------------------------------------------------------*/

void gridfile_destroy(gridfile* gfile)

{
  bson_destroy(gfile->meta);
  free(gfile->meta);
}

/*--------------------------------------------------------------------*/

bson_bool_t gridfile_exists(gridfile* gfile)

{
  return (bson_bool_t)(gfile != NULL || gfile->meta == NULL);
}

/*--------------------------------------------------------------------*/

const char* gridfile_get_filename(gridfile* gfile)

{
  bson_iterator it;

  bson_find(&it, gfile->meta, "filename");
  return bson_iterator_string(&it);
}

/*--------------------------------------------------------------------*/

int gridfile_get_chunksize(gridfile* gfile)

{
  bson_iterator it;

  bson_find(&it, gfile->meta, "chunkSize");
  return bson_iterator_int(&it);
}

/*--------------------------------------------------------------------*/

gridfs_offset gridfile_get_contentlength(gridfile* gfile)

{
  bson_iterator it;

  bson_find(&it, gfile->meta, "length");

  if( bson_iterator_type( &it ) == bson_int )
    return (gridfs_offset)bson_iterator_int( &it );
  else
    return (gridfs_offset)bson_iterator_long( &it );
}

/*--------------------------------------------------------------------*/

const char *gridfile_get_contenttype(gridfile* gfile)

{
  bson_iterator it;

  if (bson_find(&it, gfile->meta, "contentType"))
    return bson_iterator_string( &it );
  else return NULL;
}

/*--------------------------------------------------------------------*/

bson_date_t gridfile_get_uploaddate(gridfile* gfile)

{
  bson_iterator it;

  bson_find(&it, gfile->meta, "uploadDate");
  return bson_iterator_date( &it );
}

/*--------------------------------------------------------------------*/

const char* gridfile_get_md5(gridfile* gfile)

{
  bson_iterator it;

  bson_find(&it, gfile->meta, "md5");
  return bson_iterator_string( &it );
}

/*--------------------------------------------------------------------*/

const char* gridfile_get_field(gridfile* gfile, const char* name)

{
  bson_iterator it;

  bson_find(&it, gfile->meta, name);
  return bson_iterator_value( &it );
}

/*--------------------------------------------------------------------*/

bson_bool_t gridfile_get_boolean(gridfile* gfile, const char* name)
{
  bson_iterator it;

  bson_find(&it, gfile->meta, name);
  return bson_iterator_bool( &it );
}

/*--------------------------------------------------------------------*/
bson gridfile_get_metadata(gridfile* gfile)

{
  bson sub;
  bson_iterator it;

  if (bson_find(&it, gfile->meta, "metadata")) {
    bson_iterator_subobject( &it, &sub );
    return sub;
  }
  else {
    bson_empty(&sub);
    return sub;
  }
}

/*--------------------------------------------------------------------*/

int gridfile_get_numchunks(gridfile* gfile)

{
  bson_iterator it;
  gridfs_offset length;
  gridfs_offset chunkSize;
  double numchunks;

  bson_find(&it, gfile->meta, "length");

  if( bson_iterator_type( &it ) == bson_int )
    length = (gridfs_offset)bson_iterator_int( &it );
  else
    length = (gridfs_offset)bson_iterator_long( &it );

  bson_find(&it, gfile->meta, "chunkSize");
  chunkSize = bson_iterator_int(&it);
  numchunks = ((double)length/(double)chunkSize);
  return (numchunks - (int)numchunks > 0)
    ? (int)(numchunks+1)
    : (int)(numchunks);
}

/*--------------------------------------------------------------------*/

bson gridfile_get_chunk(gridfile* gfile, int n)

{
  bson query;
  bson out;
  bson_buffer buf;
  bson_iterator it;
  bson_oid_t id;

  bson_buffer_init(&buf);
  bson_find(&it, gfile->meta, "_id");
  id = *bson_iterator_oid(&it);
  bson_append_oid(&buf, "files_id", &id);
  bson_append_int(&buf, "n", n);
  bson_from_buffer(&query, &buf);

  assert(mongo_find_one(gfile->gfs->client,
      gfile->gfs->chunks_ns,
      &query, NULL, &out));
  return out;
}

/*--------------------------------------------------------------------*/

mongo_cursor* gridfile_get_chunks(gridfile* gfile, int start, int size)

{
  bson_iterator it;
  bson_oid_t id;
  bson_buffer gte_buf;
  bson gte_bson;
  bson_buffer query_buf;
  bson query_bson;
  bson_buffer orderby_buf;
  bson orderby_bson;
  bson_buffer command_buf;
  bson command_bson;

  bson_find(&it, gfile->meta, "_id");
  id = *bson_iterator_oid(&it);

  bson_buffer_init(&query_buf);
  bson_append_oid(&query_buf, "files_id", &id);
  if (size == 1) {
    bson_append_int(&query_buf, "n", start);
  } else {
    bson_buffer_init(&gte_buf);
    bson_append_int(&gte_buf, "$gte", start);
    bson_from_buffer(&gte_bson, &gte_buf);
    bson_append_bson(&query_buf, "n", &gte_bson);
  }
  bson_from_buffer(&query_bson, &query_buf);

  bson_buffer_init(&orderby_buf);
  bson_append_int(&orderby_buf, "n", 1);
  bson_from_buffer(&orderby_bson, &orderby_buf);

  bson_buffer_init(&command_buf);
  bson_append_bson(&command_buf, "query", &query_bson);
  bson_append_bson(&command_buf, "orderby", &orderby_bson);
  bson_from_buffer(&command_bson, &command_buf);

  return mongo_find(gfile->gfs->client, gfile->gfs->chunks_ns,
        &command_bson, NULL, size, 0, 0);
}

/*--------------------------------------------------------------------*/

gridfs_offset gridfile_write_file(gridfile* gfile, FILE *stream)

{
  int i;
  size_t len;
  bson chunk;
  bson_iterator it;
  const char* data;
  const int num = gridfile_get_numchunks( gfile );
  int xx;

  for ( i=0; i<num; i++ ){
    chunk = gridfile_get_chunk( gfile, i );
    bson_find( &it, &chunk, "data" );
    len = bson_iterator_bin_len( &it );
    data = bson_iterator_bin_data( &it );
    xx = fwrite( data , sizeof(char), len, stream );
  }

  return gridfile_get_contentlength(gfile);
}

/*--------------------------------------------------------------------*/

gridfs_offset gridfile_read(gridfile* gfile, gridfs_offset size, char* buf)

{
  mongo_cursor* chunks;
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
  const char * chunk_data;

  contentlength = gridfile_get_contentlength(gfile);
  chunksize = gridfile_get_chunksize(gfile);
  size = (contentlength - gfile->pos < size)
    ? contentlength - gfile->pos
    : size;
  bytes_left = size;

  first_chunk = (gfile->pos)/chunksize;
  last_chunk = (gfile->pos+size-1)/chunksize;
  total_chunks = last_chunk - first_chunk + 1;
  chunks = gridfile_get_chunks(gfile, first_chunk, total_chunks);

  for (i = 0; i < total_chunks; i++) {
    mongo_cursor_next(chunks);
    chunk = chunks->current;
    bson_find(&it, &chunk, "data");
    chunk_len = bson_iterator_bin_len( &it );
    chunk_data = bson_iterator_bin_data( &it );
    if (i == 0) {
      chunk_data += (gfile->pos)%chunksize;
      chunk_len -= (gfile->pos)%chunksize;
    }
    if (bytes_left > chunk_len) {
      memcpy(buf, chunk_data, chunk_len);
      bytes_left -= chunk_len;
      buf += chunk_len;
    } else {
      memcpy(buf, chunk_data, bytes_left);
    }
  }

  mongo_cursor_destroy(chunks);
  gfile->pos = gfile->pos + size;

  return size;
}

/*--------------------------------------------------------------------*/

gridfs_offset gridfile_seek(gridfile* gfile, gridfs_offset offset)

{
  gridfs_offset length;

  length = gridfile_get_contentlength(gfile);
  gfile->pos = length < offset ? length : offset;
  return gfile->pos;
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
        mongo_close_socket( conn->sock );
        return mongo_conn_no_socket;
    }

    if ( connect( conn->sock , (struct sockaddr*)&conn->sa , conn->addressSize ) ){
        mongo_close_socket( conn->sock );
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


void mongo_replset_init_conn(mongo_connection* conn) {
    conn->seeds = NULL;
}

int mongo_replset_add_seed(mongo_connection* conn, const char* host, int port) {
    mongo_host_port* host_port = bson_malloc(sizeof(mongo_host_port));
    host_port->port = port;
    host_port->next = NULL;
    strncpy( host_port->host, host, strlen(host) );

    if( conn->seeds == NULL )
        conn->seeds = host_port;
    else {
        mongo_host_port* p = conn->seeds;
        while( p->next != NULL )
          p = p->next;
        p->next = host_port;
    }

    return 0;
}

mongo_conn_return mongo_replset_connect(mongo_connection* conn) {

    bson* out;
    bson_bool_t ismaster;

    mongo_host_port* node = conn->seeds;

    conn->sock = 0;
    conn->connected = 0;

    while( node != NULL ) {

        memset( conn->sa.sin_zero , 0 , sizeof(conn->sa.sin_zero) );
        conn->sa.sin_family = AF_INET;
        conn->sa.sin_port = htons(node->port);
        conn->sa.sin_addr.s_addr = inet_addr(node->host);

        conn->addressSize = sizeof(conn->sa);

        conn->sock = socket( AF_INET, SOCK_STREAM, 0 );
        if ( conn->sock <= 0 ){
            mongo_close_socket( conn->sock );
            return mongo_conn_no_socket;
        }

        if ( connect( conn->sock , (struct sockaddr*)&conn->sa , conn->addressSize ) ){
            mongo_close_socket( conn->sock );
        }

        setsockopt( conn->sock, IPPROTO_TCP, TCP_NODELAY, (char *) &one, sizeof(one) );

        /* Check whether this is the primary node */
        ismaster = 0;

        out = bson_malloc(sizeof(bson));
        out->data = NULL;
        out->owned = 0;

        if (mongo_simple_int_command(conn, "admin", "ismaster", 1, out)) {
            bson_iterator it;
            bson_find(&it, out, "ismaster");
            ismaster = bson_iterator_bool(&it);
            free(out);
        }

        if(ismaster) {
            conn->connected = 1;
        }
        else {
            mongo_close_socket( conn->sock );
        }

        node = node->next;
    }

    /* TODO signals */

    /* Might be nice to know which node is primary */
    /* con->primary = NULL; */
    if( conn->connected == 1 )
        return 0;
    else
        return -1;
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
    size_t len;

    looping_read(conn, &head, sizeof(head));
    looping_read(conn, &fields, sizeof(fields));

    bson_little_endian32(&len, &head.len);

    if (len < sizeof(head)+sizeof(fields) || len > 64*1024*1024)
        MONGO_THROW(MONGO_EXCEPT_NETWORK); /* most likely corruption */

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
    volatile mongo_cursor * cursor; /* volatile due to longjmp in mongo exception handler */
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
        free((mongo_cursor*)cursor); /* cast away volatile, not changing type */
        MONGO_RETHROW();
    }

    sl = strlen(ns)+1;
    cursor->ns = bson_malloc(sl);
    if (!cursor->ns){
        free(cursor->mm);
        free((mongo_cursor*)cursor); /* cast away volatile, not changing type */
        return 0;
    }
    memcpy((void*)cursor->ns, ns, sl); /* cast needed to silence GCC warning */
    cursor->conn = conn;
    cursor->current.data = NULL;
    return (mongo_cursor*)cursor;
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

    mongo_close_socket( conn->sock );
    
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

static void mongo_pass_digest(const char* user, const char* pass, char hex_digest[32+1])
{
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

void mongo_cmd_add_user(mongo_connection* conn, const char* db, const char* user, const char* pass){
    bson_buffer bb;
    bson user_obj;
    bson pass_obj;
    char hex_digest[32+1];
    char* ns = bson_malloc(strlen(db) + strlen(".system.users") + 1);

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

    char hex_digest[32+1];

    if (mongo_simple_int_command(conn, db, "getnonce", 1, &from_db)){
        bson_iterator it;
        bson_find(&it, &from_db, "nonce");
        nonce = bson_iterator_string(&it);
    }else{
        return 0;
    }

    mongo_pass_digest(user, pass, hex_digest);

    {	DIGEST_CTX ctx = rpmDigestInit(PGPHASHALGO_MD5, RPMDIGEST_NONE);
	const char * _digest = NULL;
	int xx;
	xx = rpmDigestUpdate(ctx, nonce, strlen(nonce));
	xx = rpmDigestUpdate(ctx, user, strlen(user));
	xx = rpmDigestUpdate(ctx, hex_digest, 32);
	xx = rpmDigestFinal(ctx, &_digest, NULL, 1);
	strncpy(hex_digest, _digest, 32+1);
	_digest = _free(_digest);
    }

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

static void rpmmgoFini(void * _mgo)
	/*@globals fileSystem @*/
	/*@modifies *_mgo, fileSystem @*/
{
    rpmmgo mgo = _mgo;

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

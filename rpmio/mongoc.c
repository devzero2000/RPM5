/*
 * Copyright 2013 MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "system.h"

#include <sys/mman.h>
#include <sys/shm.h>

#if defined(__linux__)
# include <sys/syscall.h>
#elif defined(_WIN32)
# include <process.h>
#endif

#ifndef _WIN32
# include <netdb.h>
# include <netinet/tcp.h>
#endif

#ifdef _WIN32
# include <io.h>
# include <share.h>
#endif
#ifdef _WIN32
# include <winsock2.h>
# include <winerror.h>
#endif

#ifdef HAVE_LIBSASL2
#include <sasl/sasl.h>
#include <sasl/saslutil.h>
#endif

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include <openssl/crypto.h>

#include <bcon.h>
#include <mongoc.h>

#include "debug.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#endif

/*==============================================================*/
/* --- mongoc-array.c */

void
_mongoc_array_init (mongoc_array_t *array,
                    size_t          element_size)
{
   bson_return_if_fail(array);
   bson_return_if_fail(element_size);

   array->len = 0;
   array->element_size = element_size;
   array->allocated = 128;
   array->data = bson_malloc0(array->allocated);
}


void
_mongoc_array_destroy (mongoc_array_t *array)
{
   if (array && array->data) {
      bson_free(array->data);
   }
}


void
_mongoc_array_append_vals (mongoc_array_t *array,
                           const void     *data,
                           uint32_t   n_elements)
{
   size_t len;
   size_t off;
   size_t next_size;

   bson_return_if_fail(array);
   bson_return_if_fail(data);

   off = array->element_size * array->len;
   len = (size_t)n_elements * array->element_size;
   if ((off + len) > array->allocated) {
      next_size = bson_next_power_of_two(off + len);
      array->data = bson_realloc(array->data, next_size);
      array->allocated = next_size;
   }

   memcpy((uint8_t *)array->data + off, data, len);

   array->len += n_elements;
}

/*==============================================================*/
/* --- mongoc-buffer.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "buffer"

#ifndef MONGOC_BUFFER_DEFAULT_SIZE
# define MONGOC_BUFFER_DEFAULT_SIZE 1024
#endif


#define SPACE_FOR(_b, _sz) (((ssize_t)(_b)->datalen - (ssize_t)(_b)->off - (ssize_t)(_b)->len) >= (ssize_t)(_sz))


/**
 * _mongoc_buffer_init:
 * @buffer: A mongoc_buffer_t to initialize.
 * @buf: A data buffer to attach to @buffer.
 * @buflen: The size of @buflen.
 * @realloc_func: A function to resize @buf.
 *
 * Initializes @buffer for use. If additional space is needed by @buffer, then
 * @realloc_func will be called to resize @buf.
 *
 * @buffer takes ownership of @buf and will realloc it to zero bytes when
 * cleaning up the data structure.
 */
void
_mongoc_buffer_init (mongoc_buffer_t   *buffer,
                     uint8_t           *buf,
                     size_t             buflen,
                     bson_realloc_func  realloc_func,
                     void              *realloc_data)
{
   bson_return_if_fail (buffer);
   bson_return_if_fail (buflen || !buf);

   if (!realloc_func) {
      realloc_func = bson_realloc_ctx;
   }

   if (!buflen) {
      buflen = MONGOC_BUFFER_DEFAULT_SIZE;
   }

   if (!buf) {
      buf = realloc_func (NULL, buflen, NULL);
   }

   memset (buffer, 0, sizeof *buffer);

   buffer->data = buf;
   buffer->datalen = buflen;
   buffer->len = 0;
   buffer->off = 0;
   buffer->realloc_func = realloc_func;
   buffer->realloc_data = realloc_data;
}


/**
 * _mongoc_buffer_destroy:
 * @buffer: A mongoc_buffer_t.
 *
 * Cleanup after @buffer and release any allocated resources.
 */
void
_mongoc_buffer_destroy (mongoc_buffer_t *buffer)
{
   bson_return_if_fail(buffer);

   if (buffer->data && buffer->realloc_func) {
      buffer->realloc_func (buffer->data, 0, buffer->realloc_data);
   }

   memset(buffer, 0, sizeof *buffer);
}


/**
 * _mongoc_buffer_clear:
 * @buffer: A mongoc_buffer_t.
 * @zero: If the memory should be zeroed.
 *
 * Clears a buffers contents and resets it to initial state. You can request
 * that the memory is zeroed, which might be useful if you know the contents
 * contain security related information.
 */
void
_mongoc_buffer_clear (mongoc_buffer_t *buffer,
                      bool      zero)
{
   bson_return_if_fail(buffer);

   if (zero) {
      memset(buffer->data, 0, buffer->datalen);
   }

   buffer->off = 0;
   buffer->len = 0;
}


/**
 * mongoc_buffer_append_from_stream:
 * @buffer; A mongoc_buffer_t.
 * @stream: The stream to read from.
 * @size: The number of bytes to read.
 * @timeout_msec: The number of milliseconds to wait or -1 for the default
 * @error: A location for a bson_error_t, or NULL.
 *
 * Reads from stream @size bytes and stores them in @buffer. This can be used
 * in conjunction with reading RPCs from a stream. You read from the stream
 * into this buffer and then scatter the buffer into the RPC.
 *
 * Returns: true if successful; otherwise false and @error is set.
 */
bool
_mongoc_buffer_append_from_stream (mongoc_buffer_t *buffer,
                                   mongoc_stream_t *stream,
                                   size_t           size,
                                   int32_t          timeout_msec,
                                   bson_error_t    *error)
{
   uint8_t *buf;
   ssize_t ret;

   ENTRY;

   bson_return_val_if_fail (buffer, false);
   bson_return_val_if_fail (stream, false);
   bson_return_val_if_fail (size, false);

   BSON_ASSERT (buffer->datalen);
   BSON_ASSERT ((buffer->datalen + size) < INT_MAX);

   if (!SPACE_FOR (buffer, size)) {
      if (buffer->len) {
         memmove(&buffer->data[0], &buffer->data[buffer->off], buffer->len);
      }
      buffer->off = 0;
      if (!SPACE_FOR (buffer, size)) {
         buffer->datalen = bson_next_power_of_two (size + buffer->len + buffer->off);
         buffer->data = buffer->realloc_func (buffer->data, buffer->datalen, NULL);
      }
   }

   buf = &buffer->data[buffer->off + buffer->len];

   BSON_ASSERT ((buffer->off + buffer->len + size) <= buffer->datalen);

   ret = mongoc_stream_read (stream, buf, size, size, timeout_msec);
   if (ret != (ssize_t)size) {
      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_SOCKET,
                      "Failed to read %"PRIu64" bytes from socket.",
                      (uint64_t)size);
      RETURN (false);
   }

   buffer->len += ret;

   RETURN (true);
}


/**
 * _mongoc_buffer_fill:
 * @buffer: A mongoc_buffer_t.
 * @stream: A stream to read from.
 * @min_bytes: The minumum number of bytes to read.
 * @error: A location for a bson_error_t or NULL.
 *
 * Attempts to fill the entire buffer, or at least @min_bytes.
 *
 * Returns: The number of buffered bytes, or -1 on failure.
 */
ssize_t
_mongoc_buffer_fill (mongoc_buffer_t *buffer,
                     mongoc_stream_t *stream,
                     size_t           min_bytes,
                     int32_t          timeout_msec,
                     bson_error_t    *error)
{
   ssize_t ret;
   size_t avail_bytes;

   ENTRY;

   bson_return_val_if_fail(buffer, false);
   bson_return_val_if_fail(stream, false);
#ifdef	DYING
   bson_return_val_if_fail(min_bytes >= 0, false);
#endif

   BSON_ASSERT (buffer->data);
   BSON_ASSERT (buffer->datalen);

   if (min_bytes <= buffer->len) {
      RETURN (buffer->len);
   }

   min_bytes -= buffer->len;

   if (buffer->len) {
      memmove (&buffer->data[0], &buffer->data[buffer->off], buffer->len);
   }

   buffer->off = 0;

   if (!SPACE_FOR (buffer, min_bytes)) {
      buffer->datalen = bson_next_power_of_two (buffer->len + min_bytes);
      buffer->data = buffer->realloc_func (buffer->data, buffer->datalen,
                                           buffer->realloc_data);
   }

   avail_bytes = buffer->datalen - buffer->len;

   ret = mongoc_stream_read (stream,
                             &buffer->data[buffer->off + buffer->len],
                             avail_bytes, min_bytes, timeout_msec);

   if (ret == -1) {
      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_SOCKET,
                      "Failed to buffer %u bytes within %d milliseconds.",
                      (unsigned)min_bytes, (int)timeout_msec);
      RETURN (-1);
   }

   buffer->len += ret;

   if (buffer->len < min_bytes) {
      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_SOCKET,
                      "Could only buffer %u of %u bytes in %d milliseconds.",
                      (unsigned)buffer->len,
                      (unsigned)min_bytes,
                      (int)timeout_msec);
      RETURN (-1);
   }

   RETURN (buffer->len);
}

/*==============================================================*/
/* --- mongoc-rpc.c */

#define RPC(_name, _code) \
   static void \
   _mongoc_rpc_gather_##_name (mongoc_rpc_##_name##_t *rpc, \
                               mongoc_array_t *array) \
   { \
      mongoc_iovec_t iov; \
      BSON_ASSERT(rpc); \
      BSON_ASSERT(array); \
      rpc->msg_len = 0; \
      _code \
   }
#define INT32_FIELD(_name) \
   iov.iov_base = (void *)&rpc->_name; \
   iov.iov_len = 4; \
   BSON_ASSERT(iov.iov_len); \
   rpc->msg_len += (int32_t)iov.iov_len; \
   _mongoc_array_append_val(array, iov);
#define ENUM_FIELD INT32_FIELD
#define INT64_FIELD(_name) \
   iov.iov_base = (void *)&rpc->_name; \
   iov.iov_len = 8; \
   BSON_ASSERT(iov.iov_len); \
   rpc->msg_len += (int32_t)iov.iov_len; \
   _mongoc_array_append_val(array, iov);
#define CSTRING_FIELD(_name) \
   BSON_ASSERT(rpc->_name); \
   iov.iov_base = (void *)rpc->_name; \
   iov.iov_len = strlen(rpc->_name) + 1; \
   BSON_ASSERT(iov.iov_len); \
   rpc->msg_len += (int32_t)iov.iov_len; \
   _mongoc_array_append_val(array, iov);
#define BSON_FIELD(_name) \
   do { \
      int32_t __l; \
      memcpy(&__l, rpc->_name, 4); \
      __l = BSON_UINT32_FROM_LE(__l); \
      iov.iov_base = (void *)rpc->_name; \
      iov.iov_len = __l; \
      BSON_ASSERT(iov.iov_len); \
      rpc->msg_len += (int32_t)iov.iov_len; \
      _mongoc_array_append_val(array, iov); \
   } while (0);
#define BSON_OPTIONAL(_check, _code) \
   if (rpc->_check) { _code }
#define BSON_ARRAY_FIELD(_name) \
   if (rpc->_name##_len) { \
      iov.iov_base = (void *)rpc->_name; \
      iov.iov_len = rpc->_name##_len; \
      BSON_ASSERT(iov.iov_len); \
      rpc->msg_len += (int32_t)iov.iov_len; \
      _mongoc_array_append_val(array, iov); \
   }
#define IOVEC_ARRAY_FIELD(_name) \
   do { \
      ssize_t _i; \
      BSON_ASSERT(rpc->n_##_name); \
      for (_i = 0; _i < rpc->n_##_name; _i++) { \
         BSON_ASSERT(rpc->_name[_i].iov_len); \
         rpc->msg_len += (int32_t)rpc->_name[_i].iov_len; \
         _mongoc_array_append_val(array, rpc->_name[_i]); \
      } \
   } while (0);
#define RAW_BUFFER_FIELD(_name) \
   iov.iov_base = (void *)rpc->_name; \
   iov.iov_len = rpc->_name##_len; \
   BSON_ASSERT(iov.iov_len); \
   rpc->msg_len += (int32_t)iov.iov_len; \
   _mongoc_array_append_val(array, iov);
#define INT64_ARRAY_FIELD(_len, _name) \
   iov.iov_base = (void *)&rpc->_len; \
   iov.iov_len = 4; \
   BSON_ASSERT(iov.iov_len); \
   rpc->msg_len += (int32_t)iov.iov_len; \
   _mongoc_array_append_val(array, iov); \
   iov.iov_base = (void *)rpc->_name; \
   iov.iov_len = rpc->_len * 8; \
   BSON_ASSERT(iov.iov_len); \
   rpc->msg_len += (int32_t)iov.iov_len; \
   _mongoc_array_append_val(array, iov);



/*==============================================================*/
/* --- op-delete.def */

RPC(
  delete,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(zero)
  CSTRING_FIELD(collection)
  INT32_FIELD(flags)
  BSON_FIELD(selector)
)
/*==============================================================*/
/* --- op-get-more.def */

RPC(
  get_more,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(zero)
  CSTRING_FIELD(collection)
  INT32_FIELD(n_return)
  INT64_FIELD(cursor_id)
)
/*==============================================================*/
/* --- op-header.def */

RPC(
  header,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
)
/*==============================================================*/
/* --- op-insert.def */

RPC(
  insert,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(flags)
  CSTRING_FIELD(collection)
  IOVEC_ARRAY_FIELD(documents)
)
/*==============================================================*/
/* --- op-kill-cursors.def */

RPC(
  kill_cursors,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(zero)
  INT64_ARRAY_FIELD(n_cursors, cursors)
)
/*==============================================================*/
/* --- op-msg.def */

RPC(
  msg,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  CSTRING_FIELD(msg)
)
/*==============================================================*/
/* --- op-query.def */

RPC(
  query,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(flags)
  CSTRING_FIELD(collection)
  INT32_FIELD(skip)
  INT32_FIELD(n_return)
  BSON_FIELD(query)
  BSON_OPTIONAL(fields, BSON_FIELD(fields))
)
/*==============================================================*/
/* --- op-reply.def */

RPC(
  reply,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(flags)
  INT64_FIELD(cursor_id)
  INT32_FIELD(start_from)
  INT32_FIELD(n_returned)
  BSON_ARRAY_FIELD(documents)
)
/*==============================================================*/
/* --- op-update.def */

RPC(
  update,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(zero)
  CSTRING_FIELD(collection)
  INT32_FIELD(flags)
  BSON_FIELD(selector)
  BSON_FIELD(update)
)
/*==============================================================*/


#undef RPC
#undef ENUM_FIELD
#undef INT32_FIELD
#undef INT64_FIELD
#undef INT64_ARRAY_FIELD
#undef CSTRING_FIELD
#undef BSON_FIELD
#undef BSON_ARRAY_FIELD
#undef IOVEC_ARRAY_FIELD
#undef RAW_BUFFER_FIELD
#undef BSON_OPTIONAL


#if BSON_BYTE_ORDER == BSON_BIG_ENDIAN

#define RPC(_name, _code) \
   static void \
   _mongoc_rpc_swab_to_le_##_name (mongoc_rpc_##_name##_t *rpc) \
   { \
      BSON_ASSERT(rpc); \
      _code \
   }
#define INT32_FIELD(_name) \
   rpc->_name = BSON_UINT32_FROM_LE(rpc->_name);
#define ENUM_FIELD INT32_FIELD
#define INT64_FIELD(_name) \
   rpc->_name = BSON_UINT64_FROM_LE(rpc->_name);
#define CSTRING_FIELD(_name)
#define BSON_FIELD(_name)
#define BSON_ARRAY_FIELD(_name)
#define IOVEC_ARRAY_FIELD(_name)
#define BSON_OPTIONAL(_check, _code) \
   if (rpc->_check) { _code }
#define RAW_BUFFER_FIELD(_name)
#define INT64_ARRAY_FIELD(_len, _name) \
   do { \
      ssize_t i; \
      for (i = 0; i < rpc->_len; i++) { \
         rpc->_name[i] = BSON_UINT64_FROM_LE(rpc->_name[i]); \
      } \
      rpc->_len = BSON_UINT32_FROM_LE(rpc->_len); \
   } while (0);


/*==============================================================*/
/* --- op-delete.def */

RPC(
  delete,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(zero)
  CSTRING_FIELD(collection)
  INT32_FIELD(flags)
  BSON_FIELD(selector)
)
/*==============================================================*/
/* --- op-get-more.def */

RPC(
  get_more,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(zero)
  CSTRING_FIELD(collection)
  INT32_FIELD(n_return)
  INT64_FIELD(cursor_id)
)
/*==============================================================*/
/* --- op-header.def */

RPC(
  header,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
)
/*==============================================================*/
/* --- op-insert.def */

RPC(
  insert,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(flags)
  CSTRING_FIELD(collection)
  IOVEC_ARRAY_FIELD(documents)
)
/*==============================================================*/
/* --- op-kill-cursors.def */

RPC(
  kill_cursors,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(zero)
  INT64_ARRAY_FIELD(n_cursors, cursors)
)
/*==============================================================*/
/* --- op-msg.def */

RPC(
  msg,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  CSTRING_FIELD(msg)
)
/*==============================================================*/
/* --- op-query.def */

RPC(
  query,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(flags)
  CSTRING_FIELD(collection)
  INT32_FIELD(skip)
  INT32_FIELD(n_return)
  BSON_FIELD(query)
  BSON_OPTIONAL(fields, BSON_FIELD(fields))
)
/*==============================================================*/
/* --- op-reply.def */

RPC(
  reply,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(flags)
  INT64_FIELD(cursor_id)
  INT32_FIELD(start_from)
  INT32_FIELD(n_returned)
  BSON_ARRAY_FIELD(documents)
)
/*==============================================================*/
/* --- op-update.def */

RPC(
  update,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(zero)
  CSTRING_FIELD(collection)
  INT32_FIELD(flags)
  BSON_FIELD(selector)
  BSON_FIELD(update)
)
/*==============================================================*/

#undef RPC
#undef INT64_ARRAY_FIELD

#define RPC(_name, _code) \
   static void \
   _mongoc_rpc_swab_from_le_##_name (mongoc_rpc_##_name##_t *rpc) \
   { \
      BSON_ASSERT(rpc); \
      _code \
   }
#define INT64_ARRAY_FIELD(_len, _name) \
   do { \
      ssize_t i; \
      rpc->_len = BSON_UINT32_FROM_LE(rpc->_len); \
      for (i = 0; i < rpc->_len; i++) { \
         rpc->_name[i] = BSON_UINT64_FROM_LE(rpc->_name[i]); \
      } \
   } while (0);


/*==============================================================*/
/* --- op-delete.def */

RPC(
  delete,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(zero)
  CSTRING_FIELD(collection)
  INT32_FIELD(flags)
  BSON_FIELD(selector)
)
/*==============================================================*/
/* --- op-get-more.def */

RPC(
  get_more,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(zero)
  CSTRING_FIELD(collection)
  INT32_FIELD(n_return)
  INT64_FIELD(cursor_id)
)
/*==============================================================*/
/* --- op-header.def */

RPC(
  header,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
)
/*==============================================================*/
/* --- op-insert.def */

RPC(
  insert,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(flags)
  CSTRING_FIELD(collection)
  IOVEC_ARRAY_FIELD(documents)
)
/*==============================================================*/
/* --- op-kill-cursors.def */

RPC(
  kill_cursors,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(zero)
  INT64_ARRAY_FIELD(n_cursors, cursors)
)
/*==============================================================*/
/* --- op-msg.def */

RPC(
  msg,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  CSTRING_FIELD(msg)
)
/*==============================================================*/
/* --- op-query.def */

RPC(
  query,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(flags)
  CSTRING_FIELD(collection)
  INT32_FIELD(skip)
  INT32_FIELD(n_return)
  BSON_FIELD(query)
  BSON_OPTIONAL(fields, BSON_FIELD(fields))
)
/*==============================================================*/
/* --- op-reply.def */

RPC(
  reply,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(flags)
  INT64_FIELD(cursor_id)
  INT32_FIELD(start_from)
  INT32_FIELD(n_returned)
  BSON_ARRAY_FIELD(documents)
)
/*==============================================================*/
/* --- op-update.def */

RPC(
  update,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(zero)
  CSTRING_FIELD(collection)
  INT32_FIELD(flags)
  BSON_FIELD(selector)
  BSON_FIELD(update)
)
/*==============================================================*/


#undef RPC
#undef ENUM_FIELD
#undef INT32_FIELD
#undef INT64_FIELD
#undef INT64_ARRAY_FIELD
#undef CSTRING_FIELD
#undef BSON_FIELD
#undef BSON_ARRAY_FIELD
#undef IOVEC_ARRAY_FIELD
#undef BSON_OPTIONAL
#undef RAW_BUFFER_FIELD

#endif /* BSON_BYTE_ORDER == BSON_BIG_ENDIAN */


#define RPC(_name, _code) \
   static void \
   _mongoc_rpc_printf_##_name (mongoc_rpc_##_name##_t *rpc) \
   { \
      BSON_ASSERT(rpc); \
      _code \
   }
#define INT32_FIELD(_name) \
   printf("  "#_name" : %d\n", rpc->_name);
#define ENUM_FIELD(_name) \
   printf("  "#_name" : %u\n", rpc->_name);
#define INT64_FIELD(_name) \
   printf("  "#_name" : %" PRIi64 "\n", (int64_t)rpc->_name);
#define CSTRING_FIELD(_name) \
   printf("  "#_name" : %s\n", rpc->_name);
#define BSON_FIELD(_name) \
   do { \
      bson_t b; \
      char *s; \
      int32_t __l; \
      memcpy(&__l, rpc->_name, 4); \
      __l = BSON_UINT32_FROM_LE(__l); \
      bson_init_static(&b, rpc->_name, __l); \
      s = bson_as_json(&b, NULL); \
      printf("  "#_name" : %s\n", s); \
      bson_free(s); \
      bson_destroy(&b); \
   } while (0);
#define BSON_ARRAY_FIELD(_name) \
   do { \
      bson_reader_t *__r; \
      bool __eof; \
      const bson_t *__b; \
      __r = bson_reader_new_from_data(rpc->_name, rpc->_name##_len); \
      while ((__b = bson_reader_read(__r, &__eof))) { \
         char *s = bson_as_json(__b, NULL); \
         printf("  "#_name" : %s\n", s); \
         bson_free(s); \
      } \
      bson_reader_destroy(__r); \
   } while (0);
#define IOVEC_ARRAY_FIELD(_name) \
   do { \
      ssize_t _i; \
      size_t _j; \
      for (_i = 0; _i < rpc->n_##_name; _i++) { \
         printf("  "#_name" : "); \
         for (_j = 0; _j < rpc->_name[_i].iov_len; _j++) { \
            uint8_t u; \
            u = ((char *)rpc->_name[_i].iov_base)[_j]; \
            printf(" %02x", u); \
         } \
         printf("\n"); \
      } \
   } while (0);
#define BSON_OPTIONAL(_check, _code) \
   if (rpc->_check) { _code }
#define RAW_BUFFER_FIELD(_name) \
   { \
      ssize_t __i; \
      printf("  "#_name" :"); \
      for (__i = 0; __i < rpc->_name##_len; __i++) { \
         uint8_t u; \
         u = ((char *)rpc->_name)[__i]; \
         printf(" %02x", u); \
      } \
      printf("\n"); \
   }
#define INT64_ARRAY_FIELD(_len, _name) \
   do { \
      ssize_t i; \
      for (i = 0; i < rpc->_len; i++) { \
         printf("  "#_name" : %" PRIi64 "\n", (int64_t)rpc->_name[i]); \
      } \
      rpc->_len = BSON_UINT32_FROM_LE(rpc->_len); \
   } while (0);


/*==============================================================*/
/* --- op-delete.def */

RPC(
  delete,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(zero)
  CSTRING_FIELD(collection)
  INT32_FIELD(flags)
  BSON_FIELD(selector)
)
/*==============================================================*/
/* --- op-get-more.def */

RPC(
  get_more,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(zero)
  CSTRING_FIELD(collection)
  INT32_FIELD(n_return)
  INT64_FIELD(cursor_id)
)
/*==============================================================*/
/* --- op-header.def */

RPC(
  header,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
)
/*==============================================================*/
/* --- op-insert.def */

RPC(
  insert,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(flags)
  CSTRING_FIELD(collection)
  IOVEC_ARRAY_FIELD(documents)
)
/*==============================================================*/
/* --- op-kill-cursors.def */

RPC(
  kill_cursors,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(zero)
  INT64_ARRAY_FIELD(n_cursors, cursors)
)
/*==============================================================*/
/* --- op-msg.def */

RPC(
  msg,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  CSTRING_FIELD(msg)
)
/*==============================================================*/
/* --- op-query.def */

RPC(
  query,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(flags)
  CSTRING_FIELD(collection)
  INT32_FIELD(skip)
  INT32_FIELD(n_return)
  BSON_FIELD(query)
  BSON_OPTIONAL(fields, BSON_FIELD(fields))
)
/*==============================================================*/
/* --- op-reply.def */

RPC(
  reply,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(flags)
  INT64_FIELD(cursor_id)
  INT32_FIELD(start_from)
  INT32_FIELD(n_returned)
  BSON_ARRAY_FIELD(documents)
)
/*==============================================================*/
/* --- op-update.def */

RPC(
  update,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(zero)
  CSTRING_FIELD(collection)
  INT32_FIELD(flags)
  BSON_FIELD(selector)
  BSON_FIELD(update)
)
/*==============================================================*/


#undef RPC
#undef ENUM_FIELD
#undef INT32_FIELD
#undef INT64_FIELD
#undef INT64_ARRAY_FIELD
#undef CSTRING_FIELD
#undef BSON_FIELD
#undef BSON_ARRAY_FIELD
#undef IOVEC_ARRAY_FIELD
#undef BSON_OPTIONAL
#undef RAW_BUFFER_FIELD


#define RPC(_name, _code) \
   static bool \
   _mongoc_rpc_scatter_##_name (mongoc_rpc_##_name##_t *rpc, \
                                const uint8_t *buf, \
                                size_t buflen) \
   { \
      BSON_ASSERT(rpc); \
      BSON_ASSERT(buf); \
      BSON_ASSERT(buflen); \
      _code \
      return true; \
   }
#define INT32_FIELD(_name) \
   if (buflen < 4) { \
      return false; \
   } \
   memcpy(&rpc->_name, buf, 4); \
   buflen -= 4; \
   buf += 4;
#define ENUM_FIELD INT32_FIELD
#define INT64_FIELD(_name) \
   if (buflen < 8) { \
      return false; \
   } \
   memcpy(&rpc->_name, buf, 8); \
   buflen -= 8; \
   buf += 8;
#define INT64_ARRAY_FIELD(_len, _name) \
   do { \
      size_t needed; \
      if (buflen < 4) { \
         return false; \
      } \
      memcpy(&rpc->_len, buf, 4); \
      buflen -= 4; \
      buf += 4; \
      needed = BSON_UINT32_FROM_LE(rpc->_len) * 8; \
      if (needed > buflen) { \
         return false; \
      } \
      rpc->_name = (void *)buf; \
      buf += needed; \
      buflen -= needed; \
   } while (0);
#define CSTRING_FIELD(_name) \
   do { \
      size_t __i; \
      bool found = false; \
      for (__i = 0; __i < buflen; __i++) { \
         if (!buf[__i]) { \
            rpc->_name = (const char *)buf; \
            buflen -= __i + 1; \
            buf += __i + 1; \
            found = true; \
            break; \
         } \
      } \
      if (!found) { \
         return false; \
      } \
   } while (0);
#define BSON_FIELD(_name) \
   do { \
      uint32_t __l; \
      if (buflen < 4) { \
         return false; \
      } \
      memcpy(&__l, buf, 4); \
      __l = BSON_UINT32_FROM_LE(__l); \
      if (__l < 5 || __l > buflen) { \
         return false; \
      } \
      rpc->_name = (uint8_t *)buf; \
      buf += __l; \
      buflen -= __l; \
   } while (0);
#define BSON_ARRAY_FIELD(_name) \
   rpc->_name = (uint8_t *)buf; \
   rpc->_name##_len = (int32_t)buflen; \
   buf = NULL; \
   buflen = 0;
#define BSON_OPTIONAL(_check, _code) \
   if (buflen) { \
      _code \
   }
#define IOVEC_ARRAY_FIELD(_name) \
   rpc->_name##_recv.iov_base = (void *)buf; \
   rpc->_name##_recv.iov_len = buflen; \
   rpc->_name = &rpc->_name##_recv; \
   rpc->n_##_name = 1; \
   buf = NULL; \
   buflen = 0;
#define RAW_BUFFER_FIELD(_name) \
   rpc->_name = (void *)buf; \
   rpc->_name##_len = (int32_t)buflen; \
   buf = NULL; \
   buflen = 0;


/*==============================================================*/
/* --- op-delete.def */

RPC(
  delete,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(zero)
  CSTRING_FIELD(collection)
  INT32_FIELD(flags)
  BSON_FIELD(selector)
)
/*==============================================================*/
/* --- op-get-more.def */

RPC(
  get_more,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(zero)
  CSTRING_FIELD(collection)
  INT32_FIELD(n_return)
  INT64_FIELD(cursor_id)
)
/*==============================================================*/
/* --- op-header.def */

RPC(
  header,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
)
/*==============================================================*/
/* --- op-insert.def */

RPC(
  insert,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(flags)
  CSTRING_FIELD(collection)
  IOVEC_ARRAY_FIELD(documents)
)
/*==============================================================*/
/* --- op-kill-cursors.def */

RPC(
  kill_cursors,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(zero)
  INT64_ARRAY_FIELD(n_cursors, cursors)
)
/*==============================================================*/
/* --- op-msg.def */

RPC(
  msg,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  CSTRING_FIELD(msg)
)
/*==============================================================*/
/* --- op-query.def */

RPC(
  query,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(flags)
  CSTRING_FIELD(collection)
  INT32_FIELD(skip)
  INT32_FIELD(n_return)
  BSON_FIELD(query)
  BSON_OPTIONAL(fields, BSON_FIELD(fields))
)
/*==============================================================*/
/* --- op-reply.def */

RPC(
  reply,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(flags)
  INT64_FIELD(cursor_id)
  INT32_FIELD(start_from)
  INT32_FIELD(n_returned)
  BSON_ARRAY_FIELD(documents)
)
/*==============================================================*/
/* --- op-update.def */

RPC(
  update,
  INT32_FIELD(msg_len)
  INT32_FIELD(request_id)
  INT32_FIELD(response_to)
  INT32_FIELD(opcode)
  INT32_FIELD(zero)
  CSTRING_FIELD(collection)
  INT32_FIELD(flags)
  BSON_FIELD(selector)
  BSON_FIELD(update)
)
/*==============================================================*/


#undef RPC
#undef ENUM_FIELD
#undef INT32_FIELD
#undef INT64_FIELD
#undef INT64_ARRAY_FIELD
#undef CSTRING_FIELD
#undef BSON_FIELD
#undef BSON_ARRAY_FIELD
#undef IOVEC_ARRAY_FIELD
#undef BSON_OPTIONAL
#undef RAW_BUFFER_FIELD


void
_mongoc_rpc_gather (mongoc_rpc_t   *rpc,
                    mongoc_array_t *array)
{
   bson_return_if_fail(rpc);
   bson_return_if_fail(array);

   switch ((mongoc_opcode_t)rpc->header.opcode) {
   case MONGOC_OPCODE_REPLY:
      _mongoc_rpc_gather_reply(&rpc->reply, array);
      return;
   case MONGOC_OPCODE_MSG:
      _mongoc_rpc_gather_msg(&rpc->msg, array);
      return;
   case MONGOC_OPCODE_UPDATE:
      _mongoc_rpc_gather_update(&rpc->update, array);
      return;
   case MONGOC_OPCODE_INSERT:
      _mongoc_rpc_gather_insert(&rpc->insert, array);
      return;
   case MONGOC_OPCODE_QUERY:
      _mongoc_rpc_gather_query(&rpc->query, array);
      return;
   case MONGOC_OPCODE_GET_MORE:
      _mongoc_rpc_gather_get_more(&rpc->get_more, array);
      return;
   case MONGOC_OPCODE_DELETE:
      _mongoc_rpc_gather_delete(&rpc->delete, array);
      return;
   case MONGOC_OPCODE_KILL_CURSORS:
      _mongoc_rpc_gather_kill_cursors(&rpc->kill_cursors, array);
      return;
   default:
      MONGOC_WARNING("Unknown rpc type: 0x%08x", rpc->header.opcode);
      break;
   }
}


void
_mongoc_rpc_swab_to_le (mongoc_rpc_t *rpc)
{
#if BSON_BYTE_ORDER != BSON_LITTLE_ENDIAN
   mongoc_opcode_t opcode;

   bson_return_if_fail(rpc);

   opcode = rpc->header.opcode;

   switch (opcode) {
   case MONGOC_OPCODE_REPLY:
      _mongoc_rpc_swab_to_le_reply(&rpc->reply);
      break;
   case MONGOC_OPCODE_MSG:
      _mongoc_rpc_swab_to_le_msg(&rpc->msg);
      break;
   case MONGOC_OPCODE_UPDATE:
      _mongoc_rpc_swab_to_le_update(&rpc->update);
      break;
   case MONGOC_OPCODE_INSERT:
      _mongoc_rpc_swab_to_le_insert(&rpc->insert);
      break;
   case MONGOC_OPCODE_QUERY:
      _mongoc_rpc_swab_to_le_query(&rpc->query);
      break;
   case MONGOC_OPCODE_GET_MORE:
      _mongoc_rpc_swab_to_le_get_more(&rpc->get_more);
      break;
   case MONGOC_OPCODE_DELETE:
      _mongoc_rpc_swab_to_le_delete(&rpc->delete);
      break;
   case MONGOC_OPCODE_KILL_CURSORS:
      _mongoc_rpc_swab_to_le_kill_cursors(&rpc->kill_cursors);
      break;
   default:
      MONGOC_WARNING("Unknown rpc type: 0x%08x", opcode);
      break;
   }
#endif
}


void
_mongoc_rpc_swab_from_le (mongoc_rpc_t *rpc)
{
#if BSON_BYTE_ORDER != BSON_LITTLE_ENDIAN
   mongoc_opcode_t opcode;

   bson_return_if_fail(rpc);

   opcode = BSON_UINT32_FROM_LE(rpc->header.opcode);

   switch (opcode) {
   case MONGOC_OPCODE_REPLY:
      _mongoc_rpc_swab_from_le_reply(&rpc->reply);
      break;
   case MONGOC_OPCODE_MSG:
      _mongoc_rpc_swab_from_le_msg(&rpc->msg);
      break;
   case MONGOC_OPCODE_UPDATE:
      _mongoc_rpc_swab_from_le_update(&rpc->update);
      break;
   case MONGOC_OPCODE_INSERT:
      _mongoc_rpc_swab_from_le_insert(&rpc->insert);
      break;
   case MONGOC_OPCODE_QUERY:
      _mongoc_rpc_swab_from_le_query(&rpc->query);
      break;
   case MONGOC_OPCODE_GET_MORE:
      _mongoc_rpc_swab_from_le_get_more(&rpc->get_more);
      break;
   case MONGOC_OPCODE_DELETE:
      _mongoc_rpc_swab_from_le_delete(&rpc->delete);
      break;
   case MONGOC_OPCODE_KILL_CURSORS:
      _mongoc_rpc_swab_from_le_kill_cursors(&rpc->kill_cursors);
      break;
   default:
      MONGOC_WARNING("Unknown rpc type: 0x%08x", rpc->header.opcode);
      break;
   }
#endif
}


void
_mongoc_rpc_printf (mongoc_rpc_t *rpc)
{
   bson_return_if_fail(rpc);

   switch ((mongoc_opcode_t)rpc->header.opcode) {
   case MONGOC_OPCODE_REPLY:
      _mongoc_rpc_printf_reply(&rpc->reply);
      break;
   case MONGOC_OPCODE_MSG:
      _mongoc_rpc_printf_msg(&rpc->msg);
      break;
   case MONGOC_OPCODE_UPDATE:
      _mongoc_rpc_printf_update(&rpc->update);
      break;
   case MONGOC_OPCODE_INSERT:
      _mongoc_rpc_printf_insert(&rpc->insert);
      break;
   case MONGOC_OPCODE_QUERY:
      _mongoc_rpc_printf_query(&rpc->query);
      break;
   case MONGOC_OPCODE_GET_MORE:
      _mongoc_rpc_printf_get_more(&rpc->get_more);
      break;
   case MONGOC_OPCODE_DELETE:
      _mongoc_rpc_printf_delete(&rpc->delete);
      break;
   case MONGOC_OPCODE_KILL_CURSORS:
      _mongoc_rpc_printf_kill_cursors(&rpc->kill_cursors);
      break;
   default:
      MONGOC_WARNING("Unknown rpc type: 0x%08x", rpc->header.opcode);
      break;
   }
}


bool
_mongoc_rpc_scatter (mongoc_rpc_t  *rpc,
                     const uint8_t *buf,
                     size_t         buflen)
{
   mongoc_opcode_t opcode;

   bson_return_val_if_fail(rpc, false);
   bson_return_val_if_fail(buf, false);
   bson_return_val_if_fail(buflen, false);

   memset (rpc, 0, sizeof *rpc);

   if (BSON_UNLIKELY(buflen < 16)) {
      return false;
   }

   if (!_mongoc_rpc_scatter_header(&rpc->header, buf, 16)) {
      return false;
   }

   opcode = BSON_UINT32_FROM_LE(rpc->header.opcode);

   switch (opcode) {
   case MONGOC_OPCODE_REPLY:
      return _mongoc_rpc_scatter_reply(&rpc->reply, buf, buflen);
   case MONGOC_OPCODE_MSG:
      return _mongoc_rpc_scatter_msg(&rpc->msg, buf, buflen);
   case MONGOC_OPCODE_UPDATE:
      return _mongoc_rpc_scatter_update(&rpc->update, buf, buflen);
   case MONGOC_OPCODE_INSERT:
      return _mongoc_rpc_scatter_insert(&rpc->insert, buf, buflen);
   case MONGOC_OPCODE_QUERY:
      return _mongoc_rpc_scatter_query(&rpc->query, buf, buflen);
   case MONGOC_OPCODE_GET_MORE:
      return _mongoc_rpc_scatter_get_more(&rpc->get_more, buf, buflen);
   case MONGOC_OPCODE_DELETE:
      return _mongoc_rpc_scatter_delete(&rpc->delete, buf, buflen);
   case MONGOC_OPCODE_KILL_CURSORS:
      return _mongoc_rpc_scatter_kill_cursors(&rpc->kill_cursors, buf, buflen);
   default:
      MONGOC_WARNING("Unknown rpc type: 0x%08x", opcode);
      return false;
   }
}


bool
_mongoc_rpc_reply_get_first (mongoc_rpc_reply_t *reply,
                             bson_t             *bson)
{
   int32_t len;

   bson_return_val_if_fail(reply, false);
   bson_return_val_if_fail(bson, false);

   if (!reply->documents || reply->documents_len < 4) {
      return false;
   }

   memcpy(&len, reply->documents, 4);
   len = BSON_UINT32_FROM_LE(len);
   if (reply->documents_len < len) {
      return false;
   }

   return bson_init_static(bson, reply->documents, len);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_rpc_needs_gle --
 *
 *       Checks to see if an rpc requires a getlasterror command to
 *       determine the success of the rpc.
 *
 *       The write_concern is checked to ensure that the caller wants
 *       to know about a failure.
 *
 * Returns:
 *       true if a getlasterror should be delivered; otherwise false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
_mongoc_rpc_needs_gle (mongoc_rpc_t                 *rpc,
                       const mongoc_write_concern_t *write_concern)
{
   bson_return_val_if_fail(rpc, false);

   switch (rpc->header.opcode) {
   case MONGOC_OPCODE_REPLY:
   case MONGOC_OPCODE_QUERY:
   case MONGOC_OPCODE_MSG:
   case MONGOC_OPCODE_GET_MORE:
   case MONGOC_OPCODE_KILL_CURSORS:
      return false;
   case MONGOC_OPCODE_INSERT:
   case MONGOC_OPCODE_UPDATE:
   case MONGOC_OPCODE_DELETE:
   default:
      break;
   }

   if (!write_concern || !mongoc_write_concern_get_w(write_concern)) {
      return false;
   }

   return true;
}

/*==============================================================*/
/* --- mongoc-bulk-operation.c */

/*
 * This is the implementation of both write commands and bulk write commands.
 * They are all implemented as one contiguous set since we'd like to cut down
 * on code duplication here.
 *
 * This implementation is currently naive.
 *
 * Some interesting optimizations might be:
 *
 *   - If unordered mode, send operations as we get them instead of waiting
 *     for execute() to be called. This could save us memcpy()'s too.
 *   - If there is no acknowledgement desired, keep a count of how many
 *     replies we need and ask the socket layer to skip that many bytes
 *     when reading.
 *   - Try to use iovec to send write commands with subdocuments rather than
 *     copying them into the write command document.
 */


mongoc_bulk_operation_t *
mongoc_bulk_operation_new (bool ordered)
{
   mongoc_bulk_operation_t *bulk;

   bulk = bson_malloc0 (sizeof *bulk);
   bulk->ordered = ordered;

   _mongoc_array_init (&bulk->commands, sizeof (mongoc_write_command_t));

   return bulk;
}


mongoc_bulk_operation_t *
_mongoc_bulk_operation_new (mongoc_client_t              *client,        /* IN */
                            const char                   *database,      /* IN */
                            const char                   *collection,    /* IN */
                            uint32_t                      hint,          /* IN */
                            bool                          ordered,       /* IN */
                            const mongoc_write_concern_t *write_concern) /* IN */
{
   mongoc_bulk_operation_t *bulk;

   BSON_ASSERT (client);
   BSON_ASSERT (collection);

   bulk = mongoc_bulk_operation_new (ordered);
   bulk->client = client;
   bulk->database = bson_strdup (database);
   bulk->collection = bson_strdup (collection);
   bulk->hint = hint;
   bulk->write_concern = mongoc_write_concern_copy (write_concern);
   bulk->executed = false;

   return bulk;
}


void
mongoc_bulk_operation_destroy (mongoc_bulk_operation_t *bulk) /* IN */
{
   mongoc_write_command_t *command;
   int i;

   if (bulk) {
      for (i = 0; i < (int)bulk->commands.len; i++) {
         command = &_mongoc_array_index (&bulk->commands,
                                         mongoc_write_command_t, i);
         _mongoc_write_command_destroy (command);
      }

      bson_free (bulk->database);
      bson_free (bulk->collection);
      mongoc_write_concern_destroy (bulk->write_concern);
      _mongoc_array_destroy (&bulk->commands);

      if (bulk->executed) {
         _mongoc_write_result_destroy (&bulk->result);
      }

      bson_free (bulk);
   }
}


void
mongoc_bulk_operation_remove (mongoc_bulk_operation_t *bulk,     /* IN */
                              const bson_t            *selector) /* IN */
{
   mongoc_write_command_t command = { 0 };

   bson_return_if_fail (bulk);
   bson_return_if_fail (selector);

   _mongoc_write_command_init_delete (&command, selector, true, bulk->ordered);
   _mongoc_array_append_val (&bulk->commands, command);
}


void
mongoc_bulk_operation_remove_one (mongoc_bulk_operation_t *bulk,     /* IN */
                                  const bson_t            *selector) /* IN */
{
   mongoc_write_command_t command = { 0 };

   bson_return_if_fail (bulk);
   bson_return_if_fail (selector);

   _mongoc_write_command_init_delete (&command, selector, false, bulk->ordered);
   _mongoc_array_append_val (&bulk->commands, command);
}


void
mongoc_bulk_operation_delete (mongoc_bulk_operation_t *bulk,
                              const bson_t            *selector)
{
   mongoc_bulk_operation_remove (bulk, selector);
}


void
mongoc_bulk_operation_delete_one (mongoc_bulk_operation_t *bulk,
                                  const bson_t            *selector)
{
   mongoc_bulk_operation_remove_one (bulk, selector);
}


void
mongoc_bulk_operation_insert (mongoc_bulk_operation_t *bulk,
                              const bson_t            *document)
{
   mongoc_write_command_t command = { 0 };
   mongoc_write_command_t *last;

   ENTRY;

   bson_return_if_fail (bulk);
   bson_return_if_fail (document);

   if (bulk->commands.len) {
      last = &_mongoc_array_index (&bulk->commands,
                                   mongoc_write_command_t,
                                   bulk->commands.len - 1);
      if (last->type == MONGOC_WRITE_COMMAND_INSERT) {
         _mongoc_write_command_insert_append (last, &document, 1);
         EXIT;
      }
   }

   _mongoc_write_command_init_insert (&command, &document, 1, bulk->ordered,
      !_mongoc_write_concern_needs_gle (bulk->write_concern));

   _mongoc_array_append_val (&bulk->commands, command);

   EXIT;
}


void
mongoc_bulk_operation_replace_one (mongoc_bulk_operation_t *bulk,
                                   const bson_t            *selector,
                                   const bson_t            *document,
                                   bool                     upsert)
{
   mongoc_write_command_t command = { 0 };
   size_t err_off;

   bson_return_if_fail (bulk);
   bson_return_if_fail (selector);
   bson_return_if_fail (document);

   if (!bson_validate (document,
                       (BSON_VALIDATE_DOT_KEYS | BSON_VALIDATE_DOLLAR_KEYS),
                       &err_off)) {
      MONGOC_WARNING ("%s(): replacement document may not contain "
                      "$ or . in keys. Ingoring document.",
                      __FUNCTION__);
      return;
   }

   _mongoc_write_command_init_update (&command, selector, document, upsert,
                                      false, bulk->ordered);
   _mongoc_array_append_val (&bulk->commands, command);
}


void
mongoc_bulk_operation_update (mongoc_bulk_operation_t *bulk,
                              const bson_t            *selector,
                              const bson_t            *document,
                              bool                     upsert)
{
   mongoc_write_command_t command = { 0 };
   bson_iter_t iter;

   bson_return_if_fail (bulk);
   bson_return_if_fail (selector);
   bson_return_if_fail (document);

   if (bson_iter_init (&iter, document)) {
      while (bson_iter_next (&iter)) {
         if (!strchr (bson_iter_key (&iter), '$')) {
            MONGOC_WARNING ("%s(): update only works with $ operators.",
                            __FUNCTION__);
            return;
         }
      }
   }

   _mongoc_write_command_init_update (&command, selector, document, upsert,
                                      true, bulk->ordered);
   _mongoc_array_append_val (&bulk->commands, command);
}


void
mongoc_bulk_operation_update_one (mongoc_bulk_operation_t *bulk,
                                  const bson_t            *selector,
                                  const bson_t            *document,
                                  bool                     upsert)
{
   mongoc_write_command_t command = { 0 };
   bson_iter_t iter;

   bson_return_if_fail (bulk);
   bson_return_if_fail (selector);
   bson_return_if_fail (document);

   if (bson_iter_init (&iter, document)) {
      while (bson_iter_next (&iter)) {
         if (!strchr (bson_iter_key (&iter), '$')) {
            MONGOC_WARNING ("%s(): update_one only works with $ operators.",
                            __FUNCTION__);
            return;
         }
      }
   }

   _mongoc_write_command_init_update (&command, selector, document, upsert,
                                      false, bulk->ordered);
   _mongoc_array_append_val (&bulk->commands, command);
}


uint32_t
mongoc_bulk_operation_execute (mongoc_bulk_operation_t *bulk,  /* IN */
                               bson_t                  *reply, /* OUT */
                               bson_error_t            *error) /* OUT */
{
   mongoc_write_command_t *command;
   uint32_t hint = 0;
   bool ret;
   int i;

   ENTRY;

   bson_return_val_if_fail (bulk, false);

   if (!bulk->write_concern) {
      bulk->write_concern = mongoc_write_concern_new ();
   }

   if (bulk->executed) {
      _mongoc_write_result_destroy (&bulk->result);
   }

   _mongoc_write_result_init (&bulk->result);

   bulk->executed = true;

   if (!bulk->client) {
      bson_set_error (error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      "mongoc_bulk_operation_execute() requires a client "
                      "and one has not been set.");
      return false;
   } else if (!bulk->database) {
      bson_set_error (error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      "mongoc_bulk_operation_execute() requires a database "
                      "and one has not been set.");
      return false;
   } else if (!bulk->collection) {
      bson_set_error (error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      "mongoc_bulk_operation_execute() requires a collection "
                      "and one has not been set.");
      return false;
   }

   if (reply) {
      bson_init (reply);
   }

   if (!bulk->commands.len) {
      bson_set_error (error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      "Cannot do an empty bulk write");
      RETURN (false);
   }

   for (i = 0; i < (int)bulk->commands.len; i++) {
      command = &_mongoc_array_index (&bulk->commands,
                                      mongoc_write_command_t, i);

      _mongoc_write_command_execute (command, bulk->client, hint,
                                     bulk->database, bulk->collection,
                                     bulk->write_concern, &bulk->result);

      hint = command->hint;

      if (bulk->result.failed && bulk->ordered) {
         GOTO (cleanup);
      }
   }

cleanup:
   ret = _mongoc_write_result_complete (&bulk->result, reply, error);

   RETURN (ret ? hint : 0);
}

void
mongoc_bulk_operation_set_write_concern (mongoc_bulk_operation_t      *bulk,
                                         const mongoc_write_concern_t *write_concern)
{
   bson_return_if_fail (bulk);

   if (bulk->write_concern) {
      mongoc_write_concern_destroy (bulk->write_concern);
   }

   if (write_concern) {
      bulk->write_concern = mongoc_write_concern_copy (write_concern);
   } else {
      bulk->write_concern = mongoc_write_concern_new ();
   }
}


void
mongoc_bulk_operation_set_database (mongoc_bulk_operation_t *bulk,
                                    const char              *database)
{
   bson_return_if_fail (bulk);

   if (bulk->database) {
      bson_free (bulk->database);
   }

   bulk->database = bson_strdup (database);
}


void
mongoc_bulk_operation_set_collection (mongoc_bulk_operation_t *bulk,
                                      const char              *collection)
{
   bson_return_if_fail (bulk);

   if (bulk->collection) {
      bson_free (bulk->collection);
   }

   bulk->collection = bson_strdup (collection);
}


void
mongoc_bulk_operation_set_client (mongoc_bulk_operation_t *bulk,
                                  void                    *client)
{
   bson_return_if_fail (bulk);

   bulk->client = client;
}


void
mongoc_bulk_operation_set_hint (mongoc_bulk_operation_t *bulk,
                                uint32_t                 hint)
{
   bson_return_if_fail (bulk);

   bulk->hint = hint;
}

/*==============================================================*/
/* --- mongoc-client.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "client"


#ifndef DEFAULT_CONNECTTIMEOUTMS
#define DEFAULT_CONNECTTIMEOUTMS (10 * 1000L)
#endif


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_connect_tcp --
 *
 *       Connect to a host using a TCP socket.
 *
 *       This will be performed synchronously and return a mongoc_stream_t
 *       that can be used to connect with the remote host.
 *
 * Returns:
 *       A newly allocated mongoc_stream_t if successful; otherwise
 *       NULL and @error is set.
 *
 * Side effects:
 *       @error is set if return value is NULL.
 *
 *--------------------------------------------------------------------------
 */

static mongoc_stream_t *
mongoc_client_connect_tcp (const mongoc_uri_t       *uri,
                           const mongoc_host_list_t *host,
                           bson_error_t             *error)
{
   mongoc_socket_t *sock = NULL;
   struct addrinfo hints;
   struct addrinfo *result, *rp;
   int32_t connecttimeoutms = DEFAULT_CONNECTTIMEOUTMS;
   int64_t expire_at;
   const bson_t *options;
   bson_iter_t iter;
   char portstr [8];
   int s;

   ENTRY;

   bson_return_val_if_fail (uri, NULL);
   bson_return_val_if_fail (host, NULL);

   if ((options = mongoc_uri_get_options (uri)) &&
       bson_iter_init_find (&iter, options, "connecttimeoutms") &&
       BSON_ITER_HOLDS_INT32 (&iter)) {
      if (!(connecttimeoutms = bson_iter_int32(&iter))) {
         connecttimeoutms = DEFAULT_CONNECTTIMEOUTMS;
      }
   }

   BSON_ASSERT (connecttimeoutms);
   expire_at = bson_get_monotonic_time () + (connecttimeoutms * 1000L);

   bson_snprintf (portstr, sizeof portstr, "%hu", host->port);

   memset (&hints, 0, sizeof hints);
   hints.ai_family = host->family;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags = 0;
   hints.ai_protocol = 0;

   s = getaddrinfo (host->host, portstr, &hints, &result);

   if (s != 0) {
      mongoc_counter_dns_failure_inc ();
      bson_set_error(error,
                     MONGOC_ERROR_STREAM,
                     MONGOC_ERROR_STREAM_NAME_RESOLUTION,
                     "Failed to resolve %s",
                     host->host);
      RETURN (NULL);
   }

   mongoc_counter_dns_success_inc ();

   for (rp = result; rp; rp = rp->ai_next) {
      /*
       * Create a new non-blocking socket.
       */
      if (!(sock = mongoc_socket_new (rp->ai_family,
                                      rp->ai_socktype,
                                      rp->ai_protocol))) {
         continue;
      }

      /*
       * Try to connect to the peer.
       */
      if (0 != mongoc_socket_connect (sock,
                                      rp->ai_addr,
                                      (socklen_t)rp->ai_addrlen,
                                      expire_at)) {
         mongoc_socket_destroy (sock);
         sock = NULL;
         continue;
      }

      break;
   }

   if (!sock) {
      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_CONNECT,
                      "Failed to connect to target host.");
      freeaddrinfo (result);
      RETURN (NULL);
   }

   freeaddrinfo (result);

   return mongoc_stream_socket_new (sock);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_connect_unix --
 *
 *       Connect to a MongoDB server using a UNIX domain socket.
 *
 * Returns:
 *       A newly allocated mongoc_stream_t if successful; otherwise
 *       NULL and @error is set.
 *
 * Side effects:
 *       @error is set if return value is NULL.
 *
 *--------------------------------------------------------------------------
 */

static mongoc_stream_t *
mongoc_client_connect_unix (const mongoc_uri_t       *uri,
                            const mongoc_host_list_t *host,
                            bson_error_t             *error)
{
#ifdef _WIN32
   ENTRY;
   bson_set_error (error,
                   MONGOC_ERROR_STREAM,
                   MONGOC_ERROR_STREAM_CONNECT,
                   "UNIX domain sockets not supported on win32.");
   RETURN (NULL);
#else
   struct sockaddr_un saddr;
   mongoc_socket_t *sock;
   mongoc_stream_t *ret = NULL;

   ENTRY;

   bson_return_val_if_fail (uri, NULL);
   bson_return_val_if_fail (host, NULL);

   memset (&saddr, 0, sizeof saddr);
   saddr.sun_family = AF_UNIX;
   bson_snprintf (saddr.sun_path, sizeof saddr.sun_path - 1,
                  "%s", host->host_and_port);

   sock = mongoc_socket_new (AF_UNIX, SOCK_STREAM, 0);

   if (sock == NULL) {
      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_SOCKET,
                      "Failed to create socket.");
      RETURN (NULL);
   }

   if (-1 == mongoc_socket_connect (sock,
                                    (struct sockaddr *)&saddr,
                                    sizeof saddr,
                                    -1)) {
      mongoc_socket_destroy (sock);
      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_CONNECT,
                      "Failed to connect to UNIX domain socket.");
      RETURN (NULL);
   }

   ret = mongoc_stream_socket_new (sock);

   RETURN (ret);
#endif
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_default_stream_initiator --
 *
 *       A mongoc_stream_initiator_t that will handle the various type
 *       of supported sockets by MongoDB including TCP and UNIX.
 *
 *       Language binding authors may want to implement an alternate
 *       version of this method to use their native stream format.
 *
 * Returns:
 *       A mongoc_stream_t if successful; otherwise NULL and @error is set.
 *
 * Side effects:
 *       @error is set if return value is NULL.
 *
 *--------------------------------------------------------------------------
 */

static mongoc_stream_t *
mongoc_client_default_stream_initiator (const mongoc_uri_t       *uri,
                                        const mongoc_host_list_t *host,
                                        void                     *user_data,
                                        bson_error_t             *error)
{
   mongoc_stream_t *base_stream = NULL;
#ifdef MONGOC_ENABLE_SSL
   mongoc_client_t *client = user_data;
   const bson_t *options;
   bson_iter_t iter;
   const char *mechanism;
#endif

   bson_return_val_if_fail (uri, NULL);
   bson_return_val_if_fail (host, NULL);

#ifndef MONGOC_ENABLE_SSL
   if (mongoc_uri_get_ssl (uri)) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_NO_ACCEPTABLE_PEER,
                      "SSL is not enabled in this build of mongo-c-driver.");
      return NULL;
   }
#endif


   switch (host->family) {
#if defined(AF_INET6)
   case AF_INET6:
#endif
   case AF_INET:
      base_stream = mongoc_client_connect_tcp (uri, host, error);
      break;
   case AF_UNIX:
      base_stream = mongoc_client_connect_unix (uri, host, error);
      break;
   default:
      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_INVALID_TYPE,
                      "Invalid address family: 0x%02x", host->family);
      break;
   }

#ifdef MONGOC_ENABLE_SSL
   if (base_stream) {
      options = mongoc_uri_get_options (uri);
      mechanism = mongoc_uri_get_auth_mechanism (uri);

      if ((bson_iter_init_find_case (&iter, options, "ssl") &&
           bson_iter_as_bool (&iter)) ||
          (mechanism && (0 == strcmp (mechanism, "MONGODB-X509")))) {
         base_stream = mongoc_stream_tls_new (base_stream, &client->ssl_opts,
                                              true);

         if (!base_stream) {
            bson_set_error (error,
                            MONGOC_ERROR_STREAM,
                            MONGOC_ERROR_STREAM_SOCKET,
                            "Failed initialize TLS state.");
            return NULL;
         }

         if (!mongoc_stream_tls_do_handshake (base_stream, -1) ||
             !mongoc_stream_tls_check_cert (base_stream, host->host)) {
            bson_set_error (error,
                            MONGOC_ERROR_STREAM,
                            MONGOC_ERROR_STREAM_SOCKET,
                            "Failed to handshake and validate TLS certificate.");
            mongoc_stream_destroy (base_stream);
            base_stream = NULL;
            return NULL;
         }
      }
   }
#endif

   return base_stream ? mongoc_stream_buffered_new (base_stream, 1024) : NULL;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_client_create_stream --
 *
 *       INTERNAL API
 *
 *       This function is used by the mongoc_cluster_t to initiate a
 *       new stream. This is done because cluster is private API and
 *       those using mongoc_client_t may need to override this process.
 *
 *       This function calls the default initiator for new streams.
 *
 * Returns:
 *       A newly allocated mongoc_stream_t if successful; otherwise
 *       NULL and @error is set.
 *
 * Side effects:
 *       @error is set if return value is NULL.
 *
 *--------------------------------------------------------------------------
 */

mongoc_stream_t *
_mongoc_client_create_stream (mongoc_client_t          *client,
                              const mongoc_host_list_t *host,
                              bson_error_t             *error)
{
   bson_return_val_if_fail(client, NULL);
   bson_return_val_if_fail(host, NULL);

   return client->initiator (client->uri, host, client->initiator_data, error);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_client_sendv --
 *
 *       INTERNAL API
 *
 *       This function is used to deliver one or more RPCs to the remote
 *       MongoDB server.
 *
 *       Based on the cluster state and operation type, the request may
 *       be retried. This is handled by the cluster instance.
 *
 * Returns:
 *       0 upon failure and @error is set. Otherwise non-zero indicating
 *       the cluster node that performed the request.
 *
 * Side effects:
 *       @error is set if return value is 0.
 *       @rpcs is mutated and therefore invalid after calling.
 *
 *--------------------------------------------------------------------------
 */

uint32_t
_mongoc_client_sendv (mongoc_client_t              *client,
                      mongoc_rpc_t                 *rpcs,
                      size_t                        rpcs_len,
                      uint32_t                      hint,
                      const mongoc_write_concern_t *write_concern,
                      const mongoc_read_prefs_t    *read_prefs,
                      bson_error_t                 *error)
{
   size_t i;

   bson_return_val_if_fail(client, false);
   bson_return_val_if_fail(rpcs, false);
   bson_return_val_if_fail(rpcs_len, false);

   if (client->in_exhaust) {
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_IN_EXHAUST,
                     "A cursor derived from this client is in exhaust.");
      RETURN(false);
   }

   for (i = 0; i < rpcs_len; i++) {
      rpcs[i].header.msg_len = 0;
      rpcs[i].header.request_id = ++client->request_id;
   }

   switch (client->cluster.state) {
   case MONGOC_CLUSTER_STATE_BORN:
      return _mongoc_cluster_sendv(&client->cluster, rpcs, rpcs_len, hint,
                                   write_concern, read_prefs, error);
   case MONGOC_CLUSTER_STATE_HEALTHY:
   case MONGOC_CLUSTER_STATE_UNHEALTHY:
      return _mongoc_cluster_try_sendv(&client->cluster, rpcs, rpcs_len, hint,
                                       write_concern, read_prefs, error);
   case MONGOC_CLUSTER_STATE_DEAD:
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_NOT_READY,
                     "No healthy connections.");
      return false;
   default:
      BSON_ASSERT(false);
      return 0;
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_client_recv --
 *
 *       Receives a RPC from a remote MongoDB cluster node. @hint should
 *       be the result from a previous call to mongoc_client_sendv() to
 *       signify which node to recv from.
 *
 * Returns:
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       @error is set if return value is false.
 *
 *--------------------------------------------------------------------------
 */

bool
_mongoc_client_recv (mongoc_client_t *client,
                     mongoc_rpc_t    *rpc,
                     mongoc_buffer_t *buffer,
                     uint32_t         hint,
                     bson_error_t    *error)
{
   bson_return_val_if_fail(client, false);
   bson_return_val_if_fail(rpc, false);
   bson_return_val_if_fail(buffer, false);
   bson_return_val_if_fail(hint, false);
   bson_return_val_if_fail(hint <= MONGOC_CLUSTER_MAX_NODES, false);

   return _mongoc_cluster_try_recv (&client->cluster, rpc, buffer, hint,
                                    error);
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_to_error --
 *
 *       A helper routine to convert a bson document to a bson_error_t.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @error is set if non-null.
 *
 *--------------------------------------------------------------------------
 */

static void
_bson_to_error (const bson_t *b,
                bson_error_t *error)
{
   bson_iter_t iter;
   int code = 0;

   BSON_ASSERT(b);

   if (!error) {
      return;
   }

   if (bson_iter_init_find(&iter, b, "code") && BSON_ITER_HOLDS_INT32(&iter)) {
      code = bson_iter_int32(&iter);
   }

   if (bson_iter_init_find(&iter, b, "$err") && BSON_ITER_HOLDS_UTF8(&iter)) {
      bson_set_error(error,
                     MONGOC_ERROR_QUERY,
                     code,
                     "%s",
                     bson_iter_utf8(&iter, NULL));
      return;
   }

   if (bson_iter_init_find(&iter, b, "errmsg") && BSON_ITER_HOLDS_UTF8(&iter)) {
      bson_set_error(error,
                     MONGOC_ERROR_QUERY,
                     code,
                     "%s",
                     bson_iter_utf8(&iter, NULL));
      return;
   }

   bson_set_error(error,
                  MONGOC_ERROR_QUERY,
                  MONGOC_ERROR_QUERY_FAILURE,
                  "An unknown error ocurred on the server.");
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_recv_gle --
 *
 *       INTERNAL API
 *
 *       This function is used to receive the next RPC from a cluster
 *       node, expecting it to be the response to a getlasterror command.
 *
 *       The RPC is parsed into @error if it is an error and false is
 *       returned.
 *
 *       If the operation was successful, true is returned.
 *
 *       if @gle_doc is not NULL, then the actual response document for
 *       the gle command will be stored as an out parameter. The caller
 *       is responsible for freeing it in this case.
 *
 * Returns:
 *       true if getlasterror was success; otherwise false and @error
 *       is set.
 *
 * Side effects:
 *       @error if return value is false.
 *       @gle_doc will be set if non NULL and a reply was received.
 *
 *--------------------------------------------------------------------------
 */

bool
_mongoc_client_recv_gle (mongoc_client_t  *client,
                         uint32_t          hint,
                         bson_t          **gle_doc,
                         bson_error_t     *error)
{
   mongoc_buffer_t buffer;
   mongoc_rpc_t rpc;
   bson_iter_t iter;
   bool ret = false;
   bson_t b;

   ENTRY;

   bson_return_val_if_fail (client, false);
   bson_return_val_if_fail (hint, false);

   if (gle_doc) {
      *gle_doc = NULL;
   }

   _mongoc_buffer_init (&buffer, NULL, 0, NULL, NULL);

   if (!_mongoc_cluster_try_recv (&client->cluster, &rpc, &buffer,
                                  hint, error)) {
      GOTO (cleanup);
   }

   if (rpc.header.opcode != MONGOC_OPCODE_REPLY) {
      bson_set_error (error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Received message other than OP_REPLY.");
      GOTO (cleanup);
   }

   if (_mongoc_rpc_reply_get_first (&rpc.reply, &b)) {
      if (gle_doc) {
         *gle_doc = bson_copy (&b);
      }

      if ((rpc.reply.flags & MONGOC_REPLY_QUERY_FAILURE)) {
         _bson_to_error (&b, error);
         bson_destroy (&b);
         GOTO (cleanup);
      }

      if (!bson_iter_init_find (&iter, &b, "ok") ||
          BSON_ITER_HOLDS_DOUBLE (&iter)) {
        if (bson_iter_double (&iter) == 0.0) {
          _bson_to_error (&b, error);
        }
      }

      bson_destroy (&b);
   }

   ret = true;

cleanup:
   _mongoc_buffer_destroy (&buffer);

   RETURN (ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_new --
 *
 *       Create a new mongoc_client_t using the URI provided.
 *
 *       @uri should be a MongoDB URI string such as "mongodb://localhost/"
 *       More information on the format can be found at
 *       http://docs.mongodb.org/manual/reference/connection-string/
 *
 * Returns:
 *       A newly allocated mongoc_client_t or NULL if @uri_string is
 *       invalid.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_client_t *
mongoc_client_new (const char *uri_string)
{
   const mongoc_write_concern_t *write_concern;
   mongoc_client_t *client;
   const bson_t *read_prefs_tags;
   mongoc_uri_t *uri;
   const bson_t *options;
   bson_iter_t iter;
   bool has_ssl = false;
   bool slave_okay = false;

   if (!uri_string) {
      uri_string = "mongodb://127.0.0.1/";
   }

   if (!(uri = mongoc_uri_new(uri_string))) {
      return NULL;
   }

   options = mongoc_uri_get_options (uri);

   if (bson_iter_init_find (&iter, options, "ssl") &&
       BSON_ITER_HOLDS_BOOL (&iter) &&
       bson_iter_bool (&iter)) {
      has_ssl = true;
   }

   if (bson_iter_init_find_case (&iter, options, "slaveok") &&
       BSON_ITER_HOLDS_BOOL (&iter) &&
       bson_iter_bool (&iter)) {
      slave_okay = true;
   }

   client = bson_malloc0(sizeof *client);
   client->uri = uri;
   client->request_id = rand ();
   client->initiator = mongoc_client_default_stream_initiator;
   client->initiator_data = client;

   write_concern = mongoc_uri_get_write_concern (uri);
   client->write_concern = mongoc_write_concern_copy (write_concern);

   if (slave_okay) {
      client->read_prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY_PREFERRED);
   } else {
      client->read_prefs = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);
   }

   read_prefs_tags = mongoc_uri_get_read_prefs (client->uri);
   if (!bson_empty (read_prefs_tags)) {
      mongoc_read_prefs_set_tags (client->read_prefs, read_prefs_tags);
   }

   _mongoc_cluster_init (&client->cluster, client->uri, client);

#ifdef MONGOC_ENABLE_SSL
   if (has_ssl) {
      mongoc_client_set_ssl_opts (client, mongoc_ssl_opt_get_default ());
   }
#endif

   mongoc_counter_clients_active_inc ();

   return client;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_set_ssl_opts
 *
 *       set ssl opts for a client
 *
 * Returns:
 *       Nothing
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

#ifdef MONGOC_ENABLE_SSL
void
mongoc_client_set_ssl_opts (mongoc_client_t        *client,
                            const mongoc_ssl_opt_t *opts)
{

   BSON_ASSERT (client);
   BSON_ASSERT (opts);

   memcpy (&client->ssl_opts, opts, sizeof client->ssl_opts);

   bson_free (client->pem_subject);
   client->pem_subject = NULL;

   if (opts->pem_file) {
      client->pem_subject = _mongoc_ssl_extract_subject (opts->pem_file);
   }
}
#endif


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_new_from_uri --
 *
 *       Create a new mongoc_client_t for a mongoc_uri_t.
 *
 * Returns:
 *       A newly allocated mongoc_client_t.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_client_t *
mongoc_client_new_from_uri (const mongoc_uri_t *uri)
{
   const char *uristr;

   bson_return_val_if_fail(uri, NULL);

   uristr = mongoc_uri_get_string(uri);
   return mongoc_client_new(uristr);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_destroy --
 *
 *       Destroys a mongoc_client_t and cleans up all resources associated
 *       with the client instance.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @client is destroyed.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_client_destroy (mongoc_client_t *client)
{
   if (client) {
#ifdef MONGOC_ENABLE_SSL
      bson_free (client->pem_subject);
#endif

      mongoc_write_concern_destroy (client->write_concern);
      mongoc_read_prefs_destroy (client->read_prefs);
      _mongoc_cluster_destroy (&client->cluster);
      mongoc_uri_destroy (client->uri);
      bson_free (client);

      mongoc_counter_clients_active_dec ();
      mongoc_counter_clients_disposed_inc ();
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_get_uri --
 *
 *       Fetch the URI used for @client.
 *
 * Returns:
 *       A mongoc_uri_t that should not be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const mongoc_uri_t *
mongoc_client_get_uri (const mongoc_client_t *client)
{
   bson_return_val_if_fail(client, NULL);

   return client->uri;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_stamp --
 *
 *       INTERNAL API
 *
 *       Fetch the stamp for @node within @client. This is used to track
 *       if there have been changes or disconnects from a node between
 *       the last operation.
 *
 * Returns:
 *       A 32-bit monotonic stamp.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

uint32_t
_mongoc_client_stamp (mongoc_client_t *client,
                      uint32_t    node)
{
   bson_return_val_if_fail (client, 0);

   return _mongoc_cluster_stamp (&client->cluster, node);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_get_database --
 *
 *       Fetches a newly allocated database structure to communicate with
 *       a database over @client.
 *
 *       @database should be a db name such as "test".
 *
 *       This structure should be freed when the caller is done with it
 *       using mongoc_database_destroy().
 *
 * Returns:
 *       A newly allocated mongoc_database_t.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_database_t *
mongoc_client_get_database (mongoc_client_t *client,
                            const char      *name)
{
   bson_return_val_if_fail(client, NULL);
   bson_return_val_if_fail(name, NULL);

   return _mongoc_database_new(client, name, client->read_prefs, client->write_concern);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_get_collection --
 *
 *       This function returns a newly allocated collection structure.
 *
 *       @db should be the name of the database, such as "test".
 *       @collection should be the name of the collection such as "test".
 *
 *       The above would result in the namespace "test.test".
 *
 *       You should free this structure when you are done with it using
 *       mongoc_collection_destroy().
 *
 * Returns:
 *       A newly allocated mongoc_collection_t that should be freed with
 *       mongoc_collection_destroy().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_collection_t *
mongoc_client_get_collection (mongoc_client_t *client,
                              const char      *db,
                              const char      *collection)
{
   bson_return_val_if_fail(client, NULL);
   bson_return_val_if_fail(db, NULL);
   bson_return_val_if_fail(collection, NULL);

   return _mongoc_collection_new(client, db, collection, client->read_prefs,
                                 client->write_concern);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_get_gridfs --
 *
 *       This function returns a newly allocated collection structure.
 *
 *       @db should be the name of the database, such as "test".
 *       @collection should be the name of the collection such as "test".
 *
 *       The above would result in the namespace "test.test".
 *
 *       You should free this structure when you are done with it using
 *       mongoc_collection_destroy().
 *
 * Returns:
 *       A newly allocated mongoc_collection_t that should be freed with
 *       mongoc_collection_destroy().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_gridfs_t *
mongoc_client_get_gridfs (mongoc_client_t *client,
                          const char      *db,
                          const char      *prefix,
                          bson_error_t    *error)
{
   bson_return_val_if_fail(client, NULL);
   bson_return_val_if_fail(db, NULL);

   if (! prefix) {
      prefix = "fs";
   }

   return _mongoc_gridfs_new(client, db, prefix, error);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_get_write_concern --
 *
 *       Fetches the default write concern for @client.
 *
 * Returns:
 *       A mongoc_write_concern_t that should not be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const mongoc_write_concern_t *
mongoc_client_get_write_concern (const mongoc_client_t *client)
{
   bson_return_val_if_fail(client, NULL);

   return client->write_concern;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_set_write_concern --
 *
 *       Sets the default write concern for @client.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_client_set_write_concern (mongoc_client_t              *client,
                                 const mongoc_write_concern_t *write_concern)
{
   bson_return_if_fail(client);

   if (write_concern != client->write_concern) {
      if (client->write_concern) {
         mongoc_write_concern_destroy(client->write_concern);
      }
      client->write_concern = write_concern ?
         mongoc_write_concern_copy(write_concern) :
         mongoc_write_concern_new();
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_get_read_prefs --
 *
 *       Fetch the default read preferences for @client.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const mongoc_read_prefs_t *
mongoc_client_get_read_prefs (const mongoc_client_t *client)
{
   bson_return_val_if_fail (client, NULL);

   return client->read_prefs;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_set_read_prefs --
 *
 *       Set the default read preferences for @client.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_client_set_read_prefs (mongoc_client_t           *client,
                              const mongoc_read_prefs_t *read_prefs)
{
   bson_return_if_fail (client);

   if (read_prefs != client->read_prefs) {
      if (client->read_prefs) {
         mongoc_read_prefs_destroy(client->read_prefs);
      }
      client->read_prefs = read_prefs ?
         mongoc_read_prefs_copy(read_prefs) :
         mongoc_read_prefs_new(MONGOC_READ_PRIMARY);
   }
}


bool
_mongoc_client_warm_up (mongoc_client_t *client,
                        bson_error_t    *error)
{
   bool ret = true;
   bson_t cmd;

   BSON_ASSERT (client);

   if (client->cluster.state == MONGOC_CLUSTER_STATE_BORN) {
      bson_init (&cmd);
      bson_append_int32 (&cmd, "ping", 4, 1);
      ret = _mongoc_cluster_command_early (&client->cluster, "admin", &cmd,
                                           NULL, error);
      bson_destroy (&cmd);
   } else if (client->cluster.state == MONGOC_CLUSTER_STATE_DEAD) {
      ret = _mongoc_cluster_reconnect(&client->cluster, error);
   }

   return ret;
}


uint32_t
_mongoc_client_preselect (mongoc_client_t              *client,        /* IN */
                          mongoc_opcode_t               opcode,        /* IN */
                          const mongoc_write_concern_t *write_concern, /* IN */
                          const mongoc_read_prefs_t    *read_prefs,    /* IN */
                          bson_error_t                 *error)         /* OUT */
{

   BSON_ASSERT (client);

   return _mongoc_cluster_preselect (&client->cluster, opcode,
                                     write_concern, read_prefs, error);
}


mongoc_cursor_t *
mongoc_client_command (mongoc_client_t           *client,
                       const char                *db_name,
                       mongoc_query_flags_t       flags,
                       uint32_t                   skip,
                       uint32_t                   limit,
                       uint32_t                   batch_size,
                       const bson_t              *query,
                       const bson_t              *fields,
                       const mongoc_read_prefs_t *read_prefs)
{
   char ns[MONGOC_NAMESPACE_MAX];

   BSON_ASSERT (client);
   BSON_ASSERT (db_name);
   BSON_ASSERT (query);

   if (!read_prefs) {
      read_prefs = client->read_prefs;
   }

   /*
    * Allow a caller to provide a fully qualified namespace. Otherwise,
    * querying something like "$cmd.sys.inprog" is not possible.
    */
   if (NULL == strstr (db_name, "$cmd")) {
      bson_snprintf (ns, sizeof ns, "%s.$cmd", db_name);
      db_name = ns;
   }

   return _mongoc_cursor_new (client, db_name, flags, skip, limit, batch_size,
                              true, query, fields, read_prefs);
}


/**
 * mongoc_client_command_simple:
 * @client: A mongoc_client_t.
 * @db_name: The namespace, such as "admin".
 * @command: The command to execute.
 * @read_prefs: The read preferences or NULL.
 * @reply: A location for the reply document or NULL.
 * @error: A location for the error, or NULL.
 *
 * This wrapper around mongoc_client_command() aims to make it simpler to
 * run a command and check the output result.
 *
 * false is returned if the command failed to be delivered or if the execution
 * of the command failed. For example, a command that returns {'ok': 0} will
 * result in this function returning false.
 *
 * To allow the caller to disambiguate between command execution failure and
 * failure to send the command, reply will always be set if non-NULL. The
 * caller should release this with bson_destroy().
 *
 * Returns: true if the command executed and resulted in success. Otherwise
 *   false and @error is set. @reply is always set, either to the resulting
 *   document or an empty bson document upon failure.
 */
bool
mongoc_client_command_simple (mongoc_client_t           *client,
                              const char                *db_name,
                              const bson_t              *command,
                              const mongoc_read_prefs_t *read_prefs,
                              bson_t                    *reply,
                              bson_error_t              *error)
{
   mongoc_cursor_t *cursor;
   const bson_t *doc;
   bool ret;

   BSON_ASSERT (client);
   BSON_ASSERT (db_name);
   BSON_ASSERT (command);

   cursor = mongoc_client_command (client, db_name, MONGOC_QUERY_NONE, 0, 1, 0,
                                   command, NULL, read_prefs);

   ret = mongoc_cursor_next (cursor, &doc);

   if (reply) {
      if (ret) {
         bson_copy_to (doc, reply);
      } else {
         bson_init (reply);
      }
   }

   if (!ret) {
      mongoc_cursor_error (cursor, error);
   }

   mongoc_cursor_destroy (cursor);

   return ret;
}


char **
mongoc_client_get_database_names (mongoc_client_t *client,
                                  bson_error_t    *error)
{
   bson_iter_t iter;
   bson_iter_t child;
   bson_iter_t child2;
   const char *name;
   bson_t cmd = BSON_INITIALIZER;
   bson_t reply;
   char **ret = NULL;
   int i = 0;

   BSON_ASSERT (client);

   BSON_APPEND_INT32 (&cmd, "listDatabases", 1);

   if (!mongoc_client_command_simple (client, "admin", &cmd, NULL,
                                      &reply, error)) {
      bson_destroy (&cmd);
      return NULL;
   }

   if (bson_iter_init_find (&iter, &reply, "databases") &&
       BSON_ITER_HOLDS_ARRAY (&iter) &&
       bson_iter_recurse (&iter, &child)) {
      while (bson_iter_next (&child)) {
         if (BSON_ITER_HOLDS_DOCUMENT (&child) &&
             bson_iter_recurse (&child, &child2) &&
             bson_iter_find (&child2, "name") &&
             BSON_ITER_HOLDS_UTF8 (&child2) &&
             (name = bson_iter_utf8 (&child2, NULL)) &&
             (0 != strcmp (name, "local"))) {
            ret = bson_realloc (ret, sizeof(char*) * (i + 2));
            ret [i] = bson_strdup (name);
            ret [++i] = NULL;
         }
      }
   }

   if (!ret) {
      ret = bson_malloc0 (sizeof (void*));
   }

   bson_destroy (&cmd);
   bson_destroy (&reply);

   return ret;
}


int32_t
mongoc_client_get_max_message_size (mongoc_client_t *client) /* IN */
{
   bson_return_val_if_fail (client, -1);

   return client->cluster.max_msg_size;
}


int32_t
mongoc_client_get_max_bson_size (mongoc_client_t *client) /* IN */
{
   bson_return_val_if_fail (client, -1);

   return client->cluster.max_bson_size;
}


bool
mongoc_client_get_server_status (mongoc_client_t     *client,     /* IN */
                                 mongoc_read_prefs_t *read_prefs, /* IN */
                                 bson_t              *reply,      /* OUT */
                                 bson_error_t        *error)      /* OUT */
{
   bson_t cmd = BSON_INITIALIZER;
   bool ret = false;

   bson_return_val_if_fail (client, false);

   BSON_APPEND_INT32 (&cmd, "serverStatus", 1);
   ret = mongoc_client_command_simple (client, "admin", &cmd, read_prefs,
                                       reply, error);
   bson_destroy (&cmd);

   return ret;
}


void
mongoc_client_set_stream_initiator (mongoc_client_t           *client,
                                    mongoc_stream_initiator_t  initiator,
                                    void                      *user_data)
{
   bson_return_if_fail (client);

   if (!initiator) {
      initiator = mongoc_client_default_stream_initiator;
      user_data = client;
   } else {
      MONGOC_DEBUG ("Using custom stream initiator.");
   }

   client->initiator = initiator;
   client->initiator_data = user_data;
}

/*==============================================================*/
/* --- mongoc-client-pool.c */

struct _mongoc_client_pool_t
{
   mongoc_mutex_t    mutex;
   mongoc_cond_t     cond;
   mongoc_queue_t    queue;
   mongoc_uri_t     *uri;
   uint32_t          min_pool_size;
   uint32_t          max_pool_size;
   uint32_t          size;
#ifdef MONGOC_ENABLE_SSL
   bool              ssl_opts_set;
   mongoc_ssl_opt_t  ssl_opts;
#endif
};


#ifdef MONGOC_ENABLE_SSL
void
mongoc_client_pool_set_ssl_opts (mongoc_client_pool_t   *pool,
                                 const mongoc_ssl_opt_t *opts)
{
   bson_return_if_fail (pool);

   mongoc_mutex_lock (&pool->mutex);

   memset (&pool->ssl_opts, 0, sizeof pool->ssl_opts);
   pool->ssl_opts_set = false;

   if (opts) {
      memcpy (&pool->ssl_opts, opts, sizeof pool->ssl_opts);
      pool->ssl_opts_set = true;

   }

   mongoc_mutex_unlock (&pool->mutex);
}
#endif


mongoc_client_pool_t *
mongoc_client_pool_new (const mongoc_uri_t *uri)
{
   mongoc_client_pool_t *pool;
   const bson_t *b;
   bson_iter_t iter;

   ENTRY;

   bson_return_val_if_fail(uri, NULL);

   pool = bson_malloc0(sizeof *pool);
   mongoc_mutex_init(&pool->mutex);
   _mongoc_queue_init(&pool->queue);
   pool->uri = mongoc_uri_copy(uri);
   pool->min_pool_size = 0;
   pool->max_pool_size = 100;
   pool->size = 0;

   b = mongoc_uri_get_options(pool->uri);

   if (bson_iter_init_find_case(&iter, b, "minpoolsize")) {
      if (BSON_ITER_HOLDS_INT32(&iter)) {
         pool->min_pool_size = MAX(0, bson_iter_int32(&iter));
      }
   }

   if (bson_iter_init_find_case(&iter, b, "maxpoolsize")) {
      if (BSON_ITER_HOLDS_INT32(&iter)) {
         pool->max_pool_size = MAX(1, bson_iter_int32(&iter));
      }
   }

   mongoc_counter_client_pools_active_inc();

   RETURN(pool);
}


void
mongoc_client_pool_destroy (mongoc_client_pool_t *pool)
{
   mongoc_client_t *client;

   ENTRY;

   bson_return_if_fail(pool);

   while ((client = _mongoc_queue_pop_head(&pool->queue))) {
      mongoc_client_destroy(client);
   }

   mongoc_uri_destroy(pool->uri);
   mongoc_mutex_destroy(&pool->mutex);
   mongoc_cond_destroy(&pool->cond);
   bson_free(pool);

   mongoc_counter_client_pools_active_dec();
   mongoc_counter_client_pools_disposed_inc();

   EXIT;
}


mongoc_client_t *
mongoc_client_pool_pop (mongoc_client_pool_t *pool)
{
   mongoc_client_t *client;

   ENTRY;

   bson_return_val_if_fail(pool, NULL);

   mongoc_mutex_lock(&pool->mutex);

again:
   if (!(client = _mongoc_queue_pop_head(&pool->queue))) {
      if (pool->size < pool->max_pool_size) {
         client = mongoc_client_new_from_uri(pool->uri);
#ifdef MONGOC_ENABLE_SSL
         if (pool->ssl_opts_set) {
            mongoc_client_set_ssl_opts (client, &pool->ssl_opts);
         }
#endif
         pool->size++;
      } else {
         mongoc_cond_wait(&pool->cond, &pool->mutex);
         GOTO(again);
      }
   }

   mongoc_mutex_unlock(&pool->mutex);

   RETURN(client);
}


mongoc_client_t *
mongoc_client_pool_try_pop (mongoc_client_pool_t *pool)
{
   mongoc_client_t *client;

   ENTRY;

   bson_return_val_if_fail(pool, NULL);

   mongoc_mutex_lock(&pool->mutex);

   if (!(client = _mongoc_queue_pop_head(&pool->queue))) {
      if (pool->size < pool->max_pool_size) {
         client = mongoc_client_new_from_uri(pool->uri);
#ifdef MONGOC_ENABLE_SSL
         if (pool->ssl_opts_set) {
            mongoc_client_set_ssl_opts (client, &pool->ssl_opts);
         }
#endif
         pool->size++;
      }
   }

   mongoc_mutex_unlock(&pool->mutex);

   RETURN(client);
}


void
mongoc_client_pool_push (mongoc_client_pool_t *pool,
                         mongoc_client_t      *client)
{
   ENTRY;

   bson_return_if_fail(pool);
   bson_return_if_fail(client);

   /*
    * TODO: Shutdown old client connections.
    */

   /*
    * TODO: We should try to make a client healthy again if it
    *       is unhealthy since this is typically where a thread
    *       is done with its work.
    */

   mongoc_mutex_lock(&pool->mutex);
   _mongoc_queue_push_head(&pool->queue, client);
   mongoc_cond_signal(&pool->cond);
   mongoc_mutex_unlock(&pool->mutex);

   EXIT;
}

/*==============================================================*/
/* --- mongoc-cluster.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "cluster"


#ifdef _WIN32
# define strcasecmp _stricmp
#endif

#ifndef MAX_RETRY_COUNT
#define MAX_RETRY_COUNT 3
#endif


#define MIN_WIRE_VERSION 0
#define MAX_WIRE_VERSION 2


#ifndef DEFAULT_SOCKET_TIMEOUT_MSEC
/*
 * NOTE: The default socket timeout for connections is 5 minutes. This
 *       means that if your MongoDB server dies or becomes unavailable
 *       it will take 5 minutes to detect this.
 *
 *       You can change this by providing sockettimeoutms= in your
 *       connection URI.
 */
#define DEFAULT_SOCKET_TIMEOUT_MSEC (1000L * 60L * 5L)
#endif


#ifndef UNHEALTHY_RECONNECT_TIMEOUT_USEC
/*
 * Try reconnect every 20 seconds if we are unhealthy.
 */
#define UNHEALTHY_RECONNECT_TIMEOUT_USEC (1000L * 1000L * 20L)
#endif


#define DB_AND_CMD_FROM_COLLECTION(outstr, name) \
   do { \
      const char *dot = strchr(name, '.'); \
      if (!dot || ((dot - name) > (int)(sizeof outstr - 6))) { \
         bson_snprintf(outstr, sizeof outstr, "admin.$cmd"); \
      } else { \
         memcpy(outstr, name, dot - name); \
         memcpy(outstr + (dot - name), ".$cmd", 6); \
      } \
   } while (0)


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_update_state --
 *
 *       Check the all peer nodes to update the cluster state.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static void
_mongoc_cluster_update_state (mongoc_cluster_t *cluster)
{
   mongoc_cluster_state_t state;
   mongoc_cluster_node_t *node;
   int up_nodes = 0;
   int down_nodes = 0;
   int i;

   ENTRY;

   BSON_ASSERT(cluster);

   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      node = &cluster->nodes[i];
      if (node->stamp && !node->stream) {
         down_nodes++;
      } else if (node->stream) {
         up_nodes++;
      }
   }

   if (!up_nodes && !down_nodes) {
      state = MONGOC_CLUSTER_STATE_BORN;
   } else if (!up_nodes && down_nodes) {
      state = MONGOC_CLUSTER_STATE_DEAD;
   } else if (up_nodes && !down_nodes) {
      state = MONGOC_CLUSTER_STATE_HEALTHY;
   } else {
      BSON_ASSERT(up_nodes);
      BSON_ASSERT(down_nodes);
      state = MONGOC_CLUSTER_STATE_UNHEALTHY;
   }

   cluster->state = state;

   EXIT;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_add_peer --
 *
 *       Adds a peer to the list of peers that should be potentially
 *       connected to as part of a replicaSet.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static void
_mongoc_cluster_add_peer (mongoc_cluster_t *cluster,
                          const char       *peer)
{
   mongoc_list_t *iter;

   ENTRY;

   BSON_ASSERT(cluster);
   BSON_ASSERT(peer);

   MONGOC_DEBUG("Registering potential peer: %s", peer);

   for (iter = cluster->peers; iter; iter = iter->next) {
      if (!strcmp(iter->data, peer)) {
         EXIT;
      }
   }

   cluster->peers = _mongoc_list_prepend(cluster->peers, bson_strdup(peer));

   EXIT;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_clear_peers --
 *
 *       Clears list of cached potential peers that we've seen in the
 *       "hosts" field of replicaSet nodes.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static void
_mongoc_cluster_clear_peers (mongoc_cluster_t *cluster)
{
   ENTRY;

   BSON_ASSERT(cluster);

   _mongoc_list_foreach(cluster->peers, (void(*)(void*,void*))bson_free, NULL);
   _mongoc_list_destroy(cluster->peers);
   cluster->peers = NULL;

   EXIT;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_node_init --
 *
 *       Initialize a mongoc_cluster_node_t.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static void
_mongoc_cluster_node_init (mongoc_cluster_node_t *node)
{
   ENTRY;

   BSON_ASSERT(node);

   memset(node, 0, sizeof *node);

   node->index = 0;
   node->ping_avg_msec = -1;
   memset(node->pings, 0xFF, sizeof node->pings);
   node->pings_pos = 0;
   node->stamp = 0;
   bson_init(&node->tags);
   node->primary = 0;
   node->needs_auth = 0;

   EXIT;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_node_track_ping --
 *
 *       Add the ping time to the mongoc_cluster_node_t.
 *       Increment the position in the ring buffer and update the
 *       rolling average.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static void
_mongoc_cluster_node_track_ping (mongoc_cluster_node_t *node,
                                 int32_t           ping)
{
   int total = 0;
   int count = 0;
   int i;

   BSON_ASSERT(node);

   node->pings[node->pings_pos] = ping;
   node->pings_pos = (node->pings_pos + 1) % MONGOC_CLUSTER_PING_NUM_SAMPLES;

   for (i = 0; i < MONGOC_CLUSTER_PING_NUM_SAMPLES; i++) {
      if (node->pings[i] != -1) {
         total += node->pings[i];
         count++;
      }
   }

   node->ping_avg_msec = count ? (int)((double)total / (double)count) : -1;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_node_destroy --
 *
 *       Destroy allocated resources within @node.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static void
_mongoc_cluster_node_destroy (mongoc_cluster_node_t *node)
{
   ENTRY;

   BSON_ASSERT(node);

   if (node->stream) {
      mongoc_stream_close(node->stream);
      mongoc_stream_destroy(node->stream);
      node->stream = NULL;
   }

   if (node->tags.len) {
      bson_destroy (&node->tags);
      memset (&node->tags, 0, sizeof node->tags);
   }

   bson_free (node->replSet);
   node->replSet = NULL;

   EXIT;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_build_basic_auth_digest --
 *
 *       Computes the Basic Authentication digest using the credentials
 *       configured for @cluster and the @nonce provided.
 *
 *       The result should be freed by the caller using bson_free() when
 *       they are finished with it.
 *
 * Returns:
 *       A newly allocated string containing the digest.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static char *
_mongoc_cluster_build_basic_auth_digest (mongoc_cluster_t *cluster,
                                         const char       *nonce)
{
   const char *username;
   const char *password;
   char *password_digest;
   char *password_md5;
   char *digest_in;
   char *ret;

   ENTRY;

   /*
    * The following generates the digest to be used for basic authentication
    * with a MongoDB server. More information on the format can be found
    * at the following location:
    *
    * http://docs.mongodb.org/meta-driver/latest/legacy/
    *   implement-authentication-in-driver/
    */

   BSON_ASSERT(cluster);
   BSON_ASSERT(cluster->uri);

   username = mongoc_uri_get_username(cluster->uri);
   password = mongoc_uri_get_password(cluster->uri);
   password_digest = bson_strdup_printf("%s:mongo:%s", username, password);
   password_md5 = _mongoc_hex_md5(password_digest);
   digest_in = bson_strdup_printf("%s%s%s", nonce, username, password_md5);
   ret = _mongoc_hex_md5(digest_in);
   bson_free(digest_in);
   bson_free(password_md5);
   bson_free(password_digest);

   RETURN(ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_disconnect_node --
 *
 *       Disconnects a cluster node and reinitializes it so it may be
 *       connected to again in the future.
 *
 *       The stream is closed and destroyed.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
_mongoc_cluster_disconnect_node (mongoc_cluster_t      *cluster,
                                 mongoc_cluster_node_t *node)
{
   ENTRY;

   bson_return_if_fail(node);

   if (node->stream) {
      mongoc_stream_close(node->stream);
      mongoc_stream_destroy(node->stream);
      node->stream = NULL;
   }

   node->needs_auth = cluster->requires_auth;
   node->ping_avg_msec = -1;
   memset(node->pings, 0xFF, sizeof node->pings);
   node->pings_pos = 0;
   node->stamp++;
   node->primary = 0;

   bson_destroy (&node->tags);
   bson_init (&node->tags);

   _mongoc_cluster_update_state (cluster);

   EXIT;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_init --
 *
 *       Initializes @cluster using the @uri and @client provided. The
 *       @uri is used to determine the "mode" of the cluster. Based on the
 *       uri we can determine if we are connected to a single host, a
 *       replicaSet, or a shardedCluster.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @cluster is initialized.
 *
 *--------------------------------------------------------------------------
 */

void
_mongoc_cluster_init (mongoc_cluster_t   *cluster,
                      const mongoc_uri_t *uri,
                      void               *client)
{
   const mongoc_host_list_t *hosts;
   uint32_t sockettimeoutms = DEFAULT_SOCKET_TIMEOUT_MSEC;
   uint32_t i;
   const bson_t *b;
   bson_iter_t iter;

   ENTRY;

   bson_return_if_fail (cluster);
   bson_return_if_fail (uri);

   memset (cluster, 0, sizeof *cluster);

   b = mongoc_uri_get_options(uri);
   hosts = mongoc_uri_get_hosts(uri);

   if (bson_iter_init_find_case (&iter, b, "replicaSet")) {
      cluster->mode = MONGOC_CLUSTER_REPLICA_SET;
      cluster->replSet = bson_iter_dup_utf8 (&iter, NULL);
      MONGOC_DEBUG ("Client initialized in replica set mode.");
   } else if (hosts->next) {
      cluster->mode = MONGOC_CLUSTER_SHARDED_CLUSTER;
      MONGOC_DEBUG ("Client initialized in sharded cluster mode.");
   } else {
      cluster->mode = MONGOC_CLUSTER_DIRECT;
      MONGOC_DEBUG ("Client initialized in direct mode.");
   }

   if (bson_iter_init_find_case(&iter, b, "sockettimeoutms")) {
      if (!(sockettimeoutms = bson_iter_int32 (&iter))) {
         sockettimeoutms = DEFAULT_SOCKET_TIMEOUT_MSEC;
      }
   }

   cluster->uri = mongoc_uri_copy(uri);
   cluster->client = client;
   cluster->sec_latency_ms = 15;
   cluster->max_msg_size = 1024 * 1024 * 48;
   cluster->max_bson_size = 1024 * 1024 * 16;
   cluster->requires_auth = (mongoc_uri_get_username (uri) ||
                             mongoc_uri_get_auth_mechanism (uri));
   cluster->sockettimeoutms = sockettimeoutms;

   if (bson_iter_init_find_case(&iter, b, "secondaryacceptablelatencyms") &&
       BSON_ITER_HOLDS_INT32(&iter)) {
      cluster->sec_latency_ms = bson_iter_int32(&iter);
   }

   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      _mongoc_cluster_node_init(&cluster->nodes[i]);
      cluster->nodes[i].stamp = 0;
      cluster->nodes[i].index = i;
      cluster->nodes[i].ping_avg_msec = -1;
      cluster->nodes[i].needs_auth = cluster->requires_auth;
   }

   _mongoc_array_init (&cluster->iov, sizeof (mongoc_iovec_t));

   EXIT;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_destroy --
 *
 *       Clean up after @cluster and destroy all active connections.
 *       All resources for @cluster are released.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Everything.
 *
 *--------------------------------------------------------------------------
 */

void
_mongoc_cluster_destroy (mongoc_cluster_t *cluster) /* INOUT */
{
   uint32_t i;

   ENTRY;

   bson_return_if_fail (cluster);

   mongoc_uri_destroy (cluster->uri);

   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      if (cluster->nodes[i].stream) {
         _mongoc_cluster_node_destroy (&cluster->nodes [i]);
      }
   }

   bson_free (cluster->replSet);
   cluster->replSet = NULL;

   _mongoc_cluster_clear_peers (cluster);

   _mongoc_array_destroy (&cluster->iov);

   EXIT;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_select --
 *
 *       Selects a cluster node that is suitable for handling the required
 *       set of rpc messages. The read_prefs are taken into account.
 *
 *       If any operation is a write, primary will be forced.
 *
 * Returns:
 *       A mongoc_cluster_node_t if successful; otherwise NULL and
 *       @error is set.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static mongoc_cluster_node_t *
_mongoc_cluster_select (mongoc_cluster_t             *cluster,
                        mongoc_rpc_t                 *rpcs,
                        size_t                        rpcs_len,
                        uint32_t                      hint,
                        const mongoc_write_concern_t *write_concern,
                        const mongoc_read_prefs_t    *read_prefs,
                        bson_error_t                 *error)
{
   mongoc_cluster_node_t *nodes[MONGOC_CLUSTER_MAX_NODES];
   mongoc_read_mode_t read_mode = MONGOC_READ_PRIMARY;
   int scores[MONGOC_CLUSTER_MAX_NODES];
   int max_score = 0;
   uint32_t count;
   uint32_t watermark;
   int32_t nearest = -1;
   bool need_primary;
   bool need_secondary;
   unsigned i;

   ENTRY;

   bson_return_val_if_fail(cluster, NULL);
   bson_return_val_if_fail(rpcs, NULL);
   bson_return_val_if_fail(rpcs_len, NULL);
   bson_return_val_if_fail(hint <= MONGOC_CLUSTER_MAX_NODES, NULL);

   /*
    * We can take a few short-cut's if we are not talking to a replica set.
    */
   switch (cluster->mode) {
   case MONGOC_CLUSTER_DIRECT:
      RETURN (cluster->nodes[0].stream ? &cluster->nodes[0] : NULL);
   case MONGOC_CLUSTER_SHARDED_CLUSTER:
      need_primary = false;
      need_secondary = false;
      GOTO (dispatch);
   case MONGOC_CLUSTER_REPLICA_SET:
   default:
      break;
   }

   /*
    * Determine if our read preference requires communicating with PRIMARY.
    */
   if (read_prefs)
      read_mode = mongoc_read_prefs_get_mode(read_prefs);
   need_primary = (read_mode == MONGOC_READ_PRIMARY);
   need_secondary = (read_mode == MONGOC_READ_SECONDARY);

   /*
    * Check to see if any RPCs require the primary. If so, we pin all
    * of the RPCs to the primary.
    */
   for (i = 0; !need_primary && (i < rpcs_len); i++) {
      switch (rpcs[i].header.opcode) {
      case MONGOC_OPCODE_KILL_CURSORS:
      case MONGOC_OPCODE_GET_MORE:
      case MONGOC_OPCODE_MSG:
      case MONGOC_OPCODE_REPLY:
         break;
      case MONGOC_OPCODE_QUERY:
         if ((read_mode & MONGOC_READ_SECONDARY) != 0) {
            rpcs[i].query.flags |= MONGOC_QUERY_SLAVE_OK;
         } else if (!(rpcs[i].query.flags & MONGOC_QUERY_SLAVE_OK)) {
            need_primary = true;
         }
         break;
      case MONGOC_OPCODE_DELETE:
      case MONGOC_OPCODE_INSERT:
      case MONGOC_OPCODE_UPDATE:
      default:
         need_primary = true;
         break;
      }
   }

dispatch:

   /*
    * Build our list of nodes with established connections. Short circuit if
    * we require a primary and we found one.
    */
   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      if (need_primary && cluster->nodes[i].primary) {
         RETURN(&cluster->nodes[i]);
      } else if (need_secondary && cluster->nodes[i].primary) {
         nodes[i] = NULL;
      } else {
         nodes[i] = cluster->nodes[i].stream ? &cluster->nodes[i] : NULL;
      }
   }

   /*
    * Check if we failed to locate a primary.
    */
   if (need_primary) {
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_NO_ACCEPTABLE_PEER,
                     "Requested PRIMARY node is not available.");
      RETURN(NULL);
   }

   /*
    * Apply the hint if the client knows who they would like to continue
    * communicating with.
    */
   if (hint) {
      if (!nodes[hint - 1]) {
         bson_set_error(error,
                        MONGOC_ERROR_CLIENT,
                        MONGOC_ERROR_CLIENT_NO_ACCEPTABLE_PEER,
                        "Requested node (%u) is not available.",
                        hint);
      }
      RETURN(nodes[hint - 1]);
   }

   /*
    * Now, we start removing connections that don't match the requirements of
    * our requested event.
    *
    * - If read preferences are set, remove all non-matching.
    * - If slaveOk exists and is false, then remove secondaries.
    * - Find the nearest leftover node and remove those not within threshold.
    * - Select a leftover node at random.
    */

   /*
    * TODO: This whole section is ripe for optimization. It is very much
    *       in the fast path of command dispatching.
    */

   count = 0;

   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      if (nodes[i]) {
         if (read_prefs) {
            int score = _mongoc_read_prefs_score(read_prefs, nodes[i]);
            scores[i] = score;

            if (score < 0) {
               nodes[i] = NULL;
               continue;
            } else if (score > max_score) {
                max_score = score;
            }

         }
         count++;
      }
   }

   /*
    * Filter nodes with score less than highest score.
    */
   if (max_score) {
      for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
         if (nodes[i] && (scores[i] < max_score)) {
             nodes[i] = NULL;
             count--;
         }
      }
   }

   /*
    * Get the nearest node among those which have not been filtered out
    */
#define IS_NEARER_THAN(n, msec) \
   ((msec < 0 && (n)->ping_avg_msec >= 0) || ((n)->ping_avg_msec < msec))

   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      if (nodes[i]) {
         if (IS_NEARER_THAN(nodes[i], nearest)) {
            nearest = nodes[i]->ping_avg_msec;
         }
      }
   }

#undef IS_NEARAR_THAN

   /*
    * Filter nodes with latency outside threshold of nearest.
    */
   if (nearest != -1) {
      watermark = nearest + cluster->sec_latency_ms;
      for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
         if (nodes[i]) {
            if (nodes[i]->ping_avg_msec > (int32_t)watermark) {
               nodes[i] = NULL;
               count--;
            }
         }
      }
   }

   /*
    * Mark the error as unable to locate a target node.
    */
   if (!count) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_NO_ACCEPTABLE_PEER,
                      "Failed to locate a suitable MongoDB node.");
      RETURN (NULL);
   }

   /*
    * Choose a cluster node within threshold at random.
    */
   count = count ? rand() % count : count;
   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      if (nodes[i]) {
         if (!count) {
            RETURN(nodes[i]);
         }
         count--;
      }
   }

   RETURN(NULL);
}


uint32_t
_mongoc_cluster_preselect (mongoc_cluster_t             *cluster,       /* IN */
                           mongoc_opcode_t               opcode,        /* IN */
                           const mongoc_write_concern_t *write_concern, /* IN */
                           const mongoc_read_prefs_t    *read_prefs,    /* IN */
                           bson_error_t                 *error)         /* OUT */
{
   mongoc_cluster_node_t *node;
   mongoc_rpc_t rpc = {{ 0 }};
   int retry_count = 0;

   BSON_ASSERT (cluster);

   rpc.header.opcode = opcode;

   while (!(node = _mongoc_cluster_select (cluster, &rpc, 1, 0, write_concern,
                                           read_prefs, error))) {
      if ((retry_count++ == MAX_RETRY_COUNT) ||
          !_mongoc_cluster_reconnect (cluster, error)) {
         break;
      }
   }

   return node ? (node->index + 1) : 0;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_run_command --
 *
 *       Helper to run a command on a given mongoc_cluster_node_t.
 *
 * Returns:
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       @reply is set and should ALWAYS be released with bson_destroy().
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_cluster_run_command (mongoc_cluster_t      *cluster,
                             mongoc_cluster_node_t *node,
                             const char            *db_name,
                             const bson_t          *command,
                             bson_t                *reply,
                             bson_error_t          *error)
{
   mongoc_buffer_t buffer;
   mongoc_array_t ar;
   mongoc_rpc_t rpc;
   int32_t msg_len;
   bson_t reply_local;
   char ns[MONGOC_NAMESPACE_MAX];

   ENTRY;

   BSON_ASSERT(cluster);
   BSON_ASSERT(node);
   BSON_ASSERT(node->stream);
   BSON_ASSERT(db_name);
   BSON_ASSERT(command);

   bson_snprintf(ns, sizeof ns, "%s.$cmd", db_name);

   rpc.query.msg_len = 0;
   rpc.query.request_id = ++cluster->request_id;
   rpc.query.response_to = 0;
   rpc.query.opcode = MONGOC_OPCODE_QUERY;
   rpc.query.flags = MONGOC_QUERY_SLAVE_OK;
   rpc.query.collection = ns;
   rpc.query.skip = 0;
   rpc.query.n_return = -1;
   rpc.query.query = bson_get_data(command);
   rpc.query.fields = NULL;

   _mongoc_array_init (&ar, sizeof (mongoc_iovec_t));
   _mongoc_buffer_init (&buffer, NULL, 0, NULL, NULL);

   _mongoc_rpc_gather(&rpc, &ar);
   _mongoc_rpc_swab_to_le(&rpc);

   if (!mongoc_stream_writev(node->stream, ar.data, ar.len,
                             cluster->sockettimeoutms)) {
      GOTO(failure);
   }

   if (!_mongoc_buffer_append_from_stream(&buffer, node->stream, 4,
                                          cluster->sockettimeoutms, error)) {
      GOTO(failure);
   }

   BSON_ASSERT(buffer.len == 4);

   memcpy(&msg_len, buffer.data, 4);
   msg_len = BSON_UINT32_FROM_LE(msg_len);
   if ((msg_len < 16) || (msg_len > (1024 * 1024 * 16))) {
      GOTO(invalid_reply);
   }

   if (!_mongoc_buffer_append_from_stream(&buffer, node->stream, msg_len - 4,
                                          cluster->sockettimeoutms, error)) {
      GOTO(failure);
   }

   if (!_mongoc_rpc_scatter(&rpc, buffer.data, buffer.len)) {
      GOTO(invalid_reply);
   }

   _mongoc_rpc_swab_from_le(&rpc);

   if (rpc.header.opcode != MONGOC_OPCODE_REPLY) {
      GOTO(invalid_reply);
   }

   if (reply) {
      if (!_mongoc_rpc_reply_get_first(&rpc.reply, &reply_local)) {
         bson_set_error (error,
                         MONGOC_ERROR_BSON,
                         MONGOC_ERROR_BSON_INVALID,
                         "Failed to decode reply BSON document.");
         GOTO(failure);
      }
      bson_copy_to(&reply_local, reply);
      bson_destroy(&reply_local);
   }

   _mongoc_buffer_destroy(&buffer);
   _mongoc_array_destroy(&ar);

   RETURN(true);

invalid_reply:
   bson_set_error(error,
                  MONGOC_ERROR_PROTOCOL,
                  MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                  "Invalid reply from server.");

failure:
   _mongoc_buffer_destroy(&buffer);
   _mongoc_array_destroy(&ar);

   if (reply) {
      bson_init(reply);
   }

   _mongoc_cluster_disconnect_node(cluster, node);

   RETURN(false);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_ismaster --
 *
 *       Executes an isMaster command on a given mongoc_cluster_node_t.
 *
 *       node->primary will be set to true if the node is discovered to
 *       be a primary node.
 *
 * Returns:
 *       true if successful; otehrwise false and @error is set.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_cluster_ismaster (mongoc_cluster_t      *cluster,
                          mongoc_cluster_node_t *node,
                          bson_error_t          *error)
{
   int32_t v32;
   bool ret = false;
   bson_iter_t child;
   bson_iter_t iter;
   bson_t command;
   bson_t reply;

   ENTRY;

   BSON_ASSERT(cluster);
   BSON_ASSERT(node);
   BSON_ASSERT(node->stream);

   bson_init(&command);
   bson_append_int32(&command, "isMaster", 8, 1);

   if (!_mongoc_cluster_run_command (cluster, node, "admin", &command, &reply,
                                     error)) {
      _mongoc_cluster_disconnect_node (cluster, node);
      GOTO (failure);
   }

   node->primary = false;

   bson_free (node->replSet);
   node->replSet = NULL;

   if (bson_iter_init_find_case (&iter, &reply, "isMaster") &&
       BSON_ITER_HOLDS_BOOL (&iter) &&
       bson_iter_bool (&iter)) {
      node->primary = true;
   }

   if (bson_iter_init_find_case(&iter, &reply, "maxMessageSizeBytes")) {
      v32 = bson_iter_int32(&iter);
      if (!cluster->max_msg_size || (v32 < (int32_t)cluster->max_msg_size)) {
         cluster->max_msg_size = v32;
      }
   }

   if (bson_iter_init_find_case(&iter, &reply, "maxBsonObjectSize")) {
      v32 = bson_iter_int32(&iter);
	  if (!cluster->max_bson_size || (v32 < (int32_t)cluster->max_bson_size)) {
         cluster->max_bson_size = v32;
      }
   }

   if (bson_iter_init_find_case (&iter, &reply, "maxWriteBatchSize")) {
      v32 = bson_iter_int32 (&iter);
      node->max_write_batch_size = v32;
   }

   if (bson_iter_init_find_case(&iter, &reply, "maxWireVersion") &&
       BSON_ITER_HOLDS_INT32(&iter)) {
      node->max_wire_version = bson_iter_int32(&iter);
   }

   if (bson_iter_init_find_case(&iter, &reply, "minWireVersion") &&
       BSON_ITER_HOLDS_INT32(&iter)) {
      node->min_wire_version = bson_iter_int32(&iter);
   }

   if ((node->min_wire_version > MAX_WIRE_VERSION) ||
       (node->max_wire_version < MIN_WIRE_VERSION)) {
      bson_set_error (error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_BAD_WIRE_VERSION,
                      "Failed to negotiate wire version with "
                      "cluster peer. %s is [%u,%u]. I support "
                      "[%u,%u].",
                      node->host.host_and_port,
                      node->min_wire_version,
                      node->max_wire_version,
                      MIN_WIRE_VERSION,
                      MAX_WIRE_VERSION);
      GOTO (failure);
   }

   if (bson_iter_init_find (&iter, &reply, "msg") &&
       BSON_ITER_HOLDS_UTF8 (&iter) &&
       (0 == strcasecmp ("isdbgrid", bson_iter_utf8 (&iter, NULL)))) {
      node->isdbgrid = true;
      if (cluster->mode != MONGOC_CLUSTER_SHARDED_CLUSTER) {
         MONGOC_INFO ("Unexpectedly connected to sharded cluster: %s",
                      node->host.host_and_port);
      }
   } else {
      node->isdbgrid = false;
   }

   /*
    * If we are in replicaSet mode, we need to track our potential peers for
    * further connections.
    */
   if (cluster->mode == MONGOC_CLUSTER_REPLICA_SET) {
      if (bson_iter_init_find (&iter, &reply, "hosts") &&
          bson_iter_recurse (&iter, &child)) {
         if (node->primary) {
            _mongoc_cluster_clear_peers (cluster);
         }
         while (bson_iter_next (&child) && BSON_ITER_HOLDS_UTF8 (&child)) {
            _mongoc_cluster_add_peer (cluster, bson_iter_utf8(&child, NULL));
         }
      }
      if (bson_iter_init_find(&iter, &reply, "setName") &&
          BSON_ITER_HOLDS_UTF8(&iter)) {
         node->replSet = bson_iter_dup_utf8(&iter, NULL);
      }
      if (bson_iter_init_find(&iter, &reply, "tags") &&
          BSON_ITER_HOLDS_DOCUMENT(&iter)) {
          bson_t tags;
          uint32_t len;
          const uint8_t *data;

          bson_iter_document(&iter, &len, &data);

          if (bson_init_static(&tags, data, len)) {
              bson_copy_to(&tags, &(node->tags));
          }
      }
   }

   ret = true;

failure:
   bson_destroy(&command);
   bson_destroy(&reply);

   RETURN(ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_ping_node --
 *
 *       Ping a remote node and return the round-trip latency.
 *
 * Returns:
 *       A 32-bit integer counting the number of milliseconds to complete.
 *       -1 if there was a failure to communicate.
 *
 * Side effects:
 *       @error is set of -1 is returned.
 *
 *--------------------------------------------------------------------------
 */

static int32_t
_mongoc_cluster_ping_node (mongoc_cluster_t      *cluster,
                           mongoc_cluster_node_t *node,
                           bson_error_t          *error)
{
   int64_t t_begin;
   int64_t t_end;
   int32_t ret;
   bool r;
   bson_t cmd;

   ENTRY;

   BSON_ASSERT(cluster);
   BSON_ASSERT(node);
   BSON_ASSERT(node->stream);

   bson_init(&cmd);
   bson_append_int32(&cmd, "ping", 4, 1);

   t_begin = bson_get_monotonic_time ();
   r = _mongoc_cluster_run_command (cluster, node, "admin", &cmd, NULL, error);
   t_end = bson_get_monotonic_time ();

   bson_destroy(&cmd);

   ret = r ? (int32_t) ((t_end - t_begin) / 1000L) : -1;

   RETURN(ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_auth_node_cr --
 *
 *       Performs authentication of @node using the credentials provided
 *       when configuring the @cluster instance.
 *
 *       This is the Challenge-Response mode of authentication.
 *
 * Returns:
 *       true if authentication was successful; otherwise false and
 *       @error is set.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_cluster_auth_node_cr (mongoc_cluster_t      *cluster,
                              mongoc_cluster_node_t *node,
                              bson_error_t          *error)
{
   bson_iter_t iter;
   const char *auth_source;
   bson_t command = { 0 };
   bson_t reply = { 0 };
   char *digest;
   char *nonce;

   ENTRY;

   BSON_ASSERT(cluster);
   BSON_ASSERT(node);

   if (!(auth_source = mongoc_uri_get_auth_source(cluster->uri)) ||
       (*auth_source == '\0')) {
      auth_source = "admin";
   }

   /*
    * To authenticate a node using basic authentication, we need to first
    * get the nonce from the server. We use that to hash our password which
    * is sent as a reply to the server. If everything went good we get a
    * success notification back from the server.
    */

   /*
    * Execute the getnonce command to fetch the nonce used for generating
    * md5 digest of our password information.
    */
   bson_init (&command);
   bson_append_int32 (&command, "getnonce", 8, 1);
   if (!_mongoc_cluster_run_command (cluster, node, auth_source, &command,
                                     &reply, error)) {
      bson_destroy (&command);
      RETURN (false);
   }
   bson_destroy (&command);
   if (!bson_iter_init_find_case (&iter, &reply, "nonce")) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_GETNONCE,
                      "Invalid reply from getnonce");
      bson_destroy (&reply);
      RETURN (false);
   }

   /*
    * Build our command to perform the authentication.
    */
   nonce = bson_iter_dup_utf8(&iter, NULL);
   digest = _mongoc_cluster_build_basic_auth_digest(cluster, nonce);
   bson_init(&command);
   bson_append_int32(&command, "authenticate", 12, 1);
   bson_append_utf8(&command, "user", 4,
                    mongoc_uri_get_username(cluster->uri), -1);
   bson_append_utf8(&command, "nonce", 5, nonce, -1);
   bson_append_utf8(&command, "key", 3, digest, -1);
   bson_destroy(&reply);
   bson_free(nonce);
   bson_free(digest);

   /*
    * Execute the authenticate command and check for {ok:1}
    */
   if (!_mongoc_cluster_run_command (cluster, node, auth_source, &command,
                                     &reply, error)) {
      bson_destroy (&command);
      RETURN (false);
   }

   bson_destroy (&command);

   if (!bson_iter_init_find_case(&iter, &reply, "ok") ||
       !bson_iter_as_bool(&iter)) {
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_AUTHENTICATE,
                     "Failed to authenticate credentials.");
      bson_destroy(&reply);
      RETURN(false);
   }

   bson_destroy(&reply);

   RETURN(true);
}


#ifdef MONGOC_ENABLE_SASL
/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_get_canonicalized_name --
 *
 *       Query the node to get the canonicalized name. This may happen if
 *       the node has been accessed via an alias.
 *
 *       The gssapi code will use this if canonicalizeHostname is true.
 *
 *       Some underlying layers of krb might do this for us, but they can
 *       be disabled in krb.conf.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_cluster_get_canonicalized_name (mongoc_cluster_t      *cluster, /* IN */
                                        mongoc_cluster_node_t *node,    /* IN */
                                        char                  *name,    /* OUT */
                                        size_t                 namelen, /* IN */
                                        bson_error_t          *error)   /* OUT */
{
   mongoc_stream_t *stream;
   mongoc_stream_t *tmp;
   mongoc_socket_t *sock = NULL;
   char *canonicalized;

   ENTRY;

   BSON_ASSERT (cluster);
   BSON_ASSERT (node);
   BSON_ASSERT (name);

   /*
    * Find the underlying socket used in the stream chain.
    */
   for (stream = node->stream; stream;) {
      if ((tmp = mongoc_stream_get_base_stream (stream))) {
         stream = tmp;
         continue;
      }
      break;
   }

   BSON_ASSERT (stream);

   if (stream->type == MONGOC_STREAM_SOCKET) {
      sock = mongoc_stream_socket_get_socket ((mongoc_stream_socket_t *)stream);
      if (sock) {
         canonicalized = mongoc_socket_getnameinfo (sock);
         if (canonicalized) {
            bson_snprintf (name, namelen, "%s", canonicalized);
            bson_free (canonicalized);
            RETURN (true);
         }
      }
   }

   RETURN (false);
}
#endif


#ifdef MONGOC_ENABLE_SASL
/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_auth_node_sasl --
 *
 *       Perform authentication for a cluster node using SASL. This is
 *       only supported for GSSAPI at the moment.
 *
 * Returns:
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       error may be set.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_cluster_auth_node_sasl (mongoc_cluster_t      *cluster,
                                mongoc_cluster_node_t *node,
                                bson_error_t          *error)
{
   uint32_t buflen = 0;
   mongoc_sasl_t sasl;
   const bson_t *options;
   bson_iter_t iter;
   bool ret = false;
   char real_name [BSON_HOST_NAME_MAX + 1];
   const char *service_name;
   const char *mechanism;
   const char *tmpstr;
   uint8_t buf[4096] = { 0 };
   bson_t cmd;
   bson_t reply;
   int conv_id = 0;

   BSON_ASSERT (cluster);
   BSON_ASSERT (node);

   options = mongoc_uri_get_options (cluster->uri);

   _mongoc_sasl_init (&sasl);

   if ((mechanism = mongoc_uri_get_auth_mechanism (cluster->uri))) {
      _mongoc_sasl_set_mechanism (&sasl, mechanism);
   }

   if (bson_iter_init_find_case (&iter, options, "gssapiservicename") &&
       BSON_ITER_HOLDS_UTF8 (&iter) &&
       (service_name = bson_iter_utf8 (&iter, NULL))) {
      _mongoc_sasl_set_service_name (&sasl, service_name);
   }

   _mongoc_sasl_set_pass (&sasl, mongoc_uri_get_password (cluster->uri));
   _mongoc_sasl_set_user (&sasl, mongoc_uri_get_username (cluster->uri));

   /*
    * If the URI requested canonicalizeHostname, we need to resolve the real
    * hostname for the IP Address and pass that to the SASL layer. Some
    * underlying GSSAPI layers will do this for us, but can be disabled in
    * their config (krb.conf).
    *
    * This allows the consumer to specify canonicalizeHostname=true in the URI
    * and have us do that for them.
    *
    * See CDRIVER-323 for more information.
    */
   if (bson_iter_init_find_case (&iter, options, "canonicalizeHostname") &&
       BSON_ITER_HOLDS_BOOL (&iter) &&
       bson_iter_bool (&iter)) {
      if (_mongoc_cluster_get_canonicalized_name (cluster, node, real_name,
                                                  sizeof real_name, error)) {
         _mongoc_sasl_set_service_host (&sasl, real_name);
      } else {
         _mongoc_sasl_set_service_host (&sasl, node->host.host);
      }
   } else {
      _mongoc_sasl_set_service_host (&sasl, node->host.host);
   }

   for (;;) {
      if (!_mongoc_sasl_step (&sasl, buf, buflen, buf, sizeof buf, &buflen, error)) {
         goto failure;
      }

      bson_init (&cmd);

      if (sasl.step == 1) {
         BSON_APPEND_INT32 (&cmd, "saslStart", 1);
         BSON_APPEND_UTF8 (&cmd, "mechanism", mechanism ? mechanism : "GSSAPI");
         bson_append_utf8 (&cmd, "payload", 7, (const char *)buf, buflen);
         BSON_APPEND_INT32 (&cmd, "autoAuthorize", 1);
      } else {
         BSON_APPEND_INT32 (&cmd, "saslContinue", 1);
         BSON_APPEND_INT32 (&cmd, "conversationId", conv_id);
         bson_append_utf8 (&cmd, "payload", 7, (const char *)buf, buflen);
      }

      MONGOC_INFO ("SASL: authenticating \"%s\" (step %d)",
                   mongoc_uri_get_username (cluster->uri),
                   sasl.step);

      if (!_mongoc_cluster_run_command (cluster, node, "$external", &cmd, &reply, error)) {
         bson_destroy (&cmd);
         goto failure;
      }

      bson_destroy (&cmd);

      if (bson_iter_init_find (&iter, &reply, "done") &&
          bson_iter_as_bool (&iter)) {
         bson_destroy (&reply);
         break;
      }

      if (!bson_iter_init_find (&iter, &reply, "ok") ||
          !bson_iter_as_bool (&iter) ||
          !bson_iter_init_find (&iter, &reply, "conversationId") ||
          !BSON_ITER_HOLDS_INT32 (&iter) ||
          !(conv_id = bson_iter_int32 (&iter)) ||
          !bson_iter_init_find (&iter, &reply, "payload") ||
          !BSON_ITER_HOLDS_UTF8 (&iter)) {
         MONGOC_INFO ("SASL: authentication failed for \"%s\"",
                      mongoc_uri_get_username (cluster->uri));
         bson_destroy (&reply);
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_AUTHENTICATE,
                         "Received invalid SASL reply from MongoDB server.");
         goto failure;
      }

      tmpstr = bson_iter_utf8 (&iter, &buflen);

      if (buflen > sizeof buf) {
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_AUTHENTICATE,
                         "SASL reply from MongoDB is too large.");
         goto failure;
      }

      memcpy (buf, tmpstr, buflen);

      bson_destroy (&reply);
   }

   MONGOC_INFO ("SASL: \"%s\" authenticated",
                mongoc_uri_get_username (cluster->uri));

   ret = true;

failure:
   _mongoc_sasl_destroy (&sasl);

   return ret;
}
#endif


#ifdef MONGOC_ENABLE_SASL
/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_auth_node_plain --
 *
 *       Perform SASL PLAIN authentication for @node. We do this manually
 *       instead of using the SASL module because its rather simplistic.
 *
 * Returns:
 *       true if successful; otherwise false and error is set.
 *
 * Side effects:
 *       error may be set.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_cluster_auth_node_plain (mongoc_cluster_t      *cluster,
                                 mongoc_cluster_node_t *node,
                                 bson_error_t          *error)
{
   char buf[4096];
   unsigned buflen = 0;
   bson_iter_t iter;
   const char *username;
   const char *password;
   const char *errmsg = "Unknown authentication error.";
   bson_t b = BSON_INITIALIZER;
   bson_t reply;
   size_t len;
   char *str;
   int ret;

   BSON_ASSERT (cluster);
   BSON_ASSERT (node);

   username = mongoc_uri_get_username (cluster->uri);
   if (!username) {
      username = "";
   }

   password = mongoc_uri_get_password (cluster->uri);
   if (!password) {
      password = "";
   }

   str = bson_strdup_printf ("%c%s%c%s", '\0', username, '\0', password);
   len = strlen (username) + strlen (password) + 2;
   ret = sasl_encode64 (str, len, buf, sizeof buf, &buflen);
   bson_free (str);

   if (ret != SASL_OK) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      "sasl_encode64() returned %d.",
                      ret);
      return false;
   }

   BSON_APPEND_INT32 (&b, "saslStart", 1);
   BSON_APPEND_UTF8 (&b, "mechanism", "PLAIN");
   bson_append_utf8 (&b, "payload", 7, (const char *)buf, buflen);
   BSON_APPEND_INT32 (&b, "autoAuthorize", 1);

   if (!_mongoc_cluster_run_command (cluster, node, "$external", &b, &reply, error)) {
      bson_destroy (&b);
      return false;
   }

   bson_destroy (&b);

   if (!bson_iter_init_find_case (&iter, &reply, "ok") ||
       !bson_iter_as_bool (&iter)) {
      if (bson_iter_init_find_case (&iter, &reply, "errmsg") &&
          BSON_ITER_HOLDS_UTF8 (&iter)) {
         errmsg = bson_iter_utf8 (&iter, NULL);
      }
      bson_destroy (&reply);
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      "%s", errmsg);
      return false;
   }

   bson_destroy (&reply);

   return true;
}
#endif


#ifdef MONGOC_ENABLE_SSL
static bool
_mongoc_cluster_auth_node_x509 (mongoc_cluster_t      *cluster,
                                mongoc_cluster_node_t *node,
                                bson_error_t          *error)
{
   const char *username = "";
   const char *errmsg = "X509 authentication failure";
   bson_iter_t iter;
   bool ret = false;
   bson_t cmd;
   bson_t reply;

   BSON_ASSERT (cluster);
   BSON_ASSERT (node);

   if (!cluster->client->ssl_opts.pem_file) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      "mongoc_client_set_ssl_opts() must be called "
                      "with pem file for X-509 auth.");
      return false;
   }

   if (cluster->client->pem_subject) {
      username = cluster->client->pem_subject;
   }

   bson_init (&cmd);
   BSON_APPEND_INT32 (&cmd, "authenticate", 1);
   BSON_APPEND_UTF8 (&cmd, "mechanism", "MONGODB-X509");
   BSON_APPEND_UTF8 (&cmd, "user", username);

   if (!_mongoc_cluster_run_command (cluster, node, "$external", &cmd, &reply,
                                     error)) {
      bson_destroy (&cmd);
      return false;
   }

   if (!bson_iter_init_find (&iter, &reply, "ok") ||
       !bson_iter_as_bool (&iter)) {
      if (bson_iter_init_find (&iter, &reply, "errmsg") &&
          BSON_ITER_HOLDS_UTF8 (&iter)) {
         errmsg = bson_iter_utf8 (&iter, NULL);
      }
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      "%s", errmsg);
      goto failure;
   }

   ret = true;

failure:

   bson_destroy (&cmd);
   bson_destroy (&reply);

   return ret;
}
#endif


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_auth_node --
 *
 *       Authenticate a cluster node depending on the required mechanism.
 *
 * Returns:
 *       true if authenticated. false on failure and @error is set.
 *
 * Side effects:
 *       @error is set on failure.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_cluster_auth_node (mongoc_cluster_t      *cluster,
                           mongoc_cluster_node_t *node,
                           bson_error_t          *error)
{
   bool ret = false;
   const char *mechanism;

   BSON_ASSERT (cluster);
   BSON_ASSERT (node);

   mechanism = mongoc_uri_get_auth_mechanism (cluster->uri);

   if (!mechanism) {
      mechanism = "MONGODB-CR";
   }

   if (0 == strcasecmp (mechanism, "MONGODB-CR")) {
      ret = _mongoc_cluster_auth_node_cr (cluster, node, error);
#ifdef MONGOC_ENABLE_SSL
   } else if (0 == strcasecmp (mechanism, "MONGODB-X509")) {
      ret = _mongoc_cluster_auth_node_x509 (cluster, node, error);
#endif
#ifdef MONGOC_ENABLE_SASL
   } else if (0 == strcasecmp (mechanism, "GSSAPI")) {
      ret = _mongoc_cluster_auth_node_sasl (cluster, node, error);
   } else if (0 == strcasecmp (mechanism, "PLAIN")) {
      ret = _mongoc_cluster_auth_node_plain (cluster, node, error);
#endif
   } else {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      "The authentication mechanism \"%s\" is not supported.",
                      mechanism);
   }

   if (!ret) {
      mongoc_counter_auth_failure_inc ();
   } else {
      mongoc_counter_auth_success_inc ();
   }

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_reconnect_direct --
 *
 *       Reconnect to our only configured node.
 *
 *       "isMaster" is run after connecting to determine PRIMARY status.
 *
 *       If the node is valid, we will also greedily authenticate the
 *       configured user if available.
 *
 * Returns:
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_cluster_reconnect_direct (mongoc_cluster_t *cluster,
                                  bson_error_t     *error)
{
   const mongoc_host_list_t *hosts;
   mongoc_cluster_node_t *node;
   mongoc_stream_t *stream;
   struct timeval timeout;

   ENTRY;

   BSON_ASSERT(cluster);

   if (!(hosts = mongoc_uri_get_hosts(cluster->uri))) {
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_NOT_READY,
                     "Invalid host list supplied.");
      RETURN(false);
   }

   cluster->last_reconnect = bson_get_monotonic_time();

   node = &cluster->nodes[0];

   node->index = 0;
   node->host = *hosts;
   node->needs_auth = cluster->requires_auth;
   node->primary = false;
   node->ping_avg_msec = -1;
   memset(node->pings, 0xFF, sizeof node->pings);
   node->pings_pos = 0;
   node->stream = NULL;
   node->stamp++;
   bson_init(&node->tags);

   stream = _mongoc_client_create_stream (cluster->client, hosts, error);
   if (!stream) {
      RETURN (false);
   }

   node->stream = stream;
   node->stamp++;

   timeout.tv_sec = cluster->sockettimeoutms / 1000UL;
   timeout.tv_usec = (cluster->sockettimeoutms % 1000UL) * 1000UL;
   mongoc_stream_setsockopt (stream, SOL_SOCKET, SO_RCVTIMEO,
                             &timeout, sizeof timeout);
   mongoc_stream_setsockopt (stream, SOL_SOCKET, SO_SNDTIMEO,
                             &timeout, sizeof timeout);

   if (!_mongoc_cluster_ismaster (cluster, node, error)) {
      _mongoc_cluster_disconnect_node (cluster, node);
      RETURN (false);
   }

   if (node->needs_auth) {
      if (!_mongoc_cluster_auth_node (cluster, node, error)) {
         _mongoc_cluster_disconnect_node (cluster, node);
         RETURN (false);
      }
      node->needs_auth = false;
   }

   _mongoc_cluster_update_state (cluster);

   RETURN (true);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_reconnect_replica_set --
 *
 *       Reconnect to replicaSet members that are unhealthy.
 *
 *       Each of them will be checked for matching replicaSet name
 *       and capabilities via an "isMaster" command.
 *
 *       The nodes will also be greedily authenticated with the
 *       configured user if available.
 *
 * Returns:
 *       true if there is an established stream that may be used,
 *       otherwise false and @error is set.
 *
 * Side effects:
 *       @error is set upon failure if non-NULL.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_cluster_reconnect_replica_set (mongoc_cluster_t *cluster,
                                       bson_error_t     *error)
{
   const mongoc_host_list_t *hosts;
   const mongoc_host_list_t *iter;
   mongoc_cluster_node_t node;
   mongoc_cluster_node_t saved_nodes [MONGOC_CLUSTER_MAX_NODES];
   mongoc_host_list_t host;
   mongoc_stream_t *stream;
   mongoc_list_t *list;
   mongoc_list_t *liter;
   int32_t ping;
   const char *replSet;
   int i;
   int j;

   ENTRY;

   BSON_ASSERT(cluster);
   BSON_ASSERT(cluster->mode == MONGOC_CLUSTER_REPLICA_SET);

   MONGOC_DEBUG("Reconnecting to replica set.");

   if (!(hosts = mongoc_uri_get_hosts(cluster->uri))) {
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_NOT_READY,
                     "Invalid host list supplied.");
      RETURN(false);
   }

   replSet = mongoc_uri_get_replica_set(cluster->uri);
   BSON_ASSERT(replSet);

   /*
    * Replica Set (Re)Connection Strategy
    * ===================================
    *
    * First we break all existing connections. This may change.
    *
    * To perform the replica set connection, we connect to each of the
    * pre-configured replicaSet nodes. (There may in fact only be one).
    *
    * TODO: We should perform this initial connection in parallel.
    *
    * Using the result of an "isMaster" on each of these nodes, we can
    * prime the cluster nodes we want to connect to.
    *
    * We then connect to all of these nodes in parallel. Once we have
    * all of the working nodes established, we can complete the process.
    *
    * We return true if any of the connections were successful, however
    * we must update the cluster health appropriately so that callers
    * that need a PRIMARY node can force reconnection.
    *
    * TODO: At some point in the future, we will need to authenticate
    *       before calling an "isMaster". But that is dependent on a
    *       few server "features" first.
    */

   cluster->last_reconnect = bson_get_monotonic_time();

   _mongoc_cluster_clear_peers (cluster);

   /*
    * Discover all the potential peers from our seeds.
    */
   for (iter = hosts; iter; iter = iter->next) {
      stream = _mongoc_client_create_stream(cluster->client, iter, error);
      if (!stream) {
         MONGOC_WARNING("Failed connection to %s", iter->host_and_port);
         continue;
      }

      _mongoc_cluster_node_init(&node);
      node.host = *iter;
      node.stream = stream;

      if (!_mongoc_cluster_ismaster (cluster, &node, error)) {
         _mongoc_cluster_node_destroy (&node);
         continue;
      }

      if (!node.replSet || !!strcmp (node.replSet, replSet)) {
         MONGOC_INFO ("%s: Got replicaSet \"%s\" expected \"%s\".",
                      iter->host_and_port,
                      node.replSet ? node.replSet : "(null)",
                      replSet);
      }

      if (node.primary) {
         _mongoc_cluster_node_destroy (&node);
         break;
      }

      _mongoc_cluster_node_destroy (&node);
   }

   list = cluster->peers;
   cluster->peers = NULL;

   /*
    * To avoid reconnecting to all of the peers, we will save the
    * functional connections (and save their ping times) so that
    * we don't waste time doing that again.
    */

   memset (saved_nodes, 0, sizeof saved_nodes);

   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      if (cluster->nodes [i].stream) {
         saved_nodes [i].host = cluster->nodes [i].host;
         saved_nodes [i].stream = cluster->nodes [i].stream;
         cluster->nodes [i].stream = NULL;
      }
   }

   for (liter = list, i = 0;
        liter && (i < MONGOC_CLUSTER_MAX_NODES);
        liter = liter->next) {

      if (!_mongoc_host_list_from_string(&host, liter->data)) {
         MONGOC_WARNING("Failed to parse host and port: \"%s\"",
                        (char *)liter->data);
         continue;
      }

      stream = NULL;

      for (j = 0; j < MONGOC_CLUSTER_MAX_NODES; j++) {
         if (0 == strcmp (saved_nodes [j].host.host_and_port,
                          host.host_and_port)) {
            stream = saved_nodes [j].stream;
            saved_nodes [j].stream = NULL;
         }
      }

      if (!stream) {
         stream = _mongoc_client_create_stream (cluster->client, &host, error);

         if (!stream) {
            MONGOC_WARNING("Failed connection to %s", host.host_and_port);
            continue;
         }
      }

      _mongoc_cluster_node_init(&cluster->nodes[i]);

      cluster->nodes[i].host = host;
      cluster->nodes[i].index = i;
      cluster->nodes[i].stream = stream;
      cluster->nodes[i].needs_auth = cluster->requires_auth;

      if (!_mongoc_cluster_ismaster(cluster, &cluster->nodes[i], error)) {
         _mongoc_cluster_node_destroy(&cluster->nodes[i]);
         continue;
      }

      if (!cluster->nodes[i].replSet ||
          !!strcmp (cluster->nodes[i].replSet, replSet)) {
         MONGOC_INFO ("%s: Got replicaSet \"%s\" expected \"%s\".",
                      host.host_and_port,
                      cluster->nodes[i].replSet,
                      replSet);
         _mongoc_cluster_node_destroy (&cluster->nodes[i]);
         continue;
      }

      if (cluster->nodes[i].needs_auth) {
         if (!_mongoc_cluster_auth_node (cluster, &cluster->nodes[i], error)) {
            _mongoc_cluster_node_destroy (&cluster->nodes[i]);
            RETURN (false);
         }
         cluster->nodes[i].needs_auth = false;
      }

      if (-1 == (ping = _mongoc_cluster_ping_node (cluster,
                                                   &cluster->nodes[i],
                                                   error))) {
         MONGOC_INFO("%s: Lost connection during ping.",
                     host.host_and_port);
         _mongoc_cluster_node_destroy (&cluster->nodes[i]);
         continue;
      }

      _mongoc_cluster_node_track_ping(&cluster->nodes[i], ping);

      i++;
   }

   _mongoc_list_foreach(list, (void(*)(void*,void*))bson_free, NULL);
   _mongoc_list_destroy(list);

   /*
    * Cleanup all potential saved connections that were not used.
    */

   for (j = 0; j < MONGOC_CLUSTER_MAX_NODES; j++) {
      if (saved_nodes [j].stream) {
         mongoc_stream_destroy (saved_nodes [j].stream);
         saved_nodes [j].stream = NULL;
      }
   }

   if (i == 0) {
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_NO_ACCEPTABLE_PEER,
                     "No acceptable peer could be found.");
      RETURN(false);
   }

   _mongoc_cluster_update_state (cluster);

   RETURN(true);
}


static bool
_mongoc_cluster_reconnect_sharded_cluster (mongoc_cluster_t *cluster,
                                           bson_error_t     *error)
{
   const mongoc_host_list_t *hosts;
   const mongoc_host_list_t *iter;
   mongoc_stream_t *stream;
   uint32_t i;
   int32_t ping;

   ENTRY;

   BSON_ASSERT (cluster);

   MONGOC_DEBUG ("Reconnecting to sharded cluster.");

   /*
    * Sharded Cluster (Re)Connection Strategy
    * =======================================
    *
    * First we break all existing connections. This may and probably
    * should change.
    *
    * Sharded cluster connection is pretty simple, in that we just need
    * to connect to all of the nodes that we are configured to know
    * about. The reconnect_direct case will also update things if it
    * discovers that the node it connected to was a sharded cluster.
    *
    * We need to check for "msg" field of the "isMaster" command to
    * ensure that we have connected to an "isdbgrid".
    *
    * If we can connect to all of the nodes, we are in a good state,
    * otherwise we are in an unhealthy state. If no connections were
    * established then we are in a failed state.
    */

   cluster->last_reconnect = bson_get_monotonic_time ();

   hosts = mongoc_uri_get_hosts (cluster->uri);

   /*
    * Reconnect to each of our configured hosts.
    */
   for (iter = hosts, i = 0; iter; iter = iter->next) {
      stream = _mongoc_client_create_stream (cluster->client, iter, error);

      if (!stream) {
         MONGOC_WARNING ("Failed connection to %s", iter->host_and_port);
         continue;
      }

      _mongoc_cluster_node_init (&cluster->nodes[i]);

      cluster->nodes[i].host = *iter;
      cluster->nodes[i].index = i;
      cluster->nodes[i].stream = stream;
      cluster->nodes[i].needs_auth = cluster->requires_auth;

      if (!_mongoc_cluster_ismaster (cluster, &cluster->nodes[i], error)) {
         _mongoc_cluster_node_destroy (&cluster->nodes[i]);
         continue;
      }

      if (cluster->nodes[i].needs_auth) {
         if (!_mongoc_cluster_auth_node (cluster, &cluster->nodes[i], error)) {
            _mongoc_cluster_node_destroy (&cluster->nodes[i]);
            RETURN (false);
         }
         cluster->nodes[i].needs_auth = false;
      }

      if (-1 == (ping = _mongoc_cluster_ping_node (cluster,
                                                   &cluster->nodes[i],
                                                   error))) {
         MONGOC_INFO ("%s: Lost connection during ping.",
                      iter->host_and_port);
         _mongoc_cluster_node_destroy (&cluster->nodes[i]);
         continue;
      }

      _mongoc_cluster_node_track_ping (&cluster->nodes[i], ping);

      /*
       * If this node is not a mongos, we should fail unless no
       * replicaSet was specified. If that is the case, we will assume
       * the caller meant they wanted a replicaSet and migrate to that
       * reconnection strategy.
       */
      if ((i == 0) &&
          !cluster->nodes [i].isdbgrid &&
          !mongoc_uri_get_replica_set (cluster->uri) &&
          cluster->nodes [i].replSet) {
         MONGOC_WARNING ("Found replicaSet, expected sharded cluster. "
                         "Reconnecting as replicaSet.");
         cluster->mode = MONGOC_CLUSTER_REPLICA_SET;
         cluster->replSet = bson_strdup (cluster->nodes [i].replSet);
         return _mongoc_cluster_reconnect_replica_set (cluster, error);
      }

      i++;
   }

   if (i == 0) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_NO_ACCEPTABLE_PEER,
                      "No acceptable peer could be found.");
      RETURN (false);
   }

   _mongoc_cluster_update_state (cluster);

   RETURN (true);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_reconnect --
 *
 *       Reconnect to the cluster nodes.
 *
 *       This is called when no nodes were available to execute an
 *       operation on.
 *
 * Returns:
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
_mongoc_cluster_reconnect (mongoc_cluster_t *cluster,
                           bson_error_t     *error)
{
   bool ret;
   int i;

   ENTRY;

   bson_return_val_if_fail (cluster, false);

   /*
    * Close any lingering connections.
    *
    * TODO: We could be better about reusing connections.
    */
   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
       if (cluster->nodes [i].stream) {
           mongoc_stream_close (cluster->nodes [i].stream);
           mongoc_stream_destroy (cluster->nodes [i].stream);
           cluster->nodes [i].stream = NULL;
       }
   }

   _mongoc_cluster_update_state (cluster);

   switch (cluster->mode) {
   case MONGOC_CLUSTER_DIRECT:
      ret = _mongoc_cluster_reconnect_direct (cluster, error);
      RETURN (ret);
   case MONGOC_CLUSTER_REPLICA_SET:
      ret = _mongoc_cluster_reconnect_replica_set (cluster, error);
      RETURN (ret);
   case MONGOC_CLUSTER_SHARDED_CLUSTER:
      ret = _mongoc_cluster_reconnect_sharded_cluster (cluster, error);
      RETURN (ret);
   default:
      break;
   }

   bson_set_error(error,
                  MONGOC_ERROR_CLIENT,
                  MONGOC_ERROR_CLIENT_NOT_READY,
                  "Unsupported cluster mode: %02x",
                  cluster->mode);

   RETURN (false);
}


bool
_mongoc_cluster_command_early (mongoc_cluster_t *cluster,
                               const char       *dbname,
                               const bson_t     *command,
                               bson_t           *reply,
                               bson_error_t     *error)
{
   mongoc_cluster_node_t *node;
   int i;

   BSON_ASSERT (cluster);
   BSON_ASSERT (cluster->state == MONGOC_CLUSTER_STATE_BORN);
   BSON_ASSERT (dbname);
   BSON_ASSERT (command);

   if (!_mongoc_cluster_reconnect (cluster, error)) {
      return false;
   }

   node = _mongoc_cluster_get_primary (cluster);

   for (i = 0; !node && i < MONGOC_CLUSTER_MAX_NODES; i++) {
      if (cluster->nodes[i].stream) {
         node = &cluster->nodes[i];
      }
   }

   return _mongoc_cluster_run_command (cluster, node, dbname, command,
                                       reply, error);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_inc_egress_rpc --
 *
 *       Helper to increment the counter for a particular RPC based on
 *       it's opcode.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static BSON_INLINE void
_mongoc_cluster_inc_egress_rpc (const mongoc_rpc_t *rpc)
{
   mongoc_counter_op_egress_total_inc();

   switch (rpc->header.opcode) {
   case MONGOC_OPCODE_DELETE:
      mongoc_counter_op_egress_delete_inc();
      break;
   case MONGOC_OPCODE_UPDATE:
      mongoc_counter_op_egress_update_inc();
      break;
   case MONGOC_OPCODE_INSERT:
      mongoc_counter_op_egress_insert_inc();
      break;
   case MONGOC_OPCODE_KILL_CURSORS:
      mongoc_counter_op_egress_killcursors_inc();
      break;
   case MONGOC_OPCODE_GET_MORE:
      mongoc_counter_op_egress_getmore_inc();
      break;
   case MONGOC_OPCODE_REPLY:
      mongoc_counter_op_egress_reply_inc();
      break;
   case MONGOC_OPCODE_MSG:
      mongoc_counter_op_egress_msg_inc();
      break;
   case MONGOC_OPCODE_QUERY:
      mongoc_counter_op_egress_query_inc();
      break;
   default:
      BSON_ASSERT(false);
      break;
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_inc_ingress_rpc --
 *
 *       Helper to increment the counter for a particular RPC based on
 *       it's opcode.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static BSON_INLINE void
_mongoc_cluster_inc_ingress_rpc (const mongoc_rpc_t *rpc)
{
   mongoc_counter_op_ingress_total_inc ();

   switch (rpc->header.opcode) {
   case MONGOC_OPCODE_DELETE:
      mongoc_counter_op_ingress_delete_inc ();
      break;
   case MONGOC_OPCODE_UPDATE:
      mongoc_counter_op_ingress_update_inc ();
      break;
   case MONGOC_OPCODE_INSERT:
      mongoc_counter_op_ingress_insert_inc ();
      break;
   case MONGOC_OPCODE_KILL_CURSORS:
      mongoc_counter_op_ingress_killcursors_inc ();
      break;
   case MONGOC_OPCODE_GET_MORE:
      mongoc_counter_op_ingress_getmore_inc ();
      break;
   case MONGOC_OPCODE_REPLY:
      mongoc_counter_op_ingress_reply_inc ();
      break;
   case MONGOC_OPCODE_MSG:
      mongoc_counter_op_ingress_msg_inc ();
      break;
   case MONGOC_OPCODE_QUERY:
      mongoc_counter_op_ingress_query_inc ();
      break;
   default:
      BSON_ASSERT (false);
      break;
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_sendv --
 *
 *       Deliver an RPC to the MongoDB server.
 *
 *       If @hint is non-zero, the connection matching that hint will be
 *       used or the operation will fail. This is primarily used to force
 *       sending an RPC on the same connection as a previous RPC. This
 *       is often the case with OP_QUERY followed by OP_GETMORE.
 *
 *       @rpcs should be an array of mongoc_rpc_t that have not yet been
 *       gathered or swab'ed. The state of @rpcs is undefined after calling
 *       this function and should not be used afterwards.
 *
 *       @write_concern is optional. Providing it may cause this function
 *       to block until an operation has completed on the remote MongoDB
 *       server.
 *
 *       @read_prefs is optional and can be used to dictate which machines
 *       may be used to perform a query upon.
 *
 *       This function will continue to try to deliver an RPC until
 *       successful or the retry count has surprased.
 *
 * Returns:
 *       Zero on failure. A non-zero value is the hint of the connection
 *       that was used to communicate with a remote MongoDB server. This
 *       value may be passed as @hint in future calls to use the same
 *       connection.
 *
 *       If the result is zero, then @error will be set with information
 *       about the failure.
 *
 * Side effects:
 *       @rpcs may be muted and should be considered invalid after calling
 *       this function.
 *
 *       @error may be set.
 *
 *--------------------------------------------------------------------------
 */

uint32_t
_mongoc_cluster_sendv (mongoc_cluster_t             *cluster,
                       mongoc_rpc_t                 *rpcs,
                       size_t                        rpcs_len,
                       uint32_t                 hint,
                       const mongoc_write_concern_t *write_concern,
                       const mongoc_read_prefs_t    *read_prefs,
                       bson_error_t                 *error)
{
   mongoc_cluster_node_t *node;
   mongoc_iovec_t *iov;
   const bson_t *b;
   mongoc_rpc_t gle;
   int64_t now;
   size_t iovcnt;
   size_t i;
   bool need_gle;
   char cmdname[140];
   int retry_count = 0;

   ENTRY;

   bson_return_val_if_fail(cluster, false);
   bson_return_val_if_fail(rpcs, false);
   bson_return_val_if_fail(rpcs_len, false);

   /*
    * If we are in an unhealthy state, and enough time has elapsed since
    * our last reconnection, go ahead and try to perform reconnection
    * immediately.
    */
   now = bson_get_monotonic_time();
   if ((cluster->state == MONGOC_CLUSTER_STATE_DEAD) ||
       ((cluster->state == MONGOC_CLUSTER_STATE_UNHEALTHY) &&
        (cluster->last_reconnect + UNHEALTHY_RECONNECT_TIMEOUT_USEC) <= now)) {
      if (!_mongoc_cluster_reconnect(cluster, error)) {
         RETURN(false);
      }
   }

   /*
    * Try to find a node to deliver to. Since we are allowed to block in this
    * version of sendv, we try to reconnect if we cannot select a node.
    */
   while (!(node = _mongoc_cluster_select (cluster, rpcs, rpcs_len, hint,
                                           write_concern, read_prefs,
                                           error))) {
      if ((retry_count++ == MAX_RETRY_COUNT) ||
          !_mongoc_cluster_reconnect (cluster, error)) {
         RETURN (false);
      }
   }

   BSON_ASSERT(node->stream);

   _mongoc_array_clear (&cluster->iov);

   /*
    * TODO: We can probably remove the need for sendv and just do send since
    * we support write concerns now. Also, we clobber our getlasterror on
    * each subsequent mutation. It's okay, since it comes out correct anyway,
    * just useless work (and technically the request_id changes).
    */

   for (i = 0; i < rpcs_len; i++) {
      _mongoc_cluster_inc_egress_rpc (&rpcs[i]);
      rpcs[i].header.request_id = ++cluster->request_id;
      need_gle = _mongoc_rpc_needs_gle(&rpcs[i], write_concern);
      _mongoc_rpc_gather (&rpcs[i], &cluster->iov);

	  if (rpcs[i].header.msg_len >(int32_t)cluster->max_msg_size) {
         bson_set_error(error,
                        MONGOC_ERROR_CLIENT,
                        MONGOC_ERROR_CLIENT_TOO_BIG,
                        "Attempted to send an RPC larger than the "
                        "max allowed message size. Was %u, allowed %u.",
                        rpcs[i].header.msg_len,
                        cluster->max_msg_size);
         RETURN(0);
      }

      if (need_gle) {
         gle.query.msg_len = 0;
         gle.query.request_id = ++cluster->request_id;
         gle.query.response_to = 0;
         gle.query.opcode = MONGOC_OPCODE_QUERY;
         gle.query.flags = MONGOC_QUERY_NONE;
         switch (rpcs[i].header.opcode) {
         case MONGOC_OPCODE_INSERT:
            DB_AND_CMD_FROM_COLLECTION(cmdname, rpcs[i].insert.collection);
            break;
         case MONGOC_OPCODE_DELETE:
            DB_AND_CMD_FROM_COLLECTION(cmdname, rpcs[i].delete.collection);
            break;
         case MONGOC_OPCODE_UPDATE:
            DB_AND_CMD_FROM_COLLECTION(cmdname, rpcs[i].update.collection);
            break;
         default:
            BSON_ASSERT(false);
            DB_AND_CMD_FROM_COLLECTION(cmdname, "admin.$cmd");
            break;
         }
         gle.query.collection = cmdname;
         gle.query.skip = 0;
         gle.query.n_return = 1;
         b = _mongoc_write_concern_get_gle((void*)write_concern);
         gle.query.query = bson_get_data(b);
         gle.query.fields = NULL;
         _mongoc_rpc_gather(&gle, &cluster->iov);
         _mongoc_rpc_swab_to_le(&gle);
      }

      _mongoc_rpc_swab_to_le(&rpcs[i]);
   }

   iov = cluster->iov.data;
   iovcnt = cluster->iov.len;
   errno = 0;

   BSON_ASSERT (cluster->iov.len);

   if (!mongoc_stream_writev (node->stream, iov, iovcnt,
                              cluster->sockettimeoutms)) {
      char buf[128];
      char * errstr;
      errstr = bson_strerror_r(errno, buf, sizeof buf);

      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_SOCKET,
                      "Failure during socket delivery: %s",
                      errstr);
      _mongoc_cluster_disconnect_node (cluster, node);
      RETURN (0);
   }

   RETURN (node->index + 1);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_try_sendv --
 *
 *       Deliver an RPC to a remote MongoDB instance.
 *
 *       This function is similar to _mongoc_cluster_sendv() except that it
 *       will not try to reconnect until a connection has been made.
 *
 *       This is useful if you want to fire-and-forget ignoring network
 *       errors. Kill Cursors would be a candidate for this.
 *
 * Returns:
 *       0 on failure and @error is set.
 *
 *       Non-zero on success. The return value is a hint for the
 *       connection that was used to communicate with the server.
 *
 * Side effects:
 *       @rpcs will be invalid after calling this function.
 *       @error may be set if 0 is returned.
 *
 *--------------------------------------------------------------------------
 */

uint32_t
_mongoc_cluster_try_sendv (mongoc_cluster_t             *cluster,
                           mongoc_rpc_t                 *rpcs,
                           size_t                        rpcs_len,
                           uint32_t                 hint,
                           const mongoc_write_concern_t *write_concern,
                           const mongoc_read_prefs_t    *read_prefs,
                           bson_error_t                 *error)
{
   mongoc_cluster_node_t *node;
   mongoc_iovec_t *iov;
   const bson_t *b;
   mongoc_rpc_t gle;
   bool need_gle;
   size_t iovcnt;
   size_t i;
   char cmdname[140];

   ENTRY;

   bson_return_val_if_fail(cluster, false);
   bson_return_val_if_fail(rpcs, false);
   bson_return_val_if_fail(rpcs_len, false);

   if (!(node = _mongoc_cluster_select(cluster, rpcs, rpcs_len, hint,
                                       write_concern, read_prefs, error))) {
      RETURN (0);
   }

   BSON_ASSERT (node->stream);

   _mongoc_array_clear (&cluster->iov);

   for (i = 0; i < rpcs_len; i++) {
      _mongoc_cluster_inc_egress_rpc (&rpcs[i]);
      rpcs[i].header.request_id = ++cluster->request_id;
      need_gle = _mongoc_rpc_needs_gle (&rpcs[i], write_concern);
      _mongoc_rpc_gather (&rpcs[i], &cluster->iov);

	  if (rpcs[i].header.msg_len >(int32_t)cluster->max_msg_size) {
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_TOO_BIG,
                         "Attempted to send an RPC larger than the "
                         "max allowed message size. Was %u, allowed %u.",
                         rpcs[i].header.msg_len,
                         cluster->max_msg_size);
         RETURN (0);
      }

      if (need_gle) {
         gle.query.msg_len = 0;
         gle.query.request_id = ++cluster->request_id;
         gle.query.response_to = 0;
         gle.query.opcode = MONGOC_OPCODE_QUERY;
         gle.query.flags = MONGOC_QUERY_NONE;

         switch (rpcs[i].header.opcode) {
         case MONGOC_OPCODE_INSERT:
            DB_AND_CMD_FROM_COLLECTION(cmdname, rpcs[i].insert.collection);
            break;
         case MONGOC_OPCODE_DELETE:
            DB_AND_CMD_FROM_COLLECTION(cmdname, rpcs[i].delete.collection);
            break;
         case MONGOC_OPCODE_UPDATE:
            gle.query.collection = rpcs[i].update.collection;
            DB_AND_CMD_FROM_COLLECTION(cmdname, rpcs[i].update.collection);
            break;
         default:
            BSON_ASSERT(false);
            DB_AND_CMD_FROM_COLLECTION(cmdname, "admin.$cmd");
            break;
         }

         gle.query.collection = cmdname;
         gle.query.skip = 0;
         gle.query.n_return = 1;

         b = _mongoc_write_concern_get_gle ((void *)write_concern);

         gle.query.query = bson_get_data (b);
         gle.query.fields = NULL;

         _mongoc_rpc_gather (&gle, &cluster->iov);
         _mongoc_rpc_swab_to_le (&gle);
      }

      _mongoc_rpc_swab_to_le (&rpcs[i]);
   }

   iov = cluster->iov.data;
   iovcnt = cluster->iov.len;
   errno = 0;

   DUMP_IOVEC (iov, iov, iovcnt);

   if (!mongoc_stream_writev (node->stream, iov, iovcnt,
                              cluster->sockettimeoutms)) {
      char buf[128];
      char * errstr;
      errstr = bson_strerror_r(errno, buf, sizeof buf);

      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_SOCKET,
                      "Failure during socket delivery: %s",
                      errstr);
      _mongoc_cluster_disconnect_node (cluster, node);
      RETURN (0);
   }

   RETURN(node->index + 1);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_try_recv --
 *
 *       Tries to receive the next event from the node in the cluster
 *       specified by @hint. The contents are loaded into @buffer and then
 *       scattered into the @rpc structure. @rpc is valid as long as
 *       @buffer contains the contents read into it.
 *
 *       Callers that can optimize a reuse of @buffer should do so. It
 *       can save many memory allocations.
 *
 * Returns:
 *       0 on failure and @error is set.
 *       non-zero on success where the value is the hint of the connection
 *       that was used.
 *
 * Side effects:
 *       @error if return value is zero.
 *       @rpc is set if result is non-zero.
 *       @buffer will be filled with the input data.
 *
 *--------------------------------------------------------------------------
 */

bool
_mongoc_cluster_try_recv (mongoc_cluster_t *cluster,
                          mongoc_rpc_t     *rpc,
                          mongoc_buffer_t  *buffer,
                          uint32_t          hint,
                          bson_error_t     *error)
{
   mongoc_cluster_node_t *node;
   int32_t msg_len;
   off_t pos;

   ENTRY;

   bson_return_val_if_fail (cluster, false);
   bson_return_val_if_fail (rpc, false);
   bson_return_val_if_fail (buffer, false);
   bson_return_val_if_fail (hint, false);
   bson_return_val_if_fail (hint <= MONGOC_CLUSTER_MAX_NODES, false);

   /*
    * Fetch the node to communicate over.
    */
   node = &cluster->nodes[hint-1];
   if (!node->stream) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_NOT_READY,
                      "Failed to receive message, lost connection to node.");
      RETURN (false);
   }

   TRACE ("Waiting for reply from \"%s\"", node->host.host_and_port);

   /*
    * Buffer the message length to determine how much more to read.
    */
   pos = buffer->len;
   if (!_mongoc_buffer_append_from_stream (buffer, node->stream, 4,
                                           cluster->sockettimeoutms, error)) {
      mongoc_counter_protocol_ingress_error_inc ();
      _mongoc_cluster_disconnect_node (cluster, node);
      RETURN (false);
   }

   /*
    * Read the msg length from the buffer.
    */
   memcpy (&msg_len, &buffer->data[buffer->off + pos], 4);
   msg_len = BSON_UINT32_FROM_LE (msg_len);
   if ((msg_len < 16) || (msg_len > cluster->max_msg_size)) {
      bson_set_error (error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Corrupt or malicious reply received.");
      _mongoc_cluster_disconnect_node (cluster, node);
      mongoc_counter_protocol_ingress_error_inc ();
      RETURN (false);
   }

   /*
    * Read the rest of the message from the stream.
    */
   if (!_mongoc_buffer_append_from_stream (buffer, node->stream, msg_len - 4,
                                           cluster->sockettimeoutms, error)) {
      _mongoc_cluster_disconnect_node (cluster, node);
      mongoc_counter_protocol_ingress_error_inc ();
      RETURN (false);
   }

   /*
    * Scatter the buffer into the rpc structure.
    */
   if (!_mongoc_rpc_scatter (rpc, &buffer->data[buffer->off + pos], msg_len)) {
      bson_set_error (error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Failed to decode reply from server.");
      _mongoc_cluster_disconnect_node (cluster, node);
      mongoc_counter_protocol_ingress_error_inc ();
      RETURN (false);
   }

   DUMP_BYTES (buffer, buffer->data + buffer->off, buffer->len);

   _mongoc_rpc_swab_from_le (rpc);

   _mongoc_cluster_inc_ingress_rpc (rpc);

   RETURN(true);
}


/**
 * _mongoc_cluster_stamp:
 * @cluster: A mongoc_cluster_t.
 * @node: The node identifier.
 *
 * Returns the stamp of the node provided. The stamp is a monotonic counter
 * that tracks changes to a node within the cluster. As changes to the node
 * instance are made, the value is incremented. This helps cursors and other
 * connection sensitive portions fail gracefully (or reset) upon loss of
 * connection.
 *
 * Returns: A 32-bit stamp indiciating the node version.
 */
uint32_t
_mongoc_cluster_stamp (const mongoc_cluster_t *cluster,
                       uint32_t           node)
{
   bson_return_val_if_fail(cluster, 0);
   bson_return_val_if_fail(node > 0, 0);
   bson_return_val_if_fail(node <= MONGOC_CLUSTER_MAX_NODES, 0);

   return cluster->nodes[node].stamp;
}


/**
 * _mongoc_cluster_get_primary:
 * @cluster: A #mongoc_cluster_t.
 *
 * Fetches the node we currently believe is PRIMARY.
 *
 * Returns: A #mongoc_cluster_node_t or %NULL.
 */
mongoc_cluster_node_t *
_mongoc_cluster_get_primary (mongoc_cluster_t *cluster)
{
   uint32_t i;

   BSON_ASSERT (cluster);

   for (i = 0; i < MONGOC_CLUSTER_MAX_NODES; i++) {
      if (cluster->nodes[i].primary) {
         return &cluster->nodes[i];
      }
   }

   return NULL;
}

/*==============================================================*/
/* --- mongoc-collection.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "collection"


static
BSON_GNUC_PURE
bool
validate_name (const char *str)
{
   const char *c;

   if (str && *str) {
      for (c = str; *c; c++) {
         switch (*c) {
         case '/':
         case '\\':
         case '.':
         case '"':
         case '*':
         case '<':
         case '>':
         case ':':
         case '|':
         case '?':
            return false;
         default:
            break;
         }
      }
      return ((0 != strcmp (str, "oplog.$main")) &&
              (0 != strcmp (str, "$cmd")));
   }

   return false;
}


static uint32_t
_mongoc_collection_preselect (mongoc_collection_t          *collection,
                              mongoc_opcode_t               opcode,
                              const mongoc_write_concern_t *write_concern,
                              const mongoc_read_prefs_t    *read_prefs,
                              uint32_t                     *min_wire_version,
                              uint32_t                     *max_wire_version,
                              bson_error_t                 *error)
{
   mongoc_cluster_node_t *node;
   uint32_t hint;

   BSON_ASSERT (collection);
   BSON_ASSERT (opcode);
   BSON_ASSERT (min_wire_version);
   BSON_ASSERT (max_wire_version);

   /*
    * Try to discover the wire version of the server. Default to 1 so
    * we can return a valid cursor structure.
    */

   *min_wire_version = 0;
   *max_wire_version = 1;

   hint = _mongoc_client_preselect (collection->client,
                                    opcode,
                                    write_concern,
                                    read_prefs,
                                    error);

   if (hint) {
      node = &collection->client->cluster.nodes [hint - 1];
      *min_wire_version = node->min_wire_version;
      *max_wire_version = node->max_wire_version;
   }

   return hint;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_collection_new --
 *
 *       INTERNAL API
 *
 *       Create a new mongoc_collection_t structure for the given client.
 *
 *       @client must remain valid during the lifetime of this structure.
 *       @db is the db name of the collection.
 *       @collection is the name of the collection.
 *       @read_prefs is the default read preferences to apply or NULL.
 *       @write_concern is the default write concern to apply or NULL.
 *
 * Returns:
 *       A newly allocated mongoc_collection_t that should be freed with
 *       mongoc_collection_destroy().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_collection_t *
_mongoc_collection_new (mongoc_client_t              *client,
                        const char                   *db,
                        const char                   *collection,
                        const mongoc_read_prefs_t    *read_prefs,
                        const mongoc_write_concern_t *write_concern)
{
   mongoc_collection_t *col;

   ENTRY;

   bson_return_val_if_fail(client, NULL);
   bson_return_val_if_fail(db, NULL);
   bson_return_val_if_fail(collection, NULL);

   col = bson_malloc0(sizeof *col);
   col->client = client;
   col->write_concern = write_concern ?
      mongoc_write_concern_copy(write_concern) :
      mongoc_write_concern_new();
   col->read_prefs = read_prefs ?
      mongoc_read_prefs_copy(read_prefs) :
      mongoc_read_prefs_new(MONGOC_READ_PRIMARY);

   bson_snprintf (col->ns, sizeof col->ns, "%s.%s", db, collection);
   bson_snprintf (col->db, sizeof col->db, "%s", db);
   bson_snprintf (col->collection, sizeof col->collection, "%s", collection);

   col->collectionlen = (uint32_t)strlen(col->collection);
   col->nslen = (uint32_t)strlen(col->ns);

   _mongoc_buffer_init(&col->buffer, NULL, 0, NULL, NULL);

   col->gle = NULL;

   RETURN(col);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_destroy --
 *
 *       Release resources associated with @collection and frees the
 *       structure.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Everything.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_collection_destroy (mongoc_collection_t *collection) /* IN */
{
   ENTRY;

   bson_return_if_fail(collection);

   bson_clear (&collection->gle);

   _mongoc_buffer_destroy(&collection->buffer);

   if (collection->read_prefs) {
      mongoc_read_prefs_destroy(collection->read_prefs);
      collection->read_prefs = NULL;
   }

   if (collection->write_concern) {
      mongoc_write_concern_destroy(collection->write_concern);
      collection->write_concern = NULL;
   }

   bson_free(collection);

   EXIT;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_aggregate --
 *
 *       Send an "aggregate" command to the MongoDB server.
 *
 *       This varies it's behavior based on the wire version.  If we're on
 *       wire_version > 0, we use the new aggregate command, which returns a
 *       database cursor.  On wire_version == 0, we create synthetic cursor on
 *       top of the array returned in result.
 *
 *       This function will always return a new mongoc_cursor_t that should
 *       be freed with mongoc_cursor_destroy().
 *
 *       The cursor may fail once iterated upon, so check
 *       mongoc_cursor_error() if mongoc_cursor_next() returns false.
 *
 *       See http://docs.mongodb.org/manual/aggregation/ for more
 *       information on how to build aggregation pipelines.
 *
 * Requires:
 *       MongoDB >= 2.1.0
 *
 * Parameters:
 *       @flags: bitwise or of mongoc_query_flags_t or 0.
 *       @pipeline: A bson_t containing the pipeline request. @pipeline
 *                  will be sent as an array type in the request.
 *       @read_prefs: Optional read preferences for the command.
 *
 * Returns:
 *       A newly allocated mongoc_cursor_t that should be freed with
 *       mongoc_cursor_destroy().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_cursor_t *
mongoc_collection_aggregate (mongoc_collection_t       *collection, /* IN */
                             mongoc_query_flags_t       flags,      /* IN */
                             const bson_t              *pipeline,   /* IN */
                             const bson_t              *options,    /* IN */
                             const mongoc_read_prefs_t *read_prefs) /* IN */
{
   mongoc_cursor_t *cursor;
   bson_iter_t iter;
   uint32_t max_wire_version = 0;
   uint32_t min_wire_version = 0;
   uint32_t hint;
   bson_t command;
   bson_t child;
   int32_t batch_size = 0;
   bool did_batch_size = false;

   bson_return_val_if_fail (collection, NULL);
   bson_return_val_if_fail (pipeline, NULL);

   hint = _mongoc_collection_preselect (collection,
                                        MONGOC_OPCODE_QUERY,
                                        NULL,
                                        read_prefs,
                                        &min_wire_version,
                                        &max_wire_version,
                                        NULL);

   bson_init (&command);
   BSON_APPEND_UTF8 (&command, "aggregate", collection->collection);

   /*
    * The following will allow @pipeline to be either an array of
    * items for the pipeline, or {"pipeline": [...]}.
    */
   if (bson_iter_init_find (&iter, pipeline, "pipeline") &&
       BSON_ITER_HOLDS_ARRAY (&iter)) {
      bson_append_iter (&command, "pipeline", 8, &iter);
   } else {
      BSON_APPEND_ARRAY (&command, "pipeline", pipeline);
   }

   /* for newer version, we include a cursor subdocument */
   if (max_wire_version) {
      bson_append_document_begin (&command, "cursor", 6, &child);

      if (options && bson_iter_init (&iter, options)) {
         while (bson_iter_next (&iter)) {
            if (BSON_ITER_IS_KEY (&iter, "batchSize") &&
                (BSON_ITER_HOLDS_INT32 (&iter) ||
                 BSON_ITER_HOLDS_INT64 (&iter) ||
                 BSON_ITER_HOLDS_DOUBLE (&iter))) {
               did_batch_size = true;
               batch_size = (int32_t)bson_iter_as_int64 (&iter);
               BSON_APPEND_INT32 (&child, "batchSize", batch_size);
            }
         }
      }

      if (!did_batch_size) {
         BSON_APPEND_INT32 (&child, "batchSize", 100);
      }

      bson_append_document_end (&command, &child);
   }

   if (options && bson_iter_init (&iter, options)) {
      while (bson_iter_next (&iter)) {
         if (! (BSON_ITER_IS_KEY (&iter, "batchSize") || BSON_ITER_IS_KEY (&iter, "cursor"))) {
            bson_append_iter (&command, bson_iter_key (&iter), -1, &iter);
         }
      }
   }

   cursor = mongoc_collection_command (collection, flags, 0, 0, batch_size,
                                       &command, NULL, read_prefs);

   cursor->hint = hint;

   if (max_wire_version > 0) {
      /* even for newer versions, we get back a cursor document, that we have
       * to patch in */
      _mongoc_cursor_cursorid_init(cursor);
      cursor->limit = 0;
   } else {
      /* for older versions we get an array that we can create a synthetic
       * cursor on top of */
      _mongoc_cursor_array_init(cursor);
      cursor->limit = 0;
   }

   bson_destroy(&command);

   return cursor;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_find --
 *
 *       Performs a query against the configured MongoDB server. If @read_prefs
 *       is provided, it will be used to locate a MongoDB node in the cluster
 *       to deliver the query to.
 *
 *       @flags may be bitwise-or'd flags or MONGOC_QUERY_NONE.
 *
 *       @skip may contain the number of documents to skip before returning the
 *       matching document.
 *
 *       @limit may contain the maximum number of documents that may be
 *       returned.
 *
 *       This function will always return a cursor, with the exception of
 *       invalid API use.
 *
 * Parameters:
 *       @collection: A mongoc_collection_t.
 *       @flags: A bitwise or of mongoc_query_flags_t.
 *       @skip: The number of documents to skip.
 *       @limit: The maximum number of items.
 *       @batch_size: The batch size
 *       @query: The query to locate matching documents.
 *       @fields: The fields to return, or NULL for all fields.
 *       @read_prefs: Read preferences to choose cluster node.
 *
 * Returns:
 *       A newly allocated mongoc_cursor_t that should be freed with
 *       mongoc_cursor_destroy().
 *
 *       The client used by mongoc_collection_t must be valid for the
 *       lifetime of the resulting mongoc_cursor_t.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_cursor_t *
mongoc_collection_find (mongoc_collection_t       *collection, /* IN */
                        mongoc_query_flags_t       flags,      /* IN */
                        uint32_t                   skip,       /* IN */
                        uint32_t                   limit,      /* IN */
                        uint32_t                   batch_size, /* IN */
                        const bson_t              *query,      /* IN */
                        const bson_t              *fields,     /* IN */
                        const mongoc_read_prefs_t *read_prefs) /* IN */
{
   bson_return_val_if_fail(collection, NULL);
   bson_return_val_if_fail(query, NULL);

   bson_clear (&collection->gle);

   if (!read_prefs) {
      read_prefs = collection->read_prefs;
   }

   return _mongoc_cursor_new(collection->client, collection->ns, flags, skip,
                             limit, batch_size, false, query, fields, read_prefs);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_command --
 *
 *       Executes a command on a cluster node matching @read_prefs. If
 *       @read_prefs is not provided, it will be run on the primary node.
 *
 *       This function will always return a mongoc_cursor_t.
 *
 * Parameters:
 *       @collection: A mongoc_collection_t.
 *       @flags: Bitwise-or'd flags for command.
 *       @skip: Number of documents to skip, typically 0.
 *       @limit : Number of documents to return
 *       @batch_size : Batch size
 *       @query: The command to execute.
 *       @fields: The fields to return, or NULL.
 *       @read_prefs: Command read preferences or NULL.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_cursor_t *
mongoc_collection_command (mongoc_collection_t       *collection,
                           mongoc_query_flags_t       flags,
                           uint32_t                   skip,
                           uint32_t                   limit,
                           uint32_t                   batch_size,
                           const bson_t              *query,
                           const bson_t              *fields,
                           const mongoc_read_prefs_t *read_prefs)
{
   char ns[MONGOC_NAMESPACE_MAX];

   BSON_ASSERT (collection);
   BSON_ASSERT (query);

   if (!read_prefs) {
      read_prefs = collection->read_prefs;
   }

   bson_clear (&collection->gle);

   if (NULL == strstr (collection->collection, "$cmd")) {
      bson_snprintf (ns, sizeof ns, "%s", collection->db);
   } else {
      bson_snprintf (ns, sizeof ns, "%s.%s",
                     collection->db, collection->collection);
   }

   return mongoc_client_command (collection->client, ns, flags,
                                 skip, limit, batch_size, query, fields, read_prefs);
}

bool
mongoc_collection_command_simple (mongoc_collection_t       *collection,
                                  const bson_t              *command,
                                  const mongoc_read_prefs_t *read_prefs,
                                  bson_t                    *reply,
                                  bson_error_t              *error)
{
   BSON_ASSERT (collection);
   BSON_ASSERT (command);

   bson_clear (&collection->gle);

   return mongoc_client_command_simple (collection->client, collection->db,
                                        command, read_prefs, reply, error);
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_count --
 *
 *       Count the number of documents matching @query.
 *
 * Parameters:
 *       @flags: A mongoc_query_flags_t describing the query flags or 0.
 *       @query: The query to perform or NULL for {}.
 *       @skip: The $skip to perform within the query or 0.
 *       @limit: The $limit to perform within the query or 0.
 *       @read_prefs: desired read preferences or NULL.
 *       @error: A location for an error or NULL.
 *
 * Returns:
 *       -1 on failure; otherwise the number of matching documents.
 *
 * Side effects:
 *       @error is set upon failure if non-NULL.
 *
 *--------------------------------------------------------------------------
 */

int64_t
mongoc_collection_count (mongoc_collection_t       *collection,  /* IN */
                         mongoc_query_flags_t       flags,       /* IN */
                         const bson_t              *query,       /* IN */
                         int64_t               skip,        /* IN */
                         int64_t               limit,       /* IN */
                         const mongoc_read_prefs_t *read_prefs,  /* IN */
                         bson_error_t              *error)       /* OUT */
{
   int64_t ret = -1;
   bson_iter_t iter;
   bson_t reply;
   bson_t cmd;
   bson_t q;

   bson_return_val_if_fail(collection, -1);

   bson_init(&cmd);
   bson_append_utf8(&cmd, "count", 5, collection->collection,
                    collection->collectionlen);
   if (query) {
      bson_append_document(&cmd, "query", 5, query);
   } else {
      bson_init(&q);
      bson_append_document(&cmd, "query", 5, &q);
      bson_destroy(&q);
   }
   if (limit) {
      bson_append_int64(&cmd, "limit", 5, limit);
   }
   if (skip) {
      bson_append_int64(&cmd, "skip", 4, skip);
   }
   if (mongoc_collection_command_simple(collection, &cmd, read_prefs,
                                        &reply, error) &&
       bson_iter_init_find(&iter, &reply, "n")) {
      ret = bson_iter_as_int64(&iter);
   }
   bson_destroy(&reply);
   bson_destroy(&cmd);

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_drop --
 *
 *       Request the MongoDB server drop the collection.
 *
 * Returns:
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       @error is set upon failure.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_collection_drop (mongoc_collection_t *collection, /* IN */
                        bson_error_t        *error)      /* OUT */
{
   bool ret;
   bson_t cmd;

   bson_return_val_if_fail(collection, false);

   bson_init(&cmd);
   bson_append_utf8(&cmd, "drop", 4, collection->collection,
                    collection->collectionlen);
   ret = mongoc_collection_command_simple(collection, &cmd, NULL, NULL, error);
   bson_destroy(&cmd);

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_drop_index --
 *
 *       Request the MongoDB server drop the named index.
 *
 * Returns:
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       @error is setup upon failure if non-NULL.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_collection_drop_index (mongoc_collection_t *collection, /* IN */
                              const char          *index_name, /* IN */
                              bson_error_t        *error)      /* OUT */
{
   bool ret;
   bson_t cmd;

   bson_return_val_if_fail(collection, false);
   bson_return_val_if_fail(index_name, false);

   bson_init(&cmd);
   bson_append_utf8(&cmd, "dropIndexes", -1, collection->collection,
                    collection->collectionlen);
   bson_append_utf8(&cmd, "index", -1, index_name, -1);
   ret = mongoc_collection_command_simple(collection, &cmd, NULL, NULL, error);
   bson_destroy(&cmd);

   return ret;
}


char *
mongoc_collection_keys_to_index_string (const bson_t *keys)
{
   bson_string_t *s;
   bson_iter_t iter;
   int i = 0;

   BSON_ASSERT (keys);

   if (!bson_iter_init (&iter, keys)) {
      return NULL;
   }

   s = bson_string_new (NULL);

   while (bson_iter_next (&iter)) {
      bson_string_append_printf (s,
                                 (i++ ? "_%s_%d" : "%s_%d"),
                                 bson_iter_key (&iter),
                                 bson_iter_int32 (&iter));
   }

   return bson_string_free (s, false);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_collection_create_index_legacy --
 *
 *       Request the MongoDB server create the named index.
 *
 * Returns:
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       @error is setup upon failure if non-NULL.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_collection_create_index_legacy (mongoc_collection_t      *collection,
                                        const bson_t             *keys,
                                        const mongoc_index_opt_t *opt,
                                        bson_error_t             *error)
{
   const mongoc_index_opt_t *def_opt;
   mongoc_collection_t *col;
   bool ret;
   bson_t insert;
   char *name;

   bson_return_val_if_fail (collection, false);

   /*
    * TODO: this is supposed to be cached and cheap... make it that way
    */

   def_opt = mongoc_index_opt_get_default ();
   opt = opt ? opt : def_opt;

   if (!opt->is_initialized) {
      MONGOC_WARNING("Options have not yet been initialized");
      return false;
   }

   bson_init (&insert);

   bson_append_document (&insert, "key", -1, keys);
   bson_append_utf8 (&insert, "ns", -1, collection->ns, -1);

   if (opt->background != def_opt->background) {
      bson_append_bool (&insert, "background", -1, opt->background);
   }

   if (opt->unique != def_opt->unique) {
      bson_append_bool (&insert, "unique", -1, opt->unique);
   }

   if (opt->name != def_opt->name) {
      bson_append_utf8 (&insert, "name", -1, opt->name, -1);
   } else {
      name = mongoc_collection_keys_to_index_string(keys);
      bson_append_utf8 (&insert, "name", -1, name, -1);
      bson_free (name);
   }

   if (opt->drop_dups != def_opt->drop_dups) {
      bson_append_bool (&insert, "dropDups", -1, opt->drop_dups);
   }

   if (opt->sparse != def_opt->sparse) {
      bson_append_bool (&insert, "sparse", -1, opt->sparse);
   }

   if (opt->expire_after_seconds != def_opt->expire_after_seconds) {
      bson_append_int32 (&insert,
                         "expireAfterSeconds", -1,
                         opt->expire_after_seconds);
   }

   if (opt->v != def_opt->v) {
      bson_append_int32 (&insert, "v", -1, opt->v);
   }

   if (opt->weights != def_opt->weights) {
      bson_append_document (&insert, "weights", -1, opt->weights);
   }

   if (opt->default_language != def_opt->default_language) {
      bson_append_utf8 (&insert,
                        "defaultLanguage", -1,
                        opt->default_language, -1);
   }

   if (opt->language_override != def_opt->language_override) {
      bson_append_utf8 (&insert,
                        "languageOverride", -1,
                        opt->language_override, -1);
   }

   col = mongoc_client_get_collection (collection->client, collection->db,
                                       "system.indexes");

   ret = mongoc_collection_insert (col, MONGOC_INSERT_NO_VALIDATE, &insert, NULL,
                                   error);

   mongoc_collection_destroy(col);

   bson_destroy (&insert);

   return ret;
}


bool
mongoc_collection_create_index (mongoc_collection_t      *collection,
                                const bson_t             *keys,
                                const mongoc_index_opt_t *opt,
                                bson_error_t             *error)
{
   const mongoc_index_opt_t *def_opt;
   bson_error_t local_error;
   const char *name;
   bson_t cmd = BSON_INITIALIZER;
   bson_t ar;
   bson_t doc;
   bson_t reply;
   char *alloc_name = NULL;
   bool ret = false;

   bson_return_val_if_fail (collection, false);
   bson_return_val_if_fail (keys, false);

   def_opt = mongoc_index_opt_get_default ();
   opt = opt ? opt : def_opt;

   /*
    * Generate the key name if it was not provided.
    */
   name = (opt->name != def_opt->name) ? opt->name : NULL;
   if (!name) {
      alloc_name = mongoc_collection_keys_to_index_string (keys);
      name = alloc_name;
   }

   /*
    * Build our createIndexes command to send to the server.
    */
   BSON_APPEND_UTF8 (&cmd, "createIndexes", collection->collection);
   bson_append_array_begin (&cmd, "indexes", 7, &ar);
   bson_append_document_begin (&ar, "0", 1, &doc);
   BSON_APPEND_DOCUMENT (&doc, "key", keys);
   BSON_APPEND_UTF8 (&doc, "name", name);
   if (opt->background) {
      BSON_APPEND_BOOL (&doc, "background", true);
   }
   if (opt->unique) {
      BSON_APPEND_BOOL (&doc, "unique", true);
   }
   if (opt->drop_dups) {
      BSON_APPEND_BOOL (&doc, "dropDups", true);
   }
   if (opt->sparse) {
      BSON_APPEND_BOOL (&doc, "sparse", true);
   }
   if (opt->expire_after_seconds != def_opt->expire_after_seconds) {
      BSON_APPEND_INT32 (&doc, "expireAfterSeconds", opt->expire_after_seconds);
   }
   if (opt->v != def_opt->v) {
      BSON_APPEND_INT32 (&doc, "v", opt->v);
   }
   if (opt->weights && (opt->weights != def_opt->weights)) {
      BSON_APPEND_DOCUMENT (&doc, "weights", opt->weights);
   }
   if (opt->default_language != def_opt->default_language) {
      BSON_APPEND_UTF8 (&doc, "default_language", opt->default_language);
   }
   if (opt->language_override != def_opt->language_override) {
      BSON_APPEND_UTF8 (&doc, "language_override", opt->language_override);
   }
   bson_append_document_end (&ar, &doc);
   bson_append_array_end (&cmd, &ar);

   ret = mongoc_collection_command_simple (collection, &cmd, NULL, &reply,
                                           &local_error);

   /*
    * If we failed due to the command not being found, then use the legacy
    * version which performs an insert into the system.indexes collection.
    */
   if (!ret) {
      if (local_error.code == MONGOC_ERROR_QUERY_COMMAND_NOT_FOUND) {
         ret = _mongoc_collection_create_index_legacy (collection, keys, opt,
                                                       error);
      } else if (error) {
         memcpy (error, &local_error, sizeof *error);
      }
   }

   bson_destroy (&cmd);
   bson_destroy (&reply);
   bson_free (alloc_name);

   return ret;
}


bool
mongoc_collection_ensure_index (mongoc_collection_t      *collection,
                                const bson_t             *keys,
                                const mongoc_index_opt_t *opt,
                                bson_error_t             *error)
{
   return mongoc_collection_create_index (collection, keys, opt, error);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_insert_bulk --
 *
 *       Bulk insert documents into a MongoDB collection.
 *
 * Parameters:
 *       @collection: A mongoc_collection_t.
 *       @flags: flags for the insert or 0.
 *       @documents: The documents to insert.
 *       @n_documents: The number of documents to insert.
 *       @write_concern: A write concern or NULL.
 *       @error: a location for an error or NULL.
 *
 * Returns:
 *       true if successful; otherwise false and @error is set.
 *
 *       If the write concern does not dictate checking the result of the
 *       insert, then true may be returned even though the document was
 *       not actually inserted on the MongoDB server or cluster.
 *
 * Side effects:
 *       @collection->gle is setup, depending on write_concern->w value.
 *       @error may be set upon failure if non-NULL.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_collection_insert_bulk (mongoc_collection_t           *collection,
                               mongoc_insert_flags_t          flags,
                               const bson_t                 **documents,
                               uint32_t                       n_documents,
                               const mongoc_write_concern_t  *write_concern,
                               bson_error_t                  *error)
{
   mongoc_write_command_t command;
   mongoc_write_result_t result;
   bool ordered;
   bool ret;

   bson_return_val_if_fail (collection, false);
   bson_return_val_if_fail (documents, false);

   if (!write_concern) {
      write_concern = collection->write_concern;
   }

   bson_clear (&collection->gle);

   _mongoc_write_result_init (&result);

   ordered = !(flags & MONGOC_INSERT_CONTINUE_ON_ERROR);
   _mongoc_write_command_init_insert (&command, documents, n_documents,
                                      ordered, true);

   _mongoc_write_command_execute (&command, collection->client, 0,
                                  collection->db, collection->collection,
                                  write_concern, &result);

   collection->gle = bson_new ();
   ret = _mongoc_write_result_complete (&result, collection->gle, error);

   _mongoc_write_result_destroy (&result);
   _mongoc_write_command_destroy (&command);

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_insert --
 *
 *       Insert a document into a MongoDB collection.
 *
 * Parameters:
 *       @collection: A mongoc_collection_t.
 *       @flags: flags for the insert or 0.
 *       @document: The document to insert.
 *       @write_concern: A write concern or NULL.
 *       @error: a location for an error or NULL.
 *
 * Returns:
 *       true if successful; otherwise false and @error is set.
 *
 *       If the write concern does not dictate checking the result of the
 *       insert, then true may be returned even though the document was
 *       not actually inserted on the MongoDB server or cluster.
 *
 * Side effects:
 *       @collection->gle is setup, depending on write_concern->w value.
 *       @error may be set upon failure if non-NULL.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_collection_insert (mongoc_collection_t          *collection,
                          mongoc_insert_flags_t         flags,
                          const bson_t                 *document,
                          const mongoc_write_concern_t *write_concern,
                          bson_error_t                 *error)
{
   mongoc_write_command_t command;
   mongoc_write_result_t result;
   bool ret;

   ENTRY;

   bson_return_val_if_fail (collection, false);
   bson_return_val_if_fail (document, false);

   bson_clear (&collection->gle);

   if (!write_concern) {
      write_concern = collection->write_concern;
   }

   if (!(flags & MONGOC_INSERT_NO_VALIDATE)) {
      if (!bson_validate (document,
                          (BSON_VALIDATE_UTF8 |
                           BSON_VALIDATE_UTF8_ALLOW_NULL |
                           BSON_VALIDATE_DOLLAR_KEYS |
                           BSON_VALIDATE_DOT_KEYS),
                          NULL)) {
         bson_set_error (error,
                         MONGOC_ERROR_BSON,
                         MONGOC_ERROR_BSON_INVALID,
                         "A document was corrupt or contained "
                         "invalid characters . or $");
         RETURN (false);
      }
   } else {
      flags &= ~MONGOC_INSERT_NO_VALIDATE;
   }

   _mongoc_write_result_init (&result);
   _mongoc_write_command_init_insert (&command, &document, 1, true, false);

   _mongoc_write_command_execute (&command, collection->client, 0,
                                  collection->db, collection->collection,
                                  write_concern, &result);

   collection->gle = bson_new ();
   ret = _mongoc_write_result_complete (&result, collection->gle, error);

   _mongoc_write_result_destroy (&result);
   _mongoc_write_command_destroy (&command);

   RETURN (ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_update --
 *
 *       Updates one or more documents matching @selector with @update.
 *
 * Parameters:
 *       @collection: A mongoc_collection_t.
 *       @flags: The flags for the update.
 *       @selector: A bson_t containing your selector.
 *       @update: A bson_t containing your update document.
 *       @write_concern: The write concern or NULL.
 *       @error: A location for an error or NULL.
 *
 * Returns:
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       @collection->gle is setup, depending on write_concern->w value.
 *       @error is setup upon failure.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_collection_update (mongoc_collection_t          *collection,
                          mongoc_update_flags_t         flags,
                          const bson_t                 *selector,
                          const bson_t                 *update,
                          const mongoc_write_concern_t *write_concern,
                          bson_error_t                 *error)
{
   mongoc_write_command_t command;
   mongoc_write_result_t result;
   bson_iter_t iter;
   size_t err_offset;
   bool ret;

   ENTRY;

   bson_return_val_if_fail(collection, false);
   bson_return_val_if_fail(selector, false);
   bson_return_val_if_fail(update, false);

   bson_clear (&collection->gle);

   if (!((uint32_t)flags & MONGOC_UPDATE_NO_VALIDATE) &&
       bson_iter_init (&iter, update) &&
       bson_iter_next (&iter) &&
       (bson_iter_key (&iter) [0] != '$') &&
       !bson_validate (update,
                       (BSON_VALIDATE_UTF8 |
                        BSON_VALIDATE_UTF8_ALLOW_NULL |
                        BSON_VALIDATE_DOLLAR_KEYS |
                        BSON_VALIDATE_DOT_KEYS),
                       &err_offset)) {
      bson_set_error (error,
                      MONGOC_ERROR_BSON,
                      MONGOC_ERROR_BSON_INVALID,
                      "update document is corrupt or contains "
                      "invalid keys including $ or .");
      return false;
   } else {
      flags = (uint32_t)flags & ~MONGOC_UPDATE_NO_VALIDATE;
   }

   _mongoc_write_result_init (&result);
   _mongoc_write_command_init_update (&command,
                                      selector,
                                      update,
                                      !!(flags & MONGOC_UPDATE_UPSERT),
                                      !!(flags & MONGOC_UPDATE_MULTI_UPDATE),
                                      true);

   _mongoc_write_command_execute (&command, collection->client, 0,
                                  collection->db, collection->collection,
                                  write_concern, &result);

   collection->gle = bson_new ();
   ret = _mongoc_write_result_complete (&result, collection->gle, error);

   _mongoc_write_result_destroy (&result);
   _mongoc_write_command_destroy (&command);

   RETURN (ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_save --
 *
 *       Save @document to @collection.
 *
 *       If the document has an _id field, it will be updated. Otherwise,
 *       the document will be inserted into the collection.
 *
 * Returns:
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       @error is set upon failure if non-NULL.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_collection_save (mongoc_collection_t          *collection,
                        const bson_t                 *document,
                        const mongoc_write_concern_t *write_concern,
                        bson_error_t                 *error)
{
   bson_iter_t iter;
   bool ret;
   bson_t selector;

   bson_return_val_if_fail(collection, false);
   bson_return_val_if_fail(document, false);

   if (!bson_iter_init_find(&iter, document, "_id")) {
      return mongoc_collection_insert(collection,
                                      MONGOC_INSERT_NONE,
                                      document,
                                      write_concern,
                                      error);
   }

   bson_init(&selector);
   bson_append_iter(&selector, NULL, 0, &iter);

   ret = mongoc_collection_update(collection,
                                  MONGOC_UPDATE_UPSERT,
                                  &selector,
                                  document,
                                  write_concern,
                                  error);

   bson_destroy(&selector);

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_remove --
 *
 *       Delete one or more items from a collection. If you want to
 *       limit to a single delete, provided MONGOC_DELETE_SINGLE_REMOVE
 *       for @flags.
 *
 * Parameters:
 *       @collection: A mongoc_collection_t.
 *       @flags: the delete flags or 0.
 *       @selector: A selector of documents to delete.
 *       @write_concern: A write concern or NULL. If NULL, the default
 *                       write concern for the collection will be used.
 *       @error: A location for an error or NULL.
 *
 * Returns:
 *       true if successful; otherwise false and error is set.
 *
 *       If the write concern does not dictate checking the result, this
 *       function may return true even if it failed.
 *
 * Side effects:
 *       @collection->gle is setup, depending on write_concern->w value.
 *       @error is setup upon failure.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_collection_remove (mongoc_collection_t          *collection,
                          mongoc_remove_flags_t         flags,
                          const bson_t                 *selector,
                          const mongoc_write_concern_t *write_concern,
                          bson_error_t                 *error)
{
   mongoc_write_command_t command;
   mongoc_write_result_t result;
   bool multi;
   bool ret;

   ENTRY;

   bson_return_val_if_fail (collection, false);
   bson_return_val_if_fail (selector, false);

   bson_clear (&collection->gle);

   if (!write_concern) {
      write_concern = collection->write_concern;
   }

   multi = !(flags & MONGOC_REMOVE_SINGLE_REMOVE);

   _mongoc_write_result_init (&result);
   _mongoc_write_command_init_delete (&command, selector, multi, true);

   _mongoc_write_command_execute (&command, collection->client, 0,
                                  collection->db, collection->collection,
                                  write_concern, &result);

   collection->gle = bson_new ();
   ret = _mongoc_write_result_complete (&result, collection->gle, error);

   _mongoc_write_result_destroy (&result);
   _mongoc_write_command_destroy (&command);

   RETURN (ret);
}


bool
mongoc_collection_delete (mongoc_collection_t          *collection,
                          mongoc_delete_flags_t         flags,
                          const bson_t                 *selector,
                          const mongoc_write_concern_t *write_concern,
                          bson_error_t                 *error)
{
   return mongoc_collection_remove (collection, (mongoc_remove_flags_t)flags,
                                    selector, write_concern, error);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_get_read_prefs --
 *
 *       Fetch the default read preferences for the collection.
 *
 * Returns:
 *       A mongoc_read_prefs_t that should not be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const mongoc_read_prefs_t *
mongoc_collection_get_read_prefs (const mongoc_collection_t *collection)
{
   bson_return_val_if_fail(collection, NULL);
   return collection->read_prefs;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_set_read_prefs --
 *
 *       Sets the default read preferences for the collection instance.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_collection_set_read_prefs (mongoc_collection_t       *collection,
                                  const mongoc_read_prefs_t *read_prefs)
{
   bson_return_if_fail(collection);

   if (collection->read_prefs) {
      mongoc_read_prefs_destroy(collection->read_prefs);
      collection->read_prefs = NULL;
   }

   if (read_prefs) {
      collection->read_prefs = mongoc_read_prefs_copy(read_prefs);
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_get_write_concern --
 *
 *       Fetches the default write concern for the collection instance.
 *
 * Returns:
 *       A mongoc_write_concern_t that should not be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const mongoc_write_concern_t *
mongoc_collection_get_write_concern (const mongoc_collection_t *collection)
{
   bson_return_val_if_fail (collection, NULL);

   return collection->write_concern;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_set_write_concern --
 *
 *       Sets the default write concern for the collection instance.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_collection_set_write_concern (mongoc_collection_t          *collection,
                                     const mongoc_write_concern_t *write_concern)
{
   bson_return_if_fail(collection);

   if (collection->write_concern) {
      mongoc_write_concern_destroy(collection->write_concern);
      collection->write_concern = NULL;
   }

   if (write_concern) {
      collection->write_concern = mongoc_write_concern_copy(write_concern);
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_get_name --
 *
 *       Returns the name of the collection, excluding the database name.
 *
 * Returns:
 *       A string which should not be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const char *
mongoc_collection_get_name (mongoc_collection_t *collection)
{
   BSON_ASSERT (collection);

   return collection->collection;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_get_last_error --
 *
 *       Returns getLastError document, according to write_concern on last
 *       executed command for current collection instance.
 *
 * Returns:
 *       NULL or a bson_t that should not be modified or freed. This value
 *       is not guaranteed to be persistent between calls into the
 *       mongoc_collection_t instance, and therefore must be copied if
 *       you would like to keep it around.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const bson_t *
mongoc_collection_get_last_error (const mongoc_collection_t *collection) /* IN */
{
   bson_return_val_if_fail (collection, NULL);

   return collection->gle;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_validate --
 *
 *       Helper to call the validate command on the MongoDB server to
 *       validate the collection.
 *
 *       Options may be additional options, or NULL.
 *       Currently supported options are:
 *
 *          "full": Boolean
 *
 *       If full is true, then perform a more resource intensive
 *       validation.
 *
 *       The result is stored in reply.
 *
 * Returns:
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       @reply is set if successful.
 *       @error may be set.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_collection_validate (mongoc_collection_t *collection, /* IN */
                            const bson_t        *options,    /* IN */
                            bson_t              *reply,      /* OUT */
                            bson_error_t        *error)      /* IN */
{
   bson_iter_t iter;
   bson_t cmd = BSON_INITIALIZER;
   bool ret;

   bson_return_val_if_fail (collection, false);

   if (options &&
       bson_iter_init_find (&iter, options, "full") &&
       !BSON_ITER_HOLDS_BOOL (&iter)) {
      bson_set_error (error,
                      MONGOC_ERROR_BSON,
                      MONGOC_ERROR_BSON_INVALID,
                      "'full' must be a boolean value.");
      return false;
   }

   bson_append_utf8 (&cmd, "validate", 8,
                     collection->collection,
                     collection->collectionlen);

   if (options) {
      bson_concat (&cmd, options);
   }

   ret = mongoc_collection_command_simple (collection, &cmd, NULL, reply, error);

   bson_destroy (&cmd);

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_rename --
 *
 *       Rename the collection to @new_name.
 *
 *       If @new_db is NULL, the same db will be used.
 *
 *       If @drop_target_before_rename is true, then a collection named
 *       @new_name will be dropped before renaming @collection to
 *       @new_name.
 *
 * Returns:
 *       true on success; false on failure and @error is set.
 *
 * Side effects:
 *       @error is set on failure.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_collection_rename (mongoc_collection_t *collection,
                          const char          *new_db,
                          const char          *new_name,
                          bool                 drop_target_before_rename,
                          bson_error_t        *error)
{
   bson_t cmd = BSON_INITIALIZER;
   char newns [MONGOC_NAMESPACE_MAX + 1];
   bool ret;

   bson_return_val_if_fail (collection, false);
   bson_return_val_if_fail (new_name, false);

   if (!validate_name (new_name)) {
      bson_set_error (error,
                      MONGOC_ERROR_NAMESPACE,
                      MONGOC_ERROR_NAMESPACE_INVALID,
                      "\"%s\" is an invalid collection name.",
                      new_name);
      return false;
   }

   bson_snprintf (newns, sizeof newns, "%s.%s",
                  new_db ? new_db : collection->db,
                  new_name);

   BSON_APPEND_UTF8 (&cmd, "renameCollection", collection->ns);
   BSON_APPEND_UTF8 (&cmd, "to", newns);

   if (drop_target_before_rename) {
      BSON_APPEND_BOOL (&cmd, "dropTarget", true);
   }

   ret = mongoc_client_command_simple (collection->client, "admin",
                                       &cmd, NULL, NULL, error);

   if (ret) {
      if (new_db) {
         bson_snprintf (collection->db, sizeof collection->db, "%s", new_db);
      }

      bson_snprintf (collection->collection, sizeof collection->collection,
                     "%s", new_name);
      collection->collectionlen = (int) strlen (collection->collection);

      bson_snprintf (collection->ns, sizeof collection->ns,
                     "%s.%s", collection->db, new_name);
      collection->nslen = (int) strlen (collection->ns);
   }

   bson_destroy (&cmd);

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_stats --
 *
 *       Fetches statistics about the collection.
 *
 *       The result is stored in @stats, which should NOT be an initialized
 *       bson_t or a leak will occur.
 *
 *       @stats, @options, and @error are optional.
 *
 * Returns:
 *       true on success and @stats is set.
 *       false on failure and @error is set.
 *
 * Side effects:
 *       @stats and @error.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_collection_stats (mongoc_collection_t *collection,
                         const bson_t        *options,
                         bson_t              *stats,
                         bson_error_t        *error)
{
   bson_iter_t iter;
   bson_t cmd = BSON_INITIALIZER;
   bool ret;

   bson_return_val_if_fail (collection, false);

   if (options &&
       bson_iter_init_find (&iter, options, "scale") &&
       !BSON_ITER_HOLDS_INT32 (&iter)) {
      bson_set_error (error,
                      MONGOC_ERROR_BSON,
                      MONGOC_ERROR_BSON_INVALID,
                      "'scale' must be an int32 value.");
      return false;
   }

   BSON_APPEND_UTF8 (&cmd, "collStats", collection->collection);

   if (options) {
      bson_concat (&cmd, options);
   }

   ret = mongoc_collection_command_simple (collection, &cmd, NULL, stats, error);

   bson_destroy (&cmd);

   return ret;
}


mongoc_bulk_operation_t *
mongoc_collection_create_bulk_operation (
      mongoc_collection_t          *collection,
      bool                          ordered,
      const mongoc_write_concern_t *write_concern)
{
   bson_return_val_if_fail (collection, NULL);

   /*
    * TODO: where should we discover if we can do new or old style bulk ops?
    */

   return _mongoc_bulk_operation_new (collection->client,
                                      collection->db,
                                      collection->collection,
                                      0,
                                      ordered,
                                      write_concern);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_find_and_modify --
 *
 *       Find a document in @collection matching @query and update it with
 *       the update document @update.
 *
 *       If @reply is not NULL, then the result document will be placed
 *       in reply and should be released with bson_destroy().
 *
 *       If @remove is true, then the matching documents will be removed.
 *
 *       If @fields is not NULL, it will be used to select the desired
 *       resulting fields.
 *
 *       If @_new is true, then the new version of the document is returned
 *       instead of the old document.
 *
 *       See http://docs.mongodb.org/manual/reference/command/findAndModify/
 *       for more information.
 *
 * Returns:
 *       true on success; false on failure.
 *
 * Side effects:
 *       reply is initialized.
 *       error is set if false is returned.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_collection_find_and_modify (mongoc_collection_t *collection,
                                   const bson_t        *query,
                                   const bson_t        *sort,
                                   const bson_t        *update,
                                   const bson_t        *fields,
                                   bool                 _remove,
                                   bool                 upsert,
                                   bool                 _new,
                                   bson_t              *reply,
                                   bson_error_t        *error)
{
   const char *name;
   bool ret;
   bson_t command = BSON_INITIALIZER;

   ENTRY;

   bson_return_val_if_fail (collection, false);
   bson_return_val_if_fail (query, false);
   bson_return_val_if_fail (update || _remove, false);

   name = mongoc_collection_get_name (collection);

   BSON_APPEND_UTF8 (&command, "findAndModify", name);
   BSON_APPEND_DOCUMENT (&command, "query", query);

   if (sort) {
      BSON_APPEND_DOCUMENT (&command, "sort", sort);
   }

   if (update) {
      BSON_APPEND_DOCUMENT (&command, "update", update);
   }

   if (fields) {
      BSON_APPEND_DOCUMENT (&command, "fields", fields);
   }

   if (_remove) {
      BSON_APPEND_BOOL (&command, "remove", _remove);
   }

   if (upsert) {
      BSON_APPEND_BOOL (&command, "upsert", upsert);
   }

   if (_new) {
      BSON_APPEND_BOOL (&command, "new", _new);
   }

   /*
    * Submit the command to MongoDB server.
    */
   ret = mongoc_collection_command_simple (collection, &command, NULL, reply, error);

   /*
    * Cleanup.
    */
   bson_destroy (&command);

   RETURN (ret);
}

/*==============================================================*/
/* --- mongoc-counters.c */

#pragma pack(1)
typedef struct
{
   uint32_t offset;
   uint32_t slot;
   char          category[24];
   char          name[32];
   char          description[64];
} mongoc_counter_info_t;
#pragma pack()


BSON_STATIC_ASSERT(sizeof(mongoc_counter_info_t) == 128);


#pragma pack(1)
typedef struct
{
   uint32_t size;
   uint32_t n_cpu;
   uint32_t n_counters;
   uint32_t infos_offset;
   uint32_t values_offset;
   uint8_t  padding[44];
} mongoc_counters_t;
#pragma pack()


BSON_STATIC_ASSERT(sizeof(mongoc_counters_t) == 64);

static void *gCounterFallback = NULL;


#define COUNTER(ident, Category, Name, Description) \
   mongoc_counter_t __mongoc_counter_##ident;
#include "mongoc-counters.defs"
#undef COUNTER


/**
 * mongoc_counters_use_shm:
 *
 * Checks to see if counters should be exported over a shared memory segment.
 *
 * Returns: true if SHM is to be used.
 */
#ifdef BSON_OS_UNIX
static bool
mongoc_counters_use_shm (void)
{
   return !getenv("MONGOC_DISABLE_SHM");
}
#endif


/**
 * mongoc_counters_calc_size:
 *
 * Returns the number of bytes required for the shared memory segment of
 * the process. This segment contains the various statistical counters for
 * the process.
 *
 * Returns: The number of bytes required.
 */
static size_t
mongoc_counters_calc_size (void)
{
   size_t n_cpu;
   size_t n_groups;
   size_t size;

   n_cpu = _mongoc_get_cpu_count();
   n_groups = (LAST_COUNTER / SLOTS_PER_CACHELINE) + 1;
   size = (sizeof(mongoc_counters_t) +
           (LAST_COUNTER * sizeof(mongoc_counter_info_t)) +
           (n_cpu * n_groups * sizeof(mongoc_counter_slots_t)));

#ifdef BSON_OS_UNIX
   return MAX((size_t)getpagesize(), size);
#else
   return size;
#endif
}


/**
 * mongoc_counters_destroy:
 *
 * Removes the shared memory segment for the current processes counters.
 */
static void
mongoc_counters_destroy (void)
{
   if (gCounterFallback) {
      bson_free (gCounterFallback);
      gCounterFallback = NULL;
#ifndef _WIN32
   } else {
      char name [32];
      int pid;

      pid = getpid ();
      bson_snprintf (name, sizeof name, "/mongoc-%u", pid);
      shm_unlink (name);
#endif
   }
}


/**
 * mongoc_counters_alloc:
 * @size: The size of the shared memory segment.
 *
 * This function allocates the shared memory segment for use by counters
 * within the process.
 *
 * Returns: A shared memory segment, or malloc'd memory on failure.
 */
static void *
mongoc_counters_alloc (size_t size)
{
#ifdef BSON_OS_UNIX
   void *mem;
   char name[32];
   int pid;
   int fd;

   if (!mongoc_counters_use_shm ()) {
      goto use_malloc;
   }

   pid = getpid ();
   bson_snprintf (name, sizeof name, "/mongoc-%u", pid);

   if (-1 == (fd = shm_open (name, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR))) {
      goto use_malloc;
   }

   /*
    * NOTE:
    *
    * ftruncate() will cause reads to be zero. Therefore, we don't need to
    * do write() of zeroes to initialize the shared memory area.
    */
   if (-1 == ftruncate (fd, size)) {
      goto failure;
   }

   mem = mmap (NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
   if (mem == MAP_FAILED) {
      goto failure;
   }

   close (fd);
   memset (mem, 0, size);
   atexit (mongoc_counters_destroy);

   return mem;

failure:
   shm_unlink (name);
   close (fd);

use_malloc:
#endif

   MONGOC_WARNING ("Falling back to malloc for counters.");

   gCounterFallback = bson_malloc0 (size);
   atexit (mongoc_counters_destroy);

   return gCounterFallback;
}


/**
 * mongoc_counters_register:
 * @counters: A mongoc_counter_t.
 * @num: The counter number.
 * @category: The counter category.
 * @name: THe counter name.
 * @description The counter description.
 *
 * Registers a new counter in the memory segment for counters. If the counters
 * are exported over shared memory, it will be made available.
 *
 * Returns: The offset to the data for the counters values.
 */
static size_t
mongoc_counters_register (mongoc_counters_t *counters,
                          uint32_t      num,
                          const char        *category,
                          const char        *name,
                          const char        *description)
{
   mongoc_counter_info_t *infos;
   char *segment;
   int n_cpu;

   BSON_ASSERT(counters);
   BSON_ASSERT(category);
   BSON_ASSERT(name);
   BSON_ASSERT(description);

   /*
    * Implementation Note:
    *
    * The memory barrier is required so that all of the above has been
    * completed. Then increment the n_counters so that a reading application
    * only knows about the counter after we have initialized it.
    */

   n_cpu = _mongoc_get_cpu_count();
   segment = (char *)counters;

   infos = (mongoc_counter_info_t *)(segment + counters->infos_offset);
   infos = &infos[counters->n_counters];
   infos->slot = num % SLOTS_PER_CACHELINE;
   infos->offset = (counters->values_offset +
                    ((num / SLOTS_PER_CACHELINE) *
                     n_cpu * sizeof(mongoc_counter_slots_t)));

   bson_strncpy (infos->category, category, sizeof infos->category);
   bson_strncpy (infos->name, name, sizeof infos->name);
   bson_strncpy (infos->description, description, sizeof infos->description);

   bson_memory_barrier ();

   counters->n_counters++;

   return infos->offset;
}


/**
 * mongoc_counters_init:
 *
 * Initializes the mongoc counters system. This should be run on library
 * initialization using the GCC constructor attribute.
 */
void
_mongoc_counters_init (void)
{
   mongoc_counter_info_t *info;
   mongoc_counters_t *counters;
   size_t infos_size;
   size_t off;
   size_t size;
   char *segment;

   size = mongoc_counters_calc_size();
   segment = mongoc_counters_alloc(size);
   infos_size = LAST_COUNTER * sizeof *info;

   counters = (mongoc_counters_t *)segment;
   counters->n_cpu = _mongoc_get_cpu_count();
   counters->n_counters = 0;
   counters->infos_offset = sizeof *counters;
   counters->values_offset = (uint32_t)(counters->infos_offset + infos_size);

   BSON_ASSERT ((counters->values_offset % 64) == 0);

#define COUNTER(ident, Category, Name, Desc) \
   off = mongoc_counters_register(counters, COUNTER_##ident, Category, Name, Desc); \
   __mongoc_counter_##ident.cpus = (void *)(segment + off);
#include "mongoc-counters.defs"
#undef COUNTER

   /*
    * NOTE:
    *
    * Only update the size of the shared memory area for the client after
    * we have initialized the rest of the counters. Don't forget our memory
    * barrier to prevent compiler reordering.
    */
   bson_memory_barrier ();
   counters->size = (uint32_t)size;
}

/*==============================================================*/
/* --- mongoc-cursor-array.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "cursor-array"


typedef struct
{
   const bson_t       *result;
   bool         has_array;
   bson_iter_t         iter;
   bson_t              bson;
   uint32_t       document_len;
   const uint8_t *document;
} mongoc_cursor_array_t;


static void *
_mongoc_cursor_array_new (void)
{
   mongoc_cursor_array_t *arr;

   ENTRY;

   arr = bson_malloc0 (sizeof *arr);

   RETURN (arr);
}


static void
_mongoc_cursor_array_destroy (mongoc_cursor_t *cursor)
{
   ENTRY;

   bson_free (cursor->iface_data);
   _mongoc_cursor_destroy (cursor);

   EXIT;
}


static bool
_mongoc_cursor_array_next (mongoc_cursor_t *cursor,
                           const bson_t   **bson)
{
   bool ret = true;
   mongoc_cursor_array_t *arr;
   bson_iter_t iter;

   ENTRY;

   arr = cursor->iface_data;
   *bson = NULL;

   if (!arr->has_array) {
      arr->has_array = true;

      ret = _mongoc_cursor_next (cursor, &arr->result);

      if (!(ret &&
            bson_iter_init_find (&iter, arr->result, "result") &&
            BSON_ITER_HOLDS_ARRAY (&iter) &&
            bson_iter_recurse (&iter, &arr->iter) &&
            bson_iter_next (&arr->iter))) {
         ret = false;
      }
   } else {
      ret = bson_iter_next (&arr->iter);
   }

   if (ret) {
      bson_iter_document (&arr->iter, &arr->document_len, &arr->document);
      bson_init_static (&arr->bson, arr->document, arr->document_len);

      *bson = &arr->bson;
   }

   RETURN (ret);
}


static mongoc_cursor_t *
_mongoc_cursor_array_clone (const mongoc_cursor_t *cursor)
{
   mongoc_cursor_t *clone_;

   ENTRY;

   clone_ = _mongoc_cursor_clone (cursor);
   _mongoc_cursor_array_init (clone_);

   RETURN (clone_);
}


static bool
_mongoc_cursor_array_more (mongoc_cursor_t *cursor)
{
   bool ret;
   mongoc_cursor_array_t *arr;
   bson_iter_t iter;

   ENTRY;

   arr = cursor->iface_data;

   if (arr->has_array) {
      memcpy (&iter, &arr->iter, sizeof iter);

      ret = bson_iter_next (&iter);
   } else {
      ret = true;
   }

   RETURN (ret);
}


static mongoc_cursor_interface_t gMongocCursorArray = {
   _mongoc_cursor_array_clone,
   _mongoc_cursor_array_destroy,
   _mongoc_cursor_array_more,
   _mongoc_cursor_array_next,
};


void
_mongoc_cursor_array_init (mongoc_cursor_t *cursor)
{
   ENTRY;

   cursor->iface_data = _mongoc_cursor_array_new ();

   memcpy (&cursor->iface, &gMongocCursorArray,
           sizeof (mongoc_cursor_interface_t));

   EXIT;
}

/*==============================================================*/
/* --- mongoc-cursor.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "cursor"


#ifdef _WIN32
# define strcasecmp _stricmp
#endif


static const char *gSecondaryOkCommands [] = {
   "aggregate",
   "authenticate",
   "collstats",
   "count",
   "dbstats",
   "distinct",
   "geonear",
   "geosearch",
   "geowalk",
   "getnonce",
   "group",
   "ismaster",
   "mapreduce",
   "parallelcollectionscan",
   "ping",
   "replsetgetstatus",
   "text",
   NULL
};


static const char *
_mongoc_cursor_get_read_mode_string (mongoc_read_mode_t mode)
{
   switch (mode) {
   case MONGOC_READ_PRIMARY:
      return "primary";
   case MONGOC_READ_PRIMARY_PREFERRED:
      return "primaryPreferred";
   case MONGOC_READ_SECONDARY:
      return "secondary";
   case MONGOC_READ_SECONDARY_PREFERRED:
      return "secondaryPreferred";
   case MONGOC_READ_NEAREST:
      return "nearest";
   default:
      return "";
   }
}

static int32_t
_mongoc_n_return (mongoc_cursor_t * cursor)
{
   /* by default, use the batch size */
   int32_t r = cursor->batch_size;

   if (cursor->is_command) {
      /* commands always have n_return of 1 */
      r = 1;
   } else if (cursor->limit) {
      /* calculate remaining */
      uint32_t remaining = cursor->limit - cursor->count;

      /* use min of batch or remaining */
      r = MIN(r, (int32_t)remaining);
   }

   return r;
}

mongoc_cursor_t *
_mongoc_cursor_new (mongoc_client_t           *client,
                    const char                *db_and_collection,
                    mongoc_query_flags_t       flags,
                    uint32_t                   skip,
                    uint32_t                   limit,
                    uint32_t                   batch_size,
                    bool                       is_command,
                    const bson_t              *query,
                    const bson_t              *fields,
                    const mongoc_read_prefs_t *read_prefs)
{
   mongoc_read_prefs_t *local_read_prefs = NULL;
   mongoc_read_mode_t mode;
   mongoc_cursor_t *cursor;
   const bson_t *tags;
   bson_iter_t iter;
   const char *key;
   const char *mode_str;
   bson_t child;
   bool found = false;
   int i;

   ENTRY;

   BSON_ASSERT (client);
   BSON_ASSERT (db_and_collection);
   BSON_ASSERT (query);

   if (!read_prefs) {
      read_prefs = client->read_prefs;
   }

   cursor = bson_malloc0 (sizeof *cursor);

   /*
    * DRIVERS-63:
    *
    * If this is a command and we have read_prefs other than PRIMARY, we need to
    * set SlaveOK bit in the protocol.
    */
   if (is_command &&
       read_prefs &&
       (mongoc_read_prefs_get_mode (read_prefs) != MONGOC_READ_PRIMARY)) {
      flags |= MONGOC_QUERY_SLAVE_OK;
   }

   /*
    * CDRIVER-244:
    *
    * If this is a command, we need to verify we can send it to the location
    * specified by the read preferences. Otherwise, log a warning that we
    * are rerouting to the primary instance.
    */
   if (is_command &&
       read_prefs &&
       (mongoc_read_prefs_get_mode (read_prefs) != MONGOC_READ_PRIMARY) &&
       bson_iter_init (&iter, query) &&
       bson_iter_next (&iter) &&
       (key = bson_iter_key (&iter))) {
      for (i = 0; gSecondaryOkCommands [i]; i++) {
         if (0 == strcasecmp (key, gSecondaryOkCommands [i])) {
            found = true;
            break;
         }
      }
      if (!found) {
         cursor->redir_primary = true;
         local_read_prefs = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);
         read_prefs = local_read_prefs;
         MONGOC_INFO ("Database command \"%s\" rerouted to primary node", key);
      }
   }

   /*
    * Cursors execute their query lazily. This sadly means that we must copy
    * some extra data around between the bson_t structures. This should be
    * small in most cases, so it reduces to a pure memcpy. The benefit to this
    * design is simplified error handling by API consumers.
    */

   cursor->client = client;
   bson_strncpy (cursor->ns, db_and_collection, sizeof cursor->ns);
   cursor->nslen = (uint32_t)strlen(cursor->ns);
   cursor->flags = flags;
   cursor->skip = skip;
   cursor->limit = limit;
   cursor->batch_size = batch_size;
   cursor->is_command = is_command;
   cursor->has_fields = !!fields;

#define MARK_FAILED(c) \
   do { \
      bson_init (&(c)->query); \
      bson_init (&(c)->fields); \
      (c)->failed = true; \
      (c)->done = true; \
      (c)->end_of_event = true; \
      (c)->sent = true; \
   } while (0)

   /* we can't have exhaust queries with limits */
   if ((flags & MONGOC_QUERY_EXHAUST) && limit) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_CURSOR,
                      MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                      "Cannot specify MONGOC_QUERY_EXHAUST and set a limit.");
      MARK_FAILED (cursor);
      GOTO (finish);
   }

   /* we can't have exhaust queries with sharded clusters */
   if ((flags & MONGOC_QUERY_EXHAUST) &&
       (client->cluster.mode == MONGOC_CLUSTER_SHARDED_CLUSTER)) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_CURSOR,
                      MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                      "Cannot specify MONGOC_QUERY_EXHAUST with sharded cluster.");
      MARK_FAILED (cursor);
      GOTO (finish);
   }

   /*
    * Check types of various optional parameters.
    */
   if (!is_command) {
      if (bson_iter_init_find (&iter, query, "$explain") &&
          !(BSON_ITER_HOLDS_BOOL (&iter) || BSON_ITER_HOLDS_INT32 (&iter))) {
         bson_set_error (&cursor->error,
                         MONGOC_ERROR_CURSOR,
                         MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                         "$explain must be a boolean.");
         MARK_FAILED (cursor);
         GOTO (finish);
      }

      if (bson_iter_init_find (&iter, query, "$snapshot") &&
          !BSON_ITER_HOLDS_BOOL (&iter) &&
          !BSON_ITER_HOLDS_INT32 (&iter)) {
         bson_set_error (&cursor->error,
                         MONGOC_ERROR_CURSOR,
                         MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                         "$snapshot must be a boolean.");
         MARK_FAILED (cursor);
         GOTO (finish);
      }
   }

   /*
    * Check if we have a mixed top-level query and dollar keys such
    * as $orderby. This is not allowed (you must use {$query:{}}.
    */
   if (bson_iter_init (&iter, query)) {
      bool found_dollar = false;
      bool found_non_dollar = false;

      while (bson_iter_next (&iter)) {
         if (bson_iter_key (&iter)[0] == '$') {
            found_dollar = true;
         } else {
            found_non_dollar = true;
         }
      }

      if (found_dollar && found_non_dollar) {
         bson_set_error (&cursor->error,
                         MONGOC_ERROR_CURSOR,
                         MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                         "Cannot mix top-level query with dollar keys such "
                         "as $orderby. Use {$query: {},...} instead.");
         MARK_FAILED (cursor);
         GOTO (finish);
      }
   }

   if (!cursor->is_command && !bson_has_field (query, "$query")) {
      bson_init (&cursor->query);
      bson_append_document (&cursor->query, "$query", 6, query);
   } else {
      bson_copy_to (query, &cursor->query);
   }

   if (read_prefs) {
      cursor->read_prefs = mongoc_read_prefs_copy (read_prefs);

      mode = mongoc_read_prefs_get_mode (read_prefs);
      tags = mongoc_read_prefs_get_tags (read_prefs);

      if (mode != MONGOC_READ_PRIMARY) {
         flags |= MONGOC_QUERY_SLAVE_OK;

         if ((mode != MONGOC_READ_SECONDARY_PREFERRED) || tags) {
            bson_append_document_begin (&cursor->query, "$readPreference",
                                        15, &child);
            mode_str = _mongoc_cursor_get_read_mode_string (mode);
            bson_append_utf8 (&child, "mode", 4, mode_str, -1);
            if (tags) {
               bson_append_array (&child, "tags", 4, tags);
            }
            bson_append_document_end (&cursor->query, &child);
         }
      }
   }

   if (fields) {
      bson_copy_to(fields, &cursor->fields);
   } else {
      bson_init(&cursor->fields);
   }

   _mongoc_buffer_init(&cursor->buffer, NULL, 0, NULL, NULL);

finish:
   mongoc_counter_cursors_active_inc();

   if (local_read_prefs) {
      mongoc_read_prefs_destroy (local_read_prefs);
   }

   RETURN (cursor);
}


static void
_mongoc_cursor_kill_cursor (mongoc_cursor_t *cursor,
                            int64_t     cursor_id)
{
   mongoc_rpc_t rpc = {{ 0 }};

   ENTRY;

   bson_return_if_fail(cursor);
   bson_return_if_fail(cursor_id);

   rpc.kill_cursors.msg_len = 0;
   rpc.kill_cursors.request_id = 0;
   rpc.kill_cursors.response_to = 0;
   rpc.kill_cursors.opcode = MONGOC_OPCODE_KILL_CURSORS;
   rpc.kill_cursors.zero = 0;
   rpc.kill_cursors.cursors = &cursor_id;
   rpc.kill_cursors.n_cursors = 1;

   _mongoc_client_sendv (cursor->client, &rpc, 1, 0, NULL, NULL, NULL);

   EXIT;
}


void
mongoc_cursor_destroy (mongoc_cursor_t *cursor)
{
   BSON_ASSERT(cursor);

   if (cursor->iface.destroy) {
      cursor->iface.destroy(cursor);
   } else {
      _mongoc_cursor_destroy(cursor);
   }

   EXIT;
}

void
_mongoc_cursor_destroy (mongoc_cursor_t *cursor)
{
   ENTRY;

   bson_return_if_fail(cursor);

   if (cursor->in_exhaust) {
      cursor->client->in_exhaust = false;

      if (!cursor->done) {
         _mongoc_cluster_disconnect_node (
            &cursor->client->cluster,
            &cursor->client->cluster.nodes[cursor->hint - 1]);
      }
   } else if (cursor->rpc.reply.cursor_id) {
      _mongoc_cursor_kill_cursor(cursor, cursor->rpc.reply.cursor_id);
   }

   if (cursor->reader) {
      bson_reader_destroy(cursor->reader);
      cursor->reader = NULL;
   }

   bson_destroy(&cursor->query);
   bson_destroy(&cursor->fields);
   _mongoc_buffer_destroy(&cursor->buffer);
   mongoc_read_prefs_destroy(cursor->read_prefs);

   bson_free(cursor);

   mongoc_counter_cursors_active_dec();
   mongoc_counter_cursors_disposed_inc();

   EXIT;
}


static void
_mongoc_cursor_populate_error (mongoc_cursor_t *cursor,
                               const bson_t    *doc,
                               bson_error_t    *error)
{
   uint32_t code = MONGOC_ERROR_QUERY_FAILURE;
   bson_iter_t iter;
   const char *msg = "Unknown query failure";

   BSON_ASSERT (cursor);
   BSON_ASSERT (doc);
   BSON_ASSERT (error);

   if (bson_iter_init_find (&iter, doc, "code") &&
       BSON_ITER_HOLDS_INT32 (&iter)) {
      code = bson_iter_int32 (&iter);
   }

   if (bson_iter_init_find (&iter, doc, "$err") &&
       BSON_ITER_HOLDS_UTF8 (&iter)) {
      msg = bson_iter_utf8 (&iter, NULL);
   }

   if (cursor->is_command &&
       bson_iter_init_find (&iter, doc, "errmsg") &&
       BSON_ITER_HOLDS_UTF8 (&iter)) {
      msg = bson_iter_utf8 (&iter, NULL);
   }

   bson_set_error(error, MONGOC_ERROR_QUERY, code, "%s", msg);
}


static bool
_mongoc_cursor_unwrap_failure (mongoc_cursor_t *cursor)
{
   bson_iter_t iter;
   bson_t b;

   ENTRY;

   bson_return_val_if_fail(cursor, false);

   if (cursor->rpc.header.opcode != MONGOC_OPCODE_REPLY) {
      bson_set_error(&cursor->error,
                     MONGOC_ERROR_PROTOCOL,
                     MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                     "Received rpc other than OP_REPLY.");
      RETURN(true);
   }

   if ((cursor->rpc.reply.flags & MONGOC_REPLY_QUERY_FAILURE)) {
      if (_mongoc_rpc_reply_get_first(&cursor->rpc.reply, &b)) {
         _mongoc_cursor_populate_error(cursor, &b, &cursor->error);
         bson_destroy(&b);
      } else {
         bson_set_error(&cursor->error,
                        MONGOC_ERROR_QUERY,
                        MONGOC_ERROR_QUERY_FAILURE,
                        "Unknown query failure.");
      }
      RETURN(true);
   } else if (cursor->is_command) {
      if (_mongoc_rpc_reply_get_first (&cursor->rpc.reply, &b)) {
         if (bson_iter_init_find (&iter, &b, "ok")) {
            if (bson_iter_as_bool (&iter)) {
               RETURN (false);
            } else {
               _mongoc_cursor_populate_error (cursor, &b, &cursor->error);
               bson_destroy (&b);
               RETURN (true);
            }
         }
      } else {
         bson_set_error (&cursor->error,
                         MONGOC_ERROR_BSON,
                         MONGOC_ERROR_BSON_INVALID,
                         "Failed to decode document from the server.");
         RETURN (true);
      }
   }

   if ((cursor->rpc.reply.flags & MONGOC_REPLY_CURSOR_NOT_FOUND)) {
      bson_set_error(&cursor->error,
                     MONGOC_ERROR_CURSOR,
                     MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                     "The cursor is invalid or has expired.");
      RETURN(true);
   }

   RETURN(false);
}


static bool
_mongoc_cursor_query (mongoc_cursor_t *cursor)
{
   mongoc_rpc_t rpc;
   uint32_t hint;
   uint32_t request_id;

   ENTRY;

   bson_return_val_if_fail (cursor, false);

   if (!_mongoc_client_warm_up (cursor->client, &cursor->error)) {
      cursor->failed = true;
      RETURN (false);
   }

   rpc.query.msg_len = 0;
   rpc.query.request_id = 0;
   rpc.query.response_to = 0;
   rpc.query.opcode = MONGOC_OPCODE_QUERY;
   rpc.query.flags = cursor->flags;
   rpc.query.collection = cursor->ns;
   rpc.query.skip = cursor->skip;
   if ((cursor->flags & MONGOC_QUERY_TAILABLE_CURSOR)) {
      rpc.query.n_return = 0;
   } else {
      rpc.query.n_return = _mongoc_n_return(cursor);
   }
   rpc.query.query = bson_get_data(&cursor->query);

   if (cursor->has_fields) {
      rpc.query.fields = bson_get_data (&cursor->fields);
   } else {
      rpc.query.fields = NULL;
   }

   if (!(hint = _mongoc_client_sendv (cursor->client, &rpc, 1,
                                      cursor->hint, NULL,
                                      cursor->read_prefs,
                                      &cursor->error))) {
      GOTO (failure);
   }

   cursor->hint = hint;
   request_id = BSON_UINT32_FROM_LE(rpc.header.request_id);

   _mongoc_buffer_clear(&cursor->buffer, false);

   if (!_mongoc_client_recv(cursor->client,
                            &cursor->rpc,
                            &cursor->buffer,
                            hint,
                            &cursor->error)) {
      GOTO (failure);
   }

   if (cursor->rpc.header.opcode != MONGOC_OPCODE_REPLY) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Invalid opcode. Expected %d, got %d.",
                      MONGOC_OPCODE_REPLY, cursor->rpc.header.opcode);
      GOTO (failure);
   }

   if (cursor->rpc.header.response_to != (int)request_id) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Invalid response_to. Expected %d, got %d.",
                      request_id, cursor->rpc.header.response_to);
      GOTO (failure);
   }

   if (_mongoc_cursor_unwrap_failure(cursor)) {
      if ((cursor->error.domain == MONGOC_ERROR_QUERY) &&
          (cursor->error.code == MONGOC_ERROR_QUERY_NOT_TAILABLE)) {
         cursor->failed = true;
      }
      GOTO (failure);
   }

   if (cursor->reader) {
      bson_reader_destroy(cursor->reader);
   }

   cursor->reader = bson_reader_new_from_data(cursor->rpc.reply.documents,
                                              cursor->rpc.reply.documents_len);

   if ((cursor->flags & MONGOC_QUERY_EXHAUST)) {
      cursor->in_exhaust = true;
      cursor->client->in_exhaust = true;
   }

   cursor->done = false;
   cursor->end_of_event = false;
   cursor->sent = true;

   RETURN (true);

failure:
   cursor->failed = true;
   cursor->done = true;

   RETURN (false);
}


static bool
_mongoc_cursor_get_more (mongoc_cursor_t *cursor)
{
   uint64_t cursor_id;
   uint32_t request_id;
   mongoc_rpc_t rpc;

   ENTRY;

   BSON_ASSERT (cursor);

   if (!cursor->in_exhaust) {
      if (!_mongoc_client_warm_up (cursor->client, &cursor->error)) {
         cursor->failed = true;
         RETURN (false);
      }

      if (!(cursor_id = cursor->rpc.reply.cursor_id)) {
         bson_set_error(&cursor->error,
                        MONGOC_ERROR_CURSOR,
                        MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                        "No valid cursor was provided.");
         GOTO (failure);
      }

      rpc.get_more.msg_len = 0;
      rpc.get_more.request_id = 0;
      rpc.get_more.response_to = 0;
      rpc.get_more.opcode = MONGOC_OPCODE_GET_MORE;
      rpc.get_more.zero = 0;
      rpc.get_more.collection = cursor->ns;
      if ((cursor->flags & MONGOC_QUERY_TAILABLE_CURSOR)) {
         rpc.get_more.n_return = 0;
      } else {
         rpc.get_more.n_return = _mongoc_n_return(cursor);
      }
      rpc.get_more.cursor_id = cursor_id;

      /*
       * TODO: Stamp protections for disconnections.
       */

      if (!_mongoc_client_sendv(cursor->client, &rpc, 1, cursor->hint,
                                NULL, cursor->read_prefs, &cursor->error)) {
         cursor->done = true;
         cursor->failed = true;
         RETURN (false);
      }

      request_id = BSON_UINT32_FROM_LE(rpc.header.request_id);
   } else {
      request_id = BSON_UINT32_FROM_LE(cursor->rpc.header.request_id);
   }

   _mongoc_buffer_clear(&cursor->buffer, false);

   if (!_mongoc_client_recv(cursor->client,
                            &cursor->rpc,
                            &cursor->buffer,
                            cursor->hint,
                            &cursor->error)) {
      GOTO (failure);
   }

   if (cursor->rpc.header.opcode != MONGOC_OPCODE_REPLY) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Invalid opcode. Expected %d, got %d.",
                      MONGOC_OPCODE_REPLY, cursor->rpc.header.opcode);
      GOTO (failure);
   }

   if (cursor->rpc.header.response_to != (int)request_id) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Invalid response_to. Expected %d, got %d.",
                      request_id, cursor->rpc.header.response_to);
      GOTO (failure);
   }

   if (_mongoc_cursor_unwrap_failure(cursor)) {
      GOTO (failure);
   }

   if (cursor->reader) {
      bson_reader_destroy(cursor->reader);
   }

   cursor->reader = bson_reader_new_from_data(cursor->rpc.reply.documents,
                                              cursor->rpc.reply.documents_len);

   cursor->end_of_event = false;

   RETURN(true);

failure:
   cursor->done = true;
   cursor->failed = true;

   RETURN(false);
}


bool
mongoc_cursor_error (mongoc_cursor_t *cursor,
                     bson_error_t    *error)
{
   bool ret;

   BSON_ASSERT(cursor);

   if (cursor->iface.error) {
      ret = cursor->iface.error(cursor, error);
   } else {
      ret = _mongoc_cursor_error(cursor, error);
   }

   if (ret && error) {
      /*
       * Rewrite the error code if we are talking to an older mongod
       * and the command was not found. It used to simply return an
       * error code of 17 and we can synthesize 59.
       *
       * Additionally, old versions of mongos may send 13390 indicating
       * unrecognized command.
       */
      if (cursor->is_command &&
          ((error->code == MONGOC_ERROR_PROTOCOL_ERROR) ||
           (error->code == 13390))) {
         error->code = MONGOC_ERROR_QUERY_COMMAND_NOT_FOUND;
      }
   }

   RETURN(ret);
}


bool
_mongoc_cursor_error (mongoc_cursor_t *cursor,
                      bson_error_t    *error)
{
   ENTRY;

   bson_return_val_if_fail(cursor, false);

   if (BSON_UNLIKELY(cursor->failed)) {
      bson_set_error(error,
                     cursor->error.domain,
                     cursor->error.code,
                     "%s",
                     cursor->error.message);
      RETURN(true);
   }

   RETURN(false);
}


bool
mongoc_cursor_next (mongoc_cursor_t  *cursor,
                    const bson_t    **bson)
{
   bool ret;

   ENTRY;

   BSON_ASSERT(cursor);
   BSON_ASSERT(bson);

   TRACE ("cursor_id(%"PRId64")", cursor->rpc.reply.cursor_id);

   if (bson) {
      *bson = NULL;
   }

   if (cursor->failed) {
      return false;
   }

   if (cursor->iface.next) {
      ret = cursor->iface.next(cursor, bson);
   } else {
      ret = _mongoc_cursor_next(cursor, bson);
   }

   cursor->current = *bson;

   cursor->count++;

   RETURN(ret);
}


bool
_mongoc_cursor_next (mongoc_cursor_t  *cursor,
                     const bson_t    **bson)
{
   const bson_t *b;
   bool eof;

   ENTRY;

   BSON_ASSERT (cursor);

   if (bson) {
      *bson = NULL;
   }

   if (cursor->done || cursor->failed) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_CURSOR,
                      MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                      "Cannot advance a completed or failed cursor.");
      RETURN (false);
   }

   /*
    * We cannot proceed if another cursor is receiving results in exhaust mode.
    */
   if (cursor->client->in_exhaust && !cursor->in_exhaust) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_IN_EXHAUST,
                      "Another cursor derived from this client is in exhaust.");
      cursor->failed = true;
      RETURN (false);
   }

   /*
    * If we reached our limit, make sure we mark this as done and do not try to
    * make further progress.
    */
   if (cursor->limit && cursor->count >= cursor->limit) {
      cursor->done = true;
      RETURN (false);
   }

   /*
    * Try to read the next document from the reader if it exists, we might
    * get NULL back and EOF, in which case we need to submit a getmore.
    */
   if (cursor->reader) {
      eof = false;
      b = bson_reader_read (cursor->reader, &eof);
      cursor->end_of_event = eof;
      if (b) {
         GOTO (complete);
      }
   }

   /*
    * Check to see if we need to send a GET_MORE for more results.
    */
   if (!cursor->sent) {
      if (!_mongoc_cursor_query (cursor)) {
         RETURN (false);
      }
   } else if (BSON_UNLIKELY (cursor->end_of_event) && cursor->rpc.reply.cursor_id) {
      if (!_mongoc_cursor_get_more (cursor)) {
         RETURN (false);
      }
   }

   eof = false;
   b = bson_reader_read (cursor->reader, &eof);
   cursor->end_of_event = eof;

complete:
   cursor->done = (cursor->end_of_event &&
                   ((cursor->in_exhaust && !cursor->rpc.reply.cursor_id) ||
                    (!b && !(cursor->flags & MONGOC_QUERY_TAILABLE_CURSOR))));

   /*
    * Do a supplimental check to see if we had a corrupted reply in the
    * document stream.
    */
   if (!b && !eof) {
      cursor->failed = true;
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_CURSOR,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "The reply was corrupt.");
      RETURN (false);
   }

   if (bson) {
      *bson = b;
   }

   RETURN (!!b);
}


bool
mongoc_cursor_more (mongoc_cursor_t *cursor)
{
   bool ret;

   ENTRY;

   BSON_ASSERT(cursor);

   if (cursor->iface.more) {
      ret = cursor->iface.more(cursor);
   } else {
      ret = _mongoc_cursor_more(cursor);
   }

   RETURN(ret);
}


bool
_mongoc_cursor_more (mongoc_cursor_t *cursor)
{
   bson_return_val_if_fail(cursor, false);

   if (cursor->failed) {
      return false;
   }

   return (!cursor->sent ||
           cursor->rpc.reply.cursor_id ||
           !cursor->end_of_event);
}


void
mongoc_cursor_get_host (mongoc_cursor_t    *cursor,
                        mongoc_host_list_t *host)
{
   BSON_ASSERT(cursor);
   BSON_ASSERT(host);

   if (cursor->iface.get_host) {
      cursor->iface.get_host(cursor, host);
   } else {
      _mongoc_cursor_get_host(cursor, host);
   }

   EXIT;
}


void
_mongoc_cursor_get_host (mongoc_cursor_t    *cursor,
                         mongoc_host_list_t *host)
{
   bson_return_if_fail(cursor);
   bson_return_if_fail(host);

   memset(host, 0, sizeof *host);

   if (!cursor->hint) {
      MONGOC_WARNING("%s(): Must send query before fetching peer.",
                     __FUNCTION__);
      return;
   }

   *host = cursor->client->cluster.nodes[cursor->hint - 1].host;
   host->next = NULL;
}


mongoc_cursor_t *
mongoc_cursor_clone (const mongoc_cursor_t *cursor)
{
   mongoc_cursor_t *ret;

   BSON_ASSERT(cursor);

   if (cursor->iface.clone) {
      ret = cursor->iface.clone(cursor);
   } else {
      ret = _mongoc_cursor_clone(cursor);
   }

   RETURN(ret);
}


mongoc_cursor_t *
_mongoc_cursor_clone (const mongoc_cursor_t *cursor)
{
   mongoc_cursor_t *_clone;

   ENTRY;

   BSON_ASSERT (cursor);

   _clone = bson_malloc0 (sizeof *_clone);

   _clone->client = cursor->client;
   _clone->is_command = cursor->is_command;
   _clone->flags = cursor->flags;
   _clone->skip = cursor->skip;
   _clone->batch_size = cursor->batch_size;
   _clone->limit = cursor->limit;
   _clone->nslen = cursor->nslen;

   if (cursor->read_prefs) {
      _clone->read_prefs = mongoc_read_prefs_copy (cursor->read_prefs);
   }

   bson_copy_to (&cursor->query, &_clone->query);
   bson_copy_to (&cursor->fields, &_clone->fields);

   bson_strncpy (_clone->ns, cursor->ns, sizeof _clone->ns);

   _mongoc_buffer_init (&_clone->buffer, NULL, 0, NULL, NULL);

   mongoc_counter_cursors_active_inc ();

   RETURN (_clone);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cursor_is_alive --
 *
 *       Checks to see if a cursor is alive.
 *
 *       This is primarily useful with tailable cursors.
 *
 * Returns:
 *       true if the cursor is alive.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_cursor_is_alive (const mongoc_cursor_t *cursor) /* IN */
{
   bson_return_val_if_fail (cursor, false);

   return (!cursor->sent ||
           (!cursor->failed &&
            !cursor->done &&
            (cursor->rpc.header.opcode == MONGOC_OPCODE_REPLY) &&
            cursor->rpc.reply.cursor_id));
}


const bson_t *
mongoc_cursor_current (const mongoc_cursor_t *cursor) /* IN */
{
   bson_return_val_if_fail (cursor, NULL);

   return cursor->current;
}


uint32_t
mongoc_cursor_get_hint (const mongoc_cursor_t *cursor)
{
   bson_return_val_if_fail (cursor, 0);

   return cursor->hint;
}

/*==============================================================*/
/* --- mongoc-cursor-cursorid.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "cursor-cursorid"


typedef struct
{
   bool        has_cursor;
   bool        in_first_batch;
   bson_iter_t first_batch_iter;
   bson_t      first_batch_inline;
} mongoc_cursor_cursorid_t;


static void *
_mongoc_cursor_cursorid_new (void)
{
   mongoc_cursor_cursorid_t *cid;

   ENTRY;

   cid = bson_malloc0 (sizeof *cid);

   RETURN (cid);
}


static void
_mongoc_cursor_cursorid_destroy (mongoc_cursor_t *cursor)
{
   ENTRY;

   bson_free (cursor->iface_data);
   _mongoc_cursor_destroy (cursor);

   EXIT;
}


static bool
_mongoc_cursor_cursorid_next (mongoc_cursor_t *cursor,
                              const bson_t   **bson)
{
   bool ret;
   mongoc_cursor_cursorid_t *cid;
   bson_iter_t iter;
   bson_iter_t child;
   const char *ns;
   const uint8_t *data = NULL;
   uint32_t data_len = 0;

   ENTRY;

   cid = cursor->iface_data;

   if (cid->in_first_batch) {
process_first_batch:
      while (bson_iter_next (&cid->first_batch_iter)) {
         if (BSON_ITER_HOLDS_DOCUMENT (&cid->first_batch_iter)) {
            bson_iter_document (&cid->first_batch_iter, &data_len, &data);
            if (bson_init_static (&cid->first_batch_inline, data, data_len)) {
               *bson = &cid->first_batch_inline;
               RETURN (true);
            }
         }
      }
      cid->in_first_batch = false;
      cursor->end_of_event = true;
      if (!cursor->rpc.reply.cursor_id) {
         cursor->done = true;
         *bson = NULL;
         RETURN (false);
      }
   }

   ret = _mongoc_cursor_next (cursor, bson);

   if (!cid->has_cursor) {
      cid->has_cursor = true;

      if (ret &&
          bson_iter_init_find (&iter, *bson, "cursor") &&
          BSON_ITER_HOLDS_DOCUMENT (&iter) &&
          bson_iter_recurse (&iter, &child)) {
         while (bson_iter_next (&child)) {
            if (BSON_ITER_IS_KEY (&child, "id")) {
               cursor->rpc.reply.cursor_id = bson_iter_as_int64 (&child);
            } else if (BSON_ITER_IS_KEY (&child, "ns")) {
               ns = bson_iter_utf8 (&child, &cursor->nslen);
               bson_strncpy (cursor->ns, ns, sizeof cursor->ns);
            } else if (BSON_ITER_IS_KEY (&child, "firstBatch")) {
               if (BSON_ITER_HOLDS_ARRAY (&child) &&
                   bson_iter_recurse (&child, &cid->first_batch_iter)) {
                  cid->in_first_batch = true;
               }
            }
         }

         cursor->is_command = false;

         if (cid->in_first_batch) {
            cursor->end_of_event = false;
            goto process_first_batch;
         } else if (cursor->rpc.reply.cursor_id) {
            ret = _mongoc_cursor_next (cursor, bson);
         }
      }
   }

   RETURN (ret);
}


static mongoc_cursor_t *
_mongoc_cursor_cursorid_clone (const mongoc_cursor_t *cursor)
{
   mongoc_cursor_t *clone_;

   ENTRY;

   clone_ = _mongoc_cursor_clone (cursor);
   _mongoc_cursor_cursorid_init (clone_);

   RETURN (clone_);
}


static mongoc_cursor_interface_t gMongocCursorCursorid = {
   _mongoc_cursor_cursorid_clone,
   _mongoc_cursor_cursorid_destroy,
   NULL,
   _mongoc_cursor_cursorid_next,
};


void
_mongoc_cursor_cursorid_init (mongoc_cursor_t *cursor)
{
   ENTRY;

   cursor->iface_data = _mongoc_cursor_cursorid_new ();

   memcpy (&cursor->iface, &gMongocCursorCursorid,
           sizeof (mongoc_cursor_interface_t));

   EXIT;
}

/*==============================================================*/
/* --- mongoc-database.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "database"


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_database_new --
 *
 *       Create a new instance of mongoc_database_t for @client.
 *
 *       @client must stay valid for the life of the resulting
 *       database structure.
 *
 * Returns:
 *       A newly allocated mongoc_database_t that should be freed with
 *       mongoc_database_destroy().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_database_t *
_mongoc_database_new (mongoc_client_t              *client,
                      const char                   *name,
                      const mongoc_read_prefs_t    *read_prefs,
                      const mongoc_write_concern_t *write_concern)
{
   mongoc_database_t *db;

   ENTRY;

   bson_return_val_if_fail(client, NULL);
   bson_return_val_if_fail(name, NULL);

   db = bson_malloc0(sizeof *db);
   db->client = client;
   db->write_concern = write_concern ?
      mongoc_write_concern_copy(write_concern) :
      mongoc_write_concern_new();
   db->read_prefs = read_prefs ?
      mongoc_read_prefs_copy(read_prefs) :
      mongoc_read_prefs_new(MONGOC_READ_PRIMARY);

   bson_strncpy (db->name, name, sizeof db->name);

   RETURN(db);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_database_destroy --
 *
 *       Releases resources associated with @database.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Everything.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_database_destroy (mongoc_database_t *database)
{
   ENTRY;

   bson_return_if_fail(database);

   if (database->read_prefs) {
      mongoc_read_prefs_destroy(database->read_prefs);
      database->read_prefs = NULL;
   }

   if (database->write_concern) {
      mongoc_write_concern_destroy(database->write_concern);
      database->write_concern = NULL;
   }

   bson_free(database);

   EXIT;
}


mongoc_cursor_t *
mongoc_database_command (mongoc_database_t         *database,
                         mongoc_query_flags_t       flags,
                         uint32_t              skip,
                         uint32_t              limit,
                         uint32_t              batch_size,
                         const bson_t              *command,
                         const bson_t              *fields,
                         const mongoc_read_prefs_t *read_prefs)
{
   BSON_ASSERT (database);
   BSON_ASSERT (command);

   if (!read_prefs) {
      read_prefs = database->read_prefs;
   }

   return mongoc_client_command (database->client, database->name, flags, skip,
                                 limit, batch_size, command, fields, read_prefs);
}


bool
mongoc_database_command_simple (mongoc_database_t         *database,
                                const bson_t              *command,
                                const mongoc_read_prefs_t *read_prefs,
                                bson_t                    *reply,
                                bson_error_t              *error)
{
   BSON_ASSERT (database);
   BSON_ASSERT (command);

   if (!read_prefs) {
      read_prefs = database->read_prefs;
   }

   return mongoc_client_command_simple (database->client, database->name,
                                        command, read_prefs, reply, error);
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_database_drop --
 *
 *       Requests that the MongoDB server drops @database, including all
 *       collections and indexes associated with @database.
 *
 *       Make sure this is really what you want!
 *
 * Returns:
 *       true if @database was dropped.
 *
 * Side effects:
 *       @error may be set.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_database_drop (mongoc_database_t *database,
                      bson_error_t      *error)
{
   bool ret;
   bson_t cmd;

   bson_return_val_if_fail(database, false);

   bson_init(&cmd);
   bson_append_int32(&cmd, "dropDatabase", 12, 1);
   ret = mongoc_database_command_simple(database, &cmd, NULL, NULL, error);
   bson_destroy(&cmd);

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_database_add_user_legacy --
 *
 *       A helper to add a user or update their password on @database.
 *       This uses the legacy protocol by inserting into system.users.
 *
 * Returns:
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       @error may be set.
 *
 *--------------------------------------------------------------------------
 */

static bool
mongoc_database_add_user_legacy (mongoc_database_t *database,
                                 const char        *username,
                                 const char        *password,
                                 bson_error_t      *error)
{
   mongoc_collection_t *collection;
   mongoc_cursor_t *cursor = NULL;
   const bson_t *doc;
   bool ret = false;
   bson_t query;
   bson_t user;
   char *input;
   char *pwd = NULL;

   ENTRY;

   bson_return_val_if_fail(database, false);
   bson_return_val_if_fail(username, false);
   bson_return_val_if_fail(password, false);

   /*
    * Users are stored in the <dbname>.system.users virtual collection.
    */
   collection = mongoc_client_get_collection(database->client,
                                             database->name,
                                             "system.users");
   BSON_ASSERT(collection);

   /*
    * Hash the users password.
    */
   input = bson_strdup_printf("%s:mongo:%s", username, password);
   pwd = _mongoc_hex_md5(input);
   bson_free(input);

   /*
    * Check to see if the user exists. If so, we will update the
    * password instead of inserting a new user.
    */
   bson_init(&query);
   bson_append_utf8(&query, "user", 4, username, -1);
   cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE, 0, 1, 0,
                                   &query, NULL, NULL);
   if (!mongoc_cursor_next(cursor, &doc)) {
      if (mongoc_cursor_error(cursor, error)) {
         GOTO (failure);
      }
      bson_init(&user);
      bson_append_utf8(&user, "user", 4, username, -1);
      bson_append_bool(&user, "readOnly", 8, false);
      bson_append_utf8(&user, "pwd", 3, pwd, -1);
   } else {
      bson_copy_to_excluding(doc, &user, "pwd", (char *)NULL);
      bson_append_utf8(&user, "pwd", 3, pwd, -1);
   }

   if (!mongoc_collection_save(collection, &user, NULL, error)) {
      GOTO (failure_with_user);
   }

   ret = true;

failure_with_user:
   bson_destroy(&user);

failure:
   if (cursor) {
      mongoc_cursor_destroy(cursor);
   }
   mongoc_collection_destroy(collection);
   bson_destroy(&query);
   bson_free(pwd);

   RETURN (ret);
}


bool
mongoc_database_remove_user (mongoc_database_t *database,
                             const char        *username,
                             bson_error_t      *error)
{
   mongoc_collection_t *col;
   bson_error_t lerror;
   bson_t cmd;
   bool ret;

   ENTRY;

   bson_return_val_if_fail (database, false);
   bson_return_val_if_fail (username, false);

   bson_init (&cmd);
   BSON_APPEND_UTF8 (&cmd, "dropUser", username);
   ret = mongoc_database_command_simple (database, &cmd, NULL, NULL, &lerror);
   bson_destroy (&cmd);

   if (!ret && (lerror.code == MONGOC_ERROR_QUERY_COMMAND_NOT_FOUND)) {
      bson_init (&cmd);
      BSON_APPEND_UTF8 (&cmd, "user", username);

      col = mongoc_client_get_collection (database->client, database->name,
                                          "system.users");
      BSON_ASSERT (col);

      ret = mongoc_collection_remove (col,
                                      MONGOC_REMOVE_SINGLE_REMOVE,
                                      &cmd,
                                      NULL,
                                      error);

      bson_destroy (&cmd);
      mongoc_collection_destroy (col);
   }

   RETURN (ret);
}


bool
mongoc_database_remove_all_users (mongoc_database_t *database,
                                  bson_error_t      *error)
{
   mongoc_collection_t *col;
   bson_error_t lerror;
   bson_t cmd;
   bool ret;

   ENTRY;

   bson_return_val_if_fail (database, false);

   bson_init (&cmd);
   BSON_APPEND_INT32 (&cmd, "dropAllUsersFromDatabase", 1);
   ret = mongoc_database_command_simple (database, &cmd, NULL, NULL, &lerror);
   bson_destroy (&cmd);

   if (!ret && (lerror.code == MONGOC_ERROR_QUERY_COMMAND_NOT_FOUND)) {
      bson_init (&cmd);

      col = mongoc_client_get_collection (database->client, database->name,
                                          "system.users");
      BSON_ASSERT (col);

      ret = mongoc_collection_remove (col, MONGOC_REMOVE_NONE, &cmd, NULL,
                                      error);

      bson_destroy (&cmd);
      mongoc_collection_destroy (col);
   }

   RETURN (ret);
}


/**
 * mongoc_database_add_user:
 * @database: A #mongoc_database_t.
 * @username: A string containing the username.
 * @password: (allow-none): A string containing password, or NULL.
 * @roles: (allow-none): An optional bson_t of roles.
 * @custom_data: (allow-none): An optional bson_t of data to store.
 * @error: (out) (allow-none): A location for a bson_error_t or %NULL.
 *
 * Creates a new user with access to @database.
 *
 * Returns: None.
 * Side effects: None.
 */
bool
mongoc_database_add_user (mongoc_database_t *database,
                          const char        *username,
                          const char        *password,
                          const bson_t      *roles,
                          const bson_t      *custom_data,
                          bson_error_t      *error)
{
   bson_error_t lerror;
   bson_t cmd;
   bson_t ar;
   char *input;
   char *hashed_password;
   bool ret = false;

   ENTRY;

   BSON_ASSERT (database);
   BSON_ASSERT (username);

   /*
    * CDRIVER-232:
    *
    * Perform a (slow and tedious) round trip to mongod to determine if
    * we can safely call createUser. Otherwise, we will fallback and
    * perform legacy insertion into users collection.
    */
   bson_init (&cmd);
   BSON_APPEND_UTF8 (&cmd, "usersInfo", username);
   ret = mongoc_database_command_simple (database, &cmd, NULL, NULL, &lerror);
   bson_destroy (&cmd);

   if (!ret && (lerror.code == MONGOC_ERROR_QUERY_COMMAND_NOT_FOUND)) {
      ret = mongoc_database_add_user_legacy (database, username, password, error);
   } else if (ret) {
      input = bson_strdup_printf ("%s:mongo:%s", username, password);
      hashed_password = _mongoc_hex_md5 (input);
      bson_free (input);

      bson_init (&cmd);
      BSON_APPEND_UTF8 (&cmd, "createUser", username);
      BSON_APPEND_UTF8 (&cmd, "pwd", hashed_password);
      BSON_APPEND_BOOL (&cmd, "digestPassword", false);
      if (custom_data) {
         BSON_APPEND_DOCUMENT (&cmd, "customData", custom_data);
      }
      if (roles) {
         BSON_APPEND_ARRAY (&cmd, "roles", roles);
      } else {
         bson_append_array_begin (&cmd, "roles", 5, &ar);
         bson_append_array_end (&cmd, &ar);
      }

      ret = mongoc_database_command_simple (database, &cmd, NULL, NULL, error);

      bson_free (hashed_password);
      bson_destroy (&cmd);
   } else if (error) {
      memcpy (error, &lerror, sizeof *error);
   }

   RETURN (ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_database_get_read_prefs --
 *
 *       Fetch the read preferences for @database.
 *
 * Returns:
 *       A mongoc_read_prefs_t that should not be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const mongoc_read_prefs_t *
mongoc_database_get_read_prefs (const mongoc_database_t *database) /* IN */
{
   bson_return_val_if_fail(database, NULL);
   return database->read_prefs;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_database_set_read_prefs --
 *
 *       Sets the default read preferences for @database.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_database_set_read_prefs (mongoc_database_t         *database,
                                const mongoc_read_prefs_t *read_prefs)
{
   bson_return_if_fail(database);

   if (database->read_prefs) {
      mongoc_read_prefs_destroy(database->read_prefs);
      database->read_prefs = NULL;
   }

   if (read_prefs) {
      database->read_prefs = mongoc_read_prefs_copy(read_prefs);
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_database_get_write_concern --
 *
 *       Fetches the write concern for @database.
 *
 * Returns:
 *       A mongoc_write_concern_t that should not be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const mongoc_write_concern_t *
mongoc_database_get_write_concern (const mongoc_database_t *database)
{
   bson_return_val_if_fail(database, NULL);

   return database->write_concern;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_database_set_write_concern --
 *
 *       Set the default write concern for @database.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_database_set_write_concern (mongoc_database_t            *database,
                                   const mongoc_write_concern_t *write_concern)
{
   bson_return_if_fail(database);

   if (database->write_concern) {
      mongoc_write_concern_destroy(database->write_concern);
      database->write_concern = NULL;
   }

   if (write_concern) {
      database->write_concern = mongoc_write_concern_copy(write_concern);
   }
}


/**
 * mongoc_database_has_collection:
 * @database: (in): A #mongoc_database_t.
 * @name: (in): The name of the collection to check for.
 * @error: (out) (allow-none): A location for a #bson_error_t, or %NULL.
 *
 * Checks to see if a collection exists within the database on the MongoDB
 * server.
 *
 * This will return %false if their was an error communicating with the
 * server, or if the collection does not exist.
 *
 * If @error is provided, it will first be zeroed. Upon error, error.domain
 * will be set.
 *
 * Returns: %true if @name exists, otherwise %false. @error may be set.
 */
bool
mongoc_database_has_collection (mongoc_database_t *database,
                                const char        *name,
                                bson_error_t      *error)
{
   mongoc_collection_t *collection;
   mongoc_read_prefs_t *read_prefs;
   mongoc_cursor_t *cursor;
   const bson_t *doc;
   bson_iter_t iter;
   bool ret = false;
   const char *cur_name;
   bson_t q = BSON_INITIALIZER;
   char ns[140];

   ENTRY;

   BSON_ASSERT (database);
   BSON_ASSERT (name);

   if (error) {
      memset (error, 0, sizeof *error);
   }

   bson_snprintf (ns, sizeof ns, "%s.%s", database->name, name);

   read_prefs = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);
   collection = mongoc_client_get_collection (database->client,
                                              database->name,
                                              "system.namespaces");
   cursor = mongoc_collection_find (collection, MONGOC_QUERY_NONE, 0, 0, 0, &q,
                                    NULL, read_prefs);

   while (!mongoc_cursor_error (cursor, error) &&
          mongoc_cursor_more (cursor)) {
      while (mongoc_cursor_next (cursor, &doc) &&
          bson_iter_init_find (&iter, doc, "name") &&
          BSON_ITER_HOLDS_UTF8 (&iter)) {
         cur_name = bson_iter_utf8(&iter, NULL);
         if (!strcmp(cur_name, ns)) {
            ret = true;
            GOTO(cleanup);
         }
      }
   }

cleanup:
   mongoc_cursor_destroy (cursor);
   mongoc_collection_destroy (collection);
   mongoc_read_prefs_destroy (read_prefs);

   RETURN(ret);
}


char **
mongoc_database_get_collection_names (mongoc_database_t *database,
                                      bson_error_t      *error)
{
   mongoc_collection_t *col;
   mongoc_cursor_t *cursor;
   uint32_t len;
   const bson_t *doc;
   bson_iter_t iter;
   const char *name;
   bson_t q = BSON_INITIALIZER;
   char **ret = NULL;
   int i = 0;

   BSON_ASSERT (database);

   col = mongoc_client_get_collection (database->client,
                                       database->name,
                                       "system.namespaces");

   cursor = mongoc_collection_find (col, MONGOC_QUERY_NONE, 0, 0, 0, &q,
                                    NULL, NULL);

   len = (int) strlen (database->name) + 1;

   while (mongoc_cursor_more (cursor) &&
          !mongoc_cursor_error (cursor, error)) {
      if (mongoc_cursor_next (cursor, &doc)) {
         if (bson_iter_init_find (&iter, doc, "name") &&
             BSON_ITER_HOLDS_UTF8 (&iter) &&
             (name = bson_iter_utf8 (&iter, NULL)) &&
             !strchr (name, '$') &&
             (0 == strncmp (name, database->name, len - 1))) {
            ret = bson_realloc (ret, sizeof(char*) * (i + 2));
            ret [i] = bson_strdup (bson_iter_utf8 (&iter, NULL) + len);
            ret [++i] = NULL;
         }
      }
   }

   if (!ret && !mongoc_cursor_error (cursor, error)) {
      ret = bson_malloc0 (sizeof (void*));
   }

   mongoc_cursor_destroy (cursor);
   mongoc_collection_destroy (col);

   return ret;
}


mongoc_collection_t *
mongoc_database_create_collection (mongoc_database_t *database,
                                   const char        *name,
                                   const bson_t      *options,
                                   bson_error_t      *error)
{
   mongoc_collection_t *collection = NULL;
   bson_iter_t iter;
   bson_t cmd;
   bool capped = false;

   bson_return_val_if_fail (database, NULL);
   bson_return_val_if_fail (name, NULL);

   if (strchr (name, '$')) {
      bson_set_error (error,
                      MONGOC_ERROR_NAMESPACE,
                      MONGOC_ERROR_NAMESPACE_INVALID,
                      "The namespace \"%s\" is invalid.",
                      name);
      return NULL;
   }

   if (options) {
      if (bson_iter_init_find (&iter, options, "capped")) {
         if (!BSON_ITER_HOLDS_BOOL (&iter)) {
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            "The argument \"capped\" must be a boolean.");
            return NULL;
         }
         capped = bson_iter_bool (&iter);
      }

      if (bson_iter_init_find (&iter, options, "autoIndexId") &&
          !BSON_ITER_HOLDS_BOOL (&iter)) {
         bson_set_error (error,
                         MONGOC_ERROR_COMMAND,
                         MONGOC_ERROR_COMMAND_INVALID_ARG,
                         "The argument \"autoIndexId\" must be a boolean.");
         return NULL;
      }

      if (bson_iter_init_find (&iter, options, "size")) {
         if (!BSON_ITER_HOLDS_INT32 (&iter) &&
             !BSON_ITER_HOLDS_INT64 (&iter)) {
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            "The argument \"size\" must be an integer.");
            return NULL;
         }
         if (!capped) {
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            "The \"size\" parameter requires {\"capped\": true}");
            return NULL;
         }
      }

      if (bson_iter_init_find (&iter, options, "max")) {
         if (!BSON_ITER_HOLDS_INT32 (&iter) &&
             !BSON_ITER_HOLDS_INT64 (&iter)) {
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            "The argument \"max\" must be an integer.");
            return NULL;
         }
         if (!capped) {
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            "The \"size\" parameter requires {\"capped\": true}");
            return NULL;
         }
      }
   }

   bson_init (&cmd);
   BSON_APPEND_UTF8 (&cmd, "create", name);

   if (options) {
      if (!bson_iter_init (&iter, options)) {
         bson_set_error (error,
                         MONGOC_ERROR_COMMAND,
                         MONGOC_ERROR_COMMAND_INVALID_ARG,
                         "The argument \"options\" is corrupt or invalid.");
         bson_destroy (&cmd);
         return NULL;
      }

      while (bson_iter_next (&iter)) {
         if (!bson_append_iter (&cmd, bson_iter_key (&iter), -1, &iter)) {
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            "Failed to append \"options\" to create command.");
            bson_destroy (&cmd);
            return NULL;
         }
      }
   }

   if (mongoc_database_command_simple (database, &cmd, NULL, NULL, error)) {
      collection = _mongoc_collection_new (database->client,
                                           database->name,
                                           name,
                                           database->read_prefs,
                                           database->write_concern);
   }

   bson_destroy (&cmd);

   return collection;
}


mongoc_collection_t *
mongoc_database_get_collection (mongoc_database_t *database,
                                const char        *collection)
{
   bson_return_val_if_fail (database, NULL);
   bson_return_val_if_fail (collection, NULL);

   return mongoc_client_get_collection (database->client, database->name,
                                        collection);
}


const char *
mongoc_database_get_name (mongoc_database_t *database)
{
   bson_return_val_if_fail (database, NULL);

   return database->name;
}

/*==============================================================*/
/* --- mongoc-gridfs.c */

#define MONGOC_GRIDFS_STREAM_CHUNK 4096


/**
 * _mongoc_gridfs_ensure_index:
 *
 * ensure gridfs indexes
 *
 * Ensure fast searches for chunks via [ files_id, n ]
 * Ensure fast searches for files via [ filename ]
 */
static bool
_mongoc_gridfs_ensure_index (mongoc_gridfs_t *gridfs,
                             bson_error_t    *error)
{
   bson_t keys;
   mongoc_index_opt_t opt;
   bool r;

   ENTRY;

   bson_init (&keys);

   bson_append_int32 (&keys, "files_id", -1, 1);
   bson_append_int32 (&keys, "n", -1, 1);

   mongoc_index_opt_init (&opt);
   opt.unique = 1;

   r = mongoc_collection_create_index (gridfs->chunks, &keys, &opt, error);

   bson_destroy (&keys);

   if (!r) { RETURN (r); }

   bson_init (&keys);

   bson_append_int32 (&keys, "filename", -1, 1);
   opt.unique = 0;

   r = mongoc_collection_create_index (gridfs->chunks, &keys, &opt, error);

   bson_destroy (&keys);

   if (!r) { RETURN (r); }

   RETURN (1);
}


mongoc_gridfs_t *
_mongoc_gridfs_new (mongoc_client_t *client,
                    const char      *db,
                    const char      *prefix,
                    bson_error_t    *error)
{
   mongoc_gridfs_t *gridfs;
   char buf[128];
   bool r;

   ENTRY;

   BSON_ASSERT (client);
   BSON_ASSERT (db);

   if (!prefix) {
      prefix = "fs";
   }

   /* make sure prefix is short enough to bucket the chunks and files
    * collections
    */
#ifndef BSON_DISABLE_ASSERT
   {
      uint32_t prefix_len;
      prefix_len = (uint32_t)strlen (prefix);
      BSON_ASSERT (prefix_len + sizeof (".chunks") < sizeof (buf));
   }
#endif

   gridfs = bson_malloc0 (sizeof *gridfs);

   gridfs->client = client;

   bson_snprintf (buf, sizeof(buf), "%s.chunks", prefix);
   gridfs->chunks = _mongoc_collection_new (client, db, buf, NULL, NULL);

   bson_snprintf (buf, sizeof(buf), "%s.files", prefix);
   gridfs->files = _mongoc_collection_new (client, db, buf, NULL, NULL);

   r = _mongoc_gridfs_ensure_index (gridfs, error);

   if (!r) { return NULL; }

   RETURN (gridfs);
}


bool
mongoc_gridfs_drop (mongoc_gridfs_t *gridfs,
                    bson_error_t    *error)
{
   bool r;

   ENTRY;

   r = mongoc_collection_drop (gridfs->files, error);
   if (!r) {
      RETURN (0);
   }

   r = mongoc_collection_drop (gridfs->chunks, error);
   if (!r) {
      RETURN (0);
   }

   RETURN (1);
}


void
mongoc_gridfs_destroy (mongoc_gridfs_t *gridfs)
{
   ENTRY;

   BSON_ASSERT (gridfs);

   mongoc_collection_destroy (gridfs->files);
   mongoc_collection_destroy (gridfs->chunks);

   bson_free (gridfs);

   EXIT;
}


/** find all matching gridfs files */
mongoc_gridfs_file_list_t *
mongoc_gridfs_find (mongoc_gridfs_t *gridfs,
                    const bson_t    *query)
{
   return _mongoc_gridfs_file_list_new (gridfs, query, 0);
}


/** find a single gridfs file */
mongoc_gridfs_file_t *
mongoc_gridfs_find_one (mongoc_gridfs_t *gridfs,
                        const bson_t    *query,
                        bson_error_t    *error)
{
   mongoc_gridfs_file_list_t *list;
   mongoc_gridfs_file_t *file;

   ENTRY;

   list = _mongoc_gridfs_file_list_new (gridfs, query, 1);

   file = mongoc_gridfs_file_list_next (list);
   mongoc_gridfs_file_list_error(list, error);

   mongoc_gridfs_file_list_destroy (list);

   RETURN (file);
}


/** find a single gridfs file by filename */
mongoc_gridfs_file_t *
mongoc_gridfs_find_one_by_filename (mongoc_gridfs_t *gridfs,
                                    const char      *filename,
                                    bson_error_t    *error)
{
   mongoc_gridfs_file_t *file;

   bson_t query;

   bson_init (&query);

   bson_append_utf8 (&query, "filename", -1, filename, -1);

   file = mongoc_gridfs_find_one (gridfs, &query, error);

   bson_destroy (&query);

   return file;
}


/** create a gridfs file from a stream
 *
 * The stream is fully consumed in creating the file
 */
mongoc_gridfs_file_t *
mongoc_gridfs_create_file_from_stream (mongoc_gridfs_t          *gridfs,
                                       mongoc_stream_t          *stream,
                                       mongoc_gridfs_file_opt_t *opt)
{
   mongoc_gridfs_file_t *file;
   ssize_t r;
   uint8_t buf[MONGOC_GRIDFS_STREAM_CHUNK];
   mongoc_iovec_t iov;
   int timeout;

   ENTRY;

   BSON_ASSERT (gridfs);
   BSON_ASSERT (stream);

   iov.iov_base = (void *)buf;
   iov.iov_len = 0;

   file = _mongoc_gridfs_file_new (gridfs, opt);
   timeout = gridfs->client->cluster.sockettimeoutms;

   for (;; ) {
      r = mongoc_stream_read (stream, iov.iov_base, MONGOC_GRIDFS_STREAM_CHUNK,
                              0, timeout);

      if (r > 0) {
         iov.iov_len = r;
         mongoc_gridfs_file_writev (file, &iov, 1, timeout);
      } else if (r == 0) {
         break;
      } else {
         mongoc_gridfs_file_destroy (file);
         RETURN (NULL);
      }
   }

   mongoc_stream_destroy (stream);

   mongoc_gridfs_file_seek (file, 0, SEEK_SET);

   RETURN (file);
}


/** create an empty gridfs file */
mongoc_gridfs_file_t *
mongoc_gridfs_create_file (mongoc_gridfs_t          *gridfs,
                           mongoc_gridfs_file_opt_t *opt)
{
   mongoc_gridfs_file_t *file;

   ENTRY;

   bson_return_val_if_fail (gridfs, NULL);

   file = _mongoc_gridfs_file_new (gridfs, opt);

   RETURN (file);
}

/** accessor functions for collections */
mongoc_collection_t *
mongoc_gridfs_get_files (mongoc_gridfs_t *gridfs)
{
   bson_return_val_if_fail (gridfs, NULL);

   return gridfs->files;
}

mongoc_collection_t *
mongoc_gridfs_get_chunks (mongoc_gridfs_t *gridfs)
{
   bson_return_val_if_fail (gridfs, NULL);

   return gridfs->chunks;
}


bool
mongoc_gridfs_remove_by_filename (mongoc_gridfs_t *gridfs,
                                  const char      *filename,
                                  bson_error_t    *error)
{
   mongoc_bulk_operation_t *bulk_files = NULL;
   mongoc_bulk_operation_t *bulk_chunks = NULL;
   mongoc_cursor_t *cursor = NULL;
   bson_error_t files_error;
   bson_error_t chunks_error;
   const bson_t *doc;
   const char *key;
   char keybuf[16];
   int count = 0;
   bool chunks_ret;
   bool files_ret;
   bool ret = false;
   bson_iter_t iter;
   bson_t *files_q = NULL;
   bson_t *chunks_q = NULL;
   bson_t q = BSON_INITIALIZER;
   bson_t fields = BSON_INITIALIZER;
   bson_t ar = BSON_INITIALIZER;

   bson_return_val_if_fail (gridfs, false);

   if (!filename) {
      bson_set_error (error,
                      MONGOC_ERROR_GRIDFS,
                      MONGOC_ERROR_GRIDFS_INVALID_FILENAME,
                      "A non-NULL filename must be specified.");
      return false;
   }

   /*
    * Find all files matching this filename. Hopefully just one, but not
    * strictly required!
    */

   BSON_APPEND_UTF8 (&q, "filename", filename);
   BSON_APPEND_INT32 (&fields, "_id", 1);

   cursor = mongoc_collection_find (gridfs->files, MONGOC_QUERY_NONE, 0, 0, 0,
                                    &q, &fields, NULL);
   BSON_ASSERT (cursor);

   while (mongoc_cursor_next (cursor, &doc)) {
      if (bson_iter_init_find (&iter, doc, "_id")) {
         const bson_value_t *value = bson_iter_value (&iter);

         bson_uint32_to_string (count, &key, keybuf, sizeof keybuf);
         BSON_APPEND_VALUE (&ar, key, value);
      }
   }

   if (mongoc_cursor_error (cursor, error)) {
      goto failure;
   }

   bulk_files = mongoc_collection_create_bulk_operation (gridfs->files, false, NULL);
   bulk_chunks = mongoc_collection_create_bulk_operation (gridfs->chunks, false, NULL);

   files_q = BCON_NEW ("_id", "{", "$in", BCON_ARRAY (&ar), "}");
   chunks_q = BCON_NEW ("files_id", "{", "$in", BCON_ARRAY (&ar), "}");

   mongoc_bulk_operation_remove (bulk_files, files_q);
   mongoc_bulk_operation_remove (bulk_chunks, chunks_q);

   files_ret = mongoc_bulk_operation_execute (bulk_files, NULL, &files_error);
   chunks_ret = mongoc_bulk_operation_execute (bulk_chunks, NULL, &chunks_error);

   if (error) {
      if (!files_ret) {
         memcpy (error, &files_error, sizeof *error);
      } else if (!chunks_ret) {
         memcpy (error, &chunks_error, sizeof *error);
      }
   }

   ret = (files_ret && chunks_ret);

failure:
   if (cursor) {
      mongoc_cursor_destroy (cursor);
   }
   if (bulk_files) {
      mongoc_bulk_operation_destroy (bulk_files);
   }
   if (bulk_chunks) {
      mongoc_bulk_operation_destroy (bulk_chunks);
   }
   bson_destroy (&q);
   bson_destroy (&fields);
   bson_destroy (&ar);
   if (files_q) {
      bson_destroy (files_q);
   }
   if (chunks_q) {
      bson_destroy (chunks_q);
   }

   return ret;
}

/*==============================================================*/
/* --- mongoc-gridfs-file.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "gridfs_file"

static bool
_mongoc_gridfs_file_refresh_page (mongoc_gridfs_file_t *file);

static bool
_mongoc_gridfs_file_flush_page (mongoc_gridfs_file_t *file);


/*****************************************************************
* Magic accessor generation
*
* We need some accessors to get and set properties on files, to handle memory
* ownership and to determine dirtiness.  These macros produce the getters and
* setters we need
*****************************************************************/

#define MONGOC_GRIDFS_FILE_STR_ACCESSOR(name) \
   const char * \
   mongoc_gridfs_file_get_##name (mongoc_gridfs_file_t * file) \
   { \
      return file->name ? file->name : file->bson_##name; \
   } \
   void \
      mongoc_gridfs_file_set_##name (mongoc_gridfs_file_t * file, \
                                     const char           *str)  \
   { \
      if (file->name) { \
         bson_free (file->name); \
      } \
      file->name = bson_strdup (str); \
      file->is_dirty = 1; \
   }

#define MONGOC_GRIDFS_FILE_BSON_ACCESSOR(name) \
   const bson_t * \
   mongoc_gridfs_file_get_##name (mongoc_gridfs_file_t * file) \
   { \
      if (file->name.len) { \
         return &file->name; \
      } else if (file->bson_##name.len) { \
         return &file->bson_##name; \
      } else { \
         return NULL; \
      } \
   } \
   void \
      mongoc_gridfs_file_set_##name (mongoc_gridfs_file_t * file, \
                                     const bson_t * bson) \
   { \
      if (file->name.len) { \
         bson_destroy (&file->name); \
      } \
      bson_copy_to (bson, &(file->name)); \
      file->is_dirty = 1; \
   }

MONGOC_GRIDFS_FILE_STR_ACCESSOR (md5)
MONGOC_GRIDFS_FILE_STR_ACCESSOR (filename)
MONGOC_GRIDFS_FILE_STR_ACCESSOR (content_type)
MONGOC_GRIDFS_FILE_BSON_ACCESSOR (aliases)
MONGOC_GRIDFS_FILE_BSON_ACCESSOR (metadata)


/** save a gridfs file */
bool
mongoc_gridfs_file_save (mongoc_gridfs_file_t *file)
{
   bson_t *selector, *update, child;
   const char *md5;
   const char *filename;
   const char *content_type;
   const bson_t *aliases;
   const bson_t *metadata;
   bool r;

   ENTRY;

   if (!file->is_dirty) {
      return 1;
   }

   if (file->page && _mongoc_gridfs_file_page_is_dirty (file->page)) {
      _mongoc_gridfs_file_flush_page (file);
   }

   md5 = mongoc_gridfs_file_get_md5 (file);
   filename = mongoc_gridfs_file_get_filename (file);
   content_type = mongoc_gridfs_file_get_content_type (file);
   aliases = mongoc_gridfs_file_get_aliases (file);
   metadata = mongoc_gridfs_file_get_metadata (file);

   selector = bson_new ();
   bson_append_value (selector, "_id", -1, &file->files_id);

   update = bson_new ();
   bson_append_document_begin (update, "$set", -1, &child);
   bson_append_int64 (&child, "length", -1, file->length);
   bson_append_int32 (&child, "chunkSize", -1, file->chunk_size);
   bson_append_date_time (&child, "uploadDate", -1, file->upload_date);

   if (md5) {
      bson_append_utf8 (&child, "md5", -1, md5, -1);
   }

   if (filename) {
      bson_append_utf8 (&child, "filename", -1, filename, -1);
   }

   if (content_type) {
      bson_append_utf8 (&child, "contentType", -1, content_type, -1);
   }

   if (aliases) {
      bson_append_array (&child, "aliases", -1, aliases);
   }

   if (metadata) {
      bson_append_document (&child, "metadata", -1, metadata);
   }

   bson_append_document_end (update, &child);

   r = mongoc_collection_update (file->gridfs->files, MONGOC_UPDATE_UPSERT,
                                 selector, update, NULL, &file->error);

   file->failed = !r;


   bson_destroy (selector);
   bson_destroy (update);

   file->is_dirty = 0;

   RETURN (r);
}


/**
 * _mongoc_gridfs_file_new_from_bson:
 *
 * creates a gridfs file from a bson object
 *
 * This is only really useful for instantiating a gridfs file from a server
 * side object
 */
mongoc_gridfs_file_t *
_mongoc_gridfs_file_new_from_bson (mongoc_gridfs_t *gridfs,
                                   const bson_t    *data)
{
   mongoc_gridfs_file_t *file;
   const bson_value_t *value;
   const char *key;
   bson_iter_t iter;
   const uint8_t *buf;
   uint32_t buf_len;

   ENTRY;

   BSON_ASSERT (gridfs);
   BSON_ASSERT (data);

   file = bson_malloc0 (sizeof *file);

   file->gridfs = gridfs;
   bson_copy_to (data, &file->bson);

   bson_iter_init (&iter, &file->bson);

   while (bson_iter_next (&iter)) {
      key = bson_iter_key (&iter);

      if (0 == strcmp (key, "_id")) {
         value = bson_iter_value (&iter);
         bson_value_copy (value, &file->files_id);
      } else if (0 == strcmp (key, "length")) {
         if (!BSON_ITER_HOLDS_INT32 (&iter) &&
             !BSON_ITER_HOLDS_INT64 (&iter) &&
             !BSON_ITER_HOLDS_DOUBLE (&iter)) {
            GOTO (failure);
         }
         file->length = bson_iter_as_int64 (&iter);
      } else if (0 == strcmp (key, "chunkSize")) {
         if (!BSON_ITER_HOLDS_INT32 (&iter) &&
             !BSON_ITER_HOLDS_INT64 (&iter) &&
             !BSON_ITER_HOLDS_DOUBLE (&iter)) {
            GOTO (failure);
         }
         if (bson_iter_as_int64 (&iter) > INT32_MAX) {
            GOTO (failure);
         }
         file->chunk_size = (int32_t)bson_iter_as_int64 (&iter);
      } else if (0 == strcmp (key, "uploadDate")) {
         if (!BSON_ITER_HOLDS_DATE_TIME (&iter)){
            GOTO (failure);
         }
         file->upload_date = bson_iter_date_time (&iter);
      } else if (0 == strcmp (key, "md5")) {
         if (!BSON_ITER_HOLDS_UTF8 (&iter)) {
            GOTO (failure);
         }
         file->bson_md5 = bson_iter_utf8 (&iter, NULL);
      } else if (0 == strcmp (key, "filename")) {
         if (!BSON_ITER_HOLDS_UTF8 (&iter)) {
            GOTO (failure);
         }
         file->bson_filename = bson_iter_utf8 (&iter, NULL);
      } else if (0 == strcmp (key, "contentType")) {
         if (!BSON_ITER_HOLDS_UTF8 (&iter)) {
            GOTO (failure);
         }
         file->bson_content_type = bson_iter_utf8 (&iter, NULL);
      } else if (0 == strcmp (key, "aliases")) {
         if (!BSON_ITER_HOLDS_ARRAY (&iter)) {
            GOTO  (failure);
         }
         bson_iter_array (&iter, &buf_len, &buf);
         bson_init_static (&file->bson_aliases, buf, buf_len);
      } else if (0 == strcmp (key, "metadata")) {
         if (!BSON_ITER_HOLDS_DOCUMENT (&iter)) {
            GOTO (failure);
         }
         bson_iter_document (&iter, &buf_len, &buf);
         bson_init_static (&file->bson_metadata, buf, buf_len);
      }
   }

   /* TODO: is there are a minimal object we should be verifying that we
    * actually have here? */

   RETURN (file);

failure:
   bson_destroy (&file->bson);

   RETURN (NULL);
}


/**
 * _mongoc_gridfs_file_new:
 *
 * Create a new empty gridfs file
 */
mongoc_gridfs_file_t *
_mongoc_gridfs_file_new (mongoc_gridfs_t          *gridfs,
                         mongoc_gridfs_file_opt_t *opt)
{
   mongoc_gridfs_file_t *file;
   mongoc_gridfs_file_opt_t default_opt = { 0 };

   ENTRY;

   BSON_ASSERT (gridfs);

   if (!opt) {
      opt = &default_opt;
   }

   file = bson_malloc0 (sizeof *file);

   file->gridfs = gridfs;
   file->is_dirty = 1;

   if (opt->chunk_size) {
      file->chunk_size = opt->chunk_size;
   } else {
      /*
       * The default chunk size is now 255kb. This used to be 256k but has been
       * reduced to allow for them to fit within power of two sizes in mongod.
       *
       * See CDRIVER-322.
       */
      file->chunk_size = (1 << 18) - 1024;
   }

   file->files_id.value_type = BSON_TYPE_OID;
   bson_oid_init (&file->files_id.value.v_oid, NULL);

   file->upload_date = time (NULL) * 1000;

   if (opt->md5) {
      file->md5 = bson_strdup (opt->md5);
   }

   if (opt->filename) {
      file->filename = bson_strdup (opt->filename);
   }

   if (opt->content_type) {
      file->content_type = bson_strdup (opt->content_type);
   }

   if (opt->aliases) {
      bson_copy_to (opt->aliases, &(file->aliases));
   }

   if (opt->metadata) {
      bson_copy_to (opt->metadata, &(file->metadata));
   }

   RETURN (file);
}

void
mongoc_gridfs_file_destroy (mongoc_gridfs_file_t *file)
{
   ENTRY;

   BSON_ASSERT (file);

   if (file->page) {
      _mongoc_gridfs_file_page_destroy (file->page);
   }

   if (file->bson.len) {
      bson_destroy (&file->bson);
   }

   if (file->cursor) {
      mongoc_cursor_destroy (file->cursor);
   }

   if (file->files_id.value_type) {
      bson_value_destroy (&file->files_id);
   }

   if (file->md5) {
      bson_free (file->md5);
   }

   if (file->filename) {
      bson_free (file->filename);
   }

   if (file->content_type) {
      bson_free (file->content_type);
   }

   if (file->aliases.len) {
      bson_destroy (&file->aliases);
   }

   if (file->bson_aliases.len) {
      bson_destroy (&file->bson_aliases);
   }

   if (file->metadata.len) {
      bson_destroy (&file->metadata);
   }

   if (file->bson_metadata.len) {
      bson_destroy (&file->bson_metadata);
   }

   bson_free (file);

   EXIT;
}


/** readv against a gridfs file */
ssize_t
mongoc_gridfs_file_readv (mongoc_gridfs_file_t *file,
                          mongoc_iovec_t       *iov,
                          size_t                iovcnt,
                          size_t                min_bytes,
                          uint32_t              timeout_msec)
{
   uint32_t bytes_read = 0;
   int32_t r;
   size_t i;
   uint32_t iov_pos;

   ENTRY;

   BSON_ASSERT (file);
   BSON_ASSERT (iov);
   BSON_ASSERT (iovcnt);
   BSON_ASSERT (timeout_msec <= INT_MAX);

   /* TODO: we should probably do something about timeout_msec here */

   if (!file->page) {
      _mongoc_gridfs_file_refresh_page (file);
   }

   for (i = 0; i < iovcnt; i++) {
      iov_pos = 0;

      for (;; ) {
         r = _mongoc_gridfs_file_page_read (file->page,
                                           (uint8_t *)iov[i].iov_base + iov_pos,
                                           (uint32_t)(iov[i].iov_len - iov_pos));
         BSON_ASSERT (r >= 0);

         iov_pos += r;
         file->pos += r;
         bytes_read += r;

         if (iov_pos == iov[i].iov_len) {
            /* filled a bucket, keep going */
            break;
         } else if ((uint64_t)file->length == file->pos) {
            /* we're at the end of the file.  So we're done */
            RETURN (bytes_read);
         } else if (bytes_read >= min_bytes) {
            /* we need a new page, but we've read enough bytes to stop */
            RETURN (bytes_read);
         } else {
            /* more to read, just on a new page */
            _mongoc_gridfs_file_refresh_page (file);
         }
      }
   }

   RETURN (bytes_read);
}


/** writev against a gridfs file */
ssize_t
mongoc_gridfs_file_writev (mongoc_gridfs_file_t *file,
                           mongoc_iovec_t       *iov,
                           size_t                iovcnt,
                           uint32_t              timeout_msec)
{
   uint32_t bytes_written = 0;
   int32_t r;
   size_t i;
   uint32_t iov_pos;

   ENTRY;

   BSON_ASSERT (file);
   BSON_ASSERT (iov);
   BSON_ASSERT (iovcnt);
   BSON_ASSERT (timeout_msec <= INT_MAX);

   /* TODO: we should probably do something about timeout_msec here */

   for (i = 0; i < iovcnt; i++) {
      iov_pos = 0;

      for (;; ) {
         if (!file->page) {
            _mongoc_gridfs_file_refresh_page (file);
         }

         r = _mongoc_gridfs_file_page_write (file->page,
                                            (uint8_t *)iov[i].iov_base + iov_pos,
                                            (uint32_t)(iov[i].iov_len - iov_pos));
         BSON_ASSERT (r >= 0);

         iov_pos += r;
         file->pos += r;
         bytes_written += r;

         file->length = MAX (file->length, (int64_t)file->pos);

         if (iov_pos == iov[i].iov_len) {
            /** filled a bucket, keep going */
            break;
         } else {
            /** flush the buffer, the next pass through will bring in a new page
             *
             * Our file pointer is now on the new page, so push it back one so
             * that flush knows to flush the old page rather than a new one.
             * This is a little hacky
             */
            file->pos--;
            _mongoc_gridfs_file_flush_page (file);
            file->pos++;
         }
      }
   }

   file->is_dirty = 1;

   RETURN (bytes_written);
}


/** flush a gridfs file's current page to the db */
static bool
_mongoc_gridfs_file_flush_page (mongoc_gridfs_file_t *file)
{
   bson_t *selector, *update;
   bool r;
   const uint8_t *buf;
   uint32_t len;

   ENTRY;
   BSON_ASSERT (file);
   BSON_ASSERT (file->page);

   buf = _mongoc_gridfs_file_page_get_data (file->page);
   len = _mongoc_gridfs_file_page_get_len (file->page);

   selector = bson_new ();

   bson_append_value (selector, "files_id", -1, &file->files_id);
   bson_append_int32 (selector, "n", -1, (int32_t)(file->pos / file->chunk_size));

   update = bson_sized_new (file->chunk_size + 100);

   bson_append_value (update, "files_id", -1, &file->files_id);
   bson_append_int32 (update, "n", -1, (int32_t)(file->pos / file->chunk_size));
   bson_append_binary (update, "data", -1, BSON_SUBTYPE_BINARY, buf, len);

   r = mongoc_collection_update (file->gridfs->chunks, MONGOC_UPDATE_UPSERT,
                                 selector, update, NULL, &file->error);

   file->failed = !r;

   bson_destroy (selector);
   bson_destroy (update);

   if (r) {
      _mongoc_gridfs_file_page_destroy (file->page);
      file->page = NULL;
      r = mongoc_gridfs_file_save (file);
   }

   RETURN (r);
}


/** referesh a gridfs file's underlying page
 *
 * This unconditionally fetches the current page, even if the current page
 * covers the same theoretical chunk.
 */
static bool
_mongoc_gridfs_file_refresh_page (mongoc_gridfs_file_t *file)
{
   bson_t *query, *fields, child, child2;
   const bson_t *chunk;
   const char *key;
   bson_iter_t iter;

   uint32_t n;
   const uint8_t *data;
   uint32_t len;

   ENTRY;

   BSON_ASSERT (file);

   n = (uint32_t)(file->pos / file->chunk_size);

   if (file->page) {
      _mongoc_gridfs_file_page_destroy (file->page);
      file->page = NULL;
   }

   /* if the file pointer is past the end of the current file (I.e. pointing to
    * a new chunk) and we're on a chunk boundary, we'll pass the page
    * constructor a new empty page */
   if ((int64_t)file->pos >= file->length && !(file->pos % file->chunk_size)) {
      data = (uint8_t *)"";
      len = 0;
   } else {
      /* if we have a cursor, but the cursor doesn't have the chunk we're going
       * to need, destroy it (we'll grab a new one immediately there after) */
      if (file->cursor &&
          !(file->cursor_range[0] >= n && file->cursor_range[1] <= n)) {
         mongoc_cursor_destroy (file->cursor);
         file->cursor = NULL;
      }

      if (!file->cursor) {
         query = bson_new ();

         bson_append_document_begin(query, "$query", -1, &child);
            bson_append_value (&child, "files_id", -1, &file->files_id);

            bson_append_document_begin (&child, "n", -1, &child2);
               bson_append_int32 (&child2, "$gte", -1, (int32_t)(file->pos / file->chunk_size));
            bson_append_document_end (&child, &child2);
         bson_append_document_end(query, &child);

         bson_append_document_begin(query, "$orderby", -1, &child);
            bson_append_int32 (&child, "n", -1, 1);
         bson_append_document_end(query, &child);

         fields = bson_new ();
         bson_append_int32 (fields, "n", -1, 1);
         bson_append_int32 (fields, "data", -1, 1);
         bson_append_int32 (fields, "_id", -1, 0);

         /* find all chunks greater than or equal to our current file pos */
         file->cursor = mongoc_collection_find (file->gridfs->chunks,
                                                MONGOC_QUERY_NONE, 0, 0, 0, query,
                                                fields, NULL);

         file->cursor_range[0] = n;
         file->cursor_range[1] = (uint32_t)(file->length / file->chunk_size);

         bson_destroy (query);
         bson_destroy (fields);

         BSON_ASSERT (file->cursor);
      }

      /* we might have had a cursor before, then seeked ahead past a chunk.
       * iterate until we're on the right chunk */
      while (file->cursor_range[0] <= n) {
         if (!mongoc_cursor_next (file->cursor, &chunk)) {
            if (file->cursor->failed) {
               memcpy (&(file->error), &(file->cursor->error),
                       sizeof (bson_error_t));
               file->failed = true;
            }

            RETURN (0);
         }

         file->cursor_range[0]++;
      }

      bson_iter_init (&iter, chunk);

      /* grab out what we need from the chunk */
      while (bson_iter_next (&iter)) {
         key = bson_iter_key (&iter);

         if (strcmp (key, "n") == 0) {
            n = bson_iter_int32 (&iter);
         } else if (strcmp (key, "data") == 0) {
            bson_iter_binary (&iter, NULL, &len, &data);
         } else {
            RETURN (0);
         }
      }

      /* we're on the wrong chunk somehow... probably because our gridfs is
       * missing chunks.
       *
       * TODO: maybe we should make more noise here?
       */

      if (!(n == file->pos / file->chunk_size)) {
         return 0;
      }
   }

   file->page = _mongoc_gridfs_file_page_new (data, len, file->chunk_size);

   /* seek in the page towards wherever we're supposed to be */
   RETURN (_mongoc_gridfs_file_page_seek (file->page, file->pos %
                                         file->chunk_size));
}


/** Seek in a gridfs file to a given location
 *
 * @param whence is regular fseek whence.  I.e. SEEK_SET, SEEK_CUR or SEEK_END
 *
 */
int
mongoc_gridfs_file_seek (mongoc_gridfs_file_t *file,
                         uint64_t         delta,
                         int                   whence)
{
   uint64_t offset;

   BSON_ASSERT(file);

   switch (whence) {
   case SEEK_SET:
      offset = delta;
      break;
   case SEEK_CUR:
      offset = file->pos + delta;
      break;
   case SEEK_END:
      offset = (file->length - 1) + delta;
      break;
   default:
      errno = EINVAL;
      return -1;

      break;
   }

   BSON_ASSERT (file->length > (int64_t)offset);

   if (offset % file->chunk_size != file->pos % file->chunk_size) {
      /** no longer on the same page */

      if (file->page) {
         if (_mongoc_gridfs_file_page_is_dirty (file->page)) {
            _mongoc_gridfs_file_flush_page (file);
         } else {
            _mongoc_gridfs_file_page_destroy (file->page);
         }
      }

      /** we'll pick up the seek when we fetch a page on the next action.  We lazily load */
   } else {
      _mongoc_gridfs_file_page_seek (file->page, offset % file->chunk_size);
   }

   file->pos = offset;

   return 0;
}

uint64_t
mongoc_gridfs_file_tell (mongoc_gridfs_file_t *file)
{
   BSON_ASSERT(file);

   return file->pos;
}

bool
mongoc_gridfs_file_error (mongoc_gridfs_file_t *file,
                          bson_error_t         *error)
{
   BSON_ASSERT(file);
   BSON_ASSERT(error);

   if (BSON_UNLIKELY(file->failed)) {
      bson_set_error(error,
                     file->error.domain,
                     file->error.code,
                     "%s",
                     file->error.message);
      RETURN(true);
   }

   RETURN(false);
}

int64_t
mongoc_gridfs_file_get_length (mongoc_gridfs_file_t *file)
{
   bson_return_val_if_fail (file, -1);

   return file->length;
}

int32_t
mongoc_gridfs_file_get_chunk_size (mongoc_gridfs_file_t *file)
{
   bson_return_val_if_fail (file, -1);

   return file->chunk_size;
}

int64_t
mongoc_gridfs_file_get_upload_date (mongoc_gridfs_file_t *file)
{
   bson_return_val_if_fail (file, -1);

   return file->upload_date;
}

bool
mongoc_gridfs_file_remove (mongoc_gridfs_file_t *file,
                           bson_error_t         *error)
{
   bson_t sel = BSON_INITIALIZER;
   bool ret = false;

   bson_return_val_if_fail (file, false);

   BSON_APPEND_VALUE (&sel, "_id", &file->files_id);

   if (!mongoc_collection_remove (file->gridfs->files,
                                  MONGOC_REMOVE_SINGLE_REMOVE,
                                  &sel,
                                  NULL,
                                  error)) {
      goto cleanup;
   }

   bson_reinit (&sel);
   BSON_APPEND_VALUE (&sel, "files_id", &file->files_id);

   if (!mongoc_collection_remove (file->gridfs->chunks,
                                  MONGOC_REMOVE_NONE,
                                  &sel,
                                  NULL,
                                  error)) {
      goto cleanup;
   }

   ret = true;

cleanup:
   bson_destroy (&sel);

   return ret;
}

/*==============================================================*/
/* --- mongoc-gridfs-file-list.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "gridfs_file_list"


mongoc_gridfs_file_list_t *
_mongoc_gridfs_file_list_new (mongoc_gridfs_t *gridfs,
                              const bson_t    *query,
                              uint32_t    limit)
{
   mongoc_gridfs_file_list_t *list;
   mongoc_cursor_t *cursor;

   cursor = mongoc_collection_find (gridfs->files, MONGOC_QUERY_NONE, 0, limit, 0,
                                    query, NULL, NULL);

   BSON_ASSERT (cursor);

   list = bson_malloc0 (sizeof *list);

   list->cursor = cursor;
   list->gridfs = gridfs;

   return list;
}


mongoc_gridfs_file_t *
mongoc_gridfs_file_list_next (mongoc_gridfs_file_list_t *list)
{
   const bson_t *bson;

   BSON_ASSERT (list);

   if (mongoc_cursor_next (list->cursor, &bson)) {
      return _mongoc_gridfs_file_new_from_bson (list->gridfs, bson);
   } else {
      return NULL;
   }
}


bool
mongoc_gridfs_file_list_error (mongoc_gridfs_file_list_t *list,
                               bson_error_t              *error)
{
   return mongoc_cursor_error(list->cursor, error);
}


void
mongoc_gridfs_file_list_destroy (mongoc_gridfs_file_list_t *list)
{
   BSON_ASSERT (list);

   mongoc_cursor_destroy (list->cursor);
   bson_free (list);
}

/*==============================================================*/
/* --- mongoc-gridfs-file-page.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "gridfs_file_page"

/** create a new page from a buffer
 *
 * The buffer should stick around for the life of the page
 */
mongoc_gridfs_file_page_t *
_mongoc_gridfs_file_page_new (const uint8_t *data,
                              uint32_t       len,
                              uint32_t       chunk_size)
{
   mongoc_gridfs_file_page_t *page;

   ENTRY;

   BSON_ASSERT (data);
   BSON_ASSERT (len <= chunk_size);

   page = bson_malloc0 (sizeof *page);

   page->chunk_size = chunk_size;
   page->read_buf = data;
   page->len = len;

   RETURN (page);
}


bool
_mongoc_gridfs_file_page_seek (mongoc_gridfs_file_page_t *page,
                               uint32_t              offset)
{
   ENTRY;

   BSON_ASSERT (page);

   BSON_ASSERT (offset <= page->len);

   page->offset = offset;

   RETURN (1);
}


int32_t
_mongoc_gridfs_file_page_read (mongoc_gridfs_file_page_t *page,
                               void                      *dst,
                               uint32_t              len)
{
   int bytes_read;
   const uint8_t *src;

   ENTRY;

   BSON_ASSERT (page);
   BSON_ASSERT (dst);

   bytes_read = MIN (len, page->len - page->offset);

   src = page->read_buf ? page->read_buf : page->buf;

   memcpy (dst, src + page->offset, bytes_read);

   page->offset += bytes_read;

   RETURN (bytes_read);
}


/**
 * _mongoc_gridfs_file_page_write:
 *
 * writes to a page
 *
 * writes are copy on write as regards the buf passed during construction.
 * I.e. the first write allocs a buf large enough for the chunk_size, which
 * because authoritative from then on out
 */
int32_t
_mongoc_gridfs_file_page_write (mongoc_gridfs_file_page_t *page,
                                const void                *src,
                                uint32_t              len)
{
   int bytes_written;

   ENTRY;

   BSON_ASSERT (page);
   BSON_ASSERT (src);

   bytes_written = MIN (len, page->chunk_size - page->offset);

   if (!page->buf) {
      page->buf = bson_malloc (page->chunk_size);
      memcpy (page->buf, page->read_buf, MIN (page->chunk_size, page->len));
   }

   memcpy (page->buf + page->offset, src, bytes_written);
   page->offset += bytes_written;

   page->len = MAX (page->offset, page->len);

   RETURN (bytes_written);
}


const uint8_t *
_mongoc_gridfs_file_page_get_data (mongoc_gridfs_file_page_t *page)
{
   ENTRY;

   BSON_ASSERT (page);

   RETURN (page->buf ? page->buf : page->read_buf);
}


uint32_t
_mongoc_gridfs_file_page_get_len (mongoc_gridfs_file_page_t *page)
{
   ENTRY;

   BSON_ASSERT (page);

   RETURN (page->len);
}


uint32_t
_mongoc_gridfs_file_page_tell (mongoc_gridfs_file_page_t *page)
{
   ENTRY;

   BSON_ASSERT (page);

   RETURN (page->offset);
}


bool
_mongoc_gridfs_file_page_is_dirty (mongoc_gridfs_file_page_t *page)
{
   ENTRY;

   BSON_ASSERT (page);

   RETURN (page->buf ? 1 : 0);
}


void
_mongoc_gridfs_file_page_destroy (mongoc_gridfs_file_page_t *page)
{
   ENTRY;

   if (page->buf) { bson_free (page->buf); }

   bson_free (page);

   EXIT;
}

/*==============================================================*/
/* --- mongoc-index.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "gridfs_index"


static mongoc_index_opt_t gMongocIndexOptDefault = {
   1,
   0,
   0,
   NULL,
   0,
   0,
   -1,
   -1,
};


const mongoc_index_opt_t *
mongoc_index_opt_get_default (void)
{
   return &gMongocIndexOptDefault;
}


void
mongoc_index_opt_init (mongoc_index_opt_t *opt)
{
   BSON_ASSERT (opt);

   memcpy (opt, &gMongocIndexOptDefault, sizeof *opt);
}

/*==============================================================*/
/* --- mongoc-init.c */

static MONGOC_ONCE_FUN( _mongoc_do_init)
{
#ifdef MONGOC_ENABLE_SSL
   _mongoc_ssl_init();
#endif

   _mongoc_counters_init();

#ifdef _WIN32
   {
      WORD wVersionRequested;
      WSADATA wsaData;
      int err;

      wVersionRequested = MAKEWORD (2, 2);

      err = WSAStartup (wVersionRequested, &wsaData);

      /* check the version perhaps? */

      assert (err == 0);

      atexit ((void(*)(void))WSACleanup);
   }
#endif

   MONGOC_ONCE_RETURN;
}

void
mongoc_init (void)
{
   static mongoc_once_t once = MONGOC_ONCE_INIT;
   mongoc_once (&once, _mongoc_do_init);
}

static MONGOC_ONCE_FUN( _mongoc_do_cleanup)
{
#ifdef MONGOC_ENABLE_SSL
   _mongoc_ssl_cleanup();
#endif

#ifdef _WIN32
   WSACleanup ();
#endif

   MONGOC_ONCE_RETURN;
}

void
mongoc_cleanup (void)
{
   static mongoc_once_t once = MONGOC_ONCE_INIT;
   mongoc_once (&once, _mongoc_do_cleanup);
}

/*
 * On GCC, just use __attribute__((constructor)) to perform initialization
 * automatically for the application.
 */
#ifdef __GNUC__
static void _mongoc_init_ctor (void) __attribute__((constructor));
static void
_mongoc_init_ctor (void)
{
   mongoc_init ();
}

static void _mongoc_init_dtor (void) __attribute__((destructor));
static void
_mongoc_init_dtor (void)
{
   mongoc_cleanup ();
}
#endif

/*==============================================================*/
/* --- mongoc-list.c */

/**
 * mongoc_list_append:
 * @list: A list to append to, or NULL.
 * @data: Data to append to @list.
 *
 * Appends a new link onto the linked list.
 *
 * Returns: @list or a new list if @list is NULL.
 */
mongoc_list_t *
_mongoc_list_append (mongoc_list_t *list,
                     void          *data)
{
   mongoc_list_t *item;
   mongoc_list_t *iter;

   item = bson_malloc0(sizeof *item);
   item->data = data;
   if (!list) {
      return item;
   }

   for (iter = list; iter->next; iter = iter->next) { }
   iter->next = item;

   return list;
}


/**
 * mongoc_list_prepend:
 * @list: A mongoc_list_t or NULL.
 * @data: data to prepend to the list.
 *
 * Prepends to @list a new link containing @data.
 *
 * Returns: A new link containing data with @list following.
 */
mongoc_list_t *
_mongoc_list_prepend (mongoc_list_t *list,
                      void          *data)
{
   mongoc_list_t *item;

   item = bson_malloc0(sizeof *item);
   item->data = data;
   item->next = list;

   return item;
}


/**
 * mongoc_list_remove:
 * @list: A mongoc_list_t.
 * @data: Data to remove from @list.
 *
 * Removes the link containing @data from @list.
 *
 * Returns: @list with the link containing @data removed.
 */
mongoc_list_t *
_mongoc_list_remove (mongoc_list_t *list,
                     void          *data)
{
   mongoc_list_t *iter;
   mongoc_list_t *prev = NULL;
   mongoc_list_t *ret = list;

   bson_return_val_if_fail(list, NULL);

   for (iter = list; iter; iter = iter->next) {
      if (iter->data == data) {
         if (iter != list) {
            prev->next = iter->next;
         } else {
            ret = iter->next;
         }
         bson_free(iter);
         break;
      }
      prev = iter;
   }

   return ret;
}


/**
 * mongoc_list_foreach:
 * @list: A mongoc_list_t or NULL.
 * @func: A func to call for each link in @list.
 * @user_data: User data for @func.
 *
 * Calls @func for each item in @list.
 */
void
_mongoc_list_foreach (mongoc_list_t *list,
                      void (*func) (void *data, void *user_data),
                      void          *user_data)
{
   mongoc_list_t *iter;

   bson_return_if_fail(func);

   for (iter = list; iter; iter = iter->next) {
      func(iter->data, user_data);
   }
}


/**
 * mongoc_list_destroy:
 * @list: A mongoc_list_t.
 *
 * Destroys @list and releases any resources.
 */
void
_mongoc_list_destroy (mongoc_list_t *list)
{
   mongoc_list_t *tmp = list;

   while (list) {
      tmp = list->next;
      bson_free(list);
      list = tmp;
   }
}

/*==============================================================*/
/* --- mongoc-log.c */

static mongoc_mutex_t       gLogMutex;
static mongoc_log_func_t  gLogFunc = mongoc_log_default_handler;
static void              *gLogData;

static MONGOC_ONCE_FUN( _mongoc_ensure_mutex_once)
{
   mongoc_mutex_init(&gLogMutex);

   MONGOC_ONCE_RETURN;
}

void
mongoc_log_set_handler (mongoc_log_func_t  log_func,
                        void              *user_data)
{
   static mongoc_once_t once = MONGOC_ONCE_INIT;
   mongoc_once(&once, &_mongoc_ensure_mutex_once);

   mongoc_mutex_lock(&gLogMutex);
   gLogFunc = log_func;
   gLogData = user_data;
   mongoc_mutex_unlock(&gLogMutex);
}


void
mongoc_log (mongoc_log_level_t  log_level,
            const char         *log_domain,
            const char         *format,
            ...)
{
   va_list args;
   char *message;
   static mongoc_once_t once = MONGOC_ONCE_INIT;

   mongoc_once(&once, &_mongoc_ensure_mutex_once);

   bson_return_if_fail(format);

   va_start(args, format);
   message = bson_strdupv_printf(format, args);
   va_end(args);

   mongoc_mutex_lock(&gLogMutex);
   gLogFunc(log_level, log_domain, message, gLogData);
   mongoc_mutex_unlock(&gLogMutex);

   bson_free(message);
}


const char *
mongoc_log_level_str (mongoc_log_level_t log_level)
{
   switch (log_level) {
   case MONGOC_LOG_LEVEL_ERROR:
      return "ERROR";
   case MONGOC_LOG_LEVEL_CRITICAL:
      return "CRITICAL";
   case MONGOC_LOG_LEVEL_WARNING:
      return "WARNING";
   case MONGOC_LOG_LEVEL_MESSAGE:
      return "MESSAGE";
   case MONGOC_LOG_LEVEL_INFO:
      return "INFO";
   case MONGOC_LOG_LEVEL_DEBUG:
      return "DEBUG";
   case MONGOC_LOG_LEVEL_TRACE:
      return "TRACE";
   default:
      return "UNKNOWN";
   }
}


void
mongoc_log_default_handler (mongoc_log_level_t  log_level,
                            const char         *log_domain,
                            const char         *message,
                            void               *user_data)
{
   struct timeval tv;
   struct timezone tz;
   struct tm tt;
   time_t t;
   FILE *stream;
   char nowstr[32];
   int pid;

   bson_gettimeofday(&tv, &tz);
   t = tv.tv_sec;

#ifdef _WIN32
#  ifdef _MSC_VER
     localtime_s(&tt, &t);
#  else
     tt = *(localtime(&t));
#  endif
#else
   localtime_r(&t, &tt);
#endif

   strftime (nowstr, sizeof nowstr, "%Y/%m/%d %H:%M:%S", &tt);

   switch (log_level) {
   case MONGOC_LOG_LEVEL_ERROR:
   case MONGOC_LOG_LEVEL_CRITICAL:
   case MONGOC_LOG_LEVEL_WARNING:
      stream = stderr;
      break;
   case MONGOC_LOG_LEVEL_MESSAGE:
   case MONGOC_LOG_LEVEL_INFO:
   case MONGOC_LOG_LEVEL_DEBUG:
   case MONGOC_LOG_LEVEL_TRACE:
   default:
      stream = stdout;
   }

#ifdef __linux__
   pid = syscall (SYS_gettid);
#elif defined(_WIN32)
   pid = (int)_getpid ();
#else
   pid = (int)getpid ();
#endif

   fprintf (stream,
            "%s.%04ld: [%5d]: %8s: %12s: %s\n",
            nowstr,
            tv.tv_usec / 1000L,
            pid,
            mongoc_log_level_str(log_level),
            log_domain,
            message);
}

/*==============================================================*/
/* --- mongoc-matcher.c */

static mongoc_matcher_op_t *
_mongoc_matcher_parse_logical (mongoc_matcher_opcode_t  opcode,
                               bson_iter_t             *iter,
                               bool                     is_root,
                               bson_error_t            *error);


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_parse_compare --
 *
 *       Parse a compare spec such as $gt or $in.
 *
 *       See the following link for more information.
 *
 *          http://docs.mongodb.org/manual/reference/operator/query/
 *
 * Returns:
 *       A newly allocated mongoc_matcher_op_t if successful; otherwise
 *       NULL and @error is set.
 *
 * Side effects:
 *       @error may be set.
 *
 *--------------------------------------------------------------------------
 */

static mongoc_matcher_op_t *
_mongoc_matcher_parse_compare (bson_iter_t  *iter,  /* IN */
                               const char   *path,  /* IN */
                               bson_error_t *error) /* OUT */
{
   const char * key;
   mongoc_matcher_op_t * op = NULL, * op_child;
   bson_iter_t child;

   BSON_ASSERT (iter);
   BSON_ASSERT (path);

   if (bson_iter_type (iter) == BSON_TYPE_DOCUMENT) {
      if (!bson_iter_recurse (iter, &child) ||
          !bson_iter_next (&child)) {
         bson_set_error (error,
                         MONGOC_ERROR_MATCHER,
                         MONGOC_ERROR_MATCHER_INVALID,
                         "Document contains no operations.");
         return NULL;
      }

      key = bson_iter_key (&child);

      if (key[0] != '$') {
         op = _mongoc_matcher_op_compare_new (MONGOC_MATCHER_OPCODE_EQ, path,
                                              iter);
      } else if (strcmp(key, "$not") == 0) {
         if (!(op_child = _mongoc_matcher_parse_compare (&child, path,
                                                         error))) {
            return NULL;
         }
         op = _mongoc_matcher_op_not_new (path, op_child);
      } else if (strcmp(key, "$gt") == 0) {
         op = _mongoc_matcher_op_compare_new (MONGOC_MATCHER_OPCODE_GT, path,
                                              &child);
      } else if (strcmp(key, "$gte") == 0) {
         op = _mongoc_matcher_op_compare_new (MONGOC_MATCHER_OPCODE_GTE, path,
                                              &child);
      } else if (strcmp(key, "$in") == 0) {
         op = _mongoc_matcher_op_compare_new (MONGOC_MATCHER_OPCODE_IN, path,
                                              &child);
      } else if (strcmp(key, "$lt") == 0) {
         op = _mongoc_matcher_op_compare_new (MONGOC_MATCHER_OPCODE_LT, path,
                                              &child);
      } else if (strcmp(key, "$lte") == 0) {
         op = _mongoc_matcher_op_compare_new (MONGOC_MATCHER_OPCODE_LTE, path,
                                              &child);
      } else if (strcmp(key, "$ne") == 0) {
         op = _mongoc_matcher_op_compare_new (MONGOC_MATCHER_OPCODE_NE, path,
                                              &child);
      } else if (strcmp(key, "$nin") == 0) {
         op = _mongoc_matcher_op_compare_new (MONGOC_MATCHER_OPCODE_NIN, path,
                                              &child);
      } else if (strcmp(key, "$exists") == 0) {
         op = _mongoc_matcher_op_exists_new (path, bson_iter_bool (&child));
      } else if (strcmp(key, "$type") == 0) {
         op = _mongoc_matcher_op_type_new (path, bson_iter_type (&child));
      } else {
         bson_set_error (error,
                         MONGOC_ERROR_MATCHER,
                         MONGOC_ERROR_MATCHER_INVALID,
                         "Invalid operator \"%s\"",
                         key);
         return NULL;
      }
   } else {
      op = _mongoc_matcher_op_compare_new (MONGOC_MATCHER_OPCODE_EQ, path, iter);
   }

   BSON_ASSERT (op);

   return op;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_parse --
 *
 *       Parse a query spec observed by the current key of @iter.
 *
 * Returns:
 *       A newly allocated mongoc_matcher_op_t if successful; otherwise
 *       NULL an @error is set.
 *
 * Side effects:
 *       @error may be set.
 *
 *--------------------------------------------------------------------------
 */

static mongoc_matcher_op_t *
_mongoc_matcher_parse (bson_iter_t  *iter,  /* IN */
                       bson_error_t *error) /* OUT */
{
   bson_iter_t child;
   const char *key;

   BSON_ASSERT (iter);

   key = bson_iter_key (iter);

   if (*key != '$') {
      return _mongoc_matcher_parse_compare (iter, key, error);
   } else {
      BSON_ASSERT (bson_iter_type(iter) == BSON_TYPE_ARRAY);

      if (!bson_iter_recurse (iter, &child)) {
         bson_set_error (error,
                         MONGOC_ERROR_MATCHER,
                         MONGOC_ERROR_MATCHER_INVALID,
                         "Invalid value for operator \"%s\"",
                         key);
         return NULL;
      }

      if (strcmp (key, "$or") == 0) {
         return _mongoc_matcher_parse_logical (MONGOC_MATCHER_OPCODE_OR,
                                               &child, false, error);
      } else if (strcmp(key, "$and") == 0) {
         return _mongoc_matcher_parse_logical (MONGOC_MATCHER_OPCODE_AND,
                                               &child, false, error);
      } else if (strcmp(key, "$nor") == 0) {
         return _mongoc_matcher_parse_logical (MONGOC_MATCHER_OPCODE_NOR,
                                               &child, false, error);
      }
   }

   bson_set_error (error,
                   MONGOC_ERROR_MATCHER,
                   MONGOC_ERROR_MATCHER_INVALID,
                   "Invalid operator \"%s\"",
                   key);

   return NULL;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_parse_logical --
 *
 *       Parse a query spec containing a logical operator such as
 *       $or, $and, $not, and $nor.
 *
 *       See the following link for more information.
 *
 *       http://docs.mongodb.org/manual/reference/operator/query/
 *
 * Returns:
 *       A newly allocated mongoc_matcher_op_t if successful; otherwise
 *       NULL and @error is set.
 *
 * Side effects:
 *       @error may be set.
 *
 *--------------------------------------------------------------------------
 */

static mongoc_matcher_op_t *
_mongoc_matcher_parse_logical (mongoc_matcher_opcode_t  opcode,  /* IN */
                               bson_iter_t             *iter,    /* IN */
                               bool                     is_root, /* IN */
                               bson_error_t            *error)   /* OUT */
{
   mongoc_matcher_op_t *left;
   mongoc_matcher_op_t *right;
   mongoc_matcher_op_t *more;
   mongoc_matcher_op_t *more_wrap;
   bson_iter_t child;

   BSON_ASSERT (opcode);
   BSON_ASSERT (iter);
   BSON_ASSERT (iter);

   if (!bson_iter_next (iter)) {
      bson_set_error (error,
                      MONGOC_ERROR_MATCHER,
                      MONGOC_ERROR_MATCHER_INVALID,
                      "Invalid logical operator.");
      return NULL;
   }

   if (is_root) {
      if (!(left = _mongoc_matcher_parse (iter, error))) {
         return NULL;
      }
   } else {
      if (!BSON_ITER_HOLDS_DOCUMENT (iter)) {
         bson_set_error (error,
                         MONGOC_ERROR_MATCHER,
                         MONGOC_ERROR_MATCHER_INVALID,
                         "Expected document in value.");
         return NULL;
      }

      bson_iter_recurse (iter, &child);
      bson_iter_next (&child);

      if (!(left = _mongoc_matcher_parse (&child, error))) {
         return NULL;
      }
   }

   if (!bson_iter_next (iter)) {
      return left;
   }

   if (is_root) {
      if (!(right = _mongoc_matcher_parse (iter, error))) {
         return NULL;
      }
   } else {
      if (!BSON_ITER_HOLDS_DOCUMENT (iter)) {
         bson_set_error (error,
                         MONGOC_ERROR_MATCHER,
                         MONGOC_ERROR_MATCHER_INVALID,
                         "Expected document in value.");
         return NULL;
      }

      bson_iter_recurse (iter, &child);
      bson_iter_next (&child);

      if (!(right = _mongoc_matcher_parse (&child, error))) {
         return NULL;
      }
   }

   more = _mongoc_matcher_parse_logical (opcode, iter, is_root, error);

   if (more) {
      more_wrap = _mongoc_matcher_op_logical_new (opcode, right, more);
      return _mongoc_matcher_op_logical_new (opcode, left, more_wrap);
   }

   return _mongoc_matcher_op_logical_new (opcode, left, right);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_matcher_new --
 *
 *       Create a new mongoc_matcher_t using the query specification
 *       provided in @query.
 *
 *       This will build an operation tree that can be applied to arbitrary
 *       bson documents using mongoc_matcher_match().
 *
 * Returns:
 *       A newly allocated mongoc_matcher_t if successful; otherwise NULL
 *       and @error is set.
 *
 *       The mongoc_matcher_t should be freed with
 *       mongoc_matcher_destroy().
 *
 * Side effects:
 *       @error may be set.
 *
 *--------------------------------------------------------------------------
 */

mongoc_matcher_t *
mongoc_matcher_new (const bson_t *query, /* IN */
                    bson_error_t *error) /* OUT */
{
   mongoc_matcher_op_t *op;
   mongoc_matcher_t *matcher;
   bson_iter_t iter;

   BSON_ASSERT (query);

   matcher = bson_malloc0 (sizeof *matcher);
   bson_copy_to (query, &matcher->query);

   if (!bson_iter_init (&iter, &matcher->query)) {
      goto failure;
   }

   if (!(op = _mongoc_matcher_parse_logical (MONGOC_MATCHER_OPCODE_AND, &iter,
                                             true, error))) {
      goto failure;
   }

   matcher->optree = op;

   return matcher;

failure:
   bson_destroy (&matcher->query);
   bson_free (matcher);
   return NULL;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_matcher_match --
 *
 *       Checks to see if @bson matches the query specified when creating
 *       @matcher.
 *
 * Returns:
 *       TRUE if @bson matched the query, otherwise FALSE.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_matcher_match (const mongoc_matcher_t *matcher,  /* IN */
                      const bson_t           *document) /* IN */
{
   BSON_ASSERT (matcher);
   BSON_ASSERT (matcher->optree);
   BSON_ASSERT (document);

   return _mongoc_matcher_op_match (matcher->optree, document);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_matcher_destroy --
 *
 *       Release all resources associated with @matcher.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_matcher_destroy (mongoc_matcher_t *matcher) /* IN */
{
   BSON_ASSERT (matcher);

   _mongoc_matcher_op_destroy (matcher->optree);
   bson_destroy (&matcher->query);
   bson_free (matcher);
}

/*==============================================================*/
/* --- mongoc-matcher-op.c */

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_op_exists_new --
 *
 *       Create a new op for checking {$exists: bool}.
 *
 * Returns:
 *       A newly allocated mongoc_matcher_op_t that should be freed with
 *       _mongoc_matcher_op_destroy().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_matcher_op_t *
_mongoc_matcher_op_exists_new (const char  *path,   /* IN */
                               bool  exists) /* IN */
{
   mongoc_matcher_op_t *op;

   BSON_ASSERT (path);

   op = bson_malloc0 (sizeof *op);
   op->exists.base.opcode = MONGOC_MATCHER_OPCODE_EXISTS;
   op->exists.path = bson_strdup (path);
   op->exists.exists = exists;

   return op;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_op_type_new --
 *
 *       Create a new op for checking {$type: int}.
 *
 * Returns:
 *       A newly allocated mongoc_matcher_op_t that should be freed with
 *       _mongoc_matcher_op_destroy().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_matcher_op_t *
_mongoc_matcher_op_type_new (const char  *path, /* IN */
                             bson_type_t  type) /* IN */
{
   mongoc_matcher_op_t *op;

   BSON_ASSERT (path);
   BSON_ASSERT (type);

   op = bson_malloc0 (sizeof *op);
   op->type.base.opcode = MONGOC_MATCHER_OPCODE_TYPE;
   op->type.path = bson_strdup (path);
   op->type.type = type;

   return op;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_op_logical_new --
 *
 *       Create a new op for checking any of:
 *
 *          {$or: []}
 *          {$nor: []}
 *          {$and: []}
 *
 * Returns:
 *       A newly allocated mongoc_matcher_op_t that should be freed with
 *       _mongoc_matcher_op_destroy().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_matcher_op_t *
_mongoc_matcher_op_logical_new (mongoc_matcher_opcode_t  opcode, /* IN */
                                mongoc_matcher_op_t     *left,   /* IN */
                                mongoc_matcher_op_t     *right)  /* IN */
{
   mongoc_matcher_op_t *op;

   BSON_ASSERT (left);
   BSON_ASSERT ((opcode >= MONGOC_MATCHER_OPCODE_OR) &&
                (opcode <= MONGOC_MATCHER_OPCODE_NOR));

   op = bson_malloc0 (sizeof *op);
   op->logical.base.opcode = opcode;
   op->logical.left = left;
   op->logical.right = right;

   return op;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_op_compare_new --
 *
 *       Create a new op for checking any of:
 *
 *          {"abc": "def"}
 *          {$gt: {...}
 *          {$gte: {...}
 *          {$lt: {...}
 *          {$lte: {...}
 *          {$ne: {...}
 *          {$in: [...]}
 *          {$nin: [...]}
 *
 * Returns:
 *       A newly allocated mongoc_matcher_op_t that should be freed with
 *       _mongoc_matcher_op_destroy().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_matcher_op_t *
_mongoc_matcher_op_compare_new (mongoc_matcher_opcode_t  opcode, /* IN */
                                const char              *path,   /* IN */
                                const bson_iter_t       *iter)   /* IN */
{
   mongoc_matcher_op_t *op;

   BSON_ASSERT ((opcode >= MONGOC_MATCHER_OPCODE_EQ) &&
                (opcode <= MONGOC_MATCHER_OPCODE_NIN));
   BSON_ASSERT (path);
   BSON_ASSERT (iter);

   op = bson_malloc0 (sizeof *op);
   op->compare.base.opcode = opcode;
   op->compare.path = bson_strdup (path);
   memcpy (&op->compare.iter, iter, sizeof *iter);

   return op;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_op_not_new --
 *
 *       Create a new op for checking {$not: {...}}
 *
 * Returns:
 *       A newly allocated mongoc_matcher_op_t that should be freed with
 *       _mongoc_matcher_op_destroy().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_matcher_op_t *
_mongoc_matcher_op_not_new (const char          *path,  /* IN */
                            mongoc_matcher_op_t *child) /* IN */
{
   mongoc_matcher_op_t *op;

   BSON_ASSERT (path);
   BSON_ASSERT (child);

   op = bson_malloc0 (sizeof *op);
   op->not.base.opcode = MONGOC_MATCHER_OPCODE_NOT;
   op->not.path = bson_strdup (path);
   op->not.child = child;

   return op;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_op_destroy --
 *
 *       Free a mongoc_matcher_op_t structure and all children structures.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
void
_mongoc_matcher_op_destroy (mongoc_matcher_op_t *op) /* IN */
{
   BSON_ASSERT (op);

   switch (op->base.opcode) {
   case MONGOC_MATCHER_OPCODE_EQ:
   case MONGOC_MATCHER_OPCODE_GT:
   case MONGOC_MATCHER_OPCODE_GTE:
   case MONGOC_MATCHER_OPCODE_IN:
   case MONGOC_MATCHER_OPCODE_LT:
   case MONGOC_MATCHER_OPCODE_LTE:
   case MONGOC_MATCHER_OPCODE_NE:
   case MONGOC_MATCHER_OPCODE_NIN:
      bson_free (op->compare.path);
      break;
   case MONGOC_MATCHER_OPCODE_OR:
   case MONGOC_MATCHER_OPCODE_AND:
   case MONGOC_MATCHER_OPCODE_NOR:
      if (op->logical.left)
         _mongoc_matcher_op_destroy (op->logical.left);
      if (op->logical.right)
         _mongoc_matcher_op_destroy (op->logical.right);
      break;
   case MONGOC_MATCHER_OPCODE_NOT:
      _mongoc_matcher_op_destroy (op->not.child);
      bson_free (op->not.path);
      break;
   case MONGOC_MATCHER_OPCODE_EXISTS:
      bson_free (op->exists.path);
      break;
   case MONGOC_MATCHER_OPCODE_TYPE:
      bson_free (op->type.path);
      break;
   default:
      break;
   }

   bson_free (op);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_op_exists_match --
 *
 *       Checks to see if @bson matches @exists requirements. The
 *       {$exists: bool} query can be either true or fase so we must
 *       handle false as "not exists".
 *
 * Returns:
 *       true if the field exists and the spec expected it.
 *       true if the field does not exist and the spec expected it to not
 *       exist.
 *       Otherwise, false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_matcher_op_exists_match (mongoc_matcher_op_exists_t *exists, /* IN */
                                 const bson_t               *bson)   /* IN */
{
   bson_iter_t iter;
   bson_iter_t desc;
   bool found;

   BSON_ASSERT (exists);
   BSON_ASSERT (bson);

   found = (bson_iter_init (&iter, bson) &&
            bson_iter_find_descendant (&iter, exists->path, &desc));

   return (found == exists->exists);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_op_type_match --
 *
 *       Checks if @bson matches the {$type: ...} op.
 *
 * Returns:
 *       true if the requested field was found and the type matched
 *       the requested type.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_matcher_op_type_match (mongoc_matcher_op_type_t *type, /* IN */
                               const bson_t             *bson) /* IN */
{
   bson_iter_t iter;
   bson_iter_t desc;

   BSON_ASSERT (type);
   BSON_ASSERT (bson);

   if (bson_iter_init (&iter, bson) &&
       bson_iter_find_descendant (&iter, type->path, &desc)) {
      return (bson_iter_type (&iter) == type->type);
   }

   return false;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_op_not_match --
 *
 *       Checks if the {$not: ...} expression matches by negating the
 *       child expression.
 *
 * Returns:
 *       true if the child expression returned false.
 *       Otherwise false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_matcher_op_not_match (mongoc_matcher_op_not_t *not,  /* IN */
                              const bson_t            *bson) /* IN */
{
   BSON_ASSERT (not);
   BSON_ASSERT (bson);

   return !_mongoc_matcher_op_match (not->child, bson);
}


#define _TYPE_CODE(l, r) ((((int)(l)) << 8) | ((int)(r)))
#define _NATIVE_COMPARE(op, t1, t2) \
   (bson_iter##t2(iter) op bson_iter##t1(&compare->iter))
#define _EQ_COMPARE(t1, t2)  _NATIVE_COMPARE(==, t1, t2)
#define _NE_COMPARE(t1, t2)  _NATIVE_COMPARE(!=, t1, t2)
#define _GT_COMPARE(t1, t2)  _NATIVE_COMPARE(>, t1, t2)
#define _GTE_COMPARE(t1, t2) _NATIVE_COMPARE(>=, t1, t2)
#define _LT_COMPARE(t1, t2)  _NATIVE_COMPARE(<, t1, t2)
#define _LTE_COMPARE(t1, t2) _NATIVE_COMPARE(<=, t1, t2)


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_op_eq_match --
 *
 *       Performs equality match for all types on either left or right
 *       side of the equation.
 *
 *       We try to default to what the compiler would do for comparing
 *       things like integers. Therefore, we just have MACRO'tized
 *       everything so that the compiler sees the native values. (Such
 *       as (double == int64).
 *
 *       The _TYPE_CODE() stuff allows us to shove the type of the left
 *       and the right into a single integer and then do a jump table
 *       with a switch/case for all our supported types.
 *
 *       I imagine a bunch more of these will need to be added, so feel
 *       free to submit patches.
 *
 * Returns:
 *       true if the equality match succeeded.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_matcher_op_eq_match (mongoc_matcher_op_compare_t *compare, /* IN */
                             bson_iter_t                 *iter)    /* IN */
{
   int code;

   BSON_ASSERT (compare);
   BSON_ASSERT (iter);

   code = _TYPE_CODE (bson_iter_type (&compare->iter),
                      bson_iter_type (iter));

   switch (code) {

   /* Double on Left Side */
   case _TYPE_CODE(BSON_TYPE_DOUBLE, BSON_TYPE_DOUBLE):
      return _EQ_COMPARE (_double, _double);
   case _TYPE_CODE(BSON_TYPE_DOUBLE, BSON_TYPE_BOOL):
      return _EQ_COMPARE (_double, _bool);
   case _TYPE_CODE(BSON_TYPE_DOUBLE, BSON_TYPE_INT32):
      return _EQ_COMPARE (_double, _int32);
   case _TYPE_CODE(BSON_TYPE_DOUBLE, BSON_TYPE_INT64):
      return _EQ_COMPARE (_double, _int64);

   /* UTF8 on Left Side */
   case _TYPE_CODE(BSON_TYPE_UTF8, BSON_TYPE_UTF8):
      {
         uint32_t llen;
         uint32_t rlen;
         const char *lstr;
         const char *rstr;

         lstr = bson_iter_utf8 (&compare->iter, &llen);
         rstr = bson_iter_utf8 (iter, &rlen);

         return ((llen == rlen) && (0 == memcmp (lstr, rstr, llen)));
      }

   /* Int32 on Left Side */
   case _TYPE_CODE(BSON_TYPE_INT32, BSON_TYPE_DOUBLE):
      return _EQ_COMPARE (_int32, _double);
   case _TYPE_CODE(BSON_TYPE_INT32, BSON_TYPE_BOOL):
      return _EQ_COMPARE (_int32, _bool);
   case _TYPE_CODE(BSON_TYPE_INT32, BSON_TYPE_INT32):
      return _EQ_COMPARE (_int32, _int32);
   case _TYPE_CODE(BSON_TYPE_INT32, BSON_TYPE_INT64):
      return _EQ_COMPARE (_int32, _int64);

   /* Int64 on Left Side */
   case _TYPE_CODE(BSON_TYPE_INT64, BSON_TYPE_DOUBLE):
      return _EQ_COMPARE (_int64, _double);
   case _TYPE_CODE(BSON_TYPE_INT64, BSON_TYPE_BOOL):
      return _EQ_COMPARE (_int64, _bool);
   case _TYPE_CODE(BSON_TYPE_INT64, BSON_TYPE_INT32):
      return _EQ_COMPARE (_int64, _int32);
   case _TYPE_CODE(BSON_TYPE_INT64, BSON_TYPE_INT64):
      return _EQ_COMPARE (_int64, _int64);

   /* Null on Left Side */
   case _TYPE_CODE(BSON_TYPE_NULL, BSON_TYPE_NULL):
   case _TYPE_CODE(BSON_TYPE_NULL, BSON_TYPE_UNDEFINED):
      return true;

   default:
      return false;
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_op_gt_match --
 *
 *       Perform {$gt: ...} match using @compare.
 *
 *       In general, we try to default to what the compiler would do
 *       for comparison between different types.
 *
 * Returns:
 *       true if the document field was > the spec value.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_matcher_op_gt_match (mongoc_matcher_op_compare_t *compare, /* IN */
                             bson_iter_t                 *iter)    /* IN */
{
   int code;

   BSON_ASSERT (compare);
   BSON_ASSERT (iter);

   code = _TYPE_CODE (bson_iter_type (&compare->iter),
                      bson_iter_type (iter));

   switch (code) {

   /* Double on Left Side */
   case _TYPE_CODE(BSON_TYPE_DOUBLE, BSON_TYPE_DOUBLE):
      return _GT_COMPARE (_double, _double);
   case _TYPE_CODE(BSON_TYPE_DOUBLE, BSON_TYPE_BOOL):
      return _GT_COMPARE (_double, _bool);
   case _TYPE_CODE(BSON_TYPE_DOUBLE, BSON_TYPE_INT32):
      return _GT_COMPARE (_double, _int32);
   case _TYPE_CODE(BSON_TYPE_DOUBLE, BSON_TYPE_INT64):
      return _GT_COMPARE (_double, _int64);

   /* Int32 on Left Side */
   case _TYPE_CODE(BSON_TYPE_INT32, BSON_TYPE_DOUBLE):
      return _GT_COMPARE (_int32, _double);
   case _TYPE_CODE(BSON_TYPE_INT32, BSON_TYPE_BOOL):
      return _GT_COMPARE (_int32, _bool);
   case _TYPE_CODE(BSON_TYPE_INT32, BSON_TYPE_INT32):
      return _GT_COMPARE (_int32, _int32);
   case _TYPE_CODE(BSON_TYPE_INT32, BSON_TYPE_INT64):
      return _GT_COMPARE (_int32, _int64);

   /* Int64 on Left Side */
   case _TYPE_CODE(BSON_TYPE_INT64, BSON_TYPE_DOUBLE):
      return _GT_COMPARE (_int64, _double);
   case _TYPE_CODE(BSON_TYPE_INT64, BSON_TYPE_BOOL):
      return _GT_COMPARE (_int64, _bool);
   case _TYPE_CODE(BSON_TYPE_INT64, BSON_TYPE_INT32):
      return _GT_COMPARE (_int64, _int32);
   case _TYPE_CODE(BSON_TYPE_INT64, BSON_TYPE_INT64):
      return _GT_COMPARE (_int64, _int64);

   default:
      MONGOC_WARNING ("Implement for (Type(%d) > Type(%d))",
                      bson_iter_type (&compare->iter),
                      bson_iter_type (iter));
      break;
   }

   return false;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_op_gte_match --
 *
 *       Perform a match of {"path": {"$gte": value}}.
 *
 * Returns:
 *       true if the the spec matches, otherwise false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_matcher_op_gte_match (mongoc_matcher_op_compare_t *compare, /* IN */
                              bson_iter_t                 *iter)    /* IN */
{
   int code;

   BSON_ASSERT (compare);
   BSON_ASSERT (iter);

   code = _TYPE_CODE (bson_iter_type (&compare->iter),
                      bson_iter_type (iter));

   switch (code) {

   /* Double on Left Side */
   case _TYPE_CODE(BSON_TYPE_DOUBLE, BSON_TYPE_DOUBLE):
      return _GTE_COMPARE (_double, _double);
   case _TYPE_CODE(BSON_TYPE_DOUBLE, BSON_TYPE_BOOL):
      return _GTE_COMPARE (_double, _bool);
   case _TYPE_CODE(BSON_TYPE_DOUBLE, BSON_TYPE_INT32):
      return _GTE_COMPARE (_double, _int32);
   case _TYPE_CODE(BSON_TYPE_DOUBLE, BSON_TYPE_INT64):
      return _GTE_COMPARE (_double, _int64);

   /* Int32 on Left Side */
   case _TYPE_CODE(BSON_TYPE_INT32, BSON_TYPE_DOUBLE):
      return _GTE_COMPARE (_int32, _double);
   case _TYPE_CODE(BSON_TYPE_INT32, BSON_TYPE_BOOL):
      return _GTE_COMPARE (_int32, _bool);
   case _TYPE_CODE(BSON_TYPE_INT32, BSON_TYPE_INT32):
      return _GTE_COMPARE (_int32, _int32);
   case _TYPE_CODE(BSON_TYPE_INT32, BSON_TYPE_INT64):
      return _GTE_COMPARE (_int32, _int64);

   /* Int64 on Left Side */
   case _TYPE_CODE(BSON_TYPE_INT64, BSON_TYPE_DOUBLE):
      return _GTE_COMPARE (_int64, _double);
   case _TYPE_CODE(BSON_TYPE_INT64, BSON_TYPE_BOOL):
      return _GTE_COMPARE (_int64, _bool);
   case _TYPE_CODE(BSON_TYPE_INT64, BSON_TYPE_INT32):
      return _GTE_COMPARE (_int64, _int32);
   case _TYPE_CODE(BSON_TYPE_INT64, BSON_TYPE_INT64):
      return _GTE_COMPARE (_int64, _int64);

   default:
      MONGOC_WARNING ("Implement for (Type(%d) >= Type(%d))",
                      bson_iter_type (&compare->iter),
                      bson_iter_type (iter));
      break;
   }

   return false;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_op_in_match --
 *
 *       Checks the spec {"path": {"$in": [value1, value2, ...]}}.
 *
 * Returns:
 *       true if the spec matched, otherwise false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_matcher_op_in_match (mongoc_matcher_op_compare_t *compare, /* IN */
                             bson_iter_t                 *iter)    /* IN */
{
   mongoc_matcher_op_compare_t op;

   op.base.opcode = MONGOC_MATCHER_OPCODE_EQ;
   op.path = compare->path;

   if (!BSON_ITER_HOLDS_ARRAY (&compare->iter) ||
       !bson_iter_recurse (&compare->iter, &op.iter)) {
      return false;
   }

   while (bson_iter_next (&op.iter)) {
      if (_mongoc_matcher_op_eq_match (&op, iter)) {
         return true;
      }
   }

   return false;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_op_lt_match --
 *
 *       Perform a {"path": "$lt": {value}} match.
 *
 * Returns:
 *       true if the spec matched, otherwise false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_matcher_op_lt_match (mongoc_matcher_op_compare_t *compare, /* IN */
                             bson_iter_t                 *iter)    /* IN */
{
   int code;

   BSON_ASSERT (compare);
   BSON_ASSERT (iter);

   code = _TYPE_CODE (bson_iter_type (&compare->iter),
                      bson_iter_type (iter));

   switch (code) {

   /* Double on Left Side */
   case _TYPE_CODE(BSON_TYPE_DOUBLE, BSON_TYPE_DOUBLE):
      return _LT_COMPARE (_double, _double);
   case _TYPE_CODE(BSON_TYPE_DOUBLE, BSON_TYPE_BOOL):
      return _LT_COMPARE (_double, _bool);
   case _TYPE_CODE(BSON_TYPE_DOUBLE, BSON_TYPE_INT32):
      return _LT_COMPARE (_double, _int32);
   case _TYPE_CODE(BSON_TYPE_DOUBLE, BSON_TYPE_INT64):
      return _LT_COMPARE (_double, _int64);

   /* Int32 on Left Side */
   case _TYPE_CODE(BSON_TYPE_INT32, BSON_TYPE_DOUBLE):
      return _LT_COMPARE (_int32, _double);
   case _TYPE_CODE(BSON_TYPE_INT32, BSON_TYPE_BOOL):
      return _LT_COMPARE (_int32, _bool);
   case _TYPE_CODE(BSON_TYPE_INT32, BSON_TYPE_INT32):
      return _LT_COMPARE (_int32, _int32);
   case _TYPE_CODE(BSON_TYPE_INT32, BSON_TYPE_INT64):
      return _LT_COMPARE (_int32, _int64);

   /* Int64 on Left Side */
   case _TYPE_CODE(BSON_TYPE_INT64, BSON_TYPE_DOUBLE):
      return _LT_COMPARE (_int64, _double);
   case _TYPE_CODE(BSON_TYPE_INT64, BSON_TYPE_BOOL):
      return _LT_COMPARE (_int64, _bool);
   case _TYPE_CODE(BSON_TYPE_INT64, BSON_TYPE_INT32):
      return _LT_COMPARE (_int64, _int32);
   case _TYPE_CODE(BSON_TYPE_INT64, BSON_TYPE_INT64):
      return _LT_COMPARE (_int64, _int64);

   default:
      MONGOC_WARNING ("Implement for (Type(%d) < Type(%d))",
                      bson_iter_type (&compare->iter),
                      bson_iter_type (iter));
      break;
   }

   return false;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_op_lte_match --
 *
 *       Perform a {"$path": {"$lte": value}} match.
 *
 * Returns:
 *       true if the spec matched, otherwise false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_matcher_op_lte_match (mongoc_matcher_op_compare_t *compare, /* IN */
                              bson_iter_t                 *iter)    /* IN */
{
   int code;

   BSON_ASSERT (compare);
   BSON_ASSERT (iter);

   code = _TYPE_CODE (bson_iter_type (&compare->iter),
                      bson_iter_type (iter));

   switch (code) {

   /* Double on Left Side */
   case _TYPE_CODE(BSON_TYPE_DOUBLE, BSON_TYPE_DOUBLE):
      return _LTE_COMPARE (_double, _double);
   case _TYPE_CODE(BSON_TYPE_DOUBLE, BSON_TYPE_BOOL):
      return _LTE_COMPARE (_double, _bool);
   case _TYPE_CODE(BSON_TYPE_DOUBLE, BSON_TYPE_INT32):
      return _LTE_COMPARE (_double, _int32);
   case _TYPE_CODE(BSON_TYPE_DOUBLE, BSON_TYPE_INT64):
      return _LTE_COMPARE (_double, _int64);

   /* Int32 on Left Side */
   case _TYPE_CODE(BSON_TYPE_INT32, BSON_TYPE_DOUBLE):
      return _LTE_COMPARE (_int32, _double);
   case _TYPE_CODE(BSON_TYPE_INT32, BSON_TYPE_BOOL):
      return _LTE_COMPARE (_int32, _bool);
   case _TYPE_CODE(BSON_TYPE_INT32, BSON_TYPE_INT32):
      return _LTE_COMPARE (_int32, _int32);
   case _TYPE_CODE(BSON_TYPE_INT32, BSON_TYPE_INT64):
      return _LTE_COMPARE (_int32, _int64);

   /* Int64 on Left Side */
   case _TYPE_CODE(BSON_TYPE_INT64, BSON_TYPE_DOUBLE):
      return _LTE_COMPARE (_int64, _double);
   case _TYPE_CODE(BSON_TYPE_INT64, BSON_TYPE_BOOL):
      return _LTE_COMPARE (_int64, _bool);
   case _TYPE_CODE(BSON_TYPE_INT64, BSON_TYPE_INT32):
      return _LTE_COMPARE (_int64, _int32);
   case _TYPE_CODE(BSON_TYPE_INT64, BSON_TYPE_INT64):
      return _LTE_COMPARE (_int64, _int64);

   default:
      MONGOC_WARNING ("Implement for (Type(%d) <= Type(%d))",
                      bson_iter_type (&compare->iter),
                      bson_iter_type (iter));
      break;
   }

   return false;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_op_ne_match --
 *
 *       Perform a {"path": {"$ne": value}} match.
 *
 * Returns:
 *       true if the field "path" was not found or the value is not-equal
 *       to value.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_matcher_op_ne_match (mongoc_matcher_op_compare_t *compare, /* IN */
                             bson_iter_t                 *iter)    /* IN */
{
   return !_mongoc_matcher_op_eq_match (compare, iter);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_op_nin_match --
 *
 *       Perform a {"path": {"$nin": value}} match.
 *
 * Returns:
 *       true if value was not found in the array at "path".
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_matcher_op_nin_match (mongoc_matcher_op_compare_t *compare, /* IN */
                              bson_iter_t                 *iter)    /* IN */
{
   return !_mongoc_matcher_op_in_match (compare, iter);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_op_compare_match --
 *
 *       Dispatch function for mongoc_matcher_op_compare_t operations
 *       to perform a match.
 *
 * Returns:
 *       Opcode dependent.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_matcher_op_compare_match (mongoc_matcher_op_compare_t *compare, /* IN */
                                  const bson_t                *bson)    /* IN */
{
   bson_iter_t tmp;
   bson_iter_t iter;

   BSON_ASSERT (compare);
   BSON_ASSERT (bson);

   if (strchr (compare->path, '.')) {
      if (!bson_iter_init (&tmp, bson) ||
          !bson_iter_find_descendant (&tmp, compare->path, &iter)) {
         return false;
      }
   } else if (!bson_iter_init_find (&iter, bson, compare->path)) {
      return false;
   }

   switch ((int)compare->base.opcode) {
   case MONGOC_MATCHER_OPCODE_EQ:
      return _mongoc_matcher_op_eq_match (compare, &iter);
   case MONGOC_MATCHER_OPCODE_GT:
      return _mongoc_matcher_op_gt_match (compare, &iter);
   case MONGOC_MATCHER_OPCODE_GTE:
      return _mongoc_matcher_op_gte_match (compare, &iter);
   case MONGOC_MATCHER_OPCODE_IN:
      return _mongoc_matcher_op_in_match (compare, &iter);
   case MONGOC_MATCHER_OPCODE_LT:
      return _mongoc_matcher_op_lt_match (compare, &iter);
   case MONGOC_MATCHER_OPCODE_LTE:
      return _mongoc_matcher_op_lte_match (compare, &iter);
   case MONGOC_MATCHER_OPCODE_NE:
      return _mongoc_matcher_op_ne_match (compare, &iter);
   case MONGOC_MATCHER_OPCODE_NIN:
      return _mongoc_matcher_op_nin_match (compare, &iter);
   default:
      BSON_ASSERT (false);
      break;
   }

   return false;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_op_logical_match --
 *
 *       Dispatch function for mongoc_matcher_op_logical_t operations
 *       to perform a match.
 *
 * Returns:
 *       Opcode specific.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_matcher_op_logical_match (mongoc_matcher_op_logical_t *logical, /* IN */
                                  const bson_t                *bson)    /* IN */
{
   BSON_ASSERT (logical);
   BSON_ASSERT (bson);

   switch ((int)logical->base.opcode) {
   case MONGOC_MATCHER_OPCODE_OR:
      return (_mongoc_matcher_op_match (logical->left, bson) ||
              _mongoc_matcher_op_match (logical->right, bson));
   case MONGOC_MATCHER_OPCODE_AND:
      return (_mongoc_matcher_op_match (logical->left, bson) &&
              _mongoc_matcher_op_match (logical->right, bson));
   case MONGOC_MATCHER_OPCODE_NOR:
      return !(_mongoc_matcher_op_match (logical->left, bson) ||
               _mongoc_matcher_op_match (logical->right, bson));
   default:
      BSON_ASSERT (false);
      break;
   }

   return false;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_op_match --
 *
 *       Dispatch function for all operation types to perform a match.
 *
 * Returns:
 *       Opcode specific.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
_mongoc_matcher_op_match (mongoc_matcher_op_t *op,   /* IN */
                          const bson_t        *bson) /* IN */
{
   BSON_ASSERT (op);
   BSON_ASSERT (bson);

   switch (op->base.opcode) {
   case MONGOC_MATCHER_OPCODE_EQ:
   case MONGOC_MATCHER_OPCODE_GT:
   case MONGOC_MATCHER_OPCODE_GTE:
   case MONGOC_MATCHER_OPCODE_IN:
   case MONGOC_MATCHER_OPCODE_LT:
   case MONGOC_MATCHER_OPCODE_LTE:
   case MONGOC_MATCHER_OPCODE_NE:
   case MONGOC_MATCHER_OPCODE_NIN:
      return _mongoc_matcher_op_compare_match (&op->compare, bson);
   case MONGOC_MATCHER_OPCODE_OR:
   case MONGOC_MATCHER_OPCODE_AND:
   case MONGOC_MATCHER_OPCODE_NOR:
      return _mongoc_matcher_op_logical_match (&op->logical, bson);
   case MONGOC_MATCHER_OPCODE_NOT:
      return _mongoc_matcher_op_not_match (&op->not, bson);
   case MONGOC_MATCHER_OPCODE_EXISTS:
      return _mongoc_matcher_op_exists_match (&op->exists, bson);
   case MONGOC_MATCHER_OPCODE_TYPE:
      return _mongoc_matcher_op_type_match (&op->type, bson);
   default:
      break;
   }

   return false;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_op_to_bson --
 *
 *       Convert the optree specified by @op to a bson document similar
 *       to what the query would have been. This is not perfectly the
 *       same, and so should not be used as such.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @bson is appended to, and therefore must be initialized before
 *       calling this function.
 *
 *--------------------------------------------------------------------------
 */

void
_mongoc_matcher_op_to_bson (mongoc_matcher_op_t *op,   /* IN */
                            bson_t              *bson) /* IN */
{
   const char *str;
   bson_t child;
   bson_t child2;

   BSON_ASSERT (op);
   BSON_ASSERT (bson);

   switch (op->base.opcode) {
   case MONGOC_MATCHER_OPCODE_EQ:
      bson_append_iter (bson, op->compare.path, -1, &op->compare.iter);
      break;
   case MONGOC_MATCHER_OPCODE_GT:
   case MONGOC_MATCHER_OPCODE_GTE:
   case MONGOC_MATCHER_OPCODE_IN:
   case MONGOC_MATCHER_OPCODE_LT:
   case MONGOC_MATCHER_OPCODE_LTE:
   case MONGOC_MATCHER_OPCODE_NE:
   case MONGOC_MATCHER_OPCODE_NIN:
      switch ((int)op->base.opcode) {
      case MONGOC_MATCHER_OPCODE_GT:
         str = "$gt";
         break;
      case MONGOC_MATCHER_OPCODE_GTE:
         str = "$gte";
         break;
      case MONGOC_MATCHER_OPCODE_IN:
         str = "$in";
         break;
      case MONGOC_MATCHER_OPCODE_LT:
         str = "$lt";
         break;
      case MONGOC_MATCHER_OPCODE_LTE:
         str = "$lte";
         break;
      case MONGOC_MATCHER_OPCODE_NE:
         str = "$ne";
         break;
      case MONGOC_MATCHER_OPCODE_NIN:
         str = "$nin";
         break;
      default:
         str = "???";
         break;
      }
      bson_append_document_begin (bson, op->compare.path, -1, &child);
      bson_append_iter (&child, str, -1, &op->compare.iter);
      bson_append_document_end (bson, &child);
      break;
   case MONGOC_MATCHER_OPCODE_OR:
   case MONGOC_MATCHER_OPCODE_AND:
   case MONGOC_MATCHER_OPCODE_NOR:
      if (op->base.opcode == MONGOC_MATCHER_OPCODE_OR) {
         str = "$or";
      } else if (op->base.opcode == MONGOC_MATCHER_OPCODE_AND) {
         str = "$and";
      } else if (op->base.opcode == MONGOC_MATCHER_OPCODE_NOR) {
         str = "$nor";
      } else {
         BSON_ASSERT (false);
         str = NULL;
      }
      bson_append_array_begin (bson, str, -1, &child);
      bson_append_document_begin (&child, "0", 1, &child2);
      _mongoc_matcher_op_to_bson (op->logical.left, &child2);
      bson_append_document_end (&child, &child2);
      if (op->logical.right) {
         bson_append_document_begin (&child, "1", 1, &child2);
         _mongoc_matcher_op_to_bson (op->logical.right, &child2);
         bson_append_document_end (&child, &child2);
      }
      bson_append_array_end (bson, &child);
      break;
   case MONGOC_MATCHER_OPCODE_NOT:
      bson_append_document_begin (bson, op->not.path, -1, &child);
      bson_append_document_begin (&child, "$not", 4, &child2);
      _mongoc_matcher_op_to_bson (op->not.child, &child2);
      bson_append_document_end (&child, &child2);
      bson_append_document_end (bson, &child);
      break;
   case MONGOC_MATCHER_OPCODE_EXISTS:
      BSON_APPEND_BOOL (bson, "$exists", op->exists.exists);
      break;
   case MONGOC_MATCHER_OPCODE_TYPE:
      BSON_APPEND_INT32 (bson, "$type", (int)op->type.type);
      break;
   default:
      BSON_ASSERT (false);
      break;
   }
}

/*==============================================================*/
/* --- mongoc-queue.c */

void
_mongoc_queue_init (mongoc_queue_t *queue)
{
   bson_return_if_fail(queue);

   memset (queue, 0, sizeof *queue);
}


void
_mongoc_queue_push_head (mongoc_queue_t *queue,
                         void           *data)
{
   mongoc_queue_item_t *item;

   bson_return_if_fail(queue);
   bson_return_if_fail(data);

   item = bson_malloc0(sizeof *item);
   item->next = queue->head;
   item->data = data;

   queue->head = item;

   if (!queue->tail) {
      queue->tail = item;
   }
}


void
_mongoc_queue_push_tail (mongoc_queue_t *queue,
                         void           *data)
{
   mongoc_queue_item_t *item;

   bson_return_if_fail(queue);
   bson_return_if_fail(data);

   item = bson_malloc0(sizeof *item);
   item->data = data;

   if (queue->tail) {
      queue->tail->next = item;
   } else {
      queue->head = item;
   }

   queue->tail = item;
}


void *
_mongoc_queue_pop_head (mongoc_queue_t *queue)
{
   mongoc_queue_item_t *item;
   void *data = NULL;

   bson_return_val_if_fail(queue, NULL);

   if ((item = queue->head)) {
      if (!item->next) {
         queue->tail = NULL;
      }
      queue->head = item->next;
      data = item->data;
      bson_free(item);
   }

   return data;
}


uint32_t
_mongoc_queue_get_length (const mongoc_queue_t *queue)
{
   mongoc_queue_item_t *item;
   uint32_t count = 0;

   bson_return_val_if_fail(queue, 0);

   for (item = queue->head; item; item = item->next) {
      count++;
   }

   return count;
}

/*==============================================================*/
/* --- mongoc-read-prefs.c */

mongoc_read_prefs_t *
mongoc_read_prefs_new (mongoc_read_mode_t mode)
{
   mongoc_read_prefs_t *read_prefs;

   read_prefs = bson_malloc0(sizeof *read_prefs);
   read_prefs->mode = mode;
   bson_init(&read_prefs->tags);

   return read_prefs;
}


mongoc_read_mode_t
mongoc_read_prefs_get_mode (const mongoc_read_prefs_t *read_prefs)
{
   bson_return_val_if_fail(read_prefs, 0);
   return read_prefs->mode;
}


void
mongoc_read_prefs_set_mode (mongoc_read_prefs_t *read_prefs,
                            mongoc_read_mode_t   mode)
{
   bson_return_if_fail(read_prefs);
   bson_return_if_fail(mode <= MONGOC_READ_NEAREST);

   read_prefs->mode = mode;
}


const bson_t *
mongoc_read_prefs_get_tags (const mongoc_read_prefs_t *read_prefs)
{
   bson_return_val_if_fail(read_prefs, NULL);
   return &read_prefs->tags;
}


void
mongoc_read_prefs_set_tags (mongoc_read_prefs_t *read_prefs,
                            const bson_t        *tags)
{
   bson_return_if_fail(read_prefs);

   bson_destroy(&read_prefs->tags);

   if (tags) {
      bson_copy_to(tags, &read_prefs->tags);
   } else {
      bson_init(&read_prefs->tags);
   }
}


void
mongoc_read_prefs_add_tag (mongoc_read_prefs_t *read_prefs,
                           const bson_t        *tag)
{
   bson_t empty = BSON_INITIALIZER;
   char str[16];
   int key;

   BSON_ASSERT (read_prefs);

   key = bson_count_keys (&read_prefs->tags);
   bson_snprintf (str, sizeof str, "%d", key);

   if (tag) {
      bson_append_document (&read_prefs->tags, str, -1, tag);
   } else {
      bson_append_document (&read_prefs->tags, str, -1, &empty);
   }
}


bool
mongoc_read_prefs_is_valid (const mongoc_read_prefs_t *read_prefs)
{
   bson_return_val_if_fail(read_prefs, false);

   /*
    * Tags are not supported with PRIMARY mode.
    */
   if (read_prefs->mode == MONGOC_READ_PRIMARY) {
      if (!bson_empty(&read_prefs->tags)) {
         return false;
      }
   }

   return true;
}


static bool
_contains_tag (const bson_t *b,
               const char   *key,
               const char   *value,
               size_t        value_len)
{
   bson_iter_t iter;

   bson_return_val_if_fail(b, false);
   bson_return_val_if_fail(key, false);
   bson_return_val_if_fail(value, false);

   if (bson_iter_init_find(&iter, b, key) &&
       BSON_ITER_HOLDS_UTF8(&iter) &&
       !strncmp(value, bson_iter_utf8(&iter, NULL), value_len)) {
      return true;
   }

   return false;
}


static int
_score_tags (const bson_t *read_tags,
             const bson_t *node_tags)
{
   uint32_t len;
   bson_iter_t iter;
   bson_iter_t sub_iter;
   const char *key;
   const char *str;
   int count;
   bool node_matches_set;

   bson_return_val_if_fail(read_tags, -1);
   bson_return_val_if_fail(node_tags, -1);

   count = bson_count_keys(read_tags);

   /* Execute this block if read tags were provided, else bail and return 0 (all nodes equal) */
   if (!bson_empty(read_tags) && bson_iter_init(&iter, read_tags)) {

      /*
       * Iterate over array of read tag sets provided (each element is a tag set)
       * Tag sets are provided in order of preference so return the count of the
       * first set that matches the node or -1 if no set matched the node.
       */
      while (count && bson_iter_next(&iter)) {
         if (BSON_ITER_HOLDS_DOCUMENT(&iter) && bson_iter_recurse(&iter, &sub_iter)) {
            node_matches_set = true;

            /* Iterate over the key/value pairs (tags) in the current set */
            while (bson_iter_next(&sub_iter) && BSON_ITER_HOLDS_UTF8(&sub_iter)) {
               key = bson_iter_key(&sub_iter);
               str = bson_iter_utf8(&sub_iter, &len);

               /* If any of the tags do not match, this node cannot satisfy this tag set. */
               if (!_contains_tag(node_tags, key, str, len)) {
                   node_matches_set = false;
                   break;
               }
            }

            /* This set matched, return the count as the score */
            if (node_matches_set) {
                return count;
            }

            /* Decrement the score and try to match the next set. */
            count--;
         }
      }
      return -1;
   }

   return 0;
}


static int
_mongoc_read_prefs_score_primary (const mongoc_read_prefs_t   *read_prefs,
                                  const mongoc_cluster_node_t *node)
{
   bson_return_val_if_fail(read_prefs, -1);
   bson_return_val_if_fail(node, -1);
   return node->primary ? INT_MAX : 0;
}


static int
_mongoc_read_prefs_score_primary_preferred (const mongoc_read_prefs_t   *read_prefs,
                                            const mongoc_cluster_node_t *node)
{
   const bson_t *node_tags;
   const bson_t *read_tags;

   bson_return_val_if_fail(read_prefs, -1);
   bson_return_val_if_fail(node, -1);

   if (node->primary) {
      return INT_MAX;
   }

   node_tags = &node->tags;
   read_tags = &read_prefs->tags;

   return bson_empty(read_tags) ? 1 : _score_tags(read_tags, node_tags);
}


static int
_mongoc_read_prefs_score_secondary (const mongoc_read_prefs_t   *read_prefs,
                                    const mongoc_cluster_node_t *node)
{
   const bson_t *node_tags;
   const bson_t *read_tags;

   bson_return_val_if_fail(read_prefs, -1);
   bson_return_val_if_fail(node, -1);

   if (node->primary) {
      return -1;
   }

   node_tags = &node->tags;
   read_tags = &read_prefs->tags;

   return bson_empty(read_tags) ? 1 : _score_tags(read_tags, node_tags);
}


static int
_mongoc_read_prefs_score_secondary_preferred (const mongoc_read_prefs_t   *read_prefs,
                                              const mongoc_cluster_node_t *node)
{
   const bson_t *node_tags;
   const bson_t *read_tags;

   bson_return_val_if_fail(read_prefs, -1);
   bson_return_val_if_fail(node, -1);

   if (node->primary) {
      return 0;
   }

   node_tags = &node->tags;
   read_tags = &read_prefs->tags;

   return bson_empty(read_tags) ? 1 : _score_tags(read_tags, node_tags);
}


static int
_mongoc_read_prefs_score_nearest (const mongoc_read_prefs_t   *read_prefs,
                                  const mongoc_cluster_node_t *node)
{
   const bson_t *read_tags;
   const bson_t *node_tags;

   bson_return_val_if_fail(read_prefs, -1);
   bson_return_val_if_fail(node, -1);

   node_tags = &node->tags;
   read_tags = &read_prefs->tags;

   return bson_empty(read_tags) ? 1 : _score_tags(read_tags, node_tags);
}


int
_mongoc_read_prefs_score (const mongoc_read_prefs_t   *read_prefs,
                          const mongoc_cluster_node_t *node)
{
   bson_return_val_if_fail(read_prefs, -1);
   bson_return_val_if_fail(node, -1);

   switch (read_prefs->mode) {
   case MONGOC_READ_PRIMARY:
      return _mongoc_read_prefs_score_primary(read_prefs, node);
   case MONGOC_READ_PRIMARY_PREFERRED:
      return _mongoc_read_prefs_score_primary_preferred(read_prefs, node);
   case MONGOC_READ_SECONDARY:
      return _mongoc_read_prefs_score_secondary(read_prefs, node);
   case MONGOC_READ_SECONDARY_PREFERRED:
      return _mongoc_read_prefs_score_secondary_preferred(read_prefs, node);
   case MONGOC_READ_NEAREST:
      return _mongoc_read_prefs_score_nearest(read_prefs, node);
   default:
      BSON_ASSERT(false);
      return -1;
   }
}


void
mongoc_read_prefs_destroy (mongoc_read_prefs_t *read_prefs)
{
   if (read_prefs) {
      bson_destroy(&read_prefs->tags);
      bson_free(read_prefs);
   }
}


mongoc_read_prefs_t *
mongoc_read_prefs_copy (const mongoc_read_prefs_t *read_prefs)
{
   mongoc_read_prefs_t *ret = NULL;

   if (read_prefs) {
      ret = mongoc_read_prefs_new(read_prefs->mode);
      bson_copy_to(&read_prefs->tags, &ret->tags);
   }

   return ret;
}

#ifdef MONGOC_ENABLE_SASL
/*==============================================================*/
/* --- mongoc-sasl.c */

#ifndef SASL_CALLBACK_FN
#  define SASL_CALLBACK_FN(_f) ((int (*) (void))(_f))
#endif


void
_mongoc_sasl_set_mechanism (mongoc_sasl_t *sasl,
                            const char    *mechanism)
{
   BSON_ASSERT (sasl);

   free (sasl->mechanism);
   sasl->mechanism = mechanism ? strdup (mechanism) : NULL;
}


static int
_mongoc_sasl_get_pass (mongoc_sasl_t  *sasl,
                       int             param_id,
                       const char    **result,
                       unsigned       *result_len)
{
   BSON_ASSERT (sasl);
   BSON_ASSERT (param_id == SASL_CB_PASS);

   if (result) {
      *result = sasl->pass;
   }

   if (result_len) {
      *result_len = sasl->pass ? (unsigned) strlen (sasl->pass) : 0;
   }

   return (sasl->pass != NULL) ? SASL_OK : SASL_FAIL;
}


void
_mongoc_sasl_set_pass (mongoc_sasl_t *sasl,
                       const char    *pass)
{
   BSON_ASSERT (sasl);

   free (sasl->pass);
   sasl->pass = pass ? strdup (pass) : NULL;
}


static int
_mongoc_sasl_get_user (mongoc_sasl_t  *sasl,
                       int             param_id,
                       const char    **result,
                       unsigned       *result_len)
{
   BSON_ASSERT (sasl);
   BSON_ASSERT ((param_id == SASL_CB_USER) || (param_id == SASL_CB_AUTHNAME));

   if (result) {
      *result = sasl->user;
   }

   if (result_len) {
      *result_len = sasl->user ? (unsigned) strlen (sasl->user) : 0;
   }

   return (sasl->user != NULL) ? SASL_OK : SASL_FAIL;
}


void
_mongoc_sasl_set_user (mongoc_sasl_t *sasl,
                       const char    *user)
{
   BSON_ASSERT (sasl);

   free (sasl->user);
   sasl->user = user ? strdup (user) : NULL;
}


void
_mongoc_sasl_set_service_host (mongoc_sasl_t *sasl,
                               const char    *service_host)
{
   BSON_ASSERT (sasl);

   free (sasl->service_host);
   sasl->service_host = service_host ? strdup (service_host) : NULL;
}


void
_mongoc_sasl_set_service_name (mongoc_sasl_t *sasl,
                               const char    *service_name)
{
   BSON_ASSERT (sasl);

   free (sasl->service_name);
   sasl->service_name = service_name ? strdup (service_name) : NULL;
}


void
_mongoc_sasl_init (mongoc_sasl_t *sasl)
{
   sasl_callback_t callbacks [] = {
      { SASL_CB_AUTHNAME, SASL_CALLBACK_FN (_mongoc_sasl_get_user), sasl },
      { SASL_CB_USER, SASL_CALLBACK_FN (_mongoc_sasl_get_user), sasl },
      { SASL_CB_PASS, SASL_CALLBACK_FN (_mongoc_sasl_get_pass), sasl },
      { SASL_CB_LIST_END }
   };

   BSON_ASSERT (sasl);

   memset (sasl, 0, sizeof *sasl);

   memcpy (&sasl->callbacks, callbacks, sizeof callbacks);

   sasl->done = false;
   sasl->step = 0;
   sasl->conn = NULL;
   sasl->mechanism = NULL;
   sasl->user = NULL;
   sasl->pass = NULL;
   sasl->service_name = NULL;
   sasl->service_host = NULL;
   sasl->interact = NULL;

   sasl_client_init (sasl->callbacks);
}


void
_mongoc_sasl_destroy (mongoc_sasl_t *sasl)
{
   BSON_ASSERT (sasl);

   if (sasl->conn) {
      sasl_dispose (&sasl->conn);
   }

   free (sasl->user);
   free (sasl->pass);
   free (sasl->mechanism);
   free (sasl->service_name);
   free (sasl->service_host);

#if (SASL_VERSION_MAJOR >= 2) && \
    (SASL_VERSION_MINOR >= 1) && \
    (SASL_VERSION_STEP >= 24)
   sasl_client_done ();
#endif
}


static bool
_mongoc_sasl_is_failure (int           status,
                         bson_error_t *error)
{
   bool ret = (status < 0);

   if (ret) {
      switch (status) {
      case SASL_NOMEM:
         bson_set_error (error,
                         MONGOC_ERROR_SASL,
                         status,
                         "SASL Failrue: insufficient memory.");
         break;
      case SASL_NOMECH:
         bson_set_error (error,
                         MONGOC_ERROR_SASL,
                         status,
                         "SASL Failure: failure to negotiate mechanism");
         break;
      case SASL_BADPARAM:
         bson_set_error (error,
                         MONGOC_ERROR_SASL,
                         status,
                         "Bad parameter supplied. Please file a bug "
                         "with mongo-c-driver.");
         break;
      default:
         bson_set_error (error,
                         MONGOC_ERROR_SASL,
                         status,
                         "SASL Failure: (%d): %s",
                         status,
                         sasl_errstring (status, NULL, NULL));
         break;
      }
   }

   return ret;
}


static bool
_mongoc_sasl_start (mongoc_sasl_t      *sasl,
                    uint8_t       *outbuf,
                    uint32_t       outbufmax,
                    uint32_t      *outbuflen,
                    bson_error_t       *error)
{
   const char *service_name = "mongodb";
   const char *service_host = "";
   const char *mechanism = NULL;
   const char *raw = NULL;
   unsigned raw_len = 0;
   int status;

   BSON_ASSERT (sasl);
   BSON_ASSERT (outbuf);
   BSON_ASSERT (outbufmax);
   BSON_ASSERT (outbuflen);

   if (sasl->service_name) {
      service_name = sasl->service_name;
   }

   if (sasl->service_host) {
      service_host = sasl->service_host;
   }

   status = sasl_client_new (service_name, service_host, NULL, NULL,
                             sasl->callbacks, 0, &sasl->conn);
   if (_mongoc_sasl_is_failure (status, error)) {
      return false;
   }

   status = sasl_client_start (sasl->conn, sasl->mechanism,
                               &sasl->interact, &raw, &raw_len,
                               &mechanism);
   if (_mongoc_sasl_is_failure (status, error)) {
      return false;
   }

   if ((0 != strcasecmp (mechanism, "GSSAPI")) &&
       (0 != strcasecmp (mechanism, "PLAIN"))) {
      bson_set_error (error,
                      MONGOC_ERROR_SASL,
                      SASL_NOMECH,
                      "SASL Failure: invalid mechanism \"%s\"",
                      mechanism);
      return false;
   }


   status = sasl_encode64 (raw, raw_len, (char *)outbuf, outbufmax, outbuflen);
   if (_mongoc_sasl_is_failure (status, error)) {
      return false;
   }

   return true;
}


bool
_mongoc_sasl_step (mongoc_sasl_t *sasl,
                   const uint8_t *inbuf,
                   uint32_t       inbuflen,
                   uint8_t       *outbuf,
                   uint32_t       outbufmax,
                   uint32_t      *outbuflen,
                   bson_error_t  *error)
{
   const char *raw = NULL;
   unsigned rawlen = 0;
   int status;

   BSON_ASSERT (sasl);
   BSON_ASSERT (inbuf);
   BSON_ASSERT (outbuf);
   BSON_ASSERT (outbuflen);

   if (!sasl->step && !*outbuflen) {
      bson_set_error (error,
                      MONGOC_ERROR_SASL,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      "SASL Failure: no data received from SASL request. "
                      "Does server have SASL support enabled?");
      return false;
   }

   sasl->step++;

   if (sasl->step == 1) {
      return _mongoc_sasl_start (sasl, outbuf, outbufmax, outbuflen,
                                 error);
   } else if (sasl->step >= 10) {
      bson_set_error (error,
                      MONGOC_ERROR_SASL,
                      SASL_NOTDONE,
                      "SASL Failure: maximum steps detected");
      return false;
   }

   if (!inbuflen) {
      bson_set_error (error,
                      MONGOC_ERROR_SASL,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      "SASL Failure: no payload provided from server.");
      return false;
   }

   status = sasl_decode64 ((char *)inbuf, inbuflen, (char *)outbuf, outbufmax,
                           outbuflen);
   if (_mongoc_sasl_is_failure (status, error)) {
      return false;
   }

   status = sasl_client_step (sasl->conn, (char *)outbuf, *outbuflen,
                              &sasl->interact, &raw, &rawlen);
   if (_mongoc_sasl_is_failure (status, error)) {
      return false;
   }

   status = sasl_encode64 (raw, rawlen, (char *)outbuf, outbufmax, outbuflen);
   if (_mongoc_sasl_is_failure (status, error)) {
      return false;
   }

   return true;
}
#endif

/*==============================================================*/
/* --- mongoc-socket.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "socket"


struct _mongoc_socket_t
{
#ifdef _WIN32
   SOCKET sd;
#else
   int sd;
#endif
   int errno_;
   int domain;
};


#define OPERATION_EXPIRED(expire_at) \
   ((expire_at >= 0) && (expire_at < (bson_get_monotonic_time())))


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_socket_setnonblock --
 *
 *       A helper to set a socket in nonblocking mode.
 *
 * Returns:
 *       true if successful; otherwise false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
#ifdef _WIN32
_mongoc_socket_setnonblock (SOCKET sd)
#else
_mongoc_socket_setnonblock (int sd)
#endif
{
#ifdef _WIN32
   u_long io_mode = 1;
   return (NO_ERROR == ioctlsocket (sd, FIONBIO, &io_mode));
#else
   int flags;

   flags = fcntl (sd, F_GETFL, sd);
   return (-1 != fcntl (sd, F_SETFL, (flags | O_NONBLOCK)));
#endif
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_socket_wait --
 *
 *       A single socket poll helper.
 *
 *       @events: in most cases should be POLLIN or POLLOUT.
 *
 *       @expire_at should be an absolute time at which to expire using
 *       the monotonic clock (bson_get_monotonic_time(), which is in
 *       microseconds). Or zero to not block at all. Or -1 to block
 *       forever.
 *
 * Returns:
 *       true if an event matched. otherwise false.
 *       a timeout will return false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
#ifdef _WIN32
_mongoc_socket_wait (SOCKET   sd,           /* IN */
#else
_mongoc_socket_wait (int      sd,           /* IN */
#endif
                     int      events,       /* IN */
                     int64_t  expire_at)    /* IN */
{
#ifdef _WIN32
   WSAPOLLFD pfd;
#else
   struct pollfd pfd;
#endif
   int ret;
   int timeout;

   ENTRY;

   bson_return_val_if_fail (events, false);

   if (expire_at < 0) {
      timeout = -1;
   } else if (expire_at == 0) {
      timeout = 0;
   } else {
      timeout = (int)((expire_at - bson_get_monotonic_time ()) / 1000L);
      if (timeout < 0) {
         timeout = 0;
      }
   }

   pfd.fd = sd;
#ifdef _WIN32
   pfd.events = events;
#else
   pfd.events = events | POLLERR | POLLHUP;
#endif
   pfd.revents = 0;

#ifdef _WIN32
   ret = WSAPoll (&pfd, 1, timeout);
   if (ret == SOCKET_ERROR) {
      MONGOC_WARNING ("WSAGetLastError(): %d", WSAGetLastError ());
      ret = -1;
   }
#else
   ret = poll (&pfd, 1, timeout);
#endif

   if (ret > 0) {
      RETURN (0 != (pfd.revents & events));
   }

   RETURN (false);
}


static bool
#ifdef _WIN32
_mongoc_socket_setnodelay (SOCKET sd) /* IN */
#else
_mongoc_socket_setnodelay (int sd)    /* IN */
#endif
{
#ifdef _WIN32
   BOOL optval = 1;
#else
   int optval = 1;
#endif
   int ret;

   ENTRY;

   errno = 0;
   ret = setsockopt (sd, IPPROTO_TCP, TCP_NODELAY,
                     (char *)&optval, sizeof optval);

#ifdef _WIN32
   if (ret == SOCKET_ERROR) {
      MONGOC_WARNING ("WSAGetLastError(): %d", (int)WSAGetLastError ());
   }
#endif

   RETURN (ret == 0);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_socket_errno --
 *
 *       Returns the last error on the socket.
 *
 * Returns:
 *       An integer errno, or 0 on no error.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

int
mongoc_socket_errno (mongoc_socket_t *sock) /* IN */
{
   BSON_ASSERT (sock);
   return sock->errno_;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_socket_capture_errno --
 *
 *       Save the errno state for contextual use.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static void
_mongoc_socket_capture_errno (mongoc_socket_t *sock) /* IN */
{
#ifdef _WIN32
   errno = sock->errno_ = WSAGetLastError ();
#else
   sock->errno_ = errno;
#endif
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_socket_errno_is_again --
 *
 *       Check to see if we should attempt to make further progress
 *       based on the error of the last operation.
 *
 * Returns:
 *       true if we should try again. otherwise false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_socket_errno_is_again (mongoc_socket_t *sock) /* IN */
{
   return MONGOC_ERRNO_IS_AGAIN (sock->errno_);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_socket_accept --
 *
 *       Wrapper for BSD socket accept(). Handles portability between
 *       BSD sockets and WinSock2 on Windows Vista and newer.
 *
 * Returns:
 *       NULL upon failure to accept or timeout.
 *       A newly allocated mongoc_socket_t on success.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_socket_t *
mongoc_socket_accept (mongoc_socket_t *sock,      /* IN */
                      int64_t          expire_at) /* IN */
{
   mongoc_socket_t *client;
   struct sockaddr addr;
   socklen_t addrlen = sizeof addr;
   bool try_again = false;
   bool failed = false;
#ifdef _WIN32
   SOCKET sd;
#else
   int sd;
#endif

   ENTRY;

   bson_return_val_if_fail (sock, NULL);

again:
   errno = 0;
   sd = accept (sock->sd, &addr, &addrlen);

   _mongoc_socket_capture_errno (sock);

#ifdef _WIN32
   failed = (sd == INVALID_SOCKET);
#else
   failed = (sd == -1);
#endif
   try_again = (failed && _mongoc_socket_errno_is_again (sock));

   if (failed && try_again) {
      if (_mongoc_socket_wait (sock->sd, POLLIN, expire_at)) {
         GOTO (again);
      }
      RETURN (NULL);
   } else if (failed) {
      RETURN (NULL);
   } else if (!_mongoc_socket_setnonblock (sd)) {
#ifdef _WIN32
      closesocket (sd);
#else
      close (sd);
#endif
      RETURN (NULL);
   }

   client = bson_malloc0 (sizeof *client);
   client->sd = sd;

   if (!_mongoc_socket_setnodelay (client->sd)) {
      MONGOC_WARNING ("Failed to enable TCP_NODELAY.");
   }

   RETURN (client);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongo_socket_bind --
 *
 *       A wrapper around bind().
 *
 * Returns:
 *       0 on success, -1 on failure and errno is set.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

int
mongoc_socket_bind (mongoc_socket_t       *sock,    /* IN */
                    const struct sockaddr *addr,    /* IN */
                    socklen_t              addrlen) /* IN */
{
   int ret;

   ENTRY;

   bson_return_val_if_fail (sock, false);
   bson_return_val_if_fail (addr, false);
   bson_return_val_if_fail (addrlen, false);

   ret = bind (sock->sd, addr, addrlen);

   _mongoc_socket_capture_errno (sock);

   RETURN (ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_socket_close --
 *
 *       Closes the underlying socket.
 *
 *       In general, you probably don't want to handle the result from
 *       this. That could cause race conditions in the case of preemtion
 *       during system call (EINTR).
 *
 * Returns:
 *       0 on success, -1 on failure.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

int
mongoc_socket_close (mongoc_socket_t *sock) /* IN */
{
   int ret = 0;

   ENTRY;

   bson_return_val_if_fail (sock, false);

#ifdef _WIN32
   if (sock->sd != INVALID_SOCKET) {
      ret = closesocket (sock->sd);
   }
#else
   if (sock->sd != -1) {
      shutdown (sock->sd, SHUT_RDWR);
      ret = close (sock->sd);
   }
#endif

   _mongoc_socket_capture_errno (sock);

   if (ret == 0) {
#ifdef _WIN32
      sock->sd = INVALID_SOCKET;
#else
      sock->sd = -1;
#endif
      RETURN (0);
   }

   RETURN (-1);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_socket_connect --
 *
 *       Performs a socket connection but will fail if @expire_at is
 *       reached by the monotonic clock.
 *
 * Returns:
 *       0 if success, otherwise -1 and errno is set.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

int
mongoc_socket_connect (mongoc_socket_t       *sock,      /* IN */
                       const struct sockaddr *addr,      /* IN */
                       socklen_t              addrlen,   /* IN */
                       int64_t                expire_at) /* IN */
{
   bool try_again = false;
   bool failed = false;
   int ret;
   int optval;
   socklen_t optlen = sizeof optval;

   ENTRY;

   bson_return_val_if_fail (sock, false);
   bson_return_val_if_fail (addr, false);
   bson_return_val_if_fail (addrlen, false);

   ret = connect (sock->sd, addr, addrlen);

   _mongoc_socket_capture_errno (sock);

#ifdef _WIN32
   if (ret == SOCKET_ERROR) {
#else
   if (ret == -1) {
#endif
      failed = true;
      try_again = _mongoc_socket_errno_is_again (sock);
   }

   if (failed && try_again) {
      if (_mongoc_socket_wait (sock->sd, POLLOUT, expire_at)) {
         optval = -1;
         ret = getsockopt (sock->sd, SOL_SOCKET, SO_ERROR,
                           (char *)&optval, &optlen);
         if ((ret == 0) && (optval == 0)) {
            RETURN (0);
         }
      }
      RETURN (-1);
   } else if (failed) {
      RETURN (-1);
   } else {
      RETURN (0);
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_socket_destroy --
 *
 *       Cleanup after a mongoc_socket_t structure, possibly closing
 *       underlying sockets.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @sock is freed and should be considered invalid.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_socket_destroy (mongoc_socket_t *sock) /* IN */
{
   if (sock) {
      mongoc_socket_close (sock);
      bson_free (sock);
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_socket_listen --
 *
 *       Listen for incoming requests with a backlog up to @backlog.
 *
 *       If @backlog is zero, a sensible default will be chosen.
 *
 * Returns:
 *       true if successful; otherwise false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

int
mongoc_socket_listen (mongoc_socket_t *sock,    /* IN */
                      unsigned int     backlog) /* IN */
{
   int ret;

   ENTRY;

   bson_return_val_if_fail (sock, false);

   if (backlog == 0) {
      backlog = 10;
   }

   ret = listen (sock->sd, backlog);

   _mongoc_socket_capture_errno (sock);

   RETURN (ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_socket_new --
 *
 *       Create a new socket.
 *
 *       Free the result mongoc_socket_destroy().
 *
 * Returns:
 *       A newly allocated socket.
 *       NULL on failure.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_socket_t *
mongoc_socket_new (int domain,   /* IN */
                   int type,     /* IN */
                   int protocol) /* IN */
{
   mongoc_socket_t *sock;
#ifdef _WIN32
   SOCKET sd;
#else
   int sd;
#endif

   ENTRY;

   sd = socket (domain, type, protocol);

#ifdef _WIN32
   if (sd == INVALID_SOCKET) {
#else
   if (sd == -1) {
#endif
      RETURN (NULL);
   }

   if (!_mongoc_socket_setnonblock (sd)) {
      GOTO (fail);
   }

   if (!_mongoc_socket_setnodelay (sd)) {
      MONGOC_WARNING ("Failed to enable TCP_NODELAY.");
   }

   sock = bson_malloc0 (sizeof *sock);
   sock->sd = sd;
   sock->domain = domain;

   RETURN (sock);

fail:
#ifdef _WIN32
   closesocket (sd);
#else
   close (sd);
#endif

   RETURN (NULL);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_socket_recv --
 *
 *       A portable wrapper around recv() that also respects an absolute
 *       timeout.
 *
 *       @expire_at is 0 for no blocking, -1 for infinite blocking,
 *       or a time using the monotonic clock to expire. Calculate this
 *       using bson_get_monotonic_time() + N_MICROSECONDS.
 *
 * Returns:
 *       The number of bytes received on success.
 *       0 on end of stream.
 *       -1 on failure.
 *
 * Side effects:
 *       @buf will be read into.
 *
 *--------------------------------------------------------------------------
 */

ssize_t
mongoc_socket_recv (mongoc_socket_t *sock,      /* IN */
                    void            *buf,       /* OUT */
                    size_t           buflen,    /* IN */
                    int              flags,     /* IN */
                    int64_t          expire_at) /* IN */
{
   ssize_t ret = 0;
   bool failed = false;
   bool try_again = false;

   ENTRY;

   bson_return_val_if_fail (sock, -1);
   bson_return_val_if_fail (buf, -1);
   bson_return_val_if_fail (buflen, -1);

again:
   sock->errno_ = 0;
#ifdef _WIN32
   ret = recv (sock->sd, (char *)buf, (int)buflen, flags);
   failed = (ret == SOCKET_ERROR);
#else
   ret = recv (sock->sd, buf, buflen, flags);
   failed = (ret == -1);
#endif
   _mongoc_socket_capture_errno (sock);
   try_again = (failed && _mongoc_socket_errno_is_again (sock));

   if (failed && try_again) {
      if (_mongoc_socket_wait (sock->sd, POLLIN, expire_at)) {
         GOTO (again);
      }
   }

   if (failed) {
      RETURN (-1);
   }

   DUMP_BYTES (recvbuf, (uint8_t *)buf, ret);

   mongoc_counter_streams_ingress_add (ret > 0 ? ret : 0);

   RETURN (ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_socket_setsockopt --
 *
 *       A wrapper around setsockopt().
 *
 * Returns:
 *       0 on success, -1 on failure.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

int
mongoc_socket_setsockopt (mongoc_socket_t *sock,    /* IN */
                          int              level,   /* IN */
                          int              optname, /* IN */
                          const void      *optval,  /* IN */
                          socklen_t        optlen)  /* IN */
{
   int ret;

   ENTRY;

   bson_return_val_if_fail (sock, false);

   ret = setsockopt (sock->sd, level, optname, optval, optlen);

   _mongoc_socket_capture_errno (sock);

   RETURN (ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_socket_send --
 *
 *       A simplified wrapper around mongoc_socket_sendv().
 *
 *       @expire_at is 0 for no blocking, -1 for infinite blocking,
 *       or a time using the monotonic clock to expire. Calculate this
 *       using bson_get_monotonic_time() + N_MICROSECONDS.
 *
 * Returns:
 *       -1 on failure. number of bytes written on success.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

ssize_t
mongoc_socket_send (mongoc_socket_t *sock,      /* IN */
                    const void      *buf,       /* IN */
                    size_t           buflen,    /* IN */
                    int64_t          expire_at) /* IN */
{
   mongoc_iovec_t iov;

   bson_return_val_if_fail (sock, -1);
   bson_return_val_if_fail (buf, -1);
   bson_return_val_if_fail (buflen, -1);

   iov.iov_base = (void *)buf;
   iov.iov_len = buflen;

   return mongoc_socket_sendv (sock, &iov, 1, expire_at);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_socket_try_sendv_slow --
 *
 *       A slow variant of _mongoc_socket_try_sendv() that sends each
 *       iovec entry one by one. This can happen if we hit EMSGSIZE on
 *       with sendmsg() on various POSIX systems (such as Solaris), or
 *       on WinXP.
 *
 * Returns:
 *       the number of bytes sent or -1 and errno is set.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static ssize_t
_mongoc_socket_try_sendv_slow (mongoc_socket_t *sock,   /* IN */
                               mongoc_iovec_t  *iov,    /* IN */
                               size_t           iovcnt) /* IN */
{
   ssize_t ret = 0;
   size_t i;
   int wrote;

   ENTRY;

   BSON_ASSERT (sock);
   BSON_ASSERT (iov);
   BSON_ASSERT (iovcnt);

   for (i = 0; i < iovcnt; i++) {
      wrote = send (sock->sd, iov [i].iov_base, iov [i].iov_len, 0);
      _mongoc_socket_capture_errno (sock);
#ifdef _WIN32
      if (wrote == SOCKET_ERROR) {
#else
      if (wrote == -1) {
#endif
         if (!_mongoc_socket_errno_is_again (sock)) {
            RETURN (-1);
         }
         RETURN (ret ? ret : -1);
      }

      ret += wrote;

      if (wrote != (int)iov [i].iov_len) {
         RETURN (ret);
      }
   }

   RETURN (ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_socket_try_sendv --
 *
 *       Helper used by mongoc_socket_sendv() to try to write as many
 *       bytes to the underlying socket until the socket buffer is full.
 *
 *       This is performed in a non-blocking fashion.
 *
 * Returns:
 *       -1 on failure. the number of bytes written on success.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

ssize_t
_mongoc_socket_try_sendv (mongoc_socket_t *sock,   /* IN */
                          mongoc_iovec_t  *iov,    /* IN */
                          size_t           iovcnt) /* IN */
{
#ifdef _WIN32
   DWORD dwNumberofBytesSent = 0;
#else
   struct msghdr msg;
#endif
   ssize_t ret = -1;

   ENTRY;

   BSON_ASSERT (sock);
   BSON_ASSERT (iov);
   BSON_ASSERT (iovcnt);

   DUMP_IOVEC (sendbuf, iov, iovcnt);

#ifdef _WIN32
   ret = WSASend (sock->sd, (LPWSABUF)iov, iovcnt, &dwNumberofBytesSent,
                  0, NULL, NULL);
   ret = ret ? -1 : dwNumberofBytesSent;
#else
   memset (&msg, 0, sizeof msg);
   msg.msg_iov = iov;
   msg.msg_iovlen = (int) iovcnt;
   ret = sendmsg (sock->sd, &msg,
# ifdef MSG_NOSIGNAL
                  MSG_NOSIGNAL);
#else
                  0);
# endif
#endif

   /*
    * Check to see if we have sent an iovec too large for sendmsg to
    * complete. If so, we need to fallback to the slow path of multiple
    * send() commands.
    */
#ifdef _WIN32
   if ((ret == -1) && (errno == WSAEMSGSIZE)) {
#else
   if ((ret == -1) && (errno == EMSGSIZE)) {
#endif
      _mongoc_socket_try_sendv_slow (sock, iov, iovcnt);
   }

   _mongoc_socket_capture_errno (sock);

   RETURN (ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_socket_sendv --
 *
 *       A wrapper around using sendmsg() to send an iovec.
 *       This also deals with the structure differences between
 *       WSABUF and struct iovec.
 *
 *       @expire_at is 0 for no blocking, -1 for infinite blocking,
 *       or a time using the monotonic clock to expire. Calculate this
 *       using bson_get_monotonic_time() + N_MICROSECONDS.
 *
 * Returns:
 *       -1 on failure.
 *       the number of bytes written on success.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

ssize_t
mongoc_socket_sendv (mongoc_socket_t  *sock,      /* IN */
                     mongoc_iovec_t   *iov,       /* IN */
                     size_t            iovcnt,    /* IN */
                     int64_t           expire_at) /* IN */
{
   ssize_t ret = 0;
   ssize_t sent;
   size_t cur = 0;

   ENTRY;

   bson_return_val_if_fail (sock, -1);
   bson_return_val_if_fail (iov, -1);
   bson_return_val_if_fail (iovcnt, -1);

   for (;;) {
      sent = _mongoc_socket_try_sendv (sock, &iov [cur], iovcnt - cur);

      /*
       * If we failed with anything other than EAGAIN or EWOULDBLOCK,
       * we should fail immediately as there is another issue with the
       * underlying socket.
       */
      if (sent == -1) {
         if (!_mongoc_socket_errno_is_again (sock)) {
            RETURN (ret ? ret : -1);
         }
      }

      /*
       * Update internal stream counters.
       */
      if (sent > 0) {
         ret += sent;
         mongoc_counter_streams_egress_add (sent);

         /*
          * Subtract the sent amount from what we still need to send.
          */
         while ((cur < iovcnt) && (sent >= (ssize_t)iov [cur].iov_len)) {
            sent -= iov [cur++].iov_len;
         }

         /*
          * Check if that made us finish all of the iovecs. If so, we are done
          * sending data over the socket.
          */
         if (cur == iovcnt) {
            break;
         }

         /*
          * Increment the current iovec buffer to its proper offset and adjust
          * the number of bytes to write.
          */
         iov [cur].iov_base = ((char *)iov [cur].iov_base) + sent;
         iov [cur].iov_len -= sent;

         BSON_ASSERT (iovcnt - cur);
         BSON_ASSERT (iov [cur].iov_len);
      } else if (OPERATION_EXPIRED (expire_at)) {
#ifdef _WIN32
         errno = WSAETIMEDOUT;
#else
         errno = ETIMEDOUT;
#endif
         RETURN (ret ? ret : -1);
      }

      /*
       * Block on poll() until our desired condition is met.
       */
      if (!_mongoc_socket_wait (sock->sd, POLLOUT, expire_at)) {
         if (ret == 0){
#ifdef _WIN32
            errno = WSAETIMEDOUT;
#else
            errno = ETIMEDOUT;
#endif
         }
         RETURN (ret  ? ret : -1);
      }
   }

   RETURN (ret);
}


int
mongoc_socket_getsockname (mongoc_socket_t *sock,    /* IN */
                           struct sockaddr *addr,    /* OUT */
                           socklen_t       *addrlen) /* INOUT */
{
   int ret;

   ENTRY;

   bson_return_val_if_fail (sock, -1);

   ret = getsockname (sock->sd, addr, addrlen);

   _mongoc_socket_capture_errno (sock);

   RETURN (ret);
}


char *
mongoc_socket_getnameinfo (mongoc_socket_t *sock) /* IN */
{
   struct sockaddr addr;
   socklen_t len = sizeof addr;
   char *ret;
   char host [BSON_HOST_NAME_MAX + 1];

   ENTRY;

   bson_return_val_if_fail (sock, NULL);

   if ((0 == getpeername (sock->sd, &addr, &len)) &&
       (0 == getnameinfo (&addr, len, host, sizeof host, NULL, 0, 0))) {
      ret = bson_strdup (host);
      RETURN (ret);
   }

   RETURN (NULL);
}

/*==============================================================*/
/* --- mongoc-ssl.c */

#ifdef _WIN32
# define strncasecmp _strnicmp
# define strcasecmp  _stricmp
#endif

/* TODO: we could populate these from a config or something further down the
 * road for providing defaults */
#ifndef MONGOC_SSL_DEFAULT_TRUST_FILE
#define MONGOC_SSL_DEFAULT_TRUST_FILE NULL
#endif
#ifndef MONGOC_SSL_DEFAULT_TRUST_DIR
#define MONGOC_SSL_DEFAULT_TRUST_DIR NULL
#endif

static
mongoc_ssl_opt_t gMongocSslOptDefault = {
   NULL,
   NULL,
   MONGOC_SSL_DEFAULT_TRUST_FILE,
   MONGOC_SSL_DEFAULT_TRUST_DIR,
};

static mongoc_mutex_t * gMongocSslThreadLocks;

static void _mongoc_ssl_thread_startup(void);
static void _mongoc_ssl_thread_cleanup(void);

const mongoc_ssl_opt_t *
mongoc_ssl_opt_get_default (void)
{
   return &gMongocSslOptDefault;
}

/**
 * _mongoc_ssl_init:
 *
 * initialization function for SSL
 *
 * This needs to get called early on and is not threadsafe.  Called by
 * mongoc_init.
 */
void
_mongoc_ssl_init (void)
{
   SSL_CTX *ctx;

   SSL_library_init ();
   SSL_load_error_strings ();
   ERR_load_BIO_strings ();
   OpenSSL_add_all_algorithms ();
   _mongoc_ssl_thread_startup ();

   /*
    * Ensure we also load the ciphers now from the primary thread
    * or we can run into some weirdness on 64-bit Solaris 10 on
    * SPARC with openssl 0.9.7.
    */
   ctx = SSL_CTX_new (SSLv23_method ());
   if (!ctx) {
      MONGOC_ERROR ("Failed to initialize OpenSSL.");
   }

   SSL_CTX_free (ctx);
}

void
_mongoc_ssl_cleanup (void)
{
   _mongoc_ssl_thread_cleanup ();
}

static int
_mongoc_ssl_password_cb (char *buf,
                         int   num,
                         int   rwflag,
                         void *user_data)
{
   char *pass = (char *)user_data;
   int pass_len = (int)strlen (pass);

   if (num < pass_len + 1) {
      return 0;
   }

   bson_strncpy (buf, pass, num);
   return pass_len;
}


/** mongoc_ssl_hostcheck
 *
 * rfc 6125 match a given hostname against a given pattern
 *
 * Patterns come from DNS common names or subjectAltNames.
 *
 * This code is meant to implement RFC 6125 6.4.[1-3]
 *
 */
static bool
_mongoc_ssl_hostcheck (const char *pattern,
                       const char *hostname)
{
   const char *pattern_label_end;
   const char *pattern_wildcard;
   const char *hostname_label_end;
   size_t prefixlen;
   size_t suffixlen;

   pattern_wildcard = strchr (pattern, '*');

   if (pattern_wildcard == NULL) {
      return strcasecmp (pattern, hostname) == 0;
   }

   pattern_label_end = strchr (pattern, '.');

   /* Bail out on wildcarding in a couple of situations:
    * o we don't have 2 dots - we're not going to wildcard root tlds
    * o the wildcard isn't in the left most group (separated by dots)
    * o the pattern is embedded in an A-label or U-label
    */
   if (pattern_label_end == NULL ||
       strchr (pattern_label_end + 1, '.') == NULL ||
       pattern_wildcard > pattern_label_end ||
       strncasecmp (pattern, "xn--", 4) == 0) {
      return strcasecmp (pattern, hostname) == 0;
   }

   hostname_label_end = strchr (hostname, '.');

   /* we know we have a dot in the pattern, we need one in the hostname */
   if (hostname_label_end == NULL ||
       strcasecmp (pattern_label_end, hostname_label_end)) {
      return 0;
   }

   /* The wildcard must match at least one character, so the left part of the
    * hostname is at least as large as the left part of the pattern. */
   if ((hostname_label_end - hostname) < (pattern_label_end - pattern)) {
      return 0;
   }

   /* If the left prefix group before the star matches and right of the star
    * matches... we have a wildcard match */
   prefixlen = pattern_wildcard - pattern;
   suffixlen = pattern_label_end - (pattern_wildcard + 1);
   return strncasecmp (pattern, hostname, prefixlen) == 0 &&
          strncasecmp (pattern_wildcard + 1, hostname_label_end - suffixlen,
                       suffixlen) == 0;
}


/** check if a provided cert matches a passed hostname
 */
bool
_mongoc_ssl_check_cert (SSL        *ssl,
                        const char *host,
                        bool weak_cert_validation)
{
   X509 *peer;
   X509_NAME *subject_name;
   X509_NAME_ENTRY *entry;
   ASN1_STRING *entry_data;
   char *check;
   int length;
   int idx;
   int r = 0;
   long verify_status;

   size_t addrlen = 0;
   struct in_addr addr;
   int i;
   int n_sans = -1;
   int target = GEN_DNS;

   STACK_OF (GENERAL_NAME) * sans = NULL;

   BSON_ASSERT (ssl);
   BSON_ASSERT (host);

   if (weak_cert_validation) {
      return true;
   }

   /** if the host looks like an IP address, match that, otherwise we assume we
    * have a DNS name */
   if (inet_pton (AF_INET, host, &addr)) {
      target = GEN_IPADD;
      addrlen = sizeof (struct in_addr);
   }

   peer = SSL_get_peer_certificate (ssl);

   if (!peer) {
      return false;
   }

   verify_status = SSL_get_verify_result (ssl);

   /** TODO: should we return this somehow? */

   if (verify_status == X509_V_OK) {
      /* get's a stack of alt names that we can iterate through */
      sans = X509_get_ext_d2i ((X509 *)peer, NID_subject_alt_name, NULL, NULL);

      if (sans) {
         n_sans = sk_GENERAL_NAME_num (sans);

         /* loop through the stack, or until we find a match */
         for (i = 0; i < n_sans && !r; i++) {
            const GENERAL_NAME *name = sk_GENERAL_NAME_value (sans, i);

            /* skip entries that can't apply, I.e. IP entries if we've got a
             * DNS host */
            if (name->type == target) {
               check = (char *)ASN1_STRING_data (name->d.ia5);
               length = ASN1_STRING_length (name->d.ia5);

               switch (target) {
               case GEN_DNS:

                  /* check that we don't have an embedded null byte */
                  if ((length == (int)bson_strnlen (check, length)) &&
                      _mongoc_ssl_hostcheck (check, host)) {
                     r = 1;
                  }

                  break;
               case GEN_IPADD:

                  if ((length == (int)addrlen) && !memcmp (check, &addr, length)) {
                     r = 1;
                  }

                  break;
               default:
                  assert (0);
                  break;
               }
            }
         }
         GENERAL_NAMES_free (sans);
      } else {
         subject_name = X509_get_subject_name (peer);

         if (subject_name) {
            idx = -1;
            i = -1;

            /* skip to the last common name */
            while ((idx =
                       X509_NAME_get_index_by_NID (subject_name, NID_commonName, i)) >= 0) {
               i = idx;
            }

            if (i >= 0) {
               entry = X509_NAME_get_entry (subject_name, i);
               entry_data = X509_NAME_ENTRY_get_data (entry);

               if (entry_data) {
                  /* TODO: I've heard tell that old versions of SSL crap out
                   * when calling ASN1_STRING_to_UTF8 on already utf8 data.
                   * Check up on that */
                  length = ASN1_STRING_to_UTF8 ((unsigned char **)&check,
                                                entry_data);

                  if (length >= 0) {
                     /* check for embedded nulls */
                     if ((length == (int)bson_strnlen (check, length)) &&
                         _mongoc_ssl_hostcheck (check, host)) {
                        r = 1;
                     }

                     OPENSSL_free (check);
                  }
               }
            }
         }
      }
   }

   X509_free (peer);
   return r;
}


static bool
_mongoc_ssl_setup_ca (SSL_CTX    *ctx,
                      const char *cert,
                      const char *cert_dir)
{
   BSON_ASSERT(ctx);
   BSON_ASSERT(cert || cert_dir);

   if (!SSL_CTX_load_verify_locations (ctx, cert, cert_dir)) {
      return 0;
   }

   return 1;
}


static bool
_mongoc_ssl_setup_crl (SSL_CTX    *ctx,
                       const char *crlfile)
{
   X509_STORE *store;
   X509_LOOKUP *lookup;
   int status;

   store = SSL_CTX_get_cert_store (ctx);
   X509_STORE_set_flags (store, X509_V_FLAG_CRL_CHECK);

   lookup = X509_STORE_add_lookup (store, X509_LOOKUP_file ());

   status = X509_load_crl_file (lookup, crlfile, X509_FILETYPE_PEM);

   return status != 0;
}


static bool
_mongoc_ssl_setup_pem_file (SSL_CTX    *ctx,
                            const char *pem_file,
                            const char *password)
{
   if (!SSL_CTX_use_certificate_chain_file (ctx, pem_file)) {
      return 0;
   }

   if (password) {
      SSL_CTX_set_default_passwd_cb_userdata (ctx, (void *)password);
      SSL_CTX_set_default_passwd_cb (ctx, _mongoc_ssl_password_cb);
   }

   if (!(SSL_CTX_use_PrivateKey_file (ctx, pem_file, SSL_FILETYPE_PEM))) {
      return 0;
   }

   if (!(SSL_CTX_check_private_key (ctx))) {
      return 0;
   }

   return 1;
}


/**
 * _mongoc_ssl_ctx_new:
 *
 * Create a new ssl context declaratively
 *
 * The opt.pem_pwd parameter, if passed, must exist for the life of this
 * context object (for storing and loading the associated pem file)
 */
SSL_CTX *
_mongoc_ssl_ctx_new (mongoc_ssl_opt_t *opt)
{
   SSL_CTX *ctx = NULL;

   /*
    * Ensure we are initialized. This is safe to call multiple times.
    */
   mongoc_init ();

   ctx = SSL_CTX_new (SSLv23_method ());

   BSON_ASSERT (ctx);

   /* SSL_OP_ALL - Activate all bug workaround options, to support buggy client SSL's.
    * SSL_OP_NO_SSLv2 - Disable SSL v2 support */
   SSL_CTX_set_options (ctx, (SSL_OP_ALL | SSL_OP_NO_SSLv2));

   /* HIGH - Enable strong ciphers
    * !EXPORT - Disable export ciphers (40/56 bit)
    * !aNULL - Disable anonymous auth ciphers
    * @STRENGTH - Sort ciphers based on strength */
   SSL_CTX_set_cipher_list (ctx, "HIGH:!EXPORT:!aNULL@STRENGTH");

   /* If renegotiation is needed, don't return from recv() or send() until it's successful.
    * Note: this is for blocking sockets only. */
   SSL_CTX_set_mode (ctx, SSL_MODE_AUTO_RETRY);

   /* TODO: does this cargo cult actually matter?
    * Disable session caching (see SERVER-10261) */
   SSL_CTX_set_session_cache_mode (ctx, SSL_SESS_CACHE_OFF);

   /* Load in verification certs, private keys and revocation lists */
   if ((!opt->pem_file ||
        _mongoc_ssl_setup_pem_file (ctx, opt->pem_file, opt->pem_pwd))
       && (!(opt->ca_file ||
             opt->ca_dir) ||
           _mongoc_ssl_setup_ca (ctx, opt->ca_file, opt->ca_dir))
       && (!opt->crl_file || _mongoc_ssl_setup_crl (ctx, opt->crl_file))
       ) {
      return ctx;
   } else {
      SSL_CTX_free (ctx);
      return NULL;
   }
}


char *
_mongoc_ssl_extract_subject (const char *filename)
{
   X509_NAME *subject = NULL;
   X509 *cert = NULL;
   BIO *certbio = NULL;
   BIO *strbio = NULL;
   char *str = NULL;
   int ret;

   if (!filename) {
      return NULL;
   }

   certbio = BIO_new (BIO_s_file ());
   strbio = BIO_new (BIO_s_mem ());;

   BSON_ASSERT (certbio);
   BSON_ASSERT (strbio);

   BIO_read_filename (certbio, filename);

   if ((cert = PEM_read_bio_X509 (certbio, NULL, 0, NULL))) {
      if ((subject = X509_get_subject_name (cert))) {
         ret = X509_NAME_print_ex (strbio, subject, 0, XN_FLAG_RFC2253);

         if ((ret > 0) && (ret < INT_MAX)) {
            str = bson_malloc (ret + 2);
            BIO_gets (strbio, str, ret + 1);
            str [ret] = '\0';
         }
      }
   }

   if (cert) {
      X509_free (cert);
   }

   if (certbio) {
      BIO_free (certbio);
   }

   if (strbio) {
      BIO_free (strbio);
   }

   return str;
}

#ifdef _WIN32

static unsigned long
_mongoc_ssl_thread_id_callback (void)
{
   unsigned long ret;

   ret = (unsigned long)GetCurrentThreadId ();
   return ret;
}

#else

static unsigned long
_mongoc_ssl_thread_id_callback (void)
{
   unsigned long ret;

   ret = (unsigned long)pthread_self ();
   return ret;
}

#endif

static void
_mongoc_ssl_thread_locking_callback (int         mode,
                                     int         type,
                                     const char *file,
                                     int         line)
{
   if (mode & CRYPTO_LOCK) {
      mongoc_mutex_lock (&gMongocSslThreadLocks[type]);
   } else {
      mongoc_mutex_unlock (&gMongocSslThreadLocks[type]);
   }
}

static void
_mongoc_ssl_thread_startup (void)
{
   int i;

   gMongocSslThreadLocks = OPENSSL_malloc (CRYPTO_num_locks () * sizeof (mongoc_mutex_t));

   for (i = 0; i < CRYPTO_num_locks (); i++) {
      mongoc_mutex_init(&gMongocSslThreadLocks[i]);
   }

   CRYPTO_set_locking_callback (_mongoc_ssl_thread_locking_callback);
   CRYPTO_set_id_callback (_mongoc_ssl_thread_id_callback);
}

static void
_mongoc_ssl_thread_cleanup (void)
{
   int i;

   CRYPTO_set_locking_callback (NULL);

   for (i = 0; i < CRYPTO_num_locks (); i++) {
      mongoc_mutex_destroy (&gMongocSslThreadLocks[i]);
   }
   OPENSSL_free (gMongocSslThreadLocks);
}

/*==============================================================*/
/* --- mongoc-stream-buffered.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "stream"


typedef struct
{
   mongoc_stream_t  stream;
   mongoc_stream_t *base_stream;
   mongoc_buffer_t  buffer;
} mongoc_stream_buffered_t;


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_buffered_destroy --
 *
 *       Clean up after a mongoc_stream_buffered_t. Free all allocated
 *       resources and release the base stream.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Everything.
 *
 *--------------------------------------------------------------------------
 */

static void
mongoc_stream_buffered_destroy (mongoc_stream_t *stream) /* IN */
{
   mongoc_stream_buffered_t *buffered = (mongoc_stream_buffered_t *)stream;

   bson_return_if_fail(stream);

   mongoc_stream_destroy(buffered->base_stream);
   buffered->base_stream = NULL;

   _mongoc_buffer_destroy (&buffered->buffer);

   bson_free(stream);

   mongoc_counter_streams_active_dec();
   mongoc_counter_streams_disposed_inc();
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_buffered_close --
 *
 *       Close the underlying stream. The buffered content is still
 *       valid.
 *
 * Returns:
 *       The return value of mongoc_stream_close() on the underlying
 *       stream.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static int
mongoc_stream_buffered_close (mongoc_stream_t *stream) /* IN */
{
   mongoc_stream_buffered_t *buffered = (mongoc_stream_buffered_t *)stream;
   bson_return_val_if_fail(stream, -1);
   return mongoc_stream_close(buffered->base_stream);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_buffered_flush --
 *
 *       Flushes the underlying stream.
 *
 * Returns:
 *       The result of flush on the base stream.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static int
mongoc_stream_buffered_flush (mongoc_stream_t *stream) /* IN */
{
   mongoc_stream_buffered_t *buffered = (mongoc_stream_buffered_t *)stream;
   bson_return_val_if_fail(buffered, -1);
   return mongoc_stream_flush(buffered->base_stream);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_buffered_writev --
 *
 *       Write an iovec to the underlying stream. This write is not
 *       buffered, it passes through to the base stream directly.
 *
 *       timeout_msec should be the number of milliseconds to wait before
 *       considering the writev as failed.
 *
 * Returns:
 *       The number of bytes written or -1 on failure.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static ssize_t
mongoc_stream_buffered_writev (mongoc_stream_t *stream,       /* IN */
                               mongoc_iovec_t  *iov,          /* IN */
                               size_t           iovcnt,       /* IN */
                               int32_t          timeout_msec) /* IN */
{
   mongoc_stream_buffered_t *buffered = (mongoc_stream_buffered_t *)stream;
   ssize_t ret;

   ENTRY;

   bson_return_val_if_fail(buffered, -1);

   ret = mongoc_stream_writev(buffered->base_stream, iov, iovcnt,
                              timeout_msec);

   RETURN (ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_buffered_readv --
 *
 *       Read from the underlying stream. The data will be buffered based
 *       on the buffered streams target buffer size.
 *
 *       When reading from the underlying stream, we read at least the
 *       requested number of bytes, but try to also fill the stream to
 *       the size of the underlying buffer.
 *
 * Note:
 *       This isn't actually a huge savings since we never have more than
 *       one reply waiting for us, but perhaps someday that will be
 *       different. It should help for small replies, however that will
 *       reduce our read() syscalls by 50%.
 *
 * Returns:
 *       The number of bytes read or -1 on failure.
 *
 * Side effects:
 *       iov[*]->iov_base buffers are filled.
 *
 *--------------------------------------------------------------------------
 */

static ssize_t
mongoc_stream_buffered_readv (mongoc_stream_t *stream,       /* IN */
                              mongoc_iovec_t  *iov,          /* INOUT */
                              size_t           iovcnt,       /* IN */
                              size_t           min_bytes,    /* IN */
                              int32_t          timeout_msec) /* IN */
{
   mongoc_stream_buffered_t *buffered = (mongoc_stream_buffered_t *)stream;
   bson_error_t error = { 0 };
   size_t total_bytes = 0;
   size_t i;

   ENTRY;

   bson_return_val_if_fail(buffered, -1);

   for (i = 0; i < iovcnt; i++) {
      total_bytes += iov[i].iov_len;
   }

   if (-1 == _mongoc_buffer_fill (&buffered->buffer,
                                  buffered->base_stream,
                                  total_bytes,
                                  timeout_msec,
                                  &error)) {
      MONGOC_WARNING ("Failure to buffer %u bytes: %s",
                      (unsigned)total_bytes,
                      error.message);
      RETURN (-1);
   }

   BSON_ASSERT(buffered->buffer.len >= total_bytes);

   for (i = 0; i < iovcnt; i++) {
      memcpy(iov[i].iov_base,
             buffered->buffer.data + buffered->buffer.off,
             iov[i].iov_len);
      buffered->buffer.off += iov[i].iov_len;
      buffered->buffer.len -= iov[i].iov_len;
   }

   RETURN (total_bytes);
}


static mongoc_stream_t *
_mongoc_stream_buffered_get_base_stream (mongoc_stream_t *stream) /* IN */
{
   return ((mongoc_stream_buffered_t *)stream)->base_stream;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_buffered_new --
 *
 *       Creates a new mongoc_stream_buffered_t.
 *
 *       This stream will read from an underlying stream and try to read
 *       more data than necessary. It can help lower the number of read()
 *       or recv() syscalls performed.
 *
 *       @base_stream is considered owned by the resulting stream after
 *       calling this function.
 *
 * Returns:
 *       A newly allocated mongoc_stream_t.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_stream_t *
mongoc_stream_buffered_new (mongoc_stream_t *base_stream, /* IN */
                            size_t           buffer_size) /* IN */
{
   mongoc_stream_buffered_t *stream;

   bson_return_val_if_fail(base_stream, NULL);

   stream = bson_malloc0(sizeof *stream);
   stream->stream.type = MONGOC_STREAM_BUFFERED;
   stream->stream.destroy = mongoc_stream_buffered_destroy;
   stream->stream.close = mongoc_stream_buffered_close;
   stream->stream.flush = mongoc_stream_buffered_flush;
   stream->stream.writev = mongoc_stream_buffered_writev;
   stream->stream.readv = mongoc_stream_buffered_readv;
   stream->stream.get_base_stream = _mongoc_stream_buffered_get_base_stream;

   stream->base_stream = base_stream;

   _mongoc_buffer_init (&stream->buffer, NULL, buffer_size, NULL, NULL);

   mongoc_counter_streams_active_inc();

   return (mongoc_stream_t *)stream;
}

/*==============================================================*/
/* --- mongoc-stream.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "stream"

#ifndef MONGOC_DEFAULT_TIMEOUT_MSEC
# define MONGOC_DEFAULT_TIMEOUT_MSEC (60L * 60L * 1000L)
#endif


/**
 * mongoc_stream_close:
 * @stream: A mongoc_stream_t.
 *
 * Closes the underlying file-descriptor used by @stream.
 *
 * Returns: 0 on success, -1 on failure.
 */
int
mongoc_stream_close (mongoc_stream_t *stream)
{
   int ret;

   ENTRY;

   bson_return_val_if_fail(stream, -1);

   ret = stream->close(stream);

   RETURN (ret);
}


/**
 * mongoc_stream_destroy:
 * @stream: A mongoc_stream_t.
 *
 * Frees any resources referenced by @stream, including the memory allocation
 * for @stream.
 */
void
mongoc_stream_destroy (mongoc_stream_t *stream)
{
   ENTRY;

   bson_return_if_fail(stream);

   stream->destroy(stream);

   EXIT;
}


/**
 * mongoc_stream_flush:
 * @stream: A mongoc_stream_t.
 *
 * Flushes the data in the underlying stream to the transport.
 *
 * Returns: 0 on success, -1 on failure.
 */
int
mongoc_stream_flush (mongoc_stream_t *stream)
{
   bson_return_val_if_fail(stream, -1);
   return stream->flush(stream);
}


/**
 * mongoc_stream_writev:
 * @stream: A mongoc_stream_t.
 * @iov: An array of iovec to write to the stream.
 * @iovcnt: The number of elements in @iov.
 *
 * Writes an array of iovec buffers to the underlying stream.
 *
 * Returns: the number of bytes written, or -1 upon failure.
 */
ssize_t
mongoc_stream_writev (mongoc_stream_t *stream,
                      mongoc_iovec_t  *iov,
                      size_t           iovcnt,
                      int32_t          timeout_msec)
{
   ssize_t ret;

   ENTRY;

   bson_return_val_if_fail(stream, -1);
   bson_return_val_if_fail(iov, -1);
   bson_return_val_if_fail(iovcnt, -1);

   BSON_ASSERT(stream->writev);

   if (timeout_msec < 0) {
      timeout_msec = MONGOC_DEFAULT_TIMEOUT_MSEC;
   }

   ret = stream->writev(stream, iov, iovcnt, timeout_msec);

   RETURN (ret);
}


/**
 * mongoc_stream_readv:
 * @stream: A mongoc_stream_t.
 * @iov: An array of iovec containing the location and sizes to read.
 * @iovcnt: the number of elements in @iov.
 * @min_bytes: the minumum number of bytes to return, or -1.
 *
 * Reads into the various buffers pointed to by @iov and associated
 * buffer lengths.
 *
 * If @min_bytes is specified, then at least min_bytes will be returned unless
 * eof is encountered.  This may result in ETIMEDOUT
 *
 * Returns: the number of bytes read or -1 on failure.
 */
ssize_t
mongoc_stream_readv (mongoc_stream_t *stream,
                     mongoc_iovec_t  *iov,
                     size_t           iovcnt,
                     size_t           min_bytes,
                     int32_t          timeout_msec)
{
   ssize_t ret;

   ENTRY;

   bson_return_val_if_fail (stream, -1);
   bson_return_val_if_fail (iov, -1);
   bson_return_val_if_fail (iovcnt, -1);

   BSON_ASSERT (stream->readv);

   ret = stream->readv (stream, iov, iovcnt, min_bytes, timeout_msec);

   RETURN (ret);
}


/**
 * mongoc_stream_read:
 * @stream: A mongoc_stream_t.
 * @buf: A buffer to write into.
 * @count: The number of bytes to write into @buf.
 * @min_bytes: The minimum number of bytes to receive
 *
 * Simplified access to mongoc_stream_readv(). Creates a single iovec
 * with the buffer provided.
 *
 * If @min_bytes is specified, then at least min_bytes will be returned unless
 * eof is encountered.  This may result in ETIMEDOUT
 *
 * Returns: -1 on failure, otherwise the number of bytes read.
 */
ssize_t
mongoc_stream_read (mongoc_stream_t *stream,
                    void            *buf,
                    size_t           count,
                    size_t           min_bytes,
                    int32_t          timeout_msec)
{
   mongoc_iovec_t iov;
   ssize_t ret;

   ENTRY;

   bson_return_val_if_fail (stream, -1);
   bson_return_val_if_fail (buf, -1);

   iov.iov_base = buf;
   iov.iov_len = count;

   BSON_ASSERT (stream->readv);

   ret = mongoc_stream_readv (stream, &iov, 1, min_bytes, timeout_msec);

   RETURN (ret);
}


int
mongoc_stream_setsockopt (mongoc_stream_t *stream,
                          int              level,
                          int              optname,
                          void            *optval,
                          socklen_t        optlen)
{
   bson_return_val_if_fail(stream, -1);

   if (stream->setsockopt) {
      return stream->setsockopt(stream, level, optname, optval, optlen);
   }

   return 0;
}


mongoc_stream_t *
mongoc_stream_get_base_stream (mongoc_stream_t *stream) /* IN */
{
   bson_return_val_if_fail (stream, NULL);

   if (stream->get_base_stream) {
      return stream->get_base_stream (stream);
   }

   return NULL;
}

/*==============================================================*/
/* --- mongoc-stream-file.c */

/*
 * TODO: This does not respect timeouts or set O_NONBLOCK.
 *       But that should be fine until it isn't :-)
 */


struct _mongoc_stream_file_t
{
   mongoc_stream_t vtable;
   int             fd;
};


static int
_mongoc_stream_file_close (mongoc_stream_t *stream)
{
   mongoc_stream_file_t *file = (mongoc_stream_file_t *)stream;
   int ret;

   ENTRY;

   bson_return_val_if_fail (file, -1);

   if (file->fd != -1) {
#ifdef _WIN32
      ret = _close (file->fd);
#else
      ret = close (file->fd);
#endif
      file->fd = -1;
      RETURN (ret);
   }

   RETURN (0);
}


static void
_mongoc_stream_file_destroy (mongoc_stream_t *stream)
{
   mongoc_stream_file_t *file = (mongoc_stream_file_t *)stream;

   ENTRY;

   bson_return_if_fail (file);

   if (file->fd) {
      _mongoc_stream_file_close (stream);
   }

   bson_free (file);

   EXIT;
}


static int
_mongoc_stream_file_flush (mongoc_stream_t *stream) /* IN */
{
   mongoc_stream_file_t *file = (mongoc_stream_file_t *)stream;

   BSON_ASSERT (file);

   if (file->fd != -1) {
#ifdef _WIN32
      return _commit (file->fd);
#else
      return fsync (file->fd);
#endif
   }

   return 0;
}


static ssize_t
_mongoc_stream_file_readv (mongoc_stream_t *stream,       /* IN */
                           mongoc_iovec_t  *iov,          /* IN */
                           size_t           iovcnt,       /* IN */
                           size_t           min_bytes,    /* IN */
                           int32_t          timeout_msec) /* IN */
{
   mongoc_stream_file_t *file = (mongoc_stream_file_t *)stream;
   ssize_t ret = 0;

#ifdef _WIN32
   ssize_t nread;
   size_t i;

   ENTRY;

   for (i = 0; i < iovcnt; i++) {
      nread = _read (file->fd, iov [i].iov_base, iov [i].iov_len);
      if (nread < 0) {
         RETURN (ret ? ret : -1);
      } else if (nread == 0) {
         RETURN (ret ? ret : 0);
      } else {
         ret += nread;
         if (nread != iov[i].iov_len) {
            RETURN (ret ? ret : -1);
         }
      }
   }

   RETURN (ret);
#else
   ENTRY;
   ret = readv (file->fd, iov, (int) iovcnt);
   RETURN (ret);
#endif
}


static ssize_t
_mongoc_stream_file_writev (mongoc_stream_t *stream,       /* IN */
                            mongoc_iovec_t  *iov,          /* IN */
                            size_t           iovcnt,       /* IN */
                            int32_t          timeout_msec) /* IN */
{
   mongoc_stream_file_t *file = (mongoc_stream_file_t *)stream;

#ifdef _WIN32
   ssize_t ret = 0;
   ssize_t nwrite;
   size_t i;

   for (i = 0; i < iovcnt; i++) {
      nwrite = _write (file->fd, iov [i].iov_base, iov [i].iov_len);
      if (nwrite != iov [i].iov_len) {
         return ret ? ret : -1;
      }
      ret += nwrite;
   }

   return ret;
#else
   return writev (file->fd, iov, (int) iovcnt);
#endif
}


mongoc_stream_t *
mongoc_stream_file_new (int fd) /* IN */
{
   mongoc_stream_file_t *stream;

   bson_return_val_if_fail (fd != -1, NULL);

   stream = bson_malloc0 (sizeof *stream);
   stream->vtable.type = MONGOC_STREAM_FILE;
   stream->vtable.close = _mongoc_stream_file_close;
   stream->vtable.destroy = _mongoc_stream_file_destroy;
   stream->vtable.flush = _mongoc_stream_file_flush;
   stream->vtable.readv = _mongoc_stream_file_readv;
   stream->vtable.writev = _mongoc_stream_file_writev;
   stream->fd = fd;

   return (mongoc_stream_t *)stream;
}


mongoc_stream_t *
mongoc_stream_file_new_for_path (const char *path,  /* IN */
                                 int         flags, /* IN */
                                 int         mode)  /* IN */
{
   int fd = -1;

   bson_return_val_if_fail (path, NULL);

#ifdef _WIN32
   if (_sopen_s (&fd, path, (flags | _O_BINARY), _SH_DENYNO, 0) != 0) {
      fd = -1;
   }
#else
   fd = open (path, flags, mode);
#endif

   if (fd == -1) {
      return NULL;
   }

   return mongoc_stream_file_new (fd);
}


int
mongoc_stream_file_get_fd (mongoc_stream_file_t *stream)
{
   bson_return_val_if_fail (stream, -1);

   return stream->fd;
}

/*==============================================================*/
/* --- mongoc-stream-gridfs.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "stream-gridfs"


typedef struct
{
   mongoc_stream_t       stream;
   mongoc_gridfs_file_t *file;
} mongoc_stream_gridfs_t;


static void
_mongoc_stream_gridfs_destroy (mongoc_stream_t *stream)
{
   ENTRY;

   BSON_ASSERT (stream);

   mongoc_stream_close (stream);

   bson_free (stream);

   mongoc_counter_streams_active_dec ();
   mongoc_counter_streams_disposed_inc ();

   EXIT;
}


static int
_mongoc_stream_gridfs_close (mongoc_stream_t *stream)
{
   mongoc_stream_gridfs_t *gridfs = (mongoc_stream_gridfs_t *)stream;
   int ret = 0;

   ENTRY;

   BSON_ASSERT (stream);

   ret = mongoc_gridfs_file_save (gridfs->file);

   RETURN (ret);
}

static int
_mongoc_stream_gridfs_flush (mongoc_stream_t *stream)
{
   mongoc_stream_gridfs_t *gridfs = (mongoc_stream_gridfs_t *)stream;
   int ret = 0;

   ENTRY;

   BSON_ASSERT (stream);

   ret = mongoc_gridfs_file_save (gridfs->file);

   RETURN (ret);
}


static ssize_t
_mongoc_stream_gridfs_readv (mongoc_stream_t *stream,
                             mongoc_iovec_t  *iov,
                             size_t           iovcnt,
                             size_t           min_bytes,
                             int32_t          timeout_msec)
{
   mongoc_stream_gridfs_t *file = (mongoc_stream_gridfs_t *)stream;
   ssize_t ret = 0;

   ENTRY;

   BSON_ASSERT (stream);
   BSON_ASSERT (iov);
   BSON_ASSERT (iovcnt);
   BSON_ASSERT (timeout_msec <= INT_MAX);

   ret = mongoc_gridfs_file_readv (file->file, iov, iovcnt, min_bytes,
                                   timeout_msec);

   mongoc_counter_streams_ingress_add (ret);

   RETURN (ret);
}


static ssize_t
_mongoc_stream_gridfs_writev (mongoc_stream_t *stream,
                              mongoc_iovec_t  *iov,
                              size_t           iovcnt,
                              int32_t     timeout_msec)
{
   mongoc_stream_gridfs_t *file = (mongoc_stream_gridfs_t *)stream;
   ssize_t ret = 0;

   ENTRY;

   BSON_ASSERT (stream);
   BSON_ASSERT (iov);
   BSON_ASSERT (iovcnt);

   ret = mongoc_gridfs_file_writev (file->file, iov, iovcnt, timeout_msec);

   if (!ret) {
      RETURN (ret);
   }

   mongoc_counter_streams_egress_add (ret);

   RETURN (ret);
}


mongoc_stream_t *
mongoc_stream_gridfs_new (mongoc_gridfs_file_t *file)
{
   mongoc_stream_gridfs_t *stream;

   ENTRY;

   BSON_ASSERT (file);

   stream = bson_malloc0 (sizeof *stream);
   stream->file = file;
   stream->stream.type = MONGOC_STREAM_GRIDFS;
   stream->stream.destroy = _mongoc_stream_gridfs_destroy;
   stream->stream.close = _mongoc_stream_gridfs_close;
   stream->stream.flush = _mongoc_stream_gridfs_flush;
   stream->stream.writev = _mongoc_stream_gridfs_writev;
   stream->stream.readv = _mongoc_stream_gridfs_readv;

   mongoc_counter_streams_active_inc ();

   RETURN ((mongoc_stream_t *)stream);
}

/*==============================================================*/
/* --- mongoc-stream-socket.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "stream"


struct _mongoc_stream_socket_t
{
   mongoc_stream_t  vtable;
   mongoc_socket_t *sock;
};


static BSON_INLINE int64_t
get_expiration (int32_t timeout_msec)
{
   if (timeout_msec < 0) {
      return -1;
   } else if (timeout_msec == 0) {
      return 0;
   } else {
      return (bson_get_monotonic_time () + ((int64_t)timeout_msec * 1000L));
   }
}


static int
_mongoc_stream_socket_close (mongoc_stream_t *stream)
{
   mongoc_stream_socket_t *ss = (mongoc_stream_socket_t *)stream;
   int ret;

   ENTRY;

   bson_return_val_if_fail (ss, -1);

   if (ss->sock) {
      ret = mongoc_socket_close (ss->sock);
      RETURN (ret);
   }

   RETURN (0);
}


static void
_mongoc_stream_socket_destroy (mongoc_stream_t *stream)
{
   mongoc_stream_socket_t *ss = (mongoc_stream_socket_t *)stream;

   ENTRY;

   bson_return_if_fail (ss);

   if (ss->sock) {
      mongoc_socket_destroy (ss->sock);
      ss->sock = NULL;
   }

   bson_free (ss);

   EXIT;
}


static int
_mongoc_stream_socket_setsockopt (mongoc_stream_t *stream,
                                  int              level,
                                  int              optname,
                                  void            *optval,
                                  socklen_t        optlen)
{
   mongoc_stream_socket_t *ss = (mongoc_stream_socket_t *)stream;
   int ret;

   ENTRY;

   bson_return_val_if_fail (ss, -1);
   bson_return_val_if_fail (ss->sock, -1);

   ret = mongoc_socket_setsockopt (ss->sock, level, optname, optval, optlen);

   RETURN (ret);
}


static int
_mongoc_stream_socket_flush (mongoc_stream_t *stream)
{
   ENTRY;
   RETURN (0);
}


static ssize_t
_mongoc_stream_socket_readv (mongoc_stream_t *stream,
                             mongoc_iovec_t  *iov,
                             size_t           iovcnt,
                             size_t           min_bytes,
                             int32_t          timeout_msec)
{
   mongoc_stream_socket_t *ss = (mongoc_stream_socket_t *)stream;
   int64_t expire_at;
   ssize_t ret = 0;
   ssize_t nread;
   size_t cur = 0;

   ENTRY;

   bson_return_val_if_fail (ss, -1);
   bson_return_val_if_fail (ss->sock, -1);

   expire_at = get_expiration (timeout_msec);

   /*
    * This isn't ideal, we should plumb through to recvmsg(), but we
    * don't actually use this in any way but to a single buffer
    * currently anyway, so should be just fine.
    */

   for (;;) {
      nread = mongoc_socket_recv (ss->sock,
                                  iov [cur].iov_base,
                                  iov [cur].iov_len,
                                  0,
                                  expire_at);

      if (nread <= 0) {
         if (ret >= (ssize_t)min_bytes) {
            RETURN (ret);
         }
         errno = mongoc_socket_errno (ss->sock);
         RETURN (-1);
      }

      ret += nread;

      while ((cur < iovcnt) && (nread >= (ssize_t)iov [cur].iov_len)) {
         nread -= iov [cur++].iov_len;
      }

      if (cur == iovcnt) {
         break;
      }

      if (ret >= (ssize_t)min_bytes) {
         RETURN (ret);
      }

      iov [cur].iov_base = ((char *)iov [cur].iov_base) + nread;
      iov [cur].iov_len -= nread;

      BSON_ASSERT (iovcnt - cur);
      BSON_ASSERT (iov [cur].iov_len);
   }

   RETURN (ret);
}


static ssize_t
_mongoc_stream_socket_writev (mongoc_stream_t *stream,
                              mongoc_iovec_t  *iov,
                              size_t           iovcnt,
                              int32_t          timeout_msec)
{
   mongoc_stream_socket_t *ss = (mongoc_stream_socket_t *)stream;
   int64_t expire_at;
   ssize_t ret;

   ENTRY;

   if (ss->sock) {
      expire_at = get_expiration (timeout_msec);
      ret = mongoc_socket_sendv (ss->sock, iov, iovcnt, expire_at);
      errno = mongoc_socket_errno (ss->sock);
      RETURN (ret);
   }

   RETURN (-1);
}


mongoc_socket_t *
mongoc_stream_socket_get_socket (mongoc_stream_socket_t *stream) /* IN */
{
   bson_return_val_if_fail (stream, NULL);

   return stream->sock;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_socket_new --
 *
 *       Create a new mongoc_stream_t using the mongoc_socket_t for
 *       read and write underneath.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_stream_t *
mongoc_stream_socket_new (mongoc_socket_t *sock) /* IN */
{
   mongoc_stream_socket_t *stream;

   bson_return_val_if_fail (sock, NULL);

   stream = bson_malloc0 (sizeof *stream);
   stream->vtable.type = MONGOC_STREAM_SOCKET;
   stream->vtable.close = _mongoc_stream_socket_close;
   stream->vtable.destroy = _mongoc_stream_socket_destroy;
   stream->vtable.flush = _mongoc_stream_socket_flush;
   stream->vtable.readv = _mongoc_stream_socket_readv;
   stream->vtable.writev = _mongoc_stream_socket_writev;
   stream->vtable.setsockopt = _mongoc_stream_socket_setsockopt;
   stream->sock = sock;

   return (mongoc_stream_t *)stream;
}

/*==============================================================*/
/* --- mongoc-stream-tls.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "stream-tls"


/**
 * mongoc_stream_tls_t:
 *
 * Private storage for handling callbacks from mongoc_stream and BIO_*
 *
 * The one funny wrinkle comes with timeout, which we use statefully to
 * statefully pass timeouts through from the mongoc-stream api.
 *
 * TODO: is there a cleaner way to manage that?
 */
typedef struct
{
   mongoc_stream_t  parent;
   mongoc_stream_t *base_stream;
   BIO             *bio;
   SSL_CTX         *ctx;
   int32_t          timeout_msec;
   bool             weak_cert_validation;
} mongoc_stream_tls_t;


static int
_mongoc_stream_tls_bio_create (BIO *b);

static int
_mongoc_stream_tls_bio_destroy (BIO *b);

static int
_mongoc_stream_tls_bio_read (BIO  *b,
                             char *buf,
                             int   len);

static int
_mongoc_stream_tls_bio_write (BIO        *b,
                              const char *buf,
                              int         len);

static long
_mongoc_stream_tls_bio_ctrl (BIO  *b,
                             int   cmd,
                             long  num,
                             void *ptr);

static int
_mongoc_stream_tls_bio_gets (BIO  *b,
                             char *buf,
                             int   len);

static int
_mongoc_stream_tls_bio_puts (BIO        *b,
                             const char *str);


/* Magic vtable to make our BIO shim */
static BIO_METHOD gMongocStreamTlsRawMethods = {
   BIO_TYPE_FILTER,
   "mongoc-stream-tls-glue",
   _mongoc_stream_tls_bio_write,
   _mongoc_stream_tls_bio_read,
   _mongoc_stream_tls_bio_puts,
   _mongoc_stream_tls_bio_gets,
   _mongoc_stream_tls_bio_ctrl,
   _mongoc_stream_tls_bio_create,
   _mongoc_stream_tls_bio_destroy
};


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_stream_tls_bio_create --
 *
 *       BIO callback to create a new BIO instance.
 *
 * Returns:
 *       1 if successful.
 *
 * Side effects:
 *       @b is initialized.
 *
 *--------------------------------------------------------------------------
 */

static int
_mongoc_stream_tls_bio_create (BIO *b)
{
   BSON_ASSERT (b);

   b->init = 1;
   b->num = 0;
   b->ptr = NULL;
   b->flags = 0;

   return 1;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_stream_tls_bio_destroy --
 *
 *       Release resources associated with BIO.
 *
 * Returns:
 *       1 if successful.
 *
 * Side effects:
 *       @b is destroyed.
 *
 *--------------------------------------------------------------------------
 */

static int
_mongoc_stream_tls_bio_destroy (BIO *b)
{
   mongoc_stream_tls_t *tls;

   BSON_ASSERT (b);

   if (!(tls = b->ptr)) {
      return -1;
   }

   b->ptr = NULL;
   b->init = 0;
   b->flags = 0;

   tls->bio = NULL;

   return 1;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_stream_tls_bio_read --
 *
 *       Read from the underlying stream to BIO.
 *
 * Returns:
 *       -1 on failure; otherwise the number of bytes read.
 *
 * Side effects:
 *       @buf is filled with data read from underlying stream.
 *
 *--------------------------------------------------------------------------
 */

static int
_mongoc_stream_tls_bio_read (BIO  *b,
                             char *buf,
                             int   len)
{
   mongoc_stream_tls_t *tls;
   int ret;

   BSON_ASSERT (b);
   BSON_ASSERT (buf);

   if (!(tls = b->ptr)) {
      return -1;
   }

   errno = 0;
   ret = (int)mongoc_stream_read (tls->base_stream, buf, len, 0,
                                  tls->timeout_msec);
   BIO_clear_retry_flags (b);

   if ((ret < 0) && MONGOC_ERRNO_IS_AGAIN (errno)) {
      BIO_set_retry_read (b);
   }

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_stream_tls_bio_write --
 *
 *       Write to the underlying stream on behalf of BIO.
 *
 * Returns:
 *       -1 on failure; otherwise the number of bytes written.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static int
_mongoc_stream_tls_bio_write (BIO        *b,
                              const char *buf,
                              int         len)
{
   mongoc_stream_tls_t *tls;
   mongoc_iovec_t iov;
   int ret;

   BSON_ASSERT (b);
   BSON_ASSERT (buf);

   if (!(tls = b->ptr)) {
      return -1;
   }

   iov.iov_base = (void *)buf;
   iov.iov_len = len;

   errno = 0;
   ret = (int)mongoc_stream_writev (tls->base_stream, &iov, 1,
                                    tls->timeout_msec);
   BIO_clear_retry_flags (b);

   if ((ret < 0) && MONGOC_ERRNO_IS_AGAIN (errno)) {
      BIO_set_retry_write (b);
   }

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_stream_tls_bio_ctrl --
 *
 *       Handle ctrl callback for BIO.
 *
 * Returns:
 *       ioctl dependent.
 *
 * Side effects:
 *       ioctl dependent.
 *
 *--------------------------------------------------------------------------
 */

static long
_mongoc_stream_tls_bio_ctrl (BIO  *b,
                             int   cmd,
                             long  num,
                             void *ptr)
{
   switch (cmd) {
   case BIO_CTRL_FLUSH:
      return 1;
   default:
      return 0;
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_stream_tls_bio_gets --
 *
 *       BIO callback for gets(). Not supported.
 *
 * Returns:
 *       -1 always.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static int
_mongoc_stream_tls_bio_gets (BIO  *b,
                             char *buf,
                             int   len)
{
   return -1;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_stream_tls_bio_puts --
 *
 *       BIO callback to perform puts(). Just calls the actual write
 *       callback.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static int
_mongoc_stream_tls_bio_puts (BIO        *b,
                             const char *str)
{
   return _mongoc_stream_tls_bio_write (b, str, (int)strlen (str));
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_stream_tls_destroy --
 *
 *       Cleanup after usage of a mongoc_stream_tls_t. Free all allocated
 *       resources and ensure connections are closed.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static void
_mongoc_stream_tls_destroy (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;

   BSON_ASSERT (tls);

   BIO_free_all (tls->bio);
   tls->bio = NULL;

   mongoc_stream_destroy (tls->base_stream);
   tls->base_stream = NULL;

   SSL_CTX_free (tls->ctx);
   tls->ctx = NULL;

   bson_free (stream);

   mongoc_counter_streams_active_dec();
   mongoc_counter_streams_disposed_inc();
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_stream_tls_close --
 *
 *       Close the underlying socket.
 *
 *       Linus dictates that you should not check the result of close()
 *       since there is a race condition with EAGAIN and a new file
 *       descriptor being opened.
 *
 * Returns:
 *       0 on success; otherwise -1.
 *
 * Side effects:
 *       The BIO fd is closed.
 *
 *--------------------------------------------------------------------------
 */

static int
_mongoc_stream_tls_close (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;

   BSON_ASSERT (tls);

   return mongoc_stream_close (tls->base_stream);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_stream_tls_flush --
 *
 *       Flush the underlying stream.
 *
 * Returns:
 *       0 if successful; otherwise -1.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static int
_mongoc_stream_tls_flush (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;

   BSON_ASSERT (tls);

   return BIO_flush (tls->bio);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_stream_tls_writev --
 *
 *       Write the iovec to the stream. This function will try to write
 *       all of the bytes or fail. If the number of bytes is not equal
 *       to the number requested, a failure or EOF has occurred.
 *
 * Returns:
 *       -1 on failure, otherwise the number of bytes written.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static ssize_t
_mongoc_stream_tls_writev (mongoc_stream_t *stream,
                           mongoc_iovec_t  *iov,
                           size_t           iovcnt,
                           int32_t          timeout_msec)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   ssize_t ret = 0;
   size_t i;
   size_t iov_pos = 0;
   int write_ret;

   int64_t now;
   int64_t expire = 0;

   BSON_ASSERT (tls);
   BSON_ASSERT (iov);
   BSON_ASSERT (iovcnt);

   tls->timeout_msec = timeout_msec;

   if (timeout_msec >= 0) {
      expire = bson_get_monotonic_time () + (timeout_msec * 1000UL);
   }

   for (i = 0; i < iovcnt; i++) {
      iov_pos = 0;

      while (iov_pos < iov[i].iov_len) {
         write_ret = BIO_write (tls->bio, (char *)iov[i].iov_base + iov_pos,
                              (int)(iov[i].iov_len - iov_pos));

         if (write_ret < 0) {
            return write_ret;
         }

         if (expire) {
            now = bson_get_monotonic_time ();

            if ((expire - now) < 0) {
               if (write_ret == 0) {
                  mongoc_counter_streams_timeout_inc();
#ifdef _WIN32
                  errno = WSAETIMEDOUT;
#else
                  errno = ETIMEDOUT;
#endif
                  return -1;
               }

               tls->timeout_msec = 0;
            } else {
               tls->timeout_msec = (expire - now) / 1000L;
            }
         }

         ret += write_ret;
         iov_pos += write_ret;
      }
   }

   if (ret >= 0) {
      mongoc_counter_streams_egress_add(ret);
   }

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_stream_tls_readv --
 *
 *       Read from the stream into iov. This function will try to read
 *       all of the bytes or fail. If the number of bytes is not equal
 *       to the number requested, a failure or EOF has occurred.
 *
 * Returns:
 *       -1 on failure, 0 on EOF, otherwise the number of bytes read.
 *
 * Side effects:
 *       iov buffers will be written to.
 *
 *--------------------------------------------------------------------------
 */

static ssize_t
_mongoc_stream_tls_readv (mongoc_stream_t *stream,
                          mongoc_iovec_t  *iov,
                          size_t           iovcnt,
                          size_t           min_bytes,
                          int32_t          timeout_msec)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   ssize_t ret = 0;
   size_t i;
   int read_ret;
   size_t iov_pos = 0;
   int64_t now;
   int64_t expire = 0;

   BSON_ASSERT (tls);
   BSON_ASSERT (iov);
   BSON_ASSERT (iovcnt);

   tls->timeout_msec = timeout_msec;

   if (timeout_msec >= 0) {
      expire = bson_get_monotonic_time () + (timeout_msec * 1000UL);
   }

   for (i = 0; i < iovcnt; i++) {
      iov_pos = 0;

      while (iov_pos < iov[i].iov_len) {
         read_ret = BIO_read (tls->bio, (char *)iov[i].iov_base + iov_pos,
                              (int)(iov[i].iov_len - iov_pos));

         if (read_ret < 0) {
            return read_ret;
         }

         if (expire) {
            now = bson_get_monotonic_time ();

            if ((expire - now) < 0) {
               if (read_ret == 0) {
                  mongoc_counter_streams_timeout_inc();
#ifdef _WIN32
                  errno = WSAETIMEDOUT;
#else
                  errno = ETIMEDOUT;
#endif
                  return -1;
               }

               tls->timeout_msec = 0;
            } else {
               tls->timeout_msec = (expire - now) / 1000L;
            }
         }

         ret += read_ret;

         if ((size_t)ret >= min_bytes) {
            mongoc_counter_streams_ingress_add(ret);
            return ret;
         }

         iov_pos += read_ret;
      }
   }

   if (ret >= 0) {
      mongoc_counter_streams_ingress_add(ret);
   }

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_stream_tls_setsockopt --
 *
 *       Perform a setsockopt on the underlying stream.
 *
 * Returns:
 *       -1 on failure, otherwise opt specific value.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static int
_mongoc_stream_tls_setsockopt (mongoc_stream_t *stream,
                               int              level,
                               int              optname,
                               void            *optval,
                               socklen_t        optlen)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;

   BSON_ASSERT (tls);

   return mongoc_stream_setsockopt (tls->base_stream,
                                    level,
                                    optname,
                                    optval,
                                    optlen);
}


/**
 * mongoc_stream_tls_do_handshake:
 *
 * force an ssl handshake
 *
 * This will happen on the first read or write otherwise
 */
bool
mongoc_stream_tls_do_handshake (mongoc_stream_t *stream,
                                int32_t          timeout_msec)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;

   BSON_ASSERT (tls);

   tls->timeout_msec = timeout_msec;

   if (BIO_do_handshake (tls->bio) == 1) {
      return true;
   }

   if (!errno) {
#ifdef _WIN32
      errno = WSAETIMEDOUT;
#else
      errno = ETIMEDOUT;
#endif
   }

   return false;
}


/**
 * mongoc_stream_tls_check_cert:
 *
 * check the cert returned by the other party
 */
bool
mongoc_stream_tls_check_cert (mongoc_stream_t *stream,
                              const char      *host)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   SSL *ssl;

   BSON_ASSERT (tls);
   BSON_ASSERT (host);

   BIO_get_ssl (tls->bio, &ssl);

   return _mongoc_ssl_check_cert (ssl, host, tls->weak_cert_validation);
}


static mongoc_stream_t *
_mongoc_stream_tls_get_base_stream (mongoc_stream_t *stream)
{
   return ((mongoc_stream_tls_t *)stream)->base_stream;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_tls_new --
 *
 *       Creates a new mongoc_stream_tls_t to communicate with a remote
 *       server using a TLS stream.
 *
 *       @base_stream should be a stream that will become owned by the
 *       resulting tls stream. It will be used for raw I/O.
 *
 *       @trust_store_dir should be a path to the SSL cert db to use for
 *       verifying trust of the remote server.
 *
 * Returns:
 *       NULL on failure, otherwise a mongoc_stream_t.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_stream_t *
mongoc_stream_tls_new (mongoc_stream_t  *base_stream,
                       mongoc_ssl_opt_t *opt,
                       int               client)
{
   mongoc_stream_tls_t *tls;
   SSL_CTX *ssl_ctx = NULL;

   BIO *bio_ssl = NULL;
   BIO *bio_mongoc_shim = NULL;

   BSON_ASSERT(base_stream);
   BSON_ASSERT(opt);

   ssl_ctx = _mongoc_ssl_ctx_new (opt);

   if (!ssl_ctx) {
      return NULL;
   }

   bio_ssl = BIO_new_ssl (ssl_ctx, client);
   bio_mongoc_shim = BIO_new (&gMongocStreamTlsRawMethods);

   BIO_push (bio_ssl, bio_mongoc_shim);

   tls = bson_malloc0 (sizeof *tls);
   tls->base_stream = base_stream;
   tls->parent.type = MONGOC_STREAM_TLS;
   tls->parent.destroy = _mongoc_stream_tls_destroy;
   tls->parent.close = _mongoc_stream_tls_close;
   tls->parent.flush = _mongoc_stream_tls_flush;
   tls->parent.writev = _mongoc_stream_tls_writev;
   tls->parent.readv = _mongoc_stream_tls_readv;
   tls->parent.setsockopt = _mongoc_stream_tls_setsockopt;
   tls->parent.get_base_stream = _mongoc_stream_tls_get_base_stream;
   tls->weak_cert_validation = opt->weak_cert_validation;
   tls->bio = bio_ssl;
   tls->ctx = ssl_ctx;
   tls->timeout_msec = -1;
   bio_mongoc_shim->ptr = tls;

   mongoc_counter_streams_active_inc();

   return (mongoc_stream_t *)tls;
}

/*==============================================================*/
/* --- mongoc-uri.c */

#if defined(_WIN32) && !defined(strcasecmp)
# define strcasecmp _stricmp
#endif


struct _mongoc_uri_t
{
   char                   *str;
   mongoc_host_list_t     *hosts;
   char                   *username;
   char                   *password;
   char                   *database;
   bson_t                  options;
   bson_t                  read_prefs;
   mongoc_write_concern_t *write_concern;
};


static void
mongoc_uri_do_unescape (char **str)
{
   char *tmp;

   if ((tmp = *str)) {
      *str = mongoc_uri_unescape(tmp);
      bson_free(tmp);
   }
}


static void
mongoc_uri_append_host (mongoc_uri_t  *uri,
                        const char    *host,
                        uint16_t       port)
{
   mongoc_host_list_t *iter;
   mongoc_host_list_t *link_;

   link_ = bson_malloc0(sizeof *link_);
   bson_strncpy (link_->host, host, sizeof link_->host);
   if (strchr (host, ':')) {
      bson_snprintf (link_->host_and_port, sizeof link_->host_and_port,
                     "[%s]:%hu", host, port);
      link_->family = AF_INET6;
   } else {
      bson_snprintf (link_->host_and_port, sizeof link_->host_and_port,
                     "%s:%hu", host, port);
      link_->family = strstr (host, ".sock") ? AF_UNIX : AF_INET;
   }
   link_->host_and_port[sizeof link_->host_and_port - 1] = '\0';
   link_->port = port;

   if ((iter = uri->hosts)) {
      for (; iter && iter->next; iter = iter->next) {}
      iter->next = link_;
   } else {
      uri->hosts = link_;
   }
}


static char *
scan_to_unichar (const char      *str,
                 bson_unichar_t   stop,
                 const char     **end)
{
   bson_unichar_t c;
   const char *iter;

   for (iter = str;
        iter && *iter && (c = bson_utf8_get_char(iter));
        iter = bson_utf8_next_char(iter))
   {
      if (c == stop) {
         *end = iter;
         return bson_strndup(str, iter - str);
      } else if (c == '\\') {
         iter = bson_utf8_next_char(iter);
         if (!bson_utf8_get_char(iter)) {
            break;
         }
      }
   }

   return NULL;
}


static bool
mongoc_uri_parse_scheme (const char    *str,
                         const char   **end)
{
   if (!!strncmp(str, "mongodb://", 10)) {
      return false;
   }

   *end = str + 10;

   return true;
}


static bool
mongoc_uri_parse_userpass (mongoc_uri_t  *uri,
                           const char    *str,
                           const char   **end)
{
   bool ret = false;
   const char *end_userpass;
   const char *end_user;
   char *s;

   if ((s = scan_to_unichar(str, '@', &end_userpass))) {
      if ((uri->username = scan_to_unichar(s, ':', &end_user))) {
         uri->password = bson_strdup(end_user + 1);
      } else {
         uri->username = bson_strndup(str, end_userpass - str);
         uri->password = NULL;
      }
      mongoc_uri_do_unescape(&uri->username);
      mongoc_uri_do_unescape(&uri->password);
      *end = end_userpass + 1;
      bson_free(s);
      ret = true;
   } else {
      ret = true;
   }

   return ret;
}


static bool
mongoc_uri_parse_host6 (mongoc_uri_t  *uri,
                        const char    *str)
{
   uint16_t port = MONGOC_DEFAULT_PORT;
   const char *portstr;
   const char *end_host;
   char *hostname;

   if ((portstr = strrchr (str, ':')) && !strstr (portstr, "]")) {
#ifdef _MSC_VER
      sscanf_s (portstr, ":%hu", &port);
#else
      sscanf (portstr, ":%hu", &port);
#endif
   }

   hostname = scan_to_unichar (str + 1, ']', &end_host);

   mongoc_uri_do_unescape (&hostname);
   mongoc_uri_append_host (uri, hostname, port);
   bson_free (hostname);

   return true;
}


static bool
mongoc_uri_parse_host (mongoc_uri_t  *uri,
                       const char    *str)
{
   uint16_t port;
   const char *end_host;
   char *hostname;

   if (*str == '[' && strchr (str, ']')) {
      return mongoc_uri_parse_host6 (uri, str);
   }

   if ((hostname = scan_to_unichar(str, ':', &end_host))) {
      end_host++;
      if (!isdigit(*end_host)) {
         bson_free(hostname);
         return false;
      }
#ifdef _MSC_VER
      sscanf_s (end_host, "%hu", &port);
#else
      sscanf (end_host, "%hu", &port);
#endif
   } else {
      hostname = bson_strdup(str);
      port = MONGOC_DEFAULT_PORT;
   }

   mongoc_uri_do_unescape(&hostname);
   mongoc_uri_append_host(uri, hostname, port);
   bson_free(hostname);

   return true;
}


bool
_mongoc_host_list_from_string (mongoc_host_list_t *host_list,
                               const char         *host_and_port)
{
   uint16_t port;
   const char *end_host;
   char *hostname = NULL;

   bson_return_val_if_fail(host_list, false);
   bson_return_val_if_fail(host_and_port, false);

   memset(host_list, 0, sizeof *host_list);

   if ((hostname = scan_to_unichar(host_and_port, ':', &end_host))) {
      end_host++;
      if (!isdigit(*end_host)) {
         bson_free(hostname);
         return false;
      }
#ifdef _MSC_VER
      sscanf_s (end_host, "%hu", &port);
#else
      sscanf (end_host, "%hu", &port);
#endif
   } else {
      hostname = bson_strdup(host_and_port);
      port = MONGOC_DEFAULT_PORT;
   }

   bson_strncpy (host_list->host_and_port, host_and_port,
           sizeof host_list->host_and_port - 1);

   bson_strncpy (host_list->host, hostname, sizeof host_list->host - 1);

   host_list->port = port;
   host_list->family = AF_INET;

   bson_free(hostname);

   return true;
}


static bool
mongoc_uri_parse_hosts (mongoc_uri_t  *uri,
                        const char    *str,
                        const char   **end)
{
   bool ret = false;
   const char *end_hostport;
   const char *sock;
   const char *tmp;
   char *s;

   /*
    * Parsing the series of hosts is a lot more complicated than you might
    * imagine. This is due to some characters being both separators as well as
    * valid characters within the "hostname". In particularly, we can have file
    * paths to specify paths to UNIX domain sockets. We impose the restriction
    * that they must be suffixed with ".sock" to simplify the parsing.
    *
    * You can separate hosts and file system paths to UNIX domain sockets with
    * ",".
    *
    * When you reach a "/" or "?" that is not part of a file-system path, we
    * have completed our parsing of hosts.
    */

again:
   if (((*str == '/') && (sock = strstr(str, ".sock"))) &&
       (!(tmp = strstr(str, ",")) || (tmp > sock)) &&
       (!(tmp = strstr(str, "?")) || (tmp > sock))) {
      s = bson_strndup(str, sock + 5 - str);
      if (!mongoc_uri_parse_host(uri, s)) {
         bson_free(s);
         return false;
      }
      bson_free(s);
      str = sock + 5;
      ret = true;
      if (*str == ',') {
         str++;
         goto again;
      }
   } else if ((s = scan_to_unichar(str, ',', &end_hostport))) {
      if (!mongoc_uri_parse_host(uri, s)) {
         bson_free(s);
         return false;
      }
      bson_free(s);
      str = end_hostport + 1;
      ret = true;
      goto again;
   } else if ((s = scan_to_unichar(str, '/', &end_hostport)) ||
              (s = scan_to_unichar(str, '?', &end_hostport))) {
      if (!mongoc_uri_parse_host(uri, s)) {
         bson_free(s);
         return false;
      }
      bson_free(s);
      *end = end_hostport;
      return true;
   } else if (*str) {
      if (!mongoc_uri_parse_host(uri, str)) {
         return false;
      }
      *end = str + strlen(str);
      return true;
   }

   return ret;
}


static bool
mongoc_uri_parse_database (mongoc_uri_t  *uri,
                           const char    *str,
                           const char   **end)
{
   const char *end_database;

   if ((uri->database = scan_to_unichar(str, '?', &end_database))) {
      *end = end_database;
   } else if (*str) {
      uri->database = bson_strdup(str);
      *end = str + strlen(str);
   }

   mongoc_uri_do_unescape(&uri->database);

   return true;
}


static void
mongoc_uri_parse_tags (mongoc_uri_t *uri, /* IN */
                       const char   *str, /* IN */
                       bson_t       *doc) /* IN */
{
   const char *end_keyval;
   const char *end_key;
   bson_t b;
   char *keyval;
   char *key;
   char keystr[32];
   int i;

   bson_init(&b);

again:
   if ((keyval = scan_to_unichar(str, ',', &end_keyval))) {
      if ((key = scan_to_unichar(keyval, ':', &end_key))) {
         bson_append_utf8(&b, key, -1, end_key + 1, -1);
         bson_free(key);
      }
      bson_free(keyval);
      str = end_keyval + 1;
      goto again;
   } else {
      if ((key = scan_to_unichar(str, ':', &end_key))) {
         bson_append_utf8(&b, key, -1, end_key + 1, -1);
         bson_free(key);
      }
   }

   i = bson_count_keys(doc);
   bson_snprintf(keystr, sizeof keystr, "%u", i);
   bson_append_document(doc, keystr, -1, &b);
   bson_destroy(&b);
}


static bool
mongoc_uri_parse_option (mongoc_uri_t *uri,
                         const char   *str)
{
   int32_t v_int;
   const char *end_key;
   char *key;
   char *value;

   if (!(key = scan_to_unichar(str, '=', &end_key))) {
      return false;
   }

   value = bson_strdup(end_key + 1);
   mongoc_uri_do_unescape(&value);

   if (!strcasecmp(key, "connecttimeoutms") ||
       !strcasecmp(key, "sockettimeoutms") ||
       !strcasecmp(key, "maxpoolsize") ||
       !strcasecmp(key, "minpoolsize") ||
       !strcasecmp(key, "maxidletimems") ||
       !strcasecmp(key, "waitqueuemultiple") ||
       !strcasecmp(key, "waitqueuetimeoutms") ||
       !strcasecmp(key, "wtimeoutms")) {
      v_int = (int) strtol (value, NULL, 10);
      bson_append_int32(&uri->options, key, -1, v_int);
   } else if (!strcasecmp(key, "w")) {
      if (*value == '-' || isdigit(*value)) {
         v_int = (int) strtol (value, NULL, 10);
         BSON_APPEND_INT32 (&uri->options, "w", v_int);
      } else if (0 == strcasecmp (value, "majority")) {
         BSON_APPEND_UTF8 (&uri->options, "w", "majority");
      } else if (*value) {
         BSON_APPEND_UTF8 (&uri->options, "W", value);
      }
   } else if (!strcasecmp(key, "canonicalizeHostname") ||
              !strcasecmp(key, "journal") ||
              !strcasecmp(key, "safe") ||
              !strcasecmp(key, "slaveok") ||
              !strcasecmp(key, "ssl")) {
      bson_append_bool (&uri->options, key, -1,
                        (0 == strcasecmp (value, "true")) ||
                        (0 == strcasecmp (value, "t")) ||
                        (0 == strcmp (value, "1")));
   } else if (!strcasecmp(key, "readpreferencetags")) {
      mongoc_uri_parse_tags(uri, value, &uri->read_prefs);
   } else {
      bson_append_utf8(&uri->options, key, -1, value, -1);
   }

   bson_free(key);
   bson_free(value);

   return true;
}


static bool
mongoc_uri_parse_options (mongoc_uri_t *uri,
                          const char   *str)
{
   const char *end_option;
   char *option;

again:
   if ((option = scan_to_unichar(str, '&', &end_option))) {
      if (!mongoc_uri_parse_option(uri, option)) {
         bson_free(option);
         return false;
      }
      bson_free(option);
      str = end_option + 1;
      goto again;
   } else if (*str) {
      if (!mongoc_uri_parse_option(uri, str)) {
         return false;
      }
   }

   return true;
}


static bool
mongoc_uri_parse (mongoc_uri_t *uri,
                  const char   *str)
{
   if (!mongoc_uri_parse_scheme(str, &str)) {
      return false;
   }

   if (!*str || !mongoc_uri_parse_userpass(uri, str, &str)) {
      return false;
   }

   if (!*str || !mongoc_uri_parse_hosts(uri, str, &str)) {
      return false;
   }

   switch (*str) {
   case '/':
      str++;
      if (*str && !mongoc_uri_parse_database(uri, str, &str)) {
         return false;
      }
      if (!*str) {
         break;
      }
      /* Fall through */
   case '?':
      str++;
      if (*str && !mongoc_uri_parse_options(uri, str)) {
         return false;
      }
      break;
   default:
      break;
   }

   return true;
}


const mongoc_host_list_t *
mongoc_uri_get_hosts (const mongoc_uri_t *uri)
{
   bson_return_val_if_fail(uri, NULL);
   return uri->hosts;
}


const char *
mongoc_uri_get_replica_set (const mongoc_uri_t *uri)
{
   bson_iter_t iter;

   bson_return_val_if_fail(uri, NULL);

   if (bson_iter_init_find_case(&iter, &uri->options, "replicaSet") &&
       BSON_ITER_HOLDS_UTF8(&iter)) {
      return bson_iter_utf8(&iter, NULL);
   }

   return NULL;
}


const char *
mongoc_uri_get_auth_mechanism (const mongoc_uri_t *uri)
{
   bson_iter_t iter;

   bson_return_val_if_fail (uri, NULL);

   if (bson_iter_init_find_case (&iter, &uri->options, "authMechanism") &&
       BSON_ITER_HOLDS_UTF8 (&iter)) {
      return bson_iter_utf8 (&iter, NULL);
   }

   return NULL;
}


static void
_mongoc_uri_build_write_concern (mongoc_uri_t *uri) /* IN */
{
   mongoc_write_concern_t *write_concern;
   const char *str;
   bson_iter_t iter;
   int32_t wtimeoutms = 0;
   int value;

   BSON_ASSERT (uri);

   write_concern = mongoc_write_concern_new ();

   if (bson_iter_init_find_case (&iter, &uri->options, "safe") &&
       BSON_ITER_HOLDS_BOOL (&iter) &&
       !bson_iter_bool (&iter)) {
      mongoc_write_concern_set_w (write_concern,
                                  MONGOC_WRITE_CONCERN_W_UNACKNOWLEDGED);
   }

   if (bson_iter_init_find_case (&iter, &uri->options, "wtimeoutms") &&
       BSON_ITER_HOLDS_INT32 (&iter)) {
      wtimeoutms = bson_iter_int32 (&iter);
   }

   if (bson_iter_init_find_case (&iter, &uri->options, "journal") &&
       BSON_ITER_HOLDS_BOOL (&iter) &&
       bson_iter_bool (&iter)) {
      mongoc_write_concern_set_journal (write_concern, true);
   }

   if (bson_iter_init_find_case (&iter, &uri->options, "w")) {
      if (BSON_ITER_HOLDS_INT32 (&iter)) {
         value = bson_iter_int32 (&iter);

         switch (value) {
         case -1:
            mongoc_write_concern_set_w (write_concern,
                                        MONGOC_WRITE_CONCERN_W_ERRORS_IGNORED);
            break;
         case 0:
            mongoc_write_concern_set_w (write_concern,
                                        MONGOC_WRITE_CONCERN_W_UNACKNOWLEDGED);
            break;
         case 1:
            mongoc_write_concern_set_w (write_concern,
                                        MONGOC_WRITE_CONCERN_W_DEFAULT);
            break;
         default:
            if (value > 1) {
               mongoc_write_concern_set_w (write_concern, value);
               break;
            }
            MONGOC_WARNING ("Unsupported w value [w=%d].", value);
            break;
         }
      } else if (BSON_ITER_HOLDS_UTF8 (&iter)) {
         str = bson_iter_utf8 (&iter, NULL);

         if (0 == strcasecmp ("majority", str)) {
            mongoc_write_concern_set_wmajority (write_concern, wtimeoutms);
         } else {
            mongoc_write_concern_set_wtag (write_concern, str);
         }
      } else {
         BSON_ASSERT (false);
      }
   }

   uri->write_concern = write_concern;
}


mongoc_uri_t *
mongoc_uri_new (const char *uri_string)
{
   mongoc_uri_t *uri;

   uri = bson_malloc0(sizeof *uri);
   bson_init(&uri->options);
   bson_init(&uri->read_prefs);

   if (!uri_string) {
      uri_string = "mongodb://127.0.0.1/";
   }

   if (!mongoc_uri_parse(uri, uri_string)) {
      mongoc_uri_destroy(uri);
      return NULL;
   }

   uri->str = bson_strdup(uri_string);

   _mongoc_uri_build_write_concern (uri);

   return uri;
}


mongoc_uri_t *
mongoc_uri_new_for_host_port (const char *hostname,
                              uint16_t    port)
{
   mongoc_uri_t *uri;
   char *str;

   bson_return_val_if_fail(hostname, NULL);
   bson_return_val_if_fail(port, NULL);

   str = bson_strdup_printf("mongodb://%s:%hu/", hostname, port);
   uri = mongoc_uri_new(str);
   bson_free(str);

   return uri;
}


const char *
mongoc_uri_get_username (const mongoc_uri_t *uri)
{
   bson_return_val_if_fail(uri, NULL);
   return uri->username;
}


const char *
mongoc_uri_get_password (const mongoc_uri_t *uri)
{
   bson_return_val_if_fail(uri, NULL);
   return uri->password;
}


const char *
mongoc_uri_get_database (const mongoc_uri_t *uri)
{
   bson_return_val_if_fail(uri, NULL);
   return uri->database;
}


const char *
mongoc_uri_get_auth_source (const mongoc_uri_t *uri)
{
   bson_iter_t iter;

   bson_return_val_if_fail(uri, NULL);

   if (bson_iter_init_find_case(&iter, &uri->options, "authSource")) {
      return bson_iter_utf8(&iter, NULL);
   }

   return uri->database ? uri->database : "admin";
}


const bson_t *
mongoc_uri_get_options (const mongoc_uri_t *uri)
{
   bson_return_val_if_fail(uri, NULL);
   return &uri->options;
}


void
mongoc_uri_destroy (mongoc_uri_t *uri)
{
   mongoc_host_list_t *tmp;

   if (uri) {
      while (uri->hosts) {
         tmp = uri->hosts;
         uri->hosts = tmp->next;
         bson_free(tmp);
      }

      bson_free(uri->str);
      bson_free(uri->database);
      bson_free(uri->username);
      bson_destroy(&uri->options);
      bson_destroy(&uri->read_prefs);
      mongoc_write_concern_destroy(uri->write_concern);

      if (uri->password) {
         bson_zero_free(uri->password, strlen(uri->password));
      }

      bson_free(uri);
   }
}


mongoc_uri_t *
mongoc_uri_copy (const mongoc_uri_t *uri)
{
   return mongoc_uri_new(uri->str);
}


const char *
mongoc_uri_get_string (const mongoc_uri_t *uri)
{
   bson_return_val_if_fail(uri, NULL);
   return uri->str;
}


const bson_t *
mongoc_uri_get_read_prefs (const mongoc_uri_t *uri)
{
   bson_return_val_if_fail(uri, NULL);
   return &uri->read_prefs;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_uri_unescape --
 *
 *       Escapes an UTF-8 encoded string containing URI escaped segments
 *       such as %20.
 *
 *       It is a programming error to call this function with a string
 *       that is not UTF-8 encoded!
 *
 * Returns:
 *       A newly allocated string that should be freed with bson_free().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

char *
mongoc_uri_unescape (const char *escaped_string)
{
   bson_unichar_t c;
   bson_string_t *str;
   unsigned int hex = 0;
   const char *ptr;
   const char *end;
   size_t len;

   bson_return_val_if_fail(escaped_string, NULL);

   len = strlen(escaped_string);

   /*
    * Double check that this is a UTF-8 valid string. Bail out if necessary.
    */
   if (!bson_utf8_validate(escaped_string, len, false)) {
      MONGOC_WARNING("%s(): escaped_string contains invalid UTF-8",
                     __FUNCTION__);
      return false;
   }

   ptr = escaped_string;
   end = ptr + len;
   str = bson_string_new(NULL);

   for (; *ptr; ptr = bson_utf8_next_char(ptr)) {
      c = bson_utf8_get_char(ptr);
      switch (c) {
      case '%':
         if (((end - ptr) < 2) ||
             !isxdigit(ptr[1]) ||
             !isxdigit(ptr[2]) ||
#ifdef _MSC_VER
             (1 != sscanf_s(&ptr[1], "%02x", &hex)) ||
#else
             (1 != sscanf(&ptr[1], "%02x", &hex)) ||
#endif
             !isprint(hex)) {
            bson_string_free(str, true);
            return NULL;
         }
         bson_string_append_c(str, hex);
         ptr += 2;
         break;
      default:
         bson_string_append_unichar(str, c);
         break;
      }
   }

   return bson_string_free(str, false);
}


const mongoc_write_concern_t *
mongoc_uri_get_write_concern (const mongoc_uri_t *uri) /* IN */
{
   bson_return_val_if_fail (uri, NULL);

   return uri->write_concern;
}


bool
mongoc_uri_get_ssl (const mongoc_uri_t *uri) /* IN */
{
   bson_iter_t iter;

   bson_return_val_if_fail (uri, false);

   return (bson_iter_init_find_case (&iter, &uri->options, "ssl") &&
           BSON_ITER_HOLDS_BOOL (&iter) &&
           bson_iter_bool (&iter));
}

/*==============================================================*/
/* --- mongoc-util.c */

char *
_mongoc_hex_md5 (const char *input)
{
   uint8_t digest[16];
   bson_md5_t md5;
   char digest_str[33];
   int i;

   bson_md5_init(&md5);
   bson_md5_append(&md5, (const uint8_t *)input, (uint32_t)strlen(input));
   bson_md5_finish(&md5, digest);

   for (i = 0; i < (int)sizeof digest; i++) {
      bson_snprintf(&digest_str[i*2], 3, "%02x", digest[i]);
   }
   digest_str[sizeof digest_str - 1] = '\0';

   return bson_strdup(digest_str);
}

/*==============================================================*/
/* --- mongoc-write-command.c */

/*
 * TODO:
 *
 *    - Remove error parameter to ops, favor result->error.
 *    - Try to receive GLE on the stack (legacy only?)
 */


#define MAX_INSERT_BATCH 1000
#define SUPPORTS_WRITE_COMMANDS(n) \
   (((n)->min_wire_version <= 2) && ((n)->max_wire_version >= 2))
#define WRITE_CONCERN_DOC(wc) \
   (wc && _mongoc_write_concern_needs_gle ((wc))) ? \
   (_mongoc_write_concern_get_bson((mongoc_write_concern_t*)(wc))) : \
   (&gEmptyWriteConcern)


typedef void (*mongoc_write_op_t) (mongoc_write_command_t       *command,
                                   mongoc_client_t              *client,
                                   uint32_t                      hint,
                                   const char                   *database,
                                   const char                   *collection,
                                   const mongoc_write_concern_t *write_concern,
                                   mongoc_write_result_t        *result,
                                   bson_error_t                 *error);


static bson_t gEmptyWriteConcern = BSON_INITIALIZER;

static int32_t
_mongoc_write_result_merge_arrays (mongoc_write_result_t *result,
                                   bson_t                *dest,
                                   bson_iter_t           *iter);
void
_mongoc_write_command_insert_append (mongoc_write_command_t *command,
                                     const bson_t * const   *documents,
                                     uint32_t                n_documents)
{
   const char *key;
   bson_iter_t iter;
   bson_oid_t oid;
   uint32_t i;
   bson_t tmp;
   char keydata [16];

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (command->type == MONGOC_WRITE_COMMAND_INSERT);
   BSON_ASSERT (!n_documents || documents);

   for (i = 0; i < n_documents; i++) {
      BSON_ASSERT (documents [i]);
      BSON_ASSERT (documents [i]->len >= 5);

      key = NULL;
      bson_uint32_to_string (i, &key, keydata, sizeof keydata);
      BSON_ASSERT (key);

      /*
       * If the document does not contain an "_id" field, we need to generate
       * a new oid for "_id".
       */
      if (!bson_iter_init_find (&iter, documents [i], "_id")) {
         bson_init (&tmp);
         bson_oid_init (&oid, NULL);
         BSON_APPEND_OID (&tmp, "_id", &oid);
         bson_concat (&tmp, documents [i]);
         BSON_APPEND_DOCUMENT (command->u.insert.documents, key, &tmp);
         bson_destroy (&tmp);
      } else {
         BSON_APPEND_DOCUMENT (command->u.insert.documents, key,
                               documents [i]);
      }
   }

   if (command->u.insert.n_documents) {
      command->u.insert.n_merged++;
   }

   command->u.insert.n_documents += n_documents;

   EXIT;
}


void
_mongoc_write_command_init_insert
      (mongoc_write_command_t *command,              /* IN */
       const bson_t * const   *documents,            /* IN */
       uint32_t                n_documents,          /* IN */
       bool                    ordered,              /* IN */
       bool                    allow_bulk_op_insert) /* IN */
{
   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (!n_documents || documents);

   command->type = MONGOC_WRITE_COMMAND_INSERT;
   command->u.insert.documents = bson_new ();
   command->u.insert.n_documents = 0;
   command->u.insert.n_merged = 0;
   command->u.insert.ordered = ordered;
   command->u.insert.allow_bulk_op_insert = allow_bulk_op_insert;

   if (n_documents) {
      _mongoc_write_command_insert_append (command, documents, n_documents);
   }

   EXIT;
}


void
_mongoc_write_command_init_delete (mongoc_write_command_t *command,  /* IN */
                                   const bson_t           *selector, /* IN */
                                   bool                    multi,    /* IN */
                                   bool                    ordered)  /* IN */
{
   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (selector);

   command->type = MONGOC_WRITE_COMMAND_DELETE;
   command->u.delete.selector = bson_copy (selector);
   command->u.delete.multi = multi;
   command->u.delete.ordered = ordered;

   EXIT;
}


void
_mongoc_write_command_init_update (mongoc_write_command_t *command,  /* IN */
                                   const bson_t           *selector, /* IN */
                                   const bson_t           *update,   /* IN */
                                   bool                    upsert,   /* IN */
                                   bool                    multi,    /* IN */
                                   bool                    ordered)  /* IN */
{
   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (selector);
   BSON_ASSERT (update);

   command->type = MONGOC_WRITE_COMMAND_UPDATE;
   command->u.update.selector = bson_copy (selector);
   command->u.update.update = bson_copy (update);
   command->u.update.upsert = upsert;
   command->u.update.multi = multi;
   command->u.update.ordered = ordered;

   EXIT;
}


static void
_mongoc_write_command_delete_legacy (mongoc_write_command_t       *command,
                                     mongoc_client_t              *client,
                                     uint32_t                      hint,
                                     const char                   *database,
                                     const char                   *collection,
                                     const mongoc_write_concern_t *write_concern,
                                     mongoc_write_result_t        *result,
                                     bson_error_t                 *error)
{
   mongoc_rpc_t rpc;
   bson_t *gle = NULL;
   char ns [MONGOC_NAMESPACE_MAX + 1];

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (hint);
   BSON_ASSERT (collection);

   bson_snprintf (ns, sizeof ns, "%s.%s", database, collection);

   rpc.delete.msg_len = 0;
   rpc.delete.request_id = 0;
   rpc.delete.response_to = 0;
   rpc.delete.opcode = MONGOC_OPCODE_DELETE;
   rpc.delete.zero = 0;
   rpc.delete.collection = ns;
   rpc.delete.flags = command->u.delete.multi ? 0 : MONGOC_DELETE_SINGLE_REMOVE;
   rpc.delete.selector = bson_get_data (command->u.delete.selector);

   hint = _mongoc_client_sendv (client, &rpc, 1, hint, write_concern,
                                NULL, error);

   if (!hint) {
      result->failed = true;
      GOTO (cleanup);
   }

   if (_mongoc_write_concern_needs_gle (write_concern)) {
      if (!_mongoc_client_recv_gle (client, hint, &gle, error)) {
         result->failed = true;
         GOTO (cleanup);
      }
   }

cleanup:
   if (gle) {
      _mongoc_write_result_merge_legacy (result, command, gle);
      bson_destroy (gle);
   }

   EXIT;
}


static void
_mongoc_write_command_insert_legacy (mongoc_write_command_t       *command,
                                     mongoc_client_t              *client,
                                     uint32_t                      hint,
                                     const char                   *database,
                                     const char                   *collection,
                                     const mongoc_write_concern_t *write_concern,
                                     mongoc_write_result_t        *result,
                                     bson_error_t                 *error)
{
   mongoc_iovec_t *iov;
   const uint8_t *data;
   mongoc_rpc_t rpc;
   bson_iter_t iter;
   uint32_t len;
   bson_t *gle = NULL;
   size_t size = 0;
   bool has_more = false;
   char ns [MONGOC_NAMESPACE_MAX + 1];
   bool r;
   int i;
   int max_docs = MAX_INSERT_BATCH;

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (hint);
   BSON_ASSERT (collection);
   BSON_ASSERT (command->type == MONGOC_WRITE_COMMAND_INSERT);

   if (command->u.insert.ordered || !command->u.insert.allow_bulk_op_insert) {
      max_docs = 1;
   }

   r = bson_iter_init (&iter, command->u.insert.documents);
   if (!r) {
      BSON_ASSERT (false);
      EXIT;
   }

   if (!command->u.insert.n_documents || !bson_iter_next (&iter)) {
      bson_set_error (error,
                      MONGOC_ERROR_COLLECTION,
                      MONGOC_ERROR_COLLECTION_INSERT_FAILED,
                      "Cannot do an empty insert.");
      result->failed = true;
      EXIT;
   }

   bson_snprintf (ns, sizeof ns, "%s.%s", database, collection);

   iov = bson_malloc ((sizeof *iov) * command->u.insert.n_documents);

again:
   has_more = false;
   i = 0;
   size = (sizeof (mongoc_rpc_header_t) +
           4 +
           strlen (database) +
           1 +
           strlen (collection) +
           1);

   do {
      BSON_ASSERT (BSON_ITER_HOLDS_DOCUMENT (&iter));
      BSON_ASSERT (i < (int)command->u.insert.n_documents);

      bson_iter_document (&iter, &len, &data);

      BSON_ASSERT (data);
      BSON_ASSERT (len >= 5);

      /*
       * Check that the server can receive this document.
       */
      if ((len > (uint32_t)client->cluster.max_bson_size) ||
          (len > (uint32_t)client->cluster.max_msg_size)) {
         bson_set_error (error,
                         MONGOC_ERROR_BSON,
                         MONGOC_ERROR_BSON_INVALID,
                         "Document %u is too large for the cluster. "
                         "Document is %u bytes, max is %u.",
                         i, (unsigned)len, client->cluster.max_bson_size);
      }

      /*
       * Check that we will not overflow our max message size.
       */
      if ((i == max_docs) || (size > (client->cluster.max_msg_size - len))) {
         has_more = true;
         break;
      }

      iov [i].iov_base = (void *)data;
      iov [i].iov_len = len;

      size += len;
      i++;
   } while (bson_iter_next (&iter));

   rpc.insert.msg_len = 0;
   rpc.insert.request_id = 0;
   rpc.insert.response_to = 0;
   rpc.insert.opcode = MONGOC_OPCODE_INSERT;
   rpc.insert.flags =
      (command->u.insert.ordered ? 0 : MONGOC_INSERT_CONTINUE_ON_ERROR);
   rpc.insert.collection = ns;
   rpc.insert.documents = iov;
   rpc.insert.n_documents = i;

   hint = _mongoc_client_sendv (client, &rpc, 1, hint, write_concern,
                                NULL, error);

   if (!hint) {
      result->failed = true;
      GOTO (cleanup);
   }

   if (_mongoc_write_concern_needs_gle (write_concern)) {
      bson_iter_t citer;

      if (!_mongoc_client_recv_gle (client, hint, &gle, error)) {
         result->failed = true;
         GOTO (cleanup);
      }

      /*
       * Overwrite the "n" field since it will be zero. Otherwise, our
       * merge_legacy code will not know how many we tried in this batch.
       */
      if (bson_iter_init_find (&citer, gle, "n") &&
          BSON_ITER_HOLDS_INT32 (&citer) &&
          !bson_iter_int32 (&citer)) {
         bson_iter_overwrite_int32 (&citer, i);
      }
   }

cleanup:
   if (gle) {
      command->u.insert.current_n_documents = i;
      _mongoc_write_result_merge_legacy (result, command, gle);
      bson_destroy (gle);
      gle = NULL;
   }

   if (has_more) {
      GOTO (again);
   }

   bson_free (iov);

   EXIT;
}


static void
_mongoc_write_command_update_legacy (mongoc_write_command_t       *command,
                                     mongoc_client_t              *client,
                                     uint32_t                      hint,
                                     const char                   *database,
                                     const char                   *collection,
                                     const mongoc_write_concern_t *write_concern,
                                     mongoc_write_result_t        *result,
                                     bson_error_t                 *error)
{
   mongoc_rpc_t rpc;
   bson_iter_t iter;
   bson_t *gle = NULL;
   size_t err_offset;
   char ns [MONGOC_NAMESPACE_MAX + 1];

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (hint);
   BSON_ASSERT (collection);

   if (bson_iter_init (&iter, command->u.update.update) &&
       bson_iter_next (&iter) &&
       (bson_iter_key (&iter) [0] != '$') &&
       !bson_validate (command->u.update.update,
                       (BSON_VALIDATE_UTF8 |
                        BSON_VALIDATE_UTF8_ALLOW_NULL |
                        BSON_VALIDATE_DOLLAR_KEYS |
                        BSON_VALIDATE_DOT_KEYS),
                       &err_offset)) {
      result->failed = true;
      bson_set_error (error,
                      MONGOC_ERROR_BSON,
                      MONGOC_ERROR_BSON_INVALID,
                      "update document is corrupt or contains "
                      "invalid keys including $ or .");
      EXIT;
   }

   bson_snprintf (ns, sizeof ns, "%s.%s", database, collection);

   rpc.update.msg_len = 0;
   rpc.update.request_id = 0;
   rpc.update.response_to = 0;
   rpc.update.opcode = MONGOC_OPCODE_UPDATE;
   rpc.update.zero = 0;
   rpc.update.collection = ns;
   rpc.update.flags =
      ((command->u.update.upsert ? MONGOC_UPDATE_UPSERT : 0) |
       (command->u.update.multi ? MONGOC_UPDATE_MULTI_UPDATE : 0));
   rpc.update.selector = bson_get_data (command->u.update.selector);
   rpc.update.update = bson_get_data (command->u.update.update);

   hint = _mongoc_client_sendv (client, &rpc, 1, hint, write_concern,
                                NULL, error);

   if (!hint) {
      result->failed = true;
      GOTO (cleanup);
   }

   if (_mongoc_write_concern_needs_gle (write_concern)) {
      if (!_mongoc_client_recv_gle (client, hint, &gle, error)) {
         result->failed = true;
         GOTO (cleanup);
      }
   }

cleanup:
   if (gle) {
      _mongoc_write_result_merge_legacy (result, command, gle);
      bson_destroy (gle);
   }

   EXIT;
}


static void
_mongoc_write_command_delete (mongoc_write_command_t       *command,
                              mongoc_client_t              *client,
                              uint32_t                      hint,
                              const char                   *database,
                              const char                   *collection,
                              const mongoc_write_concern_t *write_concern,
                              mongoc_write_result_t        *result,
                              bson_error_t                 *error)
{
   bson_t cmd = BSON_INITIALIZER;
   bson_t ar;
   bson_t child;
   bson_t reply;
   bool ret;

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (hint);
   BSON_ASSERT (collection);

   /*
    * If we have an unacknowledged write and the server supports the legacy
    * opcodes, then submit the legacy opcode so we don't need to wait for
    * a response from the server.
    */
   if ((client->cluster.nodes [hint - 1].min_wire_version == 0) &&
       !_mongoc_write_concern_needs_gle (write_concern)) {
      _mongoc_write_command_delete_legacy (command, client, hint, database,
                                           collection, write_concern, result,
                                           error);
      EXIT;
   }

   BSON_APPEND_UTF8 (&cmd, "delete", collection);
   BSON_APPEND_DOCUMENT (&cmd, "writeConcern",
                         WRITE_CONCERN_DOC (write_concern));
   BSON_APPEND_BOOL (&cmd, "ordered", command->u.delete.ordered);
   bson_append_array_begin (&cmd, "deletes", 7, &ar);
   bson_append_document_begin (&ar, "0", 1, &child);
   BSON_APPEND_DOCUMENT (&child, "q", command->u.delete.selector);
   BSON_APPEND_INT32 (&child, "limit", command->u.delete.multi ? 0 : 1);
   bson_append_document_end (&ar, &child);
   bson_append_array_end (&cmd, &ar);

   ret = mongoc_client_command_simple (client, database, &cmd, NULL,
                                       &reply, error);

   if (!ret) {
      result->failed = true;
   }

   _mongoc_write_result_merge (result, command, &reply);

   bson_destroy (&reply);
   bson_destroy (&cmd);

   EXIT;
}


static void
_mongoc_write_command_insert (mongoc_write_command_t       *command,
                              mongoc_client_t              *client,
                              uint32_t                      hint,
                              const char                   *database,
                              const char                   *collection,
                              const mongoc_write_concern_t *write_concern,
                              mongoc_write_result_t        *result,
                              bson_error_t                 *error)
{
   const uint8_t *data;
   bson_iter_t iter;
   const char *key;
   uint32_t len;
   bson_t tmp;
   bson_t ar;
   bson_t cmd;
   bson_t reply;
   char str [16];
   size_t overhead;
   bool has_more;
   bool ret;
   int i;

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (command->type == MONGOC_WRITE_COMMAND_INSERT);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (hint);
   BSON_ASSERT (collection);

   /*
    * If we have an unacknowledged write and the server supports the legacy
    * opcodes, then submit the legacy opcode so we don't need to wait for
    * a response from the server.
    */
   if ((client->cluster.nodes [hint - 1].min_wire_version == 0) &&
       !_mongoc_write_concern_needs_gle (write_concern)) {
      _mongoc_write_command_insert_legacy (command, client, hint, database,
                                           collection, write_concern, result,
                                           error);
      EXIT;
   }

   if (!command->u.insert.n_documents ||
       !bson_iter_init (&iter, command->u.insert.documents) ||
       !bson_iter_next (&iter)) {
      bson_set_error (error,
                      MONGOC_ERROR_COLLECTION,
                      MONGOC_ERROR_COLLECTION_INSERT_FAILED,
                      "Cannot do an empty insert.");
      result->failed = true;
      EXIT;
   }

   overhead = 1 + strlen ("documents") + 1;

again:
   bson_init (&cmd);
   has_more = false;
   i = 0;

   BSON_APPEND_UTF8 (&cmd, "insert", collection);
   BSON_APPEND_DOCUMENT (&cmd, "writeConcern",
                         WRITE_CONCERN_DOC (write_concern));
   BSON_APPEND_BOOL (&cmd, "ordered", command->u.insert.ordered);

   if ((command->u.insert.documents->len < (unsigned)client->cluster.max_bson_size) &&
       (command->u.insert.documents->len < (unsigned)client->cluster.max_msg_size) &&
       (command->u.insert.n_documents <= MAX_INSERT_BATCH)) {
      BSON_APPEND_ARRAY (&cmd, "documents", command->u.insert.documents);
   } else {
      bson_append_array_begin (&cmd, "documents", 9, &ar);

      do {
         if (!BSON_ITER_HOLDS_DOCUMENT (&iter)) {
            BSON_ASSERT (false);
         }

         bson_iter_document (&iter, &len, &data);

         if ((i == MAX_INSERT_BATCH) ||
             (len > (client->cluster.max_msg_size - cmd.len - overhead))) {
            has_more = true;
            break;
         }

         bson_uint32_to_string (i, &key, str, sizeof str);

         if (!bson_init_static (&tmp, data, len)) {
            BSON_ASSERT (false);
         }

         BSON_APPEND_DOCUMENT (&ar, key, &tmp);

         bson_destroy (&tmp);

         i++;
      } while (bson_iter_next (&iter));

      bson_append_array_end (&cmd, &ar);
   }

   ret = mongoc_client_command_simple (client, database, &cmd, NULL,
                                       &reply, error);

   if (!ret) {
      result->failed = true;
   }

   command->u.insert.current_n_documents = i;
   _mongoc_write_result_merge (result, command, &reply);

   bson_destroy (&cmd);
   bson_destroy (&reply);

   if (has_more && (ret || !command->u.insert.ordered)) {
      GOTO (again);
   }

   EXIT;
}


static void
_mongoc_write_command_update (mongoc_write_command_t       *command,
                              mongoc_client_t              *client,
                              uint32_t                      hint,
                              const char                   *database,
                              const char                   *collection,
                              const mongoc_write_concern_t *write_concern,
                              mongoc_write_result_t        *result,
                              bson_error_t                 *error)
{
   bson_t cmd = BSON_INITIALIZER;
   bson_t reply;
   bson_t ar;
   bson_t child;
   bool ret;

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (hint);
   BSON_ASSERT (collection);

   /*
    * If we have an unacknowledged write and the server supports the legacy
    * opcodes, then submit the legacy opcode so we don't need to wait for
    * a response from the server.
    */
   if ((client->cluster.nodes [hint - 1].min_wire_version == 0) &&
       !_mongoc_write_concern_needs_gle (write_concern)) {
      _mongoc_write_command_update_legacy (command, client, hint, database,
                                           collection, write_concern, result,
                                           error);
      EXIT;
   }

   BSON_APPEND_UTF8 (&cmd, "update", collection);
   BSON_APPEND_DOCUMENT (&cmd, "writeConcern",
                         WRITE_CONCERN_DOC (write_concern));
   BSON_APPEND_BOOL (&cmd, "ordered", command->u.update.ordered);
   bson_append_array_begin (&cmd, "updates", 7, &ar);
   bson_append_document_begin (&ar, "0", 1, &child);
   BSON_APPEND_DOCUMENT (&child, "q", command->u.update.selector);
   BSON_APPEND_DOCUMENT (&child, "u", command->u.update.update);
   BSON_APPEND_BOOL (&child, "multi", command->u.update.multi);
   BSON_APPEND_BOOL (&child, "upsert", command->u.update.upsert);
   bson_append_document_end (&ar, &child);
   bson_append_array_end (&cmd, &ar);

   ret = mongoc_client_command_simple (client, database, &cmd, NULL,
                                       &reply, error);

   if (!ret) {
      result->failed = true;
   }

   _mongoc_write_result_merge (result, command, &reply);

   bson_destroy (&reply);
   bson_destroy (&cmd);

   EXIT;
}


static mongoc_write_op_t gWriteOps [2][3] = {
   { _mongoc_write_command_delete_legacy,
     _mongoc_write_command_insert_legacy,
     _mongoc_write_command_update_legacy },
   { _mongoc_write_command_delete,
     _mongoc_write_command_insert,
     _mongoc_write_command_update },
};


void
_mongoc_write_command_execute (mongoc_write_command_t       *command,       /* IN */
                               mongoc_client_t              *client,        /* IN */
                               uint32_t                      hint,          /* IN */
                               const char                   *database,      /* IN */
                               const char                   *collection,    /* IN */
                               const mongoc_write_concern_t *write_concern, /* IN */
                               mongoc_write_result_t        *result)        /* OUT */
{
   mongoc_cluster_node_t *node;
   int mode = 0;

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (collection);
   BSON_ASSERT (result);

   if (!write_concern) {
      write_concern = client->write_concern;
   }

   if (!hint) {
      hint = _mongoc_client_preselect (client, MONGOC_OPCODE_INSERT,
                                       write_concern, NULL, &result->error);
      if (!hint) {
         result->failed = true;
         EXIT;
      }
   }

   command->hint = hint;

   node = &client->cluster.nodes [hint - 1];
   mode = SUPPORTS_WRITE_COMMANDS (node);

   gWriteOps [mode][command->type] (command, client, hint, database,
                                    collection, write_concern, result,
                                    &result->error);

   EXIT;
}


void
_mongoc_write_command_destroy (mongoc_write_command_t *command)
{
   ENTRY;

   if (command) {
      switch (command->type) {
      case MONGOC_WRITE_COMMAND_DELETE:
         bson_destroy (command->u.delete.selector);
         break;
      case MONGOC_WRITE_COMMAND_INSERT:
         bson_destroy (command->u.insert.documents);
         break;
      case MONGOC_WRITE_COMMAND_UPDATE:
         bson_destroy (command->u.update.selector);
         bson_destroy (command->u.update.update);
         break;
      default:
         BSON_ASSERT (false);
         break;
      }
   }

   EXIT;
}


void
_mongoc_write_result_init (mongoc_write_result_t *result) /* IN */
{
   ENTRY;

   BSON_ASSERT (result);

   memset (result, 0, sizeof *result);

   bson_init (&result->upserted);
   bson_init (&result->writeConcernErrors);
   bson_init (&result->writeErrors);

   EXIT;
}


void
_mongoc_write_result_destroy (mongoc_write_result_t *result)
{
   ENTRY;

   BSON_ASSERT (result);

   bson_destroy (&result->upserted);
   bson_destroy (&result->writeConcernErrors);
   bson_destroy (&result->writeErrors);

   EXIT;
}


static void
_mongoc_write_result_append_upsert (mongoc_write_result_t *result,
                                    int32_t                idx,
                                    const bson_value_t    *value)
{
   bson_t child;
   const char *keyptr = NULL;
   char key[12];
   int len;

   BSON_ASSERT (result);
   BSON_ASSERT (value);

   len = bson_uint32_to_string (result->upsert_append_count, &keyptr, key,
                                sizeof key);

   bson_append_document_begin (&result->upserted, keyptr, len, &child);
   BSON_APPEND_INT32 (&child, "index", idx);
   BSON_APPEND_VALUE (&child, "_id", value);
   bson_append_document_end (&result->upserted, &child);

   result->upsert_append_count++;
}


void
_mongoc_write_result_merge_legacy (mongoc_write_result_t  *result, /* IN */
                                   mongoc_write_command_t *command, /* IN */
                                   const bson_t           *reply)  /* IN */
{
   const bson_value_t *value;
   bson_t holder, write_errors, child;
   bson_iter_t iter;
   bson_iter_t ar;
   bson_iter_t citer;
   const char *err = NULL;
   int32_t code = 0;
   int32_t n = 0;

   ENTRY;

   BSON_ASSERT (result);
   BSON_ASSERT (reply);

   if (bson_iter_init_find (&iter, reply, "n") &&
       BSON_ITER_HOLDS_INT32 (&iter)) {
      n = bson_iter_int32 (&iter);
   }

   if (bson_iter_init_find (&iter, reply, "err") &&
       BSON_ITER_HOLDS_UTF8 (&iter)) {
      err = bson_iter_utf8 (&iter, NULL);
   }

   if (bson_iter_init_find (&iter, reply, "code") &&
       BSON_ITER_HOLDS_INT32 (&iter)) {
      code = bson_iter_int32 (&iter);
   }

   if (code && err) {
      bson_set_error (&result->error,
                      MONGOC_ERROR_COLLECTION,
                      code,
                      "%s", err);
      result->failed = true;

      bson_init(&holder);
      bson_append_array_begin(&holder, "0", 1, &write_errors);
      bson_append_document_begin(&write_errors, "0", 1, &child);
      bson_append_int32(&child, "index", 5, 0);
      bson_append_int32(&child, "code", 4, code);
      bson_append_utf8(&child, "errmsg", 6, err, -1);
      bson_append_document_end(&write_errors, &child);
      bson_append_array_end(&holder, &write_errors);
      bson_iter_init(&iter, &holder);
      bson_iter_next(&iter);

      _mongoc_write_result_merge_arrays (result, &result->writeErrors, &iter);

      bson_destroy(&holder);
   }

   switch (command->type) {
   case MONGOC_WRITE_COMMAND_INSERT:
      if (n) {
         result->nInserted += n;
      }
      break;
   case MONGOC_WRITE_COMMAND_DELETE:
      result->nRemoved += n;
      break;
   case MONGOC_WRITE_COMMAND_UPDATE:
      if (bson_iter_init_find (&iter, reply, "upserted") &&
          BSON_ITER_HOLDS_OID (&iter)) {
         result->nUpserted += 1;
         value = bson_iter_value (&iter);
         _mongoc_write_result_append_upsert (result, result->n_commands, value);
      } else if (bson_iter_init_find (&iter, reply, "upserted") &&
                 BSON_ITER_HOLDS_ARRAY (&iter)) {
         result->nUpserted += n;
         if (bson_iter_recurse (&iter, &ar)) {
            while (bson_iter_next (&ar)) {
               if (BSON_ITER_HOLDS_DOCUMENT (&ar) &&
                   bson_iter_recurse (&ar, &citer) &&
                   bson_iter_find (&citer, "_id")) {
                  value = bson_iter_value (&citer);
                  _mongoc_write_result_append_upsert (result,
                                                      result->n_commands,
                                                      value);
               }
            }
         }
      } else if ((n == 1) &&
                 bson_iter_init_find (&iter, reply, "updatedExisting") &&
                 BSON_ITER_HOLDS_BOOL (&iter) &&
                 !bson_iter_bool (&iter)) {
         /*
          * CDRIVER-372:
          *
          * Versions of MongoDB before 2.6 don't return the _id for an
          * upsert if _id is not an ObjectId.
          */
         result->nUpserted += 1;
         if (bson_iter_init_find (&iter, command->u.update.update, "_id") ||
             bson_iter_init_find (&iter, command->u.update.selector, "_id")) {
            value = bson_iter_value (&iter);
            _mongoc_write_result_append_upsert (result, result->n_commands,
                                                value);
         }
      } else {
         result->nMatched += n;
      }
      break;
   default:
      break;
   }

   result->omit_nModified = true;

   switch (command->type) {
   case MONGOC_WRITE_COMMAND_DELETE:
   case MONGOC_WRITE_COMMAND_UPDATE:
      result->offset += 1;
      break;
   case MONGOC_WRITE_COMMAND_INSERT:
      result->offset += command->u.insert.current_n_documents;
      break;
   default:
      break;
   }

   result->n_commands++;

   if (command->type == MONGOC_WRITE_COMMAND_INSERT) {
      result->n_commands += command->u.insert.n_merged;
   }

   EXIT;
}


static int32_t
_mongoc_write_result_merge_arrays (mongoc_write_result_t *result, /* IN */
                                   bson_t                *dest,   /* IN */
                                   bson_iter_t           *iter)   /* IN */
{
   const bson_value_t *value;
   bson_iter_t ar;
   bson_iter_t citer;
   int32_t idx;
   int32_t count = 0;
   int32_t aridx;
   bson_t child;
   const char *keyptr = NULL;
   char key[12];
   int len;

   ENTRY;

   BSON_ASSERT (result);
   BSON_ASSERT (dest);
   BSON_ASSERT (iter);
   BSON_ASSERT (BSON_ITER_HOLDS_ARRAY (iter));

   aridx = bson_count_keys (dest);

   if (bson_iter_recurse (iter, &ar)) {
      while (bson_iter_next (&ar)) {
         if (BSON_ITER_HOLDS_DOCUMENT (&ar) &&
             bson_iter_recurse (&ar, &citer)) {
            len = bson_uint32_to_string (aridx++, &keyptr, key, sizeof key);
            bson_append_document_begin (dest, keyptr, len, &child);
            while (bson_iter_next (&citer)) {
               if (BSON_ITER_IS_KEY (&citer, "index")) {
                  idx = bson_iter_int32 (&citer) + result->offset;
                  BSON_APPEND_INT32 (&child, "index", idx);
               } else {
                  value = bson_iter_value (&citer);
                  BSON_APPEND_VALUE (&child, bson_iter_key (&citer), value);
               }
            }
            bson_append_document_end (dest, &child);
            count++;
         }
      }
   }

   RETURN (count);
}


void
_mongoc_write_result_merge (mongoc_write_result_t  *result,  /* IN */
                            mongoc_write_command_t *command, /* IN */
                            const bson_t           *reply)   /* IN */
{
   const bson_value_t *value;
   bson_iter_t iter;
   bson_iter_t citer;
   bson_iter_t ar;
   int32_t n_upserted = 0;
   int32_t affected = 0;

   ENTRY;

   BSON_ASSERT (result);
   BSON_ASSERT (reply);

   if (bson_iter_init_find (&iter, reply, "n") &&
       BSON_ITER_HOLDS_INT32 (&iter)) {
      affected = bson_iter_int32 (&iter);
   }

   if (bson_iter_init_find (&iter, reply, "writeErrors") &&
       BSON_ITER_HOLDS_ARRAY (&iter) &&
       bson_iter_recurse (&iter, &citer) &&
       bson_iter_next (&citer)) {
      result->failed = true;
   }

   switch (command->type) {
   case MONGOC_WRITE_COMMAND_INSERT:
      result->nInserted += affected;
      break;
   case MONGOC_WRITE_COMMAND_DELETE:
      result->nRemoved += affected;
      break;
   case MONGOC_WRITE_COMMAND_UPDATE:
      if (bson_iter_init_find (&iter, reply, "upserted")) {
         if (BSON_ITER_HOLDS_ARRAY (&iter)) {
            if (bson_iter_recurse (&iter, &ar)) {
               while (bson_iter_next (&ar)) {
                  if (BSON_ITER_HOLDS_DOCUMENT (&ar) &&
                      bson_iter_recurse (&ar, &citer) &&
                      bson_iter_find (&citer, "_id")) {
                     value = bson_iter_value (&citer);
                     _mongoc_write_result_append_upsert (result,
                                                         result->n_commands,
                                                         value);
                     n_upserted++;
                  }
               }
            }
         } else {
            value = bson_iter_value (&iter);
            _mongoc_write_result_append_upsert (result, result->n_commands, value);
            n_upserted = 1;
         }
         result->nUpserted += n_upserted;
         /*
          * XXX: The following addition to nMatched needs some checking.
          *      I'm highly skeptical of it.
          */
         result->nMatched += MAX (0, (affected - n_upserted));
      } else {
         result->nMatched += affected;
      }
      /*
       * SERVER-13001 - in a mixed sharded cluster a call to update could
       * return nModified (>= 2.6) or not (<= 2.4).  If any call does not
       * return nModified we can't report a valid final count so omit the
       * field completely.
       */
      if (bson_iter_init_find (&iter, reply, "nModified") &&
          BSON_ITER_HOLDS_INT32 (&iter)) {
         result->nModified += bson_iter_int32 (&iter);
      } else {
         /*
          * nModified could be BSON_TYPE_NULL, which should also be omitted.
          */
         result->omit_nModified = true;
      }
      break;
   default:
      BSON_ASSERT (false);
      break;
   }

   if (bson_iter_init_find (&iter, reply, "writeErrors") &&
       BSON_ITER_HOLDS_ARRAY (&iter)) {
      _mongoc_write_result_merge_arrays (result, &result->writeErrors, &iter);
   }

   if (bson_iter_init_find (&iter, reply, "writeConcernErrors") &&
       BSON_ITER_HOLDS_ARRAY (&iter)) {
      _mongoc_write_result_merge_arrays (result, &result->writeConcernErrors,
                                         &iter);
   }

   switch (command->type) {
   case MONGOC_WRITE_COMMAND_DELETE:
   case MONGOC_WRITE_COMMAND_UPDATE:
      result->offset += affected;
      break;
   case MONGOC_WRITE_COMMAND_INSERT:
      result->offset += command->u.insert.current_n_documents;
      break;
   default:
      break;
   }

   result->n_commands++;

   if (command->type == MONGOC_WRITE_COMMAND_INSERT) {
      result->n_commands += command->u.insert.n_merged;
   }

   EXIT;
}


bool
_mongoc_write_result_complete (mongoc_write_result_t *result,
                               bson_t                *bson,
                               bson_error_t          *error)
{
   bson_iter_t iter;
   bson_iter_t citer;
   const char *err = NULL;
   uint32_t code = 0;
   bool ret;

   ENTRY;

   BSON_ASSERT (result);

   ret = (!result->failed &&
          bson_empty0 (&result->writeConcernErrors) &&
          bson_empty0 (&result->writeErrors));

   if (bson) {
      BSON_APPEND_INT32 (bson, "nInserted", result->nInserted);
      BSON_APPEND_INT32 (bson, "nMatched", result->nMatched);
      if (!result->omit_nModified) {
         BSON_APPEND_INT32 (bson, "nModified", result->nModified);
      }
      BSON_APPEND_INT32 (bson, "nRemoved", result->nRemoved);
      BSON_APPEND_INT32 (bson, "nUpserted", result->nUpserted);
      if (!bson_empty0 (&result->upserted)) {
         BSON_APPEND_ARRAY (bson, "upserted", &result->upserted);
      }
      BSON_APPEND_ARRAY (bson, "writeErrors", &result->writeErrors);
      if (!bson_empty0 (&result->writeConcernErrors)) {
         BSON_APPEND_ARRAY (bson, "writeConcernErrors",
                            &result->writeConcernErrors);
      }
   }

   if (error) {
      memcpy (error, &result->error, sizeof *error);
   }

   if (!ret &&
       !bson_empty0 (&result->writeErrors) &&
       bson_iter_init (&iter, &result->writeErrors) &&
       bson_iter_next (&iter) &&
       BSON_ITER_HOLDS_DOCUMENT (&iter) &&
       bson_iter_recurse (&iter, &citer)) {
      while (bson_iter_next (&citer)) {
         if (BSON_ITER_IS_KEY (&citer, "errmsg")) {
            err = bson_iter_utf8 (&citer, NULL);
         } else if (BSON_ITER_IS_KEY (&citer, "code")) {
            code = bson_iter_int32 (&citer);
         }
      }
      if (err && code) {
         bson_set_error (error, MONGOC_ERROR_COMMAND, code, "%s", err);
      }
   }

   RETURN (ret);
}

/*==============================================================*/
/* --- mongoc-write-concern.c */

static BSON_INLINE bool
_mongoc_write_concern_warn_frozen (mongoc_write_concern_t *write_concern)
{
   if (write_concern->frozen) {
      MONGOC_WARNING("Cannot modify a frozen write-concern.");
   }

   return write_concern->frozen;
}

static void
_mongoc_write_concern_freeze (mongoc_write_concern_t *write_concern);


/**
 * mongoc_write_concern_new:
 *
 * Create a new mongoc_write_concern_t.
 *
 * Returns: A newly allocated mongoc_write_concern_t. This should be freed
 *    with mongoc_write_concern_destroy().
 */
mongoc_write_concern_t *
mongoc_write_concern_new (void)
{
   mongoc_write_concern_t *write_concern;

   write_concern = bson_malloc0(sizeof *write_concern);
   write_concern->w = -2;

   return write_concern;
}


mongoc_write_concern_t *
mongoc_write_concern_copy (const mongoc_write_concern_t *write_concern)
{
   mongoc_write_concern_t *ret = NULL;

   if (write_concern) {
      ret = mongoc_write_concern_new();
      ret->fsync_ = write_concern->fsync_;
      ret->journal = write_concern->journal;
      ret->w = write_concern->w;
      ret->wtimeout = write_concern->wtimeout;
      ret->frozen = false;
      ret->wtag = bson_strdup (write_concern->wtag);
   }

   return ret;
}


/**
 * mongoc_write_concern_destroy:
 * @write_concern: A mongoc_write_concern_t.
 *
 * Releases a mongoc_write_concern_t and all associated memory.
 */
void
mongoc_write_concern_destroy (mongoc_write_concern_t *write_concern)
{
   if (write_concern) {
      if (write_concern->compiled.len) {
         bson_destroy (&write_concern->compiled);
         bson_destroy (&write_concern->compiled_gle);
      }

      bson_free (write_concern->wtag);
      bson_free (write_concern);
   }
}


bool
mongoc_write_concern_get_fsync (const mongoc_write_concern_t *write_concern)
{
   bson_return_val_if_fail(write_concern, false);
   return write_concern->fsync_;
}


/**
 * mongoc_write_concern_set_fsync:
 * @write_concern: A mongoc_write_concern_t.
 * @fsync_: If the write concern requires fsync() by the server.
 *
 * Set if fsync() should be called on the server before acknowledging a
 * write request.
 */
void
mongoc_write_concern_set_fsync (mongoc_write_concern_t *write_concern,
                                bool             fsync_)
{
   bson_return_if_fail(write_concern);

   if (!_mongoc_write_concern_warn_frozen(write_concern)) {
      write_concern->fsync_ = fsync_;
   }
}


bool
mongoc_write_concern_get_journal (const mongoc_write_concern_t *write_concern)
{
   bson_return_val_if_fail(write_concern, false);
   return write_concern->journal;
}


/**
 * mongoc_write_concern_set_journal:
 * @write_concern: A mongoc_write_concern_t.
 * @journal: If the write should be journaled.
 *
 * Set if the write request should be journaled before acknowledging the
 * write request.
 */
void
mongoc_write_concern_set_journal (mongoc_write_concern_t *write_concern,
                                  bool             journal)
{
   bson_return_if_fail(write_concern);

   if (!_mongoc_write_concern_warn_frozen(write_concern)) {
      write_concern->journal = journal;
   }
}


int32_t
mongoc_write_concern_get_w (const mongoc_write_concern_t *write_concern)
{
   bson_return_val_if_fail(write_concern, MONGOC_WRITE_CONCERN_W_DEFAULT);
   return write_concern->w;
}


/**
 * mongoc_write_concern_set_w:
 * @w: The number of nodes for write or -1 for "majority".
 *
 * Sets the number of nodes that must acknowledge the write request before
 * acknowledging the write request to the client.
 *
 * You may specifiy @w as -1 to request that a "majority" of nodes
 * acknowledge the request.
 */
void
mongoc_write_concern_set_w (mongoc_write_concern_t *write_concern,
                            int32_t            w)
{
   bson_return_if_fail(write_concern);
   bson_return_if_fail(w >= -3);

   if (!_mongoc_write_concern_warn_frozen(write_concern)) {
      write_concern->w = w;
   }
}


int32_t
mongoc_write_concern_get_wtimeout (const mongoc_write_concern_t *write_concern)
{
   bson_return_val_if_fail(write_concern, 0);
   return write_concern->wtimeout;
}


/**
 * mongoc_write_concern_set_wtimeout:
 * @write_concern: A mongoc_write_concern_t.
 * @wtimeout_msec: Number of milliseconds before timeout.
 *
 * Sets the number of milliseconds to wait before considering a write
 * request as failed.
 */
void
mongoc_write_concern_set_wtimeout (mongoc_write_concern_t *write_concern,
                                   int32_t            wtimeout_msec)
{
   bson_return_if_fail(write_concern);

   if (!_mongoc_write_concern_warn_frozen(write_concern)) {
      write_concern->wtimeout = wtimeout_msec;
   }
}


bool
mongoc_write_concern_get_wmajority (const mongoc_write_concern_t *write_concern)
{
   bson_return_val_if_fail(write_concern, false);
   return (write_concern->w == -3);
}


/**
 * mongoc_write_concern_set_wmajority:
 * @write_concern: A mongoc_write_concern_t.
 * @wtimeout_msec: Number of milliseconds before timeout.
 *
 * Sets the "w" of a write concern to "majority". It is suggested that
 * you provide a reasonable @wtimeout_msec to wait before considering the
 * write request failed.
 */
void
mongoc_write_concern_set_wmajority (mongoc_write_concern_t *write_concern,
                                    int32_t                 wtimeout_msec)
{
   bson_return_if_fail(write_concern);

   if (!_mongoc_write_concern_warn_frozen(write_concern)) {
      write_concern->w = MONGOC_WRITE_CONCERN_W_MAJORITY;
      write_concern->wtimeout = wtimeout_msec;
   }
}


const char *
mongoc_write_concern_get_wtag (const mongoc_write_concern_t *write_concern)
{
   bson_return_val_if_fail (write_concern, NULL);

   if (write_concern->w == MONGOC_WRITE_CONCERN_W_TAG) {
      return write_concern->wtag;
   }

   return NULL;
}


void
mongoc_write_concern_set_wtag (mongoc_write_concern_t *write_concern,
                               const char             *wtag)
{
   bson_return_if_fail (write_concern);

   if (!_mongoc_write_concern_warn_frozen (write_concern)) {
      bson_free (write_concern->wtag);
      write_concern->wtag = bson_strdup (wtag);
      write_concern->w = MONGOC_WRITE_CONCERN_W_TAG;
   }
}

/**
 * mongoc_write_concern_get_bson:
 * @write_concern: A mongoc_write_concern_t.
 *
 * This is an internal function.
 *
 * Freeze the write concern if necessary and retrieve the encoded bson_t
 * representing the write concern.
 *
 * You may not modify the write concern further after calling this function.
 *
 * Returns: A bson_t that should not be modified or freed as it is owned by
 *    the mongoc_write_concern_t instance.
 */
const bson_t *
_mongoc_write_concern_get_bson (mongoc_write_concern_t *write_concern) {
   if (!write_concern->frozen) {
       _mongoc_write_concern_freeze(write_concern);
   }

   return &write_concern->compiled;
}

/**
 * mongoc_write_concern_get_gle:
 * @write_concern: A mongoc_write_concern_t.
 *
 * This is an internal function.
 *
 * Freeze the write concern if necessary and retrieve the encoded bson_t
 * representing the write concern as a get last error command.
 *
 * You may not modify the write concern further after calling this function.
 *
 * Returns: A bson_t that should not be modified or freed as it is owned by
 *    the mongoc_write_concern_t instance.
 */
const bson_t *
_mongoc_write_concern_get_gle (mongoc_write_concern_t *write_concern) {
   if (!write_concern->frozen) {
       _mongoc_write_concern_freeze(write_concern);
   }

   return &write_concern->compiled_gle;
}

/**
 * mongoc_write_concern_freeze:
 * @write_concern: A mongoc_write_concern_t.
 *
 * This is an internal function.
 *
 * Freeze the write concern if necessary and encode it into a bson_ts which
 * represent the raw bson form and the get last error command form.
 *
 * You may not modify the write concern further after calling this function.
 */
static void
_mongoc_write_concern_freeze (mongoc_write_concern_t *write_concern)
{
   bson_t *compiled;
   bson_t *compiled_gle;

   bson_return_if_fail(write_concern);

   compiled = &write_concern->compiled;
   compiled_gle = &write_concern->compiled_gle;

   write_concern->frozen = true;

   bson_init (compiled);
   bson_init (compiled_gle);

   if (write_concern->w == MONGOC_WRITE_CONCERN_W_TAG) {
      BSON_ASSERT (write_concern->wtag);
      BSON_APPEND_UTF8 (compiled, "w", write_concern->wtag);
   } else if (write_concern->w == MONGOC_WRITE_CONCERN_W_MAJORITY) {
      BSON_APPEND_UTF8 (compiled, "w", "majority");
   } else if (write_concern->w == MONGOC_WRITE_CONCERN_W_DEFAULT) {
      /* Do Nothing */
   } else if (write_concern->w > 0) {
      BSON_APPEND_INT32 (compiled, "w", write_concern->w);
   }

   if (write_concern->fsync_) {
      bson_append_bool(compiled, "fsync", 5, true);
   }

   if (write_concern->journal) {
      bson_append_bool(compiled, "j", 1, true);
   }

   if (write_concern->wtimeout) {
      bson_append_int32(compiled, "wtimeout", 8, write_concern->wtimeout);
   }

   BSON_APPEND_INT32 (compiled_gle, "getlasterror", 1);
   bson_concat (compiled_gle, compiled);
}


/**
 * mongoc_write_concern_needs_gle:
 * @concern: (in): A mongoc_write_concern_t.
 *
 * Checks to see if @write_concern requests that a getlasterror command is to
 * be delivered to the MongoDB server.
 *
 * Returns: true if a getlasterror command should be sent.
 */
bool
_mongoc_write_concern_needs_gle (const mongoc_write_concern_t *write_concern)
{
   if (write_concern) {
      return ((write_concern->w != 0) && (write_concern->w != -1));
   }
   return false;
}

/*==============================================================*/

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

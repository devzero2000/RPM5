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

#include <bcon.h>
#include <mongoc.h>

#ifdef HAVE_LIBSASL2
#include <sasl/sasl.h>
#include <sasl/saslutil.h>
#endif

#ifdef MONGOC_ENABLE_OPENSSL
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include <openssl/crypto.h>
#include <openssl/rand.h>

#ifdef MONGOC_ENABLE_LIBCRYPTO
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#endif	/* MONGOC_ENABLE_LIBCRYPTO */
#endif	/* MONGOC_ENABLE_OPENSSL */

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
   BSON_ASSERT (array);
   BSON_ASSERT (element_size);

   array->len = 0;
   array->element_size = element_size;
   array->allocated = 128;
   array->data = (void *)bson_malloc0(array->allocated);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_array_copy --
 *
 *       Destroy dst and copy src into it. Both arrays must be initialized.
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
_mongoc_array_copy (mongoc_array_t       *dst,
                    const mongoc_array_t *src)
{
   _mongoc_array_destroy (dst);

   dst->len = src->len;
   dst->element_size = src->element_size;
   dst->allocated = src->allocated;
   dst->data = (void *)bson_malloc (dst->allocated);
   memcpy (dst->data, src->data, dst->allocated);
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

   BSON_ASSERT (array);
   BSON_ASSERT (data);

   off = array->element_size * array->len;
   len = (size_t)n_elements * array->element_size;
   if ((off + len) > array->allocated) {
      next_size = bson_next_power_of_two(off + len);
      array->data = (void *)bson_realloc(array->data, next_size);
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
   BSON_ASSERT (buffer);
   BSON_ASSERT (buflen || !buf);

   if (!realloc_func) {
      realloc_func = bson_realloc_ctx;
   }

   if (!buflen) {
      buflen = MONGOC_BUFFER_DEFAULT_SIZE;
   }

   if (!buf) {
      buf = (uint8_t *)realloc_func (NULL, buflen, NULL);
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
   BSON_ASSERT (buffer);

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
   BSON_ASSERT (buffer);

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

   BSON_ASSERT (buffer);
   BSON_ASSERT (stream);
   BSON_ASSERT (size);

   BSON_ASSERT (buffer->datalen);
   BSON_ASSERT ((buffer->datalen + size) < INT_MAX);

   if (!SPACE_FOR (buffer, size)) {
      if (buffer->len) {
         memmove(&buffer->data[0], &buffer->data[buffer->off], buffer->len);
      }
      buffer->off = 0;
      if (!SPACE_FOR (buffer, size)) {
         buffer->datalen = bson_next_power_of_two (size + buffer->len + buffer->off);
         buffer->data = (uint8_t *)buffer->realloc_func (buffer->data, buffer->datalen, NULL);
      }
   }

   buf = &buffer->data[buffer->off + buffer->len];

   BSON_ASSERT ((buffer->off + buffer->len + size) <= buffer->datalen);

   ret = mongoc_stream_read (stream, buf, size, size, timeout_msec);
   if (ret != (ssize_t)size) {
      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_SOCKET,
                      _("Failed to read %"PRIu64" bytes from socket within %d milliseconds."),
                      (uint64_t)size, (int)timeout_msec);
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

   BSON_ASSERT (buffer);
   BSON_ASSERT (stream);

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
      buffer->data = (uint8_t *)buffer->realloc_func (buffer->data, buffer->datalen,
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
                      _("Failed to buffer %u bytes within %d milliseconds."),
                      (unsigned)min_bytes, (int)timeout_msec);
      RETURN (-1);
   }

   buffer->len += ret;

   if (buffer->len < min_bytes) {
      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_SOCKET,
                      _("Could only buffer %u of %u bytes in %d milliseconds."),
                      (unsigned)buffer->len,
                      (unsigned)min_bytes,
                      (int)timeout_msec);
      RETURN (-1);
   }

   RETURN (buffer->len);
}


/**
 * mongoc_buffer_try_append_from_stream:
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
 * Returns: bytes read if successful; otherwise -1 and @error is set.
 */
ssize_t
_mongoc_buffer_try_append_from_stream (mongoc_buffer_t *buffer,
                                   mongoc_stream_t *stream,
                                   size_t           size,
                                   int32_t          timeout_msec,
                                   bson_error_t    *error)
{
   uint8_t *buf;
   ssize_t ret;

   ENTRY;

   BSON_ASSERT (buffer);
   BSON_ASSERT (stream);
   BSON_ASSERT (size);

   BSON_ASSERT (buffer->datalen);
   BSON_ASSERT ((buffer->datalen + size) < INT_MAX);

   if (!SPACE_FOR (buffer, size)) {
      if (buffer->len) {
         memmove(&buffer->data[0], &buffer->data[buffer->off], buffer->len);
      }
      buffer->off = 0;
      if (!SPACE_FOR (buffer, size)) {
         buffer->datalen = bson_next_power_of_two (size + buffer->len + buffer->off);
         buffer->data = (uint8_t *)buffer->realloc_func (buffer->data, buffer->datalen, NULL);
      }
   }

   buf = &buffer->data[buffer->off + buffer->len];

   BSON_ASSERT ((buffer->off + buffer->len + size) <= buffer->datalen);

   ret = mongoc_stream_read (stream, buf, size, 0, timeout_msec);

   if (ret > 0) {
      buffer->len += ret;
   }

   RETURN (ret);
}



/*==============================================================*/
/* --- mongoc-rpc.c */

#define RPC(_name, _code) \
   static void \
   _mongoc_rpc_gather_##_name (mongoc_rpc_##_name##_t *rpc, \
                               mongoc_array_t *array) \
   { \
      mongoc_iovec_t iov; \
      assert (rpc); \
      assert (array); \
      rpc->msg_len = 0; \
      _code \
   }
#define INT32_FIELD(_name) \
   iov.iov_base = (void *)&rpc->_name; \
   iov.iov_len = 4; \
   assert (iov.iov_len); \
   rpc->msg_len += (int32_t)iov.iov_len; \
   _mongoc_array_append_val(array, iov);
#define ENUM_FIELD INT32_FIELD
#define INT64_FIELD(_name) \
   iov.iov_base = (void *)&rpc->_name; \
   iov.iov_len = 8; \
   assert (iov.iov_len); \
   rpc->msg_len += (int32_t)iov.iov_len; \
   _mongoc_array_append_val(array, iov);
#define CSTRING_FIELD(_name) \
   assert (rpc->_name); \
   iov.iov_base = (void *)rpc->_name; \
   iov.iov_len = strlen(rpc->_name) + 1; \
   assert (iov.iov_len); \
   rpc->msg_len += (int32_t)iov.iov_len; \
   _mongoc_array_append_val(array, iov);
#define BSON_FIELD(_name) \
   do { \
      int32_t __l; \
      memcpy(&__l, rpc->_name, 4); \
      __l = BSON_UINT32_FROM_LE(__l); \
      iov.iov_base = (void *)rpc->_name; \
      iov.iov_len = __l; \
      assert (iov.iov_len); \
      rpc->msg_len += (int32_t)iov.iov_len; \
      _mongoc_array_append_val(array, iov); \
   } while (0);
#define BSON_OPTIONAL(_check, _code) \
   if (rpc->_check) { _code }
#define BSON_ARRAY_FIELD(_name) \
   if (rpc->_name##_len) { \
      iov.iov_base = (void *)rpc->_name; \
      iov.iov_len = rpc->_name##_len; \
      assert (iov.iov_len); \
      rpc->msg_len += (int32_t)iov.iov_len; \
      _mongoc_array_append_val(array, iov); \
   }
#define IOVEC_ARRAY_FIELD(_name) \
   do { \
      ssize_t _i; \
      assert (rpc->n_##_name); \
      for (_i = 0; _i < rpc->n_##_name; _i++) { \
         assert (rpc->_name[_i].iov_len); \
         rpc->msg_len += (int32_t)rpc->_name[_i].iov_len; \
         _mongoc_array_append_val(array, rpc->_name[_i]); \
      } \
   } while (0);
#define RAW_BUFFER_FIELD(_name) \
   iov.iov_base = (void *)rpc->_name; \
   iov.iov_len = rpc->_name##_len; \
   assert (iov.iov_len); \
   rpc->msg_len += (int32_t)iov.iov_len; \
   _mongoc_array_append_val(array, iov);
#define INT64_ARRAY_FIELD(_len, _name) \
   iov.iov_base = (void *)&rpc->_len; \
   iov.iov_len = 4; \
   assert (iov.iov_len); \
   rpc->msg_len += (int32_t)iov.iov_len; \
   _mongoc_array_append_val(array, iov); \
   iov.iov_base = (void *)rpc->_name; \
   iov.iov_len = rpc->_len * 8; \
   assert (iov.iov_len); \
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
  ENUM_FIELD(flags)
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
  ENUM_FIELD(flags)
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
  ENUM_FIELD(flags)
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
  ENUM_FIELD(flags)
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
  ENUM_FIELD(flags)
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
      assert (rpc); \
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
  ENUM_FIELD(flags)
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
  ENUM_FIELD(flags)
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
  ENUM_FIELD(flags)
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
  ENUM_FIELD(flags)
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
  ENUM_FIELD(flags)
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
      assert (rpc); \
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
  ENUM_FIELD(flags)
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
  ENUM_FIELD(flags)
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
  ENUM_FIELD(flags)
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
  ENUM_FIELD(flags)
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
  ENUM_FIELD(flags)
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
      assert (rpc); \
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
  ENUM_FIELD(flags)
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
  ENUM_FIELD(flags)
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
  ENUM_FIELD(flags)
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
  ENUM_FIELD(flags)
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
  ENUM_FIELD(flags)
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
      assert (rpc); \
      assert (buf); \
      assert (buflen); \
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
      rpc->_name = (int64_t*)buf; \
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
  ENUM_FIELD(flags)
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
  ENUM_FIELD(flags)
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
  ENUM_FIELD(flags)
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
  ENUM_FIELD(flags)
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
  ENUM_FIELD(flags)
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
      _mongoc_rpc_gather_delete(&rpc->delete_, array);
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
      _mongoc_rpc_swab_to_le_delete(&rpc->delete_);
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
      _mongoc_rpc_swab_from_le_delete(&rpc->delete_);
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
      _mongoc_rpc_printf_delete(&rpc->delete_);
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

   memset (rpc, 0, sizeof *rpc);

   if (BSON_UNLIKELY(buflen < 16)) {
      return false;
   }

   if (!_mongoc_rpc_scatter_header(&rpc->header, buf, 16)) {
      return false;
   }

   opcode = (mongoc_opcode_t)BSON_UINT32_FROM_LE(rpc->header.opcode);

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
      return _mongoc_rpc_scatter_delete(&rpc->delete_, buf, buflen);
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

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_rpc_prep_command --
 *
 *       Prepare an RPC for mongoc_cluster_run_command_rpc. @cmd_ns and
 *       @command must not be freed or modified while the RPC is in use.
 *
 * Side effects:
 *       Fills out the RPC, including pointers into @cmd_ns and @command.
 *
 *--------------------------------------------------------------------------
 */

void
_mongoc_rpc_prep_command (mongoc_rpc_t        *rpc,
                          const char          *cmd_ns,
                          const bson_t        *command,
                          mongoc_query_flags_t flags)
{
   rpc->query.msg_len = 0;
   rpc->query.request_id = 0;
   rpc->query.response_to = 0;
   rpc->query.opcode = MONGOC_OPCODE_QUERY;
   rpc->query.collection = cmd_ns;
   rpc->query.skip = 0;
   rpc->query.n_return = -1;
   rpc->query.fields = NULL;
   rpc->query.query = bson_get_data (command);

   /* Find, getMore And killCursors Commands Spec: "When sending a find command
    * rather than a legacy OP_QUERY find, only the slaveOk flag is honored."
    * For other cursor-typed commands like aggregate, only slaveOk can be set.
    * Clear bits except slaveOk; leave slaveOk set only if it is already.
    */
   rpc->query.flags = flags & MONGOC_QUERY_SLAVE_OK;
}


static void
_mongoc_populate_error (const bson_t *doc,
                        bool          is_command,
                        bson_error_t *error)
{
   uint32_t code = MONGOC_ERROR_QUERY_FAILURE;
   bson_iter_t iter;
   const char *msg = "Unknown query failure";

   BSON_ASSERT (doc);

   if (!error) {
      return;
   }

   if (bson_iter_init_find (&iter, doc, "code") &&
       BSON_ITER_HOLDS_INT32 (&iter)) {
      code = (uint32_t) bson_iter_int32 (&iter);
   }

   if (is_command &&
       ((code == MONGOC_ERROR_PROTOCOL_ERROR) ||
        (code == 13390))) {
      code = MONGOC_ERROR_QUERY_COMMAND_NOT_FOUND;
   }

   if (bson_iter_init_find (&iter, doc, "$err") &&
       BSON_ITER_HOLDS_UTF8 (&iter)) {
      msg = bson_iter_utf8 (&iter, NULL);
   }

   if (is_command &&
       bson_iter_init_find (&iter, doc, "errmsg") &&
       BSON_ITER_HOLDS_UTF8 (&iter)) {
      msg = bson_iter_utf8 (&iter, NULL);
   }

   bson_set_error(error, MONGOC_ERROR_QUERY, code, "%s", msg);
}


static bool
_mongoc_rpc_parse_error (mongoc_rpc_t *rpc,
                         bool is_command,
                         bson_error_t *error /* OUT */)
{
   bson_iter_t iter;
   bson_t b;

   ENTRY;

   BSON_ASSERT (rpc);

   if (rpc->header.opcode != MONGOC_OPCODE_REPLY) {
      bson_set_error(error,
                     MONGOC_ERROR_PROTOCOL,
                     MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                     _("Received rpc other than OP_REPLY."));
      RETURN(true);
   }

   if ((rpc->reply.flags & MONGOC_REPLY_QUERY_FAILURE)) {
      if (_mongoc_rpc_reply_get_first(&rpc->reply, &b)) {
         _mongoc_populate_error (&b, is_command, error);
         bson_destroy(&b);
      } else {
         bson_set_error(error,
                        MONGOC_ERROR_QUERY,
                        MONGOC_ERROR_QUERY_FAILURE,
                        _("Unknown query failure."));
      }
      RETURN(true);
   } else if (is_command) {
      if (_mongoc_rpc_reply_get_first (&rpc->reply, &b)) {
         if (bson_iter_init_find (&iter, &b, "ok")) {
            if (bson_iter_as_bool (&iter)) {
               RETURN (false);
            } else {
               _mongoc_populate_error (&b, is_command, error);
               bson_destroy (&b);
               RETURN (true);
            }
         }
      } else {
         bson_set_error (error,
                         MONGOC_ERROR_BSON,
                         MONGOC_ERROR_BSON_INVALID,
                         _("Failed to decode document from the server."));
         RETURN (true);
      }
   }

   if ((rpc->reply.flags & MONGOC_REPLY_CURSOR_NOT_FOUND)) {
      bson_set_error(error,
                     MONGOC_ERROR_CURSOR,
                     MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                     _("The cursor is invalid or has expired."));
      RETURN(true);
   }

   RETURN(false);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_rpc_parse_command_error --
 *
 *       Check if a server OP_REPLY is a command error message.
 *       Optionally fill out a bson_error_t from the server error.
 *
 * Returns:
 *       true if the reply is an error message, false otherwise.
 *
 * Side effects:
 *       If rpc is an error reply and @error is not NULL, set its
 *       domain, code, and message.
 *
 *--------------------------------------------------------------------------
 */

bool _mongoc_rpc_parse_command_error (mongoc_rpc_t *rpc,
                                      bson_error_t *error)
{
   return _mongoc_rpc_parse_error (rpc, true, error);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_rpc_parse_query_error --
 *
 *       Check if a server OP_REPLY is a query error message.
 *       Optionally fill out a bson_error_t from the server error.
 *
 * Returns:
 *       true if the reply is an error message, false otherwise.
 *
 * Side effects:
 *       If rpc is an error reply and @error is not NULL, set its
 *       domain, code, and message.
 *
 *--------------------------------------------------------------------------
 */

bool _mongoc_rpc_parse_query_error (mongoc_rpc_t *rpc,
                                    bson_error_t *error)
{
   return _mongoc_rpc_parse_error (rpc, false, error);
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

   bulk = (mongoc_bulk_operation_t *)bson_malloc0 (sizeof *bulk);
   bulk->flags.bypass_document_validation = MONGOC_BYPASS_DOCUMENT_VALIDATION_DEFAULT;
   bulk->flags.ordered = ordered;
   bulk->server_id = 0;

   _mongoc_array_init (&bulk->commands, sizeof (mongoc_write_command_t));

   return bulk;
}


mongoc_bulk_operation_t *
_mongoc_bulk_operation_new (mongoc_client_t              *client,        /* IN */
                            const char                   *database,      /* IN */
                            const char                   *collection,    /* IN */
                            mongoc_bulk_write_flags_t     flags,         /* IN */
                            const mongoc_write_concern_t *write_concern) /* IN */
{
   mongoc_bulk_operation_t *bulk;

   BSON_ASSERT (client);
   BSON_ASSERT (collection);

   bulk = mongoc_bulk_operation_new (flags.ordered);
   bulk->client = client;
   bulk->database = bson_strdup (database);
   bulk->collection = bson_strdup (collection);
   bulk->write_concern = mongoc_write_concern_copy (write_concern);
   bulk->executed = false;
   bulk->flags = flags;
   bulk->operation_id = ++client->cluster.operation_id;

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
   mongoc_write_command_t *last;

   ENTRY;

   BSON_ASSERT (bulk);
   BSON_ASSERT (selector);

   if (bulk->commands.len) {
      last = &_mongoc_array_index (&bulk->commands,
                                   mongoc_write_command_t,
                                   bulk->commands.len - 1);
      if ((last->type == MONGOC_WRITE_COMMAND_DELETE) &&
          last->u.delete_.multi) {
         _mongoc_write_command_delete_append (last, selector);
         EXIT;
      }
   }

   _mongoc_write_command_init_delete (&command, selector, true, bulk->flags,
                                      bulk->operation_id);

   _mongoc_array_append_val (&bulk->commands, command);

   EXIT;
}


void
mongoc_bulk_operation_remove_one (mongoc_bulk_operation_t *bulk,     /* IN */
                                  const bson_t            *selector) /* IN */
{
   mongoc_write_command_t command = { 0 };
   mongoc_write_command_t *last;

   ENTRY;

   BSON_ASSERT (bulk);
   BSON_ASSERT (selector);

   if (bulk->commands.len) {
      last = &_mongoc_array_index (&bulk->commands,
                                   mongoc_write_command_t,
                                   bulk->commands.len - 1);
      if ((last->type == MONGOC_WRITE_COMMAND_DELETE) &&
          !last->u.delete_.multi) {
         _mongoc_write_command_delete_append (last, selector);
         EXIT;
      }
   }

   _mongoc_write_command_init_delete (&command, selector, false, bulk->flags,
                                      bulk->operation_id);

   _mongoc_array_append_val (&bulk->commands, command);

   EXIT;
}


void
mongoc_bulk_operation_delete (mongoc_bulk_operation_t *bulk,
                              const bson_t            *selector)
{
   ENTRY;

   mongoc_bulk_operation_remove (bulk, selector);

   EXIT;
}


void
mongoc_bulk_operation_delete_one (mongoc_bulk_operation_t *bulk,
                                  const bson_t            *selector)
{
   ENTRY;

   mongoc_bulk_operation_remove_one (bulk, selector);

   EXIT;
}


void
mongoc_bulk_operation_insert (mongoc_bulk_operation_t *bulk,
                              const bson_t            *document)
{
   mongoc_write_command_t command = { 0 };
   mongoc_write_command_t *last;

   ENTRY;

   BSON_ASSERT (bulk);
   BSON_ASSERT (document);

   if (bulk->commands.len) {
      last = &_mongoc_array_index (&bulk->commands,
                                   mongoc_write_command_t,
                                   bulk->commands.len - 1);

      if (last->type == MONGOC_WRITE_COMMAND_INSERT) {
         _mongoc_write_command_insert_append (last, document);
         EXIT;
      }
   }

   _mongoc_write_command_init_insert (
      &command, document, bulk->flags, bulk->operation_id,
      !mongoc_write_concern_is_acknowledged (bulk->write_concern));

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
   mongoc_write_command_t *last;
   int flags = BSON_VALIDATE_DOT_KEYS|BSON_VALIDATE_DOLLAR_KEYS;

   BSON_ASSERT (bulk);
   BSON_ASSERT (selector);
   BSON_ASSERT (document);

   ENTRY;

   if (!bson_validate (document, (bson_validate_flags_t)flags, &err_off)) {
      MONGOC_WARNING ("%s(): replacement document may not contain "
                      "$ or . in keys. Ignoring document.",
                      BSON_FUNC);
      EXIT;
   }

   if (bulk->commands.len) {
      last = &_mongoc_array_index (&bulk->commands,
                                   mongoc_write_command_t,
                                   bulk->commands.len - 1);
      if (last->type == MONGOC_WRITE_COMMAND_UPDATE) {
         _mongoc_write_command_update_append (last, selector, document, upsert, false);
         EXIT;
      }
   }

   _mongoc_write_command_init_update (&command, selector, document, upsert,
                                      false, bulk->flags, bulk->operation_id);
   _mongoc_array_append_val (&bulk->commands, command);

   EXIT;
}


void
mongoc_bulk_operation_update (mongoc_bulk_operation_t *bulk,
                              const bson_t            *selector,
                              const bson_t            *document,
                              bool                     upsert)
{
   bool multi = true;
   mongoc_write_command_t command = { 0 };
   bson_iter_t iter;
   mongoc_write_command_t *last;

   BSON_ASSERT (bulk);
   BSON_ASSERT (selector);
   BSON_ASSERT (document);

   ENTRY;

   if (bson_iter_init (&iter, document)) {
      while (bson_iter_next (&iter)) {
         if (!strchr (bson_iter_key (&iter), '$')) {
            MONGOC_WARNING ("%s(): update only works with $ operators.",
                            BSON_FUNC);
            EXIT;
         }
      }
   }

   if (bulk->commands.len) {
      last = &_mongoc_array_index (&bulk->commands,
                                   mongoc_write_command_t,
                                   bulk->commands.len - 1);
      if (last->type == MONGOC_WRITE_COMMAND_UPDATE) {
         _mongoc_write_command_update_append (last, selector, document, upsert, multi);
         EXIT;
      }
   }

   _mongoc_write_command_init_update (&command, selector, document, upsert,
                                      multi, bulk->flags, bulk->operation_id);
   _mongoc_array_append_val (&bulk->commands, command);
   EXIT;
}


void
mongoc_bulk_operation_update_one (mongoc_bulk_operation_t *bulk,
                                  const bson_t            *selector,
                                  const bson_t            *document,
                                  bool                     upsert)
{
   mongoc_write_command_t command = { 0 };
   bson_iter_t iter;
   mongoc_write_command_t *last;

   BSON_ASSERT (bulk);
   BSON_ASSERT (selector);
   BSON_ASSERT (document);

   ENTRY;

   if (bson_iter_init (&iter, document)) {
      while (bson_iter_next (&iter)) {
         if (!strchr (bson_iter_key (&iter), '$')) {
            MONGOC_WARNING ("%s(): update_one only works with $ operators.",
                            BSON_FUNC);
            EXIT;
         }
      }
   }

   if (bulk->commands.len) {
      last = &_mongoc_array_index (&bulk->commands,
                                   mongoc_write_command_t,
                                   bulk->commands.len - 1);
      if (last->type == MONGOC_WRITE_COMMAND_UPDATE) {
         _mongoc_write_command_update_append (last, selector, document, upsert, false);
         EXIT;
      }
   }

   _mongoc_write_command_init_update (&command, selector, document, upsert,
                                      false, bulk->flags, bulk->operation_id);
   _mongoc_array_append_val (&bulk->commands, command);
   EXIT;
}


uint32_t
mongoc_bulk_operation_execute (mongoc_bulk_operation_t *bulk,  /* IN */
                               bson_t                  *reply, /* OUT */
                               bson_error_t            *error) /* OUT */
{
   mongoc_cluster_t *cluster;
   mongoc_write_command_t *command;
   mongoc_server_stream_t *server_stream;
   bool ret;
   uint32_t offset = 0;
   int i;

   ENTRY;

   BSON_ASSERT (bulk);

   cluster = &bulk->client->cluster;

   if (bulk->executed) {
      _mongoc_write_result_destroy (&bulk->result);
   }

   _mongoc_write_result_init (&bulk->result);

   bulk->executed = true;

   if (!bulk->client) {
      bson_set_error (error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      _("mongoc_bulk_operation_execute() requires a client "
                      "and one has not been set."));
      RETURN (false);
   } else if (!bulk->database) {
      bson_set_error (error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      _("mongoc_bulk_operation_execute() requires a database "
                      "and one has not been set."));
      RETURN (false);
   } else if (!bulk->collection) {
      bson_set_error (error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      _("mongoc_bulk_operation_execute() requires a collection "
                      "and one has not been set."));
      RETURN (false);
   }

   if (reply) {
      bson_init (reply);
   }

   if (!bulk->commands.len) {
      bson_set_error (error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      _("Cannot do an empty bulk write"));
      RETURN (false);
   }

   if (bulk->server_id) {
      server_stream = mongoc_cluster_stream_for_server (cluster,
                                                        bulk->server_id,
                                                        true /* reconnect_ok */,
                                                        error);
   } else {
      server_stream = mongoc_cluster_stream_for_writes (cluster, error);
   }

   if (!server_stream) {
      RETURN (false);
   }

   for (i = 0; i < (int)bulk->commands.len; i++) {
      command = &_mongoc_array_index (&bulk->commands,
                                      mongoc_write_command_t, i);

      _mongoc_write_command_execute (command, bulk->client, server_stream,
                                     bulk->database, bulk->collection,
                                     bulk->write_concern, offset,
                                     &bulk->result);

      bulk->server_id = command->server_id;

      if (bulk->result.failed && bulk->flags.ordered) {
         GOTO (cleanup);
      }

      offset += command->n_documents;
   }

cleanup:
   ret = _mongoc_write_result_complete (&bulk->result, reply, error);
   mongoc_server_stream_cleanup (server_stream);

   RETURN (ret ? bulk->server_id : 0);
}

void
mongoc_bulk_operation_set_write_concern (mongoc_bulk_operation_t      *bulk,
                                         const mongoc_write_concern_t *write_concern)
{
   BSON_ASSERT (bulk);

   if (bulk->write_concern) {
      mongoc_write_concern_destroy (bulk->write_concern);
   }

   if (write_concern) {
      bulk->write_concern = mongoc_write_concern_copy (write_concern);
   } else {
      bulk->write_concern = mongoc_write_concern_new ();
   }
}

const mongoc_write_concern_t *
mongoc_bulk_operation_get_write_concern (const mongoc_bulk_operation_t *bulk)
{
   BSON_ASSERT (bulk);

   return bulk->write_concern;
}


void
mongoc_bulk_operation_set_database (mongoc_bulk_operation_t *bulk,
                                    const char              *database)
{
   BSON_ASSERT (bulk);

   if (bulk->database) {
      bson_free (bulk->database);
   }

   bulk->database = bson_strdup (database);
}


void
mongoc_bulk_operation_set_collection (mongoc_bulk_operation_t *bulk,
                                      const char              *collection)
{
   BSON_ASSERT (bulk);

   if (bulk->collection) {
      bson_free (bulk->collection);
   }

   bulk->collection = bson_strdup (collection);
}


void
mongoc_bulk_operation_set_client (mongoc_bulk_operation_t *bulk,
                                  void                    *client)
{
   BSON_ASSERT (bulk);

   bulk->client = (mongoc_client_t *)client;
}


uint32_t
mongoc_bulk_operation_get_hint (const mongoc_bulk_operation_t *bulk)
{
   BSON_ASSERT (bulk);

   return bulk->server_id;
}


void
mongoc_bulk_operation_set_hint (mongoc_bulk_operation_t *bulk,
                                uint32_t                 server_id)
{
   BSON_ASSERT (bulk);

   bulk->server_id = server_id;
}


void
mongoc_bulk_operation_set_bypass_document_validation (mongoc_bulk_operation_t *bulk,
                                                      bool                     bypass)
{
   BSON_ASSERT (bulk);

   bulk->flags.bypass_document_validation = bypass ?
      MONGOC_BYPASS_DOCUMENT_VALIDATION_TRUE :
      MONGOC_BYPASS_DOCUMENT_VALIDATION_FALSE;
}

/*==============================================================*/
/* --- mongoc-client.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "client"


static void
_mongoc_client_op_killcursors (mongoc_cluster_t       *cluster,
                               mongoc_server_stream_t *server_stream,
                               int64_t                 cursor_id,
                               int64_t                 operation_id,
                               const char             *db,
                               const char             *collection);

static void
_mongoc_client_killcursors_command (mongoc_cluster_t       *cluster,
                                    mongoc_server_stream_t *server_stream,
                                    int64_t                 cursor_id,
                                    const char             *db,
                                    const char             *collection);



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
   int32_t connecttimeoutms;
   int64_t expire_at;
   char portstr [8];
   int s;

   ENTRY;

   BSON_ASSERT (uri);
   BSON_ASSERT (host);

   connecttimeoutms = mongoc_uri_get_option_as_int32 (
      uri, "connecttimeoutms", MONGOC_DEFAULT_CONNECTTIMEOUTMS);

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
                     _("Failed to resolve %s"),
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
         char *errmsg;
         char errmsg_buf[BSON_ERROR_BUFFER_SIZE];
         char ip[255];

         mongoc_socket_inet_ntop (rp, ip, sizeof ip);
         errmsg = bson_strerror_r (
            mongoc_socket_errno (sock), errmsg_buf, sizeof errmsg_buf);
         MONGOC_WARNING ("Failed to connect to: %s:%d, error: %d, %s\n",
                         ip,
                         host->port,
                         mongoc_socket_errno(sock),
                         errmsg);
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
                      _("Failed to connect to target host: %s"),
                      host->host_and_port);
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
                   _("UNIX domain sockets not supported on win32."));
   RETURN (NULL);
#else
   struct sockaddr_un saddr;
   mongoc_socket_t *sock;
   mongoc_stream_t *ret = NULL;

   ENTRY;

   BSON_ASSERT (uri);
   BSON_ASSERT (host);

   memset (&saddr, 0, sizeof saddr);
   saddr.sun_family = AF_UNIX;
   bson_snprintf (saddr.sun_path, sizeof saddr.sun_path - 1,
                  "%s", host->host);

   sock = mongoc_socket_new (AF_UNIX, SOCK_STREAM, 0);

   if (sock == NULL) {
      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_SOCKET,
                      _("Failed to create socket."));
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
                      _("Failed to connect to UNIX domain socket."));
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

mongoc_stream_t *
mongoc_client_default_stream_initiator (const mongoc_uri_t       *uri,
                                        const mongoc_host_list_t *host,
                                        void                     *user_data,
                                        bson_error_t             *error)
{
   mongoc_stream_t *base_stream = NULL;
#ifdef MONGOC_ENABLE_SSL
   mongoc_client_t *client = (mongoc_client_t *)user_data;
   const char *mechanism;
   int32_t connecttimeoutms;
#endif

   BSON_ASSERT (uri);
   BSON_ASSERT (host);

#ifndef MONGOC_ENABLE_SSL
   if (mongoc_uri_get_ssl (uri)) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_NO_ACCEPTABLE_PEER,
                      _("SSL is not enabled in this build of mongo-c-driver."));
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
                      _("Invalid address family: 0x%02x"), host->family);
      break;
   }

#ifdef MONGOC_ENABLE_SSL
   if (base_stream) {
      mechanism = mongoc_uri_get_auth_mechanism (uri);

      if (client->use_ssl ||
          (mechanism && (0 == strcmp (mechanism, "MONGODB-X509")))) {
         base_stream = mongoc_stream_tls_new (base_stream, &client->ssl_opts,
                                              true);

         if (!base_stream) {
            bson_set_error (error,
                            MONGOC_ERROR_STREAM,
                            MONGOC_ERROR_STREAM_SOCKET,
                            _("Failed initialize TLS state."));
            return NULL;
         }

         connecttimeoutms = mongoc_uri_get_option_as_int32 (
            uri, "connecttimeoutms", MONGOC_DEFAULT_CONNECTTIMEOUTMS);

         if (!mongoc_stream_tls_do_handshake (base_stream, connecttimeoutms) ||
             !mongoc_stream_tls_check_cert (base_stream, host->host)) {
            bson_set_error (error,
                            MONGOC_ERROR_STREAM,
                            MONGOC_ERROR_STREAM_SOCKET,
                            _("Failed to handshake and validate TLS certificate."));
            mongoc_stream_destroy (base_stream);
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
   BSON_ASSERT (client);
   BSON_ASSERT (host);

   return client->initiator (client->uri, host, client->initiator_data, error);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_client_recv --
 *
 *       Receives a RPC from a remote MongoDB cluster node.
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
_mongoc_client_recv (mongoc_client_t        *client,
                     mongoc_rpc_t           *rpc,
                     mongoc_buffer_t        *buffer,
                     mongoc_server_stream_t *server_stream,
                     bson_error_t           *error)
{
   BSON_ASSERT (client);
   BSON_ASSERT (rpc);
   BSON_ASSERT (buffer);
   BSON_ASSERT (server_stream);

   if (!mongoc_cluster_try_recv (&client->cluster, rpc, buffer,
                                 server_stream, error)) {
      mongoc_topology_invalidate_server (client->topology,
                                         server_stream->sd->id, error);
      return false;
   }
   return true;
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

   BSON_ASSERT (b);

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
                  _("An unknown error ocurred on the server."));
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
 *       true if getlasterror was success; otherwise false.
 *
 * Side effects:
 *       @gle_doc will be set if non NULL and a reply was received.
 *       @error if return value is false, and @gle_doc is set to NULL.
 *
 *--------------------------------------------------------------------------
 */

bool
_mongoc_client_recv_gle (mongoc_client_t        *client,
                         mongoc_server_stream_t *server_stream,
                         bson_t                **gle_doc,
                         bson_error_t           *error)
{
   mongoc_buffer_t buffer;
   mongoc_rpc_t rpc;
   bson_iter_t iter;
   bool ret = false;
   bson_t b;

   ENTRY;

   BSON_ASSERT (client);
   BSON_ASSERT (server_stream);

   if (gle_doc) {
      *gle_doc = NULL;
   }

   _mongoc_buffer_init (&buffer, NULL, 0, NULL, NULL);

   if (!mongoc_cluster_try_recv (&client->cluster, &rpc, &buffer,
                                 server_stream, error)) {

      mongoc_topology_invalidate_server (client->topology,
                                         server_stream->sd->id, error);

      GOTO (cleanup);
   }

   if (rpc.header.opcode != MONGOC_OPCODE_REPLY) {
      bson_set_error (error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      _("Received message other than OP_REPLY."));
      GOTO (cleanup);
   }

   if (_mongoc_rpc_reply_get_first (&rpc.reply, &b)) {
      if ((rpc.reply.flags & MONGOC_REPLY_QUERY_FAILURE)) {
         _bson_to_error (&b, error);
         bson_destroy (&b);
         GOTO (cleanup);
      }

      if (gle_doc) {
         *gle_doc = bson_copy (&b);
      }

      if (!bson_iter_init_find (&iter, &b, "ok") ||
          BSON_ITER_HOLDS_DOUBLE (&iter)) {
        if (bson_iter_double (&iter) == 0.0) {
          _bson_to_error (&b, error);
        }
      }

      bson_destroy (&b);
      ret = true;
   }

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
mongoc_client_new(const char *uri_string)
{
   mongoc_topology_t *topology;
   mongoc_client_t   *client;
   mongoc_uri_t      *uri;


   if (!uri_string) {
      uri_string = "mongodb://127.0.0.1/";
   }

   if (!(uri = mongoc_uri_new (uri_string))) {
      return NULL;
   }

   topology = mongoc_topology_new(uri, true);

   client = _mongoc_client_new_from_uri (uri, topology);
   mongoc_uri_destroy (uri);

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

   client->use_ssl = true;
   memcpy (&client->ssl_opts, opts, sizeof client->ssl_opts);

   if (client->topology->single_threaded) {
      mongoc_topology_scanner_set_ssl_opts (client->topology->scanner,
                                            &client->ssl_opts);
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
   mongoc_topology_t *topology;

   topology = mongoc_topology_new (uri, true);

   return _mongoc_client_new_from_uri (uri, topology);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_client_new_from_uri --
 *
 *       Create a new mongoc_client_t for a mongoc_uri_t and a given
 *       topology object.
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
_mongoc_client_new_from_uri (const mongoc_uri_t *uri, mongoc_topology_t *topology)
{
   mongoc_client_t *client;
   const mongoc_read_prefs_t *read_prefs;
   const mongoc_read_concern_t *read_concern;
   const mongoc_write_concern_t *write_concern;

   BSON_ASSERT (uri);

#ifndef MONGOC_ENABLE_SSL
   if (mongoc_uri_get_ssl (uri)) {
      MONGOC_ERROR ("Can't create SSL client, SSL not enabled in this build.");
      return NULL;
   }
#endif

   client = (mongoc_client_t *)bson_malloc0(sizeof *client);
   client->uri = mongoc_uri_copy (uri);
   client->initiator = mongoc_client_default_stream_initiator;
   client->initiator_data = client;
   client->topology = topology;

   write_concern = mongoc_uri_get_write_concern (client->uri);
   client->write_concern = mongoc_write_concern_copy (write_concern);

   read_concern = mongoc_uri_get_read_concern (client->uri);
   client->read_concern = mongoc_read_concern_copy (read_concern);

   read_prefs = mongoc_uri_get_read_prefs_t (client->uri);
   client->read_prefs = mongoc_read_prefs_copy (read_prefs);

   mongoc_cluster_init (&client->cluster, client->uri, client);

#ifdef MONGOC_ENABLE_SSL
   client->use_ssl = false;
   if (mongoc_uri_get_ssl (client->uri)) {
      /* sets use_ssl = true */
      mongoc_client_set_ssl_opts (client, mongoc_ssl_opt_get_default ());
   }
#endif

   mongoc_counter_clients_active_inc ();

   return client;
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
      if (client->topology->single_threaded) {
         mongoc_topology_destroy(client->topology);
      }

      mongoc_write_concern_destroy (client->write_concern);
      mongoc_read_concern_destroy (client->read_concern);
      mongoc_read_prefs_destroy (client->read_prefs);
      mongoc_cluster_destroy (&client->cluster);
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
   BSON_ASSERT (client);

   return client->uri;
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
   BSON_ASSERT (client);
   BSON_ASSERT (name);

   return _mongoc_database_new(client, name, client->read_prefs,
                               client->read_concern, client->write_concern);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_get_default_database --
 *
 *       Get the database named in the MongoDB connection URI, or NULL
 *       if none was specified in the URI.
 *
 *       This structure should be freed when the caller is done with it
 *       using mongoc_database_destroy().
 *
 * Returns:
 *       A newly allocated mongoc_database_t or NULL.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_database_t *
mongoc_client_get_default_database (mongoc_client_t *client)
{
   const char *db;

   BSON_ASSERT (client);
   db = mongoc_uri_get_database (client->uri);

   if (db) {
      return mongoc_client_get_database (client, db);
   }

   return NULL;
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
   BSON_ASSERT (client);
   BSON_ASSERT (db);
   BSON_ASSERT (collection);

   return _mongoc_collection_new(client, db, collection, client->read_prefs,
                                 client->read_concern, client->write_concern);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_get_gridfs --
 *
 *       This function returns a newly allocated collection structure.
 *
 *       @db should be the name of the database, such as "test".
 *
 *       @prefix optional prefix for GridFS collection names, or NULL. Default
 *       is "fs", thus the default collection names for GridFS are "fs.files"
 *       and "fs.chunks".
 *
 * Returns:
 *       A newly allocated mongoc_gridfs_t that should be freed with
 *       mongoc_gridfs_destroy().
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
   BSON_ASSERT (client);
   BSON_ASSERT (db);

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
   BSON_ASSERT (client);

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
   BSON_ASSERT (client);

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
 * mongoc_client_get_read_concern --
 *
 *       Fetches the default read concern for @client.
 *
 * Returns:
 *       A mongoc_read_concern_t that should not be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const mongoc_read_concern_t *
mongoc_client_get_read_concern (const mongoc_client_t *client)
{
   BSON_ASSERT (client);

   return client->read_concern;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_set_read_concern --
 *
 *       Sets the default read concern for @client.
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
mongoc_client_set_read_concern (mongoc_client_t             *client,
                                const mongoc_read_concern_t *read_concern)
{
   BSON_ASSERT (client);

   if (read_concern != client->read_concern) {
      if (client->read_concern) {
         mongoc_read_concern_destroy (client->read_concern);
      }
      client->read_concern = read_concern ?
         mongoc_read_concern_copy (read_concern) :
         mongoc_read_concern_new ();
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
   BSON_ASSERT (client);

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
   BSON_ASSERT (client);

   if (read_prefs != client->read_prefs) {
      if (client->read_prefs) {
         mongoc_read_prefs_destroy(client->read_prefs);
      }
      client->read_prefs = read_prefs ?
         mongoc_read_prefs_copy(read_prefs) :
         mongoc_read_prefs_new(MONGOC_READ_PRIMARY);
   }
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
   mongoc_read_prefs_t *local_prefs = NULL;
   mongoc_cursor_t *cursor;

   BSON_ASSERT (client);
   BSON_ASSERT (db_name);
   BSON_ASSERT (query);

   /*
    * Allow a caller to provide a fully qualified namespace
    */
   if (NULL == strstr (db_name, "$cmd")) {
      bson_snprintf (ns, sizeof ns, "%s.$cmd", db_name);
      db_name = ns;
   }

   /* Server Selection Spec: "The generic command method has a default read
    * preference of mode 'primary'. The generic command method MUST ignore any
    * default read preference from client, database or collection
    * configuration. The generic command method SHOULD allow an optional read
    * preference argument."
    */
   if (!read_prefs) {
      local_prefs = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);
   }

   cursor = _mongoc_cursor_new (
      client, db_name, flags, skip, limit, batch_size, true, query, fields,
      read_prefs ? read_prefs : local_prefs, NULL);
   
   mongoc_read_prefs_destroy (local_prefs);  /* ok if NULL */

   return cursor;
}


static bool
_mongoc_client_command_with_stream (mongoc_client_t           *client,
                                    const char                *db_name,
                                    const bson_t              *command,
                                    mongoc_server_stream_t    *server_stream,
                                    const mongoc_read_prefs_t *read_prefs,
                                    bson_t                    *reply,
                                    bson_error_t              *error)
{
   mongoc_apply_read_prefs_result_t result = READ_PREFS_RESULT_INIT;
   bool ret;

   ENTRY;

   apply_read_preferences (read_prefs, server_stream, command,
                           MONGOC_QUERY_NONE, &result);

   ret = mongoc_cluster_run_command_monitored (
      &client->cluster, server_stream, result.flags, db_name,
      result.query_with_read_prefs, reply, error);

   apply_read_prefs_result_cleanup (&result);

   RETURN (ret);
}


bool
mongoc_client_command_simple (mongoc_client_t           *client,
                              const char                *db_name,
                              const bson_t              *command,
                              const mongoc_read_prefs_t *read_prefs,
                              bson_t                    *reply,
                              bson_error_t              *error)
{
   mongoc_cluster_t *cluster;
   mongoc_server_stream_t *server_stream;
   bool ret;

   ENTRY;

   BSON_ASSERT (client);
   BSON_ASSERT (db_name);
   BSON_ASSERT (command);

   cluster = &client->cluster;

   /* Server Selection Spec: "The generic command method has a default read
    * preference of mode 'primary'. The generic command method MUST ignore any
    * default read preference from client, database or collection
    * configuration. The generic command method SHOULD allow an optional read
    * preference argument."
    */
   server_stream = mongoc_cluster_stream_for_reads (cluster, read_prefs, error);

   if (server_stream) {
      ret = _mongoc_client_command_with_stream (client, db_name, command,
                                                server_stream, read_prefs,
                                                reply, error);
   } else {
      if (reply) {
         bson_init (reply);
      }

      ret = false;
   }

   mongoc_server_stream_cleanup (server_stream);

   RETURN (ret);
}


bool
mongoc_client_command_simple_with_server_id (mongoc_client_t           *client,
                                             const char                *db_name,
                                             const bson_t              *command,
                                             const mongoc_read_prefs_t *read_prefs,
                                             uint32_t                   server_id,
                                             bson_t                    *reply,
                                             bson_error_t              *error)
{
   mongoc_cluster_t *cluster;
   mongoc_server_stream_t *server_stream;
   bool ret;

   ENTRY;

   BSON_ASSERT (client);
   BSON_ASSERT (db_name);
   BSON_ASSERT (command);

   cluster = &client->cluster;
   server_stream = mongoc_cluster_stream_for_server (
      cluster, server_id, true /* reconnect ok */, error);

   if (server_stream) {
      ret = _mongoc_client_command_with_stream (client, db_name, command,
                                                server_stream, read_prefs,
                                                reply, error);
      RETURN (ret);
   } else {
      if (reply) {
         bson_init (reply);
      }

      RETURN (false);
   }
}


static void
_mongoc_client_prepare_killcursors_command (int64_t     cursor_id,
                                            const char *collection,
                                            bson_t     *command)
{
   bson_t child;

   bson_append_utf8 (command, "killCursors", 11, collection, -1);
   bson_append_array_begin (command, "cursors", 7, &child);
   bson_append_int64 (&child, "0", 1, cursor_id);
   bson_append_array_end (command, &child);
}


void
_mongoc_client_kill_cursor (mongoc_client_t *client,
                            uint32_t         server_id,
                            int64_t          cursor_id,
                            int64_t          operation_id,
                            const char      *db,
                            const char      *collection)
{
   mongoc_server_stream_t *server_stream;

   ENTRY;

   BSON_ASSERT (client);
   BSON_ASSERT (cursor_id);

   /* don't attempt reconnect if server unavailable, and ignore errors */
   server_stream = mongoc_cluster_stream_for_server (&client->cluster,
                                                     server_id,
                                                     false /* reconnect_ok */,
                                                     NULL  /* error */);

   if (!server_stream) {
      return;
   }

   if (db && collection &&
       server_stream->sd->max_wire_version >=
       WIRE_VERSION_KILLCURSORS_CMD) {
      _mongoc_client_killcursors_command (&client->cluster, server_stream,
                                          cursor_id, db, collection);
   } else {
      _mongoc_client_op_killcursors (&client->cluster,
                                     server_stream,
                                     cursor_id, operation_id,
                                     db, collection);
   }

   mongoc_server_stream_cleanup (server_stream);

   EXIT;
}


static void
_mongoc_client_monitor_op_killcursors (mongoc_cluster_t       *cluster,
                                       mongoc_server_stream_t *server_stream,
                                       int64_t                 cursor_id,
                                       int64_t                 operation_id,
                                       int64_t                 request_id,
                                       const char             *db,
                                       const char             *collection)
{
   bson_t doc;
   mongoc_client_t *client;
   mongoc_apm_command_started_t event;

   ENTRY;

   client = cluster->client;

   if (!client->apm_callbacks.started) {
      return;
   }

   bson_init (&doc);
   _mongoc_client_prepare_killcursors_command (cursor_id, collection, &doc);
   mongoc_apm_command_started_init (&event,
                                    &doc,
                                    db,
                                    "killCursors",
                                    request_id,
                                    operation_id,
                                    &server_stream->sd->host,
                                    server_stream->sd->id,
                                    client->apm_context);

   client->apm_callbacks.started (&event);
   mongoc_apm_command_started_cleanup (&event);
   bson_destroy (&doc);

   EXIT;
}


static void
_mongoc_client_monitor_op_killcursors_succeeded (
   mongoc_cluster_t       *cluster,
   int64_t                 duration,
   mongoc_server_stream_t *server_stream,
   int64_t                 cursor_id,
   int64_t                 operation_id,
   int64_t                 request_id)
{
   mongoc_client_t *client;
   bson_t doc;
   bson_t cursors_unknown;
   mongoc_apm_command_succeeded_t event;

   ENTRY;

   client = cluster->client;

   if (!client->apm_callbacks.succeeded) {
      EXIT;
   }

   /* fake server reply to killCursors command: {ok: 1, cursorsUnknown: [42]} */
   bson_init (&doc);
   bson_append_int32 (&doc, "ok", 2, 1);
   bson_append_array_begin (&doc, "cursorsUnknown", 14, &cursors_unknown);
   bson_append_int64 (&cursors_unknown, "0", 1, cursor_id);
   bson_append_array_end (&doc, &cursors_unknown);

   mongoc_apm_command_succeeded_init (&event,
                                      duration,
                                      &doc,
                                      "killCursors",
                                      request_id,
                                      operation_id,
                                      &server_stream->sd->host,
                                      server_stream->sd->id,
                                      client->apm_context);

   client->apm_callbacks.succeeded (&event);

   mongoc_apm_command_succeeded_cleanup (&event);
   bson_destroy (&doc);
}


static void
_mongoc_client_op_killcursors (mongoc_cluster_t       *cluster,
                               mongoc_server_stream_t *server_stream,
                               int64_t                 cursor_id,
                               int64_t                 operation_id,
                               const char             *db,
                               const char             *collection)
{
   int64_t started;
   mongoc_rpc_t rpc = { { 0 } };
   bool r;

   started = bson_get_monotonic_time ();

   rpc.kill_cursors.msg_len = 0;
   rpc.kill_cursors.request_id = ++cluster->request_id;
   rpc.kill_cursors.response_to = 0;
   rpc.kill_cursors.opcode = MONGOC_OPCODE_KILL_CURSORS;
   rpc.kill_cursors.zero = 0;
   rpc.kill_cursors.cursors = &cursor_id;
   rpc.kill_cursors.n_cursors = 1;

   _mongoc_client_monitor_op_killcursors (cluster, server_stream, cursor_id,
                                          operation_id,
                                          rpc.kill_cursors.request_id,
                                          db, collection);

   r = mongoc_cluster_sendv_to_server (cluster, &rpc, 1, server_stream,
                                       NULL, NULL);

   if (r) {
      _mongoc_client_monitor_op_killcursors_succeeded (
         cluster,
         bson_get_monotonic_time () - started,
         server_stream,
         cursor_id,
         operation_id,
         rpc.kill_cursors.request_id);
   }
}


static void
_mongoc_client_killcursors_command (mongoc_cluster_t       *cluster,
                                    mongoc_server_stream_t *server_stream,
                                    int64_t                 cursor_id,
                                    const char             *db,
                                    const char             *collection)
{
   bson_t command = BSON_INITIALIZER;

   ENTRY;

   _mongoc_client_prepare_killcursors_command (cursor_id,
                                               collection,
                                               &command);

   /* Find, getMore And killCursors Commands Spec: "The result from the
    * killCursors command MAY be safely ignored."
    */
   mongoc_cluster_run_command_monitored (cluster, server_stream,
                                         MONGOC_QUERY_SLAVE_OK, db, &command,
                                         NULL, NULL);

   bson_destroy (&command);

   EXIT;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_client_kill_cursor --
 *
 *       Destroy a cursor on the server.
 *
 *       NOTE: this is only reliable when connected to a single mongod or
 *       mongos. If connected to a replica set, the driver attempts to
 *       kill the cursor on the primary. If connected to multiple mongoses
 *       the kill-cursors message is sent to a *random* mongos.
 *
 *       If no primary, mongos, or standalone server is known, return
 *       without attempting to reconnect.
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
mongoc_client_kill_cursor (mongoc_client_t *client,
                           int64_t          cursor_id)
{
   mongoc_topology_t *topology;
   mongoc_server_description_t *selected_server;
   mongoc_read_prefs_t *read_prefs;
   uint32_t server_id = 0;

   topology = client->topology;
   read_prefs = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);

   mongoc_mutex_lock (&topology->mutex);

   /* see if there's a known writable server - do no I/O or retries */
   selected_server = mongoc_topology_description_select(
      &topology->description,
      MONGOC_SS_WRITE,
      read_prefs,
      topology->local_threshold_msec);

   if (selected_server) {
      server_id = selected_server->id;
   }

   mongoc_mutex_unlock (&topology->mutex);

   if (server_id) {
      _mongoc_client_kill_cursor (client, selected_server->id, cursor_id,
                                  0 /* operation_id */,
                                  NULL /* db */,
                                  NULL /* collection */);
   } else {
      MONGOC_INFO ("No server available for mongoc_client_kill_cursor");
   }

   mongoc_read_prefs_destroy (read_prefs);
}



char **
mongoc_client_get_database_names (mongoc_client_t *client,
                                  bson_error_t    *error)
{
   bson_iter_t iter;
   const char *name;
   char **ret = NULL;
   int i = 0;
   mongoc_cursor_t *cursor;
   const bson_t *doc;

   BSON_ASSERT (client);

   cursor = mongoc_client_find_databases (client, error);

   while (mongoc_cursor_next (cursor, &doc)) {
      if (bson_iter_init (&iter, doc) &&
          bson_iter_find (&iter, "name") &&
          BSON_ITER_HOLDS_UTF8 (&iter) &&
          (name = bson_iter_utf8 (&iter, NULL))) {
            ret = (char **)bson_realloc (ret, sizeof(char*) * (i + 2));
            ret [i] = bson_strdup (name);
            ret [++i] = NULL;
         }
   }

   if (!ret && !mongoc_cursor_error (cursor, error)) {
      ret = (char **)bson_malloc0 (sizeof (void*));
   }

   mongoc_cursor_destroy (cursor);

   return ret;
}


mongoc_cursor_t *
mongoc_client_find_databases (mongoc_client_t *client,
                              bson_error_t    *error)
{
   bson_t cmd = BSON_INITIALIZER;
   mongoc_cursor_t *cursor;

   BSON_ASSERT (client);

   BSON_APPEND_INT32 (&cmd, "listDatabases", 1);

   cursor = _mongoc_cursor_new (client, "admin", MONGOC_QUERY_SLAVE_OK,
                                0, 0, 0, true, NULL, NULL, NULL, NULL);

   _mongoc_cursor_array_init (cursor, &cmd, "databases");

   bson_destroy (&cmd);

   return cursor;
}


int32_t
mongoc_client_get_max_message_size (mongoc_client_t *client) /* IN */
{
   BSON_ASSERT (client);

   return mongoc_cluster_get_max_msg_size (&client->cluster);
}


int32_t
mongoc_client_get_max_bson_size (mongoc_client_t *client) /* IN */
{
   BSON_ASSERT (client);

   return mongoc_cluster_get_max_bson_obj_size (&client->cluster);
}


bool
mongoc_client_get_server_status (mongoc_client_t     *client,     /* IN */
                                 mongoc_read_prefs_t *read_prefs, /* IN */
                                 bson_t              *reply,      /* OUT */
                                 bson_error_t        *error)      /* OUT */
{
   bson_t cmd = BSON_INITIALIZER;
   bool ret = false;

   BSON_ASSERT (client);

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
   BSON_ASSERT (client);

   if (!initiator) {
      initiator = mongoc_client_default_stream_initiator;
      user_data = client;
   } else {
      MONGOC_DEBUG ("Using custom stream initiator.");
   }

   client->initiator = initiator;
   client->initiator_data = user_data;

   if (client->topology->single_threaded) {
      mongoc_topology_scanner_set_stream_initiator (client->topology->scanner,
                                                    initiator, user_data);
   }
}


void
mongoc_client_set_apm_callbacks (mongoc_client_t        *client,
                                 mongoc_apm_callbacks_t *callbacks,
                                 void                   *context)
{
   if (callbacks) {
      memcpy (&client->apm_callbacks, callbacks, sizeof client->apm_callbacks);
   } else {
      memset (&client->apm_callbacks, 0, sizeof client->apm_callbacks);
   }

   client->apm_context = context;
}


mongoc_server_description_t *
mongoc_client_get_server_description (mongoc_client_t *client,
                                      uint32_t         server_id)
{
   /* the error info isn't useful */
   return mongoc_topology_server_by_id (client->topology, server_id, NULL);
}


mongoc_server_description_t **
mongoc_client_get_server_descriptions (
   const mongoc_client_t        *client,
   size_t                       *n /* OUT */)
{
   mongoc_topology_t *topology;
   size_t i;
   mongoc_set_t *set;
   mongoc_server_description_t **sds;
   mongoc_server_description_t *sd;

   BSON_ASSERT (client);
   BSON_ASSERT (n);

   topology = (mongoc_topology_t *) client->topology;

   /* in case the client is pooled */
   mongoc_mutex_lock (&topology->mutex);

   set = topology->description.servers;

   /* enough room for all descriptions, even if some are unknown  */
   sds = (mongoc_server_description_t **) bson_malloc0 (
      sizeof (mongoc_server_description_t *) * set->items_len);

   *n = 0;
   for (i = 0; i < set->items_len; ++i) {
      sd = (mongoc_server_description_t *) mongoc_set_get_item (set, (int) i);
      if (sd->type != MONGOC_SERVER_UNKNOWN) {
         sds[i] = mongoc_server_description_new_copy (sd);
         ++(*n);
      }
   }

   mongoc_mutex_unlock (&topology->mutex);

   return sds;
}


void
mongoc_server_descriptions_destroy_all (mongoc_server_description_t **sds,
                                        size_t                        n)
{
   size_t i;

   for (i = 0; i < n; ++i) {
      mongoc_server_description_destroy (sds[i]);
   }

   bson_free (sds);
}


mongoc_server_description_t *
mongoc_client_select_server (mongoc_client_t     *client,
                             bool                 for_writes,
                             mongoc_read_prefs_t *prefs,
                             bson_error_t        *error)
{
   if (for_writes && prefs) {
      bson_set_error(error,
                     MONGOC_ERROR_SERVER_SELECTION,
                     MONGOC_ERROR_SERVER_SELECTION_FAILURE,
                     _("Cannot use read preferences with for_writes = true"));
      return NULL;
   }

   return mongoc_topology_select (client->topology,
                                  for_writes ? MONGOC_SS_WRITE : MONGOC_SS_READ,
                                  prefs,
                                  error);
}

/*==============================================================*/
/* --- mongoc-client-pool.c */

struct _mongoc_client_pool_t
{
   mongoc_mutex_t          mutex;
   mongoc_cond_t           cond;
   mongoc_queue_t          queue;
   mongoc_topology_t      *topology;
   mongoc_uri_t           *uri;
   uint32_t                min_pool_size;
   uint32_t                max_pool_size;
   uint32_t                size;
#ifdef MONGOC_ENABLE_SSL
   bool                    ssl_opts_set;
   mongoc_ssl_opt_t        ssl_opts;
#endif
   mongoc_apm_callbacks_t  apm_callbacks;
   void                   *apm_context;
};


#ifdef MONGOC_ENABLE_SSL
void
mongoc_client_pool_set_ssl_opts (mongoc_client_pool_t   *pool,
                                 const mongoc_ssl_opt_t *opts)
{
   BSON_ASSERT (pool);

   mongoc_mutex_lock (&pool->mutex);

   memset (&pool->ssl_opts, 0, sizeof pool->ssl_opts);
   pool->ssl_opts_set = false;

   if (opts) {
      memcpy (&pool->ssl_opts, opts, sizeof pool->ssl_opts);
      pool->ssl_opts_set = true;

   }

   mongoc_topology_scanner_set_ssl_opts (pool->topology->scanner, &pool->ssl_opts);

   mongoc_mutex_unlock (&pool->mutex);
}
#endif


mongoc_client_pool_t *
mongoc_client_pool_new (const mongoc_uri_t *uri)
{
   mongoc_topology_t *topology;
   mongoc_client_pool_t *pool;
   const bson_t *b;
   bson_iter_t iter;

   ENTRY;

   BSON_ASSERT (uri);

#ifndef MONGOC_ENABLE_SSL
   if (mongoc_uri_get_ssl (uri)) {
      MONGOC_ERROR ("Can't create SSL client pool,"
                    " SSL not enabled in this build.");
      return NULL;
   }
#endif

   pool = (mongoc_client_pool_t *)bson_malloc0(sizeof *pool);
   mongoc_mutex_init(&pool->mutex);
   _mongoc_queue_init(&pool->queue);
   pool->uri = mongoc_uri_copy(uri);
   pool->min_pool_size = 0;
   pool->max_pool_size = 100;
   pool->size = 0;

   topology = mongoc_topology_new(uri, false);
   pool->topology = topology;

   b = mongoc_uri_get_options(pool->uri);

   if (bson_iter_init_find_case(&iter, b, "minpoolsize")) {
      if (BSON_ITER_HOLDS_INT32(&iter)) {
         pool->min_pool_size = BSON_MAX(0, bson_iter_int32(&iter));
      }
   }

   if (bson_iter_init_find_case(&iter, b, "maxpoolsize")) {
      if (BSON_ITER_HOLDS_INT32(&iter)) {
         pool->max_pool_size = BSON_MAX(1, bson_iter_int32(&iter));
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

   BSON_ASSERT (pool);

   while ((client = (mongoc_client_t *)_mongoc_queue_pop_head(&pool->queue))) {
      mongoc_client_destroy(client);
   }

   mongoc_topology_destroy (pool->topology);

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

   BSON_ASSERT (pool);

   mongoc_mutex_lock(&pool->mutex);

again:
   if (!(client = (mongoc_client_t *)_mongoc_queue_pop_head(&pool->queue))) {
      if (pool->size < pool->max_pool_size) {
         client = _mongoc_client_new_from_uri(pool->uri, pool->topology);
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

   BSON_ASSERT (pool);

   mongoc_mutex_lock(&pool->mutex);

   if (!(client = (mongoc_client_t *)_mongoc_queue_pop_head(&pool->queue))) {
      if (pool->size < pool->max_pool_size) {
         client = _mongoc_client_new_from_uri(pool->uri, pool->topology);
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

   BSON_ASSERT (pool);
   BSON_ASSERT (client);

   mongoc_mutex_lock(&pool->mutex);
   if (pool->size > pool->min_pool_size) {
      mongoc_client_t *old_client;
      old_client = (mongoc_client_t *)_mongoc_queue_pop_head (&pool->queue);
      if (old_client) {
          mongoc_client_destroy (old_client);
          pool->size--;
      }
   }
   mongoc_mutex_unlock(&pool->mutex);

   mongoc_mutex_lock (&pool->mutex);
   _mongoc_queue_push_tail (&pool->queue, client);

   mongoc_cond_signal(&pool->cond);
   mongoc_mutex_unlock(&pool->mutex);

   EXIT;
}

size_t
mongoc_client_pool_get_size (mongoc_client_pool_t *pool)
{
   size_t size = 0;

   ENTRY;

   mongoc_mutex_lock (&pool->mutex);
   size = pool->size;
   mongoc_mutex_unlock (&pool->mutex);

   RETURN (size);
}

void
mongoc_client_pool_max_size(mongoc_client_pool_t *pool,
                            uint32_t              max_pool_size)
{
   ENTRY;

   mongoc_mutex_lock (&pool->mutex);
   pool->max_pool_size = max_pool_size;
   mongoc_mutex_unlock (&pool->mutex);

   EXIT;
}

void
mongoc_client_pool_min_size(mongoc_client_pool_t *pool,
                            uint32_t              min_pool_size)
{
   ENTRY;

   mongoc_mutex_lock (&pool->mutex);
   pool->min_pool_size = min_pool_size;
   mongoc_mutex_unlock (&pool->mutex);

   EXIT;
}

void
mongoc_client_pool_set_apm_callbacks (mongoc_client_pool_t   *pool,
                                      mongoc_apm_callbacks_t *callbacks,
                                      void                   *context)
{
   if (callbacks) {
      memcpy (&pool->apm_callbacks, callbacks, sizeof pool->apm_callbacks);
   } else {
      memset (&pool->apm_callbacks, 0, sizeof pool->apm_callbacks);
   }

   pool->apm_context = context;
}

/*==============================================================*/
/* --- mongoc-cluster.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "cluster"


#define CHECK_CLOSED_DURATION_MSEC 1000

#define DB_AND_CMD_FROM_COLLECTION(outstr, name) \
   do { \
      const char *dot = strchr(name, '.'); \
      if (!dot || ((dot - name) > ((int)sizeof outstr - 6))) { \
         bson_snprintf(outstr, sizeof outstr, "admin.$cmd"); \
      } else { \
         memcpy(outstr, name, dot - name); \
         memcpy(outstr + (dot - name), ".$cmd", 6); \
      } \
   } while (0)

static mongoc_server_stream_t *
mongoc_cluster_fetch_stream_single (mongoc_cluster_t *cluster,
                                    mongoc_server_description_t *sd,
                                    bool reconnect_ok,
                                    bson_error_t *error);

static mongoc_server_stream_t *
mongoc_cluster_fetch_stream_pooled (mongoc_cluster_t *cluster,
                                    mongoc_server_description_t *sd,
                                    bool reconnect_ok,
                                    bson_error_t *error);

static void
_bson_error_message_printf (bson_error_t *error,
                            const char *format,
                            ...) BSON_GNUC_PRINTF (2, 3);

/* Allows caller to safely overwrite error->message with a formatted string,
 * even if the formatted string includes original error->message. */
static void
_bson_error_message_printf (bson_error_t *error,
                            const char *format,
                            ...)
{
   va_list args;
   char error_message[sizeof error->message];

   if (error) {
      va_start (args, format);
      bson_vsnprintf (error_message, sizeof error->message, format, args);
      va_end (args);

      bson_strncpy (error->message, error_message, sizeof error->message);
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_run_command_internal --
 *
 *       Internal function to run a command on a given stream.
 *       @error and @reply are optional out-pointers.
 *
 * Returns:
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       @reply is set and should ALWAYS be released with bson_destroy().
 *       On failure, @error is filled out. If this was a network error
 *       and server_id is nonzero, the cluster disconnects from the server.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_cluster_run_command_internal (mongoc_cluster_t         *cluster,
                                     mongoc_stream_t          *stream,
                                     uint32_t                  server_id,
                                     mongoc_query_flags_t      flags,
                                     const char               *db_name,
                                     const bson_t             *command,
                                     bool                      monitored,
                                     const mongoc_host_list_t *host,
                                     bson_t                   *reply,
                                     bson_error_t             *error)
{
   int64_t started;
   const char *command_name;
   mongoc_apm_callbacks_t *callbacks;
   mongoc_array_t ar;                /* data to server */
   mongoc_buffer_t buffer;           /* data from server */
   mongoc_rpc_t rpc;                 /* stores request, then stores response */
   bson_error_t err_local;           /* in case the passed-in "error" is NULL */
   bson_t reply_local;
   bool reply_local_initialized = false;
   char cmd_ns[MONGOC_NAMESPACE_MAX];
   uint32_t request_id;
   int32_t msg_len;
   mongoc_apm_command_started_t started_event;
   mongoc_apm_command_succeeded_t succeeded_event;
   bool ret = false;

   ENTRY;

   BSON_ASSERT(cluster);
   BSON_ASSERT(stream);

   started = bson_get_monotonic_time ();

   /*
    * setup
    */
   command_name = _mongoc_get_command_name (command);
   BSON_ASSERT (command_name);
   callbacks = &cluster->client->apm_callbacks;
   _mongoc_array_init (&ar, sizeof (mongoc_iovec_t));
   _mongoc_buffer_init (&buffer, NULL, 0, NULL, NULL);

   if (!error) {
      error = &err_local;
   }

   error->code = 0;

   /*
    * prepare the request
    */
   bson_snprintf (cmd_ns, sizeof cmd_ns, "%s.$cmd", db_name);
   _mongoc_rpc_prep_command (&rpc, cmd_ns, command, flags);
   rpc.query.request_id = request_id = ++cluster->request_id;
   _mongoc_rpc_gather (&rpc, &ar);
   _mongoc_rpc_swab_to_le (&rpc);

   if (monitored && callbacks->started) {
      mongoc_apm_command_started_init (&started_event,
                                       command,
                                       db_name,
                                       command_name,
                                       request_id,
                                       cluster->operation_id,
                                       host,
                                       server_id,
                                       cluster->client->apm_context);

      cluster->client->apm_callbacks.started (&started_event);

      mongoc_apm_command_started_cleanup (&started_event);
   }

   if (cluster->client->in_exhaust) {
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_IN_EXHAUST,
                     _("A cursor derived from this client is in exhaust."));
      GOTO (done);
   }

   /*
    * send and receive
    */
   if (!_mongoc_stream_writev_full (stream, (mongoc_iovec_t *)ar.data, ar.len,
                                    cluster->sockettimeoutms, error) ||
       !_mongoc_buffer_append_from_stream (&buffer, stream, 4,
                                           cluster->sockettimeoutms, error)) {

      mongoc_cluster_disconnect_node (cluster, server_id);

      /* add info about the command to writev_full's error message */
      _bson_error_message_printf (
         error,
         _("Failed to send \"%s\" command with database \"%s\": %s"),
         command_name, db_name, error->message);

      GOTO (done);
   }

   BSON_ASSERT (buffer.len == 4);
   memcpy (&msg_len, buffer.data, 4);
   msg_len = BSON_UINT32_FROM_LE(msg_len);
   if ((msg_len < 16) || (msg_len > MONGOC_DEFAULT_MAX_MSG_SIZE)) {
      GOTO (done);
   }

   if (!_mongoc_buffer_append_from_stream (&buffer, stream, (size_t) msg_len - 4,
                                           cluster->sockettimeoutms, error)) {
      GOTO (done);
   }

   if (!_mongoc_rpc_scatter (&rpc, buffer.data, buffer.len)) {
      GOTO (done);
   }

   _mongoc_rpc_swab_from_le (&rpc);
   if (rpc.header.opcode != MONGOC_OPCODE_REPLY) {
      GOTO (done);
   }

   /* static-init reply_local to point into buffer */
   if (!_mongoc_rpc_reply_get_first(&rpc.reply, &reply_local)) {
      bson_set_error (error,
                      MONGOC_ERROR_BSON,
                      MONGOC_ERROR_BSON_INVALID,
                      _("Failed to decode reply BSON document."));
      GOTO (done);
   }

   reply_local_initialized = true;
   if (_mongoc_rpc_parse_command_error (&rpc, error)) {
      GOTO (done);
   }

   ret = true;
   if (monitored && callbacks->succeeded) {
      mongoc_apm_command_succeeded_init (&succeeded_event,
                                         bson_get_monotonic_time () - started,
                                         &reply_local,
                                         command_name,
                                         request_id,
                                         cluster->operation_id,
                                         host,
                                         server_id,
                                         cluster->client->apm_context);

      cluster->client->apm_callbacks.succeeded (&succeeded_event);

      mongoc_apm_command_succeeded_cleanup (&succeeded_event);
   }

done:
   if (reply && reply_local_initialized) {
      bson_copy_to (&reply_local, reply);
      bson_destroy (&reply_local);
   } else if (reply) {
      bson_init (reply);
   }

   _mongoc_buffer_destroy (&buffer);
   _mongoc_array_destroy (&ar);

   if (!ret && error->code == 0) {
      /* generic error */
      bson_set_error (error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      _("Invalid reply from server."));
   }

   RETURN (ret);
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_run_command_monitored --
 *
 *       Internal function to run a command on a given stream.
 *       @error and @reply are optional out-pointers.
 *
 * Returns:
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       If the client's APM callbacks are set, they are executed.
 *       @reply is set and should ALWAYS be released with bson_destroy().
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_cluster_run_command_monitored (mongoc_cluster_t         *cluster,
                                      mongoc_server_stream_t   *server_stream,
                                      mongoc_query_flags_t      flags,
                                      const char               *db_name,
                                      const bson_t             *command,
                                      bson_t                   *reply,
                                      bson_error_t             *error)
{
   return mongoc_cluster_run_command_internal (
      cluster, server_stream->stream, server_stream->sd->id, flags, db_name,
      command, true, &server_stream->sd->host, reply, error);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_run_command --
 *
 *       Internal function to run a command on a given stream.
 *       @error and @reply are optional out-pointers.
 *       The client's APM callbacks are not executed.
 *
 * Returns:
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       @reply is set and should ALWAYS be released with bson_destroy().
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_cluster_run_command (mongoc_cluster_t    *cluster,
                            mongoc_stream_t     *stream,
                            uint32_t             server_id,
                            mongoc_query_flags_t flags,
                            const char          *db_name,
                            const bson_t        *command,
                            bson_t              *reply,
                            bson_error_t        *error)
{
   /* monitored = false */
   return mongoc_cluster_run_command_internal (cluster,
                                               stream,
                                               server_id,
                                               flags,
                                               db_name,
                                               command,
                                               /* not monitored */
                                               false, NULL,
                                               reply, error);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_stream_run_ismaster --
 *
 *       Run an ismaster command on the given stream.
 *
 * Returns:
 *       True if ismaster ran successfully.
 *
 * Side effects:
 *       Makes a blocking I/O call and fills out @reply on success,
 *       or @error on failure.
 *
 *--------------------------------------------------------------------------
 */
static bool
_mongoc_stream_run_ismaster (mongoc_cluster_t *cluster,
                             mongoc_stream_t *stream,
                             bson_t *reply,
                             bson_error_t *error)
{
   bson_t command;
   bool ret;

   ENTRY;

   BSON_ASSERT (cluster);
   BSON_ASSERT (stream);
   BSON_ASSERT (reply);
   BSON_ASSERT (error);

   bson_init (&command);
   bson_append_int32 (&command, "ismaster", 8, 1);

   ret = mongoc_cluster_run_command (cluster, stream, 0, MONGOC_QUERY_SLAVE_OK,
                                     "admin", &command, reply, error);

   bson_destroy (&command);

   RETURN (ret);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_run_ismaster --
 *
 *       Run an ismaster command for the given node and handle result.
 *
 * Returns:
 *       True if ismaster ran successfully, false otherwise.
 *
 * Side effects:
 *       Makes a blocking I/O call.
 *
 *--------------------------------------------------------------------------
 */
static bool
_mongoc_cluster_run_ismaster (mongoc_cluster_t *cluster,
                              mongoc_cluster_node_t *node)
{
   bson_t reply;
   bson_error_t error;
   bson_iter_t iter;
   int num_fields = 0;

   ENTRY;

   BSON_ASSERT (cluster);
   BSON_ASSERT (node);
   BSON_ASSERT (node->stream);

   if (!_mongoc_stream_run_ismaster (cluster, node->stream, &reply, &error)) {
      GOTO (failure);
   }

   bson_iter_init (&iter, &reply);

   while (bson_iter_next (&iter)) {
      num_fields++;
      if (strcmp ("maxWriteBatchSize", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_INT32 (&iter)) goto failure;
         node->max_write_batch_size = bson_iter_int32 (&iter);
      } else if (strcmp ("minWireVersion", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_INT32 (&iter)) goto failure;
         node->min_wire_version = bson_iter_int32 (&iter);
      } else if (strcmp ("maxWireVersion", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_INT32 (&iter)) goto failure;
         node->max_wire_version = bson_iter_int32 (&iter);
      } else if (strcmp ("maxBsonObjSize", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_INT32 (&iter)) goto failure;
         node->max_bson_obj_size = bson_iter_int32 (&iter);
      } else if (strcmp ("maxMessageSizeBytes", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_INT32 (&iter)) goto failure;
         node->max_msg_size = bson_iter_int32 (&iter);
      }
   }

   if (num_fields == 0) goto failure;

   /* TODO: run ismaster through the topology machinery? */
   bson_destroy (&reply);

   return true;

failure:

   bson_destroy (&reply);

   return false;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cluster_build_basic_auth_digest --
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
                              mongoc_stream_t       *stream,
                              bson_error_t          *error)
{
   bson_iter_t iter;
   const char *auth_source;
   bson_t command = { 0 };
   bson_t reply = { 0 };
   char *digest;
   char *nonce;
   bool ret;

   ENTRY;

   BSON_ASSERT(cluster);
   BSON_ASSERT(stream);

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
   if (!mongoc_cluster_run_command (cluster, stream, 0, MONGOC_QUERY_SLAVE_OK,
                                    auth_source, &command, &reply, error)) {
      bson_destroy (&command);
      bson_destroy (&reply);
      RETURN (false);
   }
   bson_destroy (&command);
   if (!bson_iter_init_find_case (&iter, &reply, "nonce")) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_GETNONCE,
                      _("Invalid reply from getnonce"));
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
    * Execute the authenticate command. mongoc_cluster_run_command
    * checks for {ok: 1} in the response.
    */
   ret = mongoc_cluster_run_command (cluster, stream, 0, MONGOC_QUERY_SLAVE_OK,
                                     auth_source, &command, &reply, error);
   if (!ret) {
      /* error->message is already set */
      error->domain = MONGOC_ERROR_CLIENT;
      error->code = MONGOC_ERROR_CLIENT_AUTHENTICATE;
   }

   bson_destroy (&command);
   bson_destroy (&reply);

   RETURN (ret);
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
                                        mongoc_stream_t       *node_stream,    /* IN */
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
   BSON_ASSERT (node_stream);
   BSON_ASSERT (name);

   /*
    * Find the underlying socket used in the stream chain.
    */
   for (stream = node_stream; stream;) {
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
_mongoc_cluster_auth_node_sasl (mongoc_cluster_t *cluster,
                                mongoc_stream_t  *stream,
                                const char       *hostname,
                                bson_error_t     *error)
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
   BSON_ASSERT (stream);

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
      if (_mongoc_cluster_get_canonicalized_name (cluster, stream, real_name,
                                                  sizeof real_name, error)) {
         _mongoc_sasl_set_service_host (&sasl, real_name);
      } else {
         _mongoc_sasl_set_service_host (&sasl, hostname);
      }
   } else {
      _mongoc_sasl_set_service_host (&sasl, hostname);
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

      if (!mongoc_cluster_run_command (cluster, stream, 0, MONGOC_QUERY_SLAVE_OK,
                                       "$external", &cmd, &reply, error)) {
         bson_destroy (&cmd);
         bson_destroy (&reply);
         goto failure;
      }

      bson_destroy (&cmd);

      if (bson_iter_init_find (&iter, &reply, "done") &&
          bson_iter_as_bool (&iter)) {
         bson_destroy (&reply);
         break;
      }

      if (!bson_iter_init_find (&iter, &reply, "conversationId") ||
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
                         _("Received invalid SASL reply from MongoDB server."));
         goto failure;
      }

      tmpstr = bson_iter_utf8 (&iter, &buflen);

      if (buflen > sizeof buf) {
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_AUTHENTICATE,
                         _("SASL reply from MongoDB is too large."));

         bson_destroy (&reply);
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
                                 mongoc_stream_t       *stream,
                                 bson_error_t          *error)
{
   char buf[4096];
   int buflen = 0;
   const char *username;
   const char *password;
   bson_t b = BSON_INITIALIZER;
   bson_t reply;
   size_t len;
   char *str;
   bool ret;

   BSON_ASSERT (cluster);
   BSON_ASSERT (stream);

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
   buflen = mongoc_b64_ntop ((const uint8_t *) str, len, buf, sizeof buf);
   bson_free (str);

   if (buflen == -1) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      _("failed base64 encoding message"));
      return false;
   }

   BSON_APPEND_INT32 (&b, "saslStart", 1);
   BSON_APPEND_UTF8 (&b, "mechanism", "PLAIN");
   bson_append_utf8 (&b, "payload", 7, (const char *)buf, buflen);
   BSON_APPEND_INT32 (&b, "autoAuthorize", 1);

   ret = mongoc_cluster_run_command (cluster, stream, 0, MONGOC_QUERY_SLAVE_OK,
                                     "$external", &b, &reply, error);

   if (!ret) {
      /* error->message is already set */
      error->domain = MONGOC_ERROR_CLIENT;
      error->code = MONGOC_ERROR_CLIENT_AUTHENTICATE;
   }

   bson_destroy (&b);
   bson_destroy (&reply);

   return ret;
}


#ifdef MONGOC_ENABLE_SSL
static bool
_mongoc_cluster_auth_node_x509 (mongoc_cluster_t      *cluster,
                                mongoc_stream_t       *stream,
                                bson_error_t          *error)
{
   const char *username;
   bson_t cmd;
   bson_t reply;
   bool ret;

   BSON_ASSERT (cluster);
   BSON_ASSERT (stream);

   username = mongoc_uri_get_username (cluster->uri);
   if (username) {
      MONGOC_INFO ("X509: got username (%s) from URI", username);
   } else {
      if (!cluster->client->ssl_opts.pem_file) {
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_AUTHENTICATE,
                         _("cannot determine username for "
                         "X-509 authentication."));
         return false;
      }

      if (cluster->client->ssl_opts.pem_file) {
         username = mongoc_ssl_extract_subject (cluster->client->ssl_opts.pem_file);
         MONGOC_INFO ("X509: got username (%s) from certificate", username);
      }
   }

   bson_init (&cmd);
   BSON_APPEND_INT32 (&cmd, "authenticate", 1);
   BSON_APPEND_UTF8 (&cmd, "mechanism", "MONGODB-X509");
   BSON_APPEND_UTF8 (&cmd, "user", username);

   ret = mongoc_cluster_run_command (cluster, stream, 0, MONGOC_QUERY_SLAVE_OK,
                                     "$external", &cmd, &reply, error);

   if (!ret) {
      /* error->message is already set */
      error->domain = MONGOC_ERROR_CLIENT;
      error->code = MONGOC_ERROR_CLIENT_AUTHENTICATE;
   }

   bson_destroy (&cmd);
   bson_destroy (&reply);

   return ret;
}
#endif


#ifdef MONGOC_ENABLE_CRYPTO
static bool
_mongoc_cluster_auth_node_scram (mongoc_cluster_t      *cluster,
                                 mongoc_stream_t       *stream,
                                 bson_error_t          *error)
{
   uint32_t buflen = 0;
   mongoc_scram_t scram;
   bson_iter_t iter;
   bool ret = false;
   const char *tmpstr;
   const char *auth_source;
   uint8_t buf[4096] = { 0 };
   bson_t cmd;
   bson_t reply;
   int conv_id = 0;
   bson_subtype_t btype;

   BSON_ASSERT (cluster);
   BSON_ASSERT (stream);

   if (!(auth_source = mongoc_uri_get_auth_source(cluster->uri)) ||
       (*auth_source == '\0')) {
      auth_source = "admin";
   }

   _mongoc_scram_init(&scram);

   _mongoc_scram_set_pass (&scram, mongoc_uri_get_password (cluster->uri));
   _mongoc_scram_set_user (&scram, mongoc_uri_get_username (cluster->uri));

   for (;;) {
      if (!_mongoc_scram_step (&scram, buf, buflen, buf, sizeof buf, &buflen, error)) {
         goto failure;
      }

      bson_init (&cmd);

      if (scram.step == 1) {
         BSON_APPEND_INT32 (&cmd, "saslStart", 1);
         BSON_APPEND_UTF8 (&cmd, "mechanism", "SCRAM-SHA-1");
         bson_append_binary (&cmd, "payload", 7, BSON_SUBTYPE_BINARY, buf, buflen);
         BSON_APPEND_INT32 (&cmd, "autoAuthorize", 1);
      } else {
         BSON_APPEND_INT32 (&cmd, "saslContinue", 1);
         BSON_APPEND_INT32 (&cmd, "conversationId", conv_id);
         bson_append_binary (&cmd, "payload", 7, BSON_SUBTYPE_BINARY, buf, buflen);
      }

      MONGOC_INFO ("SCRAM: authenticating \"%s\" (step %d)",
                   mongoc_uri_get_username (cluster->uri),
                   scram.step);

      if (!mongoc_cluster_run_command (cluster, stream, 0, MONGOC_QUERY_SLAVE_OK,
                                       auth_source, &cmd, &reply, error)) {
         bson_destroy (&cmd);
         bson_destroy (&reply);

         /* error->message is already set */
         error->domain = MONGOC_ERROR_CLIENT;
         error->code = MONGOC_ERROR_CLIENT_AUTHENTICATE;
         goto failure;
      }

      bson_destroy (&cmd);

      if (bson_iter_init_find (&iter, &reply, "done") &&
          bson_iter_as_bool (&iter)) {
         bson_destroy (&reply);
         break;
      }

      if (!bson_iter_init_find (&iter, &reply, "conversationId") ||
          !BSON_ITER_HOLDS_INT32 (&iter) ||
          !(conv_id = bson_iter_int32 (&iter)) ||
          !bson_iter_init_find (&iter, &reply, "payload") ||
          !BSON_ITER_HOLDS_BINARY(&iter)) {
         const char *errmsg = "Received invalid SCRAM reply from MongoDB server.";

         MONGOC_INFO ("SCRAM: authentication failed for \"%s\"",
                      mongoc_uri_get_username (cluster->uri));

         if (bson_iter_init_find (&iter, &reply, "errmsg") &&
               BSON_ITER_HOLDS_UTF8 (&iter)) {
            errmsg = bson_iter_utf8 (&iter, NULL);
         }

         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_AUTHENTICATE,
                         "%s", errmsg);
         bson_destroy (&reply);
         goto failure;
      }

      bson_iter_binary (&iter, &btype, &buflen, (const uint8_t**)&tmpstr);

      if (buflen > sizeof buf) {
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_AUTHENTICATE,
                         _("SCRAM reply from MongoDB is too large."));
         bson_destroy (&reply);
         goto failure;
      }

      memcpy (buf, tmpstr, buflen);

      bson_destroy (&reply);
   }

   MONGOC_INFO ("SCRAM: \"%s\" authenticated",
                mongoc_uri_get_username (cluster->uri));

   ret = true;

failure:
   _mongoc_scram_destroy (&scram);

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
_mongoc_cluster_auth_node (mongoc_cluster_t *cluster,
                           mongoc_stream_t  *stream,
                           const char       *hostname,
                           int32_t           max_wire_version,
                           bson_error_t     *error)
{
   bool ret = false;
   const char *mechanism;
   ENTRY;

   BSON_ASSERT (cluster);
   BSON_ASSERT (stream);

   mechanism = mongoc_uri_get_auth_mechanism (cluster->uri);

   /* Use cached max_wire_version, not value from sd */
   if (!mechanism) {
      if (max_wire_version < WIRE_VERSION_SCRAM_DEFAULT) {
         mechanism = "MONGODB-CR";
      } else {
         mechanism = "SCRAM-SHA-1";
      }
   }

   if (0 == strcasecmp (mechanism, "MONGODB-CR")) {
      ret = _mongoc_cluster_auth_node_cr (cluster, stream, error);
   } else if (0 == strcasecmp (mechanism, "MONGODB-X509")) {
#ifdef MONGOC_ENABLE_SSL
      ret = _mongoc_cluster_auth_node_x509 (cluster, stream, error);
#else
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      _("The \"%s\" authentication mechanism requires libmongoc built with --enable-ssl"),
                      mechanism);
#endif
   } else if (0 == strcasecmp (mechanism, "SCRAM-SHA-1")) {
#ifdef MONGOC_ENABLE_CRYPTO
      ret = _mongoc_cluster_auth_node_scram (cluster, stream, error);
#else
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      _("The \"%s\" authentication mechanism requires libmongoc built with --enable-ssl"),
                      mechanism);
#endif
   } else if (0 == strcasecmp (mechanism, "GSSAPI")) {
#ifdef MONGOC_ENABLE_SASL
      ret = _mongoc_cluster_auth_node_sasl (cluster, stream, hostname, error);
#else
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      _("The \"%s\" authentication mechanism requires libmongoc built with --enable-sasl"),
                      mechanism);
#endif
   } else if (0 == strcasecmp (mechanism, "PLAIN")) {
      ret = _mongoc_cluster_auth_node_plain (cluster, stream, error);
   } else {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      _("Unknown authentication mechanism \"%s\"."),
                      mechanism);
   }

   if (!ret) {
      mongoc_counter_auth_failure_inc ();
      MONGOC_DEBUG("Authentication failed: %s", error->message);
   } else {
      mongoc_counter_auth_success_inc ();
      TRACE("%s", "Authentication succeeded");
   }

   RETURN(ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_disconnect_node --
 *
 *       Remove a node from the set of nodes. This should be done if
 *       a stream in the set is found to be invalid.
 *
 *       WARNING: pointers to a disconnected mongoc_cluster_node_t or
 *       its stream are now invalid, be careful of dangling pointers.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Removes node from cluster's set of nodes, and frees the
 *       mongoc_cluster_node_t if pooled.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_cluster_disconnect_node (mongoc_cluster_t *cluster, uint32_t server_id)
{
   mongoc_topology_t *topology = cluster->client->topology;
   ENTRY;

   if (topology->single_threaded) {
      mongoc_topology_scanner_node_t *scanner_node;

      scanner_node = mongoc_topology_scanner_get_node (topology->scanner, server_id);

      /* might never actually have connected */
      if (scanner_node && scanner_node->stream) {
         mongoc_topology_scanner_node_disconnect (scanner_node, true);
         EXIT;
      }
      EXIT;
   } else {
      mongoc_set_rm(cluster->nodes, server_id);
   }

   EXIT;
}

static void
_mongoc_cluster_node_destroy (mongoc_cluster_node_t *node)
{
   /* Failure, or Replica Set reconfigure without this node */
   mongoc_stream_failed (node->stream);

   bson_free (node);
}

static void
_mongoc_cluster_node_dtor (void *data_,
                           void *ctx_)
{
   mongoc_cluster_node_t *node = (mongoc_cluster_node_t *)data_;

   _mongoc_cluster_node_destroy (node);
}

static mongoc_cluster_node_t *
_mongoc_cluster_node_new (mongoc_stream_t *stream)
{
   mongoc_cluster_node_t *node;

   if (!stream) {
      return NULL;
   }

   node = (mongoc_cluster_node_t *)bson_malloc0(sizeof *node);

   node->stream = stream;
   node->timestamp = bson_get_monotonic_time ();

   node->max_wire_version = MONGOC_DEFAULT_WIRE_VERSION;
   node->min_wire_version = MONGOC_DEFAULT_WIRE_VERSION;

   node->max_write_batch_size = MONGOC_DEFAULT_WRITE_BATCH_SIZE;
   node->max_bson_obj_size = MONGOC_DEFAULT_BSON_OBJ_SIZE;
   node->max_msg_size = MONGOC_DEFAULT_MAX_MSG_SIZE;

   return node;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_add_node --
 *
 *       Add a new node to this cluster for the given server description.
 *
 *       NOTE: does NOT check if this server is already in the cluster.
 *
 * Returns:
 *       A stream connected to the server, or NULL on failure.
 *
 * Side effects:
 *       Adds a cluster node, or sets error on failure.
 *
 *--------------------------------------------------------------------------
 */
static mongoc_stream_t *
_mongoc_cluster_add_node (mongoc_cluster_t *cluster,
                          mongoc_server_description_t *sd,
                          bson_error_t *error /* OUT */)
{
   mongoc_cluster_node_t *cluster_node;
   mongoc_stream_t *stream;

   ENTRY;

   BSON_ASSERT (cluster);
   BSON_ASSERT (!cluster->client->topology->single_threaded);

   TRACE ("Adding new server to cluster: %s", sd->connection_address);

   stream = _mongoc_client_create_stream(cluster->client, &sd->host, error);
   if (!stream) {
      MONGOC_WARNING ("Failed connection to %s (%s)", sd->connection_address, error->message);
      RETURN (NULL);
   }

   /* take critical fields from a fresh ismaster */
   cluster_node = _mongoc_cluster_node_new (stream);
   if (!_mongoc_cluster_run_ismaster (cluster, cluster_node)) {
      _mongoc_cluster_node_destroy (cluster_node);
      MONGOC_WARNING ("Failed connection to %s (ismaster failed)", sd->connection_address);
      RETURN (NULL);
   }

   if (cluster->requires_auth) {
      if (!_mongoc_cluster_auth_node (cluster, cluster_node->stream, sd->host.host,
                                      cluster_node->max_wire_version, error)) {
         MONGOC_WARNING ("Failed authentication to %s (%s)", sd->connection_address, error->message);
         _mongoc_cluster_node_destroy (cluster_node);
         RETURN (NULL);
      }
   }

   mongoc_set_add (cluster->nodes, sd->id, cluster_node);

   RETURN (stream);
}

static void
node_not_found (mongoc_server_description_t *sd,
                bson_error_t *error /* OUT */)
{
   if (!error) {
      return;
   }

   if (sd->error.code) {
      memcpy (error, &sd->error, sizeof *error);
   } else {
      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_NOT_ESTABLISHED,
                      _("Could not find node %s"),
                      sd->host.host_and_port);
   }
}

static void
stream_not_found (mongoc_server_description_t *sd,
                  bson_error_t *error /* OUT */)
{
   if (!error) {
      return;
   }

   if (sd->error.code) {
      memcpy (error, &sd->error, sizeof *error);
   } else {
      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_NOT_ESTABLISHED,
                      _("Could not find stream for node %s"),
                      sd->host.host_and_port);
   }
}


static mongoc_server_stream_t *
_mongoc_cluster_stream_for_server_description (mongoc_cluster_t *cluster,
                                               mongoc_server_description_t *sd,
                                               bool reconnect_ok,
                                               bson_error_t *error)
{
   mongoc_topology_t *topology;
   mongoc_server_stream_t *server_stream;

   ENTRY;

   topology = cluster->client->topology;

   /* in the single-threaded use case we share topology's streams */
   if (topology->single_threaded) {
      server_stream = mongoc_cluster_fetch_stream_single (cluster,
                                                          sd,
                                                          reconnect_ok,
                                                          error);

   } else {
      server_stream = mongoc_cluster_fetch_stream_pooled (cluster,
                                                          sd,
                                                          reconnect_ok,
                                                          error);

   }

   if (!server_stream) {
      /* Server Discovery And Monitoring Spec: When an application operation
       * fails because of any network error besides a socket timeout, the
       * client MUST replace the server's description with a default
       * ServerDescription of type Unknown, and fill the ServerDescription's
       * error field with useful information.
       *
       * error was filled by fetch_stream_single/pooled, pass it to invalidate()
       */
      mongoc_cluster_disconnect_node (cluster, sd->id);
      mongoc_topology_invalidate_server (topology, sd->id, error);
   }

   RETURN (server_stream);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_stream_for_server --
 *
 *       Fetch the stream for @server_id. If @reconnect_ok and there is no
 *       valid stream, attempts to reconnect; if not @reconnect_ok then only
 *       an existing stream can be returned, or NULL.
 *
 * Returns:
 *       A mongoc_server_stream_t, or NULL
 *
 * Side effects:
 *       May add a node or reconnect one, if @reconnect_ok.
 *       Authenticates the stream if needed.
 *       May set @error.
 *
 *--------------------------------------------------------------------------
 */

mongoc_server_stream_t *
mongoc_cluster_stream_for_server (mongoc_cluster_t *cluster,
                                  uint32_t server_id,
                                  bool reconnect_ok,
                                  bson_error_t *error)
{
   mongoc_topology_t *topology;
   mongoc_server_description_t *sd;
   mongoc_server_stream_t *server_stream = NULL;

   ENTRY;

   BSON_ASSERT (cluster);
   BSON_ASSERT (server_id);

   topology = cluster->client->topology;

   if (!(sd = mongoc_topology_server_by_id (topology, server_id, error))) {
      RETURN (NULL);
   }

   server_stream = _mongoc_cluster_stream_for_server_description (cluster,
                                                                  sd,
                                                                  reconnect_ok,
                                                                  error);

   if (!server_stream) {
      /* failed */
      mongoc_cluster_disconnect_node (cluster, server_id);
      mongoc_server_description_destroy (sd);
   }

   RETURN (server_stream);
}


static mongoc_server_stream_t *
mongoc_cluster_fetch_stream_single (mongoc_cluster_t *cluster,
                                    mongoc_server_description_t *sd,
                                    bool reconnect_ok,
                                    bson_error_t *error /* OUT */)
{
   mongoc_topology_t *topology;
   mongoc_stream_t *stream;
   mongoc_topology_scanner_node_t *scanner_node;
   int64_t expire_at;
   bson_t reply;

   topology = cluster->client->topology;

   scanner_node = mongoc_topology_scanner_get_node (topology->scanner, sd->id);
   BSON_ASSERT (scanner_node && !scanner_node->retired);
   stream = scanner_node->stream;

   if (!stream) {
      if (!reconnect_ok) {
         stream_not_found (sd, error);
         return NULL;
      }

      if (!mongoc_topology_scanner_node_setup (scanner_node, error)) {
         return NULL;
      }
      stream = scanner_node->stream;

      expire_at = bson_get_monotonic_time() + topology->connect_timeout_msec * 1000;
      if (!mongoc_stream_wait (stream, expire_at)) {
         bson_set_error (error,
                         MONGOC_ERROR_STREAM,
                         MONGOC_ERROR_STREAM_CONNECT,
                         _("Failed to connect to target host: '%s'"),
                         sd->host.host_and_port);
         return NULL;
      }

      if (!_mongoc_stream_run_ismaster (cluster, stream, &reply, error)) {
         return NULL;
      }

      /* TODO: run ismaster through the topology machinery? */
      bson_destroy (&reply);
   }

   /* if stream exists but isn't authed, a disconnect happened */
   if (cluster->requires_auth && !scanner_node->has_auth) {
      /* In single-threaded mode, we can use sd's max_wire_version */
      if (!_mongoc_cluster_auth_node (cluster, stream, sd->host.host,
                                      sd->max_wire_version, &sd->error)) {
         memcpy (error, &sd->error, sizeof *error);
         return NULL;
      }

      scanner_node->has_auth = true;
   }

   return mongoc_server_stream_new (topology->description.type, sd, stream);
}

static mongoc_server_stream_t *
mongoc_cluster_fetch_stream_pooled (mongoc_cluster_t *cluster,
                                    mongoc_server_description_t *sd,
                                    bool reconnect_ok,
                                    bson_error_t *error /* OUT */)
{
   mongoc_topology_t *topology;
   mongoc_stream_t *stream;
   mongoc_cluster_node_t *cluster_node;
   int64_t timestamp;

   cluster_node = (mongoc_cluster_node_t *) mongoc_set_get (cluster->nodes,
                                                            sd->id);

   topology = cluster->client->topology;

   if (cluster_node) {
      BSON_ASSERT (cluster_node->stream);

      timestamp = mongoc_topology_server_timestamp (topology, sd->id);
      if (timestamp == -1 || cluster_node->timestamp < timestamp) {
         /* topology change or net error during background scan made us remove
          * or replace server description since node's birth. destroy node. */
         mongoc_cluster_disconnect_node (cluster, sd->id);
      } else {
         /* TODO: thread safety! */
         return mongoc_server_stream_new (topology->description.type,
                                          sd, cluster_node->stream);
      }
   }

   /* no node, or out of date */
   if (!reconnect_ok) {
      node_not_found (sd, error);
      return NULL;
   }

   stream = _mongoc_cluster_add_node (cluster, sd, error);
   if (stream) {
      /* TODO: thread safety! */
      return mongoc_server_stream_new (topology->description.type,
                                       sd, stream);
   } else {
      return NULL;
   }
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_init --
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
mongoc_cluster_init (mongoc_cluster_t   *cluster,
                     const mongoc_uri_t *uri,
                     void               *client)
{
   ENTRY;

   BSON_ASSERT (cluster);
   BSON_ASSERT (uri);

   memset (cluster, 0, sizeof *cluster);

   cluster->uri = mongoc_uri_copy(uri);
   cluster->client = (mongoc_client_t *)client;
   cluster->requires_auth = (mongoc_uri_get_username(uri) ||
                             mongoc_uri_get_auth_mechanism(uri));

   cluster->sockettimeoutms = mongoc_uri_get_option_as_int32(
      uri, "sockettimeoutms", MONGOC_DEFAULT_SOCKETTIMEOUTMS);

   cluster->socketcheckintervalms = mongoc_uri_get_option_as_int32(
      uri, "socketcheckintervalms", MONGOC_TOPOLOGY_SOCKET_CHECK_INTERVAL_MS);

   /* TODO for single-threaded case we don't need this */
   cluster->nodes = mongoc_set_new(8, _mongoc_cluster_node_dtor, NULL);

   _mongoc_array_init (&cluster->iov, sizeof (mongoc_iovec_t));

   cluster->operation_id = rand ();

   EXIT;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_destroy --
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
mongoc_cluster_destroy (mongoc_cluster_t *cluster) /* INOUT */
{
   ENTRY;

   BSON_ASSERT (cluster);

   mongoc_uri_destroy(cluster->uri);

   mongoc_set_destroy(cluster->nodes);

   _mongoc_array_destroy(&cluster->iov);

   EXIT;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_stream_for_optype --
 *
 *       Internal server selection.
 *
 * Returns:
 *       A mongoc_server_stream_t on which you must call
 *       mongoc_server_stream_cleanup, or NULL on failure (sets @error)
 *
 * Side effects:
 *       May set @error.
 *       May add new nodes to @cluster->nodes.
 *
 *--------------------------------------------------------------------------
 */

static mongoc_server_stream_t *
_mongoc_cluster_stream_for_optype (mongoc_cluster_t *cluster,
                                   mongoc_ss_optype_t optype,
                                   const mongoc_read_prefs_t *read_prefs,
                                   bson_error_t *error)
{
   mongoc_server_stream_t *server_stream;
   mongoc_server_description_t *selected_server;
   mongoc_topology_t *topology = cluster->client->topology;

   ENTRY;

   BSON_ASSERT (cluster);

   /* this is a new copy of the server description */
   selected_server = mongoc_topology_select (topology,
                                            optype,
                                            read_prefs,
                                            error);

   if (!selected_server) {
      RETURN(NULL);
   }

   /* connect or reconnect to server if necessary */
   server_stream = _mongoc_cluster_stream_for_server_description (
      cluster, selected_server,
      true /* reconnect_ok */, error);

   if (!server_stream ) {
      mongoc_server_description_destroy (selected_server);
      RETURN (NULL);
   }

   RETURN (server_stream);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_stream_for_reads --
 *
 *       Internal server selection.
 *
 * Returns:
 *       A mongoc_server_stream_t on which you must call
 *       mongoc_server_stream_cleanup, or NULL on failure (sets @error)
 *
 * Side effects:
 *       May set @error.
 *       May add new nodes to @cluster->nodes.
 *
 *--------------------------------------------------------------------------
 */

mongoc_server_stream_t *
mongoc_cluster_stream_for_reads (mongoc_cluster_t *cluster,
                                 const mongoc_read_prefs_t *read_prefs,
                                 bson_error_t *error)
{
   return _mongoc_cluster_stream_for_optype (cluster, MONGOC_SS_READ,
                                             read_prefs, error);
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_stream_for_writes --
 *
 *       Get a stream for write operations.
 *
 * Returns:
 *       A mongoc_server_stream_t on which you must call
 *       mongoc_server_stream_cleanup, or NULL on failure (sets @error)
 *
 * Side effects:
 *       May set @error.
 *       May add new nodes to @cluster->nodes.
 *
 *--------------------------------------------------------------------------
 */

mongoc_server_stream_t *
mongoc_cluster_stream_for_writes (mongoc_cluster_t *cluster,
                                  bson_error_t *error)
{
   return _mongoc_cluster_stream_for_optype (cluster, MONGOC_SS_WRITE,
                                             NULL, error);
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

static bool
_mongoc_cluster_min_of_max_obj_size_sds (void *item,
                                         void *ctx)
{
   mongoc_server_description_t *sd = (mongoc_server_description_t *)item;
   int32_t *current_min = (int32_t *)ctx;

   if (sd->max_bson_obj_size < *current_min) {
      *current_min = sd->max_bson_obj_size;
   }
   return true;
}

static bool
_mongoc_cluster_min_of_max_obj_size_nodes (void *item,
                                           void *ctx)
{
   mongoc_cluster_node_t *node = (mongoc_cluster_node_t *)item;
   int32_t *current_min = (int32_t *)ctx;

   if (node->max_bson_obj_size < *current_min) {
      *current_min = node->max_bson_obj_size;
   }
   return true;
}

static bool
_mongoc_cluster_min_of_max_msg_size_sds (void *item,
                                         void *ctx)
{
   mongoc_server_description_t *sd = (mongoc_server_description_t *)item;
   int32_t *current_min = (int32_t *)ctx;

   if (sd->max_msg_size < *current_min) {
      *current_min = sd->max_msg_size;
   }
   return true;
}

static bool
_mongoc_cluster_min_of_max_msg_size_nodes (void *item,
                                           void *ctx)
{
   mongoc_cluster_node_t *node = (mongoc_cluster_node_t *)item;
   int32_t *current_min = (int32_t *)ctx;

   if (node->max_msg_size < *current_min) {
      *current_min = node->max_msg_size;
   }
   return true;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_get_max_bson_obj_size --
 *
 *      Return the minimum max_bson_obj_size across all servers in cluster.
 *
 *      NOTE: this method uses the topology's mutex.
 *
 * Returns:
 *      The minimum max_bson_obj_size.
 *
 * Side effects:
 *      None
 *
 *--------------------------------------------------------------------------
 */
int32_t
mongoc_cluster_get_max_bson_obj_size (mongoc_cluster_t *cluster)
{
   int32_t max_bson_obj_size = -1;

   max_bson_obj_size = MONGOC_DEFAULT_BSON_OBJ_SIZE;

   if (!cluster->client->topology->single_threaded) {
      mongoc_set_for_each (cluster->nodes,
                           _mongoc_cluster_min_of_max_obj_size_nodes,
                           &max_bson_obj_size);
   } else {
      mongoc_set_for_each (cluster->client->topology->description.servers,
                           _mongoc_cluster_min_of_max_obj_size_sds,
                           &max_bson_obj_size);
   }

   return max_bson_obj_size;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_get_max_msg_size --
 *
 *      Return the minimum max msg size across all servers in cluster.
 *
 *      NOTE: this method uses the topology's mutex.
 *
 * Returns:
 *      The minimum max_msg_size
 *
 * Side effects:
 *      None
 *
 *--------------------------------------------------------------------------
 */
int32_t
mongoc_cluster_get_max_msg_size (mongoc_cluster_t *cluster)
{
   int32_t max_msg_size = MONGOC_DEFAULT_MAX_MSG_SIZE;

   if (!cluster->client->topology->single_threaded) {
      mongoc_set_for_each (cluster->nodes,
                           _mongoc_cluster_min_of_max_msg_size_nodes,
                           &max_msg_size);
   } else {
      mongoc_set_for_each (cluster->client->topology->description.servers,
                           _mongoc_cluster_min_of_max_msg_size_sds,
                           &max_msg_size);
   }

   return max_msg_size;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_node_min_wire_version --
 *
 *      Return the min wire version for the given server.
 *
 * Returns:
 *      Min wire version, or -1 if server is not found.
 *
 *--------------------------------------------------------------------------
 */

int32_t
mongoc_cluster_node_min_wire_version (mongoc_cluster_t *cluster,
                                      uint32_t          server_id)
{
   mongoc_server_description_t *sd;
   mongoc_cluster_node_t *node;

   if (cluster->client->topology->single_threaded) {
      if ((sd = mongoc_topology_description_server_by_id (
         &cluster->client->topology->description, server_id, NULL))) {
         return sd->min_wire_version;
      }
   } else {
      if((node = (mongoc_cluster_node_t *)mongoc_set_get(cluster->nodes, server_id))) {
         return node->min_wire_version;
      }
   }

   return -1;
}


static bool
_mongoc_cluster_check_interval (mongoc_cluster_t *cluster,
                                uint32_t          server_id,
                                bson_error_t     *error)
{
   mongoc_topology_t *topology;
   mongoc_topology_scanner_node_t *scanner_node;
   mongoc_server_description_t *sd;
   mongoc_stream_t *stream;
   int64_t now;
   int64_t before_ismaster;
   bson_t command;
   bson_t reply;
   bool r;

   topology = cluster->client->topology;

   if (!topology->single_threaded) {
      return true;
   }

   scanner_node =
      mongoc_topology_scanner_get_node (topology->scanner, server_id);

   if (!scanner_node) {
      return false;
   }

   BSON_ASSERT (!scanner_node->retired);

   stream = scanner_node->stream;

   if (!stream) {
      return false;
   }

   now = bson_get_monotonic_time ();

   if (scanner_node->last_used + (1000 * CHECK_CLOSED_DURATION_MSEC) < now) {
      if (mongoc_stream_check_closed (stream)) {
         mongoc_cluster_disconnect_node (cluster, server_id);
         bson_set_error (error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_SOCKET,
                         _("Stream is closed"));
         return false;
      }
   }

   if (scanner_node->last_used + (1000 * cluster->socketcheckintervalms) <
       now) {
      bson_init (&command);
      BSON_APPEND_INT32 (&command, "ismaster", 1);

      before_ismaster = now;
      r = mongoc_cluster_run_command (cluster, stream, server_id,
                                      MONGOC_QUERY_SLAVE_OK, "admin", &command,
                                      &reply, error);

      now = bson_get_monotonic_time ();

      bson_destroy (&command);

      if (r) {
         sd = mongoc_topology_description_server_by_id (&topology->description,
                                                        server_id, NULL);

         if (!sd) {
            bson_destroy (&reply);
            return false;
         }

         mongoc_topology_description_handle_ismaster (
            &topology->description, sd, &reply,
            (now - before_ismaster) / 1000,    /* RTT_MS */
            error);

         bson_destroy (&reply);
      } else {
         bson_destroy (&reply);
         return false;
      }
   }

   return true;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_sendv_to_server --
 *
 *       Sends the given RPCs to the given server.
 *
 * Returns:
 *       True if successful.
 *
 * Side effects:
 *       @rpcs may be mutated and should be considered invalid after calling
 *       this method.
 *
 *       @error may be set.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_cluster_sendv_to_server (mongoc_cluster_t              *cluster,
                                mongoc_rpc_t                  *rpcs,
                                size_t                         rpcs_len,
                                mongoc_server_stream_t        *server_stream,
                                const mongoc_write_concern_t  *write_concern,
                                bson_error_t                  *error)
{
   uint32_t server_id;
   mongoc_iovec_t *iov;
   mongoc_topology_scanner_node_t *scanner_node;
   const bson_t *b;
   mongoc_rpc_t gle;
   size_t iovcnt;
   size_t i;
   bool need_gle;
   char cmdname[140];
   int32_t max_msg_size;

   ENTRY;

   BSON_ASSERT (cluster);
   BSON_ASSERT (rpcs);
   BSON_ASSERT (rpcs_len);
   BSON_ASSERT (server_stream);

   server_id = server_stream->sd->id;

   if (cluster->client->in_exhaust) {
      bson_set_error(error,
                     MONGOC_ERROR_CLIENT,
                     MONGOC_ERROR_CLIENT_IN_EXHAUST,
                     _("A cursor derived from this client is in exhaust."));
      RETURN(false);
   }

   if (! write_concern) {
      write_concern = cluster->client->write_concern;
   }

   if (!_mongoc_cluster_check_interval (cluster,
                                        server_stream->sd->id,
                                        error)) {
      RETURN (false);
   }

   _mongoc_array_clear(&cluster->iov);

   /*
    * TODO: We can probably remove the need for sendv and just do send since
    * we support write concerns now. Also, we clobber our getlasterror on
    * each subsequent mutation. It's okay, since it comes out correct anyway,
    * just useless work (and technically the request_id changes).
    */

   for (i = 0; i < rpcs_len; i++) {
      _mongoc_cluster_inc_egress_rpc (&rpcs[i]);
      need_gle = _mongoc_rpc_needs_gle(&rpcs[i], write_concern);
      _mongoc_rpc_gather (&rpcs[i], &cluster->iov);

      max_msg_size = mongoc_server_stream_max_msg_size (server_stream);

      if (rpcs[i].header.msg_len > max_msg_size) {
         bson_set_error(error,
                        MONGOC_ERROR_CLIENT,
                        MONGOC_ERROR_CLIENT_TOO_BIG,
                        _("Attempted to send an RPC larger than the "
                        "max allowed message size. Was %u, allowed %u."),
                        rpcs[i].header.msg_len,
                        max_msg_size);
         RETURN(false);
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
            DB_AND_CMD_FROM_COLLECTION(cmdname, rpcs[i].delete_.collection);
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
         b = _mongoc_write_concern_get_gle((mongoc_write_concern_t *)write_concern);
         gle.query.query = bson_get_data(b);
         gle.query.fields = NULL;
         _mongoc_rpc_gather(&gle, &cluster->iov);
         _mongoc_rpc_swab_to_le(&gle);
      }

      _mongoc_rpc_swab_to_le(&rpcs[i]);
   }

   iov = (mongoc_iovec_t *)cluster->iov.data;
   iovcnt = cluster->iov.len;

   BSON_ASSERT (cluster->iov.len);

   if (!_mongoc_stream_writev_full (server_stream->stream, iov, iovcnt,
                                    cluster->sockettimeoutms, error)) {
      RETURN (false);
   }

   if (cluster->client->topology->single_threaded) {
      scanner_node =
         mongoc_topology_scanner_get_node (cluster->client->topology->scanner,
                                           server_id);

      if (scanner_node) {
         scanner_node->last_used = bson_get_monotonic_time ();
      }
   }

   RETURN (true);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cluster_try_recv --
 *
 *       Tries to receive the next event from the MongoDB server.
 *       The contents are loaded into @buffer and then
 *       scattered into the @rpc structure. @rpc is valid as long as
 *       @buffer contains the contents read into it.
 *
 *       Callers that can optimize a reuse of @buffer should do so. It
 *       can save many memory allocations.
 *
 * Returns:
 *       True if successful.
 *
 * Side effects:
 *       @rpc is set on success, @error on failure.
 *       @buffer will be filled with the input data.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_cluster_try_recv (mongoc_cluster_t       *cluster,
                         mongoc_rpc_t           *rpc,
                         mongoc_buffer_t        *buffer,
                         mongoc_server_stream_t *server_stream,
                         bson_error_t           *error)
{
   uint32_t server_id;
   int32_t msg_len;
   int32_t max_msg_size;
   off_t pos;

   ENTRY;

   BSON_ASSERT (cluster);
   BSON_ASSERT (rpc);
   BSON_ASSERT (buffer);
   BSON_ASSERT (server_stream);

   server_id = server_stream->sd->id;

   TRACE ("Waiting for reply from server_id \"%u\"", server_id);

   /*
    * Buffer the message length to determine how much more to read.
    */
   pos = buffer->len;
   if (!_mongoc_buffer_append_from_stream (buffer, server_stream->stream, 4,
                                           cluster->sockettimeoutms, error)) {
      MONGOC_DEBUG("Could not read 4 bytes, stream probably closed or timed out");
      mongoc_counter_protocol_ingress_error_inc ();
      mongoc_cluster_disconnect_node(cluster, server_id);
      RETURN (false);
   }

   /*
    * Read the msg length from the buffer.
    */
   memcpy (&msg_len, &buffer->data[buffer->off + pos], 4);
   msg_len = BSON_UINT32_FROM_LE (msg_len);
   max_msg_size = mongoc_server_stream_max_msg_size (server_stream);
   if ((msg_len < 16) || (msg_len > max_msg_size)) {
      bson_set_error (error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      _("Corrupt or malicious reply received."));
      mongoc_cluster_disconnect_node(cluster, server_id);
      mongoc_counter_protocol_ingress_error_inc ();
      RETURN (false);
   }

   /*
    * Read the rest of the message from the stream.
    */
   if (!_mongoc_buffer_append_from_stream (buffer, server_stream->stream,
                                           msg_len - 4,
                                           cluster->sockettimeoutms, error)) {
      mongoc_cluster_disconnect_node (cluster, server_id);
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
                      _("Failed to decode reply from server."));
      mongoc_cluster_disconnect_node (cluster, server_id);
      mongoc_counter_protocol_ingress_error_inc ();
      RETURN (false);
   }

   _mongoc_rpc_swab_from_le (rpc);

   _mongoc_cluster_inc_ingress_rpc (rpc);

   RETURN(true);
}

/*==============================================================*/
/* --- mongoc-collection.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "collection"


#define _BSON_APPEND_WRITE_CONCERN(_bson, _write_concern) \
      do { \
         const bson_t *write_concern_bson; \
         mongoc_write_concern_t *write_concern_copy = NULL; \
         if (_write_concern->frozen) { \
            write_concern_bson = _mongoc_write_concern_get_bson (_write_concern); \
         } else { \
            /* _mongoc_write_concern_get_bson will freeze the write_concern */ \
            write_concern_copy = mongoc_write_concern_copy (_write_concern); \
            write_concern_bson = _mongoc_write_concern_get_bson (write_concern_copy); \
         } \
         BSON_APPEND_DOCUMENT (_bson, "writeConcern", write_concern_bson); \
         if (write_concern_copy) { \
            mongoc_write_concern_destroy (write_concern_copy); \
         } \
      } while(0); \

static BSON_GNUC_PURE bool
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

static mongoc_cursor_t *
_mongoc_collection_cursor_new (mongoc_collection_t *collection,
                               mongoc_query_flags_t flags)
{
  return _mongoc_cursor_new (collection->client,
                             collection->ns,
                             flags,
                             0,                  /* skip */
                             0,                  /* limit */
                             0,                  /* batch_size */
                             false,              /* is_command */
                             NULL,               /* query */
                             NULL,               /* fields */
                             NULL,               /* read prefs */
                             NULL);              /* read concern */
}

static void
_mongoc_collection_write_command_execute (mongoc_write_command_t       *command,
                                          const mongoc_collection_t    *collection,
                                          const mongoc_write_concern_t *write_concern,
                                          mongoc_write_result_t        *result)
{
   mongoc_server_stream_t *server_stream;

   ENTRY;

   server_stream = mongoc_cluster_stream_for_writes (&collection->client->cluster,
                                                     &result->error);

   if (!server_stream) {
      /* result->error has been filled out */
      EXIT;
   }

   _mongoc_write_command_execute (command, collection->client, server_stream,
                                  collection->db, collection->collection,
                                  write_concern, 0 /* offset */, result);

   mongoc_server_stream_cleanup (server_stream);

   EXIT;
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
 *       @read_concern is the default read concern to apply or NULL.
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
                        const mongoc_read_concern_t  *read_concern,
                        const mongoc_write_concern_t *write_concern)
{
   mongoc_collection_t *col;

   ENTRY;

   BSON_ASSERT (client);
   BSON_ASSERT (db);
   BSON_ASSERT (collection);

   col = (mongoc_collection_t *)bson_malloc0(sizeof *col);
   col->client = client;
   col->write_concern = write_concern ?
      mongoc_write_concern_copy(write_concern) :
      mongoc_write_concern_new();
   col->read_concern = read_concern ?
      mongoc_read_concern_copy(read_concern) :
      mongoc_read_concern_new();
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

   BSON_ASSERT (collection);

   bson_clear (&collection->gle);

   _mongoc_buffer_destroy(&collection->buffer);

   if (collection->read_prefs) {
      mongoc_read_prefs_destroy(collection->read_prefs);
      collection->read_prefs = NULL;
   }

   if (collection->read_concern) {
      mongoc_read_concern_destroy(collection->read_concern);
      collection->read_concern = NULL;
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
 * mongoc_collection_copy --
 *
 *       Returns a copy of @collection that needs to be freed by calling
 *       mongoc_collection_destroy.
 *
 * Returns:
 *       A copy of this collection.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_collection_t *
mongoc_collection_copy (mongoc_collection_t *collection) /* IN */
{
   ENTRY;

   BSON_ASSERT (collection);

   RETURN(_mongoc_collection_new(collection->client, collection->db,
                                 collection->collection, collection->read_prefs,
                                 collection->read_concern,
                                 collection->write_concern));
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
 *       @options:  A bson_t containing aggregation options, such as
 *                  bypassDocumentValidation (used with $out pipeline),
 *                  maxTimeMS (declaring maximum server execution time) and
 *                  explain (return information on the processing of the pipeline).
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
   mongoc_server_description_t *selected_server;
   mongoc_cursor_t *cursor;
   bson_iter_t iter;
   bson_t command;
   bson_t child;
   int32_t batch_size = 0;
   bool use_cursor;

   ENTRY;

   BSON_ASSERT (collection);
   BSON_ASSERT (pipeline);

   bson_init (&command);

   if (!read_prefs) {
      read_prefs = collection->read_prefs;
   }

   cursor = _mongoc_collection_cursor_new (collection, flags);
   selected_server = mongoc_topology_select(collection->client->topology,
                                            MONGOC_SS_READ,
                                            read_prefs,
                                            &cursor->error);

   if (!selected_server) {
      GOTO (done);
   }

   cursor->server_id = selected_server->id;
   use_cursor = 
      selected_server->max_wire_version >= WIRE_VERSION_AGG_CURSOR;

   BSON_APPEND_UTF8 (&command, "aggregate", collection->collection);

   /*
    * The following will allow @pipeline to be either an array of
    * items for the pipeline, or {"pipeline": [...]}.
    */
   if (bson_iter_init_find (&iter, pipeline, "pipeline") &&
       BSON_ITER_HOLDS_ARRAY (&iter)) {
      if (!bson_append_iter (&command, "pipeline", 8, &iter)) {
         bson_set_error (&cursor->error,
               MONGOC_ERROR_COMMAND,
               MONGOC_ERROR_COMMAND_INVALID_ARG,
               _("Failed to append \"pipeline\" to create command."));
         GOTO (done);
      }
   } else {
      BSON_APPEND_ARRAY (&command, "pipeline", pipeline);
   }

   /* for newer version, we include a cursor subdocument */
   if (use_cursor) {
      bson_append_document_begin (&command, "cursor", 6, &child);

      if (options && bson_iter_init (&iter, options)) {
         while (bson_iter_next (&iter)) {
            if (BSON_ITER_IS_KEY (&iter, "batchSize") &&
                (BSON_ITER_HOLDS_INT32 (&iter) ||
                 BSON_ITER_HOLDS_INT64 (&iter) ||
                 BSON_ITER_HOLDS_DOUBLE (&iter))) {
               batch_size = (int32_t)bson_iter_as_int64 (&iter);
               BSON_APPEND_INT32 (&child, "batchSize", batch_size);
            }
         }
      }

      bson_append_document_end (&command, &child);
   }

   if (options && bson_iter_init (&iter, options)) {
      while (bson_iter_next (&iter)) {
         if (! (BSON_ITER_IS_KEY (&iter, "batchSize") || BSON_ITER_IS_KEY (&iter, "cursor"))) {
            if (!bson_append_iter (&command, bson_iter_key (&iter), -1, &iter)) {
               bson_set_error (&cursor->error,
                     MONGOC_ERROR_COMMAND,
                     MONGOC_ERROR_COMMAND_INVALID_ARG,
                     _("Failed to append \"batchSize\" or \"cursor\" to create command."));
               GOTO (done);
            }
         }
      }
   }

   if (collection->read_concern->level != NULL) {
      const bson_t *read_concern_bson;

      if (selected_server->max_wire_version < WIRE_VERSION_READ_CONCERN) {
         bson_set_error (&cursor->error,
                         MONGOC_ERROR_COMMAND,
                         MONGOC_ERROR_PROTOCOL_BAD_WIRE_VERSION,
                         _("The selected server does not support readConcern"));
         GOTO (done);
      }

      read_concern_bson = _mongoc_read_concern_get_bson (collection->read_concern);
      BSON_APPEND_DOCUMENT (&command, "readConcern", read_concern_bson);
   }

   if (use_cursor) {
      _mongoc_cursor_cursorid_init (cursor, &command);
   } else {
      /* for older versions we get an array that we can create a synthetic
       * cursor on top of */
      _mongoc_cursor_array_init (cursor, &command, "result");
   }

done:
   if (selected_server) {
      mongoc_server_description_destroy (selected_server);
   }

   bson_destroy(&command);

   /* we always return the cursor, even if it fails; users can detect the
    * failure on performing a cursor operation. see CDRIVER-880. */
   RETURN (cursor);
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
   BSON_ASSERT (collection);
   BSON_ASSERT (query);

   bson_clear (&collection->gle);

   if (!read_prefs) {
      read_prefs = collection->read_prefs;
   }

   return _mongoc_cursor_new (collection->client, collection->ns, flags, skip,
                              limit, batch_size, false, query, fields, read_prefs,
                              collection->read_concern);
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
                         int64_t                    skip,        /* IN */
                         int64_t                    limit,       /* IN */
                         const mongoc_read_prefs_t *read_prefs,  /* IN */
                         bson_error_t              *error)       /* OUT */
{
   return mongoc_collection_count_with_opts (
      collection,
      flags,
      query,
      skip,
      limit,
      NULL,
      read_prefs,
      error);
}


int64_t
mongoc_collection_count_with_opts (mongoc_collection_t       *collection,  /* IN */
                                   mongoc_query_flags_t       flags,       /* IN */
                                   const bson_t              *query,       /* IN */
                                   int64_t                    skip,        /* IN */
                                   int64_t                    limit,       /* IN */
                                   const bson_t               *opts,       /* IN */
                                   const mongoc_read_prefs_t *read_prefs,  /* IN */
                                   bson_error_t              *error)       /* OUT */
{
   mongoc_server_stream_t *server_stream;
   mongoc_cluster_t *cluster;
   bson_iter_t iter;
   int64_t ret = -1;
   bool success;
   bson_t reply;
   bson_t cmd;
   bson_t q;

   ENTRY;


   cluster = &collection->client->cluster;
   server_stream = mongoc_cluster_stream_for_writes (cluster, error);
   if (!server_stream) {
      RETURN (-1);
   }

   BSON_ASSERT (collection);

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
   if (collection->read_concern->level != NULL) {
      const bson_t *read_concern_bson;

      if (server_stream->sd->max_wire_version < WIRE_VERSION_READ_CONCERN) {
         bson_set_error (error,
                         MONGOC_ERROR_COMMAND,
                         MONGOC_ERROR_PROTOCOL_BAD_WIRE_VERSION,
                         _("The selected server does not support readConcern"));
         bson_destroy (&cmd);
         mongoc_server_stream_cleanup (server_stream);
         RETURN (-1);
      }

      read_concern_bson = _mongoc_read_concern_get_bson (collection->read_concern);
      BSON_APPEND_DOCUMENT (&cmd, "readConcern", read_concern_bson);
   }
   if (opts) {
       bson_concat(&cmd, opts);
   }

   success = mongoc_cluster_run_command_monitored (cluster,
                                                   server_stream,
                                                   MONGOC_QUERY_SLAVE_OK,
                                                   collection->db,
                                                   &cmd, &reply, error);

   if (success && bson_iter_init_find(&iter, &reply, "n")) {
      ret = bson_iter_as_int64(&iter);
   }
   bson_destroy (&reply);
   bson_destroy (&cmd);
   mongoc_server_stream_cleanup (server_stream);

   RETURN (ret);
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

   BSON_ASSERT (collection);

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

   BSON_ASSERT (collection);
   BSON_ASSERT (index_name);

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

      /* Index type can be specified as a string ("2d") or as an integer representing direction */
      if (bson_iter_type(&iter) == BSON_TYPE_UTF8)
      {
          bson_string_append_printf (s,
                                     (i++ ? "_%s_%s" : "%s_%s"),
                                     bson_iter_key (&iter),
                                     bson_iter_utf8 (&iter, NULL));
      }
      else
      {

          bson_string_append_printf (s,
                                     (i++ ? "_%s_%d" : "%s_%d"),
                                     bson_iter_key (&iter),
                                     bson_iter_int32 (&iter));
      }

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

   BSON_ASSERT (collection);

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
      if (!name) {
         bson_set_error (error,
                         MONGOC_ERROR_BSON,
                         MONGOC_ERROR_BSON_INVALID,
                         _("Cannot generate index name from invalid `keys` argument")
                         );
         bson_destroy (&insert);
         return false;
      }
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
                        "default_language", -1,
                        opt->default_language, -1);
   }

   if (opt->language_override != def_opt->language_override) {
      bson_append_utf8 (&insert,
                        "language_override", -1,
                        opt->language_override, -1);
   }

   col = mongoc_client_get_collection (collection->client, collection->db,
                                       "system.indexes");

   ret = mongoc_collection_insert (col,
                                  (mongoc_insert_flags_t)MONGOC_INSERT_NO_VALIDATE,
                                  &insert, NULL, error);

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
   const mongoc_index_opt_geo_t *def_geo;
   bson_error_t local_error;
   const char *name;
   bson_t cmd = BSON_INITIALIZER;
   bson_t ar;
   bson_t doc;
   bson_t reply;
   bson_t storage_doc;
   bson_t wt_doc;
   const mongoc_index_opt_geo_t *geo_opt;
   const mongoc_index_opt_storage_t *storage_opt;
   const mongoc_index_opt_wt_t *wt_opt;
   char *alloc_name = NULL;
   bool ret = false;

   BSON_ASSERT (collection);
   BSON_ASSERT (keys);

   def_opt = mongoc_index_opt_get_default ();
   opt = opt ? opt : def_opt;

   /*
    * Generate the key name if it was not provided.
    */
   name = (opt->name != def_opt->name) ? opt->name : NULL;
   if (!name) {
      alloc_name = mongoc_collection_keys_to_index_string (keys);
      if (alloc_name) {
         name = alloc_name;
      } else {
         bson_set_error (error,
                         MONGOC_ERROR_BSON,
                         MONGOC_ERROR_BSON_INVALID,
                         _("Cannot generate index name from invalid `keys` argument")
                         );
         bson_destroy (&cmd);
         return false;
      }
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
   if (opt->partial_filter_expression) {
      BSON_APPEND_DOCUMENT (&doc, "partialFilterExpression", opt->partial_filter_expression);
   }
   if (opt->geo_options) {
       geo_opt = opt->geo_options;
       def_geo = mongoc_index_opt_geo_get_default ();
       if (geo_opt->twod_sphere_version != def_geo->twod_sphere_version) {
          BSON_APPEND_INT32 (&doc, "2dsphereIndexVersion", geo_opt->twod_sphere_version);
       }
       if (geo_opt->twod_bits_precision != def_geo->twod_bits_precision) {
          BSON_APPEND_INT32 (&doc, "bits", geo_opt->twod_bits_precision);
       }
       if (geo_opt->twod_location_min != def_geo->twod_location_min) {
          BSON_APPEND_DOUBLE (&doc, "min", geo_opt->twod_location_min);
       }
       if (geo_opt->twod_location_max != def_geo->twod_location_max) {
          BSON_APPEND_DOUBLE (&doc, "max", geo_opt->twod_location_max);
       }
       if (geo_opt->haystack_bucket_size != def_geo->haystack_bucket_size) {
          BSON_APPEND_DOUBLE (&doc, "bucketSize", geo_opt->haystack_bucket_size);
       }
   }

   if (opt->storage_options) {
      storage_opt = opt->storage_options;
      switch (storage_opt->type) {
      case MONGOC_INDEX_STORAGE_OPT_WIREDTIGER:
         wt_opt = (mongoc_index_opt_wt_t *)storage_opt;
         BSON_APPEND_DOCUMENT_BEGIN (&doc, "storageEngine", &storage_doc);
         BSON_APPEND_DOCUMENT_BEGIN (&storage_doc, "wiredTiger", &wt_doc);
         BSON_APPEND_UTF8 (&wt_doc, "configString", wt_opt->config_str);
         bson_append_document_end (&storage_doc, &wt_doc);
         bson_append_document_end (&doc, &storage_doc);
         break;
      default:
         break;
      }
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

mongoc_cursor_t *
_mongoc_collection_find_indexes_legacy (mongoc_collection_t *collection,
                                        bson_error_t        *error)
{
   mongoc_database_t *db;
   mongoc_collection_t *idx_collection;
   mongoc_read_prefs_t *read_prefs;
   bson_t query = BSON_INITIALIZER;
   mongoc_cursor_t *cursor;

   BSON_ASSERT (collection);

   BSON_APPEND_UTF8 (&query, "ns", collection->ns);

   db = mongoc_client_get_database (collection->client, collection->db);
   BSON_ASSERT (db);

   idx_collection = mongoc_database_get_collection (db, "system.indexes");
   BSON_ASSERT (idx_collection);

   read_prefs = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);

   cursor = mongoc_collection_find (idx_collection, MONGOC_QUERY_NONE, 0, 0, 0,
                                    &query, NULL, read_prefs);

   mongoc_read_prefs_destroy (read_prefs);
   mongoc_collection_destroy (idx_collection);
   mongoc_database_destroy (db);

   return cursor;
}

mongoc_cursor_t *
mongoc_collection_find_indexes (mongoc_collection_t *collection,
                                bson_error_t        *error)
{
   mongoc_cursor_t *cursor;
   bson_t cmd = BSON_INITIALIZER;
   bson_t child;

   BSON_ASSERT (collection);

   bson_append_utf8 (&cmd, "listIndexes", -1, collection->collection,
                     collection->collectionlen);

   BSON_APPEND_DOCUMENT_BEGIN (&cmd, "cursor", &child);
   bson_append_document_end (&cmd, &child);

   cursor = _mongoc_collection_cursor_new (collection, MONGOC_QUERY_SLAVE_OK);
   _mongoc_cursor_cursorid_init (cursor, &cmd);

   if (_mongoc_cursor_cursorid_prime (cursor)) {
       /* intentionally empty */
   } else {
      if (mongoc_cursor_error (cursor, error)) {
         mongoc_cursor_destroy (cursor);

         if (error->code == MONGOC_ERROR_COLLECTION_DOES_NOT_EXIST) {
            bson_t empty_arr = BSON_INITIALIZER;
            /* collection does not exist. in accordance with the spec we return
             * an empty array. Also we need to clear out the error. */
            error->code = 0;
            error->domain = 0;
            cursor = _mongoc_collection_cursor_new (collection,
                                                    MONGOC_QUERY_SLAVE_OK);

            _mongoc_cursor_array_init (cursor, NULL, NULL);
            _mongoc_cursor_array_set_bson (cursor, &empty_arr);
         } else if (error->code == MONGOC_ERROR_QUERY_COMMAND_NOT_FOUND) {
            /* talking to an old server. */
            /* clear out error. */
            error->code = 0;
            error->domain = 0;
            cursor = _mongoc_collection_find_indexes_legacy (collection, error);
         }
      }
   }

   bson_destroy (&cmd);

   return cursor;
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
   mongoc_bulk_write_flags_t write_flags = MONGOC_BULK_WRITE_FLAGS_INIT;
   bool ret;
   uint32_t i;

   BSON_ASSERT (collection);
   BSON_ASSERT (documents);

   if (!write_concern) {
      write_concern = collection->write_concern;
   }

   if (!(flags & MONGOC_INSERT_NO_VALIDATE)) {
      int vflags = (BSON_VALIDATE_UTF8 | BSON_VALIDATE_UTF8_ALLOW_NULL
                  | BSON_VALIDATE_DOLLAR_KEYS | BSON_VALIDATE_DOT_KEYS);

      for (i = 0; i < n_documents; i++) {
         if (!bson_validate (documents[i], (bson_validate_flags_t)vflags, NULL)) {
            bson_set_error (error,
                            MONGOC_ERROR_BSON,
                            MONGOC_ERROR_BSON_INVALID,
                            _("A document was corrupt or contained "
                            "invalid characters . or $"));
            RETURN (false);
         }
      }
   }

   bson_clear (&collection->gle);

   _mongoc_write_result_init (&result);

   write_flags.ordered = !(flags & MONGOC_INSERT_CONTINUE_ON_ERROR);

   _mongoc_write_command_init_insert (&command, NULL, write_flags,
                                      ++collection->client->cluster.operation_id,
                                      true);

   for (i = 0; i < n_documents; i++) {
      _mongoc_write_command_insert_append (&command, documents[i]);
   }

   _mongoc_collection_write_command_execute (&command, collection,
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
   mongoc_bulk_write_flags_t write_flags = MONGOC_BULK_WRITE_FLAGS_INIT;
   mongoc_write_command_t command;
   mongoc_write_result_t result;
   bool ret;

   ENTRY;

   BSON_ASSERT (collection);
   BSON_ASSERT (document);

   bson_clear (&collection->gle);

   if (!write_concern) {
      write_concern = collection->write_concern;
   }

   if (!(flags & MONGOC_INSERT_NO_VALIDATE)) {
      int vflags = (BSON_VALIDATE_UTF8 | BSON_VALIDATE_UTF8_ALLOW_NULL
                  | BSON_VALIDATE_DOLLAR_KEYS | BSON_VALIDATE_DOT_KEYS);

      if (!bson_validate (document, (bson_validate_flags_t)vflags, NULL)) {
         bson_set_error (error,
                         MONGOC_ERROR_BSON,
                         MONGOC_ERROR_BSON_INVALID,
                         _("A document was corrupt or contained "
                         "invalid characters . or $"));
         RETURN (false);
      }
   }

   _mongoc_write_result_init (&result);
   _mongoc_write_command_init_insert (&command, document, write_flags,
                                      ++collection->client->cluster.operation_id,
                                      false);

   _mongoc_collection_write_command_execute (&command, collection,
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
                          mongoc_update_flags_t         uflags,
                          const bson_t                 *selector,
                          const bson_t                 *update,
                          const mongoc_write_concern_t *write_concern,
                          bson_error_t                 *error)
{
   mongoc_bulk_write_flags_t write_flags = MONGOC_BULK_WRITE_FLAGS_INIT;
   mongoc_write_command_t command;
   mongoc_write_result_t result;
   bson_iter_t iter;
   size_t err_offset;
   bool ret;
   int vflags = (BSON_VALIDATE_UTF8 | BSON_VALIDATE_UTF8_ALLOW_NULL
               | BSON_VALIDATE_DOLLAR_KEYS | BSON_VALIDATE_DOT_KEYS);
   int flags = uflags;

   ENTRY;

   BSON_ASSERT (collection);
   BSON_ASSERT (selector);
   BSON_ASSERT (update);

   bson_clear (&collection->gle);

   if (!write_concern) {
      write_concern = collection->write_concern;
   }

   if (!((uint32_t)flags & MONGOC_UPDATE_NO_VALIDATE) &&
       bson_iter_init (&iter, update) &&
       bson_iter_next (&iter) &&
       (bson_iter_key (&iter) [0] != '$') &&
       !bson_validate (update, (bson_validate_flags_t)vflags, &err_offset)) {
      bson_set_error (error,
                      MONGOC_ERROR_BSON,
                      MONGOC_ERROR_BSON_INVALID,
                      _("update document is corrupt or contains "
                      "invalid keys including $ or ."));
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
                                      write_flags,
                                      ++collection->client->cluster.operation_id);

   _mongoc_collection_write_command_execute (&command, collection,
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

   BSON_ASSERT (collection);
   BSON_ASSERT (document);

   if (!bson_iter_init_find(&iter, document, "_id")) {
      return mongoc_collection_insert(collection,
                                      MONGOC_INSERT_NONE,
                                      document,
                                      write_concern,
                                      error);
   }

   bson_init(&selector);
   if (!bson_append_iter(&selector, NULL, 0, &iter)) {
      bson_set_error (error,
            MONGOC_ERROR_COMMAND,
            MONGOC_ERROR_COMMAND_INVALID_ARG,
            _("Failed to append bson to create update."));
      bson_destroy (&selector);
      return NULL;
   }

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
 *       limit to a single delete, provided MONGOC_REMOVE_SINGLE_REMOVE
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
   mongoc_bulk_write_flags_t write_flags = MONGOC_BULK_WRITE_FLAGS_INIT;
   mongoc_write_command_t command;
   mongoc_write_result_t result;
   bool multi;
   bool ret;

   ENTRY;

   BSON_ASSERT (collection);
   BSON_ASSERT (selector);

   bson_clear (&collection->gle);

   if (!write_concern) {
      write_concern = collection->write_concern;
   }

   multi = !(flags & MONGOC_REMOVE_SINGLE_REMOVE);

   _mongoc_write_result_init (&result);
   _mongoc_write_command_init_delete (&command, selector, multi, write_flags,
                                      ++collection->client->cluster.operation_id);

   _mongoc_collection_write_command_execute (&command, collection,
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
   BSON_ASSERT (collection);
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
   BSON_ASSERT (collection);

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
 * mongoc_collection_get_read_concern --
 *
 *       Fetches the default read concern for the collection instance.
 *
 * Returns:
 *       A mongoc_read_concern_t that should not be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const mongoc_read_concern_t *
mongoc_collection_get_read_concern (const mongoc_collection_t *collection)
{
   BSON_ASSERT (collection);

   return collection->read_concern;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_set_read_concern --
 *
 *       Sets the default read concern for the collection instance.
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
mongoc_collection_set_read_concern (mongoc_collection_t          *collection,
                                    const mongoc_read_concern_t *read_concern)
{
   BSON_ASSERT (collection);

   if (collection->read_concern) {
      mongoc_read_concern_destroy (collection->read_concern);
      collection->read_concern = NULL;
   }

   if (read_concern) {
      collection->read_concern = mongoc_read_concern_copy (read_concern);
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
   BSON_ASSERT (collection);

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
   BSON_ASSERT (collection);

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
   BSON_ASSERT (collection);

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

   BSON_ASSERT (collection);

   if (options &&
       bson_iter_init_find (&iter, options, "full") &&
       !BSON_ITER_HOLDS_BOOL (&iter)) {
      bson_set_error (error,
                      MONGOC_ERROR_BSON,
                      MONGOC_ERROR_BSON_INVALID,
                      _("'full' must be a boolean value."));
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

   BSON_ASSERT (collection);
   BSON_ASSERT (new_name);

   if (!validate_name (new_name)) {
      bson_set_error (error,
                      MONGOC_ERROR_NAMESPACE,
                      MONGOC_ERROR_NAMESPACE_INVALID,
                      _("\"%s\" is an invalid collection name."),
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

   BSON_ASSERT (collection);

   if (options &&
       bson_iter_init_find (&iter, options, "scale") &&
       !BSON_ITER_HOLDS_INT32 (&iter)) {
      bson_set_error (error,
                      MONGOC_ERROR_BSON,
                      MONGOC_ERROR_BSON_INVALID,
                      _("'scale' must be an int32 value."));
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
   mongoc_bulk_write_flags_t write_flags = MONGOC_BULK_WRITE_FLAGS_INIT;
   BSON_ASSERT (collection);

   if (!write_concern) {
      write_concern = collection->write_concern;
   }

   write_flags.ordered = ordered;

   return _mongoc_bulk_operation_new (collection->client,
                                      collection->db,
                                      collection->collection,
                                      write_flags,
                                      write_concern);
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_collection_find_and_modify_with_opts --
 *
 *       Find a document in @collection matching @query, applying @opts.
 *
 *       If @reply is not NULL, then the result document will be placed
 *       in reply and should be released with bson_destroy().
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
mongoc_collection_find_and_modify_with_opts (mongoc_collection_t                 *collection,
                                             const bson_t                        *query,
                                             const mongoc_find_and_modify_opts_t *opts,
                                             bson_t                              *reply,
                                             bson_error_t                        *error)
{
   mongoc_cluster_t *cluster;
   mongoc_server_stream_t *server_stream;
   bson_iter_t iter;
   bson_iter_t inner;
   const char *name;
   bson_t reply_local;
   bool ret;
   bson_t command = BSON_INITIALIZER;

   ENTRY;

   BSON_ASSERT (collection);
   BSON_ASSERT (query);


   cluster = &collection->client->cluster;
   server_stream = mongoc_cluster_stream_for_writes (cluster, error);
   if (!server_stream) {
      bson_destroy (&command);
      RETURN (false);
   }

   name = mongoc_collection_get_name (collection);
   BSON_APPEND_UTF8 (&command, "findAndModify", name);
   BSON_APPEND_DOCUMENT (&command, "query", query);

   if (opts->sort) {
      BSON_APPEND_DOCUMENT (&command, "sort", opts->sort);
   }

   if (opts->update) {
      BSON_APPEND_DOCUMENT (&command, "update", opts->update);
   }

   if (opts->fields) {
      BSON_APPEND_DOCUMENT (&command, "fields", opts->fields);
   }

   if (opts->flags & MONGOC_FIND_AND_MODIFY_REMOVE) {
      BSON_APPEND_BOOL (&command, "remove", true);
   }

   if (opts->flags & MONGOC_FIND_AND_MODIFY_UPSERT) {
      BSON_APPEND_BOOL (&command, "upsert", true);
   }

   if (opts->flags & MONGOC_FIND_AND_MODIFY_RETURN_NEW) {
      BSON_APPEND_BOOL (&command, "new", true);
   }

   if (opts->bypass_document_validation != MONGOC_BYPASS_DOCUMENT_VALIDATION_DEFAULT) {
      BSON_APPEND_BOOL (&command, "bypassDocumentValidation",
                        !!opts->bypass_document_validation);
   }

   if (server_stream->sd->max_wire_version >= WIRE_VERSION_FAM_WRITE_CONCERN) {
      if (!mongoc_write_concern_is_valid (collection->write_concern)) {
         bson_set_error (error,
                         MONGOC_ERROR_COMMAND,
                         MONGOC_ERROR_COMMAND_INVALID_ARG,
                         _("The write concern is invalid."));
         bson_destroy (&command);
         mongoc_server_stream_cleanup (server_stream);
         RETURN (false);
      }

      if (mongoc_write_concern_is_acknowledged (collection->write_concern)) {
         _BSON_APPEND_WRITE_CONCERN (&command, collection->write_concern);
      }
   }

   ret = mongoc_cluster_run_command_monitored (cluster, server_stream,
                                               MONGOC_QUERY_NONE,
                                               collection->db, &command,
                                               &reply_local, error);

   if (bson_iter_init_find (&iter, &reply_local, "writeConcernError") &&
         BSON_ITER_HOLDS_DOCUMENT (&iter)) {
      const char *errmsg = NULL;
      int32_t code = 0;

      bson_iter_recurse(&iter, &inner);
      while (bson_iter_next (&inner)) {
         if (BSON_ITER_IS_KEY (&inner, "code")) {
            code = bson_iter_int32 (&inner);
         } else if (BSON_ITER_IS_KEY (&inner, "errmsg")) {
            errmsg = bson_iter_utf8 (&inner, NULL);
         }
      }
      bson_set_error (error, MONGOC_ERROR_WRITE_CONCERN, code, _("Write Concern error: %s"), errmsg);
   }
   if (reply) {
      bson_copy_to (&reply_local, reply);
   }

   bson_destroy (&reply_local);
   bson_destroy (&command);
   mongoc_server_stream_cleanup (server_stream);

   RETURN (ret);
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
   mongoc_find_and_modify_opts_t *opts;
   int flags = 0;
   bool ret;

   ENTRY;

   BSON_ASSERT (collection);
   BSON_ASSERT (query);
   BSON_ASSERT (update || _remove);


   if (_remove) {
      flags |= MONGOC_FIND_AND_MODIFY_REMOVE;
   }
   if (upsert) {
      flags |= MONGOC_FIND_AND_MODIFY_UPSERT;
   }
   if (_new) {
      flags |= MONGOC_FIND_AND_MODIFY_RETURN_NEW;
   }

   opts = mongoc_find_and_modify_opts_new ();

   mongoc_find_and_modify_opts_set_sort (opts, sort);
   mongoc_find_and_modify_opts_set_update (opts, update);
   mongoc_find_and_modify_opts_set_fields (opts, fields);
   mongoc_find_and_modify_opts_set_flags (opts, flags);

   ret = mongoc_collection_find_and_modify_with_opts (collection, query, opts, reply, error);
   mongoc_find_and_modify_opts_destroy (opts);

   return ret;
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

/* --- mongoc-counters.defs */

COUNTER(op_egress_total,        "Operations",   "Egress Total",        "The number of sent operations.")
COUNTER(op_ingress_total,       "Operations",   "Ingress Total",       "The number of received operations.")
COUNTER(op_egress_query,        "Operations",   "Egress Queries",      "The number of sent Query operations.")
COUNTER(op_ingress_query,       "Operations",   "Ingress Queries",     "The number of received Query operations.")
COUNTER(op_egress_getmore,      "Operations",   "Egress GetMore",      "The number of sent GetMore operations.")
COUNTER(op_ingress_getmore,     "Operations",   "Ingress GetMore",     "The number of received GetMore operations.")
COUNTER(op_egress_insert,       "Operations",   "Egress Insert",       "The number of sent Insert operations.")
COUNTER(op_ingress_insert,      "Operations",   "Ingress Insert",      "The number of received Insert operations.")
COUNTER(op_egress_delete,       "Operations",   "Egress Delete",       "The number of sent Delete operations.")
COUNTER(op_ingress_delete,      "Operations",   "Ingress Delete",      "The number of received Delete operations.")
COUNTER(op_egress_update,       "Operations",   "Egress Update",       "The number of sent Update operations.")
COUNTER(op_ingress_update,      "Operations",   "Ingress Update",      "The number of received Update operations.")
COUNTER(op_egress_killcursors,  "Operations",   "Egress KillCursors",  "The number of sent KillCursors operations.")
COUNTER(op_ingress_killcursors, "Operations",   "Ingress KillCursors", "The number of received KillCursors operations.")
COUNTER(op_egress_msg,          "Operations",   "Egress Msg",          "The number of sent Msg operations.")
COUNTER(op_ingress_msg,         "Operations",   "Ingress Msg",         "The number of received Msg operations.")
COUNTER(op_egress_reply,        "Operations",   "Egress Reply",        "The number of sent Reply operations.")
COUNTER(op_ingress_reply,       "Operations",   "Ingress Reply",       "The number of received Reply operations.")


COUNTER(cursors_active,         "Cursors",      "Active",              "The number of active cursors.")
COUNTER(cursors_disposed,       "Cursors",      "Disposed",            "The number of disposed cursors.")


COUNTER(clients_active,         "Clients",      "Active",              "The number of active clients.")
COUNTER(clients_disposed,       "Clients",      "Disposed",            "The number of disposed clients.")


COUNTER(streams_active,         "Streams",      "Active",              "The number of active streams.")
COUNTER(streams_disposed,       "Streams",      "Disposed",            "The number of disposed streams.")
COUNTER(streams_egress,         "Streams",      "Egress Bytes",        "The number of bytes sent.")
COUNTER(streams_ingress,        "Streams",      "Ingress Bytes",       "The number of bytes received.")
COUNTER(streams_timeout,        "Streams",      "N Socket Timeouts",   "The number of socket timeouts.")


COUNTER(client_pools_active,    "Client Pools", "Active",              "The number of active client pools.")
COUNTER(client_pools_disposed,  "Client Pools", "Disposed",            "The number of disposed client pools.")


COUNTER(protocol_ingress_error, "Protocol",     "Ingress Errors",      "The number of protocol errors on ingress.")


COUNTER(auth_failure,           "Auth",         "Failures",            "The number of failed authentication requests.")
COUNTER(auth_success,           "Auth",         "Success",             "The number of successful authentication requests.")


COUNTER(dns_failure,            "DNS",          "Failure",             "The number of failed DNS requests.")
COUNTER(dns_success,            "DNS",          "Success",             "The number of successful DNS requests.")

#undef COUNTER


/**
 * mongoc_counters_use_shm:
 *
 * Checks to see if counters should be exported over a shared memory segment.
 *
 * Returns: true if SHM is to be used.
 */
#if defined(BSON_OS_UNIX) && defined(MONGOC_ENABLE_SHM_COUNTERS)
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
   return BSON_MAX(getpagesize(), size);
#else
   return size;
#endif
}


/**
 * mongoc_counters_destroy:
 *
 * Removes the shared memory segment for the current processes counters.
 */
void
_mongoc_counters_cleanup (void)
{
   if (gCounterFallback) {
      bson_free (gCounterFallback);
      gCounterFallback = NULL;
#if defined(BSON_OS_UNIX) && defined(MONGOC_ENABLE_SHM_COUNTERS)
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
#if defined(BSON_OS_UNIX) && defined(MONGOC_ENABLE_SHM_COUNTERS)
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

   return mem;

failure:
   shm_unlink (name);
   close (fd);

use_malloc:
   MONGOC_WARNING("Falling back to malloc for counters.");
#endif

   gCounterFallback = (void *)bson_malloc0 (size);

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
   segment = (char *)mongoc_counters_alloc(size);
   infos_size = LAST_COUNTER * sizeof *info;

   counters = (mongoc_counters_t *)segment;
   counters->n_cpu = _mongoc_get_cpu_count();
   counters->n_counters = 0;
   counters->infos_offset = sizeof *counters;
   counters->values_offset = (uint32_t)(counters->infos_offset + infos_size);

   BSON_ASSERT ((counters->values_offset % 64) == 0);

#define COUNTER(ident, Category, Name, Desc) \
   off = mongoc_counters_register(counters, COUNTER_##ident, Category, Name, Desc); \
   __mongoc_counter_##ident.cpus = (mongoc_counter_slots_t *)(segment + off);

COUNTER(op_egress_total,        "Operations",   "Egress Total",        "The number of sent operations.")
COUNTER(op_ingress_total,       "Operations",   "Ingress Total",       "The number of received operations.")
COUNTER(op_egress_query,        "Operations",   "Egress Queries",      "The number of sent Query operations.")
COUNTER(op_ingress_query,       "Operations",   "Ingress Queries",     "The number of received Query operations.")
COUNTER(op_egress_getmore,      "Operations",   "Egress GetMore",      "The number of sent GetMore operations.")
COUNTER(op_ingress_getmore,     "Operations",   "Ingress GetMore",     "The number of received GetMore operations.")
COUNTER(op_egress_insert,       "Operations",   "Egress Insert",       "The number of sent Insert operations.")
COUNTER(op_ingress_insert,      "Operations",   "Ingress Insert",      "The number of received Insert operations.")
COUNTER(op_egress_delete,       "Operations",   "Egress Delete",       "The number of sent Delete operations.")
COUNTER(op_ingress_delete,      "Operations",   "Ingress Delete",      "The number of received Delete operations.")
COUNTER(op_egress_update,       "Operations",   "Egress Update",       "The number of sent Update operations.")
COUNTER(op_ingress_update,      "Operations",   "Ingress Update",      "The number of received Update operations.")
COUNTER(op_egress_killcursors,  "Operations",   "Egress KillCursors",  "The number of sent KillCursors operations.")
COUNTER(op_ingress_killcursors, "Operations",   "Ingress KillCursors", "The number of received KillCursors operations.")
COUNTER(op_egress_msg,          "Operations",   "Egress Msg",          "The number of sent Msg operations.")
COUNTER(op_ingress_msg,         "Operations",   "Ingress Msg",         "The number of received Msg operations.")
COUNTER(op_egress_reply,        "Operations",   "Egress Reply",        "The number of sent Reply operations.")
COUNTER(op_ingress_reply,       "Operations",   "Ingress Reply",       "The number of received Reply operations.")


COUNTER(cursors_active,         "Cursors",      "Active",              "The number of active cursors.")
COUNTER(cursors_disposed,       "Cursors",      "Disposed",            "The number of disposed cursors.")


COUNTER(clients_active,         "Clients",      "Active",              "The number of active clients.")
COUNTER(clients_disposed,       "Clients",      "Disposed",            "The number of disposed clients.")


COUNTER(streams_active,         "Streams",      "Active",              "The number of active streams.")
COUNTER(streams_disposed,       "Streams",      "Disposed",            "The number of disposed streams.")
COUNTER(streams_egress,         "Streams",      "Egress Bytes",        "The number of bytes sent.")
COUNTER(streams_ingress,        "Streams",      "Ingress Bytes",       "The number of bytes received.")
COUNTER(streams_timeout,        "Streams",      "N Socket Timeouts",   "The number of socket timeouts.")


COUNTER(client_pools_active,    "Client Pools", "Active",              "The number of active client pools.")
COUNTER(client_pools_disposed,  "Client Pools", "Disposed",            "The number of disposed client pools.")


COUNTER(protocol_ingress_error, "Protocol",     "Ingress Errors",      "The number of protocol errors on ingress.")


COUNTER(auth_failure,           "Auth",         "Failures",            "The number of failed authentication requests.")
COUNTER(auth_success,           "Auth",         "Success",             "The number of successful authentication requests.")


COUNTER(dns_failure,            "DNS",          "Failure",             "The number of failed DNS requests.")
COUNTER(dns_success,            "DNS",          "Success",             "The number of successful DNS requests.")

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
   bson_t         array;
   bool           has_array;
   bool           has_synthetic_bson;
   bson_iter_t    iter;
   bson_t         bson;   /* current document */
   const char    *field_name;
} mongoc_cursor_array_t;


static void *
_mongoc_cursor_array_new (const char *field_name)
{
   mongoc_cursor_array_t *arr;

   ENTRY;

   arr = (mongoc_cursor_array_t *)bson_malloc0 (sizeof *arr);
   arr->has_array = false;
   arr->has_synthetic_bson = false;
   arr->field_name = field_name;

   RETURN (arr);
}


static void
_mongoc_cursor_array_destroy (mongoc_cursor_t *cursor)
{
   mongoc_cursor_array_t *arr;

   ENTRY;

   arr = (mongoc_cursor_array_t *)cursor->iface_data;

   if (arr->has_array) {
      bson_destroy (&arr->array);
   }

   if (arr->has_synthetic_bson) {
      bson_destroy(&arr->bson);
   }

   bson_free (cursor->iface_data);
   _mongoc_cursor_destroy (cursor);

   EXIT;
}


bool
_mongoc_cursor_array_prime (mongoc_cursor_t *cursor)
{
   mongoc_cursor_array_t *arr;
   bson_iter_t iter;

   ENTRY;

   arr = (mongoc_cursor_array_t *)cursor->iface_data;

   BSON_ASSERT (arr);

   if (_mongoc_cursor_run_command (cursor, &cursor->query, &arr->array) &&
       bson_iter_init_find (&iter, &arr->array, arr->field_name) &&
       BSON_ITER_HOLDS_ARRAY (&iter) &&
       bson_iter_recurse (&iter, &arr->iter)) {
      arr->has_array = true;
      return true;
   }

   return false;
}


static bool
_mongoc_cursor_array_next (mongoc_cursor_t *cursor,
                           const bson_t   **bson)
{
   bool ret = true;
   mongoc_cursor_array_t *arr;
   uint32_t document_len;
   const uint8_t *document;

   ENTRY;

   arr = (mongoc_cursor_array_t *)cursor->iface_data;
   *bson = NULL;

   if (!arr->has_array && !arr->has_synthetic_bson) {
      ret = _mongoc_cursor_array_prime(cursor);
   }

   if (ret) {
      ret = bson_iter_next (&arr->iter);
   }

   if (ret) {
      bson_iter_document (&arr->iter, &document_len, &document);
      bson_init_static (&arr->bson, document, document_len);
      *bson = &arr->bson;
   }

   RETURN (ret);
}


static mongoc_cursor_t *
_mongoc_cursor_array_clone (const mongoc_cursor_t *cursor)
{
   mongoc_cursor_array_t *arr;
   mongoc_cursor_t *clone_;

   ENTRY;

   arr = (mongoc_cursor_array_t *)cursor->iface_data;

   clone_ = _mongoc_cursor_clone (cursor);
   _mongoc_cursor_array_init (clone_, &cursor->query, arr->field_name);

   RETURN (clone_);
}


static bool
_mongoc_cursor_array_more (mongoc_cursor_t *cursor)
{
   bool ret;
   mongoc_cursor_array_t *arr;
   bson_iter_t iter;

   ENTRY;

   arr = (mongoc_cursor_array_t *)cursor->iface_data;

   if (arr->has_array || arr->has_synthetic_bson) {
      memcpy (&iter, &arr->iter, sizeof iter);

      ret = bson_iter_next (&iter);
   } else {
      ret = true;
   }

   RETURN (ret);
}


static bool
_mongoc_cursor_array_error  (mongoc_cursor_t *cursor,
                             bson_error_t    *error)
{
   mongoc_cursor_array_t *arr;

   ENTRY;

   arr = (mongoc_cursor_array_t *)cursor->iface_data;

   if (arr->has_synthetic_bson) {
      return false;
   } else {
      return _mongoc_cursor_error(cursor, error);
   }
}


static mongoc_cursor_interface_t gMongocCursorArray = {
   _mongoc_cursor_array_clone,
   _mongoc_cursor_array_destroy,
   _mongoc_cursor_array_more,
   _mongoc_cursor_array_next,
   _mongoc_cursor_array_error,
};


void
_mongoc_cursor_array_init (mongoc_cursor_t *cursor,
                           const bson_t    *command,
                           const char      *field_name)
{
   ENTRY;


   if (command) {
      bson_destroy (&cursor->query);
      bson_copy_to (command, &cursor->query);
   }

   cursor->iface_data = _mongoc_cursor_array_new (field_name);

   memcpy (&cursor->iface, &gMongocCursorArray,
           sizeof (mongoc_cursor_interface_t));

   EXIT;
}


void
_mongoc_cursor_array_set_bson (mongoc_cursor_t *cursor,
                               const bson_t    *bson)
{
   mongoc_cursor_array_t *arr;

   ENTRY;

   arr = (mongoc_cursor_array_t *)cursor->iface_data;
   bson_copy_to(bson, &arr->bson);
   arr->has_synthetic_bson = true;
   bson_iter_init(&arr->iter, &arr->bson);

   EXIT;
}

/*==============================================================*/
/* --- mongoc-cursor.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "cursor"


#define CURSOR_FAILED(cursor_) ((cursor_)->error.domain != 0)

static const bson_t *
_mongoc_cursor_op_query (mongoc_cursor_t        *cursor,
                         mongoc_server_stream_t *server_stream);

static bool
_mongoc_cursor_prepare_find_command (mongoc_cursor_t *cursor,
                                     bson_t          *command);

static const bson_t *
_mongoc_cursor_find_command (mongoc_cursor_t *cursor);


static int32_t
_mongoc_n_return (mongoc_cursor_t * cursor)
{
   int64_t n_return;

   if (cursor->is_command) {
      /* commands always have n_return of 1 */
      return 1;
   } else if (cursor->limit < 0) {
      n_return = cursor->limit;
   } else if (cursor->limit) {
      int64_t remaining = cursor->limit - cursor->count;
      BSON_ASSERT (remaining > 0);

      if (cursor->batch_size) {
         n_return = BSON_MIN (cursor->batch_size, remaining);
      } else {
         /* batch_size 0 means accept the default */
         n_return = remaining;
      }
   } else {
      n_return = cursor->batch_size;
   }

   if (n_return < INT32_MIN) {
      return INT32_MIN;
   } else if (n_return > INT32_MAX) {
      return INT32_MAX;
   } else {
      return (int32_t) n_return;
   }
}


void
_mongoc_set_cursor_ns (mongoc_cursor_t *cursor,
                       const char      *ns,
                       uint32_t         nslen)
{
   const char *dot;

   bson_strncpy (cursor->ns, ns, sizeof cursor->ns);
   cursor->nslen = BSON_MIN (nslen, sizeof cursor->ns);
   dot = strstr (cursor->ns, ".");

   if (dot) {
      cursor->dblen = (uint32_t) (dot - cursor->ns);
   } else {
      /* a database name with no collection name */
      cursor->dblen = cursor->nslen;
   }
}


mongoc_cursor_t *
_mongoc_cursor_new (mongoc_client_t           *client,
                    const char                *db_and_collection,
                    mongoc_query_flags_t       qflags,
                    uint32_t                   skip,
                    int32_t                    limit,
                    uint32_t                   batch_size,
                    bool                       is_command,
                    const bson_t              *query,
                    const bson_t              *fields,
                    const mongoc_read_prefs_t *read_prefs,
                    const mongoc_read_concern_t *read_concern)
{
   mongoc_cursor_t *cursor;
   bson_iter_t iter;
   int flags = qflags;

   ENTRY;

   BSON_ASSERT (client);

   if (!read_concern) {
      read_concern = client->read_concern;
   }
   if (!read_prefs) {
      read_prefs = client->read_prefs;
   }

   cursor = (mongoc_cursor_t *)bson_malloc0 (sizeof *cursor);

   /*
    * Cursors execute their query lazily. This sadly means that we must copy
    * some extra data around between the bson_t structures. This should be
    * small in most cases, so it reduces to a pure memcpy. The benefit to this
    * design is simplified error handling by API consumers.
    */

   cursor->client = client;
   cursor->flags = (mongoc_query_flags_t)flags;
   cursor->skip = skip;
   cursor->limit = limit;
   cursor->batch_size = batch_size;
   cursor->is_command = is_command;
   cursor->has_fields = !!fields;

   if (db_and_collection) {
      _mongoc_set_cursor_ns (cursor, db_and_collection,
                             (uint32_t) strlen (db_and_collection));
   }

#define MARK_FAILED(c) \
   do { \
      bson_init (&(c)->query); \
      bson_init (&(c)->fields); \
      (c)->done = true; \
      (c)->end_of_event = true; \
      (c)->sent = true; \
   } while (0)

   /* we can't have exhaust queries with limits */
   if ((flags & MONGOC_QUERY_EXHAUST) && limit) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_CURSOR,
                      MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                      _("Cannot specify MONGOC_QUERY_EXHAUST and set a limit."));
      MARK_FAILED (cursor);
      GOTO (finish);
   }

   /* we can't have exhaust queries with sharded clusters */
   if ((flags & MONGOC_QUERY_EXHAUST) &&
       (client->topology->description.type == MONGOC_TOPOLOGY_SHARDED)) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_CURSOR,
                      MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                      _("Cannot specify MONGOC_QUERY_EXHAUST with sharded cluster."));
      MARK_FAILED (cursor);
      GOTO (finish);
   }

   /*
    * Check types of various optional parameters.
    */
   if (query && !is_command) {
      if (bson_iter_init_find (&iter, query, "$explain") &&
          !(BSON_ITER_HOLDS_BOOL (&iter) || BSON_ITER_HOLDS_INT32 (&iter))) {
         bson_set_error (&cursor->error,
                         MONGOC_ERROR_CURSOR,
                         MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                         _("$explain must be a boolean."));
         MARK_FAILED (cursor);
         GOTO (finish);
      }

      if (bson_iter_init_find (&iter, query, "$snapshot") &&
          !BSON_ITER_HOLDS_BOOL (&iter) &&
          !BSON_ITER_HOLDS_INT32 (&iter)) {
         bson_set_error (&cursor->error,
                         MONGOC_ERROR_CURSOR,
                         MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                         _("$snapshot must be a boolean."));
         MARK_FAILED (cursor);
         GOTO (finish);
      }
   }

   /*
    * Check if we have a mixed top-level query and dollar keys such
    * as $orderby. This is not allowed (you must use {$query:{}}.
    */
   if (query && bson_iter_init (&iter, query)) {
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
                         _("Cannot mix top-level query with dollar keys such "
                         "as $orderby. Use {$query: {},...} instead."));
         MARK_FAILED (cursor);
         GOTO (finish);
      }
   }

   /* don't use MARK_FAILED after this, you'll leak cursor->query */
   if (query) {
      bson_copy_to(query, &cursor->query);
   } else {
      bson_init(&cursor->query);
   }

   if (fields) {
      bson_copy_to(fields, &cursor->fields);
   } else {
      bson_init(&cursor->fields);
   }

   if (read_prefs) {
      cursor->read_prefs = mongoc_read_prefs_copy (read_prefs);
   }

   if (read_concern) {
      cursor->read_concern = mongoc_read_concern_copy (read_concern);
   }

   _mongoc_buffer_init(&cursor->buffer, NULL, 0, NULL, NULL);

finish:
   mongoc_counter_cursors_active_inc();

   RETURN (cursor);
}


void
mongoc_cursor_destroy (mongoc_cursor_t *cursor)
{
   ENTRY;

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
   char db[MONGOC_NAMESPACE_MAX];
   ENTRY;

   BSON_ASSERT (cursor);

   if (cursor->in_exhaust) {
      cursor->client->in_exhaust = false;
      if (!cursor->done) {
         /* The only way to stop an exhaust cursor is to kill the connection */
         mongoc_cluster_disconnect_node (&cursor->client->cluster,
                                         cursor->server_id);
      }
   } else if (cursor->rpc.reply.cursor_id) {
      bson_strncpy (db, cursor->ns, cursor->dblen + 1);

      _mongoc_client_kill_cursor(cursor->client,
                                 cursor->server_id,
                                 cursor->rpc.reply.cursor_id,
                                 cursor->operation_id,
                                 db,
                                 cursor->ns + cursor->dblen + 1);
   }

   if (cursor->reader) {
      bson_reader_destroy(cursor->reader);
      cursor->reader = NULL;
   }

   bson_destroy(&cursor->query);
   bson_destroy(&cursor->fields);
   _mongoc_buffer_destroy(&cursor->buffer);
   mongoc_read_prefs_destroy(cursor->read_prefs);
   mongoc_read_concern_destroy(cursor->read_concern);

   bson_free(cursor);

   mongoc_counter_cursors_active_dec();
   mongoc_counter_cursors_disposed_inc();

   EXIT;
}


mongoc_server_stream_t *
_mongoc_cursor_fetch_stream (mongoc_cursor_t *cursor)
{
   mongoc_server_stream_t *server_stream;

   ENTRY;

   if (cursor->server_id) {
      server_stream = mongoc_cluster_stream_for_server (&cursor->client->cluster,
                                                        cursor->server_id,
                                                        true /* reconnect_ok */,
                                                        &cursor->error);
   } else {
      server_stream = mongoc_cluster_stream_for_reads (&cursor->client->cluster,
                                                       cursor->read_prefs,
                                                       &cursor->error);

      if (server_stream) {
         cursor->server_id = server_stream->sd->id;
      }
   }

   RETURN (server_stream);
}


bool
_use_find_command (const mongoc_cursor_t *cursor,
                   const mongoc_server_stream_t *server_stream)
{
   /* Find, getMore And killCursors Commands Spec: "the find command cannot be
    * used to execute other commands" and "the find command does not support the
    * exhaust flag."
    */
   return server_stream->sd->max_wire_version >= WIRE_VERSION_FIND_CMD &&
          !cursor->is_command &&
          !(cursor->flags & MONGOC_QUERY_EXHAUST);
}


static const bson_t *
_mongoc_cursor_initial_query (mongoc_cursor_t *cursor)
{
   mongoc_server_stream_t *server_stream;
   const bson_t *b = NULL;

   ENTRY;

   BSON_ASSERT (cursor);

   server_stream = _mongoc_cursor_fetch_stream (cursor);

   if (!server_stream) {
      GOTO (done);
   }

   if (_use_find_command (cursor, server_stream)) {
      b = _mongoc_cursor_find_command (cursor);
   } else {
      /* When the user explicitly provides a readConcern -- but the server
       * doesn't support readConcern, we must error:
       * https://github.com/mongodb/specifications/blob/master/source/read-write-concern/read-write-concern.rst#errors-1
       */
      if (cursor->read_concern->level != NULL
          && server_stream->sd->max_wire_version < WIRE_VERSION_READ_CONCERN) {
         bson_set_error (&cursor->error,
                         MONGOC_ERROR_COMMAND,
                         MONGOC_ERROR_PROTOCOL_BAD_WIRE_VERSION,
                         _("The selected server does not support readConcern"));
      } else {
         b = _mongoc_cursor_op_query (cursor, server_stream);
      }
   }

done:
   /* no-op if server_stream is NULL */
   mongoc_server_stream_cleanup (server_stream);

   if (!b) {
      cursor->done = true;
   }

   RETURN (b);
}


static bool
_mongoc_cursor_monitor_legacy_query (mongoc_cursor_t        *cursor,
                                     int32_t                 request_id,
                                     mongoc_server_stream_t *server_stream)
{
   bson_t doc;
   char db[MONGOC_NAMESPACE_MAX];
   mongoc_client_t *client;
   mongoc_apm_command_started_t event;

   ENTRY;

   client = cursor->client;
   if (!client->apm_callbacks.started) {
      /* successful */
      RETURN (true);
   }
   
   bson_init (&doc);
   if (!_mongoc_cursor_prepare_find_command (cursor, &doc)) {
      /* cursor->error is set */
      bson_destroy (&doc);
      RETURN (false);
   }

   bson_strncpy (db, cursor->ns, cursor->dblen + 1);
   mongoc_apm_command_started_init (&event,
                                    &doc,
                                    db,
                                    "find",
                                    request_id,
                                    cursor->operation_id,
                                    &server_stream->sd->host,
                                    server_stream->sd->id,
                                    client->apm_context);

   client->apm_callbacks.started (&event);
   mongoc_apm_command_started_cleanup (&event);
   bson_destroy (&doc);

   RETURN (true);
}


/* append array of docs from current cursor batch */
static void
_mongoc_cursor_append_docs_array (mongoc_cursor_t *cursor,
                                  bson_t          *docs)
{
   bool eof = false;
   char str[16];
   const char *key;
   uint32_t i = 0;
   size_t keylen;
   const bson_t *doc;

   while ((doc = bson_reader_read (cursor->reader, &eof))) {
      keylen = bson_uint32_to_string (i, &key, str, sizeof str);
      bson_append_document (docs, key, (int) keylen, doc);
   }

   bson_reader_reset (cursor->reader);
}


static void
_mongoc_cursor_monitor_succeeded (mongoc_cursor_t        *cursor,
                                  int64_t                 duration,
                                  uint32_t                request_id,
                                  bool                    first_batch,
                                  mongoc_server_stream_t *stream)
{
   mongoc_apm_command_succeeded_t event;
   mongoc_client_t *client;
   bson_t reply;
   bson_t reply_cursor;
   bson_t docs_array;

   ENTRY;

   client = cursor->client;

   if (!client->apm_callbacks.succeeded) {
      EXIT;
   }

   /* fake reply to find/getMore command:
    * {ok: 1, cursor: {id: 1234, ns: "...", first/nextBatch: [ ... docs ... ]}}
    */
   bson_init (&docs_array);
   _mongoc_cursor_append_docs_array (cursor, &docs_array);

   bson_init (&reply);
   bson_append_int32 (&reply, "ok", 2, 1);
   bson_append_document_begin (&reply, "cursor", 6, &reply_cursor);
   bson_append_int64 (&reply_cursor, "id", 2, mongoc_cursor_get_id (cursor));
   bson_append_utf8 (&reply_cursor, "ns", 2, cursor->ns, cursor->nslen);
   bson_append_array (&reply_cursor,
                      first_batch ? "firstBatch" : "nextBatch",
                      first_batch ? 10 : 9,
                      &docs_array);
   bson_append_document_end (&reply, &reply_cursor);

   mongoc_apm_command_succeeded_init (&event,
                                      duration,
                                      &reply,
                                      first_batch ? "find" : "getMore",
                                      request_id,
                                      cursor->operation_id,
                                      &stream->sd->host,
                                      stream->sd->id,
                                      client->apm_context);

   client->apm_callbacks.succeeded (&event);

   mongoc_apm_command_succeeded_cleanup (&event);
   bson_destroy (&reply);
   bson_destroy (&docs_array);

   EXIT;
}


static const bson_t *
_mongoc_cursor_op_query (mongoc_cursor_t        *cursor,
                         mongoc_server_stream_t *server_stream)
{
   int64_t started;
   mongoc_apply_read_prefs_result_t result = READ_PREFS_RESULT_INIT;
   mongoc_rpc_t rpc;
   uint32_t request_id;
   const bson_t *bson = NULL;

   ENTRY;

   started = bson_get_monotonic_time ();

   cursor->sent = true;
   cursor->operation_id = ++cursor->client->cluster.operation_id;

   rpc.query.msg_len = 0;
   rpc.query.request_id = request_id = ++cursor->client->cluster.request_id;
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

   if (cursor->has_fields) {
      rpc.query.fields = bson_get_data (&cursor->fields);
   } else {
      rpc.query.fields = NULL;
   }

   apply_read_preferences (cursor->read_prefs, server_stream,
                           &cursor->query, cursor->flags, &result);

   rpc.query.query = bson_get_data (result.query_with_read_prefs);
   rpc.query.flags = result.flags;

   if (!_mongoc_cursor_monitor_legacy_query (cursor,
                                             request_id,
                                             server_stream)) {
      GOTO (failure);
   }

   if (!mongoc_cluster_sendv_to_server (&cursor->client->cluster,
                                        &rpc, 1, server_stream,
                                        NULL, &cursor->error)) {
      GOTO (failure);
   }

   _mongoc_buffer_clear(&cursor->buffer, false);

   if (!_mongoc_client_recv(cursor->client,
                            &cursor->rpc,
                            &cursor->buffer,
                            server_stream,
                            &cursor->error)) {
      GOTO (failure);
   }

   if (cursor->rpc.header.opcode != MONGOC_OPCODE_REPLY) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      _("Invalid opcode. Expected %d, got %d."),
                      MONGOC_OPCODE_REPLY, cursor->rpc.header.opcode);
      GOTO (failure);
   }

   if (cursor->rpc.header.response_to != (int32_t)request_id) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      _("Invalid response_to for query. Expected %d, got %d."),
                      request_id, cursor->rpc.header.response_to);
      GOTO (failure);
   }

   if (cursor->is_command) {
       if (_mongoc_rpc_parse_command_error (&cursor->rpc,
                                            &cursor->error)) {
          GOTO (failure);
       }
   } else {
      if (_mongoc_rpc_parse_query_error (&cursor->rpc,
                                         &cursor->error)) {
         GOTO (failure);
      }
   }

   if (cursor->reader) {
      bson_reader_destroy (cursor->reader);
   }

   cursor->reader = bson_reader_new_from_data(
      cursor->rpc.reply.documents,
      (size_t) cursor->rpc.reply.documents_len);

   if ((cursor->flags & MONGOC_QUERY_EXHAUST)) {
      cursor->in_exhaust = true;
      cursor->client->in_exhaust = true;
   }

   _mongoc_cursor_monitor_succeeded (cursor,
                                     bson_get_monotonic_time () - started,
                                     request_id,
                                     true, /* first_batch */
                                     server_stream);

   cursor->done = false;
   cursor->end_of_event = false;

   _mongoc_read_from_buffer (cursor, &bson);

   apply_read_prefs_result_cleanup (&result);

   RETURN (bson);

failure:
   cursor->done = true;

   apply_read_prefs_result_cleanup (&result);

   RETURN (false);
}


bool
_mongoc_cursor_run_command (mongoc_cursor_t *cursor,
                            const bson_t    *command,
                            bson_t          *reply)
{
   mongoc_cluster_t *cluster;
   mongoc_server_stream_t *server_stream;
   char db[MONGOC_NAMESPACE_MAX];
   mongoc_apply_read_prefs_result_t read_prefs_result = READ_PREFS_RESULT_INIT;
   bool ret = false;

   ENTRY;

   cluster = &cursor->client->cluster;

   server_stream = _mongoc_cursor_fetch_stream (cursor);

   if (!server_stream) {
      GOTO (done);
   }

   bson_strncpy (db, cursor->ns, cursor->dblen + 1);
   apply_read_preferences (cursor->read_prefs, server_stream,
                           command, cursor->flags, &read_prefs_result);

   ret = mongoc_cluster_run_command_monitored (
      cluster,
      server_stream,
      read_prefs_result.flags,
      db,
      read_prefs_result.query_with_read_prefs,
      reply,
      &cursor->error);

done:
   apply_read_prefs_result_cleanup (&read_prefs_result);
   mongoc_server_stream_cleanup (server_stream);

   return ret;
}


static bool
_invalid_field (const char      *query_field,
                mongoc_cursor_t *cursor)
{
   if (query_field[0] == '\0') {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_CURSOR,
                      MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                      _("empty string is not a valid query operator"));
      return true;
   }

   return false;
}


static bool
_translate_query_opt (const char *query_field,
                      const char **cmd_field,
                      int *len)
{
   if (query_field[0] != '$') {
      *cmd_field = query_field;
      *len = -1;
      return true;
   }

   /* strip the leading '$' */
   query_field++;

   if (!strcmp ("query", query_field)) {
      *cmd_field = "filter";
      *len = 6;
   } else if (!strcmp ("orderby", query_field)) {
      *cmd_field = "sort";
      *len = 4;
   } else if (!strcmp ("showDiskLoc", query_field)) { /* <= MongoDb 3.0 */
      *cmd_field = "showRecordId";
      *len = 12;
   } else if (!strcmp("hint", query_field)) {
      *cmd_field = "hint";
      *len = 4;
   } else if (!strcmp("comment", query_field)) {
      *cmd_field = "comment";
      *len = 7;
   } else if (!strcmp("maxScan", query_field)) {
      *cmd_field = "maxScan";
      *len = 7;
   } else if (!strcmp("maxTimeMS", query_field)) {
      *cmd_field = "maxTimeMS";
      *len = 9;
   } else if (!strcmp("max", query_field)) {
      *cmd_field = "max";
      *len = 3;
   } else if (!strcmp("min", query_field)) {
      *cmd_field = "min";
      *len = 3;
   } else if (!strcmp("returnKey", query_field)) {
      *cmd_field = "returnKey";
      *len = 9;
   } else if (!strcmp("snapshot", query_field)) {
      *cmd_field = "snapshot";
      *len = 8;
   } else {
      /* not a special command field, must be a query operator like $or */
      return false;
   }

   return true;
}


static void
_mongoc_cursor_prepare_find_command_flags (mongoc_cursor_t *cursor,
                                           bson_t          *command)
{
   mongoc_query_flags_t flags = cursor->flags;

   if (flags & MONGOC_QUERY_TAILABLE_CURSOR) {
      bson_append_bool (command, "tailable", 8, true);
   }

   if (flags & MONGOC_QUERY_OPLOG_REPLAY) {
      bson_append_bool (command, "oplogReplay", 11, true);
   }

   if (flags & MONGOC_QUERY_NO_CURSOR_TIMEOUT) {
      bson_append_bool (command, "noCursorTimeout", 15, true);
   }

   if (flags & MONGOC_QUERY_AWAIT_DATA) {
      bson_append_bool (command, "awaitData", 9, true);
   }

   if (flags & MONGOC_QUERY_PARTIAL) {
      bson_append_bool (command, "allowPartialResults", 19, true);
   }
}


void
_mongoc_cursor_collection (const mongoc_cursor_t *cursor,
                           const char **collection,
                           int *collection_len)
{
   /* ns is like "db.collection". Collection name is located past the ".". */
   *collection = cursor->ns + (cursor->dblen + 1);
   /* Collection name's length is ns length, minus length of db name and ".". */
   *collection_len = cursor->nslen - cursor->dblen - 1;

   BSON_ASSERT (*collection_len > 0);
}


static bool
_mongoc_cursor_prepare_find_command (mongoc_cursor_t *cursor,
                                     bson_t          *command)
{
   const char *collection;
   int collection_len;
   bson_iter_t iter;
   const char *command_field;
   int len;
   const bson_value_t *value;

   _mongoc_cursor_collection (cursor, &collection, &collection_len);
   bson_append_utf8 (command, "find", 4, collection, collection_len);

   if (bson_empty0 (&cursor->query)) {
      /* Find, getMore And killCursors Commands Spec: filter "MUST be included
       * in the command".
       */
      bson_t empty = BSON_INITIALIZER;
      bson_append_document (command, "filter", 6, &empty);
   } else if (bson_has_field (&cursor->query, "$query")) {
      bson_iter_init (&iter, &cursor->query);
      while (bson_iter_next (&iter)) {
         if (_invalid_field (bson_iter_key (&iter), cursor)) {
            return false;
         }

         value = bson_iter_value (&iter);
         if (_translate_query_opt (bson_iter_key (&iter),
                                   &command_field,
                                   &len)) {
            bson_append_value (command, command_field, len, value);
         } else {
            bson_append_value (command, bson_iter_key (&iter), -1, value);
         }
      }
   } else if (bson_has_field (&cursor->query, "filter")) {
      bson_concat (command, &cursor->query);
   } else {
      /* cursor->query has no "$query", use it as the filter */
      bson_append_document (command, "filter", 6, &cursor->query);
   }

   if (!bson_empty0 (&cursor->fields)) {
      bson_append_document (command, "projection", 10, &cursor->fields);
   }

   if (cursor->skip) {
      bson_append_int64 (command, "skip", 4, cursor->skip);
   }

   if (cursor->limit) {
      if (cursor->limit < 0) {
         bson_append_bool (command, "singleBatch", 11, true);
      }

      bson_append_int64 (command, "limit", 5, labs(cursor->limit));
   }

   if (cursor->batch_size) {
      bson_append_int64 (command, "batchSize", 9, cursor->batch_size);
   }

   if (cursor->read_concern->level != NULL) {
      const bson_t *read_concern_bson;

      read_concern_bson = _mongoc_read_concern_get_bson (cursor->read_concern);
      BSON_APPEND_DOCUMENT (command, "readConcern", read_concern_bson);
   }

   _mongoc_cursor_prepare_find_command_flags (cursor, command);

   return true;
}


static const bson_t *
_mongoc_cursor_find_command (mongoc_cursor_t *cursor)
{
   bson_t command = BSON_INITIALIZER;
   const bson_t *bson = NULL;

   ENTRY;

   if (!_mongoc_cursor_prepare_find_command (cursor, &command)) {
      RETURN (NULL);
   }

   _mongoc_cursor_cursorid_init (cursor, &command);
   bson_destroy (&command);

   BSON_ASSERT (cursor->iface.next);
   _mongoc_cursor_cursorid_next (cursor, &bson);

   RETURN (bson);
}


static const bson_t *
_mongoc_cursor_get_more (mongoc_cursor_t *cursor)
{
   mongoc_server_stream_t *server_stream;
   const bson_t *b = NULL;

   ENTRY;

   BSON_ASSERT (cursor);

   server_stream = _mongoc_cursor_fetch_stream (cursor);
   if (!server_stream) {
      GOTO (failure);
   }

   if (!cursor->in_exhaust && !cursor->rpc.reply.cursor_id) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_CURSOR,
                      MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                      _("No valid cursor was provided."));
      GOTO (failure);
   }

   if (!_mongoc_cursor_op_getmore (cursor, server_stream)) {
      GOTO (failure);
   }

   mongoc_server_stream_cleanup (server_stream);

   if (cursor->reader) {
      _mongoc_read_from_buffer (cursor, &b);
   }

   RETURN (b);

failure:
   cursor->done = true;

   mongoc_server_stream_cleanup (server_stream);

   RETURN (NULL);
}


static bool
_mongoc_cursor_monitor_legacy_get_more (mongoc_cursor_t        *cursor,
                                        int32_t                 request_id,
                                        mongoc_server_stream_t *server_stream)
{
   bson_t doc;
   char db[MONGOC_NAMESPACE_MAX];
   mongoc_client_t *client;
   mongoc_apm_command_started_t event;

   ENTRY;

   client = cursor->client;
   if (!client->apm_callbacks.started) {
      /* successful */
      RETURN (true);
   }

   bson_init (&doc);
   if (!_mongoc_cursor_prepare_getmore_command (cursor, &doc)) {
      bson_destroy (&doc);
      RETURN (false);
   }

   bson_strncpy (db, cursor->ns, cursor->dblen + 1);
   mongoc_apm_command_started_init (&event,
                                    &doc,
                                    db,
                                    "getMore",
                                    request_id,
                                    cursor->operation_id,
                                    &server_stream->sd->host,
                                    server_stream->sd->id,
                                    client->apm_context);

   client->apm_callbacks.started (&event);
   mongoc_apm_command_started_cleanup (&event);
   bson_destroy (&doc);

   RETURN (true);
}


bool
_mongoc_cursor_op_getmore (mongoc_cursor_t        *cursor,
                           mongoc_server_stream_t *server_stream)
{
   int64_t started;
   mongoc_rpc_t rpc;
   uint32_t request_id;
   bool ret = false;

   ENTRY;

   started = bson_get_monotonic_time ();

   if (cursor->in_exhaust) {
      request_id = (uint32_t) cursor->rpc.header.request_id;
   } else {
      rpc.get_more.cursor_id = cursor->rpc.reply.cursor_id;
      rpc.get_more.msg_len = 0;
      rpc.get_more.request_id = ++cursor->client->cluster.request_id;
      rpc.get_more.response_to = 0;
      rpc.get_more.opcode = MONGOC_OPCODE_GET_MORE;
      rpc.get_more.zero = 0;
      rpc.get_more.collection = cursor->ns;
      if ((cursor->flags & MONGOC_QUERY_TAILABLE_CURSOR)) {
         rpc.get_more.n_return = 0;
      } else {
         rpc.get_more.n_return = _mongoc_n_return(cursor);
      }

      if (!_mongoc_cursor_monitor_legacy_get_more (cursor,
                                                   rpc.get_more.request_id,
                                                   server_stream)) {
         GOTO (done);
      }

      if (!mongoc_cluster_sendv_to_server (&cursor->client->cluster,
                                           &rpc, 1, server_stream,
                                           NULL, &cursor->error)) {
         GOTO (done);
      }

      request_id = BSON_UINT32_FROM_LE (rpc.header.request_id);
   }

   _mongoc_buffer_clear (&cursor->buffer, false);

   if (!_mongoc_client_recv (cursor->client,
                             &cursor->rpc,
                             &cursor->buffer,
                             server_stream,
                             &cursor->error)) {
      GOTO (done);
   }

   if (cursor->rpc.header.opcode != MONGOC_OPCODE_REPLY) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      _("Invalid opcode. Expected %d, got %d."),
                      MONGOC_OPCODE_REPLY, cursor->rpc.header.opcode);
      GOTO (done);
   }

   if (cursor->rpc.header.response_to != (int32_t)request_id) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      _("Invalid response_to for getmore. Expected %d, got %d."),
                      request_id, cursor->rpc.header.response_to);
      GOTO (done);
   }

   if (_mongoc_rpc_parse_query_error (&cursor->rpc,
                                      &cursor->error)) {
      GOTO (done);
   }

   if (cursor->reader) {
      bson_reader_destroy (cursor->reader);
   }

   cursor->reader = bson_reader_new_from_data (
      cursor->rpc.reply.documents,
      (size_t)cursor->rpc.reply.documents_len);

   ret = true;

   _mongoc_cursor_monitor_succeeded (cursor,
                                     bson_get_monotonic_time () - started,
                                     request_id,
                                     false, /* not first batch */
                                     server_stream);

done:
   RETURN (ret);
}


bool
mongoc_cursor_error (mongoc_cursor_t *cursor,
                     bson_error_t    *error)
{
   bool ret;

   ENTRY;

   BSON_ASSERT(cursor);

   if (cursor->iface.error) {
      ret = cursor->iface.error(cursor, error);
   } else {
      ret = _mongoc_cursor_error(cursor, error);
   }

   RETURN(ret);
}


bool
_mongoc_cursor_error (mongoc_cursor_t *cursor,
                      bson_error_t    *error)
{
   ENTRY;

   BSON_ASSERT (cursor);

   if (BSON_UNLIKELY(CURSOR_FAILED (cursor))) {
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

   if (CURSOR_FAILED (cursor)) {
      return false;
   }

   /*
    * We cannot proceed if another cursor is receiving results in exhaust mode.
    */
   if (cursor->client->in_exhaust && !cursor->in_exhaust) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_IN_EXHAUST,
                      _("Another cursor derived from this client is in exhaust."));
      RETURN (false);
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
_mongoc_read_from_buffer (mongoc_cursor_t *cursor,
                          const bson_t   **bson)
{
   bool eof = false;

   BSON_ASSERT (cursor->reader);

   *bson = bson_reader_read (cursor->reader, &eof);
   cursor->end_of_event = eof ? 1 : 0;

   return *bson ? true : false;
}


bool
_mongoc_cursor_next (mongoc_cursor_t  *cursor,
                     const bson_t    **bson)
{
   const bson_t *b = NULL;

   ENTRY;

   BSON_ASSERT (cursor);

   if (bson) {
      *bson = NULL;
   }

   if (cursor->done || CURSOR_FAILED (cursor)) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_CURSOR,
                      MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                      _("Cannot advance a completed or failed cursor."));
      RETURN (false);
   }

   /*
    * If we reached our limit, make sure we mark this as done and do not try to
    * make further progress.
    */
   if (cursor->limit && cursor->count >= labs(cursor->limit)) {
      cursor->done = true;
      RETURN (false);
   }

   /*
    * Try to read the next document from the reader if it exists, we might
    * get NULL back and EOF, in which case we need to submit a getmore.
    */
   if (cursor->reader) {
      _mongoc_read_from_buffer (cursor, &b);
      if (b) {
         GOTO (complete);
      }
   }

   /*
    * Check to see if we need to send a GET_MORE for more results.
    */
   if (!cursor->sent) {
      b = _mongoc_cursor_initial_query (cursor);
   } else if (BSON_UNLIKELY (cursor->end_of_event) && cursor->rpc.reply.cursor_id) {
      b = _mongoc_cursor_get_more (cursor);
   }

complete:
   cursor->done = (cursor->end_of_event &&
                   ((cursor->in_exhaust && !cursor->rpc.reply.cursor_id) ||
                    (!b && !(cursor->flags & MONGOC_QUERY_TAILABLE_CURSOR))));

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
   BSON_ASSERT (cursor);

   if (CURSOR_FAILED (cursor)) {
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
   mongoc_server_description_t *description;

   BSON_ASSERT (cursor);
   BSON_ASSERT (host);

   memset(host, 0, sizeof *host);

   if (!cursor->server_id) {
      MONGOC_WARNING("%s(): Must send query before fetching peer.",
                     BSON_FUNC);
      return;
   }

   description = mongoc_topology_server_by_id(cursor->client->topology,
                                              cursor->server_id,
                                              &cursor->error);
   if (!description) {
      return;
   }

   *host = description->host;

   mongoc_server_description_destroy (description);

   return;
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

   _clone = (mongoc_cursor_t *)bson_malloc0 (sizeof *_clone);

   _clone->client = cursor->client;
   _clone->is_command = cursor->is_command;
   _clone->flags = cursor->flags;
   _clone->skip = cursor->skip;
   _clone->batch_size = cursor->batch_size;
   _clone->limit = cursor->limit;
   _clone->nslen = cursor->nslen;
   _clone->dblen = cursor->dblen;
   _clone->has_fields = cursor->has_fields;

   if (cursor->read_prefs) {
      _clone->read_prefs = mongoc_read_prefs_copy (cursor->read_prefs);
   }

   if (cursor->read_concern) {
      _clone->read_concern = mongoc_read_concern_copy (cursor->read_concern);
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
   BSON_ASSERT (cursor);

   return (!cursor->sent ||
           (!CURSOR_FAILED (cursor) &&
            !cursor->done &&
            (cursor->rpc.header.opcode == MONGOC_OPCODE_REPLY) &&
            cursor->rpc.reply.cursor_id));
}


const bson_t *
mongoc_cursor_current (const mongoc_cursor_t *cursor) /* IN */
{
   BSON_ASSERT (cursor);

   return cursor->current;
}


void
mongoc_cursor_set_batch_size (mongoc_cursor_t *cursor,
                              uint32_t         batch_size)
{
   BSON_ASSERT (cursor);
   cursor->batch_size = batch_size;
}

uint32_t
mongoc_cursor_get_batch_size (const mongoc_cursor_t *cursor)
{
   BSON_ASSERT (cursor);

   return cursor->batch_size;
}

bool
mongoc_cursor_set_limit (mongoc_cursor_t *cursor,
                         int64_t          limit)
{
   BSON_ASSERT (cursor);

   if (!cursor->sent) {
      cursor->limit = limit;
      return true;
   } else {
      return false;
   }
}

int64_t
mongoc_cursor_get_limit (const mongoc_cursor_t *cursor)
{
   BSON_ASSERT (cursor);

   return cursor->limit;
}

uint32_t
mongoc_cursor_get_hint (const mongoc_cursor_t *cursor)
{
   BSON_ASSERT (cursor);

   return cursor->server_id;
}

int64_t
mongoc_cursor_get_id (const mongoc_cursor_t  *cursor)
{
   BSON_ASSERT(cursor);

   return cursor->rpc.reply.cursor_id;
}

void
mongoc_cursor_set_max_await_time_ms (mongoc_cursor_t *cursor,
                                     uint32_t         max_await_time_ms)
{
   BSON_ASSERT (cursor);

   if (!cursor->sent) {
      cursor->max_await_time_ms = max_await_time_ms;
   }
}

uint32_t
mongoc_cursor_get_max_await_time_ms (const mongoc_cursor_t *cursor)
{
   BSON_ASSERT (cursor);

   return cursor->max_await_time_ms;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_cursor_new_from_command_reply --
 *
 *       Low-level function to initialize a mongoc_cursor_t from the
 *       reply to a command like "aggregate", "find", or "listCollections".
 *
 *       Useful in drivers that wrap the C driver; in applications, use
 *       high-level functions like mongoc_collection_aggregate instead.
 *
 * Returns:
 *       A cursor.
 *
 * Side effects:
 *       On failure, the cursor's error is set: retrieve it with
 *       mongoc_cursor_error. On success or failure, "reply" is
 *       destroyed.
 *
 *--------------------------------------------------------------------------
 */

mongoc_cursor_t *
mongoc_cursor_new_from_command_reply (mongoc_client_t *client,
                                      bson_t          *reply,
                                      uint32_t         server_id)
{
   mongoc_cursor_t *cursor;
   bson_t cmd = BSON_INITIALIZER;

   BSON_ASSERT (client);
   BSON_ASSERT (reply);

   cursor = _mongoc_cursor_new (client, NULL, MONGOC_QUERY_NONE,
                                0, 0, 0, false, NULL, NULL, NULL, NULL);

   _mongoc_cursor_cursorid_init (cursor, &cmd);
   _mongoc_cursor_cursorid_init_with_reply (cursor, reply, server_id);
   bson_destroy (&cmd);

   return cursor;
}

/*==============================================================*/
/* --- mongoc-cursor-cursorid.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "cursor-cursorid"


static void *
_mongoc_cursor_cursorid_new (void)
{
   mongoc_cursor_cursorid_t *cid;

   ENTRY;

   cid = (mongoc_cursor_cursorid_t *)bson_malloc0 (sizeof *cid);
   cid->array = NULL;
   cid->in_batch = false;
   cid->in_reader = false;

   RETURN (cid);
}


static void
_mongoc_cursor_cursorid_destroy (mongoc_cursor_t *cursor)
{
   mongoc_cursor_cursorid_t *cid;

   ENTRY;

   cid = (mongoc_cursor_cursorid_t *)cursor->iface_data;
   BSON_ASSERT (cid);

   if (cid->array) {
      bson_destroy (cid->array);
   }

   bson_free (cid);
   _mongoc_cursor_destroy (cursor);

   EXIT;
}


/*
 * Start iterating the reply to an "aggregate", "find", "getMore" etc. command:
 *
 *    {cursor: {id: 1234, ns: "db.collection", firstBatch: [...]}}
 */
bool
_mongoc_cursor_cursorid_start_batch (mongoc_cursor_t *cursor)
{
   mongoc_cursor_cursorid_t *cid;
   bson_iter_t iter;
   bson_iter_t child;
   const char *ns;
   uint32_t nslen;

   cid = (mongoc_cursor_cursorid_t *)cursor->iface_data;

   BSON_ASSERT (cid);
   BSON_ASSERT (cid->array);

   if (bson_iter_init_find (&iter, cid->array, "cursor") &&
       BSON_ITER_HOLDS_DOCUMENT (&iter) &&
       bson_iter_recurse (&iter, &child)) {
      while (bson_iter_next (&child)) {
         if (BSON_ITER_IS_KEY (&child, "id")) {
            cursor->rpc.reply.cursor_id = bson_iter_as_int64 (&child);
         } else if (BSON_ITER_IS_KEY (&child, "ns")) {
            ns = bson_iter_utf8 (&child, &nslen);
            _mongoc_set_cursor_ns (cursor, ns, nslen);
         } else if (BSON_ITER_IS_KEY (&child, "firstBatch") ||
                    BSON_ITER_IS_KEY (&child, "nextBatch")) {
            if (BSON_ITER_HOLDS_ARRAY (&child) &&
                bson_iter_recurse (&child, &cid->batch_iter)) {
               cid->in_batch = true;
            }
         }
      }
   }

   return cid->in_batch;
}


static bool
_mongoc_cursor_cursorid_refresh_from_command (mongoc_cursor_t *cursor,
                                              const bson_t    *command)
{
   mongoc_cursor_cursorid_t *cid;

   ENTRY;

   cid = (mongoc_cursor_cursorid_t *)cursor->iface_data;
   BSON_ASSERT (cid);

   if (cid->array) {
      bson_destroy (cid->array);
   }

   cid->array = bson_new ();

   /* server replies to find / aggregate with {cursor: {id: N, firstBatch: []}},
    * to getMore command with {cursor: {id: N, nextBatch: []}}. */
   if (_mongoc_cursor_run_command (cursor, command, cid->array) &&
       _mongoc_cursor_cursorid_start_batch (cursor)) {

      RETURN (true);
   } else {
      if (!cursor->error.domain) {
         bson_set_error (&cursor->error,
                         MONGOC_ERROR_PROTOCOL,
                         MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                         _("Invalid reply to %s command."),
                         _mongoc_get_command_name (command));
      }

      RETURN (false);
   }
}


static void
_mongoc_cursor_cursorid_read_from_batch (mongoc_cursor_t *cursor,
                                         const bson_t   **bson)
{
   mongoc_cursor_cursorid_t *cid;
   const uint8_t *data = NULL;
   uint32_t data_len = 0;

   ENTRY;

   cid = (mongoc_cursor_cursorid_t *)cursor->iface_data;
   BSON_ASSERT (cid);

   if (bson_iter_next (&cid->batch_iter) &&
       BSON_ITER_HOLDS_DOCUMENT (&cid->batch_iter)) {
      bson_iter_document (&cid->batch_iter, &data_len, &data);

      if (bson_init_static (&cid->current_doc, data, data_len)) {
         *bson = &cid->current_doc;
      }
   }
}


bool
_mongoc_cursor_cursorid_prime (mongoc_cursor_t *cursor)
{
   cursor->sent = true;
   cursor->operation_id = ++cursor->client->cluster.operation_id;
   return _mongoc_cursor_cursorid_refresh_from_command (cursor, &cursor->query);
}


bool
_mongoc_cursor_prepare_getmore_command (mongoc_cursor_t *cursor,
                                        bson_t          *command)
{
   const char *collection;
   int collection_len;

   ENTRY;

   _mongoc_cursor_collection (cursor, &collection, &collection_len);

   bson_init (command);
   bson_append_int64 (command, "getMore", 7, mongoc_cursor_get_id (cursor));
   bson_append_utf8 (command, "collection", 10, collection, collection_len);

   if (cursor->batch_size) {
      bson_append_int64 (command, "batchSize", 9, cursor->batch_size);
   }

   /* Find, getMore And killCursors Commands Spec: "In the case of a tailable
      cursor with awaitData == true the driver MUST provide a Cursor level
      option named maxAwaitTimeMS (See CRUD specification for details). The
      maxTimeMS option on the getMore command MUST be set to the value of the
      option maxAwaitTimeMS. If no maxAwaitTimeMS is specified, the driver
      SHOULD not set maxTimeMS on the getMore command."
    */
   if (cursor->flags & MONGOC_QUERY_TAILABLE_CURSOR &&
       cursor->flags & MONGOC_QUERY_AWAIT_DATA &&
       cursor->max_await_time_ms) {
      bson_append_int32 (command, "maxTimeMS", 9, cursor->max_await_time_ms);
   }

   RETURN (true);
}


static bool
_mongoc_cursor_cursorid_get_more (mongoc_cursor_t *cursor)
{
   mongoc_cursor_cursorid_t *cid;
   mongoc_server_stream_t *server_stream;
   bson_t command;
   bool ret;

   ENTRY;

   cid = (mongoc_cursor_cursorid_t *)cursor->iface_data;
   BSON_ASSERT (cid);

   server_stream = _mongoc_cursor_fetch_stream (cursor);

   if (!server_stream) {
      RETURN (false);
   }

   if (_use_find_command (cursor, server_stream)) {
      if (!_mongoc_cursor_prepare_getmore_command (cursor, &command)) {
         mongoc_server_stream_cleanup (server_stream);
         RETURN (false);
      }

      ret = _mongoc_cursor_cursorid_refresh_from_command (cursor, &command);
      bson_destroy (&command);
   } else {
      ret = _mongoc_cursor_op_getmore (cursor, server_stream);
      cid->in_reader = ret;
   }

   mongoc_server_stream_cleanup (server_stream);
   RETURN (ret);
}


bool
_mongoc_cursor_cursorid_next (mongoc_cursor_t *cursor,
                              const bson_t   **bson)
{
   mongoc_cursor_cursorid_t *cid;
   bool refreshed = false;

   ENTRY;

   *bson = NULL;

   cid = (mongoc_cursor_cursorid_t *)cursor->iface_data;
   BSON_ASSERT (cid);

   if (!cursor->sent) {
      if (!_mongoc_cursor_cursorid_prime (cursor)) {
         GOTO (done);
      }
   }

again:

   /* Two paths:
    * - Mongo 3.2+, sent "getMore" cmd, we're reading reply's "nextBatch" array
    * - Mongo 2.6 to 3, after "aggregate" or similar command we sent OP_GETMORE,
    *   we're reading the raw reply
    */
   if (cid->in_batch) {
      _mongoc_cursor_cursorid_read_from_batch (cursor, bson);

      if (*bson) {
         GOTO (done);
      }

      cid->in_batch = false;
   } else if (cid->in_reader) {
      _mongoc_read_from_buffer (cursor, bson);

      if (*bson) {
         GOTO (done);
      }

      cid->in_reader = false;
   }

   if (!refreshed && mongoc_cursor_get_id (cursor)) {
      if (!_mongoc_cursor_cursorid_get_more (cursor)) {
         GOTO (done);
      }

      refreshed = true;
      GOTO (again);
   }

done:
   RETURN (*bson ? true : false);
}


static mongoc_cursor_t *
_mongoc_cursor_cursorid_clone (const mongoc_cursor_t *cursor)
{
   mongoc_cursor_t *clone_;

   ENTRY;

   clone_ = _mongoc_cursor_clone (cursor);
   _mongoc_cursor_cursorid_init (clone_, &cursor->query);

   RETURN (clone_);
}


static mongoc_cursor_interface_t gMongocCursorCursorid = {
   _mongoc_cursor_cursorid_clone,
   _mongoc_cursor_cursorid_destroy,
   NULL,
   _mongoc_cursor_cursorid_next,
};


void
_mongoc_cursor_cursorid_init (mongoc_cursor_t *cursor,
                              const bson_t    *command)
{
   ENTRY;

   bson_destroy (&cursor->query);
   bson_copy_to (command, &cursor->query);

   cursor->iface_data = _mongoc_cursor_cursorid_new ();

   memcpy (&cursor->iface, &gMongocCursorCursorid,
           sizeof (mongoc_cursor_interface_t));

   EXIT;
}

void
_mongoc_cursor_cursorid_init_with_reply (mongoc_cursor_t *cursor,
                                         bson_t          *reply,
                                         uint32_t         server_id)
{
   mongoc_cursor_cursorid_t *cid;

   cursor->sent = true;
   cursor->server_id = server_id;

   cid = (mongoc_cursor_cursorid_t *)cursor->iface_data;
   BSON_ASSERT (cid);

   if (cid->array) {
      bson_destroy (cid->array);
   }

   cid->array = reply;

   if (!_mongoc_cursor_cursorid_start_batch (cursor)) {
      bson_set_error (&cursor->error,
                      MONGOC_ERROR_CURSOR,
                      MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                      _("Couldn't parse cursor document"));
   }
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
                      const mongoc_read_concern_t  *read_concern,
                      const mongoc_write_concern_t *write_concern)
{
   mongoc_database_t *db;

   ENTRY;

   BSON_ASSERT (client);
   BSON_ASSERT (name);

   db = (mongoc_database_t *)bson_malloc0(sizeof *db);
   db->client = client;
   db->write_concern = write_concern ?
      mongoc_write_concern_copy(write_concern) :
      mongoc_write_concern_new();
   db->read_concern = read_concern ?
      mongoc_read_concern_copy(read_concern) :
      mongoc_read_concern_new();
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

   BSON_ASSERT (database);

   if (database->read_prefs) {
      mongoc_read_prefs_destroy(database->read_prefs);
      database->read_prefs = NULL;
   }

   if (database->read_concern) {
      mongoc_read_concern_destroy(database->read_concern);
      database->read_concern = NULL;
   }

   if (database->write_concern) {
      mongoc_write_concern_destroy(database->write_concern);
      database->write_concern = NULL;
   }

   bson_free(database);

   EXIT;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_database_copy --
 *
 *       Returns a copy of @database that needs to be freed by calling
 *       mongoc_database_destroy.
 *
 * Returns:
 *       A copy of this database.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_database_t *
mongoc_database_copy (mongoc_database_t *database)
{
   ENTRY;

   BSON_ASSERT (database);

   RETURN(_mongoc_database_new(database->client, database->name,
                               database->read_prefs, database->read_concern,
                               database->write_concern));
}

mongoc_cursor_t *
mongoc_database_command (mongoc_database_t         *database,
                         mongoc_query_flags_t       flags,
                         uint32_t                   skip,
                         uint32_t                   limit,
                         uint32_t                   batch_size,
                         const bson_t              *command,
                         const bson_t              *fields,
                         const mongoc_read_prefs_t *read_prefs)
{
   BSON_ASSERT (database);
   BSON_ASSERT (command);

   /* Server Selection Spec: "The generic command method has a default read
    * preference of mode 'primary'. The generic command method MUST ignore any
    * default read preference from client, database or collection
    * configuration. The generic command method SHOULD allow an optional read
    * preference argument."
    */
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

   /* Server Selection Spec: "The generic command method has a default read
    * preference of mode 'primary'. The generic command method MUST ignore any
    * default read preference from client, database or collection
    * configuration. The generic command method SHOULD allow an optional read
    * preference argument."
    */
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

   BSON_ASSERT (database);

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

   BSON_ASSERT (database);
   BSON_ASSERT (username);
   BSON_ASSERT (password);

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
      bson_init(&user);
      bson_copy_to_excluding_noinit(doc, &user, "pwd", (char *)NULL);
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

   BSON_ASSERT (database);
   BSON_ASSERT (username);

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
   } else if (error) {
      memcpy (error, &lerror, sizeof *error);
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

   BSON_ASSERT (database);

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
   } else if (error) {
      memcpy (error, &lerror, sizeof *error);
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
   } else if (ret || (lerror.code == 13)) {
      /* usersInfo succeeded or failed with auth err, we're on modern mongod */
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
   BSON_ASSERT (database);
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
   BSON_ASSERT (database);

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
 * mongoc_database_get_read_concern --
 *
 *       Fetches the read concern for @database.
 *
 * Returns:
 *       A mongoc_read_concern_t that should not be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const mongoc_read_concern_t *
mongoc_database_get_read_concern (const mongoc_database_t *database)
{
   BSON_ASSERT (database);

   return database->read_concern;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_database_set_read_concern --
 *
 *       Set the default read concern for @database.
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
mongoc_database_set_read_concern (mongoc_database_t            *database,
                                  const mongoc_read_concern_t  *read_concern)
{
   BSON_ASSERT (database);

   if (database->read_concern) {
      mongoc_read_concern_destroy (database->read_concern);
      database->read_concern = NULL;
   }

   if (read_concern) {
      database->read_concern = mongoc_read_concern_copy (read_concern);
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
   BSON_ASSERT (database);

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
   BSON_ASSERT (database);

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
   bson_iter_t col_iter;
   bool ret = false;
   const char *cur_name;
   bson_t filter = BSON_INITIALIZER;
   mongoc_cursor_t *cursor;
   const bson_t *doc;

   ENTRY;

   BSON_ASSERT (database);
   BSON_ASSERT (name);

   if (error) {
      memset (error, 0, sizeof *error);
   }

   BSON_APPEND_UTF8 (&filter, "name", name);

   cursor = mongoc_database_find_collections (database, &filter, error);
   
   if (!cursor) {
      return ret;
   }
   
   if (error &&
        ((error->domain != 0) ||
         (error->code != 0))) {
      GOTO (cleanup);
   }

   while (mongoc_cursor_next (cursor, &doc)) {
      if (bson_iter_init (&col_iter, doc) &&
          bson_iter_find (&col_iter, "name") &&
          BSON_ITER_HOLDS_UTF8 (&col_iter) &&
          (cur_name = bson_iter_utf8 (&col_iter, NULL))) {
         if (!strcmp (cur_name, name)) {
            ret = true;
            GOTO (cleanup);
         }
      }
   }

cleanup:
   mongoc_cursor_destroy (cursor);

   RETURN (ret);
}

typedef struct
{
   const char *dbname;
   size_t      dbname_len;
   const char *name;
} mongoc_database_find_collections_legacy_ctx_t;

static mongoc_cursor_transform_mode_t
_mongoc_database_find_collections_legacy_filter (const bson_t *bson,
                                                 void         *ctx_)
{
   bson_iter_t iter;
   mongoc_database_find_collections_legacy_ctx_t *ctx;

   ctx = (mongoc_database_find_collections_legacy_ctx_t *)ctx_;

   if (bson_iter_init_find (&iter, bson, "name")
       && BSON_ITER_HOLDS_UTF8 (&iter)
       && (ctx->name = bson_iter_utf8 (&iter, NULL))
       && !strchr (ctx->name, '$')
       && (0 == strncmp (ctx->name, ctx->dbname, ctx->dbname_len))) {
      return MONGO_CURSOR_TRANSFORM_MUTATE;
   } else {
      return MONGO_CURSOR_TRANSFORM_DROP;
   }
}

static void
_mongoc_database_find_collections_legacy_mutate (const bson_t *bson,
                                                 bson_t       *out,
                                                 void         *ctx_)
{
   mongoc_database_find_collections_legacy_ctx_t *ctx;

   ctx = (mongoc_database_find_collections_legacy_ctx_t *)ctx_;

   bson_copy_to_excluding_noinit (bson, out, "name", NULL);
   BSON_APPEND_UTF8 (out, "name", ctx->name + (ctx->dbname_len + 1));  /* +1 for the '.' */
}

/* Uses old way of querying system.namespaces. */
mongoc_cursor_t *
_mongoc_database_find_collections_legacy (mongoc_database_t *database,
                                          const bson_t      *filter,
                                          bson_error_t      *error)
{
   mongoc_collection_t *col;
   mongoc_cursor_t *cursor = NULL;
   mongoc_read_prefs_t *read_prefs;
   uint32_t dbname_len;
   bson_t legacy_filter;
   bson_iter_t iter;
   const char *col_filter;
   bson_t q = BSON_INITIALIZER;
   mongoc_database_find_collections_legacy_ctx_t *ctx;

   BSON_ASSERT (database);

   col = mongoc_client_get_collection (database->client,
                                       database->name,
                                       "system.namespaces");

   BSON_ASSERT (col);

   dbname_len = (uint32_t)strlen (database->name);

   ctx = (mongoc_database_find_collections_legacy_ctx_t *)bson_malloc (sizeof (*ctx));

   ctx->dbname = database->name;
   ctx->dbname_len = dbname_len;

   /* Filtering on name needs to be handled differently for old servers. */
   if (filter && bson_iter_init_find (&iter, filter, "name")) {
      bson_string_t *buf;
      /* on legacy servers, this must be a string (i.e. not a regex) */
      if (!BSON_ITER_HOLDS_UTF8 (&iter)) {
         bson_set_error (error,
                         MONGOC_ERROR_NAMESPACE,
                         MONGOC_ERROR_NAMESPACE_INVALID_FILTER_TYPE,
                         _("On legacy servers, a filter on name can only be a string."));
         bson_free (ctx);
         goto cleanup_filter;
      }
      BSON_ASSERT (BSON_ITER_HOLDS_UTF8 (&iter));
      col_filter = bson_iter_utf8 (&iter, NULL);
      bson_init (&legacy_filter);
      bson_copy_to_excluding_noinit (filter, &legacy_filter, "name", NULL);
      /* We must db-qualify filters on name. */
      buf = bson_string_new (database->name);
      bson_string_append_c (buf, '.');
      bson_string_append (buf, col_filter);
      BSON_APPEND_UTF8 (&legacy_filter, "name", buf->str);
      bson_string_free (buf, true);
      filter = &legacy_filter;
   }

   read_prefs = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);

   cursor = mongoc_collection_find (col, MONGOC_QUERY_NONE, 0, 0, 0,
                                    filter ? filter : &q, NULL, read_prefs);

   _mongoc_cursor_transform_init (
      cursor,
      _mongoc_database_find_collections_legacy_filter,
      _mongoc_database_find_collections_legacy_mutate,
      &bson_free,
      ctx);

   mongoc_read_prefs_destroy (read_prefs);

 cleanup_filter:
   mongoc_collection_destroy (col);

   return cursor;
}

mongoc_cursor_t *
mongoc_database_find_collections (mongoc_database_t *database,
                                  const bson_t      *filter,
                                  bson_error_t      *error)
{
   mongoc_cursor_t *cursor;
   mongoc_read_prefs_t *read_prefs;
   bson_t cmd = BSON_INITIALIZER;
   bson_t child;
   bson_error_t lerror;

   BSON_ASSERT (database);

   BSON_APPEND_INT32 (&cmd, "listCollections", 1);

   if (filter) {
      BSON_APPEND_DOCUMENT (&cmd, "filter", filter);
      BSON_APPEND_DOCUMENT_BEGIN (&cmd, "cursor", &child);
      bson_append_document_end (&cmd, &child);
   }

   read_prefs = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);

   cursor = _mongoc_cursor_new (database->client, database->name,
                                MONGOC_QUERY_SLAVE_OK, 0, 0, 0, true,
                                NULL, NULL, NULL, NULL);

   _mongoc_cursor_cursorid_init (cursor, &cmd);

   if (_mongoc_cursor_cursorid_prime (cursor)) {
       /* intentionally empty */
   } else {
      if (mongoc_cursor_error (cursor, &lerror)) {
         if (lerror.code == MONGOC_ERROR_QUERY_COMMAND_NOT_FOUND) {
            /* We are talking to a server that doesn' support listCollections. */
            /* clear out the error. */
            memset (&lerror, 0, sizeof lerror);
            /* try again with using system.namespaces */
            mongoc_cursor_destroy (cursor);
            cursor = _mongoc_database_find_collections_legacy (
               database, filter, error);
         } else if (error) {
            memcpy (error, &lerror, sizeof *error);
         }
      }
   }

   bson_destroy (&cmd);
   mongoc_read_prefs_destroy (read_prefs);

   return cursor;
}

char **
mongoc_database_get_collection_names (mongoc_database_t *database,
                                      bson_error_t      *error)
{
   bson_iter_t col;
   const char *name;
   char *namecopy;
   mongoc_array_t strv_buf;
   mongoc_cursor_t *cursor;
   const bson_t *doc;
   char **ret;

   BSON_ASSERT (database);

   cursor = mongoc_database_find_collections (database, NULL, error);

   if (!cursor) {
      return NULL;
   }

   _mongoc_array_init (&strv_buf, sizeof (char *));

   while (mongoc_cursor_next (cursor, &doc)) {
      if (bson_iter_init (&col, doc) &&
          bson_iter_find (&col, "name") &&
          BSON_ITER_HOLDS_UTF8 (&col) &&
          (name = bson_iter_utf8 (&col, NULL))) {
          namecopy = bson_strdup (name);
          _mongoc_array_append_val (&strv_buf, namecopy);
      }
   }

   /* append a null pointer for the last value. also handles the case
    * of no values. */
   namecopy = NULL;
   _mongoc_array_append_val (&strv_buf, namecopy);

   if (mongoc_cursor_error (cursor, error)) {
      _mongoc_array_destroy (&strv_buf);
      ret = NULL;
   } else {
      ret = (char **)strv_buf.data;
   }

   mongoc_cursor_destroy (cursor);

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

   BSON_ASSERT (database);
   BSON_ASSERT (name);

   if (strchr (name, '$')) {
      bson_set_error (error,
                      MONGOC_ERROR_NAMESPACE,
                      MONGOC_ERROR_NAMESPACE_INVALID,
                      _("The namespace \"%s\" is invalid."),
                      name);
      return NULL;
   }

   if (options) {
      if (bson_iter_init_find (&iter, options, "capped")) {
         if (!BSON_ITER_HOLDS_BOOL (&iter)) {
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            _("The argument \"capped\" must be a boolean."));
            return NULL;
         }
         capped = bson_iter_bool (&iter);
      }

      if (bson_iter_init_find (&iter, options, "autoIndexId") &&
          !BSON_ITER_HOLDS_BOOL (&iter)) {
         bson_set_error (error,
                         MONGOC_ERROR_COMMAND,
                         MONGOC_ERROR_COMMAND_INVALID_ARG,
                         _("The argument \"autoIndexId\" must be a boolean."));
         return NULL;
      }

      if (bson_iter_init_find (&iter, options, "size")) {
         if (!BSON_ITER_HOLDS_INT32 (&iter) &&
             !BSON_ITER_HOLDS_INT64 (&iter)) {
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            _("The argument \"size\" must be an integer."));
            return NULL;
         }
         if (!capped) {
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            _("The \"size\" parameter requires {\"capped\": true}"));
            return NULL;
         }
      }

      if (bson_iter_init_find (&iter, options, "max")) {
         if (!BSON_ITER_HOLDS_INT32 (&iter) &&
             !BSON_ITER_HOLDS_INT64 (&iter)) {
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            _("The argument \"max\" must be an integer."));
            return NULL;
         }
         if (!capped) {
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            _("The \"max\" parameter requires {\"capped\": true}"));
            return NULL;
         }
      }

      if (bson_iter_init_find (&iter, options, "storage")) {
         if (!BSON_ITER_HOLDS_DOCUMENT (&iter)) {
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            _("The \"storage\" parameter must be a document"));

            return NULL;
         }

         if (bson_iter_find (&iter, "wiredtiger")) {
            if (!BSON_ITER_HOLDS_DOCUMENT (&iter)) {
               bson_set_error (error,
                               MONGOC_ERROR_COMMAND,
                               MONGOC_ERROR_COMMAND_INVALID_ARG,
                               _("The \"wiredtiger\" option must take a document argument with a \"configString\" field"));
               return NULL;
            }

            if (bson_iter_find (&iter, "configString")) {
               if (!BSON_ITER_HOLDS_UTF8 (&iter)) {
                  bson_set_error (error,
                                  MONGOC_ERROR_COMMAND,
                                  MONGOC_ERROR_COMMAND_INVALID_ARG,
                                  _("The \"configString\" parameter must be a string"));
                  return NULL;
               }
            } else {
               bson_set_error (error,
                               MONGOC_ERROR_COMMAND,
                               MONGOC_ERROR_COMMAND_INVALID_ARG,
                               _("The \"wiredtiger\" option must take a document argument with a \"configString\" field"));
               return NULL;
            }
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
                         _("The argument \"options\" is corrupt or invalid."));
         bson_destroy (&cmd);
         return NULL;
      }

      while (bson_iter_next (&iter)) {
         if (!bson_append_iter (&cmd, bson_iter_key (&iter), -1, &iter)) {
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            _("Failed to append \"options\" to create command."));
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
                                           database->read_concern,
                                           database->write_concern);
   }

   bson_destroy (&cmd);

   return collection;
}


mongoc_collection_t *
mongoc_database_get_collection (mongoc_database_t *database,
                                const char        *collection)
{
   BSON_ASSERT (database);
   BSON_ASSERT (collection);

   return _mongoc_collection_new (database->client, database->name, collection,
                                  database->read_prefs, database->read_concern,
                                  database->write_concern);
}


const char *
mongoc_database_get_name (mongoc_database_t *database)
{
   BSON_ASSERT (database);

   return database->name;
}

/*==============================================================*/
/* --- mongoc-gridfs.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "gridfs"

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
   const mongoc_read_prefs_t *read_prefs;
   const mongoc_read_concern_t *read_concern;
   const mongoc_write_concern_t *write_concern;
   char buf[128];
   bool r;
   uint32_t prefix_len;

   ENTRY;

   BSON_ASSERT (client);
   BSON_ASSERT (db);

   if (!prefix) {
      prefix = "fs";
   }

   /* make sure prefix is short enough to bucket the chunks and files
    * collections
    */
   prefix_len = (uint32_t)strlen (prefix);
   BSON_ASSERT (prefix_len + sizeof (".chunks") < sizeof (buf));

   gridfs = (mongoc_gridfs_t *) bson_malloc0 (sizeof *gridfs);

   gridfs->client = client;

   read_prefs = mongoc_client_get_read_prefs (client);
   read_concern = mongoc_client_get_read_concern (client);
   write_concern = mongoc_client_get_write_concern (client);

   bson_snprintf (buf, sizeof(buf), "%s.chunks", prefix);
   gridfs->chunks = _mongoc_collection_new (client, db, buf, read_prefs,
                                            read_concern, write_concern);

   bson_snprintf (buf, sizeof(buf), "%s.files", prefix);
   gridfs->files = _mongoc_collection_new (client, db, buf, read_prefs,
                                           read_concern, write_concern);

   r = _mongoc_gridfs_ensure_index (gridfs, error);

   if (!r) {
      mongoc_gridfs_destroy (gridfs);
      RETURN (NULL);
   }

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

   mongoc_stream_failed (stream);

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

   BSON_ASSERT (gridfs);

   file = _mongoc_gridfs_file_new (gridfs, opt);

   RETURN (file);
}

/** accessor functions for collections */
mongoc_collection_t *
mongoc_gridfs_get_files (mongoc_gridfs_t *gridfs)
{
   BSON_ASSERT (gridfs);

   return gridfs->files;
}

mongoc_collection_t *
mongoc_gridfs_get_chunks (mongoc_gridfs_t *gridfs)
{
   BSON_ASSERT (gridfs);

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

   BSON_ASSERT (gridfs);

   if (!filename) {
      bson_set_error (error,
                      MONGOC_ERROR_GRIDFS,
                      MONGOC_ERROR_GRIDFS_INVALID_FILENAME,
                      _("A non-NULL filename must be specified."));
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

static ssize_t
_mongoc_gridfs_file_extend (mongoc_gridfs_file_t *file);


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

   file = (mongoc_gridfs_file_t *)bson_malloc0 (sizeof *file);

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

   file = (mongoc_gridfs_file_t *)bson_malloc0 (sizeof *file);

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

   file->pos = 0;
   file->n = 0;

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

   /* Reading when positioned past the end does nothing */
   if ((int64_t)file->pos >= file->length) {
      return 0;
   }

   /* Try to get the current chunk */
   if (!file->page && !_mongoc_gridfs_file_refresh_page (file)) {
         return -1;
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
         } else if (file->length == (int64_t)file->pos) {
            /* we're at the end of the file.  So we're done */
            RETURN (bytes_read);
         } else if (bytes_read >= min_bytes) {
            /* we need a new page, but we've read enough bytes to stop */
            RETURN (bytes_read);
         } else if (!_mongoc_gridfs_file_refresh_page (file)) {
            /* more to read, just on a new page */
            return -1;
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

   /* Pull in the correct page */
   if (!file->page && !_mongoc_gridfs_file_refresh_page (file)) {
      return -1;
   }

   /* When writing past the end-of-file, fill the gap with zeros */
   if ((int64_t)file->pos > file->length && !_mongoc_gridfs_file_extend (file)) {
      return -1;
   }

   for (i = 0; i < iovcnt; i++) {
      iov_pos = 0;

      for (;; ) {
         if (!file->page && !_mongoc_gridfs_file_refresh_page (file)) {
            return -1;
         }

         /* write bytes until an iov is exhausted or the page is full */
         r = _mongoc_gridfs_file_page_write (file->page,
                                            (uint8_t *)iov[i].iov_base + iov_pos,
                                            (uint32_t)(iov[i].iov_len - iov_pos));
         BSON_ASSERT (r >= 0);

         iov_pos += r;
         file->pos += r;
         bytes_written += r;

         file->length = BSON_MAX (file->length, (int64_t)file->pos);

         if (iov_pos == iov[i].iov_len) {
            /** filled a bucket, keep going */
            break;
         } else {
            /** flush the buffer, the next pass through will bring in a new page */
            _mongoc_gridfs_file_flush_page (file);
         }
      }
   }

   file->is_dirty = 1;

   RETURN (bytes_written);
}


/**
 * _mongoc_gridfs_file_extend:
 *
 *      Extend a GridFS file to the current position pointer. Zeros will be
 *      appended to the end of the file until file->length is even with file->pos.
 *
 *      If file->length >= file->pos, the function exits successfully with no
 *      operation performed.
 *
 * Parameters:
 *      @file: A mongoc_gridfs_file_t.
 *
 * Returns:
 *      The number of zero bytes written, or -1 on failure.
 */
static ssize_t
_mongoc_gridfs_file_extend (mongoc_gridfs_file_t *file)
{
   int64_t target_length;
   ssize_t diff;

   ENTRY;

   BSON_ASSERT (file);

   if (file->length >= (int64_t)file->pos) {
      RETURN (0);
   }

   diff = (ssize_t)(file->pos - file->length);
   target_length = file->pos;
   mongoc_gridfs_file_seek (file, 0, SEEK_END);

   while (true) {
      if (!file->page && !_mongoc_gridfs_file_refresh_page (file)) {
         RETURN (-1);
      }

      /* Set bytes until we reach the limit or fill a page */
      file->pos += _mongoc_gridfs_file_page_memset0 (file->page,
                                                     target_length - file->pos);

      if ((int64_t)file->pos == target_length) {
         /* We're done */
         break;
      } else if (!_mongoc_gridfs_file_flush_page (file)) {
         /* We tried to flush a full buffer, but an error occurred */
         RETURN (-1);
      }
   }

   BSON_ASSERT (file->length = target_length);
   file->is_dirty = true;

   RETURN (diff);
}


/**
 * _mongoc_gridfs_file_flush_page:
 *
 *    Unconditionally flushes the file's current page to the database.
 *    The page to flush is determined by page->n.
 *
 * Side Effects:
 *
 *    On success, file->page is properly destroyed and set to NULL.
 *
 * Returns:
 *
 *    True on success; false otherwise.
 */
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
   bson_append_int32 (selector, "n", -1, file->n);

   update = bson_sized_new (file->chunk_size + 100);

   bson_append_value (update, "files_id", -1, &file->files_id);
   bson_append_int32 (update, "n", -1, file->n);
   bson_append_binary (update, "data", -1, BSON_SUBTYPE_BINARY, buf, len);

   r = mongoc_collection_update (file->gridfs->chunks, MONGOC_UPDATE_UPSERT,
                                 selector, update, NULL, &file->error);

   bson_destroy (selector);
   bson_destroy (update);

   if (r) {
      _mongoc_gridfs_file_page_destroy (file->page);
      file->page = NULL;
      r = mongoc_gridfs_file_save (file);
   }

   RETURN (r);
}


/**
 * _mongoc_gridfs_file_keep_cursor:
 *
 *    After a seek, decide if the next read should use the current cursor or
 *    start a new query.
 *
 * Preconditions:
 *
 *    file has a cursor and cursor range.
 *
 * Side Effects:
 *
 *    None.
 */
static bool
_mongoc_gridfs_file_keep_cursor (mongoc_gridfs_file_t *file)
{
   uint32_t chunk_no;
   uint32_t chunks_per_batch;

   if (file->n < 0 || file->chunk_size <= 0) {
      return false;
   }

   chunk_no = (uint32_t) file->n;
   /* server returns roughly 4 MB batches by default */
   chunks_per_batch = (4 * 1024 * 1024) / (uint32_t) file->chunk_size;

   return (
      /* cursor is on or before the desired chunk */
      file->cursor_range[0] <= chunk_no &&
      /* chunk_no is before end of file */
      chunk_no <= file->cursor_range[1] &&
      /* desired chunk is in this batch or next one */
      chunk_no < file->cursor_range[0] + 2 * chunks_per_batch);
}


/**
 * _mongoc_gridfs_file_refresh_page:
 *
 *    Refresh a GridFS file's underlying page. This recalculates the current
 *    page number based on the file's stream position, then fetches that page
 *    from the database.
 *
 *    Note that this fetch is unconditional and the page is queried from the
 *    database even if the current page covers the same theoretical chunk.
 *
 *
 * Side Effects:
 *
 *    file->page is loaded with the appropriate buffer, fetched from the
 *    database. If the file position is at the end of the file and on a new
 *    chunk boundary, a new page is created. We currently DO NOT handle the case
 *    of the file position being far past the end-of-file.
 *
 *    file->n is set based on file->pos. file->error is set on error.
 */
static bool
_mongoc_gridfs_file_refresh_page (mongoc_gridfs_file_t *file)
{
   bson_t *query, *fields, child, child2;
   const bson_t *chunk;
   const char *key;
   bson_iter_t iter;

   const uint8_t *data = NULL;	/* XXX coverity 1357857 */
   uint32_t len = 0;		/* XXX coverity 1357858 */

   ENTRY;

   BSON_ASSERT (file);

   file->n = (int32_t)(file->pos / file->chunk_size);

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
      if (file->cursor && !_mongoc_gridfs_file_keep_cursor (file)) {
         mongoc_cursor_destroy (file->cursor);
         file->cursor = NULL;
      }

      if (!file->cursor) {
         query = bson_new ();

         bson_append_document_begin(query, "$query", -1, &child);
            bson_append_value (&child, "files_id", -1, &file->files_id);

            bson_append_document_begin (&child, "n", -1, &child2);
               bson_append_int32 (&child2, "$gte", -1, file->n);
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

         file->cursor_range[0] = file->n;
         file->cursor_range[1] = (uint32_t)(file->length / file->chunk_size);

         bson_destroy (query);
         bson_destroy (fields);

         BSON_ASSERT (file->cursor);
      }

      /* we might have had a cursor before, then seeked ahead past a chunk.
       * iterate until we're on the right chunk */
      while ((int32_t)file->cursor_range[0] <= file->n) {
         if (!mongoc_cursor_next (file->cursor, &chunk)) {
            /* copy cursor error, if any. might just lack a matching chunk. */
            mongoc_cursor_error (file->cursor, &file->error);
            RETURN (0);
         }

         file->cursor_range[0]++;
      }

      bson_iter_init (&iter, chunk);

      /* grab out what we need from the chunk */
      while (bson_iter_next (&iter)) {
         key = bson_iter_key (&iter);

         if (strcmp (key, "n") == 0) {
            if (file->n != bson_iter_int32 (&iter)) {
               bson_set_error (&file->error,
                               MONGOC_ERROR_GRIDFS,
                               MONGOC_ERROR_GRIDFS_CHUNK_MISSING,
                               _("missing chunk number %" PRId32),
                               file->n);
               RETURN (0);
            }
         } else if (strcmp (key, "data") == 0) {
            bson_iter_binary (&iter, NULL, &len, &data);
         } else {
            /* Unexpected key. This should never happen */
            RETURN (0);
         }
      }

      if (file->n != (int32_t)(file->pos / file->chunk_size)) {
         return 0;
      }
   }

assert(data != NULL);	/* XXX coverity 1357858 */
assert(len > 0);	/* XXX coverity 1357857 */
   file->page = _mongoc_gridfs_file_page_new (data, len, file->chunk_size);

   /* seek in the page towards wherever we're supposed to be */
   RETURN (_mongoc_gridfs_file_page_seek (file->page, file->pos %
                                         file->chunk_size));
}


/**
 * mongoc_gridfs_file_seek:
 *
 *    Adjust the file position pointer in `file` by `delta`, starting from the
 *    position `whence`. The `whence` argument is interpreted as in fseek(2):
 *
 *       SEEK_SET    Set the position relative to the start of the file.
 *       SEEK_CUR    Move `delta` from the current file position.
 *       SEEK_END    Move `delta` from the end-of-file.
 *
 * Parameters:
 *
 *    @file: A mongoc_gridfs_file_t.
 *    @delta: The amount to move. May be positive or negative.
 *    @whence: One of SEEK_SET, SEEK_CUR or SEEK_END.
 *
 * Errors:
 *
 *    [EINVAL] `whence` is not one of SEEK_SET, SEEK_CUR or SEEK_END.
 *    [EINVAL] Resulting file position would be negative.
 *
 * Side Effects:
 *
 *    On success, the file's underlying position pointer is set appropriately.
 *    On failure, the file position is NOT changed and errno is set.
 *
 * Returns:
 *
 *    0 on success.
 *    -1 on error, and errno set to indicate the error.
 */
int
mongoc_gridfs_file_seek (mongoc_gridfs_file_t *file,
                         int64_t               delta,
                         int                   whence)
{
   int64_t offset;

   BSON_ASSERT (file);

   switch (whence) {
   case SEEK_SET:
      offset = delta;
      break;
   case SEEK_CUR:
      offset = file->pos + delta;
      break;
   case SEEK_END:
      offset = file->length + delta;
      break;
   default:
      errno = EINVAL;
      return -1;

      break;
   }

   if (offset < 0) {
      errno = EINVAL;
      return -1;
   }

   if (offset / file->chunk_size != file->n) {
      /** no longer on the same page */

      if (file->page) {
         if (_mongoc_gridfs_file_page_is_dirty (file->page)) {
            _mongoc_gridfs_file_flush_page (file);
         } else {
            _mongoc_gridfs_file_page_destroy (file->page);
            file->page = NULL;
         }
      }

      /** we'll pick up the seek when we fetch a page on the next action.  We lazily load */
   } else if (file->page) {
      _mongoc_gridfs_file_page_seek (file->page, offset % file->chunk_size);
   }

   file->pos = offset;
   file->n = file->pos / file->chunk_size;

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

   if (BSON_UNLIKELY(file->error.domain)) {
      bson_set_error(error,
                     file->error.domain,
                     file->error.code,
                     "%s",
                     file->error.message);
      RETURN(true);
   }

   RETURN(false);
}

const bson_value_t *
mongoc_gridfs_file_get_id (mongoc_gridfs_file_t *file)
{
   BSON_ASSERT (file);

   return &file->files_id;
}

int64_t
mongoc_gridfs_file_get_length (mongoc_gridfs_file_t *file)
{
   BSON_ASSERT (file);

   return file->length;
}

int32_t
mongoc_gridfs_file_get_chunk_size (mongoc_gridfs_file_t *file)
{
   BSON_ASSERT (file);

   return file->chunk_size;
}

int64_t
mongoc_gridfs_file_get_upload_date (mongoc_gridfs_file_t *file)
{
   BSON_ASSERT (file);

   return file->upload_date;
}

bool
mongoc_gridfs_file_remove (mongoc_gridfs_file_t *file,
                           bson_error_t         *error)
{
   bson_t sel = BSON_INITIALIZER;
   bool ret = false;

   BSON_ASSERT (file);

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

   list = (mongoc_gridfs_file_list_t *)bson_malloc0 (sizeof *list);

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

   page = (mongoc_gridfs_file_page_t *)bson_malloc0 (sizeof *page);

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

   bytes_read = BSON_MIN (len, page->len - page->offset);

   src = page->read_buf ? page->read_buf : page->buf;

   memcpy (dst, src + page->offset, bytes_read);

   page->offset += bytes_read;

   RETURN (bytes_read);
}


/**
 * _mongoc_gridfs_file_page_write:
 *
 * Write to a page.
 *
* Writes are copy-on-write with regards to the buffer that was passed to the
 * mongoc_gridfs_file_page_t during construction. In other words, the first
 * write allocates a large enough buffer for file->chunk_size, which becomes
 * authoritative from then on.
 *
 * A write of zero bytes will trigger the copy-on-write mechanism.
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

   bytes_written = BSON_MIN (len, page->chunk_size - page->offset);

   if (!page->buf) {
      page->buf = (uint8_t *) bson_malloc (page->chunk_size);
      memcpy (page->buf, page->read_buf, BSON_MIN (page->chunk_size, page->len));
   }

   /* Copy bytes and adjust the page position */
   memcpy (page->buf + page->offset, src, bytes_written);
   page->offset += bytes_written;
   page->len = BSON_MAX (page->offset, page->len);

   /* Don't use the old read buffer, which is no longer current */
   page->read_buf = page->buf;

   RETURN (bytes_written);
}


/**
 * _mongoc_gridfs_file_page_memset0:
 *
 *      Write zeros to a page, starting from the page's current position. Up to
 *      `len` bytes will be set to zero or until the page is full, whichever
 *      comes first.
 *
 *      Like _mongoc_gridfs_file_page_write, operations are copy-on-write with
 *      regards to the page buffer.
 *
 * Returns:
 *      True on success; false otherwise.
 */
bool
_mongoc_gridfs_file_page_memset0 (mongoc_gridfs_file_page_t *page,
                                  uint32_t len)
{
   int32_t bytes_set;

   ENTRY;

   BSON_ASSERT (page);

   bytes_set = BSON_MIN (page->chunk_size - page->offset, len);

   if (!page->buf) {
      page->buf = (uint8_t *)bson_malloc0 (page->chunk_size);
      memcpy (page->buf, page->read_buf, BSON_MIN (page->chunk_size, page->len));
   }

   /* Set bytes and adjust the page position */
   memset (page->buf + page->offset, '\0', bytes_set);
   page->offset += bytes_set;
   page->len = BSON_MAX (page->offset, page->len);

   /* Don't use the old read buffer, which is no longer current */
   page->read_buf = page->buf;

   RETURN (true);
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
   1,                     /* is_initialized */
   0,                     /* background */
   0,                     /* unique */
   NULL,                  /* name */
   0,                     /* drop_dups */
   0,                     /* sparse  */
   -1,                    /* expire_after_seconds */
   -1,                    /* v */
   NULL,                  /* weights */
   NULL,                  /* default_language */
   NULL,                  /* language_override */
   NULL,                  /* mongoc_index_opt_geo_t geo_options */
   NULL,                  /* mongoc_index_opt_storage_t storage_options */
   NULL,                  /* partial_filter_expression */
   {NULL}                 /* struct padding */
};

static mongoc_index_opt_geo_t gMongocIndexOptGeoDefault = {
   26,                        /* twod_sphere_version */
   -90,                       /* twod_bits_precision */
   90,                        /* twod_location_min */
   -1,                        /* twod_location_max */
   2,                         /* haystack_bucket_size */
   {NULL}                     /* struct padding */
};

static mongoc_index_opt_wt_t gMongocIndexOptWTDefault = {
   { MONGOC_INDEX_STORAGE_OPT_WIREDTIGER }, /* mongoc_index_opt_storage_t */
   "",                                      /* config_str */
   {NULL}                                   /* struct padding */
};

const mongoc_index_opt_t *
mongoc_index_opt_get_default (void)
{
   return &gMongocIndexOptDefault;
}

const mongoc_index_opt_geo_t *
mongoc_index_opt_geo_get_default (void)
{
   return &gMongocIndexOptGeoDefault;
}

const mongoc_index_opt_wt_t *
mongoc_index_opt_wt_get_default (void)
{
   return &gMongocIndexOptWTDefault;
}

void
mongoc_index_opt_init (mongoc_index_opt_t *opt)
{
   BSON_ASSERT (opt);

   memcpy (opt, &gMongocIndexOptDefault, sizeof *opt);
}

void
mongoc_index_opt_geo_init (mongoc_index_opt_geo_t *opt)
{
   BSON_ASSERT (opt);

   memcpy (opt, &gMongocIndexOptGeoDefault, sizeof *opt);
}

void mongoc_index_opt_wt_init (mongoc_index_opt_wt_t *opt)
{
   BSON_ASSERT(opt);

   memcpy (opt, &gMongocIndexOptWTDefault, sizeof *opt);
}

/*==============================================================*/
/* --- mongoc-init.c */

#ifdef MONGOC_ENABLE_SASL

static void *
mongoc_sasl_mutex_alloc (void)
{
   mongoc_mutex_t *mutex;

   mutex = (mongoc_mutex_t *)bson_malloc0 (sizeof (mongoc_mutex_t));
   mongoc_mutex_init (mutex);

   return (void *) mutex;
}


static int
mongoc_sasl_mutex_lock (void *mutex)
{
   mongoc_mutex_lock ((mongoc_mutex_t *) mutex);

   return SASL_OK;
}


static int
mongoc_sasl_mutex_unlock (void *mutex)
{
   mongoc_mutex_unlock ((mongoc_mutex_t *) mutex);

   return SASL_OK;
}


static void
mongoc_sasl_mutex_free (void *mutex)
{
   mongoc_mutex_destroy ((mongoc_mutex_t *) mutex);
   bson_free (mutex);
}

#endif//MONGOC_ENABLE_SASL


static MONGOC_ONCE_FUN( _mongoc_do_init)
{
#ifdef MONGOC_ENABLE_OPENSSL
   _mongoc_openssl_init();
   _mongoc_scram_startup();
#endif

#ifdef MONGOC_ENABLE_SASL
   /* The following functions should not use tracing, as they may be invoked
    * before mongoc_log_set_handler() can complete. */
   sasl_set_mutex (mongoc_sasl_mutex_alloc,
                   mongoc_sasl_mutex_lock,
                   mongoc_sasl_mutex_unlock,
                   mongoc_sasl_mutex_free);

   /* TODO: logging callback? */
   sasl_client_init (NULL);
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

      BSON_ASSERT (err == 0);
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
#ifdef MONGOC_ENABLE_OPENSSL
   _mongoc_openssl_cleanup();
#endif

#ifdef MONGOC_ENABLE_SASL
#ifdef MONGOC_HAVE_SASL_CLIENT_DONE
   sasl_client_done ();
#else
   /* fall back to deprecated function */
   sasl_done ();
#endif
#endif

#ifdef _WIN32
   WSACleanup ();
#endif

   _mongoc_counters_cleanup ();

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
#if defined(__GNUC__) && ! defined(MONGOC_NO_AUTOMATIC_GLOBALS)
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

   item = (mongoc_list_t *)bson_malloc0(sizeof *item);
   item->data = (void *)data;
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

   item = (mongoc_list_t *)bson_malloc0(sizeof *item);
   item->data = (void *)data;
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

   BSON_ASSERT (list);

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

   BSON_ASSERT (func);

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
#ifdef MONGOC_TRACE
static bool               gLogTrace = true;
#endif
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


/* just for testing */
void
_mongoc_log_get_handler (mongoc_log_func_t  *log_func,
                         void              **user_data)
{
   *log_func = gLogFunc;
   *user_data = gLogData;
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
   int stop_logging;

   mongoc_once(&once, &_mongoc_ensure_mutex_once);

   stop_logging = !gLogFunc;
#ifdef MONGOC_TRACE
   stop_logging = stop_logging || (log_level == MONGOC_LOG_LEVEL_TRACE && !gLogTrace);
#endif
   if (stop_logging) {
      return;
   }

   BSON_ASSERT (format);

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
   struct tm tt;
   time_t t;
   FILE *stream;
   char nowstr[32];
   int pid;

   bson_gettimeofday(&tv);
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

bool
_mongoc_log_trace_is_enabled (void)
{
#ifdef MONGOC_TRACE
   return gLogTrace;
#else
   return false;
#endif
}

void
mongoc_log_trace_enable (void)
{
#ifdef MONGOC_TRACE
   gLogTrace = true;
#endif
}

void
mongoc_log_trace_disable (void)
{
#ifdef MONGOC_TRACE
   gLogTrace = false;
#endif
}

void
mongoc_log_trace_bytes (const char *domain, const uint8_t *_b, size_t _l)
{
   bson_string_t *str, *astr;
   int32_t _i;
   uint8_t _v;

#ifdef MONGOC_TRACE
   if (!gLogTrace) {
      return;
   }
#endif

   str = bson_string_new(NULL);
   astr = bson_string_new(NULL);
   for (_i = 0; _i < (int32_t)_l; _i++) {
      _v = *(_b + _i);

      if ((_i % 16) == 0) {
         bson_string_append_printf(str, "%05x: ", _i);
      }

      bson_string_append_printf(str, " %02x", _v);
      if (isprint(_v)) {
         bson_string_append_printf(astr, " %c", _v);
      } else {
         bson_string_append(astr, " .");
      }

      if ((_i % 16) == 15) {
         mongoc_log(MONGOC_LOG_LEVEL_TRACE, domain,
                    "%s %s", str->str, astr->str);
         bson_string_truncate(str, 0);
         bson_string_truncate(astr, 0);
      } else if ((_i % 16) == 7) {
         bson_string_append(str, " ");
         bson_string_append(astr, " ");
      }
   }

   if (_i != 16) {
      mongoc_log(MONGOC_LOG_LEVEL_TRACE, domain,
                 "%-56s %s", str->str, astr->str);
   }

   bson_string_free(str, true);
   bson_string_free(astr, true);
}

void
mongoc_log_trace_iovec (const char *domain, const mongoc_iovec_t *_iov, size_t _iovcnt)
{
   bson_string_t *str, *astr;
   const char *_b;
   unsigned _i = 0;
   unsigned _j = 0;
   unsigned _k = 0;
   size_t _l = 0;
   uint8_t _v;

#ifdef MONGOC_TRACE
   if (!gLogTrace) {
      return;
   }
#endif

   for (_i = 0; _i < _iovcnt; _i++) {
      _l += _iov[_i].iov_len;
   }

   _i = 0;
   str = bson_string_new(NULL);
   astr = bson_string_new(NULL);

   for (_j = 0; _j < _iovcnt; _j++) {
      _b = (char *)_iov[_j].iov_base;
      _l = _iov[_j].iov_len;

      for (_k = 0; _k < _l; _k++, _i++) {
         _v = *(_b + _k);
         if ((_i % 16) == 0) {
            bson_string_append_printf(str, "%05x: ", _i);
         }

         bson_string_append_printf(str, " %02x", _v);
         if (isprint(_v)) {
            bson_string_append_printf(astr, " %c", _v);
         } else {
            bson_string_append(astr, " .");
         }

         if ((_i % 16) == 15) {
            mongoc_log(MONGOC_LOG_LEVEL_TRACE, domain,
                       "%s %s", str->str, astr->str);
            bson_string_truncate(str, 0);
            bson_string_truncate(astr, 0);
         } else if ((_i % 16) == 7) {
            bson_string_append(str, " ");
            bson_string_append(astr, " ");
         }
      }
   }

   if (_i != 16) {
      mongoc_log(MONGOC_LOG_LEVEL_TRACE, domain,
                 "%-56s %s", str->str, astr->str);
   }

   bson_string_free(str, true);
   bson_string_free(astr, true);
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
                         _("Document contains no operations."));
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
                         _("Invalid operator \"%s\""),
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
                         _("Invalid value for operator \"%s\""),
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
                   _("Invalid operator \"%s\""),
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
                      _("Invalid logical operator."));
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
                         _("Expected document in value."));
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
                         _("Expected document in value."));
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

   matcher = (mongoc_matcher_t *)bson_malloc0 (sizeof *matcher);
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

   op = (mongoc_matcher_op_t *)bson_malloc0 (sizeof *op);
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

   op = (mongoc_matcher_op_t *)bson_malloc0 (sizeof *op);
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

   op = (mongoc_matcher_op_t *)bson_malloc0 (sizeof *op);
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

   BSON_ASSERT (path);
   BSON_ASSERT (iter);

   op = (mongoc_matcher_op_t *)bson_malloc0 (sizeof *op);
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

   op = (mongoc_matcher_op_t *)bson_malloc0 (sizeof *op);
   op->not_.base.opcode = MONGOC_MATCHER_OPCODE_NOT;
   op->not_.path = bson_strdup (path);
   op->not_.child = child;

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
      _mongoc_matcher_op_destroy (op->not_.child);
      bson_free (op->not_.path);
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
_mongoc_matcher_op_not_match (mongoc_matcher_op_not_t *not_,  /* IN */
                              const bson_t            *bson) /* IN */
{
   BSON_ASSERT (not_);
   BSON_ASSERT (bson);

   return !_mongoc_matcher_op_match (not_->child, bson);
}


#define _TYPE_CODE(l, r) ((((int)(l)) << 8) | ((int)(r)))
#define _NATIVE_COMPARE(op, t1, t2) \
   (bson_iter##t2(iter) op bson_iter##t1(compare_iter))
#define _EQ_COMPARE(t1, t2)  _NATIVE_COMPARE(==, t1, t2)
#define _NE_COMPARE(t1, t2)  _NATIVE_COMPARE(!=, t1, t2)
#define _GT_COMPARE(t1, t2)  _NATIVE_COMPARE(>, t1, t2)
#define _GTE_COMPARE(t1, t2) _NATIVE_COMPARE(>=, t1, t2)
#define _LT_COMPARE(t1, t2)  _NATIVE_COMPARE(<, t1, t2)
#define _LTE_COMPARE(t1, t2) _NATIVE_COMPARE(<=, t1, t2)


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_iter_eq_match --
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
_mongoc_matcher_iter_eq_match (bson_iter_t *compare_iter, /* IN */
                               bson_iter_t *iter)         /* IN */
{
   int code;

   BSON_ASSERT (compare_iter);
   BSON_ASSERT (iter);

   code = _TYPE_CODE (bson_iter_type (compare_iter),
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

         lstr = bson_iter_utf8 (compare_iter, &llen);
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

   case _TYPE_CODE (BSON_TYPE_ARRAY, BSON_TYPE_ARRAY):
      {
         bson_iter_t left_array;
         bson_iter_t right_array;
         bson_iter_recurse (compare_iter, &left_array);
         bson_iter_recurse (iter, &right_array);

         while (true) {
            bool left_has_next = bson_iter_next (&left_array);
            bool right_has_next = bson_iter_next (&right_array);

            if (left_has_next != right_has_next) {
               /* different lengths */
               return false;
            }

            if (!left_has_next) {
               /* finished */
               return true;
            }

            if (!_mongoc_matcher_iter_eq_match (&left_array, &right_array)) {
               return false;
            }
         }
      }

   case _TYPE_CODE (BSON_TYPE_DOCUMENT, BSON_TYPE_DOCUMENT):
      {
         uint32_t llen;
         uint32_t rlen;
         const uint8_t *ldoc;
         const uint8_t *rdoc;

         bson_iter_document (compare_iter, &llen, &ldoc);
         bson_iter_document (iter, &rlen, &rdoc);

         return ((llen == rlen) && (0 == memcmp (ldoc, rdoc, llen)));
      }

   default:
      return false;
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_matcher_op_eq_match --
 *
 *       Performs equality match for all types on either left or right
 *       side of the equation.
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
   BSON_ASSERT (compare);
   BSON_ASSERT (iter);

   return _mongoc_matcher_iter_eq_match (&compare->iter, iter);
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
   bson_iter_t *compare_iter = &compare->iter;

   BSON_ASSERT (compare);
   BSON_ASSERT (iter);

   code = _TYPE_CODE (bson_iter_type (compare_iter),
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
                      bson_iter_type (compare_iter),
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
   bson_iter_t *compare_iter;
   int code;

   BSON_ASSERT (compare);
   BSON_ASSERT (iter);

   compare_iter = &compare->iter;
   code = _TYPE_CODE (bson_iter_type (compare_iter),
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
                      bson_iter_type (compare_iter),
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
   bson_iter_t *compare_iter;
   int code;

   BSON_ASSERT (compare);
   BSON_ASSERT (iter);

   compare_iter = &compare->iter;
   code = _TYPE_CODE (bson_iter_type (compare_iter),
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
                      bson_iter_type (compare_iter),
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
   bson_iter_t *compare_iter;
   int code;

   BSON_ASSERT (compare);
   BSON_ASSERT (iter);

   compare_iter = &compare->iter;
   code = _TYPE_CODE (bson_iter_type (compare_iter),
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
                      bson_iter_type (compare_iter),
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
      return _mongoc_matcher_op_not_match (&op->not_, bson);
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
      _ignore_value(bson_append_iter (bson, op->compare.path, -1, &op->compare.iter));
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
      if (bson_append_document_begin (bson, op->compare.path, -1, &child)) {
         _ignore_value (bson_append_iter (&child, str, -1, &op->compare.iter));
         bson_append_document_end (bson, &child);
      }
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
      bson_append_document_begin (bson, op->not_.path, -1, &child);
      bson_append_document_begin (&child, "$not", 4, &child2);
      _mongoc_matcher_op_to_bson (op->not_.child, &child2);
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
   BSON_ASSERT (queue);

   memset (queue, 0, sizeof *queue);
}


void
_mongoc_queue_push_head (mongoc_queue_t *queue,
                         void           *data)
{
   mongoc_queue_item_t *item;

   BSON_ASSERT (queue);
   BSON_ASSERT (data);

   item = (mongoc_queue_item_t *)bson_malloc0(sizeof *item);
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

   BSON_ASSERT (queue);
   BSON_ASSERT (data);

   item = (mongoc_queue_item_t *)bson_malloc0(sizeof *item);
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

   BSON_ASSERT (queue);

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

   BSON_ASSERT (queue);

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

   read_prefs = (mongoc_read_prefs_t *)bson_malloc0(sizeof *read_prefs);
   read_prefs->mode = mode;
   bson_init(&read_prefs->tags);

   return read_prefs;
}


mongoc_read_mode_t
mongoc_read_prefs_get_mode (const mongoc_read_prefs_t *read_prefs)
{
   return read_prefs ? read_prefs->mode : MONGOC_READ_PRIMARY;
}


void
mongoc_read_prefs_set_mode (mongoc_read_prefs_t *read_prefs,
                            mongoc_read_mode_t   mode)
{
   BSON_ASSERT (read_prefs);
   BSON_ASSERT (mode <= MONGOC_READ_NEAREST);

   read_prefs->mode = mode;
}


const bson_t *
mongoc_read_prefs_get_tags (const mongoc_read_prefs_t *read_prefs)
{
   BSON_ASSERT (read_prefs);
   return &read_prefs->tags;
}


void
mongoc_read_prefs_set_tags (mongoc_read_prefs_t *read_prefs,
                            const bson_t        *tags)
{
   BSON_ASSERT (read_prefs);

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
   BSON_ASSERT (read_prefs);

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


static const char *
_get_read_mode_string (mongoc_read_mode_t mode)
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


/* Update result with the read prefs, following Server Selection Spec.
 * The driver must have discovered the server is a mongos.
 */
static void
_apply_read_preferences_mongos (const mongoc_read_prefs_t *read_prefs,
                                const bson_t *query_bson,
                                mongoc_apply_read_prefs_result_t *result /* OUT */)
{
   mongoc_read_mode_t mode;
   const bson_t *tags = NULL;
   bson_t child;
   const char *mode_str;

   mode = mongoc_read_prefs_get_mode (read_prefs);
   if (read_prefs) {
      tags = mongoc_read_prefs_get_tags (read_prefs);
   }

   /* Server Selection Spec says:
    *
    * For mode 'primary', drivers MUST NOT set the slaveOK wire protocol flag
    *   and MUST NOT use $readPreference
    *
    * For mode 'secondary', drivers MUST set the slaveOK wire protocol flag and
    *   MUST also use $readPreference
    *
    * For mode 'primaryPreferred', drivers MUST set the slaveOK wire protocol
    *   flag and MUST also use $readPreference
    *
    * For mode 'secondaryPreferred', drivers MUST set the slaveOK wire protocol
    *   flag. If the read preference contains a non-empty tag_sets parameter,
    *   drivers MUST use $readPreference; otherwise, drivers MUST NOT use
    *   $readPreference
    *
    * For mode 'nearest', drivers MUST set the slaveOK wire protocol flag and
    *   MUST also use $readPreference
    */
   if (mode == MONGOC_READ_SECONDARY_PREFERRED && bson_empty0 (tags)) {
      result->flags |= MONGOC_QUERY_SLAVE_OK;

   } else if (mode != MONGOC_READ_PRIMARY) {
      result->flags |= MONGOC_QUERY_SLAVE_OK;

      /* Server Selection Spec: "When any $ modifier is used, including the
       * $readPreference modifier, the query MUST be provided using the $query
       * modifier".
       *
       * This applies to commands, too.
       */
      result->query_with_read_prefs = bson_new ();
      result->query_owned = true;

      if (bson_has_field (query_bson, "$query")) {
         bson_concat (result->query_with_read_prefs, query_bson);
      } else {
         bson_append_document (result->query_with_read_prefs,
                               "$query", 6, query_bson);
      }

      bson_append_document_begin (result->query_with_read_prefs,
                                  "$readPreference", 15, &child);
      mode_str = _get_read_mode_string (mode);
      bson_append_utf8 (&child, "mode", 4, mode_str, -1);
      if (!bson_empty0 (tags)) {
         bson_append_array (&child, "tags", 4, tags);
      }

      bson_append_document_end (result->query_with_read_prefs, &child);
   }
}

/*
 *--------------------------------------------------------------------------
 *
 * apply_read_preferences --
 *
 *       Update @result based on @read prefs, following the Server Selection
 *       Spec.
 *
 * Side effects:
 *       Sets @result->query_with_read_prefs and @result->flags.
 *
 *--------------------------------------------------------------------------
 */

void
apply_read_preferences (const mongoc_read_prefs_t *read_prefs,
                        const mongoc_server_stream_t *server_stream,
                        const bson_t *query_bson,
                        mongoc_query_flags_t initial_flags,
                        mongoc_apply_read_prefs_result_t *result /* OUT */)
{
   mongoc_server_description_type_t server_type;

   ENTRY;

   BSON_ASSERT (server_stream);
   BSON_ASSERT (query_bson);
   BSON_ASSERT (result);

   /* default values */
   result->query_with_read_prefs = (bson_t *) query_bson;
   result->query_owned = false;
   result->flags = initial_flags;

   server_type = server_stream->sd->type;

   switch (server_stream->topology_type) {
   case MONGOC_TOPOLOGY_SINGLE:
      if (server_type == MONGOC_SERVER_MONGOS) {
         _apply_read_preferences_mongos (read_prefs, query_bson, result);
      } else {
         /* Server Selection Spec: for topology type single and server types
          * besides mongos, "clients MUST always set the slaveOK wire protocol
          * flag on reads to ensure that any server type can handle the
          * request."
          */
         result->flags |= MONGOC_QUERY_SLAVE_OK;
      }

      break;

   case MONGOC_TOPOLOGY_RS_NO_PRIMARY:
   case MONGOC_TOPOLOGY_RS_WITH_PRIMARY:
      /* Server Selection Spec: for RS topology types, "For all read
       * preferences modes except primary, clients MUST set the slaveOK wire
       * protocol flag to ensure that any suitable server can handle the
       * request. Clients MUST  NOT set the slaveOK wire protocol flag if the
       * read preference mode is primary.
       */
      if (read_prefs && read_prefs->mode != MONGOC_READ_PRIMARY) {
         result->flags |= MONGOC_QUERY_SLAVE_OK;
      }

      break;

   case MONGOC_TOPOLOGY_SHARDED:
      _apply_read_preferences_mongos (read_prefs, query_bson, result);
      break;

   case MONGOC_TOPOLOGY_UNKNOWN:
   case MONGOC_TOPOLOGY_DESCRIPTION_TYPES:
   default:
      /* must not call _apply_read_preferences with unknown topology type */
      BSON_ASSERT (false);
   }

   EXIT;
}


void
apply_read_prefs_result_cleanup (mongoc_apply_read_prefs_result_t *result)
{
   ENTRY;

   BSON_ASSERT (result);

   if (result->query_owned) {
      bson_destroy (result->query_with_read_prefs);
   }

   EXIT;
}

/*==============================================================*/
/* --- mongoc-sasl.c */

#ifdef MONGOC_ENABLE_SASL

#ifndef SASL_CALLBACK_FN
#  define SASL_CALLBACK_FN(_f) ((int (*) (void))(_f))
#endif

void
_mongoc_sasl_set_mechanism (mongoc_sasl_t *sasl,
                            const char    *mechanism)
{
   BSON_ASSERT (sasl);

   bson_free (sasl->mechanism);
   sasl->mechanism = mechanism ? bson_strdup (mechanism) : NULL;
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

   bson_free (sasl->pass);
   sasl->pass = pass ? bson_strdup (pass) : NULL;
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

   bson_free (sasl->user);
   sasl->user = user ? bson_strdup (user) : NULL;
}


void
_mongoc_sasl_set_service_host (mongoc_sasl_t *sasl,
                               const char    *service_host)
{
   BSON_ASSERT (sasl);

   bson_free (sasl->service_host);
   sasl->service_host = service_host ? bson_strdup (service_host) : NULL;
}


void
_mongoc_sasl_set_service_name (mongoc_sasl_t *sasl,
                               const char    *service_name)
{
   BSON_ASSERT (sasl);

   bson_free (sasl->service_name);
   sasl->service_name = service_name ? bson_strdup (service_name) : NULL;
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
}


void
_mongoc_sasl_destroy (mongoc_sasl_t *sasl)
{
   BSON_ASSERT (sasl);

   if (sasl->conn) {
      sasl_dispose (&sasl->conn);
   }

   bson_free (sasl->user);
   bson_free (sasl->pass);
   bson_free (sasl->mechanism);
   bson_free (sasl->service_name);
   bson_free (sasl->service_host);
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
                         _("SASL Failure: insufficient memory."));
         break;
      case SASL_NOMECH:
         bson_set_error (error,
                         MONGOC_ERROR_SASL,
                         status,
                         _("SASL Failure: failure to negotiate mechanism"));
         break;
      case SASL_BADPARAM:
         bson_set_error (error,
                         MONGOC_ERROR_SASL,
                         status,
                         _("Bad parameter supplied. Please file a bug "
                         "with mongo-c-driver."));
         break;
      default:
         bson_set_error (error,
                         MONGOC_ERROR_SASL,
                         status,
                         _("SASL Failure: (%d): %s"),
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
                      _("SASL Failure: invalid mechanism \"%s\""),
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

   sasl->step++;

   if (sasl->step == 1) {
      return _mongoc_sasl_start (sasl, outbuf, outbufmax, outbuflen,
                                 error);
   } else if (sasl->step >= 10) {
      bson_set_error (error,
                      MONGOC_ERROR_SASL,
                      SASL_NOTDONE,
                      _("SASL Failure: maximum steps detected"));
      return false;
   }

   if (!inbuflen) {
      bson_set_error (error,
                      MONGOC_ERROR_SASL,
                      MONGOC_ERROR_CLIENT_AUTHENTICATE,
                      _("SASL Failure: no payload provided from server."));
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
   int64_t now;

   ENTRY;

   BSON_ASSERT (events);

   pfd.fd = sd;
#ifdef _WIN32
   pfd.events = events;
#else
   pfd.events = events | POLLERR | POLLHUP;
#endif
   pfd.revents = 0;

   now = bson_get_monotonic_time();

   for (;;) {
      if (expire_at < 0) {
         timeout = -1;
      } else if (expire_at == 0) {
         timeout = 0;
      } else {
         timeout = (int)((expire_at - now) / 1000L);
         if (timeout < 0) {
            timeout = 0;
         }
      }

#ifdef _WIN32
      ret = WSAPoll (&pfd, 1, timeout);
      if (ret == SOCKET_ERROR) {
         errno = WSAGetLastError();
         ret = -1;
      }
#else
      ret = poll (&pfd, 1, timeout);
#endif

      if (ret > 0) {
         /* Something happened, so return that */
#ifdef _WIN32
         RETURN (0 != (pfd.revents & (events | POLLHUP | POLLERR)));
#else
         RETURN (0 != (pfd.revents & events));
#endif
      } else if (ret < 0) {
         /* poll itself failed */

         TRACE("errno is: %d", errno);
         if (MONGOC_ERRNO_IS_AGAIN(errno)) {
            now = bson_get_monotonic_time();

            if (expire_at < now) {
               RETURN (false);
            } else {
               continue;
            }
         } else {
            /* poll failed for some non-transient reason */
            RETURN (false);
         }
      } else {
         /* poll timed out */
         RETURN (false);
      }
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_socket_poll --
 *
 *       A multi-socket poll helper.
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

ssize_t
mongoc_socket_poll (mongoc_socket_poll_t *sds,          /* IN */
                    size_t                nsds,         /* IN */
                    int32_t               timeout)      /* IN */
{
#ifdef _WIN32
   WSAPOLLFD *pfds;
#else
   struct pollfd *pfds;
#endif
   int ret;
   int i;

   ENTRY;

   BSON_ASSERT (sds);

#ifdef _WIN32
   pfds = (WSAPOLLFD *)bson_malloc(sizeof(*pfds) * nsds);
#else
   pfds = (struct pollfd *)bson_malloc(sizeof(*pfds) * nsds);
#endif

   for (i = 0; i < (int)nsds; i++) {
      pfds[i].fd = sds[i].socket->sd;
#ifdef _WIN32
      pfds[i].events = sds[i].events;
#else
      pfds[i].events = sds[i].events | POLLERR | POLLHUP;
#endif
      pfds[i].revents = 0;
   }

#ifdef _WIN32
   ret = WSAPoll (pfds, nsds, timeout);
   if (ret == SOCKET_ERROR) {
      MONGOC_WARNING ("WSAGetLastError(): %d", WSAGetLastError ());
      ret = -1;
   }
#else
   ret = poll (pfds, nsds, timeout);
#endif

   for (i = 0; i < (int)nsds; i++) {
      sds[i].revents = pfds[i].revents;
   }

   bson_free(pfds);

   return ret;
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
   TRACE("Current errno: %d", sock->errno_);
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
   TRACE("setting errno: %d", sock->errno_);
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
   TRACE("errno is: %d", sock->errno_);
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
 *       *port contains the client port number.
 *
 *--------------------------------------------------------------------------
 */

mongoc_socket_t *
mongoc_socket_accept (mongoc_socket_t *sock,      /* IN */
                      int64_t          expire_at) /* IN */
{
   return mongoc_socket_accept_ex (sock, expire_at, NULL);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_socket_accept_ex --
 *
 *       Private synonym for mongoc_socket_accept, returning client port.
 *
 * Returns:
 *       NULL upon failure to accept or timeout.
 *       A newly allocated mongoc_socket_t on success.
 *
 * Side effects:
 *       *port contains the client port number.
 *
 *--------------------------------------------------------------------------
 */

mongoc_socket_t *
mongoc_socket_accept_ex (mongoc_socket_t *sock,      /* IN */
                         int64_t          expire_at, /* IN */
                         uint16_t *port)             /* OUT */
{
   mongoc_socket_t *client;
   struct sockaddr_in addr;
   socklen_t addrlen = sizeof addr;
   bool try_again = false;
   bool failed = false;
#ifdef _WIN32
   SOCKET sd;
#else
   int sd;
#endif

   ENTRY;

   BSON_ASSERT (sock);

again:
   errno = 0;
   sd = accept (sock->sd, (struct sockaddr *) &addr, &addrlen);

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

   client = (mongoc_socket_t *)bson_malloc0 (sizeof *client);
   client->sd = sd;

   if (port) {
      *port = ntohs (addr.sin_port);
   }

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

   BSON_ASSERT (sock);
   BSON_ASSERT (addr);
   BSON_ASSERT (addrlen);

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
   ENTRY;

   BSON_ASSERT (sock);

#ifdef _WIN32
   if (sock->sd != INVALID_SOCKET) {
      shutdown (sock->sd, SD_BOTH);
      if (0 == closesocket (sock->sd)) {
         sock->sd = INVALID_SOCKET;
      } else {
         _mongoc_socket_capture_errno (sock);
         RETURN(-1);
      }
   }
   RETURN(0);
#else
   if (sock->sd != -1) {
      shutdown (sock->sd, SHUT_RDWR);
      if (0 == close (sock->sd)) {
         sock->sd = -1;
      } else {
         _mongoc_socket_capture_errno (sock);
         RETURN(-1);
      }
   }
   RETURN(0);
#endif
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

   BSON_ASSERT (sock);
   BSON_ASSERT (addr);
   BSON_ASSERT (addrlen);

   ret = connect (sock->sd, addr, addrlen);

#ifdef _WIN32
   if (ret == SOCKET_ERROR) {
#else
   if (ret == -1) {
#endif
      _mongoc_socket_capture_errno (sock);

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
         } else {
            errno = sock->errno_ = optval;
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

   BSON_ASSERT (sock);

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

   if (domain != AF_UNIX && !_mongoc_socket_setnodelay (sd)) {
      MONGOC_WARNING ("Failed to enable TCP_NODELAY.");
   }

   sock = (mongoc_socket_t *)bson_malloc0 (sizeof *sock);
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

   ENTRY;

   BSON_ASSERT (sock);
   BSON_ASSERT (buf);
   BSON_ASSERT (buflen);

again:
   sock->errno_ = 0;
#ifdef _WIN32
   ret = recv (sock->sd, (char *)buf, (int)buflen, flags);
   failed = (ret == SOCKET_ERROR);
#else
   ret = recv (sock->sd, buf, buflen, flags);
   failed = (ret == -1);
#endif
   if (failed) {
      _mongoc_socket_capture_errno (sock);
      if (_mongoc_socket_errno_is_again (sock) &&
          _mongoc_socket_wait (sock->sd, POLLIN, expire_at)) {
         GOTO (again);
      }
   }

   if (failed) {
      RETURN (-1);
   }

   mongoc_counter_streams_ingress_add (ret);

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

   BSON_ASSERT (sock);

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

   BSON_ASSERT (sock);
   BSON_ASSERT (buf);
   BSON_ASSERT (buflen);

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
 *       iovec entry one by one. This can happen if we hit EMSGSIZE
 *       with sendmsg() on various POSIX systems or WSASend()+WSAEMSGSIZE
 *       on Windows.
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
   ssize_t wrote;

   ENTRY;

   BSON_ASSERT (sock);
   BSON_ASSERT (iov);
   BSON_ASSERT (iovcnt);

   for (i = 0; i < iovcnt; i++) {
      wrote = send (sock->sd, iov [i].iov_base, iov [i].iov_len, 0);
#ifdef _WIN32
      if (wrote == SOCKET_ERROR) {
#else
      if (wrote == -1) {
#endif
         _mongoc_socket_capture_errno (sock);

         if (!_mongoc_socket_errno_is_again (sock)) {
            RETURN (-1);
         }
         RETURN (ret ? ret : -1);
      }

      ret += wrote;

      if (wrote != (ssize_t)iov [i].iov_len) {
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

static ssize_t
_mongoc_socket_try_sendv (mongoc_socket_t *sock,   /* IN */
                          mongoc_iovec_t  *iov,    /* IN */
                          size_t           iovcnt) /* IN */
{
#ifdef _WIN32
   DWORD dwNumberofBytesSent = 0;
   int ret;
#else
   struct msghdr msg;
   ssize_t ret;
#endif

   ENTRY;

   BSON_ASSERT (sock);
   BSON_ASSERT (iov);
   BSON_ASSERT (iovcnt);

   DUMP_IOVEC (sendbuf, iov, iovcnt);

#ifdef _WIN32
   ret = WSASend (sock->sd, (LPWSABUF)iov, iovcnt, &dwNumberofBytesSent,
                  0, NULL, NULL);
   TRACE("WSASend sent: %ld (out of: %ld), ret: %d", dwNumberofBytesSent, iov->iov_len, ret);
#else
   memset (&msg, 0, sizeof msg);
   msg.msg_iov = iov;
   msg.msg_iovlen = (int) iovcnt;
   ret = sendmsg (sock->sd, &msg,
# ifdef MSG_NOSIGNAL
                  MSG_NOSIGNAL);
# else
                  0);
# endif
   TRACE("Send %ld out of %ld bytes", ret, iov->iov_len);
#endif


#ifdef _WIN32
   if (ret == SOCKET_ERROR) {
#else
   if (ret == -1) {
#endif
      _mongoc_socket_capture_errno (sock);

      /*
       * Check to see if we have sent an iovec too large for sendmsg to
       * complete. If so, we need to fallback to the slow path of multiple
       * send() commands.
       */
#ifdef _WIN32
      if (mongoc_socket_errno (sock) == WSAEMSGSIZE) {
#else
      if (mongoc_socket_errno (sock) == EMSGSIZE) {
#endif
         RETURN(_mongoc_socket_try_sendv_slow (sock, iov, iovcnt));
      }

      RETURN (-1);
   }

#ifdef _WIN32
   RETURN (dwNumberofBytesSent);
#else
   RETURN (ret);
#endif
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
                     mongoc_iovec_t   *in_iov,    /* IN */
                     size_t            iovcnt,    /* IN */
                     int64_t           expire_at) /* IN */
{
   ssize_t ret = 0;
   ssize_t sent;
   size_t cur = 0;
   mongoc_iovec_t *iov;

   ENTRY;

   BSON_ASSERT (sock);
   BSON_ASSERT (in_iov);
   BSON_ASSERT (iovcnt);

   iov = bson_malloc(sizeof(*iov) * iovcnt);
   memcpy(iov, in_iov, sizeof(*iov) * iovcnt);

   for (;;) {
      sent = _mongoc_socket_try_sendv (sock, &iov [cur], iovcnt - cur);
      TRACE("Sent %ld (of %ld) out of iovcnt=%ld", sent, iov[cur].iov_len, iovcnt);

      /*
       * If we failed with anything other than EAGAIN or EWOULDBLOCK,
       * we should fail immediately as there is another issue with the
       * underlying socket.
       */
      if (sent == -1) {
         if (!_mongoc_socket_errno_is_again (sock)) {
            ret = -1;
            GOTO(CLEANUP);
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
            TRACE("still got bytes left: sent -= iov_len: %ld -= %ld", sent, iov[cur].iov_len);
            sent -= iov [cur++].iov_len;
         }

         /*
          * Check if that made us finish all of the iovecs. If so, we are done
          * sending data over the socket.
          */
         if (cur == iovcnt) {
            TRACE("%s", "Finished the iovecs");
            break;
         }

         /*
          * Increment the current iovec buffer to its proper offset and adjust
          * the number of bytes to write.
          */
         TRACE("Seeked io_base+%ld", sent);
         TRACE("Subtracting iov_len -= sent; %ld -= %ld", iov[cur].iov_len, sent);
         iov [cur].iov_base = ((char *)iov [cur].iov_base) + sent;
         iov [cur].iov_len -= sent;
         TRACE("iov_len remaining %ld", iov[cur].iov_len);

         BSON_ASSERT (iovcnt - cur);
         BSON_ASSERT (iov [cur].iov_len);
      } else if (OPERATION_EXPIRED (expire_at)) {
         GOTO(CLEANUP);
      }

      /*
       * Block on poll() until our desired condition is met.
       */
      if (!_mongoc_socket_wait (sock->sd, POLLOUT, expire_at)) {
         GOTO(CLEANUP);
      }
   }

CLEANUP:
   bson_free(iov);

   RETURN (ret);
}


int
mongoc_socket_getsockname (mongoc_socket_t *sock,    /* IN */
                           struct sockaddr *addr,    /* OUT */
                           socklen_t       *addrlen) /* INOUT */
{
   int ret;

   ENTRY;

   BSON_ASSERT (sock);

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

   BSON_ASSERT (sock);

   if ((0 == getpeername (sock->sd, &addr, &len)) &&
       (0 == getnameinfo (&addr, len, host, sizeof host, NULL, 0, 0))) {
      ret = bson_strdup (host);
      RETURN (ret);
   }

   RETURN (NULL);
}


bool
mongoc_socket_check_closed (mongoc_socket_t *sock) /* IN */
{
   bool closed = false;
   char buf [1];
   ssize_t r;

   if (_mongoc_socket_wait (sock->sd, POLLIN, 0)) {
      sock->errno_ = 0;

      r = recv (sock->sd, buf, 1, MSG_PEEK);

      if (r < 0) {
         _mongoc_socket_capture_errno (sock);
      }

      if (r < 1) {
         closed = true;
      }
   }

   return closed;
}

/*
 *
 *--------------------------------------------------------------------------
 *
 * mongoc_socket_inet_ntop --
 *
 *       Convert the ip from addrinfo into a c string.
 *
 * Returns:
 *       The value is returned into 'buffer'. The memory has to be allocated
 *       by the caller
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_socket_inet_ntop (struct addrinfo *rp,        /* IN */
                         char            *buf,       /* INOUT */
                         size_t           buflen)    /* IN */
{
   void *ptr;
   char tmp[256];

   switch (rp->ai_family) {
   case AF_INET:
      ptr = &((struct sockaddr_in *)rp->ai_addr)->sin_addr;
      inet_ntop (rp->ai_family, ptr, tmp, sizeof (tmp));
      bson_snprintf (buf, buflen, "ipv4 %s", tmp);
      break;
   case AF_INET6:
      ptr = &((struct sockaddr_in6 *)rp->ai_addr)->sin6_addr;
      inet_ntop (rp->ai_family, ptr, tmp, sizeof (tmp));
      bson_snprintf (buf, buflen, "ipv6 %s", tmp);
      break;
   default:
      bson_snprintf (buf, buflen, "unknown ip %d", rp->ai_family);
      break;
   }
}

/*==============================================================*/
/* --- mongoc-ssl.c */

#ifdef MONGOC_ENABLE_SSL

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

const mongoc_ssl_opt_t *
mongoc_ssl_opt_get_default (void)
{
   return &gMongocSslOptDefault;
}

char *
mongoc_ssl_extract_subject (const char *filename)
{
#ifdef MONGOC_ENABLE_OPENSSL
	return _mongoc_openssl_extract_subject (filename);
#elif defined(MONGOC_ENABLE_SECURE_TRANSPORT)
	return _mongoc_secure_transport_extract_subject (filename);
#else
#error "Can only extract X509 subjects using OpenSSL or Secure Transport"
#endif
}
#endif

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

   BSON_ASSERT (stream);

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
 * mongoc_stream_buffered_failed --
 *
 *       Called when a stream fails. Useful for streams that differnciate
 *       between failure and cleanup.
 *       Calls mongoc_stream_buffered_destroy() on the stream.
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
mongoc_stream_buffered_failed (mongoc_stream_t *stream) /* IN */
{
	mongoc_stream_buffered_destroy (stream);
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
   BSON_ASSERT (stream);
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
   BSON_ASSERT (buffered);
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

   BSON_ASSERT (buffered);

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

   BSON_ASSERT (buffered);

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

   BSON_ASSERT (buffered->buffer.len >= total_bytes);

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


static bool
_mongoc_stream_buffered_check_closed (mongoc_stream_t *stream) /* IN */
{
   mongoc_stream_buffered_t *buffered = (mongoc_stream_buffered_t *)stream;
   BSON_ASSERT (stream);
   return mongoc_stream_check_closed (buffered->base_stream);
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

   BSON_ASSERT (base_stream);

   stream = (mongoc_stream_buffered_t *)bson_malloc0(sizeof *stream);
   stream->stream.type = MONGOC_STREAM_BUFFERED;
   stream->stream.destroy = mongoc_stream_buffered_destroy;
   stream->stream.failed = mongoc_stream_buffered_failed;
   stream->stream.close = mongoc_stream_buffered_close;
   stream->stream.flush = mongoc_stream_buffered_flush;
   stream->stream.writev = mongoc_stream_buffered_writev;
   stream->stream.readv = mongoc_stream_buffered_readv;
   stream->stream.get_base_stream = _mongoc_stream_buffered_get_base_stream;
   stream->stream.check_closed = _mongoc_stream_buffered_check_closed;

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

   BSON_ASSERT (stream);

   BSON_ASSERT (stream->close);

   ret = stream->close(stream);

   RETURN (ret);
}


/**
 * mongoc_stream_failed:
 * @stream: A mongoc_stream_t.
 *
 * Frees any resources referenced by @stream, including the memory allocation
 * for @stream.
 * This handler is called upon stream failure, such as network errors, invalid replies
 * or replicaset reconfigures deleteing the stream
 */
void
mongoc_stream_failed (mongoc_stream_t *stream)
{
   ENTRY;

   BSON_ASSERT (stream);

   if (stream->failed) {
	   stream->failed(stream);
   } else {
	   stream->destroy(stream);
   }

   EXIT;
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

   BSON_ASSERT (stream);

   BSON_ASSERT (stream->destroy);

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
   BSON_ASSERT (stream);
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

   BSON_ASSERT (stream);
   BSON_ASSERT (iov);
   BSON_ASSERT (iovcnt);

   BSON_ASSERT (stream->writev);

   if (timeout_msec < 0) {
      timeout_msec = MONGOC_DEFAULT_TIMEOUT_MSEC;
   }

   DUMP_IOVEC (writev, iov, iovcnt);
   ret = stream->writev(stream, iov, iovcnt, timeout_msec);

   RETURN (ret);
}

/**
 * mongoc_stream_write:
 * @stream: A mongoc_stream_t.
 * @buf: A buffer to write.
 * @count: The number of bytes to write into @buf.
 *
 * Simplified access to mongoc_stream_writev(). Creates a single iovec
 * with the buffer provided.
 *
 * Returns: -1 on failure, otherwise the number of bytes write.
 */
ssize_t
mongoc_stream_write (mongoc_stream_t *stream,
                     void            *buf,
                     size_t           count,
                     int32_t          timeout_msec)
{
   mongoc_iovec_t iov;
   ssize_t ret;

   ENTRY;

   BSON_ASSERT (stream);
   BSON_ASSERT (buf);

   iov.iov_base = buf;
   iov.iov_len = count;

   BSON_ASSERT (stream->writev);

   ret = mongoc_stream_writev (stream, &iov, 1, timeout_msec);

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

   BSON_ASSERT (stream);
   BSON_ASSERT (iov);
   BSON_ASSERT (iovcnt);

   BSON_ASSERT (stream->readv);

   ret = stream->readv (stream, iov, iovcnt, min_bytes, timeout_msec);
   if (ret >= 0) {
      DUMP_IOVEC (readv, iov, iovcnt);
   }

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

   BSON_ASSERT (stream);
   BSON_ASSERT (buf);

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
   BSON_ASSERT (stream);

   if (stream->setsockopt) {
      return stream->setsockopt(stream, level, optname, optval, optlen);
   }

   return 0;
}


mongoc_stream_t *
mongoc_stream_get_base_stream (mongoc_stream_t *stream) /* IN */
{
   BSON_ASSERT (stream);

   if (stream->get_base_stream) {
      return stream->get_base_stream (stream);
   }

   return stream;
}


static mongoc_stream_t *
mongoc_stream_get_root_stream (mongoc_stream_t *stream)

{
   BSON_ASSERT (stream);

   while (stream->get_base_stream) {
      stream = stream->get_base_stream (stream);
   }

   return stream;
}

mongoc_stream_t *
mongoc_stream_get_tls_stream (mongoc_stream_t *stream) /* IN */
{
   BSON_ASSERT (stream);

   for (;stream && stream->type != MONGOC_STREAM_TLS;
        stream = stream->get_base_stream (stream));

   return stream;
}

ssize_t
mongoc_stream_poll (mongoc_stream_poll_t *streams,
                    size_t                nstreams,
                    int32_t               timeout)
{
   mongoc_stream_poll_t *poller = (mongoc_stream_poll_t *)bson_malloc(sizeof(*poller) * nstreams);

   int i;
   int last_type = 0;
   ssize_t rval = -1;

   errno = 0;

   for (i = 0; i < (int)nstreams; i++) {
      poller[i].stream = mongoc_stream_get_root_stream(streams[i].stream);
      poller[i].events = streams[i].events;
      poller[i].revents = 0;

      if (i == 0) {
         last_type = poller[i].stream->type;
      } else if (last_type != poller[i].stream->type) {
         errno = EINVAL;
         goto CLEANUP;
      }
   }

   if (! poller[0].stream->poll) {
      errno = EINVAL;
      goto CLEANUP;
   }

   rval = poller[0].stream->poll(poller, nstreams, timeout);

   if (rval > 0) {
      for (i = 0; i < (int)nstreams; i++) {
         streams[i].revents = poller[i].revents;
      }
   }

CLEANUP:
   bson_free(poller);

   return rval;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_wait --
 *
 *       Internal helper, poll a single stream until it connects.
 *
 *       For now, only the POLLOUT (connected) event is supported.
 *
 *       @expire_at should be an absolute time at which to expire using
 *       the monotonic clock (bson_get_monotonic_time(), which is in
 *       microseconds). expire_at of 0 or -1 is prohibited.
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

bool
mongoc_stream_wait (mongoc_stream_t *stream,
                    int64_t expire_at)
{
   mongoc_stream_poll_t poller;
   int64_t now;
   int32_t timeout_msec;
   ssize_t ret;

   ENTRY;

   BSON_ASSERT (stream);
   BSON_ASSERT (expire_at > 0);

   poller.stream = stream;
   poller.events = POLLOUT;
   poller.revents = 0;

   now = bson_get_monotonic_time();

   for (;;) {
      /* TODO CDRIVER-804 use int64_t for timeouts consistently */
      timeout_msec = (int32_t) BSON_MIN ((expire_at - now) / 1000L, INT32_MAX);
      if (timeout_msec < 0) {
         timeout_msec = 0;
      }

      ret = mongoc_stream_poll (&poller, 1, timeout_msec);

      if (ret > 0) {
         /* an event happened, return true if POLLOUT else false */
         RETURN (0 != (poller.revents & POLLOUT));
      } else if (ret < 0) {
         /* poll itself failed */

         TRACE("errno is: %d", errno);
         if (MONGOC_ERRNO_IS_AGAIN(errno)) {
            now = bson_get_monotonic_time();

            if (expire_at < now) {
               RETURN (false);
            } else {
               continue;
            }
         } else {
            /* poll failed for some non-transient reason */
            RETURN (false);
         }
      } else {
         /* poll timed out */
         RETURN (false);
      }
   }

   return true;
}

bool
mongoc_stream_check_closed (mongoc_stream_t *stream)
{
   int ret;

   ENTRY;

   if (!stream) {
      return true;
   }

   ret = stream->check_closed(stream);

   RETURN (ret);
}

bool
_mongoc_stream_writev_full (mongoc_stream_t *stream,
                            mongoc_iovec_t  *iov,
                            size_t           iovcnt,
                            int32_t          timeout_msec,
                            bson_error_t    *error)
{
   size_t total_bytes = 0;
   int i;
   ssize_t r;
   ENTRY;

   for (i = 0; i < (int)iovcnt; i++) {
      total_bytes += iov[i].iov_len;
   }

   r = mongoc_stream_writev(stream, iov, iovcnt, timeout_msec);
   TRACE("writev returned: %ld", r);

   if (r < 0) {
      if (error) {
         char buf[128];
         char *errstr;

         errstr = bson_strerror_r(errno, buf, sizeof(buf));

         bson_set_error (error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_SOCKET,
                         _("Failure during socket delivery: %s (%d)"), errstr, errno);
      }

      RETURN(false);
   }

   if (r != (ssize_t)total_bytes) {
      bson_set_error (error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_SOCKET,
                      _("Failure to send all requested bytes (only sent: %" PRIu64 "/%" PRId64 " in %dms) during socket delivery"),
                      (uint64_t) r, (int64_t)total_bytes, timeout_msec);

      RETURN(false);
   }

   RETURN(true);
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

   BSON_ASSERT (file);

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

   BSON_ASSERT (file);

   if (file->fd) {
      _mongoc_stream_file_close (stream);
   }

   bson_free (file);

   EXIT;
}


static void
_mongoc_stream_file_failed (mongoc_stream_t *stream)
{
   ENTRY;

   _mongoc_stream_file_destroy (stream);

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


static bool
_mongoc_stream_file_check_closed (mongoc_stream_t *stream) /* IN */
{
   return false;
}


mongoc_stream_t *
mongoc_stream_file_new (int fd) /* IN */
{
   mongoc_stream_file_t *stream;

   BSON_ASSERT (fd != -1);

   stream = (mongoc_stream_file_t *)bson_malloc0 (sizeof *stream);
   stream->vtable.type = MONGOC_STREAM_FILE;
   stream->vtable.close = _mongoc_stream_file_close;
   stream->vtable.destroy = _mongoc_stream_file_destroy;
   stream->vtable.failed = _mongoc_stream_file_failed;
   stream->vtable.flush = _mongoc_stream_file_flush;
   stream->vtable.readv = _mongoc_stream_file_readv;
   stream->vtable.writev = _mongoc_stream_file_writev;
   stream->vtable.check_closed = _mongoc_stream_file_check_closed;
   stream->fd = fd;

   return (mongoc_stream_t *)stream;
}


mongoc_stream_t *
mongoc_stream_file_new_for_path (const char *path,  /* IN */
                                 int         flags, /* IN */
                                 int         mode)  /* IN */
{
   int fd = -1;

   BSON_ASSERT (path);

#ifdef _WIN32
   if (_sopen_s (&fd, path, (flags | _O_BINARY), _SH_DENYNO, mode) != 0) {
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
   BSON_ASSERT (stream);

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


static void
_mongoc_stream_gridfs_failed (mongoc_stream_t *stream)
{
   ENTRY;

   _mongoc_stream_gridfs_destroy (stream);

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


static bool
_mongoc_stream_gridfs_check_closed (mongoc_stream_t *stream) /* IN */
{
   return false;
}


mongoc_stream_t *
mongoc_stream_gridfs_new (mongoc_gridfs_file_t *file)
{
   mongoc_stream_gridfs_t *stream;

   ENTRY;

   BSON_ASSERT (file);

   stream = (mongoc_stream_gridfs_t *)bson_malloc0 (sizeof *stream);
   stream->file = file;
   stream->stream.type = MONGOC_STREAM_GRIDFS;
   stream->stream.destroy = _mongoc_stream_gridfs_destroy;
   stream->stream.failed = _mongoc_stream_gridfs_failed;
   stream->stream.close = _mongoc_stream_gridfs_close;
   stream->stream.flush = _mongoc_stream_gridfs_flush;
   stream->stream.writev = _mongoc_stream_gridfs_writev;
   stream->stream.readv = _mongoc_stream_gridfs_readv;
   stream->stream.check_closed = _mongoc_stream_gridfs_check_closed;

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

   BSON_ASSERT (ss);

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

   BSON_ASSERT (ss);

   if (ss->sock) {
      mongoc_socket_destroy (ss->sock);
      ss->sock = NULL;
   }

   bson_free (ss);

   EXIT;
}


static void
_mongoc_stream_socket_failed (mongoc_stream_t *stream)
{
   ENTRY;

   _mongoc_stream_socket_destroy (stream);

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

   BSON_ASSERT (ss);
   BSON_ASSERT (ss->sock);

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

   BSON_ASSERT (ss);
   BSON_ASSERT (ss->sock);

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


static ssize_t
_mongoc_stream_socket_poll (mongoc_stream_poll_t *streams,
                            size_t                nstreams,
                            int32_t               timeout_msec)

{
   int i;
   ssize_t ret = -1;
   mongoc_socket_poll_t *sds;
   mongoc_stream_socket_t *ss;

   ENTRY;

   sds = (mongoc_socket_poll_t *)bson_malloc(sizeof(*sds) * nstreams);

   for (i = 0; i < (int)nstreams; i++) {
      ss = (mongoc_stream_socket_t *)streams[i].stream;

      if (! ss->sock) {
         goto CLEANUP;
      }

      sds[i].socket = ss->sock;
      sds[i].events = streams[i].events;
   }

   ret = mongoc_socket_poll(sds, nstreams, timeout_msec);

   if (ret > 0) {
      for (i = 0; i < (int)nstreams; i++) {
         streams[i].revents = sds[i].revents;
      }
   }

CLEANUP:
   bson_free(sds);

   RETURN (ret);
}


mongoc_socket_t *
mongoc_stream_socket_get_socket (mongoc_stream_socket_t *stream) /* IN */
{
   BSON_ASSERT (stream);

   return stream->sock;
}


static bool
_mongoc_stream_socket_check_closed (mongoc_stream_t *stream) /* IN */
{
   mongoc_stream_socket_t *ss = (mongoc_stream_socket_t *)stream;

   ENTRY;

   BSON_ASSERT (stream);

   if (ss->sock) {
      RETURN (mongoc_socket_check_closed (ss->sock));
   }

   RETURN (true);
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

   BSON_ASSERT (sock);

   stream = (mongoc_stream_socket_t *)bson_malloc0 (sizeof *stream);
   stream->vtable.type = MONGOC_STREAM_SOCKET;
   stream->vtable.close = _mongoc_stream_socket_close;
   stream->vtable.destroy = _mongoc_stream_socket_destroy;
   stream->vtable.failed = _mongoc_stream_socket_failed;
   stream->vtable.flush = _mongoc_stream_socket_flush;
   stream->vtable.readv = _mongoc_stream_socket_readv;
   stream->vtable.writev = _mongoc_stream_socket_writev;
   stream->vtable.setsockopt = _mongoc_stream_socket_setsockopt;
   stream->vtable.check_closed = _mongoc_stream_socket_check_closed;
   stream->vtable.poll = _mongoc_stream_socket_poll;
   stream->sock = sock;

   return (mongoc_stream_t *)stream;
}

/*==============================================================*/
/* --- mongoc-stream-tls.c */

#ifdef MONGOC_ENABLE_SSL

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "stream-tls"


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
   mongoc_stream_tls_t *stream_tls = (mongoc_stream_tls_t *)mongoc_stream_get_tls_stream (stream);

   BSON_ASSERT (stream_tls);
   BSON_ASSERT (stream_tls->do_handshake);

   return stream_tls->do_handshake(stream, timeout_msec);
}


/**
 * mongoc_stream_tls_should_retry:
 *
 * If the stream should be retried
 */
bool
mongoc_stream_tls_should_retry (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *stream_tls = (mongoc_stream_tls_t *)mongoc_stream_get_tls_stream (stream);

   BSON_ASSERT (stream_tls);
   BSON_ASSERT (stream_tls->should_retry);

   return stream_tls->should_retry(stream);
}


/**
 * mongoc_stream_tls_should_read:
 *
 * If the stream should read
 */
bool
mongoc_stream_tls_should_read (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *stream_tls = (mongoc_stream_tls_t *)mongoc_stream_get_tls_stream (stream);

   BSON_ASSERT (stream_tls);
   BSON_ASSERT (stream_tls->should_read);

   return stream_tls->should_read(stream);
}


/**
 * mongoc_stream_tls_should_write:
 *
 * If the stream should write
 */
bool
mongoc_stream_tls_should_write (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *stream_tls = (mongoc_stream_tls_t *)mongoc_stream_get_tls_stream (stream);

   BSON_ASSERT (stream_tls);
   BSON_ASSERT (stream_tls->should_write);

   return stream_tls->should_write(stream);
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
   mongoc_stream_tls_t *stream_tls = (mongoc_stream_tls_t *)mongoc_stream_get_tls_stream (stream);

   BSON_ASSERT (stream_tls);
   BSON_ASSERT (stream_tls->check_cert);

   return stream_tls->check_cert(stream, host);
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
   BSON_ASSERT (base_stream);

   switch(MONGOC_TLS_TYPE)
   {
#ifdef MONGOC_ENABLE_OPENSSL
      case MONGOC_TLS_OPENSSL:
         return mongoc_stream_tls_openssl_new (base_stream, opt, client);
         break;
#endif

      default:
         MONGOC_ERROR("Unknown crypto engine");
         return NULL;
   }
}

#endif

/*==============================================================*/
/* --- mongoc-uri.c */

struct _mongoc_uri_t
{
   char                   *str;
   mongoc_host_list_t     *hosts;
   char                   *username;
   char                   *password;
   char                   *database;
   bson_t                  options;
   bson_t                  credentials;
   mongoc_read_prefs_t    *read_prefs;
   mongoc_read_concern_t  *read_concern;
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

void
mongoc_uri_lowercase_hostname (const char *src,
                               char *buf /* OUT */,
                               int len)
{
   bson_unichar_t c;
   const char *iter;
   char *buf_iter;

   /* TODO: this code only accepts ascii, and assumes that lowercased
      chars are the same width as originals */
   for (iter = src, buf_iter = buf;
        iter && *iter && (c = bson_utf8_get_char(iter)) && buf_iter - buf < len;
        iter = bson_utf8_next_char(iter), buf_iter++) {
      assert(c < 128);
      *buf_iter = tolower(c);
   }
}

void
mongoc_uri_append_host (mongoc_uri_t  *uri,
                        const char    *host,
                        uint16_t       port)
{
   mongoc_host_list_t *iter;
   mongoc_host_list_t *link_;

   link_ = (mongoc_host_list_t *)bson_malloc0(sizeof *link_);
   mongoc_uri_lowercase_hostname(host, link_->host, sizeof link_->host);
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

/*
 *--------------------------------------------------------------------------
 *
 * scan_to_unichar --
 *
 *       Scans 'str' until either a character matching 'match' is found,
 *       until one of the characters in 'terminators' is encountered, or
 *       until we reach the end of 'str'.
 *
 *       NOTE: 'terminators' may not include multibyte UTF-8 characters.
 *
 * Returns:
 *       If 'match' is found, returns a copy of the section of 'str' before
 *       that character.  Otherwise, returns NULL.
 *
 * Side Effects:
 *       If 'match' is found, sets 'end' to begin at the matching character
 *       in 'str'.
 *
 *--------------------------------------------------------------------------
 */

static char *
scan_to_unichar (const char      *str,
                 bson_unichar_t   match,
                 const char      *terminators,
                 const char     **end)
{
   bson_unichar_t c;
   const char *iter;

   for (iter = str;
        iter && *iter && (c = bson_utf8_get_char(iter));
        iter = bson_utf8_next_char(iter)) {
      if (c == match) {
         *end = iter;
         return bson_strndup(str, iter - str);
      } else if (c == '\\') {
         iter = bson_utf8_next_char(iter);
         if (!bson_utf8_get_char(iter)) {
            break;
         }
      } else {
         const char *term_iter;
         for (term_iter = terminators; *term_iter; term_iter++) {
            if (c == (bson_unichar_t)*term_iter) {
               return NULL;
            }
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

   if ((s = scan_to_unichar(str, '@', "", &end_userpass))) {
      if ((uri->username = scan_to_unichar(s, ':', "", &end_user))) {
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
mongoc_uri_parse_port (uint16_t   *port,
                       const char *str)
{
   unsigned long ul_port;

   ul_port = strtoul (str, NULL, 10);

   if (ul_port == 0 || ul_port > UINT16_MAX) {
      /* Parse error or port number out of range. mongod prohibits port 0. */
      return false;
   }

   *port = (uint16_t)ul_port;
   return true;
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
      if (!mongoc_uri_parse_port(&port, portstr + 1)) {
         return false;
      }
   }

   hostname = scan_to_unichar (str + 1, ']', "", &end_host);

   mongoc_uri_do_unescape (&hostname);
   if (!hostname) {
      return false;
   }

   mongoc_uri_append_host (uri, hostname, port);
   bson_free (hostname);

   return true;
}


bool
mongoc_uri_parse_host (mongoc_uri_t  *uri,
                       const char    *str)
{
   uint16_t port;
   const char *end_host;
   char *hostname;

   if (*str == '[' && strchr (str, ']')) {
      return mongoc_uri_parse_host6 (uri, str);
   }

   if ((hostname = scan_to_unichar(str, ':', "?/,", &end_host))) {
      end_host++;
      if (!mongoc_uri_parse_port(&port, end_host)) {
         bson_free (hostname);
         return false;
      }
   } else {
      hostname = bson_strdup(str);
      port = MONGOC_DEFAULT_PORT;
   }

   mongoc_uri_do_unescape(&hostname);
   if (!hostname) {
      /* invalid */
      bson_free (hostname);
      return false;
   }

   mongoc_uri_append_host(uri, hostname, port);
   bson_free(hostname);

   return true;
}


bool
_mongoc_host_list_from_string (mongoc_host_list_t *host_list,
                               const char         *host_and_port)
{
   bool rval = false;
   char *uri_str = NULL;
   mongoc_uri_t *uri = NULL;
   const mongoc_host_list_t *uri_hl;

   BSON_ASSERT (host_list);
   BSON_ASSERT (host_and_port);

   uri_str = bson_strdup_printf("mongodb://%s/", host_and_port);
   if (! uri_str) goto CLEANUP;

   uri = mongoc_uri_new(uri_str);
   if (! uri) goto CLEANUP;

   uri_hl = mongoc_uri_get_hosts(uri);
   if (uri_hl->next) goto CLEANUP;

   memcpy(host_list, uri_hl, sizeof(*uri_hl));

   rval = true;

CLEANUP:

   bson_free(uri_str);
   if (uri) mongoc_uri_destroy(uri);

   return rval;
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
   } else if ((s = scan_to_unichar(str, ',', "/", &end_hostport))) {
      if (!mongoc_uri_parse_host(uri, s)) {
         bson_free(s);
         return false;
      }
      bson_free(s);
      str = end_hostport + 1;
      ret = true;
      goto again;
   } else if ((s = scan_to_unichar(str, '/', "", &end_hostport)) ||
              (s = scan_to_unichar(str, '?', "", &end_hostport))) {
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

   if ((uri->database = scan_to_unichar(str, '?', "", &end_database))) {
      *end = end_database;
   } else if (*str) {
      uri->database = bson_strdup(str);
      *end = str + strlen(str);
   }

   mongoc_uri_do_unescape(&uri->database);
   if (!uri->database) {
      /* invalid */
      return false;
   }

   return true;
}


static bool
mongoc_uri_parse_auth_mechanism_properties (mongoc_uri_t *uri,
                                            const char   *str)
{
   char *field;
   char *value;
   const char *end_scan;
   bson_t properties;

   bson_init(&properties);

   /* build up the properties document */
   while ((field = scan_to_unichar(str, ':', "&", &end_scan))) {
      str = end_scan + 1;
      if (!(value = scan_to_unichar(str, ',', ":&", &end_scan))) {
         value = bson_strdup(str);
         str = "";
      } else {
         str = end_scan + 1;
      }
      bson_append_utf8(&properties, field, -1, value, -1);
      bson_free(field);
      bson_free(value);
   }

   /* append our auth properties to our credentials */
   bson_append_document(&uri->credentials, "mechanismProperties",
                        -1, (const bson_t *)&properties);
   return true;
}

static void
mongoc_uri_parse_tags (mongoc_uri_t *uri, /* IN */
                       const char   *str) /* IN */
{
   const char *end_keyval;
   const char *end_key;
   bson_t b;
   char *keyval;
   char *key;

   bson_init(&b);

again:
   if ((keyval = scan_to_unichar(str, ',', "", &end_keyval))) {
      if ((key = scan_to_unichar(keyval, ':', "", &end_key))) {
         bson_append_utf8(&b, key, -1, end_key + 1, -1);
         bson_free(key);
      }
      bson_free(keyval);
      str = end_keyval + 1;
      goto again;
   } else {
       if ((key = scan_to_unichar(str, ':', "", &end_key))) {
         bson_append_utf8(&b, key, -1, end_key + 1, -1);
         bson_free(key);
      }
   }

   mongoc_read_prefs_add_tag(uri->read_prefs, &b);
   bson_destroy(&b);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_uri_bson_append_or_replace_key --
 *
 *
 *       Appends 'option' to the end of 'options' if not already set.
 *
 *       Since we cannot grow utf8 strings inline, we have to allocate a temporary
 *       bson variable and splice in the new value if the key is already set.
 *
 *       NOTE: This function keeps the order of the BSON keys.
 *
 *       NOTE: 'option' is case*in*sensitive.
 *
 *
 *--------------------------------------------------------------------------
 */

static void
mongoc_uri_bson_append_or_replace_key (bson_t *options, const char *option, const char *value)
{
   bson_iter_t iter;
   bool found = false;

   if (bson_iter_init (&iter, options)) {
      bson_t tmp = BSON_INITIALIZER;

      while (bson_iter_next (&iter)) {
         const bson_value_t *bvalue;

         if (!strcasecmp(bson_iter_key (&iter), option)) {
            bson_append_utf8(&tmp, option, -1, value, -1);
            found = true;
            continue;
         }

         bvalue = bson_iter_value (&iter);
         BSON_APPEND_VALUE (&tmp, bson_iter_key (&iter), bvalue);
      }

      if (! found) {
         bson_append_utf8(&tmp, option, -1, value, -1);
      }

      bson_destroy (options);
      bson_copy_to (&tmp, options);
      bson_destroy (&tmp);
   }
}


bool
mongoc_uri_option_is_int32 (const char *key)
{
   return !strcasecmp(key, "connecttimeoutms") ||
       !strcasecmp(key, "heartbeatfrequencyms") ||
       !strcasecmp(key, "serverselectiontimeoutms") ||
       !strcasecmp(key, "socketcheckintervalms") ||
       !strcasecmp(key, "sockettimeoutms") ||
       !strcasecmp(key, "localthresholdms") ||
       !strcasecmp(key, "maxpoolsize") ||
       !strcasecmp(key, "minpoolsize") ||
       !strcasecmp(key, "maxidletimems") ||
       !strcasecmp(key, "waitqueuemultiple") ||
       !strcasecmp(key, "waitqueuetimeoutms") ||
       !strcasecmp(key, "wtimeoutms");
}

bool
mongoc_uri_option_is_bool (const char *key)
{
   return !strcasecmp(key, "canonicalizeHostname") ||
              !strcasecmp(key, "journal") ||
              !strcasecmp(key, "safe") ||
              !strcasecmp(key, "serverSelectionTryOnce") ||
              !strcasecmp(key, "slaveok") ||
              !strcasecmp(key, "ssl");
}

bool
mongoc_uri_option_is_utf8 (const char *key)
{
   if (mongoc_uri_option_is_bool(key) || mongoc_uri_option_is_int32(key)) {
      return false;
   }

   if (!strcasecmp(key, "readpreferencetags") ||
         !strcasecmp(key, "authmechanismproperties")) {
      return false;
   }

   if (!strcasecmp(key, "username") || !strcasecmp(key, "password")
         || !strcasecmp(key, "authsource") || !strcasecmp(key, "database")) {
      return false;
   }

   return true;
}

static bool
mongoc_uri_parse_option (mongoc_uri_t *uri,
                         const char   *str)
{
   int32_t v_int;
   const char *end_key;
   char *key = NULL;
   char *value = NULL;
   bool ret = false;

   if (!(key = scan_to_unichar(str, '=', "", &end_key))) {
      goto CLEANUP;
   }

   value = bson_strdup(end_key + 1);
   mongoc_uri_do_unescape(&value);
   if (!value) {
      /* do_unescape detected invalid UTF-8 and freed value */
      goto CLEANUP;
   }

   if (mongoc_uri_option_is_int32(key)) {
      v_int = (int) strtol (value, NULL, 10);
      BSON_APPEND_INT32 (&uri->options, key, v_int);
   } else if (!strcasecmp(key, "w")) {
      if (*value == '-' || isdigit(*value)) {
         v_int = (int) strtol (value, NULL, 10);
         BSON_APPEND_INT32 (&uri->options, "w", v_int);
      } else if (0 == strcasecmp (value, "majority")) {
         BSON_APPEND_UTF8 (&uri->options, "w", "majority");
      } else if (*value) {
         BSON_APPEND_UTF8 (&uri->options, "w", value);
      }
   } else if (mongoc_uri_option_is_bool(key)) {
      bson_append_bool (&uri->options, key, -1,
                        (0 == strcasecmp (value, "true")) ||
                        (0 == strcasecmp (value, "t")) ||
                        (0 == strcmp (value, "1")));
   } else if (!strcasecmp(key, "readpreferencetags")) {
      mongoc_uri_parse_tags(uri, value);
   } else if (!strcasecmp(key, "authmechanism") ||
              !strcasecmp(key, "authsource")) {
      bson_append_utf8(&uri->credentials, key, -1, value, -1);
   } else if (!strcasecmp(key, "readconcernlevel")) {
      mongoc_read_concern_set_level (uri->read_concern, value);
   } else if (!strcasecmp(key, "authmechanismproperties")) {
      if (!mongoc_uri_parse_auth_mechanism_properties(uri, value)) {
         bson_free(key);
         bson_free(value);
         return false;
      }
   } else {
      bson_append_utf8(&uri->options, key, -1, value, -1);
   }

   ret = true;

CLEANUP:
   bson_free(key);
   bson_free(value);

   return ret;
}


static bool
mongoc_uri_parse_options (mongoc_uri_t *uri,
                          const char   *str)
{
   const char *end_option;
   char *option;

again:
   if ((option = scan_to_unichar(str, '&', "", &end_option))) {
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
mongoc_uri_finalize_auth (mongoc_uri_t *uri) {
   bson_iter_t iter;
   const char *source = NULL;
   const char *mechanism = mongoc_uri_get_auth_mechanism(uri);

   if (bson_iter_init_find_case(&iter, &uri->credentials, "authSource")) {
      source = bson_iter_utf8(&iter, NULL);
   }

   /* authSource with GSSAPI or X509 should always be external */
   if (mechanism) {
      if (!strcasecmp(mechanism, "GSSAPI") ||
          !strcasecmp(mechanism, "MONGODB-X509")) {
         if (source) {
            if (strcasecmp(source, "$external")) {
               return false;
            }
         } else {
            bson_append_utf8(&uri->credentials, "authsource", -1, "$external", -1);
         }
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

   return mongoc_uri_finalize_auth(uri);
}


const mongoc_host_list_t *
mongoc_uri_get_hosts (const mongoc_uri_t *uri)
{
   BSON_ASSERT (uri);
   return uri->hosts;
}


const char *
mongoc_uri_get_replica_set (const mongoc_uri_t *uri)
{
   bson_iter_t iter;

   BSON_ASSERT (uri);

   if (bson_iter_init_find_case(&iter, &uri->options, "replicaSet") &&
       BSON_ITER_HOLDS_UTF8(&iter)) {
      return bson_iter_utf8(&iter, NULL);
   }

   return NULL;
}


const bson_t *
mongoc_uri_get_credentials (const mongoc_uri_t *uri)
{
   BSON_ASSERT (uri);
   return &uri->credentials;
}


const char *
mongoc_uri_get_auth_mechanism (const mongoc_uri_t *uri)
{
   bson_iter_t iter;

   BSON_ASSERT (uri);

   if (bson_iter_init_find_case (&iter, &uri->credentials, "authMechanism") &&
       BSON_ITER_HOLDS_UTF8 (&iter)) {
      return bson_iter_utf8 (&iter, NULL);
   }

   return NULL;
}


bool
mongoc_uri_get_mechanism_properties (const mongoc_uri_t *uri, bson_t *properties)
{
   bson_iter_t iter;

   if (!uri) {
      return false;
   }

   if (bson_iter_init_find_case (&iter, &uri->credentials, "mechanismProperties") &&
      BSON_ITER_HOLDS_DOCUMENT (&iter)) {
      uint32_t len = 0;
      const uint8_t *data = NULL;

      bson_iter_document (&iter, &len, &data);
      bson_init_static (properties, data, len);

      return true;
   }

   return false;
}


static void
_mongoc_uri_assign_read_prefs_mode (mongoc_uri_t *uri) /* IN */
{
   const char *str;
   bson_iter_t iter;

   BSON_ASSERT(uri);

   if (mongoc_uri_get_option_as_bool (uri, "slaveok", false)) {
      mongoc_read_prefs_set_mode(uri->read_prefs, MONGOC_READ_SECONDARY_PREFERRED);
   }

   if (bson_iter_init_find_case(&iter, &uri->options, "readpreference") &&
       BSON_ITER_HOLDS_UTF8(&iter)) {
      str = bson_iter_utf8(&iter, NULL);

      if (0 == strcasecmp("primary", str)) {
         mongoc_read_prefs_set_mode(uri->read_prefs, MONGOC_READ_PRIMARY);
      } else if (0 == strcasecmp("primarypreferred", str)) {
         mongoc_read_prefs_set_mode(uri->read_prefs, MONGOC_READ_PRIMARY_PREFERRED);
      } else if (0 == strcasecmp("secondary", str)) {
         mongoc_read_prefs_set_mode(uri->read_prefs, MONGOC_READ_SECONDARY);
      } else if (0 == strcasecmp("secondarypreferred", str)) {
         mongoc_read_prefs_set_mode(uri->read_prefs, MONGOC_READ_SECONDARY_PREFERRED);
      } else if (0 == strcasecmp("nearest", str)) {
         mongoc_read_prefs_set_mode(uri->read_prefs, MONGOC_READ_NEAREST);
      } else {
         MONGOC_WARNING("Unsupported readPreference value [readPreference=%s].", str);
      }
   }

   /* Warn on conflict, since read preference will be validated later */
   if (mongoc_read_prefs_get_mode(uri->read_prefs) == MONGOC_READ_PRIMARY &&
       !bson_empty(mongoc_read_prefs_get_tags(uri->read_prefs))) {
      MONGOC_WARNING("Primary read preference mode conflicts with tags.");
   }
}


static void
_mongoc_uri_build_write_concern (mongoc_uri_t *uri) /* IN */
{
   mongoc_write_concern_t *write_concern;
   const char *str;
   bson_iter_t iter;
   int32_t wtimeoutms;
   int value;

   BSON_ASSERT (uri);

   write_concern = mongoc_write_concern_new ();

   if (bson_iter_init_find_case (&iter, &uri->options, "safe") &&
       BSON_ITER_HOLDS_BOOL (&iter)) {
      mongoc_write_concern_set_w (write_concern,
                                  bson_iter_bool (&iter) ? 1 : MONGOC_WRITE_CONCERN_W_UNACKNOWLEDGED);
   }

   wtimeoutms = mongoc_uri_get_option_as_int32(uri, "wtimeoutms", 0);

   if (bson_iter_init_find_case (&iter, &uri->options, "journal") &&
       BSON_ITER_HOLDS_BOOL (&iter)) {
      mongoc_write_concern_set_journal (write_concern, bson_iter_bool (&iter));
   }

   if (bson_iter_init_find_case (&iter, &uri->options, "w")) {
      if (BSON_ITER_HOLDS_INT32 (&iter)) {
         value = bson_iter_int32 (&iter);

         switch (value) {
         case MONGOC_WRITE_CONCERN_W_ERRORS_IGNORED:
         case MONGOC_WRITE_CONCERN_W_UNACKNOWLEDGED:
            /* Warn on conflict, since write concern will be validated later */
            if (mongoc_write_concern_get_journal(write_concern)) {
               MONGOC_WARNING("Journal conflicts with w value [w=%d].", value);
            }
            mongoc_write_concern_set_w(write_concern, value);
            break;
         default:
            if (value > 0) {
               mongoc_write_concern_set_w (write_concern, value);
               if (value > 1) {
                  mongoc_write_concern_set_wtimeout (write_concern, wtimeoutms);
               }
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
            mongoc_write_concern_set_wtimeout (write_concern, wtimeoutms);
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

   uri = (mongoc_uri_t *)bson_malloc0(sizeof *uri);
   bson_init(&uri->options);
   bson_init(&uri->credentials);

   /* Initialize read_prefs since tag parsing may add to it */
   uri->read_prefs = mongoc_read_prefs_new(MONGOC_READ_PRIMARY);

   /* Initialize empty read_concern */
   uri->read_concern = mongoc_read_concern_new ();

   if (!uri_string) {
      uri_string = "mongodb://127.0.0.1/";
   }

   if (!mongoc_uri_parse(uri, uri_string)) {
      mongoc_uri_destroy(uri);
      return NULL;
   }

   uri->str = bson_strdup(uri_string);

   _mongoc_uri_assign_read_prefs_mode(uri);

   if (!mongoc_read_prefs_is_valid(uri->read_prefs)) {
      mongoc_uri_destroy(uri);
      return NULL;
   }

   _mongoc_uri_build_write_concern (uri);

   if (!mongoc_write_concern_is_valid (uri->write_concern)) {
      mongoc_uri_destroy(uri);
      return NULL;
   }

   return uri;
}


mongoc_uri_t *
mongoc_uri_new_for_host_port (const char *hostname,
                              uint16_t    port)
{
   mongoc_uri_t *uri;
   char *str;

   BSON_ASSERT (hostname);
   BSON_ASSERT (port);

   str = bson_strdup_printf("mongodb://%s:%hu/", hostname, port);
   uri = mongoc_uri_new(str);
   bson_free(str);

   return uri;
}


const char *
mongoc_uri_get_username (const mongoc_uri_t *uri)
{
   BSON_ASSERT (uri);

   return uri->username;
}

bool
mongoc_uri_set_username (mongoc_uri_t *uri, const char *username)
{
   size_t len;

   BSON_ASSERT (username);

   len = strlen(username);

   if (!bson_utf8_validate (username, len, false)) {
      return false;
   }

   if (uri->username) {
      bson_free (uri->username);
   }

   uri->username = bson_strdup (username);
   return true;
}


const char *
mongoc_uri_get_password (const mongoc_uri_t *uri)
{
   BSON_ASSERT (uri);

   return uri->password;
}

bool
mongoc_uri_set_password (mongoc_uri_t *uri, const char *password)
{
   size_t len;

   BSON_ASSERT (password);

   len = strlen(password);

   if (!bson_utf8_validate (password, len, false)) {
      return false;
   }

   if (uri->password) {
      bson_free (uri->password);
   }

   uri->password = bson_strdup (password);
   return true;
}


const char *
mongoc_uri_get_database (const mongoc_uri_t *uri)
{
   BSON_ASSERT (uri);
   return uri->database;
}

bool
mongoc_uri_set_database (mongoc_uri_t *uri, const char *database)
{
   size_t len;

   BSON_ASSERT (database);

   len = strlen(database);

   if (!bson_utf8_validate (database, len, false)) {
      return false;
   }

   if (uri->database) {
      bson_free (uri->database);
   }

   uri->database = bson_strdup(database);
   return true;
}


const char *
mongoc_uri_get_auth_source (const mongoc_uri_t *uri)
{
   bson_iter_t iter;

   BSON_ASSERT (uri);

   if (bson_iter_init_find_case(&iter, &uri->credentials, "authSource")) {
      return bson_iter_utf8(&iter, NULL);
   }

   return uri->database ? uri->database : "admin";
}


bool
mongoc_uri_set_auth_source (mongoc_uri_t *uri, const char *value)
{
   size_t len;

   BSON_ASSERT (value);

   len = strlen(value);

   if (!bson_utf8_validate (value, len, false)) {
      return false;
   }

   mongoc_uri_bson_append_or_replace_key (&uri->credentials, "authSource", value);

   return true;
}

const bson_t *
mongoc_uri_get_options (const mongoc_uri_t *uri)
{
   BSON_ASSERT (uri);
   return &uri->options;
}


void
mongoc_uri_destroy (mongoc_uri_t *uri)
{
   if (uri) {
      _mongoc_host_list_destroy_all (uri->hosts);
      bson_free(uri->str);
      bson_free(uri->database);
      bson_free(uri->username);
      bson_destroy(&uri->options);
      bson_destroy(&uri->credentials);
      mongoc_read_prefs_destroy(uri->read_prefs);
      mongoc_read_concern_destroy(uri->read_concern);
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
   mongoc_uri_t       *copy;
   mongoc_host_list_t *iter;

   BSON_ASSERT (uri);

   copy = (mongoc_uri_t *)bson_malloc0(sizeof (*copy));

   copy->str      = bson_strdup (uri->str);
   copy->username = bson_strdup (uri->username);
   copy->password = bson_strdup (uri->password);
   copy->database = bson_strdup (uri->database);

   copy->read_prefs    = mongoc_read_prefs_copy (uri->read_prefs);
   copy->read_concern  = mongoc_read_concern_copy (uri->read_concern);
   copy->write_concern = mongoc_write_concern_copy (uri->write_concern);

   for (iter = uri->hosts; iter; iter = iter->next) {
      mongoc_uri_append_host (copy, iter->host, iter->port);
   }

   bson_copy_to (&uri->options, &copy->options);
   bson_copy_to (&uri->credentials, &copy->credentials);

   return copy;
}


const char *
mongoc_uri_get_string (const mongoc_uri_t *uri)
{
   BSON_ASSERT (uri);
   return uri->str;
}


const bson_t *
mongoc_uri_get_read_prefs (const mongoc_uri_t *uri)
{
   BSON_ASSERT (uri);
   return mongoc_read_prefs_get_tags(uri->read_prefs);
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

   BSON_ASSERT (escaped_string);

   len = strlen(escaped_string);

   /*
    * Double check that this is a UTF-8 valid string. Bail out if necessary.
    */
   if (!bson_utf8_validate(escaped_string, len, false)) {
      MONGOC_WARNING("%s(): escaped_string contains invalid UTF-8",
                     BSON_FUNC);
      return NULL;
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


const mongoc_read_prefs_t *
mongoc_uri_get_read_prefs_t (const mongoc_uri_t *uri) /* IN */
{
   BSON_ASSERT (uri);

   return uri->read_prefs;
}


void
mongoc_uri_set_read_prefs_t (mongoc_uri_t              *uri,
                             const mongoc_read_prefs_t *prefs)
{
   BSON_ASSERT (uri);
   BSON_ASSERT (prefs);

   mongoc_read_prefs_destroy (uri->read_prefs);
   uri->read_prefs = mongoc_read_prefs_copy (prefs);
}


const mongoc_read_concern_t *
mongoc_uri_get_read_concern (const mongoc_uri_t *uri) /* IN */
{
   BSON_ASSERT (uri);

   return uri->read_concern;
}


void
mongoc_uri_set_read_concern (mongoc_uri_t                *uri,
                             const mongoc_read_concern_t *rc)
{
   BSON_ASSERT (uri);
   BSON_ASSERT (rc);

   mongoc_read_concern_destroy (uri->read_concern);
   uri->read_concern = mongoc_read_concern_copy (rc);
}


const mongoc_write_concern_t *
mongoc_uri_get_write_concern (const mongoc_uri_t *uri) /* IN */
{
   BSON_ASSERT (uri);

   return uri->write_concern;
}


void
mongoc_uri_set_write_concern (mongoc_uri_t                 *uri,
                              const mongoc_write_concern_t *wc)
{
   BSON_ASSERT (uri);
   BSON_ASSERT (wc);

   mongoc_write_concern_destroy (uri->write_concern);
   uri->write_concern = mongoc_write_concern_copy (wc);
}


bool
mongoc_uri_get_ssl (const mongoc_uri_t *uri) /* IN */
{
   bson_iter_t iter;

   BSON_ASSERT (uri);

   return (bson_iter_init_find_case (&iter, &uri->options, "ssl") &&
           BSON_ITER_HOLDS_BOOL (&iter) &&
           bson_iter_bool (&iter));
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_uri_get_option_as_int32 --
 *
 *       Checks if the URI 'option' is set and of correct type (int32).
 *       The special value '0' is considered as "unset".
 *       This is so users can provide
 *       sprintf(mongodb://localhost/?option=%d, myvalue) style connection strings,
 *       and still apply default values.
 *
 *       If not set, or set to invalid type, 'fallback' is returned.
 *
 *       NOTE: 'option' is case*in*sensitive.
 *
 * Returns:
 *       The value of 'option' if available as int32 (and not 0), or 'fallback'.
 *
 *--------------------------------------------------------------------------
 */

int32_t
mongoc_uri_get_option_as_int32 (const mongoc_uri_t *uri,
                                const char         *option,
                                int32_t             fallback)
{
   const bson_t *options;
   bson_iter_t iter;
   int32_t retval = fallback;

   if ((options = mongoc_uri_get_options (uri)) &&
        bson_iter_init_find_case (&iter, options, option) &&
        BSON_ITER_HOLDS_INT32 (&iter)) {

      if (!(retval = bson_iter_int32(&iter))) {
         retval = fallback;
      }
   }

   return retval;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_uri_set_option_as_int32 --
 *
 *       Sets a URI option 'after the fact'. Allows users to set individual
 *       URI options without passing them as a connection string.
 *
 *       Only allows a set of known options to be set.
 *       @see mongoc_uri_option_is_int32 ().
 *
 *       Does in-place-update of the option BSON if 'option' is already set.
 *       Appends the option to the end otherwise.
 *
 *       NOTE: If 'option' is already set, and is of invalid type, this
 *       function will return false.
 *
 *       NOTE: 'option' is case*in*sensitive.
 *
 * Returns:
 *       true on successfully setting the option, false on failure.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_uri_set_option_as_int32 (mongoc_uri_t *uri,
                                const char   *option,
                                int32_t       value)
{
   const bson_t *options;
   bson_iter_t iter;

   BSON_ASSERT (option);

   if (!mongoc_uri_option_is_int32 (option)) {
      return false;
   }

   if ((options = mongoc_uri_get_options (uri)) &&
         bson_iter_init_find_case (&iter, options, option)) {
      if (BSON_ITER_HOLDS_INT32 (&iter)) {
         bson_iter_overwrite_int32 (&iter, value);
         return true;
      } else {
         return false;
      }
   }

   bson_append_int32(&uri->options, option, -1, value);
   return true;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_uri_get_option_as_bool --
 *
 *       Checks if the URI 'option' is set and of correct type (bool).
 *
 *       If not set, or set to invalid type, 'fallback' is returned.
 *
 *       NOTE: 'option' is case*in*sensitive.
 *
 * Returns:
 *       The value of 'option' if available as bool, or 'fallback'.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_uri_get_option_as_bool (const mongoc_uri_t *uri,
                               const char         *option,
                               bool                fallback)
{
   const bson_t *options;
   bson_iter_t iter;

   if ((options = mongoc_uri_get_options (uri)) &&
       bson_iter_init_find_case (&iter, options, option) &&
       BSON_ITER_HOLDS_BOOL (&iter)) {
      return bson_iter_bool (&iter);
   }

   return fallback;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_uri_set_option_as_bool --
 *
 *       Sets a URI option 'after the fact'. Allows users to set individual
 *       URI options without passing them as a connection string.
 *
 *       Only allows a set of known options to be set.
 *       @see mongoc_uri_option_is_bool ().
 *
 *       Does in-place-update of the option BSON if 'option' is already set.
 *       Appends the option to the end otherwise.
 *
 *       NOTE: If 'option' is already set, and is of invalid type, this
 *       function will return false.
 *
 *       NOTE: 'option' is case*in*sensitive.
 *
 * Returns:
 *       true on successfully setting the option, false on failure.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_uri_set_option_as_bool (mongoc_uri_t *uri,
                               const char   *option,
                               bool          value)
{
   const bson_t *options;
   bson_iter_t iter;

   BSON_ASSERT (option);

   if (!mongoc_uri_option_is_bool (option)) {
      return false;
   }

   if ((options = mongoc_uri_get_options (uri)) &&
         bson_iter_init_find_case (&iter, options, option)) {
      if (BSON_ITER_HOLDS_BOOL (&iter)) {
         bson_iter_overwrite_bool (&iter, value);
         return true;
      } else {
         return false;
      }
   }
   bson_append_bool(&uri->options, option, -1, value);
   return true;

}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_uri_get_option_as_utf8 --
 *
 *       Checks if the URI 'option' is set and of correct type (utf8).
 *
 *       If not set, or set to invalid type, 'fallback' is returned.
 *
 *       NOTE: 'option' is case*in*sensitive.
 *
 * Returns:
 *       The value of 'option' if available as utf8, or 'fallback'.
 *
 *--------------------------------------------------------------------------
 */

const char*
mongoc_uri_get_option_as_utf8 (const mongoc_uri_t *uri,
                               const char         *option,
                               const char         *fallback)
{
   const bson_t *options;
   bson_iter_t iter;

   if ((options = mongoc_uri_get_options (uri)) &&
       bson_iter_init_find_case (&iter, options, option) &&
       BSON_ITER_HOLDS_UTF8 (&iter)) {
      return bson_iter_utf8 (&iter, NULL);
   }

   return fallback;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_uri_set_option_as_utf8 --
 *
 *       Sets a URI option 'after the fact'. Allows users to set individual
 *       URI options without passing them as a connection string.
 *
 *       Only allows a set of known options to be set.
 *       @see mongoc_uri_option_is_utf8 ().
 *
 *       If the option is not already set, this function will append it to the end
 *       of the options bson.
 *       NOTE: If the option is already set the entire options bson will be
 *       overwritten, containing the new option=value (at the same position).
 *
 *       NOTE: If 'option' is already set, and is of invalid type, this
 *       function will return false.
 *
 *       NOTE: 'option' must be valid utf8.
 *
 *       NOTE: 'option' is case*in*sensitive.
 *
 * Returns:
 *       true on successfully setting the option, false on failure.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_uri_set_option_as_utf8 (mongoc_uri_t *uri,
                               const char   *option,
                               const char   *value)
{
   size_t len;

   BSON_ASSERT (option);

   len = strlen(value);

   if (!bson_utf8_validate (value, len, false)) {
      return false;
   }

   if (!mongoc_uri_option_is_utf8 (option)) {
      return false;
   }

   mongoc_uri_bson_append_or_replace_key (&uri->options, option, value);

   return true;
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


void
_mongoc_usleep (int64_t usec)
{
#ifdef _WIN32
   LARGE_INTEGER ft;
   HANDLE timer;

   BSON_ASSERT (usec >= 0);

   ft.QuadPart = -(10 * usec);
   timer = CreateWaitableTimer(NULL, true, NULL);
   SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
   WaitForSingleObject(timer, INFINITE);
   CloseHandle(timer);
#else
   BSON_ASSERT (usec >= 0);
   usleep ((useconds_t) usec);
#endif
}


const char *
_mongoc_get_command_name (const bson_t *command)
{
   bson_iter_t iter;
   const char *name;
   bson_iter_t child;
   const char *wrapper_name = NULL;

   BSON_ASSERT (command);

   if (!bson_iter_init (&iter, command) ||
       !bson_iter_next (&iter)) {
      return NULL;
   }

   name = bson_iter_key (&iter);

   /* wrapped in "$query" or "query"?
    *
    *   {$query: {count: "collection"}, $readPreference: {...}}
    */
   if (name[0] == '$') {
      wrapper_name = "$query";
   } else if (!strcmp (name, "query")) {
      wrapper_name = "query";
   }

   if (wrapper_name &&
       bson_iter_init_find (&iter, command, wrapper_name) &&
       BSON_ITER_HOLDS_DOCUMENT (&iter) &&
       bson_iter_recurse (&iter, &child) &&
       bson_iter_next (&child)) {

      name = bson_iter_key (&child);
   }

   return name;
}


void
_mongoc_get_db_name (const char *ns,
                     char *db /* OUT */)
{
   size_t dblen;
   const char *dot;

   BSON_ASSERT (ns);

   dot = strstr (ns, ".");

   if (dot) {
      dblen = BSON_MIN (dot - ns + 1, MONGOC_NAMESPACE_MAX);
      bson_strncpy (db, ns, dblen);
   } else {
      bson_strncpy (db, ns, MONGOC_NAMESPACE_MAX);
   }
}

void
_mongoc_bson_destroy_if_set (bson_t *bson)
{
   if (bson) {
      bson_destroy (bson);
   }
}

/*==============================================================*/
/* --- mongoc-write-command.c */

/*
 * TODO:
 *
 *    - Remove error parameter to ops, favor result->error.
 */

#define WRITE_CONCERN_DOC(wc) \
   (wc) ? \
   (_mongoc_write_concern_get_bson((mongoc_write_concern_t*)(wc))) : \
   (&gEmptyWriteConcern)

typedef void (*mongoc_write_op_t) (mongoc_write_command_t       *command,
                                   mongoc_client_t              *client,
                                   mongoc_server_stream_t       *server_stream,
                                   const char                   *database,
                                   const char                   *collection,
                                   const mongoc_write_concern_t *write_concern,
                                   uint32_t                      offset,
                                   mongoc_write_result_t        *result,
                                   bson_error_t                 *error);


static bson_t gEmptyWriteConcern = BSON_INITIALIZER;

/* indexed by MONGOC_WRITE_COMMAND_DELETE, INSERT, UPDATE */
static const char *gCommandNames[] = { "delete", "insert", "update"};
static const char *gCommandFields[] = { "deletes", "documents", "updates"};
static const uint32_t gCommandFieldLens[] = { 7, 9, 7 };

static int32_t
_mongoc_write_result_merge_arrays (uint32_t               offset,
                                   mongoc_write_result_t *result,
                                   bson_t                *dest,
                                   bson_iter_t           *iter);

void
_mongoc_write_command_insert_append (mongoc_write_command_t *command,
                                     const bson_t           *document)
{
   const char *key;
   bson_iter_t iter;
   bson_oid_t oid;
   bson_t tmp;
   char keydata [16];

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (command->type == MONGOC_WRITE_COMMAND_INSERT);
   BSON_ASSERT (document);
   BSON_ASSERT (document->len >= 5);

   key = NULL;
   bson_uint32_to_string (command->n_documents,
                          &key, keydata, sizeof keydata);

   BSON_ASSERT (key);

   /*
    * If the document does not contain an "_id" field, we need to generate
    * a new oid for "_id".
    */
   if (!bson_iter_init_find (&iter, document, "_id")) {
      bson_init (&tmp);
      bson_oid_init (&oid, NULL);
      BSON_APPEND_OID (&tmp, "_id", &oid);
      bson_concat (&tmp, document);
      BSON_APPEND_DOCUMENT (command->documents, key, &tmp);
      bson_destroy (&tmp);
   } else {
      BSON_APPEND_DOCUMENT (command->documents, key, document);
   }

   command->n_documents++;

   EXIT;
}

void
_mongoc_write_command_update_append (mongoc_write_command_t *command,
                                     const bson_t           *selector,
                                     const bson_t           *update,
                                     bool                    upsert,
                                     bool                    multi)
{
   const char *key;
   char keydata [16];
   bson_t doc;

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (command->type == MONGOC_WRITE_COMMAND_UPDATE);
   BSON_ASSERT (selector && update);

   bson_init (&doc);
   BSON_APPEND_DOCUMENT (&doc, "q", selector);
   BSON_APPEND_DOCUMENT (&doc, "u", update);
   BSON_APPEND_BOOL (&doc, "upsert", upsert);
   BSON_APPEND_BOOL (&doc, "multi", multi);

   key = NULL;
   bson_uint32_to_string (command->n_documents, &key, keydata, sizeof keydata);
   BSON_ASSERT (key);
   BSON_APPEND_DOCUMENT (command->documents, key, &doc);
   command->n_documents++;

   bson_destroy (&doc);

   EXIT;
}

void
_mongoc_write_command_delete_append (mongoc_write_command_t *command,
                                     const bson_t           *selector)
{
   const char *key;
   char keydata [16];
   bson_t doc;

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (command->type == MONGOC_WRITE_COMMAND_DELETE);
   BSON_ASSERT (selector);

   BSON_ASSERT (selector->len >= 5);

   bson_init (&doc);
   BSON_APPEND_DOCUMENT (&doc, "q", selector);
   BSON_APPEND_INT32 (&doc, "limit", command->u.delete_.multi ? 0 : 1);

   key = NULL;
   bson_uint32_to_string (command->n_documents, &key, keydata, sizeof keydata);
   BSON_ASSERT (key);
   BSON_APPEND_DOCUMENT (command->documents, key, &doc);
   command->n_documents++;

   bson_destroy (&doc);

   EXIT;
}

void
_mongoc_write_command_init_insert (mongoc_write_command_t    *command,              /* IN */
                                   const bson_t              *document,             /* IN */
                                   mongoc_bulk_write_flags_t  flags,                /* IN */
                                   int64_t                    operation_id,         /* IN */
                                   bool                       allow_bulk_op_insert) /* IN */
{
   ENTRY;

   BSON_ASSERT (command);

   command->type = MONGOC_WRITE_COMMAND_INSERT;
   command->documents = bson_new ();
   command->n_documents = 0;
   command->flags = flags;
   command->u.insert.allow_bulk_op_insert = (uint8_t)allow_bulk_op_insert;
   command->server_id = 0;
   command->operation_id = operation_id;

   /* must handle NULL document from mongoc_collection_insert_bulk */
   if (document) {
      _mongoc_write_command_insert_append (command, document);
   }

   EXIT;
}


void
_mongoc_write_command_init_delete (mongoc_write_command_t   *command,       /* IN */
                                   const bson_t             *selector,      /* IN */
                                   bool                      multi,         /* IN */
                                   mongoc_bulk_write_flags_t flags,         /* IN */
                                   int64_t                   operation_id)  /* IN */
{
   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (selector);

   command->type = MONGOC_WRITE_COMMAND_DELETE;
   command->documents = bson_new ();
   command->n_documents = 0;
   command->u.delete_.multi = (uint8_t)multi;
   command->flags = flags;
   command->server_id = 0;
   command->operation_id = operation_id;

   _mongoc_write_command_delete_append (command, selector);

   EXIT;
}


void
_mongoc_write_command_init_update (mongoc_write_command_t   *command,       /* IN */
                                   const bson_t             *selector,      /* IN */
                                   const bson_t             *update,        /* IN */
                                   bool                      upsert,        /* IN */
                                   bool                      multi,         /* IN */
                                   mongoc_bulk_write_flags_t flags,         /* IN */
                                   int64_t                   operation_id)  /* IN */
{
   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (selector);
   BSON_ASSERT (update);

   command->type = MONGOC_WRITE_COMMAND_UPDATE;
   command->documents = bson_new ();
   command->n_documents = 0;
   command->flags = flags;
   command->server_id = 0;
   command->operation_id = operation_id;

   _mongoc_write_command_update_append (command, selector, update, upsert, multi);

   EXIT;
}


/* takes uninitialized bson_t *doc and begins formatting a write command */
static void
_mongoc_write_command_init (bson_t                       *doc,
                            mongoc_write_command_t       *command,
                            const char                   *collection,
                            const mongoc_write_concern_t *write_concern)
{
   bson_iter_t iter;

   ENTRY;

   bson_init (doc);

   if (!command->n_documents ||
       !bson_iter_init (&iter, command->documents) ||
       !bson_iter_next (&iter)) {
      EXIT;
   }

   BSON_APPEND_UTF8 (doc, gCommandNames[command->type], collection);
   BSON_APPEND_DOCUMENT (doc, "writeConcern",
                         WRITE_CONCERN_DOC (write_concern));
   BSON_APPEND_BOOL (doc, "ordered", command->flags.ordered);

   if (command->flags.bypass_document_validation !=
       MONGOC_BYPASS_DOCUMENT_VALIDATION_DEFAULT) {
      BSON_APPEND_BOOL (doc, "bypassDocumentValidation",
                        !!command->flags.bypass_document_validation);
   }

   EXIT;
}


static void
_mongoc_monitor_legacy_write (mongoc_client_t              *client,
                              mongoc_write_command_t       *command,
                              int64_t                       request_id,
                              const char                   *db,
                              const char                   *collection,
                              const mongoc_write_concern_t *write_concern,
                              mongoc_server_stream_t       *stream)
{
   bson_t doc;
   mongoc_apm_command_started_t event;

   ENTRY;

   if (!client->apm_callbacks.started) {
      EXIT;
   }

   _mongoc_write_command_init (&doc, command, collection, write_concern);

   /* copy the whole documents buffer as e.g. "updates": [...] */
   BSON_APPEND_ARRAY (&doc,
                      gCommandFields[command->type],
                      command->documents);

   mongoc_apm_command_started_init (&event,
                                    &doc,
                                    db,
                                    gCommandNames[command->type],
                                    request_id,
                                    command->operation_id,
                                    &stream->sd->host,
                                    stream->sd->id,
                                    client->apm_context);

   client->apm_callbacks.started (&event);

   mongoc_apm_command_started_cleanup (&event);
   bson_destroy (&doc);
}


static void
append_write_err (bson_t       *doc,
                  uint32_t      code,
                  const char   *errmsg,
                  size_t        errmsg_len,
                  const bson_t *errinfo)
{
   bson_t array = BSON_INITIALIZER;
   bson_t child;

   BSON_ASSERT (errmsg);

   /* writeErrors: [{index: 0, code: code, errmsg: errmsg, errInfo: {...}}] */
   bson_append_document_begin (&array, "0", 1, &child);
   bson_append_int32 (&child, "index", 5, 0);
   bson_append_int32 (&child, "code", 4, (int32_t) code);
   bson_append_utf8 (&child, "errmsg", 6, errmsg, (int) errmsg_len);
   if (errinfo) {
      bson_append_document (&child, "errInfo", 7, errinfo);
   }

   bson_append_document_end (&array, &child);
   bson_append_array (doc, "writeErrors", 11, &array);

   bson_destroy (&array);
}


static void
append_write_concern_err (bson_t       *doc,
                          const char   *errmsg,
                          size_t        errmsg_len)
{
   bson_t array = BSON_INITIALIZER;
   bson_t child;
   bson_t errinfo;

   BSON_ASSERT (errmsg);

   /* writeConcernErrors: [{code: 64,
    *                       errmsg: errmsg,
    *                       errInfo: {wtimeout: true}}] */
   bson_append_document_begin (&array, "0", 1, &child);
   bson_append_int32 (&child, "code", 4, 64);
   bson_append_utf8 (&child, "errmsg", 6, errmsg, (int) errmsg_len);
   bson_append_document_begin (&child, "errInfo", 7, &errinfo);
   bson_append_bool (&errinfo, "wtimeout", 8, true);
   bson_append_document_end (&child, &errinfo);
   bson_append_document_end (&array, &child);
   bson_append_array (doc, "writeConcernErrors", 18, &array);

   bson_destroy (&array);
}


static bool
get_upserted_id (const bson_t *update,
                 bson_value_t *upserted_id)
{
   bson_iter_t iter;
   bson_iter_t id_iter;

   /* Versions of MongoDB before 2.6 don't return the _id for an upsert if _id
    * is not an ObjectId, so find it in the update document's query "q" or
    * update "u". It must be in one or both: if it were in neither the _id
    * would be server-generated, therefore an ObjectId, therefore returned and
    * we wouldn't call this function. If _id is in both the update document
    * *and* the query spec the update document _id takes precedence.
    */

   bson_iter_init (&iter, update);

   if (bson_iter_find_descendant (&iter, "u._id", &id_iter)) {
      bson_value_copy (bson_iter_value (&id_iter), upserted_id);
      return true;
   } else {
      bson_iter_init (&iter, update);

      if (bson_iter_find_descendant (&iter, "q._id", &id_iter)) {
         bson_value_copy (bson_iter_value (&id_iter), upserted_id);
         return true;
      }
   }

   /* server bug? */
   return false;
}


static void
append_upserted (bson_t             *doc,
                 const bson_value_t *upserted_id)
{
   bson_t array = BSON_INITIALIZER;
   bson_t child;

   /* append upserted: [{index: 0, _id: upserted_id}]*/
   bson_append_document_begin (&array, "0", 1, &child);
   bson_append_int32 (&child, "index", 5, 0);
   bson_append_value (&child, "_id", 3, upserted_id);
   bson_append_document_end (&array, &child);

   bson_append_array (doc, "upserted", 8, &array);

   bson_destroy (&array);
}


static void
_mongoc_monitor_legacy_write_succeeded (mongoc_client_t        *client,
                                        int64_t                 duration,
                                        mongoc_write_command_t *command,
                                        const bson_t           *gle,
                                        int64_t                 request_id,
                                        mongoc_server_stream_t *stream)
{
   bson_iter_t iter;
   bson_t doc;
   int64_t ok = 1;
   int64_t n = 0;
   uint32_t code = 8;
   bool wtimeout = false;

   /* server error message */
   const char *errmsg = NULL;
   size_t errmsg_len = 0;

   /* server errInfo subdocument */
   bool has_errinfo = false;
   uint32_t len;
   const uint8_t *data;
   bson_t errinfo;

   /* server upsertedId value */
   bool has_upserted_id = false;
   bson_value_t upserted_id;

   /* server updatedExisting value */
   bool has_updated_existing = false;
   bool updated_existing = false;

   mongoc_apm_command_succeeded_t event;

   ENTRY;

   if (!client->apm_callbacks.succeeded) {
      EXIT;
   }

   /* first extract interesting fields from getlasterror response */
   if (gle) {
      bson_iter_init (&iter, gle);
      while (bson_iter_next (&iter)) {
         if (!strcmp (bson_iter_key (&iter), "ok")) {
            ok = bson_iter_as_int64 (&iter);
         } else if (!strcmp (bson_iter_key (&iter), "n")) {
            n = bson_iter_as_int64 (&iter);
         } else if (!strcmp (bson_iter_key (&iter), "code")) {
            code = (uint32_t) bson_iter_as_int64 (&iter);
            if (code == 0) {
               /* server sent non-numeric error code? */
               code = 8;
            }
         } else if (!strcmp (bson_iter_key (&iter), "upserted")) {
            has_upserted_id = true;
            bson_value_copy (bson_iter_value (&iter), &upserted_id);
         } else if (!strcmp (bson_iter_key (&iter), "updatedExisting")) {
            has_updated_existing = true;
            updated_existing = bson_iter_as_bool (&iter);
         } else if ((!strcmp (bson_iter_key (&iter), "err") ||
                     !strcmp (bson_iter_key (&iter), "errmsg")) &&
                    BSON_ITER_HOLDS_UTF8 (&iter)) {
            errmsg = bson_iter_utf8_unsafe (&iter, &errmsg_len);
         } else if (!strcmp (bson_iter_key (&iter), "errInfo") &&
                    BSON_ITER_HOLDS_DOCUMENT (&iter)) {
            bson_iter_document (&iter, &len, &data);
            bson_init_static (&errinfo, data, len);
            has_errinfo = true;
         } else if (!strcmp (bson_iter_key (&iter), "wtimeout")) {
            wtimeout = true;
         }
      }
   }

   /* based on PyMongo's _convert_write_result() */
   bson_init (&doc);
   bson_append_int32 (&doc, "ok", 2, (int32_t) ok);

   if (errmsg && !wtimeout) {
      /* Failure, but pass to the success callback. Command Monitoring Spec:
       * "Commands that executed on the server and return a status of {ok: 1}
       * are considered successful commands and fire CommandSucceededEvent.
       * Commands that have write errors are included since the actual command
       * did succeed, only writes failed." */
      append_write_err (&doc, code, errmsg, errmsg_len,
                        has_errinfo ? &errinfo : NULL);
   } else {
      /* Success, perhaps with a writeConcernError. */
      if (errmsg) {
         append_write_concern_err (&doc, errmsg, errmsg_len);
      }

      if (command->type == MONGOC_WRITE_COMMAND_INSERT) {
         /* GLE result for insert is always 0 in most MongoDB versions. */
         n = command->n_documents;
      } else if (command->type == MONGOC_WRITE_COMMAND_UPDATE) {
         if (has_upserted_id) {
            append_upserted (&doc, &upserted_id);
         } else if (has_updated_existing && !updated_existing && n == 1) {
            has_upserted_id = get_upserted_id (&command->documents[0],
                                               &upserted_id);

            if (has_upserted_id) {
               append_upserted (&doc, &upserted_id);
            }
         }
      }
   }

   bson_append_int32 (&doc, "n", 1, (int32_t) n);

   mongoc_apm_command_succeeded_init (&event,
                                      duration,
                                      &doc,
                                      gCommandNames[command->type],
                                      request_id,
                                      command->operation_id,
                                      &stream->sd->host,
                                      stream->sd->id,
                                      client->apm_context);

   client->apm_callbacks.succeeded (&event);

   mongoc_apm_command_succeeded_cleanup (&event);
   bson_destroy (&doc);

   if (has_upserted_id) {
      bson_value_destroy (&upserted_id);
   }

   EXIT;
}


static void
_mongoc_write_command_delete_legacy (mongoc_write_command_t       *command,
                                     mongoc_client_t              *client,
                                     mongoc_server_stream_t       *server_stream,
                                     const char                   *database,
                                     const char                   *collection,
                                     const mongoc_write_concern_t *write_concern,
                                     uint32_t                      offset,
                                     mongoc_write_result_t        *result,
                                     bson_error_t                 *error)
{
   int64_t started;
   const uint8_t *data;
   mongoc_rpc_t rpc;
   bson_iter_t iter;
   bson_iter_t q_iter;
   uint32_t len;
   bson_t *gle = NULL;
   char ns [MONGOC_NAMESPACE_MAX + 1];
   bool r;

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (server_stream);
   BSON_ASSERT (collection);

   started = bson_get_monotonic_time ();

   r = bson_iter_init (&iter, command->documents);
   if (!r) {
      BSON_ASSERT (false);
      EXIT;
   }

   if (!command->n_documents || !bson_iter_next (&iter)) {
      bson_set_error (error,
                      MONGOC_ERROR_COLLECTION,
                      MONGOC_ERROR_COLLECTION_DELETE_FAILED,
                      _("Cannot do an empty delete."));
      result->failed = true;
      EXIT;
   }

   bson_snprintf (ns, sizeof ns, "%s.%s", database, collection);

   do {
      /* the document is like { "q": { <selector> }, limit: <0 or 1> } */
      r = (bson_iter_recurse (&iter, &q_iter) &&
           bson_iter_find (&q_iter, "q") &&
           BSON_ITER_HOLDS_DOCUMENT (&q_iter));

      if (!r) {
         BSON_ASSERT (false);
         EXIT;
      }

      bson_iter_document (&q_iter, &len, &data);
      BSON_ASSERT (data);
      BSON_ASSERT (len >= 5);

      rpc.delete_.msg_len = 0;
      rpc.delete_.request_id = ++client->cluster.request_id;
      rpc.delete_.response_to = 0;
      rpc.delete_.opcode = MONGOC_OPCODE_DELETE;
      rpc.delete_.zero = 0;
      rpc.delete_.collection = ns;
      rpc.delete_.flags = command->u.delete_.multi ? MONGOC_DELETE_NONE
                         : MONGOC_DELETE_SINGLE_REMOVE;
      rpc.delete_.selector = data;

      _mongoc_monitor_legacy_write (client, command, rpc.delete_.request_id,
                                    database, collection, write_concern,
                                    server_stream);

      if (!mongoc_cluster_sendv_to_server (&client->cluster,
                                           &rpc, 1, server_stream,
                                           write_concern, error)) {
         result->failed = true;
         EXIT;
      }

      if (mongoc_write_concern_is_acknowledged (write_concern)) {
         if (!_mongoc_client_recv_gle (client, server_stream, &gle, error)) {
            result->failed = true;
            EXIT;
         }

         _mongoc_write_result_merge_legacy (
            result, command, gle,
            MONGOC_ERROR_COLLECTION_DELETE_FAILED, offset);

         offset++;
      }

      _mongoc_monitor_legacy_write_succeeded (
         client,
         bson_get_monotonic_time () - started,
         command,
         gle,
         rpc.insert.request_id,
         server_stream);

      if (gle) {
         bson_destroy (gle);
         gle = NULL;
      }

      started = bson_get_monotonic_time ();
   } while (bson_iter_next (&iter));

   EXIT;
}


/*
 *-------------------------------------------------------------------------
 *
 * too_large_error --
 *
 *       Fill a bson_error_t and optional bson_t with error info after
 *       receiving a document for bulk insert, update, or remove that is
 *       larger than max_bson_size.
 *
 *       "err_doc" should be NULL or an empty initialized bson_t.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       "error" and optionally "err_doc" are filled out.
 *
 *-------------------------------------------------------------------------
 */

static void
too_large_error (bson_error_t *error,
                 int32_t       idx,
                 int32_t       len,
                 int32_t       max_bson_size,
                 bson_t       *err_doc)
{
   /* MongoDB 2.6 uses code 2 for "too large". TODO: see CDRIVER-644 */
   const int code = 2;

   bson_set_error (error, MONGOC_ERROR_BSON, code,
                   _("Document %u is too large for the cluster. "
                   "Document is %u bytes, max is %d."),
                   idx, len, max_bson_size);

   if (err_doc) {
      BSON_APPEND_INT32 (err_doc, "index", idx);
      BSON_APPEND_UTF8 (err_doc, "err", error->message);
      BSON_APPEND_INT32 (err_doc, "code", code);
   }
}


static void
_mongoc_write_command_insert_legacy (mongoc_write_command_t       *command,
                                     mongoc_client_t              *client,
                                     mongoc_server_stream_t       *server_stream,
                                     const char                   *database,
                                     const char                   *collection,
                                     const mongoc_write_concern_t *write_concern,
                                     uint32_t                      offset,
                                     mongoc_write_result_t        *result,
                                     bson_error_t                 *error)
{
   int64_t started;
   uint32_t current_offset;
   mongoc_iovec_t *iov;
   const uint8_t *data;
   mongoc_rpc_t rpc;
   bson_iter_t iter;
   uint32_t len;
   bson_t *gle = NULL;
   uint32_t size = 0;
   bool has_more;
   char ns [MONGOC_NAMESPACE_MAX + 1];
   bool r;
   uint32_t n_docs_in_batch;
   uint32_t idx = 0;
   int32_t max_msg_size;
   int32_t max_bson_obj_size;
   bool singly;

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (server_stream);
   BSON_ASSERT (collection);
   BSON_ASSERT (command->type == MONGOC_WRITE_COMMAND_INSERT);

   started = bson_get_monotonic_time ();
   current_offset = offset;

   max_bson_obj_size = mongoc_server_stream_max_bson_obj_size (server_stream);
   max_msg_size = mongoc_server_stream_max_msg_size (server_stream);

   singly = !command->u.insert.allow_bulk_op_insert;

   r = bson_iter_init (&iter, command->documents);

   if (!r) {
      BSON_ASSERT (false);
      EXIT;
   }

   if (!command->n_documents || !bson_iter_next (&iter)) {
      bson_set_error (error,
                      MONGOC_ERROR_COLLECTION,
                      MONGOC_ERROR_COLLECTION_INSERT_FAILED,
                      _("Cannot do an empty insert."));
      result->failed = true;
      EXIT;
   }

   bson_snprintf (ns, sizeof ns, "%s.%s", database, collection);

   iov = (mongoc_iovec_t *)bson_malloc ((sizeof *iov) * command->n_documents);

again:
   has_more = false;
   n_docs_in_batch = 0;
   size = (uint32_t)(sizeof (mongoc_rpc_header_t) +
                     4 +
                     strlen (database) +
                     1 +
                     strlen (collection) +
                     1);

   do {
      BSON_ASSERT (BSON_ITER_HOLDS_DOCUMENT (&iter));
      BSON_ASSERT (n_docs_in_batch <= idx);
      BSON_ASSERT (idx < command->n_documents);

      bson_iter_document (&iter, &len, &data);

      BSON_ASSERT (data);
      BSON_ASSERT (len >= 5);

      if ((int32_t)len > max_bson_obj_size) {
         /* document is too large */
         bson_t write_err_doc = BSON_INITIALIZER;

         too_large_error (error, idx, len,
                          max_bson_obj_size, &write_err_doc);

         _mongoc_write_result_merge_legacy (
            result, command, &write_err_doc,
            MONGOC_ERROR_COLLECTION_INSERT_FAILED, offset + idx);

         bson_destroy (&write_err_doc);

         if (command->flags.ordered) {
            /* send the batch so far (if any) and return the error */
            break;
         }
      } else if ((n_docs_in_batch == 1 && singly) || size > (max_msg_size - len)) {
         /* batch is full, send it and then start the next batch */
         has_more = true;
         break;
      } else {
         /* add document to batch and continue building the batch */
         iov[n_docs_in_batch].iov_base = (void *) data;
         iov[n_docs_in_batch].iov_len = len;
         size += len;
         n_docs_in_batch++;
      }

      idx++;
   } while (bson_iter_next (&iter));

   if (n_docs_in_batch) {
      rpc.insert.msg_len = 0;
      rpc.insert.request_id = ++client->cluster.request_id;
      rpc.insert.response_to = 0;
      rpc.insert.opcode = MONGOC_OPCODE_INSERT;
      rpc.insert.flags = (
         (command->flags.ordered) ? MONGOC_INSERT_NONE
                          : MONGOC_INSERT_CONTINUE_ON_ERROR);
      rpc.insert.collection = ns;
      rpc.insert.documents = iov;
      rpc.insert.n_documents = n_docs_in_batch;

      _mongoc_monitor_legacy_write (client, command, rpc.insert.request_id,
                                    database, collection, write_concern,
                                    server_stream);

      if (!mongoc_cluster_sendv_to_server (&client->cluster,
                                           &rpc, 1, server_stream,
                                           write_concern, error)) {
         result->failed = true;
         GOTO (cleanup);
      }

      if (mongoc_write_concern_is_acknowledged (write_concern)) {
         bool err = false;
         bson_iter_t citer;

         if (!_mongoc_client_recv_gle (client, server_stream, &gle, error)) {
            result->failed = true;
            GOTO (cleanup);
         }

         err = (bson_iter_init_find (&citer, gle, "err")
                && bson_iter_as_bool (&citer));

         /*
          * Overwrite the "n" field since it will be zero. Otherwise, our
          * merge_legacy code will not know how many we tried in this batch.
          */
         if (!err &&
             bson_iter_init_find (&citer, gle, "n") &&
             BSON_ITER_HOLDS_INT32 (&citer) &&
             !bson_iter_int32 (&citer)) {
            bson_iter_overwrite_int32 (&citer, n_docs_in_batch);
         }
      }

      _mongoc_monitor_legacy_write_succeeded (
         client,
         bson_get_monotonic_time () - started,
         command,
         gle,
         rpc.insert.request_id,
         server_stream);

      started = bson_get_monotonic_time ();
   }

cleanup:

   if (gle) {
      _mongoc_write_result_merge_legacy (
         result, command, gle, MONGOC_ERROR_COLLECTION_INSERT_FAILED,
         current_offset);

      current_offset = offset + idx;
      bson_destroy (gle);
      gle = NULL;
   }

   if (has_more) {
      GOTO (again);
   }

   bson_free (iov);

   EXIT;
}


void
_empty_error (mongoc_write_command_t *command,
              bson_error_t           *error)
{
   static const uint32_t codes[] = {
      MONGOC_ERROR_COLLECTION_DELETE_FAILED,
      MONGOC_ERROR_COLLECTION_INSERT_FAILED,
      MONGOC_ERROR_COLLECTION_UPDATE_FAILED
   };

   bson_set_error (error,
                   MONGOC_ERROR_COLLECTION,
                   codes[command->type],
                   _("Cannot do an empty %s"),
                   gCommandNames[command->type]);
}


bool
_mongoc_write_command_will_overflow (uint32_t len_so_far,
                                     uint32_t document_len,
                                     uint32_t n_documents_written,
                                     int32_t  max_bson_size,
                                     int32_t  max_write_batch_size)
{
   /* max BSON object size + 16k bytes.
    * server guarantees there is enough room: SERVER-10643
    */
   uint32_t max_cmd_size = max_bson_size + 16384;

   BSON_ASSERT (max_bson_size);

   if (len_so_far + document_len > max_cmd_size) {
      return true;
   } else if (max_write_batch_size > 0 &&
              (int)n_documents_written >= max_write_batch_size) {
      return true;
   }

   return false;
}


static void
_mongoc_write_command_update_legacy (mongoc_write_command_t       *command,
                                     mongoc_client_t              *client,
                                     mongoc_server_stream_t       *server_stream,
                                     const char                   *database,
                                     const char                   *collection,
                                     const mongoc_write_concern_t *write_concern,
                                     uint32_t                      offset,
                                     mongoc_write_result_t        *result,
                                     bson_error_t                 *error)
{
   int64_t started;
   mongoc_rpc_t rpc;
   bson_iter_t iter, subiter, subsubiter;
   bson_t doc;
   bool has_update, has_selector, is_upsert;
   bson_t update, selector;
   bson_t *gle = NULL;
   const uint8_t *data = NULL;
   uint32_t len = 0;
   size_t err_offset;
   bool val = false;
   char ns [MONGOC_NAMESPACE_MAX + 1];
   int32_t affected = 0;
   int vflags = (BSON_VALIDATE_UTF8 | BSON_VALIDATE_UTF8_ALLOW_NULL
               | BSON_VALIDATE_DOLLAR_KEYS | BSON_VALIDATE_DOT_KEYS);

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (server_stream);
   BSON_ASSERT (collection);

   started = bson_get_monotonic_time ();

   bson_iter_init (&iter, command->documents);
   while (bson_iter_next (&iter)) {
      if (bson_iter_recurse (&iter, &subiter) &&
          bson_iter_find (&subiter, "u") &&
          BSON_ITER_HOLDS_DOCUMENT (&subiter)) {
         bson_iter_document (&subiter, &len, &data);
         bson_init_static (&doc, data, len);

         if (bson_iter_init (&subsubiter, &doc) &&
             bson_iter_next (&subsubiter) &&
             (bson_iter_key (&subsubiter) [0] != '$') &&
             !bson_validate (&doc, (bson_validate_flags_t)vflags, &err_offset)) {
            result->failed = true;
            bson_set_error (error,
                            MONGOC_ERROR_BSON,
                            MONGOC_ERROR_BSON_INVALID,
                            _("update document is corrupt or contains "
                            "invalid keys including $ or ."));
            EXIT;
         }
      } else {
         result->failed = true;
         bson_set_error (error,
                         MONGOC_ERROR_BSON,
                         MONGOC_ERROR_BSON_INVALID,
                         _("updates is malformed."));
         EXIT;
      }
   }

   bson_snprintf (ns, sizeof ns, "%s.%s", database, collection);

   bson_iter_init (&iter, command->documents);
   while (bson_iter_next (&iter)) {
      rpc.update.msg_len = 0;
      rpc.update.request_id = ++client->cluster.request_id;
      rpc.update.response_to = 0;
      rpc.update.opcode = MONGOC_OPCODE_UPDATE;
      rpc.update.zero = 0;
      rpc.update.collection = ns;
      rpc.update.flags = MONGOC_UPDATE_NONE;

      has_update = false;
      has_selector = false;
      is_upsert = false;

      bson_iter_recurse (&iter, &subiter);
      while (bson_iter_next (&subiter)) {
         if (strcmp (bson_iter_key (&subiter), "u") == 0) {
            bson_iter_document (&subiter, &len, &data);
            rpc.update.update = data;
            bson_init_static (&update, data, len);
            has_update = true;
         } else if (strcmp (bson_iter_key (&subiter), "q") == 0) {
            bson_iter_document (&subiter, &len, &data);
            rpc.update.selector = data;
            bson_init_static (&selector, data, len);
            has_selector = true;
         } else if (strcmp (bson_iter_key (&subiter), "multi") == 0) {
            val = bson_iter_bool (&subiter);
            if (val) {
               rpc.update.flags = (mongoc_update_flags_t)(
                     rpc.update.flags | MONGOC_UPDATE_MULTI_UPDATE);
            }
         } else if (strcmp (bson_iter_key (&subiter), "upsert") == 0) {
            val = bson_iter_bool (&subiter);
            if (val) {
               rpc.update.flags = (mongoc_update_flags_t)(
                     rpc.update.flags | MONGOC_UPDATE_UPSERT);
            }
            is_upsert = true;
         }
      }

      _mongoc_monitor_legacy_write (client, command, rpc.update.request_id,
                                    database, collection, write_concern,
                                    server_stream);

      if (!mongoc_cluster_sendv_to_server (&client->cluster,
                                           &rpc, 1, server_stream,
                                           write_concern, error)) {
         result->failed = true;
         EXIT;
      }

      if (mongoc_write_concern_is_acknowledged (write_concern)) {
         if (!_mongoc_client_recv_gle (client, server_stream, &gle, error)) {
            result->failed = true;
            EXIT;
         }

         if (bson_iter_init_find (&subiter, gle, "n") &&
             BSON_ITER_HOLDS_INT32 (&subiter)) {
            affected = bson_iter_int32 (&subiter);
         }

         /*
          * CDRIVER-372:
          *
          * Versions of MongoDB before 2.6 don't return the _id for an
          * upsert if _id is not an ObjectId.
          */
         if (is_upsert &&
             affected &&
             !bson_iter_init_find (&subiter, gle, "upserted") &&
             bson_iter_init_find (&subiter, gle, "updatedExisting") &&
             BSON_ITER_HOLDS_BOOL (&subiter) &&
             !bson_iter_bool (&subiter)) {
            if (has_update && bson_iter_init_find (&subiter, &update, "_id")) {
               _ignore_value (bson_append_iter (gle, "upserted", 8, &subiter));
            } else if (has_selector &&
                       bson_iter_init_find (&subiter, &selector, "_id")) {
               _ignore_value (bson_append_iter (gle, "upserted", 8, &subiter));
            }
         }

         _mongoc_write_result_merge_legacy (
            result, command, gle,
            MONGOC_ERROR_COLLECTION_UPDATE_FAILED, offset);

         offset++;
      }

      _mongoc_monitor_legacy_write_succeeded (
         client,
         bson_get_monotonic_time () - started,
         command,
         gle,
         rpc.update.request_id,
         server_stream);

      if (gle) {
         bson_destroy (gle);
         gle = NULL;
      }

      started = bson_get_monotonic_time ();
   }

   EXIT;
}


static mongoc_write_op_t gLegacyWriteOps[3] = {
   _mongoc_write_command_delete_legacy,
   _mongoc_write_command_insert_legacy,
   _mongoc_write_command_update_legacy };


static void
_mongoc_write_command(mongoc_write_command_t       *command,
                      mongoc_client_t              *client,
                      mongoc_server_stream_t       *server_stream,
                      const char                   *database,
                      const char                   *collection,
                      const mongoc_write_concern_t *write_concern,
                      uint32_t                      offset,
                      mongoc_write_result_t        *result,
                      bson_error_t                 *error)
{
   const uint8_t *data;
   bson_iter_t iter;
   const char *key;
   uint32_t len = 0;
   bson_t tmp;
   bson_t ar;
   bson_t cmd;
   bson_t reply;
   char str [16];
   bool has_more;
   bool ret = false;
   uint32_t i;
   int32_t max_bson_obj_size;
   int32_t max_write_batch_size;
   int32_t min_wire_version;
   uint32_t overhead;
   uint32_t key_len;

   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (database);
   BSON_ASSERT (server_stream);
   BSON_ASSERT (collection);

   max_bson_obj_size = mongoc_server_stream_max_bson_obj_size (server_stream);
   max_write_batch_size = mongoc_server_stream_max_write_batch_size (server_stream);

   /*
    * If we have an unacknowledged write and the server supports the legacy
    * opcodes, then submit the legacy opcode so we don't need to wait for
    * a response from the server.
    */

   min_wire_version = server_stream->sd->min_wire_version;
   if ((min_wire_version == 0) &&
       !mongoc_write_concern_is_acknowledged (write_concern)) {
      if (command->flags.bypass_document_validation != MONGOC_BYPASS_DOCUMENT_VALIDATION_DEFAULT) {
         bson_set_error (error,
                         MONGOC_ERROR_COMMAND,
                         MONGOC_ERROR_COMMAND_INVALID_ARG,
                         _("Cannot set bypassDocumentValidation for unacknowledged writes"));
         EXIT;
      }
      gLegacyWriteOps[command->type] (command, client, server_stream, database,
                                      collection, write_concern, offset,
                                      result, error);
      EXIT;
   }

   if (!command->n_documents ||
       !bson_iter_init (&iter, command->documents) ||
       !bson_iter_next (&iter)) {
      _empty_error (command, error);
      result->failed = true;
      EXIT;
   }

again:
   has_more = false;
   i = 0;

   _mongoc_write_command_init (&cmd, command, collection, write_concern);

   /* 1 byte to specify array type, 1 byte for field name's null terminator */
   overhead = cmd.len + 2 + gCommandFieldLens[command->type];

   if (!_mongoc_write_command_will_overflow (overhead,
                                             command->documents->len,
                                             command->n_documents,
                                             max_bson_obj_size,
                                             max_write_batch_size)) {
      /* copy the whole documents buffer as e.g. "updates": [...] */
      bson_append_array (&cmd,
                         gCommandFields[command->type],
                         gCommandFieldLens[command->type],
                         command->documents);
      i = command->n_documents;
   } else {
      bson_append_array_begin (&cmd,
                               gCommandFields[command->type],
                               gCommandFieldLens[command->type],
                               &ar);

      do {
         if (!BSON_ITER_HOLDS_DOCUMENT (&iter)) {
            BSON_ASSERT (false);
         }

         bson_iter_document (&iter, &len, &data);

         /* append array element like "0": { ... doc ... } */
         key_len = (uint32_t) bson_uint32_to_string (i, &key, str, sizeof str);

         /* 1 byte to specify document type, 1 byte for key's null terminator */
         if (_mongoc_write_command_will_overflow (overhead,
                                                  key_len + len + 2 + ar.len,
                                                  i,
                                                  max_bson_obj_size,
                                                  max_write_batch_size)) {
            has_more = true;
            break;
         }

         if (!bson_init_static (&tmp, data, len)) {
            BSON_ASSERT (false);
         }

         BSON_APPEND_DOCUMENT (&ar, key, &tmp);

         bson_destroy (&tmp);

         i++;
      } while (bson_iter_next (&iter));

      bson_append_array_end (&cmd, &ar);
   }

   if (!i) {
      too_large_error (error, i, len, max_bson_obj_size, NULL);
      result->failed = true;
      ret = false;
   } else {
      ret = mongoc_cluster_run_command_monitored (&client->cluster,
                                                  server_stream,
                                                  MONGOC_QUERY_NONE, database,
                                                  &cmd, &reply, error);

      if (!ret) {
         result->failed = true;
      }

      _mongoc_write_result_merge (result, command, &reply, offset);
      offset += i;
      bson_destroy (&reply);
   }

   bson_destroy (&cmd);

   if (has_more && (ret || !command->flags.ordered)) {
      GOTO (again);
   }

   EXIT;
}


void
_mongoc_write_command_execute (mongoc_write_command_t       *command,       /* IN */
                               mongoc_client_t              *client,        /* IN */
                               mongoc_server_stream_t       *server_stream, /* IN */
                               const char                   *database,      /* IN */
                               const char                   *collection,    /* IN */
                               const mongoc_write_concern_t *write_concern, /* IN */
                               uint32_t                      offset,        /* IN */
                               mongoc_write_result_t        *result)        /* OUT */
{
   ENTRY;

   BSON_ASSERT (command);
   BSON_ASSERT (client);
   BSON_ASSERT (server_stream);
   BSON_ASSERT (database);
   BSON_ASSERT (collection);
   BSON_ASSERT (result);

   if (!write_concern) {
      write_concern = client->write_concern;
   }

   if (!mongoc_write_concern_is_valid (write_concern)) {
      bson_set_error (&result->error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      _("The write concern is invalid."));
      result->failed = true;
      EXIT;
   }

   if (!command->server_id) {
      command->server_id = server_stream->sd->id;
   } else {
      BSON_ASSERT (command->server_id == server_stream->sd->id);
   }

   if (server_stream->sd->max_wire_version >= WIRE_VERSION_WRITE_CMD) {
      _mongoc_write_command (command, client, server_stream, database,
                             collection, write_concern, offset,
                             result, &result->error);
   } else {
      gLegacyWriteOps[command->type] (command, client, server_stream, database,
                                      collection, write_concern, offset,
                                      result, &result->error);
   }

   EXIT;
}


void
_mongoc_write_command_destroy (mongoc_write_command_t *command)
{
   ENTRY;

   if (command) {
      bson_destroy (command->documents);
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

   len = (int)bson_uint32_to_string (result->upsert_append_count, &keyptr, key,
                                     sizeof key);

   bson_append_document_begin (&result->upserted, keyptr, len, &child);
   BSON_APPEND_INT32 (&child, "index", idx);
   BSON_APPEND_VALUE (&child, "_id", value);
   bson_append_document_end (&result->upserted, &child);

   result->upsert_append_count++;
}


static void
_append_write_concern_err_legacy (mongoc_write_result_t *result,
                                  const char            *err,
                                  int32_t                code)
{
   char str[16];
   const char *key;
   size_t keylen;
   bson_t write_concern_error;

   /* don't set result->failed; record the write concern err and continue */
   keylen = bson_uint32_to_string (result->n_writeConcernErrors, &key,
                                   str, sizeof str);

   BSON_ASSERT (keylen < INT_MAX);

   bson_append_document_begin (&result->writeConcernErrors, key, (int) keylen,
                               &write_concern_error);

   bson_append_int32 (&write_concern_error, "code", 4, code);
   bson_append_utf8 (&write_concern_error, "errmsg", 6, err, -1);
   bson_append_document_end (&result->writeConcernErrors, &write_concern_error);
   result->n_writeConcernErrors++;
}


static void
_append_write_err_legacy (mongoc_write_result_t *result,
                          const char            *err,
                          int32_t                code,
                          uint32_t               offset)
{
   bson_t holder, write_errors, child;
   bson_iter_t iter;

   BSON_ASSERT (code > 0);

   bson_set_error (&result->error, MONGOC_ERROR_COLLECTION, (uint32_t) code,
                   "%s", err);

   /* stop processing, if result->ordered */
   result->failed = true;

   bson_init (&holder);
   bson_append_array_begin (&holder, "0", 1, &write_errors);
   bson_append_document_begin (&write_errors, "0", 1, &child);

   /* set error's "index" to 0; fixed up in _mongoc_write_result_merge_arrays */
   bson_append_int32 (&child, "index", 5, 0);
   bson_append_int32 (&child, "code", 4, code);
   bson_append_utf8 (&child, "errmsg", 6, err, -1);
   bson_append_document_end (&write_errors, &child);
   bson_append_array_end (&holder, &write_errors);
   bson_iter_init (&iter, &holder);
   bson_iter_next (&iter);

   _mongoc_write_result_merge_arrays (offset, result,
                                      &result->writeErrors, &iter);

   bson_destroy (&holder);
}


void
_mongoc_write_result_merge_legacy (mongoc_write_result_t  *result,  /* IN */
                                   mongoc_write_command_t *command, /* IN */
                                   const bson_t           *reply,   /* IN */
                                   mongoc_error_code_t     default_code,
                                   uint32_t                offset)
{
   const bson_value_t *value;
   bson_iter_t iter;
   bson_iter_t ar;
   bson_iter_t citer;
   const char *err = NULL;
   int32_t code = 0;
   int32_t n = 0;
   int32_t upsert_idx = 0;

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

   if (code || err) {
      if (!err) {
         err = "unknown error";
      }

      if (bson_iter_init_find (&iter, reply, "wtimeout") &&
          bson_iter_as_bool (&iter)) {

         if (!code) {
            code = (int32_t) MONGOC_ERROR_WRITE_CONCERN_ERROR;
         }

         _append_write_concern_err_legacy (result, err, code);
      } else {
         if (!code) {
            code = (int32_t) default_code;
         }

         _append_write_err_legacy (result, err, code, offset);
      }
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
         !BSON_ITER_HOLDS_ARRAY (&iter)) {
         result->nUpserted += n;
         value = bson_iter_value (&iter);
         _mongoc_write_result_append_upsert (result, offset, value);
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
                                                      offset + upsert_idx,
                                                      value);
                  upsert_idx++;
               }
            }
         }
      } else if ((n == 1) &&
                 bson_iter_init_find (&iter, reply, "updatedExisting") &&
                 BSON_ITER_HOLDS_BOOL (&iter) &&
                 !bson_iter_bool (&iter)) {
         result->nUpserted += n;
      } else {
         result->nMatched += n;
      }
      break;
   default:
      break;
   }

   result->omit_nModified = true;

   EXIT;
}


static int32_t
_mongoc_write_result_merge_arrays (uint32_t               offset,
                                   mongoc_write_result_t *result, /* IN */
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
            len = (int)bson_uint32_to_string (aridx++, &keyptr, key,
                                              sizeof key);
            bson_append_document_begin (dest, keyptr, len, &child);
            while (bson_iter_next (&citer)) {
               if (BSON_ITER_IS_KEY (&citer, "index")) {
                  idx = bson_iter_int32 (&citer) + offset;
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
                            const bson_t           *reply,   /* IN */
                            uint32_t                offset)
{
   int32_t server_index = 0;
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

      /* server returns each upserted _id with its index into this batch
       * look for "upserted": [{"index": 4, "_id": ObjectId()}, ...] */
      if (bson_iter_init_find (&iter, reply, "upserted")) {
         if (BSON_ITER_HOLDS_ARRAY (&iter) &&
             (bson_iter_recurse (&iter, &ar))) {

            while (bson_iter_next (&ar)) {
               if (BSON_ITER_HOLDS_DOCUMENT (&ar) &&
                   bson_iter_recurse (&ar, &citer) &&
                   bson_iter_find (&citer, "index") &&
                   BSON_ITER_HOLDS_INT32 (&citer)) {
                  server_index = bson_iter_int32 (&citer);

                  if (bson_iter_recurse (&ar, &citer) &&
                      bson_iter_find (&citer, "_id")) {
                     value = bson_iter_value (&citer);
                     _mongoc_write_result_append_upsert (result,
                                                         offset + server_index,
                                                         value);
                     n_upserted++;
                  }
               }
            }
         }
         result->nUpserted += n_upserted;
         /*
          * XXX: The following addition to nMatched needs some checking.
          *      I'm highly skeptical of it.
          */
         result->nMatched += BSON_MAX (0, (affected - n_upserted));
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
      _mongoc_write_result_merge_arrays (offset, result, &result->writeErrors,
                                         &iter);
   }

   if (bson_iter_init_find (&iter, reply, "writeConcernError") &&
       BSON_ITER_HOLDS_DOCUMENT (&iter)) {

      uint32_t len;
      const uint8_t *data;
      bson_t write_concern_error;
      char str[16];
      const char *key;

      /* writeConcernError is a subdocument in the server response
       * append it to the result->writeConcernErrors array */
      bson_iter_document (&iter, &len, &data);
      bson_init_static (&write_concern_error, data, len);

      bson_uint32_to_string (result->n_writeConcernErrors, &key,
                             str, sizeof str);

      bson_append_document (&result->writeConcernErrors,
                            key, -1, &write_concern_error);

      result->n_writeConcernErrors++;
   }

   EXIT;
}


/*
 * If error is not set, set code from first document in array like
 * [{"code": 64, "errmsg": "duplicate"}, ...]. Format the error message
 * from all errors in array.
*/
static void
_set_error_from_response (bson_t *bson_array,
                          mongoc_error_domain_t domain,
                          const char *error_type,
                          bson_error_t *error /* OUT */)
{
   bson_iter_t array_iter;
   bson_iter_t doc_iter;
   bson_string_t *compound_err;
   const char *errmsg = NULL;
   int32_t code = 0;
   uint32_t n_keys, i;

   compound_err = bson_string_new (NULL);
   n_keys = bson_count_keys (bson_array);
   if (n_keys > 1) {
      bson_string_append_printf (compound_err,
                                 _("Multiple %s errors: "),
                                 error_type);
   }

   if (!bson_empty0 (bson_array) && bson_iter_init (&array_iter, bson_array)) {

      /* get first code and all error messages */
      i = 0;

      while (bson_iter_next (&array_iter)) {
         if (BSON_ITER_HOLDS_DOCUMENT (&array_iter) &&
             bson_iter_recurse (&array_iter, &doc_iter)) {

            /* parse doc, which is like {"code": 64, "errmsg": "duplicate"} */
            while (bson_iter_next (&doc_iter)) {

               /* use the first error code we find */
               if (BSON_ITER_IS_KEY (&doc_iter, "code") && code == 0) {
                  code = bson_iter_int32 (&doc_iter);
               } else if (BSON_ITER_IS_KEY (&doc_iter, "errmsg")) {
                  errmsg = bson_iter_utf8 (&doc_iter, NULL);

                  /* build message like 'Multiple write errors: "foo", "bar"' */
                  if (n_keys > 1) {
                     bson_string_append_printf (compound_err, "\"%s\"", errmsg);
                     if (i < n_keys - 1) {
                        bson_string_append (compound_err, ", ");
                     }
                  } else {
                     /* single error message */
                     bson_string_append (compound_err, errmsg);
                  }
               }
            }

            i++;
         }
      }

      if (code && compound_err->len) {
         bson_set_error (error, domain, (uint32_t) code,
                         "%s", compound_err->str);
      }
   }

   bson_string_free (compound_err, true);
}


bool
_mongoc_write_result_complete (mongoc_write_result_t *result,
                               bson_t                *bson,
                               bson_error_t          *error)
{
   ENTRY;

   BSON_ASSERT (result);

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
      if (result->n_writeConcernErrors) {
         BSON_APPEND_ARRAY (bson, "writeConcernErrors",
                            &result->writeConcernErrors);
      }
   }

   /* set bson_error_t from first write error or write concern error */
   _set_error_from_response (&result->writeErrors,
                             MONGOC_ERROR_COMMAND,
                             "write",
                             &result->error);

   if (!result->error.code) {
      _set_error_from_response (&result->writeConcernErrors,
                                MONGOC_ERROR_WRITE_CONCERN,
                                "write concern",
                                &result->error);
   }

   if (error) {
      memcpy (error, &result->error, sizeof *error);
   }

   RETURN (!result->failed && result->error.code == 0);
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

   write_concern = (mongoc_write_concern_t *)bson_malloc0(sizeof *write_concern);
   write_concern->w = MONGOC_WRITE_CONCERN_W_DEFAULT;
   write_concern->fsync_ = MONGOC_WRITE_CONCERN_FSYNC_DEFAULT;
   write_concern->journal = MONGOC_WRITE_CONCERN_JOURNAL_DEFAULT;

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
   BSON_ASSERT (write_concern);
   return (write_concern->fsync_ == true);
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
                                bool                    fsync_)
{
   BSON_ASSERT (write_concern);

   if (!_mongoc_write_concern_warn_frozen(write_concern)) {
      write_concern->fsync_ = !!fsync_;
   }
}


bool
mongoc_write_concern_get_journal (const mongoc_write_concern_t *write_concern)
{
   BSON_ASSERT (write_concern);
   return (write_concern->journal == true);
}


bool
mongoc_write_concern_journal_is_set (
   const mongoc_write_concern_t *write_concern)
{
   BSON_ASSERT (write_concern);
   return (write_concern->journal != MONGOC_WRITE_CONCERN_JOURNAL_DEFAULT);
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
   BSON_ASSERT (write_concern);

   if (!_mongoc_write_concern_warn_frozen(write_concern)) {
      write_concern->journal = !!journal;
   }
}


int32_t
mongoc_write_concern_get_w (const mongoc_write_concern_t *write_concern)
{
   BSON_ASSERT (write_concern);
   return write_concern->w;
}


/**
 * mongoc_write_concern_set_w:
 * @w: The number of nodes for write or MONGOC_WRITE_CONCERN_W_MAJORITY
 * for "majority".
 *
 * Sets the number of nodes that must acknowledge the write request before
 * acknowledging the write request to the client.
 *
 * You may specifiy @w as MONGOC_WRITE_CONCERN_W_MAJORITY to request that
 * a "majority" of nodes acknowledge the request.
 */
void
mongoc_write_concern_set_w (mongoc_write_concern_t *write_concern,
                            int32_t            w)
{
   BSON_ASSERT (write_concern);
   BSON_ASSERT (w >= -3);

   if (!_mongoc_write_concern_warn_frozen(write_concern)) {
      write_concern->w = w;
   }
}


int32_t
mongoc_write_concern_get_wtimeout (const mongoc_write_concern_t *write_concern)
{
   BSON_ASSERT (write_concern);
   return write_concern->wtimeout;
}


/**
 * mongoc_write_concern_set_wtimeout:
 * @write_concern: A mongoc_write_concern_t.
 * @wtimeout_msec: Number of milliseconds before timeout.
 *
 * Sets the number of milliseconds to wait before considering a write
 * request as failed. A value of 0 indicates no write timeout.
 *
 * The @wtimeout_msec parameter must be positive or zero. Negative values will
 * be ignored.
 */
void
mongoc_write_concern_set_wtimeout (mongoc_write_concern_t *write_concern,
                                   int32_t                 wtimeout_msec)
{
   BSON_ASSERT (write_concern);

   if (wtimeout_msec < 0) {
      return;
   }

   if (!_mongoc_write_concern_warn_frozen(write_concern)) {
      write_concern->wtimeout = wtimeout_msec;
   }
}


bool
mongoc_write_concern_get_wmajority (const mongoc_write_concern_t *write_concern)
{
   BSON_ASSERT (write_concern);
   return (write_concern->w == MONGOC_WRITE_CONCERN_W_MAJORITY);
}


/**
 * mongoc_write_concern_set_wmajority:
 * @write_concern: A mongoc_write_concern_t.
 * @wtimeout_msec: Number of milliseconds before timeout.
 *
 * Sets the "w" of a write concern to "majority". It is suggested that
 * you provide a reasonable @wtimeout_msec to wait before considering the
 * write request failed. A @wtimeout_msec value of 0 indicates no write timeout.
 *
 * The @wtimeout_msec parameter must be positive or zero. Negative values will
 * be ignored.
 */
void
mongoc_write_concern_set_wmajority (mongoc_write_concern_t *write_concern,
                                    int32_t                 wtimeout_msec)
{
   BSON_ASSERT (write_concern);

   if (!_mongoc_write_concern_warn_frozen(write_concern)) {
      write_concern->w = MONGOC_WRITE_CONCERN_W_MAJORITY;

      if (wtimeout_msec >= 0) {
         write_concern->wtimeout = wtimeout_msec;
      }
   }
}


const char *
mongoc_write_concern_get_wtag (const mongoc_write_concern_t *write_concern)
{
   BSON_ASSERT (write_concern);

   if (write_concern->w == MONGOC_WRITE_CONCERN_W_TAG) {
      return write_concern->wtag;
   }

   return NULL;
}


void
mongoc_write_concern_set_wtag (mongoc_write_concern_t *write_concern,
                               const char             *wtag)
{
   BSON_ASSERT (write_concern);

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

   BSON_ASSERT (write_concern);

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
   } else {
      BSON_APPEND_INT32 (compiled, "w", write_concern->w);
   }

   if (write_concern->fsync_ != MONGOC_WRITE_CONCERN_FSYNC_DEFAULT) {
      bson_append_bool(compiled, "fsync", 5, !!write_concern->fsync_);
   }

   if (write_concern->journal != MONGOC_WRITE_CONCERN_JOURNAL_DEFAULT) {
      bson_append_bool(compiled, "j", 1, !!write_concern->journal);
   }

   if (write_concern->wtimeout) {
      bson_append_int32(compiled, "wtimeout", 8, write_concern->wtimeout);
   }

   BSON_APPEND_INT32 (compiled_gle, "getlasterror", 1);
   bson_concat (compiled_gle, compiled);
}


/**
 * mongoc_write_concern_is_acknowledged:
 * @concern: (in): A mongoc_write_concern_t.
 *
 * Checks to see if @write_concern requests that a getlasterror command is to
 * be delivered to the MongoDB server.
 *
 * Returns: true if a getlasterror command should be sent.
 */
bool
mongoc_write_concern_is_acknowledged (
   const mongoc_write_concern_t *write_concern)
{
   if (write_concern) {
      return (((write_concern->w != MONGOC_WRITE_CONCERN_W_UNACKNOWLEDGED) &&
               (write_concern->w != MONGOC_WRITE_CONCERN_W_ERRORS_IGNORED)) ||
              write_concern->fsync_ == true ||
              mongoc_write_concern_get_journal (write_concern));
   }
   return false;
}


/**
 * mongoc_write_concern_is_valid:
 * @write_concern: (in): A mongoc_write_concern_t.
 *
 * Checks to see if @write_concern is valid and does not contain conflicting
 * options.
 *
 * Returns: true if the write concern is valid; otherwise false.
 */
bool
mongoc_write_concern_is_valid (const mongoc_write_concern_t *write_concern)
{
   if (!write_concern) {
      return false;
   }

   /* Journal or fsync should require acknowledgement.  */
   if ((write_concern->fsync_ == true ||
        mongoc_write_concern_get_journal (write_concern)) &&
       (write_concern->w == MONGOC_WRITE_CONCERN_W_UNACKNOWLEDGED ||
        write_concern->w == MONGOC_WRITE_CONCERN_W_ERRORS_IGNORED)) {
      return false;
   }

   if (write_concern->wtimeout < 0) {
      return false;
   }

   return true;
}

/*==============================================================*/
/* --- mongoc-apm.c */

/*
 * An Application Performance Management (APM) implementation, complying with
 * MongoDB's Command Monitoring Spec:
 *
 * https://github.com/mongodb/specifications/tree/master/source/command-monitoring
 */

/*
 * Private initializer / cleanup functions.
 */

void
mongoc_apm_command_started_init (mongoc_apm_command_started_t *event,
                                 const bson_t                 *command,
                                 const char                   *database_name,
                                 const char                   *command_name,
                                 int64_t                       request_id,
                                 int64_t                       operation_id,
                                 const mongoc_host_list_t     *host,
                                 uint32_t                      server_id,
                                 void                         *context)
{
   bson_iter_t iter;
   uint32_t len;
   const uint8_t *data;

   /* Command Monitoring Spec:
    *
    * In cases where queries or commands are embedded in a $query parameter
    * when a read preference is provided, they MUST be unwrapped and the value
    * of the $query attribute becomes the filter or the command in the started
    * event. The read preference will subsequently be dropped as it is
    * considered metadata and metadata is not currently provided in the command
    * events.
    */
   if (bson_has_field (command, "$readPreference")) {
      if (bson_iter_init_find (&iter, command, "$query") &&
          BSON_ITER_HOLDS_DOCUMENT (&iter)) {
         bson_iter_document (&iter, &len, &data);
         event->command = bson_new_from_data (data, len);
      } else {
         /* $query should exist, but user could provide us a misformatted doc */
         event->command = bson_new ();
      }

      event->command_owned = true;
   } else {
      /* discard "const", we promise not to modify "command" */
      event->command = (bson_t *) command;
      event->command_owned = false;
   }

   event->database_name = database_name;
   event->command_name = command_name;
   event->request_id = request_id;
   event->operation_id = operation_id;
   event->host = host;
   event->server_id = server_id;
   event->context = context;
}


void
mongoc_apm_command_started_cleanup (mongoc_apm_command_started_t *event)
{
   if (event->command_owned) {
      bson_destroy (event->command);
   }
}


void
mongoc_apm_command_succeeded_init (mongoc_apm_command_succeeded_t *event,
                                   int64_t                         duration,
                                   const bson_t                   *reply,
                                   const char                     *command_name,
                                   int64_t                         request_id,
                                   int64_t                         operation_id,
                                   const mongoc_host_list_t       *host,
                                   uint32_t                        server_id,
                                   void                           *context)
{
   BSON_ASSERT (reply);

   event->duration = duration;
   event->reply = reply;
   event->command_name = command_name;
   event->request_id = request_id;
   event->operation_id = operation_id;
   event->host = host;
   event->server_id = server_id;
   event->context = context;
}


void
mongoc_apm_command_succeeded_cleanup (mongoc_apm_command_succeeded_t *event)
{
   /* no-op */
}


void
mongoc_apm_command_failed_init (mongoc_apm_command_failed_t *event,
                                int64_t                      duration,
                                const char                  *command_name,
                                bson_error_t                *error,
                                int64_t                      request_id,
                                int64_t                      operation_id,
                                const mongoc_host_list_t    *host,
                                uint32_t                     server_id,
                                void                        *context)
{
   event->duration = duration;
   event->command_name = command_name;
   event->error = error;
   event->request_id = request_id;
   event->operation_id = operation_id;
   event->host = host;
   event->server_id = server_id;
   event->context = context;
}


void
mongoc_apm_command_failed_cleanup (mongoc_apm_command_failed_t *event)
{
   /* no-op */
}


/*
 * event field accessors
 */

/* command-started event fields */

const bson_t *
mongoc_apm_command_started_get_command (
   const mongoc_apm_command_started_t *event)
{
   return event->command;
}


const char *
mongoc_apm_command_started_get_database_name (
   const mongoc_apm_command_started_t *event)
{
   return event->database_name;
}


const char *
mongoc_apm_command_started_get_command_name (
   const mongoc_apm_command_started_t *event)
{
   return event->command_name;
}


int64_t
mongoc_apm_command_started_get_request_id (
   const mongoc_apm_command_started_t *event)
{
   return event->request_id;
}


int64_t
mongoc_apm_command_started_get_operation_id (
   const mongoc_apm_command_started_t *event)
{
   return event->operation_id;
}


const mongoc_host_list_t *
mongoc_apm_command_started_get_host (const mongoc_apm_command_started_t *event)
{
   return event->host;
}


uint32_t
mongoc_apm_command_started_get_server_id (const mongoc_apm_command_started_t *event)
{
   return event->server_id;
}


void *
mongoc_apm_command_started_get_context (
   const mongoc_apm_command_started_t *event)
{
   return event->context;
}


/* command-succeeded event fields */

int64_t
mongoc_apm_command_succeeded_get_duration (
   const mongoc_apm_command_succeeded_t *event)
{
   return event->duration;
}


const bson_t *
mongoc_apm_command_succeeded_get_reply (
   const mongoc_apm_command_succeeded_t *event)
{
   return event->reply;
}


const char *
mongoc_apm_command_succeeded_get_command_name (
   const mongoc_apm_command_succeeded_t *event)
{
   return event->command_name;
}


int64_t
mongoc_apm_command_succeeded_get_request_id (
   const mongoc_apm_command_succeeded_t *event)
{
   return event->request_id;
}


int64_t
mongoc_apm_command_succeeded_get_operation_id (
   const mongoc_apm_command_succeeded_t *event)
{
   return event->operation_id;
}


const mongoc_host_list_t *
mongoc_apm_command_succeeded_get_host (
   const mongoc_apm_command_succeeded_t *event)
{
   return event->host;
}


uint32_t
mongoc_apm_command_succeeded_get_server_id (
   const mongoc_apm_command_succeeded_t *event)
{
   return event->server_id;
}


void *
mongoc_apm_command_succeeded_get_context (
   const mongoc_apm_command_succeeded_t *event)
{
   return event->context;
}


/* command-failed event fields */

int64_t
mongoc_apm_command_failed_get_duration (
   const mongoc_apm_command_failed_t *event)
{
   return event->duration;
}


const char *
mongoc_apm_command_failed_get_command_name (
   const mongoc_apm_command_failed_t *event)
{
   return event->command_name;
}


void
mongoc_apm_command_failed_get_error (
   const mongoc_apm_command_failed_t *event,
   bson_error_t                      *error)
{
   memcpy (error, &event->error, sizeof *event->error);
}


int64_t
mongoc_apm_command_failed_get_request_id (
   const mongoc_apm_command_failed_t *event)
{
   return event->request_id;
}


int64_t
mongoc_apm_command_failed_get_operation_id (
   const mongoc_apm_command_failed_t *event)
{
   return event->operation_id;
}


const mongoc_host_list_t *
mongoc_apm_command_failed_get_host (const mongoc_apm_command_failed_t *event)
{
   return event->host;
}


uint32_t
mongoc_apm_command_failed_get_server_id (const mongoc_apm_command_failed_t *event)
{
   return event->server_id;
}


void *
mongoc_apm_command_failed_get_context (const mongoc_apm_command_failed_t *event)
{
   return event->context;
}


/*
 * registering callbacks
 */

mongoc_apm_callbacks_t *
mongoc_apm_callbacks_new (void)
{
   size_t s = sizeof (mongoc_apm_callbacks_t);

   return (mongoc_apm_callbacks_t *) bson_malloc0 (s);
}


void
mongoc_apm_callbacks_destroy (mongoc_apm_callbacks_t *callbacks)
{
   bson_free (callbacks);
}


void
mongoc_apm_set_command_started_cb (
   mongoc_apm_callbacks_t          *callbacks,
   mongoc_apm_command_started_cb_t  cb)
{
   callbacks->started = cb;
}


void
mongoc_apm_set_command_succeeded_cb (
   mongoc_apm_callbacks_t            *callbacks,
   mongoc_apm_command_succeeded_cb_t  cb)
{
   callbacks->succeeded = cb;
}


void
mongoc_apm_set_command_failed_cb (
   mongoc_apm_callbacks_t         *callbacks,
   mongoc_apm_command_failed_cb_t  cb)
{
   callbacks->failed = cb;
}

/*==============================================================*/
/* --- mongoc-async.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "async"

mongoc_async_cmd_t *
mongoc_async_cmd (mongoc_async_t           *async,
                  mongoc_stream_t          *stream,
                  mongoc_async_cmd_setup_t  setup,
                  void                     *setup_ctx,
                  const char               *dbname,
                  const bson_t             *cmd,
                  mongoc_async_cmd_cb_t     cb,
                  void                     *cb_data,
                  int32_t                   timeout_msec)
{
   return mongoc_async_cmd_new (async, stream, setup, setup_ctx, dbname, cmd, cb,
                                cb_data, timeout_msec);
}

mongoc_async_t *
mongoc_async_new ()
{
   mongoc_async_t *async = (mongoc_async_t *)bson_malloc0 (sizeof (*async));

   return async;
}

void
mongoc_async_destroy (mongoc_async_t *async)
{
   mongoc_async_cmd_t *acmd, *tmp;

   DL_FOREACH_SAFE (async->cmds, acmd, tmp)
   {
      mongoc_async_cmd_destroy (acmd);
   }

   bson_free (async);
}

bool
mongoc_async_run (mongoc_async_t *async,
                  int32_t         timeout_msec)
{
   mongoc_async_cmd_t *acmd, *tmp;
   mongoc_stream_poll_t *poller = NULL;
   int i;
   ssize_t nactive = 0;
   int64_t now;
   int64_t expire_at = 0;

   size_t poll_size = 0;

   for (;;) {
      now = bson_get_monotonic_time ();

      if (expire_at == 0) {
         if (timeout_msec >= 0) {
            expire_at = now + ((int64_t) timeout_msec * 1000);
         } else {
            expire_at = -1;
         }
      } else if (timeout_msec >= 0) {
         timeout_msec = (expire_at - now) / 1000;
      }

      if (now > expire_at) {
         break;
      }

      DL_FOREACH_SAFE (async->cmds, acmd, tmp)
      {
         /* async commands are sorted by expire_at */
         if (now > acmd->expire_at) {
            acmd->cb (MONGOC_ASYNC_CMD_TIMEOUT, NULL, (now - acmd->start_time), acmd->data,
                      &acmd->error);
            mongoc_async_cmd_destroy (acmd);
         } else {
            break;
         }
      }

      if (!async->ncmds) {
         break;
      }

      if (poll_size < async->ncmds) {
         poller = (mongoc_stream_poll_t *)bson_realloc (poller, sizeof (*poller) * async->ncmds);
         poll_size = async->ncmds;
      }

      i = 0;
      DL_FOREACH (async->cmds, acmd)
      {
         poller[i].stream = acmd->stream;
         poller[i].events = acmd->events;
         poller[i].revents = 0;
         i++;
      }

      if (timeout_msec >= 0) {
         timeout_msec = BSON_MIN (timeout_msec, (async->cmds->expire_at - now) / 1000);
      } else {
         timeout_msec = (async->cmds->expire_at - now) / 1000;
      }

      nactive = mongoc_stream_poll (poller, async->ncmds, timeout_msec);

      if (nactive) {
         i = 0;

         DL_FOREACH_SAFE (async->cmds, acmd, tmp)
         {
            if (poller[i].revents & (POLLERR | POLLHUP)) {
               acmd->state = MONGOC_ASYNC_CMD_ERROR_STATE;
            }

            if (acmd->state == MONGOC_ASYNC_CMD_ERROR_STATE
                || (poller[i].revents & poller[i].events)) {

               mongoc_async_cmd_run (acmd);
               nactive--;

               if (!nactive) {
                  break;
               }
            }

            i++;
         }
      }
   }

   if (poll_size) {
      bson_free (poller);
   }

   return async->ncmds;
}

/*==============================================================*/
/* --- mongoc-async-cmd.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "async"

typedef mongoc_async_cmd_result_t (*_mongoc_async_cmd_phase_t)(
   mongoc_async_cmd_t *cmd);

mongoc_async_cmd_result_t
_mongoc_async_cmd_phase_setup (mongoc_async_cmd_t *cmd);
mongoc_async_cmd_result_t
_mongoc_async_cmd_phase_send (mongoc_async_cmd_t *cmd);
mongoc_async_cmd_result_t
_mongoc_async_cmd_phase_recv_len (mongoc_async_cmd_t *cmd);
mongoc_async_cmd_result_t
_mongoc_async_cmd_phase_recv_rpc (mongoc_async_cmd_t *cmd);

static const _mongoc_async_cmd_phase_t gMongocCMDPhases[] = {
   _mongoc_async_cmd_phase_setup,
   _mongoc_async_cmd_phase_send,
   _mongoc_async_cmd_phase_recv_len,
   _mongoc_async_cmd_phase_recv_rpc,
   NULL,  /* no callback for MONGOC_ASYNC_CMD_ERROR_STATE    */
   NULL,  /* no callback for MONGOC_ASYNC_CMD_CANCELED_STATE */
};

#ifdef MONGOC_ENABLE_SSL
int
mongoc_async_cmd_tls_setup (mongoc_stream_t *stream,
                            int             *events,
                            void            *ctx,
                            int32_t         timeout_msec,
                            bson_error_t    *error)
{
   mongoc_stream_t *tls_stream;
   const char *host = (const char *)ctx;

   for (tls_stream = stream; tls_stream->type != MONGOC_STREAM_TLS;
        tls_stream = mongoc_stream_get_base_stream (tls_stream)) {
   }

   if (mongoc_stream_tls_do_handshake (tls_stream, timeout_msec)) {
      if (mongoc_stream_tls_check_cert (tls_stream, host)) {
         return 1;
      } else {
         bson_set_error (error, MONGOC_ERROR_STREAM,
                         MONGOC_ERROR_STREAM_SOCKET,
                         _("Failed to verify TLS cert."));
         return -1;
      }
   } else if (mongoc_stream_tls_should_retry (tls_stream)) {
      *events = mongoc_stream_tls_should_read (tls_stream) ? POLLIN : POLLOUT;
   } else {
      bson_set_error (error, MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_SOCKET,
                      _("Failed to initialize TLS state."));
      return -1;
   }

   return 0;
}
#endif

bool
mongoc_async_cmd_run (mongoc_async_cmd_t *acmd)
{
   mongoc_async_cmd_result_t result;
   int64_t rtt;
   _mongoc_async_cmd_phase_t phase_callback;

   phase_callback = gMongocCMDPhases[acmd->state];
   if (phase_callback) {
      result = phase_callback (acmd);
   } else {
      result = MONGOC_ASYNC_CMD_ERROR;
   }

   if (result == MONGOC_ASYNC_CMD_IN_PROGRESS) {
      return true;
   }

   rtt = bson_get_monotonic_time () - acmd->start_time;

   if (result == MONGOC_ASYNC_CMD_SUCCESS) {
      acmd->cb (result, &acmd->reply, rtt, acmd->data, &acmd->error);
   } else {
      /* we're in ERROR, TIMEOUT, or CANCELED */
      acmd->cb (result, NULL, rtt, acmd->data, &acmd->error);
   }

   mongoc_async_cmd_destroy (acmd);
   return false;
}

void
_mongoc_async_cmd_init_send (mongoc_async_cmd_t *acmd,
                             const char         *dbname)
{
   bson_snprintf (acmd->ns, sizeof acmd->ns, "%s.$cmd", dbname);

   acmd->rpc.query.msg_len = 0;
   acmd->rpc.query.request_id = ++acmd->async->request_id;
   acmd->rpc.query.response_to = 0;
   acmd->rpc.query.opcode = MONGOC_OPCODE_QUERY;
   acmd->rpc.query.flags = MONGOC_QUERY_SLAVE_OK;
   acmd->rpc.query.collection = acmd->ns;
   acmd->rpc.query.skip = 0;
   acmd->rpc.query.n_return = -1;
   acmd->rpc.query.query = bson_get_data (&acmd->cmd);
   acmd->rpc.query.fields = NULL;

   _mongoc_rpc_gather (&acmd->rpc, &acmd->array);
   acmd->iovec = (mongoc_iovec_t *)acmd->array.data;
   acmd->niovec = acmd->array.len;
   _mongoc_rpc_swab_to_le (&acmd->rpc);
}

void
_mongoc_async_cmd_state_start (mongoc_async_cmd_t *acmd)
{
   if (acmd->setup) {
      acmd->state = MONGOC_ASYNC_CMD_SETUP;
   } else {
      acmd->state = MONGOC_ASYNC_CMD_SEND;
   }

   acmd->events = POLLOUT;
}

mongoc_async_cmd_t *
mongoc_async_cmd_new (mongoc_async_t           *async,
                      mongoc_stream_t          *stream,
                      mongoc_async_cmd_setup_t  setup,
                      void                     *setup_ctx,
                      const char               *dbname,
                      const bson_t             *cmd,
                      mongoc_async_cmd_cb_t     cb,
                      void                     *cb_data,
                      int32_t                   timeout_msec)
{
   mongoc_async_cmd_t *acmd;
   mongoc_async_cmd_t *tmp;
   bool found = false;

   BSON_ASSERT (cmd);
   BSON_ASSERT (dbname);
   BSON_ASSERT (stream);

   acmd = (mongoc_async_cmd_t *)bson_malloc0 (sizeof (*acmd));
   acmd->async = async;
   acmd->expire_at = bson_get_monotonic_time () + (int64_t) timeout_msec * 1000;
   acmd->stream = stream;
   acmd->setup = setup;
   acmd->setup_ctx = setup_ctx;
   acmd->cb = cb;
   acmd->data = cb_data;
   bson_copy_to (cmd, &acmd->cmd);

   _mongoc_array_init (&acmd->array, sizeof (mongoc_iovec_t));
   _mongoc_buffer_init (&acmd->buffer, NULL, 0, NULL, NULL);

   _mongoc_async_cmd_init_send (acmd, dbname);

   _mongoc_async_cmd_state_start (acmd);

   /* slot the cmd into the right place in the expiration list */
   {
      async->ncmds++;
      DL_FOREACH (async->cmds, tmp)
      {
         if (tmp->expire_at >= acmd->expire_at) {
            DL_PREPEND_ELEM (async->cmds, tmp, acmd);
            found = true;
            break;
         }
      }

      if (! found) {
         DL_APPEND (async->cmds, acmd);
      }
   }

   return acmd;
}


void
mongoc_async_cmd_destroy (mongoc_async_cmd_t *acmd)
{
   BSON_ASSERT (acmd);

   DL_DELETE (acmd->async->cmds, acmd);
   acmd->async->ncmds--;

   bson_destroy (&acmd->cmd);

   if (acmd->reply_needs_cleanup) {
      bson_destroy (&acmd->reply);
   }

   _mongoc_array_destroy (&acmd->array);
   _mongoc_buffer_destroy (&acmd->buffer);

   bson_free (acmd);
}

mongoc_async_cmd_result_t
_mongoc_async_cmd_phase_setup (mongoc_async_cmd_t *acmd)
{
   int64_t now;
   int64_t timeout_msec;

   now = bson_get_monotonic_time ();
   timeout_msec = (acmd->expire_at - now) / 1000;

   BSON_ASSERT (timeout_msec < INT32_MAX);

   switch (acmd->setup (acmd->stream, &acmd->events, acmd->setup_ctx,
                        (int32_t) timeout_msec, &acmd->error)) {
      case -1:
         return MONGOC_ASYNC_CMD_ERROR;
         break;
      case 0:
         break;
      case 1:
         acmd->state = MONGOC_ASYNC_CMD_SEND;
         acmd->events = POLLOUT;
         break;
      default:
         abort();
   }

   return MONGOC_ASYNC_CMD_IN_PROGRESS;
}

mongoc_async_cmd_result_t
_mongoc_async_cmd_phase_send (mongoc_async_cmd_t *acmd)
{
   ssize_t bytes;

   bytes = mongoc_stream_writev (acmd->stream, acmd->iovec, acmd->niovec, 0);

   if (bytes < 0) {
      bson_set_error (&acmd->error, MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_SOCKET,
                      _("Failed to write rpc bytes."));
      return MONGOC_ASYNC_CMD_ERROR;
   }

   while (bytes) {
      if (acmd->iovec->iov_len < (size_t)bytes) {
         bytes -= acmd->iovec->iov_len;
         acmd->iovec++;
         acmd->niovec--;
      } else {
         acmd->iovec->iov_base = ((char *)acmd->iovec->iov_base) + bytes;
         acmd->iovec->iov_len -= bytes;
         bytes = 0;
      }
   }

   acmd->state = MONGOC_ASYNC_CMD_RECV_LEN;
   acmd->bytes_to_read = 4;
   acmd->events = POLLIN;

   acmd->start_time = bson_get_monotonic_time ();

   return MONGOC_ASYNC_CMD_IN_PROGRESS;
}

mongoc_async_cmd_result_t
_mongoc_async_cmd_phase_recv_len (mongoc_async_cmd_t *acmd)
{
   ssize_t bytes = _mongoc_buffer_try_append_from_stream (&acmd->buffer,
                                                          acmd->stream,
                                                          acmd->bytes_to_read,
                                                          0, &acmd->error);
   uint32_t msg_len;

   if (bytes < 0) {
      bson_set_error (&acmd->error, MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_SOCKET,
                      _("Failed to receive length header from server."));
      return MONGOC_ASYNC_CMD_ERROR;
   }

   if (bytes == 0) {
      bson_set_error (&acmd->error, MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_SOCKET,
                      _("Server closed connection."));
      return MONGOC_ASYNC_CMD_ERROR;
   }

   acmd->bytes_to_read -= bytes;

   if (!acmd->bytes_to_read) {
      memcpy (&msg_len, acmd->buffer.data, 4);
      msg_len = BSON_UINT32_FROM_LE (msg_len);

      if ((msg_len < 16) || (msg_len > MONGOC_DEFAULT_MAX_MSG_SIZE)) {
         bson_set_error (&acmd->error, MONGOC_ERROR_PROTOCOL,
                         MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                         _("Invalid reply from server."));
         return MONGOC_ASYNC_CMD_ERROR;
      }

      acmd->bytes_to_read = msg_len - 4;
      acmd->state = MONGOC_ASYNC_CMD_RECV_RPC;

      return _mongoc_async_cmd_phase_recv_rpc (acmd);
   }

   return MONGOC_ASYNC_CMD_IN_PROGRESS;
}

mongoc_async_cmd_result_t
_mongoc_async_cmd_phase_recv_rpc (mongoc_async_cmd_t *acmd)
{
   ssize_t bytes = _mongoc_buffer_try_append_from_stream (&acmd->buffer,
                                                          acmd->stream,
                                                          acmd->bytes_to_read,
                                                          0, &acmd->error);

   if (bytes < 0) {
      bson_set_error (&acmd->error, MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_SOCKET,
                      _("Failed to receive rpc bytes from server."));
      return MONGOC_ASYNC_CMD_ERROR;
   }

   if (bytes == 0) {
      bson_set_error (&acmd->error, MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_SOCKET,
                      _("Server closed connection."));
      return MONGOC_ASYNC_CMD_ERROR;
   }

   acmd->bytes_to_read -= bytes;

   if (!acmd->bytes_to_read) {
      if (!_mongoc_rpc_scatter (&acmd->rpc, acmd->buffer.data,
                                acmd->buffer.len)) {
         bson_set_error (&acmd->error,
                         MONGOC_ERROR_PROTOCOL,
                         MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                         _("Invalid reply from server."));
         return MONGOC_ASYNC_CMD_ERROR;
      }

      _mongoc_rpc_swab_from_le (&acmd->rpc);

      if (acmd->rpc.header.opcode != MONGOC_OPCODE_REPLY) {
         bson_set_error (&acmd->error,
                         MONGOC_ERROR_PROTOCOL,
                         MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                         _("Invalid reply from server."));
         return MONGOC_ASYNC_CMD_ERROR;
      }

      if (!_mongoc_rpc_reply_get_first (&acmd->rpc.reply, &acmd->reply)) {
         bson_set_error (&acmd->error,
                         MONGOC_ERROR_BSON,
                         MONGOC_ERROR_BSON_INVALID,
                         _("Failed to decode reply BSON document."));
         return MONGOC_ASYNC_CMD_ERROR;
      }

      acmd->reply_needs_cleanup = true;

      return MONGOC_ASYNC_CMD_SUCCESS;
   }

   return MONGOC_ASYNC_CMD_IN_PROGRESS;
}

/*==============================================================*/
/* --- mongoc-b64.c */

/*
 * Copyright (c) 1996, 1998 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Portions Copyright (c) 1995 by International Business Machines, Inc.
 *
 * International Business Machines, Inc. (hereinafter called IBM) grants
 * permission under its copyrights to use, copy, modify, and distribute this
 * Software with or without fee, provided that the above copyright notice and
 * all paragraphs of this notice appear in all copies, and that the name of IBM
 * not be used in connection with the marketing of any product incorporating
 * the Software or modifications thereof, without specific, written prior
 * permission.
 *
 * To the extent it has a right to do so, IBM grants an immunity from suit
 * under its patents, if any, for the use, sale or manufacture of products to
 * the extent that such products are used for performing Domain Name System
 * dynamic updates in TCP/IP networks by means of the Software.  No immunity is
 * granted for any product per se or for any other function of any product.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", AND IBM DISCLAIMS ALL WARRANTIES,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE.  IN NO EVENT SHALL IBM BE LIABLE FOR ANY SPECIAL,
 * DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE, EVEN
 * IF IBM IS APPRISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#define Assert(Cond) if (!(Cond)) abort ()

static const char Base64[] =
   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char Pad64 = '=';

/* (From RFC1521 and draft-ietf-dnssec-secext-03.txt)
 * The following encoding technique is taken from RFC 1521 by Borenstein
 * and Freed.  It is reproduced here in a slightly edited form for
 * convenience.
 *
 * A 65-character subset of US-ASCII is used, enabling 6 bits to be
 * represented per printable character. (The extra 65th character, "=",
 * is used to signify a special processing function.)
 *
 * The encoding process represents 24-bit groups of input bits as output
 * strings of 4 encoded characters. Proceeding from left to right, a
 * 24-bit input group is formed by concatenating 3 8-bit input groups.
 * These 24 bits are then treated as 4 concatenated 6-bit groups, each
 * of which is translated into a single digit in the base64 alphabet.
 *
 * Each 6-bit group is used as an index into an array of 64 printable
 * characters. The character referenced by the index is placed in the
 * output string.
 *
 *                       Table 1: The Base64 Alphabet
 *
 *    Value Encoding  Value Encoding  Value Encoding  Value Encoding
 *        0 A            17 R            34 i            51 z
 *        1 B            18 S            35 j            52 0
 *        2 C            19 T            36 k            53 1
 *        3 D            20 U            37 l            54 2
 *        4 E            21 V            38 m            55 3
 *        5 F            22 W            39 n            56 4
 *        6 G            23 X            40 o            57 5
 *        7 H            24 Y            41 p            58 6
 *        8 I            25 Z            42 q            59 7
 *        9 J            26 a            43 r            60 8
 *       10 K            27 b            44 s            61 9
 *       11 L            28 c            45 t            62 +
 *       12 M            29 d            46 u            63 /
 *       13 N            30 e            47 v
 *       14 O            31 f            48 w         (pad) =
 *       15 P            32 g            49 x
 *       16 Q            33 h            50 y
 *
 * Special processing is performed if fewer than 24 bits are available
 * at the end of the data being encoded.  A full encoding quantum is
 * always completed at the end of a quantity.  When fewer than 24 input
 * bits are available in an input group, zero bits are added (on the
 * right) to form an integral number of 6-bit groups.  Padding at the
 * end of the data is performed using the '=' character.
 *
 * Since all base64 input is an integral number of octets, only the
 * following cases can arise:
 *
 *     (1) the final quantum of encoding input is an integral
 *         multiple of 24 bits; here, the final unit of encoded
 *    output will be an integral multiple of 4 characters
 *    with no "=" padding,
 *     (2) the final quantum of encoding input is exactly 8 bits;
 *         here, the final unit of encoded output will be two
 *    characters followed by two "=" padding characters, or
 *     (3) the final quantum of encoding input is exactly 16 bits;
 *         here, the final unit of encoded output will be three
 *    characters followed by one "=" padding character.
 */

int
mongoc_b64_ntop (uint8_t const *src,
                 size_t         srclength,
                 char          *target,
                 size_t         targsize)
{
   size_t datalength = 0;
   uint8_t input[3];
   uint8_t output[4];
   size_t i;

   while (2 < srclength) {
      input[0] = *src++;
      input[1] = *src++;
      input[2] = *src++;
      srclength -= 3;

      output[0] = input[0] >> 2;
      output[1] = ((input[0] & 0x03) << 4) + (input[1] >> 4);
      output[2] = ((input[1] & 0x0f) << 2) + (input[2] >> 6);
      output[3] = input[2] & 0x3f;
      Assert (output[0] < 64);
      Assert (output[1] < 64);
      Assert (output[2] < 64);
      Assert (output[3] < 64);

      if (datalength + 4 > targsize) {
         return -1;
      }
      target[datalength++] = Base64[output[0]];
      target[datalength++] = Base64[output[1]];
      target[datalength++] = Base64[output[2]];
      target[datalength++] = Base64[output[3]];
   }

   /* Now we worry about padding. */
   if (0 != srclength) {
      /* Get what's left. */
      input[0] = input[1] = input[2] = '\0';

      for (i = 0; i < srclength; i++) {
         input[i] = *src++;
      }
      output[0] = input[0] >> 2;
      output[1] = ((input[0] & 0x03) << 4) + (input[1] >> 4);
      output[2] = ((input[1] & 0x0f) << 2) + (input[2] >> 6);
      Assert (output[0] < 64);
      Assert (output[1] < 64);
      Assert (output[2] < 64);

      if (datalength + 4 > targsize) {
         return -1;
      }
      target[datalength++] = Base64[output[0]];
      target[datalength++] = Base64[output[1]];

      if (srclength == 1) {
         target[datalength++] = Pad64;
      } else{
         target[datalength++] = Base64[output[2]];
      }
      target[datalength++] = Pad64;
   }

   if (datalength >= targsize) {
      return -1;
   }
   target[datalength] = '\0'; /* Returned value doesn't count \0. */
   return (int)datalength;
}

/* (From RFC1521 and draft-ietf-dnssec-secext-03.txt)
   The following encoding technique is taken from RFC 1521 by Borenstein
   and Freed.  It is reproduced here in a slightly edited form for
   convenience.

   A 65-character subset of US-ASCII is used, enabling 6 bits to be
   represented per printable character. (The extra 65th character, "=",
   is used to signify a special processing function.)

   The encoding process represents 24-bit groups of input bits as output
   strings of 4 encoded characters. Proceeding from left to right, a
   24-bit input group is formed by concatenating 3 8-bit input groups.
   These 24 bits are then treated as 4 concatenated 6-bit groups, each
   of which is translated into a single digit in the base64 alphabet.

   Each 6-bit group is used as an index into an array of 64 printable
   characters. The character referenced by the index is placed in the
   output string.

                         Table 1: The Base64 Alphabet

      Value Encoding  Value Encoding  Value Encoding  Value Encoding
          0 A            17 R            34 i            51 z
          1 B            18 S            35 j            52 0
          2 C            19 T            36 k            53 1
          3 D            20 U            37 l            54 2
          4 E            21 V            38 m            55 3
          5 F            22 W            39 n            56 4
          6 G            23 X            40 o            57 5
          7 H            24 Y            41 p            58 6
          8 I            25 Z            42 q            59 7
          9 J            26 a            43 r            60 8
         10 K            27 b            44 s            61 9
         11 L            28 c            45 t            62 +
         12 M            29 d            46 u            63 /
         13 N            30 e            47 v
         14 O            31 f            48 w         (pad) =
         15 P            32 g            49 x
         16 Q            33 h            50 y

   Special processing is performed if fewer than 24 bits are available
   at the end of the data being encoded.  A full encoding quantum is
   always completed at the end of a quantity.  When fewer than 24 input
   bits are available in an input group, zero bits are added (on the
   right) to form an integral number of 6-bit groups.  Padding at the
   end of the data is performed using the '=' character.

   Since all base64 input is an integral number of octets, only the
   following cases can arise:

       (1) the final quantum of encoding input is an integral
           multiple of 24 bits; here, the final unit of encoded
	   output will be an integral multiple of 4 characters
	   with no "=" padding,
       (2) the final quantum of encoding input is exactly 8 bits;
           here, the final unit of encoded output will be two
	   characters followed by two "=" padding characters, or
       (3) the final quantum of encoding input is exactly 16 bits;
           here, the final unit of encoded output will be three
	   characters followed by one "=" padding character.
   */

/* skips all whitespace anywhere.
   converts characters, four at a time, starting at (or after)
   src from base - 64 numbers into three 8 bit bytes in the target area.
   it returns the number of data bytes stored at the target, or -1 on error.
 */

static int mongoc_b64rmap_initialized = 0;
static uint8_t mongoc_b64rmap[256];

static const uint8_t mongoc_b64rmap_special = 0xf0;
static const uint8_t mongoc_b64rmap_end = 0xfd;
static const uint8_t mongoc_b64rmap_space = 0xfe;
static const uint8_t mongoc_b64rmap_invalid = 0xff;

/**
 * Initializing the reverse map is not thread safe.
 * Which is fine for NSD. For now...
 **/
void
mongoc_b64_initialize_rmap (void)
{
	int i;
	unsigned char ch;

	/* Null: end of string, stop parsing */
	mongoc_b64rmap[0] = mongoc_b64rmap_end;

	for (i = 1; i < 256; ++i) {
		ch = (unsigned char)i;
		/* Whitespaces */
		if (isspace(ch))
			mongoc_b64rmap[i] = mongoc_b64rmap_space;
		/* Padding: stop parsing */
		else if (ch == Pad64)
			mongoc_b64rmap[i] = mongoc_b64rmap_end;
		/* Non-base64 char */
		else
			mongoc_b64rmap[i] = mongoc_b64rmap_invalid;
	}

	/* Fill reverse mapping for base64 chars */
	for (i = 0; Base64[i] != '\0'; ++i)
		mongoc_b64rmap[(uint8_t)Base64[i]] = i;

	mongoc_b64rmap_initialized = 1;
}

static int
mongoc_b64_pton_do(char const *src, uint8_t *target, size_t targsize)
{
	int tarindex, state, ch;
	uint8_t ofs;

	state = 0;
	tarindex = 0;

	while (1)
	{
		ch = *src++;
		ofs = mongoc_b64rmap[ch];

		if (ofs >= mongoc_b64rmap_special) {
			/* Ignore whitespaces */
			if (ofs == mongoc_b64rmap_space)
				continue;
			/* End of base64 characters */
			if (ofs == mongoc_b64rmap_end)
				break;
			/* A non-base64 character. */
			return (-1);
		}

		switch (state) {
		case 0:
			if ((size_t)tarindex >= targsize)
				return (-1);
			target[tarindex] = ofs << 2;
			state = 1;
			break;
		case 1:
			if ((size_t)tarindex + 1 >= targsize)
				return (-1);
			target[tarindex]   |=  ofs >> 4;
			target[tarindex+1]  = (ofs & 0x0f)
						<< 4 ;
			tarindex++;
			state = 2;
			break;
		case 2:
			if ((size_t)tarindex + 1 >= targsize)
				return (-1);
			target[tarindex]   |=  ofs >> 2;
			target[tarindex+1]  = (ofs & 0x03)
						<< 6;
			tarindex++;
			state = 3;
			break;
		case 3:
			if ((size_t)tarindex >= targsize)
				return (-1);
			target[tarindex] |= ofs;
			tarindex++;
			state = 0;
			break;
		default:
			abort();
		}
	}

	/*
	 * We are done decoding Base-64 chars.  Let's see if we ended
	 * on a byte boundary, and/or with erroneous trailing characters.
	 */

	if (ch == Pad64) {		/* We got a pad char. */
		ch = *src++;		/* Skip it, get next. */
		switch (state) {
		case 0:		/* Invalid = in first position */
		case 1:		/* Invalid = in second position */
			return (-1);

		case 2:		/* Valid, means one byte of info */
			/* Skip any number of spaces. */
			for ((void)NULL; ch != '\0'; ch = *src++)
				if (mongoc_b64rmap[ch] != mongoc_b64rmap_space)
					break;
			/* Make sure there is another trailing = sign. */
			if (ch != Pad64)
				return (-1);
			ch = *src++;		/* Skip the = */
			/* Fall through to "single trailing =" case. */
			/* FALLTHROUGH */

		case 3:		/* Valid, means two bytes of info */
			/*
			 * We know this char is an =.  Is there anything but
			 * whitespace after it?
			 */
			for ((void)NULL; ch != '\0'; ch = *src++)
				if (mongoc_b64rmap[ch] != mongoc_b64rmap_space)
					return (-1);

			/*
			 * Now make sure for cases 2 and 3 that the "extra"
			 * bits that slopped past the last full byte were
			 * zeros.  If we don't check them, they become a
			 * subliminal channel.
			 */
			if (target[tarindex] != 0)
				return (-1);
		default:
			break;
		}
	} else {
		/*
		 * We ended by seeing the end of the string.  Make sure we
		 * have no partial bytes lying around.
		 */
		if (state != 0)
			return (-1);
	}

	return (tarindex);
}


static BSON_GNUC_PURE int
mongoc_b64_pton_len(char const *src)
{
	int tarindex, state, ch;
	uint8_t ofs;

	state = 0;
	tarindex = 0;

	while (1)
	{
		ch = *src++;
		ofs = mongoc_b64rmap[ch];

		if (ofs >= mongoc_b64rmap_special) {
			/* Ignore whitespaces */
			if (ofs == mongoc_b64rmap_space)
				continue;
			/* End of base64 characters */
			if (ofs == mongoc_b64rmap_end)
				break;
			/* A non-base64 character. */
			return (-1);
		}

		switch (state) {
		case 0:
			state = 1;
			break;
		case 1:
			tarindex++;
			state = 2;
			break;
		case 2:
			tarindex++;
			state = 3;
			break;
		case 3:
			tarindex++;
			state = 0;
			break;
		default:
			abort();
		}
	}

	/*
	 * We are done decoding Base-64 chars.  Let's see if we ended
	 * on a byte boundary, and/or with erroneous trailing characters.
	 */

	if (ch == Pad64) {		/* We got a pad char. */
		ch = *src++;		/* Skip it, get next. */
		switch (state) {
		case 0:		/* Invalid = in first position */
		case 1:		/* Invalid = in second position */
			return (-1);

		case 2:		/* Valid, means one byte of info */
			/* Skip any number of spaces. */
			for ((void)NULL; ch != '\0'; ch = *src++)
				if (mongoc_b64rmap[ch] != mongoc_b64rmap_space)
					break;
			/* Make sure there is another trailing = sign. */
			if (ch != Pad64)
				return (-1);
			ch = *src++;		/* Skip the = */
			/* Fall through to "single trailing =" case. */
			/* FALLTHROUGH */

		case 3:		/* Valid, means two bytes of info */
			/*
			 * We know this char is an =.  Is there anything but
			 * whitespace after it?
			 */
			for ((void)NULL; ch != '\0'; ch = *src++)
				if (mongoc_b64rmap[ch] != mongoc_b64rmap_space)
					return (-1);

		default:
			break;
		}
	} else {
		/*
		 * We ended by seeing the end of the string.  Make sure we
		 * have no partial bytes lying around.
		 */
		if (state != 0)
			return (-1);
	}

	return (tarindex);
}


int
mongoc_b64_pton(char const *src, uint8_t *target, size_t targsize)
{
	if (!mongoc_b64rmap_initialized)
		mongoc_b64_initialize_rmap ();

	if (target)
		return mongoc_b64_pton_do (src, target, targsize);
	else
		return mongoc_b64_pton_len (src);
}


/*==============================================================*/
/* --- mongoc-crypto.c */

#ifdef MONGOC_ENABLE_CRYPTO

void
mongoc_crypto_init (mongoc_crypto_t *crypto)
{
   switch(MONGOC_CRYPTO_TYPE)
   {
#ifdef MONGOC_ENABLE_COMMON_CRYPTO
      case MONGOC_CRYPTO_COMMON_CRYPTO:
         crypto->hmac_sha1 = mongoc_crypto_common_crypto_hmac_sha1;
         crypto->sha1 = mongoc_crypto_common_crypto_sha1;
         break;
#endif
#ifdef MONGOC_ENABLE_LIBCRYPTO
      case MONGOC_CRYPTO_OPENSSL:
         crypto->hmac_sha1 = mongoc_crypto_openssl_hmac_sha1;
         crypto->sha1 = mongoc_crypto_openssl_sha1;
         break;
#endif
      default:
         MONGOC_ERROR("Unknown crypto engine");
   }
}

void
mongoc_crypto_hmac_sha1 (mongoc_crypto_t     *crypto,
                         const void          *key,
                         int                  key_len,
                         const unsigned char *d,
                         int                  n,
                         unsigned char       *md /* OUT */)
{
   crypto->hmac_sha1 (crypto, key, key_len, d, n, md);
}

bool
mongoc_crypto_sha1      (mongoc_crypto_t     *crypto,
                         const unsigned char *input,
                         const size_t         input_len,
                         unsigned char       *output /* OUT */)
{
   return crypto->sha1 (crypto, input, input_len, output);
}
#endif


/*==============================================================*/
/* --- mongoc-crypto-common-crypto.c */

#ifdef MONGOC_ENABLE_COMMON_CRYPTO
#include <CommonCrypto/CommonHMAC.h>
#include <CommonCrypto/CommonDigest.h>



void
mongoc_crypto_common_crypto_hmac_sha1 (mongoc_crypto_t     *crypto,
                                       const void          *key,
                                       int                  key_len,
                                       const unsigned char *d,
                                       int                  n,
                                       unsigned char       *md /* OUT */)
{
   /* U1 = HMAC(input, salt + 0001) */
   CCHmac (kCCHmacAlgSHA1,
           key,
           key_len,
           d,
           n,
           md);
}

bool
mongoc_crypto_common_crypto_sha1 (mongoc_crypto_t     *crypto,
                                  const unsigned char *input,
                                  const size_t         input_len,
                                  unsigned char       *output /* OUT */)
{
   return CC_SHA1 (input, input_len, output);
}


#endif

/*==============================================================*/
/* --- mongoc-crypto-openssl.c */

#ifdef MONGOC_ENABLE_LIBCRYPTO

void
mongoc_crypto_openssl_hmac_sha1 (mongoc_crypto_t     *crypto,
                                 const void          *key,
                                 int                  key_len,
                                 const unsigned char *d,
                                 int                  n,
                                 unsigned char       *md /* OUT */)
{
   /* U1 = HMAC(input, salt + 0001) */
   HMAC (EVP_sha1 (),
         key,
         key_len,
         d,
         n,
         md,
         NULL);
}

bool
mongoc_crypto_openssl_sha1 (mongoc_crypto_t     *crypto,
                            const unsigned char *input,
                            const size_t         input_len,
                            unsigned char       *output /* OUT */)
{
   EVP_MD_CTX digest_ctx;
   bool rval = false;

   EVP_MD_CTX_init (&digest_ctx);

   if (1 != EVP_DigestInit_ex (&digest_ctx, EVP_sha1 (), NULL)) {
      goto cleanup;
   }

   if (1 != EVP_DigestUpdate (&digest_ctx, input, input_len)) {
      goto cleanup;
   }

   rval = (1 == EVP_DigestFinal_ex (&digest_ctx, output, NULL));

cleanup:
   EVP_MD_CTX_cleanup (&digest_ctx);

   return rval;
}


#endif

/*==============================================================*/
/* --- mongoc-cursor-transform.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "cursor-transform"


typedef struct
{
   mongoc_cursor_transform_filter_t filter;
   mongoc_cursor_transform_mutate_t mutate;
   mongoc_cursor_transform_dtor_t   dtor;
   void                            *ctx;
   bson_t                           tmp;
} mongoc_cursor_transform_t;


static void *
_mongoc_cursor_transform_new (mongoc_cursor_transform_filter_t filter,
                              mongoc_cursor_transform_mutate_t mutate,
                              mongoc_cursor_transform_dtor_t   dtor,
                              void                            *ctx)
{
   mongoc_cursor_transform_t *transform;

   ENTRY;

   transform = (mongoc_cursor_transform_t *)bson_malloc0 (sizeof *transform);

   transform->filter = filter;
   transform->mutate = mutate;
   transform->dtor = dtor;
   transform->ctx = ctx;
   bson_init (&transform->tmp);

   RETURN (transform);
}


static void
_mongoc_cursor_transform_destroy (mongoc_cursor_t *cursor)
{
   mongoc_cursor_transform_t *transform;

   ENTRY;

   transform = (mongoc_cursor_transform_t *)cursor->iface_data;

   if (transform->dtor) {
      transform->dtor (transform->ctx);
   }

   bson_destroy (&transform->tmp);

   bson_free (cursor->iface_data);
   _mongoc_cursor_destroy (cursor);

   EXIT;
}


static bool
_mongoc_cursor_transform_next (mongoc_cursor_t *cursor,
                               const bson_t   **bson)
{
   mongoc_cursor_transform_t *transform;

   ENTRY;

   transform = (mongoc_cursor_transform_t *)cursor->iface_data;

   for (;; ) {
      if (!_mongoc_cursor_next (cursor, bson)) {
         RETURN (false);
      }

      switch (transform->filter (*bson, transform->ctx)) {
      case MONGO_CURSOR_TRANSFORM_DROP:
         break;
      case MONGO_CURSOR_TRANSFORM_PASS:
         RETURN (true);
         break;
      case MONGO_CURSOR_TRANSFORM_MUTATE:
         bson_reinit (&transform->tmp);

         transform->mutate (*bson, &transform->tmp, transform->ctx);
         *bson = &transform->tmp;
         RETURN (true);

         break;
      default:
         abort ();
         break;
      }
   }
}


static mongoc_cursor_t *
_mongoc_cursor_transform_clone (const mongoc_cursor_t *cursor)
{
   mongoc_cursor_transform_t *transform;
   mongoc_cursor_t *clone_;

   ENTRY;

   transform = (mongoc_cursor_transform_t *)cursor->iface_data;

   clone_ = _mongoc_cursor_clone (cursor);
   _mongoc_cursor_transform_init (clone_, transform->filter, transform->mutate,
                                  transform->dtor, transform->ctx);

   RETURN (clone_);
}


static mongoc_cursor_interface_t gMongocCursorTransform = {
   _mongoc_cursor_transform_clone,
   _mongoc_cursor_transform_destroy,
   NULL,
   _mongoc_cursor_transform_next,
};


void
_mongoc_cursor_transform_init (mongoc_cursor_t                 *cursor,
                               mongoc_cursor_transform_filter_t filter,
                               mongoc_cursor_transform_mutate_t mutate,
                               mongoc_cursor_transform_dtor_t   dtor,
                               void                            *ctx)
{
   ENTRY;

   cursor->iface_data = _mongoc_cursor_transform_new (filter, mutate, dtor, ctx);

   memcpy (&cursor->iface, &gMongocCursorTransform,
           sizeof (mongoc_cursor_interface_t));

   EXIT;
}

/*==============================================================*/
/* --- mongoc-find-and-modify.c */

/**
 * mongoc_find_and_modify_new:
 *
 * Create a new mongoc_find_and_modify_t.
 *
 * Returns: A newly allocated mongoc_find_and_modify_t. This should be freed
 *    with mongoc_find_and_modify_destroy().
 */
mongoc_find_and_modify_opts_t*
mongoc_find_and_modify_opts_new (void)
{
   mongoc_find_and_modify_opts_t *opts = NULL;

   opts = (mongoc_find_and_modify_opts_t *)bson_malloc0 (sizeof *opts);
   opts->bypass_document_validation = MONGOC_BYPASS_DOCUMENT_VALIDATION_DEFAULT;

   return opts;
}

bool
mongoc_find_and_modify_opts_set_sort (mongoc_find_and_modify_opts_t *opts,
                                      const bson_t                  *sort)
{
   BSON_ASSERT (opts);

   if (sort) {
      _mongoc_bson_destroy_if_set (opts->sort);
      opts->sort = bson_copy (sort);
      return true;
   }
   return false;
}

bool
mongoc_find_and_modify_opts_set_update (mongoc_find_and_modify_opts_t *opts,
                                        const bson_t                  *update)
{
   BSON_ASSERT (opts);

   if (update) {
      _mongoc_bson_destroy_if_set (opts->update);
      opts->update = bson_copy (update);
      return true;
   }
   return false;
}

bool
mongoc_find_and_modify_opts_set_fields (mongoc_find_and_modify_opts_t *opts,
                                        const bson_t                  *fields)
{
   BSON_ASSERT (opts);

   if (fields) {
      _mongoc_bson_destroy_if_set (opts->fields);
      opts->fields = bson_copy (fields);
      return true;
   }
   return false;
}

bool
mongoc_find_and_modify_opts_set_flags (mongoc_find_and_modify_opts_t        *opts,
                                       const mongoc_find_and_modify_flags_t  flags)
{
   BSON_ASSERT (opts);

   opts->flags = flags;
   return true;
}

bool
mongoc_find_and_modify_opts_set_bypass_document_validation (mongoc_find_and_modify_opts_t *opts,
                                                            bool                           bypass)
{
   BSON_ASSERT (opts);

   opts->bypass_document_validation = bypass ?
      MONGOC_BYPASS_DOCUMENT_VALIDATION_TRUE :
      MONGOC_BYPASS_DOCUMENT_VALIDATION_FALSE;
   return true;
}

/**
 * mongoc_find_and_modify_opts_destroy:
 * @opts: A mongoc_find_and_modify_opts_t.
 *
 * Releases a mongoc_find_and_modify_opts_t and all associated memory.
 */
void
mongoc_find_and_modify_opts_destroy (mongoc_find_and_modify_opts_t *opts)
{
   if (opts) {
      _mongoc_bson_destroy_if_set (opts->sort);
      _mongoc_bson_destroy_if_set (opts->update);
      _mongoc_bson_destroy_if_set (opts->fields);
      bson_free (opts);
   }
}

/*==============================================================*/
/* --- mongoc-host-list.c */

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_host_list_equal --
 *
 *       Check two hosts have the same domain (case-insensitive), port,
 *       and address family.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
bool
_mongoc_host_list_equal (const mongoc_host_list_t *host_a,
                         const mongoc_host_list_t *host_b)
{
   return (!strcasecmp (host_a->host_and_port, host_b->host_and_port)
           && host_a->family == host_b->family);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_host_list_destroy_all --
 *
 *       Destroy whole linked list of hosts.
 *
 *--------------------------------------------------------------------------
 */
void
_mongoc_host_list_destroy_all (mongoc_host_list_t *host)
{
   mongoc_host_list_t *tmp;

   while (host) {
      tmp = host->next;
      bson_free (host);
      host = tmp;
   }
}

/*==============================================================*/
/* --- mongoc-memcmp.c */

#ifdef MONGOC_HAVE_WEAK_SYMBOLS
BSON_GNUC_CONST __attribute__((weak)) void
_mongoc_dummy_symbol_to_prevent_memcmp_lto(const unsigned char *b1,
                                           const unsigned char *b2,
                                           const size_t len)
{
    (void) b1;
    (void) b2;
    (void) len;
}
#endif

/* See: http://doc.libsodium.org/helpers/index.html#constant-time-comparison */
int
mongoc_memcmp(const void * const b1_, const void * const b2_, size_t len)
{
#ifdef MONGOC_HAVE_WEAK_SYMBOLS
    const unsigned char *b1 = (const unsigned char *) b1_;
    const unsigned char *b2 = (const unsigned char *) b2_;
#else
    const volatile unsigned char *b1 = (const volatile unsigned char *) b1_;
    const volatile unsigned char *b2 = (const volatile unsigned char *) b2_;
#endif
    size_t               i;
    unsigned char        d = (unsigned char) 0U;

#if MONGOC_HAVE_WEAK_SYMBOLS
    _mongoc_dummy_symbol_to_prevent_memcmp_lto(b1, b2, len);
#endif
    for (i = 0U; i < len; i++) {
        d |= b1[i] ^ b2[i];
    }
    return (int) ((1 & ((d - 1) >> 8)) - 1);
}



/*==============================================================*/
/* --- mongoc-opcode.c */

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_opcode_needs_primary --
 *
 *       Returns true if this operation needs to run on a primary,
 *       false if it does not.
 *
 * Returns:
 *       true, false
 *
 * Side effects:
 *       None
 *
 *--------------------------------------------------------------------------
 */

bool
_mongoc_opcode_needs_primary (mongoc_opcode_t opcode)
{
   bool needs_primary = false;

   switch(opcode) {
   case MONGOC_OPCODE_KILL_CURSORS:
   case MONGOC_OPCODE_GET_MORE:
   case MONGOC_OPCODE_MSG:
   case MONGOC_OPCODE_REPLY:
      needs_primary = false;
      break;
   case MONGOC_OPCODE_QUERY:
      /* In some cases, queries may be run against secondaries.
         However, more information is needed to make that decision.
         Callers with access to read preferences and query flags may
         route queries to a secondary when appropriate */
   case MONGOC_OPCODE_DELETE:
   case MONGOC_OPCODE_INSERT:
   case MONGOC_OPCODE_UPDATE:
   default:
      needs_primary = true;
      break;
   }

   return needs_primary;
}

/*==============================================================*/
/* --- mongoc-openssl.c */

#ifdef MONGOC_ENABLE_OPENSSL


static mongoc_mutex_t * gMongocOpenSslThreadLocks;

static void _mongoc_openssl_thread_startup(void);
static void _mongoc_openssl_thread_cleanup(void);

/**
 * _mongoc_openssl_init:
 *
 * initialization function for SSL
 *
 * This needs to get called early on and is not threadsafe.  Called by
 * mongoc_init.
 */
void
_mongoc_openssl_init (void)
{
   SSL_CTX *ctx;

   SSL_library_init ();
   SSL_load_error_strings ();
   ERR_load_BIO_strings ();
   OpenSSL_add_all_algorithms ();
   _mongoc_openssl_thread_startup ();

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
_mongoc_openssl_cleanup (void)
{
   _mongoc_openssl_thread_cleanup ();
}

static int
_mongoc_openssl_password_cb (char *buf,
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


/** mongoc_openssl_hostcheck
 *
 * rfc 6125 match a given hostname against a given pattern
 *
 * Patterns come from DNS common names or subjectAltNames.
 *
 * This code is meant to implement RFC 6125 6.4.[1-3]
 *
 */
static bool
_mongoc_openssl_hostcheck (const char *pattern,
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
_mongoc_openssl_check_cert (SSL        *ssl,
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
      sans = (STACK_OF (GENERAL_NAME) *) X509_get_ext_d2i (
         (X509 *)peer, NID_subject_alt_name, NULL, NULL);

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
                      _mongoc_openssl_hostcheck (check, host)) {
                     r = 1;
                  }

                  break;
               case GEN_IPADD:

                  if ((length == (int)addrlen) && !memcmp (check, &addr, length)) {
                     r = 1;
                  }

                  break;
               default:
                  BSON_ASSERT (0);
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
                         _mongoc_openssl_hostcheck (check, host)) {
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
_mongoc_openssl_setup_ca (SSL_CTX    *ctx,
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
_mongoc_openssl_setup_crl (SSL_CTX    *ctx,
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
_mongoc_openssl_setup_pem_file (SSL_CTX    *ctx,
                                const char *pem_file,
                                const char *password)
{
   if (!SSL_CTX_use_certificate_chain_file (ctx, pem_file)) {
      return 0;
   }

   if (password) {
      SSL_CTX_set_default_passwd_cb_userdata (ctx, (void *)password);
      SSL_CTX_set_default_passwd_cb (ctx, _mongoc_openssl_password_cb);
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
 * _mongoc_openssl_ctx_new:
 *
 * Create a new ssl context declaratively
 *
 * The opt.pem_pwd parameter, if passed, must exist for the life of this
 * context object (for storing and loading the associated pem file)
 */
SSL_CTX *
_mongoc_openssl_ctx_new (mongoc_ssl_opt_t *opt)
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
        _mongoc_openssl_setup_pem_file (ctx, opt->pem_file, opt->pem_pwd))
       && (!(opt->ca_file ||
             opt->ca_dir) ||
           _mongoc_openssl_setup_ca (ctx, opt->ca_file, opt->ca_dir))
       && (!opt->crl_file || _mongoc_openssl_setup_crl (ctx, opt->crl_file))
       ) {
      return ctx;
   } else {
      SSL_CTX_free (ctx);
      return NULL;
   }
}


char *
_mongoc_openssl_extract_subject (const char *filename)
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
            str = (char *)bson_malloc (ret + 2);
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
_mongoc_openssl_thread_id_callback (void)
{
   unsigned long ret;

   ret = (unsigned long)GetCurrentThreadId ();
   return ret;
}

#else

static unsigned long
_mongoc_openssl_thread_id_callback (void)
{
   unsigned long ret;

   ret = (unsigned long)pthread_self ();
   return ret;
}

#endif

static void
_mongoc_openssl_thread_locking_callback (int         mode,
                                         int         type,
                                         const char *file,
                                         int         line)
{
   if (mode & CRYPTO_LOCK) {
      mongoc_mutex_lock (&gMongocOpenSslThreadLocks[type]);
   } else {
      mongoc_mutex_unlock (&gMongocOpenSslThreadLocks[type]);
   }
}

static void
_mongoc_openssl_thread_startup (void)
{
   int i;

   gMongocOpenSslThreadLocks = (mongoc_mutex_t *)OPENSSL_malloc (CRYPTO_num_locks () * sizeof (mongoc_mutex_t));

   for (i = 0; i < CRYPTO_num_locks (); i++) {
      mongoc_mutex_init(&gMongocOpenSslThreadLocks[i]);
   }

   if (!CRYPTO_get_locking_callback ()) {
      CRYPTO_set_locking_callback (_mongoc_openssl_thread_locking_callback);
      CRYPTO_set_id_callback (_mongoc_openssl_thread_id_callback);
   }
}

static void
_mongoc_openssl_thread_cleanup (void)
{
   int i;

   if (CRYPTO_get_locking_callback () == _mongoc_openssl_thread_locking_callback) {
      CRYPTO_set_locking_callback (NULL);
      CRYPTO_set_id_callback (NULL);
   }

   for (i = 0; i < CRYPTO_num_locks (); i++) {
      mongoc_mutex_destroy (&gMongocOpenSslThreadLocks[i]);
   }
   OPENSSL_free (gMongocOpenSslThreadLocks);
}

#endif

/*==============================================================*/
/* --- mongoc-rand-common-crypto.c */

#ifdef MONGOC_ENABLE_COMMON_CRYPTO

#include <Security/Security.h>
/* rumour has it this wasn't in standard Security.h in ~10.8 */
#include <Security/SecRandom.h>

int _mongoc_rand_bytes(uint8_t *buf, int num) {
	return !SecRandomCopyBytes(kSecRandomDefault, num, buf);
}

int _mongoc_pseudo_rand_bytes(uint8_t *buf, int num) {
	return _mongoc_rand_bytes(buf, num);
}

void mongoc_rand_seed(const void* buf, int num) {
	/* No such thing in Common Crypto */
}

void mongoc_rand_add(const void* buf, int num, double entropy) {
	/* No such thing in Common Crypto */
}

int mongoc_rand_status(void) {
    return 1;
}

#endif

/*==============================================================*/
/* --- mongoc-rand-openssl.c */

#ifdef MONGOC_ENABLE_OPENSSL

int _mongoc_rand_bytes(uint8_t * buf, int num) {
    return RAND_bytes(buf, num);
}

int _mongoc_pseudo_rand_bytes(uint8_t * buf, int num) {
    return RAND_pseudo_bytes(buf, num);
}

void mongoc_rand_seed(const void* buf, int num) {
    RAND_seed(buf, num);
}

void mongoc_rand_add(const void* buf, int num, double entropy) {
    RAND_add(buf, num, entropy);
}

int mongoc_rand_status(void) {
    return RAND_status();
}

#endif

/*==============================================================*/
/* --- mongoc-read-concern.c */

static void
_mongoc_read_concern_freeze (mongoc_read_concern_t *read_concern);


/**
 * mongoc_read_concern_new:
 *
 * Create a new mongoc_read_concern_t.
 *
 * Returns: A newly allocated mongoc_read_concern_t. This should be freed
 *    with mongoc_read_concern_destroy().
 */
mongoc_read_concern_t *
mongoc_read_concern_new (void)
{
   mongoc_read_concern_t *read_concern;

   read_concern = (mongoc_read_concern_t *)bson_malloc0 (sizeof *read_concern);

   return read_concern;
}


mongoc_read_concern_t *
mongoc_read_concern_copy (const mongoc_read_concern_t *read_concern)
{
   mongoc_read_concern_t *ret = NULL;

   if (read_concern) {
      ret = mongoc_read_concern_new ();
      ret->level = bson_strdup (read_concern->level);
   }

   return ret;
}


/**
 * mongoc_read_concern_destroy:
 * @read_concern: A mongoc_read_concern_t.
 *
 * Releases a mongoc_read_concern_t and all associated memory.
 */
void
mongoc_read_concern_destroy (mongoc_read_concern_t *read_concern)
{
   if (read_concern) {
      if (read_concern->compiled.len) {
         bson_destroy (&read_concern->compiled);
      }

      bson_free (read_concern->level);
      bson_free (read_concern);
   }
}


const char *
mongoc_read_concern_get_level (const mongoc_read_concern_t *read_concern)
{
   BSON_ASSERT (read_concern);

   return read_concern->level;
}


/**
 * mongoc_read_concern_set_level:
 * @read_concern: A mongoc_read_concern_t.
 * @level: The read concern level
 *
 * Sets the read concern level. Any string is supported for future compatability
 * but MongoDB 3.2 only accepts "local" and "majority", aka:
 *  - MONGOC_READ_CONCERN_LEVEL_LOCAL
 *  - MONGOC_READ_CONCERN_LEVEL_MAJORITY
 *
 *  If the @read_concern has already been frozen, calling this function will not
 *  alter the read concern level.
 *
 * See the MongoDB docs for more information on readConcernLevel
 */
bool
mongoc_read_concern_set_level (mongoc_read_concern_t *read_concern,
                               const char            *level)
{
   BSON_ASSERT (read_concern);

   if (read_concern->frozen) {
      return false;
   }

   bson_free (read_concern->level);
   read_concern->level = bson_strdup (level);
   return true;
}

/**
 * mongoc_read_concern_get_bson:
 * @read_concern: A mongoc_read_concern_t.
 *
 * This is an internal function.
 *
 * Freeze the read concern if necessary and retrieve the encoded bson_t
 * representing the read concern.
 *
 * You may not modify the read concern further after calling this function.
 *
 * Returns: A bson_t that should not be modified or freed as it is owned by
 *    the mongoc_read_concern_t instance.
 */
const bson_t *
_mongoc_read_concern_get_bson (mongoc_read_concern_t *read_concern) {
   if (!read_concern->frozen) {
       _mongoc_read_concern_freeze (read_concern);
   }

   return &read_concern->compiled;
}

/**
 * mongoc_read_concern_freeze:
 * @read_concern: A mongoc_read_concern_t.
 *
 * This is an internal function.
 *
 * Freeze the read concern if necessary and encode it into a bson_ts which
 * represent the raw bson form and the get last error command form.
 *
 * You may not modify the read concern further after calling this function.
 */
static void
_mongoc_read_concern_freeze (mongoc_read_concern_t *read_concern)
{
   bson_t *compiled;

   BSON_ASSERT (read_concern);

   compiled = &read_concern->compiled;

   read_concern->frozen = true;

   bson_init (compiled);

   BSON_ASSERT (read_concern->level);
   BSON_APPEND_UTF8 (compiled, "level", read_concern->level);
}



/*==============================================================*/
/* --- mongoc-scram.c */

#ifdef MONGOC_ENABLE_CRYPTO

#define MONGOC_SCRAM_SERVER_KEY "Server Key"
#define MONGOC_SCRAM_CLIENT_KEY "Client Key"

#define MONGOC_SCRAM_B64_ENCODED_SIZE(n) (2 * n)

#define MONGOC_SCRAM_B64_HASH_SIZE \
   MONGOC_SCRAM_B64_ENCODED_SIZE (MONGOC_SCRAM_HASH_SIZE)


void
_mongoc_scram_startup()
{
   mongoc_b64_initialize_rmap();
}


void
_mongoc_scram_set_pass (mongoc_scram_t *scram,
                        const char     *pass)
{
   BSON_ASSERT (scram);

   if (scram->pass) {
      bson_zero_free (scram->pass, strlen(scram->pass));
   }

   scram->pass = pass ? bson_strdup (pass) : NULL;
}


void
_mongoc_scram_set_user (mongoc_scram_t *scram,
                        const char     *user)
{
   BSON_ASSERT (scram);

   bson_free (scram->user);
   scram->user = user ? bson_strdup (user) : NULL;
}


void
_mongoc_scram_init (mongoc_scram_t *scram)
{
   BSON_ASSERT (scram);

   memset (scram, 0, sizeof *scram);

   mongoc_crypto_init (&scram->crypto);
}


void
_mongoc_scram_destroy (mongoc_scram_t *scram)
{
   BSON_ASSERT (scram);

   bson_free (scram->user);

   if (scram->pass) {
      bson_zero_free (scram->pass, strlen(scram->pass));
   }

   bson_free (scram->auth_message);
}


static bool
_mongoc_scram_buf_write (const char *src,
                         int32_t     src_len,
                         uint8_t    *outbuf,
                         uint32_t    outbufmax,
                         uint32_t   *outbuflen)
{
   if (src_len < 0) {
      src_len = (int32_t) strlen (src);
   }

   if (*outbuflen + src_len >= outbufmax) {
      return false;
   }

   memcpy (outbuf + *outbuflen, src, src_len);

   *outbuflen += src_len;

   return true;
}


/* generate client-first-message:
 * n,a=authzid,n=encoded-username,r=client-nonce
 *
 * note that a= is optional, so we aren't dealing with that here
 */
static bool
_mongoc_scram_start (mongoc_scram_t *scram,
                     uint8_t        *outbuf,
                     uint32_t        outbufmax,
                     uint32_t       *outbuflen,
                     bson_error_t   *error)
{
   uint8_t nonce[24];
   const char *ptr;
   bool rval = true;

   BSON_ASSERT (scram);
   BSON_ASSERT (outbuf);
   BSON_ASSERT (outbufmax);
   BSON_ASSERT (outbuflen);

   /* auth message is as big as the outbuf just because */
   scram->auth_message = (uint8_t *)bson_malloc (outbufmax);
   scram->auth_messagemax = outbufmax;

   /* the server uses a 24 byte random nonce.  so we do as well */
   if (1 != _mongoc_rand_bytes (nonce, sizeof (nonce))) {
      bson_set_error (error,
                      MONGOC_ERROR_SCRAM,
                      MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,
                      _("SCRAM Failure: could not generate a cryptographically secure nonce in sasl step 1"));
      goto FAIL;
   }

   scram->encoded_nonce_len =
      mongoc_b64_ntop (nonce, sizeof (nonce), scram->encoded_nonce,
                       sizeof (scram->encoded_nonce));

   if (-1 == scram->encoded_nonce_len) {
      bson_set_error (error,
                      MONGOC_ERROR_SCRAM,
                      MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,
                      _("SCRAM Failure: could not encode nonce"));
      goto FAIL;
   }

   if (!_mongoc_scram_buf_write ("n,,n=", -1, outbuf, outbufmax, outbuflen)) {
      goto BUFFER;
   }

   for (ptr = scram->user; *ptr; ptr++) {
      /* RFC 5802 specifies that ',' and '=' and encoded as '=2C' and '=3D'
       * respectively in the user name */
      switch (*ptr) {
      case ',':

         if (!_mongoc_scram_buf_write ("=2C", -1, outbuf, outbufmax,
                                       outbuflen)) {
            goto BUFFER;
         }

         break;
      case '=':

         if (!_mongoc_scram_buf_write ("=3D", -1, outbuf, outbufmax,
                                       outbuflen)) {
            goto BUFFER;
         }

         break;
      default:

         if (!_mongoc_scram_buf_write (ptr, 1, outbuf, outbufmax, outbuflen)) {
            goto BUFFER;
         }

         break;
      }
   }

   if (!_mongoc_scram_buf_write (",r=", -1, outbuf, outbufmax, outbuflen)) {
      goto BUFFER;
   }

   if (!_mongoc_scram_buf_write (scram->encoded_nonce, scram->encoded_nonce_len,
                                 outbuf, outbufmax, outbuflen)) {
      goto BUFFER;
   }

   /* we have to keep track of the conversation to create a client proof later
    * on.  This copies the message we're crafting from the 'n=' portion onwards
    * into a buffer we're managing */
   if (!_mongoc_scram_buf_write ((char *)outbuf + 3, *outbuflen - 3,
                                 scram->auth_message,
                                 scram->auth_messagemax,
                                 &scram->auth_messagelen)) {
      goto BUFFER_AUTH;
   }

   if (!_mongoc_scram_buf_write (",", -1, scram->auth_message,
                                 scram->auth_messagemax,
                                 &scram->auth_messagelen)) {
      goto BUFFER_AUTH;
   }

   goto CLEANUP;

BUFFER_AUTH:
   bson_set_error (error,
                   MONGOC_ERROR_SCRAM,
                   MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,
                   _("SCRAM Failure: could not buffer auth message in sasl step1"));

   goto FAIL;

BUFFER:
   bson_set_error (error,
                   MONGOC_ERROR_SCRAM,
                   MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,
                   _("SCRAM Failure: could not buffer sasl step1"));

   goto FAIL;

FAIL:
   rval = false;

CLEANUP:

   return rval;
}


/* Compute the SCRAM step Hi() as defined in RFC5802 */
static void
_mongoc_scram_salt_password (mongoc_scram_t *scram,
                             const char     *password,
                             uint32_t        password_len,
                             const uint8_t  *salt,
                             uint32_t        salt_len,
                             uint32_t        iterations)
{
   uint8_t intermediate_digest[MONGOC_SCRAM_HASH_SIZE];
   uint8_t start_key[MONGOC_SCRAM_HASH_SIZE];

   int i;
   int k;
   uint8_t *output = scram->salted_password;

   memcpy (start_key, salt, salt_len);

   start_key[salt_len] = 0;
   start_key[salt_len + 1] = 0;
   start_key[salt_len + 2] = 0;
   start_key[salt_len + 3] = 1;

   mongoc_crypto_hmac_sha1 (&scram->crypto,
                            password,
                            password_len,
                            start_key,
                            sizeof (start_key),
                            output
   );

   memcpy (intermediate_digest, output, MONGOC_SCRAM_HASH_SIZE);

   /* intermediateDigest contains Ui and output contains the accumulated XOR:ed result */
   for (i = 2; i <= (int)iterations; i++) {
      mongoc_crypto_hmac_sha1 (&scram->crypto,
                               password,
                               password_len,
                               intermediate_digest,
                               sizeof (intermediate_digest),
                               intermediate_digest);

      for (k = 0; k < MONGOC_SCRAM_HASH_SIZE; k++) {
         output[k] ^= intermediate_digest[k];
      }
   }
}


static bool
_mongoc_scram_generate_client_proof (mongoc_scram_t *scram,
                                     uint8_t        *outbuf,
                                     uint32_t        outbufmax,
                                     uint32_t       *outbuflen)
{
   uint8_t client_key[MONGOC_SCRAM_HASH_SIZE];
   uint8_t stored_key[MONGOC_SCRAM_HASH_SIZE];
   uint8_t client_signature[MONGOC_SCRAM_HASH_SIZE];
   unsigned char client_proof[MONGOC_SCRAM_HASH_SIZE];
   int i;
   int r = 0;

   /* ClientKey := HMAC(saltedPassword, "Client Key") */
   mongoc_crypto_hmac_sha1 (&scram->crypto,
                            scram->salted_password,
                            MONGOC_SCRAM_HASH_SIZE,
                            (uint8_t *)MONGOC_SCRAM_CLIENT_KEY,
                            strlen (MONGOC_SCRAM_CLIENT_KEY),
                            client_key);

   /* StoredKey := H(client_key) */
   mongoc_crypto_sha1 (&scram->crypto, client_key, MONGOC_SCRAM_HASH_SIZE, stored_key);

   /* ClientSignature := HMAC(StoredKey, AuthMessage) */
   mongoc_crypto_hmac_sha1 (&scram->crypto,
                            stored_key,
                            MONGOC_SCRAM_HASH_SIZE,
                            scram->auth_message,
                            scram->auth_messagelen,
                            client_signature);

   /* ClientProof := ClientKey XOR ClientSignature */

   for (i = 0; i < MONGOC_SCRAM_HASH_SIZE; i++) {
      client_proof[i] = client_key[i] ^ client_signature[i];
   }

   r = mongoc_b64_ntop (client_proof, sizeof (client_proof),
                        (char *)outbuf + *outbuflen,
                        outbufmax - *outbuflen);

   if (-1 == r) {
      return false;
   }

   *outbuflen += r;

   return true;
}


/* Parse server-first-message of the form:
 * r=client-nonce|server-nonce,s=user-salt,i=iteration-count
 *
 * Generate client-final-message of the form:
 * c=channel-binding(base64),r=client-nonce|server-nonce,p=client-proof
 */
static bool
_mongoc_scram_step2 (mongoc_scram_t *scram,
                     const uint8_t  *inbuf,
                     uint32_t        inbuflen,
                     uint8_t        *outbuf,
                     uint32_t        outbufmax,
                     uint32_t       *outbuflen,
                     bson_error_t   *error)
{
   uint8_t *val_r = NULL;
   uint32_t val_r_len;
   uint8_t *val_s = NULL;
   uint32_t val_s_len;
   uint8_t *val_i = NULL;
   uint32_t val_i_len;

   uint8_t **current_val;
   uint32_t *current_val_len;

   const uint8_t *ptr;
   const uint8_t *next_comma;

   char *tmp;
   char *hashed_password;

   uint8_t decoded_salt[MONGOC_SCRAM_B64_HASH_SIZE];
   int32_t decoded_salt_len;
   bool rval = true;

   int iterations;

   BSON_ASSERT (scram);
   BSON_ASSERT (outbuf);
   BSON_ASSERT (outbufmax);
   BSON_ASSERT (outbuflen);

   /* all our passwords go through md5 thanks to MONGODB-CR */
   tmp = bson_strdup_printf ("%s:mongo:%s", scram->user, scram->pass);
   hashed_password = _mongoc_hex_md5 (tmp);
   bson_zero_free (tmp, strlen(tmp));

   /* we need all of the incoming message for the final client proof */
   if (!_mongoc_scram_buf_write ((char *)inbuf, inbuflen, scram->auth_message,
                                 scram->auth_messagemax,
                                 &scram->auth_messagelen)) {
      goto BUFFER_AUTH;
   }

   if (!_mongoc_scram_buf_write (",", -1, scram->auth_message,
                                 scram->auth_messagemax,
                                 &scram->auth_messagelen)) {
      goto BUFFER_AUTH;
   }

   for (ptr = inbuf; ptr < inbuf + inbuflen; ) {
      switch (*ptr) {
      case 'r':
         current_val = &val_r;
         current_val_len = &val_r_len;
         break;
      case 's':
         current_val = &val_s;
         current_val_len = &val_s_len;
         break;
      case 'i':
         current_val = &val_i;
         current_val_len = &val_i_len;
         break;
      default:
         bson_set_error (error,
                         MONGOC_ERROR_SCRAM,
                         MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,
                         _("SCRAM Failure: unknown key (%c) in sasl step 2"),
                         *ptr);
         goto FAIL;
         break;
      }

      ptr++;

      if (*ptr != '=') {
         bson_set_error (error,
                         MONGOC_ERROR_SCRAM,
                         MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,
                         _("SCRAM Failure: invalid parse state in sasl step 2"));

         goto FAIL;
      }

      ptr++;

      next_comma = (const uint8_t*)memchr (ptr, ',', (inbuf + inbuflen) - ptr);

      if (next_comma) {
         *current_val_len = (uint32_t) (next_comma - ptr);
      } else {
         *current_val_len = (uint32_t) ((inbuf + inbuflen) - ptr);
      }

      *current_val = (uint8_t *)bson_malloc (*current_val_len + 1);
      memcpy (*current_val, ptr, *current_val_len);
      (*current_val)[*current_val_len] = '\0';

      if (next_comma) {
         ptr = next_comma + 1;
      } else {
         break;
      }
   }

   if (!val_r) {
      bson_set_error (error,
                      MONGOC_ERROR_SCRAM,
                      MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,
                      _("SCRAM Failure: no r param in sasl step 2"));

      goto FAIL;
   }

   if (!val_s) {
      bson_set_error (error,
                      MONGOC_ERROR_SCRAM,
                      MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,
                      _("SCRAM Failure: no s param in sasl step 2"));

      goto FAIL;
   }

   if (!val_i) {
      bson_set_error (error,
                      MONGOC_ERROR_SCRAM,
                      MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,
                      _("SCRAM Failure: no i param in sasl step 2"));

      goto FAIL;
   }

   /* verify our nonce */
   if ((int32_t)val_r_len < scram->encoded_nonce_len ||
       mongoc_memcmp (val_r, scram->encoded_nonce, scram->encoded_nonce_len)) {
      bson_set_error (error,
                      MONGOC_ERROR_SCRAM,
                      MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,
                      _("SCRAM Failure: client nonce not repeated in sasl step 2"));
   }

   *outbuflen = 0;

   if (!_mongoc_scram_buf_write ("c=biws,r=", -1, outbuf, outbufmax,
                                 outbuflen)) {
      goto BUFFER;
   }

   if (!_mongoc_scram_buf_write ((char *)val_r, val_r_len, outbuf, outbufmax,
                                 outbuflen)) {
      goto BUFFER;
   }

   if (!_mongoc_scram_buf_write ((char *)outbuf, *outbuflen,
                                 scram->auth_message,
                                 scram->auth_messagemax,
                                 &scram->auth_messagelen)) {
      goto BUFFER_AUTH;
   }

   if (!_mongoc_scram_buf_write (",p=", -1, outbuf, outbufmax, outbuflen)) {
      goto BUFFER;
   }

   decoded_salt_len =
      mongoc_b64_pton ((char *)val_s, decoded_salt, sizeof (decoded_salt));

   if (-1 == decoded_salt_len) {
      bson_set_error (error,
                      MONGOC_ERROR_SCRAM,
                      MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,
                      _("SCRAM Failure: unable to decode salt in sasl step2"));
      goto FAIL;
   }

   if (16 != decoded_salt_len) {
      bson_set_error (error,
                      MONGOC_ERROR_SCRAM,
                      MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,
                      _("SCRAM Failure: invalid salt length of %d in sasl step2"),
                      decoded_salt_len);
      goto FAIL;
   }

   iterations = (int) bson_ascii_strtoll ((char *)val_i, &tmp, 10);
   /* tmp holds the location of the failed to parse character.  So if it's
    * null, we got to the end of the string and didn't have a parse error */

   if (*tmp) {
      bson_set_error (error,
                      MONGOC_ERROR_SCRAM,
                      MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,
                      _("SCRAM Failure: unable to parse iterations in sasl step2"));
      goto FAIL;
   }

   _mongoc_scram_salt_password (scram, hashed_password, (uint32_t) strlen (
                                   hashed_password), decoded_salt, decoded_salt_len,
                                iterations);

   _mongoc_scram_generate_client_proof (scram, outbuf, outbufmax, outbuflen);

   goto CLEANUP;

BUFFER_AUTH:
   bson_set_error (error,
                   MONGOC_ERROR_SCRAM,
                   MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,
                   _("SCRAM Failure: could not buffer auth message in sasl step2"));

   goto FAIL;

BUFFER:
   bson_set_error (error,
                   MONGOC_ERROR_SCRAM,
                   MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,
                   _("SCRAM Failure: could not buffer sasl step2"));

   goto FAIL;

FAIL:
   rval = false;

CLEANUP:
   bson_free (val_r);
   bson_free (val_s);
   bson_free (val_i);

   if (hashed_password) {
      bson_zero_free (hashed_password, strlen(hashed_password));
   }

   return rval;
}


static bool
_mongoc_scram_verify_server_signature (mongoc_scram_t *scram,
                                       uint8_t        *verification,
                                       uint32_t        len)
{
   uint8_t server_key[MONGOC_SCRAM_HASH_SIZE];
   char encoded_server_signature[MONGOC_SCRAM_B64_HASH_SIZE];
   int32_t encoded_server_signature_len;
   uint8_t server_signature[MONGOC_SCRAM_HASH_SIZE];

   /* ServerKey := HMAC(SaltedPassword, "Server Key") */
   mongoc_crypto_hmac_sha1 (&scram->crypto,
                            scram->salted_password,
                            MONGOC_SCRAM_HASH_SIZE,
                            (uint8_t *)MONGOC_SCRAM_SERVER_KEY,
                            strlen (MONGOC_SCRAM_SERVER_KEY),
                            server_key);

   /* ServerSignature := HMAC(ServerKey, AuthMessage) */
   mongoc_crypto_hmac_sha1 (&scram->crypto,
                            server_key,
                            MONGOC_SCRAM_HASH_SIZE,
                            scram->auth_message,
                            scram->auth_messagelen,
                            server_signature);

   encoded_server_signature_len =
      mongoc_b64_ntop (server_signature, sizeof (server_signature),
                       encoded_server_signature,
                       sizeof (encoded_server_signature));
   if (encoded_server_signature_len == -1) {
      return false;
   }

   return ((int32_t)len == encoded_server_signature_len) &&
          (mongoc_memcmp (verification, encoded_server_signature, len) == 0);
}


static bool
_mongoc_scram_step3 (mongoc_scram_t *scram,
                     const uint8_t  *inbuf,
                     uint32_t        inbuflen,
                     uint8_t        *outbuf,
                     uint32_t        outbufmax,
                     uint32_t       *outbuflen,
                     bson_error_t   *error)
{
   uint8_t *val_e = NULL;
   uint32_t val_e_len;
   uint8_t *val_v = NULL;
   uint32_t val_v_len;

   uint8_t **current_val;
   uint32_t *current_val_len;

   const uint8_t *ptr;
   const uint8_t *next_comma;

   bool rval = true;

   BSON_ASSERT (scram);
   BSON_ASSERT (outbuf);
   BSON_ASSERT (outbufmax);
   BSON_ASSERT (outbuflen);

   for (ptr = inbuf; ptr < inbuf + inbuflen; ) {
      switch (*ptr) {
      case 'e':
         current_val = &val_e;
         current_val_len = &val_e_len;
         break;
      case 'v':
         current_val = &val_v;
         current_val_len = &val_v_len;
         break;
      default:
         bson_set_error (error,
                         MONGOC_ERROR_SCRAM,
                         MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,
                         _("SCRAM Failure: unknown key (%c) in sasl step 3"),
                         *ptr);
         goto FAIL;
         break;
      }

      ptr++;

      if (*ptr != '=') {
         bson_set_error (error,
                         MONGOC_ERROR_SCRAM,
                         MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,
                         _("SCRAM Failure: invalid parse state in sasl step 3"));
         goto FAIL;
      }

      ptr++;

      next_comma = (const uint8_t*)memchr (ptr, ',', (inbuf + inbuflen) - ptr);

      if (next_comma) {
         *current_val_len = (uint32_t) (next_comma - ptr);
      } else {
         *current_val_len = (uint32_t) ((inbuf + inbuflen) - ptr);
      }

      *current_val = (uint8_t *)bson_malloc (*current_val_len + 1);
      memcpy (*current_val, ptr, *current_val_len);
      (*current_val)[*current_val_len] = '\0';

      if (next_comma) {
         ptr = next_comma + 1;
      } else {
         break;
      }
   }

   *outbuflen = 0;

   if (val_e) {
      bson_set_error (error,
                      MONGOC_ERROR_SCRAM,
                      MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,
                      _("SCRAM Failure: authentication failure in sasl step 3 : %s"),
                      val_e);
      goto FAIL;
   }

   if (!val_v) {
      bson_set_error (error,
                      MONGOC_ERROR_SCRAM,
                      MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,
                      _("SCRAM Failure: no v param in sasl step 3"));
      goto FAIL;
   }

   if (!_mongoc_scram_verify_server_signature (scram, val_v, val_v_len)) {
      bson_set_error (error,
                      MONGOC_ERROR_SCRAM,
                      MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,
                      _("SCRAM Failure: could not verify server signature in sasl step 3"));
      goto FAIL;
   }

   goto CLEANUP;

FAIL:
   rval = false;

CLEANUP:
   bson_free (val_e);
   bson_free (val_v);

   return rval;
}


bool
_mongoc_scram_step (mongoc_scram_t *scram,
                    const uint8_t  *inbuf,
                    uint32_t        inbuflen,
                    uint8_t        *outbuf,
                    uint32_t        outbufmax,
                    uint32_t       *outbuflen,
                    bson_error_t   *error)
{
   BSON_ASSERT (scram);
   BSON_ASSERT (inbuf);
   BSON_ASSERT (outbuf);
   BSON_ASSERT (outbuflen);

   scram->step++;

   switch (scram->step) {
   case 1:
      return _mongoc_scram_start (scram, outbuf, outbufmax, outbuflen,
                                  error);
      break;
   case 2:
      return _mongoc_scram_step2 (scram, inbuf, inbuflen, outbuf, outbufmax,
                                  outbuflen, error);
      break;
   case 3:
      return _mongoc_scram_step3 (scram, inbuf, inbuflen, outbuf, outbufmax,
                                  outbuflen, error);
      break;
   default:
      bson_set_error (error,
                      MONGOC_ERROR_SCRAM,
                      MONGOC_ERROR_SCRAM_NOT_DONE,
                      _("SCRAM Failure: maximum steps detected"));
      return false;
      break;
   }

   return true;
}

#endif	/* MONGOC_ENABLE_CRYPTO */

/*==============================================================*/
/* --- mongoc-secure-transport.c */

#ifdef MONGOC_ENABLE_SECURE_TRANSPORT

#include <Security/Security.h>
#include <Security/SecKey.h>
#include <CoreFoundation/CoreFoundation.h>

void
_bson_append_cftyperef (bson_string_t *retval, const char *label, CFTypeRef str) {
   if (str) {
      if (CFGetTypeID(str) == CFStringGetTypeID()) {
         const char *cs = CFStringGetCStringPtr(str, kCFStringEncodingMacRoman) ;

         bson_string_append_printf (retval, "%s%s", label, cs);
      }
   }
}

CFTypeRef
_mongoc_secure_transport_dict_get (CFArrayRef values, CFStringRef label)
{
   if (!values || CFGetTypeID(values) != CFArrayGetTypeID()) {
      return NULL;
   }

   for (CFIndex i = 0; i < CFArrayGetCount(values); ++i) {
      CFStringRef item_label;
      CFDictionaryRef item = CFArrayGetValueAtIndex(values, i);

      if (CFGetTypeID(item) != CFDictionaryGetTypeID()) {
         continue;
      }

      item_label = CFDictionaryGetValue(item, kSecPropertyKeyLabel);
      if (item_label && CFStringCompare(item_label, label, 0) == kCFCompareEqualTo) {
         return CFDictionaryGetValue(item, kSecPropertyKeyValue);
      }
   }

   return NULL;
}

char *
_mongoc_secure_transport_RFC2253_from_cert (SecCertificateRef cert)
{
   CFTypeRef subject_name;
   CFDictionaryRef cert_dict;

   cert_dict = SecCertificateCopyValues (cert, NULL, NULL);
   if (!cert_dict) {
      return NULL;
   }

   subject_name = CFDictionaryGetValue (cert_dict, kSecOIDX509V1SubjectName);
   if (subject_name) {
      subject_name = CFDictionaryGetValue (subject_name, kSecPropertyKeyValue);
   }

   if (subject_name) {
      CFTypeRef value;
      bson_string_t *retval = bson_string_new ("");;

      value = _mongoc_secure_transport_dict_get (subject_name, kSecOIDCommonName);
      _bson_append_cftyperef (retval, "CN=", value);

      value = _mongoc_secure_transport_dict_get (subject_name, kSecOIDOrganizationalUnitName);
      if (value) {
         if (CFGetTypeID(value) == CFStringGetTypeID()) {
            _bson_append_cftyperef (retval, ",OU=", value);
         } else if (CFGetTypeID(value) == CFArrayGetTypeID()) {
            CFIndex len = CFArrayGetCount(value);

            if (len > 0) {
               _bson_append_cftyperef (retval, ",OU=", CFArrayGetValueAtIndex(value, 0));
            }
            if (len > 1) {
               _bson_append_cftyperef (retval, ",", CFArrayGetValueAtIndex(value, 1));
            }
            if (len > 2) {
               _bson_append_cftyperef (retval, ",", CFArrayGetValueAtIndex(value, 2));
            }
         }
      }

      value = _mongoc_secure_transport_dict_get (subject_name, kSecOIDOrganizationName);
      _bson_append_cftyperef (retval, ",O=", value);

      value = _mongoc_secure_transport_dict_get (subject_name, kSecOIDLocalityName);
      _bson_append_cftyperef (retval, ",L=", value);

      value = _mongoc_secure_transport_dict_get (subject_name, kSecOIDStateProvinceName);
      _bson_append_cftyperef (retval, ",ST=", value);

      value = _mongoc_secure_transport_dict_get (subject_name, kSecOIDCountryName);
      _bson_append_cftyperef (retval, ",C=", value);

      value = _mongoc_secure_transport_dict_get (subject_name, kSecOIDStreetAddress);
      _bson_append_cftyperef (retval, ",STREET", value);

      CFRelease (cert_dict);
      return bson_string_free (retval, false);
   }

   CFRelease (cert_dict);
   return NULL;
}

char *
_mongoc_secure_transport_extract_subject (const char *filename)
{
   SecExternalItemType item_type = kSecItemTypeUnknown;
   SecExternalFormat format = kSecFormatUnknown;
   CFArrayRef cert_items = NULL;
   CFDataRef dataref;
   OSStatus res;
   CFErrorRef error;
   int n = 0;
   CFURLRef url;
   CFReadStreamRef read_stream;
   SecTransformRef sec_transform;


   url = CFURLCreateFromFileSystemRepresentation (kCFAllocatorDefault, (const UInt8 *)filename, strlen(filename), false);
   read_stream = CFReadStreamCreateWithFile (kCFAllocatorDefault, url);
   sec_transform = SecTransformCreateReadTransformWithReadStream (read_stream);
   dataref = SecTransformExecute (sec_transform, &error);
   if (error) {
      MONGOC_WARNING("Can't read '%s'", filename);
      return NULL;
   }

   res = SecItemImport(dataref, CFSTR(".pem"), &format, &item_type, 0, NULL, NULL, &cert_items);
   if (res) {
      MONGOC_WARNING("Invalid X.509 PEM file '%s'", filename);
      return NULL;
   }

   if (item_type == kSecItemTypeAggregate) {
      for (n=CFArrayGetCount(cert_items); n > 0; n--) {
         CFTypeID item_id = CFGetTypeID (CFArrayGetValueAtIndex (cert_items, n-1));

         if (item_id == SecCertificateGetTypeID()) {
            SecCertificateRef certificate = (SecCertificateRef)CFArrayGetValueAtIndex(cert_items, n-1);

            return _mongoc_secure_transport_RFC2253_from_cert (certificate);
         }
      }
      MONGOC_WARNING("Can't find certificate in '%s'", filename);
   } else {
      MONGOC_WARNING("Unexpected certificate format in '%s'", filename);
   }

   return NULL;
}

#endif

/*==============================================================*/
/* --- mongoc-server-description.c */

#define ALPHA 0.2


static uint8_t kMongocEmptyBson[] = { 5, 0, 0, 0, 0 };

static bson_oid_t kObjectIdZero = { {0} };

/* Destroy allocated resources within @description, but don't free it */
void
mongoc_server_description_cleanup (mongoc_server_description_t *sd)
{
   BSON_ASSERT(sd);

   bson_destroy (&sd->last_is_master);
}

/* Reset fields inside this sd, but keep same id, host information, and RTT,
   and leave ismaster in empty inited state */
void
mongoc_server_description_reset (mongoc_server_description_t *sd)
{
   BSON_ASSERT(sd);

   /* set other fields to default or empty states. election_id is zeroed. */
   memset (&sd->set_name, 0, sizeof (*sd) - ((char*)&sd->set_name - (char*)sd));
   sd->set_name = NULL;
   sd->type = MONGOC_SERVER_UNKNOWN;

   sd->min_wire_version = MONGOC_DEFAULT_WIRE_VERSION;
   sd->max_wire_version = MONGOC_DEFAULT_WIRE_VERSION;
   sd->max_msg_size = MONGOC_DEFAULT_MAX_MSG_SIZE;
   sd->max_bson_obj_size = MONGOC_DEFAULT_BSON_OBJ_SIZE;
   sd->max_write_batch_size = MONGOC_DEFAULT_WRITE_BATCH_SIZE;

   /* always leave last ismaster in an init-ed state until we destroy sd */
   bson_destroy (&sd->last_is_master);
   bson_init (&sd->last_is_master);
   sd->has_is_master = false;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_server_description_init --
 *
 *       Initialize a new server_description_t.
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
mongoc_server_description_init (mongoc_server_description_t *sd,
                                const char                  *address,
                                uint32_t                     id)
{
   ENTRY;

   BSON_ASSERT (sd);
   BSON_ASSERT (address);

   memset (sd, 0, sizeof *sd);

   sd->id = id;
   sd->type = MONGOC_SERVER_UNKNOWN;
   sd->round_trip_time = -1;

   sd->set_name = NULL;
   sd->set_version = MONGOC_NO_SET_VERSION;
   sd->current_primary = NULL;

   if (!_mongoc_host_list_from_string(&sd->host, address)) {
      MONGOC_WARNING("Failed to parse uri for %s", address);
      return;
   }

   sd->connection_address = sd->host.host_and_port;

   sd->me = NULL;
   sd->min_wire_version = MONGOC_DEFAULT_WIRE_VERSION;
   sd->max_wire_version = MONGOC_DEFAULT_WIRE_VERSION;
   sd->max_msg_size = MONGOC_DEFAULT_MAX_MSG_SIZE;
   sd->max_bson_obj_size = MONGOC_DEFAULT_BSON_OBJ_SIZE;
   sd->max_write_batch_size = MONGOC_DEFAULT_WRITE_BATCH_SIZE;

   bson_init_static (&sd->hosts, kMongocEmptyBson, sizeof (kMongocEmptyBson));
   bson_init_static (&sd->passives, kMongocEmptyBson, sizeof (kMongocEmptyBson));
   bson_init_static (&sd->arbiters, kMongocEmptyBson, sizeof (kMongocEmptyBson));
   bson_init_static (&sd->tags, kMongocEmptyBson, sizeof (kMongocEmptyBson));

   bson_init (&sd->last_is_master);

   EXIT;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_server_description_destroy --
 *
 *       Destroy allocated resources within @description and free
 *       @description.
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
mongoc_server_description_destroy (mongoc_server_description_t *description)
{
   ENTRY;

   mongoc_server_description_cleanup(description);

   bson_free(description);

   EXIT;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_server_description_has_rs_member --
 *
 *       Return true if this address is included in server's list of rs
 *       members, false otherwise.
 *
 * Returns:
 *       true, false
 *
 * Side effects:
 *       None
 *
 *--------------------------------------------------------------------------
 */
bool
mongoc_server_description_has_rs_member(mongoc_server_description_t *server,
                                        const char                  *address)
{
   bson_iter_t member_iter;
   const bson_t *rs_members[3];
   int i;

   if (server->type != MONGOC_SERVER_UNKNOWN) {
      rs_members[0] = &server->hosts;
      rs_members[1] = &server->arbiters;
      rs_members[2] = &server->passives;

      for (i = 0; i < 3; i++) {
         bson_iter_init (&member_iter, rs_members[i]);

         while (bson_iter_next (&member_iter)) {
            if (strcasecmp (address, bson_iter_utf8 (&member_iter, NULL)) == 0) {
               return true;
            }
         }
      }
   }

   return false;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_server_description_has_set_version --
 *
 *      Did this server's ismaster response have a "setVersion" field?
 *
 * Returns:
 *      True if the server description's setVersion is set.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_server_description_has_set_version (mongoc_server_description_t *description)
{
   return description->set_version != MONGOC_NO_SET_VERSION;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_server_description_has_election_id --
 *
 *      Did this server's ismaster response have an "electionId" field?
 *
 * Returns:
 *      True if the server description's electionId is set.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_server_description_has_election_id (mongoc_server_description_t *description)
{
   return 0 != bson_oid_compare (&description->election_id, &kObjectIdZero);
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_server_description_id --
 *
 *      Get the id of this server.
 *
 * Returns:
 *      Server's id.
 *
 *--------------------------------------------------------------------------
 */

uint32_t
mongoc_server_description_id (mongoc_server_description_t *description)
{
   return description->id;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_server_description_host --
 *
 *      Return a reference to the host associated with this server description.
 *
 * Returns:
 *      A mongoc_host_list_t *, this server description's host.
 *
 *--------------------------------------------------------------------------
 */

mongoc_host_list_t *
mongoc_server_description_host (mongoc_server_description_t *description)
{
   return &description->host;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_server_description_type --
 *
 *      Get this server's type, one of the types defined in the Server
 *      Discovery And Monitoring Spec.
 *
 * Returns:
 *      A string.
 *
 *--------------------------------------------------------------------------
 */

const char *
mongoc_server_description_type (mongoc_server_description_t *description)
{
   switch (description->type) {
   case MONGOC_SERVER_UNKNOWN:
      return "Unknown";
   case MONGOC_SERVER_STANDALONE:
      return "Standalone";
   case MONGOC_SERVER_MONGOS:
      return "Mongos";
   case MONGOC_SERVER_POSSIBLE_PRIMARY:
      return "PossiblePrimary";
   case MONGOC_SERVER_RS_PRIMARY:
      return "RSPrimary";
   case MONGOC_SERVER_RS_SECONDARY:
      return "RSSecondary";
   case MONGOC_SERVER_RS_ARBITER:
      return "RSArbiter";
   case MONGOC_SERVER_RS_OTHER:
      return "RSOther";
   case MONGOC_SERVER_RS_GHOST:
      return "RSGhost";
   case MONGOC_SERVER_DESCRIPTION_TYPES:
   default:
      MONGOC_ERROR ("Invalid mongoc_server_description_t type\n");
      return "Invalid";
   }
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_server_description_ismaster --
 *
 *      Return this server's most recent "ismaster" command response.
 *
 * Returns:
 *      A reference to a BSON document, owned by the server description.
 *
 *--------------------------------------------------------------------------
 */

const bson_t *
mongoc_server_description_ismaster (mongoc_server_description_t *description)
{
   return &description->last_is_master;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_server_description_set_state --
 *
 *       Set the server description's server type.
 *
 *--------------------------------------------------------------------------
 */
void
mongoc_server_description_set_state (mongoc_server_description_t *description,
                                     mongoc_server_description_type_t type)
{
   description->type = type;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_server_description_set_set_version --
 *
 *       Set the replica set version of this server.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
void
mongoc_server_description_set_set_version (mongoc_server_description_t *description,
                                           int64_t set_version)
{
   description->set_version = set_version;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_server_description_set_election_id --
 *
 *       Set the election_id of this server. Copies the given ObjectId or,
 *       if it is NULL, zeroes description's election_id.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
void
mongoc_server_description_set_election_id (mongoc_server_description_t *description,
                                           const bson_oid_t *election_id)
{
   if (election_id) {
      bson_oid_copy_unsafe (election_id, &description->election_id);
   } else {
      bson_oid_copy_unsafe (&kObjectIdZero, &description->election_id);
   }
}

/*
 *-------------------------------------------------------------------------
 *
 * mongoc_server_description_update_rtt --
 *
 *       Calculate this server's rtt calculation using an exponentially-
 *       weighted moving average formula.
 *
 * Side effects:
 *       None.
 *
 *-------------------------------------------------------------------------
 */
void
mongoc_server_description_update_rtt (mongoc_server_description_t *server,
                                      int64_t                      new_time)
{
   if (server->round_trip_time == -1) {
      server->round_trip_time = new_time;
   } else {
      server->round_trip_time = ALPHA * new_time + (1 - ALPHA) * server->round_trip_time;
   }
}

/*
 *-------------------------------------------------------------------------
 *
 * Called during SDAM, from topology description's ismaster handler.
 *
 *-------------------------------------------------------------------------
 */

void
mongoc_server_description_handle_ismaster (
   mongoc_server_description_t   *sd,
   const bson_t                  *ismaster_response,
   int64_t                        rtt_msec,
   bson_error_t                  *error)
{
   bson_iter_t iter;
   bool is_master = false;
   bool is_shard = false;
   bool is_secondary = false;
   bool is_arbiter = false;
   bool is_replicaset = false;
   bool is_hidden = false;
   const uint8_t *bytes;
   uint32_t len;
   int num_keys = 0;
   ENTRY;

   BSON_ASSERT (sd);

   mongoc_server_description_reset (sd);
   if (!ismaster_response) {
      EXIT;
   }

   bson_destroy (&sd->last_is_master);
   bson_copy_to (ismaster_response, &sd->last_is_master);
   sd->has_is_master = true;

   bson_iter_init (&iter, &sd->last_is_master);

   while (bson_iter_next (&iter)) {
      num_keys++;
      if (strcmp ("ok", bson_iter_key (&iter)) == 0) {
         /* ismaster responses never have ok: 0, but spec requires we check */
         if (! bson_iter_as_bool (&iter)) goto failure;
      } else if (strcmp ("ismaster", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_BOOL (&iter)) goto failure;
         is_master = bson_iter_bool (&iter);
      } else if (strcmp ("me", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_UTF8 (&iter)) goto failure;
         sd->me = bson_iter_utf8 (&iter, NULL);
      } else if (strcmp ("maxMessageSizeBytes", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_INT32 (&iter)) goto failure;
         sd->max_msg_size = bson_iter_int32 (&iter);
      } else if (strcmp ("maxBsonObjectSize", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_INT32 (&iter)) goto failure;
         sd->max_bson_obj_size = bson_iter_int32 (&iter);
      } else if (strcmp ("maxWriteBatchSize", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_INT32 (&iter)) goto failure;
         sd->max_write_batch_size = bson_iter_int32 (&iter);
      } else if (strcmp ("minWireVersion", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_INT32 (&iter)) goto failure;
         sd->min_wire_version = bson_iter_int32 (&iter);
      } else if (strcmp ("maxWireVersion", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_INT32 (&iter)) goto failure;
         sd->max_wire_version = bson_iter_int32 (&iter);
      } else if (strcmp ("msg", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_UTF8 (&iter)) goto failure;
         is_shard = !!bson_iter_utf8 (&iter, NULL);
      } else if (strcmp ("setName", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_UTF8 (&iter)) goto failure;
         sd->set_name = bson_iter_utf8 (&iter, NULL);
      } else if (strcmp ("setVersion", bson_iter_key (&iter)) == 0) {
         mongoc_server_description_set_set_version (sd,
                                                    bson_iter_as_int64 (&iter));
      } else if (strcmp ("electionId", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_OID (&iter)) goto failure;
         mongoc_server_description_set_election_id (sd, bson_iter_oid (&iter));
      } else if (strcmp ("secondary", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_BOOL (&iter)) goto failure;
         is_secondary = bson_iter_bool (&iter);
      } else if (strcmp ("hosts", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_ARRAY (&iter)) goto failure;
         bson_iter_array (&iter, &len, &bytes);
         bson_init_static (&sd->hosts, bytes, len);
      } else if (strcmp ("passives", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_ARRAY (&iter)) goto failure;
         bson_iter_array (&iter, &len, &bytes);
         bson_init_static (&sd->passives, bytes, len);
      } else if (strcmp ("arbiters", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_ARRAY (&iter)) goto failure;
         bson_iter_array (&iter, &len, &bytes);
         bson_init_static (&sd->arbiters, bytes, len);
      } else if (strcmp ("primary", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_UTF8 (&iter)) goto failure;
         sd->current_primary = bson_iter_utf8 (&iter, NULL);
      } else if (strcmp ("arbiterOnly", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_BOOL (&iter)) goto failure;
         is_arbiter = bson_iter_bool (&iter);
      } else if (strcmp ("isreplicaset", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_BOOL (&iter)) goto failure;
         is_replicaset = bson_iter_bool (&iter);
      } else if (strcmp ("tags", bson_iter_key (&iter)) == 0) {
         if (! BSON_ITER_HOLDS_DOCUMENT (&iter)) goto failure;
         bson_iter_document (&iter, &len, &bytes);
         bson_init_static (&sd->tags, bytes, len);
      } else if (strcmp ("hidden", bson_iter_key (&iter)) == 0) {
         is_hidden = bson_iter_bool (&iter);
      }
   }

   if (is_shard) {
      sd->type = MONGOC_SERVER_MONGOS;
   } else if (sd->set_name) {
      if (is_hidden) {
         sd->type = MONGOC_SERVER_RS_OTHER;
      } else if (is_master) {
         sd->type = MONGOC_SERVER_RS_PRIMARY;
      } else if (is_secondary) {
         sd->type = MONGOC_SERVER_RS_SECONDARY;
      } else if (is_arbiter) {
         sd->type = MONGOC_SERVER_RS_ARBITER;
      } else {
         sd->type = MONGOC_SERVER_RS_OTHER;
      }
   } else if (is_replicaset) {
      sd->type = MONGOC_SERVER_RS_GHOST;
   } else if (num_keys > 0) {
      sd->type = MONGOC_SERVER_STANDALONE;
   } else {
      sd->type = MONGOC_SERVER_UNKNOWN;
   }

   mongoc_server_description_update_rtt(sd, rtt_msec);

   EXIT;

failure:
   sd->type = MONGOC_SERVER_UNKNOWN;
   sd->round_trip_time = -1;

   EXIT;
}

/*
 *-------------------------------------------------------------------------
 *
 * mongoc_server_description_new_copy --
 *
 *       A copy of a server description that you must destroy, or NULL.
 *
 *-------------------------------------------------------------------------
 */
mongoc_server_description_t *
mongoc_server_description_new_copy (const mongoc_server_description_t *description)
{
   mongoc_server_description_t *copy;

   if (!description) {
      return NULL;
   }

   copy = (mongoc_server_description_t *)bson_malloc0(sizeof (*copy));

   copy->id = description->id;
   memcpy (&copy->host, &description->host, sizeof (copy->host));
   copy->round_trip_time = -1;

   copy->connection_address = copy->host.host_and_port;

   /* wait for handle_ismaster to fill these in properly */
   copy->has_is_master = false;
   copy->set_version = MONGOC_NO_SET_VERSION;
   bson_init_static (&copy->hosts, kMongocEmptyBson, sizeof (kMongocEmptyBson));
   bson_init_static (&copy->passives, kMongocEmptyBson, sizeof (kMongocEmptyBson));
   bson_init_static (&copy->arbiters, kMongocEmptyBson, sizeof (kMongocEmptyBson));
   bson_init_static (&copy->tags, kMongocEmptyBson, sizeof (kMongocEmptyBson));

   bson_init (&copy->last_is_master);

   if (description->has_is_master) {
      mongoc_server_description_handle_ismaster (copy, &description->last_is_master,
                                                 description->round_trip_time, NULL);
   }
   /* Preserve the error */
   memcpy (&copy->error, &description->error, sizeof copy->error);
   return copy;
}

/*
 *-------------------------------------------------------------------------
 *
 * mongoc_server_description_filter_eligible --
 *
 *       Given a set of server descriptions, determine which are eligible
 *       as per the Server Selection spec. Determines the number of
 *       eligible servers, and sets any servers that are NOT eligible to
 *       NULL in the descriptions set.
 *
 * Returns:
 *       Number of eligible servers found.
 *
 *-------------------------------------------------------------------------
 */

size_t
mongoc_server_description_filter_eligible (
   mongoc_server_description_t **descriptions,
   size_t                        description_len,
   const mongoc_read_prefs_t    *read_prefs)
{
   const bson_t *rp_tags;
   bson_iter_t rp_tagset_iter;
   bson_iter_t rp_iter;
   bson_iter_t sd_iter;
   uint32_t rp_len;
   uint32_t sd_len;
   const char *rp_val;
   const char *sd_val;
   bool *sd_matched = NULL;
   size_t found;
   size_t i;
   size_t rval = 0;

   if (!read_prefs) {
      /* NULL read_prefs is PRIMARY, no tags to filter by */
      return description_len;
   }

   rp_tags = mongoc_read_prefs_get_tags (read_prefs);

   if (bson_count_keys (rp_tags) == 0) {
      return description_len;
   }

   sd_matched = (bool *) bson_malloc0 (sizeof(bool) * description_len);

   bson_iter_init (&rp_tagset_iter, rp_tags);

   /* for each read preference tagset */
   while (bson_iter_next (&rp_tagset_iter)) {
      found = description_len;

      for (i = 0; i < description_len; i++) {
         sd_matched[i] = true;

         bson_iter_recurse (&rp_tagset_iter, &rp_iter);

         while (bson_iter_next (&rp_iter)) {
            /* TODO: can we have non-utf8 tags? */
            rp_val = bson_iter_utf8 (&rp_iter, &rp_len);

            if (bson_iter_init_find (&sd_iter, &descriptions[i]->tags, bson_iter_key (&rp_iter))) {

               /* If the server description has that key */
               sd_val = bson_iter_utf8 (&sd_iter, &sd_len);

               if (! (sd_len == rp_len && (0 == memcmp(rp_val, sd_val, rp_len)))) {
                  /* If the key value doesn't match, no match */
                  sd_matched[i] = false;
                  found--;
               }
            } else {
               /* If the server description doesn't have that key, no match */
               sd_matched[i] = false;
               found--;
               break;
            }
         }
      }

      if (found) {
         for (i = 0; i < description_len; i++) {
            if (! sd_matched[i]) {
               descriptions[i] = NULL;
            }
         }

         rval = found;
         goto CLEANUP;
      }
   }

   for (i = 0; i < description_len; i++) {
      if (! sd_matched[i]) {
         descriptions[i] = NULL;
      }
   }

CLEANUP:
   bson_free (sd_matched);

   return rval;
}

/*==============================================================*/
/* --- mongoc-server-stream.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "server-stream"

#define COALESCE(x, y) ((x == 0) ? (y) : (x))

mongoc_server_stream_t *
mongoc_server_stream_new (mongoc_topology_description_type_t topology_type,
                          mongoc_server_description_t *sd,
                          mongoc_stream_t *stream)
{
   mongoc_server_stream_t *server_stream;

   BSON_ASSERT (sd);
   BSON_ASSERT (stream);

   server_stream = bson_malloc (sizeof (mongoc_server_stream_t));
   server_stream->topology_type = topology_type;
   server_stream->sd = sd;                       /* becomes owned */
   server_stream->stream = stream;               /* merely borrowed */

   return server_stream;
}

void
mongoc_server_stream_cleanup (mongoc_server_stream_t *server_stream)
{
   if (server_stream) {
      mongoc_server_description_destroy (server_stream->sd);
      bson_free (server_stream);
   }
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_server_stream_max_bson_obj_size --
 *
 *      Return the max bson object size for the given server stream.
 *
 *--------------------------------------------------------------------------
 */

int32_t
mongoc_server_stream_max_bson_obj_size (mongoc_server_stream_t *server_stream)
{
   return COALESCE (server_stream->sd->max_bson_obj_size,
                    MONGOC_DEFAULT_BSON_OBJ_SIZE);
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_server_stream_max_msg_size --
 *
 *      Return the max message size for the given server stream.
 *
 *--------------------------------------------------------------------------
 */

int32_t
mongoc_server_stream_max_msg_size (mongoc_server_stream_t *server_stream)
{
   return COALESCE (server_stream->sd->max_msg_size,
                    MONGOC_DEFAULT_MAX_MSG_SIZE);
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_server_stream_max_write_batch_size --
 *
 *      Return the max write batch size for the given server stream.
 *
 *--------------------------------------------------------------------------
 */

int32_t
mongoc_server_stream_max_write_batch_size (mongoc_server_stream_t *server_stream)
{
   return COALESCE (server_stream->sd->max_write_batch_size,
                    MONGOC_DEFAULT_WRITE_BATCH_SIZE);
}
/*==============================================================*/
/* --- mongoc-set.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "set"

mongoc_set_t *
mongoc_set_new (size_t               nitems,
                mongoc_set_item_dtor dtor,
                void                *dtor_ctx)
{
   mongoc_set_t *set = (mongoc_set_t *)bson_malloc (sizeof (*set));

   set->items_allocated = nitems;
   set->items = (mongoc_set_item_t *)bson_malloc (sizeof (*set->items) * set->items_allocated);
   set->items_len = 0;

   set->dtor = dtor;
   set->dtor_ctx = dtor_ctx;

   return set;
}

static int
mongoc_set_id_cmp (const void *a_,
                   const void *b_)
{
   mongoc_set_item_t *a = (mongoc_set_item_t *)a_;
   mongoc_set_item_t *b = (mongoc_set_item_t *)b_;

   if (a->id == b->id) {
      return 0;
   }

   return a->id < b->id ? -1 : 1;
}

void
mongoc_set_add (mongoc_set_t *set,
                uint32_t      id,
                void         *item)
{
   if (set->items_len >= set->items_allocated) {
      set->items_allocated *= 2;
      set->items = (mongoc_set_item_t *)bson_realloc (set->items,
                                 sizeof (*set->items) * set->items_allocated);
   }

   set->items[set->items_len].id = id;
   set->items[set->items_len].item = item;

   set->items_len++;

   if (set->items_len > 1 && set->items[set->items_len - 2].id > id) {
      qsort (set->items, set->items_len, sizeof (*set->items),
             mongoc_set_id_cmp);
   }
}

void
mongoc_set_rm (mongoc_set_t *set,
               uint32_t      id)
{
   mongoc_set_item_t *ptr;
   mongoc_set_item_t key;
   int i;

   key.id = id;

   ptr = (mongoc_set_item_t *)bsearch (&key, set->items, set->items_len, sizeof (key),
                  mongoc_set_id_cmp);

   if (ptr) {
      set->dtor(ptr->item, set->dtor_ctx);

      i = ptr - set->items;

      if (i != (int)(set->items_len - 1)) {
         memmove (set->items + i, set->items + i + 1,
                  (set->items_len - (i + 1)) * sizeof (key));
      }

      set->items_len--;
   }
}

void *
mongoc_set_get (mongoc_set_t *set,
                uint32_t      id)
{
   mongoc_set_item_t *ptr;
   mongoc_set_item_t key;

   key.id = id;

   ptr = (mongoc_set_item_t *)bsearch (&key, set->items, set->items_len, sizeof (key),
                  mongoc_set_id_cmp);

   return ptr ? ptr->item : NULL;
}

void *
mongoc_set_get_item (mongoc_set_t *set,
                     int           idx)
{
   BSON_ASSERT (set);
   BSON_ASSERT (idx < (int)set->items_len);

   return set->items[idx].item;
}


void
mongoc_set_destroy (mongoc_set_t *set)
{
   int i;

   for (i = 0; i < (int)set->items_len; i++) {
      set->dtor(set->items[i].item, set->dtor_ctx);
   }

   bson_free (set->items);
   bson_free (set);
}

void
mongoc_set_for_each (mongoc_set_t            *set,
                     mongoc_set_for_each_cb_t cb,
                     void                    *ctx)
{
   size_t i;
   mongoc_set_item_t *old_set;
   size_t items_len;

   items_len = set->items_len;

   old_set = (mongoc_set_item_t *)bson_malloc (sizeof (*old_set) * items_len);
   memcpy (old_set, set->items, sizeof (*old_set) * items_len);

   for (i = 0; i < items_len; i++) {
      if (!cb (old_set[i].item, ctx)) {
         break;
      }
   }

   bson_free (old_set);
}


static mongoc_set_item_t *
_mongoc_set_find (mongoc_set_t            *set,
                  mongoc_set_for_each_cb_t cb,
                  void                    *ctx)
{
   size_t i;
   size_t items_len;
   mongoc_set_item_t *item;

   items_len = set->items_len;

   for (i = 0; i < items_len; i++) {
      item = &set->items[i];
      if (cb (item->item, ctx)) {
         return item;
      }
   }

   return NULL;
}


void *
mongoc_set_find_item (mongoc_set_t            *set,
                      mongoc_set_for_each_cb_t cb,
                      void                    *ctx)
{
   mongoc_set_item_t *item;

   if ((item = _mongoc_set_find (set, cb, ctx))) {
      return item->item;
   }

   return NULL;
}


uint32_t
mongoc_set_find_id (mongoc_set_t            *set,
                    mongoc_set_for_each_cb_t cb,
                    void                    *ctx)
{
   mongoc_set_item_t *item;

   if ((item = _mongoc_set_find (set, cb, ctx))) {
      return item->id;
   }

   return 0;
}

/*==============================================================*/
/* --- mongoc-stream-tls-openssl-bio.c */

#ifdef MONGOC_ENABLE_OPENSSL

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "stream-tls-openssl-bio"

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_tls_openssl_bio_create --
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

int
mongoc_stream_tls_openssl_bio_create (BIO *b)
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
 * mongoc_stream_tls_openssl_bio_destroy --
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

int
mongoc_stream_tls_openssl_bio_destroy (BIO *b)
{
   mongoc_stream_tls_t *tls;

   BSON_ASSERT (b);

   tls = (mongoc_stream_tls_t *)b->ptr;

   if (!tls) {
      return -1;
   }

   b->ptr = NULL;
   b->init = 0;
   b->flags = 0;

   ((mongoc_stream_tls_openssl_t *) tls->ctx)->bio = NULL;

   return 1;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_tls_openssl_bio_read --
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

int
mongoc_stream_tls_openssl_bio_read (BIO  *b,
                                    char *buf,
                                    int   len)
{
   mongoc_stream_tls_t *tls;
   int ret;

   BSON_ASSERT (b);
   BSON_ASSERT (buf);
   ENTRY;

   tls = (mongoc_stream_tls_t *)b->ptr;

   if (!tls) {
      RETURN (-1);
   }

   errno = 0;
   ret = (int)mongoc_stream_read (tls->base_stream, buf, len, 0,
                                  tls->timeout_msec);
   BIO_clear_retry_flags (b);

   if ((ret <= 0) && MONGOC_ERRNO_IS_AGAIN (errno)) {
      BIO_set_retry_read (b);
   }

   RETURN (ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_tls_openssl_bio_write --
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

int
mongoc_stream_tls_openssl_bio_write (BIO        *b,
                                     const char *buf,
                                     int         len)
{
   mongoc_stream_tls_t *tls;
   mongoc_iovec_t iov;
   int ret;
   ENTRY;

   BSON_ASSERT (b);
   BSON_ASSERT (buf);

   tls = (mongoc_stream_tls_t *)b->ptr;

   if (!tls) {
      RETURN (-1);
   }

   iov.iov_base = (void *)buf;
   iov.iov_len = len;

   errno = 0;
   TRACE("mongoc_stream_writev is expected to write: %d", len);
   ret = (int)mongoc_stream_writev (tls->base_stream, &iov, 1,
                                    tls->timeout_msec);
   BIO_clear_retry_flags (b);

   if (len > ret) {
      TRACE("Returned short write: %d of %d", ret, len);
   } else {
      TRACE("Completed the %d", ret);
   }
   if (ret <= 0 && MONGOC_ERRNO_IS_AGAIN (errno)) {
      TRACE("%s", "Requesting a retry");
      BIO_set_retry_write (b);
   }


   RETURN (ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_tls_openssl_bio_ctrl --
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

long
mongoc_stream_tls_openssl_bio_ctrl (BIO  *b,
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
 * mongoc_stream_tls_openssl_bio_gets --
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

int
mongoc_stream_tls_openssl_bio_gets (BIO  *b,
                                    char *buf,
                                    int   len)
{
   return -1;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_tls_openssl_bio_puts --
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

int
mongoc_stream_tls_openssl_bio_puts (BIO        *b,
                                    const char *str)
{
   return mongoc_stream_tls_openssl_bio_write (b, str, (int)strlen (str));
}


#endif /* MONGOC_ENABLE_OPENSSL */

/*==============================================================*/
/* --- mongoc-stream-tls-openssl.c */

#ifdef MONGOC_ENABLE_OPENSSL

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "stream-tls-openssl"

#define MONGOC_STREAM_TLS_OPENSSL_BUFFER_SIZE 4096

/* Magic vtable to make our BIO shim */
static BIO_METHOD gMongocStreamTlsOpenSslRawMethods = {
   BIO_TYPE_FILTER,
   "mongoc-stream-tls-glue",
   mongoc_stream_tls_openssl_bio_write,
   mongoc_stream_tls_openssl_bio_read,
   mongoc_stream_tls_openssl_bio_puts,
   mongoc_stream_tls_openssl_bio_gets,
   mongoc_stream_tls_openssl_bio_ctrl,
   mongoc_stream_tls_openssl_bio_create,
   mongoc_stream_tls_openssl_bio_destroy
};


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_stream_tls_openssl_destroy --
 *
 *       Cleanup after usage of a mongoc_stream_tls_openssl_t. Free all allocated
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
_mongoc_stream_tls_openssl_destroy (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_openssl_t *openssl = (mongoc_stream_tls_openssl_t *) tls->ctx;

   BSON_ASSERT (tls);

   BIO_free_all (openssl->bio);
   openssl->bio = NULL;

   mongoc_stream_destroy (tls->base_stream);
   tls->base_stream = NULL;

   SSL_CTX_free (openssl->ctx);
   openssl->ctx = NULL;

   bson_free (openssl);
   bson_free (stream);

   mongoc_counter_streams_active_dec();
   mongoc_counter_streams_disposed_inc();
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_stream_tls_openssl_failed --
 *
 *       Called on stream failure. Same as _mongoc_stream_tls_openssl_destroy()
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
_mongoc_stream_tls_openssl_failed (mongoc_stream_t *stream)
{
   _mongoc_stream_tls_openssl_destroy (stream);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_stream_tls_openssl_close --
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
_mongoc_stream_tls_openssl_close (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   int ret = 0;
   ENTRY;

   BSON_ASSERT (tls);

   ret = mongoc_stream_close (tls->base_stream);
   RETURN (ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_stream_tls_openssl_flush --
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
_mongoc_stream_tls_openssl_flush (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_openssl_t *openssl = (mongoc_stream_tls_openssl_t *) tls->ctx;

   BSON_ASSERT (openssl);

   return BIO_flush (openssl->bio);
}


static ssize_t
_mongoc_stream_tls_openssl_write (mongoc_stream_tls_t *tls,
                                  char                *buf,
                                  size_t               buf_len)
{
   mongoc_stream_tls_openssl_t *openssl = (mongoc_stream_tls_openssl_t *) tls->ctx;
   ssize_t ret;
   int64_t now;
   int64_t expire = 0;
   ENTRY;

   BSON_ASSERT (tls);
   BSON_ASSERT (buf);
   BSON_ASSERT (buf_len);

   if (tls->timeout_msec >= 0) {
      expire = bson_get_monotonic_time () + (tls->timeout_msec * 1000UL);
   }

   ret = BIO_write (openssl->bio, buf, buf_len);
   TRACE("BIO_write returned %ld", ret);

   TRACE("I got ret: %ld and retry: %d", ret, BIO_should_retry (openssl->bio));
   if (ret <= 0) {
      return ret;
   }

   if (expire) {
      now = bson_get_monotonic_time ();

      if ((expire - now) < 0) {
         if (ret < (ssize_t)buf_len) {
            mongoc_counter_streams_timeout_inc();
         }

         tls->timeout_msec = 0;
      } else {
         tls->timeout_msec = (expire - now) / 1000L;
      }
   }

   RETURN (ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_stream_tls_openssl_writev --
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
_mongoc_stream_tls_openssl_writev (mongoc_stream_t *stream,
                                   mongoc_iovec_t  *iov,
                                   size_t           iovcnt,
                                   int32_t          timeout_msec)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   char buf[MONGOC_STREAM_TLS_OPENSSL_BUFFER_SIZE];
   ssize_t ret = 0;
   ssize_t child_ret;
   size_t i;
   size_t iov_pos = 0;

   /* There's a bit of a dance to coalesce vectorized writes into
    * MONGOC_STREAM_TLS_OPENSSL_BUFFER_SIZE'd writes to avoid lots of small tls
    * packets.
    *
    * The basic idea is that we want to combine writes in the buffer if they're
    * smaller than the buffer, flushing as it gets full.  For larger writes, or
    * the last write in the iovec array, we want to ignore the buffer and just
    * write immediately.  We take care of doing buffer writes by re-invoking
    * ourself with a single iovec_t, pointing at our stack buffer.
    */
   char *buf_head = buf;
   char *buf_tail = buf;
   char *buf_end = buf + MONGOC_STREAM_TLS_OPENSSL_BUFFER_SIZE;
   size_t bytes;

   char *to_write = NULL;
   size_t to_write_len;

   BSON_ASSERT (tls);
   BSON_ASSERT (iov);
   BSON_ASSERT (iovcnt);
   ENTRY;

   tls->timeout_msec = timeout_msec;

   for (i = 0; i < iovcnt; i++) {
      iov_pos = 0;

      while (iov_pos < iov[i].iov_len) {
         if (buf_head != buf_tail ||
             ((i + 1 < iovcnt) &&
              ((buf_end - buf_tail) > (int)(iov[i].iov_len - iov_pos)))) {
            /* If we have either of:
             *   - buffered bytes already
             *   - another iovec to send after this one and we don't have more
             *     bytes to send than the size of the buffer.
             *
             * copy into the buffer */

            bytes = BSON_MIN (iov[i].iov_len - iov_pos, buf_end - buf_tail);

            memcpy (buf_tail, (char *) iov[i].iov_base + iov_pos, bytes);
            buf_tail += bytes;
            iov_pos += bytes;

            if (buf_tail == buf_end) {
               /* If we're full, request send */

               to_write = buf_head;
               to_write_len = buf_tail - buf_head;

               buf_tail = buf_head = buf;
            }
         } else {
            /* Didn't buffer, so just write it through */

            to_write = (char *)iov[i].iov_base + iov_pos;
            to_write_len = iov[i].iov_len - iov_pos;

            iov_pos += to_write_len;
         }

         if (to_write) {
            /* We get here if we buffered some bytes and filled the buffer, or
             * if we didn't buffer and have to send out of the iovec */

            child_ret = _mongoc_stream_tls_openssl_write (tls, to_write, to_write_len);
            if (child_ret != (ssize_t)to_write_len) {
               TRACE("Got child_ret: %ld while to_write_len is: %ld",
                     child_ret, to_write_len);
            }

            if (child_ret < 0) {
               TRACE("Returning what I had (%ld) as apposed to the error (%ld, errno:%d)", ret, child_ret, errno);
               RETURN (ret);
            }

            ret += child_ret;

            if (child_ret < (ssize_t)to_write_len) {
               /* we timed out, so send back what we could send */

               RETURN (ret);
            }

            to_write = NULL;
         }
      }
   }

   if (buf_head != buf_tail) {
      /* If we have any bytes buffered, send */

      child_ret = _mongoc_stream_tls_openssl_write (tls, buf_head, buf_tail - buf_head);

      if (child_ret < 0) {
         RETURN (child_ret);
      }

      ret += child_ret;
   }

   if (ret >= 0) {
      mongoc_counter_streams_egress_add (ret);
   }

   RETURN (ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_stream_tls_openssl_readv --
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
_mongoc_stream_tls_openssl_readv (mongoc_stream_t *stream,
                                  mongoc_iovec_t  *iov,
                                  size_t           iovcnt,
                                  size_t           min_bytes,
                                  int32_t          timeout_msec)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_openssl_t *openssl = (mongoc_stream_tls_openssl_t *) tls->ctx;
   ssize_t ret = 0;
   size_t i;
   int read_ret;
   size_t iov_pos = 0;
   int64_t now;
   int64_t expire = 0;
   ENTRY;

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
         read_ret = BIO_read (openssl->bio, (char *)iov[i].iov_base + iov_pos,
                              (int)(iov[i].iov_len - iov_pos));

         /* https://www.openssl.org/docs/crypto/BIO_should_retry.html:
          *
          * If BIO_should_retry() returns false then the precise "error
          * condition" depends on the BIO type that caused it and the return
          * code of the BIO operation. For example if a call to BIO_read() on a
          * socket BIO returns 0 and BIO_should_retry() is false then the cause
          * will be that the connection closed.
          */
         if (read_ret < 0 || (read_ret == 0 && !BIO_should_retry (openssl->bio))) {
            return -1;
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
                  RETURN (-1);
               }

               tls->timeout_msec = 0;
            } else {
               tls->timeout_msec = (expire - now) / 1000L;
            }
         }

         ret += read_ret;

         if ((size_t)ret >= min_bytes) {
            mongoc_counter_streams_ingress_add(ret);
            RETURN (ret);
         }

         iov_pos += read_ret;
      }
   }

   if (ret >= 0) {
      mongoc_counter_streams_ingress_add(ret);
   }

   RETURN (ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_stream_tls_openssl_setsockopt --
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
_mongoc_stream_tls_openssl_setsockopt (mongoc_stream_t *stream,
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


static mongoc_stream_t *
_mongoc_stream_tls_openssl_get_base_stream (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   return tls->base_stream;
}


static bool
_mongoc_stream_tls_openssl_check_closed (mongoc_stream_t *stream) /* IN */
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   BSON_ASSERT (stream);
   return mongoc_stream_check_closed (tls->base_stream);
}


/**
 * mongoc_stream_tls_openssl_do_handshake:
 *
 * force an ssl handshake
 *
 * This will happen on the first read or write otherwise
 */
bool
mongoc_stream_tls_openssl_do_handshake (mongoc_stream_t *stream,
                                        int32_t          timeout_msec)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_openssl_t *openssl = (mongoc_stream_tls_openssl_t *) tls->ctx;

   BSON_ASSERT (tls);
   ENTRY;

   tls->timeout_msec = timeout_msec;

   if (BIO_do_handshake (openssl->bio) == 1) {
      RETURN(true);
   }

   if (!timeout_msec) {
      RETURN(false);
   }

   if (!errno) {
#ifdef _WIN32
      errno = WSAETIMEDOUT;
#else
      errno = ETIMEDOUT;
#endif
   }

   RETURN(false);
}

/**
 * mongoc_stream_tls_openssl_should_retry:
 *
 * If the stream should be retried
 */
bool
mongoc_stream_tls_openssl_should_retry (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_openssl_t *openssl = (mongoc_stream_tls_openssl_t *) tls->ctx;

   BSON_ASSERT (tls);
   ENTRY;

   RETURN(BIO_should_retry (openssl->bio));
}


/**
 * mongoc_stream_tls_openssl_should_read:
 *
 * If the stream should read
 */
bool
mongoc_stream_tls_openssl_should_read (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_openssl_t *openssl = (mongoc_stream_tls_openssl_t *) tls->ctx;

   BSON_ASSERT (tls);
   ENTRY;

   RETURN(BIO_should_read (openssl->bio));
}


/**
 * mongoc_stream_tls_openssl_should_write:
 *
 * If the stream should write
 */
bool
mongoc_stream_tls_openssl_should_write (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_openssl_t *openssl = (mongoc_stream_tls_openssl_t *) tls->ctx;

   BSON_ASSERT (tls);
   ENTRY;

   RETURN(BIO_should_write (openssl->bio));
}


/**
 * mongoc_stream_tls_openssl_check_cert:
 *
 * check the cert returned by the other party
 */
bool
mongoc_stream_tls_openssl_check_cert (mongoc_stream_t *stream,
                                      const char      *host)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_openssl_t *openssl = (mongoc_stream_tls_openssl_t *) tls->ctx;
   SSL *ssl;

   BSON_ASSERT (tls);
   BSON_ASSERT (host);
   ENTRY;

   BIO_get_ssl (openssl->bio, &ssl);

   RETURN(_mongoc_openssl_check_cert (ssl, host, tls->weak_cert_validation));
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_tls_openssl_new --
 *
 *       Creates a new mongoc_stream_tls_openssl_t to communicate with a remote
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
mongoc_stream_tls_openssl_new (mongoc_stream_t  *base_stream,
                               mongoc_ssl_opt_t *opt,
                               int               client)
{
   mongoc_stream_tls_t *tls;
   mongoc_stream_tls_openssl_t *openssl;
   SSL_CTX *ssl_ctx = NULL;
   BIO *bio_ssl = NULL;
   BIO *bio_mongoc_shim = NULL;

   BSON_ASSERT(base_stream);
   BSON_ASSERT(opt);
   ENTRY;

   ssl_ctx = _mongoc_openssl_ctx_new (opt);

   if (!ssl_ctx) {
      RETURN(NULL);
   }

   bio_ssl = BIO_new_ssl (ssl_ctx, client);
   if (!bio_ssl) {
      SSL_CTX_free (ssl_ctx);
      RETURN(NULL);
   }

   bio_mongoc_shim = BIO_new (&gMongocStreamTlsOpenSslRawMethods);
   if (!bio_mongoc_shim) {
      BIO_free_all (bio_ssl);
      RETURN(NULL);
   }


   BIO_push (bio_ssl, bio_mongoc_shim);

   openssl = (mongoc_stream_tls_openssl_t *)bson_malloc0 (sizeof *openssl);
   openssl->bio = bio_ssl;
   openssl->ctx = ssl_ctx;

   tls = (mongoc_stream_tls_t *)bson_malloc0 (sizeof *tls);
   tls->parent.type = MONGOC_STREAM_TLS;
   tls->parent.destroy = _mongoc_stream_tls_openssl_destroy;
   tls->parent.failed = _mongoc_stream_tls_openssl_failed;
   tls->parent.close = _mongoc_stream_tls_openssl_close;
   tls->parent.flush = _mongoc_stream_tls_openssl_flush;
   tls->parent.writev = _mongoc_stream_tls_openssl_writev;
   tls->parent.readv = _mongoc_stream_tls_openssl_readv;
   tls->parent.setsockopt = _mongoc_stream_tls_openssl_setsockopt;
   tls->parent.get_base_stream = _mongoc_stream_tls_openssl_get_base_stream;
   tls->parent.check_closed = _mongoc_stream_tls_openssl_check_closed;
   tls->weak_cert_validation = opt->weak_cert_validation;
   tls->should_read = mongoc_stream_tls_openssl_should_read;
   tls->should_write = mongoc_stream_tls_openssl_should_write;
   tls->should_retry = mongoc_stream_tls_openssl_should_retry;
   tls->do_handshake = mongoc_stream_tls_openssl_do_handshake;
   tls->check_cert = mongoc_stream_tls_openssl_check_cert;
   tls->ctx = (void *)openssl;
   tls->timeout_msec = -1;
   tls->base_stream = base_stream;
   bio_mongoc_shim->ptr = tls;

   mongoc_counter_streams_active_inc();

   RETURN((mongoc_stream_t *)tls);
}

#endif /* MONGOC_ENABLE_OPENSSL */

/*==============================================================*/
/* --- mongoc-stream-tls-secure-transport.c */

#ifdef MONGOC_ENABLE_SECURE_TRANSPORT

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "stream-tls-secure_transport"

static void
_mongoc_stream_tls_secure_transport_destroy (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_transport_t *secure_transport = (mongoc_stream_tls_secure_transport_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (tls);

   bson_free (secure_transport);
   bson_free (stream);

   mongoc_counter_streams_active_dec();
   mongoc_counter_streams_disposed_inc();
   EXIT;
}

static void
_mongoc_stream_tls_secure_transport_failed (mongoc_stream_t *stream)
{
   ENTRY;
   _mongoc_stream_tls_secure_transport_destroy (stream);
   EXIT;
}

static int
_mongoc_stream_tls_secure_transport_close (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_transport_t *secure_transport = (mongoc_stream_tls_secure_transport_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (tls);
   RETURN (0);
}

static int
_mongoc_stream_tls_secure_transport_flush (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_transport_t *secure_transport = (mongoc_stream_tls_secure_transport_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (tls);
   RETURN (0);
}

static ssize_t
_mongoc_stream_tls_secure_transport_writev (mongoc_stream_t *stream,
                                            mongoc_iovec_t  *iov,
                                            size_t           iovcnt,
                                            int32_t          timeout_msec)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_transport_t *secure_transport = (mongoc_stream_tls_secure_transport_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (tls);
   RETURN (0);
}

static ssize_t
_mongoc_stream_tls_secure_transport_readv (mongoc_stream_t *stream,
                                           mongoc_iovec_t  *iov,
                                           size_t           iovcnt,
                                           size_t           min_bytes,
                                           int32_t          timeout_msec)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_transport_t *secure_transport = (mongoc_stream_tls_secure_transport_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (tls);
   RETURN (0);
}
static int
_mongoc_stream_tls_secure_transport_setsockopt (mongoc_stream_t *stream,
                                                int              level,
                                                int              optname,
                                                void            *optval,
                                                socklen_t        optlen)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;

   ENTRY;
   BSON_ASSERT (tls);
   RETURN (mongoc_stream_setsockopt (tls->base_stream,
                                     level,
                                     optname,
                                     optval,
                                     optlen));
}
static mongoc_stream_t *
_mongoc_stream_tls_secure_transport_get_base_stream (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;

   ENTRY;
   BSON_ASSERT (tls);
   RETURN (tls->base_stream);
}


static bool
_mongoc_stream_tls_secure_transport_check_closed (mongoc_stream_t *stream) /* IN */
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;

   ENTRY;
   BSON_ASSERT (tls);
   RETURN (mongoc_stream_check_closed (tls->base_stream));
}

bool
mongoc_stream_tls_secure_transport_do_handshake (mongoc_stream_t *stream,
                                                 int32_t          timeout_msec)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_transport_t *secure_transport = (mongoc_stream_tls_secure_transport_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (tls);
   RETURN (false);
}

bool
mongoc_stream_tls_secure_transport_should_retry (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_transport_t *secure_transport = (mongoc_stream_tls_secure_transport_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (tls);
   RETURN (false);
}

bool
mongoc_stream_tls_secure_transport_should_read (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_transport_t *secure_transport = (mongoc_stream_tls_secure_transport_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (tls);
   RETURN (false);
}


bool
mongoc_stream_tls_secure_transport_should_write (mongoc_stream_t *stream)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_transport_t *secure_transport = (mongoc_stream_tls_secure_transport_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (tls);
   RETURN (false);
}

bool
mongoc_stream_tls_secure_transport_check_cert (mongoc_stream_t *stream,
                                               const char      *host)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)stream;
   mongoc_stream_tls_secure_transport_t *secure_transport = (mongoc_stream_tls_secure_transport_t *) tls->ctx;

   ENTRY;
   BSON_ASSERT (tls);
   RETURN (false);
}

mongoc_stream_t *
mongoc_stream_tls_secure_transport_new (mongoc_stream_t  *base_stream,
                                        mongoc_ssl_opt_t *opt,
                                        int               client)
{
   mongoc_stream_tls_t *tls;
   mongoc_stream_tls_secure_transport_t *secure_transport;

   ENTRY;
   BSON_ASSERT(base_stream);
   BSON_ASSERT(opt);


   secure_transport = (mongoc_stream_tls_secure_transport_t *)bson_malloc0 (sizeof *secure_transport);

   tls = (mongoc_stream_tls_t *)bson_malloc0 (sizeof *tls);
   tls->parent.type = MONGOC_STREAM_TLS;
   tls->parent.destroy = _mongoc_stream_tls_secure_transport_destroy;
   tls->parent.failed = _mongoc_stream_tls_secure_transport_failed;
   tls->parent.close = _mongoc_stream_tls_secure_transport_close;
   tls->parent.flush = _mongoc_stream_tls_secure_transport_flush;
   tls->parent.writev = _mongoc_stream_tls_secure_transport_writev;
   tls->parent.readv = _mongoc_stream_tls_secure_transport_readv;
   tls->parent.setsockopt = _mongoc_stream_tls_secure_transport_setsockopt;
   tls->parent.get_base_stream = _mongoc_stream_tls_secure_transport_get_base_stream;
   tls->parent.check_closed = _mongoc_stream_tls_secure_transport_check_closed;
   tls->weak_cert_validation = opt->weak_cert_validation;
   tls->should_read = mongoc_stream_tls_secure_transport_should_read;
   tls->should_write = mongoc_stream_tls_secure_transport_should_write;
   tls->should_retry = mongoc_stream_tls_secure_transport_should_retry;
   tls->do_handshake = mongoc_stream_tls_secure_transport_do_handshake;
   tls->check_cert = mongoc_stream_tls_secure_transport_check_cert;
   tls->ctx = (void *)secure_transport;
   tls->timeout_msec = -1;
   tls->base_stream = base_stream;

   mongoc_counter_streams_active_inc();

   RETURN((mongoc_stream_t *)tls);
}
#endif /* MONGOC_ENABLE_SECURE_TRANSPORT */


/*==============================================================*/
/* --- mongoc-topology.c */

static void
_mongoc_topology_background_thread_stop (mongoc_topology_t *topology);

static void
_mongoc_topology_background_thread_start (mongoc_topology_t *topology);

static void
_mongoc_topology_request_scan (mongoc_topology_t *topology);

static bool
_mongoc_topology_reconcile_add_nodes (void *item,
                                      void *ctx)
{
   mongoc_server_description_t *sd = item;
   mongoc_topology_t *topology = (mongoc_topology_t *)ctx;
   mongoc_topology_scanner_t *scanner = topology->scanner;

   /* quickly search by id, then check if a node for this host was retired in
    * this scan. */
   if (! mongoc_topology_scanner_get_node (scanner, sd->id) &&
       ! mongoc_topology_scanner_has_node_for_host (scanner, &sd->host)) {
      mongoc_topology_scanner_add_and_scan (scanner, &sd->host, sd->id,
                                            topology->connect_timeout_msec);
   }

   return true;
}

void
mongoc_topology_reconcile (mongoc_topology_t *topology) {
   mongoc_topology_scanner_node_t *ele, *tmp;
   mongoc_topology_description_t *description;
   mongoc_topology_scanner_t *scanner;

   description = &topology->description;
   scanner = topology->scanner;

   /* Add newly discovered nodes */
   mongoc_set_for_each(description->servers,
                       _mongoc_topology_reconcile_add_nodes,
                       topology);

   /* Remove removed nodes */
   DL_FOREACH_SAFE (scanner->nodes, ele, tmp) {
      if (!mongoc_topology_description_server_by_id (description, ele->id, NULL)) {
         mongoc_topology_scanner_node_retire (ele);
      }
   }
}
/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_topology_scanner_cb --
 *
 *       Callback method to handle ismaster responses received by async
 *       command objects.
 *
 *       NOTE: This method locks the given topology's mutex.
 *
 *-------------------------------------------------------------------------
 */

void
_mongoc_topology_scanner_cb (uint32_t      id,
                             const bson_t *ismaster_response,
                             int64_t       rtt_msec,
                             void         *data,
                             bson_error_t *error)
{
   mongoc_topology_t *topology;
   mongoc_server_description_t *sd;

   BSON_ASSERT (data);

   topology = (mongoc_topology_t *)data;

   if (rtt_msec >= 0) {
      /* If the scanner failed to create a socket for this server, that means
       * we're in scanner_start, which means we're under the mutex.  So don't
       * take the mutex for rtt < 0 */

      mongoc_mutex_lock (&topology->mutex);
   }

   sd = mongoc_topology_description_server_by_id (&topology->description, id,
                                                  NULL);

   if (sd) {
      mongoc_topology_description_handle_ismaster (&topology->description, sd,
                                                   ismaster_response, rtt_msec,
                                                   error);

      /* The processing of the ismaster results above may have added/removed
       * server descriptions. We need to reconcile that with our monitoring agents
       */

      mongoc_topology_reconcile(topology);

      /* TODO only wake up all clients if we found any topology changes */
      mongoc_cond_broadcast (&topology->cond_client);
   }

   if (rtt_msec >= 0) {
      mongoc_mutex_unlock (&topology->mutex);
   }
}

/*
 *-------------------------------------------------------------------------
 *
 * mongoc_topology_new --
 *
 *       Creates and returns a new topology object.
 *
 * Returns:
 *       A new topology object.
 *
 * Side effects:
 *       None.
 *
 *-------------------------------------------------------------------------
 */
mongoc_topology_t *
mongoc_topology_new (const mongoc_uri_t *uri,
                     bool                single_threaded)
{
   mongoc_topology_t *topology;
   mongoc_topology_description_type_t init_type;
   uint32_t id;
   const mongoc_host_list_t *hl;

   BSON_ASSERT (uri);

   topology = (mongoc_topology_t *)bson_malloc0(sizeof *topology);

   /*
    * Not ideal, but there's no great way to do this.
    * Base on the URI, we assume:
    *   - if we've got a replicaSet name, initialize to RS_NO_PRIMARY
    *   - otherwise, if the seed list has a single host, initialize to SINGLE
    *   - everything else gets initialized to UNKNOWN
    */
   if (mongoc_uri_get_replica_set(uri)) {
      init_type = MONGOC_TOPOLOGY_RS_NO_PRIMARY;
   } else {
      hl = mongoc_uri_get_hosts(uri);
      if (hl->next) {
         init_type = MONGOC_TOPOLOGY_UNKNOWN;
      } else {
         init_type = MONGOC_TOPOLOGY_SINGLE;
      }
   }

   mongoc_topology_description_init(&topology->description, init_type);
   topology->description.set_name = bson_strdup(mongoc_uri_get_replica_set(uri));

   topology->uri = mongoc_uri_copy (uri);
   topology->scanner = mongoc_topology_scanner_new (topology->uri,
                                                    _mongoc_topology_scanner_cb,
                                                    topology);
   topology->single_threaded = single_threaded;
   if (single_threaded) {
      /* Server Selection Spec:
       *
       *   "Single-threaded drivers MUST provide a "serverSelectionTryOnce"
       *   mode, in which the driver scans the topology exactly once after
       *   server selection fails, then either selects a server or raises an
       *   error.
       *
       *   "The serverSelectionTryOnce option MUST be true by default."
       */
      topology->server_selection_try_once = mongoc_uri_get_option_as_bool (
         uri,
         "serverselectiontryonce",
         true);
   } else {
      topology->server_selection_try_once = false;
   }

   topology->server_selection_timeout_msec = mongoc_uri_get_option_as_int32(
      topology->uri, "serverselectiontimeoutms",
      MONGOC_TOPOLOGY_SERVER_SELECTION_TIMEOUT_MS);

   topology->local_threshold_msec = mongoc_uri_get_option_as_int32 (
      topology->uri, "localthresholdms",
      MONGOC_TOPOLOGY_LOCAL_THRESHOLD_MS);

   /* Total time allowed to check a server is connectTimeoutMS.
    * Server Discovery And Monitoring Spec:
    *
    *   "The socket used to check a server MUST use the same connectTimeoutMS as
    *   regular sockets. Multi-threaded clients SHOULD set monitoring sockets'
    *   socketTimeoutMS to the connectTimeoutMS."
    */
   topology->connect_timeout_msec = mongoc_uri_get_option_as_int32(
      topology->uri, "connecttimeoutms",
      MONGOC_DEFAULT_CONNECTTIMEOUTMS);

   topology->heartbeat_msec = mongoc_uri_get_option_as_int32(
      topology->uri, "heartbeatfrequencyms",
      (single_threaded ? MONGOC_TOPOLOGY_HEARTBEAT_FREQUENCY_MS_SINGLE_THREADED :
            MONGOC_TOPOLOGY_HEARTBEAT_FREQUENCY_MS_MULTI_THREADED)
   );

   mongoc_mutex_init (&topology->mutex);
   mongoc_cond_init (&topology->cond_client);
   mongoc_cond_init (&topology->cond_server);

   for ( hl = mongoc_uri_get_hosts (uri); hl; hl = hl->next) {
      mongoc_topology_description_add_server (&topology->description,
                                              hl->host_and_port,
                                              &id);
      mongoc_topology_scanner_add (topology->scanner, hl, id);
   }

   if (! topology->single_threaded) {
       _mongoc_topology_background_thread_start (topology);
   }

   return topology;
}

/*
 *-------------------------------------------------------------------------
 *
 * mongoc_topology_destroy --
 *
 *       Free the memory associated with this topology object.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @topology will be cleaned up.
 *
 *-------------------------------------------------------------------------
 */
void
mongoc_topology_destroy (mongoc_topology_t *topology)
{
   if (!topology) {
      return;
   }

   _mongoc_topology_background_thread_stop (topology);

   mongoc_uri_destroy (topology->uri);
   mongoc_topology_description_destroy(&topology->description);
   mongoc_topology_scanner_destroy (topology->scanner);
   mongoc_cond_destroy (&topology->cond_client);
   mongoc_cond_destroy (&topology->cond_server);
   mongoc_mutex_destroy (&topology->mutex);

   bson_free(topology);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_run_scanner --
 *
 *       Not threadsafe, the expectation is that we're either single
 *       threaded or only the background thread runs scans.
 *
 *       Crank the underlying scanner until we've timed out or finished.
 *
 * Returns:
 *       true if there is more work to do, false otherwise
 *
 *--------------------------------------------------------------------------
 */
static bool
_mongoc_topology_run_scanner (mongoc_topology_t *topology,
                              int64_t        work_msec)
{
   int64_t now;
   int64_t expire_at;
   bool keep_going = true;

   now = bson_get_monotonic_time ();
   expire_at = now + (work_msec * 1000);

   /* while there is more work to do and we haven't timed out */
   while (keep_going && now <= expire_at) {
      keep_going = mongoc_topology_scanner_work (topology->scanner, (expire_at - now) / 1000);

      if (keep_going) {
         now = bson_get_monotonic_time ();
      }
   }

   return keep_going;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_do_blocking_scan --
 *
 *       Monitoring entry for single-threaded use case. Assumes the caller
 *       has checked that it's the right time to scan.
 *
 *--------------------------------------------------------------------------
 */
static void
_mongoc_topology_do_blocking_scan (mongoc_topology_t *topology, bson_error_t *error) {
   mongoc_topology_scanner_start (topology->scanner,
                                  topology->connect_timeout_msec,
                                  true);

   while (_mongoc_topology_run_scanner (topology,
                                        topology->connect_timeout_msec)) {}

   /* Aggregate all scanner errors, if any */
   mongoc_topology_scanner_sum_errors (topology->scanner, error);
   /* "retired" nodes can be checked again in the next scan */
   mongoc_topology_scanner_reset (topology->scanner);
   topology->last_scan = bson_get_monotonic_time ();
   topology->stale = false;
}

/*
 *-------------------------------------------------------------------------
 *
 * mongoc_topology_select --
 *
 *       Selects a server description for an operation based on @optype
 *       and @read_prefs.
 *
 *       NOTE: this method returns a copy of the original server
 *       description. Callers must own and clean up this copy.
 *
 *       NOTE: this method locks and unlocks @topology's mutex.
 *
 * Parameters:
 *       @topology: The topology.
 *       @optype: Whether we are selecting for a read or write operation.
 *       @read_prefs: Required, the read preferences for the command.
 *       @error: Required, out pointer for error info.
 *
 * Returns:
 *       A mongoc_server_description_t, or NULL on failure, in which case
 *       @error will be set.
 *
 * Side effects:
 *       @error may be set.
 *
 *-------------------------------------------------------------------------
 */
mongoc_server_description_t *
mongoc_topology_select (mongoc_topology_t         *topology,
                        mongoc_ss_optype_t         optype,
                        const mongoc_read_prefs_t *read_prefs,
                        bson_error_t              *error)
{
   int r;
   int64_t local_threshold_ms;
   mongoc_server_description_t *selected_server = NULL;
   bool try_once;
   int64_t sleep_usec;
   bool tried_once;
   bson_error_t scanner_error = { 0 };

   /* These names come from the Server Selection Spec pseudocode */
   int64_t loop_start;  /* when we entered this function */
   int64_t loop_end;    /* when we last completed a loop (single-threaded) */
   int64_t scan_ready;  /* the soonest we can do a blocking scan */
   int64_t next_update; /* the latest we must do a blocking scan */
   int64_t expire_at;   /* when server selection timeout expires */

   BSON_ASSERT (topology);

   local_threshold_ms = topology->local_threshold_msec;
   try_once = topology->server_selection_try_once;
   loop_start = loop_end = bson_get_monotonic_time ();
   expire_at = loop_start
               + ((int64_t) topology->server_selection_timeout_msec * 1000);

   if (topology->single_threaded) {
      tried_once = false;
      next_update = topology->last_scan + topology->heartbeat_msec * 1000;
      if (next_update < loop_start) {
         /* we must scan now */
         topology->stale = true;
      }

      /* until we find a server or time out */
      for (;;) {
         if (topology->stale) {
            /* how soon are we allowed to scan? */
            scan_ready = topology->last_scan
                         + MONGOC_TOPOLOGY_MIN_HEARTBEAT_FREQUENCY_MS * 1000;

            if (scan_ready > expire_at && !try_once) {
               /* selection timeout will expire before min heartbeat passes */
               bson_set_error(error,
                              MONGOC_ERROR_SERVER_SELECTION,
                              MONGOC_ERROR_SERVER_SELECTION_FAILURE,
                              _("No suitable servers found: "
                              "`minheartbeatfrequencyms` not reached yet"));
               goto FAIL;
            }

            sleep_usec = scan_ready - loop_end;
            if (sleep_usec > 0) {
               _mongoc_usleep (sleep_usec);
            }

            /* takes up to connectTimeoutMS. sets "last_scan", clears "stale" */
            _mongoc_topology_do_blocking_scan (topology, &scanner_error);
            tried_once = true;
         }

         selected_server = mongoc_topology_description_select(&topology->description,
                                                              optype,
                                                              read_prefs,
                                                              local_threshold_ms);

         if (selected_server) {
            return mongoc_server_description_new_copy(selected_server);
         }

         topology->stale = true;

         if (try_once) {
            if (tried_once) {
               if (scanner_error.code) {
                  bson_set_error(error,
                                 MONGOC_ERROR_SERVER_SELECTION,
                                 MONGOC_ERROR_SERVER_SELECTION_FAILURE,
                                 _("No suitable servers found "
                                 "(`serverselectiontryonce` set): %s"), scanner_error.message);
               } else {
                  bson_set_error(error,
                                 MONGOC_ERROR_SERVER_SELECTION,
                                 MONGOC_ERROR_SERVER_SELECTION_FAILURE,
                                 _("No suitable servers found "
                                 "(`serverselectiontryonce` set)"));
               }
               goto FAIL;
            }
         } else {
            loop_end = bson_get_monotonic_time ();

            if (loop_end > expire_at) {
               /* no time left in server_selection_timeout_msec */
               bson_set_error(error,
                              MONGOC_ERROR_SERVER_SELECTION,
                              MONGOC_ERROR_SERVER_SELECTION_FAILURE,
                              _("No suitable servers found: "
                              "`serverselectiontimeoutms` timed out"));
               goto FAIL;
            }
         }
      }
   }

   /* With background thread */
   /* we break out when we've found a server or timed out */
   for (;;) {
      mongoc_mutex_lock (&topology->mutex);
      selected_server = mongoc_topology_description_select(&topology->description,
                                                           optype,
                                                           read_prefs,
                                                           local_threshold_ms);

      if (! selected_server) {
         _mongoc_topology_request_scan (topology);

         r = mongoc_cond_timedwait (&topology->cond_client, &topology->mutex,
                                    (expire_at - loop_start) / 1000);

         mongoc_mutex_unlock (&topology->mutex);

#ifdef _WIN32
         if (r == WSAETIMEDOUT) {
#else
         if (r == ETIMEDOUT) {
#endif
            /* handle timeouts */
            bson_set_error(error,
                           MONGOC_ERROR_SERVER_SELECTION,
                           MONGOC_ERROR_SERVER_SELECTION_FAILURE,
                           _("Timed out trying to select a server"));
            goto FAIL;
         } else if (r) {
            bson_set_error(error,
                           MONGOC_ERROR_SERVER_SELECTION,
                           MONGOC_ERROR_SERVER_SELECTION_FAILURE,
                           _("Unknown error '%d' received while waiting on thread condition"),
                           r);
            goto FAIL;
         }

         loop_start = bson_get_monotonic_time ();

         if (loop_start > expire_at) {
            bson_set_error(error,
                           MONGOC_ERROR_SERVER_SELECTION,
                           MONGOC_ERROR_SERVER_SELECTION_FAILURE,
                           _("Timed out trying to select a server"));
            goto FAIL;
         }
      } else {
         selected_server = mongoc_server_description_new_copy(selected_server);
         mongoc_mutex_unlock (&topology->mutex);
         return selected_server;
      }
   }

FAIL:
   topology->stale = true;

   return NULL;
}

/*
 *-------------------------------------------------------------------------
 *
 * mongoc_topology_server_by_id --
 *
 *      Get the server description for @id, if that server is present
 *      in @description. Otherwise, return NULL and fill out the optional
 *      @error.
 *
 *      NOTE: this method returns a copy of the original server
 *      description. Callers must own and clean up this copy.
 *
 *      NOTE: this method locks and unlocks @topology's mutex.
 *
 * Returns:
 *      A mongoc_server_description_t, or NULL.
 *
 * Side effects:
 *      Fills out optional @error if server not found.
 *
 *-------------------------------------------------------------------------
 */

mongoc_server_description_t *
mongoc_topology_server_by_id (mongoc_topology_t *topology,
                              uint32_t id,
                              bson_error_t *error)
{
   mongoc_server_description_t *sd;

   mongoc_mutex_lock (&topology->mutex);

   sd = mongoc_server_description_new_copy (
      mongoc_topology_description_server_by_id (&topology->description,
                                                id,
                                                error));

   mongoc_mutex_unlock (&topology->mutex);

   return sd;
}

/*
 *-------------------------------------------------------------------------
 *
 * mongoc_topology_get_server_type --
 *
 *      Get the topology type, and the server type for @id, if that server
 *      is present in @description. Otherwise, return false and fill out
 *      the optional @error.
 *
 *      NOTE: this method locks and unlocks @topology's mutex.
 *
 * Returns:
 *      True on success.
 *
 * Side effects:
 *      Fills out optional @error if server not found.
 *
 *-------------------------------------------------------------------------
 */

bool
mongoc_topology_get_server_type (
   mongoc_topology_t *topology,
   uint32_t id,
   mongoc_topology_description_type_t *topology_type /* OUT */,
   mongoc_server_description_type_t *server_type     /* OUT */,
   bson_error_t *error)
{
   mongoc_server_description_t *sd;
   bool ret = false;

   BSON_ASSERT (topology);
   BSON_ASSERT (topology_type);
   BSON_ASSERT (server_type);

   mongoc_mutex_lock (&topology->mutex);

   sd = mongoc_topology_description_server_by_id (&topology->description,
                                                  id,
                                                  error);

   if (sd) {
      *topology_type = topology->description.type;
      *server_type = sd->type;
      ret = true;
   }

   mongoc_mutex_unlock (&topology->mutex);

   return ret;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_request_scan --
 *
 *       Non-locking variant
 *
 *--------------------------------------------------------------------------
 */

static void
_mongoc_topology_request_scan (mongoc_topology_t *topology)
{
   topology->scan_requested = true;

   mongoc_cond_signal (&topology->cond_server);
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_request_scan --
 *
 *       Used from within the driver to request an immediate topology check.
 *
 *       NOTE: this method locks and unlocks @topology's mutex.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_topology_request_scan (mongoc_topology_t *topology)
{
   mongoc_mutex_lock (&topology->mutex);

   _mongoc_topology_request_scan (topology);

   mongoc_mutex_unlock (&topology->mutex);
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_invalidate_server --
 *
 *      Invalidate the given server after receiving a network error in
 *      another part of the client.
 *
 *      NOTE: this method uses @topology's mutex.
 *
 *--------------------------------------------------------------------------
 */
void
mongoc_topology_invalidate_server (mongoc_topology_t  *topology,
                                   uint32_t            id,
                                   const bson_error_t *error)
{
   mongoc_mutex_lock (&topology->mutex);
   mongoc_topology_description_invalidate_server (&topology->description,
                                                  id, error);
   mongoc_mutex_unlock (&topology->mutex);
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_server_timestamp --
 *
 *      Return the topology's scanner's timestamp for the given server,
 *      or -1 if there is no scanner node for the given server.
 *
 *      NOTE: this method uses @topology's mutex.
 *
 * Returns:
 *      Timestamp, or -1
 *
 *--------------------------------------------------------------------------
 */
int64_t
mongoc_topology_server_timestamp (mongoc_topology_t *topology,
                                  uint32_t           id)
{
   mongoc_topology_scanner_node_t *node;
   int64_t timestamp = -1;

   mongoc_mutex_lock (&topology->mutex);

   node = mongoc_topology_scanner_get_node (topology->scanner, id);
   if (node) {
      timestamp = node->timestamp;
   }

   mongoc_mutex_unlock (&topology->mutex);

   return timestamp;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_run_background --
 *
 *       The background topology monitoring thread runs in this loop.
 *
 *       NOTE: this method uses @topology's mutex.
 *
 *--------------------------------------------------------------------------
 */
static
void * _mongoc_topology_run_background (void *data)
{
   mongoc_topology_t *topology;
   int64_t now;
   int64_t last_scan;
   int64_t timeout;
   int64_t force_timeout;
   int r;

   BSON_ASSERT (data);

   last_scan = 0;
   topology = (mongoc_topology_t *)data;
   /* we exit this loop when shutdown_requested, or on error */
   for (;;) {
      /* unlocked after starting a scan or after breaking out of the loop */
      mongoc_mutex_lock (&topology->mutex);

      /* we exit this loop on error, or when we should scan immediately */
      for (;;) {
         if (topology->shutdown_requested) goto DONE;

         now = bson_get_monotonic_time ();

         if (last_scan == 0) {
            /* set up the "last scan" as exactly long enough to force an
             * immediate scan on the first pass */
            last_scan = now - (topology->heartbeat_msec * 1000);
         }

         timeout = topology->heartbeat_msec - ((now - last_scan) / 1000);

         /* if someone's specifically asked for a scan, use a shorter interval */
         if (topology->scan_requested) {
            force_timeout = MONGOC_TOPOLOGY_MIN_HEARTBEAT_FREQUENCY_MS - ((now - last_scan) / 1000);

            timeout = BSON_MIN (timeout, force_timeout);
         }

         /* if we can start scanning, do so immediately */
         if (timeout <= 0) {
            mongoc_topology_scanner_start (topology->scanner,
                                           topology->connect_timeout_msec,
                                           false);
            break;
         } else {
            /* otherwise wait until someone:
             *   o requests a scan
             *   o we time out
             *   o requests a shutdown
             */
            r = mongoc_cond_timedwait (&topology->cond_server, &topology->mutex, timeout);

#ifdef _WIN32
            if (! (r == 0 || r == WSAETIMEDOUT)) {
#else
            if (! (r == 0 || r == ETIMEDOUT)) {
#endif
               /* handle errors */
               goto DONE;
            }

            /* if we timed out, or were woken up, check if it's time to scan
             * again, or bail out */
         }
      }

      topology->scan_requested = false;
      topology->scanning = true;

      /* scanning locks and unlocks the mutex itself until the scan is done */
      mongoc_mutex_unlock (&topology->mutex);

      while (_mongoc_topology_run_scanner (topology,
                                           topology->connect_timeout_msec)) {}

      mongoc_mutex_lock (&topology->mutex);

      /* "retired" nodes can be checked again in the next scan */
      mongoc_topology_scanner_reset (topology->scanner);

      topology->last_scan = bson_get_monotonic_time ();
      topology->scanning = false;
      mongoc_mutex_unlock (&topology->mutex);

      last_scan = bson_get_monotonic_time();
   }

DONE:
   mongoc_mutex_unlock (&topology->mutex);

   return NULL;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_background_thread_start --
 *
 *       Start the topology background thread running. This should only be
 *       called once per pool. If clients are created separately (not
 *       through a pool) the SDAM logic will not be run in a background
 *       thread.
 *
 *       NOTE: this method uses @topology's mutex.
 *
 *--------------------------------------------------------------------------
 */

static void
_mongoc_topology_background_thread_start (mongoc_topology_t *topology)
{
   bool launch_thread = true;

   if (topology->single_threaded) {
      return;
   }

   mongoc_mutex_lock (&topology->mutex);
   if (topology->bg_thread_state != MONGOC_TOPOLOGY_BG_OFF) launch_thread = false;
   topology->bg_thread_state = MONGOC_TOPOLOGY_BG_RUNNING;
   mongoc_mutex_unlock (&topology->mutex);

   if (launch_thread) {
      mongoc_thread_create (&topology->thread, _mongoc_topology_run_background,
                            topology);
   }
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_background_thread_stop --
 *
 *       Stop the topology background thread. Called by the owning pool at
 *       its destruction.
 *
 *       NOTE: this method uses @topology's mutex.
 *
 *--------------------------------------------------------------------------
 */

static void
_mongoc_topology_background_thread_stop (mongoc_topology_t *topology)
{
   bool join_thread = false;

   if (topology->single_threaded) {
      return;
   }

   mongoc_mutex_lock (&topology->mutex);
   if (topology->bg_thread_state == MONGOC_TOPOLOGY_BG_RUNNING) {
      /* if the background thread is running, request a shutdown and signal the
       * thread */
      topology->shutdown_requested = true;
      mongoc_cond_signal (&topology->cond_server);
      topology->bg_thread_state = MONGOC_TOPOLOGY_BG_SHUTTING_DOWN;
      join_thread = true;
   } else if (topology->bg_thread_state == MONGOC_TOPOLOGY_BG_SHUTTING_DOWN) {
      /* if we're mid shutdown, wait until it shuts down */
      while (topology->bg_thread_state != MONGOC_TOPOLOGY_BG_OFF) {
         mongoc_cond_wait (&topology->cond_client, &topology->mutex);
      }
   } else {
      /* nothing to do if it's already off */
   }

   mongoc_mutex_unlock (&topology->mutex);

   if (join_thread) {
      /* if we're joining the thread, wait for it to come back and broadcast
       * all listeners */
      mongoc_thread_join (topology->thread);
      mongoc_cond_broadcast (&topology->cond_client);
   }
}

/*==============================================================*/
/* --- mongoc-topology-description.c */

static void
_mongoc_topology_server_dtor (void *server_,
                              void *ctx_)
{
   mongoc_server_description_destroy ((mongoc_server_description_t *)server_);
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_description_init --
 *
 *       Initialize the given topology description
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
mongoc_topology_description_init (mongoc_topology_description_t     *description,
                                  mongoc_topology_description_type_t type)
{
   ENTRY;

   BSON_ASSERT (description);
   BSON_ASSERT (type == MONGOC_TOPOLOGY_UNKNOWN ||
                        type == MONGOC_TOPOLOGY_SINGLE ||
                        type == MONGOC_TOPOLOGY_RS_NO_PRIMARY);

   memset (description, 0, sizeof (*description));

   description->type = type;
   description->servers = mongoc_set_new(8, _mongoc_topology_server_dtor, NULL);
   description->set_name = NULL;
   description->max_set_version = MONGOC_NO_SET_VERSION;
   description->compatible = true;
   description->compatibility_error = NULL;
   description->stale = true;

   EXIT;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_description_destroy --
 *
 *       Destroy allocated resources within @description
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
mongoc_topology_description_destroy (mongoc_topology_description_t *description)
{
   ENTRY;

   BSON_ASSERT(description);

   mongoc_set_destroy(description->servers);

   if (description->set_name) {
      bson_free (description->set_name);
   }

   if (description->compatibility_error) {
      bson_free (description->compatibility_error);
   }

   EXIT;
}

/* find the primary, then stop iterating */
static bool
_mongoc_topology_description_has_primary_cb (void *item,
                                             void *ctx /* OUT */)
{
   mongoc_server_description_t *server = (mongoc_server_description_t *)item;
   mongoc_server_description_t **primary = (mongoc_server_description_t **)ctx;

   /* TODO should this include MONGOS? */
   if (server->type == MONGOC_SERVER_RS_PRIMARY || server->type == MONGOC_SERVER_STANDALONE) {
      *primary = (mongoc_server_description_t *)item;
      return false;
   }
   return true;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_has_primary --
 *
 *       If topology has a primary, return it.
 *
 * Returns:
 *       A pointer to the primary, or NULL.
 *
 * Side effects:
 *       None
 *
 *--------------------------------------------------------------------------
 */
static mongoc_server_description_t *
_mongoc_topology_description_has_primary (mongoc_topology_description_t *description)
{
   mongoc_server_description_t *primary = NULL;

   mongoc_set_for_each(description->servers, _mongoc_topology_description_has_primary_cb, &primary);

   return primary;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_later_election --
 *
 *       Check if we've seen a more recent election in the replica set
 *       than this server has.
 *
 * Returns:
 *       True if the topology description's max replica set version plus
 *       election id is later than the server description's.
 *
 * Side effects:
 *       None
 *
 *--------------------------------------------------------------------------
 */
static bool
_mongoc_topology_description_later_election (mongoc_topology_description_t *td,
                                             mongoc_server_description_t   *sd)
{
   /* initially max_set_version is -1 and max_election_id is zeroed */
   return td->max_set_version > sd->set_version ||
      (td->max_set_version == sd->set_version &&
         bson_oid_compare (&td->max_election_id, &sd->election_id) > 0);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_set_max_set_version --
 *
 *       Remember that we've seen a new replica set version. Unconditionally
 *       sets td->set_version to sd->set_version.
 *
 *--------------------------------------------------------------------------
 */
static void
_mongoc_topology_description_set_max_set_version (
   mongoc_topology_description_t *td,
   mongoc_server_description_t   *sd)
{
   td->max_set_version = sd->set_version;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_set_max_election_id --
 *
 *       Remember that we've seen a new election id. Unconditionally sets
 *       td->max_election_id to sd->election_id.
 *
 *--------------------------------------------------------------------------
 */
static void
_mongoc_topology_description_set_max_election_id (
   mongoc_topology_description_t *td,
   mongoc_server_description_t   *sd)
{
   bson_oid_copy (&sd->election_id, &td->max_election_id);
}

static bool
_mongoc_topology_description_server_is_candidate (
   mongoc_server_description_type_t   desc_type,
   mongoc_read_mode_t                 read_mode,
   mongoc_topology_description_type_t topology_type)
{
   switch ((int)topology_type) {
   case MONGOC_TOPOLOGY_SINGLE:
      switch ((int)desc_type) {
      case MONGOC_SERVER_STANDALONE:
         return true;
      default:
         return false;
      }

   case MONGOC_TOPOLOGY_RS_NO_PRIMARY:
   case MONGOC_TOPOLOGY_RS_WITH_PRIMARY:
      switch ((int)read_mode) {
      case MONGOC_READ_PRIMARY:
         switch ((int)desc_type) {
         case MONGOC_SERVER_POSSIBLE_PRIMARY:
         case MONGOC_SERVER_RS_PRIMARY:
            return true;
         default:
            return false;
         }
      case MONGOC_READ_SECONDARY:
         switch ((int)desc_type) {
         case MONGOC_SERVER_RS_SECONDARY:
            return true;
         default:
            return false;
         }
      default:
         switch ((int)desc_type) {
         case MONGOC_SERVER_POSSIBLE_PRIMARY:
         case MONGOC_SERVER_RS_PRIMARY:
         case MONGOC_SERVER_RS_SECONDARY:
            return true;
         default:
            return false;
         }
      }

   case MONGOC_TOPOLOGY_SHARDED:
      switch ((int)desc_type) {
      case MONGOC_SERVER_MONGOS:
         return true;
      default:
         return false;
      }
   default:
      return false;
   }
}

typedef struct _mongoc_suitable_data_t
{
   mongoc_read_mode_t                 read_mode;
   mongoc_topology_description_type_t topology_type;
   mongoc_server_description_t       *primary; /* OUT */
   mongoc_server_description_t      **candidates; /* OUT */
   size_t                             candidates_len; /* OUT */
   bool                               has_secondary; /* OUT */
} mongoc_suitable_data_t;

static bool
_mongoc_replica_set_read_suitable_cb (void *item,
                                      void *ctx)
{
   mongoc_server_description_t *server = (mongoc_server_description_t *)item;
   mongoc_suitable_data_t *data = (mongoc_suitable_data_t *)ctx;

   if (_mongoc_topology_description_server_is_candidate (server->type,
                                                         data->read_mode,
                                                         data->topology_type)) {

      if (server->type == MONGOC_SERVER_RS_PRIMARY) {
         data->primary = server;

         if (data->read_mode == MONGOC_READ_PRIMARY ||
             data->read_mode == MONGOC_READ_PRIMARY_PREFERRED) {
            /* we want a primary and we have one, done! */
            return false;
         }
      }

      if (server->type == MONGOC_SERVER_RS_SECONDARY) {
         data->has_secondary = true;
      }

      /* add to our candidates */
      data->candidates[data->candidates_len++] = server;
   }
   return true;
}

/* if any mongos are candidates, add them to the candidates array */
static bool
_mongoc_find_suitable_mongos_cb (void *item,
                                 void *ctx)
{
   mongoc_server_description_t *server = (mongoc_server_description_t *)item;
   mongoc_suitable_data_t *data = (mongoc_suitable_data_t *)ctx;

   if (_mongoc_topology_description_server_is_candidate (server->type,
                                                         data->read_mode,
                                                         data->topology_type)) {
      data->candidates[data->candidates_len++] = server;
   }
   return true;
}

/*
 *-------------------------------------------------------------------------
 *
 * mongoc_topology_description_suitable_servers --
 *
 *       Return an array of suitable server descriptions for this
 *       operation and read preference.
 *
 *       NOTE: this method should only be called while holding the mutex on
 *       the owning topology object.
 *
 * Returns:
 *       Array of server descriptions, or NULL upon failure.
 *
 * Side effects:
 *       None.
 *
 *-------------------------------------------------------------------------
 */

void
mongoc_topology_description_suitable_servers (
   mongoc_array_t                *set, /* OUT */
   mongoc_ss_optype_t             optype,
   mongoc_topology_description_t *topology,
   const mongoc_read_prefs_t     *read_pref,
   size_t                         local_threshold_ms)
{
   mongoc_suitable_data_t data;
   mongoc_server_description_t **candidates;
   mongoc_server_description_t *server;
   int64_t nearest = -1;
   int i;
   mongoc_read_mode_t read_mode = mongoc_read_prefs_get_mode(read_pref);

   candidates = (mongoc_server_description_t **)bson_malloc0(sizeof(*candidates) * topology->servers->items_len);

   data.read_mode = read_mode;
   data.topology_type = topology->type;
   data.primary = NULL;
   data.candidates = candidates;
   data.candidates_len = 0;
   data.has_secondary = false;

   /* Single server --
    * Either it is suitable or it isn't */
   if (topology->type == MONGOC_TOPOLOGY_SINGLE) {
      server = (mongoc_server_description_t *)mongoc_set_get_item (topology->servers, 0);
      if (_mongoc_topology_description_server_is_candidate (server->type, read_mode, topology->type)) {
         _mongoc_array_append_val (set, server);
      }
      goto DONE;
   }

   /* Replica sets --
    * Find suitable servers based on read mode */
   if (topology->type == MONGOC_TOPOLOGY_RS_NO_PRIMARY ||
       topology->type == MONGOC_TOPOLOGY_RS_WITH_PRIMARY) {

      if (optype == MONGOC_SS_READ) {

         mongoc_set_for_each(topology->servers, _mongoc_replica_set_read_suitable_cb, &data);

         /* if we have a primary, it's a candidate, for some read modes we are done */
         if (read_mode == MONGOC_READ_PRIMARY || read_mode == MONGOC_READ_PRIMARY_PREFERRED) {
            if (data.primary) {
               _mongoc_array_append_val (set, data.primary);
               goto DONE;
            }
         }

         if (! mongoc_server_description_filter_eligible (data.candidates, data.candidates_len, read_pref)) {
            if (read_mode == MONGOC_READ_NEAREST) {
               goto DONE;
            } else {
               data.has_secondary = false;
            }
         }

         if (data.has_secondary &&
             (read_mode == MONGOC_READ_SECONDARY || read_mode == MONGOC_READ_SECONDARY_PREFERRED)) {
            /* secondary or secondary preferred and we have one. */

            for (i = 0; i < (int)data.candidates_len; i++) {
               if (candidates[i] && candidates[i]->type == MONGOC_SERVER_RS_PRIMARY) {
                  candidates[i] = NULL;
               }
            }
         } else if (read_mode == MONGOC_READ_SECONDARY_PREFERRED && data.primary) {
            /* secondary preferred, but only the one primary is a candidate */
            _mongoc_array_append_val (set, data.primary);
            goto DONE;
         }

      } else if (topology->type == MONGOC_TOPOLOGY_RS_WITH_PRIMARY) {
         /* includes optype == MONGOC_SS_WRITE as the exclusion of the above if */
         mongoc_set_for_each(topology->servers, _mongoc_topology_description_has_primary_cb,
                             &data.primary);
         if (data.primary) {
            _mongoc_array_append_val (set, data.primary);
            goto DONE;
         }
      }
   }

   /* Sharded clusters --
    * All candidates in the latency window are suitable */
   if (topology->type == MONGOC_TOPOLOGY_SHARDED) {
      mongoc_set_for_each (topology->servers, _mongoc_find_suitable_mongos_cb, &data);
   }

   /* Ways to get here:
    *   - secondary read
    *   - secondary preferred read
    *   - primary_preferred and no primary read
    *   - sharded anything
    * Find the nearest, then select within the window */

   for (i = 0; i < (int)data.candidates_len; i++) {
      if (candidates[i] && (nearest == -1 || nearest > candidates[i]->round_trip_time)) {
         nearest = candidates[i]->round_trip_time;
      }
   }

   for (i = 0; i < (int)data.candidates_len; i++) {
      if (candidates[i] && (candidates[i]->round_trip_time <= (int64_t)(nearest + local_threshold_ms))) {
         _mongoc_array_append_val (set, candidates[i]);
      }
   }

DONE:

   bson_free (candidates);

   return;
}


/*
 *-------------------------------------------------------------------------
 *
 * mongoc_topology_description_select --
 *
 *      Return a server description of a node that is appropriate for
 *      the given read preference and operation type.
 *
 *      NOTE: this method simply attempts to select a server from the
 *      current topology, it does not retry or trigger topology checks.
 *
 *      NOTE: this method should only be called while holding the mutex on
 *      the owning topology object.
 *
 * Returns:
 *      Selected server description, or NULL upon failure.
 *
 * Side effects:
 *      None.
 *
 *-------------------------------------------------------------------------
 */

mongoc_server_description_t *
mongoc_topology_description_select (mongoc_topology_description_t *topology,
                                    mongoc_ss_optype_t             optype,
                                    const mongoc_read_prefs_t     *read_pref,
                                    int64_t                        local_threshold_ms)
{
   mongoc_array_t suitable_servers;
   mongoc_server_description_t *sd = NULL;

   ENTRY;

   if (!topology->compatible) {
      /* TODO, should we return an error object here,
         or just treat as a case where there are no suitable servers? */
      RETURN(NULL);
   }

   if (topology->type == MONGOC_TOPOLOGY_SINGLE) {
      sd = (mongoc_server_description_t *)mongoc_set_get_item (topology->servers, 0);

      if (sd->has_is_master) {
         RETURN(sd);
      } else {
         RETURN(NULL);
      }
   }

   _mongoc_array_init(&suitable_servers, sizeof(mongoc_server_description_t *));

   mongoc_topology_description_suitable_servers(&suitable_servers, optype,
                                                 topology, read_pref, local_threshold_ms);
   if (suitable_servers.len != 0) {
      sd = _mongoc_array_index(&suitable_servers, mongoc_server_description_t*,
                               rand() % suitable_servers.len);
   }

   _mongoc_array_destroy (&suitable_servers);

   RETURN(sd);
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_description_server_by_id --
 *
 *       Get the server description for @id, if that server is present
 *       in @description. Otherwise, return NULL and fill out optional
 *       @error.
 *
 *       NOTE: In most cases, caller should create a duplicate of the
 *       returned server description. Caller should hold the mutex on the
 *       owning topology object while calling this method and while using
 *       the returned reference.
 *
 * Returns:
 *       A mongoc_server_description_t *, or NULL.
 *
 * Side effects:
 *      Fills out optional @error if server not found.
 *
 *--------------------------------------------------------------------------
 */

mongoc_server_description_t *
mongoc_topology_description_server_by_id (mongoc_topology_description_t *description,
                                          uint32_t id,
                                          bson_error_t *error)
{
   mongoc_server_description_t *sd;

   BSON_ASSERT (description);

   sd = (mongoc_server_description_t *)mongoc_set_get(description->servers, id);
   if (!sd) {
      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_NOT_ESTABLISHED,
                      _("Could not find description for node %u"), id);
   }

   return sd;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_remove_server --
 *
 *       If present, remove this server from this topology description.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Removes the server description from topology and destroys it.
 *
 *--------------------------------------------------------------------------
 */
static void
_mongoc_topology_description_remove_server (mongoc_topology_description_t *description,
                                            mongoc_server_description_t   *server)
{
   BSON_ASSERT (description);
   BSON_ASSERT (server);

   mongoc_set_rm(description->servers, server->id);
}

typedef struct _mongoc_address_and_id_t {
   const char *address; /* IN */
   bool found; /* OUT */
   uint32_t id; /* OUT */
} mongoc_address_and_id_t;

/* find the given server and stop iterating */
static bool
_mongoc_topology_description_has_server_cb (void *item,
                                            void *ctx /* IN - OUT */)
{
   mongoc_server_description_t *server = (mongoc_server_description_t *)item;
   mongoc_address_and_id_t *data = (mongoc_address_and_id_t *)ctx;

   if (strcasecmp (data->address, server->connection_address) == 0) {
      data->found = true;
      data->id = server->id;
      return false;
   }
   return true;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_has_set_version --
 *
 *       Whether @topology's max replica set version has been set.
 *
 * Returns:
 *       True if the max setVersion was ever set.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
static bool
_mongoc_topology_description_has_set_version (mongoc_topology_description_t *td)
{
   return td->max_set_version != MONGOC_NO_SET_VERSION;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_topology_has_server --
 *
 *       Return true if @server is in @topology. If so, place its id in
 *       @id if given.
 *
 * Returns:
 *       True if server is in topology, false otherwise.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
static bool
_mongoc_topology_description_has_server (mongoc_topology_description_t *description,
                                         const char                    *address,
                                         uint32_t                      *id /* OUT */)
{
   mongoc_address_and_id_t data;

   BSON_ASSERT (description);
   BSON_ASSERT (address);

   data.address = address;
   data.found = false;
   mongoc_set_for_each (description->servers, _mongoc_topology_description_has_server_cb, &data);

   if (data.found && id) {
      *id = data.id;
   }

   return data.found;
}

typedef struct _mongoc_address_and_type_t
{
   const char *address;
   mongoc_server_description_type_t type;
} mongoc_address_and_type_t;

static bool
_mongoc_label_unknown_member_cb (void *item,
                                 void *ctx)
{
   mongoc_server_description_t *server = (mongoc_server_description_t *)item;
   mongoc_address_and_type_t *data = (mongoc_address_and_type_t *)ctx;

   if (strcasecmp (server->connection_address, data->address) == 0 &&
       server->type == MONGOC_SERVER_UNKNOWN) {
      mongoc_server_description_set_state(server, data->type);
      return false;
   }
   return true;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_label_unknown_member --
 *
 *       Find the server description with the given @address and if its
 *       type is UNKNOWN, set its type to @type.
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
_mongoc_topology_description_label_unknown_member (mongoc_topology_description_t *description,
                                                   const char *address,
                                                   mongoc_server_description_type_t type)
{
   mongoc_address_and_type_t data;

   BSON_ASSERT (description);
   BSON_ASSERT (address);

   data.type = type;
   data.address = address;

   mongoc_set_for_each(description->servers, _mongoc_label_unknown_member_cb, &data);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_set_state --
 *
 *       Change the state of this cluster and unblock things waiting
 *       on a change of topology type.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Unblocks anything waiting on this description to change states.
 *
 *--------------------------------------------------------------------------
 */
static void
_mongoc_topology_description_set_state (mongoc_topology_description_t *description,
                                        mongoc_topology_description_type_t type)
{
   description->type = type;
}


static void
_update_rs_type (mongoc_topology_description_t *topology)
{
   if (_mongoc_topology_description_has_primary(topology)) {
      _mongoc_topology_description_set_state(topology, MONGOC_TOPOLOGY_RS_WITH_PRIMARY);
   }
   else {
      _mongoc_topology_description_set_state(topology, MONGOC_TOPOLOGY_RS_NO_PRIMARY);
   }
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_check_if_has_primary --
 *
 *       If there is a primary in topology, set topology
 *       type to RS_WITH_PRIMARY, otherwise set it to
 *       RS_NO_PRIMARY.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Changes the topology type.
 *
 *--------------------------------------------------------------------------
 */
static void
_mongoc_topology_description_check_if_has_primary (mongoc_topology_description_t *topology,
                                                   mongoc_server_description_t   *server)
{
   _update_rs_type (topology);
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_description_invalidate_server --
 *
 *      Invalidate a server if a network error occurred while using it in
 *      another part of the client. Server description is set to type
 *      UNKNOWN, the error is recorded, and other parameters are reset to
 *      defaults.
 *
 *      NOTE: this method should only be called while holding the mutex on
 *      the owning topology object.
 *
 *--------------------------------------------------------------------------
 */
void
mongoc_topology_description_invalidate_server (mongoc_topology_description_t *topology,
                                               uint32_t                       id,
                                               const bson_error_t            *error)
{
   mongoc_server_description_t *sd;

   sd = mongoc_topology_description_server_by_id (topology, id, NULL);
   mongoc_topology_description_handle_ismaster (topology, sd, NULL, 0, NULL);

   if (error) {
      memcpy (&sd->error, error, sizeof *error);
   }

   return;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_description_add_server --
 *
 *       Add the specified server to the cluster topology if it is not
 *       already a member. If @id, place its id in @id.
 *
 *       NOTE: this method should only be called while holding the mutex on
 *       the owning topology object.
 *
 * Return:
 *       True if the server was added or already existed in the topology,
 *       false if an error occurred.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
bool
mongoc_topology_description_add_server (mongoc_topology_description_t *topology,
                                        const char                    *server,
                                        uint32_t                      *id /* OUT */)
{
   uint32_t server_id;
   mongoc_server_description_t *description;

   BSON_ASSERT (topology);
   BSON_ASSERT (server);

   if (!_mongoc_topology_description_has_server(topology, server, &server_id)){

      /* TODO this might not be an accurate count in all cases */
      server_id = ++topology->max_server_id;

      description = (mongoc_server_description_t *)bson_malloc0(sizeof *description);
      mongoc_server_description_init(description, server, server_id);

      mongoc_set_add(topology->servers, server_id, description);
   }

   if (id) {
      *id = server_id;
   }

   return true;
}


static void
_mongoc_topology_description_add_new_servers (
   mongoc_topology_description_t *topology,
   mongoc_server_description_t *server)
{
   bson_iter_t member_iter;
   const bson_t *rs_members[3];
   int i;

   rs_members[0] = &server->hosts;
   rs_members[1] = &server->arbiters;
   rs_members[2] = &server->passives;

   for (i = 0; i < 3; i++) {
      bson_iter_init (&member_iter, rs_members[i]);

      while (bson_iter_next (&member_iter)) {
         mongoc_topology_description_add_server(topology, bson_iter_utf8(&member_iter, NULL), NULL);
      }
   }
}

typedef struct _mongoc_primary_and_topology_t {
   mongoc_topology_description_t *topology;
   mongoc_server_description_t *primary;
} mongoc_primary_and_topology_t;

/* invalidate old primaries */
static bool
_mongoc_topology_description_invalidate_primaries_cb (void *item,
                                                      void *ctx)
{
   mongoc_server_description_t *server = (mongoc_server_description_t *)item;
   mongoc_primary_and_topology_t *data = (mongoc_primary_and_topology_t *)ctx;

   if (server->id != data->primary->id &&
       server->type == MONGOC_SERVER_RS_PRIMARY) {
      mongoc_server_description_set_state (server, MONGOC_SERVER_UNKNOWN);
      mongoc_server_description_set_set_version (server, MONGOC_NO_SET_VERSION);
      mongoc_server_description_set_election_id (server, NULL);
   }
   return true;
}


/* Remove and destroy all replica set members not in primary's hosts lists */
static void
_mongoc_topology_description_remove_unreported_servers (
   mongoc_topology_description_t *topology,
   mongoc_server_description_t *primary)
{
   mongoc_array_t to_remove;
   int i;
   mongoc_server_description_t *member;
   const char *address;

   _mongoc_array_init (&to_remove,
                       sizeof (mongoc_server_description_t *));

   /* Accumulate servers to be removed - do this before calling
    * _mongoc_topology_description_remove_server, which could call
    * mongoc_server_description_cleanup on the primary itself if it
    * doesn't report its own connection_address in its hosts list.
    * See hosts_differ_from_seeds.json */
   for (i = 0; i < (int)topology->servers->items_len; i++) {
      member = (mongoc_server_description_t *)mongoc_set_get_item (topology->servers, i);
      address = member->connection_address;
      if (!mongoc_server_description_has_rs_member(primary, address)) {
         _mongoc_array_append_val (&to_remove, member);
      }
   }

   /* now it's safe to call _mongoc_topology_description_remove_server,
    * even on the primary */
   for (i = 0; i < (int)to_remove.len; i++) {
      member = _mongoc_array_index (
         &to_remove, mongoc_server_description_t *, i);

      _mongoc_topology_description_remove_server (topology, member);
   }

   _mongoc_array_destroy (&to_remove);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_matches_me --
 *
 *       Server Discovery And Monitoring Spec: "Removal from the topology of
 *       seed list members where the "me" property does not match the address
 *       used to connect prevents clients from being able to select a server,
 *       only to fail to re-select that server once the primary has responded.
 *
 * Returns:
 *       True if "me" matches "connection_address".
 *
 * Side Effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */
static bool
_mongoc_topology_description_matches_me (mongoc_server_description_t *server)
{
   BSON_ASSERT (server->connection_address);

   if (!server->me) {
      /* "me" is unknown: consider it a match */
      return true;
   }

   return strcasecmp (server->connection_address, server->me) == 0;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_update_rs_from_primary --
 *
 *       First, determine that this is really the primary:
 *          -If this node isn't in the cluster, do nothing.
 *          -If the cluster's set name is null, set it to node's set name.
 *           Otherwise if the cluster's set name is different from node's,
 *           we found a rogue primary, so remove it from the cluster and
 *           check the cluster for a primary, then return.
 *          -If any of the members of cluster reports an address different
 *           from node's, node cannot be the primary.
 *       Now that we know this is the primary:
 *          -If any hosts, passives, or arbiters in node's description aren't
 *           in the cluster, add them as UNKNOWN servers.
 *          -If the cluster has any servers that aren't in node's description,
 *           remove and destroy them.
 *       Finally, check the cluster for the new primary.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Changes to the cluster, possible removal of cluster nodes.
 *
 *--------------------------------------------------------------------------
 */
static void
_mongoc_topology_description_update_rs_from_primary (mongoc_topology_description_t *topology,
                                                     mongoc_server_description_t   *server)
{
   mongoc_primary_and_topology_t data;
   bson_error_t error;

   BSON_ASSERT (topology);
   BSON_ASSERT (server);

   if (!_mongoc_topology_description_has_server(topology, server->connection_address, NULL)) return;

   /* If server->set_name was null this function wouldn't be called from
    * mongoc_server_description_handle_ismaster(). static code analyzers however
    * don't know that so we check for it explicitly. */
   if (server->set_name) {
      /* 'Server' can only be the primary if it has the right rs name  */

      if (!topology->set_name) {
         topology->set_name = bson_strdup (server->set_name);
      }
      else if (strcmp(topology->set_name, server->set_name) != 0) {
         _mongoc_topology_description_remove_server(topology, server);
         _update_rs_type (topology);
         return;
      }
   }

   if (mongoc_server_description_has_set_version (server) &&
       mongoc_server_description_has_election_id (server)) {
      /* Server Discovery And Monitoring Spec: "The client remembers the
       * greatest electionId reported by a primary, and distrusts primaries
       * with lesser electionIds. This prevents the client from oscillating
       * between the old and new primary during a split-brain period."
       */
      if (_mongoc_topology_description_later_election (topology, server)) {
         bson_set_error (&error,
                         MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_CONNECT,
                         _("member's setVersion or electionId is stale"));
         mongoc_topology_description_invalidate_server (topology, server->id,
                                                        &error);
         _update_rs_type (topology);
         return;
      }

      /* server's electionId >= topology's max electionId */
      _mongoc_topology_description_set_max_election_id (topology, server);
   }

   if (mongoc_server_description_has_set_version (server) &&
         (! _mongoc_topology_description_has_set_version (topology) ||
            server->set_version > topology->max_set_version)) {
      _mongoc_topology_description_set_max_set_version (topology, server);
   }

   /* 'Server' is the primary! Invalidate other primaries if found */
   data.primary = server;
   data.topology = topology;
   mongoc_set_for_each(topology->servers, _mongoc_topology_description_invalidate_primaries_cb, &data);

   /* Add to topology description any new servers primary knows about */
   _mongoc_topology_description_add_new_servers (topology, server);

   /* Remove from topology description any servers primary doesn't know about */
   _mongoc_topology_description_remove_unreported_servers (topology, server);

   /* Finally, set topology type */
   _update_rs_type (topology);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_update_rs_without_primary --
 *
 *       Update cluster's information when there is no primary.
 *
 * Returns:
 *       None.
 *
 * Side Effects:
 *       Alters cluster state, may remove node from cluster.
 *
 *--------------------------------------------------------------------------
 */
static void
_mongoc_topology_description_update_rs_without_primary (mongoc_topology_description_t *topology,
                                                        mongoc_server_description_t   *server)
{
   BSON_ASSERT (topology);
   BSON_ASSERT (server);

   if (!_mongoc_topology_description_has_server(topology, server->connection_address, NULL)) {
      return;
   }

   /* make sure we're talking about the same replica set */
   if (server->set_name) {
      if (!topology->set_name) {
         topology->set_name = bson_strdup(server->set_name);
      }
      else if (strcmp(topology->set_name, server->set_name)!= 0) {
         _mongoc_topology_description_remove_server(topology, server);
         return;
      }
   }

   /* Add new servers that this replica set member knows about */
   _mongoc_topology_description_add_new_servers (topology, server);

   if (!_mongoc_topology_description_matches_me (server)) {
      _mongoc_topology_description_remove_server(topology, server);
      return;
   }

   /* If this server thinks there is a primary, label it POSSIBLE_PRIMARY */
   if (server->current_primary) {
      _mongoc_topology_description_label_unknown_member(topology,
                                                        server->current_primary,
                                                        MONGOC_SERVER_POSSIBLE_PRIMARY);
   }
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_update_rs_with_primary_from_member --
 *
 *       Update cluster's information when there is a primary, but the
 *       update is coming from another replica set member.
 *
 * Returns:
 *       None.
 *
 * Side Effects:
 *       Alters cluster state.
 *
 *--------------------------------------------------------------------------
 */
static void
_mongoc_topology_description_update_rs_with_primary_from_member (mongoc_topology_description_t *topology,
                                                                 mongoc_server_description_t   *server)
{
   BSON_ASSERT (topology);
   BSON_ASSERT (server);

   if (!_mongoc_topology_description_has_server(topology, server->connection_address, NULL)) {
      return;
   }

   /* set_name should never be null here */
   if (strcmp(topology->set_name, server->set_name) != 0) {
      _mongoc_topology_description_remove_server(topology, server);
      _update_rs_type (topology);
      return;
   }

   if (!_mongoc_topology_description_matches_me (server)) {
      _mongoc_topology_description_remove_server(topology, server);
      return;
   }

   /* If there is no primary, label server's current_primary as the POSSIBLE_PRIMARY */
   if (!_mongoc_topology_description_has_primary(topology) && server->current_primary) {
      _mongoc_topology_description_set_state(topology, MONGOC_TOPOLOGY_RS_NO_PRIMARY);
      _mongoc_topology_description_label_unknown_member(topology,
                                                        server->current_primary,
                                                        MONGOC_SERVER_POSSIBLE_PRIMARY);
   }
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_set_topology_type_to_sharded --
 *
 *       Sets topology's type to SHARDED.
 *
 * Returns:
 *       None
 *
 * Side effects:
 *       Alter's topology's type
 *
 *--------------------------------------------------------------------------
 */
static void
_mongoc_topology_description_set_topology_type_to_sharded (mongoc_topology_description_t *topology,
                                                           mongoc_server_description_t   *server)
{
   _mongoc_topology_description_set_state(topology, MONGOC_TOPOLOGY_SHARDED);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_transition_unknown_to_rs_no_primary --
 *
 *       Encapsulates transition from cluster state UNKNOWN to
 *       RS_NO_PRIMARY. Sets the type to RS_NO_PRIMARY,
 *       then updates the replica set accordingly.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Changes topology state.
 *
 *--------------------------------------------------------------------------
 */
static void
_mongoc_topology_description_transition_unknown_to_rs_no_primary (mongoc_topology_description_t *topology,
                                                                  mongoc_server_description_t   *server)
{
   _mongoc_topology_description_set_state(topology, MONGOC_TOPOLOGY_RS_NO_PRIMARY);
   _mongoc_topology_description_update_rs_without_primary(topology, server);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_remove_and_check_primary --
 *
 *       Remove the server and check if the topology still has a primary.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Removes server from topology and destroys it.
 *
 *--------------------------------------------------------------------------
 */
static void
_mongoc_topology_description_remove_and_check_primary (mongoc_topology_description_t *topology,
                                                       mongoc_server_description_t   *server)
{
   _mongoc_topology_description_remove_server(topology, server);
   _update_rs_type (topology);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_topology_description_update_unknown_with_standalone --
 *
 *       If the cluster doesn't contain this server, do nothing.
 *       Otherwise, if the topology only has one seed, change its
 *       type to SINGLE. If the topology has multiple seeds, it does not
 *       include us, so remove this server and destroy it.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Changes the topology type, might remove server from topology.
 *
 *--------------------------------------------------------------------------
 */
static void
_mongoc_topology_description_update_unknown_with_standalone (mongoc_topology_description_t *topology,
                                                             mongoc_server_description_t   *server)
{
   BSON_ASSERT (topology);
   BSON_ASSERT (server);

   if (!_mongoc_topology_description_has_server(topology, server->connection_address, NULL)) return;

   if (topology->servers->items_len > 1) {
      /* This cluster contains other servers, it cannot be a standalone. */
      _mongoc_topology_description_remove_server(topology, server);
   } else {
      _mongoc_topology_description_set_state(topology, MONGOC_TOPOLOGY_SINGLE);
   }
}

/*
 *--------------------------------------------------------------------------
 *
 *  This table implements the 'ToplogyType' table outlined in the Server
 *  Discovery and Monitoring spec. Each row represents a server type,
 *  and each column represents the topology type. Given a current topology
 *  type T and a newly-observed server type S, use the function at
 *  state_transions[S][T] to transition to a new state.
 *
 *  Rows should be read like so:
 *  { server type for this row
 *     UNKNOWN,
 *     SHARDED,
 *     RS_NO_PRIMARY,
 *     RS_WITH_PRIMARY
 *  }
 *
 *--------------------------------------------------------------------------
 */

typedef void (*transition_t)(mongoc_topology_description_t *topology,
                             mongoc_server_description_t   *server);

transition_t
gSDAMTransitionTable[MONGOC_SERVER_DESCRIPTION_TYPES][MONGOC_TOPOLOGY_DESCRIPTION_TYPES] = {
   { /* UNKNOWN */
      NULL, /* MONGOC_TOPOLOGY_UNKNOWN */
      NULL, /* MONGOC_TOPOLOGY_SHARDED */
      NULL, /* MONGOC_TOPOLOGY_RS_NO_PRIMARY */
      _mongoc_topology_description_check_if_has_primary /* MONGOC_TOPOLOGY_RS_WITH_PRIMARY */
   },
   { /* STANDALONE */
      _mongoc_topology_description_update_unknown_with_standalone,
      _mongoc_topology_description_remove_server,
      _mongoc_topology_description_remove_server,
      _mongoc_topology_description_remove_and_check_primary
   },
   { /* MONGOS */
      _mongoc_topology_description_set_topology_type_to_sharded,
      NULL,
      _mongoc_topology_description_remove_server,
      _mongoc_topology_description_remove_and_check_primary
   },
   { /* POSSIBLE_PRIMARY */
      NULL,
      NULL,
      NULL,
      NULL
   },
   { /* PRIMARY */
      _mongoc_topology_description_update_rs_from_primary,
      _mongoc_topology_description_remove_server,
      _mongoc_topology_description_update_rs_from_primary,
      _mongoc_topology_description_update_rs_from_primary
   },
   { /* SECONDARY */
      _mongoc_topology_description_transition_unknown_to_rs_no_primary,
      _mongoc_topology_description_remove_server,
      _mongoc_topology_description_update_rs_without_primary,
      _mongoc_topology_description_update_rs_with_primary_from_member
   },
   { /* ARBITER */
      _mongoc_topology_description_transition_unknown_to_rs_no_primary,
      _mongoc_topology_description_remove_server,
      _mongoc_topology_description_update_rs_without_primary,
      _mongoc_topology_description_update_rs_with_primary_from_member
   },
   { /* RS_OTHER */
      _mongoc_topology_description_transition_unknown_to_rs_no_primary,
      _mongoc_topology_description_remove_server,
      _mongoc_topology_description_update_rs_without_primary,
      _mongoc_topology_description_update_rs_with_primary_from_member
   },
   { /* RS_GHOST */
      NULL,
      _mongoc_topology_description_remove_server,
      NULL,
      _mongoc_topology_description_check_if_has_primary
   }
};

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_description_handle_ismaster --
 *
 *      Handle an ismaster. This is called by the background SDAM process,
 *      and by client when invalidating servers.
 *
 *      NOTE: this method should only be called while holding the mutex on
 *      the owning topology object.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_topology_description_handle_ismaster (
   mongoc_topology_description_t *topology,
   mongoc_server_description_t   *sd,
   const bson_t                  *ismaster_response,
   int64_t                        rtt_msec,
   bson_error_t                  *error)
{
   BSON_ASSERT (topology);
   BSON_ASSERT (sd);

   if (!_mongoc_topology_description_has_server (topology,
                                                 sd->connection_address,
                                                 NULL)) {
      MONGOC_DEBUG("Couldn't find %s in Topology Description", sd->connection_address);
      return;
   }

   mongoc_server_description_handle_ismaster (sd, ismaster_response, rtt_msec,
                                              error);

   if (gSDAMTransitionTable[sd->type][topology->type]) {
      TRACE("Transitioning to %d for %d", topology->type, sd->type);
      gSDAMTransitionTable[sd->type][topology->type] (topology, sd);
   } else {
      TRACE("No transition entry to %d for %d", topology->type, sd->type);
   }
}

/*==============================================================*/
/* --- mongoc-topology-scanner.c */

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "topology_scanner"

static void
mongoc_topology_scanner_ismaster_handler (mongoc_async_cmd_result_t async_status,
                                          const bson_t             *ismaster_response,
                                          int64_t                   rtt_msec,
                                          void                     *data,
                                          bson_error_t             *error);

mongoc_topology_scanner_t *
mongoc_topology_scanner_new (const mongoc_uri_t          *uri,
                             mongoc_topology_scanner_cb_t cb,
                             void                        *data)
{
   mongoc_topology_scanner_t *ts = (mongoc_topology_scanner_t *)bson_malloc0 (sizeof (*ts));

   ts->async = mongoc_async_new ();
   bson_init (&ts->ismaster_cmd);
   BSON_APPEND_INT32 (&ts->ismaster_cmd, "isMaster", 1);

   ts->cb = cb;
   ts->cb_data = data;
   ts->uri = uri;

   return ts;
}

#ifdef MONGOC_ENABLE_SSL
void
mongoc_topology_scanner_set_ssl_opts (mongoc_topology_scanner_t *ts,
                                      mongoc_ssl_opt_t          *opts)
{
   ts->ssl_opts = opts;
   ts->setup = mongoc_async_cmd_tls_setup;
}
#endif

void
mongoc_topology_scanner_set_stream_initiator (mongoc_topology_scanner_t *ts,
                                              mongoc_stream_initiator_t  si,
                                              void                      *ctx)
{
   ts->initiator = si;
   ts->initiator_context = ctx;
   ts->setup = NULL;
}

void
mongoc_topology_scanner_destroy (mongoc_topology_scanner_t *ts)
{
   mongoc_topology_scanner_node_t *ele, *tmp;

   DL_FOREACH_SAFE (ts->nodes, ele, tmp) {
      mongoc_topology_scanner_node_destroy (ele, false);
   }

   mongoc_async_destroy (ts->async);
   bson_destroy (&ts->ismaster_cmd);

   bson_free (ts);
}

mongoc_topology_scanner_node_t *
mongoc_topology_scanner_add (mongoc_topology_scanner_t *ts,
                             const mongoc_host_list_t  *host,
                             uint32_t                   id)
{
   mongoc_topology_scanner_node_t *node;

   node = (mongoc_topology_scanner_node_t *) bson_malloc0 (sizeof (*node));

   memcpy (&node->host, host, sizeof (*host));

   node->id = id;
   node->ts = ts;
   node->last_failed = -1;

   DL_APPEND(ts->nodes, node);

   return node;
}

void
mongoc_topology_scanner_add_and_scan (mongoc_topology_scanner_t *ts,
                                      const mongoc_host_list_t  *host,
                                      uint32_t                   id,
                                      int64_t                    timeout_msec)
{
   mongoc_topology_scanner_node_t *node;

   BSON_ASSERT (timeout_msec < INT32_MAX);

   node = mongoc_topology_scanner_add (ts, host, id);

   /* begin non-blocking connection, don't wait for success */
   if (node && mongoc_topology_scanner_node_setup (node, &node->last_error)) {
      node->cmd = mongoc_async_cmd (
         ts->async, node->stream, ts->setup,
         node->host.host, "admin",
         &ts->ismaster_cmd,
         &mongoc_topology_scanner_ismaster_handler,
         node, (int32_t) timeout_msec);
   }

   /* if setup fails the node stays in the scanner. destroyed after the scan. */
   return;
}

void
mongoc_topology_scanner_node_retire (mongoc_topology_scanner_node_t *node)
{
   if (node->cmd) {
      node->cmd->state = MONGOC_ASYNC_CMD_CANCELED_STATE;
   }

   node->retired = true;
}

void
mongoc_topology_scanner_node_disconnect (mongoc_topology_scanner_node_t *node,
                                         bool failed)
{
   if (node->dns_results) {
      freeaddrinfo (node->dns_results);
      node->dns_results = NULL;
      node->current_dns_result = NULL;
   }

   if (node->cmd) {
      mongoc_async_cmd_destroy (node->cmd);
      node->cmd = NULL;
   }

   if (node->stream) {
      if (failed) {
         mongoc_stream_failed (node->stream);
      } else {
         mongoc_stream_destroy (node->stream);
      }

      node->stream = NULL;
   }
}

void
mongoc_topology_scanner_node_destroy (mongoc_topology_scanner_node_t *node, bool failed)
{
   DL_DELETE (node->ts->nodes, node);
   mongoc_topology_scanner_node_disconnect (node, failed);
   bson_free (node);
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_scanner_get_node --
 *
 *      Return the scanner node with the given id.
 *
 *      NOTE: only use this method when single-threaded!
 *
 *--------------------------------------------------------------------------
 */
mongoc_topology_scanner_node_t *
mongoc_topology_scanner_get_node (mongoc_topology_scanner_t *ts,
                                  uint32_t                   id)
{
   mongoc_topology_scanner_node_t *ele, *tmp;

   DL_FOREACH_SAFE (ts->nodes, ele, tmp)
   {
      if (ele->id == id) {
         return ele;
      }

      if (ele->id > id) {
         break;
      }
   }

   return NULL;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_scanner_has_node_for_host --
 *
 *      Whether the scanner has a node for the given host and port.
 *
 *--------------------------------------------------------------------------
 */
bool
mongoc_topology_scanner_has_node_for_host (mongoc_topology_scanner_t *ts,
                                           mongoc_host_list_t        *host)
{
   mongoc_topology_scanner_node_t *ele, *tmp;

   DL_FOREACH_SAFE (ts->nodes, ele, tmp)
   {
      if (_mongoc_host_list_equal (&ele->host, host)) {
         return true;
      }
   }

   return false;
}

/*
 *-----------------------------------------------------------------------
 *
 * This is the callback passed to async_cmd when we're running
 * ismasters from within the topology monitor.
 *
 *-----------------------------------------------------------------------
 */

static void
mongoc_topology_scanner_ismaster_handler (mongoc_async_cmd_result_t async_status,
                                          const bson_t             *ismaster_response,
                                          int64_t                   rtt_msec,
                                          void                     *data,
                                          bson_error_t             *error)
{
   mongoc_topology_scanner_node_t *node;
   int64_t now;
   const char *message;

   BSON_ASSERT (data);

   node = (mongoc_topology_scanner_node_t *)data;
   node->cmd = NULL;

   if (node->retired) {
      return;
   }

   now = bson_get_monotonic_time ();

   /* if no ismaster response, async cmd had an error or timed out */
   if (!ismaster_response ||
       async_status == MONGOC_ASYNC_CMD_ERROR ||
       async_status == MONGOC_ASYNC_CMD_TIMEOUT) {
      mongoc_stream_failed (node->stream);
      node->stream = NULL;
      node->last_failed = now;
      message = async_status == MONGOC_ASYNC_CMD_TIMEOUT ?
                "connection error" :
                "connection timeout";
      bson_set_error (&node->last_error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_STREAM_CONNECT,
                      _("%s calling ismaster on \'%s\'"),
                      message,
                      node->host.host_and_port);
   } else {
      node->last_failed = -1;
   }

   node->last_used = now;

   node->ts->cb (node->id, ismaster_response, rtt_msec,
                 node->ts->cb_data, error);
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_scanner_node_connect_tcp --
 *
 *      Create a socket stream for this node, begin a non-blocking
 *      connect and return.
 *
 * Returns:
 *      A stream. On failure, return NULL and fill out the error.
 *
 *--------------------------------------------------------------------------
 */

static mongoc_stream_t *
mongoc_topology_scanner_node_connect_tcp (mongoc_topology_scanner_node_t *node,
                                          bson_error_t                   *error)
{
   mongoc_socket_t *sock = NULL;
   struct addrinfo hints;
   struct addrinfo *rp;
   char portstr [8];
   mongoc_host_list_t *host;
   int s;

   ENTRY;

   host = &node->host;

   if (!node->dns_results) {
      bson_snprintf (portstr, sizeof portstr, "%hu", host->port);

      memset (&hints, 0, sizeof hints);
      hints.ai_family = host->family;
      hints.ai_socktype = SOCK_STREAM;
      hints.ai_flags = 0;
      hints.ai_protocol = 0;

      s = getaddrinfo (host->host, portstr, &hints, &node->dns_results);

      if (s != 0) {
         mongoc_counter_dns_failure_inc ();
         bson_set_error (error,
                         MONGOC_ERROR_STREAM,
                         MONGOC_ERROR_STREAM_NAME_RESOLUTION,
                         _("Failed to resolve '%s'"),
                         host->host);
         RETURN (NULL);
      }

      node->current_dns_result = node->dns_results;

      mongoc_counter_dns_success_inc ();
   }

   for (; node->current_dns_result;
        node->current_dns_result = node->current_dns_result->ai_next) {
      rp = node->current_dns_result;

      /*
       * Create a new non-blocking socket.
       */
      if (!(sock = mongoc_socket_new (rp->ai_family,
                                      rp->ai_socktype,
                                      rp->ai_protocol))) {
         continue;
      }

      mongoc_socket_connect (sock, rp->ai_addr, (socklen_t)rp->ai_addrlen, 0);

      break;
   }

   if (!sock) {
      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_CONNECT,
                      _("Failed to connect to target host: '%s'"),
                      host->host_and_port);
      freeaddrinfo (node->dns_results);
      node->dns_results = NULL;
      node->current_dns_result = NULL;
      RETURN (NULL);
   }

   return mongoc_stream_socket_new (sock);
}

static mongoc_stream_t *
mongoc_topology_scanner_node_connect_unix (mongoc_topology_scanner_node_t *node,
                                           bson_error_t                   *error)
{
#ifdef _WIN32
   ENTRY;
   bson_set_error (error,
                   MONGOC_ERROR_STREAM,
                   MONGOC_ERROR_STREAM_CONNECT,
                   _("UNIX domain sockets not supported on win32."));
   RETURN (NULL);
#else
   struct sockaddr_un saddr;
   mongoc_socket_t *sock;
   mongoc_stream_t *ret = NULL;
   mongoc_host_list_t *host;

   ENTRY;

   host = &node->host;

   memset (&saddr, 0, sizeof saddr);
   saddr.sun_family = AF_UNIX;
   bson_snprintf (saddr.sun_path, sizeof saddr.sun_path - 1,
                  "%s", host->host);

   sock = mongoc_socket_new (AF_UNIX, SOCK_STREAM, 0);

   if (sock == NULL) {
      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_SOCKET,
                      _("Failed to create socket."));
      RETURN (NULL);
   }

   if (-1 == mongoc_socket_connect (sock,
                                    (struct sockaddr *)&saddr,
                                    sizeof saddr,
                                    -1)) {
      char buf[128];
      char *errstr;

      errstr = bson_strerror_r(mongoc_socket_errno(sock), buf, sizeof(buf));

      bson_set_error (error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_CONNECT,
                      _("Failed to connect to UNIX domain socket: %s"),
                      errstr);
      mongoc_socket_destroy (sock);
      RETURN (NULL);
   }

   ret = mongoc_stream_socket_new (sock);

   RETURN (ret);
#endif
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_scanner_node_setup --
 *
 *      Create a stream and begin a non-blocking connect.
 *
 * Returns:
 *      true on success, or false and error is set.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_topology_scanner_node_setup (mongoc_topology_scanner_node_t *node,
                                    bson_error_t                   *error)
{
   mongoc_stream_t *sock_stream;

   if (node->stream) { return true; }

   BSON_ASSERT (!node->retired);

   if (node->ts->initiator) {
      sock_stream = node->ts->initiator (node->ts->uri, &node->host,
                                         node->ts->initiator_context, error);
   } else {
      if (node->host.family == AF_UNIX) {
         sock_stream = mongoc_topology_scanner_node_connect_unix (node, error);
      } else {
         sock_stream = mongoc_topology_scanner_node_connect_tcp (node, error);
      }

#ifdef MONGOC_ENABLE_SSL
      if (sock_stream && node->ts->ssl_opts) {
         sock_stream = mongoc_stream_tls_new (sock_stream, node->ts->ssl_opts, 1);
      }
#endif
   }

   if (!sock_stream) {
      /* Pass a rtt of -1 if we couldn't initialize a stream in node_setup */
      node->ts->cb (node->id, NULL, -1, node->ts->cb_data, error);
      return false;
   }

   node->stream = sock_stream;
   node->has_auth = false;
   node->timestamp = bson_get_monotonic_time ();

   return true;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_scanner_start --
 *
 *      Initializes the scanner and begins a full topology check. This
 *      should be called once before calling mongoc_topology_scanner_work()
 *      repeatedly to complete the scan.
 *
 *      If "obey_cooldown" is true, this is a single-threaded blocking scan
 *      that must obey the Server Discovery And Monitoring Spec's cooldownMS:
 *
 *      "After a single-threaded client gets a network error trying to check
 *      a server, the client skips re-checking the server until cooldownMS has
 *      passed.
 *
 *      "This avoids spending connectTimeoutMS on each unavailable server
 *      during each scan.
 *
 *      "This value MUST be 5000 ms, and it MUST NOT be configurable."
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_topology_scanner_start (mongoc_topology_scanner_t *ts,
                               int32_t timeout_msec,
                               bool obey_cooldown)
{
   mongoc_topology_scanner_node_t *node, *tmp;
   int64_t cooldown = INT64_MAX;
   BSON_ASSERT (ts);

   if (ts->in_progress) {
      return;
   }

   if (obey_cooldown) {
      /* when current cooldown period began */
      cooldown = bson_get_monotonic_time ()
                 - 1000 * MONGOC_TOPOLOGY_COOLDOWN_MS;
   }

   DL_FOREACH_SAFE (ts->nodes, node, tmp)
   {
      /* check node if it last failed before current cooldown period began */
      if (node->last_failed < cooldown) {
         if (mongoc_topology_scanner_node_setup (node, &node->last_error)) {

            BSON_ASSERT (!node->cmd);

            node->cmd = mongoc_async_cmd (
               ts->async, node->stream, ts->setup,
               node->host.host, "admin",
               &ts->ismaster_cmd,
               &mongoc_topology_scanner_ismaster_handler,
               node, timeout_msec);
         }
      }
   }
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_scanner_work --
 *
 *      Crank the knob on the topology scanner state machine. This should
 *      be called only after mongoc_topology_scanner_start() has been used
 *      to begin the scan.
 *
 * Returns:
 *      true if there is more work to do, false if scan is done.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_topology_scanner_work (mongoc_topology_scanner_t *ts,
                              int32_t                    timeout_msec)
{
   bool r;

   r = mongoc_async_run (ts->async, timeout_msec);

   if (! r) {
      ts->in_progress = false;
   }

   return r;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_scanner_sum_errors --
 *
 *      Summarizes all scanner node errors into one error message
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_topology_scanner_sum_errors (mongoc_topology_scanner_t *ts,
                                    bson_error_t              *error)
{
   mongoc_topology_scanner_node_t *node, *tmp;

   DL_FOREACH_SAFE (ts->nodes, node, tmp) {
      if (node->last_error.code) {
         char *msg = NULL;

         if (error->code) {
            msg = bson_strdup(error->message);
         }

         bson_set_error(error,
                        MONGOC_ERROR_SERVER_SELECTION,
                        MONGOC_ERROR_SERVER_SELECTION_FAILURE,
                        "%s[%s] ",
                        msg ? msg : "", node->last_error.message);
         if (msg) {
            bson_free (msg);
         }
      }
   }
   if (error->code) {
      error->message[strlen(error->message)-1] = '\0';
   }
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_topology_scanner_reset --
 *
 *      Reset "retired" nodes that failed or were removed in the previous
 *      scan.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_topology_scanner_reset (mongoc_topology_scanner_t *ts)
{
   mongoc_topology_scanner_node_t *node, *tmp;

   DL_FOREACH_SAFE (ts->nodes, node, tmp) {
      if (node->retired) {
         mongoc_topology_scanner_node_destroy (node, true);
      }
   }
}


/*==============================================================*/
/* --- mongoc-version-functions.c */

/**
 * mongoc_get_major_version:
 *
 * Helper function to return the runtime major version of the library.
 */
int
mongoc_get_major_version (void)
{
   return MONGOC_MAJOR_VERSION;
}


/**
 * mongoc_get_minor_version:
 *
 * Helper function to return the runtime minor version of the library.
 */
int
mongoc_get_minor_version (void)
{
   return MONGOC_MINOR_VERSION;
}

/**
 * mongoc_get_micro_version:
 *
 * Helper function to return the runtime micro version of the library.
 */
int
mongoc_get_micro_version (void)
{
   return MONGOC_MICRO_VERSION;
}

/**
 * mongoc_get_version:
 *
 * Helper function to return the runtime string version of the library.
 */
const char *
mongoc_get_version (void)
{
   return MONGOC_VERSION_S;
}

/**
 * mongoc_check_version:
 *
 * True if libmongoc's version is greater than or equal to the required
 * version.
 */
bool
mongoc_check_version (int required_major,
                      int required_minor,
                      int required_micro)
{
   return MONGOC_CHECK_VERSION(required_major, required_minor, required_micro);
}

/*==============================================================*/

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

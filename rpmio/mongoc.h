#ifndef	H_MONGO
#define	H_MONGO

/** \ingroup rpmio
 * \file rpmio/mongo.h
 */

/*
 * Copyright 2014 MongoDB, Inc.
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

#include <bson.h>

#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <arpa/inet.h>
# include <poll.h>
# include <netdb.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/uio.h>
# include <sys/un.h>
#endif

#include <sasl/sasl.h>
#include <sasl/saslutil.h>

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#ifdef __linux__
# include <sched.h>
# include <sys/sysinfo.h>
#elif defined(__FreeBSD__) || \
      defined(__NetBSD__) || \
      defined(__DragonFly__) || \
      defined(__OpenBSD__)
# include <sys/types.h>
# include <sys/sysctl.h>
# include <sys/param.h>
#endif

/*==============================================================*/
/* --- mongoc-iovec.h */

BSON_BEGIN_DECLS


#ifdef _WIN32
typedef struct
{
   u_long  iov_len;
   char   *iov_base;
} mongoc_iovec_t;
#else
typedef struct iovec mongoc_iovec_t;
#endif


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-socket.h */

BSON_BEGIN_DECLS


typedef struct _mongoc_socket_t mongoc_socket_t;


mongoc_socket_t *mongoc_socket_accept     (mongoc_socket_t       *sock,
                                           int64_t                expire_at);
int              mongoc_socket_bind       (mongoc_socket_t       *sock,
                                           const struct sockaddr *addr,
                                           socklen_t              addrlen);
int              mongoc_socket_close      (mongoc_socket_t       *socket);
int              mongoc_socket_connect    (mongoc_socket_t       *sock,
                                           const struct sockaddr *addr,
                                           socklen_t              addrlen,
                                           int64_t                expire_at);
char            *mongoc_socket_getnameinfo(mongoc_socket_t       *sock);
void             mongoc_socket_destroy    (mongoc_socket_t       *sock);
int              mongoc_socket_errno      (mongoc_socket_t       *sock);
int              mongoc_socket_getsockname(mongoc_socket_t       *sock,
                                           struct sockaddr       *addr,
                                           socklen_t             *addrlen);
int              mongoc_socket_listen     (mongoc_socket_t       *sock,
                                           unsigned int           backlog);
mongoc_socket_t *mongoc_socket_new        (int                    domain,
                                           int                    type,
                                           int                    protocol);
ssize_t          mongoc_socket_recv       (mongoc_socket_t       *sock,
                                           void                  *buf,
                                           size_t                 buflen,
                                           int                    flags,
                                           int64_t                expire_at);
int              mongoc_socket_setsockopt (mongoc_socket_t       *sock,
                                           int                    level,
                                           int                    optname,
                                           const void            *optval,
                                           socklen_t              optlen);
ssize_t          mongoc_socket_send       (mongoc_socket_t       *sock,
                                           const void            *buf,
                                           size_t                 buflen,
                                           int64_t                expire_at);
ssize_t          mongoc_socket_sendv      (mongoc_socket_t       *sock,
                                           mongoc_iovec_t        *iov,
                                           size_t                 iovcnt,
                                           int64_t                expire_at);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-ssl.h */

BSON_BEGIN_DECLS


typedef struct _mongoc_ssl_opt_t mongoc_ssl_opt_t;


struct _mongoc_ssl_opt_t
{
   const char *pem_file;
   const char *pem_pwd;
   const char *ca_file;
   const char *ca_dir;
   const char *crl_file;
   bool        weak_cert_validation;
   void       *padding [8];
};


const mongoc_ssl_opt_t *mongoc_ssl_opt_get_default (void) BSON_GNUC_CONST;


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-host-list.h */

BSON_BEGIN_DECLS


#ifndef HOST_NAME_MAX
# ifdef _POSIX_HOST_NAME_MAX
#  define BSON_HOST_NAME_MAX _POSIX_HOST_NAME_MAX
# else
#  define BSON_HOST_NAME_MAX 255
# endif
#else
# define BSON_HOST_NAME_MAX HOST_NAME_MAX
#endif


typedef struct _mongoc_host_list_t mongoc_host_list_t;


struct _mongoc_host_list_t
{
   mongoc_host_list_t *next;
   char                host [BSON_HOST_NAME_MAX + 1];
   char                host_and_port [BSON_HOST_NAME_MAX + 7];
   uint16_t            port;
   int                 family;
   void               *padding [4];
};


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-write-concern.h */

BSON_BEGIN_DECLS


#define MONGOC_WRITE_CONCERN_W_UNACKNOWLEDGED  0
#define MONGOC_WRITE_CONCERN_W_ERRORS_IGNORED -1
#define MONGOC_WRITE_CONCERN_W_DEFAULT        -2
#define MONGOC_WRITE_CONCERN_W_MAJORITY       -3
#define MONGOC_WRITE_CONCERN_W_TAG            -4


typedef struct _mongoc_write_concern_t mongoc_write_concern_t;


mongoc_write_concern_t *mongoc_write_concern_new           (void);
mongoc_write_concern_t *mongoc_write_concern_copy          (const mongoc_write_concern_t *write_concern);
void                    mongoc_write_concern_destroy       (mongoc_write_concern_t       *write_concern);
bool                    mongoc_write_concern_get_fsync     (const mongoc_write_concern_t *write_concern);
void                    mongoc_write_concern_set_fsync     (mongoc_write_concern_t       *write_concern,
                                                            bool                          fsync_);
bool                    mongoc_write_concern_get_journal   (const mongoc_write_concern_t *write_concern);
void                    mongoc_write_concern_set_journal   (mongoc_write_concern_t       *write_concern,
                                                            bool                          journal);
int32_t                 mongoc_write_concern_get_w         (const mongoc_write_concern_t *write_concern);
void                    mongoc_write_concern_set_w         (mongoc_write_concern_t       *write_concern,
                                                            int32_t                       w);
const char             *mongoc_write_concern_get_wtag      (const mongoc_write_concern_t *write_concern);
void                    mongoc_write_concern_set_wtag      (mongoc_write_concern_t       *write_concern,
                                                            const char                   *tag);
int32_t                 mongoc_write_concern_get_wtimeout  (const mongoc_write_concern_t *write_concern);
void                    mongoc_write_concern_set_wtimeout  (mongoc_write_concern_t       *write_concern,
                                                            int32_t                       wtimeout_msec);
bool                    mongoc_write_concern_get_wmajority (const mongoc_write_concern_t *write_concern);
void                    mongoc_write_concern_set_wmajority (mongoc_write_concern_t       *write_concern,
                                                            int32_t                       wtimeout_msec);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-uri.h */

#ifndef MONGOC_DEFAULT_PORT
# define MONGOC_DEFAULT_PORT 27017
#endif

BSON_BEGIN_DECLS


typedef struct _mongoc_uri_t mongoc_uri_t;


mongoc_uri_t                 *mongoc_uri_copy               (const mongoc_uri_t *uri);
void                          mongoc_uri_destroy            (mongoc_uri_t       *uri);
mongoc_uri_t                 *mongoc_uri_new                (const char         *uri_string)
   BSON_GNUC_WARN_UNUSED_RESULT;
mongoc_uri_t                 *mongoc_uri_new_for_host_port  (const char         *hostname,
                                                             uint16_t            port)
   BSON_GNUC_WARN_UNUSED_RESULT;
const mongoc_host_list_t     *mongoc_uri_get_hosts          (const mongoc_uri_t *uri);
const char                   *mongoc_uri_get_database       (const mongoc_uri_t *uri);
const bson_t                 *mongoc_uri_get_options        (const mongoc_uri_t *uri);
const char                   *mongoc_uri_get_password       (const mongoc_uri_t *uri);
const bson_t                 *mongoc_uri_get_read_prefs     (const mongoc_uri_t *uri);
const char                   *mongoc_uri_get_replica_set    (const mongoc_uri_t *uri);
const char                   *mongoc_uri_get_string         (const mongoc_uri_t *uri);
const char                   *mongoc_uri_get_username       (const mongoc_uri_t *uri);
const char                   *mongoc_uri_get_auth_source    (const mongoc_uri_t *uri);
const char                   *mongoc_uri_get_auth_mechanism (const mongoc_uri_t *uri);
bool                          mongoc_uri_get_ssl            (const mongoc_uri_t *uri);
char                         *mongoc_uri_unescape           (const char         *escaped_string);
const mongoc_write_concern_t *mongoc_uri_get_write_concern  (const mongoc_uri_t *uri);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-stream.h */

BSON_BEGIN_DECLS


typedef struct _mongoc_stream_t mongoc_stream_t;


struct _mongoc_stream_t
{
   int              type;
   void             (*destroy)         (mongoc_stream_t *stream);
   int              (*close)           (mongoc_stream_t *stream);
   int              (*flush)           (mongoc_stream_t *stream);
   ssize_t          (*writev)          (mongoc_stream_t *stream,
                                        mongoc_iovec_t  *iov,
                                        size_t           iovcnt,
                                        int32_t          timeout_msec);
   ssize_t          (*readv)           (mongoc_stream_t *stream,
                                        mongoc_iovec_t  *iov,
                                        size_t           iovcnt,
                                        size_t           min_bytes,
                                        int32_t          timeout_msec);
   int              (*setsockopt)      (mongoc_stream_t *stream,
                                        int              level,
                                        int              optname,
                                        void            *optval,
                                        socklen_t        optlen);
   mongoc_stream_t *(*get_base_stream) (mongoc_stream_t *stream);
   void            *padding [8];
};


mongoc_stream_t *mongoc_stream_get_base_stream (mongoc_stream_t       *stream);
int              mongoc_stream_close           (mongoc_stream_t       *stream);
void             mongoc_stream_destroy         (mongoc_stream_t       *stream);
int              mongoc_stream_flush           (mongoc_stream_t       *stream);
ssize_t          mongoc_stream_writev          (mongoc_stream_t       *stream,
                                                mongoc_iovec_t        *iov,
                                                size_t                 iovcnt,
                                                int32_t                timeout_msec);
ssize_t          mongoc_stream_readv           (mongoc_stream_t       *stream,
                                                mongoc_iovec_t        *iov,
                                                size_t                 iovcnt,
                                                size_t                 min_bytes,
                                                int32_t                timeout_msec);
ssize_t          mongoc_stream_read            (mongoc_stream_t       *stream,
                                                void                  *buf,
                                                size_t                 count,
                                                size_t                 min_bytes,
                                                int32_t                timeout_msec);
int              mongoc_stream_setsockopt      (mongoc_stream_t       *stream,
                                                int                    level,
                                                int                    optname,
                                                void                  *optval,
                                                socklen_t              optlen);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-stream-buffered.h */

BSON_BEGIN_DECLS


mongoc_stream_t *mongoc_stream_buffered_new (mongoc_stream_t *base_stream,
                                             size_t           buffer_size);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-stream-file.h */

BSON_BEGIN_DECLS


typedef struct _mongoc_stream_file_t mongoc_stream_file_t;


mongoc_stream_t *mongoc_stream_file_new          (int                   fd);
mongoc_stream_t *mongoc_stream_file_new_for_path (const char           *path,
                                                  int                   flags,
                                                  int                   mode);
int              mongoc_stream_file_get_fd       (mongoc_stream_file_t *stream);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-stream-socket.h */

BSON_BEGIN_DECLS


typedef struct _mongoc_stream_socket_t mongoc_stream_socket_t;


mongoc_stream_t *mongoc_stream_socket_new        (mongoc_socket_t        *socket);
mongoc_socket_t *mongoc_stream_socket_get_socket (mongoc_stream_socket_t *stream);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-stream-tls.h */

BSON_BEGIN_DECLS


bool             mongoc_stream_tls_do_handshake  (mongoc_stream_t  *stream,
                                                  int32_t           timeout_msec);
bool             mongoc_stream_tls_check_cert    (mongoc_stream_t  *stream,
                                                  const char       *host);
mongoc_stream_t *mongoc_stream_tls_new           (mongoc_stream_t  *base_stream,
                                                  mongoc_ssl_opt_t *opt,
                                                  int               client);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-opcode.h */

BSON_BEGIN_DECLS


typedef enum
{
   MONGOC_OPCODE_REPLY         = 1,
   MONGOC_OPCODE_MSG           = 1000,
   MONGOC_OPCODE_UPDATE        = 2001,
   MONGOC_OPCODE_INSERT        = 2002,
   MONGOC_OPCODE_QUERY         = 2004,
   MONGOC_OPCODE_GET_MORE      = 2005,
   MONGOC_OPCODE_DELETE        = 2006,
   MONGOC_OPCODE_KILL_CURSORS  = 2007,
} mongoc_opcode_t;


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-version.h */

/**
 * MONGOC_MAJOR_VERSION:
 *
 * MONGOC major version component (e.g. 1 if %MONGOC_VERSION is 1.2.3)
 */
#define MONGOC_MAJOR_VERSION (1)


/**
 * MONGOC_MINOR_VERSION:
 *
 * MONGOC minor version component (e.g. 2 if %MONGOC_VERSION is 1.2.3)
 */
#define MONGOC_MINOR_VERSION (0)


/**
 * MONGOC_MICRO_VERSION:
 *
 * MONGOC micro version component (e.g. 3 if %MONGOC_VERSION is 1.2.3)
 */
#define MONGOC_MICRO_VERSION (1)


/**
 * MONGOC_VERSION:
 *
 * MONGOC version.
 */
#define MONGOC_VERSION (1.0.1)


/**
 * MONGOC_VERSION_S:
 *
 * MONGOC version, encoded as a string, useful for printing and
 * concatenation.
 */
#define MONGOC_VERSION_S "1.0.1"


/**
 * MONGOC_VERSION_HEX:
 *
 * MONGOC version, encoded as an hexadecimal number, useful for
 * integer comparisons.
 */
#define MONGOC_VERSION_HEX (MONGOC_MAJOR_VERSION << 24 | \
                          MONGOC_MINOR_VERSION << 16 | \
                          MONGOC_MICRO_VERSION << 8)


/**
 * MONGOC_CHECK_VERSION:
 * @major: required major version
 * @minor: required minor version
 * @micro: required micro version
 *
 * Compile-time version checking. Evaluates to %TRUE if the version
 * of MONGOC is greater than the required one.
 */
#define MONGOC_CHECK_VERSION(major,minor,micro)   \
        (MONGOC_MAJOR_VERSION > (major) || \
         (MONGOC_MAJOR_VERSION == (major) && MONGOC_MINOR_VERSION > (minor)) || \
         (MONGOC_MAJOR_VERSION == (major) && MONGOC_MINOR_VERSION == (minor) && \
          MONGOC_MICRO_VERSION >= (micro)))

/*==============================================================*/
/* --- mongoc-config.h */

/*
 * MONGOC_ENABLE_SSL is set from configure to determine if we are
 * compiled with SSL support.
 */
#define MONGOC_ENABLE_SSL 1

#if MONGOC_ENABLE_SSL != 1
#  undef MONGOC_ENABLE_SSL
#endif


/*
 * MONGOC_ENABLE_SASL is set from configure to determine if we are
 * compiled with SASL support.
 */
#define MONGOC_ENABLE_SASL 0

#if MONGOC_ENABLE_SASL != 1
#  undef MONGOC_ENABLE_SASL
#endif


/*==============================================================*/
/* --- mongoc-init.h */

BSON_BEGIN_DECLS


void mongoc_init   (void);
void mongoc_cleanup(void);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-log.h */

BSON_BEGIN_DECLS


#ifndef MONGOC_LOG_DOMAIN
# define MONGOC_LOG_DOMAIN "mongoc"
#endif


#define MONGOC_ERROR(...)    mongoc_log(MONGOC_LOG_LEVEL_ERROR,    MONGOC_LOG_DOMAIN, __VA_ARGS__)
#define MONGOC_CRITICAL(...) mongoc_log(MONGOC_LOG_LEVEL_CRITICAL, MONGOC_LOG_DOMAIN, __VA_ARGS__)
#define MONGOC_WARNING(...)  mongoc_log(MONGOC_LOG_LEVEL_WARNING,  MONGOC_LOG_DOMAIN, __VA_ARGS__)
#define MONGOC_MESSAGE(...)  mongoc_log(MONGOC_LOG_LEVEL_MESSAGE,  MONGOC_LOG_DOMAIN, __VA_ARGS__)
#define MONGOC_INFO(...)     mongoc_log(MONGOC_LOG_LEVEL_INFO,     MONGOC_LOG_DOMAIN, __VA_ARGS__)
#define MONGOC_DEBUG(...)    mongoc_log(MONGOC_LOG_LEVEL_DEBUG,    MONGOC_LOG_DOMAIN, __VA_ARGS__)


typedef enum
{
   MONGOC_LOG_LEVEL_ERROR,
   MONGOC_LOG_LEVEL_CRITICAL,
   MONGOC_LOG_LEVEL_WARNING,
   MONGOC_LOG_LEVEL_MESSAGE,
   MONGOC_LOG_LEVEL_INFO,
   MONGOC_LOG_LEVEL_DEBUG,
   MONGOC_LOG_LEVEL_TRACE,
} mongoc_log_level_t;


/**
 * mongoc_log_func_t:
 * @log_level: The level of the log message.
 * @log_domain: The domain of the log message, such as "client".
 * @message: The message generated.
 * @user_data: User data provided to mongoc_log_set_handler().
 *
 * This function prototype can be used to set a custom log handler for the
 * libmongoc library. This is useful if you would like to show them in a
 * user interface or alternate storage.
 */
typedef void (*mongoc_log_func_t) (mongoc_log_level_t  log_level,
                                   const char         *log_domain,
                                   const char         *message,
                                   void               *user_data);


/**
 * mongoc_log_set_handler:
 * @log_func: A function to handle log messages.
 * @user_data: User data for @log_func.
 *
 * Sets the function to be called to handle logging.
 */
void mongoc_log_set_handler (mongoc_log_func_t  log_func,
                             void              *user_data);


/**
 * mongoc_log:
 * @log_level: The log level.
 * @log_domain: The log domain (such as "client").
 * @format: The format string for the log message.
 *
 * Logs a message using the currently configured logger.
 *
 * This method will hold a logging lock to prevent concurrent calls to the
 * logging infrastructure. It is important that your configured log function
 * does not re-enter the logging system or deadlock will occur.
 *
 */
void mongoc_log (mongoc_log_level_t  log_level,
                 const char         *log_domain,
                 const char         *format,
                 ...)
   BSON_GNUC_PRINTF(3, 4);



void mongoc_log_default_handler (mongoc_log_level_t  log_level,
                                 const char         *log_domain,
                                 const char         *message,
                                 void               *user_data);


/**
 * mongoc_log_level_str:
 * @log_level: The log level.
 *
 * Returns: The string representation of log_level
 */
const char *
mongoc_log_level_str (mongoc_log_level_t log_level);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-trace.h */

BSON_BEGIN_DECLS


#ifdef MONGOC_TRACE
#define TRACE(msg, ...) \
                    do { mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, "TRACE: %s():%d " msg, __FUNCTION__, __LINE__, __VA_ARGS__); } while (0)
#define ENTRY       do { mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, "ENTRY: %s():%d", __FUNCTION__, __LINE__); } while (0)
#define EXIT        do { mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, " EXIT: %s():%d", __FUNCTION__, __LINE__); return; } while (0)
#define RETURN(ret) do { mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, " EXIT: %s():%d", __FUNCTION__, __LINE__); return ret; } while (0)
#define GOTO(label) do { mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, " GOTO: %s():%d %s", __FUNCTION__, __LINE__, #label); goto label; } while (0)
#define DUMP_BYTES(_n, _b, _l) \
   do { \
      bson_string_t *str, *astr; \
      int32_t _i; \
      uint8_t _v; \
      mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, \
                 " %s = %p [%d]", #_n, _b, (int)_l); \
      str = bson_string_new(NULL); \
      astr = bson_string_new(NULL); \
      for (_i = 0; _i < _l; _i++) { \
         _v = *(_b + _i); \
         if ((_i % 16) == 0) { \
            bson_string_append_printf(str, "%05x: ", _i); \
         } \
         bson_string_append_printf(str, " %02x", _v); \
         if (isprint(_v)) { \
            bson_string_append_printf(astr, " %c", _v); \
         } else { \
            bson_string_append(astr, " ."); \
         } \
         if ((_i % 16) == 15) { \
            mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, \
                       "%s %s", str->str, astr->str); \
            bson_string_truncate(str, 0); \
            bson_string_truncate(astr, 0); \
         } else if ((_i % 16) == 7) { \
            bson_string_append(str, " "); \
            bson_string_append(astr, " "); \
         } \
      } \
      if (_i != 16) { \
         mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, \
                    "%-56s %s", str->str, astr->str); \
      } \
      bson_string_free(str, true); \
      bson_string_free(astr, true); \
   } while (0)
#define DUMP_IOVEC(_n, _iov, _iovcnt) \
   do { \
      bson_string_t *str, *astr; \
      const char *_b; \
      unsigned _i = 0; \
      unsigned _j = 0; \
      unsigned _k = 0; \
      size_t _l = 0; \
      uint8_t _v; \
      for (_i = 0; _i < _iovcnt; _i++) { \
         _l += _iov[_i].iov_len; \
      } \
      mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, \
                 " %s = %p [%d]", #_n, _iov, (int)_l); \
      _i = 0; \
      str = bson_string_new(NULL); \
      astr = bson_string_new(NULL); \
      for (_j = 0; _j < _iovcnt; _j++) { \
         _b = (char *)_iov[_j].iov_base; \
         _l = _iov[_j].iov_len; \
         for (_k = 0; _k < _l; _k++, _i++) { \
            _v = *(_b + _k); \
            if ((_i % 16) == 0) { \
               bson_string_append_printf(str, "%05x: ", _i); \
            } \
            bson_string_append_printf(str, " %02x", _v); \
            if (isprint(_v)) { \
               bson_string_append_printf(astr, " %c", _v); \
            } else { \
               bson_string_append(astr, " ."); \
            } \
            if ((_i % 16) == 15) { \
               mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, \
                          "%s %s", str->str, astr->str); \
               bson_string_truncate(str, 0); \
               bson_string_truncate(astr, 0); \
            } else if ((_i % 16) == 7) { \
               bson_string_append(str, " "); \
               bson_string_append(astr, " "); \
            } \
         } \
      } \
      if (_i != 16) { \
         mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, \
                    "%-56s %s", str->str, astr->str); \
      } \
      bson_string_free(str, true); \
      bson_string_free(astr, true); \
   } while (0)
#else
#define TRACE(msg,...)
#define ENTRY
#define EXIT        return
#define RETURN(ret) return ret
#define GOTO(label) goto label
#define DUMP_BYTES(_n, _b, _l)
#define DUMP_IOVEC(_n, _iov, _iovcnt)
#endif


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-read-prefs.h */

BSON_BEGIN_DECLS


typedef struct _mongoc_read_prefs_t mongoc_read_prefs_t;


typedef enum
{
   MONGOC_READ_PRIMARY             = (1 << 0),
   MONGOC_READ_SECONDARY           = (1 << 1),
   MONGOC_READ_PRIMARY_PREFERRED   = (1 << 2) | MONGOC_READ_PRIMARY,
   MONGOC_READ_SECONDARY_PREFERRED = (1 << 2) | MONGOC_READ_SECONDARY,
   MONGOC_READ_NEAREST             = (1 << 3) | MONGOC_READ_SECONDARY,
} mongoc_read_mode_t;


mongoc_read_prefs_t *mongoc_read_prefs_new      (mongoc_read_mode_t         read_mode);
mongoc_read_prefs_t *mongoc_read_prefs_copy     (const mongoc_read_prefs_t *read_prefs);
void                 mongoc_read_prefs_destroy  (mongoc_read_prefs_t       *read_prefs);
mongoc_read_mode_t   mongoc_read_prefs_get_mode (const mongoc_read_prefs_t *read_prefs);
void                 mongoc_read_prefs_set_mode (mongoc_read_prefs_t       *read_prefs,
                                                 mongoc_read_mode_t         mode);
const bson_t        *mongoc_read_prefs_get_tags (const mongoc_read_prefs_t *read_prefs);
void                 mongoc_read_prefs_set_tags (mongoc_read_prefs_t       *read_prefs,
                                                 const bson_t              *tags);
void                 mongoc_read_prefs_add_tag  (mongoc_read_prefs_t       *read_prefs,
                                                 const bson_t              *tag);
bool                 mongoc_read_prefs_is_valid (const mongoc_read_prefs_t *read_prefs);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-matcher.h */

BSON_BEGIN_DECLS


typedef struct _mongoc_matcher_t mongoc_matcher_t;


mongoc_matcher_t *mongoc_matcher_new     (const bson_t           *query,
                                          bson_error_t           *error);
bool              mongoc_matcher_match   (const mongoc_matcher_t *matcher,
                                          const bson_t           *document);
void              mongoc_matcher_destroy (mongoc_matcher_t       *matcher);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-cursor.h */

BSON_BEGIN_DECLS


typedef struct _mongoc_cursor_t mongoc_cursor_t;


mongoc_cursor_t *mongoc_cursor_clone    (const mongoc_cursor_t  *cursor) BSON_GNUC_WARN_UNUSED_RESULT;
void             mongoc_cursor_destroy  (mongoc_cursor_t        *cursor);
bool             mongoc_cursor_more     (mongoc_cursor_t        *cursor);
bool             mongoc_cursor_next     (mongoc_cursor_t        *cursor,
                                         const bson_t          **bson);
bool             mongoc_cursor_error    (mongoc_cursor_t        *cursor,
                                         bson_error_t           *error);
void             mongoc_cursor_get_host (mongoc_cursor_t        *cursor,
                                         mongoc_host_list_t     *host);
bool             mongoc_cursor_is_alive (const mongoc_cursor_t  *cursor);
const bson_t    *mongoc_cursor_current  (const mongoc_cursor_t  *cursor);
uint32_t         mongoc_cursor_get_hint (const mongoc_cursor_t  *cursor);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-flags.h */

BSON_BEGIN_DECLS


/**
 * mongoc_delete_flags_t:
 * @MONGOC_DELETE_NONE: Specify no delete flags.
 * @MONGOC_DELETE_SINGLE_REMOVE: Only remove the first document matching the
 *    document selector.
 *
 * This type is only for use with deprecated functions and should not be
 * used in new code. Use mongoc_remove_flags_t instead.
 *
 * #mongoc_delete_flags_t are used when performing a delete operation.
 */
typedef enum
{
   MONGOC_DELETE_NONE          = 0,
   MONGOC_DELETE_SINGLE_REMOVE = 1 << 0,
} mongoc_delete_flags_t;


/**
 * mongoc_remove_flags_t:
 * @MONGOC_REMOVE_NONE: Specify no delete flags.
 * @MONGOC_REMOVE_SINGLE_REMOVE: Only remove the first document matching the
 *    document selector.
 *
 * #mongoc_remove_flags_t are used when performing a remove operation.
 */
typedef enum
{
   MONGOC_REMOVE_NONE          = 0,
   MONGOC_REMOVE_SINGLE_REMOVE = 1 << 0,
} mongoc_remove_flags_t;


/**
 * mongoc_insert_flags_t:
 * @MONGOC_INSERT_NONE: Specify no insert flags.
 * @MONGOC_INSERT_CONTINUE_ON_ERROR: Continue inserting documents from
 *    the insertion set even if one fails.
 *
 * #mongoc_insert_flags_t are used when performing an insert operation.
 */
typedef enum
{
   MONGOC_INSERT_NONE              = 0,
   MONGOC_INSERT_CONTINUE_ON_ERROR = 1 << 0,
} mongoc_insert_flags_t;


#define MONGOC_INSERT_NO_VALIDATE (1U << 31)


/**
 * mongoc_query_flags_t:
 * @MONGOC_QUERY_NONE: No query flags supplied.
 * @MONGOC_QUERY_TAILABLE_CURSOR: Cursor will not be closed when the last
 *    data is retrieved. You can resume this cursor later.
 * @MONGOC_QUERY_SLAVE_OK: Allow query of replica slave.
 * @MONGOC_QUERY_OPLOG_REPLAY: Used internally by Mongo.
 * @MONGOC_QUERY_NO_CURSOR_TIMEOUT: The server normally times out idle
 *    cursors after an inactivity period (10 minutes). This prevents that.
 * @MONGOC_QUERY_AWAIT_DATA: Use with %MONGOC_QUERY_TAILABLE_CURSOR. Block
 *    rather than returning no data. After a period, time out.
 * @MONGOC_QUERY_EXHAUST: Stream the data down full blast in multiple
 *    "more" packages. Faster when you are pulling a lot of data and
 *    know you want to pull it all down.
 * @MONGOC_QUERY_PARTIAL: Get partial results from mongos if some shards
 *    are down (instead of throwing an error).
 *
 * #mongoc_query_flags_t is used for querying a Mongo instance.
 */
typedef enum
{
   MONGOC_QUERY_NONE              = 0,
   MONGOC_QUERY_TAILABLE_CURSOR   = 1 << 1,
   MONGOC_QUERY_SLAVE_OK          = 1 << 2,
   MONGOC_QUERY_OPLOG_REPLAY      = 1 << 3,
   MONGOC_QUERY_NO_CURSOR_TIMEOUT = 1 << 4,
   MONGOC_QUERY_AWAIT_DATA        = 1 << 5,
   MONGOC_QUERY_EXHAUST           = 1 << 6,
   MONGOC_QUERY_PARTIAL           = 1 << 7,
} mongoc_query_flags_t;


/**
 * mongoc_reply_flags_t:
 * @MONGOC_REPLY_NONE: No flags set.
 * @MONGOC_REPLY_CURSOR_NOT_FOUND: Cursor was not found.
 * @MONGOC_REPLY_QUERY_FAILURE: Query failed, error document provided.
 * @MONGOC_REPLY_SHARD_CONFIG_STALE: Shard configuration is stale.
 * @MONGOC_REPLY_AWAIT_CAPABLE: Wait for data to be returned until timeout
 *    has passed. Used with %MONGOC_QUERY_TAILABLE_CURSOR.
 *
 * #mongoc_reply_flags_t contains flags supplied by the Mongo server in reply
 * to a request.
 */
typedef enum
{
   MONGOC_REPLY_NONE               = 0,
   MONGOC_REPLY_CURSOR_NOT_FOUND   = 1 << 0,
   MONGOC_REPLY_QUERY_FAILURE      = 1 << 1,
   MONGOC_REPLY_SHARD_CONFIG_STALE = 1 << 2,
   MONGOC_REPLY_AWAIT_CAPABLE      = 1 << 3,
} mongoc_reply_flags_t;


/**
 * mongoc_update_flags_t:
 * @MONGOC_UPDATE_NONE: No update flags specified.
 * @MONGOC_UPDATE_UPSERT: Perform an upsert.
 * @MONGOC_UPDATE_MULTI_UPDATE: Continue updating after first match.
 *
 * #mongoc_update_flags_t is used when updating documents found in Mongo.
 */
typedef enum
{
   MONGOC_UPDATE_NONE         = 0,
   MONGOC_UPDATE_UPSERT       = 1 << 0,
   MONGOC_UPDATE_MULTI_UPDATE = 1 << 1,
} mongoc_update_flags_t;


#define MONGOC_UPDATE_NO_VALIDATE (1U << 31)


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-index.h */

BSON_BEGIN_DECLS


typedef struct
{
   bool          is_initialized;
   bool          background;
   bool          unique;
   const char   *name;
   bool          drop_dups;
   bool          sparse;
   int32_t       expire_after_seconds;
   int32_t       v;
   const bson_t *weights;
   const char   *default_language;
   const char   *language_override;
   void         *padding[8];
} mongoc_index_opt_t;


const mongoc_index_opt_t *mongoc_index_opt_get_default (void) BSON_GNUC_CONST;
void                      mongoc_index_opt_init        (mongoc_index_opt_t *opt);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-error.h */

BSON_BEGIN_DECLS


typedef enum
{
   MONGOC_ERROR_CLIENT = 1,
   MONGOC_ERROR_STREAM,
   MONGOC_ERROR_PROTOCOL,
   MONGOC_ERROR_CURSOR,
   MONGOC_ERROR_QUERY,
   MONGOC_ERROR_INSERT,
   MONGOC_ERROR_SASL,
   MONGOC_ERROR_BSON,
   MONGOC_ERROR_MATCHER,
   MONGOC_ERROR_NAMESPACE,
   MONGOC_ERROR_COMMAND,
   MONGOC_ERROR_COLLECTION,
   MONGOC_ERROR_GRIDFS,
} mongoc_error_domain_t;


typedef enum
{
   MONGOC_ERROR_STREAM_INVALID_TYPE = 1,
   MONGOC_ERROR_STREAM_INVALID_STATE,
   MONGOC_ERROR_STREAM_NAME_RESOLUTION,
   MONGOC_ERROR_STREAM_SOCKET,
   MONGOC_ERROR_STREAM_CONNECT,
   MONGOC_ERROR_STREAM_NOT_ESTABLISHED,

   MONGOC_ERROR_CLIENT_NOT_READY,
   MONGOC_ERROR_CLIENT_TOO_BIG,
   MONGOC_ERROR_CLIENT_TOO_SMALL,
   MONGOC_ERROR_CLIENT_GETNONCE,
   MONGOC_ERROR_CLIENT_AUTHENTICATE,
   MONGOC_ERROR_CLIENT_NO_ACCEPTABLE_PEER,
   MONGOC_ERROR_CLIENT_IN_EXHAUST,

   MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
   MONGOC_ERROR_PROTOCOL_BAD_WIRE_VERSION,

   MONGOC_ERROR_CURSOR_INVALID_CURSOR,

   MONGOC_ERROR_QUERY_FAILURE,

   MONGOC_ERROR_BSON_INVALID,

   MONGOC_ERROR_MATCHER_INVALID,

   MONGOC_ERROR_NAMESPACE_INVALID,

   MONGOC_ERROR_COMMAND_INVALID_ARG,

   MONGOC_ERROR_COLLECTION_INSERT_FAILED,

   MONGOC_ERROR_GRIDFS_INVALID_FILENAME,

   MONGOC_ERROR_QUERY_COMMAND_NOT_FOUND = 59,
   MONGOC_ERROR_QUERY_NOT_TAILABLE = 13051,

   /* Dup with query failure. */
   MONGOC_ERROR_PROTOCOL_ERROR = 17,
} mongoc_error_code_t;


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-bulk-operation.h */

BSON_BEGIN_DECLS


typedef struct _mongoc_bulk_operation_t mongoc_bulk_operation_t;


void mongoc_bulk_operation_destroy     (mongoc_bulk_operation_t       *bulk);
uint32_t mongoc_bulk_operation_execute (mongoc_bulk_operation_t       *bulk,
                                        bson_t                        *reply,
                                        bson_error_t                  *error);
void mongoc_bulk_operation_delete      (mongoc_bulk_operation_t       *bulk,
                                        const bson_t                  *selector)
   BSON_GNUC_DEPRECATED_FOR (mongoc_bulk_operation_remove);
void mongoc_bulk_operation_delete_one  (mongoc_bulk_operation_t       *bulk,
                                        const bson_t                  *selector)
   BSON_GNUC_DEPRECATED_FOR (mongoc_bulk_operation_remove_one);
void mongoc_bulk_operation_insert      (mongoc_bulk_operation_t       *bulk,
                                        const bson_t                  *document);
void mongoc_bulk_operation_remove      (mongoc_bulk_operation_t       *bulk,
                                        const bson_t                  *selector);
void mongoc_bulk_operation_remove_one  (mongoc_bulk_operation_t       *bulk,
                                        const bson_t                  *selector);
void mongoc_bulk_operation_replace_one (mongoc_bulk_operation_t       *bulk,
                                        const bson_t                  *selector,
                                        const bson_t                  *document,
                                        bool                           upsert);
void mongoc_bulk_operation_update      (mongoc_bulk_operation_t       *bulk,
                                        const bson_t                  *selector,
                                        const bson_t                  *document,
                                        bool                           upsert);
void mongoc_bulk_operation_update_one  (mongoc_bulk_operation_t       *bulk,
                                        const bson_t                  *selector,
                                        const bson_t                  *document,
                                        bool                           upsert);


/*
 * The following functions are really only useful by language bindings and
 * those wanting to replay a bulk operation to a number of clients or
 * collections.
 */
mongoc_bulk_operation_t *mongoc_bulk_operation_new               (bool                          ordered);
void                     mongoc_bulk_operation_set_write_concern (mongoc_bulk_operation_t      *bulk,
                                                                  const mongoc_write_concern_t *write_concern);
void                     mongoc_bulk_operation_set_database      (mongoc_bulk_operation_t      *bulk,
                                                                  const char                   *database);
void                     mongoc_bulk_operation_set_collection    (mongoc_bulk_operation_t      *bulk,
                                                                  const char                   *collection);
void                     mongoc_bulk_operation_set_client        (mongoc_bulk_operation_t      *bulk,
                                                                  void                         *client);
void                     mongoc_bulk_operation_set_hint          (mongoc_bulk_operation_t      *bulk,
                                                                  uint32_t                      hint);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-collection.h */

BSON_BEGIN_DECLS


typedef struct _mongoc_collection_t mongoc_collection_t;


mongoc_cursor_t               *mongoc_collection_aggregate           (mongoc_collection_t           *collection,
                                                                      mongoc_query_flags_t           flags,
                                                                      const bson_t                  *pipeline,
                                                                      const bson_t                  *options,
                                                                      const mongoc_read_prefs_t     *read_prefs) BSON_GNUC_WARN_UNUSED_RESULT;
void                          mongoc_collection_destroy              (mongoc_collection_t           *collection);
mongoc_cursor_t              *mongoc_collection_command              (mongoc_collection_t           *collection,
                                                                      mongoc_query_flags_t           flags,
                                                                      uint32_t                       skip,
                                                                      uint32_t                       limit,
                                                                      uint32_t                       batch_size,
                                                                      const bson_t                  *command,
                                                                      const bson_t                  *fields,
                                                                      const mongoc_read_prefs_t     *read_prefs) BSON_GNUC_WARN_UNUSED_RESULT;
bool                          mongoc_collection_command_simple       (mongoc_collection_t           *collection,
                                                                      const bson_t                  *command,
                                                                      const mongoc_read_prefs_t     *read_prefs,
                                                                      bson_t                        *reply,
                                                                      bson_error_t                  *error);
int64_t                       mongoc_collection_count                (mongoc_collection_t           *collection,
                                                                      mongoc_query_flags_t           flags,
                                                                      const bson_t                  *query,
                                                                      int64_t                        skip,
                                                                      int64_t                        limit,
                                                                      const mongoc_read_prefs_t     *read_prefs,
                                                                      bson_error_t                  *error);
bool                          mongoc_collection_drop                 (mongoc_collection_t           *collection,
                                                                      bson_error_t                  *error);
bool                          mongoc_collection_drop_index           (mongoc_collection_t           *collection,
                                                                      const char                    *index_name,
                                                                      bson_error_t                  *error);
bool                          mongoc_collection_create_index         (mongoc_collection_t           *collection,
                                                                      const bson_t                  *keys,
                                                                      const mongoc_index_opt_t      *opt,
                                                                      bson_error_t                  *error);
bool                          mongoc_collection_ensure_index         (mongoc_collection_t           *collection,
                                                                      const bson_t                  *keys,
                                                                      const mongoc_index_opt_t      *opt,
                                                                      bson_error_t                  *error) BSON_GNUC_DEPRECATED_FOR (mongoc_collection_create_index);
mongoc_cursor_t              *mongoc_collection_find                 (mongoc_collection_t           *collection,
                                                                      mongoc_query_flags_t           flags,
                                                                      uint32_t                       skip,
                                                                      uint32_t                       limit,
                                                                      uint32_t                       batch_size,
                                                                      const bson_t                  *query,
                                                                      const bson_t                  *fields,
                                                                      const mongoc_read_prefs_t     *read_prefs) BSON_GNUC_WARN_UNUSED_RESULT;
bool                          mongoc_collection_insert               (mongoc_collection_t           *collection,
                                                                      mongoc_insert_flags_t          flags,
                                                                      const bson_t                  *document,
                                                                      const mongoc_write_concern_t  *write_concern,
                                                                      bson_error_t                  *error);
bool                          mongoc_collection_insert_bulk          (mongoc_collection_t           *collection,
                                                                      mongoc_insert_flags_t          flags,
                                                                      const bson_t                 **documents,
                                                                      uint32_t                       n_documents,
                                                                      const mongoc_write_concern_t  *write_concern,
                                                                      bson_error_t                  *error) BSON_GNUC_DEPRECATED_FOR (mongoc_collection_create_bulk_operation);
bool                          mongoc_collection_update               (mongoc_collection_t           *collection,
                                                                      mongoc_update_flags_t          flags,
                                                                      const bson_t                  *selector,
                                                                      const bson_t                  *update,
                                                                      const mongoc_write_concern_t  *write_concern,
                                                                      bson_error_t                  *error);
bool                          mongoc_collection_delete               (mongoc_collection_t           *collection,
                                                                      mongoc_delete_flags_t          flags,
                                                                      const bson_t                  *selector,
                                                                      const mongoc_write_concern_t  *write_concern,
                                                                      bson_error_t                  *error) BSON_GNUC_DEPRECATED_FOR (mongoc_collection_remove);
bool                          mongoc_collection_save                 (mongoc_collection_t           *collection,
                                                                      const bson_t                  *document,
                                                                      const mongoc_write_concern_t  *write_concern,
                                                                      bson_error_t                  *error);
bool                          mongoc_collection_remove               (mongoc_collection_t           *collection,
                                                                      mongoc_remove_flags_t          flags,
                                                                      const bson_t                  *selector,
                                                                      const mongoc_write_concern_t  *write_concern,
                                                                      bson_error_t                  *error);
bool                          mongoc_collection_rename               (mongoc_collection_t           *collection,
                                                                      const char                    *new_db,
                                                                      const char                    *new_name,
                                                                      bool                           drop_target_before_rename,
                                                                      bson_error_t                  *error);
bool                          mongoc_collection_find_and_modify      (mongoc_collection_t           *collection,
                                                                      const bson_t                  *query,
                                                                      const bson_t                  *sort,
                                                                      const bson_t                  *update,
                                                                      const bson_t                  *fields,
                                                                      bool                           _remove,
                                                                      bool                           upsert,
                                                                      bool                           _new,
                                                                      bson_t                        *reply,
                                                                      bson_error_t                  *error);
bool                          mongoc_collection_stats                (mongoc_collection_t           *collection,
                                                                      const bson_t                  *options,
                                                                      bson_t                        *reply,
                                                                      bson_error_t                  *error);
mongoc_bulk_operation_t      *mongoc_collection_create_bulk_operation(mongoc_collection_t           *collection,
                                                                      bool                           ordered,
                                                                      const mongoc_write_concern_t  *write_concern) BSON_GNUC_WARN_UNUSED_RESULT;
const mongoc_read_prefs_t    *mongoc_collection_get_read_prefs       (const mongoc_collection_t     *collection);
void                          mongoc_collection_set_read_prefs       (mongoc_collection_t           *collection,
                                                                      const mongoc_read_prefs_t     *read_prefs);
const mongoc_write_concern_t *mongoc_collection_get_write_concern    (const mongoc_collection_t     *collection);
void                          mongoc_collection_set_write_concern    (mongoc_collection_t           *collection,
                                                                      const mongoc_write_concern_t  *write_concern);
const char                   *mongoc_collection_get_name             (mongoc_collection_t           *collection);
const bson_t                 *mongoc_collection_get_last_error       (const mongoc_collection_t     *collection);
char                         *mongoc_collection_keys_to_index_string (const bson_t                  *keys);
bool                          mongoc_collection_validate             (mongoc_collection_t           *collection,
                                                                      const bson_t                  *options,
                                                                      bson_t                        *reply,
                                                                      bson_error_t                  *error);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-database.h */

BSON_BEGIN_DECLS


typedef struct _mongoc_database_t mongoc_database_t;


const char                   *mongoc_database_get_name             (mongoc_database_t            *database);
bool                          mongoc_database_remove_user          (mongoc_database_t            *database,
                                                                    const char                   *username,
                                                                    bson_error_t                 *error);
bool                          mongoc_database_remove_all_users     (mongoc_database_t            *database,
                                                                    bson_error_t                 *error);
bool                          mongoc_database_add_user             (mongoc_database_t            *database,
                                                                    const char                   *username,
                                                                    const char                   *password,
                                                                    const bson_t                 *roles,
                                                                    const bson_t                 *custom_data,
                                                                    bson_error_t                 *error);
void                          mongoc_database_destroy              (mongoc_database_t            *database);
mongoc_cursor_t              *mongoc_database_command              (mongoc_database_t            *database,
                                                                    mongoc_query_flags_t          flags,
                                                                    uint32_t                      skip,
                                                                    uint32_t                      limit,
                                                                    uint32_t                      batch_size,
                                                                    const bson_t                 *command,
                                                                    const bson_t                 *fields,
                                                                    const mongoc_read_prefs_t    *read_prefs);
bool                          mongoc_database_command_simple       (mongoc_database_t            *database,
                                                                    const bson_t                 *command,
                                                                    const mongoc_read_prefs_t    *read_prefs,
                                                                    bson_t                       *reply,
                                                                    bson_error_t                 *error);
bool                          mongoc_database_drop                 (mongoc_database_t            *database,
                                                                    bson_error_t                 *error);
bool                          mongoc_database_has_collection       (mongoc_database_t            *database,
                                                                    const char                   *name,
                                                                    bson_error_t                 *error);
mongoc_collection_t          *mongoc_database_create_collection    (mongoc_database_t            *database,
                                                                    const char                   *name,
                                                                    const bson_t                 *options,
                                                                    bson_error_t                 *error);
const mongoc_read_prefs_t    *mongoc_database_get_read_prefs       (const mongoc_database_t      *database);
void                          mongoc_database_set_read_prefs       (mongoc_database_t            *database,
                                                                    const mongoc_read_prefs_t    *read_prefs);
const mongoc_write_concern_t *mongoc_database_get_write_concern    (const mongoc_database_t      *database);
void                          mongoc_database_set_write_concern    (mongoc_database_t            *database,
                                                                    const mongoc_write_concern_t *write_concern);
char                        **mongoc_database_get_collection_names (mongoc_database_t            *database,
                                                                    bson_error_t                 *error);
mongoc_collection_t          *mongoc_database_get_collection       (mongoc_database_t            *database,
                                                                    const char                   *name);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-gridfs-file.h */

BSON_BEGIN_DECLS


#define MONGOC_GRIDFS_FILE_STR_HEADER(name) \
   const char * \
   mongoc_gridfs_file_get_##name (mongoc_gridfs_file_t * file); \
   void \
      mongoc_gridfs_file_set_##name (mongoc_gridfs_file_t * file, \
                                     const char           *str);


#define MONGOC_GRIDFS_FILE_BSON_HEADER(name) \
   const bson_t * \
   mongoc_gridfs_file_get_##name (mongoc_gridfs_file_t * file); \
   void \
      mongoc_gridfs_file_set_##name (mongoc_gridfs_file_t * file, \
                                     const bson_t * bson);


typedef struct _mongoc_gridfs_file_t     mongoc_gridfs_file_t;
typedef struct _mongoc_gridfs_file_opt_t mongoc_gridfs_file_opt_t;


struct _mongoc_gridfs_file_opt_t
{
   const char   *md5;
   const char   *filename;
   const char   *content_type;
   const bson_t *aliases;
   const bson_t *metadata;
   uint32_t      chunk_size;
};


MONGOC_GRIDFS_FILE_STR_HEADER (md5)
MONGOC_GRIDFS_FILE_STR_HEADER (filename)
MONGOC_GRIDFS_FILE_STR_HEADER (content_type)
MONGOC_GRIDFS_FILE_BSON_HEADER (aliases)
MONGOC_GRIDFS_FILE_BSON_HEADER (metadata)


int64_t  mongoc_gridfs_file_get_length      (mongoc_gridfs_file_t *file);
int32_t  mongoc_gridfs_file_get_chunk_size  (mongoc_gridfs_file_t *file);
int64_t  mongoc_gridfs_file_get_upload_date (mongoc_gridfs_file_t *file);
ssize_t  mongoc_gridfs_file_writev          (mongoc_gridfs_file_t *file,
                                             mongoc_iovec_t       *iov,
                                             size_t                iovcnt,
                                             uint32_t              timeout_msec);
ssize_t  mongoc_gridfs_file_readv           (mongoc_gridfs_file_t *file,
                                             mongoc_iovec_t       *iov,
                                             size_t                iovcnt,
                                             size_t                min_bytes,
                                             uint32_t              timeout_msec);
int      mongoc_gridfs_file_seek            (mongoc_gridfs_file_t *file,
                                             uint64_t              delta,
                                             int                   whence);
uint64_t mongoc_gridfs_file_tell            (mongoc_gridfs_file_t *file);
bool     mongoc_gridfs_file_save            (mongoc_gridfs_file_t *file);
void     mongoc_gridfs_file_destroy         (mongoc_gridfs_file_t *file);
bool     mongoc_gridfs_file_error           (mongoc_gridfs_file_t *file,
                                             bson_error_t         *error);
bool     mongoc_gridfs_file_remove          (mongoc_gridfs_file_t *file,
                                             bson_error_t         *error);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-gridfs-file-list.h */

BSON_BEGIN_DECLS


typedef struct _mongoc_gridfs_file_list_t mongoc_gridfs_file_list_t;


mongoc_gridfs_file_t *mongoc_gridfs_file_list_next    (mongoc_gridfs_file_list_t *list);
void                  mongoc_gridfs_file_list_destroy (mongoc_gridfs_file_list_t *list);
bool                  mongoc_gridfs_file_list_error   (mongoc_gridfs_file_list_t *list,
                                                       bson_error_t              *error);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-gridfs-file-page.h */

BSON_BEGIN_DECLS


typedef struct _mongoc_gridfs_file_page_t mongoc_gridfs_file_page_t;


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-gridfs.h */

BSON_BEGIN_DECLS


typedef struct _mongoc_gridfs_t mongoc_gridfs_t;


mongoc_gridfs_file_t      *mongoc_gridfs_create_file_from_stream (mongoc_gridfs_t          *gridfs,
                                                                  mongoc_stream_t          *stream,
                                                                  mongoc_gridfs_file_opt_t *opt);
mongoc_gridfs_file_t      *mongoc_gridfs_create_file             (mongoc_gridfs_t          *gridfs,
                                                                  mongoc_gridfs_file_opt_t *opt);
mongoc_gridfs_file_list_t *mongoc_gridfs_find                    (mongoc_gridfs_t          *gridfs,
                                                                  const bson_t             *query);
mongoc_gridfs_file_t      *mongoc_gridfs_find_one                (mongoc_gridfs_t          *gridfs,
                                                                  const bson_t             *query,
                                                                  bson_error_t             *error);
mongoc_gridfs_file_t      *mongoc_gridfs_find_one_by_filename    (mongoc_gridfs_t          *gridfs,
                                                                  const char               *filename,
                                                                  bson_error_t             *error);
bool                       mongoc_gridfs_drop                    (mongoc_gridfs_t          *gridfs,
                                                                  bson_error_t             *error);
void                       mongoc_gridfs_destroy                 (mongoc_gridfs_t          *gridfs);
mongoc_collection_t       *mongoc_gridfs_get_files               (mongoc_gridfs_t          *gridfs);
mongoc_collection_t       *mongoc_gridfs_get_chunks              (mongoc_gridfs_t          *gridfs);
bool                       mongoc_gridfs_remove_by_filename      (mongoc_gridfs_t          *gridfs,
                                                                  const char               *filename,
                                                                  bson_error_t             *error);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-stream-gridfs.h */

BSON_BEGIN_DECLS


mongoc_stream_t *mongoc_stream_gridfs_new (mongoc_gridfs_file_t *file);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-client.h */

BSON_BEGIN_DECLS


#define MONGOC_NAMESPACE_MAX 128


/**
 * mongoc_client_t:
 *
 * The mongoc_client_t structure maintains information about a connection to
 * a MongoDB server.
 */
typedef struct _mongoc_client_t mongoc_client_t;


/**
 * mongoc_stream_initiator_t:
 * @uri: The uri and options for the stream.
 * @host: The host and port (or UNIX domain socket path) to connect to.
 * @error: A location for an error.
 *
 * Creates a new mongoc_stream_t for the host and port. This can be used
 * by language bindings to create network transports other than those
 * built into libmongoc. An example of such would be the streams API
 * provided by PHP.
 *
 * Returns: A newly allocated mongoc_stream_t or NULL on failure.
 */
typedef mongoc_stream_t *(*mongoc_stream_initiator_t) (const mongoc_uri_t       *uri,
                                                       const mongoc_host_list_t *host,
                                                       void                     *user_data,
                                                       bson_error_t             *error);


mongoc_client_t               *mongoc_client_new                  (const char                   *uri_string);
mongoc_client_t               *mongoc_client_new_from_uri         (const mongoc_uri_t           *uri);
const mongoc_uri_t            *mongoc_client_get_uri              (const mongoc_client_t        *client);
void                           mongoc_client_set_stream_initiator (mongoc_client_t              *client,
                                                                   mongoc_stream_initiator_t     initiator,
                                                                   void                         *user_data);
mongoc_cursor_t               *mongoc_client_command              (mongoc_client_t              *client,
                                                                   const char                   *db_name,
                                                                   mongoc_query_flags_t          flags,
                                                                   uint32_t                      skip,
                                                                   uint32_t                      limit,
                                                                   uint32_t                      batch_size,
                                                                   const bson_t                 *query,
                                                                   const bson_t                 *fields,
                                                                   const mongoc_read_prefs_t    *read_prefs);
bool                           mongoc_client_command_simple       (mongoc_client_t              *client,
                                                                   const char                   *db_name,
                                                                   const bson_t                 *command,
                                                                   const mongoc_read_prefs_t    *read_prefs,
                                                                   bson_t                       *reply,
                                                                   bson_error_t                 *error);
void                           mongoc_client_destroy              (mongoc_client_t              *client);
mongoc_database_t             *mongoc_client_get_database         (mongoc_client_t              *client,
                                                                   const char                   *name);
mongoc_gridfs_t               *mongoc_client_get_gridfs           (mongoc_client_t              *client,
                                                                   const char                   *db,
                                                                   const char                   *prefix,
                                                                   bson_error_t                 *error);
mongoc_collection_t           *mongoc_client_get_collection       (mongoc_client_t              *client,
                                                                   const char                   *db,
                                                                   const char                   *collection);
char                         **mongoc_client_get_database_names   (mongoc_client_t              *client,
                                                                   bson_error_t                 *error);
bool                           mongoc_client_get_server_status    (mongoc_client_t              *client,
                                                                   mongoc_read_prefs_t          *read_prefs,
                                                                   bson_t                       *reply,
                                                                   bson_error_t                 *error);
int32_t                        mongoc_client_get_max_message_size (mongoc_client_t              *client);
int32_t                        mongoc_client_get_max_bson_size    (mongoc_client_t              *client);
const mongoc_write_concern_t  *mongoc_client_get_write_concern    (const mongoc_client_t        *client);
void                           mongoc_client_set_write_concern    (mongoc_client_t              *client,
                                                                   const mongoc_write_concern_t *write_concern);
const mongoc_read_prefs_t     *mongoc_client_get_read_prefs       (const mongoc_client_t        *client);
void                           mongoc_client_set_read_prefs       (mongoc_client_t              *client,
                                                                   const mongoc_read_prefs_t    *read_prefs);
#ifdef MONGOC_ENABLE_SSL
void                           mongoc_client_set_ssl_opts         (mongoc_client_t              *client,
                                                                   const mongoc_ssl_opt_t       *opts);
#endif


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-client-pool.h */

BSON_BEGIN_DECLS


typedef struct _mongoc_client_pool_t mongoc_client_pool_t;


mongoc_client_pool_t *mongoc_client_pool_new     (const mongoc_uri_t   *uri);
void                  mongoc_client_pool_destroy (mongoc_client_pool_t *pool);
mongoc_client_t      *mongoc_client_pool_pop     (mongoc_client_pool_t *pool);
void                  mongoc_client_pool_push    (mongoc_client_pool_t *pool,
                                                  mongoc_client_t      *client);
mongoc_client_t      *mongoc_client_pool_try_pop (mongoc_client_pool_t *pool);
#ifdef MONGOC_ENABLE_SSL
void                  mongoc_client_pool_set_ssl_opts (mongoc_client_pool_t   *pool,
                                                       const mongoc_ssl_opt_t *opts);
#endif


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-array-private.h */

BSON_BEGIN_DECLS


typedef struct _mongoc_array_t mongoc_array_t;


struct _mongoc_array_t
{
   size_t  len;
   size_t  element_size;
   size_t  allocated;
   void   *data;
};


#define _mongoc_array_append_val(a, v) _mongoc_array_append_vals(a, &v, 1)
#define _mongoc_array_index(a, t, i)   (((t*)(a)->data)[i])
#define _mongoc_array_clear(a)         (a)->len = 0


void _mongoc_array_init        (mongoc_array_t *array,
                                size_t          element_size);
void _mongoc_array_append_vals (mongoc_array_t *array,
                                const void     *data,
                                uint32_t        n_elements);
void _mongoc_array_destroy     (mongoc_array_t *array);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-buffer-private.h */

BSON_BEGIN_DECLS


typedef struct _mongoc_buffer_t mongoc_buffer_t;


struct _mongoc_buffer_t
{
   uint8_t            *data;
   size_t              datalen;
   off_t               off;
   size_t              len;
   bson_realloc_func   realloc_func;
   void               *realloc_data;
};


void
_mongoc_buffer_init (mongoc_buffer_t   *buffer,
                     uint8_t           *buf,
                     size_t             buflen,
                     bson_realloc_func  realloc_func,
                     void              *realloc_data);

bool
_mongoc_buffer_append_from_stream (mongoc_buffer_t *buffer,
                                   mongoc_stream_t *stream,
                                   size_t           size,
                                   int32_t     timeout_msec,
                                   bson_error_t    *error);

ssize_t
_mongoc_buffer_fill (mongoc_buffer_t *buffer,
                     mongoc_stream_t *stream,
                     size_t           min_bytes,
                     int32_t          timeout_msec,
                     bson_error_t    *error);

void
_mongoc_buffer_destroy (mongoc_buffer_t *buffer);

void
_mongoc_buffer_clear (mongoc_buffer_t *buffer,
                      bool      zero);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-rpc-private.h */

BSON_BEGIN_DECLS


#define RPC(_name, _code)                typedef struct { _code } mongoc_rpc_##_name##_t;
#define ENUM_FIELD(_name)                uint32_t _name;
#define INT32_FIELD(_name)               int32_t _name;
#define INT64_FIELD(_name)               int64_t _name;
#define INT64_ARRAY_FIELD(_len, _name)   int32_t _len; int64_t *_name;
#define CSTRING_FIELD(_name)             const char *_name;
#define BSON_FIELD(_name)                const uint8_t *_name;
#define BSON_ARRAY_FIELD(_name)          const uint8_t *_name; int32_t _name##_len;
#define IOVEC_ARRAY_FIELD(_name)         const mongoc_iovec_t *_name; int32_t n_##_name; mongoc_iovec_t _name##_recv;
#define RAW_BUFFER_FIELD(_name)          const uint8_t *_name; int32_t _name##_len;
#define BSON_OPTIONAL(_check, _code)     _code


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


typedef union
{
   mongoc_rpc_delete_t       delete;
   mongoc_rpc_get_more_t     get_more;
   mongoc_rpc_header_t       header;
   mongoc_rpc_insert_t       insert;
   mongoc_rpc_kill_cursors_t kill_cursors;
   mongoc_rpc_msg_t          msg;
   mongoc_rpc_query_t        query;
   mongoc_rpc_reply_t        reply;
   mongoc_rpc_update_t       update;
} mongoc_rpc_t;


BSON_STATIC_ASSERT (sizeof (mongoc_rpc_header_t) == 16);

#if !defined(offsetof)
#define offsetof(type, member) ( (long) & ((type*)0) -> member )
#endif
BSON_STATIC_ASSERT (offsetof (mongoc_rpc_header_t, opcode) ==
                    offsetof (mongoc_rpc_reply_t, opcode));


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


void _mongoc_rpc_gather          (mongoc_rpc_t                 *rpc,
                                  mongoc_array_t               *array);
bool _mongoc_rpc_needs_gle       (mongoc_rpc_t                 *rpc,
                                  const mongoc_write_concern_t *write_concern);
void _mongoc_rpc_swab_to_le      (mongoc_rpc_t                 *rpc);
void _mongoc_rpc_swab_from_le    (mongoc_rpc_t                 *rpc);
void _mongoc_rpc_printf          (mongoc_rpc_t                 *rpc);
bool _mongoc_rpc_scatter         (mongoc_rpc_t                 *rpc,
                                  const uint8_t                *buf,
                                  size_t                        buflen);
bool _mongoc_rpc_reply_get_first (mongoc_rpc_reply_t           *reply,
                                  bson_t                       *bson);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-collection-private.h */

BSON_BEGIN_DECLS


struct _mongoc_collection_t
{
   mongoc_client_t        *client;
   char                    ns[128];
   uint32_t                nslen;
   char                    db[128];
   char                    collection[128];
   uint32_t                collectionlen;
   mongoc_buffer_t         buffer;
   mongoc_read_prefs_t    *read_prefs;
   mongoc_write_concern_t *write_concern;
   bson_t                 *gle;
};


mongoc_collection_t *_mongoc_collection_new (mongoc_client_t              *client,
                                             const char                   *db,
                                             const char                   *collection,
                                             const mongoc_read_prefs_t    *read_prefs,
                                             const mongoc_write_concern_t *write_concern);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-counters-private.h */

BSON_BEGIN_DECLS


void _mongoc_counters_init (void);


static BSON_INLINE unsigned
_mongoc_get_cpu_count (void)
{
#if defined(__linux__)
   return get_nprocs ();
#elif defined(__FreeBSD__) || \
      defined(__NetBSD__) || \
      defined(__DragonFly__) || \
      defined(__OpenBSD__)
   int mib[2];
   int maxproc;
   size_t len;

   mib[0] = CTL_HW;
   mib[1] = HW_NCPU;
   len = sizeof (maxproc);

   if (-1 == sysctl (mib, 2, &maxproc, &len, NULL, 0)) {
      return 1;
   }

   return len;
#elif defined(__APPLE__) || defined(__sun)
   int ncpu;

   ncpu = (int) sysconf (_SC_NPROCESSORS_ONLN);
   return (ncpu > 0) ? ncpu : 1;
#elif defined(_MSC_VER) || defined(_WIN32)
   SYSTEM_INFO si;
   GetSystemInfo (&si);
   return si.dwNumberOfProcessors;
#else
# warning "_mongoc_get_cpu_count() not supported, defaulting to 1."
   return 1;
#endif
}


#define _mongoc_counter_add(v,count) \
   bson_atomic_int64_add(&(v), (count))


#if defined(ENABLE_RDTSCP)
 static BSON_INLINE unsigned
 _mongoc_sched_getcpu (void)
 {
    volatile uint32_t rax, rdx, aux;
    __asm__ volatile ("rdtscp\n" : "=a" (rax), "=d" (rdx), "=c" (aux) : : );
    return aux;
 }
#elif defined(HAVE_SCHED_GETCPU)
# define _mongoc_sched_getcpu sched_getcpu
#else
# define _mongoc_sched_getcpu() (0)
#endif


#ifndef SLOTS_PER_CACHELINE
# define SLOTS_PER_CACHELINE 8
#endif


typedef struct
{
   int64_t slots [SLOTS_PER_CACHELINE];
} mongoc_counter_slots_t;


typedef struct
{
   mongoc_counter_slots_t *cpus;
} mongoc_counter_t;


#define COUNTER(ident, Category, Name, Description) \
   extern mongoc_counter_t __mongoc_counter_##ident;
#include "mongoc-counters.defs"
#undef COUNTER


enum
{
#define COUNTER(ident, Category, Name, Description) \
   COUNTER_##ident,
#include "mongoc-counters.defs"
#undef COUNTER
   LAST_COUNTER
};


#define COUNTER(ident, Category, Name, Description) \
static BSON_INLINE void \
mongoc_counter_##ident##_add (int64_t val) \
{ \
   _mongoc_counter_add(\
      __mongoc_counter_##ident.cpus[_mongoc_sched_getcpu()].slots[ \
         COUNTER_##ident%SLOTS_PER_CACHELINE], val); \
} \
static BSON_INLINE void \
mongoc_counter_##ident##_inc (void) \
{ \
   mongoc_counter_##ident##_add (1); \
} \
static BSON_INLINE void \
mongoc_counter_##ident##_dec (void) \
{ \
   mongoc_counter_##ident##_add (-1); \
} \
static BSON_INLINE void \
mongoc_counter_##ident##_reset (void) \
{ \
   uint32_t i; \
   for (i = 0; i < _mongoc_get_cpu_count(); i++) { \
      __mongoc_counter_##ident.cpus [i].slots [\
         COUNTER_##ident%SLOTS_PER_CACHELINE] = 0; \
   } \
   bson_memory_barrier (); \
}
#include "mongoc-counters.defs"
#undef COUNTER


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-cursor-private.h */

BSON_BEGIN_DECLS


typedef struct _mongoc_cursor_interface_t mongoc_cursor_interface_t;


struct _mongoc_cursor_interface_t
{
   mongoc_cursor_t *(*clone)    (const mongoc_cursor_t  *cursor);
   void             (*destroy)  (mongoc_cursor_t        *cursor);
   bool             (*more)     (mongoc_cursor_t        *cursor);
   bool             (*next)     (mongoc_cursor_t        *cursor,
                                 const bson_t          **bson);
   bool             (*error)    (mongoc_cursor_t        *cursor,
                                 bson_error_t           *error);
   void             (*get_host) (mongoc_cursor_t        *cursor,
                                 mongoc_host_list_t     *host);
};


struct _mongoc_cursor_t
{
   mongoc_client_t           *client;

   uint32_t                   hint;
   uint32_t                   stamp;

   unsigned                   is_command   : 1;
   unsigned                   sent         : 1;
   unsigned                   done         : 1;
   unsigned                   failed       : 1;
   unsigned                   end_of_event : 1;
   unsigned                   in_exhaust   : 1;
   unsigned                   redir_primary: 1;
   unsigned                   has_fields   : 1;

   bson_t                     query;
   bson_t                     fields;

   mongoc_read_prefs_t       *read_prefs;

   mongoc_query_flags_t       flags;
   uint32_t                   skip;
   uint32_t                   limit;
   uint32_t                   count;
   uint32_t                   batch_size;

   char                       ns [140];
   uint32_t                   nslen;

   bson_error_t               error;

   mongoc_rpc_t               rpc;
   mongoc_buffer_t            buffer;
   bson_reader_t             *reader;

   const bson_t              *current;

   mongoc_cursor_interface_t  iface;
   void                      *iface_data;
};


mongoc_cursor_t * _mongoc_cursor_new      (mongoc_client_t            *client,
                                           const char                 *db_and_collection,
                                           mongoc_query_flags_t        flags,
                                           uint32_t                    skip,
                                           uint32_t                    limit,
                                           uint32_t                    batch_size,
                                           bool                        is_command,
                                           const bson_t               *query,
                                           const bson_t               *fields,
                                           const mongoc_read_prefs_t  *read_prefs);
mongoc_cursor_t *_mongoc_cursor_clone     (const mongoc_cursor_t      *cursor);
void             _mongoc_cursor_destroy   (mongoc_cursor_t            *cursor);
bool             _mongoc_cursor_more      (mongoc_cursor_t            *cursor);
bool             _mongoc_cursor_next      (mongoc_cursor_t            *cursor,
                                           const bson_t              **bson);
bool             _mongoc_cursor_error     (mongoc_cursor_t            *cursor,
                                           bson_error_t               *error);
void             _mongoc_cursor_get_host  (mongoc_cursor_t            *cursor,
                                           mongoc_host_list_t         *host);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-cursor-cursorid-private.h */

BSON_BEGIN_DECLS


void _mongoc_cursor_cursorid_init (mongoc_cursor_t *cursor);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-cursor-array-private.h */

BSON_BEGIN_DECLS


void _mongoc_cursor_array_init (mongoc_cursor_t *cursor);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-errno-private.h */

BSON_BEGIN_DECLS


#if defined(_WIN32)
# define MONGOC_ERRNO_IS_AGAIN(errno) ((errno == EAGAIN) || (errno == WSAEWOULDBLOCK) || (errno == WSAEINPROGRESS))
#elif defined(__sun)
/* for some reason, accept() returns -1 and errno of 0 */
# define MONGOC_ERRNO_IS_AGAIN(errno) ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINPROGRESS) || (errno == 0))
#else
# define MONGOC_ERRNO_IS_AGAIN(errno) ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINPROGRESS))
#endif


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-gridfs-file-list-private.h */

BSON_BEGIN_DECLS


struct _mongoc_gridfs_file_list_t
{
   mongoc_gridfs_t *gridfs;
   mongoc_cursor_t *cursor;
   bson_error_t     error;
};


mongoc_gridfs_file_list_t *_mongoc_gridfs_file_list_new (mongoc_gridfs_t *gridfs,
                                                         const bson_t    *query,
                                                         uint32_t         limit);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-gridfs-file-page-private.h */

BSON_BEGIN_DECLS


struct _mongoc_gridfs_file_page_t
{
   const uint8_t *read_buf;
   uint8_t       *buf;
   uint32_t       len;
   uint32_t       chunk_size;
   uint32_t       offset;
};


mongoc_gridfs_file_page_t *_mongoc_gridfs_file_page_new      (const uint8_t             *data,
                                                              uint32_t                   len,
                                                              uint32_t                   chunk_size);
void                       _mongoc_gridfs_file_page_destroy  (mongoc_gridfs_file_page_t *page);
bool                       _mongoc_gridfs_file_page_seek     (mongoc_gridfs_file_page_t *page,
                                                              uint32_t                   offset);
int32_t                    _mongoc_gridfs_file_page_read     (mongoc_gridfs_file_page_t *page,
                                                              void                      *dst,
                                                              uint32_t                   len);
int32_t                    _mongoc_gridfs_file_page_write    (mongoc_gridfs_file_page_t *page,
                                                              const void                *src,
                                                              uint32_t                   len);
uint32_t                   _mongoc_gridfs_file_page_tell     (mongoc_gridfs_file_page_t *page);
const uint8_t             *_mongoc_gridfs_file_page_get_data (mongoc_gridfs_file_page_t *page);
uint32_t                   _mongoc_gridfs_file_page_get_len  (mongoc_gridfs_file_page_t *page);
bool                       _mongoc_gridfs_file_page_is_dirty (mongoc_gridfs_file_page_t *page);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-gridfs-file-private.h */

BSON_BEGIN_DECLS


struct _mongoc_gridfs_file_t
{
   mongoc_gridfs_t           *gridfs;
   bson_t                     bson;
   mongoc_gridfs_file_page_t *page;
   uint64_t                   pos;
   bson_error_t               error;
   bool                       failed;
   mongoc_cursor_t           *cursor;
   uint32_t                   cursor_range[2];
   bool                       is_dirty;

   bson_value_t               files_id;
   int64_t                    length;
   int32_t                    chunk_size;
   int64_t                    upload_date;

   char                      *md5;
   char                      *filename;
   char                      *content_type;
   bson_t                     aliases;
   bson_t                     metadata;
   const char                *bson_md5;
   const char                *bson_filename;
   const char                *bson_content_type;
   bson_t                     bson_aliases;
   bson_t                     bson_metadata;
};


mongoc_gridfs_file_t *_mongoc_gridfs_file_new_from_bson (mongoc_gridfs_t          *gridfs,
                                                         const bson_t             *data);
mongoc_gridfs_file_t *_mongoc_gridfs_file_new           (mongoc_gridfs_t          *gridfs,
                                                         mongoc_gridfs_file_opt_t *opt);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-gridfs-private.h */

BSON_BEGIN_DECLS


struct _mongoc_gridfs_t
{
   mongoc_client_t     *client;
   mongoc_collection_t *files;
   mongoc_collection_t *chunks;
};


mongoc_gridfs_t *_mongoc_gridfs_new (mongoc_client_t *client,
                                     const char      *db,
                                     const char      *prefix,
                                     bson_error_t    *error);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-host-list-private.h */

BSON_BEGIN_DECLS


bool _mongoc_host_list_from_string (mongoc_host_list_t *host_list,
                                    const char         *host_and_port);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-list-private.h */

BSON_BEGIN_DECLS


typedef struct _mongoc_list_t mongoc_list_t;


struct _mongoc_list_t
{
   mongoc_list_t *next;
   void          *data;
};


mongoc_list_t *_mongoc_list_append  (mongoc_list_t *list,
                                     void          *data);
mongoc_list_t *_mongoc_list_prepend (mongoc_list_t *list,
                                     void          *data);
mongoc_list_t *_mongoc_list_remove  (mongoc_list_t *list,
                                     void          *data);
void           _mongoc_list_foreach (mongoc_list_t *list,
                                     void (*func) (void *data, void *user_data),
                                     void *         user_data);
void           _mongoc_list_destroy (mongoc_list_t *list);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-matcher-op-private.h */

BSON_BEGIN_DECLS


typedef union  _mongoc_matcher_op_t         mongoc_matcher_op_t;
typedef struct _mongoc_matcher_op_base_t    mongoc_matcher_op_base_t;
typedef struct _mongoc_matcher_op_logical_t mongoc_matcher_op_logical_t;
typedef struct _mongoc_matcher_op_compare_t mongoc_matcher_op_compare_t;
typedef struct _mongoc_matcher_op_exists_t  mongoc_matcher_op_exists_t;
typedef struct _mongoc_matcher_op_type_t    mongoc_matcher_op_type_t;
typedef struct _mongoc_matcher_op_not_t     mongoc_matcher_op_not_t;


typedef enum
{
   MONGOC_MATCHER_OPCODE_EQ,
   MONGOC_MATCHER_OPCODE_GT,
   MONGOC_MATCHER_OPCODE_GTE,
   MONGOC_MATCHER_OPCODE_IN,
   MONGOC_MATCHER_OPCODE_LT,
   MONGOC_MATCHER_OPCODE_LTE,
   MONGOC_MATCHER_OPCODE_NE,
   MONGOC_MATCHER_OPCODE_NIN,
   MONGOC_MATCHER_OPCODE_OR,
   MONGOC_MATCHER_OPCODE_AND,
   MONGOC_MATCHER_OPCODE_NOT,
   MONGOC_MATCHER_OPCODE_NOR,
   MONGOC_MATCHER_OPCODE_EXISTS,
   MONGOC_MATCHER_OPCODE_TYPE,
} mongoc_matcher_opcode_t;


struct _mongoc_matcher_op_base_t
{
   mongoc_matcher_opcode_t opcode;
};


struct _mongoc_matcher_op_logical_t
{
   mongoc_matcher_op_base_t base;
   mongoc_matcher_op_t *left;
   mongoc_matcher_op_t *right;
};


struct _mongoc_matcher_op_compare_t
{
   mongoc_matcher_op_base_t base;
   char *path;
   bson_iter_t iter;
};


struct _mongoc_matcher_op_exists_t
{
   mongoc_matcher_op_base_t base;
   char *path;
   bool exists;
};


struct _mongoc_matcher_op_type_t
{
   mongoc_matcher_op_base_t base;
   bson_type_t type;
   char *path;
};


struct _mongoc_matcher_op_not_t
{
   mongoc_matcher_op_base_t base;
   mongoc_matcher_op_t *child;
   char *path;
};


union _mongoc_matcher_op_t
{
   mongoc_matcher_op_base_t base;
   mongoc_matcher_op_logical_t logical;
   mongoc_matcher_op_compare_t compare;
   mongoc_matcher_op_exists_t exists;
   mongoc_matcher_op_type_t type;
   mongoc_matcher_op_not_t not;
};


mongoc_matcher_op_t *_mongoc_matcher_op_logical_new (mongoc_matcher_opcode_t  opcode,
                                                     mongoc_matcher_op_t     *left,
                                                     mongoc_matcher_op_t     *right);
mongoc_matcher_op_t *_mongoc_matcher_op_compare_new (mongoc_matcher_opcode_t  opcode,
                                                     const char              *path,
                                                     const bson_iter_t       *iter);
mongoc_matcher_op_t *_mongoc_matcher_op_exists_new  (const char              *path,
                                                     bool                     exists);
mongoc_matcher_op_t *_mongoc_matcher_op_type_new    (const char              *path,
                                                     bson_type_t              type);
mongoc_matcher_op_t *_mongoc_matcher_op_not_new     (const char              *path,
                                                     mongoc_matcher_op_t     *child);
bool                 _mongoc_matcher_op_match       (mongoc_matcher_op_t     *op,
                                                     const bson_t            *bson);
void                 _mongoc_matcher_op_destroy     (mongoc_matcher_op_t     *op);
void                 _mongoc_matcher_op_to_bson     (mongoc_matcher_op_t     *op,
                                                     bson_t                  *bson);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-matcher-private.h */

BSON_BEGIN_DECLS


struct _mongoc_matcher_t
{
   bson_t               query;
   mongoc_matcher_op_t *optree;
};


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-queue-private.h */

BSON_BEGIN_DECLS


#define MONGOC_QUEUE_INITIALIZER {NULL,NULL}


typedef struct _mongoc_queue_t      mongoc_queue_t;
typedef struct _mongoc_queue_item_t mongoc_queue_item_t;


struct _mongoc_queue_t
{
   mongoc_queue_item_t *head;
   mongoc_queue_item_t *tail;
};


struct _mongoc_queue_item_t
{
   mongoc_queue_item_t *next;
   void                *data;
};


void      _mongoc_queue_init      (mongoc_queue_t        *queue);
void     *_mongoc_queue_pop_head  (mongoc_queue_t        *queue);
void      _mongoc_queue_push_head (mongoc_queue_t        *queue,
                                    void                 *data);
void      _mongoc_queue_push_tail  (mongoc_queue_t       *queue,
                                    void                 *data);
uint32_t  _mongoc_queue_get_length (const mongoc_queue_t *queue);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-sasl-private.h */

BSON_BEGIN_DECLS


typedef struct _mongoc_sasl_t mongoc_sasl_t;


struct _mongoc_sasl_t
{
   sasl_callback_t  callbacks [4];
   sasl_conn_t     *conn;
   bool      done;
   int              step;
   char            *mechanism;
   char            *user;
   char            *pass;
   char            *service_name;
   char            *service_host;
   sasl_interact_t *interact;
};


void _mongoc_sasl_init             (mongoc_sasl_t      *sasl);
void _mongoc_sasl_set_pass         (mongoc_sasl_t      *sasl,
                                    const char         *pass);
void _mongoc_sasl_set_user         (mongoc_sasl_t      *sasl,
                                    const char         *user);
void _mongoc_sasl_set_mechanism    (mongoc_sasl_t      *sasl,
                                    const char         *mechanism);
void _mongoc_sasl_set_service_name (mongoc_sasl_t      *sasl,
                                    const char         *service_name);
void _mongoc_sasl_set_service_host (mongoc_sasl_t      *sasl,
                                    const char         *service_host);
void _mongoc_sasl_destroy          (mongoc_sasl_t      *sasl);
bool _mongoc_sasl_step             (mongoc_sasl_t      *sasl,
                                    const uint8_t      *inbuf,
                                    uint32_t            inbuflen,
                                    uint8_t            *outbuf,
                                    uint32_t            outbufmax,
                                    uint32_t           *outbuflen,
                                    bson_error_t       *error);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-ssl-private.h */

BSON_BEGIN_DECLS


bool     _mongoc_ssl_check_cert      (SSL              *ssl,
                                      const char       *host,
                                      bool              weak_cert_validation);
SSL_CTX *_mongoc_ssl_ctx_new         (mongoc_ssl_opt_t *opt);
char    *_mongoc_ssl_extract_subject (const char       *filename);
void     _mongoc_ssl_init            (void);
void     _mongoc_ssl_cleanup         (void);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-stream-private.h */

BSON_BEGIN_DECLS


#define MONGOC_STREAM_SOCKET   1
#define MONGOC_STREAM_FILE     2
#define MONGOC_STREAM_BUFFERED 3
#define MONGOC_STREAM_GRIDFS   4
#define MONGOC_STREAM_TLS      5


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-thread-private.h */

#if !defined(_WIN32)
# include <pthread.h>
# define MONGOC_MUTEX_INITIALIZER       PTHREAD_MUTEX_INITIALIZER
# define mongoc_cond_t                  pthread_cond_t
# define mongoc_cond_init(_n)           pthread_cond_init((_n), NULL)
# define mongoc_cond_wait               pthread_cond_wait
# define mongoc_cond_signal             pthread_cond_signal
# define mongoc_cond_destroy            pthread_cond_destroy
# define mongoc_mutex_t                 pthread_mutex_t
# define mongoc_mutex_init(_n)          pthread_mutex_init((_n), NULL)
# define mongoc_mutex_lock              pthread_mutex_lock
# define mongoc_mutex_unlock            pthread_mutex_unlock
# define mongoc_mutex_destroy           pthread_mutex_destroy
# define mongoc_thread_t                pthread_t
# define mongoc_thread_create(_t,_f,_d) pthread_create((_t), NULL, (_f), (_d))
# define mongoc_thread_join(_n)         pthread_join((_n), NULL)
# define mongoc_once_t                  pthread_once_t
# define mongoc_once                    pthread_once
# define MONGOC_ONCE_FUN(n)             void n(void)
# define MONGOC_ONCE_RETURN             return
# ifdef _PTHREAD_ONCE_INIT_NEEDS_BRACES
#  define MONGOC_ONCE_INIT              {PTHREAD_ONCE_INIT}
# else
#  define MONGOC_ONCE_INIT              PTHREAD_ONCE_INIT
# endif
#else
# define mongoc_thread_t                HANDLE
static BSON_INLINE int
mongoc_thread_create (mongoc_thread_t *thread,
                      void *(*cb)(void *),
                      void            *arg)
{
   *thread = CreateThread(NULL, 0, (void *)cb, arg, 0, NULL);
   return 0;
}
# define mongoc_thread_join(_n)         WaitForSingleObject((_n), INFINITE)
# define mongoc_mutex_t                 CRITICAL_SECTION
# define mongoc_mutex_init              InitializeCriticalSection
# define mongoc_mutex_lock              EnterCriticalSection
# define mongoc_mutex_unlock            LeaveCriticalSection
# define mongoc_mutex_destroy           DeleteCriticalSection
# define mongoc_cond_t                  CONDITION_VARIABLE
# define mongoc_cond_init               InitializeConditionVariable
# define mongoc_cond_wait(_c, _m)       SleepConditionVariableCS((_c), (_m), INFINITE)
# define mongoc_cond_signal             WakeConditionVariable
static BSON_INLINE int
mongoc_cond_destroy (mongoc_cond_t *_ignored)
{
   return 0;
}
# define mongoc_once_t                  INIT_ONCE
# define MONGOC_ONCE_INIT               INIT_ONCE_STATIC_INIT
# define mongoc_once(o, c)              InitOnceExecuteOnce(o, c, NULL, NULL)
# define MONGOC_ONCE_FUN(n)             BOOL CALLBACK n(PINIT_ONCE _ignored_a, PVOID _ignored_b, PVOID *_ignored_c)
# define MONGOC_ONCE_RETURN             return true
#endif

/*==============================================================*/
/* --- mongoc-util-private.h */

BSON_BEGIN_DECLS


char *_mongoc_hex_md5 (const char *input);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-write-concern-private.h */

BSON_BEGIN_DECLS


struct _mongoc_write_concern_t
{
   bool      fsync_;
   bool      journal;
   int32_t   w;
   int32_t   wtimeout;
   char     *wtag;
   bool      frozen;
   bson_t    compiled;
   bson_t    compiled_gle;
};


const bson_t *_mongoc_write_concern_get_gle   (mongoc_write_concern_t       *write_cocnern);
const bson_t *_mongoc_write_concern_get_bson  (mongoc_write_concern_t       *write_concern);
bool          _mongoc_write_concern_needs_gle (const mongoc_write_concern_t *write_concern);

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-write-command-private.h */

BSON_BEGIN_DECLS


#define MONGOC_WRITE_COMMAND_DELETE 0
#define MONGOC_WRITE_COMMAND_INSERT 1
#define MONGOC_WRITE_COMMAND_UPDATE 2


typedef struct
{
   int type;
   uint32_t hint;
   union {
      struct {
         uint8_t   ordered : 1;
         uint8_t   multi : 1;
         bson_t   *selector;
      } delete;
      struct {
         uint8_t   ordered : 1;
         uint8_t   allow_bulk_op_insert : 1;
         bson_t   *documents;
         uint32_t  n_documents;
         uint32_t  n_merged;
         uint32_t  current_n_documents;
      } insert;
      struct {
         uint8_t   ordered : 1;
         uint8_t   upsert : 1;
         uint8_t   multi  : 1;
         bson_t   *selector;
         bson_t   *update;
      } update;
   } u;
} mongoc_write_command_t;


typedef struct
{
   bool         omit_nModified;
   uint32_t     nInserted;
   uint32_t     nMatched;
   uint32_t     nModified;
   uint32_t     nRemoved;
   uint32_t     nUpserted;
   uint32_t     offset;
   uint32_t     n_commands;
   bson_t       upserted;
   bson_t       writeErrors;
   bson_t       writeConcernErrors;
   bool         failed;
   bson_error_t error;
   uint32_t     upsert_append_count;
} mongoc_write_result_t;


void _mongoc_write_command_destroy     (mongoc_write_command_t        *command);
void _mongoc_write_command_init_insert (mongoc_write_command_t        *command,
                                        const bson_t * const          *documents,
                                        uint32_t                       n_documents,
                                        bool                           ordered,
                                        bool                           allow_bulk_op_insert);
void _mongoc_write_command_init_delete (mongoc_write_command_t        *command,
                                        const bson_t                  *selector,
                                        bool                           multi,
                                        bool                           ordered);
void _mongoc_write_command_init_update (mongoc_write_command_t        *command,
                                        const bson_t                  *selector,
                                        const bson_t                  *update,
                                        bool                           upsert,
                                        bool                           multi,
                                        bool                           ordered);
void _mongoc_write_command_insert_append (mongoc_write_command_t      *command,
                                          const bson_t * const        *documents,
                                          uint32_t                     n_documents);
void _mongoc_write_command_execute     (mongoc_write_command_t        *command,
                                        mongoc_client_t               *client,
                                        uint32_t                       hint,
                                        const char                    *database,
                                        const char                    *collection,
                                        const mongoc_write_concern_t  *write_concern,
                                        mongoc_write_result_t         *result);
void _mongoc_write_result_init         (mongoc_write_result_t         *result);
void _mongoc_write_result_merge        (mongoc_write_result_t         *result,
                                        mongoc_write_command_t        *command,
                                        const bson_t                  *reply);
void _mongoc_write_result_merge_legacy (mongoc_write_result_t         *result,
                                        mongoc_write_command_t        *command,
                                        const bson_t                  *reply);
bool _mongoc_write_result_complete     (mongoc_write_result_t         *result,
                                        bson_t                        *reply,
                                        bson_error_t                  *error);
void _mongoc_write_result_destroy      (mongoc_write_result_t         *result);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-database-private.h */

BSON_BEGIN_DECLS


struct _mongoc_database_t
{
   mongoc_client_t        *client;
   char                    name [128];
   mongoc_write_concern_t *write_concern;
   mongoc_read_prefs_t    *read_prefs;
};


mongoc_database_t *_mongoc_database_new (mongoc_client_t              *client,
                                         const char                   *name,
                                         const mongoc_read_prefs_t    *read_prefs,
                                         const mongoc_write_concern_t *write_concern);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-bulk-operation-private.h */

BSON_BEGIN_DECLS


struct _mongoc_bulk_operation_t
{
   char                   *database;
   char                   *collection;
   mongoc_client_t        *client;
   mongoc_write_concern_t *write_concern;
   bool                    ordered;
   uint32_t                hint;
   mongoc_array_t          commands;
   mongoc_write_result_t   result;
   bool                    executed;
};


mongoc_bulk_operation_t *_mongoc_bulk_operation_new (mongoc_client_t              *client,
                                                     const char                   *database,
                                                     const char                   *collection,
                                                     uint32_t                      hint,
                                                     bool                          ordered,
                                                     const mongoc_write_concern_t *write_concern);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-cluster-private.h */

BSON_BEGIN_DECLS


#define MONGOC_CLUSTER_MAX_NODES 12
#define MONGOC_CLUSTER_PING_NUM_SAMPLES 5


typedef enum
{
   MONGOC_CLUSTER_DIRECT,
   MONGOC_CLUSTER_REPLICA_SET,
   MONGOC_CLUSTER_SHARDED_CLUSTER,
} mongoc_cluster_mode_t;


typedef enum
{
   MONGOC_CLUSTER_STATE_BORN      = 0,
   MONGOC_CLUSTER_STATE_HEALTHY   = 1,
   MONGOC_CLUSTER_STATE_DEAD      = 2,
   MONGOC_CLUSTER_STATE_UNHEALTHY = (MONGOC_CLUSTER_STATE_DEAD |
                                     MONGOC_CLUSTER_STATE_HEALTHY),
} mongoc_cluster_state_t;


typedef struct
{
   uint32_t            index;
   mongoc_host_list_t  host;
   mongoc_stream_t    *stream;
   int32_t             ping_avg_msec;
   int32_t             pings[MONGOC_CLUSTER_PING_NUM_SAMPLES];
   int32_t             pings_pos;
   uint32_t            stamp;
   bson_t              tags;
   unsigned            primary    : 1;
   unsigned            needs_auth : 1;
   unsigned            isdbgrid   : 1;
   int32_t             min_wire_version;
   int32_t             max_wire_version;
   int32_t             max_write_batch_size;
   char               *replSet;
} mongoc_cluster_node_t;


typedef struct
{
   mongoc_cluster_mode_t   mode;
   mongoc_cluster_state_t  state;

   uint32_t                request_id;
   uint32_t                sockettimeoutms;

   int64_t                 last_reconnect;

   mongoc_uri_t           *uri;

   unsigned                requires_auth : 1;

   mongoc_cluster_node_t   nodes[MONGOC_CLUSTER_MAX_NODES];
   mongoc_client_t        *client;
   int32_t                 max_bson_size;
   int32_t                 max_msg_size;
   uint32_t                sec_latency_ms;
   mongoc_array_t          iov;

   mongoc_list_t          *peers;

   char                   *replSet;
} mongoc_cluster_t;


void                   _mongoc_cluster_destroy         (mongoc_cluster_t             *cluster);
void                   _mongoc_cluster_init            (mongoc_cluster_t             *cluster,
                                                        const mongoc_uri_t           *uri,
                                                        void                         *client);
uint32_t               _mongoc_cluster_sendv           (mongoc_cluster_t             *cluster,
                                                        mongoc_rpc_t                 *rpcs,
                                                        size_t                        rpcs_len,
                                                        uint32_t                      hint,
                                                        const mongoc_write_concern_t *write_concern,
                                                        const mongoc_read_prefs_t    *read_prefs,
                                                        bson_error_t                 *error);
uint32_t               _mongoc_cluster_try_sendv       (mongoc_cluster_t             *cluster,
                                                        mongoc_rpc_t                 *rpcs,
                                                        size_t                        rpcs_len,
                                                        uint32_t                      hint,
                                                        const mongoc_write_concern_t *write_concern,
                                                        const mongoc_read_prefs_t    *read_prefs,
                                                        bson_error_t                 *error);
bool                   _mongoc_cluster_try_recv        (mongoc_cluster_t             *cluster,
                                                        mongoc_rpc_t                 *rpc,
                                                        mongoc_buffer_t              *buffer,
                                                        uint32_t                      hint,
                                                        bson_error_t                 *error);
uint32_t               _mongoc_cluster_stamp           (const mongoc_cluster_t       *cluster,
                                                        uint32_t                      node);
mongoc_cluster_node_t *_mongoc_cluster_get_primary     (mongoc_cluster_t             *cluster);
bool                   _mongoc_cluster_command_early   (mongoc_cluster_t             *cluster,
                                                        const char                   *dbname,
                                                        const bson_t                 *command,
                                                        bson_t                       *reply,
                                                        bson_error_t                 *error);
void                   _mongoc_cluster_disconnect_node (mongoc_cluster_t             *cluster,
                                                        mongoc_cluster_node_t        *node);
bool                   _mongoc_cluster_reconnect       (mongoc_cluster_t             *cluster,
                                                        bson_error_t                 *error);
uint32_t               _mongoc_cluster_preselect       (mongoc_cluster_t             *cluster,
                                                        mongoc_opcode_t               opcode,
                                                        const mongoc_write_concern_t *write_concern,
                                                        const mongoc_read_prefs_t    *read_prefs,
                                                        bson_error_t                 *error);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-read-prefs-private.h */

BSON_BEGIN_DECLS


struct _mongoc_read_prefs_t
{
   mongoc_read_mode_t mode;
   bson_t             tags;
};


int _mongoc_read_prefs_score (const mongoc_read_prefs_t   *read_prefs,
                              const mongoc_cluster_node_t *node);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-client-private.h */

BSON_BEGIN_DECLS


struct _mongoc_client_t
{
   uint32_t                   request_id;
   mongoc_list_t             *conns;
   mongoc_uri_t              *uri;
   mongoc_cluster_t           cluster;
   bool                       in_exhaust;

   mongoc_stream_initiator_t  initiator;
   void                      *initiator_data;

#ifdef MONGOC_ENABLE_SSL
   mongoc_ssl_opt_t           ssl_opts;
   char                      *pem_subject;
#endif

   mongoc_read_prefs_t       *read_prefs;
   mongoc_write_concern_t    *write_concern;
};


mongoc_stream_t *_mongoc_client_create_stream (mongoc_client_t              *client,
                                               const mongoc_host_list_t     *host,
                                               bson_error_t                 *error);
uint32_t         _mongoc_client_sendv         (mongoc_client_t              *client,
                                               mongoc_rpc_t                 *rpcs,
                                               size_t                        rpcs_len,
                                               uint32_t                      hint,
                                               const mongoc_write_concern_t *write_concern,
                                               const mongoc_read_prefs_t    *read_prefs,
                                               bson_error_t                 *error);
bool             _mongoc_client_recv          (mongoc_client_t              *client,
                                               mongoc_rpc_t                 *rpc,
                                               mongoc_buffer_t              *buffer,
                                               uint32_t                      hint,
                                               bson_error_t                 *error);
bool             _mongoc_client_recv_gle      (mongoc_client_t              *client,
                                               uint32_t                      hint,
                                               bson_t                      **gle_doc,
                                               bson_error_t                 *error);
uint32_t         _mongoc_client_stamp         (mongoc_client_t              *client,
                                               uint32_t                      node);
bool             _mongoc_client_warm_up       (mongoc_client_t              *client,
                                               bson_error_t                 *error);
uint32_t         _mongoc_client_preselect     (mongoc_client_t              *client,
                                               mongoc_opcode_t               opcode,
                                               const mongoc_write_concern_t *write_concern,
                                               const mongoc_read_prefs_t    *read_prefs,
                                               bson_error_t                 *error);


BSON_END_DECLS

#endif	/* H_MONGO */


#ifndef        H_MONGOC
#define        H_MONGOC

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

#ifdef HAVE_LIBSASL2
#include <sasl/sasl.h>
#include <sasl/saslutil.h>
#endif

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

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
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

BSON_STATIC_ASSERT(sizeof(mongoc_iovec_t) == sizeof(WSABUF));
BSON_STATIC_ASSERT(offsetof(mongoc_iovec_t, iov_base) == offsetof(WSABUF, buf));
BSON_STATIC_ASSERT(offsetof(mongoc_iovec_t, iov_len) == offsetof(WSABUF, len));

#else
typedef struct iovec mongoc_iovec_t;
#endif


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-socket.h */

BSON_BEGIN_DECLS


typedef struct _mongoc_socket_t mongoc_socket_t;

typedef struct
{
   mongoc_socket_t *socket;
   int              events;
   int              revents;
} mongoc_socket_poll_t;

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
int              mongoc_socket_errno      (mongoc_socket_t       *sock)
	BSON_GNUC_PURE;
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
bool             mongoc_socket_check_closed (mongoc_socket_t       *sock);
void             mongoc_socket_inet_ntop  (struct addrinfo         *rp,
                                           char                    *buf,
                                           size_t                   buflen);
ssize_t          mongoc_socket_poll       (mongoc_socket_poll_t  *sds,
                                           size_t                 nsds,
                                           int32_t                timeout);


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
#define MONGOC_WRITE_CONCERN_W_ERRORS_IGNORED -1  /* deprecated */
#define MONGOC_WRITE_CONCERN_W_DEFAULT        -2
#define MONGOC_WRITE_CONCERN_W_MAJORITY       -3
#define MONGOC_WRITE_CONCERN_W_TAG            -4


typedef struct _mongoc_write_concern_t mongoc_write_concern_t;


mongoc_write_concern_t *mongoc_write_concern_new             (void);
mongoc_write_concern_t *mongoc_write_concern_copy            (const mongoc_write_concern_t *write_concern);
void                    mongoc_write_concern_destroy         (mongoc_write_concern_t       *write_concern);
bool                    mongoc_write_concern_get_fsync       (const mongoc_write_concern_t *write_concern)
   BSON_GNUC_DEPRECATED;
void                    mongoc_write_concern_set_fsync       (mongoc_write_concern_t       *write_concern,
                                                              bool                          fsync_)
   BSON_GNUC_DEPRECATED;
bool                    mongoc_write_concern_get_journal     (const mongoc_write_concern_t *write_concern);
bool                    mongoc_write_concern_journal_is_set  (const mongoc_write_concern_t *write_concern);
void                    mongoc_write_concern_set_journal     (mongoc_write_concern_t       *write_concern,
                                                              bool                          journal);
int32_t                 mongoc_write_concern_get_w           (const mongoc_write_concern_t *write_concern);
void                    mongoc_write_concern_set_w           (mongoc_write_concern_t       *write_concern,
                                                              int32_t                       w);
const char             *mongoc_write_concern_get_wtag        (const mongoc_write_concern_t *write_concern);
void                    mongoc_write_concern_set_wtag        (mongoc_write_concern_t       *write_concern,
                                                              const char                   *tag);
int32_t                 mongoc_write_concern_get_wtimeout    (const mongoc_write_concern_t *write_concern);
void                    mongoc_write_concern_set_wtimeout    (mongoc_write_concern_t       *write_concern,
                                                              int32_t                       wtimeout_msec);
bool                    mongoc_write_concern_get_wmajority   (const mongoc_write_concern_t *write_concern);
void                    mongoc_write_concern_set_wmajority   (mongoc_write_concern_t       *write_concern,
                                                              int32_t                       wtimeout_msec);
bool                    mongoc_write_concern_is_acknowledged (const mongoc_write_concern_t *write_concern);
bool                    mongoc_write_concern_is_valid        (const mongoc_write_concern_t *write_concern);


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
mongoc_read_mode_t   mongoc_read_prefs_get_mode (const mongoc_read_prefs_t *read_prefs)
	BSON_GNUC_PURE;
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
/* --- mongoc-read-concern.h */

BSON_BEGIN_DECLS


#define MONGOC_READ_CONCERN_LEVEL_LOCAL    "local"
#define MONGOC_READ_CONCERN_LEVEL_MAJORITY "majority"

typedef struct _mongoc_read_concern_t mongoc_read_concern_t;


mongoc_read_concern_t  *mongoc_read_concern_new           (void);
mongoc_read_concern_t  *mongoc_read_concern_copy          (const mongoc_read_concern_t *read_concern);
void                    mongoc_read_concern_destroy       (mongoc_read_concern_t       *read_concern);
const char             *mongoc_read_concern_get_level     (const mongoc_read_concern_t *read_concern);
bool                    mongoc_read_concern_set_level     (mongoc_read_concern_t       *read_concern,
                                                           const char                  *level);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-uri.h */

#ifndef MONGOC_DEFAULT_PORT
# define MONGOC_DEFAULT_PORT 27017
#endif


BSON_BEGIN_DECLS


typedef struct _mongoc_uri_t mongoc_uri_t;


mongoc_uri_t                 *mongoc_uri_copy                     (const mongoc_uri_t           *uri);
void                          mongoc_uri_destroy                  (mongoc_uri_t                 *uri);
mongoc_uri_t                 *mongoc_uri_new                      (const char                   *uri_string)
   BSON_GNUC_WARN_UNUSED_RESULT;
mongoc_uri_t                 *mongoc_uri_new_for_host_port        (const char                   *hostname,
                                                                   uint16_t                      port)
   BSON_GNUC_WARN_UNUSED_RESULT;
const mongoc_host_list_t     *mongoc_uri_get_hosts                (const mongoc_uri_t           *uri);
const char                   *mongoc_uri_get_database             (const mongoc_uri_t           *uri);
bool                          mongoc_uri_set_database             (mongoc_uri_t                 *uri,
                                                                   const char                   *database);
const bson_t                 *mongoc_uri_get_options              (const mongoc_uri_t           *uri);
const char                   *mongoc_uri_get_password             (const mongoc_uri_t           *uri);
bool                          mongoc_uri_set_password             (mongoc_uri_t                 *uri,
                                                                   const char                   *password);
bool                          mongoc_uri_option_is_int32          (const char                   *key)
	BSON_GNUC_PURE;
bool                          mongoc_uri_option_is_bool           (const char                   *key)
	BSON_GNUC_PURE;
bool                          mongoc_uri_option_is_utf8           (const char                   *key)
	BSON_GNUC_PURE;
int32_t                       mongoc_uri_get_option_as_int32      (const mongoc_uri_t           *uri,
                                                                   const char                   *option,
                                                                   int32_t                       fallback);
bool                          mongoc_uri_get_option_as_bool       (const mongoc_uri_t           *uri,
                                                                   const char                   *option,
                                                                   bool                          fallback);
const char*                   mongoc_uri_get_option_as_utf8       (const mongoc_uri_t           *uri,
                                                                   const char                   *option,
                                                                   const char                   *fallback);
bool                          mongoc_uri_set_option_as_int32      (mongoc_uri_t                 *uri,
                                                                   const char                   *option,
                                                                   int32_t                       value);
bool                          mongoc_uri_set_option_as_bool       (mongoc_uri_t                 *uri,
                                                                   const char                   *option,
                                                                   bool                          value);
bool                          mongoc_uri_set_option_as_utf8       (mongoc_uri_t                 *uri,
                                                                   const char                   *option,
                                                                   const char                   *value);
const bson_t                 *mongoc_uri_get_read_prefs           (const mongoc_uri_t           *uri)
   BSON_GNUC_DEPRECATED_FOR (mongoc_uri_get_read_prefs_t);
const char                   *mongoc_uri_get_replica_set          (const mongoc_uri_t           *uri);
const char                   *mongoc_uri_get_string               (const mongoc_uri_t           *uri);
const char                   *mongoc_uri_get_username             (const mongoc_uri_t           *uri);
bool                          mongoc_uri_set_username             (mongoc_uri_t                 *uri,
                                                                   const char                   *username);
const bson_t                 *mongoc_uri_get_credentials          (const mongoc_uri_t           *uri);
const char                   *mongoc_uri_get_auth_source          (const mongoc_uri_t           *uri);
bool                          mongoc_uri_set_auth_source          (mongoc_uri_t                 *uri,
                                                                   const char                   *value);
const char                   *mongoc_uri_get_auth_mechanism       (const mongoc_uri_t           *uri);
bool                          mongoc_uri_get_mechanism_properties (const mongoc_uri_t           *uri,
                                                                   bson_t                       *properties);
bool                          mongoc_uri_get_ssl                  (const mongoc_uri_t           *uri);
char                         *mongoc_uri_unescape                 (const char                   *escaped_string);
const mongoc_read_prefs_t *   mongoc_uri_get_read_prefs_t         (const mongoc_uri_t           *uri);
void                          mongoc_uri_set_read_prefs_t         (mongoc_uri_t                 *uri,
                                                                   const mongoc_read_prefs_t    *prefs);
const mongoc_write_concern_t *mongoc_uri_get_write_concern        (const mongoc_uri_t           *uri);
void                          mongoc_uri_set_write_concern        (mongoc_uri_t                 *uri,
                                                                   const mongoc_write_concern_t *wc);
const mongoc_read_concern_t  *mongoc_uri_get_read_concern         (const mongoc_uri_t           *uri);
void                          mongoc_uri_set_read_concern         (mongoc_uri_t                 *uri,
                                                                   const mongoc_read_concern_t  *rc);

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-stream.h */

BSON_BEGIN_DECLS


typedef struct _mongoc_stream_t mongoc_stream_t;

typedef struct _mongoc_stream_poll_t {
   mongoc_stream_t *stream;
   int              events;
   int              revents;
} mongoc_stream_poll_t;

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
   bool             (*check_closed)    (mongoc_stream_t *stream);
   ssize_t          (*poll)            (mongoc_stream_poll_t *streams,
                                        size_t                nstreams,
                                        int32_t               timeout);
   void             (*failed)          (mongoc_stream_t *stream);
   void             *padding [5];
};


mongoc_stream_t *mongoc_stream_get_base_stream (mongoc_stream_t       *stream);
mongoc_stream_t *mongoc_stream_get_tls_stream  (mongoc_stream_t       *stream);
int              mongoc_stream_close           (mongoc_stream_t       *stream);
void             mongoc_stream_destroy         (mongoc_stream_t       *stream);
void             mongoc_stream_failed          (mongoc_stream_t       *stream);
int              mongoc_stream_flush           (mongoc_stream_t       *stream);
ssize_t          mongoc_stream_writev          (mongoc_stream_t       *stream,
                                                mongoc_iovec_t        *iov,
                                                size_t                 iovcnt,
                                                int32_t                timeout_msec);
ssize_t          mongoc_stream_write           (mongoc_stream_t       *stream,
                                                void                  *buf,
                                                size_t                 count,
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
bool             mongoc_stream_check_closed    (mongoc_stream_t       *stream);
ssize_t          mongoc_stream_poll            (mongoc_stream_poll_t  *streams,
                                                size_t                 nstreams,
                                                int32_t                timeout);


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
/* --- mongoc-config.h */

/*
 * MONGOC_ENABLE_SECURE_TRANSPORT is set from configure to determine if we are
 * compiled with Native SSL support on Darwin
 */
#define MONGOC_ENABLE_SECURE_TRANSPORT 0

#if MONGOC_ENABLE_SECURE_TRANSPORT != 1
#  undef MONGOC_ENABLE_SECURE_TRANSPORT
#endif


/*
 * MONGOC_ENABLE_COMMON_CRYPTO is set from configure to determine if we are
 * compiled with Native Crypto support on Darwin
 */
#define MONGOC_ENABLE_COMMON_CRYPTO 0

#if MONGOC_ENABLE_COMMON_CRYPTO != 1
#  undef MONGOC_ENABLE_COMMON_CRYPTO
#endif


/*
 * MONGOC_ENABLE_OPENSSL is set from configure to determine if we are
 * compiled with OpenSSL support.
 */
#define MONGOC_ENABLE_OPENSSL 1

#if MONGOC_ENABLE_OPENSSL != 1
#  undef MONGOC_ENABLE_OPENSSL
#endif


/*
 * MONGOC_ENABLE_LIBCRYPTO is set from configure to determine if we are
 * compiled with OpenSSL support.
 */
#define MONGOC_ENABLE_LIBCRYPTO 1

#if MONGOC_ENABLE_LIBCRYPTO != 1
#  undef MONGOC_ENABLE_LIBCRYPTO
#endif


/*
 * MONGOC_ENABLE_SSL is set from configure to determine if we are
 * compiled with any SSL support.
 */
#define MONGOC_ENABLE_SSL 1

#if MONGOC_ENABLE_SSL != 1
#  undef MONGOC_ENABLE_SSL
#endif


/*
 * MONGOC_ENABLE_CRYPTO is set from configure to determine if we are
 * compiled with any crypto support.
 */
#define MONGOC_ENABLE_CRYPTO 1

#if MONGOC_ENABLE_CRYPTO != 1
#  undef MONGOC_ENABLE_CRYPTO
#endif


/*
 * MONGOC_ENABLE_SASL is set from configure to determine if we are
 * compiled with SASL support.
 */
#define MONGOC_ENABLE_SASL 1

#if MONGOC_ENABLE_SASL != 1
#  undef MONGOC_ENABLE_SASL
#endif


/*
 * MONGOC_HAVE_SASL_CLIENT_DONE is set from configure to determine if we
 * have SASL and its version is new enough to use sasl_client_done (),
 * which supersedes sasl_done ().
 */
#define MONGOC_HAVE_SASL_CLIENT_DONE 1

#if MONGOC_HAVE_SASL_CLIENT_DONE != 1
#  undef MONGOC_HAVE_SASL_CLIENT_DONE
#endif


/*
 * MONGOC_HAVE_WEAK_SYMBOLS is set from configure to determine if the
 * compiler supports the (weak) annotation. We use it to prevent
 * Link-Time-Optimization (LTO) in our constant-time mongoc_memcmp()
 * This is known to work with GNU GCC and Solaris Studio
 */
#define MONGOC_HAVE_WEAK_SYMBOLS 1

#if MONGOC_HAVE_WEAK_SYMBOLS != 1
#  undef MONGOC_HAVE_WEAK_SYMBOLS
#endif

/*==============================================================*/
/* --- mongoc-ssl.h */

#ifdef MONGOC_ENABLE_SSL

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
char                   *mongoc_ssl_extract_subject (const char *filename);


BSON_END_DECLS

#endif	/* MONGOC_ENABLE_OPENSSL */

/*==============================================================*/
/* --- mongoc-stream-tls.h */

#ifdef MONGOC_ENABLE_OPENSSL

BSON_BEGIN_DECLS

typedef struct   _mongoc_stream_tls_t mongoc_stream_tls_t;

bool             mongoc_stream_tls_do_handshake  (mongoc_stream_t  *stream,
                                                  int32_t           timeout_msec);
bool             mongoc_stream_tls_should_retry  (mongoc_stream_t  *stream);
bool             mongoc_stream_tls_should_read   (mongoc_stream_t  *stream);
bool             mongoc_stream_tls_should_write  (mongoc_stream_t  *stream);
bool             mongoc_stream_tls_check_cert    (mongoc_stream_t  *stream,
                                                  const char       *host);
mongoc_stream_t *mongoc_stream_tls_new           (mongoc_stream_t  *base_stream,
                                                  mongoc_ssl_opt_t *opt,
                                                  int               client);


BSON_END_DECLS

#endif	/* MONGOC_ENABLE_OPENSSL */

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
#define MONGOC_MINOR_VERSION (3)


/**
 * MONGOC_MICRO_VERSION:
 *
 * MONGOC micro version component (e.g. 3 if %MONGOC_VERSION is 1.2.3)
 */
#define MONGOC_MICRO_VERSION (2)


/**
 * MONGOC_PRERELEASE_VERSION:
 *
 * MONGOC prerelease version component (e.g. rc0 if %MONGOC_VERSION is 1.2.3-rc0)
 */
#define MONGOC_PRERELEASE_VERSION ()


/**
 * MONGOC_VERSION:
 *
 * MONGOC version.
 */
#define MONGOC_VERSION (1.3.2)


/**
 * MONGOC_VERSION_S:
 *
 * MONGOC version, encoded as a string, useful for printing and
 * concatenation.
 */
#define MONGOC_VERSION_S "1.3.2"


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
mongoc_log_level_str (mongoc_log_level_t log_level)
	BSON_GNUC_CONST;


/**
 * mongoc_log_trace_enable:
 *
 * Enables tracing at runtime (if it has been enabled at compile time).
 */
void
mongoc_log_trace_enable (void)
	BSON_GNUC_CONST;


/**
 * mongoc_log_trace_disable:
 *
 * Disables tracing at runtime (if it has been enabled at compile time).
 */
void
mongoc_log_trace_disable (void)
	BSON_GNUC_CONST;


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-trace.h */

BSON_BEGIN_DECLS


#ifdef MONGOC_TRACE
#define TRACE(msg, ...) \
                    do { mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, "TRACE: %s():%d " msg, BSON_FUNC, __LINE__, __VA_ARGS__); } while (0)
#define ENTRY       do { mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, "ENTRY: %s():%d", BSON_FUNC, __LINE__); } while (0)
#define EXIT        do { mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, " EXIT: %s():%d", BSON_FUNC, __LINE__); return; } while (0)
#define RETURN(ret) do { mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, " EXIT: %s():%d", BSON_FUNC, __LINE__); return ret; } while (0)
#define GOTO(label) do { mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, " GOTO: %s():%d %s", BSON_FUNC, __LINE__, #label); goto label; } while (0)
#define DUMP_BYTES(_n, _b, _l) do { \
   mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, "TRACE: %s():%d %s = %p [%d]", BSON_FUNC, __LINE__, #_n, _b, (int)_l); \
   mongoc_log_trace_bytes(MONGOC_LOG_DOMAIN, _b, _l); \
} while (0)
#define DUMP_IOVEC(_n, _iov, _iovcnt) do { \
   mongoc_log(MONGOC_LOG_LEVEL_TRACE, MONGOC_LOG_DOMAIN, "TRACE: %s():%d %s = %p [%d]", BSON_FUNC, __LINE__, #_n, _iov, (int)_iovcnt); \
   mongoc_log_trace_iovec(MONGOC_LOG_DOMAIN, _iov, _iovcnt); \
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
/* --- mongoc-matcher.h */

BSON_BEGIN_DECLS


typedef struct _mongoc_matcher_t mongoc_matcher_t;


mongoc_matcher_t *mongoc_matcher_new     (const bson_t           *query,
                                          bson_error_t           *error)      BSON_GNUC_DEPRECATED;
bool              mongoc_matcher_match   (const mongoc_matcher_t *matcher,
                                          const bson_t           *document)   BSON_GNUC_DEPRECATED;
void              mongoc_matcher_destroy (mongoc_matcher_t       *matcher)    BSON_GNUC_DEPRECATED;


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-cursor.h */

BSON_BEGIN_DECLS

typedef struct _mongoc_cursor_t mongoc_cursor_t;


/* forward decl */
struct _mongoc_client_t;

mongoc_cursor_t *mongoc_cursor_clone                  (const mongoc_cursor_t   *cursor)
   BSON_GNUC_WARN_UNUSED_RESULT;
void             mongoc_cursor_destroy                (mongoc_cursor_t         *cursor);
bool             mongoc_cursor_more                   (mongoc_cursor_t         *cursor);
bool             mongoc_cursor_next                   (mongoc_cursor_t         *cursor,
                                                       const bson_t           **bson);
bool             mongoc_cursor_error                  (mongoc_cursor_t         *cursor,
                                                       bson_error_t            *error);
void             mongoc_cursor_get_host               (mongoc_cursor_t         *cursor,
                                                       mongoc_host_list_t      *host);
bool             mongoc_cursor_is_alive               (const mongoc_cursor_t   *cursor);
const bson_t    *mongoc_cursor_current                (const mongoc_cursor_t   *cursor);
void             mongoc_cursor_set_batch_size         (mongoc_cursor_t         *cursor,
                                                       uint32_t                 batch_size);
uint32_t         mongoc_cursor_get_batch_size         (const mongoc_cursor_t   *cursor);
bool             mongoc_cursor_set_limit              (mongoc_cursor_t         *cursor,
                                                       int64_t                  limit);
int64_t          mongoc_cursor_get_limit              (const mongoc_cursor_t   *cursor);
uint32_t         mongoc_cursor_get_hint               (const mongoc_cursor_t   *cursor);
int64_t          mongoc_cursor_get_id                 (const mongoc_cursor_t   *cursor);
void             mongoc_cursor_set_max_await_time_ms  (mongoc_cursor_t         *cursor,
                                                       uint32_t                 max_await_time_ms);
uint32_t         mongoc_cursor_get_max_await_time_ms  (const mongoc_cursor_t   *cursor);
mongoc_cursor_t *mongoc_cursor_new_from_command_reply (struct _mongoc_client_t *client,
                                                       bson_t                  *reply,
                                                       uint32_t                 server_id)
   BSON_GNUC_WARN_UNUSED_RESULT;

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
   uint8_t twod_sphere_version;
   uint8_t twod_bits_precision;
   double  twod_location_min;
   double  twod_location_max;
   double  haystack_bucket_size;
   uint8_t *padding[32];
} mongoc_index_opt_geo_t;

typedef struct
{
   int type;
} mongoc_index_opt_storage_t;

typedef enum
{
   MONGOC_INDEX_STORAGE_OPT_MMAPV1,
   MONGOC_INDEX_STORAGE_OPT_WIREDTIGER,
} mongoc_index_storage_opt_type_t;

typedef struct
{
   mongoc_index_opt_storage_t  base;
   const char                 *config_str;
   void                       *padding[8];
} mongoc_index_opt_wt_t;

typedef struct
{
   bool                        is_initialized;
   bool                        background;
   bool                        unique;
   const char                 *name;
   bool                        drop_dups;
   bool                        sparse;
   int32_t                     expire_after_seconds;
   int32_t                     v;
   const bson_t               *weights;
   const char                 *default_language;
   const char                 *language_override;
   mongoc_index_opt_geo_t     *geo_options;
   mongoc_index_opt_storage_t *storage_options;
   const bson_t               *partial_filter_expression;
   void                       *padding[5];
} mongoc_index_opt_t;


const mongoc_index_opt_t     *mongoc_index_opt_get_default     (void) BSON_GNUC_CONST;
const mongoc_index_opt_geo_t *mongoc_index_opt_geo_get_default (void) BSON_GNUC_CONST;
const mongoc_index_opt_wt_t  *mongoc_index_opt_wt_get_default  (void) BSON_GNUC_CONST;
void                          mongoc_index_opt_init            (mongoc_index_opt_t *opt);
void                          mongoc_index_opt_geo_init        (mongoc_index_opt_geo_t *opt);
void                          mongoc_index_opt_wt_init         (mongoc_index_opt_wt_t *opt);

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
   MONGOC_ERROR_SCRAM,
   MONGOC_ERROR_SERVER_SELECTION,
   MONGOC_ERROR_WRITE_CONCERN,
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
   MONGOC_ERROR_NAMESPACE_INVALID_FILTER_TYPE,

   MONGOC_ERROR_COMMAND_INVALID_ARG,

   MONGOC_ERROR_COLLECTION_INSERT_FAILED,
   MONGOC_ERROR_COLLECTION_UPDATE_FAILED,
   MONGOC_ERROR_COLLECTION_DELETE_FAILED,
   MONGOC_ERROR_COLLECTION_DOES_NOT_EXIST = 26,

   MONGOC_ERROR_GRIDFS_INVALID_FILENAME,

   MONGOC_ERROR_SCRAM_NOT_DONE,
   MONGOC_ERROR_SCRAM_PROTOCOL_ERROR,

   MONGOC_ERROR_QUERY_COMMAND_NOT_FOUND = 59,
   MONGOC_ERROR_QUERY_NOT_TAILABLE = 13051,

   MONGOC_ERROR_SERVER_SELECTION_BAD_WIRE_VERSION,
   MONGOC_ERROR_SERVER_SELECTION_FAILURE,
   MONGOC_ERROR_SERVER_SELECTION_INVALID_ID,

   MONGOC_ERROR_GRIDFS_CHUNK_MISSING,

   /* Dup with query failure. */
   MONGOC_ERROR_PROTOCOL_ERROR = 17,

   MONGOC_ERROR_WRITE_CONCERN_ERROR = 64,
} mongoc_error_code_t;


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-bulk-operation.h */

#define MONGOC_BULK_WRITE_FLAGS_INIT { true, MONGOC_BYPASS_DOCUMENT_VALIDATION_DEFAULT }

BSON_BEGIN_DECLS


typedef struct _mongoc_bulk_operation_t mongoc_bulk_operation_t;
typedef struct _mongoc_bulk_write_flags_t mongoc_bulk_write_flags_t;


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
void mongoc_bulk_operation_set_bypass_document_validation (mongoc_bulk_operation_t   *bulk,
                                                           bool                       bypass);


/*
 * The following functions are really only useful by language bindings and
 * those wanting to replay a bulk operation to a number of clients or
 * collections.
 */
mongoc_bulk_operation_t      *mongoc_bulk_operation_new               (bool                           ordered);
void                          mongoc_bulk_operation_set_write_concern (mongoc_bulk_operation_t       *bulk,
                                                                       const mongoc_write_concern_t  *write_concern);
void                          mongoc_bulk_operation_set_database      (mongoc_bulk_operation_t       *bulk,
                                                                       const char                    *database);
void                          mongoc_bulk_operation_set_collection    (mongoc_bulk_operation_t       *bulk,
                                                                       const char                    *collection);
void                          mongoc_bulk_operation_set_client        (mongoc_bulk_operation_t       *bulk,
                                                                       void                          *client);
/* These names include the term "hint" for backward compatibility, should be
 * mongoc_bulk_operation_get_server_id, mongoc_bulk_operation_set_server_id. */
void                          mongoc_bulk_operation_set_hint          (mongoc_bulk_operation_t       *bulk,
                                                                       uint32_t                       server_id);
uint32_t                      mongoc_bulk_operation_get_hint          (const mongoc_bulk_operation_t *bulk);
const mongoc_write_concern_t *mongoc_bulk_operation_get_write_concern (const mongoc_bulk_operation_t *bulk);

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-find-and-modify.h */

BSON_BEGIN_DECLS

typedef enum
{
   MONGOC_FIND_AND_MODIFY_NONE   = 0,
   MONGOC_FIND_AND_MODIFY_REMOVE = 1 << 0,
   MONGOC_FIND_AND_MODIFY_UPSERT = 1 << 1,
   MONGOC_FIND_AND_MODIFY_RETURN_NEW = 1 << 2,
} mongoc_find_and_modify_flags_t;

typedef struct _mongoc_find_and_modify_opts_t mongoc_find_and_modify_opts_t ;

mongoc_find_and_modify_opts_t*
mongoc_find_and_modify_opts_new               (void);

bool
mongoc_find_and_modify_opts_set_sort          (mongoc_find_and_modify_opts_t        *opts,
                                               const bson_t                         *sort);
bool
mongoc_find_and_modify_opts_set_update        (mongoc_find_and_modify_opts_t        *opts,
                                               const bson_t                         *update);
bool
mongoc_find_and_modify_opts_set_fields        (mongoc_find_and_modify_opts_t        *opts,
                                               const bson_t                         *fields);
bool
mongoc_find_and_modify_opts_set_flags         (mongoc_find_and_modify_opts_t        *opts,
                                               const mongoc_find_and_modify_flags_t  flags);
bool
mongoc_find_and_modify_opts_set_bypass_document_validation (mongoc_find_and_modify_opts_t *opts,
                                                            bool                           bypass);
void
mongoc_find_and_modify_opts_destroy           (mongoc_find_and_modify_opts_t *opts);

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
mongoc_collection_t          *mongoc_collection_copy                 (mongoc_collection_t           *collection);
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
int64_t                       mongoc_collection_count_with_opts      (mongoc_collection_t           *collection,
                                                                      mongoc_query_flags_t           flags,
                                                                      const bson_t                  *query,
                                                                      int64_t                        skip,
                                                                      int64_t                        limit,
                                                                      const bson_t                  *opts,
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
mongoc_cursor_t              *mongoc_collection_find_indexes         (mongoc_collection_t           *collection,
                                                                      bson_error_t                  *error);
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
bool                          mongoc_collection_find_and_modify_with_opts (mongoc_collection_t                 *collection,
                                                                           const bson_t                        *query,
                                                                           const mongoc_find_and_modify_opts_t *opts,
                                                                           bson_t                              *reply,
                                                                           bson_error_t                        *error);
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
const mongoc_read_concern_t  *mongoc_collection_get_read_concern     (const mongoc_collection_t     *collection);
void                          mongoc_collection_set_read_concern     (mongoc_collection_t           *collection,
                                                                      const mongoc_read_concern_t   *read_concern);
const mongoc_write_concern_t *mongoc_collection_get_write_concern    (const mongoc_collection_t     *collection);
void                          mongoc_collection_set_write_concern    (mongoc_collection_t           *collection,
                                                                      const mongoc_write_concern_t  *write_concern);
const char                   *mongoc_collection_get_name             (mongoc_collection_t           *collection)
	BSON_GNUC_PURE;
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
mongoc_database_t            *mongoc_database_copy                 (mongoc_database_t            *database);
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
const mongoc_read_concern_t  *mongoc_database_get_read_concern     (const mongoc_database_t      *database);
void                          mongoc_database_set_read_concern     (mongoc_database_t            *database,
                                                                    const mongoc_read_concern_t *read_concern);
mongoc_cursor_t              *mongoc_database_find_collections     (mongoc_database_t            *database,
                                                                    const bson_t                 *filter,
                                                                    bson_error_t                 *error);
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
   mongoc_gridfs_file_get_##name (mongoc_gridfs_file_t * file) \
	BSON_GNUC_PURE; \
   void \
      mongoc_gridfs_file_set_##name (mongoc_gridfs_file_t * file, \
                                     const char           *str);


#define MONGOC_GRIDFS_FILE_BSON_HEADER(name) \
   const bson_t * \
   mongoc_gridfs_file_get_##name (mongoc_gridfs_file_t * file) \
	BSON_GNUC_PURE; \
   void \
      mongoc_gridfs_file_set_##name (mongoc_gridfs_file_t * file, \
                                     const bson_t * bson);


typedef struct _mongoc_gridfs_file_t mongoc_gridfs_file_t;
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


const bson_value_t *
mongoc_gridfs_file_get_id (mongoc_gridfs_file_t * file);

int64_t
mongoc_gridfs_file_get_length (mongoc_gridfs_file_t *file);

int32_t
mongoc_gridfs_file_get_chunk_size (mongoc_gridfs_file_t *file);

int64_t
mongoc_gridfs_file_get_upload_date (mongoc_gridfs_file_t *file);

ssize_t
mongoc_gridfs_file_writev (mongoc_gridfs_file_t *file,
                           mongoc_iovec_t       *iov,
                           size_t                iovcnt,
                           uint32_t              timeout_msec);
ssize_t
mongoc_gridfs_file_readv (mongoc_gridfs_file_t *file,
                          mongoc_iovec_t       *iov,
                          size_t                iovcnt,
                          size_t                min_bytes,
                          uint32_t              timeout_msec);
int
mongoc_gridfs_file_seek (mongoc_gridfs_file_t *file,
                         int64_t               delta,
                         int                   whence);

uint64_t
mongoc_gridfs_file_tell (mongoc_gridfs_file_t *file)
	BSON_GNUC_PURE;

bool
mongoc_gridfs_file_save (mongoc_gridfs_file_t *file);

void
mongoc_gridfs_file_destroy (mongoc_gridfs_file_t *file);

bool
mongoc_gridfs_file_error (mongoc_gridfs_file_t *file,
                          bson_error_t         *error);

bool
mongoc_gridfs_file_remove (mongoc_gridfs_file_t *file,
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
/* --- mongoc-apm.h */

BSON_BEGIN_DECLS

/*
 * An Application Performance Management (APM) interface, complying with
 * MongoDB's Command Monitoring Spec:
 *
 * https://github.com/mongodb/specifications/tree/master/source/command-monitoring
 */

/*
 * callbacks to receive APM events
 */

typedef struct _mongoc_apm_callbacks_t mongoc_apm_callbacks_t;


/*
 * events: command started, succeeded, or failed
 */

typedef struct _mongoc_apm_command_started_t   mongoc_apm_command_started_t;
typedef struct _mongoc_apm_command_succeeded_t mongoc_apm_command_succeeded_t;
typedef struct _mongoc_apm_command_failed_t    mongoc_apm_command_failed_t;


/*
 * event field accessors
 */

/* command-started event fields */

const bson_t *
mongoc_apm_command_started_get_command        (const mongoc_apm_command_started_t *event)
	BSON_GNUC_PURE;
const char   *
mongoc_apm_command_started_get_database_name  (const mongoc_apm_command_started_t *event)
	BSON_GNUC_PURE;
const char   *
mongoc_apm_command_started_get_command_name   (const mongoc_apm_command_started_t *event)
	BSON_GNUC_PURE;
int64_t
mongoc_apm_command_started_get_request_id     (const mongoc_apm_command_started_t *event)
	BSON_GNUC_PURE;
int64_t
mongoc_apm_command_started_get_operation_id   (const mongoc_apm_command_started_t *event)
	BSON_GNUC_PURE;
const mongoc_host_list_t *
mongoc_apm_command_started_get_host           (const mongoc_apm_command_started_t *event)
	BSON_GNUC_PURE;
uint32_t
mongoc_apm_command_started_get_server_id      (const mongoc_apm_command_started_t *event)
	BSON_GNUC_PURE;
void *
mongoc_apm_command_started_get_context        (const mongoc_apm_command_started_t *event)
	BSON_GNUC_PURE;

/* command-succeeded event fields */

int64_t
mongoc_apm_command_succeeded_get_duration     (const mongoc_apm_command_succeeded_t *event)
	BSON_GNUC_PURE;
const bson_t *
mongoc_apm_command_succeeded_get_reply        (const mongoc_apm_command_succeeded_t *event)
	BSON_GNUC_PURE;
const char *
mongoc_apm_command_succeeded_get_command_name (const mongoc_apm_command_succeeded_t *event)
	BSON_GNUC_PURE;
int64_t
mongoc_apm_command_succeeded_get_request_id   (const mongoc_apm_command_succeeded_t *event)
	BSON_GNUC_PURE;
int64_t
mongoc_apm_command_succeeded_get_operation_id (const mongoc_apm_command_succeeded_t *event)
	BSON_GNUC_PURE;
const mongoc_host_list_t *
mongoc_apm_command_succeeded_get_host         (const mongoc_apm_command_succeeded_t *event)
	BSON_GNUC_PURE;
uint32_t
mongoc_apm_command_succeeded_get_server_id    (const mongoc_apm_command_succeeded_t *event)
	BSON_GNUC_PURE;
void *
mongoc_apm_command_succeeded_get_context      (const mongoc_apm_command_succeeded_t *event)
	BSON_GNUC_PURE;

/* command-failed event fields */

int64_t
mongoc_apm_command_failed_get_duration        (const mongoc_apm_command_failed_t *event)
	BSON_GNUC_PURE;
const char *
mongoc_apm_command_failed_get_command_name    (const mongoc_apm_command_failed_t *event)
	BSON_GNUC_PURE;
/* retrieve the error by filling out the passed-in "error" struct */
void
mongoc_apm_command_failed_get_error           (const mongoc_apm_command_failed_t *event,
                                               bson_error_t *error);
int64_t
mongoc_apm_command_failed_get_request_id      (const mongoc_apm_command_failed_t *event)
	BSON_GNUC_PURE;
int64_t
mongoc_apm_command_failed_get_operation_id    (const mongoc_apm_command_failed_t *event)
	BSON_GNUC_PURE;
const mongoc_host_list_t *
mongoc_apm_command_failed_get_host            (const mongoc_apm_command_failed_t *event)
	BSON_GNUC_PURE;
uint32_t
mongoc_apm_command_failed_get_server_id       (const mongoc_apm_command_failed_t *event)
	BSON_GNUC_PURE;
void *
mongoc_apm_command_failed_get_context         (const mongoc_apm_command_failed_t *event)
	BSON_GNUC_PURE;

/*
 * callbacks
 */

typedef void
(*mongoc_apm_command_started_cb_t)   (const mongoc_apm_command_started_t   *event);
typedef void
(*mongoc_apm_command_succeeded_cb_t) (const mongoc_apm_command_succeeded_t *event);
typedef void
(*mongoc_apm_command_failed_cb_t)    (const mongoc_apm_command_failed_t    *event);

/*
 * registering callbacks
 */

mongoc_apm_callbacks_t *
mongoc_apm_callbacks_new             (void);
void
mongoc_apm_callbacks_destroy         (mongoc_apm_callbacks_t            *callbacks);
void
mongoc_apm_set_command_started_cb    (mongoc_apm_callbacks_t            *callbacks,
                                      mongoc_apm_command_started_cb_t    cb);
void
mongoc_apm_set_command_succeeded_cb  (mongoc_apm_callbacks_t            *callbacks,
                                      mongoc_apm_command_succeeded_cb_t  cb);
void
mongoc_apm_set_command_failed_cb     (mongoc_apm_callbacks_t            *callbacks,
                                      mongoc_apm_command_failed_cb_t     cb);

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-server-description.h */

BSON_BEGIN_DECLS

typedef struct _mongoc_server_description_t mongoc_server_description_t;

void
mongoc_server_description_destroy (mongoc_server_description_t *description);

mongoc_server_description_t *
mongoc_server_description_new_copy (const mongoc_server_description_t *description);

uint32_t
mongoc_server_description_id (mongoc_server_description_t *description)
	BSON_GNUC_PURE;

mongoc_host_list_t *
mongoc_server_description_host (mongoc_server_description_t *description)
	BSON_GNUC_CONST;

const char *
mongoc_server_description_type (mongoc_server_description_t *description);

const bson_t *
mongoc_server_description_ismaster (mongoc_server_description_t *description)
	BSON_GNUC_CONST;

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-client.h */

BSON_BEGIN_DECLS


#define MONGOC_NAMESPACE_MAX 128


#ifndef MONGOC_DEFAULT_CONNECTTIMEOUTMS
#define MONGOC_DEFAULT_CONNECTTIMEOUTMS (10 * 1000L)
#endif


#ifndef MONGOC_DEFAULT_SOCKETTIMEOUTMS
/*
 * NOTE: The default socket timeout for connections is 5 minutes. This
 *       means that if your MongoDB server dies or becomes unavailable
 *       it will take 5 minutes to detect this.
 *
 *       You can change this by providing sockettimeoutms= in your
 *       connection URI.
 */
#define MONGOC_DEFAULT_SOCKETTIMEOUTMS (1000L * 60L * 5L)
#endif


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
 * @user_data: The pointer passed to mongoc_client_set_stream_initiator.
 * @error: A location for an error.
 *
 * Creates a new mongoc_stream_t for the host and port. Begin a
 * non-blocking connect and return immediately.
 *
 * This can be used by language bindings to create network transports other
 * than those built into libmongoc. An example of such would be the streams
 * API provided by PHP.
 *
 * Returns: A newly allocated mongoc_stream_t or NULL on failure.
 */
typedef mongoc_stream_t *(*mongoc_stream_initiator_t) (const mongoc_uri_t       *uri,
                                                       const mongoc_host_list_t *host,
                                                       void                     *user_data,
                                                       bson_error_t             *error);


mongoc_client_t               *mongoc_client_new                           (const char                   *uri_string);
mongoc_client_t               *mongoc_client_new_from_uri                  (const mongoc_uri_t           *uri);
const mongoc_uri_t            *mongoc_client_get_uri                       (const mongoc_client_t        *client);
void                           mongoc_client_set_stream_initiator          (mongoc_client_t              *client,
                                                                            mongoc_stream_initiator_t     initiator,
                                                                            void                         *user_data);
mongoc_cursor_t               *mongoc_client_command                       (mongoc_client_t              *client,
                                                                            const char                   *db_name,
                                                                            mongoc_query_flags_t          flags,
                                                                            uint32_t                      skip,
                                                                            uint32_t                      limit,
                                                                            uint32_t                      batch_size,
                                                                            const bson_t                 *query,
                                                                            const bson_t                 *fields,
                                                                            const mongoc_read_prefs_t    *read_prefs);
void                           mongoc_client_kill_cursor                   (mongoc_client_t *client,
                                                                            int64_t          cursor_id) BSON_GNUC_DEPRECATED;
bool                           mongoc_client_command_simple                (mongoc_client_t              *client,
                                                                            const char                   *db_name,
                                                                            const bson_t                 *command,
                                                                            const mongoc_read_prefs_t    *read_prefs,
                                                                            bson_t                       *reply,
                                                                            bson_error_t                 *error);
bool                           mongoc_client_command_simple_with_server_id (mongoc_client_t              *client,
                                                                            const char                   *db_name,
                                                                            const bson_t                 *command,
                                                                            const mongoc_read_prefs_t    *read_prefs,
                                                                            uint32_t                      server_id,
                                                                            bson_t                       *reply,
                                                                            bson_error_t                 *error);
void                           mongoc_client_destroy                       (mongoc_client_t              *client);
mongoc_database_t             *mongoc_client_get_database                  (mongoc_client_t              *client,
                                                                            const char                   *name);
mongoc_database_t             *mongoc_client_get_default_database          (mongoc_client_t              *client);
mongoc_gridfs_t               *mongoc_client_get_gridfs                    (mongoc_client_t              *client,
                                                                            const char                   *db,
                                                                            const char                   *prefix,
                                                                            bson_error_t                 *error);
mongoc_collection_t           *mongoc_client_get_collection                (mongoc_client_t              *client,
                                                                            const char                   *db,
                                                                            const char                   *collection);
char                         **mongoc_client_get_database_names            (mongoc_client_t              *client,
                                                                            bson_error_t                 *error);
mongoc_cursor_t               *mongoc_client_find_databases                (mongoc_client_t              *client,
                                                                            bson_error_t                 *error);
bool                           mongoc_client_get_server_status             (mongoc_client_t              *client,
                                                                            mongoc_read_prefs_t          *read_prefs,
                                                                            bson_t                       *reply,
                                                                            bson_error_t                 *error);
int32_t                        mongoc_client_get_max_message_size          (mongoc_client_t              *client) BSON_GNUC_DEPRECATED;
int32_t                        mongoc_client_get_max_bson_size             (mongoc_client_t              *client) BSON_GNUC_DEPRECATED;
const mongoc_write_concern_t  *mongoc_client_get_write_concern             (const mongoc_client_t        *client);
void                           mongoc_client_set_write_concern             (mongoc_client_t              *client,
                                                                            const mongoc_write_concern_t *write_concern);
const mongoc_read_concern_t   *mongoc_client_get_read_concern              (const mongoc_client_t        *client);
void                           mongoc_client_set_read_concern              (mongoc_client_t              *client,
                                                                            const mongoc_read_concern_t  *read_concern);
const mongoc_read_prefs_t     *mongoc_client_get_read_prefs                (const mongoc_client_t        *client);
void                           mongoc_client_set_read_prefs                (mongoc_client_t              *client,
                                                                            const mongoc_read_prefs_t    *read_prefs);
#ifdef MONGOC_ENABLE_SSL
void                           mongoc_client_set_ssl_opts                  (mongoc_client_t              *client,
                                                                            const mongoc_ssl_opt_t       *opts);
#endif
void                           mongoc_client_set_apm_callbacks             (mongoc_client_t              *client,
                                                                            mongoc_apm_callbacks_t       *callbacks,
                                                                            void                         *context);
mongoc_server_description_t   *mongoc_client_get_server_description        (mongoc_client_t              *client,
                                                                            uint32_t                      server_id);
mongoc_server_description_t  **mongoc_client_get_server_descriptions       (const mongoc_client_t        *client,
                                                                            size_t                       *n);
void                           mongoc_server_descriptions_destroy_all      (mongoc_server_description_t **sds,
                                                                            size_t                        n);
mongoc_server_description_t   *mongoc_client_select_server                 (mongoc_client_t              *client,
                                                                            bool                          for_writes,
                                                                            mongoc_read_prefs_t          *prefs,
                                                                            bson_error_t                 *error);

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-client-pool.h */

BSON_BEGIN_DECLS


typedef struct _mongoc_client_pool_t mongoc_client_pool_t;


mongoc_client_pool_t *mongoc_client_pool_new               (const mongoc_uri_t     *uri);
void                  mongoc_client_pool_destroy           (mongoc_client_pool_t   *pool);
mongoc_client_t      *mongoc_client_pool_pop               (mongoc_client_pool_t   *pool);
void                  mongoc_client_pool_push              (mongoc_client_pool_t   *pool,
                                                            mongoc_client_t        *client);
mongoc_client_t      *mongoc_client_pool_try_pop           (mongoc_client_pool_t   *pool);
void                  mongoc_client_pool_max_size          (mongoc_client_pool_t   *pool,
                                                            uint32_t                max_pool_size);
void                  mongoc_client_pool_min_size          (mongoc_client_pool_t   *pool,
                                                            uint32_t                min_pool_size);
#ifdef MONGOC_ENABLE_SSL
void                  mongoc_client_pool_set_ssl_opts      (mongoc_client_pool_t   *pool,
                                                            const mongoc_ssl_opt_t *opts);
#endif
void                  mongoc_client_pool_set_apm_callbacks (mongoc_client_pool_t   *pool,
                                                            mongoc_apm_callbacks_t *callbacks,
                                                            void                   *context);


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
void _mongoc_array_copy        (mongoc_array_t *dst,
                                const mongoc_array_t *src);
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
_mongoc_buffer_try_append_from_stream (mongoc_buffer_t *buffer,
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

typedef union
{
   mongoc_rpc_delete_t       delete_;
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


void _mongoc_rpc_gather             (mongoc_rpc_t                 *rpc,
                                     mongoc_array_t               *array);
bool _mongoc_rpc_needs_gle          (mongoc_rpc_t                 *rpc,
                                     const mongoc_write_concern_t *write_concern)
	BSON_GNUC_PURE;
void _mongoc_rpc_swab_to_le         (mongoc_rpc_t                 *rpc)
	BSON_GNUC_CONST;
void _mongoc_rpc_swab_from_le       (mongoc_rpc_t                 *rpc)
	BSON_GNUC_CONST;
void _mongoc_rpc_printf             (mongoc_rpc_t                 *rpc);
bool _mongoc_rpc_scatter            (mongoc_rpc_t                 *rpc,
                                     const uint8_t                *buf,
                                     size_t                        buflen);
bool _mongoc_rpc_reply_get_first    (mongoc_rpc_reply_t           *reply,
                                     bson_t                       *bson);
void _mongoc_rpc_prep_command       (mongoc_rpc_t                 *rpc,
                                     const char                   *cmd_ns,
                                     const bson_t                 *command,
                                     mongoc_query_flags_t          flags);
bool _mongoc_rpc_parse_command_error(mongoc_rpc_t                 *rpc,
                                     bson_error_t                 *error);
bool _mongoc_rpc_parse_query_error  (mongoc_rpc_t                 *rpc,
                                     bson_error_t                 *error);


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
   mongoc_read_concern_t  *read_concern;
   mongoc_write_concern_t *write_concern;
   bson_t                 *gle;
};


mongoc_collection_t *_mongoc_collection_new                  (mongoc_client_t              *client,
                                                              const char                   *db,
                                                              const char                   *collection,
                                                              const mongoc_read_prefs_t    *read_prefs,
                                                              const mongoc_read_concern_t  *read_concern,
                                                              const mongoc_write_concern_t *write_concern);
mongoc_cursor_t    *_mongoc_collection_find_indexes_legacy   (mongoc_collection_t          *collection,
                                                              bson_error_t                 *error);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-counters-private.h */

BSON_BEGIN_DECLS


void _mongoc_counters_init (void);
void _mongoc_counters_cleanup (void);


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
/* --- mongoc-set-private.h */

BSON_BEGIN_DECLS

typedef void (*mongoc_set_item_dtor)(void *item,
                                     void *ctx);

/* return true to continue iteration, false to stop */
typedef bool (*mongoc_set_for_each_cb_t)(void *item,
                                         void *ctx);

typedef struct
{
   uint32_t id;
   void    *item;
} mongoc_set_item_t;

typedef struct
{
   mongoc_set_item_t   *items;
   size_t               items_len;
   size_t               items_allocated;
   mongoc_set_item_dtor dtor;
   void                *dtor_ctx;
} mongoc_set_t;

mongoc_set_t *
mongoc_set_new (size_t               nitems,
                mongoc_set_item_dtor dtor,
                void                *dtor_ctx);

void
mongoc_set_add (mongoc_set_t *set,
                uint32_t      id,
                void         *item);

void
mongoc_set_rm (mongoc_set_t *set,
               uint32_t      id);

void *
mongoc_set_get (mongoc_set_t *set,
                uint32_t      id)
	BSON_GNUC_PURE;

void *
mongoc_set_get_item (mongoc_set_t *set,
                     int           idx);

void
mongoc_set_destroy (mongoc_set_t *set);

/* loops over the set safe-ish.
 *
 * Caveats:
 *   - you can add items at any iteration
 *   - if you remove elements other than the one you're currently looking at,
 *     you may see it later in the iteration
 */
void
mongoc_set_for_each (mongoc_set_t            *set,
                     mongoc_set_for_each_cb_t cb,
                     void                    *ctx);

/* first item in set for which "cb" returns true */
void *
mongoc_set_find_item (mongoc_set_t            *set,
                      mongoc_set_for_each_cb_t cb,
                      void                    *ctx);

/* id of first item in set for which "cb" returns true, or 0. */
uint32_t
mongoc_set_find_id (mongoc_set_t            *set,
                    mongoc_set_for_each_cb_t cb,
                    void                    *ctx);

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-topology-description-private.h */

BSON_BEGIN_DECLS

typedef enum
   {
      MONGOC_TOPOLOGY_UNKNOWN,
      MONGOC_TOPOLOGY_SHARDED,
      MONGOC_TOPOLOGY_RS_NO_PRIMARY,
      MONGOC_TOPOLOGY_RS_WITH_PRIMARY,
      MONGOC_TOPOLOGY_SINGLE,
      MONGOC_TOPOLOGY_DESCRIPTION_TYPES
   } mongoc_topology_description_type_t;

typedef struct _mongoc_topology_description_t
{
   mongoc_topology_description_type_t type;
   mongoc_set_t                      *servers;
   char                              *set_name;
   int64_t                            max_set_version;
   bson_oid_t                         max_election_id;
   bool                               compatible;
   char                              *compatibility_error;
   uint32_t                           max_server_id;
   bool                               stale;
} mongoc_topology_description_t;

typedef enum
   {
      MONGOC_SS_READ,
      MONGOC_SS_WRITE
   } mongoc_ss_optype_t;

void
mongoc_topology_description_init (mongoc_topology_description_t     *description,
                                  mongoc_topology_description_type_t type);

void
mongoc_topology_description_destroy (mongoc_topology_description_t *description);

void
mongoc_topology_description_handle_ismaster (
   mongoc_topology_description_t *topology,
   mongoc_server_description_t   *sd,
   const bson_t                  *reply,
   int64_t                        rtt_msec,
   bson_error_t                  *error);

mongoc_server_description_t *
mongoc_topology_description_select (mongoc_topology_description_t *description,
                                    mongoc_ss_optype_t             optype,
                                    const mongoc_read_prefs_t     *read_pref,
                                    int64_t                        local_threshold_ms);

mongoc_server_description_t *
mongoc_topology_description_server_by_id (mongoc_topology_description_t *description,
                                          uint32_t                       id,
                                          bson_error_t                  *error);

void
mongoc_topology_description_suitable_servers (
   mongoc_array_t                *set, /* OUT */
   mongoc_ss_optype_t             optype,
   mongoc_topology_description_t *topology,
   const mongoc_read_prefs_t     *read_pref,
   size_t                         local_threshold_ms);

void
mongoc_topology_description_invalidate_server (mongoc_topology_description_t *topology,
                                               uint32_t                       id,
                                               const bson_error_t            *error);

bool
mongoc_topology_description_add_server (mongoc_topology_description_t *topology,
                                        const char                    *server,
                                        uint32_t                      *id /* OUT */);

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-server-stream-private.h */

BSON_BEGIN_DECLS

typedef struct _mongoc_server_stream_t
{
   mongoc_topology_description_type_t  topology_type;
   mongoc_server_description_t        *sd;            /* owned */
   mongoc_stream_t                    *stream;        /* borrowed */
} mongoc_server_stream_t;


mongoc_server_stream_t *
mongoc_server_stream_new (mongoc_topology_description_type_t topology_type,
                          mongoc_server_description_t *sd,
                          mongoc_stream_t *stream);

int32_t
mongoc_server_stream_max_bson_obj_size (mongoc_server_stream_t *server_stream)
	BSON_GNUC_PURE;

int32_t
mongoc_server_stream_max_msg_size (mongoc_server_stream_t *server_stream)
	BSON_GNUC_PURE;

int32_t
mongoc_server_stream_max_write_batch_size (mongoc_server_stream_t *server_stream)
	BSON_GNUC_PURE;

void
mongoc_server_stream_cleanup (mongoc_server_stream_t *server_stream);

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

   uint32_t                   server_id;
   uint32_t                   stamp;

   unsigned                   is_command      : 1;
   unsigned                   sent            : 1;
   unsigned                   done            : 1;
   unsigned                   end_of_event    : 1;
   unsigned                   has_fields      : 1;
   unsigned                   in_exhaust      : 1;

   bson_t                     query;
   bson_t                     fields;

   mongoc_read_concern_t     *read_concern;
   mongoc_read_prefs_t       *read_prefs;

   mongoc_query_flags_t       flags;
   uint32_t                   skip;
   int64_t                    limit;
   uint32_t                   count;
   uint32_t                   batch_size;
   uint32_t                   max_await_time_ms;

   char                       ns [140];
   uint32_t                   nslen;
   uint32_t                   dblen;

   bson_error_t               error;

   /* for OP_QUERY and OP_GETMORE replies*/
   mongoc_rpc_t               rpc;
   mongoc_buffer_t            buffer;
   bson_reader_t             *reader;
   const bson_t              *current;

   mongoc_cursor_interface_t  iface;
   void                      *iface_data;

   int64_t                    operation_id;
};


void                      _mongoc_set_cursor_ns       (mongoc_cursor_t              *cursor,
                                                       const char                   *ns,
                                                       uint32_t                      nslen);
mongoc_cursor_t         * _mongoc_cursor_new          (mongoc_client_t              *client,
                                                       const char                   *db_and_collection,
                                                       mongoc_query_flags_t          flags,
                                                       uint32_t                      skip,
                                                       int32_t                       limit,
                                                       uint32_t                      batch_size,
                                                       bool                          is_command,
                                                       const bson_t                 *query,
                                                       const bson_t                 *fields,
                                                       const mongoc_read_prefs_t    *read_prefs,
                                                       const mongoc_read_concern_t  *read_concern);
mongoc_cursor_t         *_mongoc_cursor_clone         (const mongoc_cursor_t        *cursor);
void                     _mongoc_cursor_destroy       (mongoc_cursor_t              *cursor);
bool                     _mongoc_read_from_buffer     (mongoc_cursor_t              *cursor,
                                                       const bson_t                **bson);
bool                     _use_find_command            (const mongoc_cursor_t        *cursor,
                                                       const mongoc_server_stream_t *server_stream)
	BSON_GNUC_PURE;
mongoc_server_stream_t * _mongoc_cursor_fetch_stream  (mongoc_cursor_t              *cursor);
void                     _mongoc_cursor_collection    (const mongoc_cursor_t        *cursor,
                                                       const char                  **collection,
                                                       int                          *collection_len);
bool                     _mongoc_cursor_op_getmore    (mongoc_cursor_t              *cursor,
                                                       mongoc_server_stream_t       *server_stream);
bool                     _mongoc_cursor_run_command   (mongoc_cursor_t              *cursor,
                                                       const bson_t                 *command,
                                                       bson_t                       *reply);
bool                     _mongoc_cursor_more          (mongoc_cursor_t              *cursor);
bool                     _mongoc_cursor_next          (mongoc_cursor_t              *cursor,
                                                       const bson_t                **bson);
bool                     _mongoc_cursor_error         (mongoc_cursor_t              *cursor,
                                                       bson_error_t                 *error);
void                     _mongoc_cursor_get_host      (mongoc_cursor_t              *cursor,
                                                       mongoc_host_list_t           *host);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-cursor-cursorid-private.h */

BSON_BEGIN_DECLS


typedef struct
{
   bson_t     *array;
   bool        in_batch;
   bool        in_reader;
   bson_iter_t batch_iter;
   bson_t      current_doc;
} mongoc_cursor_cursorid_t;


bool _mongoc_cursor_cursorid_start_batch    (mongoc_cursor_t  *cursor);
bool _mongoc_cursor_cursorid_prime          (mongoc_cursor_t  *cursor);
bool _mongoc_cursor_cursorid_next           (mongoc_cursor_t  *cursor,
                                             const bson_t    **bson);
void _mongoc_cursor_cursorid_init           (mongoc_cursor_t  *cursor,
                                             const bson_t     *command);
bool _mongoc_cursor_prepare_getmore_command (mongoc_cursor_t  *cursor,
                                             bson_t           *command);
void _mongoc_cursor_cursorid_init_with_reply (mongoc_cursor_t *cursor,
                                              bson_t          *reply,
                                              uint32_t         server_id);

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-cursor-array-private.h */

BSON_BEGIN_DECLS


void
_mongoc_cursor_array_init (mongoc_cursor_t *cursor,
                           const bson_t    *command,
                           const char      *field_name);

bool
_mongoc_cursor_array_prime (mongoc_cursor_t *cursor);


void
_mongoc_cursor_array_set_bson (mongoc_cursor_t *cursor,
                               const bson_t    *bson);


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
bool                       _mongoc_gridfs_file_page_memset0  (mongoc_gridfs_file_page_t *page,
                                                              uint32_t                   len);
uint32_t                   _mongoc_gridfs_file_page_tell     (mongoc_gridfs_file_page_t *page)
	BSON_GNUC_PURE;
const uint8_t             *_mongoc_gridfs_file_page_get_data (mongoc_gridfs_file_page_t *page)
	BSON_GNUC_PURE;
uint32_t                   _mongoc_gridfs_file_page_get_len  (mongoc_gridfs_file_page_t *page)
	BSON_GNUC_PURE;
bool                       _mongoc_gridfs_file_page_is_dirty (mongoc_gridfs_file_page_t *page)
	BSON_GNUC_PURE;


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
   int32_t                    n;
   bson_error_t               error;
   mongoc_cursor_t           *cursor;
   uint32_t                   cursor_range[2]; /* current chunk, # of chunks */
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

bool _mongoc_host_list_equal (const mongoc_host_list_t *host_a,
                              const mongoc_host_list_t *host_b)
	BSON_GNUC_PURE;

void _mongoc_host_list_destroy_all (mongoc_host_list_t *host);

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
   mongoc_matcher_op_not_t not_;
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

#ifdef MONGOC_ENABLE_SASL

BSON_BEGIN_DECLS


typedef struct _mongoc_sasl_t mongoc_sasl_t;


struct _mongoc_sasl_t
{
   sasl_callback_t  callbacks [4];
   sasl_conn_t     *conn;
   bool             done;
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

#endif	/* MONGOC_ENABLE_SASL */

/*==============================================================*/
/* --- mongoc-openssl-private.h */

#ifdef MONGOC_ENABLE_SSL
#ifdef MONGOC_ENABLE_OPENSSL
BSON_BEGIN_DECLS


bool     _mongoc_openssl_check_cert      (SSL              *ssl,
                                          const char       *host,
                                          bool              weak_cert_validation);
SSL_CTX *_mongoc_openssl_ctx_new         (mongoc_ssl_opt_t *opt);
char    *_mongoc_openssl_extract_subject (const char       *filename);
void     _mongoc_openssl_init            (void);
void     _mongoc_openssl_cleanup         (void);


BSON_END_DECLS
#endif	/* MONGOC_ENABLE_OPENSSL */
#endif	/* MONGOC_ENABLE_SSL */

/*==============================================================*/
/* --- mongoc-stream-private.h */

BSON_BEGIN_DECLS


#define MONGOC_STREAM_SOCKET   1
#define MONGOC_STREAM_FILE     2
#define MONGOC_STREAM_BUFFERED 3
#define MONGOC_STREAM_GRIDFS   4
#define MONGOC_STREAM_TLS      5

bool
mongoc_stream_wait (mongoc_stream_t *stream,
                    int64_t expire_at);

bool
_mongoc_stream_writev_full (mongoc_stream_t *stream,
                            mongoc_iovec_t  *iov,
                            size_t           iovcnt,
                            int32_t          timeout_msec,
                            bson_error_t    *error);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-thread-private.h */

#if !defined(_WIN32)
# include <pthread.h>
# define MONGOC_MUTEX_INITIALIZER       PTHREAD_MUTEX_INITIALIZER
# define mongoc_cond_t                  pthread_cond_t
# define mongoc_cond_broadcast          pthread_cond_broadcast
# define mongoc_cond_init(_n)           pthread_cond_init((_n), NULL)
# define mongoc_cond_wait               pthread_cond_wait
# define mongoc_cond_signal             pthread_cond_signal
static BSON_INLINE int
mongoc_cond_timedwait (pthread_cond_t  *cond,
                       pthread_mutex_t *mutex,
                       int64_t         timeout_msec)
{
   struct timespec to;
   struct timeval tv;
   int64_t msec;

   bson_gettimeofday (&tv);

   msec = ((int64_t)tv.tv_sec * 1000) + (tv.tv_usec / 1000) + timeout_msec;

   to.tv_sec = msec / 1000;
   to.tv_nsec = (msec % 1000) * 1000 * 1000;

   return pthread_cond_timedwait (cond, mutex, &to);
}
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
   *thread = CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE) cb, arg, 0, NULL);
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
# define mongoc_cond_wait(_c, _m)       mongoc_cond_timedwait((_c), (_m), INFINITE)
static BSON_INLINE int
mongoc_cond_timedwait (mongoc_cond_t  *cond,
                       mongoc_mutex_t *mutex,
                       int64_t         timeout_msec)
{
   int r;

   if (SleepConditionVariableCS(cond, mutex, timeout_msec)) {
      return 0;
   } else {
      r = GetLastError();

      if (r == WAIT_TIMEOUT || r == ERROR_TIMEOUT) {
         return WSAETIMEDOUT;
      } else {
         return EINVAL;
      }
   }
}
# define mongoc_cond_signal             WakeConditionVariable
# define mongoc_cond_broadcast          WakeAllConditionVariable
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

/* string comparison functions for Windows */
#ifdef _WIN32
# define strcasecmp  _stricmp
# define strncasecmp _strnicmp
#endif

/* Suppress CWE-252 ("Unchecked return value") warnings for things we can't deal with */
#if defined(__GNUC__) && __GNUC__ >= 4
# define _ignore_value(x) (({ __typeof__ (x) __x = (x); (void) __x; }))
#else
# define _ignore_value(x) ((void) (x))
#endif


BSON_BEGIN_DECLS


char *_mongoc_hex_md5 (const char *input);

void _mongoc_usleep (int64_t usec);

const char *_mongoc_get_command_name (const bson_t *command);

void _mongoc_get_db_name (const char *ns,
                          char *db /* OUT */);

void _mongoc_bson_destroy_if_set (bson_t *bson);

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-write-concern-private.h */

BSON_BEGIN_DECLS


#define MONGOC_WRITE_CONCERN_FSYNC_DEFAULT   -1
#define MONGOC_WRITE_CONCERN_JOURNAL_DEFAULT -1


struct _mongoc_write_concern_t
{
   int8_t    fsync_;
   int8_t    journal;
   int32_t   w;
   int32_t   wtimeout;
   char     *wtag;
   bool      frozen;
   bson_t    compiled;
   bson_t    compiled_gle;
};


const bson_t *_mongoc_write_concern_get_gle   (mongoc_write_concern_t       *write_concern);
const bson_t *_mongoc_write_concern_get_bson  (mongoc_write_concern_t       *write_concern);

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-write-command-private.h */

BSON_BEGIN_DECLS


#define MONGOC_WRITE_COMMAND_DELETE 0
#define MONGOC_WRITE_COMMAND_INSERT 1
#define MONGOC_WRITE_COMMAND_UPDATE 2


typedef enum
{
   MONGOC_BYPASS_DOCUMENT_VALIDATION_FALSE   = 0,
   MONGOC_BYPASS_DOCUMENT_VALIDATION_TRUE    = 1 << 0,
   MONGOC_BYPASS_DOCUMENT_VALIDATION_DEFAULT = 1 << 1,
} mongoc_write_bypass_document_validation_t;

struct _mongoc_bulk_write_flags_t
{
   bool ordered;
   mongoc_write_bypass_document_validation_t bypass_document_validation;
};


typedef struct
{
   int      type;
   uint32_t server_id;
   bson_t  *documents;
   uint32_t n_documents;
   mongoc_bulk_write_flags_t flags;
   int64_t operation_id;
   union {
      struct {
         bool multi;
      } delete_;
      struct {
         bool allow_bulk_op_insert;
      } insert;
   } u;
} mongoc_write_command_t;


typedef struct
{
   /* true after a legacy update prevents us from calculating nModified */
   bool         omit_nModified;
   uint32_t     nInserted;
   uint32_t     nMatched;
   uint32_t     nModified;
   uint32_t     nRemoved;
   uint32_t     nUpserted;
   /* like [{"index": int, "_id": value}, ...] */
   bson_t       writeErrors;
   /* like [{"index": int, "code": int, "errmsg": str}, ...] */
   bson_t       upserted;
   /* like [{"code": 64, "errmsg": "duplicate"}, ...] */
   uint32_t     n_writeConcernErrors;
   bson_t       writeConcernErrors;
   bool         failed;
   bson_error_t error;
   uint32_t     upsert_append_count;
} mongoc_write_result_t;


void _mongoc_write_command_destroy     (mongoc_write_command_t        *command);
void _mongoc_write_command_init_insert (mongoc_write_command_t        *command,
                                        const bson_t                  *document,
                                        mongoc_bulk_write_flags_t      flags,
                                        int64_t                        operation_id,
                                        bool                           allow_bulk_op_insert);
void _mongoc_write_command_init_delete (mongoc_write_command_t        *command,
                                        const bson_t                  *selectors,
                                        bool                           multi,
                                        mongoc_bulk_write_flags_t      flags,
                                        int64_t                        operation_id);
void _mongoc_write_command_init_update (mongoc_write_command_t        *command,
                                        const bson_t                  *selector,
                                        const bson_t                  *update,
                                        bool                           upsert,
                                        bool                           multi,
                                        mongoc_bulk_write_flags_t      flags,
                                        int64_t                        operation_id);
void _mongoc_write_command_insert_append (mongoc_write_command_t      *command,
                                          const bson_t                *document);
void _mongoc_write_command_update_append (mongoc_write_command_t      *command,
                                          const bson_t                *selector,
                                          const bson_t                *update,
                                          bool                         upsert,
                                          bool                         multi);

void _mongoc_write_command_delete_append (mongoc_write_command_t *command,
                                          const bson_t           *selector);

void _mongoc_write_command_execute     (mongoc_write_command_t        *command,
                                        mongoc_client_t               *client,
                                        mongoc_server_stream_t        *server_stream,
                                        const char                    *database,
                                        const char                    *collection,
                                        const mongoc_write_concern_t  *write_concern,
                                        uint32_t                       offset,
                                        mongoc_write_result_t         *result);
void _mongoc_write_result_init         (mongoc_write_result_t         *result);
void _mongoc_write_result_merge        (mongoc_write_result_t         *result,
                                        mongoc_write_command_t        *command,
                                        const bson_t                  *reply,
                                        uint32_t                       offset);
void _mongoc_write_result_merge_legacy (mongoc_write_result_t         *result,
                                        mongoc_write_command_t        *command,
                                        const bson_t                  *reply,
                                        mongoc_error_code_t            default_code,
                                        uint32_t                       offset);
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
   mongoc_read_prefs_t    *read_prefs;
   mongoc_read_concern_t  *read_concern;
   mongoc_write_concern_t *write_concern;
};


mongoc_database_t *_mongoc_database_new                         (mongoc_client_t              *client,
                                                                 const char                   *name,
                                                                 const mongoc_read_prefs_t    *read_prefs,
                                                                 const mongoc_read_concern_t  *read_concern,
                                                                 const mongoc_write_concern_t *write_concern);
mongoc_cursor_t   *_mongoc_database_find_collections_legacy     (mongoc_database_t            *database,
                                                                 const bson_t                 *filter,
                                                                 bson_error_t                 *error);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-bulk-operation-private.h */

BSON_BEGIN_DECLS

struct _mongoc_bulk_operation_t
{
   char                          *database;
   char                          *collection;
   mongoc_client_t               *client;
   mongoc_write_concern_t        *write_concern;
   mongoc_bulk_write_flags_t      flags;
   uint32_t                       server_id;
   mongoc_array_t                 commands;
   mongoc_write_result_t          result;
   bool                           executed;
   int64_t                        operation_id;
};


mongoc_bulk_operation_t *_mongoc_bulk_operation_new (mongoc_client_t               *client,
                                                     const char                    *database,
                                                     const char                    *collection,
                                                     mongoc_bulk_write_flags_t      flags,
                                                     const mongoc_write_concern_t  *write_concern);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-cluster-private.h */

BSON_BEGIN_DECLS


typedef struct _mongoc_cluster_node_t
{
   mongoc_stream_t *stream;

   int32_t          max_wire_version;
   int32_t          min_wire_version;
   int32_t          max_write_batch_size;
   int32_t          max_bson_obj_size;
   int32_t          max_msg_size;

   int64_t          timestamp;
} mongoc_cluster_node_t;

typedef struct _mongoc_cluster_t
{
   int64_t          operation_id;
   uint32_t         request_id;
   uint32_t         sockettimeoutms;
   uint32_t         socketcheckintervalms;
   mongoc_uri_t    *uri;
   unsigned         requires_auth : 1;

   mongoc_client_t *client;

   mongoc_set_t    *nodes;
   mongoc_array_t   iov;
} mongoc_cluster_t;

void
mongoc_cluster_init (mongoc_cluster_t   *cluster,
                     const mongoc_uri_t *uri,
                     void               *client);

void
mongoc_cluster_destroy (mongoc_cluster_t *cluster);

void
mongoc_cluster_disconnect_node (mongoc_cluster_t *cluster,
                                uint32_t          id);

int32_t
mongoc_cluster_get_max_bson_obj_size (mongoc_cluster_t *cluster);

int32_t
mongoc_cluster_get_max_msg_size (mongoc_cluster_t *cluster);

int32_t
mongoc_cluster_node_max_wire_version (mongoc_cluster_t *cluster,
                                      uint32_t          server_id);

int32_t
mongoc_cluster_node_min_wire_version (mongoc_cluster_t *cluster,
                                      uint32_t          server_id);

bool
mongoc_cluster_sendv_to_server (mongoc_cluster_t             *cluster,
                                mongoc_rpc_t                 *rpcs,
                                size_t                        rpcs_len,
                                mongoc_server_stream_t       *server_stream,
                                const mongoc_write_concern_t *write_concern,
                                bson_error_t                 *error);

bool
mongoc_cluster_try_recv (mongoc_cluster_t       *cluster,
                         mongoc_rpc_t           *rpc,
                         mongoc_buffer_t        *buffer,
                         mongoc_server_stream_t *server_stream,
                         bson_error_t           *error);

mongoc_server_stream_t *
mongoc_cluster_stream_for_reads (mongoc_cluster_t *cluster,
                                 const mongoc_read_prefs_t *read_prefs,
                                 bson_error_t *error);

mongoc_server_stream_t *
mongoc_cluster_stream_for_writes (mongoc_cluster_t *cluster,
                                  bson_error_t *error);

mongoc_server_stream_t *
mongoc_cluster_stream_for_server (mongoc_cluster_t *cluster,
                                  uint32_t server_id,
                                  bool reconnect_ok,
                                  bson_error_t *error);

bool
mongoc_cluster_run_command_monitored (mongoc_cluster_t         *cluster,
                                      mongoc_server_stream_t   *server_stream,
                                      mongoc_query_flags_t      flags,
                                      const char               *db_name,
                                      const bson_t             *command,
                                      bson_t                   *reply,
                                      bson_error_t             *error);

bool
mongoc_cluster_run_command (mongoc_cluster_t    *cluster,
                            mongoc_stream_t     *stream,
                            uint32_t             server_id,
                            mongoc_query_flags_t flags,
                            const char          *db_name,
                            const bson_t        *command,
                            bson_t              *reply,
                            bson_error_t        *error);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-read-prefs-private.h */

BSON_BEGIN_DECLS

struct _mongoc_read_prefs_t
{
   mongoc_read_mode_t mode;
   bson_t             tags;
};


typedef struct _mongoc_apply_read_prefs_result_t {
   bson_t              *query_with_read_prefs;
   bool                 query_owned;
   mongoc_query_flags_t flags;
} mongoc_apply_read_prefs_result_t;


#define READ_PREFS_RESULT_INIT { NULL, false, MONGOC_QUERY_NONE }

void
apply_read_preferences (const mongoc_read_prefs_t *read_prefs,
                        const mongoc_server_stream_t *server_stream,
                        const bson_t *query_bson,
                        mongoc_query_flags_t initial_flags,
                        mongoc_apply_read_prefs_result_t *result);

void
apply_read_prefs_result_cleanup (mongoc_apply_read_prefs_result_t *result);

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-async-private.h */

BSON_BEGIN_DECLS

struct _mongoc_async_cmd;

typedef struct _mongoc_async
{
   struct _mongoc_async_cmd *cmds;
   size_t                    ncmds;
   uint32_t                  request_id;
} mongoc_async_t;

typedef enum
{
   MONGOC_ASYNC_CMD_IN_PROGRESS,
   MONGOC_ASYNC_CMD_SUCCESS,
   MONGOC_ASYNC_CMD_ERROR,
   MONGOC_ASYNC_CMD_TIMEOUT,
} mongoc_async_cmd_result_t;

typedef void (*mongoc_async_cmd_cb_t)(mongoc_async_cmd_result_t result,
                                      const bson_t             *bson,
                                      int64_t                   rtt_msec,
                                      void                     *data,
                                      bson_error_t             *error);

typedef int
(*mongoc_async_cmd_setup_t)(mongoc_stream_t *stream,
                            int             *events,
                            void            *ctx,
                            int32_t         timeout_msec,
                            bson_error_t    *error);


mongoc_async_t *
mongoc_async_new ();

void
mongoc_async_destroy (mongoc_async_t *async);

bool
mongoc_async_run (mongoc_async_t *async,
                  int32_t         timeout_msec);

struct _mongoc_async_cmd *
mongoc_async_cmd (mongoc_async_t          *async,
                  mongoc_stream_t         *stream,
                  mongoc_async_cmd_setup_t setup,
                  void                    *setup_ctx,
                  const char              *dbname,
                  const bson_t            *cmd,
                  mongoc_async_cmd_cb_t    cb,
                  void                    *cb_data,
                  int32_t                  timeout_msec);

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-async-cmd-private.h */

BSON_BEGIN_DECLS

typedef enum
{
   MONGOC_ASYNC_CMD_SETUP,
   MONGOC_ASYNC_CMD_SEND,
   MONGOC_ASYNC_CMD_RECV_LEN,
   MONGOC_ASYNC_CMD_RECV_RPC,
   MONGOC_ASYNC_CMD_ERROR_STATE,
   MONGOC_ASYNC_CMD_CANCELED_STATE,
} mongoc_async_cmd_state_t;

typedef struct _mongoc_async_cmd
{
   mongoc_stream_t *stream;

   mongoc_async_t          *async;
   mongoc_async_cmd_state_t state;
   int                      events;
   mongoc_async_cmd_setup_t setup;
   void                    *setup_ctx;
   mongoc_async_cmd_cb_t    cb;
   void                    *data;
   bson_error_t             error;
   int64_t                  start_time;
   int64_t                  expire_at;
   bson_t                   cmd;
   mongoc_buffer_t          buffer;
   mongoc_array_t           array;
   mongoc_iovec_t          *iovec;
   size_t                   niovec;
   size_t                   bytes_to_read;
   mongoc_rpc_t             rpc;
   bson_t                   reply;
   bool                     reply_needs_cleanup;
   char                     ns[MONGOC_NAMESPACE_MAX];

   struct _mongoc_async_cmd *next;
   struct _mongoc_async_cmd *prev;
} mongoc_async_cmd_t;

mongoc_async_cmd_t *
mongoc_async_cmd_new (mongoc_async_t           *async,
                      mongoc_stream_t          *stream,
                      mongoc_async_cmd_setup_t  setup,
                      void                     *setup_ctx,
                      const char               *dbname,
                      const bson_t             *cmd,
                      mongoc_async_cmd_cb_t     cb,
                      void                     *cb_data,
                      int32_t                   timeout_msec);

void
mongoc_async_cmd_destroy (mongoc_async_cmd_t *acmd);

bool
mongoc_async_cmd_run (mongoc_async_cmd_t *acmd);

#ifdef MONGOC_ENABLE_SSL
int
mongoc_async_cmd_tls_setup (mongoc_stream_t *stream,
                            int             *events,
                            void            *ctx,
                            int32_t         timeout_msec,
                            bson_error_t    *error);
#endif

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-topology-scanner-private.h */

BSON_BEGIN_DECLS

typedef void (*mongoc_topology_scanner_cb_t)(uint32_t      id,
                                             const bson_t *bson,
                                             int64_t       rtt,
                                             void         *data,
                                             bson_error_t *error);

struct mongoc_topology_scanner;

typedef struct mongoc_topology_scanner_node
{
   uint32_t                        id;
   mongoc_async_cmd_t             *cmd;
   mongoc_stream_t                *stream;
   int64_t                         timestamp;
   int64_t                         last_used;
   int64_t                         last_failed;
   bool                            has_auth;
   mongoc_host_list_t              host;
   struct addrinfo                *dns_results;
   struct addrinfo                *current_dns_result;
   struct mongoc_topology_scanner *ts;

   struct mongoc_topology_scanner_node *next;
   struct mongoc_topology_scanner_node *prev;

   bool                            retired;
   bson_error_t                    last_error;
} mongoc_topology_scanner_node_t;

typedef struct mongoc_topology_scanner
{
   mongoc_async_t                 *async;
   mongoc_topology_scanner_node_t *nodes;
   uint32_t                        seq;
   bson_t                          ismaster_cmd;
   mongoc_topology_scanner_cb_t    cb;
   void                           *cb_data;
   bool                            in_progress;
   const mongoc_uri_t             *uri;
   mongoc_async_cmd_setup_t        setup;
   mongoc_stream_initiator_t       initiator;
   void                           *initiator_context;

#ifdef MONGOC_ENABLE_SSL
   mongoc_ssl_opt_t *ssl_opts;
#endif
} mongoc_topology_scanner_t;

mongoc_topology_scanner_t *
mongoc_topology_scanner_new (const mongoc_uri_t          *uri,
                             mongoc_topology_scanner_cb_t cb,
                             void                        *data);

void
mongoc_topology_scanner_destroy (mongoc_topology_scanner_t *ts);

mongoc_topology_scanner_node_t *
mongoc_topology_scanner_add (mongoc_topology_scanner_t *ts,
                             const mongoc_host_list_t  *host,
                             uint32_t                   id);

void
mongoc_topology_scanner_add_and_scan (mongoc_topology_scanner_t *ts,
                                      const mongoc_host_list_t  *host,
                                      uint32_t                   id,
                                      int64_t                    timeout_msec);

void
mongoc_topology_scanner_node_retire (mongoc_topology_scanner_node_t *node);

void
mongoc_topology_scanner_node_disconnect (mongoc_topology_scanner_node_t *node,
                                         bool failed);

void
mongoc_topology_scanner_node_destroy (mongoc_topology_scanner_node_t *node,
                                      bool failed);

void
mongoc_topology_scanner_start (mongoc_topology_scanner_t *ts,
                               int32_t timeout_msec,
                               bool obey_cooldown);

bool
mongoc_topology_scanner_work (mongoc_topology_scanner_t *ts,
                              int32_t                    timeout_msec);

void
mongoc_topology_scanner_sum_errors (mongoc_topology_scanner_t *ts,
                                    bson_error_t              *error);

void
mongoc_topology_scanner_reset (mongoc_topology_scanner_t *ts);

bool
mongoc_topology_scanner_node_setup (mongoc_topology_scanner_node_t *node,
                                    bson_error_t *error);

mongoc_topology_scanner_node_t *
mongoc_topology_scanner_get_node (mongoc_topology_scanner_t *ts,
                                  uint32_t                   id)
	BSON_GNUC_PURE;

bool
mongoc_topology_scanner_has_node_for_host (mongoc_topology_scanner_t *ts,
                                           mongoc_host_list_t        *host)
	BSON_GNUC_PURE;

void
mongoc_topology_scanner_set_stream_initiator (mongoc_topology_scanner_t *ts,
                                              mongoc_stream_initiator_t  si,
                                              void                      *ctx);

#ifdef MONGOC_ENABLE_SSL
void
mongoc_topology_scanner_set_ssl_opts (mongoc_topology_scanner_t *ts,
                                      mongoc_ssl_opt_t          *opts);
#endif

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-server-description-private.h */

BSON_BEGIN_DECLS

#define MONGOC_DEFAULT_WIRE_VERSION 0
#define MONGOC_DEFAULT_WRITE_BATCH_SIZE 1000
#define MONGOC_DEFAULT_BSON_OBJ_SIZE 16 * 1024 * 1024
#define MONGOC_DEFAULT_MAX_MSG_SIZE 48000000

/* represent a server or topology with no replica set config version */
#define MONGOC_NO_SET_VERSION -1

typedef enum
   {
      MONGOC_SERVER_UNKNOWN,
      MONGOC_SERVER_STANDALONE,
      MONGOC_SERVER_MONGOS,
      MONGOC_SERVER_POSSIBLE_PRIMARY,
      MONGOC_SERVER_RS_PRIMARY,
      MONGOC_SERVER_RS_SECONDARY,
      MONGOC_SERVER_RS_ARBITER,
      MONGOC_SERVER_RS_OTHER,
      MONGOC_SERVER_RS_GHOST,
      MONGOC_SERVER_DESCRIPTION_TYPES,
   } mongoc_server_description_type_t;

struct _mongoc_server_description_t
{
   uint32_t                         id;
   mongoc_host_list_t               host;
   int64_t                          round_trip_time;
   bson_t                           last_is_master;
   bool                             has_is_master;
   const char                      *connection_address;
   const char                      *me;

   /* The following fields are filled from the last_is_master and are zeroed on
    * parse.  So order matters here.  DON'T move set_name */
   const char                      *set_name;
   bson_error_t                     error;
   mongoc_server_description_type_t type;

   int32_t                          min_wire_version;
   int32_t                          max_wire_version;
   int32_t                          max_msg_size;
   int32_t                          max_bson_obj_size;
   int32_t                          max_write_batch_size;

   bson_t                           hosts;
   bson_t                           passives;
   bson_t                           arbiters;

   bson_t                           tags;
   const char                      *current_primary;
   int64_t                          set_version;
   bson_oid_t                       election_id;
};

void
mongoc_server_description_init (mongoc_server_description_t *sd,
                                const char                  *address,
                                uint32_t                     id);
bool
mongoc_server_description_has_rs_member (mongoc_server_description_t *description,
                                         const char                  *address);


bool
mongoc_server_description_has_set_version (mongoc_server_description_t *description)
	BSON_GNUC_PURE;

bool
mongoc_server_description_has_election_id (mongoc_server_description_t *description);

void
mongoc_server_description_cleanup (mongoc_server_description_t *sd);

void
mongoc_server_description_reset (mongoc_server_description_t *sd);

void
mongoc_server_description_set_state (mongoc_server_description_t     *description,
                                     mongoc_server_description_type_t type);
void
mongoc_server_description_set_set_version (mongoc_server_description_t *description,
                                           int64_t                      set_version);
void
mongoc_server_description_set_election_id (mongoc_server_description_t *description,
                                           const bson_oid_t            *election_id);
void
mongoc_server_description_update_rtt (mongoc_server_description_t *server,
                                      int64_t                      new_time);

void
mongoc_server_description_handle_ismaster (
   mongoc_server_description_t   *sd,
   const bson_t                  *reply,
   int64_t                        rtt_msec,
   bson_error_t                  *error);

size_t
mongoc_server_description_filter_eligible (
   mongoc_server_description_t **descriptions,
   size_t                        description_len,
   const mongoc_read_prefs_t    *read_prefs);

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-topology-private.h */

BSON_BEGIN_DECLS

#define MONGOC_TOPOLOGY_MIN_HEARTBEAT_FREQUENCY_MS 500
#define MONGOC_TOPOLOGY_SOCKET_CHECK_INTERVAL_MS 5000
#define MONGOC_TOPOLOGY_COOLDOWN_MS 5000
#define MONGOC_TOPOLOGY_LOCAL_THRESHOLD_MS 15000
#define MONGOC_TOPOLOGY_SERVER_SELECTION_TIMEOUT_MS 30000
#define MONGOC_TOPOLOGY_HEARTBEAT_FREQUENCY_MS_MULTI_THREADED 10000
#define MONGOC_TOPOLOGY_HEARTBEAT_FREQUENCY_MS_SINGLE_THREADED 60000

typedef enum {
   MONGOC_TOPOLOGY_BG_OFF,
   MONGOC_TOPOLOGY_BG_RUNNING,
   MONGOC_TOPOLOGY_BG_SHUTTING_DOWN,
} mongoc_topology_bg_state_t;

typedef struct _mongoc_topology_t
{
   mongoc_topology_description_t description;
   mongoc_uri_t                 *uri;
   mongoc_topology_scanner_t    *scanner;
   bool                          server_selection_try_once;

   int64_t                       last_scan;
   int64_t                       local_threshold_msec;
   int64_t                       connect_timeout_msec;
   int64_t                       server_selection_timeout_msec;
   int64_t                       heartbeat_msec;

   mongoc_mutex_t                mutex;
   mongoc_cond_t                 cond_client;
   mongoc_cond_t                 cond_server;
   mongoc_thread_t               thread;

   mongoc_topology_bg_state_t    bg_thread_state;
   bool                          scan_requested;
   bool                          scanning;
   bool                          got_ismaster;
   bool                          shutdown_requested;
   bool                          single_threaded;
   bool                          stale;
} mongoc_topology_t;

mongoc_topology_t *
mongoc_topology_new (const mongoc_uri_t *uri,
                     bool                single_threaded);

void
mongoc_topology_destroy (mongoc_topology_t *topology);

mongoc_server_description_t *
mongoc_topology_select (mongoc_topology_t         *topology,
                        mongoc_ss_optype_t         optype,
                        const mongoc_read_prefs_t *read_prefs,
                        bson_error_t              *error);

mongoc_server_description_t *
mongoc_topology_server_by_id (mongoc_topology_t *topology,
                              uint32_t           id,
                              bson_error_t      *error);

bool mongoc_topology_get_server_type (mongoc_topology_t                  *topology,
                                      uint32_t                            id,
                                      mongoc_topology_description_type_t *topology_type,
                                      mongoc_server_description_type_t   *server_type,
                                      bson_error_t                       *error);

void
mongoc_topology_request_scan (mongoc_topology_t *topology);

void
mongoc_topology_invalidate_server (mongoc_topology_t  *topology,
                                   uint32_t            id,
                                   const bson_error_t *error);

int64_t
mongoc_topology_server_timestamp (mongoc_topology_t *topology,
                                  uint32_t           id);

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-apm-private.h */

BSON_BEGIN_DECLS

struct _mongoc_apm_callbacks_t
{
   mongoc_apm_command_started_cb_t    started;
   mongoc_apm_command_succeeded_cb_t  succeeded;
   mongoc_apm_command_failed_cb_t     failed;
};

struct _mongoc_apm_command_started_t
{
   bson_t                   *command;
   bool                      command_owned;
   const char               *database_name;
   const char               *command_name;
   int64_t                   request_id;
   int64_t                   operation_id;
   const mongoc_host_list_t *host;
   uint32_t                  server_id;
   void                     *context;
};

struct _mongoc_apm_command_succeeded_t
{
   int64_t                   duration;
   const bson_t             *reply;
   const char               *command_name;
   int64_t                   request_id;
   int64_t                   operation_id;
   const mongoc_host_list_t *host;
   uint32_t                  server_id;
   void                     *context;
};

struct _mongoc_apm_command_failed_t
{
   int64_t                   duration;
   const char               *command_name;
   const bson_error_t       *error;
   int64_t                   request_id;
   int64_t                   operation_id;
   const mongoc_host_list_t *host;
   uint32_t                  server_id;
   void                     *context;
};

void
mongoc_apm_command_started_init (mongoc_apm_command_started_t *event,
                                 const bson_t                 *command,
                                 const char                   *database_name,
                                 const char                   *command_name,
                                 int64_t                       request_id,
                                 int64_t                       operation_id,
                                 const mongoc_host_list_t     *host,
                                 uint32_t                      server_id,
                                 void                         *context);

void
mongoc_apm_command_started_cleanup (mongoc_apm_command_started_t *event);

void
mongoc_apm_command_succeeded_init (mongoc_apm_command_succeeded_t *event,
                                   int64_t                         duration,
                                   const bson_t                   *reply,
                                   const char                     *command_name,
                                   int64_t                         request_id,
                                   int64_t                         operation_id,
                                   const mongoc_host_list_t       *host,
                                   uint32_t                        server_id,
                                   void                           *context);

void
mongoc_apm_command_succeeded_cleanup (mongoc_apm_command_succeeded_t *event)
	BSON_GNUC_CONST;

void
mongoc_apm_command_failed_init (mongoc_apm_command_failed_t *event,
                                int64_t                      duration,
                                const char                  *command_name,
                                bson_error_t                *error,
                                int64_t                      request_id,
                                int64_t                      operation_id,
                                const mongoc_host_list_t    *host,
                                uint32_t                     server_id,
                                void                        *context);

void
mongoc_apm_command_failed_cleanup (mongoc_apm_command_failed_t *event)
	BSON_GNUC_CONST;

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-client-private.h */

BSON_BEGIN_DECLS

/* protocol versions this driver can speak */
#define WIRE_VERSION_MIN 0
#define WIRE_VERSION_MAX 4

/* first version that supported aggregation cursors */
#define WIRE_VERSION_AGG_CURSOR 1
/* first version that supported "insert", "update", "delete" commands */
#define WIRE_VERSION_WRITE_CMD 2
/* first version when SCRAM-SHA-1 replaced MONGODB-CR as default auth mech */
#define WIRE_VERSION_SCRAM_DEFAULT 3
/* first version that supported "find" and "getMore" commands */
#define WIRE_VERSION_FIND_CMD 4
/* first version with "killCursors" command */
#define WIRE_VERSION_KILLCURSORS_CMD 4
/* first version when findAndModify accepts writeConcern */
#define WIRE_VERSION_FAM_WRITE_CONCERN 4
/* first version to support readConcern */
#define WIRE_VERSION_READ_CONCERN 4


struct _mongoc_client_t
{
   mongoc_list_t             *conns;
   mongoc_uri_t              *uri;
   mongoc_cluster_t           cluster;
   bool                       in_exhaust;

   mongoc_stream_initiator_t  initiator;
   void                      *initiator_data;

#ifdef MONGOC_ENABLE_SSL
   bool                       use_ssl;
   mongoc_ssl_opt_t           ssl_opts;
#endif

   mongoc_topology_t         *topology;

   mongoc_read_prefs_t       *read_prefs;
   mongoc_read_concern_t     *read_concern;
   mongoc_write_concern_t    *write_concern;

   mongoc_apm_callbacks_t     apm_callbacks;
   void                      *apm_context;
};


mongoc_client_t *
_mongoc_client_new_from_uri (const mongoc_uri_t *uri,
                             mongoc_topology_t  *topology);

mongoc_stream_t *
mongoc_client_default_stream_initiator (const mongoc_uri_t       *uri,
                                        const mongoc_host_list_t *host,
                                        void                     *user_data,
                                        bson_error_t             *error);

mongoc_stream_t *
_mongoc_client_create_stream (mongoc_client_t          *client,
                              const mongoc_host_list_t *host,
                              bson_error_t             *error);

bool
_mongoc_client_recv (mongoc_client_t        *client,
                     mongoc_rpc_t           *rpc,
                     mongoc_buffer_t        *buffer,
                     mongoc_server_stream_t *server_stream,
                     bson_error_t           *error);

bool
_mongoc_client_recv_gle (mongoc_client_t        *client,
                         mongoc_server_stream_t *server_stream,
                         bson_t                **gle_doc,
                         bson_error_t           *error);

#ifdef	DYING
void
_mongoc_topology_background_thread_start (mongoc_topology_t *topology);

void
_mongoc_topology_background_thread_stop (mongoc_topology_t *topology);
#endif

void
_mongoc_client_kill_cursor              (mongoc_client_t *client,
                                         uint32_t         server_id,
                                         int64_t          cursor_id,
                                         int64_t          operation_id,
                                         const char      *db,
                                         const char      *collection);

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-b64-private.h */

BSON_BEGIN_DECLS

int
mongoc_b64_ntop (uint8_t const *src,
                 size_t         srclength,
                 char          *target,
                 size_t         targsize);

void
mongoc_b64_initialize_rmap (void);

int
mongoc_b64_pton (char const *src,
                 uint8_t    *target,
                 size_t      targsize);

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-client-pool-private.h */

BSON_BEGIN_DECLS

size_t 				  mongoc_client_pool_get_size(mongoc_client_pool_t *pool);

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-crypto-private.h */

#ifdef MONGOC_ENABLE_CRYPTO

BSON_BEGIN_DECLS

#ifdef MONGOC_ENABLE_LIBCRYPTO
#define MONGOC_CRYPTO_TYPE 1
#elif defined(MONGOC_ENABLE_COMMON_CRYPTO)
#define MONGOC_CRYPTO_TYPE 2
#endif

typedef struct _mongoc_crypto_t mongoc_crypto_t;

typedef enum
{
   MONGOC_CRYPTO_OPENSSL = 1,
   MONGOC_CRYPTO_COMMON_CRYPTO = 2
} mongoc_crypto_types_t;

struct _mongoc_crypto_t
{
   void (*hmac_sha1) (mongoc_crypto_t     *crypto,
                      const void          *key,
                      int                  key_len,
                      const unsigned char *d,
                      int                  n,
                      unsigned char       *md /* OUT */);
   bool (*sha1)      (mongoc_crypto_t     *crypto,
                      const unsigned char *input,
                      const size_t         input_len,
                      unsigned char       *output /* OUT */);
};

void
mongoc_crypto_init (mongoc_crypto_t *crypto);

void
mongoc_crypto_hmac_sha1 (mongoc_crypto_t     *crypto,
                         const void          *key,
                         int                  key_len,
                         const unsigned char *d,
                         int                  n,
                         unsigned char       *md /* OUT */);

bool
mongoc_crypto_sha1      (mongoc_crypto_t     *crypto,
                         const unsigned char *input,
                         const size_t         input_len,
                         unsigned char       *output /* OUT */);


BSON_END_DECLS

#endif /* MONGOC_ENABLE_CRYPTO */

/*==============================================================*/
/* --- mongoc-crypto-common-crypto-private.h */

#ifdef MONGOC_ENABLE_CRYPTO
#ifdef MONGOC_ENABLE_COMMON_CRYPTO

BSON_BEGIN_DECLS

void
mongoc_crypto_common_crypto_hmac_sha1 (mongoc_crypto_t     *crypto,
                                       const void          *key,
                                       int                  key_len,
                                       const unsigned char *d,
                                       int                  n,
                                       unsigned char       *md /* OUT */);

bool
mongoc_crypto_common_crypto_sha1 (mongoc_crypto_t     *crypto,
                                  const unsigned char *input,
                                  const size_t         input_len,
                                  unsigned char       *output /* OUT */);

BSON_END_DECLS

#endif	/* MONGOC_ENABLE_COMMON_CRYPTO */
#endif	/* MONGOC_ENABLE_CRYPTO */

/*==============================================================*/
/* --- mongoc-crypto-openssl-private.h */

#ifdef MONGOC_ENABLE_CRYPTO
#ifdef MONGOC_ENABLE_LIBCRYPTO

BSON_BEGIN_DECLS

void
mongoc_crypto_openssl_hmac_sha1 (mongoc_crypto_t     *crypto,
                                 const void          *key,
                                 int                  key_len,
                                 const unsigned char *d,
                                 int                  n,
                                 unsigned char       *md /* OUT */);

bool
mongoc_crypto_openssl_sha1 (mongoc_crypto_t     *crypto,
                            const unsigned char *input,
                            const size_t         input_len,
                            unsigned char       *output /* OUT */);

BSON_END_DECLS

#endif	/* MONGOC_ENABLE_LIBCRYPTO */
#endif	/* MONGOC_ENABLE_CRYPTO */

/*==============================================================*/
/* --- mongoc-cursor-transform-private.h */

BSON_BEGIN_DECLS

typedef enum
{
   MONGO_CURSOR_TRANSFORM_DROP,
   MONGO_CURSOR_TRANSFORM_PASS,
   MONGO_CURSOR_TRANSFORM_MUTATE,
} mongoc_cursor_transform_mode_t;

typedef mongoc_cursor_transform_mode_t
(*mongoc_cursor_transform_filter_t)(const bson_t *bson,
                                    void         *ctx);

typedef void (*mongoc_cursor_transform_mutate_t)(const bson_t *bson,
                                                 bson_t       *out,
                                                 void         *ctx);

typedef void (*mongoc_cursor_transform_dtor_t)(void *ctx);

void
_mongoc_cursor_transform_init (mongoc_cursor_t                 *cursor,
                               mongoc_cursor_transform_filter_t filter,
                               mongoc_cursor_transform_mutate_t mutate,
                               mongoc_cursor_transform_dtor_t   dtor,
                               void                            *ctx);


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-find-and-modify-private.h */

BSON_BEGIN_DECLS

struct _mongoc_find_and_modify_opts_t
{
   bson_t                                    *sort;
   bson_t                                    *update;
   bson_t                                    *fields;
   mongoc_find_and_modify_flags_t             flags;
   mongoc_write_bypass_document_validation_t  bypass_document_validation;
};


BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-log-private.h */

BSON_BEGIN_DECLS

/* just for testing */
void _mongoc_log_get_handler (mongoc_log_func_t  *log_func,
                              void              **user_data);

bool _mongoc_log_trace_is_enabled (void)
	BSON_GNUC_CONST;

void
mongoc_log_trace_bytes       (const char *domain,
                              const uint8_t *_b,
                              size_t _l);

void
mongoc_log_trace_iovec       (const char *domain,
                              const mongoc_iovec_t *_iov,
                              size_t _iovcnt);

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-memcmp-private.h */

BSON_BEGIN_DECLS

/* WARNING: mongoc_memcmp() must be used to verify if two secret keys
 * are equal, in constant time.
 * It returns 0 if the keys are equal, and -1 if they differ.
 * This function is not designed for lexicographical comparisons.
 */
int mongoc_memcmp(const void * const b1_, const void * const b2_, size_t len)
	BSON_GNUC_PURE;

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-opcode-private.h */

BSON_BEGIN_DECLS

bool _mongoc_opcode_needs_primary(mongoc_opcode_t opcode)
	BSON_GNUC_CONST;

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-rand.h */

#ifdef MONGOC_ENABLE_OPENSSL
BSON_BEGIN_DECLS

void mongoc_rand_seed(const void* buf, int num);
void mongoc_rand_add(const void* buf, int num, double entropy);
int mongoc_rand_status(void);

BSON_END_DECLS
#endif	/* MONGOC_ENABLE_OPENSSL */

/*==============================================================*/
/* --- mongoc-rand-private.h */

BSON_BEGIN_DECLS

int _mongoc_rand_bytes(uint8_t * buf, int num);
int _mongoc_pseudo_rand_bytes(uint8_t * buf, int num);

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-read-concern-private.h */

BSON_BEGIN_DECLS


struct _mongoc_read_concern_t
{
   char   *level;
   bool    frozen;
   bson_t  compiled;
};


const bson_t *_mongoc_read_concern_get_bson  (mongoc_read_concern_t       *read_concern);

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-scram-private.h */

BSON_BEGIN_DECLS

#define MONGOC_SCRAM_HASH_SIZE 20

typedef struct _mongoc_scram_t
{
   bool                done;
   int                 step;
   char               *user;
   char               *pass;
   uint8_t             salted_password[MONGOC_SCRAM_HASH_SIZE];
   char                encoded_nonce[48];
   int32_t             encoded_nonce_len;
   uint8_t            *auth_message;
   uint32_t            auth_messagemax;
   uint32_t            auth_messagelen;
#ifdef MONGOC_ENABLE_CRYPTO
   mongoc_crypto_t     crypto;
#endif
} mongoc_scram_t;

void
_mongoc_scram_startup();

void
_mongoc_scram_init (mongoc_scram_t *scram);

void
_mongoc_scram_set_pass (mongoc_scram_t *scram,
                        const char     *pass);

void
_mongoc_scram_set_user (mongoc_scram_t *scram,
                        const char     *user);
void
_mongoc_scram_destroy (mongoc_scram_t *scram);

bool
_mongoc_scram_step (mongoc_scram_t *scram,
                    const uint8_t  *inbuf,
                    uint32_t        inbuflen,
                    uint8_t        *outbuf,
                    uint32_t        outbufmax,
                    uint32_t       *outbuflen,
                    bson_error_t   *error);

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-secure-transport-private.h */

#ifdef MONGOC_ENABLE_SSL
#if !defined(MONGOC_OPENSSL) && defined(MONGOC_ENABLE_SECURE_TRANSPORT)
BSON_BEGIN_DECLS


char *
_mongoc_secure_transport_extract_subject (const char *filename);


BSON_END_DECLS
#endif /* !defined(MONGOC_OPENSSL) && defined(MONGOC_ENABLE_SECURE_TRANSPORT) */
#endif	/* MONGOC_ENABLE_SSL */

/*==============================================================*/
/* --- mongoc-socket-private.h */

BSON_BEGIN_DECLS

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

mongoc_socket_t *mongoc_socket_accept_ex (mongoc_socket_t *sock,
                                          int64_t          expire_at,
                                          uint16_t        *port);

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-stream-tls-openssl-bio-private.h */

#ifdef MONGOC_ENABLE_OPENSSL

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

BSON_BEGIN_DECLS

int
mongoc_stream_tls_openssl_bio_create (BIO *b);

int
mongoc_stream_tls_openssl_bio_destroy (BIO *b);

int
mongoc_stream_tls_openssl_bio_read (BIO  *b,
                                    char *buf,
                                    int   len);

int
mongoc_stream_tls_openssl_bio_write (BIO        *b,
                                     const char *buf,
                                     int         len);

long
mongoc_stream_tls_openssl_bio_ctrl (BIO  *b,
                                    int   cmd,
                                    long  num,
                                    void *ptr)
	BSON_GNUC_CONST;

int
mongoc_stream_tls_openssl_bio_gets (BIO  *b,
                                    char *buf,
                                    int   len)
	BSON_GNUC_CONST;

int
mongoc_stream_tls_openssl_bio_puts (BIO        *b,
                                    const char *str);

BSON_END_DECLS

#endif /* MONGOC_ENABLE_OPENSSL */

/*==============================================================*/
/* --- mongoc-stream-tls-openssl.h */

#ifdef MONGOC_ENABLE_SSL
#ifdef MONGOC_ENABLE_OPENSSL

BSON_BEGIN_DECLS

mongoc_stream_t *
mongoc_stream_tls_openssl_new (mongoc_stream_t  *base_stream,
                               mongoc_ssl_opt_t *opt,
                               int               client);

BSON_END_DECLS

#endif	/* MONGOC_ENABLE_OPENSSL */
#endif	/* MONGOC_ENABLE_SSL */

/*==============================================================*/
/* --- mongoc-stream-tls-openssl-private.h */

#ifdef MONGOC_ENABLE_OPENSSL

BSON_BEGIN_DECLS


/**
 * mongoc_stream_tls_openssl_t:
 *
 * Private storage for handling callbacks from mongoc_stream and BIO_*
 */
typedef struct
{
   BIO                *bio;
   SSL_CTX            *ctx;
} mongoc_stream_tls_openssl_t;


BSON_END_DECLS

#endif /* MONGOC_ENABLE_OPENSSL */

/*==============================================================*/
/* --- mongoc-stream-tls-private.h */

#ifdef MONGOC_ENABLE_SSL
#ifdef MONGOC_ENABLE_OPENSSL
#define MONGOC_TLS_TYPE 1
#else
/* FIXME: TLS through Secure Transport isn't implemented yet ! */
#define MONGOC_TLS_TYPE 1
#endif

BSON_BEGIN_DECLS

/* Available TLS Implementations */
typedef enum
{
   MONGOC_TLS_OPENSSL = 1
} mongoc_tls_types_t;

/**
 * mongoc_stream_tls_t:
 *
 * Overloaded mongoc_stream_t with additional TLS handshake and verification
 * callbacks.
 * 
 */
struct _mongoc_stream_tls_t
{
   mongoc_stream_t  parent;      /* The TLS stream wrapper */
   mongoc_stream_t *base_stream; /* The underlying actual stream */
   void            *ctx;         /* TLS lib specific configuration or wrappers */
   int32_t          timeout_msec;
   bool             weak_cert_validation;
   bool (*do_handshake) (mongoc_stream_t *stream, int32_t     timeout_msec);
   bool (*check_cert)   (mongoc_stream_t *stream, const char *host);
   bool (*should_retry) (mongoc_stream_t *stream);
   bool (*should_read)  (mongoc_stream_t *stream);
   bool (*should_write) (mongoc_stream_t *stream);
};


BSON_END_DECLS
#endif	/* MONGOC_ENABLE_SSL */

/*==============================================================*/
/* --- mongoc-stream-tls-secure-transport.h */

#ifdef MONGOC_ENABLE_SECURE_TRANSPORT

BSON_BEGIN_DECLS

mongoc_stream_t *
mongoc_stream_tls_secure_transport_new (mongoc_stream_t  *base_stream,
                                        mongoc_ssl_opt_t *opt,
                                        int               client);

BSON_END_DECLS

#endif /* MONGOC_ENABLE_SECURE_TRANSPORT */

/*==============================================================*/
/* --- mongoc-stream-tls-secure-transport-private.h */

#ifdef MONGOC_ENABLE_SECURE_TRANSPORT

BSON_BEGIN_DECLS

/**
 * mongoc_stream_tls_secure_transport_t:
 *
 * Private storage for Secure Transport Streams
 */
typedef struct
{
} mongoc_stream_tls_secure_transport_t;


BSON_END_DECLS

#endif /* MONGOC_ENABLE_SECURE_TRANSPORT */

/*==============================================================*/
/* --- mongoc-uri-private.h */

BSON_BEGIN_DECLS

void
mongoc_uri_lowercase_hostname    (      const char   *src,
                                        char         *buf /* OUT */,
                                        int           len);
void
mongoc_uri_append_host           (      mongoc_uri_t *uri,
                                  const char         *host,
                                        uint16_t      port);
bool
mongoc_uri_parse_host            (      mongoc_uri_t  *uri,
                                  const char          *str);

BSON_END_DECLS

/*==============================================================*/
/* --- mongoc-version-functions.h */

BSON_BEGIN_DECLS

int mongoc_get_major_version (void)
	BSON_GNUC_CONST;
int mongoc_get_minor_version (void)
	BSON_GNUC_CONST;
int mongoc_get_micro_version (void)
	BSON_GNUC_CONST;
const char *mongoc_get_version (void)
	BSON_GNUC_CONST;
bool mongoc_check_version (int required_major,
                           int required_minor,
                           int required_micro)
	BSON_GNUC_CONST;

BSON_END_DECLS

/*==============================================================*/
/* --- utlist.h */

/*
Copyright (c) 2007-2014, Troy D. Hanson   http://troydhanson.github.com/uthash/
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define UTLIST_VERSION 1.9.9

/*
 * This file contains macros to manipulate singly and doubly-linked lists.
 *
 * 1. LL_ macros:  singly-linked lists.
 * 2. DL_ macros:  doubly-linked lists.
 * 3. CDL_ macros: circular doubly-linked lists.
 *
 * To use singly-linked lists, your structure must have a "next" pointer.
 * To use doubly-linked lists, your structure must "prev" and "next" pointers.
 * Either way, the pointer to the head of the list must be initialized to NULL.
 *
 * ----------------.EXAMPLE -------------------------
 * struct item {
 *      int id;
 *      struct item *prev, *next;
 * }
 *
 * struct item *list = NULL:
 *
 * int main() {
 *      struct item *item;
 *      ... allocate and populate item ...
 *      DL_APPEND(list, item);
 * }
 * --------------------------------------------------
 *
 * For doubly-linked lists, the append and delete macros are O(1)
 * For singly-linked lists, append and delete are O(n) but prepend is O(1)
 * The sort macro is O(n log(n)) for all types of single/double/circular lists.
 */

/* These macros use decltype or the earlier __typeof GNU extension.
   As decltype is only available in newer compilers (VS2010 or gcc 4.3+
   when compiling c++ code), this code uses whatever method is needed
   or, for VS2008 where neither is available, uses casting workarounds. */
#ifdef _MSC_VER            /* MS compiler */
#if _MSC_VER >= 1600 && defined(__cplusplus)  /* VS2010 or newer in C++ mode */
#define LDECLTYPE(x) decltype(x)
#else                     /* VS2008 or older (or VS2010 in C mode) */
#define NO_DECLTYPE
#define LDECLTYPE(x) char*
#endif
#elif defined(__ICCARM__)
#define NO_DECLTYPE
#define LDECLTYPE(x) char*
#else                      /* GNU, Sun and other compilers */
#define LDECLTYPE(x) __typeof(x)
#endif

/* for VS2008 we use some workarounds to get around the lack of decltype,
 * namely, we always reassign our tmp variable to the list head if we need
 * to dereference its prev/next pointers, and save/restore the real head.*/
#ifdef NO_DECLTYPE
#define _SV(elt,list) _tmp = (char*)(list); {char **_alias = (char**)&(list); *_alias = (elt); }
#define _NEXT(elt,list,next) ((char*)((list)->next))
#define _NEXTASGN(elt,list,to,next) { char **_alias = (char**)&((list)->next); *_alias=(char*)(to); }
/* #define _PREV(elt,list,prev) ((char*)((list)->prev)) */
#define _PREVASGN(elt,list,to,prev) { char **_alias = (char**)&((list)->prev); *_alias=(char*)(to); }
#define _RS(list) { char **_alias = (char**)&(list); *_alias=_tmp; }
#define _CASTASGN(a,b) { char **_alias = (char**)&(a); *_alias=(char*)(b); }
#else
#define _SV(elt,list)
#define _NEXT(elt,list,next) ((elt)->next)
#define _NEXTASGN(elt,list,to,next) ((elt)->next)=(to)
/* #define _PREV(elt,list,prev) ((elt)->prev) */
#define _PREVASGN(elt,list,to,prev) ((elt)->prev)=(to)
#define _RS(list)
#define _CASTASGN(a,b) (a)=(b)
#endif

/******************************************************************************
 * The sort macro is an adaptation of Simon Tatham's O(n log(n)) mergesort    *
 * Unwieldy variable names used here to avoid shadowing passed-in variables.  *
 *****************************************************************************/
#define LL_SORT(list, cmp)                                                                     \
    LL_SORT2(list, cmp, next)

#define LL_SORT2(list, cmp, next)                                                              \
do {                                                                                           \
  LDECLTYPE(list) _ls_p;                                                                       \
  LDECLTYPE(list) _ls_q;                                                                       \
  LDECLTYPE(list) _ls_e;                                                                       \
  LDECLTYPE(list) _ls_tail;                                                                    \
  int _ls_insize, _ls_nmerges, _ls_psize, _ls_qsize, _ls_i, _ls_looping;                       \
  if (list) {                                                                                  \
    _ls_insize = 1;                                                                            \
    _ls_looping = 1;                                                                           \
    while (_ls_looping) {                                                                      \
      _CASTASGN(_ls_p,list);                                                                   \
      list = NULL;                                                                             \
      _ls_tail = NULL;                                                                         \
      _ls_nmerges = 0;                                                                         \
      while (_ls_p) {                                                                          \
        _ls_nmerges++;                                                                         \
        _ls_q = _ls_p;                                                                         \
        _ls_psize = 0;                                                                         \
        for (_ls_i = 0; _ls_i < _ls_insize; _ls_i++) {                                         \
          _ls_psize++;                                                                         \
          _SV(_ls_q,list); _ls_q = _NEXT(_ls_q,list,next); _RS(list);                          \
          if (!_ls_q) break;                                                                   \
        }                                                                                      \
        _ls_qsize = _ls_insize;                                                                \
        while (_ls_psize > 0 || (_ls_qsize > 0 && _ls_q)) {                                    \
          if (_ls_psize == 0) {                                                                \
            _ls_e = _ls_q; _SV(_ls_q,list); _ls_q =                                            \
              _NEXT(_ls_q,list,next); _RS(list); _ls_qsize--;                                  \
          } else if (_ls_qsize == 0 || !_ls_q) {                                               \
            _ls_e = _ls_p; _SV(_ls_p,list); _ls_p =                                            \
              _NEXT(_ls_p,list,next); _RS(list); _ls_psize--;                                  \
          } else if (cmp(_ls_p,_ls_q) <= 0) {                                                  \
            _ls_e = _ls_p; _SV(_ls_p,list); _ls_p =                                            \
              _NEXT(_ls_p,list,next); _RS(list); _ls_psize--;                                  \
          } else {                                                                             \
            _ls_e = _ls_q; _SV(_ls_q,list); _ls_q =                                            \
              _NEXT(_ls_q,list,next); _RS(list); _ls_qsize--;                                  \
          }                                                                                    \
          if (_ls_tail) {                                                                      \
            _SV(_ls_tail,list); _NEXTASGN(_ls_tail,list,_ls_e,next); _RS(list);                \
          } else {                                                                             \
            _CASTASGN(list,_ls_e);                                                             \
          }                                                                                    \
          _ls_tail = _ls_e;                                                                    \
        }                                                                                      \
        _ls_p = _ls_q;                                                                         \
      }                                                                                        \
      if (_ls_tail) {                                                                          \
        _SV(_ls_tail,list); _NEXTASGN(_ls_tail,list,NULL,next); _RS(list);                     \
      }                                                                                        \
      if (_ls_nmerges <= 1) {                                                                  \
        _ls_looping=0;                                                                         \
      }                                                                                        \
      _ls_insize *= 2;                                                                         \
    }                                                                                          \
  }                                                                                            \
} while (0)


#define DL_SORT(list, cmp)                                                                     \
    DL_SORT2(list, cmp, prev, next)

#define DL_SORT2(list, cmp, prev, next)                                                        \
do {                                                                                           \
  LDECLTYPE(list) _ls_p;                                                                       \
  LDECLTYPE(list) _ls_q;                                                                       \
  LDECLTYPE(list) _ls_e;                                                                       \
  LDECLTYPE(list) _ls_tail;                                                                    \
  int _ls_insize, _ls_nmerges, _ls_psize, _ls_qsize, _ls_i, _ls_looping;                       \
  if (list) {                                                                                  \
    _ls_insize = 1;                                                                            \
    _ls_looping = 1;                                                                           \
    while (_ls_looping) {                                                                      \
      _CASTASGN(_ls_p,list);                                                                   \
      list = NULL;                                                                             \
      _ls_tail = NULL;                                                                         \
      _ls_nmerges = 0;                                                                         \
      while (_ls_p) {                                                                          \
        _ls_nmerges++;                                                                         \
        _ls_q = _ls_p;                                                                         \
        _ls_psize = 0;                                                                         \
        for (_ls_i = 0; _ls_i < _ls_insize; _ls_i++) {                                         \
          _ls_psize++;                                                                         \
          _SV(_ls_q,list); _ls_q = _NEXT(_ls_q,list,next); _RS(list);                          \
          if (!_ls_q) break;                                                                   \
        }                                                                                      \
        _ls_qsize = _ls_insize;                                                                \
        while (_ls_psize > 0 || (_ls_qsize > 0 && _ls_q)) {                                    \
          if (_ls_psize == 0) {                                                                \
            _ls_e = _ls_q; _SV(_ls_q,list); _ls_q =                                            \
              _NEXT(_ls_q,list,next); _RS(list); _ls_qsize--;                                  \
          } else if (_ls_qsize == 0 || !_ls_q) {                                               \
            _ls_e = _ls_p; _SV(_ls_p,list); _ls_p =                                            \
              _NEXT(_ls_p,list,next); _RS(list); _ls_psize--;                                  \
          } else if (cmp(_ls_p,_ls_q) <= 0) {                                                  \
            _ls_e = _ls_p; _SV(_ls_p,list); _ls_p =                                            \
              _NEXT(_ls_p,list,next); _RS(list); _ls_psize--;                                  \
          } else {                                                                             \
            _ls_e = _ls_q; _SV(_ls_q,list); _ls_q =                                            \
              _NEXT(_ls_q,list,next); _RS(list); _ls_qsize--;                                  \
          }                                                                                    \
          if (_ls_tail) {                                                                      \
            _SV(_ls_tail,list); _NEXTASGN(_ls_tail,list,_ls_e,next); _RS(list);                \
          } else {                                                                             \
            _CASTASGN(list,_ls_e);                                                             \
          }                                                                                    \
          _SV(_ls_e,list); _PREVASGN(_ls_e,list,_ls_tail,prev); _RS(list);                     \
          _ls_tail = _ls_e;                                                                    \
        }                                                                                      \
        _ls_p = _ls_q;                                                                         \
      }                                                                                        \
      _CASTASGN(list->prev, _ls_tail);                                                         \
      _SV(_ls_tail,list); _NEXTASGN(_ls_tail,list,NULL,next); _RS(list);                       \
      if (_ls_nmerges <= 1) {                                                                  \
        _ls_looping=0;                                                                         \
      }                                                                                        \
      _ls_insize *= 2;                                                                         \
    }                                                                                          \
  }                                                                                            \
} while (0)

#define CDL_SORT(list, cmp)                                                                    \
    CDL_SORT2(list, cmp, prev, next)

#define CDL_SORT2(list, cmp, prev, next)                                                       \
do {                                                                                           \
  LDECLTYPE(list) _ls_p;                                                                       \
  LDECLTYPE(list) _ls_q;                                                                       \
  LDECLTYPE(list) _ls_e;                                                                       \
  LDECLTYPE(list) _ls_tail;                                                                    \
  LDECLTYPE(list) _ls_oldhead;                                                                 \
  LDECLTYPE(list) _tmp;                                                                        \
  int _ls_insize, _ls_nmerges, _ls_psize, _ls_qsize, _ls_i, _ls_looping;                       \
  if (list) {                                                                                  \
    _ls_insize = 1;                                                                            \
    _ls_looping = 1;                                                                           \
    while (_ls_looping) {                                                                      \
      _CASTASGN(_ls_p,list);                                                                   \
      _CASTASGN(_ls_oldhead,list);                                                             \
      list = NULL;                                                                             \
      _ls_tail = NULL;                                                                         \
      _ls_nmerges = 0;                                                                         \
      while (_ls_p) {                                                                          \
        _ls_nmerges++;                                                                         \
        _ls_q = _ls_p;                                                                         \
        _ls_psize = 0;                                                                         \
        for (_ls_i = 0; _ls_i < _ls_insize; _ls_i++) {                                         \
          _ls_psize++;                                                                         \
          _SV(_ls_q,list);                                                                     \
          if (_NEXT(_ls_q,list,next) == _ls_oldhead) {                                         \
            _ls_q = NULL;                                                                      \
          } else {                                                                             \
            _ls_q = _NEXT(_ls_q,list,next);                                                    \
          }                                                                                    \
          _RS(list);                                                                           \
          if (!_ls_q) break;                                                                   \
        }                                                                                      \
        _ls_qsize = _ls_insize;                                                                \
        while (_ls_psize > 0 || (_ls_qsize > 0 && _ls_q)) {                                    \
          if (_ls_psize == 0) {                                                                \
            _ls_e = _ls_q; _SV(_ls_q,list); _ls_q =                                            \
              _NEXT(_ls_q,list,next); _RS(list); _ls_qsize--;                                  \
            if (_ls_q == _ls_oldhead) { _ls_q = NULL; }                                        \
          } else if (_ls_qsize == 0 || !_ls_q) {                                               \
            _ls_e = _ls_p; _SV(_ls_p,list); _ls_p =                                            \
              _NEXT(_ls_p,list,next); _RS(list); _ls_psize--;                                  \
            if (_ls_p == _ls_oldhead) { _ls_p = NULL; }                                        \
          } else if (cmp(_ls_p,_ls_q) <= 0) {                                                  \
            _ls_e = _ls_p; _SV(_ls_p,list); _ls_p =                                            \
              _NEXT(_ls_p,list,next); _RS(list); _ls_psize--;                                  \
            if (_ls_p == _ls_oldhead) { _ls_p = NULL; }                                        \
          } else {                                                                             \
            _ls_e = _ls_q; _SV(_ls_q,list); _ls_q =                                            \
              _NEXT(_ls_q,list,next); _RS(list); _ls_qsize--;                                  \
            if (_ls_q == _ls_oldhead) { _ls_q = NULL; }                                        \
          }                                                                                    \
          if (_ls_tail) {                                                                      \
            _SV(_ls_tail,list); _NEXTASGN(_ls_tail,list,_ls_e,next); _RS(list);                \
          } else {                                                                             \
            _CASTASGN(list,_ls_e);                                                             \
          }                                                                                    \
          _SV(_ls_e,list); _PREVASGN(_ls_e,list,_ls_tail,prev); _RS(list);                     \
          _ls_tail = _ls_e;                                                                    \
        }                                                                                      \
        _ls_p = _ls_q;                                                                         \
      }                                                                                        \
      _CASTASGN(list->prev,_ls_tail);                                                          \
      _CASTASGN(_tmp,list);                                                                    \
      _SV(_ls_tail,list); _NEXTASGN(_ls_tail,list,_tmp,next); _RS(list);                       \
      if (_ls_nmerges <= 1) {                                                                  \
        _ls_looping=0;                                                                         \
      }                                                                                        \
      _ls_insize *= 2;                                                                         \
    }                                                                                          \
  }                                                                                            \
} while (0)

/******************************************************************************
 * singly linked list macros (non-circular)                                   *
 *****************************************************************************/
#define LL_PREPEND(head,add)                                                                   \
    LL_PREPEND2(head,add,next)

#define LL_PREPEND2(head,add,next)                                                             \
do {                                                                                           \
  (add)->next = head;                                                                          \
  head = add;                                                                                  \
} while (0)

#define LL_CONCAT(head1,head2)                                                                 \
    LL_CONCAT2(head1,head2,next)

#define LL_CONCAT2(head1,head2,next)                                                           \
do {                                                                                           \
  LDECLTYPE(head1) _tmp;                                                                       \
  if (head1) {                                                                                 \
    _tmp = head1;                                                                              \
    while (_tmp->next) { _tmp = _tmp->next; }                                                  \
    _tmp->next=(head2);                                                                        \
  } else {                                                                                     \
    (head1)=(head2);                                                                           \
  }                                                                                            \
} while (0)

#define LL_APPEND(head,add)                                                                    \
    LL_APPEND2(head,add,next)

#define LL_APPEND2(head,add,next)                                                              \
do {                                                                                           \
  LDECLTYPE(head) _tmp;                                                                        \
  (add)->next=NULL;                                                                            \
  if (head) {                                                                                  \
    _tmp = head;                                                                               \
    while (_tmp->next) { _tmp = _tmp->next; }                                                  \
    _tmp->next=(add);                                                                          \
  } else {                                                                                     \
    (head)=(add);                                                                              \
  }                                                                                            \
} while (0)

#define LL_DELETE(head,del)                                                                    \
    LL_DELETE2(head,del,next)

#define LL_DELETE2(head,del,next)                                                              \
do {                                                                                           \
  LDECLTYPE(head) _tmp;                                                                        \
  if ((head) == (del)) {                                                                       \
    (head)=(head)->next;                                                                       \
  } else {                                                                                     \
    _tmp = head;                                                                               \
    while (_tmp->next && (_tmp->next != (del))) {                                              \
      _tmp = _tmp->next;                                                                       \
    }                                                                                          \
    if (_tmp->next) {                                                                          \
      _tmp->next = ((del)->next);                                                              \
    }                                                                                          \
  }                                                                                            \
} while (0)

/* Here are VS2008 replacements for LL_APPEND and LL_DELETE */
#define LL_APPEND_VS2008(head,add)                                                             \
    LL_APPEND2_VS2008(head,add,next)

#define LL_APPEND2_VS2008(head,add,next)                                                       \
do {                                                                                           \
  if (head) {                                                                                  \
    (add)->next = head;     /* use add->next as a temp variable */                             \
    while ((add)->next->next) { (add)->next = (add)->next->next; }                             \
    (add)->next->next=(add);                                                                   \
  } else {                                                                                     \
    (head)=(add);                                                                              \
  }                                                                                            \
  (add)->next=NULL;                                                                            \
} while (0)

#define LL_DELETE_VS2008(head,del)                                                             \
    LL_DELETE2_VS2008(head,del,next)

#define LL_DELETE2_VS2008(head,del,next)                                                       \
do {                                                                                           \
  if ((head) == (del)) {                                                                       \
    (head)=(head)->next;                                                                       \
  } else {                                                                                     \
    char *_tmp = (char*)(head);                                                                \
    while ((head)->next && ((head)->next != (del))) {                                          \
      head = (head)->next;                                                                     \
    }                                                                                          \
    if ((head)->next) {                                                                        \
      (head)->next = ((del)->next);                                                            \
    }                                                                                          \
    {                                                                                          \
      char **_head_alias = (char**)&(head);                                                    \
      *_head_alias = _tmp;                                                                     \
    }                                                                                          \
  }                                                                                            \
} while (0)
#ifdef NO_DECLTYPE
#undef LL_APPEND
#define LL_APPEND LL_APPEND_VS2008
#undef LL_DELETE
#define LL_DELETE LL_DELETE_VS2008
#undef LL_DELETE2
#define LL_DELETE2 LL_DELETE2_VS2008
#undef LL_APPEND2
#define LL_APPEND2 LL_APPEND2_VS2008
#undef LL_CONCAT /* no LL_CONCAT_VS2008 */
#undef DL_CONCAT /* no DL_CONCAT_VS2008 */
#endif
/* end VS2008 replacements */

#define LL_COUNT(head,el,counter)                                                              \
    LL_COUNT2(head,el,counter,next)                                                            \

#define LL_COUNT2(head,el,counter,next)                                                        \
{                                                                                              \
    counter = 0;                                                                               \
    LL_FOREACH2(head,el,next){ ++counter; }                                                    \
}

#define LL_FOREACH(head,el)                                                                    \
    LL_FOREACH2(head,el,next)

#define LL_FOREACH2(head,el,next)                                                              \
    for(el=head;el;el=(el)->next)

#define LL_FOREACH_SAFE(head,el,tmp)                                                           \
    LL_FOREACH_SAFE2(head,el,tmp,next)

#define LL_FOREACH_SAFE2(head,el,tmp,next)                                                     \
  for((el)=(head);(el) && (tmp = (el)->next, 1); (el) = tmp)

#define LL_SEARCH_SCALAR(head,out,field,val)                                                   \
    LL_SEARCH_SCALAR2(head,out,field,val,next)

#define LL_SEARCH_SCALAR2(head,out,field,val,next)                                             \
do {                                                                                           \
    LL_FOREACH2(head,out,next) {                                                               \
      if ((out)->field == (val)) break;                                                        \
    }                                                                                          \
} while(0)

#define LL_SEARCH(head,out,elt,cmp)                                                            \
    LL_SEARCH2(head,out,elt,cmp,next)

#define LL_SEARCH2(head,out,elt,cmp,next)                                                      \
do {                                                                                           \
    LL_FOREACH2(head,out,next) {                                                               \
      if ((cmp(out,elt))==0) break;                                                            \
    }                                                                                          \
} while(0)

#define LL_REPLACE_ELEM(head, el, add)                                                         \
do {                                                                                           \
 LDECLTYPE(head) _tmp;                                                                         \
 assert(head != NULL);                                                                         \
 assert(el != NULL);                                                                           \
 assert(add != NULL);                                                                          \
 (add)->next = (el)->next;                                                                     \
 if ((head) == (el)) {                                                                         \
  (head) = (add);                                                                              \
 } else {                                                                                      \
  _tmp = head;                                                                                 \
  while (_tmp->next && (_tmp->next != (el))) {                                                 \
   _tmp = _tmp->next;                                                                          \
  }                                                                                            \
  if (_tmp->next) {                                                                            \
    _tmp->next = (add);                                                                        \
  }                                                                                            \
 }                                                                                             \
} while (0)

#define LL_PREPEND_ELEM(head, el, add)                                                         \
do {                                                                                           \
 LDECLTYPE(head) _tmp;                                                                         \
 assert(head != NULL);                                                                         \
 assert(el != NULL);                                                                           \
 assert(add != NULL);                                                                          \
 (add)->next = (el);                                                                           \
 if ((head) == (el)) {                                                                         \
  (head) = (add);                                                                              \
 } else {                                                                                      \
  _tmp = head;                                                                                 \
  while (_tmp->next && (_tmp->next != (el))) {                                                 \
   _tmp = _tmp->next;                                                                          \
  }                                                                                            \
  if (_tmp->next) {                                                                            \
    _tmp->next = (add);                                                                        \
  }                                                                                            \
 }                                                                                             \
} while (0)                                                                                    \


/******************************************************************************
 * doubly linked list macros (non-circular)                                   *
 *****************************************************************************/
#define DL_PREPEND(head,add)                                                                   \
    DL_PREPEND2(head,add,prev,next)

#define DL_PREPEND2(head,add,prev,next)                                                        \
do {                                                                                           \
 (add)->next = head;                                                                           \
 if (head) {                                                                                   \
   (add)->prev = (head)->prev;                                                                 \
   (head)->prev = (add);                                                                       \
 } else {                                                                                      \
   (add)->prev = (add);                                                                        \
 }                                                                                             \
 (head) = (add);                                                                               \
} while (0)

#define DL_APPEND(head,add)                                                                    \
    DL_APPEND2(head,add,prev,next)

#define DL_APPEND2(head,add,prev,next)                                                         \
do {                                                                                           \
  if (head) {                                                                                  \
      (add)->prev = (head)->prev;                                                              \
      (head)->prev->next = (add);                                                              \
      (head)->prev = (add);                                                                    \
      (add)->next = NULL;                                                                      \
  } else {                                                                                     \
      (head)=(add);                                                                            \
      (head)->prev = (head);                                                                   \
      (head)->next = NULL;                                                                     \
  }                                                                                            \
} while (0)

#define DL_CONCAT(head1,head2)                                                                 \
    DL_CONCAT2(head1,head2,prev,next)

#define DL_CONCAT2(head1,head2,prev,next)                                                      \
do {                                                                                           \
  LDECLTYPE(head1) _tmp;                                                                       \
  if (head2) {                                                                                 \
    if (head1) {                                                                               \
        _tmp = (head2)->prev;                                                                  \
        (head2)->prev = (head1)->prev;                                                         \
        (head1)->prev->next = (head2);                                                         \
        (head1)->prev = _tmp;                                                                  \
    } else {                                                                                   \
        (head1)=(head2);                                                                       \
    }                                                                                          \
  }                                                                                            \
} while (0)

#define DL_DELETE(head,del)                                                                    \
    DL_DELETE2(head,del,prev,next)

#define DL_DELETE2(head,del,prev,next)                                                         \
do {                                                                                           \
  assert((del)->prev != NULL);                                                                 \
  if ((del)->prev == (del)) {                                                                  \
      (head)=NULL;                                                                             \
  } else if ((del)==(head)) {                                                                  \
      (del)->next->prev = (del)->prev;                                                         \
      (head) = (del)->next;                                                                    \
  } else {                                                                                     \
      (del)->prev->next = (del)->next;                                                         \
      if ((del)->next) {                                                                       \
          (del)->next->prev = (del)->prev;                                                     \
      } else {                                                                                 \
          (head)->prev = (del)->prev;                                                          \
      }                                                                                        \
  }                                                                                            \
} while (0)

#define DL_COUNT(head,el,counter)                                                              \
    DL_COUNT2(head,el,counter,next)                                                            \

#define DL_COUNT2(head,el,counter,next)                                                        \
{                                                                                              \
    counter = 0;                                                                               \
    DL_FOREACH2(head,el,next){ ++counter; }                                                    \
}

#define DL_FOREACH(head,el)                                                                    \
    DL_FOREACH2(head,el,next)

#define DL_FOREACH2(head,el,next)                                                              \
    for(el=head;el;el=(el)->next)

/* this version is safe for deleting the elements during iteration */
#define DL_FOREACH_SAFE(head,el,tmp)                                                           \
    DL_FOREACH_SAFE2(head,el,tmp,next)

#define DL_FOREACH_SAFE2(head,el,tmp,next)                                                     \
  for((el)=(head);(el) && (tmp = (el)->next, 1); (el) = tmp)

/* these are identical to their singly-linked list counterparts */
#define DL_SEARCH_SCALAR LL_SEARCH_SCALAR
#define DL_SEARCH LL_SEARCH
#define DL_SEARCH_SCALAR2 LL_SEARCH_SCALAR2
#define DL_SEARCH2 LL_SEARCH2

#define DL_REPLACE_ELEM(head, el, add)                                                         \
do {                                                                                           \
 assert(head != NULL);                                                                         \
 assert(el != NULL);                                                                           \
 assert(add != NULL);                                                                          \
 if ((head) == (el)) {                                                                         \
  (head) = (add);                                                                              \
  (add)->next = (el)->next;                                                                    \
  if ((el)->next == NULL) {                                                                    \
   (add)->prev = (add);                                                                        \
  } else {                                                                                     \
   (add)->prev = (el)->prev;                                                                   \
   (add)->next->prev = (add);                                                                  \
  }                                                                                            \
 } else {                                                                                      \
  (add)->next = (el)->next;                                                                    \
  (add)->prev = (el)->prev;                                                                    \
  (add)->prev->next = (add);                                                                   \
  if ((el)->next == NULL) {                                                                    \
   (head)->prev = (add);                                                                       \
  } else {                                                                                     \
   (add)->next->prev = (add);                                                                  \
  }                                                                                            \
 }                                                                                             \
} while (0)

#define DL_PREPEND_ELEM(head, el, add)                                                         \
do {                                                                                           \
 assert(head != NULL);                                                                         \
 assert(el != NULL);                                                                           \
 assert(add != NULL);                                                                          \
 (add)->next = (el);                                                                           \
 (add)->prev = (el)->prev;                                                                     \
 (el)->prev = (add);                                                                           \
 if ((head) == (el)) {                                                                         \
  (head) = (add);                                                                              \
 } else {                                                                                      \
  (add)->prev->next = (add);                                                                   \
 }                                                                                             \
} while (0)                                                                                    \


/******************************************************************************
 * circular doubly linked list macros                                         *
 *****************************************************************************/
#define CDL_PREPEND(head,add)                                                                  \
    CDL_PREPEND2(head,add,prev,next)

#define CDL_PREPEND2(head,add,prev,next)                                                       \
do {                                                                                           \
 if (head) {                                                                                   \
   (add)->prev = (head)->prev;                                                                 \
   (add)->next = (head);                                                                       \
   (head)->prev = (add);                                                                       \
   (add)->prev->next = (add);                                                                  \
 } else {                                                                                      \
   (add)->prev = (add);                                                                        \
   (add)->next = (add);                                                                        \
 }                                                                                             \
(head)=(add);                                                                                  \
} while (0)

#define CDL_DELETE(head,del)                                                                   \
    CDL_DELETE2(head,del,prev,next)

#define CDL_DELETE2(head,del,prev,next)                                                        \
do {                                                                                           \
  if ( ((head)==(del)) && ((head)->next == (head))) {                                          \
      (head) = 0L;                                                                             \
  } else {                                                                                     \
     (del)->next->prev = (del)->prev;                                                          \
     (del)->prev->next = (del)->next;                                                          \
     if ((del) == (head)) (head)=(del)->next;                                                  \
  }                                                                                            \
} while (0)

#define CDL_COUNT(head,el,counter)                                                             \
    CDL_COUNT2(head,el,counter,next)                                                           \

#define CDL_COUNT2(head, el, counter,next)                                                     \
{                                                                                              \
    counter = 0;                                                                               \
    CDL_FOREACH2(head,el,next){ ++counter; }                                                   \
}

#define CDL_FOREACH(head,el)                                                                   \
    CDL_FOREACH2(head,el,next)

#define CDL_FOREACH2(head,el,next)                                                             \
    for(el=head;el;el=((el)->next==head ? 0L : (el)->next))

#define CDL_FOREACH_SAFE(head,el,tmp1,tmp2)                                                    \
    CDL_FOREACH_SAFE2(head,el,tmp1,tmp2,prev,next)

#define CDL_FOREACH_SAFE2(head,el,tmp1,tmp2,prev,next)                                         \
  for((el)=(head), ((tmp1)=(head)?((head)->prev):NULL);                                        \
      (el) && ((tmp2)=(el)->next, 1);                                                          \
      ((el) = (((el)==(tmp1)) ? 0L : (tmp2))))

#define CDL_SEARCH_SCALAR(head,out,field,val)                                                  \
    CDL_SEARCH_SCALAR2(head,out,field,val,next)

#define CDL_SEARCH_SCALAR2(head,out,field,val,next)                                            \
do {                                                                                           \
    CDL_FOREACH2(head,out,next) {                                                              \
      if ((out)->field == (val)) break;                                                        \
    }                                                                                          \
} while(0)

#define CDL_SEARCH(head,out,elt,cmp)                                                           \
    CDL_SEARCH2(head,out,elt,cmp,next)

#define CDL_SEARCH2(head,out,elt,cmp,next)                                                     \
do {                                                                                           \
    CDL_FOREACH2(head,out,next) {                                                              \
      if ((cmp(out,elt))==0) break;                                                            \
    }                                                                                          \
} while(0)

#define CDL_REPLACE_ELEM(head, el, add)                                                        \
do {                                                                                           \
 assert(head != NULL);                                                                         \
 assert(el != NULL);                                                                           \
 assert(add != NULL);                                                                          \
 if ((el)->next == (el)) {                                                                     \
  (add)->next = (add);                                                                         \
  (add)->prev = (add);                                                                         \
  (head) = (add);                                                                              \
 } else {                                                                                      \
  (add)->next = (el)->next;                                                                    \
  (add)->prev = (el)->prev;                                                                    \
  (add)->next->prev = (add);                                                                   \
  (add)->prev->next = (add);                                                                   \
  if ((head) == (el)) {                                                                        \
   (head) = (add);                                                                             \
  }                                                                                            \
 }                                                                                             \
} while (0)

#define CDL_PREPEND_ELEM(head, el, add)                                                        \
do {                                                                                           \
 assert(head != NULL);                                                                         \
 assert(el != NULL);                                                                           \
 assert(add != NULL);                                                                          \
 (add)->next = (el);                                                                           \
 (add)->prev = (el)->prev;                                                                     \
 (el)->prev = (add);                                                                           \
 (add)->prev->next = (add);                                                                    \
 if ((head) == (el)) {                                                                         \
  (head) = (add);                                                                              \
 }                                                                                             \
} while (0)                                                                                    \

/*==============================================================*/
/* --- mongoc.h */

#if 0
#define MONGOC_INSIDE
#include "mongoc-apm.h"
#include "mongoc-bulk-operation.h"
#include "mongoc-client.h"
#include "mongoc-client-pool.h"
#include "mongoc-collection.h"
#include "mongoc-config.h"
#include "mongoc-cursor.h"
#include "mongoc-database.h"
#include "mongoc-index.h"
#include "mongoc-error.h"
#include "mongoc-flags.h"
#include "mongoc-gridfs.h"
#include "mongoc-gridfs-file.h"
#include "mongoc-gridfs-file-list.h"
#include "mongoc-gridfs-file-page.h"
#include "mongoc-host-list.h"
#include "mongoc-init.h"
#include "mongoc-matcher.h"
#include "mongoc-opcode.h"
#include "mongoc-log.h"
#include "mongoc-socket.h"
#include "mongoc-stream.h"
#include "mongoc-stream-buffered.h"
#include "mongoc-stream-file.h"
#include "mongoc-stream-gridfs.h"
#include "mongoc-stream-socket.h"
#include "mongoc-uri.h"
#include "mongoc-write-concern.h"
#include "mongoc-version.h"
#include "mongoc-version-functions.h"
#ifdef MONGOC_ENABLE_OPENSSL
#include "mongoc-rand.h"
#include "mongoc-stream-tls.h"
#include "mongoc-ssl.h"
#endif
#undef MONGOC_INSIDE
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif /* H_MONGOC */

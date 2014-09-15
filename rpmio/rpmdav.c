/** \ingroup rpmio
 * \file rpmio/rpmdav.c
 */

#include "system.h"

#ifdef WITH_NEON

#include "ne_alloc.h"
#include "ne_auth.h"
#include "ne_basic.h"
#include "ne_dates.h"
#include "ne_locks.h"

#define	NEONBLOWSCHUNKS
#ifndef	NEONBLOWSCHUNKS
/* HACK: include ne_private.h to access sess->socket for now. */
#include "../neon/src/ne_private.h"
#endif

#include "ne_props.h"
#include "ne_request.h"
#include "ne_socket.h"
#include "ne_string.h"

#include "ne_utils.h"
#if !defined(HEADER_ERR_H)
/* cheats to avoid having to explicitly build against OpenSSL */
#ifdef __cplusplus
extern "C" {
#endif
extern void ERR_remove_state(int foo);
extern void ENGINE_cleanup(void);
extern void CONF_modules_unload(int foo);
extern void ERR_free_strings(void);
extern void EVP_cleanup(void);
extern void CRYPTO_cleanup_all_ex_data(void);
extern void CRYPTO_mem_leaks(void * ptr);
#ifdef __cplusplus
}
#endif
#endif

#endif /* WITH_NEON */

#include <rpmio_internal.h>

#include <rpmhash.h>
#include <rpmmacro.h>		/* XXX rpmExpand */
#include <ugid.h>

#define	_RPMDIR_INTERNAL
#include <rpmdir.h>
#define _RPMDAV_INTERNAL
#include <rpmdav.h>
#include <mire.h>

#include "debug.h"

#define	DAVDEBUG(_f, _list) \
    if (((_f) < 0 && _dav_debug < 0) || ((_f) > 0 && _dav_debug)) \
	fprintf _list

/* HACK: reasonable value needed (wget uses 900 as default). */
#if 0
#define READ_TIMEOUT_SECS	120	/* neon-0.28.5 default */
#define CONNECT_TIMEOUT_SECS	0	/* neon-0.28.5 default */
#else
#define READ_TIMEOUT_SECS	120
#define CONNECT_TIMEOUT_SECS	0	/* connect(2) EINPROGRESS if too low. */
#endif

static const char _rpmioHttpUserAgent[] = PACKAGE "/" PACKAGE_VERSION;

static int rpmioHttpPersist = 1;
int rpmioHttpReadTimeoutSecs = READ_TIMEOUT_SECS;
int rpmioHttpConnectTimeoutSecs = CONNECT_TIMEOUT_SECS;
#ifdef	NOTYET
int rpmioHttpRetries = 20;
int rpmioHttpRecurseMax = 5;
int rpmioHttpMaxRedirect = 20;
#endif

const char * rpmioHttpAccept;
const char * rpmioHttpUserAgent;

#ifdef WITH_NEON
/* =============================================================== */
static int davCheck(void * _req, const char * msg, int err)
{
    if (err || _dav_debug) {
	FILE * fp = stderr;
#ifdef	NOTYET
	char b[256];
	size_t nb = sizeof(b);
	const char * syserr = ne_strerror(errno, b, nb);
#endif

	if (msg) {
	    if (_req)
		fprintf(fp, "*** %s(%p):", msg, _req);
	    else
		fprintf(fp, "*** %s:", msg);
	}
	/* HACK FTPERR_NE_FOO == -NE_FOO error impedance match */
	fprintf(fp, " rc(%d) %s\n", err, ftpStrerror(-err));
    }
    return err;
}

int davDisconnect(void * _u)
{
    urlinfo u = (urlinfo) _u;
    int rc = 0;

    rc = (u->info.status == ne_status_sending || u->info.status == ne_status_recving);
    if (u != NULL) {
#ifdef	NOTYET
	if (u->ctrl->req != NULL) {
	    if (u->ctrl && u->ctrl->req) {
		ne_request_destroy((ne_request *)u->ctrl->req);
		u->ctrl->req = NULL;
	    }
	    if (u->data && u->data->req) {
		ne_request_destroy(u->data->req);
		u->data->req = NULL;
	    }
	}
#else
#ifdef	STILL_NOTYET	/* XXX closer but no cigar */
	if (u->sess != NULL)
	    ne_close_connection((ne_session *)u->sess);
#endif
#endif	/* NOTYET */
    }
DAVDEBUG(-1, (stderr, "<-- %s(%p) active %d\n", __FUNCTION__, u, rc));
    rc = 0;	/* XXX return active state? */
    return rc;
}

int davFree(urlinfo u)
{
    const char * url = NULL;
    if (u != NULL) {
	url = xstrdup(u->url);
	if (u->sess != NULL) {
	    ne_session_destroy((ne_session *)u->sess);
	    u->sess = NULL;
	}
	switch (urlType(u)) {
	default:
	    break;
	case URL_IS_HTTPS:
	case URL_IS_HTTP:
	case URL_IS_HKP:
	    u->capabilities = _free(u->capabilities);
	    if (u->lockstore != NULL)
		ne_lockstore_destroy((ne_lock_store *)u->lockstore);
	    u->lockstore = NULL;
	    u->info.status = 0;
	    ne_sock_exit();
	    break;
	}
    }
DAVDEBUG(-1, (stderr, "<-- %s(%p) url %s\n", __FUNCTION__, u, url));
    url = _free(url);
    return 0;
}

void davDestroy(void)
{
#if defined(NE_FEATURE_SSL)
    if (ne_has_support(NE_FEATURE_SSL)) {
#if defined(WITH_OPENSSL)	/* XXX FIXME: hard AutoFu to get right. */
/* XXX http://www.nabble.com/Memory-Leaks-in-SSL_Library_init()-t3431875.html */
	ENGINE_cleanup();
	CRYPTO_cleanup_all_ex_data();
	ERR_free_strings();
	ERR_remove_state(0);
	EVP_cleanup();
	CRYPTO_mem_leaks(NULL);
	CONF_modules_unload(1);
#endif	/* WITH_OPENSSL */
    }
#endif	/* NE_FEATURE_SSL */
DAVDEBUG(-1, (stderr, "<-- %s()\n", __FUNCTION__));
}

static void davNotify(void * userdata,
		ne_session_status status, const ne_session_status_info *info)
{
    char buf[64];
    urlinfo u = (urlinfo) userdata;
    ne_session * sess;

assert(u != NULL);
    sess = (ne_session *) u->sess;
assert(sess != NULL);
assert(u == ne_get_session_private(sess, "urlinfo"));

    u->info.hostname = NULL;
    u->info.address = NULL;
    u->info.progress = 0;
    u->info.total = 0;

#ifdef	REFERENCE
typedef enum {
    ne_status_lookup = 0, /* looking up hostname */
    ne_status_connecting, /* connecting to host */
    ne_status_connected, /* connected to host */
    ne_status_sending, /* sending a request body */
    ne_status_recving, /* receiving a response body */
    ne_status_disconnected /* disconnected from host */
} ne_session_status;
#endif
    switch (status) {
    default:
	break;
    case ne_status_lookup:	/* looking up hostname */
	u->info.hostname = info->lu.hostname;
	break;
    case ne_status_connecting:	/* connecting to host */
	u->info.hostname = info->ci.hostname;
	(void) ne_iaddr_print(info->ci.address, buf, sizeof(buf));
	buf[sizeof(buf)-1] = '\0';
	u->info.address = buf;		/* XXX strdup? */
    	break;
    case ne_status_connected:	/* connected to host */
    case ne_status_disconnected:/* disconnected from host */
	u->info.hostname = info->cd.hostname;
	break;
    case ne_status_sending:	/* sending a request body */
    case ne_status_recving:	/* receiving a response body */
	u->info.progress = info->sr.progress;
	u->info.total = info->sr.total;
	break;
    }

    if (u->notify != NULL)
	(void) (*u->notify) (u, status);

    u->info.status = status;
    u->info.hostname = NULL;
    u->info.address = NULL;
    u->info.progress = 0;
    u->info.total = 0;
}

static void davCreateRequest(ne_request * req, void * userdata,
		const char * method, const char * uri)
{
    urlinfo u = (urlinfo) userdata;
    ne_session * sess;
    void * myprivate = NULL;
    const char * id = "urlinfo";

assert(u != NULL);
assert(u->sess != NULL);
assert(req != NULL);
    sess = ne_get_session(req);
assert(sess == u->sess);
assert(u == ne_get_session_private(sess, "urlinfo"));

assert(sess != NULL);
    myprivate = ne_get_session_private(sess, id);
assert(u == myprivate);

DAVDEBUG(-1, (stderr, "<-- %s(%p,%p,%s,%s) %s:%p\n", __FUNCTION__, req, userdata, method, uri, id, myprivate));
}

static void davPreSend(ne_request * req, void * userdata, ne_buffer * buf)
{
    urlinfo u = (urlinfo) userdata;
    ne_session * sess;
    const char * id = "fd";
    FD_t fd = NULL;

assert(u != NULL);
assert(u->sess != NULL);
assert(req != NULL);
    sess = ne_get_session(req);
assert(sess == u->sess);
assert(u == ne_get_session_private(sess, "urlinfo"));

    fd = (FD_t) ne_get_request_private(req, id);

DAVDEBUG(1, (stderr, "-> %s\n", buf->data));
DAVDEBUG(-1, (stderr, "<-- %s(%p,%p,%p) sess %p %s %p\n", __FUNCTION__, req, userdata, buf, sess, id, fd));

}

static void davPostHeaders(ne_request * req, void * userdata, const ne_status * status)
{
    urlinfo u = (urlinfo) userdata;
    ne_session * sess;
    const char * id = "fd";
    FD_t fd = NULL;

assert(u != NULL);
assert(u->sess != NULL);
assert(req != NULL);
    sess = ne_get_session(req);
assert(sess == u->sess);
assert(u == ne_get_session_private(sess, "urlinfo"));

    fd = (FD_t) ne_get_request_private(req, id);

DAVDEBUG(-1, (stderr, "<-- %s(%p,%p,%p) sess %p %s %p %s\n", __FUNCTION__, req, userdata, status, sess, id, fd, ne_get_error(sess)));
}

static int davPostSend(ne_request * req, void * userdata, const ne_status * status)
{
    urlinfo u = (urlinfo) userdata;
    ne_session * sess;
    const char * id = "fd";
    FD_t fd = NULL;
    int rc = NE_OK;

assert(u != NULL);
assert(u->sess != NULL);
assert(req != NULL);
    sess = ne_get_session(req);
assert(sess == u->sess);
assert(u == ne_get_session_private(sess, "urlinfo"));

    if (status->klass == 3) {
	const char * htag = "Location";
	const char * value = ne_get_response_header(req, htag); 
	if (value) {
	    u->location = _free(u->location);
	    u->location = xstrdup(value);
	    rc = NE_REDIRECT;
	}
DAVDEBUG(-1, (stderr, "\t%d Location: %s\n", status->code, value));
    }

    fd = (FD_t) ne_get_request_private(req, id);

DAVDEBUG(-1, (stderr, "<-- %s(%p,%p,%p) rc %d sess %p %s %p %s\n", __FUNCTION__, req, userdata, status, rc, sess, id, fd, ne_get_error(sess)));
    return rc;
}

static void davDestroyRequest(ne_request * req, void * userdata)
{
    urlinfo u = (urlinfo) userdata;
    ne_session * sess;
    const char * id = "fd";
    FD_t fd = NULL;

assert(u != NULL);
assert(u->sess != NULL);
assert(req != NULL);
    sess = ne_get_session(req);
assert(sess == u->sess);
assert(u == ne_get_session_private(sess, "urlinfo"));

    fd = (FD_t) ne_get_request_private(req, id);

DAVDEBUG(-1, (stderr, "<-- %s(%p,%p) sess %p %s %p\n", __FUNCTION__, req, userdata, sess, id, fd));
}

static void davCloseConn(void * userdata)
{
    urlinfo u = (urlinfo) userdata;
    ne_session * sess;
    void * myprivate = NULL;
    const char * id = "urlinfo";
    FD_t fd = NULL;

assert(u != NULL);
assert(u->sess != NULL);
    sess = (ne_session *) u->sess;
assert(u == ne_get_session_private(sess, id));
    
    myprivate = ne_get_session_private(sess, id);
assert(u == myprivate);

DAVDEBUG(-1, (stderr, "<-- %s(%p) sess %p %s %p\n", __FUNCTION__, userdata, sess, id, myprivate));
}

static void davDestroySession(void * userdata)
{
    urlinfo u = (urlinfo) userdata;
    ne_session * sess;
    void * myprivate = NULL;
    const char * id = "urlinfo";

assert(u != NULL);
assert(u->sess != NULL);
    sess = (ne_session *) u->sess;
assert(u == ne_get_session_private(sess, id));

    myprivate = ne_get_session_private(sess, id);
assert(u == myprivate);

DAVDEBUG(-1, (stderr, "<-- %s(%p) sess %p %s %p\n", __FUNCTION__, userdata, sess, id, myprivate));
}

#ifdef	REFERENCE
/* Certificate verification failures. */

/* NE_SSL_NOTYETVALID: the certificate is not yet valid. */
#define NE_SSL_NOTYETVALID (0x01)

/* NE_SSL_EXPIRED: the certificate has expired. */
#define NE_SSL_EXPIRED (0x02)

/* NE_SSL_IDMISMATCH: the hostname for which the certificate was
 * issued does not match the hostname of the server; this could mean
 * that the connection is being intercepted. */
#define NE_SSL_IDMISMATCH (0x04)

/* NE_SSL_UNTRUSTED: the certificate authority which signed the server
 * certificate is not trusted: there is no indicatation the server is
 * who they claim to be: */
#define NE_SSL_UNTRUSTED (0x08)

/* NE_SSL_BADCHAIN: the certificate chain contained a certificate
 * other than the server cert which failed verification for a reason
 * other than lack of trust; for example, due to a CA cert being
 * outside its validity period. */
#define NE_SSL_BADCHAIN (0x10)

/* N.B.: 0x20 is reserved. */

/* NE_SSL_REVOKED: the server certificate has been revoked by the
 * issuing authority. */
#define NE_SSL_REVOKED (0x40)

/* For purposes of forwards-compatibility, the bitmask of all
 * currently exposed failure bits is given as NE_SSL_FAILMASK.  If the
 * expression (failures & ~NE_SSL_FAILMASK) is non-zero a failure type
 * is present which the application does not recognize but must treat
 * as a verification failure nonetheless. */
#define NE_SSL_FAILMASK (0x5f)
#endif	/* REFERENCE */

static int
davVerifyCert(void *userdata, int failures, const ne_ssl_certificate *cert)
{
    const char *hostname = (const char *) userdata;

DAVDEBUG(-1, (stderr, "--> %s(%p,%d,%p) %s\n", __FUNCTION__, userdata, failures, cert, hostname));

    return 0;	/* HACK: trust all server certificates. */
}

static int davConnect(urlinfo u)
{
    const char * path = NULL;
    int rc;

    /* HACK: hkp:// has no steenkin' options */
    switch (urlType(u)) {
    case URL_IS_HKP:
    default:
	return 0;
	break;
    case URL_IS_HTTP:
    case URL_IS_HTTPS:
	break;
    }

    /* HACK: where should server capabilities be read? */
    (void) urlPath(u->url, &path);
    if (path == NULL || *path == '\0')
	path = "/";

#ifdef NOTYET	/* XXX too many new directories while recursing. */
    /* Repeat OPTIONS for new directories. */
    if (path != NULL && path[strlen(path)-1] == '/')
	u->caps &= ~RPMURL_SERVER_OPTIONSDONE;
#endif
    /* Have options been run? */
    if (u->caps & RPMURL_SERVER_OPTIONSDONE)
	return 0;

    u->caps &= ~(RPMURL_SERVER_DAV_CLASS1 |
		  RPMURL_SERVER_DAV_CLASS2 |
		  RPMURL_SERVER_MODDAV_EXEC);

    /* HACK: perhaps capture Allow: tag, look for PUT permitted. */
    /* XXX [hdr] Allow: GET,HEAD,POST,OPTIONS,TRACE */
#ifdef	DYING	/* XXX fall back for downrev servers? */
    /* XXX filter NE_REDIRECT spewage. */
    rc = davCheck(u->sess, "ne_options",
		ne_options((ne_session *)u->sess, path, (ne_server_capabilities *)u->capabilities));
#else
    rc = davCheck(u->sess, "ne_options2",
		ne_options2((ne_session *)u->sess, path, &u->caps));
#endif
    switch (rc) {
    case NE_OK:
	u->caps |= RPMURL_SERVER_OPTIONSDONE;
#ifdef	DYING	/* XXX fall back for downrev servers? */
    {	ne_server_capabilities *cap = (ne_server_capabilities *)u->capabilities;
	if (cap->dav_class1)
	    u->caps |= RPMURL_SERVER_DAV_CLASS1;
	else
	    u->caps &= ~RPMURL_SERVER_DAV_CLASS1;
	if (cap->dav_class2)
	    u->caps |= RPMURL_SERVER_DAV_CLASS2;
	else
	    u->caps &= ~RPMURL_SERVER_DAV_CLASS2;
	if (cap->dav_executable)
	    u->caps |= RPMURL_SERVER_MODDAV_EXEC;
	else
	    u->caps &= ~RPMURL_SERVER_MODDAV_EXEC;
    }
#endif
	break;
    case NE_ERROR:
	/* HACK: "501 Not Implemented" if OPTIONS not permitted. */
	if (!strncmp("501 ", ne_get_error((ne_session *)u->sess), sizeof("501 ")-1)) {
	    u->caps |= RPMURL_SERVER_OPTIONSDONE;
	    rc = NE_OK;
	    break;
	}
	/* HACK: "301 Moved Permanently" on empty subdir. */
	if (!strncmp("301 ", ne_get_error((ne_session *)u->sess), sizeof("301 ")-1))
	    break;
#ifdef	HACK	/* XXX need davHEAD changes here? */
	/* HACK: "302 Found" if URI is missing pesky trailing '/'. */
	if (!strncmp("302 ", ne_get_error((ne_session *)u->sess), sizeof("302 ")-1)) {
	    char * t;
	    if ((t = strchr(u->url, '\0')) != NULL)
		*t = '/';
	    break;
	}
#endif
	errno = EIO;		/* HACK: more precise errno. */
	goto bottom;
    case NE_REDIRECT:
assert(u->location);
return (rc);
	break;
    case NE_LOOKUP:
	errno = ENOENT;		/* HACK: errno same as non-existent path. */
	goto bottom;
    case NE_CONNECT:		/* HACK: errno set already? */
    case NE_AUTH:
    case NE_PROXYAUTH:
    case NE_TIMEOUT:
    case NE_FAILED:
    case NE_RETRY:
    default:
bottom:
DAVDEBUG(-1, (stderr, "*** Connect to %s:%d failed(%d):\n\t%s\n", u->host, u->port, rc, ne_get_error((ne_session *)u->sess)));
	break;
    }

    /* HACK: sensitive to error returns? */
    u->httpVersion = (ne_version_pre_http11((ne_session *)u->sess) ? 0 : 1);

    return rc;
}

static int davInit(const char * url, urlinfo * uret)
{
    urlinfo u = NULL;
    int rc = 0;

retry:
    /* Chain through redirects looking for a session. */
    do {
	if (urlSplit(url, &u))
	    return -1;	/* XXX error returns needed. */
	if (u->location) {
if (_dav_debug < 0)
fprintf(stderr, "\tREDIRECT %s -> %s\n", url, u->location);
	    url = u->location;
	    continue;
	}
	break;
    } while (1);

    if (u->url != NULL && u->sess == NULL)
    switch (u->ut) {
    default:
	assert(u->ut != u->ut);
	break;
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    case URL_IS_HKP:
      {	ne_session * sess = NULL;
	ne_lock_store * lockstore = NULL;
	ne_server_capabilities * capabilities = NULL;;

	/* HACK: oneshots should be done Somewhere Else Instead. */
	rc = ((_dav_debug < 0) ? NE_DBG_HTTP : 0);
	ne_debug_init(stderr, rc);		/* XXX oneshot? */
	rc = ne_sock_init();	/* XXX refcounted. oneshot? */

	lockstore = ne_lockstore_create();	/* XXX oneshot? */

	u->capabilities = capabilities = (ne_server_capabilities *) xcalloc(1, sizeof(*capabilities));
	sess = ne_session_create(u->scheme, u->host, u->port);

	ne_lockstore_register(lockstore, sess);

	if (u->proxyh != NULL)
	    ne_session_proxy(sess, u->proxyh, u->proxyp);

#if 0
	{   const ne_inet_addr ** addrs;
	    unsigned int n;
	    ne_set_addrlist(sess, addrs, n);
	}
#endif

	ne_set_notifier(sess, davNotify, u);

	ne_set_session_flag(sess, NE_SESSFLAG_PERSIST, rpmioHttpPersist);
	ne_set_connect_timeout(sess, rpmioHttpConnectTimeoutSecs);
	ne_set_read_timeout(sess, rpmioHttpReadTimeoutSecs);
	ne_set_useragent(sess,
	    (rpmioHttpUserAgent ? rpmioHttpUserAgent : _rpmioHttpUserAgent));

	/* XXX check that neon is ssl enabled. */
	if (!strcasecmp(u->scheme, "https")) {
	    ne_ssl_set_verify(sess, davVerifyCert, (char *)u->host);
#ifdef	NOTYET
	    ne_ssl_set_clicert(sess, const ne_ssl_client_cert *clicert);
	    ne_ssl_set_trust_cert(sess, const ne_ssl_certifcate *cert);
	    ne_ssl_set_trust_default_ca(sess);
	    ne_ssl_provide_clicert(sess,
                            ne_ssl_provide_fn fn, void *userdata);
#endif
	}

	ne_set_session_private(sess, "urlinfo", u);

	ne_hook_destroy_session(sess, davDestroySession, u);

	ne_hook_create_request(sess, davCreateRequest, u);
	ne_hook_pre_send(sess, davPreSend, u);
	ne_hook_post_headers(sess, davPostHeaders, u);
	ne_hook_post_send(sess, davPostSend, u);
	ne_hook_destroy_request(sess, davDestroyRequest, u);
	ne_hook_close_conn(sess, davCloseConn, u);

#ifdef	DYING	/* XXX handled by davPostSend */
	ne_redirect_register(sess);
#endif

	u->sess = sess;
	u->lockstore = lockstore;
	u->capabilities = capabilities;

	/* HACK: where should server capabilities be read? */
	rc = davConnect(u);
	switch (rc) {
	case NE_OK:
	    break;
	case NE_REDIRECT:
	    if (u->location) {
		urlinfo v = NULL;
		rc = davInit(u->location, &v);	/* XXX recurse or goto retry? */
		if (rc == 0) {
		    /* XXX xx = davFree(u); u->lookup is critical?*/
		    u = urlLink(v, "davInit REDIRECT");
		}
		v = urlFree(v, "urlSplit (davInit REDIRECT)");
	    }
	    break;
	default:
	    goto exit;
	    break;
	}
      }	break;
    }

exit:
DAVDEBUG(-1, (stderr, "<-- %s(%s) rc %d u->url %s\n", __FUNCTION__, url, rc, u->url));
    if (uret != NULL)
	*uret = urlLink(u, "davInit");
    u = urlFree(u, "urlSplit (davInit)");

    return rc;
}

/* =============================================================== */
enum fetch_rtype_e {
    resr_normal = 0,
    resr_collection,
    resr_reference,
    resr_error
};

struct fetch_resource_s {
    struct fetch_resource_s *next;
    char *uri;
    char *displayname;
    enum fetch_rtype_e type;
    size_t size;
    time_t modtime;
    int is_executable;
    int is_vcr;    /* Is version resource. 0: no vcr, 1 checkin 2 checkout */
    char *error_reason; /* error string returned for this resource */
    int error_status; /* error status returned for this resource */
};

#ifdef __cplusplus
GENfree(struct fetch_resource_s *)
#endif	/* __cplusplus */

static void *fetch_destroy_item(struct fetch_resource_s *res)
{
    ne_free(res->uri);
    ne_free(res->error_reason);
    res = _free(res);
    return NULL;
}

#ifdef	NOTUSED
static void *fetch_destroy_list(struct fetch_resource_s *res)
{
    struct fetch_resource_s *next;
    for (; res != NULL; res = next) {
	next = res->next;
	res = fetch_destroy_item(res);
    }
    return NULL;
}
#endif

static void *fetch_create_item(void *userdata, const ne_uri *uri)
{
    struct fetch_resource_s * res = (struct fetch_resource_s *) ne_calloc(sizeof(*res));
    return res;
}

/* =============================================================== */

static const ne_propname fetch_props[] = {
    { "DAV:", "getcontentlength" },
    { "DAV:", "getlastmodified" },
    { "http://apache.org/dav/props/", "executable" },
    { "DAV:", "resourcetype" },
    { "DAV:", "checked-in" },
    { "DAV:", "checked-out" },
    { NULL, NULL }
};

#define ELM_resourcetype (NE_PROPS_STATE_TOP + 1)
#define ELM_collection (NE_PROPS_STATE_TOP + 2)

static const struct ne_xml_idmap fetch_idmap[] = {
    { "DAV:", "resourcetype", ELM_resourcetype },
    { "DAV:", "collection", ELM_collection }
};

static int fetch_startelm(void *userdata, int parent,
		const char *nspace, const char *name,
		const char **atts)
{
    ne_propfind_handler *pfh = (ne_propfind_handler *) userdata;
    struct fetch_resource_s *r = (struct fetch_resource_s *)
	ne_propfind_current_private(pfh);
    int state = ne_xml_mapid(fetch_idmap, NE_XML_MAPLEN(fetch_idmap),
                             nspace, name);

    if (r == NULL ||
        !((parent == NE_207_STATE_PROP && state == ELM_resourcetype) ||
          (parent == ELM_resourcetype && state == ELM_collection)))
        return NE_XML_DECLINE;

    if (state == ELM_collection) {
	r->type = resr_collection;
    }

    return state;
}

static int fetch_compare(const struct fetch_resource_s *r1,
			    const struct fetch_resource_s *r2)
{
    /* Sort errors first, then collections, then alphabetically */
    if (r1->type == resr_error) {
	return -1;
    } else if (r2->type == resr_error) {
	return 1;
    } else if (r1->type == resr_collection) {
	if (r2->type != resr_collection) {
	    return -1;
	} else {
	    return strcmp(r1->uri, r2->uri);
	}
    } else {
	if (r2->type != resr_collection) {
	    return strcmp(r1->uri, r2->uri);
	} else {
	    return 1;
	}
    }
}

static void fetch_results(void *userdata, const ne_uri *uarg,
		    const ne_prop_result_set *set)
{
    rpmavx avx = (rpmavx) userdata;
    struct fetch_resource_s *current, *previous, *newres;
    const char *clength, *modtime, *isexec;
    const char *checkin, *checkout;
    const ne_status *status = NULL;
    const char * path = NULL;

    const ne_uri * uri = uarg;
    (void) urlPath(uri->path, &path);
    if (path == NULL)
	return;

    newres = (struct fetch_resource_s *) ne_propset_private(set);

DAVDEBUG(-1, (stderr, "==> %s in uri %s\n", path, avx->uri));

    if (ne_path_compare(avx->uri, path) == 0) {
	/* This is the target URI */
DAVDEBUG(-1, (stderr, "==> %s skipping target resource.\n", path));
	/* Free the private structure. */
	free(newres);
	return;
    }

    newres->uri = ne_strdup(path);

    clength = ne_propset_value(set, &fetch_props[0]);
    modtime = ne_propset_value(set, &fetch_props[1]);
    isexec = ne_propset_value(set, &fetch_props[2]);
    checkin = ne_propset_value(set, &fetch_props[4]);
    checkout = ne_propset_value(set, &fetch_props[5]);

    if (clength == NULL)
	status = ne_propset_status(set, &fetch_props[0]);
    if (modtime == NULL)
	status = ne_propset_status(set, &fetch_props[1]);

    if (newres->type == resr_normal && status != NULL) {
	/* It's an error! */
	newres->error_status = status->code;

	/* Special hack for Apache 1.3/mod_dav */
	if (strcmp(status->reason_phrase, "status text goes here") == 0) {
	    const char *desc;
	    if (status->code == 401) {
		desc = _("Authorization Required");
	    } else if (status->klass == 3) {
		desc = _("Redirect");
	    } else if (status->klass == 5) {
		desc = _("Server Error");
	    } else {
		desc = _("Unknown Error");
	    }
	    newres->error_reason = ne_strdup(desc);
	} else {
	    newres->error_reason = ne_strdup(status->reason_phrase);
	}
	newres->type = resr_error;
    }

    if (isexec && strcasecmp(isexec, "T") == 0) {
	newres->is_executable = 1;
    } else {
	newres->is_executable = 0;
    }

    if (modtime)
	newres->modtime = ne_httpdate_parse(modtime);

    if (clength)
	newres->size = atoi(clength);

    /* is vcr */
    if (checkin) {
	newres->is_vcr = 1;
    } else if (checkout) {
	newres->is_vcr = 2;
    } else {
	newres->is_vcr = 0;
    }

    current = *(struct fetch_resource_s **)avx->resrock;
    for (current = (struct fetch_resource_s *) *avx->resrock, previous = NULL;
	current != NULL;
	previous = current, current = current->next)
    {
	if (fetch_compare(current, newres) >= 0) {
	    break;
	}
    }
    if (previous) {
	previous->next = newres;
    } else {
        *(struct fetch_resource_s **)avx->resrock = newres;
    }
    newres->next = current;
}

static int davFetch(const urlinfo u, rpmavx avx)
{
    const char * path = NULL;
    int depth = 1;					/* XXX passed arg? */
    struct fetch_resource_s * resitem = NULL;
    ne_propfind_handler *pfh;
    struct fetch_resource_s *current, *next;
    struct stat * st = avx->st;
    mode_t st_mode;
    int rc = 0;
    int xx;

    (void) urlPath(u->url, &path);
    pfh = ne_propfind_create((ne_session *)u->sess, avx->uri, depth);

    /* HACK: need to set RPMURL_SERVER_HASRANGE in u->caps here. */

    avx->resrock = (void **) &resitem;

    ne_xml_push_handler(ne_propfind_get_parser(pfh),
                        fetch_startelm, NULL, NULL, pfh);

    ne_propfind_set_private(pfh, fetch_create_item, NULL, NULL);

    rc = davCheck(pfh, "ne_propfind_named",
		ne_propfind_named(pfh, fetch_props, fetch_results, avx));

    ne_propfind_destroy(pfh);

    for (current = resitem; current != NULL; current = next) {
	const char *s, *se;
	char * val;

	next = current->next;

	/* Collections have trailing '/' that needs trim. */
	/* The top level collection is returned as well. */
	se = current->uri + strlen(current->uri);
	if (se[-1] == '/') {
	    if (strlen(current->uri) <= strlen(path)) {
		st->st_mode = (S_IFDIR|0755);
		st->st_nlink += 2;
		/* XXX TODO: current-size is 0 here. */
		st->st_size = current->size;
		st->st_blocks = (st->st_size + 511)/512;
		st->st_mtime = current->modtime;
		st->st_atime = st->st_ctime = st->st_mtime;        /* HACK */
		current = (struct fetch_resource_s *)
			fetch_destroy_item(current);
		continue;
	    }
	    se--;
	}
	s = se;
	while (s > current->uri && s[-1] != '/')
	    s--;

	{   char * ual = ne_strndup(s, (se - s));
	    val = ne_path_unescape(ual);
	    ne_free(ual);
	}

	switch (current->type) {
	case resr_normal:
	    st_mode = S_IFREG | 0644;
	    break;
	case resr_collection:
	    st_mode = S_IFDIR | 0755;
	    if (S_ISDIR(st->st_mode))
		st->st_nlink++;
	    break;
	case resr_reference:
	case resr_error:
	default:
	    st_mode = 0;
	    break;
	}

	xx = rpmavxAdd(avx, val, st_mode, current->size, current->modtime);
	ne_free(val);

	if (current == resitem && next == NULL) {
	    st->st_mode = st_mode;
	    st->st_nlink = S_ISDIR(st_mode) ? 2 : 1;
	    st->st_size = current->size;
	    st->st_blocks = (st->st_size + 511)/512;
	    st->st_mtime = current->modtime;
	    st->st_atime = st->st_ctime = st->st_mtime;        /* HACK */
	}

	current = (struct fetch_resource_s *)
		fetch_destroy_item(current);
    }
    avx->resrock = NULL;	/* HACK: avoid leaving stack reference. */
    /* HACK realloc to truncate modes/sizes/mtimes */

    return rc;
}

/* =============================================================== */
static void davAcceptRanges(void * userdata, const char * value)
{
    urlinfo u = (urlinfo) userdata;

    if (!(u != NULL && value != NULL)) return;
DAVDEBUG(-1, (stderr, "***  u %p Accept-Ranges: %s\n", u, value));
    if (!strcmp(value, "bytes"))
	u->caps |= RPMURL_SERVER_HASRANGE;
    if (!strcmp(value, "none"))
	u->caps &= ~RPMURL_SERVER_HASRANGE;
}

static void davContentLength(void * userdata, const char * value)
{
    FD_t ctrl = (FD_t) userdata;

    if (!(ctrl != NULL && value != NULL)) return;
DAVDEBUG(-1, (stderr, "*** fd %p Content-Length: %s\n", ctrl, value));
   ctrl->contentLength = strtoll(value, NULL, 10);
}

static void davContentType(void * userdata, const char * value)
{
    FD_t ctrl = (FD_t) userdata;

    if (!(ctrl != NULL && value != NULL)) return;
DAVDEBUG(-1, (stderr, "*** fd %p Content-Type: %s\n", ctrl, value));
   ctrl->contentType = _free(ctrl->contentType);
   ctrl->contentType = xstrdup(value);
}

static void davContentDisposition(void * userdata, const char * value)
{
    FD_t ctrl = (FD_t) userdata;

    if (!(ctrl != NULL && value != NULL)) return;
DAVDEBUG(-1, (stderr, "*** fd %p Content-Disposition: %s\n", ctrl, value));
   ctrl->contentDisposition = _free(ctrl->contentDisposition);
   ctrl->contentDisposition = xstrdup(value);
}

static void davLastModified(void * userdata, const char * value)
{
    FD_t ctrl = (FD_t) userdata;

    if (!(ctrl != NULL && value != NULL)) return;
DAVDEBUG(-1, (stderr, "*** fd %p Last-Modified: %s\n", ctrl, value));
   ctrl->lastModified = ne_httpdate_parse(value);
}

static void davConnection(void * userdata, const char * value)
{
    FD_t ctrl = (FD_t) userdata;

    if (!(ctrl != NULL && value != NULL)) return;
DAVDEBUG(-1, (stderr, "*** fd %p Connection: %s\n", ctrl, value));
    if (!strcasecmp(value, "close"))
	ctrl->persist = 0;
    else if (!strcasecmp(value, "Keep-Alive"))
	ctrl->persist = 1;
}

static void davDate(void * userdata, const char * value)
{
    FD_t ctrl = (FD_t) userdata;
    urlinfo u;

    if (!(ctrl != NULL && value != NULL)) return;
DAVDEBUG(-1, (stderr, "*** fd %p Date: %s\n", ctrl, value));

    u = (urlinfo) ctrl->u;
    URLSANE(u);

    if (value && u->date == NULL) {		/* XXX test for NULL? */
	u->date = _free(u->date);
	u->date = xstrdup(value);
    }
}

static void davServer(void * userdata, const char * value)
{
    FD_t ctrl = (FD_t) userdata;
    urlinfo u;

    if (!(ctrl != NULL && value != NULL)) return;
DAVDEBUG(-1, (stderr, "*** fd %p Server: %s\n", ctrl, value));

    u = (urlinfo) ctrl->u;
    URLSANE(u);

    if (value && u->server == NULL) {		/* XXX test for NULL? */
	u->server = _free(u->server);
	u->server = xstrdup(value);
    }
}

static void davAllow(void * userdata, const char * value)
{
    FD_t ctrl = (FD_t) userdata;
    urlinfo u;

    if (!(ctrl != NULL && value != NULL)) return;
DAVDEBUG(-1, (stderr, "*** fd %p Allow: %s\n", ctrl, value));

    u = (urlinfo) ctrl->u;
    URLSANE(u);

    if (value && u->allow == NULL) {		/* XXX test for NULL? */
	u->allow = _free(u->allow);
	u->allow = xstrdup(value);
    }
}

static void davLocation(void * userdata, const char * value)
{
    FD_t ctrl = (FD_t) userdata;
    urlinfo u;

    if (!(ctrl != NULL && value != NULL)) return;
DAVDEBUG(-1, (stderr, "*** fd %p Location: %s\n", ctrl, value));

    u = (urlinfo) ctrl->u;
    URLSANE(u);

    if (value && u->location == NULL) {		/* XXX test for NULL? */
	u->location = _free(u->location);
	u->location = xstrdup(value);
    }
}

static void davETag(void * userdata, const char * value)
{
    FD_t ctrl = (FD_t) userdata;
    urlinfo u;

    if (!(ctrl != NULL && value != NULL)) return;
DAVDEBUG(-1, (stderr, "*** fd %p ETag: %s\n", ctrl, value));

    u = (urlinfo) ctrl->u;
    URLSANE(u);

    if (value && u->location == NULL) {		/* XXX test for NULL? */
	u->etag = _free(u->etag);
	u->etag = xstrdup(value);
    }
}

struct davHeaders_s {
    const char *name;
    void (*save) (void *userdata, const char *value);
} davHeaders[] = {
    { "Content-Length",		davContentLength },
    { "Content-Type",		davContentType },
    { "Content-Disposition",	davContentDisposition },
    { "Last-Modified",		davLastModified },
    { "Connection",		davConnection },
    { "Date",			davDate },
    { "Server",			davServer },
    { "Allow",			davAllow },
    { "Location",		davLocation },
    { "ETag",			davETag },
    { NULL, NULL }
};
static size_t ndavHeaders = sizeof(davHeaders)/sizeof(davHeaders[0]);

#if !defined(HAVE_NEON_NE_GET_RESPONSE_HEADER)
static void davAllHeaders(void * userdata, const char * value)
{
    FD_t ctrl = (FD_t) userdata;

    if (!(ctrl != NULL && value != NULL)) return;
DAVDEBUG(1, (stderr, "<- %s\n", value));
}
#endif

/* =============================================================== */

/* HACK davHEAD() should be rewritten to use davReq/davResp w callbacks. */
static int davHEAD(urlinfo u, struct stat *st) 
{
    ne_request *req;
    const ne_status *status = NULL;
    const char *htag;
    const char *value = NULL;
    int rc;
int printing = 0;

    /* XXX HACK: URI's with pesky trailing '/' are directories. */
    {	size_t nb = strlen(u->url);
	st->st_mode = (u->url[nb-1] == '/' ? S_IFDIR : S_IFREG);
    }
    st->st_blksize = 4 * 1024;	/* HACK correct for linux ext */
    st->st_atime = -1;
    st->st_mtime = -1;
    st->st_ctime = -1;

    req = ne_request_create((ne_session *)u->sess, "HEAD", u->url);
    if (rpmioHttpAccept != NULL)
	ne_add_request_header(req, "Accept", rpmioHttpAccept);

    /* XXX if !defined(HAVE_NEON_NE_GET_RESPONSE_HEADER) handlers? */

    rc = davCheck(req, "ne_request_dispatch",
		ne_request_dispatch(req));
    status = ne_get_status(req);

/* XXX somewhere else instead? */
DAVDEBUG(1, (stderr, "HTTP request sent, awaiting response... %d %s\n", status->code, status->reason_phrase));

    switch (rc) {
    case NE_OK:
	if (status->klass != 2)		/* XXX is this necessary? */
	    rc = NE_ERROR;
	break;
    case NE_ERROR:
#ifdef	NEVER	/* XXX not returned */
    case NE_LOOKUP:
#endif
    case NE_AUTH:
    case NE_PROXYAUTH:
    case NE_CONNECT:
    case NE_TIMEOUT:
#ifdef	NEVER	/* XXX not returned */
    case NE_FAILED:
    case NE_RETRY:
    case NE_REDIRECT:
#endif
    default:
	goto exit;
	break;
    }

#if defined(HAVE_NEON_NE_GET_RESPONSE_HEADER)
    davDate(u->ctrl,	ne_get_response_header(req, "Date"));	/* XXX needed? */
    davServer(u->ctrl,	ne_get_response_header(req, "Server"));	/* XXX needed? */
    davAllow(u->ctrl,	ne_get_response_header(req, "Allow"));	/* XXX needed? */
    davLocation(u->ctrl,ne_get_response_header(req, "Location"));
    davETag(u->ctrl,	ne_get_response_header(req, "ETag"));

/* XXX Content-Length: is returned only for files. */
    htag = "Content-Length";
    value = ne_get_response_header(req, htag); 
    if (value) {
/* XXX should wget's "... (1.2K)..." be added? */
if (_dav_debug && ++printing)
fprintf(stderr, "Length: %s", value);
	st->st_size = strtoll(value, NULL, 10);
	st->st_blocks = (st->st_size + 511)/512;
    } else {
	st->st_size = 0;
	st->st_blocks = 0;
    }

    htag = "Content-Type";
    value = ne_get_response_header(req, htag); 
    if (value) {
if (_dav_debug && printing)
fprintf(stderr, " [%s]", value);
	if (!strcmp(value, "text/html")
	 || !strcmp(value, "application/xhtml+xml"))
	    st->st_blksize = 2 * 1024;
	else
	if (!strcmp(value, "application/x-redhat-package-manager")
	 || !strcmp(value, "application/x-rpm-package-manager"))
	    st->st_blksize = 4 * 1024;	/* XXX W2DO? */
    }

    htag = "Last-Modified";
    value = ne_get_response_header(req, htag); 
    if (value) {
if (_dav_debug && printing)
fprintf(stderr, " [%s]", value);
	st->st_mtime = ne_httpdate_parse(value);
	st->st_atime = st->st_ctime = st->st_mtime;	/* HACK */
    }

if (_dav_debug && printing)
fprintf(stderr, "\n");
#endif	/* HAVE_NEON_NE_GET_RESPONSE_HEADER */

exit:
    ne_request_destroy(req);
    return rc;
}

/* XXX TODO move to rpmhtml.c */
typedef struct rpmhtml_s * rpmhtml;
#endif	/* WITH_NEON */

int _html_debug = 0;

rpmioPool _htmlPool = NULL;

#ifdef WITH_NEON
struct rpmhtml_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
    rpmavx avx;
    ne_request *req;

    const char * pattern;
    miRE mires;
    int nmires;

    char * buf;
    size_t nbuf;
    char * b;
    size_t nb;
#if defined(__LCLINT__)
/*@refs@*/
    int nrefs;			/*!< (unused) keep splint happy */
#endif
};

/**
 * Unreference a html wrapper instance.
 * @param html		html wrapper
 * @return		NULL on last derefernce
 */
rpmhtml htmlUnlink (rpmhtml html);
#define htmlUnlink(_html)  \
    ((rpmhtml)rpmioUnlinkPoolItem((rpmioItem)(_html), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a html wrapper instance.
 * @param html		html wrapper
 * @return		new html wrapper reference
 */
rpmhtml htmlLink (rpmhtml html);
#define htmlLink(_html)  \
    ((rpmhtml)rpmioLinkPoolItem((rpmioItem)(_html), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a html wrapper instance.
 * @param html		html wrapper
 * @return		NULL on last derefernce
 */
rpmhtml htmlFree (rpmhtml html);
#define htmlFree(_html)  \
    ((rpmhtml)rpmioFreePoolItem((rpmioItem)(_html), __FUNCTION__, __FILE__, __LINE__))

static void htmlFini(void * _html)
{
    rpmhtml html = (rpmhtml) _html;

    html->avx = NULL;
    if (html->req != NULL) {
	ne_request_destroy(html->req);
	html->req = NULL;
    }
    html->pattern = NULL;
    html->mires = NULL;
    html->nmires = 0;
    html->b = html->buf = _free(html->buf);
    html->nb = html->nbuf = 0;
}

static rpmhtml htmlGetPool(rpmioPool pool)
{
    rpmhtml html;

    if (_htmlPool == NULL) {
	_htmlPool = rpmioNewPool("html", sizeof(*html), -1, _html_debug,
			NULL, NULL, htmlFini);
	pool = _htmlPool;
    }
    html = (rpmhtml) rpmioGetPool(pool, sizeof(*html));
    memset(((char *)html)+sizeof(html->_item), 0, sizeof(*html)-sizeof(html->_item));
    return html;
}

static
rpmhtml htmlNew(urlinfo u, rpmavx avx) 
{
    rpmhtml html = htmlGetPool(_htmlPool);
    html->avx = avx;
    html->req = ne_request_create((ne_session *)u->sess, "GET", u->url);
    html->pattern = NULL;
    html->mires = NULL;
    html->nmires = 0;
    html->nbuf = 16 * BUFSIZ;	/* XXX larger buffer? */
    html->buf = (char *) xmalloc(html->nbuf + 1 + 1);
    html->b = NULL;
    html->nb = 0;
    return htmlLink(html);
}

static ssize_t htmlFill(rpmhtml html)
{
    char * b = html->buf;
    size_t nb = html->nbuf;
    ssize_t rc;

    if (html->b != NULL && html->nb > 0 && html->b > html->buf) {
	memmove(html->buf, html->b, html->nb);
	b += html->nb;
	nb -= html->nb;
    }
DAVDEBUG(-1, (stderr, "--> %s(%p) %p[%u]\n", __FUNCTION__, html, b, (unsigned)nb));

    /* XXX FIXME: "server awol" segfaults here. gud enuf atm ... */
    rc = ne_read_response_block(html->req, b, nb);
    if (rc > 0) {
	html->nb += rc;
	b += rc;
	nb -= rc;
    }
    html->b = html->buf;

DAVDEBUG(-1, (stderr, "<-- %s(%p) %p[%u] rc %d\n", __FUNCTION__, html, b, (unsigned)nb, (int)rc));
    return rc;
}

/**
 * Convert hex to binary nibble.
 * @param c            hex character
 * @return             binary nibble
 */
static
unsigned char nibble(char c)
{
    if (c >= '0' && c <= '9')
	return (unsigned char) (c - '0');
    if (c >= 'A' && c <= 'F')
	return (unsigned char)((int)(c - 'A') + 10);
    if (c >= 'a' && c <= 'f')
	return (unsigned char)((int)(c - 'a') + 10);
    return (unsigned char) '\0';
}

static const char * hrefpat = "(?i)<a(?:\\s+[a-z][a-z0-9_]*(?:=(?:\"[^\"]*\"|\\S+))?)*?\\s+href=(?:\"([^\"]*)\"|(\\S+))";

static int htmlParse(rpmhtml html)
{
    struct stat * st = html->avx->st;
    miRE mire;
    int noffsets = 3;
    int offsets[3];
    ssize_t nr = (html->b != NULL ? (ssize_t)html->nb : htmlFill(html));
    size_t contentLength = (nr >= 0 ? nr : 0);
    int rc = 0;
    int xx;

DAVDEBUG(-1, (stderr, "--> %s(%p) %p[%u]\n", __FUNCTION__, html, html->buf, (unsigned)html->nbuf));

    if (st) {
	st->st_mode |= 0755;	/* htmlParse() is always a directory. */
	st->st_nlink = 2;	/* count . and .. links */
    }

    html->pattern = hrefpat;
    xx = mireAppend(RPMMIRE_PCRE, 0, html->pattern, NULL, &html->mires, &html->nmires);
    mire = html->mires;

    xx = mireSetEOptions(mire, offsets, noffsets);

    while (html->nb > 0) {
	char * gbn, * href;
	const char * hbn, * lpath;
	char * be;
	char * f, * fe;
	char * g, * ge;
	size_t ng;
	char * h, * he;
	size_t nh;
	char * t;
	mode_t st_mode = S_IFREG | 0644;
	int ut;

assert(html->b != NULL);
	be = html->b + html->nb;
	*be = '\0';
	offsets[0] = offsets[1] = -1;
	xx = mireRegexec(mire, html->b, html->nb);
	if (xx == 0 && offsets[0] != -1 && offsets[1] != -1) {

	    /* [f:fe) contains |<a href="..."| match. */
	    f = html->b + offsets[0];
	    fe = html->b + offsets[1];

	    he = fe;
	    if (he[-1] == '"') he--;
	    h = he;
	    while (h > f && h[-1] != '"')
		h--;
	    /* [h:he) contains the href. */
assert(he > h);
	    nh = (size_t)(he - h);
	    href = t = (char *) xmalloc(nh + 1 + 1);	/* XXX +1 for trailing '/' */
	    *t = '\0';
	    while (h < he) {
		char c = *h++;
		switch (c) {
		default:
		    break;
		case '%':
		    if (isxdigit((int)h[0]) && isxdigit((int)h[1])) {
			c = (char) (nibble(h[0]) << 4) | nibble(h[1]);
			h += 2;
		    }
		    break;
		}
		*t++ = c;
	    }
	    *t = '\0';

	    /* Determine type of href. */
	    switch ((ut = urlPath(href, &lpath))) {
	    case URL_IS_UNKNOWN:
	    default:
		/* XXX verify "same tree" as root URI. */
		if (href[nh-1] == '/') {
		    st_mode = S_IFDIR | 0755;
		    href[nh-1] = '\0';
		} else
		    st_mode = S_IFREG | 0644;
		break;
	    case URL_IS_FTP:
	    case URL_IS_HTTPS:
	    case URL_IS_HTTP:
#ifdef	NOTYET	/* XXX rpmavx needs to save linktos first. */
		st_mode = S_IFLNK | 0755;
		break;
#endif
	    case URL_IS_PATH:
	    case URL_IS_DASH:
	    case URL_IS_HKP:
		href[0] = '\0';
		break;
	    }
	    if ((hbn = strrchr(href, '/')) != NULL)
		hbn++;
	    else
		hbn = href;
assert(hbn != NULL);

	    /* Parse the URI path. */
	    g = fe;
	    while (g < be && *g && *g != '>')
		g++;
	    if (g >= be || *g != '>') {
		href = _free(href);
		goto refill;
	    }
	    ge = ++g;
	    while (ge < be && *ge && *ge != '<')
		ge++;
	    if (ge >= be || *ge != '<') {	
		href = _free(href);
		goto refill;
	    }
	    /* [g:ge) contains the URI basename. */
	    ng = (size_t)(ge - g);
	    gbn = t = (char *) xmalloc(ng + 1 + 1);
	    while (g < ge && *g != '/')		/* XXX prohibit '/' in gbn. */
		*t++ = *g++;
	    *t = '\0';

if (_dav_debug)
if (*hbn != '\0' && *gbn != '\0' && strcasecmp(hbn, gbn))
fprintf(stderr, "\t[%s] != [%s]\n", hbn, gbn);

	    /*
	     * Heuristics to identify HTML sub-directories:
	     *   Avoid empty strings.
	     *   Both "." and ".." will be added by rpmavx.
	     *
	     * Assume (case insensitive) basename(href) == basename(URI) is
	     * a subdirectory.
	     */
	    if (*hbn != '\0' && *gbn != '\0')
	    if (strcmp(hbn, ".") && strcmp(hbn, ".."))
	    if (!strcasecmp(hbn, gbn)) {
		size_t _st_size = (size_t)0;	/* XXX HACK */
		time_t _st_mtime = (time_t)0;	/* XXX HACK */
		xx = rpmavxAdd(html->avx, gbn, st_mode, _st_size, _st_mtime);
		/* count subdir links */
		if (st && S_ISDIR(st_mode)) st->st_nlink++;
	    }

	    gbn = _free(gbn);
	    href = _free(href);

	    offsets[1] += (ge - fe);
	    html->b += offsets[1];
	    html->nb -= offsets[1];
	} else {
	    size_t nb = html->nb;
	    if (nr > 0) nb -= 256;	/* XXX overlap a bit if filling. */
	    html->b += nb;
	    html->nb -= nb;
	}

	/* XXX Refill iff lowater reaches nbuf/4 (~2kB) */
	if (nr <= 0 || html->nb >= (html->nbuf/4))
	    continue;
refill:
	if ((nr = htmlFill(html)) >= 0)
	    contentLength += nr;
    }

    /* XXX Set directory length to no. of bytes of HTML parsed. */
    if (st) {
	if (st->st_size == 0) {
	    st->st_size = contentLength;
	    st->st_blocks = (st->st_size + 511)/512;
	}
    }

    xx = mireSetEOptions(mire, NULL, 0);

    html->mires = (miRE) mireFreeAll(html->mires, html->nmires);
    html->nmires = 0;

DAVDEBUG(-1, (stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, html, rc));
    return rc;
}

/* HACK htmlNLST() should be rewritten to use davReq/davResp w callbacks. */
static int htmlNLST(urlinfo u, rpmavx avx) 
{
    rpmhtml html = htmlNew(u, avx);
    int rc = 0;

    do {
	rc = davCheck(html->req, "ne_begin_request",
		ne_begin_request(html->req));
	switch (rc) {
	case NE_OK:
	    break;
	case NE_TIMEOUT:
	    errno = ETIMEDOUT;
	    /*@fallthrough@*/
	case NE_ERROR:
	case NE_LOOKUP:
	case NE_AUTH:
	case NE_PROXYAUTH:
	case NE_CONNECT:
	case NE_FAILED:
	case NE_RETRY:
	case NE_REDIRECT:
	default:
	    goto exit;
	    break;
	}

	(void) htmlParse(html);		/* XXX error code needs handling. */

	rc = davCheck(html->req, "ne_end_request",
		ne_end_request(html->req));
    } while (rc == NE_RETRY);

exit:
    html = htmlFree(html);
    return rc;
}

static int davNLST(rpmavx avx)
{
    urlinfo u = NULL;
    int rc;
    int xx;

retry:
    rc = davInit(avx->uri, &u);
    if (rc || u == NULL)
	goto exit;

    /*
     * Do PROPFIND through davFetch iff server supports.
     * Otherwise, do HEAD to get Content-length/ETag/Last-Modified,
     * followed by GET through htmlNLST() to find the contained href's.
     */
    if (u->caps & RPMURL_SERVER_HASDAV)
	rc = davFetch(u, avx);	/* use PROPFIND to get contentLength */
    else {
	rc = davHEAD(u, avx->st);	/* use HEAD to get contentLength */
	/* Parse directory elements. */
	if (rc == NE_OK && S_ISDIR(avx->st->st_mode))
	    rc = htmlNLST(u, avx);
    }

    switch (rc) {
    case NE_OK:
        break;
    case NE_ERROR:
	/* HACK: "405 Method Not Allowed" for PROPFIND on non-DAV servers. */
	/* XXX #206066 OPTIONS is ok, but PROPFIND from Stat() fails. */
	/* rpm -qp --rpmiodebug --davdebug http://people.freedesktop.org/~sandmann/metacity-2.16.0-2.fc6/i386/metacity-2.16.0-2.fc6.i386.rpm */

	/* HACK: "301 Moved Permanently" on empty subdir. */
	if (!strncmp("301 ", ne_get_error((ne_session *)u->sess), sizeof("301 ")-1))
	    break;

	/* HACK: "302 Found" if URI is missing pesky trailing '/'. */
	if (!strncmp("302 ", ne_get_error((ne_session *)u->sess), sizeof("302 ")-1)) {
	    const char * path = NULL;
	    int ut = urlPath(u->url, &path);
	    size_t nb = strlen(path);
	    ut = ut;	/* XXX keep gcc happy */
	    if (u->location != NULL && !strncmp(path, u->location, nb)
	     && u->location[nb] == '/' && u->location[nb+1] == '\0')
	    {
		char * te = (char *) strchr(u->url, '\0');
		/* Append the pesky trailing '/'. */
		if (te != NULL && te[-1] != '/') {
		    /* XXX u->uri malloc'd w room for +1b */
		    *te++ = '/';
		    *te = '\0';
		    u->location = _free(u->location);
		    /* XXX retry here needed iff ContentLength:. */
		    xx = davFree(u);
		    goto retry;
		    break;
		}
	    }
	}
	/*@fallthrough@*/
    case NE_LOOKUP:
    case NE_AUTH:
    case NE_PROXYAUTH:
    case NE_CONNECT:
    case NE_TIMEOUT:
    case NE_FAILED:
    case NE_RETRY:
    case NE_REDIRECT:
    default:
DAVDEBUG(1, (stderr, "*** Fetch from %s:%d failed rc(%d):\n\t%s\n",
		   u->host, u->port, rc, ne_get_error((ne_session *)u->sess)));
        break;
    }

exit:
    /* XXX Destroy the session iff not OK, otherwise persist. */
    if (rc)
	xx = davFree(u);
    return rc;
}

/* =============================================================== */
int davResp(urlinfo u, FD_t ctrl, char *const * str)
{
    int rc = 0;

DAVDEBUG(-1, (stderr, "--> %s(%p,%p,%p) sess %p req %p\n", __FUNCTION__, u, ctrl, str, u->sess, ctrl->req));

    rc = davCheck(ctrl->req, "ne_begin_request",
		ne_begin_request((ne_request *)ctrl->req));

    /* HACK FTPERR_NE_FOO == -NE_FOO error impedance match */
    if (rc)
	fdSetSyserrno(ctrl, errno, ftpStrerror(-rc));

DAVDEBUG(-1, (stderr, "<-- %s(%p,%p,%p) sess %p req %p rc %d\n", __FUNCTION__, u, ctrl, str, u->sess, ctrl->req, rc));

    return rc;
}

int davReq(FD_t ctrl, const char * httpCmd, const char * httpArg)
{
    urlinfo u;
    const char * path = NULL;
    int ut;
    size_t npath = 0;
    int rc = 0;

assert(ctrl != NULL);
    u = (urlinfo) ctrl->u;
    URLSANE(u);
    ut = urlPath(u->url, &path);
    npath = strlen(path);

DAVDEBUG(-1, (stderr, "--> %s(%p,%s,\"%s\") entry sess %p req %p\n", __FUNCTION__, ctrl, httpCmd, (httpArg ? httpArg : ""), u->sess, ctrl->req));

    if (strncmp(httpArg, path, npath)) {
if (_dav_debug < 0)
fprintf(stderr, "\tREDIRECT %s -> %s\n", httpArg, path);
	httpArg = path;
    }

    ctrl->persist = (u->httpVersion > 0 ? 1 : 0);
    ctrl = fdLink(ctrl, "open ctrl (davReq)");
assert(ctrl != NULL);

assert(u->sess != NULL);
    /* XXX reset disconnected handle to NULL. should never happen ... */
    if (ctrl->req == (void *)-1)
	ctrl->req = NULL;
assert(ctrl->req == NULL);
    ctrl->req = ne_request_create((ne_session *)u->sess, httpCmd, httpArg);
assert(ctrl->req != NULL);

    ne_set_request_private((ne_request *)ctrl->req, "fd", ctrl);

#ifdef	NOTYET	/* XXX "Pull"-based requests. */
    ne_set_request_body_provider((ne_request *)ctrl->req, ne_off_t length,
                                  ne_provide_body provider, void *userdata);
    xx = ne_accept_2xx(void *userdata, (ne_request *)ctrl->req, const ne_status *st);
    xx = ne_accept_always(void *userdata, (ne_request *)ctrl->req, const ne_status *st);
    ne_add_response_body_reader((ne_request *)ctrl->req, ne_accept_response accpt,
                                 ne_block_reader reader, void *userdata);
#endif

    /* XXX NE_REQFLAG_EXPECT100 for "Expect: 100-continue" */
    /* XXX OK for GET, disable for POST */
    ne_set_request_flag((ne_request *)ctrl->req, NE_REQFLAG_IDEMPOTENT, 1);

#if !defined(HAVE_NEON_NE_GET_RESPONSE_HEADER)
    ne_add_response_header_catcher((ne_request *)ctrl->req, davAllHeaders, ctrl);

    {	struct davHeaders_s * p;
	for (p = davHeaders; p->name != NULL; p++)
	    ne_add_response_header_handler((ne_request *)ctrl->req,
		p->name, p->save, ctrl);
    }

#endif

    if (!strcmp(httpCmd, "PUT")) {
#if defined(HAVE_NEON_NE_SEND_REQUEST_CHUNK)
	ctrl->wr_chunked = 1;
	ne_add_request_header((ne_request *)ctrl->req,
		"Transfer-Encoding", "chunked");
	ne_set_request_chunked((ne_request *)ctrl->req, 1);
	/* HACK: no retries if/when chunking. */
	rc = davResp(u, ctrl, NULL);
#else
	rc = FTPERR_SERVER_IO_ERROR;
#endif
    } else {
	/* HACK: possible ETag: "inode-size-mtime" */
#if !defined(HAVE_NEON_NE_GET_RESPONSE_HEADER)
	ne_add_response_header_handler((ne_request *)ctrl->req, "Accept-Ranges",
			davAcceptRanges, u);
#endif
	/* HACK: possible Transfer-Encoding: on GET. */

	/* HACK: other errors may need retry too. */
	/* HACK: neon retries once, gud enuf. */
	/* HACK: retry counter? */
	do {
	    rc = davResp(u, ctrl, NULL);
	} while (rc == NE_RETRY);
    }

/* XXX somewhere else instead? */
if (_dav_debug) {
    const ne_status *status = ne_get_status((ne_request *)ctrl->req);
fprintf(stderr, "HTTP request sent, awaiting response... %d %s\n", status->code, status->reason_phrase);
}

    if (rc)
	goto errxit;

DAVDEBUG(-1, (stderr, "<-- %s(%p,%s,\"%s\") exit sess %p req %p rc %d\n", __FUNCTION__, ctrl, httpCmd, (httpArg ? httpArg : ""), u->sess, ctrl->req, rc));

#if defined(HAVE_NEON_NE_GET_RESPONSE_HEADER)

#ifdef	NOTYET
    void *ptr =
	ne_response_header_iterate(ne_request *req, void *cursor,
                                 const char **name, const char **value);
#endif

    {	struct davHeaders_s * p;
	for (p = davHeaders; p->name != NULL; p++)
	    (*p->save) (ctrl,
		ne_get_response_header((ne_request *)ctrl->req, p->name));
    }

    if (strcmp(httpCmd, "PUT"))
	davAcceptRanges(u,
		ne_get_response_header((ne_request *)ctrl->req, "Accept-Ranges"));

#endif

    ctrl = fdLink(ctrl, "open data (davReq)");
    return 0;

errxit:
    fdSetSyserrno(ctrl, errno, ftpStrerror(rc));

    /* HACK balance fd refs. ne_session_destroy to tear down non-keepalive? */
    ctrl = fdLink(ctrl, "error data (davReq)");

    return rc;
}

FD_t davOpen(const char * url, int flags,
		mode_t mode, urlinfo * uret)
{
    const char * path = NULL;
    urltype ut = urlPath(url, &path);
    urlinfo u = NULL;
    FD_t fd = NULL;
    int rc;

#if 0	/* XXX makeTempFile() heartburn */
    assert(!(flags & O_RDWR));
#endif

DAVDEBUG(-1, (stderr, "--> %s(%s,0x%x,0%o,%p)\n", __FUNCTION__, url, flags, (unsigned)mode, uret));
    rc = davInit(url, &u);
    if (rc || u == NULL || u->sess == NULL)
	goto exit;

    if (u->ctrl == NULL)
	u->ctrl = fdNew("persist ctrl (davOpen)");
    else {
	yarnLock use = u->ctrl->_item.use;
	yarnPossess(use);
	if (yarnPeekLock(use) > 2L && u->data == NULL)
	    u->data = fdNew("persist data (davOpen)");
	yarnRelease(use);
    }

    if (u->ctrl->u == NULL)
	fd = u->ctrl = fdLink(u->ctrl, "grab ctrl (davOpen persist ctrl)");
    else if (u->data->u == NULL)
	fd = u->data = fdLink(u->data, "grab ctrl (davOpen persist data)");
    else
	fd = fdNew("grab ctrl (davOpen)");

    if (fd) {
	fdSetOpen(fd, url, flags, mode);
	fdSetIo(fd, ufdio);

	fd->ftpFileDoneNeeded = 0;
	fd->rd_timeoutsecs = rpmioHttpReadTimeoutSecs;
	fd->contentLength = fd->bytesRemain = -1;
assert(ut == URL_IS_HTTPS || ut == URL_IS_HTTP || ut == URL_IS_HKP);
	fd->u = urlLink(u, "url (davOpen)");
	fd = fdLink(fd, "grab data (davOpen)");
    }

exit:
    if (uret)
	*uret = u;
    return fd;
}

ssize_t davRead(void * cookie, char * buf, size_t count)
{
    FD_t fd = (FD_t) cookie;
    ssize_t rc;

  { urlinfo u = NULL;
    u = urlLink(fd->u, "url (davRead)");
    if (u->info.status == ne_status_recving)
	rc = ne_read_response_block((ne_request *)fd->req, buf, count);
    else {
	/* If server has disconnected, then tear down the neon request. */
	if (u->info.status == ne_status_disconnected) {
	    int xx;
	    xx = davCheck(fd->req, "ne_end_request",
			ne_end_request((ne_request *)fd->req));
	    ne_request_destroy((ne_request *)fd->req);
	    fd->req = (void *)-1;	/* XXX see davReq assert failure */
	}
	errno = EIO;       /* XXX what to do? */
	rc = -1;
    }
    u = urlFree(u, "url (davRead)");
  }

DAVDEBUG(-1, (stderr, "<-- %s(%p,%p,0x%x) rc 0x%x\n", __FUNCTION__, cookie, buf, (unsigned)count, (unsigned)rc));

    return rc;
}

ssize_t davWrite(void * cookie, const char * buf, size_t count)
{
#if !defined(NEONBLOWSCHUNKS) || defined(HAVE_NEON_NE_SEND_REQUEST_CHUNK) || defined(__LCLINT__)
    FD_t fd = (FD_t) cookie;
#endif
    ssize_t rc;
    int xx = -1;

#if !defined(NEONBLOWSCHUNKS)
    ne_session * sess;

assert(fd->req != NULL);
    sess = ne_get_session((ne_request *)fd->req);
assert(sess != NULL);

    /* HACK: include ne_private.h to access sess->socket for now. */
    xx = ne_sock_fullwrite(sess->socket, buf, count);
#else
#if defined(HAVE_NEON_NE_SEND_REQUEST_CHUNK) || defined(__LCLINT__)
assert(fd->req != NULL);
    xx = ne_send_request_chunk((ne_request *)fd->req, buf, count);
#else
    errno = EIO;       /* HACK */
    return -1;
#endif
#endif

    /* HACK: stupid error impedance matching. */
    rc = (xx == 0 ? (ssize_t)count : -1);

DAVDEBUG(-1, (stderr, "<-- %s(%p,%p,0x%x) rc 0x%x\n", __FUNCTION__, cookie, buf, (unsigned)count, (unsigned)rc));

    return rc;
}

int davSeek(void * cookie, _libio_pos_t pos, int whence)
{
    int rc = -1;
DAVDEBUG(-1, (stderr, "<-- %s(%p,pos,%d) rc %d\n", __FUNCTION__, cookie, whence, rc));
    return rc;
}

int davClose(void * cookie)
{
    FD_t fd = (FD_t) cookie;
    int rc = 0;

DAVDEBUG(-1, (stderr, "--> %s(%p) rc %d clen %d req %p u %p\n", __FUNCTION__, fd, rc, (int)fd->bytesRemain, fd->req, fd->u));

assert(fd->req != NULL);
    if (fd->req != (void *)-1) {
	rc = davCheck(fd->req, "ne_end_request",
		ne_end_request((ne_request *)fd->req));
	ne_request_destroy((ne_request *)fd->req);
    }
    fd->req = (void *)NULL;

DAVDEBUG(-1, (stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, fd, rc));
    return rc;
}

/* =============================================================== */
int davMkdir(const char * path, mode_t mode)
{
    urlinfo u = NULL;
    const char * src = NULL;
    int rc;

    rc = davInit(path, &u);
    if (rc)
	goto exit;
assert(u != NULL);

    (void) urlPath(path, &src);

    rc = davCheck(u->sess, "ne_mkcol",
		ne_mkcol((ne_session *)u->sess, path));

    if (rc) rc = -1;	/* XXX HACK: errno impedance match */

    /* XXX HACK: verify getrestype(remote) == resr_collection */

exit:
DAVDEBUG(1, (stderr, "<-- %s(%s,0%o) rc %d\n", __FUNCTION__, path, (unsigned)mode, rc));
    return rc;
}

int davRmdir(const char * path)
{
    urlinfo u = NULL;
    const char * src = NULL;
    int rc;

    rc = davInit(path, &u);
    if (rc)
	goto exit;
assert(u != NULL);

    (void) urlPath(path, &src);

    /* XXX HACK: only getrestype(remote) == resr_collection */

    rc = davCheck(u->sess, "ne_delete",
		ne_delete((ne_session *)u->sess, path));

    if (rc) rc = -1;	/* XXX HACK: errno impedance match */

exit:
DAVDEBUG(1, (stderr, "<-- %s(%s) rc %d\n", __FUNCTION__, path, rc));
    return rc;
}

int davRename(const char * oldpath, const char * newpath)
{
    urlinfo u = NULL;
    const char * src = NULL;
    const char * dst = NULL;
    int overwrite = 1;		/* HACK: set this correctly. */
    int rc;

    rc = davInit(oldpath, &u);
    if (rc)
	goto exit;
assert(u != NULL);

    (void) urlPath(oldpath, &src);
    (void) urlPath(newpath, &dst);

    /* XXX HACK: only getrestype(remote) != resr_collection */

    rc = davCheck(u->sess, "ne_move",
		ne_move((ne_session *)u->sess, overwrite, src, dst));

    if (rc) rc = -1;	/* XXX HACK: errno impedance match */

exit:
DAVDEBUG(1, (stderr, "<-- %s(%s,%s) rc %d\n", __FUNCTION__, oldpath, newpath, rc));
    return rc;
}

int davUnlink(const char * path)
{
    urlinfo u = NULL;
    const char * src = NULL;
    int rc;

    rc = davInit(path, &u);
    if (rc)
	goto exit;
assert(u != NULL);

    (void) urlPath(path, &src);

    /* XXX HACK: only getrestype(remote) != resr_collection */

    rc = davCheck(u->sess, "ne_delete",
		ne_delete((ne_session *)u->sess, src));

exit:
    if (rc) rc = -1;	/* XXX HACK: errno impedance match */

DAVDEBUG(1, (stderr, "<-- %s(%s) rc %d\n", __FUNCTION__, path, rc));
    return rc;
}

#ifdef	NOTYET
static int davChdir(const char * path)
{
    return davCommand("CWD", path, NULL);
}
#endif	/* NOTYET */

/* =============================================================== */

static const char * statstr(const struct stat * st, char * buf)
{
    sprintf(buf,
	"dev 0x%x ino 0x%x mode 0%0o nlink %d uid %d gid %d rdev 0x%x size %u",
	(unsigned)st->st_dev,
	(unsigned)st->st_ino,
	(unsigned)st->st_mode,
	(unsigned)st->st_nlink,
	(unsigned)st->st_uid,
	(unsigned)st->st_gid,
	(unsigned)st->st_rdev,
	(unsigned)st->st_size);
    return buf;
}

int davStat(const char * path, struct stat *st)
{
    rpmavx avx = NULL;
    char buf[1024];
    int rc = -1;

DAVDEBUG(-1, (stderr, "--> %s(%s)\n", __FUNCTION__, path));
    if (path == NULL || *path == '\0') {
	errno = ENOENT;
	goto exit;
    }
    avx = (rpmavx) rpmavxNew(path, st);
    if (avx == NULL) {
	errno = ENOENT;		/* Note: avx is NULL iff urlSplit() fails. */
	goto exit;
    }
    rc = davNLST(avx);
    if (rc) {
	if (errno == 0)	errno = EAGAIN;	/* HACK: errno = ??? */
	rc = -1;
	goto exit;
    }

    /* XXX fts(3) needs/uses st_ino. */
    /* Hash the path to generate a st_ino analogue. */
    st->st_ino = hashFunctionString(0, path, 0);

    /* XXX rpmct.c does Lstat() -> Fchmod() on target fd. */
    /* XXX HACK: ensure st->st_mode is sane, not 0000 */
    if ((st->st_mode & 07777) == 00000)
    switch(st->st_mode) {
    case S_IFDIR:	st->st_mode |= 0755;	break;
    default:
    case S_IFREG:	st->st_mode |= 0644;	break;
    }

exit:
DAVDEBUG(-1, (stderr, "<-- %s(%s) rc %d\n\t%s\n", __FUNCTION__, path, rc, statstr(st, buf)));
    avx = rpmavxFree(avx);
    return rc;
}

int davLstat(const char * path, struct stat *st)
{
    rpmavx avx = NULL;
    char buf[1024];
    int rc = -1;

    if (path == NULL || *path == '\0') {
	errno = ENOENT;
	goto exit;
    }
    avx = (rpmavx) rpmavxNew(path, st);
    if (avx == NULL) {
	errno = ENOENT;		/* Note: avx is NULL iff urlSplit() fails. */
	goto exit;
    }
    rc = davNLST(avx);
    if (rc) {
	if (errno == 0)	errno = EAGAIN;	/* HACK: errno = ??? */
	rc = -1;
	goto exit;
    }

    /* XXX fts(3) needs/uses st_ino. */
    /* Hash the path to generate a st_ino analogue. */
    st->st_ino = hashFunctionString(0, path, 0);

    /* XXX rpmct.c does Lstat() -> Fchmod() on target fd. */
    /* XXX HACK: ensure st->st_mode is sane, not 0000 */
    if ((st->st_mode & 07777) == 00000)
    switch(st->st_mode) {
    case S_IFDIR:	st->st_mode |= 0755;	break;
    default:
    case S_IFREG:	st->st_mode |= 0644;	break;
    }


DAVDEBUG(-1, (stderr, "<-- %s(%s) rc %d\n\t%s\n", __FUNCTION__, path, rc, statstr(st, buf)));
exit:
    avx = rpmavxFree(avx);
    return rc;
}

#ifdef	NOTYET
static int davReadlink(const char * path, char * buf, size_t bufsiz)
{
    int rc;
    rc = davNLST(path, DO_FTP_READLINK, NULL, buf, bufsiz);
DAVDEBUG(-1, (stderr, "<-- %s(%s) rc %d\n", __FUNCTION__, path, rc));
    return rc;
}
#endif	/* NOTYET */

#endif /* WITH_NEON */

/* =============================================================== */
int avmagicdir = 0x3607113;

#ifndef WITH_NEON
FD_t httpOpen(const char * url, int flags,
                mode_t mode, urlinfo * uret)
{
    urlinfo u = NULL;
    FD_t fd = NULL;

#if 0   /* XXX makeTempFile() heartburn */
    assert(!(flags & O_RDWR));
#endif
    if (urlSplit(url, &u))
        goto exit;

    if (u->ctrl == NULL)
        u->ctrl = fdNew("persist ctrl (httpOpen)");
    if (u->ctrl != NULL) {	/* XXX can't happen */
	yarnLock use = u->ctrl->_item.use;
	yarnPossess(use);
	if (yarnPeekLock(use) > 2L && u->data == NULL)
	    u->data = fdNew("persist data (httpOpen)");
	yarnRelease(use);
    }

    if (u->ctrl->u == NULL)
        fd = fdLink(u->ctrl, "grab ctrl (httpOpen persist ctrl)");
    else if (u->data->u == NULL)
        fd = fdLink(u->data, "grab ctrl (httpOpen persist data)");
    else
        fd = fdNew("grab ctrl (httpOpen)");

    if (fd) {
        fdSetIo(fd, ufdio);
        fd->ftpFileDoneNeeded = 0;
	fd->rd_timeoutsecs = rpmioHttpReadTimeoutSecs;
        fd->contentLength = fd->bytesRemain = -1;
        fd->u = urlLink(u, "url (httpOpen)");
        fd = fdLink(fd, "grab data (httpOpen)");
    }

exit:
    if (uret)
        *uret = u;
    return fd;
}
#endif

#ifdef WITH_NEON
/* =============================================================== */
int davClosedir(DIR * dir)
{
    return avClosedir(dir);
}

struct dirent * davReaddir(DIR * dir)
{
    return avReaddir(dir);
}

DIR * davOpendir(const char * path)
{
    AVDIR avdir = NULL;
    rpmavx avx = NULL;
    struct stat sb, *st = &sb; /* XXX HACK: davHEAD needs avx->st. */
    const char * uri = NULL;
    int rc;

DAVDEBUG(-1, (stderr, "--> %s(%s)\n", __FUNCTION__, path));

    if (path == NULL || *path == '\0') {
	errno = ENOENT;
	goto exit;
    }

    /* Note: all Opendir(3) URI's need pesky trailing '/' */
    if (path[strlen(path)-1] != '/')
	uri = rpmExpand(path, "/", NULL);
    else
	uri = xstrdup(path);

    /* Load DAV collection into argv. */
    /* XXX HACK: davHEAD needs avx->st. */
    avx = (rpmavx) rpmavxNew(uri, st);
    if (avx == NULL) {
	errno = ENOENT;		/* Note: avx is NULL iff urlSplit() fails. */
	goto exit;
    }

    rc = davNLST(avx);
    if (rc) {
	if (errno == 0)	errno = EAGAIN;	/* HACK: errno = ??? */
	goto exit;
    } else
	avdir = (AVDIR) avOpendir(uri, avx->av, avx->modes);

exit:
    uri = _free(uri);
    avx = rpmavxFree(avx);
    return (DIR *) avdir;
}

char * davRealpath(const char * path, char * resolved_path)
{
assert(resolved_path == NULL);	/* XXX no POSIXly broken realpath(3) here. */
    /* XXX TODO: handle redirects. For now, just dupe the path. */
    return xstrdup(path);
}

#endif /* WITH_NEON */

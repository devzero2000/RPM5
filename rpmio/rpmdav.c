/*@-modfilesys@*/
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
/*@-exportheader -redecl @*/
extern void ERR_remove_state(int foo);
extern void ENGINE_cleanup(void);
extern void CONF_modules_unload(int foo);
extern void ERR_free_strings(void);
extern void EVP_cleanup(void);
extern void CRYPTO_cleanup_all_ex_data(void);
extern void CRYPTO_mem_leaks(void * ptr);
/*@=exportheader =redecl @*/
#endif

#include "ne_md5.h" /* for version detection only */

/* poor-man's NEON version determination */
#if defined(NE_MD5_H)
#define WITH_NEON_MIN_VERSION 0x002700
#elif defined(NE_FEATURE_I18N)
#define WITH_NEON_MIN_VERSION 0x002600
#else
#define WITH_NEON_MIN_VERSION 0x002500
#endif

/* XXX API changes for NEON 0.26 */
#if WITH_NEON_MIN_VERSION >= 0x002600
#define	ne_propfind_set_private(_pfh, _create_item, NULL) \
	ne_propfind_set_private(_pfh, _create_item, NULL, NULL)
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

/*@access DIR @*/
/*@access FD_t @*/
/*@access urlinfo @*/
/*@access miRE @*/

/* HACK: reasonable value needed (wget uses 900 as default). */
#if 0
#define TIMEOUT_SECS 60
#else
#define TIMEOUT_SECS 5
#endif

/*@unchecked@*/ /*@observer@*/
static const char _rpmioHttpUserAgent[] = PACKAGE "/" PACKAGE_VERSION;

/*@unchecked@*/
static int rpmioHttpPersist = 1;
/*@unchecked@*/
int rpmioHttpReadTimeoutSecs = TIMEOUT_SECS;
/*@unchecked@*/
int rpmioHttpConnectTimeoutSecs = TIMEOUT_SECS;
#ifdef	NOTYET
int rpmioHttpRetries = 20;
int rpmioHttpRecurseMax = 5;
int rpmioHttpMaxRedirect = 20;
#endif

/*@unchecked@*/ /*@null@*/
const char * rpmioHttpAccept;
/*@unchecked@*/ /*@null@*/
const char * rpmioHttpUserAgent;

#ifdef WITH_NEON
/* =============================================================== */
/*@-mustmod@*/
int davDisconnect(/*@unused@*/ void * _u)
{
#ifdef	NOTYET	/* XXX not quite right yet. */
    urlinfo u = (urlinfo)_u;
    int rc;

#if WITH_NEON_MIN_VERSION >= 0x002700
    rc = (u->info.status == ne_status_sending || u->info.status == ne_status_recving);
#else
    rc = 0;	/* XXX W2DO? */
#endif
    if (u != NULL && rc != 0) {
	if (u->ctrl->req != NULL) {
	    if (u->ctrl && u->ctrl->req) {
		ne_request_destroy(u->ctrl->req);
		u->ctrl->req = NULL;
	    }
	    if (u->data && u->data->req) {
		ne_request_destroy(u->data->req);
		u->data->req = NULL;
	    }
	}
    }
if (_dav_debug < 0)
fprintf(stderr, "*** davDisconnect(%p) active %d\n", u, rc);
    /* XXX return active state? */
#endif	/* NOTYET */
    return 0;
}
/*@=mustmod@*/

int davFree(urlinfo u)
{
    if (u != NULL) {
	if (u->sess != NULL) {
	    ne_session_destroy(u->sess);
	    u->sess = NULL;
	}
	switch (u->urltype) {
	default:
	    /*@notreached@*/ break;
	case URL_IS_HTTPS:
	case URL_IS_HTTP:
	case URL_IS_HKP:
	    u->capabilities = _free(u->capabilities);
	    if (u->lockstore != NULL)
		ne_lockstore_destroy(u->lockstore);
	    u->lockstore = NULL;
	    u->info.status = 0;
	    ne_sock_exit();
	    break;
	}
    }
if (_dav_debug < 0)
fprintf(stderr, "*** davFree(%p)\n", u);
    return 0;
}

void davDestroy(void)
{
#ifdef NE_FEATURE_SSL
    if (ne_has_support(NE_FEATURE_SSL)) {
/* XXX http://www.nabble.com/Memory-Leaks-in-SSL_Library_init()-t3431875.html */
	ENGINE_cleanup();
	CRYPTO_cleanup_all_ex_data();
	ERR_free_strings();
	ERR_remove_state(0);
	EVP_cleanup();
	CRYPTO_mem_leaks(NULL);
	CONF_modules_unload(1);
    }
#endif
if (_dav_debug < 0)
fprintf(stderr, "*** davDestroy()\n");
}

static void davProgress(void * userdata, off_t progress, off_t total)
	/*@*/
{
    urlinfo u = userdata;
    ne_session * sess;

assert(u != NULL);
    sess = u->sess;
assert(sess != NULL);
/*@-sefuncon@*/
assert(u == ne_get_session_private(sess, "urlinfo"));
/*@=sefuncon@*/

    u->info.progress = progress;
    u->info.total = total;

if (_dav_debug < 0)
fprintf(stderr, "*** davProgress(%p,0x%x:0x%x) sess %p u %p\n", userdata, (unsigned int)progress, (unsigned int)total, sess, u);
}

#if WITH_NEON_MIN_VERSION >= 0x002700
static void davNotify(void * userdata,
		ne_session_status status, const ne_session_status_info *info)
#else
static void davNotify(void * userdata,
		ne_conn_status status, const char * info)
#endif
	/*@*/
{
    char buf[64];
    urlinfo u = userdata;
    ne_session * sess;

assert(u != NULL);
    sess = u->sess;
assert(sess != NULL);
/*@-sefuncon@*/
assert(u == ne_get_session_private(sess, "urlinfo"));
/*@=sefuncon@*/

    u->info.hostname = NULL;
    u->info.address = NULL;
    u->info.progress = 0;
    u->info.total = 0;

#if WITH_NEON_MIN_VERSION >= 0x002700
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
	u->info.hostname = info->ci.hostname;
	break;
    case ne_status_connecting:	/* connecting to host */
	u->info.hostname = info->ci.hostname;
	(void) ne_iaddr_print(info->ci.address, buf, sizeof(buf));
	buf[sizeof(buf)-1] = '\0';
	u->info.address = buf;
    	break;
    case ne_status_connected:	/* connected to host */
	u->info.hostname = info->ci.hostname;
	break;
    case ne_status_sending:	/* sending a request body */
	u->info.progress = info->sr.progress;
	u->info.total = info->sr.total;
	break;
    case ne_status_recving:	/* receiving a response body */
	u->info.progress = info->sr.progress;
	u->info.total = info->sr.total;
	break;
    case ne_status_disconnected:
	u->info.hostname = info->ci.hostname;
	break;
    }

    if (u->notify != NULL)
	(void) (*u->notify) (u, status);

#else
#ifdef	REFERENCE
typedef enum {
    ne_conn_namelookup, /* lookup up hostname (info = hostname) */
    ne_conn_connecting, /* connecting to host (info = hostname) */
    ne_conn_connected, /* connected to host (info = hostname) */
    ne_conn_secure /* connection now secure (info = crypto level) */
} ne_conn_status;
#endif

if (_dav_debug < 0) {
/*@observer@*/
    static const char * connstates[] = {
	"namelookup",
	"connecting",
	"connected",
	"secure",
	"unknown"
    };

fprintf(stderr, "*** davNotify(%p,%d,%p) sess %p u %p %s\n", userdata, status, info, sess, u, connstates[ (status < 4 ? status : 4)]);
}
#endif

    u->info.status = status;
    u->info.hostname = NULL;
    u->info.address = NULL;
    u->info.progress = 0;
    u->info.total = 0;
}

static void davCreateRequest(ne_request * req, void * userdata,
		const char * method, const char * uri)
	/*@*/
{
    urlinfo u = userdata;
    ne_session * sess;
    void * private = NULL;
    const char * id = "urlinfo";

assert(u != NULL);
assert(u->sess != NULL);
assert(req != NULL);
    sess = ne_get_session(req);
assert(sess == u->sess);
/*@-sefuncon@*/
assert(u == ne_get_session_private(sess, "urlinfo"));
/*@=sefuncon@*/

assert(sess != NULL);
    private = ne_get_session_private(sess, id);
assert(u == private);

if (_dav_debug < 0)
fprintf(stderr, "*** davCreateRequest(%p,%p,%s,%s) %s:%p\n", req, userdata, method, uri, id, private);
}

static void davPreSend(ne_request * req, void * userdata, ne_buffer * buf)
{
    urlinfo u = userdata;
    ne_session * sess;
    const char * id = "fd";
    FD_t fd = NULL;

/*@-modunconnomods@*/
assert(u != NULL);
assert(u->sess != NULL);
assert(req != NULL);
    sess = ne_get_session(req);
assert(sess == u->sess);
/*@-sefuncon@*/
assert(u == ne_get_session_private(sess, "urlinfo"));
/*@=sefuncon@*/

    fd = ne_get_request_private(req, id);
/*@=modunconnomods@*/

if (_dav_debug < 0)
fprintf(stderr, "*** davPreSend(%p,%p,%p) sess %p %s %p\n", req, userdata, buf, sess, id, fd);
if (_dav_debug)
fprintf(stderr, "-> %s\n", buf->data);

}

static int davPostSend(ne_request * req, void * userdata, const ne_status * status)
	/*@*/
{
    urlinfo u = userdata;
    ne_session * sess;
    const char * id = "fd";
    FD_t fd = NULL;

assert(u != NULL);
assert(u->sess != NULL);
assert(req != NULL);
    sess = ne_get_session(req);
assert(sess == u->sess);
/*@-sefuncon@*/
assert(u == ne_get_session_private(sess, "urlinfo"));
/*@=sefuncon@*/

    fd = ne_get_request_private(req, id);

/*@-evalorder@*/
if (_dav_debug < 0)
fprintf(stderr, "*** davPostSend(%p,%p,%p) sess %p %s %p %s\n", req, userdata, status, sess, id, fd, ne_get_error(sess));
/*@=evalorder@*/
    return NE_OK;
}

static void davDestroyRequest(ne_request * req, void * userdata)
	/*@*/
{
    urlinfo u = userdata;
    ne_session * sess;
    const char * id = "fd";
    FD_t fd = NULL;

assert(u != NULL);
assert(u->sess != NULL);
assert(req != NULL);
    sess = ne_get_session(req);
assert(sess == u->sess);
/*@-sefuncon@*/
assert(u == ne_get_session_private(sess, "urlinfo"));
/*@=sefuncon@*/

    fd = ne_get_request_private(req, id);

if (_dav_debug < 0)
fprintf(stderr, "*** davDestroyRequest(%p,%p) sess %p %s %p\n", req, userdata, sess, id, fd);
}

static void davDestroySession(void * userdata)
	/*@*/
{
    urlinfo u = userdata;
    ne_session * sess;
    void * private = NULL;
    const char * id = "urlinfo";

assert(u != NULL);
assert(u->sess != NULL);
    sess = u->sess;
/*@-sefuncon@*/
assert(u == ne_get_session_private(sess, "urlinfo"));
/*@=sefuncon@*/

assert(sess != NULL);
    private = ne_get_session_private(sess, id);
assert(u == private);

if (_dav_debug < 0)
fprintf(stderr, "*** davDestroySession(%p) sess %p %s %p\n", userdata, sess, id, private);
}

static int
davVerifyCert(void *userdata, int failures, const ne_ssl_certificate *cert)
	/*@*/
{
    const char *hostname = userdata;

if (_dav_debug < 0)
fprintf(stderr, "*** davVerifyCert(%p,%d,%p) %s\n", userdata, failures, cert, hostname);

    return 0;	/* HACK: trust all server certificates. */
}

static int davConnect(urlinfo u)
	/*@globals errno, internalState @*/
	/*@modifies u, errno, internalState @*/
{
    const char * path = NULL;
    int rc;

    /* HACK: hkp:// has no steenkin' options */
    if (!(u->urltype == URL_IS_HTTP || u->urltype == URL_IS_HTTPS))
	return 0;

    /* HACK: where should server capabilities be read? */
    (void) urlPath(u->url, &path);
    if (path == NULL || *path == '\0')
	path = "/";

#ifdef NOTYET	/* XXX too many new directories while recursing. */
    /* Repeat OPTIONS for new directories. */
    if (path != NULL && path[strlen(path)-1] == '/')
	u->allow &= ~RPMURL_SERVER_OPTIONSDONE;
#endif
    /* Have options been run? */
    if (u->allow & RPMURL_SERVER_OPTIONSDONE)
	return 0;

    u->allow &= ~(RPMURL_SERVER_HASDAVCLASS1 |
		  RPMURL_SERVER_HASDAVCLASS2 |
		  RPMURL_SERVER_HASDAVEXEC);

    /* HACK: perhaps capture Allow: tag, look for PUT permitted. */
    /* XXX [hdr] Allow: GET,HEAD,POST,OPTIONS,TRACE */
    rc = ne_options(u->sess, path, u->capabilities);
    switch (rc) {
    case NE_OK:
	u->allow |= RPMURL_SERVER_OPTIONSDONE;
    {	ne_server_capabilities *cap = u->capabilities;
	if (cap->dav_class1)
	    u->allow |= RPMURL_SERVER_HASDAVCLASS1;
	else
	    u->allow &= ~RPMURL_SERVER_HASDAVCLASS1;
	if (cap->dav_class2)
	    u->allow |= RPMURL_SERVER_HASDAVCLASS2;
	else
	    u->allow &= ~RPMURL_SERVER_HASDAVCLASS2;
	if (cap->dav_executable)
	    u->allow |= RPMURL_SERVER_HASDAVEXEC;
	else
	    u->allow &= ~RPMURL_SERVER_HASDAVEXEC;
    }	break;
    case NE_ERROR:
	/* HACK: "501 Not Implemented" if OPTIONS not permitted. */
	if (!strncmp("501 ", ne_get_error(u->sess), sizeof("501 ")-1)) {
	    u->allow |= RPMURL_SERVER_OPTIONSDONE;
	    rc = NE_OK;
	    break;
	}
	/* HACK: "301 Moved Permanently" on empty subdir. */
	if (!strncmp("301 ", ne_get_error(u->sess), sizeof("301 ")-1))
	    break;
#ifdef	HACK	/* XXX need davHEAD changes here? */
	/* HACK: "302 Found" if URI is missing pesky trailing '/'. */
	if (!strncmp("302 ", ne_get_error(u->sess), sizeof("302 ")-1)) {
	    char * t;
	    if ((t = strchr(u->url, '\0')) != NULL)
		*t = '/';
	    break;
	}
#endif
	errno = EIO;		/* HACK: more precise errno. */
	goto bottom;
    case NE_LOOKUP:
	errno = ENOENT;		/* HACK: errno same as non-existent path. */
	goto bottom;
    case NE_CONNECT:		/* HACK: errno set already? */
    default:
bottom:
/*@-evalorderuncon@*/
if (_dav_debug)
fprintf(stderr, "*** Connect to %s:%d failed(%d):\n\t%s\n",
		   u->host, u->port, rc, ne_get_error(u->sess));
/*@=evalorderuncon@*/
	break;
    }

    /* HACK: sensitive to error returns? */
    u->httpVersion = (ne_version_pre_http11(u->sess) ? 0 : 1);

    return rc;
}

static int davInit(const char * url, urlinfo * uret)
	/*@globals internalState @*/
	/*@modifies *uret, internalState @*/
{
    urlinfo u = NULL;
    int rc = 0;

/*@-globs@*/	/* FIX: h_errno annoyance. */
    if (urlSplit(url, &u))
	return -1;	/* XXX error returns needed. */
/*@=globs@*/

    if (u->url != NULL && u->sess == NULL)
    switch (u->urltype) {
    default:
	assert(u->urltype != u->urltype);
	/*@notreached@*/ break;
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    case URL_IS_HKP:
      {	ne_server_capabilities * capabilities;

	/* HACK: oneshots should be done Somewhere Else Instead. */
/*@-noeffect@*/
	rc = ((_dav_debug < 0) ? NE_DBG_HTTP : 0);
	ne_debug_init(stderr, rc);		/* XXX oneshot? */
/*@=noeffect@*/
	rc = ne_sock_init();			/* XXX oneshot? */

	u->lockstore = ne_lockstore_create();	/* XXX oneshot? */

	u->capabilities = capabilities = xcalloc(1, sizeof(*capabilities));
	u->sess = ne_session_create(u->scheme, u->host, u->port);

	ne_lockstore_register(u->lockstore, u->sess);

	if (u->proxyh != NULL)
	    ne_session_proxy(u->sess, u->proxyh, u->proxyp);

#if 0
	{   const ne_inet_addr ** addrs;
	    unsigned int n;
	    ne_set_addrlist(u->sess, addrs, n);
	}
#endif

	ne_set_progress(u->sess, davProgress, u);
#if WITH_NEON_MIN_VERSION >= 0x002700
	ne_set_notifier(u->sess, davNotify, u);
#else
	ne_set_status(u->sess, davNotify, u);
#endif

#if WITH_NEON_MIN_VERSION >= 0x002600
	ne_set_session_flag(u->sess, NE_SESSFLAG_PERSIST, rpmioHttpPersist);
#else
	ne_set_persist(u->sess, rpmioHttpPersist);
#endif
	ne_set_read_timeout(u->sess, rpmioHttpReadTimeoutSecs);
	ne_set_useragent(u->sess,
	    (rpmioHttpUserAgent ? rpmioHttpUserAgent : _rpmioHttpUserAgent));

	/* XXX check that neon is ssl enabled. */
	if (!strcasecmp(u->scheme, "https"))
	    ne_ssl_set_verify(u->sess, davVerifyCert, (char *)u->host);

	ne_set_session_private(u->sess, "urlinfo", u);

	ne_hook_destroy_session(u->sess, davDestroySession, u);

	ne_hook_create_request(u->sess, davCreateRequest, u);
	ne_hook_pre_send(u->sess, davPreSend, u);
	ne_hook_post_send(u->sess, davPostSend, u);
	ne_hook_destroy_request(u->sess, davDestroyRequest, u);

	/* HACK: where should server capabilities be read? */
	rc = davConnect(u);
	if (rc)
	    goto exit;
      }	break;
    }

exit:
if (_dav_debug < 0)
fprintf(stderr, "<-- %s(%s) u->url %s\n", __FUNCTION__, url, u->url);
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
/*@dependent@*/
    struct fetch_resource_s *next;
    char *uri;
/*@unused@*/
    char *displayname;
    enum fetch_rtype_e type;
    size_t size;
    time_t modtime;
    int is_executable;
    int is_vcr;    /* Is version resource. 0: no vcr, 1 checkin 2 checkout */
    char *error_reason; /* error string returned for this resource */
    int error_status; /* error status returned for this resource */
};

/*@null@*/
static void *fetch_destroy_item(/*@only@*/ struct fetch_resource_s *res)
	/*@modifies res @*/
{
    ne_free(res->uri);
    ne_free(res->error_reason);
    res = _free(res);
    return NULL;
}

#ifdef	NOTUSED
/*@null@*/
static void *fetch_destroy_list(/*@only@*/ struct fetch_resource_s *res)
	/*@modifies res @*/
{
    struct fetch_resource_s *next;
    for (; res != NULL; res = next) {
	next = res->next;
	res = fetch_destroy_item(res);
    }
    return NULL;
}
#endif

#if WITH_NEON_MIN_VERSION >= 0x002600
static void *fetch_create_item(/*@unused@*/ void *userdata, /*@unused@*/ const ne_uri *uri)
#else
static void *fetch_create_item(/*@unused@*/ void *userdata, /*@unused@*/ const char *uri)
#endif
        /*@*/
{
    struct fetch_resource_s * res = ne_calloc(sizeof(*res));
    return res;
}

/* =============================================================== */

/*@-nullassign -readonlytrans@*/
/*@unchecked@*/ /*@observer@*/
static const ne_propname fetch_props[] = {
    { "DAV:", "getcontentlength" },
    { "DAV:", "getlastmodified" },
    { "http://apache.org/dav/props/", "executable" },
    { "DAV:", "resourcetype" },
    { "DAV:", "checked-in" },
    { "DAV:", "checked-out" },
    { NULL, NULL }
};
/*@=nullassign =readonlytrans@*/

#define ELM_resourcetype (NE_PROPS_STATE_TOP + 1)
#define ELM_collection (NE_PROPS_STATE_TOP + 2)

/*@-readonlytrans@*/
/*@unchecked@*/ /*@observer@*/
static const struct ne_xml_idmap fetch_idmap[] = {
    { "DAV:", "resourcetype", ELM_resourcetype },
    { "DAV:", "collection", ELM_collection }
};
/*@=readonlytrans@*/

static int fetch_startelm(void *userdata, int parent,
		const char *nspace, const char *name,
		/*@unused@*/ const char **atts)
	/*@*/
{
    ne_propfind_handler *pfh = userdata;
    struct fetch_resource_s *r = ne_propfind_current_private(pfh);
/*@-sizeoftype@*/
    int state = ne_xml_mapid(fetch_idmap, NE_XML_MAPLEN(fetch_idmap),
                             nspace, name);
/*@=sizeoftype@*/

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
	/*@*/
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

#if WITH_NEON_MIN_VERSION >= 0x002600
static void fetch_results(void *userdata, const ne_uri *uarg,
		    const ne_prop_result_set *set)
#else
static void fetch_results(void *userdata, void *uarg,
		    const ne_prop_result_set *set)
#endif
	/*@*/
{
    rpmavx avx = userdata;
    struct fetch_resource_s *current, *previous, *newres;
    const char *clength, *modtime, *isexec;
    const char *checkin, *checkout;
    const ne_status *status = NULL;
    const char * path = NULL;

#if WITH_NEON_MIN_VERSION >= 0x002600
    const ne_uri * uri = uarg;
    (void) urlPath(uri->path, &path);
#else
    const char * uri = uarg;
    (void) urlPath(uri, &path);
#endif
    if (path == NULL)
	return;

    newres = ne_propset_private(set);

if (_dav_debug < 0)
fprintf(stderr, "==> %s in uri %s\n", path, avx->uri);

    if (ne_path_compare(avx->uri, path) == 0) {
	/* This is the target URI */
if (_dav_debug < 0)
fprintf(stderr, "==> %s skipping target resource.\n", path);
	/* Free the private structure. */
/*@-dependenttrans -exposetrans@*/
	free(newres);
/*@=dependenttrans =exposetrans@*/
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
    for (current = *avx->resrock, previous = NULL; current != NULL;
	previous = current, current = current->next)
    {
	if (fetch_compare(current, newres) >= 0) {
	    break;
	}
    }
    if (previous) {
	previous->next = newres;
    } else {
/*@-dependenttrans @*/
        *(struct fetch_resource_s **)avx->resrock = newres;
/*@=dependenttrans @*/
    }
    newres->next = current;
}

static int davFetch(const urlinfo u, rpmavx avx)
	/*@globals internalState @*/
	/*@modifies avx, internalState @*/
{
    const char * path = NULL;
    int depth = 1;					/* XXX passed arg? */
    struct fetch_resource_s * resitem = NULL;
    ne_propfind_handler *pfh;
    struct fetch_resource_s *current, *next;
    mode_t st_mode;
    int rc = 0;
    int xx;

    (void) urlPath(u->url, &path);
    pfh = ne_propfind_create(u->sess, avx->uri, depth);

    /* HACK: need to set RPMURL_SERVER_HASRANGE in u->allow here. */

    avx->resrock = (void **) &resitem;

    ne_xml_push_handler(ne_propfind_get_parser(pfh),
                        fetch_startelm, NULL, NULL, pfh);

    ne_propfind_set_private(pfh, fetch_create_item, NULL);

    rc = ne_propfind_named(pfh, fetch_props, fetch_results, avx);

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
		current = fetch_destroy_item(current);
		continue;
	    }
	    se--;
	}
	s = se;
	while (s > current->uri && s[-1] != '/')
	    s--;

	val = ne_strndup(s, (se - s));

/*@-nullpass@*/
	val = ne_path_unescape(val);
/*@=nullpass@*/

	switch (current->type) {
	case resr_normal:
	    st_mode = S_IFREG;
	    /*@switchbreak@*/ break;
	case resr_collection:
	    st_mode = S_IFDIR;
	    /*@switchbreak@*/ break;
	case resr_reference:
	case resr_error:
	default:
	    st_mode = 0;
	    /*@switchbreak@*/ break;
	}

	xx = rpmavxAdd(avx, val, st_mode, current->size, current->modtime);
	ne_free(val);

	current = fetch_destroy_item(current);
    }
    avx->resrock = NULL;	/* HACK: avoid leaving stack reference. */
    /* HACK realloc to truncate modes/sizes/mtimes */

    return rc;
}

/* HACK davHEAD() should be rewritten to use davReq/davResp w callbacks. */
static int davHEAD(urlinfo u, struct stat *st) 
	/*@modifies u, *st @*/
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
    st->st_size = -1;
    st->st_atime = -1;
    st->st_mtime = -1;
    st->st_ctime = -1;

    req = ne_request_create(u->sess, "HEAD", u->url);
    if (rpmioHttpAccept != NULL)
	ne_add_request_header(req, "Accept", rpmioHttpAccept);

    /* XXX if !defined(HAVE_NEON_NE_GET_RESPONSE_HEADER) handlers? */

    rc = ne_request_dispatch(req);
    status = ne_get_status(req);

/* XXX somewhere else instead? */
if (_dav_debug) {
fprintf(stderr, "HTTP request sent, awaiting response... %d %s\n", status->code, status->reason_phrase);
}

    switch (rc) {
    default:
	goto exit;
	/*@notreached@*/ break;
    case NE_OK:
	if (status->klass != 2)		/* XXX is this necessary? */
	    rc = NE_ERROR;
	break;
    }

#if defined(HAVE_NEON_NE_GET_RESPONSE_HEADER)
    htag = "ETag";
    value = ne_get_response_header(req, htag); 
    if (value) {
	/* inode-size-mtime */
	u->etag = _free(u->etag);
	u->etag = xstrdup(value);
    }

    /* XXX limit to 3xx returns? */
    htag = "Location";
    value = ne_get_response_header(req, htag); 
    if (value) {
	u->location = _free(u->location);
	u->location = xstrdup(value);
    }

    htag = "Content-Length";
    value = ne_get_response_header(req, htag); 
    if (value) {
/* XXX should wget's "... (1.2K)..." be added? */
if (_dav_debug && ++printing)
fprintf(stderr, "Length: %s", value);

/*@-unrecog@*/	/* XXX LCLINT needs stdlib.h update. */
	st->st_size = strtoll(value, NULL, 10);
/*@=unrecog@*/
	st->st_blocks = (st->st_size + 511)/512;
    }

    htag = "Content-Type";
    value = ne_get_response_header(req, htag); 
    if (value) {
if (_dav_debug && printing)
fprintf(stderr, " [%s]", value);
	if (!strcmp(value, "text/html")
	 || !strcmp(value, "application/xhtml+xml"))
	    st->st_blksize = 2 * 1024;
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
#endif

exit:
    ne_request_destroy(req);
    return rc;
}

static int my_result(const char * msg, int ret, /*@null@*/ FILE * fp)
	/*@modifies *fp @*/
{
    /* HACK: don't print unless debugging. */
    if (_dav_debug >= 0)
	return ret;
    if (fp == NULL)
	fp = stderr;
    if (msg != NULL)
	fprintf(fp, "*** %s: ", msg);

    /* HACK FTPERR_NE_FOO == -NE_FOO error impedance match */
#ifdef	HACK
    fprintf(fp, "%s: %s\n", ftpStrerror(-ret), ne_get_error(sess));
#else
    fprintf(fp, "%s\n", ftpStrerror(-ret));
#endif
    return ret;
}

/* XXX TODO move to rpmhtml.c */
/**
 */
typedef struct rpmhtml_s * rpmhtml;

int _html_debug = 0;

/**
 */
struct rpmhtml_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
/*@kept@*/
    rpmavx avx;
    ne_request *req;

/*@observer@*/
    const char * pattern;
/*@relnull@*/
    miRE mires;
    int nmires;

    char * buf;
    size_t nbuf;
/*@null@*/
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
/*@unused@*/ /*@null@*/
rpmhtml htmlUnlink (/*@null@*/ rpmhtml html)
        /*@modifies html @*/;
#define htmlUnlink(_html)  \
    ((rpmhtml)rpmioUnlinkPoolItem((rpmioItem)(_html), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a html wrapper instance.
 * @param html		html wrapper
 * @return		new html wrapper reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmhtml htmlLink (/*@null@*/ rpmhtml html)
        /*@modifies html @*/;
#define htmlLink(_html)  \
    ((rpmhtml)rpmioLinkPoolItem((rpmioItem)(_html), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a html wrapper instance.
 * @param html		html wrapper
 * @return		NULL on last derefernce
 */
/*@null@*/
rpmhtml htmlFree (/*@null@*/ rpmhtml html)
        /*@modifies html @*/;
#define htmlFree(_html)  \
    ((rpmhtml)rpmioFreePoolItem((rpmioItem)(_html), __FUNCTION__, __FILE__, __LINE__))

/**
 */
static void htmlFini(void * _html)
	/*@globals fileSystem @*/
	/*@modifies *_html, fileSystem @*/
{
    rpmhtml html = _html;

    if (html->req != NULL) {
	ne_request_destroy(html->req);
	html->req = NULL;
    }
    html->buf = _free(html->buf);
    html->nbuf = 0;
    html->avx = NULL;
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _htmlPool = NULL;

static rpmhtml htmlGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _htmlPool, fileSystem @*/
	/*@modifies pool, _htmlPool, fileSystem @*/
{
    rpmhtml html;

    if (_htmlPool == NULL) {
	_htmlPool = rpmioNewPool("html", sizeof(*html), -1, _html_debug,
			NULL, NULL, htmlFini);
	pool = _htmlPool;
    }
    return (rpmhtml) rpmioGetPool(pool, sizeof(*html));
}

/**
 */
static
rpmhtml htmlNew(urlinfo u, /*@kept@*/ rpmavx avx) 
	/*@*/
{
    rpmhtml html = htmlGetPool(_htmlPool);
    html->avx = avx;
    html->nbuf = BUFSIZ;	/* XXX larger buffer? */
    html->buf = xmalloc(html->nbuf + 1 + 1);
    html->req = ne_request_create(u->sess, "GET", u->url);
    return htmlLink(html);
}

/**
 */
static ssize_t htmlFill(rpmhtml html)
	/*@modifies html @*/
{
    char * b = html->buf;
    size_t nb = html->nbuf;
    ssize_t rc;

    if (html->b != NULL && html->nb > 0 && html->b > html->buf) {
	memmove(html->buf, html->b, html->nb);
	b += html->nb;
	nb -= html->nb;
    }
if (_dav_debug < 0)
fprintf(stderr, "--> htmlFill(%p) %p[%u]\n", html, b, (unsigned)nb);

    /* XXX FIXME: "server awol" segfaults here. gud enuf atm ... */
    rc = ne_read_response_block(html->req, b, nb) ;
    if (rc > 0) {
	html->nb += rc;
	b += rc;
	nb -= rc;
    }
    html->b = html->buf;

if (_dav_debug < 0)
fprintf(stderr, "<-- htmlFill(%p) %p[%u] rc %d\n", html, b, (unsigned)nb, (int)rc);
    return rc;
}

/**
 * Convert hex to binary nibble.
 * @param c            hex character
 * @return             binary nibble
 */
static
unsigned char nibble(char c)
	/*@*/
{
    if (c >= '0' && c <= '9')
	return (unsigned char) (c - '0');
    if (c >= 'A' && c <= 'F')
	return (unsigned char)((int)(c - 'A') + 10);
    if (c >= 'a' && c <= 'f')
	return (unsigned char)((int)(c - 'a') + 10);
    return (unsigned char) '\0';
}

/*@observer@*/
static const char * hrefpat = "(?i)<a(?:\\s+[a-z][a-z0-9_]*(?:=(?:\"[^\"]*\"|\\S+))?)*?\\s+href=(?:\"([^\"]*)\"|(\\S+))";

/**
 */
static int htmlParse(rpmhtml html)
	/*@globals hrefpat, internalState @*/
	/*@modifies html, internalState @*/
{
    miRE mire;
    int noffsets = 3;
    int offsets[3];
    ssize_t nr = (html->b != NULL ? (ssize_t)html->nb : htmlFill(html));
    int rc = 0;
    int xx;

if (_dav_debug < 0)
fprintf(stderr, "--> htmlParse(%p) %p[%u]\n", html, html->buf, (unsigned)html->nbuf);
    html->pattern = hrefpat;
    xx = mireAppend(RPMMIRE_PCRE, 0, html->pattern, NULL, &html->mires, &html->nmires);
    mire = html->mires;

    xx = mireSetEOptions(mire, offsets, noffsets);

    while (html->nb > 0) {
	char * gbn, * href;
	const char * hbn, * lpath;
	char * f, * fe;
	char * g, * ge;
	size_t ng;
	char * h, * he;
	size_t nh;
	char * t;
	mode_t st_mode;
	int ut;

assert(html->b != NULL);
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
	    nh = (size_t)(he - h);
	    href = t = xmalloc(nh + 1 + 1);
	    while (h < he) {
		char c = *h++;
		switch (c) {
		default:
		    /*@switchbreak@*/ break;
		case '%':
		    if (isxdigit((int)h[0]) && isxdigit((int)h[1])) {
			c = (char) (nibble(h[0]) << 4) | nibble(h[1]);
			h += 2;
		    }
		    /*@switchbreak@*/ break;
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
		/*@switchbreak@*/ break;
	    case URL_IS_FTP:
	    case URL_IS_HTTPS:
	    case URL_IS_HTTP:
#ifdef	NOTYET	/* XXX rpmavx needs to save linktos first. */
		st_mode = S_IFLNK | 0755;
		/*@switchbreak@*/ break;
#endif
	    case URL_IS_PATH:
	    case URL_IS_DASH:
	    case URL_IS_HKP:
		href[0] = '\0';
		/*@switchbreak@*/ break;
	    }
	    if ((hbn = strrchr(href, '/')) != NULL)
		hbn++;
	    else
		hbn = href;
assert(hbn != NULL);

	    /* Parse the URI path. */
	    g = fe;
	    while (*g != '>')
		g++;
	    ge = ++g;
	    while (*ge != '<')
		ge++;
	    /* [g:ge) contains the URI basename. */
	    ng = (size_t)(ge - g);
	    gbn = t = xmalloc(ng + 1 + 1);
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
	    }

	    gbn = _free(gbn);
	    href = _free(href);

	    offsets[1] += (ge - fe);
	    html->b += offsets[1];
	    html->nb -= offsets[1];
	} else {
	    size_t nb = html->nb;
	    if (nr > 0) nb -= 128;	/* XXX overlap a bit if filling. */
	    html->b += nb;
	    html->nb -= nb;
	}

	if (nr > 0)
	    nr = htmlFill(html);
    }

    xx = mireSetEOptions(mire, NULL, 0);

    html->mires = mireFreeAll(html->mires, html->nmires);
    html->nmires = 0;

if (_dav_debug < 0)
fprintf(stderr, "<-- htmlParse(%p) rc %d\n", html, rc);
    return rc;
}

/* HACK htmlNLST() should be rewritten to use davReq/davResp w callbacks. */
/*@-mustmod@*/
static int htmlNLST(urlinfo u, rpmavx avx) 
	/*@globals hrefpat, internalState @*/
	/*@modifies avx, internalState @*/
{
    rpmhtml html = htmlNew(u, avx);
    int rc = 0;

    do {
	rc = ne_begin_request(html->req);
	rc = my_result("ne_begin_req(html->req)", rc, NULL);
	if (rc != NE_OK) goto exit;

	(void) htmlParse(html);		/* XXX error code needs handling. */

	rc = ne_end_request(html->req);
	rc = my_result("ne_end_req(html->req)", rc, NULL);
    } while (rc == NE_RETRY);

exit:
    html = htmlFree(html);
    return rc;
}
/*@=mustmod@*/

static int davNLST(rpmavx avx)
	/*@globals hrefpat, internalState @*/
	/*@modifies avx, internalState @*/
{
    urlinfo u = NULL;
const char * u_url = NULL;	/* XXX FIXME: urlFind should save current URI */
    int rc;
    int xx;

retry:
    rc = davInit(avx->uri, &u);
    if (rc || u == NULL)
	goto exit;

if (u_url == NULL) {		/* XXX FIXME: urlFind should save current URI */
u_url = u->url;
u->url = avx->uri;
}
    /*
     * Do PROPFIND through davFetch iff server supports.
     * Otherwise, do HEAD to get Content-length/ETag/Last-Modified,
     * followed by GET through htmlNLST() to find the contained href's.
     */
    if (u->allow & RPMURL_SERVER_HASDAV)
	rc = davFetch(u, avx);	/* use PROPFIND to get contentLength */
    else {
/*@-nullpass@*/	/* XXX annotate avx->st correctly */
	rc = davHEAD(u, avx->st);	/* use HEAD to get contentLength */
/*@=nullpass@*/
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
	if (!strncmp("301 ", ne_get_error(u->sess), sizeof("301 ")-1))
	    break;

	/* HACK: "302 Found" if URI is missing pesky trailing '/'. */
	if (!strncmp("302 ", ne_get_error(u->sess), sizeof("302 ")-1)) {
	    const char * path = NULL;
	    int ut = urlPath(u->url, &path);
	    size_t nb = strlen(path);
	    ut = ut;	/* XXX keep gcc happy */
	    if (u->location != NULL && !strncmp(path, u->location, nb)
	     && u->location[nb] == '/' && u->location[nb+1] == '\0')
	    {
		char * te = strchr(u->url, '\0');
		/* Append the pesky trailing '/'. */
		if (te != NULL && te[-1] != '/') {
		    /* XXX u->uri malloc'd w room for +1b */
		    *te++ = '/';
		    *te = '\0';
		    u->location = _free(u->location);
		    /* XXX retry here needed iff ContentLength:. */
if (u_url != NULL) {		/* XXX FIXME: urlFind should save current URI */
u->url = u_url;
u_url = NULL;
}
		    xx = davFree(u);
		    goto retry;
		    /*@notreached@*/ break;
		}
	    }
	}
	/*@fallthrough@*/
    default:
/*@-evalorderuncon@*/
if (_dav_debug)
fprintf(stderr, "*** Fetch from %s:%d failed:\n\t%s\n",
		   u->host, u->port, ne_get_error(u->sess));
/*@=evalorderuncon@*/
        break;
    }

exit:
if (u_url != NULL) {		/* XXX FIXME: urlFind should save current URI */
u->url = u_url;
u_url = NULL;
}
    xx = davFree(u);
    return rc;
}

/* =============================================================== */
/*@-mustmod@*/
static void davAcceptRanges(void * userdata, /*@null@*/ const char * value)
	/*@modifies userdata @*/
{
    urlinfo u = userdata;

    if (!(u != NULL && value != NULL)) return;
if (_dav_debug < 0)
fprintf(stderr, "*** u %p Accept-Ranges: %s\n", u, value);
    if (!strcmp(value, "bytes"))
	u->allow |= RPMURL_SERVER_HASRANGE;
    if (!strcmp(value, "none"))
	u->allow &= ~RPMURL_SERVER_HASRANGE;
}
/*@=mustmod@*/

#if !defined(HAVE_NEON_NE_GET_RESPONSE_HEADER)
static void davAllHeaders(void * userdata, const char * value)
{
    FD_t ctrl = userdata;

    if (!(ctrl != NULL && value != NULL)) return;
if (_dav_debug)
fprintf(stderr, "<- %s\n", value);
}
#endif

/*@-mustmod@*/
static void davContentLength(void * userdata, /*@null@*/ const char * value)
	/*@modifies userdata @*/
{
    FD_t ctrl = userdata;

    if (!(ctrl != NULL && value != NULL)) return;
if (_dav_debug < 0)
fprintf(stderr, "*** fd %p Content-Length: %s\n", ctrl, value);
/*@-unrecog@*/
   ctrl->contentLength = strtoll(value, NULL, 10);
/*@=unrecog@*/
}
/*@=mustmod@*/

/*@-mustmod@*/
static void davContentType(void * userdata, /*@null@*/ const char * value)
	/*@modifies userdata @*/
{
    FD_t ctrl = userdata;

    if (!(ctrl != NULL && value != NULL)) return;
if (_dav_debug < 0)
fprintf(stderr, "*** fd %p Content-Type: %s\n", ctrl, value);
   ctrl->contentType = _free(ctrl->contentType);
   ctrl->contentType = xstrdup(value);
}
/*@=mustmod@*/

/*@-mustmod@*/
static void davContentDisposition(void * userdata, /*@null@*/ const char * value)
	/*@modifies userdata @*/
{
    FD_t ctrl = userdata;

    if (!(ctrl != NULL && value != NULL)) return;
if (_dav_debug < 0)
fprintf(stderr, "*** fd %p Content-Disposition: %s\n", ctrl, value);
   ctrl->contentDisposition = _free(ctrl->contentDisposition);
   ctrl->contentDisposition = xstrdup(value);
}
/*@=mustmod@*/

/*@-mustmod@*/
static void davLastModified(void * userdata, /*@null@*/ const char * value)
	/*@modifies userdata @*/
{
    FD_t ctrl = userdata;

    if (!(ctrl != NULL && value != NULL)) return;
if (_dav_debug < 0)
fprintf(stderr, "*** fd %p Last-Modified: %s\n", ctrl, value);
/*@-unrecog@*/
   ctrl->lastModified = ne_httpdate_parse(value);
/*@=unrecog@*/
}
/*@=mustmod@*/

/*@-mustmod@*/
static void davConnection(void * userdata, /*@null@*/ const char * value)
	/*@modifies userdata @*/
{
    FD_t ctrl = userdata;

    if (!(ctrl != NULL && value != NULL)) return;
if (_dav_debug < 0)
fprintf(stderr, "*** fd %p Connection: %s\n", ctrl, value);
    if (!strcasecmp(value, "close"))
	ctrl->persist = 0;
    else if (!strcasecmp(value, "Keep-Alive"))
	ctrl->persist = 1;
}
/*@=mustmod@*/

/*@-mustmod@*/ /* HACK: stash error in *str. */
int davResp(urlinfo u, FD_t ctrl, /*@unused@*/ char *const * str)
{
    int rc = 0;

    rc = ne_begin_request(ctrl->req);
    rc = my_result("ne_begin_req(ctrl->req)", rc, NULL);

if (_dav_debug < 0)
fprintf(stderr, "*** davResp(%p,%p,%p) sess %p req %p rc %d\n", u, ctrl, str, u->sess, ctrl->req, rc);

    /* HACK FTPERR_NE_FOO == -NE_FOO error impedance match */
/*@-observertrans@*/
    if (rc)
	fdSetSyserrno(ctrl, errno, ftpStrerror(-rc));
/*@=observertrans@*/

    return rc;
}
/*@=mustmod@*/

int davReq(FD_t ctrl, const char * httpCmd, const char * httpArg)
{
    urlinfo u;
    int rc = 0;

assert(ctrl != NULL);
    u = ctrl->url;
    URLSANE(u);

if (_dav_debug < 0)
fprintf(stderr, "*** davReq(%p,%s,\"%s\") entry sess %p req %p\n", ctrl, httpCmd, (httpArg ? httpArg : ""), u->sess, ctrl->req);

    ctrl->persist = (u->httpVersion > 0 ? 1 : 0);
    ctrl = fdLink(ctrl, "open ctrl (davReq)");
assert(ctrl != NULL);

assert(u->sess != NULL);
    /* XXX reset disconnected handle to NULL. should never happen ... */
    if (ctrl->req == (void *)-1)
	ctrl->req = NULL;
/*@-nullderef@*/
assert(ctrl->req == NULL);
/*@=nullderef@*/
/*@-nullpass@*/
    ctrl->req = ne_request_create(u->sess, httpCmd, httpArg);
/*@=nullpass@*/
assert(ctrl->req != NULL);

    ne_set_request_private(ctrl->req, "fd", ctrl);

#if !defined(HAVE_NEON_NE_GET_RESPONSE_HEADER)
    ne_add_response_header_catcher(ctrl->req, davAllHeaders, ctrl);

    ne_add_response_header_handler(ctrl->req, "Content-Length",
		davContentLength, ctrl);
    ne_add_response_header_handler(ctrl->req, "Content-Type",
		davContentType, ctrl);
    ne_add_response_header_handler(ctrl->req, "Content-Disposition",
		davContentDisposition, ctrl);
    ne_add_response_header_handler(ctrl->req, "Last-Modified",
		davLastModified, ctrl);
    ne_add_response_header_handler(ctrl->req, "Connection",
		davConnection, ctrl);
#endif

    if (!strcmp(httpCmd, "PUT")) {
#if defined(HAVE_NEON_NE_SEND_REQUEST_CHUNK)
	ctrl->wr_chunked = 1;
	ne_add_request_header(ctrl->req, "Transfer-Encoding", "chunked");
	ne_set_request_chunked(ctrl->req, 1);
	/* HACK: no retries if/when chunking. */
	rc = davResp(u, ctrl, NULL);
#else
	rc = FTPERR_SERVER_IO_ERROR;
#endif
    } else {
	/* HACK: possible ETag: "inode-size-mtime" */
#if !defined(HAVE_NEON_NE_GET_RESPONSE_HEADER)
	ne_add_response_header_handler(ctrl->req, "Accept-Ranges",
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

/* XXX somwhere else instead? */
if (_dav_debug) {
    const ne_status *status = ne_get_status(ctrl->req);
fprintf(stderr, "HTTP request sent, awaiting response... %d %s\n", status->code, status->reason_phrase);
}

    if (rc)
	goto errxit;

if (_dav_debug < 0)
fprintf(stderr, "*** davReq(%p,%s,\"%s\") exit sess %p req %p rc %d\n", ctrl, httpCmd, (httpArg ? httpArg : ""), u->sess, ctrl->req, rc);

#if defined(HAVE_NEON_NE_GET_RESPONSE_HEADER)
    davContentLength(ctrl,
		ne_get_response_header(ctrl->req, "Content-Length"));
    davContentType(ctrl,
		ne_get_response_header(ctrl->req, "Content-Type"));
    davContentDisposition(ctrl,
		ne_get_response_header(ctrl->req, "Content-Disposition"));
    davLastModified(ctrl,
		ne_get_response_header(ctrl->req, "Last-Modified"));
    davConnection(ctrl,
		ne_get_response_header(ctrl->req, "Connection"));
    if (strcmp(httpCmd, "PUT"))
	davAcceptRanges(u,
		ne_get_response_header(ctrl->req, "Accept-Ranges"));
#endif

    ctrl = fdLink(ctrl, "open data (davReq)");
    return 0;

errxit:
/*@-observertrans@*/
    fdSetSyserrno(ctrl, errno, ftpStrerror(rc));
/*@=observertrans@*/

    /* HACK balance fd refs. ne_session_destroy to tear down non-keepalive? */
    ctrl = fdLink(ctrl, "error data (davReq)");

    return rc;
}

FD_t davOpen(const char * url, /*@unused@*/ int flags,
		/*@unused@*/ mode_t mode, /*@out@*/ urlinfo * uret)
{
    const char * path = NULL;
    urltype urlType = urlPath(url, &path);
    urlinfo u = NULL;
    FD_t fd = NULL;
    int rc;

#if 0	/* XXX makeTempFile() heartburn */
    assert(!(flags & O_RDWR));
#endif

if (_dav_debug < 0)
fprintf(stderr, "*** davOpen(%s,0x%x,0%o,%p)\n", url, flags, (unsigned)mode, uret);
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

    if (u->ctrl->url == NULL)
	fd = u->ctrl = fdLink(u->ctrl, "grab ctrl (davOpen persist ctrl)");
    else if (u->data->url == NULL)
	fd = u->data = fdLink(u->data, "grab ctrl (davOpen persist data)");
    else
	fd = fdNew("grab ctrl (davOpen)");

    if (fd) {
	fdSetOpen(fd, url, flags, mode);
	fdSetIo(fd, ufdio);

	fd->ftpFileDoneNeeded = 0;
	fd->rd_timeoutsecs = rpmioHttpReadTimeoutSecs;
	fd->contentLength = fd->bytesRemain = -1;
assert(urlType == URL_IS_HTTPS || urlType == URL_IS_HTTP || urlType == URL_IS_HKP);
	fd->urlType = urlType;
	fd->url = urlLink(u, "url (davOpen)");
	fd = fdLink(fd, "grab data (davOpen)");
    }

exit:
    if (uret)
	*uret = u;
    /*@-refcounttrans@*/
    return fd;
    /*@=refcounttrans@*/
}

/*@-mustmod@*/
ssize_t davRead(void * cookie, /*@out@*/ char * buf, size_t count)
{
    FD_t fd = cookie;
    ssize_t rc;

#if WITH_NEON_MIN_VERSION >= 0x002700
  { urlinfo u = NULL;
    u = urlLink(fd->url, "url (davRead)");
    if (u->info.status == ne_status_recving)
	rc = ne_read_response_block(fd->req, buf, count);
    else {
	/* If server has disconnected, then tear down the neon request. */
	if (u->info.status == ne_status_disconnected) {
	    int xx;
	    xx = ne_end_request(fd->req);
	    xx = my_result("davRead: ne_end_request(req)", xx, NULL);
	    ne_request_destroy(fd->req);
	    fd->req = (void *)-1;
	}
	errno = EIO;       /* XXX what to do? */
	rc = -1;
    }
    u = urlFree(u, "url (davRead)");
  }
#else
    rc = ne_read_response_block(fd->req, buf, count);
#endif

if (_dav_debug < 0) {
fprintf(stderr, "*** davRead(%p,%p,0x%x) rc 0x%x\n", cookie, buf, (unsigned)count, (unsigned)rc);
    }

    return rc;
}
/*@=mustmod@*/

ssize_t davWrite(void * cookie, const char * buf, size_t count)
{
#if !defined(NEONBLOWSCHUNKS) || defined(HAVE_NEON_NE_SEND_REQUEST_CHUNK) || defined(__LCLINT__)
    FD_t fd = cookie;
#endif
    ssize_t rc;
    int xx = -1;

#if !defined(NEONBLOWSCHUNKS)
    ne_session * sess;

assert(fd->req != NULL);
    sess = ne_get_session(fd->req);
assert(sess != NULL);

    /* HACK: include ne_private.h to access sess->socket for now. */
    xx = ne_sock_fullwrite(sess->socket, buf, count);
#else
#if defined(HAVE_NEON_NE_SEND_REQUEST_CHUNK) || defined(__LCLINT__)
assert(fd->req != NULL);
/*@-unrecog@*/
    xx = ne_send_request_chunk(fd->req, buf, count);
/*@=unrecog@*/
#else
    errno = EIO;       /* HACK */
    return -1;
#endif
#endif

    /* HACK: stupid error impedence matching. */
    rc = (xx == 0 ? (ssize_t)count : -1);

if (_dav_debug < 0)
fprintf(stderr, "*** davWrite(%p,%p,0x%x) rc 0x%x\n", cookie, buf, (unsigned)count, (unsigned)rc);

    return rc;
}

int davSeek(void * cookie, /*@unused@*/ _libio_pos_t pos, int whence)
{
if (_dav_debug < 0)
fprintf(stderr, "*** davSeek(%p,pos,%d)\n", cookie, whence);
    return -1;
}

/*@-mustmod@*/	/* HACK: fd->req is modified. */
int davClose(void * cookie)
{
/*@-onlytrans@*/
    FD_t fd = cookie;
/*@=onlytrans@*/
    int rc = 0;

assert(fd->req != NULL);
    if (fd->req != (void *)-1) {
	rc = ne_end_request(fd->req);
	rc = my_result("ne_end_request(req)", rc, NULL);

	ne_request_destroy(fd->req);
    }
    fd->req = NULL;

if (_dav_debug < 0)
fprintf(stderr, "*** davClose(%p) rc %d\n", fd, rc);
    return rc;
}
/*@=mustmod@*/

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

    rc = ne_mkcol(u->sess, path);

    if (rc) rc = -1;	/* XXX HACK: errno impedance match */

    /* XXX HACK: verify getrestype(remote) == resr_collection */

exit:
if (_dav_debug)
fprintf(stderr, "*** davMkdir(%s,0%o) rc %d\n", path, (unsigned)mode, rc);
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

    rc = ne_delete(u->sess, path);

    if (rc) rc = -1;	/* XXX HACK: errno impedance match */

exit:
if (_dav_debug)
fprintf(stderr, "*** davRmdir(%s) rc %d\n", path, rc);
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

    rc = ne_move(u->sess, overwrite, src, dst);

    if (rc) rc = -1;	/* XXX HACK: errno impedance match */

exit:
if (_dav_debug)
fprintf(stderr, "*** davRename(%s,%s) rc %d\n", oldpath, newpath, rc);
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

    rc = ne_delete(u->sess, src);

exit:
    if (rc) rc = -1;	/* XXX HACK: errno impedance match */

if (_dav_debug)
fprintf(stderr, "*** davUnlink(%s) rc %d\n", path, rc);
    return rc;
}

#ifdef	NOTYET
static int davChdir(const char * path)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    return davCommand("CWD", path, NULL);
}
#endif	/* NOTYET */

/* =============================================================== */

static const char * statstr(const struct stat * st,
		/*@returned@*/ /*@out@*/ char * buf)
	/*@modifies *buf @*/
{
    sprintf(buf,
	"*** dev %x ino %x mode %0o nlink %d uid %d gid %d rdev %x size %x\n",
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

int davStat(const char * path, /*@out@*/ struct stat *st)
	/*@globals hrefpat, fileSystem, internalState @*/
	/*@modifies *st, fileSystem, internalState @*/
{
    rpmavx avx = NULL;
    char buf[1024];
    int rc = -1;

if (_dav_debug < 0)
fprintf(stderr, "--> davStat(%s)\n", path);
    if (path == NULL || *path == '\0') {
	errno = ENOENT;
	goto exit;
    }
    avx = rpmavxNew(path, st);
    if (avx == NULL) {
	errno = ENOENT;		/* Note: avx is NULL iff urlSplit() fails. */
	goto exit;
    }
    rc = davNLST(avx);
    if (rc) {
/* HACK: errno = ??? */
	goto exit;
    }

    if (st->st_mode == 0)
	st->st_mode = (avx->ac > 1 ? S_IFDIR : S_IFREG);
    st->st_size = (avx->sizes ? avx->sizes[0] : (size_t)st->st_size);
    st->st_mtime = (avx->mtimes ? avx->mtimes[0] : st->st_mtime);
    st->st_atime = st->st_ctime = st->st_mtime;	/* HACK */
    if (S_ISDIR(st->st_mode)) {
	st->st_nlink = 2;
	st->st_mode |= 0755;
    } else
    if (S_ISREG(st->st_mode)) {
	st->st_nlink = 1;
	st->st_mode |= 0644;
    }

    /* XXX Fts(3) needs/uses st_ino. */
    /* Hash the path to generate a st_ino analogue. */
    if (st->st_ino == 0)
	st->st_ino = hashFunctionString(0, path, 0);

exit:
if (_dav_debug < 0)
fprintf(stderr, "<-- davStat(%s) rc %d\n%s", path, rc, statstr(st, buf));
    avx = rpmavxFree(avx);
    return rc;
}

int davLstat(const char * path, /*@out@*/ struct stat *st)
	/*@globals hrefpat, fileSystem, internalState @*/
	/*@modifies *st, fileSystem, internalState @*/
{
    rpmavx avx = NULL;
    char buf[1024];
    int rc = -1;

    if (path == NULL || *path == '\0') {
	errno = ENOENT;
	goto exit;
    }
    avx = rpmavxNew(path, st);
    if (avx == NULL) {
	errno = ENOENT;		/* Note: avx is NULL iff urlSplit() fails. */
	goto exit;
    }
    rc = davNLST(avx);
    if (rc) {
/* HACK: errno = ??? */
	goto exit;
    }

    if (st->st_mode == 0)
	st->st_mode = (avx->ac > 1 ? S_IFDIR : S_IFREG);
    st->st_size = (avx->sizes ? avx->sizes[0] : (size_t)st->st_size);
    st->st_mtime = (avx->mtimes ? avx->mtimes[0] : st->st_mtime);
    st->st_atime = st->st_ctime = st->st_mtime;	/* HACK */
    if (S_ISDIR(st->st_mode)) {
	st->st_nlink = 2;
	st->st_mode |= 0755;
    } else
    if (S_ISREG(st->st_mode)) {
	st->st_nlink = 1;
	st->st_mode |= 0644;
    }

    /* XXX fts(3) needs/uses st_ino. */
    /* Hash the path to generate a st_ino analogue. */
    if (st->st_ino == 0)
	st->st_ino = hashFunctionString(0, path, 0);

if (_dav_debug < 0)
fprintf(stderr, "*** davLstat(%s) rc %d\n%s\n", path, rc, statstr(st, buf));
exit:
    avx = rpmavxFree(avx);
    return rc;
}

#ifdef	NOTYET
static int davReadlink(const char * path, /*@out@*/ char * buf, size_t bufsiz)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies *buf, fileSystem, internalState @*/
{
    int rc;
    rc = davNLST(path, DO_FTP_READLINK, NULL, buf, bufsiz);
if (_dav_debug < 0)
fprintf(stderr, "*** davReadlink(%s) rc %d\n", path, rc);
    return rc;
}
#endif	/* NOTYET */

#endif /* WITH_NEON */

/* =============================================================== */
/*@unchecked@*/
int avmagicdir = 0x3607113;

#ifndef WITH_NEON
/*@-nullstate@*/        /* FIX: u->{ctrl,data}->url undef after XurlLink. */
FD_t httpOpen(const char * url, /*@unused@*/ int flags,
                /*@unused@*/ mode_t mode, /*@out@*/ urlinfo * uret)
        /*@globals internalState @*/
        /*@modifies *uret, internalState @*/
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

    if (u->ctrl->url == NULL)
        fd = fdLink(u->ctrl, "grab ctrl (httpOpen persist ctrl)");
    else if (u->data->url == NULL)
        fd = fdLink(u->data, "grab ctrl (httpOpen persist data)");
    else
        fd = fdNew("grab ctrl (httpOpen)");

    if (fd) {
        fdSetIo(fd, ufdio);
        fd->ftpFileDoneNeeded = 0;
	fd->rd_timeoutsecs = rpmioHttpReadTimeoutSecs;
        fd->contentLength = fd->bytesRemain = -1;
        fd->url = urlLink(u, "url (httpOpen)");
        fd = fdLink(fd, "grab data (httpOpen)");
        fd->urlType = URL_IS_HTTP;
    }

exit:
    if (uret)
        *uret = u;
    /*@-refcounttrans@*/
    return fd;
    /*@=refcounttrans@*/
}
/*@=nullstate@*/
#endif

#ifdef WITH_NEON
/* =============================================================== */
int davClosedir(/*@only@*/ DIR * dir)
{
    return avClosedir(dir);
}

struct dirent * davReaddir(DIR * dir)
{
    return avReaddir(dir);
}

DIR * davOpendir(const char * path)
	/*@globals hrefpat @*/
{
    AVDIR avdir = NULL;
    rpmavx avx = NULL;
    struct stat sb, *st = &sb; /* XXX HACK: davHEAD needs avx->st. */
    const char * uri = NULL;
    int rc;

if (_dav_debug < 0)
fprintf(stderr, "*** davOpendir(%s)\n", path);

    if (path == NULL || *path == '\0') {
	errno = ENOENT;
	goto exit;
    }

    /* Note: all Opendir(3) URI's need pesky trailing '/' */
/*@-globs -mods@*/
    if (path[strlen(path)-1] != '/')
	uri = rpmExpand(path, "/", NULL);
    else
	uri = xstrdup(path);
/*@=globs =mods@*/

    /* Load DAV collection into argv. */
    /* XXX HACK: davHEAD needs avx->st. */
    avx = rpmavxNew(uri, st);
    if (avx == NULL) {
	errno = ENOENT;		/* Note: avx is NULL iff urlSplit() fails. */
	goto exit;
    }

    rc = davNLST(avx);
    if (rc) {
/* HACK: errno = ??? */
	goto exit;
    } else
	avdir = (AVDIR) avOpendir(uri, avx->av, avx->modes);

exit:
    uri = _free(uri);
    avx = rpmavxFree(avx);
/*@-kepttrans@*/
    return (DIR *) avdir;
/*@=kepttrans@*/
}
/*@=modfilesys@*/

/*@-mustmod@*/
char * davRealpath(const char * path, char * resolved_path)
{
assert(resolved_path == NULL);	/* XXX no POSIXly broken realpath(3) here. */
    /* XXX TODO: handle redirects. For now, just dupe the path. */
    return xstrdup(path);
}
/*@=mustmod@*/

#endif /* WITH_NEON */

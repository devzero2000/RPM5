/** \ingroup rpmio
 * \file rpmio/url.c
 */

#include "system.h"

#include <netinet/in.h>

#include <rpmmacro.h>
#include <rpmcb.h>
#include <rpmio_internal.h>
#ifdef WITH_NEON
#include <rpmdav.h>
#endif

#include "debug.h"

/*@access FD_t@*/		/* XXX compared with NULL */
/*@access urlinfo@*/

#ifndef	IPPORT_FTP
#define	IPPORT_FTP	21
#endif
#ifndef	IPPORT_HTTP
#define	IPPORT_HTTP	80
#endif
#ifndef	IPPORT_HTTPS
#define	IPPORT_HTTPS	443
#endif
#ifndef	IPPORT_PGPKEYSERVER
#define	IPPORT_PGPKEYSERVER	11371
#endif

/**
 */
/*@-redecl@*/
int (*urlNotify) (const urlinfo u, unsigned status)
        /*@*/;
/*@=redecl@*/

/**
 */
/*@unchecked@*/ /*@null@*/
void * urlNotifyArg;

/**
 */
/*@unchecked@*/
int _url_iobuf_size = RPMURL_IOBUF_SIZE;

/**
 */
/*@unchecked@*/
int _url_debug = 0;

#define	URLDBG(_f, _m, _x)	if ((_url_debug | (_f)) & (_m)) fprintf _x

#define URLDBGIO(_f, _x)	URLDBG((_f), RPMURL_DEBUG_IO, _x)
#define URLDBGREFS(_f, _x)	URLDBG((_f), RPMURL_DEBUG_REFS, _x)

/**
 */
/*@unchecked@*/
/*@only@*/ /*@null@*/
urlinfo *_url_cache = NULL;

static void urlFini(void * _u)
	/*@globals fileSystem, internalState @*/
	/*@modifies _u, fileSystem, internalState @*/
{
    urlinfo u =_u;
    int xx;

    if (u->ctrl) {
#ifndef	NOTYET
	void * fp = fdGetFp(u->ctrl);
	if (fp) {
	    fdPush(u->ctrl, fpio, fp, -1);   /* Push fpio onto stack */
	    xx = Fclose(u->ctrl);
	} else if (fdFileno(u->ctrl) >= 0)
	    xx = fdio->close(u->ctrl);
#else
	xx = Fclose(u->ctrl);
#endif

/*@-usereleased@*/
	u->ctrl = (FD_t)rpmioFreePoolItem((rpmioItem)u->ctrl, "persist ctrl (urlFree)", __FILE__, __LINE__);
	if (u->ctrl)
	    fprintf(stderr, _("warning: u %p ctrl %p nrefs != 0 (%s %s)\n"),
			u, u->ctrl, (u->host ? u->host : ""),
			(u->scheme ? u->scheme : ""));
/*@=usereleased@*/
    }
    if (u->data) {
#ifndef	NOTYET
	void * fp = fdGetFp(u->data);
	if (fp) {
	    fdPush(u->data, fpio, fp, -1);   /* Push fpio onto stack */
	    xx = Fclose(u->data);
	} else if (fdFileno(u->data) >= 0)
	    xx = fdio->close(u->data);
#else
	xx = Fclose(u->ctrl);
#endif

/*@-usereleased@*/
	u->data = (FD_t)rpmioFreePoolItem((rpmioItem)u->data, "persist data (urlFree)", __FILE__, __LINE__);
	if (u->data)
	    fprintf(stderr, _("warning: u %p data %p nrefs != 0 (%s %s)\n"),
			u, u->data, (u->host ? u->host : ""),
			(u->scheme ? u->scheme : ""));
/*@=usereleased@*/
    }
#ifdef WITH_NEON
    xx = davFree(u);
#endif
    u->etag = _free(u->etag);
    u->location = _free(u->location);
    u->rop = _free(u->rop);
    u->sop = _free(u->sop);
    u->top = _free(u->top);
    u->buf = _free(u->buf);
    u->url = _free(u->url);
    u->scheme = _free((void *)u->scheme);
    u->user = _free((void *)u->user);
    u->password = _free((void *)u->password);
    u->host = _free((void *)u->host);
    u->portstr = _free((void *)u->portstr);
    u->query = _free(u->query);
    u->fragment = _free(u->fragment);
    u->proxyu = _free((void *)u->proxyu);
    u->proxyh = _free((void *)u->proxyh);
}

/**
 */
/*@unchecked@*/
int _url_count = 0;

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _urlPool;

static urlinfo urlGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _urlPool, fileSystem @*/
	/*@modifies pool, _urlPool, fileSystem @*/
{
    urlinfo u;

    if (_urlPool == NULL) {
	_urlPool = rpmioNewPool("u", sizeof(*u), -1, _url_debug,
			NULL, NULL, urlFini);
	pool = _urlPool;
    }
    return (urlinfo) rpmioGetPool(pool, sizeof(*u));
}

urlinfo XurlNew(const char *msg, const char *fn, unsigned ln)
{
    urlinfo u = urlGetPool(_urlPool);

    u->proxyp = -1;
    u->port = -1;
    u->urltype = URL_IS_UNKNOWN;
    u->ctrl = NULL;
    u->data = NULL;
    u->location = NULL;
    u->etag = NULL;
    u->notify = urlNotify;
/*@-assignexpose@*/
    u->arg = urlNotifyArg;
/*@=assignexpose@*/
    u->rop = xcalloc(1, sizeof(*u->rop));
    u->sop = xcalloc(1, sizeof(*u->sop));
    u->top = xcalloc(1, sizeof(*u->top));
    u->bufAlloced = 0;
    u->buf = NULL;
    u->allow = RPMURL_SERVER_HASRANGE;
    u->httpVersion = 0;
    u->magic = URLMAGIC;
    return (urlinfo) rpmioLinkPoolItem((rpmioItem)u, msg, fn, ln);
}

void urlFreeCache(void)
{
    if (_url_cache) {
	int i;
	for (i = 0; i < _url_count; i++) {
	    if (_url_cache[i] == NULL) continue;
	    _url_cache[i] = urlFree(_url_cache[i], "_url_cache");
	    if (_url_cache[i] == NULL)
		continue;
	    yarnPossess(_url_cache[i]->_item.use);
	    fprintf(stderr,
		_("warning: _url_cache[%d] %p nrefs(%ld) != 1 (%s %s)\n"),
		i, _url_cache[i], yarnPeekLock(_url_cache[i]->_item.use),
		(_url_cache[i]->host ? _url_cache[i]->host : ""),
		(_url_cache[i]->scheme ? _url_cache[i]->scheme : ""));
	    yarnRelease(_url_cache[i]->_item.use);
	}
    }
    _url_cache = _free(_url_cache);
    _url_count = 0;
}

static int urlStrcmp(/*@null@*/ const char * str1, /*@null@*/ const char * str2)
	/*@*/
{
    if (str1)
	if (str2)
	    return strcmp(str1, str2);
    if (str1 != str2)
	return -1;
    return 0;
}

/*@-mods@*/
static void urlFind(/*@null@*/ /*@in@*/ /*@out@*/ urlinfo * uret, int mustAsk)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies *uret, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    urlinfo u;
    int ucx;
    int i = 0;

    if (uret == NULL)
	return;

    u = *uret;
    URLSANE(u);

    ucx = -1;
    for (i = 0; i < _url_count; i++) {
	urlinfo ou = NULL;
	if (_url_cache == NULL || (ou = _url_cache[i]) == NULL) {
	    if (ucx < 0)
		ucx = i;
	    continue;
	}

	/* Check for cache-miss condition. A cache miss is
	 *    a) both items are not NULL and don't compare.
	 *    b) either of the items is not NULL.
	 */
	if (urlStrcmp(u->scheme, ou->scheme))
	    continue;
    	if (urlStrcmp(u->host, ou->host))
	    continue;
	if (urlStrcmp(u->user, ou->user))
	    continue;
	if (urlStrcmp(u->portstr, ou->portstr))
	    continue;
	break;	/* Found item in cache */
    }

    if (i == _url_count) {
	if (ucx < 0) {
	    ucx = _url_count++;
	    _url_cache = xrealloc(_url_cache, sizeof(*_url_cache) * _url_count);
	}
	if (_url_cache)		/* XXX always true */
	    _url_cache[ucx] = urlLink(u, "_url_cache (miss)");
	u = urlFree(u, "urlSplit (urlFind miss)");
    } else {
	ucx = i;
	u = urlFree(u, "urlSplit (urlFind hit)");
    }

    /* This URL is now cached. */

    if (_url_cache)		/* XXX always true */
	u = urlLink(_url_cache[ucx], "_url_cache");
    *uret = u;
    /*@-usereleased@*/
    u = urlFree(u, "_url_cache (urlFind)");
    /*@=usereleased@*/
assert(u != NULL);

    /* Zap proxy host and port in case they have been reset */
    u->proxyp = -1;
    u->proxyh = _free(u->proxyh);

    /* Perform one-time FTP initialization */
    if (u->urltype == URL_IS_FTP) {

	if (mustAsk || (u->user != NULL && u->password == NULL)) {
	    const char * host = (u->host ? u->host : "");
	    const char * user = (u->user ? u->user : "");
	    char * prompt;
	    prompt = alloca(strlen(host) + strlen(user) + 256);
	    sprintf(prompt, _("Password for %s@%s: "), user, host);
	    u->password = _free(u->password);
/*@-dependenttrans -moduncon @*/
	    u->password = Getpass(prompt);
/*@=dependenttrans =moduncon @*/
	    if (u->password)
		u->password = xstrdup(u->password);
	}

	if (u->proxyh == NULL) {
	    const char *proxy = rpmExpand("%{_ftpproxy}", NULL);
	    if (proxy && *proxy != '%') {
/*@observer@*/
		const char * host = (u->host ? u->host : "");
		const char *uu = (u->user ? u->user : "anonymous");
		char *nu = xmalloc(strlen(uu) + sizeof("@") + strlen(host));
		(void) stpcpy( stpcpy( stpcpy(nu, uu), "@"), host);
		u->proxyu = nu;
		u->proxyh = xstrdup(proxy);
	    }
	    proxy = _free(proxy);
	}

	if (u->proxyp < 0) {
	    const char *proxy = rpmExpand("%{_ftpport}", NULL);
	    if (proxy && *proxy != '%') {
		char *end = NULL;
		int port = strtol(proxy, &end, 0);
		if (!(end && *end == '\0')) {
		    fprintf(stderr, _("error: %sport must be a number\n"),
			(u->scheme ? u->scheme : ""));
		    return;
		}
		u->proxyp = port;
	    }
	    proxy = _free(proxy);
	}
    }

    /* Perform one-time HTTP initialization */
    if (u->urltype == URL_IS_HTTP || u->urltype == URL_IS_HTTPS || u->urltype == URL_IS_HKP) {

	if (u->proxyh == NULL) {
	    const char *proxy = rpmExpand("%{_httpproxy}", NULL);
	    if (proxy && *proxy != '%')
		u->proxyh = xstrdup(proxy);
	    proxy = _free(proxy);
	}

	if (u->proxyp < 0) {
	    const char *proxy = rpmExpand("%{_httpport}", NULL);
	    if (proxy && *proxy != '%') {
		char *end;
		int port = strtol(proxy, &end, 0);
		if (!(end && *end == '\0')) {
		    fprintf(stderr, _("error: %sport must be a number\n"),
			(u->scheme ? u->scheme : ""));
		    return;
		}
		u->proxyp = port;
	    }
	    proxy = _free(proxy);
	}

    }

    return;
}
/*@=mods@*/

/**
 */
/*@observer@*/ /*@unchecked@*/
static struct urlstring {
/*@observer@*/ /*@null@*/
    const char * leadin;
    urltype	ret;
} urlstrings[] = {
    { "file://",	URL_IS_PATH },
    { "ftp://",		URL_IS_FTP },
    { "hkp://",		URL_IS_HKP },
    { "http://",	URL_IS_HTTP },
    { "https://",	URL_IS_HTTPS },
    { "-",		URL_IS_DASH },
    { NULL,		URL_IS_UNKNOWN }
};

urltype urlIsURL(const char * url)
{
    struct urlstring *us;

    if (url && *url) {
	for (us = urlstrings; us->leadin != NULL; us++) {
	    if (strncmp(url, us->leadin, strlen(us->leadin)))
		continue;
	    return us->ret;
	}
    }

    return URL_IS_UNKNOWN;
}

/* Return path portion of url (or pointer to NUL if url == NULL) */
urltype urlPath(const char * url, const char ** pathp)
{
    const char *path;
    int urltype;

    path = url;
    urltype = urlIsURL(url);
    switch (urltype) {
    case URL_IS_FTP:
	url += sizeof("ftp://") - 1;
	path = strchr(url, '/');
	if (path == NULL) path = url + strlen(url);
	break;
    case URL_IS_PATH:
	url += sizeof("file://") - 1;
	path = strchr(url, '/');
	if (path == NULL) path = url + strlen(url);
	break;
    case URL_IS_HKP:
	url += sizeof("hkp://") - 1;
	path = strchr(url, '/');
	if (path == NULL) path = url + strlen(url);
	break;
    case URL_IS_HTTP:
	url += sizeof("http://") - 1;
	path = strchr(url, '/');
	if (path == NULL) path = url + strlen(url);
	break;
    case URL_IS_HTTPS:
	url += sizeof("https://") - 1;
	path = strchr(url, '/');
	if (path == NULL) path = url + strlen(url);
	break;
    case URL_IS_UNKNOWN:
	if (path == NULL) path = "";
	break;
    case URL_IS_DASH:
	path = "";
	break;
    }
    if (pathp)
	/*@-observertrans@*/
	*pathp = path;
	/*@=observertrans@*/
    return urltype;
}

/**
 * Copy a URL, adding extra byte for the pesky trailing '/'.
 */
static const char * urlStrdup(const char * url)
	/*@*/
{
    size_t nb = strlen(url);
    char * t = xmalloc(nb + 1 + 1);
    const char * nurl = t;
    while (*url != '\0')
	*t++ = *url++;
    *t = '\0';
    return nurl;
}

/*
 * Split URL into components. The URL can look like
 *	scheme://user:password@host:port/path
  * or as in RFC2732 for IPv6 address
  *    service://user:password@[ip:v6:ad:dr:es:s]:port/path?query#fragment
 */
/*@-modfilesys@*/
int urlSplit(const char * url, urlinfo *uret)
{
    urlinfo u;
    char *myurl;
    char *s, *se, *f, *fe;

    if (uret == NULL)
	return -1;
    if ((u = urlNew("urlSplit")) == NULL)
	return -1;

    myurl = xstrdup(url);
    if ((se = strrchr(myurl, '#')) != NULL) {
	*se++ = '\0';
	u->fragment = xstrdup(se);
    }
    if ((se = strrchr(myurl, '?')) != NULL) {
	*se++ = '\0';
	u->query = xstrdup(se);
    }

    u->url = urlStrdup(myurl);		/* XXX +1 byte for pesky trailing '/' */
    u->urltype = urlIsURL(myurl);

    se = s = myurl;
    while (1) {
	/* Point to end of next item */
	while (*se && *se != '/') se++;
	/* Item was scheme. Save scheme and go for the rest ...*/
    	if (*se && (se != s) && se[-1] == ':' && se[0] == '/' && se[1] == '/') {
		se[-1] = '\0';
	    u->scheme = xstrdup(s);
	    se += 2;	/* skip over "//" */
	    s = se++;
	    continue;
	}
	
	/* Item was everything-but-path. Continue parse on rest */
	*se = '\0';
	break;
    }

    /* Look for ...@host... */
    fe = f = s;
    while (*fe && *fe != '@') fe++;
    if (*fe == '@') {
	s = fe + 1;
	*fe = '\0';
    	/* Look for user:password@host... */
	while (fe > f && *fe != ':') fe--;
	if (*fe == ':') {
	    *fe++ = '\0';
	    u->password = xstrdup(fe);
	}
	u->user = xstrdup(f);
    }

    /* Look for ...host:port or [v6addr]:port*/
    fe = f = s;
    if (strchr(fe, '[') && strchr(fe, ']')) {
	fe = strchr(f, ']');
	*f++ = '\0';
	*fe++ = '\0';
    }
assert(fe != NULL);	/* XXX can't happen */
    while (*fe && *fe != ':') fe++;
    if (*fe == ':') {
	*fe++ = '\0';
	u->portstr = xstrdup(fe);
	if (u->portstr != NULL && u->portstr[0] != '\0') {
	    char *end;
	    u->port = strtol(u->portstr, &end, 0);
	    if (!(end && *end == '\0')) {
		rpmlog(RPMLOG_ERR, _("url port must be a number\n"));
		myurl = _free(myurl);
		u = urlFree(u, "urlSplit (error #3)");
		return -1;
	    }
	}
    }
    u->host = xstrdup(f);

    if (u->port < 0 && u->scheme != NULL) {
	struct servent *serv;
/*@-multithreaded -moduncon @*/
	/* HACK hkp:// might lookup "pgpkeyserver" */
	serv = getservbyname(u->scheme, "tcp");
/*@=multithreaded =moduncon @*/
	if (serv != NULL)
	    u->port = (int) ntohs(serv->s_port);
	else if (u->urltype == URL_IS_FTP)
	    u->port = IPPORT_FTP;
	else if (u->urltype == URL_IS_HKP)
	    u->port = IPPORT_PGPKEYSERVER;
	else if (u->urltype == URL_IS_HTTP)
	    u->port = IPPORT_HTTP;
	else if (u->urltype == URL_IS_HTTPS)
	    u->port = IPPORT_HTTPS;
    }

    myurl = _free(myurl);
    if (uret) {
	*uret = u;
/*@-globs -mods @*/ /* FIX: rpmGlobalMacroContext not in <rpmlib.h> */
	urlFind(uret, 0);
/*@=globs =mods @*/
    }
    return 0;
}
/*@=modfilesys@*/

int urlGetFile(const char * url, const char * dest)
{
    int rc;
    FD_t sfd = NULL;
    FD_t tfd = NULL;
    const char * sfuPath = NULL;
    int urlType = urlPath(url, &sfuPath);
    char *result;

    if (*sfuPath == '\0')
	return FTPERR_UNKNOWN;

    if (dest == NULL) {
	if ((dest = strrchr(sfuPath, '/')) != NULL)
	    dest++;
	else
	    dest = sfuPath;
    }
    if (dest == NULL)
	return FTPERR_UNKNOWN;

/*@-globs -mods@*/	/* Avoid including <rpmmacro.h> everywhere for now */
    if (rpmExpandNumeric("%{?__urlgetfile:1}%{!?__urlgetfile:0}")) {
        result = rpmExpand("%{__urlgetfile ", url, " ", dest, "}", NULL);
        if (result != NULL && strcmp(result, "OK") == 0)
            rc = 0;
        else {
            rpmlog(RPMLOG_DEBUG, D_("failed to fetch URL %s via external command\n"), url);
            rc = FTPERR_UNKNOWN;
        }
        result = _free(result);
        goto exit;
    }
/*@=globs =mods@*/

    sfd = Fopen(url, "r.ufdio");
    if (sfd == NULL || Ferror(sfd)) {
	rpmlog(RPMLOG_DEBUG, D_("failed to open %s: %s\n"), url, Fstrerror(sfd));
	rc = FTPERR_UNKNOWN;
	goto exit;
    }

    /* XXX this can fail if directory in path does not exist. */
    tfd = Fopen(dest, "w");
if (_url_debug)
fprintf(stderr, "*** urlGetFile sfd %p %s tfd %p %s\n", sfd, url, (tfd ? tfd : NULL), dest);
    if (tfd == NULL || Ferror(tfd)) {
	rpmlog(RPMLOG_DEBUG, D_("failed to create %s: %s\n"), dest, Fstrerror(tfd));
	rc = FTPERR_UNKNOWN;
	goto exit;
    }

    switch (urlType) {
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    case URL_IS_HKP:
    case URL_IS_FTP:
    case URL_IS_PATH:
    case URL_IS_DASH:
    case URL_IS_UNKNOWN:
	if ((rc = ufdGetFile(sfd, tfd))) {
	    (void) Unlink(dest);
	    /* XXX FIXME: sfd possibly closed by copyData */
	    /*@-usereleased@*/ (void) Fclose(sfd) /*@=usereleased@*/ ;
	}
	sfd = NULL;	/* XXX Fclose(sfd) done by ufdGetFile */
	break;
    default:
	rc = FTPERR_UNKNOWN;
	break;
    }

exit:
    if (tfd)
	(void) Fclose(tfd);
    if (sfd)
	(void) Fclose(sfd);

    return rc;
}

#include "miscfn.h"

#include <fcntl.h>
#include <pwd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#if HAVE_ALLOCA_H
# include <alloca.h>
#endif

#include <netinet/in.h>
#include <netdb.h>

#ifndef IPPORT_FTP
# define IPPORT_FTP 21
#endif

#include "ftp.h"
#include "intl.h"
#include "messages.h"
#include "miscfn.h"
#include "rpmlib.h"
#include "url.h"

#ifndef	FREE
#define	FREE(_x)   { if ((_x) != NULL) { free((void *)(_x)); (_x) = NULL; } }
#endif

static void findUrlinfo(urlinfo **uret, int mustAsk);
static int urlConnect(const char * url, urlinfo **uret);

void freeUrlinfo(urlinfo *u)
{
    if (u->ftpControl >= 0)
	urlFinishedFd(u);
    FREE(u->service);
    FREE(u->user);
    FREE(u->password);
    FREE(u->host);
    FREE(u->portstr);
    FREE(u->path);

    FREE(u);
}

urlinfo *newUrlinfo(void)
{
    urlinfo *u;
    if ((u = malloc(sizeof(*u))) == NULL)
	return NULL;
    memset(u, 0, sizeof(*u));
    u->port = -1;
    u->ftpControl = -1;
    return u;
}

static void findUrlinfo(urlinfo **uret, int mustAsk)
{
    urlinfo *u;
    urlinfo **empty;
    static urlinfo **uCache = NULL;
    static int uCount = 0;
    int i;

    if (uret == NULL)
	return;

    u = *uret;

    empty = NULL;
    for (i = 0; i < uCount; i++) {
	urlinfo *ou;
	if ((ou = uCache[i]) == NULL) {
	    if (empty == NULL)
		empty = &uCache[i];
	    continue;
	}
	if (u->service && ou->service && strcmp(ou->service, u->service))
	    continue;
	if (u->host && ou->host && strcmp(ou->host, u->host))
	    continue;
	if (u->user && ou->user && strcmp(ou->user, u->user))
	    continue;
	if (u->password && ou->password && strcmp(ou->password, u->password))
	    continue;
	if (u->portstr && ou->portstr && strcmp(ou->portstr, u->portstr))
	    continue;
	break;
    }

    if (i == uCount) {
	if (empty == NULL) {
	    uCount++;
	    if (uCache)
		uCache = realloc(uCache, sizeof(*uCache) * uCount);
	    else
		uCache = malloc(sizeof(*uCache));
	    empty = &uCache[i];
	}
	*empty = u;
    } else {
	const char *up = uCache[i]->path;
	uCache[i]->path = u->path;
	u->path = up;
	freeUrlinfo(u);
    }

    *uret = u = uCache[i];

    if (!strcmp(u->service, "ftp")) {
	if (mustAsk || (u->user != NULL && u->password == NULL)) {
	    char * prompt;
	    FREE(u->password);
	    prompt = alloca(strlen(u->host) + strlen(u->user) + 40);
	    sprintf(prompt, _("Password for %s@%s: "), u->user, u->host);
	    u->password = strdup(getpass(prompt));
	}
    }
    return;
}

int urlSplit(const char * url, urlinfo **uret)
{
    urlinfo *u;
    char *myurl;
    char *s, *se, *f, *fe;

    if (uret == NULL)
	return -1;
    if ((u = newUrlinfo()) == NULL)
	return -1;

    if ((se = s = myurl = strdup(url)) == NULL) {
	freeUrlinfo(u);
	return -1;
    }
    do {
	while (*se && *se != '/') se++;
	if (*se == '\0') {
	    if (myurl) free(myurl);
	    freeUrlinfo(u);
	    return -1;
	}
    	if ((se != s) && se[-1] == ':' && se[0] == '/' && se[1] == '/') {
		se[-1] = '\0';
	    u->service = strdup(s);
	    se += 2;
	    s = se++;
	    continue;
	}
        u->path = strdup(se);
	*se = '\0';
	break;
    } while (1);

    fe = f = s;
    while (*fe && *fe != '@') fe++;
    if (*fe == '@') {
	s = fe + 1;
	*fe = '\0';
	while (fe > f && *fe != ':') fe--;
	if (*fe == ':') {
	    *fe++ = '\0';
	    u->password = strdup(fe);
	}
	u->user = strdup(f);
    }

    fe = f = s;
    while (*fe && *fe != ':') fe++;
    if (*fe == ':') {
	*fe++ = '\0';
	u->portstr = strdup(fe);
	if (u->portstr != NULL && u->portstr[0] != '\0') {
	    char *end;
	    u->port = strtol(u->portstr, &end, 0);
	    if (*end) {
		rpmMessage(RPMMESS_ERROR, _("url port must be a number\n"));
		if (myurl) free(myurl);
		freeUrlinfo(u);
		return -1;
	    }
	}
    }
    u->host = strdup(f);

    if (u->port < 0 && u->service != NULL) {
	struct servent *se;
	if ((se = getservbyname(u->service, "tcp")) != NULL)
	    u->port = ntohs(se->s_port);
	else if (!strcasecmp(u->service, "ftp"))
	    u->port = IPPORT_FTP;
	else if (!strcasecmp(u->service, "http"))
	    u->port = IPPORT_HTTP;
    }

    if (myurl) free(myurl);
    if (uret) {
	*uret = u;
	findUrlinfo(uret, 0);
    }
    return 0;
}

static int urlConnect(const char * url, urlinfo **uret)
{
    urlinfo *u;
   
    rpmMessage(RPMMESS_DEBUG, _("getting %s\n"), url);

    if (urlSplit(url, &u) < 0)
	return -1;

    if (u->ftpControl < 0) {
	char * proxy;
	char * portStr;
	char * endPtr;
	int port;

	rpmMessage(RPMMESS_DEBUG, _("logging into %s as %s, pw %s\n"),
		u->host,
		u->user ? u->user : "ftp", 
		u->password ? u->password : "(username)");

	proxy = rpmGetVar(RPMVAR_FTPPROXY);
	portStr = rpmGetVar(RPMVAR_FTPPORT);
	if (!portStr) {
	    port = -1;
	} else {
	    port = strtol(portStr, &endPtr, 0);
	    if (*endPtr) {
		rpmMessage(RPMMESS_ERROR, _("ftpport must be a number\n"));
		return -1;
	    }
 	}

	u->ftpControl = ftpOpen(u->host, u->user, u->password, proxy, port);
    }

    if (u->ftpControl < 0)
	return -1;

    if (uret)
	*uret = u;

    return 0;
}

int urlGetFd(const char * url, void **xx)
{
    urlinfo **uret = (urlinfo **)xx;
    urlinfo *u;
    int fd;
    int rc;

    if ((rc = urlConnect(url, &u)) < 0)
	return rc;

    fd = ftpGetFileDesc(u->ftpControl, u->path);

    if (fd < 0) {
	urlFinishedFd(u);
	return fd;
    }

    if (uret)
	*uret = u;

    return fd;
}

int urlAbortFd(void *x, int fd)
{
    urlinfo *u = (urlinfo *)x;
    if (u->ftpControl) {
	ftpAbort(u->ftpControl, fd);
    } else if (fd >= 0)
	close(fd);
    return 0;
}

int urlFinishedFd(void *x)
{
    urlinfo *u = (urlinfo *)x;
    if (u->ftpControl) {
	ftpClose(u->ftpControl);
	u->ftpControl = -1;
    }
    return 0;
}

int urlGetFile(const char * url, const char * dest)
{
    urlinfo *u;
    int rc;
    int fd;

    if ((rc = urlConnect(url, &u)) < 0)
	return rc;

    if ((fd = open(dest, (O_CREAT|O_WRONLY|O_TRUNC), 0600)) < 0) {
	rpmMessage(RPMMESS_DEBUG, _("failed to create %s\n"), dest);
	return FTPERR_UNKNOWN;
    }

    if ((rc = ftpGetFile(u->ftpControl, u->path, fd))) {
	urlFinishedFd(u);
	unlink(dest);
    }
    close(fd);

    return rc;
}

urltype urlIsURL(const char * url)
{
    if (!strncmp(url, "ftp://", 6))
	return URL_IS_FTP;
    return URL_IS_UNKNOWN;
}

#ifndef H_URL
#define H_URL

#include "ftp.h"

/* right now, only ftp type URL's are supported */
typedef enum {
    URL_IS_UNKNOWN	= 0,
    URL_IS_DASH		= 1,
    URL_IS_PATH		= 2,
    URL_IS_FTP		= 3,
    URL_IS_HTTP		= 4,
} urltype;

typedef struct urlinfo {
    const char *service;
    const char *user;
    const char *password;
    const char *host;
    const char *portstr;
    const char *path;
    int port;
    int ftpControl;
} urlinfo;

#ifndef IPPORT_HTTP
#define IPPORT_HTTP	80
#endif

#ifdef __cplusplus
extern "C" {
#endif

urltype	urlIsURL(const char * url);
int	urlSplit(const char *url, urlinfo **u);

int	urlGetFile(const char * url, const char * dest);

int urlGetFd(const char * url, void **xx);
int urlAbortFd(void *x, int fd);
int urlFinishedFd(void *x);

#ifdef __cplusplus
}
#endif

#endif

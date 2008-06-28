#include "system.h"

#include <rpmio.h>
#include <poptIO.h>

#include "debug.h"

#ifdef	NOTYET
#define	_CFB(n)	((1U << (n)) | 0x40000000)
#define CF_ISSET(_FLAG) ((wgetFlags & ((WGET_FLAGS_##_FLAG) & ~0x40000000)) != WGET_FLAGS_NONE)

enum wgetFlags_e {
    WGET_FLAGS_NONE		= 0,

    WGET_FLAGS_APPEND		= _CFB( 0), /*!< -a,--append ... */
    WGET_FLAGS_ASCII		= _CFB( 1), /*!< -B,--use-ascii ... */
    WGET_FLAGS_COMPRESSED	= _CFB( 2), /*!<    --compressed ... */
    WGET_FLAGS_CREATEDIRS	= _CFB( 3), /*!<    --create-dirs ... */
    WGET_FLAGS_CRLF		= _CFB( 4), /*!<    --crlf ... */
};

/*@unchecked@*/
static enum wgetFlags_e wgetFlags = WGET_FLAGS_NONE;

enum wgetAuth_e {
    WGET_AUTH_ANY		= 0,
    WGET_AUTH_BASIC		= 1,
    WGET_AUTH_DIGEST		= 2,
    WGET_AUTH_NTLM		= 3,
    WGET_AUTH_NEGOTIATE		= 4,
};

/*@unchecked@*/
static enum wgetAuth_e wgetAuth = WGET_AUTH_ANY;
/*@unchecked@*/
static enum wgetAuth_e wgetProxyAuth = WGET_AUTH_ANY;

static const char * _user_agent;
static const char * _cacert;
static const char * _capath;
static const char * _cert;
static const char * _cert_type;
static const char * _ciphers;
static const char * _cookie_jar;
static int _connect_timeout;
static long long _connect_at;

static int _max_time;
#endif	/* NOTYET */

/**
 */
static void rpmwgetArgCallback(poptContext con,
                /*@unused@*/ enum poptCallbackReason reason,
                const struct poptOption * opt, const char * arg,
                /*@unused@*/ void * data)
	/*@*/
{
    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
    default:
	fprintf(stderr, _("%s: Unknown option -%c\n"), __progname, opt->val);
	poptPrintUsage(con, stderr, 0);
	/*@-exitarg@*/ exit(2); /*@=exitarg@*/
	/*@notreached@*/ break;
    }
}

/*==============================================================*/

#define	POPTWGET_XXX	0
#if !defined(POPT_BIT_TOGGLE)
#define	POPT_BIT_TOGGLE	(POPT_ARG_VAL|POPT_ARGFLAG_XOR)
#endif

/*@unchecked@*/ /*@observer@*/
static struct poptOption wgetStartupTable[] = {
#ifdef	NOTYET
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmwgetArgCallback, 0, NULL, NULL },
/*@=type@*/
#endif	/* NOTYET */
  { "version", 'V', POPT_ARG_NONE,	NULL, 'V',
	N_("display the version of Wget and exit."), NULL },
#ifdef	NOTYET	/* XXX FIXME: lt-rpmwget: option table misconfigured (104) */
  { "help", 'h', POPT_ARG_NONE,	NULL, 'h',
	N_("print this help."), NULL },
#endif	/* NOTYET */
  { "background", 'b', POPT_ARG_NONE,	NULL, 'b',
	N_("go to background after startup."), NULL },
  { "execute", 'e', POPT_ARG_NONE,	NULL, 'e',
	N_("execute a `.wgetrc'-style command."), N_("COMMAND") },
  POPT_TABLEEND
};

/*@unchecked@*/ /*@observer@*/
static struct poptOption wgetLoggingTable[] = {
#ifdef	NOTYET
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmwgetArgCallback, 0, NULL, NULL },
/*@=type@*/
#endif	/* NOTYET */
  { "output-file", 'o', POPT_ARG_NONE,	NULL, 'o',
	N_("log messages to FILE."), N_("FILE") },
  { "append-output", 'a', POPT_ARG_NONE,	NULL, 'a',
	N_("append messages to FILE."), N_("FILE") },
  { "debug", 'd', POPT_ARG_NONE,	NULL, 'd',
	N_("print lots of debugging information."), NULL },
  { "quiet", 'q', POPT_ARG_NONE,	NULL, 'q',
	N_("quiet (no output)."), NULL },
  { "verbose", 'v', POPT_ARG_NONE,	NULL, 'v',
	N_("be verbose (this is the default)."), NULL },
  { "no-verbose", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("turn off verboseness, without being quiet."), NULL },
  { "nv", '\0', POPT_ARG_NONE|POPT_ARGFLAG_ONEDASH,	NULL, POPTWGET_XXX,
	N_("turn off verboseness, without being quiet."), NULL },
  { "input-file", 'i', POPT_ARG_NONE,	NULL, 'i',
	N_("download URLs found in FILE."), N_("FILE") },
  { "force-html", 'F', POPT_ARG_NONE,	NULL, 'F',
	N_("treat input file as HTML."), NULL },
  { "base", 'B', POPT_ARG_NONE,	NULL, 'B',
	N_("prepends URL to relative links in -F -i file."), N_("URL") },
  POPT_TABLEEND
};

/*@unchecked@*/ /*@observer@*/
static struct poptOption wgetDownloadTable[] = {
#ifdef	NOTYET
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmwgetArgCallback, 0, NULL, NULL },
/*@=type@*/
#endif	/* NOTYET */
  { "tries", 't', POPT_ARG_NONE,	NULL, 't',
	N_("set number of retries to NUMBER (0 unlimits)."), N_("NUMBER") },
  { "retry-connrefused", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("retry even if connection is refused."), NULL },
  { "output-document", 'O', POPT_ARG_NONE,	NULL, 'O',
	N_("write documents to FILE."), N_("FILE") },
  { "no-clobber", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("skip downloads that would download to existing files."), NULL },
  { "nc", '\0', POPT_ARG_NONE|POPT_ARGFLAG_ONEDASH,	NULL, POPTWGET_XXX,
	N_("skip downloads that would download to existing files."), NULL },
  { "continue", 'c', POPT_ARG_NONE,	NULL, 'c',
	N_("resume getting a partially-downloaded file."), NULL },
  { "progress", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("select progress gauge type."), N_("TYPE") },
  { "timestamping", 'N', POPT_ARG_NONE,	NULL, 'N',
	N_("don't re-retrieve files unless newer than local."), NULL },
  { "server-response", 'S', POPT_ARG_NONE,	NULL, 'S',
	N_("print server response."), NULL },
  { "spider", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("don't download anything."), NULL },
  { "timeout", 'T', POPT_ARG_NONE,	NULL, 'T',
	N_("set all timeout values to SECONDS."), N_("SECONDS") },
  { "dns-timeout", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("set the DNS lookup timeout to SECS."), N_("SECS") },
  { "connect-timeout", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("set the connect timeout to SECS."), N_("SECS") },
  { "read-timeout", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("set the read timeout to SECS."), N_("SECS") },
  { "wait", 'w', POPT_ARG_NONE,	NULL, 'w',
	N_("wait SECONDS between retrievals."), N_("SECONDS") },
  { "waitretry", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("wait 1..SECONDS between retries of a retrieval."), N_("SECONDS") },
  { "random-wait", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("wait from 0...2*WAIT secs between retrievals."), NULL },
  { "no-proxy", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("explicitly turn off proxy."), NULL },
  { "quota", 'Q', POPT_ARG_NONE,	NULL, 'Q',
	N_("set retrieval quota to NUMBER."), N_("NUMBER") },
  { "bind-address", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("bind to ADDRESS (hostname or IP) on local host."), N_("ADDRESS") },
  { "limit-rate", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("limit download rate to RATE."), N_("RATE") },
  { "no-dns-cache", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("disable caching DNS lookups."), NULL },
  { "restrict-file-names", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("restrict chars in file names to ones OS allows."), N_("OS") },
  { "ignore-case", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("ignore case when matching files/directories."), NULL },
  { "inet4-only", '4', POPT_ARG_NONE,	NULL, '4',
	N_("connect only to IPv4 addresses."), NULL },
  { "inet6-only", '6', POPT_ARG_NONE,	NULL, '6',
	N_("connect only to IPv6 addresses."), NULL },
  { "prefer-family", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("connect first to addresses of specified family, one of IPv6, IPv4, or none."), N_("FAMILY") },
  { "user", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("set both ftp and http user to USER."), N_("USER") },
  { "password", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("set both ftp and http password to PASS."), N_("PASS") },
  POPT_TABLEEND
};

/*@unchecked@*/ /*@observer@*/
static struct poptOption wgetDirectoriesTable[] = {
#ifdef	NOTYET
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmwgetArgCallback, 0, NULL, NULL },
/*@=type@*/
#endif	/* NOTYET */
  { "no-directories", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("don't create directories."), NULL },
  { "nd", '\0', POPT_ARG_NONE|POPT_ARGFLAG_ONEDASH,	NULL, POPTWGET_XXX,
	N_("don't create directories."), NULL },
  { "force-directories", 'x', POPT_ARG_NONE,	NULL, 'x',
	N_("force creation of directories."), NULL },
  { "no-host-directories", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("don't create host directories."), NULL },
  { "nH", '\0', POPT_ARG_NONE|POPT_ARGFLAG_ONEDASH,	NULL, POPTWGET_XXX,
	N_("don't create host directories."), NULL },
  { "protocol-directories", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("use protocol name in directories."), NULL },
  { "directory-prefix", 'P', POPT_ARG_NONE,	NULL, 'P',
	N_("save files to PREFIX/..."), N_("PREFIX") },
  { "cut-dirs", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("ignore NUMBER remote directory components."), N_("NUMBER") },
  POPT_TABLEEND
};

/*@unchecked@*/ /*@observer@*/
static struct poptOption wgetHTTPTable[] = {
#ifdef	NOTYET
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmwgetArgCallback, 0, NULL, NULL },
/*@=type@*/
#endif	/* NOTYET */
  { "http-user", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("set http user to USER."), N_("USER") },
  { "http-password", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("set http password to PASS."), N_("PASS") },
  { "no-cache", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("disallow server-cached data."), NULL },
  { "html-extension", 'E', POPT_ARG_NONE,	NULL, 'E',
	N_("save HTML documents with `.html' extension."), NULL },
  { "ignore-length", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("ignore `Content-Length' header field."), NULL },
  { "header", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("insert STRING among the headers."), N_("STRING") },
  { "max-redirect", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("maximum redirections allowed per page."), NULL },
  { "proxy-user", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("set USER as proxy username."), N_("USER") },
  { "proxy-password", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("set PASS as proxy password."), N_("PASS") },
  { "referer", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("include `Referer: URL' header in HTTP request."), N_("URL") },
  { "save-headers", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("save the HTTP headers to file."), NULL },
  { "user-agent", 'U', POPT_ARG_NONE,	NULL, 'U',
	N_("identify as AGENT instead of Wget/VERSION."), N_("AGENT") },
  { "no-http-keep-alive", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("disable HTTP keep-alive (persistent connections)."), NULL },
  { "no-cookies", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("don't use cookies."), NULL },
  { "load-cookies", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("load cookies from FILE before session."), N_("FILE") },
  { "save-cookies", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("save cookies to FILE after session."), N_("FILE") },
  { "keep-session-cookies", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("load and save session (non-permanent) cookies."), NULL },
  { "post-data", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("use the POST method; send STRING as the data."), N_("STRING") },
  { "post-file", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("use the POST method; send contents of FILE."), N_("FILE") },
  { "content-disposition", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("honor the Content-Disposition header when choosing local file names (EXPERIMENTAL)."), NULL },
  { "auth-no-challenge", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("Send Basic HTTP authentication information without first waiting for the server's challenge."), NULL },
  POPT_TABLEEND
};

/*@unchecked@*/ /*@observer@*/
static struct poptOption wgetHTTPSTable[] = {
#ifdef	NOTYET
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmwgetArgCallback, 0, NULL, NULL },
/*@=type@*/
#endif	/* NOTYET */
  { "secure-protocol", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("choose secure protocol, one of auto, SSLv2, SSLv3, and TLSv1."), N_("PR") },
  { "no-check-certificate", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("don't validate the server's certificate."), NULL },
  { "certificate", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("client certificate file."), N_("FILE") },
  { "certificate-type", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("client certificate type, PEM or DER."), N_("TYPE") },
  { "private-key", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("private key file."), N_("FILE") },
  { "private-key-type", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("private key type, PEM or DER."), N_("TYPE") },
  { "ca-certificate", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("file with the bundle of CA's."), N_("FILE") },
  { "ca-directory", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("directory where hash list of CA's is stored."), N_("DIR") },
  { "random-file", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("file with random data for seeding the SSL PRNG."), N_("FILE") },
  { "egd-file", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("file naming the EGD socket with random data."), N_("FILE") },
  POPT_TABLEEND
};

/*@unchecked@*/ /*@observer@*/
static struct poptOption wgetFTPTable[] = {
#ifdef	NOTYET
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmwgetArgCallback, 0, NULL, NULL },
/*@=type@*/
#endif	/* NOTYET */
  { "ftp-user", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("set ftp user to USER."), N_("USER") },
  { "ftp-password", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("set ftp password to PASS."), N_("PASS") },
  { "no-remove-listing", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("don't remove `.listing' files."), NULL },
  { "no-glob", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("turn off FTP file name globbing."), NULL },
  { "no-passive-ftp", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("disable the \"passive\" transfer mode."), NULL },
  { "retr-symlinks", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("when recursing, get linked-to files (not dir)."), NULL },
  { "preserve-permissions", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("preserve remote file permissions."), NULL },
  POPT_TABLEEND
};

/*@unchecked@*/ /*@observer@*/
static struct poptOption wgetRecursiveDownloadTable[] = {
#ifdef	NOTYET
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmwgetArgCallback, 0, NULL, NULL },
/*@=type@*/
#endif	/* NOTYET */
  { "recursive", 'r', POPT_ARG_NONE,	NULL, 'r',
	N_("specify recursive download."), NULL },
  { "level", 'l', POPT_ARG_NONE,	NULL, 'l',
	N_("maximum recursion depth (inf or 0 for infinite)."), N_("NUMBER") },
  { "delete-after", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("delete files locally after downloading them."), NULL },
  { "convert-links", 'k', POPT_ARG_NONE,	NULL, 'k',
	N_("make links in downloaded HTML point to local files."), NULL },
  { "backup-converted", 'K', POPT_ARG_NONE,	NULL, 'K',
	N_("before converting file X, back up as X.orig."), NULL },
  { "mirror", 'm', POPT_ARG_NONE,	NULL, 'm',
	N_("shortcut for -N -r -l inf --no-remove-listing."), NULL },
  { "page-requisites", 'p', POPT_ARG_NONE,	NULL, 'p',
	N_("get all images, etc. needed to display HTML page."), NULL },
  { "strict-comments", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("turn on strict (SGML) handling of HTML comments."), NULL },
  POPT_TABLEEND
};

/*@unchecked@*/ /*@observer@*/
static struct poptOption wgetRecursiveAcceptTable[] = {
#ifdef	NOTYET
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmwgetArgCallback, 0, NULL, NULL },
/*@=type@*/
#endif	/* NOTYET */
  { "accept", 'A', POPT_ARG_NONE,	NULL, 'A',
	N_("comma-separated list of accepted extensions."), N_("LIST") },
  { "reject", 'R', POPT_ARG_NONE,	NULL, 'R',
	N_("comma-separated list of rejected extensions."), N_("LIST") },
  { "domains", 'D', POPT_ARG_NONE,	NULL, 'D',
	N_("comma-separated list of accepted domains."), N_("LIST") },
  { "exclude-domains", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("comma-separated list of rejected domains."), N_("LIST") },
  { "follow-ftp", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("follow FTP links from HTML documents."), NULL },
  { "follow-tags", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("comma-separated list of followed HTML tags."), N_("LIST") },
  { "ignore-tags", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("comma-separated list of ignored HTML tags."), N_("LIST") },
  { "span-hosts", 'H', POPT_ARG_NONE,	NULL, 'H',
	N_("go to foreign hosts when recursive."), NULL },
  { "relative", 'L', POPT_ARG_NONE,	NULL, 'L',
	N_("follow relative links only."), NULL },
  { "include-directories", 'I', POPT_ARG_NONE,	NULL, 'I',
	N_("list of allowed directories."), N_("LIST") },
  { "exclude-directories", 'X', POPT_ARG_NONE,	NULL, 'X',
	N_("list of excluded directories."), N_("LIST") },
  { "no-parent", '\0', POPT_ARG_NONE,	NULL, POPTWGET_XXX,
	N_("don't ascend to the parent directory."), NULL },
  { "np", '\0', POPT_ARG_NONE|POPT_ARGFLAG_ONEDASH,	NULL, POPTWGET_XXX,
	N_("don't ascend to the parent directory."), NULL },
  POPT_TABLEEND
};

/*@unchecked@*/ /*@observer@*/
static struct poptOption optionsTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmwgetArgCallback, 0, NULL, NULL },
/*@=type@*/

/* XXX FIXME: this option should not be needed. */
  { "debug", 'd', POPT_ARG_NONE,	NULL, 'd',
	N_("print lots of debugging information."), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, wgetStartupTable, 0,
	N_("Startup:"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, wgetLoggingTable, 0,
	N_("Logging and input file:"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, wgetDownloadTable, 0,
	N_("Download:"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, wgetDirectoriesTable, 0,
	N_("Directories:"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, wgetHTTPTable, 0,
	N_("HTTP options:"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, wgetHTTPSTable, 0,
	N_("HTTPS (SSL/TLS) options:"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, wgetFTPTable, 0,
	N_("FTP options:"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, wgetRecursiveDownloadTable, 0,
	N_("Recursive download:"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, wgetRecursiveAcceptTable, 0,
	N_("Recursive accept/reject:"), NULL },

  POPT_AUTOALIAS
  POPT_AUTOHELP

  { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
	N_("\
Usage: rpmwget [OPTION]... [URL]...\n\
\n\
Mandatory arguments to long options are mandatory for short options too.\n\
\n\
Mail bug reports and suggestions to <rpm-devel@rpm5.org>.\n\
"), NULL },

  POPT_TABLEEND

};

int
main(int argc, char *argv[])
	/*@globals __assert_program_name,
		rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies __assert_program_name, _rpmrepo,
		rpmGlobalMacroContext, fileSystem, internalState @*/
{
    poptContext optCon = rpmioInit(argc, argv, optionsTable);
    const char ** av = NULL;
    int ac;
    int rc = 1;		/* assume failure. */
    int i;

/*@-observertrans -readonlytrans @*/
    __progname = "rpmgenpkglist";
/*@=observertrans =readonlytrans @*/

    av = poptGetArgs(optCon);
    if (av == NULL || av[0] == NULL) {
	poptPrintUsage(optCon, stderr, 0);
	goto exit;
    }
    ac = argvCount(av);

    if (av != NULL)
    for (i = 0; i < ac; i++) {
    }
    rc = 0;

exit:
    optCon = rpmioFini(optCon);

    return rc;
}

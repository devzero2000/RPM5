#include "system.h"

#include <rpmio.h>
#include <poptIO.h>

#include "debug.h"

#define _KFB(n) (1U << (n))
#define _WFB(n) (_KFB(n) | 0x40000000)

#define F_ISSET(_f, _F, _FLAG) (((_f) & ((_F##_FLAGS_##_FLAG) & ~0x40000000)) != _F##_FLAGS_NONE)

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
enum rpmioWCTLFlags_e {
    WCTL_FLAGS_NONE		= 0,
    WCTL_FLAGS_BACKGROUND	= _WFB( 0), /*!< -b,--background ... */
    WCTL_FLAGS_FORCEHTML	= _WFB( 1), /*!< -F,--force-html ... */

    WCTL_FLAGS_RECURSE		= _WFB( 2), /*!< -r,--recurse ... */
    WCTL_FLAGS_DELETEAFTER	= _WFB( 3), /*!<    --delete-after ... */
    WCTL_FLAGS_CONVERTLINKS	= _WFB( 4), /*!< -k,--convert-links ... */
    WCTL_FLAGS_BACKUP		= _WFB( 5), /*!< -K,--backup-converted ... */
/*XXX popt alias? */
    WCTL_FLAGS_MIRROR		= _WFB( 6), /*!< -m,--mirror ... */
    WCTL_FLAGS_PAGEREQUISITES	= _WFB( 7), /*!< -p,--page-requisites ... */
    WCTL_FLAGS_STRICTCOMMENTS	= _WFB( 8), /*!<    --strict-comments ... */

    WCTL_FLAGS_FOLLOWFTP	= _WFB( 9), /*!<    --follow-ftp ... */
    WCTL_FLAGS_SPANHOSTS	= _WFB(10), /*!<    --span-hosts ... */
    WCTL_FLAGS_RELATIVEONLY	= _WFB(11), /*!< -L,--relative ... */
    WCTL_FLAGS_NOPARENT		= _WFB(12), /*!< -np,--no-parent ... */
};
/*@unchecked@*/
static enum rpmioWCTLFlags_e rpmioWCTLFlags = WCTL_FLAGS_NONE;

/*@unchecked@*/
static const char * _wctl_execute_cmd;
/*@unchecked@*/
static const char * _wctl_output_file;
/*@unchecked@*/
static const char * _wctl_append_file;
/*@unchecked@*/
static int _wctl_debug;
/*@unchecked@*/
static int _wctl_quiet;
/*@unchecked@*/
static int _wctl_verbose = -1;
/*@unchecked@*/
static const char * _wctl_input_file;
/*@unchecked@*/
static const char * _wctl_base_prefix;

/*@unchecked@*/ /*@observer@*/
static struct poptOption rpmioWCTLStartupPoptTable[] = {
  { "version", 'V', POPT_ARG_NONE,	NULL, 'V',
	N_("display the version of Wget and exit."), NULL },
#ifdef	NOTYET	/* XXX FIXME: lt-rpmwget: option table misconfigured (104) */
  { "help", 'h', POPT_ARG_NONE,	NULL, 'h',
	N_("print this help."), NULL },
#endif	/* NOTYET */
  { "background", 'b', POPT_BIT_SET,	&rpmioWCTLFlags, WCTL_FLAGS_BACKGROUND,
	N_("go to background after startup."), NULL },
  { "execute", 'e', POPT_ARG_STRING,	&_wctl_execute_cmd, 0,
	N_("execute a `.wgetrc'-style command."), N_("COMMAND") },
  POPT_TABLEEND
};

/*@unchecked@*/ /*@observer@*/
static struct poptOption rpmioWCTLLoggingPoptTable[] = {
  { "output-file", 'o', POPT_ARG_STRING,	&_wctl_output_file, 0,
	N_("log messages to FILE."), N_("FILE") },
  { "append-output", 'a', POPT_ARG_STRING,	&_wctl_append_file, 0,
	N_("append messages to FILE."), N_("FILE") },
  { "debug", 'd', POPT_ARG_VAL,	&_wctl_debug, -1,
	N_("print lots of debugging information."), NULL },
  { "quiet", 'q', POPT_ARG_VAL,	&_wctl_quiet, -1,
	N_("quiet (no output)."), NULL },
  { "verbose", 'v', POPT_ARG_VAL,	&_wctl_verbose, -1,
	N_("be verbose (this is the default)."), NULL },
  { "no-verbose", '\0', POPT_ARG_VAL,	&_wctl_verbose, 0,
	N_("turn off verboseness, without being quiet."), NULL },
  { "nv", '\0', POPT_ARG_VAL|POPT_ARGFLAG_ONEDASH,	&_wctl_verbose, 0,
	N_("turn off verboseness, without being quiet."), NULL },
  { "input-file", 'i', POPT_ARG_STRING,	&_wctl_input_file, 0,
	N_("download URLs found in FILE."), N_("FILE") },
  { "force-html", 'F', POPT_BIT_SET,	&rpmioWCTLFlags, WCTL_FLAGS_FORCEHTML,
	N_("treat input file as HTML."), NULL },
  { "base", 'B', POPT_ARG_STRING,	&_wctl_base_prefix, 0,
	N_("prepends URL to relative links in -F -i file."), N_("URL") },
  POPT_TABLEEND
};

enum rpmioWDLFlags_e {
    WDL_FLAGS_NONE		= 0,
    WDL_FLAGS_RETRYCONN		= _WFB( 0), /*!<    --retry-connrefused ... */
    WDL_FLAGS_NOCLOBBER		= _WFB( 1), /*!<    --no-clobber ... */
    WDL_FLAGS_RESUME		= _WFB( 2), /*!<    --continue ... */
    WDL_FLAGS_PROGRESS		= _WFB( 3), /*!<    --progress ... */
    WDL_FLAGS_NEWERONLY		= _WFB( 4), /*!<    --timestamping ... */
    WDL_FLAGS_SERVERRESPONSE	= _WFB( 5), /*!<    --server-response ... */
    WDL_FLAGS_SPIDER		= _WFB( 6), /*!<    --spider ... */
    WDL_FLAGS_NOPROXY		= _WFB( 7), /*!<    --no-proxy ... */
    WDL_FLAGS_NODNSCACHE	= _WFB( 8), /*!<    --no-dns-cache ... */
    WDL_FLAGS_RESTRICTFILENAMES	= _WFB( 9), /*!<    --restrict-file-names ... */
    WDL_FLAGS_IGNORECASE	= _WFB(10), /*!<    --ignore-case ... */
    WDL_FLAGS_INET4		= _WFB(11), /*!<    --inet4-only ... */
    WDL_FLAGS_INET6		= _WFB(12), /*!<    --inet6-only ... */
};
/*@unchecked@*/
static enum rpmioWDLFlags_e rpmioWDLFlags = WDL_FLAGS_NONE;

/*@unchecked@*/
static int _wdl_tries;
/*@unchecked@*/
static int _wdl_timeout_secs;
/*@unchecked@*/
static int _wdl_dns_timeout_secs;
/*@unchecked@*/
static int _wdl_connect_timeout_secs;
/*@unchecked@*/
static int _wdl_quota;
/*@unchecked@*/
static int _wdl_read_timeout_secs;
/*@unchecked@*/
static int _wdl_wait_secs;
/*@unchecked@*/
static int _wdl_waitretry_secs;
/*@unchecked@*/
static int _wdl_randomwaitretry_secs;
/*@unchecked@*/
static int _wdl_limit_rate;
/*@unchecked@*/
static int _wdl_bind_address;

/*@unchecked@*/
static const char * _wdl_output_file;

/*@unchecked@*/
static const char * _wdl_prefer;
/*@unchecked@*/
static const char * _wdl_user;
/*@unchecked@*/
static const char * _wdl_password;
/*@unchecked@*/ /*@observer@*/

static struct poptOption rpmioWDLPoptTable[] = {
  { "tries", 't', POPT_ARG_INT,	&_wdl_tries, 0,
	N_("set number of retries to NUMBER (0 unlimits)."), N_("NUMBER") },
  { "retry-connrefused", '\0', POPT_BIT_SET,	&rpmioWDLFlags, WDL_FLAGS_RETRYCONN,
	N_("retry even if connection is refused."), NULL },
  { "output-document", 'O', POPT_ARG_STRING,	&_wdl_output_file, 0,
	N_("write documents to FILE."), N_("FILE") },
  { "no-clobber", '\0', POPT_BIT_SET,	&rpmioWDLFlags, WDL_FLAGS_NOCLOBBER,
	N_("skip downloads that would download to existing files."), NULL },
  { "nc", '\0', POPT_BIT_SET|POPT_ARGFLAG_ONEDASH,	&rpmioWDLFlags, WDL_FLAGS_NOCLOBBER,
	N_("skip downloads that would download to existing files."), NULL },
  { "continue", 'c', POPT_BIT_SET,	&rpmioWDLFlags, WDL_FLAGS_RESUME,
	N_("resume getting a partially-downloaded file."), NULL },
  { "progress", '\0', POPT_BIT_SET,	&rpmioWDLFlags, WDL_FLAGS_PROGRESS,
	N_("select progress gauge type."), N_("TYPE") },
  { "timestamping", 'N', POPT_BIT_SET,	&rpmioWDLFlags, WDL_FLAGS_NEWERONLY,
	N_("don't re-retrieve files unless newer than local."), NULL },
  { "server-response", 'S', POPT_BIT_SET,	&rpmioWDLFlags, WDL_FLAGS_SERVERRESPONSE,
	N_("print server response."), NULL },
  { "spider", '\0', POPT_BIT_SET,	&rpmioWDLFlags, WDL_FLAGS_SPIDER,
	N_("don't download anything."), NULL },
  { "timeout", 'T', POPT_ARG_INT,	&_wdl_timeout_secs, 0,
	N_("set all timeout values to SECONDS."), N_("SECONDS") },
  { "dns-timeout", '\0', POPT_ARG_INT,	&_wdl_dns_timeout_secs, 0,
	N_("set the DNS lookup timeout to SECS."), N_("SECONDS") },
  { "connect-timeout", '\0', POPT_ARG_INT,	&_wdl_connect_timeout_secs, 0,
	N_("set the connect timeout to SECS."), N_("SECONDS") },
  { "read-timeout", '\0', POPT_ARG_INT,	&_wdl_read_timeout_secs, 0,
	N_("set the read timeout to SECS."), N_("SECONDS") },
  { "wait", 'w', POPT_ARG_INT,	&_wdl_wait_secs, 0,
	N_("wait SECONDS between retrievals."), N_("SECONDS") },
/* XXX POPT_ARGFLAG_RANDOM? */
  { "waitretry", '\0', POPT_ARG_INT,	&_wdl_waitretry_secs, 0,
	N_("wait 1..SECONDS between retries of a retrieval."), N_("SECONDS") },
/* XXX POPT_ARGFLAG_RANDOM */
  { "random-wait", '\0', POPT_ARG_INT,	&_wdl_randomwaitretry_secs, 0,
	N_("wait from 0...2*WAIT secs between retrievals."), NULL },
  { "no-proxy", '\0', POPT_BIT_SET,	&rpmioWDLFlags, WDL_FLAGS_NOPROXY,
	N_("explicitly turn off proxy."), NULL },
  { "quota", 'Q', POPT_ARG_INT,	&_wdl_quota, 0,
	N_("set retrieval quota to NUMBER."), N_("NUMBER") },

/* XXX add IP addr parsing */
  { "bind-address", '\0', POPT_ARG_INT,	&_wdl_bind_address, 0,
	N_("bind to ADDRESS (hostname or IP) on local host."), N_("ADDRESS") },

/* XXX double? */
  { "limit-rate", '\0', POPT_ARG_INT,	&_wdl_limit_rate, 0,
	N_("limit download rate to RATE."), N_("RATE") },

  { "no-dns-cache", '\0', POPT_BIT_SET,	&rpmioWDLFlags, WDL_FLAGS_NODNSCACHE,
	N_("disable caching DNS lookups."), NULL },
  { "restrict-file-names", '\0', POPT_BIT_SET,	&rpmioWDLFlags, WDL_FLAGS_RESTRICTFILENAMES,
	N_("restrict chars in file names to ones OS allows."), N_("OS") },
  { "ignore-case", '\0', POPT_BIT_SET,	&rpmioWDLFlags, WDL_FLAGS_IGNORECASE,
	N_("ignore case when matching files/directories."), NULL },
  { "inet4-only", '4', POPT_BIT_SET,	&rpmioWDLFlags, WDL_FLAGS_INET4,
	N_("connect only to IPv4 addresses."), NULL },
  { "inet6-only", '6', POPT_BIT_SET,	&rpmioWDLFlags, WDL_FLAGS_INET6,
	N_("connect only to IPv6 addresses."), NULL },
  { "prefer-family", '\0', POPT_ARG_STRING,	&_wdl_prefer, 0,
	N_("connect first to addresses of specified family, one of IPv6, IPv4, or none."), N_("FAMILY") },
  { "user", '\0', POPT_ARG_STRING,	&_wdl_user, 0,
	N_("set both ftp and http user to USER."), N_("USER") },
  { "password", '\0', POPT_ARG_STRING,	&_wdl_password, 0,
	N_("set both ftp and http password to PASS."), N_("PASS") },
  POPT_TABLEEND
};

enum rpmioWDirFlags_e {
    WDIR_FLAGS_NONE		= 0,
    WDIR_FLAGS_NOCREATE		= _WFB( 0), /*!<    --no-directories ... */
    WDIR_FLAGS_FORCE		= _WFB( 1), /*!<    --force-directories ... */
    WDIR_FLAGS_NOHOST		= _WFB( 2), /*!<    --no-host-directories ... */
    WDIR_FLAGS_ADDPROTO		= _WFB( 3), /*!<    --protocol-directories ... */
};
/*@unchecked@*/
static enum rpmioWDirFlags_e rpmioWDirFlags = WDIR_FLAGS_NONE;

/*@unchecked@*/
static const char * _wdir_prefix;
/*@unchecked@*/
static int _wdir_cut;

/*@unchecked@*/ /*@observer@*/
static struct poptOption rpmioWDirPoptTable[] = {
  { "no-directories", '\0', POPT_BIT_SET,	&rpmioWDirFlags, WDIR_FLAGS_NOCREATE,
	N_("don't create directories."), NULL },
  { "nd", '\0', POPT_BIT_SET|POPT_ARGFLAG_ONEDASH,	&rpmioWDirFlags, WDIR_FLAGS_NOCREATE,
	N_("don't create directories."), NULL },
  { "force-directories", 'x', POPT_BIT_SET,	&rpmioWDirFlags, WDIR_FLAGS_FORCE,
	N_("force creation of directories."), NULL },
  { "no-host-directories", '\0', POPT_BIT_SET,	&rpmioWDirFlags, WDIR_FLAGS_NOHOST,
	N_("don't create host directories."), NULL },
  { "nH", '\0', POPT_BIT_SET|POPT_ARGFLAG_ONEDASH, &rpmioWDirFlags, WDIR_FLAGS_NOHOST,
	N_("don't create host directories."), NULL },
  { "protocol-directories", '\0', POPT_BIT_SET,	&rpmioWDirFlags, WDIR_FLAGS_ADDPROTO,
	N_("use protocol name in directories."), NULL },
  { "directory-prefix", 'P', POPT_ARG_STRING,	&_wdir_prefix, 0,
	N_("save files to PREFIX/..."), N_("PREFIX") },
  { "cut-dirs", '\0', POPT_ARG_INT,	&_wdir_cut, 0,
	N_("ignore NUMBER remote directory components."), N_("NUMBER") },
  POPT_TABLEEND
};

enum rpmioHttpFlags_e {
    HTTP_FLAGS_NONE		= 0,
    HTTP_FLAGS_NOCACHE		= _WFB( 0), /*!<    --no-cache ... */
    HTTP_FLAGS_IGNORELENGTH	= _WFB( 1), /*!<    --ignore-length ... */
    HTTP_FLAGS_NOKEEPALIVE	= _WFB( 2), /*!<    --no-http-keep-alive ... */
    HTTP_FLAGS_NOCOOKIES	= _WFB( 3), /*!<    --no-cookies ... */
    HTTP_FLAGS_KEEPCOOKIES	= _WFB( 4), /*!<    --keep-session-cookies ... */
    HTTP_FLAGS_CONTENTDISPOSITION = _WFB( 5), /*!<    --content-dispostion ... */
    HTTP_FLAGS_NOCHALLENGE	= _WFB( 6), /*!<    --auth-no-challenge ... */
    HTTP_FLAGS_SAVEHEADERS	= _WFB( 7), /*!<    --save-headers ... */
};
/*@unchecked@*/
static enum rpmioHttpFlags_e rpmioHttpFlags = HTTP_FLAGS_NONE;

/*@unchecked@*/
static const char * _http_user;
/*@unchecked@*/
static const char * _http_password;
/*@unchecked@*/
static const char * _http_ext = ".html";
/*@unchecked@*/
static const char * _http_header;
/*@unchecked@*/
static int _http_max_redirect;
/*@unchecked@*/
static const char * _http_proxy_user;
/*@unchecked@*/
static const char * _http_proxy_password;
/*@unchecked@*/
static const char * _http_referer;
/*@unchecked@*/
static const char * _http_user_agent;
/*@unchecked@*/
static const char * _http_load_cookies_file;
/*@unchecked@*/
static const char * _http_save_cookies_file;
/*@unchecked@*/
static const char * _http_post_data;
/*@unchecked@*/
static const char * _http_post_file;

/*@unchecked@*/ /*@observer@*/
static struct poptOption rpmioHttpPoptTable[] = {
  { "http-user", '\0', POPT_ARG_STRING,	&_http_user, 0,
	N_("set http user to USER."), N_("USER") },
  { "http-password", '\0', POPT_ARG_STRING,	&_http_password, 0,
	N_("set http password to PASS."), N_("PASS") },
  { "no-cache", '\0', POPT_BIT_SET,	&rpmioHttpFlags, HTTP_FLAGS_NOCACHE,
	N_("disallow server-cached data."), NULL },
  { "html-extension", 'E', POPT_ARG_STRING,	&_http_ext, 0,
	N_("save HTML documents with `.html' extension."), NULL },
  { "ignore-length", '\0', POPT_BIT_SET,&rpmioHttpFlags, HTTP_FLAGS_IGNORELENGTH,
	N_("ignore `Content-Length' header field."), NULL },
/* XXX ARGV */
  { "header", '\0', POPT_ARG_STRING,	&_http_header, 0,
	N_("insert STRING among the headers."), N_("STRING") },
  { "max-redirect", '\0', POPT_ARG_INT,	&_http_max_redirect, 0,
	N_("maximum redirections allowed per page."), N_("NUM") },
  { "proxy-user", '\0', POPT_ARG_STRING,	&_http_proxy_user, 0,
	N_("set USER as proxy username."), N_("USER") },
  { "proxy-password", '\0', POPT_ARG_STRING,	&_http_proxy_password, 0,
	N_("set PASS as proxy password."), N_("PASS") },
  { "referer", '\0', POPT_ARG_STRING,	&_http_referer, 0,
	N_("include `Referer: URL' header in HTTP request."), N_("URL") },
  { "save-headers", '\0', POPT_BIT_SET,	&rpmioHttpFlags, HTTP_FLAGS_SAVEHEADERS,
	N_("save the HTTP headers to file."), NULL },
  { "user-agent", 'U', POPT_ARG_STRING,	&_http_user_agent, 0,
	N_("identify as AGENT instead of Wget/VERSION."), N_("AGENT") },
  { "no-http-keep-alive", '\0', POPT_BIT_SET,	&rpmioHttpFlags, HTTP_FLAGS_NOKEEPALIVE,
	N_("disable HTTP keep-alive (persistent connections)."), NULL },
  { "no-cookies", '\0', POPT_BIT_SET,	&rpmioHttpFlags, HTTP_FLAGS_NOCOOKIES,
	N_("don't use cookies."), NULL },
  { "load-cookies", '\0', POPT_ARG_STRING,	&_http_load_cookies_file, 0,
	N_("load cookies from FILE before session."), N_("FILE") },
  { "save-cookies", '\0', POPT_ARG_STRING,	&_http_save_cookies_file, 0,
	N_("save cookies to FILE after session."), N_("FILE") },
  { "keep-session-cookies", '\0', POPT_BIT_SET,	&rpmioHttpFlags, HTTP_FLAGS_KEEPCOOKIES,
	N_("load and save session (non-permanent) cookies."), NULL },
  { "post-data", '\0', POPT_ARG_STRING,	&_http_post_data, 0,
	N_("use the POST method; send STRING as the data."), N_("STRING") },
  { "post-file", '\0', POPT_ARG_STRING,	&_http_post_file, 0,
	N_("use the POST method; send contents of FILE."), N_("FILE") },
  { "content-disposition", '\0', POPT_BIT_SET,	&rpmioHttpFlags, 0,
	N_("honor the Content-Disposition header when choosing local file names (EXPERIMENTAL)."), NULL },
  { "auth-no-challenge", '\0', POPT_BIT_SET,	&rpmioHttpFlags, HTTP_FLAGS_NOCHALLENGE,
	N_("Send Basic HTTP authentication information without first waiting for the server's challenge."), NULL },
  POPT_TABLEEND
};

enum rpmioHttpsFlags_e {
    HTTPS_FLAGS_NONE		= 0,
    HTTPS_FLAGS_NOCHECK		= _WFB( 0), /*!<    --no-check-certificate ... */
};
/*@unchecked@*/
static enum rpmioHttpsFlags_e rpmioHttpsFlags = HTTPS_FLAGS_NONE;

/*@unchecked@*/
static const char * _https_protocol;
/*@unchecked@*/
static const char * _https_certificate_file;
/*@unchecked@*/
static const char * _https_certificate_type;
/*@unchecked@*/
static const char * _https_privatekey_file;
/*@unchecked@*/
static const char * _https_privatekey_type;
/*@unchecked@*/
static const char * _https_cacertificate_file;
/*@unchecked@*/
static const char * _https_cacertificate_dir;
/*@unchecked@*/
static const char * _https_random_file;
/*@unchecked@*/
static const char * _https_egd_file;

/*@unchecked@*/ /*@observer@*/
static struct poptOption rpmioHttpsPoptTable[] = {
  { "secure-protocol", '\0', POPT_ARG_STRING,	&_https_protocol, 0,
	N_("choose secure protocol, one of auto, SSLv2, SSLv3, and TLSv1."), N_("PR") },
  { "no-check-certificate", '\0', POPT_BIT_SET,	&rpmioHttpsFlags, HTTPS_FLAGS_NOCHECK,
	N_("don't validate the server's certificate."), NULL },
  { "certificate", '\0', POPT_ARG_STRING,	&_https_certificate_file, 0,
	N_("client certificate file."), N_("FILE") },
  { "certificate-type", '\0', POPT_ARG_STRING,	&_https_certificate_type, 0,
	N_("client certificate type, PEM or DER."), N_("TYPE") },
  { "private-key", '\0', POPT_ARG_STRING,	&_https_privatekey_file, 0,
	N_("private key file."), N_("FILE") },
  { "private-key-type", '\0', POPT_ARG_STRING,	&_https_privatekey_type, 0,
	N_("private key type, PEM or DER."), N_("TYPE") },
  { "ca-certificate", '\0', POPT_ARG_STRING,	&_https_cacertificate_file, 0,
	N_("file with the bundle of CA's."), N_("FILE") },
  { "ca-directory", '\0', POPT_ARG_STRING,	&_https_cacertificate_dir, 0,
	N_("directory where hash list of CA's is stored."), N_("DIR") },
  { "random-file", '\0', POPT_ARG_STRING,	&_https_random_file, 0,
	N_("file with random data for seeding the SSL PRNG."), N_("FILE") },
  { "egd-file", '\0', POPT_ARG_NONE,		&_https_egd_file, 0,
	N_("file naming the EGD socket with random data."), N_("FILE") },
  POPT_TABLEEND
};

enum rpmioFtpFlags_e {
    FTP_FLAGS_NONE		= 0,
    FTP_FLAGS_KEEPLISTING	= _WFB( 0), /*!<    --no-remove-listing ... */
    FTP_FLAGS_NOGLOB		= _WFB( 1), /*!<    --no-glob ... */
    FTP_FLAGS_ACTIVE		= _WFB( 2), /*!<    --no-passive-ftp ... */
    FTP_FLAGS_FOLLOW		= _WFB( 3), /*!<    --retr-symlinks ... */
    FTP_FLAGS_PRESERVE		= _WFB( 4), /*!<    --preserve-permissions ... */
};
/*@unchecked@*/
static enum rpmioFtpFlags_e rpmioFtpFlags = FTP_FLAGS_NONE;

/*@unchecked@*/
static const char * _ftp_user;
/*@unchecked@*/
static const char * _ftp_password;

/*@unchecked@*/ /*@observer@*/
static struct poptOption rpmioFtpPoptTable[] = {
  { "ftp-user", '\0', POPT_ARG_STRING,		&_ftp_user, 0,
	N_("set ftp user to USER."), N_("USER") },
  { "ftp-password", '\0', POPT_ARG_STRING,	&_ftp_password, 0,
	N_("set ftp password to PASS."), N_("PASS") },
  { "no-remove-listing", '\0', POPT_BIT_SET,	&rpmioFtpFlags, FTP_FLAGS_KEEPLISTING,
	N_("don't remove `.listing' files."), NULL },
  { "no-glob", '\0', POPT_BIT_SET,		&rpmioFtpFlags, FTP_FLAGS_NOGLOB,
	N_("turn off FTP file name globbing."), NULL },
  { "no-passive-ftp", '\0', POPT_BIT_SET,	&rpmioFtpFlags, FTP_FLAGS_ACTIVE,
	N_("disable the \"passive\" transfer mode."), NULL },
  { "retr-symlinks", '\0', POPT_BIT_SET,	&rpmioFtpFlags, FTP_FLAGS_FOLLOW,
	N_("when recursing, get linked-to files (not dir)."), NULL },
  { "preserve-permissions", '\0', POPT_BIT_SET,	&rpmioFtpFlags, FTP_FLAGS_PRESERVE,
	N_("preserve remote file permissions."), NULL },
  POPT_TABLEEND
};

/*@unchecked@*/
static int _wctl_recurse_max;

/*@unchecked@*/ /*@observer@*/
static struct poptOption rpmioWCTLDownloadPoptTable[] = {
  { "recursive", 'r', POPT_BIT_SET,	&rpmioWCTLFlags, WCTL_FLAGS_RECURSE,
	N_("specify recursive download."), NULL },
  { "level", 'l', POPT_ARG_INT,	&_wctl_recurse_max, 0,
	N_("maximum recursion depth (inf or 0 for infinite)."), N_("NUMBER") },
  { "delete-after", '\0', POPT_BIT_SET,	&rpmioWCTLFlags, WCTL_FLAGS_DELETEAFTER,
	N_("delete files locally after downloading them."), NULL },
  { "convert-links", 'k', POPT_BIT_SET,	&rpmioWCTLFlags, WCTL_FLAGS_CONVERTLINKS,
	N_("make links in downloaded HTML point to local files."), NULL },
  { "backup-converted", 'K', POPT_BIT_SET,	&rpmioWCTLFlags, WCTL_FLAGS_BACKUP,
	N_("before converting file X, back up as X.orig."), NULL },
/*XXX popt alias? */
  { "mirror", 'm', POPT_BIT_SET,	&rpmioWCTLFlags, WCTL_FLAGS_MIRROR,
	N_("shortcut for -N -r -l inf --no-remove-listing."), NULL },
  { "page-requisites", 'p', POPT_BIT_SET,	&rpmioWCTLFlags, WCTL_FLAGS_PAGEREQUISITES,
	N_("get all images, etc. needed to display HTML page."), NULL },
  { "strict-comments", '\0', POPT_BIT_SET,	&rpmioWCTLFlags, WCTL_FLAGS_STRICTCOMMENTS,
	N_("turn on strict (SGML) handling of HTML comments."), NULL },
  POPT_TABLEEND
};

/*@unchecked@*/
static const char * _wctl_accept_exts;
/*@unchecked@*/
static const char * _wctl_reject_exts;
/*@unchecked@*/
static const char * _wctl_accept_domains;
/*@unchecked@*/
static const char * _wctl_reject_domains;
/*@unchecked@*/
static const char * _wctl_accept_tags;
/*@unchecked@*/
static const char * _wctl_reject_tags;
/*@unchecked@*/
static const char * _wctl_accept_dirs;
/*@unchecked@*/
static const char * _wctl_reject_dirs;

/*@unchecked@*/ /*@observer@*/
static struct poptOption rpmioWCTLAcceptPoptTable[] = {
  { "accept", 'A', POPT_ARG_STRING,	&_wctl_accept_exts, 0,
	N_("comma-separated list of accepted extensions."), N_("LIST") },
  { "reject", 'R', POPT_ARG_STRING,	&_wctl_reject_exts, 0,
	N_("comma-separated list of rejected extensions."), N_("LIST") },
  { "domains", 'D', POPT_ARG_STRING,	&_wctl_accept_domains, 0,
	N_("comma-separated list of accepted domains."), N_("LIST") },
  { "exclude-domains", '\0', POPT_ARG_STRING,	&_wctl_reject_domains, 0,
	N_("comma-separated list of rejected domains."), N_("LIST") },
  { "follow-ftp", '\0', POPT_BIT_SET,	&rpmioWCTLFlags, WCTL_FLAGS_FOLLOWFTP,
	N_("follow FTP links from HTML documents."), NULL },
  { "follow-tags", '\0', POPT_ARG_STRING,	&_wctl_accept_tags, 0,
	N_("comma-separated list of followed HTML tags."), N_("LIST") },
  { "ignore-tags", '\0', POPT_ARG_STRING,	&_wctl_reject_tags, 0,
	N_("comma-separated list of ignored HTML tags."), N_("LIST") },
  { "span-hosts", 'H', POPT_BIT_SET,	&rpmioWCTLFlags, WCTL_FLAGS_SPANHOSTS,
	N_("go to foreign hosts when recursive."), NULL },
  { "relative", 'L', POPT_BIT_SET,	&rpmioWCTLFlags, WCTL_FLAGS_RELATIVEONLY,
	N_("follow relative links only."), NULL },
  { "include-directories", 'I', POPT_ARG_STRING,	&_wctl_accept_dirs, 0,
	N_("list of allowed directories."), N_("LIST") },
  { "exclude-directories", 'X', POPT_ARG_STRING,	&_wctl_reject_dirs, 0,
	N_("list of excluded directories."), N_("LIST") },
  { "no-parent", '\0', POPT_BIT_SET,	&rpmioWCTLFlags, WCTL_FLAGS_NOPARENT,
	N_("don't ascend to the parent directory."), NULL },
  { "np", '\0', POPT_BIT_SET|POPT_ARGFLAG_ONEDASH,	&rpmioWCTLFlags, WCTL_FLAGS_NOPARENT,
	N_("don't ascend to the parent directory."), NULL },
  POPT_TABLEEND
};

/*@unchecked@*/ /*@observer@*/
static struct poptOption optionsTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmwgetArgCallback, 0, NULL, NULL },
/*@=type@*/

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioWCTLStartupPoptTable, 0,
	N_("Startup:"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioWCTLLoggingPoptTable, 0,
	N_("Logging and input file:"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioWDLPoptTable, 0,
	N_("Download:"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioWDirPoptTable, 0,
	N_("Directories:"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioHttpPoptTable, 0,
	N_("HTTP options:"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioHttpsPoptTable, 0,
	N_("HTTPS (SSL/TLS) options:"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioFtpPoptTable, 0,
	N_("FTP options:"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioWCTLDownloadPoptTable, 0,
	N_("Recursive download:"), NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioWCTLAcceptPoptTable, 0,
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

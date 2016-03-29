/** \ingroup rpmio
 * \file rpmio/rpmdate.c
 */

#define _XOPEN_SOURCE /* glibc2 needs this (man strptime) */
#include "system.h"
#include <sys/timeb.h>

#include <rpmiotypes.h>
#include <rpmio.h>	/* for *Pool methods */
#include <rpmlog.h>
#include <rpmurl.h>
#include <poptIO.h>
#include <argv.h>

#define	_RPMDATE_INTERNAL
#include <rpmdate.h>

#include "debug.h"

/* This is portable and avoids bringing in all of the ctype stuff. */
#undef	isdigit
#define isdigit(c) ((c) >= '0' && (c) <= '9')

extern time_t get_date(char * p, struct timeb * now);

/*
 * putenv string to use Universal Coordinated Time.
 * POSIX.2 says it should be "TZ=UCT0" or "TZ=GMT0".
 */
#ifndef TZ_UCT
#if defined(hpux) || defined(__hpux__) || defined(ultrix) || defined(__ultrix__) || defined(USG)
#define TZ_UCT "TZ=GMT0"
#else
#define TZ_UCT "TZ="
#endif
#endif

int _rpmdate_debug = 0;
#define SPEW(_list)	if (_rpmdate_debug) fprintf _list

static rpmRC
rpmdateInit(rpmdate date, int ac, char * const* av)
{
    static char const rfc_2822_format[] = "%a, %d %b %Y %H:%M:%S %z";
    const char *format = NULL;
    char *fn = NULL;
    char *get_str = NULL;
    char *ref_fn = NULL;
    char *set_str = NULL;
    char *timespec = NULL;
#define	DATE_FLAGS_HOURS	0
#define	DATE_FLAGS_MINS		1
#define	DATE_FLAGS_DATE		2
#define	DATE_FLAGS_SECONDS	3
#define	DATE_FLAGS_NS		4

#define	DATE_FLAGS_RFC2822	(1 << 8)	/* -R,--rfc-2822 */
#define	DATE_FLAGS_UTC		(1 << 9)	/* -u */
#define DATE_ISSET(_a) (date->flags & DATE_FLAGS_##_a)
struct poptOption rpmdateOptionsTable[] = {
    { "date",'d', POPT_ARG_STRING,	&get_str, 0,
        N_("Display time described by STRING."), N_("STRING") },
    { "file",'f', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN,	&fn, 0,
        N_("Like --date, once for each line of DATEFILE."), N_("DATEFILE") },
    { "ref",'r', POPT_ARG_STRING,	&ref_fn, 0,
        N_("Set time from FILE mtime."), N_("FILE")  },
    { "set",'s', POPT_ARG_STRING,	&set_str, 0,
        N_("Set time described by STRING."), N_("STRING")  },
    { "rfc-822",'R', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN, &date->flags, DATE_FLAGS_RFC2822,
        N_("Output date and time in RFC 2822 format."), NULL },
    { "rfc-2822",'R', POPT_BIT_SET,	&date->flags, DATE_FLAGS_RFC2822,
        N_("Output date and time in RFC 2822 format."), NULL },
    { "rfc-3339",'\0', POPT_ARG_STRING,	&timespec, 0,
        N_("Output date and time in RFC 3339 format."), N_("<hours|minutes|date|seconds|ns>")  },
    { "utc",'u', POPT_BIT_SET,		&date->flags, DATE_FLAGS_UTC,
        N_("Print or set Coordinated Universal Time."), NULL },
    { "uct",'\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,	&date->flags, DATE_FLAGS_UTC,
        N_("Print or set Coordinated Universal Time."), NULL },
    { "universal",'\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,	&date->flags, DATE_FLAGS_UTC,
        N_("Print or set Coordinated Universal Time."), NULL },

    { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
        "\
Usage: date [-u] [-d datestr] [-s datestr] [+FORMAT] [MMDDhhmm[[CC]YY][.ss]]\
", NULL },

    POPT_AUTOHELP
    POPT_TABLEEND
};
    poptContext con = poptGetContext(av[0], ac, (const char **)av,
		rpmdateOptionsTable, 0);
    ARGV_t nav = NULL;
    int nac = 0;
    struct timeval now;
    struct timespec when;
    struct tm tm = {};
    rpmRC rc = RPMRC_FAIL;	/* assume failure */
    int xx;
    int i;

    while ((xx = poptGetNextOpt(con)) > 0)
    switch (xx) {
    default:
	fprintf(stderr, _("%s: option table misconfigured (%d)\n"),
                __FUNCTION__, xx);
	goto exit;
	break;
    }

    if (DATE_ISSET(UTC))
	assert(putenv(TZ_UCT) == 0);

    nav = NULL;
    xx = argvAppend(&nav, poptGetArgs(con));
    nac = argvCount(nav);

    if (fn) {
	FD_t fd = Fopen(fn, "r.fpio");
	if (fd == NULL || Ferror(fd)) {
	    xx = Fclose(fd);
	    rpmlog(RPMLOG_ERR, _("%s: Fopen failed\n"), fn);
	    goto exit;
	}
	xx = argvFgets(&nav, fd);
	xx = Fclose(fd);
	nac = argvCount(nav);
    }

    when.tv_sec = (time_t)-1;
    when.tv_nsec = 0;

    xx = gettimeofday(&now, NULL);

#ifdef	NOTYET
    if (set_str == NULL && nac == 1 && nav && nav[0] && nav[0][0] != '+')
	set_str = strdup(nav[0]);
	when = posixtime(&when.tv_sec, set_str,
		(PDS_TRAILING_YEAR|PDS_CENTIRY|PDS_SECONDS));
	when.tv_nsec = 0;
    }
#endif

    /* Set the time. */
    if (set_str) {
	struct timeval tv;
	when.tv_sec = get_date(set_str, NULL);
	if (when.tv_sec == (time_t)-1) {
	    rpmlog(RPMLOG_ERR, _("invalid data\n"));
	    goto exit;
	}
	when.tv_nsec = 0;
	TIMESPEC_TO_TIMEVAL(&tv, &when);
	xx = settimeofday(&tv, NULL);
	if (xx != 0) {
	    rpmlog(RPMLOG_ERR, _("cannot set date\n"));
	    goto exit;
	}
	get_str = _free(get_str);
	get_str = set_str;
	set_str = _free(set_str);
    }

    /* Choose initial timespec. */
    if (ref_fn) {
	struct stat sb;
	int xx = Stat(ref_fn, &sb);
	(void)xx;
#if defined(TIMEVAL_TO_TIMESPEC)
	when = sb.st_mtimespec;
#else
	when.tv_sec = sb.st_mtime;
#endif
    }
    else if (get_str)
	when.tv_sec = get_date(get_str, NULL);
    else
	TIMEVAL_TO_TIMESPEC(&now, &when);

    /* Choose initial display format. */
    if (DATE_ISSET(RFC2822))
	format = rfc_2822_format;
    else
	format = "%a %b %e %H:%M:%S %Z %Y";

    i = 0;
    do {
	/* Choose next format or timespec. */
	if (get_str == NULL && ref_fn == NULL && nav && nav[i])
	switch (nav[i][0]) {
	case '+':		/* Override the display format. */
	    format = nav[i] + 1;
	    break;
	case '/':		/* Override the reference file */
	{   struct stat sb;
	    if (Stat(nav[i], &sb))	/* XXX skip on error? */
		continue;
#if defined(TIMEVAL_TO_TIMESPEC)
	    when = sb.st_mtimespec;
#else
	    when.tv_sec = sb.st_mtime;
#endif
	}   break;
	default:		/* Override the reference time */
	    when.tv_sec = get_date((char *)nav[i], NULL);
	    break;
	}
	else
	    TIMEVAL_TO_TIMESPEC(&now, &when);

	if (when.tv_sec == (time_t)-1) {
	    rpmlog(RPMLOG_ERR, _("invalid data\n"));
	    goto exit;
	}

	memset(&tm, 0, sizeof(tm));
	(void) localtime_r(&when.tv_sec, &tm);

	/* Display the time */
	{   char *t = NULL;
	    size_t nt = 0;

	    if (format == rfc_2822_format)
		setlocale(LC_TIME, "C");

	    if (!(format && format[0]))
		t = xstrdup("");
	    else do {
		nt += 64;
		t = (char *) realloc(t, nt);
assert(t != NULL);	/* XXX coverity 1357687 */
	    } while (strftime(t, nt, format, &tm) == 0);

	    if (format == rfc_2822_format)
		setlocale(LC_TIME, "");

	    argvAdd(&date->results, t);
	    t = _free(t);
	    nt = 0;
	}
	if (get_str || ref_fn) {
	    get_str = _free(get_str);
	    ref_fn = _free(ref_fn);
	} else
	    i++;
    } while (i < nac);

    rc = RPMRC_OK;

exit:
SPEW((stderr, "<-- %s(%p,%p[%d]) rc %d\n", __FUNCTION__, date, av, ac, rc));

    timespec = _free(timespec);
    ref_fn = _free(ref_fn);
    set_str = _free(set_str);
    get_str = _free(get_str);
    fn = _free(fn);

    nav = argvFree(nav);
    nac = 0;
    if (con)
	con = poptFreeContext(con);

    return rc;
}

static void rpmdateFini(void * _date)
{
    rpmdate date = (rpmdate) _date;
    date->flags = 0;
    date->results = argvFree(date->results);
}

rpmioPool _rpmdatePool = NULL;

static rpmdate rpmdateGetPool(rpmioPool pool)
{
    rpmdate date;

    if (_rpmdatePool == NULL) {
	_rpmdatePool = rpmioNewPool("date", sizeof(*date), -1, _rpmdate_debug,
			NULL, NULL, rpmdateFini);
	pool = _rpmdatePool;
    }
    date = (rpmdate) rpmioGetPool(pool, sizeof(*date));
    memset(((char *)date)+sizeof(date->_item), 0, sizeof(*date)-sizeof(date->_item));
    return date;
}

rpmdate rpmdateNew(char ** argv, unsigned flags)
{
    static char *_argv[] = { "date", NULL };
    ARGV_t av = (ARGV_t) (argv ? argv : _argv);
    int ac = argvCount(av);
    rpmdate date = rpmdateGetPool(_rpmdatePool);
    rpmRC rc;

    date->flags = (flags ? flags : 0);
    rc = rpmdateInit(date, ac, (char * const*)av);
    if (rc != RPMRC_OK)
	date = rpmdateFree(date);

    return rpmdateLink(date);
}

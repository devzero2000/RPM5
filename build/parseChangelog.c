/** \ingroup rpmbuild
 * \file build/parseChangelog.c
 *  Parse %changelog section from spec file.
 */

#include "system.h"

#include "rpmbuild.h"
#include "debug.h"

#define mySKIPSPACE(s) { while (*(s) && isspace(*(s))) (s)++; }
#define mySKIPNONSPACE(s) { while (*(s) && !isspace(*(s))) (s)++; }

void addChangelogEntry(Header h, time_t time, const char *name, const char *text)
{
    int_32 mytime = time;	/* XXX convert to int_32 in header */

    (void) headerAddOrAppendEntry(h, RPMTAG_CHANGELOGTIME,
		RPM_INT32_TYPE, &mytime, 1);
    (void) headerAddOrAppendEntry(h, RPMTAG_CHANGELOGNAME,
		RPM_STRING_ARRAY_TYPE, &name, 1);
    (void) headerAddOrAppendEntry(h, RPMTAG_CHANGELOGTEXT,
		RPM_STRING_ARRAY_TYPE, &text, 1);
}

/**
 * Parse date string to seconds.
 * @param datestr	date string (e.g. 'Wed Jan 1 1997')
 * @retval secs		secs since the unix epoch
 * @return 		0 on success, -1 on error
 */
/*@-boundswrite@*/
static int dateToTimet(const char * datestr, /*@out@*/ time_t * secs)
	/*@modifies *secs @*/
{
    struct tm time;
    time_t timezone_offset;
    char * p, * pe, * q, ** idx;
    char * date = strcpy(alloca(strlen(datestr) + 1), datestr);
/*@observer@*/ static char * days[] =
	{ "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", NULL };
/*@observer@*/ static char * months[] =
	{ "Jan", "Feb", "Mar", "Apr", "May", "Jun",
 	  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL };
/*@observer@*/ static char lengths[] =
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    
    memset(&time, 0, sizeof(time));

    pe = date;

    /* day of week */
    p = pe; mySKIPSPACE(p);
    if (*p == '\0') return -1;
    pe = p; mySKIPNONSPACE(pe); if (*pe != '\0') *pe++ = '\0';
    for (idx = days; *idx && strcmp(*idx, p); idx++)
	{};
    if (*idx == NULL) return -1;

    /* month */
    p = pe; mySKIPSPACE(p);
    if (*p == '\0') return -1;
    pe = p; mySKIPNONSPACE(pe); if (*pe != '\0') *pe++ = '\0';
    for (idx = months; *idx && strcmp(*idx, p); idx++)
	{};
    if (*idx == NULL) return -1;
    time.tm_mon = idx - months;

    /* day */
    p = pe; mySKIPSPACE(p);
    if (*p == '\0') return -1;
    pe = p; mySKIPNONSPACE(pe); if (*pe != '\0') *pe++ = '\0';

    /* make this noon so the day is always right (as we make this UTC) */
    time.tm_hour = 12;

    time.tm_mday = strtol(p, &q, 10);
    if (!(q && *q == '\0')) return -1;
    if (time.tm_mday < 0 || time.tm_mday > lengths[time.tm_mon]) return -1;

    /* year */
    p = pe; mySKIPSPACE(p);
    if (*p == '\0') return -1;
    pe = p; mySKIPNONSPACE(pe); if (*pe != '\0') *pe++ = '\0';
    time.tm_year = strtol(p, &q, 10);
    if (!(q && *q == '\0')) return -1;
    if (time.tm_year < 1990 || time.tm_year >= 3000) return -1;
    time.tm_year -= 1900;

    *secs = mktime(&time);
    if (*secs == -1) return -1;

    /* determine timezone offset */
    timezone_offset = mktime(gmtime(secs)) - *secs;

    /* adjust to UTC */
    *secs += timezone_offset;

    return 0;
}
/*@=boundswrite@*/

/*@-redecl@*/
extern time_t get_date(const char * p, void * now);     /* XXX expedient lies */
/*@=redecl@*/

/**
 * Add %changelog section to header.
 * @param h		header
 * @param sb		changelog strings
 * @return		0 on success
 */
/*@-boundswrite@*/
static int addChangelog(Header h, StringBuf sb)
	/*@globals rpmGlobalMacroContext, h_errno @*/
	/*@modifies h, rpmGlobalMacroContext @*/
{
    char * s = getStringBuf(sb);
    char * se;
    char *date, *name, *text;
    int i;
    time_t time;
    time_t lastTime = 0;
    int nentries = 0;
    static time_t last = 0;
    static int oneshot = 0;

    /* Determine changelog truncation criteria. */
    if (!oneshot++) {
	char * t = rpmExpand("%{?_changelog_truncate}", NULL);
	char *te = NULL;
	if (t && *t) {
	    long res = strtol(t, &te, 0);
	    if (res >= 0 && *te == '\0') {
		last = res;		/* truncate to no. of entries. */
	    } else {
/*@-moduncon@*/
		res = get_date (t, NULL);
/*@=moduncon@*/
		/* XXX malformed date string silently ignored. */
		if (res > 0) {
		    last = res;		/* truncate to date. */
		}
	    }
	}
	t = _free(t);
    }

    /* skip space */
    mySKIPSPACE(s);

    while (*s != '\0') {
	if (*s != '*') {
	    rpmError(RPMERR_BADSPEC,
			_("%%changelog entries must start with *\n"));
	    return RPMERR_BADSPEC;
	}

	/* find end of line */
	date = s;
	while(*s && *s != '\n') s++;
	if (! *s) {
	    rpmError(RPMERR_BADSPEC, _("incomplete %%changelog entry\n"));
	    return RPMERR_BADSPEC;
	}
/*@-modobserver@*/
	*s = '\0';
/*@=modobserver@*/
	text = s + 1;
	
	/* 4 fields of date */
	date++;
	s = date;
	for (i = 0; i < 4; i++) {
	    mySKIPSPACE(s);
	    mySKIPNONSPACE(s);
	}
	mySKIPSPACE(date);
	if (dateToTimet(date, &time)) {
	    rpmError(RPMERR_BADSPEC, _("bad date in %%changelog: %s\n"), date);
	    return RPMERR_BADSPEC;
	}
	if (lastTime && lastTime < time) {
	    rpmError(RPMERR_BADSPEC,
		     _("%%changelog not in descending chronological order\n"));
	    return RPMERR_BADSPEC;
	}
	lastTime = time;

	/* skip space to the name */
	mySKIPSPACE(s);
	if (! *s) {
	    rpmError(RPMERR_BADSPEC, _("missing name in %%changelog\n"));
	    return RPMERR_BADSPEC;
	}

	/* name */
	name = s;
	while (*s != '\0') s++;
	while (s > name && isspace(*s))
	    *s-- = '\0';

	if (s == name) {
	    rpmError(RPMERR_BADSPEC, _("missing name in %%changelog\n"));
	    return RPMERR_BADSPEC;
	}

	/* text */
	mySKIPSPACE(text);
	if (! *text) {
	    rpmError(RPMERR_BADSPEC, _("no description in %%changelog\n"));
	    return RPMERR_BADSPEC;
	}
	    
	/* find the next leading '*' (or eos) */
	s = text;
	do {
	   s++;
	} while (*s && (*(s-1) != '\n' || *s != '*'));
	se = s;
	s--;

	/* backup to end of description */
	while ((s > text) && xisspace(*s))
	    *s-- = '\0';
	
	/* Add entry if not truncated. */
	nentries++;

	if (last <= 0
	 || (last < 1000 && nentries < last)
	 || (last > 1000 && time >= last))
	    addChangelogEntry(h, time, name, text);

	s = se;

    }

    return 0;
}
/*@=boundswrite@*/

int parseChangelog(Spec spec)
{
    int nextPart, res, rc;
    StringBuf sb = newStringBuf();
    
    /* There are no options to %changelog */
    if ((rc = readLine(spec, STRIP_COMMENTS)) > 0) {
	sb = freeStringBuf(sb);
	return PART_NONE;
    }
    if (rc)
	return rc;
    
    while (! (nextPart = isPart(spec->line))) {
	const char * line;
	line = xstrdup(spec->line);
	line = xstrtolocale(line);
	appendStringBuf(sb, spec->line);
	line = _free(line);
	if ((rc = readLine(spec, STRIP_COMMENTS)) > 0) {
	    nextPart = PART_NONE;
	    break;
	}
	if (rc)
	    return rc;
    }

    res = addChangelog(spec->packages->header, sb);
    sb = freeStringBuf(sb);

    return (res) ? res : nextPart;
}

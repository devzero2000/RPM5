/** \ingroup header
 * \file rpmdb/formats.c
 */

#include "system.h"

#include "rpmio_internal.h"
#include <rpmlib.h>
#include <rpmmacro.h>	/* XXX for %_i18ndomains */

#define _RPMEVR_INTERNAL
#include <rpmevr.h>	/* XXX RPMSENSE_FOO */

#include "legacy.h"
#include "argv.h"
#include "misc.h"

#include "debug.h"

/*@access pgpDig @*/
/*@access pgpDigParams @*/

/*@-boundswrite@*/
/* XXX FIXME: static for now, refactor from manifest.c later. */
static char * rpmPermsString(int mode)
{
    char *perms = xstrdup("----------");
   
    if (S_ISREG(mode)) 
	perms[0] = '-';
    else if (S_ISDIR(mode)) 
	perms[0] = 'd';
    else if (S_ISLNK(mode))
	perms[0] = 'l';
    else if (S_ISFIFO(mode)) 
	perms[0] = 'p';
    /*@-unrecog@*/
    else if (S_ISSOCK(mode)) 
	perms[0] = 's';
    /*@=unrecog@*/
    else if (S_ISCHR(mode))
	perms[0] = 'c';
    else if (S_ISBLK(mode))
	perms[0] = 'b';
    else
	perms[0] = '?';

    if (mode & S_IRUSR) perms[1] = 'r';
    if (mode & S_IWUSR) perms[2] = 'w';
    if (mode & S_IXUSR) perms[3] = 'x';
 
    if (mode & S_IRGRP) perms[4] = 'r';
    if (mode & S_IWGRP) perms[5] = 'w';
    if (mode & S_IXGRP) perms[6] = 'x';

    if (mode & S_IROTH) perms[7] = 'r';
    if (mode & S_IWOTH) perms[8] = 'w';
    if (mode & S_IXOTH) perms[9] = 'x';

    if (mode & S_ISUID)
	perms[3] = ((mode & S_IXUSR) ? 's' : 'S'); 

    if (mode & S_ISGID)
	perms[6] = ((mode & S_IXGRP) ? 's' : 'S'); 

    if (mode & S_ISVTX)
	perms[9] = ((mode & S_IXOTH) ? 't' : 'T');

    return perms;
}
/*@=boundswrite@*/
/**
 * Identify type of trigger.
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix	(unused)
 * @param padding	(unused)
 * @param element	(unused)
 * @return		formatted string
 */
static /*@only@*/ char * triggertypeFormat(int_32 type, const void * data,
		/*@unused@*/ char * formatPrefix, /*@unused@*/ int padding,
		/*@unused@*/ int element)
	/*@requires maxRead(data) >= 0 @*/
{
    const int_32 * item = data;
    char * val;

    if (type != RPM_INT32_TYPE)
	val = xstrdup(_("(invalid type)"));
    else if (*item & RPMSENSE_TRIGGERPREIN)
	val = xstrdup("prein");
    else if (*item & RPMSENSE_TRIGGERIN)
	val = xstrdup("in");
    else if (*item & RPMSENSE_TRIGGERUN)
	val = xstrdup("un");
    else if (*item & RPMSENSE_TRIGGERPOSTUN)
	val = xstrdup("postun");
    else
	val = xstrdup("");
    return val;
}

/**
 * Format file permissions for display.
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix
 * @param padding
 * @param element	(unused)
 * @return		formatted string
 */
static /*@only@*/ char * permsFormat(int_32 type, const void * data,
		char * formatPrefix, int padding, /*@unused@*/ int element)
	/*@modifies formatPrefix @*/
	/*@requires maxRead(data) >= 0 @*/
{
    char * val;
    char * buf;

    if (type != RPM_INT32_TYPE) {
	val = xstrdup(_("(invalid type)"));
    } else {
	val = xmalloc(15 + padding);
/*@-boundswrite@*/
	strcat(formatPrefix, "s");
/*@=boundswrite@*/
	buf = rpmPermsString(*((int_32 *) data));
	/*@-formatconst@*/
	sprintf(val, formatPrefix, buf);
	/*@=formatconst@*/
	buf = _free(buf);
    }

    return val;
}

/**
 * Format file flags for display.
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix
 * @param padding
 * @param element	(unused)
 * @return		formatted string
 */
static /*@only@*/ char * fflagsFormat(int_32 type, const void * data,
		char * formatPrefix, int padding, /*@unused@*/ int element)
	/*@modifies formatPrefix @*/
	/*@requires maxRead(data) >= 0 @*/
{
    char * val;
    char buf[15];
    int anint = *((int_32 *) data);

    if (type != RPM_INT32_TYPE) {
	val = xstrdup(_("(invalid type)"));
    } else {
	buf[0] = '\0';
/*@-boundswrite@*/
	if (anint & RPMFILE_DOC)
	    strcat(buf, "d");
	if (anint & RPMFILE_CONFIG)
	    strcat(buf, "c");
	if (anint & RPMFILE_SPECFILE)
	    strcat(buf, "s");
	if (anint & RPMFILE_MISSINGOK)
	    strcat(buf, "m");
	if (anint & RPMFILE_NOREPLACE)
	    strcat(buf, "n");
	if (anint & RPMFILE_GHOST)
	    strcat(buf, "g");
	if (anint & RPMFILE_LICENSE)
	    strcat(buf, "l");
	if (anint & RPMFILE_README)
	    strcat(buf, "r");
/*@=boundswrite@*/

	val = xmalloc(5 + padding);
/*@-boundswrite@*/
	strcat(formatPrefix, "s");
/*@=boundswrite@*/
	/*@-formatconst@*/
	sprintf(val, formatPrefix, buf);
	/*@=formatconst@*/
    }

    return val;
}

/**
 * Wrap a pubkey in ascii armor for display.
 * @todo Permit selectable display formats (i.e. binary).
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix	(unused)
 * @param padding	(unused)
 * @param element	no. bytes of binary data
 * @return		formatted string
 */
static /*@only@*/ char * armorFormat(int_32 type, const void * data,
		/*@unused@*/ char * formatPrefix, /*@unused@*/ int padding,
		int element)
	/*@*/
{
    const char * enc;
    const unsigned char * s;
    size_t ns;
    int atype;
    char * val;

    switch (type) {
    case RPM_OPENPGP_TYPE:
    case RPM_ASN1_TYPE:		/* XXX WRONG */
    case RPM_BIN_TYPE:
	s = data;
	/* XXX HACK ALERT: element field abused as no. bytes of binary data. */
	ns = element;
	atype = PGPARMOR_SIGNATURE;	/* XXX check pkt for signature */
	break;
    case RPM_STRING_TYPE:
    case RPM_STRING_ARRAY_TYPE:
	enc = data;
	s = NULL;
	ns = 0;
	if (b64decode(enc, (void **)&s, &ns))
	    return xstrdup(_("(not base64)"));
	atype = PGPARMOR_PUBKEY;	/* XXX check pkt for pubkey */
	break;
    case RPM_NULL_TYPE:
    case RPM_CHAR_TYPE:
    case RPM_INT8_TYPE:
    case RPM_INT16_TYPE:
    case RPM_INT32_TYPE:
    case RPM_INT64_TYPE:
    case RPM_I18NSTRING_TYPE:
    default:
	return xstrdup(_("(invalid type)"));
	/*@notreached@*/ break;
    }

    /* XXX this doesn't use padding directly, assumes enough slop in retval. */
    val = pgpArmorWrap(atype, s, ns);
    if (atype == PGPARMOR_PUBKEY)
	s = _free(s);
    return val;
}

/**
 * Encode binary data in base64 for display.
 * @todo Permit selectable display formats (i.e. binary).
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix	(unused)
 * @param padding
 * @param element
 * @return		formatted string
 */
static /*@only@*/ char * base64Format(int_32 type, const void * data,
		/*@unused@*/ char * formatPrefix, int padding, int element)
	/*@*/
{
    char * val;

    if (!(type == RPM_BIN_TYPE || type == RPM_ASN1_TYPE || type == RPM_OPENPGP_TYPE)) {
	val = xstrdup(_("(not a blob)"));
    } else {
	const char * enc;
	char * t;
	int lc;
	/* XXX HACK ALERT: element field abused as no. bytes of binary data. */
	size_t ns = element;
	size_t nt = ((ns + 2) / 3) * 4;

/*@-boundswrite@*/
	/*@-globs@*/
	/* Add additional bytes necessary for eol string(s). */
	if (b64encode_chars_per_line > 0 && b64encode_eolstr != NULL) {
	    lc = (nt + b64encode_chars_per_line - 1) / b64encode_chars_per_line;
	if (((nt + b64encode_chars_per_line - 1) % b64encode_chars_per_line) != 0)
	    ++lc;
	    nt += lc * strlen(b64encode_eolstr);
	}
	/*@=globs@*/

	val = t = xcalloc(1, nt + padding + 1);
	*t = '\0';

    /* XXX b64encode accesses uninitialized memory. */
    { 	unsigned char * _data = xcalloc(1, ns+1);
	memcpy(_data, data, ns);
	if ((enc = b64encode(_data, ns)) != NULL) {
	    t = stpcpy(t, enc);
	    enc = _free(enc);
	}
	_data = _free(_data);
    }
/*@=boundswrite@*/
    }

    return val;
}

/**
 * Return length of string represented with xml characters substituted.
 * @param s		string
 * @return		length of xml string
 */
static size_t xmlstrlen(const char * s)
	/*@*/
{
    size_t len = 0;
    int c;

/*@-boundsread@*/
    while ((c = *s++) != '\0')
/*@=boundsread@*/
    {
	switch (c) {
	case '<':
	case '>':	len += sizeof("&lt;") - 1;	/*@switchbreak@*/ break;
	case '&':	len += sizeof("&amp;") - 1;	/*@switchbreak@*/ break;
	default:	len += 1;			/*@switchbreak@*/ break;
	}
    }
    return len;
}

/**
 * Copy source string to target, substituting for  xml characters.
 * @param t		target xml string
 * @param s		source string
 * @return		target xml string
 */
static char * xmlstrcpy(/*@returned@*/ char * t, const char * s)
	/*@modifies t @*/
{
    char * te = t;
    int c;

/*@-bounds@*/
    while ((c = *s++) != '\0') {
	switch (c) {
	case '<':	te = stpcpy(te, "&lt;");	/*@switchbreak@*/ break;
	case '>':	te = stpcpy(te, "&gt;");	/*@switchbreak@*/ break;
	case '&':	te = stpcpy(te, "&amp;");	/*@switchbreak@*/ break;
	default:	*te++ = c;			/*@switchbreak@*/ break;
	}
    }
    *te = '\0';
/*@=bounds@*/
    return t;
}

/**
 * Wrap tag data in simple header xml markup.
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix
 * @param padding
 * @param element	(unused)
 * @return		formatted string
 */
/*@-bounds@*/
static /*@only@*/ char * xmlFormat(int_32 type, const void * data,
		char * formatPrefix, int padding,
		/*@unused@*/ int element)
	/*@modifies formatPrefix @*/
{
    const char * xtag = NULL;
    size_t nb;
    char * val;
    const char * s = NULL;
    char * t, * te;
    unsigned long long anint = 0;
    int freeit = 0;
    int xx;

/*@-branchstate@*/
    switch (type) {
    case RPM_I18NSTRING_TYPE:
    case RPM_STRING_TYPE:
	s = data;
	xtag = "string";
	/* XXX Force utf8 strings. */
	s = xstrdup(s);
	s = xstrtolocale(s);
	freeit = 1;
	break;
    case RPM_OPENPGP_TYPE:
    case RPM_ASN1_TYPE:
    case RPM_BIN_TYPE:
    {	int cpl = b64encode_chars_per_line;
/*@-mods@*/
	b64encode_chars_per_line = 0;
/*@=mods@*/
/*@-formatconst@*/
	s = base64Format(type, data, formatPrefix, padding, element);
/*@=formatconst@*/
/*@-mods@*/
	b64encode_chars_per_line = cpl;
/*@=mods@*/
	xtag = "base64";
	freeit = 1;
    }	break;
    case RPM_CHAR_TYPE:
    case RPM_INT8_TYPE:
	anint = *((uint_8 *) data);
	break;
    case RPM_INT16_TYPE:
	anint = *((uint_16 *) data);
	break;
    case RPM_INT32_TYPE:
	anint = *((uint_32 *) data);
	break;
    case RPM_INT64_TYPE:
	anint = *((uint_64 *) data);
	break;
    case RPM_NULL_TYPE:
    case RPM_STRING_ARRAY_TYPE:
    default:
	return xstrdup(_("(invalid xml type)"));
	/*@notreached@*/ break;
    }
/*@=branchstate@*/

/*@-branchstate@*/
    if (s == NULL) {
	int tlen = 64;
	t = memset(alloca(tlen+1), 0, tlen+1);
/*@-duplicatequals@*/
	if (anint != 0)
	    xx = snprintf(t, tlen, "%llu", anint);
/*@=duplicatequals@*/
	s = t;
	xtag = "integer";
    }
/*@=branchstate@*/

    nb = xmlstrlen(s);
    if (nb == 0) {
	nb += strlen(xtag) + sizeof("\t</>");
	te = t = alloca(nb);
	te = stpcpy( stpcpy( stpcpy(te, "\t<"), xtag), "/>");
    } else {
	nb += 2 * strlen(xtag) + sizeof("\t<></>");
	te = t = alloca(nb);
	te = stpcpy( stpcpy( stpcpy(te, "\t<"), xtag), ">");
	te = xmlstrcpy(te, s);
	te += strlen(te);
	te = stpcpy( stpcpy( stpcpy(te, "</"), xtag), ">");
    }

/*@-branchstate@*/
    if (freeit)
	s = _free(s);
/*@=branchstate@*/

    nb += padding;
    val = xmalloc(nb+1);
/*@-boundswrite@*/
    strcat(formatPrefix, "s");
/*@=boundswrite@*/
/*@-formatconst@*/
    xx = snprintf(val, nb, formatPrefix, t);
/*@=formatconst@*/
    val[nb] = '\0';

    return val;
}
/*@=bounds@*/

/**
 * Return length of string represented with yaml indentation.
 * @param s		string
 * @param lvl		indentation level
 * @return		length of yaml string
 */
static size_t yamlstrlen(const char * s, int lvl)
	/*@*/
{
    size_t len = 0;
    int indent = (lvl > 0);
    int c;

/*@-boundsread@*/
    while ((c = *s++) != '\0')
/*@=boundsread@*/
    {
	if (indent) {
	    len += 2 * lvl;
	    indent = 0;
	}
	if (c == '\n')
	    indent = (lvl > 0);
	len++;
    }
    return len;
}

/**
 * Copy source string to target, indenting for yaml.
 * @param t		target yaml string
 * @param s		source string
 * @param lvl		indentation level
 * @return		target yaml string
 */
static char * yamlstrcpy(/*@out@*/ /*@returned@*/ char * t, const char * s, int lvl)
	/*@modifies t @*/
{
    char * te = t;
    int indent = (lvl > 0);
    int c;

/*@-bounds@*/
    while ((c = *s++) != '\0') {
	if (indent) {
	    int i;
	    for (i = 0; i < lvl; i++) {
		*te++ = ' ';
		*te++ = ' ';
	    }
	    indent = 0;
	}
	if (c == '\n')
	    indent = (lvl > 0);
	*te++ = c;
    }
    *te = '\0';
/*@=bounds@*/
    return t;
}

/**
 * Wrap tag data in simple header yaml markup.
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix
 * @param padding
 * @param element	element index (or -1 for non-array).
 * @return		formatted string
 */
/*@-bounds@*/
static /*@only@*/ char * yamlFormat(int_32 type, const void * data,
		char * formatPrefix, int padding,
		int element)
	/*@modifies formatPrefix @*/
{
    const char * xtag = NULL;
    const char * ytag = NULL;
    size_t nb;
    char * val;
    const char * s = NULL;
    char * t, * te;
    unsigned long long anint = 0;
    int freeit = 0;
    int lvl = 0;
    int xx;
    int c;

/*@-branchstate@*/
    switch (type) {
    case RPM_I18NSTRING_TYPE:
    case RPM_STRING_TYPE:
	xx = 0;
	s = data;
	if (strchr("[", s[0]))	/* leading [ */
	    xx = 1;
	if (xx == 0)
	while ((c = *s++) != '\0') {
	    switch (c) {
	    default:
		continue;
	    case '\n':	/* multiline */
		xx = 1;
		/*@switchbreak@*/ break;
	    case '-':	/* leading "- \"" */
	    case ':':	/* embedded ": " or ":" at EOL */
		if (s[0] != ' ' && s[0] != '\0' && s[1] != '"')
		    continue;
		xx = 1;
		/*@switchbreak@*/ break;
	    }
	    /*@loopbreak@*/ break;
	}
	if (xx) {
	    if (element >= 0) {
		xtag = "- |-\n";
		lvl = 3;
	    } else {
		xtag = "|-\n";
		lvl = 2;
	    }
	} else {
	    xtag = (element >= 0 ? "- " : NULL);
	}

	/* XXX Force utf8 strings. */
	s = xstrdup(data);
	s = xstrtolocale(s);
	freeit = 1;
	break;
    case RPM_OPENPGP_TYPE:
    case RPM_ASN1_TYPE:
    case RPM_BIN_TYPE:
    {	int cpl = b64encode_chars_per_line;
/*@-mods@*/
	b64encode_chars_per_line = 0;
/*@=mods@*/
/*@-formatconst@*/
	s = base64Format(type, data, formatPrefix, padding, element);
	element = -element;	/* XXX skip "    " indent. */
/*@=formatconst@*/
/*@-mods@*/
	b64encode_chars_per_line = cpl;
/*@=mods@*/
	xtag = "!!binary ";
	freeit = 1;
    }	break;
    case RPM_CHAR_TYPE:
    case RPM_INT8_TYPE:
	anint = *((uint_8 *) data);
	break;
    case RPM_INT16_TYPE:
	anint = *((uint_16 *) data);
	break;
    case RPM_INT32_TYPE:
	anint = *((uint_32 *) data);
	break;
    case RPM_INT64_TYPE:
	anint = *((uint_64 *) data);
	break;
    case RPM_NULL_TYPE:
    case RPM_STRING_ARRAY_TYPE:
    default:
	return xstrdup(_("(invalid yaml type)"));
	/*@notreached@*/ break;
    }
/*@=branchstate@*/

/*@-branchstate@*/
    if (s == NULL) {
	int tlen = 64;
	t = memset(alloca(tlen+1), 0, tlen+1);
/*@-duplicatequals@*/
	xx = snprintf(t, tlen, "%llu", anint);
/*@=duplicatequals@*/
	s = t;
	xtag = (element >= 0 ? "- " : NULL);
    }
/*@=branchstate@*/

    nb = yamlstrlen(s, lvl);
    if (nb == 0) {
	if (element >= 0)
	    nb += sizeof("    ") - 1;
	nb += sizeof("- ~") - 1;
	nb++;
	te = t = alloca(nb);
	if (element >= 0)
	    te = stpcpy(te, "    ");
	te = stpcpy(te, "- ~");
    } else {
	if (element >= 0)
	    nb += sizeof("    ") - 1;
	if (xtag)
	    nb += strlen(xtag);
	if (ytag)
	    nb += strlen(ytag);
	nb++;
	te = t = alloca(nb);
	if (element >= 0)
	    te = stpcpy(te, "    ");
	if (xtag)
	    te = stpcpy(te, xtag);
	te = yamlstrcpy(te, s, lvl);
	te += strlen(te);
	if (ytag)
	    te = stpcpy(te, ytag);
    }

    /* XXX s was malloc'd */
/*@-branchstate@*/
    if (freeit)
	s = _free(s);
/*@=branchstate@*/

    nb += padding;
    val = xmalloc(nb+1);
/*@-boundswrite@*/
    strcat(formatPrefix, "s");
/*@=boundswrite@*/
/*@-formatconst@*/
    xx = snprintf(val, nb, formatPrefix, t);
/*@=formatconst@*/
    val[nb] = '\0';

    return val;
}
/*@=bounds@*/

/**
 * Display signature fingerprint and time.
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix	(unused)
 * @param padding
 * @param element	(unused)
 * @return		formatted string
 */
static /*@only@*/ char * pgpsigFormat(int_32 type, const void * data,
		/*@unused@*/ char * formatPrefix, /*@unused@*/ int padding,
		/*@unused@*/ int element)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    char * val, * t;

    if (!(type == RPM_BIN_TYPE || type == RPM_ASN1_TYPE || type == RPM_OPENPGP_TYPE)) {
	val = xstrdup(_("(not a blob)"));
    } else {
	unsigned char * pkt = (byte *) data;
	unsigned int pktlen = 0;
/*@-boundsread@*/
	unsigned int v = *pkt;
/*@=boundsread@*/
	pgpTag tag = 0;
	unsigned int plen;
	unsigned int hlen = 0;

	if (v & 0x80) {
	    if (v & 0x40) {
		tag = (v & 0x3f);
		plen = pgpLen(pkt+1, &hlen);
	    } else {
		tag = (v >> 2) & 0xf;
		plen = (1 << (v & 0x3));
		hlen = pgpGrab(pkt+1, plen);
	    }
	
	    pktlen = 1 + plen + hlen;
	}

	if (pktlen == 0 || tag != PGPTAG_SIGNATURE) {
	    val = xstrdup(_("(not an OpenPGP signature)"));
	} else {
	    pgpDig dig = pgpNewDig();
	    pgpDigParams sigp = &dig->signature;
	    size_t nb = 0;
	    const char *tempstr;

	    (void) pgpPrtPkts(pkt, pktlen, dig, 0);

	    val = NULL;
	again:
	    nb += 100;
	    val = t = xrealloc(val, nb + 1);

/*@-boundswrite@*/
	    switch (sigp->pubkey_algo) {
	    case PGPPUBKEYALGO_DSA:
		t = stpcpy(t, "DSA");
		break;
	    case PGPPUBKEYALGO_RSA:
		t = stpcpy(t, "RSA");
		break;
	    default:
		(void) snprintf(t, nb - (t - val), "%d", sigp->pubkey_algo);
		t += strlen(t);
		break;
	    }
	    if (t + 5 >= val + nb)
		goto again;
	    *t++ = '/';
	    switch (sigp->hash_algo) {
	    case PGPHASHALGO_MD5:
		t = stpcpy(t, "MD5");
		break;
	    case PGPHASHALGO_SHA1:
		t = stpcpy(t, "SHA1");
		break;
	    default:
		(void) snprintf(t, nb - (t - val), "%d", sigp->hash_algo);
		t += strlen(t);
		break;
	    }
	    if (t + strlen (", ") + 1 >= val + nb)
		goto again;

	    t = stpcpy(t, ", ");

	    /* this is important if sizeof(int_32) ! sizeof(time_t) */
	    {	time_t dateint = pgpGrab(sigp->time, sizeof(sigp->time));
		struct tm * tstruct = localtime(&dateint);
		if (tstruct)
 		    (void) strftime(t, (nb - (t - val)), "%c", tstruct);
	    }
	    t += strlen(t);
	    if (t + strlen (", Key ID ") + 1 >= val + nb)
		goto again;
	    t = stpcpy(t, ", Key ID ");
	    tempstr = pgpHexStr(sigp->signid, sizeof(sigp->signid));
	    if (t + strlen (tempstr) > val + nb)
		goto again;
	    t = stpcpy(t, tempstr);
/*@=boundswrite@*/

	    dig = pgpFreeDig(dig);
	}
    }

    return val;
}

/**
 * Format dependency flags for display.
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix
 * @param padding
 * @param element	(unused)
 * @return		formatted string
 */
static /*@only@*/ char * depflagsFormat(int_32 type, const void * data,
		char * formatPrefix, int padding, /*@unused@*/ int element)
	/*@modifies formatPrefix @*/
	/*@requires maxRead(data) >= 0 @*/
{
    char * val;

    if (type != RPM_INT32_TYPE) {
	val = xstrdup(_("(invalid type)"));
    } else {
	int anint = *((int_32 *) data);
	char *t, *buf;

	t = buf = alloca(32);
	*t = '\0';

/*@-boundswrite@*/
#ifdef	NOTYET	/* XXX appending markers breaks :depflags format. */
	if (anint & RPMSENSE_SCRIPT_PRE)
	    t = stpcpy(t, "(pre)");
	if (anint & RPMSENSE_SCRIPT_POST)
	    t = stpcpy(t, "(post)");
	if (anint & RPMSENSE_SCRIPT_PREUN)
	    t = stpcpy(t, "(preun)");
	if (anint & RPMSENSE_SCRIPT_POSTUN)
	    t = stpcpy(t, "(postun)");
#endif
	if (anint & RPMSENSE_SENSEMASK)
	    *t++ = ' ';
	if (anint & RPMSENSE_LESS)
	    *t++ = '<';
	if (anint & RPMSENSE_GREATER)
	    *t++ = '>';
	if (anint & RPMSENSE_EQUAL)
	    *t++ = '=';
	if (anint & RPMSENSE_SENSEMASK)
	    *t++ = ' ';
	*t = '\0';
/*@=boundswrite@*/

	val = xmalloc(5 + padding);
/*@-boundswrite@*/
	strcat(formatPrefix, "s");
/*@=boundswrite@*/
	/*@-formatconst@*/
	sprintf(val, formatPrefix, buf);
	/*@=formatconst@*/
    }

    return val;
}

/**
 * Retrieve install prefixes.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int instprefixTag(Header h, /*@null@*/ /*@out@*/ rpmTagType * type,
		/*@null@*/ /*@out@*/ const void ** data,
		/*@null@*/ /*@out@*/ int_32 * count,
		/*@null@*/ /*@out@*/ int * freeData)
	/*@modifies *type, *data, *freeData @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(data) >= 0
		/\ maxSet(count) >= 0 /\ maxSet(freeData) >= 0 @*/
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    rpmTagType ipt;
    char ** array;

    if (hge(h, RPMTAG_INSTALLPREFIX, type, data, count)) {
	if (freeData) *freeData = 0;
	return 0;
    } else if (hge(h, RPMTAG_INSTPREFIXES, &ipt, &array, count)) {
	if (type) *type = RPM_STRING_TYPE;
/*@-boundsread@*/
	if (data) *data = xstrdup(array[0]);
/*@=boundsread@*/
	if (freeData) *freeData = 1;
	array = hfd(array, ipt);
	return 0;
    }

    return 1;
}

/**
 * Retrieve trigger info.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int triggercondsTag(Header h, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@modifies *type, *data, *count, *freeData @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(data) >= 0
		/\ maxSet(count) >= 0 /\ maxSet(freeData) >= 0 @*/
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    rpmTagType tnt, tvt, tst;
    int_32 * indices, * flags;
    char ** names, ** versions;
    int numNames, numScripts;
    char ** conds, ** s;
    char * item, * flagsStr;
    char * chptr;
    int i, j, xx;
    char buf[5];

    if (!hge(h, RPMTAG_TRIGGERNAME, &tnt, &names, &numNames)) {
	*freeData = 0;
	return 0;
    }

    xx = hge(h, RPMTAG_TRIGGERINDEX, NULL, &indices, NULL);
    xx = hge(h, RPMTAG_TRIGGERFLAGS, NULL, &flags, NULL);
    xx = hge(h, RPMTAG_TRIGGERVERSION, &tvt, &versions, NULL);
    xx = hge(h, RPMTAG_TRIGGERSCRIPTS, &tst, &s, &numScripts);
    s = hfd(s, tst);

    *freeData = 1;
    *data = conds = xmalloc(sizeof(*conds) * numScripts);
    *count = numScripts;
    *type = RPM_STRING_ARRAY_TYPE;
/*@-bounds@*/
    for (i = 0; i < numScripts; i++) {
	chptr = xstrdup("");

	for (j = 0; j < numNames; j++) {
	    if (indices[j] != i)
		/*@innercontinue@*/ continue;

	    item = xmalloc(strlen(names[j]) + strlen(versions[j]) + 20);
	    if (flags[j] & RPMSENSE_SENSEMASK) {
		buf[0] = '%', buf[1] = '\0';
		flagsStr = depflagsFormat(RPM_INT32_TYPE, flags, buf, 0, j);
		sprintf(item, "%s %s %s", names[j], flagsStr, versions[j]);
		flagsStr = _free(flagsStr);
	    } else {
		strcpy(item, names[j]);
	    }

	    chptr = xrealloc(chptr, strlen(chptr) + strlen(item) + 5);
	    if (*chptr != '\0') strcat(chptr, ", ");
	    strcat(chptr, item);
	    item = _free(item);
	}

	conds[i] = chptr;
    }
/*@=bounds@*/

    names = hfd(names, tnt);
    versions = hfd(versions, tvt);

    return 0;
}

/**
 * Retrieve trigger type info.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int triggertypeTag(Header h, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@modifies *type, *data, *count, *freeData @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(data) >= 0
		/\ maxSet(count) >= 0 /\ maxSet(freeData) >= 0 @*/
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    rpmTagType tst;
    int_32 * indices, * flags;
    const char ** conds;
    const char ** s;
    int i, j, xx;
    int numScripts, numNames;

    if (!hge(h, RPMTAG_TRIGGERINDEX, NULL, &indices, &numNames)) {
	*freeData = 0;
	return 1;
    }

    xx = hge(h, RPMTAG_TRIGGERFLAGS, NULL, &flags, NULL);
    xx = hge(h, RPMTAG_TRIGGERSCRIPTS, &tst, &s, &numScripts);
    s = hfd(s, tst);

    *freeData = 1;
    *data = conds = xmalloc(sizeof(*conds) * numScripts);
    *count = numScripts;
    *type = RPM_STRING_ARRAY_TYPE;
/*@-bounds@*/
    for (i = 0; i < numScripts; i++) {
	for (j = 0; j < numNames; j++) {
	    if (indices[j] != i)
		/*@innercontinue@*/ continue;

	    if (flags[j] & RPMSENSE_TRIGGERPREIN)
		conds[i] = xstrdup("prein");
	    else if (flags[j] & RPMSENSE_TRIGGERIN)
		conds[i] = xstrdup("in");
	    else if (flags[j] & RPMSENSE_TRIGGERUN)
		conds[i] = xstrdup("un");
	    else if (flags[j] & RPMSENSE_TRIGGERPOSTUN)
		conds[i] = xstrdup("postun");
	    else
		conds[i] = xstrdup("");
	    /*@innerbreak@*/ break;
	}
    }
/*@=bounds@*/

    return 0;
}

/* I18N look aside diversions */

#if defined(ENABLE_NLS)
/*@-exportlocal -exportheadervar@*/
/*@unchecked@*/
extern int _nl_msg_cat_cntr;	/* XXX GNU gettext voodoo */
/*@=exportlocal =exportheadervar@*/
#endif
/*@observer@*/ /*@unchecked@*/
static const char * language = "LANGUAGE";

/*@observer@*/ /*@unchecked@*/
static const char * _macro_i18ndomains = "%{?_i18ndomains}";

/**
 * Retrieve i18n text.
 * @param h		header
 * @param tag		tag
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int i18nTag(Header h, int_32 tag, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@globals rpmGlobalMacroContext, h_errno @*/
	/*@modifies *type, *data, *count, *freeData, rpmGlobalMacroContext @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(data) >= 0
		/\ maxSet(count) >= 0 /\ maxSet(freeData) >= 0 @*/
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    char * dstring = rpmExpand(_macro_i18ndomains, NULL);
    int rc;

    *type = RPM_STRING_TYPE;
    *data = NULL;
    *count = 0;
    *freeData = 0;

    if (dstring && *dstring) {
	char *domain, *de;
	const char * langval;
	const char * msgkey;
	const char * msgid;

	{   const char * tn = tagName(tag);
	    const char * n = NULL;
	    char * mk;
	    size_t nb = sizeof("()");
	    int xx = headerNVR(h, &n, NULL, NULL);
	    xx = 0;	/* XXX keep gcc quiet */
	    if (tn)	nb += strlen(tn);
	    if (n)	nb += strlen(n);
	    mk = alloca(nb);
	    sprintf(mk, "%s(%s)", (n?n:""), (tn?tn:""));
	    msgkey = mk;
	}

	/* change to en_US for msgkey -> msgid resolution */
	langval = getenv(language);
	(void) setenv(language, "en_US", 1);
#if defined(ENABLE_NLS)
/*@i@*/	++_nl_msg_cat_cntr;
#endif

	msgid = NULL;
	/*@-branchstate@*/
	for (domain = dstring; domain != NULL; domain = de) {
	    de = strchr(domain, ':');
	    if (de) *de++ = '\0';
	    msgid = /*@-unrecog@*/ dgettext(domain, msgkey) /*@=unrecog@*/;
	    if (msgid != msgkey) break;
	}
	/*@=branchstate@*/

	/* restore previous environment for msgid -> msgstr resolution */
	if (langval)
	    (void) setenv(language, langval, 1);
	else
	    unsetenv(language);
#if defined(ENABLE_NLS)
/*@i@*/	++_nl_msg_cat_cntr;
#endif

	if (domain && msgid) {
	    *data = /*@-unrecog@*/ dgettext(domain, msgid) /*@=unrecog@*/;
	    *data = xstrdup(*data);	/* XXX xstrdup has side effects. */
	    *count = 1;
	    *freeData = 1;
	}
	dstring = _free(dstring);
	if (*data)
	    return 0;
    }

    dstring = _free(dstring);

    rc = hge(h, tag, type, (void **)data, count);

    if (rc && (*data) != NULL) {
	*data = xstrdup(*data);
	*data = xstrtolocale(*data);
	*freeData = 1;
	return 0;
    }

    *freeData = 0;
    *data = NULL;
    *count = 0;
    return 1;
}

/**
 * Retrieve text and convert to locale.
 */
static int localeTag(Header h, int_32 tag, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@modifies *type, *data, *count, *freeData @*/
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    rpmTagType t;
    char **d, **d2, *dp;
    int rc, i, l;

    rc = hge(h, tag, &t, &d, count);
    if (!rc || d == NULL || *count == 0) {
	*freeData = 0;
	*data = NULL;
	*count = 0;
	return 1;
    }
    if (type)
	*type = t;
    if (t == RPM_STRING_TYPE) {
	d = (char **)xstrdup((char *)d);
	d = (char **)xstrtolocale((char *)d);
	*freeData = 1;
    } else if (t == RPM_STRING_ARRAY_TYPE) {
	l = 0;
	for (i = 0; i < *count; i++) {
	    d[i] = xstrdup(d[i]);
	    d[i] = (char *)xstrtolocale(d[i]);
assert(d[i] != NULL);
	    l += strlen(d[i]) + 1;
	}
	d2 = xmalloc(*count * sizeof(*d2) + l);
	dp = (char *)(d2 + *count);
	for (i = 0; i < *count; i++) {
	    d2[i] = dp;
	    strcpy(dp, d[i]);
	    dp += strlen(dp) + 1;
	    d[i] = _free(d[i]);
	}
	d = _free(d);
	d = d2;
	*freeData = 1;
    } else
	*freeData = 0;
    *data = (void **)d;
    return 0;
}

/**
 * Retrieve summary text.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int summaryTag(Header h, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@globals rpmGlobalMacroContext, h_errno @*/
	/*@modifies *type, *data, *count, *freeData, rpmGlobalMacroContext @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(data) >= 0
		/\ maxSet(count) >= 0 /\ maxSet(freeData) >= 0 @*/
{
    return i18nTag(h, RPMTAG_SUMMARY, type, data, count, freeData);
}

/**
 * Retrieve description text.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int descriptionTag(Header h, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@globals rpmGlobalMacroContext, h_errno @*/
	/*@modifies *type, *data, *count, *freeData, rpmGlobalMacroContext @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(data) >= 0
		/\ maxSet(count) >= 0 /\ maxSet(freeData) >= 0 @*/
{
    return i18nTag(h, RPMTAG_DESCRIPTION, type, data, count, freeData);
}

static int changelognameTag(Header h, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@modifies *type, *data, *count, *freeData @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(data) >= 0
		/\ maxSet(count) >= 0 /\ maxSet(freeData) >= 0 @*/
{
    return localeTag(h, RPMTAG_CHANGELOGNAME, type, data, count, freeData);
}

static int changelogtextTag(Header h, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@modifies *type, *data, *count, *freeData @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(data) >= 0
		/\ maxSet(count) >= 0 /\ maxSet(freeData) >= 0 @*/
{
    return localeTag(h, RPMTAG_CHANGELOGTEXT, type, data, count, freeData);
}

/**
 * Retrieve group text.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int groupTag(Header h, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@globals rpmGlobalMacroContext, h_errno @*/
	/*@modifies *type, *data, *count, *freeData, rpmGlobalMacroContext @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(data) >= 0
		/\ maxSet(count) >= 0 /\ maxSet(freeData) >= 0 @*/
{
    return i18nTag(h, RPMTAG_GROUP, type, data, count, freeData);
}

/**
 * Retrieve db instance from header.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int dbinstanceTag(Header h, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@globals rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies *type, *data, *count, *freeData, rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(data) >= 0
		/\ maxSet(count) >= 0 /\ maxSet(freeData) >= 0 @*/
{
    int_32 * valuep = xcalloc(1, sizeof(*valuep));

    *valuep = headerGetInstance(h);

    *type = RPM_INT32_TYPE;
    *data = valuep;
    *count = 1;
    *freeData = 1;

    return 0;
}

/**
 * Return (malloc'd) header name-version-release.arch string.
 * @param h		header
 * @return		name-version-release.arch string
 */
/*@only@*/
static char * hGetNVRA(Header h)
	/*@modifies *np @*/
{
    const char * N = NULL;
    const char * V = NULL;
    const char * R = NULL;
    const char * A = NULL;
    size_t nb = 0;
    char * NVRA, * t;

    (void) headerNEVRA(h, &N, NULL, &V, &R, &A);
    if (N)	nb += strlen(N) + 1;
    if (V)	nb += strlen(V) + 1;
    if (R)	nb += strlen(R) + 1;
    if (A)	nb += strlen(A) + 1;
    NVRA = t = xcalloc(1, nb);
/*@-boundswrite@*/
    if (N)	t = stpcpy(t, N);
    if (V)	t = stpcpy( stpcpy(t, "-"), V);
    if (R)	t = stpcpy( stpcpy(t, "-"), R);
    if (A)	t = stpcpy( stpcpy(t, "."), A);
/*@=boundswrite@*/
    return NVRA;
}

/**
 * Retrieve N-V-R.A compound string from header.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int nvraTag(Header h, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@globals rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies *type, *data, *count, *freeData, rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(data) >= 0
		/\ maxSet(count) >= 0 /\ maxSet(freeData) >= 0 @*/
{
    *type = RPM_STRING_TYPE;
    *data = hGetNVRA(h);
    *count = 1;
    *freeData = 1;

    return 0;
}

/**
 * Retrieve file names from header.
 *
 * The representation of file names in package headers changed in rpm-4.0.
 * Originally, file names were stored as an array of absolute paths.
 * In rpm-4.0, file names are stored as separate arrays of dirname's and
 * basename's, * with a dirname index to associate the correct dirname
 * with each basname.
 *
 * This function is used to retrieve file names independent of how the
 * file names are represented in the package header.
 * 
 * @param h		header
 * @param tagN		RPMTAG_BASENAMES | PMTAG_ORIGBASENAMES
 * @retval *fnp		array of file names
 * @retval *fcp		number of files
 */
static void rpmfiBuildFNames(Header h, rpmTag tagN,
		/*@out@*/ const char *** fnp, /*@out@*/ int * fcp)
	/*@modifies *fnp, *fcp @*/
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    const char ** baseNames;
    const char ** dirNames;
    int * dirIndexes;
    int count;
    const char ** fileNames;
    int size;
    rpmTag dirNameTag = 0;
    rpmTag dirIndexesTag = 0;
    rpmTagType bnt, dnt;
    char * t;
    int i, xx;

    if (tagN == RPMTAG_BASENAMES) {
	dirNameTag = RPMTAG_DIRNAMES;
	dirIndexesTag = RPMTAG_DIRINDEXES;
    } else if (tagN == RPMTAG_ORIGBASENAMES) {
	dirNameTag = RPMTAG_ORIGDIRNAMES;
	dirIndexesTag = RPMTAG_ORIGDIRINDEXES;
    }

    if (!hge(h, tagN, &bnt, &baseNames, &count)) {
	if (fnp) *fnp = NULL;
	if (fcp) *fcp = 0;
	return;		/* no file list */
    }

    xx = hge(h, dirNameTag, &dnt, &dirNames, NULL);
    xx = hge(h, dirIndexesTag, NULL, &dirIndexes, &count);

    size = sizeof(*fileNames) * count;
    for (i = 0; i < count; i++) {
	const char * dn = NULL;
	(void) urlPath(dirNames[dirIndexes[i]], &dn);
	size += strlen(baseNames[i]) + strlen(dn) + 1;
    }

    fileNames = xmalloc(size);
    t = ((char *) fileNames) + (sizeof(*fileNames) * count);
    /*@-branchstate@*/
    for (i = 0; i < count; i++) {
	const char * dn = NULL;
	fileNames[i] = t;
	(void) urlPath(dirNames[dirIndexes[i]], &dn);
	t = stpcpy( stpcpy(t, dn), baseNames[i]);
	*t++ = '\0';
    }
    /*@=branchstate@*/
    baseNames = hfd(baseNames, bnt);
    dirNames = hfd(dirNames, dnt);

    /*@-branchstate@*/
    if (fnp)
	*fnp = fileNames;
    else
	fileNames = _free(fileNames);
    /*@=branchstate@*/
    if (fcp) *fcp = count;
}

/**
 * Retrieve file paths.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int _fnTag(Header h, rpmTag tag, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@modifies *type, *data, *count, *freeData @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(data) >= 0
		/\ maxSet(count) >= 0 /\ maxSet(freeData) >= 0 @*/
{
    *type = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildFNames(h, tag, (const char ***) data, count);
    *freeData = 1;
    return 0;
}

static int filenamesTag(Header h, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@modifies *type, *data, *count, *freeData @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(data) >= 0
		/\ maxSet(count) >= 0 /\ maxSet(freeData) >= 0 @*/
{
    return _fnTag(h, RPMTAG_BASENAMES, type, data, count, freedata);
}

static int origfilenamesTag(Header h, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@modifies *type, *data, *count, *freeData @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(data) >= 0
		/\ maxSet(count) >= 0 /\ maxSet(freeData) >= 0 @*/
{
    return _fnTag(h, RPMTAG_ORIGBASENAMES, type, data, count, freedata);
}

/*@-type@*/ /* FIX: cast? */
const struct headerSprintfExtension_s headerCompoundFormats[] = {
    { HEADER_EXT_TAG, "RPMTAG_CHANGELOGNAME",	{ changelognameTag } },
    { HEADER_EXT_TAG, "RPMTAG_CHANGELOGTEXT",	{ changelogtextTag } },
    { HEADER_EXT_TAG, "RPMTAG_DESCRIPTION",	{ descriptionTag } },
    { HEADER_EXT_TAG, "RPMTAG_GROUP",		{ groupTag } },
    { HEADER_EXT_TAG, "RPMTAG_INSTALLPREFIX",	{ instprefixTag } },
    { HEADER_EXT_TAG, "RPMTAG_SUMMARY",		{ summaryTag } },
    { HEADER_EXT_TAG, "RPMTAG_TRIGGERCONDS",	{ triggercondsTag } },
    { HEADER_EXT_TAG, "RPMTAG_TRIGGERTYPE",	{ triggertypeTag } },
    { HEADER_EXT_TAG, "RPMTAG_DBINSTANCE",	{ dbinstanceTag } },
    { HEADER_EXT_TAG, "RPMTAG_NVRA",		{ nvraTag } },
    { HEADER_EXT_TAG, "RPMTAG_FILENAMES",	{ filenamesTag } },
    { HEADER_EXT_TAG, "RPMTAG_ORIGFILENAMES",	{ filenamesTag } },
    { HEADER_EXT_FORMAT, "armor",		{ armorFormat } },
    { HEADER_EXT_FORMAT, "base64",		{ base64Format } },
    { HEADER_EXT_FORMAT, "depflags",		{ depflagsFormat } },
    { HEADER_EXT_FORMAT, "fflags",		{ fflagsFormat } },
    { HEADER_EXT_FORMAT, "perms",		{ permsFormat } },
    { HEADER_EXT_FORMAT, "permissions",		{ permsFormat } },
    { HEADER_EXT_FORMAT, "pgpsig",		{ pgpsigFormat } },
    { HEADER_EXT_FORMAT, "triggertype",		{ triggertypeFormat } },
    { HEADER_EXT_FORMAT, "xml",			{ xmlFormat } },
    { HEADER_EXT_FORMAT, "yaml",		{ yamlFormat } },
    { HEADER_EXT_MORE, NULL,		{ (void *) headerDefaultFormats } }
} ;
/*@=type@*/

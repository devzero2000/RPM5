/** \ingroup header
 * \file rpmdb/formats.c
 */

#include "system.h"

#include "rpmio_internal.h"
#include <header.h>
#include <rpmlib.h>	/* XXX RPMFILE_FOO, rpmMkdirPath */
#include <rpmmacro.h>	/* XXX for %_i18ndomains */

#define _RPMEVR_INTERNAL
#include <rpmevr.h>	/* XXX RPMSENSE_FOO */

#include "legacy.h"
#include "argv.h"
#include "misc.h"

#include "debug.h"

/*@access pgpDig @*/
/*@access pgpDigParams @*/

/* XXX FIXME: static for now, refactor from manifest.c later. */
static char * rpmPermsString(int mode)
	/*@*/
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

/**
 * Identify type of trigger.
 * @param he		tag container
 * @return		formatted string
 */
static /*@only@*/ char * triggertypeFormat(HE_t he)
{
    rpmTagData data = { .ptr = he->p.ptr };
    int ix = (he->ix > 0 ? he->ix : 0);
    char * val;

assert(ix == 0);
    if (he->t != RPM_UINT64_TYPE)
	val = xstrdup(_("(invalid type)"));
    else {
	uint64_t anint = data.ui64p[ix];
	if (anint & RPMSENSE_TRIGGERPREIN)
	    val = xstrdup("prein");
	else if (anint & RPMSENSE_TRIGGERIN)
	    val = xstrdup("in");
	else if (anint & RPMSENSE_TRIGGERUN)
	    val = xstrdup("un");
	else if (anint & RPMSENSE_TRIGGERPOSTUN)
	    val = xstrdup("postun");
	else
	    val = xstrdup("");
    }
    return val;
}

/**
 * Format file permissions for display.
 * @param he		tag container
 * @return		formatted string
 */
static /*@only@*/ char * permsFormat(HE_t he)
{
    int ix = (he->ix > 0 ? he->ix : 0);
    char * val;

assert(ix == 0);
    if (he->t != RPM_UINT64_TYPE) {
	val = xstrdup(_("(invalid type)"));
    } else {
	uint64_t anint = he->p.ui64p[0];
	val = rpmPermsString((int)anint);
    }

    return val;
}

/**
 * Format file flags for display.
 * @param he		tag container
 * @return		formatted string
 */
static /*@only@*/ char * fflagsFormat(HE_t he)
{
    rpmTagData data = { .ptr = he->p.ptr };
    int ix = (he->ix >= 0 ? he->ix : 0);
    char * val;

assert(ix == 0);
    if (he->t != RPM_UINT64_TYPE) {
	val = xstrdup(_("(invalid type)"));
    } else {
	char buf[15];
	uint64_t anint = data.ui64p[ix];
	buf[0] = '\0';
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
	val = xstrdup(buf);
    }

    return val;
}

/**
 * Wrap a pubkey in ascii armor for display.
 * @todo Permit selectable display formats (i.e. binary).
 * @param he		tag container
 * @return		formatted string
 */
static /*@only@*/ char * armorFormat(HE_t he)
	/*@*/
{
    rpmTagData data = { .ptr = he->p.ptr };
    int ix = (he->ix > 0 ? he->ix : 0);
    const char * enc;
    const unsigned char * s;
    size_t ns;
    int atype;
    char * val;

assert(ix == 0);
    switch (he->t) {
    case RPM_BIN_TYPE:
	s = (unsigned char *) data.ui8p;
	ns = he->c;
	atype = PGPARMOR_SIGNATURE;	/* XXX check pkt for signature */
	break;
    case RPM_STRING_TYPE:
    case RPM_STRING_ARRAY_TYPE:
	enc = data.str;
	s = NULL;
	ns = 0;
/*@-moduncon@*/
	if (b64decode(enc, (void **)&s, &ns))
	    return xstrdup(_("(not base64)"));
/*@=moduncon@*/
	atype = PGPARMOR_PUBKEY;	/* XXX check pkt for pubkey */
	break;
    case RPM_UINT8_TYPE:
    case RPM_UINT16_TYPE:
    case RPM_UINT32_TYPE:
    case RPM_UINT64_TYPE:
    case RPM_I18NSTRING_TYPE:
    default:
	return xstrdup(_("(invalid type)"));
	/*@notreached@*/ break;
    }

    val = pgpArmorWrap(atype, s, ns);
    if (atype == PGPARMOR_PUBKEY)
	s = _free(s);
    return val;
}

/**
 * Encode binary data in base64 for display.
 * @todo Permit selectable display formats (i.e. binary).
 * @param he		tag container
 * @return		formatted string
 */
static /*@only@*/ char * base64Format(HE_t he)
	/*@*/
{
    rpmTagData data = { .ptr = he->p.ptr };
    int ix = (he->ix > 0 ? he->ix : 0);
    char * val;

assert(ix == 0);
    if (!(he->t == RPM_BIN_TYPE)) {
	val = xstrdup(_("(not a blob)"));
    } else {
	const char * enc;
	char * t;
	int lc;
	size_t ns = he->c;
	size_t nt = ((ns + 2) / 3) * 4;

	/*@-globs@*/
	/* Add additional bytes necessary for eol string(s). */
	if (b64encode_chars_per_line > 0 && b64encode_eolstr != NULL) {
	    lc = (nt + b64encode_chars_per_line - 1) / b64encode_chars_per_line;
	if (((nt + b64encode_chars_per_line - 1) % b64encode_chars_per_line) != 0)
	    ++lc;
	    nt += lc * strlen(b64encode_eolstr);
	}
	/*@=globs@*/

	val = t = xcalloc(1, nt + 1);
	*t = '\0';

    /* XXX b64encode accesses uninitialized memory. */
    { 	unsigned char * _data = xcalloc(1, ns+1);
	memcpy(_data, data.ptr, ns);
/*@-moduncon@*/
	if ((enc = b64encode(_data, ns)) != NULL) {
	    t = stpcpy(t, enc);
	    enc = _free(enc);
	}
/*@=moduncon@*/
	_data = _free(_data);
    }
    }

/*@-globstate@*/
    return val;
/*@=globstate@*/
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

    while ((c = (int) *s++) != (int) '\0')
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

    while ((c = (int) *s++) != (int) '\0') {
	switch (c) {
	case '<':	te = stpcpy(te, "&lt;");	/*@switchbreak@*/ break;
	case '>':	te = stpcpy(te, "&gt;");	/*@switchbreak@*/ break;
	case '&':	te = stpcpy(te, "&amp;");	/*@switchbreak@*/ break;
	default:	*te++ = (char) c;		/*@switchbreak@*/ break;
	}
    }
    *te = '\0';
    return t;
}

/**
 * Wrap tag data in simple header xml markup.
 * @param he		tag container
 * @return		formatted string
 */
static /*@only@*/ char * xmlFormat(HE_t he)
	/*@*/
{
    rpmTagData data = { .ptr = he->p.ptr };
    int ix = (he->ix > 0 ? he->ix : 0);
    const char * xtag = NULL;
    size_t nb;
    char * val;
    const char * s = NULL;
    char * t, * te;
    uint64_t anint = 0;
    int freeit = 0;
    int xx;

assert(ix == 0);
assert(he->t == RPM_STRING_TYPE || he->t == RPM_UINT64_TYPE || he->t == RPM_BIN_TYPE);
    switch (he->t) {
    case RPM_STRING_ARRAY_TYPE:
	s = data.argv[ix];
	xtag = "string";
	/* XXX Force utf8 strings. */
	s = xstrdup(s);
	s = xstrtolocale(s);
	freeit = 1;
	break;
    case RPM_I18NSTRING_TYPE:
    case RPM_STRING_TYPE:
	s = data.str;
	xtag = "string";
	/* XXX Force utf8 strings. */
	s = xstrdup(s);
	s = xstrtolocale(s);
	freeit = 1;
	break;
    case RPM_BIN_TYPE:
/*@-globs -mods@*/
    {	int cpl = b64encode_chars_per_line;
	b64encode_chars_per_line = 0;
/*@-formatconst@*/
	s = base64Format(he);
/*@=formatconst@*/
	b64encode_chars_per_line = cpl;
	xtag = "base64";
	freeit = 1;
    }	break;
/*@=globs =mods@*/
    case RPM_UINT8_TYPE:
	anint = data.ui8p[ix];
	break;
    case RPM_UINT16_TYPE:
	anint = data.ui16p[ix];	/* XXX note unsigned */
	break;
    case RPM_UINT32_TYPE:
	anint = data.ui32p[ix];
	break;
    case RPM_UINT64_TYPE:
	anint = data.ui64p[ix];
	break;
    default:
	return xstrdup(_("(invalid xml type)"));
	/*@notreached@*/ break;
    }

    if (s == NULL) {
	int tlen = 64;
	t = memset(alloca(tlen+1), 0, tlen+1);
/*@-duplicatequals@*/
	if (anint != 0)
	    xx = snprintf(t, tlen, "%llu", (unsigned long long)anint);
/*@=duplicatequals@*/
	s = t;
	xtag = "integer";
    }

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

    if (freeit)
	s = _free(s);

    val = xstrdup(t);

    return val;
}

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

    while ((c = (int) *s++) != (int) '\0')
    {
	if (indent) {
	    len += 2 * lvl;
	    indent = 0;
	}
	if (c == (int) '\n')
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

    while ((c = (int) *s++) != (int) '\0') {
	if (indent) {
	    int i;
	    for (i = 0; i < lvl; i++) {
		*te++ = ' ';
		*te++ = ' ';
	    }
	    indent = 0;
	}
	if (c == (int) '\n')
	    indent = (lvl > 0);
	*te++ = (char) c;
    }
    *te = '\0';
    return t;
}

/**
 * Wrap tag data in simple header yaml markup.
 * @param he		tag container
 * @return		formatted string
 */
static /*@only@*/ char * yamlFormat(HE_t he)
	/*@*/
{
    rpmTagData data = { .ptr = he->p.ptr };
    int element = he->ix;
    int ix = (he->ix > 0 ? he->ix : 0);
    const char * xtag = NULL;
    const char * ytag = NULL;
    size_t nb;
    char * val;
    const char * s = NULL;
    char * t, * te;
    uint64_t anint = 0;
    int freeit = 0;
    int lvl = 0;
    int xx;
    int c;

assert(ix == 0);
assert(he->t == RPM_STRING_TYPE || he->t == RPM_UINT64_TYPE || he->t == RPM_BIN_TYPE);
    switch (he->t) {
    case RPM_STRING_ARRAY_TYPE:
    case RPM_I18NSTRING_TYPE:
    case RPM_STRING_TYPE:
	xx = 0;
	s = (he->t == RPM_STRING_ARRAY_TYPE ? data.argv[ix] : data.str);
	if (strchr("[", s[0]))	/* leading [ */
	    xx = 1;
	if (xx == 0)
	while ((c = (int) *s++) != (int) '\0') {
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
	s = xstrdup(data.str);
	s = xstrtolocale(s);
	freeit = 1;
	break;
    case RPM_BIN_TYPE:
/*@-globs -mods@*/
    {	int cpl = b64encode_chars_per_line;
	b64encode_chars_per_line = 0;
/*@-formatconst@*/
	s = base64Format(he);
	element = -element; 	/* XXX skip "    " indent. */
/*@=formatconst@*/
	b64encode_chars_per_line = cpl;
	xtag = "!!binary ";
	freeit = 1;
    }	break;
/*@=globs =mods@*/
    case RPM_UINT8_TYPE:
	anint = data.ui8p[ix];
	break;
    case RPM_UINT16_TYPE:
	anint = data.ui16p[ix];	/* XXX note unsigned */
	break;
    case RPM_UINT32_TYPE:
	anint = data.ui32p[ix];
	break;
    case RPM_UINT64_TYPE:
	anint = data.ui64p[ix];
	break;
    default:
	return xstrdup(_("(invalid yaml type)"));
	/*@notreached@*/ break;
    }

    if (s == NULL) {
	int tlen = 64;
	t = memset(alloca(tlen+1), 0, tlen+1);
/*@-duplicatequals@*/
	xx = snprintf(t, tlen, "%llu", (unsigned long long)anint);
/*@=duplicatequals@*/
	s = t;
	xtag = (element >= 0 ? "- " : NULL);
    }

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
    if (freeit)
	s = _free(s);

    val = xstrdup(t);

    return val;
}

/**
 * Display signature fingerprint and time.
 * @param he		tag container
 * @return		formatted string
 */
static /*@only@*/ char * pgpsigFormat(HE_t he)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    rpmTagData data = { .ptr = he->p.ptr };
    int ix = (he->ix > 0 ? he->ix : 0);
    char * val, * t;

assert(ix == 0);
    if (!(he->t == RPM_BIN_TYPE)) {
	val = xstrdup(_("(not a blob)"));
    } else {
	unsigned char * pkt = (unsigned char *) data.ui8p;
	unsigned int pktlen = 0;
	unsigned int v = (unsigned int) *pkt;
	pgpTag tag = 0;
	unsigned int plen;
	unsigned int hlen = 0;

	if (v & 0x80) {
	    if (v & 0x40) {
		tag = (v & 0x3f);
		plen = pgpLen((byte *)pkt+1, &hlen);
	    } else {
		tag = (v >> 2) & 0xf;
		plen = (1 << (v & 0x3));
		hlen = pgpGrab((byte *)pkt+1, plen);
	    }
	
	    pktlen = 1 + plen + hlen;
	}

	if (pktlen == 0 || tag != PGPTAG_SIGNATURE) {
	    val = xstrdup(_("(not an OpenPGP signature)"));
	} else {
	    pgpDig dig = pgpDigNew(0);
	    pgpDigParams sigp = pgpGetSignature(dig);
	    size_t nb = 0;
	    const char *tempstr;

	    (void) pgpPrtPkts((byte *)pkt, pktlen, dig, 0);

	    val = NULL;
	again:
	    nb += 100;
	    val = t = xrealloc(val, nb + 1);

	    switch (sigp->pubkey_algo) {
	    case PGPPUBKEYALGO_DSA:
		t = stpcpy(t, "DSA");
		break;
	    case PGPPUBKEYALGO_RSA:
		t = stpcpy(t, "RSA");
		break;
	    default:
		(void) snprintf(t, nb - (t - val), "%u", (unsigned)sigp->pubkey_algo);
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
		(void) snprintf(t, nb - (t - val), "%u", (unsigned)sigp->hash_algo);
		t += strlen(t);
		break;
	    }
	    if (t + strlen (", ") + 1 >= val + nb)
		goto again;

	    t = stpcpy(t, ", ");

	    /* this is important if sizeof(uint32_t) ! sizeof(time_t) */
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

	    dig = pgpDigFree(dig);
	}
    }

    return val;
}

/**
 * Format dependency flags for display.
 * @param he		tag container
 * @return		formatted string
 */
static /*@only@*/ char * depflagsFormat(HE_t he)
	/*@*/
{
    rpmTagData data = { .ptr = he->p.ptr };
    int ix = (he->ix > 0 ? he->ix : 0);
    char * val;

assert(ix == 0);
    if (he->t != RPM_UINT64_TYPE) {
	val = xstrdup(_("(invalid type)"));
    } else {
	uint64_t anint = data.ui64p[ix];
	char *t, *buf;

	t = buf = alloca(32);
	*t = '\0';

#ifdef	NOTYET	/* XXX appending markers breaks :depflags format. */
	if (anint & RPMSENSE_SCRIPT_PRE)
	    t = stpcpy(t, "(pre)");
	else if (anint & RPMSENSE_SCRIPT_POST)
	    t = stpcpy(t, "(post)");
	else if (anint & RPMSENSE_SCRIPT_PREUN)
	    t = stpcpy(t, "(preun)");
	else if (anint & RPMSENSE_SCRIPT_POSTUN)
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

	val = xstrdup(buf);
    }

    return val;
}

/**
 * Retrieve install prefixes.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int instprefixTag(Header h, HE_t he)
	/*@modifies he @*/
{
    rpmTagType ipt;
    rpmTagData array;

    he->tag = RPMTAG_INSTALLPREFIX;
    if (headerGetEntry(h, RPMTAG_INSTALLPREFIX, &he->t, &he->p, &he->c)) {
	he->freeData = 0;
	return 0;
    }
    he->tag = RPMTAG_INSTPREFIXES;
    if (headerGetEntry(h, he->tag, &ipt, &array, &he->c)) {
	he->t = RPM_STRING_TYPE;
	he->c = 1;
	he->p.str = xstrdup(array.argv[0]);
	he->freeData = 1;
	array.ptr = headerFreeData(array.ptr, ipt);
	return 0;
    }
    return 1;
}

/**
 * Retrieve trigger info.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int triggercondsTag(Header h, HE_t he)
	/*@modifies he @*/
{
    HE_t _he = memset(alloca(sizeof(*_he)), 0, sizeof(*_he));
    rpmTagData flags;
    rpmTagData indices;
    rpmTagData names;
    rpmTagData versions;
    const char ** conds;
    rpmTagData s;
    char * item, * flagsStr;
    char * chptr;
    uint32_t numNames, numScripts;
    unsigned i, j;
    int xx;

    he->freeData = 0;
    xx = headerGetEntry(h, RPMTAG_TRIGGERNAME, NULL, &names, &numNames);
    if (!xx)
	return 0;

    _he->tag = he->tag;
    _he->t = RPM_UINT32_TYPE;
    _he->p.ui32p = NULL;
    _he->c = 1;
    _he->freeData = -1;

    xx = headerGetEntry(h, RPMTAG_TRIGGERINDEX, NULL, &indices, NULL);
    xx = headerGetEntry(h, RPMTAG_TRIGGERFLAGS, NULL, &flags, NULL);
    xx = headerGetEntry(h, RPMTAG_TRIGGERVERSION, NULL, &versions, NULL);
    xx = headerGetEntry(h, RPMTAG_TRIGGERSCRIPTS, NULL, &s, &numScripts);

    he->t = RPM_STRING_ARRAY_TYPE;
    he->c = numScripts;

    he->freeData = 1;
    he->p.argv = conds = xmalloc(sizeof(*conds) * numScripts);
    for (i = 0; i < (unsigned) numScripts; i++) {
	chptr = xstrdup("");

	for (j = 0; j < (unsigned) numNames; j++) {
	    if (indices.ui32p[j] != i)
		/*@innercontinue@*/ continue;

	    item = xmalloc(strlen(names.argv[j]) + strlen(versions.argv[j]) + 20);
	    if (flags.ui32p[j] & RPMSENSE_SENSEMASK) {
		_he->p.ui32p = &flags.ui32p[j];
		flagsStr = depflagsFormat(_he);
		sprintf(item, "%s %s %s", names.argv[j], flagsStr, versions.argv[j]);
		flagsStr = _free(flagsStr);
	    } else
		strcpy(item, names.argv[j]);

	    chptr = xrealloc(chptr, strlen(chptr) + strlen(item) + 5);
	    if (*chptr != '\0') strcat(chptr, ", ");
	    strcat(chptr, item);
	    item = _free(item);
	}

	conds[i] = chptr;
    }

    names.ptr = headerFreeData(names.ptr, -1);
    versions.ptr = headerFreeData(versions.ptr, -1);
    s.ptr = headerFreeData(s.ptr, -1);

    return 0;
}

/**
 * Retrieve trigger type info.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int triggertypeTag(Header h, HE_t he)
	/*@modifies he @*/
{
    rpmTagData indices;
    rpmTagData flags;
    const char ** conds;
    rpmTagData s;
    uint32_t numScripts, numNames;
    unsigned i, j;
    int xx;

    he->freeData = 0;
    if (!headerGetEntry(h, RPMTAG_TRIGGERINDEX, NULL, &indices, &numNames))
	return 1;

    xx = headerGetEntry(h, RPMTAG_TRIGGERFLAGS, NULL, &flags, NULL);
    xx = headerGetEntry(h, RPMTAG_TRIGGERSCRIPTS, NULL, &s, &numScripts);

    he->t = RPM_STRING_ARRAY_TYPE;
    he->c = numScripts;

    he->freeData = 1;
    he->p.argv = conds = xmalloc(sizeof(*conds) * numScripts);
    for (i = 0; i < (unsigned) numScripts; i++) {
	for (j = 0; j < (unsigned) numNames; j++) {
	    if (indices.ui32p[j] != i)
		/*@innercontinue@*/ continue;

	    if (flags.ui32p[j] & RPMSENSE_TRIGGERPREIN)
		conds[i] = xstrdup("prein");
	    else if (flags.ui32p[j] & RPMSENSE_TRIGGERIN)
		conds[i] = xstrdup("in");
	    else if (flags.ui32p[j] & RPMSENSE_TRIGGERUN)
		conds[i] = xstrdup("un");
	    else if (flags.ui32p[j] & RPMSENSE_TRIGGERPOSTUN)
		conds[i] = xstrdup("postun");
	    else
		conds[i] = xstrdup("");
	    /*@innerbreak@*/ break;
	}
    }

    s.ptr = headerFreeData(s.ptr, -1);
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
 * @retval *he		tag container
 * @return		0 on success
 */
static int i18nTag(Header h, HE_t he)
	/*@globals rpmGlobalMacroContext, h_errno @*/
	/*@modifies he, rpmGlobalMacroContext @*/
{
    char * dstring = rpmExpand(_macro_i18ndomains, NULL);
    int rc = 1;		/* assume failure */

    he->t = RPM_STRING_TYPE;
    he->p.str = NULL;
    he->c = 0;
    he->freeData = 0;

    if (dstring && *dstring) {
	char *domain, *de;
	const char * langval;
	const char * msgkey;
	const char * msgid;

	{   const char * tn = tagName(he->tag);
	    rpmTagData n = { .ptr = NULL };
	    char * mk;
	    size_t nb = sizeof("()");
	    int xx = headerGetEntry(h, RPMTAG_NAME, NULL, &n, NULL);
	    xx = 0;	/* XXX keep gcc quiet */
	    if (tn)	nb += strlen(tn);
	    if (n.str)	nb += strlen(n.str);
	    mk = alloca(nb);
	    sprintf(mk, "%s(%s)", (n.str ? n.str : ""), (tn ? tn : ""));
	    msgkey = mk;
	}

	/* change to en_US for msgkey -> msgid resolution */
	langval = getenv(language);
	(void) setenv(language, "en_US", 1);
#if defined(ENABLE_NLS)
/*@i@*/	++_nl_msg_cat_cntr;
#endif

	msgid = NULL;
	for (domain = dstring; domain != NULL; domain = de) {
	    de = strchr(domain, ':');
	    if (de) *de++ = '\0';
	    msgid = /*@-unrecog@*/ dgettext(domain, msgkey) /*@=unrecog@*/;
	    if (msgid != msgkey) break;
	}

	/* restore previous environment for msgid -> msgstr resolution */
	if (langval)
	    (void) setenv(language, langval, 1);
	else
	    unsetenv(language);
#if defined(ENABLE_NLS)
/*@i@*/	++_nl_msg_cat_cntr;
#endif

	if (domain && msgid) {
	    const char * s = /*@-unrecog@*/ dgettext(domain, msgid) /*@=unrecog@*/;
	    if (s) {
		rc = 0;
		he->p.str = xstrdup(s);
		he->c = 1;
		he->freeData = 1;
	    }
	}
    }

/*@-dependenttrans@*/
    dstring = _free(dstring);
/*@=dependenttrans@*/
    if (!rc)
	return rc;

    rc = headerGetEntry(h, he->tag, &he->t, &he->p, &he->c);
    if (rc) {
	rc = 0;
	he->p.str = xstrdup(he->p.str);
	he->p.str = xstrtolocale(he->p.str);
	he->freeData = 1;
/*@-nullstate@*/
	return rc;
/*@=nullstate@*/
    }

    he->t = RPM_STRING_TYPE;
    he->p.str = NULL;
    he->c = 0;
    he->freeData = 0;

    return 1;
}

/**
 * Retrieve text and convert to locale.
 */
static int localeTag(Header h, HE_t he)
	/*@modifies he @*/
{
    rpmTagType t;
    rpmTagData p;
    rpmTagCount c;
    const char ** argv;
    char * te;
    int rc;

    rc = headerGetEntry(h, he->tag, &t, &p, &c);
    if (!rc || p.ptr == NULL || c == 0) {
	he->t = RPM_STRING_TYPE;
	he->p.str = NULL;
	he->c = 0;
	he->freeData = 0;
	return 1;
    }

    if (t == RPM_STRING_TYPE) {
	p.str = xstrdup(p.str);
	p.str = xstrtolocale(p.str);
	he->freeData = 1;
    } else if (t == RPM_STRING_ARRAY_TYPE) {
	size_t l = 0;
	unsigned i;
	for (i = 0; i < (unsigned)c; i++) {
	    p.argv[i] = xstrdup(p.argv[i]);
	    p.argv[i] = xstrtolocale(p.argv[i]);
assert(p.argv[i] != NULL);
	    l += strlen(p.argv[i]) + 1;
	}
	argv = xmalloc(c * sizeof(*argv) + l);
	te = (char *)&argv[c];
	for (i = 0; i < (unsigned) c; i++) {
	    argv[i] = te;
	    te = stpcpy(te, p.argv[i]);
	    te++;
	    p.argv[i] = _free(p.argv[i]);
	}
	p.ptr = _free(p.ptr);
	p.argv = argv;
	he->freeData = 1;
    } else
	he->freeData = 0;

    he->t = t;
    he->p.ptr = p.ptr;
    he->c = c;
    return 0;
}

/**
 * Retrieve summary text.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int summaryTag(Header h, HE_t he)
	/*@globals rpmGlobalMacroContext, h_errno @*/
	/*@modifies he, rpmGlobalMacroContext @*/
{
    he->tag = RPMTAG_SUMMARY;
    return i18nTag(h, he);
}

/**
 * Retrieve description text.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int descriptionTag(Header h, HE_t he)
	/*@globals rpmGlobalMacroContext, h_errno @*/
	/*@modifies he, rpmGlobalMacroContext @*/
{
    he->tag = RPMTAG_DESCRIPTION;
    return i18nTag(h, he);
}

static int changelognameTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->tag = RPMTAG_CHANGELOGNAME;
    return localeTag(h, he);
}

static int changelogtextTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->tag = RPMTAG_CHANGELOGTEXT;
    return localeTag(h, he);
}

/**
 * Retrieve group text.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int groupTag(Header h, HE_t he)
	/*@globals rpmGlobalMacroContext, h_errno @*/
	/*@modifies he, rpmGlobalMacroContext @*/
{
    he->tag = RPMTAG_GROUP;
    return i18nTag(h, he);
}

/**
 * Retrieve db instance from header.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
/*@-globuse@*/
static int dbinstanceTag(Header h, HE_t he)
	/*@globals rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies he, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    he->tag = RPMTAG_DBINSTANCE;
    he->t = RPM_UINT32_TYPE;
    he->p.ui32p = xmalloc(sizeof(*he->p.ui32p));
    he->p.ui32p[0] = headerGetInstance(h);
    he->freeData = 1;
    he->c = 1;
    return 0;
}
/*@=globuse@*/

/**
 * Return (malloc'd) header name-version-release.arch string.
 * @param h		header
 * @return		name-version-release.arch string
 */
/*@only@*/
static char * hGetNVRA(Header h)
	/*@modifies h @*/
{
    const char * N = NULL;
    const char * V = NULL;
    const char * R = NULL;
    const char * A = NULL;
    size_t nb = 0;
    char * NVRA, * t;

    (void) headerNEVRA(h, &N, NULL, &V, &R, &A);
    if (N)	nb += strlen(N);
    if (V)	nb += strlen(V) + 1;
    if (R)	nb += strlen(R) + 1;
    if (A)	nb += strlen(A) + 1;
    nb++;
    NVRA = t = xmalloc(nb);
    *t = '\0';
    if (N)	t = stpcpy(t, N);
    if (V)	t = stpcpy( stpcpy(t, "-"), V);
    if (R)	t = stpcpy( stpcpy(t, "-"), R);
    if (A)	t = stpcpy( stpcpy(t, "."), A);
    return NVRA;
}

/**
 * Retrieve N-V-R.A compound string from header.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
/*@-globuse@*/
static int nvraTag(Header h, HE_t he)
	/*@globals rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies h, he, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    he->t = RPM_STRING_TYPE;
    he->p.str = hGetNVRA(h);
    he->c = 1;
    he->freeData = 1;
    return 0;
}
/*@=globuse@*/

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
		/*@null@*/ /*@out@*/ const char *** fnp,
		/*@null@*/ /*@out@*/ rpmTagCount * fcp)
	/*@modifies *fnp, *fcp @*/
{
    rpmTagData baseNames;
    rpmTagData dirNames;
    rpmTagData dirIndexes;
    rpmTagData fileNames;
    rpmTagCount count;
    size_t size;
    rpmTag dirNameTag = 0;
    rpmTag dirIndexesTag = 0;
    rpmTagType bnt, dnt;
    char * t;
    unsigned i;
    int xx;

    if (tagN == RPMTAG_BASENAMES) {
	dirNameTag = RPMTAG_DIRNAMES;
	dirIndexesTag = RPMTAG_DIRINDEXES;
    } else if (tagN == RPMTAG_ORIGBASENAMES) {
	dirNameTag = RPMTAG_ORIGDIRNAMES;
	dirIndexesTag = RPMTAG_ORIGDIRINDEXES;
    }

    if (!headerGetEntry(h, tagN, &bnt, &baseNames, &count)) {
	if (fnp) *fnp = NULL;
	if (fcp) *fcp = 0;
	return;		/* no file list */
    }

    xx = headerGetEntry(h, dirNameTag, &dnt, &dirNames, NULL);
    xx = headerGetEntry(h, dirIndexesTag, NULL, &dirIndexes, &count);

    size = sizeof(*fileNames.argv) * count;
    for (i = 0; i < (unsigned)count; i++) {
	const char * dn = NULL;
	(void) urlPath(dirNames.argv[dirIndexes.ui32p[i]], &dn);
	size += strlen(baseNames.argv[i]) + strlen(dn) + 1;
    }

    fileNames.argv = xmalloc(size);
    t = (char *)&fileNames.argv[count];
    for (i = 0; i < (unsigned)count; i++) {
	const char * dn = NULL;
	(void) urlPath(dirNames.argv[dirIndexes.ui32p[i]], &dn);
	fileNames.argv[i] = t;
	t = stpcpy( stpcpy(t, dn), baseNames.argv[i]);
	*t++ = '\0';
    }
    baseNames.ptr = headerFreeData(baseNames.ptr, bnt);
    dirNames.ptr = headerFreeData(dirNames.ptr, dnt);

/*@-onlytrans@*/
    if (fnp)
	*fnp = fileNames.argv;
    else
	fileNames.ptr = _free(fileNames.ptr);
/*@=onlytrans@*/
    if (fcp) *fcp = count;
}

/**
 * Retrieve file paths.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int _fnTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->t = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildFNames(h, he->tag, &he->p.argv, &he->c);
    he->freeData = 1;
    return 0;
}

static int filepathsTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->tag = RPMTAG_BASENAMES;
    return _fnTag(h, he);
}

static int origpathsTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->tag = RPMTAG_ORIGBASENAMES;
    return _fnTag(h, he);
}

/*@-type@*/ /* FIX: cast? */
const struct headerSprintfExtension_s headerCompoundFormats[] = {
    { HEADER_EXT_TAG, "RPMTAG_CHANGELOGNAME",
	{ .tagFunction = changelognameTag } },
    { HEADER_EXT_TAG, "RPMTAG_CHANGELOGTEXT",
	{ .tagFunction = changelogtextTag } },
    { HEADER_EXT_TAG, "RPMTAG_DESCRIPTION",
	{ .tagFunction = descriptionTag } },
    { HEADER_EXT_TAG, "RPMTAG_GROUP",
	{ .tagFunction = groupTag } },
    { HEADER_EXT_TAG, "RPMTAG_INSTALLPREFIX",
	{ .tagFunction = instprefixTag } },
    { HEADER_EXT_TAG, "RPMTAG_SUMMARY",
	{ .tagFunction = summaryTag } },
    { HEADER_EXT_TAG, "RPMTAG_TRIGGERCONDS",
	{ .tagFunction = triggercondsTag } },
    { HEADER_EXT_TAG, "RPMTAG_TRIGGERTYPE",
	{ .tagFunction = triggertypeTag } },
    { HEADER_EXT_TAG, "RPMTAG_DBINSTANCE",
	{ .tagFunction = dbinstanceTag } },
    { HEADER_EXT_TAG, "RPMTAG_NVRA",
	{ .tagFunction = nvraTag } },
    { HEADER_EXT_TAG, "RPMTAG_FILENAMES",
	{ .tagFunction = filepathsTag } },
    { HEADER_EXT_TAG, "RPMTAG_FILEPATHS",
	{ .tagFunction = filepathsTag } },
    { HEADER_EXT_TAG, "RPMTAG_ORIGPATHS",
	{ .tagFunction = origpathsTag } },
    { HEADER_EXT_FORMAT, "armor",
	{ .fmtFunction = armorFormat } },
    { HEADER_EXT_FORMAT, "base64",
	{ .fmtFunction = base64Format } },
    { HEADER_EXT_FORMAT, "depflags",
	{ .fmtFunction = depflagsFormat } },
    { HEADER_EXT_FORMAT, "fflags",
	{ .fmtFunction = fflagsFormat } },
    { HEADER_EXT_FORMAT, "perms",
	{ .fmtFunction = permsFormat } },
    { HEADER_EXT_FORMAT, "permissions",	
	{ .fmtFunction = permsFormat } },
    { HEADER_EXT_FORMAT, "pgpsig",
	{ .fmtFunction = pgpsigFormat } },
    { HEADER_EXT_FORMAT, "triggertype",	
	{ .fmtFunction = triggertypeFormat } },
    { HEADER_EXT_FORMAT, "xml",
	{ .fmtFunction = xmlFormat } },
    { HEADER_EXT_FORMAT, "yaml",
	{ .fmtFunction = yamlFormat } },
    { HEADER_EXT_MORE, NULL,		{ (void *) headerDefaultFormats } }
} ;
/*@=type@*/

/** \ingroup header
 * \file lib/formats.c
 */

#include "system.h"

#include "rpmio_internal.h"
#include <rpmlib.h>
#include <rpmmacro.h>	/* XXX for %_i18ndomains */
#include <rpmuuid.h>

#define	_RPMEVR_INTERNAL
#include <rpmds.h>
#include <rpmfi.h>

#include "legacy.h"
#include "argv.h"
#include "ugid.h"
#include "misc.h"
#include "fs.h"

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
 * @param av		parameter array (or NULL)
 * @return		formatted string
 */
static /*@only@*/ char * triggertypeFormat(HE_t he, /*@null@*/ const char ** av)
{
    rpmTagData data = { .ptr = he->p.ptr };
    int ix = (he->ix > 0 ? he->ix : 0);
    char * val;

assert(ix == 0);
    if (he->t != RPM_INT64_TYPE)
	val = xstrdup(_("(invalid type)"));
    else {
	int anint = data.i64p[ix];
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
 * @param av		parameter array (or NULL)
 * @return		formatted string
 */
static /*@only@*/ char * permsFormat(HE_t he, /*@null@*/ const char ** av)
{
    int ix = (he->ix > 0 ? he->ix : 0);
    char * val;

assert(ix == 0);
    if (he->t != RPM_INT64_TYPE) {
	val = xstrdup(_("(invalid type)"));
    } else {
	int_32 anint = he->p.i64p[0];
	val = rpmPermsString(anint);
    }

    return val;
}

/**
 * Format file flags for display.
 * @param he		tag container
 * @param av		parameter array (or NULL)
 * @return		formatted string
 */
static /*@only@*/ char * fflagsFormat(HE_t he, /*@null@*/ const char ** av)
{
    rpmTagData data = { .ptr = he->p.ptr };
    int ix = (he->ix >= 0 ? he->ix : 0);
    char * val;

assert(ix == 0);
    if (he->t != RPM_INT64_TYPE) {
	val = xstrdup(_("(invalid type)"));
    } else {
	char buf[15];
	unsigned anint = data.i64p[ix];
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
 * @param av		parameter array (or NULL)
 * @return		formatted string
 */
static /*@only@*/ char * armorFormat(HE_t he, /*@null@*/ const char ** av)
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
    case RPM_OPENPGP_TYPE:
    case RPM_ASN1_TYPE:		/* XXX WRONG */
    case RPM_BIN_TYPE:
	s = data.ui8p;
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

    val = pgpArmorWrap(atype, s, ns);
    if (atype == PGPARMOR_PUBKEY)
	s = _free(s);
    return val;
}

/**
 * Encode binary data in base64 for display.
 * @todo Permit selectable display formats (i.e. binary).
 * @param he		tag container
 * @param av		parameter array (or NULL)
 * @return		formatted string
 */
static /*@only@*/ char * base64Format(HE_t he, /*@null@*/ const char ** av)
	/*@*/
{
    rpmTagData data = { .ptr = he->p.ptr };
    int ix = (he->ix > 0 ? he->ix : 0);
    char * val;

assert(ix == 0);
    if (!(he->t == RPM_BIN_TYPE || he->t == RPM_ASN1_TYPE || he->t == RPM_OPENPGP_TYPE)) {
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

    while ((c = *s++) != '\0')
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

    while ((c = *s++) != '\0') {
	switch (c) {
	case '<':	te = stpcpy(te, "&lt;");	/*@switchbreak@*/ break;
	case '>':	te = stpcpy(te, "&gt;");	/*@switchbreak@*/ break;
	case '&':	te = stpcpy(te, "&amp;");	/*@switchbreak@*/ break;
	default:	*te++ = c;			/*@switchbreak@*/ break;
	}
    }
    *te = '\0';
    return t;
}

static /*@only@*/ /*@null@*/ char *
strdup_locale_convert (/*@null@*/ const char * buffer,
		/*@null@*/ const char * tocode)
	/*@*/
{
    char *dest_str;
#if defined(HAVE_ICONV)
    char *fromcode = NULL;
    iconv_t fd;

    if (buffer == NULL)
	return NULL;

    if (tocode == NULL)
	tocode = "UTF-8";

#ifdef HAVE_LANGINFO_H
/*@-type@*/
    fromcode = nl_langinfo (CODESET);
/*@=type@*/
#endif

    if (fromcode != NULL && strcmp(tocode, fromcode) != 0
     && (fd = iconv_open(tocode, fromcode)) != (iconv_t)-1)
    {
	const char *pin = buffer;
	char *pout = NULL;
	size_t ib, ob, dest_size;
	int done;
	int is_error;
	size_t err;
	const char *shift_pin = NULL;
	int xx;

	err = iconv(fd, NULL, &ib, &pout, &ob);
	dest_size = ob = ib = strlen(buffer);
	dest_str = pout = malloc((dest_size + 1) * sizeof(*dest_str));
	if (dest_str)
	    *dest_str = '\0';
	done = is_error = 0;
	if (pout != NULL)
	while (done == 0 && is_error == 0) {
	    err = iconv(fd, (char **)&pin, &ib, &pout, &ob);

	    if (err == (size_t)-1) {
		switch (errno) {
		case EINVAL:
		    done = 1;
		    /*@switchbreak@*/ break;
		case E2BIG:
		{   size_t used = (size_t)(pout - dest_str);
		    dest_size *= 2;
		    dest_str = realloc(dest_str, (dest_size + 1) * sizeof(*dest_str));
		    if (dest_str == NULL) {
			is_error = 1;
			continue;
		    }
		    pout = dest_str + used;
		    ob = dest_size - used;
		}   /*@switchbreak@*/ break;
		case EILSEQ:
		    is_error = 1;
		    /*@switchbreak@*/ break;
		default:
		    is_error = 1;
		    /*@switchbreak@*/ break;
		}
	    } else {
		if (shift_pin == NULL) {
		    shift_pin = pin;
		    pin = NULL;
		    ib = 0;
		} else {
		    done = 1;
		}
	    }
	}
	xx = iconv_close(fd);
	if (pout)
	    *pout = '\0';
	if (dest_str != NULL)
	    dest_str = xstrdup(dest_str);
    } else
#endif
    {
	dest_str = xstrdup((buffer ? buffer : ""));
    }

    return dest_str;
}

/**
 * Encode string for use in XML CDATA.
 * @param he		tag container
 * @param av		parameter array (or NULL)
 * @return		formatted string
 */
static /*@only@*/ char * cdataFormat(HE_t he, /*@null@*/ const char ** av)
	/*@*/
{
    int ix = (he->ix > 0 ? he->ix : 0);
    char * val;

assert(ix == 0);
    if (he->t != RPM_STRING_TYPE) {
	val = xstrdup(_("(not a string)"));
    } else {
	const char * s = strdup_locale_convert(he->p.str, (av ? av[0] : NULL));
	size_t nb = xmlstrlen(s);
	char * t;

	val = t = xcalloc(1, nb + 1);
	t = xmlstrcpy(t, s);	t += strlen(t);
	*t = '\0';
	s = _free(s);
    }

/*@-globstate@*/
    return val;
/*@=globstate@*/
}

/**
 * Convert string encoding.
 * @param he		tag container
 * @param av		parameter list (NULL assumes UTF-8)
 * @return		formatted string
 */
static /*@only@*/ char * iconvFormat(HE_t he, /*@null@*/ const char ** av)
	/*@*/
{
    int ix = (he->ix > 0 ? he->ix : 0);
    char * val;

assert(ix == 0);
    if (he->t != RPM_STRING_TYPE) {
	val = xstrdup(_("(not a string)"));
    } else {
	val = strdup_locale_convert(he->p.str, (av ? av[0] : NULL));
    }

/*@-globstate@*/
    return val;
/*@=globstate@*/
}

/**
 * Wrap tag data in simple header xml markup.
 * @param he		tag container
 * @param av		parameter array (or NULL)
 * @return		formatted string
 */
static /*@only@*/ char * xmlFormat(HE_t he, /*@null@*/ const char ** av)
	/*@*/
{
    rpmTagData data = { .ptr = he->p.ptr };
    int ix = (he->ix > 0 ? he->ix : 0);
    const char * xtag = NULL;
    size_t nb;
    char * val;
    const char * s = NULL;
    char * t, * te;
    unsigned long long anint = 0;
    int freeit = 0;
    int xx;

assert(ix == 0);
assert(he->t == RPM_STRING_TYPE || he->t == RPM_INT64_TYPE || he->t == RPM_BIN_TYPE);
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
    case RPM_OPENPGP_TYPE:
    case RPM_ASN1_TYPE:
    case RPM_BIN_TYPE:
/*@-globs -mods@*/
    {	int cpl = b64encode_chars_per_line;
	b64encode_chars_per_line = 0;
/*@-formatconst@*/
	s = base64Format(he, NULL);
/*@=formatconst@*/
	b64encode_chars_per_line = cpl;
	xtag = "base64";
	freeit = 1;
    }	break;
/*@=globs =mods@*/
    case RPM_CHAR_TYPE:
    case RPM_INT8_TYPE:
	anint = data.i8p[ix];
	break;
    case RPM_INT16_TYPE:
	anint = data.ui16p[ix];	/* XXX note unsigned */
	break;
    case RPM_INT32_TYPE:
	anint = data.i32p[ix];
	break;
    case RPM_INT64_TYPE:
	anint = data.i64p[ix];
	break;
    case RPM_NULL_TYPE:
    default:
	return xstrdup(_("(invalid xml type)"));
	/*@notreached@*/ break;
    }

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
 * @param av		parameter list (or NULL)
 * @return		formatted string
 */
static /*@only@*/ char * yamlFormat(HE_t he, /*@null@*/ const char ** av)
	/*@*/
{
    int element = he->ix;
    int ix = (he->ix > 0 ? he->ix : 0);
    const char * xtag = NULL;
    size_t nb;
    char * val;
    const char * s = NULL;
    char * t, * te;
    unsigned long long anint = 0;
    int freeit = 0;
    int lvl = 0;
    int xx;
    int c;

assert(ix == 0);
assert(he->t == RPM_STRING_TYPE || he->t == RPM_UINT64_TYPE || he->t == RPM_BIN_TYPE);
    xx = 0;
    switch (he->t) {
    case RPM_STRING_ARRAY_TYPE:	/* XXX currently never happens */
    case RPM_I18NSTRING_TYPE:	/* XXX currently never happens */
    case RPM_STRING_TYPE:
	s = (he->t == RPM_STRING_ARRAY_TYPE ? he->p.argv[ix] : he->p.str);
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
		if (he->ix < 0) lvl++;	/* XXX extra indent for array[1] */
	    }
	} else {
	    xtag = (element >= 0 ? "- " : NULL);
	}

	/* XXX Force utf8 strings. */
	s = xstrdup(he->p.str);
	s = xstrtolocale(s);
	freeit = 1;
	break;
    case RPM_OPENPGP_TYPE:
    case RPM_ASN1_TYPE:
    case RPM_BIN_TYPE:
/*@-globs -mods@*/
    {	int cpl = b64encode_chars_per_line;
	b64encode_chars_per_line = 0;
/*@-formatconst@*/
	s = base64Format(he, NULL);
	element = -element; 	/* XXX skip "    " indent. */
/*@=formatconst@*/
	b64encode_chars_per_line = cpl;
	xtag = "!!binary ";
	freeit = 1;
    }	break;
/*@=globs =mods@*/
    case RPM_CHAR_TYPE:
    case RPM_UINT8_TYPE:
	anint = he->p.ui8p[ix];
	break;
    case RPM_UINT16_TYPE:
	anint = he->p.ui16p[ix];
	break;
    case RPM_UINT32_TYPE:
	anint = he->p.ui32p[ix];
	break;
    case RPM_UINT64_TYPE:
	anint = he->p.ui64p[ix];
	break;
    case RPM_NULL_TYPE:
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
	nb++;
	te = t = alloca(nb);
	if (element >= 0)
	    te = stpcpy(te, "    ");
	if (xtag)
	    te = stpcpy(te, xtag);
	te = yamlstrcpy(te, s, lvl);
	te += strlen(te);
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
 * @param av		parameter array (or NULL)
 * @return		formatted string
 */
static /*@only@*/ char * pgpsigFormat(HE_t he, /*@null@*/ const char ** av)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    rpmTagData data = { .ptr = he->p.ptr };
    int ix = (he->ix > 0 ? he->ix : 0);
    char * val, * t;

assert(ix == 0);
    if (!(he->t == RPM_BIN_TYPE || he->t == RPM_ASN1_TYPE || he->t == RPM_OPENPGP_TYPE)) {
	val = xstrdup(_("(not a blob)"));
    } else {
	unsigned char * pkt = (byte *) data.ptr;
	unsigned int pktlen = 0;
	unsigned int v = *pkt;
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

	    dig = pgpFreeDig(dig);
	}
    }

    return val;
}

/**
 * Format dependency flags for display.
 * @param he		tag container
 * @param av		parameter array (or NULL)
 * @return		formatted string
 */
static /*@only@*/ char * depflagsFormat(HE_t he, /*@null@*/ const char ** av)
	/*@*/
{
    rpmTagData data = { .ptr = he->p.ptr };
    int ix = (he->ix > 0 ? he->ix : 0);;
    char * val;

assert(ix == 0);
    if (he->t != RPM_INT64_TYPE) {
	val = xstrdup(_("(invalid type)"));
    } else {
	int anint = data.i64p[ix];
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
    if (headerGetEntry(h, RPMTAG_INSTALLPREFIX, (hTYP_t)&he->t, &he->p, &he->c)) {
	he->freeData = 0;
	return 0;
    }
    he->tag = RPMTAG_INSTPREFIXES;
    if (headerGetEntry(h, he->tag, (hTYP_t)&ipt, &array, &he->c)) {
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
 * Convert unix timeval to UUIDv1.
 * @param h		header
 * @retval *he		tag container
 * @param tv		unix timeval
 * @return		0 on success
 */
static int tv2uuidv1(Header h, HE_t he, struct timeval *tv)
	/*@modifies he @*/
{
    uint64_t uuid_time = ((uint64_t)tv->tv_sec * 10000000) +
			(tv->tv_usec * 10) + 0x01B21DD213814000ULL;

    he->t = RPM_BIN_TYPE;
    he->c = 128/8;
    he->p.ptr = xcalloc(1, he->c);
    he->freeData = 1;
    if (rpmuuidMake(1, NULL, NULL, NULL, (unsigned char *)he->p.ui8p)) {
	he->p.ptr = _free(he->p.ptr);
	he->freeData = 0;
	return 1;
    }

    he->p.ui8p[6] &= 0xf0;	/* preserve version, clear time_hi nibble */
    he->p.ui8p[8] &= 0x3f;	/* preserve reserved, clear clock */
    he->p.ui8p[9] &= 0x00;

    he->p.ui8p[3] = (uuid_time >>  0);
    he->p.ui8p[2] = (uuid_time >>  8);
    he->p.ui8p[1] = (uuid_time >> 16);
    he->p.ui8p[0] = (uuid_time >> 24);
    he->p.ui8p[5] = (uuid_time >> 32);
    he->p.ui8p[4] = (uuid_time >> 40);
    he->p.ui8p[6] |= (uuid_time >> 56) & 0x0f;

#ifdef	NOTYET
    /* XXX Jigger up a non-zero (but constant) clock value. Is this needed? */
    he->p.ui8p[8] |= (he->p.ui8p[2] & 0x3f);
    he->p.ui8p[9] |= he->p.ui8p[3]
#endif

    return 0;
}

/**
 * Retrieve time and convert to UUIDv1.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int tag2uuidv1(Header h, HE_t he)
	/*@modifies he @*/
{
    struct timeval tv;

    if (!headerGetEntry(h, he->tag, (hTYP_t)&he->t, &he->p, &he->c))
	return 1;
    tv.tv_sec = he->p.ui32p[0];
    tv.tv_usec = (he->c > 1 ? he->p.ui32p[1] : 0);
    he->p.ptr = headerFreeData(he->p.ptr, he->t);
    return tv2uuidv1(h, he, &tv);
}

/**
 * Retrieve install time and convert to UUIDv1.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int installtime_uuidTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->tag = RPMTAG_INSTALLTIME;
    return tag2uuidv1(h, he);
}

/**
 * Retrieve build time and convert to UUIDv1.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int buildtime_uuidTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->tag = RPMTAG_BUILDTIME;
    return tag2uuidv1(h, he);
}

/**
 * Retrieve origin time and convert to UUIDv1.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int origintime_uuidTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->tag = RPMTAG_ORIGINTIME;
    return tag2uuidv1(h, he);
}

/**
 * Retrieve install tid and convert to UUIDv1.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int installtid_uuidTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->tag = RPMTAG_INSTALLTID;
    return tag2uuidv1(h, he);
}

/**
 * Retrieve remove tid and convert to UUIDv1.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int removetid_uuidTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->tag = RPMTAG_REMOVETID;
    return tag2uuidv1(h, he);
}

/**
 * Retrieve origin tid and convert to UUIDv1.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int origintid_uuidTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->tag = RPMTAG_ORIGINTID;
    return tag2uuidv1(h, he);
}

/*@unchecked@*/ /*@observer@*/
static const char uuid_ns[] = "ns:URL";
/*@unchecked@*/ /*@observer@*/
static const char uuid_auth[] = "%{?_uuid_auth}%{!?_uuid_auth:http://rpm5.org}";
/*@unchecked@*/ /*@observer@*/
static const char uuid_path[] = "%{?_uuid_path}%{!?_uuid_path:/package}";
/*@unchecked@*/ /*@observer@*/
static int uuid_version = 5;

/**
 * Convert tag string to UUID.
 * @param he		tag container
 * @param av		parameter list (or NULL)
 * @praram version	UUID version
 * @retval val		UUID string
 * @return		0 on success
 */
static int str2uuid(HE_t he, /*@null@*/ const char ** av,
		int version, /*@null@*/ char * val)
	/*@modifies he @*/
{
    const char * ns = NULL;
    const char * tagn = tagName(he->tag);
    const char * s = NULL;
    int rc;

    /* XXX Substitute Pkgid & Hdrid strings for aliases. */
    if (!strcmp("Sigmd5", tagn))
	tagn = "Pkgid";
    else if (!strcmp("Sha1header", tagn))
	tagn = "Hdrid";

    switch (version) {
    default:
	version = uuid_version;
	/*@fallthrough@*/
    case 3:
    case 5:
assert(he->t == RPM_STRING_TYPE);
	ns = uuid_ns;
	s = rpmGetPath(uuid_auth, "/", uuid_path, "/", tagn, "/",
			he->p.str, NULL);
	/*@fallthrough@*/
    case 4:
	break;
    }
    he->p.ptr = _free(he->p.ptr);
    he->t = RPM_BIN_TYPE;
    he->c = 128/8;
    he->p.ptr = xcalloc(1, he->c);
    he->freeData = 1;
    rc = rpmuuidMake(version, ns, s, val, (unsigned char *)he->p.ui8p);
    if (rc) {
	he->p.ptr = _free(he->p.ptr);
	he->freeData = 0;
    }
    s = _free(s);
    return rc;
}

/**
 * Retrieve tag and convert to UUIDv5.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int tag2uuidv5(Header h, HE_t he)
	/*@modifies he @*/
{
    if (!headerGetEntry(h, he->tag, (hTYP_t)&he->t, &he->p, &he->c))
	return 1;
    switch (he->t) {
    default:
assert(0);
	/*@notreached@*/ break;
    case RPM_BIN_TYPE:	{	/* Convert RPMTAG_PKGID from binary => hex. */
	static const char hex[] = "0123456789abcdef";
	char * t;
	char * te;
	uint32_t i;

	t = te = xmalloc (2*he->c + 1);
	for (i = 0; i < he->c; i++) {
	    *te++ = hex[ ((he->p.ui8p[i] >> 4) & 0x0f) ];
	    *te++ = hex[ ((he->p.ui8p[i]     ) & 0x0f) ];
	}
	*te = '\0';
	he->p.ptr = headerFreeData(he->p.ptr, he->t);
	he->t = RPM_STRING_TYPE;
	he->p.ptr = t;
	he->c = 1;
	he->freeData = 1;
    }	break;
    case RPM_STRING_TYPE:
	break;
    }
    return str2uuid(he, NULL, 0, NULL);
}

/**
 * Retrieve pkgid and convert to UUIDv5.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int pkguuidTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->tag = RPMTAG_PKGID;
    return tag2uuidv5(h, he);
}

/**
 * Retrieve sourcepkgid and convert to UUIDv5.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int sourcepkguuidTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->tag = RPMTAG_SOURCEPKGID;
    return tag2uuidv5(h, he);
}

/**
 * Retrieve hdrid and convert to UUIDv5.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int hdruuidTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->tag = RPMTAG_HDRID;
    return tag2uuidv5(h, he);
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
    int numNames, numScripts;
    const char ** conds;
    rpmTagData s;
    char * item, * flagsStr;
    char * chptr;
    int i, j, xx;

    he->freeData = 0;
    xx = headerGetEntry(h, RPMTAG_TRIGGERNAME, NULL, &names, &numNames);
    if (!xx)
	return 0;

    _he->tag = he->tag;
    _he->t = RPM_INT32_TYPE;
    _he->p.i32p = NULL;
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
    for (i = 0; i < numScripts; i++) {
	chptr = xstrdup("");

	for (j = 0; j < numNames; j++) {
	    if (indices.i32p[j] != i)
		/*@innercontinue@*/ continue;

	    item = xmalloc(strlen(names.argv[j]) + strlen(versions.argv[j]) + 20);
	    if (flags.i32p[j] & RPMSENSE_SENSEMASK) {
		_he->p.i32p = &flags.i32p[j];
		flagsStr = depflagsFormat(_he, NULL);
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
    int i, j, xx;
    int numScripts, numNames;

    he->freeData = 0;
    if (!headerGetEntry(h, RPMTAG_TRIGGERINDEX, NULL, &indices, &numNames))
	return 1;

    xx = headerGetEntry(h, RPMTAG_TRIGGERFLAGS, NULL, &flags, NULL);
    xx = headerGetEntry(h, RPMTAG_TRIGGERSCRIPTS, NULL, &s, &numScripts);

    he->t = RPM_STRING_ARRAY_TYPE;
    he->c = numScripts;

    he->freeData = 1;
    he->p.argv = conds = xmalloc(sizeof(*conds) * numScripts);
    for (i = 0; i < numScripts; i++) {
	for (j = 0; j < numNames; j++) {
	    if (indices.i32p[j] != i)
		/*@innercontinue@*/ continue;

	    if (flags.i32p[j] & RPMSENSE_TRIGGERPREIN)
		conds[i] = xstrdup("prein");
	    else if (flags.i32p[j] & RPMSENSE_TRIGGERIN)
		conds[i] = xstrdup("in");
	    else if (flags.i32p[j] & RPMSENSE_TRIGGERUN)
		conds[i] = xstrdup("un");
	    else if (flags.i32p[j] & RPMSENSE_TRIGGERPOSTUN)
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

    rc = headerGetEntry(h, he->tag, (hTYP_t)&he->t, &he->p, &he->c);
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

    rc = headerGetEntry(h, he->tag, (hTYP_t)&t, &p, &c);
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
	int i;
	for (i = 0; i < c; i++) {
	    p.argv[i] = xstrdup(p.argv[i]);
	    p.argv[i] = xstrtolocale(p.argv[i]);
assert(p.argv[i] != NULL);
	    l += strlen(p.argv[i]) + 1;
	}
	argv = xmalloc(c * sizeof(*argv) + l);
	te = (char *)&argv[c];
	for (i = 0; i < c; i++) {
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
    he->t = RPM_INT32_TYPE;
    he->p.i32p = xmalloc(sizeof(*he->p.i32p));
    he->p.i32p[0] = headerGetInstance(h);
    he->freeData = 1;
    he->c = 1;
    return 0;
}
/*@=globuse@*/

/**
 * Retrieve starting byte offset of header.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
/*@-globuse@*/
static int headerstartoffTag(Header h, HE_t he)
	/*@globals rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies he, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    he->tag = RPMTAG_HEADERSTARTOFF;
    he->t = RPM_INT64_TYPE;
    he->p.ui64p = xmalloc(sizeof(*he->p.ui64p));
    he->p.ui64p[0] = headerGetStartOff(h);
    he->freeData = 1;
    he->c = 1;
    return 0;
}
/*@=globuse@*/

/**
 * Retrieve ending byte offset of header.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
/*@-globuse@*/
static int headerendoffTag(Header h, HE_t he)
	/*@globals rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies he, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    he->tag = RPMTAG_HEADERENDOFF;
    he->t = RPM_INT64_TYPE;
    he->p.ui64p = xmalloc(sizeof(*he->p.ui64p));
    he->p.ui64p[0] = headerGetEndOff(h);
    he->freeData = 1;
    he->c = 1;
    return 0;
}
/*@=globuse@*/

/**
 * Retrieve package origin from header.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
/*@-globuse@*/
static int pkgoriginTag(Header h, HE_t he)
	/*@globals rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies he, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    const char * origin;

    he->tag = RPMTAG_PACKAGEORIGIN;
    if (!headerGetEntry(h, he->tag, NULL, &origin, NULL)
     && (origin = headerGetOrigin(h)) != NULL)
    {
	he->t = RPM_STRING_TYPE;
	he->p.str = xstrdup(origin);
	he->c = 1;
	he->freeData = 1;
    }
    return 0;
}
/*@=globuse@*/

/**
 * Retrieve package baseurl from header.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
/*@-globuse@*/
static int pkgbaseurlTag(Header h, HE_t he)
	/*@globals rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies he, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    const char * baseurl;
    int rc = 1;

    he->tag = RPMTAG_PACKAGEBASEURL;
    if (!headerGetEntry(h, he->tag, NULL, &baseurl, NULL)
     && (baseurl = headerGetBaseURL(h)) != NULL)
    {
	he->t = RPM_STRING_TYPE;
	he->p.str = xstrdup(baseurl);
	he->c = 1;
	he->freeData = 1;
	rc = 0;
    }
    return rc;
}
/*@=globuse@*/

/**
 * Retrieve package digest from header.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
/*@-globuse@*/
static int pkgdigestTag(Header h, HE_t he)
	/*@globals rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies he, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    const char * digest;
    int rc = 1;

    he->tag = RPMTAG_PACKAGEDIGEST;
    if ((digest = headerGetDigest(h)) != NULL)
    {
	he->t = RPM_STRING_TYPE;
	he->p.str = xstrdup(digest);
	he->c = 1;
	he->freeData = 1;
	rc = 0;
    }
    return rc;
}
/*@=globuse@*/

/**
 * Retrieve *.rpm package st->st_mtime from header.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
/*@-globuse@*/
static int pkgmtimeTag(Header h, HE_t he)
	/*@globals rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies he, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    struct stat * st = headerGetStatbuf(h);
    he->tag = RPMTAG_PACKAGETIME;
    he->t = RPM_UINT64_TYPE;
    he->p.ui64p = xmalloc(sizeof(*he->p.ui64p));
    he->p.ui64p[0] = st->st_mtime;
    he->freeData = 1;
    he->c = 1;
    return 0;
}
/*@=globuse@*/

/**
 * Retrieve *.rpm package st->st_size from header.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
/*@-globuse@*/
static int pkgsizeTag(Header h, HE_t he)
	/*@globals rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies he, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    struct stat * st = headerGetStatbuf(h);
    he->tag = RPMTAG_PACKAGESIZE;
    he->t = RPM_UINT64_TYPE;
    he->p.ui64p = xmalloc(sizeof(*he->p.ui64p));
    he->p.ui64p[0] = st->st_size;
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

/**
 * Retrieve mounted file system paths.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int fsnamesTag( /*@unused@*/ Header h, HE_t he)
	/*@globals fileSystem, internalState @*/
	/*@modifies he, fileSystem, internalState @*/
{
    const char ** list;

    if (rpmGetFilesystemList(&list, &he->c))
	return 1;

    he->t = RPM_STRING_ARRAY_TYPE;
    he->p.argv = list;
    he->freeData = 0;

    return 0;
}

/**
 * Retrieve mounted file system space.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int fssizesTag(Header h, HE_t he)
	/*@globals rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies he, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    HE_t vhe = memset(alloca(sizeof(*vhe)), 0, sizeof(*vhe));
    const char ** fnames;
    uint_64 * usages;
    int numFiles;
    int rc = 1;		/* assume error */
    int xx;

    vhe->tag = RPMTAG_FILESIZES;
    xx = headerGetEntry(h, vhe->tag, (hTYP_t)&vhe->t, &vhe->p.ptr, &vhe->c);
    if (!xx) {
	numFiles = 0;
	fnames = NULL;
    } else
	rpmfiBuildFNames(h, RPMTAG_BASENAMES, &fnames, &numFiles);

    if (rpmGetFilesystemList(NULL, &he->c))
	goto exit;

    he->t = RPM_INT64_TYPE;
    he->freeData = 1;

    if (fnames == NULL)
	he->p.ui64p = xcalloc(he->c, sizeof(*usages));
    else
    if (rpmGetFilesystemUsage(fnames, vhe->p.ui32p, numFiles, &he->p.ui64p, 0))	
	goto exit;

    rc = 0;

exit:
    fnames = _free(fnames);

    return rc;
}

/**
 * Retrieve file classes.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int fileclassTag(Header h, HE_t he)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies h, he,
		rpmGlobalMacroContext, fileSystem, internalState @*/
{
    he->t = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildFClasses(h, &he->p.argv, &he->c);
    he->freeData = 1;
    return 0;
}

/**
 * Retrieve file contexts from header.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int filecontextsTag(Header h, HE_t he)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies h, he,
		rpmGlobalMacroContext, fileSystem, internalState @*/
{
    he->t = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildFContexts(h, &he->p.argv, &he->c);
    he->freeData = 1;
    return 0;
}

/**
 * Retrieve file contexts from file system.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int fscontextsTag(Header h, HE_t he)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies h, he,
		rpmGlobalMacroContext, fileSystem, internalState @*/
{
    he->t = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildFSContexts(h, &he->p.argv, &he->c);
    he->freeData = 1;
    return 0;
}

/**
 * Retrieve file contexts from policy RE's.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int recontextsTag(Header h, HE_t he)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies h, he,
		rpmGlobalMacroContext, fileSystem, internalState @*/
{
    he->t = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildREContexts(h, &he->p.argv, &he->c);
    he->freeData = 1;
    return 0;
}

/**
 * Retrieve file provides.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int fileprovideTag(Header h, HE_t he)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies h, he,
		rpmGlobalMacroContext, fileSystem, internalState @*/
{
    he->t = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildFDeps(h, RPMTAG_PROVIDENAME, &he->p.argv, &he->c);
    he->freeData = 1;
    return 0;
}

/**
 * Retrieve file requires.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int filerequireTag(Header h, HE_t he)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies h, he,
		rpmGlobalMacroContext, fileSystem, internalState @*/
{
    he->t = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildFDeps(h, RPMTAG_REQUIRENAME, &he->p.argv, &he->c);
    he->freeData = 1;
    return 0;
}

/**
 * Retrieve Requires(missingok): array for Suggests: or Enhances:.
 * @param h		header
 * @retval *he		tag container
 * @return		0 on success
 */
static int missingokTag(Header h, HE_t he)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies h, he,
		rpmGlobalMacroContext, fileSystem, internalState @*/
{
    rpmds ds = rpmdsNew(h, RPMTAG_REQUIRENAME, 0);
    ARGV_t av = NULL;
    ARGV_t argv;
    int argc = 0;
    char * t;
    size_t nb = 0;
    int i;

assert(ds != NULL);
    /* Collect dependencies marked as hints. */
    ds = rpmdsInit(ds);
    if (ds != NULL)
    while (rpmdsNext(ds) >= 0) {
	int Flags = rpmdsFlags(ds);
	const char * DNEVR;
	if (!(Flags & RPMSENSE_MISSINGOK))
	    continue;
	DNEVR = rpmdsDNEVR(ds);
	if (DNEVR == NULL)
	    continue;
	nb += sizeof(*argv) + strlen(DNEVR+2) + 1;
	(void) argvAdd(&av, DNEVR+2);
	argc++;
    }
    nb += sizeof(*argv);	/* final argv NULL */

    /* Create contiguous header string array. */
    argv = (ARGV_t) xcalloc(nb, 1);
    t = (char *)(argv + argc);
    for (i = 0; i < argc; i++) {
	argv[i] = t;
	t = stpcpy(t, av[i]);
	*t++ = '\0';
    }
    av = argvFree(av);
    ds = rpmdsFree(ds);

    /* XXX perhaps return "(none)" inband if no suggests/enhances <shrug>. */

    he->t = RPM_STRING_ARRAY_TYPE;
    he->p.argv = argv;
    he->c = argc;
    he->freeData = 1;
    return 0;
}

static int PRCOSkip(rpmTag tag, rpmTagData N, rpmTagData EVR, rpmTagData F,
		uint32_t i)
	/*@*/
{
    int a = -2, b = -2;

    if (N.argv[i] == NULL || *N.argv[i] == '\0')
	return 1;
    if (tag == RPMTAG_REQUIRENAME && i > 0
     && !(a=strcmp(N.argv[i], N.argv[i-1]))
     && !(b=strcmp(EVR.argv[i], EVR.argv[i-1]))
     && (F.ui32p[i] & 0x4e) == ((F.ui32p[i-1] & 0x4e)))
	return 1;
    return 0;
}

static int PRCOxmlTag(Header h, HE_t he, rpmTag EVRtag, rpmTag Ftag)
	/*@modifies he @*/
{
    rpmTag tag = he->tag;
    rpmTagData N = { .ptr = NULL };
    rpmTagData EVR = { .ptr = NULL };
    rpmTagData F = { .ptr = NULL };
    size_t nb;
    uint32_t ac;
    uint32_t c;
    uint32_t i;
    char *t;
    int rc = 1;		/* assume failure */
    int xx;

    xx = headerGetEntry(h, he->tag, (hTYP_t)&he->t, &he->p, &he->c);
    if (xx == 0) goto exit;
    N.argv = he->p.argv;
    c = he->c;

    he->tag = EVRtag;
    xx = headerGetEntry(h, he->tag, (hTYP_t)&he->t, &he->p, &he->c);
    if (xx == 0) goto exit;
    EVR.argv = he->p.argv;

    he->tag = Ftag;
    xx = headerGetEntry(h, he->tag, (hTYP_t)&he->t, &he->p, &he->c);
    if (xx == 0) goto exit;
    F.ui32p = he->p.ui32p;

    nb = sizeof(*he->p.argv);
    ac = 0;
    for (i = 0; i < c; i++) {
	if (PRCOSkip(tag, N, EVR, F, i))
	    continue;
	ac++;
	nb += sizeof(*he->p.argv);
	nb += sizeof("<rpm:entry name=\"\"/>");
	if (*N.argv[i] == '/')
	    nb += xmlstrlen(N.argv[i]);
	else
	    nb += strlen(N.argv[i]);
	if (EVR.argv != NULL && EVR.argv[i] != NULL && *EVR.argv[i] != '\0') {
	    nb += sizeof(" flags=\"EQ\" epoch=\"0\" ver=\"\"") - 1;
	    nb += strlen(EVR.argv[i]);
	    if (strchr(EVR.argv[i], ':') != NULL)
		nb -= 2;
	    if (strchr(EVR.argv[i], '-') != NULL)
		nb += sizeof(" rel=\"\"") - 2;
	}
#ifdef	NOTNOW
	if (tag == RPMTAG_REQUIRENAME && (F.ui32p[i] & 0x40))
	    nb += sizeof(" pre=\"1\"") - 1;
#endif
    }

    he->t = RPM_STRING_ARRAY_TYPE;
    he->c = ac;
    he->freeData = 1;
    he->p.argv = xmalloc(nb + BUFSIZ);	/* XXX hack: leave slop */
    t = (char *) &he->p.argv[he->c + 1];
    ac = 0;
    for (i = 0; i < c; i++) {
	if (PRCOSkip(tag, N, EVR, F, i))
	    continue;
	he->p.argv[ac++] = t;
	t = stpcpy(t, "<rpm:entry");
	t = stpcpy(t, " name=\"");
	if (*N.argv[i] == '/') {
	    t = xmlstrcpy(t, N.argv[i]);	t += strlen(t);
	} else
	    t = stpcpy(t, N.argv[i]);
	t = stpcpy(t, "\"");
	if (EVR.argv != NULL && EVR.argv[i] != NULL && *EVR.argv[i] != '\0') {
	    static char *Fstr[] = { "?0","LT","GT","?3","EQ","LE","GE","?7" };
	    int Fx = ((F.ui32p[i] >> 1) & 0x7);
	    const char *E, *V, *R;
	    char *f, *fe;
	    t = stpcpy( stpcpy( stpcpy(t, " flags=\""), Fstr[Fx]), "\"");
	    f = (char *) EVR.argv[i];
	    for (fe = f; *fe != '\0' && *fe >= '0' && *fe <= '9'; fe++);
	    if (*fe == ':') { *fe++ = '\0'; E = f; f = fe; } else E = NULL;
	    V = f;
	    for (fe = f; *fe != '\0' && *fe != '-'; fe++);
	    if (*fe == '-') { *fe++ = '\0'; R = fe; } else R = NULL;
	    t = stpcpy( stpcpy( stpcpy(t, " epoch=\""), (E && *E ? E : "0")), "\"");
	    t = stpcpy( stpcpy( stpcpy(t, " ver=\""), V), "\"");
	    if (R != NULL)
		t = stpcpy( stpcpy( stpcpy(t, " rel=\""), R), "\"");
	}
#ifdef	NOTNOW
	if (tag == RPMTAG_REQUIRENAME && (F.ui32p[i] & 0x40))
	    t = stpcpy(t, " pre=\"1\"");
#endif
	t = stpcpy(t, "/>");
	*t++ = '\0';
    }
    he->p.argv[he->c] = NULL;
    rc = 0;

exit:
    N.argv = _free(N.argv);
    EVR.argv = _free(EVR.argv);
    return rc;
}

static int PxmlTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->tag = RPMTAG_PROVIDENAME;
    return PRCOxmlTag(h, he, RPMTAG_PROVIDEVERSION, RPMTAG_PROVIDEFLAGS);
}

static int RxmlTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->tag = RPMTAG_REQUIRENAME;
    return PRCOxmlTag(h, he, RPMTAG_REQUIREVERSION, RPMTAG_REQUIREFLAGS);
}

static int CxmlTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->tag = RPMTAG_CONFLICTNAME;
    return PRCOxmlTag(h, he, RPMTAG_CONFLICTVERSION, RPMTAG_CONFLICTFLAGS);
}

static int OxmlTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->tag = RPMTAG_OBSOLETENAME;
    return PRCOxmlTag(h, he, RPMTAG_OBSOLETEVERSION, RPMTAG_OBSOLETEFLAGS);
}

/**
 * Return length of string represented with single quotes doubled.
 * @param s		string
 * @return		length of sql string
 */
static size_t sqlstrlen(const char * s)
	/*@*/
{
    size_t len = 0;
    int c;

    while ((c = (int) *s++) != (int) '\0')
    {
	switch (c) {
	case '\'':	len += 1;			/*@fallthrough@*/
	default:	len += 1;			/*@switchbreak@*/ break;
	}
    }
    return len;
}

/**
 * Copy source string to target, doubling single quotes.
 * @param t		target sql string
 * @param s		source string
 * @return		target sql string
 */
static char * sqlstrcpy(/*@returned@*/ char * t, const char * s)
	/*@modifies t @*/
{
    char * te = t;
    int c;

    while ((c = (int) *s++) != (int) '\0') {
	switch (c) {
	case '\'':	*te++ = (char) c;		/*@fallthrough@*/
	default:	*te++ = (char) c;		/*@switchbreak@*/ break;
	}
    }
    *te = '\0';
    return t;
}

/**
 * Encode string for use in SQL statements.
 * @param he		tag container
 * @param av		parameter array (or NULL)
 * @return		formatted string
 */
static /*@only@*/ char * sqlescapeFormat(HE_t he, /*@null@*/ const char ** av)
	/*@*/
{
    int ix = (he->ix > 0 ? he->ix : 0);
    char * val;

assert(ix == 0);
    if (he->t != RPM_STRING_TYPE) {
	val = xstrdup(_("(not a string)"));
    } else {
	const char * s = strdup_locale_convert(he->p.str, (av ? av[0] : NULL));
	size_t nb = sqlstrlen(s);
	char * t;

	val = t = xcalloc(1, nb + 1);
	t = sqlstrcpy(t, s);	t += strlen(t);
	*t = '\0';
	s = _free(s);
    }

/*@-globstate@*/
    return val;
/*@=globstate@*/
}

static int PRCOsqlTag(Header h, HE_t he, rpmTag EVRtag, rpmTag Ftag)
	/*@modifies he @*/
{
    rpmTag tag = he->tag;
    rpmTagData N = { .ptr = NULL };
    rpmTagData EVR = { .ptr = NULL };
    rpmTagData F = { .ptr = NULL };
    char instance[64];
    size_t nb;
    uint32_t ac;
    uint32_t c;
    uint32_t i;
    char *t;
    int rc = 1;		/* assume failure */
    int xx;

    xx = headerGetEntry(h, he->tag, (hTYP_t)&he->t, &he->p, &he->c);
    if (xx == 0) goto exit;
    N.argv = he->p.argv;
    c = he->c;

    he->tag = EVRtag;
    xx = headerGetEntry(h, he->tag, (hTYP_t)&he->t, &he->p, &he->c);
    if (xx == 0) goto exit;
    EVR.argv = he->p.argv;

    he->tag = Ftag;
    xx = headerGetEntry(h, he->tag, (hTYP_t)&he->t, &he->p, &he->c);
    if (xx == 0) goto exit;
    F.ui32p = he->p.ui32p;

    xx = snprintf(instance, sizeof(instance), "'%d'", headerGetInstance(h));
    nb = sizeof(*he->p.argv);
    ac = 0;
    for (i = 0; i < c; i++) {
	if (PRCOSkip(tag, N, EVR, F, i))
	    continue;
	ac++;
	nb += sizeof(*he->p.argv);
	nb += strlen(instance) + sizeof(", '', '', '', '', ''");
	if (tag == RPMTAG_REQUIRENAME)
	    nb += sizeof(", ''") - 1;
	nb += strlen(N.argv[i]);
	if (EVR.argv != NULL && EVR.argv[i] != NULL && *EVR.argv[i] != '\0') {
	    nb += strlen(EVR.argv[i]);
	    nb += sizeof("EQ0") - 1;
	}
#ifdef	NOTNOW
	if (tag == RPMTAG_REQUIRENAME && (F.ui32p[i] & 0x40))
	    nb += sizeof("1") - 1;
#endif
    }

    he->t = RPM_STRING_ARRAY_TYPE;
    he->c = ac;
    he->freeData = 1;
    he->p.argv = xmalloc(nb + BUFSIZ);	/* XXX hack: leave slop */
    t = (char *) &he->p.argv[he->c + 1];
    ac = 0;
    for (i = 0; i < c; i++) {
	if (PRCOSkip(tag, N, EVR, F, i))
	    continue;
	he->p.argv[ac++] = t;
	t = stpcpy(t, instance);
	t = stpcpy( stpcpy( stpcpy(t, ", '"), N.argv[i]), "'");
	if (EVR.argv != NULL && EVR.argv[i] != NULL && *EVR.argv[i] != '\0') {
	    static char *Fstr[] = { "?0","LT","GT","?3","EQ","LE","GE","?7" };
	    int Fx = ((F.ui32p[i] >> 1) & 0x7);
	    const char *E, *V, *R;
	    char *f, *fe;
	    t = stpcpy( stpcpy( stpcpy(t, ", '"), Fstr[Fx]), "'");
	    f = (char *) EVR.argv[i];
	    for (fe = f; *fe != '\0' && *fe >= '0' && *fe <= '9'; fe++);
	    if (*fe == ':') { *fe++ = '\0'; E = f; f = fe; } else E = NULL;
	    V = f;
	    for (fe = f; *fe != '\0' && *fe != '-'; fe++);
	    if (*fe == '-') { *fe++ = '\0'; R = fe; } else R = NULL;
	    t = stpcpy( stpcpy( stpcpy(t, ", '"), (E && *E ? E : "0")), "'");
	    t = stpcpy( stpcpy( stpcpy(t, ", '"), V), "'");
	    t = stpcpy( stpcpy( stpcpy(t, ", '"), (R ? R : "")), "'");
	} else
	    t = stpcpy(t, ", '', '', '', ''");
#ifdef	NOTNOW
	if (tag == RPMTAG_REQUIRENAME)
	    t = stpcpy(stpcpy(stpcpy(t, ", '"),(F.ui32p[i] & 0x40) ? "1" : "0"), "'");
#endif
	*t++ = '\0';
    }
    he->p.argv[he->c] = NULL;
    rc = 0;

exit:
    N.argv = _free(N.argv);
    EVR.argv = _free(EVR.argv);
    return rc;
}

static int PsqlTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->tag = RPMTAG_PROVIDENAME;
    return PRCOsqlTag(h, he, RPMTAG_PROVIDEVERSION, RPMTAG_PROVIDEFLAGS);
}

static int RsqlTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->tag = RPMTAG_REQUIRENAME;
    return PRCOsqlTag(h, he, RPMTAG_REQUIREVERSION, RPMTAG_REQUIREFLAGS);
}

static int CsqlTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->tag = RPMTAG_CONFLICTNAME;
    return PRCOsqlTag(h, he, RPMTAG_CONFLICTVERSION, RPMTAG_CONFLICTFLAGS);
}

static int OsqlTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->tag = RPMTAG_OBSOLETENAME;
    return PRCOsqlTag(h, he, RPMTAG_OBSOLETEVERSION, RPMTAG_OBSOLETEFLAGS);
}

static int FDGSkip(rpmTagData DN, rpmTagData BN, rpmTagData DI, uint32_t i)
	/*@*/
{
    const char * dn = DN.argv[DI.ui32p[i]];
    size_t dnlen = strlen(dn);

    if (strstr(dn, "bin/") != NULL)
	return 1;
    if (dnlen >= sizeof("/etc/")-1 && !strncmp(dn, "/etc/", dnlen))
	return 1;
    if (!strcmp(dn, "/usr/lib/") && !strcmp(BN.argv[i], "sendmail"))
	return 1;
    return 2;
}

static int FDGxmlTag(Header h, HE_t he, int lvl)
	/*@modifies he @*/
{
    rpmTagData BN = { .ptr = NULL };
    rpmTagData DN = { .ptr = NULL };
    rpmTagData DI = { .ptr = NULL };
    rpmTagData FMODES = { .ptr = NULL };
    rpmTagData FFLAGS = { .ptr = NULL };
    size_t nb;
    uint32_t ac;
    uint32_t c;
    uint32_t i;
    char *t;
    int rc = 1;		/* assume failure */
    int xx;

    he->tag = RPMTAG_BASENAMES;
    xx = headerGetEntry(h, he->tag, (hTYP_t)&he->t, &he->p, &he->c);
    if (xx == 0) goto exit;
    BN.argv = he->p.argv;
    c = he->c;

    he->tag = RPMTAG_DIRNAMES;
    xx = headerGetEntry(h, he->tag, (hTYP_t)&he->t, &he->p, &he->c);
    if (xx == 0) goto exit;
    DN.argv = he->p.argv;

    he->tag = RPMTAG_DIRINDEXES;
    xx = headerGetEntry(h, he->tag, (hTYP_t)&he->t, &he->p, &he->c);
    if (xx == 0) goto exit;
    DI.ui32p = he->p.ui32p;

    he->tag = RPMTAG_FILEMODES;
    xx = headerGetEntry(h, he->tag, (hTYP_t)&he->t, &he->p, &he->c);
    if (xx == 0) goto exit;
    FMODES.ui16p = he->p.ui16p;

    he->tag = RPMTAG_FILEFLAGS;
    xx = headerGetEntry(h, he->tag, (hTYP_t)&he->t, &he->p, &he->c);
    if (xx == 0) goto exit;
    FFLAGS.ui32p = he->p.ui32p;

    nb = sizeof(*he->p.argv);
    ac = 0;
    for (i = 0; i < c; i++) {
	if (lvl > 0 && FDGSkip(DN, BN, DI, i) != lvl)
	    continue;
	ac++;
	nb += sizeof(*he->p.argv);
	nb += sizeof("<file></file>");
	nb += xmlstrlen(DN.argv[DI.ui32p[i]]);
	nb += xmlstrlen(BN.argv[i]);
	if (FFLAGS.ui32p[i] & 0x40)	/* XXX RPMFILE_GHOST */
	    nb += sizeof(" type=\"ghost\"") - 1;
	else if (S_ISDIR(FMODES.ui16p[i]))
	    nb += sizeof(" type=\"dir\"") - 1;
    }

    he->t = RPM_STRING_ARRAY_TYPE;
    he->c = ac;
    he->freeData = 1;
    he->p.argv = xmalloc(nb);
    t = (char *) &he->p.argv[he->c + 1];
    ac = 0;
    /* FIXME: Files, then dirs, finally ghosts breaks sort order.  */
    for (i = 0; i < c; i++) {
	if (lvl > 0 && FDGSkip(DN, BN, DI, i) != lvl)
	    continue;
	if (FFLAGS.ui32p[i] & 0x40)	/* XXX RPMFILE_GHOST */
	    continue;
	if (S_ISDIR(FMODES.ui16p[i]))
	    continue;
	he->p.argv[ac++] = t;
	t = stpcpy(t, "<file>");
	t = xmlstrcpy(t, DN.argv[DI.ui32p[i]]); t += strlen(t);
	t = xmlstrcpy(t, BN.argv[i]);		t += strlen(t);
	t = stpcpy(t, "</file>");
	*t++ = '\0';
    }
    for (i = 0; i < c; i++) {
	if (lvl > 0 && FDGSkip(DN, BN, DI, i) != lvl)
	    continue;
	if (FFLAGS.ui32p[i] & 0x40)	/* XXX RPMFILE_GHOST */
	    continue;
	if (!S_ISDIR(FMODES.ui16p[i]))
	    continue;
	he->p.argv[ac++] = t;
	t = stpcpy(t, "<file type=\"dir\">");
	t = xmlstrcpy(t, DN.argv[DI.ui32p[i]]); t += strlen(t);
	t = xmlstrcpy(t, BN.argv[i]);		t += strlen(t);
	t = stpcpy(t, "</file>");
	*t++ = '\0';
    }
    for (i = 0; i < c; i++) {
	if (lvl > 0 && FDGSkip(DN, BN, DI, i) != lvl)
	    continue;
	if (!(FFLAGS.ui32p[i] & 0x40))	/* XXX RPMFILE_GHOST */
	    continue;
	he->p.argv[ac++] = t;
	t = stpcpy(t, "<file type=\"ghost\">");
	t = xmlstrcpy(t, DN.argv[DI.ui32p[i]]); t += strlen(t);
	t = xmlstrcpy(t, BN.argv[i]);		t += strlen(t);
	t = stpcpy(t, "</file>");
	*t++ = '\0';
    }

    he->p.argv[he->c] = NULL;
    rc = 0;

exit:
    BN.argv = _free(BN.argv);
    DN.argv = _free(DN.argv);
    return rc;
}

static int F1xmlTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->tag = RPMTAG_BASENAMES;
    return FDGxmlTag(h, he, 1);
}

static int F2xmlTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->tag = RPMTAG_BASENAMES;
    return FDGxmlTag(h, he, 2);
}

static int FDGsqlTag(Header h, HE_t he, int lvl)
	/*@modifies he @*/
{
    rpmTagData BN = { .ptr = NULL };
    rpmTagData DN = { .ptr = NULL };
    rpmTagData DI = { .ptr = NULL };
    rpmTagData FMODES = { .ptr = NULL };
    rpmTagData FFLAGS = { .ptr = NULL };
    char instance[64];
    size_t nb;
    uint32_t ac;
    uint32_t c;
    uint32_t i;
    char *t;
    int rc = 1;		/* assume failure */
    int xx;

    he->tag = RPMTAG_BASENAMES;
    xx = headerGetEntry(h, he->tag, (hTYP_t)&he->t, &he->p, &he->c);
    if (xx == 0) goto exit;
    BN.argv = he->p.argv;
    c = he->c;

    he->tag = RPMTAG_DIRNAMES;
    xx = headerGetEntry(h, he->tag, (hTYP_t)&he->t, &he->p, &he->c);
    if (xx == 0) goto exit;
    DN.argv = he->p.argv;

    he->tag = RPMTAG_DIRINDEXES;
    xx = headerGetEntry(h, he->tag, (hTYP_t)&he->t, &he->p, &he->c);
    if (xx == 0) goto exit;
    DI.ui32p = he->p.ui32p;

    he->tag = RPMTAG_FILEMODES;
    xx = headerGetEntry(h, he->tag, (hTYP_t)&he->t, &he->p, &he->c);
    if (xx == 0) goto exit;
    FMODES.ui16p = he->p.ui16p;

    he->tag = RPMTAG_FILEFLAGS;
    xx = headerGetEntry(h, he->tag, (hTYP_t)&he->t, &he->p, &he->c);
    if (xx == 0) goto exit;
    FFLAGS.ui32p = he->p.ui32p;

    xx = snprintf(instance, sizeof(instance), "'%d'", headerGetInstance(h));
    nb = sizeof(*he->p.argv);
    ac = 0;
    for (i = 0; i < c; i++) {
	if (lvl > 0 && FDGSkip(DN, BN, DI, i) != lvl)
	    continue;
	ac++;
	nb += sizeof(*he->p.argv);
	nb += strlen(instance) + sizeof(", '', ''");
	nb += strlen(DN.argv[DI.ui32p[i]]);
	nb += strlen(BN.argv[i]);
	if (FFLAGS.ui32p[i] & 0x40)	/* XXX RPMFILE_GHOST */
	    nb += sizeof("ghost") - 1;
	else if (S_ISDIR(FMODES.ui16p[i]))
	    nb += sizeof("dir") - 1;
	else
	    nb += sizeof("file") - 1;
    }

    he->t = RPM_STRING_ARRAY_TYPE;
    he->c = ac;
    he->freeData = 1;
    he->p.argv = xmalloc(nb);
    t = (char *) &he->p.argv[he->c + 1];
    ac = 0;
    /* FIXME: Files, then dirs, finally ghosts breaks sort order.  */
    for (i = 0; i < c; i++) {
	if (lvl > 0 && FDGSkip(DN, BN, DI, i) != lvl)
	    continue;
	if (FFLAGS.ui32p[i] & 0x40)	/* XXX RPMFILE_GHOST */
	    continue;
	if (S_ISDIR(FMODES.ui16p[i]))
	    continue;
	he->p.argv[ac++] = t;
	t = stpcpy( stpcpy(t, instance), ", '");
	t = strcpy(t, DN.argv[DI.ui32p[i]]);	t += strlen(t);
	t = strcpy(t, BN.argv[i]);		t += strlen(t);
	t = stpcpy(t, "', 'file'");
	*t++ = '\0';
    }
    for (i = 0; i < c; i++) {
	if (lvl > 0 && FDGSkip(DN, BN, DI, i) != lvl)
	    continue;
	if (FFLAGS.ui32p[i] & 0x40)	/* XXX RPMFILE_GHOST */
	    continue;
	if (!S_ISDIR(FMODES.ui16p[i]))
	    continue;
	he->p.argv[ac++] = t;
	t = stpcpy( stpcpy(t, instance), ", '");
	t = strcpy(t, DN.argv[DI.ui32p[i]]);	t += strlen(t);
	t = strcpy(t, BN.argv[i]);		t += strlen(t);
	t = stpcpy(t, "', 'dir'");
	*t++ = '\0';
    }
    for (i = 0; i < c; i++) {
	if (lvl > 0 && FDGSkip(DN, BN, DI, i) != lvl)
	    continue;
	if (!(FFLAGS.ui32p[i] & 0x40))	/* XXX RPMFILE_GHOST */
	    continue;
	he->p.argv[ac++] = t;
	t = stpcpy( stpcpy(t, instance), ", '");
	t = strcpy(t, DN.argv[DI.ui32p[i]]);	t += strlen(t);
	t = strcpy(t, BN.argv[i]);		t += strlen(t);
	t = stpcpy(t, "', 'ghost'");
	*t++ = '\0';
    }

    he->p.argv[he->c] = NULL;
    rc = 0;

exit:
    BN.argv = _free(BN.argv);
    DN.argv = _free(DN.argv);
    return rc;
}

static int F1sqlTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->tag = RPMTAG_BASENAMES;
    return FDGsqlTag(h, he, 1);
}

static int F2sqlTag(Header h, HE_t he)
	/*@modifies he @*/
{
    he->tag = RPMTAG_BASENAMES;
    return FDGsqlTag(h, he, 2);
}

/**
 * Encode the basename of a string for use in XML CDATA.
 * @param he            tag container
 * @param av		parameter array (or NULL)
 * @return              formatted string
 */
static /*@only@*/ char * bncdataFormat(HE_t he, /*@null@*/ const char ** av)
	/*@*/
{
    char * val;

    if (he->t != RPM_STRING_TYPE) {
	val = xstrdup(_("(not a string)"));
    } else {
	const char * bn;
	const char * s;
	size_t nb;
	char * t;

	/* Get rightmost '/' in string (i.e. basename(3) behavior). */
	if ((bn = strrchr(he->p.str, '/')) != NULL)
	    bn++;
	else
	    bn = he->p.str;

	/* Convert to utf8, escape for XML CDATA. */
	s = strdup_locale_convert(bn, (av ? av[0] : NULL));
	nb = xmlstrlen(s);
	val = t = xcalloc(1, nb + 1);
	t = xmlstrcpy(t, s);	t += strlen(t);
	*t = '\0';
	s = _free(s);
    }

/*@-globstate@*/
    return val;
/*@=globstate@*/
}

typedef struct key_s {
/*@observer@*/
	const char *name;		/* key name */
	uint32_t value;
} KEY;

/*@unchecked@*/ /*@observer@*/
static KEY keyDigests[] = {
    { "adler32",	PGPHASHALGO_ADLER32 },
    { "crc32",		PGPHASHALGO_CRC32 },
    { "crc64",		PGPHASHALGO_CRC64 },
    { "haval160",	PGPHASHALGO_HAVAL_5_160 },
    { "jlu32",		PGPHASHALGO_JLU32 },
    { "md2",		PGPHASHALGO_MD2 },
    { "md4",		PGPHASHALGO_MD4 },
    { "md5",		PGPHASHALGO_MD5 },
    { "rmd128",		PGPHASHALGO_RIPEMD128 },
    { "rmd160",		PGPHASHALGO_RIPEMD160 },
    { "rmd256",		PGPHASHALGO_RIPEMD256 },
    { "rmd320",		PGPHASHALGO_RIPEMD320 },
    { "salsa10",	PGPHASHALGO_SALSA10 },
    { "salsa20",	PGPHASHALGO_SALSA20 },
    { "sha1",		PGPHASHALGO_SHA1 },
    { "sha224",		PGPHASHALGO_SHA224 },
    { "sha256",		PGPHASHALGO_SHA256 },
    { "sha384",		PGPHASHALGO_SHA384 },
    { "sha512",		PGPHASHALGO_SHA512 },
    { "tiger192",	PGPHASHALGO_TIGER192 },
};
/*@unchecked@*/
static size_t nkeyDigests = sizeof(keyDigests) / sizeof(keyDigests[0]);

/**
 * Bit field enum for stat(2) keys.
 */
enum keyStat_e {
    STAT_KEYS_NONE	= 0,
    STAT_KEYS_DEV	= (1U <<  0),	/*!< st_dev */
    STAT_KEYS_INO	= (1U <<  1),	/*!< st_ino */
    STAT_KEYS_MODE	= (1U <<  2),	/*!< st_mode */
    STAT_KEYS_NLINK	= (1U <<  3),	/*!< st_nlink */
    STAT_KEYS_UID	= (1U <<  4),	/*!< st_uid */
    STAT_KEYS_GID	= (1U <<  5),	/*!< st_gid */
    STAT_KEYS_RDEV	= (1U <<  6),	/*!< st_rdev */
    STAT_KEYS_SIZE	= (1U <<  7),	/*!< st_size */
    STAT_KEYS_BLKSIZE	= (1U <<  8),	/*!< st_blksize */
    STAT_KEYS_BLOCKS	= (1U <<  9),	/*!< st_blocks */
    STAT_KEYS_ATIME	= (1U << 10),	/*!< st_atime */
    STAT_KEYS_CTIME	= (1U << 11),	/*!< st_ctime */
    STAT_KEYS_MTIME	= (1U << 12),	/*!< st_mtime */
#ifdef	NOTYET
    STAT_KEYS_FLAGS	= (1U << 13),	/*!< st_flags */
#endif
    STAT_KEYS_SLINK	= (1U << 14),	/*!< symlink */
    STAT_KEYS_DIGEST	= (1U << 15),	/*!< digest */
#ifdef	NOTYET
    STAT_KEYS_FCONTEXT	= (1U << 16),	/*!< fcontext */
#endif
    STAT_KEYS_UNAME	= (1U << 17),	/*!< user name */
    STAT_KEYS_GNAME	= (1U << 18),	/*!< group name */
};

/*@unchecked@*/ /*@observer@*/
static KEY keyStat[] = {
    { "adler32",	STAT_KEYS_DIGEST },
    { "atime",		STAT_KEYS_ATIME },
    { "ctime",		STAT_KEYS_CTIME },
    { "blksize",	STAT_KEYS_BLKSIZE },
    { "blocks",		STAT_KEYS_BLOCKS },
    { "crc32",		STAT_KEYS_DIGEST },
    { "crc64",		STAT_KEYS_DIGEST },
    { "dev",		STAT_KEYS_DEV },
#ifdef	NOTYET
    { "digest",		STAT_KEYS_DIGEST },
    { "fcontext",	STAT_KEYS_FCONTEXT },
    { "flags",		STAT_KEYS_FLAGS },
#endif
    { "gid",		STAT_KEYS_GID },
    { "gname",		STAT_KEYS_GNAME },
    { "haval160",	STAT_KEYS_DIGEST },
    { "ino",		STAT_KEYS_INO },
    { "jlu32",		STAT_KEYS_DIGEST },
    { "link",		STAT_KEYS_SLINK },
    { "md2",		STAT_KEYS_DIGEST },
    { "md4",		STAT_KEYS_DIGEST },
    { "md5",		STAT_KEYS_DIGEST },
    { "mode",		STAT_KEYS_MODE },
    { "mtime",		STAT_KEYS_MTIME },
    { "nlink",		STAT_KEYS_NLINK },
    { "rdev",		STAT_KEYS_RDEV },
    { "rmd128",		STAT_KEYS_DIGEST },
    { "rmd160",		STAT_KEYS_DIGEST },
    { "rmd256",		STAT_KEYS_DIGEST },
    { "rmd320",		STAT_KEYS_DIGEST },
    { "salsa10",	STAT_KEYS_DIGEST },
    { "salsa20",	STAT_KEYS_DIGEST },
    { "sha1",		STAT_KEYS_DIGEST },
    { "sha224",		STAT_KEYS_DIGEST },
    { "sha256",		STAT_KEYS_DIGEST },
    { "sha384",		STAT_KEYS_DIGEST },
    { "sha512",		STAT_KEYS_DIGEST },
    { "size",		STAT_KEYS_SIZE },
    { "tiger192",	STAT_KEYS_DIGEST },
    { "uid",		STAT_KEYS_UID },
    { "uname",		STAT_KEYS_UNAME },
};
/*@unchecked@*/
static size_t nkeyStat = sizeof(keyStat) / sizeof(keyStat[0]);

/**
 * Bit field enum for stat(2) keys.
 */
enum keyUuids_e {
    UUID_KEYS_NONE	= (0U <<  0),
    UUID_KEYS_V1	= (1U <<  0),
    UUID_KEYS_V3	= (3U <<  0),
    UUID_KEYS_V4	= (4U <<  0),
    UUID_KEYS_V5	= (5U <<  0),
#ifdef	NOTYET
    UUID_KEYS_STRING	= (0U <<  4),
    UUID_KEYS_SIV	= (1U <<  4),
    UUID_KEYS_BINARY	= (2U <<  4),
    UUID_KEYS_TEXT	= (3U <<  4),
#endif
};

/*@unchecked@*/ /*@observer@*/
static KEY keyUuids[] = {
#ifdef	NOTYET
    { "binary",		UUID_KEYS_BINARY },
    { "siv",		UUID_KEYS_SIV },
    { "string",		UUID_KEYS_STRING },
    { "text",		UUID_KEYS_TEXT },
#endif
    { "v1",		UUID_KEYS_V1 },
    { "v3",		UUID_KEYS_V3 },
    { "v4",		UUID_KEYS_V4 },
    { "v5",		UUID_KEYS_V5 },
};
/*@unchecked@*/
static size_t nkeyUuids = sizeof(keyUuids) / sizeof(keyUuids[0]);

/**
 */
static int
keyCmp(const void * a, const void * b)
	/*@*/
{
    return strcmp(((KEY *)a)->name, ((KEY *)b)->name);
}

/**
 */
static uint32_t
keyValue(KEY * keys, size_t nkeys, /*@null@*/ const char *name)
	/*@*/
{
    uint32_t keyval = 0;

    if (name && * name) {
	KEY needle = { .name = name };
	KEY *k = (KEY *)bsearch(&needle, keys, nkeys, sizeof(*keys), keyCmp);
	if (k)
	    keyval = k->value;
    }
    return keyval;
}

/**
 * Return digest of tag data.
 * @param he		tag container
 * @param av		parameter list (NULL uses md5)
 * @return		formatted string
 */
static /*@only@*/ char * digestFormat(HE_t he, /*@null@*/ const char ** av)
	/*@*/
{
    int ix = (he->ix > 0 ? he->ix : 0);
    char * val = NULL;
    size_t ns;

assert(ix == 0);
    switch(he->t) {
    default:
	val = xstrdup(_("(invalid type :digest)"));
	goto exit;
	/*@notreached@*/ break;
    case RPM_UINT64_TYPE:
	ns = sizeof(he->p.ui64p[0]);
	break;
    case RPM_STRING_TYPE:
	ns = strlen(he->p.str);
	break;
    case RPM_BIN_TYPE:
	ns = he->c;
	break;
    }

    {	uint32_t keyval = keyValue(keyDigests, nkeyDigests, (av ? av[0] : NULL));
	uint32_t algo = (keyval ? keyval : PGPHASHALGO_SHA1);
	DIGEST_CTX ctx = rpmDigestInit(algo, 0);
	int xx = rpmDigestUpdate(ctx, he->p.ptr, ns);
	xx = rpmDigestFinal(ctx, &val, NULL, 1);
    }

exit:
/*@-globstate@*/
    return val;
/*@=globstate@*/
}

/**
 * Return file info.
 * @param he		tag container
 * @param av		parameter list (NULL uses sha1)
 * @return		formatted string
 */
static /*@only@*/ char * statFormat(HE_t he, /*@null@*/ const char ** av)
	/*@*/
{
    /*@unchecked@*/
    static const char *avdefault[] = { "mode", NULL };
    const char * fn = NULL;
    struct stat sb, *st = &sb;
    int ix = (he->ix > 0 ? he->ix : 0);
    char * val = NULL;
    int xx;
    int i;

assert(ix == 0);
    switch(he->t) {
    case RPM_BIN_TYPE:
	/* XXX limit to RPMTAG_PACKAGESTAT ... */
	if (he->tag == RPMTAG_PACKAGESTAT)
	if (he->c == sizeof(*st)) {
	    st = (struct stat *)he->p.ptr;
	    break;
	}
	/*@fallthrough @*/
    default:
	val = xstrdup(_("(invalid type :stat)"));
	goto exit;
	/*@notreached@*/ break;
    case RPM_STRING_TYPE:
	fn = he->p.str;
	if (Lstat(fn, st) == 0)
	    break;
	val = rpmExpand("(Lstat:", fn, ":", strerror(errno), ")", NULL);
	goto exit;
	/*@notreached@*/ break;
    }

    if (!(av && av[0] && *av[0]))
	av = avdefault;
    for (i = 0; av[i] != NULL; i++) {
	char b[BUFSIZ];
	size_t nb = sizeof(b);
	char * nval;
	uint32_t keyval = keyValue(keyStat, nkeyStat, av[i]);

	nval = NULL;
	b[0] = '\0';
	switch (keyval) {
	default:
	    break;
	case STAT_KEYS_NONE:
	    break;
	case STAT_KEYS_DEV:
	    xx = snprintf(b, nb, "0x%lx", (unsigned long)st->st_dev);
	    break;
	case STAT_KEYS_INO:
	    xx = snprintf(b, nb, "0x%lx", (unsigned long)st->st_ino);
	    break;
	case STAT_KEYS_MODE:
	    xx = snprintf(b, nb, "%06o", (unsigned)st->st_mode);
	    break;
	case STAT_KEYS_NLINK:
	    xx = snprintf(b, nb, "0x%ld", (unsigned long)st->st_nlink);
	    break;
	case STAT_KEYS_UID:
	    xx = snprintf(b, nb, "%ld", (unsigned long)st->st_uid);
	    break;
	case STAT_KEYS_GID:
	    xx = snprintf(b, nb, "%ld", (unsigned long)st->st_gid);
	    break;
	case STAT_KEYS_RDEV:
	    xx = snprintf(b, nb, "0x%lx", (unsigned long)st->st_rdev);
	    break;
	case STAT_KEYS_SIZE:
	    xx = snprintf(b, nb, "%ld", (unsigned long)st->st_size);
	    break;
	case STAT_KEYS_BLKSIZE:
	    xx = snprintf(b, nb, "%ld", (unsigned long)st->st_blksize);
	    break;
	case STAT_KEYS_BLOCKS:
	    xx = snprintf(b, nb, "%ld", (unsigned long)st->st_blocks);
	    break;
	case STAT_KEYS_ATIME:
	    (void) stpcpy(b, ctime(&st->st_atime));
	    break;
	case STAT_KEYS_CTIME:
	    (void) stpcpy(b, ctime(&st->st_ctime));
	    break;
	case STAT_KEYS_MTIME:
	    (void) stpcpy(b, ctime(&st->st_mtime));
	    break;
#ifdef	NOTYET
	case STAT_KEYS_FLAGS:
	    break;
#endif
	case STAT_KEYS_SLINK:
	    if (fn != NULL && S_ISLNK(st->st_mode)) {
		ssize_t size = Readlink(fn, b, nb);
		if (size == -1) {
		    nval = rpmExpand("(Readlink:", fn, ":", strerror(errno), ")", NULL);
		    stpcpy(b, nval);
		    nval = _free(nval);
		} else
		    b[size] = '\0';
	    }
	    break;
	case STAT_KEYS_DIGEST:
	    if (fn != NULL && S_ISREG(st->st_mode)) {
		uint32_t digval = keyValue(keyDigests, nkeyDigests, av[i]);
		uint32_t algo = (digval ? digval : PGPHASHALGO_SHA1);
		FD_t fd = Fopen(fn, "r%{?_rpmgio}");
		if (fd == NULL || Ferror(fd)) {
		    nval = rpmExpand("(Fopen:", fn, ":", Fstrerror(fd), ")", NULL);
		} else {
		    static int asAscii = 1;
		    char buffer[16 * 1024];
		    fdInitDigest(fd, algo, 0);
		    while (Fread(buffer, sizeof(buffer[0]), sizeof(buffer), fd) > 0)
			{};
		    if (Ferror(fd))
			nval = rpmExpand("(Fread:", fn, ":", Fstrerror(fd), ")", NULL);
		    else
			fdFiniDigest(fd, algo, &nval, NULL, asAscii);
	    }
		if (nval) {
		    stpcpy(b, nval);
		    nval = _free(nval);
		}
		if (fd != NULL)
		    xx = Fclose(fd);
	    }
	    break;
	case STAT_KEYS_UNAME:
	    (void) stpcpy(b, uidToUname(st->st_uid));
	    break;
	case STAT_KEYS_GNAME:
	    (void) stpcpy(b, gidToGname(st->st_gid));
	    break;
	}
	if (b[0] == '\0')
	    continue;
	b[nb-1] = '\0';

	if (val == NULL)
	    val = xstrdup(b);
	else {
	    nval = rpmExpand(val, " | ", b, NULL);
	    val = _free(val);
	    val = nval;
	}
    }

exit:
/*@-globstate@*/
    return val;
/*@=globstate@*/
}

/**
 * Reformat tag string as a UUID.
 * @param he		tag container
 * @param av		parameter list (NULL uses UUIDv5)
 * @return		formatted string
 */
static /*@only@*/ char * uuidFormat(HE_t he, /*@null@*/ const char ** av)
	/*@*/
{
    /*@unchecked@*/
    static const char *avdefault[] = { "v5", NULL };
    int version = 0;
    int ix = (he->ix > 0 ? he->ix : 0);
    char * val = NULL;
    int i;

assert(ix == 0);
    switch(he->t) {
    default:
	val = xstrdup(_("(invalid type :uuid)"));
	goto exit;
	/*@notreached@*/ break;
    case RPM_STRING_TYPE:
	break;
    }

    if (!(av && av[0] && *av[0]))
	av = avdefault;

    for (i = 0; av[i] != NULL; i++) {
	uint32_t keyval = keyValue(keyUuids, nkeyUuids, av[i]);

	switch (keyval) {
	default:
	    break;
	case UUID_KEYS_V1:
	case UUID_KEYS_V3:
	case UUID_KEYS_V4:
	case UUID_KEYS_V5:
	    version = keyval;
	    break;
	}
    }

    /* XXX use private tag container to avoid memory issues for now. */
    {	HE_t nhe = memset(alloca(sizeof(*nhe)), 0, sizeof(*nhe));
	int xx;
	nhe->tag = he->tag;
	nhe->t = he->t;
	nhe->p.str = xstrdup(he->p.str);
	nhe->c = he->c;
	val = xmalloc((128/4 + 4) + 1);
	xx = str2uuid(nhe, NULL, version, val);
	nhe->p.ptr = _free(nhe->p.ptr);
    }

exit:
/*@-globstate@*/
    return val;
/*@=globstate@*/
}

/**
 * Return arithmetic expressions of input.
 * @param he		tag container
 * @param av		parameter list (NULL uses sha1)
 * @return		formatted string
 */
static /*@only@*/ char * rpnFormat(HE_t he, /*@null@*/ const char ** av)
	/*@*/
{
    int ac = argvCount(av) + 1;
    int64_t * stack = memset(alloca(ac*sizeof(*stack)), 0, (ac*sizeof(*stack)));
    char * end;
    char * val = NULL;
    int ix = 0;
    int i;

    switch(he->t) {
    default:
	val = xstrdup(_("(invalid type :rpn)"));
	goto exit;
	/*@notreached@*/ break;
    case RPM_UINT64_TYPE:
	stack[ix] = he->p.ui64p[0];
	break;
    case RPM_STRING_TYPE:
	end = NULL;
	stack[ix] = strtoll(he->p.str, &end, 0);
	if (*end != '\0') {
	    val = xstrdup(_("(invalid string :rpn)"));
	    goto exit;
	}
	break;
    }

    if (av != NULL)
    for (i = 0; av[i] != NULL; i++) {
	const char * arg = av[i];
	size_t len = strlen(arg);
	int c = *arg;

	if (len == 0) {
	    /* do nothing */
	} else if (len > 1) {
	    if (!(xisdigit(c) || (c == '-' && xisdigit(arg[1])))) {
		val = xstrdup(_("(expected number :rpn)"));
		goto exit;
	    }
	    if (++ix == ac) {
		val = xstrdup(_("(stack overflow :rpn)"));
		goto exit;
	    }
	    end = NULL;
	    stack[ix] = strtoll(arg, &end, 0);
	    if (*end != '\0') {
		val = xstrdup(_("(invalid number :rpn)"));
		goto exit;
	    }
	} else {
	    if (ix-- < 1) {
		val = xstrdup(_("(stack underflow :rpn)"));
		goto exit;
	    }
	    switch (c) {
	    case '&':	stack[ix] &= stack[ix+1];	break;
	    case '|':	stack[ix] |= stack[ix+1];	break;
	    case '^':	stack[ix] ^= stack[ix+1];	break;
	    case '+':	stack[ix] += stack[ix+1];	break;
	    case '-':	stack[ix] -= stack[ix+1];	break;
	    case '*':	stack[ix] *= stack[ix+1];	break;
	    case '%':	
	    case '/':	
		if (stack[ix+1] == 0) {
		    val = xstrdup(_("(divide by zero :rpn)"));
		    goto exit;
		}
		if (c == '%')
		    stack[ix] %= stack[ix+1];
		else
		    stack[ix] /= stack[ix+1];
		break;
	    }
	}
    }

    {	HE_t nhe = memset(alloca(sizeof(*nhe)), 0, sizeof(*nhe));
	nhe->tag = he->tag;
	nhe->t = RPM_UINT64_TYPE;
	nhe->p.ui64p = (uint64_t *)&stack[ix];
	nhe->c = 1;
	val = intFormat(nhe, NULL, NULL);
    }

exit:
/*@-globstate@*/
    return val;
/*@=globstate@*/
}

/*@-type@*/ /* FIX: cast? */
const struct headerSprintfExtension_s rpmHeaderFormats[] = {
    { HEADER_EXT_TAG, "RPMTAG_BUILDTIMEUUID",
	{ .tagFunction = buildtime_uuidTag } },
    { HEADER_EXT_TAG, "RPMTAG_CHANGELOGNAME",
	{ .tagFunction = changelognameTag } },
    { HEADER_EXT_TAG, "RPMTAG_CHANGELOGTEXT",
	{ .tagFunction = changelogtextTag } },
    { HEADER_EXT_TAG, "RPMTAG_DESCRIPTION",
	{ .tagFunction = descriptionTag } },
    { HEADER_EXT_TAG, "RPMTAG_ENHANCES",
	{ .tagFunction = missingokTag } },
    { HEADER_EXT_TAG, "RPMTAG_FILECLASS",
	{ .tagFunction = fileclassTag } },
    { HEADER_EXT_TAG, "RPMTAG_FILECONTEXTS",
	{ .tagFunction = filecontextsTag } },
    { HEADER_EXT_TAG, "RPMTAG_FILENAMES",
	{ .tagFunction = filepathsTag } },
    { HEADER_EXT_TAG, "RPMTAG_ORIGPATHS",
	{ .tagFunction = origpathsTag } },
    { HEADER_EXT_TAG, "RPMTAG_FILEPROVIDE",
	{ .tagFunction = fileprovideTag } },
    { HEADER_EXT_TAG, "RPMTAG_FILEREQUIRE",
	{ .tagFunction = filerequireTag } },
    { HEADER_EXT_TAG, "RPMTAG_FSCONTEXTS",
	{ .tagFunction = fscontextsTag } },
    { HEADER_EXT_TAG, "RPMTAG_FSNAMES",	
	{ .tagFunction = fsnamesTag } },
    { HEADER_EXT_TAG, "RPMTAG_FSSIZES",	
	{ .tagFunction = fssizesTag } },
    { HEADER_EXT_TAG, "RPMTAG_GROUP",
	{ .tagFunction = groupTag } },
    { HEADER_EXT_TAG, "RPMTAG_HDRUUID",
	{ .tagFunction = hdruuidTag } },
    { HEADER_EXT_TAG, "RPMTAG_INSTALLPREFIX",
	{ .tagFunction = instprefixTag } },
    { HEADER_EXT_TAG, "RPMTAG_INSTALLTIDUUID",
	{ .tagFunction = installtid_uuidTag } },
    { HEADER_EXT_TAG, "RPMTAG_INSTALLTIMEUUID",
	{ .tagFunction = installtime_uuidTag } },
    { HEADER_EXT_TAG, "RPMTAG_ORIGINTIDUUID",
	{ .tagFunction = origintid_uuidTag } },
    { HEADER_EXT_TAG, "RPMTAG_ORIGINTIMEUUID",
	{ .tagFunction = origintime_uuidTag } },
    { HEADER_EXT_TAG, "RPMTAG_PKGUUID",
	{ .tagFunction = pkguuidTag } },
    { HEADER_EXT_TAG, "RPMTAG_RECONTEXTS",
	{ .tagFunction = recontextsTag } },
    { HEADER_EXT_TAG, "RPMTAG_REMOVETIDUUID",
	{ .tagFunction = removetid_uuidTag } },
    { HEADER_EXT_TAG, "RPMTAG_SUGGESTS",
	{ .tagFunction = missingokTag } },
    { HEADER_EXT_TAG, "RPMTAG_SOURCEPKGUUID",
	{ .tagFunction = sourcepkguuidTag } },
    { HEADER_EXT_TAG, "RPMTAG_SUMMARY",
	{ .tagFunction = summaryTag } },
    { HEADER_EXT_TAG, "RPMTAG_TRIGGERCONDS",
	{ .tagFunction = triggercondsTag } },
    { HEADER_EXT_TAG, "RPMTAG_TRIGGERTYPE",
	{ .tagFunction = triggertypeTag } },
    { HEADER_EXT_TAG, "RPMTAG_DBINSTANCE",
	{ .tagFunction = dbinstanceTag } },
    { HEADER_EXT_TAG, "RPMTAG_HEADERSTARTOFF",
	{ .tagFunction = headerstartoffTag } },
    { HEADER_EXT_TAG, "RPMTAG_HEADERENDOFF",
	{ .tagFunction = headerendoffTag } },
    { HEADER_EXT_TAG, "RPMTAG_PACKAGEBASEURL",
	{ .tagFunction = pkgbaseurlTag } },
    { HEADER_EXT_TAG, "RPMTAG_PACKAGEDIGEST",
	{ .tagFunction = pkgdigestTag } },
    { HEADER_EXT_TAG, "RPMTAG_PACKAGEORIGIN",
	{ .tagFunction = pkgoriginTag } },
    { HEADER_EXT_TAG, "RPMTAG_PACKAGESIZE",
	{ .tagFunction = pkgsizeTag } },
    { HEADER_EXT_TAG, "RPMTAG_PACKAGETIME",
	{ .tagFunction = pkgmtimeTag } },
    { HEADER_EXT_TAG, "RPMTAG_NVRA",
	{ .tagFunction = nvraTag } },
    { HEADER_EXT_TAG, "RPMTAG_PROVIDEXMLENTRY",
	{ .tagFunction = PxmlTag } },
    { HEADER_EXT_TAG, "RPMTAG_REQUIREXMLENTRY",
	{ .tagFunction = RxmlTag } },
    { HEADER_EXT_TAG, "RPMTAG_CONFLICTXMLENTRY",
	{ .tagFunction = CxmlTag } },
    { HEADER_EXT_TAG, "RPMTAG_OBSOLETEXMLENTRY",
	{ .tagFunction = OxmlTag } },
    { HEADER_EXT_TAG, "RPMTAG_FILESXMLENTRY1",
	{ .tagFunction = F1xmlTag } },
    { HEADER_EXT_TAG, "RPMTAG_FILESXMLENTRY2",
	{ .tagFunction = F2xmlTag } },
    { HEADER_EXT_TAG, "RPMTAG_PROVIDESQLENTRY",
	{ .tagFunction = PsqlTag } },
    { HEADER_EXT_TAG, "RPMTAG_REQUIRESQLENTRY",
	{ .tagFunction = RsqlTag } },
    { HEADER_EXT_TAG, "RPMTAG_CONFLICTSQLENTRY",
	{ .tagFunction = CsqlTag } },
    { HEADER_EXT_TAG, "RPMTAG_OBSOLETESQLENTRY",
	{ .tagFunction = OsqlTag } },
    { HEADER_EXT_TAG, "RPMTAG_FILESSQLENTRY1",
	{ .tagFunction = F1sqlTag } },
    { HEADER_EXT_TAG, "RPMTAG_FILESSQLENTRY2",
	{ .tagFunction = F2sqlTag } },
    { HEADER_EXT_FORMAT, "armor",
	{ .fmtFunction = armorFormat } },
    { HEADER_EXT_FORMAT, "base64",
	{ .fmtFunction = base64Format } },
    { HEADER_EXT_FORMAT, "bncdata",
	{ .fmtFunction = bncdataFormat } },
    { HEADER_EXT_FORMAT, "cdata",
	{ .fmtFunction = cdataFormat } },
    { HEADER_EXT_FORMAT, "depflags",
	{ .fmtFunction = depflagsFormat } },
    { HEADER_EXT_FORMAT, "digest",
	{ .fmtFunction = digestFormat } },
    { HEADER_EXT_FORMAT, "fflags",
	{ .fmtFunction = fflagsFormat } },
    { HEADER_EXT_FORMAT, "iconv",
	{ .fmtFunction = iconvFormat } },
    { HEADER_EXT_FORMAT, "perms",
	{ .fmtFunction = permsFormat } },
    { HEADER_EXT_FORMAT, "permissions",	
	{ .fmtFunction = permsFormat } },
    { HEADER_EXT_FORMAT, "pgpsig",
	{ .fmtFunction = pgpsigFormat } },
    { HEADER_EXT_FORMAT, "rpn",
	{ .fmtFunction = rpnFormat } },
    { HEADER_EXT_FORMAT, "sqlescape",
	{ .fmtFunction = sqlescapeFormat } },
    { HEADER_EXT_FORMAT, "stat",
	{ .fmtFunction = statFormat } },
    { HEADER_EXT_FORMAT, "triggertype",
	{ .fmtFunction = triggertypeFormat } },
    { HEADER_EXT_FORMAT, "utf8",
	{ .fmtFunction = iconvFormat } },
    { HEADER_EXT_FORMAT, "uuid",
	{ .fmtFunction = uuidFormat } },
    { HEADER_EXT_FORMAT, "xml",	
	{ .fmtFunction = xmlFormat } },
    { HEADER_EXT_FORMAT, "yaml",
	{ .fmtFunction = yamlFormat } },
    { HEADER_EXT_MORE, NULL,
	{ .more = (void *) headerDefaultFormats } }
} ;
/*@=type@*/

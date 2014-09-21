/** \ingroup rpmio
 * \file rpmio/strtolocale.c
 */

#include "system.h"

#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif
#ifdef HAVE_ICONV
#include <iconv.h>
#endif
#include <rpmiotypes.h>

#include "debug.h"

const char * xstrtolocale(const char *str)
{
#ifdef HAVE_ICONV
    static char *locale_encoding = NULL;
    static int locale_encoding_is_utf8 = 0;
    iconv_t cd;
    size_t src_size, dest_size;
    char *result, *dest;
    const char *src;

    if (locale_encoding == NULL) {
#ifdef HAVE_LANGINFO_H
	const char *encoding = nl_langinfo(CODESET);
#else
	const char *encoding = "char";
#endif
	locale_encoding = (char *) xmalloc(strlen(encoding) + 11);
	sprintf(locale_encoding, "%s//TRANSLIT", encoding);
	locale_encoding_is_utf8 = strcasecmp(encoding, "UTF-8") == 0;
    }

    if (!str || !*str || locale_encoding_is_utf8)
	return str;

    cd = iconv_open(locale_encoding, "UTF-8");
    if (cd == (iconv_t)-1)
	return str;

    src_size = strlen(str);
    dest_size = src_size + 1;
    result = (char *) xmalloc(dest_size);
    src = str;
    dest = result;
    for(;;) {
	size_t status = iconv(cd, (char **)&src, &src_size, &dest, &dest_size);
	if (status == (size_t)-1) {
	    size_t dest_offset;
	    if (errno != E2BIG) {
		free(result);
		(void) iconv_close(cd);
		return str;
	    }
	    dest_offset = dest - result;
	    dest_size += 16;
	    result = (char *) xrealloc(result, dest_offset + dest_size);
	    dest = result + dest_offset;
	} else if (src_size == 0) {
	    if (src == NULL) break;
	    src = NULL;
	}
    }
    (void) iconv_close(cd);
    free((void *)str);
    if (dest_size == 0) {
	size_t dest_offset = dest - result;
	result = (char *) xrealloc(result, dest_offset + 1);
	dest = result + dest_offset;
    }
    *dest = '\0';
    return result;
#else
    return str;
#endif
}

#if defined(__GLIBC__)	/* XXX todo: find where iconv(3) was implemented. */
/* XXX using "//TRANSLIT" instead assumes known fromcode? */
static const char * _iconv_tocode = "UTF-8//IGNORE";
static const char * _iconv_fromcode = "UTF-8";
#else
static const char * _iconv_tocode = "UTF-8";
static const char * _iconv_fromcode = NULL;
#endif

char * xstrdup_iconv_check (const char * buffer, const char * tocode)
{
    const char *s = buffer;
    char *t = NULL;
#if defined(HAVE_ICONV)
    const char *fromcode = _iconv_fromcode;
    iconv_t fd;

assert(buffer != NULL);

    if (tocode == NULL)
	tocode = _iconv_tocode;
assert(tocode != NULL);

#ifdef HAVE_LANGINFO_H
    /* XXX the current locale's encoding != package data encodings. */
    if (fromcode == NULL)
        fromcode = nl_langinfo (CODESET);
#endif
assert(fromcode != NULL);

    if ((fd = iconv_open(tocode, fromcode)) != (iconv_t)-1) {
	size_t ileft = strlen(s);
	size_t nt = ileft;
	char * te = t = xmalloc((nt + 1) * sizeof(*t));
	size_t oleft = ileft;
	size_t err = iconv(fd, NULL, NULL, NULL, NULL);
	const char *sprev = NULL;
	int _iconv_errno = 0;
	int done = 0;

	while (done == 0 && _iconv_errno == 0) {
	    err = iconv(fd, (char **)&s, &ileft, &te, &oleft);
	    if (err == (size_t)-1) {
		switch (errno) {
		case E2BIG:
		{   size_t used = (size_t)(te - t);
		    nt *= 2;
		    t = xrealloc(t, (nt + 1) * sizeof(*t));
		    te = t + used;
		    oleft = nt - used;
		}   break;
		case EINVAL:
		    done = 1;
		    _iconv_errno = errno;
		    break;
		case EILSEQ:
		default:
		    _iconv_errno = errno;
		    break;
		}
	    } else
	    if (sprev == NULL) {
		sprev = s;
		s = NULL;
		ileft = 0;
	    } else
	        done = 1;
	}
	if (iconv_close(fd))
	    _iconv_errno = errno;
	*te = '\0';
	te = xstrdup(t);
	t = _free(t);
	t = te;

if (_iconv_errno)
fprintf(stderr, "warning: %s: from iconv(%s -> %s) for \"%s\" -> \"%s\"\n", strerror(_iconv_errno), fromcode, tocode, buffer, t);

    } else
#endif
	t = xstrdup((s ? s : ""));

    return t;
}


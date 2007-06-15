/** \ingroup rpmio
 * \file rpmio/strtolocale.c
 */

#include "system.h"
#include <langinfo.h>
#ifdef HAVE_ICONV
#include <iconv.h>
#endif
#include "debug.h"

static char *locale_encoding = NULL;
static int locale_encoding_is_utf8 = 0;

const char * xstrtolocale(const char *str)
{
#ifdef HAVE_ICONV
    iconv_t cd;
    size_t src_size, dest_size;
    char *result, *src, *dest;

    if (locale_encoding == NULL) {
	const char *encoding = nl_langinfo(CODESET);
	locale_encoding = xmalloc(strlen(encoding) + 11);
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
    result = xmalloc(dest_size);
    src = (char *)str;
    dest = result;
    for(;;) {
	size_t status = iconv(cd, &src, &src_size, &dest, &dest_size);
	if (status == (size_t)-1) {
	    size_t dest_offset;
	    if (errno != E2BIG) {
		free(result);
		iconv_close(cd);
		return str;
	    }
	    dest_offset = dest - result;
	    dest_size += 16;
	    result = xrealloc(result, dest_offset + dest_size);
	    dest = result + dest_offset;
	} else if (src_size == 0) {
	    if (src == NULL) break;
	    src = NULL;
	}
    }
    iconv_close(cd);
    free((void *)str);
    if (dest_size == 0) {
	size_t dest_offset = dest - result;
	result = xrealloc(result, dest_offset + 1);
	dest = result + dest_offset;
    }
    *dest = '\0';
    return result;
#else
    return str;
#endif
}

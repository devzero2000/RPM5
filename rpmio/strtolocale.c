/** \ingroup rpmio
 * \file rpmio/strtolocale.c
 */

#include "system.h"
#include <wchar.h>
#include "debug.h"

/*@access mbstate_t @*/

/**
 * Wrapper to free(3), hides const compilation noise, permit NULL, return NULL.
 * @param p		memory to free
 * @retval		NULL always
 */
/*@unused@*/ static inline /*@null@*/ void *
_free(/*@only@*/ /*@null@*/ const void * p) /*@modifies p@*/
{
    if (p != NULL)	free((void *)p);
    return NULL;
}

const char * xstrtolocale(const char *str)
{
    wchar_t *wstr, *wp;
    const unsigned char *cp;
    char *cc;
    unsigned state = 0;
    int c;
    int ccl, cca, mb_cur_max;
    size_t l;
    mbstate_t ps;
    int strisutf8 = 1;
    int locisutf8 = 1;

    if (!str)
	return 0;
    if (!*str)
	return str;
    wstr = (wchar_t *)xmalloc((strlen(str) + 1) * sizeof(*wstr));
    wp = wstr;
    cp = (const unsigned char *)str;
    while ((c = *cp++) != 0) {
	if (state) {
	    if ((c & 0xc0) != 0x80) {
		/* encoding error */
		break;
	    }
	    c = (c & 0x3f) | (state << 6);
	    if (!(state & 0x40000000)) {
	      /* check for overlong sequences */
	        if ((c & 0x820823e0) == 0x80000000)
		    c = 0xfdffffff;
	        else if ((c & 0x020821f0) == 0x02000000)
		    c = 0xfff7ffff;
	        else if ((c & 0x000820f8) == 0x00080000)
		    c = 0xffffd000;
	        else if ((c & 0x0000207c) == 0x00002000)
		    c = 0xffffff70;
	    }
	} else {
	    /* new sequence */
	    if (c >= 0xfe)
		c = 0xfffd;
	    else if (c >= 0xfc)
		c = (c & 0x01) | 0xbffffffc;    /* 5 bytes to follow */
	    else if (c >= 0xf8)
		c = (c & 0x03) | 0xbfffff00;    /* 4 */ 
	    else if (c >= 0xf0)
		c = (c & 0x07) | 0xbfffc000;    /* 3 */ 
	    else if (c >= 0xe0)
		c = (c & 0x0f) | 0xbff00000;    /* 2 */ 
	    else if (c >= 0xc2)
		c = (c & 0x1f) | 0xfc000000;    /* 1 */ 
	    else if (c >= 0xc0)
		c = 0xfdffffff;         /* overlong */
	    else if (c >= 0x80)
		c = 0xfffd;
        }
	state = (c & 0x80000000) ? c : 0;
	if (state)
	    continue;
	*wp++ = (wchar_t)c;
    }
/*@-branchstate@*/
    if (state) {
	/* encoding error, assume latin1 */
        strisutf8 = 0;
	cp = (const unsigned char *)str;
	wp = wstr;
	while ((c = *cp++) != 0) {
	    *wp++ = (wchar_t)c;
	}
    }
/*@=branchstate@*/
    *wp = 0;
    mb_cur_max = MB_CUR_MAX;
    memset(&ps, 0, sizeof(ps));
    cc = xmalloc(mb_cur_max);
    /* test locale encoding */
    if (wcrtomb(cc, 0x20ac, &ps) != 3 || memcmp(cc, "\342\202\254", 3))
	locisutf8 = 0;
    if (locisutf8 == strisutf8) {
	wstr = _free(wstr);
	cc = _free(cc);		/* XXX memory leak plugged. */
	return str;
    }
    str = _free(str);
    memset(&ps, 0, sizeof(ps));
    ccl = cca = 0;
    for (wp = wstr; ; wp++) {
	l = wcrtomb(cc + ccl, *wp, &ps);
	if (*wp == 0)
	    break;
	if (l == (size_t)-1) {
	    if (*wp < (wchar_t)256 && mbsinit(&ps)) {
		cc[ccl] = *wp;
		l = 1;
	    } else
	        l = wcrtomb(cc + ccl, (wchar_t)'?', &ps);
	}
        if (l == 0 || l == (size_t)-1)
	    continue;
        ccl += l;
        if (ccl > cca) {
	    cca = ccl + 16;
	    cc = xrealloc(cc, cca + mb_cur_max);
	}
    }
    wstr = _free(wstr);
    return (const char *)cc;
}

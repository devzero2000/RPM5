/**
 * \file rpmio/stringbuf.c
 */

#include "system.h"

#include <rpmiotypes.h>
#include "stringbuf.h"
#include "debug.h"

#define BUF_CHUNK 1024

struct StringBufRec {
/*@owned@*/
    char * buf;
/*@dependent@*/
    char * tail;     /* Points to first "free" char */
    size_t allocated;
    size_t free;
};

StringBuf newStringBuf(void)
{
    StringBuf sb = xmalloc(sizeof(*sb));

    sb->free = sb->allocated = BUF_CHUNK;
    sb->buf = xcalloc(sb->allocated, sizeof(*sb->buf));
    sb->buf[0] = '\0';
    sb->tail = sb->buf;
    
    return sb;
}

StringBuf freeStringBuf(StringBuf sb)
{
    if (sb) {
	sb->buf = _free(sb->buf);
	sb = _free(sb);
    }
    return sb;
}

void truncStringBuf(StringBuf sb)
{
    sb->buf[0] = '\0';
    sb->tail = sb->buf;
    sb->free = sb->allocated;
}

void stripTrailingBlanksStringBuf(StringBuf sb)
{
    while (sb->free != sb->allocated) {
	if (!xisspace((int)*(sb->tail - 1)))
	    break;
	sb->free++;
	sb->tail--;
    }
    sb->tail[0] = '\0';
}

char * getStringBuf(StringBuf sb)
{
    return sb->buf;
}

void appendStringBufAux(StringBuf sb, const char *s, size_t nl)
{
    size_t l = strlen(s);

    /* If free == l there is no room for NULL terminator! */
    while ((l + nl) >= sb->free) {
        sb->allocated += BUF_CHUNK;
	sb->free += BUF_CHUNK;
        sb->buf = xrealloc(sb->buf, sb->allocated);
	sb->tail = sb->buf + (sb->allocated - sb->free);
    }
    
    /*@-mayaliasunique@*/ /* FIX: shrug */
    strcpy(sb->tail, s);
    /*@=mayaliasunique@*/
    sb->tail += l;
    sb->free -= l;
    if (nl) {
	*sb->tail++ = '\n';
	sb->free--;
	*sb->tail = '\0';
    }
}

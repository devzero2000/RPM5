/** \ingroup payload
 * \file rpmio/ar.c
 *  Handle ar(1) archives.
 */

#include "system.h"

#include <rpmio_internal.h>	/* XXX fdGetCpioPos AR_MAGIC */
#include <rpmlib.h>

#include "ar.h"
#include "fsm.h"
#include "ugid.h"

#include "debug.h"

/*@access FSM_t @*/

/*@unchecked@*/
int _ar_debug = 0;

/**
 *  * Vector to fsmNext.
 *   */
int (*_fsmNext) (void * _fsm, int nstage)
        /*@modifies _fsm @*/;

/**
 * Convert string to unsigned integer (with buffer size check).
 * @param str		input string
 * @retval endptr	address of 1st character not processed
 * @param base		numerical conversion base
 * @param num		max no. of bytes to read
 * @return		converted integer
 */
static int strntoul(const char *str, /*@out@*/char **endptr, int base, int num)
	/*@modifies *endptr @*/
{
    char * buf, * end;
    unsigned long ret;

    buf = alloca(num + 1);
    strncpy(buf, str, num);
    buf[num] = '\0';

    ret = strtoul(buf, &end, base);
    if (endptr != NULL) {
	if (*end != '\0')
	    *endptr = ((char *)str) + (end - buf);	/* XXX discards const */
	else
	    *endptr = ((char *)str) + strlen(buf);
    }

    return ret;
}

static ssize_t arRead(FSM_t fsm, void * buf, size_t count)
	/*@modifies fsm, *buf @*/
{
    char * t = buf;
    size_t nb = 0;

if (_ar_debug)
fprintf(stderr, "          arRead(%p, %p[%u])\n", fsm, buf, (unsigned)count);

    while (count > 0) {
	size_t rc;

	/* Read next ar block. */
	fsm->wrlen = count;
	rc = _fsmNext(fsm, FSM_DREAD);
	if (!rc && fsm->rdnb != fsm->wrlen)
	    rc = CPIOERR_READ_FAILED;
	if (rc) return -rc;

	/* Append to buffer. */
	rc = (count > fsm->rdnb ? fsm->rdnb : count);
	if (buf != fsm->wrbuf)
	     memcpy(t + nb, fsm->wrbuf, rc);
	nb += rc;
	count -= rc;
    }
    return nb;
}

int arHeaderRead(void * _fsm, struct stat * st)
	/*@modifies fsm, *st @*/
{
    FSM_t fsm = _fsm;
    arHeader hdr = (arHeader) fsm->wrbuf;
    ssize_t rc = 0;

if (_ar_debug)
fprintf(stderr, "    arHeaderRead(%p, %p)\n", fsm, st);

    /* XXX Read AR_MAGIC to beginning of ar(1) archive. */
    if (fdGetCpioPos(fsm->cfd) == 0) {
	(void) arRead(fsm, fsm->wrbuf, sizeof(AR_MAGIC)-1);
    }

top:
    rc = arRead(fsm, hdr, sizeof(*hdr));
    if (rc <= 0)	return -rc;
if (_ar_debug)
fprintf(stderr, "==> %p[%u] \"%.*s\"\n", hdr, (unsigned)rc, (int)sizeof(*hdr), (char *)hdr);
    rc = 0;

    /* Verify header marker. */
    if (strncmp(hdr->marker, AR_MARKER, sizeof(AR_MARKER)-1))
	return CPIOERR_BAD_MAGIC;

    st->st_size = strntoul(hdr->filesize, NULL, 10, sizeof(hdr->filesize));

    /* Special ar(1) members. */
    if (hdr->name[0] == '/') {
	/* GNU: on "//":	Save long member names. */
	if (hdr->name[1] == '/' && hdr->name[2] == ' ') {
	    char * t;
	    int i;

	    rc = arRead(fsm, fsm->wrbuf, st->st_size);
	    if (rc <= 0)	return -rc;

	    fsm->wrbuf[rc] = '\0';
	    fsm->lmtab = t = xstrdup(fsm->wrbuf);
	    fsm->lmtablen = rc;
	    fsm->lmtaboff = 0;

	    for (i = 1; i < fsm->lmtablen; i++) {
		t++;
		if (t[0] != '\n') continue;
		t[0] = '\0';
		/* GNU: trailing '/' to permit file names with trailing ' '. */
		if (t[-1] == '/') t[-1] = '\0';
	    }
	    goto top;
	}
	/* GNU: on "/":	Skip symbols (if any) */
	if (hdr->name[1] == ' ') {
	    rc = arRead(fsm, fsm->wrbuf, st->st_size);
	    if (rc <= 0)	return -rc;
	    goto top;
	}
	/* GNU: on "/123": Substitute long member name. */
	if (xisdigit(hdr->name[1])) {
	    char * te = NULL;
	    int i = strntoul(&hdr->name[1], &te, 10, sizeof(hdr->name)-2);
	    if (*te == ' ' && fsm->lmtab != NULL && i < fsm->lmtablen)
		fsm->path = xstrdup(fsm->lmtab + i);
	}
    } else
    if (hdr->name[0] != ' ') {	/* Short member name. */
	size_t nb = sizeof(hdr->name);
	char t[sizeof(hdr->name)+1];
	memcpy(t, hdr->name, nb);
	t[nb] = '\0';
	while (nb > 0 && t[nb-1] == ' ')
	    t[--nb] = '\0';
	/* GNU: trailing '/' to permit file names with trailing ' '. */
	if (nb > 0 && t[nb - 1] == '/')
	    t[--nb] = '\0';
	fsm->path = xstrdup(t);
    }

    st->st_mtime = strntoul(hdr->mtime, NULL, 10, sizeof(hdr->mtime));
    st->st_ctime = st->st_atime = st->st_mtime;

    st->st_uid = strntoul(hdr->uid, NULL, 10, sizeof(hdr->uid));
    st->st_gid = strntoul(hdr->gid, NULL, 10, sizeof(hdr->gid));

    st->st_mode = strntoul(hdr->mode, NULL, 8, sizeof(hdr->mode));

    st->st_nlink = 1;

if (_ar_debug)
fprintf(stderr, "\t     %06o%3d (%4d,%4d)%12d %s\n",
                (unsigned)st->st_mode, (int)st->st_nlink,
                (int)st->st_uid, (int)st->st_gid, (int)st->st_size,
                (fsm->path ? fsm->path : ""));

    return rc;
}

static ssize_t arWrite(FSM_t fsm, const void *buf, size_t count)
	/*@modifies fsm @*/
{
    const char * s = buf;
    size_t nb = 0;

if (_ar_debug)
fprintf(stderr, "    arWrite(%p, %p[%u])\n", fsm, buf, (unsigned)count);

    while (count > 0) {
	size_t rc;

	/* XXX DWRITE uses rdnb for I/O length. */
	fsm->rdnb = count;
	if (s != fsm->rdbuf)
	    memmove(fsm->rdbuf, s + nb, fsm->rdnb);

	rc = _fsmNext(fsm, FSM_DWRITE);
	if (!rc && fsm->rdnb != fsm->wrnb)
		rc = CPIOERR_WRITE_FAILED;
	if (rc) return -rc;

	nb += fsm->rdnb;
	count -= fsm->rdnb;
    }
    return nb;
}

int arHeaderWrite(void * _fsm, struct stat * st)
{
    FSM_t fsm = _fsm;
    arHeader hdr = (arHeader) fsm->rdbuf;
    size_t nb;
    int rc = 0;

if (_ar_debug)
fprintf(stderr, "    arHeaderWrite(%p, %p)\n", fsm, st);

    /* XXX Write AR_MAGIC to beginning of ar(1) archive. */
    if (fdGetCpioPos(fsm->cfd) == 0) {
	(void) arWrite(fsm, AR_MAGIC, sizeof(AR_MAGIC)-1);
    }

    memset(hdr, ' ', sizeof(*hdr));

    nb = strlen(fsm->path);
    if (nb >= sizeof(hdr->name))
	nb = sizeof(hdr->name)-1;
    strncpy(hdr->name, fsm->path, nb);
    hdr->name[nb] = '/';

    sprintf(hdr->mtime, "%-12d", (unsigned) (st->st_mtime & 037777777777));
    sprintf(hdr->uid, "%-6d", (unsigned int)(st->st_uid & 07777777));
    sprintf(hdr->gid, "%-6d", (unsigned int)(st->st_gid & 07777777));

    sprintf(hdr->mode, "%-8o", (unsigned int)(st->st_mode & 07777777));
    sprintf(hdr->filesize, "%-10d", (unsigned) (st->st_size & 037777777777));

    strncpy(hdr->marker, AR_MARKER, sizeof(AR_MARKER)-1);

rc = sizeof(*hdr);
if (_ar_debug)
fprintf(stderr, "==> %p[%u] \"%.*s\"\n", hdr, (unsigned)rc, (int)sizeof(*hdr), (char *)hdr);

    rc = arWrite(fsm, hdr, sizeof(*hdr));
    if (rc < 0)	return -rc;
    rc = 0;

    return rc;
}

int arTrailerWrite(void * _fsm)
{
    FSM_t fsm = _fsm;
    int rc = 0;

if (_ar_debug)
fprintf(stderr, "    arTrailerWrite(%p)\n", fsm);

    rc = _fsmNext(fsm, FSM_PAD);	/* XXX likely unnecessary. */

    return rc;
}

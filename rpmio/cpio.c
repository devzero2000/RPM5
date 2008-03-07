/** \ingroup payload
 * \file rpmio/cpio.c
 *  Handle cpio(1) archives.
 */

#include "system.h"

#include <rpmio.h>
#include <cpio.h>
#define	_IOSM_INTERNAL
#include <iosm.h>

#include "debug.h"

/*@access IOSM_t @*/

/*@unchecked@*/
int _cpio_debug = 0;

/**
 * Convert string to unsigned integer (with buffer size check).
 * @param str		input string
 * @retval endptr	address of 1st character not processed
 * @param base		numerical conversion base
 * @param num		max no. of bytes to read
 * @return		converted integer
 */
static int strntoul(const char *str, /*@out@*/char **endptr, int base, size_t num)
	/*@modifies *endptr @*/
	/*@requires maxSet(endptr) >= 0 @*/
{
    char * buf, * end;
    unsigned long ret;

    buf = alloca(num + 1);
    strncpy(buf, str, num);
    buf[num] = '\0';

    ret = strtoul(buf, &end, base);
    if (*end != '\0')
	*endptr = ((char *)str) + (end - buf);	/* XXX discards const */
    else
	*endptr = ((char *)str) + strlen(buf);

    return ret;
}

#define GET_NUM_FIELD(phys, log) \
	log = strntoul(phys, &end, 16, sizeof(phys)); \
	if ( (end - phys) != sizeof(phys) ) return IOSMERR_BAD_HEADER;
#define SET_NUM_FIELD(phys, val, space) \
	sprintf(space, "%8.8lx", (unsigned long) (val)); \
	memcpy(phys, space, 8)

int cpioTrailerWrite(void * _iosm)
{
    IOSM_t iosm = _iosm;
    struct cpioCrcPhysicalHeader * hdr =
	(struct cpioCrcPhysicalHeader *)iosm->rdbuf;
    int rc;

    memset(hdr, (int)'0', PHYS_HDR_SIZE);
    memcpy(hdr->magic, CPIO_NEWC_MAGIC, sizeof(hdr->magic));
    memcpy(hdr->nlink, "00000001", 8);
    memcpy(hdr->namesize, "0000000b", 8);
    memcpy(iosm->rdbuf + PHYS_HDR_SIZE, CPIO_TRAILER, sizeof(CPIO_TRAILER));

    /* XXX DWRITE uses rdnb for I/O length. */
    iosm->rdnb = PHYS_HDR_SIZE + sizeof(CPIO_TRAILER);
    rc = iosmNext(iosm, IOSM_DWRITE);

    /*
     * GNU cpio pads to 512 bytes here, but we don't. This may matter for
     * tape device(s) and/or concatenated cpio archives. <shrug>
     */
    if (!rc)
	rc = iosmNext(iosm, IOSM_PAD);

    return rc;
}

int cpioHeaderWrite(void * _iosm, struct stat * st)
{
    IOSM_t iosm = _iosm;
    struct cpioCrcPhysicalHeader * hdr = (struct cpioCrcPhysicalHeader *)iosm->rdbuf;
    char field[64];
    size_t len;
    dev_t dev;
    int rc = 0;

if (_cpio_debug)
fprintf(stderr, "    cpioHeaderWrite(%p, %p)\n", iosm, st);

    memcpy(hdr->magic, CPIO_NEWC_MAGIC, sizeof(hdr->magic));
    SET_NUM_FIELD(hdr->inode, st->st_ino, field);
    SET_NUM_FIELD(hdr->mode, st->st_mode, field);
    SET_NUM_FIELD(hdr->uid, st->st_uid, field);
    SET_NUM_FIELD(hdr->gid, st->st_gid, field);
    SET_NUM_FIELD(hdr->nlink, st->st_nlink, field);
    SET_NUM_FIELD(hdr->mtime, st->st_mtime, field);
    SET_NUM_FIELD(hdr->filesize, st->st_size, field);

    dev = major((unsigned)st->st_dev); SET_NUM_FIELD(hdr->devMajor, dev, field);
    dev = minor((unsigned)st->st_dev); SET_NUM_FIELD(hdr->devMinor, dev, field);
    dev = major((unsigned)st->st_rdev); SET_NUM_FIELD(hdr->rdevMajor, dev, field);
    dev = minor((unsigned)st->st_rdev); SET_NUM_FIELD(hdr->rdevMinor, dev, field);

    len = strlen(iosm->path) + 1; SET_NUM_FIELD(hdr->namesize, len, field);
    memcpy(hdr->checksum, "00000000", 8);
    memcpy(iosm->rdbuf + PHYS_HDR_SIZE, iosm->path, len);

    /* XXX DWRITE uses rdnb for I/O length. */
    iosm->rdnb = PHYS_HDR_SIZE + len;
    rc = iosmNext(iosm, IOSM_DWRITE);
    if (!rc && iosm->rdnb != iosm->wrnb)
	rc = IOSMERR_WRITE_FAILED;
    if (!rc)
	rc = iosmNext(iosm, IOSM_PAD);

    if (!rc && S_ISLNK(st->st_mode)) {
assert(iosm->lpath != NULL);
	iosm->rdnb = strlen(iosm->lpath);
	memcpy(iosm->rdbuf, iosm->lpath, iosm->rdnb);
	rc = iosmNext(iosm, IOSM_DWRITE);
	if (!rc && iosm->rdnb != iosm->wrnb)
	    rc = IOSMERR_WRITE_FAILED;
	if (!rc)
	    rc = iosmNext(iosm, IOSM_PAD);
    }

    return rc;
}

int cpioHeaderRead(void * _iosm, struct stat * st)
{
    IOSM_t iosm = _iosm;
    struct cpioCrcPhysicalHeader hdr;
    int nameSize;
    char * end;
    int major, minor;
    int rc = 0;

if (_cpio_debug)
fprintf(stderr, "    cpioHeaderRead(%p, %p)\n", iosm, st);

    iosm->wrlen = PHYS_HDR_SIZE;
    rc = iosmNext(iosm, IOSM_DREAD);
    if (!rc && iosm->rdnb != iosm->wrlen)
	rc = IOSMERR_READ_FAILED;
    if (rc) return rc;
    memcpy(&hdr, iosm->wrbuf, iosm->rdnb);

    if (strncmp(CPIO_CRC_MAGIC, hdr.magic, sizeof(CPIO_CRC_MAGIC)-1) &&
	strncmp(CPIO_NEWC_MAGIC, hdr.magic, sizeof(CPIO_NEWC_MAGIC)-1))
	return IOSMERR_BAD_MAGIC;

    GET_NUM_FIELD(hdr.inode, st->st_ino);
    GET_NUM_FIELD(hdr.mode, st->st_mode);
    GET_NUM_FIELD(hdr.uid, st->st_uid);
    GET_NUM_FIELD(hdr.gid, st->st_gid);
    GET_NUM_FIELD(hdr.nlink, st->st_nlink);
    GET_NUM_FIELD(hdr.mtime, st->st_mtime);
    GET_NUM_FIELD(hdr.filesize, st->st_size);

    GET_NUM_FIELD(hdr.devMajor, major);
    GET_NUM_FIELD(hdr.devMinor, minor);
    /*@-shiftimplementation@*/
    st->st_dev = Makedev(major, minor);
    /*@=shiftimplementation@*/

    GET_NUM_FIELD(hdr.rdevMajor, major);
    GET_NUM_FIELD(hdr.rdevMinor, minor);
    /*@-shiftimplementation@*/
    st->st_rdev = Makedev(major, minor);
    /*@=shiftimplementation@*/

    GET_NUM_FIELD(hdr.namesize, nameSize);
    if (nameSize >= iosm->wrsize)
	return IOSMERR_BAD_HEADER;

    {	char * t = xmalloc(nameSize + 1);
	iosm->wrlen = nameSize;
	rc = iosmNext(iosm, IOSM_DREAD);
	if (!rc && iosm->rdnb != iosm->wrlen)
	    rc = IOSMERR_BAD_HEADER;
	if (rc) {
	    t = _free(t);
	    iosm->path = NULL;
	    return rc;
	}
	memcpy(t, iosm->wrbuf, iosm->rdnb);
	t[nameSize] = '\0';
	iosm->path = t;
    }

    if (S_ISLNK(st->st_mode)) {
	rc = iosmNext(iosm, IOSM_POS);
	if (rc) return rc;
	iosm->wrlen = st->st_size;
	rc = iosmNext(iosm, IOSM_DREAD);
	if (!rc && iosm->rdnb != iosm->wrlen)
	    rc = IOSMERR_READ_FAILED;
	if (rc) return rc;
	iosm->wrbuf[st->st_size] = '\0';
	iosm->lpath = xstrdup(iosm->wrbuf);
    }

if (_cpio_debug)
fprintf(stderr, "\t     %06o%3d (%4d,%4d)%10d %s\n\t-> %s\n",
                (unsigned)st->st_mode, (int)st->st_nlink,
                (int)st->st_uid, (int)st->st_gid, (int)st->st_size,
                (iosm->path ? iosm->path : ""), (iosm->lpath ? iosm->lpath : ""));

    return rc;
}

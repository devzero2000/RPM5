/** \ingroup payload
 * \file lib/tar.c
 *  Handle tar payloads within rpm packages.
 */

#include "system.h"

#include <rpmio_internal.h>
#include <rpmlib.h>

#include "tar.h"
#include "fsm.h"
#include "ugid.h"

#include "rpmerr.h"
#include "debug.h"

/*@access FSM_t @*/

/*@unchecked@*/
int _tar_debug = 0;

/*@unchecked@*/
static int nochksum = 0;

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
	/*@requires maxSet(endptr) >= 0 @*/
{
    char * buf, * end;
    unsigned long ret;

    buf = alloca(num + 1);
    strncpy(buf, str, num);
    buf[num] = '\0';

    ret = strtoul(buf, &end, base);
/*@-boundsread@*/ /* LCL: strtoul annotations */
    if (endptr != NULL) {
	if (*end != '\0')
	    *endptr = ((char *)str) + (end - buf);	/* XXX discards const */
	else
	    *endptr = ((char *)str) + strlen(buf);
    }
/*@=boundsread@*/

    return ret;
}

/**
 * Read long file/link name from tar archive.
 * @param fsm		file state machine
 * @param len		no. bytes of name
 * @retval *fnp		long file/link name
 * @return		0 on success
 */
static int tarHeaderReadName(FSM_t fsm, int len, /*@out@*/ const char ** fnp)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies fsm, *fnp, fileSystem, internalState @*/
{
    char * t;
    int nb;
    int rc = 0;

    *fnp = t = xmalloc(len + 1);
    while (len > 0) {
	/* Read next tar block. */
	fsm->wrlen = TAR_BLOCK_SIZE;
	rc = fsmNext(fsm, FSM_DREAD);
	if (!rc && fsm->rdnb != fsm->wrlen)
		rc = CPIOERR_READ_FAILED;
	if (rc) break;

	/* Append to name. */
	nb = (len > fsm->rdnb ? fsm->rdnb : len);
	memcpy(t, fsm->wrbuf, nb);
	t += nb;
	len -= nb;
    }
    *t = '\0';

    if (rc)
	*fnp = _free(*fnp);
    return rc;
}

int tarHeaderRead(FSM_t fsm, struct stat * st)
	/*@modifies fsm, *st @*/
{
    tarHeader hdr = (tarHeader) fsm->wrbuf;
    char * t;
    int nb;
    int major, minor;
    int rc = 0;
    int zblk = 0;

if (_tar_debug)
fprintf(stderr, "    %s(%p, %p)\n", __FUNCTION__, fsm, st);

top:
    do {
	/* Read next tar block. */
	fsm->wrlen = TAR_BLOCK_SIZE;
	rc = fsmNext(fsm, FSM_DREAD);
	if (!rc && fsm->rdnb != fsm->wrlen)
	    rc = CPIOERR_READ_FAILED;
	if (rc) return rc;

	/* Look for end-of-archive, i.e. 2 (or more) zero blocks. */
	if (hdr->name[0] == '\0' && hdr->checksum[0] == '\0') {
	    if (++zblk == 2)
		return CPIOERR_HDR_TRAILER;
	}
    } while (zblk > 0);

    /* Verify header checksum. */
    {	const unsigned char * hp = (const unsigned char *) hdr;
	char checksum[8];
	char hdrchecksum[8];
	long sum = 0;
	int i;

	memcpy(hdrchecksum, hdr->checksum, sizeof(hdrchecksum));
	memset(hdr->checksum, ' ', sizeof(hdr->checksum));

	for (i = 0; i < TAR_BLOCK_SIZE; i++)
	    sum += *hp++;

#if 0
	for (i = 0; i < sizeof(hdr->checksum) - 1; i++)
	    sum += (' ' - hdr->checksum[i]);
fprintf(stderr, "\tsum %ld\n", sum);
	if (sum != 0)
	    return CPIOERR_BAD_HEADER;
#else
	memset(checksum, ' ', sizeof(checksum));
	sprintf(checksum, "%06o", (unsigned) (sum & 07777777));
if (_tar_debug)
fprintf(stderr, "\tmemcmp(\"%s\", \"%s\", %d)\n", hdrchecksum, checksum, sizeof(hdrchecksum));
	if (memcmp(hdrchecksum, checksum, sizeof(hdrchecksum)))
	    if (!nochksum)
		return CPIOERR_BAD_HEADER;
#endif

    }

    /* Verify header magic. */
    if (strncmp(hdr->magic, TAR_MAGIC, sizeof(TAR_MAGIC)-1))
	return CPIOERR_BAD_MAGIC;

    st->st_size = strntoul(hdr->filesize, NULL, 8, sizeof(hdr->filesize));

    st->st_nlink = 1;
    st->st_mode = strntoul(hdr->mode, NULL, 8, sizeof(hdr->mode));
    st->st_mode &= ~S_IFMT;
    switch (hdr->typeflag) {
    case 'x':		/* Extended header referring to next file in archive. */
    case 'g':		/* Global extended header. */
    default:
	break;
    case '7':		/* reserved (contiguous files?) */
    case '\0':		/* (ancient) regular file */
    case '0':		/* regular file */
	st->st_mode |= S_IFREG;
	break;
    case '1':		/* hard link */
	st->st_mode |= S_IFREG;
#ifdef DYING
	st->st_nlink++;
#endif
	break;
    case '2':		/* symbolic link */
	st->st_mode |= S_IFLNK;
	break;
    case '3':		/* character special */
	st->st_mode |= S_IFCHR;
	break;
    case '4':		/* block special */
	st->st_mode |= S_IFBLK;
	break;
    case '5':		/* directory */
	st->st_mode |= S_IFDIR;
	st->st_nlink++;
	break;
    case '6':		/* FIFO special */
	st->st_mode |= S_IFIFO;
	break;
#ifdef	REFERENCE
    case 'A':		/* Solaris ACL */
    case 'E':		/* Solaris XATTR */
    case 'I':		/* Inode only, as in 'star' */
    case 'X':		/* POSIX 1003.1-2001 eXtended (VU version) */
    case 'D':		/* GNU dumpdir (with -G, --incremental) */
    case 'M':		/* GNU multivol (with -M, --multi-volume) */
    case 'N':		/* GNU names */
    case 'S':		/* GNU sparse  (with -S, --sparse) */
    case 'V':		/* GNU tape/volume header (with -Vlll, --label=lll) */
#endif
    case 'K':		/* GNU long (>100 chars) link name */
	rc = tarHeaderReadName(fsm, st->st_size, &fsm->lpath);
	if (rc) return rc;
	goto top;
	/*@notreached@*/ break;
    case 'L':		/* GNU long (>100 chars) file name */
	rc = tarHeaderReadName(fsm, st->st_size, &fsm->path);
	if (rc) return rc;
	goto top;
	/*@notreached@*/ break;
    }

    st->st_uid = strntoul(hdr->uid, NULL, 8, sizeof(hdr->uid));
    st->st_gid = strntoul(hdr->gid, NULL, 8, sizeof(hdr->gid));
    st->st_mtime = strntoul(hdr->mtime, NULL, 8, sizeof(hdr->mtime));
    st->st_ctime = st->st_atime = st->st_mtime;		/* XXX compat? */

    major = strntoul(hdr->devMajor, NULL, 8, sizeof(hdr->devMajor));
    minor = strntoul(hdr->devMinor, NULL, 8, sizeof(hdr->devMinor));
    /*@-shiftimplementation@*/
    st->st_dev = makedev(major, minor);
    /*@=shiftimplementation@*/
    st->st_rdev = st->st_dev;		/* XXX compat? */

    /* char prefix[155]; */
    /* char padding[12]; */

    /* Read short file name. */
    if (fsm->path == NULL && hdr->name[0] != '\0') {
	nb = strlen(hdr->name);
	t = xmalloc(nb + 1);
/*@-boundswrite@*/
	memcpy(t, hdr->name, nb);
	t[nb] = '\0';
/*@=boundswrite@*/
	fsm->path = t;
    }

    /* Read short link name. */
    if (fsm->lpath == NULL && hdr->linkname[0] != '\0') {
	nb = strlen(hdr->linkname);
	t = xmalloc(nb + 1);
/*@-boundswrite@*/
	memcpy(t, hdr->linkname, nb);
	t[nb] = '\0';
/*@=boundswrite@*/
	fsm->lpath = t;
    }

if (_tar_debug)
fprintf(stderr, "\t     %06o%3d (%4d,%4d)%10d %s\n\t-> %s\n",
                (unsigned)st->st_mode, (int)st->st_nlink,
                (int)st->st_uid, (int)st->st_gid, (int)st->st_size,
                (fsm->path ? fsm->path : ""), (fsm->lpath ? fsm->lpath : ""));

    return rc;
}

/**
 * Write long file/link name into tar archive.
 * @param fsm		file state machine
 * @param path		long file/link name
 * @return		0 on success
 */
static int tarHeaderWriteName(FSM_t fsm, const char * path)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies fsm, fileSystem, internalState @*/
{
    const char * s = path;
    int nb = strlen(s);
    int rc = 0;

if (_tar_debug)
fprintf(stderr, "\t%s(%p, %s) nb %d\n", __FUNCTION__, fsm, path, nb);

    while (nb > 0) {
	memset(fsm->rdbuf, 0, TAR_BLOCK_SIZE);

	/* XXX DWRITE uses rdnb for I/O length. */
	fsm->rdnb = (nb < TAR_BLOCK_SIZE) ? nb : TAR_BLOCK_SIZE;
	memmove(fsm->rdbuf, s, fsm->rdnb);
	rc = fsmNext(fsm, FSM_DWRITE);
	if (!rc && fsm->rdnb != fsm->wrnb)
		rc = CPIOERR_WRITE_FAILED;

	if (rc) break;
	s += fsm->rdnb;
	nb -= fsm->rdnb;
    }

    if (!rc)
	rc = fsmNext(fsm, FSM_PAD);

    return rc;
}

/**
 * Write tar header block with checksum  into tar archive.
 * @param fsm		file state machine
 * @param st		file info
 * @param hdr		tar header block
 * @return		0 on success
 */
static int tarHeaderWriteBlock(FSM_t fsm, struct stat * st, tarHeader hdr)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies fsm, hdr, fileSystem, internalState @*/
{
    int rc;

if (_tar_debug)
fprintf(stderr, "\t%s(%p, %p) type %c\n", __FUNCTION__, fsm, hdr, hdr->typeflag);
if (_tar_debug)
fprintf(stderr, "\t     %06o%3d (%4d,%4d)%10d %s\n",
                (unsigned)st->st_mode, (int)st->st_nlink,
                (int)st->st_uid, (int)st->st_gid, (int)st->st_size,
                (fsm->path ? fsm->path : ""));


    (void) stpcpy( stpcpy(hdr->magic, TAR_MAGIC), TAR_VERSION);

    /* Calculate header checksum. */
    {	const unsigned char * hp = (const unsigned char *) hdr;
	long sum = 0;
	int i;

	memset(hdr->checksum, ' ', sizeof(hdr->checksum));
	for (i = 0; i < TAR_BLOCK_SIZE; i++)
	    sum += *hp++;
	sprintf(hdr->checksum, "%06o", (unsigned)(sum & 07777777));
if (_tar_debug)
fprintf(stderr, "\thdrchksum \"%s\"\n", hdr->checksum);
    }

    /* XXX DWRITE uses rdnb for I/O length. */
    fsm->rdnb = TAR_BLOCK_SIZE;
    rc = fsmNext(fsm, FSM_DWRITE);
    if (!rc && fsm->rdnb != fsm->wrnb)
	rc = CPIOERR_WRITE_FAILED;

    return rc;
}

int tarHeaderWrite(FSM_t fsm, struct stat * st)
{
/*@observer@*/
    static const char * llname = "././@LongLink";
    tarHeader hdr = (tarHeader) fsm->rdbuf;
    char * t;
    dev_t dev;
    int rc = 0;
    int len;

if (_tar_debug)
fprintf(stderr, "    %s(%p, %p)\n", __FUNCTION__, fsm, st);

    len = strlen(fsm->path);
    if (len > sizeof(hdr->name)) {
	memset(hdr, 0, sizeof(*hdr));
	strcpy(hdr->name, llname);
	sprintf(hdr->mode, "%07o", 0);
	sprintf(hdr->uid, "%07o", 0);
	sprintf(hdr->gid, "%07o", 0);
	sprintf(hdr->filesize, "%011o", (unsigned) (len & 037777777777));
	sprintf(hdr->mtime, "%011o", 0);
	hdr->typeflag = 'L';
	strncpy(hdr->uname, "root", sizeof(hdr->uname));
	strncpy(hdr->gname, "root", sizeof(hdr->gname));
	rc = tarHeaderWriteBlock(fsm, st, hdr);
	if (rc) return rc;
	rc = tarHeaderWriteName(fsm, fsm->path);
	if (rc) return rc;
    }

    if (fsm->lpath && fsm->lpath[0] != '0') {
	len = strlen(fsm->lpath);
	if (len > sizeof(hdr->name)) {
	    memset(hdr, 0, sizeof(*hdr));
	    strcpy(hdr->linkname, llname);
	sprintf(hdr->mode, "%07o", 0);
	sprintf(hdr->uid, "%07o", 0);
	sprintf(hdr->gid, "%07o", 0);
	    sprintf(hdr->filesize, "%011o", (unsigned) (len & 037777777777));
	sprintf(hdr->mtime, "%011o", 0);
	    hdr->typeflag = 'K';
	strncpy(hdr->uname, "root", sizeof(hdr->uname));
	strncpy(hdr->gname, "root", sizeof(hdr->gname));
	    rc = tarHeaderWriteBlock(fsm, st, hdr);
	    if (rc) return rc;
	    rc = tarHeaderWriteName(fsm, fsm->lpath);
	    if (rc) return rc;
	}
    }

    memset(hdr, 0, sizeof(*hdr));

    strncpy(hdr->name, fsm->path, sizeof(hdr->name));

    if (fsm->lpath && fsm->lpath[0] != '0')
	strncpy(hdr->linkname, fsm->lpath, sizeof(hdr->linkname));

    sprintf(hdr->mode, "%07o", (st->st_mode & 00007777));
    sprintf(hdr->uid, "%07o", (st->st_uid & 07777777));
    sprintf(hdr->gid, "%07o", (st->st_gid & 07777777));

    sprintf(hdr->filesize, "%011o", (unsigned) (st->st_size & 037777777777));
    sprintf(hdr->mtime, "%011o", (unsigned) (st->st_mtime & 037777777777));

    hdr->typeflag = '0';	/* XXX wrong! */
    if (S_ISLNK(st->st_mode))
	hdr->typeflag = '2';
    else if (S_ISCHR(st->st_mode))
	hdr->typeflag = '3';
    else if (S_ISBLK(st->st_mode))
	hdr->typeflag = '4';
    else if (S_ISDIR(st->st_mode))
	hdr->typeflag = '5';
    else if (S_ISFIFO(st->st_mode))
	hdr->typeflag = '6';
#ifdef WHAT2DO
    else if (S_ISSOCK(st->st_mode))
	hdr->typeflag = '?';
#endif
    else if (S_ISREG(st->st_mode))
	hdr->typeflag = (fsm->lpath != NULL ? '1' : '0');

    /* XXX FIXME: map uname/gname from uid/gid. */
    t = uidToUname(st->st_uid);
    if (t == NULL) t = "root";
    strncpy(hdr->uname, t, sizeof(hdr->uname));
    t = gidToGname(st->st_gid);
    if (t == NULL) t = "root";
    strncpy(hdr->gname, t, sizeof(hdr->gname));

    /* XXX W2DO? st_dev or st_rdev? */
    dev = major((unsigned)st->st_dev);
    sprintf(hdr->devMajor, "%07o", (unsigned) (dev & 07777777));
    dev = minor((unsigned)st->st_dev);
    sprintf(hdr->devMinor, "%07o", (unsigned) (dev & 07777777));

    rc = tarHeaderWriteBlock(fsm, st, hdr);

    /* XXX Padding is unnecessary but shouldn't hurt. */
    if (!rc)
	rc = fsmNext(fsm, FSM_PAD);

    return rc;
}

int tarTrailerWrite(FSM_t fsm)
{
    int rc = 0;

if (_tar_debug)
fprintf(stderr, "    %s(%p)\n", __FUNCTION__, fsm);

    /* Pad up to 20 blocks (10Kb) of zeroes. */
    fsm->blksize *= 20;
    if (!rc)
	rc = fsmNext(fsm, FSM_PAD);
    fsm->blksize /= 20;

    return rc;
}

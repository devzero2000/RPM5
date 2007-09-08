
/**
 * \file rpmdb/pkgio.c
 */

#include "system.h"

#if defined(HAVE_MACHINE_TYPES_H)
# include <machine/types.h>
#endif

#include <netinet/in.h>

#include <rpmio.h>
#include <rpmlib.h>

#include "header_internal.h"
#include "signature.h"		/* XXX RPMSIGTYPE_HEADERSIG */
#include <pkgio.h>
#include "debug.h"


/*@access entryInfo @*/		/* XXX rpmReadSignature */
/*@access indexEntry @*/	/* XXX rpmReadSignature */

/**
 * Signature types stored in rpm lead.
 */
typedef	enum sigType_e {
    RPMSIGTYPE_HEADERSIG= 5	/*!< Header style signature */
} sigType;

#define	RPMLEAD_BINARY 0
#define	RPMLEAD_SOURCE 1

#define	RPMLEAD_MAGIC0 0xed
#define	RPMLEAD_MAGIC1 0xab
#define	RPMLEAD_MAGIC2 0xee
#define	RPMLEAD_MAGIC3 0xdb

#define	RPMLEAD_SIZE 96		/*!< Don't rely on sizeof(struct) */

/**
 * The lead data structure.
 * The lead needs to be 8 byte aligned.
 * @deprecated The lead (except for signature_type) is legacy.
 * @todo Don't use any information from lead.
 */
struct rpmlead {
    unsigned char magic[4];
    unsigned char major;
    unsigned char minor;
    short type;
    short archnum;
    char name[66];
    short osnum;
    short signature_type;	/*!< Signature header type (RPMSIG_HEADERSIG) */
/*@unused@*/ char reserved[16];	/*!< Pad to 96 bytes -- 8 byte aligned! */
} ;

/*@unchecked@*/
int _nolead = SUPPORT_RPMLEAD;

/*@unchecked@*/ /*@observer@*/
static unsigned char lead_magic[] = {
    RPMLEAD_MAGIC0, RPMLEAD_MAGIC1, RPMLEAD_MAGIC2, RPMLEAD_MAGIC3
};

/* The lead needs to be 8 byte aligned */

/**
 * Write lead to file handle.
 * @param fd		file handle
 * @param ptr		package lead
 * @param *msg		name to include in lead
 * @return		RPMRC_OK on success, RPMRC_FAIL on error
 */
static rpmRC writeLead(FD_t fd, void * ptr, const char ** msg)
	/*@globals fileSystem @*/
	/*@modifies fd, fileSystem @*/
{
    struct rpmlead l;

/*@-boundswrite@*/
    memcpy(&l, ptr, sizeof(l));

    /* Set some sane defaults */
    if (l.major == 0)
	l.major = 3;
    if (l.type == 0)
	l.type = RPMLEAD_BINARY;
    if (l.signature_type == 0)
	l.signature_type = RPMSIGTYPE_HEADERSIG;
    if (msg && *msg)
	(void) strncpy(l.name, *msg, sizeof(l.name));
    
    memcpy(&l.magic, lead_magic, sizeof(l.magic));
/*@=boundswrite@*/
    l.type = htons(l.type);
    l.archnum = htons(l.archnum);
    l.osnum = htons(l.osnum);
    l.signature_type = htons(l.signature_type);
	
/*@-boundswrite@*/
    if (Fwrite(&l, 1, sizeof(l), fd) != sizeof(l))
	return RPMRC_FAIL;
/*@=boundswrite@*/

    return RPMRC_OK;
}

/**
 * Read lead from file handle.
 * @param fd		file handle
 * @retval *ptr		package lead
 * @retval *msg		failure msg
 * @return		rpmRC return code
 */
static rpmRC readLead(FD_t fd, void * ptr, const char ** msg)
	/*@modifies fd, *lead, *msg @*/
{
    struct rpmlead ** leadp = ptr;
    struct rpmlead * lead = xcalloc(1, sizeof(*lead));
    char buf[BUFSIZ];
    rpmRC rc = RPMRC_FAIL;		/* assume failure */
    int xx;

    buf[0] = '\0';
    if (leadp != NULL) *leadp = NULL;

    /*@-type@*/ /* FIX: remove timed read */
    if ((xx = timedRead(fd, (char *)lead, sizeof(*lead))) != sizeof(*lead)) {
	if (Ferror(fd)) {
	    (void) snprintf(buf, sizeof(buf),
		_("lead size(%u): BAD, read(%d), %s(%d)"),
		(unsigned)sizeof(*lead), xx, Fstrerror(fd), errno);
	    rc = RPMRC_FAIL;
	} else {
	    (void) snprintf(buf, sizeof(buf),
		_("lead size(%u): BAD, read(%d), %s(%d)"),
		(unsigned)sizeof(*lead), xx, strerror(errno), errno);
	    rc = RPMRC_NOTFOUND;
	}
	goto exit;
    }
    /*@=type@*/

    if (memcmp(lead->magic, lead_magic, sizeof(lead_magic))) {
	(void) snprintf(buf, sizeof(buf), _("lead magic: BAD"));
	rc = RPMRC_NOTFOUND;
	goto exit;
    }
    lead->type = ntohs(lead->type);
    lead->archnum = ntohs(lead->archnum);
    lead->osnum = ntohs(lead->osnum);
    lead->signature_type = ntohs(lead->signature_type);

    switch (lead->major) {
    default:
	(void) snprintf(buf, sizeof(buf),
		_("lead version(%d): UNSUPPORTED"), lead->major);
	rc = RPMRC_NOTFOUND;
	break;
    case 3:
    case 4:
	break;
    }

    if (lead->signature_type != RPMSIGTYPE_HEADERSIG) {
	(void) snprintf(buf, sizeof(buf),
		_("sigh type(%d): UNSUPPORTED"), lead->signature_type);
	rc = RPMRC_NOTFOUND;
	goto exit;
    }

    rc = RPMRC_OK;

exit:
    if (rc == RPMRC_OK && leadp != NULL)
	*leadp = lead;
    else
	lead = _free(lead);
	
    if (msg != NULL) {
	buf[sizeof(buf)-1] = '\0';
	*msg = xstrdup(buf);
    }
    return rc;
}


/*@unchecked@*/
extern int _newmagic;

/*@observer@*/ /*@unchecked@*/
static unsigned char sigh_magic[8] = {
	0x8e, 0xad, 0xe8, 0x3e, 0x00, 0x00, 0x00, 0x00
};

/**
 * Write signature header.
 * @param _fd		file handle
 * @param ptr		signature header
 * @retval *msg		failure msg
 * @return		rpmRC return code
 */
static rpmRC rpmWriteSignature(void * _fd, void * ptr, const char ** msg)
	/*@globals fileSystem @*/
	/*@modifies _fd, sigh, fileSystem @*/
{
    FD_t fd = _fd;
    Header sigh = ptr;
    static unsigned char buf[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    int sigSize, pad;
    rpmRC rc = RPMRC_OK;
    int xx;

    xx = headerWrite(fd, sigh);
    if (xx)
	return RPMRC_FAIL;

    sigSize = headerSizeof(sigh);
    pad = (8 - (sigSize % 8)) % 8;
    if (pad) {
	if (Fwrite(buf, sizeof(buf[0]), pad, fd) != pad)
	    rc = RPMRC_FAIL;
    }
    rpmMessage(RPMMESS_DEBUG, D_("Signature: size(%d)+pad(%d)\n"), sigSize, pad);
    return rc;
}

/**
 * Print package size.
 * @todo rpmio: use fdSize rather than fstat(2) to get file size.
 * @param fd			package file handle
 * @param siglen		signature header size
 * @param pad			signature padding
 * @param datalen		length of header+payload
 * @return 			rpmRC return code
 */
static inline rpmRC printSize(FD_t fd, int siglen, int pad, size_t datalen)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    int fdno = Fileno(fd);
    struct stat st;
    size_t expected;
    size_t nl = rpmpkgSizeof("Lead");

    /* HACK: workaround for davRead wiring. */
    if (fdno == 123456789) {
	st.st_size = 0;
/*@-sizeoftype@*/
	st.st_size -= nl+siglen+pad+datalen;
/*@=sizeoftype@*/
    } else if (fstat(fdno, &st) < 0)
	return RPMRC_FAIL;

/*@-sizeoftype@*/
    expected = nl + siglen + pad;
    expected += datalen,
    rpmMessage(RPMMESS_DEBUG,
	D_("Expected size: %12lu = lead(%u)+sigs(%d)+pad(%d)+data(%lu)\n"),
		(unsigned long)expected,
		(unsigned)nl, siglen, pad, (unsigned long)datalen);
/*@=sizeoftype@*/
    rpmMessage(RPMMESS_DEBUG,
	D_("  Actual size: %12lu\n"), (unsigned long)st.st_size);

    return RPMRC_OK;
}

/**
 * Read (and verify header+payload size) signature header.
 * @param _fd		file handle
 * @retval *ptr		signature header (or NULL)
 * @retval *msg		failure msg
 * @return		rpmRC return code
 */
static rpmRC rpmReadSignature(void * _fd, void * ptr, const char ** msg)
	/*@globals fileSystem @*/
	/*@modifies _fd, *ptr, *msg, fileSystem @*/
{
    FD_t fd = _fd;
    Header * sighp = ptr;
    char buf[BUFSIZ];
    int_32 block[4];
    int_32 il;
    int_32 dl;
    int_32 * ei = NULL;
    entryInfo pe;
    size_t nb;
    int_32 ril = 0;
    indexEntry entry = memset(alloca(sizeof(*entry)), 0, sizeof(*entry));
    entryInfo info = memset(alloca(sizeof(*info)), 0, sizeof(*info));
    unsigned char * dataStart;
    unsigned char * dataEnd = NULL;
    Header sigh = NULL;
    rpmRC rc = RPMRC_FAIL;		/* assume failure */
    int xx;
    int i;

    if (sighp)
	*sighp = NULL;

    buf[0] = '\0';

    memset(block, 0, sizeof(block));
    if ((xx = timedRead(fd, (void *)block, sizeof(block))) != sizeof(block)) {
	(void) snprintf(buf, sizeof(buf),
		_("sigh size(%d): BAD, read returned %d\n"), (int)sizeof(block), xx);
	goto exit;
    }
    {   unsigned char * hmagic = NULL;
	size_t nmagic = 0;

	(void) headerGetMagic(NULL, &hmagic, &nmagic);
	if (_newmagic)	/* XXX FIXME: sigh needs its own magic. */
	    hmagic = sigh_magic;

	if (memcmp(block, hmagic, nmagic)) {
	    (void) snprintf(buf, sizeof(buf),
		_("sigh magic: BAD\n"));
	    goto exit;
	}
    }
    il = ntohl(block[2]);
    if (il < 0 || il > 32) {
	(void) snprintf(buf, sizeof(buf),
		_("sigh tags: BAD, no. of tags(%d) out of range\n"), il);
	goto exit;
    }
    dl = ntohl(block[3]);
    if (dl < 0 || dl > 8192) {
	(void) snprintf(buf, sizeof(buf),
		_("sigh data: BAD, no. of  bytes(%d) out of range\n"), dl);
	goto exit;
    }

/*@-sizeoftype@*/
    nb = (il * sizeof(struct entryInfo_s)) + dl;
/*@=sizeoftype@*/
    ei = xmalloc(sizeof(il) + sizeof(dl) + nb);
    ei[0] = block[2];
    ei[1] = block[3];
    pe = (entryInfo) &ei[2];
    dataStart = (unsigned char *) (pe + il);
    if ((xx = timedRead(fd, (void *)pe, nb)) != nb) {
	(void) snprintf(buf, sizeof(buf),
		_("sigh blob(%d): BAD, read returned %d\n"), (int)nb, xx);
	goto exit;
    }
    
    /* Check (and convert) the 1st tag element. */
    xx = headerVerifyInfo(1, dl, pe, &entry->info, 0);
    if (xx != -1) {
	(void) snprintf(buf, sizeof(buf),
		_("tag[%d]: BAD, tag %d type %d offset %d count %d\n"),
		0, entry->info.tag, entry->info.type,
		entry->info.offset, entry->info.count);
	goto exit;
    }

    /* Is there an immutable header region tag? */
/*@-sizeoftype@*/
    if (entry->info.tag == RPMTAG_HEADERSIGNATURES
       && entry->info.type == RPM_BIN_TYPE
       && entry->info.count == REGION_TAG_COUNT)
    {
/*@=sizeoftype@*/

	if (entry->info.offset >= dl) {
	    (void) snprintf(buf, sizeof(buf),
		_("region offset: BAD, tag %d type %d offset %d count %d\n"),
		entry->info.tag, entry->info.type,
		entry->info.offset, entry->info.count);
	    goto exit;
	}

	/* Is there an immutable header region tag trailer? */
	dataEnd = dataStart + entry->info.offset;
/*@-sizeoftype@*/
	(void) memcpy(info, dataEnd, REGION_TAG_COUNT);
	/* XXX Really old packages have HEADER_IMAGE, not HEADER_SIGNATURES. */
	if (info->tag == htonl(RPMTAG_HEADERIMAGE)) {
	    int_32 stag = htonl(RPMTAG_HEADERSIGNATURES);
	    info->tag = stag;
	    memcpy(dataEnd, &stag, sizeof(stag));
	}
	dataEnd += REGION_TAG_COUNT;

	xx = headerVerifyInfo(1, dl, info, &entry->info, 1);
	if (xx != -1 ||
	    !(entry->info.tag == RPMTAG_HEADERSIGNATURES
	   && entry->info.type == RPM_BIN_TYPE
	   && entry->info.count == REGION_TAG_COUNT))
	{
	    (void) snprintf(buf, sizeof(buf),
		_("region trailer: BAD, tag %d type %d offset %d count %d\n"),
		entry->info.tag, entry->info.type,
		entry->info.offset, entry->info.count);
	    goto exit;
	}
/*@=sizeoftype@*/
	memset(info, 0, sizeof(*info));

	/* Is the no. of tags in the region less than the total no. of tags? */
	ril = entry->info.offset/sizeof(*pe);
	if ((entry->info.offset % sizeof(*pe)) || ril > il) {
	    (void) snprintf(buf, sizeof(buf),
		_("region size: BAD, ril(%d) > il(%d)\n"), ril, il);
	    goto exit;
	}
    }

    /* Sanity check signature tags */
    memset(info, 0, sizeof(*info));
    for (i = 1; i < il; i++) {
	xx = headerVerifyInfo(1, dl, pe+i, &entry->info, 0);
	if (xx != -1) {
	    (void) snprintf(buf, sizeof(buf),
		_("sigh tag[%d]: BAD, tag %d type %d offset %d count %d\n"),
		i, entry->info.tag, entry->info.type,
		entry->info.offset, entry->info.count);
	    goto exit;
	}
    }

    /* OK, blob looks sane, load the header. */
    sigh = headerLoad(ei);
    if (sigh == NULL) {
	(void) snprintf(buf, sizeof(buf), _("sigh load: BAD\n"));
	goto exit;
    }
    sigh->flags |= HEADERFLAG_ALLOCATED;
    if (_newmagic)	/* XXX FIXME: sigh needs its own magic. */
	(void) headerSetMagic(sigh, sigh_magic, sizeof(sigh_magic));

    {	int sigSize = headerSizeof(sigh);
	int pad = (8 - (sigSize % 8)) % 8; /* 8-byte pad */
	int_32 * archSize = NULL;

	/* Position at beginning of header. */
	if (pad && (xx = timedRead(fd, (void *)block, pad)) != pad) {
	    (void) snprintf(buf, sizeof(buf),
		_("sigh pad(%d): BAD, read %d bytes\n"), pad, xx);
	    goto exit;
	}

	/* Print package component sizes. */
	if (headerGetEntry(sigh, RPMSIGTAG_SIZE, NULL, &archSize, NULL)) {
	    size_t datasize = *(uint_32 *)archSize;
	    rc = printSize(fd, sigSize, pad, datasize);
	    if (rc != RPMRC_OK)
		(void) snprintf(buf, sizeof(buf),
			_("sigh sigSize(%d): BAD, fstat(2) failed\n"), sigSize);
	}
    }

exit:
    if (sighp && sigh && rc == RPMRC_OK)
	*sighp = headerLink(sigh);
    sigh = headerFree(sigh);

    if (msg != NULL) {
	buf[sizeof(buf)-1] = '\0';
	*msg = xstrdup(buf);
    }

    return rc;
}

size_t rpmpkgSizeof(const char * fn)
{
    size_t len = 0;
    if (!strcmp(fn, "Lead"));
	return sizeof(struct rpmlead);
    return len;
}

rpmRC rpmpkgRead(const char * fn, FD_t fd, void * ptr, const char ** msg)
{
    rpmRC rc = RPMRC_FAIL;

    if (!strcmp(fn, "Lead"))
	return readLead(fd, ptr, msg);
    if (!strcmp(fn, "Signature"))
	return rpmReadSignature(fd, ptr, msg);
    return rc;
}

rpmRC rpmpkgWrite(const char * fn, FD_t fd, void * ptr, const char ** msg)
{
    rpmRC rc = RPMRC_FAIL;

    if (!strcmp(fn, "Lead"))
	return writeLead(fd, ptr, msg);
    if (!strcmp(fn, "Signature"))
	return rpmWriteSignature(fd, ptr, msg);
    return rc;
}

#ifdef	NOTYET
rpmRC rpmpkgCheck(const char * fn, FD_t fd, void * ptr, const char ** msg)
{
    rpmRC rc = RPMRC_FAIL;

    if (!strcmp(fn, "Lead"))
	return checkLead(fd, ptr, msg);
    return rc;
}
#endif

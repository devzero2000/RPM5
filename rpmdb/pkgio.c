
/** \ingroup lead
 * \file rpmdb/pkgio.c
 */

#include "system.h"

#if defined(HAVE_MACHINE_TYPES_H)
# include <machine/types.h>
#endif

#include <netinet/in.h>

#include <rpmlib.h>
#include <rpmio.h>

#include "signature.h"		/* XXX RPMSIGTYPE_HEADERSIG */
#include <pkgio.h>
#include "debug.h"

#define	RPMLEAD_BINARY 0
#define	RPMLEAD_SOURCE 1

#define	RPMLEAD_MAGIC0 0xed
#define	RPMLEAD_MAGIC1 0xab
#define	RPMLEAD_MAGIC2 0xee
#define	RPMLEAD_MAGIC3 0xdb

#define	RPMLEAD_SIZE 96		/*!< Don't rely on sizeof(struct) */

/** \ingroup lead
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
    return rc;
}

rpmRC rpmpkgWrite(const char * fn, FD_t fd, void * ptr, const char ** msg)
{
    rpmRC rc = RPMRC_FAIL;

    if (!strcmp(fn, "Lead"))
	return writeLead(fd, ptr, msg);
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

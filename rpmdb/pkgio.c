
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
#define	_RPMLEAD_INTERNAL
#include <pkgio.h>
#include "debug.h"

/*@unchecked@*/
int _nolead = SUPPORT_RPMLEAD;

/*@unchecked@*/ /*@observer@*/
static unsigned char lead_magic[] = {
    RPMLEAD_MAGIC0, RPMLEAD_MAGIC1, RPMLEAD_MAGIC2, RPMLEAD_MAGIC3
};

/* The lead needs to be 8 byte aligned */

rpmRC writeLead(FD_t fd, const struct rpmlead *lead)
{
    struct rpmlead l;

/*@-boundswrite@*/
    memcpy(&l, lead, sizeof(l));
    
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

rpmRC readLead(FD_t fd, struct rpmlead ** leadp, const char ** msg)
{
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

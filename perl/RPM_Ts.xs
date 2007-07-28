#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#undef Fflush
#undef Mkdir
#undef Stat

#include <stdio.h>
#include <string.h>
#include <utime.h>

#include "rpmlib.h"
#include "rpmio.h"
#include "rpmcli.h"
#include "rpmts.h"
#include "rpmte.h"
#include "header.h"
#include "rpmdb.h"
#include "misc.h"


/* Chip, this is somewhat stripped down from the default callback used by
   the rpmcli.  It has to be here to insure that we open the pkg again. 
   If we don't do this we get segfaults.  I also, kept the updating of some
   of the rpmcli static vars, but I may not have needed to do this.

   Also, we probably want to give a nice interface such that we could allow
   users of RPM to do their own callback, but that will have to come later.
*/
void * _null_callback(
	const void * arg, 
	const rpmCallbackType what, 
	const unsigned long long amount, 
	const unsigned long long total, 
	fnpyKey key, 
	rpmCallbackData data)
{
	Header h = (Header) arg;
	char * s;
	int flags = (int) ((long)data);
	void * rc = NULL;
	const char * filename = (const char *)key;
	static FD_t fd = NULL;
	int xx;

	/* Code stolen from rpminstall.c and modified */
	switch(what) {
		case RPMCALLBACK_INST_OPEN_FILE:
	 		if (filename == NULL || filename[0] == '\0')
			     return NULL;
			fd = Fopen(filename, "r.ufdio");
			/* FIX: still necessary? */
			if (fd == NULL || Ferror(fd)) {
				fprintf(stderr, "open of %s failed!\n", filename);
				if (fd != NULL) {
					xx = Fclose(fd);
					fd = NULL;
				}
			} else
				fd = fdLink(fd, "persist (showProgress)");
			return (void *)fd;
	 		break;

	case RPMCALLBACK_INST_CLOSE_FILE:
		/* FIX: still necessary? */
		fd = fdFree(fd, "persist (showProgress)");
		if (fd != NULL) {
			xx = Fclose(fd);
			fd = NULL;
		}
		break;

	case RPMCALLBACK_INST_START:
		rpmcliHashesCurrent = 0;
		if (h == NULL || !(flags & INSTALL_LABEL))
			break;
		break;

	case RPMCALLBACK_TRANS_PROGRESS:
	case RPMCALLBACK_INST_PROGRESS:
		break;

	case RPMCALLBACK_TRANS_START:
		rpmcliHashesCurrent = 0;
		rpmcliProgressTotal = 1;
		rpmcliProgressCurrent = 0;
		break;

	case RPMCALLBACK_TRANS_STOP:
		rpmcliProgressTotal = rpmcliPackagesTotal;
		rpmcliProgressCurrent = 0;
		break;

	case RPMCALLBACK_REPACKAGE_START:
		rpmcliHashesCurrent = 0;
		rpmcliProgressTotal = total;
		rpmcliProgressCurrent = 0;
		break;

	case RPMCALLBACK_REPACKAGE_PROGRESS:
		break;

	case RPMCALLBACK_REPACKAGE_STOP:
		rpmcliProgressTotal = total;
		rpmcliProgressCurrent = total;
		rpmcliProgressTotal = rpmcliPackagesTotal;
		rpmcliProgressCurrent = 0;
		break;

	case RPMCALLBACK_UNINST_PROGRESS:
		break;
	case RPMCALLBACK_UNINST_START:
		break;
	case RPMCALLBACK_UNINST_STOP:
		break;
	case RPMCALLBACK_UNPACK_ERROR:
		break;
	case RPMCALLBACK_CPIO_ERROR:
		break;
	case RPMCALLBACK_UNKNOWN:
		break;
	default:
		break;
	}
	
	return rc;	
}

MODULE = RPM_Ts		PACKAGE = RPM::Transaction

void
DESTROY(t)
	rpmts t
    CODE:
	t = rpmtsFree(t);

# XXX:  Add relocations some day. 
int
_add_install(t, h, fn, upgrade)
	rpmts  t
	Header h
	char * fn
	int    upgrade
    PREINIT:
	rpmRC rc = 0;
    CODE:
	rc = rpmtsAddInstallElement(t, h, (fnpyKey) fn, upgrade, NULL);
	RETVAL = (rc == RPMRC_OK) ? 1 : 0;
    OUTPUT:
	RETVAL	

int
_add_delete(t, h, offset)
	rpmts        t
	Header       h
	unsigned int offset
    PREINIT:
	rpmRC rc = 0;
    CODE:
	rc = rpmtsAddEraseElement(t, h, offset);
	RETVAL = (rc == RPMRC_OK) ? 1 : 0;
    OUTPUT:
	RETVAL	

int
_element_count(t)
	rpmts t
PREINIT:
	int ret;
CODE:
	ret    = rpmtsNElements(t);
	RETVAL = ret;
OUTPUT:
	RETVAL

int
_close_db(t)
	rpmts t
PREINIT:
	int ret;
CODE:
	ret    = rpmtsCloseDB(t);
	RETVAL = (ret == 0) ? 1 : 0;
OUTPUT:
	RETVAL

int
_check(t)
	rpmts t
PREINIT:
	int ret;
CODE:
	ret    = rpmtsCheck(t);
	RETVAL = (ret == 0) ? 1 : 0;
OUTPUT:
	RETVAL

int
_order(t)
	rpmts t
PREINIT:
	int ret;
CODE:
	ret    = rpmtsOrder(t);
	/* XXX:  May want to do something different here.  It actually
	         returns the number of non-ordered elements...maybe we
	         want this?
	*/
	RETVAL = (ret == 0) ? 1 : 0;
OUTPUT:
	RETVAL

void
_elements(t, type)
	rpmts t;
	rpmElementType type;
PREINIT:
	rpmtsi       i;
	rpmte        te;
	const char * NEVR;
PPCODE:
	i = rpmtsiInit(t);
	if(i == NULL) {
		printf("Did not get a thing!\n");
		return;	
	} else {
		while((te = rpmtsiNext(i, type)) != NULL) {
			NEVR = rpmteNEVR(te);
			XPUSHs(sv_2mortal(newSVpv(NEVR,	0)));
		}
		i = rpmtsiFree(i);
	}

int
_run(t, ok_probs, prob_filter)
	rpmts t
	rpmprobFilterFlags prob_filter 
    PREINIT:
	int i;
	rpmProblem p;
	int ret;
    CODE:
	/* Make sure we could run this transactions */
	ret = rpmtsCheck(t);
	if (ret != 0) {
		RETVAL = 0;
		return;
	}
	ret = rpmtsOrder(t);
	if (ret != 0) {
		RETVAL = 0;
		return;
	}

	/* XXX:  Should support callbacks eventually */
	(void) rpmtsSetNotifyCallback(t, _null_callback, (void *) ((long)0));
	ret    = rpmtsRun(t, NULL, prob_filter);
	RETVAL = (ret == 0) ? 1 : 0;
    OUTPUT:
	RETVAL



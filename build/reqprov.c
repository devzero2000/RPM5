/** \ingroup rpmbuild
 * \file build/reqprov.c
 *  Add dependency tags to package header(s).
 */

#include "system.h"

#include <rpmio.h>
#include <rpmcb.h>
#define	_RPMEVR_INTERNAL
#include "rpmbuild.h"
#include "debug.h"

int addReqProv(/*@unused@*/ Spec spec, Header h,
		/*@unused@*/ rpmTag tagN,
		const char * N, const char * EVR, rpmsenseFlags Flags,
		uint32_t index)
{
    HE_t he = memset(alloca(sizeof(*he)), 0, sizeof(*he));
    const char ** names;
    rpmTag nametag = 0;
    rpmTag versiontag = 0;
    rpmTag flagtag = 0;
    rpmTag indextag = 0;
    int len;
    rpmsenseFlags extra = RPMSENSE_ANY;
    int xx;

    if (Flags & RPMSENSE_PROVIDES) {
	nametag = RPMTAG_PROVIDENAME;
	versiontag = RPMTAG_PROVIDEVERSION;
	flagtag = RPMTAG_PROVIDEFLAGS;
	extra = Flags & RPMSENSE_FIND_PROVIDES;
    } else if (Flags & RPMSENSE_OBSOLETES) {
	nametag = RPMTAG_OBSOLETENAME;
	versiontag = RPMTAG_OBSOLETEVERSION;
	flagtag = RPMTAG_OBSOLETEFLAGS;
    } else if (Flags & RPMSENSE_CONFLICTS) {
	nametag = RPMTAG_CONFLICTNAME;
	versiontag = RPMTAG_CONFLICTVERSION;
	flagtag = RPMTAG_CONFLICTFLAGS;
    } else if (Flags & RPMSENSE_TRIGGER) {
	nametag = RPMTAG_TRIGGERNAME;
	versiontag = RPMTAG_TRIGGERVERSION;
	flagtag = RPMTAG_TRIGGERFLAGS;
	indextag = RPMTAG_TRIGGERINDEX;
	extra = Flags & RPMSENSE_TRIGGER;
    } else {
	nametag = RPMTAG_REQUIRENAME;
	versiontag = RPMTAG_REQUIREVERSION;
	flagtag = RPMTAG_REQUIREFLAGS;
	extra = Flags & _ALL_REQUIRES_MASK;
    }

    Flags = (Flags & RPMSENSE_SENSEMASK) | extra;

    if (EVR == NULL)
	EVR = "";

    /* Check for duplicate dependencies. */
    he->tag = nametag;
    xx = headerGet(h, he, 0);
    names = he->p.argv;
    len = he->c;
    if (xx) {
	const char ** versions = NULL;
	uint32_t * flags = NULL;
	uint32_t * indexes = NULL;
	int duplicate = 0;

	if (flagtag) {
	    he->tag = versiontag;
	    xx = headerGet(h, he, 0);
	    versions = he->p.argv;
	    he->tag = flagtag;
	    xx = headerGet(h, he, 0);
	    flags = he->p.ui32p;
	}
	if (indextag) {
	    he->tag = indextag;
	    xx = headerGet(h, he, 0);
	    indexes = he->p.ui32p;
	}

	while (len > 0) {
	    len--;
	    if (strcmp(names[len], N))
		continue;
	    if (flagtag && versions != NULL &&
		(strcmp(versions[len], EVR) || (rpmsenseFlags)flags[len] != Flags))
		continue;
	    if (indextag && indexes != NULL && indexes[len] != index)
		continue;

	    /* This is a duplicate dependency. */
	    duplicate = 1;

	    break;
	}
	names = _free(names);
	versions = _free(versions);
	flags = _free(flags);
	indexes = _free(indexes);
	if (duplicate)
	    return 0;
    }

    /* Add this dependency. */
    he->tag = nametag;
    he->t = RPM_STRING_ARRAY_TYPE;
    he->p.argv = &N;
    he->c = 1;
    he->append = 1;
    xx = headerPut(h, he, 0);
    he->append = 0;

    if (flagtag) {
	he->tag = versiontag;
	he->t = RPM_STRING_ARRAY_TYPE;
	he->p.argv = &EVR;
	he->c = 1;
	he->append = 1;
	xx = headerPut(h, he, 0);
	he->append = 0;

	he->tag = flagtag;
	he->t = RPM_UINT32_TYPE;
	he->p.ui32p = (uint32_t *) &Flags;
	he->c = 1;
	he->append = 1;
	xx = headerPut(h, he, 0);
	he->append = 0;
    }
    if (indextag) {
	he->tag = indextag;
	he->t = RPM_UINT32_TYPE;
	he->p.ui32p = &index;
	he->c = 1;
	he->append = 1;
	xx = headerPut(h, he, 0);
	he->append = 0;
    }

    return 0;
}

int rpmlibNeedsFeature(Header h, const char * feature, const char * featureEVR)
{
    char * reqname = alloca(sizeof("rpmlib()") + strlen(feature));

    (void) stpcpy( stpcpy( stpcpy(reqname, "rpmlib("), feature), ")");

    /* XXX 1st arg is unused */
   return addReqProv(NULL, h, RPMTAG_REQUIRENAME, reqname, featureEVR,
	RPMSENSE_RPMLIB|(RPMSENSE_LESS|RPMSENSE_EQUAL), 0);
}

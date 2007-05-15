/** \ingroup rpmbuild
 * \file build/parseReqs.c
 *  Parse dependency tag from spec file or from auto-dependency generator.
 */

#include "system.h"

#define	_RPMEVR_INTERNAL
#include "rpmbuild.h"
#include "debug.h"

#define	SKIPWHITE(_x)	{while(*(_x) && (xisspace(*_x) || *(_x) == ',')) (_x)++;}
#define	SKIPNONWHITE(_x){while(*(_x) &&!(xisspace(*_x) || *(_x) == ',')) (_x)++;}

int parseRCPOT(Spec spec, Package pkg, const char *field, rpmTag tagN,
	       int index, rpmsenseFlags tagflags)
{
    const char *r, *re, *v, *ve;
    char * N, * EVR;
    rpmsenseFlags Flags;
    Header h;

    switch (tagN) {
    case RPMTAG_PROVIDEFLAGS:
	tagflags |= RPMSENSE_PROVIDES;
	h = pkg->header;
	break;
    case RPMTAG_OBSOLETEFLAGS:
	tagflags |= RPMSENSE_OBSOLETES;
	h = pkg->header;
	break;
    case RPMTAG_CONFLICTFLAGS:
	tagflags |= RPMSENSE_CONFLICTS;
	h = pkg->header;
	break;
    case RPMTAG_BUILDCONFLICTS:
	tagflags |= RPMSENSE_CONFLICTS;
	h = spec->sourceHeader;
	break;
    case RPMTAG_PREREQ:
	tagflags |= RPMSENSE_ANY;
	h = pkg->header;
	break;
    case RPMTAG_TRIGGERPREIN:
	tagflags |= RPMSENSE_TRIGGERPREIN;
	h = pkg->header;
	break;
    case RPMTAG_TRIGGERIN:
	tagflags |= RPMSENSE_TRIGGERIN;
	h = pkg->header;
	break;
    case RPMTAG_TRIGGERPOSTUN:
	tagflags |= RPMSENSE_TRIGGERPOSTUN;
	h = pkg->header;
	break;
    case RPMTAG_TRIGGERUN:
	tagflags |= RPMSENSE_TRIGGERUN;
	h = pkg->header;
	break;
    case RPMTAG_BUILDSUGGESTS:
    case RPMTAG_BUILDENHANCES:
	tagflags |= RPMSENSE_MISSINGOK;
	h = spec->sourceHeader;
	break;
    case RPMTAG_BUILDPREREQ:
    case RPMTAG_BUILDREQUIRES:
	tagflags |= RPMSENSE_ANY;
	h = spec->sourceHeader;
	break;
    case RPMTAG_BUILDPROVIDES:
	tagflags |= RPMSENSE_PROVIDES;
	h = spec->sourceHeader;
	break;
    case RPMTAG_BUILDOBSOLETES:
	tagflags |= RPMSENSE_OBSOLETES;
	h = spec->sourceHeader;
	break;
    default:
    case RPMTAG_REQUIREFLAGS:
	tagflags |= RPMSENSE_ANY;
	h = pkg->header;
	break;
    }

/*@-boundsread@*/
    for (r = field; *r != '\0'; r = re) {
	size_t nr;
	SKIPWHITE(r);
	if (*r == '\0')
	    break;

	Flags = (tagflags & ~RPMSENSE_SENSEMASK);

	/* Tokens must begin with alphanumeric, _, or / */
	nr = strlen(r);
	if (!(xisalnum(r[0]) || r[0] == '_' || r[0] == '/'
	 || (nr > 2 && r[0] == '!')
	 || (nr > 3 && r[0] == '%' && r[1] == '{' && r[nr-1] == '}')))
	{
	    rpmError(RPMERR_BADSPEC,
		     _("line %d: Dependency \"%s\" must begin with alpha-numeric, '_' or '/': %s\n"),
		     spec->lineNum, spec->line, r);
	    return RPMERR_BADSPEC;
	}

	re = r;
	SKIPNONWHITE(re);
	N = xmalloc((re-r) + 1);
	strncpy(N, r, (re-r));
	N[re-r] = '\0';

	/* Parse EVR */
	v = re;
	SKIPWHITE(v);
	ve = v;
	SKIPNONWHITE(ve);

	re = v;	/* ==> next token (if no EVR found) starts here */

	/* Check for possible logical operator */
	if (ve > v) {
	    Flags = rpmEVRflags(v, &ve);
	    if (Flags && r[0] == '/') {
		rpmError(RPMERR_BADSPEC,
			 _("line %d: Versioned file name not permitted: %s\n"),
			 spec->lineNum, spec->line);
		return RPMERR_BADSPEC;
	    }
	    if (Flags) {
		/* now parse EVR */
		v = ve;
		SKIPWHITE(v);
		ve = v;
		SKIPNONWHITE(ve);
	    }
	}

	/*@-branchstate@*/
 	if (Flags & RPMSENSE_SENSEMASK) {
	    if (*v == '\0' || ve == v) {
		rpmError(RPMERR_BADSPEC, _("line %d: Version required: %s\n"),
			spec->lineNum, spec->line);
		return RPMERR_BADSPEC;
	    }
	    EVR = xmalloc((ve-v) + 1);
	    strncpy(EVR, v, (ve-v));
	    EVR[ve-v] = '\0';
	    re = ve;	/* ==> next token after EVR string starts here */
	} else
	    EVR = NULL;
	/*@=branchstate@*/

	(void) addReqProv(spec, h, tagN, N, EVR, Flags, index);

	N = _free(N);
	EVR = _free(EVR);

    }
/*@=boundsread@*/

    return 0;
}

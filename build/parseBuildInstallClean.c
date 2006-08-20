/** \ingroup rpmbuild
 * \file build/parseBuildInstallClean.c
 *  Parse %build/%install/%clean section from spec file.
 */
#include "system.h"

#include "rpmbuild.h"
#include "debug.h"

/*@access StringBuf @*/

/*@-boundswrite@*/
int parseBuildInstallClean(Spec spec, rpmParseState parsePart)
{
    int nextPart, rc;
    StringBuf *sbp = NULL;
    const char *name = NULL;

    /*@-branchstate@*/
    if (parsePart == PART_BUILD) {
	sbp = &spec->build;
	name = "%build";
    } else if (parsePart == PART_INSTALL) {
	sbp = &spec->install;
	name = "%install";
    } else if (parsePart == PART_CHECK) {
	sbp = &spec->check;
	name = "%check";
    } else if (parsePart == PART_CLEAN) {
	sbp = &spec->clean;
	name = "%clean";
    }
    /*@=branchstate@*/
    
    if (*sbp != NULL) {
	rpmError(RPMERR_BADSPEC, _("line %d: second %s\n"),
		spec->lineNum, name);
	return RPMERR_BADSPEC;
    }
    
    *sbp = newStringBuf();

    /* Make sure the buildroot is removed where needed. */
    if (parsePart == PART_INSTALL) {
	const char * s = rpmExpand("%{?buildroot:rm -rf %{buildroot}\n}", NULL);
	if (s && *s)
	    appendStringBuf(*sbp, s);
	s = _free(s);
    } else if (parsePart == PART_CLEAN) {
	const char * s = rpmExpand("%{?buildroot:rm -rf %{buildroot}\n}", NULL);
	if (s && *s)
	    appendStringBuf(*sbp, s);
	s = _free(s);
	sbp = NULL;	/* XXX skip %clean from spec file. */
    }

    /* There are no options to %build, %install, %check, or %clean */
    if ((rc = readLine(spec, STRIP_NOTHING)) > 0)
	return PART_NONE;
    if (rc)
	return rc;
    
    while (! (nextPart = isPart(spec->line))) {
	if (sbp)
	    appendStringBuf(*sbp, spec->line);
	if ((rc = readLine(spec, STRIP_NOTHING)) > 0)
	    return PART_NONE;
	if (rc)
	    return rc;
    }

    return nextPart;
}
/*@=boundswrite@*/

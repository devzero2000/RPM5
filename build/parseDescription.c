/** \ingroup rpmbuild
 * \file build/parseDescription.c
 *  Parse %description section from spec file.
 */

#include "system.h"

#include <rpmio.h>
#include <rpmiotypes.h>
#include <rpmlog.h>
#include "rpmbuild.h"
#include "debug.h"

/*@-exportheadervar@*/
/*@unchecked@*/
extern int noLang;
/*@=exportheadervar@*/

/* These have to be global scope to make up for *stupid* compilers */
/*@unchecked@*/
    /*@observer@*/ /*@null@*/ static const char *name = NULL;
/*@unchecked@*/
    /*@observer@*/ /*@null@*/ static const char *lang = NULL;

/*@unchecked@*/
    static struct poptOption optionsTable[] = {
	{ NULL, 'n', POPT_ARG_STRING, &name, 0,	NULL, NULL},
	{ NULL, 'l', POPT_ARG_STRING, &lang, 0,	NULL, NULL},
	{ 0, 0, 0, 0, 0,	NULL, NULL}
    };

int parseDescription(Spec spec)
	/*@globals name, lang @*/
	/*@modifies name, lang @*/
{
    rpmParseState nextPart = (rpmParseState) RPMRC_FAIL; /* assume error */
    rpmiob iob;
    int flag = PART_SUBNAME;
    Package pkg;
    int rc, argc;
    int arg;
    const char **argv = NULL;
    poptContext optCon = NULL;
    spectag t = NULL;

    {	char * se = strchr(spec->line, '#');
	if (se) {
	    *se = '\0';
	    while (--se >= spec->line && strchr(" \t\n\r", *se) != NULL)
		*se = '\0';
	}
    }

    if ((rc = poptParseArgvString(spec->line, &argc, &argv))) {
	rpmlog(RPMLOG_ERR, _("line %d: Error parsing %%description: %s\n"),
		 spec->lineNum, poptStrerror(rc));
	return RPMRC_FAIL;
    }

    name = NULL;
    lang = RPMBUILD_DEFAULT_LANG;
    optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
    while ((arg = poptGetNextOpt(optCon)) > 0)
	{;}
    if (name != NULL)
	flag = PART_NAME;

    if (arg < -1) {
	rpmlog(RPMLOG_ERR, _("line %d: Bad option %s: %s\n"),
		 spec->lineNum,
		 poptBadOption(optCon, POPT_BADOPTION_NOALIAS), 
		 spec->line);
	goto exit;
    }

    if (poptPeekArg(optCon)) {
	if (name == NULL)
	    name = poptGetArg(optCon);
	if (poptPeekArg(optCon)) {
	    rpmlog(RPMLOG_ERR, _("line %d: Too many names: %s\n"),
		     spec->lineNum, spec->line);
	    goto exit;
	}
    }

    if (lookupPackage(spec, name, flag, &pkg) != RPMRC_OK) {
	rpmlog(RPMLOG_ERR, _("line %d: Package does not exist: %s\n"),
		 spec->lineNum, spec->line);
	goto exit;
    }


    /******************/

#if 0    
    if (headerIsEntry(pkg->header, RPMTAG_DESCRIPTION)) {
	rpmlog(RPMLOG_ERR, _("line %d: Second description\n"),
		spec->lineNum);
	goto exit;
    }
#endif

    t = stashSt(spec, pkg->header, RPMTAG_DESCRIPTION, lang);
    
    iob = rpmiobNew(0);

    if ((rc = readLine(spec, STRIP_TRAILINGSPACE | STRIP_COMMENTS)) > 0) {
	nextPart = PART_NONE;
    } else {
	if (rc) {
	    nextPart = (rpmParseState) RPMRC_FAIL;
	    goto exit;
	}
	while ((nextPart = isPart(spec)) == PART_NONE) {
	    iob = rpmiobAppend(iob, spec->line, 1);
	    if (t) t->t_nlines++;
	    if ((rc =
		readLine(spec, STRIP_TRAILINGSPACE | STRIP_COMMENTS)) > 0) {
		nextPart = PART_NONE;
		break;
	    }
	    if (rc) {
		nextPart = (rpmParseState) RPMRC_FAIL;
		goto exit;
	    }
	}
    }
    
    iob = rpmiobRTrim(iob);
    if (!(noLang && strcmp(lang, RPMBUILD_DEFAULT_LANG))) {
	(void) headerAddI18NString(pkg->header, RPMTAG_DESCRIPTION,
			rpmiobStr(iob), lang);
    }
    
    iob = rpmiobFree(iob);
     
exit:
    argv = _free(argv);
    optCon = poptFreeContext(optCon);
    return nextPart;
}

#include "config.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "intl.h"
#include "macro.h"
#include "misc.h"
#include "messages.h"
#include "read.h"
#include "rpmlib.h"
#include "spec.h"

static int matchTok(char *token, char *line);

/* returns 0 - success */
/*         1 - EOF     */
/*        <0 - error   */

void handleComments(char *s)
{
    SKIPSPACE(s);
    if (*s == '#') {
	*s = '\0';
    }
}

int readLine(Spec spec, int strip)
{
    char *from, *to, *first, *last, *s, *arch, *os;
    int match;
    char ch;
    struct ReadLevelEntry *rl;
    struct OpenFileInfo *ofi = spec->fileStack;

    /* Make sure the current file is open */
retry:
    if (!ofi->file) {
	if (!(ofi->file = fopen(ofi->fileName, "r"))) {
	    rpmError(RPMERR_BADSPEC, _("Unable to open: %s\n"),
		     ofi->fileName);
	    return RPMERR_BADSPEC;
	}
	spec->lineNum = ofi->lineNum = 0;
    }

    /* Make sure we have something in the read buffer */
    if (!ofi->readPtr || ! *(ofi->readPtr)) {
	if (!fgets(ofi->readBuf, BUFSIZ, ofi->file)) {
	    /* EOF */
	    if (spec->readStack->next) {
		rpmError(RPMERR_UNMATCHEDIF, _("Unclosed %%if"));
	        return RPMERR_UNMATCHEDIF;
	    }

	    /* remove this file from the stack */
	    spec->fileStack = ofi->next;
	    fclose(ofi->file);

	    /* only on last file do we signal EOF to caller */
	    ofi = spec->fileStack;
	    if (ofi == NULL)
		return 1;

	    /* otherwise, go back and try the read again. */
	    goto retry;
	}
	ofi->readPtr = ofi->readBuf;
	ofi->lineNum++;
	spec->lineNum = ofi->lineNum;
	/*rpmMessage(RPMMESS_DEBUG, _("LINE: %s"), spec->readBuf);*/
    }
    
    /* Copy a single line to the line buffer */
    from = ofi->readPtr;
    to = last = spec->line;
    ch = ' ';
    while (*from && ch != '\n') {
	ch = *to++ = *from++;
	if (!isspace(ch)) {
	    last = to;
	}
    }
    *to = '\0';
    ofi->readPtr = from;
    
    if (strip & STRIP_COMMENTS) {
	handleComments(spec->line);
    }
    
    if (strip & STRIP_TRAILINGSPACE) {
	*last = '\0';
    }

    if (spec->readStack->reading) {
	if (expandMacros(&spec->macros, spec->line)) {
	    rpmError(RPMERR_BADSPEC, _("%s:%d: %s"),
		     ofi->fileName, ofi->lineNum, spec->line);
	    return RPMERR_BADSPEC;
	}
    }

    rpmGetArchInfo(&arch, NULL);
    rpmGetOsInfo(&os, NULL);
    s = spec->line;
    SKIPSPACE(s);
    match = -1;
    if (! strncmp("%ifarch", s, 7)) {
	match = matchTok(arch, s);
    } else if (! strncmp("%ifnarch", s, 8)) {
	match = !matchTok(arch, s);
    } else if (! strncmp("%ifos", s, 5)) {
	match = matchTok(os, s);
    } else if (! strncmp("%ifnos", s, 6)) {
	match = !matchTok(os, s);
    } else if (! strncmp("%else", s, 5)) {
	if (! spec->readStack->next) {
	    /* Got an else with no %if ! */
	    rpmError(RPMERR_UNMATCHEDIF, _("%s:%d: Got a %%else with no if"),
		     ofi->fileName, ofi->lineNum);
	    return RPMERR_UNMATCHEDIF;
	}
	spec->readStack->reading =
	    spec->readStack->next->reading && ! spec->readStack->reading;
	spec->line[0] = '\0';
    } else if (! strncmp("%endif", s, 6)) {
	if (! spec->readStack->next) {
	    /* Got an end with no %if ! */
	    rpmError(RPMERR_UNMATCHEDIF, _("%s:%d: Got a %%endif with no if"),
		     ofi->fileName, ofi->lineNum);
	    return RPMERR_UNMATCHEDIF;
	}
	rl = spec->readStack;
	spec->readStack = spec->readStack->next;
	free(rl);
	spec->line[0] = '\0';
    } else if (! strncmp("%include", s, 8)) {
	char *fileName = s + 8, *endFileName, *p;

	if (!isspace(*fileName)) {
	    rpmError(RPMERR_BADSPEC, _("%s:%d: malformed %%include statement"),
		     ofi->fileName, ofi->lineNum);
	    return RPMERR_BADSPEC;
	}
	while (*fileName && isspace(*fileName)) fileName++;
	endFileName = fileName;
	while (*endFileName && !isspace(*endFileName)) endFileName++;
	p = endFileName;
	SKIPSPACE(p);
	if (*p != '\0') {
	    rpmError(RPMERR_BADSPEC, _("%s:%d: malformed %%include statement"),
		     ofi->fileName, ofi->lineNum);
	    return RPMERR_BADSPEC;
	}

	*endFileName = '\0';
	forceIncludeFile(spec, fileName);

	ofi = spec->fileStack;
	goto retry;
    }
    if (match != -1) {
	rl = malloc(sizeof(struct ReadLevelEntry));
	rl->reading = spec->readStack->reading && match;
	rl->next = spec->readStack;
	spec->readStack = rl;
	spec->line[0] = '\0';
    }

    if (! spec->readStack->reading) {
	spec->line[0] = '\0';
    }

    return 0;
}

static int matchTok(char *token, char *line)
{
    char buf[BUFSIZ], *tok;

    strcpy(buf, line);
    strtok(buf, " \n\t");
    while ((tok = strtok(NULL, " \n\t"))) {
	if (! strcmp(tok, token)) {
	    return 1;
	}
    }

    return 0;
}

void closeSpec(Spec spec)
{
    struct OpenFileInfo *ofi;

    while (spec->fileStack) {
	ofi = spec->fileStack;
	spec->fileStack = spec->fileStack->next;
	if (ofi->file) fclose(ofi->file);
	FREE(ofi->fileName);
	free(ofi);
    }
}

void forceIncludeFile(Spec spec, const char * fileName)
{
    struct OpenFileInfo * ofi;

    ofi = newOpenFileInfo();
    ofi->fileName = strdup(fileName);
    ofi->next = spec->fileStack;
    spec->fileStack = ofi;
}

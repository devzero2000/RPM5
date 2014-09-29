/*********************************************************************
 *
 * File        :  $Source$
 *
 * Purpose     :  pcrs is a supplement to the pcre library by Philip Hazel
 *                <ph10@cam.ac.uk> and adds Perl-style substitution. That
 *                is, it mimics Perl's 's' operator. See pcrs(3) for details.
 *
 *
 * Copyright   :  Written and Copyright (C) 2000, 2001 by Andreas S. Oesterhelt
 *                <andreas@oesterhelt.org>
 *
 *                This program is free software; you can redistribute it
 *                and/or modify it under the terms of the GNU Lesser
 *                General Public License (LGPL), version 2.1, which  should
 *                be included in this distribution (see LICENSE.txt), with
 *                the exception that the permission to replace that license
 *                with the GNU General Public License (GPL) given in section
 *                3 is restricted to version 2 of the GPL.
 *
 *                This program is distributed in the hope that it will
 *                be useful, but WITHOUT ANY WARRANTY; without even the
 *                implied warranty of MERCHANTABILITY or FITNESS FOR A
 *                PARTICULAR PURPOSE.  See the license for more details.
 *
 *                The GNU Lesser General Public License should be included
 *                with this file.  If not, you can view it at
 *                http://www.gnu.org/licenses/lgpl.html
 *                or write to the Free Software Foundation, Inc., 59
 *                Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *********************************************************************/

#include "system.h"

#include <rpmiotypes.h>

#include "pcrs.h"

#include "debug.h"

#define PCRS_H_VERSION "$Id$"
static const char pcrs_h_rcs[] = PCRS_H_VERSION;

#define FALSE 0
#define TRUE 1

/*********************************************************************
 *
 * Function    :  pcrs_strerror
 *
 * Description :  Return a string describing a given error code.
 *             
 * Parameters  :
 *          1  :  error = the error code
 *
 * Returns     :  char * to the descriptive string
 *
 *********************************************************************/
const char *pcrs_strerror(const int error)
{
    if (error < 0)
	switch (error) {
	    /* Passed-through PCRE error: */
	case PCRE_ERROR_NOMEMORY:
	    return "(pcre:) No memory";

	    /* Shouldn't happen unless PCRE or PCRS bug, or user messed with compiled job: */
	case PCRE_ERROR_NULL:
	    return "(pcre:) NULL code or subject or ovector";
	case PCRE_ERROR_BADOPTION:
	    return "(pcre:) Unrecognized option bit";
	case PCRE_ERROR_BADMAGIC:
	    return "(pcre:) Bad magic number in code";
	case PCRE_ERROR_UNKNOWN_NODE:
	    return "(pcre:) Bad node in pattern";

	    /* Can't happen / not passed: */
	case PCRE_ERROR_NOSUBSTRING:
	    return "(pcre:) Fire in power supply";
	case PCRE_ERROR_NOMATCH:
	    return "(pcre:) Water in power supply";

	    /* PCRS errors: */
	case PCRS_ERR_NOMEM:
	    return "(pcrs:) No memory";
	case PCRS_ERR_CMDSYNTAX:
	    return "(pcrs:) Syntax error while parsing command";
	case PCRS_ERR_STUDY:
	    return "(pcrs:) PCRE error while studying the pattern";
	case PCRS_ERR_BADJOB:
	    return "(pcrs:) Bad job - NULL job, pattern or substitute";
	case PCRS_WARN_BADREF:
	    return "(pcrs:) Backreference out of range";

	    /* What's that? */
	default:
	    return "Unknown error";
	}
    /* error >= 0: No error */
    return "(pcrs:) Everything's just fine. Thanks for asking.";

}

/*********************************************************************
 *
 * Function    :  pcrs_parse_perl_options
 *
 * Description :  This function parses a string containing the options to
 *                Perl's s/// operator. It returns an integer that is the
 *                pcre equivalent of the symbolic optstring.
 *                Since pcre doesn't know about Perl's 'g' (global) or pcrs',
 *                'T' (trivial) options but pcrs needs them, the corresponding
 *                flags are set if 'g'or 'T' is encountered.
 *                Note: The 'T' and 'U' options do not conform to Perl.
 *             
 * Parameters  :
 *          1  :  optstring = string with options in perl syntax
 *          2  :  flags = see description
 *
 * Returns     :  option integer suitable for pcre 
 *
 *********************************************************************/
static int pcrs_parse_perl_options(const char *optstring, int *flags)
{
    size_t i;
    int rc = 0;

    *flags = 0;

    if (optstring)
	for (i = 0; i < strlen(optstring); i++)
	    switch (optstring[i]) {
	    case 'e':
		break;		/* ToDo ;-) */
	    case 'g':
		*flags |= PCRS_GLOBAL;
		break;
	    case 'i':
		rc |= PCRE_CASELESS;
		break;
	    case 'm':
		rc |= PCRE_MULTILINE;
		break;
	    case 'o':
		break;
	    case 's':
		rc |= PCRE_DOTALL;
		break;
	    case 'x':
		rc |= PCRE_EXTENDED;
		break;
	    case 'U':
		rc |= PCRE_UNGREEDY;
		break;
	    case 'T':
		*flags |= PCRS_TRIVIAL;
		break;
	    default:
		break;
	    }
    return rc;
}

/*********************************************************************
 *
 * Function    :  pcrs_compile_replacement
 *
 * Description :  This function takes a Perl-style replacement (2nd argument
 *                to the s/// operator and returns a compiled pcrs_substitute,
 *                or NULL if memory allocation for the substitute structure
 *                fails.
 *
 * Parameters  :
 *          1  :  replacement = replacement part of s/// operator
 *                              in perl syntax
 *          2  :  trivialflag = Flag that causes backreferences to be
 *                              ignored.
 *          3  :  capturecount = Number of capturing subpatterns in
 *                               the pattern. Needed for $+ handling.
 *          4  :  errptr = pointer to an integer in which error
 *                         conditions can be returned.
 *
 * Returns     :  pcrs_substitute data structure, or NULL if an
 *                error is encountered. In that case, *errptr has
 *                the reason.
 *
 *********************************************************************/
static pcrs_substitute *pcrs_compile_replacement(const char *replacement,
		int trivialflag, int capturecount, int *errptr)
{
    int i = 0;
    int k = 0;
    int l = 0;
    int quoted = 0;
    size_t length;
    char *text;
    pcrs_substitute *r;

    /* Sanity check */
    if (replacement == NULL)
	replacement = "";

    /* Get memory or fail */
    r = (pcrs_substitute *) xcalloc(1, sizeof(pcrs_substitute));

    length = strlen(replacement);

    text = (char *) xcalloc(1, length + 1);

    if (trivialflag) {	/* In trivial mode, just copy the substitute text */
	text = strncpy(text, replacement, length + 1);
	k = length;
    } else {		/* Else, parse, cut out and record all backreferences */
	while (i < (int) length) {
	    /* Quoting */
	    if (replacement[i] == '\\') {
		if (quoted) {
		    text[k++] = replacement[i++];
		    quoted = 0;
		} else {
		    if (replacement[i+1] && strchr("tnrfae0", replacement[i+1]))
		    {
			switch (replacement[++i]) {
			case 't':
			    text[k++] = '\t';
			    break;
			case 'n':
			    text[k++] = '\n';
			    break;
			case 'r':
			    text[k++] = '\r';
			    break;
			case 'f':
			    text[k++] = '\f';
			    break;
			case 'a':
			    text[k++] = 7;
			    break;
			case 'e':
			    text[k++] = 27;
			    break;
			case '0':
			    text[k++] = '\0';
			    break;
			}
		    } else
			quoted = 1;
		    i++;
		}
		continue;
	    }

	    /* Backreferences */
	    if (replacement[i] == '$' && !quoted && i < (int)(length - 1)) {
		static char symbols[] = "'`+&";
		char *symbol;
		r->block_length[l] = k - r->block_offset[l];

		/* Numerical backreferences */
		if (isdigit((int) replacement[i + 1])) {
		    while (i < (int) length && isdigit((int) replacement[++i]))
			r->backref[l] = r->backref[l]*10 + replacement[i] - 48;
		    if (r->backref[l] > capturecount)
			*errptr = PCRS_WARN_BADREF;
		} else /* Symbolic backreferences: */
		if ((symbol = strchr(symbols, replacement[i + 1])) != NULL) {

		    switch (symbol - symbols) {
		    case 2:
			r->backref[l] = capturecount;
			break;	/* $+ */
		    case 3:
			r->backref[l] = 0;
			break;	/* $& */
		    default:	/* $' or $` */
			r->backref[l] =
			    PCRS_MAX_SUBMATCHES + 1 - (symbol - symbols);
			break;
		    }
		    i += 2;
		} else		/* Invalid backref -> plain '$' */
		    goto plainchar;

		/* Valid and in range? -> record */
		if (r->backref[l] < PCRS_MAX_SUBMATCHES + 2) {
		    r->backref_count[r->backref[l]] += 1;
		    r->block_offset[++l] = k;
		} else
		    *errptr = PCRS_WARN_BADREF;
		continue;
	    }

	  plainchar:
	    /* Plain chars are copied */
	    text[k++] = replacement[i++];
	    quoted = 0;
	}
    }				/* -END- if (!trivialflag) */

    /* Finish & return */
    r->text = text;
    r->backrefs = l;
    r->block_length[l] = k - r->block_offset[l];

    return r;
}

/*********************************************************************
 *
 * Function    :  pcrs_free_job
 *
 * Description :  Frees the memory used by a pcrs_job struct and its
 *                dependant structures.
 *
 * Parameters  :
 *          1  :  job = pointer to the pcrs_job structure to be freed
 *
 * Returns     :  a pointer to the next job, if there was any, or
 *                NULL otherwise. 
 *
 *********************************************************************/
pcrs_job *pcrs_free_job(pcrs_job * job)
{
    pcrs_job *next;

    if (job == NULL)
	return NULL;
    next = job->next;
    if (job->pattern != NULL)
	free(job->pattern);
    if (job->hints != NULL)
	free(job->hints);
    if (job->substitute != NULL) {
	if (job->substitute->text != NULL)
	    free(job->substitute->text);
	free(job->substitute);
    }
    free(job);
    return next;
}

/*********************************************************************
 *
 * Function    :  pcrs_free_joblist
 *
 * Description :  Iterates through a chained list of pcrs_job's and
 *                frees them using pcrs_free_job.
 *
 * Parameters  :
 *          1  :  joblist = pointer to the first pcrs_job structure to
 *                be freed
 *
 * Returns     :  N/A
 *
 *********************************************************************/
void pcrs_free_joblist(pcrs_job * joblist)
{
    while ((joblist = pcrs_free_job(joblist)) != NULL)
	{ };

    return;

}

/*********************************************************************
 *
 * Function    :  pcrs_compile_command
 *
 * Description :  Parses a string with a Perl-style s/// command, 
 *                calls pcrs_compile, and returns a corresponding
 *                pcrs_job, or NULL if parsing or compiling the job
 *                fails.
 *
 * Parameters  :
 *          1  :  command = string with perl-style s/// command
 *          2  :  errptr = pointer to an integer in which error
 *                         conditions can be returned.
 *
 * Returns     :  a corresponding pcrs_job data structure, or NULL
 *                if an error was encountered. In that case, *errptr
 *                has the reason.
 *
 *********************************************************************/
pcrs_job *pcrs_compile_command(const char *command, int *errptr)
{
    size_t limit;
    char delimiter;
    char *tokens[4] = { NULL, NULL, NULL, NULL };
    pcrs_job *newjob = NULL;	/* assume error */
    int quoted = FALSE;
    int i = 0;
    int k = 0;
    int l = 0;

    /* Tokenize the perl command */
    limit = strlen(command);
    if (limit < sizeof("s///") - 1)
	goto exit;
    delimiter = command[1];

    tokens[0] = (char *) xmalloc(limit + 1);

    for (i = 0; i <= (int) limit; i++) {

	if (command[i] == delimiter && !quoted) {
	    if (l == 3) {
		l = -1;
		break;
	    }
	    tokens[0][k++] = '\0';
	    tokens[++l] = tokens[0] + k;
	    continue;
	} else if (command[i] == '\\' && !quoted) {
	    quoted = TRUE;
	    if (command[i + 1] == delimiter)
		continue;
	} else
	    quoted = FALSE;
	tokens[0][k++] = command[i];
    }

    /* Syntax error ? */
    if (l != 3)
	goto exit;

    newjob = pcrs_compile(tokens[1], tokens[2], tokens[3], errptr);

exit:
    if (tokens[0])
	free(tokens[0]);
    if (newjob == NULL)
	*errptr = PCRS_ERR_CMDSYNTAX;
    return newjob;
}

/*********************************************************************
 *
 * Function    :  pcrs_compile
 *
 * Description :  Takes the three arguments to a perl s/// command
 *                and compiles a pcrs_job structure from them.
 *
 * Parameters  :
 *          1  :  pattern = string with perl-style pattern
 *          2  :  substitute = string with perl-style substitute
 *          3  :  options = string with perl-style options
 *          4  :  errptr = pointer to an integer in which error
 *                         conditions can be returned.
 *
 * Returns     :  a corresponding pcrs_job data structure, or NULL
 *                if an error was encountered. In that case, *errptr
 *                has the reason.
 *
 *********************************************************************/
pcrs_job *pcrs_compile(const char *pattern, const char *substitute,
		       const char *options, int *errptr)
{
    pcrs_job *newjob;
    int flags;
    int capturecount;
    const char *error;

    *errptr = 0;

    /* Handle NULL arguments */
    if (pattern == NULL)
	pattern = "";
    if (substitute == NULL)
	substitute = "";

    /* Get and init memory */
    newjob = (pcrs_job *) xcalloc(1, sizeof(pcrs_job));

    /* Evaluate the options */
    newjob->options = pcrs_parse_perl_options(options, &flags);
    newjob->flags = flags;

    /* Compile the pattern */
    newjob->pattern =
	pcre_compile(pattern, newjob->options, &error, errptr, NULL);
    if (newjob->pattern == NULL) {
	pcrs_free_job(newjob);
	return NULL;
    }

    /*
     * Generate hints. This has little overhead, since the
     * hints will be NULL for a boring pattern anyway.
     */
    newjob->hints = pcre_study(newjob->pattern, 0, &error);
    if (error != NULL) {
	*errptr = PCRS_ERR_STUDY;
	pcrs_free_job(newjob);
	return NULL;
    }

    /* 
     * Determine the number of capturing subpatterns. 
     * This is needed for handling $+ in the substitute.
     */
    *errptr = pcre_fullinfo(newjob->pattern, newjob->hints,
		       PCRE_INFO_CAPTURECOUNT, &capturecount);
    if (*errptr < 0) {
	pcrs_free_job(newjob);
	return NULL;
    }

    /*
     * Compile the substitute
     */
    newjob->substitute =
	 pcrs_compile_replacement(substitute, newjob->flags & PCRS_TRIVIAL,
				  capturecount, errptr);
    if (newjob->substitute == NULL) {
	pcrs_free_job(newjob);
	return NULL;
    }

    return newjob;
}

/*********************************************************************
 *
 * Function    :  pcrs_execute_list
 *
 * Description :  This is a multiple job wrapper for pcrs_execute().
 *                Apply the regular substitutions defined by the jobs in
 *                the joblist to the subject.
 *                The subject itself is left untouched, memory for the result
 *                is malloc()ed and it is the caller's responsibility to free
 *                the result when it's no longer needed. 
 *
 *                Note: For convenient string handling, a null byte is
 *                      appended to the result. It does not count towards the
 *                      result_length, though.
 *
 *
 * Parameters  :
 *          1  :  joblist = the chained list of pcrs_jobs to be executed
 *          2  :  subject = the subject string
 *          3  :  subject_length = the subject's length 
 *          4  :  result = char** for returning  the result 
 *          5  :  result_length = size_t* for returning the result's length
 *
 * Returns     :  On success, the number of substitutions that were made.
 *                 May be > 1 if job->flags contained PCRS_GLOBAL
 *                On failiure, the (negative) pcre error code describing the
 *                 failiure, which may be translated to text using pcrs_strerror().
 *
 *********************************************************************/
int pcrs_execute_list(pcrs_job * joblist, char *s, size_t ns,
		char **result, size_t * result_length)
{
    pcrs_job *job;
    char *old = s;
    char *new = NULL;
    int hits = 0;
    int total_hits = 0;

    *result_length = ns;

    for (job = joblist; job != NULL; job = job->next) {
	hits = pcrs_execute(job, old, *result_length, &new, result_length);

	if (old != s)
	    free(old);

	if (hits < 0)
	    return (hits);
	total_hits += hits;
	old = new;
    }

    *result = new;
    return (total_hits);
}

/*********************************************************************
 *
 * Function    :  pcrs_execute
 *
 * Description :  Apply the regular substitution defined by the job to the
 *                subject.
 *                The subject itself is left untouched, memory for the result
 *                is malloc()ed and it is the caller's responsibility to free
 *                the result when it's no longer needed.
 *
 *                Note: For convenient string handling, a null byte is
 *                      appended to the result. It does not count towards the
 *                      result_length, though.
 *
 * Parameters  :
 *          1  :  job = the pcrs_job to be executed
 *          2  :  subject = the subject (== original) string
 *          3  :  subject_length = the subject's length 
 *          4  :  result = char** for returning  the result 
 *          5  :  result_length = size_t* for returning the result's length
 *
 * Returns     :  On success, the number of substitutions that were made.
 *                 May be > 1 if job->flags contained PCRS_GLOBAL
 *                On failiure, the (negative) pcre error code describing the
 *                 failiure, which may be translated to text using pcrs_strerror().
 *
 *********************************************************************/
int pcrs_execute(pcrs_job * job, char *s, size_t ns,
		 char **result, size_t * result_length)
{
    const size_t noffsets = PCRS_MAX_SUBMATCHES;
    int offsets[3 * noffsets];
    int nmatches = PCRS_MAX_MATCH_INIT;
    pcrs_match *matches = NULL;
    pcrs_substitute *jsp = (job ? job->substitute : NULL);
    size_t soff;
    char * te;
    char * t = NULL;
    size_t nt = 0;
    int nfound = PCRS_ERR_BADJOB;	/* assume error */
    int i;
    int k;

    if (job == NULL || job->pattern == NULL || jsp == NULL)
	goto exit;

    matches = (pcrs_match *) xcalloc(nmatches, sizeof(*matches));

    /*
     * Find the pattern and calculate the space requirements for the result
     */
    nt = ns;
    soff = 0;
    i = 0;
    while ((nfound = pcre_exec(job->pattern, job->hints, s, ns, soff,
		0, offsets, 3 * noffsets)) > 0)
    {
	pcrs_match * mip = matches + i;

	job->flags |= PCRS_SUCCESS;
	mip->submatches = nfound;

	for (k = 0; k < nfound; k++) {
	    mip->submatch_offset[k] = offsets[2 * k];

	    /* Note: Non-found optional submatches have length -1-(-1)==0 */
	    mip->submatch_length[k] = offsets[2 * k + 1] - offsets[2 * k];

	    /* reserve mem for each submatch as often as it is ref'd */
	    nt += mip->submatch_length[k] * jsp->backref_count[k];
	}
	/* plus replacement text size minus match text size */
	nt += strlen(jsp->text) - mip->submatch_length[0];

	/* chunk before match */
	mip->submatch_offset[noffsets] = 0;
	mip->submatch_length[noffsets] = offsets[0];
	nt += offsets[0] * jsp->backref_count[noffsets];

	/* chunk after match */
	mip->submatch_offset[noffsets + 1] = offsets[1];
	mip->submatch_length[noffsets + 1] = ns - offsets[1] - 1;
	nt += (ns - offsets[1]) * jsp->backref_count[noffsets + 1];

	/* Storage for matches exhausted? -> Extend! */
	if (++i >= nmatches) {
	    pcrs_match *newp;
	    nmatches = (int) (nmatches * PCRS_MAX_MATCH_GROW);
	    newp = (pcrs_match *)xrealloc(matches, nmatches * sizeof(*matches));
	    matches = newp;
	}

	/* Non-global search or limit reached? */
	if (!(job->flags & PCRS_GLOBAL))
	    break;

	/* Don't loop on empty matches */
	if (offsets[1] == (int)soff)
	    if (soff < ns)
		soff++;
	    else
		break;
	/* Go find the next one */
	else
	    soff = offsets[1];
    }
    /* Pass pcre error through if (bad) failiure */
    if (nfound < PCRE_ERROR_NOMATCH)
	goto exit;

    nfound = i;

    /* 
     * Get memory for the result (must be freed by caller!)
     * and append terminating null byte.
     */
    t = (char *) xmalloc(nt + 1);
    t[nt] = '\0';

    /* 
     * Replace
     */
    for (i = 0, soff = 0, te = t; i < nfound; i++) {
	pcrs_match * mip = matches + i;
	size_t msoff;
	size_t mslen;

	/* copy the chunk preceding the match */
	msoff = mip->submatch_offset[0];
	memcpy(te, s + soff, msoff - soff);
	te += msoff - soff;

	/* For every segment of the substitute.. */
	for (k = 0; k <= jsp->backrefs; k++) {
	    size_t jboff = jsp->block_offset[k];
	    size_t jblen = jsp->block_length[k];
	    size_t kx = jsp->backref[k];

	    /* ...copy its text.. */
	    memcpy(te, jsp->text + jboff, jblen);
	    te += jblen;

	    msoff = mip->submatch_offset[kx];
	    mslen = mip->submatch_length[kx];

	    /* ..plus, if not the last chunk, i.e.: There *is* a backref.. */
	    if (!(k != jsp->backrefs
		/* ..in legal range.. */
	     && kx < noffsets + 2
		/* ..and referencing a real submatch.. */
	     && kx < mip->submatches
		/* ..that is nonempty.. */
	     && mslen > 0))
		continue;

	    /* ..copy the submatch that is ref'd. */
	    memcpy(te, s + msoff, mslen);
	    te += mslen;
	}

	msoff = mip->submatch_offset[0];
	mslen = mip->submatch_length[0];
	soff = msoff + mslen;
    }

    /* Copy the rest. */
    memcpy(te, s + soff, ns - soff);
    te += (ns - soff);
    *te = '\0';

exit:
    if (matches)
	free(matches);
    *result = t;
    *result_length = nt;
    return nfound;
}

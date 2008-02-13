/*************************************************
*               pcregrep program                 *
*************************************************/

/* This is a grep program that uses the PCRE regular expression library to do
its pattern matching. On a Unix or Win32 system it can recurse into
directories.

           Copyright (c) 1997-2008 University of Cambridge

-----------------------------------------------------------------------------
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    * Neither the name of the University of Cambridge nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
-----------------------------------------------------------------------------
*/

#include "system.h"
const char *__progname;

#define _MIRE_INTERNAL
#include <rpmio_internal.h>
#include <mire.h>

#include <argv.h>
#include <pcre.h>

#ifdef SUPPORT_LIBZ
#include <zlib.h>
#endif

#ifdef SUPPORT_LIBBZ2
#include <bzlib.h>
#endif

#include "debug.h"

#define FALSE 0
#define TRUE 1

typedef int BOOL;

#define MAX_PATTERN_COUNT 100

#if BUFSIZ > 8192
#define MBUFTHIRD BUFSIZ
#else
#define MBUFTHIRD 8192
#endif

/**
 * Values for the "filenames" variable, which specifies options for file name
 * output. The order is important; it is assumed that a file name is wanted for
 * all values greater than FN_DEFAULT.
 */
enum { FN_NONE, FN_DEFAULT, FN_ONLY, FN_NOMATCH_ONLY, FN_FORCE };

/** File reading styles */
enum { FR_PLAIN, FR_LIBZ, FR_LIBBZ2 };

/** Actions for the -d and -D options */
enum { dee_READ, dee_SKIP, dee_RECURSE };
enum { DEE_READ, DEE_SKIP };

/** Actions for special processing options (flag bits) */
#define PO_WORD_MATCH     0x0001
#define PO_LINE_MATCH     0x0002
#define PO_FIXED_STRINGS  0x0004

/** Line ending types */
enum { EL_LF, EL_CR, EL_CRLF, EL_ANY, EL_ANYCRLF };

/*************************************************
*               Global variables                 *
*************************************************/

/* Jeffrey Friedl has some debugging requirements that are not part of the
regular code. */

#ifdef JFRIEDL_DEBUG
static int S_arg = -1;
static unsigned int jfriedl_XR = 0; /* repeat regex attempt this many times */
static unsigned int jfriedl_XT = 0; /* replicate text this many times */
static const char *jfriedl_prefix = "";
static const char *jfriedl_postfix = "";
#endif

static int  endlinetype;

static char *colour_string = (char *)"1;31";
static char *colour_option = NULL;
static char *dee_option = NULL;
static char *DEE_option = NULL;
static char *newline = NULL;
static char *pattern_filename = NULL;
static char *stdin_name = (char *)"(standard input)";
static char *locale = NULL;

static const unsigned char *pcretables = NULL;

static int  pattern_count = 0;
static miRE pattern_list = NULL;

static const char *include_pattern = NULL;
static miRE includeMire = NULL;

static const char *exclude_pattern = NULL;
static miRE excludeMire = NULL;

static int after_context = 0;
static int before_context = 0;
static int both_context = 0;
static int dee_action = dee_READ;
static int DEE_action = DEE_READ;
static int error_count = 0;
static int filenames = FN_DEFAULT;
static int process_options = 0;

static int global_options = 0;
static ARGV_t patterns = NULL;
static int npatterns = 0;

static BOOL count_only = FALSE;
static BOOL do_colour = FALSE;
static BOOL file_offsets = FALSE;
static BOOL hyphenpending = FALSE;
static BOOL invert = FALSE;
static BOOL line_offsets = FALSE;
static BOOL multiline = FALSE;
static BOOL number = FALSE;
static BOOL only_matching = FALSE;
static BOOL quiet = FALSE;
static BOOL silent = FALSE;
static BOOL utf8 = FALSE;

/**
 * Tables for prefixing and suffixing patterns, according to the -w, -x, and -F
 * options. These set the 1, 2, and 4 bits in process_options, respectively.
 * Note that the combination of -w and -x has the same effect as -x on its own,
 * so we can treat them as the same.
 */
static const char *prefix[] = {
  "", "\\b", "^(?:", "^(?:", "\\Q", "\\b\\Q", "^(?:\\Q", "^(?:\\Q" };

static const char *suffix[] = {
  "", "\\b", ")$",   ")$",   "\\E", "\\E\\b", "\\E)$",   "\\E)$" };

/** UTF-8 tables - used only when the newline setting is "any". */
const int utf8_table3[] = { 0xff, 0x1f, 0x0f, 0x07, 0x03, 0x01};

const char utf8_table4[] = {
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  3,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5 };

/*************************************************
*            OS-specific functions               *
*************************************************/

/* These functions are defined so that they can be made system specific,
although at present the only ones are for Unix, Win32, and for "no support". */


/************* Directory scanning in Unix ***********/

#if defined HAVE_SYS_STAT_H && defined HAVE_DIRENT_H && defined HAVE_SYS_TYPES_H

typedef DIR directory_type;

static int
isdirectory(const char *filename)
{
    struct stat statbuf;
    if (Stat(filename, &statbuf) < 0)
	return 0;	/* In the expectation that opening as a file will fail */
    return ((statbuf.st_mode & S_IFMT) == S_IFDIR)? '/' : 0;
}

static directory_type *
opendirectory(const char *filename)
{
    return Opendir(filename);
}

static char *
readdirectory(directory_type *dir)
{
    for (;;) {
	struct dirent *dent = Readdir(dir);
	if (dent == NULL)
	    return NULL;
	if (strcmp(dent->d_name, ".") != 0 && strcmp(dent->d_name, "..") != 0)
	    return dent->d_name;
    }
    /*@notreached@*/
}

static void
closedirectory(directory_type *dir)
{
    Closedir(dir);
}

/************* Test for regular file in Unix **********/
static int
isregfile(const char *filename)
{
    struct stat sb;
    if (Stat(filename, &sb) < 0)
	return 1;	/* In the expectation that opening as a file will fail */
    return (sb.st_mode & S_IFMT) == S_IFREG;
}

/************* Test stdout for being a terminal in Unix **********/
static BOOL
is_stdout_tty(void)
{
    return isatty(fileno(stdout));
}

#else

typedef void directory_type;

int isdirectory(char *filename) { return 0; }
directory_type * opendirectory(char *filename) { return (directory_type *)NULL;}
char * readdirectory(directory_type *dir) { return (char *)NULL;}
void closedirectory(directory_type *dir) {}
int isregfile(char *filename) { return 1; } /* Assume all files are regular. */
static BOOL is_stdout_tty(void) { return FALSE; }

#endif

#ifndef HAVE_STRERROR
extern int   sys_nerr;
extern char *sys_errlist[];

char *
strerror(int n)
{
    if (n < 0 || n >= sys_nerr)
	return "unknown error number";
    return sys_errlist[n];
}
#endif /* HAVE_STRERROR */

/*************************************************
 * Find end of line.
 *
 * The length of the endline sequence that is found is set via lenptr.
 * This may be zero at the very end of the file if there is no line-ending
 * sequence there.
 *
 * @param p		current position in line
 * @param endptr	end of available data
 * @retval *lenptr	length of the eol sequence
 * @return		pointer to the last byte of the line
*/
static const char *
end_of_line(const char *p, const char *endptr, int *lenptr)
{
    switch(endlinetype) {
    default:	/* Just in case */
    case EL_LF:
	while (p < endptr && *p != '\n') p++;
	if (p < endptr) {
	    *lenptr = 1;
	    return p + 1;
	}
	*lenptr = 0;
	return endptr;

    case EL_CR:
	while (p < endptr && *p != '\r') p++;
	if (p < endptr) {
	    *lenptr = 1;
	    return p + 1;
	}
	*lenptr = 0;
	return endptr;

    case EL_CRLF:
	for (;;) {
	    while (p < endptr && *p != '\r') p++;
	    if (++p >= endptr) {
		*lenptr = 0;
		return endptr;
	    }
	    if (*p == '\n') {
		*lenptr = 2;
		return p + 1;
	    }
	}
	break;

    case EL_ANYCRLF:
	while (p < endptr) {
	    int extra = 0;
	    register int c = *((unsigned char *)p);

	    if (utf8 && c >= 0xc0) {
		int gcii, gcss;
		extra = utf8_table4[c & 0x3f];  /* No. of additional bytes */
		gcss = 6*extra;
		c = (c & utf8_table3[extra]) << gcss;
		for (gcii = 1; gcii <= extra; gcii++) {
		    gcss -= 6;
		    c |= (p[gcii] & 0x3f) << gcss;
		}
	    }

	    p += 1 + extra;

	    switch (c) {
	    case 0x0a:    /* LF */
		*lenptr = 1;
		return p;

	    case 0x0d:    /* CR */
		if (p < endptr && *p == 0x0a) {
		    *lenptr = 2;
		    p++;
		}
		else *lenptr = 1;
		return p;

	    default:
		break;
		}
	}   /* End of loop for ANYCRLF case */

	*lenptr = 0;  /* Must have hit the end */
	return endptr;

    case EL_ANY:
	while (p < endptr) {
	    int extra = 0;
	    register int c = *((unsigned char *)p);

	    if (utf8 && c >= 0xc0) {
		int gcii, gcss;
		extra = utf8_table4[c & 0x3f];  /* No. of additional bytes */
		gcss = 6*extra;
		c = (c & utf8_table3[extra]) << gcss;
		for (gcii = 1; gcii <= extra; gcii++) {
		    gcss -= 6;
		    c |= (p[gcii] & 0x3f) << gcss;
		}
	    }

	    p += 1 + extra;

	    switch (c) {
	    case 0x0a:    /* LF */
	    case 0x0b:    /* VT */
	    case 0x0c:    /* FF */
		*lenptr = 1;
		return p;

	    case 0x0d:    /* CR */
		if (p < endptr && *p == 0x0a) {
		    *lenptr = 2;
		    p++;
		}
		else *lenptr = 1;
		return p;

	    case 0x85:    /* NEL */
		*lenptr = utf8? 2 : 1;
		return p;

	    case 0x2028:  /* LS */
	    case 0x2029:  /* PS */
		*lenptr = 3;
		return p;

	    default:
		break;
	    }
	}   /* End of loop for ANY case */

	*lenptr = 0;  /* Must have hit the end */
	return endptr;
    }	/* End of overall switch */
}

/*************************************************
 * Find start of previous line
 *
 * This is called when looking back for before lines to print.
 *
 * @param p		start of the subsequent line
 * @param startptr	start of available data
 * @return 		pointer to the start of the previous line
 */
static const char *
previous_line(const char *p, const char *startptr)
{
    switch (endlinetype) {
    default:      /* Just in case */
    case EL_LF:
	p--;
	while (p > startptr && p[-1] != '\n') p--;
	return p;

    case EL_CR:
	p--;
	while (p > startptr && p[-1] != '\n') p--;
	return p;

    case EL_CRLF:
	for (;;) {
	    p -= 2;
	    while (p > startptr && p[-1] != '\n') p--;
	    if (p <= startptr + 1 || p[-2] == '\r') return p;
	}
	return p;   /* But control should never get here */

    case EL_ANY:
    case EL_ANYCRLF:
	if (*(--p) == '\n' && p > startptr && p[-1] == '\r') p--;
	if (utf8) while ((*p & 0xc0) == 0x80) p--;

	while (p > startptr) {
	    const char *pp = p - 1;
	    register int c;

	    if (utf8) {
		int extra = 0;
		while ((*pp & 0xc0) == 0x80) pp--;
		c = *((unsigned char *)pp);
		if (c >= 0xc0) {
		    int gcii, gcss;
		    extra = utf8_table4[c & 0x3f]; /* No. of additional bytes */
		    gcss = 6*extra;
		    c = (c & utf8_table3[extra]) << gcss;
		    for (gcii = 1; gcii <= extra; gcii++) {
			gcss -= 6;
			c |= (pp[gcii] & 0x3f) << gcss;
		    }
		}
	    } else
		c = *((unsigned char *)pp);

	    if (endlinetype == EL_ANYCRLF) {
		switch (c) {
		case 0x0a:    /* LF */
		case 0x0d:    /* CR */
		    return p;

		default:
		    break;
		}
	    } else {
		switch (c) {
		case 0x0a:    /* LF */
		case 0x0b:    /* VT */
		case 0x0c:    /* FF */
		case 0x0d:    /* CR */
		case 0x85:    /* NEL */
		case 0x2028:  /* LS */
		case 0x2029:  /* PS */
		    return p;

		default:
		    break;
		}
	    }

	    p = pp;  /* Back one character */
	}	/* End of loop for ANY case */

	return startptr;  /* Hit start of data */
    }	/* End of overall switch */
}

/*************************************************
 * Print the previous "after" lines
 *
 * This is called if we are about to lose said lines because of buffer filling,
 * and at the end of the file. The data in the line is written using fwrite() so
 * that a binary zero does not terminate it.
 *
 * @param lastmatchnumber	the number of the last matching line, plus one
 * @param lastmatchrestart	where we restarted after the last match
 * @param endptr		end of available data
 * @param printname		filename for printing
 */
static void do_after_lines(int lastmatchnumber, const char *lastmatchrestart,
		const char *endptr, const char *printname)
{
    if (after_context > 0 && lastmatchnumber > 0) {
	int count = 0;
	while (lastmatchrestart < endptr && count++ < after_context) {
	    const char *pp = lastmatchrestart;
	    int ellength;
	    if (printname != NULL) fprintf(stdout, "%s-", printname);
	    if (number) fprintf(stdout, "%d-", lastmatchnumber++);
	    pp = end_of_line(pp, endptr, &ellength);
	    fwrite(lastmatchrestart, 1, pp - lastmatchrestart, stdout);
	    lastmatchrestart = pp;
	}
	hyphenpending = TRUE;
    }
}

/*************************************************
 * Grep an individual file
 *
 * This is called from grep_or_recurse() below. It uses a buffer that is three
 * times the value of MBUFTHIRD. The matching point is never allowed to stray
 * into the top third of the buffer, thus keeping more of the file available
 * for context printing or for multiline scanning. For large files, the pointer
 * will be in the middle third most of the time, so the bottom third is
 * available for "before" context printing.
 *
 * @param  handle	the fopen'd FILE stream for a normal file
 *			the gzFile pointer when reading is via libz
 *			the BZFILE pointer when reading is via libbz2
 * @param frtype	FR_PLAIN, FR_LIBZ, or FR_LIBBZ2
 * @param printname	the file name if it is to be printed for each match
 *			or NULL if the file name is not to be printed
 *			it cannot be NULL if filenames[_nomatch]_only is set
 * @return		0: at least one match, 1: no match, 2: read error (bz2)
 */
static int
pcregrep(void *handle, int frtype, const char *printname)
{
    int rc = 1;
    int linenumber = 1;
    int lastmatchnumber = 0;
    int count = 0;
    int filepos = 0;
    int offsets[99];
    const char *lastmatchrestart = NULL;
    char buffer[3*MBUFTHIRD];
    char *ptr = buffer;
    char *endptr;
    size_t bufflength;
    BOOL endhyphenpending = FALSE;
    FILE *in = NULL;	/* Ensure initialized */

#ifdef SUPPORT_LIBZ
    gzFile ingz = NULL;
#endif

#ifdef SUPPORT_LIBBZ2
    BZFILE *inbz2 = NULL;
#endif

    /*
     * Do the first read into the start of the buffer and set up the pointer
     * to end of what we have. In the case of libz, a non-zipped .gz file will
     * be read as a plain file. However, if a .bz2 file isn't actually bzipped,
     * the first read will fail.
     */
#ifdef SUPPORT_LIBZ
    if (frtype == FR_LIBZ) {
	ingz = (gzFile)handle;
	bufflength = gzread (ingz, buffer, 3*MBUFTHIRD);
    } else
#endif

#ifdef SUPPORT_LIBBZ2
    if (frtype == FR_LIBBZ2) {
	inbz2 = (BZFILE *)handle;
	bufflength = BZ2_bzread(inbz2, buffer, 3*MBUFTHIRD);
	/* Gotcha: bufflength is size_t; without the cast it is unsigned. */
	if ((int)bufflength < 0) {
	    rc = 2;
	    goto exit;
	}
    } else
#endif

    {
	in = (FILE *)handle;
	bufflength = fread(buffer, 1, 3*MBUFTHIRD, in);
    }

    endptr = buffer + bufflength;

    /*
     * Loop while the current pointer is not at the end of the file. For large
     * files, endptr will be at the end of the buffer when we are in the middle
     * of the file, but ptr will never get there, because as soon as it gets
     * over 2/3 of the way, the buffer is shifted left and re-filled.
     */
    while (ptr < endptr) {
	int i, endlinelength;
	int mrc = 0;
	BOOL match = FALSE;
	char *matchptr = ptr;
	const char *t = ptr;
	size_t length, linelength;

	/*
	 * At this point, ptr is at the start of a line. We need to find the
	 * length of the subject string to pass to mireRegexec(). In multiline
	 * mode, it is the length remainder of the data in the buffer.
	 * Otherwise, it is the length of the next line. After matching, we
	 * always advance by the length of the next line. In multiline mode
	 * the PCRE_FIRSTLINE option is used for compiling, so that any match
	 * is constrained to be in the first line.
	 */
	t = end_of_line(t, endptr, &endlinelength);
	linelength = t - ptr - endlinelength;
	length = multiline? (size_t)(endptr - ptr) : linelength;

	/* Extra processing for Jeffrey Friedl's debugging. */
#ifdef JFRIEDL_DEBUG
	if (jfriedl_XT || jfriedl_XR) {
	    struct timeval start_time, end_time;
	    struct timezone dummy;
	    double delta;

	    if (jfriedl_XT) {
		unsigned long newlen = length * jfriedl_XT + strlen(jfriedl_prefix) + strlen(jfriedl_postfix);
		const char *orig = ptr;
		endptr = ptr = xmalloc(newlen + 1);
		strcpy(endptr, jfriedl_prefix);
		endptr += strlen(jfriedl_prefix);
		for (i = 0; i < jfriedl_XT; i++) {
		    strncpy(endptr, orig,  length);
		    endptr += length;
		}
		strcpy(endptr, jfriedl_postfix);
		endptr += strlen(jfriedl_postfix);
		length = newlen;
	    }

	    if (gettimeofday(&start_time, &dummy) != 0)
		perror("bad gettimeofday");

	    for (i = 0; i < jfriedl_XR; i++) {
		miRE mire = pattern_list;
		mire->startoff = 0;	/* XXX needed? */
		mire->eoptions = 0;	/* XXX needed? */
		/* XXX save offsets for use by pcre_exec. */
		mire->offsets = offsets;
		mire->noffsets = 99;
		/* XXX WATCHOUT: mireRegexec w length=0 does strlen(matchptr)! */
		match = ((length > 0 ? mireRegexec(mire, matchptr, length) : PCRE_ERROR_NOMATCH) >= 0);
	    }

	    if (gettimeofday(&end_time, &dummy) != 0)
		perror("bad gettimeofday");

	    delta = ((end_time.tv_sec + (end_time.tv_usec / 1000000.0))
		            -
		    (start_time.tv_sec + (start_time.tv_usec / 1000000.0)));
	    printf("%s TIMER[%.4f]\n", match ? "MATCH" : "FAIL", delta);
	    rc = 0;
	    goto exit;
	}
#endif

	/*
	 * We come back here after a match when the -o option (only_matching)
	 * is set, in order to find any further matches in the same line.
	 */
ONLY_MATCHING_RESTART:

	/*
	 * Run through all the patterns until one matches. Note that we don't
	 * include the final newline in the subject string.
	 */
	for (i = 0; i < pattern_count; i++) {
	    miRE mire = pattern_list + i;
	    mire->startoff = 0;	/* XXX needed? */
	    mire->eoptions = 0;	/* XXX needed? */
	    /* XXX save offsets for use by pcre_exec. */
	    mire->offsets = offsets;
	    mire->noffsets = 99;
	    /* XXX WATCHOUT: mireRegexec w length=0 does strlen(matchptr)! */
	    mrc = (length > 0 ? mireRegexec(mire, matchptr, length) : PCRE_ERROR_NOMATCH);
	    if (mrc >= 0) { match = TRUE; break; }
	    if (mrc != PCRE_ERROR_NOMATCH) {
		fprintf(stderr, _("%s: pcre_exec() error %d while matching "), __progname, mrc);
		if (pattern_count > 1) fprintf(stderr, _("pattern number %d to "), i+1);
		fprintf(stderr, _("this line:\n"));
		fwrite(matchptr, 1, linelength, stderr);  /* In case binary zero included */
		fprintf(stderr, "\n");
		if (error_count == 0 &&
			(mrc == PCRE_ERROR_MATCHLIMIT || mrc == PCRE_ERROR_RECURSIONLIMIT))
		{
		    fprintf(stderr,
			_("%s: error %d means that a resource limit was exceeded\n"),
			__progname, mrc);
		    fprintf(stderr,
			_("%s: check your regex for nested unlimited loops\n"),
			__progname);
		}
		if (error_count++ > 20) {
		    fprintf(stderr, _("%s: too many errors - abandoned\n"),
			__progname);
		    exit(2);
		}
		match = invert;    /* No more matching; don't show the line again */
		break;
	    }
	}

	/* If it's a match or a not-match (as required), do what's wanted. */
	if (match != invert) {
	    BOOL hyphenprinted = FALSE;

	    /* We've failed if we want a file that doesn't have any matches. */
	    if (filenames == FN_NOMATCH_ONLY) {
		rc = 1;
		goto exit;
	    }

	    /* Just count if just counting is wanted. */
	    if (count_only) count++;

	    /*
	     * If all we want is a file name, there is no need to scan any
	     * more lines in the file.
	     */
	    else if (filenames == FN_ONLY) {
		fprintf(stdout, "%s\n", printname);
		rc = 0;
		goto exit;
	    }

	    /* Likewise, if all we want is a yes/no answer. */
	    else if (quiet) {
		rc = 0;
		goto exit;
	    }

	    /*
	     * The --only-matching option prints just the substring that
	     * matched, and the --file-offsets and --line-offsets options
	     * output offsets for the matching substring (they both force
	     * --only-matching). None of these options prints any context.
	     * Afterwards, adjust the start and length, and then jump back
	     * to look for further matches in the same line. If we are in
	     * invert mode, however, nothing is printed - this could be
	     * still useful because the return code is set.
	     */
	    else if (only_matching) {
		if (!invert) {
		    if (printname != NULL) fprintf(stdout, "%s:", printname);
		    if (number) fprintf(stdout, "%d:", linenumber);
		    if (line_offsets)
			fprintf(stdout, "%d,%d", matchptr + offsets[0] - ptr,
			    offsets[1] - offsets[0]);
		    else if (file_offsets)
			fprintf(stdout, "%d,%d", filepos + matchptr + offsets[0] - ptr,
			    offsets[1] - offsets[0]);
		    else
			fwrite(matchptr + offsets[0], 1, offsets[1] - offsets[0], stdout);
		    fprintf(stdout, "\n");
		    matchptr += offsets[1];
		    length -= offsets[1];
		    match = FALSE;
		    goto ONLY_MATCHING_RESTART;
		}
	    }

	    /*
	     * This is the default case when none of the above options is set.
	     * We print the matching lines(s), possibly preceded and/or
	     * followed by other lines of context.
	     */
	    else {
		/*
		 * See if there is a requirement to print some "after" lines
		 * from a previous match. We never print any overlaps.
		 */
		if (after_context > 0 && lastmatchnumber > 0) {
		    int ellength;
		    int linecount = 0;
		    const char *p = lastmatchrestart;

		    while (p < ptr && linecount < after_context) {
			p = end_of_line(p, ptr, &ellength);
			linecount++;
		    }

		    /*
		     * It is important to advance lastmatchrestart during this
		     * printing so that it interacts correctly with any
		     * "before" printing below. Print each line's data using
		     * fwrite() in case there are binary zeroes.
		     */
		    while (lastmatchrestart < p) {
			const char *pp = lastmatchrestart;
			if (printname != NULL) fprintf(stdout, "%s-", printname);
			if (number) fprintf(stdout, "%d-", lastmatchnumber++);
			pp = end_of_line(pp, endptr, &ellength);
			fwrite(lastmatchrestart, 1, pp - lastmatchrestart, stdout);
			lastmatchrestart = pp;
		    }
		    if (lastmatchrestart != ptr) hyphenpending = TRUE;
		}

		/* If there were non-contiguous lines printed above, insert hyphens. */
		if (hyphenpending) {
		    fprintf(stdout, "--\n");
		    hyphenpending = FALSE;
		    hyphenprinted = TRUE;
		}

		/*
		 * See if there is a requirement to print some "before" lines
		 * for this match. Again, don't print overlaps.
		 */
		if (before_context > 0) {
		    int linecount = 0;
		    const char *p = ptr;

		    while (p > buffer && (lastmatchnumber == 0 || p > lastmatchrestart) &&
			       linecount < before_context)
		    {
			linecount++;
			p = previous_line(p, buffer);
		    }

		    if (lastmatchnumber > 0 && p > lastmatchrestart && !hyphenprinted)
			fprintf(stdout, "--\n");

		    while (p < ptr) {
			int ellength;
			const char *pp = p;
			if (printname != NULL) fprintf(stdout, "%s-", printname);
			if (number) fprintf(stdout, "%d-", linenumber - linecount--);
			pp = end_of_line(pp, endptr, &ellength);
			fwrite(p, 1, pp - p, stdout);
			p = pp;
		    }
		}

		/*
		 * Now print the matching line(s); ensure we set hyphenpending
		 * at the end of the file if any context lines are being output.
		 */
		if (after_context > 0 || before_context > 0)
		    endhyphenpending = TRUE;

		if (printname != NULL) fprintf(stdout, "%s:", printname);
		if (number) fprintf(stdout, "%d:", linenumber);

		/*
		 * In multiline mode, we want to print to the end of the line
		 * in which the end of the matched string is found, so we
		 * adjust linelength and the line number appropriately, but
		 * only when there actually was a match (invert not set).
		 * Because the PCRE_FIRSTLINE option is set, the start of
		 * the match will always be before the first newline sequence.
		 * */
		if (multiline) {
		    int ellength;
		    const char *endmatch = ptr;
		    if (!invert) {
			endmatch += offsets[1];
			t = ptr;
			while (t < endmatch) {
			    t = end_of_line(t, endptr, &ellength);
			    if (t <= endmatch) linenumber++; else break;
			}
		    }
		    endmatch = end_of_line(endmatch, endptr, &ellength);
		    linelength = endmatch - ptr - ellength;
		}
		/*
		 * NOTE: Use only fwrite() to output the data line, so that
		 * binary zeroes are treated as just another data character.
		 */

		/*
		 * This extra option, for Jeffrey Friedl's debugging
		 * requirements, replaces the matched string, or a specific
		 * captured string if it exists, with X. When this happens,
		 * colouring is ignored.
		 */
#ifdef JFRIEDL_DEBUG
		if (S_arg >= 0 && S_arg < mrc) {
		    int first = S_arg * 2;
		    int last  = first + 1;
		    fwrite(ptr, 1, offsets[first], stdout);
		    fprintf(stdout, "X");
		    fwrite(ptr + offsets[last], 1, linelength - offsets[last], stdout);
		} else
#endif

		/* We have to split the line(s) up if colouring. */
		if (do_colour) {
		    fwrite(ptr, 1, offsets[0], stdout);
		    fprintf(stdout, "%c[%sm", 0x1b, colour_string);
		    fwrite(ptr + offsets[0], 1, offsets[1] - offsets[0], stdout);
		    fprintf(stdout, "%c[00m", 0x1b);
		    fwrite(ptr + offsets[1], 1, (linelength + endlinelength) - offsets[1],
			stdout);
		}
		else fwrite(ptr, 1, linelength + endlinelength, stdout);
	    }

	    /* End of doing what has to be done for a match */
	    rc = 0;    /* Had some success */

	    /*
	     * Remember where the last match happened for after_context.
	     * We remember where we are about to restart, and that line's
	     * number.
	     */
	    lastmatchrestart = ptr + linelength + endlinelength;
	    lastmatchnumber = linenumber + 1;
	}

	/*
	 * For a match in multiline inverted mode (which of course did not
	 * cause anything to be printed), we have to move on to the end of
	 * the match before proceeding.
	 */
	if (multiline && invert && match) {
	    int ellength;
	    const char *endmatch = ptr + offsets[1];
	    t = ptr;
	    while (t < endmatch) {
		t = end_of_line(t, endptr, &ellength);
		if (t <= endmatch) linenumber++; else break;
	    }
	    endmatch = end_of_line(endmatch, endptr, &ellength);
	    linelength = endmatch - ptr - ellength;
	}

	/*
	 * Advance to after the newline and increment the line number. The
	 * file offset to the current line is maintained in filepos.
	 */
	ptr += linelength + endlinelength;
	filepos += linelength + endlinelength;
	linenumber++;

	/*
	 * If we haven't yet reached the end of the file (the buffer is full),
	 * and the current point is in the top 1/3 of the buffer, slide the
	 * buffer down by 1/3 and refill it. Before we do this, if some
	 * unprinted "after" lines are about to be lost, print them.
	 */
	if (bufflength >= sizeof(buffer) && ptr > buffer + 2*MBUFTHIRD) {
	    if (after_context > 0 &&
		lastmatchnumber > 0 &&
		lastmatchrestart < buffer + MBUFTHIRD)
	    {
		do_after_lines(lastmatchnumber, lastmatchrestart, endptr, printname);
		lastmatchnumber = 0;
	    }

	    /* Now do the shuffle */

	    memmove(buffer, buffer + MBUFTHIRD, 2*MBUFTHIRD);
	    ptr -= MBUFTHIRD;

#ifdef SUPPORT_LIBZ
	    if (frtype == FR_LIBZ)
		bufflength = 2*MBUFTHIRD +
		gzread (ingz, buffer + 2*MBUFTHIRD, MBUFTHIRD);
	    else
#endif

#ifdef SUPPORT_LIBBZ2
	    if (frtype == FR_LIBBZ2)
		bufflength = 2*MBUFTHIRD +
		BZ2_bzread(inbz2, buffer + 2*MBUFTHIRD, MBUFTHIRD);
	    else
#endif

	    bufflength = 2*MBUFTHIRD + fread(buffer + 2*MBUFTHIRD, 1, MBUFTHIRD, in);

	    endptr = buffer + bufflength;

	    /* Adjust any last match point */
	    if (lastmatchnumber > 0) lastmatchrestart -= MBUFTHIRD;
	}
    }	/* Loop through the whole file */

    /*
     * End of file; print final "after" lines if wanted; do_after_lines sets
     * hyphenpending if it prints something.
     */
    if (!only_matching && !count_only) {
	do_after_lines(lastmatchnumber, lastmatchrestart, endptr, printname);
	hyphenpending |= endhyphenpending;
    }

    /*
     * Print the file name if we are looking for those without matches and
     * there were none. If we found a match, we won't have got this far.
     */
    if (filenames == FN_NOMATCH_ONLY) {
	fprintf(stdout, "%s\n", printname);
	rc = 0;
	goto exit;
    }

    /* Print the match count if wanted */
    if (count_only) {
	if (printname != NULL) fprintf(stdout, "%s:", printname);
	fprintf(stdout, "%d\n", count);
    }

exit:
    return rc;
}

/*************************************************
 * Grep a file or recurse into a directory.
 *
 * Given a path name, if it's a directory, scan all the files if we are
 * recursing; if it's a file, grep it.
 *
 * @param pathname		the path to investigate
 * @param dir_recurse		TRUE if recursing is wanted (-r or -drecurse)
 * @param only_one_at_top	TRUE if the path is the only one at toplevel
 * @return		0: at least one match, 1: no match, 2: read error (bz2)
 *
 * @note file opening failures are suppressed if "silent" is set.
 */
static int
grep_or_recurse(const char *pathname, BOOL dir_recurse, BOOL only_one_at_top)
{
    int rc = 1;
    int sep;
    int frtype;
    int pathlen;
    void *handle;
    FILE * infp = NULL;	/* Ensure initialized */

#ifdef SUPPORT_LIBZ
    gzFile ingz = NULL;
#endif

#ifdef SUPPORT_LIBBZ2
    BZFILE *inbz2 = NULL;
#endif

    /* If the file name is "-" we scan stdin */
    if (strcmp(pathname, "-") == 0) {
	rc = pcregrep(stdin, FR_PLAIN,
	    (filenames > FN_DEFAULT || (filenames == FN_DEFAULT && !only_one_at_top))?
		stdin_name : NULL);
	goto exit;
    }

    /*
     * If the file is a directory, skip if skipping or if we are recursing,
     * scan each file within it, subject to any include or exclude patterns
     * that were set.  The scanning code is localized so it can be made
     * system-specific.
     */
    if ((sep = isdirectory(pathname)) != 0) {
	if (dee_action == dee_SKIP) {
	    rc = 1;
	    goto exit;
	}
	if (dee_action == dee_RECURSE) {
	    char buffer[1024];
	    char *nextfile;
	    directory_type *dir = opendirectory(pathname);

	    if (dir == NULL) {
		if (!silent)
		    fprintf(stderr, _("%s: Failed to open directory %s: %s\n"),
			__progname, pathname, strerror(errno));
		rc = 2;
		goto exit;
	    }

	    while ((nextfile = readdirectory(dir)) != NULL) {
		int frc;
		sprintf(buffer, "%.512s%c%.128s", pathname, sep, nextfile);

		if (excludeMire && mireRegexec(excludeMire, buffer, 0) >= 0)
		    continue;

		if (includeMire && mireRegexec(includeMire, buffer, 0) < 0)
		    continue;

		frc = grep_or_recurse(buffer, dir_recurse, FALSE);
		if (frc > 1) rc = frc;
		else if (frc == 0 && rc == 1) rc = 0;
	    }

	    closedirectory(dir);
	    goto exit;
	}
    }

    /*
     * If the file is not a directory and not a regular file, skip it if
     * that's been requested.
     */
    else if (!isregfile(pathname) && DEE_action == DEE_SKIP) {
	rc = 1;
	goto exit;
    }

    /*
     * Control reaches here if we have a regular file, or if we have a
     * directory and recursion or skipping was not requested, or if we have
     * anything else and skipping was not requested. The scan proceeds.
     * If this is the first and only argument at top level, we don't show
     * the file name, unless we are only showing the file name, or the
     * filename was forced (-H).
     */
    pathlen = strlen(pathname);

    /* Open using zlib if it is supported and the file name ends with .gz. */
#ifdef SUPPORT_LIBZ
    if (pathlen > 3 && strcmp(pathname + pathlen - 3, ".gz") == 0) {
	ingz = gzopen(pathname, "rb");
	if (ingz == NULL) {
	    if (!silent)
		fprintf(stderr, _("%s: Failed to open %s: %s\n"),
			__progname, pathname, strerror(errno));
	    rc = 2;
	    goto exit;
	}
	handle = (void *)ingz;
	frtype = FR_LIBZ;
    } else
#endif

    /* Otherwise open with bz2lib if it is supported and the name ends with .bz2. */
#ifdef SUPPORT_LIBBZ2
    if (pathlen > 4 && strcmp(pathname + pathlen - 4, ".bz2") == 0) {
	inbz2 = BZ2_bzopen(pathname, "rb");
	handle = (void *)inbz2;
	frtype = FR_LIBBZ2;
    } else
#endif

    /*
     * Otherwise use plain fopen(). The label is so that we can come back
     * here if an attempt to read a .bz2 file indicates that it really is
     * a plain file.
     */
#ifdef SUPPORT_LIBBZ2
PLAIN_FILE:
#endif
    {
	infp = fopen(pathname, "r");
	handle = (void *)infp;
	frtype = FR_PLAIN;
    }

    /* All the opening methods return errno when they fail. */
    if (handle == NULL) {
	if (!silent)
	    fprintf(stderr, _("%s: Failed to open %s: %s\n"),
		__progname, pathname, strerror(errno));
	rc = 2;
	goto exit;
    }

    /* Now grep the file */
    rc = pcregrep(handle, frtype, (filenames > FN_DEFAULT ||
	(filenames == FN_DEFAULT && !only_one_at_top))? pathname : NULL);

    /* Close in an appropriate manner. */
#ifdef SUPPORT_LIBZ
    if (frtype == FR_LIBZ)
	gzclose(ingz);
    else
#endif

    /*
     * If it is a .bz2 file and the result is 2, it means that the first
     * attempt to read failed. If the error indicates that the file isn't
     * in fact bzipped, try again as a normal file.
     */
#ifdef SUPPORT_LIBBZ2
    if (frtype == FR_LIBBZ2) {
	if (rc == 2) {
	    int errnum;
	    const char *err = BZ2_bzerror(inbz2, &errnum);
	    if (errnum == BZ_DATA_ERROR_MAGIC) {
		BZ2_bzclose(inbz2);
		goto PLAIN_FILE;
	    }
	    else if (!silent)
		fprintf(stderr, _("%s: Failed to read %s using bzlib: %s\n"),
		    __progname, pathname, err);
	}
	BZ2_bzclose(inbz2);
    } else
#endif

    if (infp)
	fclose(infp);	/* Normal file close */

exit:
    return rc;	/* Pass back the yield from pcregrep(). */
}

/** Structure for options and list of them */
enum {
    OP_NODATA	= POPT_ARG_NONE,
    OP_STRING	= POPT_ARG_STRING,
    OP_OP_STRING	= POPT_ARG_STRING + 100,
    OP_NUMBER	= POPT_ARG_INT,
    OP_OP_NUMBER	= POPT_ARG_INT + 100,
    OP_PATLIST		= POPT_ARG_NONE + 100
};

/* Options without a single-letter equivalent get a negative value. This can be
used to identify them. */
#define N_COLOUR    (-1)
#define N_EXCLUDE   (-2)
#define N_HELP      (-3)
#define N_INCLUDE   (-4)
#define N_LABEL     (-5)
#define N_LOCALE    (-6)
#define N_LOFFSETS  (-8)
#define N_FOFFSETS  (-9)

/* XXX forward ref. */
static void rpmgrepArgCallback(poptContext con,
                /*@unused@*/ enum poptCallbackReason reason,
                const struct poptOption * opt, const char * arg,
                /*@unused@*/ void * data);

static struct poptOption optionsTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmgrepArgCallback, 0, NULL, NULL },
/*@=type@*/

  { "", '\0',	POPT_ARG_NONE,	NULL, 0,
	N_("terminate options"), NULL },
  { "help", N_HELP,	POPT_ARG_NONE,	NULL, N_HELP,
	N_("display this help and exit"), NULL },
  { "after-context", 'A',	POPT_ARG_INT,	&after_context, 'A',
	N_("set number of following context lines"), N_("=number") },
  { "before-context", 'B',	POPT_ARG_INT,	&before_context, 'B',
	N_("set number of prior context lines"), N_("=number") },
  { "color", N_COLOUR,	OP_OP_STRING,	&colour_option, N_COLOUR,
	N_("matched text color option"), N_("option") },
  { "context", 'C',	POPT_ARG_INT,	&both_context, 'C',
	N_("set number of context lines, before & after"), N_("=number") },
  { "count", 'c',	POPT_ARG_NONE,		NULL, 'c',
	N_("print only a count of matching lines per FILE"), NULL },
  { "colour", N_COLOUR,	OP_OP_STRING,	&colour_option, N_COLOUR,
	N_("matched text colour option"), N_("=option") },
  { "devices", 'D',	POPT_ARG_STRING,	&DEE_option, 0,
	N_("how to handle devices, FIFOs, and sockets"), N_("=action") },
  { "directories", 'd',	POPT_ARG_STRING,	&dee_option, 0,
	N_("how to handle directories"), N_("=action") },
  { "regex", 'e',	OP_PATLIST,		NULL, 'e',
	N_("specify pattern (may be used more than once)"), N_("(p)") },
  { "fixed_strings", 'F',	POPT_ARG_NONE,	NULL, 'F',
	N_("patterns are sets of newline-separated strings"), NULL },
  { "file", 'f',	POPT_ARG_STRING,	&pattern_filename, 'f',
	N_("read patterns from file"), N_("=path") },
  { "file-offsets", N_FOFFSETS,	POPT_ARG_NONE,	NULL, N_FOFFSETS,
	N_("output file offsets, not text"), NULL },
  { "with-filename", 'H',	POPT_ARG_NONE,	NULL, 'H',
	N_("force the prefixing filename on output"), NULL },
  { "no-filename", 'h',	POPT_ARG_NONE,		NULL, 'h',
	N_("suppress the prefixing filename on output"), NULL },
  { "ignore-case", 'i',	POPT_ARG_NONE,		NULL, 'i',
	N_("ignore case distinctions"), NULL },
  { "files-with-matches", 'l',	POPT_ARG_NONE,	NULL, 'l',
	N_("print only FILE names containing matches"), NULL },
  { "files-without-match", 'L',	POPT_ARG_NONE,	NULL, 'L',
	N_("print only FILE names not containing matches"), NULL },
  { "label", N_LABEL,	POPT_ARG_STRING,	&stdin_name, N_LABEL,
	N_("set name for standard input"), N_("=name") },
  { "line-offsets", N_LOFFSETS,	POPT_ARG_NONE,	NULL, N_LOFFSETS,
	N_("output line numbers and offsets, not text"), NULL },
  { "locale", N_LOCALE,	POPT_ARG_STRING,	&locale, N_LOCALE,
	N_("use the named locale"), N_("=locale") },
  { "multiline", 'M',	POPT_ARG_NONE,		NULL, 'M',
	N_("run in multiline mode"), NULL },
  { "newline", 'N',	POPT_ARG_STRING,	&newline, 0,
	N_("set newline type (CR, LF, CRLF, ANYCRLF or ANY)"), N_("=type") },
  { "line-number", 'n',	POPT_ARG_NONE,		NULL, 'n',
	N_("print line number with output lines"), NULL },
  { "only-matching", 'o',	POPT_ARG_NONE,	NULL, 'o',
	N_("show only the part of the line that matched"), NULL },
  { "quiet", 'q',	POPT_ARG_NONE,		NULL, 'q',
	N_("suppress output, just set return code"), NULL },
  { "recursive", 'r',	POPT_ARG_NONE,		NULL, 'r',
	N_("recursively scan sub-directories"), NULL },
  { "exclude", N_EXCLUDE,	POPT_ARG_STRING,	&exclude_pattern, N_EXCLUDE,
	N_("exclude matching files when recursing"), N_("=pattern") },
  { "include", N_INCLUDE,	POPT_ARG_STRING,	&include_pattern, N_INCLUDE,
	N_("include matching files when recursing"), N_("=pattern") },
#ifdef JFRIEDL_DEBUG
  { "jeffS", 'S',	OP_OP_NUMBER,	&S_arg, 0,
	N_("replace matched (sub)string with X"), NULL },
#endif
  { "no-messages", 's',	POPT_ARG_NONE,		NULL, 's',
	N_("suppress error messages"), NULL },
  { "utf-8", 'u',	POPT_ARG_NONE,		NULL, 'u',
	N_("use UTF-8 mode"), NULL },
  { "version", 'V',	POPT_ARG_NONE,		NULL, 'V',
	N_("print version information and exit"), NULL },
  { "invert-match", 'v',	POPT_ARG_NONE,	NULL, 'v',
	N_("select non-matching lines"), NULL },
  { "word-regex", 'w',	POPT_ARG_NONE,		NULL, 'w',
	N_("force patterns to match only as words") , N_("(p)") },
  { "line-regex", 'x',	POPT_ARG_NONE,		NULL, 'x',
	N_("force patterns to match only whole lines"), N_("(p)") },

  POPT_AUTOALIAS
  POPT_AUTOHELP

  { NULL, -1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
	N_("\
Usage: rpmgrep [OPTION...] [PATTERN] [FILE1 FILE2 ...]\n\n\
  Search for PATTERN in each FILE or standard input.\n\
  PATTERN must be present if neither -e nor -f is used.\n\
  \"-\" can be used as a file name to mean STDIN.\n\
  All files are read as plain files, without any interpretation.\n\n\
Example: rpmgrep -i 'hello.*world' menu.h main.c\
") , NULL },

  { NULL, -1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
	N_("\
  When reading patterns from a file instead of using a command line option,\n\
  trailing white space is removed and blank lines are ignored.\n\
\n\
  With no FILEs, read standard input. If fewer than two FILEs given, assume -h.\n\
") , NULL },

  POPT_TABLEEND
};

/*************************************************
 * Usage function.
 * @param rc			return code
 * @return			return code
 */
static int
usage(poptContext optCon, int rc)
{
#ifdef	DYING
    struct poptOption *opt = optionsTable;
    fprintf(stderr, _("Usage: %s [-"), __progname);
    for (; (opt->longName || opt->shortName || opt->arg); opt++) {
      if (opt->shortName > 0) fprintf(stderr, "%c", opt->shortName);
    }
    fprintf(stderr, _("] [long options] [pattern] [files]\n"));
    fprintf(stderr,
	_("Type `%s --help' for more information and the long options.\n"),
	__progname);
#else
    poptPrintUsage(optCon, stdout, 0);
#endif
    return rc;
}

/*************************************************
 * Help function.
 */
static void
help(poptContext optCon)
{
#ifdef	DYING
    struct poptOption *opt = optionsTable;

    printf(_("Usage: %s [OPTION]... [PATTERN] [FILE1 FILE2 ...]\n"),
	__progname);
    printf(_("Search for PATTERN in each FILE or standard input.\n"));
    printf(_("PATTERN must be present if neither -e nor -f is used.\n"));
    printf(_("\"-\" can be used as a file name to mean STDIN.\n"));

#ifdef SUPPORT_LIBZ
    printf(_("Files whose names end in .gz are read using zlib.\n"));
#endif

#ifdef SUPPORT_LIBBZ2
    printf(_("Files whose names end in .bz2 are read using bzlib2.\n"));
#endif

#if defined SUPPORT_LIBZ || defined SUPPORT_LIBBZ2
    printf(_("Other files and the standard input are read as plain files.\n\n"));
#else
    printf(_("All files are read as plain files, without any interpretation.\n\n"));
#endif

    printf(_("Example: %s -i 'hello.*world' menu.h main.c\n\n"), __progname);
    printf(_("Options:\n"));

    for (; (opt->longName || opt->shortName || opt->arg); opt++) {
	int n;
	char s[4];
	if ((opt->argInfo & POPT_ARG_MASK) == POPT_ARG_INCLUDE_TABLE)
	    continue;
	if (opt->shortName > 0) sprintf(s, "-%c,", opt->shortName);
	else strcpy(s, "   ");
	n = 30;
	n -= printf("  %s --%s", s, opt->longName);
	if (opt->argDescrip && (strchr("=(", opt->argDescrip[0]) != NULL))
	    n -= printf("%s", opt->argDescrip);

	if (n < 1) n = 1;
	printf("%.*s%s\n", n, "                    ", opt->descrip);
    }

    printf(_("\nWhen reading patterns from a file instead of using a command line option,\n"));
    printf(_("trailing white space is removed and blank lines are ignored.\n"));
    printf(_("There is a maximum of %d patterns.\n"), MAX_PATTERN_COUNT);

    printf(_("\nWith no FILEs, read standard input. If fewer than two FILEs given, assume -h.\n"));
    printf(_("Exit status is 0 if any matches, 1 if no matches, and 2 if trouble.\n"));
#else
    poptPrintHelp(optCon, stdout, 0);
#endif
}

typedef union rpmgrepArg_u {
    void * ptr;
    int * intp;
    long * longp;
    const char ** argv;
} rpmgrepArg;

/**
 */
static void rpmgrepArgCallback(poptContext con,
                /*@unused@*/ enum poptCallbackReason reason,
                const struct poptOption * opt, const char * arg,
                /*@unused@*/ void * data)
{
    rpmgrepArg u = { .ptr = opt->arg };
    int xx;

#if 0
    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
#endif
    switch (opt->val) {
    case N_FOFFSETS: file_offsets = TRUE; break;
    case N_LOFFSETS: line_offsets = number = TRUE; break;
    case 'c': count_only = TRUE; break;
    case 'F': process_options |= PO_FIXED_STRINGS; break;
    case 'H': filenames = FN_FORCE; break;
    case 'h': filenames = FN_NONE; break;
    case 'i': global_options |= PCRE_CASELESS; break;
    case 'l': filenames = FN_ONLY; break;
    case 'L': filenames = FN_NOMATCH_ONLY; break;
    case 'M':
	multiline = TRUE;
	global_options |= PCRE_MULTILINE|PCRE_FIRSTLINE;
	break;
    case 'n': number = TRUE; break;
    case 'o': only_matching = TRUE; break;
    case 'q': quiet = TRUE; break;
    case 'r': dee_action = dee_RECURSE; break;
    case 's': silent = TRUE; break;
    case 'u': global_options |= PCRE_UTF8; utf8 = TRUE; break;
    case 'v': invert = TRUE; break;
    case 'w': process_options |= PO_WORD_MATCH; break;
    case 'x': process_options |= PO_LINE_MATCH; break;

    case 'f': u.argv[0] = arg; break;
    case 'A': u.intp[0] = strtol(arg, NULL, 0); break;
    case 'B': u.intp[0] = strtol(arg, NULL, 0); break;
    case 'C': u.intp[0] = strtol(arg, NULL, 0); break;
    case N_INCLUDE:
	include_pattern = arg;
	break;
    case N_EXCLUDE:
	exclude_pattern = arg;
	break;

    case 'e':
	xx = poptSaveString(&patterns, opt->argInfo, arg);
	break;

    case 'V':
	fprintf(stderr, _("%s version %s\n"), __progname, pcre_version());
	exit(0);
	/*@notreached@*/ break;
    case N_HELP:
	help(con);
	exit(0);
	/*@notreached@*/ break;
    default:
	fprintf(stderr, _("%s: Unknown option -%c\n"), __progname, opt->val);
	exit(usage(con, 2));
	/*@notreached@*/ break;
    }
}

/*************************************************
 * Construct printed ordinal.
 *
 * This turns a number into "1st", "3rd", etc display string.
 * @param n		the number
 * @return		the number's ordinal display string
 */
static char *
ordin(unsigned n)
{
    static char buffer[16];
    buffer[0] = '\0';
    if (n > 0) {
	char *p = buffer;
	sprintf(p, " %u", n);
	while (*p != '\0') p++;
	switch (n%10) {
	case 1:		strcpy(p, "st"); break;
	case 2:		strcpy(p, "nd"); break;
	case 3:		strcpy(p, "rd"); break;
	default:	strcpy(p, "th"); break;
	}
    }
    return buffer;
}

/*************************************************
 * Compile a single pattern.
 *
 * When the -F option has been used, this is called for each substring.
 * Otherwise it's called for each supplied pattern.
 *
 * @param pattern	the pattern string
 * @param options	the PCRE options
 * @param filename	the file name (NULL for a command-line pattern)
 * @param count		pattern index (0 is single pattern)
 * @return		TRUE on success, FALSE after an error
 */
static BOOL
compile_single_pattern(const char *pattern, int options,
		/*@null@*/ const char *filename, int count)
{
    miRE mire;
    char buffer[MBUFTHIRD + 16];

    if (pattern_count >= MAX_PATTERN_COUNT) {
	fprintf(stderr, _("%s: Too many %spatterns (max %d)\n"), __progname,
		(filename == NULL)? "command-line " : "", MAX_PATTERN_COUNT);
	return FALSE;
    }
    mire = pattern_list + pattern_count;

    sprintf(buffer, "%s%.*s%s", prefix[process_options], MBUFTHIRD, pattern,
	suffix[process_options]);
    /* XXX initialize mire->mode & mire->tag. */
    mire->mode = RPMMIRE_PCRE;
    mire->tag = 0;
    mire->coptions = options;
    /* XXX save locale tables for use by pcre_compile2. */
    mire->table = pcretables;
    if (!mireRegcomp(mire, buffer)) {
	pattern_count++;
	return TRUE;
    }
    /* Handle compile errors */
    mire->erroff -= (int)strlen(prefix[process_options]);
    if (mire->erroff < 0)
	mire->erroff = 0;
    if (mire->erroff > (int)strlen(pattern))
	mire->erroff = (int)strlen(pattern);

    fprintf(stderr, _("%s: Error in"), __progname);
    if (filename == NULL)
	fprintf(stderr, _("%s command-line"), ordin(count));
    else
	fprintf(stderr, _(" file:line %s:%d"), filename, count);
    fprintf(stderr, _(" regex at offset %d: %s\n"), mire->erroff, mire->errmsg);
    return FALSE;
}

/*************************************************
 * Compile one supplied pattern.
 *
 * When the -F option has been used, each string may be a list of strings,
 * separated by line breaks. They will be matched literally.
 *
 * @param pattern	the pattern string
 * @param options	the PCRE options
 * @param filename	the file name, or NULL for a command-line pattern
 * @param count		pattern index (0 is single pattern)
 * @return		TRUE on success, FALSE after an error
 */
static BOOL
compile_pattern(const char *pattern, int options,
		/*@null@*/ const char *filename, int count)
{
    if ((process_options & PO_FIXED_STRINGS) != 0) {
	const char *eop = pattern + strlen(pattern);
	char buffer[MBUFTHIRD];
	for(;;) {
	    int ellength;
	    const char *p = end_of_line(pattern, eop, &ellength);
	    if (ellength == 0)
		return compile_single_pattern(pattern, options, filename, count);
	    sprintf(buffer, "%.*s", (int)(p - pattern - ellength), pattern);
	    pattern = p;
	    if (!compile_single_pattern(buffer, options, filename, count))
		return FALSE;
	}
    }
    else return compile_single_pattern(pattern, options, filename, count);
}

/**
 * Destroy compiled patterns.
 * @param mire		pattern array
 * @param nre		no of patterns in array
 * @return		NULL always
 */
/*@-onlytrans@*/	/* XXX miRE array, not refcounted. */
/*@null@*/
static void * mireFreeAll(/*@only@*/ /*@null@*/ miRE mire, int nre)
	/*@modifies mire@*/
{
    if (mire != NULL) {
	int i;
	for (i = 0; i < nre; i++)
	    (void) mireClean(mire + i);
	mire = _free(mire);
    }
    return NULL;
}
/*@=onlytrans@*/

/**
 * Append pattern to array.
 * @param mode		type of pattern match
 * @param tag		identifier (like an rpmTag)
 * @param pattern	pattern to compile
 * @retval *mi_rep	platform pattern array
 * @retval *mi_nrep	no. of patterns in array
 */
/*@-onlytrans@*/	/* XXX miRE array, not refcounted. */
/*@null@*/
static int mireAppend(rpmMireMode mode, int tag, const char * pattern,
		miRE * mi_rep, int * mi_nrep)
	/*@modifies *mi_rep, *mi_nrep @*/
{
    miRE mire;

    mire = (*mi_rep);
/*@-refcounttrans@*/
    mire = xrealloc(mire, ((*mi_nrep) + 1) * sizeof(*mire));
/*@=refcounttrans@*/
    (*mi_rep) = mire;
    mire += (*mi_nrep);
    (*mi_nrep)++;
    memset(mire, 0, sizeof(*mire));
    mire->mode = mode;
    mire->tag = tag;
    return mireRegcomp(mire, pattern);
}
/*@=onlytrans@*/

/*************************************************
 * Main program.
 * @return		0: match found, 1: no match, 2: error.
 */
int
main(int argc, char **argv)
{
    poptContext optCon;
    int i, j;
    int rc = 1;
    int pcre_options = 0;
    int errptr;
    BOOL only_one_at_top;
    ARGV_t av = NULL;
    int ac = 0;
    const char *locale_from = "--locale";
    const char *error;
    int xx;

#if defined(HAVE_MCHECK_H) && defined(HAVE_MTRACE)
    /*@-noeffect@*/
    mtrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
    /*@=noeffect@*/
#endif
/*@-globs -mods@*/
    setprogname(argv[0]);       /* Retrofit glibc __progname */

    /* XXX glibc churn sanity */
    if (__progname == NULL) {
	if ((__progname = strrchr(argv[0], '/')) != NULL) __progname++;
	else __progname = argv[0];
    }
/*@=globs =mods@*/

    __progname = "pcregrep";	/* XXX HACK in expected name. */

#if defined(ENABLE_NLS) && !defined(__LCLINT__)
    (void) setlocale(LC_ALL, "" );
    (void) bindtextdomain(PACKAGE, LOCALEDIR);
    (void) textdomain(PACKAGE);
#endif

    optCon = poptGetContext(__progname, argc, (const char **)argv, optionsTable, 0);

    /*
     * Set the default line ending value from the default in the PCRE library;
     * "lf", "cr", "crlf", and "any" are supported. Anything else is treated
     * as "lf".
     */
    (void)pcre_config(PCRE_CONFIG_NEWLINE, &i);
    switch (i) {
    default:	newline = (char *)"lf";		break;
    case '\r':	newline = (char *)"cr";		break;
    case ('\r' << 8) | '\n': newline = (char *)"crlf"; break;
    case -1:	newline = (char *)"any";	break;
    case -2:	newline = (char *)"anycrlf";	break;
    }

    /* Process all options, whine if unknown. */
    while ((rc = poptGetNextOpt(optCon)) > 0) {
	const char * optArg = poptGetOptArg(optCon);
	optArg = _free(optArg);
	switch (rc) {
	default:
/*@-nullpass@*/
	    fprintf(stderr, _("%s: option table misconfigured (%d)\n"),
		__progname, rc);
/*@=nullpass@*/
	    exit(EXIT_FAILURE);

	    /*@notreached@*/ /*@switchbreak@*/ break;
        }
    }

    if (rc < -1) {
/*@-nullpass@*/
	fprintf(stderr, "%s: %s: %s\n", __progname,
		poptBadOption(optCon, POPT_BADOPTION_NOALIAS),
		poptStrerror(rc));
/*@=nullpass@*/
	exit(EXIT_FAILURE);
    }
    pcre_options = global_options;
    av = poptGetArgs(optCon);
    ac = argvCount(av);
    i = 0;
    rc = 1;

    /*
     * Options have been decoded. If -C was used, its value is used as a default
     * for -A and -B.
     */
    if (both_context > 0) {
	if (after_context == 0) after_context = both_context;
	if (before_context == 0) before_context = both_context;
    }

    /*
     * Only one of --only-matching, --file-offsets, or --line-offsets is
     * permitted.  However, the latter two set the only_matching flag.
     */
    if ((only_matching && (file_offsets || line_offsets)) ||
	    (file_offsets && line_offsets))
    {
	fprintf(stderr,
_("%s: Cannot mix --only-matching, --file-offsets and/or --line-offsets\n"),
		__progname);
	exit(usage(optCon, 2));
    }

    if (file_offsets || line_offsets) only_matching = TRUE;

    /*
     * If a locale has not been provided as an option, see if the LC_CTYPE or
     * LC_ALL environment variable is set, and if so, use it.
     */
    if (locale == NULL) {
	locale = getenv("LC_ALL");
	locale_from = "LCC_ALL";
    }

    if (locale == NULL) {
	locale = getenv("LC_CTYPE");
	locale_from = "LC_CTYPE";
    }

    /*
     * If a locale has been provided, set it, and generate the tables the PCRE
     * needs. Otherwise, pcretables==NULL, which causes the use of default
     * tables.
     */
    if (locale != NULL) {
	if (setlocale(LC_CTYPE, locale) == NULL) {
	    fprintf(stderr, _("%s: Failed to set locale %s (obtained from %s)\n"),
		__progname, locale, locale_from);
	    return 2;
	}
	pcretables = pcre_maketables();
    }

    /* Sort out colouring */
    if (colour_option != NULL && strcmp(colour_option, "never") != 0) {
	if (strcmp(colour_option, "always") == 0)
	    do_colour = TRUE;
	else if (strcmp(colour_option, "auto") == 0)
	    do_colour = is_stdout_tty();
	else {
	    fprintf(stderr, _("%s: Unknown colour setting \"%s\"\n"),
		__progname, colour_option);
	    return 2;
	}
	if (do_colour) {
	    char *cs = getenv("PCREGREP_COLOUR");
	    if (cs == NULL) cs = getenv("PCREGREP_COLOR");
	    if (cs != NULL) colour_string = cs;
	}
    }

    /* Interpret the newline type; the default settings are Unix-like. */
    if (strcmp(newline, "cr") == 0 || strcmp(newline, "CR") == 0) {
	pcre_options |= PCRE_NEWLINE_CR;
	endlinetype = EL_CR;
    }
    else if (strcmp(newline, "lf") == 0 || strcmp(newline, "LF") == 0) {
	pcre_options |= PCRE_NEWLINE_LF;
	endlinetype = EL_LF;
    }
    else if (strcmp(newline, "crlf") == 0 || strcmp(newline, "CRLF") == 0) {
	pcre_options |= PCRE_NEWLINE_CRLF;
	endlinetype = EL_CRLF;
    }
    else if (strcmp(newline, "any") == 0 || strcmp(newline, "ANY") == 0) {
	pcre_options |= PCRE_NEWLINE_ANY;
	endlinetype = EL_ANY;
    }
    else if (strcmp(newline, "anycrlf") == 0 || strcmp(newline, "ANYCRLF") == 0) {
	pcre_options |= PCRE_NEWLINE_ANYCRLF;
	endlinetype = EL_ANYCRLF;
    }
    else {
	fprintf(stderr, _("%s: Invalid newline specifier \"%s\"\n"),
		__progname, newline);
	return 2;
    }

    /* Interpret the text values for -d and -D */
    if (dee_option != NULL) {
	if (strcmp(dee_option, "read") == 0) dee_action = dee_READ;
	else if (strcmp(dee_option, "recurse") == 0) dee_action = dee_RECURSE;
	else if (strcmp(dee_option, "skip") == 0) dee_action = dee_SKIP;
	else {
	    fprintf(stderr, _("%s: Invalid value \"%s\" for -d\n"),
		__progname, dee_option);
	    return 2;
	}
    }

    if (DEE_option != NULL) {
	if (strcmp(DEE_option, "read") == 0) DEE_action = DEE_READ;
	else if (strcmp(DEE_option, "skip") == 0) DEE_action = DEE_SKIP;
	else {
	    fprintf(stderr, _("%s: Invalid value \"%s\" for -D\n"),
		__progname, DEE_option);
	    return 2;
	}
    }

    /* Check the values for Jeffrey Friedl's debugging options. */
#ifdef JFRIEDL_DEBUG
    if (S_arg > 9) {
	fprintf(stderr, _("%s: bad value for -S option\n"), __progname);
	return 2;
    }
    if (jfriedl_XT != 0 || jfriedl_XR != 0) {
	if (jfriedl_XT == 0) jfriedl_XT = 1;
	if (jfriedl_XR == 0) jfriedl_XR = 1;
    }
#endif

    /* Get memory to store the pattern and hints lists. */
    pattern_list = xcalloc(MAX_PATTERN_COUNT, sizeof(*pattern_list));

    /*
     * If no patterns were provided by -e, and there is no file provided by -f,
     * the first argument is the one and only pattern, and it must exist.
     */
    npatterns = argvCount(patterns);
    if (npatterns == 0 && pattern_filename == NULL) {
	if (i >= ac) return usage(optCon, 2);
	xx = poptSaveString(&patterns, POPT_ARG_ARGV, av[i]);
	i++;
    }

    /*
     * Compile the patterns that were provided on the command line, either by
     * multiple uses of -e or as a single unkeyed pattern.
     */
    npatterns = argvCount(patterns);
    for (j = 0; j < npatterns; j++) {
	if (!compile_pattern(patterns[j], pcre_options, NULL,
		 (j == 0 && npatterns == 1)? 0 : j + 1))
	    goto errorexit;
    }

    /* Compile the regular expressions that are provided in a file. */
    if (pattern_filename != NULL) {
	int linenumber = 0;
	FILE *fp;
	char *fn;
	char buffer[MBUFTHIRD];

	if (strcmp(pattern_filename, "-") == 0) {
	    fp = stdin;
	    fn = stdin_name;
	} else {
	    fp = fopen(pattern_filename, "r");
	    if (fp == NULL) {
		fprintf(stderr, _("%s: Failed to open %s: %s\n"),
			__progname, pattern_filename, strerror(errno));
		goto errorexit;
	    }
	    fn = pattern_filename;
	}

	while (fgets(buffer, MBUFTHIRD, fp) != NULL) {
	    char *se = buffer + (int)strlen(buffer);
	    while (se > buffer && xisspace((int)se[-1]))
		se--;
	    *se = 0;
	    linenumber++;
	    if (buffer[0] == 0)	continue;	/* Skip blank lines */
	    if (!compile_pattern(buffer, pcre_options, fn, linenumber))
		goto errorexit;
	}

	if (fp != stdin)
	    fclose(fp);
    }

    /* Study the regular expressions, as we will be running them many times */
    for (j = 0; j < pattern_count; j++) {
	miRE mire = pattern_list + j;
	mire->hints = pcre_study(mire->pcre, 0, &error);
	if (error != NULL) {
	    char s[16];
	    if (pattern_count == 1) s[0] = 0; else sprintf(s, " number %d", j);
	    fprintf(stderr, _("%s: Error while studying regex%s: %s\n"),
		__progname, s, error);
	    goto errorexit;
	}
    }

    /* If there are include or exclude patterns, compile them. */
    if (exclude_pattern != NULL) {
	excludeMire = mireNew(RPMMIRE_PCRE, 0);
	/* XXX save locale tables for use by pcre_compile2. */
	excludeMire->table = pcretables;
	xx = mireRegcomp(excludeMire, exclude_pattern);
	if (xx) {
	    fprintf(stderr, _("%s: Error in 'exclude' regex at offset %d: %s\n"),
		__progname, errptr, error);
	    goto errorexit;
	}
    }

    if (include_pattern != NULL) {
	includeMire = mireNew(RPMMIRE_PCRE, 0);
	/* XXX save locale tables for use by pcre_compile2. */
	includeMire->table = pcretables;
	xx = mireRegcomp(includeMire, include_pattern);
	if (xx) {
	    fprintf(stderr, _("%s: Error in 'include' regex at offset %d: %s\n"),
		__progname, errptr, error);
	    goto errorexit;
	}
    }

    /* If there are no further arguments, do the business on stdin and exit. */
    if (i >= ac) {
	rc = pcregrep(stdin, FR_PLAIN, (filenames > FN_DEFAULT)? stdin_name : NULL);
	goto exit;
    }

    /*
     * Otherwise, work through the remaining arguments as files or directories.
     * Pass in the fact that there is only one argument at top level - this
     * suppresses the file name if the argument is not a directory and
     * filenames are not otherwise forced.
     */
    only_one_at_top = (i == ac - 1);   /* Catch initial value of i */

    for (; i < ac; i++) {
	int frc = grep_or_recurse(av[i], dee_action == dee_RECURSE,
	    only_one_at_top);
	if (frc > 1) rc = frc;
	else if (frc == 0 && rc == 1) rc = 0;
    }

exit:
    includeMire = mireFree(includeMire);
    excludeMire = mireFree(excludeMire);
    pattern_list = mireFreeAll(pattern_list, pattern_count);

    patterns = argvFree(patterns);

    optCon = poptFreeContext(optCon);

    return rc;

errorexit:
    rc = 2;
    goto exit;
}

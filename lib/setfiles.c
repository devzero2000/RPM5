/* 
 * setfiles
 *
 * AUTHOR:  Stephen Smalley <sds@epoch.ncsc.mil>
 * This program was derived in part from the setfiles.pl script
 * developed by Secure Computing Corporation.
 *
 * PURPOSE:
 * This program reads a set of file security context specifications
 * based on pathname regular expressions and labels files
 * accordingly, traversing a set of file systems specified by
 * the user.  The program does not cross file system boundaries.
 *
 * USAGE:
 * setfiles [-dnpqsvW] spec_file pathname...
 * 
 * -d   Show what specification matched each file.
 * -n	Do not change any file labels.
 * -q   Be quiet (suppress non-error output).
 * -r   Use an alternate root path
 * -s   Use stdin for a list of files instead of searching a partition.
 * -v	Show changes in file labels.  
 * -W   Warn about entries that have no matching file.
 *
 * spec_file	The specification file.
 * pathname...	The file systems to label (omit if using -s).	
 *
 * EXAMPLE USAGE:
 * ./setfiles -v file_contexts `mount | awk '/ext3/{print $3}'`
 *
 * SPECIFICATION FILE:
 * Each specification has the form:
 *       regexp [ -type ] ( context | <<none>> )
 *
 * By default, the regexp is an anchored match on both ends (i.e. a 
 * caret (^) is prepended and a dollar sign ($) is appended automatically).
 * This default may be overridden by using .* at the beginning and/or
 * end of the regular expression.  
 *
 * The optional type field specifies the file type as shown in the mode
 * field by ls, e.g. use -d to match only directories or -- to match only
 * regular files.
 * 
 * The value of <<none> may be used to indicate that matching files
 * should not be relabeled.
 *
 * The last matching specification is used.
 *
 * If there are multiple hard links to a file that match 
 * different specifications and those specifications indicate
 * different security contexts, then a warning is displayed
 * but the file is still labeled based on the last matching
 * specification other than <<none>>.
 */

#include "system.h"

#define	_RPMSX_INTERNAL
#include "rpmsx.h"

#include <sys/vfs.h>
#define __USE_XOPEN_EXTENDED 1	/* nftw */
#include <ftw.h>

#include <selinux/selinux.h>
#include <popt.h>

#include "debug.h"

static int add_assoc = 1;

/*
 * Command-line options.
 */
static int change = 1;
static int quiet = 0;
#define QPRINTF(args...) do { if (!quiet) printf(args); } while (0)
static int use_stdin = 0;
static int verbose = 0;
static int warn_no_match = 0;
static char *rootpath = NULL;
static int rootpathlen = 0;

static rpmsx mysx;

/**
 * Wrapper to free(3), hides const compilation noise, permit NULL, return NULL.
 * @param p		memory to free
 * @return		NULL always
 */
/*@unused@*/ static inline /*@null@*/
void * _free(/*@only@*/ /*@null@*/ /*@out@*/ const void * p)
	/*@modifies p @*/
{
    if (p != NULL)	free((void *)p);
    return NULL;
}

/*@unchecked@*/
int _rpmsx_debug = 0;

/**
 * Stable sort for policy specifications, patterns before paths.
 * @param sx           security context patterns
 */
static void rpmsxSort(rpmsx sx)
       /*@modifies sx @*/
{
    rpmsxp sxp;
    int i, j;

    /* Stable sort for policy regex's and paths. */
    sxp = xmalloc(sizeof(*sxp) * sx->Count);

    /* Regex patterns first ... */
    j = 0;
    for (i = 0; i < sx->Count; i++) {
	if (!sx->sxp[i].hasMetaChars)
	    continue;
	memcpy(sxp + j, sx->sxp + i, sizeof(*sxp));
	j++;
    }

    /* ... then file paths. */
    for (i = 0; i < sx->Count; i++) {
	if (sx->sxp[i].hasMetaChars)
	    continue;
	memcpy(sxp + j, sx->sxp + i, sizeof(*sxp));
	j++;
    }

    sx->sxp = _free(sx->sxp);
    sx->sxp = sxp;
}

/* Determine if the regular expression specification has any meta characters. */
static void rpmsxHasMetaChars(rpmsxp sxp)
{
    const char * c = sxp->pattern;
    int len = strlen(c);
    const char * end = c + len;

    sxp->hasMetaChars = 0; 

    /* Look at each character in the RE specification string for a 
     * meta character. Return when any meta character reached. */
    while (c != end) {
	switch(*c) {
	case '.':
	case '^':
	case '$':
	case '?':
	case '*':
	case '+':
	case '|':
	case '[':
	case '(':
	case '{':
	    sxp->hasMetaChars = 1;
	    return;
	    break;
	case '\\':		/* skip the next character */
	    c++;
	    break;
	default:
	    break;

	}
	c++;
    }
    return;
}

/* Return the length of the text that can be considered the stem, returns 0
 * if there is no identifiable stem */
static int rpmsxsPStem(const char * const buf)
{
    static const char * const regex_chars = ".^$?*+|[({";
    const char *tmp = strchr(buf + 1, '/');
    const char *ind;

    if (!tmp)
	return 0;

    for (ind = buf + 1; ind < tmp; ind++) {
	if (strchr(regex_chars, (int)*ind))
	    return 0;
    }
    return tmp - buf;
}

/* return the length of the text that is the stem of a file name */
static int rpmsxsFStem(const char * const buf)
{
    const char *tmp = strchr(buf + 1, '/');

    if (!tmp)
	return 0;
    return tmp - buf;
}

/* find the stem of a file spec, returns the index into stem_arr for a new
 * or existing stem, (or -1 if there is no possible stem - IE for a file in
 * the root directory or a regex that is too complex for us).  Makes bpp
 * point to the text AFTER the stem. */
static int rpmsxAdd(rpmsx sx, const char **bpp)
{
    int stem_len = rpmsxsPStem(*bpp);
    rpmsxs sxs;
    int i;

    if (!stem_len)
	return -1;
    for (i = 0; i < sx->nsxs; i++) {
	sxs = sx->sxs + i;
	if (stem_len != sxs->len)
	    continue;
	if (strncmp(*bpp, sxs->stem, stem_len))
	    continue;
	*bpp += stem_len;
	return i;
    }

    if (sx->nsxs == sx->maxsxs) {
	sx->maxsxs = sx->maxsxs * 2 + 16;
	sx->sxs = xrealloc(sx->sxs, sizeof(*sx->sxs) * sx->maxsxs);
    }
    sxs = sx->sxs + sx->nsxs;
    sxs->len = stem_len;
    sxs->stem = strndup(*bpp, stem_len);
    sx->nsxs++;
    *bpp += stem_len;
    return sx->nsxs - 1;
}

/* find the stem of a file name, returns the index into stem_arr (or -1 if
 * there is no match - IE for a file in the root directory or a regex that is
 * too complex for us).  Makes bpp point to the text AFTER the stem. */
static int rpmsxFind(const rpmsx sx, const char ** bpp)
{
    int stem_len = rpmsxsFStem(*bpp);
    rpmsxs sxs;
    int i;

    if (stem_len)
    for (i = 0; i < sx->nsxs; i++) {
	sxs = sx->sxs + i;
	if (stem_len != sxs->len)
	    continue;
	if (strncmp(*bpp, sxs->stem, stem_len))
	    continue;
	*bpp += stem_len;
	return i;
    }
    return -1;
}

rpmsx XrpmsxUnlink(rpmsx sx, const char * msg, const char * fn, unsigned ln)
{
    if (sx == NULL) return NULL;
/*@-modfilesys@*/
if (_rpmsx_debug && msg != NULL)
fprintf(stderr, "--> sx %p -- %d %s at %s:%u\n", sx, sx->nrefs, msg, fn, ln);
/*@=modfilesys@*/
    sx->nrefs--;
    return NULL;
}

rpmsx XrpmsxLink(rpmsx sx, const char * msg, const char * fn, unsigned ln)
{
    if (sx == NULL) return NULL;
    sx->nrefs++;

/*@-modfilesys@*/
if (_rpmsx_debug && msg != NULL)
fprintf(stderr, "--> sx %p ++ %d %s at %s:%u\n", sx, sx->nrefs, msg, fn, ln);
/*@=modfilesys@*/

    /*@-refcounttrans@*/ return sx; /*@=refcounttrans@*/
}

rpmsx rpmsxFree(rpmsx sx)
{
    int i;

    if (sx == NULL)
	return NULL;

    if (sx->nrefs > 1)
	return rpmsxUnlink(sx, __func__);

/*@-modfilesys@*/
if (_rpmsx_debug < 0)
fprintf(stderr, "*** sx %p\t%s[%d]\n", sx, __func__, sx->Count);
/*@=modfilesys@*/

    /*@-branchstate@*/
    if (sx->Count > 0)
    for (i = 0; i < sx->Count; i++) {
	rpmsxp sxp = sx->sxp + i;
	sxp->pattern = _free(sxp->pattern);
	sxp->type = _free(sxp->type);
	sxp->context = _free(sxp->context);
/*@i@*/	regfree(sxp->preg);
/*@i@*/	sxp->preg = _free(sxp->preg);
    }
    sx->sxp = _free(sx->sxp);

    if (sx->nsxs > 0)
    for (i = 0; i < sx->nsxs; i++) {
	rpmsxs sxs = sx->sxs + i;
	sxs->stem = _free(sxs->stem);
    }
    sx->sxs = _free(sx->sxs);
    /*@=branchstate@*/

    (void) rpmsxUnlink(sx, __func__);
    /*@-refcounttrans -usereleased@*/
/*@-boundswrite@*/
    memset(sx, 0, sizeof(*sx));		/* XXX trash and burn */
/*@=boundswrite@*/
    sx = _free(sx);
    /*@=refcounttrans =usereleased@*/
    return NULL;
}

/*
 * An association between an inode and a 
 * specification.  
 */
typedef struct file_spec * file_spec_t;
struct file_spec {
    ino_t ino;		/* inode number */
    int specind;	/* index of specification in spec */
    char *file;		/* full pathname for diagnostic messages about conflicts */
    file_spec_t next;	/* next association in hash bucket chain */
};

/*
 * The hash table of associations, hashed by inode number.
 * Chaining is used for collisions, with elements ordered
 * by inode number in each bucket.  Each hash bucket has a dummy 
 * header.
 */
#define HASH_BITS 16
#define HASH_BUCKETS (1 << HASH_BITS)
#define HASH_MASK (HASH_BUCKETS-1)
static struct file_spec fl_head[HASH_BUCKETS];

/*
 * Try to add an association between an inode and
 * a specification.  If there is already an association
 * for the inode and it conflicts with this specification,
 * then use the specification that occurs later in the
 * specification array.
 */
static file_spec_t file_spec_add(ino_t ino, int specind, const char *file)
{
    file_spec_t prevfl;
    file_spec_t fl;
    int h, no_conflict, ret;
    struct stat sb;

    h = (ino + (ino >> HASH_BITS)) & HASH_MASK;
    for (prevfl = &fl_head[h], fl = fl_head[h].next;
	 fl != NULL;
	 prevfl = fl, fl = fl->next)
    {
	rpmsxp sxp;

	if (ino == fl->ino) {
	    ret = lstat(fl->file, &sb);
	    if (ret < 0 || sb.st_ino != ino) {
		fl->specind = specind;
		fl->file = _free(fl->file);
		fl->file = xstrdup(file);
		return fl;
	    }

	    sxp = mysx->sxp + fl->specind;
	    no_conflict = (strcmp(sxp->context, sxp->context) == 0);
	    if (no_conflict)
		return fl;

	    fprintf(stderr,
		"%s:  conflicting specifications for %s and %s, using %s.\n",
		__progname, file, fl->file,
		((specind > fl->specind)
			? mysx->sxp[specind].context : sxp->context));
	    fl->specind = (specind > fl->specind) ? specind : fl->specind;
	    fl->file = _free(fl->file);
	    fl->file = xstrdup(file);
	    return fl;
	}

	if (ino > fl->ino)
	    break;
    }

    fl = xmalloc(sizeof(*fl));
    fl->ino = ino;
    fl->specind = specind;
    fl->file = xstrdup(file);
    fl->next = prevfl->next;
    prevfl->next = fl;
    return fl;
}

/*
 * Evaluate the association hash table distribution.
 */
static void file_spec_eval(void)
{
    file_spec_t fl;
    int h, used, nel, len, longest;

    used = 0;
    longest = 0;
    nel = 0;
    for (h = 0; h < HASH_BUCKETS; h++) {
	len = 0;
	for (fl = fl_head[h].next; fl; fl = fl->next) {
	    len++;
	}
	if (len)
	    used++;
	if (len > longest)
	    longest = len;
	nel += len;
    }

    QPRINTF ("%s:  hash table stats: %d elements, %d/%d buckets used, longest chain length %d\n",
	     __progname, nel, used, HASH_BUCKETS, longest);
}


/*
 * Destroy the association hash table.
 */
static void file_spec_destroy(void)
{
    file_spec_t fl;
    file_spec_t tmp;
    int h;

    for (h = 0; h < HASH_BUCKETS; h++) {
	fl = fl_head[h].next;
	while (fl) {
	    tmp = fl;
	    fl = fl->next;
	    tmp->file = _free(tmp->file);
	    tmp = _free(tmp);
	}
	fl_head[h].next = NULL;
    }
}


static int match(rpmsx sx, const char *name, struct stat *sb)
{
    rpmsxp sxp;
    static char errbuf[255 + 1];
    const char *fullname = name;
    const char *buf = name;
    int i, ret, file_stem;

    /* fullname will be the real file that gets labeled
     * name will be what is matched in the policy */
    if (rootpath != NULL) {
	if (strncmp(rootpath, name, rootpathlen) != 0) {
	    fprintf(stderr, "%s:  %s is not located in %s\n", 
			__progname, name, rootpath);
	    return -1;
	}
	name += rootpathlen;
	buf += rootpathlen;
    }

    ret = lstat(fullname, sb);
    if (ret) {
	fprintf(stderr, "%s:  unable to stat file %s\n", __progname, fullname);
	return -1;
    }

    file_stem = rpmsxFind(sx, &buf);

    /* 
     * Check for matching specifications in reverse order, so that
     * the last matching specification is used.
     */
    for (i = sx->Count - 1; i >= 0; i--) {
	sxp = sx->sxp + i;

	/* if the spec in question matches no stem or has the same
	 * stem as the file AND if the spec in question has no mode
	 * specified or if the mode matches the file mode then we do
	 * a regex check	*/
	if (sxp->fstem != -1 && sxp->fstem != file_stem)
	    continue;
	if (sxp->fmode && (sb->st_mode & S_IFMT) != sxp->fmode)
	    continue;

	if (sxp->fstem == -1)
	    ret = regexec(sxp->preg, name, 0, NULL, 0);
	else
	    ret = regexec(sxp->preg, buf, 0, NULL, 0);
	if (ret == 0)
	    break;

	if (ret == REG_NOMATCH)
	    continue;

	/* else it's an error */
	regerror(ret, sxp->preg, errbuf, sizeof errbuf);
	fprintf(stderr, "%s:  unable to match %s against %s:  %s\n",
		__progname, name, sxp->pattern, errbuf);
	return -1;
    }

    /* Cound matches found. */
    if (i >= 0)
	sxp->matches++;

    return i;
}

#ifdef DYING
/* Used with qsort to sort specs from lowest to highest hasMetaChars value */
static int spec_compare(const void* specA, const void* specB)
{
    return (
	((const rpmsxp)specB)->hasMetaChars -
	((const rpmsxp)specA)->hasMetaChars
	); 
}
#endif

/*
 * Check for duplicate specifications. If a duplicate specification is found 
 * and the context is the same, give a warning to the user. If a duplicate 
 * specification is found and the context is different, give a warning
 * to the user (This could be changed to error). Return of non-zero is an error.
 */
static int nodups_specs(rpmsx sx)
{
    int i, j;

    for (i = 0; i < sx->Count; i++) {
	rpmsxp sip = sx->sxp + i;
	for (j = i + 1; j < sx->Count; j++) { 
	    rpmsxp sjp = sx->sxp + j;

	    /* Check if same RE string */
	    if (strcmp(sjp->pattern, sip->pattern))
		continue;
	    if (sjp->fmode && sip->fmode && sjp->fmode != sip->fmode)
		continue;

	    /* Same RE string found */
	    if (strcmp(sjp->context, sip->context)) {
		/* If different contexts, give warning */
		fprintf(stderr,
		"ERROR: Multiple different specifications for %s  (%s and %s).\n",
			sip->pattern, sjp->context, sip->context);
	    } else {
		/* If same contexts give warning */
		fprintf(stderr,
		"WARNING: Multiple same specifications for %s.\n",
			sip->pattern);
	    }
	}
    }
    return 0;
}

/*
 * Apply the last matching specification to a file.
 * This function is called by nftw on each file during
 * the directory traversal.
 */
static int apply_spec(const char *file,
		      const struct stat *sb_unused, int flag, struct FTW *s_unused)
{
    const char * my_file;
    file_spec_t fl;
    struct stat my_sb;
    int i, ret;
    char * context; 
    rpmsxp sxp;

    /* Skip the extra slash at the beginning, if present. */
    if (file[0] == '/' && file[1] == '/')
	my_file = &file[1];
    else
	my_file = file;

    if (flag == FTW_DNR) {
	fprintf(stderr, "%s:  unable to read directory %s\n",
		__progname, my_file);
	return 0;
    }

    i = match(mysx, my_file, &my_sb);
    if (i < 0)
	/* No matching specification. */
	return 0;
    sxp = mysx->sxp + i;

    /*
     * Try to add an association between this inode and
     * this specification.  If there is already an association
     * for this inode and it conflicts with this specification,
     * then use the last matching specification.
     */
    if (add_assoc) {
	fl = file_spec_add(my_sb.st_ino, i, my_file);
	if (!fl)
	    /* Insufficient memory to proceed. */
	    return 1;

	if (fl->specind != i)
	    /* There was already an association and it took precedence. */
	    return 0;
    }

    if (_rpmsx_debug) {
	if (sxp->type) {
	    printf("%s:  %s matched by (%s,%s,%s)\n", __progname,
		       my_file, sxp->pattern,
		       sxp->type, sxp->context);
	} else {
	    printf("%s:  %s matched by (%s,%s)\n", __progname,
		       my_file, sxp->pattern, sxp->context);
	}
    }

    /* Get the current context of the file. */
    ret = lgetfilecon(my_file, &context);
    if (ret < 0) {
	if (errno == ENODATA) {
	    context = xstrdup("<<none>>");
	} else {
	    perror(my_file);
	    fprintf(stderr, "%s:  unable to obtain attribute for file %s\n", 
			__progname, my_file);
	    return -1;
	}
    }

    /*
     * Do not relabel the file if the matching specification is 
     * <<none>> or the file is already labeled according to the 
     * specification.
     */
    if ((strcmp(sxp->context, "<<none>>") == 0) || 
	(strcmp(sxp->context, context) == 0))
    {
	freecon(context);
	return 0;
    }

    if (verbose) {
	printf("%s:  relabeling %s from %s to %s\n", __progname,
	       my_file, context, sxp->context);
    }

    freecon(context);

    /*
     * Do not relabel the file if -n was used.
     */
    if (!change)
	return 0;

    /*
     * Relabel the file to the specified context.
     */
    ret = lsetfilecon(my_file, sxp->context);
    if (ret) {
	perror(my_file);
	fprintf(stderr, "%s:  unable to relabel %s to %s\n",
		__progname, my_file, sxp->context);
	return 1;
    }

    return 0;
}

static int nerr = 0;

static void inc_err(void)
{
    nerr++;
    if (nerr > 9 && !_rpmsx_debug) {
	fprintf(stderr, "Exiting after 10 errors.\n");
	exit(1);
    }
}

int rpmsxParse(rpmsx sx, const char *fn)
{
    FILE * fp;
    char errbuf[BUFSIZ + 1];
    char buf[BUFSIZ + 1];
    char * bp;
    char * regex;
    char * type;
    char * context;
    char * anchored_regex;
    int items;
    int len;
    int lineno;
    int pass;
    int regerr;

    if ((fp = fopen(fn, "r")) == NULL) {
	perror(fn);
	return -1;
    }

    /* 
     * Perform two passes over the specification file.
     * The first pass counts the number of specifications and
     * performs simple validation of the input.  At the end
     * of the first pass, the spec array is allocated.
     * The second pass performs detailed validation of the input
     * and fills in the spec array.
     */
    for (pass = 0; pass < 2; pass++) {
	rpmsxp sxp;

	lineno = 0;
	sx->Count = 0;
	sxp = sx->sxp;
	while (fgets(buf, sizeof buf, fp)) {
	    lineno++;
	    len = strlen(buf);
	    if (buf[len - 1] != '\n') {
		fprintf(stderr,
			"%s:  no newline on line number %d (only read %s)\n",
			fn, lineno, buf);
		inc_err();
		continue;
	    }
	    buf[len - 1] = 0;
	    bp = buf;
	    while (isspace(*bp))
		bp++;
	    /* Skip comment lines and empty lines. */
	    if (*bp == '#' || *bp == 0)
		continue;
	    items = sscanf(buf, "%as %as %as", &regex, &type, &context);
	    if (items < 2) {
		fprintf(stderr,
			"%s:  line number %d is missing fields (only read %s)\n",
			fn, lineno, buf);
		inc_err();
		if (items == 1)
		    regex = _free(regex);
		continue;
	    } else if (items == 2) {
		/* The type field is optional. */
		context = _free(context);
		context = type;
		type = 0;
	    }

	    if (pass == 1) {
		/* On the second pass, compile and store the specification in spec. */
		const char *reg_buf = regex;
		sxp->fstem = rpmsxAdd(sx, &reg_buf);
		sxp->pattern = regex;

		/* Anchor the regular expression. */
		len = strlen(reg_buf);
		anchored_regex = xmalloc(len + 3);
		sprintf(anchored_regex, "^%s$", reg_buf);

		/* Compile the regular expression. */
		sxp->preg = xcalloc(1, sizeof(*sxp->preg));
		regerr = regcomp(sxp->preg, anchored_regex,
			    REG_EXTENDED | REG_NOSUB);
		if (regerr < 0) {
		    regerror(regerr, sxp->preg, errbuf, sizeof errbuf);
		    fprintf(stderr,
			"%s:  unable to compile regular expression %s on line number %d:  %s\n",
			fn, regex, lineno,
			errbuf);
		    inc_err();
		}
		anchored_regex = _free(anchored_regex);

		/* Convert the type string to a mode format */
		sxp->type = type;
		sxp->fmode = 0;
		if (!type)
		    goto skip_type;
		len = strlen(type);
		if (type[0] != '-' || len != 2) {
		    fprintf(stderr,
			"%s:  invalid type specifier %s on line number %d\n",
			fn, type, lineno);
		    inc_err();
		    goto skip_type;
		}
		switch (type[1]) {
		case 'b':
		    sxp->fmode = S_IFBLK;
		    break;
		case 'c':
		    sxp->fmode = S_IFCHR;
		    break;
		case 'd':
		    sxp->fmode = S_IFDIR;
		    break;
		case 'p':
		    sxp->fmode = S_IFIFO;
		    break;
		case 'l':
		    sxp->fmode = S_IFLNK;
		    break;
		case 's':
		    sxp->fmode = S_IFSOCK;
		    break;
		case '-':
		    sxp->fmode = S_IFREG;
		    break;
		default:
		    fprintf(stderr,
			"%s:  invalid type specifier %s on line number %d\n",
			fn, type, lineno);
		    inc_err();
		}

	      skip_type:

		sxp->context = context;

		if (strcmp(context, "<<none>>")) {
		    if (security_check_context(context) < 0 && errno != ENOENT) {
			fprintf(stderr,
				"%s:  invalid context %s on line number %d\n",
				fn, context, lineno);
			inc_err();
		    }
		}

		/* Determine if specification has 
		 * any meta characters in the RE */
		rpmsxHasMetaChars(sxp);
		sxp++;
	    }

	    sx->Count++;
	    if (pass == 0) {
		regex = _free(regex);
		if (type)
		    type = _free(type);
		context = _free(context);
	    }
	}

	if (nerr)
	    return -1;

	if (pass == 0) {
	    QPRINTF("%s:  read %d specifications\n", fn, sx->Count);
	    if (sx->Count == 0)
		return 0;
	    sx->sxp = xcalloc(sx->Count, sizeof(*sx->sxp));
	    rewind(fp);
	}
    }
    fclose(fp);

#ifdef	DYING
    /* Sort the specifications with most general first */
    qsort(sx->sxp, sx->Count, sizeof(*sx->sxp), spec_compare);
#else
    rpmsxSort(sx);
#endif

    /* Verify no exact duplicates */
    if (nodups_specs(sx) != 0)
	return -1;
    return 0;
}

rpmsx rpmsxNew(const char * fn)
{
    rpmsx sx;

    sx = xcalloc(1, sizeof(*sx));
    sx->sxp = NULL;
    sx->Count = 0;
    sx->i = -1;
    sx->sxs = NULL;
    sx->nsxs = 0;
    sx->maxsxs = 0;
    sx->reverse = 0;

    (void) rpmsxLink(sx, __func__);

    if (rpmsxParse(sx, fn) != 0)
	return rpmsxFree(sx);

    return sx;
}

static struct poptOption optionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL, &_rpmsx_debug, 1,
        N_("show what specification matched each file"), NULL },
 { "nochange", 'n', POPT_ARG_VAL, &change, 0,
        N_("do not change any file labels"), NULL },
 { "quiet", 'q', POPT_ARG_VAL, &quiet, 1,
        N_("be quiet (suppress non-error output)"), NULL },
 { "root", 'r', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT, &rootpath, 0,
        N_("use an alternate root path"), N_("ROOT") },
 { "stdin", 's', POPT_ARG_VAL, &use_stdin, 1,
        N_("use stdin for a list of files instead of searching a partition"), NULL },
 { "verbose", 'v', POPT_ARG_VAL, &verbose, 1,
        N_("show changes in file labels"), NULL },
 { "warn", 'W', POPT_ARG_VAL, &warn_no_match, 1,
        N_("warn about entries that have no matching file"), NULL },

   POPT_AUTOHELP
   POPT_TABLEEND
};

static void usage(const char * const name, poptContext optCon)
{
	fprintf(stderr,
		"usage:  %s [-dnqvW] spec_file pathname...\n"
		"usage:  %s -s [-dnqvW] spec_file\n", name, name);
	poptPrintUsage(optCon, stderr, 0);
	exit(1);
}


int main(int argc, char **argv)
{
    poptContext optCon;
    const char ** av;
    const char * arg;
    int ec = EXIT_FAILURE;	/* assume failure. */
    int rc;
    int i;

#if HAVE_MCHECK_H && HAVE_MTRACE
    /*@-noeffect@*/
    mtrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
    /*@=noeffect@*/
#endif

    setprogname(argv[0]);       /* Retrofit glibc __progname */
    /* XXX glibc churn sanity */
    if (__progname == NULL) {
	if ((__progname = strrchr(argv[0], '/')) != NULL) __progname++;
	else __progname = argv[0];
    }
                                                                                
    (void) setlocale(LC_ALL, "" );
    (void) bindtextdomain(PACKAGE, LOCALEDIR);
    (void) textdomain(PACKAGE);

    optCon = poptGetContext(__progname, argc, (const char **)argv, optionsTable, 0);

    /* Process all options, whine if unknown. */
    while ((rc = poptGetNextOpt(optCon)) > 0) {
	switch (rc) {
	default:
/*@-nullpass@*/
	    fprintf(stderr, _("%s: option table misconfigured (%d)\n"),
		__progname, rc);
/*@=nullpass@*/
	    goto exit;
	    /*@notreached@*/ /*@switchbreak@*/ break;
        }
    }

    if (rc < -1) {
/*@-nullpass@*/
	fprintf(stderr, "%s: %s: %s\n", __progname,
		poptBadOption(optCon, POPT_BADOPTION_NOALIAS),
		poptStrerror(rc));
/*@=nullpass@*/
	goto exit;
    }

    /* trim trailing /, if present */
    if (rootpath != NULL) {
	rootpathlen = strlen(rootpath);
	while (rootpath[rootpathlen - 1] == '/')
	    rootpath[--rootpathlen] = 0;
    }

    av = poptGetArgs(optCon);
    if (use_stdin) {
	add_assoc = 0;
	/* Check exactly 1 arg */
	if (av == NULL || !(av[0] != NULL && av[1] == NULL)) {
	    usage(__progname, optCon);
	}
    } else {
	/* Check at least 2 args */
	if (av == NULL || !(av[0] != NULL && av[1] != NULL))
	    usage(__progname, optCon);
    }

    /* Parse the specification file. */
    mysx = rpmsxNew(*av);
    if (mysx == NULL) {
	perror(*av);
	goto exit;
    }
    av++;

    /*
     * Apply the specifications to the file systems.
     */
    if (use_stdin) {
	char buf[PATH_MAX];
	struct stat sb;
	int flag;

	while(fgets(buf, sizeof(buf), stdin)) {
	    strtok(buf, "\n");
	    if (buf[0] == '\n')
		continue;
	    if (stat(buf, &sb)) {
		fprintf(stderr, "File \"%s\" not found.\n", buf);
		continue;
	    }
	    switch(sb.st_mode) {
	    case S_IFDIR:
		flag = FTW_D;
		break;
	    case S_IFLNK:
		flag = FTW_SL;
		break;
	    default:
		flag = FTW_F;
		break;
	    }
	    apply_spec(buf, &sb, flag, NULL);
	}
    }
    else while ((arg = *av++) != NULL)
    {
	if (rootpath != NULL) {
	    QPRINTF("%s:  labeling files, pretending %s is /\n",
			__progname, rootpath);
	}

	QPRINTF("%s:  labeling files under %s\n", __progname, arg);
		
	/* Walk the file tree, calling apply_spec on each file. */
	if (nftw(arg, apply_spec, 1024, FTW_PHYS | FTW_MOUNT)) {
	    fprintf(stderr, "%s:  error while labeling files under %s\n",
			__progname, arg);
	    goto exit;
	}

	/*
	 * Evaluate the association hash table distribution for the
	 * directory tree just traversed.
	 */
	file_spec_eval();

	/* Reset the association hash table for the next directory tree. */
	file_spec_destroy();
    }

    if (warn_no_match) {
	for (i = 0; i < mysx->Count; i++) {
	    rpmsxp sxp;

	    sxp = mysx->sxp + i;
	    if (sxp->matches > 0)
		continue;
	    if (sxp->type) {
		printf("%s:  Warning!  No matches for (%s, %s, %s)\n",
			 __progname, sxp->pattern,
			 sxp->type, sxp->context);
	    } else {
		printf("%s:  Warning!  No matches for (%s, %s)\n",
			 __progname, sxp->pattern, sxp->context);
	    }
	}
    }

    QPRINTF("%s:  Done.\n", __progname);
    ec = 0;

exit:
    mysx = rpmsxFree(mysx);
    optCon = poptFreeContext(optCon);

#if HAVE_MCHECK_H && HAVE_MTRACE
    /*@-noeffect@*/
    muntrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
    /*@=noeffect@*/
#endif

    return ec;
}

#ifndef H_PCRS
#define H_PCRS

/**
 * \file rpmio/pcrs.h
 * RPM pattern find&replace.
 */

#include <pcre.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Capacity */
/* Maximum number of capturing subpatterns allowed. MUST be <= 99! */
/* FIXME: Should be dynamic */
#define PCRS_MAX_SUBMATCHES  33
/* Initial amount of matches that can be stored in global searches */
#define PCRS_MAX_MATCH_INIT  40
/* Factor by which storage for matches is extended if exhausted */
#define PCRS_MAX_MATCH_GROW  1.6

/* Error codes */
#define PCRS_ERR_NOMEM     -10	/* Failed to acquire memory. */
#define PCRS_ERR_CMDSYNTAX -11	/* Syntax of s///-command */
#define PCRS_ERR_STUDY     -12	/* pcre error while studying the pattern */
#define PCRS_ERR_BADJOB    -13	/* NULL job pointer, pattern or substitute */
#define PCRS_WARN_BADREF   -14	/* Backreference out of range */

/* Flags */
#define PCRS_GLOBAL          1	/* Job should be applied globally, as with perl's g option */
#define PCRS_TRIVIAL         2	/* Backreferences in the substitute are ignored */
#define PCRS_SUCCESS         4	/* Job did previously match */

/* A compiled substitute */
typedef struct {
    char *text;			/* The plaintext part of the substitute, with all backreferences stripped */
    int backrefs;		/* The number of backreferences */
    int block_offset[PCRS_MAX_SUBMATCHES];	/* Array with the offsets of all plaintext blocks in text */
    size_t block_length[PCRS_MAX_SUBMATCHES];	/* Array with the lengths of all plaintext blocks in text */
    int backref[PCRS_MAX_SUBMATCHES];	/* Array with the backref number for all plaintext block borders */
    int backref_count[PCRS_MAX_SUBMATCHES + 2];	/* Array with the number of references to each backref index */
} pcrs_substitute;

/*
 * A match, including all captured subpatterns (submatches)
 * Note: The zeroth is the whole match, the PCRS_MAX_SUBMATCHES + 0th
 * is the range before the match, the PCRS_MAX_SUBMATCHES + 1th is the
 * range after the match.
 */
typedef struct {
    int submatches;		/* Number of captured subpatterns */
    int submatch_offset[PCRS_MAX_SUBMATCHES + 2];	/* Offset for each submatch in the subject */
    size_t submatch_length[PCRS_MAX_SUBMATCHES + 2];	/* Length of each submatch in the subject */
} pcrs_match;

/* A PCRS job */
typedef struct PCRS_JOB {
    pcre *pattern;		/* The compiled pcre pattern */
    pcre_extra *hints;	/* The pcre hints for the pattern */
    int options;		/* The pcre options (numeric) */
    int flags;		/* The pcrs and user flags (see "Flags" above) */
    pcrs_substitute *substitute;	/* The compiled pcrs substitute */
    struct PCRS_JOB *next;	/* Pointer for chaining jobs to joblists */
} pcrs_job;

/* Main usage */
extern pcrs_job *pcrs_compile_command(const char *command, int *errptr);
extern pcrs_job *pcrs_compile(const char *pattern, const char *substitute,
		const char *options, int *errptr);
extern int pcrs_execute(pcrs_job * job, char *s, size_t ns,
		char **result, size_t * result_length);
extern int pcrs_execute_list(pcrs_job * joblist, char *s, size_t ns,
		char **result, size_t * result_length);

/* Freeing jobs */
extern pcrs_job *pcrs_free_job(pcrs_job * job);
extern void pcrs_free_joblist(pcrs_job * joblist);

/* Info on errors: */
extern const char *pcrs_strerror(const int error);

#ifdef __cplusplus
}				/* extern "C" */
#endif
#endif				/* ndef PCRS_H_INCLUDED */

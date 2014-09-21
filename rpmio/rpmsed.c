/*-
 * Copyright (c) 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * David Hitz of Auspex Systems Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "system.h"

#include <rpmio_internal.h>	/* XXX fdGetFILE */
#include <rpmlog.h>
#include <poptIO.h>

#define	_RPMSED_INTERNAL
#include <rpmsed.h>

#include "debug.h"

int _rpmsed_debug;
#define	SPEW(_list)	if (_rpmsed_debug) fprintf _list

/*==============================================================*/
static rpmRC rpmsedCompile(rpmsed sed)
{
    rpmRC rc = RPMRC_FAIL;	/* assume failure */
    int i;

    sed->jobs = (pcrs_job **) xcalloc(sed->nsubcmds, sizeof(*sed->jobs));
    sed->njobs = 0;
    for (i = 0; i < sed->nsubcmds; i++) {
	sed->job = pcrs_compile_command(sed->subcmds[i], &sed->err);
	if (sed->job == NULL) {
	    fprintf(stderr, "%s: Compile error:  %s (%d).\n", __FUNCTION__,
			pcrs_strerror(sed->err), sed->err);
	    goto exit;
	}
	sed->jobs[i] = sed->job;
	sed->njobs++;
	sed->job = NULL;
    }
    rc = RPMRC_OK;

exit:
SPEW((stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, sed, rc));
    return rc;
}

static rpmRC rpmsedExecute(rpmsed sed)
{
    rpmRC rc = RPMRC_FAIL;	/* assume failure */
    int i;

    sed->b = sed->buf;;
    for (i = 0; i < sed->njobs; i++) {
	sed->job = sed->jobs[i];

	sed->result = NULL;
	sed->length = 0;
	sed->nb = strlen(sed->b);
	sed->err =
	    pcrs_execute(sed->job, sed->b, sed->nb, &sed->result, &sed->length);
	if (sed->err < 0) {
	    fprintf(stderr, "%s: Exec error:  %s (%d) in input line %d\n",
		__FUNCTION__, pcrs_strerror(sed->err), sed->err, sed->linenum);
	    goto exit;
	}
	if (i > 0)
	    sed->b = _free(sed->b);
	sed->b = sed->result;
	sed->job = NULL;
    }
    rc = RPMRC_OK;

exit:
SPEW((stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, sed, rc));
    return (sed->job ? RPMRC_FAIL : RPMRC_OK);
}

rpmRC rpmsedClose(rpmsed sed)
{
    rpmRC rc = RPMRC_FAIL;	/* assume failure */
    int xx;

    /* Close input file */
    if (sed->fd)
	xx = Fclose(sed->fd);
    sed->fd = NULL;
    sed->ifp = NULL;
    rc = RPMRC_OK;

SPEW((stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, sed, rc));
    return rc;
}

rpmRC rpmsedProcess(rpmsed sed)
{
    rpmRC rc = RPMRC_FAIL;	/* assume failure */

    if (sed->ofp == NULL)
	sed->ofp = stdout;

    /* Read & Execute on every input line */
    sed->linenum = 0;
    while (!feof(sed->ifp) && !ferror(sed->ifp) && fgets(sed->buf, sed->nbuf, sed->ifp)) {

	sed->linenum++;

	rc = rpmsedExecute(sed);
	if (rc)
	    goto exit;

	if (fwrite(sed->result, 1, sed->length, sed->ofp) != sed->length) {
	    fprintf(stderr, _("%s: fwrite failed\n"), __FUNCTION__);
	    goto exit;
	}
	sed->result = _free(sed->result);
	sed->length = 0;
    }
    rc = RPMRC_OK;

exit:
SPEW((stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, sed, rc));
    return rc;
}

rpmRC rpmsedOpen(rpmsed sed, const char *fn)
{
    rpmRC rc = RPMRC_FAIL;	/* assume failure */

    /* Open input file */
    sed->ifp = NULL;
    sed->fd = Fopen(fn, "r.fpio");
    if (sed->fd == NULL || Ferror(sed->fd)) {
	fprintf(stderr, _("%s: Fopen(%s, \"r.fpio\") failed\n"), __FUNCTION__, fn);
	goto exit;
    }
    sed->ifp = (FILE *) fdGetFILE(sed->fd);
    rc = RPMRC_OK;

exit:
SPEW((stderr, "<-- %s(%p, %s) rc %d\n", __FUNCTION__, sed, fn, rc));
    return rc;
}

/*==============================================================*/
static rpmRC
rpmsedInitPopt(rpmsed sed, int ac, char * const* av)
{

  struct poptOption rpmsedOptionsTable[] = {
  { "expression", 'e', POPT_ARG_ARGV,	&sed->subcmds, 0,
	N_("Add the SUBCMD to the commands to be executed"), N_("SUBCMD") },
  { "file", 'f', POPT_ARG_ARGV,		&sed->cmdfiles, 0,
	N_("Append commands to be executed from FILE"), N_("FILE") },
  { "follow-symlinks", '\0', POPT_ARG_ARGV,	&sed->flags, SED_FLAGS_FOLLOW,
	N_("Follow symlinks when processing"),	NULL },
  { "in-place", 'i', POPT_ARG_STRING|POPT_ARGFLAG_OPTIONAL, &sed->suffix, 0,
	N_("Edit files in-place"),		N_("SUFFIX") },
  { "copy", 'c', POPT_BIT_SET,		&sed->flags, SED_FLAGS_COPY,
	N_("Use copy instead of rename"),	NULL },
  { "line-length", 'l', POPT_ARG_INT,	&sed->line_length, 0,
	N_("Specify the desired line-wrap length"),	N_("N") },
  { "posix", '\0', POPT_BIT_SET,		&sed->flags, SED_FLAGS_POSIX,
	N_("Disable all GNU extensions"),	NULL },
  { "regex-extended", 'r', POPT_BIT_SET,	&sed->flags, SED_FLAGS_EXTENDED,
	N_("Use extended regular expressions"),	NULL },
  { "separate", 's', POPT_BIT_SET,	&sed->flags, SED_FLAGS_SEPARATE,
	N_("Consider files as separate rather than continuous"),	NULL },
  { "unbuffered", 'u', POPT_BIT_SET,	&sed->flags, SED_FLAGS_UNBUFFERED,
	N_("Load minimal amounts of data"),	NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"),
	NULL },

    POPT_AUTOHELP

  { NULL, (char)-1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
        "\
Usage: rpmsed [OPTION]... {script-only-if-no-other-script} [input-file]...\n\
\n\
  -n, --quiet, --silent\n\
                 suppress automatic printing of pattern space\n\
  -e script, --expression=script\n\
                 add the script to the commands to be executed\n\
  -f script-file, --file=script-file\n\
                 add the contents of script-file to the commands to be executed\n\
  --follow-symlinks\n\
                 follow symlinks when processing in place; hard links\n\
                 will still be broken.\n\
  -i[SUFFIX], --in-place[=SUFFIX]\n\
                 edit files in place (makes backup if extension supplied).\n\
                 The default operation mode is to break symbolic and hard links.\n\
                 This can be changed with --follow-symlinks and --copy.\n\
  -c, --copy\n\
                 use copy instead of rename when shuffling files in -i mode.\n\
                 While this will avoid breaking links (symbolic or hard), the\n\
                 resulting editing operation is not atomic.  This is rarely\n\
                 the desired mode; --follow-symlinks is usually enough, and\n\
                 it is both faster and more secure.\n\
  -l N, --line-length=N\n\
                 specify the desired line-wrap length for the `l' command\n\
  --posix\n\
                 disable all GNU extensions.\n\
  -r, --regexp-extended\n\
                 use extended regular expressions in the script.\n\
  -s, --separate\n\
                 consider files as separate rather than as a single continuous\n\
                 long stream.\n\
  -u, --unbuffered\n\
                 load minimal amounts of data from the input files and flush\n\
                 the output buffers more often\n\
      --help     display this help and exit\n\
      --version  output version information and exit\n\
\n\
If no -e, --expression, -f, or --file option is given, then the first\n\
non-option argument is taken as the sed script to interpret.  All\n\
remaining arguments are names of input files; if no input files are\n\
specified, then the standard input is read.\n\
", NULL },

    POPT_TABLEEND
  };

    static int _popt_context_flags = 0;	/* XXX POPT_CONTEXT_POSIXMEHARDER */
    poptContext con = NULL;
    int have_trailing_slash;
    int r;
    rpmRC rc = RPMRC_FAIL;	/* assume failure */
    int xx;

    con = poptGetContext(av[0], ac, (const char **)av,
		rpmsedOptionsTable, _popt_context_flags);
    while ((xx = poptGetNextOpt(con)) > 0)
    switch (xx) {
    default:
	fprintf(stderr, _("%s: option table misconfigured (%d)\n"),
                __FUNCTION__, xx);
	goto exit;
	break;
    }

    sed->av = NULL;
    r = argvAppend(&sed->av, poptGetArgs(con));
    sed->ac = argvCount(sed->av);
    if (sed->ac < 2) {
	poptPrintUsage(con, stderr, 0);
	goto exit;
    }
    rc = RPMRC_OK;

exit:
    if (con)
	con = poptFreeContext(con);

SPEW((stderr, "<-- %s(%p,%p[%d]) rc %d\n", __FUNCTION__, sed, av, ac, rc));
    return rc;
}

static rpmRC rpmsedInit(rpmsed sed, int ac, const char ** av, unsigned flags)
{
    rpmRC rc;
    int xx;
    int i;

    sed->flags = (flags ? flags : SED_FLAGS_NONE);
    rc = rpmsedInitPopt(sed, ac, (char *const *)av);
    if (rc)
	goto exit;

    sed->ac = argvCount(sed->av);
    sed->nsubcmds = argvCount(sed->subcmds);
    sed->ncmdfiles = argvCount(sed->cmdfiles);

    /* Use av[0] as pattern if no -e/-f commands. */
    if (sed->nsubcmds == 0 && sed->ncmdfiles == 0 && sed->ac >= 2) {
	xx = argvAdd(&sed->subcmds, sed->av[0]);
	sed->nsubcmds++;
	for (i = 1; i < sed->ac; i++)
	    sed->av[i-1] = sed->av[i];
	sed->av[--sed->ac] = NULL;
    }

    /* Append commands from files. */
    if (sed->cmdfiles)
    for (i = 0; i < sed->ncmdfiles; i++) {
	const char * fn = sed->cmdfiles[i];
	sed->fd = Fopen(fn, "r.fpio");
	if (sed->fd == NULL || Ferror(sed->fd)) {
	    fprintf(stderr, _("Fopen(%s, \"r.fpio\") failed\n"), fn);
	    goto exit;
	}
	xx = argvFgets(&sed->subcmds, sed->fd);
	xx = Fclose(sed->fd);
	sed->fd = NULL;
    }

    /* Check usage */
    if (sed->nsubcmds == 0)
	goto exit;

    /* Compile the job(s) */
    rc = rpmsedCompile(sed);
    if (rc)
	goto exit;
	
    /* Allocate a per-line buffer. */
    if (sed->buf == NULL) {
	sed->nbuf = BUFSIZ;
	sed->buf = (char *) xmalloc(sed->nbuf);
    }

    /* Use stdin if no arguments supplied. */
    if (sed->ac == 0) {
	xx = argvAdd(&sed->av, "-");
	sed->ac++;
    }

    rc = RPMRC_OK;

exit:
SPEW((stderr, "<-- %s(%p,%p[%d],0x%x) rc %d\n", __FUNCTION__, sed, av, ac, flags, rc));
    return rc;
}

static void rpmsedFini(void * _sed)
{
    rpmsed sed = (rpmsed) _sed;
    rpmRC xx;

    sed->flags = 0;
    sed->fn = NULL;
    sed->job = NULL;
    sed->ifp = NULL;
    sed->ofp = NULL;

    sed->av = argvFree(sed->av);
    sed->ac = 0;
    sed->suffix = _free(sed->suffix);
    sed->line_length = 0;
    sed->cmdfiles = _free(sed->cmdfiles);
    sed->ncmdfiles = 0;
    sed->cmdfiles = _free(sed->cmdfiles);
    sed->ncmdfiles = 0;
    sed->subcmds = argvFree(sed->subcmds);
    sed->nsubcmds = 0;
    sed->job = NULL;
    if (sed->jobs) {
	int i;
	for (i = 0; i < sed->njobs; i++) {
	    if (sed->jobs[i])
		pcrs_free_job(sed->jobs[i]);
	    sed->jobs[i] = NULL;
	}
	sed->jobs = _free(sed->jobs);
    }
    sed->njobs = 0;
    sed->err = 0;

    xx = rpmsedClose(sed);

    sed->linenum = 0;
    sed->buf = _free(sed->buf);
    sed->nbuf = 0;
    sed->b = NULL;
    sed->nb = 0;
    sed->result = _free(sed->result);
    sed->length = 0;
}

rpmioPool _rpmsedPool = NULL;

static rpmsed rpmsedGetPool(/*@null@*/ rpmioPool pool)
{
    rpmsed sed;

    if (_rpmsedPool == NULL) {
	_rpmsedPool = rpmioNewPool("sed", sizeof(*sed), -1, _rpmsed_debug,
			NULL, NULL, rpmsedFini);
	pool = _rpmsedPool;
    }
    sed = (rpmsed) rpmioGetPool(pool, sizeof(*sed));
    memset(((char *)sed)+sizeof(sed->_item), 0, sizeof(*sed)-sizeof(sed->_item));
    return sed;
}

rpmsed rpmsedNew(char ** argv, unsigned flags)
{
    static char *_argv[] = { "rpmsed", NULL };
    ARGV_t av = (ARGV_t) (argv ? argv : _argv);
    int ac = argvCount(av);
    rpmsed sed = rpmsedGetPool(_rpmsedPool);
    rpmRC rc;

SPEW((stderr, "--> %s(%p,0x%x)\n", __FUNCTION__, argv, flags));

    rc = rpmsedInit(sed, ac, av, flags);
    if (rc)
	sed = rpmsedFree(sed);

    return rpmsedLink(sed);
}

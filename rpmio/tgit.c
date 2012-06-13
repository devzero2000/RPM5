#include "system.h"

#if defined(HAVE_GIT2_H)
#include <git2.h>
#include <git2/branch.h>
#include <git2/errors.h>
#endif

#define	_RPMIOB_INTERNAL
#include <rpmio.h>
#include <ugid.h>
#include <poptIO.h>

#define	_RPMGIT_INTERNAL
#include <rpmgit.h>

#include "debug.h"

#define	SPEW(_t, _rc, _git)	\
  { if ((_t) || _rpmgit_debug ) \
	fprintf(stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, (_git), \
		(_rc)); \
  }

#define	RPMGIT_DIR	"/var/tmp/git"

static const char * exec_path;
static const char * html_path;
static const char * man_path;
static const char * info_path;
static int paginate;
static int no_replace_objects;
static int bare;
static const char * git_dir = RPMGIT_DIR "/.git";
static const char * work_tree = RPMGIT_DIR;

/*==============================================================*/
static int Xchkgit(/*@unused@*/ rpmgit git, const char * msg,
                int error, int printit,
                const char * func, const char * fn, unsigned ln)
{
    int rc = error;

    if (printit && rc) {
	const git_error * e = giterr_last();
	char * message = (e ? e->message : "");
	int klass = (e ? e->klass : -12345);
        rpmlog(RPMLOG_ERR, "%s:%s:%u: %s(%d): %s(%d)\n",
                func, fn, ln, msg, rc, message, klass);
    }

    return rc;
}
#define chkgit(_git, _msg, _error)  \
    Xchkgit(_git, _msg, _error, _rpmgit_debug, __FUNCTION__, __FILE__, __LINE__)

/*==============================================================*/
static poptContext
rpmgitPopt(int argc, char *argv[], struct poptOption * opts)
{
    static int _popt_flags = 0;
    poptContext con =
	poptGetContext(argv[0], argc, (const char **)argv, opts, _popt_flags);
    int rc;

    while ((rc = poptGetNextOpt(con)) > 0) {
	const char * arg = poptGetOptArg(con);
	arg = _free(arg);
    }
    if (rc < -1) {
        fprintf(stderr, "%s: %s: %s\n", argv[0],
                poptBadOption(con, POPT_BADOPTION_NOALIAS),
                poptStrerror(rc));
	con = poptFreeContext(con);
    }
assert(con);
    return con;
}

/*==============================================================*/
static int rpmgitToyFile(rpmgit git, const char * fn,
		const char * b, size_t nb)
{
    const char * workdir = git_repository_workdir(git->R);
    char * path = rpmGetPath(workdir, "/", fn, NULL);
    char * dn = dirname(xstrdup(path));
    int rc = rpmioMkpath(dn, 0755, (uid_t)-1, (gid_t)-1);
    FD_t fd;

    if (rc)
	goto exit;
    if (fn[strlen(fn)-1] == '/' || b == NULL)
	goto exit;

    if ((fd = Fopen(path, "w")) != NULL) {
	size_t nw = Fwrite(b, 1, nb, fd);
	rc = Fclose(fd);
assert(nw == nb);
    }

exit:
SPEW(0, rc, git);
    dn = _free(dn);
    path = _free(path);
    return rc;
}

/*==============================================================*/
#ifdef	REFERENCE
OPTIONS
       -q, --quiet
           Only print error and warning messages, all other output will be
           suppressed.

       --bare
           Create a bare repository. If GIT_DIR environment is not set, it is
           set to the current working directory.

       --template=<template_directory>
           Specify the directory from which templates will be used. (See the
           "TEMPLATE DIRECTORY" section below.)

       --shared[={false|true|umask|group|all|world|everybody|0xxx}]
           Specify that the git repository is to be shared amongst several
           users. This allows users belonging to the same group to push into
           that repository. When specified, the config variable
           "core.sharedRepository" is set so that files and directories under
           $GIT_DIR are created with the requested permissions. When not
           specified, git will use permissions reported by umask(2).

       The option can have the following values, defaulting to group if no
       value is given:

       ·    umask (or false): Use permissions reported by umask(2). The
           default, when --shared is not specified.

       ·    group (or true): Make the repository group-writable, (and g+sx,
           since the git group may be not the primary group of all users).
           This is used to loosen the permissions of an otherwise safe
           umask(2) value. Note that the umask still applies to the other
           permission bits (e.g. if umask is 0022, using group will not remove
           read privileges from other (non-group) users). See 0xxx for how to
           exactly specify the repository permissions.

       ·    all (or world or everybody): Same as group, but make the
           repository readable by all users.

       ·    0xxx: 0xxx is an octal number and each file will have mode 0xxx.
           0xxx will override users´ umask(2) value (and not only loosen
           permissions as group and all does).  0640 will create a repository
           which is group-readable, but not group-writable or accessible to
           others.  0660 will create a repo that is readable and writable to
           the current user and group, but inaccessible to others.

       By default, the configuration flag receive.denyNonFastForwards is
       enabled in shared repositories, so that you cannot force a non
       fast-forwarding push into it.

       If you name a (possibly non-existent) directory at the end of the
       command line, the command is run inside the directory (possibly after
       creating it).
#endif

static rpmRC cmd_init(int argc, char *argv[])
{
    unsigned init_bare = 0;
    const char * init_template = NULL;
    const char * init_shared = NULL;
    enum {
	_INIT_QUIET	= (1 << 0),
    };
    int init_flags = 0;
#define	INIT_ISSET(_a)	(init_flags & _INIT_##_a)
    struct poptOption initOpts[] = {
     { "quiet", 'q', POPT_ARG_VAL,		&init_flags, _INIT_QUIET,
	N_("Quiet mode."), NULL },
     { "bare", '\0', POPT_ARG_VAL,		&init_bare, 1,
	N_(""), NULL },
     { "template", '\0', POPT_ARG_STRING,	&init_template, 0,
	N_(""), N_("<template>") },
     { "shared", '\0', POPT_ARG_STRING,		&init_shared, 0,
	N_(""), N_("<template>") },
      POPT_TABLEEND
    };
    poptContext con = rpmgitPopt(argc, argv, initOpts);
    ARGV_t av = NULL;
    int ac = 0;
    rpmgit git = rpmgitNew(git_dir, 0);
    rpmRC rc = RPMRC_FAIL;
    int xx = -1;
    int i;

#ifdef	DYING
fprintf(stderr, "--> %s(%p[%d]) con %p init_flags %x\n", __FUNCTION__, argv, argc, con, init_flags);

argvPrint(__FUNCTION__, (ARGV_t)argv, NULL);
#endif
if (strcmp(argv[0], "init")) assert(0);
rpmgitPrintRepo(git, git->R, git->fp);

    xx = argvAppend(&av, (ARGV_t)poptGetArgs(con));
    ac = argvCount(av);

    /* Initialize a git repository. */
    xx = rpmgitInit(git);
    if (xx)
	goto exit;

    /* Read/print/save configuration info. */
    xx = rpmgitConfig(git);
    if (xx)
	goto exit;

    /* XXX automagic add and commit (for now) */
    if (ac <= 0)
	goto exit;

    /* Create file(s) in _workdir (if any). */
    for (i = 0; i < ac; i++) {
	const char * fn = av[i];
	struct stat sb;


	/* XXX Create non-existent files lazily. */
	if (Stat(fn, &sb) < 0)
	    xx = rpmgitToyFile(git, fn, fn, strlen(fn));

	/* Add the file to the repository. */
	xx = rpmgitAddFile(git, fn);
	if (xx)
	    goto exit;
    }

    /* Commit added files. */
    if (ac > 0) {
	static const char _msg[] = "WDJ commit";
	xx = rpmgitCommit(git, _msg);
    }
    if (xx)
	goto exit;
rpmgitPrintCommit(git, git->C, git->fp);

rpmgitPrintIndex(git->I, git->fp);
rpmgitPrintHead(git, NULL, git->fp);

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);
    init_shared = _free(init_shared);
    init_template = _free(init_template);

    av = argvFree(av);
    git = rpmgitFree(git);
    con = poptFreeContext(con);
    return rc;
}
#undef	INIT_ISSET

/*==============================================================*/
#ifdef	REFERENCE
OPTIONS
       <filepattern>...
           Files to add content from. Fileglobs (e.g.  *.c) can be given to
           add all matching files. Also a leading directory name (e.g.  dir to
           add dir/file1 and dir/file2) can be given to add all files in the
           directory, recursively.

       -n, --dry-run
           Don’t actually add the file(s), just show if they exist.

       -v, --verbose
           Be verbose.

       -f, --force
           Allow adding otherwise ignored files.

       -i, --interactive
           Add modified contents in the working tree interactively to the
           index. Optional path arguments may be supplied to limit operation
           to a subset of the working tree. See “Interactive mode” for
           details.

       -p, --patch
           Interactively choose hunks of patch between the index and the work
           tree and add them to the index. This gives the user a chance to
           review the difference before adding modified contents to the index.

           This effectively runs add --interactive, but bypasses the initial
           command menu and directly jumps to the patch subcommand. See
           “Interactive mode” for details.

       -e, --edit
           Open the diff vs. the index in an editor and let the user edit it.
           After the editor was closed, adjust the hunk headers and apply the
           patch to the index.

           NOTE: Obviously, if you change anything else than the first
           character on lines beginning with a space or a minus, the patch
           will no longer apply.

       -u, --update
           Only match <filepattern> against already tracked files in the index
           rather than the working tree. That means that it will never stage
           new files, but that it will stage modified new contents of tracked
           files and that it will remove files from the index if the
           corresponding files in the working tree have been removed.

           If no <filepattern> is given, default to "."; in other words,
           update all tracked files in the current directory and its
           subdirectories.

       -A, --all
           Like -u, but match <filepattern> against files in the working tree
           in addition to the index. That means that it will find new files as
           well as staging modified content and removing files that are no
           longer in the working tree.

       -N, --intent-to-add
           Record only the fact that the path will be added later. An entry
           for the path is placed in the index with no content. This is useful
           for, among other things, showing the unstaged content of such files
           with git diff and committing them with git commit -a.

       --refresh
           Don’t add the file(s), but only refresh their stat() information in
           the index.

       --ignore-errors
           If some files could not be added because of errors indexing them,
           do not abort the operation, but continue adding the others. The
           command shall still exit with non-zero status.

       --
           This option can be used to separate command-line options from the
           list of files, (useful when filenames might be mistaken for
           command-line options).

#endif

static rpmRC cmd_add(int argc, char *argv[])
{
    enum {
	_ADD_DRY_RUN		= (1 <<  0),
	_ADD_VERBOSE		= (1 <<  1),
	_ADD_FORCE		= (1 <<  2),
	_ADD_INTERACTIVE	= (1 <<  3),
	_ADD_PATCH		= (1 <<  4),
	_ADD_EDIT		= (1 <<  5),
	_ADD_UPDATE		= (1 <<  6),
	_ADD_ALL		= (1 <<  7),
	_ADD_INTENT_TO_ADD	= (1 <<  8),
	_ADD_REFRESH		= (1 <<  9),
	_ADD_IGNORE_ERRORS	= (1 << 10),
    };
    int add_flags = 0;
#define	ADD_ISSET(_a)	(add_flags & _ADD_##_a)
    struct poptOption addOpts[] = {
     { "dry-run", 'n', POPT_BIT_SET,		&add_flags, _ADD_DRY_RUN,
	N_(""), NULL },
     { "verbose", 'v', POPT_BIT_SET,		&add_flags, _ADD_VERBOSE,
	N_("Verbose mode."), NULL },
     { "force", 'f', POPT_BIT_SET,		&add_flags, _ADD_FORCE,
	N_(""), NULL },
     { "interactive", 'i', POPT_BIT_SET,	&add_flags, _ADD_INTERACTIVE,
	N_(""), NULL },
     { "patch", 'p', POPT_BIT_SET,		&add_flags, _ADD_PATCH,
	N_(""), NULL },
     { "edit", 'e', POPT_BIT_SET,		&add_flags, _ADD_EDIT,
	N_(""), NULL },
     { "update", 'u', POPT_BIT_SET,		&add_flags, _ADD_UPDATE,
	N_(""), NULL },
     { "all", 'A', POPT_BIT_SET,		&add_flags, _ADD_ALL,
	N_(""), NULL },
     { "intent-to-add", 'N', POPT_BIT_SET,	&add_flags, _ADD_INTENT_TO_ADD,
	N_(""), NULL },
     { "refresh", '\0', POPT_BIT_SET,		&add_flags, _ADD_REFRESH,
	N_(""), NULL },
     { "ignore-errors", '\0', POPT_BIT_SET,	&add_flags, _ADD_IGNORE_ERRORS,
	N_(""), NULL },
      POPT_TABLEEND
    };
    poptContext con = rpmgitPopt(argc, argv, addOpts);
    ARGV_t av = NULL;
    int ac = 0;
    rpmgit git = rpmgitNew(git_dir, 0);
    rpmRC rc = RPMRC_FAIL;
    int xx = -1;
    int i;

#ifdef	DYING
fprintf(stderr, "--> %s(%p[%d]) con %p init_flags %x\n", __FUNCTION__, argv, argc, con, add_flags);

argvPrint(__FUNCTION__, (ARGV_t)argv, NULL);
#endif
if (strcmp(argv[0], "add")) assert(0);
rpmgitPrintRepo(git, git->R, git->fp);

    xx = argvAppend(&av, (ARGV_t)poptGetArgs(con));
    ac = argvCount(av);

    /* XXX Get the index file for this repository. */
    xx = chkgit(git, "git_repository_index",
		git_repository_index((git_index **)&git->I, git->R));
    if (xx)
	goto exit;

if (_rpmgit_debug < 0) rpmgitPrintIndex(git->I, git->fp);
    /* Create file(s) in _workdir (if any). */
    for (i = 0; i < ac; i++) {
	const char * fn = av[i];
	struct stat sb;

	/* XXX Create non-existent files lazily. */
	if (Stat(fn, &sb) < 0)
	    xx = rpmgitToyFile(git, fn, fn, strlen(fn));

	/* Add the file to the repository. */
	xx = rpmgitAddFile(git, fn);
	if (xx)
	    goto exit;
    }
if (_rpmgit_debug < 0) rpmgitPrintIndex(git->I, git->fp);

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);

    av = argvFree(av);
    git = rpmgitFree(git);
    con = poptFreeContext(con);
    return rc;
}
#undef	ADD_ISSET

/*==============================================================*/
#ifdef	REFERENCE
OPTIONS
       -a, --all
           Tell the command to automatically stage files that have been
           modified and deleted, but new files you have not told git about are
           not affected.

       -C <commit>, --reuse-message=<commit>
           Take an existing commit object, and reuse the log message and the
           authorship information (including the timestamp) when creating the
           commit.

       -c <commit>, --reedit-message=<commit>
           Like -C, but with -c the editor is invoked, so that the user can
           further edit the commit message.

       --reset-author
           When used with -C/-c/--amend options, declare that the authorship
           of the resulting commit now belongs of the committer. This also
           renews the author timestamp.

       --short
           When doing a dry-run, give the output in the short-format. See git-
           status(1) for details. Implies --dry-run.

       --porcelain
           When doing a dry-run, give the output in a porcelain-ready format.
           See git-status(1) for details. Implies --dry-run.

       -z
           When showing short or porcelain status output, terminate entries in
           the status output with NUL, instead of LF. If no format is given,
           implies the --porcelain output format.

       -F <file>, --file=<file>
           Take the commit message from the given file. Use - to read the
           message from the standard input.

       --author=<author>
           Override the author name used in the commit. You can use the
           standard A U Thor <author@example.com[1]> format. Otherwise, an
           existing commit that matches the given string and its author name
           is used.

       --date=<date>
           Override the author date used in the commit.

       -m <msg>, --message=<msg>
           Use the given <msg> as the commit message.

       -t <file>, --template=<file>
           Use the contents of the given file as the initial version of the
           commit message. The editor is invoked and you can make subsequent
           changes. If a message is specified using the -m or -F options, this
           option has no effect. This overrides the commit.template
           configuration variable.

       -s, --signoff
           Add Signed-off-by line by the committer at the end of the commit
           log message.

       -n, --no-verify
           This option bypasses the pre-commit and commit-msg hooks. See also
           githooks(5).

       --allow-empty
           Usually recording a commit that has the exact same tree as its sole
           parent commit is a mistake, and the command prevents you from
           making such a commit. This option bypasses the safety, and is
           primarily for use by foreign scm interface scripts.

       --cleanup=<mode>
           This option sets how the commit message is cleaned up. The <mode>
           can be one of verbatim, whitespace, strip, and default. The default
           mode will strip leading and trailing empty lines and #commentary
           from the commit message only if the message is to be edited.
           Otherwise only whitespace removed. The verbatim mode does not
           change message at all, whitespace removes just leading/trailing
           whitespace lines and strip removes both whitespace and commentary.

       -e, --edit
           The message taken from file with -F, command line with -m, and from
           file with -C are usually used as the commit log message unmodified.
           This option lets you further edit the message taken from these
           sources.

       --amend
           Used to amend the tip of the current branch. Prepare the tree
           object you would want to replace the latest commit as usual (this
           includes the usual -i/-o and explicit paths), and the commit log
           editor is seeded with the commit message from the tip of the
           current branch. The commit you create replaces the current tip — if
           it was a merge, it will have the parents of the current tip as
           parents — so the current top commit is discarded.

           It is a rough equivalent for:

                       $ git reset --soft HEAD^
                       $ ... do something else to come up with the right tree ...
                       $ git commit -c ORIG_HEAD

           but can be used to amend a merge commit.

           You should understand the implications of rewriting history if you
           amend a commit that has already been published. (See the
           "RECOVERING FROM UPSTREAM REBASE" section in git-rebase(1).)

       -i, --include
           Before making a commit out of staged contents so far, stage the
           contents of paths given on the command line as well. This is
           usually not what you want unless you are concluding a conflicted
           merge.

       -o, --only
           Make a commit only from the paths specified on the command line,
           disregarding any contents that have been staged so far. This is the
           default mode of operation of git commit if any paths are given on
           the command line, in which case this option can be omitted. If this
           option is specified together with --amend, then no paths need to be
           specified, which can be used to amend the last commit without
           committing changes that have already been staged.

       -u[<mode>], --untracked-files[=<mode>]
           Show untracked files (Default: all).

           The mode parameter is optional, and is used to specify the handling
           of untracked files.

           The possible options are:

           ·    no - Show no untracked files

           ·    normal - Shows untracked files and directories

           ·    all - Also shows individual files in untracked directories.

               See git-config(1) for configuration variable used to change the
               default for when the option is not specified.

       -v, --verbose
           Show unified diff between the HEAD commit and what would be
           committed at the bottom of the commit message template. Note that
           this diff output doesn’t have its lines prefixed with #.

       -q, --quiet
           Suppress commit summary message.

       --dry-run
           Do not create a commit, but show a list of paths that are to be
           committed, paths with local changes that will be left uncommitted
           and paths that are untracked.

       --status
           Include the output of git-status(1) in the commit message template
           when using an editor to prepare the commit message. Defaults to on,
           but can be used to override configuration variable commit.status.

       --no-status
           Do not include the output of git-status(1) in the commit message
           template when using an editor to prepare the default commit
           message.

       --
           Do not interpret any more arguments as options.

       <file>...
           When files are given on the command line, the command commits the
           contents of the named files, without recording the changes already
           staged. The contents of these files are also staged for the next
           commit on top of what have been staged before.

#endif

static rpmRC cmd_commit(int argc, char *argv[])
{
    const char * commit_file = NULL;		/* XXX argv? */
    const char * commit_author = NULL;
    const char * commit_date = NULL;
    const char * commit_msg = NULL;
    const char * commit_template = NULL;
    const char * commit_cleanup = NULL;
    const char * commit_untracked = NULL;
    enum {
	_COMMIT_ALL		= (1 <<  0),
	_COMMIT_RESET_AUTHOR	= (1 <<  1),
	_COMMIT_SHORT		= (1 <<  2),
	_COMMIT_PORCELAIN	= (1 <<  3),
	_COMMIT_ZERO		= (1 <<  4),
	_COMMIT_SIGNOFF		= (1 <<  5),
	_COMMIT_NO_VERIFY	= (1 <<  6),
	_COMMIT_ALLOW_EMPTY	= (1 <<  7),
	_COMMIT_EDIT		= (1 <<  8),
	_COMMIT_AMEND		= (1 <<  9),
	_COMMIT_INCLUDE		= (1 << 10),
	_COMMIT_ONLY		= (1 << 11),
	_COMMIT_VERBOSE		= (1 << 12),
	_COMMIT_QUIET		= (1 << 13),
	_COMMIT_DRY_RUN		= (1 << 14),
	_COMMIT_STATUS		= (1 << 15),
	_COMMIT_NO_STATUS	= (1 << 16),
    };
    int commit_flags = 0;
#define	COMMIT_ISSET(_a)	(commit_flags & _COMMIT_##_a)
    struct poptOption commitOpts[] = {
     { "all", 'a', POPT_BIT_SET,		&commit_flags, _COMMIT_ALL,
	N_(""), NULL },
	/* XXX -C */
	/* XXX -c */
     { "reset-author", '\0', POPT_BIT_SET,	&commit_flags, _COMMIT_RESET_AUTHOR,
	N_(""), NULL },
     { "short", '\0', POPT_BIT_SET,		&commit_flags, _COMMIT_SHORT,
	N_(""), NULL },
     { "porcelain", '\0', POPT_BIT_SET,		&commit_flags, _COMMIT_PORCELAIN,
	N_(""), NULL },
     { NULL, 'z', POPT_BIT_SET,			&commit_flags, _COMMIT_ZERO,
	N_(""), NULL },
     { "file", 'F', POPT_ARG_STRING,		&commit_file, 0,
	N_(""), NULL },
     { "author", '\0', POPT_ARG_STRING,		&commit_author, 0,
	N_(""), NULL },
     { "date", '\0', POPT_ARG_STRING,		&commit_date, 0,
	N_(""), NULL },
     { "message", 'm', POPT_ARG_STRING,		&commit_msg, 0,
	N_(""), NULL },
     { "template", 't', POPT_ARG_STRING,	&commit_template, 0,
	N_(""), NULL },
     { "signoff", 's', POPT_BIT_SET,		&commit_flags, _COMMIT_SIGNOFF,
	N_(""), NULL },
     { "no-verify", 'n', POPT_BIT_SET,		&commit_flags, _COMMIT_NO_VERIFY,
	N_(""), NULL },
     { "allow-empty", '\0', POPT_BIT_SET,	&commit_flags, _COMMIT_ALLOW_EMPTY,
	N_(""), NULL },
     { "cleanup", '\0', POPT_ARG_STRING,	&commit_cleanup, 0,
	N_(""), NULL },
     { "edit", 'e', POPT_BIT_SET,		&commit_flags, _COMMIT_EDIT,
	N_(""), NULL },
     { "amend", '\0', POPT_BIT_SET,		&commit_flags, _COMMIT_AMEND,
	N_(""), NULL },
     { "include", 'i', POPT_BIT_SET,		&commit_flags, _COMMIT_INCLUDE,
	N_(""), NULL },
     { "only", 'o', POPT_BIT_SET,		&commit_flags, _COMMIT_ONLY,
	N_(""), NULL },
     { "untracked", 'u', POPT_ARG_STRING,	&commit_untracked, 0,
	N_(""), NULL },
     { "verbose", 'v', POPT_ARG_VAL,		&commit_flags, _COMMIT_VERBOSE,
	N_("Verbose mode."), NULL },
     { "quiet", 'q', POPT_ARG_VAL,		&commit_flags, _COMMIT_QUIET,
	N_("Quiet mode."), NULL },
     { "dry-run", '\n', POPT_BIT_SET,		&commit_flags, _COMMIT_DRY_RUN,
	N_(""), NULL },

     { "status", '\0', POPT_ARG_VAL,		&commit_flags, _COMMIT_STATUS,
	N_(""), NULL },
     { "no-status", '\0', POPT_ARG_VAL,		&commit_flags, _COMMIT_NO_STATUS,
	N_(""), NULL },

      POPT_TABLEEND
    };
    poptContext con = rpmgitPopt(argc, argv, commitOpts);
    ARGV_t av = NULL;
    int ac = 0;
    rpmgit git = rpmgitNew(git_dir, 0);
    rpmRC rc = RPMRC_FAIL;
    int xx = -1;

fprintf(stderr, "--> %s(%p[%d]) con %p commit_flags %x\n", __FUNCTION__, argv, argc, con, commit_flags);

#ifdef	DYING
argvPrint(__FUNCTION__, (ARGV_t)argv, NULL);
#endif
if (strcmp(argv[0], "commit")) assert(0);
rpmgitPrintRepo(git, git->R, git->fp);

    xx = argvAppend(&av, (ARGV_t)poptGetArgs(con));
    ac = argvCount(av);

    /* XXX Get the index file for this repository. */
    xx = chkgit(git, "git_repository_index",
		git_repository_index((git_index **)&git->I, git->R));
    if (xx)
	goto exit;

    /* Commit changes. */
    if (commit_msg == NULL) commit_msg = xstrdup("WDJ commit");	/* XXX */
    xx = rpmgitCommit(git, commit_msg);
    if (xx)
	goto exit;
rpmgitPrintCommit(git, git->C, git->fp);

rpmgitPrintIndex(git->I, git->fp);
rpmgitPrintHead(git, NULL, git->fp);

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);
    commit_file = _free(commit_file);		/* XXX argv? */
    commit_author = _free(commit_author);
    commit_date = _free(commit_date);
    commit_msg = _free(commit_msg);
    commit_template = _free(commit_template);
    commit_cleanup = _free(commit_cleanup);
    commit_untracked = _free(commit_untracked);

    av = argvFree(av);
    git = rpmgitFree(git);
    con = poptFreeContext(con);
    return rc;
}

/*==============================================================*/
static int resolve_to_tree(git_repository * repo, const char *identifier,
		    git_tree ** tree)
{
    int err = 0;
    size_t len = strlen(identifier);
    git_oid oid;
    git_object *obj = NULL;

    /* try to resolve as OID */
    if (git_oid_fromstrn(&oid, identifier, len) == 0)
	git_object_lookup_prefix(&obj, repo, &oid, len, GIT_OBJ_ANY);

    /* try to resolve as reference */
    if (obj == NULL) {
	git_reference *ref, *resolved;
	if (git_reference_lookup(&ref, repo, identifier) == 0) {
	    git_reference_resolve(&resolved, ref);
	    git_reference_free(ref);
	    if (resolved) {
		git_object_lookup(&obj, repo, git_reference_oid(resolved),
				  GIT_OBJ_ANY);
		git_reference_free(resolved);
	    }
	}
    }

    if (obj == NULL)
	return GIT_ENOTFOUND;

    switch (git_object_type(obj)) {
    case GIT_OBJ_TREE:
	*tree = (git_tree *) obj;
	break;
    case GIT_OBJ_COMMIT:
	err = git_commit_tree(tree, (git_commit *) obj);
	git_object_free(obj);
	break;
    default:
	err = GIT_ENOTFOUND;
    }

    return err;
}

static char *colors[] = {
    "\033[m",			/* reset */
    "\033[1m",			/* bold */
    "\033[31m",			/* red */
    "\033[32m",			/* green */
    "\033[36m"			/* cyan */
};

static int printer(void *data,
		git_diff_delta *delta,
		git_diff_range *range,
		char usage,
		const char *line,
		size_t line_len)
{
    int *last_color = data, color = 0;

    if (*last_color >= 0) {
	switch (usage) {
	case GIT_DIFF_LINE_ADDITION:
	    color = 3;
	    break;
	case GIT_DIFF_LINE_DELETION:
	    color = 2;
	    break;
	case GIT_DIFF_LINE_ADD_EOFNL:
	    color = 3;
	    break;
	case GIT_DIFF_LINE_DEL_EOFNL:
	    color = 2;
	    break;
	case GIT_DIFF_LINE_FILE_HDR:
	    color = 1;
	    break;
	case GIT_DIFF_LINE_HUNK_HDR:
	    color = 4;
	    break;
	default:
	    color = 0;
	}
	if (color != *last_color) {
	    if (*last_color == 1 || color == 1)
		fputs(colors[0], stdout);
	    fputs(colors[color], stdout);
	    *last_color = color;
	}
    }

    fputs(line, stdout);
    return 0;
}

#ifdef	REFERENCE
OPTIONS
       -p, -u
           Generate patch (see section on generating patches). This is the
           default.

       -U<n>, --unified=<n>
           Generate diffs with <n> lines of context instead of the usual
           three. Implies -p.

       --raw
           Generate the raw format.

       --patch-with-raw
           Synonym for -p --raw.

       --patience
           Generate a diff using the "patience diff" algorithm.

       --stat[=width[,name-width]]
           Generate a diffstat. You can override the default output width for
           80-column terminal by --stat=width. The width of the filename part
           can be controlled by giving another width to it separated by a
           comma.

       --numstat
           Similar to --stat, but shows number of added and deleted lines in
           decimal notation and pathname without abbreviation, to make it more
           machine friendly. For binary files, outputs two - instead of saying
           0 0.

       --shortstat
           Output only the last line of the --stat format containing total
           number of modified files, as well as number of added and deleted
           lines.

       --dirstat[=limit]
           Output the distribution of relative amount of changes (number of
           lines added or removed) for each sub-directory. Directories with
           changes below a cut-off percent (3% by default) are not shown. The
           cut-off percent can be set with --dirstat=limit. Changes in a child
           directory is not counted for the parent directory, unless
           --cumulative is used.

       --dirstat-by-file[=limit]
           Same as --dirstat, but counts changed files instead of lines.

       --summary
           Output a condensed summary of extended header information such as
           creations, renames and mode changes.

       --patch-with-stat
           Synonym for -p --stat.

       -z
           When --raw, --numstat, --name-only or --name-status has been given,
           do not munge pathnames and use NULs as output field terminators.

           Without this option, each pathname output will have TAB, LF, double
           quotes, and backslash characters replaced with \t, \n, \"", and \\,
           respectively, and the pathname will be enclosed in double quotes if
           any of those replacements occurred.

       --name-only
           Show only names of changed files.

       --name-status
           Show only names and status of changed files. See the description of
           the --diff-filter option on what the status letters mean.

       --submodule[=<format>]
           Chose the output format for submodule differences. <format> can be
           one of short and log.  short just shows pairs of commit names, this
           format is used when this option is not given.  log is the default
           value for this option and lists the commits in that commit range
           like the summary option of git-submodule(1) does.

       --color[=<when>]
           Show colored diff. The value must be always (the default), never,
           or auto.

       --no-color
           Turn off colored diff, even when the configuration file gives the
           default to color output. Same as --color=never.

       --color-words[=<regex>]
           Show colored word diff, i.e., color words which have changed. By
           default, words are separated by whitespace.

           When a <regex> is specified, every non-overlapping match of the
           <regex> is considered a word. Anything between these matches is
           considered whitespace and ignored(!) for the purposes of finding
           differences. You may want to append |[^[:space:]] to your regular
           expression to make sure that it matches all non-whitespace
           characters. A match that contains a newline is silently
           truncated(!) at the newline.

           The regex can also be set via a diff driver or configuration
           option, see gitattributes(1) or git-config(1). Giving it explicitly
           overrides any diff driver or configuration setting. Diff drivers
           override configuration settings.

       --no-renames
           Turn off rename detection, even when the configuration file gives
           the default to do so.

       --check
           Warn if changes introduce trailing whitespace or an indent that
           uses a space before a tab. Exits with non-zero status if problems
           are found. Not compatible with --exit-code.

       --full-index
           Instead of the first handful of characters, show the full pre- and
           post-image blob object names on the "index" line when generating
           patch format output.

       --binary
           In addition to --full-index, output a binary diff that can be
           applied with git-apply.

       --abbrev[=<n>]
           Instead of showing the full 40-byte hexadecimal object name in
           diff-raw format output and diff-tree header lines, show only a
           partial prefix. This is independent of the --full-index option
           above, which controls the diff-patch output format. Non default
           number of digits can be specified with --abbrev=<n>.

       -B
           Break complete rewrite changes into pairs of delete and create.

       -M
           Detect renames.

       -C
           Detect copies as well as renames. See also --find-copies-harder.

       --diff-filter=[ACDMRTUXB*]
           Select only files that are Added (A), Copied (C), Deleted (D),
           Modified (M), Renamed (R), have their type (i.e. regular file,
           symlink, submodule, ...) changed (T), are Unmerged (U), are Unknown
           (X), or have had their pairing Broken (B). Any combination of the
           filter characters may be used. When * (All-or-none) is added to the
           combination, all paths are selected if there is any file that
           matches other criteria in the comparison; if there is no file that
           matches other criteria, nothing is selected.

       --find-copies-harder
           For performance reasons, by default, -C option finds copies only if
           the original file of the copy was modified in the same changeset.
           This flag makes the command inspect unmodified files as candidates
           for the source of copy. This is a very expensive operation for
           large projects, so use it with caution. Giving more than one -C
           option has the same effect.

       -l<num>
           The -M and -C options require O(n^2) processing time where n is the
           number of potential rename/copy targets. This option prevents
           rename/copy detection from running if the number of rename/copy
           targets exceeds the specified number.

       -S<string>
           Look for differences that introduce or remove an instance of
           <string>. Note that this is different than the string simply
           appearing in diff output; see the pickaxe entry in gitdiffcore(7)
           for more details.

       --pickaxe-all
           When -S finds a change, show all the changes in that changeset, not
           just the files that contain the change in <string>.

       --pickaxe-regex
           Make the <string> not a plain string but an extended POSIX regex to
           match.

       -O<orderfile>
           Output the patch in the order specified in the <orderfile>, which
           has one shell glob pattern per line.

       -R
           Swap two inputs; that is, show differences from index or on-disk
           file to tree contents.

       --relative[=<path>]
           When run from a subdirectory of the project, it can be told to
           exclude changes outside the directory and show pathnames relative
           to it with this option. When you are not in a subdirectory (e.g. in
           a bare repository), you can name which subdirectory to make the
           output relative to by giving a <path> as an argument.

       -a, --text
           Treat all files as text.

       --ignore-space-at-eol
           Ignore changes in whitespace at EOL.

       -b, --ignore-space-change
           Ignore changes in amount of whitespace. This ignores whitespace at
           line end, and considers all other sequences of one or more
           whitespace characters to be equivalent.

       -w, --ignore-all-space
           Ignore whitespace when comparing lines. This ignores differences
           even if one line has whitespace where the other line has none.

       --inter-hunk-context=<lines>
           Show the context between diff hunks, up to the specified number of
           lines, thereby fusing hunks that are close to each other.

       --exit-code
           Make the program exit with codes similar to diff(1). That is, it
           exits with 1 if there were differences and 0 means no differences.

       --quiet
           Disable all output of the program. Implies --exit-code.

       --ext-diff
           Allow an external diff helper to be executed. If you set an
           external diff driver with gitattributes(5), you need to use this
           option with git-log(1) and friends.

       --no-ext-diff
           Disallow external diff drivers.

       --ignore-submodules
           Ignore changes to submodules in the diff generation.

       --src-prefix=<prefix>
           Show the given source prefix instead of "a/".

       --dst-prefix=<prefix>
           Show the given destination prefix instead of "b/".

       --no-prefix
           Do not show any source or destination prefix.
#endif
static rpmRC cmd_diff(int argc, char *argv[])
{
    git_diff_options opts = { 0, 0, 0, NULL, NULL, { NULL, 0} };
    int color = -1;
    int compact = 0;
    int cached = 0;
    enum {	/* XXX FIXME */
	_DIFF_ALL		= (1 <<  0),
	_DIFF_ZERO		= (1 <<  1),
    };
    int diff_flags = 0;
#define	DIFF_ISSET(_a)	(opts.flags & GIT_DIFF_##_a)
    struct poptOption diffOpts[] = {
	/* XXX -u */
     { "patch", 'p', POPT_ARG_VAL,			&compact, 0,
	N_("Generate patch."), NULL },
     { "unified", 'U', POPT_ARG_SHORT,	&opts.context_lines, 0,
	N_("Generate diffs with <n> lines of context."), N_("<n>") },
	/* XXX --raw */
	/* XXX --patch-with-raw */
	/* XXX --patience */
	/* XXX --stat */
	/* XXX --numstat */
	/* XXX --shortstat */
	/* XXX --dirstat */
	/* XXX --dirstat-by-file */
	/* XXX --summary */
	/* XXX --patch-with-stat */
     { NULL, 'z', POPT_BIT_SET,			&diff_flags, _DIFF_ZERO,
	N_(""), NULL },
	/* XXX --name-only */
     { "name-status", '\0', POPT_ARG_VAL,		&compact, 1,
	N_("Show only names and status of changed files."), NULL },
	/* XXX --submodule */
     { "color", '\0', POPT_ARG_VAL,			&color, 0,
	N_("Show colored diff."), NULL },
     { "no-color", '\0', POPT_ARG_VAL,		&color, -1,
	N_("Turn off colored diff."), NULL },
	/* XXX --color-words */
	/* XXX --no-renames */
	/* XXX --check */
	/* XXX --full-index */
	/* XXX --binary */
	/* XXX --abbrev */
	/* XXX -B */
	/* XXX -M */
	/* XXX -C */
	/* XXX --diff-filter */
	/* XXX --find-copies-harder */
	/* XXX -l */
	/* XXX -S */
	/* XXX --pickaxe-all */
	/* XXX --pickaxe-regex */
	/* XXX -O */
     { NULL, 'R', POPT_BIT_SET,		&opts.flags, GIT_DIFF_REVERSE,
	N_("Swap two inputs."), NULL },
	/* XXX --relative */
     { "text", 'a', POPT_BIT_SET,		&opts.flags, GIT_DIFF_FORCE_TEXT,
	N_("Treat all files as text."), NULL },
     { "ignore-space-at-eol", '\0',	POPT_BIT_SET, &opts.flags, GIT_DIFF_IGNORE_WHITESPACE_EOL,
	N_("Ignore changes in whitespace at EOL."), NULL },
     { "ignore-space-change", 'b',	POPT_BIT_SET, &opts.flags, GIT_DIFF_IGNORE_WHITESPACE_CHANGE,
	N_("Ignore changes in amount of whitespace."), NULL },
     { "ignore-all-space", 'w', POPT_BIT_SET, &opts.flags, GIT_DIFF_IGNORE_WHITESPACE,
	N_("Ignore whitespace when comparing lines."), NULL },
     { "inter-hunk-context", '\0', POPT_ARG_SHORT,	&opts.interhunk_lines, 0,
	N_("Show the context between diff hunks."), N_("<lines>") },
	/* XXX --exit-code */
	/* XXX --quiet */
	/* XXX --ext-diff */
	/* XXX --no-ext-diff */
	/* XXX --ignore-submodules */
#ifdef	NOTYET
     { "src-prefix", '\0', POPT_ARG_STRING,	&opts.src_prefix, 0,
	N_("Show the given source <prefix> instead of \"a/\"."), N_("<prefix>") },
     { "dst-prefix", '\0', POPT_ARG_STRING,	&opts.dst_prefix, 0,
	N_("Show the given destination prefix instead of \"b/\"."), N_("<prefix>") },
#endif
	/* XXX --no-prefix */

     { "cached", '\0', POPT_ARG_VAL,		&cached, 1,
	NULL, NULL },
     { "ignored", '\0', POPT_BIT_SET,	&opts.flags, GIT_DIFF_INCLUDE_IGNORED,
	NULL, NULL },
     { "untracked", '\0', POPT_BIT_SET,	&opts.flags, GIT_DIFF_INCLUDE_UNTRACKED,
	NULL, NULL },

      POPT_TABLEEND
    };
    poptContext con = rpmgitPopt(argc, argv, diffOpts);
    ARGV_t av = NULL;
    int ac = 0;
    rpmgit git = NULL;		/* XXX rpmgitNew(git_dir, 0); */
    rpmRC rc = RPMRC_FAIL;
git_diff_list * diff = NULL;
const char * treeish1 = NULL;
git_tree *t1 = NULL;
const char * treeish2 = NULL;
git_tree *t2 = NULL;
    int xx = -1;

if (strcmp(argv[0], "diff")) assert(0);

#ifdef	NOTYET
const char * dir = ".";
char path[GIT_PATH_MAX];
const char * fn;
    xx = chkgit(git, "git_repository_discover",
	git_repository_discover(path, sizeof(path), dir, 0, "/"));
    if (xx) {
	fprintf(stderr, "Could not discover repository\n");
	goto exit;
    }
    fn = path;
#endif
    git = rpmgitNew(git_dir, 0);

    xx = argvAppend(&av, (ARGV_t)poptGetArgs(con));
    ac = argvCount(av);

    treeish1 = (ac >= 1 ? av[0] : NULL);
    treeish2 = (ac >= 2 ? av[1] : NULL);

    if (treeish1) {
	xx = chkgit(git, "resolve_to_tree",
		resolve_to_tree(git->R, treeish1, &t1));
	if (xx) {
	    fprintf(stderr, "Looking up first tree\n");
	    goto exit;
	}
    }
    if (treeish2) {
	xx = chkgit(git, "resolve_to_tree",
		resolve_to_tree(git->R, treeish2, &t2));
	if (xx) {
	    fprintf(stderr, "Looking up second tree\n");
	    goto exit;
	}
    }

    /* <sha1> <sha2> */
    /* <sha1> --cached */
    /* <sha1> */
    /* --cached */
    /* nothing */

    if (t1 && t2)
	xx = chkgit(git, "git_diff_tree_to_tree",
		git_diff_tree_to_tree(git->R, &opts, t1, t2, &diff));
    else if (t1 && cached)
	xx = chkgit(git, "git_diff_index_to_tree",
		git_diff_index_to_tree(git->R, &opts, t1, &diff));
    else if (t1) {
	git_diff_list *diff2;
	xx = chkgit(git, "git_diff_index_to_tree",
		git_diff_index_to_tree(git->R, &opts, t1, &diff));
	xx = chkgit(git, "git_diff_workdir_to_index",
		git_diff_workdir_to_index(git->R, &opts, &diff2));
	xx = chkgit(git, "git_diff_merge",
		git_diff_merge(diff, diff2));
	git_diff_list_free(diff2);
    }
    else if (cached) {
	xx = chkgit(git, "resolve_to_tree",
		resolve_to_tree(git->R, "HEAD", &t1));
	xx = chkgit(git, "git_diff_index_to_tree",
		git_diff_index_to_tree(git->R, &opts, t1, &diff));
    }
    else
	xx = chkgit(git, "git_diff_workdir_to_index",
		git_diff_workdir_to_index(git->R, &opts, &diff));

    if (color >= 0)
	fputs(colors[0], stdout);

    if (compact)
	xx = chkgit(git, "git_diff_print_compact",
		git_diff_print_compact(diff, &color, printer));
    else
	xx = chkgit(git, "git_diff_print_patch",
		git_diff_print_patch(diff, &color, printer));

    if (color >= 0)
	fputs(colors[0], stdout);

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);

    if (diff)
	git_diff_list_free(diff);
    if (t1)
	git_tree_free(t1);
    if (t2)
	git_tree_free(t2);

    av = argvFree(av);
    git = rpmgitFree(git);
    con = poptFreeContext(con);
    return rc;
}
#undef	DIFF_ISSET

/*==============================================================*/

static int status_cb(const char *path, unsigned int status, void *data)
{
    FILE * fp = stdout;
    rpmgit git = (rpmgit) data;
    int state = git->state;
    int rc = 0;

    if (!(state & 0x1)) {
	fprintf(fp,	"# On branch master\n"
			"#\n"
			"# Initial commit\n");
	state |= 0x1;
    }

#define	_INDEX_MASK \
    (GIT_STATUS_INDEX_NEW|GIT_STATUS_INDEX_MODIFIED|GIT_STATUS_INDEX_DELETED)
#define	_WT_MASK \
    (                  GIT_STATUS_WT_MODIFIED|GIT_STATUS_WT_DELETED)
    if (status & _INDEX_MASK) {
	if (!(state & 0x2)) {
	    fprintf(fp,	"#\n"
			"# Changes to be committed:\n"
			"#   (use \"git rm --cached <file>...\" to unstage)\n"
			"#\n");
	    state |= 0x2;
	}
	if (status & GIT_STATUS_INDEX_NEW)
	    fprintf(fp, "#	new file:   %s\n", path);
	if (status & GIT_STATUS_INDEX_MODIFIED)
	    fprintf(fp, "#	?M? file:   %s\n", path);
	if (status & GIT_STATUS_INDEX_DELETED)
	    fprintf(fp, "#	?D? file:   %s\n", path);
    } else
    if (status & _WT_MASK) {
	if (!(state & 0x4)) {
	    fprintf(fp,	"#\n"
			"# Changed but not updated:\n"
			"#   (use \"git add/rm <file>...\" to update what will be committed)\n"
			"#   (use \"git checkout -- <file>...\" to discard changes in working directory)\n"
			"#\n");
	    state |= 0x4;
	}
	if (status & GIT_STATUS_WT_MODIFIED)
	    fprintf(fp, "#	modified:   %s\n", path);
	else
	if (status & GIT_STATUS_WT_DELETED)
	    fprintf(fp, "#	deleted:    %s\n", path);
    } else
    if (status & GIT_STATUS_WT_NEW) {
	if (!(state & 0x8)) {
	    fprintf(fp,
			"#\n"
			"# Untracked files:\n"
			"#   (use \"git add <file>...\" to include in what will be committed)\n"
			"#\n");
	    state |= 0x8;
	}
	fprintf(fp, "#	%s\n", path);
    }

    git->state = state;

    return rc;
}

static int status_porcelain_cb(const char *path, unsigned int status, void *data)
{
    FILE * fp = stdout;
    rpmgit git = (rpmgit) data;
    int state = git->state;
    char Istatus = '?';
    char Wstatus = ' ';
    int rc = 0;

    if (status & GIT_STATUS_INDEX_NEW)
	Istatus = 'A';
    else if (status & GIT_STATUS_INDEX_MODIFIED)
	Istatus = 'M';	/* XXX untested */
    else if (status & GIT_STATUS_INDEX_DELETED)
	Istatus = 'D';	/* XXX untested */

    if (status & GIT_STATUS_WT_NEW)
	Wstatus = '?';
    else if (status & GIT_STATUS_WT_MODIFIED)
	Wstatus = 'M';
    else if (status & GIT_STATUS_WT_DELETED)
	Wstatus = 'D';
fprintf(fp, "%c%c %s\n", Istatus, Wstatus, path);

    git->state = rc;
    return rc;
}

#ifdef	REFERENCE
OPTIONS
       -s, --short
           Give the output in the short-format.

       --porcelain
           Give the output in a stable, easy-to-parse format for scripts.
           Currently this is identical to --short output, but is guaranteed
           not to change in the future, making it safe for scripts.

       -u[<mode>], --untracked-files[=<mode>]
           Show untracked files (Default: all).

           The mode parameter is optional, and is used to specify the handling
           of untracked files. The possible options are:

           ·    no - Show no untracked files

           ·    normal - Shows untracked files and directories

           ·    all - Also shows individual files in untracked directories.
               See git-config(1) for configuration variable used to change the
               default for when the option is not specified.

           -z
               Terminate entries with NUL, instead of LF. This implies the
               --porcelain output format if no other format is given.

#endif

static rpmRC cmd_status(int argc, char *argv[])
{
    git_status_options opts = { 0, 0, { NULL, 0} };
    const char * status_untracked_files = xstrdup("all");
    enum {
	_STATUS_SHORT		= (1 <<  0),
	_STATUS_PORCELAIN	= (1 <<  1),
	_STATUS_ZERO		= (1 <<  2),
    };
    int status_flags = 0;
#define	STATUS_ISSET(_a)	(status_flags & _STATUS_##_a)
    struct poptOption statusOpts[] = {
     { "short", 's', POPT_BIT_SET,		&status_flags, _STATUS_SHORT,
	N_("Give the output in the short-format."), NULL },
     { "porcelain", '\0', POPT_BIT_SET,		&status_flags, _STATUS_PORCELAIN,
	N_("Give the output in a stable, easy-to-parse format for scripts."), NULL },
     { "untracked-files", 'u', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT,	&status_untracked_files, 0,
	N_("Show untracked files."), N_("no|normal|all") },
     { NULL, 'z', POPT_BIT_SET,	&status_flags, _STATUS_ZERO,
	N_(""), NULL },
      POPT_TABLEEND
    };
    poptContext con = rpmgitPopt(argc, argv, statusOpts);
    ARGV_t av = NULL;
    int ac = 0;
    rpmgit git = rpmgitNew(git_dir, 0);
    rpmRC rc = RPMRC_FAIL;
    int xx = -1;

if (strcmp(argv[0], "status")) assert(0);

    xx = argvAppend(&av, (ARGV_t)poptGetArgs(con));
    ac = argvCount(av);

    /* XXX popt wiring */
    opts.show   =  STATUS_ISSET(PORCELAIN)
	? GIT_STATUS_SHOW_INDEX_AND_WORKDIR
	: GIT_STATUS_SHOW_INDEX_THEN_WORKDIR;
    opts.flags |=  GIT_STATUS_OPT_INCLUDE_UNTRACKED;
    opts.flags |=  GIT_STATUS_OPT_INCLUDE_IGNORED;
    opts.flags |=  GIT_STATUS_OPT_INCLUDE_UNMODIFIED;
    opts.flags &= ~GIT_STATUS_OPT_EXCLUDE_SUBMODULED;
    opts.flags |=  GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS;

    git->state = 0;
    if (STATUS_ISSET(PORCELAIN))
	xx = chkgit(git, "git_status_foreach_ext",
		git_status_foreach_ext(git->R, &opts, status_porcelain_cb, (void *)git));
    else
	xx = chkgit(git, "git_status_foreach_ext",
		git_status_foreach_ext(git->R, &opts, status_cb, (void *)git));

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);
    status_untracked_files = _free(status_untracked_files);

    av = argvFree(av);
    git = rpmgitFree(git);
    con = poptFreeContext(con);
    return rc;
}
#undef	STATUS_ISSET

/*==============================================================*/
#ifdef	REFERENCE
OPTIONS
       --local, -l
           When the repository to clone from is on a local machine, this flag
           bypasses the normal "git aware" transport mechanism and clones the
           repository by making a copy of HEAD and everything under objects
           and refs directories. The files under .git/objects/ directory are
           hardlinked to save space when possible. This is now the default
           when the source repository is specified with /path/to/repo syntax,
           so it essentially is a no-op option. To force copying instead of
           hardlinking (which may be desirable if you are trying to make a
           back-up of your repository), but still avoid the usual "git aware"
           transport mechanism, --no-hardlinks can be used.

       --no-hardlinks
           Optimize the cloning process from a repository on a local
           filesystem by copying files under .git/objects directory.

       --shared, -s
           When the repository to clone is on the local machine, instead of
           using hard links, automatically setup .git/objects/info/alternates
           to share the objects with the source repository. The resulting
           repository starts out without any object of its own.

           NOTE: this is a possibly dangerous operation; do not use it unless
           you understand what it does. If you clone your repository using
           this option and then delete branches (or use any other git command
           that makes any existing commit unreferenced) in the source
           repository, some objects may become unreferenced (or dangling).
           These objects may be removed by normal git operations (such as git
           commit) which automatically call git gc --auto. (See git-gc(1).) If
           these objects are removed and were referenced by the cloned
           repository, then the cloned repository will become corrupt.

           Note that running git repack without the -l option in a repository
           cloned with -s will copy objects from the source repository into a
           pack in the cloned repository, removing the disk space savings of
           clone -s. It is safe, however, to run git gc, which uses the -l
           option by default.

           If you want to break the dependency of a repository cloned with -s
           on its source repository, you can simply run git repack -a to copy
           all objects from the source repository into a pack in the cloned
           repository.

       --reference <repository>
           If the reference repository is on the local machine, automatically
           setup .git/objects/info/alternates to obtain objects from the
           reference repository. Using an already existing repository as an
           alternate will require fewer objects to be copied from the
           repository being cloned, reducing network and local storage costs.

           NOTE: see the NOTE for the --shared option.

       --quiet, -q
           Operate quietly. Progress is not reported to the standard error
           stream. This flag is also passed to the ‘rsync’ command when given.

       --verbose, -v
           Run verbosely. Does not affect the reporting of progress status to
           the standard error stream.

       --progress
           Progress status is reported on the standard error stream by default
           when it is attached to a terminal, unless -q is specified. This
           flag forces progress status even if the standard error stream is
           not directed to a terminal.

       --no-checkout, -n
           No checkout of HEAD is performed after the clone is complete.

       --bare
           Make a bare GIT repository. That is, instead of creating
           <directory> and placing the administrative files in
           <directory>/.git, make the <directory> itself the $GIT_DIR. This
           obviously implies the -n because there is nowhere to check out the
           working tree. Also the branch heads at the remote are copied
           directly to corresponding local branch heads, without mapping them
           to refs/remotes/origin/. When this option is used, neither
           remote-tracking branches nor the related configuration variables
           are created.

       --mirror
           Set up a mirror of the remote repository. This implies --bare.

       --origin <name>, -o <name>
           Instead of using the remote name origin to keep track of the
           upstream repository, use <name>.

       --branch <name>, -b <name>
           Instead of pointing the newly created HEAD to the branch pointed to
           by the cloned repository’s HEAD, point to <name> branch instead. In
           a non-bare repository, this is the branch that will be checked out.

       --upload-pack <upload-pack>, -u <upload-pack>
           When given, and the repository to clone from is accessed via ssh,
           this specifies a non-default path for the command run on the other
           end.

       --template=<template_directory>
           Specify the directory from which templates will be used; (See the
           "TEMPLATE DIRECTORY" section of git-init(1).)

       --depth <depth>
           Create a shallow clone with a history truncated to the specified
           number of revisions. A shallow repository has a number of
           limitations (you cannot clone or fetch from it, nor push from nor
           into it), but is adequate if you are only interested in the recent
           history of a large project with a long history, and would want to
           send in fixes as patches.

       --recursive
           After the clone is created, initialize all submodules within, using
           their default settings. This is equivalent to running git submodule
           update --init --recursive immediately after the clone is finished.
           This option is ignored if the cloned repository does not have a
           worktree/checkout (i.e. if any of --no-checkout/-n, --bare, or
           --mirror is given)

       <repository>
           The (possibly remote) repository to clone from. See the URLS
           section below for more information on specifying repositories.

       <directory>
           The name of a new directory to clone into. The "humanish" part of
           the source repository is used if no directory is explicitly given
           (repo for /path/to/repo.git and foo for host.xz:foo/.git). Cloning
           into an existing directory is only allowed if the directory is
           empty.
#endif
static rpmRC cmd_clone(int argc, char *argv[])
{
    const char * clone_reference = xstrdup("all");
    const char * clone_origin = NULL;
    const char * clone_branch = NULL;
    const char * clone_upload_pack = NULL;
    const char * clone_template = NULL;
    int clone_depth = 0;
    enum {
	_CLONE_LOCAL		= (1 <<  0),
	_CLONE_NO_HARDLINKS	= (1 <<  1),
	_CLONE_SHARED		= (1 <<  2),
	_CLONE_QUIET		= (1 <<  3),
	_CLONE_VERBOSE		= (1 <<  4),
	_CLONE_PROGRESS		= (1 <<  5),
	_CLONE_NO_CHECKOUT	= (1 <<  6),
	_CLONE_BARE		= (1 <<  7),
	_CLONE_MIRROR		= (1 <<  8),
	_CLONE_RECURSIVE	= (1 <<  9),
    };
    int clone_flags = 0;
#define	CLONE_ISSET(_a)	(clone_flags & _CLONE_##_a)
    struct poptOption cloneOpts[] = {
     { "local", 'l', POPT_BIT_SET,		&clone_flags, _CLONE_LOCAL,
	N_(""), NULL },
     { "no-hardlinks", '\0', POPT_BIT_SET,	&clone_flags, _CLONE_NO_HARDLINKS,
	N_(""), NULL },
     { "shared", 's', POPT_BIT_SET,		&clone_flags, _CLONE_SHARED,
	N_(""), NULL },
     { "reference", '\0', POPT_ARG_STRING,	&clone_reference, 0,
	N_(""), N_("<repository>") },
     { "quiet", 'q', POPT_ARG_VAL,		&clone_flags, _CLONE_QUIET,
	N_("Quiet mode."), NULL },
     { "verbose", 'v', POPT_BIT_SET,		&clone_flags, _CLONE_VERBOSE,
	N_("Verbose mode."), NULL },
     { "progress", '\0', POPT_BIT_SET,		&clone_flags, _CLONE_PROGRESS,
	N_(""), NULL },
     { "no-checkout", 'n', POPT_BIT_SET,	&clone_flags, _CLONE_NO_CHECKOUT,
	N_(""), NULL },
     { "bare", '\0', POPT_BIT_SET,		&clone_flags, _CLONE_BARE,
	N_(""), NULL },
     { "mirror", '\0', POPT_BIT_SET,		&clone_flags, _CLONE_MIRROR,
	N_(""), NULL },
     { "origin", 'o', POPT_ARG_STRING,		&clone_origin, 0,
	N_(""), N_("<name>") },
     { "branch", 'b', POPT_ARG_STRING,		&clone_branch, 0,
	N_(""), N_("<name>") },
     { "upload-pack", 'u', POPT_ARG_STRING,	&clone_upload_pack, 0,
	N_(""), N_("<upload-pack>") },
     { "template", '\0', POPT_ARG_STRING,	&clone_template, 0,
	N_(""), N_("<template_directory>") },
     { "depth", '\0', POPT_ARG_INT,		&clone_depth, 0,
	N_(""), N_("<depth>") },
     { "recursive", '\0', POPT_BIT_SET,		&clone_flags, _CLONE_RECURSIVE,
	N_(""), NULL },
      POPT_TABLEEND
    };
    poptContext con = rpmgitPopt(argc, argv, cloneOpts);
    ARGV_t av = NULL;
    int ac = 0;
    rpmgit git = rpmgitNew(git_dir, 0);
    rpmRC rc = RPMRC_FAIL;
    int xx = -1;

argvPrint(__FUNCTION__, (ARGV_t)argv, NULL);
if (strcmp(argv[0], "clone")) assert(0);
argvPrint(__FUNCTION__, (ARGV_t)argv, NULL);

rpmgitPrintRepo(git, git->R, git->fp);

    xx = argvAppend(&av, (ARGV_t)poptGetArgs(con));
    ac = argvCount(av);
argvPrint(__FUNCTION__, (ARGV_t)av, NULL);

fprintf(stderr, "FIXME:\ttgit clone %s\n", av[0]);

    xx = 0;

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);

    av = argvFree(av);
    git = rpmgitFree(git);
    con = poptFreeContext(con);
    return rc;
}
#undef	CLONE_ISSET

/*==============================================================*/

static rpmRC cmd_walk(int ac, char *av[])
{
    rpmRC rc = RPMRC_FAIL;
    rpmgit git = rpmgitNew(git_dir, 0);
    int xx = -1;

argvPrint(__FUNCTION__, (ARGV_t)av, NULL);
if (strcmp(av[0], "walk")) assert(0);

rpmgitPrintRepo(git, git->R, git->fp);

    xx = 0;

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);

    git = rpmgitFree(git);
    return rc;
}

/*==============================================================*/
#ifdef	REFERENCE
commit 8e60c712acef798a6b3913359f8d9dffcddf7256
Author: Adam Roben <adam@roben.org>
Date:   Thu Jun 7 09:50:19 2012 -0400

    Fix git_status_file for files that start with a character > 0x7f
    
    git_status_file would always return GIT_ENOTFOUND for these files.
    
    ...

OPTIONS
       -p, -u
           Generate patch (see section on generating patches).

       -U<n>, --unified=<n>
           Generate diffs with <n> lines of context instead of the usual
           three. Implies -p.

       --raw
           Generate the raw format.

       --patch-with-raw
           Synonym for -p --raw.

       --patience
           Generate a diff using the "patience diff" algorithm.

       --stat[=width[,name-width]]
           Generate a diffstat. You can override the default output width for
           80-column terminal by --stat=width. The width of the filename part
           can be controlled by giving another width to it separated by a
           comma.

       --numstat
           Similar to --stat, but shows number of added and deleted lines in
           decimal notation and pathname without abbreviation, to make it more
           machine friendly. For binary files, outputs two - instead of saying
           0 0.

       --shortstat
           Output only the last line of the --stat format containing total
           number of modified files, as well as number of added and deleted
           lines.

       --dirstat[=limit]
           Output the distribution of relative amount of changes (number of
           lines added or removed) for each sub-directory. Directories with
           changes below a cut-off percent (3% by default) are not shown. The
           cut-off percent can be set with --dirstat=limit. Changes in a child
           directory is not counted for the parent directory, unless
           --cumulative is used.

       --dirstat-by-file[=limit]
           Same as --dirstat, but counts changed files instead of lines.

       --summary
           Output a condensed summary of extended header information such as
           creations, renames and mode changes.

       --patch-with-stat
           Synonym for -p --stat.

       -z
           Separate the commits with NULs instead of with new newlines.

           Also, when --raw or --numstat has been given, do not munge
           pathnames and use NULs as output field terminators.

           Without this option, each pathname output will have TAB, LF, double
           quotes, and backslash characters replaced with \t, \n, \"", and \\,
           respectively, and the pathname will be enclosed in double quotes if
           any of those replacements occurred.

       --name-only
           Show only names of changed files.

       --name-status
           Show only names and status of changed files. See the description of
           the --diff-filter option on what the status letters mean.

       --submodule[=<format>]
           Chose the output format for submodule differences. <format> can be
           one of _____ and ___.  _____ just shows pairs of commit names, this
           format is used when this option is not given.  ___ is the default
           value for this option and lists the commits in that commit range
           like the _______ option of git-submodule(1) does.

       --color[=<when>]
           Show colored diff. The value must be always (the default), never,
           or auto.

       --no-color
           Turn off colored diff, even when the configuration file gives the
           default to color output. Same as --color=never.

       --color-words[=<regex>]
           Show colored word diff, i.e., color words which have changed. By
           default, words are separated by whitespace.

           When a <regex> is specified, every non-overlapping match of the
           <regex> is considered a word. Anything between these matches is
           considered whitespace and ignored(!) for the purposes of finding
           differences. You may want to append |[^[:space:]] to your regular
           expression to make sure that it matches all non-whitespace
           characters. A match that contains a newline is silently
           truncated(!) at the newline.

           The regex can also be set via a diff driver or configuration
           option, see gitattributes(1) or git-config(1). Giving it explicitly
           overrides any diff driver or configuration setting. Diff drivers
           override configuration settings.

       --no-renames
           Turn off rename detection, even when the configuration file gives
           the default to do so.

       --check
           Warn if changes introduce trailing whitespace or an indent that
           uses a space before a tab. Exits with non-zero status if problems
           are found. Not compatible with --exit-code.

       --full-index
           Instead of the first handful of characters, show the full pre- and
           post-image blob object names on the "index" line when generating
           patch format output.

       --binary
           In addition to --full-index, output a binary diff that can be
           applied with git-apply.

       --abbrev[=<n>]
           Instead of showing the full 40-byte hexadecimal object name in
           diff-raw format output and diff-tree header lines, show only a
           partial prefix. This is independent of the --full-index option
           above, which controls the diff-patch output format. Non default
           number of digits can be specified with --abbrev=<n>.

       -B
           Break complete rewrite changes into pairs of delete and create.

       -M
           Detect renames.

       -C
           Detect copies as well as renames. See also --find-copies-harder.

       --diff-filter=[ACDMRTUXB*]
           Select only files that are Added (A), Copied (C), Deleted (D),
           Modified (M), Renamed (R), have their type (i.e. regular file,
           symlink, submodule, ...) changed (T), are Unmerged (U), are Unknown
           (X), or have had their pairing Broken (B). Any combination of the
           filter characters may be used. When * (All-or-none) is added to the
           combination, all paths are selected if there is any file that
           matches other criteria in the comparison; if there is no file that
           matches other criteria, nothing is selected.

       --find-copies-harder
           For performance reasons, by default, -C option finds copies only if
           the original file of the copy was modified in the same changeset.
           This flag makes the command inspect unmodified files as candidates
           for the source of copy. This is a very expensive operation for
           large projects, so use it with caution. Giving more than one -C
           option has the same effect.

       -l<num>
           The -M and -C options require O(n^2) processing time where n is the
           number of potential rename/copy targets. This option prevents
           rename/copy detection from running if the number of rename/copy
           targets exceeds the specified number.

       -S<string>
           Look for differences that introduce or remove an instance of
           <string>. Note that this is different than the string simply
           appearing in diff output; see the _______ entry in gitdiffcore(7)
           for more details.

       --pickaxe-all
           When -S finds a change, show all the changes in that changeset, not
           just the files that contain the change in <string>.

       --pickaxe-regex
           Make the <string> not a plain string but an extended POSIX regex to
           match.

       -O<orderfile>
           Output the patch in the order specified in the <orderfile>, which
           has one shell glob pattern per line.

       -R
           Swap two inputs; that is, show differences from index or on-disk
           file to tree contents.

       --relative[=<path>]
           When run from a subdirectory of the project, it can be told to
           exclude changes outside the directory and show pathnames relative
           to it with this option. When you are not in a subdirectory (e.g. in
           a bare repository), you can name which subdirectory to make the
           output relative to by giving a <path> as an argument.

       -a, --text
           Treat all files as text.

       --ignore-space-at-eol
           Ignore changes in whitespace at EOL.

       -b, --ignore-space-change
           Ignore changes in amount of whitespace. This ignores whitespace at
           line end, and considers all other sequences of one or more
           whitespace characters to be equivalent.

       -w, --ignore-all-space
           Ignore whitespace when comparing lines. This ignores differences
           even if one line has whitespace where the other line has none.

       --inter-hunk-context=<lines>
           Show the context between diff hunks, up to the specified number of
           lines, thereby fusing hunks that are close to each other.

       --exit-code
           Make the program exit with codes similar to diff(1). That is, it
           exits with 1 if there were differences and 0 means no differences.

       --quiet
           Disable all output of the program. Implies --exit-code.

       --ext-diff
           Allow an external diff helper to be executed. If you set an
           external diff driver with gitattributes(5), you need to use this
           option with git-log(1) and friends.

       --no-ext-diff
           Disallow external diff drivers.

       --ignore-submodules
           Ignore changes to submodules in the diff generation.

       --src-prefix=<prefix>
           Show the given source prefix instead of "a/".

       --dst-prefix=<prefix>
           Show the given destination prefix instead of "b/".

       --no-prefix
           Do not show any source or destination prefix.

       For more detailed explanation on these common options, see also
       gitdiffcore(7).

       -<n>
           Limits the number of commits to show.

       <since>..<until>
           Show only commits between the named two commits. When either
           <since> or <until> is omitted, it defaults to HEAD, i.e. the tip of
           the current branch. For a more complete list of ways to spell
           <since> and <until>, see "SPECIFYING REVISIONS" section in git-rev-
           parse(1).

       --decorate[=short|full]
           Print out the ref names of any commits that are shown. If _____ is
           specified, the ref name prefixes ___________, __________ and
           _____________ will not be printed. If ____ is specified, the full
           ref name (including prefix) will be printed. The default option is
           _____.

       --source
           Print out the ref name given on the command line by which each
           commit was reached.

       --full-diff
           Without this flag, "git log -p <path>..." shows commits that touch
           the specified paths, and diffs about the same specified paths. With
           this, the full diff is shown for commits that touch the specified
           paths; this means that "<path>..." limits only commits, and doesn’t
           limit diff for those commits.

       --follow
           Continue listing the history of a file beyond renames.

       --log-size
           Before the log message print out its size in bytes. Intended mainly
           for porcelain tools consumption. If git is unable to produce a
           valid value size is set to zero. Note that only message is
           considered, if also a diff is shown its size is not included.

       [--] <path>...
           Show only commits that affect any of the specified paths. To
           prevent confusion with options and branch names, paths may need to
           be prefixed with "-- " to separate them from options or refnames.

#endif

static rpmRC cmd_log(int ac, char *av[])
{
    FILE * fp = stdout;
    rpmRC rc = RPMRC_FAIL;
    rpmgit git = rpmgitNew(git_dir, 0);
    int xx = -1;

argvPrint(__FUNCTION__, (ARGV_t)av, NULL);
if (strcmp(av[0], "log")) assert(0);

rpmgitPrintRepo(git, git->R, git->fp);

#ifdef	DYING
    xx = chkgit(git, "git_repository_head",
		git_repository_head((git_reference **)&git->H, git->R));
    if (xx)
	goto exit;
rpmgitPrintHead(git, git->H, git->fp);
git_reference * H = (git_reference *) git->H;
git_oid * Hoid = (git_oid *) git_reference_oid(H);

git_revwalk * walk = NULL;
    git_revwalk_new(&walk, git->R);

    git_revwalk_sorting(walk, GIT_SORT_TOPOLOGICAL | GIT_SORT_REVERSE);
    git_revwalk_push(walk, Hoid);

    while ((git_revwalk_next(Hoid, walk)) == 0) {
	git_commit * C;
	const git_signature * S;
	xx = chkgit(git, "git_commit_lookup",
		git_commit_lookup(&C, git->R, Hoid));
rpmgitPrintOid("Commit", git_commit_id(C), fp);
	S = git_commit_author(C);
fprintf(fp, "Author: %s <%s>\n", S->name, S->email);
rpmgitPrintTime("  Date", (time_t)S->when.time, fp);
fprintf(fp, "%s\n", git_commit_message(C));
	git_commit_free(C);
    }

    git_revwalk_free(walk);
    walk = NULL;
    git->walk = NULL;

    xx = 0;
#else
    xx = rpmgitWalk(git);
#endif

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);

    git = rpmgitFree(git);
    return rc;
}

/*==============================================================*/
#ifdef	REFERENCE
OPTIONS
       -d
           Delete a branch. The branch must be fully merged in its upstream
           branch, or in HEAD if no upstream was set with --track or
           --set-upstream.

       -D
           Delete a branch irrespective of its merged status.

       -l
           Create the branch’s reflog. This activates recording of all changes
           made to the branch ref, enabling use of date based sha1 expressions
           such as "<branchname>@{yesterday}". Note that in non-bare
           repositories, reflogs are usually enabled by default by the
           core.logallrefupdates config option.

       -f, --force
           Reset <branchname> to <startpoint> if <branchname> exists already.
           Without -f git branch refuses to change an existing branch.

       -m
           Move/rename a branch and the corresponding reflog.

       -M
           Move/rename a branch even if the new branch name already exists.

       --color[=<when>]
           Color branches to highlight current, local, and remote branches.
           The value must be always (the default), never, or auto.

       --no-color
           Turn off branch colors, even when the configuration file gives the
           default to color output. Same as --color=never.

       -r
           List or delete (if used with -d) the remote-tracking branches.

       -a
           List both remote-tracking branches and local branches.

       -v, --verbose
           Show sha1 and commit subject line for each head, along with
           relationship to upstream branch (if any). If given twice, print the
           name of the upstream branch, as well.

       --abbrev=<length>
           Alter the sha1’s minimum display length in the output listing. The
           default value is 7.

       --no-abbrev
           Display the full sha1s in the output listing rather than
           abbreviating them.

       -t, --track
           When creating a new branch, set up configuration to mark the
           start-point branch as "upstream" from the new branch. This
           configuration will tell git to show the relationship between the
           two branches in git status and git branch -v. Furthermore, it
           directs git pull without arguments to pull from the upstream when
           the new branch is checked out.

           This behavior is the default when the start point is a remote
           branch. Set the branch.autosetupmerge configuration variable to
           false if you want git checkout and git branch to always behave as
           if --no-track were given. Set it to always if you want this
           behavior when the start-point is either a local or remote branch.

       --no-track
           Do not set up "upstream" configuration, even if the
           branch.autosetupmerge configuration variable is true.

       --set-upstream
           If specified branch does not exist yet or if --force has been
           given, acts exactly like --track. Otherwise sets up configuration
           like --track would when creating the branch, except that where
           branch points to is not changed.

       --contains <commit>
           Only list branches which contain the specified commit.

       --merged [<commit>]
           Only list branches whose tips are reachable from the specified
           commit (HEAD if not specified).

       --no-merged [<commit>]
           Only list branches whose tips are not reachable from the specified
           commit (HEAD if not specified).

       <branchname>
           The name of the branch to create or delete. The new branch name
           must pass all checks defined by git-check-ref-format(1). Some of
           these checks may restrict the characters allowed in a branch name.

       <start-point>
           The new branch head will point to this commit. It may be given as a
           branch name, a commit-id, or a tag. If this option is omitted, the
           current HEAD will be used instead.

       <oldbranch>
           The name of an existing branch to rename.

       <newbranch>
           The new name for an existing branch. The same restrictions as for
           <branchname> apply.

#endif

static rpmRC cmd_branch(int argc, char *argv[])
{
    enum {
	_BRANCH_DELETE		= (1 <<  0),
	_BRANCH_DELETE_ALWAYS	= (1 <<  1),
	_BRANCH_CREATE_REFLOG	= (1 <<  2),
	_BRANCH_FORCE		= (1 <<  3),
	_BRANCH_MOVE		= (1 <<  4),
	_BRANCH_MOVE_ALWAYS	= (1 <<  5),
	_BRANCH_VERBOSE		= (1 <<  6),
    };
    int branch_flags = 0;
#define	BRANCH_ISSET(_a)	(branch_flags & _BRANCH_##_a)
    struct poptOption branchOpts[] = {
      POPT_TABLEEND
    };
    poptContext con = rpmgitPopt(argc, argv, branchOpts);
    ARGV_t av = NULL;
    int ac = 0;
    rpmgit git = rpmgitNew(git_dir, 0);
    FILE * fp = stdout;
    rpmRC rc = RPMRC_FAIL;
git_strarray branches;
    char active = '*';	/* XXX assumes 1st branch is active */
    int xx = -1;
    int i;

argvPrint(__FUNCTION__, (ARGV_t)argv, NULL);
if (strcmp(argv[0], "branch")) assert(0);
rpmgitPrintRepo(git, git->R, git->fp);

    xx = argvAppend(&av, (ARGV_t)poptGetArgs(con));
    ac = argvCount(av);
argvPrint(__FUNCTION__, (ARGV_t)av, NULL);

    /* XXX assumes -a listing */
    xx = chkgit(git, "git_branch_list",
		git_branch_list(&branches, git->R, GIT_BRANCH_LOCAL));
    if (xx)
	goto exit;
    for (i = 0; i < (int)branches.count; ++i) {
	char * brname = branches.strings[i];
	fprintf(fp, "%c %s\n", active, basename(brname));
	active = ' ';	/* XXX assumes 1st branch is active */
    }

#ifdef	NOTYET	/* XXX .git/refs/remotes/... */
    xx = chkgit(git, "git_branch_list",
		git_branch_list(&branches, git->R, GIT_BRANCH_REMOTE));
    for (i = 0; i < (int)branches.count; ++i) {
	char * brname = branches.strings[i];
	fprintf(fp, "%c %s\n", active, brname);
    }
#endif
    xx = 0;

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);

    av = argvFree(av);
    git = rpmgitFree(git);
    con = poptFreeContext(con);
    return rc;
}

/*==============================================================*/
#ifdef	REFERENCE
OPTIONS
       -t <type>
           Specify the type (default: "blob").

       -w
           Actually write the object into the object database.

       --stdin
           Read the object from standard input instead of from a file.

       --stdin-paths
           Read file names from stdin instead of from the command-line.

       --path
           Hash object as it were located at the given path. The location of
           file does not directly influence on the hash value, but path is
           used to determine what git filters should be applied to the object
           before it can be placed to the object database, and, as result of
           applying filters, the actual blob put into the object database may
           differ from the given file. This option is mainly useful for
           hashing temporary files located outside of the working directory or
           files read from stdin.

       --no-filters
           Hash the contents as is, ignoring any input filter that would have
           been chosen by the attributes mechanism, including crlf conversion.
           If the file is read from standard input then this is always
           implied, unless the --path option is given.
#endif
static rpmRC cmd_hash_object(int argc, char *argv[])
{
    const char * ho_type = xstrdup("blob");
    const char * ho_path = NULL;;
    enum {
	_HO_WRITE	= (1 << 0),
	_HO_STDIN	= (1 << 1),
	_HO_STDIN_PATHS	= (1 << 2),
	_HO_NO_FILTERS	= (1 << 3),
    };
    int ho_flags = 0;
#define	HO_ISSET(_a)	(ho_flags & _HO_##_a)
    struct poptOption hoOpts[] = {
     { NULL, 't', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT,	&ho_type, 0,
	N_("Specify the object <type>."), N_("<type>") },
     { NULL, 'w', POPT_ARG_VAL,			&ho_flags, _HO_WRITE,
	N_("Write the object into the object database."), NULL },
     { "stdin", '\0', POPT_ARG_VAL,		&ho_flags, _HO_STDIN,
	N_("Read object content from stdin instead of from a file."), NULL },
     { "stdin-paths", '\0', POPT_ARG_VAL,	&ho_flags, _HO_STDIN_PATHS,
	N_("Read file path(s) from stdin instead of from args."), NULL },
     { "path", '\0', POPT_ARG_STRING,		&ho_path, 0,
	N_("Hash object as if it were located at the given <path>."), N_("<path>") },
     { "no-filters", '\0', POPT_ARG_VAL,	&ho_flags, _HO_NO_FILTERS,
	N_("Hash the contents as is, ignoring any input filter."), NULL },
      POPT_TABLEEND
    };
    poptContext con = rpmgitPopt(argc, argv, hoOpts);
    ARGV_t av = NULL;
    int ac = 0;
    rpmgit git = rpmgitNew(git_dir, 0);
    git_odb * odb = NULL;
    FILE * fp = stdout;
    rpmRC rc = RPMRC_FAIL;
    int xx = -1;
    int i;

fprintf(stderr, "--> %s(%p[%d]) con %p ho_flags %x\n", __FUNCTION__, argv, argc, con, ho_flags);

argvPrint(__FUNCTION__, (ARGV_t)argv, NULL);
if (strcmp(argv[0], "hash-object")) assert(0);
rpmgitPrintRepo(git, git->R, git->fp);

    xx = argvAppend(&av, (ARGV_t)poptGetArgs(con));
    if (HO_ISSET(STDIN)) {
	xx = argvAdd(&av, "-");
    } else
    if (HO_ISSET(STDIN_PATHS)) {
	ARGV_t nav = NULL;
	/* XXX white space in paths? */
	xx = argvFgets(&nav, NULL);
	xx = argvAppend(&av, nav);
	nav = argvFree(nav);
    }
    ac = argvCount(av);
argvPrint(__FUNCTION__, (ARGV_t)av, NULL);

    /* XXX assumes -t blob */

    for (i = 0; i < ac; i++) {
	/* XXX compute SHA1 while reading instead. */
	rpmiob iob;

	iob = NULL;
	xx = rpmiobSlurp(av[i], &iob);
	if (xx || iob == NULL)
	    goto exit;
	if (iob) {
	    DIGEST_CTX ctx = rpmDigestInit(PGPHASHALGO_SHA1, 0);
	    char * digest;
	    char b[128];
	    size_t nb = sizeof(b);
	    size_t nw = snprintf(b, nb, "blob %u", (unsigned)rpmiobLen(iob));

	    b[nb-1] = '\0';
	    (void) rpmDigestUpdate(ctx, (char *)b, nw+1);
	    (void) rpmDigestUpdate(ctx, (char *)rpmiobBuf(iob), rpmiobLen(iob));
	    digest = NULL;
	    (void) rpmDigestFinal(ctx, &digest, NULL, 1);
fprintf(fp, "%s\n", digest);

	    if (HO_ISSET(WRITE)) {
		git_oid oid;
		if (odb == NULL) {
		    xx = chkgit(git, "git_repositroy_odb",
				git_repository_odb(&odb, git->R));
		    if (xx)
			goto exit;
		}
		xx = chkgit(git, "git_oid_fromstr",
			git_oid_fromstr(&oid, digest));
		if (xx)
		    goto exit;
		xx = chkgit(git, "git_odb_write",
			git_odb_write(&oid, odb,
				rpmiobBuf(iob), rpmiobLen(iob), GIT_OBJ_BLOB));
		if (xx)
		    goto exit;
	    }
	    digest = _free(digest);
	    iob = rpmiobFree(iob);
	}
    }
    xx = 0;

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);
    ho_path = _free(ho_path);
    ho_type = _free(ho_type);

    git_odb_free(odb);
    av = argvFree(av);
    git = rpmgitFree(git);
    con = poptFreeContext(con);
    return rc;
}
#undef	HO_ISSET

/*==============================================================*/
#ifdef	REFERENCE
OPTIONS
       <object>
           The name of the object to show. For a more complete list of ways to spell object names, see the
           "SPECIFYING REVISIONS" section in git-rev-parse(1).

       -t
           Instead of the content, show the object type identified by <object>.

       -s
           Instead of the content, show the object size identified by <object>.

       -e
           Suppress all output; instead exit with zero status if <object> exists and is a valid object.

       -p
           Pretty-print the contents of <object> based on its type.

       <type>
           Typically this matches the real type of <object> but asking for a type that can trivially be
           dereferenced from the given <object> is also permitted. An example is to ask for a "tree" with
           <object> being a commit object that contains it, or to ask for a "blob" with <object> being a tag
           object that points at it.

       --batch
           Print the SHA1, type, size, and contents of each object provided on stdin. May not be combined with
           any other options or arguments.

       --batch-check
           Print the SHA1, type, and size of each object provided on stdin. May not be combined with any other
           options or arguments.
#endif
static rpmRC cmd_cat_file(int argc, char *argv[])
{
    enum {
	_CF_CONTENT	= (1 << 0),
	_CF_SHA1	= (1 << 1),
	_CF_TYPE	= (1 << 2),
	_CF_SIZE	= (1 << 3),
	_CF_BATCH	= (1 << 4),
	_CF_CHECK	= (1 << 5),
	_CF_PRETTY	= (1 << 6),
	_CF_QUIET	= (1 << 7),
    };
    int cf_flags = 0;
#define	CF_ISSET(_a)	(cf_flags & _CF_##_a)
    struct poptOption cfOpts[] = {
     { NULL, 'e', POPT_ARG_VAL,			&cf_flags, _CF_QUIET,
	N_("Suppress all output; exit with zero if valid object."), NULL },
	/* XXX _CF_CONTENT */
	/* XXX _CF_SHA1 */
     { NULL, 't', POPT_BIT_SET,			&cf_flags, _CF_TYPE,
	N_("Show the object type."), NULL },
     { NULL, 's', POPT_BIT_SET,			&cf_flags, _CF_SIZE,
	N_("Show the object size."), NULL },
     { NULL, 'p', POPT_BIT_SET,			&cf_flags, _CF_PRETTY,
	N_("Pretty-print the object contents."), NULL },
     { "batch", '\0', POPT_BIT_SET,		&cf_flags, _CF_BATCH,
	N_("Print SHA1, type, size, contents of each stdin object."), NULL },
     { "batch-check", '\0', POPT_BIT_SET,	&cf_flags, _CF_CHECK,
	N_("Print SHA1, type, size of each stdin object."), NULL },
      POPT_TABLEEND
    };
    poptContext con = rpmgitPopt(argc, argv, cfOpts);
    ARGV_t av = NULL;
    int ac = 0;
    rpmgit git = rpmgitNew(git_dir, 0);
    FILE * fp = stdout;
    rpmRC rc = RPMRC_FAIL;
    int missing = 0;
    int xx = -1;
    int i;

fprintf(stderr, "--> %s(%p[%d]) con %p cf_flags %x\n", __FUNCTION__, argv, argc, con, cf_flags);

argvPrint(__FUNCTION__, (ARGV_t)argv, NULL);
if (strcmp(argv[0], "cat-file")) assert(0);
rpmgitPrintRepo(git, git->R, git->fp);

    xx = argvAppend(&av, (ARGV_t)poptGetArgs(con));
    if (CF_ISSET(BATCH) || CF_ISSET(CHECK)) {
	ARGV_t nav = NULL;
	xx = argvFgets(&nav, NULL);
	xx = argvAppend(&av, nav);
	nav = argvFree(nav);
	cf_flags |= (_CF_SHA1|_CF_TYPE|_CF_SIZE);
	if (CF_ISSET(BATCH))
	    cf_flags |=  _CF_CONTENT;
	else
	    cf_flags &= ~_CF_CONTENT;
    }
    ac = argvCount(av);
argvPrint(__FUNCTION__, (ARGV_t)av, NULL);

    if (!(cf_flags & (_CF_CONTENT|_CF_SHA1|_CF_TYPE|_CF_SIZE|_CF_QUIET)))
	cf_flags |= _CF_CONTENT;

    /* XXX 1st arg can be <type> */

    for (i = 0; i < ac; i++) {
	git_object * obj;
	git_oid oid;
	char Ooid[GIT_OID_HEXSZ + 1];
	git_otype otype;
	const char * Otype;
	size_t Osize;
	int ndigits = strlen(av[i]);

	xx = chkgit(git, "git_oid_fromstrn",
		git_oid_fromstrn(&oid, av[i], ndigits));
	xx = chkgit(git, "git_object_lookup_prefix",
		git_object_lookup_prefix(&obj, git->R, &oid, ndigits, GIT_OBJ_ANY));

	if (xx)
	    missing++;
	if (CF_ISSET(QUIET))
	    continue;
	if (CF_ISSET(BATCH) && CF_ISSET(CHECK)) {
	    if (xx)
		fprintf(fp, "%s missing\n", av[i]);
	    continue;
	}
	if (xx)
	    continue;

	git_oid_fmt(Ooid, git_object_id(obj));
	Ooid[GIT_OID_HEXSZ] = '\0';
	otype = git_object_type(obj);
	Otype = git_object_type2string(otype);
	Osize = git_odb_object_size(obj);	/* XXX W2DO? */

	if (CF_ISSET(BATCH)) {
	    fprintf(fp, "%s %s %u\n", Ooid, Otype, (unsigned)Osize);
	} else
	if (CF_ISSET(CHECK)) {
	    fprintf(fp, "%s %s %u\n", Ooid, Otype, (unsigned)Osize);
	} else {
	    if (CF_ISSET(SHA1))
		fprintf(fp, "%s\n", Ooid);
	    if (CF_ISSET(TYPE))
		fprintf(fp, "%s\n", Otype);
	    if (CF_ISSET(SIZE))
	 	fprintf(fp, "%u\n", (unsigned)Osize);
	}
	if (CF_ISSET(CONTENT))
	switch (otype) {
	case GIT_OBJ_ANY:
	case GIT_OBJ_BAD:
	default:
assert(0);
	case GIT_OBJ_BLOB:
	    fprintf(fp, "%s\n", (const char *) git_blob_rawcontent(obj));
	    break;
	case GIT_OBJ_TREE:
rpmgitPrintTree(obj, fp);
	    break;
	case GIT_OBJ_COMMIT:
rpmgitPrintCommit(git, obj, fp);
	    break;
	case GIT_OBJ__EXT1:
	case GIT_OBJ_TAG:
	case GIT_OBJ__EXT2:
	case GIT_OBJ_OFS_DELTA:
	case GIT_OBJ_REF_DELTA:
	    fprintf(fp, "%s %s\n", "*** FIXME:", Otype);
	    break;
	}
	git_object_free(obj);
    }

exit:
    rc = (missing ? RPMRC_NOTFOUND : RPMRC_OK);
SPEW(0, rc, git);

    av = argvFree(av);
    git = rpmgitFree(git);
    con = poptFreeContext(con);
    return rc;
}
#undef	CF_ISSET

/*==============================================================*/
#ifdef	REFERENCE
OPTIONS
       --add
           If a specified file isn’t in the index already then it’s added.
           Default behaviour is to ignore new files.

       --remove
           If a specified file is in the index but is missing then it’s
           removed. Default behavior is to ignore removed file.

       --refresh
           Looks at the current index and checks to see if merges or updates
           are needed by checking stat() information.

       -q
           Quiet. If --refresh finds that the index needs an update, the
           default behavior is to error out. This option makes git
           update-index continue anyway.

       --ignore-submodules
           Do not try to update submodules. This option is only respected when
           passed before --refresh.

       --unmerged
           If --refresh finds unmerged changes in the index, the default
           behavior is to error out. This option makes git update-index
           continue anyway.

       --ignore-missing
           Ignores missing files during a --refresh

       --cacheinfo <mode> <object> <path>
           Directly insert the specified info into the index.

       --index-info
           Read index information from stdin.

       --chmod=(+|-)x
           Set the execute permissions on the updated files.

       --assume-unchanged, --no-assume-unchanged
           When these flags are specified, the object names recorded for the
           paths are not updated. Instead, these options set and unset the
           "assume unchanged" bit for the paths. When the "assume unchanged"
           bit is on, git stops checking the working tree files for possible
           modifications, so you need to manually unset the bit to tell git
           when you change the working tree file. This is sometimes helpful
           when working with a big project on a filesystem that has very slow
           lstat(2) system call (e.g. cifs).

           This option can be also used as a coarse file-level mechanism to
           ignore uncommitted changes in tracked files (akin to what
           .gitignore does for untracked files). You should remember that an
           explicit git add operation will still cause the file to be
           refreshed from the working tree. Git will fail (gracefully) in case
           it needs to modify this file in the index e.g. when merging in a
           commit; thus, in case the assumed-untracked file is changed
           upstream, you will need to handle the situation manually.

       --really-refresh
           Like --refresh, but checks stat information unconditionally,
           without regard to the "assume unchanged" setting.

       --skip-worktree, --no-skip-worktree
           When one of these flags is specified, the object name recorded for
           the paths are not updated. Instead, these options set and unset the
           "skip-worktree" bit for the paths. See section "Skip-worktree bit"
           below for more information.

       -g, --again
           Runs git update-index itself on the paths whose index entries are
           different from those from the HEAD commit.

       --unresolve
           Restores the unmerged or needs updating state of a file during a
           merge if it was cleared by accident.

       --info-only
           Do not create objects in the object database for all <file>
           arguments that follow this flag; just insert their object IDs into
           the index.

       --force-remove
           Remove the file from the index even when the working directory
           still has such a file. (Implies --remove.)

       --replace
           By default, when a file path exists in the index, git update-index
           refuses an attempt to add path/file. Similarly if a file path/file
           exists, a file path cannot be added. With --replace flag, existing
           entries that conflict with the entry being added are automatically
           removed with warning messages.

       --stdin
           Instead of taking list of paths from the command line, read list of
           paths from the standard input. Paths are separated by LF (i.e. one
           path per line) by default.

       --verbose
           Report what is being added and removed from index.

       -z
           Only meaningful with --stdin; paths are separated with NUL
           character instead of LF.

       --
           Do not interpret any more arguments as options.

       <file>
           Files to act on. Note that files beginning with .  are discarded.
           This includes ./file and dir/./file. If you don’t want this, then
           use cleaner names. The same applies to directories ending / and
           paths with //
#endif
static rpmRC cmd_update_index(int argc, char *argv[])
{
    enum {
	_UI_ADD			= (1 <<  0),
	_UI_REMOVE		= (1 <<  1),
	_UI_REFRESH		= (1 <<  2),
	_UI_QUIET		= (1 <<  3),
	_UI_IGNORE_SUBMODULES	= (1 <<  4),
	_UI_UNMERGED		= (1 <<  5),
	_UI_IGNORE_MISSING	= (1 <<  6),
	_UI_INDEX_INFO		= (1 <<  7),
	_UI_ASSUME_UNCHANGED	= (1 <<  8),
	_UI_REALLY_REFRESH	= (1 <<  9),
	_UI_SKIP_WORKTREE	= (1 << 10),
	_UI_AGAIN		= (1 << 11),
	_UI_UNRESOLVE		= (1 << 12),
	_UI_INFO_ONLY		= (1 << 13),
	_UI_FORCE_REMOVE	= (1 << 14),
	_UI_REPLACE		= (1 << 15),
	_UI_STDIN		= (1 << 16),
	_UI_VERBOSE		= (1 << 17),
	_UI_ZERO		= (1 << 18),
    };
    int ui_flags = 0;
#define	UI_ISSET(_a)	(ui_flags & _UI_##_a)
    struct poptOption uiOpts[] = {
     { "add", '\0', POPT_BIT_SET,		&ui_flags, _UI_ADD,
	N_("Add file (if not already present)."), NULL },
     { "remove", '\0', POPT_BIT_SET,		&ui_flags, _UI_REMOVE,
	N_("Remove file in index if missing."), NULL },
     { "refresh", '\0', POPT_BIT_SET,		&ui_flags, _UI_REFRESH,
	N_("Check for merges/updates in index."), NULL },
     { NULL, 'q', POPT_ARG_VAL,			&ui_flags, _UI_QUIET,
	N_("Quiet mode."), NULL },
     { "ignore-submodules", '\0', POPT_BIT_SET,	&ui_flags, _UI_IGNORE_SUBMODULES,
	N_("Do not update sub-modules."), NULL },
     { "unmerged", '\0', POPT_BIT_SET,		&ui_flags, _UI_UNMERGED,
	N_("Continue if --refresh finds unmerged changes in the index."), NULL },
     { "ignore-missing", '\0', POPT_BIT_SET,	&ui_flags, _UI_IGNORE_MISSING,
	N_("Ignore missing files during --refresh."), NULL },
	/* XXX --cacheinfo a b c */
     { "index-info", '\0', POPT_BIT_SET,	&ui_flags, _UI_INDEX_INFO,
	N_("Read index information from stdin"), NULL },
	/* XXX --chmod=(+|-)x */
     { "assume-unchanged", '\0', POPT_BIT_SET,	&ui_flags, _UI_ASSUME_UNCHANGED,
	N_(""), NULL },
	/* XXX --no-assume-unchanged */
     { "really-refresh", '\0', POPT_BIT_SET,	&ui_flags, _UI_REALLY_REFRESH,
	N_("Like --refresh, but always check stat(2) info."), NULL },
     { "skip-worktree", '\0', POPT_BIT_SET,	&ui_flags, _UI_SKIP_WORKTREE,
	N_(""), NULL },
	/* XXX --no-skip-work-tree */
     { "again", 'g', POPT_ARG_VAL,		&ui_flags, _UI_AGAIN,
	N_(""), NULL },
     { "unresolve", '\0', POPT_BIT_SET,		&ui_flags, _UI_UNRESOLVE,
	N_(""), NULL },
     { "info-only", '\0', POPT_BIT_SET,		&ui_flags, _UI_INFO_ONLY,
	N_(""), NULL },
     { "force-remove", '\0', POPT_BIT_SET,	&ui_flags, _UI_FORCE_REMOVE,
	N_(""), NULL },
     { "replace", '\0', POPT_BIT_SET,		&ui_flags, _UI_REPLACE,
	N_(""), NULL },
     { "stdin", '\0', POPT_BIT_SET,		&ui_flags, _UI_STDIN,
	N_(""), NULL },
     { "verbose", 'v', POPT_BIT_SET,		&ui_flags, _UI_VERBOSE,
	N_(""), NULL },
     { NULL, 'z', POPT_BIT_SET,			&ui_flags, _UI_ZERO,
	N_(""), NULL },
      POPT_TABLEEND
    };
    poptContext con = rpmgitPopt(argc, argv, uiOpts);
    ARGV_t av = NULL;
    int ac = 0;
    rpmgit git = rpmgitNew(git_dir, 0);
    FILE * fp = stdout;
    rpmRC rc = RPMRC_FAIL;
    int xx = -1;

fprintf(stderr, "--> %s(%p[%d]) con %p ui_flags %x\n", __FUNCTION__, argv, argc, con, ui_flags);

argvPrint(__FUNCTION__, (ARGV_t)argv, NULL);
if (strcmp(argv[0], "update-index")) assert(0);
rpmgitPrintRepo(git, git->R, git->fp);

    xx = argvAppend(&av, (ARGV_t)poptGetArgs(con));
    if (UI_ISSET(STDIN)) {
	ARGV_t nav = NULL;
	/* XXX white space in paths? */
	xx = argvFgets(&nav, NULL);
	xx = argvAppend(&av, nav);
	nav = argvFree(nav);
    }
    ac = argvCount(av);
argvPrint(__FUNCTION__, (ARGV_t)av, NULL);

#ifdef	NOTYET
    xx = chkgit(git, "git_repositroy_odb",
		git_repository_odb(&odb, git->R));
    git_odb_free(odb);
#endif

exit:
    rc = (xx ? RPMRC_NOTFOUND : RPMRC_OK);
SPEW(0, rc, git);

    av = argvFree(av);
    git = rpmgitFree(git);
    con = poptFreeContext(con);
    return rc;
}
#undef	UI_ISSET

/*==============================================================*/

static rpmRC cmd_index(int ac, char *av[])
{
    rpmRC rc = RPMRC_FAIL;
    rpmgit git = rpmgitNew(git_dir, 0);
    int xx = -1;

argvPrint(__FUNCTION__, (ARGV_t)av, NULL);
if (strcmp(av[0], "index")) assert(0);
rpmgitPrintRepo(git, git->R, git->fp);

    xx = chkgit(git, "git_repository_index",
		git_repository_index((git_index **)&git->I, git->R));
    if (xx)
	goto exit;

    git->fp = stdout;
    rpmgitPrintIndex(git->I, git->fp);
    git->fp = NULL;

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);

    git = rpmgitFree(git);
    return rc;
}

/*==============================================================*/

static rpmRC cmd_refs(int ac, char *av[])
{
    FILE * fp = stdout;
    rpmRC rc = RPMRC_FAIL;
    rpmgit git = rpmgitNew(git_dir, 0);
git_strarray refs;
    int xx = -1;
    int i;

argvPrint(__FUNCTION__, (ARGV_t)av, NULL);
if (strcmp(av[0], "refs")) assert(0);
rpmgitPrintRepo(git, git->R, git->fp);

    xx = chkgit(git, "git_reference_list",
		git_reference_list(&refs, git->R, GIT_REF_LISTALL));
    if (xx)
	goto exit;

    for (i = 0; i < (int)refs.count; ++i) {
	char oid[GIT_OID_HEXSZ + 1];
	const char * refname = refs.strings[i];
	git_reference *ref;
	ref = NULL;
	xx = chkgit(git, "git_reference_lookuo",
		git_reference_lookup(&ref, git->R, refname));
	switch (git_reference_type(ref)) {
	case GIT_REF_OID:
 	    git_oid_fmt(oid, git_reference_oid(ref));
	    oid[GIT_OID_HEXSZ] = '\0';
	    fprintf(fp, "%s [%s]\n", refname, oid);
	    break;
	case GIT_REF_SYMBOLIC:
	    fprintf(fp, "%s => %s\n", refname, git_reference_target(ref));
	    break;
	default:
assert(0);
	    break;
	}
    }
    xx = 0;

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);

    git = rpmgitFree(git);
    return rc;
}

/*==============================================================*/

#ifdef	REFERENCE
OPTIONS
       --replace-all
           Default behavior is to replace at most one line. This replaces all
           lines matching the key (and optionally the value_regex).

       --add
           Adds a new line to the option without altering any existing values.
           This is the same as providing ^$ as the value_regex in
           --replace-all.

       --get
           Get the value for a given key (optionally filtered by a regex
           matching the value). Returns error code 1 if the key was not found
           and error code 2 if multiple key values were found.

       --get-all
           Like get, but does not fail if the number of values for the key is
           not exactly one.

       --get-regexp
           Like --get-all, but interprets the name as a regular expression.
           Also outputs the key names.

       --global
           For writing options: write to global ~/.gitconfig file rather than
           the repository .git/config.

           For reading options: read only from global ~/.gitconfig rather than
           from all available files.

           See also the section called “FILES”.

       --system
           For writing options: write to system-wide $(prefix)/etc/gitconfig
           rather than the repository .git/config.

           For reading options: read only from system-wide
           $(prefix)/etc/gitconfig rather than from all available files.

           See also the section called “FILES”.

       -f config-file, --file config-file
           Use the given config file instead of the one specified by
           GIT_CONFIG.

       --remove-section
           Remove the given section from the configuration file.

       --rename-section
           Rename the given section to a new name.

       --unset
           Remove the line matching the key from config file.

       --unset-all
           Remove all lines matching the key from config file.

       -l, --list
           List all variables set in config file.

       --bool

           git config will ensure that the output is "true" or "false"

       --int

           git config will ensure that the output is a simple decimal number.
           An optional value suffix of k, m, or g in the config file will
           cause the value to be multiplied by 1024, 1048576, or 1073741824
           prior to output.

       --bool-or-int

           git config will ensure that the output matches the format of either
           --bool or --int, as described above.

       --path

           git-config will expand leading ~ to the value of $HOME, and ~user
           to the home directory for the specified user. This option has no
           effect when setting the value (but you can use git config bla ~/
           from the command line to let your shell do the expansion).

       -z, --null
           For all options that output values and/or keys, always end values
           with the null character (instead of a newline). Use newline instead
           as a delimiter between key and value. This allows for secure
           parsing of the output without getting confused e.g. by values that
           contain line breaks.

       --get-colorbool name [stdout-is-tty]
           Find the color setting for name (e.g.  color.diff) and output
           "true" or "false".  stdout-is-tty should be either "true" or
           "false", and is taken into account when configuration says "auto".
           If stdout-is-tty is missing, then checks the standard output of the
           command itself, and exits with status 0 if color is to be used, or
           exits with status 1 otherwise. When the color setting for name is
           undefined, the command uses color.ui as fallback.

       --get-color name [default]
           Find the color configured for name (e.g.  color.diff.new) and
           output it as the ANSI color escape sequence to the standard output.
           The optional default parameter is used instead, if there is no
           color configured for name.

       -e, --edit
           Opens an editor to modify the specified config file; either
           --system, --global, or repository (default).
#endif
static rpmRC cmd_config(int ac, char *av[])
{
    rpmRC rc = RPMRC_FAIL;
    rpmgit git = rpmgitNew(git_dir, 0);
    int xx = -1;

argvPrint(__FUNCTION__, (ARGV_t)av, NULL);
if (strcmp(av[0], "config")) assert(0);
rpmgitPrintRepo(git, git->R, git->fp);

    /* Print configuration info. */
    git->fp = stdout;
    xx = rpmgitConfig(git);
    git->fp = NULL;
    if (xx)
	goto exit;

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);

    git = rpmgitFree(git);
    return rc;
}

/*==============================================================*/
static int show_ref__cb(git_remote_head *head, void *payload)
{
    char oid[GIT_OID_HEXSZ + 1] = {0};
    git_oid_fmt(oid, &head->oid);
    printf("%s\t%s\n", oid, head->name);
    return GIT_OK;
}

#ifdef	REFERENCE
OPTIONS
       -h, --heads, -t, --tags
           Limit to only refs/heads and refs/tags, respectively. These options
           are not mutually exclusive; when given both, references stored in
           refs/heads and refs/tags are displayed.

       -u <exec>, --upload-pack=<exec>
           Specify the full path of git-upload-pack on the remote host. This
           allows listing references from repositories accessed via SSH and
           where the SSH daemon does not use the PATH configured by the user.

       <repository>
           Location of the repository. The shorthand defined in
           $GIT_DIR/branches/ can be used. Use "." (dot) to list references in
           the local repository.

       <refs>...
           When unspecified, all references, after filtering done with --heads
           and --tags, are shown. When <refs>... are specified, only
           references matching the given patterns are displayed.
#endif
static rpmRC cmd_ls_remote(int ac, char *av[])
{
    rpmRC rc = RPMRC_FAIL;
    rpmgit git = rpmgitNew(git_dir, 0);
    git_remote * remote = NULL;
    int xx = -1;

argvPrint(__FUNCTION__, (ARGV_t)av, NULL);
if (strcmp(av[0], "ls-remote")) assert(0);
    if (ac != 2)
	goto exit;

    /* If there's a ':' in the name, assume it's an URL */
    if (strchr(av[1], ':') != NULL) {
	/*
	 * Create an instance of a remote from the URL. The transport to use
	 * is detected from the URL
	 */
	xx = chkgit(git, "git_remote_new",
		git_remote_new(&remote, git->R, NULL, av[1], NULL));
	if (xx < GIT_OK)
	    goto exit;

    } else {
	/* Find the remote by name */
	xx = chkgit(git, "git_remote_load",
		git_remote_load(&remote, git->R, av[1]));
	if (xx < GIT_OK)
	    goto exit;
    }

    /*
     * When connecting, the underlying code needs to know wether we
     * want to push or fetch
     */
    xx = chkgit(git, "git_remote_connect",
	git_remote_connect(remote, GIT_DIR_FETCH));
    if (xx < GIT_OK)
	goto exit;

    /* With git_remote_ls we can retrieve the advertised heads */
    xx = chkgit(git, "git_remote_ls",
	git_remote_ls(remote, &show_ref__cb, NULL));

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);
    if (remote)
	git_remote_free(remote);
    git = rpmgitFree(git);
    return rc;
}

/*==============================================================*/
struct dl_data {
    git_remote *remote;
    git_off_t *bytes;
    git_indexer_stats *stats;
    int ret;
    int finished;
};

static void *download(void *ptr)
{
    rpmgit git = (rpmgit) ptr;
    struct dl_data *data = (struct dl_data *) git->data;
    int xx = -1;

    /*
     * Connect to the remote end specifying that we want to fetch
     * information from it.
     */
    xx = chkgit(git, "git_remote_connect",
	git_remote_connect(data->remote, GIT_DIR_FETCH));
    if (xx < 0) {
	data->ret = -1;
	goto exit;
    }
    /*
     * Download the packfile and index it. This function updates the
     * amount of received data and the indexer stats which lets you
     * inform the user about progress.
     */
    xx = chkgit(git, "git_remote_download",
	git_remote_download(data->remote, data->bytes, data->stats));
    if (xx < 0) {
	data->ret = -1;
	goto exit;
    }

    data->ret = 0;

  exit:
    data->finished = 1;
    pthread_exit(&data->ret);
}

static int update_cb(const char *refname, const git_oid * a, const git_oid * b)
{
    FILE * fp = stdout;
    char a_str[GIT_OID_HEXSZ + 1];
    char b_str[GIT_OID_HEXSZ + 1];

    git_oid_fmt(b_str, b);
    b_str[GIT_OID_HEXSZ] = '\0';

    if (git_oid_iszero(a)) {
	fprintf(fp, "[new]     %.20s %s\n", b_str, refname);
    } else {
	git_oid_fmt(a_str, a);
	a_str[GIT_OID_HEXSZ] = '\0';
	fprintf(fp, "[updated] %.10s..%.10s %s\n", a_str, b_str, refname);
    }

    return 0;
}

#ifdef	REFERENCE
OPTIONS
       --all
           Fetch all remotes.

       -a, --append
           Append ref names and object names of fetched refs to the existing
           contents of .git/FETCH_HEAD. Without this option old data in
           .git/FETCH_HEAD will be overwritten.

       --depth=<depth>
           Deepen the history of a shallow repository created by git clone
           with --depth=<depth> option (see git-clone(1)) by the specified
           number of commits.

       --dry-run
           Show what would be done, without making any changes.

       -f, --force
           When git fetch is used with <rbranch>:<lbranch> refspec, it refuses
           to update the local branch <lbranch> unless the remote branch
           <rbranch> it fetches is a descendant of <lbranch>. This option
           overrides that check.

       -k, --keep
           Keep downloaded pack.

       --multiple
           Allow several <repository> and <group> arguments to be specified.
           No <refspec>s may be specified.

       --prune
           After fetching, remove any remote tracking branches which no longer
           exist on the remote.

       -n, --no-tags
           By default, tags that point at objects that are downloaded from the
           remote repository are fetched and stored locally. This option
           disables this automatic tag following.

       -t, --tags
           Most of the tags are fetched automatically as branch heads are
           downloaded, but tags that do not point at objects reachable from
           the branch heads that are being tracked will not be fetched by this
           mechanism. This flag lets all tags and their associated objects be
           downloaded.

       -u, --update-head-ok
           By default git fetch refuses to update the head which corresponds
           to the current branch. This flag disables the check. This is purely
           for the internal use for git pull to communicate with git fetch,
           and unless you are implementing your own Porcelain you are not
           supposed to use it.

       --upload-pack <upload-pack>
           When given, and the repository to fetch from is handled by git
           fetch-pack, --exec=<upload-pack> is passed to the command to
           specify non-default path for the command run on the other end.

       -q, --quiet
           Pass --quiet to git-fetch-pack and silence any other internally
           used git commands. Progress is not reported to the standard error
           stream.

       -v, --verbose
           Be verbose.

       --progress
           Progress status is reported on the standard error stream by default
           when it is attached to a terminal, unless -q is specified. This
           flag forces progress status even if the standard error stream is
           not directed to a terminal.

       <repository>
           The "remote" repository that is the source of a fetch or pull
           operation. This parameter can be either a URL (see the section GIT
           URLS below) or the name of a remote (see the section REMOTES
           below).

       <group>
           A name referring to a list of repositories as the value of
           remotes.<group> in the configuration file. (See git-config(1)).

       <refspec>
           The format of a <refspec> parameter is an optional plus +, followed
           by the source ref <src>, followed by a colon :, followed by the
           destination ref <dst>.

           The remote ref that matches <src> is fetched, and if <dst> is not
           empty string, the local ref that matches it is fast-forwarded using
           <src>. If the optional plus + is used, the local ref is updated
           even if it does not result in a fast-forward update.

               Note
               If the remote branch from which you want to pull is modified in
               non-linear ways such as being rewound and rebased frequently,
               then a pull will attempt a merge with an older version of
               itself, likely conflict, and fail. It is under these conditions
               that you would want to use the + sign to indicate
               non-fast-forward updates will be needed. There is currently no
               easy way to determine or declare that a branch will be made
               available in a repository with this behavior; the pulling user
               simply must know this is the expected usage pattern for a
               branch.

               Note
               You never do your own development on branches that appear on
               the right hand side of a <refspec> colon on Pull: lines; they
               are to be updated by git fetch. If you intend to do development
               derived from a remote branch B, have a Pull: line to track it
               (i.e.  Pull: B:remote-B), and have a separate branch my-B to do
               your development on top of it. The latter is created by git
               branch my-B remote-B (or its equivalent git checkout -b my-B
               remote-B). Run git fetch to keep track of the progress of the
               remote side, and when you see something new on the remote
               branch, merge it into your development branch with git pull .
               remote-B, while you are on my-B branch.

               Note
               There is a difference between listing multiple <refspec>
               directly on git pull command line and having multiple Pull:
               <refspec> lines for a <repository> and running git pull command
               without any explicit <refspec> parameters. <refspec> listed
               explicitly on the command line are always merged into the
               current branch after fetching. In other words, if you list more
               than one remote refs, you would be making an Octopus. While git
               pull run without any explicit <refspec> parameter takes default
               <refspec>s from Pull: lines, it merges only the first <refspec>
               found into the current branch, after fetching all the remote
               refs. This is because making an Octopus from remote refs is
               rarely done, while keeping track of multiple remote heads in
               one-go by fetching more than one is often useful.
           Some short-cut notations are also supported.

           ·    tag <tag> means the same as refs/tags/<tag>:refs/tags/<tag>;
               it requests fetching everything up to the given tag.

           ·   A parameter <ref> without a colon is equivalent to <ref>: when
               pulling/fetching, so it merges <ref> into the current branch
               without storing the remote branch anywhere locally
#endif
static rpmRC cmd_fetch(int ac, char *av[])
{
    FILE * fp = stdout;
    rpmRC rc = RPMRC_FAIL;
    rpmgit git = rpmgitNew(git_dir, 0);
    git_remote *remote = NULL;
    git_off_t bytes = 0;
    git_indexer_stats stats;
    int xx = -1;
    pthread_t worker;
    struct dl_data data;

argvPrint(__FUNCTION__, (ARGV_t)av, NULL);
if (strcmp(av[0], "fetch")) assert(0);
    if (ac != 2)
	goto exit;

    /* Figure out whether it's a named remote or a URL */
    fprintf(fp, "Fetching %s\n", av[1]);
    xx = chkgit(git, "git_remote_load",
	git_remote_load(&remote, git->R, av[1]));
    if (xx < 0) {
	xx = chkgit(git, "git_remote_new",
	    git_remote_new(&remote, git->R, NULL, av[1], NULL));
    }
    if (xx < GIT_OK)
	goto exit;

    /* Set up the information for the background worker thread */
    data.remote = remote;
    data.bytes = &bytes;
    data.stats = &stats;
    data.ret = 0;
    data.finished = 0;
    memset(&stats, 0, sizeof(stats));

    git->data = &data;
    /* XXX yarn? */
    pthread_create(&worker, NULL, download, git);

    /*
     * Loop while the worker thread is still running. Here we show processed
     * and total objects in the pack and the amount of received
     * data. Most frontends will probably want to show a percentage and
     * the download rate.
     */
    do {
	usleep(10000);
	fprintf(fp, "\rReceived %d/%d objects in %ld bytes", stats.processed,
	       stats.total, (long)bytes);
    } while (!data.finished);
    fprintf(fp, "\rReceived %d/%d objects in %ld bytes\n", stats.processed,
	   stats.total, (long)bytes);

    /* Disconnect the underlying connection to prevent from idling. */
    git_remote_disconnect(remote);

    /*
     * Update the references in the remote's namespace to point to the
     * right commits. This may be needed even if there was no packfile
     * to download, which can happen e.g. when the branches have been
     * changed but all the neede objects are available locally.
     */
    xx = chkgit(git, "git_remote_update_tips",
		git_remote_update_tips(remote, update_cb));
    if (xx < GIT_OK)
	goto exit;

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);
    if (remote)
	git_remote_free(remote);
    git = rpmgitFree(git);
    return rc;
}

/*==============================================================*/
#ifdef	REFERENCE
/*
 * This could be run in the main loop whilst the application waits for
 * the indexing to finish in a worker thread
 */
static int index_cb(const git_indexer_stats * stats, void *data)
{
    printf("\rProcessing %d of %d", stats->processed, stats->total);
    return 0;
}
#endif

#ifdef	REFERENCE
OPTIONS
       -v
           Be verbose about what is going on, including progress status.

       -o <index-file>
           Write the generated pack index into the specified file. Without
           this option the name of pack index file is constructed from the
           name of packed archive file by replacing .pack with .idx (and the
           program fails if the name of packed archive does not end with
           .pack).

       --stdin
           When this flag is provided, the pack is read from stdin instead and
           a copy is then written to <pack-file>. If <pack-file> is not
           specified, the pack is written to objects/pack/ directory of the
           current git repository with a default name determined from the pack
           content. If <pack-file> is not specified consider using --keep to
           prevent a race condition between this process and git repack.

       --fix-thin
           Fix a "thin" pack produced by git pack-objects --thin (see git-
           pack-objects(1) for details) by adding the excluded objects the
           deltified objects are based on to the pack. This option only makes
           sense in conjunction with --stdin.

       --keep
           Before moving the index into its final destination create an empty
           .keep file for the associated pack file. This option is usually
           necessary with --stdin to prevent a simultaneous git repack process
           from deleting the newly constructed pack and index before refs can
           be updated to use objects contained in the pack.

       --keep=why
           Like --keep create a .keep file before moving the index into its
           final destination, but rather than creating an empty file place why
           followed by an LF into the .keep file. The why message can later be
           searched for within all .keep files to locate any which have
           outlived their usefulness.

       --index-version=<version>[,<offset>]
           This is intended to be used by the test suite only. It allows to
           force the version for the generated pack index, and to force 64-bit
           index entries on objects located above the given offset.

       --strict
           Die, if the pack contains broken objects or links.
#endif
static rpmRC cmd_index_pack(int ac, char *av[])
{
    FILE * fp = stderr;
    rpmRC rc = RPMRC_FAIL;
    rpmgit git = rpmgitNew(git_dir, 0);
    git_indexer_stream *idx = NULL;
    git_indexer_stats stats = { 0, 0 };
    int fdno = 0;
    char hash[GIT_OID_HEXSZ + 1] = {0};
    ssize_t nr;
    char b[512];
    size_t nb = sizeof(b);
    int xx = -1;

argvPrint(__FUNCTION__, (ARGV_t)av, NULL);
if (strcmp(av[0], "index-pack")) assert(0);
    if (ac != 2)
	goto exit;

    xx = chkgit(git, "git_indexer_stream_new",
	git_indexer_stream_new(&idx, ".git"));
    if (xx < 0) {
	fputs("bad idx\n", fp);
	goto exit;
    }

    if ((fdno = open(av[1], 0)) < 0) {
	perror("open");
	goto exit;
    }

    do {
	nr = read(fdno, b, nb);
	if (nr < 0)
	    break;

	xx = chkgit(git, "git_indexer_stream_add",
	     git_indexer_stream_add(idx, b, nr, &stats));
	if (xx < 0)
	    goto exit;

	fprintf(fp, "\rIndexing %d of %d", stats.processed, stats.total);
    } while (nr > 0);

    if (nr < 0) {
	xx = -1;
	perror("failed reading");
	goto exit;
    }

    xx = chkgit(git, "git_indexer_stream_finalize",
	git_indexer_stream_finalize(idx, &stats));
    if (xx < 0)
	goto exit;

    fprintf(fp, "\rIndexing %d of %d\n", stats.processed, stats.total);

    git_oid_fmt(hash, git_indexer_stream_hash(idx));
    fputs(hash, fp);

    rc = RPMRC_OK;

exit:
SPEW(0, rc, git);
    if (fdno > 2)
	xx = close(fdno);
    if (idx)
	git_indexer_stream_free(idx);
    git = rpmgitFree(git);
    return rc;
}

static rpmRC cmd_index_pack_old(int ac, char *av[])
{
    FILE * fp = stderr;
    rpmRC rc = RPMRC_FAIL;
    rpmgit git = rpmgitNew(git_dir, 0);
    git_indexer *indexer = NULL;
    git_indexer_stats stats;
    char hash[GIT_OID_HEXSZ + 1] = {0};
    int xx;

argvPrint(__FUNCTION__, (ARGV_t)av, NULL);
if (strcmp(av[0], "index-pack-old")) assert(0);
    if (ac != 2)
	goto exit;

    /* Create a new indexer */
    xx = chkgit(git, "git_indexer_new",
	git_indexer_new(&indexer, av[1]));
    if (xx < GIT_OK)
	goto exit;
         
    /*
     * Index the packfile. This function can take a very long time and
     * should be run in a worker thread.
     */
    xx = chkgit(git, "git_indexer_run",
	git_indexer_run(indexer, &stats));
    if (xx < GIT_OK)
	goto exit;
                    
    /* Write the information out to an index file */
    xx = chkgit(git, "git_indexer_write",
	git_indexer_write(indexer));
                             
    /* Get the packfile's hash (which should become it's filename) */
    git_oid_fmt(hash, git_indexer_hash(indexer));
    fprintf(fp, "%s\n", hash);

    rc = RPMRC_OK;

exit:
SPEW(0, rc, git);
    if (indexer)
	git_indexer_free(indexer);
    git = rpmgitFree(git);
    return rc;
}

/*==============================================================*/
static rpmRC cmd_noop(int ac, char *av[])
{
argvPrint(__FUNCTION__, (ARGV_t)av, NULL);
    return RPMRC_FAIL;
}

/*==============================================================*/

#define ARGMINMAX(_min, _max)   (int)(((_min) << 8) | ((_max) & 0xff))

static struct poptOption _rpmgitCommandTable[] = {
/* --- PORCELAIN */
 { "init", '\0', POPT_ARG_MAINCALL,	cmd_init, ARGMINMAX(1,1),
	N_("Initialize a git repository"), N_("DIR") },
 { "add", '\0', POPT_ARG_MAINCALL,	cmd_add, ARGMINMAX(1,0),
	N_("Add a file to a git repository"), N_("FILE") },
 { "commit", '\0', POPT_ARG_MAINCALL,	cmd_commit, ARGMINMAX(0,0),
	N_("Commit a git tree"), N_("DIR") },
 { "diff", '\0', POPT_ARG_MAINCALL,	cmd_diff, ARGMINMAX(0,0),
	N_("Show modifciations in a git tree"), N_("DIR") },
 { "status", '\0', POPT_ARG_MAINCALL,	cmd_status, ARGMINMAX(0,0),
	N_("Show status of a git tree"), N_("DIR") },
 { "clone", '\0', POPT_ARG_MAINCALL,	cmd_clone, ARGMINMAX(0,0),
	N_("Clone a remote git tree"), N_("DIR") },
 { "walk", '\0', POPT_ARG_MAINCALL,	cmd_walk, ARGMINMAX(0,0),
	N_("Walk a git tree"), N_("DIR") },
 { "log", '\0', POPT_ARG_MAINCALL,	cmd_log, ARGMINMAX(0,0),
	N_("Walk a git tree"), N_("DIR") },
 { "branch", '\0', POPT_ARG_MAINCALL,	cmd_branch, ARGMINMAX(0,0),
	N_("Show git branches."), NULL },

/* --- PLUMBING: manipulation */
 { "apply", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Apply a patch to files and/or to the index."), NULL },
 { "checkout-index", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Copy files from the index to the working tree."), NULL },
 { "commit-tree", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Create a new commit object."), NULL },
 { "hash-object", '\0', POPT_ARG_MAINCALL,	cmd_hash_object, ARGMINMAX(0,0),
	N_("Compute object ID and optionally creates a blob from a file."), NULL },
 { "index-pack", '\0', POPT_ARG_MAINCALL,	cmd_index_pack, ARGMINMAX(0,0),
	N_("Build pack index file for an existing packed archive."), NULL },
 { "merge-file", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Run a three-way file merge."), NULL },
 { "merge-index", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Run a merge for files needing merging."), NULL },
 { "mktag", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Creates a tag object."), NULL },
 { "mktree", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Build a tree-object from ls-tree formatted text."), NULL },
 { "pack-objects", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Create a packed archive of objects."), NULL },
 { "prune-packed", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Remove extra objects that are already in pack files."), NULL },
 { "read-tree", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("iReads tree information into the index."), NULL },
 { "symbolic-ref", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("iRead and modify symbolic refs."), NULL },
 { "unpack-objects", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Unpack objects from a packed archive."), NULL },
 { "update-index", '\0', POPT_ARG_MAINCALL,	cmd_update_index, ARGMINMAX(0,0),
	N_("Register file contents in the working tree to the index."), NULL },
 { "update-ref", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Update the object name stored in a ref safely."), NULL },
 { "write-ref", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Create a tree object from the current index."), NULL },

/* --- PLUMBING: interrogation */
 { "cat-file", '\0', POPT_ARG_MAINCALL,	cmd_cat_file, ARGMINMAX(0,0),
	N_("Provide content or type and size information for repository objects."), NULL },
 { "diff-files", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Compares files in the working tree and the index."), NULL },
 { "diff-index", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Compares content and mode of blobs between the index and repository."), NULL },
 { "diff-tree", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Compares the content and mode of blobs found via two tree objects."), NULL },
 { "for-each-ref", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Output information on each ref."), NULL },
 { "ls-files", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("iShow information about files in the index and the working tree."), NULL },
 { "ls-remote", '\0', POPT_ARG_MAINCALL,	cmd_ls_remote, ARGMINMAX(0,0),
	N_("List references in a remote repository."), NULL },
 { "ls-tree", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("List the contents of a tree object."), NULL },
 { "merge-base", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Find as good common ancestors as possible for a merge."), NULL },
 { "name-rev", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Find symbolic names for given revs."), NULL },
 { "pack-redundant", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Find redundant pack files."), NULL },
 { "rev-list", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Lists commit objects in reverse chronological order."), NULL },
 { "show-index", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Show packed archive index."), NULL },
 { "show-ref", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("List references in a local repository."), NULL },
 { "tar-tree", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("(deprecated) Create a tar archive of the files in the named tree object."), NULL },
 { "unpack-file", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Creates a temporary file with a blob’s contents."), NULL },
 { "var", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Show a git logical variable."), NULL },
 { "verify-pack", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Validate packed git archive files."), NULL },

/* --- WIP */
 { "index", '\0', POPT_ARG_MAINCALL,	cmd_index, ARGMINMAX(0,0),
	N_("Show git index."), NULL },
 { "refs", '\0', POPT_ARG_MAINCALL,	cmd_refs, ARGMINMAX(0,0),
	N_("Show git references."), NULL },
 { "config", '\0', POPT_ARG_MAINCALL,	cmd_config, ARGMINMAX(0,0),
	N_("Show git configuration."), NULL },

 { "fetch", '\0', POPT_ARG_MAINCALL,		cmd_fetch, ARGMINMAX(0,0),
	N_("Download the packfile from a git server"), N_("GITURI") },
 { "index-pack-old", '\0', POPT_ARG_MAINCALL,	cmd_index_pack_old, ARGMINMAX(0,0),
	N_("Index a PACKFILE"), N_("PACKFILE") },

  POPT_TABLEEND
};

static rpmRC cmd_help(int ac, /*@unused@*/ char *av[])
{
    FILE * fp = stdout;
    struct poptOption * c;

    fprintf(fp, "Commands:\n\n");
    for (c = _rpmgitCommandTable; c->longName != NULL; c++) {
        fprintf(fp, "    %s %s\n        %s\n\n",
		c->longName, (c->argDescrip ? c->argDescrip : ""), c->descrip);
    }
    return RPMRC_OK;
}

static rpmRC cmd_run(int ac, /*@unused@*/ char *av[])
{
    struct poptOption * c;
    const char * cmd;
    rpmRC rc = RPMRC_FAIL;

if (_rpmgit_debug < 0) argvPrint(__FUNCTION__, (ARGV_t)av, NULL);
    if (av == NULL || av[0] == NULL)	/* XXX segfault avoidance */
	goto exit;
    cmd = av[0];
    for (c = _rpmgitCommandTable; c->longName != NULL; c++) {
	rpmRC (*func) (int ac, char *av[]) = NULL;

	if (strcmp(cmd, c->longName))
	    continue;

	func = c->arg;
	rc = (*func) (ac, av);
	break;
    }

exit:
    return rc;
}

static struct poptOption rpmgitOptionsTable[] = {
  { "exec-path", '\0', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN,	&exec_path, 0,
        N_("Set exec path to <DIR>. env(GIT_EXEC_PATH)"), N_("<DIR>") },
  { "html-path", '\0', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN,	&html_path, 0,
        N_("Set html path to <DIR>. env(GIT_HTML_PATH)"), N_("<DIR>") },
  { "man-path", '\0', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN,	&man_path, 0,
        N_("Set man path to <DIR>."), N_("<DIR>") },
  { "info-path", '\0', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN,	&info_path, 0,
        N_("Set info path to <DIR>."), N_("<DIR>") },
 { "paginate", 'p', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&paginate, 1,
	N_("Set paginate. env(PAGER)"), NULL },
 { "no-replace-objects", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&no_replace_objects, 1,
	N_("Do not use replacement refs for objects."), NULL },
 { "bare", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN,	&bare, 1,
	N_("Treat as a bare repository."), NULL },

  { "git-dir", '\0', POPT_ARG_STRING,	&git_dir, 0,
        N_("Set git repository dir to <DIR>. env(GIT_DIR)"), N_("<DIR>") },
  { "work-tree", '\0', POPT_ARG_STRING,	&work_tree, 0,
        N_("Set git work tree to <DIR>. env(GIT_WORK_TREE)"), N_("<DIR>") },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"),
	NULL },

  POPT_AUTOALIAS
  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, char *argv[])
{
    poptContext con;
    char ** av;
    int ac;
    int rc = 0;

    if ((git_dir = getenv("GIT_DIR")) == NULL)
	git_dir = RPMGIT_DIR "/.git";
    git_dir = xstrdup(git_dir);
    if ((work_tree = getenv("GIT_WORK_TREE")) == NULL)
	work_tree = RPMGIT_DIR;
    work_tree = xstrdup(work_tree);

	/* XXX POSIX_ME_HARDER to avoid need of -- before MAINCALL */
    con = rpmioInit(argc, argv, rpmgitOptionsTable);
    av = (char **) poptGetArgs(con);
    ac = argvCount((ARGV_t)av);
#ifdef	DYING
    git = rpmgitNew(NULL, 0);

    rc = rpmgitConfig(git);

    rc = rpmgitInfo(git);

    rc = rpmgitWalk(git);

    git = rpmgitFree(git);

/*@i@*/ urlFreeCache();
#endif

    rc = cmd_run(ac, av);
    if (rc)
	goto exit;

exit:
    con = rpmioFini(con);

    git_dir = _free(git_dir);
    work_tree = _free(work_tree);

    return rc;
}

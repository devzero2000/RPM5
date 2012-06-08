#include "system.h"

#include <rpmio.h>
#include <ugid.h>
#include <poptIO.h>

#if defined(HAVE_GIT2_H)
#include <git2.h>
#include <git2/errors.h>
#endif

#define	_RPMGIT_INTERNAL
#include <rpmgit.h>

#include "debug.h"

static const char * repofn = "/var/tmp/git/.git";

#define	SPEW(_t, _rc, _git)	\
  { if ((_t) || _rpmgit_debug ) \
	fprintf(stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, (_git), \
		(_rc)); \
  }

static git_diff_options opts = {0};
static int color = -1;
static int compact = 0;
static int cached = 0;

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

static rpmRC cmd_init(int ac, char *av[])
{
    static const char _msg[] = "WDJ commit";
    rpmRC rc = RPMRC_FAIL;
    const char * fn;
    rpmgit git;
    int xx = -1;
    int i;

assert(ac >= 2);
if (strcmp(av[0], "init")) assert(0);

    fn = (ac >= 2 ? av[1] : repofn);
    git = rpmgitNew(fn, 0);

    /* Initialize a git repository. */
    xx = rpmgitInit(git);
    if (xx)
	goto exit;

    /* Read/print/save configuration info. */
    xx = rpmgitConfig(git);
    if (xx)
	goto exit;

    /* XXX automagic add and commit (for now) */
    if (ac <= 2)
	goto exit;

    /* Create file(s) in _workdir (if any). */
    for (i = 2; i < ac; i++) {
	struct stat sb;

	fn = av[i];

	/* XXX Create non-existent files lazily. */
	if (Stat(fn, &sb) < 0)
	    xx = rpmgitToyFile(git, fn, fn, strlen(fn));

	/* Add the file to the repository. */
	xx = rpmgitAddFile(git, fn);
	if (xx)
	    goto exit;
    }

    /* Commit added files. */
    xx = rpmgitCommit(git, _msg);
    if (xx)
	goto exit;
rpmgitPrintCommit(git, git->C, git->fp);

rpmgitPrintIndex(git->I, git->fp);
rpmgitPrintHead(git, NULL, git->fp);

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);

    git = rpmgitFree(git);
    return rc;
}

/*==============================================================*/

static rpmRC cmd_add(int ac, char *av[])
{
    rpmRC rc = RPMRC_FAIL;
    const char * fn;
    rpmgit git;
    int xx = -1;
    int i;

assert(ac >= 2);
if (strcmp(av[0], "add")) assert(0);

    fn = (ac >= 2 ? av[1] : repofn);
    git = rpmgitNew(fn, 0);

    /* XXX Get the index file for this repository. */
    xx = chkgit(git, "git_repository_index",
		git_repository_index((git_index **)&git->I, git->R));
    if (xx)
	goto exit;

if (_rpmgit_debug < 0) rpmgitPrintIndex(git->I, git->fp);
    /* Create file(s) in _workdir (if any). */
    for (i = 2; i < ac; i++) {
	struct stat sb;

	fn = av[i];

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

    git = rpmgitFree(git);
    return rc;
}

/*==============================================================*/

static rpmRC cmd_commit(int ac, char *av[])
{
    static const char _msg[] = "WDJ commit";
    rpmRC rc = RPMRC_FAIL;
    const char * fn;
    rpmgit git;
    int xx = -1;
    int i;

assert(ac >= 2);
if (strcmp(av[0], "commit")) assert(0);

    fn = (ac >= 2 ? av[1] : repofn);
    git = rpmgitNew(fn, 0);

    /* XXX Get the index file for this repository. */
    xx = chkgit(git, "git_repository_index",
		git_repository_index((git_index **)&git->I, git->R));
    if (xx)
	goto exit;

    /* Commit changes. */
    xx = rpmgitCommit(git, _msg);
    if (xx)
	goto exit;
rpmgitPrintCommit(git, git->C, git->fp);

rpmgitPrintIndex(git->I, git->fp);
rpmgitPrintHead(git, NULL, git->fp);

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);

    git = rpmgitFree(git);
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

static int printer(void *data, char usage, const char *line)
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

static rpmRC cmd_diff(int ac, char *av[])
{
char path[GIT_PATH_MAX];
    rpmRC rc = RPMRC_FAIL;
    const char * fn;
    rpmgit git = NULL;
const char * dir = ".";
git_diff_list * diff = NULL;
const char * treeish1 = NULL;
git_tree *t1 = NULL;
const char * treeish2 = NULL;
git_tree *t2 = NULL;
    int xx = -1;
    int i;

argvPrint(__FUNCTION__, (ARGV_t)av, NULL);
assert(ac >= 1);
if (strcmp(av[0], "diff")) assert(0);

#ifdef	NOTYET
    xx = chkgit(git, "git_repository_discover",
	git_repository_discover(path, sizeof(path), dir, 0, "/"));
    if (xx) {
	fprintf(stderr, "Could not discover repository\n");
	goto exit;
    }
    fn = path;
#else
    fn = "/var/tmp/xxx";
#endif
    git = rpmgitNew(fn, 0);
rpmgitPrintRepo(git, git->R, git->fp);

    if (ac >= 2)
	treeish1 = av[1];
    if (ac >= 3)
	treeish2 = av[2];

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

    git = rpmgitFree(git);
    return rc;
}

/*==============================================================*/

static rpmRC cmd_status(int ac, char *av[])
{
    rpmRC rc = RPMRC_FAIL;
    const char * fn;
    rpmgit git;
    int xx = -1;
    int i;

argvPrint(__FUNCTION__, (ARGV_t)av, NULL);
assert(ac >= 2);
if (strcmp(av[0], "status")) assert(0);

    fn = (ac >= 2 ? av[1] : repofn);
    git = rpmgitNew(fn, 0);

rpmgitPrintRepo(git, git->R, git->fp);

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);

    git = rpmgitFree(git);
    return rc;
}

/*==============================================================*/

static rpmRC cmd_clone(int ac, char *av[])
{
    rpmRC rc = RPMRC_FAIL;
    const char * fn;
    rpmgit git;
    int xx = -1;
    int i;

argvPrint(__FUNCTION__, (ARGV_t)av, NULL);
assert(ac >= 2);
if (strcmp(av[0], "clone")) assert(0);

    fn = (ac >= 2 ? av[1] : repofn);
    git = rpmgitNew(fn, 0);

rpmgitPrintRepo(git, git->R, git->fp);

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);

    git = rpmgitFree(git);
    return rc;
}

/*==============================================================*/

static rpmRC cmd_walk(int ac, char *av[])
{
    rpmRC rc = RPMRC_FAIL;
    const char * fn;
    rpmgit git;
    int xx = -1;
    int i;

argvPrint(__FUNCTION__, (ARGV_t)av, NULL);
assert(ac >= 2);
if (strcmp(av[0], "walk")) assert(0);

    fn = (ac >= 2 ? av[1] : repofn);
    git = rpmgitNew(fn, 0);

rpmgitPrintRepo(git, git->R, git->fp);

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);

    git = rpmgitFree(git);
    return rc;
}

/*==============================================================*/
#ifdef	REFERENCE
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
static rpmRC cmd_hash_object(int ac, char *av[])
{
    rpmRC rc = RPMRC_FAIL;
    const char * fn = NULL;
    rpmgit git;
    int xx = -1;
rpmiob iob = NULL;
char * digest = NULL;

argvPrint(__FUNCTION__, (ARGV_t)av, NULL);
assert(ac >= 1);
if (strcmp(av[0], "hash-object")) assert(0);

    fn = (ac >= 2 ? av[1] : repofn);
    git = rpmgitNew(fn, 0);

    /* XXX assume -t blob */
    fn = "-";	/* XXX assume --stdin */

    xx = rpmiobSlurp(fn, &iob);
    if (!(xx || iob == NULL)) {
	DIGEST_CTX ctx;
	char b[128];
	size_t nb = sizeof(b);
	size_t nw = snprintf(b, nb, "blob %u", rpmiobLen(iob));
	(void)nw;
	b[nb-1] = '\0';

	ctx = rpmDigestInit(PGPHASHALGO_SHA1, 0);
	(void) rpmDigestUpdate(ctx, (char *)b, nw+1);
	(void) rpmDigestUpdate(ctx, (char *)rpmiobBuf(iob), rpmiobLen(iob));
	(void) rpmDigestFinal(ctx, &digest, NULL, 1);
    }
fprintf(stdout, "%s\n", digest);
    xx = 0;

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);
    digest = _free(digest);
    iob = rpmiobFree(iob);

    git = rpmgitFree(git);
    return rc;
}

/*==============================================================*/

static rpmRC cmd_index(int ac, char *av[])
{
    rpmRC rc = RPMRC_FAIL;
    const char * fn;
    rpmgit git;
    int xx = -1;

argvPrint(__FUNCTION__, (ARGV_t)av, NULL);
assert(ac >= 1);
if (strcmp(av[0], "index")) assert(0);

    fn = (ac >= 2 ? av[1] : repofn);
    git = rpmgitNew(fn, 0);

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
    const char * fn;
    rpmgit git;
git_strarray refs;
    int xx = -1;
    int i;

argvPrint(__FUNCTION__, (ARGV_t)av, NULL);
assert(ac >= 1);
if (strcmp(av[0], "refs")) assert(0);

    fn = (ac >= 2 ? av[1] : repofn);
    git = rpmgitNew(fn, 0);

rpmgitPrintRepo(git, git->R, git->fp);

    xx = chkgit(git, "git_reference_list",
		git_reference_list(&refs, git->R, GIT_REF_LISTALL));

    for (i = 0; i < refs.count; ++i) {
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

static rpmRC cmd_config(int ac, char *av[])
{
    rpmRC rc = RPMRC_FAIL;
    const char * fn;
    rpmgit git;
    int xx = -1;

argvPrint(__FUNCTION__, (ARGV_t)av, NULL);
assert(ac >= 1);
if (strcmp(av[0], "config")) assert(0);

    fn = (ac >= 2 ? av[1] : repofn);
    git = rpmgitNew(fn, 0);

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

static rpmRC cmd_ls_remote(int ac, char *av[])
{
    rpmRC rc = RPMRC_FAIL;
    rpmgit git = rpmgitNew(repofn, 0);
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
    const char *action;
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

static rpmRC cmd_fetch(int ac, char *av[])
{
    FILE * fp = stdout;
    rpmRC rc = RPMRC_FAIL;
    rpmgit git = rpmgitNew(repofn, 0);
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

static rpmRC cmd_index_pack(int ac, char *av[])
{
    FILE * fp = stderr;
    rpmRC rc = RPMRC_FAIL;
    rpmgit git = rpmgitNew(repofn, 0);
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
    rpmgit git = rpmgitNew(repofn, 0);
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

#define ARGMINMAX(_min, _max)   (int)(((_min) << 8) | ((_max) & 0xff))

static struct poptOption _rpmgitCommandTable[] = {
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

 { "hash-object", '\0', POPT_ARG_MAINCALL,	cmd_hash_object, ARGMINMAX(0,0),
	N_("Show git index."), NULL },

 { "index", '\0', POPT_ARG_MAINCALL,	cmd_index, ARGMINMAX(0,0),
	N_("Show git index."), NULL },
 { "refs", '\0', POPT_ARG_MAINCALL,	cmd_refs, ARGMINMAX(0,0),
	N_("Show git references."), NULL },
 { "config", '\0', POPT_ARG_MAINCALL,	cmd_config, ARGMINMAX(0,0),
	N_("Show git configuration."), NULL },

 { "ls-remote", '\0', POPT_ARG_MAINCALL,	cmd_ls_remote, ARGMINMAX(0,0),
	N_("List remote heads"), N_("GITURI") },
 { "fetch", '\0', POPT_ARG_MAINCALL,		cmd_fetch, ARGMINMAX(0,0),
	N_("Download the packfile from a git server"), N_("GITURI") },
 { "index-pack", '\0', POPT_ARG_MAINCALL,	cmd_index_pack, ARGMINMAX(0,0),
	N_("Index a PACKFILE"), N_("PACKFILE") },
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
    FILE * fp = stderr;
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
    /* XXX -z */
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
    poptContext con = rpmioInit(argc, argv, rpmgitOptionsTable);
    char ** av = (char **) poptGetArgs(con);
    int ac = argvCount((ARGV_t)av);
    int rc = 0;

#ifdef	DYING
    git = rpmgitNew(NULL, 0);

    rc = rpmgitConfig(git);

    rc = rpmgitInfo(git);

    rc = rpmgitWalk(git);

    git = rpmgitFree(git);

/*@i@*/ urlFreeCache();
#endif
    rc = cmd_run(ac, av);

    con = rpmioFini(con);

    return rc;
}

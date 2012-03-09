#include "system.h"

#include <rpmio.h>
#include <ugid.h>
#include <poptIO.h>

#if defined(HAVE_GIT2_H)
#include <git2.h>
#endif

#define	_RPMGIT_INTERNAL
#include <rpmgit.h>

#include "debug.h"

static const char * repofn = "/var/tmp/rpmgit/.git";

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
	/* XXX git_strerror? */
        rpmlog(RPMLOG_ERR, "%s:%s:%u: %s(%d): %s\n",
                func, fn, ln, msg, rc, git_strerror(rc));
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
    FILE * fp = stderr;
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
rpmgitPrintCommit(git, git->C, fp);

rpmgitPrintIndex(git->I, fp);
rpmgitPrintHead(git, NULL, fp);

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);

    git = rpmgitFree(git);
    return rc;
}

/*==============================================================*/

static rpmRC cmd_add(int ac, char *av[])
{
    FILE * fp = stderr;
    rpmRC rc = RPMRC_FAIL;
    const char * fn;
    rpmgit git;
    int xx = -1;
    int i;

assert(ac >= 2);
if (strcmp(av[0], "add")) assert(0);

    fn = (ac >= 2 ? av[1] : repofn);
    git = rpmgitNew(fn, 0);
rpmgitPrintRepo(git, git->R, fp);

    /* XXX Get the index file for this repository. */
    xx = chkgit(git, "git_repository_index",
		git_repository_index((git_index **)&git->I, git->R));
    if (xx)
	goto exit;
rpmgitPrintIndex(git->I, fp);

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

rpmgitPrintIndex(git->I, fp);
rpmgitPrintHead(git, NULL, fp);

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
    FILE * fp = stderr;
    rpmRC rc = RPMRC_FAIL;
    const char * fn;
    rpmgit git;
    int xx = -1;
    int i;

assert(ac >= 2);
if (strcmp(av[0], "commit")) assert(0);

    fn = (ac >= 2 ? av[1] : repofn);
    git = rpmgitNew(fn, 0);
rpmgitPrintRepo(git, git->R, fp);

    /* XXX Get the index file for this repository. */
    xx = chkgit(git, "git_repository_index",
		git_repository_index((git_index **)&git->I, git->R));
    if (xx)
	goto exit;

    /* Commit changes. */
    xx = rpmgitCommit(git, _msg);
    if (xx)
	goto exit;
rpmgitPrintCommit(git, git->C, fp);

rpmgitPrintIndex(git->I, fp);
rpmgitPrintHead(git, NULL, fp);

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
    FILE * fp = stderr;
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

argvPrint(__FUNCTION__, (ARGV_t)av, fp);
assert(ac >= 1);
if (strcmp(av[0], "diff")) assert(0);

    xx = chkgit(git, "git_repository_discover",
	git_repository_discover(path, sizeof(path), dir, 0, "/"));
    if (xx) {
	fprintf(stderr, "Could not discover repository\n");
	goto exit;
    }
    fn = path;
    git = rpmgitNew(fn, 0);
rpmgitPrintRepo(git, git->R, fp);

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
    FILE * fp = stderr;
    rpmRC rc = RPMRC_FAIL;
    const char * fn;
    rpmgit git;
    int xx = -1;
    int i;

argvPrint(__FUNCTION__, (ARGV_t)av, fp);
assert(ac >= 2);
if (strcmp(av[0], "status")) assert(0);

    fn = (ac >= 2 ? av[1] : repofn);
    git = rpmgitNew(fn, 0);

rpmgitPrintRepo(git, git->R, fp);

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);

    git = rpmgitFree(git);
    return rc;
}

/*==============================================================*/

static rpmRC cmd_clone(int ac, char *av[])
{
    FILE * fp = stderr;
    rpmRC rc = RPMRC_FAIL;
    const char * fn;
    rpmgit git;
    int xx = -1;
    int i;

argvPrint(__FUNCTION__, (ARGV_t)av, fp);
assert(ac >= 2);
if (strcmp(av[0], "clone")) assert(0);

    fn = (ac >= 2 ? av[1] : repofn);
    git = rpmgitNew(fn, 0);

rpmgitPrintRepo(git, git->R, fp);

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);

    git = rpmgitFree(git);
    return rc;
}

/*==============================================================*/

static rpmRC cmd_walk(int ac, char *av[])
{
    FILE * fp = stderr;
    rpmRC rc = RPMRC_FAIL;
    const char * fn;
    rpmgit git;
    int xx = -1;
    int i;

argvPrint(__FUNCTION__, (ARGV_t)av, fp);
assert(ac >= 2);
if (strcmp(av[0], "walk")) assert(0);

    fn = (ac >= 2 ? av[1] : repofn);
    git = rpmgitNew(fn, 0);

rpmgitPrintRepo(git, git->R, fp);

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
    return GIT_SUCCESS;
}

static rpmRC cmd_ls_remote(int ac, char *av[])
{
    FILE * fp = stderr;
    rpmRC rc = RPMRC_FAIL;
    rpmgit git = rpmgitNew(repofn, 0);
    git_remote * remote = NULL;
    int xx = -1;

argvPrint(__FUNCTION__, (ARGV_t)av, fp);
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
		git_remote_new(&remote, git->R, av[1], NULL));
	if (xx < GIT_SUCCESS)
	    goto exit;

    } else {
	/* Find the remote by name */
	xx = chkgit(git, "git_remote_load",
		git_remote_load(&remote, git->R, av[1]));
	if (xx < GIT_SUCCESS)
	    goto exit;
    }

    /*
     * When connecting, the underlying code needs to know wether we
     * want to push or fetch
     */
    xx = chkgit(git, "git_remote_connect",
	git_remote_connect(remote, GIT_DIR_FETCH));
    if (xx < GIT_SUCCESS)
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
static int rename_packfile(char *packname, git_indexer *idx)
{
    char path[GIT_PATH_MAX];
    char oid[GIT_OID_HEXSZ + 1];
    char *slash;
    int ret;

    strcpy(path, packname);
    slash = strrchr(path, '/');

    if (!slash)
	return GIT_EINVALIDARGS;

    memset(oid, 0x0, sizeof(oid));
    /*
     * The name of the packfile is given by it's hash which you can get
     * with git_indexer_hash after the index has been written out to
     * disk. Rename the packfile to its "real" name in the same
     * directory as it was originally (libgit2 stores it in the folder
     * where the packs go, so a rename in place is the right thing to do here
     */
    git_oid_fmt(oid, git_indexer_hash(idx));
    ret = sprintf(slash + 1, "pack-%s.pack", oid);
    if (ret < 0)
	return GIT_EOSERR;

    fprintf(stderr, "Renaming pack to %s\n", path);
    return Rename(packname, path);
}

static rpmRC cmd_fetch(int ac, char *av[])
{
    FILE * fp = stderr;
    rpmRC rc = RPMRC_FAIL;
    rpmgit git = rpmgitNew(repofn, 0);
    git_remote *remote = NULL;
    git_indexer *idx = NULL;
    git_indexer_stats stats;
    int xx = -1;
    char *packname = NULL;

argvPrint(__FUNCTION__, (ARGV_t)av, fp);
if (strcmp(av[0], "fetch")) assert(0);
    if (ac != 2)
	goto exit;

    /* Get the remote and connect to it */
    fprintf(fp, "Fetching %s\n", av[1]);
    xx = chkgit(git, "git_remote_new",
	git_remote_new(&remote, git->R, av[1], NULL));
    if (xx < GIT_SUCCESS)
	goto exit;

    xx = chkgit(git, "git_remote_connect",
	git_remote_connect(remote, GIT_DIR_FETCH));
    if (xx < GIT_SUCCESS)
	goto exit;

    /*
     * Download the packfile from the server. As we don't know its hash
     * yet, it will get a temporary filename
     */
    xx = chkgit(git, "git_remote_download",
	git_remote_download(&packname, remote));
    if (xx < GIT_SUCCESS)
	goto exit;

    /* No error and a NULL packname means no packfile was needed */
    if (packname != NULL) {
	fprintf(fp, "The packname is %s\n", packname);

	/* Create a new instance indexer */
	xx = chkgit(git, "git_indexer_new",
		git_indexer_new(&idx, packname));
	if (xx < GIT_SUCCESS)
	    goto exit;

	/* Should be run in parallel, but too complicated for the example */
	xx = chkgit(git, "git_indexer_run",
		git_indexer_run(idx, &stats));
	if (xx < GIT_SUCCESS)
	    goto exit;

	fprintf(fp, "Received %d objects\n", stats.total);

	/*
	 * Write the index file. The index will be stored with the
	 * correct filename
	 */
	xx = chkgit(git, "git_indexer_write",
		git_indexer_write(idx));
	if (xx < GIT_SUCCESS)
	    goto exit;

	xx = chkgit(git, "rename_packfile",
		rename_packfile(packname, idx));
	if (xx < GIT_SUCCESS)
	    goto exit;
    }

    /*
     * Update the references in the remote's namespace to point to the
     * right commits. This may be needed even if there was no packfile
     * to download, which can happen e.g. when the branches have been
     * changed but all the needed objects are available locally.
     */
    xx = chkgit(git, "git_remote_update_tips",
		git_remote_update_tips(remote));
    if (xx < GIT_SUCCESS)
	goto exit;

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);
    if (packname)
	free(packname);
    if (idx)
	git_indexer_free(idx);
    if (remote)
	git_remote_free(remote);
    git = rpmgitFree(git);
    return rc;
}

/*==============================================================*/
static rpmRC cmd_index_pack(int ac, char *av[])
{
    FILE * fp = stderr;
    rpmRC rc = RPMRC_FAIL;
    rpmgit git = rpmgitNew(repofn, 0);
    git_indexer *indexer = NULL;
    git_indexer_stats stats;
    char hash[GIT_OID_HEXSZ + 1] = {0};
    int xx;

argvPrint(__FUNCTION__, (ARGV_t)av, fp);
if (strcmp(av[0], "index-pack")) assert(0);
    if (ac != 2)
	goto exit;

    /* Create a new indexer */
    xx = chkgit(git, "git_indexer_new",
	git_indexer_new(&indexer, av[1]));
    if (xx < GIT_SUCCESS)
	goto exit;
         
    /*
     * Index the packfile. This function can take a very long time and
     * should be run in a worker thread.
     */
    xx = chkgit(git, "git_indexer_run",
	git_indexer_run(indexer, &stats));
    if (xx < GIT_SUCCESS)
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

 { "ls-remote", '\0', POPT_ARG_MAINCALL,	cmd_ls_remote, ARGMINMAX(0,0),
	N_("List remote heads"), N_("GITURI") },
 { "fetch", '\0', POPT_ARG_MAINCALL,		cmd_fetch, ARGMINMAX(0,0),
	N_("Download the packfile from a git server"), N_("GITURI") },
 { "index-pack", '\0', POPT_ARG_MAINCALL,	cmd_index_pack, ARGMINMAX(0,0),
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
    const char * cmd = av[0];
    rpmRC rc = RPMRC_FAIL;

argvPrint(__FUNCTION__, (ARGV_t)av, fp);
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

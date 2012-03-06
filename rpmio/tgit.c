#include "system.h"

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
static int show_ref__cb(git_remote_head *head, void *payload)
{
    char oid[GIT_OID_HEXSZ + 1] = {0};
    git_oid_fmt(oid, &head->oid);
    printf("%s\t%s\n", oid, head->name);
    return GIT_SUCCESS;
}

static rpmRC cmd_ls_remote(int ac, char *av[])
{
    rpmRC rc = RPMRC_FAIL;
    rpmgit git = rpmgitNew(repofn, 0);
    git_remote * remote = NULL;
    int xx;

argvPrint(__FUNCTION__, (ARGV_t)av, stderr);
assert(!strcmp("--ls-remote", av[0]));
    if (ac != 2)
	goto exit;

    /* If there's a ':' in the name, assume it's an URL */
    if (strchr(av[1], ':') != NULL) {
	/*
	 * Create an instance of a remote from the URL. The transport to use
	 * is detected from the URL
	 */
	xx = chkgit(git, "git_remote_new",
		git_remote_new(&remote, git->repo, av[1], NULL));
	if (xx < GIT_SUCCESS)
	    goto exit;

    } else {
	/* Find the remote by name */
	xx = chkgit(git, "git_remote_load",
		git_remote_load(&remote, git->repo, av[1]));
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
    rpmRC rc = RPMRC_FAIL;
    rpmgit git = rpmgitNew(repofn, 0);
    git_remote *remote = NULL;
    git_indexer *idx = NULL;
    git_indexer_stats stats;
    int xx;
    char *packname = NULL;

argvPrint(__FUNCTION__, (ARGV_t)av, stderr);
assert(!strcmp("--fetch", av[0]));
    if (ac != 2)
	goto exit;

    /* Get the remote and connect to it */
    fprintf(stderr, "Fetching %s\n", av[1]);
    xx = chkgit(git, "git_remote_new",
	git_remote_new(&remote, git->repo, av[1], NULL));
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
	fprintf(stderr, "The packname is %s\n", packname);

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

	fprintf(stderr, "Received %d objects\n", stats.total);

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
    FILE * fp = stdout;
    rpmRC rc = RPMRC_FAIL;
    rpmgit git = rpmgitNew(repofn, 0);
    git_indexer *indexer = NULL;
    git_indexer_stats stats;
    char hash[GIT_OID_HEXSZ + 1] = {0};
    int xx;

argvPrint(__FUNCTION__, (ARGV_t)av, stderr);
assert(!strcmp("--index-pack", av[0]));
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

static struct poptOption rpmgitOptionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL,			&_rpmmg_debug, -1,
	NULL, NULL },

 { "ls-remote", '\0', POPT_ARG_MAINCALL,	cmd_ls_remote, 0,
	N_("List remote heads"), N_("GITURI") },
 { "fetch", '\0', POPT_ARG_MAINCALL,		cmd_fetch, 0,
	N_("Download the packfile from a git server"), N_("GITURI") },
 { "index-pack", '\0', POPT_ARG_MAINCALL,	cmd_index_pack, 0,
	N_("Index a PACKFILE"), N_("PACKFILE") },

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
    int rc = 0;

#ifdef	DYING
    git = rpmgitNew(NULL, 0);

    rc = rpmgitConfig(git);

    rc = rpmgitInfo(git);

    rc = rpmgitWalk(git);

    git = rpmgitFree(git);

/*@i@*/ urlFreeCache();
#endif

    return rc;
}

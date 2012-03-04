/** \ingroup rpmio
 * \file rpmio/rpmgit.c
 */

#include "system.h"

#include <rpmiotypes.h>
#include <rpmio.h>	/* for *Pool methods */
#include <rpmlog.h>
#include <rpmurl.h>
#include <ugid.h>

#if defined(HAVE_GIT2_H)
#include <git2.h>
#endif

#define	_RPMGIT_INTERNAL
#include <rpmgit.h>

#include "debug.h"

/*@unchecked@*/
int _rpmgit_debug = 0;

#define	SPEW(_t, _rc, _git)	\
  { if ((_t) || _rpmgit_debug ) \
	fprintf(stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, (_git), \
		(_rc)); \
  }

/*==============================================================*/
/*@-redef@*/
typedef struct key_s {
    uint32_t	v;
/*@observer@*/
    const char *n;
} KEY;
/*@=redef@*/

/*@observer@*/
static const char * tblName(uint32_t v, KEY * tbl, size_t ntbl)
	/*@*/
{
    const char * n = NULL;
    static char buf[32];
    size_t i;

    for (i = 0; i < ntbl; i++) {
	if (v != tbl[i].v)
	    continue;
	n = tbl[i].n;
	break;
    }
    if (n == NULL) {
	(void) snprintf(buf, sizeof(buf), "0x%x", (unsigned)v);
	n = buf;
    }
    return n;
}

static const char * fmtBits(uint32_t flags, KEY tbl[], size_t ntbl, char *t)
	/*@modifies t @*/
{
    char pre = '<';
    char * te = t;
    int i;

    sprintf(t, "0x%x", (unsigned)flags);
    te = t;
    te += strlen(te);
    for (i = 0; i < 32; i++) {
	uint32_t mask = (1 << i);
	const char * name;

	if (!(flags & mask))
	    continue;

	name = tblName(mask, tbl, ntbl);
	*te++ = pre;
	pre = ',';
	te = stpcpy(te, name);
    }
    if (pre == ',') *te++ = '>';
    *te = '\0';
    return t;
}

#define _ENTRY(_v)      { GIT_IDXENTRY_##_v, #_v, }

/*@unchecked@*/ /*@observer@*/
static KEY IDXEflags[] = {
    _ENTRY(UPDATE),
    _ENTRY(REMOVE),
    _ENTRY(UPTODATE),
    _ENTRY(ADDED),
    _ENTRY(HASHED),		/* XXX not in DBENV->open() doco */
    _ENTRY(UNHASHED),
    _ENTRY(WT_REMOVE),
    _ENTRY(CONFLICTED),
    _ENTRY(UNPACKED),
    _ENTRY(NEW_SKIP_WORKTREE),

    _ENTRY(INTENT_TO_ADD),
    _ENTRY(SKIP_WORKTREE),
    _ENTRY(EXTENDED2),
};
/*@unchecked@*/
static size_t nIDXEflags = sizeof(IDXEflags) / sizeof(IDXEflags[0]);
/*@observer@*/
static const char * fmtIDXEflags(uint32_t flags)
	/*@*/
{
    static char buf[BUFSIZ];
    char * te = buf;
    te = stpcpy(te, "\n\tflags: ");
    (void) fmtBits(flags, IDXEflags, nIDXEflags, te);
    return buf;
}
#define	_IDXFLAGS(_idxeflags)	fmtIDXEflags(_idxeflags)

/*==============================================================*/

static int Xchkgit(/*@unused@*/ rpmgit git, const char * msg,
                int error, int printit,
                const char * func, const char * fn, unsigned ln)
{
    int rc = error;

    if (printit && rc) {
	/* XXX git_strerror? */
        rpmlog(RPMLOG_ERR, "%s:%s:%u: %s(%d)\n",
                func, fn, ln, msg, rc);
    }

    return rc;
}
#define chkgit(_git, _msg, _error)  \
    Xchkgit(_git, _msg, _error, _rpmgit_debug, __FUNCTION__, __FILE__, __LINE__)

/*==============================================================*/

static int rpmgitConfigPrint(const char * var_name, const char * value,
		void * _git)
{
    rpmgit git = (rpmgit) _git;
    FILE * fp = stdout;
    int rc = 0;

    fprintf(fp, "%s: %s\n", var_name, value);

    return rc;
}

int rpmgitConfig(rpmgit git)
{
    int rc = -1;
#if defined(WITH_LIBGIT2)

    rc = chkgit(git, "git_repository_config",
		git_repository_config(&git->cfg, git->repo));

    rc = chkgit(git, "git_config_foreach",
		git_config_foreach(git->cfg, rpmgitConfigPrint, git));
#endif
SPEW(0, rc, git);
    return rc;
}

int rpmgitTree(rpmgit git)
{
    int rc = -1;
#if defined(WITH_LIBGIT2)
    const git_tree_entry *entry;
    git_object *objt;

#endif
SPEW(0, rc, git);
    return rc;
}

int rpmgitWalk(rpmgit git)
{
    int rc = -1;
#if defined(WITH_LIBGIT2) && defined(NOTYET)
    FILE * fp = stdout;
    git_oid oid;
    int xx;

    xx = chkgit(git, "git_revwalk_new",
		git_revwalk_new(&git->walk, git->repo));
    git_revwalk_sorting(git->walk, GIT_SORT_TOPOLOGICAL | GIT_SORT_REVERSE);
    xx = chkgit(git, "git_revwalk_push_head",
		git_revwalk_push_head(git->walk));

    while ((xx = chkgit(git, "git_revwalk_next",
		git_revwalk_next(&oid, git->walk))) == GIT_SUCCESS)
    {
	git_commot *wcommit;
	const git_signature * cauth;
	const char * cmsg;

	git_oid_fmt(t, &oid);
	t[GIT_OID_HEXSZ] = '\0';
	fprintf(fp, "\t  oid: %s", t);

	xx = chkgit(git, "git_commit_lookup",
		git_commit_lookup(&wcommit, git->repo, &oid));
	cmsg  = git_commit_message(wcommit);
	cauth = git_commit_author(wcommit);
	fprintf(fp, "\n\t%s (%s)", cmsg, cauth->email);
	git_commit_free(wcommit);
	
	fprintf(fp, "\n");
    }

    git_revwalk_free(git->walk);
    git->walk = NULL;
    rc = 0;	/* XXX */
#endif
SPEW(0, rc, git);
    return rc;
}

int rpmgitInfo(rpmgit git)
{
    int rc = -1;
#if defined(WITH_LIBGIT2)
    FILE * fp = stdout;
    unsigned ecount;
    unsigned i;
    size_t nt = 128;
    char * t = alloca(nt);
    size_t nb;
    int xx;

    xx = chkgit(git, "git_repository_index",
		git_repository_index(&git->index, git->repo));
    xx = chkgit(git, "git_index_read",
		git_index_read(git->index));

    ecount = git_index_entrycount(git->index);
    for (i = 0; i < ecount; i++) {
	static const char _fmt[] = "%c";
	git_index_entry * e = git_index_get(git->index, i);
	git_oid oid = e->oid;
	time_t mtime;
	struct tm tm;

	fprintf(fp, "=== %s:", e->path);

	git_oid_fmt(t, &oid);
	t[GIT_OID_HEXSZ] = '\0';
	fprintf(fp, "\n\t  oid: %s", t);

	fprintf(fp, "\n\t  dev: %x", (unsigned)e->dev);
	fprintf(fp, "\n\t  ino: %lu", (unsigned long)e->ino);
	fprintf(fp, "\n\t mode: %o", (unsigned)e->mode);

	fprintf(fp, "\n\t user: %s", uidToUname((uid_t)e->uid));
	fprintf(fp, "\n\tgroup: %s", gidToGname((gid_t)e->gid));

	fprintf(fp, "\n\t size: %lu", (unsigned long)e->file_size);

	mtime = e->ctime.seconds;
	nb = strftime(t, nt, _fmt, localtime_r(&mtime, &tm));
	fprintf(fp, "\n\tctime: %s", t);

	mtime = e->mtime.seconds;
	nb = strftime(t, nt, _fmt, localtime_r(&mtime, &tm));
	fprintf(fp, "\n\tmtime: %s", t);

	fprintf(fp, "%s", _IDXFLAGS(e->flags));

	fprintf(fp, "\n");
    }

    git_index_free(git->index);
    git->index = NULL;
    rc = 0;
#endif
SPEW(0, rc, git);
    return rc;
}

int rpmgitRead(rpmgit git)
{
    int rc = -1;
#if defined(WITH_LIBGIT2) && defined(NOTYET)
    FILE * fp = stdout;
    git_odb * odb = git_repository_database(git->repo);
    git_odb_object * obj;
    git_oid oid;
    unsigned char * data;
    const char * str_type;
    git_otype otype;

    rc = git_odb_read(&obj, odb, &oid);
    data = (char *)git_odb_object_data(obj);
    otype = git_odb_object_type(oid);
    str_type = git_object_type2string(otype);
    fprintf(fp, "object length and type: %d, %s\n",
	(int)git_odb_object_size(obj), str_type);

    git_odb_object_close(obj);

    rc = 0;	/* XXX */
#endif
SPEW(0, rc, git);
    return rc;
}

int rpmgitWrite(rpmgit git)
{
    int rc = -1;
#if defined(WITH_LIBGIT2) && defined(NOTYET)
    FILE * fp = stdout;
    git_odb * odb = git_repository_database(git->repo);
    git_oid oid;
    size_t nb = GIT_OID_HEXSZ;
    char * b = alloca(nb+1);

    nb = strncpy(b, nb, "blah blah") - b;
    rc = git_odb_write(&oid, odb, b, nb, GIT_OBJ_BLOB);
    git_oid_format(b, &oid);
    fprintf(fp, "Written Object: %s\n", b);

    rc = 0;	/* XXX */
#endif
SPEW(0, rc, git);
    return rc;
}

/*==============================================================*/

static void rpmgitFini(void * _git)
	/*@globals fileSystem @*/
	/*@modifies *_git, fileSystem @*/
{
    rpmgit git = _git;

#if defined(WITH_LIBGIT2)
    if (git->cmtter)
	git_signature_free(git->cmtter);
    if (git->author)
	git_signature_free(git->author);
    if (git->commit)
	git_commit_free(git->commit);
    if (git->cfg)
	git_config_free(git->cfg);
    if (git->index)
	git_index_free(git->index);
    if (git->repo)
	git_repository_free(git->repo);
#endif
    git->cmtter = NULL;
    git->author = NULL;
    git->commit = NULL;
    git->cfg = NULL;
    git->index = NULL;
    git->repo = NULL;
    git->fn = _free(git->fn);
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmgitPool = NULL;

static rpmgit rpmgitGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmgitPool, fileSystem @*/
	/*@modifies pool, _rpmgitPool, fileSystem @*/
{
    rpmgit git;

    if (_rpmgitPool == NULL) {
	_rpmgitPool = rpmioNewPool("git", sizeof(*git), -1, _rpmgit_debug,
			NULL, NULL, rpmgitFini);
	pool = _rpmgitPool;
    }
    git = (rpmgit) rpmioGetPool(pool, sizeof(*git));
    memset(((char *)git)+sizeof(git->_item), 0, sizeof(*git)-sizeof(git->_item));
    return git;
}

rpmgit rpmgitNew(const char * fn, int flags)
{
    static const char _gitfn[] = "/var/tmp/rpmgit/.git";
    rpmgit git = rpmgitGetPool(_rpmgitPool);
    int xx;

    if (fn == NULL)
	fn = _gitfn;

    if (fn)
	git->fn = xstrdup(fn);

#if defined(WITH_LIBGIT2)
    git_libgit2_version(&git->major, &git->minor, &git->rev);
    xx = chkgit(git, "git_repository_open",
		git_repository_open(&git->repo, fn));
#endif

    return rpmgitLink(git);
}

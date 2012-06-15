/** \ingroup rpmio
 * \file rpmio/rpmgit.c
 */

#include "system.h"

#include <rpmiotypes.h>
#include <rpmio.h>	/* for *Pool methods */
#include <rpmlog.h>
#include <rpmurl.h>
#include <ugid.h>

#define	_RPMGIT_INTERNAL
#include <rpmgit.h>

#include "debug.h"

/*@unchecked@*/
int _rpmgit_debug = 0;
/*@unchecked@*/
const char * _rpmgit_dir;	/* XXX GIT_DIR */
/*@unchecked@*/
const char * _rpmgit_tree;	/* XXX GIT_WORK_TREE */

#define	SPEW(_t, _rc, _git)	\
  { if ((_t) || _rpmgit_debug ) \
	fprintf(stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, (_git), \
		(_rc)); \
  }

/*==============================================================*/
#if defined(WITH_LIBGIT2)
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
    _ENTRY(HASHED),
    _ENTRY(UNHASHED),
    _ENTRY(WT_REMOVE),
    _ENTRY(CONFLICTED),
    _ENTRY(UNPACKED),
    _ENTRY(NEW_SKIP_WORKTREE),

    _ENTRY(INTENT_TO_ADD),
    _ENTRY(SKIP_WORKTREE),
    _ENTRY(EXTENDED2),
};
#undef	_ENTRY
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

#define _ENTRY(_v)      { GIT_REF_##_v, #_v, }
/*@unchecked@*/ /*@observer@*/
static KEY REFflags[] = {
    _ENTRY(OID),
    _ENTRY(SYMBOLIC),
    _ENTRY(PACKED),
    _ENTRY(HAS_PEEL),
};
#undef	_ENTRY
/*@unchecked@*/
static size_t nREFflags = sizeof(REFflags) / sizeof(REFflags[0]);
/*@observer@*/
static const char * fmtREFflags(uint32_t flags)
	/*@*/
{
    static char buf[BUFSIZ];
    char * te = buf;
    te = stpcpy(te, "\n\tflags: ");
    (void) fmtBits(flags, REFflags, nREFflags, te);
    return buf;
}
#define	_REFFLAGS(_refflags)	fmtREFflags(_refflags)

/*==============================================================*/

static int Xchkgit(/*@unused@*/ rpmgit git, const char * msg,
                int error, int printit,
                const char * func, const char * fn, unsigned ln)
	/*@*/
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

void rpmgitPrintOid(const char * msg, const void * _oidp, void * _fp)
{
    FILE * fp = (_fp ? _fp : stderr);
    const git_oid * oidp = _oidp;
    char * t;
assert(oidp != NULL);
    t = git_oid_allocfmt(oidp);
    git_oid_fmt(t, oidp);
if (msg) fprintf(fp, "%s:", msg);
fprintf(fp, " %s\n", t);
    t = _free(t);
}

void rpmgitPrintTime(const char * msg, time_t _Ctime, void * _fp)
{
    FILE * fp = (_fp ? _fp : stderr);
    static const char _fmt[] = "%c";
    struct tm tm;
    char _b[BUFSIZ];
    size_t _nb = sizeof(_b);
    size_t nw = strftime(_b, _nb-1, _fmt, localtime_r(&_Ctime, &tm));
    (void)nw;
if (msg) fprintf(fp, "%s:", msg);
fprintf(fp, " %s", _b);
}

void rpmgitPrintSig(const char * msg, const void * _S, void * _fp)
{
    FILE * fp = (_fp ? _fp : stderr);
    const git_signature * S = _S;
assert(S != NULL);
if (msg) fprintf(fp, "%s:", msg);
fprintf(fp, " %s <%s>", S->name, S->email);
#ifdef	DYING
rpmgitPrintTime(NULL, (time_t)S->when.time, fp);
#else
fprintf(fp, " %lu", (unsigned long)S->when.time);
fprintf(fp, " %.02d%.02d", (S->when.offset/60), (S->when.offset%60));
#endif
fprintf(fp, "\n");
}

void rpmgitPrintIndex(void * _I, void * _fp)
{
    FILE * fp = (FILE *) _fp;
    git_index * I = (git_index *) _I;
    unsigned Icnt;
    unsigned i;

assert(I != NULL);
if (_rpmgit_debug < 0 && fp == NULL) fp = stderr;
if (fp == NULL) return;

    Icnt = git_index_entrycount(I);
    fprintf(fp, "-------- Index(%u)\n", Icnt);
    for (i = 0; i < Icnt; i++) {
	git_index_entry * E = git_index_get(I, i);

	fprintf(fp, "=== %s:", E->path);

     rpmgitPrintOid("\n\t  oid", &E->oid, fp);

	fprintf(fp,   "\t  dev: %x", (unsigned)E->dev);
	fprintf(fp, "\n\t  ino: %lu", (unsigned long)E->ino);
	fprintf(fp, "\n\t mode: %o", (unsigned)E->mode);

	fprintf(fp, "\n\t user: %s", uidToUname((uid_t)E->uid));
	fprintf(fp, "\n\tgroup: %s", gidToGname((gid_t)E->gid));

	fprintf(fp, "\n\t size: %lu", (unsigned long)E->file_size);

    rpmgitPrintTime("\n\tctime", E->ctime.seconds, fp);

    rpmgitPrintTime("\n\tmtime", E->mtime.seconds, fp);

	fprintf(fp, "%s", _IDXFLAGS(E->flags));

	fprintf(fp, "\n");
    }
}

#ifdef	DYING
static const char * rpmgitOtype(git_otype otype)
	/*@*/
{
    const char * s = "unknown";
    switch(otype) {
    case GIT_OBJ_ANY:		s = "any";	break;
    case GIT_OBJ_BAD:		s = "bad";	break;
    case GIT_OBJ__EXT1:		s = "EXT1";	break;
    case GIT_OBJ_COMMIT:	s = "commit";	break;
    case GIT_OBJ_TREE:		s = "tree";	break;
    case GIT_OBJ_BLOB:		s = "blob";	break;
    case GIT_OBJ_TAG:		s = "tag";	break;
    case GIT_OBJ__EXT2:		s = "EXT2";	break;
    case GIT_OBJ_OFS_DELTA:	s = "delta off";	break;
    case GIT_OBJ_REF_DELTA:	s = "delta oid";	break;
    }
    return s;
}
#endif

void rpmgitPrintTree(void * _T, void * _fp)
{
    git_tree * T = (git_tree *) _T;
    FILE * fp = (FILE *) _fp;
    unsigned Tcnt;
    unsigned i;

assert(T != NULL);
if (_rpmgit_debug < 0 && fp == NULL) fp = stderr;
if (fp == NULL) return;

#ifdef	DYING
 rpmgitPrintOid("-------- Toid", git_tree_id(T), fp);
#endif
    Tcnt = git_tree_entrycount(T);
#ifdef	DYING
fprintf(fp,     "         Tcnt: %u\n", Tcnt);
#endif
    for (i = 0; i < Tcnt; i++) {
	const git_tree_entry * E = git_tree_entry_byindex(T, i);
	char * t;
assert(E != NULL);
#ifdef	DYING
fprintf(fp,     "       Eattrs: 0%o\n", git_tree_entry_attributes(E));
fprintf(fp,     "        Ename: %s\n", git_tree_entry_name(E));
 rpmgitPrintOid("         Eoid", git_tree_entry_id(E), fp);
fprintf(fp,     "        Etype: %s\n", rpmgitOtype(git_tree_entry_type(E)));
#else
	t = git_oid_allocfmt(git_tree_entry_id(E));
	fprintf(fp, "%06o %.4s %s\t%s\n",
		git_tree_entry_attributes(E),
		git_object_type2string(git_tree_entry_type(E)),
		t,
		git_tree_entry_name(E));
	t = _free(t);
#endif
    }
}

void rpmgitPrintCommit(rpmgit git, void * _C, void * _fp)
{
    FILE * fp = (FILE *) _fp;
    git_commit * C = _C;
    unsigned Pcnt;
    unsigned i;
    int xx;

assert(C != NULL);
if (_rpmgit_debug < 0 && fp == NULL) fp = stderr;
if (fp == NULL) return;

#ifdef	DYING
 rpmgitPrintOid("-------- Coid", git_commit_id(C), fp);
fprintf(fp,     "      Cmsgenc: %s\n", git_commit_message_encoding(C));
fprintf(fp,     "         Cmsg: %s\n", git_commit_message(C));

rpmgitPrintTime("        Ctime", git_commit_time(C), fp);
fprintf(fp, "\n");

fprintf(fp,     "          Ctz: %d\n", git_commit_time_offset(C));
 rpmgitPrintSig("      Cauthor", git_commit_author(C), fp);
 rpmgitPrintSig("      Ccmtter", git_commit_committer(C), fp);
 rpmgitPrintOid("         Toid", git_commit_tree_oid(C), fp);

    Pcnt = git_commit_parentcount(C);
fprintf(fp,     "         Pcnt: %u\n", Pcnt);
    for (i = 0; i < Pcnt; i++) {
	git_commit * E;
	xx = chkgit(git, "git_commit_parent",
			git_commit_parent(&E, C, i));
	const git_oid * Poidp = git_commit_parent_oid(E, i);
 rpmgitPrintOid("         Poid", Poidp, fp);
    }
#else
 rpmgitPrintOid("tree", git_commit_tree_oid(C), fp);
    Pcnt = git_commit_parentcount(C);
    for (i = 0; i < Pcnt; i++)
	rpmgitPrintOid("parent", git_commit_parent_oid(C, i), fp);
 rpmgitPrintSig("author", git_commit_author(C), fp);
 rpmgitPrintSig("committer", git_commit_committer(C), fp);
fprintf(fp,     "encoding: %s\n", git_commit_message_encoding(C));
fprintf(fp,     "\n%s", git_commit_message(C));
#endif
}

void rpmgitPrintTag(rpmgit git, void * _tag, void * _fp)
{
    FILE * fp = (_fp ? _fp : stderr);
    git_tag * tag = (git_tag *) _tag;
    int xx;

assert(tag != NULL);
#ifdef	NOTYET
if (_rpmgit_debug >= 0) return;
#endif

#ifdef	DYING
 rpmgitPrintOid("--------  TAG", git_tag_id(tag), fp);
fprintf(fp,     "         name: %s\n", git_tag_name(tag));
fprintf(fp,     "         type: %s\n", git_object_type2string(git_tag_type(tag)));
 rpmgitPrintOid("       target", git_tag_target_oid(tag), fp);
 rpmgitPrintSig("       tagger", git_tag_tagger(tag), fp);
fprintf(fp,     "\n%s", git_tag_message(tag));
#else
 rpmgitPrintOid("object", git_tag_target_oid(tag), fp);
fprintf(fp,     "type: %s\n", git_object_type2string(git_tag_type(tag)));
fprintf(fp,     "tag: %s\n", git_tag_name(tag));
/* XXX needs strftime(3) */
 rpmgitPrintSig("tagger", git_tag_tagger(tag), fp);
fprintf(fp,     "\n%s", git_tag_message(tag));
#endif

}

void rpmgitPrintHead(rpmgit git, void * _H, void * _fp)
{
    FILE * fp = (_fp ? _fp : stderr);
    git_reference * H = (_H ? _H : git->H);
    git_reference * Hresolved = NULL;
    int xx;

    if (H == NULL) {
	xx = chkgit(git, "git_repository_head",
		git_repository_head((git_reference **)&git->H, git->R));
	H = git->H;
    }
assert(H != NULL);
if (_rpmgit_debug >= 0) return;

    xx = chkgit(git, "git_reference_resolve",
		git_reference_resolve(&Hresolved, H));

 rpmgitPrintOid("-------- Hoid", git_reference_oid(H), fp);
fprintf(fp,     "      Htarget: %s\n", git_reference_target(H));
fprintf(fp,     "        Hname: %s\n", git_reference_name(H));
fprintf(fp,     "    Hresolved: %p\n", Hresolved);
fprintf(fp,     "       Howner: %p", git_reference_owner(H));
#ifdef	DYING
fprintf(fp,     "       Hrtype: %d\n", (int)git_reference_type(H));
#else
fprintf(fp,     "%s\n", _REFFLAGS(git_reference_type(H)));
#endif

    if (Hresolved)	/* XXX leak */
	git_reference_free(Hresolved);

}

void rpmgitPrintRepo(rpmgit git, void * _R, void * _fp)
{
    FILE * fp = (_fp ? _fp : stderr);
    git_repository * R = _R;
    const char * fn;

if (_rpmgit_debug >= 0) return;

fprintf(fp, "head_detached: %d\n", git_repository_head_detached(R));
fprintf(fp, "  head_orphan: %d\n", git_repository_head_orphan(R));
fprintf(fp, "     is_empty: %d\n", git_repository_is_empty(R));
fprintf(fp, "      is_bare: %d\n", git_repository_is_bare(R));
    fn = git_repository_path(R);
fprintf(fp, "         path: %s\n", fn);
    fn = git_repository_workdir(R);
fprintf(fp, "      workdir: %s\n", fn);
    /* XXX get_repository_config */
    /* XXX get_repository_odb */
    /* XXX get_repository_index */

}
#endif	/* defined(WITH_LIBGT2) */

/*==============================================================*/
int rpmgitInit(rpmgit git)
{
    int rc = -1;
#if defined(WITH_LIBGIT2)
    static const unsigned _is_bare = 0;		/* XXX W2DO? */

    if (git->R) {	/* XXX leak */
	git_repository_free(git->R);
	git->R = NULL;
    }
    rc = chkgit(git, "git_repository_init",
		git_repository_init((git_repository **)&git->R, git->fn, _is_bare));
    if (rc)
	goto exit;
if (_rpmgit_debug < 0) rpmgitPrintRepo(git, git->R, git->fp);

    /* Add an empty index to the new repository. */
    rc = chkgit(git, "git_repository_index",
		git_repository_index((git_index **)&git->I, git->R));
    if (rc)
	goto exit;

    /* XXX Clear the index??? */
    rc = chkgit(git, "git_index_read",
		git_index_read(git->I));
    git_index_clear(git->I);
    rc = chkgit(git, "git_index_write",
		git_index_write(git->I));

exit:
#endif	/* defined(WITH_LIBGT2) */
SPEW(0, rc, git);
    return rc;
}

int rpmgitAddFile(rpmgit git, const char * fn)
{
    int rc = -1;
#if defined(WITH_LIBGIT2)
    int _stage = 0;	/* XXX W2DO? */

    /* XXX TODO: strip out workdir prefix if present. */

    /* Upsert the file into the index. */
    rc = chkgit(git, "git_index_add",
		git_index_add(git->I, fn, _stage));
    if (rc)
	goto exit;

    /* Write the index to disk. */
    rc = chkgit(git, "git_index_write",
		git_index_write(git->I));
    if (rc)
	goto exit;

exit:
#endif	/* defined(WITH_LIBGT2) */
SPEW(0, rc, git);
    return rc;
}

int rpmgitCommit(rpmgit git, const char * msg)
{
    int rc = -1;
#if defined(WITH_LIBGIT2)
    static const char _msg[] = "WDJ commit";
    static const char * update_ref = "HEAD";
    static const char * message_encoding = "UTF-8";
    static int parent_count = 0;
    static const git_commit ** parents = NULL;

    const char * message = (msg ? msg : _msg);
    const char * user_name =
	(git->user_name ? git->user_name : "Jeff Johnson");
    const char * user_email =
	(git->user_email ? git->user_email : "jbj@jbj.org");

    git_oid _oidC;
    git_oid * Coidp = &_oidC;
    git_signature * Cauthor = NULL;
    git_signature * Ccmtter = NULL;

    git_oid _oidT;

    /* Find the root tree oid. */
    rc = chkgit(git, "git_tree_create_fromindex",
		git_tree_create_fromindex(&_oidT, git->I));
    if (rc)
	goto exit;
if (_rpmgit_debug < 0) rpmgitPrintOid("         oidT", &_oidT, NULL);

    /* Find the tree object. */
    rc = chkgit(git, "git_tree_lookup",
		git_tree_lookup((git_tree **)&git->T, git->R, &_oidT));
    if (rc)
	goto exit;

if (_rpmgit_debug < 0) rpmgitPrintTree(git->T, NULL);

    /* Commit the added file. */
    rc = chkgit(git, "git_signature_now",
		git_signature_now(&Cauthor, user_name, user_email));
    if (rc)
	goto exit;
    rc = chkgit(git, "git_signature_now",
		git_signature_now(&Ccmtter, user_name, user_email));
    if (rc)
	goto exit;

    rc = chkgit(git, "git_commit_create",
		git_commit_create(Coidp, git->R, update_ref,
			Cauthor, Ccmtter,
			message_encoding, message,
			git->T,
			parent_count, parents));
    if (rc)
	goto exit;

if (_rpmgit_debug < 0) rpmgitPrintOid("         oidC", Coidp, NULL);

    /* Find the commit object. */
    rc = chkgit(git, "git_commit_lookup",
		git_commit_lookup((git_commit **)&git->C, git->R, Coidp));

    if (git->T) {	/* XXX leak */
	git_tree_free(git->T);
	git->T = NULL;
    }
    rc = chkgit(git, "git_commit_tree",
		git_commit_tree((git_tree **)&git->T, git->C));

exit:
    if (Cauthor)
	git_signature_free((git_signature *)Cauthor);
    if (Ccmtter)
	git_signature_free((git_signature *)Ccmtter);
#endif	/* defined(WITH_LIBGT2) */
SPEW(0, rc, git);

    return rc;
}

static int rpmgitConfigCB(const char * var_name, const char * value,
                void * _git)
{
    rpmgit git = (rpmgit) _git;
    int rc = 0;

    if (!strcmp("core.bare", var_name)) {
	git->core_bare = strcmp(value, "false") ? 1 : 0;
    } else
    if (!strcmp("core.repositoryformatversion", var_name)) {
	git->core_repositoryformatversion = atol(value);
    } else
    if (!strcmp("user.name", var_name)) {
	git->user_name = _free(git->user_name);
	git->user_name = xstrdup(value);
    } else
    if (!strcmp("user.email", var_name)) {
	git->user_email = _free(git->user_email);
	git->user_email = xstrdup(value);
    }
    if (git->fp)
	fprintf(git->fp, "%s: %s\n", var_name, value);
SPEW(0, rc, git);

    return rc;
}

int rpmgitConfig(rpmgit git)
{
    int rc = -1;
#if defined(WITH_LIBGIT2)

    /* Read/print/save configuration info. */
    rc = chkgit(git, "git_repository_config",
		git_repository_config((git_config **)&git->cfg, git->R));
    if (rc)
	goto exit;

    rc = chkgit(git, "git_config_foreach",
		git_config_foreach(git->cfg, rpmgitConfigCB, git));
    if (rc)
	goto exit;

exit:
#endif	/* defined(WITH_LIBGT2) */
SPEW(0, rc, git);
    return rc;
}

/*==============================================================*/

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
#if defined(WITH_LIBGIT2)
    FILE * fp = (git->fp ? git->fp : stdout);
    git_revwalk * walk;
    git_oid oid;
    int xx;

    xx = chkgit(git, "git_revwalk_new",
		git_revwalk_new(&walk, git->R));
    git_revwalk_sorting(walk, GIT_SORT_TOPOLOGICAL | GIT_SORT_REVERSE);
    xx = chkgit(git, "git_revwalk_push_head",
		git_revwalk_push_head(walk));
    git->walk = (void *) walk;

    while ((xx = chkgit(git, "git_revwalk_next",
		git_revwalk_next(&oid, walk))) == GIT_OK)
    {
	git_commit * C;
	const git_signature * S;

	xx = chkgit(git, "git_commit_lookup",
		git_commit_lookup(&C, git->R, &oid));
rpmgitPrintOid("Commit", git_commit_id(C), fp);
	S = git_commit_author(C);
fprintf(fp, "Author: %s <%s>", S->name, S->email);
rpmgitPrintTime("\n  Date", (time_t)S->when.time, fp);
fprintf(fp, "\n%s", git_commit_message(C));
fprintf(fp, "\n");
	git_commit_free(C);
    }

    git->walk = NULL;
    git_revwalk_free(walk);
    walk = NULL;
    rc = 0;	/* XXX */

exit:
#endif
SPEW(0, rc, git);
    return rc;
}

int rpmgitInfo(rpmgit git)
{
    int rc = -1;
#if defined(WITH_LIBGIT2)
    FILE * fp = (git->fp ? git->fp : stdout);
    unsigned ecount;
    unsigned i;
    size_t nt = 128;
    char * t = alloca(nt);
    size_t nb;
    int xx;

    xx = chkgit(git, "git_repository_index",
		git_repository_index((git_index **)&git->I, git->R));
    xx = chkgit(git, "git_index_read",
		git_index_read(git->I));

    ecount = git_index_entrycount(git->I);
    for (i = 0; i < ecount; i++) {
	static const char _fmt[] = "%c";
	git_index_entry * e = git_index_get(git->I, i);
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

    git_index_free(git->I);
    git->I = NULL;
    rc = 0;
#endif
SPEW(0, rc, git);
    return rc;
}

int rpmgitRead(rpmgit git)
{
    int rc = -1;
#if defined(WITH_LIBGIT2) && defined(NOTYET)
    FILE * fp = (git->fp ? git->fp : stdout);
    git_odb * odb = git_repository_database(git->R);
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
    FILE * fp = (git->fp ? git->fp : stdout);
    git_odb * odb = git_repository_database(git->R);
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
static int
rpmgitPopt(rpmgit git, int argc, char *argv[], struct poptOption * opts)
{
    static int _popt_flags = 0;
    int rc;
int xx;

if (_rpmgit_debug) argvPrint("before", argv, NULL);
if (_rpmgit_debug) rpmgitPrintRepo(git, git->R, git->fp);
    git->con = poptFreeContext(git->con);	/* XXX necessary? */
    git->con = poptGetContext(argv[0], argc, (const char **)argv, opts, _popt_flags);
    while ((rc = poptGetNextOpt(git->con)) > 0) {
	const char * arg = poptGetOptArg(git->con);
	arg = _free(arg);
    }
    if (rc < -1) {
        fprintf(stderr, "%s: %s: %s\n", argv[0],
                poptBadOption(git->con, POPT_BADOPTION_NOALIAS),
                poptStrerror(rc));
	git->con = poptFreeContext(git->con);
    }
    git->av = argvFree(git->av);	/* XXX necessary? */
    if (git->con)
	xx = argvAppend(&git->av, (ARGV_t)poptGetArgs(git->con));
    git->ac = argvCount(git->av);
if (_rpmgit_debug) argvPrint(" after", git->av, NULL);
    return rc;
}

/*==============================================================*/

static void rpmgitFini(void * _git)
	/*@globals fileSystem @*/
	/*@modifies *_git, fileSystem @*/
{
    rpmgit git = (rpmgit) _git;

#if defined(WITH_LIBGIT2)
    if (git->walk)
	git_revwalk_free(git->walk);
    if (git->odb)
	git_odb_free(git->odb);
    if (git->cfg)
	git_config_free(git->cfg);
    if (git->H)
	git_commit_free(git->H);
    if (git->C)
	git_commit_free(git->C);
    if (git->T)
	git_tree_free(git->T);
    if (git->I)
	git_index_free(git->I);
    if (git->R)
	git_repository_free(git->R);
#endif
    git->walk = NULL;
    git->odb = NULL;
    git->cfg = NULL;
    git->H = NULL;
    git->C = NULL;
    git->T = NULL;
    git->I = NULL;
    git->R = NULL;

    git->data = NULL;
    git->fp = NULL;

    git->user_email = _free(git->user_email);
    git->user_name = _free(git->user_name);

    git->ac = 0;
    git->av = argvFree(git->av);
    git->con = poptFreeContext(git->con);

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

rpmgit rpmgitNew(char ** argv, uint32_t flags, void * _opts)
{
    rpmgit git = rpmgitGetPool(_rpmgitPool);
int argc = argvCount(argv);
poptOption * opts = (poptOption *) _opts;
const char * fn = _rpmgit_dir;
int xx;

    xx = rpmgitPopt(git, argc, argv, opts);
    git->fn = (fn ? xstrdup(fn) : NULL);
    git->flags = flags;

#if defined(WITH_LIBGIT2)
    git_libgit2_version(&git->major, &git->minor, &git->rev);
    if (git->fn) {
	int xx;
	xx = chkgit(git, "git_repository_open",
		git_repository_open((git_repository **)&git->R, git->fn));
    }
#endif

    return rpmgitLink(git);
}

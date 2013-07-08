/** \ingroup rpmio
 * \file rpmio/rpmgit.c
 */

#include "system.h"

#include <rpmiotypes.h>
#include <rpmio.h>	/* for *Pool methods */
#include <rpmlog.h>
#include <rpmmacro.h>
#include <rpmurl.h>
#include <ugid.h>
#include <poptIO.h>

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
	const git_index_entry * E = git_index_get_byindex(I, i);

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
fprintf(fp,     "       Eattrs: 0%o\n", git_tree_entry_filemode(E));
fprintf(fp,     "        Ename: %s\n", git_tree_entry_name(E));
 rpmgitPrintOid("         Eoid", git_tree_entry_id(E), fp);
fprintf(fp,     "        Etype: %s\n", rpmgitOtype(git_tree_entry_type(E)));
#else
	t = git_oid_allocfmt(git_tree_entry_id(E));
	fprintf(fp, "%06o %.4s %s\t%s\n",
		git_tree_entry_filemode(E),
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
 rpmgitPrintOid("tree", git_commit_tree_id(C), fp);
    Pcnt = git_commit_parentcount(C);
    for (i = 0; i < Pcnt; i++)
	rpmgitPrintOid("parent", git_commit_parent_id(C, i), fp);
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

assert(tag != NULL);
#ifdef	NOTYET
if (_rpmgit_debug >= 0) return;
#endif

#ifdef	DYING
 rpmgitPrintOid("--------  TAG", git_tag_id(tag), fp);
fprintf(fp,     "         name: %s\n", git_tag_name(tag));
fprintf(fp,     "         type: %s\n", git_object_type2string(git_tag_type(tag)));
 rpmgitPrintOid("       target", git_tag_target_id(tag), fp);
 rpmgitPrintSig("       tagger", git_tag_tagger(tag), fp);
fprintf(fp,     "\n%s", git_tag_message(tag));
#else
 rpmgitPrintOid("object", git_tag_target_id(tag), fp);
fprintf(fp,     "type: %s\n", git_object_type2string(git_tag_target_type(tag)));
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

 rpmgitPrintOid("------- Hpeel", git_reference_target_peel(H), fp);
 rpmgitPrintOid("      Htarget", git_reference_target(H), fp);
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
    if (git->R) {	/* XXX leak */
	git_repository_free(git->R);
	git->R = NULL;
    }
    rc = chkgit(git, "git_repository_init",
		git_repository_init((git_repository **)&git->R,
			git->fn, git->is_bare));
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

    /* XXX TODO: strip out workdir prefix if present. */

    /* Upsert the file into the index. */
    rc = chkgit(git, "git_index_add_bypath",
		git_index_add_bypath(git->I, fn));
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
    rc = chkgit(git, "git_index_write_tree",
		git_index_write_tree(&_oidT, git->I));
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

#if defined(WITH_LIBGIT2)
static int rpmgitConfigCB(const git_config_entry * CE, void * _git)
{
    rpmgit git = (rpmgit) _git;
    const char * var_name = CE->name;
    const char * value = CE->value;
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
#endif	/* defined(WITH_LIBGT2) */

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
    (void)entry;
    (void)objt;

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
    goto exit;	/* XXX GCC warning */

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
	const git_index_entry * e = git_index_get_byindex(git->I, i);
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

static int rpmgitToyFile(rpmgit git, const char * fn,
		const char * b, size_t nb)
{
    int rc = RPMRC_FAIL;
#if defined(WITH_LIBGIT2)
    const char * workdir = git_repository_workdir(git->R);
    char * path = rpmGetPath(workdir, "/", fn, NULL);
    char * dn = dirname(xstrdup(path));
    FD_t fd;

    rc = rpmioMkpath(dn, 0755, (uid_t)-1, (gid_t)-1);
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
#endif	/* defined(WITH_LIBGIT2) */
    return rc;
}

/*==============================================================*/

static int rpmgitPopt(rpmgit git, int argc, char *argv[], poptOption opts)
{
    static int _popt_flags = POPT_CONTEXT_POSIXMEHARDER;
    int rc;
int xx;

if (_rpmgit_debug) argvPrint("before", (ARGV_t)argv, NULL);
#if defined(WITH_LIBGIT2)
if (_rpmgit_debug && git->R) rpmgitPrintRepo(git, git->R, git->fp);
#endif	/* defined(WITH_LIBGIT2) */

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

rpmRC rpmgitCmdInit(int argc, char *argv[])
{
    int rc = RPMRC_FAIL;
#if defined(WITH_LIBGIT2)
    const char * init_template = NULL;
    const char * init_shared = NULL;
    enum {
	_INIT_QUIET	= (1 << 0),
	_INIT_BARE	= (1 << 1),
    };
    int init_flags = 0;
#define	INIT_ISSET(_a)	(init_flags & _INIT_##_a)
    struct poptOption initOpts[] = {
     { "bare", '\0', POPT_BIT_SET,		&init_flags, _INIT_BARE,
	N_("Create a bare repository."), NULL },
     { "template", '\0', POPT_ARG_STRING,	&init_template, 0,
	N_("Specify the <template> directory."), N_("<template>") },
	/* XXX POPT_ARGFLAG_OPTIONAL */
     { "shared", '\0', POPT_ARG_STRING,		&init_shared, 0,
	N_("Specify how the git repository is to be shared."),
	N_("{false|true|umask|group|all|world|everybody|0xxx}") },
     { "quiet", 'q', POPT_BIT_SET,		&init_flags, _INIT_QUIET,
	N_("Quiet mode."), NULL },
      POPT_AUTOALIAS
      POPT_AUTOHELP
      POPT_TABLEEND
    };
    rpmgit git = rpmgitNew(argv, 0x80000000, initOpts);
    int xx = -1;
    int i;

if (_rpmgit_debug)
fprintf(stderr, "==> %s(%p[%d]) git %p flags 0x%x\n", __FUNCTION__, argv, argc, git, init_flags);

	/* XXX template? */
	/* XXX parse shared to fmode/dmode. */
	/* XXX quiet? */

    /* Initialize a git repository. */
    git->is_bare = (INIT_ISSET(BARE) ? 1 : 0);
    xx = rpmgitInit(git);
    if (xx)
	goto exit;

	/* XXX elsewhere */
    /* Read/print/save configuration info. */
    xx = rpmgitConfig(git);
    if (xx)
	goto exit;

    /* XXX automagic add and commit (for now) */
    if (git->ac <= 0)
	goto exit;

    /* Create file(s) in _workdir (if any). */
    for (i = 0; i < git->ac; i++) {
	const char * fn = git->av[i];
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
    if (git->ac > 0) {
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

    git = rpmgitFree(git);
#endif	/* defined(WITH_LIBGIT2) */
    return rc;
}
#undef	INIT_ISSET

/*==============================================================*/

rpmRC rpmgitCmdAdd(int argc, char *argv[])
{
    int rc = RPMRC_FAIL;
#if defined(WITH_LIBGIT2)
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
      POPT_AUTOALIAS
      POPT_AUTOHELP
      POPT_TABLEEND
    };
    rpmgit git = rpmgitNew(argv, 0, addOpts);
    int xx = -1;
    int i;

    /* XXX Get the index file for this repository. */
    xx = chkgit(git, "git_repository_index",
		git_repository_index((git_index **)&git->I, git->R));
    if (xx)
	goto exit;

if (_rpmgit_debug < 0) rpmgitPrintIndex(git->I, git->fp);
    /* Create file(s) in _workdir (if any). */
    for (i = 0; i < git->ac; i++) {
	const char * fn = git->av[i];
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

    git = rpmgitFree(git);
#endif	/* defined(WITH_LIBGIT2) */
    return rc;
}
#undef	ADD_ISSET

/*==============================================================*/

rpmRC rpmgitCmdCommit(int argc, char *argv[])
{
    int rc = RPMRC_FAIL;
#if defined(WITH_LIBGIT2)
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
     { "verbose", 'v', POPT_BIT_SET,		&commit_flags, _COMMIT_VERBOSE,
	N_("Verbose mode."), NULL },
     { "quiet", 'q', POPT_BIT_SET,		&commit_flags, _COMMIT_QUIET,
	N_("Quiet mode."), NULL },
     { "dry-run", '\n', POPT_BIT_SET,		&commit_flags, _COMMIT_DRY_RUN,
	N_(""), NULL },

     { "status", '\0', POPT_BIT_SET,		&commit_flags, _COMMIT_STATUS,
	N_(""), NULL },
     { "no-status", '\0', POPT_BIT_SET,		&commit_flags, _COMMIT_NO_STATUS,
	N_(""), NULL },

      POPT_AUTOALIAS
      POPT_AUTOHELP
      POPT_TABLEEND
    };
    rpmgit git = rpmgitNew(argv, 0, commitOpts);
    int xx = -1;

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

    git = rpmgitFree(git);
#endif	/* defined(WITH_LIBGIT2) */
    return rc;
}
#undef	COMMIT_ISSET

/*==============================================================*/

#if defined(WITH_LIBGIT2)
static int resolve_to_tree(git_repository * repo, const char *identifier,
		    git_tree ** tree)
{
    int err = 0;
    git_object *obj = NULL;

    if ((err = git_revparse_single(&obj, repo, identifier)) < 0)
	return err;

    switch (git_object_type(obj)) {
    case GIT_OBJ_TREE:
	*tree = (git_tree *)obj;
	break;
    case GIT_OBJ_COMMIT:
	err = git_commit_tree(tree, (git_commit *)obj);
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

static int printer(
		const git_diff_delta *delta,
		const git_diff_range *range,
		char usage,
		const char *line,
		size_t line_len,
		void * data)
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
#endif	/* defined(WITH_LIBGIT2) */

rpmRC rpmgitCmdDiff(int argc, char *argv[])
{
    int rc = RPMRC_FAIL;
#if defined(WITH_LIBGIT2)
    git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
    git_diff_find_options findopts = GIT_DIFF_FIND_OPTIONS_INIT;
    int color = -1;
    int cached = 0;
    enum {	/* XXX FIXME */
	_DIFF_ALL		= (1 <<  0),
	_DIFF_ZERO		= (1 <<  1),
    };
    enum {
	FORMAT_PATCH	= 0,
	FORMAT_COMPACT	= 1,
	FORMAT_RAW	= 2,
    };
    int format = FORMAT_PATCH;
    int diff_flags = 0;
#define	DIFF_ISSET(_a)	(opts.flags & GIT_DIFF_##_a)
    struct poptOption diffOpts[] = {
	/* XXX -u */
     { "patch", 'p', POPT_ARG_VAL,		&format, FORMAT_PATCH,
	N_("Generate patch."), NULL },
     { "unified", 'U', POPT_ARG_SHORT,		&opts.context_lines, 0,
	N_("Generate diffs with <n> lines of context."), N_("<n>") },
     { "raw", '\0', POPT_ARG_VAL,		&format, FORMAT_RAW,
	N_(""), NULL },
	/* XXX --patch-with-raw */
	/* XXX --patience */
     { "patience", '\0', POPT_BIT_SET,		&opts.flags, GIT_DIFF_PATIENCE,
	N_("Generate a diff using the \"patience diff\" algorithm."), NULL },
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
     { "name-status", '\0', POPT_ARG_VAL,	&format, FORMAT_COMPACT,
	N_("Show only names and status of changed files."), NULL },
	/* XXX --submodule */
     { "color", '\0', POPT_ARG_VAL,		&color, 0,
	N_("Show colored diff."), NULL },
     { "no-color", '\0', POPT_ARG_VAL,		&color, -1,
	N_("Turn off colored diff."), NULL },
	/* XXX --color-words */
	/* XXX --no-renames */
	/* XXX --check */
	/* XXX --full-index */
	/* XXX --binary */
	/* XXX --abbrev */
     { "find-copies-harder", '\0', POPT_BIT_SET, &findopts.flags, GIT_DIFF_FIND_COPIES_FROM_UNMODIFIED,
	N_(""), NULL },
     { "break-rewrites", 'B', POPT_BIT_SET, &findopts.flags, GIT_DIFF_FIND_REWRITES,
	N_(""), NULL },
     { "find-renames", 'M', POPT_ARG_SHORT,	&findopts.rename_threshold, 0,
	N_(""), NULL },
     { "find-copies", 'C', POPT_ARG_SHORT,	&findopts.copy_threshold, 0,
	N_(""), NULL },
	/* XXX --diff-filter */
	/* XXX --find-copies-harder */
	/* XXX -l */
	/* XXX -S */
	/* XXX --pickaxe-all */
	/* XXX --pickaxe-regex */
	/* XXX -O */
     { NULL, 'R', POPT_BIT_SET,			&opts.flags, GIT_DIFF_REVERSE,
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
     { "ignore-submodules", '\0', POPT_BIT_SET,	&opts.flags, GIT_DIFF_IGNORE_SUBMODULES,
	N_("Ignore changes to submodules in the diff generation."), NULL },
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
	/* XXX --untracked-dirs? */

      POPT_AUTOALIAS
      POPT_AUTOHELP
      POPT_TABLEEND
    };
    rpmgit git = NULL;		/* XXX rpmgitNew(argv, 0, diffOpts); */
git_diff_list * diff = NULL;
const char * treeish1 = NULL;
git_tree *t1 = NULL;
const char * treeish2 = NULL;
git_tree *t2 = NULL;
    int xx = -1;

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

    if (findopts.rename_threshold)
	findopts.flags |= GIT_DIFF_FIND_RENAMES;
    if (findopts.copy_threshold)
	findopts.flags |= GIT_DIFF_FIND_COPIES;

    git = rpmgitNew(argv, 0, diffOpts);

    treeish1 = (git->ac >= 1 ? git->av[0] : NULL);
    treeish2 = (git->ac >= 2 ? git->av[1] : NULL);

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
		git_diff_tree_to_tree(&diff, git->R, t1, t2, &opts));
    else if (t1 && cached)
	xx = chkgit(git, "git_diff_index_to_tree",
		git_diff_tree_to_index(&diff, git->R, t1, NULL, &opts));
    else if (t1) {
	git_diff_list *diff2;
	xx = chkgit(git, "git_diff_tree_to_index",
		git_diff_tree_to_index(&diff, git->R, t1, NULL, &opts));
	xx = chkgit(git, "git_diff_workdir_to_index",
		git_diff_index_to_workdir(&diff2, git->R, NULL, &opts));
	xx = chkgit(git, "git_diff_merge",
		git_diff_merge(diff, diff2));
	git_diff_list_free(diff2);
    }
    else if (cached) {
	xx = chkgit(git, "resolve_to_tree",
		resolve_to_tree(git->R, "HEAD", &t1));
	xx = chkgit(git, "git_diff_tree_to_index",
		git_diff_tree_to_index(&diff, git->R, t1, NULL, &opts));
    }
    else
	xx = chkgit(git, "git_diff_index_to_workdir",
		git_diff_index_to_workdir(&diff, git->R, NULL, &opts));

    if ((findopts.flags & GIT_DIFF_FIND_ALL) != 0)
	xx = chkgit(git, "git_diff_find_similar",
		git_diff_find_similar(diff, &findopts));

    if (color >= 0)
	fputs(colors[0], stdout);

    switch (format) {
    case FORMAT_PATCH:
	xx = chkgit(git, "git_diff_print_match",
		git_diff_print_patch(diff, printer, &color));
	break;
    case FORMAT_COMPACT:
	xx = chkgit(git, "git_diff_print_compact",
		git_diff_print_compact(diff, printer, &color));
	break;
    case FORMAT_RAW:
	xx = chkgit(git, "git_diff_print_raw",
		git_diff_print_raw(diff, printer, &color));
	break;
    }

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
#endif	/* defined(WITH_LIBGIT2) */
    return rc;
}
#undef	DIFF_ISSET

/*==============================================================*/

#if defined(WITH_LIBGIT2)
static int status_long_cb(const char *path, unsigned int status, void *data)
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
	if (status & GIT_STATUS_INDEX_MODIFIED)		/* XXX untested */
	    fprintf(fp, "#	?M? file:   %s\n", path);
	if (status & GIT_STATUS_INDEX_DELETED)		/* XXX untested */
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

static int status_short_cb(const char *path, unsigned int status, void *data)
{
    FILE * fp = stdout;
    rpmgit git = (rpmgit) data;
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
#endif	/* defined(WITH_LIBGIT2) */

rpmRC rpmgitCmdStatus(int argc, char *argv[])
{
    int rc = RPMRC_FAIL;
#if defined(WITH_LIBGIT2)
    git_status_options opts = { GIT_STATUS_OPTIONS_VERSION, 0, 0, { NULL, 0} };
    const char * status_untracked_files = xstrdup("all");
    enum {
	_STATUS_SHORT		= (1 <<  0),
	_STATUS_ZERO		= (1 <<  1),
    };
    int status_flags = 0;
#define	STATUS_ISSET(_a)	(status_flags & _STATUS_##_a)
    struct poptOption statusOpts[] = {
     { "short", 's', POPT_BIT_SET,		&status_flags, _STATUS_SHORT,
	N_("Give the output in the short-format."), NULL },
     { "porcelain", '\0', POPT_BIT_SET,		&status_flags, _STATUS_SHORT,
	N_("Give the output in a stable, easy-to-parse format for scripts."), NULL },
     { "untracked-files", 'u', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT,	&status_untracked_files, 0,
	N_("Show untracked files."), N_("{no|normal|all}") },
     { NULL, 'z', POPT_BIT_SET,	&status_flags, _STATUS_ZERO,
	N_(""), NULL },
      POPT_AUTOALIAS
      POPT_AUTOHELP
      POPT_TABLEEND
    };
    rpmgit git = rpmgitNew(argv, 0, statusOpts);
    int xx = -1;

    opts.show   =  STATUS_ISSET(SHORT)
	? GIT_STATUS_SHOW_INDEX_AND_WORKDIR
	: GIT_STATUS_SHOW_INDEX_THEN_WORKDIR;

    opts.flags |=  GIT_STATUS_OPT_INCLUDE_IGNORED;
    opts.flags |=  GIT_STATUS_OPT_INCLUDE_UNMODIFIED;
    opts.flags &= ~GIT_STATUS_OPT_EXCLUDE_SUBMODULES;
    if (!strcmp(status_untracked_files, "no")) {
	opts.flags &= ~GIT_STATUS_OPT_INCLUDE_UNTRACKED;
	opts.flags &= ~GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS;
    } else
    if (!strcmp(status_untracked_files, "normal")) {
	opts.flags |=  GIT_STATUS_OPT_INCLUDE_UNTRACKED;
	opts.flags &= ~GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS;
    } else
    if (!strcmp(status_untracked_files, "all")) {
	opts.flags |=  GIT_STATUS_OPT_INCLUDE_UNTRACKED;
	opts.flags |=  GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS;
    }

    git->state = 0;
    if (STATUS_ISSET(SHORT))
	xx = chkgit(git, "git_status_foreach_ext",
		git_status_foreach_ext(git->R, &opts, status_short_cb, (void *)git));
    else
	xx = chkgit(git, "git_status_foreach_ext",
		git_status_foreach_ext(git->R, &opts, status_long_cb, (void *)git));
    goto exit;	/* XXX GCC warning */

exit:
    rc = (xx ? RPMRC_FAIL : RPMRC_OK);
SPEW(0, rc, git);
    status_untracked_files = _free(status_untracked_files);

    git = rpmgitFree(git);
#endif	/* defined(WITH_LIBGIT2) */
    return rc;
}
#undef	STATUS_ISSET

/*==============================================================*/

#define ARGMINMAX(_min, _max)   (int)(((_min) << 8) | ((_max) & 0xff))

static struct poptOption rpmgitCmds[] = {
/* --- PORCELAIN */
 { "add", '\0',      POPT_ARG_MAINCALL,	rpmgitCmdAdd, ARGMINMAX(1,0),
	N_("Add file contents to the index."), NULL },
#ifdef	NOTYET
 { "bisect", '\0',   POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Find by binary search the change that introduced a bug."), NULL },
 { "branch", '\0',   POPT_ARG_MAINCALL,	cmd_branch, ARGMINMAX(0,0),
	N_("List, create, or delete branches."), NULL },
 { "checkout", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Checkout a branch or paths to the working tree."), NULL },
 { "clone", '\0',    POPT_ARG_MAINCALL,	cmd_clone, ARGMINMAX(0,0),
	N_("Clone a repository into a new directory."), NULL },
#endif	/* NOTYET */
 { "commit", '\0',   POPT_ARG_MAINCALL,	rpmgitCmdCommit, ARGMINMAX(0,0),
	N_("Record changes to the repository."), NULL },
 { "diff", '\0',     POPT_ARG_MAINCALL,	rpmgitCmdDiff, ARGMINMAX(0,0),
	N_("Show changes between commits, commit and working tree, etc."), NULL },
#ifdef	NOTYET
 { "fetch", '\0',    POPT_ARG_MAINCALL,	cmd_fetch, ARGMINMAX(0,0),
	N_("Download objects and refs from another repository."), NULL },
 { "grep", '\0',     POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Print lines matching a pattern."), NULL },
#endif	/* NOTYET */
	/* XXX maxargs=1 */
 { "init", '\0',     POPT_ARG_MAINCALL,	rpmgitCmdInit, ARGMINMAX(1,0),
	N_("Create an empty git repository or reinitialize an existing one."), NULL },
#ifdef	NOTYET
 { "log", '\0',      POPT_ARG_MAINCALL,	cmd_log, ARGMINMAX(0,0),
	N_("Show commit logs."), NULL },
 { "merge", '\0',    POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Join two or more development histories together."), NULL },
 { "mv", '\0',       POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Move or rename a file, a directory, or a symlink."), NULL },
 { "pull", '\0',     POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Fetch from and merge with another repository or a local branch."), NULL },
 { "push", '\0',     POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Update remote refs along with associated objects."), NULL },
 { "rebase", '\0',   POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Forward-port local commits to the updated upstream head."), NULL },
 { "reset", '\0',    POPT_ARG_MAINCALL,	cmd_reset, ARGMINMAX(0,0),
	N_("Reset current HEAD to the specified state."), NULL },
 { "rm", '\0',       POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Remove files from the working tree and from the index."), NULL },
 { "show", '\0',     POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Show various types of objects."), NULL },
#endif	/* NOTYET */
 { "status", '\0',   POPT_ARG_MAINCALL,	rpmgitCmdStatus, ARGMINMAX(0,0),
	N_("Show the working tree status."), NULL },
#ifdef	NOTYET
 { "tag", '\0',      POPT_ARG_MAINCALL,	cmd_tag, ARGMINMAX(0,0),
	N_("Create, list, delete or verify a tag object signed with GPG."), NULL },
#endif	/* NOTYET */

/* --- PLUMBING: manipulation */
#ifdef	NOTYET
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
 { "write-tree", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Create a tree object from the current index."), NULL },
#endif	/* NOTYET */

/* --- PLUMBING: interrogation */
#ifdef	NOTYET
 { "archive", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Create a tar/zip archive of the files in the named tree object."), NULL },
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
	N_("Show information about files in the index and the working tree."), NULL },
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
 { "unpack-file", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Creates a temporary file with a blob’s contents."), NULL },
 { "var", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Show a git logical variable."), NULL },
 { "verify-pack", '\0', POPT_ARG_MAINCALL,	cmd_noop, ARGMINMAX(0,0),
	N_("Validate packed git archive files."), NULL },
#endif	/* NOTYET */

/* --- WIP */
#ifdef	NOTYET
 { "config", '\0', POPT_ARG_MAINCALL,	cmd_config, ARGMINMAX(0,0),
	N_("Show git configuration."), NULL },
 { "index", '\0', POPT_ARG_MAINCALL,	cmd_index, ARGMINMAX(0,0),
	N_("Show git index."), NULL },
 { "refs", '\0', POPT_ARG_MAINCALL,	cmd_refs, ARGMINMAX(0,0),
	N_("Show git references."), NULL },
 { "rev-parse", '\0', POPT_ARG_MAINCALL,cmd_rev_parse, ARGMINMAX(0,0),
	N_("."), NULL },

 { "index-pack-old", '\0', POPT_ARG_MAINCALL,	cmd_index_pack_old, ARGMINMAX(0,0),
	N_("Index a <PACKFILE>."), N_("<PACKFILE>") },
#endif	/* NOTYET */

  POPT_TABLEEND
};

#ifdef	NOTYET
static rpmRC rpmgitCmdHelp(int argc, /*@unused@*/ char *argv[])
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

static rpmRC rpmgitCmdRun(int argc, /*@unused@*/ char *argv[])
{
    struct poptOption * c;
    const char * cmd;
    rpmRC rc = RPMRC_FAIL;

    if (argv == NULL || argv[0] == NULL)	/* XXX segfault avoidance */
	goto exit;
    cmd = argv[0];
    for (c = _rpmgitCommandTable; c->longName != NULL; c++) {
	rpmRC (*func) (int argc, char *argv[]) = NULL;

	if (strcmp(cmd, c->longName))
	    continue;

	func = c->arg;
	rc = (*func) (argc, argv);
	break;
    }

exit:
    return rc;
}
#endif	/* NOTYET */

/*==============================================================*/

/* XXX move to struct rpmgit_s? */
static const char * exec_path;
static const char * html_path;
static const char * man_path;
static const char * info_path;
static int paginate;
static int no_replace_objects;
static int bare;

static struct poptOption rpmgitOpts[] = {
	/* XXX --version */
  { "exec-path", '\0', POPT_ARG_STRING,		&exec_path, 0,
        N_("Set exec path to <DIR>. env(GIT_EXEC_PATH)"), N_("<DIR>") },
  { "html-path", '\0', POPT_ARG_STRING,		&html_path, 0,
        N_("Set html path to <DIR>. env(GIT_HTML_PATH)"), N_("<DIR>") },
  { "man-path", '\0', POPT_ARG_STRING,		&man_path, 0,
        N_("Set man path to <DIR>."), N_("<DIR>") },
  { "info-path", '\0', POPT_ARG_STRING,		&info_path, 0,
        N_("Set info path to <DIR>."), N_("<DIR>") },
	/* XXX --pager */
 { "paginate", 'p', POPT_ARG_VAL,		&paginate, 1,
	N_("Set paginate. env(PAGER)"), NULL },
 { "no-paginate", '\0', POPT_ARG_VAL,		&paginate, 0,
	N_("Set paginate. env(PAGER)"), NULL },
 { "no-replace-objects", '\0', POPT_ARG_VAL,	&no_replace_objects, 1,
	N_("Do not use replacement refs for objects."), NULL },
 { "bare", '\0', POPT_ARG_VAL,	&bare, 1,
	N_("Treat as a bare repository."), NULL },

  { "git-dir", '\0', POPT_ARG_STRING,	&_rpmgit_dir, 0,
        N_("Set git repository dir to <DIR>. env(GIT_DIR)"), N_("<DIR>") },
  { "work-tree", '\0', POPT_ARG_STRING,	&_rpmgit_tree, 0,
        N_("Set git work tree to <DIR>. env(GIT_WORK_TREE)"), N_("<DIR>") },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmgitCmds, 0,
	N_("The most commonly used git commands are::"),
	NULL },

#ifdef	NOTYET
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmioAllPoptTable, 0,
	N_("Common options for all rpmio executables:"),
	NULL },

  POPT_AUTOALIAS
#endif
  POPT_AUTOHELP

  { NULL, (char) -1, POPT_ARG_INCLUDE_TABLE, NULL, 0,
        N_("\
usage: git [--version] [--exec-path[=GIT_EXEC_PATH]] [--html-path]\n\
           [-p|--paginate|--no-pager] [--no-replace-objects]\n\
           [--bare] [--git-dir=GIT_DIR] [--work-tree=GIT_WORK_TREE]\n\
           [--help] COMMAND [ARGS]\n\
\n\
The most commonly used git commands are:\n\
   add        Add file contents to the index\n\
   bisect     Find by binary search the change that introduced a bug\n\
   branch     List, create, or delete branches\n\
   checkout   Checkout a branch or paths to the working tree\n\
   clone      Clone a repository into a new directory\n\
   commit     Record changes to the repository\n\
   diff       Show changes between commits, commit and working tree, etc\n\
   fetch      Download objects and refs from another repository\n\
   grep       Print lines matching a pattern\n\
   init       Create an empty git repository or reinitialize an existing one\n\
   log        Show commit logs\n\
   merge      Join two or more development histories together\n\
   mv         Move or rename a file, a directory, or a symlink\n\
   pull       Fetch from and merge with another repository or a local branch\n\
   push       Update remote refs along with associated objects\n\
   rebase     Forward-port local commits to the updated upstream head\n\
   reset      Reset current HEAD to the specified state\n\
   rm         Remove files from the working tree and from the index\n\
   show       Show various types of objects\n\
   status     Show the working tree status\n\
   tag        Create, list, delete or verify a tag object signed with GPG\n\
\n\
See 'git COMMAND --help' for more information on a specific command.\n\
"), NULL},

  POPT_TABLEEND
};

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
rpmioPool _rpmgitPool;

/*@unchecked@*/ /*@relnull@*/
rpmgit _rpmgitI;

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

#ifdef	NOTYET	/* XXX rpmgitRun() uses pre-parsed git->av */
/*@unchecked@*/
static const char * _gitI_init = "\
";
#endif	/* NOTYET */

static rpmgit rpmgitI(void)
        /*@globals _rpmgitI @*/
        /*@modifies _rpmgitI @*/
{
    if (_rpmgitI == NULL)
        _rpmgitI = rpmgitNew(NULL, 0, NULL);
    return _rpmgitI;
}

rpmgit rpmgitNew(char ** av, uint32_t flags, void * _opts)
{
    static char * _av[] = { (char *) "rpmgit", NULL };
    int initialize = (!(flags & 0x80000000) || _rpmgitI == NULL);
    rpmgit git = (flags & 0x80000000)
        ? rpmgitI() : rpmgitGetPool(_rpmgitPool);
    int ac;
poptOption opts = (poptOption) _opts;
const char * fn = _rpmgit_dir;
int xx;

if (_rpmgit_debug)
fprintf(stderr, "==> %s(%p, 0x%x) git %p\n", __FUNCTION__, av, flags, git);

    if (av == NULL) av = _av;
    if (opts == NULL) opts = rpmgitOpts;

    ac = argvCount((ARGV_t)av);
    if (ac > 1)
	xx = rpmgitPopt(git, ac, av, opts);

    git->fn = (fn ? xstrdup(fn) : NULL);
    git->flags = flags;

#if defined(WITH_LIBGIT2)
    if (initialize) {
	git_libgit2_version(&git->major, &git->minor, &git->rev);
	if (git->fn && git->R == NULL) {
	    int xx;
	    xx = chkgit(git, "git_repository_open",
		git_repository_open((git_repository **)&git->R, git->fn));
	}
    }
#ifdef	NOTYET	/* XXX rpmgitRun() uses pre-parsed git->av */
    if (initialize) {
        static const char _rpmgitI_init[] = "%{?_rpmgitI_init}";
        const char * s = rpmExpand(_rpmgitI_init, _gitI_init, NULL);
	ARGV_t sav = NULL;
	const char * arg;
	int i;

	xx = argvSplit(&sav, s, "\n");
	for (i = 0; (arg = sav[i]) != NULL; i++) {
	    if (*arg == '\0')
		continue;
	    (void) rpmgitRun(git, arg, NULL);
	}
	sav = argvFree(sav);
        s = _free(s);
    }
#endif	/* NOTYET */
#endif

    return rpmgitLink(git);
}

/* XXX str argument isn't used. */
rpmRC rpmgitRun(rpmgit git, const char * str, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;
    const char * cmd;
    struct poptOption * c;
    rpmRC (*handler) (int argc, char *argv[]);
    int minargs;
    int maxargs;

if (_rpmgit_debug)
fprintf(stderr, "==> %s(%p,%s,%p)\n", __FUNCTION__, git, str, resultp);

    if (git == NULL) git = rpmgitI();


    if (git->av == NULL || git->av[0] == NULL)	/* XXX segfault avoidance */
	goto exit;
    cmd = git->av[0];
    for (c = rpmgitCmds; c->longName != NULL; c++) {
	if (strcmp(cmd, c->longName))
	    continue;
	handler = c->arg;
	minargs = (c->val >> 8) & 0xff;
	maxargs = (c->val     ) & 0xff;
	break;
    }
    if (c->longName == NULL) {
	fprintf(stderr, "Unknown command '%s'\n", cmd);
	rc = RPMRC_FAIL;
    } else
    if (minargs && git->ac < minargs) {
	fprintf(stderr, "Not enough arguments for \"git %s\"\n", c->longName);
	rc = RPMRC_FAIL;
    } else
    if (maxargs && git->ac > maxargs) {
	fprintf(stderr, "Too many arguments for \"git %s\"\n", c->longName);
	rc = RPMRC_FAIL;
    } else {
	ARGV_t av = git->av;
	int ac = git->ac;

	git->av = NULL;
	git->ac = 0;
        rc = (*handler)(git->ac, (char **)git->av);
	git->av = av;
	git->ac = ac;
    }

exit:
    return rc;
}

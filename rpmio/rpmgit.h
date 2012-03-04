#ifndef	H_RPMGIT
#define	H_RPMGIT

/** \ingroup rpmio
 * \file rpmio/rpmgit.h
 */

/** \ingroup rpmio
 */
/*@unchecked@*/
extern int _rpmgit_debug;

/** \ingroup rpmio
 */
typedef /*@refcounted@*/ struct rpmgit_s * rpmgit;

#if defined(_RPMGIT_INTERNAL)

/** \ingroup rpmio
 */
struct rpmgit_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */
    const char * fn;
    int major;
    int minor;
    int rev;

    git_repository * repo;
    git_index * index;
    git_config * cfg;

    git_revwalk * walk;

    git_commit * commit;
    git_signature * author;
    git_signature * cmtter;

#ifdef	NOTYET
    const char * message;
    time_t ctime;

    git_oid oid;
    git_odb * odb;
    git_odb_object * obj;
    git_otype otype;
    unsigned int parents;
    unsigned int p;

    git_oid tree_id;
    git_oid parent_id;
    git_oid commit_id;
    git_tree * tree;
    git_commit * parent;

    git_tag * tag;
    const char * tmessage;
    const char * tname;
    git_otype ttype;

    git_tree_entry * entry;
    git_object * objt;

    git_blob * blob;
    git_commit * wcommit;

    git_signature * cauth;
    const char * cmsg;

    unsigned int i;
    unsigned int ecount;
    git_index_entry * e;

    git_strarray ref_list;
    const char * refname;
    git_reference * ref;

    const char * email;
    int32_t j;
#endif

#if defined(__LCLINT__)
/*@refs@*/
    int nrefs;			/*!< (unused) keep splint happy */
#endif
};
#endif	/* _RPMGIT_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a git wrapper instance.
 * @param git		git wrapper
 * @return		NULL on last dereference
 */
/*@unused@*/ /*@null@*/
rpmgit rpmgitUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmgit git)
	/*@modifies git @*/;
#define	rpmgitUnlink(_git)	\
    ((rpmgit)rpmioUnlinkPoolItem((rpmioItem)(_git), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a git wrapper instance.
 * @param git		git wrapper
 * @return		new git wrapper reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmgit rpmgitLink (/*@null@*/ rpmgit git)
	/*@modifies git @*/;
#define	rpmgitLink(_git)	\
    ((rpmgit)rpmioLinkPoolItem((rpmioItem)(_git), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a git wrapper.
 * @param git		git wrapper
 * @return		NULL on last dereference
 */
/*@null@*/
rpmgit rpmgitFree(/*@killref@*/ /*@null@*/rpmgit git)
	/*@globals fileSystem @*/
	/*@modifies git, fileSystem @*/;
#define	rpmgitFree(_git)	\
    ((rpmgit)rpmioFreePoolItem((rpmioItem)(_git), __FUNCTION__, __FILE__, __LINE__))

/**
 * Create and load a git wrapper.
 * @param fn		git file
 * @param flags		git flags
 * @return		new git wrapper
 */
/*@newref@*/ /*@null@*/
rpmgit rpmgitNew(const char * fn, int flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

int rpmgitConfig(rpmgit git);
int rpmgitInfo(rpmgit git);
int rpmgitTree(rpmgit git);
int rpmgitWalk(rpmgit git);
int rpmgitRead(rpmgit git);
int rpmgitWrite(rpmgit git);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMGIT */

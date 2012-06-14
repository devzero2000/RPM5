#ifndef	H_RPMGIT
#define	H_RPMGIT

/** \ingroup rpmio
 * \file rpmio/rpmgit.h
 */

/** \ingroup rpmio
 */
/*@unchecked@*/
extern int _rpmgit_debug;
/*@unchecked@*/
extern const char * _rpmgit_dir;	/* XXX GIT_DIR */
/*@unchecked@*/
extern const char * _rpmgit_tree;	/* XXX GIT_WORK_TREE */

/** \ingroup rpmio
 */
typedef /*@refcounted@*/ struct rpmgit_s * rpmgit;

#if defined(_RPMGIT_INTERNAL)

#include <popt.h>
#include <argv.h>

#if defined(HAVE_GIT2_H)
#include <git2.h>
#include <git2/branch.h>
#include <git2/errors.h>
#endif

/** \ingroup rpmio
 */
struct rpmgit_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */

    const char * fn;
    uint32_t flags;
    
    poptContext con;
    ARGV_t av;
    int ac;

    int core_bare;
    int core_repositoryformatversion;
    const char * user_name;
    const char * user_email;

    int major;
    int minor;
    int rev;

    int state;			/*!< git_status_foreach() state */

    void * R;			/*!< git_repository * */
    void * I;			/*!< git_index * */
    void * T;			/*!< git_tree * */
    void * C;			/*!< git_commit * */
    void * H;			/*!< git_reference * */

    void * cfg;			/*!< git_config * */
    void * odb;			/*!< git_odb * */
    void * walk;		/*!< git_revwalk * */

    void * data;
    FILE * fp;

#ifdef	NOTYET
    const void * Cauthor;
    const void * Ccmtter;
    const char * msg;
    const char * msgenc;
    time_t tstamp;
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
 * @param argv		git args
 * @param flags		git flags
 * @param _opts		poptOption table
 * @return		new git wrapper
 */
/*@newref@*/ /*@null@*/
rpmgit rpmgitNew(/*@null@*/ char ** argv, uint32_t flags, void * _opts)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

void rpmgitPrintOid(const char * msg, const void * _oidp, void * _fp)
	/*@*/;

void rpmgitPrintTime(const char * msg, time_t _Ctime, void * _fp)
	/*@*/;

void rpmgitPrintSig(const char * msg, const void * _S, void * _fp)
	/*@*/;

void rpmgitPrintIndex(void * _I, void * _fp)
	/*@*/;

void rpmgitPrintTree(void * _T, void * _fp)
	/*@*/;

void rpmgitPrintCommit(rpmgit git, void * _C, void * _fp)
	/*@*/;

void rpmgitPrintTag(rpmgit git, void * _tag, void * _fp)
	/*@*/;

void rpmgitPrintHead(rpmgit git, void * _H, void * _fp)
	/*@*/;

void rpmgitPrintRepo(rpmgit git, void * _R, void * _fp)
	/*@*/;

int rpmgitInit(rpmgit git)
	/*@*/;

int rpmgitAddFile(rpmgit git, const char * fn)
	/*@*/;

int rpmgitCommit(rpmgit git, const char * msg)
	/*@*/;

int rpmgitConfig(rpmgit git)
	/*@*/;

int rpmgitInfo(rpmgit git);
int rpmgitTree(rpmgit git);
int rpmgitWalk(rpmgit git);
int rpmgitRead(rpmgit git);
int rpmgitWrite(rpmgit git);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMGIT */

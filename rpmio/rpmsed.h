#ifndef	H_RPMSED
#define	H_RPMSED

/** \ingroup rpmpgp
 * \file rpmio/rpmsed.h
 */

#include <rpmiotypes.h>

extern int _rpmsed_debug;

typedef /*refcounted@*/ struct rpmsed_s * rpmsed;

/*
 * Cp copies source files to target files.
 *
 * The global PATH_T structure "to" always contains the path to the
 * current target file.  Since Fts(3) does not change directories,
 * this path can be either absolute or dot-relative.
 *
 * The basic algorithm is to initialize rpmsed target and use Fts(3) to traverse
 * the file hierarchy rooted in the argument list.  A trivial case is the
 * case of 'cp file1 file2'.  The more interesting case is the case of
 * 'cp file1 file2 ... fileN dir' where the hierarchy is traversed and the
 * path (relative to the root of the traversal) is appended to dir (stored
 * in rpmsed) to form the final target path.
 */

#if defined(_RPMSED_INTERNAL)
#include <argv.h>
#include <pcrs.h>

/**
 * Bit field enum for copy CLI options.
 */
#define _KFB(n) (1U << (n))
#define _MFB(n) (_KFB(n) | 0x40000000)
enum rpmsedFlags_e {
    SED_FLAGS_NONE		= 0,
    SED_FLAGS_FOLLOW		= _MFB( 0), /*!< --follow-symlinks ... */
    SED_FLAGS_COPY		= _MFB( 1), /*!< -c,--copy ... */
    SED_FLAGS_POSIX		= _MFB( 2), /*!< --posix ... */
    SED_FLAGS_EXTENDED		= _MFB( 3), /*!< -r,--regexp-extended ... */
    SED_FLAGS_SEPARATE		= _MFB( 4), /*!< -s,--seperate ... */
    SED_FLAGS_UNBUFFERED	= _MFB( 5), /*!< -u,--unbuffered ... */
	/* 6-31 unused */
};

struct rpmsed_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */

    enum rpmsedFlags_e flags;

    const char * fn;
    pcrs_job * job;
    FILE * ifp;
    FILE * ofp;

    ARGV_t av;
    int ac;
    const char * suffix;
    int line_length;
    const char ** cmdfiles;
    int ncmdfiles;
    const char ** subcmds;
    int nsubcmds;

    pcrs_job ** jobs;
    int njobs;
    int err;

    FD_t fd;
    int linenum;
    char * buf;
    size_t nbuf;
    char * b;
    size_t nb;
    char * result;
    size_t length;

#if defined(__LCLINT__)
/*@refs@*/
    int nrefs;			/*!< (unused) keep splint happy */
#endif
};
#endif	/* defined(_RPMSED_INTERNAL) */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a sed wrapper instance.
 * @param ct		sed wrapper
 * @return		NULL on last dereference
 */
/*@unused@*/ /*@null@*/
rpmsed rpmsedUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmsed ct)
	/*@modifies ct @*/;
#define	rpmsedUnlink(_ct)	\
    ((rpmsed)rpmioUnlinkPoolItem((rpmioItem)(_ct), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a sed wrapper instance.
 * @param ct		sed wrapper
 * @return		new sed wrapper reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmsed rpmsedLink (/*@null@*/ rpmsed ct)
	/*@modifies ct @*/;
#define	rpmsedLink(_ct)	\
    ((rpmsed)rpmioLinkPoolItem((rpmioItem)(_ct), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a sed wrapper.
 * @param ct		sed wrapper
 * @return		NULL on last dereference
 */
/*@null@*/
rpmsed rpmsedFree(/*@killref@*/ /*@null@*/rpmsed ct)
	/*@globals fileSystem @*/
	/*@modifies ct, fileSystem @*/;
#define	rpmsedFree(_ct)	\
    ((rpmsed)rpmioFreePoolItem((rpmioItem)(_ct), __FUNCTION__, __FILE__, __LINE__))

/**
 * Create a sed wrapper.
 * @param argv		sed arguments
 * @param flags		sed flags
 * @return		new sed wrapper (NULL on error)
 */
rpmsed rpmsedNew(char ** argv, unsigned flags)
	/*@*/;

rpmRC rpmsedOpen(rpmsed sed, const char *fn)
	/*@*/;

rpmRC rpmsedProcess(rpmsed sed)
	/*@*/;

rpmRC rpmsedClose(rpmsed sed)
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMSED */

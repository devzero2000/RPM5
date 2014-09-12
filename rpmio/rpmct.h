#ifndef	H_RPMCT
#define	H_RPMCT

/** \ingroup rpmpgp
 * \file rpmio/rpmct.h
 */

#include <rpmiotypes.h>

extern int _rpmct_debug;

typedef /*refcounted@*/ struct rpmct_s * rpmct;

/*
 * Cp copies source files to target files.
 *
 * The global PATH_T structure "to" always contains the path to the
 * current target file.  Since Fts(3) does not change directories,
 * this path can be either absolute or dot-relative.
 *
 * The basic algorithm is to initialize rpmct target and use Fts(3) to traverse
 * the file hierarchy rooted in the argument list.  A trivial case is the
 * case of 'cp file1 file2'.  The more interesting case is the case of
 * 'cp file1 file2 ... fileN dir' where the hierarchy is traversed and the
 * path (relative to the root of the traversal) is appended to dir (stored
 * in rpmct) to form the final target path.
 */

#if defined(_RPMCT_INTERNAL)
#include <argv.h>
#include <fts.h>

/**
 * Bit field enum for copy CLI options.
 */
#define _KFB(n) (1U << (n))
#define _MFB(n) (_KFB(n) | 0x40000000)
enum rpmctFlags_e {
    COPY_FLAGS_NONE		= 0,
    COPY_FLAGS_FOLLOWARGS	= _MFB( 0), /*!< -H ... */
    COPY_FLAGS_FOLLOW		= _MFB( 1), /*!< -L ... */
    COPY_FLAGS_RECURSE		= _MFB( 2), /*!< -R ... */
    COPY_FLAGS_ARCHIVE		= _MFB( 3), /*!< -a,--archive ... */
    COPY_FLAGS_FORCE		= _MFB( 4), /*!< -f,--force ... */
    COPY_FLAGS_INTERACTIVE	= _MFB( 5), /*!< -i,--interactive ... */
    COPY_FLAGS_HARDLINK		= _MFB( 6), /*!< -l,--link ... */
    COPY_FLAGS_NOCLOBBER	= _MFB( 7), /*!< -n,--noclobber ... */
    COPY_FLAGS_PRESERVE		= _MFB( 8), /*!< -p,--preserve ... */
    COPY_FLAGS_XDEV		= _MFB( 9), /*!< -x,--xdev ... */

    COPY_FLAGS_SYMLINK		= _MFB(10), /*!< -s,--symlink ... */
    COPY_FLAGS_UPDATE		= _MFB(11), /*!< -u,--update ... */
    COPY_FLAGS_SPARSE		= _MFB(12), /*!<    --sparse ... */

	/* 13-31 unused */
};

enum rpmctType_e { FILE_TO_FILE, FILE_TO_DIR, DIR_TO_DNE };

struct rpmct_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */

    enum rpmctFlags_e flags;
    enum rpmctType_e type;
    ARGV_t av;
    int ac;
    int ftsoptions;
    FTS * t;
    FTSENT * p;
    struct stat sb;
    char * b;
    size_t blen;
    size_t ballocated;
    struct timeval tv[2];
    char * p_end;		/* pointer to NULL at end of path */
    char * target_end;		/* pointer to end of target base */
    char npath[PATH_MAX];	/* pointer to the start of a path */

#if defined(__LCLINT__)
/*@refs@*/
    int nrefs;			/*!< (unused) keep splint happy */
#endif
};
#endif	/* defined(_RPMCT_INTERNAL) */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a copytree wrapper instance.
 * @param ct		copytree wrapper
 * @return		NULL on last dereference
 */
/*@unused@*/ /*@null@*/
rpmct rpmctUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmct ct)
	/*@modifies ct @*/;
#define	rpmctUnlink(_ct)	\
    ((rpmct)rpmioUnlinkPoolItem((rpmioItem)(_ct), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a copytree wrapper instance.
 * @param ct		copytree wrapper
 * @return		new copytree wrapper reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmct rpmctLink (/*@null@*/ rpmct ct)
	/*@modifies ct @*/;
#define	rpmctLink(_ct)	\
    ((rpmct)rpmioLinkPoolItem((rpmioItem)(_ct), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a copytree wrapper.
 * @param ct		copytree wrapper
 * @return		NULL on last dereference
 */
/*@null@*/
rpmct rpmctFree(/*@killref@*/ /*@null@*/rpmct ct)
	/*@globals fileSystem @*/
	/*@modifies ct, fileSystem @*/;
#define	rpmctFree(_ct)	\
    ((rpmct)rpmioFreePoolItem((rpmioItem)(_ct), __FUNCTION__, __FILE__, __LINE__))

/**
 * Create a copytree wrapper.
 * @param argv		copytree arguments
 * @param flags		copytree flags
 * @return		new copytree wrapper (NULL on error)
 */
rpmct rpmctNew(char ** argv, unsigned flags)
	/*@*/;

/**
 * Copy a tree.
 * @param ct		copytree wrapper
 * @return		RPMRC_OK on success
 */
rpmRC rpmctCopy(/*@null@*/ rpmct ct)
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMCT */

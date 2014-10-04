#ifndef	H_RPMGFS
#define	H_RPMGFS

/** \ingroup rpmio
 * \file rpmio/rpmgfs.h
 */

/** \ingroup rpmio
 */
typedef /*@refcounted@*/ struct rpmgfs_s * rpmgfs;

/** \ingroup rpmgo
 */
/*@unchecked@*/
extern int _rpmgfs_debug;

/*@unchecked@*/ /*@relnull@*/
extern rpmgfs _rpmgfsI;

/*==============================================================*/

#if defined(_RPMGFS_INTERNAL)

#include <bson.h>
#include <mongoc.h>

/** \ingroup rpmio
 */
struct rpmgfs_s {
    struct rpmioItem_s _item;	/*!< usage mutex and pool identifier. */

    const char * fn;
    int flags;
    mode_t mode;

    const char * scheme;
    const char * u;
    const char * pw;
    const char * user;
    const char * h;
    const char * p;
    const char * host;
    const char * db;
    const char * coll;
    const char * opts;
    const char * uri;

    mongoc_client_t *C;
    mongoc_stream_t *S;

    mongoc_gridfs_t *G;
    mongoc_gridfs_file_t *F;
    mongoc_gridfs_file_list_t *D;

    mongoc_iovec_t iov;

#if defined(__LCLINT__)
/*@refs@*/
    int nrefs;			/*!< (unused) keep splint happy */
#endif
};
#endif	/* _RPMGFS_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a mongo wrapper instance.
 * @param mgo		mongo wrapper
 * @return		NULL on last dereference
 */
/*@unused@*/ /*@null@*/
rpmgfs rpmgfsUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmgfs mgo)
	/*@modifies mgo @*/;
#define	rpmgfsUnlink(_mgo)	\
    ((rpmgfs)rpmioUnlinkPoolItem((rpmioItem)(_mgo), __FUNCTION__, __FILE__, __LINE__))

/**
 * Reference a mongo wrapper instance.
 * @param mgo		mongo wrapper
 * @return		new mongo wrapper reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmgfs rpmgfsLink (/*@null@*/ rpmgfs mgo)
	/*@modifies mgo @*/;
#define	rpmgfsLink(_mgo)	\
    ((rpmgfs)rpmioLinkPoolItem((rpmioItem)(_mgo), __FUNCTION__, __FILE__, __LINE__))

/**
 * Destroy a mongo wrapper.
 * @param mgo		mongo wrapper
 * @return		NULL on last dereference
 */
/*@null@*/
rpmgfs rpmgfsFree(/*@killref@*/ /*@null@*/rpmgfs mgo)
	/*@globals fileSystem @*/
	/*@modifies mgo, fileSystem @*/;
#define	rpmgfsFree(_mgo)	\
    ((rpmgfs)rpmioFreePoolItem((rpmioItem)(_mgo), __FUNCTION__, __FILE__, __LINE__))

/**
 * Create and load a mongo wrapper.
 * @param fn		mongo file
 * @param flags		mongo flags
 * @return		new mongo wrapper
 */
/*@newref@*/ /*@null@*/
rpmgfs rpmgfsNew(const char * fn, int flags)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

/**
 * Copy a file from GridFS.
 * @param gfs		mongo wrapper
 * @param dfn		local file name (NULL prints to stdout)
 * @param sfn		GridFS file name
 * @return		0 on success
 */
int rpmgfsGet(rpmgfs gfs, const char * dfn, const char * sfn);

/**
 * Copy a file into GridFS.
 * @param gfs		mongo wrapper
 * @param dfn		GridFS file name
 * @param sfn		local file name
 * @return		0 on success
 */
int rpmgfsPut(rpmgfs gfs, const char * dfn, const char * sfn);

/**
 * Delete a file in GridFS.
 * @param gfs		mongo wrapper
 * @param sfn		file name
 * @return		0 on success
 */
int rpmgfsDel(rpmgfs gfs, const char * fn);

/**
 * List files in GridFS.
 * @param gfs		mongo wrapper
 * @return		0 on success
 */
int rpmgfsList(rpmgfs gfs);

/**
 * Dump gfs database.
 * @param gfs		mongo wrapper
 * @return		0 on success
 */
int rpmgfsDump(rpmgfs gfs);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMGFS */

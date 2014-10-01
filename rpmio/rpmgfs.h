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

ssize_t rpmgfsRead(rpmgfs gfs, void * buf, size_t count);

ssize_t rpmgfsWrite(rpmgfs gfs, const void * buf, size_t count);

int rpmgfsClose(rpmgfs gfs);

int rpmgfsOpen(rpmgfs gfs, const char * fn, int flags, mode_t mode);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMGFS */

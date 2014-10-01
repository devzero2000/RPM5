#include "system.h"

#include <rpmiotypes.h>
#include <rpmio.h>
#include <rpmlog.h>
#include <rpmurl.h>

#define	_RPMGFS_INTERNAL
#include <rpmgfs.h>

#include "debug.h"

int _rpmgfs_debug;

rpmgfs _rpmgfsI;

static int gfs_opens;

/*==============================================================*/
ssize_t rpmgfsRead(rpmgfs gfs, void * buf, size_t count)
{
    bson_error_t err;
    ssize_t rc = 0;

    gfs->iov.iov_base = buf;
    gfs->iov.iov_len = count;

    if (gfs->S == NULL) {
	gfs->F = mongoc_gridfs_find_one_by_filename(gfs->G, gfs->fn, &err);
assert(gfs->F);

	gfs->S = mongoc_stream_gridfs_new(gfs->F);
assert(gfs->S);
    }

    for (;;) {
	size_t _iovcnt = 1;
	size_t _min_bytes = -1;
	int32_t _timeout_msec = 0;

	rc = mongoc_stream_readv(gfs->S, &gfs->iov, _iovcnt,
			_min_bytes, _timeout_msec);

	if (rc <= 0) {
	    goto exit;
	}

	if (fwrite(gfs->iov.iov_base, rc, 1, stdout) != (size_t)rc) {
	    rc = -1;
	    goto exit;
	}

	break;
    }

exit:
    if (rc <= 0) {
	if (gfs->S)
	    mongoc_stream_destroy(gfs->S);
	gfs->S = NULL;
    }
    return rc;
}

ssize_t rpmgfsWrite(rpmgfs gfs, const void * buf, size_t count)
{
    bson_error_t err;
    ssize_t rc = 0;

    gfs->iov.iov_base = (void *) buf;
    gfs->iov.iov_len = count;

    if (gfs->S == NULL) {
	gfs->F = mongoc_gridfs_find_one_by_filename(gfs->G, gfs->fn, &err);
assert(gfs->F);

	gfs->S = mongoc_stream_gridfs_new(gfs->F);
assert(gfs->S);
    }

    for (;;) {
	size_t _iovcnt = 1;
	int32_t _timeout_msec = 0;

	rc = mongoc_stream_writev(gfs->S, &gfs->iov, _iovcnt, _timeout_msec);
assert(rc >= 0);

	break;
    }

    if (rc <= 0) {
	if (gfs->S)
	    mongoc_stream_destroy(gfs->S);
	gfs->S = NULL;
    }
    return rc;
}

int rpmgfsClose(rpmgfs gfs)
{
    int rc = 0;

    gfs->fn = _free(gfs->fn);
    gfs->flags = 0;
    gfs->mode = 0;

    return rc;
}

int rpmgfsOpen(rpmgfs gfs, const char * fn, int flags, mode_t mode)
{
    int rc = 0;

    gfs->fn = _free(gfs->fn);
    gfs->fn = xstrdup(fn);
    gfs->flags = flags;
    gfs->mode = mode;

    return rc;
}

/*==============================================================*/

static void rpmgfsFini(void * _gfs)
	/*@globals fileSystem @*/
	/*@modifies *_gfs, fileSystem @*/
{
    rpmgfs gfs = (rpmgfs) _gfs;

    if (gfs->S)
	mongoc_stream_destroy(gfs->S);
    gfs->S = NULL;

    if (gfs->G)
	mongoc_gridfs_destroy(gfs->G);
    gfs->G = NULL;

    if (gfs->C)
	mongoc_client_destroy(gfs->C);
    gfs->C = NULL;

    if (--gfs_opens == 0)
	mongoc_cleanup();

    gfs->fn = _free(gfs->fn);
    gfs->flags = 0;
    gfs->mode = 0;
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmgfsPool = NULL;

static rpmgfs rpmgfsGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmgfsPool, fileSystem @*/
	/*@modifies pool, _rpmgfsPool, fileSystem @*/
{
    rpmgfs gfs;

    if (_rpmgfsPool == NULL) {
	_rpmgfsPool = rpmioNewPool("gfs", sizeof(*gfs), -1, _rpmgfs_debug,
			NULL, NULL, rpmgfsFini);
	pool = _rpmgfsPool;
    }
    gfs = (rpmgfs) rpmioGetPool(pool, sizeof(*gfs));
    memset(((char *)gfs)+sizeof(gfs->_item), 0, sizeof(*gfs)-sizeof(gfs->_item));
    return gfs;
}

rpmgfs rpmgfsNew(const char * fn, int flags)
{
    static const char * _Curi =		"mongodb://127.0.0.1:27017";
    static const char * _Gdb =		"test";
    static const char * _Gprefix =	"fs";
    rpmgfs gfs = rpmgfsGetPool(_rpmgfsPool);
    bson_error_t err;

    if (fn)
	gfs->fn = xstrdup(fn);

    if (gfs_opens++ == 0)
	mongoc_init();

    gfs->C = mongoc_client_new(_Curi);
assert(gfs->C);

    gfs->G = mongoc_client_get_gridfs(gfs->C, _Gdb, _Gprefix, &err);
assert(gfs->G);

    return rpmgfsLink(gfs);
}

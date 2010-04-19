#include "system.h"

#define _RPMSQL_INTERNAL
#define _RPMVT_INTERNAL
#define _RPMVC_INTERNAL
#include <rpmsql.h>

#include <sqlite3.h>	/* XXX sqlite3_module */

#include "debug.h"

/*==============================================================*/

static struct rpmvd_s _nixdbVD = {
	/* XXX glob needs adjustment for base32 and possibly length. */
	.prefix = "%{?_nixdb}%{!?_nixdb:/nix/store}/[a-z0-9]*-",
	.split	= "/-",
	.parse	= "dir/instance-name",
	.regex	= "^(.+/)([^-]+)-(.*)$",
	.idx	= 2,
	.nrows	= -1
};

static int nixdbCreateConnect(void * _db, void * pAux,
		int argc, const char *const * argv,
		rpmvt * vtp, char ** pzErr)
{
    return rpmvtLoadArgv(rpmvtNew(_db, pAux, argv, &_nixdbVD), vtp);
}

struct sqlite3_module nixdbModule = {
    .xCreate	= (void *) nixdbCreateConnect,
    .xConnect	= (void *) nixdbCreateConnect,
    .xColumn	= (void *) _npydbColumn,
};

/*==============================================================*/

static struct rpmsqlVMT_s __VMT[] = {
  { "Nixdb",		&nixdbModule, NULL },
  { NULL,               NULL, NULL }
};

extern int sqlite3_extension_init(void * _db);
int sqlite3_extension_init(void * _db)
{
    int rc = 0;		/* SQLITE_OK */
SQLDBG((stderr, "--> %s(%p)\n", __FUNCTION__, _db));
    rc = _rpmsqlLoadVMT(_db, __VMT);
fprintf(stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, _db, rc);
    return rc;
}

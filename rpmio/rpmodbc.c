/** \ingroup rpmio
 * \file rpmio/rpmodbc.c
 */

#include "system.h"

#include <rpmiotypes.h>
#include <rpmio.h>	/* for *Pool methods */
#include <rpmlog.h>
#include <rpmurl.h>
#define	_RPMODBC_INTERNAL
#include <rpmodbc.h>

#include "debug.h"

/*@unchecked@*/
int _rpmodbc_debug = -1;

static void rpmodbcFini(void * _odbc)
	/*@globals fileSystem @*/
	/*@modifies *_odbc, fileSystem @*/
{
    rpmodbc odbc = _odbc;

    odbc->fn = _free(odbc->fn);
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmodbcPool = NULL;

static rpmodbc rpmodbcGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmodbcPool, fileSystem @*/
	/*@modifies pool, _rpmodbcPool, fileSystem @*/
{
    rpmodbc odbc;

    if (_rpmodbcPool == NULL) {
	_rpmodbcPool = rpmioNewPool("odbc", sizeof(*odbc), -1, _rpmodbc_debug,
			NULL, NULL, rpmodbcFini);
	pool = _rpmodbcPool;
    }
    odbc = (rpmodbc) rpmioGetPool(pool, sizeof(*odbc));
    memset(((char *)odbc)+sizeof(odbc->_item), 0, sizeof(*odbc)-sizeof(odbc->_item));
    return odbc;
}

rpmodbc rpmodbcNew(const char * fn, int flags)
{
    rpmodbc odbc = rpmodbcGetPool(_rpmodbcPool);
    int xx;

    if (fn)
	odbc->fn = xstrdup(fn);

    return rpmodbcLink(odbc);
}

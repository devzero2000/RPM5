/** \ingroup rpmio
 * \file rpmio/rpmbag.c
 */

#include "system.h"

#include <rpmiotypes.h>
#include <rpmio.h>	/* for *Pool methods */
#include <rpmlog.h>
#include <rpmurl.h>
#define	_RPMBAG_INTERNAL
#include <rpmbag.h>

#include "debug.h"

/*@unchecked@*/
int _rpmbag_debug = 0;

static void rpmbagFini(void * _bag)
	/*@globals fileSystem @*/
	/*@modifies *_bag, fileSystem @*/
{
    rpmbag bag = _bag;

    bag->fn = _free(bag->fn);
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmbagPool = NULL;

static rpmbag rpmbagGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmbagPool, fileSystem @*/
	/*@modifies pool, _rpmbagPool, fileSystem @*/
{
    rpmbag bag;

    if (_rpmbagPool == NULL) {
	_rpmbagPool = rpmioNewPool("bag", sizeof(*bag), -1, _rpmbag_debug,
			NULL, NULL, rpmbagFini);
	pool = _rpmbagPool;
    }
    bag = (rpmbag) rpmioGetPool(pool, sizeof(*bag));
    memset(((char *)bag)+sizeof(bag->_item), 0, sizeof(*bag)-sizeof(bag->_item));
    return bag;
}

rpmbag rpmbagNew(const char * fn, int flags)
{
    rpmbag bag = rpmbagGetPool(_rpmbagPool);
    int xx;

    if (fn)
	bag->fn = xstrdup(fn);

    return rpmbagLink(bag);
}

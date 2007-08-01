/** \ingroup rpmio
 * \file rpmio/rpmmg.c
 */

#include "system.h"

#include "magic.h"

#include <rpmio.h>
#include <rpmerr.h>
#define	_RPMMG_INTERNAL
#include <rpmmg.h>

#include "debug.h"

/*@unchecked@*/
int _rpmmg_debug = 0;

rpmmg rpmmgFree(rpmmg mg)
{
if (_rpmmg_debug)
fprintf(stderr, "--> rpmmgFree(%p)\n", mg);
    if (mg) {
	if (mg->ms) {
	    magic_close(mg->ms);
	    mg->ms = NULL;
	}
	mg->fn = _free(mg->fn);
	mg = _free(mg);
    }
    return NULL;
}

rpmmg rpmmgNew(const char * fn, int flags)
{
    rpmmg mg = xcalloc(1, sizeof(*mg));
    int xx;

    if (fn)
	mg->fn = xstrdup(fn);
    mg->flags = (flags ? flags : MAGIC_CHECK);
    mg->ms = magic_open(flags);
    if (mg->ms == NULL) {
	rpmError(RPMERR_EXEC, _("magic_open(0x%x) failed: %s\n"),
		flags, strerror(errno));
	return rpmmgFree(mg);
    }
    xx = magic_load(mg->ms, mg->fn);
    if (xx == -1) {
        rpmError(RPMERR_EXEC, _("magic_load(ms, \"%s\") failed: %s\n"),
                fn, magic_error(mg->ms));
	return rpmmgFree(mg);
    }

if (_rpmmg_debug)
fprintf(stderr, "--> rpmmgNew(%s, 0x%x) mg %p\n", fn, flags, mg);
    return mg;
}

const char * rpmmgFile(rpmmg mg, const char *fn)
{
    const char * t = NULL;

    if (mg->ms)
	t = magic_file(mg->ms, fn);
    t = xstrdup((t ? t : ""));

if (_rpmmg_debug)
fprintf(stderr, "--> rpmmgFile(%p, %s) %s\n", mg, fn, t);
    return t;
}

const char * rpmmgBuffer(rpmmg mg, const char * b, size_t nb)
{
    const char * t = NULL;

    if (mg->ms)
	t = magic_buffer(mg->ms, b, nb);
    t = xstrdup((t ? t : ""));

if (_rpmmg_debug)
fprintf(stderr, "--> rpmmgBuffer(%p, %p[%d]) %s\n", mg, b, nb, t);
    return t;
}

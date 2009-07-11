/** \ingroup rpmio
 * \file rpmio/rpmsex.c
 */

#include "system.h"

#if defined(WITH_SELINUX)
#include <selinux/selinux.h>
#endif

#include <rpmiotypes.h>
#include <rpmio.h>	/* for *Pool methods */
#include <rpmmacro.h>
#include <rpmlog.h>
#define	_RPMSEX_INTERNAL
#include <rpmsx.h>

#include "debug.h"

/*@unchecked@*/
int _rpmsex_debug = 0;

static void rpmsexFini(void * _sex)
	/*@globals fileSystem @*/
	/*@modifies *_sex, fileSystem @*/
{
    rpmsex sex = _sex;

#if defined(WITH_SELINUX)
    if (sex->fn)
	(void) matchpathcon_fini();
#endif
    sex->flags = 0;
    sex->fn = _free(sex->fn);
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmsexPool = NULL;

static rpmsex rpmsexGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmsexPool, fileSystem @*/
	/*@modifies pool, _rpmsexPool, fileSystem @*/
{
    rpmsex sex;

    if (_rpmsexPool == NULL) {
	_rpmsexPool = rpmioNewPool("sex", sizeof(*sex), -1, _rpmsex_debug,
			NULL, NULL, rpmsexFini);
	pool = _rpmsexPool;
    }
    return (rpmsex) rpmioGetPool(pool, sizeof(*sex));
}

rpmsex rpmsexNew(const char * fn, int flags)
{
    rpmsex sex = rpmsexGetPool(_rpmsexPool);

    if (fn)
	sex->fn = rpmGetPath(fn, NULL);
#if defined(WITH_SELINUX)
    if (sex->fn && *sex->fn && *sex->fn != '%')
	(void) matchpathcon_init(sex->fn);
    else
#endif
	sex->fn = _free(sex->fn);
    sex->flags = flags;
    return rpmsexLink(sex);
}

const char * rpmsexMatch(rpmsex sex, const char *fn, mode_t mode)
{
    const char * scon = NULL;

if (_rpmsex_debug)
fprintf(stderr, "--> %s(%p,%s,0%o)\n", __FUNCTION__, sex, (fn ? fn : "(nil)"), mode);

#if defined(WITH_SELINUX)
    if (sex->fn) {
	static char nocon[] = "";
/*@-moduncon@*/
	if (matchpathcon(fn, mode, (security_context_t *)&scon) || scon == NULL)
	    scon = xstrdup(nocon);
/*@=moduncon@*/
    }
#endif

if (_rpmsex_debug)
fprintf(stderr, "<-- %s(%p,%s,0%o) %s\n", __FUNCTION__, sex, (fn ? fn : "(nil)"), mode, scon);
    return scon;
}

int rpmsexExec(rpmsex sex, int verified, const char ** argv)
{
    int rc = -1;

if (_rpmsex_debug)
fprintf(stderr, "--> %s(%p,%d,%p)\n", __FUNCTION__, sex, verified, argv);

#if defined(WITH_SELINUX)
    rc = rpm_execcon(verified, argv[0], (char *const *)argv, environ);
#endif

if (_rpmsex_debug)
fprintf(stderr, "<-- %s(%p,%d,%p) rc %d\n", __FUNCTION__, sex, verified, argv, rc);
    return rc;
}

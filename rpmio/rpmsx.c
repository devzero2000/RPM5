/** \ingroup rpmio
 * \file rpmio/rpmsx.c
 */

#include "system.h"

#if defined(WITH_SELINUX)
#include <selinux/selinux.h>
#if defined(__LCLINT__)
/*@-incondefs@*/
extern void freecon(/*@only@*/ security_context_t con)
	/*@modifies con @*/;

extern int getfilecon(const char *path, /*@out@*/ security_context_t *con)
	/*@modifies *con @*/;
extern int lgetfilecon(const char *path, /*@out@*/ security_context_t *con)
	/*@modifies *con @*/;
extern int fgetfilecon(int fd, /*@out@*/ security_context_t *con)
	/*@modifies *con @*/;

extern int setfilecon(const char *path, security_context_t con)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
extern int lsetfilecon(const char *path, security_context_t con)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
extern int fsetfilecon(int fd, security_context_t con)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;

extern int getcon(/*@out@*/ security_context_t *con)
	/*@modifies *con @*/;
extern int getexeccon(/*@out@*/ security_context_t *con)
	/*@modifies *con @*/;
extern int setexeccon(security_context_t con)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;

extern int security_check_context(security_context_t con)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
extern int security_getenforce(void)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;

extern int is_selinux_enabled(void)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
/*@=incondefs@*/
#endif
#endif

#define	_RPMSEX_INTERNAL
#include <rpmsx.h>
#include <rpmlog.h>
#include <rpmmacro.h>

#include "debug.h"

/*@unchecked@*/
int _rpmsx_debug = 0;

/*@unchecked@*/ /*@relnull@*/
rpmsx _rpmsxI = NULL;

static void rpmsxFini(void * _sx)
	/*@globals fileSystem @*/
	/*@modifies *_sx, fileSystem @*/
{
    rpmsx sx = _sx;

#if defined(WITH_SELINUX)
    if (sx->fn)
	(void) matchpathcon_fini();
#endif
    sx->flags = 0;
    sx->fn = _free(sx->fn);
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmsxPool = NULL;

static rpmsx rpmsxGetPool(/*@null@*/ rpmioPool pool)
	/*@globals _rpmsxPool, fileSystem @*/
	/*@modifies pool, _rpmsxPool, fileSystem @*/
{
    rpmsx sx;

    if (_rpmsxPool == NULL) {
	_rpmsxPool = rpmioNewPool("sx", sizeof(*sx), -1, _rpmsx_debug,
			NULL, NULL, rpmsxFini);
	pool = _rpmsxPool;
    }
    return (rpmsx) rpmioGetPool(pool, sizeof(*sx));
}

rpmsx rpmsxNew(const char * fn, int flags)
{
    rpmsx sx = rpmsxGetPool(_rpmsxPool);

    sx->fn = NULL;
    sx->flags = flags;

#if defined(WITH_SELINUX)
    sx->fn = (fn ? rpmGetPath(fn,NULL) : xstrdup(selinux_file_context_path()));
    if (sx->fn && *sx->fn && *sx->fn != '%')
	(void) matchpathcon_init(sx->fn);
    else
	sx->fn = _free(sx->fn);
#endif
    return rpmsxLink(sx);
}

/*@unchecked@*/ /*@null@*/
static const char * _rpmsxI_fn;
/*@unchecked@*/
static int _rpmsxI_flags;

static rpmsx rpmsxI(void)
	/*@globals _rpmsxI @*/
	/*@modifies _rpmsxI @*/
{
    if (_rpmsxI == NULL)
	_rpmsxI = rpmsxNew(_rpmsxI_fn, _rpmsxI_flags);
    return _rpmsxI;
}

int rpmsxEnabled(/*@null@*/ rpmsx sx)
{
    int rc = 0;

#if defined(WITH_SELINUX)
    rc = is_selinux_enabled();
#endif

if (_rpmsx_debug)
fprintf(stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, sx, rc);
    return rc;
}

const char * rpmsxMatch(rpmsx sx, const char *fn, mode_t mode)
{
    const char * scon = NULL;

    if (sx == NULL) sx = rpmsxI();

if (_rpmsx_debug)
fprintf(stderr, "--> %s(%p,%s,0%o)\n", __FUNCTION__, sx, fn, mode);

#if defined(WITH_SELINUX)
    if (sx->fn) {
	static char nocon[] = "";
	if (matchpathcon(fn, mode, (security_context_t *)&scon) < 0)
	    scon = xstrdup(nocon);
    }
#endif

if (_rpmsx_debug)
fprintf(stderr, "<-- %s(%p,%s,0%o) %s\n", __FUNCTION__, sx, fn, mode, scon);
    return scon;
}

int rpmsxSetfilecon(rpmsx sx, const char *fn, mode_t mode,
		const char * scon)
{
    int rc = 0;

    if (sx == NULL) sx = rpmsxI();

if (_rpmsx_debug)
fprintf(stderr, "--> %s(%p,%s,0%o,%s)\n", __FUNCTION__, sx, fn, mode, scon);

#if defined(WITH_SELINUX)
    if (sx->fn) {
	security_context_t _scon = (security_context_t)
		(scon ? scon : rpmsxMatch(sx, fn, mode));
	rc = setfilecon(fn, _scon);
	if (scon == NULL) {
	    freecon(_scon);
	    _scon = NULL;
	}
    }
#endif

if (_rpmsx_debug)
fprintf(stderr, "<-- %s(%p,%s,0%o,%s) rc %d\n", __FUNCTION__, sx, fn, mode, scon, rc);
    return rc;
}

int rpmsxLsetfilecon(rpmsx sx, const char *fn, mode_t mode,
		const char * scon)
{
    int rc = 0;

    if (sx == NULL) sx = rpmsxI();

if (_rpmsx_debug)
fprintf(stderr, "--> %s(%p,%s,0%o,%s)\n", __FUNCTION__, sx, fn, mode, scon);

#if defined(WITH_SELINUX)
    if (sx->fn) {
	security_context_t _scon = (security_context_t)
		(scon ? scon : rpmsxMatch(sx, fn, mode));
	rc = lsetfilecon(fn, _scon);
	if (scon == NULL) {
	    freecon(_scon);
	    _scon = NULL;
	}
    }
#endif

if (_rpmsx_debug)
fprintf(stderr, "<-- %s(%p,%s,0%o,%s) rc %d\n", __FUNCTION__, sx, fn, mode, scon, rc);
    return rc;
}

int rpmsxExec(rpmsx sx, int verified, const char ** argv)
{
    int rc = -1;

    if (sx == NULL) sx = rpmsxI();

if (_rpmsx_debug)
fprintf(stderr, "--> %s(%p,%d,%p)\n", __FUNCTION__, sx, verified, argv);

#if defined(WITH_SELINUX)
    rc = rpm_execcon(verified, argv[0], (char *const *)argv, environ);
#endif

if (_rpmsx_debug)
fprintf(stderr, "<-- %s(%p,%d,%p) rc %d\n", __FUNCTION__, sx, verified, argv, rc);
    return rc;
}

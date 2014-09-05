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

#define	_RPMSX_INTERNAL
#include <rpmsx.h>
#include <rpmlog.h>
#include <rpmmacro.h>

#include "debug.h"

/*@unchecked@*/
int _rpmsx_debug = 0;

#define	SPEW(_list)	if (_rpmsx_debug) fprintf _list

static void rpmsxFini(void * _sx)
	/*@globals fileSystem @*/
	/*@modifies *_sx, fileSystem @*/
{
    rpmsx sx = (rpmsx) _sx;

#if defined(WITH_SELINUX)
    if (sx->fn) {
SPEW((stderr, "<-- matchpathcon_fini()\n"));
	matchpathcon_fini();
    }
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

/*@unchecked@*/ /*@relnull@*/
rpmsx _rpmsxI = NULL;

static rpmsx rpmsxI(void)
	/*@globals _rpmsxI @*/
	/*@modifies _rpmsxI @*/
{
    if (_rpmsxI == NULL) {
	rpmsx sx = rpmsxGetPool(_rpmsxPool);
	sx->fn = NULL;
	sx->flags = 0;
	_rpmsxI = rpmsxLink(sx);
    }
    return _rpmsxI;
}

rpmsx rpmsxNew(const char * _fn, unsigned int flags)
{
    const char * fn = rpmGetPath(_fn, NULL);
    rpmsx sx = rpmsxI();

    /* Use default file context path if arg is not an absolute path. */
    if (!(fn && fn[0] == '/'))
	fn = _free(fn);
#if defined(WITH_SELINUX)
    if (fn == NULL) {
	fn = xstrdup(selinux_file_context_path());
SPEW((stderr, "--- selinux_file_context_path: %s\n", fn));
    }
#endif

    /* Already initialized? */
    if (sx->fn) {
	/* If already opened, add a new reference and return. */
	if (!strcmp(sx->fn, fn))	/* XXX chroot by stat(fn) inode */
	    goto exit;
	/* Otherwise do an implicit matchpathcon_fini(). */
	rpmsxFini(sx);
    }

    sx->fn = fn;
    sx->flags = flags;

#if defined(WITH_SELINUX)
    if (sx->flags) {
	set_matchpathcon_flags(sx->flags);
SPEW((stderr, "--- set_matchpathcon_flags(0x%x)\n", sx->flags));
    }

    /* If matchpathcon_init fails, turn off SELinux functionality. */
    {	int rc;
SPEW((stderr, "--> matchpathcon_init(%s)\n", sx->fn));
	rc = matchpathcon_init(sx->fn);
	if (rc < 0) {
	    rpmsxFini(sx);
	    sx = NULL;		/* XXX transaction.c logic needs this. */
	}
    }
#endif

exit:
    return rpmsxLink(sx);
}

int rpmsxEnabled(/*@null@*/ rpmsx sx)
{
    static int rc = 0;
#if defined(WITH_SELINUX)
    static int oneshot = 0;

    if (!oneshot) {
	rc = is_selinux_enabled();
SPEW((stderr, "<-- %s(%p) rc %d\n", __FUNCTION__, sx, rc));
	oneshot++;
    }
#endif

    return rc;
}

const char * rpmsxMatch(rpmsx sx, const char *fn, mode_t mode)
{
    const char * scon = NULL;

    if (sx == NULL) sx = rpmsxI();

#if defined(WITH_SELINUX)
    if (sx->fn) {
	static char nocon[] = "";
	int rc = matchpathcon(fn, mode, (security_context_t *)&scon);
	if (rc < 0)
	    scon = xstrdup(nocon);
    }
#endif

if (_rpmsx_debug < 0 || (_rpmsx_debug > 0 && scon != NULL && *scon != '\0' &&strcmp("(null)", scon)))
fprintf(stderr, "<-- %s(%p,%s,0%o) \"%s\"\n", __FUNCTION__, sx, fn, mode, scon);
    return scon;
}

const char * rpmsxGetfilecon(rpmsx sx, const char *fn)
{
    const char * scon = NULL;

    if (sx == NULL) sx = rpmsxI();

SPEW((stderr, "--> %s(%p,%s) sxfn %s\n", __FUNCTION__, sx, fn, sx->fn));

#if defined(WITH_SELINUX)
    if (sx->fn && fn) {
	security_context_t _con = NULL;
	int rc = getfilecon(fn, &_con);
	if (rc > 0 && _con != NULL)
	    scon = (const char *) _con;
	else
	    freecon(_con);
    }
#endif

SPEW((stderr, "<-- %s(%p,%s) scon %s\n", __FUNCTION__, sx, fn, scon));
    return scon;
}

int rpmsxSetfilecon(rpmsx sx, const char *fn, mode_t mode,
		const char * scon)
{
    int rc = 0;

    if (sx == NULL) sx = rpmsxI();

SPEW((stderr, "--> %s(%p,%s,0%o,%s) sxfn %s\n", __FUNCTION__, sx, fn, mode, scon, sx->fn));

#if defined(WITH_SELINUX)
    if (sx->fn) {
	security_context_t _con = (security_context_t)
		(scon ? scon : rpmsxMatch(sx, fn, mode));
	rc = setfilecon(fn, _con);
	if (scon == NULL) {	/* XXX free lazy rpmsxMatch() string */
	    freecon(_con);
	    _con = NULL;
	}
    }
#endif

SPEW((stderr, "<-- %s(%p,%s,0%o,%s) rc %d\n", __FUNCTION__, sx, fn, mode, scon, rc));
    return rc;
}

const char * rpmsxLgetfilecon(rpmsx sx, const char *fn)
{
    const char * scon = NULL;

    if (sx == NULL) sx = rpmsxI();

SPEW((stderr, "--> %s(%p,%s) sxfn %s\n", __FUNCTION__, sx, fn, sx->fn));

#if defined(WITH_SELINUX)
    if (sx->fn && fn) {
	security_context_t _con = NULL;
	int rc = lgetfilecon(fn, &_con);
	if (rc > 0 && _con != NULL)
	    scon = (const char *) _con;
	else
	    freecon(_con);
    }
#endif

SPEW((stderr, "<-- %s(%p,%s) scon %s\n", __FUNCTION__, sx, fn, scon));
    return scon;
}

int rpmsxLsetfilecon(rpmsx sx, const char *fn, mode_t mode,
		const char * scon)
{
    int rc = 0;

    if (sx == NULL) sx = rpmsxI();

SPEW((stderr, "--> %s(%p,%s,0%o,%s) sxfn %s\n", __FUNCTION__, sx, fn, mode, scon, sx->fn));

#if defined(WITH_SELINUX)
    if (sx->fn) {
	security_context_t _con = (security_context_t)
		(scon ? scon : rpmsxMatch(sx, fn, mode));
	rc = lsetfilecon(fn, _con);
	if (scon == NULL) {	/* XXX free lazy rpmsxMatch() string */
	    freecon(_con);
	    _con = NULL;
	}
    }
#endif

SPEW((stderr, "<-- %s(%p,%s,0%o,%s) rc %d\n", __FUNCTION__, sx, fn, mode, scon, rc));
    return rc;
}

int rpmsxExec(rpmsx sx, int verified, const char ** argv)
{
    int rc = -1;

    if (sx == NULL) sx = rpmsxI();

SPEW((stderr, "--> %s(%p,%d,%p)\n", __FUNCTION__, sx, verified, argv));

#if defined(WITH_SELINUX)
    rc = rpm_execcon(verified, argv[0], (char *const *)argv, environ);
#endif

SPEW((stderr, "<-- %s(%p,%d,%p) rc %d\n", __FUNCTION__, sx, verified, argv, rc));
    return rc;
}

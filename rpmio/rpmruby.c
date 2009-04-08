/* XXX grrr, ruby.h includes its own config.h too. */
#ifdef	HAVE_CONFIG_H
#include "config.h"
#endif
#undef	PACKAGE_NAME
#undef	PACKAGE_TARNAME
#undef	PACKAGE_VERSION
#undef	PACKAGE_STRING
#undef	PACKAGE_BUGREPORT

#include <ruby.h>

#define _RPMRUBY_INTERNAL
#include "rpmruby.h"

#include "debug.h"

/*@unchecked@*/
int _rpmruby_debug = 0;

static void rpmrubyFini(void * _ruby)
        /*@globals fileSystem @*/
        /*@modifies *_ruby, fileSystem @*/
{
    rpmruby ruby = _ruby;

    ruby->fn = _free(ruby->fn);
    ruby->flags = 0;
#if defined(WITH_RUBY)
    ruby_finalize();
    ruby_cleanup(0);
#endif
    ruby->I = NULL;
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmrubyPool;

static rpmruby rpmrubyGetPool(/*@null@*/ rpmioPool pool)
        /*@globals _rpmrubyPool, fileSystem @*/
        /*@modifies pool, _rpmrubyPool, fileSystem @*/
{
    rpmruby ruby;

    if (_rpmrubyPool == NULL) {
        _rpmrubyPool = rpmioNewPool("ruby", sizeof(*ruby), -1, _rpmruby_debug,
                        NULL, NULL, rpmrubyFini);
        pool = _rpmrubyPool;
    }
    return (rpmruby) rpmioGetPool(pool, sizeof(*ruby));
}

rpmruby rpmrubyNew(const char * fn, int flags)
{
    rpmruby ruby = rpmrubyGetPool(_rpmrubyPool);

    if (fn)
	ruby->fn = strdup(fn);
    ruby->flags = flags;

#if defined(WITH_RUBY)
    ruby_init();
    ruby_script("rpmruby");
#endif

    return rpmrubyLink(ruby);
}

rpmRC rpmrubyRunFile(rpmruby ruby, const char * fn, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

if (_rpmruby_debug)
fprintf(stderr, "==> %s(%p,%s)\n", __FUNCTION__, ruby, fn);

    if (fn != NULL) {
#if defined(WITH_RUBY)
	rb_load_file(fn);
	ruby->state = ruby_exec();
	rc = RPMRC_OK;
#ifdef	NOTYET
	if (resultp)
	    *resultp = Tcl_GetStringResult(ruby->I);
#endif
#endif
    }
    return rc;
}

rpmRC rpmrubyRun(rpmruby ruby, const char * str, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

if (_rpmruby_debug)
fprintf(stderr, "==> %s(%p,%s)\n", __FUNCTION__, ruby, str);

    if (str != NULL) {
#if defined(WITH_RUBY)
	ruby->state = rb_eval_string(str);
	rc = RPMRC_OK;
#ifdef	NOTYET
	if (resultp)
	    *resultp = Tcl_GetStringResult(ruby->I);
#endif
#endif
    }
    return rc;
}

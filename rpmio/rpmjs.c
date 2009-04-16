#include "system.h"

#include <argv.h>

#ifdef	WITH_JS
#include <js.h>
#endif
#define _RPMJS_INTERNAL
#include "rpmjs.h"

#include "debug.h"

/*@unchecked@*/
int _rpmjs_debug = 0;

/*@unchecked@*/ /*@relnull@*/
rpmjs _rpmjsI = NULL;

static void rpmjsFini(void * _js)
        /*@globals fileSystem @*/
        /*@modifies *_js, fileSystem @*/
{
    rpmjs js = _js;

#if defined(WITH_JS)
#endif
    js->I = NULL;
}

/*@unchecked@*/ /*@only@*/ /*@null@*/
rpmioPool _rpmjsPool;

static rpmjs rpmjsGetPool(/*@null@*/ rpmioPool pool)
        /*@globals _rpmjsPool, fileSystem @*/
        /*@modifies pool, _rpmjsPool, fileSystem @*/
{
    rpmjs js;

    if (_rpmjsPool == NULL) {
        _rpmjsPool = rpmioNewPool("js", sizeof(*js), -1, _rpmjs_debug,
                        NULL, NULL, rpmjsFini);
        pool = _rpmjsPool;
    }
    return (rpmjs) rpmioGetPool(pool, sizeof(*js));
}

rpmjs rpmjsNew(const char ** av, int flags)
{
    rpmjs js = rpmjsGetPool(_rpmjsPool);

#if defined(WITH_JS)
    static const char * _av[] = { "rpmjs", NULL };
#endif

    return rpmjsLink(js);
}

static rpmjs rpmjsI(void)
	/*@globals _rpmjsI @*/
	/*@modifies _rpmjsI @*/
{
    if (_rpmjsI == NULL)
	_rpmjsI = rpmjsNew(NULL, 0);
    return _rpmjsI;
}

rpmRC rpmjsRunFile(rpmjs js, const char * fn, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

if (_rpmjs_debug)
fprintf(stderr, "==> %s(%p,%s)\n", __FUNCTION__, js, fn);

    if (js == NULL) js = rpmjsI();

    if (fn != NULL) {
#if defined(WITH_JS)
	rc = RPMRC_OK;
#ifdef	NOTYET
	if (resultp)
	    *resultp = rpmiobStr(js->iob);
#endif
#endif
    }
    return rc;
}

rpmRC rpmjsRun(rpmjs js, const char * str, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

if (_rpmjs_debug)
fprintf(stderr, "==> %s(%p,%s)\n", __FUNCTION__, js, str);

    if (js == NULL) js = rpmjsI();

    if (str != NULL) {
#if defined(WITH_JS)
	rc = RPMRC_OK;
#ifdef	NOTYET
	if (resultp)
	    *resultp = rpmiobStr(js->iob);
#endif
#endif
    }
    return rc;
}


#include "system.h"

#include <argv.h>

#ifdef	WITH_JS

#define	XP_UNIX	1
#include "jsprf.h"
#include "jsapi.h"

#if defined(WITH_GPSEE)
#include <gpsee.h>
typedef	gpsee_interpreter_t * JSI_t;
#endif

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

if (_rpmjs_debug)
fprintf(stderr, "==> %s(%p) I %p\n", __FUNCTION__, js, js->I);

#if defined(WITH_GPSEE)
    (void) gpsee_destroyInterpreter(js->I);
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
    JSI_t I = NULL;

#if defined(WITH_GPSEE)
    I = gpsee_createInterpreter((char *const *)av, environ);
#endif
    js->I = I;

    return rpmjsLink(js);
}

static rpmjs rpmjsI(void)
	/*@globals _rpmjsI @*/
	/*@modifies _rpmjsI @*/
{
    if (_rpmjsI == NULL)
	_rpmjsI = rpmjsNew(NULL, 0);
if (_rpmjs_debug)
fprintf(stderr, "<== %s() _rpmjsI %p\n", __FUNCTION__, _rpmjsI);
    return _rpmjsI;
}

rpmRC rpmjsRunFile(rpmjs js, const char * fn, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

    if (js == NULL) js = rpmjsI();

    if (fn != NULL) {
#if defined(WITH_JS)
	JSI_t I = js->I;
	JSContext *cx = I->cx;
	JSObject *glob = I->globalObj;
	JSScript *script = JS_CompileFile(cx, glob, fn);
	jsval rval;

	if (script) {
	    if (JS_ExecuteScript(cx, glob, script, &rval)) {
		rc = RPMRC_OK;
		if (resultp) {
		    JSString *rstr = JS_ValueToString(cx, rval);
		    *resultp = JS_GetStringBytes(rstr);
		}
	    }
	    JS_DestroyScript(cx, script);
	}
#endif
    }

if (_rpmjs_debug)
fprintf(stderr, "<== %s(%p,%s) rc %d\n", __FUNCTION__, js, fn, rc);

    return rc;
}

rpmRC rpmjsRun(rpmjs js, const char * str, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

    if (js == NULL) js = rpmjsI();

    if (str != NULL) {
#if defined(WITH_JS)
	JSI_t I = js->I;
	JSContext *cx = I->cx;
	JSObject *glob = I->globalObj;
	jsval rval = JSVAL_VOID;
	JSBool ok;

	ok = JS_EvaluateScript(cx, glob, str, strlen(str),
					__FILE__, __LINE__, &rval);
	if (ok) {
	    rc = RPMRC_OK;
	    if (resultp) {
		JSString *rstr = JS_ValueToString(cx, rval);
		*resultp = JS_GetStringBytes(rstr);
	    }
	}
#endif
    }

if (_rpmjs_debug)
fprintf(stderr, "<== %s(%p,%p[%u]) rc %d\n", __FUNCTION__, js, str, (str ? strlen(str) : 0), rc);

    return rc;
}

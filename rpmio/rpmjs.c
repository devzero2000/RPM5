
#include "system.h"

#include <argv.h>
#include <popt.h>

#if defined(__APPLE__)
#include <crt_externs.h>
#else
extern char ** environ;
#endif

#ifdef	WITH_JS

#define	XP_UNIX	1
#include "jsprf.h"
#include "jsapi.h"

#if defined(WITH_GPSEE)
#include <gpsee.h>
typedef	gpsee_interpreter_t * JSI_t;
#endif

#define	_RPMJS_OPTIONS	\
    (JSOPTION_STRICT | JSOPTION_RELIMIT | JSOPTION_ANONFUNFIX | JSOPTION_JIT)
#else
typedef void * JSI_t;
#define	_RPMJS_OPTIONS	0
#endif

#define _RPMJS_INTERNAL
#include "rpmjs.h"

#include "debug.h"

#define F_ISSET(_flags, _FLAG) ((_flags) & RPMJS_FLAGS_##_FLAG)

/*@unchecked@*/
int _rpmjs_debug = 0;

/*@unchecked@*/ /*@relnull@*/
rpmjs _rpmjsI = NULL;

/*@unchecked@*/
uint32_t _rpmjs_options = _RPMJS_OPTIONS;

/*@unchecked@*/
int _rpmjs_zeal = 2;

#ifdef	NOTYET
struct poptOption rpmjsIPoptTable[] = {
  { "allow", 'a', POPT_BIT_SET,		&_js.flags, RPMJS_FLAGS_ALLOW,
        N_("Allow (read-only) access to caller's environmen"), NULL },
  { "nocache", 'C', POPT_BIT_SET,	&_js.flags, RPMJS_FLAGS_NOCACHE,
        N_("Disables compiler caching via JSScript XDR serialization"), NULL },
  { "loadrc", 'R', POPT_BIT_SET,	&_js.flags, RPMJS_FLAGS_LOADRC,
        N_("Load RC file for interpreter based on script filename."), NULL },
  { "nowarn", 'W', POPT_BIT_SET,	&_js.flags, RPMJS_FLAGS_NOWARN,
        N_("Do not report warnings"), NULL },

  { "norelimit", 'e', POPT_BIT_CLR,	&_js.flags, RPMJS_FLAGS_RELIMIT,
        N_("Do not limit regexps to n^3 levels of backtracking"), NULL },
  { "nojit", 'J', POPT_BIT_CLR,		&_js.flags, RPMJS_FLAGS_JIT,
        N_("Disable nanojit"), NULL },
  { "nostrict", 'S', POPT_BIT_CLR,	&_js.flags, RPMJS_FLAGS_STRICT,
        N_("Disable Strict mode"), NULL },
  { "noutf8", 'U', POPT_BIT_SET,	&_js.flags, RPMJS_FLAGS_NOUTF8,
        N_("Disable UTF-8 C string processing"), NULL },
  { "xml", 'x', POPT_BIT_SET,		&_js.flags, RPMJS_FLAGS_XML,
        N_("Parse <!-- comments --> as E4X tokens"), NULL },

  { "anonfunfix", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,	&_js.flags, RPMJS_FLAGS_ANONFUNFIX,
        N_("Parse //@line number [\"filename\"] for XUL"), NULL },
  { "atline", 'A', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,	&_js.flags, RPMJS_FLAGS_ATLINE,
        N_("Parse //@line number [\"filename\"] for XUL"), NULL },
  { "werror", 'w', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,	&_js.flags, RPMJS_FLAGS_WERROR,
        N_("Convert warnings to errors"), NULL },

  POPT_TABLEEND
};
#endif

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

rpmjs rpmjsNew(const char ** av, uint32_t flags)
{
    rpmjs js =
#ifdef	NOTYET
	(flags & 0x80000000) ? rpmjsI() :
#endif
	rpmjsGetPool(_rpmjsPool);
    JSI_t I = NULL;

#if defined(WITH_GPSEE)
    static char *const _empty[] = { NULL };
    char *const * Iargv = (av ? (char *const *)av : _empty);
    char *const * Ienviron = NULL;
    if (flags == 0)
	flags = _rpmjs_options;

    if (F_ISSET(flags, ALLOW)) {
#if defined(__APPLE__)
        Ienviron = (char *const *) _NSGetEnviron();
#else
        Ienviron = environ;
#endif
    }

    I = gpsee_createInterpreter(Iargv, Ienviron);

    if (F_ISSET(flags, NOCACHE))
	I->useCompilerCache = 0;
    if (F_ISSET(flags, NOWARN))
	I->errorReport = er_noWarnings;

    JS_SetOptions(I->cx, (flags & 0xffff));
#if defined(JS_GC_ZEAL)
    JS_SetGCZeal(I->cx, _rpmjs_zeal);
#endif
#endif

    js->flags = flags;
    js->I = I;

    return rpmjsLink(js);
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
	if (ok && !JS_IsExceptionPending(I->cx)) {
	    rc = RPMRC_OK;
	    if (resultp) {
		JSString *rstr = JS_ValueToString(cx, rval);
		*resultp = JS_GetStringBytes(rstr);
	    }
	}
#endif
    }

if (_rpmjs_debug)
fprintf(stderr, "<== %s(%p,%p[%u]) rc %d\n", __FUNCTION__, js, str, (unsigned)(str ? strlen(str) : 0), rc);

    return rc;
}

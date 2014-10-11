#include "system.h"

#include "rpmio_internal.h"
#include <argv.h>
#include <popt.h>

#if defined(__APPLE__)
#include <crt_externs.h>
#else
extern char ** environ;
#endif

#if defined(WITH_MOZJS185)
#define	JS_THREADSAFE	1
#include <jsapi.h>
#endif	/* WITH_MOZJS185 */

#define _RPMJS_INTERNAL
#include "rpmjs.h"

#include "debug.h"

#define F_ISSET(_flags, _FLAG) ((_flags) & RPMJS_FLAGS_##_FLAG)

int _rpmjs_debug = 0;

#define RPMJSDBG(_t, _l) \
  if ((_t) || _rpmjs_debug) fprintf _l

rpmjs _rpmjsI = NULL;

uint32_t _rpmjs_options = 0;

int _rpmjs_zeal = 2;

struct rpmjs_s __rpmjs;
#ifdef	NOTYET
rpmjs _rpmjs = &_rpmjs;
#endif

#if defined(WITH_MOZJS185)

typedef struct JSI_s * JSI_t;
struct JSI_s {
    JSRuntime	*rt;
    JSContext	*cx;
    JSObject	*global;
};

static JSBool
rpmjsPrint(JSContext *cx, uintN argc, jsval *vp)
{
    jsval * argv = JS_ARGV(cx, vp);
    FILE * fp = stdout;
    uintN i;

    for (i = 0; i < argc; i++) {
	JSString * str = JS_ValueToString(cx, argv[i]);
	char *bytes;
        if (!str)
            return JS_FALSE;
        bytes = JS_EncodeString(cx, str);
        if (!bytes)
            return JS_FALSE;
        fprintf(fp, "%s%s", i ? " " : "", bytes);
        JS_free(cx, bytes);
    }

    fputc('\n', fp);
    fflush(fp);

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSFunctionSpec global_functions[] = {
    JS_FN("print",	rpmjsPrint,	0, 0),
    JS_FS_END
};

static const JSClass global_class = {
    "global", JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};
static void
rpmjsReportError(JSContext *cx, const char *message, JSErrorReport *report)
{
    fprintf(stderr, "%s:%u:%s\n",
	report->filename ? report->filename : "<no filename=\"filename\">",
	(unsigned int) report->lineno, message);
}
#endif	/* WITH_MOZJS185 */

struct poptOption rpmjsIPoptTable[] = {
  { "allow", 'a', POPT_BIT_SET,		&__rpmjs.flags, RPMJS_FLAGS_ALLOW,
        N_("Allow (read-only) access to caller's environmen"), NULL },
  { "nocache", 'C', POPT_BIT_SET,	&__rpmjs.flags, RPMJS_FLAGS_NOCACHE,
        N_("Disables compiler caching via JSScript XDR serialization"), NULL },
  { "loadrc", 'R', POPT_BIT_SET,	&__rpmjs.flags, RPMJS_FLAGS_LOADRC,
        N_("Load RC file for interpreter based on script filename."), NULL },
  { "nowarn", 'W', POPT_BIT_SET,	&__rpmjs.flags, RPMJS_FLAGS_NOWARN,
        N_("Do not report warnings"), NULL },

  { "norelimit", 'e', POPT_BIT_CLR,	&__rpmjs.flags, RPMJS_FLAGS_RELIMIT,
        N_("Do not limit regexps to n^3 levels of backtracking"), NULL },
  { "nojit", 'J', POPT_BIT_CLR,		&__rpmjs.flags, RPMJS_FLAGS_JIT,
        N_("Disable nanojit"), NULL },
  { "nostrict", 'S', POPT_BIT_CLR,	&__rpmjs.flags, RPMJS_FLAGS_STRICT,
        N_("Disable Strict mode"), NULL },
  { "noutf8", 'U', POPT_BIT_SET,	&__rpmjs.flags, RPMJS_FLAGS_NOUTF8,
        N_("Disable UTF-8 C string processing"), NULL },
  { "xml", 'x', POPT_BIT_SET,		&__rpmjs.flags, RPMJS_FLAGS_XML,
        N_("Parse <!-- comments --> as E4X tokens"), NULL },

  { "anonfunfix", '\0', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmjs.flags, RPMJS_FLAGS_ANONFUNFIX,
        N_("Parse //@line number [\"filename\"] for XUL"), NULL },
  { "atline", 'A', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmjs.flags, RPMJS_FLAGS_ATLINE,
        N_("Parse //@line number [\"filename\"] for XUL"), NULL },
  { "werror", 'w', POPT_BIT_SET|POPT_ARGFLAG_DOC_HIDDEN,	&__rpmjs.flags, RPMJS_FLAGS_WERROR,
        N_("Convert warnings to errors"), NULL },

  POPT_TABLEEND
};

static void rpmjsFini(void * _js)
{
    rpmjs js = (rpmjs) _js;

RPMJSDBG(0, (stderr, "==> %s(%p) I %p\n", __FUNCTION__, js, js->I));

#if defined(WITH_MOZJS185)
    {	/* js-1.8.5 */
	JSI_t I = (JSI_t) js->I;
	if (I->cx)
	    JS_DestroyContext(I->cx);
	I->cx = NULL;
	if (I->rt)
	    JS_DestroyRuntime(I->rt);
	I->rt = NULL;
	/* XXX JS_ShutDown */
    }
#endif
    js->I = NULL;
}

rpmioPool _rpmjsPool;

static rpmjs rpmjsGetPool(rpmioPool pool)
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
{
    if (_rpmjsI == NULL) {
	_rpmjsI = rpmjsNew(NULL, 0);
    }
RPMJSDBG(0, (stderr, "<== %s() _rpmjsI %p\n", __FUNCTION__, _rpmjsI));
    return _rpmjsI;
}

/* XXX FIXME: Iargv/Ienviron are now associated with running. */
rpmjs rpmjsNew(char ** av, uint32_t flags)
{
    rpmjs js =
#ifdef	NOTYET
	(flags & 0x80000000) ? rpmjsI() :
#endif
	rpmjsGetPool(_rpmjsPool);
    JSI_t I = NULL;

#if defined(WITH_MOZJS185)

    if (flags == 0)
	flags = _rpmjs_options;

    {	/* js-1.8.5 */
	static uint32_t _maxbytes = 8L * 1024L * 1024L;
	static size_t _stackChunkSize = 8192;
	JSClass * _clasp = (JSClass *)&global_class;
	JSFunctionSpec * _functions = global_functions;
	JSPrincipals * _principals = NULL;
	JSBool ok;

	I = (JSI_t) xcalloc(1, sizeof(*I));

	I->rt = JS_NewRuntime(_maxbytes);
assert(I->rt);
	JS_SetRuntimePrivate(I->rt, (void *)js);

	I->cx = JS_NewContext(I->rt, _stackChunkSize);
assert(I->cx);
	JS_SetContextPrivate(I->cx, (void *)js);
	JS_SetOptions(I->cx,
		JSOPTION_VAROBJFIX | JSOPTION_JIT | JSOPTION_METHODJIT);
	JS_SetVersion(I->cx, JSVERSION_LATEST);
	JS_SetErrorReporter(I->cx, rpmjsReportError);

	I->global = JS_NewCompartmentAndGlobalObject(I->cx,
			_clasp, _principals);
assert(I->global);
	JS_SetGlobalObject(I->cx, I->global);

	ok = JS_InitStandardClasses(I->cx, I->global);
assert(ok);

#ifdef	JS_HAS_CTYPES
	ok = JS_InitCTypesClass(I->cx, I->global);
assert(ok);
#endif

	ok = JS_DefineFunctions(I->cx, I->global, _functions);
assert(ok);

    }

#endif	/* WITH_MOZJS185 */

    js->flags = flags;
    js->I = I;

    return rpmjsLink(js);
}

rpmRC rpmjsRunFile(rpmjs js, const char * fn,
		char *const * Iargv,
		const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

    if (js == NULL) js = rpmjsI();

RPMJSDBG(0, (stderr, "<== %s(%p,%s) rc %d |%s|\n", __FUNCTION__, js, fn, rc, (resultp ? *resultp :"")));

    return rc;
}

rpmRC rpmjsRun(rpmjs js, const char * str, const char ** resultp)
{
    rpmRC rc = RPMRC_FAIL;

    if (!(str && *str))
	goto exit;

    if (js == NULL) js = rpmjsI();

#if defined(WITH_MOZJS185)
    {	/* js-1.8.5 */
	JSI_t I = (JSI_t) js->I;
	jsval v = JSVAL_VOID;
	JSBool ok = JS_EvaluateScript(I->cx, I->global,
				str, strlen(str), __FILE__, __LINE__, &v);
	if (!ok)	/* XXX if (v == JS_NULL || v == JS_FALSE) */
	    goto exit;
	rc = RPMRC_OK;
	if (resultp) {
	    char b[128];
	    size_t nb = sizeof(b);
	    char * t = NULL;
	    if (JSVAL_IS_NULL(v)) {
		t = xstrdup("");
	    } else
	    if (JSVAL_IS_VOID(v)) {
		t = xstrdup("");
	    } else
	    if (JSVAL_IS_INT(v)) {	/* int32_t */
		snprintf(b, nb, "%ld", (long)JSVAL_TO_INT(v));
		t = xstrdup(b);
	    } else
	    if (JSVAL_IS_DOUBLE(v)) {
		snprintf(b, nb, "%.16g", (double)JSVAL_TO_DOUBLE(v));
		t = xstrdup(b);
	    } else
#ifdef	NOTYET
	    if (JSVAL_IS_NUMBER(v)) {
	    } else
#endif
	    if (JSVAL_IS_STRING(v)) {
		JSString * rstr = JSVAL_TO_STRING(v);
		size_t nt = JS_GetStringEncodingLength(I->cx, rstr);
		t = (char *) xmalloc(nt+1);
		nt = JS_EncodeStringToBuffer(rstr, t, nt);
		t[nt] = '\0';
	    } else
#ifdef	NOTYET
	    if (JSVAL_IS_OBJECT(v)) {
	    } else
#endif
	    if (JSVAL_IS_BOOLEAN(v)) {
		snprintf(b, nb, "%c", (JSVAL_TO_BOOLEAN(v) ? 'T' : 'F'));
		t = xstrdup(b);
	    } else
#ifdef	NOTYET
	    if (JSVAL_IS_PRIMITIVE(v)) {
	    } else
	    if (JSVAL_IS_GCTHING(v)) {
	    } else
#endif
	    {
		jsval_layout l;
		l.asBits = JSVAL_BITS(v);
		snprintf(b, nb, "FIXME: JSVAL tag 0x%x payload 0x%llx",
			(unsigned)(l.asBits >> JSVAL_TAG_SHIFT),
			(unsigned long long)(l.asBits & JSVAL_PAYLOAD_MASK)
		);
		t = xstrdup(b);
	    }
	    *resultp = t;
	}
    }
#endif	/* WITH_MOZJS185 */

exit:
RPMJSDBG(0, (stderr, "<== %s(%p,%p[%u]) rc %d\n", __FUNCTION__, js, str, (unsigned)(str ? strlen(str) : 0), rc));

    return rc;
}
